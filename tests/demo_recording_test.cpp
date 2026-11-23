// SPDX-License-Identifier: GPL-3.0-only
//
// Demo recordings: the header and event layout written out independently,
// what is refused, the exact round trip, and the key statistics.
#include "world/demo_recording.hpp"

#include "check.hpp"
#include "fixture.hpp"

using namespace eggbert;
using namespace eggbert::test;

namespace {

// Header as the games write it: two shorts 1 and 0, then 200 zero bytes.
Bytes recording(std::initializer_list<std::pair<int, int>> events) {
    Bytes bytes(204, std::byte{0});
    bytes[0] = std::byte{1};
    for (const auto &[time, input] : events) {
        for (const int value : {time, input}) {
            bytes.push_back(static_cast<std::byte>(value & 0xff));
            bytes.push_back(static_cast<std::byte>((value >> 8) & 0xff));
        }
    }
    return bytes;
}

} // namespace

int main() {
    CHECK_EQ(demoHeaderSize, std::size_t{204});
    CHECK_EQ(demoMaxEvents, std::size_t{10'000});
    CHECK_EQ(demoKeyName(0), std::string_view("Left"));
    CHECK_EQ(demoKeyName(5), std::string_view("Fire"));
    CHECK(demoKeyName(6).empty());
    CHECK_EQ(demoInputName(0), std::string("none"));
    CHECK_EQ(demoInputName(2), std::string("Right"));
    CHECK_EQ(demoInputName(18), std::string("Right+Jump"));
    CHECK_EQ(demoInputName(63), std::string("Left+Right+Up+Down+Jump+Fire"));

    // right from step 10; jump added at 20; jump released at 26; right
    // released at 300; left tapped 310..312; end at 400.
    const auto bytes = recording({{10, 2}, {20, 18}, {26, 2}, {300, 0}, {310, 1}, {312, 0},
                                  {312, 16}, {312, 0}, {400, 0}});
    CHECK(looksLikeDemoRecording(bytes));
    const auto parsed = DemoRecording::parse(bytes);
    CHECK(parsed.has_value());
    const auto &demo = *parsed;
    CHECK_EQ(demo.events.size(), std::size_t{9});
    CHECK(demo.events[1] == (DemoEvent{20, 18}));
    CHECK_EQ(demo.lastTime(), 400);
    CHECK(demo.write() == bytes);
    // Held stretches: adjoining events with the key held are one stretch,
    // and a press released within the same step has no length.
    CHECK(demo.held(1) == (std::vector<DemoInterval>{{10, 300}}));
    CHECK(demo.held(4) == (std::vector<DemoInterval>{{20, 26}}));
    CHECK(demo.held(0) == (std::vector<DemoInterval>{{310, 312}}));
    CHECK(demo.held(5).empty());
    CHECK(demo.held(9).empty());
    CHECK_EQ(demoStepMilliseconds, 50);
    CHECK_EQ(demo.inputAt(0), std::int16_t{0});
    CHECK_EQ(demo.inputAt(9), std::int16_t{0});
    CHECK_EQ(demo.inputAt(10), std::int16_t{2});
    CHECK_EQ(demo.inputAt(25), std::int16_t{18});
    CHECK_EQ(demo.inputAt(26), std::int16_t{2});
    CHECK_EQ(demo.inputAt(311), std::int16_t{1});
    CHECK_EQ(demo.inputAt(312), std::int16_t{0}); // the last of several events of one step
    CHECK_EQ(demo.inputAt(399), std::int16_t{0});
    CHECK_EQ(demo.inputAt(400), std::int16_t{0});
    CHECK_EQ(demo.inputAt(5000), std::int16_t{0});
    CHECK_EQ(demo.inputAt(-3), std::int16_t{0});
    const auto steps = demo.heldSteps();
    CHECK_EQ(steps[0], 2LL);
    CHECK_EQ(steps[1], 290LL);
    CHECK_EQ(steps[4], 6LL);
    CHECK_EQ(steps[5], 0LL);
    const auto presses = demo.presses();
    CHECK_EQ(presses[1], std::size_t{1});
    CHECK_EQ(presses[4], std::size_t{2}); // the zero-length press counts as a press
    CHECK_EQ(presses[0], std::size_t{1});
    const auto text = demo.describe();
    CHECK(text.starts_with("Demo recording, revision 1.0: 9 event(s) over 400 game step(s).\n"));
    CHECK(text.find("Right: pressed 1 time(s), held for 290 step(s)\n") != std::string::npos);
    CHECK(text.find("Jump: pressed 2 time(s), held for 6 step(s)\n") != std::string::npos);
    CHECK(text.find("not where the player was") != std::string::npos);

    // A header alone is an empty recording.
    {
        const auto empty = recording({});
        const auto none = DemoRecording::parse(empty);
        CHECK(none.has_value() && none->events.empty());
        CHECK_EQ(none->lastTime(), 0);
        CHECK(none->write() == empty);
        CHECK(looksLikeDemoRecording(empty));
    }
    // What the games would not have written is refused with the reason.
    const auto refused = [](const Bytes &input, std::string_view reason) {
        const auto result = DemoRecording::parse(input);
        return !result.has_value() && result.error().find(reason) != std::string::npos &&
               !looksLikeDemoRecording(input);
    };
    CHECK(refused(Bytes(203, std::byte{0}), "shorter than"));
    CHECK(refused({}, "shorter than"));
    {
        auto other = bytes;
        other[2] = std::byte{2}; // revision 1.2: a world's header, not a recording's
        CHECK(refused(other, "revision 1.0"));
        other = bytes;
        other[0] = std::byte{2};
        CHECK(refused(other, "revision 1.0"));
        other = bytes;
        other[203] = std::byte{1};
        CHECK(refused(other, "reserved"));
        other = bytes;
        other.push_back(std::byte{0});
        CHECK(refused(other, "whole four-byte"));
        CHECK(refused(recording({{10, 2}, {9, 0}}), "Event 2 is earlier"));
        CHECK(refused(recording({{-1, 0}}), "Event 1 is earlier"));
        CHECK(refused(recording({{5, 64}}), "Event 1 holds keys"));
        CHECK(refused(recording({{5, -1}}), "Event 1 holds keys"));
    }
    // The games' buffer is the limit.
    {
        Bytes full = recording({});
        full.resize(204 + 10'000 * 4, std::byte{0});
        CHECK(DemoRecording::parse(full).has_value());
        full.resize(full.size() + 4, std::byte{0});
        CHECK(refused(full, "10,000"));
    }
    // A complete world is always a world, even one whose bytes would pass:
    // 25,668 bytes are the exact size of a revision 1.0 world.
    {
        Bytes ambiguous = recording({});
        ambiguous.resize(25'668, std::byte{0});
        CHECK(DemoRecording::parse(ambiguous).has_value());
        CHECK(!looksLikeDemoRecording(ambiguous));
    }
    // Worlds of every revision are not recordings.
    for (const auto *revision : {&fxRevision10, &fxRevision11, &fxRevision12, &fxRevision13}) {
        CHECK(!looksLikeDemoRecording(blankWorld(*revision)));
    }
    return test::finish("demo_recording");
}
