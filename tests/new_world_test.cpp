// SPDX-License-Identifier: GPL-3.0-only
//
// The new world: exactly what the games' editors start with, written out
// field by field here so the builder is checked against an independent
// description, and nothing else set.
#include "world/new_world.hpp"

#include "world/doctor.hpp"
#include "world/world_probe.hpp"
#include "world/world_view.hpp"

#include "check.hpp"
#include "fixture.hpp"

using namespace eggbert;
using namespace eggbert::test;

int main() {
    CHECK(newWorldRevision(GameProfile::speedyBlupi1) == FormatRevision::v1_2);
    CHECK(newWorldRevision(GameProfile::speedyBlupi2) == FormatRevision::v1_3);
    CHECK(newWorldRevision(GameProfile::unknown) == FormatRevision::v1_3);

    for (const auto profile : {GameProfile::speedyBlupi1, GameProfile::speedyBlupi2}) {
        const auto bytes = newWorld(profile);
        const auto &fixture = profile == GameProfile::speedyBlupi1 ? fxRevision12 : fxRevision13;

        // Built independently with the fixture helpers.
        auto expected = blankWorld(fixture);
        put32(expected, 212, 100); // dimDecor
        put32(expected, 216, 100);
        put16(expected, 222, 1); // music
        put16(expected, 224, 2); // region
        for (std::size_t player = 0; player < 4; ++player) {
            put32(expected, 328 + 8 * player, 194);
            put32(expected, 332 + 8 * player, 198);
            put32(expected, 360 + 4 * player, 2);
        }
        put16(expected, fxCellOffset(fxForegroundOffset, 3, 4), 40);
        put16(expected, fxCellOffset(fxForegroundOffset, 4, 4), 38);
        put16(expected, fxCellOffset(fxForegroundOffset, 5, 4), 39);
        const auto object = [&](std::size_t slot, int type, int x, int icon) {
            const auto at = fxSlotOffset(fixture, slot);
            put16(expected, at, type);
            put16(expected, at + 2, 1);
            put16(expected, at + 4, 1);
            for (const std::size_t position : {12u, 20u, 28u}) {
                put32(expected, at + position, x);
                put32(expected, at + position + 4, 196);
            }
            put16(expected, at + 36, 1);
            put16(expected, at + 42, 10);
            put16(expected, at + 44, icon);
        };
        object(0, 5, 258, 0);
        object(1, 7, 322, 29);
        CHECK(bytes == expected);

        // And read back through the views.
        const auto probe = probeWorld(bytes);
        CHECK(probe.status == ProbeStatus::sizeMatches);
        const WorldView view(bytes, probe);
        CHECK_EQ(view.header()->dimDecor().x, 100);
        CHECK_EQ(view.header()->nameLength(), std::size_t{0});
        CHECK_EQ(view.objects()->activeCount(), std::size_t{2});
        CHECK_EQ(*view.foreground()->cell(4, 4), 38);
        CHECK_EQ(*view.bigDecor()->cell(4, 4), -1);
        // The treasure stands on the middle tile, the goal on the right one.
        CHECK_EQ(view.objects()->record(0)->posStart.x / 64, 4);
        CHECK_EQ(view.objects()->record(1)->posStart.x / 64, 5);
        CHECK_EQ(view.objects()->record(0)->posStart.y / 64, 3);

        // World Doctor has nothing to object to.
        const auto report = runDoctor(view, profile);
        CHECK_EQ(report.count(Severity::error), std::size_t{0});
        CHECK_EQ(report.count(Severity::warning), std::size_t{0});
    }
    return test::finish("new_world");
}
