// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "world/layout.hpp"

namespace eggbert {

// One rule of a game's per-tick picture choice for objects of a stored type.
struct IconRule {
    std::int16_t type = 0;
    // 0: in any state; otherwise the motion state the rule is for
    // (1 waiting at the start, 2 advancing, 3 waiting at the end,
    // 4 returning).
    std::int8_t step = 0;
    // 0: any path; 1: only when posStart.x > posEnd.x; 2: only otherwise.
    std::int8_t direction = 0;
    // The counter that selects the frame: the object's `time` (true) or its
    // `phase` (false).
    bool usesTime = false;
    std::int16_t divisor = 1;
    // icon = frames[(counter / divisor) % frames.size()]
    std::span<const std::int16_t> frames;
    // The channel the rule sets, or -1 to keep the stored one.
    std::int16_t channel = -1;
    std::string_view provenance;
};

// Rules of a profile in the games' source order; empty for unknown.
[[nodiscard]] std::span<const IconRule> objectIconRules(GameProfile profile);

} // namespace eggbert
