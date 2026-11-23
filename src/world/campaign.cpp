// SPDX-License-Identifier: GPL-3.0-only
#include "world/campaign.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <tuple>

namespace eggbert {

namespace {

constexpr std::string_view numberingBasis =
    "numbering rules of the games (hubs are world 1 and multiples of ten, a group has missions "
    "hub+1 to hub+8)";

int roleRank(WorldRole role) {
    switch (role) {
    case WorldRole::start:
        return 0;
    case WorldRole::hub:
    case WorldRole::mission:
        return 1;
    case WorldRole::finale:
        return 2;
    case WorldRole::multiplayer:
        return 3;
    case WorldRole::demo:
        return 4;
    case WorldRole::other:
        return 5;
    }
    return 5;
}

} // namespace

std::vector<CampaignEntry> campaignOrder(const Campaign &campaign) {
    auto entries = campaign.entries;
    const auto key = [&](const CampaignEntry &entry) {
        if (entry.slot.user) {
            return std::tuple(6, entry.slot.gamer, entry.slot.number);
        }
        return std::tuple(roleRank(officialWorldRole(campaign.game, entry.slot.number)), 0,
                          entry.slot.number);
    };
    std::ranges::stable_sort(entries, [&](const CampaignEntry &a, const CampaignEntry &b) {
        return key(a) < key(b);
    });
    return entries;
}

std::vector<CampaignFinding> reviewCampaign(const Campaign &campaign) {
    std::vector<CampaignFinding> findings;
    const auto add = [&](CampaignSeverity severity, std::string text) {
        findings.push_back({severity, std::move(text), numberingBasis});
    };
    std::map<std::string, int> names;
    std::set<int> official;
    for (const auto &entry : campaign.entries) {
        const auto name = worldSlotFileName(entry.slot);
        if (name.empty()) {
            add(CampaignSeverity::warning,
                "An entry is not in a slot the games use (" + entry.fileName + ").");
            continue;
        }
        if (++names[name] == 2) {
            add(CampaignSeverity::warning, name + " is used by more than one entry.");
        }
        if (!entry.slot.user) {
            official.insert(entry.slot.number);
        }
    }
    if (official.empty() || campaign.game == GameProfile::unknown) {
        return findings;
    }
    const bool second = campaign.game == GameProfile::speedyBlupi2;
    const int finale = second ? 199 : 99;
    if (!official.contains(1)) {
        add(CampaignSeverity::warning,
            "World 1 is missing: the game always starts a campaign there.");
    }
    std::set<int> hubsWithMissions;
    for (const auto number : official) {
        const auto role = officialWorldRole(campaign.game, number);
        if (role == WorldRole::mission) {
            hubsWithMissions.insert(number / 10 * 10);
        } else if (role == WorldRole::other) {
            add(CampaignSeverity::note, "World " + std::to_string(number) +
                                            " is outside the campaign numbering; the game "
                                            "reaches it only by its number.");
        }
    }
    for (const auto hub : hubsWithMissions) {
        if (!official.contains(hub)) {
            add(CampaignSeverity::warning,
                "Hub " + std::to_string(hub) + " is missing, but missions of its group (" +
                    std::to_string(hub + 1) + " to " + std::to_string(hub + 8) + ") are present.");
        }
    }
    for (const auto number : official) {
        if (officialWorldRole(campaign.game, number) == WorldRole::hub &&
            !hubsWithMissions.contains(number)) {
            add(CampaignSeverity::note,
                "Hub " + std::to_string(number) + " has none of its missions (" +
                    std::to_string(number + 1) + " to " + std::to_string(number + 8) + ").");
        }
    }
    if (official.contains(1) && !official.contains(finale)) {
        add(CampaignSeverity::note, "World " + std::to_string(finale) +
                                        ", which the game enters after world 1 is won, is "
                                        "missing.");
    }
    return findings;
}

} // namespace eggbert
