// SPDX-License-Identifier: GPL-3.0-only
//
// The headless commands: output, exit codes, read-only behaviour, and that
// export-png writes exactly what the library's exporter writes.

#include "app/cli.hpp"
#include "app/png_export.hpp"
#include "app/svg_export.hpp"
#include "world/game_convert.hpp"
#include "world/merge.hpp"
#include "world/new_world.hpp"
#include "world/revision_convert.hpp"
#include "world/world_view.hpp"

#include "check.hpp"
#include "fixture.hpp"
#include "generated_port.hpp"

#include <algorithm>
#include <filesystem>
#include <tuple>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QTemporaryDir>
#include <QXmlStreamReader>

using namespace eggbert;
using namespace eggbert::test;

namespace {

struct Run {
    int code = 0;
    QString out;
    QString err;
};

Run run(const QStringList &arguments) {
    Run result;
    QTextStream out(&result.out);
    QTextStream err(&result.err);
    result.code = runCommandLine(arguments, out, err);
    return result;
}

void writeAll(const QString &path, const Bytes &bytes) {
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<qint64>(bytes.size()));
}

QByteArray readAll(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    QTemporaryDir dir;
    auto world = blankWorld(fxRevision13);
    put16(world, fxCellOffset(fxForegroundOffset, 3, 70), 9);
    put16(world, fxCellOffset(fxForegroundOffset, 40, 40), 30);
    put16(world, 222, 3); // music
    const auto path = dir.filePath("a.blp");
    writeAll(path, world);
    auto changed = world;
    put16(changed, fxCellOffset(fxForegroundOffset, 3, 70), 10);
    put16(changed, 222, 4);
    const auto otherPath = dir.filePath("b.blp");
    writeAll(otherPath, changed);
    auto broken = world;
    put16(broken, 222, 99); // music outside every game's range
    broken.resize(broken.size() - 100); // and truncated
    const auto brokenPath = dir.filePath("broken.blp");
    writeAll(brokenPath, broken);
    const auto before = readAll(path);

    CHECK(isCommandLineCommand("info") && isCommandLineCommand("export-png"));
    CHECK(!isCommandLineCommand("--version") && !isCommandLineCommand("open"));
    CHECK(commandNeedsGraphics("export-png") && !commandNeedsGraphics("validate"));
    CHECK(commandLineUsage().contains("export-png") && commandLineUsage().contains("Exit codes"));

    // info
    {
        const auto result = run({"info", path});
        CHECK_EQ(result.code, 0);
        CHECK(result.out.contains("a.blp"));
        CHECK(result.out.contains("1.3"));
        CHECK(result.out.contains("50468") || result.out.contains("50,468"));
        CHECK(result.err.isEmpty());
        CHECK_EQ(run({"info"}).code, 2);
        CHECK_EQ(run({"info", path, otherPath}).code, 2);
        const auto missing = run({"info", dir.filePath("missing.blp")});
        CHECK_EQ(missing.code, 2);
        CHECK(missing.err.contains("missing.blp"));
        CHECK_EQ(run({"info", path, "--bogus"}).code, 2);
    }
    // validate
    {
        const auto clean = run({"validate", path, "--game", "1"});
        CHECK_EQ(clean.code, 0);
        CHECK(clean.out.contains("0 error(s)"));
        const auto bad = run({"validate", brokenPath, "--game", "1"});
        CHECK_EQ(bad.code, 1);
        CHECK(bad.out.contains("[error] STRUCT-TRUNCATED"));
        CHECK(bad.out.contains("HDR-MUSIC-RANGE"));
        CHECK(bad.out.contains("@222+2"));
        // Without a game, the game-specific rule is not judged.
        const auto noGame = run({"validate", brokenPath});
        CHECK_EQ(noGame.code, 1);
        CHECK(!noGame.out.contains("HDR-MUSIC-RANGE"));
        CHECK_EQ(run({"validate", path, "--game", "3"}).code, 2);
        CHECK_EQ(run({"validate", path, "--game"}).code, 2);
        CHECK_EQ(run({"validate"}).code, 2);
    }
    // diff
    {
        const auto same = run({"diff", path, path});
        CHECK_EQ(same.code, 0);
        CHECK(same.out.contains("identical"));
        const auto different = run({"diff", path, otherPath});
        CHECK_EQ(different.code, 1);
        CHECK(different.out.contains("offset 222, 1 byte(s), header.music: 03 -> 04"));
        CHECK(different.out.contains("offset 1608, 1 byte(s), foreground[3][70]: 09 -> 0a"));
        CHECK(different.out.contains("2 byte(s) in 2 range(s) differ"));
        const auto shorter = run({"diff", path, brokenPath});
        CHECK_EQ(shorter.code, 1);
        CHECK(shorter.out.contains("sizes differ: 50468 and 50368 bytes"));
        CHECK_EQ(run({"diff", path}).code, 2);
        CHECK_EQ(run({"diff", path, dir.filePath("missing.blp")}).code, 2);
    }
    for (const auto *command : {"export-png", "export-svg"}) {
        const auto before = readAll(path);
        CHECK_EQ(run({command, path, path}).code, 2);
        CHECK(readAll(path) == before);
    }
    {
        const auto alias = dir.filePath("alias.blp");
        std::error_code error;
        std::filesystem::create_hard_link(QFileInfo(path).filesystemAbsoluteFilePath(),
                                          QFileInfo(alias).filesystemAbsoluteFilePath(), error);
        if (!error) {
            const auto before = readAll(path);
            CHECK_EQ(run({"export-svg", path, alias}).code, 2);
            CHECK(readAll(alias) == before && readAll(path) == before);
        }
    }
    // export-png: the same bytes as the library call with the same options.
    {
        const auto out = dir.filePath("whole.png");
        const auto result = run({"export-png", path, out, "--scale", "8"});
        CHECK_EQ(result.code, 0);
        CHECK(result.out.contains("800 x 800 pixels"));
        PngExportOptions options;
        options.pixelsPerCell = 8;
        options.render.showLabels = false;
        const auto reference = dir.filePath("reference.png");
        const WorldView view(world);
        CHECK(exportWorldPng(view, GameProfile::unknown, options, reference).ok);
        CHECK(readAll(out) == readAll(reference));
        CHECK(!readAll(out).isEmpty());

        // A crop: four by three cells around the tile at (40, 40).
        const auto cropped = dir.filePath("crop.png");
        const auto crop = run({"export-png", path, cropped, "--crop", "39,39,4,3", "--scale",
                               "16", "--game", "1", "--no-starts", "--grid"});
        CHECK_EQ(crop.code, 0);
        CHECK(crop.out.contains("64 x 48 pixels"));
        QImageReader reader(cropped);
        CHECK(reader.size() == QSize(64, 48));
        CHECK(reader.text("EggbertSettings").contains("crop=39,39,4,3"));
        CHECK(reader.text("EggbertSettings").contains("starts=0"));
        CHECK(reader.text("EggbertSettings").contains("grid=1"));
        CHECK_EQ(reader.text("EggbertProfile"), QString("speedy-blupi-1"));
        const auto image = reader.read();
        // Cell (40, 40) is the second cell of the crop in both directions.
        const auto whole = QImage(out);
        CHECK(image.pixelColor(16 + 8, 16 + 8) != image.pixelColor(2 * 16 + 8, 8));
        // The crop equals that part of a whole export at the same scale.
        options.pixelsPerCell = 16;
        const auto full = renderWorldForExport(view, options);
        PngExportOptions cropOptions = options;
        cropOptions.crop = CellRect{{39, 39}, {42, 41}};
        const auto part = renderWorldForExport(view, cropOptions);
        CHECK(part.size() == QSize(64, 48));
        CHECK(part == full.copy(39 * 16, 39 * 16, 64, 48));
        CHECK(!whole.isNull());
        // A crop reaching past the map is clamped by the library, refused by
        // the command.
        cropOptions.crop = CellRect{{98, 98}, {150, 150}};
        CHECK(renderWorldForExport(view, cropOptions).size() == QSize(32, 32));
        CHECK_EQ(run({"export-png", path, cropped, "--crop", "98,98,5,5"}).code, 2);
        CHECK_EQ(run({"export-png", path, cropped, "--crop", "1,2,3"}).code, 2);
        CHECK_EQ(run({"export-png", path, cropped, "--scale", "0"}).code, 2);
        CHECK_EQ(run({"export-png", path, cropped, "--scale", "65"}).code, 2);
        CHECK_EQ(run({"export-png", path}).code, 2);
        // An output that cannot be written, a game directory that is none.
        CHECK_EQ(run({"export-png", path, dir.filePath("no/such/dir/x.png")}).code, 2);
        const auto noGame = run({"export-png", path, cropped, "--game-data", dir.path()});
        CHECK_EQ(noGame.code, 2);
        CHECK(noGame.err.contains("not recognised"));
    }
    // convert
    {
        CHECK(isCommandLineCommand("convert") && !commandNeedsGraphics("convert"));
        CHECK(commandLineUsage().contains("convert FILE OUT --to"));
        // Up: the output is exactly what the library produces.
        const auto oldWorld = blankWorld(fxRevision10);
        const auto oldPath = dir.filePath("old.blp");
        writeAll(oldPath, oldWorld);
        const auto upPath = dir.filePath("up.blp");
        const auto up = run({"convert", oldPath, upPath, "--to", "1.3"});
        CHECK_EQ(up.code, 0);
        CHECK(up.out.contains("revision 1.3, 50468 bytes"));
        CHECK(up.out.contains("empty bigDecor layer was added"));
        const auto expected = convertRevision(oldWorld, FormatRevision::v1_3);
        CHECK(expected.has_value());
        const auto written = readAll(upPath);
        CHECK_EQ(written.size(), qsizetype{50'468});
        CHECK(std::equal(written.begin(), written.end(),
                         reinterpret_cast<const char *>(expected->bytes.data())));
        CHECK_EQ(run({"validate", upPath}).code, 0);
        // An existing output is never replaced.
        const auto again = run({"convert", oldPath, upPath, "--to", "1.2"});
        CHECK_EQ(again.code, 2);
        CHECK(again.err.contains("already exists"));
        CHECK(readAll(upPath) == written);
        // Down with something the target cannot store: refused, nothing
        // written, unless the loss is accepted.
        auto full = world;
        put16(full, fxSlotOffset(fxRevision13, 150), 5);
        const auto fullPath = dir.filePath("full.blp");
        writeAll(fullPath, full);
        const auto downPath = dir.filePath("down.blp");
        const auto refused = run({"convert", fullPath, downPath, "--to", "1.1"});
        CHECK_EQ(refused.code, 1);
        CHECK(refused.err.contains("slots 100 to 199"));
        CHECK(refused.err.contains("--allow-loss"));
        CHECK(!QFile::exists(downPath));
        const auto forced = run({"convert", fullPath, downPath, "--to", "1.1", "--allow-loss"});
        CHECK_EQ(forced.code, 0);
        CHECK(forced.out.contains("Lost:"));
        CHECK_EQ(readAll(downPath).size(), qsizetype{45'668});
        // Usage and file errors.
        const auto unused = dir.filePath("unused.blp");
        CHECK_EQ(run({"convert", path, unused}).code, 2);
        CHECK_EQ(run({"convert", path, unused, "--to", "2.0"}).code, 2);
        CHECK_EQ(run({"convert", path, unused, "--to", "1.3"}).code, 2); // already 1.3
        CHECK_EQ(run({"convert", brokenPath, unused, "--to", "1.2"}).code, 2);
        CHECK_EQ(run({"convert", dir.filePath("missing.blp"), unused, "--to", "1.2"}).code, 2);
        CHECK_EQ(run({"convert", path, dir.filePath("no/such/dir/x.blp"), "--to", "1.2"}).code, 2);
        CHECK(!QFile::exists(unused));
        CHECK(readAll(oldPath) == QByteArray(reinterpret_cast<const char *>(oldWorld.data()),
                                              static_cast<qsizetype>(oldWorld.size())));
    }
    // new
    {
        CHECK(isCommandLineCommand("new") && !commandNeedsGraphics("new"));
        CHECK(commandLineUsage().contains("new OUT --game"));
        for (const auto &[flag, profile, revision] :
             {std::tuple{"1", GameProfile::speedyBlupi1, "1.2"},
              std::tuple{"2", GameProfile::speedyBlupi2, "1.3"}}) {
            const auto created = dir.filePath(QStringLiteral("new%1.blp").arg(flag));
            const auto result = run({"new", created, "--game", flag});
            CHECK_EQ(result.code, 0);
            CHECK(result.out.contains(QStringLiteral("revision %1, 50468 bytes").arg(revision)));
            const auto expected = newWorld(profile);
            const auto written = readAll(created);
            CHECK_EQ(written.size(), static_cast<qsizetype>(expected.size()));
            CHECK(std::equal(written.begin(), written.end(),
                             reinterpret_cast<const char *>(expected.data())));
            CHECK_EQ(run({"validate", created, "--game", flag}).code, 0);
            // Never over an existing file.
            const auto again = run({"new", created, "--game", flag});
            CHECK_EQ(again.code, 2);
            CHECK(again.err.contains("already exists"));
            CHECK(readAll(created) == written);
        }
        const auto unused = dir.filePath("unused-new.blp");
        CHECK_EQ(run({"new", unused}).code, 2);
        CHECK_EQ(run({"new", unused, "--game", "3"}).code, 2);
        CHECK_EQ(run({"new", "--game", "1"}).code, 2);
        CHECK_EQ(run({"new", dir.filePath("no/such/dir/x.blp"), "--game", "1"}).code, 2);
        CHECK(!QFile::exists(unused));
    }
    // convert-game
    {
        CHECK(isCommandLineCommand("convert-game") && !commandNeedsGraphics("convert-game"));
        CHECK(commandLineUsage().contains("convert-game FILE OUT --from"));
        // A world of the second game with one tile and one object the first
        // game does not have.
        auto modern = blankWorld(fxRevision13);
        put16(modern, fxCellOffset(fxForegroundOffset, 7, 7), 300);
        put16(modern, fxSlotOffset(fxRevision13, 2), 44);
        const auto modernPath = dir.filePath("modern.blp");
        writeAll(modernPath, modern);
        const auto outPath = dir.filePath("for-first.blp");
        const auto refused = run({"convert-game", modernPath, outPath, "--from", "2", "--to", "1"});
        CHECK_EQ(refused.code, 1);
        CHECK(refused.err.contains("unknown to the target game"));
        CHECK(refused.err.contains("[unknown] foreground tile 300 (1 place(s), first at cell 7,7)"));
        CHECK(refused.err.contains("[unknown] object type 44 (1 place(s), first in slot 2)"));
        CHECK(refused.err.contains("--unknown keep"));
        CHECK(!QFile::exists(outPath));
        // Kept: the library's bytes, the notes on standard output.
        const auto kept = run({"convert-game", modernPath, outPath, "--from", "2", "--to", "1",
                               "--unknown", "keep"});
        CHECK_EQ(kept.code, 0);
        CHECK(kept.out.contains("for game 1, 50468 bytes"));
        CHECK(kept.out.contains("[unknown] foreground tile 300"));
        const auto expectedKept = convertGame(modern, GameProfile::speedyBlupi2,
                                              GameProfile::speedyBlupi1, UnknownPolicy::keep);
        CHECK(expectedKept.has_value());
        const auto keptBytes = readAll(outPath);
        CHECK(std::equal(keptBytes.begin(), keptBytes.end(),
                         reinterpret_cast<const char *>(expectedKept->bytes.data())));
        CHECK_EQ(keptBytes.size(), qsizetype{50'468});
        // Removed, to another file (the first is never replaced).
        CHECK_EQ(run({"convert-game", modernPath, outPath, "--from", "2", "--to", "1",
                      "--unknown", "remove"})
                     .code,
                 2);
        const auto removedPath = dir.filePath("for-first-clean.blp");
        const auto removed = run({"convert-game", modernPath, removedPath, "--from", "2", "--to",
                                  "1", "--unknown", "remove"});
        CHECK_EQ(removed.code, 0);
        CHECK(removed.out.contains("2 place(s) cleared"));
        CHECK(!removed.out.contains("[unknown]"));
        auto expectedRemoved = blankWorld(fxRevision12);
        const auto removedBytes = readAll(removedPath);
        CHECK(std::equal(removedBytes.begin(), removedBytes.end(),
                         reinterpret_cast<const char *>(expectedRemoved.data())));
        // The other direction with nothing unknown needs no choice.
        const auto backPath = dir.filePath("for-second.blp");
        const auto back = run({"convert-game", removedPath, backPath, "--from", "1", "--to", "2"});
        CHECK_EQ(back.code, 0);
        CHECK(back.out.contains("for game 2"));
        // Usage and file errors.
        const auto unused = dir.filePath("unused-game.blp");
        CHECK_EQ(run({"convert-game", modernPath, unused}).code, 2);
        CHECK_EQ(run({"convert-game", modernPath, unused, "--from", "2", "--to", "2"}).code, 2);
        CHECK_EQ(run({"convert-game", modernPath, unused, "--from", "2", "--to", "3"}).code, 2);
        CHECK_EQ(run({"convert-game", modernPath, unused, "--from", "2", "--to", "1", "--unknown",
                      "maybe"})
                     .code,
                 2);
        CHECK_EQ(run({"convert-game", brokenPath, unused, "--from", "2", "--to", "1"}).code, 2);
        CHECK_EQ(run({"convert-game", dir.filePath("missing.blp"), unused, "--from", "2", "--to",
                      "1"})
                     .code,
                 2);
        CHECK(!QFile::exists(unused));
        CHECK(readAll(modernPath).size() == 50'468 && readAll(modernPath)[2] == 3);
    }
    // info on a Mobile Eggbert text world: described as such, not as BLP.
    {
        QByteArray text = "DescFile: posDecor=0;0 dimDecor=100;100 world=0 music=2 region=5 "
                          "blupiPos=194;198 blupiDir=2 \n";
        for (const auto *section : {"Decor: \n", "BigDecor: \n"}) {
            text += section;
            for (int x = 0; x < 100; ++x) {
                text += QByteArray(100, ',') + "\n";
            }
        }
        text += "MoveObject: type=7 posStart=322;196 posEnd=322;196 posCurrent=322;196 "
                "channel=10 icon=29 \n";
        const auto textPath = dir.filePath("world001.txt");
        QFile file(textPath);
        CHECK(file.open(QIODevice::WriteOnly));
        file.write(text);
        file.close();
        const auto result = run({"info", textPath});
        CHECK_EQ(result.code, 0);
        CHECK(result.out.contains("world001.txt"));
        CHECK(result.out.contains("Mobile Eggbert text world"));
        CHECK(result.out.contains("music 2; region 5"));
        CHECK(result.out.contains("objects: 1, type 7 x 1"));
        CHECK(result.out.contains("The file has the expected shape."));
        CHECK(!result.out.contains("Revision"));
        CHECK(readAll(textPath) == text);
        // The converters have nothing to do with it.
        const auto unused = dir.filePath("never.blp");
        CHECK_EQ(run({"convert", textPath, unused, "--to", "1.3"}).code, 2);
        CHECK_EQ(run({"convert-game", textPath, unused, "--from", "1", "--to", "2"}).code, 2);
        CHECK(!QFile::exists(unused));
    }
    // export-svg: a well-formed schematic picture with one rectangle per
    // occupied cell, the same bytes every time, never game artwork.
    {
        CHECK(isCommandLineCommand("export-svg") && commandNeedsGraphics("export-svg"));
        const auto svgPath = dir.filePath("a.svg");
        CHECK_EQ(run({"export-svg", path, svgPath}).code, 2);
        CHECK(!QFileInfo::exists(svgPath));
        const auto result = run({"export-svg", "--schematic", path, svgPath, "--scale", "10"});
        CHECK_EQ(result.code, 0);
        CHECK(result.out.contains("schematic SVG"));
        const auto svg = readAll(svgPath);
        SvgExportOptions options;
        options.cellSize = 10;
        CHECK(svg == renderWorldSvg(WorldView(world), options));
        QXmlStreamReader xml(svg);
        int tiles = 0;
        int starts = 0;
        bool sawCell = false;
        QString width;
        while (!xml.atEnd()) {
            xml.readNext();
            if (!xml.isStartElement()) {
                continue;
            }
            const auto attributes = xml.attributes();
            if (xml.name() == QLatin1String("svg")) {
                width = attributes.value("width").toString();
            } else if (xml.name() == QLatin1String("rect") && attributes.hasAttribute("data-tile")) {
                ++tiles;
                // The tile at cell (3, 70) holds 9: at x 30, y 700, 10 wide.
                sawCell = sawCell || (attributes.value("x") == QLatin1String("30") &&
                                      attributes.value("y") == QLatin1String("700") &&
                                      attributes.value("data-tile") == QLatin1String("9") &&
                                      attributes.value("width") == QLatin1String("10"));
            } else if (xml.name() == QLatin1String("polygon")) {
                ++starts;
            }
        }
        CHECK(!xml.hasError());
        CHECK_EQ(width, QString("1000"));
        CHECK_EQ(tiles, 2); // the two foreground tiles of this world
        CHECK(sawCell);
        CHECK_EQ(starts, 4);
        CHECK(!svg.contains("<image")); // no raster content at all
        // Layers can be left out.
        const auto bare = dir.filePath("bare.svg");
        CHECK_EQ(run({"export-svg", "--schematic", path, bare, "--no-foreground", "--no-starts"}).code, 0);
        CHECK(!readAll(bare).contains("data-tile") && !readAll(bare).contains("<polygon"));
        // An object is a circle that names its slot and type; when it moves,
        // the line of its path names the slot too.
        {
            auto moving = world;
            const auto slot = fxSlotOffset(fxRevision13, 2);
            put16(moving, slot, 1); // a lift
            for (const std::size_t field : {12u, 28u}) {
                put32(moving, slot + field, 640);
                put32(moving, slot + field + 4, 640);
            }
            put32(moving, slot + 20, 1280);
            put32(moving, slot + 24, 640);
            const auto movingPath = dir.filePath("moving.blp");
            writeAll(movingPath, moving);
            const auto movingSvg = dir.filePath("moving.svg");
            CHECK_EQ(run({"export-svg", "--schematic", movingPath, movingSvg}).code, 0);
            const auto picture = readAll(movingSvg);
            CHECK(picture.contains("data-slot=\"2\" data-type=\"1\""));
            CHECK_EQ(picture.count("data-path-of=\"2\""), qsizetype{1});
            CHECK_EQ(picture.count("<line"), qsizetype{1});
        }
        CHECK_EQ(run({"export-svg", "--schematic", path}).code, 2);
        CHECK_EQ(run({"export-svg", "--schematic", path, svgPath, "--scale", "0"}).code, 2);
        // A file cut short after its tile layers can still be drawn; one
        // without a complete tile layer cannot.
        CHECK_EQ(run({"export-svg", "--schematic", brokenPath, dir.filePath("broken.svg")}).code, 0);
        auto stub = world;
        stub.resize(900);
        const auto stubPath = dir.filePath("stub.blp");
        writeAll(stubPath, stub);
        const auto refusedSvg = run({"export-svg", "--schematic", stubPath, dir.filePath("stub.svg")});
        CHECK_EQ(refusedSvg.code, 2);
        CHECK(refusedSvg.err.contains("complete tile layer"));
        CHECK(!QFile::exists(dir.filePath("stub.svg")));
        CHECK_EQ(run({"export-svg", "--schematic", path, dir.filePath("no/such/dir/x.svg")}).code, 2);
    }
    // export-batch: one picture per world, each equal to export-png's.
    {
        CHECK(isCommandLineCommand("export-batch") && commandNeedsGraphics("export-batch"));
        const auto outDir = dir.filePath("pictures");
        CHECK(QDir().mkpath(outDir));
        const auto result = run({"export-batch", outDir, path, otherPath, "--scale", "4"});
        CHECK_EQ(result.code, 0);
        CHECK(result.out.contains("2 of 2 picture(s) written"));
        const auto single = dir.filePath("single.png");
        CHECK_EQ(run({"export-png", path, single, "--scale", "4"}).code, 0);
        CHECK(readAll(QDir(outDir).filePath("a.png")) == readAll(single));
        CHECK(!readAll(QDir(outDir).filePath("b.png")).isEmpty());
        // A file that is not a world: the others are still written, exit 1.
        const auto mixed = run({"export-batch", outDir, path, dir.filePath("missing.blp")});
        CHECK_EQ(mixed.code, 1);
        CHECK(mixed.out.contains("1 of 2 picture(s) written"));
        // A wrong option is a usage error, reported once, and nothing is written.
        {
            const auto emptyDir = dir.filePath("no-pictures");
            CHECK(QDir().mkpath(emptyDir));
            for (const auto &bad : {QStringList{"--scale", "100"}, QStringList{"--frobnicate"},
                                    QStringList{"--scale"}}) {
                const auto wrong = run(QStringList{"export-batch", emptyDir, path, otherPath} + bad);
                CHECK_EQ(wrong.code, 2);
                CHECK_EQ(wrong.err.count("eggbert-editor: "), qsizetype{1});
                CHECK(!wrong.out.contains("picture(s) written"));
            }
            CHECK(QDir(emptyDir).entryList(QDir::Files).isEmpty());
        }
        // Two inputs with the same name, a missing directory, too few arguments.
        const auto twinDir = dir.filePath("twin");
        CHECK(QDir().mkpath(twinDir));
        writeAll(QDir(twinDir).filePath("a.blp"), world);
        CHECK_EQ(run({"export-batch", outDir, path, QDir(twinDir).filePath("a.blp")}).code, 2);
        CHECK_EQ(run({"export-batch", dir.filePath("nowhere"), path}).code, 2);
        CHECK_EQ(run({"export-batch", outDir}).code, 2);
    }
    // merge
    {
        CHECK(isCommandLineCommand("merge") && !commandNeedsGraphics("merge"));
        CHECK(commandLineUsage().contains("merge BASE MINE THEIRS OUT"));
        const auto base = blankWorld(fxRevision13);
        auto mine = base;
        auto theirs = base;
        put16(mine, fxCellOffset(fxForegroundOffset, 1, 1), 30);
        put16(theirs, fxCellOffset(fxForegroundOffset, 2, 2), 31);
        const auto basePath = dir.filePath("merge-base.blp");
        const auto minePath = dir.filePath("merge-mine.blp");
        const auto theirsPath = dir.filePath("merge-theirs.blp");
        writeAll(basePath, base);
        writeAll(minePath, mine);
        writeAll(theirsPath, theirs);
        const auto outPath = dir.filePath("merged.blp");
        const auto clean = run({"merge", basePath, minePath, theirsPath, outPath});
        CHECK_EQ(clean.code, 0);
        CHECK(clean.out.contains("1 element(s) from mine, 1 from theirs, 0 changed alike, 0 "
                                 "conflict(s)"));
        const auto expected = mergeWorlds(base, mine, theirs, MergePreference::none);
        CHECK(expected.has_value());
        const auto written = readAll(outPath);
        CHECK_EQ(written.size(), qsizetype{50'468});
        CHECK(std::equal(written.begin(), written.end(),
                         reinterpret_cast<const char *>(expected->bytes.data())));
        // Never over an existing file.
        CHECK_EQ(run({"merge", basePath, minePath, theirsPath, outPath}).code, 2);

        // A conflict: listed, nothing written, unless a side is preferred.
        put16(theirs, fxCellOffset(fxForegroundOffset, 1, 1), 44);
        writeAll(theirsPath, theirs);
        const auto conflictPath = dir.filePath("merged-conflict.blp");
        const auto refused = run({"merge", basePath, minePath, theirsPath, conflictPath});
        CHECK_EQ(refused.code, 1);
        CHECK(refused.err.contains("conflict at offset 1070, 2 byte(s)"));
        CHECK(refused.err.contains("mine 1e 00 | theirs 2c 00"));
        CHECK(refused.err.contains("1 conflict(s); nothing was written"));
        CHECK(!QFile::exists(conflictPath));
        const auto preferred = run({"merge", basePath, minePath, theirsPath, conflictPath,
                                    "--prefer", "theirs"});
        CHECK_EQ(preferred.code, 0);
        CHECK(preferred.out.contains("1 conflict(s) resolved for theirs"));
        CHECK(readAll(conflictPath) ==
              QByteArray(reinterpret_cast<const char *>(theirs.data()),
                         static_cast<qsizetype>(theirs.size())));
        // Usage and file errors.
        const auto unused = dir.filePath("unused-merge.blp");
        CHECK_EQ(run({"merge", basePath, minePath, theirsPath}).code, 2);
        CHECK_EQ(run({"merge", basePath, minePath, theirsPath, unused, "--prefer", "both"}).code,
                 2);
        CHECK_EQ(run({"merge", basePath, minePath, dir.filePath("missing.blp"), unused}).code, 2);
        CHECK_EQ(run({"merge", basePath, minePath, brokenPath, unused}).code, 2);
        CHECK(!QFile::exists(unused));
    }
    // info on a demo recording of the games: described as what it is, not
    // as a damaged revision 1.0 world.
    {
        Bytes demo(204, std::byte{0});
        demo[0] = std::byte{1};
        for (const int value : {10, 2, 20, 18, 26, 2, 300, 0}) {
            demo.push_back(static_cast<std::byte>(value & 0xff));
            demo.push_back(static_cast<std::byte>((value >> 8) & 0xff));
        }
        const auto demoPath = dir.filePath("demo200.blp");
        writeAll(demoPath, demo);
        const auto described = run({"info", demoPath});
        CHECK_EQ(described.code, 0);
        CHECK(described.out.startsWith("demo200.blp\n220 bytes\n"));
        CHECK(described.out.contains("Demo recording, revision 1.0: 4 event(s) over 300 game "
                                     "step(s)."));
        CHECK(described.out.contains("Right: pressed 1 time(s), held for 290 step(s)"));
        CHECK(!described.out.contains("truncated"));
        CHECK(!described.out.contains("step 20: "));
        const auto listed = run({"info", demoPath, "--events"});
        CHECK_EQ(listed.code, 0);
        CHECK(listed.out.contains("step 10: Right\nstep 20: Right+Jump\nstep 26: Right\n"
                                  "step 300: none\n"));
        CHECK(commandLineUsage().contains("info FILE [--events]"));
        // A world ignores the option; a truncated world is still a world.
        CHECK_EQ(run({"info", path, "--events"}).code, 0);
        CHECK(run({"info", brokenPath}).out.contains("truncated"));
    }
    // scenario-record and scenario-check
    {
        CHECK(isCommandLineCommand("scenario-record") && isCommandLineCommand("scenario-check"));
        CHECK(!commandNeedsGraphics("scenario-check"));
        CHECK(commandLineUsage().contains("scenario-record FILE OUT.txt"));
        CHECK(commandLineUsage().contains("scenario-check FILE SCENARIO.txt"));
        auto subject = blankWorld(fxRevision13);
        put16(subject, fxSlotOffset(fxRevision13, 0), 5);
        put16(subject, fxSlotOffset(fxRevision13, 3), 5);
        const auto subjectPath = dir.filePath("scenario-world.blp");
        writeAll(subjectPath, subject);
        const auto scenarioPath = dir.filePath("scenario.txt");
        const auto recorded = run({"scenario-record", subjectPath, scenarioPath, "--game", "1"});
        CHECK_EQ(recorded.code, 0);
        CHECK(recorded.out.contains("expectation(s) recorded"));
        const auto text = readAll(scenarioPath);
        CHECK(text.contains("objects.type.5 = 2\n"));
        CHECK(text.contains("doctor.errors = "));
        // Never over an existing file.
        CHECK_EQ(run({"scenario-record", subjectPath, scenarioPath}).code, 2);
        CHECK(readAll(scenarioPath) == text);
        // The world it was recorded from passes.
        const auto passed = run({"scenario-check", subjectPath, scenarioPath, "--game", "1"});
        CHECK_EQ(passed.code, 0);
        CHECK(passed.out.contains(" expectation(s) checked, 0 failed"));
        // A changed world fails, with the line and both numbers.
        put16(subject, fxSlotOffset(fxRevision13, 3), 0);
        const auto changedPath = dir.filePath("scenario-changed.blp");
        writeAll(changedPath, subject);
        const auto failed = run({"scenario-check", changedPath, scenarioPath, "--game", "1"});
        CHECK_EQ(failed.code, 1);
        CHECK(failed.out.contains("objects.type.5 is 1, expected = 2."));
        CHECK(failed.out.contains(" failed\n") && !failed.out.contains(", 0 failed"));
        // A hand-written scenario.
        const auto ownPath = dir.filePath("own-scenario.txt");
        {
            QFile own(ownPath);
            CHECK(own.open(QIODevice::WriteOnly));
            own.write("# at least one treasure\nobjects.type.5 >= 1\n");
        }
        CHECK_EQ(run({"scenario-check", changedPath, ownPath}).code, 0);
        // Usage and file errors.
        {
            QFile bad(dir.filePath("bad-scenario.txt"));
            CHECK(bad.open(QIODevice::WriteOnly));
            bad.write("objects.type.5 about 2\n");
        }
        const auto malformed = run({"scenario-check", changedPath, dir.filePath("bad-scenario.txt")});
        CHECK_EQ(malformed.code, 2);
        CHECK(malformed.err.contains("Line 1: "));
        CHECK_EQ(run({"scenario-check", changedPath}).code, 2);
        CHECK_EQ(run({"scenario-check", changedPath, dir.filePath("missing.txt")}).code, 2);
        CHECK_EQ(run({"scenario-check", dir.filePath("missing.blp"), ownPath}).code, 2);
        CHECK_EQ(run({"scenario-check", changedPath, ownPath, "--game", "3"}).code, 2);
        CHECK_EQ(run({"scenario-record", subjectPath}).code, 2);
        const auto unusedScenario = dir.filePath("unused-scenario.txt");
        CHECK_EQ(run({"scenario-record", dir.filePath("missing.blp"), unusedScenario}).code, 2);
        CHECK(!QFile::exists(unusedScenario));
        // A damaged world is still described: what World Doctor says is a fact too.
        const auto damagedScenario = dir.filePath("damaged-scenario.txt");
        CHECK_EQ(run({"scenario-record", brokenPath, damagedScenario}).code, 0);
        CHECK(readAll(damagedScenario).contains("doctor.rule.STRUCT-TRUNCATED = 1\n"));
    }
    CHECK_EQ(run({}).code, 2);
    CHECK_EQ(run({"frobnicate"}).code, 2);
    // No command changed its input.
    CHECK(readAll(path) == before);
    // A Mobile Eggbert text world: read under its own profile, drawn with
    // the port's pictures, and never turned into a .blp file.
    {
        const auto port = makePort(dir.filePath("port"));
        const auto text = readAll(port.worldPath);
        const auto valid = run({"validate", port.worldPath});
        CHECK_EQ(valid.code, 0);
        CHECK(valid.out.contains("0 error(s)"));
        const auto withGame = run({"validate", port.worldPath, "--game", "2"});
        CHECK_EQ(withGame.code, 2);
        CHECK(withGame.err.contains("takes no --game"));
        // Tile 50 is one the second Windows game replaces on loading; the
        // port does not, so nothing is said about it here.
        {
            auto withTile = text;
            withTile.replace(",,,,40", ",,,,50");
            QFile file(dir.filePath("tile50.txt"));
            CHECK(file.open(QIODevice::WriteOnly));
            file.write(withTile);
            file.close();
            CHECK(!run({"validate", dir.filePath("tile50.txt")}).out.contains("TILE-SB2-REMAP"));
        }
        const auto pngPath = dir.filePath("port.png");
        const auto png = run({"export-png", port.worldPath, pngPath, "--game-data", port.root,
                              "--scale", "64", "--crop", "3,4,1,1", "--no-backdrop"});
        CHECK_EQ(png.code, 0);
        CHECK(QImage(pngPath).pixelColor(32, 32) == portColor(0, 40));
        // Pictures of another game are refused in both directions.
        const auto wrong = run({"export-png", path, dir.filePath("wrong.png"), "--game-data",
                                port.root});
        CHECK_EQ(wrong.code, 2);
        CHECK(wrong.err.contains("for its text worlds"));
        CHECK(!QFileInfo::exists(dir.filePath("wrong.png")));
        const auto svg = run({"export-svg", port.worldPath, dir.filePath("port.svg")});
        CHECK_EQ(svg.code, 0);
        CHECK(svg.out.contains("artwork SVG"));
        QXmlStreamReader xml(readAll(dir.filePath("port.svg")));
        bool foundTile = false;
        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement() && xml.name() == QLatin1String("image") &&
                xml.attributes().value("y") == QLatin1String("0")) {
                const auto url = xml.attributes().value("http://www.w3.org/1999/xlink", "href").toString();
                const QString prefix = QStringLiteral("data:image/png;base64,");
                CHECK(url.startsWith(prefix));
                const auto image = QImage::fromData(QByteArray::fromBase64(url.mid(prefix.size()).toLatin1()), "PNG");
                CHECK(!image.isNull());
                CHECK(image.pixelColor(3 * 64 + 32, 4 * 64 + 32) == portColor(0, 40));
                foundTile = true;
            }
        }
        CHECK(foundTile && !xml.hasError());
        const auto scenarioPath = dir.filePath("port.scenario");
        CHECK_EQ(run({"scenario-record", port.worldPath, scenarioPath}).code, 0);
        CHECK_EQ(run({"scenario-check", port.worldPath, scenarioPath}).code, 0);
        CHECK(run({"info", port.worldPath}).out.contains("Mobile Eggbert"));
        // The commands that write .blp files do not take it.
        CHECK_EQ(run({"convert", port.worldPath, dir.filePath("t.blp"), "--to", "1.3"}).code, 2);
        CHECK_EQ(run({"convert-game", port.worldPath, dir.filePath("t.blp"), "--from", "2",
                      "--to", "1"})
                     .code,
                 2);
        CHECK(!QFileInfo::exists(dir.filePath("t.blp")));
        CHECK(readAll(port.worldPath) == text);
        // Three text worlds merge into a text world: MINE's text with the
        // values THEIRS changed.
        const auto put = [&](const QString &name, QByteArray content) {
            QFile file(dir.filePath(name));
            CHECK(file.open(QIODevice::WriteOnly));
            file.write(content);
            return dir.filePath(name);
        };
        auto mineText = text;
        mineText.replace("music=3", "music=5");
        auto theirsText = text;
        theirsText.replace(",,,,40,", ",,,,40,42");
        const auto minePath = put("mine.txt", mineText);
        const auto theirsPath = put("theirs.txt", theirsText);
        const auto mergedPath = dir.filePath("merged.txt");
        const auto merged = run({"merge", port.worldPath, minePath, theirsPath, mergedPath});
        CHECK_EQ(merged.code, 0);
        CHECK(merged.out.contains("1 element(s) from mine, 1 from theirs"));
        auto expected = text;
        expected.replace("music=3", "music=5");
        expected.replace(",,,,40,", ",,,,40,42");
        CHECK(readAll(mergedPath) == expected);
        // Both sides changing one value differently is a conflict here too.
        auto otherMusic = text;
        otherMusic.replace("music=3", "music=6");
        const auto otherPath = put("other.txt", otherMusic);
        const auto conflict = run({"merge", port.worldPath, minePath, otherPath,
                                   dir.filePath("conflict.txt")});
        CHECK_EQ(conflict.code, 1);
        CHECK(!QFileInfo::exists(dir.filePath("conflict.txt")));
        const auto preferred = run({"merge", port.worldPath, minePath, otherPath,
                                    dir.filePath("conflict.txt"), "--prefer", "theirs"});
        CHECK_EQ(preferred.code, 0);
        CHECK(readAll(dir.filePath("conflict.txt")).contains("music=6"));
        // new --game mobile writes the text of a new world.
        const auto created = run({"new", dir.filePath("fresh.txt"), "--game", "mobile"});
        CHECK_EQ(created.code, 0);
        CHECK(created.out.contains("Mobile Eggbert text world"));
        CHECK(readAll(dir.filePath("fresh.txt")).startsWith("DescFile: "));
        CHECK_EQ(run({"validate", dir.filePath("fresh.txt")}).code, 0);
        CHECK_EQ(run({"new", dir.filePath("fresh.txt"), "--game", "mobile"}).code, 2);
        // A text world and .blp worlds do not mix.
        CHECK_EQ(run({"merge", path, minePath, otherPath, dir.filePath("mixed.txt")}).code, 2);
        CHECK(!QFileInfo::exists(dir.filePath("mixed.txt")));
    }
    return test::finish("cli");
}
