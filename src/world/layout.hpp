// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace eggbert {

enum class GameProfile { unknown, speedyBlupi1, speedyBlupi2 };
enum class FormatRevision { unknown, v1_0, v1_1, v1_2, v1_3 };

// What the 1.0 product may do with a structurally sound file of a revision.
enum class WritePolicy { readOnly, losslessEdit };

enum class SectionKind { header, foreground, bigDecor, objects };

namespace layout {

inline constexpr std::size_t mapWidth = 100;
inline constexpr std::size_t mapHeight = 100;
inline constexpr std::size_t tileBytes = 2;
inline constexpr std::size_t headerBytes = 868;
inline constexpr std::size_t tileLayerBytes = mapWidth * mapHeight * tileBytes;
inline constexpr std::size_t objectRecordBytes = 48;
inline constexpr std::size_t revisionFieldBytes = 4;

} // namespace layout

// A half-open range of absolute file offsets.
struct ByteRange {
    std::size_t offset = 0;
    std::size_t size = 0;

    [[nodiscard]] constexpr std::size_t end() const { return offset + size; }
    [[nodiscard]] constexpr bool empty() const { return size == 0; }
    [[nodiscard]] constexpr bool contains(std::size_t position) const {
        return position >= offset && position - offset < size;
    }
    [[nodiscard]] constexpr bool overlaps(const ByteRange &other) const {
        return size != 0 && other.size != 0 && offset < other.end() && other.offset < end();
    }
    friend constexpr bool operator==(const ByteRange &, const ByteRange &) = default;
};

// Windows POINT on disk: two signed 32-bit little-endian values.
struct Point {
    std::int32_t x = 0;
    std::int32_t y = 0;

    friend constexpr bool operator==(const Point &, const Point &) = default;
};

enum class WireType { int16, int32, point, bytes };

[[nodiscard]] constexpr std::size_t wireBytes(WireType type) {
    switch (type) {
    case WireType::int16:
        return 2;
    case WireType::int32:
        return 4;
    case WireType::point:
        return 8;
    case WireType::bytes:
        return 1;
    }
    return 0;
}

enum class HeaderField {
    majRev,
    minRev,
    reserve1,
    posDecor,
    dimDecor,
    world,
    music,
    region,
    reserve2,
    blupiPos,
    blupiDir,
    name,
    reserve3,
};

struct HeaderFieldDescriptor {
    HeaderField field{};
    std::string_view name;
    std::size_t offset = 0;
    WireType type{};
    std::size_t elementCount = 0;

    [[nodiscard]] constexpr std::size_t elementBytes() const { return wireBytes(type); }
    [[nodiscard]] constexpr std::size_t size() const { return elementBytes() * elementCount; }
    [[nodiscard]] constexpr ByteRange range() const { return {offset, size()}; }
    // Precondition: index < elementCount.
    [[nodiscard]] constexpr ByteRange elementRange(std::size_t index) const {
        return {offset + index * elementBytes(), elementBytes()};
    }
};

// The 868-byte header, identical in every known revision.
inline constexpr std::array<HeaderFieldDescriptor, 13> headerFields{{
    {HeaderField::majRev, "majRev", 0, WireType::int16, 1},
    {HeaderField::minRev, "minRev", 2, WireType::int16, 1},
    {HeaderField::reserve1, "reserve1", 4, WireType::int32, 50},
    {HeaderField::posDecor, "posDecor", 204, WireType::point, 1},
    {HeaderField::dimDecor, "dimDecor", 212, WireType::point, 1},
    {HeaderField::world, "world", 220, WireType::int16, 1},
    {HeaderField::music, "music", 222, WireType::int16, 1},
    {HeaderField::region, "region", 224, WireType::int16, 1},
    {HeaderField::reserve2, "reserve2", 226, WireType::int16, 51},
    {HeaderField::blupiPos, "blupiPos", 328, WireType::point, 4},
    {HeaderField::blupiDir, "blupiDir", 360, WireType::int32, 4},
    {HeaderField::name, "name", 376, WireType::bytes, 100},
    {HeaderField::reserve3, "reserve3", 476, WireType::int16, 196},
}};

[[nodiscard]] constexpr const HeaderFieldDescriptor &headerField(HeaderField field) {
    return headerFields[static_cast<std::size_t>(field)];
}

enum class ObjectField {
    type,
    stepAdvance,
    stepRecede,
    timeStopStart,
    timeStopEnd,
    unknown1,
    posStart,
    posEnd,
    posCurrent,
    step,
    time,
    phase,
    channel,
    icon,
    unknown2,
};

struct ObjectFieldDescriptor {
    ObjectField field{};
    std::string_view name;
    // Offset inside the 48-byte record.
    std::size_t offset = 0;
    WireType type{};

    [[nodiscard]] constexpr std::size_t size() const { return wireBytes(type); }
};

inline constexpr std::array<ObjectFieldDescriptor, 15> objectFields{{
    {ObjectField::type, "type", 0, WireType::int16},
    {ObjectField::stepAdvance, "stepAdvance", 2, WireType::int16},
    {ObjectField::stepRecede, "stepRecede", 4, WireType::int16},
    {ObjectField::timeStopStart, "timeStopStart", 6, WireType::int16},
    {ObjectField::timeStopEnd, "timeStopEnd", 8, WireType::int16},
    {ObjectField::unknown1, "unknown1", 10, WireType::int16},
    {ObjectField::posStart, "posStart", 12, WireType::point},
    {ObjectField::posEnd, "posEnd", 20, WireType::point},
    {ObjectField::posCurrent, "posCurrent", 28, WireType::point},
    {ObjectField::step, "step", 36, WireType::int16},
    {ObjectField::time, "time", 38, WireType::int16},
    {ObjectField::phase, "phase", 40, WireType::int16},
    {ObjectField::channel, "channel", 42, WireType::int16},
    {ObjectField::icon, "icon", 44, WireType::int16},
    {ObjectField::unknown2, "unknown2", 46, WireType::int16},
}};

[[nodiscard]] constexpr const ObjectFieldDescriptor &objectField(ObjectField field) {
    return objectFields[static_cast<std::size_t>(field)];
}

struct SectionDescriptor {
    SectionKind kind{};
    std::size_t offset = 0;
    std::size_t size = 0;

    [[nodiscard]] constexpr std::size_t end() const { return offset + size; }
};

// Immutable description of one on-disk revision. The byte layout depends only
// on the revision; the game profile never changes it.
struct RevisionDescriptor {
    FormatRevision revision = FormatRevision::unknown;
    std::int16_t major = 0;
    std::int16_t minor = 0;
    std::size_t headerBytes = 0;
    bool hasBigDecor = false;
    std::size_t tileLayerCount = 0;
    std::size_t objectCount = 0;
    std::size_t objectRecordBytes = 0;
    std::size_t expectedBytes = 0;
    WritePolicy writePolicy = WritePolicy::readOnly;
    std::string_view rule;
    std::size_t sectionCount = 0;
    std::array<SectionDescriptor, 4> sectionStorage{};

    [[nodiscard]] constexpr std::span<const SectionDescriptor> sections() const {
        return std::span<const SectionDescriptor>(sectionStorage).first(sectionCount);
    }

    [[nodiscard]] constexpr const SectionDescriptor *section(SectionKind kind) const {
        for (const auto &candidate : sections()) {
            if (candidate.kind == kind) {
                return &candidate;
            }
        }
        return nullptr;
    }
};

namespace detail {

constexpr RevisionDescriptor makeRevision(FormatRevision revision, std::int16_t minor,
                                          bool hasBigDecor, std::size_t objectCount,
                                          WritePolicy policy, std::string_view rule) {
    RevisionDescriptor result;
    result.revision = revision;
    result.major = 1;
    result.minor = minor;
    result.headerBytes = layout::headerBytes;
    result.hasBigDecor = hasBigDecor;
    result.tileLayerCount = hasBigDecor ? 2 : 1;
    result.objectCount = objectCount;
    result.objectRecordBytes = layout::objectRecordBytes;
    result.writePolicy = policy;
    result.rule = rule;

    std::size_t offset = 0;
    const auto append = [&](SectionKind kind, std::size_t size) {
        result.sectionStorage[result.sectionCount++] = {kind, offset, size};
        offset += size;
    };
    append(SectionKind::header, layout::headerBytes);
    append(SectionKind::foreground, layout::tileLayerBytes);
    if (hasBigDecor) {
        append(SectionKind::bigDecor, layout::tileLayerBytes);
    }
    append(SectionKind::objects, objectCount * layout::objectRecordBytes);
    result.expectedBytes = offset;
    return result;
}

} // namespace detail

inline constexpr std::array<RevisionDescriptor, 4> knownRevisions{
    detail::makeRevision(
        FormatRevision::v1_0, 0, false, 100, WritePolicy::readOnly,
        "Header 1.0: foreground only, no bigDecor layer, 100 stored object records."),
    detail::makeRevision(
        FormatRevision::v1_1, 1, true, 100, WritePolicy::readOnly,
        "Header 1.1: foreground and bigDecor layers, 100 stored object records."),
    detail::makeRevision(
        FormatRevision::v1_2, 2, true, 200, WritePolicy::losslessEdit,
        "Header 1.2: foreground and bigDecor layers, 200 stored object records."),
    detail::makeRevision(
        FormatRevision::v1_3, 3, true, 200, WritePolicy::losslessEdit,
        "Header 1.3: foreground and bigDecor layers, 200 stored object records."),
};

// Returns nullptr for FormatRevision::unknown.
[[nodiscard]] constexpr const RevisionDescriptor *findRevision(FormatRevision revision) {
    for (const auto &descriptor : knownRevisions) {
        if (descriptor.revision == revision) {
            return &descriptor;
        }
    }
    return nullptr;
}

// Only the four exact pairs are recognized. A historical reader would follow
// its newest branch for a later pair; the editor treats that as unverified.
[[nodiscard]] constexpr FormatRevision revisionFromHeader(std::int16_t major,
                                                          std::int16_t minor) {
    for (const auto &descriptor : knownRevisions) {
        if (descriptor.major == major && descriptor.minor == minor) {
            return descriptor.revision;
        }
    }
    return FormatRevision::unknown;
}

// True when the pair is numerically newer than every recognized revision.
[[nodiscard]] constexpr bool isLaterThanKnownRevisions(std::int16_t major, std::int16_t minor) {
    return major > 1 || (major == 1 && minor > 3);
}

[[nodiscard]] constexpr std::string_view sectionName(SectionKind kind) {
    switch (kind) {
    case SectionKind::header:
        return "header";
    case SectionKind::foreground:
        return "foreground";
    case SectionKind::bigDecor:
        return "bigDecor";
    case SectionKind::objects:
        return "objects";
    }
    return "unknown";
}

} // namespace eggbert
