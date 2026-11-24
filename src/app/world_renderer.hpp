// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QColor>
#include <QImage>
#include <QRectF>

#include <memory>
#include <vector>

#include "app/game_assets.hpp"
#include "app/view_transform.hpp"
#include "world/animation.hpp"
#include "world/world_view.hpp"

class QPainter;

namespace eggbert {

// One moment of the display-only animation: the tick for animated tiles and,
// per object slot, the pose to draw instead of the stored one (an empty
// list means the stored poses).
struct AnimationFrame {
    std::uint32_t tick = 0;
    std::vector<ObjectPose> objects;
};

struct RenderOptions {
    bool showForeground = true;
    bool showBigDecor = true;
    bool showObjects = true;
    bool showStarts = true;
    bool showPaths = true;
    bool showGrid = false;
    bool showLabels = true;
    bool transparentBackground = false;
    // With artwork: draw the world's backdrop picture (header.region)
    // behind everything instead of the plain background colour.
    bool showBackdrop = true;
    // The games tile the backdrop and scroll it at two thirds of the map's
    // speed. True shifts it that way for the visible part (the canvas);
    // false anchors it at the world's top-left corner (exports).
    bool backdropParallax = false;
    // Game artwork to draw with; null draws everything schematically.
    // Sprites the artwork cannot provide fall back to their placeholder.
    std::shared_ptr<const GameAssets> artwork;
    // The game whose drawing rules apply (draw order, animation). Taken
    // from the artwork when that is set.
    GameProfile profile = GameProfile::unknown;
    // Null draws the stored state; otherwise animated tiles show the frame
    // of the tick (with artwork) and objects are drawn at their poses.
    std::shared_ptr<const AnimationFrame> animation;

    friend bool operator==(const RenderOptions &, const RenderOptions &) = default;
};

// Schematic colours are fixed (not themed) so renders are reproducible.
[[nodiscard]] QColor schematicBackgroundColor();
[[nodiscard]] QColor schematicOutsideColor();
// Fill of the numbered badges at the two ends of a motion path.
[[nodiscard]] QColor pathMarkerColor();
// The pictures the games' own editors draw at the two ends of a moving
// object's path: entries 30 ("1", the start) and 31 ("2", the end) of the
// tile sheet, which stored channel 1 selects.
inline constexpr int pathMarkerChannel = 1;
inline constexpr int pathStartIcon = 30;
inline constexpr int pathEndIcon = 31;
// Opaque colour of a stored cell value; the same value always gets the same
// colour. Values below -1 get a warning colour.
[[nodiscard]] QColor schematicTileColor(SectionKind layer, std::int16_t id);
[[nodiscard]] QColor schematicObjectColor(std::int16_t type);
[[nodiscard]] QColor schematicStartColor(std::size_t index);

// Size in world pixels of the schematic box for a stored sprite channel:
// tiles 64, the big-decor/explosion sheet 128, everything else 60.
[[nodiscard]] int schematicSpriteSize(std::int16_t channel);

// A selectable item in the draw stack. Header entries refer to player starts.
struct WorldHit {
    SectionKind kind{};
    CellPos anchor{};
    int value = 0; // tile ID, object slot, or zero-based start index
    friend bool operator==(const WorldHit &, const WorldHit &) = default;
};

struct WorldPick {
    QPoint pixel;
    std::vector<WorldHit> hits; // back to front
};

// Draws the part of the world visible in `viewport` (screen coordinates of
// the painter). Draw order follows the engines: background, bigDecor,
// starts, objects, foreground, lifts (with artwork), then editor overlays
// (paths, outlines, labels, grid). Without artwork, or for any sprite the
// artwork lacks, a schematic placeholder is drawn. The ends of a motion path
// are marked 1 (start) and 2 (end), as in the games' editors.
void renderWorld(QPainter &painter, const WorldView &world, const ViewTransform &transform,
                 const QRectF &viewport, const RenderOptions &options, WorldPick *pick = nullptr);

// The complete 100 x 100 world at `scale` screen pixels per world pixel.
[[nodiscard]] QImage renderWorldImage(const WorldView &world, double scale,
                                      const RenderOptions &options);

} // namespace eggbert
