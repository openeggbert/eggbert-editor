// SPDX-License-Identifier: GPL-3.0-only
#include "world_renderer.hpp"

#include <QFont>
#include <QPainter>
#include <QPen>

#include <algorithm>
#include <cmath>

namespace eggbert {

namespace {

constexpr int cell = layout::cellPixels;
constexpr int bigDecorSize = 2 * cell;
// The engines draw a bigDecor tile two pixels below its anchor cell.
constexpr int bigDecorYOffset = 2;

struct CellRange {
    int x0 = 0;
    int y0 = 0;
    int x1 = -1; // inclusive
    int y1 = -1;
};

// Cells intersecting the viewport, widened by `before` cells to the left and
// top for tiles that extend beyond their anchor cell.
CellRange visibleCells(const ViewTransform &transform, const QRectF &viewport, int before) {
    const auto clampCell = [](double value, int limit) {
        return static_cast<int>(std::clamp(value, 0.0, static_cast<double>(limit - 1)));
    };
    CellRange range;
    range.x0 = clampCell(std::floor(transform.toWorldX(viewport.left()) / cell) - before,
                         static_cast<int>(layout::mapWidth));
    range.y0 = clampCell(std::floor(transform.toWorldY(viewport.top()) / cell) - before,
                         static_cast<int>(layout::mapHeight));
    range.x1 = clampCell(std::floor(transform.toWorldX(viewport.right()) / cell),
                         static_cast<int>(layout::mapWidth));
    range.y1 = clampCell(std::floor(transform.toWorldY(viewport.bottom()) / cell),
                         static_cast<int>(layout::mapHeight));
    if (transform.toWorldX(viewport.right()) < 0 || transform.toWorldY(viewport.bottom()) < 0 ||
        transform.toWorldX(viewport.left()) >= layout::worldPixelWidth + before * cell ||
        transform.toWorldY(viewport.top()) >= layout::worldPixelHeight + before * cell) {
        range.x1 = range.x0 - 1; // nothing visible
    }
    return range;
}

// Screen rectangle of a world-pixel rectangle, snapped so that adjacent
// rectangles share an edge without gaps or overlap.
QRect screenRect(const ViewTransform &transform, double worldX, double worldY, double width,
                 double height) {
    const auto left = static_cast<int>(std::floor(transform.toScreenX(worldX)));
    const auto top = static_cast<int>(std::floor(transform.toScreenY(worldY)));
    const auto right = static_cast<int>(std::floor(transform.toScreenX(worldX + width)));
    const auto bottom = static_cast<int>(std::floor(transform.toScreenY(worldY + height)));
    return QRect(left, top, std::max(1, right - left), std::max(1, bottom - top));
}

void drawLabel(QPainter &painter, const QRect &rect, const QString &text, const QColor &color) {
    QFont font = painter.font();
    font.setPixelSize(std::clamp(rect.height() / 3, 8, 14));
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(rect, Qt::AlignCenter | Qt::TextDontClip, text);
}

// Exactly the colour an 8-bit-per-channel image stores, so a rendered pixel
// compares equal to the palette entry it was drawn with.
QColor eightBit(const QColor &color) {
    return QColor::fromRgb(color.rgb());
}

QColor readableOn(const QColor &background) {
    return background.lightness() > 140 ? QColor(20, 20, 20) : QColor(245, 245, 245);
}

} // namespace

QColor schematicBackgroundColor() {
    return QColor(30, 33, 40);
}

QColor schematicOutsideColor() {
    return QColor(18, 19, 23);
}

QColor schematicTileColor(SectionKind layer, std::int16_t id) {
    if (id < layout::emptyCell) {
        return QColor(255, 0, 200); // not a tile number any engine reads as valid
    }
    const int hue = (static_cast<int>(id) * 47 + 11) % 360;
    if (layer == SectionKind::bigDecor) {
        return eightBit(QColor::fromHsv(hue, 70, 120 + (id * 13) % 40));
    }
    return eightBit(QColor::fromHsv(hue, 150 + (id * 29) % 80, 170 + (id * 17) % 70));
}

QColor schematicObjectColor(std::int16_t type) {
    const int hue = ((static_cast<int>(type) % 360 + 360) * 67 + 200) % 360;
    return eightBit(QColor::fromHsv(hue, 220, 255));
}

QColor schematicStartColor(std::size_t index) {
    static const QColor colors[] = {QColor(255, 214, 10), QColor(255, 122, 60),
                                    QColor(70, 190, 255), QColor(120, 230, 120)};
    return colors[index % 4];
}

int schematicSpriteSize(std::int16_t channel) {
    if (channel == 1) {
        return cell;
    }
    if (channel == 9) {
        return bigDecorSize;
    }
    return 60;
}

void renderWorld(QPainter &painter, const WorldView &world, const ViewTransform &transform,
                 const QRectF &viewport, const RenderOptions &options) {
    painter.save();
    painter.setClipRect(viewport, Qt::IntersectClip);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const auto worldRect =
        screenRect(transform, 0, 0, layout::worldPixelWidth, layout::worldPixelHeight);
    if (!options.transparentBackground) {
        painter.fillRect(viewport, schematicOutsideColor());
        painter.fillRect(worldRect, schematicBackgroundColor());
    }
    const double cellScreen = cell * transform.scale;
    const bool labels = options.showLabels && cellScreen >= 26.0;

    // bigDecor: each stored value covers 2 x 2 cells from its anchor cell.
    if (const auto layer = world.bigDecor(); layer && options.showBigDecor) {
        const auto range = visibleCells(transform, viewport, 1);
        for (int x = range.x0; x <= range.x1; ++x) {
            for (int y = range.y0; y <= range.y1; ++y) {
                const auto id = *layer->cell(x, y);
                if (id == layout::emptyCell) {
                    continue;
                }
                const auto rect = screenRect(transform, x * cell, y * cell + bigDecorYOffset,
                                             bigDecorSize, bigDecorSize);
                const auto color = schematicTileColor(SectionKind::bigDecor, id);
                painter.fillRect(rect, color);
                if (cellScreen >= 8.0) {
                    painter.setPen(QPen(color.lighter(150), 0));
                    painter.drawRect(rect.adjusted(0, 0, -1, -1));
                }
                if (labels) {
                    drawLabel(painter, rect, QStringLiteral("B%1").arg(id), readableOn(color));
                }
            }
        }
    }

    const auto header = world.header();

    // Player starts, last slot first so slot 1 ends on top.
    if (header && options.showStarts) {
        for (int index = 3; index >= 0; --index) {
            const auto position = *header->blupiPos(index);
            const auto rect = screenRect(transform, position.x, position.y, 60, 60);
            const auto color = schematicStartColor(index);
            painter.setRenderHint(QPainter::Antialiasing, cellScreen >= 16.0);
            painter.setPen(QPen(QColor(20, 20, 20), 0));
            painter.setBrush(color);
            painter.drawEllipse(rect.adjusted(0, 0, -1, -1));
            painter.setBrush(Qt::NoBrush);
            painter.setRenderHint(QPainter::Antialiasing, false);
            if (labels) {
                const auto direction = *header->blupiDir(index);
                const auto arrow = direction == 1   ? QStringLiteral("<%1")
                                   : direction == 2 ? QStringLiteral("%1>")
                                                    : QStringLiteral("%1?");
                drawLabel(painter, rect, arrow.arg(index + 1), QColor(20, 20, 20));
            }
        }
    }

    const auto objects = world.objects();
    const auto objectRect = [&](const ObjectRecord &record, Point position) {
        const auto size = schematicSpriteSize(record.channel);
        return screenRect(transform, position.x, position.y, size, size);
    };

    // Object bodies, highest slot first as the engines iterate.
    if (objects && options.showObjects) {
        for (std::size_t remaining = objects->slotCount(); remaining > 0; --remaining) {
            const auto record = *objects->record(remaining - 1);
            if (record.isFree()) {
                continue;
            }
            const auto rect = objectRect(record, record.posCurrent);
            if (!QRectF(rect).intersects(viewport)) {
                continue;
            }
            auto color = schematicObjectColor(record.type);
            color.setAlpha(150);
            painter.fillRect(rect, color);
        }
    }

    // Foreground cells.
    if (const auto layer = world.foreground(); layer && options.showForeground) {
        const auto range = visibleCells(transform, viewport, 0);
        for (int x = range.x0; x <= range.x1; ++x) {
            for (int y = range.y0; y <= range.y1; ++y) {
                const auto id = *layer->cell(x, y);
                if (id == layout::emptyCell) {
                    continue;
                }
                const auto rect = screenRect(transform, x * cell, y * cell, cell, cell);
                const auto color = schematicTileColor(SectionKind::foreground, id);
                painter.fillRect(rect, color);
                if (cellScreen >= 10.0) {
                    painter.setPen(QPen(color.darker(140), 0));
                    painter.drawLine(rect.bottomLeft(), rect.bottomRight());
                    painter.drawLine(rect.topRight(), rect.bottomRight());
                }
                if (labels) {
                    drawLabel(painter, rect, QString::number(id), readableOn(color));
                }
            }
        }
    }

    // Editor overlays: motion paths, object outlines and slot labels.
    if (objects && (options.showObjects || options.showPaths)) {
        for (std::size_t slot = 0; slot < objects->slotCount(); ++slot) {
            const auto record = *objects->record(slot);
            if (record.isFree()) {
                continue;
            }
            const auto size = schematicSpriteSize(record.channel);
            const auto color = schematicObjectColor(record.type);
            const auto centre = [&](Point position) {
                return QPointF(transform.toScreenX(position.x + size / 2.0),
                               transform.toScreenY(position.y + size / 2.0));
            };
            if (options.showPaths && record.posStart != record.posEnd) {
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.setPen(QPen(color, 2, Qt::DashLine));
                painter.drawLine(centre(record.posStart), centre(record.posEnd));
                painter.setPen(QPen(color, 2));
                const auto end = objectRect(record, record.posEnd);
                painter.drawRect(end.adjusted(1, 1, -2, -2));
                painter.setRenderHint(QPainter::Antialiasing, false);
            }
            if (options.showObjects) {
                const auto rect = objectRect(record, record.posCurrent);
                if (!QRectF(rect).intersects(viewport)) {
                    continue;
                }
                painter.setPen(QPen(color, cellScreen >= 16.0 ? 2 : 0));
                painter.drawRect(rect.adjusted(0, 0, -1, -1));
                if (labels) {
                    drawLabel(painter, rect,
                              QStringLiteral("T%1\n#%2").arg(record.type).arg(slot),
                              QColor(255, 255, 255));
                }
            }
        }
    }

    if (options.showGrid && cellScreen >= 6.0) {
        const auto range = visibleCells(transform, viewport, 0);
        for (int x = range.x0; x <= range.x1 + 1; ++x) {
            painter.setPen(QPen(QColor(255, 255, 255, x % 10 == 0 ? 110 : 45), 0));
            const auto screenX = static_cast<int>(std::floor(transform.toScreenX(x * cell)));
            painter.drawLine(screenX, worldRect.top(), screenX, worldRect.bottom());
        }
        for (int y = range.y0; y <= range.y1 + 1; ++y) {
            painter.setPen(QPen(QColor(255, 255, 255, y % 10 == 0 ? 110 : 45), 0));
            const auto screenY = static_cast<int>(std::floor(transform.toScreenY(y * cell)));
            painter.drawLine(worldRect.left(), screenY, worldRect.right(), screenY);
        }
    }
    painter.restore();
}

QImage renderWorldImage(const WorldView &world, double scale, const RenderOptions &options) {
    ViewTransform transform;
    transform.scale = std::clamp(scale, ViewTransform::minimumScale, ViewTransform::maximumScale);
    const auto width = static_cast<int>(std::lround(layout::worldPixelWidth * transform.scale));
    const auto height = static_cast<int>(std::lround(layout::worldPixelHeight * transform.scale));
    QImage image(std::max(1, width), std::max(1, height), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    renderWorld(painter, world, transform, QRectF(0, 0, image.width(), image.height()), options);
    return image;
}

} // namespace eggbert
