// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QPixmap>

#include "world/layout.hpp"

namespace eggbert {

class GameAssets;

// Small pictures of what a stored number stands for, for choosers and lists.
// With game artwork the sprite itself is shown; without it (or when the
// artwork has no such sprite) a schematic swatch in the canvas colours with
// the stored number, so every chooser works without game data.

// True when the artwork can draw this stored tile value of a layer.
[[nodiscard]] bool tileHasArtwork(SectionKind layer, int value, const GameAssets *artwork);
// True when that picture has at least one visible pixel (sheets contain
// unused, fully transparent entries).
[[nodiscard]] bool tileHasVisibleArtwork(SectionKind layer, int value, const GameAssets *artwork);
// Square thumbnail of a tile value; -1 (empty cell) is shown as an empty box.
[[nodiscard]] QPixmap tileThumbnail(SectionKind layer, int value, const GameAssets *artwork,
                                    int size);

// One atlas entry without object-type animation remapping.
[[nodiscard]] QPixmap spriteThumbnail(int channel, int icon, const GameAssets *artwork, int size);

[[nodiscard]] bool objectHasArtwork(int channel, int icon, const GameAssets *artwork);
// Square thumbnail of an object's still picture; the placeholder
// shows the stored type.
[[nodiscard]] QPixmap objectThumbnail(int type, int channel, int icon, const GameAssets *artwork,
                                      int size);

} // namespace eggbert
