// SPDX-License-Identifier: GPL-3.0-only
#include "world/offset_map.hpp"

namespace eggbert {

OffsetLocation locateOffset(const WorldProbe &probe, std::size_t offset) {
    OffsetLocation location;
    if (probe.descriptor == nullptr || offset >= probe.inputBytes) {
        return location;
    }
    const auto &descriptor = *probe.descriptor;
    if (offset >= descriptor.expectedBytes) {
        location.area = OffsetArea::suffix;
        location.areaOffset = offset - descriptor.expectedBytes;
        return location;
    }
    for (const auto &section : descriptor.sections()) {
        if (offset < section.offset || offset >= section.end()) {
            continue;
        }
        const auto relative = offset - section.offset;
        location.areaOffset = relative;
        switch (section.kind) {
        case SectionKind::header:
            location.area = OffsetArea::header;
            for (const auto &field : headerFields) {
                if (field.range().contains(offset)) {
                    location.headerField = field.field;
                    location.elementIndex = (offset - field.offset) / field.elementBytes();
                    location.byteInElement = (offset - field.offset) % field.elementBytes();
                    location.elementRange = field.elementRange(location.elementIndex);
                    break;
                }
            }
            break;
        case SectionKind::foreground:
        case SectionKind::bigDecor: {
            location.area = section.kind == SectionKind::foreground ? OffsetArea::foreground
                                                                    : OffsetArea::bigDecor;
            const auto cellIndex = relative / layout::tileBytes;
            location.cell = {cellIndex / layout::mapHeight, cellIndex % layout::mapHeight};
            location.byteInElement = relative % layout::tileBytes;
            location.elementRange = {section.offset + cellIndex * layout::tileBytes,
                                     layout::tileBytes};
            break;
        }
        case SectionKind::objects: {
            location.area = OffsetArea::objects;
            location.slot = relative / layout::objectRecordBytes;
            const auto inRecord = relative % layout::objectRecordBytes;
            for (const auto &field : objectFields) {
                if (inRecord >= field.offset && inRecord < field.offset + field.size()) {
                    location.objectField = field.field;
                    location.byteInElement = inRecord - field.offset;
                    location.elementRange = {
                        section.offset + location.slot * layout::objectRecordBytes + field.offset,
                        field.size()};
                    break;
                }
            }
            break;
        }
        }
        break;
    }
    return location;
}

std::optional<ByteRange> cellByteRange(const RevisionDescriptor &descriptor, SectionKind layer,
                                       CellPos cell) {
    if ((layer != SectionKind::foreground && layer != SectionKind::bigDecor) ||
        cell.x >= layout::mapWidth || cell.y >= layout::mapHeight) {
        return std::nullopt;
    }
    const auto *section = descriptor.section(layer);
    if (section == nullptr) {
        return std::nullopt;
    }
    return ByteRange{section->offset + layout::tileBytes * (layout::mapHeight * cell.x + cell.y),
                     layout::tileBytes};
}

std::optional<ByteRange> slotByteRange(const RevisionDescriptor &descriptor, std::size_t slot) {
    const auto *section = descriptor.section(SectionKind::objects);
    if (section == nullptr || slot >= descriptor.objectCount) {
        return std::nullopt;
    }
    return ByteRange{section->offset + slot * layout::objectRecordBytes,
                     layout::objectRecordBytes};
}

} // namespace eggbert
