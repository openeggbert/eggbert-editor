// SPDX-License-Identifier: GPL-3.0-only
#include "world/tile_tools.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <algorithm>
#include <set>
#include <utility>

using namespace eggbert;
using namespace eggbert::test;

namespace {

using CellSet = std::set<std::pair<std::size_t, std::size_t>>;

CellSet asSet(const std::vector<CellPos> &cells) {
    CellSet result;
    for (const auto &cell : cells) {
        result.insert({cell.x, cell.y});
    }
    return result;
}

// Cells whose two bytes differ between the document and the input, derived
// from literal offsets (foreground at 868, bigDecor at 20868).
CellSet changedCells(const WorldDocument &document, const Bytes &input, std::size_t layerOffset) {
    CellSet cells;
    const auto current = document.currentBytes();
    for (std::size_t x = 0; x < 100; ++x) {
        for (std::size_t y = 0; y < 100; ++y) {
            const auto offset = layerOffset + 200 * x + 2 * y;
            if (current[offset] != input[offset] || current[offset + 1] != input[offset + 1]) {
                cells.insert({x, y});
            }
        }
    }
    return cells;
}

// True when every byte outside one layer equals the input.
bool onlyLayerChanged(const WorldDocument &document, const Bytes &input, std::size_t layerOffset) {
    const auto current = document.currentBytes();
    for (std::size_t index = 0; index < input.size(); ++index) {
        const bool inLayer = index >= layerOffset && index < layerOffset + 20'000;
        if (!inLayer && current[index] != input[index]) {
            return false;
        }
    }
    return true;
}

std::int16_t fgAt(const WorldDocument &document, std::size_t x, std::size_t y) {
    return *document.view().foreground()->cell(x, y);
}

} // namespace

int main() {
    // Geometry helpers.
    CHECK((rectFromCorners({9, 2}, {3, 7}) == CellRect{{3, 2}, {9, 7}}));
    CHECK_EQ(rectFromCorners({9, 2}, {3, 7}).cellCount(), std::size_t{42});
    CHECK((clipToMap(-5, -5, 3, 2).value() == CellRect{{0, 0}, {3, 2}}));
    CHECK((clipToMap(98, 97, 150, 150).value() == CellRect{{98, 97}, {99, 99}}));
    CHECK((clipToMap(7, 9, 2, 4).value() == CellRect{{2, 4}, {7, 9}}));
    CHECK(!clipToMap(-9, 0, -1, 5).has_value());
    CHECK(!clipToMap(0, 100, 5, 120).has_value());
    CHECK_EQ(wholeMap().cellCount(), std::size_t{10'000});

    // Lines: endpoints included, one cell per major step, any direction.
    CHECK((asSet(lineCells({2, 2}, {2, 2})) == CellSet{{2, 2}}));
    CHECK((asSet(lineCells({1, 5}, {4, 5})) == CellSet{{1, 5}, {2, 5}, {3, 5}, {4, 5}}));
    CHECK((asSet(lineCells({4, 5}, {1, 5})) == CellSet{{1, 5}, {2, 5}, {3, 5}, {4, 5}}));
    CHECK((asSet(lineCells({7, 9}, {7, 6})) == CellSet{{7, 6}, {7, 7}, {7, 8}, {7, 9}}));
    CHECK((asSet(lineCells({0, 0}, {3, 3})) == CellSet{{0, 0}, {1, 1}, {2, 2}, {3, 3}}));
    CHECK((asSet(lineCells({3, 0}, {0, 3})) == CellSet{{3, 0}, {2, 1}, {1, 2}, {0, 3}}));
    {
        const auto shallow = lineCells({0, 0}, {6, 2});
        CHECK_EQ(shallow.size(), std::size_t{7});
        CHECK((shallow.front() == CellPos{0, 0}));
        CHECK((shallow.back() == CellPos{6, 2}));
        // Asymmetric: the transposed line is a different set.
        CHECK(asSet(shallow) != asSet(lineCells({0, 0}, {2, 6})));
        CHECK_EQ(lineCells({99, 0}, {0, 99}).size(), std::size_t{100});
    }
    // Outlines.
    CHECK_EQ(outlineCells({{2, 3}, {5, 6}}).size(), std::size_t{12});
    CHECK_EQ(asSet(outlineCells({{2, 3}, {5, 6}})).size(), std::size_t{12});
    CHECK(!asSet(outlineCells({{2, 3}, {5, 6}})).contains({3, 4}));
    CHECK_EQ(outlineCells({{4, 4}, {4, 4}}).size(), std::size_t{1});
    CHECK_EQ(outlineCells({{4, 4}, {8, 4}}).size(), std::size_t{5});
    CHECK_EQ(outlineCells({{4, 4}, {4, 9}}).size(), std::size_t{6});

    // Masks.
    {
        auto mask = CellMask::fromRect({{10, 20}, {12, 21}});
        CHECK_EQ(mask.count(), std::size_t{6});
        CHECK(mask.test({11, 21}));
        CHECK(!mask.test({21, 11}));
        CHECK(!mask.test({100, 0}));
        CHECK((mask.bounds().value() == CellRect{{10, 20}, {12, 21}}));
        mask.set({50, 60});
        mask.set({500, 60}); // ignored
        CHECK((mask.bounds().value() == CellRect{{10, 20}, {50, 60}}));
        CHECK_EQ(mask.cells().size(), std::size_t{7});
        CHECK((mask.cells().back() == CellPos{50, 60}));
        mask.invert();
        CHECK_EQ(mask.count(), std::size_t{9'993});
        CHECK(!mask.test({50, 60}));
        mask.clear();
        CHECK(mask.empty());
        CHECK(!mask.bounds().has_value());
    }

    const auto input = patternedWorld(fxRevision13, 9);

    // Paint and rectangles on the foreground only.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        const auto cells = lineCells({97, 0}, {99, 2});
        // The patterned fixture may already hold the value somewhere; pick a
        // value that no cell on the line has.
        CHECK_EQ(paintCells(builder, SectionKind::foreground, cells, 12'345), std::size_t{3});
        // Painting the same cells again changes nothing more.
        CHECK_EQ(paintCells(builder, SectionKind::foreground, cells, 12'345), std::size_t{0});
        CHECK(document.apply(builder.build("Line")).has_value());
        CHECK((changedCells(document, input, 868) == CellSet{{97, 0}, {98, 1}, {99, 2}}));
        CHECK(onlyLayerChanged(document, input, 868));
        CHECK_EQ(document.historySize(), std::size_t{1});
    }
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        CHECK_EQ(fillRect(builder, SectionKind::bigDecor, {{0, 97}, {1, 99}}, 12'345),
                 std::size_t{6});
        CHECK(document.apply(builder.build("Rectangle")).has_value());
        CHECK((changedCells(document, input, 20'868) ==
               CellSet{{0, 97}, {0, 98}, {0, 99}, {1, 97}, {1, 98}, {1, 99}}));
        CHECK(onlyLayerChanged(document, input, 20'868));
        // The transposed corner of the foreground is untouched.
        CHECK(changedCells(document, input, 868).empty());
    }
    // A limit mask restricts painting.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        auto limit = CellMask::fromRect({{10, 10}, {11, 10}});
        CHECK_EQ(fillRect(builder, SectionKind::foreground, {{9, 9}, {13, 13}}, 12'345, &limit),
                 std::size_t{2});
        CHECK(document.apply(builder.build("Limited")).has_value());
        CHECK((changedCells(document, input, 868) == CellSet{{10, 10}, {11, 10}}));
        EditBuilder masked(document);
        CHECK_EQ(fillMask(masked, SectionKind::foreground, limit, 12'345), std::size_t{0});
        CHECK_EQ(fillMask(masked, SectionKind::foreground, limit, 1), std::size_t{2});
        CHECK_EQ(fillMask(masked, SectionKind::foreground, CellMask{}, 2), std::size_t{0});
    }

    // Flood fill on a hand-drawn shape in a blank world.
    {
        auto blank = blankWorld(fxRevision13);
        const auto wall = [&](std::size_t x, std::size_t y) {
            put16(blank, fxCellOffset(fxForegroundOffset, x, y), 9);
        };
        // A closed box with interior (11..13, 21..22) and a diagonal-only gap.
        for (std::size_t x = 10; x <= 14; ++x) {
            wall(x, 20);
            wall(x, 23);
        }
        for (std::size_t y = 20; y <= 23; ++y) {
            wall(10, y);
            wall(14, y);
        }
        put16(blank, fxCellOffset(fxForegroundOffset, 14, 23), -1); // corner open diagonally
        WorldDocument document(blank);
        const auto region = floodRegion(document.view(), SectionKind::foreground, {12, 21});
        CHECK((asSet(region) ==
               CellSet{{11, 21}, {12, 21}, {13, 21}, {11, 22}, {12, 22}, {13, 22}}));
        EditBuilder builder(document);
        CHECK_EQ(floodFill(builder, SectionKind::foreground, {12, 21}, 4), std::size_t{6});
        CHECK(document.apply(builder.build("Fill")).has_value());
        CHECK((changedCells(document, blank, 868) == asSet(region)));
        CHECK_EQ(fgAt(document, 15, 24), std::int16_t{-1}); // did not leak diagonally
        CHECK_EQ(fgAt(document, 10, 21), std::int16_t{9});

        // Filling the wall follows the connected wall cells only.
        const auto wallRegion = floodRegion(document.view(), SectionKind::foreground, {10, 20});
        CHECK_EQ(wallRegion.size(), std::size_t{13});
        // Filling with the value already present changes nothing.
        EditBuilder same(document);
        CHECK_EQ(floodFill(same, SectionKind::foreground, {12, 21}, 4), std::size_t{0});
        CHECK(!same.hasChanges());

        // The outside region is bounded by the map; a limit bounds it further.
        const auto outside = floodRegion(document.view(), SectionKind::foreground, {0, 0});
        CHECK_EQ(outside.size(), std::size_t{10'000 - 13 - 6});
        const auto limit = CellMask::fromRect({{0, 0}, {4, 1}});
        CHECK_EQ(floodRegion(document.view(), SectionKind::foreground, {0, 0}, &limit).size(),
                 std::size_t{10});
        CHECK(floodRegion(document.view(), SectionKind::foreground, {50, 50}, &limit).empty());
        CHECK(floodRegion(document.view(), SectionKind::foreground, {100, 0}).empty());
        CHECK(floodRegion(document.view(), SectionKind::objects, {0, 0}).empty());
        // Corner start.
        EditBuilder corner(document);
        const auto small = CellMask::fromRect({{98, 98}, {99, 99}});
        CHECK_EQ(floodFill(corner, SectionKind::foreground, {99, 99}, 8, &small), std::size_t{4});

        // Replace one value everywhere, then only inside a mask.
        EditBuilder replace(document);
        CHECK_EQ(replaceValue(replace, SectionKind::foreground, 9, 3), std::size_t{13});
        CHECK_EQ(replaceValue(replace, SectionKind::foreground, 9, 3), std::size_t{0});
        CHECK_EQ(replaceValue(replace, SectionKind::foreground, 3, 3), std::size_t{0});
        const auto top = CellMask::fromRect({{10, 20}, {14, 20}});
        CHECK_EQ(replaceValue(replace, SectionKind::foreground, 3, 2, &top), std::size_t{5});
        CHECK(document.apply(replace.build("Replace")).has_value());
        CHECK_EQ(fgAt(document, 12, 20), std::int16_t{2});
        CHECK_EQ(fgAt(document, 12, 23), std::int16_t{3});
        CHECK_EQ(document.historySize(), std::size_t{2});
    }

    // Copy, paste with clipping, holes and skip-empty.
    {
        auto blank = blankWorld(fxRevision13);
        put16(blank, fxCellOffset(fxForegroundOffset, 5, 5), 1);
        put16(blank, fxCellOffset(fxForegroundOffset, 6, 5), 2);
        put16(blank, fxCellOffset(fxForegroundOffset, 7, 5), 3);
        put16(blank, fxCellOffset(fxForegroundOffset, 5, 6), 4);
        // (6,6) stays -1, (7,6) = 6
        put16(blank, fxCellOffset(fxForegroundOffset, 7, 6), 6);
        WorldDocument document(blank);
        const auto mask = CellMask::fromRect({{5, 5}, {7, 6}});
        const auto patch = copyPatch(document.view(), SectionKind::foreground, mask);
        CHECK_EQ(patch.width, std::size_t{3});
        CHECK_EQ(patch.height, std::size_t{2});
        CHECK((patch.values == std::vector<std::int16_t>{1, 2, 3, 4, -1, 6}));
        CHECK_EQ(patch.presentCount(), std::size_t{6});
        CHECK(patch.sourceLayer == SectionKind::foreground);

        // Paste elsewhere: all six cells written (including the -1).
        {
            EditBuilder builder(document);
            put16(blank, 0, 1); // no-op, keeps `blank` in use
            CHECK_EQ(pastePatch(builder, SectionKind::foreground, patch, 40, 50), std::size_t{5});
            CHECK(document.apply(builder.build("Paste")).has_value());
            CHECK_EQ(fgAt(document, 40, 50), std::int16_t{1});
            CHECK_EQ(fgAt(document, 42, 50), std::int16_t{3});
            CHECK_EQ(fgAt(document, 40, 51), std::int16_t{4});
            CHECK_EQ(fgAt(document, 41, 51), std::int16_t{-1});
            CHECK_EQ(fgAt(document, 42, 51), std::int16_t{6});
            CHECK_EQ(fgAt(document, 50, 40), std::int16_t{-1}); // not transposed
            CHECK(document.undo());
        }
        // Clipped at the right/bottom edge and at the top/left edge.
        {
            EditBuilder builder(document);
            CHECK_EQ(pastePatch(builder, SectionKind::foreground, patch, 98, 99), std::size_t{2});
            CHECK_EQ(pasteFootprint(patch, 98, 99).size(), std::size_t{2});
            CHECK_EQ(pastePatch(builder, SectionKind::foreground, patch, -2, -1), std::size_t{1});
            CHECK((asSet(pasteFootprint(patch, -2, -1)) == CellSet{{0, 0}}));
            CHECK(document.apply(builder.build("Clipped paste")).has_value());
            CHECK_EQ(fgAt(document, 98, 99), std::int16_t{1});
            CHECK_EQ(fgAt(document, 99, 99), std::int16_t{2});
            CHECK_EQ(fgAt(document, 0, 0), std::int16_t{6});
            CHECK(document.undo());
            CHECK(pasteFootprint(patch, 100, 0).empty());
            CHECK(pasteFootprint(patch, -3, 0).empty());
        }
        // skipEmpty leaves the destination where the patch holds -1.
        {
            EditBuilder prepare(document);
            prepare.setCell(SectionKind::foreground, 21, 31, 77);
            CHECK(document.apply(prepare.build("prepare")).has_value());
            EditBuilder builder(document);
            CHECK_EQ(pastePatch(builder, SectionKind::foreground, patch, 20, 30, true),
                     std::size_t{5});
            CHECK(document.apply(builder.build("Paste over")).has_value());
            CHECK_EQ(fgAt(document, 21, 31), std::int16_t{77});
            CHECK(document.undo());
            EditBuilder overwrite(document);
            CHECK_EQ(pastePatch(overwrite, SectionKind::foreground, patch, 20, 30, false),
                     std::size_t{6});
            CHECK(document.apply(overwrite.build("Paste all")).has_value());
            CHECK_EQ(fgAt(document, 21, 31), std::int16_t{-1});
            CHECK(document.undo());
            CHECK(document.undo());
        }
        // A non-rectangular mask leaves holes that paste does not touch.
        {
            CellMask lShape;
            lShape.set({5, 5});
            lShape.set({7, 6});
            const auto holes = copyPatch(document.view(), SectionKind::foreground, lShape);
            CHECK_EQ(holes.width, std::size_t{3});
            CHECK_EQ(holes.height, std::size_t{2});
            CHECK_EQ(holes.presentCount(), std::size_t{2});
            EditBuilder builder(document);
            CHECK_EQ(pastePatch(builder, SectionKind::foreground, holes, 60, 60), std::size_t{2});
            CHECK((asSet(pasteFootprint(holes, 60, 60)) == CellSet{{60, 60}, {62, 61}}));
        }
        // Cut = copy + clear in one transaction; paste restores it exactly.
        {
            EditBuilder cut(document);
            CHECK_EQ(fillMask(cut, SectionKind::foreground, mask, -1), std::size_t{5});
            CHECK(document.apply(cut.build("Cut")).has_value());
            CHECK_EQ(fgAt(document, 5, 5), std::int16_t{-1});
            EditBuilder paste(document);
            CHECK_EQ(pastePatch(paste, SectionKind::foreground, patch, 5, 5), std::size_t{5});
            CHECK(document.apply(paste.build("Paste")).has_value());
            CHECK(std::ranges::equal(document.currentBytes(), blank));
        }
        // Pasting into bigDecor writes only bigDecor; into an absent layer nothing.
        {
            EditBuilder builder(document);
            CHECK_EQ(pastePatch(builder, SectionKind::bigDecor, patch, 0, 0), std::size_t{5});
            CHECK(document.apply(builder.build("Paste big")).has_value());
            CHECK(onlyLayerChanged(document, blank, 20'868));
            const WorldDocument old(blankWorld(fxRevision10));
            EditBuilder oldBuilder(old);
            CHECK_EQ(pastePatch(oldBuilder, SectionKind::bigDecor, patch, 0, 0), std::size_t{0});
            CHECK_EQ(fillRect(oldBuilder, SectionKind::bigDecor, wholeMap(), 1), std::size_t{0});
            CHECK(copyPatch(old.view(), SectionKind::bigDecor, mask).empty());
        }
        // Flips move values without changing them.
        {
            const auto horizontal = flippedHorizontally(patch);
            CHECK((horizontal.values == std::vector<std::int16_t>{3, 2, 1, 6, -1, 4}));
            const auto vertical = flippedVertically(patch);
            CHECK((vertical.values == std::vector<std::int16_t>{4, -1, 6, 1, 2, 3}));
            CHECK(flippedHorizontally(horizontal) == patch);
            CHECK(flippedVertically(vertical) == patch);
        }
    }
    return test::finish("tile_tools");
}
