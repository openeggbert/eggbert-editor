// SPDX-License-Identifier: GPL-3.0-only
#include "world/world_probe.hpp"

#include <algorithm>

#include "world/le_reader.hpp"

namespace eggbert {

const ProbedSection *WorldProbe::section(SectionKind kind) const {
    for (const auto &candidate : sections()) {
        if (candidate.kind == kind) {
            return &candidate;
        }
    }
    return nullptr;
}

WorldProbe probeWorld(std::span<const std::byte> bytes) noexcept {
    WorldProbe probe;
    probe.inputBytes = bytes.size();

    const auto major = readI16(bytes, 0);
    const auto minor = readI16(bytes, 2);
    if (!major || !minor) {
        probe.status = ProbeStatus::revisionUnavailable;
        probe.firstMissingOffset =
            major ? minor.error().firstUnavailableOffset : major.error().firstUnavailableOffset;
        probe.missingBytes = layout::revisionFieldBytes - bytes.size();
        return probe;
    }
    probe.headerMajor = *major;
    probe.headerMinor = *minor;
    probe.laterThanKnownRevisions = isLaterThanKnownRevisions(*major, *minor);
    probe.candidateRevision = revisionFromHeader(*major, *minor);
    probe.descriptor = findRevision(probe.candidateRevision);
    if (probe.descriptor == nullptr) {
        probe.status = ProbeStatus::unknownRevision;
        return probe;
    }

    probe.expectedBytes = probe.descriptor->expectedBytes;
    for (const auto &section : probe.descriptor->sections()) {
        ProbedSection probed{section.kind, section.offset, section.size, 0};
        if (bytes.size() > section.offset) {
            probed.availableBytes = std::min(section.size, bytes.size() - section.offset);
        }
        if (probed.complete()) {
            ++probe.completeSectionCount;
        }
        probe.sectionStorage[probe.sectionCount++] = probed;
    }

    if (bytes.size() < probe.descriptor->expectedBytes) {
        probe.status = ProbeStatus::truncated;
        probe.firstMissingOffset = bytes.size();
        probe.missingBytes = probe.descriptor->expectedBytes - bytes.size();
        probe.partialInspectionAvailable = probe.completeSectionCount > 0;
        return probe;
    }
    probe.surplusBytes = bytes.size() - probe.descriptor->expectedBytes;
    probe.status = probe.surplusBytes == 0 ? ProbeStatus::sizeMatches : ProbeStatus::surplus;
    probe.structuredInspectionSafe = true;
    return probe;
}

std::string_view probeStatusReason(ProbeStatus status) noexcept {
    switch (status) {
    case ProbeStatus::revisionUnavailable:
        return "The input ends inside the four-byte revision field, so no layout can be "
               "selected.";
    case ProbeStatus::unknownRevision:
        return "The revision field is not 1.0, 1.1, 1.2 or 1.3, so the layout is unverified "
               "and the input may not be a world at all.";
    case ProbeStatus::truncated:
        return "The revision field is recognized, but the input ends before the layout it "
               "selects; only complete sections can be inspected.";
    case ProbeStatus::sizeMatches:
        return "The revision field is recognized and the length equals the layout it selects. "
               "The format has no signature, so this is consistency, not proof.";
    case ProbeStatus::surplus:
        return "The revision field is recognized and the layout it selects fits; the bytes "
               "after it are retained as an opaque suffix. The format has no signature, so "
               "this is consistency, not proof.";
    }
    return {};
}

} // namespace eggbert
