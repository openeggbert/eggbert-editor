// SPDX-License-Identifier: GPL-3.0-only
#include "world/editor_groups.hpp"

#include <array>

namespace eggbert {

namespace {

// What each button of the games' own level editors offers, entry by entry,
// read from blupi_recon at commit e05e22b990abb782c37a0d9fafa864d86a27e29f
// (docs/research/evidence.md, "The games' editor menus"). Group names are
// this project's own: the games' buttons have none. Tile numbers are listed
// in the order the editors cycle through them.

constexpr std::int16_t sb1_scenery_0_values[] = {20, 21, 22};
constexpr std::int16_t sb1_scenery_1_values[] = {16, 17, 18, 19};
constexpr std::int16_t sb1_scenery_2_values[] = {23, 24, 25};
constexpr std::int16_t sb1_scenery_3_values[] = {203};
constexpr std::int16_t sb1_scenery_4_values[] = {26, 27};
constexpr std::int16_t sb1_scenery_5_values[] = {28, 29, 30, 31};
constexpr std::int16_t sb1_scenery_6_values[] = {45, 46, 47, 36, 37, 38, 39, 40, 41, 42, 43, 44};
constexpr TileEntry sb1_scenery_tiles[] = {
    {"Tree", SectionKind::bigDecor, sb1_scenery_0_values, {},
     "src/decdesign.cpp:82-87; src/decblock.cpp:187"},
    {"Palm tree", SectionKind::bigDecor, sb1_scenery_1_values, {},
     "src/decdesign.cpp:88-93; src/decblock.cpp:189"},
    {"Decorative plant", SectionKind::bigDecor, sb1_scenery_2_values, {},
     "src/decdesign.cpp:94-99; src/decblock.cpp:191"},
    {"Marine plant", SectionKind::bigDecor, sb1_scenery_3_values, {},
     "src/decdesign.cpp:100"},
    {"House", SectionKind::bigDecor, sb1_scenery_4_values, {},
     "src/decdesign.cpp:101-106; src/decblock.cpp:193"},
    {"Mechanical background", SectionKind::bigDecor, sb1_scenery_5_values, {},
     "src/decdesign.cpp:107-112; src/decblock.cpp:195"},
    {"Toy background", SectionKind::bigDecor, sb1_scenery_6_values, {},
     "src/decdesign.cpp:113-118; src/decblock.cpp:197"},
};

constexpr std::int16_t sb1_rock_tech_0_values[] = {153};
constexpr std::string_view sb1_rock_tech_0_brushes[] = {"rock"};
constexpr std::int16_t sb1_rock_tech_1_values[] = {154, 155};
constexpr std::string_view sb1_rock_tech_1_brushes[] = {"triangular-rock", "triangular-rock"};
constexpr std::int16_t sb1_rock_tech_2_values[] = {185};
constexpr std::int16_t sb1_rock_tech_3_values[] = {10, 2, 3, 4, 5, 6, 7, 8, 9, 1, 11, 12, 13, 14};
constexpr std::int16_t sb1_rock_tech_4_values[] = {19, 20, 21, 23, 24, 25, 26, 27, 28, 29};
constexpr std::int16_t sb1_rock_tech_5_values[] = {15, 16, 17, 18};
constexpr std::int16_t sb1_rock_tech_6_values[] = {73, 22};
constexpr std::int16_t sb1_rock_tech_7_values[] = {78, 79, 80, 81, 82, 83, 84};
constexpr std::int16_t sb1_rock_tech_8_values[] = {87, 88, 89, 90};
constexpr std::int16_t sb1_rock_tech_9_values[] = {85, 86};
constexpr TileEntry sb1_rock_tech_tiles[] = {
    {"Square rock", SectionKind::foreground, sb1_rock_tech_0_values, sb1_rock_tech_0_brushes,
     "src/decdesign.cpp:129-134; src/decblock.cpp:120-122"},
    {"Triangular rock", SectionKind::foreground, sb1_rock_tech_1_values, sb1_rock_tech_1_brushes,
     "src/decdesign.cpp:135-140; src/decblock.cpp:124-126"},
    {"Secret rock", SectionKind::foreground, sb1_rock_tech_2_values, {},
     "src/decdesign.cpp:141-146; src/decblock.cpp:128-130"},
    {"Square tech blocks", SectionKind::foreground, sb1_rock_tech_3_values, {},
     "src/decdesign.cpp:147-152; src/decblock.cpp:90-93"},
    {"Special tech blocks", SectionKind::foreground, sb1_rock_tech_4_values, {},
     "src/decdesign.cpp:153-158; src/decblock.cpp:95-98"},
    {"Triangular tech blocks", SectionKind::foreground, sb1_rock_tech_5_values, {},
     "src/decdesign.cpp:159-164; src/decblock.cpp:100-102"},
    {"Secret tech block", SectionKind::foreground, sb1_rock_tech_6_values, {},
     "src/decdesign.cpp:165-170; src/decblock.cpp:104-106"},
    {"Mechanical blocks", SectionKind::foreground, sb1_rock_tech_7_values, {},
     "src/decdesign.cpp:171-176; src/decblock.cpp:108-110"},
    {"Special mechanical blocks", SectionKind::foreground, sb1_rock_tech_8_values, {},
     "src/decdesign.cpp:177-182; src/decblock.cpp:112-114"},
    {"Secret mechanical blocks", SectionKind::foreground, sb1_rock_tech_9_values, {},
     "src/decdesign.cpp:183-188; src/decblock.cpp:116-118"},
};

constexpr std::int16_t sb1_buildings_0_values[] = {139, 140, 141, 142, 143};
constexpr std::int16_t sb1_buildings_1_values[] = {41, 42, 43, 44, 45, 46, 47};
constexpr std::int16_t sb1_buildings_2_values[] = {48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67};
constexpr std::int16_t sb1_buildings_3_values[] = {215, 216, 217, 218, 219, 220, 221, 222, 230, 231, 233, 234};
constexpr std::int16_t sb1_buildings_4_values[] = {223, 224, 225, 226, 227, 228, 229, 232};
constexpr std::int16_t sb1_buildings_5_values[] = {186, 187, 188, 189, 190, 191, 192};
constexpr std::int16_t sb1_buildings_6_values[] = {193, 194, 195, 196, 197};
constexpr std::int16_t sb1_buildings_7_values[] = {157};
constexpr std::string_view sb1_buildings_7_brushes[] = {"grass"};
constexpr std::int16_t sb1_buildings_8_values[] = {76, 77, 199, 198, 200, 201, 235, 236};
constexpr std::int16_t sb1_buildings_9_values[] = {214};
constexpr TileEntry sb1_buildings_tiles[] = {
    {"Furniture", SectionKind::foreground, sb1_buildings_0_values, {},
     "src/decdesign.cpp:200-205; src/decblock.cpp:155-157"},
    {"Wooden wall", SectionKind::foreground, sb1_buildings_1_values, {},
     "src/decdesign.cpp:206-211; src/decblock.cpp:132-134"},
    {"Dream", SectionKind::foreground, sb1_buildings_2_values, {},
     "src/decdesign.cpp:212-217; src/decblock.cpp:136-140"},
    {"Toy blocks", SectionKind::foreground, sb1_buildings_3_values, {},
     "src/decdesign.cpp:218-223; src/decblock.cpp:142-145"},
    {"Special toy blocks", SectionKind::foreground, sb1_buildings_4_values, {},
     "src/decdesign.cpp:224-229; src/decblock.cpp:147-149"},
    {"Building facade", SectionKind::foreground, sb1_buildings_5_values, {},
     "src/decdesign.cpp:230-235; src/decblock.cpp:159-161"},
    {"Roof", SectionKind::foreground, sb1_buildings_6_values, {},
     "src/decdesign.cpp:236-241; src/decblock.cpp:163-165"},
    {"Grass", SectionKind::foreground, sb1_buildings_7_values, sb1_buildings_7_brushes,
     "src/decdesign.cpp:242-247; src/decblock.cpp:151-153"},
    {"Support", SectionKind::foreground, sb1_buildings_8_values, {},
     "src/decdesign.cpp:248-253; src/decblock.cpp:183-185"},
    {"Invisible block", SectionKind::foreground, sb1_buildings_9_values, {},
     "src/decdesign.cpp:254"},
};

constexpr std::string_view sb1_lifts_enemies_presets[] = {"lift-slow", "lift-medium", "lift-fast", "lift-fast-out", "crate", "crate-disguised", "bulldozer", "fish", "bird", "moving-bomb", "enemy-helicopter", "enemy-tank"};

constexpr std::string_view sb1_bonuses_presets[] = {"treasure", "egg", "power", "shield", "drink", "bullets", "charger", "goal"};

constexpr std::int16_t sb1_hazards_vehicles_2_values[] = {68};
constexpr std::int16_t sb1_hazards_vehicles_3_values[] = {91, 92};
constexpr std::int16_t sb1_hazards_vehicles_4_values[] = {126, 129, 132, 135};
constexpr std::int16_t sb1_hazards_vehicles_5_values[] = {110, 114, 118, 122};
constexpr std::int16_t sb1_hazards_vehicles_6_values[] = {138};
constexpr std::string_view sb1_hazards_vehicles_6_brushes[] = {"bar"};
constexpr std::int16_t sb1_hazards_vehicles_7_values[] = {211};
constexpr TileEntry sb1_hazards_vehicles_tiles[] = {
    {"Lava", SectionKind::foreground, sb1_hazards_vehicles_2_values, {},
     "src/decdesign.cpp:860-865; src/decblock.cpp:167-169"},
    {"Water", SectionKind::foreground, sb1_hazards_vehicles_3_values, {},
     "src/decdesign.cpp:867-872; src/decblock.cpp:171-173"},
    {"Fan", SectionKind::foreground, sb1_hazards_vehicles_4_values, {},
     "src/decdesign.cpp:874-879; src/decblock.cpp:175-177"},
    {"Wind", SectionKind::foreground, sb1_hazards_vehicles_5_values, {},
     "src/decdesign.cpp:881-886; src/decblock.cpp:179-181"},
    {"Bar to hang from", SectionKind::foreground, sb1_hazards_vehicles_6_values, sb1_hazards_vehicles_6_brushes,
     "src/decdesign.cpp:887"},
    {"Spring", SectionKind::foreground, sb1_hazards_vehicles_7_values, {},
     "src/decdesign.cpp:888"},
};
constexpr std::string_view sb1_hazards_vehicles_presets[] = {"bomb-floor", "bomb-ceiling", "personal-bomb-1", "personal-bomb-2", "personal-bomb-3", "personal-bomb-4", "helicopter", "skateboard", "jeep", "tank"};

constexpr std::int16_t sb1_doors_keys_0_values[] = {182, 183};
constexpr std::int16_t sb1_doors_keys_1_values[] = {158, 159, 160, 161, 162, 163, 164, 165, 174, 175, 176, 177, 178, 179, 180, 181, 184};
constexpr TileEntry sb1_doors_keys_tiles[] = {
    {"Door", SectionKind::foreground, sb1_doors_keys_0_values, {},
     "src/decdesign.cpp:1017-1022; src/decblock.cpp:204-206"},
    {"Mission doors", SectionKind::foreground, sb1_doors_keys_1_values, {},
     "src/decdesign.cpp:1024-1029; src/decblock.cpp:199-202"},
};
constexpr std::string_view sb1_doors_keys_presets[] = {"key"};

constexpr EditorGroup firstGameGroups[] = {
    {"scenery", "Scenery", sb1_scenery_tiles, {},
     "src/decdesign.cpp:76-120; src/event.cpp:400"},
    {"rock-tech", "Rock and tech blocks", sb1_rock_tech_tiles, {},
     "src/decdesign.cpp:122-191; src/event.cpp:401"},
    {"buildings", "Buildings and decor", sb1_buildings_tiles, {},
     "src/decdesign.cpp:193-257; src/event.cpp:402"},
    {"lifts-enemies", "Lifts, cases and enemies", {}, sb1_lifts_enemies_presets,
     "src/decdesign.cpp:259-606; src/event.cpp:403"},
    {"bonuses", "Bonuses and goal", {}, sb1_bonuses_presets,
     "src/decdesign.cpp:608-810; src/event.cpp:404"},
    {"hazards-vehicles", "Bombs, hazards and vehicles", sb1_hazards_vehicles_tiles, sb1_hazards_vehicles_presets,
     "src/decdesign.cpp:812-1009; src/event.cpp:405"},
    {"doors-keys", "Doors and keys", sb1_doors_keys_tiles, sb1_doors_keys_presets,
     "src/decdesign.cpp:1011-1056; src/event.cpp:406"},
};

constexpr std::int16_t sb2_scenery_0_values[] = {20, 21, 22};
constexpr std::int16_t sb2_scenery_1_values[] = {16, 17, 18, 19};
constexpr std::int16_t sb2_scenery_2_values[] = {23, 24, 25};
constexpr std::int16_t sb2_scenery_3_values[] = {203};
constexpr std::int16_t sb2_scenery_4_values[] = {26, 27};
constexpr std::int16_t sb2_scenery_5_values[] = {28, 29, 30, 31};
constexpr std::int16_t sb2_scenery_6_values[] = {45, 46, 47, 36, 37, 38, 39, 40, 41, 42, 43, 44};
constexpr std::int16_t sb2_scenery_7_values[] = {66, 67, 68, 69};
constexpr std::int16_t sb2_scenery_8_values[] = {87, 88, 89};
constexpr std::int16_t sb2_scenery_9_values[] = {76, 77, 199, 198, 200, 201, 235, 236, 375, 376, 377};
constexpr std::int16_t sb2_scenery_10_values[] = {403, 401, 402};
constexpr TileEntry sb2_scenery_tiles[] = {
    {"Tree", SectionKind::bigDecor, sb2_scenery_0_values, {},
     "games/sb2/src/decdesign.cpp:104-109; games/sb2/src/decblock.cpp:240"},
    {"Palm tree", SectionKind::bigDecor, sb2_scenery_1_values, {},
     "games/sb2/src/decdesign.cpp:110-115; games/sb2/src/decblock.cpp:242"},
    {"Decorative plant", SectionKind::bigDecor, sb2_scenery_2_values, {},
     "games/sb2/src/decdesign.cpp:116-121; games/sb2/src/decblock.cpp:244"},
    {"Marine plant", SectionKind::bigDecor, sb2_scenery_3_values, {},
     "games/sb2/src/decdesign.cpp:122"},
    {"House", SectionKind::bigDecor, sb2_scenery_4_values, {},
     "games/sb2/src/decdesign.cpp:123-128; games/sb2/src/decblock.cpp:246"},
    {"Mechanical background", SectionKind::bigDecor, sb2_scenery_5_values, {},
     "games/sb2/src/decdesign.cpp:129-134; games/sb2/src/decblock.cpp:248"},
    {"Toy background", SectionKind::bigDecor, sb2_scenery_6_values, {},
     "games/sb2/src/decdesign.cpp:135-140; games/sb2/src/decblock.cpp:250"},
    {"Green slime", SectionKind::bigDecor, sb2_scenery_7_values, {},
     "games/sb2/src/decdesign.cpp:141-146; games/sb2/src/decblock.cpp:252"},
    {"Palace elements", SectionKind::bigDecor, sb2_scenery_8_values, {},
     "games/sb2/src/decdesign.cpp:147-152; games/sb2/src/decblock.cpp:177"},
    {"Support", SectionKind::foreground, sb2_scenery_9_values, {},
     "games/sb2/src/decdesign.cpp:153-158; games/sb2/src/decblock.cpp:236"},
    {"Spider's web", SectionKind::foreground, sb2_scenery_10_values, {},
     "games/sb2/src/decdesign.cpp:159-164; games/sb2/src/decblock.cpp:238"},
};

constexpr std::int16_t sb2_tech_0_values[] = {10, 2, 3, 4, 5, 6, 7, 8, 9, 1, 11, 12, 13, 14};
constexpr std::int16_t sb2_tech_1_values[] = {19, 20, 21, 23, 24, 25, 26, 27, 28, 29};
constexpr std::int16_t sb2_tech_2_values[] = {15, 16, 17, 18};
constexpr std::int16_t sb2_tech_3_values[] = {73, 22};
constexpr std::int16_t sb2_tech_4_values[] = {78, 79, 80, 81, 82, 83, 84};
constexpr std::int16_t sb2_tech_5_values[] = {87, 88, 89, 90};
constexpr std::int16_t sb2_tech_6_values[] = {85, 86};
constexpr std::int16_t sb2_tech_7_values[] = {250};
constexpr std::string_view sb2_tech_7_brushes[] = {"pipes"};
constexpr TileEntry sb2_tech_tiles[] = {
    {"Square tech blocks", SectionKind::foreground, sb2_tech_0_values, {},
     "games/sb2/src/decdesign.cpp:174-179; games/sb2/src/decblock.cpp:143-146"},
    {"Special tech blocks", SectionKind::foreground, sb2_tech_1_values, {},
     "games/sb2/src/decdesign.cpp:180-185; games/sb2/src/decblock.cpp:148-151"},
    {"Triangular tech blocks", SectionKind::foreground, sb2_tech_2_values, {},
     "games/sb2/src/decdesign.cpp:186-191; games/sb2/src/decblock.cpp:153-155"},
    {"Secret tech block", SectionKind::foreground, sb2_tech_3_values, {},
     "games/sb2/src/decdesign.cpp:192-197; games/sb2/src/decblock.cpp:157-159"},
    {"Mechanical blocks", SectionKind::foreground, sb2_tech_4_values, {},
     "games/sb2/src/decdesign.cpp:198-203; games/sb2/src/decblock.cpp:161-163"},
    {"Special mechanical blocks", SectionKind::foreground, sb2_tech_5_values, {},
     "games/sb2/src/decdesign.cpp:204-209; games/sb2/src/decblock.cpp:165-167"},
    {"Secret mechanical blocks", SectionKind::foreground, sb2_tech_6_values, {},
     "games/sb2/src/decdesign.cpp:210-215; games/sb2/src/decblock.cpp:169-171"},
    {"Pipes", SectionKind::foreground, sb2_tech_7_values, sb2_tech_7_brushes,
     "games/sb2/src/decdesign.cpp:216"},
};

constexpr std::int16_t sb2_rock_cave_0_values[] = {153};
constexpr std::string_view sb2_rock_cave_0_brushes[] = {"rock"};
constexpr std::int16_t sb2_rock_cave_1_values[] = {154, 155};
constexpr std::string_view sb2_rock_cave_1_brushes[] = {"triangular-rock", "triangular-rock"};
constexpr std::int16_t sb2_rock_cave_2_values[] = {185};
constexpr std::int16_t sb2_rock_cave_3_values[] = {284, 301};
constexpr std::string_view sb2_rock_cave_3_brushes[] = {"cave-284", "cave-301"};
constexpr std::int16_t sb2_rock_cave_4_values[] = {337};
constexpr std::string_view sb2_rock_cave_4_brushes[] = {"cave-337"};
constexpr std::int16_t sb2_rock_cave_5_values[] = {246, 247, 248, 249};
constexpr std::string_view sb2_rock_cave_5_brushes[] = {"cheese-246", "cheese-247", "cheese-248", "cheese-249"};
constexpr std::int16_t sb2_rock_cave_6_values[] = {339};
constexpr std::string_view sb2_rock_cave_6_brushes[] = {"cheese-339"};
constexpr std::int16_t sb2_rock_cave_7_values[] = {341};
constexpr std::string_view sb2_rock_cave_7_brushes[] = {"slime"};
constexpr std::int16_t sb2_rock_cave_8_values[] = {157};
constexpr std::string_view sb2_rock_cave_8_brushes[] = {"grass"};
constexpr std::int16_t sb2_rock_cave_9_values[] = {91, 92};
constexpr TileEntry sb2_rock_cave_tiles[] = {
    {"Square rock", SectionKind::foreground, sb2_rock_cave_0_values, sb2_rock_cave_0_brushes,
     "games/sb2/src/decdesign.cpp:227-232; games/sb2/src/decblock.cpp:179-181"},
    {"Triangular rock", SectionKind::foreground, sb2_rock_cave_1_values, sb2_rock_cave_1_brushes,
     "games/sb2/src/decdesign.cpp:233-238; games/sb2/src/decblock.cpp:183-185"},
    {"Secret rock", SectionKind::foreground, sb2_rock_cave_2_values, {},
     "games/sb2/src/decdesign.cpp:239-244; games/sb2/src/decblock.cpp:187-189"},
    {"Cave", SectionKind::foreground, sb2_rock_cave_3_values, sb2_rock_cave_3_brushes,
     "games/sb2/src/decdesign.cpp:245"},
    {"Secret cave", SectionKind::foreground, sb2_rock_cave_4_values, sb2_rock_cave_4_brushes,
     "games/sb2/src/decdesign.cpp:246"},
    {"Cheese", SectionKind::foreground, sb2_rock_cave_5_values, sb2_rock_cave_5_brushes,
     "games/sb2/src/decdesign.cpp:247"},
    {"Secret cheese", SectionKind::foreground, sb2_rock_cave_6_values, sb2_rock_cave_6_brushes,
     "games/sb2/src/decdesign.cpp:248"},
    {"Green slime", SectionKind::foreground, sb2_rock_cave_7_values, sb2_rock_cave_7_brushes,
     "games/sb2/src/decdesign.cpp:249"},
    {"Grass", SectionKind::foreground, sb2_rock_cave_8_values, sb2_rock_cave_8_brushes,
     "games/sb2/src/decdesign.cpp:250-255; games/sb2/src/decblock.cpp:204-206"},
    {"Water", SectionKind::foreground, sb2_rock_cave_9_values, {},
     "games/sb2/src/decdesign.cpp:256-261; games/sb2/src/decblock.cpp:224-226"},
};

constexpr std::int16_t sb2_palace_buildings_0_values[] = {396, 388, 389, 390, 391};
constexpr std::string_view sb2_palace_buildings_0_brushes[] = {"palace", "palace", "palace", "palace", "palace"};
constexpr std::int16_t sb2_palace_buildings_1_values[] = {398, 399, 400};
constexpr std::int16_t sb2_palace_buildings_2_values[] = {186, 187, 188, 189, 190, 191, 192};
constexpr std::int16_t sb2_palace_buildings_3_values[] = {193, 194, 195, 196, 197};
constexpr std::int16_t sb2_palace_buildings_4_values[] = {261};
constexpr std::string_view sb2_palace_buildings_4_brushes[] = {"bricks"};
constexpr std::int16_t sb2_palace_buildings_5_values[] = {139, 140, 141, 142, 143};
constexpr std::int16_t sb2_palace_buildings_6_values[] = {41, 42, 43, 44, 45, 46, 47};
constexpr std::int16_t sb2_palace_buildings_7_values[] = {215, 216, 217, 218, 219, 220, 221, 222, 230, 231, 233, 234};
constexpr std::int16_t sb2_palace_buildings_8_values[] = {223, 224, 225, 226, 227, 228, 229, 232};
constexpr std::int16_t sb2_palace_buildings_9_values[] = {214};
constexpr TileEntry sb2_palace_buildings_tiles[] = {
    {"Palace", SectionKind::foreground, sb2_palace_buildings_0_values, sb2_palace_buildings_0_brushes,
     "games/sb2/src/decdesign.cpp:272-278; games/sb2/src/decblock.cpp:173"},
    {"Palace elements", SectionKind::foreground, sb2_palace_buildings_1_values, {},
     "games/sb2/src/decdesign.cpp:279-284; games/sb2/src/decblock.cpp:175"},
    {"Building facade", SectionKind::foreground, sb2_palace_buildings_2_values, {},
     "games/sb2/src/decdesign.cpp:285-290; games/sb2/src/decblock.cpp:300"},
    {"Roof", SectionKind::foreground, sb2_palace_buildings_3_values, {},
     "games/sb2/src/decdesign.cpp:291-296; games/sb2/src/decblock.cpp:216-218"},
    {"Bricks", SectionKind::foreground, sb2_palace_buildings_4_values, sb2_palace_buildings_4_brushes,
     "games/sb2/src/decdesign.cpp:297"},
    {"Furniture", SectionKind::foreground, sb2_palace_buildings_5_values, {},
     "games/sb2/src/decdesign.cpp:298-303; games/sb2/src/decblock.cpp:298"},
    {"Wooden wall", SectionKind::foreground, sb2_palace_buildings_6_values, {},
     "games/sb2/src/decdesign.cpp:304-309; games/sb2/src/decblock.cpp:191-193"},
    {"Toy blocks", SectionKind::foreground, sb2_palace_buildings_7_values, {},
     "games/sb2/src/decdesign.cpp:310-315; games/sb2/src/decblock.cpp:195-198"},
    {"Special toy blocks", SectionKind::foreground, sb2_palace_buildings_8_values, {},
     "games/sb2/src/decdesign.cpp:316-321; games/sb2/src/decblock.cpp:200-202"},
    {"Invisible block", SectionKind::foreground, sb2_palace_buildings_9_values, {},
     "games/sb2/src/decdesign.cpp:322"},
};

constexpr std::int16_t sb2_bombs_traps_3_values[] = {68};
constexpr std::int16_t sb2_bombs_traps_4_values[] = {126, 129, 132, 135};
constexpr std::int16_t sb2_bombs_traps_5_values[] = {110, 114, 118, 122};
constexpr std::int16_t sb2_bombs_traps_6_values[] = {373};
constexpr std::int16_t sb2_bombs_traps_7_values[] = {404};
constexpr std::int16_t sb2_bombs_traps_8_values[] = {378};
constexpr std::int16_t sb2_bombs_traps_switch[] = {384};
constexpr std::int16_t sb2_bombs_traps_10_values[] = {304};
constexpr std::string_view sb2_bombs_traps_10_brushes[] = {"lightning"};
constexpr std::int16_t sb2_bombs_traps_11_values[] = {317};
constexpr TileEntry sb2_bombs_traps_tiles[] = {
    {"Lava", SectionKind::foreground, sb2_bombs_traps_3_values, {},
     "games/sb2/src/decdesign.cpp:393-398; games/sb2/src/decblock.cpp:220-222"},
    {"Fan", SectionKind::foreground, sb2_bombs_traps_4_values, {},
     "games/sb2/src/decdesign.cpp:399-404; games/sb2/src/decblock.cpp:228-230"},
    {"Wind", SectionKind::foreground, sb2_bombs_traps_5_values, {},
     "games/sb2/src/decdesign.cpp:405-410; games/sb2/src/decblock.cpp:232-234"},
    {"Slime trap", SectionKind::foreground, sb2_bombs_traps_6_values, {},
     "games/sb2/src/decdesign.cpp:411"},
    {"Slime trap", SectionKind::foreground, sb2_bombs_traps_7_values, {},
     "games/sb2/src/decdesign.cpp:412"},
    {"Circular saw", SectionKind::foreground, sb2_bombs_traps_8_values, {},
     "games/sb2/src/decdesign.cpp:413-418; games/sb2/src/decblock.cpp:264"},
    {"Floor switch", SectionKind::foreground, sb2_bombs_traps_switch, {},
     "games/sb2/src/decblock.cpp:264 (second tile on the saw button)"},
    {"Lightning", SectionKind::foreground, sb2_bombs_traps_10_values, sb2_bombs_traps_10_brushes,
     "games/sb2/src/decdesign.cpp:439"},
    {"Crusher", SectionKind::foreground, sb2_bombs_traps_11_values, {},
     "games/sb2/src/decdesign.cpp:440"},
};
constexpr std::string_view sb2_bombs_traps_presets[] = {"bomb-floor", "bomb-ceiling", "homing-bomb", "inverter"};

constexpr std::string_view sb2_lifts_creatures_presets[] = {"lift-slow", "lift-medium", "lift-fast", "lift-fast-out", "conveyor-lift-a", "conveyor-lift-b", "bulldozer", "fish", "bird", "wasp", "creature", "moving-bomb", "enemy-helicopter", "enemy-tank"};

constexpr std::string_view sb2_bonuses_presets[] = {"treasure", "egg", "power", "shield", "drink", "bullets", "charger", "personal-bomb-1", "personal-bomb-2", "personal-bomb-3", "personal-bomb-4", "dynamite"};

constexpr std::int16_t sb2_devices_1_values[] = {334, 335, 336};
constexpr std::int16_t sb2_devices_2_values[] = {421, 422, 423, 424, 425, 426, 427, 428, 429, 430, 431, 432, 433, 434, 435, 436, 437, 438, 439, 440};
constexpr std::int16_t sb2_devices_3_values[] = {330, 331, 332, 333};
constexpr std::int16_t sb2_devices_4_values[] = {138};
constexpr std::string_view sb2_devices_4_brushes[] = {"bar"};
constexpr std::int16_t sb2_devices_5_values[] = {211};
constexpr std::int16_t sb2_devices_6_values[] = {324};
constexpr std::int16_t sb2_devices_7_values[] = {364};
constexpr TileEntry sb2_devices_tiles[] = {
    {"Door", SectionKind::foreground, sb2_devices_1_values, {},
     "games/sb2/src/decdesign.cpp:1024-1029; games/sb2/src/decblock.cpp:260"},
    {"Door closed until all chests are found", SectionKind::foreground, sb2_devices_2_values, {},
     "games/sb2/src/decdesign.cpp:1030-1035; games/sb2/src/decblock.cpp:262"},
    {"Teleporter", SectionKind::foreground, sb2_devices_3_values, {},
     "games/sb2/src/decdesign.cpp:1036-1041; games/sb2/src/decblock.cpp:308"},
    {"Bar to hang from", SectionKind::foreground, sb2_devices_4_values, sb2_devices_4_brushes,
     "games/sb2/src/decdesign.cpp:1042"},
    {"Spring", SectionKind::foreground, sb2_devices_5_values, {},
     "games/sb2/src/decdesign.cpp:1043"},
    {"Vanishing block", SectionKind::foreground, sb2_devices_6_values, {},
     "games/sb2/src/decdesign.cpp:1044"},
    {"Fragile bridge", SectionKind::foreground, sb2_devices_7_values, {},
     "games/sb2/src/decdesign.cpp:1045"},
};
constexpr std::string_view sb2_devices_presets[] = {"key-colour-1", "key-colour-2", "key-colour-3", "crate", "crate-disguised"};

constexpr std::string_view sb2_vehicles_presets[] = {"hovercraft", "helicopter", "skateboard", "jeep", "tank"};

constexpr std::int16_t sb2_goal_missions_5_values[] = {182, 183};
constexpr std::int16_t sb2_goal_missions_6_values[] = {158, 159, 160, 161, 162, 163, 164, 165, 309, 411, 412, 413, 414, 415, 174, 175, 176, 177, 178, 179, 180, 181, 184};
constexpr TileEntry sb2_goal_missions_tiles[] = {
    {"Door", SectionKind::foreground, sb2_goal_missions_5_values, {},
     "games/sb2/src/decdesign.cpp:1281-1286; games/sb2/src/decblock.cpp:256-258"},
    {"Mission doors", SectionKind::foreground, sb2_goal_missions_6_values, {},
     "games/sb2/src/decdesign.cpp:1287-1292; games/sb2/src/decblock.cpp:254"},
};
constexpr std::string_view sb2_goal_missions_presets[] = {"goal", "key"};

constexpr EditorGroup secondGameGroups[] = {
    {"scenery", "Scenery", sb2_scenery_tiles, {},
     "games/sb2/src/decdesign.cpp:99-165; games/sb2/src/event.cpp:429"},
    {"tech", "Tech blocks and pipes", sb2_tech_tiles, {},
     "games/sb2/src/decdesign.cpp:168-218; games/sb2/src/event.cpp:430"},
    {"rock-cave", "Rock, cave and cheese", sb2_rock_cave_tiles, {},
     "games/sb2/src/decdesign.cpp:221-263; games/sb2/src/event.cpp:431"},
    {"palace-buildings", "Palace and buildings", sb2_palace_buildings_tiles, {},
     "games/sb2/src/decdesign.cpp:266-324; games/sb2/src/event.cpp:432"},
    {"bombs-traps", "Bombs and traps", sb2_bombs_traps_tiles, sb2_bombs_traps_presets,
     "games/sb2/src/decdesign.cpp:327-442; games/sb2/src/event.cpp:433"},
    {"lifts-creatures", "Lifts and creatures", {}, sb2_lifts_creatures_presets,
     "games/sb2/src/decdesign.cpp:445-787; games/sb2/src/event.cpp:434"},
    {"bonuses", "Bonuses", {}, sb2_bonuses_presets,
     "games/sb2/src/decdesign.cpp:790-991; games/sb2/src/event.cpp:435"},
    {"devices", "Keys, doors and devices", sb2_devices_tiles, sb2_devices_presets,
     "games/sb2/src/decdesign.cpp:994-1115; games/sb2/src/event.cpp:436"},
    {"vehicles", "Vehicles", {}, sb2_vehicles_presets,
     "games/sb2/src/decdesign.cpp:1118-1225; games/sb2/src/event.cpp:437"},
    {"goal-missions", "Goal and mission doors", sb2_goal_missions_tiles, sb2_goal_missions_presets,
     "games/sb2/src/decdesign.cpp:1228-1318; games/sb2/src/event.cpp:438"},
};

} // namespace

std::span<const EditorGroup> editorGroups(GameProfile profile) {
    switch (profile) {
    case GameProfile::speedyBlupi1:
        return firstGameGroups;
    case GameProfile::speedyBlupi2:
    case GameProfile::mobileEggbert:
        return secondGameGroups;
    case GameProfile::unknown:
        break;
    }
    return {};
}

} // namespace eggbert
