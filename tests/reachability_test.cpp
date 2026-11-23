// SPDX-License-Identifier: GPL-3.0-only
//
// The reachability estimate: cells connected to a start through cells that
// are not fully solid, and the treasures and goals outside them. Tile 30 is
// fully solid in both games, tile 91 (water) is not.
#include "world/reachability.hpp"

#include "world/doctor.hpp"
#include "world/world_probe.hpp"

#include "check.hpp"
#include "fixture.hpp"

using namespace eggbert;
using namespace eggbert::test;

namespace {

constexpr auto first = GameProfile::speedyBlupi1;
constexpr auto second = GameProfile::speedyBlupi2;

void tile(Bytes &bytes, std::size_t x, std::size_t y, int value) {
    put16(bytes, fxCellOffset(fxForegroundOffset, x, y), value);
}

void start(Bytes &bytes, std::size_t x, std::size_t y) {
    for (std::size_t player = 0; player < 4; ++player) {
        put32(bytes, 328 + 8 * player, static_cast<int>(x) * 64 + 2);
        put32(bytes, 332 + 8 * player, static_cast<int>(y) * 64 + 6);
    }
}

void object(Bytes &bytes, std::size_t slot, int type, std::size_t x, std::size_t y) {
    const auto at = fxSlotOffset(fxRevision13, slot);
    put16(bytes, at, type);
    put32(bytes, at + 12, static_cast<int>(x) * 64 + 2);
    put32(bytes, at + 16, static_cast<int>(y) * 64 + 4);
}

// A closed box of solid tiles with its inside free.
void box(Bytes &bytes, std::size_t left, std::size_t top, std::size_t size, int value = 30) {
    for (std::size_t offset = 0; offset < size; ++offset) {
        tile(bytes, left + offset, top, value);
        tile(bytes, left + offset, top + size - 1, value);
        tile(bytes, left, top + offset, value);
        tile(bytes, left + size - 1, top + offset, value);
    }
}

ReachabilityEstimate estimate(const Bytes &bytes, GameProfile profile) {
    return estimateReachability(WorldView(bytes, probeWorld(bytes)), profile);
}

} // namespace

int main() {
    // The solid classes.
    CHECK(tileFullySolid(first, 0) && tileFullySolid(first, 30) && tileFullySolid(first, 234));
    CHECK(!tileFullySolid(first, -1) && !tileFullySolid(first, 15) && !tileFullySolid(first, 91));
    CHECK(!tileFullySolid(first, 235) && !tileFullySolid(first, 246));
    CHECK(tileFullySolid(second, 246) && tileFullySolid(second, 397));
    CHECK(!tileFullySolid(second, 330) && !tileFullySolid(second, 440));
    CHECK(tileFullySolid(second, 30));
    CHECK(!tileFullySolid(GameProfile::unknown, 30));

    auto world = blankWorld(fxRevision13);
    start(world, 10, 10);
    object(world, 0, 5, 20, 10);  // a treasure in the open
    object(world, 1, 7, 40, 40);  // the goal in the open
    box(world, 60, 60, 5);
    object(world, 2, 5, 62, 62);  // a treasure inside the closed box
    object(world, 3, 12, 62, 61); // a crate in there too: not a treasure or goal
    {
        const auto result = estimate(world, first);
        CHECK(result.made);
        CHECK(result.cutOffSlots == std::vector<std::size_t>{2});
        // Everything but the box's 16 wall cells and its 9 inner cells.
        CHECK_EQ(result.connectedCells, std::size_t{10'000 - 16 - 9});
    }
    // One gap in the wall connects the inside.
    {
        auto open = world;
        tile(open, 60, 62, -1);
        CHECK(estimate(open, first).cutOffSlots.empty());
        // A tile that is not fully solid is a gap too (water).
        auto wet = world;
        tile(wet, 60, 62, 91);
        CHECK(estimate(wet, first).cutOffSlots.empty());
    }
    // An object standing in a solid cell is cut off; so is a goal.
    {
        auto buried = world;
        tile(buried, 20, 10, 30);
        object(buried, 4, 7, 62, 63);
        CHECK(estimate(buried, first).cutOffSlots == (std::vector<std::size_t>{0, 4, 2}) ||
              estimate(buried, first).cutOffSlots == (std::vector<std::size_t>{0, 2, 4}));
    }
    // Any of the four starts counts.
    {
        auto inside = world;
        put32(inside, 328 + 8 * 3, 62 * 64 + 2);
        put32(inside, 332 + 8 * 3, 63 * 64 + 6);
        CHECK(estimate(inside, first).cutOffSlots.empty());
    }
    // A second-game solid tile is no wall for the first game's estimate.
    {
        auto modern = blankWorld(fxRevision13);
        start(modern, 10, 10);
        box(modern, 60, 60, 5, 246);
        object(modern, 0, 5, 62, 62);
        CHECK(estimate(modern, second).cutOffSlots == std::vector<std::size_t>{0});
        CHECK(estimate(modern, first).cutOffSlots.empty());
        // Teleporters or key doors: no estimate for the second game.
        for (const int special : {330, 333, 334, 336}) {
            auto with = modern;
            tile(with, 5, 5, special);
            const auto result = estimate(with, second);
            CHECK(!result.made && result.reason == "the world has teleporters or key doors");
            CHECK(result.cutOffSlots.empty());
            CHECK(estimate(with, first).made); // no such thing in the first game
        }
    }
    // When no estimate is made, and why.
    {
        CHECK(estimate(world, GameProfile::unknown).reason == "no game is chosen");
        auto walled = world;
        tile(walled, 10, 10, 30);
        CHECK(estimate(walled, first).reason == "no start lies in a free cell");
        auto outside = world;
        for (std::size_t player = 0; player < 4; ++player) {
            put32(outside, 328 + 8 * player, -5000);
        }
        CHECK(!estimate(outside, first).made);
        auto cut = world;
        cut.resize(30'000);
        CHECK(estimate(cut, first).reason == "the world is incomplete");
    }

    // World Doctor reports it as a note marked as an estimate, per object,
    // and stays silent when no estimate is made.
    {
        const auto report = runDoctor(WorldView(world, probeWorld(world)), first);
        CHECK_EQ(report.countRule("REACH-CUT-OFF"), std::size_t{1});
        const auto *finding = report.first("REACH-CUT-OFF");
        CHECK(finding != nullptr && finding->severity == Severity::note);
        CHECK(finding != nullptr && finding->confidence == Confidence::estimate);
        CHECK(finding != nullptr && finding->slot == std::size_t{2});
        CHECK(finding != nullptr && finding->message.find("treasure in slot 2") != std::string::npos);
        CHECK(finding != nullptr && finding->range.offset == fxSlotOffset(fxRevision13, 2));
        CHECK_EQ(report.count(Severity::error), std::size_t{0});
        CHECK_EQ(runDoctor(WorldView(world, probeWorld(world)), GameProfile::unknown)
                     .countRule("REACH-CUT-OFF"),
                 std::size_t{0});
    }
    return test::finish("reachability");
}
