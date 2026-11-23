// SPDX-License-Identifier: GPL-3.0-only
#include "world/object_catalog.hpp"

#include <algorithm>
#include <array>

namespace eggbert {

namespace {

// Values written by each game's own level editor, read from blupi_recon at
// commit e05e22b990abb782c37a0d9fafa864d86a27e29f (see
// docs/research/evidence.md). Columns: id, name, source identifier, type,
// stepAdvance, stepRecede, timeStopStart, timeStopEnd, offsetX, offsetY,
// channel, icon, hasPath, iconIsChoice, provenance.
constexpr ObjectPreset speedyBlupi1Presets[] = {
    {"lift-slow", "Lift, slow", "TYPE_ASCENSEUR", 1, 20, 20, 20, 20, 0, 0, 1, 153, true, true,
     "src/decdesign.cpp:313-334"},
    {"lift-medium", "Lift, medium", "TYPE_ASCENSEUR", 1, 10, 10, 10, 10, 0, 0, 1, 153, true, true,
     "src/decdesign.cpp:336-357"},
    {"lift-fast", "Lift, fast", "TYPE_ASCENSEUR", 1, 6, 6, 3, 3, 0, 0, 1, 153, true, true,
     "src/decdesign.cpp:359-380"},
    {"lift-fast-out", "Lift, fast out and slow back", "TYPE_ASCENSEUR", 1, 5, 22, 2, 2, 0, 0, 1, 153, true, true,
     "src/decdesign.cpp:382-403"},
    {"crate", "Crate", "TYPE_CAISSE", 12, 1, 1, 0, 0, 0, 0, 1, 32, false, true,
     "src/decdesign.cpp:405-438"},
    {"crate-disguised", "Crate disguised as a tile", "TYPE_CAISSE", 12, 1, 1, 0, 0, 0, 0, 1, 2, false, true,
     "src/decdesign.cpp:440-473"},
    {"bulldozer", "Bulldozer", "TYPE_BULLDOZER", 4, 30, 30, 20, 20, 0, 0, 10, 57, true, false,
     "src/decdesign.cpp:475-495"},
    {"fish", "Fish", "TYPE_POISSON", 17, 15, 15, 47, 47, 0, 0, 10, 79, true, false,
     "src/decdesign.cpp:497-517"},
    {"bird", "Bird", "TYPE_OISEAU", 20, 8, 8, 8, 8, 0, 0, 10, 90, true, false,
     "src/decdesign.cpp:519-539"},
    {"moving-bomb", "Moving bomb", "TYPE_BOMBEMOVE", 16, 10, 10, 0, 0, 0, 0, 10, 69, true, false,
     "src/decdesign.cpp:541-561"},
    {"enemy-helicopter", "Enemy in a helicopter", "TYPE_BLUPIHELICO", 32, 8, 8, 25, 25, 0, 0, 12, 64, true, false,
     "src/decdesign.cpp:563-583"},
    {"enemy-tank", "Enemy in a tank", "TYPE_BLUPITANK", 33, 10, 10, 23, 23, 0, 0, 12, 240, true, false,
     "src/decdesign.cpp:585-605"},
    {"treasure", "Treasure chest", "TYPE_TRESOR", 5, 1, 1, 0, 0, 2, 4, 10, 0, false, false,
     "src/decdesign.cpp:615-634"},
    {"egg", "Egg", "TYPE_EGG", 6, 1, 1, 0, 0, 2, 8, 10, 21, false, false,
     "src/decdesign.cpp:636-655"},
    {"power", "Power bonus", "TYPE_POWER", 26, 1, 1, 0, 0, 2, 8, 10, 136, false, false,
     "src/decdesign.cpp:657-676"},
    {"shield", "Shield", "TYPE_SHIELD", 25, 1, 1, 0, 0, 2, 8, 10, 144, false, false,
     "src/decdesign.cpp:678-697"},
    {"drink", "Drink", "TYPE_DRINK", 30, 1, 1, 0, 0, 2, 6, 10, 178, false, false,
     "src/decdesign.cpp:699-718"},
    {"bullets", "Bullets", "TYPE_BULLET", 29, 1, 1, 0, 0, 2, 6, 10, 177, false, false,
     "src/decdesign.cpp:720-739"},
    {"charger", "Charger", "TYPE_CHARGE", 31, 1, 1, 0, 0, 0, 0, 1, 238, false, false,
     "src/decdesign.cpp:741-760"},
    {"goal", "Goal", "TYPE_GOAL", 7, 1, 1, 0, 0, 2, 4, 10, 29, false, false,
     "src/decdesign.cpp:785-808"},
    {"bomb-floor", "Bomb standing on the floor", "TYPE_BOMBEDOWN", 2, 1, 1, 0, 0, 2, 4, 10, 12, false, false,
     "src/decdesign.cpp:818-837"},
    {"bomb-ceiling", "Bomb hanging from the ceiling", "TYPE_BOMBEUP", 3, 1, 1, 0, 0, 2, 0, 10, 48, false, false,
     "src/decdesign.cpp:839-858"},
    {"personal-bomb-1", "Personal bomb, player 1", "TYPE_BOMBEPERSO1", 200, 1, 1, 0, 0, 2, 6, 2, 257, false, false,
     "src/decdesign.cpp:890-923"},
    {"personal-bomb-2", "Personal bomb, player 2", "TYPE_BOMBEPERSO2", 201, 1, 1, 0, 0, 2, 6, 11, 257, false, false,
     "src/decdesign.cpp:890-923"},
    {"personal-bomb-3", "Personal bomb, player 3", "TYPE_BOMBEPERSO3", 202, 1, 1, 0, 0, 2, 6, 12, 257, false, false,
     "src/decdesign.cpp:890-923"},
    {"personal-bomb-4", "Personal bomb, player 4", "TYPE_BOMBEPERSO4", 203, 1, 1, 0, 0, 2, 6, 13, 257, false, false,
     "src/decdesign.cpp:890-923"},
    {"helicopter", "Helicopter", "TYPE_HELICO", 13, 1, 1, 0, 0, 2, 4, 10, 68, false, false,
     "src/decdesign.cpp:925-944"},
    {"skateboard", "Skateboard", "TYPE_SKATE", 24, 1, 1, 0, 0, 2, 5, 10, 129, false, false,
     "src/decdesign.cpp:946-965"},
    {"jeep", "Jeep", "TYPE_JEEP", 19, 1, 1, 0, 0, 2, 4, 10, 89, false, false,
     "src/decdesign.cpp:967-986"},
    {"tank", "Tank", "TYPE_TANK", 28, 1, 1, 0, 0, 2, 6, 10, 167, false, false,
     "src/decdesign.cpp:988-1007"},
    {"key", "Key", "TYPE_CLE", 21, 1, 1, 0, 0, 2, 4, 10, 125, false, false,
     "src/decdesign.cpp:1031-1054"},
};

constexpr ObjectPreset speedyBlupi2Presets[] = {
    {"bomb-floor", "Bomb standing on the floor", "TYPE_BOMBEDOWN", 2, 1, 1, 0, 0, 2, 4, 10, 12, false, false,
     "games/sb2/src/decdesign.cpp:333-352"},
    {"bomb-ceiling", "Bomb hanging from the ceiling", "TYPE_BOMBEUP", 3, 1, 1, 0, 0, 2, 0, 10, 48, false, false,
     "games/sb2/src/decdesign.cpp:353-372"},
    {"homing-bomb", "Homing bomb", "TYPE_BOMBEFOLLOW1", 96, 1, 1, 0, 0, 0, 0, 10, 256, false, false,
     "games/sb2/src/decdesign.cpp:373-392"},
    {"inverter", "Inverter", "TYPE_INVERT", 40, 1, 1, 0, 0, 2, 8, 10, 187, false, false,
     "games/sb2/src/decdesign.cpp:419-438"},
    {"lift-slow", "Lift, slow", "TYPE_ASCENSEUR", 1, 20, 20, 20, 20, 0, 0, 1, 29, true, true,
     "games/sb2/src/decdesign.cpp:508-529"},
    {"lift-medium", "Lift, medium", "TYPE_ASCENSEUR", 1, 10, 10, 10, 10, 0, 0, 1, 29, true, true,
     "games/sb2/src/decdesign.cpp:530-551"},
    {"lift-fast", "Lift, fast", "TYPE_ASCENSEUR", 1, 6, 6, 3, 3, 0, 0, 1, 29, true, true,
     "games/sb2/src/decdesign.cpp:552-573"},
    {"lift-fast-out", "Lift, fast out and slow back", "TYPE_ASCENSEUR", 1, 5, 22, 2, 2, 0, 0, 1, 29, true, true,
     "games/sb2/src/decdesign.cpp:574-595"},
    {"conveyor-lift-a", "Conveyor lift, carries +2 pixels per tick", "TYPE_ASCENSEURs", 47, 10, 10, 10, 10, 0, 0, 1, 311, true, false,
     "games/sb2/src/decdesign.cpp:596-618"},
    {"conveyor-lift-b", "Conveyor lift, carries -2 pixels per tick", "TYPE_ASCENSEURsi", 48, 10, 10, 10, 10, 0, 0, 1, 311, true, false,
     "games/sb2/src/decdesign.cpp:596-618"},
    {"bulldozer", "Bulldozer", "TYPE_BULLDOZER", 4, 30, 30, 20, 20, 0, 0, 10, 57, true, false,
     "games/sb2/src/decdesign.cpp:619-639"},
    {"fish", "Fish", "TYPE_POISSON", 17, 15, 15, 47, 47, 0, 0, 10, 79, true, false,
     "games/sb2/src/decdesign.cpp:640-660"},
    {"bird", "Bird", "TYPE_OISEAU", 20, 8, 8, 8, 8, 0, 0, 10, 90, true, false,
     "games/sb2/src/decdesign.cpp:661-681"},
    {"wasp", "Wasp", "TYPE_GUEPE", 44, 6, 6, 4, 4, 0, 0, 10, 195, true, false,
     "games/sb2/src/decdesign.cpp:682-702"},
    {"creature", "Creature", "TYPE_CREATURE", 54, 20, 20, 200, 200, 0, 0, 10, 244, true, false,
     "games/sb2/src/decdesign.cpp:703-723"},
    {"moving-bomb", "Moving bomb", "TYPE_BOMBEMOVE", 16, 10, 10, 0, 0, 0, 0, 10, 69, true, false,
     "games/sb2/src/decdesign.cpp:724-744"},
    {"enemy-helicopter", "Enemy in a helicopter", "TYPE_BLUPIHELICO", 32, 8, 8, 25, 25, 0, 0, 12, 64, true, false,
     "games/sb2/src/decdesign.cpp:745-765"},
    {"enemy-tank", "Enemy in a tank", "TYPE_BLUPITANK", 33, 10, 10, 23, 23, 0, 0, 12, 240, true, false,
     "games/sb2/src/decdesign.cpp:766-786"},
    {"treasure", "Treasure chest", "TYPE_TRESOR", 5, 1, 1, 0, 0, 2, 4, 10, 0, false, false,
     "games/sb2/src/decdesign.cpp:796-815"},
    {"egg", "Egg", "TYPE_EGG", 6, 1, 1, 0, 0, 2, 8, 10, 21, false, false,
     "games/sb2/src/decdesign.cpp:816-835"},
    {"power", "Power bonus", "TYPE_POWER", 26, 1, 1, 0, 0, 2, 8, 10, 136, false, false,
     "games/sb2/src/decdesign.cpp:836-855"},
    {"shield", "Shield", "TYPE_SHIELD", 25, 1, 1, 0, 0, 2, 8, 10, 144, false, false,
     "games/sb2/src/decdesign.cpp:856-875"},
    {"drink", "Drink", "TYPE_DRINK", 30, 1, 1, 0, 0, 2, 6, 10, 178, false, false,
     "games/sb2/src/decdesign.cpp:876-895"},
    {"bullets", "Bullets", "TYPE_BULLET", 29, 1, 1, 0, 0, 2, 6, 10, 177, false, false,
     "games/sb2/src/decdesign.cpp:896-915"},
    {"charger", "Charger", "TYPE_CHARGE", 31, 1, 1, 0, 0, 0, 0, 1, 238, false, false,
     "games/sb2/src/decdesign.cpp:916-935"},
    {"personal-bomb-1", "Personal bomb, player 1", "TYPE_BOMBEPERSO1", 200, 1, 1, 0, 0, 2, 6, 2, 257, false, false,
     "games/sb2/src/decdesign.cpp:936-969"},
    {"personal-bomb-2", "Personal bomb, player 2", "TYPE_BOMBEPERSO2", 201, 1, 1, 0, 0, 2, 6, 11, 257, false, false,
     "games/sb2/src/decdesign.cpp:936-969"},
    {"personal-bomb-3", "Personal bomb, player 3", "TYPE_BOMBEPERSO3", 202, 1, 1, 0, 0, 2, 6, 12, 257, false, false,
     "games/sb2/src/decdesign.cpp:936-969"},
    {"personal-bomb-4", "Personal bomb, player 4", "TYPE_BOMBEPERSO4", 203, 1, 1, 0, 0, 2, 6, 13, 257, false, false,
     "games/sb2/src/decdesign.cpp:936-969"},
    {"dynamite", "Dynamite", "TYPE_DYNAMITE", 55, 1, 1, 0, 0, 2, 6, 10, 252, false, false,
     "games/sb2/src/decdesign.cpp:970-989"},
    {"key-colour-1", "Coloured key 1", "TYPE_CLE1", 49, 1, 1, 0, 0, 2, 4, 10, 125, false, false,
     "games/sb2/src/decdesign.cpp:1001-1023"},
    {"key-colour-2", "Coloured key 2", "TYPE_CLE2", 50, 1, 1, 0, 0, 2, 4, 10, 125, false, false,
     "games/sb2/src/decdesign.cpp:1001-1023"},
    {"key-colour-3", "Coloured key 3", "TYPE_CLE3", 51, 1, 1, 0, 0, 2, 4, 10, 125, false, false,
     "games/sb2/src/decdesign.cpp:1001-1023"},
    {"crate", "Crate", "TYPE_CAISSE", 12, 1, 1, 0, 0, 0, 0, 1, 32, false, true,
     "games/sb2/src/decdesign.cpp:1046-1079"},
    {"crate-disguised", "Crate disguised as a tile", "TYPE_CAISSE", 12, 1, 1, 0, 0, 0, 0, 1, 2, false, true,
     "games/sb2/src/decdesign.cpp:1080-1113"},
    {"hovercraft", "Hovercraft", "TYPE_OVER", 46, 1, 1, 0, 0, 2, 4, 10, 208, false, false,
     "games/sb2/src/decdesign.cpp:1124-1143"},
    {"helicopter", "Helicopter", "TYPE_HELICO", 13, 1, 1, 0, 0, 2, 4, 10, 68, false, false,
     "games/sb2/src/decdesign.cpp:1144-1163"},
    {"skateboard", "Skateboard", "TYPE_SKATE", 24, 1, 1, 0, 0, 2, 5, 10, 129, false, false,
     "games/sb2/src/decdesign.cpp:1164-1183"},
    {"jeep", "Jeep", "TYPE_JEEP", 19, 1, 1, 0, 0, 2, 4, 10, 89, false, false,
     "games/sb2/src/decdesign.cpp:1184-1203"},
    {"tank", "Tank", "TYPE_TANK", 28, 1, 1, 0, 0, 2, 6, 10, 167, false, false,
     "games/sb2/src/decdesign.cpp:1204-1223"},
    {"goal", "Goal", "TYPE_GOAL", 7, 1, 1, 0, 0, 2, 4, 10, 29, false, false,
     "games/sb2/src/decdesign.cpp:1235-1258"},
    {"key", "Key", "TYPE_CLE", 21, 1, 1, 0, 0, 2, 4, 10, 125, false, false,
     "games/sb2/src/decdesign.cpp:1293-1316"},
};

// Types the games create only while playing. A stored record of such a type
// is shown but never offered as a preset.
constexpr ObjectTypeInfo speedyBlupi1Runtime[] = {
    {8, "Explosion 1", "TYPE_EXPLO1", ObjectTypeStatus::runtimeOnly},
    {9, "Explosion 2", "TYPE_EXPLO2", ObjectTypeStatus::runtimeOnly},
    {10, "Explosion 3", "TYPE_EXPLO3", ObjectTypeStatus::runtimeOnly},
    {11, "Explosion 4", "TYPE_EXPLO4", ObjectTypeStatus::runtimeOnly},
    {14, "Splash", "TYPE_PLOUF", ObjectTypeStatus::runtimeOnly},
    {15, "Bubble", "TYPE_BLUP", ObjectTypeStatus::runtimeOnly},
    {22, "Door", "TYPE_DOOR", ObjectTypeStatus::runtimeOnly},
    {23, "Projectile", "TYPE_BALLE", ObjectTypeStatus::runtimeOnly},
    {27, "Magic trail", "TYPE_MAGICTRACK", ObjectTypeStatus::runtimeOnly},
    {34, "Glue", "TYPE_GLU", ObjectTypeStatus::runtimeOnly},
    {35, "Small splash", "TYPE_TIPLOUF", ObjectTypeStatus::runtimeOnly},
    {36, "Exhaust", "TYPE_POLLUTION", ObjectTypeStatus::runtimeOnly},
    {37, "Clearing effect", "TYPE_CLEAR", ObjectTypeStatus::runtimeOnly},
    {38, "Electric effect", "TYPE_ELECTRO", ObjectTypeStatus::runtimeOnly},
};

constexpr ObjectTypeInfo speedyBlupi2Runtime[] = {
    {8, "Explosion 1", "TYPE_EXPLO1", ObjectTypeStatus::runtimeOnly},
    {9, "Explosion 2", "TYPE_EXPLO2", ObjectTypeStatus::runtimeOnly},
    {10, "Explosion 3", "TYPE_EXPLO3", ObjectTypeStatus::runtimeOnly},
    {11, "Explosion 4", "TYPE_EXPLO4", ObjectTypeStatus::runtimeOnly},
    {14, "Splash", "TYPE_PLOUF", ObjectTypeStatus::runtimeOnly},
    {15, "Bubble", "TYPE_BLUP", ObjectTypeStatus::runtimeOnly},
    {22, "Door", "TYPE_DOOR", ObjectTypeStatus::runtimeOnly},
    {23, "Projectile", "TYPE_BALLE", ObjectTypeStatus::runtimeOnly},
    {27, "Magic trail", "TYPE_MAGICTRACK", ObjectTypeStatus::runtimeOnly},
    {34, "Glue", "TYPE_GLU", ObjectTypeStatus::runtimeOnly},
    {35, "Small splash", "TYPE_TIPLOUF", ObjectTypeStatus::runtimeOnly},
    {36, "Exhaust", "TYPE_POLLUTION", ObjectTypeStatus::runtimeOnly},
    {37, "Clearing effect", "TYPE_CLEAR", ObjectTypeStatus::runtimeOnly},
    {38, "Electric effect", "TYPE_ELECTRO", ObjectTypeStatus::runtimeOnly},
    {39, "Treasure trail", "TYPE_TRESORTRACK", ObjectTypeStatus::runtimeOnly},
    {41, "Inverter start effect", "TYPE_INVERTSTART", ObjectTypeStatus::runtimeOnly},
    {42, "Inverter stop effect", "TYPE_INVERTSTOP", ObjectTypeStatus::runtimeOnly},
    {43, "Inverter spin effect", "TYPE_INVERTSPIN", ObjectTypeStatus::runtimeOnly},
    {52, "Bridge", "TYPE_BRIDGE", ObjectTypeStatus::runtimeOnly},
    {53, "Tentacle", "TYPE_TENTACULE", ObjectTypeStatus::runtimeOnly},
    {56, "Lit dynamite", "TYPE_DYNAMITEf", ObjectTypeStatus::runtimeOnly},
    {57, "Shield trail", "TYPE_SHIELDTRACK", ObjectTypeStatus::runtimeOnly},
    {90, "Explosion 5", "TYPE_EXPLO5", ObjectTypeStatus::runtimeOnly},
    {91, "Explosion 6", "TYPE_EXPLO6", ObjectTypeStatus::runtimeOnly},
    {92, "Explosion 7", "TYPE_EXPLO7", ObjectTypeStatus::runtimeOnly},
    {93, "Explosion 8", "TYPE_EXPLO8", ObjectTypeStatus::runtimeOnly},
    {97, "Homing bomb, triggered", "TYPE_BOMBEFOLLOW2", ObjectTypeStatus::runtimeOnly},
    {98, "Slime splash 1", "TYPE_SPLOUTCH1", ObjectTypeStatus::runtimeOnly},
    {99, "Slime splash 2", "TYPE_SPLOUTCH2", ObjectTypeStatus::runtimeOnly},
    {100, "Slime splash 3", "TYPE_SPLOUTCH3", ObjectTypeStatus::runtimeOnly},
};

std::span<const ObjectTypeInfo> runtimeTypes(GameProfile profile) {
    switch (profile) {
    case GameProfile::speedyBlupi1:
        return speedyBlupi1Runtime;
    case GameProfile::speedyBlupi2:
    case GameProfile::mobileEggbert:
        return speedyBlupi2Runtime;
    case GameProfile::unknown:
        break;
    }
    return {};
}

} // namespace

std::span<const ObjectPreset> objectPresets(GameProfile profile) {
    switch (profile) {
    case GameProfile::speedyBlupi1:
        return speedyBlupi1Presets;
    case GameProfile::speedyBlupi2:
    case GameProfile::mobileEggbert:
        return speedyBlupi2Presets;
    case GameProfile::unknown:
        break;
    }
    return {};
}

const ObjectPreset *findObjectPreset(GameProfile profile, std::string_view id) {
    for (const auto &preset : objectPresets(profile)) {
        if (preset.id == id) {
            return &preset;
        }
    }
    return nullptr;
}

const ObjectPreset *presetForType(GameProfile profile, std::int16_t type) {
    for (const auto &preset : objectPresets(profile)) {
        if (preset.type == type) {
            return &preset;
        }
    }
    return nullptr;
}

ObjectTypeInfo objectTypeInfo(GameProfile profile, std::int16_t type) {
    if (const auto *preset = presetForType(profile, type)) {
        // Several presets may share a type (lift speeds, crate kinds); the
        // type's own name is the text before the first comma.
        const auto comma = preset->name.find(',');
        return {type, preset->type == 1 || preset->type == 12 ? preset->name.substr(0, comma)
                                                              : preset->name,
                preset->sourceName, ObjectTypeStatus::placeable};
    }
    for (const auto &info : runtimeTypes(profile)) {
        if (info.type == type) {
            return info;
        }
    }
    return {type, {}, {}, ObjectTypeStatus::unknown};
}

} // namespace eggbert
