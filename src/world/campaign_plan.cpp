// SPDX-License-Identifier: GPL-3.0-only
#include "world/campaign_plan.hpp"

#include <algorithm>

#include "world/campaign_doors.hpp"
#include "world/new_world.hpp"
#include "world/object_catalog.hpp"

namespace eggbert {

std::optional<CampaignShape> campaignShape(GameProfile profile) {
    switch (profile) {
    case GameProfile::speedyBlupi1:
        return CampaignShape{8, 8, 99};
    case GameProfile::speedyBlupi2:
    case GameProfile::mobileEggbert:
        return CampaignShape{14, 8, 199};
    case GameProfile::unknown:
        break;
    }
    return std::nullopt;
}

std::int16_t hubSignTile(GameProfile profile, int index) {
    const auto shape = campaignShape(profile);
    if (!shape || index < 1 || index > shape->hubCount) {
        return -1;
    }
    if (index <= 8) {
        return static_cast<std::int16_t>(157 + index);
    }
    return static_cast<std::int16_t>(index == 9 ? 309 : 401 + index);
}

std::int16_t missionSignTile(int index) {
    return index >= 1 && index <= 8 ? static_cast<std::int16_t>(173 + index) : std::int16_t{-1};
}

namespace {

std::size_t countTile(const WorldView &world, std::int16_t tile) {
    std::size_t count = 0;
    if (const auto foreground = world.foreground()) {
        for (std::size_t x = 0; x < layout::mapWidth; ++x) {
            for (std::size_t y = 0; y < layout::mapHeight; ++y) {
                count += *foreground->cell(x, y) == tile ? 1 : 0;
            }
        }
    }
    return count;
}

std::size_t countObjects(const WorldView &world, std::int16_t type) {
    std::size_t count = 0;
    if (const auto objects = world.objects()) {
        for (std::size_t slot = 0; slot < objects->slotCount(); ++slot) {
            count += objects->record(slot)->type == type ? 1 : 0;
        }
    }
    return count;
}

// True when the hub holds the sign of mission `index` with a door tile one
// or two cells to its left or right, which is where the games look for the
// door they open.
bool signWithDoor(const WorldView &hub, int index) {
    const auto foreground = hub.foreground();
    if (!foreground) {
        return false;
    }
    const auto sign = missionSignTile(index);
    for (std::size_t x = 0; x < layout::mapWidth; ++x) {
        for (std::size_t y = 0; y < layout::mapHeight; ++y) {
            if (*foreground->cell(x, y) != sign) {
                continue;
            }
            for (const int offset : {-1, -2, 1, 2}) {
                const auto beside = static_cast<long long>(x) + offset;
                if (beside >= 0 && beside < static_cast<long long>(layout::mapWidth) &&
                    *foreground->cell(static_cast<std::size_t>(beside), y) == missionDoorTile) {
                    return true;
                }
            }
        }
    }
    return false;
}

} // namespace

CampaignPlan planCampaign(GameProfile profile, const std::map<int, WorldView> &worlds) {
    CampaignPlan plan;
    plan.game = profile;
    const auto shape = campaignShape(profile);
    if (!shape) {
        return plan;
    }
    plan.shape = *shape;
    const auto first = worlds.find(1);
    plan.firstWorld = first != worlds.end();
    plan.finale = worlds.contains(shape->finale);
    std::vector<int> firstWorldCodes;
    if (plan.firstWorld) {
        plan.barriers = static_cast<int>(countTile(first->second, barrierTile));
        for (const auto &door : doorTiles(first->second, profile)) {
            firstWorldCodes.push_back(door.code);
        }
    }
    const auto add = [&](PlanSeverity severity, int world, std::string text) {
        plan.findings.push_back({severity, std::move(text), world});
    };
    const auto name = [](int number) { return std::to_string(number); };
    int hubsPresent = 0;
    for (int index = 1; index <= shape->hubCount; ++index) {
        PlanHub hub;
        hub.number = index * 10;
        const auto found = worlds.find(hub.number);
        hub.present = found != worlds.end();
        hub.signInFirstWorld = std::ranges::find(firstWorldCodes, index) != firstWorldCodes.end();
        std::vector<int> hubCodes;
        if (hub.present) {
            ++hubsPresent;
            hub.hasExit = countTile(found->second, exitTile) != 0;
            for (const auto &door : doorTiles(found->second, profile)) {
                hubCodes.push_back(door.code);
            }
        }
        int keys = 0;
        int missionsPresent = 0;
        for (int mission = 1; mission <= shape->missionsPerHub; ++mission) {
            PlanMission slot;
            slot.number = hub.number + mission;
            const auto world = worlds.find(slot.number);
            slot.present = world != worlds.end();
            slot.signInHub = std::ranges::find(hubCodes, mission) != hubCodes.end();
            slot.doorInHub = hub.present && signWithDoor(found->second, mission);
            if (slot.present) {
                ++missionsPresent;
                slot.hasGoal = countObjects(world->second, goalObjectType) != 0;
                slot.hasKey = countObjects(world->second, hubKeyObjectType) != 0;
                keys += slot.hasKey ? 1 : 0;
                if (!slot.hasGoal && !slot.hasKey) {
                    add(PlanSeverity::warning, slot.number,
                        "Mission " + name(slot.number) +
                            " has neither a goal nor a key, so it cannot be won.");
                }
                // The first mission has no door in the games' own hubs: it
                // is open from the start, and winning a mission opens the
                // door of the next.
                if (hub.present && mission > 1 && slot.signInHub && !slot.doorInHub) {
                    add(PlanSeverity::note, slot.number,
                        "The sign of mission " + name(slot.number) + " in hub " +
                            name(hub.number) +
                            " has no door (tile 182) one or two cells beside it, so the mission "
                            "can be entered from the start.");
                }
            }
            hub.missions.push_back(slot);
        }
        if (missionsPresent != 0 && keys == 0) {
            add(PlanSeverity::warning, hub.number,
                "No mission of hub " + name(hub.number) +
                    " ends with a key (an object of type 21), so the hub can never be "
                    "completed and no barrier of world 1 opens for it.");
        } else if (keys > 1) {
            add(PlanSeverity::note, hub.number,
                name(keys) + " missions of hub " + name(hub.number) +
                    " end with a key; winning any one of them completes the hub.");
        }
        plan.hubs.push_back(std::move(hub));
    }
    if (plan.firstWorld && plan.barriers > hubsPresent) {
        add(PlanSeverity::note, 1,
            "World 1 has " + name(plan.barriers) + " barriers (tile 183) but only " +
                name(hubsPresent) +
                " hubs are present; the game removes one barrier for every hub that is won.");
    }
    return plan;
}

std::vector<std::byte> newCampaignWorld(GameProfile profile, CampaignWorldKind kind) {
    auto bytes = newWorld(profile);
    if (kind == CampaignWorldKind::mission) {
        return bytes;
    }
    const auto &revision = *findRevision(newWorldRevision(profile));
    const auto put16 = [&](std::size_t offset, std::int16_t value) {
        const auto raw = static_cast<std::uint16_t>(value);
        bytes[offset] = static_cast<std::byte>(raw & 0xFFu);
        bytes[offset + 1] = static_cast<std::byte>(raw >> 8);
    };
    const auto objects = revision.section(SectionKind::objects)->offset;
    // The new world's two objects: the treasure in slot 0, the goal in slot 1.
    const auto goal = objects + layout::objectRecordBytes;
    if (kind == CampaignWorldKind::keyMission) {
        // The key as the game's own editor stores it, where the goal stood.
        const auto *key = findObjectPreset(profile, "key");
        put16(goal + objectField(ObjectField::type).offset, hubKeyObjectType);
        put16(goal + objectField(ObjectField::channel).offset, key != nullptr ? key->channel : 10);
        put16(goal + objectField(ObjectField::icon).offset, key != nullptr ? key->icon : 125);
        return bytes;
    }
    // A hub: no objects. In the Windows games the exit to world 1 stands
    // where the goal stood (the cell above the right end of the platform),
    // as their own hubs have one; the hubs of Mobile Eggbert have none (a
    // hub is left through the game's menu there).
    std::fill(bytes.begin() + static_cast<std::ptrdiff_t>(objects),
              bytes.begin() + static_cast<std::ptrdiff_t>(objects + 2 * layout::objectRecordBytes),
              std::byte{0});
    if (profile != GameProfile::mobileEggbert) {
        const auto foreground = revision.section(SectionKind::foreground)->offset;
        put16(foreground + (5 * layout::mapHeight + 3) * layout::tileBytes, exitTile);
    }
    return bytes;
}

} // namespace eggbert
