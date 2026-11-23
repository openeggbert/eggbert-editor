// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "world/tile_tools.hpp"
#include "world/world_document.hpp"

namespace eggbert {

// A reusable piece of a world: a rectangle of both tile layers and,
// optionally, the objects that start in it. Empty cells (-1) are holes: a
// stamp overlays what is there instead of erasing it.
struct Stamp {
    std::string name;
    GameProfile game = GameProfile::unknown;
    std::size_t width = 0;
    std::size_t height = 0;
    // Row-major, width * height values each.
    std::vector<std::int16_t> foreground;
    std::vector<std::int16_t> bigDecor;
    // Records with their three positions relative to the stamp's top-left
    // corner, in world pixels.
    std::vector<ObjectRecord> objects;

    friend bool operator==(const Stamp &, const Stamp &) = default;
};

inline constexpr std::size_t stampMostObjects = 200;

// The rectangle's tiles and, with `withObjects`, every active object whose
// start lies in it (runtime fields are stored as they are).
[[nodiscard]] Stamp makeStamp(const WorldView &world, const CellRect &rect, std::string name,
                              GameProfile game, bool withObjects);

struct StampPlacement {
    std::size_t tilesChanged = 0;
    std::size_t objectsCreated = 0;
    // Stamp cells that fell outside the map.
    std::size_t tilesClipped = 0;
};

// Places the stamp with its top-left at `at`. Tiles outside the map are
// clipped. Objects are all placed or none: the placement is refused when the
// pool has too few free slots or an object would lie outside the world, and
// then the builder is left untouched.
[[nodiscard]] std::expected<StampPlacement, std::string>
placeStamp(EditBuilder &builder, const Stamp &stamp, CellPos at);

// The editor's own text format (see docs/user-guide.md). Reading validates
// everything and names the line of an error.
[[nodiscard]] std::string writeStamp(const Stamp &stamp);
[[nodiscard]] std::expected<Stamp, std::string> readStamp(std::string_view text);

// Moving objects as a group. Every operation shifts an object's start, end
// and current position by the same amount, so paths keep their shape.
enum class AlignMode { left, right, top, bottom, distributeHorizontally, distributeVertically };

// Aligns or evenly distributes (by start position) the active objects in
// `slots`; returns how many records changed. Fewer than two objects (three
// to distribute) change nothing.
std::size_t alignObjects(EditBuilder &builder, std::span<const std::size_t> slots, AlignMode mode);

} // namespace eggbert
