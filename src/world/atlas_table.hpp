// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

#include "world/layout.hpp"

namespace eggbert {

// Where one sprite sits in its sheet and how far it is shifted when drawn:
// the sheet rectangle is (srcX, srcY, width, height) and the sprite is drawn
// at the requested position plus (offsetX, offsetY).
struct SpriteRect {
    std::int16_t srcX = 0;
    std::int16_t srcY = 0;
    std::int16_t offsetX = 0;
    std::int16_t offsetY = 0;
    std::int16_t width = 0;
    std::int16_t height = 0;

    [[nodiscard]] constexpr bool empty() const { return width <= 0 || height <= 0; }
    friend constexpr bool operator==(const SpriteRect &, const SpriteRect &) = default;
};

// What a sprite table must look like to be accepted.
struct AtlasSearch {
    // Number of sprites the table declares (its first 16-bit value).
    std::size_t count = 0;
    // Size of the decoded sheet the table must fit.
    int sheetWidth = 0;
    int sheetHeight = 0;
    // Largest allowed offset + size on each axis: the sprite's logical cell.
    int maxExtentX = 0;
    int maxExtentY = 0;
    // Byte ranges that are known to hold something else (other tables that
    // were already located): a candidate overlapping one is not considered.
    std::span<const ByteRange> excluded{};
};

enum class AtlasSearchError {
    notFound,
    // More than one different table matched; none is trusted.
    ambiguous,
};

struct AtlasTable {
    // Offset of the count value in the searched bytes.
    std::size_t fileOffset = 0;
    std::vector<SpriteRect> sprites;
    // The last record's height did not fit and was reduced to what fits
    // (one known table in the first game stores no height for its last
    // sprite).
    bool lastHeightAdjusted = false;

    // The bytes the table occupies in the searched input.
    [[nodiscard]] ByteRange byteRange() const {
        return {fileOffset, 2 + sprites.size() * 12 - (lastHeightAdjusted ? 2 : 0)};
    }
};

// Looks for `count` followed by `count` records of six signed 16-bit
// little-endian values {srcX, srcY, offsetX, offsetY, width, height} in
// arbitrary bytes (a game executable the user selected). A candidate is
// accepted only if every rectangle lies inside the sheet and inside the
// logical cell, at least half of the sprites are non-empty and they are not
// all the same. Sprite tables of similar sheets can contain each other's
// look-alikes (a slice of a large table that happens to start with the
// wanted count), so a caller that already located other tables passes their
// ranges in `excluded`. Reads only; never executes or modifies anything.
[[nodiscard]] std::expected<AtlasTable, AtlasSearchError>
findAtlasTable(std::span<const std::byte> bytes, const AtlasSearch &search);

} // namespace eggbert
