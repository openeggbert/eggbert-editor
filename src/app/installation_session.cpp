// SPDX-License-Identifier: GPL-3.0-only
#include "app/installation_session.hpp"
#include "app/installation_panel.hpp"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <algorithm>

namespace eggbert {
namespace {
QString artworkSettingKey(GameProfile profile) {
    return profile == GameProfile::speedyBlupi1     ? QStringLiteral("artwork/directory-game1")
           : profile == GameProfile::mobileEggbert ? QStringLiteral("artwork/directory-mobile")
                                                    : QStringLiteral("artwork/directory-game2");
}

} // namespace

QString InstallationSession::gameRoot(const QString &path) {
    if (path.isEmpty()) {
        return {};
    }
    QDir folder(path);
    const auto name = folder.dirName();
    if (name.compare("DATA", Qt::CaseInsensitive) == 0 ||
        name.compare("worlds", Qt::CaseInsensitive) == 0) {
        folder.cdUp();
    } else if (InstallationPanel::findDataDirectory(path).isEmpty()) {
        for (const auto &child : folder.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            if (child.compare("Game", Qt::CaseInsensitive) == 0 &&
                !InstallationPanel::findDataDirectory(folder.filePath(child)).isEmpty()) {
                return folder.filePath(child);
            }
        }
    }
    return folder.absolutePath();
}

InstallationSession::InstallationSession() {
    const QSettings settings;
    for (const auto game : gamesWithArtwork) {
        const auto path = settings.value(artworkSettingKey(game)).toString();
        if (!path.isEmpty()) {
            directories_[game] = GameAssets::load(gameRoot(path), game);
            if (directories_.size() == 1) {
                selected_ = game;
            }
        }
    }
    const auto remembered = static_cast<GameProfile>(settings.value("installation/profile").toInt());
    if (assets(remembered)) {
        selected_ = remembered;
    }
}

std::shared_ptr<const GameAssets> InstallationSession::load(const QString &path, GameProfile game, bool select) {
    if (std::ranges::find(gamesWithArtwork, game) == std::end(gamesWithArtwork) || path.isEmpty()) {
        return nullptr;
    }
    const auto result = GameAssets::load(gameRoot(path), game);
    directories_[game] = result;
    QSettings settings;
    settings.setValue(artworkSettingKey(game), result->directory());
    if (select || !chosenInWindow_) {
        chosenInWindow_ = select;
        selected_ = game;
        settings.setValue("installation/profile", static_cast<int>(game));
    }
    return result;
}

std::shared_ptr<const GameAssets> InstallationSession::assets(GameProfile game) const {
    const auto it = directories_.find(game);
    return it == directories_.end() ? nullptr : it->second;
}

bool InstallationSession::accepts(GameProfile document, bool text) const {
    return selected_ != GameProfile::unknown && document == selected_ &&
           text == (selected_ == GameProfile::mobileEggbert);
}

GameProfile InstallationSession::profileOfWorld(const QString &path) const {
    const auto parent = QFileInfo(path).canonicalPath();
    auto match = GameProfile::unknown;
    for (const auto &[game, assets] : directories_) {
        const auto folder = game == GameProfile::mobileEggbert
            ? InstallationPanel::findPortWorldsDirectory(assets->directory())
            : InstallationPanel::findDataDirectory(assets->directory());
        if (!folder.isEmpty() && QFileInfo(folder).canonicalFilePath() == parent) {
            if (game == selected_) {
                return game;
            }
            match = game;
        }
    }
    return match;
}

void InstallationSession::followWorld(GameProfile game) {
    if (!chosenInWindow_ && game != GameProfile::unknown) {
        selected_ = game;
    }
}

bool InstallationSession::containsWorld(const QString &path) const {
    const QFileInfo file(path);
    const auto parent = file.dir().dirName();
    const auto name = file.fileName().toStdString();
    if (parent.compare("DATA", Qt::CaseInsensitive) == 0 && worldSlotOf(classifyDataFileName(name))) {
        return true;
    }
    if (parent.compare("worlds", Qt::CaseInsensitive) == 0 && portWorldNumber(name)) {
        return true;
    }
    return profileOfWorld(path) != GameProfile::unknown;
}

} // namespace eggbert
