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

void WorldCanvas::userViewChange() {
    // Any explicit view change stops the automatic fit on resize.
    autoFit_ = false;
    applyViewChange();
}

void WorldCanvas::setZoom(double scale) {
    transform_.zoomAbout(width() / 2.0, height() / 2.0, scale);
    userViewChange();
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
    autoFit_ = true;
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
    userViewChange();
}

void WorldCanvas::centerOnWorld(double worldX, double worldY) {
    transform_.originX = worldX - width() / (2 * transform_.scale);
    transform_.originY = worldY - height() / (2 * transform_.scale);
    userViewChange();
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
    userViewChange();
}

void WorldCanvas::setSelectionMask(const CellMask &mask) {
    selection_ = mask;
    update();
}

void WorldCanvas::setPreviewCells(std::vector<CellPos> cells) {
    if (cells.empty() && preview_.empty()) {
        return;
    }
    preview_ = std::move(cells);
    update();
}

void WorldCanvas::setSelectedSlot(std::optional<std::size_t> slot) {
    if (selectedSlot_ == slot) {
        return;
    }
    selectedSlot_ = slot;
    update();
}

void WorldCanvas::setHighlights(std::vector<CellPos> cells, std::vector<std::size_t> objectSlots) {
    markedCells_ = std::move(cells);
    markedSlots_ = std::move(objectSlots);
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
    // On the canvas the backdrop scrolls at two thirds of the map's speed,
    // as in the games.
    auto options = options_;
    options.backdropParallax = true;
    renderWorld(painter, document_->view(), transform_, QRectF(rect()), options);

    const auto cellRect = [this](CellPos first, CellPos last) {
        const auto left = std::floor(transform_.toScreenX(static_cast<double>(first.x) * cell));
        const auto top = std::floor(transform_.toScreenY(static_cast<double>(first.y) * cell));
        const auto right =
            std::floor(transform_.toScreenX(static_cast<double>(last.x + 1) * cell));
        const auto bottom =
            std::floor(transform_.toScreenY(static_cast<double>(last.y + 1) * cell));
        return QRectF(left, top, right - left, bottom - top);
    };
    if (!selection_.empty()) {
        // Tint selected cells in view and outline the selection's bounds.
        const auto first = transform_.cellAtScreen(0, 0).value_or(CellPos{0, 0});
        const auto visible = visibleWorldRect();
        const auto lastX = std::min<std::size_t>(
            layout::mapWidth - 1,
            static_cast<std::size_t>(std::max(0.0, visible.right() / cell)));
        const auto lastY = std::min<std::size_t>(
            layout::mapHeight - 1,
            static_cast<std::size_t>(std::max(0.0, visible.bottom() / cell)));
        const auto firstX = visible.left() > 0 ? static_cast<std::size_t>(visible.left() / cell) : 0;
        const auto firstY = visible.top() > 0 ? static_cast<std::size_t>(visible.top() / cell) : 0;
        (void)first;
        for (std::size_t x = firstX; x <= lastX; ++x) {
            for (std::size_t y = firstY; y <= lastY; ++y) {
                if (selection_.test({x, y})) {
                    painter.fillRect(cellRect({x, y}, {x, y}), QColor(80, 160, 255, 70));
                }
            }
        }
        if (const auto bounds = selection_.bounds()) {
            painter.setPen(QPen(QColor(120, 190, 255), 1, Qt::DashLine));
            painter.drawRect(cellRect(bounds->min, bounds->max).adjusted(0, 0, -1, -1));
        }
    }
    if (!preview_.empty()) {
        const auto visible = QRectF(rect());
        for (const auto &pending : preview_) {
            const auto area = cellRect(pending, pending);
            if (area.intersects(visible)) {
                painter.fillRect(area, QColor(255, 255, 255, 90));
            }
        }
    }
    if (!markedCells_.empty() || !markedSlots_.empty()) {
        const QColor mark(255, 150, 0);
        painter.setPen(QPen(mark, 2));
        const auto visible = QRectF(rect());
        for (const auto &marked : markedCells_) {
            const auto area = cellRect(marked, marked);
            if (area.intersects(visible)) {
                painter.drawRect(area.adjusted(1, 1, -2, -2));
            }
        }
        if (const auto objects = document_->view().objects()) {
            for (const auto slot : markedSlots_) {
                const auto record = objects->record(slot);
                if (!record) {
                    continue;
                }
                const auto size = schematicSpriteSize(record->channel);
                const QRectF area(transform_.toScreenX(record->posCurrent.x),
                                  transform_.toScreenY(record->posCurrent.y),
                                  size * transform_.scale, size * transform_.scale);
                painter.drawEllipse(area.adjusted(-3, -3, 3, 3));
            }
        }
    }
    if (selectedSlot_) {
        const auto objects = document_->view().objects();
        const auto record = objects ? objects->record(*selectedSlot_) : std::nullopt;
        if (record) {
            const auto size = schematicSpriteSize(record->channel);
            const auto box = [&](Point position) {
                return QRectF(std::floor(transform_.toScreenX(position.x)),
                              std::floor(transform_.toScreenY(position.y)),
                              size * transform_.scale, size * transform_.scale);
            };
            if (record->posStart != record->posEnd) {
                painter.setPen(QPen(QColor(255, 255, 255), 1, Qt::DashLine));
                painter.drawLine(box(record->posStart).center(), box(record->posEnd).center());
                painter.setPen(QPen(QColor(120, 255, 120), 2));
                painter.drawRect(box(record->posStart).adjusted(1, 1, -2, -2));
                painter.setPen(QPen(QColor(255, 120, 120), 2));
                painter.drawRect(box(record->posEnd).adjusted(1, 1, -2, -2));
            }
            painter.setPen(QPen(QColor(0, 0, 0), 4));
            painter.drawRect(box(record->posCurrent).adjusted(0, 0, -1, -1));
            painter.setPen(QPen(QColor(255, 255, 255), 2));
            painter.drawRect(box(record->posCurrent).adjusted(0, 0, -1, -1));
        }
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
    // Until the user changes the view, keep the whole world fitted while
    // the surrounding layout settles.
    if (autoFit_ && width() > 0 && height() > 0) {
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
        userViewChange();
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
            userViewChange();
        }
    } else if (event->modifiers() & Qt::ShiftModifier) {
        transform_.panByScreen(delta.y() != 0 ? delta.y() : delta.x(), 0);
        userViewChange();
    } else {
        transform_.panByScreen(delta.x(), delta.y());
        userViewChange();
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
            userViewChange();
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
