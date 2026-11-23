// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QByteArray>

#include "world/world_view.hpp"

namespace eggbert {

// A scalable picture of a world in the schematic style: one rectangle per
// occupied cell in the colour the schematic view uses for its number, back
// layer cells as outlines, objects as circles with their path, starts as
// diamonds. It never contains game artwork. The same world and options give
// the same bytes.
struct SvgExportOptions {
    // Size of a cell in SVG user units.
    int cellSize = 16;
    bool showForeground = true;
    bool showBigDecor = true;
    bool showObjects = true;
    bool showStarts = true;

    friend bool operator==(const SvgExportOptions &, const SvgExportOptions &) = default;
};

// Empty when the world has no complete foreground layer.
[[nodiscard]] QByteArray renderWorldSvg(const WorldView &world, const SvgExportOptions &options);

} // namespace eggbert
