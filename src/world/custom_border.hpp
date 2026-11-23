// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "world/adaptive.hpp"

namespace eggbert {

// A border rule written by the user, not taken from a game: sixteen tile
// numbers, one for each combination of joined neighbours, indexed by the
// same mask the games use (1 below, 2 above, 4 right, 8 left). A neighbour
// joins when it holds any of the sixteen numbers; cells beyond the edge of
// the 100 x 100 map count as joined. Nothing about it is verified against a
// game, which is why it belongs to experimental editing.
struct CustomBorderRule {
    std::string name;
    std::array<std::int16_t, 16> forms{};

    [[nodiscard]] bool contains(std::int16_t value) const;
};

// Text format, one rule per section:
//
//     # comment
//     [Stone wall]
//     forms = 153 147 148 146 40 151 150 144 39 152 149 145 38 36 37 35
//
// Numbers are 0..32767. The error names the line.
[[nodiscard]] std::expected<std::vector<CustomBorderRule>, std::string>
parseCustomBorderRules(std::string_view text);

// Paints the rule into each footprint cell (or empties the cell when
// `erase`), then gives that cell and its four neighbours the form matching
// their joined neighbours, when they hold a number of the rule. Only the
// foreground layer is touched.
AdaptiveResult customBorderStroke(EditBuilder &builder, const CustomBorderRule &rule,
                                  std::span<const CellPos> footprint, bool erase);

} // namespace eggbert
