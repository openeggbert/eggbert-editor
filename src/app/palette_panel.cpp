// SPDX-License-Identifier: GPL-3.0-only
#include "palette_panel.hpp"

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QVBoxLayout>

#include "app/document_tab.hpp"
#include "app/game_assets.hpp"
#include "app/sprite_icons.hpp"
#include "world/adaptive.hpp"
#include "world/object_catalog.hpp"
#include "world/profile.hpp"

namespace eggbert {

namespace {

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

} // namespace

PalettePanel::PalettePanel(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    hint_ = new QLabel(this);
    hint_->setObjectName(QStringLiteral("paletteHint"));
    hint_->setWordWrap(true);
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
    tiles_ = makeGrid(tabs_, tileThumbnailSize, QSize(tileThumbnailSize + 8, tileThumbnailSize + 8));
    tiles_->setObjectName(QStringLiteral("paletteTiles"));
    objects_ = makeGrid(tabs_, objectThumbnailSize, QSize(112, objectThumbnailSize + 52));
    objects_->setObjectName(QStringLiteral("paletteObjects"));
    brushes_ = makeGrid(tabs_, objectThumbnailSize, QSize(112, objectThumbnailSize + 52));
    brushes_->setObjectName(QStringLiteral("paletteBrushes"));
    tabs_->addTab(tiles_, tr("Tiles"));
    tabs_->addTab(objects_, tr("Objects"));
    tabs_->addTab(brushes_, tr("Borders"));
    layout->addWidget(tabs_, 1);
    setMinimumWidth(260);

    connect(tiles_, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        emit tileChosen(item->data(Qt::UserRole).toInt());
    });
    connect(tiles_, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        emit tileChosen(item->data(Qt::UserRole).toInt());
    });
    connect(objects_, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        emit presetChosen(item->data(Qt::UserRole).toString());
    });
    connect(objects_, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        emit presetChosen(item->data(Qt::UserRole).toString());
    });
    connect(brushes_, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        emit brushChosen(item->data(Qt::UserRole).toString());
    });
    connect(brushes_, &QListWidget::itemActivated, this, [this](QListWidgetItem *item) {
        emit brushChosen(item->data(Qt::UserRole).toString());
    });
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
    profile_ = profile;
    layer_ = layer;
    artwork_ = std::move(artwork);
    rebuild();
}

void PalettePanel::setCurrentTile(int value) {
    selectByData(tiles_, value);
}

void PalettePanel::setCurrentPreset(const QString &id) {
    selectByData(objects_, id);
}

void PalettePanel::setCurrentBrush(const QString &id) {
    selectByData(brushes_, id);
}

void PalettePanel::setEditable(bool editable) {
    tabs_->setEnabled(editable);
}

void PalettePanel::rebuild() {
    built_ = true;
    const auto *artwork = artwork_.get();
    const auto *info = profileInfo(profile_);

    if (info == nullptr) {
        hint_->setText(tr("Choose the game in the tool bar to see its objects and border "
                          "brushes. Tiles are shown as numbered swatches."));
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

    // Tiles of the active layer: the empty cell, then every number the
    // chosen game has a picture for (a generic range without a game).
    {
        const QSignalBlocker blocker(tiles_);
        const auto current = tiles_->currentItem() != nullptr
                                 ? tiles_->currentItem()->data(Qt::UserRole)
                                 : QVariant();
        tiles_->clear();
        std::vector<int> values{layout::emptyCell};
        const bool big = layer_ == SectionKind::bigDecor;
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
        // With artwork, leave out numbers whose picture is missing or
        // blank (they can still be typed as a raw tile ID). If the artwork
        // has nothing for this layer at all, keep the numbered swatches.
        if (artwork != nullptr) {
            std::vector<int> visible{layout::emptyCell};
            for (const int value : values) {
                if (value != layout::emptyCell && tileHasVisibleArtwork(layer_, value, artwork)) {
                    visible.push_back(value);
                }
            }
            if (visible.size() > 1) {
                values = std::move(visible);
            }
        }
        for (const int value : values) {
            auto *item = new QListWidgetItem(
                QIcon(tileThumbnail(layer_, value, artwork, tileThumbnailSize)), QString(), tiles_);
            item->setData(Qt::UserRole, value);
            item->setSizeHint(tiles_->gridSize());
            item->setToolTip(value == layout::emptyCell
                                 ? tr("Empty cell (-1)")
                                 : big ? tr("bigDecor %1").arg(value) : tr("Tile %1").arg(value));
        }
        if (current.isValid()) {
            selectByData(tiles_, current);
        }
    }

    // Verified object presets of the chosen game.
    {
        const QSignalBlocker blocker(objects_);
        const auto current = objects_->currentItem() != nullptr
                                 ? objects_->currentItem()->data(Qt::UserRole)
                                 : QVariant();
        objects_->clear();
        for (const auto &preset : objectPresets(profile_)) {
            auto *item = new QListWidgetItem(
                QIcon(objectThumbnail(preset.type, preset.channel, preset.icon, artwork,
                                      objectThumbnailSize)),
                text(preset.name), objects_);
            item->setData(Qt::UserRole, text(preset.id));
            item->setSizeHint(objects_->gridSize());
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
            item->setToolTip(tr("%1\nStored type %2. Click, then click a cell to place it%3.")
                                 .arg(text(preset.name))
                                 .arg(preset.type)
                                 .arg(preset.hasPath ? tr("; drag to set its path") : QString()));
        }
        if (current.isValid()) {
            selectByData(objects_, current);
        }
    }

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
                text(family.name), brushes_);
            item->setData(Qt::UserRole, text(family.id));
            item->setSizeHint(brushes_->gridSize());
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
            item->setToolTip(tr("%1\nPaints tile %2 and recomputes the border forms around it.")
                                 .arg(text(family.name))
                                 .arg(family.seed));
        }
        if (current.isValid()) {
            selectByData(brushes_, current);
        }
    }
    tabs_->setTabEnabled(1, objects_->count() > 0);
    tabs_->setTabEnabled(2, brushes_->count() > 0);
}

} // namespace eggbert
