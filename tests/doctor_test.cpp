// SPDX-License-Identifier: GPL-3.0-only
#include "world/doctor.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <algorithm>
#include <functional>
#include <set>
#include <string>
#include <utility>

using namespace eggbert;
using namespace eggbert::test;

namespace {

constexpr auto sb1 = GameProfile::speedyBlupi1;
constexpr auto sb2 = GameProfile::speedyBlupi2;

// A world no rule objects to, for either game: valid header values, starts
// inside the map, empty layers, empty pool.
Bytes cleanWorld(const FixtureLayout &layout = fxRevision13, std::size_t surplus = 0) {
    auto bytes = blankWorld(layout, surplus);
    put32(bytes, 212, 100);
    put32(bytes, 216, 100);
    put16(bytes, 222, 1);
    put16(bytes, 224, 2);
    for (int index = 0; index < 4; ++index) {
        put32(bytes, 328 + 8 * index, 194);
        put32(bytes, 332 + 8 * index, 198);
        put32(bytes, 360 + 4 * index, 2);
    }
    return bytes;
}

DoctorReport audit(const Bytes &bytes, GameProfile profile) {
    const auto snapshot = bytes;
    const WorldView view(bytes);
    auto report = runDoctor(view, profile);
    CHECK(bytes == snapshot); // the doctor never mutates
    return report;
}

std::set<std::string> ruleIds(const DoctorReport &report) {
    std::set<std::string> ids;
    for (const auto &finding : report.findings) {
        ids.insert(std::string(finding.ruleId));
    }
    return ids;
}

// Applies `mutate` to a clean world and expects exactly the given rules.
DoctorReport expectRules(const std::function<void(Bytes &)> &mutate, GameProfile profile,
                         const std::set<std::string> &expected,
                         const FixtureLayout &layout = fxRevision13) {
    auto bytes = cleanWorld(layout);
    mutate(bytes);
    auto report = audit(bytes, profile);
    const auto got = ruleIds(report);
    if (got != expected) {
        std::string list;
        for (const auto &id : got) {
            list += id + " ";
        }
        std::cerr << "rules reported: " << list << '\n';
    }
    CHECK(got == expected);
    return report;
}

void object(Bytes &bytes, std::size_t slot, int type, Point start, Point end, int channel,
            int icon, int advance = 1, int recede = 1) {
    const auto base = fxSlotOffset(fxRevision13, slot);
    put16(bytes, base, type);
    put16(bytes, base + 2, advance);
    put16(bytes, base + 4, recede);
    put32(bytes, base + 12, start.x);
    put32(bytes, base + 16, start.y);
    put32(bytes, base + 20, end.x);
    put32(bytes, base + 24, end.y);
    put32(bytes, base + 28, start.x);
    put32(bytes, base + 32, start.y);
    put16(bytes, base + 36, 1);
    put16(bytes, base + 42, channel);
    put16(bytes, base + 44, icon);
}

} // namespace

int main() {
    // Negative case for every rule at once: a clean world has no findings
    // under any profile.
    for (const auto profile : {GameProfile::unknown, sb1, sb2}) {
        const auto report = audit(cleanWorld(), profile);
        CHECK(report.findings.empty());
        CHECK_EQ(report.count(Severity::error), std::size_t{0});
    }
    CHECK(audit(cleanWorld(fxRevision12), sb1).findings.empty());

    // Rule catalogue: unique IDs, every reported ID is listed.
    {
        std::set<std::string> ids;
        for (const auto &info : doctorRules()) {
            CHECK(ids.insert(std::string(info.id)).second);
            CHECK(!info.summary.empty());
        }
        CHECK_EQ(ids.size(), std::size_t{30}); // 29 of the 1.0 scope and REACH-CUT-OFF
    }

    // --- Structure -----------------------------------------------------
    {
        const auto tooShort = audit(Bytes(3, std::byte{1}), sb1);
        CHECK((ruleIds(tooShort) == std::set<std::string>{"STRUCT-SHORT"}));
        CHECK((tooShort.findings.front().range == ByteRange{0, 3}));
        CHECK(tooShort.findings.front().severity == Severity::error);
        CHECK(audit(Bytes{}, sb1).first("STRUCT-SHORT") != nullptr);

        auto unknown = cleanWorld();
        put16(unknown, 2, 4);
        const auto later = audit(unknown, sb2);
        CHECK((ruleIds(later) == std::set<std::string>{"STRUCT-UNKNOWN-REVISION"}));
        CHECK(later.findings.front().message.contains("later than every known revision"));
        CHECK((later.findings.front().range == ByteRange{0, 4}));
        put16(unknown, 0, 0);
        CHECK(audit(unknown, sb2).findings.front().message.contains("may not be a world file"));

        // Truncation boundaries: one byte short, and cut inside the header.
        auto cut = cleanWorld();
        cut.resize(50'467);
        const auto truncated = audit(cut, sb1);
        CHECK(truncated.first("STRUCT-TRUNCATED") != nullptr);
        CHECK((truncated.first("STRUCT-TRUNCATED")->range == ByteRange{50'467, 1}));
        CHECK(truncated.first("STRUCT-TRUNCATED")->severity == Severity::error);
        cut.resize(300);
        const auto headerless = audit(cut, sb1);
        CHECK((ruleIds(headerless) == std::set<std::string>{"STRUCT-TRUNCATED"}));
        CHECK((headerless.findings.front().range == ByteRange{300, 50'168}));

        const auto surplus = audit(cleanWorld(fxRevision13, 9), sb1);
        CHECK((ruleIds(surplus) == std::set<std::string>{"STRUCT-SURPLUS"}));
        CHECK((surplus.findings.front().range == ByteRange{50'468, 9}));
        CHECK(surplus.findings.front().severity == Severity::note);

        for (const auto &layout : {fxRevision10, fxRevision11}) {
            const auto old = audit(cleanWorld(layout), sb1);
            CHECK((ruleIds(old) == std::set<std::string>{"STRUCT-READONLY-REVISION"}));
        }
    }

    // --- Header --------------------------------------------------------
    // Music and region: boundary values per game.
    expectRules([](Bytes &b) { put16(b, 222, 10); }, sb1, {});
    expectRules([](Bytes &b) { put16(b, 222, 0); }, sb1, {});
    {
        const auto report = expectRules([](Bytes &b) { put16(b, 222, 11); }, sb1,
                                        {"HDR-MUSIC-RANGE"});
        const auto &finding = report.findings.front();
        CHECK((finding.range == ByteRange{222, 2}));
        CHECK(finding.headerField == HeaderField::music);
        CHECK(finding.severity == Severity::warning);
        CHECK(finding.confidence == Confidence::verifiedRange);
        CHECK(finding.profile == sb1);
        CHECK(!finding.evidence.empty());
        CHECK(!finding.fix.has_value());
    }
    expectRules([](Bytes &b) { put16(b, 222, -1); }, sb2, {"HDR-MUSIC-RANGE"});
    expectRules([](Bytes &b) { put16(b, 224, 23); }, sb1, {});
    expectRules([](Bytes &b) { put16(b, 224, 24); }, sb1, {"HDR-REGION-RANGE"});
    expectRules([](Bytes &b) { put16(b, 224, 24); }, sb2, {});
    expectRules([](Bytes &b) { put16(b, 224, 31); }, sb2, {});
    expectRules([](Bytes &b) { put16(b, 224, 32); }, sb2, {"HDR-REGION-RANGE"});
    // Without a chosen game, game-specific rules stay silent.
    expectRules([](Bytes &b) {
        put16(b, 222, 99);
        put16(b, 224, 99);
    }, GameProfile::unknown, {});

    // Starts: last pixel inside, first pixel outside, negative.
    expectRules([](Bytes &b) {
        put32(b, 328 + 8 * 2, 6399);
        put32(b, 332 + 8 * 2, 6399);
    }, sb1, {});
    {
        const auto report = expectRules([](Bytes &b) { put32(b, 328 + 8 * 2, 6400); }, sb1,
                                        {"HDR-START-OUTSIDE"});
        CHECK((report.findings.front().range == ByteRange{344, 8}));
        CHECK_EQ(report.findings.front().elementIndex, std::size_t{2});
    }
    expectRules([](Bytes &b) { put32(b, 332, -1); }, GameProfile::unknown, {"HDR-START-OUTSIDE"});
    expectRules([](Bytes &b) { put32(b, 360, 1); }, sb1, {});
    {
        const auto report = expectRules([](Bytes &b) { put32(b, 360 + 12, 0); }, sb1,
                                        {"HDR-START-DIRECTION"});
        CHECK((report.findings.front().range == ByteRange{372, 4}));
        CHECK(report.findings.front().severity == Severity::note);
    }

    // Title.
    expectRules([](Bytes &b) { putText(b, 376, std::string(40, 'x')); }, sb1, {});
    expectRules([](Bytes &b) { putText(b, 376, std::string(41, 'x')); }, sb1, {"HDR-NAME-LONG"});
    expectRules([](Bytes &b) { putText(b, 376, std::string(99, 'x')); }, sb1, {"HDR-NAME-LONG"});
    {
        const auto report = expectRules(
            [](Bytes &b) { putText(b, 376, std::string(100, 'x')); }, sb1,
            {"HDR-NAME-UNTERMINATED"});
        const auto &finding = report.findings.front();
        CHECK(finding.severity == Severity::error);
        CHECK((finding.range == ByteRange{376, 100}));
        // The proposed fix replaces exactly the last name byte with NUL.
        CHECK(finding.fix.has_value());
        CHECK_EQ(finding.fix->changes.size(), std::size_t{1});
        CHECK_EQ(finding.fix->changes.front().offset, std::size_t{475});
        CHECK((finding.fix->changes.front().before == Bytes{std::byte{'x'}}));
        CHECK((finding.fix->changes.front().after == Bytes{std::byte{0}}));
        // Applying it through a document resolves the finding.
        auto bytes = cleanWorld();
        putText(bytes, 376, std::string(100, 'x'));
        WorldDocument document(bytes);
        Transaction transaction;
        transaction.label = finding.fix->label;
        transaction.changes = finding.fix->changes;
        CHECK(document.apply(transaction).has_value());
        const auto after = runDoctor(document.view(), sb1);
        CHECK(after.first("HDR-NAME-UNTERMINATED") == nullptr);
        CHECK(after.first("HDR-NAME-LONG") != nullptr);
    }
    {
        // In a revision 1.2 file the title is not read: unterminated is a
        // warning, and a non-empty title is noted as ignored.
        const auto report = expectRules(
            [](Bytes &b) { putText(b, 376, std::string(100, 'x')); }, sb1,
            {"HDR-NAME-UNTERMINATED", "HDR-NAME-IGNORED"}, fxRevision12);
        CHECK(report.first("HDR-NAME-UNTERMINATED")->severity == Severity::warning);
    }
    expectRules([](Bytes &b) { putText(b, 376, "Title"); }, sb1, {"HDR-NAME-IGNORED"},
                fxRevision12);
    expectRules([](Bytes &b) { putText(b, 376, "Title"); }, sb1, {});
    {
        const auto report = expectRules([](Bytes &b) {
            putText(b, 376, "Ab");
            put8(b, 400, 7);
        }, sb1, {"HDR-NAME-PADDING"});
        CHECK((report.findings.front().range == ByteRange{379, 97}));
    }

    // Reserved fields and unusual scroll values are noted, never errors.
    {
        const auto report = expectRules([](Bytes &b) { put32(b, 4, 256); }, sb1,
                                        {"HDR-RESERVED-NONZERO"});
        CHECK((report.findings.front().range == ByteRange{4, 200}));
        CHECK(report.findings.front().severity == Severity::note);
        const auto all = expectRules([](Bytes &b) {
            put32(b, 200, 1);
            put16(b, 326, 1);
            put16(b, 866, 1);
        }, sb1, {"HDR-RESERVED-NONZERO"});
        CHECK_EQ(all.countRule("HDR-RESERVED-NONZERO"), std::size_t{3});
    }
    expectRules([](Bytes &b) { put32(b, 212, 0); }, sb1, {});
    expectRules([](Bytes &b) { put32(b, 216, 37); }, sb1, {"HDR-SCROLL-UNUSUAL"});

    // --- Tiles ---------------------------------------------------------
    {
        const auto report = expectRules([](Bytes &b) {
            put16(b, fxCellOffset(fxForegroundOffset, 3, 70), -2);
            put16(b, fxCellOffset(fxForegroundOffset, 4, 70), -2);
            put16(b, fxCellOffset(fxRevision13.bigDecorOffset, 9, 9), -32768);
        }, GameProfile::unknown, {"TILE-BELOW-EMPTY"});
        CHECK_EQ(report.findings.size(), std::size_t{2}); // grouped per layer and value
        const auto &foreground = report.findings.front();
        CHECK(foreground.layer == SectionKind::foreground);
        CHECK_EQ(foreground.cells.size(), std::size_t{2});
        CHECK((foreground.cells.front() == CellPos{3, 70}));
        CHECK((foreground.range == ByteRange{1608, 2}));
        CHECK(report.findings.back().layer == SectionKind::bigDecor);
    }
    // Foreground boundaries per game: collision data and picture table.
    const auto fgCell = [](int value) {
        return [value](Bytes &b) { put16(b, fxCellOffset(fxForegroundOffset, 10, 20), value); };
    };
    expectRules(fgCell(0), sb1, {});
    expectRules(fgCell(236), sb1, {});
    expectRules(fgCell(237), sb1, {"TILE-NO-COLLISION-DATA"});
    expectRules(fgCell(245), sb1, {"TILE-NO-COLLISION-DATA"});
    {
        const auto report = expectRules(fgCell(246), sb1, {"TILE-NOT-DRAWN"});
        CHECK((report.findings.front().range == ByteRange{868 + 2000 + 40, 2}));
        CHECK(report.findings.front().profile == sb1);
    }
    expectRules(fgCell(246), sb2, {});
    expectRules(fgCell(440), sb2, {});
    expectRules(fgCell(441), sb2, {"TILE-NOT-DRAWN"});
    expectRules(fgCell(441), GameProfile::unknown, {});
    // The SB2 loader remap applies to SB2 only, values 48..67 only.
    expectRules(fgCell(47), sb2, {});
    expectRules(fgCell(48), sb2, {"TILE-SB2-REMAP"});
    expectRules(fgCell(67), sb2, {"TILE-SB2-REMAP"});
    expectRules(fgCell(68), sb2, {});
    expectRules(fgCell(50), sb1, {});
    // bigDecor picture table and the animated plant exception.
    const auto bigCell = [](int value) {
        return [value](Bytes &b) {
            put16(b, fxCellOffset(fxRevision13.bigDecorOffset, 10, 20), value);
        };
    };
    expectRules(bigCell(53), sb1, {});
    expectRules(bigCell(54), sb1, {"TILE-NOT-DRAWN"});
    expectRules(bigCell(99), sb2, {});
    expectRules(bigCell(100), sb2, {"TILE-NOT-DRAWN"});
    expectRules(bigCell(203), sb1, {});
    expectRules(bigCell(203), sb2, {});
    expectRules(bigCell(50), sb2, {}); // the remap is foreground-only

    // Border forms of the adaptive families (needs scrolling on so the
    // playfield is the whole map; cleanWorld sets dimDecor to 100, 100).
    expectRules(fgCell(153), sb1, {});                     // isolated rock in its isolated form
    {
        const auto report = expectRules(fgCell(35), sb1, {"ADAPT-BORDER-FORM"});
        const auto &finding = report.findings.front();
        CHECK(finding.severity == Severity::note);
        CHECK(finding.message.contains("store 35"));
        CHECK(finding.message.contains("(153 at the first"));
        CHECK_EQ(finding.cells.size(), std::size_t{1});
        CHECK(!finding.fix.has_value());
    }
    expectRules([](Bytes &b) {
        // A correct horizontal pair: joined right, joined left.
        put16(b, fxCellOffset(fxForegroundOffset, 10, 20), 40);
        put16(b, fxCellOffset(fxForegroundOffset, 11, 20), 39);
    }, sb1, {});
    expectRules([](Bytes &b) {
        put16(b, fxCellOffset(fxForegroundOffset, 10, 20), 40);
        put16(b, fxCellOffset(fxForegroundOffset, 11, 20), 153);
    }, sb1, {"ADAPT-BORDER-FORM"});
    // A random look of the right form is not reported; values of no family
    // are never judged; without a game nothing is judged.
    expectRules([](Bytes &b) {
        for (const auto &[x, y] : {std::pair{10, 20}, {9, 20}, {11, 20}, {10, 19}, {10, 21}}) {
            put16(b, fxCellOffset(fxForegroundOffset, x, y), 1);
        }
        put16(b, fxCellOffset(fxForegroundOffset, 10, 20), 156);
    }, sb1, {});
    expectRules(fgCell(5), sb1, {});
    expectRules(fgCell(35), GameProfile::unknown, {});
    expectRules(fgCell(261), sb1, {"TILE-NOT-DRAWN"});        // bricks exist only in SB2
    expectRules(fgCell(261), sb2, {"ADAPT-BORDER-FORM"});     // isolated bricks are 283

    // --- Objects -------------------------------------------------------
    const Point cell{640 + 2, 1280 + 4};
    expectRules([&](Bytes &b) { object(b, 0, 5, cell, cell, 10, 0); }, sb1, {});
    {
        const auto report = expectRules([&](Bytes &b) { object(b, 7, 8, cell, cell, 9, 0); }, sb1,
                                        {"OBJ-TYPE-RUNTIME"});
        const auto &finding = report.findings.front();
        CHECK_EQ(finding.slot.value(), std::size_t{7});
        CHECK(finding.objectField == ObjectField::type);
        CHECK((finding.range == ByteRange{40'868 + 48 * 7, 2}));
    }
    expectRules([&](Bytes &b) { object(b, 7, 150, cell, cell, 10, 0); }, sb1, {"OBJ-TYPE-UNKNOWN"});
    // A type that exists only in the second game.
    expectRules([&](Bytes &b) { object(b, 7, 44, {640, 640}, {640, 640}, 10, 195); }, sb1,
                {"OBJ-TYPE-UNKNOWN"});
    expectRules([&](Bytes &b) { object(b, 7, 44, {640, 640}, {640, 640}, 10, 195); }, sb2, {});
    expectRules([&](Bytes &b) { object(b, 7, 150, cell, cell, 10, 0); }, GameProfile::unknown, {});

    // Positions: boundaries of the world.
    expectRules([&](Bytes &b) { object(b, 1, 5, {6399, 6399}, {6399, 6399}, 10, 0); }, sb1, {});
    {
        // Start and end differ and both lie outside; durations are non-zero,
        // so only the position rule applies.
        const auto report = expectRules(
            [&](Bytes &b) { object(b, 1, 5, {6400, 0}, {0, -1}, 10, 0); }, sb1,
            {"OBJ-POSITION-OUTSIDE"});
        CHECK_EQ(report.findings.size(), std::size_t{2});
    }
    {
        const auto report = expectRules(
            [&](Bytes &b) { object(b, 1, 5, {6400, 0}, {6400, 0}, 10, 0); }, sb1,
            {"OBJ-POSITION-OUTSIDE"});
        CHECK_EQ(report.findings.size(), std::size_t{2}); // start and end separately
        CHECK((report.findings[0].range == ByteRange{40'868 + 48 + 12, 8}));
        CHECK((report.findings[1].range == ByteRange{40'868 + 48 + 20, 8}));
    }

    // Channel expectations.
    expectRules([&](Bytes &b) { object(b, 2, 5, cell, cell, 1, 0); }, sb1, {"OBJ-CHANNEL"});
    expectRules([&](Bytes &b) { object(b, 2, 32, {640, 640}, {640, 640}, 11, 64); }, sb1, {});
    expectRules([&](Bytes &b) { object(b, 2, 32, {640, 640}, {640, 640}, 13, 64); }, sb1, {});
    expectRules([&](Bytes &b) { object(b, 2, 32, {640, 640}, {640, 640}, 10, 64); }, sb1,
                {"OBJ-CHANNEL"});
    expectRules([&](Bytes &b) { object(b, 2, 201, cell, cell, 11, 257); }, sb1, {});
    expectRules([&](Bytes &b) { object(b, 2, 201, cell, cell, 12, 257); }, sb1, {"OBJ-CHANNEL"});

    // Lift and crate icons must have a picture.
    expectRules([&](Bytes &b) { object(b, 3, 1, {640, 640}, {640, 640}, 1, 245); }, sb1, {});
    expectRules([&](Bytes &b) { object(b, 3, 1, {640, 640}, {640, 640}, 1, 246); }, sb1,
                {"OBJ-ICON-RANGE"});
    expectRules([&](Bytes &b) { object(b, 3, 12, {640, 640}, {640, 640}, 1, -1); }, sb2,
                {"OBJ-ICON-RANGE"});
    expectRules([&](Bytes &b) { object(b, 3, 1, {640, 640}, {640, 640}, 1, 246); }, sb2, {});
    // Other types' icons are regenerated by the game and are not judged.
    expectRules([&](Bytes &b) { object(b, 3, 5, cell, cell, 10, 9999); }, sb1, {});

    // Paths: zero duration only matters when start and end differ.
    expectRules([&](Bytes &b) { object(b, 4, 1, {640, 640}, {640, 320}, 1, 153, 20, 20); }, sb1, {});
    expectRules([&](Bytes &b) { object(b, 4, 1, {640, 640}, {640, 640}, 1, 153, 0, 0); }, sb1, {});
    {
        const auto report = expectRules(
            [&](Bytes &b) { object(b, 4, 1, {640, 640}, {640, 320}, 1, 153, 20, 0); }, sb1,
            {"OBJ-PATH-ZERO-DURATION"});
        CHECK(report.findings.front().objectField == ObjectField::stepRecede);
        CHECK((report.findings.front().range == ByteRange{40'868 + 48 * 4 + 4, 2}));
    }

    // Pool capacity: hard limit and estimated headroom boundaries.
    const auto fill = [&](std::size_t count) {
        return [&, count](Bytes &b) {
            for (std::size_t slot = 0; slot < count; ++slot) {
                object(b, slot, 5, cell, cell, 10, 0);
            }
        };
    };
    expectRules(fill(192), sb1, {});                      // 8 free: at the SB1 threshold
    expectRules(fill(193), sb1, {"OBJ-LOW-HEADROOM"});    // 7 free
    expectRules(fill(184), sb2, {});                      // 16 free: at the SB2 threshold
    expectRules(fill(185), sb2, {"OBJ-LOW-HEADROOM"});
    expectRules(fill(199), GameProfile::unknown, {});     // an estimate needs a game
    {
        const auto report = expectRules(fill(200), sb1, {"OBJ-POOL-FULL"});
        const auto &finding = report.findings.front();
        CHECK(finding.severity == Severity::warning);
        CHECK(finding.confidence == Confidence::provenLimit);
        CHECK((finding.range == ByteRange{40'868, 9'600}));
        CHECK(expectRules(fill(193), sb1, {"OBJ-LOW-HEADROOM"}).findings.front().confidence ==
              Confidence::estimate);
    }
    expectRules(fill(200), GameProfile::unknown, {"OBJ-POOL-FULL"});

    // Free slots with stale bytes: one summary note.
    {
        const auto report = expectRules([](Bytes &b) {
            put16(b, fxSlotOffset(fxRevision13, 5) + 44, 9);
            put16(b, fxSlotOffset(fxRevision13, 90) + 10, 9);
        }, sb1, {"OBJ-FREE-STALE"});
        CHECK_EQ(report.findings.size(), std::size_t{1});
        CHECK(report.findings.front().message.starts_with("2 free slot(s)"));
        CHECK_EQ(report.findings.front().slot.value(), std::size_t{5});
        CHECK((report.findings.front().range == ByteRange{40'868 + 240, 48}));
    }

    // Determinism and names.
    {
        auto bytes = cleanWorld();
        put16(bytes, 222, 50);
        put16(bytes, fxCellOffset(fxForegroundOffset, 1, 1), 300);
        object(bytes, 0, 150, cell, cell, 10, 0);
        const auto first = audit(bytes, sb1);
        const auto second = audit(bytes, sb1);
        CHECK_EQ(first.findings.size(), second.findings.size());
        for (std::size_t index = 0; index < first.findings.size(); ++index) {
            CHECK(first.findings[index].ruleId == second.findings[index].ruleId);
            CHECK(first.findings[index].message == second.findings[index].message);
            CHECK(first.findings[index].range == second.findings[index].range);
        }
        CHECK_EQ(first.count(Severity::warning), std::size_t{3});
        CHECK(severityName(Severity::warning) == "warning");
        CHECK(!confidenceName(Confidence::estimate).empty());
    }
    return test::finish("doctor");
}
