// settings.h — plugin.json settings parsed into a validated snapshot.
//
// Every field is clamped: a hand-edited settings file must degrade to the
// default behaviour, never to a hook that fires on nothing or on everything.
// The color parser and the clamps are pure and covered by unit tests.
#pragma once

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "shake_logic.h"
#include "winshun.h"

namespace fm {

struct Rgb {
    std::uint8_t r = 0x2A;
    std::uint8_t g = 0xA8;
    std::uint8_t b = 0xFF;
};

// "#RRGGBB", "RRGGBB" or "#RGB" (each hex digit doubled). ASCII case-insensitive.
inline bool parseColor(const std::string& text, Rgb& out)
{
    std::string s;
    s.reserve(text.size());
    for (char c : text) {
        if (c == ' ' || c == '#' || c == '"')
            continue;
        if (c >= 'A' && c <= 'F')
            c = static_cast<char>(c - 'A' + 'a');
        s.push_back(c);
    }
    auto hexVal = [](char c) -> int {
        if (c >= '0' && c <= '9')
            return c - '0';
        if (c >= 'a' && c <= 'f')
            return c - 'a' + 10;
        return -1;
    };
    if (s.size() == 3) {
        const int r = hexVal(s[0]), g = hexVal(s[1]), b = hexVal(s[2]);
        if (r < 0 || g < 0 || b < 0)
            return false;
        out = Rgb { static_cast<std::uint8_t>(r * 17), static_cast<std::uint8_t>(g * 17),
            static_cast<std::uint8_t>(b * 17) };
        return true;
    }
    if (s.size() == 6) {
        const int r1 = hexVal(s[0]), r2 = hexVal(s[1]);
        const int g1 = hexVal(s[2]), g2 = hexVal(s[3]);
        const int b1 = hexVal(s[4]), b2 = hexVal(s[5]);
        if (r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0)
            return false;
        out = Rgb { static_cast<std::uint8_t>(r1 * 16 + r2), static_cast<std::uint8_t>(g1 * 16 + g2),
            static_cast<std::uint8_t>(b1 * 16 + b2) };
        return true;
    }
    return false;
}

inline int clampInt(long long v, int lo, int hi)
{
    if (v < lo)
        return lo;
    if (v > hi)
        return hi;
    return static_cast<int>(v);
}

// The beacon's look. All styles follow the pointer and end on a mouse button;
// only the drawing differs. Custom is a user-provided JSON style (see
// StyleDoc below); if it is missing or fails to parse the renderer falls
// back to Ring, never to a blank overlay.
enum class Effect {
    Ring, // pulsing expanding ring (safe fallback)
    Waves, // three staggered expanding ripples
    Halo, // breathing soft glow disc around the pointer
    Radar, // rotating sonar sweep inside a static circle
    Ping, // DSL preset: fast ring plus a slow echo (presets.h)
    Heart, // default DSL preset: lub-dub double beat (presets.h)
    Orbit, // DSL preset: two counter-rotating sweeps (presets.h)
    Custom // layers from the user's customStyle JSON
};

// ASCII-lowercase canonical names; also echoed in diagnostics.
inline const char* effectName(Effect e)
{
    switch (e) {
    case Effect::Waves:
        return "waves";
    case Effect::Halo:
        return "halo";
    case Effect::Radar:
        return "radar";
    case Effect::Ping:
        return "ping";
    case Effect::Heart:
        return "heart";
    case Effect::Orbit:
        return "orbit";
    case Effect::Custom:
        return "custom";
    case Effect::Ring:
        break;
    }
    return "ring";
}

// Unknown or missing names degrade to Ring, never to a blank overlay.
inline Effect parseEffect(std::string s)
{
    for (char& c : s)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    if (s == "waves")
        return Effect::Waves;
    if (s == "halo")
        return Effect::Halo;
    if (s == "radar")
        return Effect::Radar;
    if (s == "ping")
        return Effect::Ping;
    if (s == "heart")
        return Effect::Heart;
    if (s == "orbit")
        return Effect::Orbit;
    if (s == "custom")
        return Effect::Custom;
    return Effect::Ring;
}

inline bool usesStyleDoc(Effect e)
{
    return e == Effect::Custom || e == Effect::Ping || e == Effect::Heart || e == Effect::Orbit;
}

// ---------- Custom style DSL ----------
//
// A declarative JSON description of beacon layers, deliberately NOT a
// scripting language: radii are structured curve objects (from/to + a named
// easing), never user-supplied expressions, so there is nothing to inject.
// The generic renderer draws layers front-to-back with "strongest alpha
// wins" compositing, clamps every number, and rejects the whole document on
// any unknown shape or unparseable color (the caller then falls back).
//
// {
//   "loopMs": 900,                       // optional; 0/missing = rippleMs setting
//   "layers": [
//     {"shape": "ring",  "from": 0.2, "to": 1.0, "ease": "outCubic",
//      "width": 3.5, "offset": 0, "phase": 0,
//      "alpha": {"mode": "linearFade", "value": 1.0}, "color": "accent"},
//     {"shape": "disc",  "radius": 0.05, "alpha": 1.0, "color": "accent"},
//     {"shape": "sweep", "speed": 1.0, "trailDeg": 120, "fill": true,
//      "alpha": {"mode": "constant", "value": 0.85}, "color": "accent"}
//   ]
// }
//
// Radius fields ("from"/"to"/"radius") are fractions of the rippleSize
// setting; "width"/"offset" are px at 96 dpi. "phase" (0..1) shifts a layer's
// cycle — three rings at phases 0, 1/3, 2/3 make continuous waves.

enum class Ease { Linear, OutCubic, InQuad, OutQuad, Sin };

inline Ease parseEase(std::string s)
{
    for (char& c : s)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    if (s == "outcubic")
        return Ease::OutCubic;
    if (s == "inquad")
        return Ease::InQuad;
    if (s == "outquad")
        return Ease::OutQuad;
    if (s == "sin")
        return Ease::Sin;
    return Ease::Linear;
}

inline double evalEase(Ease e, double p)
{
    switch (e) {
    case Ease::OutCubic:
        return 1.0 - (1.0 - p) * (1.0 - p) * (1.0 - p);
    case Ease::InQuad:
        return p * p;
    case Ease::OutQuad:
        return 1.0 - (1.0 - p) * (1.0 - p);
    case Ease::Sin:
        return 0.55 + 0.45 * std::sin(2.0 * 3.14159265358979323846 * p);
    case Ease::Linear:
        break;
    }
    return p;
}

enum class AlphaMode { Constant, LinearFade, HoldFade, Sin };

struct AlphaSpec {
    AlphaMode mode = AlphaMode::Constant;
    double value = 1.0; // peak alpha, 0..1
};

inline double evalAlpha(const AlphaSpec& a, double p)
{
    switch (a.mode) {
    case AlphaMode::LinearFade:
        return a.value * (1.0 - p);
    case AlphaMode::HoldFade:
        return a.value * (p < 0.6 ? 1.0 : 1.0 - (p - 0.6) / 0.4);
    case AlphaMode::Sin:
        return a.value * (0.55 + 0.45 * std::sin(2.0 * 3.14159265358979323846 * p));
    case AlphaMode::Constant:
        break;
    }
    return a.value;
}

// alpha accepts an object {"mode","value"} or a bare number (constant).
inline bool parseAlpha(const winshun::Json& j, AlphaSpec& a)
{
    if (j.isNumber()) {
        a.mode = AlphaMode::Constant;
        a.value = clampInt(static_cast<long long>(j.toNumber() * 1000.0), 0, 1000) / 1000.0;
        return true;
    }
    if (!j.isObject())
        return false;
    const winshun::Json& mode = j["mode"];
    if (!mode.isString())
        return false;
    const std::string m = mode.toString();
    if (m == "constant")
        a.mode = AlphaMode::Constant;
    else if (m == "linearFade")
        a.mode = AlphaMode::LinearFade;
    else if (m == "holdFade")
        a.mode = AlphaMode::HoldFade;
    else if (m == "sin")
        a.mode = AlphaMode::Sin;
    else
        return false;
    a.value = 1.0;
    const winshun::Json& v = j["value"];
    if (v.isNumber())
        a.value = clampInt(static_cast<long long>(v.toNumber() * 1000.0), 0, 1000) / 1000.0;
    return true;
}

enum class LayerShape { Ring, Disc, Sweep };

struct StyleLayer {
    LayerShape shape = LayerShape::Ring;
    double phase = 0.0; // 0..1, added to the cycle progress
    bool animated = false;
    Ease ease = Ease::Linear; // radius curve when animated
    double from = 1.0; // animated radius, fraction of rippleSize
    double to = 1.0;
    double radius = 1.0; // static radius, fraction of rippleSize
    double width = 3.5; // ring half-width, px at 96 dpi
    double offset = 0.0; // outward offset, px at 96 dpi
    AlphaSpec alpha;
    bool useAccent = true; // false: the layer's own color
    Rgb color;
    // Sweep only:
    double speed = 1.0; // turns per loop (negative = the other way)
    double trailDeg = 120.0; // glow lags this far behind the line
    bool fill = false; // faint disc under the sweep
};

struct StyleDoc {
    int loopMs = 0; // 0 = follow the rippleMs setting
    std::vector<StyleLayer> layers;
};

// Sweeps keep their fractional turns across pulse boundaries. Wrapping the
// pulse first makes a speed such as -0.6 jump back every loop.
inline double sweepAngle(double cycles, double phase, double speed)
{
    constexpr double twoPi = 6.28318530717958647692;
    return twoPi * std::fmod((cycles + phase) * speed, 1.0);
}

inline double sweepTrailAngle(double theta, double angle, double speed)
{
    constexpr double twoPi = 6.28318530717958647692;
    double rel = std::fmod((theta - angle) * (speed < 0.0 ? -1.0 : 1.0), twoPi);
    return rel < 0.0 ? rel + twoPi : rel;
}

// Strict: any unknown shape/mode/color or a missing/empty "layers" fails the
// whole document, so typos surface in diagnostics instead of half-rendering.
constexpr std::size_t kMaxStyleLayers = 12;

inline bool parseStyleLayer(const winshun::Json& j, StyleLayer& l)
{
    if (!j.isObject())
        return false;
    const winshun::Json& shape = j["shape"];
    if (!shape.isString())
        return false;
    const std::string sh = shape.toString();
    if (sh == "ring")
        l.shape = LayerShape::Ring;
    else if (sh == "disc")
        l.shape = LayerShape::Disc;
    else if (sh == "sweep")
        l.shape = LayerShape::Sweep;
    else
        return false;

    const auto num = [&](const char* key, double lo, double hi, double fallback) -> double {
        const winshun::Json& v = j[key];
        if (!v.isNumber())
            return fallback;
        const double d = v.toNumber();
        return d < lo ? lo : (d > hi ? hi : d);
    };
    l.phase = num("phase", 0.0, 1.0, l.phase);
    l.width = num("width", 0.5, 40.0, l.width);
    l.offset = num("offset", 0.0, 40.0, l.offset);
    l.radius = num("radius", 0.0, 1.0, l.radius);
    l.speed = num("speed", -4.0, 4.0, l.speed);
    l.trailDeg = num("trailDeg", 1.0, 360.0, l.trailDeg);

    const winshun::Json& from = j["from"];
    const winshun::Json& to = j["to"];
    if (from.isNumber() || to.isNumber()) {
        l.animated = true;
        l.from = num("from", 0.0, 1.0, l.radius);
        l.to = num("to", 0.0, 1.0, l.radius);
    }

    const winshun::Json& ease = j["ease"];
    if (ease.isString())
        l.ease = parseEase(ease.toString());

    const winshun::Json& alpha = j["alpha"];
    if (!alpha.isNull() && (alpha.isObject() || alpha.isNumber())) {
        if (!parseAlpha(alpha, l.alpha))
            return false;
    }

    const winshun::Json& color = j["color"];
    if (color.isString()) {
        const std::string c = color.toString();
        if (c == "accent") {
            l.useAccent = true;
        } else {
            Rgb parsed;
            if (!parseColor(c, parsed))
                return false;
            l.useAccent = false;
            l.color = parsed;
        }
    }

    const winshun::Json& fill = j["fill"];
    if (fill.isBool())
        l.fill = fill.toBool();
    else if (fill.isNumber())
        l.fill = fill.toInt() != 0;
    return true;
}

inline bool parseStyleDoc(const winshun::Json& j, StyleDoc& out)
{
    if (!j.isObject())
        return false;
    out = StyleDoc {};
    out.loopMs = 0;
    const winshun::Json& loop = j["loopMs"];
    if (loop.isNumber())
        out.loopMs = clampInt(loop.toInt(), 200, 5000);

    const winshun::Json& layers = j["layers"];
    if (!layers.isArray())
        return false;
    const auto& items = layers.toArray();
    if (items.empty() || items.size() > kMaxStyleLayers)
        return false;
    for (const winshun::Json& item : items) {
        StyleLayer l;
        if (!parseStyleLayer(item, l))
            return false;
        out.layers.push_back(l);
    }
    return !out.layers.empty();
}

struct Settings {
    bool shakeEnabled = true;
    ShakeConfig shake; // already sanitized
    Effect effect = Effect::Heart;
    int rippleMs = 700; // 200..3000
    int rippleSize = 64; // ring's outer radius at rest, px at 96 dpi; 24..256
    int followTimeoutMs = 3000; // auto-dismiss; 0 = follow until a mouse button
    int fadeOutMs = 400; // decay when the beacon ends; 0..3000, 0 = hide at once
    Rgb accent;
    std::string accentText = "#2AA8FF"; // as given, echoed in diagnostics
    std::string customStyleText; // raw customStyle setting; parsed by the caller
};

// Map the parsed settings onto the detector config. Kept next to Settings so
// the field names stay in one place.
inline ShakeConfig shakeConfigOf(const Settings& s)
{
    ShakeConfig c;
    c.windowMs = s.shake.windowMs;
    c.minReversals = s.shake.minReversals;
    c.strokePx = s.shake.strokePx;
    c.cooldownMs = s.shake.cooldownMs;
    return sanitized(c);
}

// Read the initialize/settingsChanged payload. Missing or wrong-typed fields
// keep the defaults; numbers arriving as strings (host JSON quirk safety) are
// still accepted.
inline Settings readSettings(const winshun::Json& s)
{
    Settings out;
    // Missing or wrong-typed fields keep the fallback; present values are
    // clamped into range. Numbers arriving as strings are still accepted.
    const auto numClamped = [&](const char* key, int lo, int hi, int fallback) -> int {
        const winshun::Json& v = s[key];
        if (v.isNumber())
            return clampInt(v.toInt(), lo, hi);
        if (v.isString())
            return clampInt(std::atoll(v.toString().c_str()), lo, hi);
        return fallback;
    };
    const auto boolOf = [&](const char* key, bool fallback) -> bool {
        const winshun::Json& v = s[key];
        if (v.isBool())
            return v.toBool();
        if (v.isNumber())
            return v.toInt() != 0;
        return fallback;
    };

    out.shakeEnabled = boolOf("shakeEnabled", out.shakeEnabled);

    ShakeConfig c;
    c.windowMs = numClamped("shakeWindowMs", 80, 2000, c.windowMs);
    c.minReversals = numClamped("shakeReversals", 2, static_cast<int>(kMaxReversals), c.minReversals);
    c.strokePx = numClamped("shakeStrokePx", 2, 600, c.strokePx);
    c.cooldownMs = numClamped("shakeCooldownMs", 0, 10000, c.cooldownMs);
    c.minSpeedPxS = numClamped("shakeMinSpeed", 0, 20000, c.minSpeedPxS);
    out.shake = sanitized(c);

    out.rippleMs = numClamped("rippleMs", 200, 3000, out.rippleMs);
    out.rippleSize = numClamped("rippleSize", 24, 256, out.rippleSize);
    out.followTimeoutMs = numClamped("followTimeoutMs", 0, 60000, out.followTimeoutMs);
    out.fadeOutMs = numClamped("fadeOutMs", 0, 3000, out.fadeOutMs);

    const winshun::Json& effect = s["rippleEffect"];
    if (effect.isString() && !effect.toString().empty())
        out.effect = parseEffect(effect.toString());

    const winshun::Json& custom = s["customStyle"];
    if (custom.isString())
        out.customStyleText = custom.toString();

    const winshun::Json& color = s["rippleColor"];
    if (color.isString() && !color.toString().empty()) {
        Rgb parsed;
        if (parseColor(color.toString(), parsed)) {
            out.accent = parsed;
            out.accentText = color.toString();
        }
    }
    return out;
}

} // namespace fm
