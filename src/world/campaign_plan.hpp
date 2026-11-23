// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "world/installation.hpp"
#include "world/world_view.hpp"

namespace eggbert {

// What a campaign of a game can hold and what its worlds need, for building
// one (docs/research/evidence.md, "Room in a campaign"). Nothing here
// changes a world.

struct CampaignShape {
    // Hubs are worlds 10, 20, ... 10 * hubCount; a hub's missions are
    // hub + 1 ... hub + missionsPerHub.
    int hubCount = 0;
    int missionsPerHub = 8;
    // The world entered after world 1 is won.
    int finale = 0;
};

// Nothing for GameProfile::unknown.
[[nodiscard]] std::optional<CampaignShape> campaignShape(GameProfile profile);

// Tiles the games give a meaning between worlds.
inline constexpr std::int16_t missionDoorTile = 182; // closed door beside a mission's sign
inline constexpr std::int16_t barrierTile = 183;     // barrier in world 1, one opens per hub won
inline constexpr std::int16_t exitTile = 184;        // back to world 1

// The sign for hub `index` (1 = world 10) as it is placed in world 1, in
// its closed form; -1 when the game has no sign for that number.
[[nodiscard]] std::int16_t hubSignTile(GameProfile profile, int index);
// The sign for mission `index` (1 to 8) as it is placed in its hub.
[[nodiscard]] std::int16_t missionSignTile(int index);

// The type of the object that ends the last mission of a hub in place of
// the goal: reaching it wins the hub (TYPE_CLE in both games).
inline constexpr std::int16_t hubKeyObjectType = 21;
inline constexpr std::int16_t goalObjectType = 7;

struct PlanMission {
    int number = 0;
    bool present = false;
    // The hub holds a sign leading to it, and a door beside that sign. The
    // first mission of a hub needs no door: it is open from the start.
    bool signInHub = false;
    bool doorInHub = false;
    bool hasGoal = false;
    bool hasKey = false;
};

struct PlanHub {
    int number = 0;
    bool present = false;
    // World 1 holds a sign leading to it.
    bool signInFirstWorld = false;
    // The hub holds the exit tile to world 1. Not needed: leaving a hub
    // through the game's menu leads to world 1 as well, and some of the
    // games' own hubs have none.
    bool hasExit = false;
    std::vector<PlanMission> missions;
};

enum class PlanSeverity { note, warning };

struct PlanFinding {
    PlanSeverity severity = PlanSeverity::note;
    std::string text;
    // The world the finding is about.
    int world = 0;
};

struct CampaignPlan {
    GameProfile game = GameProfile::unknown;
    CampaignShape shape;
    bool firstWorld = false;
    bool finale = false;
    // Barrier tiles in world 1; the game removes one for every hub won.
    int barriers = 0;
    std::vector<PlanHub> hubs;
    std::vector<PlanFinding> findings;
};

// `worlds` maps official numbers to their complete worlds.
[[nodiscard]] CampaignPlan planCampaign(GameProfile profile,
                                        const std::map<int, WorldView> &worlds);

enum class CampaignWorldKind {
    // The game's new world: a platform, a treasure and the goal.
    mission,
    // The same with the hub's key in place of the goal.
    keyMission,
    // A platform and no objects; with the exit to world 1 in the Windows
    // games.
    hub,
};

// A world to start a hub or a mission from, in the revision new worlds of
// the game are written in (for Mobile Eggbert: the image of its text).
[[nodiscard]] std::vector<std::byte> newCampaignWorld(GameProfile profile, CampaignWorldKind kind);

} // namespace eggbert
