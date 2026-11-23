// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QImage>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QStringList>

#include <array>
#include <map>
#include <memory>
#include <optional>

#include "world/atlas_table.hpp"
#include "world/layout.hpp"

namespace eggbert {

// What a directory looks like, with the evidence shown to the user.
struct ProfileDetection {
    GameProfile profile = GameProfile::unknown;
    QStringList evidence;
};

// Guesses the game from files that exist in only one of them. Reads
// directory listings only.
[[nodiscard]] ProfileDetection detectGameProfile(const QString &directory);

enum class SpriteSheet { tiles, elements, bigDecor, player1, player2, player3, player4 };
inline constexpr std::size_t spriteSheetCount = 7;

// Artwork read from a game directory the user selected. The directory is
// only ever read. Sprite rectangles are not shipped with the editor: they
// are located in the game's own executable and accepted only when they fit
// the sheets found next to it. Anything that cannot be validated is simply
// unavailable and the renderer draws its schematic placeholder instead.
class GameAssets {
public:
    // Largest file read from the directory.
    static constexpr qint64 maximumFileBytes = 64 * 1024 * 1024;

    [[nodiscard]] static std::shared_ptr<const GameAssets> load(const QString &directory,
                                                                GameProfile profile);
    // Mobile Eggbert keeps its pictures as PNG sheets on a regular grid
    // (`Content/icons`) and its backdrops in `Content/backgrounds`. Given
    // the port's folder or its Content folder, the folder that holds
    // `icons`; empty when neither does.
    [[nodiscard]] static QString mobileContentDirectory(const QString &directory);

    [[nodiscard]] const QString &directory() const { return directory_; }
    [[nodiscard]] GameProfile profile() const { return profile_; }
    // True when at least the tile sheet can be drawn.
    [[nodiscard]] bool usable() const;
    // One line per resource: what was found, what was not and why.
    [[nodiscard]] const QStringList &report() const { return report_; }

    struct Sprite {
        const QImage *sheet = nullptr;
        QRect source;
        QPoint offset;
    };
    // Sprite of a stored channel and icon number; nullopt when the channel
    // has no validated sheet or the icon has no (non-empty) entry.
    [[nodiscard]] std::optional<Sprite> sprite(int channel, int icon) const;
    [[nodiscard]] static std::optional<SpriteSheet> sheetForChannel(int channel);

    // The backdrop picture of a region (decorNNN), or null when the
    // directory has none for it. Loaded when first asked for.
    [[nodiscard]] const QImage *backdrop(int region) const;
    // The same picture read without keeping it (for thumbnails); a null
    // image when there is none.
    [[nodiscard]] QImage readBackdrop(int region) const;
    // Path of the music file of a stored music number (1 and up; the games
    // play file n - 1), or an empty string. Only a path: nothing is read.
    [[nodiscard]] QString musicFile(int music) const;
    [[nodiscard]] std::size_t spriteCount(SpriteSheet sheet) const;

private:
    struct Sheet {
        QImage image;
        // Index into tables_; player sheets share one table.
        int table = -1;
    };

    static std::shared_ptr<const GameAssets> loadMobile(const QString &directory);

    QString directory_;
    QString imageDirectory_;
    // Backdrops already looked up; a null image records "not available".
    mutable std::map<int, QImage> backdrops_;
    GameProfile profile_ = GameProfile::unknown;
    std::array<Sheet, spriteSheetCount> sheets_;
    std::array<AtlasTable, 4> tables_; // tiles, elements, bigDecor, players
    std::array<bool, 4> tableFound_{};
    QStringList report_;
};

// Version string recorded in exported pictures drawn with artwork.
inline constexpr const char *artworkRendererVersion = "artwork-1";

} // namespace eggbert
