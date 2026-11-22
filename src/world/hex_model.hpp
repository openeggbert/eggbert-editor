// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "world/offset_map.hpp"

namespace eggbert {

// Short structural name of an offset, for example "header.music",
// "header.reserve1[17]", "foreground[3][70]", "objects[137].posEnd.y",
// "suffix+5" or "unmapped".
[[nodiscard]] std::string offsetLabel(const WorldProbe &probe, std::size_t offset);

// The arithmetic that leads to the element containing an offset, spelled
// out with this file's numbers, for example
// "foreground cell (3, 70): 868 + 2 x (100 x 3 + 70) = 1608".
// Empty for suffix and unmapped offsets.
[[nodiscard]] std::string offsetCalculation(const WorldProbe &probe, std::size_t offset);

// The little-endian value that contains an offset.
struct DecodedElement {
    ByteRange range;
    // 1, 2 or 4 bytes.
    std::size_t width = 0;
    std::int64_t signedValue = 0;
    std::uint64_t unsignedValue = 0;
};

// Decodes the scalar containing `offset`: a 16-bit header/cell/object value,
// a 32-bit header value, one 32-bit component of a POINT, or one name byte.
// nullopt for suffix and unmapped offsets or when the scalar is incomplete.
[[nodiscard]] std::optional<DecodedElement> decodeScalarAt(std::span<const std::byte> bytes,
                                                           const WorldProbe &probe,
                                                           std::size_t offset);

// Parses "0a ff 1B", "0AFF1B" or "0x0a,0xff": hex digit pairs with optional
// spaces, commas and 0x prefixes. nullopt when malformed or empty.
[[nodiscard]] std::optional<std::vector<std::byte>> parseHexPattern(std::string_view text);
// Lower-case hex pairs separated by `separator` (none when '\0').
[[nodiscard]] std::string formatHex(std::span<const std::byte> bytes, char separator = ' ');

// First occurrence of `needle` at or after `from`; with `wrap`, continues
// from the beginning up to `from`. nullopt when absent or needle is empty.
[[nodiscard]] std::optional<std::size_t> findBytes(std::span<const std::byte> haystack,
                                                   std::span<const std::byte> needle,
                                                   std::size_t from, bool wrap);

// Start of each section in file order with its display name, for "jump to
// section". Includes "suffix" when surplus bytes exist.
struct SectionJump {
    std::string name;
    std::size_t offset = 0;
    std::size_t size = 0;
};
[[nodiscard]] std::vector<SectionJump> sectionJumps(const WorldProbe &probe);

} // namespace eggbert
