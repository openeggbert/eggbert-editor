// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QImage>

#include "world/stamp.hpp"

namespace eggbert {

inline constexpr int stampThumbnailSide = 96;

// A schematic picture of a stamp for lists: `side` x `side` pixels, the
// stamp centred and scaled to fit with whole pixels per cell where it can.
// Foreground tiles in their schematic colours, bigDecor tiles where no
// foreground tile covers them, a dot per object start; holes transparent.
// Never drawn with game artwork.
[[nodiscard]] QImage stampThumbnail(const Stamp &stamp, int side);

} // namespace eggbert
