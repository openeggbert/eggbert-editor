// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "world/layout.hpp"

namespace eggbert {

// Rewriting a complete world as another header revision (1.0 to 1.3). The
// result is always a new byte image; the source is only read.
//
// What the games do with an older file is the model (docs/research/
// evidence.md, "Older revisions"): a 1.0 file has no bigDecor layer and the
// game starts with an empty one; 1.0 and 1.1 files store 100 object slots
// and the game leaves the other 100 free. Going up therefore adds exactly
// that; going down is possible only when nothing would be dropped, unless
// the caller explicitly accepts the loss.

enum class ConversionError {
    // The source is not a complete world of a known revision.
    sourceNotConvertible,
    // The target is the revision the source already has, or unknown.
    invalidTarget,
    // The target cannot store something the source has; `detail` says what.
    wouldLoseData,
};

struct ConversionFailure {
    ConversionError error = ConversionError::sourceNotConvertible;
    std::string detail;
};

struct RevisionConversion {
    std::vector<std::byte> bytes;
    // What was added, dropped or carries another meaning in the target.
    std::vector<std::string> notes;
    // True when something stored in the source is not in the result.
    bool lossy = false;
};

[[nodiscard]] std::expected<RevisionConversion, ConversionFailure>
convertRevision(std::span<const std::byte> source, FormatRevision target, bool allowLoss = false);

} // namespace eggbert
