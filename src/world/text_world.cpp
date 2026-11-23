// SPDX-License-Identifier: GPL-3.0-only
#include "world/text_world.hpp"

#include <charconv>
#include <map>

namespace eggbert {

namespace {

constexpr std::string_view headerSection = "DescFile";
constexpr std::string_view decorSection = "Decor";
constexpr std::string_view bigDecorSection = "BigDecor";
constexpr std::string_view objectSection = "MoveObject";

bool startsSection(std::string_view line, std::string_view section) {
    return line.size() > section.size() && line.starts_with(section) &&
           line[section.size()] == ':';
}

std::optional<long long> number(std::string_view text) {
    long long value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (text.empty() || parsed.ec != std::errc() || parsed.ptr != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

// Where the value of `name=` lies in a line: [begin, end) with end at the
// terminating space. As the game does, the first occurrence of `name=` is
// used.
struct Token {
    std::size_t begin = 0;
    std::size_t end = 0;
};

std::optional<Token> fieldToken(std::string_view line, std::string_view name) {
    std::string key(name);
    key += '=';
    const auto at = line.find(key);
    if (at == std::string_view::npos) {
        return std::nullopt;
    }
    const auto begin = at + key.size();
    const auto end = line.find(' ', begin);
    if (end == std::string_view::npos) {
        return std::nullopt;
    }
    return Token{begin, end};
}

std::optional<long long> fieldValue(std::string_view line, std::string_view name) {
    const auto token = fieldToken(line, name);
    return token ? number(line.substr(token->begin, token->end - token->begin)) : std::nullopt;
}

std::optional<Point> fieldPoint(std::string_view line, std::string_view name) {
    const auto token = fieldToken(line, name);
    if (!token) {
        return std::nullopt;
    }
    const auto text = line.substr(token->begin, token->end - token->begin);
    const auto separator = text.find(';');
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }
    const auto x = number(text.substr(0, separator));
    const auto y = number(text.substr(separator + 1));
    if (!x || !y || *x < INT32_MIN || *x > INT32_MAX || *y < INT32_MIN || *y > INT32_MAX) {
        return std::nullopt;
    }
    return Point{static_cast<std::int32_t>(*x), static_cast<std::int32_t>(*y)};
}

bool replaceField(std::string &line, std::string_view name, const std::string &text) {
    const auto token = fieldToken(line, name);
    if (!token) {
        return false;
    }
    line.replace(token->begin, token->end - token->begin, text);
    return true;
}

// The [begin, end) of comma-separated entry `index` of a line.
std::optional<Token> entryToken(std::string_view line, std::size_t index) {
    std::size_t begin = 0;
    for (std::size_t entry = 0;; ++entry) {
        const auto comma = line.find(',', begin);
        const auto end = comma == std::string_view::npos ? line.size() : comma;
        if (entry == index) {
            return Token{begin, end};
        }
        if (comma == std::string_view::npos) {
            return std::nullopt;
        }
        begin = comma + 1;
    }
}

std::size_t entryCount(std::string_view line) {
    std::size_t count = 1;
    for (const char character : line) {
        count += character == ',' ? 1 : 0;
    }
    return count;
}

} // namespace

bool looksLikeTextWorld(std::string_view text) {
    return startsSection(text.substr(0, text.find('\n')), headerSection);
}

TextWorld TextWorld::parse(std::string_view text) {
    TextWorld world;
    std::size_t begin = 0;
    while (true) {
        const auto end = text.find('\n', begin);
        world.lines_.emplace_back(text.substr(begin, end == std::string_view::npos
                                                         ? std::string_view::npos
                                                         : end - begin));
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    for (std::size_t index = 0; index < world.lines_.size(); ++index) {
        if (startsSection(world.lines_[index], objectSection)) {
            world.objectLines_.push_back(index);
        }
    }
    return world;
}

std::string TextWorld::write() const {
    std::string text;
    for (std::size_t index = 0; index < lines_.size(); ++index) {
        if (index != 0) {
            text += '\n';
        }
        text += lines_[index];
    }
    return text;
}

std::optional<std::size_t> TextWorld::sectionLine(std::string_view section) const {
    for (std::size_t index = 0; index < lines_.size(); ++index) {
        if (startsSection(lines_[index], section)) {
            return index;
        }
    }
    return std::nullopt;
}

std::optional<std::size_t> TextWorld::blockLine(SectionKind layer, std::size_t x) const {
    const auto start =
        sectionLine(layer == SectionKind::bigDecor ? bigDecorSection : decorSection);
    if (!start || x >= layout::mapWidth || *start + 1 + x >= lines_.size()) {
        return std::nullopt;
    }
    return *start + 1 + x;
}

std::optional<long long> TextWorld::headerValue(std::string_view name) const {
    const auto line = sectionLine(headerSection);
    return line ? fieldValue(lines_[*line], name) : std::nullopt;
}

std::optional<Point> TextWorld::headerPoint(std::string_view name) const {
    const auto line = sectionLine(headerSection);
    return line ? fieldPoint(lines_[*line], name) : std::nullopt;
}

bool TextWorld::setHeaderValue(std::string_view name, long long value) {
    const auto line = sectionLine(headerSection);
    return line && replaceField(lines_[*line], name, std::to_string(value));
}

bool TextWorld::setHeaderPoint(std::string_view name, Point value) {
    const auto line = sectionLine(headerSection);
    return line && replaceField(lines_[*line], name,
                                std::to_string(value.x) + ";" + std::to_string(value.y));
}

std::optional<long long> TextWorld::tile(SectionKind layer, std::size_t x, std::size_t y) const {
    const auto line = blockLine(layer, x);
    if (!line) {
        return std::nullopt;
    }
    const auto token = entryToken(lines_[*line], y);
    if (!token) {
        return std::nullopt;
    }
    if (token->begin == token->end) {
        return -1;
    }
    return number(std::string_view(lines_[*line]).substr(token->begin, token->end - token->begin));
}

bool TextWorld::setTile(SectionKind layer, std::size_t x, std::size_t y, long long value) {
    const auto line = blockLine(layer, x);
    if (!line) {
        return false;
    }
    const auto token = entryToken(lines_[*line], y);
    if (!token) {
        return false;
    }
    lines_[*line].replace(token->begin, token->end - token->begin,
                          value == -1 ? std::string() : std::to_string(value));
    return true;
}

std::optional<long long> TextWorld::objectValue(std::size_t index, std::string_view name) const {
    return index < objectLines_.size() ? fieldValue(lines_[objectLines_[index]], name)
                                       : std::nullopt;
}

std::optional<Point> TextWorld::objectPoint(std::size_t index, std::string_view name) const {
    return index < objectLines_.size() ? fieldPoint(lines_[objectLines_[index]], name)
                                       : std::nullopt;
}

bool TextWorld::setObjectValue(std::size_t index, std::string_view name, long long value) {
    return index < objectLines_.size() &&
           replaceField(lines_[objectLines_[index]], name, std::to_string(value));
}

bool TextWorld::setObjectPoint(std::size_t index, std::string_view name, Point value) {
    return index < objectLines_.size() &&
           replaceField(lines_[objectLines_[index]], name,
                        std::to_string(value.x) + ";" + std::to_string(value.y));
}

bool TextWorld::removeObject(std::size_t index) {
    if (index >= objectLines_.size()) {
        return false;
    }
    const auto line = objectLines_[index];
    const bool last = line + 1 == lines_.size();
    lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(line));
    // The line before a removed last line becomes the end of the file: it
    // loses the carriage return that belonged to its line break.
    if (last && !lines_.empty() && !lines_.back().empty() && lines_.back().back() == '\r') {
        lines_.back().pop_back();
    }
    *this = parse(write());
    return true;
}

bool TextWorld::duplicateObject(std::size_t index) {
    if (index >= objectLines_.size() || objectLines_.size() >= textWorldMostObjects) {
        return false;
    }
    const auto line = objectLines_[index];
    // The last line of a file without a final line break ends in nothing;
    // its copy keeps the line ending the other lines use.
    auto copy = lines_[line];
    if (line + 1 == lines_.size()) {
        if (!lines_.front().empty() && lines_.front().back() == '\r' && lines_.size() > 1) {
            lines_[line] += '\r';
        }
    }
    lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(line) + 1, std::move(copy));
    *this = parse(write());
    return true;
}

bool TextWorld::addObject(long long type, Point position, long long channel, long long icon) {
    ObjectLine fields;
    fields.type = type;
    fields.stepAdvance = 1;
    fields.stepRecede = 1;
    fields.posStart = position;
    fields.posEnd = position;
    fields.posCurrent = position;
    fields.step = 1;
    fields.channel = channel;
    fields.icon = icon;
    return addObject(fields);
}

bool TextWorld::addObject(const ObjectLine &fields) {
    if (fields.type == 0 || objectLines_.size() >= textWorldMostObjects || lines_.empty()) {
        return false;
    }
    const bool carriageReturns =
        lines_.size() > 1 && !lines_.front().empty() && lines_.front().back() == '\r';
    const auto point = [](Point value) {
        return std::to_string(value.x) + ";" + std::to_string(value.y);
    };
    std::string text = std::string(objectSection) + ": type=" + std::to_string(fields.type) +
                       " stepAdvance=" + std::to_string(fields.stepAdvance) +
                       " stepRecede=" + std::to_string(fields.stepRecede) +
                       " timeStopStart=" + std::to_string(fields.timeStopStart) +
                       " timeStopEnd=" + std::to_string(fields.timeStopEnd) +
                       " posStart=" + point(fields.posStart) + " posEnd=" + point(fields.posEnd) +
                       " posCurrent=" + point(fields.posCurrent) +
                       " step=" + std::to_string(fields.step) +
                       " time=" + std::to_string(fields.time) +
                       " phase=" + std::to_string(fields.phase) +
                       " channel=" + std::to_string(fields.channel) +
                       " icon=" + std::to_string(fields.icon) + " ";
    if (!objectLines_.empty() && objectLines_.back() + 1 < lines_.size()) {
        // After the last object line, which ends with a line break.
        if (carriageReturns) {
            text += '\r';
        }
        lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(objectLines_.back()) + 1,
                      std::move(text));
    } else if (lines_.back().empty()) {
        // The file ends with a line break: the new line goes before the
        // empty last element and ends with a break of its own.
        if (carriageReturns) {
            text += '\r';
        }
        lines_.insert(lines_.end() - 1, std::move(text));
    } else {
        // No final line break: end the last line first.
        if (carriageReturns) {
            lines_.back() += '\r';
        }
        lines_.push_back(std::move(text));
    }
    *this = parse(write());
    return true;
}

std::vector<std::string> TextWorld::problems() const {
    std::vector<std::string> found;
    const auto at = [](std::size_t line) { return "Line " + std::to_string(line + 1) + ": "; };
    const auto header = sectionLine(headerSection);
    if (!header) {
        found.push_back("There is no DescFile line.");
    } else {
        for (const auto name : {"dimDecor", "posDecor", "blupiPos"}) {
            if (!headerPoint(name)) {
                found.push_back(at(*header) + "the field " + std::string(name) +
                                " is missing or is not two numbers followed by a space.");
            }
        }
        for (const auto name : {"music", "region", "blupiDir"}) {
            if (!headerValue(name)) {
                found.push_back(at(*header) + "the field " + std::string(name) +
                                " is missing or is not a number followed by a space.");
            }
        }
    }
    for (const auto layer : {SectionKind::foreground, SectionKind::bigDecor}) {
        const auto name = layer == SectionKind::bigDecor ? bigDecorSection : decorSection;
        const auto start = sectionLine(name);
        if (!start) {
            found.push_back("There is no " + std::string(name) + " block.");
            continue;
        }
        for (std::size_t x = 0; x < layout::mapWidth; ++x) {
            const auto line = blockLine(layer, x);
            if (!line) {
                found.push_back("The " + std::string(name) + " block ends after " +
                                std::to_string(x) + " of 100 lines.");
                break;
            }
            const auto &text = lines_[*line];
            if (startsSection(text, bigDecorSection) || startsSection(text, objectSection) ||
                startsSection(text, headerSection) || startsSection(text, decorSection)) {
                found.push_back(at(*line) + "the " + std::string(name) + " block has only " +
                                std::to_string(x) + " of 100 lines before the next section.");
                break;
            }
            if (entryCount(text) < layout::mapHeight) {
                found.push_back(at(*line) + "the line has " + std::to_string(entryCount(text)) +
                                " entries; a column has 100 cells.");
                continue;
            }
            for (std::size_t y = 0; y < layout::mapHeight; ++y) {
                if (!tile(layer, x, y)) {
                    found.push_back(at(*line) + "entry " + std::to_string(y + 1) +
                                    " is not a number.");
                    break;
                }
            }
        }
    }
    for (const auto line : objectLines_) {
        for (const auto name : {"type", "channel", "icon"}) {
            if (!fieldValue(lines_[line], name)) {
                found.push_back(at(line) + "the object field " + std::string(name) +
                                " is missing or is not a number followed by a space.");
            }
        }
        for (const auto name : {"posStart", "posEnd", "posCurrent"}) {
            if (!fieldPoint(lines_[line], name)) {
                found.push_back(at(line) + "the object field " + std::string(name) +
                                " is missing or is not two numbers followed by a space.");
            }
        }
    }
    // The game reads object lines in order and stops at the first whose
    // type is 0 or unreadable, and after its pool is full.
    for (std::size_t index = 0; index + 1 < objectLines_.size(); ++index) {
        if (fieldValue(lines_[objectLines_[index]], "type").value_or(0) == 0) {
            found.push_back(at(objectLines_[index]) +
                            "the object's type is 0 or not readable; the game stops reading "
                            "objects here and ignores the " +
                            std::to_string(objectLines_.size() - index - 1) +
                            " object line(s) after it.");
            break;
        }
    }
    if (objectLines_.size() > textWorldMostObjects) {
        found.push_back("There are " + std::to_string(objectLines_.size()) +
                        " object lines; the game reads the first 200.");
    }
    return found;
}

std::string TextWorld::describe() const {
    std::string text = "Mobile Eggbert text world\n";
    text += "  lines: " + std::to_string(lines_.size()) + "\n";
    const auto point = [&](std::string_view name) {
        const auto value = headerPoint(name);
        return std::string(name) + " " +
               (value ? std::to_string(value->x) + ", " + std::to_string(value->y)
                      : std::string("not readable"));
    };
    const auto value = [&](std::string_view name) {
        const auto read = headerValue(name);
        return std::string(name) + " " + (read ? std::to_string(*read) : std::string("not readable"));
    };
    text += "  " + point("dimDecor") + "; " + point("posDecor") + "\n";
    text += "  " + value("music") + "; " + value("region") + "; " + point("blupiPos") + "; " +
            value("blupiDir") + "\n";
    for (const auto layer : {SectionKind::foreground, SectionKind::bigDecor}) {
        std::size_t used = 0;
        for (std::size_t x = 0; x < layout::mapWidth; ++x) {
            for (std::size_t y = 0; y < layout::mapHeight; ++y) {
                const auto stored = tile(layer, x, y);
                used += stored && *stored != -1 ? 1 : 0;
            }
        }
        text += std::string("  ") + (layer == SectionKind::bigDecor ? "BigDecor" : "Decor") +
                ": " + std::to_string(used) + " occupied cell(s)\n";
    }
    std::map<long long, std::size_t> types;
    for (std::size_t index = 0; index < objectLines_.size(); ++index) {
        if (const auto type = objectValue(index, "type")) {
            ++types[*type];
        }
    }
    text += "  objects: " + std::to_string(objectLines_.size());
    for (const auto &[type, count] : types) {
        text += ", type " + std::to_string(type) + " x " + std::to_string(count);
    }
    text += "\n";
    const auto found = problems();
    text += found.empty() ? "  The file has the expected shape.\n"
                          : "  " + std::to_string(found.size()) + " problem(s):\n";
    for (const auto &problem : found) {
        text += "    " + problem + "\n";
    }
    text += "  This kind of world is described only; it is never converted to or from BLP.\n";
    return text;
}

} // namespace eggbert
