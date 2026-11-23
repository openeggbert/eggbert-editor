// SPDX-License-Identifier: GPL-3.0-only
#include "metadata_dialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <limits>

#include "app/game_assets.hpp"
#include "app/music_player.hpp"
#include "app/world_renderer.hpp"

namespace eggbert {

namespace {

constexpr QSize backgroundPicture(96, 72);

void selectNumber(QListWidget *list, int number) {
    const QSignalBlocker blocker(list);
    list->setCurrentItem(nullptr);
    for (int row = 0; row < list->count(); ++row) {
        if (list->item(row)->data(Qt::UserRole).toInt() == number) {
            list->setCurrentRow(row);
            list->scrollToItem(list->item(row));
        }
    }
}

} // namespace

QString decodeTitle(std::span<const std::byte> bytes) {
    return QString::fromStdU32String(decodeWindows1252(bytes));
}

std::optional<std::vector<std::byte>> encodeTitle(const QString &text) {
    return encodeWindows1252(text.toStdU32String());
}

MetadataDialog::MetadataDialog(const WorldView &world, GameProfile profile, QWidget *parent)
    : QDialog(parent), original_(readMetadata(world).value_or(Metadata{})), profile_(profile) {
    setWindowTitle(tr("World Metadata"));
    // The games copy the title only from revision 1.3 files.
    titleRead_ = world.probe().candidateRevision == FormatRevision::v1_3;

    auto *layout = new QVBoxLayout(this);
    pages_ = new QTabWidget(this);
    layout->addWidget(pages_);

    auto *worldPageWidget = new QWidget(pages_);
    auto *form = new QFormLayout(worldPageWidget);
    name_ = new QLineEdit(decodeTitle(original_.name), worldPageWidget);
    name_->setObjectName(QStringLiteral("nameEdit"));
    form->addRow(tr("Title"), name_);

    const auto makeSpin = [](QWidget *parent, const QString &name, int minimum, int maximum,
                             int value) {
        auto *spin = new QSpinBox(parent);
        spin->setObjectName(name);
        spin->setRange(minimum, maximum);
        spin->setValue(value);
        return spin;
    };
    const auto *info = profileInfo(profile_);
    music_ = makeSpin(worldPageWidget, QStringLiteral("musicSpin"), -32768, 32767,
                      original_.music);
    region_ = makeSpin(worldPageWidget, QStringLiteral("regionSpin"), -32768, 32767,
                       original_.region);
    music_->setEnabled(info != nullptr);
    region_->setEnabled(info != nullptr);
    form->addRow(info != nullptr ? tr("Music (%1 to %2, 0 is none)")
                                       .arg(info->music.minimum)
                                       .arg(info->music.maximum)
                                 : tr("Music (choose a game profile to edit)"),
                 music_);
    // The same choice by ear: the list, and a way to listen.
    musicList_ = new QListWidget(worldPageWidget);
    musicList_->setObjectName(QStringLiteral("metadataMusicList"));
    musicList_->setAccessibleName(tr("Music"));
    musicList_->setMaximumHeight(118);
    form->addRow(musicList_);
    {
        auto *row = new QHBoxLayout;
        play_ = new QPushButton(tr("Play"), worldPageWidget);
        play_->setObjectName(QStringLiteral("metadataMusicPlay"));
        stop_ = new QPushButton(tr("Stop"), worldPageWidget);
        stop_->setObjectName(QStringLiteral("metadataMusicStop"));
        musicLine_ = new QLabel(worldPageWidget);
        musicLine_->setObjectName(QStringLiteral("metadataMusicLine"));
        musicLine_->setWordWrap(true);
        row->addWidget(play_);
        row->addWidget(stop_);
        row->addWidget(musicLine_, 1);
        form->addRow(row);
    }
    form->addRow(info != nullptr ? tr("Region / background (%1 to %2)")
                                       .arg(info->region.minimum)
                                       .arg(info->region.maximum)
                                 : tr("Region (choose a game profile to edit)"),
                 region_);
    // The same choice by picture.
    backgrounds_ = new QListWidget(worldPageWidget);
    backgrounds_->setObjectName(QStringLiteral("metadataBackgroundList"));
    backgrounds_->setAccessibleName(tr("Background"));
    backgrounds_->setViewMode(QListView::IconMode);
    backgrounds_->setResizeMode(QListView::Adjust);
    backgrounds_->setMovement(QListView::Static);
    backgrounds_->setIconSize(backgroundPicture);
    backgrounds_->setGridSize(backgroundPicture + QSize(10, 24));
    backgrounds_->setFixedHeight(2 * (backgroundPicture.height() + 24) + 8);
    form->addRow(backgrounds_);

    scrollX_ = new QCheckBox(tr("Scrolls horizontally (otherwise one screen, 10 cells wide)"),
                             worldPageWidget);
    scrollX_->setObjectName(QStringLiteral("scrollXCheck"));
    scrollX_->setChecked(original_.scrollsHorizontally);
    scrollY_ = new QCheckBox(tr("Scrolls vertically (otherwise one screen, 480 pixels high)"),
                             worldPageWidget);
    scrollY_->setObjectName(QStringLiteral("scrollYCheck"));
    scrollY_->setChecked(original_.scrollsVertically);
    form->addRow(scrollX_);
    form->addRow(scrollY_);

    constexpr int intMin = std::numeric_limits<int>::min();
    constexpr int intMax = std::numeric_limits<int>::max();
    posX_ = makeSpin(worldPageWidget, QStringLiteral("posXSpin"), intMin, intMax,
                     original_.posDecor.x);
    posY_ = makeSpin(worldPageWidget, QStringLiteral("posYSpin"), intMin, intMax,
                     original_.posDecor.y);
    form->addRow(tr("Saved scroll position x (pixels)"), posX_);
    form->addRow(tr("Saved scroll position y (pixels)"), posY_);
    pages_->addTab(worldPageWidget, tr("World"));

    auto *startsPageWidget = new QWidget(pages_);
    auto *grid = new QGridLayout(startsPageWidget);
    grid->addWidget(new QLabel(tr("Start"), startsPageWidget), 0, 0);
    grid->addWidget(new QLabel(tr("x (pixels)"), startsPageWidget), 0, 1);
    grid->addWidget(new QLabel(tr("y (pixels)"), startsPageWidget), 0, 2);
    grid->addWidget(new QLabel(tr("Direction"), startsPageWidget), 0, 3);
    for (int index = 0; index < 4; ++index) {
        const auto &start = original_.starts[static_cast<std::size_t>(index)];
        grid->addWidget(new QLabel(QString::number(index + 1), startsPageWidget), index + 1, 0);
        startX_[index] = makeSpin(startsPageWidget, QStringLiteral("start%1X").arg(index + 1),
                                  intMin, intMax, start.position.x);
        startY_[index] = makeSpin(startsPageWidget, QStringLiteral("start%1Y").arg(index + 1),
                                  intMin, intMax, start.position.y);
        auto *direction = new QComboBox(startsPageWidget);
        direction->setObjectName(QStringLiteral("start%1Direction").arg(index + 1));
        direction->addItem(tr("Left (1)"), startDirectionLeft);
        direction->addItem(tr("Right (2)"), startDirectionRight);
        if (start.direction != startDirectionLeft && start.direction != startDirectionRight) {
            // Keep an unusual stored value selectable so it is not lost.
            direction->addItem(tr("Stored value %1 (drawn as right)").arg(start.direction),
                               start.direction);
        }
        direction->setCurrentIndex(direction->findData(start.direction));
        startDirection_[index] = direction;
        grid->addWidget(startX_[index], index + 1, 1);
        grid->addWidget(startY_[index], index + 1, 2);
        grid->addWidget(direction, index + 1, 3);
    }
    auto *hint = new QLabel(
        tr("Positions are the sprite's top-left corner in world pixels. The games' own editors "
           "place a start at cell x 64 + 2, cell y 64 + 6. Single-player games use start 1."),
        startsPageWidget);
    hint->setWordWrap(true);
    grid->addWidget(hint, 5, 0, 1, 4);
    grid->setRowStretch(6, 1);
    pages_->addTab(startsPageWidget, tr("Player Starts"));

    messages_ = new QLabel(this);
    messages_->setObjectName(QStringLiteral("metadataMessages"));
    messages_->setWordWrap(true);
    messages_->setTextFormat(Qt::PlainText);
    layout->addWidget(messages_);

    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons_);

    connect(name_, &QLineEdit::textChanged, this, [this] { refresh(); });
    connect(music_, &QSpinBox::valueChanged, this, [this] {
        followNumbers();
        refresh();
    });
    connect(region_, &QSpinBox::valueChanged, this, [this] {
        followNumbers();
        refresh();
    });
    connect(musicList_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        if (item != nullptr) {
            music_->setValue(item->data(Qt::UserRole).toInt());
        }
    });
    connect(backgrounds_, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *item) {
        if (item != nullptr) {
            region_->setValue(item->data(Qt::UserRole).toInt());
        }
    });
    connect(musicList_, &QListWidget::itemActivated, this, [this] { play_->click(); });
    connect(play_, &QPushButton::clicked, this, [this] {
        if (playMusic_ && music_->value() > 0) {
            playMusic_(music_->value());
        } else if (stopMusic_) {
            stopMusic_();
        }
        musicLine_->setText(musicStatus_ ? musicStatus_() : QString());
    });
    connect(stop_, &QPushButton::clicked, this, [this] {
        if (stopMusic_) {
            stopMusic_();
        }
        musicLine_->setText(musicStatus_ ? musicStatus_() : QString());
    });
    // Whatever was being listened to ends with the dialog.
    connect(this, &QDialog::finished, this, [this] {
        if (stopMusic_) {
            stopMusic_();
        }
    });
    fillLists();
    refresh();
    // Wide enough for five background pictures in a row.
    setMinimumWidth(5 * (backgroundPicture.width() + 10) + 90);
}

void MetadataDialog::setGame(std::shared_ptr<const GameAssets> pictures,
                             std::shared_ptr<const GameAssets> game) {
    pictures_ = std::move(pictures);
    game_ = std::move(game);
    fillLists();
}

void MetadataDialog::setMusicControls(std::function<bool(int)> play, std::function<void()> stop,
                                      std::function<QString()> status) {
    playMusic_ = std::move(play);
    stopMusic_ = std::move(stop);
    musicStatus_ = std::move(status);
    fillLists();
}

void MetadataDialog::fillLists() {
    const auto *info = profileInfo(profile_);
    {
        const QSignalBlocker blocker(backgrounds_);
        backgrounds_->clear();
        for (int region = info != nullptr ? info->region.minimum : 0;
             info != nullptr && region <= info->region.maximum; ++region) {
            QPixmap pixmap(backgroundPicture);
            const auto picture = pictures_ != nullptr ? pictures_->readBackdrop(region) : QImage();
            if (!picture.isNull()) {
                pixmap = QPixmap::fromImage(picture.scaled(backgroundPicture, Qt::IgnoreAspectRatio,
                                                           Qt::SmoothTransformation));
            } else {
                pixmap.fill(schematicBackgroundColor());
                QPainter painter(&pixmap);
                painter.setPen(Qt::white);
                painter.drawText(pixmap.rect(), Qt::AlignCenter, QString::number(region));
            }
            auto *item = new QListWidgetItem(QIcon(pixmap), QString::number(region), backgrounds_);
            item->setData(Qt::UserRole, region);
            item->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
            item->setToolTip(tr("Background %1").arg(region));
        }
    }
    {
        const QSignalBlocker blocker(musicList_);
        musicList_->clear();
        for (int number = info != nullptr ? info->music.minimum : 0;
             info != nullptr && number <= info->music.maximum; ++number) {
            auto *item = new QListWidgetItem(
                MusicPlayer::trackLabel(number, game_ != nullptr ? game_->musicFile(number)
                                                                 : QString()),
                musicList_);
            item->setData(Qt::UserRole, number);
        }
    }
    const bool lists = info != nullptr;
    backgrounds_->setVisible(lists);
    musicList_->setVisible(lists);
    play_->setVisible(lists);
    stop_->setVisible(lists);
    musicLine_->setVisible(lists);
    play_->setEnabled(static_cast<bool>(playMusic_));
    stop_->setEnabled(static_cast<bool>(stopMusic_));
    musicLine_->setText(musicStatus_ ? musicStatus_() : QString());
    followNumbers();
}

void MetadataDialog::followNumbers() {
    selectNumber(musicList_, music_->value());
    selectNumber(backgrounds_, region_->value());
}

void MetadataDialog::setPage(Page page) {
    pages_->setCurrentIndex(page);
}

std::optional<Metadata> MetadataDialog::metadata() const {
    auto result = original_;
    // An untouched title keeps its stored bytes even when they could not be
    // typed (for example a title without a terminator).
    if (name_->text() != decodeTitle(original_.name)) {
        const auto name = encodeTitle(name_->text());
        if (!name || name->size() > storedTitleLimit) {
            return std::nullopt;
        }
        result.name = *name;
    }
    const auto *info = profileInfo(profile_);
    // A changed music or region must be a value the chosen game offers; an
    // unchanged stored value is kept whatever it is.
    if (!experimental_ && music_->value() != original_.music &&
        (info == nullptr || !info->music.contains(music_->value()))) {
        return std::nullopt;
    }
    if (!experimental_ && region_->value() != original_.region &&
        (info == nullptr || !info->region.contains(region_->value()))) {
        return std::nullopt;
    }
    result.music = static_cast<std::int16_t>(music_->value());
    result.region = static_cast<std::int16_t>(region_->value());
    result.scrollsHorizontally = scrollX_->isChecked();
    result.scrollsVertically = scrollY_->isChecked();
    result.posDecor = {posX_->value(), posY_->value()};
    for (std::size_t index = 0; index < 4; ++index) {
        result.starts[index].position = {startX_[index]->value(), startY_[index]->value()};
        result.starts[index].direction = startDirection_[index]->currentData().toInt();
    }
    return result;
}

QString MetadataDialog::messages() const {
    QStringList lines;
    const auto name = encodeTitle(name_->text());
    if (name_->text() == decodeTitle(original_.name) && original_.name.size() > storedTitleLimit) {
        lines << tr("Note: the stored title fills the whole field without a terminator; it is "
                    "kept as it is unless you change it.");
    } else if (!name) {
        lines << tr("The title contains a character that cannot be stored as one Windows-1252 "
                    "byte.");
    } else if (name->size() > storedTitleLimit) {
        lines << tr("The title is %1 bytes; the field holds at most %2.")
                     .arg(name->size())
                     .arg(storedTitleLimit);
    } else if (name->size() > typedTitleLimit) {
        lines << tr("Note: the games let a player type at most %1 characters; this title has %2.")
                     .arg(typedTitleLimit)
                     .arg(name->size());
    }
    if (!titleRead_) {
        lines << tr("Note: this file is not revision 1.3, so the games ignore its stored title.");
    }
    const auto *info = profileInfo(profile_);
    if (info == nullptr) {
        lines << tr("Music and region depend on the game: choose a game profile to edit them.");
    } else {
        if (!info->music.contains(music_->value())) {
            lines << (music_->value() == original_.music
                          ? tr("Note: the stored music %1 is outside %2 to %3 and is kept.")
                          : experimental_
                                ? tr("Experimental: music %1 is outside the verified range %2 "
                                     "to %3 and is written as typed.")
                                : tr("Music %1 is outside the verified range %2 to %3."))
                         .arg(music_->value())
                         .arg(info->music.minimum)
                         .arg(info->music.maximum);
        }
        if (!info->region.contains(region_->value())) {
            lines << (region_->value() == original_.region
                          ? tr("Note: the stored region %1 is outside %2 to %3 and is kept.")
                          : experimental_
                                ? tr("Experimental: region %1 is outside the verified range %2 "
                                     "to %3 and is written as typed.")
                                : tr("Region %1 is outside the verified range %2 to %3."))
                         .arg(region_->value())
                         .arg(info->region.minimum)
                         .arg(info->region.maximum);
        }
    }
    return lines.join(QLatin1Char('\n'));
}

void MetadataDialog::setExperimental(bool experimental) {
    experimental_ = experimental;
    if (experimental) {
        music_->setEnabled(true);
        region_->setEnabled(true);
    }
    refresh();
}

void MetadataDialog::setTextWorld(bool textWorld) {
    textWorld_ = textWorld;
    name_->setEnabled(!textWorld);
    if (textWorld) {
        name_->setToolTip(tr("A Mobile Eggbert world has no title."));
    }
    for (int index = 1; index < 4; ++index) {
        startX_[index]->setEnabled(!textWorld);
        startY_[index]->setEnabled(!textWorld);
        startDirection_[index]->setEnabled(!textWorld);
    }
    refresh();
}

void MetadataDialog::refresh() {
    messages_->setText(messages());
    buttons_->button(QDialogButtonBox::Ok)->setEnabled(isValid());
}

} // namespace eggbert
