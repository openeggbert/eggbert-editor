// SPDX-License-Identifier: GPL-3.0-only
//
// Names and numbers of an installation's data directory.
#include "world/installation.hpp"

#include "check.hpp"

using namespace eggbert;

int main() {
    using K = DataFileKind;
    // The five numbered patterns and the three fixed names, in any case.
    CHECK(classifyDataFileName("world001.blp") == (DataFileName{K::officialWorld, 0, 1}));
    CHECK(classifyDataFileName("WORLD212.BLP") == (DataFileName{K::officialWorld, 0, 212}));
    CHECK(classifyDataFileName("u003-017.blp") == (DataFileName{K::userWorld, 3, 17}));
    CHECK(classifyDataFileName("U008-999.BLP") == (DataFileName{K::userWorld, 8, 999}));
    CHECK(classifyDataFileName("s001-005.blp") == (DataFileName{K::savedGame, 1, 5}));
    CHECK(classifyDataFileName("info002.blp") == (DataFileName{K::gamerInfo, 2, 0}));
    CHECK(classifyDataFileName("demo300.blp") == (DataFileName{K::demoRecording, 0, 300}));
    CHECK(classifyDataFileName("Config.def").kind == K::gameState);
    CHECK(classifyDataFileName("TIME.BLP").kind == K::gameState);
    CHECK(classifyDataFileName("bench.blp").kind == K::gameState);
    // Near misses are not guessed at.
    for (const auto *name : {"world01.blp", "world0001.blp", "world001.bak", "worldabc.blp",
                             "world001.blp.tmp", "u003_017.blp", "u03-017.blp", "u003-17.blp",
                             "x003-017.blp", "info02.blp", "demo.blp", "", "world001",
                             "myworld001.blp", "decor000.blp"}) {
        CHECK(classifyDataFileName(name) == DataFileName{});
    }

    // Slots.
    CHECK(validWorldSlot({true, 1, 1}) && validWorldSlot({true, 8, 20}));
    CHECK(!validWorldSlot({true, 0, 1}) && !validWorldSlot({true, 9, 1}));
    CHECK(!validWorldSlot({true, 1, 0}) && !validWorldSlot({true, 1, 21}));
    CHECK(!validWorldSlot({true, 1, scratchWorldNumber})); // the game's own scratch file
    CHECK(validWorldSlot({false, 1, 1}) && validWorldSlot({false, 1, 999}));
    CHECK(!validWorldSlot({false, 1, 0}) && !validWorldSlot({false, 1, 1000}));
    CHECK_EQ(worldSlotFileName({true, 1, 7}), std::string("u001-007.blp"));
    CHECK_EQ(worldSlotFileName({true, 8, 20}), std::string("u008-020.blp"));
    CHECK_EQ(worldSlotFileName({false, 1, 12}), std::string("world012.blp"));
    CHECK(worldSlotFileName({true, 9, 1}).empty());
    // A name and its slot agree.
    for (const WorldSlot slot : {WorldSlot{true, 2, 5}, WorldSlot{false, 1, 140}}) {
        const auto back = worldSlotOf(classifyDataFileName(worldSlotFileName(slot)));
        CHECK(back.has_value() && back->user == slot.user && back->number == slot.number);
        CHECK(!slot.user || (back && back->gamer == slot.gamer));
    }
    CHECK(!worldSlotOf(classifyDataFileName("u001-999.blp")).has_value());
    CHECK(!worldSlotOf(classifyDataFileName("info001.blp")).has_value());
    CHECK(!worldSlotOf(classifyDataFileName("world000.blp")).has_value());

    // Roles of official numbers.
    const auto one = GameProfile::speedyBlupi1;
    const auto two = GameProfile::speedyBlupi2;
    CHECK(officialWorldRole(one, 1) == WorldRole::start);
    CHECK(officialWorldRole(one, 10) == WorldRole::hub);
    CHECK(officialWorldRole(one, 80) == WorldRole::hub);
    CHECK(officialWorldRole(one, 11) == WorldRole::mission);
    CHECK(officialWorldRole(one, 88) == WorldRole::mission);
    CHECK(officialWorldRole(one, 19) == WorldRole::other); // a group has eight missions
    CHECK(officialWorldRole(one, 90) == WorldRole::other);
    CHECK(officialWorldRole(one, 5) == WorldRole::other);
    CHECK(officialWorldRole(one, 99) == WorldRole::finale);
    CHECK(officialWorldRole(one, 101) == WorldRole::multiplayer);
    CHECK(officialWorldRole(one, 110) == WorldRole::multiplayer);
    CHECK(officialWorldRole(one, 111) == WorldRole::other);
    CHECK(officialWorldRole(one, 200) == WorldRole::demo);
    CHECK(officialWorldRole(two, 90) == WorldRole::hub);
    CHECK(officialWorldRole(two, 140) == WorldRole::hub);
    CHECK(officialWorldRole(two, 148) == WorldRole::mission);
    CHECK(officialWorldRole(two, 150) == WorldRole::other);
    CHECK(officialWorldRole(two, 99) == WorldRole::other);
    CHECK(officialWorldRole(two, 199) == WorldRole::finale);
    CHECK(officialWorldRole(two, 101) == WorldRole::mission);
    CHECK(officialWorldRole(two, 201) == WorldRole::multiplayer);
    CHECK(officialWorldRole(two, 212) == WorldRole::multiplayer);
    CHECK(officialWorldRole(two, 213) == WorldRole::other);
    CHECK(officialWorldRole(two, 300) == WorldRole::demo);
    CHECK(officialWorldRole(GameProfile::unknown, 1) == WorldRole::other);
    CHECK(officialWorldRole(one, 0) == WorldRole::other);
    CHECK(worldRoleName(WorldRole::hub) == "group hub");
    CHECK(worldRoleName(WorldRole::other).empty());
    return test::finish("installation");
}
