// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Hand-written synthetic world fixtures. Everything here uses literal sizes
// and offsets from the format notes in plan.md and deliberately includes no
// production header, so a mistake in the production layout tables cannot hide
// behind a matching mistake in the fixtures.

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace eggbert::test {

using Bytes = std::vector<std::byte>;

inline void put8(Bytes &bytes, std::size_t offset, unsigned value) {
    bytes.at(offset) = static_cast<std::byte>(value & 0xFF);
}

inline void put16(Bytes &bytes, std::size_t offset, std::int32_t value) {
    const auto raw = static_cast<std::uint16_t>(value);
    put8(bytes, offset, raw & 0xFF);
    put8(bytes, offset + 1, raw >> 8);
}

inline void put32(Bytes &bytes, std::size_t offset, std::int64_t value) {
    const auto raw = static_cast<std::uint32_t>(value);
    for (unsigned index = 0; index < 4; ++index) {
        put8(bytes, offset + index, (raw >> (8 * index)) & 0xFF);
    }
}

inline void putText(Bytes &bytes, std::size_t offset, std::string_view text) {
    for (std::size_t index = 0; index < text.size(); ++index) {
        put8(bytes, offset + index, static_cast<unsigned char>(text[index]));
    }
}

inline unsigned get8(const Bytes &bytes, std::size_t offset) {
    return static_cast<unsigned>(bytes.at(offset));
}

inline Bytes toVector(std::span<const std::byte> bytes) {
    return Bytes(bytes.begin(), bytes.end());
}

// Literal layout numbers.
inline constexpr std::size_t fxHeaderBytes = 868;
inline constexpr std::size_t fxForegroundOffset = 868;
inline constexpr std::size_t fxLayerBytes = 20'000;

struct FixtureLayout {
    int minor;
    bool hasBigDecor;
    std::size_t bigDecorOffset; // meaningful only when hasBigDecor
    std::size_t objectsOffset;
    std::size_t objectCount;
    std::size_t totalBytes;
};

inline constexpr FixtureLayout fxRevision10{0, false, 0, 20'868, 100, 25'668};
inline constexpr FixtureLayout fxRevision11{1, true, 20'868, 40'868, 100, 45'668};
inline constexpr FixtureLayout fxRevision12{2, true, 20'868, 40'868, 200, 50'468};
inline constexpr FixtureLayout fxRevision13{3, true, 20'868, 40'868, 200, 50'468};
inline constexpr FixtureLayout fxRevisions[] = {fxRevision10, fxRevision11, fxRevision12,
                                                fxRevision13};

// Offset of cell (x, y) in a layer that starts at layerOffset: storage is
// [x][y], so x selects the 200-byte column block.
inline std::size_t fxCellOffset(std::size_t layerOffset, std::size_t x, std::size_t y) {
    return layerOffset + 200 * x + 2 * y;
}

inline std::size_t fxSlotOffset(const FixtureLayout &layout, std::size_t slot) {
    return layout.objectsOffset + 48 * slot;
}

// A world of the given revision whose every byte after the revision field is
// a position-dependent non-zero-biased pattern, so reserved fields, padding,
// free slots and any suffix are all distinguishable from zero fill.
inline Bytes patternedWorld(const FixtureLayout &layout, std::size_t surplus = 0) {
    Bytes bytes(layout.totalBytes + surplus);
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<std::byte>((index * 31 + 7) % 253);
    }
    put16(bytes, 0, 1);
    put16(bytes, 2, layout.minor);
    return bytes;
}

// A quiet world: zero header apart from the revision, every cell -1 (empty)
// and every object slot zero (free).
inline Bytes blankWorld(const FixtureLayout &layout, std::size_t surplus = 0) {
    Bytes bytes(layout.totalBytes + surplus);
    put16(bytes, 0, 1);
    put16(bytes, 2, layout.minor);
    for (std::size_t offset = fxForegroundOffset; offset < layout.objectsOffset; ++offset) {
        bytes[offset] = std::byte{0xFF};
    }
    return bytes;
}

// For UI tests that navigate throughout the scrolling map.
inline Bytes blankLargeWorld(const FixtureLayout &layout, std::size_t surplus = 0) {
    auto bytes = blankWorld(layout, surplus);
    put32(bytes, 212, 100);
    put32(bytes, 216, 100);
    return bytes;
}

} // namespace eggbert::test
