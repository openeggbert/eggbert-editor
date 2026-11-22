// SPDX-License-Identifier: GPL-3.0-only
#include "tool_icons.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace eggbert {

namespace {

// Everything is drawn on a 32 x 32 grid and scaled for larger pixmaps.
void draw(QPainter &p, Tool tool, const QColor &ink) {
    const QColor accent(70, 150, 220);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(ink, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    switch (tool) {
    case Tool::browse: { // four arrows: move the view
        p.drawLine(QPointF(16, 5), QPointF(16, 27));
        p.drawLine(QPointF(5, 16), QPointF(27, 16));
        for (const auto &[tip, a, b] :
             {std::tuple{QPointF(16, 4), QPointF(12, 9), QPointF(20, 9)},
              std::tuple{QPointF(16, 28), QPointF(12, 23), QPointF(20, 23)},
              std::tuple{QPointF(4, 16), QPointF(9, 12), QPointF(9, 20)},
              std::tuple{QPointF(28, 16), QPointF(23, 12), QPointF(23, 20)}}) {
            p.drawLine(tip, a);
            p.drawLine(tip, b);
        }
        break;
    }
    case Tool::select:
        p.setPen(QPen(ink, 2, Qt::DashLine));
        p.drawRect(QRectF(6, 7, 20, 18));
        break;
    case Tool::pencil: {
        QPainterPath body;
        body.moveTo(7, 25);
        body.lineTo(9, 19);
        body.lineTo(22, 6);
        body.lineTo(26, 10);
        body.lineTo(13, 23);
        body.closeSubpath();
        p.setBrush(accent);
        p.drawPath(body);
        p.drawLine(QPointF(9, 19), QPointF(13, 23));
        break;
    }
    case Tool::eraser: {
        QPainterPath block;
        block.moveTo(6, 20);
        block.lineTo(16, 8);
        block.lineTo(26, 14);
        block.lineTo(16, 26);
        block.lineTo(10, 26);
        block.closeSubpath();
        p.setBrush(QColor(235, 130, 150));
        p.drawPath(block);
        p.drawLine(QPointF(11, 14), QPointF(21, 20));
        break;
    }
    case Tool::eyedropper:
        p.drawLine(QPointF(8, 24), QPointF(19, 13));
        p.setBrush(accent);
        p.drawEllipse(QPointF(22, 10), 4.5, 4.5);
        p.drawLine(QPointF(6, 26), QPointF(8, 24));
        break;
    case Tool::line:
        p.drawLine(QPointF(7, 25), QPointF(25, 7));
        p.setBrush(accent);
        p.drawEllipse(QPointF(7, 25), 2.5, 2.5);
        p.drawEllipse(QPointF(25, 7), 2.5, 2.5);
        break;
    case Tool::rectangle:
        p.setBrush(accent);
        p.drawRect(QRectF(6, 9, 20, 14));
        break;
    case Tool::fill: { // a tipped bucket with a drop
        QPainterPath bucket;
        bucket.moveTo(8, 14);
        bucket.lineTo(18, 6);
        bucket.lineTo(25, 15);
        bucket.lineTo(15, 23);
        bucket.closeSubpath();
        p.setBrush(accent);
        p.drawPath(bucket);
        p.setBrush(ink);
        p.drawEllipse(QPointF(25, 23), 2.2, 3.2);
        break;
    }
    case Tool::adaptive: { // a block whose border follows its neighbours
        p.setBrush(accent);
        p.drawRoundedRect(QRectF(6, 6, 20, 20), 6, 6);
        p.setPen(QPen(ink, 1.4));
        p.drawLine(QPointF(16, 6), QPointF(16, 26));
        p.drawLine(QPointF(6, 16), QPointF(26, 16));
        break;
    }
    case Tool::start: { // a flag
        p.drawLine(QPointF(9, 5), QPointF(9, 27));
        QPainterPath flag;
        flag.moveTo(9, 6);
        flag.lineTo(25, 10);
        flag.lineTo(9, 16);
        flag.closeSubpath();
        p.setBrush(accent);
        p.drawPath(flag);
        break;
    }
    case Tool::object: { // an arrow pointer
        QPainterPath arrow;
        arrow.moveTo(8, 5);
        arrow.lineTo(8, 24);
        arrow.lineTo(13, 19);
        arrow.lineTo(17, 27);
        arrow.lineTo(20, 25);
        arrow.lineTo(16, 18);
        arrow.lineTo(23, 18);
        arrow.closeSubpath();
        p.setBrush(accent);
        p.drawPath(arrow);
        break;
    }
    case Tool::placeObject: // a box with a plus
        p.setBrush(accent);
        p.drawRect(QRectF(6, 10, 16, 16));
        p.setPen(QPen(ink, 2.6, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(24, 4), QPointF(24, 12));
        p.drawLine(QPointF(20, 8), QPointF(28, 8));
        break;
    }
}

} // namespace

QIcon toolIcon(Tool tool, const QColor &ink) {
    QIcon icon;
    for (const int size : {24, 32, 48, 64}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.scale(size / 32.0, size / 32.0);
        draw(painter, tool, ink);
        painter.end();
        icon.addPixmap(pixmap);
    }
    return icon;
}

} // namespace eggbert
