// SPDX-License-Identifier: GPL-3.0-only
#include "app/png_export.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImageReader>
#include <QTemporaryDir>

using namespace eggbert;
using namespace eggbert::test;

namespace {

QByteArray readRaw(const QString &path) {
    QFile file(path);
    CHECK(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

} // namespace

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);

    auto bytes = blankWorld(fxRevision13);
    // Asymmetric scene with content at corners and on both sides of the
    // band borders (bands are 512 output pixels high).
    put16(bytes, fxCellOffset(fxForegroundOffset, 3, 70), 5);
    put16(bytes, fxCellOffset(fxForegroundOffset, 70, 3), 9);
    put16(bytes, fxCellOffset(fxForegroundOffset, 0, 0), 1);
    put16(bytes, fxCellOffset(fxForegroundOffset, 99, 99), 2);
    put16(bytes, fxCellOffset(fxForegroundOffset, 99, 0), 3);
    put16(bytes, fxCellOffset(fxForegroundOffset, 0, 99), 4);
    put16(bytes, fxCellOffset(fxForegroundOffset, 50, 7), 11);  // rows 448..511 at native size
    put16(bytes, fxCellOffset(fxForegroundOffset, 50, 8), 12);  // rows 512..575: next band
    put16(bytes, fxCellOffset(fxRevision13.bigDecorOffset, 40, 15), 20); // straddles y = 1024
    for (int index = 0; index < 4; ++index) {
        put32(bytes, 328 + 8 * index, 90 * 64);
        put32(bytes, 332 + 8 * index, 20 * 64);
        put32(bytes, 360 + 4 * index, 2);
    }
    const auto snapshot = bytes;
    const WorldView world(bytes);

    const auto fg = [](int id) { return schematicTileColor(SectionKind::foreground, id); };
    const auto background = schematicBackgroundColor();

    PngExportOptions options;
    options.render.showLabels = false;

    // Estimates: native and reduced sizes.
    {
        const auto native = estimatePngExport(options);
        CHECK_EQ(native.width, 6400);
        CHECK_EQ(native.height, 6400);
        CHECK_EQ(native.imageBytes, qint64{163'840'000}); // about 156 MiB
        auto small = options;
        small.pixelsPerCell = 8;
        CHECK_EQ(estimatePngExport(small).width, 800);
        CHECK_EQ(estimatePngExport(small).imageBytes, qint64{2'560'000});
        small.pixelsPerCell = 0; // clamped
        CHECK_EQ(estimatePngExport(small).width, 100);
        small.pixelsPerCell = 1000;
        CHECK_EQ(estimatePngExport(small).width, 6400);
    }

    // Native size: every cell is an exact 64-pixel square at cell * 64.
    {
        const auto image = renderWorldForExport(world, options);
        CHECK_EQ(image.width(), 6400);
        CHECK_EQ(image.height(), 6400);
        const auto at = [&](int x, int y) { return image.pixelColor(x, y); };
        // Corners of the image and of individual cells.
        CHECK(at(0, 0) == fg(1));
        CHECK(at(62, 62) == fg(1));
        CHECK(at(64, 0) == background);
        CHECK(at(0, 64) == background);
        // The last pixel column and row of a cell are its darker edge line.
        CHECK(at(6398, 6398) == fg(2));
        CHECK(at(6399, 6399) == QColor::fromRgb(fg(2).darker(140).rgb()));
        CHECK(at(6336, 6336) == fg(2));
        CHECK(at(6335, 6335) == background);
        CHECK(at(6398, 0) == fg(3));
        CHECK(at(0, 6398) == fg(4));
        // Not transposed.
        CHECK(at(3 * 64 + 5, 70 * 64 + 5) == fg(5));
        CHECK(at(70 * 64 + 5, 3 * 64 + 5) == fg(9));
        CHECK(at(70 * 64 + 5, 70 * 64 + 5) == background);
        // Seamless across a band border (y = 512).
        CHECK(at(50 * 64 + 10, 510) == fg(11));
        CHECK(at(50 * 64 + 10, 512) == fg(12));
        CHECK(at(50 * 64 + 10, 574) == fg(12));
        CHECK(at(50 * 64 + 10, 576) == background);
        // A bigDecor tile drawn in two bands (anchor row 15: y 962..1089).
        const auto big = schematicTileColor(SectionKind::bigDecor, 20);
        CHECK(at(40 * 64 + 40, 15 * 64 + 30) == big);
        CHECK(at(40 * 64 + 40, 1023) == big);
        CHECK(at(40 * 64 + 40, 1024) == big);
        CHECK(at(41 * 64 + 40, 16 * 64 + 60) == big);
        CHECK(at(40 * 64 + 40, 15 * 64 + 1) == background); // 2-pixel vertical offset
        // The banded export equals a single-pass render with the canvas renderer.
        const auto reference = renderWorldImage(world, 1.0, options.render);
        CHECK(image == reference);
    }

    // Reduced sizes align the same way.
    for (const int pixels : {32, 8, 1}) {
        auto small = options;
        small.pixelsPerCell = pixels;
        const auto image = renderWorldForExport(world, small);
        CHECK_EQ(image.width(), 100 * pixels);
        CHECK_EQ(image.height(), 100 * pixels);
        CHECK(image.pixelColor(3 * pixels, 70 * pixels) == fg(5));
        CHECK(image.pixelColor(70 * pixels, 3 * pixels) == fg(9));
        CHECK(image.pixelColor(99 * pixels, 99 * pixels) == fg(2));
        CHECK(image.pixelColor(50 * pixels, 8 * pixels) == fg(12));
        CHECK(image.pixelColor(60 * pixels, 60 * pixels) == background);
        CHECK(image == renderWorldImage(world, pixels / 64.0, small.render));
    }

    // Visibility options and transparent background.
    {
        auto partial = options;
        partial.pixelsPerCell = 8;
        partial.render.showForeground = false;
        partial.render.showStarts = false;
        const auto image = renderWorldForExport(world, partial);
        CHECK(image.pixelColor(3 * 8 + 2, 70 * 8 + 2) == background);
        CHECK(image.pixelColor(90 * 8 + 4, 20 * 8 + 4) == background);
        partial.render.transparentBackground = true;
        const auto clear = renderWorldForExport(world, partial);
        CHECK_EQ(clear.pixelColor(60 * 8, 60 * 8).alpha(), 0);
        CHECK(clear.pixelColor(40 * 8 + 6, 15 * 8 + 6) ==
              schematicTileColor(SectionKind::bigDecor, 20));
    }

    // Writing: dimensions, metadata, determinism and failure handling.
    QTemporaryDir dir;
    CHECK(dir.isValid());
    auto small = options;
    small.pixelsPerCell = 8;
    const auto path = dir.filePath("world.png");
    {
        const auto result = exportWorldPng(world, GameProfile::speedyBlupi2, small, path);
        CHECK(result.ok);
        CHECK(result.error.isEmpty());
        CHECK_EQ(result.width, 800);
        CHECK_EQ(result.height, 800);
        QImageReader reader(path);
        CHECK(reader.format() == "png");
        CHECK_EQ(reader.size(), QSize(800, 800));
        CHECK(reader.text("Software").startsWith("Eggbert Editor "));
        CHECK_EQ(reader.text("EggbertProfile"), QString("speedy-blupi-2"));
        CHECK_EQ(reader.text("EggbertRenderer"), QString("schematic-1"));
        CHECK_EQ(reader.text("EggbertSettings"), describeExportSettings(small));
        CHECK(reader.text("EggbertSettings").contains("pixelsPerCell=8"));
        CHECK(reader.text("EggbertSettings").contains("labels=0"));
        const auto loaded = reader.read();
        CHECK(loaded.pixelColor(3 * 8 + 2, 70 * 8 + 2) == fg(5));
        CHECK(loaded.pixelColor(70 * 8 + 2, 3 * 8 + 2) == fg(9));
        // Two exports of the same input are byte-identical.
        const auto second = dir.filePath("again.png");
        CHECK(exportWorldPng(world, GameProfile::speedyBlupi2, small, second).ok);
        CHECK(readRaw(path) == readRaw(second));
        // A different profile only changes metadata.
        const auto other = dir.filePath("other.png");
        CHECK(exportWorldPng(world, GameProfile::unknown, small, other).ok);
        CHECK_EQ(QImageReader(other).text("EggbertProfile"), QString("not-chosen"));
        CHECK(QImage(other) == QImage(path));
    }
    {
        // Unwritable destination: reported, nothing created, an existing
        // export is left untouched.
        const auto before = readRaw(path);
        const auto failed =
            exportWorldPng(world, GameProfile::unknown, small, path + "/child.png");
        CHECK(!failed.ok);
        CHECK(!failed.error.isEmpty());
        CHECK(readRaw(path) == before);
        CHECK(!exportWorldPng(world, GameProfile::unknown, small,
                              dir.filePath("missing-dir/x.png"))
                   .ok);
        CHECK_EQ(QDir(dir.path()).entryList(QDir::Files).size(), qsizetype{3});
    }

    // Partial and unknown inputs still export what exists.
    {
        auto cut = bytes;
        cut.resize(900);
        CHECK(exportWorldPng(WorldView(cut), GameProfile::unknown, small,
                             dir.filePath("cut.png"))
                  .ok);
        const auto old = blankWorld(fxRevision10);
        CHECK_EQ(renderWorldForExport(WorldView(old), small).width(), 800);
    }

    CHECK(bytes == snapshot); // exporting never touches the world bytes
    return test::finish("png_export");
}
