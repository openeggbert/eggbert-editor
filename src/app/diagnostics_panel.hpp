// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QPointer>
#include <QWidget>

#include "app/document_tab.hpp"
#include "world/doctor.hpp"

class QCheckBox;
class QLabel;
class QPushButton;
class QTreeWidget;

namespace eggbert {

// World Doctor findings for the current document. Validation never changes
// the document; a fix is applied only through fixRequested.
class DiagnosticsPanel final : public QWidget {
    Q_OBJECT

public:
    explicit DiagnosticsPanel(QWidget *parent = nullptr);

    void setTab(DocumentTab *tab);
    // Runs the complete audit for the tab's current bytes and profile.
    void validate();
    [[nodiscard]] const DoctorReport &report() const { return report_; }
    [[nodiscard]] QTreeWidget *tree() const { return tree_; }
    [[nodiscard]] QString summaryText() const;
    // Finding shown in a row of the (filtered) list; nullptr when out of range.
    [[nodiscard]] const Finding *findingAtRow(int row) const;
    [[nodiscard]] const Finding *selectedFinding() const;
    void setShown(Severity severity, bool shown);

signals:
    void findingActivated(int findingIndex);
    void fixRequested(int findingIndex);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rebuildList();
    // Gives the finding column the width the other three leave and lays the
    // rows out again for it. Done by the panel itself: the header's own
    // stretching did not follow a narrower table with every Qt version.
    void fitFindingColumn();
    void scheduleFit();

    bool fitPending_ = false;

    QPointer<DocumentTab> tab_;
    DoctorReport report_;
    QLabel *summary_ = nullptr;
    QCheckBox *showErrors_ = nullptr;
    QCheckBox *showWarnings_ = nullptr;
    QCheckBox *showNotes_ = nullptr;
    QTreeWidget *tree_ = nullptr;
    QLabel *detail_ = nullptr;
    QPushButton *fixButton_ = nullptr;
};

// Plain-text list of the byte ranges that differ from the original or the
// saved bytes, with structural names and old/new bytes.
[[nodiscard]] QString formatByteDifferences(const WorldDocument &document, bool againstOriginal);
// Description of a fix for confirmation: every byte range with old and new bytes.
[[nodiscard]] QString formatFix(const WorldDocument &document, const DoctorFix &fix);

} // namespace eggbert
