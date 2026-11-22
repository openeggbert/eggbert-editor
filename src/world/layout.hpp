// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <cstdint>

namespace eggbert {

enum class GameProfile { unknown, speedyBlupi1, speedyBlupi2 };
enum class FormatRevision { unknown, v1_0, v1_1, v1_2, v1_3 };

namespace v1_3 {

using TileId = std::int16_t;
inline constexpr std::size_t width = 100;
inline constexpr std::size_t height = 100;
inline constexpr std::size_t layerCount = 2;
inline constexpr std::size_t headerBytes = 868;
inline constexpr std::size_t tileLayerBytes = width * height * 2;
inline constexpr std::size_t objectCount = 200;
inline constexpr std::size_t objectBytes = 48;
inline constexpr std::size_t worldBytes =
    headerBytes + layerCount * tileLayerBytes + objectCount * objectBytes;

static_assert(worldBytes == 50'468);

} // namespace v1_3
} // namespace eggbert
