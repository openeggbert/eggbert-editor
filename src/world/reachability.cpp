// SPDX-License-Identifier: GPL-3.0-only
#include "world/reachability.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace eggbert {

namespace {

struct Range {
    int first;
    int last;
};

// Tiles all of whose sixteen entries in table_decor_quart are non-zero
// (blupi_recon e05e22b src/decblock.cpp:480-600; the two games' entries for
// 0-236 are identical).
constexpr Range sharedSolid[] = {{0, 14},   {23, 27},   {30, 44},   {48, 67},   {78, 84},
                                 {96, 107}, {139, 153}, {156, 157}, {186, 197}, {209, 234}};
// games/sb2/src/decblock.cpp:408-630, tiles 237-440.
constexpr Range secondSolid[] = {{246, 250}, {254, 255}, {257, 258}, {261, 263},
                                 {283, 284}, {301, 301}, {304, 304}, {338, 338},
                                 {340, 363}, {373, 374}, {386, 397}};

bool inRanges(std::span<const Range> ranges, int value) {
    for (const auto &range : ranges) {
        if (value >= range.first && value <= range.last) {
            return true;
        }
    }
    return false;
}

constexpr std::int16_t typeTreasure = 5;
constexpr std::int16_t typeGoal = 7;

} // namespace

bool tileFullySolid(GameProfile profile, int tile) {
    if (profile == GameProfile::unknown) {
        return false;
    }
    // The port's collision table equals the second game's except for tiles
    // 49 to 67, none of whose quarters is solid there.
    if (profile == GameProfile::mobileEggbert && tile >= 49 && tile <= 67) {
        return false;
    }
    return inRanges(sharedSolid, tile) ||
           (tablesOf(profile) == GameProfile::speedyBlupi2 && inRanges(secondSolid, tile));
}

ReachabilityEstimate estimateReachability(const WorldView &world, GameProfile profile) {
    ReachabilityEstimate estimate;
    const auto foreground = world.foreground();
    const auto header = world.header();
    const auto objects = world.objects();
    if (profile == GameProfile::unknown) {
        estimate.reason = "no game is chosen";
        return estimate;
    }
    if (!foreground || !header || !objects) {
        estimate.reason = "the world is incomplete";
        return estimate;
    }
    constexpr auto width = layout::mapWidth;
    constexpr auto height = layout::mapHeight;
    std::vector<unsigned char> solid(width * height, 0);
    for (std::size_t x = 0; x < width; ++x) {
        for (std::size_t y = 0; y < height; ++y) {
            const int tile = *foreground->cell(x, y);
            if (tablesOf(profile) == GameProfile::speedyBlupi2 && tile >= 330 && tile <= 336) {
                estimate.reason = "the world has teleporters or key doors";
                return estimate;
            }
            solid[x * height + y] = tileFullySolid(profile, tile) ? 1 : 0;
        }
    }
    // The cell a position is in, as the games test positions (30 pixels in).
    const auto cellOf = [](Point position) -> std::optional<std::size_t> {
        const long long x = static_cast<long long>(position.x) + 30;
        const long long y = static_cast<long long>(position.y) + 30;
        if (x < 0 || y < 0 || x >= layout::worldPixelWidth || y >= layout::worldPixelHeight) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(x / layout::cellPixels) * height +
               static_cast<std::size_t>(y / layout::cellPixels);
    };
    std::vector<unsigned char> connected(width * height, 0);
    std::vector<std::size_t> pending;
    for (std::size_t player = 0; player < 4; ++player) {
        if (const auto cell = cellOf(*header->blupiPos(player)); cell && solid[*cell] == 0) {
            pending.push_back(*cell);
        }
    }
    if (pending.empty()) {
        estimate.reason = "no start lies in a free cell";
        return estimate;
    }
    while (!pending.empty()) {
        const auto cell = pending.back();
        pending.pop_back();
        if (connected[cell] != 0 || solid[cell] != 0) {
            continue;
        }
        connected[cell] = 1;
        ++estimate.connectedCells;
        const auto x = cell / height;
        const auto y = cell % height;
        if (x > 0) {
            pending.push_back(cell - height);
        }
        if (x + 1 < width) {
            pending.push_back(cell + height);
        }
        if (y > 0) {
            pending.push_back(cell - 1);
        }
        if (y + 1 < height) {
            pending.push_back(cell + 1);
        }
    }
    estimate.made = true;
    for (std::size_t slot = 0; slot < objects->slotCount(); ++slot) {
        const auto record = *objects->record(slot);
        if (record.type != typeTreasure && record.type != typeGoal) {
            continue;
        }
        const auto cell = cellOf(record.posStart);
        if (cell && connected[*cell] == 0) {
            estimate.cutOffSlots.push_back(slot);
        }
    }
    return estimate;
}

} // namespace eggbert
