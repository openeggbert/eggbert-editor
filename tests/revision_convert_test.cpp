// SPDX-License-Identifier: GPL-3.0-only
#include "world/revision_convert.hpp"

#include "world/world_probe.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <algorithm>

using namespace eggbert;
using namespace eggbert::test;

namespace {

bool equalRange(const Bytes &left, std::size_t leftOffset, const Bytes &right,
                std::size_t rightOffset, std::size_t size) {
    return std::equal(left.begin() + static_cast<std::ptrdiff_t>(leftOffset),
                      left.begin() + static_cast<std::ptrdiff_t>(leftOffset + size),
                      right.begin() + static_cast<std::ptrdiff_t>(rightOffset));
}

bool allAre(const Bytes &bytes, std::size_t offset, std::size_t size, unsigned value) {
    return std::all_of(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                       bytes.begin() + static_cast<std::ptrdiff_t>(offset + size),
                       [value](std::byte byte) { return byte == static_cast<std::byte>(value); });
}

bool mentions(const std::vector<std::string> &notes, std::string_view text) {
    return std::ranges::any_of(notes, [&](const std::string &note) {
        return note.find(text) != std::string::npos;
    });
}

} // namespace

int main() {
    // 1.0 -> 1.3: what the games have in memory after loading a 1.0 world.
    {
        const auto source = patternedWorld(fxRevision10); // arbitrary bytes everywhere
        const auto converted = convertRevision(source, FormatRevision::v1_3);
        CHECK(converted.has_value());
        const auto &bytes = converted->bytes;
        CHECK_EQ(bytes.size(), std::size_t{50'468});
        // Header: every byte kept except the minor revision.
        CHECK(equalRange(bytes, 0, source, 0, 2));
        CHECK_EQ(get8(bytes, 2), 3u);
        CHECK_EQ(get8(bytes, 3), 0u);
        CHECK(equalRange(bytes, 4, source, 4, 864));
        // Foreground unchanged, bigDecor empty (-1 everywhere).
        CHECK(equalRange(bytes, 868, source, 868, 20'000));
        CHECK(allAre(bytes, 20'868, 20'000, 0xFF));
        // 100 stored slots kept, 100 free ones added.
        CHECK(equalRange(bytes, 40'868, source, 20'868, 4'800));
        CHECK(allAre(bytes, 45'668, 4'800, 0x00));
        CHECK(!converted->lossy);
        CHECK(mentions(converted->notes, "empty bigDecor layer was added"));
        CHECK(mentions(converted->notes, "100 free object slots were added"));
        const auto probe = probeWorld(bytes);
        CHECK(probe.status == ProbeStatus::sizeMatches);
        CHECK(probe.descriptor != nullptr && probe.descriptor->revision == FormatRevision::v1_3);
        // The source was only read.
        CHECK(source == patternedWorld(fxRevision10));
    }
    // 1.1 -> 1.2 -> 1.1 is lossless both ways.
    {
        const auto source = patternedWorld(fxRevision11);
        const auto up = convertRevision(source, FormatRevision::v1_2);
        CHECK(up.has_value());
        CHECK_EQ(up->bytes.size(), std::size_t{50'468});
        CHECK(equalRange(up->bytes, 868, source, 868, 40'000 + 4'800));
        CHECK(allAre(up->bytes, 45'668, 4'800, 0x00));
        const auto down = convertRevision(up->bytes, FormatRevision::v1_1);
        CHECK(down.has_value());
        CHECK(!down->lossy);
        CHECK(down->bytes == source);
        CHECK(mentions(down->notes, "empty object slots were left out"));
    }
    // Every upgrade can be undone: up and back down gives the source bytes.
    for (const auto &from : fxRevisions) {
        for (const auto &to : fxRevisions) {
            if (to.minor <= from.minor) {
                continue;
            }
            const auto source = patternedWorld(from, 5);
            const auto target = static_cast<FormatRevision>(
                static_cast<int>(FormatRevision::v1_0) + to.minor);
            const auto origin = static_cast<FormatRevision>(
                static_cast<int>(FormatRevision::v1_0) + from.minor);
            const auto up = convertRevision(source, target);
            CHECK(up.has_value());
            CHECK(!up->lossy);
            CHECK_EQ(up->bytes.size(), to.totalBytes + 5);
            const auto down = convertRevision(up->bytes, origin);
            CHECK(down.has_value());
            CHECK(!down->lossy);
            CHECK(down->bytes == source);
        }
    }
    // 1.2 <-> 1.3 differ only in the revision number (and what the games do
    // with the title).
    {
        auto source = blankWorld(fxRevision13);
        putText(source, 376, "My world");
        const auto to12 = convertRevision(source, FormatRevision::v1_2);
        CHECK(to12.has_value());
        auto expected = source;
        put16(expected, 2, 2);
        CHECK(to12->bytes == expected);
        CHECK(mentions(to12->notes, "games read it only from revision 1.3"));
        const auto back = convertRevision(to12->bytes, FormatRevision::v1_3);
        CHECK(back.has_value());
        CHECK(back->bytes == source);
        CHECK(back->notes.empty());
    }
    // Going down refuses to drop anything unless that is accepted.
    {
        auto source = blankWorld(fxRevision13);
        // Empty bigDecor and free upper slots: nothing is lost.
        const auto clean = convertRevision(source, FormatRevision::v1_0);
        CHECK(clean.has_value());
        CHECK_EQ(clean->bytes.size(), std::size_t{25'668});
        CHECK(!clean->lossy);
        CHECK(mentions(clean->notes, "empty bigDecor layer was left out"));
        // A bigDecor cell.
        put16(source, fxCellOffset(fxRevision13.bigDecorOffset, 5, 6), 20);
        auto refused = convertRevision(source, FormatRevision::v1_0);
        CHECK(!refused.has_value());
        CHECK(refused.error().error == ConversionError::wouldLoseData);
        CHECK(refused.error().detail.find("1 bigDecor cell") != std::string::npos);
        CHECK(convertRevision(source, FormatRevision::v1_1).has_value()); // 1.1 has the layer
        const auto accepted = convertRevision(source, FormatRevision::v1_0, true);
        CHECK(accepted.has_value());
        CHECK(accepted->lossy);
        CHECK(mentions(accepted->notes, "Lost: 1 bigDecor cell"));
        // An object in slot 150.
        put16(source, fxCellOffset(fxRevision13.bigDecorOffset, 5, 6), -1);
        put16(source, fxSlotOffset(fxRevision13, 150), 5);
        refused = convertRevision(source, FormatRevision::v1_1);
        CHECK(!refused.has_value());
        CHECK(refused.error().detail.find("1 object(s) are stored in slots 100 to 199") !=
              std::string::npos);
        // Leftover bytes in a free slot count too: they are stored data.
        put16(source, fxSlotOffset(fxRevision13, 150), 0);
        put16(source, fxSlotOffset(fxRevision13, 150) + 44, 77);
        refused = convertRevision(source, FormatRevision::v1_1);
        CHECK(!refused.has_value());
        CHECK(refused.error().detail.find("leftover bytes") != std::string::npos);
        CHECK(convertRevision(source, FormatRevision::v1_1, true)->lossy);
        // An object in slot 99 fits every revision.
        put16(source, fxSlotOffset(fxRevision13, 150) + 44, 0);
        put16(source, fxSlotOffset(fxRevision13, 99), 5);
        const auto fits = convertRevision(source, FormatRevision::v1_1);
        CHECK(fits.has_value() && !fits->lossy);
        CHECK(equalRange(fits->bytes, 40'868, source, 40'868, 4'800));
    }
    // Bytes after the layout are carried over.
    {
        const auto source = patternedWorld(fxRevision10, 37);
        const auto converted = convertRevision(source, FormatRevision::v1_2);
        CHECK(converted.has_value());
        CHECK_EQ(converted->bytes.size(), std::size_t{50'468 + 37});
        CHECK(equalRange(converted->bytes, 50'468, source, 25'668, 37));
        CHECK(mentions(converted->notes, "37 byte(s) after the layout"));
    }
    // What cannot be converted.
    {
        auto truncated = blankWorld(fxRevision13);
        truncated.resize(truncated.size() - 1);
        CHECK(convertRevision(truncated, FormatRevision::v1_2).error().error ==
              ConversionError::sourceNotConvertible);
        auto unknown = blankWorld(fxRevision13);
        put16(unknown, 2, 9);
        CHECK(convertRevision(unknown, FormatRevision::v1_2).error().error ==
              ConversionError::sourceNotConvertible);
        CHECK(convertRevision(Bytes{}, FormatRevision::v1_2).error().error ==
              ConversionError::sourceNotConvertible);
        const auto world = blankWorld(fxRevision12);
        CHECK(convertRevision(world, FormatRevision::v1_2).error().error ==
              ConversionError::invalidTarget);
        CHECK(convertRevision(world, FormatRevision::unknown).error().error ==
              ConversionError::invalidTarget);
    }
    return test::finish("revision_convert");
}
