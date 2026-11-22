// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "world/world_document.hpp"

namespace eggbert {

// Start directions as both engines define them. Any other stored value is
// drawn like "right" by the games (they only test for left).
inline constexpr std::int32_t startDirectionLeft = 1;
inline constexpr std::int32_t startDirectionRight = 2;

// Longest title the games let a player type, in bytes. The field itself
// holds up to 99 bytes plus the terminator.
inline constexpr std::size_t typedTitleLimit = 40;
inline constexpr std::size_t storedTitleLimit = 99;

// Where both games' editors put a start placed on a cell: the cell's top
// left plus (2, 6) world pixels.
[[nodiscard]] constexpr Point startPositionForCell(CellPos cell) {
    return {static_cast<std::int32_t>(cell.x) * layout::cellPixels + 2,
            static_cast<std::int32_t>(cell.y) * layout::cellPixels + 6};
}

// Cell containing a world-pixel position; nullopt outside the map.
[[nodiscard]] std::optional<CellPos> cellOfPosition(Point position);

// Titles are single-byte text typed in a Windows ANSI window. The file does
// not record a code page; Windows-1252 is assumed for display and entry.
[[nodiscard]] std::u32string decodeWindows1252(std::span<const std::byte> bytes);
// nullopt when a character has no Windows-1252 byte or is a control
// character below U+0020.
[[nodiscard]] std::optional<std::vector<std::byte>> encodeWindows1252(std::u32string_view text);

struct StartInfo {
    Point position;
    std::int32_t direction = startDirectionRight;

    friend constexpr bool operator==(const StartInfo &, const StartInfo &) = default;
};

// The editable header fields as one value.
struct Metadata {
    // Title bytes before the terminator.
    std::vector<std::byte> name;
    std::int16_t music = 0;
    std::int16_t region = 0;
    // Each dimDecor axis as the engines read it: zero means the world is one
    // screen in that direction, anything else means it scrolls.
    bool scrollsHorizontally = false;
    bool scrollsVertically = false;
    Point posDecor;
    std::array<StartInfo, 4> starts{};

    friend bool operator==(const Metadata &, const Metadata &) = default;
};

// nullopt when the header is incomplete.
[[nodiscard]] std::optional<Metadata> readMetadata(const WorldView &world);

// Writes every field of `wanted` that differs from the builder's current
// state. A dimDecor axis is rewritten (to 100 or 0) only when its
// scroll/no-scroll meaning changes, so an unusual non-zero stored value
// survives edits to other fields. Returns false, changing nothing, when the
// header is missing or the name is too long or contains a NUL.
bool applyMetadata(EditBuilder &builder, const Metadata &wanted);

// Single-field helpers used by tools.
bool placeStart(EditBuilder &builder, std::size_t index, CellPos cell);
bool toggleStartDirection(EditBuilder &builder, std::size_t index);
// Copies start `from` (position and direction) to the other three.
bool copyStartToAll(EditBuilder &builder, std::size_t from);

} // namespace eggbert
