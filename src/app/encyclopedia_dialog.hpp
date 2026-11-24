// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDialog>

#include <memory>
#include <vector>

#include "world/encyclopedia.hpp"

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;

namespace eggbert {

class GameAssets;

// Searchable list of everything the catalogs know about a game: what it is
// called, what is stored for it and where that was observed. Pictures come
// only from the game directory the user chose; without one the entries show
// numbered placeholders.
class EncyclopediaDialog : public QDialog {
    Q_OBJECT

public:
    explicit EncyclopediaDialog(QWidget *parent = nullptr);

    // Rebuilds the list for a game and its artwork (may be null).
    void setGame(GameProfile profile, std::shared_ptr<const GameAssets> artwork);
    [[nodiscard]] GameProfile game() const { return profile_; }
    [[nodiscard]] const std::shared_ptr<const GameAssets> &artwork() const { return artwork_; }

    [[nodiscard]] QLineEdit *searchBox() const { return search_; }
    [[nodiscard]] QListWidget *list() const { return list_; }
    [[nodiscard]] QLabel *details() const { return details_; }
    [[nodiscard]] QPushButton *selectButton() const { return select_; }
    // The entry shown in the details pane, or null.
    [[nodiscard]] const EncyclopediaEntry *currentEntry() const;

signals:
    // "Select in Editor": the same choices the Palette makes.
    void tileChosen(int layer, int value);
    void presetPictureChosen(const QString &id, int picture);
    void presetChannelChosen(const QString &id, int channel);
    void presetChosen(const QString &id);
    void brushChosen(const QString &id);

private:
    void rebuildEntries();
    void refill();
    void showVariants(const EncyclopediaEntry &entry);
    void showCurrent();
    void chooseCurrent();

    GameProfile profile_ = GameProfile::unknown;
    std::shared_ptr<const GameAssets> artwork_;
    std::vector<EncyclopediaEntry> entries_;
    QLineEdit *search_ = nullptr;
    QListWidget *list_ = nullptr;
    QComboBox *category_ = nullptr;
    QListWidget *variants_ = nullptr;
    QLabel *details_ = nullptr;
    QLabel *backdrop_ = nullptr;
    QLabel *count_ = nullptr;
    QPushButton *select_ = nullptr;
};

} // namespace eggbert
