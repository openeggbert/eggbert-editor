// SPDX-License-Identifier: GPL-3.0-only
#include "palette_panel.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QScrollArea>
#include <QSettings>
#include <QToolButton>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <set>
#include <vector>

#include "app/document_tab.hpp"
#include "app/music_player.hpp"
#include "app/game_assets.hpp"
#include "app/catalog_texts.hpp"
#include "app/sprite_icons.hpp"
#include "app/world_renderer.hpp"
#include "world/adaptive.hpp"
#include "world/animation.hpp"
#include "world/editor_groups.hpp"
#include "world/object_catalog.hpp"
#include "world/profile.hpp"

namespace eggbert {

namespace {

constexpr int groupThumbnailSize = 36;
// Item data of a tile: the value (Qt::UserRole), its layer and, for tiles
// the games' editors reshape, the adaptive brush that paints them.
constexpr int layerRole = Qt::UserRole + 1;
constexpr int brushRole = Qt::UserRole + 2;

QString text(std::string_view value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

QListWidget *makeGrid(QWidget *parent, int iconSize, QSize cell) {
    auto *list = new QListWidget(parent);
    list->setViewMode(QListView::IconMode);
    list->setResizeMode(QListView::Adjust);
    list->setMovement(QListView::Static);
    list->setWrapping(true);
    list->setIconSize(QSize(iconSize, iconSize));
    list->setGridSize(cell);
    list->setSelectionMode(QAbstractItemView::SingleSelection);
    list->setWordWrap(true);
    list->setTextElideMode(Qt::ElideNone);
    list->setSpacing(2);
    return list;
}

// The strip of groups above a grid: small pictures with short names.
QListWidget *makeGroupStrip(QWidget *parent) {
    auto *list = makeGrid(parent, groupThumbnailSize, QSize(84, groupThumbnailSize + 36));
    list->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    return list;
}

// A strip is as tall as its groups need, up to three rows; more scroll.
void fitStrip(QListWidget *list) {
    const int columns = std::max(1, std::max(list->viewport()->width(), 240) / list->gridSize().width());
    const int rows = std::clamp((list->count() + columns - 1) / columns, 1, 3);
    list->setFixedHeight(rows * list->gridSize().height() + 2 * list->frameWidth() + 6);
}

void selectByData(QListWidget *list, const QVariant &data) {
    const QSignalBlocker blocker(list);
    for (int row = 0; row < list->count(); ++row) {
        if (list->item(row)->data(Qt::UserRole) == data) {
            list->setCurrentRow(row);
            return;
        }
    }
    list->setCurrentRow(-1);
    list->clearSelection();
}

bool hasData(const QListWidget *list, const QVariant &data) {
    for (int row = 0; row < list->count(); ++row) {
        if (list->item(row)->data(Qt::UserRole) == data) {
            return true;
        }
    }
    return false;
}

// A small diagram of a world size: the 100 x 100 map area in outline with
// the part the game uses filled (one screen is 10 x 8 cells).
QIcon sizeIcon(bool horizontally, bool vertically) {
    QPixmap pixmap(40, 40);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setPen(QColor(120, 120, 130));
    painter.drawRect(4, 4, 31, 31);
    painter.fillRect(5, 5, horizontally ? 30 : 9, vertically ? 30 : 7, QColor(70, 150, 220));
    return QIcon(pixmap);
}

QWidget *page(QWidget *parent, QListWidget *groups, QListWidget *members) {
    auto *host = new QWidget(parent);
    auto *layout = new QVBoxLayout(host);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);
    layout->addWidget(groups);
    layout->addWidget(members, 1);
    return host;
}

std::vector<int> allTileValues(const ProfileInfo *info, SectionKind layer) {
    std::vector<int> values;
    const bool big = layer == SectionKind::bigDecor;
    int last = big ? 99 : 255;
    if (info != nullptr) {
        last = big ? info->bigDecorAtlas.maximum : info->foregroundAtlas.maximum;
    }
    for (int value = 0; value <= last; ++value) {
        values.push_back(value);
    }
    if (big) {
        values.push_back(203); // drawn by both games as a plant
    }
    return values;
}

} // namespace

PalettePanel::PalettePanel(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    hint_ = new QLabel(this);
    hint_->setObjectName(QStringLiteral("paletteHint"));
    hint_->setWordWrap(true);
    hint_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    chooseDirectory_ = new QPushButton(tr("Choose Game Directory..."), this);
    chooseDirectory_->setObjectName(QStringLiteral("paletteChooseDirectory"));
    chooseDirectory_->setToolTip(tr("Select the directory of a game you have, to draw tiles and "
                                    "objects with its graphics. The directory is only read."));
    connect(chooseDirectory_, &QPushButton::clicked, this,
            &PalettePanel::artworkDirectoryRequested);
    layout->addWidget(hint_);
    layout->addWidget(chooseDirectory_);

    tabs_ = new QTabWidget(this);
    tabs_->setObjectName(QStringLiteral("paletteTabs"));
    tileGroups_ = makeGroupStrip(tabs_);
    tileGroups_->setObjectName(QStringLiteral("paletteTileGroups"));
    tileGroups_->setAccessibleName(tr("Tile groups"));
    tiles_ = makeGrid(tabs_, tileThumbnailSize, QSize(tileThumbnailSize + 8, tileThumbnailSize + 8));
    tiles_->setObjectName(QStringLiteral("paletteTiles"));
    tiles_->setAccessibleName(tr("Tiles of the group"));
    objectGroups_ = makeGroupStrip(tabs_);
    objectGroups_->setObjectName(QStringLiteral("paletteObjectGroups"));
    objectGroups_->setAccessibleName(tr("Object groups"));
    objects_ = makeGrid(tabs_, objectThumbnailSize, QSize(112, objectThumbnailSize + 52));
    objects_->setObjectName(QStringLiteral("paletteObjects"));
    objects_->setAccessibleName(tr("Objects of the group"));
    brushes_ = makeGrid(tabs_, objectThumbnailSize, QSize(112, objectThumbnailSize + 52));
    brushes_->setObjectName(QStringLiteral("paletteBrushes"));
    brushes_->setAccessibleName(tr("Border brushes"));
    tabs_->addTab(page(tabs_, tileGroups_, tiles_), tr("Tiles"));
    auto *objectPage = page(tabs_, objectGroups_, objects_);
    appearance_ = new QComboBox(objectPage);
    appearance_->setObjectName(QStringLiteral("paletteObjectAppearance"));
    appearance_->setAccessibleName(tr("Object appearance"));
    appearance_->setToolTip(tr("Choose the appearance to use when placing this object."));
    appearance_->setIconSize(QSize(32, 32));
    appearance_->hide();
    objectPage->layout()->addWidget(appearance_);
    connect(appearance_, &QComboBox::activated, this, [this](int row) {
        const auto *preset = findObjectPreset(profile_, appearanceId_.toStdString());
        if (!preset || row < 0) {
            return;
        }
        const int value = appearance_->itemData(row).toInt();
        if (preset->iconIsChoice) {
            emit presetPictureChosen(appearanceId_, value);
        } else if (preset->hasColourVariants()) {
            emit presetChannelChosen(appearanceId_, value);
        }
    });
    tabs_->addTab(objectPage, tr("Objects"));
    tabs_->addTab(brushes_, tr("Borders"));

    // World: size, background and music of the whole world.
    {
        auto *host = new QWidget(tabs_);
        auto *world = new QVBoxLayout(host);
        world->setContentsMargins(4, 4, 4, 4);
        worldNote_ = new QLabel(host);
        worldNote_->setObjectName(QStringLiteral("paletteWorldNote"));
        worldNote_->setWordWrap(true);
        world->addWidget(worldNote_);

        sizeBox_ = new QGroupBox(tr("World size"), host);
        auto *sizeRow = new QHBoxLayout(sizeBox_);
        sizeGroup_ = new QButtonGroup(this);
        sizeGroup_->setExclusive(true);
        const auto addSize = [&](bool horizontally, bool vertically, const QString &label,
                                 const QString &tip) {
            auto *button = new QToolButton(sizeBox_);
            button->setObjectName(QStringLiteral("paletteSize%1%2")
                                      .arg(horizontally ? 1 : 0)
                                      .arg(vertically ? 1 : 0));
            button->setCheckable(true);
            button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
            button->setIcon(sizeIcon(horizontally, vertically));
            button->setIconSize(QSize(40, 40));
            button->setText(label);
            button->setToolTip(tip);
            sizeGroup_->addButton(button, (horizontally ? 1 : 0) | (vertically ? 2 : 0));
            sizeRow->addWidget(button);
        };
        addSize(false, false, tr("Screen"),
                tr("One screen: 10 cells wide and 480 pixels high, no scrolling"));
        addSize(true, false, tr("Wide"), tr("Scrolls horizontally: 100 cells wide, one screen high"));
        addSize(false, true, tr("Tall"), tr("Scrolls vertically: one screen wide, 100 cells high"));
        addSize(true, true, tr("Large"), tr("Scrolls both ways: 100 x 100 cells"));
        world->addWidget(sizeBox_);
        connect(sizeGroup_, &QButtonGroup::idClicked, this, [this](int id) {
            emit worldSizeChosen((id & 1) != 0, (id & 2) != 0);
        });

        auto *backdropBox = new QGroupBox(tr("Background"), host);
        auto *backdropLayout = new QVBoxLayout(backdropBox);
        backdrops_ = makeGrid(backdropBox, backdropThumbnailWidth,
                              QSize(backdropThumbnailWidth + 10, backdropThumbnailHeight + 26));
        backdrops_->setIconSize(QSize(backdropThumbnailWidth, backdropThumbnailHeight));
        backdrops_->setObjectName(QStringLiteral("paletteBackdrops"));
        backdrops_->setAccessibleName(tr("Backdrop pictures"));
        backdropLayout->addWidget(backdrops_);
        world->addWidget(backdropBox, 3);
        const auto onBackdrop = [this](QListWidgetItem *item) {
            emit regionChosen(item->data(Qt::UserRole).toInt());
        };
        connect(backdrops_, &QListWidget::itemClicked, this, onBackdrop);
        connect(backdrops_, &QListWidget::itemActivated, this, onBackdrop);

        musicBox_ = new QGroupBox(tr("Music"), host);
        auto *musicLayout = new QVBoxLayout(musicBox_);
        music_ = new QListWidget(musicBox_);
        music_->setObjectName(QStringLiteral("paletteMusic"));
        music_->setAccessibleName(tr("Music"));
        musicLayout->addWidget(music_);
        auto *playRow = new QHBoxLayout;
        play_ = new QPushButton(tr("Play"), musicBox_);
        play_->setObjectName(QStringLiteral("palettePlay"));
        playWhenChosen_ = new QCheckBox(tr("Play when chosen"), musicBox_);
        playWhenChosen_->setObjectName(QStringLiteral("palettePlayWhenChosen"));
        playWhenChosen_->setChecked(
            QSettings().value(QStringLiteral("music/play-when-chosen"), true).toBool());
        connect(playWhenChosen_, &QCheckBox::toggled, this, [](bool checked) {
            QSettings().setValue(QStringLiteral("music/play-when-chosen"), checked);
        });
        playRow->addWidget(play_);
        playRow->addWidget(playWhenChosen_, 1);
        musicLayout->addLayout(playRow);
        musicStatus_ = new QLabel(musicBox_);
        musicStatus_->setObjectName(QStringLiteral("paletteMusicStatus"));
        musicStatus_->setWordWrap(true);
        musicLayout->addWidget(musicStatus_);
        world->addWidget(musicBox_, 2);
        const auto onMusic = [this](QListWidgetItem *item) {
            emit musicChosen(item->data(Qt::UserRole).toInt());
        };
        connect(music_, &QListWidget::itemClicked, this, onMusic);
        connect(music_, &QListWidget::itemActivated, this, onMusic);
        connect(play_, &QPushButton::clicked, this, [this] {
            if (playing_ != 0) {
                emit musicStopRequested();
            } else if (music_->currentItem() != nullptr &&
                       music_->currentItem()->data(Qt::UserRole).toInt() > 0) {
                emit musicPlayRequested(music_->currentItem()->data(Qt::UserRole).toInt());
            }
        });
        tabs_->addTab(host, tr("World"));
    }
    layout->addWidget(tabs_, 1);
    setMinimumWidth(260);

    const auto onGroup = [this](QListWidget *list, bool tiles) {
        const auto choose = [this, tiles](QListWidgetItem *item) {
            const auto id = item->data(Qt::UserRole).toString();
            if (tiles) {
                showTileGroup(id);
            } else {
                showObjectGroup(id);
            }
        };
        connect(list, &QListWidget::itemClicked, this, choose);
        connect(list, &QListWidget::itemActivated, this, choose);
    };
    onGroup(tileGroups_, true);
    onGroup(objectGroups_, false);

    const auto onTile = [this](QListWidgetItem *item) {
        const auto brush = item->data(brushRole).toString();
        if (!brush.isEmpty()) {
            // The games' editors reshape this tile to fit its neighbours:
            // paint it with the adaptive brush that does the same.
            emit brushChosen(brush);
            return;
        }
        emit tileChosen(item->data(layerRole).toInt(), item->data(Qt::UserRole).toInt());
    };
    connect(tiles_, &QListWidget::itemClicked, this, onTile);
    connect(tiles_, &QListWidget::itemActivated, this, onTile);
    const auto onObject = [this](QListWidgetItem *item) {
        emit presetChosen(item->data(Qt::UserRole).toString());
    };
    connect(objects_, &QListWidget::itemClicked, this, onObject);
    connect(objects_, &QListWidget::itemActivated, this, onObject);
    const auto onBrush = [this](QListWidgetItem *item) {
        emit brushChosen(item->data(Qt::UserRole).toString());
    };
    connect(brushes_, &QListWidget::itemClicked, this, onBrush);
    connect(brushes_, &QListWidget::itemActivated, this, onBrush);
    rebuild();
}

bool PalettePanel::hintVisible() const {
    return !hint_->isHidden();
}

QString PalettePanel::hintText() const {
    return hint_->text();
}

void PalettePanel::setContext(GameProfile profile, SectionKind layer,
                              std::shared_ptr<const GameAssets> artwork) {
    if (built_ && profile == profile_ && layer == layer_ && artwork == artwork_) {
        return;
    }
    const bool layerChanged = layer != layer_;
    profile_ = profile;
    layer_ = layer;
    artwork_ = std::move(artwork);
    const bool plainGroup = tileGroup_ == QLatin1String(allForegroundGroup) ||
                            tileGroup_ == QLatin1String(otherForegroundGroup) ||
                            tileGroup_ == QLatin1String(allBigDecorGroup);
    if (layerChanged && plainGroup && tileGroupLayer(tileGroup_) != layer_) {
        // The active layer was changed elsewhere: show that layer's tiles.
        tileGroup_ = QString::fromLatin1(layer_ == SectionKind::bigDecor ? allBigDecorGroup
                                                                          : allForegroundGroup);
    }
    rebuild();
}

void PalettePanel::setCurrentTile(int value) {
    selectByData(tiles_, value);
}

void PalettePanel::setCurrentPreset(const QString &id, std::optional<std::int16_t> appearance) {
    currentPresetId_ = id;
    currentAppearance_ = appearance;
    selectByData(objects_, id);
    const auto *preset = objects_->currentItem() ? findObjectPreset(profile_, id.toStdString()) : nullptr;
    if (!preset || (!preset->iconIsChoice && !preset->hasColourVariants())) {
        appearance_->hide();
        return;
    }
    if (appearanceId_ != id) {
        appearance_->clear();
        appearanceId_ = id;
        if (preset->iconIsChoice) {
            const int last = profileInfo(profile_)->foregroundAtlas.maximum;
            for (int icon = 0; icon <= last; ++icon) {
                appearance_->addItem(spriteThumbnail(1, icon, artwork_.get(), 32), tr("Tile %1").arg(icon), icon);
            }
        } else {
            for (int sheet = 11; sheet <= 13; ++sheet) {
                appearance_->addItem(spriteThumbnail(sheet, preset->icon, artwork_.get(), 32),
                                     tr("Player %1 (channel %2)").arg(sheet - 9).arg(sheet), sheet);
            }
        }
    }
    const int value = appearance.value_or(preset->iconIsChoice ? preset->icon : preset->channel);
    appearance_->setCurrentIndex(appearance_->findData(value));
    appearance_->show();
}

void PalettePanel::setCurrentBrush(const QString &id) {
    selectByData(brushes_, id);
}

void PalettePanel::setHasWorld(bool hasWorld) {
    if (hasWorld_ == hasWorld) {
        return;
    }
    hasWorld_ = hasWorld;
    tabs_->setVisible(hasWorld_);
    rebuild();
}

void PalettePanel::setEditable(bool editable) {
    tabs_->setEnabled(editable);
}

QAbstractButton *PalettePanel::sizeButton(bool horizontally, bool vertically) const {
    return sizeGroup_->button((horizontally ? 1 : 0) | (vertically ? 2 : 0));
}

bool PalettePanel::playWhenChosen() const {
    return playWhenChosen_->isChecked();
}

QString PalettePanel::musicStatus() const {
    return musicStatus_->text();
}

void PalettePanel::setWorldState(const WorldState &state) {
    if (state == world_) {
        return;
    }
    world_ = state;
    rebuildWorld();
}

void PalettePanel::setMusicStatus(const QString &text, int playingNumber, bool canPlay) {
    playing_ = playingNumber;
    musicStatus_->setText(text);
    play_->setText(playingNumber != 0 ? tr("Stop") : tr("Play"));
    play_->setEnabled(playingNumber != 0 || canPlay);
}

void PalettePanel::rebuildWorld() {
    const bool mobile = profile_ == GameProfile::mobileEggbert;
    sizeBox_->setVisible(!mobile);
    musicBox_->setVisible(!mobile);
    const auto *info = profileInfo(profile_);
    const auto *artwork = artwork_.get();
    {
        const QSignalBlocker blocker(sizeGroup_);
        for (auto *button : sizeGroup_->buttons()) {
            button->setEnabled(world_.available);
        }
        // Exclusive groups cannot be cleared by unchecking: lift the rule.
        sizeGroup_->setExclusive(false);
        for (auto *button : sizeGroup_->buttons()) {
            button->setChecked(false);
        }
        sizeGroup_->setExclusive(true);
        if (world_.available) {
            sizeButton(world_.scrollsHorizontally, world_.scrollsVertically)->setChecked(true);
        }
    }
    if (!world_.available) {
        worldNote_->setText(tr("Open a world to change its size, background and music."));
    } else if (info == nullptr) {
        worldNote_->setText(tr("Choose the game in the tool bar to change background and music; "
                               "their numbers mean different things in the two games."));
    } else {
        worldNote_->setText(QString());
    }
    worldNote_->setVisible(!worldNote_->text().isEmpty());

    {
        const QSignalBlocker blocker(backdrops_);
        backdrops_->clear();
        if (info != nullptr && world_.available) {
            for (int region = info->region.minimum; region <= info->region.maximum; ++region) {
                QPixmap pixmap(backdropThumbnailWidth, backdropThumbnailHeight);
                const auto picture = artwork != nullptr ? artwork->readBackdrop(region) : QImage();
                if (!picture.isNull()) {
                    pixmap = QPixmap::fromImage(picture.scaled(pixmap.size(), Qt::IgnoreAspectRatio,
                                                               Qt::SmoothTransformation));
                } else {
                    pixmap.fill(schematicBackgroundColor());
                    QPainter painter(&pixmap);
                    painter.setPen(Qt::white);
                    painter.drawText(pixmap.rect(), Qt::AlignCenter, QString::number(region));
                }
                auto *item = new QListWidgetItem(QIcon(pixmap), tr("%1").arg(region), backdrops_);
                item->setData(Qt::UserRole, region);
                item->setSizeHint(backdrops_->gridSize());
                item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
                item->setToolTip(tr("Background %1").arg(region));
            }
            selectByData(backdrops_, world_.region);
        }
        backdrops_->setEnabled(backdrops_->count() > 0);
    }
    {
        const QSignalBlocker blocker(music_);
        music_->clear();
        if (!mobile && info != nullptr && world_.available) {
            for (int number = info->music.minimum; number <= info->music.maximum; ++number) {
                auto *item = new QListWidgetItem(
                    MusicPlayer::trackLabel(number, artwork != nullptr ? artwork->musicFile(number)
                                                                       : QString()),
                    music_);
                item->setData(Qt::UserRole, number);
            }
            selectByData(music_, world_.music);
        }
        music_->setEnabled(music_->count() > 0);
    }
}

SectionKind PalettePanel::tileGroupLayer(const QString &id) const {
    return id == QLatin1String(allBigDecorGroup) ? SectionKind::bigDecor
                                                  : SectionKind::foreground;
}

bool PalettePanel::showTileGroup(const QString &id) {
    if (!hasData(tileGroups_, id)) {
        return false;
    }
    tileGroup_ = id;
    selectByData(tileGroups_, id);
    rebuildTiles();
    return true;
}

bool PalettePanel::showObjectGroup(const QString &id) {
    if (!hasData(objectGroups_, id)) {
        return false;
    }
    objectGroup_ = id;
    selectByData(objectGroups_, id);
    rebuildObjects();
    return true;
}

void PalettePanel::rebuildTileGroups() {
    const auto *artwork = artwork_.get();
    const QSignalBlocker blocker(tileGroups_);
    tileGroups_->clear();
    const auto add = [&](const QString &id, const QString &name, SectionKind layer, int picture,
                         const QString &tip) {
        auto *item = new QListWidgetItem(
            QIcon(tileThumbnail(layer, picture, artwork, groupThumbnailSize)), name, tileGroups_);
        item->setData(Qt::UserRole, id);
        item->setSizeHint(tileGroups_->gridSize());
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        item->setToolTip(tip);
    };
    bool anyGroup = false;
    for (const auto &group : editorGroups(profile_)) {
        if (group.tiles.empty()) {
            continue;
        }
        anyGroup = true;
        const auto &first = group.tiles.front();
        std::size_t count = 0;
        for (const auto &entry : group.tiles) {
            count += entry.values.size();
        }
        add(text(group.id), catalogText(group.name), first.layer,
            first.values.empty() ? layout::emptyCell : first.values.front(),
            tr("%1: %2 tile(s) in %3 kind(s), as one button of the game's own editor offers them")
                .arg(catalogText(group.name))
                .arg(count)
                .arg(group.tiles.size()));
    }
    if (anyGroup) {
        add(QString::fromLatin1(otherForegroundGroup), tr("Other tiles"), SectionKind::foreground,
            layout::emptyCell, tr("Foreground tiles that are in none of the groups"));
    }
    add(QString::fromLatin1(allForegroundGroup), tr("All tiles"), SectionKind::foreground,
        layout::emptyCell, tr("Every foreground tile, by number"));
    add(QString::fromLatin1(allBigDecorGroup), tr("All bigDecor"), SectionKind::bigDecor,
        layout::emptyCell, tr("Every bigDecor picture, by number"));
    if (!hasData(tileGroups_, tileGroup_)) {
        tileGroup_ = QString::fromLatin1(layer_ == SectionKind::bigDecor ? allBigDecorGroup
                                                                          : allForegroundGroup);
    }
    selectByData(tileGroups_, tileGroup_);
    fitStrip(tileGroups_);
}

void PalettePanel::rebuildTiles() {
    const auto *artwork = artwork_.get();
    const auto *info = profileInfo(profile_);
    const auto layer = tileGroupLayer(tileGroup_);
    const QSignalBlocker blocker(tiles_);
    const auto current =
        tiles_->currentItem() != nullptr ? tiles_->currentItem()->data(Qt::UserRole) : QVariant();
    tiles_->clear();

    struct Shown {
        SectionKind layer;
        int value;
        QString entry;
        QString brush;
    };
    std::vector<Shown> shown;
    bool editorGroup = false;
    for (const auto &group : editorGroups(profile_)) {
        if (text(group.id) != tileGroup_ || group.tiles.empty()) {
            continue;
        }
        editorGroup = true;
        // The empty cell first: it is what erases.
        shown.push_back({group.tiles.front().layer, layout::emptyCell, QString(), QString()});
        for (const auto &entry : group.tiles) {
            for (std::size_t index = 0; index < entry.values.size(); ++index) {
                shown.push_back({entry.layer, entry.values[index], catalogText(entry.name),
                                 index < entry.brushes.size() ? text(entry.brushes[index])
                                                              : QString()});
            }
        }
    }
    if (!editorGroup) {
        auto values = allTileValues(info, layer);
        std::set<int> grouped;
        for (const auto &group : editorGroups(profile_)) {
            for (const auto &entry : group.tiles) {
                if (entry.layer == layer) {
                    grouped.insert(entry.values.begin(), entry.values.end());
                }
            }
        }
        std::erase_if(values, [&](int value) {
            if (grouped.contains(value)) {
                return tileGroup_ == QLatin1String(otherForegroundGroup);
            }
            // Foreground 347 is both a static slime border and a picture of the spike trap.
            if (layer == SectionKind::foreground && value == 347) {
                return false;
            }
            return isTileAnimationFrame(profile_, layer, static_cast<std::int16_t>(value));
        });
        // With artwork, leave out numbers whose picture is missing or
        // blank (they can still be typed as a raw tile ID). If the artwork
        // has nothing for this layer at all, keep the numbered swatches.
        if (artwork != nullptr) {
            std::vector<int> visible;
            for (const int value : values) {
                if (tileHasVisibleArtwork(layer, value, artwork)) {
                    visible.push_back(value);
                }
            }
            if (!visible.empty()) {
                values = std::move(visible);
            }
        }
        shown.push_back({layer, layout::emptyCell, QString(), QString()});
        for (const int value : values) {
            shown.push_back({layer, value, QString(), QString()});
        }
    }
    for (const auto &tile : shown) {
        auto *item = new QListWidgetItem(
            QIcon(tileThumbnail(tile.layer, tile.value, artwork, tileThumbnailSize)), QString(),
            tiles_);
        item->setData(Qt::UserRole, tile.value);
        item->setData(layerRole, static_cast<int>(tile.layer));
        item->setData(brushRole, tile.brush);
        item->setSizeHint(tiles_->gridSize());
        const bool big = tile.layer == SectionKind::bigDecor;
        auto tip = tile.value == layout::emptyCell
                       ? tr("Empty cell (-1)")
                       : big ? tr("bigDecor %1").arg(tile.value) : tr("Tile %1").arg(tile.value);
        if (!tile.entry.isEmpty()) {
            tip = tr("%1 - %2").arg(tile.entry, tip);
        }
        if (!tile.brush.isEmpty()) {
            tip += tr("\nPainted with the adaptive brush, which fits it to its neighbours.");
        }
        item->setToolTip(tip);
    }
    if (current.isValid()) {
        selectByData(tiles_, current);
    }
}

void PalettePanel::rebuildObjectGroups() {
    const auto *artwork = artwork_.get();
    const QSignalBlocker blocker(objectGroups_);
    objectGroups_->clear();
    const auto presets = objectPresets(profile_);
    const auto add = [&](const QString &id, const QString &name, const ObjectPreset *picture,
                         const QString &tip) {
        const auto pixmap =
            picture != nullptr ? objectThumbnail(picture->type, picture->channel, picture->icon,
                                                 artwork, groupThumbnailSize)
                               : tileThumbnail(SectionKind::foreground, layout::emptyCell, nullptr,
                                               groupThumbnailSize);
        auto *item = new QListWidgetItem(QIcon(pixmap), name, objectGroups_);
        item->setData(Qt::UserRole, id);
        item->setSizeHint(objectGroups_->gridSize());
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        item->setToolTip(tip);
    };
    if (!presets.empty()) {
        add(QString::fromLatin1(allObjectsGroup), tr("All objects"), &presets.front(),
            tr("Every object the game's own editor can place"));
    }
    for (const auto &group : editorGroups(profile_)) {
        if (group.presets.empty()) {
            continue;
        }
        const auto *first = findObjectPreset(profile_, group.presets.front());
        add(text(group.id), catalogText(group.name), first,
            tr("%1: %2 object(s), as one button of the game's own editor offers them")
                .arg(catalogText(group.name))
                .arg(group.presets.size()));
    }
    if (!hasData(objectGroups_, objectGroup_)) {
        objectGroup_ = QString::fromLatin1(allObjectsGroup);
    }
    selectByData(objectGroups_, objectGroup_);
    objectGroups_->setVisible(objectGroups_->count() > 1);
    fitStrip(objectGroups_);
}

void PalettePanel::rebuildObjects() {
    const auto *artwork = artwork_.get();
    const QSignalBlocker blocker(objects_);
    objects_->clear();
    appearanceId_.clear();
    appearance_->hide();
    std::vector<const ObjectPreset *> shown;
    bool editorGroup = false;
    for (const auto &group : editorGroups(profile_)) {
        if (text(group.id) == objectGroup_ && !group.presets.empty()) {
            editorGroup = true;
            for (const auto id : group.presets) {
                if (const auto *preset = findObjectPreset(profile_, id)) {
                    shown.push_back(preset);
                }
            }
        }
    }
    if (!editorGroup) {
        for (const auto &preset : objectPresets(profile_)) {
            shown.push_back(&preset);
        }
    }
    for (const auto *preset : shown) {
        auto *item = new QListWidgetItem(
            QIcon(objectThumbnail(preset->type, preset->channel, preset->icon, artwork,
                                  objectThumbnailSize)),
            catalogText(preset->name), objects_);
        item->setData(Qt::UserRole, text(preset->id));
        item->setSizeHint(objects_->gridSize());
        item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
        item->setToolTip(tr("%1\nStored type %2. Click, then click a cell to place it%3.")
                             .arg(catalogText(preset->name))
                             .arg(preset->type)
                             .arg(preset->hasPath ? tr("; drag to set its path") : QString()));
    }
    setCurrentPreset(currentPresetId_, currentAppearance_);
}

void PalettePanel::rebuild() {
    built_ = true;
    const auto *artwork = artwork_.get();
    const auto *info = profileInfo(profile_);

    if (!hasWorld_) {
        hint_->setText(tr("Open a world to see its tiles, objects and border brushes here."));
        hint_->show();
        chooseDirectory_->hide();
    } else if (info == nullptr) {
        hint_->setText(tr("Choose the game in the tool bar to see its objects, its editor's "
                          "groups and its border brushes. Tiles are shown as numbered swatches."));
        hint_->show();
        chooseDirectory_->hide();
    } else if (artwork == nullptr) {
        hint_->setText(tr("Pictures are schematic: no game graphics are loaded for this game. "
                          "The editor ships none; it can read them from a game you have."));
        hint_->show();
        chooseDirectory_->show();
    } else {
        hint_->hide();
        chooseDirectory_->hide();
    }

    rebuildTileGroups();
    rebuildTiles();
    rebuildObjectGroups();
    rebuildObjects();

    // Adaptive border brushes of the chosen game.
    {
        const QSignalBlocker blocker(brushes_);
        const auto current = brushes_->currentItem() != nullptr
                                 ? brushes_->currentItem()->data(Qt::UserRole)
                                 : QVariant();
        brushes_->clear();
        for (const auto &family : adaptiveFamilies(profile_)) {
            auto *item = new QListWidgetItem(
                QIcon(tileThumbnail(SectionKind::foreground, family.seed, artwork,
                                    objectThumbnailSize)),
                catalogText(family.name), brushes_);
            item->setData(Qt::UserRole, text(family.id));
            item->setSizeHint(brushes_->gridSize());
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
            item->setToolTip(tr("%1\nPaints tile %2 and recomputes the border forms around it.")
                                 .arg(catalogText(family.name))
                                 .arg(family.seed));
        }
        if (current.isValid()) {
            selectByData(brushes_, current);
        }
    }
    rebuildWorld();
    tabs_->setTabEnabled(1, objects_->count() > 0);
    tabs_->setTabEnabled(2, brushes_->count() > 0);
}

} // namespace eggbert
