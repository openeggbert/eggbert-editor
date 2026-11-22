// SPDX-License-Identifier: GPL-3.0-only
#include "hex_view.hpp"

#include <QApplication>
#include <QClipboard>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>

#include <algorithm>

#include "world/hex_model.hpp"

namespace eggbert {

namespace {

constexpr int offsetDigits = 8;
constexpr int leftMargin = 6;

QColor areaTint(OffsetArea area, bool dark) {
    const int alpha = dark ? 70 : 45;
    switch (area) {
    case OffsetArea::header:
        return QColor(90, 140, 255, alpha);
    case OffsetArea::foreground:
        return QColor(90, 200, 120, alpha);
    case OffsetArea::bigDecor:
        return QColor(200, 170, 80, alpha);
    case OffsetArea::objects:
        return QColor(210, 110, 200, alpha);
    case OffsetArea::suffix:
        return QColor(150, 150, 150, alpha);
    case OffsetArea::unmapped:
        break;
    }
    return QColor(0, 0, 0, 0);
}

} // namespace

HexView::HexView(QWidget *parent) : QAbstractScrollArea(parent) {
    setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    setFocusPolicy(Qt::StrongFocus);
    const QFontMetrics metrics(font());
    charWidth_ = std::max(1, metrics.horizontalAdvance(QLatin1Char('0')));
    rowHeight_ = std::max(1, metrics.height() + 2);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, viewport(),
            qOverload<>(&QWidget::update));
}

void HexView::setDocument(const WorldDocument *document) {
    document_ = document;
    cursor_ = std::min(cursor_, bytes().empty() ? 0 : bytes().size() - 1);
    anchor_ = cursor_;
    updateScrollBar();
    viewport()->update();
}

void HexView::setSource(Source source) {
    if (source_ == source) {
        return;
    }
    source_ = source;
    viewport()->update();
}

void HexView::setBytesPerRow(int count) {
    count = std::clamp(count, 4, 64);
    if (count == bytesPerRow_) {
        return;
    }
    bytesPerRow_ = count;
    updateScrollBar();
    setCursorOffset(cursor_, true);
    viewport()->update();
}

void HexView::setBookmarks(std::vector<std::size_t> offsets) {
    bookmarks_ = std::move(offsets);
    viewport()->update();
}

std::span<const std::byte> HexView::bytes() const {
    if (document_ == nullptr) {
        return {};
    }
    switch (source_) {
    case Source::current:
        return document_->currentBytes();
    case Source::original:
        return document_->originalBytes();
    case Source::saved:
        return document_->savedBytes();
    }
    return {};
}

std::size_t HexView::rowCount() const {
    const auto size = bytes().size();
    return (size + static_cast<std::size_t>(bytesPerRow_) - 1) /
           static_cast<std::size_t>(bytesPerRow_);
}

ByteRange HexView::selection() const {
    if (bytes().empty()) {
        return {};
    }
    const auto first = std::min(anchor_, cursor_);
    const auto last = std::max(anchor_, cursor_);
    return {first, last - first + 1};
}

std::size_t HexView::firstVisibleRow() const {
    return static_cast<std::size_t>(verticalScrollBar()->value());
}

int HexView::visibleRowCount() const {
    return std::max(1, viewport()->height() / rowHeight_);
}

void HexView::updateScrollBar() {
    const auto rows = rowCount();
    const auto visible = static_cast<std::size_t>(visibleRowCount());
    verticalScrollBar()->setRange(0, rows > visible ? static_cast<int>(rows - visible) : 0);
    verticalScrollBar()->setPageStep(static_cast<int>(visible));
}

void HexView::setCursorOffset(std::size_t offset, bool extendSelection) {
    const auto size = bytes().size();
    if (size == 0) {
        return;
    }
    offset = std::min(offset, size - 1);
    const bool changed = offset != cursor_ || (!extendSelection && anchor_ != offset);
    cursor_ = offset;
    if (!extendSelection) {
        anchor_ = offset;
    }
    // Scroll the cursor row into view.
    const auto row = offset / static_cast<std::size_t>(bytesPerRow_);
    const auto first = firstVisibleRow();
    const auto visible = static_cast<std::size_t>(visibleRowCount());
    if (row < first) {
        verticalScrollBar()->setValue(static_cast<int>(row));
    } else if (row >= first + visible) {
        verticalScrollBar()->setValue(static_cast<int>(row - visible + 1));
    }
    viewport()->update();
    if (changed) {
        emit cursorChanged();
    }
}

void HexView::moveByUser(std::size_t offset, bool extend) {
    const auto before = cursor_;
    setCursorOffset(offset, extend);
    if (cursor_ != before || !extend) {
        emit cursorMovedByUser();
    }
}

QString HexView::selectionAsHex() const {
    const auto range = selection();
    if (range.empty()) {
        return {};
    }
    return QString::fromStdString(formatHex(bytes().subspan(range.offset, range.size)));
}

void HexView::copySelection() const {
    QApplication::clipboard()->setText(selectionAsHex());
}

int HexView::hexColumnX(int column) const {
    // "oooooooo  " then three characters per byte and one extra gap after
    // every eighth byte.
    return leftMargin + charWidth_ * (offsetDigits + 2 + 3 * column + column / 8);
}

int HexView::asciiColumnX(int column) const {
    return hexColumnX(bytesPerRow_) + charWidth_ * (2 + column);
}

QRect HexView::byteRect(std::size_t offset) const {
    const auto row = offset / static_cast<std::size_t>(bytesPerRow_);
    const auto column = static_cast<int>(offset % static_cast<std::size_t>(bytesPerRow_));
    const auto y = (static_cast<long long>(row) - static_cast<long long>(firstVisibleRow())) *
                   rowHeight_;
    return QRect(hexColumnX(column), static_cast<int>(y), 2 * charWidth_, rowHeight_);
}

std::optional<std::size_t> HexView::offsetAt(const QPoint &position) const {
    if (position.y() < 0) {
        return std::nullopt;
    }
    const auto row = firstVisibleRow() + static_cast<std::size_t>(position.y() / rowHeight_);
    int column = -1;
    if (position.x() >= asciiColumnX(0)) {
        column = (position.x() - asciiColumnX(0)) / charWidth_;
    } else {
        for (int candidate = 0; candidate < bytesPerRow_; ++candidate) {
            const auto x = hexColumnX(candidate);
            if (position.x() >= x - charWidth_ / 2 && position.x() < x + 2 * charWidth_ + charWidth_ / 2) {
                column = candidate;
                break;
            }
        }
    }
    if (column < 0 || column >= bytesPerRow_) {
        return std::nullopt;
    }
    const auto offset = row * static_cast<std::size_t>(bytesPerRow_) + static_cast<std::size_t>(column);
    if (offset >= bytes().size()) {
        return std::nullopt;
    }
    return offset;
}

void HexView::paintEvent(QPaintEvent *) {
    QPainter painter(viewport());
    const auto palette = this->palette();
    painter.fillRect(viewport()->rect(), palette.base());
    const auto data = bytes();
    if (document_ == nullptr || data.empty()) {
        painter.setPen(palette.color(QPalette::Disabled, QPalette::Text));
        painter.drawText(viewport()->rect(), Qt::AlignCenter,
                         document_ == nullptr ? tr("No document") : tr("The file is empty"));
        return;
    }
    const bool dark = palette.base().color().lightness() < 128;
    const auto &probe = document_->probe();
    const auto original = document_->originalBytes();
    const auto saved = document_->savedBytes();
    const auto selected = selection();
    const QFontMetrics metrics(font());
    const int baseline = metrics.ascent() + 1;

    const auto first = firstVisibleRow();
    const auto rows = static_cast<std::size_t>(viewport()->height() / rowHeight_ + 1);
    for (std::size_t line = 0; line < rows; ++line) {
        const auto row = first + line;
        const auto rowStart = row * static_cast<std::size_t>(bytesPerRow_);
        if (rowStart >= data.size()) {
            break;
        }
        const int y = static_cast<int>(line) * rowHeight_;
        painter.setPen(palette.color(QPalette::Disabled, QPalette::Text));
        painter.drawText(leftMargin, y + baseline,
                         QStringLiteral("%1").arg(rowStart, offsetDigits, 16, QLatin1Char('0')));
        for (int column = 0; column < bytesPerRow_; ++column) {
            const auto offset = rowStart + static_cast<std::size_t>(column);
            if (offset >= data.size()) {
                break;
            }
            const auto value = std::to_integer<unsigned>(data[offset]);
            const QRect hexRect(hexColumnX(column) - charWidth_ / 2, y, 3 * charWidth_, rowHeight_);
            const QRect asciiRect(asciiColumnX(column), y, charWidth_, rowHeight_);
            painter.fillRect(hexRect, areaTint(locateOffset(probe, offset).area, dark));
            // Differences are always judged on the current bytes.
            const auto current = document_->currentBytes()[offset];
            const bool changed = current != original[offset];
            const bool unsaved = current != saved[offset];
            if (changed) {
                painter.fillRect(hexRect, QColor(255, 140, 0, 110));
                painter.fillRect(asciiRect, QColor(255, 140, 0, 110));
            }
            if (selected.contains(offset)) {
                painter.fillRect(hexRect, palette.highlight());
                painter.fillRect(asciiRect, palette.highlight());
                painter.setPen(palette.highlightedText().color());
            } else {
                painter.setPen(palette.text().color());
            }
            painter.drawText(hexColumnX(column), y + baseline,
                             QStringLiteral("%1").arg(value, 2, 16, QLatin1Char('0')));
            painter.drawText(asciiColumnX(column), y + baseline,
                             value >= 0x20 && value < 0x7F ? QString(QChar(value))
                                                           : QStringLiteral("."));
            if (unsaved) {
                painter.setPen(QPen(QColor(230, 60, 60), 2));
                painter.drawLine(hexRect.left() + 2, hexRect.bottom(), hexRect.right() - 2,
                                 hexRect.bottom());
            }
            if (std::ranges::find(bookmarks_, offset) != bookmarks_.end()) {
                painter.setPen(QPen(QColor(60, 160, 255), 2));
                painter.drawRect(hexRect.adjusted(1, 1, -2, -2));
            }
            if (offset == cursor_ && hasFocus()) {
                painter.setPen(QPen(palette.text().color(), 1, Qt::DotLine));
                painter.drawRect(hexRect.adjusted(0, 0, -1, -1));
            }
        }
    }
    // Truncation notice after the last row.
    if (probe.missingBytes != 0) {
        const auto lastRow = rowCount();
        if (lastRow >= first && lastRow < first + rows) {
            painter.setPen(QColor(230, 60, 60));
            painter.drawText(leftMargin,
                             static_cast<int>(lastRow - first) * rowHeight_ + baseline,
                             tr("truncated: %1 byte(s) missing from offset %2")
                                 .arg(probe.missingBytes)
                                 .arg(probe.firstMissingOffset.value_or(data.size())));
        }
    }
}

void HexView::resizeEvent(QResizeEvent *event) {
    QAbstractScrollArea::resizeEvent(event);
    updateScrollBar();
}

void HexView::mousePressEvent(QMouseEvent *event) {
    setFocus(Qt::MouseFocusReason);
    if (event->button() != Qt::LeftButton) {
        return;
    }
    if (const auto offset = offsetAt(event->position().toPoint())) {
        moveByUser(*offset, event->modifiers() & Qt::ShiftModifier);
    }
}

void HexView::mouseMoveEvent(QMouseEvent *event) {
    if (!(event->buttons() & Qt::LeftButton)) {
        return;
    }
    if (const auto offset = offsetAt(event->position().toPoint())) {
        moveByUser(*offset, true);
    }
}

void HexView::keyPressEvent(QKeyEvent *event) {
    const auto size = bytes().size();
    if (size == 0) {
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    if (event->matches(QKeySequence::Copy)) {
        copySelection();
        return;
    }
    if (event->matches(QKeySequence::SelectAll)) {
        anchor_ = 0;
        setCursorOffset(size - 1, true);
        return;
    }
    const bool extend = event->modifiers() & Qt::ShiftModifier;
    const auto perRow = static_cast<std::size_t>(bytesPerRow_);
    const auto page = perRow * static_cast<std::size_t>(visibleRowCount());
    auto target = cursor_;
    switch (event->key()) {
    case Qt::Key_Left:
        target = cursor_ > 0 ? cursor_ - 1 : 0;
        break;
    case Qt::Key_Right:
        target = cursor_ + 1;
        break;
    case Qt::Key_Up:
        target = cursor_ >= perRow ? cursor_ - perRow : cursor_;
        break;
    case Qt::Key_Down:
        target = cursor_ + perRow < size ? cursor_ + perRow : cursor_;
        break;
    case Qt::Key_PageUp:
        target = cursor_ >= page ? cursor_ - page : cursor_ % perRow;
        break;
    case Qt::Key_PageDown:
        target = cursor_ + page < size ? cursor_ + page : size - 1;
        break;
    case Qt::Key_Home:
        target = (event->modifiers() & Qt::ControlModifier) ? 0 : cursor_ - cursor_ % perRow;
        break;
    case Qt::Key_End:
        target = (event->modifiers() & Qt::ControlModifier)
                     ? size - 1
                     : std::min(size - 1, cursor_ - cursor_ % perRow + perRow - 1);
        break;
    default:
        QAbstractScrollArea::keyPressEvent(event);
        return;
    }
    moveByUser(std::min(target, size - 1), extend);
}

} // namespace eggbert
