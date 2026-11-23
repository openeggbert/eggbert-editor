// SPDX-License-Identifier: GPL-3.0-only
#include "world/animation.hpp"

#include <array>
#include <algorithm>
#include <span>

#include "world/object_animation_data.hpp"

namespace eggbert {

namespace {

// Frame tables read from blupi_recon at commit
// e05e22b990abb782c37a0d9fafa864d86a27e29f: src/decblock.cpp:26-56 and
// games/sb2/src/decblock.cpp:44-88 (docs/research/evidence.md, "Animation").
constexpr std::int16_t marinePlant[] = {203, 204, 205, 206, 207, 208, 207, 206, 205, 204, 203};
constexpr std::int16_t spring[] = {209, 210, 211, 212, 213, 212, 211, 210};
constexpr std::int16_t lavaFirst[] = {68, 69, 70, 71, 72};
constexpr std::int16_t lavaSecond[] = {68, 69, 70, 71, 72, 71, 70, 69};
constexpr std::int16_t waterSurface[] = {92, 93, 94, 95};
constexpr std::int16_t waterBody[] = {91, 96, 97, 98};
constexpr std::int16_t slimeTrap[] = {374, 374, 373, 347, 373, 374, 374, 374,
                                      373, 347, 347, 373, 374, 374, 374, 374};
constexpr std::int16_t drip[] = {410, 409, 410, -1,  -1,  -1,  -1,  -1,  -1,  410, 409, 408,
                                 407, 408, 409, 410, -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
                                 410, 409, 408, 407, 406, 405, 404, 405, 406, 407, 408, 409,
                                 410, -1,  -1,  -1,  -1,  410, 409, 410, -1,  -1,  -1,  -1};
constexpr std::int16_t crusher[] = {317, 317, 318, 319, 320, 321, 322, 323, 323, 323};
constexpr std::int16_t saw[] = {378, 379, 380, 381, 382, 383};
constexpr std::int16_t vanishing[] = {328, 328, 327, 327, 326, 326, 325, 325, 324, 324,
                                      325, 325, 326, 326, 327, 329, 328, 328, -1,  -1};
constexpr std::int16_t lightning[] = {305, 306, 307, 308, 306, 308, 305, 307, 306, 305,
                                      308, 307, 305, 306, 307, 305, 308, 307, 305, 306,
                                      305, 308, 306, 308, 305, 306, 307, 308, 306, 307,
                                      308, 307, 306, 305, 307, 308, 305, 306, 308, 307,
                                      306, 308, 305, 308, 307, 305, 306, 307, 306, 308};

std::int16_t at(std::span<const std::int16_t> table, std::uint64_t index) {
    return table[index % table.size()];
}

// Wind (four frames from its head, every tick) and fans (three frames,
// every second tick): the frames are the numbers that follow the head.
bool isWindHead(std::int16_t value) {
    return value == 110 || value == 114 || value == 118 || value == 122;
}
bool isFanHead(std::int16_t value) {
    return value == 126 || value == 129 || value == 132 || value == 135;
}

TileFrame flags(GameProfile profile, SectionKind layer, std::int16_t stored) {
    TileFrame frame;
    frame.icon = stored;
    if (profile == GameProfile::unknown || layer != SectionKind::foreground) {
        return frame;
    }
    const bool second = tablesOf(profile) == GameProfile::speedyBlupi2;
    // Second pass of both games (src/decblock.cpp:1461-1515,
    // games/sb2/src/decblock.cpp:1900-2015).
    if (stored == 68 || stored == 91 || stored == 92 || isWindHead(stored) || isFanHead(stored)) {
        frame.late = true;
    }
    if (second && (stored == 373 || stored == 404 || stored == 410 || stored == 317 ||
                   stored == 378 || stored == 324 || stored == 305)) {
        frame.late = true;
    }
    if (second && (stored == 404 || stored == 410)) {
        frame.offsetY = -9; // games/sb2/src/decblock.cpp:1926
    }
    if (second && (stored == 384 || stored == 385)) {
        frame.early = true; // games/sb2/src/decblock.cpp, pass before objects
    }
    return frame;
}

} // namespace

bool isTileAnimationFrame(GameProfile profile, SectionKind layer, std::int16_t value) {
    if (profile == GameProfile::unknown || value < 0 || tileFrame(profile, layer, value, 0, 0, 0).animated) {
        return false;
    }
    const auto includes = [value](std::span<const std::int16_t> frames) {
        return std::ranges::find(frames, value) != frames.end();
    };
    if (layer == SectionKind::bigDecor) {
        return includes(marinePlant);
    }
    if (layer != SectionKind::foreground) {
        return false;
    }
    return includes(spring) || includes(lavaFirst) || includes(waterSurface) || includes(waterBody) ||
           (value >= 110 && value <= 137) ||
           (tablesOf(profile) == GameProfile::speedyBlupi2 &&
            (includes(saw) || includes(crusher) || includes(lightning) || includes(vanishing) ||
             includes(drip) || includes(slimeTrap)));
}

TileFrame stillTileFrame(GameProfile profile, SectionKind layer, std::int16_t stored) {
    return flags(profile, layer, stored);
}

TileFrame tileFrame(GameProfile profile, SectionKind layer, std::int16_t stored, std::size_t x,
                    std::size_t y, std::uint32_t tick) {
    auto frame = flags(profile, layer, stored);
    if (profile == GameProfile::unknown) {
        return frame;
    }
    const bool second = tablesOf(profile) == GameProfile::speedyBlupi2;
    const std::uint64_t t = tick;
    const std::uint64_t cellPhase = x * 13 + y * 7;
    const auto animate = [&](std::int16_t icon) {
        frame.icon = icon;
        frame.animated = true;
    };
    if (layer == SectionKind::bigDecor) {
        if (stored == 203) {
            animate(at(marinePlant, t / 3));
        }
        return frame;
    }
    if (stored == 211) {
        animate(at(spring, t / 2 + x * 7));
    } else if (stored == 68) {
        animate(second ? at(lavaSecond, cellPhase + t / 3) : at(lavaFirst, cellPhase + t / 3));
    } else if (stored == 92) {
        animate(at(waterSurface, cellPhase + t / 3));
    } else if (stored == 91) {
        animate(at(waterBody, cellPhase + t / 3));
    } else if (isWindHead(stored)) {
        animate(static_cast<std::int16_t>(stored + t % 4));
    } else if (isFanHead(stored)) {
        animate(static_cast<std::int16_t>(stored + (t / 2) % 3));
    } else if (second) {
        if (stored == 373) {
            animate(at(slimeTrap, cellPhase + t / 4));
        } else if (stored == 404 || stored == 410) {
            animate(at(drip, cellPhase + t / 2));
        } else if (stored == 317) {
            animate(at(crusher, t / 3));
        } else if (stored == 378) {
            animate(at(saw, cellPhase + t));
        } else if (stored == 324) {
            animate(at(vanishing, t / 4));
        } else if (stored == 305) {
            // In the games' editor the beam is drawn on every even tick.
            animate(t % 2 == 0 ? at(lightning, cellPhase + t) : layout::emptyCell);
        }
    }
    return frame;
}

std::int16_t stillObjectIcon(GameProfile profile, std::int16_t type,
                             std::int16_t channel, std::int16_t icon) {
    // The original editor initializes all three coloured keys with element 125.
    // The game chooses the actual key from its type on the first animation tick.
    if (tablesOf(profile) == GameProfile::speedyBlupi2 && type >= 49 && type <= 51 &&
        channel == 10 && icon == 125) {
        for (const auto &rule : objectIconRules(profile)) {
            if (rule.type == type && !rule.frames.empty()) {
                return rule.frames.front();
            }
        }
    }
    return icon;
}

void ObjectAnimation::reset(Slot &slot, const ObjectRecord &record) {
    // What the games' editors do on entry (src/decblock.cpp:1085-1092).
    slot.stored = record;
    slot.position = record.posStart;
    slot.step = 1;
    slot.time = 0;
    slot.phase = 0;
    slot.channel = record.channel;
    slot.icon = stillObjectIcon(profile_, record.type, record.channel, record.icon);
}

void ObjectAnimation::sync(const WorldView &world, GameProfile profile) {
    const auto objects = world.objects();
    const std::size_t count = objects ? objects->slotCount() : 0;
    const bool restartAll = profile != profile_ || count != slots_.size();
    profile_ = profile;
    if (restartAll) {
        slots_.assign(count, Slot{});
        tick_ = 0;
    }
    poses_.resize(count);
    for (std::size_t index = 0; index < count; ++index) {
        const auto record = *objects->record(index);
        if (restartAll || !(record == slots_[index].stored)) {
            reset(slots_[index], record);
        }
        poses_[index] = {slots_[index].position, slots_[index].channel, slots_[index].icon};
    }
}

void ObjectAnimation::restart() {
    tick_ = 0;
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        const auto record = slots_[index].stored;
        reset(slots_[index], record);
        poses_[index] = {slots_[index].position, slots_[index].channel, slots_[index].icon};
    }
}

int ObjectAnimation::step(std::size_t slot) const {
    return slot < slots_.size() ? slots_[slot].step : 0;
}

int ObjectAnimation::time(std::size_t slot) const {
    return slot < slots_.size() ? slots_[slot].time : 0;
}

void ObjectAnimation::stepSlot(Slot &slot) {
    const auto &record = slot.stored;
    if (record.isFree()) {
        return;
    }
    // The line motion of both games (src/decmove.cpp:518-600, identical in
    // games/sb2/src/decmove.cpp): wait, go, wait, come back. Arrival is seen
    // on the tick after the position reaches the other end.
    if (record.posStart != record.posEnd) {
        // The games compute this in 32 bits. Stored positions can be any
        // 32-bit values (a file is not checked), so the same formula is
        // evaluated in 64 bits here: no overflow, identical results for
        // every position inside a world.
        const auto interpolate = [&](Point from, Point to, std::int16_t duration) {
            if (duration != 0) {
                const auto along = [&](std::int32_t start, std::int32_t end) {
                    const auto distance = static_cast<std::int64_t>(end) - start;
                    return static_cast<std::int32_t>(distance * slot.time / duration + start);
                };
                slot.position.x = along(from.x, to.x);
                slot.position.y = along(from.y, to.y);
            }
        };
        switch (slot.step) {
        case 1:
            if (slot.time < record.timeStopStart) {
                ++slot.time;
            } else {
                slot.step = 2;
                slot.time = 0;
            }
            break;
        case 2:
            if (slot.position != record.posEnd) {
                ++slot.time;
                interpolate(record.posStart, record.posEnd, record.stepAdvance);
            } else {
                slot.step = 3;
                slot.time = 0;
            }
            break;
        case 3:
            if (slot.time < record.timeStopEnd) {
                ++slot.time;
            } else {
                slot.step = 4;
                slot.time = 0;
            }
            break;
        case 4:
            if (slot.position != record.posStart) {
                ++slot.time;
                interpolate(record.posEnd, record.posStart, record.stepRecede);
            } else {
                slot.step = 1;
                slot.time = 0;
            }
            break;
        default:
            break;
        }
        // A zero duration never arrives; keep the counter in range.
        if (slot.time > 30'000) {
            slot.time = 0;
        }
    }
    // The picture for this tick, from the state after the motion and the
    // phase before its increment (the games' order).
    for (const auto &rule : objectIconRules(profile_)) {
        if (rule.type != record.type || rule.frames.empty()) {
            continue;
        }
        if (rule.step != 0 && rule.step != slot.step) {
            continue;
        }
        const bool startRightOfEnd = record.posStart.x > record.posEnd.x;
        if ((rule.direction == 1 && !startRightOfEnd) || (rule.direction == 2 && startRightOfEnd)) {
            continue;
        }
        const auto counter = static_cast<std::uint32_t>(rule.usesTime ? slot.time : slot.phase);
        const auto divisor = static_cast<std::uint32_t>(rule.divisor > 0 ? rule.divisor : 1);
        slot.icon = rule.frames[(counter / divisor) % rule.frames.size()];
        if (rule.channel >= 0) {
            slot.channel = rule.channel;
        }
    }
    ++slot.phase;
    if (slot.phase > 32'700) {
        slot.phase = 0;
    }
}

void ObjectAnimation::advance(std::uint32_t ticks) {
    for (std::uint32_t done = 0; done < ticks; ++done) {
        for (auto &slot : slots_) {
            stepSlot(slot);
        }
        ++tick_;
    }
    for (std::size_t index = 0; index < slots_.size(); ++index) {
        poses_[index] = {slots_[index].position, slots_[index].channel, slots_[index].icon};
    }
}

} // namespace eggbert
