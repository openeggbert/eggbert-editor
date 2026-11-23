// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QImage>
#include <QPointer>
#include <QWidget>

#include "app/document_tab.hpp"

namespace eggbert {

// Whole-world overview with the canvas viewport outlined. Clicking or
// dragging moves the canvas; the document is never touched.
class MinimapWidget final : public QWidget {
    Q_OBJECT

public:
    enum class Mode { composite, foreground, bigDecor };

    explicit MinimapWidget(QWidget *parent = nullptr);

    void setTab(DocumentTab *tab);
    [[nodiscard]] Mode mode() const { return mode_; }
    void setMode(Mode mode);

    // Square area of the widget that shows the 6400 x 6400 world.
    [[nodiscard]] QRectF worldArea() const;
    // The canvas viewport in widget coordinates (may extend past worldArea).
    [[nodiscard]] QRectF viewportRect() const;
    // World position under a widget position.
    [[nodiscard]] QPointF worldAt(const QPointF &widgetPosition) const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    QSize sizeHint() const override { return {220, 220}; }

private:
    void invalidate();
    void panTo(const QPointF &widgetPosition);
    // Takes over the artwork and game the tab's map is drawn with.
    void followCanvas();

    QPointer<DocumentTab> tab_;
    Mode mode_ = Mode::composite;
    QImage cache_;
    bool dirty_ = true;
    // What the map is drawn with, so that the small map looks like it:
    // the game's pictures while drawing with game artwork is on.
    std::shared_ptr<const GameAssets> artwork_;
    GameProfile profile_ = GameProfile::unknown;
};

} // namespace eggbert
