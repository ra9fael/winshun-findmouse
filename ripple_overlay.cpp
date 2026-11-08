// ripple_overlay.cpp — layered-window beacon that follows the pointer.
//
// After a trigger the overlay enters follow mode: every frame (~60 fps) it
// re-centers on the current cursor position and draws the configured style
// (Effect, from settings.h):
//   * ring  — one ring expanding from 35% to full radius and fading out,
//   * waves — three staggered expanding ripples,
//   * halo  — a breathing soft glow disc around the pointer,
//   * radar — a rotating sonar sweep inside a static circle,
//   * ping/heart/orbit/custom — validated declarative layers.
// Every style ends with a constant center dot so the exact pointer spot is
// never lost. Follow mode ends when the user presses the left or right mouse
// button (polled with GetAsyncKeyState, no hook involvement) or the configured
// follow timeout, then fades for the configured duration.
//
// Visibility on any background (findmouse-plugin-design.md §6): the accent
// figure, a thin white outline just outside it (dark surfaces), and a soft
// black outline outside that (white surfaces). Where bands overlap the
// stronger one wins, which keeps the alpha channel from blowing out to
// opaque white.
#include "ripple_overlay.h"

#include <cmath>
#include <cstdint>

namespace fm {

namespace {

constexpr wchar_t kClassName[] = L"WinShunFindMouseRipple";
constexpr UINT_PTR kFrameTimer = 1;
constexpr UINT kFrameIntervalMs = 15; // ~60 fps; follow mode lives until a click

constexpr UINT WM_APP_TRIGGER = WM_APP + 1; // wParam = x, lParam = y
constexpr UINT WM_APP_STYLE = WM_APP + 2; // wParam = heap StyleMsg*, lParam = 0

// Band radii relative to the current ring radius r, in 96-dpi pixels.
constexpr double kMainHalfWidth = 3.5;
constexpr double kWhiteOffset = 6.5;
constexpr double kWhiteHalfWidth = 1.6;
constexpr double kDarkOffset = 10.5;
constexpr double kDarkHalfWidth = 2.8;
constexpr double kDarkAlpha = 110.0;

// Center dot (constant while following): accent disc with a white rim.
constexpr double kDotRadius = 2.6;
constexpr double kDotRimOuter = 4.4;

int monitorDpiPct(POINT pt)
{
    // Per-monitor DPI, without a hard Shcore import: the loader probe keeps
    // the exe running on very old SDKs / stripped-down systems (100% then).
    using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
    if (HMODULE shcore = ::GetModuleHandleW(L"Shcore.dll")) {
        if (const auto fn = reinterpret_cast<GetDpiForMonitorFn>(
                ::GetProcAddress(shcore, "GetDpiForMonitor"))) {
            const HMONITOR mon = ::MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
            UINT dx = 96, dy = 96;
            if (mon && SUCCEEDED(fn(mon, 0 /* MDT_EFFECTIVE_DPI */, &dx, &dy)) && dx != 0)
                return static_cast<int>(dx * 100 / 96);
        }
    }
    if (HDC dc = ::GetDC(nullptr)) {
        const int logpix = ::GetDeviceCaps(dc, LOGPIXELSX);
        ::ReleaseDC(nullptr, dc);
        if (logpix > 0)
            return logpix * 100 / 96;
    }
    return 100;
}

inline double smooth(double t)
{
    t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
    return t * t * (3.0 - 2.0 * t); // smoothstep: soft band edges, no aliasing
}

struct BandColor {
    std::uint8_t b, g, r;
};

} // namespace

void RippleOverlay::applyStyle(const StyleMsg& m)
{
    m_rippleMs = m.rippleMs;
    m_rippleSize = m.rippleSize;
    m_followTimeoutMs = m.followTimeoutMs;
    m_fadeOutMs = m.fadeOutMs;
    m_effect = m.effect;
    m_accent = m.accent;
    m_style = m.doc;
}

bool RippleOverlay::start()
{
    if (m_running.load())
        return true;
    if (m_thread.joinable())
        m_thread.join();
    std::promise<bool> ready;
    auto started = ready.get_future();
    m_running.store(true);
    try {
        m_thread = std::thread(&RippleOverlay::run, this, std::move(ready));
    } catch (...) {
        m_running.store(false);
        return false;
    }
    // The first updateStyle/request must reach a real message queue. Returning
    // before the worker creates it silently drops initialize-time presets.
    return started.get();
}

void RippleOverlay::stop()
{
    if (m_thread.joinable()) {
        m_running.store(false);
        ::PostThreadMessageW(m_threadId.load(), WM_QUIT, 0, 0);
        m_thread.join();
    }
}

void RippleOverlay::request(int x, int y)
{
    const DWORD tid = m_threadId.load();
    if (tid != 0)
        ::PostThreadMessageW(tid, WM_APP_TRIGGER, static_cast<WPARAM>(x), static_cast<LPARAM>(y));
}

void RippleOverlay::updateStyle(const Settings& settings, std::shared_ptr<const StyleDoc> customDoc)
{
    const DWORD tid = m_threadId.load();
    if (tid == 0)
        return;
    auto m = std::make_unique<StyleMsg>(StyleMsg { settings.rippleMs, settings.rippleSize,
        settings.followTimeoutMs, settings.fadeOutMs, settings.effect, settings.accent, std::move(customDoc) });
    if (::PostThreadMessageW(tid, WM_APP_STYLE, reinterpret_cast<WPARAM>(m.get()), 0))
        m.release(); // ownership moves to the worker only after a successful post
}

void RippleOverlay::run(std::promise<bool> ready)
{
    static const bool classRegistered = [] {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = &RippleOverlay::procStatic;
        wc.hInstance = ::GetModuleHandleW(nullptr);
        wc.hCursor = nullptr;
        wc.hbrBackground = nullptr;
        wc.lpszClassName = kClassName;
        return ::RegisterClassW(&wc) != 0; // ERROR_CLASS_ALREADY_EXISTS is fine
    }();
    (void)classRegistered;

    m_hwnd = ::CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
        kClassName, L"", WS_POPUP, 0, 0, 1, 1, nullptr, nullptr,
        ::GetModuleHandleW(nullptr), this);

    if (!m_hwnd) {
        m_running.store(false);
        ready.set_value(false);
        return;
    }
    m_threadId.store(::GetCurrentThreadId());
    ready.set_value(true);

    MSG msg;
    while (m_running.load()) {
        const BOOL got = ::GetMessageW(&msg, nullptr, 0, 0);
        if (got <= 0)
            break;
        switch (msg.message) {
        case WM_APP_TRIGGER:
            beginFollow(m_hwnd, static_cast<int>(msg.wParam), static_cast<int>(msg.lParam));
            break;
        case WM_APP_STYLE:
            if (msg.wParam != 0) {
                const std::unique_ptr<StyleMsg> m(reinterpret_cast<StyleMsg*>(msg.wParam));
                applyStyle(*m);
            }
            break;
        default:
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
            break;
        }
    }

    m_threadId.store(0);
    // Shutdown may leave settings messages queued behind WM_QUIT.
    while (::PeekMessageW(&msg, nullptr, WM_APP_STYLE, WM_APP_STYLE, PM_REMOVE))
        delete reinterpret_cast<StyleMsg*>(msg.wParam);
    if (m_hwnd) {
        ::DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    if (m_dib) {
        ::DeleteObject(m_dib);
        m_dib = nullptr;
    }
    m_running.store(false);
}

void RippleOverlay::beginFollow(HWND hwnd, int x, int y)
{
    if (!hwnd)
        return;
    m_scalePct = monitorDpiPct(POINT { x, y });
    const double s = m_scalePct / 100.0;

    // Window must hold the ring plus the outermost dark band with margin.
    const int margin = static_cast<int>(std::lround((kDarkOffset + kDarkHalfWidth + 4) * s));
    m_winSize = 2 * (m_rippleSize + margin);
    m_cx = x;
    m_cy = y;

    // Keep the whole beacon on the virtual screen (multi-monitor, negative
    // coordinates included); the center is what matters, so pull it in.
    const int vx = ::GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int vy = ::GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int vw = ::GetSystemMetrics(SM_CXVIRTUALSCREEN);
    const int vh = ::GetSystemMetrics(SM_CYVIRTUALSCREEN);
    const int left = m_cx - m_winSize / 2;
    const int top = m_cy - m_winSize / 2;
    if (left < vx)
        m_cx += vx - left;
    if (top < vy)
        m_cy += vy - top;
    if (left + m_winSize > vx + vw)
        m_cx -= left + m_winSize - (vx + vw);
    if (top + m_winSize > vy + vh)
        m_cy -= top + m_winSize - (vy + vh);

    // A new trigger (re)starts the beacon; a trigger during follow mode just
    // restarts the pulse. A trigger during the fade-out revives the beacon at
    // full opacity as a fresh follow (new timeout origin).
    const long long now = static_cast<long long>(::GetTickCount64());
    if (!m_following || m_fading)
        m_followStart = now;
    m_following = true;
    m_fading = false;
    m_pulseStart = now;

    ::SetWindowPos(hwnd, HWND_TOPMOST, m_cx - m_winSize / 2, m_cy - m_winSize / 2,
        m_winSize, m_winSize, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    ::SetTimer(hwnd, kFrameTimer, kFrameIntervalMs, nullptr);
    drawFrame(hwnd);
}

void RippleOverlay::beginFade(HWND hwnd, long long now)
{
    // fadeOutMs = 0 keeps the old behavior: hide on the spot.
    if (m_fadeOutMs <= 0) {
        endFollow(hwnd);
        return;
    }
    m_fading = true;
    m_fadeStart = now;
}

void RippleOverlay::endFollow(HWND hwnd)
{
    m_following = false;
    m_fading = false;
    if (hwnd) {
        ::KillTimer(hwnd, kFrameTimer);
        ::ShowWindow(hwnd, SW_HIDE);
    }
}

void RippleOverlay::drawFrame(HWND hwnd)
{
    if (!hwnd || !m_following)
        return;

    const long long now = static_cast<long long>(::GetTickCount64());

    // End conditions: a mouse button (left or right), the follow timeout
    // (0 = never), and nothing else. Polled here so neither the hook nor any
    // other thread is involved. Meeting one starts the decay; during the
    // decay the buttons are no longer polled (a held button must not restart
    // or extend it).
    if (!m_fading
        && ((::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0
            || (::GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0
            || (m_followTimeoutMs > 0 && now - m_followStart > m_followTimeoutMs))) {
        beginFade(hwnd, now);
        if (!m_following)
            return; // fadeOutMs = 0: already hidden
    }

    // Whole-window opacity. Quadratic decay: drops quickly at first, then
    // tails off smoothly, so the end reads as a fade, not a cut. The beacon
    // keeps animating and following the pointer while it fades.
    BYTE alpha = 255;
    if (m_fading) {
        const double t = static_cast<double>(now - m_fadeStart) / m_fadeOutMs;
        if (t >= 1.0) {
            endFollow(hwnd);
            return;
        }
        const double k = 1.0 - t;
        alpha = static_cast<BYTE>(std::lround(255.0 * k * k));
    }

    // Follow the pointer. The DPI snapshot stays from the trigger point (a
    // mid-follow monitor change keeps the size until the next trigger).
    POINT pt {};
    if (::GetCursorPos(&pt)) {
        m_cx = pt.x;
        m_cy = pt.y;
    }

    // One pulse cycle is p in [0, 1); each style maps it to its own motion.
    // A preset or custom document may carry its own loop length.
    const double s = m_scalePct / 100.0;
    int cycle = m_rippleMs;
    if (usesStyleDoc(m_effect) && m_style && m_style->loopMs > 0)
        cycle = m_style->loopMs;
    const double cycles = static_cast<double>(now - m_pulseStart) / (cycle > 0 ? cycle : 1);
    const double p = std::fmod(cycles, 1.0);

    if (!ensureDib())
        return;
    switch (m_effect) {
    case Effect::Waves:
        renderWaves(s, p);
        break;
    case Effect::Halo:
        renderHalo(s, p);
        break;
    case Effect::Radar:
        renderRadar(s, p);
        break;
    case Effect::Custom:
    case Effect::Ping:
    case Effect::Heart:
    case Effect::Orbit:
        renderCustom(s, cycles); // DSL presets share the generic renderer
        break;
    case Effect::Ring:
        renderRing(s, p);
        break;
    }
    stampDot(s);
    present(hwnd, alpha);
}

bool RippleOverlay::ensureDib()
{
    // (Re)create the frame buffer at the current window size.
    if (m_dib && m_bits) {
        BITMAP bm = {};
        if (::GetObjectW(m_dib, sizeof(bm), &bm) && (bm.bmWidth != m_winSize || bm.bmHeight != m_winSize)) {
            ::DeleteObject(m_dib);
            m_dib = nullptr;
            m_bits = nullptr;
        }
    }
    if (!m_dib) {
        BITMAPINFO bi = {};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = m_winSize;
        bi.bmiHeader.biHeight = -m_winSize; // top-down
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        void* bits = nullptr;
        m_dib = ::CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
        m_bits = bits;
        if (!m_dib || !m_bits)
            return false;
    }
    return true;
}

void RippleOverlay::present(HWND hwnd, BYTE alpha)
{
    HDC screen = ::GetDC(nullptr);
    if (!screen)
        return;
    HDC mem = ::CreateCompatibleDC(screen);
    if (mem) {
        HGDIOBJ old = ::SelectObject(mem, m_dib);
        POINT dst { m_cx - m_winSize / 2, m_cy - m_winSize / 2 };
        SIZE size { m_winSize, m_winSize };
        POINT zero { 0, 0 };
        BLENDFUNCTION blend { AC_SRC_OVER, 0, alpha, AC_SRC_ALPHA };
        ::UpdateLayeredWindow(hwnd, screen, &dst, &size, mem, &zero, 0, &blend, ULW_ALPHA);
        ::SelectObject(mem, old);
        ::DeleteDC(mem);
    }
    ::ReleaseDC(nullptr, screen);
}

// Premultiplied-BGRA write; a <= 0 clears the pixel (the buffer is reused).
inline void put(std::uint8_t* px, double a, const BandColor& col)
{
    if (a <= 0.0) {
        px[0] = px[1] = px[2] = px[3] = 0;
        return;
    }
    if (a > 1.0)
        a = 1.0;
    px[0] = static_cast<std::uint8_t>(col.b * a + 0.5);
    px[1] = static_cast<std::uint8_t>(col.g * a + 0.5);
    px[2] = static_cast<std::uint8_t>(col.r * a + 0.5);
    px[3] = static_cast<std::uint8_t>(a * 255.0 + 0.5);
}

void RippleOverlay::renderRing(double s, double p)
{
    const BandColor mainC { m_accent.b, m_accent.g, m_accent.r };
    const BandColor whiteC { 255, 255, 255 };
    const BandColor darkC { 0, 0, 0 };
    const double c = (m_winSize - 1) / 2.0;
    auto pixels = static_cast<std::uint8_t*>(m_bits);
    const double mainHw = kMainHalfWidth * s;
    const double whiteHw = kWhiteHalfWidth * s;
    const double darkHw = kDarkHalfWidth * s;

    const double eased = 1.0 - (1.0 - p) * (1.0 - p) * (1.0 - p); // ease-out cubic
    const double r = (0.35 + 0.65 * eased) * m_rippleSize * s;
    const double fade = 1.0 - p; // each pulse fades before the next one starts

    for (int y = 0; y < m_winSize; ++y) {
        std::uint8_t* row = pixels + static_cast<std::size_t>(y) * m_winSize * 4;
        const double dy = y - c;
        for (int x = 0; x < m_winSize; ++x) {
            const double dx = x - c;
            const double d = std::sqrt(dx * dx + dy * dy);
            double a = smooth(1.0 - std::fabs(d - r) / mainHw) * fade;
            BandColor col = mainC;
            const double aWhite = smooth(1.0 - std::fabs(d - (r + kWhiteOffset * s)) / whiteHw) * fade;
            if (aWhite > a) {
                a = aWhite;
                col = whiteC;
            }
            const double aDark = smooth(1.0 - std::fabs(d - (r + kDarkOffset * s)) / darkHw) * fade
                * (kDarkAlpha / 255.0);
            if (aDark > a) {
                a = aDark;
                col = darkC;
            }
            put(row + static_cast<std::size_t>(x) * 4, a, col);
        }
    }
}

void RippleOverlay::renderWaves(double s, double p)
{
    const BandColor mainC { m_accent.b, m_accent.g, m_accent.r };
    const BandColor whiteC { 255, 255, 255 };
    const BandColor darkC { 0, 0, 0 };
    const double c = (m_winSize - 1) / 2.0;
    auto pixels = static_cast<std::uint8_t*>(m_bits);
    const double mainHw = kMainHalfWidth * s;
    const double whiteHw = kWhiteHalfWidth * s;
    const double darkHw = kDarkHalfWidth * s;
    const double R = m_rippleSize * s;

    // Three ripples a third of a cycle apart: as one dies the next is born,
    // so the motion never stops between pulses.
    constexpr int kWaves = 3;
    double waveR[kWaves], waveA[kWaves];
    for (int i = 0; i < kWaves; ++i) {
        const double q = std::fmod(p + static_cast<double>(i) / kWaves, 1.0);
        const double eased = 1.0 - (1.0 - q) * (1.0 - q) * (1.0 - q);
        waveR[i] = (0.15 + 0.85 * eased) * R;
        waveA[i] = (1.0 - q) * (1.0 - 0.35 * q); // die out smoothly
    }

    for (int y = 0; y < m_winSize; ++y) {
        std::uint8_t* row = pixels + static_cast<std::size_t>(y) * m_winSize * 4;
        const double dy = y - c;
        for (int x = 0; x < m_winSize; ++x) {
            const double dx = x - c;
            const double d = std::sqrt(dx * dx + dy * dy);
            double a = 0.0;
            BandColor col = mainC;
            for (int i = 0; i < kWaves; ++i) {
                const double aMain = smooth(1.0 - std::fabs(d - waveR[i]) / mainHw) * waveA[i];
                if (aMain > a) {
                    a = aMain;
                    col = mainC;
                }
                const double aWhite = smooth(1.0 - std::fabs(d - (waveR[i] + kWhiteOffset * s)) / whiteHw) * waveA[i];
                if (aWhite > a) {
                    a = aWhite;
                    col = whiteC;
                }
                const double aDark = smooth(1.0 - std::fabs(d - (waveR[i] + kDarkOffset * s)) / darkHw)
                    * waveA[i] * (kDarkAlpha / 255.0);
                if (aDark > a) {
                    a = aDark;
                    col = darkC;
                }
            }
            put(row + static_cast<std::size_t>(x) * 4, a, col);
        }
    }
}

void RippleOverlay::renderHalo(double s, double p)
{
    const BandColor mainC { m_accent.b, m_accent.g, m_accent.r };
    const BandColor whiteC { 255, 255, 255 };
    const BandColor darkC { 0, 0, 0 };
    const double c = (m_winSize - 1) / 2.0;
    auto pixels = static_cast<std::uint8_t*>(m_bits);
    const double mainHw = kMainHalfWidth * s;
    const double whiteHw = kWhiteHalfWidth * s;
    const double darkHw = kDarkHalfWidth * s;
    const double rad = m_rippleSize * s;

    // A soft filled disc that breathes (grows ~15% and brightens each cycle),
    // with constant outline rings so the edge stays findable on any surface.
    const double breathe = 0.55 + 0.45 * std::sin(2.0 * 3.14159265358979323846 * p);
    const double glowRad = rad * (0.88 + 0.12 * breathe);
    const double fillA = 0.30 * breathe;
    const double rimA = 0.45 + 0.35 * breathe;

    for (int y = 0; y < m_winSize; ++y) {
        std::uint8_t* row = pixels + static_cast<std::size_t>(y) * m_winSize * 4;
        const double dy = y - c;
        for (int x = 0; x < m_winSize; ++x) {
            const double dx = x - c;
            const double d = std::sqrt(dx * dx + dy * dy);
            double a = smooth(1.0 - d / glowRad) * fillA; // soft fill, low alpha
            BandColor col = mainC;
            const double aRim = smooth(1.0 - std::fabs(d - glowRad) / mainHw) * rimA;
            if (aRim > a) {
                a = aRim;
                col = mainC;
            }
            const double aWhite = smooth(1.0 - std::fabs(d - (rad + 4.0 * s)) / whiteHw) * 0.9;
            if (aWhite > a) {
                a = aWhite;
                col = whiteC;
            }
            const double aDark = smooth(1.0 - std::fabs(d - (rad + 8.0 * s)) / darkHw) * (kDarkAlpha / 255.0);
            if (aDark > a) {
                a = aDark;
                col = darkC;
            }
            put(row + static_cast<std::size_t>(x) * 4, a, col);
        }
    }
}

void RippleOverlay::renderRadar(double s, double p)
{
    const BandColor mainC { m_accent.b, m_accent.g, m_accent.r };
    const BandColor darkC { 0, 0, 0 };
    const double c = (m_winSize - 1) / 2.0;
    auto pixels = static_cast<std::uint8_t*>(m_bits);
    const double mainHw = kMainHalfWidth * s;
    const double darkHw = kDarkHalfWidth * s;
    const double outerR = m_rippleSize * s;

    const double twoPi = 2.0 * 3.14159265358979323846;
    const double sweep = twoPi * p; // the sweep line's angle this frame
    const double trailSpan = twoPi / 3.0; // glow lags 120 degrees behind the line

    for (int y = 0; y < m_winSize; ++y) {
        std::uint8_t* row = pixels + static_cast<std::size_t>(y) * m_winSize * 4;
        const double dy = y - c;
        for (int x = 0; x < m_winSize; ++x) {
            const double dx = x - c;
            const double d = std::sqrt(dx * dx + dy * dy);
            double a = 0.0;
            BandColor col = mainC;

            // Static outer circle with a dark outline; faint disc background.
            const double aOuter = smooth(1.0 - std::fabs(d - outerR) / mainHw) * 0.9;
            if (aOuter > a) {
                a = aOuter;
                col = mainC;
            }
            const double aOuterDark = smooth(1.0 - std::fabs(d - (outerR + 6.0 * s)) / darkHw)
                * (kDarkAlpha / 255.0);
            if (aOuterDark > a) {
                a = aOuterDark;
                col = darkC;
            }
            if (d < outerR) {
                const double aBg = smooth(1.0 - d / outerR) * 0.10;
                if (aBg > a) {
                    a = aBg;
                    col = mainC;
                }
                // Sonar sweep: bright at the line, fading backwards over the
                // trail span. Screen y grows downward, so this sweeps
                // clockwise visually.
                const double ang = std::atan2(dy, dx); // [-pi, pi]
                double rel = std::fmod(sweep - ang, twoPi);
                if (rel < 0.0)
                    rel += twoPi;
                const double glow = smooth(1.0 - rel / trailSpan);
                const double aSweep = glow * glow * (0.35 + 0.65 * smooth(1.0 - d / outerR));
                if (aSweep > a) {
                    a = aSweep;
                    col = mainC;
                }
            }
            put(row + static_cast<std::size_t>(x) * 4, a, col);
        }
    }
}

void RippleOverlay::renderCustom(double s, double cycles)
{
    // Generic renderer for DSL presets and the user's StyleDoc. Strongest
    // alpha wins across layers; a missing/failed document degrades to the
    // ring, never to a blank overlay.
    if (!m_style || m_style->layers.empty()) {
        renderRing(s, std::fmod(cycles, 1.0));
        return;
    }
    const BandColor accentC { m_accent.b, m_accent.g, m_accent.r };
    const double c = (m_winSize - 1) / 2.0;
    auto pixels = static_cast<std::uint8_t*>(m_bits);
    const double R = m_rippleSize * s;
    const double twoPi = 2.0 * 3.14159265358979323846;

    for (int y = 0; y < m_winSize; ++y) {
        std::uint8_t* row = pixels + static_cast<std::size_t>(y) * m_winSize * 4;
        const double dy = y - c;
        for (int x = 0; x < m_winSize; ++x) {
            const double dx = x - c;
            const double d = std::sqrt(dx * dx + dy * dy);
            double a = 0.0;
            BandColor col = accentC;
            for (const StyleLayer& l : m_style->layers) {
                const double pe = std::fmod(cycles + l.phase, 1.0);
                double cand = 0.0;
                switch (l.shape) {
                case LayerShape::Ring: {
                    const double rf = l.animated ? l.from + (l.to - l.from) * evalEase(l.ease, pe)
                                                 : l.radius;
                    const double r = rf * R + l.offset * s;
                    const double hw = l.width * s;
                    if (std::fabs(d - r) > hw)
                        break; // quick reject: most pixels skip every ring
                    cand = smooth(1.0 - std::fabs(d - r) / hw) * evalAlpha(l.alpha, pe);
                    break;
                }
                case LayerShape::Disc: {
                    const double r = (l.animated ? l.from + (l.to - l.from) * evalEase(l.ease, pe) : l.radius)
                            * R
                        + l.offset * s;
                    if (r <= 0.0 || d > r)
                        break;
                    cand = smooth(1.0 - d / r) * evalAlpha(l.alpha, pe);
                    break;
                }
                case LayerShape::Sweep: {
                    const double r = (l.animated ? l.from + (l.to - l.from) * evalEase(l.ease, pe) : l.radius)
                            * R;
                    if (d > r || r <= 0.0)
                        break;
                    const double theta = sweepAngle(cycles, l.phase, l.speed);
                    const double ang = std::atan2(dy, dx); // screen y is down:
                    const double rel = sweepTrailAngle(theta, ang, l.speed);
                    const double trail = twoPi * l.trailDeg / 360.0;
                    const double glow = smooth(1.0 - rel / trail);
                    cand = evalAlpha(l.alpha, pe) * glow * glow
                        * (0.35 + 0.65 * smooth(1.0 - d / r));
                    if (l.fill) {
                        const double bg = evalAlpha(l.alpha, pe) * 0.12 * smooth(1.0 - d / r);
                        if (bg > cand)
                            cand = bg;
                    }
                    break;
                }
                }
                if (cand > a) {
                    a = cand;
                    col = l.useAccent ? accentC : BandColor { l.color.b, l.color.g, l.color.r };
                }
            }
            put(row + static_cast<std::size_t>(x) * 4, a, col);
        }
    }
}

void RippleOverlay::stampDot(double s)
{
    // Constant center marker shared by every style: accent disc with a white
    // rim, so the exact pointer spot stays visible between pulses.
    const BandColor mainC { m_accent.b, m_accent.g, m_accent.r };
    const BandColor whiteC { 255, 255, 255 };
    const double c = (m_winSize - 1) / 2.0;
    auto pixels = static_cast<std::uint8_t*>(m_bits);
    const double dotR = kDotRadius * s;
    const double rimR = (kDotRadius + kDotRimOuter) / 2.0 * s;
    const double rimHw = (kDotRimOuter - kDotRadius) / 2.0 * s;

    const int half = static_cast<int>(kDotRimOuter * s) + 2;
    const int ci = static_cast<int>(c);
    const int y0 = ci - half < 0 ? 0 : ci - half;
    const int y1 = ci + half >= m_winSize ? m_winSize - 1 : ci + half;
    const int x0 = y0, x1 = y1; // square window, same bounds on both axes
    for (int y = y0; y <= y1; ++y) {
        std::uint8_t* row = pixels + static_cast<std::size_t>(y) * m_winSize * 4;
        const double dy = y - c;
        for (int x = x0; x <= x1; ++x) {
            const double dx = x - c;
            const double d = std::sqrt(dx * dx + dy * dy);
            const double aDot = smooth(1.0 - d / dotR);
            const double aDotRim = smooth(1.0 - std::fabs(d - rimR) / rimHw);
            std::uint8_t* px = row + static_cast<std::size_t>(x) * 4;
            if (aDot >= aDotRim) {
                if (aDot > 0.0)
                    put(px, aDot, mainC);
            } else if (aDotRim > 0.0) {
                put(px, aDotRim, whiteC);
            }
        }
    }
}

LRESULT CALLBACK RippleOverlay::procStatic(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    RippleOverlay* self = nullptr;
    if (msg == WM_NCCREATE) {
        const CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<RippleOverlay*>(cs->lpCreateParams);
        ::SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<RippleOverlay*>(::GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    if (self)
        return self->proc(hwnd, msg, wp, lp);
    return ::DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT RippleOverlay::proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_TIMER:
        if (wp == kFrameTimer)
            drawFrame(hwnd);
        return 0;
    case WM_NCHITTEST:
        return HTTRANSPARENT; // belt and braces: never the target of a click
    default:
        return ::DefWindowProcW(hwnd, msg, wp, lp);
    }
}

} // namespace fm
