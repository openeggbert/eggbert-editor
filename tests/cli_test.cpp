// SPDX-License-Identifier: GPL-3.0-only
//
// The headless commands: output, exit codes, read-only behaviour, and that
// export-png writes exactly what the library's exporter writes.

#include "app/cli.hpp"
#include "app/png_export.hpp"
#include "world/game_convert.hpp"
#include "world/new_world.hpp"
#include "world/revision_convert.hpp"
#include "world/world_view.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <algorithm>
#include <tuple>

#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QTemporaryDir>

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
    CHECK_EQ(run({}).code, 2);
    CHECK_EQ(run({"frobnicate"}).code, 2);
    // No command changed its input.
    CHECK(readAll(path) == before);
    return test::finish("cli");
}
