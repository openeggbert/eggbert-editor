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
