// SPDX-License-Identifier: GPL-3.0-only
#include "world/object_ops.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace eggbert {

namespace {

std::optional<Point> shifted(Point position, Point delta) {
    const auto x = static_cast<long long>(position.x) + delta.x;
    const auto y = static_cast<long long>(position.y) + delta.y;
    if (x < std::numeric_limits<std::int32_t>::min() ||
        x > std::numeric_limits<std::int32_t>::max() ||
        y < std::numeric_limits<std::int32_t>::min() ||
        y > std::numeric_limits<std::int32_t>::max()) {
        return std::nullopt;
    }
    return Point{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
}

ObjectOpResult<ObjectRecord> activeRecord(const EditBuilder &builder, std::size_t slot) {
    const auto pool = builder.view().objects();
    if (!pool) {
        return std::unexpected(ObjectOpError::noObjectPool);
    }
    const auto record = pool->record(slot);
    if (!record) {
        return std::unexpected(ObjectOpError::invalidSlot);
    }
    if (record->isFree()) {
        return std::unexpected(ObjectOpError::slotIsFree);
    }
    return *record;
}

Point cellOrigin(CellPos cell) {
    return {static_cast<std::int32_t>(cell.x) * layout::cellPixels,
            static_cast<std::int32_t>(cell.y) * layout::cellPixels};
}

bool inMap(CellPos cell) {
    return cell.x < layout::mapWidth && cell.y < layout::mapHeight;
}

} // namespace

PoolCapacity poolCapacity(const WorldView &world) {
    PoolCapacity capacity;
    if (const auto pool = world.objects()) {
        capacity.stored = pool->slotCount();
        capacity.active = pool->activeCount();
        capacity.free = capacity.stored - capacity.active;
    }
    return capacity;
}

std::vector<std::size_t> freeSlots(const WorldView &world, std::size_t count) {
    std::vector<std::size_t> result;
    const auto pool = world.objects();
    if (!pool) {
        return result;
    }
    for (std::size_t slot = 0; slot < pool->slotCount() && result.size() < count; ++slot) {
        if (pool->record(slot)->isFree()) {
            result.push_back(slot);
        }
    }
    return result;
}

int spriteBoxSize(std::int16_t channel) {
    if (channel == 1) {
        return layout::cellPixels;
    }
    if (channel == 9) {
        return 2 * layout::cellPixels;
    }
    return 60;
}

ObjectOpResult<ObjectRecord> recordForRequest(const ObjectRequest &request) {
    if (request.preset == nullptr) {
        return std::unexpected(ObjectOpError::notApplicable);
    }
    const auto &preset = *request.preset;
    if (!inMap(request.cell) || (request.pathEnd && !inMap(*request.pathEnd))) {
        return std::unexpected(ObjectOpError::outsideMap);
    }
    if ((request.pathEnd && !preset.hasPath) || (request.icon && !preset.iconIsChoice)) {
        return std::unexpected(ObjectOpError::notApplicable);
    }
    ObjectRecord record; // every field zero
    record.type = preset.type;
    record.stepAdvance = preset.stepAdvance;
    record.stepRecede = preset.stepRecede;
    record.timeStopStart = preset.timeStopStart;
    record.timeStopEnd = preset.timeStopEnd;
    const auto origin = cellOrigin(request.cell);
    record.posStart = {origin.x + preset.offsetX, origin.y + preset.offsetY};
    record.posEnd = record.posStart;
    record.posCurrent = record.posStart;
    record.step = stepStopStart;
    record.channel = preset.channel;
    record.icon = request.icon.value_or(preset.icon);
    if (request.pathEnd && *request.pathEnd != request.cell) {
        record.posEnd = cellOrigin(*request.pathEnd);
        const auto cells =
            std::max(std::abs(record.posStart.x - record.posEnd.x),
                     std::abs(record.posStart.y - record.posEnd.y)) /
            layout::cellPixels;
        // At most 99 cells times a base of at most 30: fits in 16 bits.
        record.stepAdvance = static_cast<std::int16_t>(preset.stepAdvance * cells);
        record.stepRecede = static_cast<std::int16_t>(preset.stepRecede * cells);
    }
    return record;
}

ObjectOpResult<std::vector<std::size_t>> createObjects(EditBuilder &builder,
                                                       std::span<const ObjectRequest> requests) {
    const auto view = builder.view();
    if (!view.objects()) {
        return std::unexpected(ObjectOpError::noObjectPool);
    }
    // Validate everything and count slot demand before the first write.
    std::vector<ObjectRecord> records;
    records.reserve(requests.size());
    for (const auto &request : requests) {
        const auto record = recordForRequest(request);
        if (!record) {
            return std::unexpected(record.error());
        }
        records.push_back(*record);
    }
    const auto slots = freeSlots(view, records.size());
    if (slots.size() < records.size()) {
        return std::unexpected(ObjectOpError::poolFull);
    }
    for (std::size_t index = 0; index < records.size(); ++index) {
        builder.setObjectRecord(slots[index], records[index]);
    }
    return slots;
}

ObjectOpResult<std::size_t> createObject(EditBuilder &builder, const ObjectRequest &request) {
    const ObjectRequest requests[] = {request};
    const auto slots = createObjects(builder, requests);
    if (!slots) {
        return std::unexpected(slots.error());
    }
    return slots->front();
}

ObjectOpResult<void> deleteObject(EditBuilder &builder, std::size_t slot) {
    const auto record = activeRecord(builder, slot);
    if (!record) {
        return std::unexpected(record.error());
    }
    builder.setObjectValue(slot, ObjectField::type, 0);
    return {};
}

ObjectOpResult<std::size_t> duplicateObject(EditBuilder &builder, std::size_t slot, Point delta) {
    const auto record = activeRecord(builder, slot);
    if (!record) {
        return std::unexpected(record.error());
    }
    const auto start = shifted(record->posStart, delta);
    const auto end = shifted(record->posEnd, delta);
    const auto current = shifted(record->posCurrent, delta);
    if (!start || !end || !current) {
        return std::unexpected(ObjectOpError::outsideMap);
    }
    const auto slots = freeSlots(builder.view(), 1);
    if (slots.empty()) {
        return std::unexpected(ObjectOpError::poolFull);
    }
    // Raw copy first so opaque fields travel with the object.
    builder.copyObjectSlot(slot, slots.front());
    builder.setObjectPoint(slots.front(), ObjectField::posStart, *start);
    builder.setObjectPoint(slots.front(), ObjectField::posEnd, *end);
    builder.setObjectPoint(slots.front(), ObjectField::posCurrent, *current);
    return slots.front();
}

ObjectOpResult<void> translatePath(EditBuilder &builder, std::size_t slot, Point delta) {
    const auto record = activeRecord(builder, slot);
    if (!record) {
        return std::unexpected(record.error());
    }
    const auto start = shifted(record->posStart, delta);
    const auto end = shifted(record->posEnd, delta);
    const auto current = shifted(record->posCurrent, delta);
    if (!start || !end || !current) {
        return std::unexpected(ObjectOpError::outsideMap);
    }
    builder.setObjectPoint(slot, ObjectField::posStart, *start);
    builder.setObjectPoint(slot, ObjectField::posEnd, *end);
    builder.setObjectPoint(slot, ObjectField::posCurrent, *current);
    return {};
}

ObjectOpResult<void> relocateStatic(EditBuilder &builder, std::size_t slot, Point delta) {
    const auto record = activeRecord(builder, slot);
    if (!record) {
        return std::unexpected(record.error());
    }
    if (record->posStart != record->posEnd) {
        return std::unexpected(ObjectOpError::notApplicable);
    }
    return translatePath(builder, slot, delta);
}

ObjectOpResult<void> moveObject(EditBuilder &builder, std::size_t slot, Point delta) {
    return translatePath(builder, slot, delta);
}

ObjectOpResult<void> moveStart(EditBuilder &builder, std::size_t slot, Point position) {
    const auto record = activeRecord(builder, slot);
    if (!record) {
        return std::unexpected(record.error());
    }
    builder.setObjectPoint(slot, ObjectField::posStart, position);
    return {};
}

ObjectOpResult<void> moveEnd(EditBuilder &builder, std::size_t slot, Point position) {
    const auto record = activeRecord(builder, slot);
    if (!record) {
        return std::unexpected(record.error());
    }
    builder.setObjectPoint(slot, ObjectField::posEnd, position);
    return {};
}

std::vector<std::size_t> objectsAt(const WorldView &world, Point position) {
    std::vector<std::size_t> result;
    const auto pool = world.objects();
    if (!pool) {
        return result;
    }
    for (std::size_t slot = 0; slot < pool->slotCount(); ++slot) {
        const auto record = *pool->record(slot);
        if (record.isFree()) {
            continue;
        }
        const auto size = spriteBoxSize(record.channel);
        const auto dx = static_cast<long long>(position.x) - record.posCurrent.x;
        const auto dy = static_cast<long long>(position.y) - record.posCurrent.y;
        if (dx >= 0 && dy >= 0 && dx < size && dy < size) {
            result.push_back(slot);
        }
    }
    return result;
}

} // namespace eggbert
