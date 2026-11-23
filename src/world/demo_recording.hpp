// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace eggbert {

// A demo recording of the games (`demo%.3d.blp`): the keys a player held
// while playing the world with the same number, as the games record and
// replay them. Evidence in docs/research/evidence.md, "Demo recordings".
//
// The file is a 204-byte header (revision 1.0 and one hundred zero shorts)
// followed by four-byte events: the game step at which the keys changed and
// the keys held from then on. It holds inputs only — no positions.

inline constexpr std::size_t demoHeaderSize = 204;
// The games' buffer; they neither write nor replay more events.
inline constexpr std::size_t demoMaxEvents = 10'000;
inline constexpr std::size_t demoKeyCount = 6;
// One game step at the games' normal speed, in milliseconds (their timer's
// default interval; a line of the games' configuration can change it).
inline constexpr int demoStepMilliseconds = 50;

struct DemoEvent {
    // Game steps since the world started.
    std::int16_t time = 0;
    // Bit set of the keys held: left 1, right 2, up 4, down 8, jump 16, fire 32.
    std::int16_t input = 0;

    friend bool operator==(const DemoEvent &, const DemoEvent &) = default;
};

// "Left", "Right", "Up", "Down", "Jump", "Fire" for bits 0..5.
[[nodiscard]] std::string_view demoKeyName(std::size_t key);
// "none", or the held keys joined with '+' ("Right+Jump").
[[nodiscard]] std::string demoInputName(std::int16_t input);

struct DemoInterval {
    int from = 0;
    int to = 0;

    friend bool operator==(const DemoInterval &, const DemoInterval &) = default;
};

struct DemoRecording {
    std::vector<DemoEvent> events;

    // Refuses, with the reason, anything the games would not have written:
    // another revision, a non-zero reserved header, a size that is not the
    // header plus whole events, more events than the games' buffer, a time
    // that is negative or runs backwards, keys outside the six.
    [[nodiscard]] static std::expected<DemoRecording, std::string>
    parse(std::span<const std::byte> bytes);
    // Exactly the bytes `parse` accepted.
    [[nodiscard]] std::vector<std::byte> write() const;

    // The step of the last event: the recording ends there.
    [[nodiscard]] int lastTime() const;
    // The keys held during a step: those of the last event at or before it;
    // none before the first event and from the last event on.
    [[nodiscard]] std::int16_t inputAt(int step) const;
    // The stretches of steps during which a key is held.
    [[nodiscard]] std::vector<DemoInterval> held(std::size_t key) const;
    // Steps a key is held in total, and how often it goes down.
    [[nodiscard]] std::array<long long, demoKeyCount> heldSteps() const;
    [[nodiscard]] std::array<std::size_t, demoKeyCount> presses() const;
    // Multi-line summary for the command line and the window.
    [[nodiscard]] std::string describe() const;
};

// True when `parse` accepts the bytes and they are not a complete world of
// revision 1.0, whose first four bytes are the same: a complete world is
// always a world.
[[nodiscard]] bool looksLikeDemoRecording(std::span<const std::byte> bytes);

} // namespace eggbert
