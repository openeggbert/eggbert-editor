// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "world/layout.hpp"

namespace eggbert {

enum class EntryKind { tile, object, border };

// One thing a game's worlds can contain, described from the catalogs the
// editor already holds. Nothing here is new knowledge: every entry repeats
// the citation of the catalog entry it was built from.
struct EncyclopediaEntry {
    EntryKind kind = EntryKind::tile;
    std::string title;
    // Name of the game editor's group the entry belongs to; for a border
    // family "Adaptive borders".
    std::string group;
    // What is stored for it, in words and numbers.
    std::string facts;
    std::string provenance;

    // How the editor selects it. Tiles: layer and the numbers the entry
    // writes. Objects: the preset id. Borders: the family id.
    SectionKind layer = SectionKind::foreground;
    std::vector<std::int16_t> values;
    std::string id;
    // Tiles the games' editors reshape to fit their neighbours: the adaptive
    // family the first value belongs to (empty for plain tiles). Choosing
    // such an entry selects that brush, as the Palette does.
    std::string brush;
    // Objects only: what the thumbnail is drawn from.
    std::int16_t type = 0;
    std::int16_t channel = 0;
    std::int16_t icon = 0;
};

// Every tile entry and object of the game editor's groups, then the adaptive
// border families, in catalog order. Empty for GameProfile::unknown.
[[nodiscard]] std::vector<EncyclopediaEntry> encyclopedia(GameProfile profile);

// Indices of the entries matching every word of `query` (case-insensitive,
// in title, group, facts or provenance; a number also matches a tile value
// or object type exactly). An empty query matches everything.
[[nodiscard]] std::vector<std::size_t> searchEncyclopedia(
    const std::vector<EncyclopediaEntry> &entries, std::string_view query);

} // namespace eggbert
