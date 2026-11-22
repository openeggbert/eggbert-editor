// SPDX-License-Identifier: GPL-3.0-only
#include "world/adaptive.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <utility>

using namespace eggbert;
using namespace eggbert::test;

namespace {

constexpr auto sb1 = GameProfile::speedyBlupi1;
constexpr auto sb2 = GameProfile::speedyBlupi2;

// A scrolling blank world (dimDecor 100, 100), so the playfield is the
// whole 100 x 100 map.
Bytes scrollingWorld() {
    auto bytes = blankWorld(fxRevision13);
    put32(bytes, 212, 100);
    put32(bytes, 216, 100);
    return bytes;
}

void setFg(Bytes &bytes, std::size_t x, std::size_t y, int value) {
    put16(bytes, fxCellOffset(fxForegroundOffset, x, y), value);
}

std::int16_t fg(const EditBuilder &builder, std::size_t x, std::size_t y) {
    return *builder.view().foreground()->cell(x, y);
}

using CellSet = std::set<std::pair<std::size_t, std::size_t>>;
CellSet asSet(const std::vector<CellPos> &cells) {
    CellSet result;
    for (const auto &cell : cells) {
        result.insert({cell.x, cell.y});
    }
    return result;
}

// Expected border forms by mask, typed from the recorded evidence
// (bit 1 below, 2 above, 4 right, 8 left).
struct FamilyCase {
    GameProfile profile;
    const char *id;
    // A neighbour value that joins this family's cells.
    int joiner;
    std::array<int, 16> forms;
};

const FamilyCase familyCases[] = {
    {sb1, "rock", 1, {153, 147, 148, 146, 40, 151, 150, 144, 39, 152, 149, 145, 38, 36, 37, 35}},
    {sb1, "triangular-rock", 1,
     {75, 75, 155, 155, 75, 75, 155, 155, 74, 74, 154, 154, 75, 75, 155, 155}},
    {sb1, "grass", 1,
     {157, 157, 157, 157, 109, 109, 109, 109, 108, 108, 108, 108, 107, 107, 107, 107}},
    {sb1, "bar", 1,
     {202, 202, 138, 138, 202, 202, 138, 138, 202, 202, 138, 138, 202, 202, 138, 138}},
    {sb2, "rock", 1, {153, 147, 148, 146, 40, 151, 150, 144, 39, 152, 149, 145, 38, 36, 37, 35}},
    {sb2, "triangular-rock", 1,
     {75, 75, 155, 155, 75, 75, 155, 155, 74, 74, 154, 154, 75, 75, 155, 155}},
    {sb2, "grass", 1,
     {157, 157, 157, 157, 109, 109, 109, 109, 108, 108, 108, 108, 107, 107, 107, 107}},
    {sb2, "bar", 1,
     {202, 202, 138, 138, 202, 202, 138, 138, 202, 202, 138, 138, 202, 202, 138, 138}},
    {sb2, "bricks", 1,
     {283, 283, 283, 283, 262, 262, 262, 262, 263, 263, 263, 263, 261, 261, 261, 261}},
    {sb2, "slime", 1,
     {361, 359, 360, 352, 357, 353, 355, 349, 358, 354, 356, 350, 351, 347, 348, 341}},
    // Palace joins only palace: 400 is a palace value that never changes.
    {sb2, "palace", 400,
     {387, 387, 386, 386, 394, 394, 392, 392, 395, 395, 393, 393, 397, 397, 396, 396}},
    // Pipes join only pipes.
    {sb2, "pipes", 250,
     {251, 254, 254, 254, 251, 258, 260, 258, 251, 257, 259, 257, 251, 250, 256, 250}},
};

void checkAllMasks() {
    for (const auto &entry : familyCases) {
        const auto *family = findAdaptiveFamily(entry.profile, entry.id);
        CHECK(family != nullptr);
        if (family == nullptr) {
            continue;
        }
        CHECK(!family->provenance.empty());
        CHECK_EQ(family->seed, entry.forms[0]); // the seed is the isolated form
        for (unsigned mask = 0; mask < 16; ++mask) {
            auto bytes = scrollingWorld();
            if (mask & 1) {
                setFg(bytes, 50, 51, entry.joiner); // below
            }
            if (mask & 2) {
                setFg(bytes, 50, 49, entry.joiner); // above
            }
            if (mask & 4) {
                setFg(bytes, 51, 50, entry.joiner); // right
            }
            if (mask & 8) {
                setFg(bytes, 49, 50, entry.joiner); // left
            }
            const WorldDocument document(bytes);
            EditBuilder builder(document);
            NoVariants plain;
            const CellPos centre{50, 50};
            const auto result = adaptiveStroke(builder, entry.profile, {&centre, 1},
                                               family->seed, plain);
            CHECK_EQ(fg(builder, 50, 50), entry.forms[mask]);
            CHECK_EQ(result.changedFootprint.size(), std::size_t{1});
            // The mask the rules compute for the adapted cell is the one built.
            CHECK_EQ(adjacencyMask(builder.view(), entry.profile, centre), mask);
            CHECK_EQ(expectedBorderForm(builder.view(), entry.profile, centre).value(),
                     entry.forms[mask]);
        }
    }
    // Family lists per game; nothing is offered without a chosen game or
    // across games.
    CHECK_EQ(adaptiveFamilies(sb1).size(), std::size_t{4});
    CHECK_EQ(adaptiveFamilies(sb2).size(), std::size_t{17});
    CHECK(adaptiveFamilies(GameProfile::unknown).empty());
    CHECK(findAdaptiveFamily(sb1, "bricks") == nullptr);
    CHECK(findAdaptiveFamily(sb1, "cheese-246") == nullptr);
    CHECK(findAdaptiveFamily(sb1, "lightning") == nullptr);
    CHECK(findAdaptiveFamily(sb2, "cheese-339")->bordersEmptyNeighbours);
    CHECK(!findAdaptiveFamily(sb2, "rock")->bordersEmptyNeighbours);
}

void checkJoinRules() {
    // Map edges count as joined; empty cells do not.
    {
        const auto bytes = scrollingWorld();
        const WorldView world(bytes);
        CHECK_EQ(adjacencyMask(world, sb1, {50, 50}), 0u);
        CHECK_EQ(adjacencyMask(world, sb1, {0, 0}), joinLeft | joinAbove);
        CHECK_EQ(adjacencyMask(world, sb1, {99, 0}), joinRight | joinAbove);
        CHECK_EQ(adjacencyMask(world, sb1, {0, 99}), joinLeft | joinBelow);
        CHECK_EQ(adjacencyMask(world, sb2, {99, 99}), joinRight | joinBelow);
        CHECK_EQ(adjacencyMask(world, sb1, {50, 0}), joinAbove);
        CHECK_EQ(adjacencyMask(world, GameProfile::unknown, {0, 0}), 0u);
        CHECK_EQ(adjacencyMask(world, sb1, {100, 0}), 0u);
    }
    // With scrolling off the playfield is 10 cells wide and 8 high.
    {
        auto bytes = blankWorld(fxRevision13); // dimDecor (0, 0)
        const WorldView world(bytes);
        CHECK_EQ(adjacencyMask(world, sb1, {9, 3}), joinRight);
        CHECK_EQ(adjacencyMask(world, sb1, {8, 3}), 0u);
        CHECK_EQ(adjacencyMask(world, sb1, {4, 7}), joinBelow);
        CHECK_EQ(adjacencyMask(world, sb1, {4, 6}), 0u);
        put32(bytes, 212, 1); // horizontal scrolling on: only y is limited
        const WorldView wide(bytes);
        CHECK_EQ(adjacencyMask(wide, sb1, {9, 3}), 0u);
        CHECK_EQ(adjacencyMask(wide, sb1, {9, 7}), joinBelow);
    }
    // Directional rules of individual neighbours (same in both games
    // unless noted). `around` puts one value on each side of (50, 50).
    const auto maskWith = [](GameProfile profile, int value, int centre = -1) {
        auto bytes = scrollingWorld();
        setFg(bytes, 50, 51, value);
        setFg(bytes, 50, 49, value);
        setFg(bytes, 51, 50, value);
        setFg(bytes, 49, 50, value);
        setFg(bytes, 50, 50, centre);
        return adjacencyMask(WorldView(bytes), profile, {50, 50});
    };
    const unsigned all = joinBelow | joinAbove | joinRight | joinLeft;
    for (const auto profile : {sb1, sb2}) {
        CHECK_EQ(maskWith(profile, 1), all);         // ordinary tiles join
        CHECK_EQ(maskWith(profile, 33), 0u);         // 32..34 never join
        CHECK_EQ(maskWith(profile, 115), 0u);        // 110..125 never join
        CHECK_EQ(maskWith(profile, 141), 0u);        // 139..143 never join
        CHECK_EQ(maskWith(profile, 202), 0u);
        CHECK_EQ(maskWith(profile, 92), all & ~joinBelow);  // not to the cell above it
        CHECK_EQ(maskWith(profile, 68), all & ~joinBelow);
        CHECK_EQ(maskWith(profile, 21), all & ~joinAbove);  // not to the cell below it
        CHECK_EQ(maskWith(profile, 46), all & ~joinAbove);
        CHECK_EQ(maskWith(profile, 27), joinBelow | joinAbove);   // vertical only
        CHECK_EQ(maskWith(profile, 199), joinBelow | joinAbove);
        CHECK_EQ(maskWith(profile, 108), joinRight | joinLeft);   // horizontal only
        CHECK_EQ(maskWith(profile, 16), joinBelow | joinLeft);
        CHECK_EQ(maskWith(profile, 17), joinBelow | joinRight);
        CHECK_EQ(maskWith(profile, 126), joinLeft);
        CHECK_EQ(maskWith(profile, 129), joinRight);
        CHECK_EQ(maskWith(profile, 132), joinAbove);
        CHECK_EQ(maskWith(profile, 135), joinBelow);
        CHECK_EQ(maskWith(profile, 138), joinBelow);
        CHECK_EQ(maskWith(profile, 170), joinAbove);
    }
    // Differences between the games.
    CHECK_EQ(maskWith(sb1, 182), 0u);
    CHECK_EQ(maskWith(sb2, 182), joinBelow | joinAbove);
    CHECK_EQ(maskWith(sb1, 317), all);
    CHECK_EQ(maskWith(sb2, 317), all & ~joinAbove);
    CHECK_EQ(maskWith(sb1, 270), all);
    CHECK_EQ(maskWith(sb2, 270), 0u);  // cheese borders never join
    CHECK_EQ(maskWith(sb2, 404), 0u);
    CHECK_EQ(maskWith(sb2, 378), 0u);
    CHECK_EQ(maskWith(sb2, 430), joinBelow | joinAbove);
    CHECK_EQ(maskWith(sb2, 335), joinBelow | joinAbove);
    CHECK_EQ(maskWith(sb2, 415), joinAbove);
    CHECK_EQ(maskWith(sb1, 415), all);
    // Speedy Blupi 2: palace joins only palace, pipes only pipes, judged
    // from the centre cell.
    CHECK_EQ(maskWith(sb2, 1, 396), 0u);
    CHECK_EQ(maskWith(sb2, 400, 396), all);
    CHECK_EQ(maskWith(sb2, 1, 251), 0u);
    CHECK_EQ(maskWith(sb2, 256, 251), all);
    CHECK_EQ(maskWith(sb2, 256, 1), 0u); // a pipe does not join other tiles
    CHECK_EQ(maskWith(sb1, 1, 396), all); // no such rule in the first game
}

void checkVariants() {
    // A fully surrounded rock: the coin picks 156 or 35.
    const auto surrounded = [](GameProfile profile, int centre, VariantSource &variants) {
        auto bytes = scrollingWorld();
        setFg(bytes, 50, 51, 1);
        setFg(bytes, 50, 49, 1);
        setFg(bytes, 51, 50, 1);
        setFg(bytes, 49, 50, 1);
        const WorldDocument document(bytes);
        EditBuilder builder(document);
        const CellPos centreCell{50, 50};
        adaptiveStroke(builder, profile, {&centreCell, 1}, static_cast<std::int16_t>(centre),
                       variants);
        return fg(builder, 50, 50);
    };
    for (const auto profile : {sb1, sb2}) {
        RecordedVariants yes({1});
        CHECK_EQ(surrounded(profile, 153, yes), std::int16_t{156});
        CHECK_EQ(yes.used(), std::size_t{1});
        RecordedVariants no({0});
        CHECK_EQ(surrounded(profile, 153, no), std::int16_t{35});
        NoVariants plain;
        CHECK_EQ(surrounded(profile, 153, plain), std::int16_t{35});
    }
    // A stored 156: the first game keeps it without drawing; the second
    // game draws again.
    {
        RecordedVariants unused({0, 0, 0});
        CHECK_EQ(surrounded(sb1, 156, unused), std::int16_t{156});
        CHECK_EQ(unused.used(), std::size_t{0});
        RecordedVariants redraw({0});
        CHECK_EQ(surrounded(sb2, 156, redraw), std::int16_t{35});
        CHECK_EQ(redraw.used(), std::size_t{1});
    }
    // The first game's 156 still takes a border form when a side opens.
    {
        auto bytes = scrollingWorld();
        setFg(bytes, 50, 51, 1);
        setFg(bytes, 50, 49, 1);
        setFg(bytes, 51, 50, 1);
        const WorldDocument document(bytes);
        EditBuilder builder(document);
        NoVariants plain;
        const CellPos centre{50, 50};
        adaptiveStroke(builder, sb1, {&centre, 1}, 156, plain);
        CHECK_EQ(fg(builder, 50, 50), std::int16_t{144}); // below, above, right
    }
    // Speedy Blupi 2 looks: pipes 251..253, vertical pipe 254/255, slime.
    const auto isolated = [](int seed, VariantSource &variants, int joinerBelow = -1) {
        auto bytes = scrollingWorld();
        setFg(bytes, 50, 51, joinerBelow);
        const WorldDocument document(bytes);
        EditBuilder builder(document);
        const CellPos centre{50, 50};
        adaptiveStroke(builder, sb2, {&centre, 1}, static_cast<std::int16_t>(seed), variants);
        return fg(builder, 50, 50);
    };
    for (int pick = 0; pick < 3; ++pick) {
        RecordedVariants variants({pick});
        CHECK_EQ(isolated(251, variants), 251 + pick);
    }
    {
        RecordedVariants alternative({1});
        CHECK_EQ(isolated(251, alternative, 250), std::int16_t{255}); // vertical piece
        NoVariants plain;
        CHECK_EQ(isolated(251, plain, 250), std::int16_t{254});
        // Stored aliases are folded before lookup: 253 is a 251.
        CHECK_EQ(isolated(253, plain), std::int16_t{251});
    }
    for (const int pick : {0, 3, 5}) {
        auto bytes = scrollingWorld();
        for (const auto &[x, y] : {std::pair{50, 51}, {50, 49}, {51, 50}, {49, 50}}) {
            setFg(bytes, x, y, 1);
        }
        const WorldDocument document(bytes);
        EditBuilder builder(document);
        RecordedVariants variants({pick});
        const CellPos centre{50, 50};
        adaptiveStroke(builder, sb2, {&centre, 1}, 361, variants);
        CHECK_EQ(fg(builder, 50, 50), 341 + pick);
        CHECK_EQ(baseBorderForm(sb2, fg(builder, 50, 50)), std::int16_t{341});
    }
    CHECK_EQ(baseBorderForm(sb1, 156), std::int16_t{35});
    CHECK_EQ(baseBorderForm(sb2, 363), std::int16_t{348});
    CHECK_EQ(baseBorderForm(sb1, 363), std::int16_t{363});
    CHECK_EQ(baseBorderForm(sb2, 7), std::int16_t{7});
}

void checkStrokes() {
    const auto input = scrollingWorld();
    NoVariants plain;

    // A three-cell rock line: ends and middle take their forms; no cell
    // outside the footprint changes.
    const std::vector<CellPos> line{{20, 30}, {21, 30}, {22, 30}};
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        const auto result = adaptiveStroke(builder, sb1, line, 153, plain);
        CHECK_EQ(fg(builder, 20, 30), std::int16_t{40}); // joined right
        CHECK_EQ(fg(builder, 21, 30), std::int16_t{38}); // joined left and right
        CHECK_EQ(fg(builder, 22, 30), std::int16_t{39}); // joined left
        CHECK_EQ(result.changedFootprint.size(), std::size_t{3});
        CHECK(result.sideEffects.empty());
        // One transaction, exact undo and redo.
        CHECK(document.apply(builder.build("Adaptive Rock")).has_value());
        CHECK_EQ(document.historySize(), std::size_t{1});
        const auto painted = toVector(document.currentBytes());
        CHECK(document.undo());
        CHECK(std::ranges::equal(document.currentBytes(), input));
        CHECK(document.redo());
        CHECK(std::ranges::equal(document.currentBytes(), painted));
        // Only foreground bytes changed.
        for (std::size_t index = 0; index < input.size(); ++index) {
            if (index < 868 || index >= 20'868) {
                CHECK(painted[index] == input[index]);
            }
        }
        // The same line painted in the opposite order ends up identical.
        WorldDocument reversed(input);
        EditBuilder reverseBuilder(reversed);
        const std::vector<CellPos> backwards{{22, 30}, {21, 30}, {20, 30}};
        adaptiveStroke(reverseBuilder, sb1, backwards, 153, plain);
        CHECK(reversed.apply(reverseBuilder.build("Adaptive Rock")).has_value());
        CHECK(std::ranges::equal(reversed.currentBytes(), painted));

        // Adding a cell next to the line changes its neighbour as a side
        // effect, and only that neighbour.
        EditBuilder extend(document);
        const CellPos next{23, 30};
        const auto extended = adaptiveStroke(extend, sb1, {&next, 1}, 153, plain);
        CHECK((asSet(extended.changedFootprint) == CellSet{{23, 30}}));
        CHECK((asSet(extended.sideEffects) == CellSet{{22, 30}}));
        CHECK_EQ(fg(extend, 22, 30), std::int16_t{38});
        CHECK_EQ(fg(extend, 23, 30), std::int16_t{39});
        CHECK_EQ(fg(extend, 20, 30), std::int16_t{40}); // two cells away: untouched

        // Erasing the middle adaptively turns both ends into isolated rock.
        EditBuilder erase(document);
        const CellPos middle{21, 30};
        const auto erased = adaptiveStroke(erase, sb1, {&middle, 1}, -1, plain);
        CHECK_EQ(fg(erase, 21, 30), std::int16_t{-1});
        CHECK_EQ(fg(erase, 20, 30), std::int16_t{153});
        CHECK_EQ(fg(erase, 22, 30), std::int16_t{153});
        CHECK((asSet(erased.sideEffects) == CellSet{{20, 30}, {22, 30}}));
    }
    // A 2 x 2 block: every cell joins two neighbours.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        const std::vector<CellPos> block{{40, 40}, {41, 40}, {40, 41}, {41, 41}};
        adaptiveStroke(builder, sb2, block, 153, plain);
        CHECK_EQ(fg(builder, 40, 40), std::int16_t{151}); // below + right
        CHECK_EQ(fg(builder, 41, 40), std::int16_t{152}); // below + left
        CHECK_EQ(fg(builder, 40, 41), std::int16_t{150}); // above + right
        CHECK_EQ(fg(builder, 41, 41), std::int16_t{149}); // above + left
    }
    // Corners of the map: the outside counts as joined and nothing is
    // written outside the map.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        const std::vector<CellPos> corners{{0, 0}, {99, 99}, {99, 0}, {0, 99}};
        const auto result = adaptiveStroke(builder, sb1, corners, 153, plain);
        CHECK_EQ(fg(builder, 0, 0), std::int16_t{149});   // above + left
        CHECK_EQ(fg(builder, 99, 99), std::int16_t{151}); // below + right
        CHECK_EQ(fg(builder, 99, 0), std::int16_t{150});  // above + right
        CHECK_EQ(fg(builder, 0, 99), std::int16_t{152});  // below + left
        CHECK_EQ(result.changedFootprint.size(), std::size_t{4});
        CHECK(result.sideEffects.empty());
        const CellPos outside{100, 5};
        EditBuilder nothing(document);
        CHECK_EQ(adaptiveStroke(nothing, sb1, {&outside, 1}, 153, plain).changedCount(),
                 std::size_t{0});
    }
    // Painting an ordinary tile refreshes a neighbouring family tile, and
    // values of no family are never rewritten.
    {
        auto bytes = input;
        setFg(bytes, 60, 60, 153);
        setFg(bytes, 60, 62, 77);
        WorldDocument document(bytes);
        EditBuilder builder(document);
        const CellPos cell{60, 61};
        const auto result = adaptiveStroke(builder, sb1, {&cell, 1}, 5, plain);
        CHECK_EQ(fg(builder, 60, 61), std::int16_t{5});
        CHECK_EQ(fg(builder, 60, 60), std::int16_t{147}); // rock now joined below
        CHECK_EQ(fg(builder, 60, 62), std::int16_t{77});
        CHECK((asSet(result.sideEffects) == CellSet{{60, 60}}));
    }
    // Without a chosen game nothing is done; read-only revisions compute but
    // cannot apply.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        CHECK_EQ(adaptiveStroke(builder, GameProfile::unknown, line, 153, plain).changedCount(),
                 std::size_t{0});
        CHECK(!builder.hasChanges());
        auto old = blankWorld(fxRevision11);
        WorldDocument oldDocument(old);
        EditBuilder oldBuilder(oldDocument);
        adaptiveStroke(oldBuilder, sb1, line, 153, plain);
        CHECK(oldDocument.apply(oldBuilder.build("Adaptive")).error().error ==
              EditError::readOnly);
    }
}

void checkCheeseCaveAndLightning() {
    const auto input = scrollingWorld();
    NoVariants plain;

    // Cheese keeps its own value; its four empty neighbours get borders.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        const CellPos cell{30, 30};
        const auto result = adaptiveStroke(builder, sb2, {&cell, 1}, 246, plain);
        CHECK_EQ(fg(builder, 30, 30), std::int16_t{246});
        CHECK_EQ(fg(builder, 30, 31), std::int16_t{264}); // cheese above it
        CHECK_EQ(fg(builder, 30, 29), std::int16_t{265}); // cheese below it
        CHECK_EQ(fg(builder, 31, 30), std::int16_t{266}); // cheese to its left
        CHECK_EQ(fg(builder, 29, 30), std::int16_t{267}); // cheese to its right
        CHECK((asSet(result.sideEffects) == CellSet{{30, 31}, {30, 29}, {31, 30}, {29, 30}}));
        CHECK_EQ(fg(builder, 31, 31), std::int16_t{-1}); // no diagonal borders

        // The alternative looks of the border tiles.
        WorldDocument second(input);
        EditBuilder alternative(second);
        RecordedVariants always({1, 1, 1, 1, 1, 1, 1, 1});
        adaptiveStroke(alternative, sb2, {&cell, 1}, 246, always);
        CHECK_EQ(fg(alternative, 30, 31), std::int16_t{281});
        CHECK_EQ(fg(alternative, 30, 29), std::int16_t{282});
        CHECK_EQ(fg(alternative, 31, 30), std::int16_t{266}); // has no alternative

        // Erasing the cheese adaptively removes its borders again.
        CHECK(document.apply(builder.build("Cheese")).has_value());
        EditBuilder erase(document);
        adaptiveStroke(erase, sb2, {&cell, 1}, -1, plain);
        CHECK(document.apply(erase.build("Erase")).has_value());
        CHECK(std::ranges::equal(document.currentBytes(), input));
    }
    // Borders never overwrite other tiles, and two cheese cells border a
    // shared empty cell with the combined form.
    {
        auto bytes = input;
        setFg(bytes, 31, 30, 5);
        WorldDocument document(bytes);
        EditBuilder builder(document);
        const std::vector<CellPos> cells{{30, 30}, {30, 32}};
        adaptiveStroke(builder, sb2, cells, 247, plain);
        CHECK_EQ(fg(builder, 31, 30), std::int16_t{5});
        CHECK_EQ(fg(builder, 30, 31), std::int16_t{268}); // cheese above and below
    }
    // Cave works the same way with its own border tiles.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        const CellPos cell{70, 70};
        adaptiveStroke(builder, sb2, {&cell, 1}, 284, plain);
        CHECK_EQ(fg(builder, 70, 70), std::int16_t{284});
        CHECK_EQ(fg(builder, 70, 71), std::int16_t{285});
        CHECK_EQ(fg(builder, 70, 69), std::int16_t{286});
        CHECK_EQ(fg(builder, 71, 70), std::int16_t{287});
        CHECK_EQ(fg(builder, 69, 70), std::int16_t{288});
        RecordedVariants always({1, 1, 1, 1, 1, 1});
        WorldDocument second(input);
        EditBuilder alternative(second);
        adaptiveStroke(alternative, sb2, {&cell, 1}, 284, always);
        CHECK_EQ(fg(alternative, 70, 71), std::int16_t{302});
        CHECK_EQ(fg(alternative, 70, 69), std::int16_t{303});
    }
    // At the map corner nothing is written outside and the edge does not
    // count as cheese.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        const CellPos corner{0, 0};
        const auto result = adaptiveStroke(builder, sb2, {&corner, 1}, 339, plain);
        CHECK((asSet(result.sideEffects) == CellSet{{1, 0}, {0, 1}}));
        CHECK_EQ(fg(builder, 1, 0), std::int16_t{266});
        CHECK_EQ(fg(builder, 0, 1), std::int16_t{264});
    }
    // The first game has no cheese rule: the value is simply stored.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        const CellPos cell{30, 30};
        const auto result = adaptiveStroke(builder, sb1, {&cell, 1}, 246, plain);
        CHECK_EQ(fg(builder, 30, 30), std::int16_t{246});
        CHECK(result.sideEffects.empty());
    }

    // Lightning: up to four empty cells below the emitter become the beam.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        const CellPos emitter{10, 10};
        const auto result = adaptiveStroke(builder, sb2, {&emitter, 1}, 304, plain);
        CHECK_EQ(fg(builder, 10, 10), std::int16_t{304});
        for (std::size_t y = 11; y <= 14; ++y) {
            CHECK_EQ(fg(builder, 10, y), std::int16_t{305});
        }
        CHECK_EQ(fg(builder, 10, 15), std::int16_t{-1});
        CHECK((asSet(result.sideEffects) == CellSet{{10, 11}, {10, 12}, {10, 13}, {10, 14}}));
        CHECK(document.apply(builder.build("Lightning")).has_value());
        // Erasing the emitter adaptively takes the beam with it.
        EditBuilder erase(document);
        adaptiveStroke(erase, sb2, {&emitter, 1}, -1, plain);
        CHECK(document.apply(erase.build("Erase")).has_value());
        CHECK(std::ranges::equal(document.currentBytes(), input));
    }
    {
        // The beam stops at the first non-empty cell and at the map bottom.
        auto bytes = input;
        setFg(bytes, 10, 13, 5);
        WorldDocument document(bytes);
        EditBuilder builder(document);
        const std::vector<CellPos> emitters{{10, 10}, {20, 98}, {30, 99}};
        adaptiveStroke(builder, sb2, emitters, 304, plain);
        CHECK_EQ(fg(builder, 10, 11), std::int16_t{305});
        CHECK_EQ(fg(builder, 10, 12), std::int16_t{305});
        CHECK_EQ(fg(builder, 10, 13), std::int16_t{5});
        CHECK_EQ(fg(builder, 10, 14), std::int16_t{-1});
        CHECK_EQ(fg(builder, 20, 99), std::int16_t{305});
        CHECK_EQ(fg(builder, 30, 99), std::int16_t{304});
        // In the first game 304 is just a value.
        WorldDocument first(input);
        EditBuilder firstBuilder(first);
        const CellPos emitter{10, 10};
        adaptiveStroke(firstBuilder, sb1, {&emitter, 1}, 304, plain);
        CHECK_EQ(fg(firstBuilder, 10, 11), std::int16_t{-1});
    }
}

} // namespace

int main() {
    checkAllMasks();
    checkJoinRules();
    checkVariants();
    checkStrokes();
    checkCheeseCaveAndLightning();
    return test::finish("adaptive");
}
