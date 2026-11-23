// SPDX-License-Identifier: GPL-3.0-only
#include "world/merge.hpp"

#include <algorithm>

#include "world/world_probe.hpp"

namespace eggbert {

namespace {

bool complete(const WorldProbe &probe) {
    return probe.descriptor != nullptr &&
           (probe.status == ProbeStatus::sizeMatches || probe.status == ProbeStatus::surplus);
}

bool equal(std::span<const std::byte> left, std::span<const std::byte> right, ByteRange range) {
    return std::equal(left.begin() + static_cast<std::ptrdiff_t>(range.offset),
                      left.begin() + static_cast<std::ptrdiff_t>(range.end()),
                      right.begin() + static_cast<std::ptrdiff_t>(range.offset));
}

} // namespace

std::expected<MergeResult, std::string> mergeWorlds(std::span<const std::byte> base,
                                                    std::span<const std::byte> mine,
                                                    std::span<const std::byte> theirs,
                                                    MergePreference preference) {
    const auto baseProbe = probeWorld(base);
    const auto mineProbe = probeWorld(mine);
    const auto theirsProbe = probeWorld(theirs);
    if (!complete(baseProbe) || !complete(mineProbe) || !complete(theirsProbe)) {
        return std::unexpected(
            std::string("All three files must be complete worlds of a known revision."));
    }
    if (baseProbe.descriptor != mineProbe.descriptor ||
        baseProbe.descriptor != theirsProbe.descriptor) {
        return std::unexpected(std::string(
            "The three files are not of the same revision. Convert them to one revision first."));
    }
    if (base.size() != mine.size() || base.size() != theirs.size()) {
        return std::unexpected(std::string(
            "The three files differ in length (bytes after the world); they cannot be merged "
            "position by position."));
    }
    MergeResult result;
    result.bytes.assign(base.begin(), base.end());
    const auto &descriptor = *baseProbe.descriptor;
    std::size_t offset = 0;
    while (offset < base.size()) {
        // The element this offset starts.
        auto location = locateOffset(baseProbe, offset);
        ByteRange range{offset, 1};
        if (location.area == OffsetArea::objects) {
            if (const auto slot = slotByteRange(descriptor, location.slot)) {
                range = *slot;
            }
        } else if (!location.elementRange.empty()) {
            range = location.elementRange;
        }
        // Always start at an element boundary and stay inside the file.
        if (range.offset != offset || range.end() > base.size()) {
            range = {offset, 1};
        }
        const bool mineChanged = !equal(base, mine, range);
        const bool theirsChanged = !equal(base, theirs, range);
        const auto take = [&](std::span<const std::byte> from) {
            std::copy(from.begin() + static_cast<std::ptrdiff_t>(range.offset),
                      from.begin() + static_cast<std::ptrdiff_t>(range.end()),
                      result.bytes.begin() + static_cast<std::ptrdiff_t>(range.offset));
        };
        if (mineChanged && theirsChanged) {
            if (equal(mine, theirs, range)) {
                take(mine);
                ++result.agreed;
            } else {
                result.conflicts.push_back({range, location});
                if (preference == MergePreference::mine) {
                    take(mine);
                } else if (preference == MergePreference::theirs) {
                    take(theirs);
                }
            }
        } else if (mineChanged) {
            take(mine);
            ++result.fromMine;
        } else if (theirsChanged) {
            take(theirs);
            ++result.fromTheirs;
        }
        offset = range.end();
    }
    return result;
}

std::vector<std::byte> resolveMerge(const MergeResult &result, std::span<const std::byte> mine,
                                    std::span<const std::byte> theirs,
                                    const std::vector<bool> &takeTheirs) {
    auto bytes = result.bytes;
    for (std::size_t index = 0; index < result.conflicts.size(); ++index) {
        const auto &range = result.conflicts[index].range;
        const auto from = index < takeTheirs.size() && takeTheirs[index] ? theirs : mine;
        if (range.end() <= from.size() && range.end() <= bytes.size()) {
            std::copy(from.begin() + static_cast<std::ptrdiff_t>(range.offset),
                      from.begin() + static_cast<std::ptrdiff_t>(range.end()),
                      bytes.begin() + static_cast<std::ptrdiff_t>(range.offset));
        }
    }
    return bytes;
}

} // namespace eggbert
