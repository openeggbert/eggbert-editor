// SPDX-License-Identifier: GPL-3.0-only
#include "objects_panel.hpp"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QSpinBox>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <limits>

#include "world/profile.hpp"

namespace eggbert {

namespace {

constexpr int slotRole = Qt::UserRole;

QString pointText(Point value) {
    return QStringLiteral("%1, %2").arg(value.x).arg(value.y);
}

QString fieldLabel(ObjectField field) {
    switch (field) {
    case ObjectField::stepAdvance:
        return ObjectsPanel::tr("Advance duration (ticks)");
    case ObjectField::stepRecede:
        return ObjectsPanel::tr("Return duration (ticks)");
    case ObjectField::timeStopStart:
        return ObjectsPanel::tr("Wait at start (ticks)");
    case ObjectField::timeStopEnd:
        return ObjectsPanel::tr("Wait at end (ticks)");
    case ObjectField::posStart:
        return ObjectsPanel::tr("Start x, y (pixels)");
    case ObjectField::posEnd:
        return ObjectsPanel::tr("End x, y (pixels)");
    case ObjectField::posCurrent:
        return ObjectsPanel::tr("Current x, y (pixels)");
    case ObjectField::channel:
        return ObjectsPanel::tr("Stored channel");
    case ObjectField::icon:
        return ObjectsPanel::tr("Stored icon");
    default:
        break;
    }
    return {};
}

} // namespace

ObjectsPanel::ObjectsPanel(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    capacity_ = new QLabel(this);
    capacity_->setObjectName(QStringLiteral("capacityLabel"));
    capacity_->setWordWrap(true);
    layout->addWidget(capacity_);

    showFree_ = new QCheckBox(tr("Show free slots"), this);
    showFree_->setObjectName(QStringLiteral("showFreeCheck"));
    layout->addWidget(showFree_);
    connect(showFree_, &QCheckBox::toggled, this, [this] { refresh(); });

    tree_ = new QTreeWidget(this);
    tree_->setObjectName(QStringLiteral("objectsTree"));
    tree_->setRootIsDecorated(false);
    tree_->setAlternatingRowColors(true);
    tree_->setHeaderLabels(
        {tr("Slot"), tr("Type"), tr("Start"), tr("End"), tr("Current"), tr("Ch"), tr("Icon")});
    tree_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(tree_, 1);
    connect(tree_, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *item) {
        if (refreshing_ || !tab_ || item == nullptr) {
            return;
        }
        tab_->setSelectedSlot(static_cast<std::size_t>(item->data(0, slotRole).toULongLong()));
    });
    connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        if (item != nullptr) {
            emit slotActivated(item->data(0, slotRole).toULongLong());
        }
    });

    stored_ = new QLabel(this);
    stored_->setObjectName(QStringLiteral("storedFieldsLabel"));
    stored_->setWordWrap(true);
    stored_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(stored_);

    form_ = new QWidget(this);
    auto *form = new QFormLayout(form_);
    form->setContentsMargins(0, 0, 0, 0);
    constexpr int intMin = std::numeric_limits<int>::min();
    constexpr int intMax = std::numeric_limits<int>::max();
    for (const auto &descriptor : objectFields) {
        if (!DocumentTab::isEditableObjectField(descriptor.field)) {
            continue;
        }
        const auto index = static_cast<std::size_t>(descriptor.field);
        const auto name = QString::fromUtf8(descriptor.name.data(),
                                            static_cast<qsizetype>(descriptor.name.size()));
        const auto connectSpin = [this](QSpinBox *spin) {
            connect(spin, &QSpinBox::editingFinished, this, [this] { commitEditors(); });
        };
        if (descriptor.type == WireType::point) {
            auto *row = new QWidget(form_);
            auto *rowLayout = new QHBoxLayout(row);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            pointX_[index] = new QSpinBox(row);
            pointY_[index] = new QSpinBox(row);
            pointX_[index]->setObjectName(QStringLiteral("object_%1_x").arg(name));
            pointY_[index]->setObjectName(QStringLiteral("object_%1_y").arg(name));
            for (auto *spin : {pointX_[index], pointY_[index]}) {
                spin->setRange(intMin, intMax);
                spin->setSingleStep(layout::cellPixels);
                rowLayout->addWidget(spin);
                connectSpin(spin);
            }
            form->addRow(fieldLabel(descriptor.field), row);
        } else {
            scalar_[index] = new QSpinBox(form_);
            scalar_[index]->setObjectName(QStringLiteral("object_%1").arg(name));
            scalar_[index]->setRange(-32768, 32767);
            connectSpin(scalar_[index]);
            form->addRow(fieldLabel(descriptor.field), scalar_[index]);
        }
    }
    layout->addWidget(form_);
    setMinimumWidth(340);
    refresh();
}

void ObjectsPanel::setTab(DocumentTab *tab) {
    if (tab_ == tab) {
        return;
    }
    if (tab_) {
        disconnect(tab_, nullptr, this, nullptr);
    }
    tab_ = tab;
    if (tab_) {
        connect(tab_, &DocumentTab::documentChanged, this, [this] { refresh(); });
        connect(tab_, &DocumentTab::profileChanged, this, [this] { refresh(); });
        connect(tab_, &DocumentTab::objectSelectionChanged, this, [this] { refresh(); });
    }
    refresh();
}

QString ObjectsPanel::capacityText() const {
    return capacity_->text();
}

QSpinBox *ObjectsPanel::fieldEditor(ObjectField field, int component) const {
    const auto index = static_cast<std::size_t>(field);
    if (objectField(field).type == WireType::point) {
        return component == 0 ? pointX_[index] : pointY_[index];
    }
    return scalar_[index];
}

void ObjectsPanel::refresh() {
    refreshing_ = true;
    tree_->clear();
    if (!tab_) {
        capacity_->setText(tr("No document."));
        stored_->clear();
        form_->setEnabled(false);
        refreshing_ = false;
        return;
    }
    const auto view = tab_->document().view();
    const auto capacity = poolCapacity(view);
    QString text = capacity.stored == 0
                       ? tr("No complete object pool in this document.")
                       : tr("Stored slots %1 - in use %2 - free %3")
                             .arg(capacity.stored)
                             .arg(capacity.active)
                             .arg(capacity.free);
    if (capacity.stored != 0) {
        if (capacity.free == 0) {
            text += QLatin1Char('\n') +
                    tr("The pool is full: this is the format's hard limit. Nothing can be added "
                       "and effects the game creates while playing will find no slot.");
        } else if (const auto *info = profileInfo(tab_->profile());
                   info != nullptr &&
                   capacity.free < static_cast<std::size_t>(info->lowHeadroomFreeSlots)) {
            text += QLatin1Char('\n') +
                    tr("Low headroom (estimate, not a proven limit): fewer than %1 free slots "
                       "may leave no room for effects created while playing.")
                        .arg(info->lowHeadroomFreeSlots);
        }
    }
    capacity_->setText(text);

    QTreeWidgetItem *selectedItem = nullptr;
    if (const auto objects = view.objects()) {
        for (std::size_t slot = 0; slot < objects->slotCount(); ++slot) {
            const auto record = *objects->record(slot);
            if (record.isFree() && !showFree_->isChecked()) {
                continue;
            }
            auto *item = new QTreeWidgetItem(tree_);
            item->setData(0, slotRole, static_cast<qulonglong>(slot));
            item->setText(0, QString::number(slot));
            item->setText(1, record.isFree() ? tr("free (0)") : tab_->objectTypeText(record.type));
            item->setText(2, pointText(record.posStart));
            item->setText(3, pointText(record.posEnd));
            item->setText(4, pointText(record.posCurrent));
            item->setText(5, QString::number(record.channel));
            item->setText(6, QString::number(record.icon));
            if (tab_->selectedSlot() == slot) {
                selectedItem = item;
            }
        }
    }
    if (selectedItem != nullptr) {
        tree_->setCurrentItem(selectedItem);
    }
    refreshEditors();
    refreshing_ = false;
}

void ObjectsPanel::refreshEditors() {
    const auto slot = tab_ ? tab_->selectedSlot() : std::nullopt;
    const auto objects = tab_ ? tab_->document().view().objects() : std::nullopt;
    const auto record = slot && objects ? objects->record(*slot) : std::nullopt;
    if (!record) {
        stored_->setText(tab_ ? tr("Select an object to see its stored fields.") : QString());
        form_->setEnabled(false);
        return;
    }
    stored_->setText(tr("Slot %1 at offset %2: type %3, step %4, time %5, phase %6, unknown1 %7, "
                        "unknown2 %8. These fields are shown as stored and are not edited here.")
                         .arg(*slot)
                         .arg(objects->slotRange(*slot)->offset)
                         .arg(record->type)
                         .arg(record->step)
                         .arg(record->time)
                         .arg(record->phase)
                         .arg(record->unknown1)
                         .arg(record->unknown2));
    const auto setScalar = [this](ObjectField field, int value) {
        scalar_[static_cast<std::size_t>(field)]->setValue(value);
    };
    const auto setPoint = [this](ObjectField field, Point value) {
        pointX_[static_cast<std::size_t>(field)]->setValue(value.x);
        pointY_[static_cast<std::size_t>(field)]->setValue(value.y);
    };
    setScalar(ObjectField::stepAdvance, record->stepAdvance);
    setScalar(ObjectField::stepRecede, record->stepRecede);
    setScalar(ObjectField::timeStopStart, record->timeStopStart);
    setScalar(ObjectField::timeStopEnd, record->timeStopEnd);
    setPoint(ObjectField::posStart, record->posStart);
    setPoint(ObjectField::posEnd, record->posEnd);
    setPoint(ObjectField::posCurrent, record->posCurrent);
    setScalar(ObjectField::channel, record->channel);
    setScalar(ObjectField::icon, record->icon);
    form_->setEnabled(tab_->canEditObject(*slot));
}

void ObjectsPanel::commitEditors() {
    if (refreshing_ || !tab_) {
        return;
    }
    const auto slot = tab_->selectedSlot();
    const auto objects = tab_->document().view().objects();
    if (!slot || !objects || !tab_->canEditObject(*slot)) {
        return;
    }
    // Apply the first differing field; the resulting refresh reloads the
    // editors, and each numeric entry is its own undo step.
    for (const auto &descriptor : objectFields) {
        if (!DocumentTab::isEditableObjectField(descriptor.field)) {
            continue;
        }
        const auto record = *objects->record(*slot);
        const auto index = static_cast<std::size_t>(descriptor.field);
        if (descriptor.type == WireType::point) {
            const Point wanted{pointX_[index]->value(), pointY_[index]->value()};
            const auto current = descriptor.field == ObjectField::posStart ? record.posStart
                                 : descriptor.field == ObjectField::posEnd ? record.posEnd
                                                                           : record.posCurrent;
            if (wanted != current) {
                tab_->setObjectPosition(*slot, descriptor.field, wanted);
                return;
            }
        } else {
            const auto wanted = static_cast<std::int16_t>(scalar_[index]->value());
            const auto current =
                descriptor.field == ObjectField::stepAdvance     ? record.stepAdvance
                : descriptor.field == ObjectField::stepRecede    ? record.stepRecede
                : descriptor.field == ObjectField::timeStopStart ? record.timeStopStart
                : descriptor.field == ObjectField::timeStopEnd   ? record.timeStopEnd
                : descriptor.field == ObjectField::channel       ? record.channel
                                                                 : record.icon;
            if (wanted != current) {
                tab_->setObjectNumber(*slot, descriptor.field, wanted);
                return;
            }
        }
    }
}

} // namespace eggbert
