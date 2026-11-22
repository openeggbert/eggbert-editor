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
    CHECK(exact.contains("Editing and saving are not available."));
    CHECK(!exact.contains("verified world", Qt::CaseInsensitive));

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

    const auto empty = formatProbeReport("e.blp", probeWorld({}));
    CHECK(empty.contains("Revision field: unavailable"));
    CHECK(empty.contains("Not inspectable as a world."));
    return test::finish("probe_report");
}
