// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "world/world_document.hpp"

namespace eggbert {

// Source of the random choices an adaptive edit makes. Injected so tests and
// replays are deterministic; an edit's result is stored as bytes, so undo
// and redo never draw again.
class VariantSource {
public:
    virtual ~VariantSource() = default;
    // True to take the alternative look of a tile (the engines' coin flip).
    virtual bool coin() = 0;
    // A value in [0, count): which of `count` equally likely looks to use.
    virtual int choice(int count) = 0;
};

// Never an alternative: always the base look.
class NoVariants final : public VariantSource {
public:
    bool coin() override { return false; }
    int choice(int) override { return 0; }
};

// Replays recorded draws in order: a coin is true for a non-zero value, a
// choice is the value clamped to the range. After the sequence it yields
// base looks.
class RecordedVariants final : public VariantSource {
public:
    explicit RecordedVariants(std::vector<int> values) : values_(std::move(values)) {}
    bool coin() override;
    int choice(int count) override;
    [[nodiscard]] std::size_t used() const { return next_; }

private:
    int take();

    std::vector<int> values_;
    std::size_t next_ = 0;
};

// Pseudo-random draws for interactive use, from a given seed.
class SeededVariants final : public VariantSource {
public:
    explicit SeededVariants(std::uint32_t seed) : state_(seed == 0 ? 0x9E3779B9u : seed) {}
    bool coin() override { return (next() & 1u) != 0; }
    int choice(int count) override { return static_cast<int>(next() % static_cast<unsigned>(count)); }

private:
    // xorshift32: small, deterministic and independent of the C library.
    std::uint32_t next() {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return state_ >> 8;
    }

    std::uint32_t state_;
};

// A tile family the adaptive brush can paint for a game. `seed` is the value
// written before the border forms are recomputed.
struct AdaptiveFamily {
    std::string_view id;
    std::string_view name;
    std::int16_t seed = 0;
    // True for families whose own cells keep their value while border tiles
    // are written into empty neighbours (Speedy Blupi 2 cheese and cave).
    bool bordersEmptyNeighbours = false;
    std::string_view provenance;
};

// Verified families of a profile; empty for GameProfile::unknown.
[[nodiscard]] std::span<const AdaptiveFamily> adaptiveFamilies(GameProfile profile);
[[nodiscard]] const AdaptiveFamily *findAdaptiveFamily(GameProfile profile, std::string_view id);

// Bits of the adjacency mask, as the engines number them.
inline constexpr unsigned joinBelow = 1;
inline constexpr unsigned joinAbove = 2;
inline constexpr unsigned joinRight = 4;
inline constexpr unsigned joinLeft = 8;

// Which of the four orthogonal neighbours of a cell count as joined under
// the game's rules. Cells outside the playfield count as joined; the
// playfield is 10 cells wide or 8 high on an axis whose scrolling is off.
[[nodiscard]] unsigned adjacencyMask(const WorldView &world, GameProfile profile, CellPos cell);

// The border form the game's editor would give a cell of a table family for
// the cell's current neighbours, without random variants; nullopt when the
// stored value belongs to no such family (or the profile is unknown).
[[nodiscard]] std::optional<std::int16_t> expectedBorderForm(const WorldView &world,
                                                             GameProfile profile, CellPos cell);
// The stored value with random variants mapped back to their base form, so
// it can be compared with expectedBorderForm.
[[nodiscard]] std::int16_t baseBorderForm(GameProfile profile, std::int16_t value);

struct AdaptiveResult {
    // Cells whose stored value differs after the edit, footprint first.
    std::vector<CellPos> changedFootprint;
    // Changed cells outside the footprint: neighbours that took a new
    // border form, border tiles written into empty cells, lightning beams.
    std::vector<CellPos> sideEffects;

    [[nodiscard]] std::size_t changedCount() const {
        return changedFootprint.size() + sideEffects.size();
    }
};

// Paints `value` into each footprint cell in order and, after each cell,
// recomputes the border forms of that cell and its four orthogonal
// neighbours exactly as the chosen game's level editor does (including
// Speedy Blupi 2's cheese/cave borders in empty cells and lightning beams).
// `value` may be -1 to erase adaptively. Only the foreground layer is
// touched and objects are never deleted. With an unknown profile nothing is
// done.
AdaptiveResult adaptiveStroke(EditBuilder &builder, GameProfile profile,
                              std::span<const CellPos> footprint, std::int16_t value,
                              VariantSource &variants);

} // namespace eggbert
