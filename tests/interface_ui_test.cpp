// SPDX-License-Identifier: GPL-3.0-only
//
// The interface around the documents: welcome page, drag and drop, zoom
// slider, tool pictograms, the game remembered per file and recognised from
// the folder a world is opened from.

#include "app/main_window.hpp"
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
    CHECK(directoryDigest(first.root) == firstDigest);
    CHECK(directoryDigest(second.root) == secondDigest);
    // A Mobile Eggbert text world is described in a report and opens no tab.
    {
        MainWindow window;
        QStringList reports;
        QStringList errors;
        window.prompts.showReport = [&](const QString &title, const QString &text) {
            reports << title + "\n" + text;
        };
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
        CHECK_EQ(reports.size(), qsizetype{1});
        CHECK(reports[0].startsWith("Mobile Eggbert Text World\nworld020.txt"));
        CHECK(reports[0].contains("music 1; region 2"));
        CHECK(reports[0].contains("problem(s):")); // this little file is incomplete
        QFile again(textPath);
        CHECK(again.open(QIODevice::ReadOnly) && again.readAll() == content);
    }
    return test::finish("interface_ui");
}
