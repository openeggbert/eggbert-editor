// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include "world/layout.hpp"

namespace eggbert {

enum class ProbeStatus {
    // Fewer than four bytes: the revision field itself is incomplete.
    revisionUnavailable,
    // The revision pair is not one of 1.0, 1.1, 1.2 or 1.3.
    unknownRevision,
    // Recognized revision, but the input ends before the expected size.
    truncated,
    // Recognized revision and exactly the expected size.
    sizeMatches,
    // Recognized revision followed by bytes beyond the expected size.
    surplus,
};

struct ProbedSection {
    SectionKind kind{};
    std::size_t offset = 0;
    std::size_t size = 0;
    // Bytes of this section that the input actually contains.
    std::size_t availableBytes = 0;

    [[nodiscard]] constexpr bool complete() const { return availableBytes == size; }
};

// Read-only structural assessment of an input. The BLP world format has no
// signature, so no result of a probe is proof that the input is a world: at
// best the revision field and the length are mutually consistent.
struct WorldProbe {
    std::size_t inputBytes = 0;
    ProbeStatus status = ProbeStatus::revisionUnavailable;

    // Raw header values, present whenever four bytes were available.
    std::optional<std::int16_t> headerMajor;
    std::optional<std::int16_t> headerMinor;
    bool laterThanKnownRevisions = false;

    FormatRevision candidateRevision = FormatRevision::unknown;
    const RevisionDescriptor *descriptor = nullptr;
    std::optional<std::size_t> expectedBytes;

    std::size_t sectionCount = 0;
    std::array<ProbedSection, 4> sectionStorage{};
    std::size_t completeSectionCount = 0;

    // First byte the candidate layout needs but the input lacks.
    std::optional<std::size_t> firstMissingOffset;
    std::size_t missingBytes = 0;
    std::size_t surplusBytes = 0;

    // Every section of the candidate layout is present.
    bool structuredInspectionSafe = false;
    // Some, but not all, sections are complete and may be inspected.
    bool partialInspectionAvailable = false;

    [[nodiscard]] std::span<const ProbedSection> sections() const {
        return std::span<const ProbedSection>(sectionStorage).first(sectionCount);
    }
    [[nodiscard]] const ProbedSection *section(SectionKind kind) const;
};

// Never reads outside `bytes`, never throws and never modifies the input.
[[nodiscard]] WorldProbe probeWorld(std::span<const std::byte> bytes) noexcept;

// One-sentence justification of structuredInspectionSafe for the status.
[[nodiscard]] std::string_view probeStatusReason(ProbeStatus status) noexcept;

} // namespace eggbert
