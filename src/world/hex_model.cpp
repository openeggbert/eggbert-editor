// SPDX-License-Identifier: GPL-3.0-only
#include "world/hex_model.hpp"

#include <algorithm>
#include <cctype>

#include "world/le_reader.hpp"

namespace eggbert {

namespace {

std::string number(std::size_t value) {
    return std::to_string(value);
}

const SectionDescriptor *sectionFor(const WorldProbe &probe, OffsetArea area) {
    if (probe.descriptor == nullptr) {
        return nullptr;
    }
    switch (area) {
    case OffsetArea::header:
        return probe.descriptor->section(SectionKind::header);
    case OffsetArea::foreground:
        return probe.descriptor->section(SectionKind::foreground);
    case OffsetArea::bigDecor:
        return probe.descriptor->section(SectionKind::bigDecor);
    case OffsetArea::objects:
        return probe.descriptor->section(SectionKind::objects);
    case OffsetArea::suffix:
    case OffsetArea::unmapped:
        break;
    }
    return nullptr;
}

// For POINT elements: which component an offset falls in.
const char *pointComponent(std::size_t byteInElement) {
    return byteInElement < 4 ? ".x" : ".y";
}

int hexValue(char character) {
    if (character >= '0' && character <= '9') {
        return character - '0';
    }
    if (character >= 'a' && character <= 'f') {
        return character - 'a' + 10;
    }
    if (character >= 'A' && character <= 'F') {
        return character - 'A' + 10;
    }
    return -1;
}

} // namespace

std::string offsetLabel(const WorldProbe &probe, std::size_t offset) {
    const auto location = locateOffset(probe, offset);
    switch (location.area) {
    case OffsetArea::header: {
        if (!location.headerField) {
            return "header";
        }
        const auto &field = headerField(*location.headerField);
        std::string label = "header." + std::string(field.name);
        if (field.elementCount > 1) {
            label += "[" + number(location.elementIndex) + "]";
        }
        if (field.type == WireType::point) {
            label += pointComponent(location.byteInElement);
        }
        return label;
    }
    case OffsetArea::foreground:
    case OffsetArea::bigDecor:
        return std::string(location.area == OffsetArea::foreground ? "foreground" : "bigDecor") +
               "[" + number(location.cell.x) + "][" + number(location.cell.y) + "]";
    case OffsetArea::objects: {
        std::string label = "objects[" + number(location.slot) + "]";
        if (location.objectField) {
            const auto &field = objectField(*location.objectField);
            label += "." + std::string(field.name);
            if (field.type == WireType::point) {
                label += pointComponent(location.byteInElement);
            }
        }
        return label;
    }
    case OffsetArea::suffix:
        return "suffix+" + number(location.areaOffset);
    case OffsetArea::unmapped:
        break;
    }
    return "unmapped";
}

std::string offsetCalculation(const WorldProbe &probe, std::size_t offset) {
    const auto location = locateOffset(probe, offset);
    const auto *section = sectionFor(probe, location.area);
    if (section == nullptr) {
        return {};
    }
    switch (location.area) {
    case OffsetArea::header: {
        if (!location.headerField) {
            return {};
        }
        const auto &field = headerField(*location.headerField);
        std::string text = "header field " + std::string(field.name) + " at " +
                           number(field.offset);
        if (field.elementCount > 1) {
            text += ", element " + number(location.elementIndex) + ": " + number(field.offset) +
                    " + " + number(field.elementBytes()) + " x " +
                    number(location.elementIndex) + " = " +
                    number(location.elementRange.offset);
        }
        return text;
    }
    case OffsetArea::foreground:
    case OffsetArea::bigDecor:
        return std::string(location.area == OffsetArea::foreground ? "foreground" : "bigDecor") +
               " cell (" + number(location.cell.x) + ", " + number(location.cell.y) +
               "), stored [x][y]: " + number(section->offset) + " + 2 x (100 x " +
               number(location.cell.x) + " + " + number(location.cell.y) + ") = " +
               number(location.elementRange.offset);
    case OffsetArea::objects: {
        std::string text = "object slot " + number(location.slot) + ": " +
                           number(section->offset) + " + 48 x " + number(location.slot) + " = " +
                           number(section->offset + 48 * location.slot);
        if (location.objectField) {
            const auto &field = objectField(*location.objectField);
            text += "; field " + std::string(field.name) + " at +" + number(field.offset) +
                    " = " + number(location.elementRange.offset);
        }
        return text;
    }
    case OffsetArea::suffix:
    case OffsetArea::unmapped:
        break;
    }
    return {};
}

std::optional<DecodedElement> decodeScalarAt(std::span<const std::byte> bytes,
                                             const WorldProbe &probe, std::size_t offset) {
    const auto location = locateOffset(probe, offset);
    if (location.elementRange.empty()) {
        return std::nullopt;
    }
    auto range = location.elementRange;
    if (range.size == 8) {
        // A POINT: decode the 32-bit component containing the offset.
        range = {range.offset + (location.byteInElement < 4 ? 0 : 4), 4};
    }
    DecodedElement element;
    element.range = range;
    element.width = range.size;
    if (range.size == 1) {
        if (!rangeFits(bytes.size(), range.offset, 1)) {
            return std::nullopt;
        }
        element.unsignedValue = std::to_integer<std::uint64_t>(bytes[range.offset]);
        element.signedValue = static_cast<std::int8_t>(element.unsignedValue);
        return element;
    }
    if (range.size == 2) {
        const auto value = readU16(bytes, range.offset);
        if (!value) {
            return std::nullopt;
        }
        element.unsignedValue = *value;
        element.signedValue = static_cast<std::int16_t>(*value);
        return element;
    }
    const auto value = readU32(bytes, range.offset);
    if (!value) {
        return std::nullopt;
    }
    element.unsignedValue = *value;
    element.signedValue = static_cast<std::int32_t>(*value);
    return element;
}

std::optional<std::vector<std::byte>> parseHexPattern(std::string_view text) {
    std::vector<std::byte> bytes;
    std::size_t index = 0;
    while (index < text.size()) {
        const auto character = text[index];
        if (character == ' ' || character == ',' || character == '\t') {
            ++index;
            continue;
        }
        if (character == '0' && index + 1 < text.size() &&
            (text[index + 1] == 'x' || text[index + 1] == 'X')) {
            index += 2;
            continue;
        }
        if (index + 1 >= text.size()) {
            return std::nullopt;
        }
        const auto high = hexValue(character);
        const auto low = hexValue(text[index + 1]);
        if (high < 0 || low < 0) {
            return std::nullopt;
        }
        bytes.push_back(static_cast<std::byte>(high * 16 + low));
        index += 2;
    }
    if (bytes.empty()) {
        return std::nullopt;
    }
    return bytes;
}

std::string formatHex(std::span<const std::byte> bytes, char separator) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    for (std::size_t index = 0; index < bytes.size(); ++index) {
        if (index != 0 && separator != '\0') {
            text.push_back(separator);
        }
        const auto value = std::to_integer<unsigned>(bytes[index]);
        text.push_back(digits[value >> 4]);
        text.push_back(digits[value & 0x0F]);
    }
    return text;
}

std::optional<std::size_t> findBytes(std::span<const std::byte> haystack,
                                     std::span<const std::byte> needle, std::size_t from,
                                     bool wrap) {
    if (needle.empty() || needle.size() > haystack.size()) {
        return std::nullopt;
    }
    const auto search = [&](std::size_t begin, std::size_t end) -> std::optional<std::size_t> {
        // Matches starting in [begin, end).
        end = std::min(end, haystack.size() - needle.size() + 1);
        for (std::size_t start = begin; start < end; ++start) {
            if (std::equal(needle.begin(), needle.end(), haystack.begin() + static_cast<std::ptrdiff_t>(start))) {
                return start;
            }
        }
        return std::nullopt;
    };
    if (from < haystack.size()) {
        if (const auto found = search(from, haystack.size())) {
            return found;
        }
    }
    if (wrap) {
        return search(0, std::min(from, haystack.size()));
    }
    return std::nullopt;
}

std::vector<SectionJump> sectionJumps(const WorldProbe &probe) {
    std::vector<SectionJump> jumps;
    if (probe.descriptor == nullptr) {
        return jumps;
    }
    for (const auto &section : probe.sections()) {
        if (section.availableBytes == 0) {
            continue;
        }
        jumps.push_back({std::string(sectionName(section.kind)), section.offset,
                         section.availableBytes});
    }
    if (probe.surplusBytes != 0) {
        jumps.push_back({"suffix", probe.descriptor->expectedBytes, probe.surplusBytes});
    }
    return jumps;
}

} // namespace eggbert
