// SPDX-License-Identifier: GPL-3.0-only
#include "world/editor_groups.hpp"

#include "world/adaptive.hpp"
#include "world/object_catalog.hpp"
#include "world/profile.hpp"

#include "check.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace eggbert;

namespace {

const EditorGroup *find(GameProfile profile, std::string_view id) {
    for (const auto &group : editorGroups(profile)) {
        if (group.id == id) {
            return &group;
        }
    }
    return nullptr;
}

const TileEntry *entry(const EditorGroup *group, std::string_view name) {
    if (group == nullptr) {
        return nullptr;
    }
    for (const auto &tiles : group->tiles) {
        if (tiles.name == name) {
            return &tiles;
        }
    }
    return nullptr;
}

std::vector<int> values(const TileEntry *tiles) {
    return tiles == nullptr ? std::vector<int>{}
                            : std::vector<int>(tiles->values.begin(), tiles->values.end());
}

} // namespace

int main() {
    CHECK(editorGroups(GameProfile::unknown).empty());
    // One group per editor button that places something (the delete button
    // has none): seven in the first game, ten in the second.
    CHECK_EQ(editorGroups(GameProfile::speedyBlupi1).size(), std::size_t{7});
    CHECK_EQ(editorGroups(GameProfile::speedyBlupi2).size(), std::size_t{10});

    for (const auto profile : {GameProfile::speedyBlupi1, GameProfile::speedyBlupi2}) {
        const auto *info = profileInfo(profile);
        std::set<std::string_view> ids;
        std::map<std::string, int> presetUse;
        std::set<std::string_view> families;
        for (const auto &family : adaptiveFamilies(profile)) {
            families.insert(family.id);
        }
        for (const auto &group : editorGroups(profile)) {
            CHECK(ids.insert(group.id).second);
            CHECK(!group.name.empty());
            CHECK(!group.provenance.empty());
            CHECK(!group.tiles.empty() || !group.presets.empty());
            for (const auto &tiles : group.tiles) {
                CHECK(!tiles.name.empty());
                CHECK(!tiles.values.empty());
                CHECK(!tiles.provenance.empty());
                CHECK(tiles.brushes.empty() || tiles.brushes.size() == tiles.values.size());
                for (const auto brush : tiles.brushes) {
                    CHECK(families.contains(brush));
                }
                std::set<int> seen;
                for (const auto value : tiles.values) {
                    CHECK(seen.insert(value).second); // no number twice in one entry
                    if (tiles.layer == SectionKind::bigDecor) {
                        CHECK(value == 203 || info->bigDecorAtlas.contains(value));
                    } else {
                        CHECK(info->foregroundAtlas.contains(value));
                    }
                }
            }
            for (const auto id : group.presets) {
                CHECK(findObjectPreset(profile, id) != nullptr);
                ++presetUse[std::string(id)];
            }
        }
        // Every object the catalog offers is in exactly one group.
        for (const auto &preset : objectPresets(profile)) {
            CHECK_EQ(presetUse[std::string(preset.id)], 1);
        }
        CHECK_EQ(presetUse.size(), objectPresets(profile).size());
    }

    // Entries re-read at their citations.
    const auto first = GameProfile::speedyBlupi1;
    const auto second = GameProfile::speedyBlupi2;
    {
        const auto *scenery = find(first, "scenery");
        CHECK(scenery != nullptr);
        CHECK_EQ(scenery->tiles.size(), std::size_t{7});
        CHECK(scenery->presets.empty());
        CHECK(entry(scenery, "Tree")->layer == SectionKind::bigDecor);
        CHECK((values(entry(scenery, "Tree")) == std::vector<int>{20, 21, 22}));
        CHECK((values(entry(scenery, "Marine plant")) == std::vector<int>{203}));
        CHECK((values(entry(scenery, "Toy background")) ==
               std::vector<int>{45, 46, 47, 36, 37, 38, 39, 40, 41, 42, 43, 44}));
        const auto dream = values(entry(find(first, "buildings"), "Dream"));
        CHECK_EQ(dream.size(), std::size_t{20});
        CHECK_EQ(dream.front(), 48);
        CHECK_EQ(dream.back(), 67);
        CHECK((values(entry(find(first, "hazards-vehicles"), "Fan")) ==
               std::vector<int>{126, 129, 132, 135}));
        // Tiles the games' editors reshape name their adaptive family.
        const auto *rock = entry(find(first, "rock-tech"), "Square rock");
        CHECK(rock != nullptr && rock->brushes.size() == 1 && rock->brushes[0] == "rock");
        CHECK(entry(find(first, "rock-tech"), "Secret rock")->brushes.empty());
        // Objects in menu order.
        const auto *lifts = find(first, "lifts-enemies");
        CHECK(lifts != nullptr && lifts->tiles.empty());
        CHECK_EQ(lifts->presets.size(), std::size_t{12});
        CHECK(lifts->presets.front() == "lift-slow");
        CHECK(lifts->presets.back() == "enemy-tank");
        CHECK(find(first, "doors-keys")->presets.size() == 1);
    }
    {
        // The second game's scenery button also writes two foreground kinds.
        const auto *scenery = find(second, "scenery");
        CHECK(scenery != nullptr);
        CHECK_EQ(scenery->tiles.size(), std::size_t{11});
        CHECK(entry(scenery, "Spider's web")->layer == SectionKind::foreground);
        CHECK((values(entry(scenery, "Spider's web")) == std::vector<int>{403, 401, 402}));
        CHECK((values(entry(find(second, "devices"), "Teleporter")) ==
               std::vector<int>{330, 331, 332, 333}));
        const auto *cheese = entry(find(second, "rock-cave"), "Cheese");
        CHECK((values(cheese) == std::vector<int>{246, 247, 248, 249}));
        CHECK(cheese->brushes.size() == 4 && cheese->brushes[1] == "cheese-247");
        const auto *cave = entry(find(second, "rock-cave"), "Cave");
        CHECK((values(cave) == std::vector<int>{284, 301}));
        CHECK(cave->brushes[1] == "cave-301");
        CHECK_EQ(find(second, "vehicles")->presets.size(), std::size_t{5});
        CHECK(find(second, "no-such-group") == nullptr);
    }
    return test::finish("editor_groups");
}
