// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <expected>
#include <span>
#include <string>
#include <vector>

#include "world/offset_map.hpp"

namespace eggbert {

// Three-way merge of world files of one revision and size: what two people
// changed, starting from the same base, combined into one world.
//
// The file is merged element by element, where an element is one header
// value, one cell of a layer, one whole object record, or one byte anywhere
// else (bytes after the world, for example). An element changed on one side
// only takes that side's bytes; changed to the same bytes on both sides it
// takes them; changed differently on both sides it is a conflict. An object
// record is merged as a whole because its fields describe one object.

struct MergeConflict {
    ByteRange range;
    OffsetLocation location;
};

enum class MergePreference {
    // Conflicting elements keep the base's bytes.
    none,
    mine,
    theirs,
};

struct MergeResult {
    std::vector<std::byte> bytes;
    // Elements taken from each side because only that side changed them.
    std::size_t fromMine = 0;
    std::size_t fromTheirs = 0;
    // Elements both sides changed to the same bytes.
    std::size_t agreed = 0;
    // Elements both sides changed differently, in file order; resolved by
    // the preference in `bytes`.
    std::vector<MergeConflict> conflicts;
};

// Refused, with the reason, when the three files are not complete worlds of
// the same revision and the same length.
[[nodiscard]] std::expected<MergeResult, std::string>
mergeWorlds(std::span<const std::byte> base, std::span<const std::byte> mine,
            std::span<const std::byte> theirs, MergePreference preference);

// The merged world with every conflict decided one by one: `takeTheirs[i]`
// chooses theirs for `result.conflicts[i]`, otherwise mine. Conflicts beyond
// the end of `takeTheirs` take mine.
[[nodiscard]] std::vector<std::byte> resolveMerge(const MergeResult &result,
                                                  std::span<const std::byte> mine,
                                                  std::span<const std::byte> theirs,
                                                  const std::vector<bool> &takeTheirs);

} // namespace eggbert
