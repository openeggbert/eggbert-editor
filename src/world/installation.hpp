// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "world/layout.hpp"

namespace eggbert {

// What the games keep in an installation's `data` directory, by file name
// (docs/research/evidence.md, "Files of an installation"). Names only: the
// content is probed separately.
enum class DataFileKind {
    // world%.3d.blp
    officialWorld,
    // u%.3d-%.3d.blp: a world a player designed (gamer, number)
    userWorld,
    // s%.3d-%.3d.blp: a saved game
    savedGame,
    // info%.3d.blp: a player's progress
    gamerInfo,
    // demo%.3d.blp: a recording
    demoRecording,
    // config.def, time.blp, bench.blp
    gameState,
    other,
};

struct DataFileName {
    DataFileKind kind = DataFileKind::other;
    // Player slot for user worlds, saves and info files; 0 otherwise.
    int gamer = 0;
    // Mission, world, save or demo number; 0 when the kind has none.
    int number = 0;

    friend bool operator==(const DataFileName &, const DataFileName &) = default;
};

// Case does not matter (installations exist with upper-case names).
[[nodiscard]] DataFileName classifyDataFileName(std::string_view name);

inline constexpr int gamerCount = 8;        // MAXGAMER
inline constexpr int userWorldCount = 20;   // user worlds per gamer
inline constexpr int scratchWorldNumber = 999; // the games' own play-test file

// A place in an installation a world can be put.
struct WorldSlot {
    // False: an official world (world%.3d.blp). True: a user world.
    bool user = true;
    int gamer = 1;
    int number = 1;

    friend bool operator==(const WorldSlot &, const WorldSlot &) = default;
};

// True for gamer 1..8 and number 1..20 (user) or number 1..999 (official).
// The scratch number 999 of a user slot is refused: the game overwrites it.
[[nodiscard]] bool validWorldSlot(const WorldSlot &slot);
// Lower-case file name of a slot, as the games format it; empty when the
// slot is not valid.
[[nodiscard]] std::string worldSlotFileName(const WorldSlot &slot);
[[nodiscard]] std::optional<WorldSlot> worldSlotOf(const DataFileName &name);

// What an official world number is used for, by the numbering both games'
// code implies. The file's content does not enter into it.
enum class WorldRole {
    // World 1: the first world, whose doors lead to the group hubs.
    start,
    // A multiple of ten in the game's range: the hub of a group.
    hub,
    // Hub + 1..8: a mission of a group.
    mission,
    // 99 (first game) or 199 (second): the world reached from world 1.
    finale,
    // 101..110 or 201..212: chosen by the host of a network game.
    multiplayer,
    // 200.. or 300..: played back or recorded as a demo.
    demo,
    other,
};

// Mobile Eggbert numbers its campaign as the second game does (hubs 10 to
// 140, missions hub + 1 to 8, last world 199) but has neither network
// worlds nor demos (evidence.md, "Mobile Eggbert as a game profile").
[[nodiscard]] WorldRole officialWorldRole(GameProfile profile, int number);

// The number of a Mobile Eggbert world file, `world%.3d.txt` in the port's
// `worlds` folder; case does not matter.
[[nodiscard]] std::optional<int> portWorldNumber(std::string_view name);
// Its file name for a number from 0 to 999; empty otherwise.
[[nodiscard]] std::string portWorldFileName(int number);
[[nodiscard]] std::string_view worldRoleName(WorldRole role);

} // namespace eggbert
