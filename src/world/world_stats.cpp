// SPDX-License-Identifier: GPL-3.0-only
#include "world/world_stats.hpp"

#include <algorithm>

namespace eggbert {

namespace {

void extend(std::optional<CellBounds> &bounds, std::size_t x, std::size_t y) {
    if (!bounds) {
        bounds = CellBounds{x, y, x, y};
        return;
    }
    bounds->minX = std::min(bounds->minX, x);
    bounds->minY = std::min(bounds->minY, y);
    bounds->maxX = std::max(bounds->maxX, x);
    bounds->maxY = std::max(bounds->maxY, y);
}

LayerStats layerStats(const std::optional<TileLayerView> &layer) {
    LayerStats stats;
    if (!layer) {
        return stats;
    }
    stats.present = true;
    for (std::size_t x = 0; x < layout::mapWidth; ++x) {
        for (std::size_t y = 0; y < layout::mapHeight; ++y) {
            const auto value = *layer->cell(x, y);
            if (value == layout::emptyCell) {
                ++stats.empty;
                continue;
            }
            ++stats.occupied;
            ++stats.valueCounts[value];
            extend(stats.bounds, x, y);
        }
    }
    return stats;
}

} // namespace

WorldStats computeWorldStats(const WorldView &world) {
    WorldStats stats;
    const auto &probe = world.probe();
    stats.fileBytes = probe.inputBytes;
    stats.headerMajor = probe.headerMajor;
    stats.headerMinor = probe.headerMinor;
    stats.revision = probe.candidateRevision;
    stats.surplusBytes = probe.surplusBytes;
    stats.missingBytes = probe.missingBytes;

    if (const auto header = world.header()) {
        stats.headerPresent = true;
        for (std::size_t index = 0; index < 4; ++index) {
            stats.startPositions[index] = *header->blupiPos(index);
            stats.startDirections[index] = *header->blupiDir(index);
        }
    }

    stats.foreground = layerStats(world.foreground());
    stats.bigDecor = layerStats(world.bigDecor());
    for (const auto *layer : {&stats.foreground, &stats.bigDecor}) {
        if (layer->bounds) {
            extend(stats.usedBounds, layer->bounds->minX, layer->bounds->minY);
            extend(stats.usedBounds, layer->bounds->maxX, layer->bounds->maxY);
        }
    }

    if (const auto pool = world.objects()) {
        auto &objects = stats.objects;
        objects.present = true;
        objects.stored = pool->slotCount();
        for (std::size_t slot = 0; slot < pool->slotCount(); ++slot) {
            const auto record = *pool->record(slot);
            if (record.isFree()) {
                ++objects.free;
                const auto raw = pool->slotBytes(slot);
                if (std::ranges::any_of(raw, [](std::byte value) { return value != std::byte{0}; })) {
                    ++objects.freeWithOpaqueBytes;
                }
                continue;
            }
            ++objects.active;
            ++objects.byType[record.type];
            ++objects.byChannel[record.channel];
            ++objects.byIcon[record.icon];
            if (record.posStart != record.posEnd) {
                ++objects.withPath;
            }
        }
    }
    return stats;
}

std::vector<CellPos> findCells(const WorldView &world, SectionKind layer, std::int16_t value) {
    std::vector<CellPos> cells;
    const auto view = world.layer(layer);
    if (!view) {
        return cells;
    }
    for (std::size_t x = 0; x < layout::mapWidth; ++x) {
        for (std::size_t y = 0; y < layout::mapHeight; ++y) {
            if (*view->cell(x, y) == value) {
                cells.push_back({x, y});
            }
        }
    }
    return cells;
}

std::vector<std::size_t> findActiveSlots(const WorldView &world, ObjectKey key,
                                         std::int16_t value) {
    std::vector<std::size_t> slots;
    const auto pool = world.objects();
    if (!pool) {
        return slots;
    }
    for (std::size_t slot = 0; slot < pool->slotCount(); ++slot) {
        const auto record = *pool->record(slot);
        if (record.isFree()) {
            continue;
        }
        const auto candidate = key == ObjectKey::type      ? record.type
                               : key == ObjectKey::channel ? record.channel
                                                           : record.icon;
        if (candidate == value) {
            slots.push_back(slot);
        }
    }
    return slots;
}

} // namespace eggbert
