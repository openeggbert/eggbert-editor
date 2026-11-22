// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QPointer>
#include <QWidget>

#include "app/document_tab.hpp"

class QCheckBox;
class QLabel;
class QSpinBox;
class QTreeWidget;

namespace eggbert {

// Lists the object pool of the current document and edits the exact numeric
// fields of the selected record. Everything goes through DocumentTab.
class ObjectsPanel final : public QWidget {
    Q_OBJECT

public:
    explicit ObjectsPanel(QWidget *parent = nullptr);

    void setTab(DocumentTab *tab);
    void refresh();
    [[nodiscard]] QTreeWidget *tree() const { return tree_; }
    [[nodiscard]] QString capacityText() const;
    // Spin box of an editable field; x/y select the component of a POINT
    // field (0 = x, 1 = y) and are ignored for scalar fields.
    [[nodiscard]] QSpinBox *fieldEditor(ObjectField field, int component = 0) const;
    // Applies the editors' current values to the selected record.
    void commitEditors();

signals:
    void slotActivated(qulonglong slot);

private:
    void refreshEditors();

    QPointer<DocumentTab> tab_;
    QLabel *capacity_ = nullptr;
    QCheckBox *showFree_ = nullptr;
    QTreeWidget *tree_ = nullptr;
    QLabel *stored_ = nullptr;
    QWidget *form_ = nullptr;
    QSpinBox *scalar_[15] = {};
    QSpinBox *pointX_[15] = {};
    QSpinBox *pointY_[15] = {};
    bool refreshing_ = false;
};

} // namespace eggbert
