// mouse_hook.h — the WH_MOUSE_LL hook on its own message-pumping thread.
//
// Contract (findmouse-plugin-design.md §5):
//  * The callback does O(µs) work only: read the point, check the gates, feed
//    the pure detector. The trigger sink must only enqueue (the overlay's
//    request() posts a message and returns).
//  * Nothing is ever swallowed: the proc always ends in CallNextHookEx, does
//    not inject input, and skips injected events (LLMHF_INJECTED) so tools
//    that move the cursor programmatically cannot ring the bell.
//  * Config is confined to the hook thread: settingsChanged posts a message
//    and the hook thread applies it between events. No locks on the hot path.
#pragma once

#include "shake_logic.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <thread>
#include <windows.h>

namespace fm {

class MouseHook {
public:
    // Runs on the hook thread; must be non-blocking (enqueue + post).
    using TriggerSink = std::function<void(int x, int y)>;

    MouseHook() = default;
    ~MouseHook() { stop(); }
    MouseHook(const MouseHook&) = delete;
    MouseHook& operator=(const MouseHook&) = delete;

    bool start(ShakeConfig config, TriggerSink onTrigger);
    void stop();
    bool active() const { return m_running.load(); }

    // Gates every event; atomic so the action path can call it directly.
    void setEnabled(bool on) { m_enabled.store(on); }
    bool enabled() const { return m_enabled.load(); }

    // Thread-safe: stashes the config and wakes the hook thread to apply it.
    void updateConfig(const ShakeConfig& config);

    // Counter snapshot for diagnostics (relaxed atomics, order not needed).
    struct Stats {
        long long moves = 0;
        long long reversals = 0;
        long long slowStrokes = 0;
        long long triggers = 0;
    };
    Stats stats() const;

private:
    void run();
    static LRESULT CALLBACK hookProcStatic(int code, WPARAM wp, LPARAM lp);
    LRESULT hookProc(int code, WPARAM wp, LPARAM lp);

    std::thread m_thread;
    std::atomic<bool> m_running { false };
    std::atomic<bool> m_enabled { true };
    std::atomic<long long> m_moves { 0 };
    std::atomic<long long> m_reversals { 0 };
    std::atomic<long long> m_slowStrokes { 0 };
    std::atomic<long long> m_triggers { 0 };
    DWORD m_threadId = 0;
    TriggerSink m_onTrigger;

    // Brief: written by any thread under a lock, read by the hook thread only
    // after it sees WM_APP_CONFIG.
    std::mutex m_pendingMutex;
    ShakeConfig m_pendingConfig;
    bool m_pendingValid = false;

    // Hook-thread-only state.
    ShakeDetector m_detector;
    bool m_buttonHeld = false;
};

} // namespace fm
