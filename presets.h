// presets.h — the effect registry: one table as the single source of truth.
//
// Every beacon effect the plugin offers is a row here: built-in renderer
// styles (ring/waves/halo/radar), DSL presets (rows whose `doc` is non-null
// and render through the generic StyleDoc renderer), and three shortlisted
// renderer-extension candidates (nullptr `doc`, marked in `note`). Custom
// is a user-authored document, not a registry preset. Other concepts in the
// workspace design preview are not selectable plugin effects.
#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string>

#include "settings.h"

namespace fm {

// One registry row. Built-ins own a dedicated render pass; DSL presets carry
// a JSON document that parseStyleDoc accepts, so selecting one needs zero
// renderer code and the same clamps/rejections as hand-written styles.
struct EffectInfo {
    enum class Kind { BuiltIn, DslPreset, Planned };
    const char* id; // canonical effect name, lowercase ASCII
    const char* nameZh;
    const char* nameEn;
    const char* note; // Planned rows: what the renderer still needs; "" otherwise
    Kind kind;
    const char* doc; // JSON for DslPreset rows; nullptr otherwise
};

inline const std::array<EffectInfo, 10>& effectRegistry()
{
    static const std::array<EffectInfo, 10> kRegistry = { {
        // Built-ins: each owns its dedicated render pass.
        { "ring", "涟漪", "Ring", "", EffectInfo::Kind::BuiltIn, nullptr },
        { "waves", "水波", "Waves", "", EffectInfo::Kind::BuiltIn, nullptr },
        { "halo", "光晕", "Halo", "", EffectInfo::Kind::BuiltIn, nullptr },
        { "radar", "雷达", "Radar", "", EffectInfo::Kind::BuiltIn, nullptr },
        // DSL presets: rows straight from the design preview (the JSON is the
        // same the preview page hands out as copyable customStyle documents).
        { "ping", "声呐 Ping", "Sonar Ping", "", EffectInfo::Kind::DslPreset,
            "{\"loopMs\":1100,\"layers\":["
            "{\"shape\":\"ring\",\"from\":0.12,\"to\":1.0,\"ease\":\"outCubic\",\"width\":2.5,"
            "\"alpha\":{\"mode\":\"linearFade\",\"value\":0.95},\"color\":\"accent\"},"
            "{\"shape\":\"ring\",\"from\":0.12,\"to\":0.55,\"phase\":0.35,\"ease\":\"outCubic\",\"width\":1.5,"
            "\"alpha\":{\"mode\":\"linearFade\",\"value\":0.4},\"color\":\"accent\"},"
            "{\"shape\":\"disc\",\"radius\":0.05,\"alpha\":1.0,\"color\":\"accent\"}]}" },
        { "heart", "心跳 Lub-dub", "Heartbeat", "", EffectInfo::Kind::DslPreset,
            "{\"loopMs\":1400,\"layers\":["
            "{\"shape\":\"ring\",\"from\":0.15,\"to\":0.85,\"ease\":\"outCubic\",\"width\":3,"
            "\"alpha\":{\"mode\":\"holdFade\",\"value\":1.0},\"color\":\"accent\"},"
            "{\"shape\":\"ring\",\"from\":0.15,\"to\":0.85,\"ease\":\"outCubic\",\"width\":3,\"phase\":0.16,"
            "\"alpha\":{\"mode\":\"holdFade\",\"value\":0.75},\"color\":\"accent\"},"
            "{\"shape\":\"disc\",\"radius\":0.05,\"alpha\":1.0,\"color\":\"accent\"}]}" },
        { "orbit", "轨道双针", "Orbit Needles", "", EffectInfo::Kind::DslPreset,
            "{\"loopMs\":1600,\"layers\":["
            "{\"shape\":\"sweep\",\"speed\":1.0,\"trailDeg\":40,"
            "\"alpha\":{\"mode\":\"constant\",\"value\":0.8},\"color\":\"accent\"},"
            "{\"shape\":\"sweep\",\"speed\":-0.6,\"trailDeg\":30,"
            "\"alpha\":{\"mode\":\"constant\",\"value\":0.5},\"color\":\"accent\"},"
            "{\"shape\":\"disc\",\"radius\":0.04,\"alpha\":1.0,\"color\":\"accent\"}]}" },
        // Renderer-extension candidates from the preview, kept visible so the
        // gap between "selectable" and "planned" is explicit. Selecting one
        // is rejected by parseEffect and degrades to Ring.
        { "comet", "彗尾", "Comet", "needs pointer-history rendering", EffectInfo::Kind::Planned, nullptr },
        { "spotlight", "聚光灯", "Spotlight", "needs a full-screen dim layer", EffectInfo::Kind::Planned, nullptr },
        { "pin", "图钉落点", "Pin Drop", "needs polygon/path shapes", EffectInfo::Kind::Planned, nullptr },
    } };
    return kRegistry;
}

// Case-insensitive lookup by id (the settings values are lowercase already,
// but be forgiving like parseEffect is).
inline const EffectInfo* findEffectInfo(std::string id)
{
    for (char& c : id)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    for (const EffectInfo& e : effectRegistry()) {
        if (id == e.id)
            return &e;
    }
    return nullptr;
}

// Parse a preset row's JSON into a StyleDoc. The registry docs are authored
// data, but they go through the exact same parseStyleDoc validation as
// hand-written styles, so a typo in a preset degrades the same way (the
// caller falls back). This runs only when settings change, never per frame;
// parsing three small layers avoids a mutable cache keyed by row addresses.
inline std::shared_ptr<const StyleDoc> presetDoc(const EffectInfo& e)
{
    if (e.kind != EffectInfo::Kind::DslPreset || !e.doc)
        return nullptr;
    const auto json = winshun::Json::parse(e.doc);
    StyleDoc doc;
    std::shared_ptr<const StyleDoc> parsed;
    if (json && json->isObject() && parseStyleDoc(*json, doc))
        parsed = std::make_shared<const StyleDoc>(std::move(doc));
    return parsed;
}

} // namespace fm
