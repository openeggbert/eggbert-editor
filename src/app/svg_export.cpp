// SPDX-License-Identifier: GPL-3.0-only
#include "app/svg_export.hpp"

#include <QString>

#include <algorithm>

#include "app/world_renderer.hpp"

namespace eggbert {

namespace {

QString number(double value) {
    // Short, locale-independent, stable.
    auto text = QString::number(value, 'f', 2);
    while (text.contains(QLatin1Char('.')) &&
           (text.endsWith(QLatin1Char('0')) || text.endsWith(QLatin1Char('.')))) {
        const bool dot = text.endsWith(QLatin1Char('.'));
        text.chop(1);
        if (dot) {
            break;
        }
    }
    return text;
}

} // namespace

QByteArray renderWorldSvg(const WorldView &world, const SvgExportOptions &options) {
    const auto foreground = world.foreground();
    if (!foreground) {
        return {};
    }
    const int cell = std::clamp(options.cellSize, 1, 64);
    const int size = static_cast<int>(layout::mapWidth) * cell;
    const double scale = static_cast<double>(cell) / layout::cellPixels;
    QString svg;
    svg += QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    svg += QStringLiteral("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%1\" height=\"%1\" "
                          "viewBox=\"0 0 %1 %1\">\n")
               .arg(size);
    svg += QStringLiteral("<title>Eggbert Editor schematic export</title>\n");
    svg += QStringLiteral("<rect id=\"background\" width=\"%1\" height=\"%1\" fill=\"%2\"/>\n")
               .arg(size)
               .arg(schematicBackgroundColor().name());
    const auto layer = [&](const TileLayerView &view, SectionKind kind, const QString &id,
                           bool outline) {
        svg += QStringLiteral("<g id=\"%1\">\n").arg(id);
        for (std::size_t x = 0; x < layout::mapWidth; ++x) {
            for (std::size_t y = 0; y < layout::mapHeight; ++y) {
                const auto value = *view.cell(x, y);
                if (value == layout::emptyCell) {
                    continue;
                }
                const auto color = schematicTileColor(kind, value).name();
                svg += outline
                           ? QStringLiteral("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%3\" "
                                            "fill=\"none\" stroke=\"%4\" data-tile=\"%5\"/>\n")
                                 .arg(static_cast<int>(x) * cell)
                                 .arg(static_cast<int>(y) * cell)
                                 .arg(cell)
                                 .arg(color)
                                 .arg(value)
                           : QStringLiteral("<rect x=\"%1\" y=\"%2\" width=\"%3\" height=\"%3\" "
                                            "fill=\"%4\" data-tile=\"%5\"/>\n")
                                 .arg(static_cast<int>(x) * cell)
                                 .arg(static_cast<int>(y) * cell)
                                 .arg(cell)
                                 .arg(color)
                                 .arg(value);
            }
        }
        svg += QStringLiteral("</g>\n");
    };
    if (const auto big = world.bigDecor(); big && options.showBigDecor) {
        layer(*big, SectionKind::bigDecor, QStringLiteral("bigdecor"), true);
    }
    if (options.showForeground) {
        layer(*foreground, SectionKind::foreground, QStringLiteral("foreground"), false);
    }
    const auto centre = [&](Point position) {
        return QPointF((position.x + layout::cellPixels / 2.0) * scale,
                       (position.y + layout::cellPixels / 2.0) * scale);
    };
    if (const auto objects = world.objects(); objects && options.showObjects) {
        svg += QStringLiteral("<g id=\"objects\">\n");
        for (std::size_t slot = 0; slot < objects->slotCount(); ++slot) {
            const auto record = *objects->record(slot);
            if (record.isFree()) {
                continue;
            }
            const auto start = centre(record.posStart);
            const auto color = schematicObjectColor(record.type).name();
            if (record.posStart != record.posEnd) {
                const auto end = centre(record.posEnd);
                svg += QStringLiteral("<line x1=\"%1\" y1=\"%2\" x2=\"%3\" y2=\"%4\" stroke=\"%5\" "
                                      "stroke-dasharray=\"3 2\"/>\n")
                           .arg(number(start.x()), number(start.y()), number(end.x()),
                                number(end.y()), color);
            }
            svg += QStringLiteral("<circle cx=\"%1\" cy=\"%2\" r=\"%3\" fill=\"%4\" "
                                  "data-slot=\"%5\" data-type=\"%6\"/>\n")
                       .arg(number(start.x()), number(start.y()), number(cell * 0.4), color)
                       .arg(slot)
                       .arg(record.type);
        }
        svg += QStringLiteral("</g>\n");
    }
    if (const auto header = world.header(); header && options.showStarts) {
        svg += QStringLiteral("<g id=\"starts\">\n");
        for (std::size_t index = 0; index < 4; ++index) {
            const auto at = centre(*header->blupiPos(index));
            const double r = cell * 0.45;
            svg += QStringLiteral("<polygon points=\"%1,%2 %3,%4 %1,%5 %6,%4\" fill=\"%7\" "
                                  "data-start=\"%8\"/>\n")
                       .arg(number(at.x()), number(at.y() - r), number(at.x() + r),
                            number(at.y()), number(at.y() + r), number(at.x() - r),
                            schematicStartColor(index).name())
                       .arg(index + 1);
        }
        svg += QStringLiteral("</g>\n");
    }
    svg += QStringLiteral("</svg>\n");
    return svg.toUtf8();
}

} // namespace eggbert
