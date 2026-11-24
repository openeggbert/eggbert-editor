// SPDX-License-Identifier: GPL-3.0-only
#include "app/doctor_texts.hpp"
#include "app/encyclopedia_dialog.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include "world/adaptive.hpp"
#include "world/animation.hpp"
#include "world/object_animation_data.hpp"
#include "world/object_catalog.hpp"
#include "world/profile.hpp"
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "app/catalog_texts.hpp"
#include "app/game_assets.hpp"
#include "app/sprite_icons.hpp"

namespace eggbert {

namespace {

constexpr int thumbnailSize = 40;
enum Category { catalog, foreground, backLayer, elements, player1, player2, player3, player4,
                runtime, backdrops };
enum VariantAction { frameOnly, paintValue, placePicture, placeChannel };
constexpr int sheetChannels[] = {1, 9, 10, 2, 11, 12, 13};

QString text(const std::string &value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

std::vector<std::int16_t> tileForms(GameProfile profile, const EncyclopediaEntry &entry) {
    auto forms = entry.values;
    auto appendFamily = [&](const std::string &id) {
        if (const auto *family = findAdaptiveFamily(profile, id)) {
            const auto values = borderVariants(profile, *family);
            forms.insert(forms.end(), values.begin(), values.end());
        }
    };
    if (entry.kind == EntryKind::border) {
        appendFamily(entry.id);
    } else {
        for (const auto &id : entry.brushes) {
            appendFamily(id);
        }
    }
    return forms;
}

int tileChannel(SectionKind layer, int value) {
    return layer == SectionKind::bigDecor && value != bigDecorAnimatedPlant ? 9 : 1;
}

} // namespace

EncyclopediaDialog::EncyclopediaDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Element Encyclopedia"));
    setObjectName(QStringLiteral("encyclopediaDialog"));
    auto *layout = new QVBoxLayout(this);
    category_ = new QComboBox(this);
    category_->setObjectName("encyclopediaCategory");
    category_->setAccessibleName(tr("Browse collection"));
    category_->addItems({tr("Named elements and brushes"), tr("All foreground tiles"),
                         tr("All back-layer tiles"), tr("Object sprite sheet"),
                         tr("Player 1 sprite sheet"), tr("Player 2 sprite sheet"),
                         tr("Player 3 sprite sheet"), tr("Player 4 sprite sheet"),
                         tr("Objects created during play"), tr("World backdrops")});
    layout->addWidget(category_);
    search_ = new QLineEdit(this);
    search_->setObjectName(QStringLiteral("encyclopediaSearch"));
    search_->setPlaceholderText(tr("Search by name, group or number"));
    search_->setClearButtonEnabled(true);
    search_->setAccessibleName(tr("Search the encyclopedia"));
    layout->addWidget(search_);
    count_ = new QLabel(this);
    layout->addWidget(count_);

    auto *columns = new QHBoxLayout;
    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("encyclopediaList"));
    list_->setIconSize(QSize(thumbnailSize, thumbnailSize));
    list_->setUniformItemSizes(true);
    list_->setAccessibleName(tr("Elements"));
    columns->addWidget(list_, 1);

    auto *side = new QVBoxLayout;
    details_ = new QLabel(this);
    details_->setObjectName(QStringLiteral("encyclopediaDetails"));
    details_->setWordWrap(true);
    details_->setTextFormat(Qt::RichText);
    details_->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
    details_->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(details_);
    scroll->setFrameShape(QFrame::NoFrame);
    side->addWidget(scroll, 1);
    backdrop_ = new QLabel(this);
    backdrop_->setAlignment(Qt::AlignCenter);
    side->addWidget(backdrop_, 2);
    variants_ = new QListWidget(this);
    variants_->setObjectName("encyclopediaVariants");
    variants_->setAccessibleName(tr("Pictures and variants"));
    variants_->setViewMode(QListView::IconMode);
    variants_->setResizeMode(QListView::Adjust);
    variants_->setMovement(QListView::Static);
    variants_->setIconSize(QSize(52, 52));
    variants_->setGridSize(QSize(84, 82));
    side->addWidget(variants_, 2);
    select_ = new QPushButton(tr("Select in Editor"), this);
    select_->setObjectName(QStringLiteral("encyclopediaSelect"));
    select_->setToolTip(tr("Chooses this element as the Palette does: the tile for the Pencil, "
                           "the object for Place, the family for the Adaptive brush."));
    side->addWidget(select_);
    columns->addLayout(side, 1);
    layout->addLayout(columns, 1);

    connect(category_, &QComboBox::currentIndexChanged, this, [this] { rebuildEntries(); });
    connect(variants_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        if (item) {
            select_->setEnabled(item->data(Qt::UserRole).toInt() != frameOnly);
        }
    });
    connect(variants_, &QListWidget::itemActivated, this, [this] { chooseCurrent(); });
    connect(search_, &QLineEdit::textChanged, this, [this] { refill(); });
    connect(list_, &QListWidget::currentRowChanged, this, [this] { showCurrent(); });
    connect(list_, &QListWidget::itemClicked, this, [this] {
        if (variants_->currentRow() >= 0) {
            showCurrent();
        }
    });
    connect(list_, &QListWidget::itemActivated, this, [this] {
        variants_->setCurrentRow(-1);
        chooseCurrent();
    });
    connect(select_, &QPushButton::clicked, this, [this] { chooseCurrent(); });
    resize(930, 640);
    refill();
}

void EncyclopediaDialog::setGame(GameProfile profile, std::shared_ptr<const GameAssets> artwork) {
    // The rows index into entries_: drop them before the entries change.
    list_->clear();
    profile_ = profile;
    artwork_ = std::move(artwork);
    rebuildEntries();
}

void EncyclopediaDialog::rebuildEntries() {
    list_->clear();
    entries_.clear();
    const int category = category_->currentIndex();
    const auto *info = profileInfo(profile_);
    if (category == catalog) {
        entries_ = encyclopedia(profile_);
    } else if (info && category == runtime) {
        for (const auto &type : runtimeTypes(profile_)) {
            EncyclopediaEntry e;
            e.kind = EntryKind::object;
            e.title = type.name;
            e.type = type.type;
            e.group = category_->currentText().toStdString();
            e.provenance = type.sourceName;
            e.facts = tr("Runtime object type %1.").arg(type.type).toStdString();
            e.icon = -1;
            if (profile_ == GameProfile::mobileEggbert && e.type == 58) {
                e.channel = 2;
                e.icon = 0;
                e.facts += " " + tr("The trail copies Blupi's current pose.").toStdString();
            }
            for (const auto &rule : objectIconRules(profile_)) {
                if (rule.type == type.type && rule.channel >= 0 && !rule.frames.empty()) {
                    e.channel = rule.channel;
                    e.icon = rule.frames.front();
                    e.provenance += " " + std::string(rule.provenance);
                    break;
                }
            }
            entries_.push_back(std::move(e));
        }
    } else if (info) {
        const bool backdrop = category == backdrops;
        const bool tiles = category == foreground || category == backLayer;
        const int channel = backdrop ? 0 : sheetChannels[category - foreground];
        const auto sheet = GameAssets::sheetForChannel(channel);
        const auto actual = artwork_ && sheet ? artwork_->spriteCount(*sheet) : 0;
        const int count = backdrop ? info->region.maximum + 1
            : actual ? static_cast<int>(actual) : GameAssets::expectedSpriteCount(profile_, *sheet);
        const auto group = category_->currentText();
        std::map<std::pair<int, int>, QStringList> names;
        auto namePicture = [&](int sheetChannel, int picture, const QString &title) {
            auto &labels = names[{sheetChannel, picture}];
            if (!labels.contains(title)) {
                labels << title;
            }
        };
        if (!backdrop) {
            for (const auto &entry : encyclopedia(profile_)) {
                const auto title = catalogText(entry.title);
                if (entry.kind == EntryKind::object) {
                    namePicture(entry.channel, stillObjectIcon(profile_, entry.type, entry.channel, entry.icon), title);
                } else {
                    for (const auto value : tileForms(profile_, entry)) {
                        const int source = tileChannel(entry.layer, value);
                        namePicture(source, value, title);
                        for (const auto frame : tilePictures(profile_, entry.layer, value)) {
                            namePicture(source, frame, title);
                        }
                    }
                }
            }
            for (const auto &rule : objectIconRules(profile_)) {
                const auto info = objectTypeInfo(profile_, rule.type);
                if (info.name.empty()) {
                    continue;
                }
                for (const auto frame : rule.frames) {
                    namePicture(rule.channel, frame, catalogText(info.name));
                }
            }
        }
        auto add = [&](int id) {
            EncyclopediaEntry e;
            e.kind = tiles ? EntryKind::tile : EntryKind::picture;
            e.layer = category == backLayer ? SectionKind::bigDecor : SectionKind::foreground;
            e.values.push_back(static_cast<std::int16_t>(id));
            e.channel = static_cast<std::int16_t>(id == 203 && category == backLayer ? 1 : channel);
            e.icon = static_cast<std::int16_t>(id);
            e.group = group.toStdString();
            const auto label = names.find({e.channel, id});
            e.title = QStringLiteral("%1 #%2").arg(label == names.end() ? group : label->second.join(", ")).arg(id).toStdString();
            e.facts = backdrop ? tr("Backdrop %1").arg(id).toStdString()
                               : tr("Sprite channel %1, picture %2.").arg(e.channel).arg(id).toStdString();
            e.provenance = backdrop ? info->region.provenance :
                tiles ? (category == foreground ? info->foregroundAtlas.provenance : info->bigDecorAtlas.provenance)
                      : std::string_view("GameAssets: sprite sheet");
            entries_.push_back(std::move(e));
        };
        for (int id = 0; id < count; ++id) {
            add(id);
        }
        if (category == backLayer && count <= bigDecorAnimatedPlant) {
            add(bigDecorAnimatedPlant);
        }
    }
    refill();
}

const EncyclopediaEntry *EncyclopediaDialog::currentEntry() const {
    const auto *item = list_->currentItem();
    if (item == nullptr) {
        return nullptr;
    }
    const auto index = static_cast<std::size_t>(item->data(Qt::UserRole).toULongLong());
    return index < entries_.size() ? &entries_[index] : nullptr;
}

void EncyclopediaDialog::refill() {
    const auto *previous = currentEntry();
    const auto previousIndex =
        previous == nullptr ? entries_.size() : static_cast<std::size_t>(previous - entries_.data());
    list_->clear();
    const auto query = search_->text().simplified();
    const auto words = query.split(' ', Qt::SkipEmptyParts);
    auto matches = searchEncyclopedia(entries_, query.toStdString());
    // The core catalog is in English. Also match the names displayed in the current language.
    if (!words.empty()) {
        for (std::size_t i = 0; i < entries_.size(); ++i) {
            const auto &e = entries_[i];
            const QString local = catalogText(e.title) + " " + catalogText(e.group);
            if (std::ranges::all_of(words, [&](const QString &word) {
                    bool numeric = false;
                    const int n = word.toInt(&numeric);
                    return numeric ? (e.kind == EntryKind::object ? e.type == n :
                        std::ranges::find(e.values, n) != e.values.end()) : local.contains(word, Qt::CaseInsensitive);
                })) {
                matches.push_back(i);
            }
        }
        std::ranges::sort(matches);
        matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
    }
    for (const auto index : matches) {
        const auto &entry = entries_[index];
        QPixmap icon;
        if (category_->currentIndex() == backdrops) {
            const auto image = artwork_ ? artwork_->readBackdrop(entry.icon) : QImage{};
            if (!image.isNull()) {
                icon = QPixmap::fromImage(image.scaled(thumbnailSize, thumbnailSize, Qt::KeepAspectRatio));
            }
        } else if (entry.kind == EntryKind::picture) {
            icon = spriteThumbnail(entry.channel, entry.icon, artwork_.get(), thumbnailSize);
        } else {
            icon = entry.kind == EntryKind::object
                ? objectThumbnail(entry.type, entry.channel, entry.icon, artwork_.get(),
                                  thumbnailSize)
                : tileThumbnail(entry.layer, entry.values.front(), artwork_.get(), thumbnailSize);
        }
        auto *item = new QListWidgetItem(QIcon(icon), catalogText(entry.title), list_);
        item->setData(Qt::UserRole, static_cast<qulonglong>(index));
        item->setToolTip(catalogText(entry.group));
        if (index == previousIndex) {
            list_->setCurrentItem(item);
        }
    }
    if (list_->currentItem() == nullptr && list_->count() > 0) {
        list_->setCurrentRow(0);
    }
    if (profile_ == GameProfile::unknown) {
        count_->setText(tr("Choose the game in the tool bar to see what its worlds can contain."));
    } else {
        count_->setText(tr("%1 of %2 entries").arg(matches.size()).arg(entries_.size()));
    }
    showCurrent();
}

void EncyclopediaDialog::showCurrent() {
    const auto *entry = currentEntry();
    variants_->clear();
    variants_->hide();
    backdrop_->hide();
    if (entry && category_->currentIndex() == backdrops && artwork_) {
        const auto picture = artwork_->backdrop(entry->icon);
        if (picture && !picture->isNull()) {
            backdrop_->setPixmap(QPixmap::fromImage(picture->scaled(340, 240, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
            backdrop_->show();
        }
    }
    select_->setEnabled(entry && entry->kind != EntryKind::picture &&
                        (entry->kind != EntryKind::object || !entry->id.empty()));
    if (entry == nullptr) {
        details_->setText(QString());
        return;
    }
    const auto kind = entry->kind == EntryKind::tile     ? tr("Tile")
                      : entry->kind == EntryKind::object ? tr("Object")
                                                         : entry->kind == EntryKind::border ? tr("Border family") : tr("Picture");
    details_->setText(
        QStringLiteral("<h3>%1</h3><p><b>%2</b> &middot; %3</p><p>%4</p><p><b>%5</b><br>%6</p>")
            .arg(catalogText(entry->title).toHtmlEscaped(), kind,
                 catalogText(entry->group).toHtmlEscaped(),
                 encyclopediaFactsText(*entry).toHtmlEscaped(), tr("Evidence"),
                 text(entry->provenance).toHtmlEscaped()));
    showVariants(*entry);
}

void EncyclopediaDialog::showVariants(const EncyclopediaEntry &entry) {
    std::set<std::tuple<int, int, int>> seen;
    auto add = [&](int channel, int id, VariantAction action) {
        if (id < 0 || !seen.emplace(channel, id, action).second) {
            return;
        }
        const auto label = action == placeChannel ? tr("Player %1").arg(channel - 9)
                         : action == frameOnly ? tr("Frame %1").arg(id) : tr("Tile %1").arg(id);
        const auto image = spriteThumbnail(channel, id, artwork_.get(), 52);
        auto *item = new QListWidgetItem(QIcon(image), label, variants_);
        item->setData(Qt::UserRole, static_cast<int>(action));
        item->setData(Qt::UserRole + 1, action == placeChannel ? channel : id);
        item->setToolTip(tr("Sprite channel %1, picture %2.").arg(channel).arg(id));
    };
    if (entry.kind == EntryKind::tile || entry.kind == EntryKind::border) {
        for (const auto value : tileForms(profile_, entry)) {
            const int channel = tileChannel(entry.layer, value);
            add(channel, value, paintValue);
            for (const auto frame : tilePictures(profile_, entry.layer, value)) {
                if (frame != value) {
                    add(channel, frame, frameOnly);
                }
            }
        }
    } else if (entry.kind == EntryKind::object) {
        const auto *preset = findObjectPreset(profile_, entry.id);
        if (preset && preset->iconIsChoice) {
            const int count = profileInfo(profile_)->foregroundAtlas.maximum + 1;
            for (int i = 0; i < count; ++i) {
                add(1, i, placePicture);
            }
        } else {
            if (preset && preset->hasColourVariants()) {
                for (int channel = 11; channel <= 13; ++channel) {
                    add(channel, preset->icon, placeChannel);
                }
            }
            for (const auto &rule : objectIconRules(profile_)) {
                if (rule.type == entry.type) {
                    for (const auto frame : rule.frames) {
                        add(rule.channel < 0 ? entry.channel : rule.channel, frame, frameOnly);
                    }
                }
            }
            if (seen.empty()) {
                add(entry.channel, entry.icon, frameOnly);
            }
        }
    } else if (category_->currentIndex() != backdrops) {
        add(entry.channel, entry.icon, frameOnly);
    }
    variants_->setVisible(variants_->count() > 0);
}

void EncyclopediaDialog::chooseCurrent() {
    const auto *entry = currentEntry();
    if (entry == nullptr) {
        return;
    }
    if (auto *item = variants_->currentItem()) {
        const int id = item->data(Qt::UserRole + 1).toInt();
        switch (item->data(Qt::UserRole).toInt()) {
        case paintValue: emit tileChosen(static_cast<int>(entry->layer), id); break;
        case placePicture: emit presetPictureChosen(text(entry->id), id); break;
        case placeChannel: emit presetChannelChosen(text(entry->id), id); break;
        }
        return;
    }
    switch (entry->kind) {
    case EntryKind::picture:
        break;
    case EntryKind::tile:
        if (!entry->brushes.empty()) {
            emit brushChosen(text(entry->brushes.front()));
        } else {
            emit tileChosen(static_cast<int>(entry->layer), entry->values.front());
        }
        break;
    case EntryKind::object:
        if (!entry->id.empty()) {
            emit presetChosen(text(entry->id));
        }
        break;
    case EntryKind::border:
        emit brushChosen(text(entry->id));
        break;
    }
}

} // namespace eggbert
