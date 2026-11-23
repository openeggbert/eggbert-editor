// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "world/installation.hpp"

namespace eggbert {

// One world of a campaign and the place it has in an installation.
struct CampaignEntry {
    WorldSlot slot;
    // Name of the world file inside a pack (the slot's own file name).
    std::string fileName;
    std::uint64_t size = 0;
    // Lower-case hexadecimal SHA-256 of the file.
    std::string sha256;
    // Title stored in the world, when it has one (informational).
    std::string title;

    friend bool operator==(const CampaignEntry &, const CampaignEntry &) = default;
};

// A named set of worlds for one game.
struct Campaign {
    std::string name;
    GameProfile game = GameProfile::unknown;
    std::vector<CampaignEntry> entries;

    friend bool operator==(const Campaign &, const Campaign &) = default;
};

enum class CampaignSeverity { note, warning };

struct CampaignFinding {
    CampaignSeverity severity = CampaignSeverity::note;
    std::string text;
    // What the finding rests on: always the games' numbering rules
    // (docs/research/evidence.md, "Files of an installation"), never the
    // content of a world.
    std::string_view basis;
};

// Entries in the order the game reaches them: the first world, then each
// group's hub followed by its missions, the final world, network worlds,
// demo worlds, other official numbers, and designed worlds by player and
// number. Stable for equal keys.
[[nodiscard]] std::vector<CampaignEntry> campaignOrder(const Campaign &campaign);

// What the numbering alone says about the set: slots used twice, a hub
// without its missions or missions without their hub, a missing first or
// final world, numbers outside the scheme. An empty result means the
// official worlds form a complete chain by number; it says nothing about
// whether the doors inside the worlds lead anywhere.
[[nodiscard]] std::vector<CampaignFinding> reviewCampaign(const Campaign &campaign);

} // namespace eggbert
