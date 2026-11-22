// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <optional>

#include "world/layout.hpp"
#include "world/world_probe.hpp"

namespace eggbert {

enum class OffsetArea {
    header,
    foreground,
    bigDecor,
    objects,
    // After the candidate layout: retained opaque bytes.
    suffix,
    // No layout is known, or the offset is not inside the input.
    unmapped,
};

// What a file offset means under the probed layout. Works for incomplete
// sections too: an offset is mapped whenever the byte exists in the input.
struct OffsetLocation {
    OffsetArea area = OffsetArea::unmapped;
    // Offset relative to the start of the area.
    std::size_t areaOffset = 0;

    // Header: field, element index inside the field and byte inside the element.
    std::optional<HeaderField> headerField;
    std::size_t elementIndex = 0;
    std::size_t byteInElement = 0;

    // Layers: the cell and which of its two bytes.
    CellPos cell;
    // Objects: slot, field and byte inside the field.
    std::size_t slot = 0;
    std::optional<ObjectField> objectField;

    // Full range of the element (header element, cell or object field) that
    // contains the offset; empty for suffix and unmapped offsets.
    ByteRange elementRange;
};

[[nodiscard]] OffsetLocation locateOffset(const WorldProbe &probe, std::size_t offset);

// Absolute byte range of a cell; nullopt when the revision has no such
// layer or the cell is outside the map. Does not require the bytes to exist.
[[nodiscard]] std::optional<ByteRange> cellByteRange(const RevisionDescriptor &descriptor,
                                                     SectionKind layer, CellPos cell);
[[nodiscard]] std::optional<ByteRange> slotByteRange(const RevisionDescriptor &descriptor,
                                                     std::size_t slot);

} // namespace eggbert
