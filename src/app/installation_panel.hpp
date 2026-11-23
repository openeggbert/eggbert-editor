// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QWidget>

#include <deque>
#include <memory>
#include <vector>

#include "world/installation.hpp"

class QLabel;
class QListWidget;
class QPushButton;
class QTimer;

namespace eggbert {

class GameAssets;

// The worlds of a game installation the user chose: every official and
// user-designed world in its data directory with a small picture, its role
// and its title. The directory is only read here; putting a world into it
// goes through DeployStore (see MainWindow).
class InstallationPanel final : public QWidget {
    Q_OBJECT

public:
    struct Entry {
        QString path;
        DataFileName name;
        // False when the file is not a complete world of a known revision.
        bool complete = false;
        QString title;
    };

    explicit InstallationPanel(QWidget *parent = nullptr);

    // The game directory (the one that contains DATA; for Mobile Eggbert
    // its Content folder) and its game; an empty directory shows how to
    // choose one.
    void setInstallation(const QString &gameDirectory, GameProfile profile);
    void refresh();
    // The game's pictures for the small pictures of the list; none for
    // schematic ones. The list appears with schematic pictures at once and
    // each is then replaced by one drawn with the artwork, one world at a
    // time, so a large installation does not hold the window up.
    void setArtwork(std::shared_ptr<const GameAssets> artwork);
    // How many pictures are still waiting to be drawn with artwork, and a
    // way to draw them all now.
    [[nodiscard]] std::size_t pendingThumbnails() const { return pending_.size(); }
    void finishThumbnails();
    [[nodiscard]] const QString &gameDirectory() const { return gameDirectory_; }
    // The folder its worlds are in (DATA, or Mobile Eggbert's `worlds`), or
    // empty.
    [[nodiscard]] const QString &dataDirectory() const { return dataDirectory_; }
    [[nodiscard]] GameProfile profile() const { return profile_; }
    [[nodiscard]] const std::vector<Entry> &entries() const { return entries_; }

    [[nodiscard]] QListWidget *list() const { return list_; }
    // The installation's demo recordings (`demo%.3d.blp`), below the worlds;
    // hidden when there are none. Activating one asks to open it.
    [[nodiscard]] QListWidget *recordingList() const { return recordings_; }
    [[nodiscard]] QLabel *summary() const { return summary_; }
    [[nodiscard]] QPushButton *deployButton() const { return deploy_; }
    [[nodiscard]] QPushButton *rollBackButton() const { return rollBack_; }
    [[nodiscard]] QPushButton *playtestButton() const { return playtest_; }
    // What the window can currently do, so the buttons say so.
    void setActions(bool canDeploy, bool canRollBack, const QString &rollBackHint);

    // The data directory of a game directory, whatever its letter case.
    [[nodiscard]] static QString findDataDirectory(const QString &gameDirectory);
    // The `worlds` folder of Mobile Eggbert, next to its Content folder (or
    // inside the given folder); empty when there is none.
    [[nodiscard]] static QString findPortWorldsDirectory(const QString &contentDirectory);

signals:
    void openRequested(const QString &path);
    void deployRequested();
    void rollBackRequested();
    void playtestRequested();
    void chooseDirectoryRequested();

private:
    struct Loaded {
        bool complete = false;
        QString title;
        QImage picture;
    };
    // Reads a world file and draws its small picture, schematically or with
    // the given artwork.
    [[nodiscard]] Loaded loadWorld(const QString &path, const GameAssets *artwork) const;
    [[nodiscard]] QString thumbnailKey(const QString &path) const;
    void drawNextThumbnail();

    std::shared_ptr<const GameAssets> artwork_;
    // Rows whose picture is still schematic, and pictures drawn with the
    // current artwork by file (path, time and size).
    std::deque<int> pending_;
    QHash<QString, QPixmap> thumbnails_;
    QTimer *thumbnailTimer_ = nullptr;
    QString gameDirectory_;
    QString dataDirectory_;
    GameProfile profile_ = GameProfile::unknown;
    std::vector<Entry> entries_;
    QLabel *summary_ = nullptr;
    QListWidget *list_ = nullptr;
    QLabel *recordingsTitle_ = nullptr;
    QListWidget *recordings_ = nullptr;
    QPushButton *deploy_ = nullptr;
    QPushButton *rollBack_ = nullptr;
    QPushButton *playtest_ = nullptr;
    QPushButton *choose_ = nullptr;
};

} // namespace eggbert
