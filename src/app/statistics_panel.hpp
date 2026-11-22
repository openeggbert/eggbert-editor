// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QPointer>
#include <QWidget>

#include "app/document_tab.hpp"
#include "world/world_stats.hpp"

class QTreeWidget;
class QTreeWidgetItem;

namespace eggbert {

// Read-only statistics of the current document. Activating a row asks the
// window to select or locate what the row counts.
class StatisticsPanel final : public QWidget {
    Q_OBJECT

public:
    explicit StatisticsPanel(QWidget *parent = nullptr);

    void setTab(DocumentTab *tab);
    void refresh();
    [[nodiscard]] QTreeWidget *tree() const { return tree_; }
    // Value column of the first row whose label matches; empty when absent.
    [[nodiscard]] QString valueOf(const QString &label) const;
    [[nodiscard]] QTreeWidgetItem *findRow(const QString &label) const;

signals:
    void tileValueActivated(eggbert::SectionKind layer, int value);
    void objectKeyActivated(eggbert::ObjectKey key, int value);
    void startActivated(int index);
    void byteRangeActivated(qulonglong offset, qulonglong size);

private:
    void activate(QTreeWidgetItem *item);

    QPointer<DocumentTab> tab_;
    QTreeWidget *tree_ = nullptr;
};

} // namespace eggbert
