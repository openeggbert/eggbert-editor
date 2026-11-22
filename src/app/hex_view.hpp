// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QAbstractScrollArea>

#include <optional>
#include <vector>

#include "world/world_document.hpp"

namespace eggbert {

// Read-only, virtualized hex dump of one document: only visible rows are
// painted. It has no editing path at all.
class HexView final : public QAbstractScrollArea {
    Q_OBJECT

public:
    enum class Source { current, original, saved };

    explicit HexView(QWidget *parent = nullptr);

    void setDocument(const WorldDocument *document);
    [[nodiscard]] Source source() const { return source_; }
    void setSource(Source source);
    [[nodiscard]] int bytesPerRow() const { return bytesPerRow_; }
    void setBytesPerRow(int count);
    void setBookmarks(std::vector<std::size_t> offsets);

    // The bytes shown.
    [[nodiscard]] std::span<const std::byte> bytes() const;
    [[nodiscard]] std::size_t rowCount() const;

    [[nodiscard]] std::size_t cursorOffset() const { return cursor_; }
    // Inclusive selected range (anchor to cursor, ordered).
    [[nodiscard]] ByteRange selection() const;
    // Moves the cursor and scrolls it into view. Does not emit
    // cursorMovedByUser.
    void setCursorOffset(std::size_t offset, bool extendSelection = false);
    [[nodiscard]] QString selectionAsHex() const;
    void copySelection() const;

    // Geometry helpers.
    [[nodiscard]] std::optional<std::size_t> offsetAt(const QPoint &viewportPosition) const;
    [[nodiscard]] QRect byteRect(std::size_t offset) const;
    [[nodiscard]] std::size_t firstVisibleRow() const;
    [[nodiscard]] int visibleRowCount() const;

signals:
    void cursorChanged();
    // Emitted only for mouse and keyboard navigation inside the view.
    void cursorMovedByUser();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void updateScrollBar();
    void moveByUser(std::size_t offset, bool extend);
    [[nodiscard]] int hexColumnX(int column) const;
    [[nodiscard]] int asciiColumnX(int column) const;

    const WorldDocument *document_ = nullptr;
    Source source_ = Source::current;
    int bytesPerRow_ = 16;
    std::size_t cursor_ = 0;
    std::size_t anchor_ = 0;
    std::vector<std::size_t> bookmarks_;
    int charWidth_ = 8;
    int rowHeight_ = 16;
};

} // namespace eggbert
