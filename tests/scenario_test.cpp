// SPDX-License-Identifier: GPL-3.0-only
//
// World regression scenarios: the facts of a world, a recorded scenario
// holding for that world, and expectations failing after a change.
#include "world/scenario.hpp"

#include "world/world_probe.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <algorithm>

using namespace eggbert;
using namespace eggbert::test;

namespace {

long long fact(const std::vector<WorldFact> &facts, std::string_view name) {
    const auto found = std::ranges::find(facts, name, &WorldFact::name);
    return found == facts.end() ? -999 : found->value;
}

std::vector<WorldFact> factsOf(const Bytes &bytes, GameProfile profile) {
    return worldFacts(WorldView(bytes, probeWorld(bytes)), profile);
}

} // namespace

int main() {
    auto world = blankWorld(fxRevision13);
    put32(world, 212, 100); // scrolls horizontally only
    put16(world, 222, 3);
    put16(world, 224, 7);
    put32(world, 328, 194);
    put32(world, 332, 198);
    put16(world, fxCellOffset(fxForegroundOffset, 3, 4), 40);
    put16(world, fxCellOffset(fxForegroundOffset, 4, 4), 38);
    put16(world, fxCellOffset(fxRevision13.bigDecorOffset, 9, 9), 20);
    for (const std::size_t slot : {0u, 1u, 7u}) {
        put16(world, fxSlotOffset(fxRevision13, slot), slot == 7 ? 7 : 5);
        put16(world, fxSlotOffset(fxRevision13, slot) + 42, 10);
    }
    const auto first = GameProfile::speedyBlupi1;
    const auto facts = factsOf(world, first);
    CHECK(std::ranges::is_sorted(facts, {}, &WorldFact::name));
    CHECK_EQ(fact(facts, "revision.minor"), 3);
    CHECK_EQ(fact(facts, "file.bytes"), 50'468);
    CHECK_EQ(fact(facts, "header.music"), 3);
    CHECK_EQ(fact(facts, "header.region"), 7);
    CHECK_EQ(fact(facts, "header.scrolls.x"), 1);
    CHECK_EQ(fact(facts, "header.scrolls.y"), 0);
    CHECK_EQ(fact(facts, "start.1.x"), 194);
    CHECK_EQ(fact(facts, "start.1.y"), 198);
    CHECK_EQ(fact(facts, "foreground.occupied"), 2);
    CHECK_EQ(fact(facts, "bigdecor.occupied"), 1);
    CHECK_EQ(fact(facts, "objects.active"), 3);
    CHECK_EQ(fact(facts, "objects.free"), 197);
    CHECK_EQ(fact(facts, "objects.type.5"), 2);
    CHECK_EQ(fact(facts, "objects.type.7"), 1);
    CHECK_EQ(fact(facts, "doctor.errors"), 0);
    CHECK(fact(facts, "objects.type.9") == -999); // types that do not occur are not listed

    // A recorded scenario holds for the world it was recorded from.
    const auto recorded = recordScenario(facts);
    CHECK(recorded.starts_with("# "));
    CHECK(recorded.find("objects.type.5 = 2\n") != std::string::npos);
    {
        const auto result = checkScenario(recorded, facts);
        CHECK(result.has_value());
        CHECK_EQ(result->checked, facts.size());
        CHECK(result->failures.empty());
    }
    // After a change the affected expectations fail, each with its line.
    {
        auto changed = world;
        put16(changed, fxSlotOffset(fxRevision13, 1), 0); // one treasure removed
        put16(changed, 222, 77);                          // a music no game offers
        const auto result = checkScenario(recorded, factsOf(changed, first));
        CHECK(result.has_value());
        const auto mentions = [&](std::string_view text) {
            return std::ranges::any_of(result->failures, [&](const std::string &failure) {
                return failure.find(text) != std::string::npos;
            });
        };
        CHECK(mentions("objects.type.5 is 1, expected = 2."));
        CHECK(mentions("objects.active is 2, expected = 3."));
        CHECK(mentions("header.music is 77, expected = 3."));
        CHECK(mentions("doctor.warnings is "));
        CHECK(result->failures.front().starts_with("Line "));
        CHECK(!mentions("foreground.occupied"));
    }
    // Hand-written expectations: comparisons, comments, blank lines, and a
    // fact the world does not have counts as zero.
    {
        const auto result = checkScenario("# my world must keep this\r\n\r\n"
                                          "objects.type.5 >= 2\n"
                                          "  objects.type.5 <= 2  \n"
                                          "foreground.occupied>=3\n"
                                          "doctor.rule.OBJ-POOL-FULL = 0\n"
                                          "objects.type.99 <= 0\n",
                                          facts);
        CHECK(result.has_value());
        CHECK_EQ(result->checked, std::size_t{5});
        CHECK_EQ(result->failures.size(), std::size_t{1});
        CHECK_EQ(result->failures[0],
                 std::string("Line 5: foreground.occupied is 2, expected >= 3."));
    }
    // What is not a scenario is refused with its line.
    for (const auto *bad : {"objects.active\n", "objects.active = many\n", "= 3\n",
                            "two words = 3\n", "objects.active == 3\n", "objects.active = \n"}) {
        const auto result = checkScenario(std::string("# header\n") + bad, facts);
        CHECK(!result.has_value());
        CHECK(!result.has_value() && result.error().starts_with("Line 2: "));
    }
    CHECK(checkScenario("", facts)->checked == 0);
    // The game matters for what World Doctor says.
    {
        auto modern = world;
        put16(modern, fxCellOffset(fxForegroundOffset, 50, 50), 300); // a second-game tile
        CHECK(fact(factsOf(modern, GameProfile::speedyBlupi1), "doctor.warnings") >
              fact(factsOf(modern, GameProfile::speedyBlupi2), "doctor.warnings"));
    }
    return test::finish("scenario");
}
