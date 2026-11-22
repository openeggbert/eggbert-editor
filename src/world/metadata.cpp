// SPDX-License-Identifier: GPL-3.0-only
#include "world/metadata.hpp"

#include <algorithm>

namespace eggbert {

namespace {

// Windows-1252 code points for bytes 0x80..0x9F. Bytes the code page leaves
// undefined map to the C1 control of the same value so that every byte
// decodes and re-encodes to itself.
constexpr char32_t highTable[32] = {
    0x20AC, 0x0081, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
    0x2039, 0x0152, 0x008D, 0x017D, 0x008F, 0x0090, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
    0x2013, 0x2014, 0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x009D, 0x017E, 0x0178,
};

} // namespace

std::optional<CellPos> cellOfPosition(Point position) {
    if (position.x < 0 || position.y < 0 || position.x >= layout::worldPixelWidth ||
        position.y >= layout::worldPixelHeight) {
        return std::nullopt;
    }
    return CellPos{static_cast<std::size_t>(position.x / layout::cellPixels),
                   static_cast<std::size_t>(position.y / layout::cellPixels)};
}

std::u32string decodeWindows1252(std::span<const std::byte> bytes) {
    std::u32string text;
    text.reserve(bytes.size());
    for (const auto value : bytes) {
        const auto code = std::to_integer<unsigned>(value);
        text.push_back(code >= 0x80 && code <= 0x9F ? highTable[code - 0x80]
                                                    : static_cast<char32_t>(code));
    }
    return text;
}

std::optional<std::vector<std::byte>> encodeWindows1252(std::u32string_view text) {
    std::vector<std::byte> bytes;
    bytes.reserve(text.size());
    for (const auto character : text) {
        if (character < 0x20) {
            return std::nullopt;
        }
        if (character < 0x80 || (character >= 0xA0 && character <= 0xFF)) {
            bytes.push_back(static_cast<std::byte>(character));
            continue;
        }
        const auto *found = std::ranges::find(highTable, character);
        if (found == std::end(highTable)) {
            return std::nullopt;
        }
        bytes.push_back(static_cast<std::byte>(0x80 + (found - std::begin(highTable))));
    }
    return bytes;
}

std::optional<Metadata> readMetadata(const WorldView &world) {
    const auto header = world.header();
    if (!header) {
        return std::nullopt;
    }
    Metadata metadata;
    const auto name = header->nameText();
    metadata.name.assign(name.begin(), name.end());
    metadata.music = header->music();
    metadata.region = header->region();
    metadata.scrollsHorizontally = header->dimDecor().x != 0;
    metadata.scrollsVertically = header->dimDecor().y != 0;
    metadata.posDecor = header->posDecor();
    for (std::size_t index = 0; index < 4; ++index) {
        metadata.starts[index] = {*header->blupiPos(index), *header->blupiDir(index)};
    }
    return metadata;
}

bool applyMetadata(EditBuilder &builder, const Metadata &wanted) {
    const auto current = readMetadata(builder.view());
    if (!current) {
        return false;
    }
    // An unchanged title is kept whatever it is (even one that fills the
    // field without a terminator); only a new title must be storable.
    if (wanted.name != current->name) {
        if (wanted.name.size() > storedTitleLimit ||
            std::ranges::find(wanted.name, std::byte{0}) != wanted.name.end()) {
            return false;
        }
        builder.setName(wanted.name);
    }
    if (wanted.music != current->music) {
        builder.setMusic(wanted.music);
    }
    if (wanted.region != current->region) {
        builder.setRegion(wanted.region);
    }
    if (wanted.scrollsHorizontally != current->scrollsHorizontally ||
        wanted.scrollsVertically != current->scrollsVertically) {
        auto raw = builder.view().header()->dimDecor();
        if (wanted.scrollsHorizontally != current->scrollsHorizontally) {
            raw.x = wanted.scrollsHorizontally ? 100 : 0;
        }
        if (wanted.scrollsVertically != current->scrollsVertically) {
            raw.y = wanted.scrollsVertically ? 100 : 0;
        }
        builder.setDimDecor(raw);
    }
    if (wanted.posDecor != current->posDecor) {
        builder.setPosDecor(wanted.posDecor);
    }
    for (std::size_t index = 0; index < 4; ++index) {
        if (wanted.starts[index].position != current->starts[index].position) {
            builder.setBlupiPos(index, wanted.starts[index].position);
        }
        if (wanted.starts[index].direction != current->starts[index].direction) {
            builder.setBlupiDir(index, wanted.starts[index].direction);
        }
    }
    return true;
}

bool placeStart(EditBuilder &builder, std::size_t index, CellPos cell) {
    if (cell.x >= layout::mapWidth || cell.y >= layout::mapHeight) {
        return false;
    }
    return builder.setBlupiPos(index, startPositionForCell(cell));
}

bool toggleStartDirection(EditBuilder &builder, std::size_t index) {
    const auto header = builder.view().header();
    if (!header) {
        return false;
    }
    const auto direction = header->blupiDir(index);
    if (!direction) {
        return false;
    }
    // As in the games' editors: left becomes right, everything else left.
    return builder.setBlupiDir(index, *direction == startDirectionLeft ? startDirectionRight
                                                                        : startDirectionLeft);
}

bool copyStartToAll(EditBuilder &builder, std::size_t from) {
    const auto header = builder.view().header();
    if (!header || from >= 4) {
        return false;
    }
    const auto position = *header->blupiPos(from);
    const auto direction = *header->blupiDir(from);
    for (std::size_t index = 0; index < 4; ++index) {
        builder.setBlupiPos(index, position);
        builder.setBlupiDir(index, direction);
    }
    return true;
}

} // namespace eggbert
