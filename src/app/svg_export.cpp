// SPDX-License-Identifier: GPL-3.0-only
#include "app/svg_export.hpp"

#include <QBuffer>
#include <QPainter>
#include <QXmlStreamWriter>
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

// Keep the sprites at their original resolution even when the requested SVG
// display size is small. Bands limit peak image memory on large worlds.
QByteArray artworkSvg(const WorldView &world, const SvgExportOptions &options, int displaySize) {
    RenderOptions render;
    render.artwork = options.artwork;
    render.showForeground = options.showForeground;
    render.showBigDecor = options.showBigDecor;
    render.showObjects = options.showObjects;
    render.showStarts = options.showStarts;
    render.showPaths = false;
    render.showLabels = false;

    constexpr int width = layout::worldPixelWidth;
    constexpr int height = layout::worldPixelHeight;
    constexpr int bandHeight = 512;
    QByteArray svg;
    QXmlStreamWriter xml(&svg);
    xml.writeStartDocument();
    xml.writeStartElement(QStringLiteral("svg"));
    xml.writeDefaultNamespace(QStringLiteral("http://www.w3.org/2000/svg"));
    xml.writeNamespace(QStringLiteral("http://www.w3.org/1999/xlink"), QStringLiteral("xlink"));
    xml.writeAttribute(QStringLiteral("width"), QString::number(displaySize));
    xml.writeAttribute(QStringLiteral("height"), QString::number(displaySize));
    xml.writeAttribute(QStringLiteral("viewBox"), QStringLiteral("0 0 %1 %2").arg(width).arg(height));
    xml.writeTextElement(QStringLiteral("title"), QStringLiteral("Eggbert Editor world export"));
    for (int top = 0; top < height; top += bandHeight) {
        QImage band(width, std::min(bandHeight, height - top), QImage::Format_RGB32);
        if (band.isNull()) {
            return {};
        }
        QPainter painter(&band);
        // Use the transform and viewport of the whole map. Sprites whose
        // anchor is outside this band can still overlap it.
        painter.translate(0, -top);
        ViewTransform transform;
        transform.scale = 1.0;
        renderWorld(painter, world, transform, QRectF(0, 0, width, height), render);
        painter.end();
        QBuffer png;
        if (!png.open(QIODevice::WriteOnly) || !band.save(&png, "PNG")) {
            return {};
        }
        xml.writeEmptyElement(QStringLiteral("image"));
        xml.writeAttribute(QStringLiteral("y"), QString::number(top));
        xml.writeAttribute(QStringLiteral("width"), QString::number(width));
        xml.writeAttribute(QStringLiteral("height"), QString::number(band.height()));
        xml.writeAttribute(QStringLiteral("xlink:href"), QStringLiteral("data:image/png;base64,") +
                           QString::fromLatin1(png.data().toBase64()));
    }
    xml.writeEndDocument();
    return svg;
}

} // namespace

QByteArray renderWorldSvg(const WorldView &world, const SvgExportOptions &options) {
    const auto foreground = world.foreground();
    if (!foreground) {
        return {};
    }
    const int cell = std::clamp(options.cellSize, 1, 64);
    const int size = static_cast<int>(layout::mapWidth) * cell;
    if (options.artwork) {
        return options.artwork->usable() ? artworkSvg(world, options, size) : QByteArray{};
    }
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
                                      "stroke-dasharray=\"3 2\" data-path-of=\"%6\"/>\n")
                           .arg(number(start.x()), number(start.y()), number(end.x()),
                                number(end.y()), color)
                           .arg(slot);
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
