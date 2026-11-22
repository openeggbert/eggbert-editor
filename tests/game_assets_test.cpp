// SPDX-License-Identifier: GPL-3.0-only
#include "app/game_assets.hpp"
#include "app/png_export.hpp"
#include "app/world_renderer.hpp"

#include "check.hpp"
#include "fixture.hpp"
#include "generated_game.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QGuiApplication>
#include <QImageReader>
#include <QPainter>
#include <QTemporaryDir>

using namespace eggbert;
using namespace eggbert::test;

namespace {

// Renders a window of the world at scale 1 whose top-left is a world pixel.
QImage window(const WorldView &world, const RenderOptions &options, int worldX, int worldY,
              int size = 96) {
    ViewTransform transform;
    transform.scale = 1.0;
    transform.originX = worldX;
    transform.originY = worldY;
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    renderWorld(painter, world, transform, QRectF(0, 0, size, size), options);
    return image;
}

} // namespace

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QTemporaryDir dir;
    CHECK(dir.isValid());

    // Profile recognition from files that exist in only one game.
    {
        const auto first = makeGame(dir.filePath("first"));
        const auto second = makeGame(dir.filePath("second"), true);
        const auto detectedFirst = detectGameProfile(first.root);
        CHECK(detectedFirst.profile == GameProfile::speedyBlupi1);
        CHECK(detectedFirst.evidence.join(' ').contains("demo200"));
        CHECK(detectGameProfile(second.root).profile == GameProfile::speedyBlupi2);
        QDir().mkpath(dir.filePath("empty/data"));
        const auto neither = detectGameProfile(dir.filePath("empty"));
        CHECK(neither.profile == GameProfile::unknown);
        CHECK(!neither.evidence.isEmpty());
        write(dir.filePath("empty/data/demo200.blp"), "x");
        write(dir.filePath("empty/data/demo300.blp"), "x");
        CHECK(detectGameProfile(dir.filePath("empty")).profile == GameProfile::unknown);
        CHECK(detectGameProfile(dir.filePath("no-such-directory")).profile ==
              GameProfile::unknown);
    }

    const auto game = makeGame(dir.filePath("game"));
    const auto digestBefore = directoryDigest(game.root);
    const auto assets = GameAssets::load(game.root, GameProfile::speedyBlupi1);
    CHECK(assets->usable());
    CHECK(assets->profile() == GameProfile::speedyBlupi1);
    {
        const auto report = assets->report().join('\n');
        CHECK(report.contains("IMAGE08"));
        CHECK(report.contains("246 sprites located in GAME.EXE"));
        CHECK(report.contains("187 sprites located in GAME.EXE"));
        CHECK(report.contains("54 sprites located in GAME.EXE"));
        CHECK(report.contains("291 sprites located in GAME.EXE"));
        CHECK(report.contains("object.blp: 1024 x 1024 pixels, usable."));
        CHECK(report.contains("Tiles can be drawn"));
    }
    CHECK_EQ(assets->spriteCount(SpriteSheet::tiles), std::size_t{246});
    CHECK_EQ(assets->spriteCount(SpriteSheet::player4), std::size_t{291});

    // Sprite lookup by stored channel and icon.
    {
        const auto trimmed = assets->sprite(1, 5).value();
        CHECK(trimmed.source == QRect(5 * 64 + 4, 6, 40, 20));
        CHECK(trimmed.offset == QPoint(12, 34));
        CHECK(assets->sprite(1, 245).has_value());
        CHECK(!assets->sprite(1, 246).has_value());
        CHECK(!assets->sprite(1, -1).has_value());
        CHECK(!assets->sprite(7, 0).has_value());  // not a sprite channel
        CHECK(!assets->sprite(0, 0).has_value());
        CHECK(assets->sprite(10, 186).has_value());
        CHECK(!assets->sprite(10, 187).has_value());
        CHECK(assets->sprite(9, 53).has_value());
        CHECK(assets->sprite(13, 4).has_value());
        CHECK(assets->sprite(2, 0)->sheet != assets->sprite(11, 0)->sheet);
        // Pure blue became transparent; other colours are exact.
        const auto &sheet = *assets->sprite(1, 6)->sheet;
        CHECK_EQ(sheet.pixelColor(6 * 64 + 25, 25).alpha(), 0);
        CHECK(sheet.pixelColor(6 * 64 + 5, 5) == spriteColor(0, 6));
        CHECK_EQ(sheet.pixelColor(5 * 64 + 1, 1).alpha(), 0); // trimmed sprite's margin
        // The 8-bit palette sheet decodes to the same colours.
        const auto big = assets->sprite(9, 20).value();
        CHECK(big.sheet->pixelColor(big.source.center()) == spriteColor(2, 20));
    }

    // An asymmetric scene drawn with the generated artwork.
    auto bytes = blankWorld(fxRevision13);
    const auto fgAt = [&](std::size_t x, std::size_t y, int value) {
        put16(bytes, fxCellOffset(fxForegroundOffset, x, y), value);
    };
    fgAt(3, 70, 9);    // full-cell tile
    fgAt(70, 3, 5);    // trimmed tile: 40 x 20 at offset (12, 34)
    fgAt(10, 10, 107); // grass: drawn 7 pixels up
    fgAt(12, 10, 6);   // tile with a transparent hole ...
    put16(bytes, fxCellOffset(fxRevision13.bigDecorOffset, 12, 10), 20); // ... over bigDecor
    put16(bytes, fxCellOffset(fxRevision13.bigDecorOffset, 40, 10), 21);
    fgAt(60, 60, 300); // no such picture in the first game: placeholder
    fgAt(50, 50, 9);   // a lift overlaps this tile
    const auto object = [&](std::size_t slot, int type, int x, int y, int channel, int icon) {
        const auto base = fxSlotOffset(fxRevision13, slot);
        put16(bytes, base, type);
        for (const std::size_t field : {12, 20, 28}) {
            put32(bytes, base + field, x);
            put32(bytes, base + field + 4, y);
        }
        put16(bytes, base + 42, channel);
        put16(bytes, base + 44, icon);
    };
    object(0, 5, 20 * 64 + 2, 80 * 64 + 4, 10, 3);  // an item from the element sheet
    object(1, 4, 30 * 64, 80 * 64, 10, 57);         // type 4: drawn at +2, +6
    object(2, 1, 50 * 64, 50 * 64, 1, 153);         // a lift on top of a tile
    object(3, 5, 25 * 64, 80 * 64, 10, 500);        // icon without picture: placeholder
    object(4, 5, 3 * 64, 70 * 64, 10, 8);           // an item behind the tile at (3, 70)
    for (int index = 0; index < 4; ++index) {
        put32(bytes, 328 + 8 * index, (80 + 2 * index) * 64 + 2);
        put32(bytes, 332 + 8 * index, 20 * 64 + 6);
        put32(bytes, 360 + 4 * index, index == 2 ? 1 : 2);
    }
    const auto snapshot = bytes;
    const WorldView world(bytes);

    RenderOptions options;
    options.showLabels = false;
    options.artwork = assets;
    const auto background = schematicBackgroundColor();
    const auto tile = [](int index) { return spriteColor(0, index); };

    // Full-cell tile at exactly cell * 64; not transposed.
    {
        const auto image = window(world, options, 3 * 64 - 16, 70 * 64 - 16);
        CHECK(image.pixelColor(15, 15) == background);
        CHECK(image.pixelColor(16, 16) == tile(9));
        CHECK(image.pixelColor(79, 79) == tile(9));
        CHECK(image.pixelColor(80, 80) == background);
        // The item on the same cell is behind the tile: objects are drawn
        // before the foreground.
        CHECK(image.pixelColor(40, 40) == tile(9));
        const auto transposed = window(world, options, 70 * 64 - 16, 3 * 64 - 16);
        CHECK(transposed.pixelColor(16, 16) == background); // trimmed sprite starts lower right
    }
    // Trimmed tile: its 40 x 20 picture at the cell origin plus (12, 34).
    {
        const auto image = window(world, options, 70 * 64, 3 * 64, 64);
        CHECK(image.pixelColor(11, 34) == background);
        CHECK(image.pixelColor(12, 33) == background);
        CHECK(image.pixelColor(12, 34) == tile(5));
        CHECK(image.pixelColor(51, 53) == tile(5));
        CHECK(image.pixelColor(52, 53) == background);
        CHECK(image.pixelColor(51, 54) == background);
    }
    // Grass is drawn seven pixels above its cell.
    {
        const auto image = window(world, options, 10 * 64, 10 * 64 - 16, 96);
        CHECK(image.pixelColor(5, 8) == background);
        CHECK(image.pixelColor(5, 9) == tile(107));
        CHECK(image.pixelColor(5, 72) == tile(107));
        CHECK(image.pixelColor(5, 73) == background);
    }
    // bigDecor is two pixels below its cell and shows through a tile's
    // transparent pixels; foreground is drawn over it.
    {
        const auto image = window(world, options, 12 * 64, 10 * 64, 140);
        CHECK(image.pixelColor(5, 5) == tile(6));
        CHECK(image.pixelColor(25, 25) == spriteColor(2, 20)); // through the hole
        CHECK(image.pixelColor(100, 1) == background);
        CHECK(image.pixelColor(100, 2) == spriteColor(2, 20));
        CHECK(image.pixelColor(127, 129) == spriteColor(2, 20));
        CHECK(image.pixelColor(128, 129) == background);
        CHECK(image.pixelColor(127, 130) == background);
    }
    // Objects: stored channel and icon; type 4 is shifted by (2, 6).
    {
        const auto item = window(world, options, 20 * 64, 80 * 64, 64);
        CHECK(item.pixelColor(1, 4) == background);
        CHECK(item.pixelColor(2, 4) == spriteColor(1, 3));
        CHECK(item.pixelColor(61, 63) == spriteColor(1, 3));
        CHECK(item.pixelColor(62, 63) == background);
        const auto shifted = window(world, options, 30 * 64, 80 * 64, 80);
        CHECK(shifted.pixelColor(1, 6) == background);
        CHECK(shifted.pixelColor(2, 5) == background);
        CHECK(shifted.pixelColor(2, 6) == spriteColor(1, 57));
        CHECK(shifted.pixelColor(61, 65) == spriteColor(1, 57));
        CHECK(shifted.pixelColor(62, 65) == background);
    }
    // A lift is drawn after the foreground.
    CHECK(window(world, options, 50 * 64, 50 * 64, 64).pixelColor(30, 30) == tile(153));
    // Starts: player sheets by slot, picture 0 facing right and 4 facing left.
    {
        const auto first = window(world, options, 80 * 64 + 2, 20 * 64 + 6, 60);
        CHECK(first.pixelColor(30, 30) == spriteColor(3, 0));
        const auto third = window(world, options, 84 * 64 + 2, 20 * 64 + 6, 60);
        CHECK(third.pixelColor(30, 30) == spriteColor(3, 4));
    }
    // Pictures the artwork lacks fall back to the schematic placeholder.
    {
        const auto missingTile = window(world, options, 60 * 64, 60 * 64, 64);
        CHECK(missingTile.pixelColor(30, 30) == schematicTileColor(SectionKind::foreground, 300));
        const auto missingIcon = window(world, options, 25 * 64, 80 * 64, 64);
        CHECK(missingIcon.pixelColor(30, 30) != background);
        CHECK(missingIcon.pixelColor(30, 30) != spriteColor(1, 3));
    }
    // Reduced scale averages pixels (dither patterns must not become a
    // checkerboard); inside a plain sprite the colour stays exact.
    {
        const auto small = renderWorldImage(world, 0.25, options); // 16 pixels per cell
        CHECK(small.pixelColor(3 * 16 + 8, 70 * 16 + 8) == tile(9));
        CHECK(small.pixelColor(70 * 16 + 8, 3 * 16 + 11) == tile(5));
        CHECK(small.pixelColor(70 * 16 + 1, 3 * 16 + 1) == background);
    }
    // Without artwork the same options draw schematically.
    {
        auto schematic = options;
        schematic.artwork.reset();
        const auto image = window(world, schematic, 3 * 64, 70 * 64, 64);
        CHECK(image.pixelColor(30, 30) == schematicTileColor(SectionKind::foreground, 9));
    }
    // The exported picture uses the artwork and says so in its metadata.
    {
        PngExportOptions exportOptions;
        exportOptions.pixelsPerCell = 16;
        exportOptions.render = options;
        const auto path = dir.filePath("artwork.png");
        CHECK(exportWorldPng(world, GameProfile::speedyBlupi1, exportOptions, path).ok);
        QImageReader reader(path);
        CHECK_EQ(reader.text("EggbertRenderer"), QString("artwork-1"));
        CHECK(reader.read().pixelColor(3 * 16 + 8, 70 * 16 + 8) == tile(9));
        exportOptions.render.artwork.reset();
        CHECK(exportWorldPng(world, GameProfile::speedyBlupi1, exportOptions, path).ok);
        CHECK_EQ(QImageReader(path).text("EggbertRenderer"), QString("schematic-1"));
    }
    CHECK(bytes == snapshot);
    // Loading and drawing never wrote into the game directory.
    CHECK(directoryDigest(game.root) == digestBefore);

    // Fallbacks when the directory is incomplete or does not match.
    {
        // The wrong game: sprite counts differ, so no table is accepted.
        const auto wrong = GameAssets::load(game.root, GameProfile::speedyBlupi2);
        CHECK(!wrong->usable());
        CHECK(wrong->report().join('\n').contains("not found in any executable"));
        CHECK(!wrong->sprite(1, 5).has_value());
        // No game chosen.
        CHECK(!GameAssets::load(game.root, GameProfile::unknown)->usable());
        // Not a game directory at all.
        const auto none = GameAssets::load(dir.filePath("no-such-directory"),
                                           GameProfile::speedyBlupi1);
        CHECK(!none->usable());
        CHECK(!none->report().isEmpty());

        // No executable: sheets exist but positions are unknown.
        const auto noExe = makeGame(dir.filePath("no-exe"));
        CHECK(QFile::remove(noExe.root + "/GAME.EXE"));
        const auto withoutTables = GameAssets::load(noExe.root, GameProfile::speedyBlupi1);
        CHECK(!withoutTables->usable());
        CHECK(withoutTables->report().join('\n').contains("No executable"));

        // A damaged tile sheet: tiles unusable, everything stays schematic.
        const auto damaged = makeGame(dir.filePath("damaged"));
        write(damaged.root + "/IMAGE08/OBJECT.BLP", QByteArray(5000, '\x5A'));
        const auto broken = GameAssets::load(damaged.root, GameProfile::speedyBlupi1);
        CHECK(!broken->usable());
        CHECK(broken->report().join('\n').contains("object.blp: not a readable bitmap."));
        auto brokenOptions = options;
        brokenOptions.artwork = broken;
        CHECK(window(world, brokenOptions, 3 * 64, 70 * 64, 64).pixelColor(30, 30) ==
              schematicTileColor(SectionKind::foreground, 9));

        // A bitmap header claiming an enormous image is refused, not allocated.
        QByteArray huge = QByteArray::fromHex(
            "424d36000000000000003600000028000000a0860100a08601000100180000000000"
            "0000000000000000000000000000000000000000");
        write(damaged.root + "/IMAGE08/ELEMENT.BLP", huge);
        CHECK(GameAssets::load(damaged.root, GameProfile::speedyBlupi1)
                  ->report()
                  .join('\n')
                  .contains("element.blp: not a readable bitmap."));

        // A missing element sheet: tiles are drawn, items fall back.
        const auto partial = makeGame(dir.filePath("partial"));
        CHECK(QFile::remove(partial.root + "/IMAGE08/ELEMENT.BLP"));
        const auto partly = GameAssets::load(partial.root, GameProfile::speedyBlupi1);
        CHECK(partly->usable());
        CHECK(partly->report().join('\n').contains("element.blp: not found."));
        CHECK(!partly->sprite(10, 3).has_value());
        auto partialOptions = options;
        partialOptions.artwork = partly;
        // (The item on this cell now has its schematic outline, so sample
        // the tile's interior rather than its corner.)
        CHECK(window(world, partialOptions, 3 * 64 - 16, 70 * 64 - 16).pixelColor(40, 40) ==
              tile(9));
        CHECK(window(world, partialOptions, 20 * 64, 80 * 64, 64).pixelColor(30, 30) !=
              spriteColor(1, 3));

        // A tile sheet smaller than the sprite positions need is not used.
        const auto shrunk = makeGame(dir.filePath("shrunk"));
        saveBitmap(shrunk.tiles.image.copy(0, 0, 1024, 512), shrunk.root + "/IMAGE08/OBJECT.BLP");
        CHECK(!GameAssets::load(shrunk.root, GameProfile::speedyBlupi1)->usable());
    }

    // A slice of the player table that reads like the 54-sprite table: on
    // its own that is ambiguous, but bytes inside an accepted table cannot
    // be another table, so the real one is found.
    {
        const auto alike = makeGame(dir.filePath("look-alike"), false, 54);
        const auto loaded = GameAssets::load(alike.root, GameProfile::speedyBlupi1);
        const auto report = loaded->report().join('\n');
        CHECK(report.contains("Sprite positions for player: 291 sprites located in GAME.EXE"));
        CHECK(report.contains("Sprite positions for bigDecor and effects: 54 sprites located in "
                              "GAME.EXE"));
        CHECK(report.contains("look-alikes inside the other sprite tables were set aside"));
        CHECK(!report.contains("more than one candidate"));
        const auto big = loaded->sprite(9, 20);
        CHECK(big.has_value());
        if (big) {
            CHECK(big->source == QRect(alike.bigDecor.sprites[20].srcX,
                                       alike.bigDecor.sprites[20].srcY, 128, 128));
        }
        // The unambiguous game reports no such note.
        CHECK(!assets->report().join('\n').contains("set aside"));
    }

    // Backdrop pictures, music files and the numbered ends of a path.
    {
        const auto scenic = makeGame(dir.filePath("scenic"));
        addBackdrops(scenic, 2);
        addMusic(scenic);
        const auto scenicDigest = directoryDigest(scenic.root);
        const auto loaded = GameAssets::load(scenic.root, GameProfile::speedyBlupi1);
        CHECK(loaded->usable());
        CHECK(loaded->backdrop(0) != nullptr);
        CHECK(loaded->backdrop(0)->size() == QSize(640, 480));
        CHECK(loaded->backdrop(0) == loaded->backdrop(0)); // kept once read
        CHECK(loaded->backdrop(2) == nullptr);
        CHECK(loaded->backdrop(-1) == nullptr);
        CHECK(loaded->readBackdrop(1).pixelColor(600, 10) == backdropColor(1, true));
        CHECK(loaded->readBackdrop(7).isNull());
        CHECK(assets->backdrop(0) == nullptr); // the plain generated game has none
        // Music n is file n - 1; only a path is returned.
        CHECK(loaded->musicFile(1).endsWith("MUSIC000.BLP"));
        CHECK(loaded->musicFile(2).endsWith("MUSIC001.BLP"));
        CHECK(loaded->musicFile(3).isEmpty());
        CHECK(loaded->musicFile(0).isEmpty());
        CHECK(assets->musicFile(1).isEmpty());

        auto scene = blankWorld(fxRevision13);
        put16(scene, 224, 1); // region 1
        // A mover from cell (20, 20) to cell (24, 20).
        const auto base = fxSlotOffset(fxRevision13, 0);
        put16(scene, base, 4);
        put32(scene, base + 12, 20 * 64);
        put32(scene, base + 16, 20 * 64);
        put32(scene, base + 20, 24 * 64);
        put32(scene, base + 24, 20 * 64);
        put32(scene, base + 28, 20 * 64);
        put32(scene, base + 32, 20 * 64);
        put16(scene, base + 42, 10);
        put16(scene, base + 44, 57);
        const WorldView sceneView(scene);
        RenderOptions scenicOptions;
        scenicOptions.showLabels = false;
        scenicOptions.artwork = loaded;
        // The backdrop of the stored region fills the empty world, tiled
        // every 640 x 480 pixels from the world's corner.
        CHECK(window(sceneView, scenicOptions, 128, 128).pixelColor(10, 10) == backdropColor(1, false));
        CHECK(window(sceneView, scenicOptions, 300, 0).pixelColor(19, 5) == backdropColor(1, false));
        CHECK(window(sceneView, scenicOptions, 300, 0).pixelColor(20, 5) == backdropColor(1, true));
        CHECK(window(sceneView, scenicOptions, 640, 480).pixelColor(0, 0) ==
              backdropColor(1, false));
        CHECK(window(sceneView, scenicOptions, 6400 - 96, 6400 - 96).pixelColor(95, 95) ==
              backdropColor(1, true));
        // With parallax it is shifted by a third of the view position.
        auto parallax = scenicOptions;
        parallax.backdropParallax = true;
        CHECK(window(sceneView, parallax, 128, 128).pixelColor(10, 10) == backdropColor(1, false));
        // View at x = 900: shift 300, so the picture's middle is at world 620.
        CHECK(window(sceneView, parallax, 900, 0).pixelColor(15, 5) == backdropColor(1, true));
        CHECK(window(sceneView, parallax, 900, 0).pixelColor(40, 5) == backdropColor(1, false));
        // Switched off, transparent or without a picture: the plain colour.
        auto plain = scenicOptions;
        plain.showBackdrop = false;
        CHECK(window(sceneView, plain, 128, 128).pixelColor(10, 10) == background);
        auto clear = scenicOptions;
        clear.transparentBackground = true;
        CHECK_EQ(window(sceneView, clear, 128, 128).pixelColor(10, 10).alpha(), 0);
        put16(scene, 224, 9);
        CHECK(window(WorldView(scene), scenicOptions, 128, 128).pixelColor(10, 10) == background);
        put16(scene, 224, 1);
        CHECK(window(sceneView, RenderOptions{}, 128, 128).pixelColor(10, 10) == background);

        // The ends of the path carry badges: 1 at the start, 2 at the end.
        // (Sampled left of the digit, inside the badge.)
        const auto badgeAt = [&](const RenderOptions &with, int cellX) {
            return window(sceneView, with, cellX * 64, 20 * 64).pixelColor(6, 13);
        };
        CHECK(badgeAt(scenicOptions, 20) == pathMarkerColor());
        CHECK(badgeAt(scenicOptions, 24) == pathMarkerColor());
        CHECK(badgeAt(RenderOptions{}, 24) == pathMarkerColor()); // schematic drawing too
        auto noPaths = scenicOptions;
        noPaths.showPaths = false;
        CHECK(badgeAt(noPaths, 24) != pathMarkerColor());
        CHECK(badgeAt(scenicOptions, 22) != pathMarkerColor()); // nothing along the way
        CHECK(directoryDigest(scenic.root) == scenicDigest);
    }

    // Animation frames, the games' second tile pass and object poses.
    {
        auto scene = blankWorld(fxRevision13);
        put16(scene, fxCellOffset(fxForegroundOffset, 30, 30), 68);  // lava
        put16(scene, fxCellOffset(fxForegroundOffset, 34, 30), 68);  // lava under a lift
        put16(scene, fxCellOffset(fxForegroundOffset, 36, 30), 9);   // plain tile under a lift
        put16(scene, fxCellOffset(fxRevision13.bigDecorOffset, 40, 30), 203); // marine plant
        const auto put = [&](std::size_t slot, int type, int x, int y, int channel, int icon) {
            const auto base = fxSlotOffset(fxRevision13, slot);
            put16(scene, base, type);
            for (const std::size_t field : {12, 20, 28}) {
                put32(scene, base + field, x);
                put32(scene, base + field + 4, y);
            }
            put16(scene, base + 42, channel);
            put16(scene, base + 44, icon);
        };
        put(0, 1, 34 * 64, 30 * 64, 1, 153); // lifts
        put(1, 1, 36 * 64, 30 * 64, 1, 153);
        put(2, 5, 44 * 64, 30 * 64, 10, 3);  // an item
        put(3, 12, 48 * 64, 30 * 64, 1, 2);  // a crate disguised as tile 2
        put(4, 12, 50 * 64, 30 * 64, 1, 32); // an ordinary crate
        const WorldView sceneView(scene);
        const auto at = [&](const RenderOptions &with, int cellX, int px = 30, int py = 30) {
            return window(sceneView, with, cellX * 64, 30 * 64, 64).pixelColor(px, py);
        };
        // Stored state: lava is drawn after lifts, a plain tile before them.
        CHECK(at(options, 30) == tile(68));
        CHECK(at(options, 34) == tile(68));
        CHECK(at(options, 36) == tile(153));
        CHECK(at(options, 40) == tile(203));
        // The games' editors mark a disguised crate with picture 214.
        CHECK(at(options, 48) == tile(214));
        CHECK(at(options, 50) == tile(32));
        // With an animation frame the tick chooses the picture.
        auto moving = options;
        auto frame = std::make_shared<AnimationFrame>();
        frame->tick = 3;
        moving.animation = frame;
        CHECK(at(moving, 30) == tile(69));
        CHECK(at(moving, 34) == tile(71)); // another cell, another phase: (34 * 13 + 30 * 7 + 1) % 5
        CHECK(at(moving, 36) == tile(153));
        CHECK(at(moving, 40) == tile(204));
        CHECK(at(moving, 44, 10, 10) == spriteColor(1, 3)); // no poses given: stored ones
        // Poses replace the stored position and picture of each slot.
        auto posed = std::make_shared<AnimationFrame>();
        posed->tick = 0;
        posed->objects.assign(200, ObjectPose{});
        posed->objects[2] = ObjectPose{{46 * 64, 30 * 64}, 10, 7};
        moving.animation = posed;
        CHECK(at(moving, 46, 10, 10) == spriteColor(1, 7));
        CHECK(at(moving, 44, 10, 10) != spriteColor(1, 3));
        CHECK(at(moving, 30) == tile(68)); // tick 0 again
        // Without artwork tiles keep their stored colour at any tick, while
        // objects still follow their poses.
        RenderOptions schematic;
        schematic.showLabels = false;
        schematic.profile = GameProfile::speedyBlupi1;
        auto late = std::make_shared<AnimationFrame>();
        late->tick = 3;
        schematic.animation = late;
        CHECK(at(schematic, 30) == schematicTileColor(SectionKind::foreground, 68));
    }

    // The second game's sizes and its extra vertical adjustments.
    {
        const auto second = makeGame(dir.filePath("second-game"), true);
        const auto secondAssets = GameAssets::load(second.root, GameProfile::speedyBlupi2);
        CHECK(secondAssets->usable());
        CHECK_EQ(secondAssets->spriteCount(SpriteSheet::tiles), std::size_t{441});
        CHECK_EQ(secondAssets->spriteCount(SpriteSheet::bigDecor), std::size_t{100});
        auto secondBytes = blankWorld(fxRevision13);
        put16(secondBytes, fxCellOffset(fxForegroundOffset, 5, 5), 364);
        put16(secondBytes, fxCellOffset(fxRevision13.bigDecorOffset, 20, 20), 67);
        put16(secondBytes, fxCellOffset(fxRevision13.bigDecorOffset, 30, 20), 88);
        const WorldView secondWorld(secondBytes);
        RenderOptions secondOptions;
        secondOptions.showLabels = false;
        secondOptions.artwork = secondAssets;
        const auto raised = window(secondWorld, secondOptions, 5 * 64, 5 * 64 - 8, 80);
        CHECK(raised.pixelColor(5, 5) == background);
        CHECK(raised.pixelColor(5, 6) == spriteColor(0, 364)); // two pixels up
        const auto big67 = window(secondWorld, secondOptions, 20 * 64, 20 * 64 - 16, 64);
        CHECK(big67.pixelColor(5, 4) == background);
        CHECK(big67.pixelColor(5, 5) == spriteColor(2, 67)); // 2 down, 13 up
        const auto big88 = window(secondWorld, secondOptions, 30 * 64, 20 * 64 - 16, 64);
        CHECK(big88.pixelColor(5, 15) == background);
        CHECK(big88.pixelColor(5, 16) == spriteColor(2, 88)); // 2 down, 2 up
    }
    return test::finish("game_assets");
}
