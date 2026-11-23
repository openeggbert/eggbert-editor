// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace eggbert {

// A small synthesizer of the editor's own, so that the games' music (standard
// MIDI files) can be heard without any other program. It reads a MIDI file
// and renders it to sound with simple instrument shapes: the tune, rhythm
// and tempo are the file's, the instruments are approximations by family
// (plucked, sustained, bass, drums). Qt-free and deterministic.

struct MidiNote {
    double start = 0; // seconds
    double end = 0;
    std::uint8_t channel = 0; // 0..15; 9 is percussion
    std::uint8_t key = 0;
    std::uint8_t velocity = 0;
    std::uint8_t program = 0;
    // Channel volume (controller 7) when the note began, 0..127.
    std::uint8_t volume = 100;

    friend bool operator==(const MidiNote &, const MidiNote &) = default;
};

struct MidiSong {
    std::vector<MidiNote> notes; // by start time
    // When the last note ends, in seconds.
    double duration = 0;
    int tracks = 0;
};

inline constexpr std::size_t midiLargestFile = 4 * 1024 * 1024;
inline constexpr std::size_t midiMostNotes = 400'000;
inline constexpr double midiLongestSeconds = 20 * 60;

// Formats 0 and 1 with a tempo in ticks per quarter note. Refused, with the
// reason: anything that is not such a file, a damaged one, or one longer or
// larger than the limits above. A note still sounding at the end of its
// track ends there.
[[nodiscard]] std::expected<MidiSong, std::string> parseMidi(std::span<const std::byte> file);

inline constexpr int synthSampleRate = 22'050;

// The song as 16-bit mono samples at synthSampleRate, with a short tail
// after the last note. Never louder than full scale.
[[nodiscard]] std::vector<std::int16_t> renderMidi(const MidiSong &song);

// A WAV file (PCM, 16 bit, mono) of the samples.
[[nodiscard]] std::vector<std::byte> wavFile(std::span<const std::int16_t> samples,
                                             int sampleRate = synthSampleRate);

} // namespace eggbert
