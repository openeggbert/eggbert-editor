// SPDX-License-Identifier: GPL-3.0-only
#include "app/main_window.hpp"
#include "app/png_export_dialog.hpp"
#include "app/smoke_test.hpp"

#include "check.hpp"
#include "fixture.hpp"
#include "generated_game.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace eggbert;
using namespace eggbert::test;

namespace {

// Colour the canvas shows inside a cell, away from the centre where the
// schematic drawing puts its value label.
QColor canvasAt(WorldCanvas *canvas, int x, int y) {
    const auto &transform = canvas->transform();
    const QPoint point(static_cast<int>(transform.toScreenX(x * 64 + 10)),
                       static_cast<int>(transform.toScreenY(y * 64 + 10)));
    return canvas->grab().toImage().pixelColor(point);
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setOrganizationName("eggbert-editor-tests");
    QApplication::setApplicationName("artwork-ui-test");
    QSettings().clear();

    QTemporaryDir dir;
    const auto game = makeGame(dir.filePath("game"));
    const auto digest = directoryDigest(game.root);
    auto bytes = blankWorld(fxRevision13);
    put16(bytes, fxCellOffset(fxForegroundOffset, 3, 70), 9);
    const auto path = dir.filePath("art.blp");
    write(path, QByteArray(reinterpret_cast<const char *>(bytes.data()),
                           static_cast<qsizetype>(bytes.size())));
    const auto schematic = schematicTileColor(SectionKind::foreground, 9);
    const auto painted = spriteColor(0, 9);

    {
        MainWindow window;
        window.prompts.error = [](const QString &, const QString &) {};
        window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
        window.prompts.saveAsPath = [](const QString &) { return QString(); };
        std::optional<std::pair<QString, GameProfile>> directoryAnswer;
        GameProfile suggested = GameProfile::unknown;
        window.prompts.chooseArtworkDirectory = [&](GameProfile profile) {
            suggested = profile;
            return directoryAnswer;
        };
        QString shown;
        window.prompts.showDocument = [&](const QString &, const QString &text, bool) {
            shown = text;
        };
        window.resize(1300, 950);
        window.show();
        CHECK(QTest::qWaitForWindowExposed(&window));

        auto *tab = window.openPath(path);
        CHECK(tab != nullptr);
        tab->setProfile(GameProfile::speedyBlupi1);
        QApplication::processEvents();
        auto *canvas = tab->canvas();
        canvas->actualPixels();
        canvas->centerOnCell({3, 70});
        QApplication::processEvents();

        // Without a directory everything is schematic and the toggle is off.
        CHECK(!window.action("viewArtwork")->isEnabled());
        CHECK(window.artworkFor(GameProfile::speedyBlupi1) == nullptr);
        CHECK(canvas->renderOptions().artwork == nullptr);
        CHECK(canvasAt(canvas, 3, 70) == schematic);
        window.action("viewArtworkReport")->trigger();
        CHECK(shown.contains("No directory chosen."));
        CHECK(shown.contains("never modified"));

        // Cancelling the directory dialog changes nothing; the tab's game is
        // offered as the suggestion.
        window.action("viewArtworkDirectory")->trigger();
        CHECK(suggested == GameProfile::speedyBlupi1);
        CHECK(window.artworkFor(GameProfile::speedyBlupi1) == nullptr);

        // Choosing the generated game directory loads it and draws with it.
        directoryAnswer = std::pair{game.root, GameProfile::speedyBlupi1};
        window.action("viewArtworkDirectory")->trigger();
        CHECK(window.artworkFor(GameProfile::speedyBlupi1) != nullptr);
        CHECK(window.artworkEnabled());
        CHECK(window.action("viewArtwork")->isEnabled());
        CHECK(window.action("viewArtwork")->isChecked());
        CHECK(shown.contains("246 sprites located in GAME.EXE"));
        CHECK(shown.contains("other editions may differ"));
        CHECK(canvas->renderOptions().artwork == window.artworkFor(GameProfile::speedyBlupi1));
        CHECK(canvasAt(canvas, 3, 70) == painted);
        CHECK(!tab->document().isModified());

        // Artwork belongs to one game: another profile draws schematically.
        tab->setProfile(GameProfile::speedyBlupi2);
        CHECK(canvas->renderOptions().artwork == nullptr);
        CHECK(canvasAt(canvas, 3, 70) == schematic);
        tab->setProfile(GameProfile::unknown);
        CHECK(canvas->renderOptions().artwork == nullptr);
        tab->setProfile(GameProfile::speedyBlupi1);
        CHECK(canvasAt(canvas, 3, 70) == painted);

        // The toggle switches back to the schematic drawing and on again.
        window.action("viewArtwork")->trigger();
        CHECK(!window.artworkEnabled());
        CHECK(canvasAt(canvas, 3, 70) == schematic);
        window.action("viewArtwork")->trigger();
        CHECK(canvasAt(canvas, 3, 70) == painted);

        // A directory that is not usable is reported and not used.
        CHECK(!window.loadArtwork(dir.filePath("nothing-here"), GameProfile::speedyBlupi2));
        CHECK(window.artworkFor(GameProfile::speedyBlupi2) == nullptr);
        CHECK(window.artworkReport().contains("No IMAGE08, IMAGE16 or IMAGE folder was found."));
        CHECK(!window.loadArtwork(game.root, GameProfile::unknown));

        // PNG export: the artwork option exists only when artwork is
        // available, and it carries a copyright note.
        {
            PngExportDialog withArtwork(canvas->renderOptions(),
                                        window.artworkFor(GameProfile::speedyBlupi1));
            auto *box = withArtwork.findChild<QCheckBox *>("pngArtwork");
            CHECK(box->isEnabled());
            CHECK(box->isChecked());
            CHECK(withArtwork.options().render.artwork != nullptr);
            CHECK(withArtwork.estimateText().contains("copyrighted"));
            box->setChecked(false);
            CHECK(withArtwork.options().render.artwork == nullptr);
            CHECK(withArtwork.estimateText().contains("uses no game artwork"));
            PngExportDialog without(canvas->renderOptions(), nullptr);
            CHECK(!without.findChild<QCheckBox *>("pngArtwork")->isEnabled());
            CHECK(without.options().render.artwork == nullptr);
        }
        // The world file is never told about the directory.
        CHECK(!toByteArray(tab->document().currentBytes()).contains(game.root.toUtf8()));
        CHECK(!tab->document().isModified());
        CHECK(window.close());
    }

    // The choice is remembered as a local preference and restored.
    {
        CHECK_EQ(QSettings().value("artwork/directory-game1").toString(), game.root);
        MainWindow restored;
        restored.prompts.error = [](const QString &, const QString &) {};
        CHECK(restored.artworkFor(GameProfile::speedyBlupi1) != nullptr);
        CHECK(restored.artworkEnabled());
        // A remembered directory that has disappeared just means schematic.
        QSettings().setValue("artwork/directory-game2", dir.filePath("gone"));
        MainWindow missing;
        CHECK(missing.artworkFor(GameProfile::speedyBlupi2) == nullptr);
        CHECK(missing.artworkFor(GameProfile::speedyBlupi1) != nullptr);
    }

    // The self check with a game directory: passes for a recognisable game,
    // fails for a directory that is none or whose tiles cannot be drawn.
    CHECK_EQ(runSmokeTest(game.root), 0);
    CHECK_EQ(runSmokeTest(dir.filePath("not-a-game")), 1);
    {
        const auto broken = makeGame(dir.filePath("broken-game"));
        CHECK(QFile::remove(broken.root + "/GAME.EXE"));
        CHECK_EQ(runSmokeTest(broken.root), 1);
    }

    // Nothing was written into the game directory.
    CHECK(directoryDigest(game.root) == digest);
    return test::finish("artwork_ui");
}
