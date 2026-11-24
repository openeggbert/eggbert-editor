// SPDX-License-Identifier: GPL-3.0-only
#include "world/world_view.hpp"

#include <algorithm>

#include "world/le_reader.hpp"

namespace eggbert {

namespace {

// Callers have already established that the range is inside `bytes`, so a
// failed read can only mean a broken invariant; it then decodes as zero
// rather than touching memory outside the span.
std::int16_t i16At(std::span<const std::byte> bytes, std::size_t offset) {
    return readI16(bytes, offset).value_or(0);
}

std::int32_t i32At(std::span<const std::byte> bytes, std::size_t offset) {
    return readI32(bytes, offset).value_or(0);
}

Point pointAt(std::span<const std::byte> bytes, std::size_t offset) {
    return {i32At(bytes, offset), i32At(bytes, offset + 4)};
}

bool allZero(std::span<const std::byte> bytes) {
    return std::ranges::all_of(bytes, [](std::byte value) { return value == std::byte{0}; });
}

} // namespace

std::int16_t HeaderView::majRev() const {
    return i16At(bytes_, headerField(HeaderField::majRev).offset);
}

std::int16_t HeaderView::minRev() const {
    return i16At(bytes_, headerField(HeaderField::minRev).offset);
}

Point HeaderView::posDecor() const {
    return pointAt(bytes_, headerField(HeaderField::posDecor).offset);
}

Point HeaderView::dimDecor() const {
    return pointAt(bytes_, headerField(HeaderField::dimDecor).offset);
}

Point HeaderView::playfieldPixels() const {
    const auto scroll = dimDecor();
    return {scroll.x == 0 ? 640 : layout::worldPixelWidth,
            scroll.y == 0 ? 480 : layout::worldPixelHeight};
}

std::int16_t HeaderView::world() const {
    return i16At(bytes_, headerField(HeaderField::world).offset);
}

std::int16_t HeaderView::music() const {
    return i16At(bytes_, headerField(HeaderField::music).offset);
}

std::int16_t HeaderView::region() const {
    return i16At(bytes_, headerField(HeaderField::region).offset);
}

std::optional<std::int32_t> HeaderView::reserve1(std::size_t index) const {
    const auto &field = headerField(HeaderField::reserve1);
    if (index >= field.elementCount) {
        return std::nullopt;
    }
    return i32At(bytes_, field.elementRange(index).offset);
}

std::optional<std::int16_t> HeaderView::reserve2(std::size_t index) const {
    const auto &field = headerField(HeaderField::reserve2);
    if (index >= field.elementCount) {
        return std::nullopt;
    }
    return i16At(bytes_, field.elementRange(index).offset);
}

std::optional<std::int16_t> HeaderView::reserve3(std::size_t index) const {
    const auto &field = headerField(HeaderField::reserve3);
    if (index >= field.elementCount) {
        return std::nullopt;
    }
    return i16At(bytes_, field.elementRange(index).offset);
}

std::optional<Point> HeaderView::blupiPos(std::size_t index) const {
    const auto &field = headerField(HeaderField::blupiPos);
    if (index >= field.elementCount) {
        return std::nullopt;
    }
    return pointAt(bytes_, field.elementRange(index).offset);
}

std::optional<std::int32_t> HeaderView::blupiDir(std::size_t index) const {
    const auto &field = headerField(HeaderField::blupiDir);
    if (index >= field.elementCount) {
        return std::nullopt;
    }
    return i32At(bytes_, field.elementRange(index).offset);
}

std::span<const std::byte> HeaderView::nameBytes() const {
    return fieldBytes(HeaderField::name);
}

std::size_t HeaderView::nameLength() const {
    const auto name = nameBytes();
    return static_cast<std::size_t>(std::ranges::find(name, std::byte{0}) - name.begin());
}

bool HeaderView::nameTerminated() const {
    return nameLength() < nameBytes().size();
}

std::span<const std::byte> HeaderView::nameText() const {
    return nameBytes().first(nameLength());
}

std::span<const std::byte> HeaderView::namePadding() const {
    const auto name = nameBytes();
    const auto length = nameLength();
    return length < name.size() ? name.subspan(length + 1) : std::span<const std::byte>{};
}

bool HeaderView::namePaddingIsZero() const {
    return allZero(namePadding());
}

bool HeaderView::reservedIsZero(HeaderField field) const {
    return allZero(fieldBytes(field));
}

std::span<const std::byte> HeaderView::fieldBytes(HeaderField field) const {
    const auto &descriptor = headerField(field);
    return bytes_.subspan(descriptor.offset, descriptor.size());
}

std::optional<std::int16_t> TileLayerView::cell(std::size_t x, std::size_t y) const {
    if (!contains(x, y)) {
        return std::nullopt;
    }
    return i16At(bytes_, relativeCellOffset(x, y));
}

std::optional<ByteRange> TileLayerView::cellRange(std::size_t x, std::size_t y) const {
    if (!contains(x, y)) {
        return std::nullopt;
    }
    return ByteRange{fileOffset_ + relativeCellOffset(x, y), layout::tileBytes};
}

std::optional<ObjectRecord> ObjectPoolView::record(std::size_t slot) const {
    const auto raw = slotBytes(slot);
    if (raw.empty()) {
        return std::nullopt;
    }
    const auto i16 = [&](ObjectField field) { return i16At(raw, objectField(field).offset); };
    const auto point = [&](ObjectField field) { return pointAt(raw, objectField(field).offset); };
    ObjectRecord record;
    record.type = i16(ObjectField::type);
    record.stepAdvance = i16(ObjectField::stepAdvance);
    record.stepRecede = i16(ObjectField::stepRecede);
    record.timeStopStart = i16(ObjectField::timeStopStart);
    record.timeStopEnd = i16(ObjectField::timeStopEnd);
    record.unknown1 = i16(ObjectField::unknown1);
    record.posStart = point(ObjectField::posStart);
    record.posEnd = point(ObjectField::posEnd);
    record.posCurrent = point(ObjectField::posCurrent);
    record.step = i16(ObjectField::step);
    record.time = i16(ObjectField::time);
    record.phase = i16(ObjectField::phase);
    record.channel = i16(ObjectField::channel);
    record.icon = i16(ObjectField::icon);
    record.unknown2 = i16(ObjectField::unknown2);
    return record;
}

std::optional<ByteRange> ObjectPoolView::slotRange(std::size_t slot) const {
    if (slot >= slotCount_) {
        return std::nullopt;
    }
    return ByteRange{fileOffset_ + slot * layout::objectRecordBytes, layout::objectRecordBytes};
}

std::optional<ByteRange> ObjectPoolView::fieldRange(std::size_t slot, ObjectField field) const {
    const auto range = slotRange(slot);
    if (!range) {
        return std::nullopt;
    }
    const auto &descriptor = objectField(field);
    return ByteRange{range->offset + descriptor.offset, descriptor.size()};
}

std::span<const std::byte> ObjectPoolView::slotBytes(std::size_t slot) const {
    if (slot >= slotCount_) {
        return {};
    }
    return bytes_.subspan(slot * layout::objectRecordBytes, layout::objectRecordBytes);
}

std::size_t ObjectPoolView::activeCount() const {
    std::size_t active = 0;
    for (std::size_t slot = 0; slot < slotCount_; ++slot) {
        if (i16At(slotBytes(slot), objectField(ObjectField::type).offset) != 0) {
            ++active;
        }
    }
    return active;
}

WorldView::WorldView(std::span<const std::byte> bytes)
    : bytes_(bytes), probe_(probeWorld(bytes)) {}

WorldView::WorldView(std::span<const std::byte> bytes, const WorldProbe &probe)
    : bytes_(bytes), probe_(probe) {}

std::span<const std::byte> WorldView::completeSection(SectionKind kind) const {
    const auto *section = probe_.section(kind);
    if (section == nullptr || !section->complete() ||
        !rangeFits(bytes_.size(), section->offset, section->size)) {
        return {};
    }
    return bytes_.subspan(section->offset, section->size);
}

std::optional<HeaderView> WorldView::header() const {
    const auto section = completeSection(SectionKind::header);
    if (section.empty()) {
        return std::nullopt;
    }
    return HeaderView(section);
}

std::optional<TileLayerView> WorldView::layer(SectionKind kind) const {
    if (kind != SectionKind::foreground && kind != SectionKind::bigDecor) {
        return std::nullopt;
    }
    const auto section = completeSection(kind);
    if (section.empty()) {
        return std::nullopt;
    }
    return TileLayerView(kind, probe_.section(kind)->offset, section);
}

std::optional<TileLayerView> WorldView::foreground() const {
    return layer(SectionKind::foreground);
}

std::optional<TileLayerView> WorldView::bigDecor() const {
    return layer(SectionKind::bigDecor);
}

std::optional<ObjectPoolView> WorldView::objects() const {
    const auto section = completeSection(SectionKind::objects);
    if (section.empty()) {
        return std::nullopt;
    }
    return ObjectPoolView(probe_.section(SectionKind::objects)->offset,
                          probe_.descriptor->objectCount, section);
}

ByteRange WorldView::suffixRange() const {
    if (probe_.surplusBytes == 0 || !probe_.expectedBytes) {
        return {};
    }
    return {*probe_.expectedBytes, probe_.surplusBytes};
}

std::span<const std::byte> WorldView::suffixBytes() const {
    const auto range = suffixRange();
    if (range.empty() || !rangeFits(bytes_.size(), range.offset, range.size)) {
        return {};
    }
    return bytes_.subspan(range.offset, range.size);
}

} // namespace eggbert
