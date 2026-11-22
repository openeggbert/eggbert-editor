// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "world/layout.hpp"
#include "world/world_probe.hpp"

namespace eggbert {

// Read-only decoded header. Constructed only over a complete 868-byte header.
class HeaderView {
public:
    [[nodiscard]] std::int16_t majRev() const;
    [[nodiscard]] std::int16_t minRev() const;
    [[nodiscard]] Point posDecor() const;
    [[nodiscard]] Point dimDecor() const;
    [[nodiscard]] std::int16_t world() const;
    [[nodiscard]] std::int16_t music() const;
    [[nodiscard]] std::int16_t region() const;
    // Indexed fields return nullopt when the index is out of range.
    [[nodiscard]] std::optional<std::int32_t> reserve1(std::size_t index) const;
    [[nodiscard]] std::optional<std::int16_t> reserve2(std::size_t index) const;
    [[nodiscard]] std::optional<std::int16_t> reserve3(std::size_t index) const;
    [[nodiscard]] std::optional<Point> blupiPos(std::size_t index) const;
    [[nodiscard]] std::optional<std::int32_t> blupiDir(std::size_t index) const;

    // All 100 stored name bytes.
    [[nodiscard]] std::span<const std::byte> nameBytes() const;
    // Number of bytes before the first NUL; 100 when there is no terminator.
    [[nodiscard]] std::size_t nameLength() const;
    [[nodiscard]] bool nameTerminated() const;
    // Bytes before the first NUL: what a C-string reader would see.
    [[nodiscard]] std::span<const std::byte> nameText() const;
    // Bytes after the first NUL, retained even though no reader shows them.
    [[nodiscard]] std::span<const std::byte> namePadding() const;
    [[nodiscard]] bool namePaddingIsZero() const;

    // True when every element of the reserved field is zero.
    [[nodiscard]] bool reservedIsZero(HeaderField field) const;

    // Raw bytes of any field.
    [[nodiscard]] std::span<const std::byte> fieldBytes(HeaderField field) const;
    [[nodiscard]] std::span<const std::byte> bytes() const { return bytes_; }

private:
    friend class WorldView;
    explicit HeaderView(std::span<const std::byte> bytes) : bytes_(bytes) {}

    std::span<const std::byte> bytes_;
};

// Read-only 100 x 100 layer of signed 16-bit cells stored as [x][y].
class TileLayerView {
public:
    // Offset of cell (x, y) relative to the layer start. Precondition: both
    // coordinates are below 100.
    [[nodiscard]] static constexpr std::size_t relativeCellOffset(std::size_t x, std::size_t y) {
        return layout::tileBytes * (layout::mapHeight * x + y);
    }
    [[nodiscard]] static constexpr bool contains(std::size_t x, std::size_t y) {
        return x < layout::mapWidth && y < layout::mapHeight;
    }

    [[nodiscard]] SectionKind kind() const { return kind_; }
    // Absolute offset of the layer in the file.
    [[nodiscard]] std::size_t fileOffset() const { return fileOffset_; }
    // nullopt outside the map.
    [[nodiscard]] std::optional<std::int16_t> cell(std::size_t x, std::size_t y) const;
    [[nodiscard]] std::optional<ByteRange> cellRange(std::size_t x, std::size_t y) const;
    [[nodiscard]] std::span<const std::byte> bytes() const { return bytes_; }

private:
    friend class WorldView;
    TileLayerView(SectionKind kind, std::size_t fileOffset, std::span<const std::byte> bytes)
        : kind_(kind), fileOffset_(fileOffset), bytes_(bytes) {}

    SectionKind kind_;
    std::size_t fileOffset_;
    std::span<const std::byte> bytes_;
};

struct ObjectRecord {
    std::int16_t type = 0;
    std::int16_t stepAdvance = 0;
    std::int16_t stepRecede = 0;
    std::int16_t timeStopStart = 0;
    std::int16_t timeStopEnd = 0;
    std::int16_t unknown1 = 0;
    Point posStart;
    Point posEnd;
    Point posCurrent;
    std::int16_t step = 0;
    std::int16_t time = 0;
    std::int16_t phase = 0;
    std::int16_t channel = 0;
    std::int16_t icon = 0;
    std::int16_t unknown2 = 0;

    // The engines treat type 0 as a free slot. Its other bytes still belong
    // to the document.
    [[nodiscard]] constexpr bool isFree() const { return type == 0; }
    friend constexpr bool operator==(const ObjectRecord &, const ObjectRecord &) = default;
};

// Read-only object pool: 100 or 200 stored 48-byte records.
class ObjectPoolView {
public:
    [[nodiscard]] std::size_t slotCount() const { return slotCount_; }
    [[nodiscard]] std::size_t fileOffset() const { return fileOffset_; }
    // nullopt when slot >= slotCount().
    [[nodiscard]] std::optional<ObjectRecord> record(std::size_t slot) const;
    [[nodiscard]] std::optional<ByteRange> slotRange(std::size_t slot) const;
    [[nodiscard]] std::optional<ByteRange> fieldRange(std::size_t slot, ObjectField field) const;
    // Empty span when slot >= slotCount().
    [[nodiscard]] std::span<const std::byte> slotBytes(std::size_t slot) const;
    [[nodiscard]] std::size_t activeCount() const;
    [[nodiscard]] std::size_t freeCount() const { return slotCount_ - activeCount(); }

private:
    friend class WorldView;
    ObjectPoolView(std::size_t fileOffset, std::size_t slotCount,
                   std::span<const std::byte> bytes)
        : fileOffset_(fileOffset), slotCount_(slotCount), bytes_(bytes) {}

    std::size_t fileOffset_;
    std::size_t slotCount_;
    std::span<const std::byte> bytes_;
};

// Bounded read-only views over one input. A section view exists only when the
// probe found that section complete; nothing is copied or modified.
class WorldView {
public:
    explicit WorldView(std::span<const std::byte> bytes);
    // `probe` must be the probe of exactly these bytes.
    WorldView(std::span<const std::byte> bytes, const WorldProbe &probe);

    [[nodiscard]] const WorldProbe &probe() const { return probe_; }
    [[nodiscard]] std::span<const std::byte> bytes() const { return bytes_; }

    [[nodiscard]] std::optional<HeaderView> header() const;
    [[nodiscard]] std::optional<TileLayerView> foreground() const;
    // Absent for revision 1.0 as well as when incomplete.
    [[nodiscard]] std::optional<TileLayerView> bigDecor() const;
    [[nodiscard]] std::optional<TileLayerView> layer(SectionKind kind) const;
    [[nodiscard]] std::optional<ObjectPoolView> objects() const;

    // Bytes after the candidate layout; empty unless the probe found surplus.
    [[nodiscard]] ByteRange suffixRange() const;
    [[nodiscard]] std::span<const std::byte> suffixBytes() const;

private:
    [[nodiscard]] std::span<const std::byte> completeSection(SectionKind kind) const;

    std::span<const std::byte> bytes_;
    WorldProbe probe_;
};

} // namespace eggbert
