// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "world/layout.hpp"
#include "world/world_view.hpp"

namespace eggbert {

// What the games show while time passes. Nothing here changes a stored
// byte: these functions only say which picture belongs to a tick.
//
// The games advance one tick per drawn frame, 20 times a second with their
// default 50 ms timer (docs/research/evidence.md, "Animation").
inline constexpr int animationTicksPerSecond = 20;

// How one stored tile is drawn at one tick.
struct TileFrame {
    // The picture number drawn; layout::emptyCell when nothing is drawn at
    // this tick (some animations have blank frames).
    std::int16_t icon = layout::emptyCell;
    // Extra vertical pixels for this tile (negative is up).
    int offsetY = 0;
    // The games draw these tiles in a second pass, above objects and lifts
    // (lava, water, wind, fans and the second game's traps).
    bool late = false;
    // The second game draws these two tiles before all objects.
    bool early = false;
    // True when the picture depends on the tick.
    bool animated = false;

    friend constexpr bool operator==(const TileFrame &, const TileFrame &) = default;
};

// The frame of a stored tile number in cell (x, y) at `tick`. For a number
// the chosen game does not animate (and for GameProfile::unknown) the icon
// is the stored number itself. Draw order flags are returned in any case.
[[nodiscard]] TileFrame tileFrame(GameProfile profile, SectionKind layer, std::int16_t stored,
                                  std::size_t x, std::size_t y, std::uint32_t tick);

// Additional pictures drawn for an animated tile. The stored animation heads remain selectable.
[[nodiscard]] bool isTileAnimationFrame(GameProfile profile, SectionKind layer, std::int16_t value);

// The same tile as the games would draw it at tick 0 with animation off:
// the stored number with its draw order flags.
[[nodiscard]] TileFrame stillTileFrame(GameProfile profile, SectionKind layer,
                                       std::int16_t stored);

// Resolve the shared placement icon of a coloured key to its first game frame.
// Already stored animation frames and custom pictures retain their exact appearance.
[[nodiscard]] std::int16_t stillObjectIcon(GameProfile profile, std::int16_t type,
                                           std::int16_t channel, std::int16_t icon);

// Where and how one object is shown at a moment of the animation.
struct ObjectPose {
    Point position;
    std::int16_t channel = 0;
    std::int16_t icon = 0;

    friend constexpr bool operator==(const ObjectPose &, const ObjectPose &) = default;
};

// The games keep stepping every object while their editor is open: objects
// with different start and end travel there and back with their stored
// durations and waits, and most types change their picture every few
// ticks. ObjectAnimation reproduces that for display. It works on copies:
// the document is only read.
//
// Like the games' editors on entry, every object starts at posStart,
// waiting, with its counters at zero - not at the run-time state that
// happens to be stored.
class ObjectAnimation {
public:
    // Takes over the world's records. A slot whose stored record is the same
    // as at the previous call keeps running; a new or changed one starts
    // again from its posStart. A changed profile restarts everything.
    void sync(const WorldView &world, GameProfile profile);
    // Starts every object again and sets the tick to zero.
    void restart();
    // One tick is one step of the games (20 per second by default).
    void advance(std::uint32_t ticks);

    [[nodiscard]] std::uint32_t tick() const { return tick_; }
    // One pose per stored slot; for a free slot the pose is meaningless.
    [[nodiscard]] const std::vector<ObjectPose> &poses() const { return poses_; }

    // The motion state of a slot, for tests: 1 waiting at the start,
    // 2 advancing, 3 waiting at the end, 4 returning.
    [[nodiscard]] int step(std::size_t slot) const;
    [[nodiscard]] int time(std::size_t slot) const;

private:
    struct Slot {
        ObjectRecord stored;
        Point position;
        std::int16_t step = 1;
        std::int32_t time = 0;
        std::int32_t phase = 0;
        std::int16_t channel = 0;
        std::int16_t icon = 0;
    };
    void reset(Slot &slot, const ObjectRecord &record);
    void stepSlot(Slot &slot);

    GameProfile profile_ = GameProfile::unknown;
    std::uint32_t tick_ = 0;
    std::vector<Slot> slots_;
    std::vector<ObjectPose> poses_;
};

} // namespace eggbert
