// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "world/layout.hpp"

namespace eggbert {

// How a stored object type is known for a game profile.
enum class ObjectTypeStatus {
    // Not referenced by the profile's evidence at all.
    unknown,
    // Created by the game while playing, never by its level editor.
    runtimeOnly,
    // Placed by the game's own level editor; a verified preset exists.
    placeable,
};

// What a game's level editor writes when it places one kind of object on a
// cell. Every number is recorded evidence (docs/research/evidence.md); the
// remaining record fields are zero, as the engines zero a slot before use.
struct ObjectPreset {
    // Stable key, unique within a profile.
    std::string_view id;
    // Editor's descriptive name.
    std::string_view name;
    // Identifier used by the reconstructed source, for traceability.
    std::string_view sourceName;
    std::int16_t type = 0;
    // For movers these are ticks per cell of travel; the engines' editors
    // multiply them by the path length in cells when the end is set.
    std::int16_t stepAdvance = 1;
    std::int16_t stepRecede = 1;
    std::int16_t timeStopStart = 0;
    std::int16_t timeStopEnd = 0;
    // posStart relative to the cell's top-left corner, in world pixels.
    std::int16_t offsetX = 0;
    std::int16_t offsetY = 0;
    std::int16_t channel = 0;
    std::int16_t icon = 0;
    // True for objects that travel between posStart and posEnd.
    bool hasPath = false;
    // True when the stored icon is the user's choice of a tile number
    // (lifts and crates); otherwise the game redraws the icon itself.
    bool iconIsChoice = false;
    std::string_view provenance;
};

// Verified presets of a profile; empty for GameProfile::unknown.
[[nodiscard]] std::span<const ObjectPreset> objectPresets(GameProfile profile);
[[nodiscard]] const ObjectPreset *findObjectPreset(GameProfile profile, std::string_view id);
// First preset of the profile with this stored type, or nullptr.
[[nodiscard]] const ObjectPreset *presetForType(GameProfile profile, std::int16_t type);

struct ObjectTypeInfo {
    std::int16_t type = 0;
    std::string_view name;
    std::string_view sourceName;
    ObjectTypeStatus status = ObjectTypeStatus::unknown;
};

// Types spawned during play, kept as reference entries in the encyclopedia.
[[nodiscard]] std::span<const ObjectTypeInfo> runtimeTypes(GameProfile profile);

// Classification of a stored type under a profile. Unknown types and the
// unknown profile yield status `unknown` with an empty name.
[[nodiscard]] ObjectTypeInfo objectTypeInfo(GameProfile profile, std::int16_t type);

// The step value both editors store for a freshly placed object.
inline constexpr std::int16_t stepStopStart = 1;

} // namespace eggbert
