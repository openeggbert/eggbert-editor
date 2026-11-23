// SPDX-License-Identifier: GPL-3.0-only
//
// The interface around the documents: welcome page, drag and drop, zoom
// slider, tool pictograms, the game remembered per file and recognised from
// the folder a world is opened from.

#include "app/main_window.hpp"
#include "app/installation_panel.hpp"
#include "app/text_world_editor.hpp"
#include <QLineEdit>
#include "app/world_canvas.hpp"

#include "check.hpp"
#include "fixture.hpp"
#include "generated_game.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QListWidget>
#include <QMimeData>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QToolBar>
#include <QUrl>

using namespace eggbert;
using namespace eggbert::test;

namespace {

void writeWorld(const QString &path) {
    const auto bytes = blankWorld(fxRevision13);
    write(path, QByteArray(reinterpret_cast<const char *>(bytes.data()),
                           static_cast<qsizetype>(bytes.size())));
}

void configure(MainWindow &window) {
    window.prompts.error = [](const QString &, const QString &) {};
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    window.resize(1300, 900);
    window.show();
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setOrganizationName("eggbert-editor-tests");
    QApplication::setApplicationName("interface-ui-test");
    QSettings().clear();

    QTemporaryDir dir;
    const auto loose = dir.filePath("loose.blp");
    const auto other = dir.filePath("other.blp");
    writeWorld(loose);
    writeWorld(other);

    // Generated game directories with a world in their DATA folder.
    const auto first = makeGame(dir.filePath("first"));
    const auto second = makeGame(dir.filePath("second"), true);
    writeWorld(first.root + "/DATA/WORLD001.BLP");
    writeWorld(second.root + "/DATA/world001.blp");
    writeWorld(second.root + "/DATA/world010.blp");
    const auto firstDigest = directoryDigest(first.root);
    const auto secondDigest = directoryDigest(second.root);

    {
        MainWindow window;
        configure(window);
        CHECK(QTest::qWaitForWindowExposed(&window));

        // Nothing open: the welcome page, with its two buttons.
        CHECK(window.welcomeVisible());
        auto *recent = window.findChild<QListWidget *>("welcomeRecent");
        auto *openButton = window.findChild<QPushButton *>("welcomeOpen");
        auto *gameButton = window.findChild<QPushButton *>("welcomeGame");
        CHECK(recent != nullptr && openButton != nullptr && gameButton != nullptr);
        CHECK_EQ(recent->count(), 0);
        // "Open World" is the File menu's action.
        int asked = 0;
        window.prompts.openPaths = [&] {
            ++asked;
            return QStringList{loose};
        };
        openButton->click();
        CHECK_EQ(asked, 1);
        CHECK_EQ(window.tabCount(), 1);
        CHECK(!window.welcomeVisible());
        // "Choose Game Directory" is the View menu's action.
        int gameAsked = 0;
        window.prompts.chooseArtworkDirectory =
            [&](GameProfile) -> std::optional<std::pair<QString, GameProfile>> {
            ++gameAsked;
            return std::nullopt;
        };
        gameButton->click();
        CHECK_EQ(gameAsked, 1);

        // Closing the last world brings the page back, now with a recent entry.
        CHECK(window.closeTab(window.currentTab()));
        CHECK(window.welcomeVisible());
        CHECK_EQ(recent->count(), 1);
        CHECK_EQ(recent->item(0)->text(), QString("loose.blp"));
        CHECK(recent->item(0)->toolTip().endsWith("loose.blp"));
        QTest::mouseClick(recent->viewport(), Qt::LeftButton, Qt::NoModifier,
                          recent->visualItemRect(recent->item(0)).center());
        CHECK_EQ(window.tabCount(), 1);
        CHECK(!window.welcomeVisible());

        // Dropping files opens them; anything that is not a local file is
        // ignored.
        {
            QMimeData data;
            data.setUrls({QUrl::fromLocalFile(other), QUrl("https://example.invalid/x.blp"),
                          QUrl::fromLocalFile(dir.filePath("missing.blp"))});
            QDragEnterEvent enter(QPoint(50, 50), Qt::CopyAction, &data, Qt::LeftButton,
                                  Qt::NoModifier);
            QApplication::sendEvent(&window, &enter);
            CHECK(enter.isAccepted());
            QDropEvent drop(QPointF(50, 50), Qt::CopyAction, &data, Qt::LeftButton,
                            Qt::NoModifier);
            QApplication::sendEvent(&window, &drop);
            CHECK_EQ(window.tabCount(), 2);
            CHECK(window.currentTab()->file().path().endsWith("other.blp"));
            QMimeData text;
            text.setText("not a file");
            QDragEnterEvent refused(QPoint(50, 50), Qt::CopyAction, &text, Qt::LeftButton,
                                    Qt::NoModifier);
            refused.ignore();
            QApplication::sendEvent(&window, &refused);
            CHECK(!refused.isAccepted());
        }

        // The zoom slider and the canvas follow each other.
        auto *slider = window.findChild<QSlider *>("zoomSlider");
        CHECK(slider != nullptr && slider->isEnabled());
        auto *canvas = window.currentTab()->canvas();
        const auto fitted = slider->value();
        window.action("viewActualPixels")->trigger();
        QApplication::processEvents();
        CHECK(std::abs(canvas->zoom() - 1.0) < 1e-9);
        CHECK(slider->value() > fitted);
        // 1:1 is two thirds of the way from 1/64 to 8 in ratio steps.
        CHECK(std::abs(slider->value() - 667) <= 1);
        slider->setValue(889); // two more doublings, minus rounding: about 4
        CHECK(std::abs(canvas->zoom() - 4.0) < 0.02);
        slider->setValue(0);
        CHECK(std::abs(canvas->zoom() - 1.0 / 64.0) < 1e-9);
        slider->setValue(1000);
        CHECK(std::abs(canvas->zoom() - 8.0) < 1e-9);
        window.action("viewFitWorld")->trigger();

        // Every tool has its pictogram and keeps its name under it.
        for (const char *name : {"toolBrowse", "toolSelect", "toolPencil", "toolEraser",
                                 "toolEyedropper", "toolLine", "toolRectangle", "toolFill",
                                 "toolAdaptive", "toolStart", "toolObject", "toolPlaceObject"}) {
            CHECK(!window.action(name)->icon().isNull());
            CHECK(!window.action(name)->text().isEmpty());
        }
        CHECK(window.findChild<QToolBar *>("toolsToolBar")->toolButtonStyle() ==
              Qt::ToolButtonTextUnderIcon);
        // The dark theme redraws them in a light ink.
        const auto lightInk = window.action("toolPencil")->icon().pixmap(32, 32).toImage();
        window.setTheme("dark");
        CHECK(window.action("toolPencil")->icon().pixmap(32, 32).toImage() != lightInk);
        window.setTheme("light");

        // A loose file belongs to no game until one is chosen; the choice in
        // the tool bar is remembered for that file.
        auto *tab = window.openPath(loose);
        CHECK(tab->profile() == GameProfile::unknown);
        CHECK(MainWindow::rememberedProfile(loose) == GameProfile::unknown);
        auto *combo = window.findChild<QComboBox *>("profileCombo");
        combo->setCurrentIndex(combo->findData(static_cast<int>(GameProfile::speedyBlupi2)));
        CHECK(tab->profile() == GameProfile::speedyBlupi2);
        CHECK(MainWindow::rememberedProfile(loose) == GameProfile::speedyBlupi2);
        CHECK(MainWindow::rememberedProfile(other) == GameProfile::unknown);
        CHECK(window.closeTab(tab));
        CHECK(window.openPath(loose)->profile() == GameProfile::speedyBlupi2);
        // A profile set by the program (not chosen by the user) is not stored.
        window.openPath(other)->setProfile(GameProfile::speedyBlupi1);
        CHECK(MainWindow::rememberedProfile(other) == GameProfile::unknown);

        // A world in a game's DATA folder: the game is recognised and its
        // graphics are used, without a question and without writing there.
        CHECK(window.artworkFor(GameProfile::speedyBlupi1) == nullptr);
        auto *fromGame = window.openPath(first.root + "/DATA/WORLD001.BLP");
        CHECK(fromGame != nullptr);
        CHECK(fromGame->profile() == GameProfile::speedyBlupi1);
        CHECK(window.artworkFor(GameProfile::speedyBlupi1) != nullptr);
        CHECK(window.artworkEnabled());
        CHECK(fromGame->canvas()->renderOptions().artwork ==
              window.artworkFor(GameProfile::speedyBlupi1));
        CHECK(!fromGame->document().isModified());
        auto *fromSecond = window.openPath(second.root + "/DATA/world001.blp");
        CHECK(fromSecond->profile() == GameProfile::speedyBlupi2);
        CHECK(window.artworkFor(GameProfile::speedyBlupi2) != nullptr);
        CHECK(gameAsked == 1); // no further question was asked
        // A directory the user chose for a game is not replaced by another
        // folder of the same game.
        const auto another = makeGame(dir.filePath("another-first"));
        writeWorld(another.root + "/DATA/WORLD009.BLP");
        const auto chosen = window.artworkFor(GameProfile::speedyBlupi1)->directory();
        CHECK(window.openPath(another.root + "/DATA/WORLD009.BLP")->profile() ==
              GameProfile::speedyBlupi1);
        CHECK_EQ(window.artworkFor(GameProfile::speedyBlupi1)->directory(), chosen);
        // Choosing "no game" for the file forgets it; the folder decides again.
        window.close();
    }
    // The remembered game survives a restart, and an explicit choice beats
    // the folder.
    {
        MainWindow window;
        configure(window);
        CHECK(window.openPath(loose)->profile() == GameProfile::speedyBlupi2);
        auto *tab = window.openPath(first.root + "/DATA/WORLD001.BLP");
        CHECK(tab->profile() == GameProfile::speedyBlupi1);
        auto *combo = window.findChild<QComboBox *>("profileCombo");
        combo->setCurrentIndex(combo->findData(static_cast<int>(GameProfile::speedyBlupi2)));
        CHECK(window.closeTab(tab));
        CHECK(window.openPath(first.root + "/DATA/WORLD001.BLP")->profile() ==
              GameProfile::speedyBlupi2);
        combo->setCurrentIndex(combo->findData(static_cast<int>(GameProfile::unknown)));
        CHECK(MainWindow::rememberedProfile(first.root + "/DATA/WORLD001.BLP") ==
              GameProfile::unknown);
        window.close();
    }
    // Choosing a game directory is also an installation workflow when no
    // world is open: the dock is shown and populated, and several selected
    // worlds can be opened together.
    {
        MainWindow window;
        configure(window);
        CHECK(window.tabCount() == 0 && window.welcomeVisible());
        window.prompts.chooseArtworkDirectory = [&](GameProfile suggested) {
            CHECK(suggested == GameProfile::unknown);
            return std::optional<std::pair<QString, GameProfile>>(
                std::pair{second.root + "/DATA", GameProfile::speedyBlupi2});
        };
        window.prompts.showDocument = [](const QString &, const QString &, bool) {};
        window.action("fileOpenInstallation")->trigger();
        QApplication::processEvents();
        auto *installation = window.installation();
        CHECK(installation->gameDirectory() == second.root);
        CHECK(installation->dataDirectory() == second.root + "/DATA");
        CHECK_EQ(installation->list()->count(), 2);
        CHECK(window.action("viewInstallation")->isChecked());
        CHECK(!installation->openSelectedButton()->isEnabled());
        CHECK(window.action("filePlaytest")->isEnabled());
        int launched = 0;
        window.prompts.playtestCommand = [](GameProfile, const QString &) {
            return std::optional<QString>("fixture-player");
        };
        window.prompts.startProcess = [&](const QString &, const QStringList &, const QString &folder) {
            ++launched;
            return folder == second.root;
        };
        window.action("filePlaytest")->trigger();
        CHECK_EQ(launched, 1);
        auto *search = installation->findChild<QLineEdit *>("installationSearch");
        auto *roles = installation->findChild<QComboBox *>("installationRole");
        auto *openAll = installation->findChild<QPushButton *>("installationOpenAll");
        search->setText("010");
        roles->setCurrentIndex(roles->findData(static_cast<int>(WorldRole::hub)));
        openAll->click();
        CHECK_EQ(window.tabCount(), 1);
        CHECK(window.currentTab()->file().path().endsWith("world010.blp"));
        search->setText("999");
        CHECK(!openAll->isEnabled());
        search->clear();
        roles->setCurrentIndex(0);
        installation->list()->setFocus();
        QTest::keyClick(installation->list(), Qt::Key_A, Qt::ControlModifier);
        CHECK_EQ(installation->list()->selectedItems().size(), 2);
        CHECK(installation->openSelectedButton()->isEnabled());
        installation->openSelectedButton()->click();
        CHECK_EQ(window.tabCount(), 2);
        auto *world = window.currentTab();
        CHECK(world->profile() == GameProfile::speedyBlupi2);
        world->setProfile(GameProfile::speedyBlupi1);
        CHECK(!installation->deployButton()->isEnabled());
        CHECK(!window.action("fileDeploy")->isEnabled());
        CHECK(!installation->findChild<QPushButton *>("installationConvert")->isHidden());
        CHECK(!window.deployWorld(world));
        world->setProfile(GameProfile::speedyBlupi2);
        const auto original = world->file().savedContent();
        EditBuilder edit(world->document());
        edit.setCell(SectionKind::foreground, 7, 8, 42);
        CHECK(world->applyTransaction(edit.build("Save All test")));
        const auto path = world->file().path();
        window.prompts.reviewSaveAll = [&](const QStringList &paths) {
            CHECK(paths == QStringList{path});
            return std::optional<bool>(true);
        };
        QString report;
        window.prompts.showDocument = [&](const QString &, const QString &text, bool) { report = text; };
        CHECK(window.saveAll());
        CHECK(report.contains(path));
        const auto snapshots = window.recoveryStore().snapshots(path);
        CHECK(!snapshots.empty() && window.recoveryStore().read(snapshots.front()) == original);
        window.prompts.chooseSnapshot = [](DocumentTab *, const std::vector<RecoveryEntry> &listed) {
            return std::optional(std::pair{listed.front(), SnapshotAction::restore});
        };
        CHECK(window.manageSnapshots(world));
        CHECK(window.saveTab(world));
        window.close();
    }
    {
        MainWindow window;
        configure(window);
        QApplication::processEvents();
        CHECK(window.tabCount() == 0);
        CHECK(window.installation()->gameDirectory() == second.root);
        CHECK_EQ(window.installation()->list()->count(), 2);
        window.close();
    }
    CHECK(directoryDigest(first.root) == firstDigest);
    CHECK(directoryDigest(second.root) == secondDigest);
    // A Mobile Eggbert text world opens in a window of its own, not in a tab.
    {
        MainWindow window;
        QStringList errors;
        window.prompts.error = [&](const QString &, const QString &text) { errors << text; };
        QTemporaryDir textDir;
        const auto textPath = textDir.filePath("world020.txt");
        QFile file(textPath);
        CHECK(file.open(QIODevice::WriteOnly));
        const QByteArray content = "DescFile: posDecor=0;0 dimDecor=100;100 world=0 music=1 "
                                   "region=2 blupiPos=194;198 blupiDir=2 \nDecor: \n";
        file.write(content);
        file.close();
        CHECK(window.openPath(textPath) == nullptr);
        CHECK_EQ(window.tabCount(), 0);
        CHECK(errors.isEmpty());
        CHECK_EQ(window.textWorldEditors().size(), std::size_t{1});
        // Opening it again raises the same window.
        CHECK(window.openPath(textPath) == nullptr);
        CHECK_EQ(window.textWorldEditors().size(), std::size_t{1});
        auto *looseTab = window.openPath(loose);
        CHECK(!window.textWorldEditors().front()->saveAs(loose));
        window.prompts.saveAsPath = [&](const QString &) { return textPath; };
        CHECK(!window.saveTabAs(looseTab));
        QFile again(textPath);
        CHECK(again.open(QIODevice::ReadOnly) && again.readAll() == content);
    }
    // File -> Export World SVG writes the schematic picture of the tab.
    {
        MainWindow window;
        QStringList errors;
        window.prompts.error = [&](const QString &, const QString &text) { errors << text; };
        window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
        QTemporaryDir svgDir;
        const auto worldPath = svgDir.filePath("pic.blp");
        {
            auto bytes = eggbert::test::blankWorld(eggbert::test::fxRevision13);
            eggbert::test::put16(
                bytes, eggbert::test::fxCellOffset(eggbert::test::fxForegroundOffset, 2, 3), 30);
            QFile file(worldPath);
            CHECK(file.open(QIODevice::WriteOnly));
            file.write(reinterpret_cast<const char *>(bytes.data()),
                       static_cast<qint64>(bytes.size()));
        }
        auto *action = window.findChild<QAction *>("fileExportSvg");
        CHECK(action != nullptr && !action->isEnabled());
        CHECK(window.openPath(worldPath) != nullptr);
        CHECK(action->isEnabled());
        QString suggested;
        const auto svgPath = svgDir.filePath("out.svg");
        window.prompts.saveTextPath = [&](const QString &name) {
            suggested = name;
            return svgPath;
        };
        action->trigger();
        CHECK(!errors.isEmpty() && !QFileInfo::exists(svgPath));
        errors.clear();
        action = window.findChild<QAction *>("fileExportSchematicSvg");
        CHECK(action != nullptr && action->isEnabled());
        action->trigger();
        CHECK_EQ(suggested, QString("pic.svg"));
        CHECK(errors.isEmpty());
        QFile written(svgPath);
        CHECK(written.open(QIODevice::ReadOnly));
        const auto svg = written.readAll();
        CHECK(svg.startsWith("<?xml") && svg.contains("data-tile=\"30\""));
        const auto original = window.currentTab()->file().savedContent();
        window.prompts.saveTextPath = [&](const QString &) { return worldPath; };
        action->trigger();
        CHECK(!errors.isEmpty());
        QFile source(worldPath);
        CHECK(source.open(QIODevice::ReadOnly) && source.readAll() == original);
        // Cancelled: nothing is written.
        const auto other = svgDir.filePath("other.svg");
        window.prompts.saveTextPath = [](const QString &) { return QString(); };
        action->trigger();
        CHECK(!QFile::exists(other));
        // The standard action reads native artwork for a recognised desktop installation.
        CHECK(window.openPath(first.root + "/DATA/WORLD001.BLP") != nullptr);
        window.prompts.saveTextPath = [&](const QString &) { return other; };
        window.findChild<QAction *>("fileExportSvg")->trigger();
        QFile image(other);
        CHECK(image.open(QIODevice::ReadOnly));
        CHECK(image.readAll().contains("data:image/png;base64,"));
    }
    // File -> Record Scenario and Check Against Scenario: the scenario of the
    // world as it is now in the tab, unsaved edits included.
    {
        MainWindow window;
        QStringList errors;
        QStringList reports;
        window.prompts.error = [&](const QString &, const QString &text) { errors << text; };
        window.prompts.showReport = [&](const QString &, const QString &text) { reports << text; };
        window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
        QTemporaryDir scenarioDir;
        const auto worldPath = scenarioDir.filePath("level.blp");
        {
            auto bytes = eggbert::test::blankWorld(eggbert::test::fxRevision13);
            eggbert::test::put16(
                bytes, eggbert::test::fxCellOffset(eggbert::test::fxForegroundOffset, 2, 3), 30);
            QFile file(worldPath);
            CHECK(file.open(QIODevice::WriteOnly));
            file.write(reinterpret_cast<const char *>(bytes.data()),
                       static_cast<qint64>(bytes.size()));
        }
        auto *record = window.findChild<QAction *>("fileRecordScenario");
        auto *check = window.findChild<QAction *>("fileCheckScenario");
        CHECK(record != nullptr && check != nullptr);
        CHECK(!record->isEnabled() && !check->isEnabled());
        auto *tab = window.openPath(worldPath);
        CHECK(tab != nullptr);
        CHECK(record->isEnabled() && check->isEnabled());
        QString suggested;
        const auto scenarioPath = scenarioDir.filePath("kept.txt");
        window.prompts.saveTextPath = [&](const QString &name) {
            suggested = name;
            return scenarioPath;
        };
        record->trigger();
        CHECK_EQ(suggested, QString("level-scenario.txt"));
        QFile written(scenarioPath);
        CHECK(written.open(QIODevice::ReadOnly));
        CHECK(written.readAll().contains("foreground.occupied = 1\n"));
        window.prompts.scenarioPath = [&] { return scenarioPath; };
        check->trigger();
        CHECK_EQ(reports.size(), qsizetype{1});
        CHECK(reports.last().endsWith(" expectation(s) checked, 0 failed."));
        // An unsaved edit is checked too.
        {
            const std::vector<CellPos> cells{{5, 5}};
            CHECK(tab->paintStroke(cells, 31, QStringLiteral("Paint"), 0));
        }
        check->trigger();
        CHECK_EQ(reports.size(), qsizetype{2});
        CHECK(reports.last().contains("foreground.occupied is 2, expected = 1."));
        CHECK(reports.last().endsWith(" expectation(s) checked, 1 failed."));
        // What is not a scenario, a missing file and cancelling.
        const auto notScenario = scenarioDir.filePath("notes.txt");
        {
            QFile file(notScenario);
            CHECK(file.open(QIODevice::WriteOnly));
            file.write("just some notes\n");
        }
        window.prompts.scenarioPath = [&] { return notScenario; };
        check->trigger();
        CHECK_EQ(errors.size(), qsizetype{1});
        CHECK(errors.last().contains("Line 1: "));
        window.prompts.scenarioPath = [&] { return scenarioDir.filePath("missing.txt"); };
        check->trigger();
        CHECK_EQ(errors.size(), qsizetype{2});
        window.prompts.scenarioPath = [] { return QString(); };
        check->trigger();
        CHECK_EQ(errors.size(), qsizetype{2});
        CHECK_EQ(reports.size(), qsizetype{2});
    }
    return test::finish("interface_ui");
}
