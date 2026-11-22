// SPDX-License-Identifier: GPL-3.0-only
//
// Display-only animation: which picture a tile shows at a tick, and how an
// object moves and changes its picture, as the games do while their editor
// is open. Expected numbers are written out from the recorded rules.

#include "world/animation.hpp"

#include "world/object_animation_data.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <vector>

using namespace eggbert;
using namespace eggbert::test;

namespace {

constexpr auto first = GameProfile::speedyBlupi1;
constexpr auto second = GameProfile::speedyBlupi2;
constexpr auto fg = SectionKind::foreground;

int icon(GameProfile profile, SectionKind layer, int stored, std::size_t x, std::size_t y,
         std::uint32_t tick) {
    return tileFrame(profile, layer, static_cast<std::int16_t>(stored), x, y, tick).icon;
}

struct Mover {
    int type = 4;
    Point start;
    Point end;
    int advance = 1;
    int recede = 1;
    int waitStart = 0;
    int waitEnd = 0;
    int channel = 10;
    int icon = 57;
};

void putObject(Bytes &bytes, std::size_t slot, const Mover &mover) {
    const auto base = fxSlotOffset(fxRevision13, slot);
    put16(bytes, base, mover.type);
    put16(bytes, base + 2, mover.advance);
    put16(bytes, base + 4, mover.recede);
    put16(bytes, base + 6, mover.waitStart);
    put16(bytes, base + 8, mover.waitEnd);
    put32(bytes, base + 12, mover.start.x);
    put32(bytes, base + 16, mover.start.y);
    put32(bytes, base + 20, mover.end.x);
    put32(bytes, base + 24, mover.end.y);
    put32(bytes, base + 28, mover.start.x);
    put32(bytes, base + 32, mover.start.y);
    put16(bytes, base + 36, 1);
    put16(bytes, base + 42, mover.channel);
    put16(bytes, base + 44, mover.icon);
}

} // namespace

int main() {
    CHECK_EQ(animationTicksPerSecond, 20);

    // --- Tiles -----------------------------------------------------------
    // Lava: a new frame every third tick, shifted from cell to cell
    // (x * 13 + y * 7); five frames in the first game, eight in the second.
    CHECK_EQ(icon(first, fg, 68, 0, 0, 0), 68);
    CHECK_EQ(icon(first, fg, 68, 0, 0, 2), 68);
    CHECK_EQ(icon(first, fg, 68, 0, 0, 3), 69);
    CHECK_EQ(icon(first, fg, 68, 0, 0, 12), 72);
    CHECK_EQ(icon(first, fg, 68, 0, 0, 15), 68);
    CHECK_EQ(icon(first, fg, 68, 1, 0, 0), 71);  // 13 % 5 = 3
    CHECK_EQ(icon(first, fg, 68, 0, 1, 0), 70);  // 7 % 5 = 2
    CHECK_EQ(icon(second, fg, 68, 0, 0, 15), 71); // 68 69 70 71 72 71 70 69
    CHECK_EQ(icon(second, fg, 68, 0, 0, 24), 68);
    // Water surface and body.
    CHECK_EQ(icon(first, fg, 92, 0, 0, 3), 93);
    CHECK_EQ(icon(first, fg, 91, 0, 0, 3), 96);
    CHECK_EQ(icon(first, fg, 91, 0, 0, 9), 98);
    CHECK_EQ(icon(first, fg, 91, 0, 0, 12), 91);
    // Wind: the three numbers after its head, one per tick; fans: two
    // numbers after the head, one every second tick. No shift per cell.
    CHECK_EQ(icon(first, fg, 110, 5, 9, 1), 111);
    CHECK_EQ(icon(first, fg, 110, 5, 9, 3), 113);
    CHECK_EQ(icon(first, fg, 110, 5, 9, 4), 110);
    CHECK_EQ(icon(first, fg, 122, 0, 0, 2), 124);
    CHECK_EQ(icon(first, fg, 126, 7, 7, 1), 126);
    CHECK_EQ(icon(first, fg, 126, 7, 7, 2), 127);
    CHECK_EQ(icon(first, fg, 126, 7, 7, 5), 128);
    CHECK_EQ(icon(first, fg, 126, 7, 7, 6), 126);
    CHECK_EQ(icon(first, fg, 135, 0, 0, 4), 137);
    // The frames in between are not heads and stay as stored.
    CHECK_EQ(icon(first, fg, 111, 0, 0, 7), 111);
    CHECK(!tileFrame(first, fg, 111, 0, 0, 7).animated);
    // Spring: 209 210 211 212 213 212 211 210, every second tick, shifted
    // by seven per column.
    CHECK_EQ(icon(first, fg, 211, 0, 0, 0), 209);
    CHECK_EQ(icon(first, fg, 211, 0, 0, 2), 210);
    CHECK_EQ(icon(first, fg, 211, 0, 0, 8), 213);
    CHECK_EQ(icon(first, fg, 211, 1, 0, 0), 210);
    CHECK_EQ(icon(first, fg, 211, 0, 5, 0), 209); // the row does not matter
    // bigDecor: only the marine plant, 203 to 208 and back.
    CHECK_EQ(icon(first, SectionKind::bigDecor, 203, 0, 0, 3), 204);
    CHECK_EQ(icon(first, SectionKind::bigDecor, 203, 0, 0, 15), 208);
    CHECK_EQ(icon(first, SectionKind::bigDecor, 203, 0, 0, 18), 207);
    CHECK_EQ(icon(first, SectionKind::bigDecor, 20, 0, 0, 18), 20);
    CHECK_EQ(icon(first, SectionKind::bigDecor, 68, 0, 0, 3), 68); // lava is foreground only
    // Ordinary tiles never change.
    for (const std::uint32_t tick : {0u, 1u, 77u, 40'000u}) {
        CHECK_EQ(icon(first, fg, 9, 3, 70, tick), 9);
        CHECK_EQ(icon(second, fg, 153, 3, 70, tick), 153);
    }
    // Second game only: slime trap, drip, crusher, saw, vanishing block and
    // the lightning beam.
    CHECK_EQ(icon(second, fg, 373, 0, 0, 0), 374);
    CHECK_EQ(icon(second, fg, 373, 0, 0, 8), 373);
    CHECK_EQ(icon(second, fg, 373, 0, 0, 12), 347);
    CHECK_EQ(icon(first, fg, 373, 0, 0, 12), 373); // not a first-game animation
    CHECK_EQ(icon(second, fg, 404, 0, 0, 0), 410);
    CHECK_EQ(icon(second, fg, 404, 0, 0, 2), 409);
    CHECK_EQ(icon(second, fg, 404, 0, 0, 6), -1); // blank frames between drops
    CHECK_EQ(icon(second, fg, 410, 0, 0, 60), 404);
    CHECK_EQ(tileFrame(second, fg, 404, 0, 0, 0).offsetY, -9);
    CHECK_EQ(tileFrame(second, fg, 410, 0, 0, 0).offsetY, -9);
    CHECK_EQ(icon(second, fg, 317, 4, 4, 6), 318);
    CHECK_EQ(icon(second, fg, 317, 4, 4, 27), 323);
    CHECK_EQ(icon(second, fg, 378, 0, 0, 5), 383);
    CHECK_EQ(icon(second, fg, 378, 0, 0, 6), 378);
    CHECK_EQ(icon(second, fg, 324, 0, 0, 32), 324);
    CHECK_EQ(icon(second, fg, 324, 0, 0, 72), -1);
    CHECK_EQ(icon(second, fg, 305, 0, 0, 0), 305);
    CHECK_EQ(icon(second, fg, 305, 0, 0, 2), 307);
    CHECK_EQ(icon(second, fg, 305, 0, 0, 3), -1); // the beam flickers: even ticks only
    // Draw order flags do not depend on the tick.
    CHECK(tileFrame(first, fg, 68, 0, 0, 5).late);
    CHECK(stillTileFrame(first, fg, 68).late);
    CHECK(stillTileFrame(first, fg, 91).late && stillTileFrame(first, fg, 126).late);
    CHECK(!stillTileFrame(first, fg, 211).late);
    CHECK(!stillTileFrame(first, fg, 9).late);
    CHECK(!stillTileFrame(first, fg, 373).late);
    CHECK(stillTileFrame(second, fg, 373).late && stillTileFrame(second, fg, 305).late);
    CHECK(stillTileFrame(second, fg, 384).early && stillTileFrame(second, fg, 385).early);
    CHECK(!stillTileFrame(first, fg, 384).early);
    CHECK_EQ(stillTileFrame(second, fg, 404).icon, 404);
    CHECK(!stillTileFrame(second, fg, 404).animated);
    // Without a game nothing is animated or reordered.
    CHECK((tileFrame(GameProfile::unknown, fg, 68, 1, 1, 9) == TileFrame{68, 0, false, false, false}));

    // --- Objects ---------------------------------------------------------
    auto bytes = blankWorld(fxRevision13);
    // 0: a bulldozer going three cells to the right: 20 + 90 + 20 + 90.
    putObject(bytes, 0, {4, {640, 640}, {832, 640}, 90, 90, 20, 20, 10, 57});
    // 1: the same to the left (negative distance).
    putObject(bytes, 1, {4, {832, 1280}, {640, 1280}, 90, 90, 20, 20, 10, 57});
    // 2: a treasure chest, 3: a bomb on the floor, 4: a key - none moves.
    putObject(bytes, 2, {5, {514, 1092}, {514, 1092}, 1, 1, 0, 0, 10, 0});
    putObject(bytes, 3, {2, {770, 1092}, {770, 1092}, 1, 1, 0, 0, 10, 12});
    putObject(bytes, 4, {21, {642, 772}, {642, 772}, 1, 1, 0, 0, 10, 125});
    // 5: a lift with a zero duration, 6: a lift going down.
    putObject(bytes, 5, {1, {1920, 640}, {1920, 320}, 0, 0, 0, 0, 1, 153});
    putObject(bytes, 6, {1, {2560, 320}, {2560, 448}, 4, 8, 1, 2, 1, 153});
    // 7: a type no rule knows, with stale run-time values stored.
    putObject(bytes, 7, {123, {64, 64}, {64, 64}, 1, 1, 0, 0, 10, 99});
    {
        const auto base = fxSlotOffset(fxRevision13, 0);
        put32(bytes, base + 28, 777); // stored posCurrent in the middle of nowhere
        put16(bytes, base + 36, 3);   // stored step, time and phase
        put16(bytes, base + 38, 7);
        put16(bytes, base + 40, 99);
    }
    const auto snapshot = bytes;
    const WorldView world(bytes);

    ObjectAnimation animation;
    animation.sync(world, first);
    CHECK_EQ(animation.poses().size(), std::size_t{200});
    CHECK_EQ(animation.tick(), 0u);
    // Like the games' editors on entry: every object waits at its start,
    // whatever run-time state the file holds.
    CHECK((animation.poses()[0] == ObjectPose{{640, 640}, 10, 57}));
    CHECK_EQ(animation.step(0), 1);
    CHECK_EQ(animation.time(0), 0);

    const auto at = [&](std::size_t slot) { return animation.poses()[slot].position; };
    // Waiting at the start for 20 ticks, then one tick to set off.
    animation.advance(20);
    CHECK((at(0) == Point{640, 640}));
    CHECK_EQ(animation.step(0), 1);
    CHECK_EQ(animation.time(0), 20);
    animation.advance(1);
    CHECK_EQ(animation.step(0), 2);
    CHECK_EQ(animation.time(0), 0);
    // Advancing: (end - start) * time / duration + start, truncated.
    animation.advance(1);
    CHECK((at(0) == Point{642, 640}));  // 192 * 1 / 90 = 2
    CHECK((at(1) == Point{830, 1280})); // -192 * 1 / 90 = -2 (towards zero)
    animation.advance(44);
    CHECK((at(0) == Point{736, 640})); // 192 * 45 / 90
    animation.advance(45);
    CHECK((at(0) == Point{832, 640}));
    CHECK((at(1) == Point{640, 1280}));
    CHECK_EQ(animation.step(0), 2); // arrival is noticed on the next tick
    animation.advance(1);
    CHECK_EQ(animation.step(0), 3);
    CHECK_EQ(animation.time(0), 0);
    animation.advance(21);
    CHECK_EQ(animation.step(0), 4);
    CHECK((at(0) == Point{832, 640}));
    animation.advance(45);
    CHECK((at(0) == Point{736, 640}));
    animation.advance(45);
    CHECK((at(0) == Point{640, 640}));
    animation.advance(1);
    CHECK_EQ(animation.step(0), 1);
    CHECK_EQ(animation.time(0), 0);
    // The whole cycle is the four durations plus four: back where it began.
    CHECK_EQ(animation.tick(), 224u);
    animation.advance(224);
    CHECK((at(0) == Point{640, 640}));
    CHECK_EQ(animation.step(0), 1);
    CHECK_EQ(animation.time(0), 0);

    // Pictures. A chest opens and closes: 0..11 and back, every third tick.
    animation.restart();
    CHECK_EQ(animation.tick(), 0u);
    CHECK_EQ(animation.poses()[2].icon, 0);
    animation.advance(4); // the last step saw phase 3
    CHECK_EQ(animation.poses()[2].icon, 1);
    CHECK_EQ(animation.poses()[3].icon, 13);  // bomb: 12 + phase / 2 % 9
    CHECK_EQ(animation.poses()[4].icon, 123); // key: 122 123 124 ...
    // A bulldozer waiting at the start of a rightward path turns with the
    // counter of its wait (its first frames are 58 and three times 59).
    CHECK_EQ(animation.poses()[0].channel, 10);
    const auto expectedIcon = [&](std::size_t slot, int type, int step, int direction,
                                  int counter) {
        for (const auto &rule : objectIconRules(first)) {
            if (rule.type == type && rule.step == step && rule.direction == direction) {
                return static_cast<int>(
                    rule.frames[static_cast<std::size_t>(counter / rule.divisor) %
                                rule.frames.size()]);
            }
        }
        return static_cast<int>(animation.poses()[slot].icon);
    };
    CHECK_EQ(animation.poses()[0].icon, expectedIcon(0, 4, 1, 2, 4));
    CHECK_EQ(animation.poses()[1].icon, 60); // leftward path, time 4: 58 59 59 59 60
    animation.advance(30);                   // tick 34: chest phase 33, both dozers advancing
    CHECK_EQ(animation.poses()[2].icon, 11);
    CHECK_EQ(animation.step(0), 2);
    CHECK_EQ(animation.poses()[0].icon, expectedIcon(0, 4, 2, 2, animation.time(0)));
    CHECK_EQ(animation.poses()[1].icon, expectedIcon(1, 4, 2, 1, animation.time(1)));
    CHECK(animation.poses()[0].icon != animation.poses()[1].icon); // they face opposite ways
    animation.advance(3); // chest phase 36: on its way back
    CHECK_EQ(animation.poses()[2].icon, 10);
    // Objects that do not move stay where they are; a lift keeps its picture.
    CHECK((at(2) == Point{514, 1092}));
    CHECK((at(3) == Point{770, 1092}));
    CHECK_EQ(animation.poses()[6].icon, 153);
    CHECK_EQ(animation.poses()[6].channel, 1);
    // A type without a rule keeps the stored picture; it does not move.
    CHECK((animation.poses()[7] == ObjectPose{{64, 64}, 10, 99}));
    // A zero duration never leaves; nothing breaks however long it runs.
    animation.advance(70'000);
    CHECK((at(5) == Point{1920, 640}));
    CHECK_EQ(animation.step(5), 2);
    // The lift going down: 1 + 4 + 2 + 8 + 4 = 19 ticks per cycle.
    animation.restart();
    animation.advance(4);
    CHECK((at(6) == Point{2560, 384})); // waited 1, set off, two of four steps
    animation.advance(15);
    CHECK((at(6) == Point{2560, 320}));
    CHECK_EQ(animation.step(6), 1);

    // Edits: an unchanged record keeps running, a changed one starts again.
    animation.restart();
    animation.advance(60);
    const auto dozerRunning = at(0);
    CHECK(dozerRunning.x > 640);
    put16(bytes, fxSlotOffset(fxRevision13, 1) + 8, 5); // another wait for slot 1 only
    const WorldView edited(bytes);
    animation.sync(edited, first);
    CHECK((at(0) == dozerRunning));
    CHECK((at(1) == Point{832, 1280}));
    CHECK_EQ(animation.step(1), 1);
    CHECK_EQ(animation.tick(), 60u);
    // A freed slot is left alone.
    put16(bytes, fxSlotOffset(fxRevision13, 0), 0);
    animation.sync(WorldView(bytes), first);
    animation.advance(50);
    CHECK((at(0) == Point{640, 640}));
    put16(bytes, fxSlotOffset(fxRevision13, 0), 4);
    put16(bytes, fxSlotOffset(fxRevision13, 1) + 8, 20);
    // Another game restarts everything; without a game objects still move
    // but keep their stored pictures.
    animation.sync(WorldView(bytes), second);
    CHECK_EQ(animation.tick(), 0u);
    animation.sync(WorldView(bytes), GameProfile::unknown);
    animation.advance(66);
    CHECK((at(0) == Point{736, 640}));
    CHECK_EQ(animation.poses()[0].icon, 57);
    CHECK_EQ(animation.poses()[2].icon, 0);

    // Rules exist only for a known game, and only with usable frames.
    CHECK(objectIconRules(GameProfile::unknown).empty());
    for (const auto profile : {first, second}) {
        CHECK(!objectIconRules(profile).empty());
        for (const auto &rule : objectIconRules(profile)) {
            CHECK(rule.type > 0);
            CHECK(rule.step >= 0 && rule.step <= 4);
            CHECK(rule.direction >= 0 && rule.direction <= 2);
            CHECK(rule.divisor >= 1);
            CHECK(!rule.frames.empty());
            CHECK(!rule.provenance.empty());
        }
    }
    // Nothing of all this wrote into the world's bytes.
    CHECK(bytes == snapshot);
    return test::finish("animation");
}
