// SPDX-License-Identifier: GPL-3.0-only
#include "metadata_dialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

#include <limits>

namespace eggbert {

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
    form->addRow(info != nullptr ? tr("Region / background (%1 to %2)")
                                       .arg(info->region.minimum)
                                       .arg(info->region.maximum)
                                 : tr("Region (choose a game profile to edit)"),
                 region_);

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
    connect(music_, &QSpinBox::valueChanged, this, [this] { refresh(); });
    connect(region_, &QSpinBox::valueChanged, this, [this] { refresh(); });
    refresh();
}

void MetadataDialog::setPage(Page page) {
    pages_->setCurrentIndex(page);
}

std::optional<Metadata> MetadataDialog::metadata() const {
    auto result = original_;
    const auto name = encodeTitle(name_->text());
    if (!name || name->size() > storedTitleLimit) {
        return std::nullopt;
    }
    result.name = *name;
    const auto *info = profileInfo(profile_);
    // A changed music or region must be a value the chosen game offers; an
    // unchanged stored value is kept whatever it is.
    if (music_->value() != original_.music &&
        (info == nullptr || !info->music.contains(music_->value()))) {
        return std::nullopt;
    }
    if (region_->value() != original_.region &&
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
    if (!name) {
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
                          : tr("Music %1 is outside the verified range %2 to %3."))
                         .arg(music_->value())
                         .arg(info->music.minimum)
                         .arg(info->music.maximum);
        }
        if (!info->region.contains(region_->value())) {
            lines << (region_->value() == original_.region
                          ? tr("Note: the stored region %1 is outside %2 to %3 and is kept.")
                          : tr("Region %1 is outside the verified range %2 to %3."))
                         .arg(region_->value())
                         .arg(info->region.minimum)
                         .arg(info->region.maximum);
        }
    }
    return lines.join(QLatin1Char('\n'));
}

void MetadataDialog::refresh() {
    messages_->setText(messages());
    buttons_->button(QDialogButtonBox::Ok)->setEnabled(isValid());
}

} // namespace eggbert
