// SPDX-License-Identifier: GPL-3.0-only
#include "world_canvas.hpp"

#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace eggbert {

namespace {

constexpr double zoomStep = 1.25;
constexpr int cell = layout::cellPixels;

// Snap to "nice" scales so nearest-neighbour artwork stays crisp at and
// above actual size.
double steppedZoom(double scale, bool in) {
    const auto next = in ? scale * zoomStep : scale / zoomStep;
    if (next > 0.9 && next < 1.1) {
        return 1.0;
    }
    return next;
}

} // namespace

WorldCanvas::WorldCanvas(const WorldDocument *document, QWidget *parent)
    : QWidget(parent), document_(document) {
    qRegisterMetaType<eggbert::CellPos>();
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(200, 150);
    setAttribute(Qt::WA_OpaquePaintEvent);
}

void WorldCanvas::setDocument(const WorldDocument *document) {
    document_ = document;
    update();
}

void WorldCanvas::setRenderOptions(const RenderOptions &options) {
    if (options_ == options) {
        return;
    }
    options_ = options;
    update();
}

void WorldCanvas::applyViewChange() {
    transform_.clampToWorld(width(), height());
    update();
    emit viewChanged();
}

void WorldCanvas::setZoom(double scale) {
    transform_.zoomAbout(width() / 2.0, height() / 2.0, scale);
    applyViewChange();
}

void WorldCanvas::zoomIn() {
    setZoom(steppedZoom(transform_.scale, true));
}

void WorldCanvas::zoomOut() {
    setZoom(steppedZoom(transform_.scale, false));
}

void WorldCanvas::actualPixels() {
    setZoom(1.0);
}

void WorldCanvas::fitWorld() {
    transform_.fit(0, 0, layout::worldPixelWidth, layout::worldPixelHeight, width(), height(), 8);
    fitted_ = true;
    applyViewChange();
}

void WorldCanvas::fitCells(CellPos topLeft, CellPos bottomRight) {
    const auto left = std::min(topLeft.x, bottomRight.x);
    const auto top = std::min(topLeft.y, bottomRight.y);
    const auto right = std::max(topLeft.x, bottomRight.x);
    const auto bottom = std::max(topLeft.y, bottomRight.y);
    transform_.fit(static_cast<double>(left) * cell, static_cast<double>(top) * cell,
                   static_cast<double>(right - left + 1) * cell,
                   static_cast<double>(bottom - top + 1) * cell, width(), height(), 24);
    applyViewChange();
}

void WorldCanvas::centerOnWorld(double worldX, double worldY) {
    transform_.originX = worldX - width() / (2 * transform_.scale);
    transform_.originY = worldY - height() / (2 * transform_.scale);
    applyViewChange();
}

void WorldCanvas::centerOnCell(CellPos target) {
    centerOnWorld((static_cast<double>(target.x) + 0.5) * cell,
                  (static_cast<double>(target.y) + 0.5) * cell);
}

QRectF WorldCanvas::visibleWorldRect() const {
    return QRectF(transform_.toWorldX(0), transform_.toWorldY(0), width() / transform_.scale,
                  height() / transform_.scale);
}

void WorldCanvas::setCursorCell(CellPos target) {
    target.x = std::min(target.x, layout::mapWidth - 1);
    target.y = std::min(target.y, layout::mapHeight - 1);
    if (cursor_ == target) {
        return;
    }
    cursor_ = target;
    ensureCursorVisible();
    update();
    emit cursorCellChanged();
}

void WorldCanvas::ensureCursorVisible() {
    const auto visible = visibleWorldRect();
    const QRectF cursorRect(static_cast<double>(cursor_.x) * cell,
                            static_cast<double>(cursor_.y) * cell, cell, cell);
    if (visible.contains(cursorRect)) {
        return;
    }
    if (cursorRect.left() < visible.left()) {
        transform_.originX = cursorRect.left();
    } else if (cursorRect.right() > visible.right()) {
        transform_.originX = cursorRect.right() - visible.width();
    }
    if (cursorRect.top() < visible.top()) {
        transform_.originY = cursorRect.top();
    } else if (cursorRect.bottom() > visible.bottom()) {
        transform_.originY = cursorRect.bottom() - visible.height();
    }
    applyViewChange();
}

void WorldCanvas::setSelection(std::optional<std::pair<CellPos, CellPos>> selection) {
    selection_ = selection;
    update();
}

void WorldCanvas::setHovered(std::optional<CellPos> target) {
    if (hovered_ == target) {
        return;
    }
    hovered_ = target;
    update();
    emit hoveredCellChanged();
}

void WorldCanvas::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    if (document_ == nullptr) {
        painter.fillRect(rect(), schematicOutsideColor());
        return;
    }
    renderWorld(painter, document_->view(), transform_, QRectF(rect()), options_);

    const auto cellRect = [this](CellPos first, CellPos last) {
        const auto left = std::floor(transform_.toScreenX(static_cast<double>(first.x) * cell));
        const auto top = std::floor(transform_.toScreenY(static_cast<double>(first.y) * cell));
        const auto right =
            std::floor(transform_.toScreenX(static_cast<double>(last.x + 1) * cell));
        const auto bottom =
            std::floor(transform_.toScreenY(static_cast<double>(last.y + 1) * cell));
        return QRectF(left, top, right - left, bottom - top);
    };
    if (selection_) {
        const CellPos first{std::min(selection_->first.x, selection_->second.x),
                            std::min(selection_->first.y, selection_->second.y)};
        const CellPos last{std::max(selection_->first.x, selection_->second.x),
                           std::max(selection_->first.y, selection_->second.y)};
        const auto area = cellRect(first, last);
        painter.fillRect(area, QColor(80, 160, 255, 60));
        painter.setPen(QPen(QColor(120, 190, 255), 1, Qt::DashLine));
        painter.drawRect(area.adjusted(0, 0, -1, -1));
    }
    if (hovered_) {
        painter.setPen(QPen(QColor(255, 255, 255, 200), 1));
        painter.drawRect(cellRect(*hovered_, *hovered_).adjusted(0, 0, -1, -1));
    }
    if (hasFocus()) {
        // Visible keyboard focus: the cursor cell.
        const auto area = cellRect(cursor_, cursor_);
        painter.setPen(QPen(QColor(0, 0, 0), 3));
        painter.drawRect(area.adjusted(0, 0, -1, -1));
        painter.setPen(QPen(QColor(255, 220, 60), 1));
        painter.drawRect(area.adjusted(0, 0, -1, -1));
    }
}

void WorldCanvas::resizeEvent(QResizeEvent *) {
    if (!fitted_ && width() > 0 && height() > 0) {
        fitWorld();
    } else {
        applyViewChange();
    }
}

void WorldCanvas::mousePressEvent(QMouseEvent *event) {
    setFocus(Qt::MouseFocusReason);
    const bool panButton = event->button() == Qt::MiddleButton ||
                           (event->button() == Qt::LeftButton &&
                            (!toolActive_ || (event->modifiers() & Qt::AltModifier)));
    if (panButton) {
        panning_ = true;
        lastPanPosition_ = event->position();
        setCursor(Qt::ClosedHandCursor);
        return;
    }
    if (event->button() == Qt::LeftButton) {
        const auto target = transform_.cellAtScreen(event->position().x(), event->position().y());
        if (target) {
            toolGesture_ = true;
            lastToolCell_ = target;
            if (cursor_ != *target) {
                cursor_ = *target;
                emit cursorCellChanged();
            }
            emit toolPressed(*target);
            update();
        }
    }
}

void WorldCanvas::mouseMoveEvent(QMouseEvent *event) {
    if (panning_) {
        const auto delta = event->position() - lastPanPosition_;
        lastPanPosition_ = event->position();
        transform_.panByScreen(delta.x(), delta.y());
        applyViewChange();
        return;
    }
    const auto target = transform_.cellAtScreen(event->position().x(), event->position().y());
    setHovered(target);
    if (toolGesture_ && target && lastToolCell_ != target) {
        lastToolCell_ = target;
        emit toolDragged(*target);
    }
}

void WorldCanvas::mouseReleaseEvent(QMouseEvent *event) {
    if (panning_ && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        panning_ = false;
        unsetCursor();
        return;
    }
    if (toolGesture_ && event->button() == Qt::LeftButton) {
        toolGesture_ = false;
        lastToolCell_.reset();
        emit toolReleased();
    }
}

void WorldCanvas::leaveEvent(QEvent *) {
    setHovered(std::nullopt);
}

void WorldCanvas::wheelEvent(QWheelEvent *event) {
    const auto delta = event->angleDelta();
    if (event->modifiers() & Qt::ControlModifier) {
        if (delta.y() != 0) {
            // Zoom about the pointer.
            const auto factor = std::pow(zoomStep, delta.y() / 120.0);
            transform_.zoomAbout(event->position().x(), event->position().y(),
                                 transform_.scale * factor);
            applyViewChange();
        }
    } else if (event->modifiers() & Qt::ShiftModifier) {
        transform_.panByScreen(delta.y() != 0 ? delta.y() : delta.x(), 0);
        applyViewChange();
    } else {
        transform_.panByScreen(delta.x(), delta.y());
        applyViewChange();
    }
    setHovered(transform_.cellAtScreen(event->position().x(), event->position().y()));
    event->accept();
}

void WorldCanvas::keyPressEvent(QKeyEvent *event) {
    const bool pan = event->modifiers() & Qt::ControlModifier;
    const int step = (event->modifiers() & Qt::ShiftModifier) ? 10 : 1;
    const auto moveCursor = [&](int dx, int dy) {
        if (pan) {
            transform_.panByScreen(-dx * 64.0, -dy * 64.0);
            applyViewChange();
            return;
        }
        const auto clampAxis = [](std::size_t value, int delta, std::size_t limit) {
            const auto moved = static_cast<long long>(value) + delta;
            return static_cast<std::size_t>(
                std::clamp<long long>(moved, 0, static_cast<long long>(limit) - 1));
        };
        setCursorCell({clampAxis(cursor_.x, dx * step, layout::mapWidth),
                       clampAxis(cursor_.y, dy * step, layout::mapHeight)});
    };
    switch (event->key()) {
    case Qt::Key_Left:
        moveCursor(-1, 0);
        break;
    case Qt::Key_Right:
        moveCursor(1, 0);
        break;
    case Qt::Key_Up:
        moveCursor(0, -1);
        break;
    case Qt::Key_Down:
        moveCursor(0, 1);
        break;
    case Qt::Key_Home:
        setCursorCell({0, cursor_.y});
        break;
    case Qt::Key_End:
        setCursorCell({layout::mapWidth - 1, cursor_.y});
        break;
    case Qt::Key_PageUp:
        moveCursor(0, -10 / step);
        break;
    case Qt::Key_PageDown:
        moveCursor(0, 10 / step);
        break;
    case Qt::Key_Space:
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (toolActive_) {
            emit toolPressed(cursor_);
            emit toolReleased();
        }
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
}

} // namespace eggbert
