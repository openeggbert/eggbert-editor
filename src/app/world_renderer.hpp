// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QColor>
#include <QImage>
#include <QRectF>

#include "app/view_transform.hpp"
#include "world/world_view.hpp"

class QPainter;

namespace eggbert {

struct RenderOptions {
    bool showForeground = true;
    bool showBigDecor = true;
    bool showObjects = true;
    bool showStarts = true;
    bool showPaths = true;
    bool showGrid = false;
    bool showLabels = true;
    bool transparentBackground = false;

    friend bool operator==(const RenderOptions &, const RenderOptions &) = default;
};

// Schematic colours are fixed (not themed) so renders are reproducible.
[[nodiscard]] QColor schematicBackgroundColor();
[[nodiscard]] QColor schematicOutsideColor();
// Opaque colour of a stored cell value; the same value always gets the same
// colour. Values below -1 get a warning colour.
[[nodiscard]] QColor schematicTileColor(SectionKind layer, std::int16_t id);
[[nodiscard]] QColor schematicObjectColor(std::int16_t type);
[[nodiscard]] QColor schematicStartColor(std::size_t index);

// Size in world pixels of the schematic box for a stored sprite channel:
// tiles 64, the big-decor/explosion sheet 128, everything else 60.
[[nodiscard]] int schematicSpriteSize(std::int16_t channel);

// Draws the part of the world visible in `viewport` (screen coordinates of
// the painter) without any game assets. Draw order follows the engines:
// background, bigDecor, starts, objects, foreground, then editor overlays
// (paths, outlines, labels, grid).
void renderWorld(QPainter &painter, const WorldView &world, const ViewTransform &transform,
                 const QRectF &viewport, const RenderOptions &options);

// The complete 100 x 100 world at `scale` screen pixels per world pixel.
[[nodiscard]] QImage renderWorldImage(const WorldView &world, double scale,
                                      const RenderOptions &options);

} // namespace eggbert
