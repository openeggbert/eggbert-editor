// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <map>
#include <vector>

#include "world/installation.hpp"
#include "world/world_view.hpp"

namespace eggbert {

// Door tiles are how a campaign's worlds lead to one another (see
// docs/research/evidence.md, "Doors between worlds").

// The number a door tile stands for in a game: 1 to 8 (first game) or 1 to
// 14 (second), the exit code 99 / 199 for tile 184, or -1 for any other
// tile.
[[nodiscard]] int doorCode(GameProfile profile, int tile);

// The official world a door with `code` leads to from official world
// `number`: from world 1 to code * 10; elsewhere the exit code leads to
// world 1 and any other code to the world's group hub + code.
[[nodiscard]] int doorTarget(GameProfile profile, int number, int code);

struct DoorTile {
    CellPos cell;
    std::int16_t tile = 0;
    int code = 0;
};

[[nodiscard]] std::vector<DoorTile> doorTiles(const WorldView &world, GameProfile profile);

struct MissingDoorTarget {
    // The world the door is in, where, which tile, and the absent target.
    int world = 0;
    CellPos cell;
    std::int16_t tile = 0;
    int target = 0;
};

struct DoorReview {
    // Doors of the first world, hubs and missions whose target world is not
    // among the given worlds.
    std::vector<MissingDoorTarget> missingTargets;
    // Hubs and missions present that no door of the given worlds leads to.
    std::vector<int> unreferenced;
};

// `worlds` maps official numbers to their (complete) worlds.
[[nodiscard]] DoorReview reviewDoors(GameProfile profile, const std::map<int, WorldView> &worlds);

} // namespace eggbert
