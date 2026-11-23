// SPDX-License-Identifier: GPL-3.0-only
#include "world/campaign_doors.hpp"

#include <set>

namespace eggbert {

namespace {

int exitCode(GameProfile profile) {
    return profile == GameProfile::speedyBlupi2 ? 199 : 99;
}

bool campaignWorld(GameProfile profile, int number) {
    const auto role = officialWorldRole(profile, number);
    return role == WorldRole::start || role == WorldRole::hub || role == WorldRole::mission;
}

} // namespace

int doorCode(GameProfile profile, int tile) {
    if (profile == GameProfile::unknown) {
        return -1;
    }
    if (tile >= 158 && tile <= 165) {
        return tile - 157;
    }
    if (tile >= 166 && tile <= 173) {
        return tile - 165;
    }
    if (tile >= 174 && tile <= 181) {
        return tile - 173;
    }
    if (tile == 184) {
        return exitCode(profile);
    }
    if (profile == GameProfile::speedyBlupi2) {
        if (tile == 309 || tile == 310) {
            return 9;
        }
        if (tile >= 411 && tile <= 415) {
            return tile - 401;
        }
        if (tile >= 416 && tile <= 420) {
            return tile - 406;
        }
    }
    return -1;
}

int doorTarget(GameProfile profile, int number, int code) {
    if (number == 1) {
        return code * 10;
    }
    if (code == exitCode(profile)) {
        return 1;
    }
    return number / 10 * 10 + code;
}

std::vector<DoorTile> doorTiles(const WorldView &world, GameProfile profile) {
    std::vector<DoorTile> doors;
    const auto foreground = world.foreground();
    if (!foreground) {
        return doors;
    }
    for (std::size_t x = 0; x < layout::mapWidth; ++x) {
        for (std::size_t y = 0; y < layout::mapHeight; ++y) {
            const auto tile = *foreground->cell(x, y);
            if (const auto code = doorCode(profile, tile); code != -1) {
                doors.push_back({{x, y}, tile, code});
            }
        }
    }
    return doors;
}

DoorReview reviewDoors(GameProfile profile, const std::map<int, WorldView> &worlds) {
    DoorReview review;
    std::set<int> referenced;
    for (const auto &[number, world] : worlds) {
        if (!campaignWorld(profile, number)) {
            continue;
        }
        for (const auto &door : doorTiles(world, profile)) {
            const auto target = doorTarget(profile, number, door.code);
            referenced.insert(target);
            if (!worlds.contains(target)) {
                review.missingTargets.push_back({number, door.cell, door.tile, target});
            }
        }
    }
    for (const auto &[number, world] : worlds) {
        const auto role = officialWorldRole(profile, number);
        if ((role == WorldRole::hub || role == WorldRole::mission) &&
            !referenced.contains(number)) {
            review.unreferenced.push_back(number);
        }
    }
    return review;
}

} // namespace eggbert
