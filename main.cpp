// main.cpp — Win顺 resident plugin: find the mouse by shaking it.
//
// The SDK (winshun.h, vendored from WinShun main) owns stdio and the JSON-RPC
// framing. This file wires three pieces together:
//   * MouseHook  — WH_MOUSE_LL on its own thread; feeds the pure detector;
//     never swallows input, never injects input.
//   * ShakeDetector — pure state machine (shake_logic.h), unit-tested.
//   * RippleOverlay — layered click-through topmost window that draws the
//     ring; lives on its own UI thread.
//
// The `mouse` keyword query is read-only (rows and copy effects only). The
// only side effects — showing the ripple at the cursor, toggling detection,
// copying diagnostics — happen in action(), i.e. when the user presses a key.
// Queries never install hooks, never touch files, never touch the clipboard.
//
// Code and comments are English; user-visible strings are localized with the
// host locale (Chinese by default, English when the locale starts with "en").
#include "mouse_hook.h"
#include "presets.h"
#include "ripple_overlay.h"
#include "settings.h"
#include "shake_logic.h"
#include "winshun.h"

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr const char* kPluginName = "ra9fael.findmouse";
constexpr const char* kVersion = "0.4.5";
constexpr int kStateVersion = 1; // state.json format version, not the plugin version

using fm::MouseHook;
using fm::RippleOverlay;
using fm::Settings;

std::string toLowerAscii(std::string s)
{
    for (char& c : s)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return s;
}

bool containsFold(const std::string& haystackLower, const std::string& needleLower)
{
    return needleLower.empty() || haystackLower.find(needleLower) != std::string::npos;
}

// ---------- Host integrity (UIPI) ----------
//
// The plugin runs at medium integrity: the runner de-elevates to an Explorer
// token whenever Win顺 itself is elevated. Windows UIPI then hides all input
// whose destination is an elevated window — the low-level mouse hook receives
// nothing and GetAsyncKeyState reads nothing while an elevated window (the
// Win顺 settings dialog, the launcher) has focus. Both shake triggering and
// click-to-dismiss die silently in that state, so the plugin detects it and
// says so instead of leaving a mute detector.

enum class HostIntegrity { Unknown, Medium, High };

struct HostInfo {
    HostIntegrity integrity = HostIntegrity::Unknown;
    std::string exe; // ASCII, diagnostics only
};

HostInfo queryHostIntegrity()
{
    const DWORD self = ::GetCurrentProcessId();
    struct Proc {
        DWORD pid = 0;
        DWORD parent = 0;
        std::wstring exe;
    };
    std::vector<Proc> procs;
    const HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32W pe {};
        pe.dwSize = sizeof(pe);
        if (::Process32FirstW(snap, &pe)) {
            do {
                procs.push_back({ pe.th32ProcessID, pe.th32ParentProcessID, pe.szExeFile });
            } while (::Process32NextW(snap, &pe));
        }
        ::CloseHandle(snap);
    }
    const auto find = [&procs](DWORD pid) -> const Proc* {
        for (const Proc& p : procs)
            if (p.pid == pid)
                return &p;
        return nullptr;
    };
    const Proc* me = find(self);
    const Proc* runner = me ? find(me->parent) : nullptr;
    const Proc* host = runner && runner->pid != self ? find(runner->parent) : nullptr;
    if (me == nullptr || runner == nullptr || host == nullptr || host->pid == self)
        return {};

    HostInfo info;
    info.exe.reserve(host->exe.size());
    for (wchar_t c : host->exe)
        info.exe.push_back(c <= 0x7F ? static_cast<char>(c) : '?');

    const HANDLE proc = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, host->pid);
    if (!proc)
        return info; // exe known, integrity not readable across the boundary
    HANDLE token = nullptr;
    if (::OpenProcessToken(proc, TOKEN_QUERY, &token)) {
        DWORD need = 0;
        ::GetTokenInformation(token, TokenIntegrityLevel, nullptr, 0, &need);
        if (need > 0) {
            std::vector<BYTE> buffer(need);
            if (::GetTokenInformation(token, TokenIntegrityLevel, buffer.data(), need, &need)) {
                const auto label = reinterpret_cast<PTOKEN_MANDATORY_LABEL>(buffer.data());
                const DWORD count = *::GetSidSubAuthorityCount(label->Label.Sid);
                const DWORD rid = *::GetSidSubAuthority(label->Label.Sid, static_cast<DWORD>(count - 1));
                info.integrity = rid >= SECURITY_MANDATORY_HIGH_RID ? HostIntegrity::High : HostIntegrity::Medium;
            }
        }
        ::CloseHandle(token);
    }
    ::CloseHandle(proc);
    return info;
}

class FindMousePlugin : public winshun::Plugin {
public:
    std::string name() const override { return kPluginName; }

    void initialized() override
    {
        m_host = queryHostIntegrity();
        m_settings = fm::readSettings(settings());
        resolveActiveStyle();
        loadState();
        startWorkers();
    }

    void settingsChanged(const winshun::Json& s) override
    {
        m_settings = fm::readSettings(s);
        resolveActiveStyle();
        m_shakeEnabled = m_settings.shakeEnabled; // the settings UI is an explicit user choice too
        if (m_hookStarted)
            m_hook.updateConfig(m_settings.shake);
        m_hook.setEnabled(m_shakeEnabled);
        if (m_overlay.active())
            m_overlay.updateStyle(m_settings, m_activeStyle);
        saveState();
    }

    void shuttingDown() override
    {
        stopWorkers();
        saveState();
    }

    ~FindMousePlugin() override { stopWorkers(); }

    winshun::Items query(const winshun::Query& q) override
    {
        // Snapshot what the rows need; the workers run concurrently.
        const bool enabled = m_hook.enabled();
        const bool hookUp = m_hookStarted && m_hook.active();
        const bool chinese = this->chinese();

        struct Row {
            const char* id;
            std::string title;
            std::string subtitle;
            const char* icon;
            int score;
        };
        std::vector<Row> rows;
        rows.reserve(3);

        rows.push_back({ "showNow",
            chinese ? "立即显示定位动画" : "Show the locator beacon now",
            chinese ? (enabled ? "涟漪跟随鼠标，按下左/右键结束 · Enter" : "晃动检测已关闭，仍可手动显示 · Enter")
                    : (enabled ? "Follows the pointer until a mouse button · Enter" : "Shake detection is off; this still works · Enter"),
            "glyph:U+E7C9", 90 });

        rows.push_back({ "toggleShake",
            chinese ? (enabled ? "晃动检测：已开启" : "晃动检测：已关闭")
                    : (enabled ? "Shake detection: on" : "Shake detection: off"),
            chinese ? "快速左右晃动鼠标时显示跟随涟漪 · Ctrl+Enter 切换"
                    : "Follows the pointer on a quick horizontal wiggle · Ctrl+Enter toggles",
            enabled ? "glyph:U+E8B7" : "glyph:U+E8B8", 80 });

        rows.push_back({ "copyDiagnostics",
            chinese ? "复制诊断信息" : "Copy diagnostics",
            chinese ? "钩子状态、手势计数与当前参数 · Shift+Enter"
                    : "Hook state, gesture counters and settings · Shift+Enter",
            "glyph:U+E946", 70 });

        if (m_host.integrity == HostIntegrity::High) {
            rows.push_back({ "elevatedNotice",
                chinese ? "Win顺 提权中：提权窗口上晃动不触发、点击不消失"
                        : "Win顺 runs elevated: shakes and clicks on elevated windows are invisible",
                chinese ? "Windows UIPI 限制，不是插件缺陷；以普通权限运行 Win顺 即可恢复 · Enter 详情"
                        : "A Windows UIPI limit, not a plugin bug; run Win顺 without elevation to restore · Enter for details",
                "glyph:U+E7BA", 60 });
        }

        const std::string needle = toLowerAscii(q.text);
        winshun::Items items;
        for (const Row& row : rows) {
            if (!containsFold(toLowerAscii(row.title) + " " + toLowerAscii(row.subtitle), needle))
                continue;
            winshun::Item item(row.id, row.title, row.subtitle);
            item.withIcon(row.icon).withScore(row.score);
            if (!hookUp && row.id != std::string("copyDiagnostics")) {
                // The hook could not install; say so instead of pretending.
                item.subtitle = chinese ? "钩子未运行（诊断里有详情） · Shift+Enter 查看详情"
                                        : "Hook not running (see diagnostics) · Shift+Enter";
            }
            items.push_back(std::move(item));
        }
        return items;
    }

    winshun::Effects action(const winshun::ActionRequest& a) override
    {
        if (a.action == "showNow") {
            POINT pt {};
            if (!::GetCursorPos(&pt))
                return { winshun::effect::notify(tr("无法获取鼠标位置", "Could not read the pointer position")), effectKeep() };
            if (!m_overlay.active()) {
                m_overlay.start();
                m_overlay.updateStyle(m_settings, m_activeStyle);
            }
            m_manualTriggers.fetch_add(1); // counted separately from hook gestures
            m_overlay.request(pt.x, pt.y);
            return { effectHide() };
        }
        if (a.action == "toggleShake") {
            m_shakeEnabled = !m_shakeEnabled;
            m_hook.setEnabled(m_shakeEnabled);
            saveState();
            return { winshun::effect::notify(m_shakeEnabled ? tr("晃动检测已开启", "Shake detection is on")
                                                            : tr("晃动检测已关闭", "Shake detection is off")),
                effectHide() };
        }
        if (a.action == "copyDiagnostics") {
            const std::string text = diagnostics();
            return { winshun::effect::copy(text),
                winshun::effect::notify(tr("已复制诊断信息", "Diagnostics copied")), effectHide() };
        }
        if (a.action == "elevatedNotice") {
            return { winshun::effect::notify(tr(
                "Windows UIPI：本插件以中等完整性运行，焦点在提权窗口（Win顺 设置、启动器等）时晃动与点击都感知不到，"
                "信标靠自动消失时间（默认 3000 ms）兜底。以普通权限运行 Win顺 即可完全恢复。",
                "Windows UIPI: this plugin runs at medium integrity. While an elevated window (Win顺 settings, the "
                "launcher) has focus, shakes and clicks cannot be seen and only the auto-dismiss timeout (default "
                "3000 ms) ends the beacon. Run Win顺 without elevation to restore everything.")),
                effectKeep() };
        }
        return Plugin::action(a);
    }

private:
    using Effects = winshun::Effects;

    winshun::Json effectHide() const { return winshun::effect::hide(); }
    winshun::Json effectKeep() const { return winshun::effect::keep(); }

    std::string tr(const char* zh, const char* en) const { return chinese() ? zh : en; }

    // Resolve the active StyleDoc for the current effect. DSL presets take
    // their document from the registry (presets.h); Custom parses the
    // user's customStyle setting. Keep the last valid user document separate
    // from the selected preset so an invalid custom edit never adopts a
    // preset, and switching through a built-in never loses the user fallback.
    void resolveActiveStyle()
    {
        if (m_settings.effect == fm::Effect::Ping || m_settings.effect == fm::Effect::Heart
            || m_settings.effect == fm::Effect::Orbit) {
            const fm::EffectInfo* e = fm::findEffectInfo(fm::effectName(m_settings.effect));
            m_activeStyle = e ? fm::presetDoc(*e) : nullptr;
            m_customStatus = m_activeStyle ? std::string("preset (") + e->id + ")"
                                          : "preset error (ring fallback)";
            if (!m_activeStyle)
                log("warn", "ra9fael.findmouse: preset unavailable; using the ring fallback");
            return;
        }
        if (m_settings.effect != fm::Effect::Custom) {
            m_activeStyle.reset();
            m_customStatus = "none";
            return;
        }
        if (m_settings.customStyleText.empty()) {
            m_customStyle.reset();
            m_activeStyle.reset();
            m_customStatus = "none";
            return;
        }
        const auto json = winshun::Json::parse(m_settings.customStyleText);
        fm::StyleDoc doc;
        if (json && json->isObject() && fm::parseStyleDoc(*json, doc)) {
            m_customStyle = std::make_shared<const fm::StyleDoc>(std::move(doc));
            m_customStatus = "ok (" + std::to_string(m_customStyle->layers.size()) + " layers)";
        } else {
            if (!m_customStyle)
                log("warn", "ra9fael.findmouse: customStyle parse failed; using the ring fallback");
            m_customStatus = m_customStyle ? "parse error (previous custom style kept)"
                                          : "parse error (ring fallback)";
        }
        m_activeStyle = m_customStyle;
    }

    void startWorkers()
    {
        m_overlay.start();
        m_overlay.updateStyle(m_settings, m_activeStyle);
        m_hookStarted = m_hook.start(m_settings.shake, [this](int x, int y) {
            m_overlay.request(x, y); // enqueue + post: fast enough for the hook
        });
        m_hook.setEnabled(m_shakeEnabled);
        if (!m_hookStarted) {
            const std::string text = tr("ra9fael.findmouse：鼠标钩子安装失败，晃动检测不可用",
                "ra9fael.findmouse: the mouse hook could not be installed; shake detection is off");
            log("error", text);
        }
    }

    void stopWorkers()
    {
        m_hook.stop();
        m_overlay.stop();
        m_hookStarted = false;
    }

    std::filesystem::path statePath() const
    {
        const std::string dir = dataDir();
        if (dir.empty())
            return {};
        return std::filesystem::path(dir) / "state.json";
    }

    void loadState()
    {
        m_shakeEnabled = m_settings.shakeEnabled;
        const auto path = statePath();
        if (path.empty())
            return;
        std::error_code ec;
        std::ifstream f(path, std::ios::binary);
        if (!f)
            return;
        std::stringstream buf;
        buf << f.rdbuf();
        const auto json = winshun::Json::parse(buf.str());
        if (json && json->isObject()) {
            const winshun::Json& v = (*json)["shakeEnabled"];
            if (v.isBool())
                m_shakeEnabled = v.toBool();
            else if (v.isNumber())
                m_shakeEnabled = v.toInt() != 0;
        }
    }

    void saveState()
    {
        const auto path = statePath();
        if (path.empty())
            return;
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f) {
            log("warn", "ra9fael.findmouse: cannot write " + path.string());
            return;
        }
        f << "{\"shakeEnabled\":" << (m_shakeEnabled ? "true" : "false")
          << ",\"version\":" << kStateVersion << "}\n";
    }

    std::string diagnostics() const
    {
        const fm::MouseHook::Stats st = m_hook.stats();
        std::ostringstream o;
        o << "WinShun Find Mouse (" << kPluginName << ") diagnostics\n";
        o << "version: " << kVersion << "\n";
        o << "hook installed: " << (m_hookStarted ? "yes" : "no") << "\n";
        o << "hook thread alive: " << (m_hook.active() ? "yes" : "no") << "\n";
        o << "host process: " << (m_host.exe.empty() ? "unknown" : m_host.exe) << ", integrity: "
          << (m_host.integrity == HostIntegrity::High
                  ? "high (elevated; UIPI hides input aimed at elevated windows — shake and click-to-dismiss are blind while an elevated window has focus)"
                  : m_host.integrity == HostIntegrity::Medium ? "medium"
                                                              : "unknown")
          << "\n";
        o << "shake detection: " << (m_hook.enabled() ? "enabled" : "disabled") << "\n";
        o << "moves seen: " << st.moves << "\n";
        o << "reversals: " << st.reversals << "\n";
        o << "ripples triggered: " << (st.triggers + m_manualTriggers.load()) << "\n";
        o << "config: window=" << m_settings.shake.windowMs << "ms, reversals=" << m_settings.shake.minReversals
          << ", stroke=" << m_settings.shake.strokePx << "px, cooldown=" << m_settings.shake.cooldownMs
          << "ms, minSpeed=" << m_settings.shake.minSpeedPxS << "px/s\n";
        o << "ripple: " << m_settings.rippleMs << "ms, size=" << m_settings.rippleSize
          << "px, accent=" << m_settings.accentText
          << ", effect=" << fm::effectName(m_settings.effect) << "\n";
        o << "follow timeout: " << m_settings.followTimeoutMs
          << "ms (0 = until a mouse button), fade out: " << m_settings.fadeOutMs
          << "ms (0 = instant)\n";
        o << "custom style: " << m_customStatus << "\n";
        if (m_activeStyle)
            o << "style document: " << m_activeStyle->layers.size() << " layers, loop="
              << (m_activeStyle->loopMs > 0 ? m_activeStyle->loopMs : m_settings.rippleMs) << "ms\n";
        else
            o << "style document: none\n";
        o << "locale: " << locale() << ", theme: " << theme() << "\n";
        o << "dataDir: " << dataDir() << "\n";
        return o.str();
    }

    Settings m_settings;
    bool m_shakeEnabled = true;
    bool m_hookStarted = false;
    HostInfo m_host; // parent-chain integrity probe, taken once at startup
    std::atomic<long long> m_manualTriggers { 0 }; // ripples shown via showNow, not the hook
    std::shared_ptr<const fm::StyleDoc> m_customStyle; // parsed customStyle JSON
    std::shared_ptr<const fm::StyleDoc> m_activeStyle; // selected preset or last valid custom doc
    std::string m_customStatus = "none"; // echoed in diagnostics
    fm::MouseHook m_hook;
    fm::RippleOverlay m_overlay;
};

} // namespace

int main()
{
    FindMousePlugin plugin;
    return plugin.serve();
}
