// SPDX-License-Identifier: GPL-3.0-only
#include "world/atlas_table.hpp"

#include <algorithm>
#include <optional>

#include "world/le_reader.hpp"

namespace eggbert {

namespace {

constexpr std::size_t recordBytes = 12;

bool fits(const SpriteRect &sprite, const AtlasSearch &search) {
    if (sprite.srcX < 0 || sprite.srcY < 0 || sprite.offsetX < 0 || sprite.offsetY < 0 ||
        sprite.width < 0 || sprite.height < 0) {
        return false;
    }
    return sprite.srcX + sprite.width <= search.sheetWidth &&
           sprite.srcY + sprite.height <= search.sheetHeight &&
           sprite.offsetX + sprite.width <= search.maxExtentX &&
           sprite.offsetY + sprite.height <= search.maxExtentY;
}

std::optional<AtlasTable> candidateAt(std::span<const std::byte> bytes, std::size_t offset,
                                      const AtlasSearch &search) {
    AtlasTable table;
    table.fileOffset = offset;
    table.sprites.reserve(search.count);
    std::size_t nonEmpty = 0;
    for (std::size_t index = 0; index < search.count; ++index) {
        const auto base = offset + 2 + index * recordBytes;
        SpriteRect sprite;
        const auto value = [&](std::size_t field) {
            return readI16(bytes, base + 2 * field);
        };
        const auto srcX = value(0);
        const auto srcY = value(1);
        const auto offsetX = value(2);
        const auto offsetY = value(3);
        const auto width = value(4);
        const auto height = value(5);
        if (!srcX || !srcY || !offsetX || !offsetY || !width || !height) {
            return std::nullopt; // runs past the end of the bytes
        }
        sprite = {*srcX, *srcY, *offsetX, *offsetY, *width, *height};
        if (!fits(sprite, search)) {
            // Only the very last record may have a height that is not part
            // of the table; everything else about it must still fit.
            auto adjusted = sprite;
            adjusted.height = 0;
            if (index + 1 != search.count || !fits(adjusted, search)) {
                return std::nullopt;
            }
            const auto room = std::min(search.sheetHeight - sprite.srcY,
                                       search.maxExtentY - sprite.offsetY);
            sprite.height = static_cast<std::int16_t>(
                std::clamp<int>(sprite.height, 0, std::max(room, 0)));
            table.lastHeightAdjusted = true;
        }
        nonEmpty += sprite.empty() ? 0 : 1;
        table.sprites.push_back(sprite);
    }
    if (nonEmpty * 2 < search.count) {
        return std::nullopt; // mostly empty: zero fill or unrelated data
    }
    // Unrelated repetitive data can satisfy the bounds; real tables place
    // their sprites at many different sheet positions.
    std::vector<std::pair<int, int>> positions;
    for (const auto &sprite : table.sprites) {
        if (!sprite.empty()) {
            positions.emplace_back(sprite.srcX, sprite.srcY);
        }
    }
    std::ranges::sort(positions);
    const auto distinct =
        static_cast<std::size_t>(std::ranges::unique(positions).begin() - positions.begin());
    if (distinct * 2 < nonEmpty) {
        return std::nullopt;
    }
    return table;
}

} // namespace

std::expected<AtlasTable, AtlasSearchError> findAtlasTable(std::span<const std::byte> bytes,
                                                           const AtlasSearch &search) {
    if (search.count == 0 || search.count > 0x7FFF || search.sheetWidth <= 0 ||
        search.sheetHeight <= 0 || search.maxExtentX <= 0 || search.maxExtentY <= 0) {
        return std::unexpected(AtlasSearchError::notFound);
    }
    const auto needed = 2 + search.count * recordBytes;
    if (bytes.size() < needed) {
        return std::unexpected(AtlasSearchError::notFound);
    }
    std::optional<AtlasTable> found;
    for (std::size_t offset = 0; offset + needed <= bytes.size(); ++offset) {
        const auto count = readU16(bytes, offset);
        if (!count || *count != search.count) {
            continue;
        }
        const auto overlapsExcluded = std::ranges::any_of(search.excluded, [&](const ByteRange &range) {
            return offset < range.offset + range.size && range.offset < offset + needed;
        });
        if (overlapsExcluded) {
            continue;
        }
        auto candidate = candidateAt(bytes, offset, search);
        if (!candidate) {
            continue;
        }
        if (found && found->sprites != candidate->sprites) {
            return std::unexpected(AtlasSearchError::ambiguous);
        }
        if (!found) {
            found = std::move(candidate);
        }
    }
    if (!found) {
        return std::unexpected(AtlasSearchError::notFound);
    }
    return std::move(*found);
}

} // namespace eggbert
