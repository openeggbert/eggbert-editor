// SPDX-License-Identifier: GPL-3.0-only
#include "world/offset_map.hpp"

#include "check.hpp"
#include "fixture.hpp"

using namespace eggbert;
using namespace eggbert::test;

int main() {
    const auto bytes = blankWorld(fxRevision13, 6);
    const auto probe = probeWorld(bytes);

    // Header fields at literal offsets.
    {
        const auto minor = locateOffset(probe, 3);
        CHECK(minor.area == OffsetArea::header);
        CHECK(minor.headerField == HeaderField::minRev);
        CHECK_EQ(minor.byteInElement, std::size_t{1});
        CHECK((minor.elementRange == ByteRange{2, 2}));

        const auto reserve1 = locateOffset(probe, 4 + 4 * 17 + 2);
        CHECK(reserve1.headerField == HeaderField::reserve1);
        CHECK_EQ(reserve1.elementIndex, std::size_t{17});
        CHECK_EQ(reserve1.byteInElement, std::size_t{2});
        CHECK((reserve1.elementRange == ByteRange{72, 4}));

        const auto dim = locateOffset(probe, 219);
        CHECK(dim.headerField == HeaderField::dimDecor);
        CHECK_EQ(dim.byteInElement, std::size_t{7});
        CHECK((dim.elementRange == ByteRange{212, 8}));

        CHECK(locateOffset(probe, 222).headerField == HeaderField::music);
        CHECK(locateOffset(probe, 225).headerField == HeaderField::region);

        const auto start = locateOffset(probe, 328 + 8 * 2 + 5);
        CHECK(start.headerField == HeaderField::blupiPos);
        CHECK_EQ(start.elementIndex, std::size_t{2});
        CHECK((start.elementRange == ByteRange{344, 8}));

        const auto name = locateOffset(probe, 376 + 42);
        CHECK(name.headerField == HeaderField::name);
        CHECK_EQ(name.elementIndex, std::size_t{42});
        CHECK((name.elementRange == ByteRange{418, 1}));

        const auto last = locateOffset(probe, 867);
        CHECK(last.headerField == HeaderField::reserve3);
        CHECK_EQ(last.elementIndex, std::size_t{195});
        CHECK_EQ(last.areaOffset, std::size_t{867});
    }
    // Layers: [x][y] order, asymmetric cells.
    {
        const auto first = locateOffset(probe, 868);
        CHECK(first.area == OffsetArea::foreground);
        CHECK((first.cell == CellPos{0, 0}));
        const auto cell = locateOffset(probe, 868 + 200 * 3 + 2 * 70 + 1);
        CHECK(cell.area == OffsetArea::foreground);
        CHECK((cell.cell == CellPos{3, 70}));
        CHECK_EQ(cell.byteInElement, std::size_t{1});
        CHECK((cell.elementRange == ByteRange{1608, 2}));
        const auto lastForeground = locateOffset(probe, 20'867);
        CHECK(lastForeground.area == OffsetArea::foreground);
        CHECK((lastForeground.cell == CellPos{99, 99}));
        const auto big = locateOffset(probe, 20'868 + 200 * 70 + 2 * 3);
        CHECK(big.area == OffsetArea::bigDecor);
        CHECK((big.cell == CellPos{70, 3}));
        CHECK_EQ(big.areaOffset, std::size_t{14'006});
    }
    // Objects.
    {
        const auto type = locateOffset(probe, 40'868);
        CHECK(type.area == OffsetArea::objects);
        CHECK_EQ(type.slot, std::size_t{0});
        CHECK(type.objectField == ObjectField::type);
        const auto end = locateOffset(probe, 40'868 + 48 * 137 + 20 + 6);
        CHECK_EQ(end.slot, std::size_t{137});
        CHECK(end.objectField == ObjectField::posEnd);
        CHECK_EQ(end.byteInElement, std::size_t{6});
        CHECK((end.elementRange == ByteRange{40'868 + 48 * 137 + 20, 8}));
        const auto last = locateOffset(probe, 50'467);
        CHECK_EQ(last.slot, std::size_t{199});
        CHECK(last.objectField == ObjectField::unknown2);
    }
    // Suffix and beyond the input.
    {
        const auto suffix = locateOffset(probe, 50'468 + 5);
        CHECK(suffix.area == OffsetArea::suffix);
        CHECK_EQ(suffix.areaOffset, std::size_t{5});
        CHECK(suffix.elementRange.empty());
        CHECK(locateOffset(probe, 50'474).area == OffsetArea::unmapped);
        CHECK(locateOffset(probe, static_cast<std::size_t>(-1)).area == OffsetArea::unmapped);
    }
    // Revision 1.0: objects follow the foreground directly.
    {
        const auto old = blankWorld(fxRevision10);
        const auto oldProbe = probeWorld(old);
        const auto object = locateOffset(oldProbe, 20'868 + 48 * 99 + 44);
        CHECK(object.area == OffsetArea::objects);
        CHECK_EQ(object.slot, std::size_t{99});
        CHECK(object.objectField == ObjectField::icon);
        CHECK(locateOffset(oldProbe, 25'668).area == OffsetArea::unmapped);
    }
    // Truncated: existing bytes are mapped, missing ones are not.
    {
        auto cut = bytes;
        cut.resize(1000);
        const auto cutProbe = probeWorld(cut);
        CHECK(locateOffset(cutProbe, 999).area == OffsetArea::foreground);
        CHECK(locateOffset(cutProbe, 1000).area == OffsetArea::unmapped);
    }
    // Unknown revision: nothing is mapped.
    {
        const Bytes junk(60'000, std::byte{0x33});
        CHECK(locateOffset(probeWorld(junk), 900).area == OffsetArea::unmapped);
    }

    // Range helpers agree with literal offsets.
    const auto &descriptor = *findRevision(FormatRevision::v1_3);
    CHECK((cellByteRange(descriptor, SectionKind::foreground, {3, 70}).value() ==
           ByteRange{1608, 2}));
    CHECK((cellByteRange(descriptor, SectionKind::bigDecor, {99, 0}).value() ==
           ByteRange{40'668, 2}));
    CHECK(!cellByteRange(descriptor, SectionKind::foreground, {100, 0}).has_value());
    CHECK(!cellByteRange(descriptor, SectionKind::objects, {0, 0}).has_value());
    CHECK(!cellByteRange(*findRevision(FormatRevision::v1_0), SectionKind::bigDecor, {0, 0})
               .has_value());
    CHECK((slotByteRange(descriptor, 199).value() == ByteRange{50'420, 48}));
    CHECK(!slotByteRange(descriptor, 200).has_value());
    CHECK(!slotByteRange(*findRevision(FormatRevision::v1_1), 100).has_value());
    return test::finish("offset_map");
}
