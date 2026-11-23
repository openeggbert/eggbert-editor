// SPDX-License-Identifier: GPL-3.0-only
//
// Stamps (a piece of a world with holes and objects, placed as an overlay,
// all objects or none), their text format, and aligning objects as a group.
#include "world/stamp.hpp"

#include "world/world_probe.hpp"

#include "check.hpp"
#include "fixture.hpp"

using namespace eggbert;
using namespace eggbert::test;

namespace {

void tile(Bytes &bytes, std::size_t layerOffset, std::size_t x, std::size_t y, int value) {
    put16(bytes, fxCellOffset(layerOffset, x, y), value);
}

void object(Bytes &bytes, std::size_t slot, int type, int x, int y, int endX, int endY) {
    const auto at = fxSlotOffset(fxRevision13, slot);
    put16(bytes, at, type);
    put16(bytes, at + 2, 3);
    put32(bytes, at + 12, x);
    put32(bytes, at + 16, y);
    put32(bytes, at + 20, endX);
    put32(bytes, at + 24, endY);
    put32(bytes, at + 28, x);
    put32(bytes, at + 32, y);
    put16(bytes, at + 42, 10);
    put16(bytes, at + 44, 7);
}

std::string errorOf(const std::expected<Stamp, std::string> &result) {
    return result ? std::string() : result.error();
}

} // namespace

int main() {
    // A source world: a 3 x 2 piece at (10, 20) with a hole, a back-layer
    // tile and two objects, one of them a mover.
    auto source = blankWorld(fxRevision13);
    tile(source, fxForegroundOffset, 10, 20, 30);
    tile(source, fxForegroundOffset, 11, 20, 31);
    tile(source, fxForegroundOffset, 12, 21, 32);
    tile(source, fxRevision13.bigDecorOffset, 10, 21, 20);
    object(source, 5, 5, 10 * 64 + 2, 20 * 64 + 4, 10 * 64 + 2, 20 * 64 + 4);
    object(source, 6, 1, 12 * 64, 20 * 64, 15 * 64, 20 * 64); // its path leaves the piece
    object(source, 7, 5, 30 * 64, 30 * 64, 30 * 64, 30 * 64); // outside the piece
    const WorldView sourceView(source, probeWorld(source));
    const CellRect rect{{10, 20}, {12, 21}};

    const auto stamp = makeStamp(sourceView, rect, "Ledge", GameProfile::speedyBlupi1, true);
    CHECK_EQ(stamp.width, std::size_t{3});
    CHECK_EQ(stamp.height, std::size_t{2});
    CHECK(stamp.foreground == (std::vector<std::int16_t>{30, 31, -1, -1, -1, 32}));
    CHECK(stamp.bigDecor == (std::vector<std::int16_t>{-1, -1, -1, 20, -1, -1}));
    CHECK_EQ(stamp.objects.size(), std::size_t{2});
    CHECK(stamp.objects[0].posStart == (Point{2, 4}));
    CHECK(stamp.objects[1].posStart == (Point{128, 0}) && stamp.objects[1].posEnd == (Point{320, 0}));
    CHECK_EQ(stamp.objects[1].stepAdvance, 3);
    CHECK(makeStamp(sourceView, rect, "Tiles", GameProfile::speedyBlupi1, false).objects.empty());

    // The text format round-trips exactly.
    const auto text = writeStamp(stamp);
    const auto read = readStamp(text);
    CHECK(read.has_value());
    CHECK(read && *read == stamp);
    CHECK(read && writeStamp(*read) == text);
    CHECK(text.starts_with("eggbert-stamp 1\nname Ledge\ngame 1\nsize 3 2\nforeground\n30 31 -1\n"));
    {
        std::string windows;
        for (const char character : text) {
            windows += character == '\n' ? std::string("\r\n") : std::string(1, character);
        }
        CHECK(readStamp(windows).has_value());
    }
    // And refuses what is not a stamp.
    {
        const auto replaced = [&](std::string_view from, std::string_view to) {
            auto copy = text;
            const auto at = copy.find(from);
            CHECK(at != std::string::npos);
            copy.replace(at, from.size(), to);
            return errorOf(readStamp(copy));
        };
        CHECK(errorOf(readStamp("")).starts_with("Line 1"));
        CHECK(errorOf(readStamp("something else")).starts_with("Line 1"));
        CHECK(replaced("eggbert-stamp 1", "eggbert-stamp 2").starts_with("Line 1"));
        CHECK(replaced("game 1", "game 7").starts_with("Line 3"));
        CHECK(replaced("size 3 2", "size 3").starts_with("Line 4"));
        CHECK(replaced("size 3 2", "size 0 2").starts_with("Line 4"));
        CHECK(replaced("size 3 2", "size 101 2").starts_with("Line 4"));
        CHECK(replaced("30 31 -1", "30 31").starts_with("Line 6"));
        CHECK(replaced("30 31 -1", "30 31 x").starts_with("Line 6"));
        CHECK(replaced("30 31 -1", "30 31 40000").starts_with("Line 6"));
        CHECK(replaced("bigdecor", "backlayer").find("expected \"bigdecor\"") != std::string::npos);
        CHECK(replaced("objects 2", "objects 3").find("eighteen numbers") != std::string::npos);
        CHECK(replaced("objects 2", "objects 201").find("0 to 200") != std::string::npos);
        CHECK(errorOf(readStamp(text + "junk\n")).find("unexpected text") != std::string::npos);
        // A record with type 0 or a field that does not fit.
        CHECK(replaced("5 3 0 0 0 0 2 4", "0 3 0 0 0 0 2 4").find("type 0") != std::string::npos);
        CHECK(replaced("5 3 0 0 0 0 2 4", "5 99999 0 0 0 0 2 4").find("16-bit") !=
              std::string::npos);
    }

    // The footprint of a placement: cells that get a tile and cells holding
    // an object's start, each once, in row order; holes and cells outside
    // the map are not part of it.
    {
        // tiles at (0,0), (1,0), (2,1), back tile at (0,1); starts in (0,0) and (2,0)
        CHECK(stampFootprint(stamp, {50, 40}) ==
              (std::vector<CellPos>{{50, 40}, {51, 40}, {52, 40}, {50, 41}, {52, 41}}));
        CHECK(stampFootprint(stamp, {98, 99}) == (std::vector<CellPos>{{98, 99}, {99, 99}}));
        CHECK(stampFootprint(stamp, {99, 99}) == (std::vector<CellPos>{{99, 99}}));
        CHECK(stampFootprint(Stamp{}, {0, 0}).empty());
        auto damaged = stamp;
        damaged.bigDecor.pop_back();
        CHECK(stampFootprint(damaged, {0, 0}).empty());
    }

    // Placing: an overlay (holes leave what is there), one transaction.
    {
        auto target = blankWorld(fxRevision13);
        tile(target, fxForegroundOffset, 52, 40, 99); // under a hole of the stamp: stays
        tile(target, fxForegroundOffset, 50, 40, 98); // under a tile: replaced
        object(target, 0, 5, 64, 64, 64, 64);         // slot 0 is taken
        WorldDocument document{Bytes(target)};
        EditBuilder builder(document);
        const auto placed = placeStamp(builder, stamp, {50, 40});
        CHECK(placed.has_value());
        CHECK_EQ(placed->tilesChanged, std::size_t{4});
        CHECK_EQ(placed->objectsCreated, std::size_t{2});
        CHECK_EQ(placed->tilesClipped, std::size_t{0});
        CHECK(document.apply(builder.build("Place Stamp")).has_value());
        const auto view = document.view();
        CHECK_EQ(*view.foreground()->cell(50, 40), 30);
        CHECK_EQ(*view.foreground()->cell(51, 40), 31);
        CHECK_EQ(*view.foreground()->cell(52, 40), 99);
        CHECK_EQ(*view.foreground()->cell(52, 41), 32);
        CHECK_EQ(*view.bigDecor()->cell(50, 41), 20);
        // The objects took the lowest free slots, moved with the stamp.
        CHECK(view.objects()->record(1)->posStart == (Point{50 * 64 + 2, 40 * 64 + 4}));
        CHECK(view.objects()->record(2)->posStart == (Point{52 * 64, 40 * 64}));
        CHECK(view.objects()->record(2)->posEnd == (Point{55 * 64, 40 * 64}));
        CHECK_EQ(view.objects()->record(2)->stepAdvance, 3);
        CHECK_EQ(view.objects()->activeCount(), std::size_t{3});
        // One undo step restores everything.
        CHECK(document.undo());
        CHECK(toVector(document.currentBytes()) == target);
    }
    // At the edge: tiles are clipped, an object outside the world refuses
    // the whole placement.
    {
        WorldDocument document{blankWorld(fxRevision13)};
        const auto tilesOnly = makeStamp(sourceView, rect, "Tiles", GameProfile::speedyBlupi1, false);
        EditBuilder builder(document);
        const auto placed = placeStamp(builder, tilesOnly, {98, 99});
        CHECK(placed.has_value());
        CHECK_EQ(placed->tilesChanged, std::size_t{2});
        CHECK_EQ(placed->tilesClipped, std::size_t{2});
        EditBuilder second(document);
        const auto refused = placeStamp(second, stamp, {98, 99});
        CHECK(!refused.has_value());
        CHECK(refused.error().find("outside the world") != std::string::npos);
        CHECK(!second.hasChanges());
    }
    // A full pool: nothing is placed, not even the tiles.
    {
        auto full = blankWorld(fxRevision13);
        for (std::size_t slot = 0; slot < 199; ++slot) {
            object(full, slot, 5, 64, 64, 64, 64);
        }
        WorldDocument document{std::move(full)};
        EditBuilder builder(document);
        const auto refused = placeStamp(builder, stamp, {50, 40});
        CHECK(!refused.has_value());
        CHECK(refused.error().find("2 object(s) but only 1 object slot(s)") != std::string::npos);
        CHECK(!builder.hasChanges());
    }
    {
        WorldDocument document{blankWorld(fxRevision13)};
        EditBuilder builder(document);
        CHECK(!placeStamp(builder, Stamp{}, {0, 0}).has_value());
    }

    // Aligning: every position of an object moves together.
    {
        auto world = blankWorld(fxRevision13);
        object(world, 0, 5, 100, 500, 100, 500);
        object(world, 1, 1, 300, 520, 600, 520);
        object(world, 2, 5, 900, 480, 900, 480);
        object(world, 3, 5, 500, 700, 500, 700); // not among the chosen
        const std::size_t chosen[] = {0, 1, 2, 9, 1}; // a free slot and a repeat are ignored
        {
            WorldDocument document{Bytes(world)};
            EditBuilder builder(document);
            CHECK_EQ(alignObjects(builder, chosen, AlignMode::top), std::size_t{2});
            const auto pool = builder.view().objects();
            CHECK_EQ(pool->record(0)->posStart.y, 480);
            CHECK(pool->record(1)->posStart == (Point{300, 480}));
            CHECK(pool->record(1)->posEnd == (Point{600, 480}));
            CHECK(pool->record(1)->posCurrent == (Point{300, 480}));
            CHECK(pool->record(3)->posStart == (Point{500, 700}));
        }
        {
            WorldDocument document{Bytes(world)};
            EditBuilder builder(document);
            CHECK_EQ(alignObjects(builder, chosen, AlignMode::right), std::size_t{2});
            const auto pool = builder.view().objects();
            CHECK_EQ(pool->record(0)->posStart.x, 900);
            CHECK(pool->record(1)->posStart.x == 900 && pool->record(1)->posEnd.x == 1200);
        }
        {
            WorldDocument document{Bytes(world)};
            EditBuilder builder(document);
            CHECK_EQ(alignObjects(builder, chosen, AlignMode::left), std::size_t{2});
            CHECK_EQ(builder.view().objects()->record(2)->posStart.x, 100);
            EditBuilder bottom(document);
            CHECK_EQ(alignObjects(bottom, chosen, AlignMode::bottom), std::size_t{2});
            CHECK_EQ(bottom.view().objects()->record(2)->posStart.y, 520);
        }
        {
            // Evenly between the outermost starts: 100, 500, 900.
            WorldDocument document{Bytes(world)};
            EditBuilder builder(document);
            CHECK_EQ(alignObjects(builder, chosen, AlignMode::distributeHorizontally),
                     std::size_t{1});
            const auto pool = builder.view().objects();
            CHECK(pool->record(1)->posStart.x == 500 && pool->record(1)->posEnd.x == 800);
            CHECK(pool->record(0)->posStart.x == 100 && pool->record(2)->posStart.x == 900);
            EditBuilder vertical(document);
            CHECK_EQ(alignObjects(vertical, chosen, AlignMode::distributeVertically),
                     std::size_t{0}); // 480, 500, 520 are already even
        }
        {
            // Too few objects: nothing happens.
            WorldDocument document{Bytes(world)};
            EditBuilder builder(document);
            const std::size_t one[] = {0};
            const std::size_t two[] = {0, 1};
            CHECK_EQ(alignObjects(builder, one, AlignMode::left), std::size_t{0});
            CHECK_EQ(alignObjects(builder, two, AlignMode::distributeHorizontally), std::size_t{0});
            CHECK(!builder.hasChanges());
        }
    }
    return test::finish("stamp");
}
