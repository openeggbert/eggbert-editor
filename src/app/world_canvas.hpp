// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QWidget>

#include <optional>

#include "app/view_transform.hpp"
#include "app/world_renderer.hpp"
#include "world/world_document.hpp"

namespace eggbert {

// Scrollable, zoomable view of one world. It never edits the document
// itself: tool gestures are reported as cell-level signals.
class WorldCanvas final : public QWidget {
    Q_OBJECT

public:
    explicit WorldCanvas(const WorldDocument *document, QWidget *parent = nullptr);

    void setDocument(const WorldDocument *document);

    [[nodiscard]] const ViewTransform &transform() const { return transform_; }
    [[nodiscard]] const RenderOptions &renderOptions() const { return options_; }
    void setRenderOptions(const RenderOptions &options);

    // Screen pixels per world pixel.
    [[nodiscard]] double zoom() const { return transform_.scale; }
    void setZoom(double scale);
    void zoomIn();
    void zoomOut();
    void actualPixels();
    void fitWorld();
    void fitCells(CellPos topLeft, CellPos bottomRight);
    void centerOnCell(CellPos cell);
    // World rectangle currently visible, in world pixels.
    [[nodiscard]] QRectF visibleWorldRect() const;
    void centerOnWorld(double worldX, double worldY);

    // Keyboard cursor cell, shown with a focus outline.
    [[nodiscard]] CellPos cursorCell() const { return cursor_; }
    void setCursorCell(CellPos cell);
    [[nodiscard]] std::optional<CellPos> hoveredCell() const { return hovered_; }

    // When false, left-button gestures pan instead of invoking the tool.
    void setToolActive(bool active) { toolActive_ = active; }

    // Optional highlighted cell rectangle (inclusive corners).
    void setSelection(std::optional<std::pair<CellPos, CellPos>> selection);

signals:
    void hoveredCellChanged();
    void cursorCellChanged();
    void viewChanged();
    // One tool gesture: pressed, zero or more dragged (only when the cell
    // changes), then released. Keyboard activation sends pressed + released.
    void toolPressed(eggbert::CellPos cell);
    void toolDragged(eggbert::CellPos cell);
    void toolReleased();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void applyViewChange();
    void setHovered(std::optional<CellPos> cell);
    void ensureCursorVisible();

    const WorldDocument *document_;
    ViewTransform transform_;
    RenderOptions options_;
    CellPos cursor_{0, 0};
    std::optional<CellPos> hovered_;
    std::optional<std::pair<CellPos, CellPos>> selection_;
    std::optional<CellPos> lastToolCell_;
    bool toolActive_ = true;
    bool toolGesture_ = false;
    bool panning_ = false;
    bool fitted_ = false;
    QPointF lastPanPosition_;
};

} // namespace eggbert

Q_DECLARE_METATYPE(eggbert::CellPos)
