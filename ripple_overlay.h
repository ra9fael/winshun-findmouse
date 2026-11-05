// ripple_overlay.h — a layered, click-through, non-activating topmost window
// on its own UI thread that draws a two-color ripple ring at the pointer.
//
// Contract:
//  * request() is called from the hook thread and the protocol thread; it is
//    a single PostThreadMessage — the x/y ride in wParam/lParam, so there is
//    no queue and no lock on any caller.
//  * All GDI work and all state live on the overlay thread.
//  * updateStyle() applies new settings the same way (values packed into the
//    message; nothing shared).
//  * The window is WS_EX_TRANSPARENT|WS_EX_LAYERED: it never takes a click
//    and never steals focus. The hook always sees the real input stream.
#pragma once

#include "settings.h"

#include <atomic>
#include <memory>
#include <thread>
#include <windows.h>

namespace fm {

class RippleOverlay {
public:
    RippleOverlay() = default;
    ~RippleOverlay() { stop(); }
    RippleOverlay(const RippleOverlay&) = delete;
    RippleOverlay& operator=(const RippleOverlay&) = delete;

    bool start();
    void stop();
    bool active() const { return m_running.load(); }

    // Show a beacon at (x, y) that then follows the pointer until the user
    // presses a mouse button (left or right) or the follow timeout elapses
    // (0 = never auto-dismiss); it then decays to transparent over fadeOutMs
    // (0 = hide at once). A new trigger restarts the animation, also mid-fade.
    void request(int x, int y);

    // Apply new settings. customDoc carries the parsed custom-style JSON and
    // may be null (Custom then renders the ring fallback). The document is
    // shared; the overlay thread only reads it.
    void updateStyle(const Settings& settings, std::shared_ptr<const StyleDoc> customDoc);

private:
    // Everything a style change needs, posted to the overlay thread as one
    // heap block (PostThreadMessage only carries two machine words).
    struct StyleMsg {
        int rippleMs;
        int rippleSize;
        int followTimeoutMs;
        int fadeOutMs;
        Effect effect;
        Rgb accent;
        std::shared_ptr<const StyleDoc> doc;
    };

    void run();
    static LRESULT CALLBACK procStatic(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    void beginFollow(HWND hwnd, int x, int y);
    void beginFade(HWND hwnd, long long now);
    void endFollow(HWND hwnd);
    void drawFrame(HWND hwnd);
    void applyStyle(const StyleMsg& m);
    bool ensureDib();
    void present(HWND hwnd, BYTE alpha);

    // One render pass per style; each fills the whole frame buffer, then the
    // shared center dot is stamped on top (it must survive every style).
    void renderRing(double s, double p);
    void renderWaves(double s, double p);
    void renderHalo(double s, double p);
    void renderRadar(double s, double p);
    void renderCustom(double s, double p);
    void stampDot(double s);

    std::thread m_thread;
    std::atomic<bool> m_running { false };
    std::atomic<DWORD> m_threadId { 0 };

    // Overlay-thread-only state below.
    HWND m_hwnd = nullptr;
    Effect m_effect = Effect::Ring;
    int m_rippleMs = 700; // one pulse cycle: expand from 35% to full and fade
    int m_rippleSize = 64; // ring's outer radius, px at 96 dpi
    int m_followTimeoutMs = 0; // 0 = follow until a mouse button
    int m_fadeOutMs = 400; // decay after the end condition; 0 = hide at once
    std::shared_ptr<const StyleDoc> m_style; // Custom's document (may be null)
    Rgb m_accent;
    int m_scalePct = 100; // dpi*100/96 of the monitor the trigger fired on
    bool m_following = false;
    long long m_followStart = 0; // for the auto-dismiss timeout
    bool m_fading = false; // end condition met; decaying to transparent
    long long m_fadeStart = 0;
    long long m_pulseStart = 0; // current expansion cycle's origin
    int m_cx = 0, m_cy = 0; // window center in screen coordinates
    int m_winSize = 0; // square window edge, device pixels
    HBITMAP m_dib = nullptr; // reused frame buffer, resized with the window
    void* m_bits = nullptr;
};

} // namespace fm
