// SPDX-License-Identifier: GPL-3.0-only
//
// Conversion between the two games: only the revision and, on request, the
// values the target does not know change; everything else is byte-identical;
// every value that cannot be carried is listed.
#include "world/game_convert.hpp"

#include "world/doctor.hpp"
#include "world/world_probe.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <algorithm>
#include <cstdio>
#include <set>
#include <string>

using namespace eggbert;
using namespace eggbert::test;

namespace {

constexpr auto first = GameProfile::speedyBlupi1;
constexpr auto second = GameProfile::speedyBlupi2;

const GameConversionNote *find(const std::vector<GameConversionNote> &notes, GameItem item,
                               int value) {
    const auto found = std::ranges::find_if(notes, [&](const GameConversionNote &note) {
        return note.item == item && note.value == value;
    });
    return found == notes.end() ? nullptr : &*found;
}

void object(Bytes &bytes, const FixtureLayout &layout, std::size_t slot, int type, int channel,
            int icon) {
    const auto at = fxSlotOffset(layout, slot);
    put16(bytes, at, type);
    put16(bytes, at + 42, channel);
    put16(bytes, at + 44, icon);
}

} // namespace

int main() {
    // A world both games know entirely: only the revision changes, both ways.
    {
        auto source = blankWorld(fxRevision12, 9);
        put16(source, fxCellOffset(fxForegroundOffset, 3, 4), 40);
        put16(source, fxCellOffset(fxRevision12.bigDecorOffset, 3, 4), 20);
        object(source, fxRevision12, 0, 5, 10, 0);
        put16(source, 224, 2);
        const auto up = convertGame(source, first, second);
        CHECK(up.has_value());
        auto expected = source;
        put16(expected, 2, 3);
        CHECK(up->bytes == expected);
        CHECK(up->notes.empty());
        CHECK_EQ(up->removedCount, std::size_t{0});
        const auto back = convertGame(up->bytes, second, first);
        CHECK(back.has_value());
        CHECK(back->bytes == source);
        CHECK(back->notes.empty());
    }
    // A world already in the target's revision is copied as it is.
    {
        const auto source = blankWorld(fxRevision13);
        const auto same = convertGame(source, first, second);
        CHECK(same.has_value() && same->bytes == source && same->revisionNotes.empty());
    }
    // Old revisions are upgraded to the target's revision on the way.
    {
        const auto source = blankWorld(fxRevision10);
        const auto converted = convertGame(source, first, second);
        CHECK(converted.has_value());
        CHECK_EQ(converted->bytes.size(), std::size_t{50'468});
        CHECK_EQ(get8(converted->bytes, 2), 3u);
        CHECK(!converted->revisionNotes.empty());
    }

    // First game to second: everything is carried; differences are noted.
    {
        auto source = blankWorld(fxRevision12);
        put16(source, fxCellOffset(fxForegroundOffset, 10, 10), 48); // a "Dream" block
        put16(source, fxCellOffset(fxForegroundOffset, 11, 10), 48);
        put16(source, fxCellOffset(fxForegroundOffset, 12, 10), 67);
        object(source, fxRevision12, 4, 1, 1, 50);  // a lift drawn as block 50
        object(source, fxRevision12, 5, 12, 1, 48); // a secret crate drawn as block 48
        object(source, fxRevision12, 6, 12, 1, 32); // an ordinary crate
        object(source, fxRevision12, 7, 58, 10, 0); // named only by the first game's code
        put16(source, 224, 6);                       // "Dreams" there, "Soil" here
        const WorldView view(source, probeWorld(source));
        const auto notes = reviewGameConversion(view, first, second);
        const auto *dream = find(notes, GameItem::foregroundTile, 48);
        CHECK(dream != nullptr && dream->kind == CarryKind::differsInTarget);
        CHECK(dream != nullptr && dream->count == 2 && dream->firstCell == (CellPos{10, 10}));
        CHECK(dream != nullptr && dream->text.find("215") != std::string::npos);
        CHECK(dream != nullptr && dream->provenance.find("games/sb2/src/decor.cpp:675-684") !=
                                      std::string::npos);
        CHECK(find(notes, GameItem::foregroundTile, 67) != nullptr);
        const auto *lift = find(notes, GameItem::objectIcon, 50);
        CHECK(lift != nullptr && lift->firstSlot == std::size_t{4});
        CHECK(find(notes, GameItem::objectIcon, 48) != nullptr);
        CHECK(find(notes, GameItem::objectIcon, 32) == nullptr);
        const auto *region = find(notes, GameItem::region, 6);
        CHECK(region != nullptr && region->kind == CarryKind::differsInTarget);
        const auto *hide = find(notes, GameItem::objectType, 58);
        CHECK(hide != nullptr && hide->kind == CarryKind::unknownToTarget);
        // Unknown values come first.
        CHECK(notes.front().kind == CarryKind::unknownToTarget);
        for (const auto &note : notes) {
            CHECK(!note.text.empty() && !note.provenance.empty() && note.count > 0);
        }

        // Refused while the unknown type is there; kept or removed on request.
        const auto refused = convertGame(source, first, second);
        CHECK(!refused.has_value());
        CHECK(refused.error().error == ConversionError::wouldLoseData);
        CHECK(refused.error().detail.find("1 stored value(s) in 1 place(s)") != std::string::npos);
        const auto kept = convertGame(source, first, second, UnknownPolicy::keep);
        CHECK(kept.has_value());
        auto expected = source;
        put16(expected, 2, 3);
        CHECK(kept->bytes == expected);
        CHECK_EQ(kept->notes.size(), notes.size());
        const auto removed = convertGame(source, first, second, UnknownPolicy::remove);
        CHECK(removed.has_value());
        CHECK_EQ(removed->removedCount, std::size_t{1});
        for (std::size_t byte = 0; byte < 48; ++byte) {
            expected[fxSlotOffset(fxRevision13, 7) + byte] = std::byte{0};
        }
        CHECK(removed->bytes == expected); // the differing values stay as stored
        CHECK(find(removed->notes, GameItem::objectType, 58) == nullptr);
        CHECK(find(removed->notes, GameItem::foregroundTile, 48) != nullptr);
    }

    // Second game to first: what the first game does not have.
    {
        auto source = blankWorld(fxRevision13);
        put16(source, fxCellOffset(fxForegroundOffset, 1, 1), 245);  // last shared tile
        put16(source, fxCellOffset(fxForegroundOffset, 2, 1), 246);  // first new tile
        put16(source, fxCellOffset(fxForegroundOffset, 3, 1), 440);  // last new tile
        put16(source, fxCellOffset(fxForegroundOffset, 4, 1), 246);
        put16(source, fxCellOffset(fxForegroundOffset, 5, 1), 48);   // shown as 215 there, a block here
        put16(source, fxCellOffset(fxRevision13.bigDecorOffset, 1, 1), 53);
        put16(source, fxCellOffset(fxRevision13.bigDecorOffset, 2, 1), 54);
        put16(source, fxCellOffset(fxRevision13.bigDecorOffset, 3, 1), 99);
        put16(source, fxCellOffset(fxRevision13.bigDecorOffset, 4, 1), 203); // animated plant
        object(source, fxRevision13, 0, 5, 10, 0);    // treasure: shared
        object(source, fxRevision13, 1, 44, 10, 0);   // wasp: second game only
        object(source, fxRevision13, 2, 49, 10, 0);   // red key: second game only
        object(source, fxRevision13, 3, 1, 1, 311);   // lift with a new tile picture
        object(source, fxRevision13, 150, 96, 10, 0); // homing bomb
        put16(source, 224, 27);
        putText(source, 376, "Cave");
        const WorldView view(source, probeWorld(source));
        const auto notes = reviewGameConversion(view, second, first);
        const auto *tile = find(notes, GameItem::foregroundTile, 246);
        CHECK(tile != nullptr && tile->kind == CarryKind::unknownToTarget && tile->count == 2);
        CHECK(find(notes, GameItem::foregroundTile, 440) != nullptr);
        CHECK(find(notes, GameItem::foregroundTile, 245) == nullptr);
        const auto *block = find(notes, GameItem::foregroundTile, 48);
        CHECK(block != nullptr && block->kind == CarryKind::differsInTarget);
        CHECK(block != nullptr && block->text.find("draws its own block 48") != std::string::npos);
        CHECK(find(notes, GameItem::bigDecorTile, 54) != nullptr);
        CHECK(find(notes, GameItem::bigDecorTile, 99) != nullptr);
        CHECK(find(notes, GameItem::bigDecorTile, 53) == nullptr);
        CHECK(find(notes, GameItem::bigDecorTile, 203) == nullptr);
        CHECK(find(notes, GameItem::objectType, 44) != nullptr);
        CHECK(find(notes, GameItem::objectType, 49) != nullptr);
        CHECK(find(notes, GameItem::objectType, 96) != nullptr);
        CHECK(find(notes, GameItem::objectType, 5) == nullptr);
        const auto *lift = find(notes, GameItem::objectIcon, 311);
        CHECK(lift != nullptr && lift->kind == CarryKind::differsInTarget);
        const auto *region = find(notes, GameItem::region, 27);
        CHECK(region != nullptr && region->kind == CarryKind::unknownToTarget);

        const auto refused = convertGame(source, second, first);
        CHECK(!refused.has_value() && refused.error().error == ConversionError::wouldLoseData);

        // Kept: only the revision number differs from the source.
        const auto kept = convertGame(source, second, first, UnknownPolicy::keep);
        CHECK(kept.has_value());
        auto expectedKept = source;
        put16(expectedKept, 2, 2);
        CHECK(kept->bytes == expectedKept);
        CHECK(std::ranges::any_of(kept->revisionNotes, [](const std::string &note) {
            return note.find("title") != std::string::npos;
        }));

        // Removed: exactly the unknown cells, slots and the region.
        const auto removed = convertGame(source, second, first, UnknownPolicy::remove);
        CHECK(removed.has_value());
        CHECK_EQ(removed->removedCount, std::size_t{3 + 2 + 3 + 1});
        auto expected = expectedKept;
        for (const std::size_t x : {2u, 3u, 4u}) {
            put16(expected, fxCellOffset(fxForegroundOffset, x, 1), -1);
        }
        for (const std::size_t x : {2u, 3u}) {
            put16(expected, fxCellOffset(fxRevision13.bigDecorOffset, x, 1), -1);
        }
        for (const std::size_t slot : {1u, 2u, 150u}) {
            for (std::size_t byte = 0; byte < 48; ++byte) {
                expected[fxSlotOffset(fxRevision13, slot) + byte] = std::byte{0};
            }
        }
        put16(expected, 224, 2);
        CHECK(removed->bytes == expected);
        // The lift with the unknown picture and the block shown differently
        // are still there and still noted.
        CHECK_EQ(removed->notes.size(), std::size_t{2});
        CHECK(find(removed->notes, GameItem::foregroundTile, 48) != nullptr);
        CHECK(find(removed->notes, GameItem::objectIcon, 311) != nullptr);
        // The source was only read.
        CHECK_EQ(get8(source, 2), 3u);
    }

    // Free slots are not objects, whatever their leftover bytes say.
    {
        auto source = blankWorld(fxRevision13);
        put16(source, fxSlotOffset(fxRevision13, 9) + 44, 311);
        put16(source, fxSlotOffset(fxRevision13, 9) + 42, 1);
        const auto converted = convertGame(source, second, first);
        CHECK(converted.has_value() && converted->notes.empty());
    }

    // What cannot be converted.
    {
        const auto world = blankWorld(fxRevision13);
        CHECK(convertGame(world, first, first).error().error == ConversionError::invalidTarget);
        CHECK(convertGame(world, GameProfile::unknown, first).error().error ==
              ConversionError::invalidTarget);
        CHECK(convertGame(world, first, GameProfile::unknown).error().error ==
              ConversionError::invalidTarget);
        auto truncated = world;
        truncated.resize(40'000);
        CHECK(convertGame(truncated, first, second).error().error ==
              ConversionError::sourceNotConvertible);
        CHECK(convertGame(Bytes{}, first, second).error().error ==
              ConversionError::sourceNotConvertible);
        const WorldView view(world, probeWorld(world));
        CHECK(reviewGameConversion(view, first, first).empty());
        CHECK(reviewGameConversion(view, GameProfile::unknown, second).empty());
    }
    // Every note carries its sentence as a listed pattern that rebuilds to
    // the text, and every listed pattern is produced by these two worlds:
    // one with everything the first game lacks or shows differently, one
    // with what the second game does.
    {
        std::set<std::string> produced;
        const auto listed = gameConversionPatterns();
        const auto review = [&](const Bytes &bytes, GameProfile from, GameProfile to) {
            const WorldView view(bytes, probeWorld(bytes));
            const auto notes = reviewGameConversion(view, from, to);
            for (const auto &note : notes) {
                CHECK(!note.textPattern.empty());
                CHECK(std::ranges::find(listed, note.textPattern) != listed.end());
                const std::string filler[] = {std::to_string(note.value)};
                CHECK(fillPattern(note.textPattern, filler) == note.text);
                produced.insert(std::string(note.textPattern));
            }
            return notes.size();
        };
        const auto object = [](Bytes &bytes, std::size_t slot, int type, int channel, int icon) {
            const auto at = fxSlotOffset(fxRevision13, slot);
            put16(bytes, at, type);
            put16(bytes, at + 42, channel);
            put16(bytes, at + 44, icon);
        };
        auto modern = blankWorld(fxRevision13);
        put16(modern, fxCellOffset(fxForegroundOffset, 1, 1), 300);          // no such tile
        put16(modern, fxCellOffset(fxForegroundOffset, 2, 1), 50);           // shown as 215
        put16(modern, fxCellOffset(fxRevision13.bigDecorOffset, 3, 1), 60);  // no back picture
        put16(modern, 224, 26);                                              // a second-game backdrop
        object(modern, 0, 40, 10, 0);                                        // a second-game type
        object(modern, 1, 1, 1, 300);                                        // a lift with a new tile
        object(modern, 2, 12, 1, 50);                                        // a crate drawn as 215
        CHECK(review(modern, second, first) >= 7);
        auto classic = blankWorld(fxRevision13);
        put16(classic, fxCellOffset(fxForegroundOffset, 2, 1), 50);
        put16(classic, 224, 4); // a backdrop that is another picture there
        object(classic, 0, 58, 10, 0);
        object(classic, 1, 12, 1, 50);
        CHECK(review(classic, first, second) >= 3);
        for (const auto pattern : listed) {
            if (!produced.contains(std::string(pattern))) {
                std::fprintf(stderr, "conversion sentence never produced: %.*s\n",
                             static_cast<int>(pattern.size()), pattern.data());
                CHECK(!"a listed conversion sentence is never produced in this test");
            }
        }
    }
    return test::finish("game_convert");
}
