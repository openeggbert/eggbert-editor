// SPDX-License-Identifier: GPL-3.0-only
#include "world/installation.hpp"

#include <cctype>
#include <cstdio>

namespace eggbert {

namespace {

std::string lower(std::string_view text) {
    std::string result(text);
    for (auto &character : result) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return result;
}

// Exactly three digits at `position`, as "%.3d" writes numbers below 1000.
std::optional<int> threeDigits(std::string_view text, std::size_t position) {
    if (position + 3 > text.size()) {
        return std::nullopt;
    }
    int value = 0;
    for (std::size_t index = position; index < position + 3; ++index) {
        if (text[index] < '0' || text[index] > '9') {
            return std::nullopt;
        }
        value = value * 10 + (text[index] - '0');
    }
    return value;
}

// prefix + NNN + ".blp"
std::optional<int> numbered(std::string_view name, std::string_view prefix) {
    if (name.size() != prefix.size() + 7 || !name.starts_with(prefix) ||
        !name.ends_with(".blp")) {
        return std::nullopt;
    }
    return threeDigits(name, prefix.size());
}

// letter + GGG + "-" + NNN + ".blp"
std::optional<std::pair<int, int>> paired(std::string_view name, char letter) {
    if (name.size() != 12 || name[0] != letter || name[4] != '-' || !name.ends_with(".blp")) {
        return std::nullopt;
    }
    const auto gamer = threeDigits(name, 1);
    const auto number = threeDigits(name, 5);
    if (!gamer || !number) {
        return std::nullopt;
    }
    return std::pair{*gamer, *number};
}

} // namespace

DataFileName classifyDataFileName(std::string_view name) {
    const auto text = lower(name);
    if (const auto number = numbered(text, "world")) {
        return {DataFileKind::officialWorld, 0, *number};
    }
    if (const auto pair = paired(text, 'u')) {
        return {DataFileKind::userWorld, pair->first, pair->second};
    }
    if (const auto pair = paired(text, 's')) {
        return {DataFileKind::savedGame, pair->first, pair->second};
    }
    if (const auto number = numbered(text, "info")) {
        return {DataFileKind::gamerInfo, *number, 0};
    }
    if (const auto number = numbered(text, "demo")) {
        return {DataFileKind::demoRecording, 0, *number};
    }
    if (text == "config.def" || text == "time.blp" || text == "bench.blp") {
        return {DataFileKind::gameState, 0, 0};
    }
    return {};
}

std::optional<int> portWorldNumber(std::string_view name) {
    std::string text(name);
    for (auto &character : text) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    if (text.size() != 12 || !text.starts_with("world") || !text.ends_with(".txt")) {
        return std::nullopt;
    }
    int number = 0;
    for (std::size_t index = 5; index < 8; ++index) {
        if (text[index] < '0' || text[index] > '9') {
            return std::nullopt;
        }
        number = number * 10 + (text[index] - '0');
    }
    return number;
}

std::string portWorldFileName(int number) {
    if (number < 0 || number > 999) {
        return {};
    }
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "world%.3d.txt", number);
    return buffer;
}

bool validWorldSlot(const WorldSlot &slot) {
    if (!slot.user) {
        return slot.number >= 1 && slot.number <= 999;
    }
    return slot.gamer >= 1 && slot.gamer <= gamerCount && slot.number >= 1 &&
           slot.number <= userWorldCount;
}

std::string worldSlotFileName(const WorldSlot &slot) {
    if (!validWorldSlot(slot)) {
        return {};
    }
    char buffer[32];
    if (slot.user) {
        std::snprintf(buffer, sizeof buffer, "u%.3d-%.3d.blp", slot.gamer, slot.number);
    } else {
        std::snprintf(buffer, sizeof buffer, "world%.3d.blp", slot.number);
    }
    return buffer;
}

std::optional<WorldSlot> worldSlotOf(const DataFileName &name) {
    WorldSlot slot;
    if (name.kind == DataFileKind::officialWorld) {
        slot = {false, 1, name.number};
    } else if (name.kind == DataFileKind::userWorld) {
        slot = {true, name.gamer, name.number};
    } else {
        return std::nullopt;
    }
    return validWorldSlot(slot) ? std::optional(slot) : std::nullopt;
}

WorldRole officialWorldRole(GameProfile profile, int number) {
    if (profile == GameProfile::unknown || number < 1) {
        return WorldRole::other;
    }
    if (profile == GameProfile::mobileEggbert) {
        if (number == 1) {
            return WorldRole::start;
        }
        if (number == 199) {
            return WorldRole::finale;
        }
        // Door tiles stand for 1 to 14, so hubs end at 140.
        if (number <= 140 && number % 10 == 0) {
            return WorldRole::hub;
        }
        if (number > 10 && number <= 148 && number % 10 <= 8) {
            return WorldRole::mission;
        }
        return WorldRole::other;
    }
    const bool second = profile == GameProfile::speedyBlupi2;
    const int lastHub = second ? 140 : 80;
    if (number == 1) {
        return WorldRole::start;
    }
    if (number == (second ? 199 : 99)) {
        return WorldRole::finale;
    }
    const int multiplayerFirst = second ? 201 : 101;
    const int multiplayerLast = second ? 212 : 110;
    if (number >= multiplayerFirst && number <= multiplayerLast) {
        return WorldRole::multiplayer;
    }
    if (number >= (second ? 300 : 200)) {
        return WorldRole::demo;
    }
    if (number <= lastHub + 8) {
        if (number % 10 == 0) {
            return WorldRole::hub;
        }
        if (number > 10 && number % 10 <= 8) {
            return WorldRole::mission;
        }
    }
    return WorldRole::other;
}

std::string_view worldRoleName(WorldRole role) {
    switch (role) {
    case WorldRole::start:
        return "first world";
    case WorldRole::hub:
        return "group hub";
    case WorldRole::mission:
        return "mission";
    case WorldRole::finale:
        return "final world";
    case WorldRole::multiplayer:
        return "network game";
    case WorldRole::demo:
        return "demo";
    case WorldRole::other:
        return "";
    }
    return "";
}

} // namespace eggbert
