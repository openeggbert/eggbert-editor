// SPDX-License-Identifier: GPL-3.0-only
#include "png_export.hpp"

#include <QCoreApplication>
#include <QImageWriter>
#include <QPainter>
#include <QSaveFile>

#include <algorithm>

namespace eggbert {

namespace {

constexpr int bandHeight = 512;

int clampedPixelsPerCell(const PngExportOptions &options) {
    return std::clamp(options.pixelsPerCell, 1, layout::cellPixels);
}

QString profileKey(GameProfile profile) {
    switch (profile) {
    case GameProfile::speedyBlupi1:
        return QStringLiteral("speedy-blupi-1");
    case GameProfile::speedyBlupi2:
        return QStringLiteral("speedy-blupi-2");
    case GameProfile::unknown:
        break;
    }
    return QStringLiteral("not-chosen");
}

} // namespace

PngExportEstimate estimatePngExport(const PngExportOptions &options) {
    PngExportEstimate estimate;
    const auto pixels = clampedPixelsPerCell(options);
    estimate.width = static_cast<int>(layout::mapWidth) * pixels;
    estimate.height = static_cast<int>(layout::mapHeight) * pixels;
    estimate.imageBytes = static_cast<qint64>(estimate.width) * estimate.height * 4;
    return estimate;
}

QString describeExportSettings(const PngExportOptions &options) {
    const auto flag = [](bool value) { return value ? QStringLiteral("1") : QStringLiteral("0"); };
    const auto &render = options.render;
    return QStringLiteral("pixelsPerCell=%1 foreground=%2 bigDecor=%3 objects=%4 starts=%5 "
                          "paths=%6 grid=%7 labels=%8 transparent=%9")
        .arg(clampedPixelsPerCell(options))
        .arg(flag(render.showForeground), flag(render.showBigDecor), flag(render.showObjects),
             flag(render.showStarts), flag(render.showPaths), flag(render.showGrid),
             flag(render.showLabels), flag(render.transparentBackground));
}

QImage renderWorldForExport(const WorldView &world, const PngExportOptions &options) {
    const auto estimate = estimatePngExport(options);
    QImage image(estimate.width, estimate.height, QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        return image;
    }
    image.fill(Qt::transparent);
    ViewTransform transform;
    transform.scale = static_cast<double>(clampedPixelsPerCell(options)) / layout::cellPixels;
    QPainter painter(&image);
    // Each band clips the painter and limits the renderer to the cells it
    // touches, so the work per step is bounded whatever the output size.
    for (int top = 0; top < estimate.height; top += bandHeight) {
        const QRectF band(0, top, estimate.width, std::min(bandHeight, estimate.height - top));
        painter.setClipRect(band);
        renderWorld(painter, world, transform, band, options.render);
    }
    painter.end();
    return image;
}

PngExportResult exportWorldPng(const WorldView &world, GameProfile profile,
                               const PngExportOptions &options, const QString &path) {
    PngExportResult result;
    const auto image = renderWorldForExport(world, options);
    if (image.isNull()) {
        result.error = QCoreApplication::translate("PngExport",
                                                   "Not enough memory for the image.");
        return result;
    }
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        result.error = file.errorString();
        return result;
    }
    QImageWriter writer(&file, "png");
    writer.setText(QStringLiteral("Software"),
                   QStringLiteral("Eggbert Editor %1").arg(QStringLiteral(EGGBERT_EDITOR_VERSION)));
    writer.setText(QStringLiteral("EggbertProfile"), profileKey(profile));
    writer.setText(QStringLiteral("EggbertRenderer"),
                   QString::fromLatin1(schematicRendererVersion));
    writer.setText(QStringLiteral("EggbertSettings"), describeExportSettings(options));
    if (!writer.write(image)) {
        result.error = writer.errorString();
        file.cancelWriting();
        return result;
    }
    if (!file.commit()) {
        result.error = file.errorString();
        return result;
    }
    result.ok = true;
    result.width = image.width();
    result.height = image.height();
    return result;
}

} // namespace eggbert
