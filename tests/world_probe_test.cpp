// SPDX-License-Identifier: GPL-3.0-only
#include "world/world_probe.hpp"

#include "check.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

using Bytes = std::vector<std::byte>;

// Independent fixture construction: sizes and offsets are literal values from
// the format notes, and bytes are placed by hand in little-endian order.
void put16(Bytes &bytes, std::size_t offset, int value) {
    const auto raw = static_cast<std::uint16_t>(value);
    bytes[offset] = static_cast<std::byte>(raw & 0xFF);
    bytes[offset + 1] = static_cast<std::byte>(raw >> 8);
}

Bytes patterned(std::size_t size) {
    Bytes bytes(size);
    for (std::size_t index = 0; index < size; ++index) {
        bytes[index] = static_cast<std::byte>((index * 7 + 3) % 251);
    }
    return bytes;
}

Bytes world(int major, int minor, std::size_t size) {
    auto bytes = patterned(size);
    if (size >= 2) {
        put16(bytes, 0, major);
    }
    if (size >= 4) {
        put16(bytes, 2, minor);
    }
    return bytes;
}

eggbert::WorldProbe probeUnchanged(const Bytes &bytes) {
    const auto snapshot = bytes;
    const auto probe = eggbert::probeWorld(bytes);
    CHECK(std::ranges::equal(bytes, snapshot));
    CHECK_EQ(probe.inputBytes, bytes.size());
    return probe;
}

struct Boundary {
    eggbert::SectionKind kind;
    std::size_t offset;
    std::size_t size;
};

struct Layout {
    int minor;
    eggbert::FormatRevision revision;
    std::size_t total;
    std::vector<Boundary> sections;
};

} // namespace

int main() {
    using namespace eggbert;
    using enum SectionKind;

    const std::vector<Layout> layouts = {
        {0, FormatRevision::v1_0, 25'668,
         {{header, 0, 868}, {foreground, 868, 20'000}, {objects, 20'868, 4'800}}},
        {1, FormatRevision::v1_1, 45'668,
         {{header, 0, 868}, {foreground, 868, 20'000}, {bigDecor, 20'868, 20'000},
          {objects, 40'868, 4'800}}},
        {2, FormatRevision::v1_2, 50'468,
         {{header, 0, 868}, {foreground, 868, 20'000}, {bigDecor, 20'868, 20'000},
          {objects, 40'868, 9'600}}},
        {3, FormatRevision::v1_3, 50'468,
         {{header, 0, 868}, {foreground, 868, 20'000}, {bigDecor, 20'868, 20'000},
          {objects, 40'868, 9'600}}},
    };

    for (const auto &layout : layouts) {
        // Exact expected size.
        {
            const auto probe = probeUnchanged(world(1, layout.minor, layout.total));
            CHECK(probe.status == ProbeStatus::sizeMatches);
            CHECK(probe.candidateRevision == layout.revision);
            CHECK(probe.descriptor != nullptr);
            CHECK_EQ(probe.headerMajor.value(), std::int16_t{1});
            CHECK_EQ(probe.headerMinor.value(), static_cast<std::int16_t>(layout.minor));
            CHECK(!probe.laterThanKnownRevisions);
            CHECK_EQ(probe.expectedBytes.value(), layout.total);
            CHECK_EQ(probe.sections().size(), layout.sections.size());
            CHECK_EQ(probe.completeSectionCount, layout.sections.size());
            CHECK(!probe.firstMissingOffset.has_value());
            CHECK_EQ(probe.missingBytes, std::size_t{0});
            CHECK_EQ(probe.surplusBytes, std::size_t{0});
            CHECK(probe.structuredInspectionSafe);
            CHECK(!probe.partialInspectionAvailable);
            for (std::size_t index = 0; index < layout.sections.size(); ++index) {
                const auto &want = layout.sections[index];
                const auto &got = probe.sections()[index];
                CHECK(got.kind == want.kind);
                CHECK_EQ(got.offset, want.offset);
                CHECK_EQ(got.size, want.size);
                CHECK(got.complete());
                CHECK(probe.section(want.kind) == &got);
            }
            CHECK_EQ(probe.section(bigDecor) != nullptr, layout.minor != 0);
        }

        // One byte before, exactly at and one byte after every section end.
        for (std::size_t index = 0; index < layout.sections.size(); ++index) {
            const auto end = layout.sections[index].offset + layout.sections[index].size;
            for (const std::size_t size : {end - 1, end, end + 1}) {
                const auto probe = probeUnchanged(world(1, layout.minor, size));
                CHECK(probe.candidateRevision == layout.revision);
                std::size_t wantComplete = 0;
                for (std::size_t other = 0; other < layout.sections.size(); ++other) {
                    const auto &want = layout.sections[other];
                    const auto &got = probe.sections()[other];
                    const std::size_t available =
                        size <= want.offset ? 0 : std::min(want.size, size - want.offset);
                    CHECK_EQ(got.availableBytes, available);
                    CHECK_EQ(got.complete(), size >= want.offset + want.size);
                    wantComplete += size >= want.offset + want.size ? 1 : 0;
                }
                CHECK_EQ(probe.completeSectionCount, wantComplete);
                if (size < layout.total) {
                    CHECK(probe.status == ProbeStatus::truncated);
                    CHECK_EQ(probe.firstMissingOffset.value(), size);
                    CHECK_EQ(probe.missingBytes, layout.total - size);
                    CHECK_EQ(probe.surplusBytes, std::size_t{0});
                    CHECK(!probe.structuredInspectionSafe);
                    CHECK_EQ(probe.partialInspectionAvailable, wantComplete > 0);
                } else if (size == layout.total) {
                    CHECK(probe.status == ProbeStatus::sizeMatches);
                    CHECK(probe.structuredInspectionSafe);
                } else {
                    CHECK(probe.status == ProbeStatus::surplus);
                    CHECK_EQ(probe.surplusBytes, size - layout.total);
                    CHECK(!probe.firstMissingOffset.has_value());
                    CHECK(probe.structuredInspectionSafe);
                    CHECK(!probe.partialInspectionAvailable);
                }
            }
        }

        // A header-only fragment: nothing after the revision field is complete.
        {
            const auto probe = probeUnchanged(world(1, layout.minor, 4));
            CHECK(probe.status == ProbeStatus::truncated);
            CHECK_EQ(probe.completeSectionCount, std::size_t{0});
            CHECK(!probe.partialInspectionAvailable);
            CHECK_EQ(probe.firstMissingOffset.value(), std::size_t{4});
            CHECK_EQ(probe.missingBytes, layout.total - 4);
        }

        // Large surplus is retained and counted, never rejected.
        {
            const auto probe = probeUnchanged(world(1, layout.minor, layout.total + 70'001));
            CHECK(probe.status == ProbeStatus::surplus);
            CHECK_EQ(probe.surplusBytes, std::size_t{70'001});
            CHECK_EQ(probe.expectedBytes.value(), layout.total);
        }
    }

    // The revision field itself is truncated at every scalar boundary.
    for (std::size_t size = 0; size < 4; ++size) {
        const auto probe = probeUnchanged(world(1, 3, size));
        CHECK(probe.status == ProbeStatus::revisionUnavailable);
        CHECK(!probe.headerMajor.has_value());
        CHECK(!probe.headerMinor.has_value());
        CHECK(probe.candidateRevision == FormatRevision::unknown);
        CHECK(probe.descriptor == nullptr);
        CHECK(!probe.expectedBytes.has_value());
        CHECK_EQ(probe.firstMissingOffset.value(), size);
        CHECK_EQ(probe.missingBytes, 4 - size);
        CHECK(probe.sections().empty());
        CHECK(!probe.structuredInspectionSafe);
        CHECK(!probe.partialInspectionAvailable);
    }

    // Unknown and later revisions, including signed extremes, at a size that
    // would otherwise match the newest layout.
    struct Unknown {
        int major;
        int minor;
        bool later;
    };
    const Unknown unknowns[] = {
        {1, 4, true},    {2, 0, true},      {2, 3, true},  {32767, -32768, true},
        {1, -1, false},  {0, 3, false},     {0, 0, false}, {-1, 3, false},
        {-32768, 32767, false}, {1, 256, true}, {257, 3, true}, {3, 1, true},
    };
    for (const auto &unknown : unknowns) {
        const auto probe = probeUnchanged(world(unknown.major, unknown.minor, 50'468));
        CHECK(probe.status == ProbeStatus::unknownRevision);
        CHECK_EQ(probe.headerMajor.value(), static_cast<std::int16_t>(unknown.major));
        CHECK_EQ(probe.headerMinor.value(), static_cast<std::int16_t>(unknown.minor));
        CHECK_EQ(probe.laterThanKnownRevisions, unknown.later);
        CHECK(probe.candidateRevision == FormatRevision::unknown);
        CHECK(probe.descriptor == nullptr);
        CHECK(!probe.expectedBytes.has_value());
        CHECK(probe.sections().empty());
        CHECK(!probe.structuredInspectionSafe);
        CHECK(!probe.partialInspectionAvailable);
    }

    // Arbitrary data of each canonical size is not accepted on size alone.
    for (const std::size_t size : {std::size_t{25'668}, std::size_t{45'668}, std::size_t{50'468}}) {
        const auto probe = probeUnchanged(patterned(size));
        CHECK(probe.status == ProbeStatus::unknownRevision);
        CHECK(!probe.structuredInspectionSafe);
    }
    // A bitmap-like file ("BM" magic) of the canonical world size.
    {
        auto bitmap = patterned(50'468);
        bitmap[0] = std::byte{'B'};
        bitmap[1] = std::byte{'M'};
        const auto probe = probeUnchanged(bitmap);
        CHECK(probe.status == ProbeStatus::unknownRevision);
        CHECK_EQ(probe.headerMajor.value(), std::int16_t{0x4D42});
        CHECK(probe.laterThanKnownRevisions);
        CHECK(!probe.structuredInspectionSafe);
    }
    // Sizes belonging to a different revision are reported as a mismatch for
    // the revision the header names; the size never overrides the header.
    {
        const auto shorter = probeUnchanged(world(1, 3, 25'668));
        CHECK(shorter.candidateRevision == FormatRevision::v1_3);
        CHECK(shorter.status == ProbeStatus::truncated);
        CHECK_EQ(shorter.missingBytes, std::size_t{24'800});
        const auto longer = probeUnchanged(world(1, 0, 50'468));
        CHECK(longer.candidateRevision == FormatRevision::v1_0);
        CHECK(longer.status == ProbeStatus::surplus);
        CHECK_EQ(longer.surplusBytes, std::size_t{24'800});
    }

    // Every status explains itself, and both "consistent" statuses state that
    // consistency is not proof.
    for (const auto status :
         {ProbeStatus::revisionUnavailable, ProbeStatus::unknownRevision, ProbeStatus::truncated,
          ProbeStatus::sizeMatches, ProbeStatus::surplus}) {
        CHECK(!probeStatusReason(status).empty());
    }
    CHECK(probeStatusReason(ProbeStatus::sizeMatches).contains("not proof"));
    CHECK(probeStatusReason(ProbeStatus::surplus).contains("not proof"));

    CHECK(probeUnchanged({}).status == ProbeStatus::revisionUnavailable);
    return test::finish("world_probe");
}
