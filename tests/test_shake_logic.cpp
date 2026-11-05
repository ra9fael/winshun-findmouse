// test_shake_logic.cpp — unit tests for the pure gesture state machine and
// the settings helpers. No OS interaction: the detector is fed synthetic
// positions with a synthetic monotonic clock.
//
// Build & run: cmake --build build --config Release && ctest --test-dir build
#include "../shake_logic.h"
#include "../settings.h"

#include <cmath>
#include <cstdio>
#include <string>

using namespace fm;

namespace {

int g_passed = 0;
int g_failed = 0;

void check(bool ok, const std::string& label)
{
    if (ok) {
        ++g_passed;
        std::printf("  PASS  %s\n", label.c_str());
    } else {
        ++g_failed;
        std::printf("  FAIL  %s\n", label.c_str());
    }
}

// Feed alternating stroke positions: x = center ± strokeAmp every `stepMs`.
// The first feed becomes the anchor and the second fixes a direction, so
// `strokes` feeds produce `strokes - 2` reversals when starting from rest.
long long feedShake(ShakeDetector& d, long long t0, int stepMs, int strokeAmp, int strokes)
{
    long long t = t0;
    for (int i = 0; i < strokes; ++i) {
        t += stepMs;
        const int x = (i % 2 == 0) ? strokeAmp : -strokeAmp;
        d.feed(t, x);
    }
    return t;
}

void test_configSanitized()
{
    ShakeConfig c = sanitized(ShakeConfig { 0, 1, 0, -5, -100 });
    check(c.windowMs == 80 && c.minReversals == 2 && c.strokePx == 2 && c.cooldownMs == 0
            && c.minSpeedPxS == 0,
        "sanitized: below-range values clamp to lower bounds");

    c = sanitized(ShakeConfig { 99999, 99, 9999, 99999, 999999 });
    check(c.windowMs == 2000 && c.minReversals == static_cast<int>(kMaxReversals)
            && c.strokePx == 200 && c.cooldownMs == 10000 && c.minSpeedPxS == 20000,
        "sanitized: above-range values clamp to upper bounds");

    c = sanitized(ShakeConfig { 300, 4, 8, 1500, 500 });
    check(c.windowMs == 300 && c.minReversals == 4 && c.strokePx == 8 && c.cooldownMs == 1500
            && c.minSpeedPxS == 500,
        "sanitized: sane values pass through");
}

void test_recognizesShake()
{
    ShakeDetector d(ShakeConfig { 300, 4, 8, 1500 });
    // 6 alternating strokes, 40 px each, 40 ms apart: reversals at
    // 120/160/200/240 — 4 reversals spanning 120 ms, inside the window.
    const long long end = feedShake(d, 0, 40, 40, 6);
    check(d.stats().triggers == 1, "shake: one trigger after 4 fast reversals");
    check(d.stats().reversals == 4, "shake: 4 reversals counted");
    check(d.stats().moves == 6, "shake: all moves counted");
    check(!d.armed(), "shake: disarmed during cooldown");
    check(end == 240, "shake: clock advanced as expected");
}

void test_oneTriggerPerGesture()
{
    ShakeDetector d(ShakeConfig { 300, 4, 8, 1500 });
    // Reversals at 120..440; the trigger fires at the 4th (t=240) and the
    // cooldown runs until 1740. Wiggling on changes nothing.
    feedShake(d, 0, 40, 40, 11);
    check(d.stats().triggers == 1, "cooldown: continuing to wiggle does not re-fire");

    // A shake that starts inside the cooldown is absorbed (its last stroke
    // re-arms the detector).
    feedShake(d, 1640, 40, 40, 4);
    check(d.stats().triggers == 1, "cooldown: shake ending just before cooldown end is ignored");

    // A fresh shake after the cooldown fires again.
    feedShake(d, 1800, 40, 40, 6);
    check(d.stats().triggers == 2, "cooldown: a fresh shake after cooldown fires again");
}

void test_slowMovementNeverFires()
{
    ShakeDetector d(ShakeConfig { 300, 4, 8, 1500, 500 });
    // Positions alternate ±25 px every 120 ms: each stroke's peak speed is
    // 50 px / 120 ms ≈ 416 px/s < the 500 px/s gate, so every reversal is
    // suppressed even though the geometry is a perfect wiggle.
    long long t = 0;
    for (int i = 0; i < 7; ++i) {
        t += 120;
        d.feed(t, (i % 2 == 0) ? 25 : -25);
    }
    check(d.stats().triggers == 0, "speed gate: a slow wiggle never fires");
    check(d.stats().reversals == 0, "speed gate: slow reversals are not counted");
    check(d.stats().slowStrokes >= 4, "speed gate: suppressed strokes are counted");
}

void test_speedGateCanBeDisabled()
{
    // minSpeed 0: the same 416 px/s wiggle is a legitimate gesture — this
    // also re-verifies the window-span logic the speed gate hides above.
    ShakeDetector d(ShakeConfig { 500, 4, 8, 0, 0 });
    long long t = 0;
    for (int i = 0; i < 7; ++i) {
        t += 120;
        d.feed(t, (i % 2 == 0) ? 25 : -25);
    }
    check(d.stats().triggers == 1, "speed gate off: slow wiggle fires again");
    check(d.stats().reversals >= 4, "speed gate off: reversals counted");
}

void test_smallJitterNeverFires()
{
    ShakeDetector d(ShakeConfig { 300, 4, 8, 1500 });
    // The two positions are 6 px apart (< strokePx = 8), so no direction is
    // ever fixed and no reversal can exist, however long it wobbles.
    long long t = 0;
    for (int i = 0; i < 60; ++i) {
        t += 5;
        d.feed(t, (i % 2 == 0) ? 3 : -3);
    }
    check(d.stats().triggers == 0, "jitter: sub-stroke retreats never fire");
    check(d.stats().reversals == 0, "jitter: hysteresis swallows the retreats");
}

void test_driftAndDirectionChanges()
{
    ShakeDetector d(ShakeConfig { 300, 4, 8, 0 });
    // Slow one-direction drift, then a real shake: the drift alone must not
    // set up a reversal.
    long long t = 0;
    for (int i = 0; i < 20; ++i) {
        t += 10;
        d.feed(t, i); // +1 px per step, all in one direction
    }
    check(d.stats().reversals == 0, "drift: monotonic movement has no reversal");
    const long long end = feedShake(d, t, 40, 200, 5); // now around x ≈ 19 ± 200
    check(d.stats().triggers == 1, "drift: a real shake after drift still fires");
    check(end == t + 200, "drift: clock handled across phases");
}

void test_reversalSpanUsesNewestEntries()
{
    ShakeDetector d(ShakeConfig { 300, 4, 8, 0 });
    // Two old reversals (span too wide), then 4 fresh ones: the detector must
    // judge by the newest minReversals entries, not by all history.
    long long t = 0;
    t += 10;
    d.feed(t, 40);
    t += 10;
    d.feed(t, -40); // reversal 1 at t=20
    t += 10;
    d.feed(t, 40); // reversal 2 at t=30
    t += 400; // let the window fully elapse
    d.feed(t, 0);
    const long long end = feedShake(d, t, 40, 40, 5);
    check(d.stats().triggers == 1, "span: old reversals do not poison the newest window");
    (void)end;
}

void test_configSwapMidStream()
{
    ShakeDetector d(ShakeConfig { 300, 4, 8, 1500 });
    feedShake(d, 0, 40, 40, 6); // trigger #1 at 240, cooldown until 1740
    check(d.stats().triggers == 1, "swap: baseline trigger");
    d.setConfig(ShakeConfig { 300, 4, 8, 0 }); // no cooldown; also resets motion
    check(d.armed(), "swap: config change re-arms the detector");
    feedShake(d, 2000, 40, 40, 6); // a full fresh shake
    check(d.stats().triggers == 2, "swap: next shake fires immediately with cooldown 0");
}

void test_parseColor()
{
    Rgb c;
    check(parseColor("#2AA8FF", c) && c.r == 0x2A && c.g == 0xA8 && c.b == 0xFF, "color: #RRGGBB");
    check(parseColor("2aa8ff", c) && c.r == 0x2A && c.g == 0xA8 && c.b == 0xFF, "color: rrggbb without #");
    check(parseColor("#2a8", c) && c.r == 0x22 && c.g == 0xAA && c.b == 0x88, "color: #RGB short form");
    check(parseColor(" #FF8000 ", c) && c.r == 0xFF && c.g == 0x80 && c.b == 0x00, "color: spaces tolerated");
    check(!parseColor("#12345", c), "color: 5 digits rejected");
    check(!parseColor("#12345Z", c), "color: non-hex rejected");
    check(!parseColor("", c), "color: empty rejected");
    check(!parseColor("blue", c), "color: names rejected");
}

void test_readSettings()
{
    // All defaults on an empty object.
    const Settings def = readSettings(winshun::Json(winshun::Json::Object {}));
    check(def.shakeEnabled && def.shake.windowMs == 300 && def.shake.minReversals == 4
            && def.shake.strokePx == 8 && def.shake.cooldownMs == 1500 && def.shake.minSpeedPxS == 500,
        "settings: missing keys give the defaults");
    check(def.rippleMs == 700 && def.rippleSize == 64 && def.accent.r == 0x2A,
        "settings: ripple defaults hold");

    // Out-of-range values clamp instead of breaking the detector.
    winshun::Json::Object bad {
        {"shakeWindowMs", 99999.0},
        {"shakeReversals", 1.0},
        {"shakeStrokePx", -3.0},
        {"shakeMinSpeed", 99999.0},
        {"rippleMs", 99999.0},
        {"rippleSize", 9999.0},
        {"rippleColor", "#010203"},
    };
    const Settings clamped = readSettings(winshun::Json(std::move(bad)));
    check(clamped.shake.windowMs == 2000 && clamped.shake.minReversals == 2
            && clamped.shake.strokePx == 2 && clamped.shake.minSpeedPxS == 20000
            && clamped.rippleMs == 3000 && clamped.rippleSize == 256,
        "settings: out-of-range values clamp");
    check(clamped.accent.r == 0x01 && clamped.accent.g == 0x02 && clamped.accent.b == 0x03,
        "settings: valid color parsed");

    // An invalid color keeps the default accent.
    winshun::Json::Object ugly { {"rippleColor", "not-a-color"} };
    const Settings fallback = readSettings(winshun::Json(std::move(ugly)));
    check(fallback.accentText == "#2AA8FF", "settings: bad color falls back to default accent");

    // Numbers arriving as strings are accepted.
    winshun::Json::Object asStrings { {"shakeReversals", "6"}, {"rippleMs", "500"} };
    const Settings str = readSettings(winshun::Json(std::move(asStrings)));
    check(str.shake.minReversals == 6 && str.rippleMs == 500, "settings: stringified numbers accepted");

    check(shakeConfigOf(def).minReversals == 4, "settings: shakeConfigOf maps through");

    // Effect parsing: all names, case-insensitivity, and the garbage fallback.
    check(def.effect == Effect::Ring, "settings: missing effect defaults to ring");
    winshun::Json::Object eff { {"rippleEffect", "radar"} };
    check(readSettings(winshun::Json(std::move(eff))).effect == Effect::Radar,
        "settings: radar parsed");
    winshun::Json::Object eff2 { {"rippleEffect", "WAVES"} };
    check(readSettings(winshun::Json(std::move(eff2))).effect == Effect::Waves,
        "settings: effect names are case-insensitive");
    winshun::Json::Object eff3 { {"rippleEffect", "sparkles"} };
    const Settings effFall = readSettings(winshun::Json(std::move(eff3)));
    check(effFall.effect == Effect::Ring, "settings: unknown effect falls back to ring");
    check(std::string(effectName(effFall.effect)) == "ring", "effect: ring's name is canonical");
    check(std::string(effectName(Effect::Halo)) == "halo", "effect: halo's name is canonical");
    winshun::Json::Object eff4 { {"rippleEffect", "custom"} };
    check(readSettings(winshun::Json(std::move(eff4))).effect == Effect::Custom,
        "settings: custom parsed");
    winshun::Json::Object eff5 { {"followTimeoutMs", 99999.0} };
    check(readSettings(winshun::Json(std::move(eff5))).followTimeoutMs == 60000,
        "settings: follow timeout clamps to 60000");
    check(readSettings(winshun::Json(winshun::Json::Object {})).fadeOutMs == 400,
        "settings: fade-out defaults to 400 ms");
    winshun::Json::Object fade1 { {"fadeOutMs", 99999.0} };
    check(readSettings(winshun::Json(std::move(fade1))).fadeOutMs == 3000,
        "settings: fade-out clamps to 3000");
    winshun::Json::Object fade2 { {"fadeOutMs", 0.0} };
    check(readSettings(winshun::Json(std::move(fade2))).fadeOutMs == 0,
        "settings: fade-out 0 (instant) accepted");
    winshun::Json::Object eff6 { {"customStyle", "{\"layers\":[{\"shape\":\"disc\"}]}"} };
    check(readSettings(winshun::Json(std::move(eff6))).customStyleText.find("disc") != std::string::npos,
        "settings: customStyle text carried through");
}

void test_styleDoc()
{
    // Minimal valid doc: everything falls back to sane defaults.
    const auto jmin = winshun::Json::parse("{\"layers\":[{\"shape\":\"ring\"}]}");
    StyleDoc doc;
    check(jmin.has_value() && parseStyleDoc(*jmin, doc) && doc.layers.size() == 1 && doc.loopMs == 0,
        "style: minimal doc accepted with defaults");
    const StyleLayer& l0 = doc.layers[0];
    check(l0.shape == LayerShape::Ring && !l0.animated && l0.radius == 1.0 && l0.width == 3.5
            && l0.offset == 0 && l0.alpha.mode == AlphaMode::Constant && l0.alpha.value == 1.0
            && l0.useAccent && !l0.fill,
        "style: layer defaults hold");

    // Full doc: every field lands where it should.
    const char* full = "{"
        "\"loopMs\": 900,"
        "\"layers\": ["
        "  {\"shape\":\"ring\",\"from\":0.2,\"to\":1.0,\"ease\":\"outCubic\","
        "   \"width\":7,\"offset\":6.5,\"phase\":0.25,"
        "   \"alpha\":{\"mode\":\"holdFade\",\"value\":0.8},\"color\":\"#FF8800\"},"
        "  {\"shape\":\"disc\",\"radius\":0.05,\"alpha\":0.5,\"color\":\"accent\"},"
        "  {\"shape\":\"sweep\",\"speed\":-2,\"trailDeg\":90,\"fill\":true,"
        "   \"alpha\":{\"mode\":\"sin\"}}"
        "]}";
    const auto jfull = winshun::Json::parse(full);
    StyleDoc dfull;
    check(jfull.has_value() && parseStyleDoc(*jfull, dfull) && dfull.layers.size() == 3,
        "style: full doc accepted");
    check(dfull.loopMs == 900, "style: loopMs parsed");
    const StyleLayer& r = dfull.layers[0];
    check(r.animated && r.from == 0.2 && r.to == 1.0 && r.ease == Ease::OutCubic
            && r.width == 7 && r.offset == 6.5 && r.phase == 0.25
            && r.alpha.mode == AlphaMode::HoldFade && r.alpha.value == 0.8
            && !r.useAccent && r.color.r == 0xFF && r.color.g == 0x88 && r.color.b == 0x00,
        "style: ring layer fields");
    const StyleLayer& dsk = dfull.layers[1];
    check(dsk.shape == LayerShape::Disc && dsk.alpha.mode == AlphaMode::Constant
            && dsk.alpha.value == 0.5 && dsk.useAccent,
        "style: numeric alpha means constant accent");
    const StyleLayer& sw = dfull.layers[2];
    check(sw.shape == LayerShape::Sweep && sw.speed == -2 && sw.trailDeg == 90 && sw.fill
            && sw.alpha.mode == AlphaMode::Sin,
        "style: sweep fields");

    // Rejections: typos must fail the whole document, not half-render.
    StyleDoc tmp;
    const auto badEmpty = winshun::Json::parse("{\"layers\":[]}");
    check(!parseStyleDoc(*badEmpty, tmp), "style: empty layers rejected");
    const auto badShape = winshun::Json::parse("{\"layers\":[{\"shape\":\"sparkle\"}]}");
    check(!parseStyleDoc(*badShape, tmp), "style: unknown shape rejected");
    const auto badColor = winshun::Json::parse("{\"layers\":[{\"shape\":\"ring\",\"color\":\"nope\"}]}");
    check(!parseStyleDoc(*badColor, tmp), "style: bad color rejected");
    const auto badAlpha = winshun::Json::parse("{\"layers\":[{\"shape\":\"ring\",\"alpha\":{\"mode\":\"whoosh\"}}]}");
    check(!parseStyleDoc(*badAlpha, tmp), "style: unknown alpha mode rejected");
    const auto badRoot = winshun::Json::parse("{\"loopMs\": 900}");
    check(!parseStyleDoc(*badRoot, tmp), "style: missing layers rejected");

    // Layer-count cap.
    std::string many = "{\"layers\":[";
    for (int i = 0; i < 13; ++i)
        many += (i ? "," : "") + std::string("{\"shape\":\"ring\"}");
    many += "]}";
    const auto jmany = winshun::Json::parse(many);
    check(!parseStyleDoc(*jmany, tmp), "style: more than 12 layers rejected");

    // Clamps.
    const auto jclamp = winshun::Json::parse(
        "{\"loopMs\":9,\"layers\":[{\"shape\":\"sweep\",\"width\":999,\"radius\":-5,"
        "\"phase\":2,\"speed\":99,\"trailDeg\":9999,\"offset\":-9}]}");
    StyleDoc dclamp;
    check(parseStyleDoc(*jclamp, dclamp), "style: clamp doc accepted");
    const StyleLayer& cl = dclamp.layers[0];
    check(dclamp.loopMs == 200 && cl.width == 40 && cl.radius == 0 && cl.phase == 1
            && cl.speed == 4 && cl.trailDeg == 360 && cl.offset == 0,
        "style: out-of-range values clamp");

    // Easing and alpha math.
    const double eps = 1e-9;
    check(std::abs(evalEase(Ease::OutCubic, 0.5) - 0.875) < eps, "ease: outCubic(0.5) = 0.875");
    check(std::abs(evalEase(Ease::InQuad, 0.5) - 0.25) < eps, "ease: inQuad(0.5) = 0.25");
    check(std::abs(evalEase(Ease::OutQuad, 0.5) - 0.75) < eps, "ease: outQuad(0.5) = 0.75");
    check(std::abs(evalEase(Ease::Sin, 0.25) - 1.0) < eps, "ease: sin(0.25) peaks at 1");
    check(std::abs(evalEase(Ease::Linear, 0.25) - 0.25) < eps, "ease: linear passes through");
    AlphaSpec a;
    a.value = 0.8;
    a.mode = AlphaMode::LinearFade;
    check(std::abs(evalAlpha(a, 0.5) - 0.4) < eps, "alpha: linearFade halves at p=0.5");
    a.mode = AlphaMode::HoldFade;
    check(std::abs(evalAlpha(a, 0.3) - 0.8) < eps, "alpha: holdFade holds before 0.6");
    check(std::abs(evalAlpha(a, 0.8) - 0.4) < eps, "alpha: holdFade fades to 0 at p=1");
    a.mode = AlphaMode::Sin;
    check(std::abs(evalAlpha(a, 0.25) - 0.8) < eps, "alpha: sin peaks at p=0.25");
}

} // namespace

int main()
{
    std::printf("ra9fael.findmouse logic tests\n");
    test_configSanitized();
    test_recognizesShake();
    test_oneTriggerPerGesture();
    test_slowMovementNeverFires();
    test_speedGateCanBeDisabled();
    test_smallJitterNeverFires();
    test_driftAndDirectionChanges();
    test_reversalSpanUsesNewestEntries();
    test_configSwapMidStream();
    test_parseColor();
    test_readSettings();
    test_styleDoc();
    std::printf("%d passed, %d failed\n", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}
