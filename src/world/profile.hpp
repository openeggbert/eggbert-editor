// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstdint>
#include <string_view>

#include "world/layout.hpp"

namespace eggbert {

// Inclusive range of values a game offers for a field, with where that was
// observed. See docs/research/evidence.md for the full citations.
struct VerifiedRange {
    int minimum = 0;
    int maximum = 0;
    std::string_view provenance;

    [[nodiscard]] constexpr bool contains(int value) const {
        return value >= minimum && value <= maximum;
    }
};

// Game-specific facts. The byte layout is the same for both games; a profile
// only selects catalogs, previews and validation.
struct ProfileInfo {
    GameProfile profile = GameProfile::unknown;
    std::string_view name;
    // header.music: 0 plays nothing, n > 0 plays music file n - 1.
    VerifiedRange music;
    // header.region selects the background picture decorNNN.
    VerifiedRange region;
    // Foreground tile numbers that have collision data in the engine.
    VerifiedRange foregroundTiles;
    // True when the game's loader rewrites stored foreground values 48..67
    // to 215 in memory. The file is never changed by that.
    bool loaderRemaps48To67 = false;
    // Estimate, not a proven limit: with fewer free object slots than this,
    // effects the game creates while playing may find the pool full.
    int lowHeadroomFreeSlots = 0;
    std::string_view headroomRationale;
};

inline constexpr ProfileInfo speedyBlupi1Info{
    GameProfile::speedyBlupi1,
    "Speedy Blupi / Speedy Eggbert 1",
    {0, 10, "blupi_recon e05e22b src/event.cpp:416-426 (11 music buttons), :3088 (message - "
            "WM_BUTTON1)"},
    {0, 23, "blupi_recon e05e22b src/event.cpp:3093: WM_BUTTON1..WM_BUTTON24"},
    {0, 236, "blupi_recon e05e22b src/decor.h:13 MAXQUART 237"},
    false,
    8,
    "Estimate: explosions, splashes, projectiles and dropped vehicles each take one slot while "
    "playing (blupi_recon e05e22b src/decblupi.cpp, src/decmove.cpp); no larger single burst was "
    "found, so room for several simultaneous effects is suggested.",
};

inline constexpr ProfileInfo speedyBlupi2Info{
    GameProfile::speedyBlupi2,
    "Speedy Blupi II / Speedy Eggbert 2",
    {0, 10, "blupi_recon e05e22b games/sb2/src/event.cpp:448-458 (11 music buttons)"},
    {0, 31, "blupi_recon e05e22b games/sb2/src/event.cpp:3679: WM_BUTTON1..WM_BUTTON32"},
    {0, 440, "blupi_recon e05e22b games/sb2/src/decor.h:13 MAXQUART 441"},
    true,
    16,
    "Estimate: one slime splash takes 7 slots at once and one dynamite starts 9 explosions "
    "(blupi_recon e05e22b games/sb2/src/decnet.cpp:20-43, games/sb2/src/decmove.cpp:778-810).",
};

// nullptr for GameProfile::unknown.
[[nodiscard]] constexpr const ProfileInfo *profileInfo(GameProfile profile) {
    switch (profile) {
    case GameProfile::speedyBlupi1:
        return &speedyBlupi1Info;
    case GameProfile::speedyBlupi2:
        return &speedyBlupi2Info;
    case GameProfile::unknown:
        break;
    }
    return nullptr;
}

} // namespace eggbert
