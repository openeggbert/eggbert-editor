// SPDX-License-Identifier: GPL-3.0-only
#include "world/stamp.hpp"

#include <algorithm>
#include <charconv>

#include "world/object_ops.hpp"

namespace eggbert {

namespace {

std::unexpected<std::string> failAt(std::size_t line, std::string_view message) {
    return std::unexpected("Line " + std::to_string(line) + ": " + std::string(message));
}

// Splits on spaces; false when a word is not an integer in [low, high].
bool numbers(std::string_view line, long long low, long long high, std::vector<long long> &out) {
    out.clear();
    while (!line.empty()) {
        while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) {
            line.remove_prefix(1);
        }
        if (line.empty()) {
            break;
        }
        const auto end = line.find_first_of(" \t");
        const auto word = line.substr(0, end);
        long long value = 0;
        const auto parsed = std::from_chars(word.data(), word.data() + word.size(), value);
        if (parsed.ec != std::errc() || parsed.ptr != word.data() + word.size() || value < low ||
            value > high) {
            return false;
        }
        out.push_back(value);
        line.remove_prefix(end == std::string_view::npos ? line.size() : end);
    }
    return true;
}

Point shifted(Point point, long long dx, long long dy) {
    return {static_cast<std::int32_t>(point.x + dx), static_cast<std::int32_t>(point.y + dy)};
}

bool insideWorld(long long x, long long y) {
    return x >= 0 && y >= 0 && x < layout::worldPixelWidth && y < layout::worldPixelHeight;
}

} // namespace

Stamp makeStamp(const WorldView &world, const CellRect &rect, std::string name, GameProfile game,
                bool withObjects) {
    Stamp stamp;
    stamp.name = std::move(name);
    stamp.game = game;
    stamp.width = rect.width();
    stamp.height = rect.height();
    const auto copy = [&](SectionKind kind, std::vector<std::int16_t> &out) {
        out.assign(stamp.width * stamp.height, layout::emptyCell);
        const auto layer = world.layer(kind);
        if (!layer) {
            return;
        }
        for (std::size_t y = 0; y < stamp.height; ++y) {
            for (std::size_t x = 0; x < stamp.width; ++x) {
                if (const auto value = layer->cell(rect.min.x + x, rect.min.y + y)) {
                    out[y * stamp.width + x] = *value;
                }
            }
        }
    };
    copy(SectionKind::foreground, stamp.foreground);
    copy(SectionKind::bigDecor, stamp.bigDecor);
    const auto objects = world.objects();
    if (withObjects && objects) {
        const auto left = static_cast<long long>(rect.min.x) * layout::cellPixels;
        const auto top = static_cast<long long>(rect.min.y) * layout::cellPixels;
        const auto right = left + static_cast<long long>(stamp.width) * layout::cellPixels;
        const auto bottom = top + static_cast<long long>(stamp.height) * layout::cellPixels;
        for (std::size_t slot = 0; slot < objects->slotCount(); ++slot) {
            auto record = *objects->record(slot);
            if (record.isFree() || record.posStart.x < left || record.posStart.x >= right ||
                record.posStart.y < top || record.posStart.y >= bottom) {
                continue;
            }
            record.posStart = shifted(record.posStart, -left, -top);
            record.posEnd = shifted(record.posEnd, -left, -top);
            record.posCurrent = shifted(record.posCurrent, -left, -top);
            stamp.objects.push_back(record);
        }
    }
    return stamp;
}

std::expected<StampPlacement, std::string> placeStamp(EditBuilder &builder, const Stamp &stamp,
                                                      CellPos at) {
    if (stamp.width == 0 || stamp.height == 0 ||
        stamp.foreground.size() != stamp.width * stamp.height ||
        stamp.bigDecor.size() != stamp.width * stamp.height) {
        return std::unexpected(std::string("The stamp is empty or damaged."));
    }
    const auto view = builder.view();
    const auto pool = view.objects();
    // Objects first: nothing is written unless all of them fit.
    const auto left = static_cast<long long>(at.x) * layout::cellPixels;
    const auto top = static_cast<long long>(at.y) * layout::cellPixels;
    std::vector<ObjectRecord> placed;
    for (auto record : stamp.objects) {
        record.posStart = shifted(record.posStart, left, top);
        record.posEnd = shifted(record.posEnd, left, top);
        record.posCurrent = shifted(record.posCurrent, left, top);
        if (!insideWorld(record.posStart.x, record.posStart.y) ||
            !insideWorld(record.posEnd.x, record.posEnd.y)) {
            return std::unexpected(std::string(
                "An object of the stamp would lie outside the world here. Place the stamp "
                "further from the edge."));
        }
        placed.push_back(record);
    }
    std::vector<std::size_t> slots;
    if (!placed.empty()) {
        if (!pool) {
            return std::unexpected(std::string("This world has no object records."));
        }
        slots = freeSlots(view, placed.size());
        if (slots.size() < placed.size()) {
            return std::unexpected("The stamp has " + std::to_string(placed.size()) +
                                   " object(s) but only " +
                                   std::to_string(poolCapacity(view).free) +
                                   " object slot(s) are free.");
        }
    }
    StampPlacement result;
    const auto paste = [&](SectionKind kind, const std::vector<std::int16_t> &values) {
        const auto layer = view.layer(kind);
        for (std::size_t y = 0; y < stamp.height; ++y) {
            for (std::size_t x = 0; x < stamp.width; ++x) {
                const auto value = values[y * stamp.width + x];
                if (value == layout::emptyCell) {
                    continue; // a hole
                }
                const auto cx = at.x + x;
                const auto cy = at.y + y;
                if (!layer || !TileLayerView::contains(cx, cy)) {
                    ++result.tilesClipped;
                    continue;
                }
                if (*layer->cell(cx, cy) != value && builder.setCell(kind, cx, cy, value)) {
                    ++result.tilesChanged;
                }
            }
        }
    };
    paste(SectionKind::foreground, stamp.foreground);
    paste(SectionKind::bigDecor, stamp.bigDecor);
    for (std::size_t index = 0; index < placed.size(); ++index) {
        builder.setObjectRecord(slots[index], placed[index]);
        ++result.objectsCreated;
    }
    return result;
}

std::string writeStamp(const Stamp &stamp) {
    std::string out = "eggbert-stamp 1\n";
    out += "name " + stamp.name + "\n";
    out += std::string("game ") + (stamp.game == GameProfile::speedyBlupi1   ? "1"
                                   : stamp.game == GameProfile::speedyBlupi2 ? "2"
                                                                             : "0") +
           "\n";
    out += "size " + std::to_string(stamp.width) + " " + std::to_string(stamp.height) + "\n";
    const auto layer = [&](std::string_view title, const std::vector<std::int16_t> &values) {
        out += std::string(title) + "\n";
        for (std::size_t y = 0; y < stamp.height; ++y) {
            for (std::size_t x = 0; x < stamp.width; ++x) {
                out += (x == 0 ? "" : " ") + std::to_string(values[y * stamp.width + x]);
            }
            out += "\n";
        }
    };
    layer("foreground", stamp.foreground);
    layer("bigdecor", stamp.bigDecor);
    out += "objects " + std::to_string(stamp.objects.size()) + "\n";
    for (const auto &record : stamp.objects) {
        const long long fields[] = {record.type,        record.stepAdvance,  record.stepRecede,
                                    record.timeStopStart, record.timeStopEnd, record.unknown1,
                                    record.posStart.x,  record.posStart.y,   record.posEnd.x,
                                    record.posEnd.y,    record.posCurrent.x, record.posCurrent.y,
                                    record.step,        record.time,         record.phase,
                                    record.channel,     record.icon,         record.unknown2};
        bool first = true;
        for (const auto field : fields) {
            out += (first ? "" : " ") + std::to_string(field);
            first = false;
        }
        out += "\n";
    }
    return out;
}

std::expected<Stamp, std::string> readStamp(std::string_view text) {
    std::vector<std::string_view> lines;
    while (!text.empty()) {
        const auto end = text.find('\n');
        auto line = text.substr(0, end);
        if (!line.empty() && line.back() == '\r') {
            line.remove_suffix(1);
        }
        lines.push_back(line);
        text.remove_prefix(end == std::string_view::npos ? text.size() : end + 1);
    }
    std::size_t index = 0;
    const auto next = [&]() -> std::string_view {
        return index < lines.size() ? lines[index++] : std::string_view();
    };
    Stamp stamp;
    std::vector<long long> values;
    if (next() != "eggbert-stamp 1") {
        return failAt(1, "this is not a stamp file of format 1.");
    }
    auto line = next();
    if (!line.starts_with("name ") || line.size() > 5 + 100) {
        return failAt(index, "expected \"name\" and a name of at most 100 characters.");
    }
    stamp.name = std::string(line.substr(5));
    line = next();
    if (line == "game 1") {
        stamp.game = GameProfile::speedyBlupi1;
    } else if (line == "game 2") {
        stamp.game = GameProfile::speedyBlupi2;
    } else if (line != "game 0") {
        return failAt(index, "expected \"game 0\", \"game 1\" or \"game 2\".");
    }
    line = next();
    if (!line.starts_with("size ") ||
        !numbers(line.substr(5), 1, static_cast<long long>(layout::mapWidth), values) ||
        values.size() != 2) {
        return failAt(index, "expected \"size\" with a width and a height from 1 to 100.");
    }
    stamp.width = static_cast<std::size_t>(values[0]);
    stamp.height = static_cast<std::size_t>(values[1]);
    const auto layer = [&](std::string_view title,
                           std::vector<std::int16_t> &out) -> std::expected<void, std::string> {
        if (next() != title) {
            return failAt(index, "expected \"" + std::string(title) + "\".");
        }
        for (std::size_t y = 0; y < stamp.height; ++y) {
            if (!numbers(next(), -32'768, 32'767, values) || values.size() != stamp.width) {
                return failAt(index, "expected " + std::to_string(stamp.width) +
                                         " tile numbers from -32768 to 32767.");
            }
            for (const auto value : values) {
                out.push_back(static_cast<std::int16_t>(value));
            }
        }
        return {};
    };
    if (const auto read = layer("foreground", stamp.foreground); !read) {
        return std::unexpected(read.error());
    }
    if (const auto read = layer("bigdecor", stamp.bigDecor); !read) {
        return std::unexpected(read.error());
    }
    line = next();
    if (!line.starts_with("objects ") ||
        !numbers(line.substr(8), 0, static_cast<long long>(stampMostObjects), values) ||
        values.size() != 1) {
        return failAt(index, "expected \"objects\" and a count from 0 to 200.");
    }
    const auto count = static_cast<std::size_t>(values[0]);
    for (std::size_t object = 0; object < count; ++object) {
        // Positions are 32-bit in a record; the other fields 16-bit.
        if (!numbers(next(), -2'147'483'648LL, 2'147'483'647LL, values) || values.size() != 18) {
            return failAt(index, "expected the eighteen numbers of an object record.");
        }
        for (const std::size_t field : {0u, 1u, 2u, 3u, 4u, 5u, 12u, 13u, 14u, 15u, 16u, 17u}) {
            if (values[field] < -32'768 || values[field] > 32'767) {
                return failAt(index, "an object field is outside the 16-bit range.");
            }
        }
        const auto small = [&](std::size_t field) { return static_cast<std::int16_t>(values[field]); };
        const auto point = [&](std::size_t field) {
            return Point{static_cast<std::int32_t>(values[field]),
                         static_cast<std::int32_t>(values[field + 1])};
        };
        ObjectRecord record;
        record.type = small(0);
        record.stepAdvance = small(1);
        record.stepRecede = small(2);
        record.timeStopStart = small(3);
        record.timeStopEnd = small(4);
        record.unknown1 = small(5);
        record.posStart = point(6);
        record.posEnd = point(8);
        record.posCurrent = point(10);
        record.step = small(12);
        record.time = small(13);
        record.phase = small(14);
        record.channel = small(15);
        record.icon = small(16);
        record.unknown2 = small(17);
        if (record.isFree()) {
            return failAt(index, "an object of a stamp cannot have type 0.");
        }
        stamp.objects.push_back(record);
    }
    while (index < lines.size()) {
        if (!next().empty()) {
            return failAt(index, "unexpected text after the objects.");
        }
    }
    return stamp;
}

std::size_t alignObjects(EditBuilder &builder, std::span<const std::size_t> slots,
                         AlignMode mode) {
    const auto pool = builder.view().objects();
    if (!pool) {
        return 0;
    }
    struct Item {
        std::size_t slot;
        ObjectRecord record;
    };
    std::vector<Item> items;
    for (const auto slot : slots) {
        const auto record = pool->record(slot);
        if (record && !record->isFree() &&
            std::ranges::none_of(items, [&](const Item &item) { return item.slot == slot; })) {
            items.push_back({slot, *record});
        }
    }
    const bool distribute = mode == AlignMode::distributeHorizontally ||
                            mode == AlignMode::distributeVertically;
    if (items.size() < (distribute ? 3u : 2u)) {
        return 0;
    }
    const bool horizontal = mode == AlignMode::left || mode == AlignMode::right ||
                            mode == AlignMode::distributeHorizontally;
    const auto coordinate = [&](const ObjectRecord &record) -> long long {
        return horizontal ? record.posStart.x : record.posStart.y;
    };
    long long low = coordinate(items.front().record);
    long long high = low;
    for (const auto &item : items) {
        low = std::min(low, coordinate(item.record));
        high = std::max(high, coordinate(item.record));
    }
    if (distribute) {
        std::ranges::stable_sort(items, [&](const Item &a, const Item &b) {
            return coordinate(a.record) < coordinate(b.record);
        });
    }
    std::size_t changed = 0;
    for (std::size_t index = 0; index < items.size(); ++index) {
        auto record = items[index].record;
        long long target = 0;
        switch (mode) {
        case AlignMode::left:
        case AlignMode::top:
            target = low;
            break;
        case AlignMode::right:
        case AlignMode::bottom:
            target = high;
            break;
        case AlignMode::distributeHorizontally:
        case AlignMode::distributeVertically:
            target = low + (high - low) * static_cast<long long>(index) /
                               static_cast<long long>(items.size() - 1);
            break;
        }
        const auto delta = target - coordinate(record);
        if (delta == 0) {
            continue;
        }
        const auto dx = horizontal ? delta : 0;
        const auto dy = horizontal ? 0 : delta;
        record.posStart = shifted(record.posStart, dx, dy);
        record.posEnd = shifted(record.posEnd, dx, dy);
        record.posCurrent = shifted(record.posCurrent, dx, dy);
        if (builder.setObjectRecord(items[index].slot, record)) {
            ++changed;
        }
    }
    return changed;
}

} // namespace eggbert
