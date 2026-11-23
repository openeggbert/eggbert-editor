// SPDX-License-Identifier: GPL-3.0-only
#include "stamp_thumbnail.hpp"

#include <QPainter>

#include <algorithm>

#include "app/world_renderer.hpp"

namespace eggbert {

QImage stampThumbnail(const Stamp &stamp, int side) {
    QImage image(side, side, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    const auto cells = stamp.width * stamp.height;
    if (side <= 0 || cells == 0 || stamp.foreground.size() != cells ||
        stamp.bigDecor.size() != cells) {
        return image;
    }
    const auto longest = static_cast<double>(std::max(stamp.width, stamp.height));
    // Whole pixels per cell while the stamp fits that way.
    const auto cell = longest <= side ? std::floor(side / longest) : side / longest;
    const auto left = (side - cell * static_cast<double>(stamp.width)) / 2.0;
    const auto top = (side - cell * static_cast<double>(stamp.height)) / 2.0;
    QPainter painter(&image);
    painter.setPen(Qt::NoPen);
    for (std::size_t y = 0; y < stamp.height; ++y) {
        for (std::size_t x = 0; x < stamp.width; ++x) {
            const auto index = y * stamp.width + x;
            QColor color;
            if (stamp.foreground[index] != layout::emptyCell) {
                color = schematicTileColor(SectionKind::foreground, stamp.foreground[index]);
            } else if (stamp.bigDecor[index] != layout::emptyCell) {
                color = schematicTileColor(SectionKind::bigDecor, stamp.bigDecor[index]);
            } else {
                continue;
            }
            const QRectF box(left + cell * static_cast<double>(x),
                             top + cell * static_cast<double>(y), cell, cell);
            // At least one pixel, so tiles of a large stamp do not vanish.
            painter.fillRect(box.toAlignedRect(), color);
        }
    }
    painter.setRenderHint(QPainter::Antialiasing);
    for (const auto &record : stamp.objects) {
        const QPointF centre(left + cell * (record.posStart.x + layout::cellPixels / 2.0) /
                                        layout::cellPixels,
                             top + cell * (record.posStart.y + layout::cellPixels / 2.0) /
                                       layout::cellPixels);
        const auto radius = std::max(2.0, cell * 0.3);
        painter.setBrush(schematicObjectColor(record.type));
        painter.setPen(QPen(Qt::black, 1));
        painter.drawEllipse(centre, radius, radius);
    }
    return image;
}

} // namespace eggbert
