// SPDX-License-Identifier: GPL-3.0-only
#include "app/probe_report.hpp"

#include "check.hpp"

#include <QCoreApplication>

#include <vector>

namespace {

std::vector<std::byte> world(int minor, std::size_t size) {
    std::vector<std::byte> bytes(size, std::byte{0x5A});
    bytes[0] = std::byte{1};
    bytes[1] = std::byte{0};
    bytes[2] = static_cast<std::byte>(minor);
    bytes[3] = std::byte{0};
    return bytes;
}

} // namespace

int main(int argc, char **argv) {
    using namespace eggbert;
    QCoreApplication app(argc, argv);

    const auto exact = formatProbeReport("a.blp", probeWorld(world(3, 50'468)));
    CHECK(exact.contains("Revision field: 1.3"));
    CHECK(exact.contains("50468 bytes"));
    CHECK(exact.contains("Structured inspection: available."));
    CHECK(exact.contains("not proof"));
    // A complete 1.2 or 1.3 world can be edited; the report says so, and
    // says the opposite for a read-only revision or an incomplete file.
    CHECK(exact.contains("Editing and saving are available for this revision."));
    CHECK(!exact.contains("verified world", Qt::CaseInsensitive));
    CHECK(formatProbeReport("old.blp", probeWorld(world(0, 25'668)))
              .contains("Editing and saving are not available."));
    CHECK(formatProbeReport("cut.blp", probeWorld(world(3, 30'000)))
              .contains("Editing and saving are not available."));

    const auto truncated = formatProbeReport("b.blp", probeWorld(world(2, 20'869)));
    CHECK(truncated.contains("truncated"));
    CHECK(truncated.contains("First missing offset: 20869 (29599 bytes missing)"));
    CHECK(truncated.contains("Structured inspection: complete sections only."));
    CHECK(truncated.contains("bigDecor layer: offset 20868, 1 of 20000 bytes, incomplete"));

    const auto surplus = formatProbeReport("c.blp", probeWorld(world(0, 25'670)));
    CHECK(surplus.contains("Surplus: 2 bytes after offset 25668, retained unchanged"));
    CHECK(!surplus.contains("bigDecor layer:"));

    // A matching-size file without a recognized revision is never presented
    // as an inspectable world.
    const std::vector<std::byte> arbitrary(50'468, std::byte{0x42});
    const auto unrelated = formatProbeReport("d.blp", probeWorld(arbitrary));
    CHECK(unrelated.contains("not inspected as a world"));
    CHECK(unrelated.contains("Structured inspection: unavailable."));
    CHECK(!unrelated.contains("Candidate revision"));

    // Decoded summary of complete sections.
    {
        auto bytes = world(3, 50'468);
        const char name[] = "Caf\xE9\x01";
        for (std::size_t index = 0; index < sizeof(name); ++index) { // includes the NUL
            bytes[376 + index] = static_cast<std::byte>(name[index]);
        }
        bytes[222] = std::byte{4}; // music low byte
        bytes[223] = std::byte{0};
        const WorldView view(bytes);
        const auto summary = formatWorldSummary(view);
        CHECK(summary.contains(QString::fromLatin1("Name: \"Caf\xE9\\x01\" (5 bytes")));
        CHECK(summary.contains("non-zero bytes retained after it"));
        CHECK(summary.contains("music 4,"));
        CHECK(summary.contains("Start 4: position (1515870810, 1515870810)"));
        CHECK(summary.contains("Non-zero reserved header fields (retained): reserve1, reserve2, reserve3"));
        CHECK(summary.contains("Foreground layer: 10000 of 10000 cells differ from -1"));
        CHECK(summary.contains("Object records: 200 stored, 200 with a non-zero type, 0 free"));

        bytes.resize(20'000);
        const WorldView truncated(bytes);
        const auto partial = formatWorldSummary(truncated);
        CHECK(partial.contains("Name:"));
        CHECK(!partial.contains("Foreground layer"));
        CHECK(!partial.contains("Object records"));
        CHECK(formatWorldSummary(WorldView(arbitrary)).isEmpty());
    }
    CHECK_EQ(displayNameBytes(std::vector<std::byte>{std::byte{'a'}, std::byte{'\\'}, std::byte{0x7F},
                                                     std::byte{0x9F}, std::byte{0xA0}}),
             QString::fromUtf8("a\\\\\\x7f\\x9f\u00A0"));

    const auto empty = formatProbeReport("e.blp", probeWorld({}));
    CHECK(empty.contains("Revision field: unavailable"));
    CHECK(empty.contains("Not inspectable as a world."));
    return test::finish("probe_report");
}
