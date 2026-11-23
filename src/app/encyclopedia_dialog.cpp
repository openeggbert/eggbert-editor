// SPDX-License-Identifier: GPL-3.0-only
#include "app/encyclopedia_dialog.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

#include "app/game_assets.hpp"
#include "app/sprite_icons.hpp"

namespace eggbert {

namespace {

constexpr int thumbnailSize = 40;

QString text(const std::string &value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

} // namespace

EncyclopediaDialog::EncyclopediaDialog(QWidget *parent) : QDialog(parent) {
    setWindowTitle(tr("Element Encyclopedia"));
    setObjectName(QStringLiteral("encyclopediaDialog"));
    auto *layout = new QVBoxLayout(this);
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
    select_ = new QPushButton(tr("Select in Editor"), this);
    select_->setObjectName(QStringLiteral("encyclopediaSelect"));
    select_->setToolTip(tr("Chooses this element as the Palette does: the tile for the Pencil, "
                           "the object for Place, the family for the Adaptive brush."));
    side->addWidget(select_);
    columns->addLayout(side, 1);
    layout->addLayout(columns, 1);

    connect(search_, &QLineEdit::textChanged, this, [this] { refill(); });
    connect(list_, &QListWidget::currentRowChanged, this, [this] { showCurrent(); });
    connect(list_, &QListWidget::itemActivated, this, [this] { chooseCurrent(); });
    connect(select_, &QPushButton::clicked, this, [this] { chooseCurrent(); });
    resize(760, 520);
    refill();
}

void EncyclopediaDialog::setGame(GameProfile profile, std::shared_ptr<const GameAssets> artwork) {
    // The rows index into entries_: drop them before the entries change.
    list_->clear();
    profile_ = profile;
    artwork_ = std::move(artwork);
    entries_ = encyclopedia(profile);
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
    const auto query = search_->text().toStdString();
    const auto matches = searchEncyclopedia(entries_, query);
    for (const auto index : matches) {
        const auto &entry = entries_[index];
        const auto icon =
            entry.kind == EntryKind::object
                ? objectThumbnail(entry.type, entry.channel, entry.icon, artwork_.get(),
                                  thumbnailSize)
                : tileThumbnail(entry.layer, entry.values.front(), artwork_.get(), thumbnailSize);
        auto *item = new QListWidgetItem(QIcon(icon), text(entry.title), list_);
        item->setData(Qt::UserRole, static_cast<qulonglong>(index));
        item->setToolTip(text(entry.group));
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
    select_->setEnabled(entry != nullptr);
    if (entry == nullptr) {
        details_->setText(QString());
        return;
    }
    const auto kind = entry->kind == EntryKind::tile     ? tr("Tile")
                      : entry->kind == EntryKind::object ? tr("Object")
                                                         : tr("Border family");
    details_->setText(
        QStringLiteral("<h3>%1</h3><p><b>%2</b> &middot; %3</p><p>%4</p><p><b>%5</b><br>%6</p>")
            .arg(text(entry->title).toHtmlEscaped(), kind, text(entry->group).toHtmlEscaped(),
                 text(entry->facts).toHtmlEscaped(), tr("Evidence"),
                 text(entry->provenance).toHtmlEscaped()));
}

void EncyclopediaDialog::chooseCurrent() {
    const auto *entry = currentEntry();
    if (entry == nullptr) {
        return;
    }
    switch (entry->kind) {
    case EntryKind::tile:
        emit tileChosen(static_cast<int>(entry->layer), entry->values.front());
        break;
    case EntryKind::object:
        emit presetChosen(text(entry->id));
        break;
    case EntryKind::border:
        emit brushChosen(text(entry->id));
        break;
    }
}

} // namespace eggbert
