// SPDX-License-Identifier: GPL-3.0-only
#include "world/revision_convert.hpp"

#include <algorithm>

#include "world/world_probe.hpp"
#include "world/world_view.hpp"

namespace eggbert {

namespace {

std::string number(std::size_t value) {
    return std::to_string(value);
}

} // namespace

std::expected<RevisionConversion, ConversionFailure>
convertRevision(std::span<const std::byte> source, FormatRevision target, bool allowLoss) {
    const auto probe = probeWorld(source);
    if (probe.descriptor == nullptr ||
        (probe.status != ProbeStatus::sizeMatches && probe.status != ProbeStatus::surplus)) {
        return std::unexpected(ConversionFailure{
            ConversionError::sourceNotConvertible,
            "Only a complete world of revision 1.0 to 1.3 can be converted."});
    }
    const auto &from = *probe.descriptor;
    const auto *to = findRevision(target);
    if (to == nullptr || to->revision == from.revision) {
        return std::unexpected(ConversionFailure{
            ConversionError::invalidTarget, "Choose a revision other than the world's own."});
    }
    const WorldView view(source, probe);
    RevisionConversion result;
    std::vector<std::string> losses;

    // Header: every byte kept, only the revision's minor number rewritten.
    const auto header = source.first(layout::headerBytes);
    result.bytes.assign(header.begin(), header.end());
    result.bytes[2] = static_cast<std::byte>(to->minor & 0xFF);
    result.bytes[3] = static_cast<std::byte>((to->minor >> 8) & 0xFF);
    if (const auto headerView = view.header()) {
        const bool titleRead = to->minor >= 3;
        if (headerView->nameLength() > 0 && !titleRead && from.minor >= 3) {
            result.notes.push_back("The title is kept in the file, but the games read it only "
                                   "from revision 1.3.");
        }
    }

    // Foreground: unchanged.
    const auto foreground = source.subspan(from.section(SectionKind::foreground)->offset,
                                           layout::tileLayerBytes);
    result.bytes.insert(result.bytes.end(), foreground.begin(), foreground.end());

    // bigDecor.
    if (to->hasBigDecor) {
        if (from.hasBigDecor) {
            const auto layer = source.subspan(from.section(SectionKind::bigDecor)->offset,
                                              layout::tileLayerBytes);
            result.bytes.insert(result.bytes.end(), layer.begin(), layer.end());
        } else {
            // As the games start a 1.0 world: every cell empty (-1).
            result.bytes.insert(result.bytes.end(), layout::tileLayerBytes, std::byte{0xFF});
            result.notes.push_back("An empty bigDecor layer was added.");
        }
    } else if (from.hasBigDecor) {
        std::size_t used = 0;
        const auto layer = *view.bigDecor();
        for (std::size_t x = 0; x < layout::mapWidth; ++x) {
            for (std::size_t y = 0; y < layout::mapHeight; ++y) {
                used += *layer.cell(x, y) != layout::emptyCell ? 1 : 0;
            }
        }
        if (used != 0) {
            losses.push_back(number(used) + " bigDecor cell(s) are not empty; revision 1.0 has "
                                            "no bigDecor layer.");
        } else {
            result.notes.push_back("The empty bigDecor layer was left out.");
        }
    }

    // Objects.
    const auto pool = source.subspan(from.section(SectionKind::objects)->offset,
                                     from.objectCount * layout::objectRecordBytes);
    const auto kept = std::min(from.objectCount, to->objectCount);
    result.bytes.insert(result.bytes.end(), pool.begin(),
                        pool.begin() + static_cast<std::ptrdiff_t>(kept * layout::objectRecordBytes));
    if (to->objectCount > from.objectCount) {
        const auto added = to->objectCount - from.objectCount;
        result.bytes.insert(result.bytes.end(), added * layout::objectRecordBytes, std::byte{0});
        result.notes.push_back(number(added) + " free object slots were added.");
    } else if (to->objectCount < from.objectCount) {
        const auto objects = *view.objects();
        std::size_t active = 0;
        std::size_t stale = 0;
        for (std::size_t slot = to->objectCount; slot < from.objectCount; ++slot) {
            if (!objects.record(slot)->isFree()) {
                ++active;
            } else if (std::ranges::any_of(objects.slotBytes(slot),
                                           [](std::byte value) { return value != std::byte{0}; })) {
                ++stale;
            }
        }
        if (active != 0) {
            losses.push_back(number(active) + " object(s) are stored in slots " +
                             number(to->objectCount) + " to " + number(from.objectCount - 1) +
                             "; the target revision stores only " + number(to->objectCount) +
                             " slots.");
        }
        if (stale != 0) {
            losses.push_back(number(stale) + " free slot(s) beyond the target's " +
                             number(to->objectCount) + " hold leftover bytes that would be "
                             "dropped.");
        }
        if (active == 0 && stale == 0) {
            result.notes.push_back(number(from.objectCount - to->objectCount) +
                                   " empty object slots were left out.");
        }
    }

    // Bytes after the layout are carried over as they are.
    if (const auto suffix = view.suffixRange(); suffix.size > 0) {
        const auto tail = source.subspan(suffix.offset, suffix.size);
        result.bytes.insert(result.bytes.end(), tail.begin(), tail.end());
        result.notes.push_back(number(suffix.size) + " byte(s) after the layout were carried over.");
    }

    if (!losses.empty()) {
        if (!allowLoss) {
            std::string detail;
            for (const auto &loss : losses) {
                detail += (detail.empty() ? "" : " ") + loss;
            }
            return std::unexpected(ConversionFailure{ConversionError::wouldLoseData, detail});
        }
        result.lossy = true;
        for (auto &loss : losses) {
            result.notes.push_back("Lost: " + loss);
        }
    }
    return result;
}

} // namespace eggbert
