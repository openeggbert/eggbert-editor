// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <vector>

#include "world/layout.hpp"

namespace eggbert {

// Revision a new world of a game is written in: what that game's stock
// worlds use (1.2 for the first game, 1.3 otherwise).
[[nodiscard]] FormatRevision newWorldRevision(GameProfile profile);

// The world both games' own editors start a new mission with (see "New
// world" in docs/research/evidence.md): a full-size playfield, music 1,
// region 2, the four starts on one spot, a three-tile platform, one
// treasure and the goal. Every byte the games do not set is zero.
[[nodiscard]] std::vector<std::byte> newWorld(GameProfile profile);

} // namespace eggbert
