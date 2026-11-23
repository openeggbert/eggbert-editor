// SPDX-License-Identifier: GPL-3.0-only
#include "world_renderer.hpp"

#include "world/animation.hpp"

#include <QFont>
#include <QPainter>
#include <QPen>

#include <algorithm>
#include <cmath>
#include <vector>

namespace eggbert {

namespace {

constexpr int cell = layout::cellPixels;
constexpr int bigDecorSize = 2 * cell;
// The engines draw a bigDecor tile two pixels below its anchor cell.
constexpr int bigDecorYOffset = 2;

struct CellRange {
    int x0 = 0;
    int y0 = 0;
    int x1 = -1; // inclusive
    int y1 = -1;
};

// Cells intersecting the viewport, widened by `before` cells to the left and
// top for tiles that extend beyond their anchor cell.
CellRange visibleCells(const ViewTransform &transform, const QRectF &viewport, int before) {
    const auto clampCell = [](double value, int limit) {
        return static_cast<int>(std::clamp(value, 0.0, static_cast<double>(limit - 1)));
    };
    CellRange range;
    range.x0 = clampCell(std::floor(transform.toWorldX(viewport.left()) / cell) - before,
                         static_cast<int>(layout::mapWidth));
    range.y0 = clampCell(std::floor(transform.toWorldY(viewport.top()) / cell) - before,
                         static_cast<int>(layout::mapHeight));
    range.x1 = clampCell(std::floor(transform.toWorldX(viewport.right()) / cell),
                         static_cast<int>(layout::mapWidth));
    range.y1 = clampCell(std::floor(transform.toWorldY(viewport.bottom()) / cell),
                         static_cast<int>(layout::mapHeight));
    if (transform.toWorldX(viewport.right()) < 0 || transform.toWorldY(viewport.bottom()) < 0 ||
        transform.toWorldX(viewport.left()) >= layout::worldPixelWidth + before * cell ||
        transform.toWorldY(viewport.top()) >= layout::worldPixelHeight + before * cell) {
        range.x1 = range.x0 - 1; // nothing visible
    }
    return range;
}

// Screen rectangle of a world-pixel rectangle, snapped so that adjacent
// rectangles share an edge without gaps or overlap.
QRect screenRect(const ViewTransform &transform, double worldX, double worldY, double width,
                 double height) {
    const auto left = static_cast<int>(std::floor(transform.toScreenX(worldX)));
    const auto top = static_cast<int>(std::floor(transform.toScreenY(worldY)));
    const auto right = static_cast<int>(std::floor(transform.toScreenX(worldX + width)));
    const auto bottom = static_cast<int>(std::floor(transform.toScreenY(worldY + height)));
    return QRect(left, top, std::max(1, right - left), std::max(1, bottom - top));
}

void drawLabel(QPainter &painter, const QRect &rect, const QString &text, const QColor &color) {
    QFont font = painter.font();
    font.setPixelSize(std::clamp(rect.height() / 3, 8, 14));
    painter.setFont(font);
    painter.setPen(color);
    painter.drawText(rect, Qt::AlignCenter | Qt::TextDontClip, text);
}

// Exactly the colour an 8-bit-per-channel image stores, so a rendered pixel
// compares equal to the palette entry it was drawn with.
QColor eightBit(const QColor &color) {
    return QColor::fromRgb(color.rgb());
}

QColor readableOn(const QColor &background) {
    return background.lightness() > 140 ? QColor(20, 20, 20) : QColor(245, 245, 245);
}

} // namespace

QColor schematicBackgroundColor() {
    return QColor(30, 33, 40);
}

QColor pathMarkerColor() {
    return QColor(255, 221, 51);
}

QColor schematicOutsideColor() {
    return QColor(18, 19, 23);
}

QColor schematicTileColor(SectionKind layer, std::int16_t id) {
    if (id < layout::emptyCell) {
        return QColor(255, 0, 200); // not a tile number any engine reads as valid
    }
    const int hue = (static_cast<int>(id) * 47 + 11) % 360;
    if (layer == SectionKind::bigDecor) {
        return eightBit(QColor::fromHsv(hue, 70, 120 + (id * 13) % 40));
    }
    return eightBit(QColor::fromHsv(hue, 150 + (id * 29) % 80, 170 + (id * 17) % 70));
}

QColor schematicObjectColor(std::int16_t type) {
    const int hue = ((static_cast<int>(type) % 360 + 360) * 67 + 200) % 360;
    return eightBit(QColor::fromHsv(hue, 220, 255));
}

QColor schematicStartColor(std::size_t index) {
    static const QColor colors[] = {QColor(255, 214, 10), QColor(255, 122, 60),
                                    QColor(70, 190, 255), QColor(120, 230, 120)};
    return colors[index % 4];
}

int schematicSpriteSize(std::int16_t channel) {
    if (channel == 1) {
        return cell;
    }
    if (channel == 9) {
        return bigDecorSize;
    }
    return 60;
}

void renderWorld(QPainter &painter, const WorldView &world, const ViewTransform &transform,
                 const QRectF &viewport, const RenderOptions &options) {
    painter.save();
    painter.setClipRect(viewport, Qt::IntersectClip);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const auto worldRect =
        screenRect(transform, 0, 0, layout::worldPixelWidth, layout::worldPixelHeight);
    const GameAssets *art =
        options.artwork && options.artwork->usable() ? options.artwork.get() : nullptr;
    if (!options.transparentBackground) {
        painter.fillRect(viewport, schematicOutsideColor());
        painter.fillRect(worldRect, schematicBackgroundColor());
        // The backdrop picture of the stored region, tiled like the games
        // do. With parallax it is shifted by a third of the view position,
        // which is what scrolling it at two thirds of the map's speed means
        // in world coordinates.
        const auto header = world.header();
        const QImage *backdrop = art != nullptr && options.showBackdrop && header
                                     ? art->backdrop(header->region())
                                     : nullptr;
        if (backdrop != nullptr && !backdrop->isNull()) {
            const int tileWidth = backdrop->width();
            const int tileHeight = backdrop->height();
            double shiftX = 0.0;
            double shiftY = 0.0;
            if (options.backdropParallax) {
                shiftX = std::max(0.0, transform.toWorldX(viewport.left())) / 3.0;
                shiftY = std::max(0.0, transform.toWorldY(viewport.top())) / 3.0;
            }
            const double left = std::max(0.0, transform.toWorldX(viewport.left()));
            const double top = std::max(0.0, transform.toWorldY(viewport.top()));
            const double right = std::min<double>(layout::worldPixelWidth,
                                                  transform.toWorldX(viewport.right()) + 1);
            const double bottom = std::min<double>(layout::worldPixelHeight,
                                                   transform.toWorldY(viewport.bottom()) + 1);
            painter.save();
            painter.setClipRect(QRectF(worldRect).intersected(viewport), Qt::IntersectClip);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, transform.scale < 1.0);
            const auto firstColumn = static_cast<long long>(std::floor((left - shiftX) / tileWidth));
            const auto firstRow = static_cast<long long>(std::floor((top - shiftY) / tileHeight));
            for (auto row = firstRow; shiftY + row * tileHeight < bottom; ++row) {
                for (auto column = firstColumn; shiftX + column * tileWidth < right; ++column) {
                    const double x = shiftX + column * tileWidth;
                    const double y = shiftY + row * tileHeight;
                    const QRectF target(
                        QPointF(std::floor(transform.toScreenX(x)), std::floor(transform.toScreenY(y))),
                        QPointF(std::floor(transform.toScreenX(x + tileWidth)),
                                std::floor(transform.toScreenY(y + tileHeight))));
                    painter.drawImage(target, *backdrop);
                }
            }
            painter.restore();
        }
    }
    const double cellScreen = cell * transform.scale;
    const bool labels = options.showLabels && cellScreen >= 26.0;

    const bool secondGame =
        art != nullptr && tablesOf(art->profile()) == GameProfile::speedyBlupi2;
    const auto drawProfile = art != nullptr ? art->profile() : options.profile;
    const AnimationFrame *animation = options.animation.get();
    // The frame of a tile now: animated with artwork, the stored number
    // (with the games' draw order) otherwise.
    const auto frameOf = [&](SectionKind kind, std::int16_t stored, int x, int y) {
        if (animation != nullptr && art != nullptr) {
            return tileFrame(drawProfile, kind, stored, static_cast<std::size_t>(x),
                             static_cast<std::size_t>(y), animation->tick);
        }
        return stillTileFrame(drawProfile, kind, stored);
    };
    // Draws one sprite at a world position with nearest-neighbour scaling;
    // false when the artwork has no such sprite.
    const auto drawSprite = [&](int channel, int icon, double worldX, double worldY) {
        if (art == nullptr) {
            return false;
        }
        const auto sprite = art->sprite(channel, icon);
        if (!sprite) {
            return false;
        }
        const auto target =
            screenRect(transform, worldX + sprite->offset.x(), worldY + sprite->offset.y(),
                       sprite->source.width(), sprite->source.height());
        // Exact pixels at full size and above. Below it, averaging: the
        // games' water and glass are fine dither patterns, which sampling
        // single pixels would turn into a coarse checkerboard.
        painter.setRenderHint(QPainter::SmoothPixmapTransform, transform.scale < 1.0);
        painter.drawImage(target, *sprite->sheet, sprite->source);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
        return true;
    };

    // bigDecor: each stored value covers 2 x 2 cells from its anchor cell.
    if (const auto layer = world.bigDecor(); layer && options.showBigDecor) {
        const auto range = visibleCells(transform, viewport, 1);
        for (int x = range.x0; x <= range.x1; ++x) {
            for (int y = range.y0; y <= range.y1; ++y) {
                const auto id = *layer->cell(x, y);
                if (id == layout::emptyCell) {
                    continue;
                }
                {
                    // The plant 203 comes from the tile sheet; two groups of
                    // the second game's pictures sit a little higher.
                    double spriteY = y * cell + bigDecorYOffset;
                    if (secondGame && id >= 66 && id <= 68) {
                        spriteY -= 13;
                    } else if (secondGame && id >= 87 && id <= 89) {
                        spriteY -= 2;
                    }
                    const auto frame = frameOf(SectionKind::bigDecor, id, x, y);
                    if (drawSprite(id == 203 ? 1 : 9, frame.icon, x * cell, spriteY)) {
                        continue;
                    }
                }
                const auto rect = screenRect(transform, x * cell, y * cell + bigDecorYOffset,
                                             bigDecorSize, bigDecorSize);
                const auto color = schematicTileColor(SectionKind::bigDecor, id);
                painter.fillRect(rect, color);
                if (cellScreen >= 8.0) {
                    painter.setPen(QPen(color.lighter(150), 0));
                    painter.drawRect(rect.adjusted(0, 0, -1, -1));
                }
                if (labels) {
                    drawLabel(painter, rect, QStringLiteral("B%1").arg(id), readableOn(color));
                }
            }
        }
    }

    const auto header = world.header();

    // Player starts, last slot first so slot 1 ends on top.
    if (header && options.showStarts) {
        for (int index = 3; index >= 0; --index) {
            const auto position = *header->blupiPos(index);
            // Player sheets 2, 11, 12, 13; picture 4 faces left, 0 right.
            if (drawSprite(index == 0 ? 2 : 10 + index, *header->blupiDir(index) == 2 ? 0 : 4,
                           position.x, position.y)) {
                continue;
            }
            const auto rect = screenRect(transform, position.x, position.y, 60, 60);
            const auto color = schematicStartColor(index);
            painter.setRenderHint(QPainter::Antialiasing, cellScreen >= 16.0);
            painter.setPen(QPen(QColor(20, 20, 20), 0));
            painter.setBrush(color);
            painter.drawEllipse(rect.adjusted(0, 0, -1, -1));
            painter.setBrush(Qt::NoBrush);
            painter.setRenderHint(QPainter::Antialiasing, false);
            if (labels) {
                const auto direction = *header->blupiDir(index);
                const auto arrow = direction == 1   ? QStringLiteral("<%1")
                                   : direction == 2 ? QStringLiteral("%1>")
                                                    : QStringLiteral("%1?");
                drawLabel(painter, rect, arrow.arg(index + 1), QColor(20, 20, 20));
            }
        }
    }

    const auto objects = world.objects();
    const auto objectRect = [&](const ObjectRecord &record, Point position) {
        const auto size = schematicSpriteSize(record.channel);
        return screenRect(transform, position.x, position.y, size, size);
    };

    // With artwork, lifts are drawn after the foreground as in the games.
    const auto isLift = [&](std::int16_t type) {
        return art != nullptr && (type == 1 || (secondGame && (type == 47 || type == 48)));
    };
    std::vector<bool> drawnWithArtwork(objects ? objects->slotCount() : 0, false);
    // The pose an object is shown with: the animation's, or the stored one.
    const auto poseOf = [&](std::size_t slot, const ObjectRecord &record) {
        if (animation != nullptr && slot < animation->objects.size()) {
            return animation->objects[slot];
        }
        return ObjectPose{record.posCurrent, record.channel, record.icon};
    };
    const auto drawObjectBody = [&](std::size_t slot, const ObjectRecord &record) {
        const auto pose = poseOf(slot, record);
        const auto rect = objectRect(record, pose.position);
        if (!QRectF(rect).adjusted(-64, -64, 64, 64).intersects(viewport)) {
            return;
        }
        // Three types are drawn slightly lower and to the right; one more
        // in the second game.
        double worldX = pose.position.x;
        double worldY = pose.position.y;
        if (record.type == 4 || record.type == 32 || record.type == 33) {
            worldX += 2;
            worldY += 6;
        } else if (secondGame && record.type == 54) {
            worldY += 6;
        }
        if (drawSprite(pose.channel, pose.icon, worldX, worldY)) {
            drawnWithArtwork[slot] = true;
            // The games' editors mark a crate that is disguised as a tile.
            if (record.type == 12 && (pose.icon < 32 || pose.icon > 34)) {
                drawSprite(1, 214, worldX, worldY);
            }
            return;
        }
        auto color = schematicObjectColor(record.type);
        color.setAlpha(150);
        painter.fillRect(rect, color);
    };

    // Foreground cells. With artwork the games' order applies: ordinary
    // tiles, then lifts, then the tiles of the second pass (lava, water,
    // wind, fans, traps), which so cover what is inside them. The schematic
    // drawing shows every stored number in one pass.
    enum class Pass { all, early, ordinary, late };
    const auto drawForeground = [&](Pass pass) {
        const auto layer = world.foreground();
        if (!layer || !options.showForeground) {
            return;
        }
        const auto range = visibleCells(transform, viewport, 0);
        for (int x = range.x0; x <= range.x1; ++x) {
            for (int y = range.y0; y <= range.y1; ++y) {
                const auto id = *layer->cell(x, y);
                if (id == layout::emptyCell) {
                    continue;
                }
                const auto frame = frameOf(SectionKind::foreground, id, x, y);
                if ((pass == Pass::early && !frame.early) ||
                    (pass == Pass::ordinary && (frame.late || frame.early)) ||
                    (pass == Pass::late && !frame.late)) {
                    continue;
                }
                if (art != nullptr && frame.animated && frame.icon == layout::emptyCell) {
                    continue; // a blank frame of the animation
                }
                // Grass pictures (107 to 109, 157) are drawn seven pixels up,
                // and picture 364 of the second game two pixels up.
                const int lift = (id >= 107 && id <= 109) || id == 157 ? 7
                                 : secondGame && id == 364             ? 2
                                                                       : 0;
                if (drawSprite(1, frame.icon, x * cell, y * cell - lift + frame.offsetY)) {
                    continue;
                }
                const auto rect = screenRect(transform, x * cell, y * cell, cell, cell);
                const auto color = schematicTileColor(SectionKind::foreground, id);
                painter.fillRect(rect, color);
                if (cellScreen >= 10.0) {
                    painter.setPen(QPen(color.darker(140), 0));
                    painter.drawLine(rect.bottomLeft(), rect.bottomRight());
                    painter.drawLine(rect.topRight(), rect.bottomRight());
                }
                if (labels) {
                    drawLabel(painter, rect, QString::number(id), readableOn(color));
                }
            }
        }
    };
    // The second game draws two tiles (384, 385) before everything else of
    // the foreground and the objects.
    if (art != nullptr) {
        drawForeground(Pass::early);
    }
    // Object bodies, highest slot first as the engines iterate.
    if (objects && options.showObjects) {
        for (std::size_t remaining = objects->slotCount(); remaining > 0; --remaining) {
            const auto record = *objects->record(remaining - 1);
            if (!record.isFree() && !isLift(record.type)) {
                drawObjectBody(remaining - 1, record);
            }
        }
    }

    drawForeground(art != nullptr ? Pass::ordinary : Pass::all);

    if (objects && options.showObjects && art != nullptr) {
        for (std::size_t slot = 0; slot < objects->slotCount(); ++slot) {
            const auto record = *objects->record(slot);
            if (!record.isFree() && isLift(record.type)) {
                drawObjectBody(slot, record);
            }
        }
    }
    if (art != nullptr) {
        drawForeground(Pass::late);
    }

    // Editor overlays: motion paths, object outlines and slot labels.
    if (objects && (options.showObjects || options.showPaths)) {
        for (std::size_t slot = 0; slot < objects->slotCount(); ++slot) {
            const auto record = *objects->record(slot);
            if (record.isFree()) {
                continue;
            }
            const auto size = schematicSpriteSize(record.channel);
            const auto color = schematicObjectColor(record.type);
            const auto centre = [&](Point position) {
                return QPointF(transform.toScreenX(position.x + size / 2.0),
                               transform.toScreenY(position.y + size / 2.0));
            };
            if (options.showPaths && record.posStart != record.posEnd) {
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.setPen(QPen(color, 2, Qt::DashLine));
                painter.drawLine(centre(record.posStart), centre(record.posEnd));
                painter.setPen(QPen(color, 2));
                const auto end = objectRect(record, record.posEnd);
                // The two ends, marked as the games' editors do: 1 where the
                // object starts, 2 where it turns back. With game artwork
                // these are the games' own two pictures (tile sheet entries
                // 30 and 31, drawn at the stored positions); without it, or
                // where a picture is missing, a numbered badge.
                painter.setRenderHint(QPainter::Antialiasing, false);
                const bool startPictured =
                    drawSprite(pathMarkerChannel, pathStartIcon, record.posStart.x,
                               record.posStart.y);
                const bool endPictured =
                    drawSprite(pathMarkerChannel, pathEndIcon, record.posEnd.x, record.posEnd.y);
                painter.setRenderHint(QPainter::Antialiasing, true);
                painter.setPen(QPen(color, 2));
                if (!endPictured) {
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRect(end.adjusted(1, 1, -2, -2));
                }
                if (cellScreen >= 5.0 && !(startPictured && endPictured)) {
                    const double radius = std::clamp(cellScreen * 0.3, 5.0, 13.0);
                    auto font = painter.font();
                    font.setPixelSize(static_cast<int>(radius * 1.5));
                    font.setBold(true);
                    painter.setFont(font);
                    const auto badge = [&](const QRect &box, const QString &number) {
                        const QPointF centre(box.left() + radius, box.top() + radius);
                        painter.setPen(QPen(Qt::black, 1));
                        painter.setBrush(pathMarkerColor());
                        painter.drawEllipse(centre, radius, radius);
                        painter.setBrush(Qt::NoBrush);
                        painter.setPen(Qt::black);
                        painter.drawText(QRectF(centre.x() - radius, centre.y() - radius,
                                                2 * radius, 2 * radius),
                                         Qt::AlignCenter, number);
                    };
                    if (!startPictured) {
                        badge(objectRect(record, record.posStart), QStringLiteral("1"));
                    }
                    if (!endPictured) {
                        badge(end, QStringLiteral("2"));
                    }
                }
                painter.setRenderHint(QPainter::Antialiasing, false);
            }
            if (options.showObjects && !drawnWithArtwork[slot]) {
                const auto rect = objectRect(record, poseOf(slot, record).position);
                if (!QRectF(rect).intersects(viewport)) {
                    continue;
                }
                painter.setPen(QPen(color, cellScreen >= 16.0 ? 2 : 0));
                painter.drawRect(rect.adjusted(0, 0, -1, -1));
                if (labels) {
                    drawLabel(painter, rect,
                              QStringLiteral("T%1\n#%2").arg(record.type).arg(slot),
                              QColor(255, 255, 255));
                }
            }
        }
    }

    if (options.showGrid && cellScreen >= 6.0) {
        const auto range = visibleCells(transform, viewport, 0);
        for (int x = range.x0; x <= range.x1 + 1; ++x) {
            painter.setPen(QPen(QColor(255, 255, 255, x % 10 == 0 ? 110 : 45), 0));
            const auto screenX = static_cast<int>(std::floor(transform.toScreenX(x * cell)));
            painter.drawLine(screenX, worldRect.top(), screenX, worldRect.bottom());
        }
        for (int y = range.y0; y <= range.y1 + 1; ++y) {
            painter.setPen(QPen(QColor(255, 255, 255, y % 10 == 0 ? 110 : 45), 0));
            const auto screenY = static_cast<int>(std::floor(transform.toScreenY(y * cell)));
            painter.drawLine(worldRect.left(), screenY, worldRect.right(), screenY);
        }
    }
    painter.restore();
}

QImage renderWorldImage(const WorldView &world, double scale, const RenderOptions &options) {
    ViewTransform transform;
    transform.scale = std::clamp(scale, ViewTransform::minimumScale, ViewTransform::maximumScale);
    const auto width = static_cast<int>(std::lround(layout::worldPixelWidth * transform.scale));
    const auto height = static_cast<int>(std::lround(layout::worldPixelHeight * transform.scale));
    QImage image(std::max(1, width), std::max(1, height), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    renderWorld(painter, world, transform, QRectF(0, 0, image.width(), image.height()), options);
    return image;
}

} // namespace eggbert
