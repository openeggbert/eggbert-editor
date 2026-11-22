// SPDX-License-Identifier: GPL-3.0-only
#include "world/world_stats.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <algorithm>

using namespace eggbert;
using namespace eggbert::test;

int main() {
    // Blank world: everything empty and free.
    {
        const auto bytes = blankWorld(fxRevision13, 4);
        const auto stats = computeWorldStats(WorldView(bytes));
        CHECK_EQ(stats.fileBytes, std::size_t{50'472});
        CHECK_EQ(stats.headerMajor.value(), std::int16_t{1});
        CHECK_EQ(stats.headerMinor.value(), std::int16_t{3});
        CHECK(stats.revision == FormatRevision::v1_3);
        CHECK_EQ(stats.surplusBytes, std::size_t{4});
        CHECK(stats.headerPresent);
        CHECK(stats.foreground.present && stats.bigDecor.present);
        CHECK_EQ(stats.foreground.occupied, std::size_t{0});
        CHECK_EQ(stats.foreground.empty, std::size_t{10'000});
        CHECK(stats.foreground.valueCounts.empty());
        CHECK(!stats.foreground.bounds.has_value());
        CHECK(!stats.usedBounds.has_value());
        CHECK(stats.objects.present);
        CHECK_EQ(stats.objects.stored, std::size_t{200});
        CHECK_EQ(stats.objects.active, std::size_t{0});
        CHECK_EQ(stats.objects.free, std::size_t{200});
        CHECK_EQ(stats.objects.freeWithOpaqueBytes, std::size_t{0});
        CHECK(stats.objects.byType.empty());
    }

    // A hand-built scene.
    auto bytes = blankWorld(fxRevision12);
    const auto fg = [&](std::size_t x, std::size_t y, int value) {
        put16(bytes, fxCellOffset(fxForegroundOffset, x, y), value);
    };
    const auto big = [&](std::size_t x, std::size_t y, int value) {
        put16(bytes, fxCellOffset(fxRevision12.bigDecorOffset, x, y), value);
    };
    fg(3, 70, 5);
    fg(4, 70, 5);
    fg(90, 2, 5);
    fg(10, 10, 0);       // zero is a stored tile value, not empty
    fg(11, 10, -2);      // below -1 is still "not empty"
    fg(12, 10, 32767);
    fg(13, 10, -32768);
    big(1, 98, 20);
    big(60, 60, 21);
    for (int index = 0; index < 4; ++index) {
        put32(bytes, 328 + 8 * index, 100 + index);
        put32(bytes, 332 + 8 * index, -200 - index);
        put32(bytes, 360 + 4 * index, index + 1);
    }
    const auto object = [&](std::size_t slot, int type, int channel, int icon, int startX,
                            int endX) {
        const auto base = fxSlotOffset(fxRevision12, slot);
        put16(bytes, base + 0, type);
        put32(bytes, base + 12, startX);
        put32(bytes, base + 20, endX);
        put16(bytes, base + 42, channel);
        put16(bytes, base + 44, icon);
    };
    object(0, 12, 1, 32, 640, 640);
    object(3, 12, 1, 33, 640, 960);  // has a path
    object(199, 5, 10, 0, 64, 64);
    object(50, 0, 10, 99, 5, 6);     // free slot carrying opaque bytes
    const auto snapshot = bytes;

    const WorldView world(bytes);
    const auto stats = computeWorldStats(world);
    CHECK(stats.revision == FormatRevision::v1_2);
    CHECK_EQ(stats.foreground.occupied, std::size_t{7});
    CHECK_EQ(stats.foreground.empty, std::size_t{9'993});
    CHECK_EQ(stats.foreground.valueCounts.size(), std::size_t{5});
    CHECK_EQ(stats.foreground.valueCounts.at(5), std::size_t{3});
    CHECK_EQ(stats.foreground.valueCounts.at(0), std::size_t{1});
    CHECK_EQ(stats.foreground.valueCounts.at(-2), std::size_t{1});
    CHECK_EQ(stats.foreground.valueCounts.at(32767), std::size_t{1});
    CHECK_EQ(stats.foreground.valueCounts.at(-32768), std::size_t{1});
    CHECK(!stats.foreground.valueCounts.contains(-1));
    CHECK((stats.foreground.bounds.value() == CellBounds{3, 2, 90, 70}));
    CHECK_EQ(stats.bigDecor.occupied, std::size_t{2});
    CHECK((stats.bigDecor.bounds.value() == CellBounds{1, 60, 60, 98}));
    CHECK((stats.usedBounds.value() == CellBounds{1, 2, 90, 98}));
    for (int index = 0; index < 4; ++index) {
        CHECK((stats.startPositions[index] == Point{100 + index, -200 - index}));
        CHECK_EQ(stats.startDirections[index], index + 1);
    }
    CHECK_EQ(stats.objects.stored, std::size_t{200});
    CHECK_EQ(stats.objects.active, std::size_t{3});
    CHECK_EQ(stats.objects.free, std::size_t{197});
    CHECK_EQ(stats.objects.freeWithOpaqueBytes, std::size_t{1});
    CHECK_EQ(stats.objects.withPath, std::size_t{1});
    CHECK_EQ(stats.objects.byType.at(12), std::size_t{2});
    CHECK_EQ(stats.objects.byType.at(5), std::size_t{1});
    CHECK(!stats.objects.byType.contains(0));
    CHECK_EQ(stats.objects.byChannel.at(1), std::size_t{2});
    CHECK_EQ(stats.objects.byChannel.at(10), std::size_t{1}); // the free slot is not counted
    CHECK_EQ(stats.objects.byIcon.at(32), std::size_t{1});
    CHECK(!stats.objects.byIcon.contains(99));

    // Searches.
    const auto fives = findCells(world, SectionKind::foreground, 5);
    CHECK_EQ(fives.size(), std::size_t{3});
    CHECK((fives[0] == CellPos{3, 70}));
    CHECK((fives[1] == CellPos{4, 70}));
    CHECK((fives[2] == CellPos{90, 2}));
    CHECK_EQ(findCells(world, SectionKind::foreground, -1).size(), std::size_t{9'993});
    CHECK_EQ(findCells(world, SectionKind::bigDecor, 21).size(), std::size_t{1});
    CHECK(findCells(world, SectionKind::bigDecor, 5).empty());
    CHECK(findCells(world, SectionKind::objects, 5).empty());
    CHECK((findActiveSlots(world, ObjectKey::type, 12) == std::vector<std::size_t>{0, 3}));
    CHECK((findActiveSlots(world, ObjectKey::channel, 10) == std::vector<std::size_t>{199}));
    CHECK((findActiveSlots(world, ObjectKey::icon, 33) == std::vector<std::size_t>{3}));
    CHECK(findActiveSlots(world, ObjectKey::type, 0).empty());
    CHECK(bytes == snapshot);

    // A completely filled layer.
    {
        auto full = blankWorld(fxRevision13);
        for (std::size_t x = 0; x < 100; ++x) {
            for (std::size_t y = 0; y < 100; ++y) {
                put16(full, fxCellOffset(fxForegroundOffset, x, y), 7);
            }
        }
        const auto fullStats = computeWorldStats(WorldView(full));
        CHECK_EQ(fullStats.foreground.occupied, std::size_t{10'000});
        CHECK_EQ(fullStats.foreground.empty, std::size_t{0});
        CHECK((fullStats.foreground.bounds.value() == CellBounds{0, 0, 99, 99}));
    }
    // Revision 1.0: no bigDecor, 100 slots.
    {
        auto old = blankWorld(fxRevision10);
        put16(old, fxSlotOffset(fxRevision10, 99), 9);
        const auto oldStats = computeWorldStats(WorldView(old));
        CHECK(oldStats.foreground.present);
        CHECK(!oldStats.bigDecor.present);
        CHECK_EQ(oldStats.objects.stored, std::size_t{100});
        CHECK_EQ(oldStats.objects.active, std::size_t{1});
        CHECK(findCells(WorldView(old), SectionKind::bigDecor, -1).empty());
    }
    // Truncated inside the object pool: layers counted, objects absent.
    {
        auto cut = bytes;
        cut.resize(45'000);
        const auto cutStats = computeWorldStats(WorldView(cut));
        CHECK(cutStats.headerPresent);
        CHECK_EQ(cutStats.foreground.occupied, std::size_t{7});
        CHECK(cutStats.bigDecor.present);
        CHECK(!cutStats.objects.present);
        CHECK_EQ(cutStats.objects.stored, std::size_t{0});
        CHECK_EQ(cutStats.missingBytes, std::size_t{5'468});
        CHECK(findActiveSlots(WorldView(cut), ObjectKey::type, 12).empty());
    }
    // Not a world at all.
    {
        const Bytes junk(100, std::byte{0x7F});
        const auto junkStats = computeWorldStats(WorldView(junk));
        CHECK(junkStats.revision == FormatRevision::unknown);
        CHECK(!junkStats.headerPresent);
        CHECK(!junkStats.foreground.present);
        CHECK(!junkStats.objects.present);
        CHECK_EQ(junkStats.fileBytes, std::size_t{100});
    }
    return test::finish("world_stats");
}
