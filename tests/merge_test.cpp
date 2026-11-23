// SPDX-License-Identifier: GPL-3.0-only
//
// Three-way merge: changes of either side are combined element by element;
// an element both sides changed differently is a conflict, resolved only by
// an explicit preference.
#include "world/merge.hpp"

#include "check.hpp"
#include "fixture.hpp"

using namespace eggbert;
using namespace eggbert::test;

int main() {
    const auto base = patternedWorld(fxRevision13, 6); // arbitrary bytes, six after the world
    const auto cell = [](std::size_t x, std::size_t y) {
        return fxCellOffset(fxForegroundOffset, x, y);
    };

    // Nothing changed anywhere: the base comes back.
    {
        const auto merged = mergeWorlds(base, base, base, MergePreference::none);
        CHECK(merged.has_value());
        CHECK(merged->bytes == base);
        CHECK(merged->conflicts.empty());
        CHECK_EQ(merged->fromMine + merged->fromTheirs + merged->agreed, std::size_t{0});
    }

    // Disjoint changes: both are in the result, everything else is the base.
    auto mine = base;
    auto theirs = base;
    put16(mine, cell(10, 10), 300);                         // a cell
    put16(mine, 222, 5);                                    // music
    put16(mine, fxSlotOffset(fxRevision13, 3) + 44, 777);   // an object's icon
    put16(theirs, cell(20, 20), 301);                       // another cell
    put16(theirs, fxCellOffset(fxRevision13.bigDecorOffset, 10, 10), 21);
    put16(theirs, 224, 9);                                  // region
    put32(theirs, fxSlotOffset(fxRevision13, 8) + 12, 4242); // another object's start
    theirs[base.size() - 1] = std::byte{0x5A};              // a byte after the world
    {
        const auto merged = mergeWorlds(base, mine, theirs, MergePreference::none);
        CHECK(merged.has_value());
        CHECK(merged->conflicts.empty());
        CHECK_EQ(merged->fromMine, std::size_t{3});
        CHECK_EQ(merged->fromTheirs, std::size_t{5});
        auto expected = base;
        put16(expected, cell(10, 10), 300);
        put16(expected, 222, 5);
        put16(expected, fxSlotOffset(fxRevision13, 3) + 44, 777);
        put16(expected, cell(20, 20), 301);
        put16(expected, fxCellOffset(fxRevision13.bigDecorOffset, 10, 10), 21);
        put16(expected, 224, 9);
        put32(expected, fxSlotOffset(fxRevision13, 8) + 12, 4242);
        expected[base.size() - 1] = std::byte{0x5A};
        CHECK(merged->bytes == expected);
        // The same merge with the sides swapped gives the same world.
        const auto swapped = mergeWorlds(base, theirs, mine, MergePreference::none);
        CHECK(swapped.has_value() && swapped->bytes == expected);
        CHECK_EQ(swapped->fromMine, std::size_t{5});
    }

    // The same change on both sides is no conflict.
    {
        auto both = base;
        put16(both, cell(30, 30), 55);
        const auto merged = mergeWorlds(base, both, both, MergePreference::none);
        CHECK(merged.has_value() && merged->conflicts.empty());
        CHECK_EQ(merged->agreed, std::size_t{1});
        CHECK(merged->bytes == both);
    }

    // Conflicts: a cell, a header value, and two fields of one object (the
    // record is one element).
    {
        auto left = base;
        auto right = base;
        put16(left, cell(40, 40), 1);
        put16(right, cell(40, 40), 2);
        put16(left, 222, 3);
        put16(right, 222, 4);
        put16(left, fxSlotOffset(fxRevision13, 5) + 44, 11);   // icon
        put32(right, fxSlotOffset(fxRevision13, 5) + 12, 999); // start x
        put16(left, cell(41, 40), 7); // not a conflict
        const auto none = mergeWorlds(base, left, right, MergePreference::none);
        CHECK(none.has_value());
        CHECK_EQ(none->conflicts.size(), std::size_t{3});
        CHECK(none->conflicts[0].range == (ByteRange{222, 2}));
        CHECK(none->conflicts[0].location.headerField == HeaderField::music);
        CHECK(none->conflicts[1].range == (ByteRange{cell(40, 40), 2}));
        CHECK(none->conflicts[1].location.area == OffsetArea::foreground);
        CHECK(none->conflicts[1].location.cell == (CellPos{40, 40}));
        CHECK(none->conflicts[2].range == (ByteRange{fxSlotOffset(fxRevision13, 5), 48}));
        CHECK_EQ(none->conflicts[2].location.slot, std::size_t{5});
        CHECK_EQ(none->fromMine, std::size_t{1});
        // Without a preference the conflicting elements keep the base.
        auto expected = base;
        put16(expected, cell(41, 40), 7);
        CHECK(none->bytes == expected);
        // With one, that side's whole element is taken.
        const auto preferMine = mergeWorlds(base, left, right, MergePreference::mine);
        CHECK(preferMine.has_value() && preferMine->conflicts.size() == 3);
        CHECK(preferMine->bytes == left);
        const auto preferTheirs = mergeWorlds(base, left, right, MergePreference::theirs);
        auto expectedTheirs = right;
        put16(expectedTheirs, cell(41, 40), 7);
        CHECK(preferTheirs.has_value() && preferTheirs->bytes == expectedTheirs);
    }

    // What cannot be merged.
    {
        const auto other = patternedWorld(fxRevision12, 6);
        CHECK(!mergeWorlds(base, base, other, MergePreference::none).has_value());
        const auto longer = patternedWorld(fxRevision13, 7);
        CHECK(mergeWorlds(base, longer, base, MergePreference::none).error().find("length") !=
              std::string::npos);
        auto cut = base;
        cut.resize(30'000);
        CHECK(!mergeWorlds(cut, base, base, MergePreference::none).has_value());
        CHECK(!mergeWorlds(base, Bytes{}, base, MergePreference::none).has_value());
        // Old revisions merge too: the result is only ever written to a new file.
        const auto old = patternedWorld(fxRevision10);
        auto oldMine = old;
        put16(oldMine, cell(1, 1), 9);
        const auto merged = mergeWorlds(old, oldMine, old, MergePreference::none);
        CHECK(merged.has_value() && merged->bytes == oldMine);
    }
    return test::finish("merge");
}
