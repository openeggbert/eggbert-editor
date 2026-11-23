// SPDX-License-Identifier: GPL-3.0-only
#include "diagnostics_panel.hpp"

#include <QCheckBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "app/doctor_texts.hpp"
#include "world/hex_model.hpp"

namespace eggbert {

namespace {

// Qt's tree wraps text but keeps every row one line high. This gives a row
// the height the wrapped text of one column needs at that column's width.
class WrappedColumnDelegate final : public QStyledItemDelegate {
public:
    WrappedColumnDelegate(QTreeWidget *tree, int column)
        : QStyledItemDelegate(tree), tree_(tree), column_(column) {}

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        auto size = QStyledItemDelegate::sizeHint(option, index);
        if (index.column() == column_) {
            const int margin =
                tree_->style()->pixelMetric(QStyle::PM_FocusFrameHMargin, nullptr, tree_) + 1;
            const int width = std::max(40, tree_->columnWidth(column_) - 2 * margin);
            const auto text = option.fontMetrics.boundingRect(
                QRect(0, 0, width, 100'000), Qt::TextWordWrap, index.data().toString());
            size.setHeight(std::max(size.height(), text.height() + 4));
        }
        return size;
    }

private:
    QTreeWidget *tree_;
    int column_;
};

constexpr int indexRole = Qt::UserRole;

QString fromView(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

QString severityText(Severity severity) {
    switch (severity) {
    case Severity::error:
        return DiagnosticsPanel::tr("Error");
    case Severity::warning:
        return DiagnosticsPanel::tr("Warning");
    case Severity::note:
        return DiagnosticsPanel::tr("Note");
    }
    return {};
}

QString hexOf(std::span<const std::byte> bytes) {
    constexpr std::size_t shown = 16;
    auto text = QString::fromStdString(formatHex(bytes.first(std::min(bytes.size(), shown))));
    if (bytes.size() > shown) {
        text += QStringLiteral(" ...");
    }
    return text;
}

} // namespace

QString formatByteDifferences(const WorldDocument &document, bool againstOriginal) {
    const auto reference = againstOriginal ? document.originalBytes() : document.savedBytes();
    const auto ranges =
        againstOriginal ? document.changedRangesFromOriginal() : document.unsavedRanges();
    QStringList lines;
    std::size_t total = 0;
    for (const auto &range : ranges) {
        total += range.size;
    }
    lines << (againstOriginal
                  ? DiagnosticsPanel::tr("%1 byte(s) in %2 range(s) differ from the file as opened.")
                  : DiagnosticsPanel::tr("%1 byte(s) in %2 range(s) differ from the saved file."))
                 .arg(total)
                 .arg(ranges.size());
    constexpr std::size_t listed = 500;
    for (std::size_t index = 0; index < ranges.size() && index < listed; ++index) {
        const auto &range = ranges[index];
        lines << DiagnosticsPanel::tr("Offset %1, %2 byte(s), %3: %4 -> %5")
                     .arg(range.offset)
                     .arg(range.size)
                     .arg(QString::fromStdString(offsetLabel(document.probe(), range.offset)))
                     .arg(hexOf(reference.subspan(range.offset, range.size)))
                     .arg(hexOf(document.currentBytes().subspan(range.offset, range.size)));
    }
    if (ranges.size() > listed) {
        lines << DiagnosticsPanel::tr("... and %1 more range(s)").arg(ranges.size() - listed);
    }
    return lines.join(QLatin1Char('\n'));
}

QString formatFix(const WorldDocument &document, const DoctorFix &fix) {
    QStringList lines;
    lines << fromView(fix.label);
    for (const auto &change : fix.changes) {
        lines << DiagnosticsPanel::tr("Offset %1, %2 byte(s), %3: %4 -> %5")
                     .arg(change.offset)
                     .arg(change.after.size())
                     .arg(QString::fromStdString(offsetLabel(document.probe(), change.offset)))
                     .arg(hexOf(change.before))
                     .arg(hexOf(change.after));
    }
    return lines.join(QLatin1Char('\n'));
}

DiagnosticsPanel::DiagnosticsPanel(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    summary_ = new QLabel(this);
    summary_->setObjectName(QStringLiteral("diagnosticsSummary"));
    summary_->setWordWrap(true);
    layout->addWidget(summary_);

    auto *filters = new QHBoxLayout;
    const auto addFilter = [&](const QString &name, const QString &text) {
        auto *box = new QCheckBox(text, this);
        box->setObjectName(name);
        box->setChecked(true);
        filters->addWidget(box);
        connect(box, &QCheckBox::toggled, this, [this] { rebuildList(); });
        return box;
    };
    showErrors_ = addFilter(QStringLiteral("showErrorsCheck"), tr("Errors"));
    showWarnings_ = addFilter(QStringLiteral("showWarningsCheck"), tr("Warnings"));
    showNotes_ = addFilter(QStringLiteral("showNotesCheck"), tr("Notes"));
    filters->addStretch(1);
    auto *validateButton = new QPushButton(tr("Validate"), this);
    validateButton->setObjectName(QStringLiteral("validateButton"));
    filters->addWidget(validateButton);
    layout->addLayout(filters);
    connect(validateButton, &QPushButton::clicked, this, [this] { validate(); });

    tree_ = new QTreeWidget(this);
    tree_->setObjectName(QStringLiteral("diagnosticsTree"));
    tree_->setAccessibleName(tr("World Doctor findings"));
    tree_->setRootIsDecorated(false);
    tree_->setAlternatingRowColors(true);
    tree_->setHeaderLabels({tr("Severity"), tr("Rule"), tr("Finding"), tr("Offset")});
    // The finding takes the width that is left and wraps onto as many lines
    // as it needs, so that it can be read without a tooltip or scrolling.
    // Its width is set by fitFindingColumn whenever the table's width or
    // the other columns change.
    tree_->header()->setStretchLastSection(false);
    tree_->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(2, QHeaderView::Fixed);
    tree_->setWordWrap(true);
    tree_->setItemDelegate(new WrappedColumnDelegate(tree_, 2));
    tree_->setTextElideMode(Qt::ElideNone);
    tree_->setUniformRowHeights(false);
    tree_->viewport()->installEventFilter(this);
    connect(tree_->header(), &QHeaderView::sectionResized, this, [this](int section) {
        if (section != 2) {
            scheduleFit();
        }
    });
    layout->addWidget(tree_, 1);

    detail_ = new QLabel(this);
    detail_->setObjectName(QStringLiteral("diagnosticsDetail"));
    detail_->setWordWrap(true);
    detail_->setTextFormat(Qt::PlainText);
    detail_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(detail_);

    fixButton_ = new QPushButton(tr("Review Proposed Fix..."), this);
    fixButton_->setObjectName(QStringLiteral("fixButton"));
    fixButton_->setEnabled(false);
    layout->addWidget(fixButton_);

    connect(tree_, &QTreeWidget::currentItemChanged, this, [this] {
        const auto *finding = selectedFinding();
        if (finding == nullptr) {
            detail_->clear();
            fixButton_->setEnabled(false);
            return;
        }
        detail_->setText(
            tr("%1\nBasis: %2. %3\nBytes: offset %4, length %5%6")
                .arg(findingText(*finding), confidenceText(finding->confidence),
                     fromView(finding->evidence))
                .arg(finding->range.offset)
                .arg(finding->range.size)
                .arg(finding->profile == GameProfile::unknown
                         ? QString()
                         : tr("\nJudged for: %1").arg(profileDisplayName(finding->profile))));
        fixButton_->setEnabled(finding->fix.has_value() && tab_ &&
                               tab_->document().isWritable());
    });
    connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        if (item != nullptr) {
            emit findingActivated(item->data(0, indexRole).toInt());
        }
    });
    connect(fixButton_, &QPushButton::clicked, this, [this] {
        if (const auto *item = tree_->currentItem()) {
            emit fixRequested(item->data(0, indexRole).toInt());
        }
    });
    setMinimumHeight(180);
    validate();
}

void DiagnosticsPanel::setTab(DocumentTab *tab) {
    if (tab_ == tab) {
        return;
    }
    if (tab_) {
        disconnect(tab_, nullptr, this, nullptr);
    }
    tab_ = tab;
    if (tab_) {
        // The audit is a single fast pass, so it simply reruns after edits.
        connect(tab_, &DocumentTab::documentChanged, this, [this] { validate(); });
        connect(tab_, &DocumentTab::profileChanged, this, [this] { validate(); });
    }
    validate();
}

void DiagnosticsPanel::validate() {
    report_ = tab_ ? runDoctor(tab_->document().view(), tab_->profile()) : DoctorReport{};
    rebuildList();
}

QString DiagnosticsPanel::summaryText() const {
    return summary_->text();
}

void DiagnosticsPanel::setShown(Severity severity, bool shown) {
    (severity == Severity::error ? showErrors_
     : severity == Severity::warning ? showWarnings_
                                     : showNotes_)
        ->setChecked(shown);
}

bool DiagnosticsPanel::eventFilter(QObject *watched, QEvent *event) {
    if (watched == tree_->viewport() && event->type() == QEvent::Resize) {
        scheduleFit();
    }
    return QWidget::eventFilter(watched, event);
}

void DiagnosticsPanel::scheduleFit() {
    // Not from inside the header's or the view's own layout pass.
    if (fitPending_) {
        return;
    }
    fitPending_ = true;
    QTimer::singleShot(0, this, [this] {
        fitPending_ = false;
        fitFindingColumn();
    });
}

void DiagnosticsPanel::fitFindingColumn() {
    auto *header = tree_->header();
    const int others = header->sectionSize(0) + header->sectionSize(1) + header->sectionSize(3);
    // Never narrower than a few words: a very narrow dock scrolls sideways
    // rather than making one word a line.
    const int width = std::max(160, tree_->viewport()->width() - others);
    if (header->sectionSize(2) != width) {
        header->resizeSection(2, width);
    }
    tree_->doItemsLayout(); // row heights follow the width
}

void DiagnosticsPanel::rebuildList() {
    tree_->clear();
    detail_->clear();
    fixButton_->setEnabled(false);
    if (!tab_) {
        summary_->setText(tr("No document."));
        return;
    }
    auto summary = tr("%1 error(s), %2 warning(s), %3 note(s).")
                       .arg(report_.count(Severity::error))
                       .arg(report_.count(Severity::warning))
                       .arg(report_.count(Severity::note));
    summary += QLatin1Char(' ') +
               (tab_->profile() == GameProfile::unknown
                    ? tr("No game profile is chosen, so game-specific rules were skipped.")
                    : tr("Judged for %1.").arg(profileDisplayName(tab_->profile())));
    summary += QLatin1Char(' ') +
               tr("World Doctor checks structure and verified limits; it does not judge whether "
                  "a world can be completed.");
    summary_->setText(summary);
    for (std::size_t index = 0; index < report_.findings.size(); ++index) {
        const auto &finding = report_.findings[index];
        const bool shown = finding.severity == Severity::error     ? showErrors_->isChecked()
                           : finding.severity == Severity::warning ? showWarnings_->isChecked()
                                                                   : showNotes_->isChecked();
        if (!shown) {
            continue;
        }
        auto *item = new QTreeWidgetItem(tree_);
        item->setData(0, indexRole, static_cast<int>(index));
        item->setText(0, severityText(finding.severity));
        item->setText(1, fromView(finding.ruleId));
        item->setText(2, findingText(finding));
        item->setText(3, QString::number(finding.range.offset));
        item->setToolTip(2, findingText(finding));
    }
    fitFindingColumn();
}

const Finding *DiagnosticsPanel::findingAtRow(int row) const {
    const auto *item = tree_->topLevelItem(row);
    if (item == nullptr) {
        return nullptr;
    }
    const auto index = static_cast<std::size_t>(item->data(0, indexRole).toInt());
    return index < report_.findings.size() ? &report_.findings[index] : nullptr;
}

const Finding *DiagnosticsPanel::selectedFinding() const {
    const auto *item = tree_->currentItem();
    if (item == nullptr) {
        return nullptr;
    }
    const auto index = static_cast<std::size_t>(item->data(0, indexRole).toInt());
    return index < report_.findings.size() ? &report_.findings[index] : nullptr;
}

} // namespace eggbert
