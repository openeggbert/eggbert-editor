// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "world/world_document.hpp"

namespace eggbert {

enum class Severity { note, warning, error };

// How strong the basis of a finding is.
enum class Confidence {
    // Follows from the byte layout itself.
    structural,
    // A limit or behavior read in the reconstructed source.
    provenLimit,
    // Outside a range the chosen game is known to offer.
    verifiedRange,
    // Reported by other notes, not re-read here.
    reported,
    // An estimate, clearly not a proven limit.
    estimate,
};

// A proposed repair: exact byte replacements, applied only on request as
// one named undo step.
struct DoctorFix {
    std::string label;
    std::vector<ByteChange> changes;
};

struct Finding {
    // Stable identifier such as "HDR-MUSIC-RANGE".
    std::string_view ruleId;
    Severity severity = Severity::note;
    Confidence confidence = Confidence::structural;
    // Profile the finding was judged under (unknown for profile-free rules).
    GameProfile profile = GameProfile::unknown;
    std::string message;
    // Where the rule comes from.
    std::string_view evidence;
    // Exact bytes concerned. For truncation: the bytes that are missing.
    ByteRange range;
    // Semantic location, when the finding is about one of these.
    std::optional<HeaderField> headerField;
    std::size_t elementIndex = 0;
    std::optional<SectionKind> layer;
    // All cells concerned (tile rules group cells by stored value).
    std::vector<CellPos> cells;
    std::optional<std::size_t> slot;
    std::optional<ObjectField> objectField;
    std::optional<DoctorFix> fix;
};

struct DoctorReport {
    std::vector<Finding> findings;

    [[nodiscard]] std::size_t count(Severity severity) const;
    [[nodiscard]] std::size_t countRule(std::string_view ruleId) const;
    [[nodiscard]] const Finding *first(std::string_view ruleId) const;
};

// Complete, deterministic audit. Never modifies anything; with an unknown
// profile the game-specific rules are skipped.
[[nodiscard]] DoctorReport runDoctor(const WorldView &world, GameProfile profile);

struct RuleInfo {
    std::string_view id;
    Severity severity;
    std::string_view summary;
};
// Every rule the doctor can report, for documentation and filtering.
[[nodiscard]] std::span<const RuleInfo> doctorRules();

[[nodiscard]] std::string_view severityName(Severity severity);
[[nodiscard]] std::string_view confidenceName(Confidence confidence);

} // namespace eggbert
