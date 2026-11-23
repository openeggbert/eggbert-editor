// SPDX-License-Identifier: GPL-3.0-only
//
// Door tiles and where they lead, as the games compute it.
#include "world/campaign_doors.hpp"

#include "world/world_probe.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <algorithm>

using namespace eggbert;
using namespace eggbert::test;

namespace {

constexpr auto first = GameProfile::speedyBlupi1;
constexpr auto second = GameProfile::speedyBlupi2;

Bytes worldWithDoors(std::initializer_list<int> tiles) {
    auto bytes = blankWorld(fxRevision13);
    std::size_t x = 10;
    for (const auto tile : tiles) {
        put16(bytes, fxCellOffset(fxForegroundOffset, x++, 20), tile);
    }
    return bytes;
}

} // namespace

int main() {
    // Codes.
    CHECK_EQ(doorCode(first, 158), 1);
    CHECK_EQ(doorCode(first, 165), 8);
    CHECK_EQ(doorCode(first, 166), 1);
    CHECK_EQ(doorCode(first, 173), 8);
    CHECK_EQ(doorCode(first, 174), 1);
    CHECK_EQ(doorCode(first, 181), 8);
    CHECK_EQ(doorCode(first, 184), 99);
    CHECK_EQ(doorCode(second, 184), 199);
    CHECK_EQ(doorCode(first, 157), -1);
    CHECK_EQ(doorCode(first, 182), -1);
    CHECK_EQ(doorCode(first, 183), -1);
    CHECK_EQ(doorCode(first, 309), -1); // second game only
    CHECK_EQ(doorCode(second, 309), 9);
    CHECK_EQ(doorCode(second, 310), 9);
    CHECK_EQ(doorCode(second, 411), 10);
    CHECK_EQ(doorCode(second, 415), 14);
    CHECK_EQ(doorCode(second, 416), 10);
    CHECK_EQ(doorCode(second, 420), 14);
    CHECK_EQ(doorCode(second, 421), -1);
    CHECK_EQ(doorCode(GameProfile::unknown, 158), -1);
    CHECK_EQ(doorCode(first, -1), -1);

    // Targets.
    CHECK_EQ(doorTarget(first, 1, 3), 30);     // from the first world to a hub
    CHECK_EQ(doorTarget(second, 1, 14), 140);
    CHECK_EQ(doorTarget(first, 30, 5), 35);    // from a hub to a mission
    CHECK_EQ(doorTarget(first, 35, 2), 32);    // from a mission into its own group
    CHECK_EQ(doorTarget(first, 30, 99), 1);    // the exit
    CHECK_EQ(doorTarget(second, 120, 199), 1);
    CHECK_EQ(doorTarget(second, 120, 99), 219); // 99 is no exit code in the second game

    // Tiles found in a world.
    {
        const auto bytes = worldWithDoors({174, 30, 181, 184, 309});
        const WorldView view(bytes, probeWorld(bytes));
        const auto doors = doorTiles(view, first);
        CHECK_EQ(doors.size(), std::size_t{3});
        CHECK(doors[0].cell == (CellPos{10, 20}) && doors[0].tile == 174 && doors[0].code == 1);
        CHECK(doors[2].code == 99);
        CHECK_EQ(doorTiles(view, second).size(), std::size_t{4});
        CHECK(doorTiles(view, GameProfile::unknown).empty());
    }

    // A small campaign: world 1 leads to hubs 10 and 20; hub 10 to missions
    // 11 and 12 and back; hub 20 is absent; mission 13 has no door to it.
    {
        const auto start = worldWithDoors({158, 159});
        const auto hub = worldWithDoors({174, 175, 184});
        const auto mission = worldWithDoors({});
        const auto outside = worldWithDoors({174}); // world 95: not in the scheme
        const WorldView startView(start, probeWorld(start));
        const WorldView hubView(hub, probeWorld(hub));
        const WorldView missionView(mission, probeWorld(mission));
        const WorldView outsideView(outside, probeWorld(outside));
        const std::map<int, WorldView> worlds{{1, startView},     {10, hubView},
                                              {11, missionView},  {12, missionView},
                                              {13, missionView},  {95, outsideView}};
        const auto review = reviewDoors(first, worlds);
        CHECK_EQ(review.missingTargets.size(), std::size_t{1});
        CHECK(review.missingTargets[0].world == 1 && review.missingTargets[0].target == 20);
        CHECK(review.missingTargets[0].tile == 159);
        CHECK(review.missingTargets[0].cell == (CellPos{11, 20}));
        CHECK(review.unreferenced == std::vector<int>{13});
        // Complete it.
        auto complete = worlds;
        complete.erase(13);
        complete.erase(95);
        complete.emplace(20, missionView);
        const auto clean = reviewDoors(first, complete);
        CHECK(clean.missingTargets.empty());
        CHECK(clean.unreferenced.empty());
        CHECK(reviewDoors(GameProfile::unknown, worlds).missingTargets.empty());
    }
    return test::finish("campaign_doors");
}
