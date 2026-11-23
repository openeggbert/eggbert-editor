// SPDX-License-Identifier: GPL-3.0-only
//
// What a campaign can hold and what its worlds need, on worlds built here.
#include "world/campaign_plan.hpp"

#include "world/campaign_doors.hpp"
#include "world/new_world.hpp"
#include "world/world_probe.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <algorithm>

using namespace eggbert;
using namespace eggbert::test;

namespace {

void tile(Bytes &world, std::size_t x, std::size_t y, int value) {
    put16(world, fxCellOffset(fxForegroundOffset, x, y), value);
}

void object(Bytes &world, std::size_t slot, int type) {
    put16(world, fxSlotOffset(fxRevision13, slot), type);
}

bool mentions(const CampaignPlan &plan, std::string_view text, PlanSeverity severity, int world) {
    return std::ranges::any_of(plan.findings, [&](const PlanFinding &finding) {
        return finding.severity == severity && finding.world == world &&
               finding.text.find(text) != std::string::npos;
    });
}

} // namespace

int main() {
    const auto one = GameProfile::speedyBlupi1;
    const auto two = GameProfile::speedyBlupi2;
    const auto port = GameProfile::mobileEggbert;

    // How much a campaign of each game holds.
    CHECK(!campaignShape(GameProfile::unknown).has_value());
    CHECK(campaignShape(one)->hubCount == 8 && campaignShape(one)->finale == 99);
    CHECK(campaignShape(two)->hubCount == 14 && campaignShape(two)->finale == 199);
    CHECK(campaignShape(port)->hubCount == 14 && campaignShape(port)->finale == 199);
    CHECK_EQ(campaignShape(two)->missionsPerHub, 8);

    // Signs: the tiles the games read back as the same numbers.
    CHECK_EQ(hubSignTile(one, 1), 158);
    CHECK_EQ(hubSignTile(one, 8), 165);
    CHECK_EQ(hubSignTile(one, 9), -1);
    CHECK_EQ(hubSignTile(two, 9), 309);
    CHECK_EQ(hubSignTile(two, 10), 411);
    CHECK_EQ(hubSignTile(two, 14), 415);
    CHECK_EQ(hubSignTile(two, 15), -1);
    CHECK_EQ(hubSignTile(two, 0), -1);
    CHECK_EQ(hubSignTile(GameProfile::unknown, 1), -1);
    for (const auto profile : {one, two, port}) {
        for (int index = 1; index <= campaignShape(profile)->hubCount; ++index) {
            CHECK_EQ(doorCode(profile, hubSignTile(profile, index)), index);
            CHECK_EQ(doorTarget(profile, 1, index), index * 10);
        }
        for (int index = 1; index <= 8; ++index) {
            CHECK_EQ(doorCode(profile, missionSignTile(index)), index);
        }
    }
    CHECK_EQ(missionSignTile(1), 174);
    CHECK_EQ(missionSignTile(8), 181);
    CHECK(missionSignTile(0) == -1 && missionSignTile(9) == -1);

    // A small campaign with something of everything.
    {
        auto first = blankWorld(fxRevision13);
        tile(first, 10, 10, 159); // the sign of hub 20
        tile(first, 20, 10, 183);
        tile(first, 21, 10, 183);
        tile(first, 22, 10, 183);
        auto hub20 = blankWorld(fxRevision13);
        tile(hub20, 5, 5, 184);
        tile(hub20, 11, 8, 174); // sign of mission 1: needs no door
        tile(hub20, 30, 8, 175); // sign of mission 2, no door
        tile(hub20, 30, 9, 182); // below it does not count
        tile(hub20, 42, 8, 176); // sign of mission 3 ...
        tile(hub20, 40, 8, 182); // ... with its door two cells to the left
        auto mission21 = blankWorld(fxRevision13);
        object(mission21, 0, 5);
        object(mission21, 1, 7);
        auto mission22 = blankWorld(fxRevision13);
        object(mission22, 3, 21);
        const auto mission23 = blankWorld(fxRevision13);
        const auto hub30 = blankWorld(fxRevision13);
        auto mission31 = blankWorld(fxRevision13);
        object(mission31, 0, 7);
        const std::map<int, Bytes> files{{1, first},      {20, hub20},     {21, mission21},
                                         {22, mission22}, {23, mission23}, {30, hub30},
                                         {31, mission31}};
        std::map<int, WorldView> worlds;
        for (const auto &[number, bytes] : files) {
            worlds.emplace(number, WorldView(bytes));
        }
        const auto plan = planCampaign(two, worlds);
        CHECK(plan.game == two && plan.firstWorld && !plan.finale);
        CHECK_EQ(plan.barriers, 3);
        CHECK_EQ(plan.hubs.size(), std::size_t{14});
        const auto &first10 = plan.hubs[0];
        CHECK(first10.number == 10 && !first10.present && !first10.signInFirstWorld);
        CHECK_EQ(first10.missions.size(), std::size_t{8});
        CHECK(first10.missions[7].number == 18 && !first10.missions[7].present);
        const auto &hub = plan.hubs[1];
        CHECK(hub.number == 20 && hub.present && hub.signInFirstWorld && hub.hasExit);
        CHECK(hub.missions[0].present && hub.missions[0].signInHub && !hub.missions[0].doorInHub &&
              hub.missions[0].hasGoal && !hub.missions[0].hasKey);
        CHECK(hub.missions[1].present && hub.missions[1].signInHub && !hub.missions[1].doorInHub &&
              hub.missions[1].hasKey && !hub.missions[1].hasGoal);
        CHECK(hub.missions[2].present && hub.missions[2].signInHub && hub.missions[2].doorInHub);
        CHECK(!hub.missions[3].present);
        const auto &other = plan.hubs[2];
        CHECK(other.present && !other.signInFirstWorld && !other.hasExit);
        CHECK(mentions(plan, "Mission 23 has neither a goal nor a key", PlanSeverity::warning, 23));
        CHECK(mentions(plan, "The sign of mission 22 in hub 20 has no door", PlanSeverity::note, 22));
        CHECK(!mentions(plan, "The sign of mission 21", PlanSeverity::note, 21));
        // A hub without the exit tile is fine: the games' menus lead back too.
        CHECK(!mentions(plan, "Hub 30", PlanSeverity::warning, 30) ||
              mentions(plan, "No mission of hub 30 ends with a key", PlanSeverity::warning, 30));
        CHECK(mentions(plan, "No mission of hub 30 ends with a key", PlanSeverity::warning, 30));
        CHECK(!mentions(plan, "No mission of hub 20", PlanSeverity::warning, 20));
        CHECK(mentions(plan, "World 1 has 3 barriers (tile 183) but only 2 hubs",
                       PlanSeverity::note, 1));
        CHECK_EQ(plan.findings.size(), std::size_t{4});

        // Two keys in one hub is said; an empty hub is not told to have none.
        auto second = mission21;
        object(second, 1, 21);
        worlds.erase(21);
        worlds.emplace(21, WorldView(second));
        worlds.erase(31);
        const auto again = planCampaign(two, worlds);
        CHECK(mentions(again, "2 missions of hub 20 end with a key", PlanSeverity::note, 20));
        CHECK(!mentions(again, "No mission of hub 30", PlanSeverity::warning, 30));
        // The first game has eight hubs; no game, nothing.
        CHECK_EQ(planCampaign(one, worlds).hubs.size(), std::size_t{8});
        CHECK(planCampaign(GameProfile::unknown, worlds).hubs.empty());
        CHECK(planCampaign(GameProfile::unknown, worlds).findings.empty());
        // Without world 1 nothing is said about barriers or signs.
        worlds.erase(1);
        const auto headless = planCampaign(two, worlds);
        CHECK(!headless.firstWorld && headless.barriers == 0);
        CHECK(!headless.hubs[1].signInFirstWorld);
    }

    // Worlds to start from.
    for (const auto profile : {one, two, port}) {
        const auto mission = newCampaignWorld(profile, CampaignWorldKind::mission);
        CHECK(mission == newWorld(profile));
        const auto key = newCampaignWorld(profile, CampaignWorldKind::keyMission);
        const auto hub = newCampaignWorld(profile, CampaignWorldKind::hub);
        for (const auto &bytes : {key, hub}) {
            const auto probe = probeWorld(bytes);
            CHECK(probe.status == ProbeStatus::sizeMatches);
            CHECK_EQ(bytes.size(), mission.size());
        }
        const WorldView keyWorld(key);
        CHECK_EQ(keyWorld.objects()->activeCount(), std::size_t{2});
        CHECK_EQ(keyWorld.objects()->record(1)->type, hubKeyObjectType);
        CHECK_EQ(keyWorld.objects()->record(1)->icon, 125);
        CHECK_EQ(keyWorld.objects()->record(1)->channel, 10);
        CHECK(keyWorld.objects()->record(1)->posStart ==
              WorldView(mission).objects()->record(1)->posStart);
        const WorldView hubWorld(hub);
        CHECK_EQ(hubWorld.objects()->activeCount(), std::size_t{0});
        // The Windows games' hubs have the exit; those of the port have none.
        CHECK_EQ(*hubWorld.foreground()->cell(5, 3), profile == port ? -1 : exitTile);
        CHECK_EQ(*hubWorld.foreground()->cell(4, 4), 38); // the platform stays
        // As the plan sees them.
        std::map<int, WorldView> worlds;
        worlds.emplace(20, hubWorld);
        worlds.emplace(21, keyWorld);
        const auto plan = planCampaign(profile, worlds);
        CHECK(plan.hubs[1].hasExit == (profile != port) && plan.hubs[1].missions[0].hasKey);
        CHECK(plan.findings.empty());
    }
    return test::finish("campaign_plan");
}
