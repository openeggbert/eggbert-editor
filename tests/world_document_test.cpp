// SPDX-License-Identifier: GPL-3.0-only
#include "world/world_document.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <algorithm>
#include <limits>
#include <string_view>

using namespace eggbert;
using namespace eggbert::test;

namespace {

std::vector<std::byte> text(std::string_view value) {
    std::vector<std::byte> result;
    for (const char character : value) {
        result.push_back(static_cast<std::byte>(character));
    }
    return result;
}

// Positions where two equally sized byte vectors differ, computed here
// independently of the production diff.
std::vector<std::size_t> diffPositions(std::span<const std::byte> left, const Bytes &right) {
    std::vector<std::size_t> positions;
    CHECK_EQ(left.size(), right.size());
    for (std::size_t index = 0; index < std::min(left.size(), right.size()); ++index) {
        if (left[index] != right[index]) {
            positions.push_back(index);
        }
    }
    return positions;
}

bool onlyInside(const std::vector<std::size_t> &positions, std::size_t offset, std::size_t size) {
    return std::ranges::all_of(positions, [&](std::size_t position) {
        return position >= offset && position < offset + size;
    });
}

void checkUntouched() {
    for (const auto &layout : fxRevisions) {
        const auto input = patternedWorld(layout, 11);
        const WorldDocument document(input);
        CHECK(std::ranges::equal(document.originalBytes(), input));
        CHECK(std::ranges::equal(document.savedBytes(), input));
        CHECK(std::ranges::equal(document.currentBytes(), input));
        CHECK(!document.isModified());
        CHECK(!document.canUndo());
        CHECK(!document.canRedo());
        CHECK(document.changedRangesFromOriginal().empty());
        CHECK(document.unsavedRanges().empty());
        CHECK_EQ(document.cleanIndex().value(), std::size_t{0});
        CHECK_EQ(document.isWritable(), layout.minor >= 2);
        CHECK(document.writeAccess() ==
              (layout.minor >= 2 ? WriteAccess::writable : WriteAccess::readOnlyRevision));
        CHECK(!writeAccessReason(document.writeAccess()).empty());
    }
}

void checkReadOnlyDocuments() {
    auto unknown = patternedWorld(fxRevision13);
    put16(unknown, 2, 4);
    auto truncated = patternedWorld(fxRevision13);
    truncated.resize(50'467);
    struct Case {
        Bytes bytes;
        WriteAccess access;
    };
    const Case cases[] = {
        {patternedWorld(fxRevision10), WriteAccess::readOnlyRevision},
        {patternedWorld(fxRevision11, 3), WriteAccess::readOnlyRevision},
        {truncated, WriteAccess::truncated},
        {unknown, WriteAccess::unknownLayout},
        {Bytes{}, WriteAccess::unknownLayout},
        {Bytes(3, std::byte{1}), WriteAccess::unknownLayout},
    };
    for (const auto &entry : cases) {
        WorldDocument document(entry.bytes);
        CHECK(document.writeAccess() == entry.access);
        CHECK(!document.isWritable());

        // A hand-made, otherwise well-formed change is refused.
        if (entry.bytes.size() > 870) {
            Transaction manual;
            manual.label = "manual";
            manual.changes.push_back(
                {868, {entry.bytes[868], entry.bytes[869]}, {std::byte{1}, std::byte{2}}});
            const auto result = document.apply(manual);
            CHECK(!result.has_value());
            CHECK(result.error().error == EditError::readOnly);
        }
        // The builder may compute a change, but applying it is refused.
        EditBuilder builder(document);
        builder.setCell(SectionKind::foreground, 1, 1, 77);
        builder.setMusic(3);
        auto transaction = builder.build("edit");
        if (!transaction.empty()) {
            CHECK(document.apply(transaction).error().error == EditError::readOnly);
        }
        CHECK(std::ranges::equal(document.currentBytes(), entry.bytes));
        CHECK(!document.isModified());
        CHECK_EQ(document.historySize(), std::size_t{0});
    }
}

void checkCellEdits() {
    for (const auto &layout : {fxRevision12, fxRevision13}) {
        const auto input = patternedWorld(layout, 6);
        WorldDocument document(input);

        // One foreground cell: exactly its two bytes at the literal offset.
        {
            EditBuilder builder(document);
            CHECK(builder.setCell(SectionKind::foreground, 3, 70, -12345));
            const auto transaction = builder.build("Set cell");
            CHECK(document.apply(transaction).has_value());
            const auto offset = fxCellOffset(fxForegroundOffset, 3, 70); // 868 + 740
            CHECK_EQ(offset, std::size_t{1608});
            const auto positions = diffPositions(document.currentBytes(), input);
            CHECK(!positions.empty());
            CHECK(onlyInside(positions, offset, 2));
            CHECK_EQ(get8({document.currentBytes().begin(), document.currentBytes().end()},
                          offset),
                     0xC7u); // -12345 = 0xCFC7
            CHECK_EQ(document.view().foreground()->cell(3, 70).value(), std::int16_t{-12345});
            // The transposed cell is untouched.
            CHECK_EQ(document.view().foreground()->cell(70, 3).value(),
                     document.originalView().foreground()->cell(70, 3).value());
        }
        // One bigDecor cell at a corner.
        {
            EditBuilder builder(document);
            CHECK(builder.setCell(SectionKind::bigDecor, 99, 0, 0x0102));
            CHECK(document.apply(builder.build("Set big cell")).has_value());
            const auto positions = diffPositions(document.currentBytes(), input);
            const auto bigOffset = fxCellOffset(layout.bigDecorOffset, 99, 0); // 20868+19800
            CHECK_EQ(bigOffset, std::size_t{40'668});
            CHECK(std::ranges::all_of(positions, [&](std::size_t position) {
                return (position >= 1608 && position < 1610) ||
                       (position >= bigOffset && position < bigOffset + 2);
            }));
        }
        const auto ranges = document.changedRangesFromOriginal();
        CHECK_EQ(ranges.size(), std::size_t{2});
        CHECK(document.isModified());

        // Out-of-map cells and non-layer sections are rejected by the builder.
        EditBuilder builder(document);
        CHECK(!builder.setCell(SectionKind::foreground, 100, 0, 1));
        CHECK(!builder.setCell(SectionKind::foreground, 0, 100, 1));
        CHECK(!builder.setCell(SectionKind::objects, 0, 0, 1));
        CHECK(!builder.setCell(SectionKind::header, 0, 0, 1));
        CHECK(!builder.hasChanges());
        CHECK(builder.build("nothing").empty());

        // Writing the value a cell already holds produces no change at all.
        const auto existing = *document.view().foreground()->cell(5, 5);
        CHECK(builder.setCell(SectionKind::foreground, 5, 5, existing));
        CHECK(!builder.hasChanges());
        CHECK(document.apply(builder.build("no-op")).error().error ==
              EditError::emptyTransaction);

        // A value sharing one byte with the old value changes only one byte.
        const auto low = static_cast<std::int16_t>(existing ^ 0x0001);
        EditBuilder oneByte(document);
        oneByte.setCell(SectionKind::foreground, 5, 5, low);
        const auto narrow = oneByte.build("low byte");
        CHECK_EQ(narrow.changedByteCount(), std::size_t{1});
        CHECK_EQ(narrow.changes.front().offset, fxCellOffset(fxForegroundOffset, 5, 5));
    }

    // Revision 1.0 has no bigDecor layer to edit.
    const WorldDocument old(patternedWorld(fxRevision10));
    EditBuilder builder(old);
    CHECK(!builder.setCell(SectionKind::bigDecor, 0, 0, 1));
}

void checkHeaderEdits() {
    const auto input = patternedWorld(fxRevision13, 4);
    struct Edit {
        const char *label;
        std::size_t offset;
        std::size_t size;
        bool (*apply)(EditBuilder &);
    };
    constexpr auto i32Min = std::numeric_limits<std::int32_t>::min();
    const Edit edits[] = {
        {"music", 222, 2, [](EditBuilder &b) { return b.setMusic(-321); }},
        {"region", 224, 2, [](EditBuilder &b) { return b.setRegion(12345); }},
        {"posDecor", 204, 8, [](EditBuilder &b) { return b.setPosDecor({-777, 888}); }},
        {"dimDecor", 212, 8, [](EditBuilder &b) { return b.setDimDecor({0, 1}); }},
        {"blupiPos0", 328, 8, [](EditBuilder &b) { return b.setBlupiPos(0, {64, -64}); }},
        {"blupiPos3", 352, 8,
         [](EditBuilder &b) { return b.setBlupiPos(3, {std::numeric_limits<std::int32_t>::min(), 9}); }},
        {"blupiDir0", 360, 4, [](EditBuilder &b) { return b.setBlupiDir(0, -2); }},
        {"blupiDir3", 372, 4, [](EditBuilder &b) { return b.setBlupiDir(3, 0x01020304); }},
    };
    for (const auto &edit : edits) {
        WorldDocument document(input);
        EditBuilder builder(document);
        CHECK(edit.apply(builder));
        CHECK(document.apply(builder.build(edit.label)).has_value());
        const auto positions = diffPositions(document.currentBytes(), input);
        CHECK(!positions.empty());
        CHECK(onlyInside(positions, edit.offset, edit.size));
    }
    (void)i32Min;

    // Decoded values after a combined edit.
    WorldDocument document(input);
    EditBuilder builder(document);
    builder.setMusic(7);
    builder.setRegion(-1);
    builder.setDimDecor({0, 5});
    builder.setBlupiPos(2, {1234, -5678});
    builder.setBlupiDir(2, 1);
    CHECK(!builder.setBlupiPos(4, {0, 0}));
    CHECK(!builder.setBlupiDir(4, 0));
    CHECK(document.apply(builder.build("Metadata")).has_value());
    const auto header = document.view().header().value();
    CHECK_EQ(header.music(), std::int16_t{7});
    CHECK_EQ(header.region(), std::int16_t{-1});
    CHECK((header.dimDecor() == Point{0, 5}));
    CHECK((header.blupiPos(2).value() == Point{1234, -5678}));
    CHECK_EQ(header.blupiDir(2).value(), 1);
    CHECK_EQ(document.historySize(), std::size_t{1}); // one transaction, one step
    // Reserved fields and the revision are byte-identical.
    const auto positions = diffPositions(document.currentBytes(), input);
    CHECK(std::ranges::none_of(positions, [](std::size_t position) {
        return position < 204 || (position >= 220 && position < 222) ||
               (position >= 226 && position < 328) || position >= 476;
    }));
}

void checkNameEdits() {
    auto input = blankWorld(fxRevision13);
    putText(input, 376, "Old title");
    put8(input, 385, 0);
    putText(input, 386, "stale-padding");
    put8(input, 475, 0xEE);

    // Shorter name: text plus one terminator; later bytes stay.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        CHECK(builder.setName(text("New")));
        CHECK(document.apply(builder.build("Rename")).has_value());
        const auto positions = diffPositions(document.currentBytes(), input);
        CHECK(onlyInside(positions, 376, 4));
        const auto header = document.view().header().value();
        CHECK_EQ(header.nameLength(), std::size_t{3});
        CHECK(header.nameBytes()[4] == std::byte{'t'}); // "Old title"[4], after the new NUL
        CHECK(header.nameBytes()[10] == std::byte{'s'});
        CHECK(header.nameBytes()[99] == std::byte{0xEE});
    }
    // Longer name: overwrites exactly its own length plus the terminator.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        CHECK(builder.setName(text("A much longer title")));
        CHECK(document.apply(builder.build("Rename")).has_value());
        const auto positions = diffPositions(document.currentBytes(), input);
        CHECK(onlyInside(positions, 376, 20));
        CHECK_EQ(document.view().header()->nameLength(), std::size_t{19});
        CHECK(document.view().header()->nameBytes()[99] == std::byte{0xEE});
    }
    // Same name: no change. Empty name: only the first byte.
    {
        WorldDocument document(input);
        EditBuilder same(document);
        CHECK(same.setName(text("Old title")));
        CHECK(!same.hasChanges());
        EditBuilder empty(document);
        CHECK(empty.setName({}));
        const auto transaction = empty.build("Clear name");
        CHECK_EQ(transaction.changedByteCount(), std::size_t{1});
        CHECK_EQ(transaction.changes.front().offset, std::size_t{376});
    }
    // Limits: 99 bytes fit, 100 do not, embedded NUL is refused.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        CHECK(builder.setName(Bytes(99, std::byte{'z'})));
        CHECK(document.apply(builder.build("Long")).has_value());
        CHECK_EQ(document.view().header()->nameLength(), std::size_t{99});
        CHECK(document.view().header()->nameTerminated());
        CHECK(std::ranges::equal(document.currentBytes().subspan(476),
                                 std::span<const std::byte>(input).subspan(476)));
        EditBuilder tooLong(document);
        CHECK(!tooLong.setName(Bytes(100, std::byte{'z'})));
        CHECK(!tooLong.setName(Bytes{std::byte{'a'}, std::byte{0}, std::byte{'b'}}));
        CHECK(!tooLong.hasChanges());
    }
}

void checkObjectEdits() {
    const auto input = patternedWorld(fxRevision12, 2);
    const auto slot = std::size_t{137};
    const auto base = fxSlotOffset(fxRevision12, slot); // 40868 + 6576
    CHECK_EQ(base, std::size_t{47'444});

    struct Scalar {
        ObjectField field;
        std::size_t offset;
    };
    const Scalar scalars[] = {
        {ObjectField::type, 0},        {ObjectField::stepAdvance, 2},
        {ObjectField::stepRecede, 4},  {ObjectField::timeStopStart, 6},
        {ObjectField::timeStopEnd, 8}, {ObjectField::unknown1, 10},
        {ObjectField::step, 36},       {ObjectField::time, 38},
        {ObjectField::phase, 40},      {ObjectField::channel, 42},
        {ObjectField::icon, 44},       {ObjectField::unknown2, 46},
    };
    for (const auto &scalar : scalars) {
        WorldDocument document(input);
        const auto old = get8(input, base + scalar.offset) |
                         (get8(input, base + scalar.offset + 1) << 8);
        const auto value = static_cast<std::int16_t>(static_cast<std::uint16_t>(~old));
        EditBuilder builder(document);
        CHECK(builder.setObjectValue(slot, scalar.field, value));
        CHECK(!builder.setObjectPoint(slot, scalar.field, {1, 2}));
        CHECK(document.apply(builder.build("field")).has_value());
        const auto positions = diffPositions(document.currentBytes(), input);
        CHECK_EQ(positions.size(), std::size_t{2});
        CHECK(onlyInside(positions, base + scalar.offset, 2));
    }
    struct PointField {
        ObjectField field;
        std::size_t offset;
    };
    const PointField points[] = {
        {ObjectField::posStart, 12}, {ObjectField::posEnd, 20}, {ObjectField::posCurrent, 28}};
    for (const auto &point : points) {
        WorldDocument document(input);
        EditBuilder builder(document);
        CHECK(builder.setObjectPoint(slot, point.field, {-640, 6400}));
        CHECK(!builder.setObjectValue(slot, point.field, 1));
        CHECK(document.apply(builder.build("point")).has_value());
        const auto positions = diffPositions(document.currentBytes(), input);
        CHECK(!positions.empty());
        CHECK(onlyInside(positions, base + point.offset, 8));
        CHECK((document.view().objects()->record(slot).value().*
               (point.field == ObjectField::posStart
                    ? &ObjectRecord::posStart
                    : point.field == ObjectField::posEnd ? &ObjectRecord::posEnd
                                                         : &ObjectRecord::posCurrent) ==
               Point{-640, 6400}));
    }

    // Whole record, last slot; neighbours and suffix untouched.
    {
        WorldDocument document(input);
        const ObjectRecord record{5, 1, 2, 3, 4, 0x55, {10, 20}, {30, 40}, {50, 60},
                                  6, 7, 8, 9, 10, 0x66};
        EditBuilder builder(document);
        CHECK(builder.setObjectRecord(199, record));
        CHECK(!builder.setObjectRecord(200, record));
        CHECK(document.apply(builder.build("record")).has_value());
        CHECK(document.view().objects()->record(199).value() == record);
        const auto positions = diffPositions(document.currentBytes(), input);
        CHECK(onlyInside(positions, 50'468 - 48, 48));
    }
    // Raw slot copy carries opaque bytes exactly.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        CHECK(builder.copyObjectSlot(3, 150));
        CHECK(!builder.copyObjectSlot(3, 200));
        CHECK(!builder.copyObjectSlot(200, 3));
        CHECK(builder.copyObjectSlot(4, 4));
        CHECK(document.apply(builder.build("copy")).has_value());
        CHECK(std::ranges::equal(document.view().objects()->slotBytes(150),
                                 document.originalView().objects()->slotBytes(3)));
        const auto positions = diffPositions(document.currentBytes(), input);
        CHECK(onlyInside(positions, fxSlotOffset(fxRevision12, 150), 48));
    }
    // Slots beyond the pool are rejected.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        CHECK(!builder.setObjectValue(200, ObjectField::type, 1));
        CHECK(!builder.setObjectPoint(200, ObjectField::posStart, {1, 1}));
        CHECK(!builder.hasChanges());
    }
}

void checkRejectedTransactions() {
    const auto input = patternedWorld(fxRevision13, 8);
    WorldDocument document(input);
    const auto at = [&](std::size_t offset) { return input[offset]; };
    const auto change = [&](std::size_t offset, std::size_t size) {
        ByteChange result;
        result.offset = offset;
        for (std::size_t index = 0; index < size; ++index) {
            result.before.push_back(at(offset + index));
            result.after.push_back(at(offset + index) ^ std::byte{0xFF});
        }
        return result;
    };
    const auto expectFailure = [&](Transaction transaction, EditError error,
                                   std::size_t changeIndex) {
        const auto result = document.apply(std::move(transaction));
        CHECK(!result.has_value());
        if (!result) {
            CHECK(result.error().error == error);
            CHECK_EQ(result.error().changeIndex, changeIndex);
        }
        CHECK(std::ranges::equal(document.currentBytes(), input));
        CHECK_EQ(document.historySize(), std::size_t{0});
        CHECK(!document.isModified());
    };
    const auto one = [&](ByteChange value) {
        Transaction transaction;
        transaction.label = "bad";
        transaction.changes.push_back(std::move(value));
        return transaction;
    };

    expectFailure(Transaction{"empty", {}, 0}, EditError::emptyTransaction, 0);

    // Every protected field, at its first and last byte.
    struct Protected {
        std::size_t first;
        std::size_t last;
    };
    const Protected protectedRanges[] = {
        {0, 1},     // majRev
        {2, 3},     // minRev
        {4, 203},   // reserve1
        {220, 221}, // world
        {226, 327}, // reserve2
        {476, 867}, // reserve3
        {50'468, 50'475}, // opaque suffix
    };
    for (const auto &range : protectedRanges) {
        expectFailure(one(change(range.first, 1)), EditError::protectedRange, 0);
        expectFailure(one(change(range.last, 1)), EditError::protectedRange, 0);
    }
    // A change that starts in an editable field but runs into a protected one.
    expectFailure(one(change(224, 4)), EditError::protectedRange, 0);  // region -> reserve2
    expectFailure(one(change(218, 3)), EditError::protectedRange, 0);  // dimDecor -> world
    expectFailure(one(change(50'466, 4)), EditError::protectedRange, 0); // last object -> suffix
    {
        const auto result = document.apply(one(change(224, 4)));
        CHECK_EQ(result.error().offset, std::size_t{226});
    }

    // Beyond the end of the file.
    {
        ByteChange beyond{50'476, {std::byte{0}}, {std::byte{1}}};
        expectFailure(one(beyond), EditError::outsideLayout, 0);
        ByteChange far{std::numeric_limits<std::size_t>::max(), {std::byte{0}}, {std::byte{1}}};
        expectFailure(one(far), EditError::outsideLayout, 0);
    }

    // Malformed: empty or mismatched lengths.
    expectFailure(one(ByteChange{900, {}, {}}), EditError::malformedChange, 0);
    expectFailure(one(ByteChange{900, {at(900)}, {std::byte{1}, std::byte{2}}}),
                  EditError::malformedChange, 0);

    // Stale: `before` does not match the document.
    {
        auto stale = change(900, 2);
        stale.before[1] ^= std::byte{0x01};
        const auto result = document.apply(one(stale));
        CHECK(result.error().error == EditError::staleBeforeBytes);
        CHECK_EQ(result.error().offset, std::size_t{901});
        expectFailure(one(stale), EditError::staleBeforeBytes, 0);
    }

    // Overlapping or unsorted changes; and atomicity: a valid first change is
    // not applied when a later change is invalid.
    {
        Transaction overlapping;
        overlapping.changes = {change(900, 4), change(902, 2)};
        expectFailure(overlapping, EditError::unsortedOrOverlapping, 1);
        Transaction unsorted;
        unsorted.changes = {change(1000, 2), change(900, 2)};
        expectFailure(unsorted, EditError::unsortedOrOverlapping, 1);
        Transaction mixed;
        mixed.changes = {change(900, 2), change(1000, 2), change(476, 2)};
        expectFailure(mixed, EditError::unsortedOrOverlapping, 2);
        Transaction lateProtected;
        lateProtected.changes = {change(204, 2), change(900, 2), change(50'468, 1)};
        expectFailure(lateProtected, EditError::protectedRange, 2);
    }

    // Adjacent, sorted changes are fine.
    Transaction adjacent;
    adjacent.label = "adjacent";
    adjacent.changes = {change(900, 2), change(902, 2)};
    CHECK(document.apply(adjacent).has_value());
    CHECK_EQ(diffPositions(document.currentBytes(), input).size(), std::size_t{4});

    // The published protected ranges match the literal list (merged).
    const auto published = document.protectedRanges();
    CHECK_EQ(published.size(), std::size_t{5});
    if (published.size() == 5) {
        CHECK((published[0] == ByteRange{0, 204}));
        CHECK((published[1] == ByteRange{220, 2}));
        CHECK((published[2] == ByteRange{226, 102}));
        CHECK((published[3] == ByteRange{476, 392}));
        CHECK((published[4] == ByteRange{50'468, 8}));
    }
}

void checkHistory() {
    const auto input = patternedWorld(fxRevision13, 5);
    WorldDocument document(input);
    const auto setCell = [&](std::size_t x, std::size_t y, std::int16_t value, const char *label,
                             std::uint64_t mergeKey = 0) {
        EditBuilder builder(document);
        builder.setCell(SectionKind::foreground, x, y, value);
        return document.apply(builder.build(label, mergeKey));
    };
    const auto cell = [&](std::size_t x, std::size_t y) {
        return *document.view().foreground()->cell(x, y);
    };
    const auto a0 = cell(1, 2);
    const auto b0 = cell(3, 4);

    CHECK(setCell(1, 2, 100, "A").has_value());
    CHECK(setCell(3, 4, 200, "B").has_value());
    CHECK_EQ(document.undoLabel(), std::string_view("B"));
    CHECK(document.redoLabel().empty());
    CHECK(document.undo());
    CHECK_EQ(cell(3, 4), b0);
    CHECK_EQ(cell(1, 2), std::int16_t{100});
    CHECK_EQ(document.redoLabel(), std::string_view("B"));
    CHECK(document.undo());
    CHECK(std::ranges::equal(document.currentBytes(), input));
    CHECK(!document.isModified());
    CHECK(!document.undo());
    CHECK(document.redo());
    CHECK(document.redo());
    CHECK(!document.redo());
    CHECK_EQ(cell(1, 2), std::int16_t{100});
    CHECK_EQ(cell(3, 4), std::int16_t{200});

    // Save, then undo across the clean index and redo back to it.
    document.markSaved();
    CHECK(!document.isModified());
    CHECK_EQ(document.cleanIndex().value(), std::size_t{2});
    CHECK(document.unsavedRanges().empty());
    CHECK_EQ(document.changedRangesFromOriginal().size(), std::size_t{2});
    CHECK(document.undo());
    CHECK(document.isModified());
    CHECK_EQ(document.unsavedRanges().size(), std::size_t{1});
    CHECK((document.unsavedRanges().front() == ByteRange{fxCellOffset(868, 3, 4), 2}) ||
          document.unsavedRanges().front().size == 1);
    CHECK(document.redo());
    CHECK(!document.isModified());
    CHECK_EQ(document.cleanIndex().value(), std::size_t{2});

    // A new edit after undo discards only the redo branch; the clean state
    // then lies on the discarded branch and is unreachable.
    CHECK(document.undo());
    CHECK(setCell(5, 6, 300, "C").has_value());
    CHECK(!document.canRedo());
    CHECK_EQ(document.historySize(), std::size_t{2});
    CHECK(!document.cleanIndex().has_value());
    CHECK(document.isModified());
    CHECK(document.undo());
    CHECK(document.undo());
    CHECK(std::ranges::equal(document.currentBytes(), input));
    CHECK(document.isModified()); // differs from the saved bytes
    CHECK(document.changedRangesFromOriginal().empty());
    CHECK(document.redo());
    CHECK(document.redo());
    CHECK_EQ(cell(5, 6), std::int16_t{300});
    CHECK_EQ(cell(1, 2), std::int16_t{100});
    CHECK_EQ(cell(3, 4), b0);

    // Repeated save keeps the clean index at the current position.
    document.markSaved();
    document.markSaved();
    CHECK_EQ(document.cleanIndex().value(), std::size_t{2});
    CHECK(!document.isModified());

    // Drag merging: same key merges into one step with the earliest
    // `before` and the latest `after`.
    WorldDocument drag(input);
    const auto dragCell = [&](std::size_t x, std::size_t y, std::int16_t value,
                              std::uint64_t key) {
        EditBuilder builder(drag);
        builder.setCell(SectionKind::foreground, x, y, value);
        return drag.apply(builder.build("Pencil", key));
    };
    CHECK(dragCell(10, 10, 1, 77).has_value());
    CHECK(dragCell(11, 10, 1, 77).has_value());
    CHECK(dragCell(10, 10, 2, 77).has_value()); // revisits the first cell
    CHECK_EQ(drag.historySize(), std::size_t{1});
    CHECK_EQ(*drag.view().foreground()->cell(10, 10), std::int16_t{2});
    CHECK(drag.undo());
    CHECK(std::ranges::equal(drag.currentBytes(), input));
    CHECK(drag.redo());
    CHECK_EQ(*drag.view().foreground()->cell(10, 10), std::int16_t{2});
    CHECK_EQ(*drag.view().foreground()->cell(11, 10), std::int16_t{1});

    // A different key, key 0, endMerge(), undo/redo and save all end a merge.
    CHECK(dragCell(12, 10, 1, 78).has_value());
    CHECK_EQ(drag.historySize(), std::size_t{2});
    CHECK(dragCell(13, 10, 1, 0).has_value());
    CHECK(dragCell(14, 10, 1, 0).has_value());
    CHECK_EQ(drag.historySize(), std::size_t{4});
    CHECK(dragCell(15, 10, 1, 79).has_value());
    drag.endMerge();
    CHECK(dragCell(16, 10, 1, 79).has_value());
    CHECK_EQ(drag.historySize(), std::size_t{6});
    drag.markSaved();
    CHECK(dragCell(17, 10, 1, 79).has_value());
    CHECK_EQ(drag.historySize(), std::size_t{7}); // must not merge into the saved step
    CHECK(drag.undo());
    CHECK(!drag.isModified());
    CHECK(drag.redo());
    CHECK(dragCell(18, 10, 1, 79).has_value());
    CHECK_EQ(drag.historySize(), std::size_t{8}); // redo closed the merge

    // A merged drag that returns every byte to its starting value vanishes.
    WorldDocument circular(input);
    const auto start = *circular.view().foreground()->cell(20, 20);
    {
        EditBuilder builder(circular);
        builder.setCell(SectionKind::foreground, 20, 20, static_cast<std::int16_t>(start + 1));
        CHECK(circular.apply(builder.build("Pencil", 5)).has_value());
    }
    {
        EditBuilder builder(circular);
        builder.setCell(SectionKind::foreground, 20, 20, start);
        CHECK(circular.apply(builder.build("Pencil", 5)).has_value());
    }
    CHECK_EQ(circular.historySize(), std::size_t{0});
    CHECK(!circular.canUndo());
    CHECK(std::ranges::equal(circular.currentBytes(), input));

    (void)a0;
}

void checkPreservationUnderManyEdits() {
    // Edit something in every editable area of a fully patterned world, then
    // verify that every byte outside the named ranges equals the input.
    const auto input = patternedWorld(fxRevision13, 33);
    WorldDocument document(input);
    EditBuilder builder(document);
    builder.setMusic(1);
    builder.setRegion(2);
    builder.setPosDecor({3, 4});
    builder.setDimDecor({5, 6});
    for (std::size_t index = 0; index < 4; ++index) {
        builder.setBlupiPos(index, {7, 8});
        builder.setBlupiDir(index, 9);
    }
    builder.setName(text("Preserve"));
    builder.setCell(SectionKind::foreground, 0, 0, 10);
    builder.setCell(SectionKind::foreground, 99, 99, 11);
    builder.setCell(SectionKind::bigDecor, 0, 99, 12);
    builder.setObjectRecord(0, ObjectRecord{});
    builder.setObjectValue(199, ObjectField::type, 13);
    // The builder's view already reflects pending edits.
    CHECK_EQ(builder.view().header()->music(), std::int16_t{1});
    CHECK_EQ(*builder.view().foreground()->cell(99, 99), std::int16_t{11});
    CHECK(document.apply(builder.build("Everything")).has_value());

    struct Allowed {
        std::size_t offset;
        std::size_t size;
    };
    const Allowed allowed[] = {
        {204, 16},  // posDecor, dimDecor
        {222, 4},   // music, region
        {328, 48},  // blupiPos, blupiDir
        {376, 9},   // "Preserve" + NUL
        {868, 2},   // fg (0,0)
        {20'866, 2}, // fg (99,99)
        {20'868 + 198, 2}, // bigDecor (0,99)
        {40'868, 48},      // slot 0
        {50'468 - 48, 2},  // slot 199 type
    };
    for (const auto position : diffPositions(document.currentBytes(), input)) {
        const bool inside = std::ranges::any_of(allowed, [&](const Allowed &range) {
            return position >= range.offset && position < range.offset + range.size;
        });
        CHECK(inside);
    }
    CHECK(std::ranges::equal(document.currentBytes().subspan(50'468),
                             std::span<const std::byte>(input).subspan(50'468)));
    CHECK(std::ranges::equal(document.originalBytes(), input));

    CHECK(document.undo());
    CHECK(std::ranges::equal(document.currentBytes(), input));

    // differingRanges merges runs and ignores equal bytes.
    const Bytes left{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}, std::byte{5}};
    const Bytes right{std::byte{1}, std::byte{9}, std::byte{9}, std::byte{4}, std::byte{9}};
    const auto ranges = differingRanges(left, right);
    CHECK_EQ(ranges.size(), std::size_t{2});
    CHECK((ranges[0] == ByteRange{1, 2}));
    CHECK((ranges[1] == ByteRange{4, 1}));
    CHECK(differingRanges(left, left).empty());
}

} // namespace

int main() {
    checkUntouched();
    checkReadOnlyDocuments();
    checkCellEdits();
    checkHeaderEdits();
    checkNameEdits();
    checkObjectEdits();
    checkRejectedTransactions();
    checkHistory();
    checkPreservationUnderManyEdits();
    return test::finish("world_document");
}
