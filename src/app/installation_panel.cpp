// SPDX-License-Identifier: GPL-3.0-only
#include "app/installation_panel.hpp"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

#include "app/catalog_texts.hpp"
#include "app/demo_viewer.hpp"
#include "app/game_assets.hpp"
#include "app/metadata_dialog.hpp"
#include "app/png_export.hpp"
#include "world/demo_recording.hpp"
#include "world/text_world_image.hpp"
#include "world/world_probe.hpp"
#include "world/world_stats.hpp"
#include "world/world_view.hpp"

namespace eggbert {

namespace {

constexpr int thumbnailSize = 64;
constexpr qint64 largestWorldRead = 1024 * 1024;

// A picture centred in a square, so the names line up whatever its shape.
QPixmap squareIcon(const QImage &picture) {
    QPixmap icon(thumbnailSize, thumbnailSize);
    icon.fill(Qt::transparent);
    if (!picture.isNull()) {
        const auto scaled = picture.scaled(thumbnailSize, thumbnailSize, Qt::KeepAspectRatio,
                                           Qt::SmoothTransformation);
        QPainter painter(&icon);
        painter.drawImage((thumbnailSize - scaled.width()) / 2,
                          (thumbnailSize - scaled.height()) / 2, scaled);
    }
    return icon;
}

} // namespace

InstallationPanel::InstallationPanel(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    summary_ = new QLabel(this);
    summary_->setObjectName(QStringLiteral("installationSummary"));
    summary_->setWordWrap(true);
    layout->addWidget(summary_);
    choose_ = new QPushButton(tr("Choose Game Directory..."), this);
    choose_->setObjectName(QStringLiteral("installationChoose"));
    layout->addWidget(choose_);
    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("installationList"));
    list_->setIconSize(QSize(thumbnailSize, thumbnailSize));
    list_->setUniformItemSizes(true);
    list_->setAccessibleName(tr("Worlds of the installation"));
    layout->addWidget(list_, 1);
    recordingsTitle_ = new QLabel(tr("Demo recordings (double-click to see the keys held):"), this);
    recordingsTitle_->setWordWrap(true);
    layout->addWidget(recordingsTitle_);
    recordings_ = new QListWidget(this);
    recordings_->setObjectName(QStringLiteral("installationRecordings"));
    recordings_->setAccessibleName(tr("Demo recordings of the installation"));
    recordings_->setMaximumHeight(110);
    recordingsTitle_->setBuddy(recordings_);
    layout->addWidget(recordings_);
    auto *buttons = new QHBoxLayout;
    deploy_ = new QPushButton(tr("Deploy Current World..."), this);
    deploy_->setObjectName(QStringLiteral("installationDeploy"));
    deploy_->setToolTip(tr("Puts the world of the current tab into this installation. The file "
                           "it replaces is kept and can be put back."));
    rollBack_ = new QPushButton(tr("Roll Back Last Deploy"), this);
    rollBack_->setObjectName(QStringLiteral("installationRollBack"));
    playtest_ = new QPushButton(tr("Playtest"), this);
    playtest_->setObjectName(QStringLiteral("installationPlaytest"));
    playtest_->setToolTip(tr("Starts the command you configured for this game, in the game "
                             "directory."));
    buttons->addWidget(deploy_);
    buttons->addWidget(rollBack_);
    buttons->addWidget(playtest_);
    layout->addLayout(buttons);

    connect(choose_, &QPushButton::clicked, this, [this] { emit chooseDirectoryRequested(); });
    connect(deploy_, &QPushButton::clicked, this, [this] { emit deployRequested(); });
    connect(rollBack_, &QPushButton::clicked, this, [this] { emit rollBackRequested(); });
    connect(playtest_, &QPushButton::clicked, this, [this] { emit playtestRequested(); });
    connect(list_, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        emit openRequested(item->data(Qt::UserRole).toString());
    });
    connect(recordings_, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        emit openRequested(item->data(Qt::UserRole).toString());
    });
    thumbnailTimer_ = new QTimer(this);
    thumbnailTimer_->setSingleShot(true);
    thumbnailTimer_->setInterval(0);
    connect(thumbnailTimer_, &QTimer::timeout, this, [this] { drawNextThumbnail(); });
    setActions(false, false, QString());
    refresh();
}

QString InstallationPanel::findDataDirectory(const QString &gameDirectory) {
    if (gameDirectory.isEmpty()) {
        return {};
    }
    const QDir directory(gameDirectory);
    for (const auto &entry : directory.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        if (entry.compare(QLatin1String("data"), Qt::CaseInsensitive) == 0) {
            return directory.filePath(entry);
        }
    }
    return {};
}

QString InstallationPanel::findPortWorldsDirectory(const QString &contentDirectory) {
    if (contentDirectory.isEmpty()) {
        return {};
    }
    for (const auto &candidate : {QDir(contentDirectory).filePath(QStringLiteral("../worlds")),
                                  QDir(contentDirectory).filePath(QStringLiteral("worlds"))}) {
        if (QFileInfo(candidate).isDir()) {
            return QDir::cleanPath(candidate);
        }
    }
    return {};
}

void InstallationPanel::setInstallation(const QString &gameDirectory, GameProfile profile) {
    gameDirectory_ = gameDirectory;
    profile_ = profile;
    refresh();
}

void InstallationPanel::setActions(bool canDeploy, bool canRollBack, const QString &rollBackHint) {
    deploy_->setEnabled(canDeploy && !dataDirectory_.isEmpty());
    rollBack_->setEnabled(canRollBack);
    rollBack_->setToolTip(rollBackHint);
    playtest_->setEnabled(!gameDirectory_.isEmpty());
}

InstallationPanel::Loaded InstallationPanel::loadWorld(const QString &path,
                                                       const GameAssets *artwork) const {
    Loaded loaded;
    const bool port = profile_ == GameProfile::mobileEggbert;
    QFile file(path);
    if (file.size() > largestWorldRead || !file.open(QIODevice::ReadOnly)) {
        return loaded;
    }
    const auto bytes = file.readAll();
    std::span<const std::byte> span(reinterpret_cast<const std::byte *>(bytes.constData()),
                                    static_cast<std::size_t>(bytes.size()));
    // A text world is drawn from its image, as its tab draws it.
    std::vector<std::byte> image;
    if (const std::string_view text(bytes.constData(), static_cast<std::size_t>(bytes.size()));
        port && looksLikeTextWorld(text)) {
        if (auto made = makeTextWorldImage(TextWorld::parse(text))) {
            image = std::move(made->bytes);
        }
        span = image;
    }
    const auto probe = probeWorld(span);
    loaded.complete = probe.descriptor != nullptr && (probe.status == ProbeStatus::sizeMatches ||
                                                      probe.status == ProbeStatus::surplus);
    if (!loaded.complete) {
        return loaded;
    }
    const WorldView view(span, probe);
    if (const auto header = view.header(); header && probe.descriptor->minor >= 3 && !port) {
        loaded.title = decodeTitle(header->nameText());
    }
    PngExportOptions options;
    options.pixelsPerCell = 1;
    options.render.showLabels = false;
    options.render.showPaths = false;
    options.render.showStarts = false;
    if (artwork != nullptr) {
        // The game's pictures on a plain ground: a backdrop would only blur
        // a picture this small.
        options.render.profile = profile_;
        options.render.artwork = artwork_;
        options.render.showBackdrop = false;
    }
    // Only the part of the map that is used, so a small world is not a speck
    // in a 100 x 100 picture.
    if (const auto used = computeWorldStats(view).usedBounds) {
        options.crop = CellRect{{used->minX, used->minY}, {used->maxX, used->maxY}};
        const auto longest = std::max(used->maxX - used->minX, used->maxY - used->minY) + 1;
        options.pixelsPerCell = std::clamp(static_cast<int>(2 * thumbnailSize / longest), 1, 8);
    }
    loaded.picture = renderWorldForExport(view, options);
    return loaded;
}

QString InstallationPanel::thumbnailKey(const QString &path) const {
    const QFileInfo info(path);
    return QStringLiteral("%1|%2|%3")
        .arg(info.absoluteFilePath())
        .arg(info.lastModified().toMSecsSinceEpoch())
        .arg(info.size());
}

void InstallationPanel::setArtwork(std::shared_ptr<const GameAssets> artwork) {
    if (artwork != nullptr && (!artwork->usable() || artwork->profile() != profile_)) {
        artwork = nullptr;
    }
    if (artwork == artwork_) {
        return;
    }
    artwork_ = std::move(artwork);
    thumbnails_.clear();
    refresh();
}

void InstallationPanel::drawNextThumbnail() {
    if (pending_.empty() || artwork_ == nullptr) {
        pending_.clear();
        return;
    }
    const auto row = pending_.front();
    pending_.pop_front();
    if (auto *item = list_->item(row); item != nullptr) {
        const auto path = item->data(Qt::UserRole).toString();
        const auto loaded = loadWorld(path, artwork_.get());
        if (!loaded.picture.isNull()) {
            const auto icon = squareIcon(loaded.picture);
            thumbnails_.insert(thumbnailKey(path), icon);
            item->setIcon(QIcon(icon));
        }
    }
    if (!pending_.empty()) {
        thumbnailTimer_->start();
    }
}

void InstallationPanel::finishThumbnails() {
    thumbnailTimer_->stop();
    while (!pending_.empty()) {
        drawNextThumbnail();
    }
    thumbnailTimer_->stop();
}

void InstallationPanel::refresh() {
    thumbnailTimer_->stop();
    pending_.clear();
    entries_.clear();
    list_->clear();
    recordings_->clear();
    recordingsTitle_->hide();
    recordings_->hide();
    const bool port = profile_ == GameProfile::mobileEggbert;
    dataDirectory_ = port ? findPortWorldsDirectory(gameDirectory_)
                          : findDataDirectory(gameDirectory_);
    choose_->setVisible(dataDirectory_.isEmpty());
    if (dataDirectory_.isEmpty()) {
        summary_->setText(gameDirectory_.isEmpty()
                              ? tr("Choose the directory of the game you have (View > Game "
                                   "Artwork Directory) to see its worlds here.")
                              : port ? tr("%1 has no worlds folder next to it.").arg(gameDirectory_)
                                     : tr("%1 has no DATA folder.").arg(gameDirectory_));
        playtest_->setEnabled(!gameDirectory_.isEmpty());
        deploy_->setEnabled(false);
        return;
    }
    int saves = 0;
    int players = 0;
    int recordings = 0;
    const QDir data(dataDirectory_);
    for (const auto &fileName : data.entryList(QDir::Files, QDir::Name | QDir::IgnoreCase)) {
        if (port) {
            // Mobile Eggbert keeps worlds only: world%.3d.txt.
            if (const auto number = portWorldNumber(fileName.toStdString())) {
                entries_.push_back({data.filePath(fileName),
                                    {DataFileKind::officialWorld, 0, *number}, false, QString()});
            }
            continue;
        }
        const auto name = classifyDataFileName(fileName.toStdString());
        switch (name.kind) {
        case DataFileKind::savedGame:
            ++saves;
            break;
        case DataFileKind::gamerInfo:
            ++players;
            break;
        case DataFileKind::demoRecording: {
            ++recordings;
            // What the file holds, when it is a recording the games wrote.
            QString detail = tr("not a recording the games would write");
            QFile file(data.filePath(fileName));
            if (file.size() <= largestWorldRead && file.open(QIODevice::ReadOnly)) {
                const auto bytes = file.readAll();
                if (const auto recording = DemoRecording::parse(
                        {reinterpret_cast<const std::byte *>(bytes.constData()),
                         static_cast<std::size_t>(bytes.size())})) {
                    detail = tr("%1 step(s), %2")
                                 .arg(recording->lastTime())
                                 .arg(demoDuration(recording->lastTime()));
                }
            }
            auto *item = new QListWidgetItem(
                tr("%1 - replayed on world %2 - %3").arg(fileName).arg(name.number).arg(detail),
                recordings_);
            item->setData(Qt::UserRole, data.filePath(fileName));
            item->setToolTip(data.filePath(fileName));
            break;
        }
        case DataFileKind::officialWorld:
        case DataFileKind::userWorld:
            entries_.push_back({data.filePath(fileName), name, false, QString()});
            break;
        default:
            break;
        }
    }
    // Official worlds by number, then user worlds by player and number.
    std::ranges::stable_sort(entries_, [](const Entry &a, const Entry &b) {
        const auto key = [](const Entry &entry) {
            return std::tuple(entry.name.kind == DataFileKind::userWorld, entry.name.gamer,
                              entry.name.number);
        };
        return key(a) < key(b);
    });
    int official = 0;
    int user = 0;
    for (auto &entry : entries_) {
        (entry.name.kind == DataFileKind::officialWorld ? official : user) += 1;
        const auto loaded = loadWorld(entry.path, nullptr);
        entry.complete = loaded.complete;
        entry.title = loaded.title;
        QString label = QFileInfo(entry.path).fileName();
        if (entry.name.kind == DataFileKind::officialWorld) {
            const auto role = worldRoleName(officialWorldRole(profile_, entry.name.number));
            if (!role.empty()) {
                label += QStringLiteral(" - ") + catalogText(role);
            }
        } else if (entry.name.number == scratchWorldNumber) {
            label += QStringLiteral(" - ") + tr("the game's play-test copy");
        } else {
            label += QStringLiteral(" - ") + tr("player %1, world %2")
                                                  .arg(entry.name.gamer)
                                                  .arg(entry.name.number);
        }
        if (!entry.title.isEmpty()) {
            label += QStringLiteral("\n") + entry.title;
        }
        if (!entry.complete) {
            label += QStringLiteral("\n") + tr("(not a complete world)");
        }
        // The schematic picture now; the one with artwork from an earlier
        // pass when the file has not changed, otherwise later.
        auto icon = squareIcon(loaded.picture);
        if (artwork_ != nullptr && entry.complete) {
            if (const auto drawn = thumbnails_.constFind(thumbnailKey(entry.path));
                drawn != thumbnails_.constEnd()) {
                icon = *drawn;
            } else {
                pending_.push_back(list_->count());
            }
        }
        auto *item = new QListWidgetItem(QIcon(icon), label, list_);
        item->setData(Qt::UserRole, entry.path);
        item->setToolTip(entry.path);
    }
    if (!pending_.empty()) {
        thumbnailTimer_->start();
    }
    recordingsTitle_->setVisible(recordings != 0);
    recordings_->setVisible(recordings != 0);
    summary_->setText(
        port ? tr("%1\n%n world(s) of Mobile Eggbert. Double-click a world to open it.", nullptr,
                  official)
                   .arg(dataDirectory_)
             : tr("%1\n%2 official world(s), %3 user world(s), %4 saved game(s), %5 "
                  "player file(s), %6 recording(s). Double-click a world to open it.")
                   .arg(dataDirectory_)
                   .arg(official)
                   .arg(user)
                   .arg(saves)
                   .arg(players)
                   .arg(recordings));
    playtest_->setEnabled(true);
}

} // namespace eggbert
