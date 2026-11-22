// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QWidget>

#include <memory>

#include "world/layout.hpp"

class QAbstractButton;
class QButtonGroup;
class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class QTabWidget;

namespace eggbert {

class GameAssets;

// Visual chooser for what the tools paint and place: the tile numbers of the
// active layer, the chosen game's verified object presets and its adaptive
// border brushes, each with a picture. Pictures come from the game artwork
// when it is loaded and are schematic swatches otherwise, so the panel never
// needs game data. Choosing is reported by signals; the panel itself changes
// nothing in a document.
class PalettePanel final : public QWidget {
    Q_OBJECT

public:
    explicit PalettePanel(QWidget *parent = nullptr);

    // What to offer. `artwork` may be null. Rebuilds only when something
    // changed.
    void setContext(GameProfile profile, SectionKind layer,
                    std::shared_ptr<const GameAssets> artwork);
    // Marks the current choices without emitting signals.
    void setCurrentTile(int value);
    void setCurrentPreset(const QString &id);
    void setCurrentBrush(const QString &id);
    // What the World tab shows as the document's current settings.
    struct WorldState {
        bool available = false; // a document with a complete header
        bool scrollsHorizontally = false;
        bool scrollsVertically = false;
        int region = 0;
        int music = 0;

        friend bool operator==(const WorldState &, const WorldState &) = default;
    };
    void setWorldState(const WorldState &state);
    // Text under the music list and which music number is playing (0: none).
    void setMusicStatus(const QString &text, int playingNumber, bool canPlay);
    [[nodiscard]] bool playWhenChosen() const;

    [[nodiscard]] QListWidget *backdropList() const { return backdrops_; }
    [[nodiscard]] QListWidget *musicList() const { return music_; }
    [[nodiscard]] QAbstractButton *sizeButton(bool horizontally, bool vertically) const;
    [[nodiscard]] QPushButton *playButton() const { return play_; }
    [[nodiscard]] QString musicStatus() const;
    static constexpr int backdropThumbnailWidth = 96;
    static constexpr int backdropThumbnailHeight = 72;

    // Greys the lists out for a document that cannot be edited.
    void setEditable(bool editable);

    // Group strips above the grids: the games' own editor groups plus
    // "all" (and, for tiles, "other") entries. Selecting a group only
    // changes what the grid below shows.
    [[nodiscard]] QListWidget *tileGroupList() const { return tileGroups_; }
    [[nodiscard]] QListWidget *objectGroupList() const { return objectGroups_; }
    // Shows a group by its id; false when there is no such group. The ids
    // of the built-in groups are the constants below.
    bool showTileGroup(const QString &id);
    bool showObjectGroup(const QString &id);
    [[nodiscard]] QString currentTileGroup() const { return tileGroup_; }
    [[nodiscard]] QString currentObjectGroup() const { return objectGroup_; }
    static constexpr const char *allForegroundGroup = "@all-foreground";
    static constexpr const char *otherForegroundGroup = "@other-foreground";
    static constexpr const char *allBigDecorGroup = "@all-bigdecor";
    static constexpr const char *allObjectsGroup = "@all-objects";

    [[nodiscard]] QListWidget *tileList() const { return tiles_; }
    [[nodiscard]] QListWidget *objectList() const { return objects_; }
    [[nodiscard]] QListWidget *brushList() const { return brushes_; }
    [[nodiscard]] QTabWidget *tabs() const { return tabs_; }
    [[nodiscard]] bool showsArtwork() const { return artwork_ != nullptr; }
    [[nodiscard]] bool hintVisible() const;
    [[nodiscard]] QString hintText() const;

    static constexpr int tileThumbnailSize = 48;
    static constexpr int objectThumbnailSize = 56;

signals:
    // `layer` is the SectionKind the tile belongs to (a group decides it).
    void tileChosen(int layer, int value);
    void presetChosen(const QString &id);
    void brushChosen(const QString &id);
    void artworkDirectoryRequested();
    // The World tab: each is a request to change the header.
    void worldSizeChosen(bool scrollsHorizontally, bool scrollsVertically);
    void regionChosen(int region);
    void musicChosen(int music);
    // Listen to a music number / stop listening; changes nothing stored.
    void musicPlayRequested(int music);
    void musicStopRequested();

private:
    void rebuild();
    void rebuildTileGroups();
    void rebuildTiles();
    void rebuildObjectGroups();
    void rebuildObjects();
    void rebuildWorld();
    [[nodiscard]] SectionKind tileGroupLayer(const QString &id) const;

    GameProfile profile_ = GameProfile::unknown;
    SectionKind layer_ = SectionKind::foreground;
    std::shared_ptr<const GameAssets> artwork_;
    bool built_ = false;

    QLabel *hint_ = nullptr;
    QPushButton *chooseDirectory_ = nullptr;
    QTabWidget *tabs_ = nullptr;
    QString tileGroup_ = QString::fromLatin1(allForegroundGroup);
    QString objectGroup_ = QString::fromLatin1(allObjectsGroup);
    QListWidget *tileGroups_ = nullptr;
    QListWidget *objectGroups_ = nullptr;
    QListWidget *tiles_ = nullptr;
    QListWidget *objects_ = nullptr;
    QListWidget *brushes_ = nullptr;
    QListWidget *backdrops_ = nullptr;
    QListWidget *music_ = nullptr;
    QPushButton *play_ = nullptr;
    QCheckBox *playWhenChosen_ = nullptr;
    QLabel *musicStatus_ = nullptr;
    QLabel *worldNote_ = nullptr;
    QButtonGroup *sizeGroup_ = nullptr;
    WorldState world_;
    int playing_ = 0;
};

} // namespace eggbert
