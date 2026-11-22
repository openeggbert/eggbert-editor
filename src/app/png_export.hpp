// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <optional>
#include <QImage>
#include <QString>

#include "world/tile_tools.hpp"
#include "app/world_renderer.hpp"
#include "world/layout.hpp"

namespace eggbert {

// Identifies the drawing code in exported metadata; bump when output changes.
inline constexpr const char *schematicRendererVersion = "schematic-1";

struct PngExportOptions {
    // Output pixels per map cell: 64 is the native size (6400 x 6400).
    int pixelsPerCell = layout::cellPixels;
    RenderOptions render;
    // Only these cells (inclusive corners, clamped to the map); unset for
    // the whole world.
    std::optional<CellRect> crop;

    friend bool operator==(const PngExportOptions &, const PngExportOptions &) = default;
};

struct PngExportEstimate {
    int width = 0;
    int height = 0;
    // Memory for the finished RGBA image (4 bytes per pixel). Rendering goes
    // band by band straight into it, so no second full-size surface exists.
    qint64 imageBytes = 0;
};

[[nodiscard]] PngExportEstimate estimatePngExport(const PngExportOptions &options);

// The complete 100 x 100 world (or its crop), drawn in horizontal bands with the same
// renderer and draw order as the canvas.
[[nodiscard]] QImage renderWorldForExport(const WorldView &world,
                                          const PngExportOptions &options);

struct PngExportResult {
    bool ok = false;
    QString error;
    int width = 0;
    int height = 0;
};

// Writes the PNG atomically: on failure no file, or the previous file, is
// left at `path`. Text metadata records profile, renderer and settings.
PngExportResult exportWorldPng(const WorldView &world, GameProfile profile,
                               const PngExportOptions &options, const QString &path);

// One-line description of the settings, as stored in the PNG metadata.
[[nodiscard]] QString describeExportSettings(const PngExportOptions &options);

} // namespace eggbert
