// SPDX-License-Identifier: GPL-3.0-only
#include "world/world_view.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <algorithm>
#include <limits>

using namespace eggbert;
using namespace eggbert::test;

namespace {

constexpr auto i16Min = std::numeric_limits<std::int16_t>::min();
constexpr auto i16Max = std::numeric_limits<std::int16_t>::max();
constexpr auto i32Min = std::numeric_limits<std::int32_t>::min();
constexpr auto i32Max = std::numeric_limits<std::int32_t>::max();

void checkFieldTables() {
    // Header fields: literal offsets and sizes from the format notes.
    struct Want {
        HeaderField field;
        std::size_t offset;
        std::size_t size;
    };
    const Want header[] = {
        {HeaderField::majRev, 0, 2},      {HeaderField::minRev, 2, 2},
        {HeaderField::reserve1, 4, 200},  {HeaderField::posDecor, 204, 8},
        {HeaderField::dimDecor, 212, 8},  {HeaderField::world, 220, 2},
        {HeaderField::music, 222, 2},     {HeaderField::region, 224, 2},
        {HeaderField::reserve2, 226, 102}, {HeaderField::blupiPos, 328, 32},
        {HeaderField::blupiDir, 360, 16}, {HeaderField::name, 376, 100},
        {HeaderField::reserve3, 476, 392},
    };
    std::size_t cursor = 0;
    for (const auto &want : header) {
        const auto &descriptor = headerField(want.field);
        CHECK(descriptor.field == want.field);
        CHECK_EQ(descriptor.offset, want.offset);
        CHECK_EQ(descriptor.size(), want.size);
        CHECK_EQ(descriptor.offset, cursor); // contiguous, no gaps or overlap
        cursor += descriptor.size();
    }
    CHECK_EQ(cursor, std::size_t{868});
    CHECK((headerField(HeaderField::blupiPos).elementRange(3) == ByteRange{352, 8}));
    CHECK((headerField(HeaderField::reserve3).elementRange(195) == ByteRange{866, 2}));

    struct WantObject {
        ObjectField field;
        std::size_t offset;
        std::size_t size;
    };
    const WantObject object[] = {
        {ObjectField::type, 0, 2},          {ObjectField::stepAdvance, 2, 2},
        {ObjectField::stepRecede, 4, 2},    {ObjectField::timeStopStart, 6, 2},
        {ObjectField::timeStopEnd, 8, 2},   {ObjectField::unknown1, 10, 2},
        {ObjectField::posStart, 12, 8},     {ObjectField::posEnd, 20, 8},
        {ObjectField::posCurrent, 28, 8},   {ObjectField::step, 36, 2},
        {ObjectField::time, 38, 2},         {ObjectField::phase, 40, 2},
        {ObjectField::channel, 42, 2},      {ObjectField::icon, 44, 2},
        {ObjectField::unknown2, 46, 2},
    };
    cursor = 0;
    for (const auto &want : object) {
        const auto &descriptor = objectField(want.field);
        CHECK(descriptor.field == want.field);
        CHECK_EQ(descriptor.offset, want.offset);
        CHECK_EQ(descriptor.size(), want.size);
        CHECK_EQ(descriptor.offset, cursor);
        cursor += descriptor.size();
    }
    CHECK_EQ(cursor, std::size_t{48});
}

void checkHeader() {
    auto bytes = blankWorld(fxRevision13);
    // reserve1: first, middle and last 32-bit elements.
    put32(bytes, 4, 0x11223344);
    put32(bytes, 4 + 4 * 17, -5);
    put32(bytes, 200, i32Min);
    put32(bytes, 204, -640); // posDecor.x
    put32(bytes, 208, 4800); // posDecor.y
    put32(bytes, 212, i32Max); // dimDecor.x
    put32(bytes, 216, 0);    // dimDecor.y
    put16(bytes, 220, -7);   // world
    put16(bytes, 222, 9);    // music
    put16(bytes, 224, i16Min); // region
    put16(bytes, 226, 0x7ABC); // reserve2[0]
    put16(bytes, 326, -2);     // reserve2[50]
    for (int index = 0; index < 4; ++index) {
        put32(bytes, 328 + 8 * index, 1000 + index);    // blupiPos[i].x
        put32(bytes, 332 + 8 * index, -2000 - index);   // blupiPos[i].y
        put32(bytes, 360 + 4 * index, index % 2 == 0 ? 1 : -1); // blupiDir[i]
    }
    putText(bytes, 376, "Asym");
    put8(bytes, 380, 0);
    putText(bytes, 381, "old tail"); // padding after the terminator
    put8(bytes, 475, 0xEE);          // last name byte
    put16(bytes, 476, i16Max);       // reserve3[0]
    put16(bytes, 866, -32000);       // reserve3[195]
    const auto snapshot = bytes;

    const WorldView view(bytes);
    const auto header = view.header();
    CHECK(header.has_value());
    if (!header) {
        return;
    }
    CHECK_EQ(header->majRev(), std::int16_t{1});
    CHECK_EQ(header->minRev(), std::int16_t{3});
    CHECK_EQ(header->reserve1(0).value(), std::int32_t{0x11223344});
    CHECK_EQ(header->reserve1(17).value(), std::int32_t{-5});
    CHECK_EQ(header->reserve1(49).value(), i32Min);
    CHECK(!header->reserve1(50).has_value());
    CHECK((header->posDecor() == Point{-640, 4800}));
    CHECK((header->dimDecor() == Point{i32Max, 0}));
    CHECK_EQ(header->world(), std::int16_t{-7});
    CHECK_EQ(header->music(), std::int16_t{9});
    CHECK_EQ(header->region(), i16Min);
    CHECK_EQ(header->reserve2(0).value(), std::int16_t{0x7ABC});
    CHECK_EQ(header->reserve2(50).value(), std::int16_t{-2});
    CHECK(!header->reserve2(51).has_value());
    for (int index = 0; index < 4; ++index) {
        CHECK((header->blupiPos(index).value() == Point{1000 + index, -2000 - index}));
        CHECK_EQ(header->blupiDir(index).value(), index % 2 == 0 ? 1 : -1);
    }
    CHECK(!header->blupiPos(4).has_value());
    CHECK(!header->blupiDir(4).has_value());
    CHECK_EQ(header->reserve3(0).value(), i16Max);
    CHECK_EQ(header->reserve3(195).value(), std::int16_t{-32000});
    CHECK(!header->reserve3(196).has_value());

    // Name: text before the terminator and the retained padding after it.
    CHECK_EQ(header->nameBytes().size(), std::size_t{100});
    CHECK_EQ(header->nameLength(), std::size_t{4});
    CHECK(header->nameTerminated());
    CHECK_EQ(header->nameText().size(), std::size_t{4});
    CHECK(header->nameText()[0] == std::byte{'A'} && header->nameText()[3] == std::byte{'m'});
    CHECK_EQ(header->namePadding().size(), std::size_t{95});
    CHECK(header->namePadding()[0] == std::byte{'o'});
    CHECK(header->namePadding()[94] == std::byte{0xEE});
    CHECK(!header->namePaddingIsZero());
    CHECK(!header->reservedIsZero(HeaderField::reserve1));
    CHECK(!header->reservedIsZero(HeaderField::reserve2));
    CHECK(!header->reservedIsZero(HeaderField::reserve3));
    CHECK_EQ(header->fieldBytes(HeaderField::reserve3).size(), std::size_t{392});
    CHECK(std::ranges::equal(bytes, snapshot));

    // Unterminated name: all 100 bytes are text and there is no padding.
    auto full = blankWorld(fxRevision12);
    for (std::size_t offset = 376; offset < 476; ++offset) {
        put8(full, offset, 'x');
    }
    const WorldView fullView(full);
    CHECK_EQ(fullView.header()->nameLength(), std::size_t{100});
    CHECK(!fullView.header()->nameTerminated());
    CHECK(fullView.header()->namePadding().empty());
    CHECK(fullView.header()->namePaddingIsZero());
    CHECK(fullView.header()->reservedIsZero(HeaderField::reserve1));

    // Empty name with clean padding.
    const auto blank = blankWorld(fxRevision13);
    const WorldView blankView(blank);
    CHECK_EQ(blankView.header()->nameLength(), std::size_t{0});
    CHECK_EQ(blankView.header()->namePadding().size(), std::size_t{99});
    CHECK(blankView.header()->namePaddingIsZero());
}

void checkLayers() {
    for (const auto &layout : fxRevisions) {
        auto bytes = blankWorld(layout, 5);
        // Asymmetric content: every marked cell differs from its transpose.
        put16(bytes, fxCellOffset(fxForegroundOffset, 0, 0), 11);
        put16(bytes, fxCellOffset(fxForegroundOffset, 99, 0), 22);
        put16(bytes, fxCellOffset(fxForegroundOffset, 0, 99), 33);
        put16(bytes, fxCellOffset(fxForegroundOffset, 99, 99), 44);
        put16(bytes, fxCellOffset(fxForegroundOffset, 3, 70), i16Min);
        put16(bytes, fxCellOffset(fxForegroundOffset, 70, 3), i16Max);
        if (layout.hasBigDecor) {
            put16(bytes, fxCellOffset(layout.bigDecorOffset, 0, 0), 501);
            put16(bytes, fxCellOffset(layout.bigDecorOffset, 99, 0), 502);
            put16(bytes, fxCellOffset(layout.bigDecorOffset, 0, 99), 503);
            put16(bytes, fxCellOffset(layout.bigDecorOffset, 99, 99), 504);
            put16(bytes, fxCellOffset(layout.bigDecorOffset, 12, 34), -2);
        }
        const auto snapshot = bytes;
        const WorldView view(bytes);

        const auto foreground = view.foreground();
        CHECK(foreground.has_value());
        if (!foreground) {
            continue;
        }
        CHECK(foreground->kind() == SectionKind::foreground);
        CHECK_EQ(foreground->fileOffset(), std::size_t{868});
        CHECK_EQ(foreground->cell(0, 0).value(), std::int16_t{11});
        CHECK_EQ(foreground->cell(99, 0).value(), std::int16_t{22});
        CHECK_EQ(foreground->cell(0, 99).value(), std::int16_t{33});
        CHECK_EQ(foreground->cell(99, 99).value(), std::int16_t{44});
        CHECK_EQ(foreground->cell(3, 70).value(), i16Min);
        CHECK_EQ(foreground->cell(70, 3).value(), i16Max);
        CHECK_EQ(foreground->cell(1, 0).value(), std::int16_t{-1});
        CHECK(!foreground->cell(100, 0).has_value());
        CHECK(!foreground->cell(0, 100).has_value());
        CHECK(!foreground->cellRange(100, 99).has_value());
        // Literal offsets: (99,0) is in the last 200-byte column block.
        CHECK((foreground->cellRange(0, 0).value() == ByteRange{868, 2}));
        CHECK((foreground->cellRange(0, 99).value() == ByteRange{868 + 198, 2}));
        CHECK((foreground->cellRange(99, 0).value() == ByteRange{868 + 19'800, 2}));
        CHECK((foreground->cellRange(99, 99).value() == ByteRange{868 + 19'998, 2}));
        CHECK((foreground->cellRange(3, 70).value() == ByteRange{868 + 740, 2}));

        const auto big = view.bigDecor();
        CHECK_EQ(big.has_value(), layout.hasBigDecor);
        if (big) {
            CHECK(big->kind() == SectionKind::bigDecor);
            CHECK_EQ(big->fileOffset(), std::size_t{20'868});
            CHECK_EQ(big->cell(0, 0).value(), std::int16_t{501});
            CHECK_EQ(big->cell(99, 0).value(), std::int16_t{502});
            CHECK_EQ(big->cell(0, 99).value(), std::int16_t{503});
            CHECK_EQ(big->cell(99, 99).value(), std::int16_t{504});
            CHECK_EQ(big->cell(12, 34).value(), std::int16_t{-2});
            CHECK_EQ(big->cell(34, 12).value(), std::int16_t{-1});
            CHECK((big->cellRange(12, 34).value() == ByteRange{20'868 + 2'468, 2}));
        }
        CHECK(!view.layer(SectionKind::header).has_value());
        CHECK(!view.layer(SectionKind::objects).has_value());

        CHECK((view.suffixRange() == ByteRange{layout.totalBytes, 5}));
        CHECK_EQ(view.suffixBytes().size(), std::size_t{5});
        CHECK(std::ranges::equal(bytes, snapshot));
    }
}

void checkObjects() {
    for (const auto &layout : fxRevisions) {
        auto bytes = blankWorld(layout);
        const auto last = layout.objectCount - 1;

        // Slot 0: every field distinct, including signed limits.
        const auto slot0 = fxSlotOffset(layout, 0);
        put16(bytes, slot0 + 0, 12);       // type
        put16(bytes, slot0 + 2, -3);       // stepAdvance
        put16(bytes, slot0 + 4, 4);        // stepRecede
        put16(bytes, slot0 + 6, i16Max);   // timeStopStart
        put16(bytes, slot0 + 8, i16Min);   // timeStopEnd
        put16(bytes, slot0 + 10, 0x1357);  // unknown1
        put32(bytes, slot0 + 12, 640);     // posStart.x
        put32(bytes, slot0 + 16, -64);     // posStart.y
        put32(bytes, slot0 + 20, i32Max);  // posEnd.x
        put32(bytes, slot0 + 24, i32Min);  // posEnd.y
        put32(bytes, slot0 + 28, 100'000); // posCurrent.x
        put32(bytes, slot0 + 32, -100'000); // posCurrent.y
        put16(bytes, slot0 + 36, 7);       // step
        put16(bytes, slot0 + 38, 8);       // time
        put16(bytes, slot0 + 40, 9);       // phase
        put16(bytes, slot0 + 42, 10);      // channel
        put16(bytes, slot0 + 44, 311);     // icon
        put16(bytes, slot0 + 46, -9);      // unknown2

        // Slot 1: free (type 0) but carrying opaque bytes.
        const auto slot1 = fxSlotOffset(layout, 1);
        put16(bytes, slot1 + 0, 0);
        put16(bytes, slot1 + 44, 77);
        put32(bytes, slot1 + 28, 123'456);

        // Last slot of this revision: active.
        const auto slotLast = fxSlotOffset(layout, last);
        put16(bytes, slotLast + 0, -4);
        put16(bytes, slotLast + 46, 5);
        const auto snapshot = bytes;

        const WorldView view(bytes);
        const auto pool = view.objects();
        CHECK(pool.has_value());
        if (!pool) {
            continue;
        }
        CHECK_EQ(pool->slotCount(), layout.objectCount);
        CHECK_EQ(pool->fileOffset(), layout.objectsOffset);
        CHECK_EQ(pool->activeCount(), std::size_t{2});
        CHECK_EQ(pool->freeCount(), layout.objectCount - 2);

        const ObjectRecord want0{12,  -3, 4, i16Max, i16Min, 0x1357, {640, -64}, {i32Max, i32Min},
                                 {100'000, -100'000}, 7, 8, 9, 10, 311, -9};
        CHECK(pool->record(0).value() == want0);
        CHECK(!pool->record(0)->isFree());

        const auto free = pool->record(1).value();
        CHECK(free.isFree());
        CHECK_EQ(free.icon, std::int16_t{77});
        CHECK((free.posCurrent == Point{123'456, 0}));

        CHECK_EQ(pool->record(last)->type, std::int16_t{-4});
        CHECK_EQ(pool->record(last)->unknown2, std::int16_t{5});
        CHECK(!pool->record(last + 1).has_value());
        CHECK(!pool->slotRange(layout.objectCount).has_value());
        CHECK(pool->slotBytes(layout.objectCount).empty());
        CHECK(!pool->fieldRange(layout.objectCount, ObjectField::type).has_value());

        CHECK((pool->slotRange(0).value() == ByteRange{layout.objectsOffset, 48}));
        CHECK((pool->slotRange(last).value() ==
               ByteRange{layout.totalBytes - 48, 48}));
        CHECK((pool->fieldRange(1, ObjectField::posCurrent).value() ==
               ByteRange{layout.objectsOffset + 48 + 28, 8}));
        CHECK((pool->fieldRange(last, ObjectField::unknown2).value() ==
               ByteRange{layout.totalBytes - 2, 2}));
        CHECK_EQ(pool->slotBytes(1).size(), std::size_t{48});
        CHECK(pool->slotBytes(1)[44] == std::byte{77});
        CHECK(std::ranges::equal(bytes, snapshot));
    }
}

void checkTruncation() {
    for (const auto &layout : fxRevisions) {
        const auto full = patternedWorld(layout);
        struct Cut {
            std::size_t size;
            bool header;
            bool foreground;
            bool bigDecor;
            bool objects;
        };
        std::vector<Cut> cuts = {
            {4, false, false, false, false},
            {867, false, false, false, false},
            {868, true, false, false, false},
            {869, true, false, false, false},
            {20'867, true, false, false, false},
            {20'868, true, true, false, false},
        };
        if (layout.hasBigDecor) {
            cuts.push_back({40'867, true, true, false, false});
            cuts.push_back({40'868, true, true, true, false});
        }
        cuts.push_back({layout.totalBytes - 1, true, true, layout.hasBigDecor, false});
        cuts.push_back({layout.totalBytes, true, true, layout.hasBigDecor, true});
        cuts.push_back({layout.totalBytes + 1, true, true, layout.hasBigDecor, true});

        for (const auto &cut : cuts) {
            Bytes bytes = full;
            bytes.resize(cut.size, std::byte{0xA5});
            const auto snapshot = bytes;
            const WorldView view(bytes);
            CHECK_EQ(view.header().has_value(), cut.header);
            CHECK_EQ(view.foreground().has_value(), cut.foreground);
            CHECK_EQ(view.bigDecor().has_value(), cut.bigDecor);
            CHECK_EQ(view.objects().has_value(), cut.objects);
            const auto suffix = cut.size > layout.totalBytes ? cut.size - layout.totalBytes : 0;
            CHECK_EQ(view.suffixRange().size, suffix);
            CHECK_EQ(view.suffixBytes().size(), suffix);
            if (suffix != 0) {
                CHECK(view.suffixBytes()[0] == std::byte{0xA5});
            }
            CHECK(std::ranges::equal(bytes, snapshot));
        }
    }

    // Unknown revision and too-short inputs expose no structured view at all.
    auto unknown = patternedWorld(fxRevision13);
    put16(unknown, 2, 4);
    for (const Bytes &bytes : {unknown, Bytes{}, Bytes(3, std::byte{1})}) {
        const WorldView view(bytes);
        CHECK(!view.header().has_value());
        CHECK(!view.foreground().has_value());
        CHECK(!view.bigDecor().has_value());
        CHECK(!view.objects().has_value());
        CHECK(view.suffixRange().empty());
        CHECK(view.suffixBytes().empty());
    }

    // A probe that does not belong to the bytes cannot widen access.
    const auto full = patternedWorld(fxRevision13);
    const auto fullProbe = probeWorld(full);
    Bytes shorter(full.begin(), full.begin() + 900);
    const WorldView mismatched(shorter, fullProbe);
    CHECK(mismatched.header().has_value());
    CHECK(!mismatched.foreground().has_value());
    CHECK(!mismatched.objects().has_value());
}

void checkPatternedDecoding() {
    // Every reserved and padding byte is non-zero in the patterned fixture;
    // decode a few values straight from the fixture bytes by hand.
    const auto bytes = patternedWorld(fxRevision11, 9);
    const WorldView view(bytes);
    const auto handI16 = [&](std::size_t offset) {
        return static_cast<std::int16_t>(
            static_cast<std::uint16_t>(get8(bytes, offset) | (get8(bytes, offset + 1) << 8)));
    };
    CHECK_EQ(view.header()->music(), handI16(222));
    CHECK_EQ(view.header()->reserve2(7).value(), handI16(226 + 14));
    CHECK_EQ(view.foreground()->cell(41, 7).value(), handI16(868 + 8'200 + 14));
    CHECK_EQ(view.bigDecor()->cell(7, 41).value(), handI16(20'868 + 1'400 + 82));
    CHECK_EQ(view.objects()->record(99)->unknown2, handI16(45'668 - 2));
    CHECK_EQ(view.objects()->slotCount(), std::size_t{100});
    CHECK((view.suffixRange() == ByteRange{45'668, 9}));
}

} // namespace

int main() {
    checkFieldTables();
    checkHeader();
    checkLayers();
    checkObjects();
    checkTruncation();
    checkPatternedDecoding();
    return test::finish("world_view");
}
