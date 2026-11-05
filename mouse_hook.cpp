// mouse_hook.cpp — low-level mouse hook implementation.
//
// WM_APP_CONFIG carries no payload: the config itself sits in m_pendingConfig
// guarded by m_pendingMutex; the message just tells the hook thread when to
// pick it up. That keeps the hot path lock-free without atomics-wide structs.
#include "mouse_hook.h"

#include <windows.h>

namespace fm {

namespace {
MouseHook* g_hook = nullptr; // exactly one instance per process

constexpr UINT WM_APP_CONFIG = WM_APP + 1;
} // namespace

bool MouseHook::start(ShakeConfig config, TriggerSink onTrigger)
{
    if (m_running.load())
        return true;
    m_onTrigger = std::move(onTrigger);
    m_detector = ShakeDetector(config);
    m_enabled.store(true);
    m_running.store(true);
    try {
        m_thread = std::thread([this] { run(); });
    } catch (...) {
        m_running.store(false);
        return false;
    }
    return true;
}

void MouseHook::stop()
{
    if (m_thread.joinable()) {
        m_running.store(false);
        ::PostThreadMessageW(m_threadId, WM_QUIT, 0, 0);
        m_thread.join();
    }
}

void MouseHook::updateConfig(const ShakeConfig& config)
{
    {
        const std::lock_guard lock(m_pendingMutex);
        m_pendingConfig = config;
        m_pendingValid = true;
    }
    if (m_threadId != 0)
        ::PostThreadMessageW(m_threadId, WM_APP_CONFIG, 0, 0);
}

MouseHook::Stats MouseHook::stats() const
{
    Stats s;
    s.moves = m_moves.load(std::memory_order_relaxed);
    s.reversals = m_reversals.load(std::memory_order_relaxed);
    s.slowStrokes = m_slowStrokes.load(std::memory_order_relaxed);
    s.triggers = m_triggers.load(std::memory_order_relaxed);
    return s;
}

void MouseHook::run()
{
    m_threadId = ::GetCurrentThreadId();
    g_hook = this;

    // The installing thread must pump messages for the hook to fire.
    const HHOOK hook = ::SetWindowsHookExW(WH_MOUSE_LL, &MouseHook::hookProcStatic,
        ::GetModuleHandleW(nullptr), 0);
    if (hook) {
        MSG msg;
        while (m_running.load()) {
            const BOOL got = ::GetMessageW(&msg, nullptr, 0, 0);
            if (got <= 0)
                break;
            if (msg.message == WM_APP_CONFIG) {
                std::lock_guard lock(m_pendingMutex);
                if (m_pendingValid) {
                    m_detector.setConfig(m_pendingConfig);
                    m_pendingValid = false;
                }
            }
        }
        ::UnhookWindowsHookEx(hook);
    } else {
        // Install failed (rare: blocked by policy). Surface it via the
        // diagnostics counters staying at zero rather than a crash.
        m_running.store(false);
    }
    g_hook = nullptr;
    m_running.store(false);
}

LRESULT CALLBACK MouseHook::hookProcStatic(int code, WPARAM wp, LPARAM lp)
{
    if (g_hook && code >= 0) {
        g_hook->hookProc(code, wp, lp); // result ignored: never swallow
    }
    return ::CallNextHookEx(nullptr, code, wp, lp);
}

LRESULT MouseHook::hookProc(int /*code*/, WPARAM wp, LPARAM lp)
{
    const MSLLHOOKSTRUCT* m = reinterpret_cast<const MSLLHOOKSTRUCT*>(lp);
    if (!m)
        return 0;

    switch (wp) {
    case WM_MOUSEMOVE: {
        if (m->flags & LLMHF_INJECTED)
            return 0; // simulated input (tests, remote tools) is not a gesture
        m_moves.fetch_add(1, std::memory_order_relaxed);
        if (m_buttonHeld)
            return 0; // drags are not shakes
        if (!m_enabled.load(std::memory_order_relaxed))
            return 0;

        const FeedResult r = m_detector.feed(static_cast<long long>(::GetTickCount64()), m->pt.x);
        if (r == FeedResult::Reversal) {
            m_reversals.fetch_add(1, std::memory_order_relaxed);
        } else if (r == FeedResult::SlowStroke) {
            m_slowStrokes.fetch_add(1, std::memory_order_relaxed);
        } else if (r == FeedResult::Trigger) {
            m_triggers.fetch_add(1, std::memory_order_relaxed);
            if (m_onTrigger)
                m_onTrigger(m->pt.x, m->pt.y);
        }
        return 0;
    }
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_XBUTTONDOWN:
        m_buttonHeld = true;
        m_detector.resetMotion(); // a drag's history is not a gesture's
        return 0;
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP:
    case WM_XBUTTONUP:
        m_buttonHeld = false;
        m_detector.resetMotion();
        return 0;
    default:
        return 0;
    }
}

} // namespace fm
