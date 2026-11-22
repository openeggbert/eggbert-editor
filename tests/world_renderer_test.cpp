// SPDX-License-Identifier: GPL-3.0-only
#include "app/world_renderer.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QGuiApplication>
#include <QPainter>

using namespace eggbert;
using namespace eggbert::test;

namespace {

// Screen pixel at the centre of a cell when the world is rendered at
// `cellPixels` screen pixels per cell.
QColor at(const QImage &image, int cellPixels, int x, int y) {
    return image.pixelColor(x * cellPixels + cellPixels / 2, y * cellPixels + cellPixels / 2);
}

} // namespace

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);

    auto bytes = blankWorld(fxRevision13);
    // Asymmetric scene. Foreground ids 5 at (3,70) and 9 at (70,3); bigDecor
    // id 20 anchored at (40,10) (covers cells 40..41 x 10..11); foreground id
    // 7 at (41,11) on top of that bigDecor tile.
    put16(bytes, fxCellOffset(fxForegroundOffset, 3, 70), 5);
    put16(bytes, fxCellOffset(fxForegroundOffset, 70, 3), 9);
    put16(bytes, fxCellOffset(fxRevision13.bigDecorOffset, 40, 10), 20);
    put16(bytes, fxCellOffset(fxForegroundOffset, 41, 11), 7);
    put16(bytes, fxCellOffset(fxForegroundOffset, 0, 0), 1);
    put16(bytes, fxCellOffset(fxForegroundOffset, 99, 99), 2);
    put16(bytes, fxCellOffset(fxForegroundOffset, 50, 50), -7); // invalid value
    // Starts far away from the sampled cells.
    for (int index = 0; index < 4; ++index) {
        put32(bytes, 328 + 8 * index, 90 * 64);
        put32(bytes, 332 + 8 * index, 20 * 64);
        put32(bytes, 360 + 4 * index, 2);
    }
    // One object in slot 5 at cell (20,80), tile-sized (channel 1), with a
    // path to cell (24,80).
    const auto slot = fxSlotOffset(fxRevision13, 5);
    put16(bytes, slot + 0, 12);
    put32(bytes, slot + 12, 20 * 64);
    put32(bytes, slot + 16, 80 * 64);
    put32(bytes, slot + 20, 24 * 64);
    put32(bytes, slot + 24, 80 * 64);
    put32(bytes, slot + 28, 20 * 64);
    put32(bytes, slot + 32, 80 * 64);
    put16(bytes, slot + 42, 1);
    const auto snapshot = bytes;

    const WorldView world(bytes);
    RenderOptions options;
    options.showLabels = false;
    const int px = 8; // screen pixels per cell
    const auto image = renderWorldImage(world, px / 64.0, options);
    CHECK_EQ(image.width(), 800);
    CHECK_EQ(image.height(), 800);

    const auto background = schematicBackgroundColor();
    const auto fg = [](int id) { return schematicTileColor(SectionKind::foreground, id); };
    const auto big = [](int id) { return schematicTileColor(SectionKind::bigDecor, id); };

    // x/y are not transposed.
    CHECK(at(image, px, 3, 70) == fg(5));
    CHECK(at(image, px, 70, 3) == fg(9));
    CHECK(fg(5) != fg(9));
    // Corners.
    CHECK(at(image, px, 0, 0) == fg(1));
    CHECK(at(image, px, 99, 99) == fg(2));
    CHECK(at(image, px, 99, 0) == background);
    CHECK(at(image, px, 0, 99) == background);
    // bigDecor covers 2 x 2 cells from its anchor; foreground is drawn over it.
    CHECK(at(image, px, 40, 10) == big(20));
    CHECK(at(image, px, 41, 10) == big(20));
    CHECK(at(image, px, 40, 11) == big(20));
    CHECK(at(image, px, 41, 11) == fg(7));
    CHECK(at(image, px, 42, 10) == background);
    CHECK(at(image, px, 39, 10) == background);
    CHECK(at(image, px, 10, 40) == background); // not transposed either
    // A value below -1 is flagged, -1 is empty.
    CHECK(at(image, px, 50, 50) == QColor(255, 0, 200));
    CHECK(at(image, px, 51, 50) == background);
    // The start marker and the object are visible.
    CHECK(at(image, px, 90, 20) != background);
    CHECK(at(image, px, 20, 80) != background);
    // The path end handle is drawn at the end cell.
    CHECK(image.pixelColor(24 * px + 1, 80 * px + 1) != background);

    // Visibility flags.
    {
        auto hidden = options;
        hidden.showForeground = false;
        const auto render = renderWorldImage(world, px / 64.0, hidden);
        CHECK(at(render, px, 3, 70) == background);
        CHECK(at(render, px, 41, 11) == big(20)); // bigDecor shows through
    }
    {
        auto hidden = options;
        hidden.showBigDecor = false;
        const auto render = renderWorldImage(world, px / 64.0, hidden);
        CHECK(at(render, px, 40, 10) == background);
        CHECK(at(render, px, 41, 11) == fg(7));
    }
    {
        auto hidden = options;
        hidden.showStarts = false;
        hidden.showObjects = false;
        hidden.showPaths = false;
        const auto render = renderWorldImage(world, px / 64.0, hidden);
        CHECK(at(render, px, 90, 20) == background);
        CHECK(at(render, px, 20, 80) == background);
        CHECK(render.pixelColor(24 * px + 1, 80 * px + 1) == background);
    }
    {
        auto transparent = options;
        transparent.transparentBackground = true;
        const auto render = renderWorldImage(world, px / 64.0, transparent);
        CHECK_EQ(at(render, px, 99, 0).alpha(), 0);
        CHECK(at(render, px, 3, 70) == fg(5));
    }
    // Grid lines appear on cell boundaries only when enabled.
    {
        auto grid = options;
        grid.showGrid = true;
        const auto render = renderWorldImage(world, px / 64.0, grid);
        CHECK(render.pixelColor(60 * px, 60 * px + 3) != background);
        CHECK(at(render, px, 60, 60) == background);
        CHECK(image.pixelColor(60 * px, 60 * px + 3) == background);
    }

    // The same scene at 64 pixels per cell maps cells to exact 64-pixel
    // squares (native size), checked through a small viewport.
    {
        ViewTransform transform;
        transform.scale = 1.0;
        transform.originX = 3 * 64 - 10;
        transform.originY = 70 * 64 - 10;
        QImage window(100, 100, QImage::Format_ARGB32_Premultiplied);
        window.fill(Qt::transparent);
        QPainter painter(&window);
        renderWorld(painter, world, transform, QRectF(0, 0, 100, 100), options);
        painter.end();
        CHECK(window.pixelColor(9, 9) == background);
        CHECK(window.pixelColor(10, 10) == fg(5));
        CHECK(window.pixelColor(72, 72) == fg(5));
        CHECK(window.pixelColor(74, 74) == background);
        CHECK(window.pixelColor(74, 40) == background);
    }

    // Revision 1.0 has no bigDecor layer; truncated input renders what exists.
    {
        auto old = blankWorld(fxRevision10);
        put16(old, fxCellOffset(fxForegroundOffset, 3, 70), 5);
        const auto render = renderWorldImage(WorldView(old), px / 64.0, options);
        CHECK(at(render, px, 3, 70) == fg(5));
        auto cut = bytes;
        cut.resize(900);
        const auto partial = renderWorldImage(WorldView(cut), px / 64.0, options);
        CHECK(at(partial, px, 3, 70) == background);
        const auto nothing = renderWorldImage(WorldView(Bytes{}), px / 64.0, options);
        CHECK(at(nothing, px, 3, 70) == background);
    }

    // Colours are stable and distinguish neighbouring ids.
    CHECK(fg(5) == schematicTileColor(SectionKind::foreground, 5));
    CHECK(fg(5) != fg(6));
    CHECK(fg(5) != big(5));
    CHECK_EQ(schematicSpriteSize(1), 64);
    CHECK_EQ(schematicSpriteSize(9), 128);
    CHECK_EQ(schematicSpriteSize(10), 60);

    CHECK(bytes == snapshot);
    return test::finish("world_renderer");
}
