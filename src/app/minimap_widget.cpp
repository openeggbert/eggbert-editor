// SPDX-License-Identifier: GPL-3.0-only
#include "minimap_widget.hpp"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>

namespace eggbert {

MinimapWidget::MinimapWidget(QWidget *parent) : QWidget(parent) {
    setMinimumSize(120, 120);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void MinimapWidget::setTab(DocumentTab *tab) {
    if (tab_ == tab) {
        return;
    }
    if (tab_) {
        disconnect(tab_, nullptr, this, nullptr);
        disconnect(tab_->canvas(), nullptr, this, nullptr);
    }
    tab_ = tab;
    if (tab_) {
        connect(tab_, &DocumentTab::documentChanged, this, [this] { invalidate(); });
        connect(tab_->canvas(), &WorldCanvas::viewChanged, this, [this] { update(); });
        connect(tab_->canvas(), &WorldCanvas::renderOptionsChanged, this,
                [this] { followCanvas(); });
    }
    artwork_.reset();
    profile_ = GameProfile::unknown;
    followCanvas();
    invalidate();
}

void MinimapWidget::followCanvas() {
    // Only artwork and game matter here: the options also change with every
    // animation frame, which the small map does not show.
    std::shared_ptr<const GameAssets> artwork;
    auto profile = GameProfile::unknown;
    if (tab_) {
        artwork = tab_->canvas()->renderOptions().artwork;
        profile = tab_->canvas()->renderOptions().profile;
    }
    if (artwork != artwork_ || profile != profile_) {
        artwork_ = std::move(artwork);
        profile_ = profile;
        invalidate();
    }
}

void MinimapWidget::setMode(Mode mode) {
    if (mode_ == mode) {
        return;
    }
    mode_ = mode;
    invalidate();
}

void MinimapWidget::invalidate() {
    dirty_ = true;
    update();
}

QRectF MinimapWidget::worldArea() const {
    const auto side = std::max(1, std::min(width(), height()) - 4);
    return QRectF((width() - side) / 2.0, (height() - side) / 2.0, side, side);
}

QRectF MinimapWidget::viewportRect() const {
    if (!tab_) {
        return {};
    }
    const auto area = worldArea();
    const auto scale = area.width() / layout::worldPixelWidth;
    const auto visible = tab_->canvas()->visibleWorldRect();
    return QRectF(area.left() + visible.left() * scale, area.top() + visible.top() * scale,
                  visible.width() * scale, visible.height() * scale);
}

QPointF MinimapWidget::worldAt(const QPointF &widgetPosition) const {
    const auto area = worldArea();
    const auto scale = layout::worldPixelWidth / area.width();
    return QPointF((widgetPosition.x() - area.left()) * scale,
                   (widgetPosition.y() - area.top()) * scale);
}

void MinimapWidget::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.fillRect(rect(), schematicOutsideColor());
    if (!tab_) {
        return;
    }
    const auto area = worldArea().toRect();
    if (dirty_ || cache_.size() != area.size()) {
        RenderOptions options;
        options.showLabels = false;
        options.showGrid = false;
        options.showPaths = false;
        options.showForeground = mode_ != Mode::bigDecor;
        options.showBigDecor = mode_ != Mode::foreground;
        options.showObjects = mode_ == Mode::composite;
        options.showStarts = mode_ == Mode::composite;
        // The game's pictures on a plain ground; a backdrop would only blur
        // a map this small.
        options.artwork = artwork_;
        options.profile = profile_;
        options.showBackdrop = false;
        ViewTransform transform;
        transform.scale = static_cast<double>(area.width()) / layout::worldPixelWidth;
        cache_ = QImage(area.size(), QImage::Format_ARGB32_Premultiplied);
        cache_.fill(schematicBackgroundColor());
        QPainter cachePainter(&cache_);
        renderWorld(cachePainter, tab_->document().view(), transform,
                    QRectF(0, 0, area.width(), area.height()), options);
        dirty_ = false;
    }
    painter.drawImage(area.topLeft(), cache_);

    const auto viewport = viewportRect().intersected(QRectF(area));
    painter.setPen(QPen(QColor(0, 0, 0), 3));
    painter.drawRect(viewport.adjusted(0, 0, -1, -1));
    painter.setPen(QPen(QColor(255, 255, 255), 1));
    painter.drawRect(viewport.adjusted(0, 0, -1, -1));
}

void MinimapWidget::resizeEvent(QResizeEvent *) {
    invalidate();
}

void MinimapWidget::panTo(const QPointF &widgetPosition) {
    if (!tab_) {
        return;
    }
    const auto world = worldAt(widgetPosition);
    tab_->canvas()->centerOnWorld(
        std::clamp(world.x(), 0.0, static_cast<double>(layout::worldPixelWidth)),
        std::clamp(world.y(), 0.0, static_cast<double>(layout::worldPixelHeight)));
}

void MinimapWidget::mousePressEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        panTo(event->position());
    }
}

void MinimapWidget::mouseMoveEvent(QMouseEvent *event) {
    if (event->buttons() & Qt::LeftButton) {
        panTo(event->position());
    }
}

} // namespace eggbert
