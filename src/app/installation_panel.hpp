// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QWidget>

#include <vector>

#include "world/installation.hpp"

class QLabel;
class QListWidget;
class QPushButton;

namespace eggbert {

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

    // The game directory (the one that contains DATA) and its game; an
    // empty directory shows how to choose one.
    void setInstallation(const QString &gameDirectory, GameProfile profile);
    void refresh();
    [[nodiscard]] const QString &gameDirectory() const { return gameDirectory_; }
    // The data directory inside it, or empty.
    [[nodiscard]] const QString &dataDirectory() const { return dataDirectory_; }
    [[nodiscard]] GameProfile profile() const { return profile_; }
    [[nodiscard]] const std::vector<Entry> &entries() const { return entries_; }

    [[nodiscard]] QListWidget *list() const { return list_; }
    [[nodiscard]] QLabel *summary() const { return summary_; }
    [[nodiscard]] QPushButton *deployButton() const { return deploy_; }
    [[nodiscard]] QPushButton *rollBackButton() const { return rollBack_; }
    [[nodiscard]] QPushButton *playtestButton() const { return playtest_; }
    // What the window can currently do, so the buttons say so.
    void setActions(bool canDeploy, bool canRollBack, const QString &rollBackHint);

    // The data directory of a game directory, whatever its letter case.
    [[nodiscard]] static QString findDataDirectory(const QString &gameDirectory);

signals:
    void openRequested(const QString &path);
    void deployRequested();
    void rollBackRequested();
    void playtestRequested();
    void chooseDirectoryRequested();

private:
    QString gameDirectory_;
    QString dataDirectory_;
    GameProfile profile_ = GameProfile::unknown;
    std::vector<Entry> entries_;
    QLabel *summary_ = nullptr;
    QListWidget *list_ = nullptr;
    QPushButton *deploy_ = nullptr;
    QPushButton *rollBack_ = nullptr;
    QPushButton *playtest_ = nullptr;
    QPushButton *choose_ = nullptr;
};

} // namespace eggbert
