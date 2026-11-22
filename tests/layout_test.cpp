// SPDX-License-Identifier: GPL-3.0-only
#include "world/layout.hpp"

#include "check.hpp"

#include <cstdint>
#include <limits>

int main() {
    using namespace eggbert;

    // Expected numbers are written out literally from the format notes rather
    // than recomputed from production constants.
    struct Expected {
        FormatRevision revision;
        std::int16_t minor;
        bool hasBigDecor;
        std::size_t layers;
        std::size_t objects;
        std::size_t objectsOffset;
        std::size_t objectsBytes;
        std::size_t totalBytes;
        WritePolicy policy;
    };
    const Expected expected[] = {
        {FormatRevision::v1_0, 0, false, 1, 100, 20'868, 4'800, 25'668, WritePolicy::readOnly},
        {FormatRevision::v1_1, 1, true, 2, 100, 40'868, 4'800, 45'668, WritePolicy::readOnly},
        {FormatRevision::v1_2, 2, true, 2, 200, 40'868, 9'600, 50'468, WritePolicy::losslessEdit},
        {FormatRevision::v1_3, 3, true, 2, 200, 40'868, 9'600, 50'468, WritePolicy::losslessEdit},
    };

    CHECK_EQ(knownRevisions.size(), std::size_t{4});
    for (const auto &want : expected) {
        const auto *descriptor = findRevision(want.revision);
        CHECK(descriptor != nullptr);
        if (descriptor == nullptr) {
            continue;
        }
        CHECK_EQ(descriptor->major, std::int16_t{1});
        CHECK_EQ(descriptor->minor, want.minor);
        CHECK_EQ(descriptor->headerBytes, std::size_t{868});
        CHECK_EQ(descriptor->hasBigDecor, want.hasBigDecor);
        CHECK_EQ(descriptor->tileLayerCount, want.layers);
        CHECK_EQ(descriptor->objectCount, want.objects);
        CHECK_EQ(descriptor->objectRecordBytes, std::size_t{48});
        CHECK_EQ(descriptor->expectedBytes, want.totalBytes);
        CHECK(descriptor->writePolicy == want.policy);
        CHECK(!descriptor->rule.empty());
        CHECK_EQ(revisionFromHeader(1, want.minor), want.revision);

        const auto *header = descriptor->section(SectionKind::header);
        const auto *foreground = descriptor->section(SectionKind::foreground);
        const auto *bigDecor = descriptor->section(SectionKind::bigDecor);
        const auto *objects = descriptor->section(SectionKind::objects);
        CHECK(header != nullptr && header->offset == 0 && header->size == 868);
        CHECK(foreground != nullptr && foreground->offset == 868 && foreground->size == 20'000);
        CHECK_EQ(bigDecor != nullptr, want.hasBigDecor);
        if (bigDecor != nullptr) {
            CHECK(bigDecor->offset == 20'868 && bigDecor->size == 20'000);
        }
        CHECK(objects != nullptr && objects->offset == want.objectsOffset &&
              objects->size == want.objectsBytes);
        CHECK_EQ(descriptor->sections().size(), want.hasBigDecor ? std::size_t{4} : std::size_t{3});

        // Sections are contiguous, ordered and end at the expected size.
        std::size_t cursor = 0;
        for (const auto &section : descriptor->sections()) {
            CHECK_EQ(section.offset, cursor);
            cursor = section.end();
        }
        CHECK_EQ(cursor, want.totalBytes);
    }

    CHECK(findRevision(FormatRevision::unknown) == nullptr);

    // Only the four exact pairs are recognized.
    constexpr auto low = std::numeric_limits<std::int16_t>::min();
    constexpr auto high = std::numeric_limits<std::int16_t>::max();
    CHECK(revisionFromHeader(1, 4) == FormatRevision::unknown);
    CHECK(revisionFromHeader(1, -1) == FormatRevision::unknown);
    CHECK(revisionFromHeader(0, 3) == FormatRevision::unknown);
    CHECK(revisionFromHeader(2, 0) == FormatRevision::unknown);
    CHECK(revisionFromHeader(low, low) == FormatRevision::unknown);
    CHECK(revisionFromHeader(high, high) == FormatRevision::unknown);
    CHECK(revisionFromHeader(0x4D42, 0) == FormatRevision::unknown); // "BM" bitmap magic

    CHECK(isLaterThanKnownRevisions(1, 4));
    CHECK(isLaterThanKnownRevisions(2, 0));
    CHECK(isLaterThanKnownRevisions(high, low));
    CHECK(!isLaterThanKnownRevisions(1, 3));
    CHECK(!isLaterThanKnownRevisions(1, -1));
    CHECK(!isLaterThanKnownRevisions(0, 9));
    CHECK(!isLaterThanKnownRevisions(-1, 9));

    CHECK_EQ(layout::mapWidth, std::size_t{100});
    CHECK_EQ(layout::mapHeight, std::size_t{100});
    CHECK_EQ(layout::tileLayerBytes, std::size_t{20'000});
    return test::finish("layout");
}
