// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include "app/game_assets.hpp"

#include <map>

namespace eggbert {

inline constexpr GameProfile gamesWithArtwork[] = {GameProfile::speedyBlupi1, GameProfile::speedyBlupi2,
                                            GameProfile::mobileEggbert};

class InstallationSession {
public:
    InstallationSession();
    static QString gameRoot(const QString &path);
    std::shared_ptr<const GameAssets> load(const QString &path, GameProfile game, bool select);
    [[nodiscard]] std::shared_ptr<const GameAssets> assets(GameProfile game) const;
    [[nodiscard]] GameProfile selected() const { return selected_; }
    [[nodiscard]] bool accepts(GameProfile document, bool text) const;
    [[nodiscard]] GameProfile profileOfWorld(const QString &path) const;
    void followWorld(GameProfile game);
    [[nodiscard]] bool containsWorld(const QString &path) const;

private:
    std::map<GameProfile, std::shared_ptr<const GameAssets>> directories_;
    bool chosenInWindow_ = false;
    GameProfile selected_ = GameProfile::unknown;
};

} // namespace eggbert
