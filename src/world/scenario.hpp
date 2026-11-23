// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "world/world_view.hpp"

namespace eggbert {

// A regression scenario for a world: numbers that should stay true while
// the world is edited — how many treasures it has, that World Doctor finds
// no error, where the first start is, and so on. A scenario is a text file
// of expectations, one per line:
//
//     # comment
//     objects.type.5 = 12
//     doctor.errors = 0
//     foreground.occupied >= 400
//
// with the comparisons =, >= and <=. `recordScenario` writes every fact of
// a world as an `=` line; lines can then be deleted or loosened by hand.

struct WorldFact {
    std::string name;
    long long value = 0;

    friend bool operator==(const WorldFact &, const WorldFact &) = default;
};

// Every fact of a world under a game profile, sorted by name: revision,
// header values, occupied cells per layer, objects in total and per stored
// type, World Doctor findings in total per severity and per rule.
[[nodiscard]] std::vector<WorldFact> worldFacts(const WorldView &world, GameProfile profile);

[[nodiscard]] std::string recordScenario(const std::vector<WorldFact> &facts);

// An expectation that does not hold.
struct ScenarioFailure {
    std::size_t line = 0;
    std::string name;
    long long actual = 0;
    // "=", ">=" or "<=".
    std::string comparison;
    long long wanted = 0;
};

struct ScenarioResult {
    // Expectations read.
    std::size_t checked = 0;
    // One sentence per expectation that does not hold, with its line.
    std::vector<std::string> failures;
    // The same failures as numbers, for interfaces in other languages.
    std::vector<ScenarioFailure> details;
};

// The parse error names its line. A fact a scenario names but the world
// does not have (for example a rule that reports nothing) counts as 0.
[[nodiscard]] std::expected<ScenarioResult, std::string>
checkScenario(std::string_view scenario, const std::vector<WorldFact> &facts);

} // namespace eggbert
