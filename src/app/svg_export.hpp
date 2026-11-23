// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QByteArray>
#include <memory>

#include "world/world_view.hpp"

namespace eggbert {

class GameAssets;
struct SvgExportOptions {
    // Size of a cell in SVG user units.
    int cellSize = 16;
    bool showForeground = true;
    bool showBigDecor = true;
    bool showObjects = true;
    bool showStarts = true;
    // Without artwork, export the labelled vector schematic.
    std::shared_ptr<const GameAssets> artwork;

    friend bool operator==(const SvgExportOptions &, const SvgExportOptions &) = default;
};

// Empty when the world has no complete foreground layer.
[[nodiscard]] QByteArray renderWorldSvg(const WorldView &world, const SvgExportOptions &options);

} // namespace eggbert
