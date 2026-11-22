// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QWidget>

#include <memory>

#include "world/layout.hpp"

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
    // Greys the lists out for a document that cannot be edited.
    void setEditable(bool editable);

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
    void tileChosen(int value);
    void presetChosen(const QString &id);
    void brushChosen(const QString &id);
    void artworkDirectoryRequested();

private:
    void rebuild();

    GameProfile profile_ = GameProfile::unknown;
    SectionKind layer_ = SectionKind::foreground;
    std::shared_ptr<const GameAssets> artwork_;
    bool built_ = false;

    QLabel *hint_ = nullptr;
    QPushButton *chooseDirectory_ = nullptr;
    QTabWidget *tabs_ = nullptr;
    QListWidget *tiles_ = nullptr;
    QListWidget *objects_ = nullptr;
    QListWidget *brushes_ = nullptr;
};

} // namespace eggbert
