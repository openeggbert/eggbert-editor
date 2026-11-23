// SPDX-License-Identifier: GPL-3.0-only
//
// The encyclopedia repeats the catalogs: one entry per tile entry, object
// and border family of a game, each with the catalog's citation.
#include "world/encyclopedia.hpp"

#include "world/adaptive.hpp"
#include "world/doctor.hpp"
#include "world/editor_groups.hpp"
#include "world/object_catalog.hpp"

#include "check.hpp"

#include <algorithm>
#include <cstdio>
#include <set>

using namespace eggbert;

int main() {
    CHECK(encyclopedia(GameProfile::unknown).empty());

    for (const auto profile : {GameProfile::speedyBlupi1, GameProfile::speedyBlupi2}) {
        const auto entries = encyclopedia(profile);
        std::size_t tiles = 0;
        std::size_t presets = 0;
        for (const auto &group : editorGroups(profile)) {
            tiles += group.tiles.size();
            presets += group.presets.size();
        }
        const auto count = [&](EntryKind kind) {
            return static_cast<std::size_t>(std::ranges::count_if(
                entries, [kind](const EncyclopediaEntry &entry) { return entry.kind == kind; }));
        };
        CHECK(tiles > 0 && presets > 0);
        CHECK_EQ(count(EntryKind::tile), tiles);
        CHECK_EQ(count(EntryKind::object), presets);
        CHECK_EQ(count(EntryKind::border), adaptiveFamilies(profile).size());
        CHECK_EQ(entries.size(), tiles + presets + adaptiveFamilies(profile).size());

        std::set<std::string> objectIds;
        for (const auto &entry : entries) {
            // Every entry is named, described and cited.
            CHECK(!entry.title.empty());
            CHECK(!entry.group.empty());
            CHECK(!entry.facts.empty() && entry.facts.back() == '.');
            CHECK(!entry.provenance.empty());
            switch (entry.kind) {
            case EntryKind::tile:
                CHECK(!entry.values.empty());
                CHECK(entry.layer == SectionKind::foreground ||
                      entry.layer == SectionKind::bigDecor);
                break;
            case EntryKind::object: {
                const auto *preset = findObjectPreset(profile, entry.id);
                CHECK(preset != nullptr);
                CHECK(preset != nullptr && entry.provenance == preset->provenance);
                CHECK(preset != nullptr && entry.type == preset->type &&
                      entry.icon == preset->icon && entry.channel == preset->channel);
                objectIds.insert(entry.id);
                break;
            }
            case EntryKind::border: {
                const auto *family = findAdaptiveFamily(profile, entry.id);
                CHECK(family != nullptr);
                CHECK(family != nullptr && entry.values == std::vector{family->seed});
                break;
            }
            }
        }
        // Every preset the groups name is there.
        for (const auto &group : editorGroups(profile)) {
            for (const auto id : group.presets) {
                CHECK(objectIds.contains(std::string(id)));
            }
        }

        // Search.
        CHECK_EQ(searchEncyclopedia(entries, "").size(), entries.size());
        CHECK_EQ(searchEncyclopedia(entries, "   ").size(), entries.size());
        CHECK(searchEncyclopedia(entries, "no such thing anywhere").empty());
        const auto &first = entries.front();
        const auto byTitle = searchEncyclopedia(entries, first.title);
        CHECK(std::ranges::find(byTitle, std::size_t{0}) != byTitle.end());
        // Case does not matter; every word must match.
        std::string shouted = first.title;
        std::ranges::transform(shouted, shouted.begin(), [](char character) {
            return static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
        });
        CHECK(searchEncyclopedia(entries, shouted) == byTitle);
        CHECK(searchEncyclopedia(entries, first.title + " zzzz").empty());
        // A number finds the entries that store exactly that value or type.
        const auto treasure = std::ranges::find_if(entries, [](const EncyclopediaEntry &entry) {
            return entry.kind == EntryKind::object && entry.type == 5;
        });
        CHECK(treasure != entries.end());
        for (const auto index : searchEncyclopedia(entries, "5")) {
            const auto &entry = entries[index];
            CHECK(entry.kind == EntryKind::object
                      ? entry.type == 5
                      : std::ranges::find(entry.values, std::int16_t{5}) != entry.values.end());
        }
        CHECK(!searchEncyclopedia(entries, "5").empty());
    }
    // The facts are the filled parts in order, every part is a listed
    // pattern, and every listed pattern occurs in one of the two games.
    {
        const auto listed = encyclopediaFactPatterns();
        std::set<std::string_view> used;
        for (const auto profile : {GameProfile::speedyBlupi1, GameProfile::speedyBlupi2}) {
            for (const auto &entry : encyclopedia(profile)) {
                std::string rebuilt;
                for (const auto &part : entry.factParts) {
                    CHECK(std::ranges::find(listed, part.pattern) != listed.end());
                    rebuilt += fillPattern(part.pattern, part.arguments);
                    used.insert(part.pattern);
                }
                CHECK(rebuilt == entry.facts);
                CHECK(!entry.factParts.empty());
            }
        }
        for (const auto pattern : listed) {
            if (!used.contains(pattern)) {
                std::fprintf(stderr, "fact pattern never used: %.*s\n",
                             static_cast<int>(pattern.size()), pattern.data());
            }
            CHECK(used.contains(pattern));
        }
    }
    // The second game knows more.
    CHECK(encyclopedia(GameProfile::speedyBlupi2).size() >
          encyclopedia(GameProfile::speedyBlupi1).size());
    return test::finish("encyclopedia");
}
