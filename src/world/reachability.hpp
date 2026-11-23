// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "world/world_view.hpp"

namespace eggbert {

// True for a foreground tile number whose sixteen collision quarters are
// all solid in the game (the games' own IsBlocIcon test). Computed from each
// game's collision table; see docs/research/evidence.md, "Reachability".
[[nodiscard]] bool tileFullySolid(GameProfile profile, int tile);

// An estimate, not a proof, of which treasures and goals a player can get
// to: the cells connected to a player start through cells that are not
// fully solid (up, down, left, right). It knows nothing about jumping,
// falling, vehicles or lifts, so "connected" does not mean reachable; but an
// object in a cell that is NOT connected can only be reached if the game
// changes the map while playing.
struct ReachabilityEstimate {
    // False when the estimate was not made; `reason` says why.
    bool made = false;
    std::string_view reason;
    // Cells connected to a start.
    std::size_t connectedCells = 0;
    // Slots of treasures (type 5) and goals (type 7) outside them.
    std::vector<std::size_t> cutOffSlots;
};

// Not made for an unknown game, an incomplete world, or a second-game world
// that contains teleporters (tiles 330-333) or key doors (334-336), which
// connect or open cells in ways cell adjacency does not show.
[[nodiscard]] ReachabilityEstimate estimateReachability(const WorldView &world,
                                                        GameProfile profile);

} // namespace eggbert
