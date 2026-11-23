// SPDX-License-Identifier: GPL-3.0-only
//
// The editor's own MIDI reader and synthesizer, on files built here.
#include "app/midi_synth.hpp"

#include "check.hpp"

#include <cmath>
#include <string>
#include <vector>

using namespace eggbert;

namespace {

using Bytes = std::vector<std::byte>;

void put(Bytes &out, std::initializer_list<int> values) {
    for (const int value : values) {
        out.push_back(static_cast<std::byte>(value));
    }
}

void text(Bytes &out, const char *tag) {
    for (int index = 0; index < 4; ++index) {
        out.push_back(static_cast<std::byte>(tag[index]));
    }
}

void big(Bytes &out, std::uint32_t value, int count) {
    for (int index = count - 1; index >= 0; --index) {
        out.push_back(static_cast<std::byte>((value >> (8 * index)) & 0xFFu));
    }
}

Bytes chunk(const char *tag, const Bytes &content) {
    Bytes out;
    text(out, tag);
    big(out, static_cast<std::uint32_t>(content.size()), 4);
    out.insert(out.end(), content.begin(), content.end());
    return out;
}

Bytes header(int format, int tracks, int division) {
    Bytes content;
    big(content, static_cast<std::uint32_t>(format), 2);
    big(content, static_cast<std::uint32_t>(tracks), 2);
    big(content, static_cast<std::uint32_t>(division), 2);
    return chunk("MThd", content);
}

Bytes join(std::initializer_list<Bytes> parts) {
    Bytes out;
    for (const auto &part : parts) {
        out.insert(out.end(), part.begin(), part.end());
    }
    return out;
}

// Two tracks, 480 ticks a quarter note. The tempo track halves the quarter
// note from 0.5 s to 0.25 s at tick 960. The second track plays A4 on a
// flute for 480 ticks, C4 from tick 960 to 1440, and one bass drum.
Bytes song() {
    Bytes tempo;
    put(tempo, {0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20});       // 500,000
    put(tempo, {0x87, 0x40, 0xFF, 0x51, 0x03, 0x03, 0xD0, 0x90}); // +960: 250,000
    put(tempo, {0x00, 0xFF, 0x2F, 0x00});
    Bytes notes;
    put(notes, {0x00, 0xC0, 73});             // flute on channel 1
    put(notes, {0x00, 0xB0, 7, 127});         // its volume
    put(notes, {0x00, 0x99, 36, 110});        // bass drum on the percussion channel
    put(notes, {0x00, 0x90, 69, 100});        // A4
    put(notes, {0x83, 0x60, 69, 0});          // +480, running status, velocity 0: off
    put(notes, {0x83, 0x60, 0x90, 60, 90});   // +480 (tick 960): C4
    put(notes, {0x00, 0xF0, 0x02, 0x7E, 0xF7}); // a system-exclusive message, skipped
    put(notes, {0x83, 0x60, 0x80, 60, 0});    // +480 (tick 1440): off
    put(notes, {0x00, 0xFF, 0x2F, 0x00});
    return join({header(1, 2, 480), chunk("MTrk", tempo), chunk("MTrk", notes)});
}

bool near(double value, double wanted, double tolerance = 1e-6) {
    return std::abs(value - wanted) <= tolerance;
}

} // namespace

int main() {
    const auto file = song();
    const auto parsed = parseMidi(file);
    CHECK(parsed.has_value());
    if (!parsed) {
        return test::finish("midi_synth");
    }
    CHECK_EQ(parsed->tracks, 2);
    CHECK_EQ(parsed->notes.size(), std::size_t{3});
    // By start time; the drum and A4 both start at 0, in file order.
    const auto &drum = parsed->notes[0];
    const auto &a4 = parsed->notes[1];
    const auto &c4 = parsed->notes[2];
    CHECK(drum.channel == 9 && drum.key == 36 && drum.velocity == 110);
    CHECK(a4.channel == 0 && a4.key == 69 && a4.program == 73 && a4.volume == 127);
    CHECK(near(a4.start, 0.0) && near(a4.end, 0.5));
    // 960 ticks at 0.5 s a quarter note, then 480 at 0.25 s.
    CHECK(near(c4.start, 1.0) && near(c4.end, 1.25));
    CHECK(near(parsed->duration, 1.25));

    // Sound: as long as the song plus a tail, A4 at 440 Hz, silent in between.
    const auto samples = renderMidi(*parsed);
    CHECK_EQ(samples.size(), static_cast<std::size_t>((1.25 + 0.5) * synthSampleRate) + 1);
    CHECK(samples == renderMidi(*parsed)); // deterministic
    const auto at = [](double seconds) {
        return static_cast<std::size_t>(seconds * synthSampleRate);
    };
    int crossings = 0;
    for (auto index = at(0.25); index < at(0.45); ++index) {
        crossings += samples[index] < 0 && samples[index + 1] >= 0 ? 1 : 0;
    }
    CHECK(crossings >= 87 && crossings <= 89); // 0.2 s of 440 Hz
    int loudest = 0;
    for (auto index = at(0.25); index < at(0.45); ++index) {
        loudest = std::max(loudest, std::abs(static_cast<int>(samples[index])));
    }
    CHECK(loudest > 2000);
    for (auto index = at(0.7); index < at(0.95); ++index) {
        CHECK_EQ(samples[index], 0);
    }
    int later = 0;
    for (auto index = at(1.05); index < at(1.2); ++index) {
        later = std::max(later, std::abs(static_cast<int>(samples[index])));
    }
    CHECK(later > 1000);
    CHECK_EQ(samples.back(), 0);

    // A WAV file of it: 44 bytes of header, then the samples.
    const auto wav = wavFile(samples);
    CHECK_EQ(wav.size(), 44 + 2 * samples.size());
    CHECK(static_cast<char>(wav[0]) == 'R' && static_cast<char>(wav[8]) == 'W' &&
          static_cast<char>(wav[36]) == 'd');
    const auto little = [&](std::size_t offset, int count) {
        std::uint32_t value = 0;
        for (int index = 0; index < count; ++index) {
            value |= static_cast<std::uint32_t>(wav[offset + static_cast<std::size_t>(index)])
                     << (8 * index);
        }
        return value;
    };
    CHECK_EQ(little(4, 4), static_cast<std::uint32_t>(wav.size() - 8));
    CHECK_EQ(little(22, 2), 1u);
    CHECK_EQ(little(24, 4), static_cast<std::uint32_t>(synthSampleRate));
    CHECK_EQ(little(34, 2), 16u);
    CHECK_EQ(little(40, 4), static_cast<std::uint32_t>(2 * samples.size()));
    CHECK_EQ(static_cast<std::int16_t>(little(44 + 2 * at(0.3), 2)), samples[at(0.3)]);

    // A note still sounding when its track ends stops there; a format 0 file.
    {
        Bytes notes;
        put(notes, {0x00, 0x90, 60, 80, 0x83, 0x60, 0xFF, 0x2F, 0x00});
        const auto open = parseMidi(join({header(0, 1, 480), chunk("MTrk", notes)}));
        CHECK(open.has_value() && open->notes.size() == 1 && near(open->notes[0].end, 0.5));
    }
    // What is refused, each with a reason and without reading out of bounds.
    const auto refused = [](const Bytes &bytes, const char *reason) {
        const auto result = parseMidi(bytes);
        CHECK(!result.has_value());
        CHECK(!result.has_value() && result.error().find(reason) != std::string::npos);
    };
    refused({}, "not a standard MIDI file");
    refused(Bytes(100, std::byte{0}), "not a standard MIDI file");
    refused(header(2, 1, 480), "format 0 or 1");
    refused(header(1, 0, 480), "format 0 or 1");
    refused(header(1, 1, 0xE728), "frames");
    refused(header(1, 1, 480), "missing or damaged");
    {
        auto cut = file;
        cut.resize(cut.size() - 5);
        refused(cut, "cut off");
        Bytes orphan;
        put(orphan, {0x00, 60, 80}); // data bytes with no status before them
        refused(join({header(0, 1, 480), chunk("MTrk", orphan)}), "damaged");
        Bytes endless;
        put(endless, {0x81, 0x81, 0x81, 0x81, 0x81, 0x90, 60, 80}); // a five-byte length
        refused(join({header(0, 1, 480), chunk("MTrk", endless)}), "damaged");
        Bytes meta;
        put(meta, {0x00, 0xFF, 0x51, 0x7F, 0x01}); // a meta event longer than the track
        refused(join({header(0, 1, 480), chunk("MTrk", meta)}), "damaged");
    }
    refused(Bytes(midiLargestFile + 1, std::byte{0}), "larger");
    {
        // One tick lasting very long: longer than can be played.
        Bytes slow;
        put(slow, {0x00, 0xFF, 0x51, 0x03, 0xFF, 0xFF, 0xFF, 0x00, 0x90, 60, 80});
        put(slow, {0xFF, 0xFF, 0xFF, 0x7F, 0x80, 60, 0, 0x00, 0xFF, 0x2F, 0x00});
        refused(join({header(0, 1, 1), chunk("MTrk", slow)}), "longer");
    }
    // An empty song renders to its short tail of silence.
    {
        MidiSong empty;
        const auto quiet = renderMidi(empty);
        CHECK(!quiet.empty());
        for (const auto sample : quiet) {
            CHECK_EQ(sample, 0);
        }
    }
    return test::finish("midi_synth");
}
