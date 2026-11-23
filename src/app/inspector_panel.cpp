// SPDX-License-Identifier: GPL-3.0-only
#include "inspector_panel.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QVBoxLayout>

#include <algorithm>

#include "world/hex_model.hpp"

namespace eggbert {

InspectorPanel::InspectorPanel(QWidget *parent) : QWidget(parent) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto *jumpRow = new QHBoxLayout;
    jumpEdit_ = new QLineEdit(this);
    jumpEdit_->setObjectName(QStringLiteral("inspectorJumpEdit"));
    jumpEdit_->setPlaceholderText(tr("Go to: 1608, 0x648, cell 3,70, big 3,70, slot 137"));
    jumpRow->addWidget(jumpEdit_, 1);
    sectionCombo_ = new QComboBox(this);
    sectionCombo_->setObjectName(QStringLiteral("inspectorSectionCombo"));
    sectionCombo_->setAccessibleName(tr("Jump to a section"));
    jumpRow->addWidget(sectionCombo_);
    layout->addLayout(jumpRow);

    auto *searchRow = new QHBoxLayout;
    searchEdit_ = new QLineEdit(this);
    searchEdit_->setObjectName(QStringLiteral("inspectorSearchEdit"));
    searchEdit_->setPlaceholderText(tr("Search bytes"));
    searchRow->addWidget(searchEdit_, 1);
    searchMode_ = new QComboBox(this);
    searchMode_->setObjectName(QStringLiteral("inspectorSearchMode"));
    searchMode_->setAccessibleName(tr("Search as hex or text"));
    searchMode_->addItem(tr("Hex"));
    searchMode_->addItem(tr("ASCII"));
    searchRow->addWidget(searchMode_);
    auto *findButton = new QPushButton(tr("Find Next"), this);
    findButton->setObjectName(QStringLiteral("inspectorFindButton"));
    searchRow->addWidget(findButton);
    layout->addLayout(searchRow);

    auto *optionRow = new QHBoxLayout;
    widthCombo_ = new QComboBox(this);
    widthCombo_->setObjectName(QStringLiteral("inspectorWidthCombo"));
    widthCombo_->setAccessibleName(tr("Bytes per row"));
    for (const int width : {8, 16, 24, 32}) {
        widthCombo_->addItem(tr("%1 bytes per row").arg(width), width);
    }
    widthCombo_->setCurrentIndex(1);
    optionRow->addWidget(widthCombo_);
    compareCombo_ = new QComboBox(this);
    compareCombo_->setObjectName(QStringLiteral("inspectorCompareCombo"));
    compareCombo_->setAccessibleName(tr("Which bytes are shown"));
    compareCombo_->addItem(tr("Current bytes"), static_cast<int>(Compare::current));
    compareCombo_->addItem(tr("Original bytes"), static_cast<int>(Compare::original));
    compareCombo_->addItem(tr("Original | current"), static_cast<int>(Compare::sideBySide));
    optionRow->addWidget(compareCombo_);
    auto *bookmarkButton = new QPushButton(tr("Bookmark"), this);
    bookmarkButton->setObjectName(QStringLiteral("inspectorBookmarkButton"));
    bookmarkButton->setToolTip(tr("Add or remove a bookmark at the cursor"));
    optionRow->addWidget(bookmarkButton);
    bookmarkCombo_ = new QComboBox(this);
    bookmarkCombo_->setObjectName(QStringLiteral("inspectorBookmarkCombo"));
    bookmarkCombo_->setAccessibleName(tr("Bookmarks"));
    optionRow->addWidget(bookmarkCombo_, 1);
    auto *copyButton = new QPushButton(tr("Copy as Hex"), this);
    copyButton->setObjectName(QStringLiteral("inspectorCopyButton"));
    optionRow->addWidget(copyButton);
    layout->addLayout(optionRow);

    auto *views = new QHBoxLayout;
    originalView_ = new HexView(this);
    originalView_->setObjectName(QStringLiteral("inspectorOriginalView"));
    originalView_->setSource(HexView::Source::original);
    originalView_->hide();
    currentView_ = new HexView(this);
    currentView_->setObjectName(QStringLiteral("inspectorCurrentView"));
    views->addWidget(originalView_, 1);
    views->addWidget(currentView_, 1);
    layout->addLayout(views, 1);

    message_ = new QLabel(this);
    message_->setObjectName(QStringLiteral("inspectorMessage"));
    layout->addWidget(message_);
    details_ = new QLabel(this);
    details_->setObjectName(QStringLiteral("inspectorDetails"));
    details_->setWordWrap(true);
    details_->setTextFormat(Qt::PlainText);
    details_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(details_);

    connect(jumpEdit_, &QLineEdit::returnPressed, this, [this] {
        message_->setText(goToText(jumpEdit_->text()) ? QString()
                                                      : tr("Nothing at \"%1\".").arg(jumpEdit_->text()));
    });
    connect(sectionCombo_, &QComboBox::activated, this, [this](int index) {
        if (!rebuilding_ && index >= 0) {
            goToOffset(sectionCombo_->itemData(index).toULongLong());
            emit offsetChosenByUser(cursorOffset());
        }
    });
    const auto search = [this] {
        message_->setText(findNext(searchEdit_->text(), searchMode_->currentIndex() == 1)
                              ? QString()
                              : tr("Not found."));
    };
    connect(searchEdit_, &QLineEdit::returnPressed, this, search);
    connect(findButton, &QPushButton::clicked, this, search);
    connect(widthCombo_, &QComboBox::currentIndexChanged, this,
            [this](int index) { setBytesPerRow(widthCombo_->itemData(index).toInt()); });
    connect(compareCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        setCompare(static_cast<Compare>(compareCombo_->itemData(index).toInt()));
    });
    connect(bookmarkButton, &QPushButton::clicked, this, [this] { toggleBookmark(); });
    connect(bookmarkCombo_, &QComboBox::activated, this, [this](int index) {
        if (!rebuilding_ && index >= 0) {
            goToOffset(bookmarkCombo_->itemData(index).toULongLong());
            emit offsetChosenByUser(cursorOffset());
        }
    });
    connect(copyButton, &QPushButton::clicked, this, [this] { currentView_->copySelection(); });

    connect(currentView_, &HexView::cursorChanged, this, [this] { refreshDetails(); });
    connect(currentView_, &HexView::cursorMovedByUser, this,
            [this] { emit offsetChosenByUser(cursorOffset()); });
    // The two views scroll and move together.
    connect(currentView_->verticalScrollBar(), &QScrollBar::valueChanged,
            originalView_->verticalScrollBar(), &QScrollBar::setValue);
    connect(originalView_->verticalScrollBar(), &QScrollBar::valueChanged,
            currentView_->verticalScrollBar(), &QScrollBar::setValue);
    connect(originalView_, &HexView::cursorMovedByUser, this, [this] {
        currentView_->setCursorOffset(originalView_->cursorOffset());
        emit offsetChosenByUser(cursorOffset());
    });
    connect(currentView_, &HexView::cursorChanged, this, [this] {
        if (originalView_->cursorOffset() != currentView_->cursorOffset()) {
            originalView_->setCursorOffset(currentView_->cursorOffset());
        }
    });
    setMinimumHeight(220);
    refresh();
}

void InspectorPanel::setTab(DocumentTab *tab) {
    if (tab_ == tab) {
        return;
    }
    if (tab_) {
        disconnect(tab_, nullptr, this, nullptr);
    }
    tab_ = tab;
    if (tab_) {
        connect(tab_, &DocumentTab::documentChanged, this, [this] { refresh(); });
        connect(tab_, &DocumentTab::profileChanged, this, [this] { refreshDetails(); });
        connect(tab_, &DocumentTab::documentReplaced, this, [this] { adoptDocument(); });
    }
    adoptDocument();
}

void InspectorPanel::adoptDocument() {
    const WorldDocument *document = tab_ ? &tab_->document() : nullptr;
    currentView_->setDocument(document);
    originalView_->setDocument(document);
    rebuildSections();
    rebuildBookmarks();
    refresh();
}

void InspectorPanel::rebuildSections() {
    rebuilding_ = true;
    sectionCombo_->clear();
    if (tab_) {
        for (const auto &jump : sectionJumps(tab_->document().probe())) {
            sectionCombo_->addItem(
                tr("%1 @ %2").arg(QString::fromStdString(jump.name)).arg(jump.offset),
                static_cast<qulonglong>(jump.offset));
        }
    }
    sectionCombo_->setEnabled(sectionCombo_->count() > 0);
    rebuilding_ = false;
}

void InspectorPanel::rebuildBookmarks() {
    rebuilding_ = true;
    bookmarkCombo_->clear();
    const auto marks = bookmarks();
    if (tab_) {
        for (const auto offset : marks) {
            bookmarkCombo_->addItem(
                tr("%1  %2")
                    .arg(offset)
                    .arg(QString::fromStdString(offsetLabel(tab_->document().probe(), offset))),
                static_cast<qulonglong>(offset));
        }
    }
    bookmarkCombo_->setEnabled(!marks.empty());
    currentView_->setBookmarks(marks);
    originalView_->setBookmarks(marks);
    rebuilding_ = false;
}

std::vector<std::size_t> InspectorPanel::bookmarks() const {
    return tab_ ? tab_->bookmarks() : std::vector<std::size_t>{};
}

void InspectorPanel::toggleBookmark() {
    if (!tab_ || currentView_->bytes().empty()) {
        return;
    }
    tab_->toggleBookmark(cursorOffset());
    rebuildBookmarks();
}

void InspectorPanel::refresh() {
    currentView_->viewport()->update();
    originalView_->viewport()->update();
    refreshDetails();
}

QString InspectorPanel::detailsText() const {
    return details_->text();
}

void InspectorPanel::refreshDetails() {
    if (!tab_ || currentView_->bytes().empty()) {
        details_->setText(tab_ ? tr("The file is empty.") : QString());
        return;
    }
    const auto &document = tab_->document();
    const auto &probe = document.probe();
    const auto offset = cursorOffset();
    const auto location = locateOffset(probe, offset);
    QStringList lines;
    lines << tr("Offset %1 (0x%2) - %3 - offset in section %4")
                 .arg(offset)
                 .arg(offset, 0, 16)
                 .arg(QString::fromStdString(offsetLabel(probe, offset)))
                 .arg(location.areaOffset);
    if (const auto calculation = offsetCalculation(probe, offset); !calculation.empty()) {
        lines << QString::fromStdString(calculation);
    }
    const auto describe = [&](std::span<const std::byte> bytes) -> QString {
        const auto element = decodeScalarAt(bytes, probe, offset);
        if (!element) {
            return tr("byte 0x%1")
                .arg(std::to_integer<unsigned>(bytes[offset]), 2, 16, QLatin1Char('0'));
        }
        return tr("signed %1, unsigned %2, bytes %3")
            .arg(element->signedValue)
            .arg(element->unsignedValue)
            .arg(QString::fromStdString(
                formatHex(bytes.subspan(element->range.offset, element->range.size))));
    };
    if (const auto element = decodeScalarAt(document.currentBytes(), probe, offset)) {
        lines << tr("Value at %1, %2 byte(s) little-endian: %3")
                     .arg(element->range.offset)
                     .arg(element->width)
                     .arg(describe(document.currentBytes()));
    } else {
        lines << tr("Current: %1").arg(describe(document.currentBytes()));
    }
    const bool changed = document.currentBytes()[offset] != document.originalBytes()[offset];
    const bool unsaved = document.currentBytes()[offset] != document.savedBytes()[offset];
    lines << tr("Original: %1%2")
                 .arg(describe(document.originalBytes()))
                 .arg(changed ? tr("  (this byte differs from the original)") : QString());
    if (unsaved) {
        lines << tr("Saved: %1  (this byte has unsaved changes)").arg(describe(document.savedBytes()));
    }
    lines << (probe.descriptor != nullptr
                  ? tr("Layout assumed from revision field %1.%2; values are shown as stored. "
                       "Their meaning depends on the game profile (%3).")
                        .arg(probe.descriptor->major)
                        .arg(probe.descriptor->minor)
                        .arg(profileDisplayName(tab_->profile()))
                  : tr("No layout is known for this file; only raw bytes are shown."));
    details_->setText(lines.join(QLatin1Char('\n')));
}

void InspectorPanel::goToOffset(std::size_t offset, std::size_t length) {
    if (currentView_->bytes().empty()) {
        return;
    }
    currentView_->setCursorOffset(offset);
    if (length > 1) {
        currentView_->setCursorOffset(offset + length - 1, true);
    }
    refreshDetails();
}

bool InspectorPanel::goToText(const QString &text) {
    if (!tab_) {
        return false;
    }
    const auto &probe = tab_->document().probe();
    const auto size = currentView_->bytes().size();
    const auto trimmed = text.trimmed().toLower();
    static const QRegularExpression cellPattern(
        QStringLiteral("^(cell|fg|foreground|big|bigdecor)\\s*(\\d+)\\s*[, ]\\s*(\\d+)$"));
    static const QRegularExpression slotPattern(QStringLiteral("^(slot|object)\\s*(\\d+)$"));
    std::optional<ByteRange> range;
    if (const auto match = cellPattern.match(trimmed); match.hasMatch()) {
        if (probe.descriptor == nullptr) {
            return false;
        }
        const auto layer = match.captured(1).startsWith(QLatin1String("big"))
                               ? SectionKind::bigDecor
                               : SectionKind::foreground;
        range = cellByteRange(*probe.descriptor, layer,
                              {match.captured(2).toULongLong(), match.captured(3).toULongLong()});
    } else if (const auto slotMatch = slotPattern.match(trimmed); slotMatch.hasMatch()) {
        if (probe.descriptor == nullptr) {
            return false;
        }
        range = slotByteRange(*probe.descriptor, slotMatch.captured(2).toULongLong());
    } else {
        bool ok = false;
        const auto offset = trimmed.startsWith(QLatin1String("0x"))
                                ? trimmed.mid(2).toULongLong(&ok, 16)
                                : trimmed.toULongLong(&ok, 10);
        if (ok) {
            range = ByteRange{static_cast<std::size_t>(offset), 1};
        }
    }
    if (!range || range->offset >= size) {
        return false;
    }
    goToOffset(range->offset, std::min(range->size, size - range->offset));
    emit offsetChosenByUser(range->offset);
    return true;
}

bool InspectorPanel::goToSection(const QString &name) {
    if (!tab_) {
        return false;
    }
    for (const auto &jump : sectionJumps(tab_->document().probe())) {
        if (QString::fromStdString(jump.name).compare(name, Qt::CaseInsensitive) == 0) {
            goToOffset(jump.offset);
            return true;
        }
    }
    return false;
}

bool InspectorPanel::findNext(const QString &pattern, bool ascii) {
    const auto haystack = currentView_->bytes();
    if (haystack.empty()) {
        return false;
    }
    std::vector<std::byte> needle;
    if (ascii) {
        const auto latin = pattern.toLatin1();
        needle = toByteVector(latin);
    } else if (const auto parsed = parseHexPattern(pattern.toStdString())) {
        needle = *parsed;
    }
    if (needle.empty()) {
        return false;
    }
    const auto found = findBytes(haystack, needle, cursorOffset() + 1, true);
    if (!found) {
        return false;
    }
    goToOffset(*found, needle.size());
    emit offsetChosenByUser(*found);
    return true;
}

void InspectorPanel::setCompare(Compare compare) {
    currentView_->setSource(compare == Compare::original ? HexView::Source::original
                                                         : HexView::Source::current);
    originalView_->setVisible(compare == Compare::sideBySide);
    const auto index = compareCombo_->findData(static_cast<int>(compare));
    if (compareCombo_->currentIndex() != index) {
        compareCombo_->setCurrentIndex(index);
    }
}

void InspectorPanel::setBytesPerRow(int count) {
    currentView_->setBytesPerRow(count);
    originalView_->setBytesPerRow(count);
    const auto index = widthCombo_->findData(count);
    if (index >= 0 && widthCombo_->currentIndex() != index) {
        widthCombo_->setCurrentIndex(index);
    }
}

} // namespace eggbert
