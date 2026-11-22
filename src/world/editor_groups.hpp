// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "world/layout.hpp"

namespace eggbert {

// How each game's own level editor groups what it places: one group per
// editor button, with the button's menu entries in order. The groups are a
// way to find things; they do not limit what can be stored. Every list is
// recorded evidence (docs/research/evidence.md, "The games' editor menus").

// One menu entry that writes tiles.
struct TileEntry {
    // What the entry places, e.g. "Tree".
    std::string_view name;
    SectionKind layer = SectionKind::foreground;
    // The tile numbers the entry writes; repeated clicks in the games'
    // editors cycle through them in this order.
    std::span<const std::int16_t> values;
    // For entries whose tile the games' editors then reshape to fit its
    // neighbours: the adaptive family (see adaptive.hpp) each value belongs
    // to, same length as `values`; empty for plain tiles.
    std::span<const std::string_view> brushes;
    std::string_view provenance;
};

struct EditorGroup {
    // Stable key, unique within a profile.
    std::string_view id;
    std::string_view name;
    std::span<const TileEntry> tiles;
    // Ids of ObjectPreset entries of the same profile, in menu order.
    std::span<const std::string_view> presets;
    std::string_view provenance;
};

// Empty for GameProfile::unknown.
[[nodiscard]] std::span<const EditorGroup> editorGroups(GameProfile profile);

} // namespace eggbert
