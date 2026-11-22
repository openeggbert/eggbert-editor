// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <bitset>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "world/world_document.hpp"

namespace eggbert {

// Inclusive rectangle of cells inside the map.
struct CellRect {
    CellPos min;
    CellPos max;

    [[nodiscard]] constexpr std::size_t width() const { return max.x - min.x + 1; }
    [[nodiscard]] constexpr std::size_t height() const { return max.y - min.y + 1; }
    [[nodiscard]] constexpr std::size_t cellCount() const { return width() * height(); }
    [[nodiscard]] constexpr bool contains(CellPos cell) const {
        return cell.x >= min.x && cell.x <= max.x && cell.y >= min.y && cell.y <= max.y;
    }
    friend constexpr bool operator==(const CellRect &, const CellRect &) = default;
};

// Rectangle spanned by two corners in any order.
[[nodiscard]] CellRect rectFromCorners(CellPos first, CellPos second);
// Clips signed, possibly out-of-map bounds to the map; nullopt when the
// rectangle lies entirely outside.
[[nodiscard]] std::optional<CellRect> clipToMap(long long x0, long long y0, long long x1,
                                                long long y1);
[[nodiscard]] constexpr CellRect wholeMap() {
    return {{0, 0}, {layout::mapWidth - 1, layout::mapHeight - 1}};
}

// A set of map cells: the selection.
class CellMask {
public:
    [[nodiscard]] static CellMask fromRect(const CellRect &rect);

    [[nodiscard]] bool test(CellPos cell) const {
        return cell.x < layout::mapWidth && cell.y < layout::mapHeight &&
               bits_[cell.x * layout::mapHeight + cell.y];
    }
    void set(CellPos cell, bool value = true);
    void add(const CellRect &rect);
    void invert() { bits_.flip(); }
    void clear() { bits_.reset(); }
    [[nodiscard]] std::size_t count() const { return bits_.count(); }
    [[nodiscard]] bool empty() const { return bits_.none(); }
    [[nodiscard]] std::optional<CellRect> bounds() const;
    // Selected cells ordered by x then y.
    [[nodiscard]] std::vector<CellPos> cells() const;
    friend bool operator==(const CellMask &, const CellMask &) = default;

private:
    std::bitset<layout::mapWidth * layout::mapHeight> bits_;
};

// Cells of a straight line between two cells, endpoints included, in order.
[[nodiscard]] std::vector<CellPos> lineCells(CellPos from, CellPos to);
// Border cells of a rectangle, each once.
[[nodiscard]] std::vector<CellPos> outlineCells(const CellRect &rect);

// The tools below edit only `layer` through the builder and return how many
// cells now hold a different value. An absent layer yields 0. `limit`, when
// given, restricts every write to the mask.

std::size_t paintCells(EditBuilder &builder, SectionKind layer, std::span<const CellPos> cells,
                       std::int16_t value, const CellMask *limit = nullptr);
std::size_t fillRect(EditBuilder &builder, SectionKind layer, const CellRect &rect,
                     std::int16_t value, const CellMask *limit = nullptr);
std::size_t fillMask(EditBuilder &builder, SectionKind layer, const CellMask &mask,
                     std::int16_t value);
// 4-connected region of cells equal to the start cell's value, bounded by
// the map and by `limit`.
std::size_t floodFill(EditBuilder &builder, SectionKind layer, CellPos start, std::int16_t value,
                      const CellMask *limit = nullptr);
// The cells floodFill would visit, without editing.
[[nodiscard]] std::vector<CellPos> floodRegion(const WorldView &world, SectionKind layer,
                                               CellPos start, const CellMask *limit = nullptr);
std::size_t replaceValue(EditBuilder &builder, SectionKind layer, std::int16_t from,
                         std::int16_t to, const CellMask *limit = nullptr);

// A rectangular block of stored values; cells with `present == false` are
// holes that paste leaves alone.
struct TilePatch {
    std::size_t width = 0;
    std::size_t height = 0;
    // Row-major: index = y * width + x.
    std::vector<std::int16_t> values;
    std::vector<bool> present;
    SectionKind sourceLayer = SectionKind::foreground;

    [[nodiscard]] bool empty() const { return width == 0 || height == 0; }
    [[nodiscard]] std::size_t presentCount() const;
    friend bool operator==(const TilePatch &, const TilePatch &) = default;
};

// Copies the masked cells' bounding box; unmasked cells become holes.
[[nodiscard]] TilePatch copyPatch(const WorldView &world, SectionKind layer,
                                  const CellMask &mask);
// Pastes with the patch's top-left at (x, y), which may lie outside the map;
// cells falling outside are clipped. With skipEmpty, -1 values are not written.
std::size_t pastePatch(EditBuilder &builder, SectionKind layer, const TilePatch &patch,
                       long long x, long long y, bool skipEmpty = false);
// Cells a paste would cover inside the map (holes excluded).
[[nodiscard]] std::vector<CellPos> pasteFootprint(const TilePatch &patch, long long x,
                                                  long long y);
// Moves stored values only; no tile is remapped to a mirrored counterpart.
[[nodiscard]] TilePatch flippedHorizontally(const TilePatch &patch);
[[nodiscard]] TilePatch flippedVertically(const TilePatch &patch);

} // namespace eggbert
