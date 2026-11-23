// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDialog>

#include <functional>
#include <memory>
#include <optional>

#include "world/metadata.hpp"
#include "world/profile.hpp"

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTabWidget;

namespace eggbert {

class GameAssets;

// Edits the verified header fields of one document. It only collects values;
// the caller applies them as one transaction.
class MetadataDialog final : public QDialog {
    Q_OBJECT

public:
    enum Page { worldPage = 0, startsPage = 1 };

    MetadataDialog(const WorldView &world, GameProfile profile, QWidget *parent = nullptr);

    void setPage(Page page);
    // Experimental editing: music and region numbers the chosen game is not
    // known to offer are accepted as typed (and said to be so).
    void setExperimental(bool experimental);
    // A Mobile Eggbert world: no title, and one start (the other three rows
    // follow the first and cannot be edited).
    void setTextWorld(bool textWorld);
    // Background and music are chosen from lists as well as by number: the
    // backgrounds as the game's pictures (numbered swatches without them),
    // the music by number and length. `pictures` may be null; `game` is the
    // game directory the music files are in (may be null too).
    void setGame(std::shared_ptr<const GameAssets> pictures,
                 std::shared_ptr<const GameAssets> game);
    // Listening while choosing: `play` starts a music number and says
    // whether it plays, `stop` ends it, `status` is a line about it.
    void setMusicControls(std::function<bool(int)> play, std::function<void()> stop,
                          std::function<QString()> status);
    [[nodiscard]] QListWidget *backgroundList() const { return backgrounds_; }
    [[nodiscard]] QListWidget *musicList() const { return musicList_; }
    // The edited values, or nullopt while something cannot be stored.
    [[nodiscard]] std::optional<Metadata> metadata() const;
    [[nodiscard]] bool isValid() const { return metadata().has_value(); }
    // Current problems and warnings, one per line.
    [[nodiscard]] QString messages() const;

private:
    void refresh();
    void fillLists();
    void followNumbers();

    std::shared_ptr<const GameAssets> pictures_;
    std::shared_ptr<const GameAssets> game_;
    std::function<bool(int)> playMusic_;
    std::function<void()> stopMusic_;
    std::function<QString()> musicStatus_;
    QListWidget *backgrounds_ = nullptr;
    QListWidget *musicList_ = nullptr;
    QPushButton *play_ = nullptr;
    QPushButton *stop_ = nullptr;
    QLabel *musicLine_ = nullptr;
    Metadata original_;
    GameProfile profile_;
    bool titleRead_ = true;
    bool experimental_ = false;
    bool textWorld_ = false;
    QTabWidget *pages_ = nullptr;
    QLineEdit *name_ = nullptr;
    QSpinBox *music_ = nullptr;
    QSpinBox *region_ = nullptr;
    QCheckBox *scrollX_ = nullptr;
    QCheckBox *scrollY_ = nullptr;
    QSpinBox *posX_ = nullptr;
    QSpinBox *posY_ = nullptr;
    QSpinBox *startX_[4] = {};
    QSpinBox *startY_[4] = {};
    QComboBox *startDirection_[4] = {};
    QLabel *messages_ = nullptr;
    QDialogButtonBox *buttons_ = nullptr;
};

[[nodiscard]] QString decodeTitle(std::span<const std::byte> bytes);
[[nodiscard]] std::optional<std::vector<std::byte>> encodeTitle(const QString &text);

} // namespace eggbert
