// SPDX-License-Identifier: GPL-3.0-only
#include "world/scenario.hpp"

#include <algorithm>
#include <charconv>
#include <map>

#include "world/doctor.hpp"

namespace eggbert {

namespace {

std::string_view trim(std::string_view text) {
    while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) {
        text.remove_prefix(1);
    }
    while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r')) {
        text.remove_suffix(1);
    }
    return text;
}

} // namespace

std::vector<WorldFact> worldFacts(const WorldView &world, GameProfile profile) {
    std::map<std::string, long long> facts;
    const auto &probe = world.probe();
    facts["revision.minor"] = probe.descriptor != nullptr ? probe.descriptor->minor : -1;
    facts["file.bytes"] = static_cast<long long>(world.bytes().size());
    if (const auto header = world.header()) {
        facts["header.music"] = header->music();
        facts["header.region"] = header->region();
        facts["header.scrolls.x"] = header->dimDecor().x != 0 ? 1 : 0;
        facts["header.scrolls.y"] = header->dimDecor().y != 0 ? 1 : 0;
        for (std::size_t index = 0; index < 4; ++index) {
            const auto position = *header->blupiPos(index);
            const auto prefix = "start." + std::to_string(index + 1);
            facts[prefix + ".x"] = position.x;
            facts[prefix + ".y"] = position.y;
        }
    }
    const auto occupied = [&](const std::optional<TileLayerView> &layer, const std::string &name) {
        if (!layer) {
            return;
        }
        long long count = 0;
        for (std::size_t x = 0; x < layout::mapWidth; ++x) {
            for (std::size_t y = 0; y < layout::mapHeight; ++y) {
                count += *layer->cell(x, y) != layout::emptyCell ? 1 : 0;
            }
        }
        facts[name + ".occupied"] = count;
    };
    occupied(world.foreground(), "foreground");
    occupied(world.bigDecor(), "bigdecor");
    if (const auto objects = world.objects()) {
        facts["objects.active"] = static_cast<long long>(objects->activeCount());
        facts["objects.free"] = static_cast<long long>(objects->freeCount());
        for (std::size_t slot = 0; slot < objects->slotCount(); ++slot) {
            const auto record = *objects->record(slot);
            if (!record.isFree()) {
                ++facts["objects.type." + std::to_string(record.type)];
            }
        }
    }
    const auto report = runDoctor(world, profile);
    facts["doctor.errors"] = static_cast<long long>(report.count(Severity::error));
    facts["doctor.warnings"] = static_cast<long long>(report.count(Severity::warning));
    facts["doctor.notes"] = static_cast<long long>(report.count(Severity::note));
    for (const auto &finding : report.findings) {
        ++facts["doctor.rule." + std::string(finding.ruleId)];
    }
    std::vector<WorldFact> result;
    for (const auto &[name, value] : facts) {
        result.push_back({name, value});
    }
    return result;
}

std::string recordScenario(const std::vector<WorldFact> &facts) {
    std::string text = "# Eggbert Editor world scenario: delete or loosen (>=, <=) lines as needed.\n";
    for (const auto &fact : facts) {
        text += fact.name + " = " + std::to_string(fact.value) + "\n";
    }
    return text;
}

std::expected<ScenarioResult, std::string>
checkScenario(std::string_view scenario, const std::vector<WorldFact> &facts) {
    ScenarioResult result;
    std::size_t lineNumber = 0;
    while (!scenario.empty()) {
        ++lineNumber;
        const auto end = scenario.find('\n');
        const auto line = trim(scenario.substr(0, end));
        scenario.remove_prefix(end == std::string_view::npos ? scenario.size() : end + 1);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const auto fail = [&](std::string_view why) {
            return std::unexpected("Line " + std::to_string(lineNumber) + ": " + std::string(why));
        };
        // name, comparison, number
        std::string_view comparison;
        std::size_t at = std::string_view::npos;
        for (const std::string_view candidate : {">=", "<=", "="}) {
            at = line.find(candidate);
            if (at != std::string_view::npos) {
                comparison = candidate;
                break;
            }
        }
        if (at == std::string_view::npos) {
            return fail("expected a name, one of =, >= or <=, and a number.");
        }
        const auto name = trim(line.substr(0, at));
        const auto number = trim(line.substr(at + comparison.size()));
        long long wanted = 0;
        const auto parsed = std::from_chars(number.data(), number.data() + number.size(), wanted);
        if (name.empty() || name.find(' ') != std::string_view::npos || number.empty() ||
            parsed.ec != std::errc() || parsed.ptr != number.data() + number.size()) {
            return fail("expected a name, one of =, >= or <=, and a number.");
        }
        long long actual = 0;
        if (const auto found = std::ranges::find(facts, name, &WorldFact::name);
            found != facts.end()) {
            actual = found->value;
        }
        const bool holds = comparison == "=" ? actual == wanted
                           : comparison == ">=" ? actual >= wanted
                                                : actual <= wanted;
        ++result.checked;
        if (!holds) {
            result.details.push_back(
                {lineNumber, std::string(name), actual, std::string(comparison), wanted});
            result.failures.push_back("Line " + std::to_string(lineNumber) + ": " +
                                      std::string(name) + " is " + std::to_string(actual) +
                                      ", expected " + std::string(comparison) + " " +
                                      std::to_string(wanted) + ".");
        }
    }
    return result;
}

} // namespace eggbert
