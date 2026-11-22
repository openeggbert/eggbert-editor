// SPDX-License-Identifier: GPL-3.0-only
#include "statistics_panel.hpp"

#include <QHeaderView>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

namespace eggbert {

namespace {

enum RowKind {
    plainRow = 0,
    foregroundValueRow,
    bigDecorValueRow,
    objectTypeRow,
    objectChannelRow,
    objectIconRow,
    startRow,
    byteRangeRow,
};

constexpr int kindRole = Qt::UserRole;
constexpr int valueRole = Qt::UserRole + 1;
constexpr int sizeRole = Qt::UserRole + 2;

QTreeWidgetItem *addRow(QTreeWidgetItem *parent, const QString &label, const QString &value,
                        RowKind kind = plainRow, qlonglong data = 0, qlonglong size = 0) {
    auto *item = new QTreeWidgetItem(parent, {label, value});
    item->setData(0, kindRole, static_cast<int>(kind));
    item->setData(0, valueRole, data);
    item->setData(0, sizeRole, size);
    if (kind != plainRow) {
        item->setToolTip(0, StatisticsPanel::tr("Double-click to locate"));
    }
    return item;
}

QString number(std::size_t value) {
    return QString::number(static_cast<qulonglong>(value));
}

// Most frequent first; ties by value.
std::vector<std::pair<std::int16_t, std::size_t>>
byFrequency(const std::map<std::int16_t, std::size_t> &counts) {
    std::vector<std::pair<std::int16_t, std::size_t>> rows(counts.begin(), counts.end());
    std::ranges::stable_sort(rows, [](const auto &left, const auto &right) {
        return left.second > right.second;
    });
    return rows;
}

} // namespace

StatisticsPanel::StatisticsPanel(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    tree_ = new QTreeWidget(this);
    tree_->setObjectName(QStringLiteral("statisticsTree"));
    tree_->setHeaderLabels({tr("Statistic"), tr("Value")});
    tree_->setRootIsDecorated(true);
    tree_->setAlternatingRowColors(true);
    tree_->header()->setStretchLastSection(true);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    tree_->setColumnWidth(0, 230);
    setMinimumWidth(340);
    layout->addWidget(tree_);
    connect(tree_, &QTreeWidget::itemActivated, this,
            [this](QTreeWidgetItem *item) { activate(item); });
}

void StatisticsPanel::setTab(DocumentTab *tab) {
    if (tab_ == tab) {
        return;
    }
    if (tab_) {
        disconnect(tab_, nullptr, this, nullptr);
    }
    tab_ = tab;
    if (tab_) {
        connect(tab_, &DocumentTab::documentChanged, this, [this] { refresh(); });
    }
    refresh();
}

void StatisticsPanel::refresh() {
    tree_->clear();
    if (!tab_) {
        return;
    }
    const auto &document = tab_->document();
    const auto stats = computeWorldStats(document.view());
    auto *root = tree_->invisibleRootItem();

    auto *file = addRow(root, tr("File (stored facts)"), {});
    addRow(file, tr("Size"), tr("%1 bytes").arg(number(stats.fileBytes)));
    addRow(file, tr("Revision field"),
           stats.headerMajor ? QStringLiteral("%1.%2").arg(*stats.headerMajor).arg(*stats.headerMinor)
                             : tr("unavailable"));
    addRow(file, tr("Surplus bytes"), number(stats.surplusBytes));
    addRow(file, tr("Missing bytes"), number(stats.missingBytes));

    const auto addLayer = [&](const QString &title, const LayerStats &layer, RowKind kind) {
        auto *group = addRow(root, title, layer.present ? QString() : tr("absent"));
        if (!layer.present) {
            return;
        }
        addRow(group, tr("Occupied cells"), number(layer.occupied));
        addRow(group, tr("Empty cells (-1)"), number(layer.empty));
        addRow(group, tr("Unique values"), number(layer.valueCounts.size()));
        addRow(group, tr("Bounds of occupied cells"),
               layer.bounds ? tr("(%1, %2) to (%3, %4)")
                                  .arg(layer.bounds->minX)
                                  .arg(layer.bounds->minY)
                                  .arg(layer.bounds->maxX)
                                  .arg(layer.bounds->maxY)
                            : tr("none"));
        auto *values = addRow(group, tr("Values by frequency"), {});
        for (const auto &[value, count] : byFrequency(layer.valueCounts)) {
            addRow(values, tr("Value %1").arg(value), tr("%1 cells").arg(number(count)), kind,
                   value);
        }
    };
    addLayer(tr("Foreground layer"), stats.foreground, foregroundValueRow);
    addLayer(tr("bigDecor layer"), stats.bigDecor, bigDecorValueRow);

    auto *objects = addRow(root, tr("Object records"),
                           stats.objects.present ? QString() : tr("absent"));
    if (stats.objects.present) {
        addRow(objects, tr("Stored slots"), number(stats.objects.stored));
        addRow(objects, tr("Active (type not 0)"), number(stats.objects.active));
        addRow(objects, tr("Free (type 0)"), number(stats.objects.free));
        addRow(objects, tr("Free slots with other non-zero bytes"),
               number(stats.objects.freeWithOpaqueBytes));
        addRow(objects, tr("Active with different start and end"),
               number(stats.objects.withPath));
        const auto addCounts = [&](const QString &title, const QString &rowLabel,
                                   const std::map<std::int16_t, std::size_t> &counts,
                                   RowKind kind) {
            auto *group = addRow(objects, title, number(counts.size()));
            for (const auto &[value, count] : byFrequency(counts)) {
                addRow(group, rowLabel.arg(value), number(count), kind, value);
            }
        };
        addCounts(tr("By stored type"), tr("Type %1"), stats.objects.byType, objectTypeRow);
        addCounts(tr("By stored channel"), tr("Channel %1"), stats.objects.byChannel,
                  objectChannelRow);
        addCounts(tr("By stored icon"), tr("Icon %1"), stats.objects.byIcon, objectIconRow);
    }

    if (stats.headerPresent) {
        auto *starts = addRow(root, tr("Player starts"), {});
        for (int index = 0; index < 4; ++index) {
            addRow(starts, tr("Start %1").arg(index + 1),
                   tr("(%1, %2) direction %3")
                       .arg(stats.startPositions[index].x)
                       .arg(stats.startPositions[index].y)
                       .arg(stats.startDirections[index]),
                   startRow, index);
        }
    }
    addRow(root, tr("Bounds of used cells"),
           stats.usedBounds ? tr("(%1, %2) to (%3, %4)")
                                  .arg(stats.usedBounds->minX)
                                  .arg(stats.usedBounds->minY)
                                  .arg(stats.usedBounds->maxX)
                                  .arg(stats.usedBounds->maxY)
                            : tr("none"));

    const auto addRanges = [&](const QString &title, const std::vector<ByteRange> &ranges) {
        std::size_t bytes = 0;
        for (const auto &range : ranges) {
            bytes += range.size;
        }
        auto *group = addRow(root, title,
                             tr("%1 bytes in %2 range(s)").arg(number(bytes), number(ranges.size())));
        constexpr std::size_t listed = 200;
        for (std::size_t index = 0; index < ranges.size() && index < listed; ++index) {
            addRow(group, tr("Offset %1").arg(number(ranges[index].offset)),
                   tr("%1 byte(s)").arg(number(ranges[index].size)), byteRangeRow,
                   static_cast<qlonglong>(ranges[index].offset),
                   static_cast<qlonglong>(ranges[index].size));
        }
        if (ranges.size() > listed) {
            addRow(group, tr("... %1 more").arg(number(ranges.size() - listed)), {});
        }
    };
    addRanges(tr("Unsaved byte ranges"), document.unsavedRanges());
    addRanges(tr("Changed since opened"), document.changedRangesFromOriginal());

    for (int index = 0; index < tree_->topLevelItemCount(); ++index) {
        tree_->topLevelItem(index)->setExpanded(true);
    }
}

QTreeWidgetItem *StatisticsPanel::findRow(const QString &label) const {
    for (QTreeWidgetItemIterator iterator(tree_); *iterator != nullptr; ++iterator) {
        if ((*iterator)->text(0) == label) {
            return *iterator;
        }
    }
    return nullptr;
}

QString StatisticsPanel::valueOf(const QString &label) const {
    const auto *item = findRow(label);
    return item != nullptr ? item->text(1) : QString();
}

void StatisticsPanel::activate(QTreeWidgetItem *item) {
    if (item == nullptr) {
        return;
    }
    const auto value = item->data(0, valueRole).toLongLong();
    switch (item->data(0, kindRole).toInt()) {
    case foregroundValueRow:
        emit tileValueActivated(SectionKind::foreground, static_cast<int>(value));
        break;
    case bigDecorValueRow:
        emit tileValueActivated(SectionKind::bigDecor, static_cast<int>(value));
        break;
    case objectTypeRow:
        emit objectKeyActivated(ObjectKey::type, static_cast<int>(value));
        break;
    case objectChannelRow:
        emit objectKeyActivated(ObjectKey::channel, static_cast<int>(value));
        break;
    case objectIconRow:
        emit objectKeyActivated(ObjectKey::icon, static_cast<int>(value));
        break;
    case startRow:
        emit startActivated(static_cast<int>(value));
        break;
    case byteRangeRow:
        emit byteRangeActivated(static_cast<qulonglong>(value),
                                item->data(0, sizeRole).toULongLong());
        break;
    default:
        break;
    }
}

} // namespace eggbert
