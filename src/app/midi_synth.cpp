// SPDX-License-Identifier: GPL-3.0-only
#include "app/midi_synth.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <numbers>
#include <optional>

namespace eggbert {

namespace {

class Reader {
public:
    explicit Reader(std::span<const std::byte> bytes) : bytes_(bytes) {}

    [[nodiscard]] std::size_t left() const { return bytes_.size() - at_; }
    [[nodiscard]] bool failed() const { return failed_; }

    std::uint8_t u8() {
        if (left() < 1) {
            failed_ = true;
            return 0;
        }
        return static_cast<std::uint8_t>(bytes_[at_++]);
    }
    std::uint32_t big(int count) {
        std::uint32_t value = 0;
        for (int index = 0; index < count; ++index) {
            value = (value << 8) | u8();
        }
        return value;
    }
    // A variable-length quantity: at most four bytes of seven bits.
    std::uint32_t variable() {
        std::uint32_t value = 0;
        for (int index = 0; index < 4; ++index) {
            const auto byte = u8();
            value = (value << 7) | (byte & 0x7Fu);
            if ((byte & 0x80u) == 0) {
                return value;
            }
        }
        failed_ = true;
        return 0;
    }
    void skip(std::size_t count) {
        if (left() < count) {
            failed_ = true;
            at_ = bytes_.size();
        } else {
            at_ += count;
        }
    }
    [[nodiscard]] Reader part(std::size_t count) {
        if (left() < count) {
            failed_ = true;
            return Reader({});
        }
        Reader inner(bytes_.subspan(at_, count));
        at_ += count;
        return inner;
    }
    [[nodiscard]] bool tag(const char *name) {
        if (left() < 4) {
            failed_ = true;
            return false;
        }
        bool same = true;
        for (int index = 0; index < 4; ++index) {
            same = same && static_cast<char>(bytes_[at_ + static_cast<std::size_t>(index)]) ==
                               name[index];
        }
        at_ += 4;
        return same;
    }

private:
    std::span<const std::byte> bytes_;
    std::size_t at_ = 0;
    bool failed_ = false;
};

struct TickNote {
    std::uint64_t start = 0;
    std::uint64_t end = 0;
    std::uint8_t channel = 0;
    std::uint8_t key = 0;
    std::uint8_t velocity = 0;
    std::uint8_t program = 0;
    std::uint8_t volume = 100;
};

std::unexpected<std::string> fail(const char *text) {
    return std::unexpected(std::string(text));
}

} // namespace

std::expected<MidiSong, std::string> parseMidi(std::span<const std::byte> file) {
    if (file.size() > midiLargestFile) {
        return fail("The file is larger than a MIDI file of a game can be.");
    }
    Reader reader(file);
    if (!reader.tag("MThd") || reader.big(4) != 6) {
        return fail("This is not a standard MIDI file.");
    }
    const auto format = reader.big(2);
    const auto trackCount = reader.big(2);
    const auto division = reader.big(2);
    if (reader.failed() || format > 1 || trackCount == 0 || trackCount > 64) {
        return fail("Only MIDI files of format 0 or 1 can be played.");
    }
    if ((division & 0x8000u) != 0 || division == 0) {
        return fail("The MIDI file measures time in frames, which is not supported.");
    }

    std::vector<TickNote> notes;
    // Tempo changes: tick -> microseconds per quarter note.
    std::map<std::uint64_t, std::uint32_t> tempos{{0, 500'000}};
    int tracks = 0;
    for (std::uint32_t track = 0; track < trackCount; ++track) {
        if (!reader.tag("MTrk")) {
            return fail("A track of the MIDI file is missing or damaged.");
        }
        auto events = reader.part(reader.big(4));
        if (reader.failed()) {
            return fail("A track of the MIDI file is cut off.");
        }
        ++tracks;
        std::uint64_t tick = 0;
        std::uint8_t status = 0;
        std::array<std::uint8_t, 16> program{};
        std::array<std::uint8_t, 16> volume;
        volume.fill(100);
        // Sounding notes of this track: (channel, key) -> index in `notes`.
        std::map<std::pair<int, int>, std::size_t> sounding;
        const auto noteOff = [&](int channel, int key) {
            if (const auto found = sounding.find({channel, key}); found != sounding.end()) {
                notes[found->second].end = tick;
                sounding.erase(found);
            }
        };
        while (events.left() > 0) {
            tick += events.variable();
            auto first = events.u8();
            if (events.failed()) {
                return fail("A track of the MIDI file is damaged.");
            }
            if (first == 0xFF) {
                const auto type = events.u8();
                auto data = events.part(events.variable());
                if (events.failed()) {
                    return fail("A track of the MIDI file is damaged.");
                }
                if (type == 0x51 && data.left() == 3) {
                    if (const auto tempo = data.big(3); tempo != 0) {
                        tempos[tick] = tempo;
                    }
                } else if (type == 0x2F) {
                    break;
                }
                continue;
            }
            if (first == 0xF0 || first == 0xF7) {
                events.skip(events.variable());
                if (events.failed()) {
                    return fail("A track of the MIDI file is damaged.");
                }
                continue;
            }
            std::uint8_t data1 = 0;
            if ((first & 0x80u) != 0) {
                status = first;
                if (status >= 0xF0) {
                    return fail("A track of the MIDI file holds an event that cannot be read.");
                }
                data1 = events.u8();
            } else {
                // Running status: the byte is already the first data byte.
                if (status == 0) {
                    return fail("A track of the MIDI file is damaged.");
                }
                data1 = first;
            }
            const auto kind = status & 0xF0u;
            const int channel = status & 0x0F;
            const std::uint8_t data2 = kind == 0xC0 || kind == 0xD0 ? 0 : events.u8();
            if (events.failed() || data1 > 127 || data2 > 127) {
                return fail("A track of the MIDI file is damaged.");
            }
            if (kind == 0x90 && data2 != 0) {
                noteOff(channel, data1); // the same key struck again
                if (notes.size() >= midiMostNotes) {
                    return fail("The MIDI file holds more notes than can be played here.");
                }
                sounding[{channel, data1}] = notes.size();
                notes.push_back({tick, tick, static_cast<std::uint8_t>(channel), data1, data2,
                                 program[static_cast<std::size_t>(channel)],
                                 volume[static_cast<std::size_t>(channel)]});
            } else if (kind == 0x80 || kind == 0x90) {
                noteOff(channel, data1);
            } else if (kind == 0xC0) {
                program[static_cast<std::size_t>(channel)] = data1;
            } else if (kind == 0xB0 && data1 == 7) {
                volume[static_cast<std::size_t>(channel)] = data2;
            } else if (kind == 0xB0 && (data1 == 120 || data1 == 123)) {
                for (auto item = sounding.begin(); item != sounding.end();) {
                    if (item->first.first == channel) {
                        notes[item->second].end = tick;
                        item = sounding.erase(item);
                    } else {
                        ++item;
                    }
                }
            }
        }
        for (const auto &[key, index] : sounding) {
            notes[index].end = tick;
        }
    }

    // Ticks to seconds through the tempo changes.
    const auto seconds = [&](std::uint64_t tick) {
        double total = 0;
        std::uint64_t from = 0;
        std::uint32_t tempo = 500'000;
        for (const auto &[at, value] : tempos) {
            if (at >= tick) {
                break;
            }
            total += static_cast<double>(at - from) * tempo / 1e6 / division;
            from = at;
            tempo = value;
        }
        return total + static_cast<double>(tick - from) * tempo / 1e6 / division;
    };
    MidiSong song;
    song.tracks = tracks;
    song.notes.reserve(notes.size());
    for (const auto &note : notes) {
        MidiNote out;
        out.start = seconds(note.start);
        out.end = std::max(out.start, seconds(note.end));
        out.channel = note.channel;
        out.key = note.key;
        out.velocity = note.velocity;
        out.program = note.program;
        out.volume = note.volume;
        song.duration = std::max(song.duration, out.end);
        song.notes.push_back(out);
    }
    if (song.duration > midiLongestSeconds) {
        return fail("The MIDI file is longer than can be played here.");
    }
    std::ranges::stable_sort(song.notes, {}, &MidiNote::start);
    return song;
}

namespace {

constexpr std::size_t tableSize = 2048;
using Table = std::array<float, tableSize>;

// One cycle built from harmonics with the given strengths.
Table makeTable(std::initializer_list<float> harmonics) {
    Table table{};
    int number = 1;
    float peak = 0;
    for (const float strength : harmonics) {
        for (std::size_t index = 0; index < tableSize; ++index) {
            table[index] += strength * static_cast<float>(std::sin(
                                           2 * std::numbers::pi * number *
                                           static_cast<double>(index) / tableSize));
        }
        ++number;
    }
    for (const float value : table) {
        peak = std::max(peak, std::abs(value));
    }
    for (auto &value : table) {
        value /= peak;
    }
    return table;
}

struct Voice {
    const Table *table;
    double attack;  // seconds to full level
    double decay;   // time constant of the fall while the key is held; 0 holds
    double release; // seconds to silence after the key is let go
};

// A deterministic noise source for the drums.
class Noise {
public:
    float next() {
        state_ = state_ * 1'664'525u + 1'013'904'223u;
        return static_cast<float>(static_cast<std::int32_t>(state_)) / 2'147'483'648.0f;
    }

private:
    std::uint32_t state_ = 0x1234'5678u;
};

} // namespace

std::vector<std::int16_t> renderMidi(const MidiSong &song) {
    static const Table plucked = makeTable({1.0f, 0.5f, 0.3f, 0.12f, 0.06f});
    static const Table organ = makeTable({1.0f, 0.6f, 0.0f, 0.3f});
    static const Table strings = makeTable({1.0f, 0.5f, 0.33f, 0.25f, 0.2f, 0.16f});
    static const Table reed = makeTable({1.0f, 0.0f, 0.33f, 0.0f, 0.2f});
    static const Table flute = makeTable({1.0f, 0.15f});
    static const Table bass = makeTable({1.0f, 0.3f, 0.1f});
    static const Table pure = makeTable({1.0f});
    // By General MIDI instrument family (program / 8).
    static const std::array<Voice, 16> families{{
        {&plucked, 0.004, 0.7, 0.15}, // pianos
        {&pure, 0.002, 0.5, 0.2},     // chromatic percussion
        {&organ, 0.01, 0, 0.06},      // organs
        {&plucked, 0.003, 0.5, 0.1},  // guitars
        {&bass, 0.004, 0.6, 0.08},    // basses
        {&strings, 0.06, 0, 0.15},    // strings
        {&strings, 0.08, 0, 0.2},     // ensembles
        {&reed, 0.03, 0, 0.08},       // brass
        {&reed, 0.03, 0, 0.08},       // reeds
        {&flute, 0.04, 0, 0.1},       // pipes
        {&reed, 0.01, 0, 0.08},       // synth leads
        {&strings, 0.1, 0, 0.3},      // synth pads
        {&plucked, 0.01, 0.8, 0.2},   // synth effects
        {&plucked, 0.003, 0.4, 0.1},  // ethnic
        {&pure, 0.002, 0.3, 0.1},     // percussive
        {&pure, 0.01, 0.3, 0.1},      // sound effects
    }};

    const double tail = 0.5;
    const auto total = static_cast<std::size_t>((song.duration + tail) * synthSampleRate) + 1;
    std::vector<float> mix(total, 0.0f);
    Noise noise;
    for (const auto &note : song.notes) {
        const double level = note.velocity / 127.0 * note.volume / 127.0 * 0.22;
        const auto first = static_cast<std::size_t>(note.start * synthSampleRate);
        if (note.channel == 9) {
            // Percussion: a falling tone for the drums, noise for the rest.
            const bool kick = note.key == 35 || note.key == 36;
            const bool tom = note.key == 41 || note.key == 43 || note.key == 45 ||
                             note.key == 47 || note.key == 48 || note.key == 50;
            const bool snare = note.key == 38 || note.key == 40;
            const bool longNoise = note.key == 46 || note.key == 49 || note.key == 51 ||
                                   note.key == 57;
            const double length = kick ? 0.18 : tom ? 0.2 : snare ? 0.14 : longNoise ? 0.4 : 0.05;
            const auto count = static_cast<std::size_t>(length * synthSampleRate);
            const double from = kick ? 130 : 90 + 4.0 * (note.key - 40);
            double phase = 0;
            float previous = 0;
            for (std::size_t index = 0; index < count && first + index < total; ++index) {
                const double t = static_cast<double>(index) / synthSampleRate;
                const double envelope = std::exp(-t / (length / 4));
                double sample = 0;
                if (kick || tom) {
                    const double frequency = from * (0.4 + 0.6 * std::exp(-t / 0.05));
                    phase += frequency / synthSampleRate;
                    sample = std::sin(2 * std::numbers::pi * phase);
                } else {
                    const float white = noise.next();
                    // Cymbals keep only the hiss; the snare keeps it all.
                    sample = snare ? 0.7 * white + 0.3 * std::sin(2 * std::numbers::pi * 185 * t)
                                   : 0.5 * (white - previous);
                    previous = white;
                }
                mix[first + index] += static_cast<float>(sample * envelope * level * 1.6);
            }
            continue;
        }
        const auto &voice = families[note.program / 8];
        const double frequency = 440.0 * std::pow(2.0, (note.key - 69) / 12.0);
        if (frequency >= synthSampleRate / 2.0) {
            continue;
        }
        // High notes use the pure tone so that no overtone folds back.
        const Table &table = frequency * 6 >= synthSampleRate / 2.0 ? pure : *voice.table;
        const double held = note.end - note.start;
        const auto count =
            static_cast<std::size_t>((held + voice.release) * synthSampleRate) + 1;
        const double step = frequency * tableSize / synthSampleRate;
        double position = 0;
        for (std::size_t index = 0; index < count && first + index < total; ++index) {
            const double t = static_cast<double>(index) / synthSampleRate;
            double envelope = t < voice.attack ? t / voice.attack : 1.0;
            if (voice.decay > 0) {
                envelope *= std::exp(-t / voice.decay);
            }
            if (t > held) {
                envelope *= std::max(0.0, 1.0 - (t - held) / voice.release);
            }
            mix[first + index] += static_cast<float>(
                table[static_cast<std::size_t>(position) % tableSize] * envelope * level);
            position += step;
            if (position >= tableSize) {
                position -= tableSize;
            }
        }
    }
    float peak = 0;
    for (const float value : mix) {
        peak = std::max(peak, std::abs(value));
    }
    const float gain = peak > 0.95f ? 0.95f / peak : 1.0f;
    std::vector<std::int16_t> samples(total);
    for (std::size_t index = 0; index < total; ++index) {
        samples[index] = static_cast<std::int16_t>(std::lround(mix[index] * gain * 32'767.0f));
    }
    return samples;
}

std::vector<std::byte> wavFile(std::span<const std::int16_t> samples, int sampleRate) {
    std::vector<std::byte> file;
    const auto text = [&](const char *value) {
        for (int index = 0; index < 4; ++index) {
            file.push_back(static_cast<std::byte>(value[index]));
        }
    };
    const auto little = [&](std::uint32_t value, int count) {
        for (int index = 0; index < count; ++index) {
            file.push_back(static_cast<std::byte>((value >> (8 * index)) & 0xFFu));
        }
    };
    const auto dataBytes = static_cast<std::uint32_t>(samples.size() * 2);
    file.reserve(44 + dataBytes);
    text("RIFF");
    little(36 + dataBytes, 4);
    text("WAVE");
    text("fmt ");
    little(16, 4);
    little(1, 2); // PCM
    little(1, 2); // mono
    little(static_cast<std::uint32_t>(sampleRate), 4);
    little(static_cast<std::uint32_t>(sampleRate) * 2, 4);
    little(2, 2);
    little(16, 2);
    text("data");
    little(dataBytes, 4);
    for (const auto sample : samples) {
        little(static_cast<std::uint16_t>(sample), 2);
    }
    return file;
}

} // namespace eggbert
