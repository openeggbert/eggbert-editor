// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "world/layout.hpp"
#include "world/revision_convert.hpp"
#include "world/world_view.hpp"

namespace eggbert {

// Both games read the same file layout and share almost every stored number
// (docs/research/evidence.md, "The two games compared"). Converting a world
// for the other game therefore changes few bytes; what matters is knowing
// which stored values the other game does not have or treats differently.

enum class GameItem { foregroundTile, bigDecorTile, objectType, objectIcon, region };

enum class CarryKind {
    // The target game has no picture or code for the value.
    unknownToTarget,
    // Both games have it, but the target shows or treats it differently.
    differsInTarget,
};

struct GameConversionNote {
    GameItem item = GameItem::foregroundTile;
    CarryKind kind = CarryKind::unknownToTarget;
    // The stored value (tile number, object type, icon or region).
    int value = 0;
    // How many cells or object slots hold it.
    std::size_t count = 0;
    // First place it occurs: a cell for tiles, a slot for objects.
    std::optional<CellPos> firstCell;
    std::optional<std::size_t> firstSlot;
    std::string text;
    std::string provenance;
};

// What to do with values the target game does not know.
enum class UnknownPolicy {
    // Convert nothing while such values exist (the default question).
    refuse,
    // Keep them stored; the notes say what the target game does with them.
    keep,
    // Empty those cells, free those object slots, reset such a region to 2
    // (the region both games' editors start with).
    remove,
};

struct GameConversion {
    std::vector<std::byte> bytes;
    std::vector<GameConversionNote> notes;
    // Revision notes from convertRevision, when the revision changed.
    std::vector<std::string> revisionNotes;
    // Cells and slots changed by UnknownPolicy::remove.
    std::size_t removedCount = 0;
};

// Everything in the world the target game does not know or treats
// differently, one note per distinct value, unknown values first. Empty for
// equal or unknown games.
[[nodiscard]] std::vector<GameConversionNote> reviewGameConversion(const WorldView &world,
                                                                  GameProfile from,
                                                                  GameProfile to);

// A copy of the world for the other game, in the revision that game's stock
// worlds use. Nothing but the revision field and, with UnknownPolicy::remove,
// the listed values is changed. Refused (`wouldLoseData`) under
// UnknownPolicy::refuse when the target does not know something.
[[nodiscard]] std::expected<GameConversion, ConversionFailure>
convertGame(std::span<const std::byte> source, GameProfile from, GameProfile to,
            UnknownPolicy policy = UnknownPolicy::refuse);

} // namespace eggbert
