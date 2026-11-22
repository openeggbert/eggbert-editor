// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "world/world_view.hpp"

namespace eggbert {

// Inclusive cell rectangle.
struct CellBounds {
    std::size_t minX = 0;
    std::size_t minY = 0;
    std::size_t maxX = 0;
    std::size_t maxY = 0;

    friend constexpr bool operator==(const CellBounds &, const CellBounds &) = default;
};

struct LayerStats {
    // False when the layer is absent (revision 1.0 bigDecor) or incomplete.
    bool present = false;
    // Cells whose stored value is not -1, and cells storing exactly -1.
    std::size_t occupied = 0;
    std::size_t empty = 0;
    // Every stored value except -1 with its number of cells.
    std::map<std::int16_t, std::size_t> valueCounts;
    // Bounding box of occupied cells.
    std::optional<CellBounds> bounds;
};

struct ObjectStats {
    bool present = false;
    std::size_t stored = 0;
    // Records with a non-zero type, and records with type 0.
    std::size_t active = 0;
    std::size_t free = 0;
    // Free records in which some other byte is non-zero.
    std::size_t freeWithOpaqueBytes = 0;
    // Active records whose start and end positions differ.
    std::size_t withPath = 0;
    // Counts over active records only.
    std::map<std::int16_t, std::size_t> byType;
    std::map<std::int16_t, std::size_t> byChannel;
    std::map<std::int16_t, std::size_t> byIcon;
};

// Profile-independent facts about one world. Nothing here interprets a value
// for a particular game.
struct WorldStats {
    std::size_t fileBytes = 0;
    std::optional<std::int16_t> headerMajor;
    std::optional<std::int16_t> headerMinor;
    FormatRevision revision = FormatRevision::unknown;
    std::size_t surplusBytes = 0;
    std::size_t missingBytes = 0;

    bool headerPresent = false;
    std::array<Point, 4> startPositions{};
    std::array<std::int32_t, 4> startDirections{};

    LayerStats foreground;
    LayerStats bigDecor;
    ObjectStats objects;
    // Bounding box of occupied cells in either layer (bigDecor by anchor).
    std::optional<CellBounds> usedBounds;
};

[[nodiscard]] WorldStats computeWorldStats(const WorldView &world);

// Cells of a layer storing exactly `value`, ordered by x then y. Empty when
// the layer is absent.
[[nodiscard]] std::vector<CellPos> findCells(const WorldView &world, SectionKind layer,
                                               std::int16_t value);

enum class ObjectKey { type, channel, icon };

// Active slots whose type, channel or icon equals `value`, ascending.
[[nodiscard]] std::vector<std::size_t> findActiveSlots(const WorldView &world, ObjectKey key,
                                                       std::int16_t value);

} // namespace eggbert
