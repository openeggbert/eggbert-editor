// SPDX-License-Identifier: GPL-3.0-only
#include "world/demo_recording.hpp"

#include <algorithm>

#include "world/layout.hpp"
#include "world/le_reader.hpp"

namespace eggbert {

namespace {

constexpr std::array<std::string_view, demoKeyCount> keyNames{"Left", "Right", "Up",
                                                               "Down", "Jump",  "Fire"};
constexpr int knownKeys = (1 << demoKeyCount) - 1;

void put16(std::vector<std::byte> &out, std::int16_t value) {
    const auto raw = static_cast<std::uint16_t>(value);
    out.push_back(static_cast<std::byte>(raw & 0xffu));
    out.push_back(static_cast<std::byte>(raw >> 8));
}

} // namespace

std::string_view demoKeyName(std::size_t key) {
    return key < keyNames.size() ? keyNames[key] : std::string_view{};
}

std::string demoInputName(std::int16_t input) {
    std::string name;
    for (std::size_t key = 0; key < demoKeyCount; ++key) {
        if ((input & (1 << key)) != 0) {
            if (!name.empty()) {
                name += '+';
            }
            name += keyNames[key];
        }
    }
    return name.empty() ? "none" : name;
}

std::expected<DemoRecording, std::string> DemoRecording::parse(std::span<const std::byte> bytes) {
    if (bytes.size() < demoHeaderSize) {
        return std::unexpected("The file is shorter than a recording's 204-byte header.");
    }
    if (*readI16(bytes, 0) != 1 || *readI16(bytes, 2) != 0) {
        return std::unexpected("The header does not carry revision 1.0.");
    }
    if (std::ranges::any_of(bytes.subspan(4, demoHeaderSize - 4),
                            [](std::byte value) { return value != std::byte{0}; })) {
        return std::unexpected("The reserved part of the header is not zero.");
    }
    const auto body = bytes.size() - demoHeaderSize;
    if (body % 4 != 0) {
        return std::unexpected("The events do not fill whole four-byte records.");
    }
    if (body / 4 > demoMaxEvents) {
        return std::unexpected("There are more than the 10,000 events the games record.");
    }
    DemoRecording recording;
    recording.events.reserve(body / 4);
    int previous = 0;
    for (std::size_t offset = demoHeaderSize; offset < bytes.size(); offset += 4) {
        const DemoEvent event{*readI16(bytes, offset), *readI16(bytes, offset + 2)};
        const auto number = std::to_string((offset - demoHeaderSize) / 4 + 1);
        if (event.time < previous) {
            return std::unexpected("Event " + number + " is earlier than the one before it.");
        }
        if ((event.input & ~knownKeys) != 0) {
            return std::unexpected("Event " + number + " holds keys the games do not have.");
        }
        previous = event.time;
        recording.events.push_back(event);
    }
    return recording;
}

std::vector<std::byte> DemoRecording::write() const {
    std::vector<std::byte> out;
    out.reserve(demoHeaderSize + events.size() * 4);
    put16(out, 1);
    put16(out, 0);
    out.resize(demoHeaderSize, std::byte{0});
    for (const auto &event : events) {
        put16(out, event.time);
        put16(out, event.input);
    }
    return out;
}

int DemoRecording::lastTime() const { return events.empty() ? 0 : events.back().time; }

std::int16_t DemoRecording::inputAt(int step) const {
    if (events.empty() || step >= events.back().time) {
        return 0;
    }
    // The last event whose time is not after the step.
    const auto after = std::ranges::upper_bound(events, step, {}, &DemoEvent::time);
    return after == events.begin() ? std::int16_t{0} : std::prev(after)->input;
}

std::vector<DemoInterval> DemoRecording::held(std::size_t key) const {
    std::vector<DemoInterval> intervals;
    if (key >= demoKeyCount) {
        return intervals;
    }
    // The keys of an event are held until the next event; the last event
    // ends the recording.
    for (std::size_t index = 0; index + 1 < events.size(); ++index) {
        if ((events[index].input & (1 << key)) == 0 ||
            events[index].time == events[index + 1].time) {
            continue;
        }
        if (!intervals.empty() && intervals.back().to == events[index].time) {
            intervals.back().to = events[index + 1].time;
        } else {
            intervals.push_back({events[index].time, events[index + 1].time});
        }
    }
    return intervals;
}

std::array<long long, demoKeyCount> DemoRecording::heldSteps() const {
    std::array<long long, demoKeyCount> steps{};
    for (std::size_t key = 0; key < demoKeyCount; ++key) {
        for (const auto &interval : held(key)) {
            steps[key] += interval.to - interval.from;
        }
    }
    return steps;
}

std::array<std::size_t, demoKeyCount> DemoRecording::presses() const {
    std::array<std::size_t, demoKeyCount> counts{};
    int before = 0;
    for (const auto &event : events) {
        for (std::size_t key = 0; key < demoKeyCount; ++key) {
            if ((event.input & (1 << key)) != 0 && (before & (1 << key)) == 0) {
                ++counts[key];
            }
        }
        before = event.input;
    }
    return counts;
}

std::string DemoRecording::describe() const {
    std::string text = "Demo recording, revision 1.0: " + std::to_string(events.size()) +
                       " event(s) over " + std::to_string(lastTime()) + " game step(s).\n";
    const auto steps = heldSteps();
    const auto downs = presses();
    for (std::size_t key = 0; key < demoKeyCount; ++key) {
        text += std::string(keyNames[key]) + ": pressed " + std::to_string(downs[key]) +
                " time(s), held for " + std::to_string(steps[key]) + " step(s)\n";
    }
    text += "It records the keys held, not where the player was.\n";
    return text;
}

bool looksLikeDemoRecording(std::span<const std::byte> bytes) {
    const auto *first = findRevision(FormatRevision::v1_0);
    if (first != nullptr && bytes.size() == first->expectedBytes) {
        return false;
    }
    return DemoRecording::parse(bytes).has_value();
}

} // namespace eggbert
