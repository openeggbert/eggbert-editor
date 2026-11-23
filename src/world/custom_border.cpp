// SPDX-License-Identifier: GPL-3.0-only
#include "world/custom_border.hpp"

#include <algorithm>
#include <charconv>
#include <map>

namespace eggbert {

namespace {

std::string_view trim(std::string_view text) {
    const auto blank = [](char character) {
        return character == ' ' || character == '\t' || character == '\r';
    };
    while (!text.empty() && blank(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && blank(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

std::unexpected<std::string> failAt(std::size_t line, std::string_view message) {
    return std::unexpected("Line " + std::to_string(line) + ": " + std::string(message));
}

bool inMap(long long x, long long y) {
    return x >= 0 && y >= 0 && x < static_cast<long long>(layout::mapWidth) &&
           y < static_cast<long long>(layout::mapHeight);
}

} // namespace

bool CustomBorderRule::contains(std::int16_t value) const {
    return std::ranges::find(forms, value) != forms.end();
}

std::expected<std::vector<CustomBorderRule>, std::string>
parseCustomBorderRules(std::string_view text) {
    std::vector<CustomBorderRule> rules;
    bool haveForms = true; // no section is open yet
    std::size_t lineNumber = 0;
    std::size_t sectionLine = 0;
    while (!text.empty()) {
        ++lineNumber;
        const auto end = text.find('\n');
        const auto line = trim(text.substr(0, end));
        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        if (line.front() == '[') {
            if (!haveForms) {
                return failAt(sectionLine, "this rule has no \"forms = ...\" line.");
            }
            if (line.back() != ']' || trim(line.substr(1, line.size() - 2)).empty()) {
                return failAt(lineNumber, "a rule starts with its name in brackets, like [Wall].");
            }
            rules.push_back({std::string(trim(line.substr(1, line.size() - 2))), {}});
            haveForms = false;
            sectionLine = lineNumber;
            continue;
        }
        const auto equals = line.find('=');
        if (equals == std::string_view::npos || trim(line.substr(0, equals)) != "forms") {
            return failAt(lineNumber, "expected \"forms = \" followed by sixteen tile numbers.");
        }
        if (rules.empty()) {
            return failAt(lineNumber, "forms appear before any [name] line.");
        }
        if (haveForms) {
            return failAt(lineNumber, "this rule already has its forms.");
        }
        auto rest = trim(line.substr(equals + 1));
        std::size_t count = 0;
        while (!rest.empty()) {
            const auto space = rest.find_first_of(" \t");
            const auto word = rest.substr(0, space);
            int value = 0;
            const auto parsed = std::from_chars(word.data(), word.data() + word.size(), value);
            if (parsed.ec != std::errc() || parsed.ptr != word.data() + word.size() ||
                value < 0 || value > 32'767) {
                return failAt(lineNumber, "\"" + std::string(word) +
                                              "\" is not a tile number from 0 to 32767.");
            }
            if (count < 16) {
                rules.back().forms[count] = static_cast<std::int16_t>(value);
            }
            ++count;
            rest = space == std::string_view::npos ? std::string_view() : trim(rest.substr(space));
        }
        if (count != 16) {
            return failAt(lineNumber, "a rule needs exactly sixteen forms, this line has " +
                                          std::to_string(count) + ".");
        }
        haveForms = true;
    }
    if (!haveForms) {
        return failAt(sectionLine, "this rule has no \"forms = ...\" line.");
    }
    if (rules.empty()) {
        return std::unexpected(std::string("The file contains no rule."));
    }
    return rules;
}

AdaptiveResult customBorderStroke(EditBuilder &builder, const CustomBorderRule &rule,
                                  std::span<const CellPos> footprint, bool erase) {
    AdaptiveResult result;
    const auto before = builder.view().foreground();
    if (!before) {
        return result;
    }
    // Values as stored before the stroke, for the cells it may touch.
    std::map<std::pair<std::size_t, std::size_t>, std::int16_t> original;
    const auto remember = [&](long long x, long long y) {
        if (inMap(x, y)) {
            const auto cx = static_cast<std::size_t>(x);
            const auto cy = static_cast<std::size_t>(y);
            original.try_emplace({cx, cy}, *builder.view().foreground()->cell(cx, cy));
        }
    };
    const auto valueAt = [&](long long x, long long y) -> std::int16_t {
        return *builder.view().foreground()->cell(static_cast<std::size_t>(x),
                                                  static_cast<std::size_t>(y));
    };
    const auto joins = [&](long long x, long long y) {
        return !inMap(x, y) || rule.contains(valueAt(x, y));
    };
    const auto refresh = [&](long long x, long long y) {
        if (!inMap(x, y) || !rule.contains(valueAt(x, y))) {
            return;
        }
        unsigned mask = 0;
        mask |= joins(x, y + 1) ? joinBelow : 0u;
        mask |= joins(x, y - 1) ? joinAbove : 0u;
        mask |= joins(x + 1, y) ? joinRight : 0u;
        mask |= joins(x - 1, y) ? joinLeft : 0u;
        builder.setCell(SectionKind::foreground, static_cast<std::size_t>(x),
                        static_cast<std::size_t>(y), rule.forms[mask]);
    };
    for (const auto &cell : footprint) {
        if (!TileLayerView::contains(cell.x, cell.y)) {
            continue;
        }
        const auto x = static_cast<long long>(cell.x);
        const auto y = static_cast<long long>(cell.y);
        for (const auto &[dx, dy] : {std::pair{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
            remember(x + dx, y + dy);
        }
        builder.setCell(SectionKind::foreground, cell.x, cell.y,
                        erase ? layout::emptyCell : rule.forms[0]);
        for (const auto &[dx, dy] : {std::pair{0, 0}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
            refresh(x + dx, y + dy);
        }
    }
    const auto inFootprint = [&](std::size_t x, std::size_t y) {
        return std::ranges::any_of(
            footprint, [&](const CellPos &cell) { return cell.x == x && cell.y == y; });
    };
    for (const auto &[position, value] : original) {
        if (valueAt(static_cast<long long>(position.first),
                    static_cast<long long>(position.second)) == value) {
            continue;
        }
        const CellPos cell{position.first, position.second};
        (inFootprint(cell.x, cell.y) ? result.changedFootprint : result.sideEffects)
            .push_back(cell);
    }
    return result;
}

} // namespace eggbert
