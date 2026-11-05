// shake_logic.h — pure shake-gesture state machine (no OS dependencies).
//
// A "shake" is a quick horizontal wiggle of the pointer. The detector tracks
// direction reversals of the X axis with hysteresis (an extreme-tracking
// scheme, so a single O(1) update per event) and fires once when enough
// reversals happen inside a short time window. It stays quiet for a cooldown
// afterwards so one gesture can never chain into a burst of ripples.
//
// Design constraints (findmouse-plugin-design.md §5):
//  * O(1) time and memory per event — it runs inside a WH_MOUSE_LL hook,
//    where the system budget is a few milliseconds and stalls are visible.
//  * No allocation, no clocks of its own: the caller passes a monotonic
//    timestamp in milliseconds (GetTickCount64() in production, a synthetic
//    counter in tests).
//  * Vertical wiggling is deliberately not detected in v0.1: X-only keeps the
//    false-positive rate low for scrolling touchpads and text selection.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace fm {

// Maximum tracked reversals. minReversals is clamped to [2, kMaxReversals].
inline constexpr std::size_t kMaxReversals = 8;

struct ShakeConfig {
    int windowMs = 300;     // reversal span that still counts as one gesture
    int minReversals = 4;   // direction changes needed (2 = one there-and-back)
    int strokePx = 8;       // retreat from the extreme that marks a reversal
    int cooldownMs = 1500;  // quiet time after a trigger
    int minSpeedPxS = 500;  // strokes slower than this (peak speed) are ignored
};

// Clamp a config into safe bounds; out-of-range settings (a bad settings file,
// a typo in plugin.json) degrade to the defaults instead of misbehaving.
inline ShakeConfig sanitized(ShakeConfig c)
{
    if (c.windowMs < 80)
        c.windowMs = 80;
    if (c.windowMs > 2000)
        c.windowMs = 2000;
    if (c.minReversals < 2)
        c.minReversals = 2;
    if (c.minReversals > static_cast<int>(kMaxReversals))
        c.minReversals = static_cast<int>(kMaxReversals);
    if (c.strokePx < 2)
        c.strokePx = 2;
    if (c.strokePx > 200)
        c.strokePx = 200;
    if (c.cooldownMs < 0)
        c.cooldownMs = 0;
    if (c.cooldownMs > 10000)
        c.cooldownMs = 10000;
    if (c.minSpeedPxS < 0)
        c.minSpeedPxS = 0;
    if (c.minSpeedPxS > 20000)
        c.minSpeedPxS = 20000;
    return c;
}

struct ShakeStats {
    long long moves = 0;       // every position the caller fed in
    long long reversals = 0;   // accepted direction changes
    long long slowStrokes = 0; // reversals suppressed by the speed gate
    long long triggers = 0;
};

enum class FeedResult { None, Reversal, SlowStroke, Trigger };

class ShakeDetector {
public:
    explicit ShakeDetector(ShakeConfig cfg = {})
        : m_cfg(sanitized(cfg))
    {
    }

    // A fresh config starts a fresh gesture: pending cooldowns and motion
    // history belonged to the old parameters. Counters survive (they are
    // diagnostics, not state).
    void setConfig(ShakeConfig cfg)
    {
        m_cfg = sanitized(cfg);
        resetMotion();
        m_armed = true;
        m_cooldownUntil = 0;
    }

    const ShakeConfig& config() const { return m_cfg; }
    ShakeStats stats() const { return m_stats; }
    bool armed() const { return m_armed; }

    // Forget the motion history but keep the counters and the cooldown. Used
    // when the context makes the history meaningless (a button went down, the
    // detector was re-enabled).
    void resetMotion()
    {
        m_haveLast = false;
        m_dir = 0;
        m_extreme = 0;
        m_strokePeakSpeed = 0;
        m_count = 0;
        m_head = 0;
    }

    // Full reset, counters included (tests, plugin start).
    void reset()
    {
        resetMotion();
        m_armed = true;
        m_cooldownUntil = 0;
        m_stats = ShakeStats {};
    }

    // Feed one pointer position (x only; the Y axis is ignored in v0.2).
    // nowMs must be monotonic. Returns Trigger exactly once per gesture,
    // Reversal for an accepted direction change, SlowStroke when the speed
    // gate swallowed one.
    FeedResult feed(long long nowMs, int x)
    {
        ++m_stats.moves;

        if (!m_armed) {
            if (nowMs < m_cooldownUntil)
                return FeedResult::None;
            // Cooldown over: re-arm from a clean slate so the pre-cooldown
            // history cannot immediately re-fire.
            resetMotion();
            m_armed = true;
        }

        if (!m_haveLast) {
            m_haveLast = true;
            m_lastX = x;
            m_lastEventMs = nowMs;
            m_extreme = x; // anchor until the first stroke is long enough
            m_dir = 0;
            m_strokePeakSpeed = 0;
            return FeedResult::None;
        }

        // Peak speed within the current stroke. Per-event speed is noisy at
        // high report rates, so it is only used as a maximum over the stroke;
        // the gate then asks "was this stroke ever fast", not "is every
        // sample fast". dt == 0 (coalesced events) cannot judge speed.
        const long long dt = nowMs - m_lastEventMs;
        m_lastEventMs = nowMs;
        const int dx = x - m_lastX;
        m_lastX = x;
        if (dx != 0 && dt > 0) {
            const int speed = static_cast<int>((dx > 0 ? dx : -dx) * 1000 / dt);
            if (speed > m_strokePeakSpeed)
                m_strokePeakSpeed = speed;
        }

        if (m_dir == 0) {
            // No direction yet: m_extreme is the anchor the pointer must get
            // strokePx away from before a direction exists.
            if (x - m_extreme >= m_cfg.strokePx) {
                m_dir = 1;
                m_extreme = x;
                m_strokePeakSpeed = 0;
            } else if (m_extreme - x >= m_cfg.strokePx) {
                m_dir = -1;
                m_extreme = x;
                m_strokePeakSpeed = 0;
            }
            return FeedResult::None;
        }

        if (m_dir > 0) {
            if (x > m_extreme) {
                m_extreme = x; // still pushing out: just move the extreme
                return FeedResult::None;
            }
            if (m_extreme - x < m_cfg.strokePx)
                return FeedResult::None; // small retreat: hysteresis absorbs it
        } else {
            if (x < m_extreme) {
                m_extreme = x;
                return FeedResult::None;
            }
            if (x - m_extreme < m_cfg.strokePx)
                return FeedResult::None;
        }

        // A reversal: the pointer retreated strokePx from the extreme.
        m_dir = -m_dir;
        m_extreme = x;
        if (m_strokePeakSpeed < m_cfg.minSpeedPxS) {
            // Too slow to be a shake (a drag, a slow glide): the geometry
            // still flips, but the gesture machinery ignores it.
            ++m_stats.slowStrokes;
            m_strokePeakSpeed = 0;
            return FeedResult::SlowStroke;
        }
        m_strokePeakSpeed = 0;
        ++m_stats.reversals;
        m_times[m_head] = nowMs;
        m_head = (m_head + 1) % kMaxReversals;
        if (m_count < static_cast<int>(kMaxReversals))
            ++m_count;

        if (m_count < m_cfg.minReversals)
            return FeedResult::Reversal;

        // Span of the newest minReversals entries: m_head is the next write,
        // so the oldest of them sits minReversals slots behind it.
        const int oldest = static_cast<int>((m_head + kMaxReversals
            - static_cast<std::size_t>(m_cfg.minReversals)) % kMaxReversals);
        if (nowMs - m_times[static_cast<std::size_t>(oldest)] > m_cfg.windowMs)
            return FeedResult::Reversal; // too slow: keep counting, no trigger

        // Gesture recognised. One shot, then quiet for the cooldown.
        ++m_stats.triggers;
        m_armed = false;
        m_cooldownUntil = nowMs + m_cfg.cooldownMs;
        resetMotion();
        return FeedResult::Trigger;
    }

private:
    ShakeConfig m_cfg;
    ShakeStats m_stats;
    bool m_armed = true;
    long long m_cooldownUntil = 0;
    bool m_haveLast = false;
    int m_lastX = 0; // for per-event dx (speed tracking)
    long long m_lastEventMs = 0;
    int m_strokePeakSpeed = 0; // peak px/s since the current stroke began
    int m_dir = 0; // -1 left, 0 unknown, +1 right
    int m_extreme = 0; // extreme position in m_dir (anchor while m_dir == 0)
    std::array<long long, kMaxReversals> m_times {};
    int m_count = 0; // valid entries in m_times (saturates at kMaxReversals)
    std::size_t m_head = 0; // next write slot in m_times
};

} // namespace fm
