// SPDX-License-Identifier: GPL-3.0-only
#include "sprite_icons.hpp"

#include <QPainter>

#include <algorithm>

#include "app/game_assets.hpp"
#include "app/world_renderer.hpp"

namespace eggbert {

namespace {

std::optional<GameAssets::Sprite> tileSprite(SectionKind layer, int value,
                                             const GameAssets *artwork) {
    if (artwork == nullptr || value < 0) {
        return std::nullopt;
    }
    if (layer == SectionKind::bigDecor) {
        // The games draw value 203 from the tile sheet (an animated plant).
        return value == 203 ? artwork->sprite(1, value) : artwork->sprite(9, value);
    }
    return artwork->sprite(1, value);
}

QPixmap blank(int size) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    return pixmap;
}

// The sprite scaled so that its logical cell (at least 64 pixels, larger for
// big pictures) fits the thumbnail, on the canvas background colour.
QPixmap drawSprite(const GameAssets::Sprite &sprite, int size) {
    auto pixmap = blank(size);
    QPainter painter(&pixmap);
    painter.fillRect(pixmap.rect(), schematicBackgroundColor());
    const int extent = std::max({64, sprite.offset.x() + sprite.source.width(),
                                 sprite.offset.y() + sprite.source.height()});
    const double scale = static_cast<double>(size) / extent;
    painter.setRenderHint(QPainter::SmoothPixmapTransform, scale < 1.0);
    const QRectF target(sprite.offset.x() * scale, sprite.offset.y() * scale,
                        sprite.source.width() * scale, sprite.source.height() * scale);
    painter.drawImage(target, *sprite.sheet, sprite.source);
    return pixmap;
}

QPixmap drawSwatch(const QColor &color, const QString &text, int size) {
    auto pixmap = blank(size);
    QPainter painter(&pixmap);
    painter.fillRect(pixmap.rect(), color);
    auto font = painter.font();
    font.setPixelSize(std::max(8, size / 3));
    painter.setFont(font);
    painter.setPen(color.lightness() > 140 ? Qt::black : Qt::white);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, text);
    return pixmap;
}

} // namespace

bool tileHasArtwork(SectionKind layer, int value, const GameAssets *artwork) {
    return tileSprite(layer, value, artwork).has_value();
}

bool tileHasVisibleArtwork(SectionKind layer, int value, const GameAssets *artwork) {
    const auto sprite = tileSprite(layer, value, artwork);
    if (!sprite) {
        return false;
    }
    const auto area = sprite->source.intersected(sprite->sheet->rect());
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            if (qAlpha(sprite->sheet->pixel(x, y)) != 0) {
                return true;
            }
        }
    }
    return false;
}

QPixmap tileThumbnail(SectionKind layer, int value, const GameAssets *artwork, int size) {
    if (value == layout::emptyCell) {
        auto pixmap = blank(size);
        QPainter painter(&pixmap);
        painter.fillRect(pixmap.rect(), schematicBackgroundColor());
        painter.setPen(QPen(QColor(150, 150, 160), 1, Qt::DashLine));
        painter.drawRect(pixmap.rect().adjusted(1, 1, -2, -2));
        return pixmap;
    }
    if (const auto sprite = tileSprite(layer, value, artwork)) {
        return drawSprite(*sprite, size);
    }
    return drawSwatch(schematicTileColor(layer, static_cast<std::int16_t>(value)),
                      QString::number(value), size);
}

bool objectHasArtwork(int channel, int icon, const GameAssets *artwork) {
    return artwork != nullptr && artwork->sprite(channel, icon).has_value();
}

QPixmap objectThumbnail(int type, int channel, int icon, const GameAssets *artwork, int size) {
    if (artwork != nullptr) {
        if (const auto sprite = artwork->sprite(channel, icon)) {
            return drawSprite(*sprite, size);
        }
    }
    return drawSwatch(schematicObjectColor(static_cast<std::int16_t>(type)),
                      QStringLiteral("T%1").arg(type), size);
}

} // namespace eggbert
