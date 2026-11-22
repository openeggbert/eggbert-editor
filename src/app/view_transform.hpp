// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

#include "world/layout.hpp"

namespace eggbert {

// Cell containing a world-pixel position, or nullopt outside the map.
[[nodiscard]] inline std::optional<CellPos> cellAtWorld(double worldX, double worldY) {
    const auto cellX = std::floor(worldX / layout::cellPixels);
    const auto cellY = std::floor(worldY / layout::cellPixels);
    if (cellX < 0 || cellY < 0 || cellX >= static_cast<double>(layout::mapWidth) ||
        cellY >= static_cast<double>(layout::mapHeight)) {
        return std::nullopt;
    }
    return CellPos{static_cast<std::size_t>(cellX), static_cast<std::size_t>(cellY)};
}

// The single mapping between world pixels and screen pixels:
// screen = (world - origin) * scale. File offsets never appear here; cells
// convert to world pixels by multiplying with layout::cellPixels.
struct ViewTransform {
    static constexpr double minimumScale = 1.0 / 64.0;
    static constexpr double maximumScale = 8.0;

    double scale = 0.125;
    double originX = 0.0;
    double originY = 0.0;

    [[nodiscard]] double toScreenX(double worldX) const { return (worldX - originX) * scale; }
    [[nodiscard]] double toScreenY(double worldY) const { return (worldY - originY) * scale; }
    [[nodiscard]] double toWorldX(double screenX) const { return screenX / scale + originX; }
    [[nodiscard]] double toWorldY(double screenY) const { return screenY / scale + originY; }

    [[nodiscard]] std::optional<CellPos> cellAtScreen(double screenX, double screenY) const {
        return cellAtWorld(toWorldX(screenX), toWorldY(screenY));
    }

    // Changes the scale while the world point under the screen anchor stays
    // under it.
    void zoomAbout(double screenX, double screenY, double newScale) {
        const auto worldX = toWorldX(screenX);
        const auto worldY = toWorldY(screenY);
        scale = std::clamp(newScale, minimumScale, maximumScale);
        originX = worldX - screenX / scale;
        originY = worldY - screenY / scale;
    }

    // Moves the content by a screen-pixel delta (dragging right moves the
    // world right).
    void panByScreen(double deltaX, double deltaY) {
        originX -= deltaX / scale;
        originY -= deltaY / scale;
    }

    // Shows the whole world rectangle centred in the viewport with a margin.
    void fit(double worldX, double worldY, double worldWidth, double worldHeight,
             double viewWidth, double viewHeight, double marginPixels = 0.0) {
        const auto usableWidth = std::max(1.0, viewWidth - 2 * marginPixels);
        const auto usableHeight = std::max(1.0, viewHeight - 2 * marginPixels);
        const auto wanted = std::min(usableWidth / std::max(1.0, worldWidth),
                                     usableHeight / std::max(1.0, worldHeight));
        scale = std::clamp(wanted, minimumScale, maximumScale);
        originX = worldX + worldWidth / 2 - viewWidth / (2 * scale);
        originY = worldY + worldHeight / 2 - viewHeight / (2 * scale);
    }

    // Keeps the world from being scrolled entirely out of view: on each axis
    // content smaller than the viewport is centred, larger content cannot
    // leave a gap at either edge.
    void clampToWorld(double viewWidth, double viewHeight) {
        const auto clampAxis = [this](double &origin, double worldSize, double viewSize) {
            const auto visible = viewSize / scale;
            if (visible >= worldSize) {
                origin = (worldSize - visible) / 2;
            } else {
                origin = std::clamp(origin, 0.0, worldSize - visible);
            }
        };
        clampAxis(originX, layout::worldPixelWidth, viewWidth);
        clampAxis(originY, layout::worldPixelHeight, viewHeight);
    }
};

} // namespace eggbert
