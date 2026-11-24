// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

#include "world/object_catalog.hpp"
#include "world/world_document.hpp"

namespace eggbert {

struct PoolCapacity {
    // Records the revision stores (100 or 200); 0 when the pool is absent.
    std::size_t stored = 0;
    // Records with a non-zero type.
    std::size_t active = 0;
    std::size_t free = 0;
};

[[nodiscard]] PoolCapacity poolCapacity(const WorldView &world);
// Lowest free slots, at most `count` of them, ascending. Both engines take
// the lowest free slot for a new object.
[[nodiscard]] std::vector<std::size_t> freeSlots(const WorldView &world, std::size_t count);

enum class ObjectOpError {
    noObjectPool,
    // The operation needs more free slots than the pool has. Nothing was
    // changed.
    poolFull,
    invalidSlot,
    // The slot holds no object (type 0).
    slotIsFree,
    outsideMap,
    // The operation does not apply to this record (for example relocating a
    // record that has a path as if it were static).
    notApplicable,
};

template <typename Value>
using ObjectOpResult = std::expected<Value, ObjectOpError>;

// One object to create: a verified preset on a cell, optionally with a path
// end cell (only for presets that have a path) and a chosen icon (only for
// presets whose icon is a choice).
struct ObjectRequest {
    const ObjectPreset *preset = nullptr;
    CellPos cell;
    std::optional<CellPos> pathEnd;
    std::optional<std::int16_t> icon;
    std::optional<std::int16_t> channel = std::nullopt;
};

// The record a request produces, exactly as the games' editors fill it:
// all 48 bytes zero, then the preset values; posEnd and posCurrent equal
// posStart; step is 1. With a path end on another cell, posEnd is that
// cell's top-left corner and both step durations are multiplied by the
// path length in cells (the larger of the two axis distances).
[[nodiscard]] ObjectOpResult<ObjectRecord> recordForRequest(const ObjectRequest &request);

// Creates every requested object in the lowest free slots, or none at all
// when the pool cannot hold them. A free slot's previous bytes are replaced
// completely, as in the engines. Returns the slots used, in request order.
ObjectOpResult<std::vector<std::size_t>> createObjects(EditBuilder &builder,
                                                       std::span<const ObjectRequest> requests);
ObjectOpResult<std::size_t> createObject(EditBuilder &builder, const ObjectRequest &request);

// Frees a slot the way the engines do: only `type` becomes 0; the other 46
// bytes stay.
ObjectOpResult<void> deleteObject(EditBuilder &builder, std::size_t slot);

// Copies all 48 bytes of an active record into the lowest free slot and
// shifts its three positions by `delta`. Returns the new slot.
ObjectOpResult<std::size_t> duplicateObject(EditBuilder &builder, std::size_t slot, Point delta);

// Moves a record whose start and end coincide: posStart, posEnd and
// posCurrent all shift by `delta`. Fails with notApplicable when the record
// has a path.
ObjectOpResult<void> relocateStatic(EditBuilder &builder, std::size_t slot, Point delta);
// Changes exactly one endpoint.
ObjectOpResult<void> moveStart(EditBuilder &builder, std::size_t slot, Point position);
ObjectOpResult<void> moveEnd(EditBuilder &builder, std::size_t slot, Point position);
// Shifts posStart, posEnd and posCurrent by the same delta.
ObjectOpResult<void> translatePath(EditBuilder &builder, std::size_t slot, Point delta);
// Shifts the object as a whole: relocateStatic or translatePath as fits.
ObjectOpResult<void> moveObject(EditBuilder &builder, std::size_t slot, Point delta);

// Active slots whose sprite box at posCurrent contains the world position,
// ascending by slot. The box is 64 pixels for channel 1, 128 for channel 9
// and 60 otherwise.
[[nodiscard]] std::vector<std::size_t> objectsAt(const WorldView &world, Point position);
[[nodiscard]] int spriteBoxSize(std::int16_t channel);

} // namespace eggbert
