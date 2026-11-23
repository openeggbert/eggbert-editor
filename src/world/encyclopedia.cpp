// SPDX-License-Identifier: GPL-3.0-only
#include "world/encyclopedia.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>

#include "world/adaptive.hpp"
#include "world/editor_groups.hpp"
#include "world/object_catalog.hpp"

namespace eggbert {

namespace {

std::string lower(std::string_view text) {
    std::string result(text);
    std::ranges::transform(result, result.begin(), [](char character) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    });
    return result;
}

std::string numberList(const std::vector<std::int16_t> &values) {
    std::string result;
    for (const auto value : values) {
        if (!result.empty()) {
            result += ", ";
        }
        result += std::to_string(value);
    }
    return result;
}

EncyclopediaEntry tileEntry(const EditorGroup &group, const TileEntry &tile) {
    EncyclopediaEntry entry;
    entry.kind = EntryKind::tile;
    entry.title = std::string(tile.name);
    entry.group = std::string(group.name);
    entry.layer = tile.layer;
    entry.values.assign(tile.values.begin(), tile.values.end());
    entry.facts = tile.layer == SectionKind::bigDecor ? "Back layer (bigDecor) " : "Foreground ";
    entry.facts += entry.values.size() == 1 ? "tile " : "tiles ";
    entry.facts += numberList(entry.values);
    if (entry.values.size() > 1) {
        entry.facts += "; the game's editor cycles through them in this order";
    }
    if (!tile.brushes.empty()) {
        entry.brush = std::string(tile.brushes.front());
        entry.facts += "; reshaped to fit its neighbours (adaptive family ";
        entry.facts += std::string(tile.brushes.front());
        entry.facts += ")";
    }
    entry.facts += ".";
    entry.provenance = std::string(tile.provenance);
    return entry;
}

EncyclopediaEntry objectEntry(const EditorGroup &group, const ObjectPreset &preset) {
    EncyclopediaEntry entry;
    entry.kind = EntryKind::object;
    entry.title = std::string(preset.name);
    entry.group = std::string(group.name);
    entry.id = std::string(preset.id);
    entry.type = preset.type;
    entry.channel = preset.channel;
    entry.icon = preset.icon;
    entry.facts = "Object type " + std::to_string(preset.type) + " (" +
                  std::string(preset.sourceName) + "); channel " +
                  std::to_string(preset.channel) + ", icon " + std::to_string(preset.icon) +
                  "; placed " + std::to_string(preset.offsetX) + ", " +
                  std::to_string(preset.offsetY) + " pixels into its cell";
    if (preset.hasPath) {
        entry.facts += "; travels between its start and its end, " +
                       std::to_string(preset.stepAdvance) + " tick(s) per cell out and " +
                       std::to_string(preset.stepRecede) + " back";
        if (preset.timeStopStart != 0 || preset.timeStopEnd != 0) {
            entry.facts += ", waiting " + std::to_string(preset.timeStopStart) +
                           " tick(s) at the start and " + std::to_string(preset.timeStopEnd) +
                           " at the end";
        }
    }
    if (preset.iconIsChoice) {
        entry.facts += "; the stored icon is a tile number of your choice";
    }
    entry.facts += ".";
    entry.provenance = std::string(preset.provenance);
    return entry;
}

} // namespace

std::vector<EncyclopediaEntry> encyclopedia(GameProfile profile) {
    std::vector<EncyclopediaEntry> entries;
    for (const auto &group : editorGroups(profile)) {
        for (const auto &tile : group.tiles) {
            entries.push_back(tileEntry(group, tile));
        }
        for (const auto id : group.presets) {
            if (const auto *preset = findObjectPreset(profile, id)) {
                entries.push_back(objectEntry(group, *preset));
            }
        }
    }
    for (const auto &family : adaptiveFamilies(profile)) {
        EncyclopediaEntry entry;
        entry.kind = EntryKind::border;
        entry.title = std::string(family.name);
        entry.group = "Adaptive borders";
        entry.id = std::string(family.id);
        entry.values.assign(1, family.seed);
        entry.facts = "Border family painted from tile " + std::to_string(family.seed) +
                      (family.bordersEmptyNeighbours
                           ? "; its own cells keep their value and border tiles are written "
                             "into the empty cells around them."
                           : "; each cell takes the form that fits its four neighbours.");
        entry.provenance = std::string(family.provenance);
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::vector<std::size_t> searchEncyclopedia(const std::vector<EncyclopediaEntry> &entries,
                                            std::string_view query) {
    std::vector<std::string> words;
    {
        const auto text = lower(query);
        std::size_t position = 0;
        while (position < text.size()) {
            const auto end = text.find(' ', position);
            const auto word = text.substr(position, end == std::string::npos ? end : end - position);
            if (!word.empty()) {
                words.push_back(word);
            }
            if (end == std::string::npos) {
                break;
            }
            position = end + 1;
        }
    }
    std::vector<std::size_t> matches;
    for (std::size_t index = 0; index < entries.size(); ++index) {
        const auto &entry = entries[index];
        const auto haystack = lower(entry.title) + '\n' + lower(entry.group) + '\n' +
                              lower(entry.facts) + '\n' + lower(entry.provenance);
        const bool all = std::ranges::all_of(words, [&](const std::string &word) {
            int number = 0;
            const auto parsed = std::from_chars(word.data(), word.data() + word.size(), number);
            if (parsed.ec == std::errc() && parsed.ptr == word.data() + word.size()) {
                // A number: an exact tile value or object type, not a digit
                // somewhere in a citation.
                if (entry.kind == EntryKind::object) {
                    return entry.type == number;
                }
                return std::ranges::find(entry.values, number) != entry.values.end();
            }
            return haystack.find(word) != std::string::npos;
        });
        if (all) {
            matches.push_back(index);
        }
    }
    return matches;
}

} // namespace eggbert
