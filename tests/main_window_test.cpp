// SPDX-License-Identifier: GPL-3.0-only
#include "app/main_window.hpp"
#include "app/png_export_dialog.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QImage>
#include <QSpinBox>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QWheelEvent>

using namespace eggbert;
using namespace eggbert::test;

namespace {

QByteArray raw(const Bytes &bytes) {
    return QByteArray(reinterpret_cast<const char *>(bytes.data()),
                      static_cast<qsizetype>(bytes.size()));
}

void writeRaw(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    CHECK_EQ(file.write(bytes), bytes.size());
}

QByteArray readRaw(const QString &path) {
    QFile file(path);
    CHECK(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

std::vector<qsizetype> diffPositions(const QByteArray &left, const QByteArray &right) {
    std::vector<qsizetype> positions;
    CHECK_EQ(left.size(), right.size());
    for (qsizetype index = 0; index < std::min(left.size(), right.size()); ++index) {
        if (left[index] != right[index]) {
            positions.push_back(index);
        }
    }
    return positions;
}

QPoint cellCentre(const WorldCanvas *canvas, int x, int y) {
    const auto &transform = canvas->transform();
    return QPoint(static_cast<int>(transform.toScreenX(x * 64 + 32)),
                  static_cast<int>(transform.toScreenY(y * 64 + 32)));
}

QByteArray current(const DocumentTab *tab) {
    return toByteArray(tab->document().currentBytes());
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setOrganizationName("eggbert-editor-tests");
    QApplication::setApplicationName("main-window-test");
    QSettings().clear();

    QTemporaryDir dir;
    CHECK(dir.isValid());
    const auto input = raw(blankLargeWorld(fxRevision13, 3));
    const auto path = dir.filePath("world.blp");
    writeRaw(path, input);
    const auto oldInput = raw(patternedWorld(fxRevision11));
    const auto oldPath = dir.filePath("old.blp");
    writeRaw(oldPath, oldInput);

    MainWindow window;
    QStringList errors;
    auto unsavedAnswer = UnsavedChoice::cancel;
    auto conflictAnswer = ConflictChoice::cancel;
    int conflictPrompts = 0;
    QString saveAsAnswer;
    bool revertAnswer = true;
    window.prompts.error = [&](const QString &, const QString &text) { errors << text; };
    window.prompts.unsavedChanges = [&](DocumentTab *) { return unsavedAnswer; };
    window.prompts.saveConflict = [&](DocumentTab *, const SaveResult &) {
        ++conflictPrompts;
        return conflictAnswer;
    };
    window.prompts.saveAsPath = [&](const QString &) { return saveAsAnswer; };
    window.prompts.confirmRevert = [&](DocumentTab *) { return revertAnswer; };
    window.prompts.openPaths = [&] { return QStringList{path}; };
    window.resize(1000, 800);
    window.show();
    CHECK(QTest::qWaitForWindowExposed(&window));

    // Without a document, document commands are disabled.
    CHECK_EQ(window.tabCount(), 0);
    CHECK(!window.action("fileSave")->isEnabled());
    CHECK(!window.action("editUndo")->isEnabled());
    CHECK(!window.action("toolPencil")->isEnabled());
    CHECK(window.action("fileOpen")->isEnabled());

    // Open through the action; opening the same file again reuses the tab.
    window.action("fileOpen")->trigger();
    CHECK_EQ(window.tabCount(), 1);
    auto *tab = window.currentTab();
    CHECK(tab != nullptr);
    CHECK(window.openPath(path) == tab);
    CHECK_EQ(window.tabCount(), 1);
    CHECK_EQ(tab->title(), QString("world.blp"));
    CHECK(window.windowTitle().startsWith("world.blp"));
    CHECK(window.recentFiles().contains(tab->file().path()));
    CHECK(window.action("fileSave")->isEnabled());
    CHECK(window.action("toolPencil")->isEnabled());
    CHECK(!window.action("editUndo")->isEnabled());
    CHECK(MainWindow::propertiesText(*tab).contains("Revision field: 1.3"));
    CHECK(MainWindow::propertiesText(*tab).contains("Surplus: 3 bytes"));
    CHECK(window.openPath(dir.filePath("missing.blp")) == nullptr);
    CHECK_EQ(errors.size(), qsizetype{1});
    QApplication::processEvents();

    auto *canvas = tab->canvas();
    // Browse tool: a click edits nothing.
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 3, 70));
    CHECK(current(tab) == input);

    // Pencil: one click changes exactly the two bytes of that cell.
    window.action("toolPencil")->trigger();
    CHECK(tab->tool() == Tool::pencil);
    window.findChild<QSpinBox *>("tileSpin")->setValue(77);
    CHECK_EQ(tab->tileId(), std::int16_t{77});
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 3, 70));
    {
        const auto positions = diffPositions(current(tab), input);
        CHECK(!positions.empty());
        CHECK(std::ranges::all_of(positions, [](qsizetype position) {
            return position == 1608 || position == 1609; // 868 + 200*3 + 2*70
        }));
        CHECK_EQ(*tab->document().view().foreground()->cell(3, 70), std::int16_t{77});
        CHECK_EQ(*tab->document().view().foreground()->cell(70, 3), std::int16_t{-1});
    }
    CHECK_EQ(tab->title(), QString("world.blp*"));
    CHECK(window.action("editUndo")->isEnabled());
    CHECK(window.action("editUndo")->text().contains("Pencil"));
    CHECK(window.action("fileSaveAll")->isEnabled());

    // A drag over several cells is one undo step.
    QTest::mousePress(canvas, Qt::LeftButton, {}, cellCentre(canvas, 10, 20));
    for (int x = 11; x <= 13; ++x) {
        QTest::mouseMove(canvas, cellCentre(canvas, x, 20));
    }
    QTest::mouseRelease(canvas, Qt::LeftButton, {}, cellCentre(canvas, 13, 20));
    CHECK_EQ(tab->document().historySize(), std::size_t{2});
    for (int x = 10; x <= 13; ++x) {
        CHECK_EQ(*tab->document().view().foreground()->cell(x, 20), std::int16_t{77});
    }
    window.action("editUndo")->trigger();
    for (int x = 10; x <= 13; ++x) {
        CHECK_EQ(*tab->document().view().foreground()->cell(x, 20), std::int16_t{-1});
    }
    CHECK(window.action("editRedo")->isEnabled());
    // A separate second drag is a separate step.
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 30, 30));
    CHECK_EQ(tab->document().historySize(), std::size_t{2});
    CHECK(!window.action("editRedo")->isEnabled());
    window.action("editUndo")->trigger();

    // The bigDecor layer can be selected and edited separately.
    auto *layerCombo = window.findChild<QComboBox *>("layerCombo");
    CHECK_EQ(layerCombo->count(), 2);
    layerCombo->setCurrentIndex(1);
    CHECK(tab->activeLayer() == SectionKind::bigDecor);
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 50, 60));
    CHECK_EQ(*tab->document().view().bigDecor()->cell(50, 60), std::int16_t{77});
    CHECK_EQ(*tab->document().view().foreground()->cell(50, 60), std::int16_t{-1});
    window.action("editUndo")->trigger();
    layerCombo->setCurrentIndex(0);

    // Keyboard: move the cursor and apply the tool without a mouse.
    canvas->setFocus();
    canvas->setCursorCell({5, 5});
    QTest::keyClick(canvas, Qt::Key_Right);
    QTest::keyClick(canvas, Qt::Key_Down);
    CHECK((canvas->cursorCell() == CellPos{6, 6}));
    QTest::keyClick(canvas, Qt::Key_Space);
    CHECK_EQ(*tab->document().view().foreground()->cell(6, 6), std::int16_t{77});
    window.action("editUndo")->trigger();
    QTest::keyClick(canvas, Qt::Key_Left, Qt::ShiftModifier);
    CHECK((canvas->cursorCell() == CellPos{0, 6}));

    // Save through the action: only the pencilled cell differs on disk.
    window.action("fileSave")->trigger();
    CHECK_EQ(tab->title(), QString("world.blp"));
    {
        const auto positions = diffPositions(readRaw(path), input);
        CHECK(std::ranges::all_of(positions, [](qsizetype position) {
            return position == 1608 || position == 1609;
        }));
        CHECK(!positions.empty());
    }
    CHECK(!window.action("fileSaveAll")->isEnabled());
    const auto saved = readRaw(path);

    // External change: Save asks, cancel writes nothing.
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 40, 40));
    CHECK(tab->document().isModified());
    auto external = saved;
    external[30'000] = 'X';
    writeRaw(path, external);
    conflictAnswer = ConflictChoice::cancel;
    CHECK(!window.saveTab(tab));
    CHECK_EQ(conflictPrompts, 1);
    CHECK(readRaw(path) == external);
    CHECK(tab->document().isModified());
    // Save As keeps the user's version elsewhere and leaves the conflict file.
    conflictAnswer = ConflictChoice::saveAs;
    saveAsAnswer = dir.filePath("mine.blp");
    CHECK(window.saveTab(tab));
    CHECK(readRaw(path) == external);
    CHECK(readRaw(saveAsAnswer) == current(tab));
    CHECK_EQ(tab->title(), QString("mine.blp"));
    CHECK(!tab->document().isModified());

    // Reload on conflict replaces the document with the disk version.
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 41, 41));
    auto external2 = readRaw(saveAsAnswer);
    external2[30'002] = 'Y';
    writeRaw(saveAsAnswer, external2);
    conflictAnswer = ConflictChoice::reload;
    CHECK(!window.saveTab(tab));
    CHECK(current(tab) == external2);
    CHECK(!tab->document().isModified());
    CHECK(readRaw(saveAsAnswer) == external2);

    // Revert: confirmation refused keeps the edit; accepted reloads.
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 42, 42));
    revertAnswer = false;
    CHECK(!window.revertTab(tab));
    CHECK(tab->document().isModified());
    revertAnswer = true;
    CHECK(window.revertTab(tab));
    CHECK(current(tab) == external2);
    CHECK(!tab->document().canUndo());

    // Closing with unsaved changes: cancel keeps the tab, discard closes it
    // without writing.
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 43, 43));
    unsavedAnswer = UnsavedChoice::cancel;
    CHECK(!window.closeTab(tab));
    CHECK_EQ(window.tabCount(), 1);
    unsavedAnswer = UnsavedChoice::discard;
    CHECK(window.closeTab(tab));
    CHECK_EQ(window.tabCount(), 0);
    CHECK(readRaw(saveAsAnswer) == external2);
    QApplication::processEvents();

    // A read-only revision opens, shows why, and refuses every edit path.
    auto *old = window.openPath(oldPath);
    CHECK(old != nullptr);
    CHECK(!window.action("toolPencil")->isEnabled());
    CHECK(!window.action("fileSave")->isEnabled());
    CHECK(window.action("fileSaveAs")->isEnabled());
    QString lastStatus;
    QObject::connect(old, &DocumentTab::statusMessage,
                     [&](const QString &message) { lastStatus = message; });
    old->setTool(Tool::pencil);
    old->setTileId(5);
    CHECK(!old->pencilAt({1, 1}, 0));
    CHECK(lastStatus.contains("read-only"));
    QTest::mouseClick(old->canvas(), Qt::LeftButton, {}, cellCentre(old->canvas(), 2, 2));
    CHECK(current(old) == oldInput);
    CHECK(!old->document().isModified());
    saveAsAnswer = dir.filePath("old-copy.blp");
    CHECK(window.saveTabAs(old));
    CHECK(readRaw(saveAsAnswer) == oldInput);
    CHECK(readRaw(oldPath) == oldInput);

    auto *secondOpen = window.openPath(oldPath);
    CHECK(secondOpen != old);
    const auto target = old->file().path();
    saveAsAnswer = target;
    CHECK(!window.saveTabAs(secondOpen));
    CHECK(!window.exportPng(secondOpen, PngExportOptions{}, target));
    CHECK(readRaw(target) == oldInput);
    CHECK(window.currentTab() == old);
    CHECK(window.closeTab(secondOpen));
    // View commands: zoom, fit and visibility toggles act on the canvas and
    // never dirty the document.
    auto *oldCanvas = old->canvas();
    window.action("viewFitWorld")->trigger();
    const auto fitted = oldCanvas->zoom();
    window.action("viewZoomIn")->trigger();
    CHECK(oldCanvas->zoom() > fitted);
    window.action("viewActualPixels")->trigger();
    CHECK_EQ(oldCanvas->zoom(), 1.0);
    window.action("viewZoomOut")->trigger();
    CHECK(oldCanvas->zoom() < 1.0);
    {
        // Ctrl+wheel zooms about the pointer.
        const QPointF anchor(300, 200);
        const auto worldX = oldCanvas->transform().toWorldX(anchor.x());
        const auto worldY = oldCanvas->transform().toWorldY(anchor.y());
        QWheelEvent wheel(anchor, oldCanvas->mapToGlobal(anchor), QPoint(), QPoint(0, 240),
                          Qt::NoButton, Qt::ControlModifier, Qt::NoScrollPhase, false);
        const auto before = oldCanvas->zoom();
        QApplication::sendEvent(oldCanvas, &wheel);
        CHECK(oldCanvas->zoom() > before);
        CHECK(std::abs(oldCanvas->transform().toWorldX(anchor.x()) - worldX) < 1e-6);
        CHECK(std::abs(oldCanvas->transform().toWorldY(anchor.y()) - worldY) < 1e-6);
    }
    CHECK(!oldCanvas->renderOptions().showGrid);
    window.action("viewGrid")->trigger();
    CHECK(oldCanvas->renderOptions().showGrid);
    window.action("viewForeground")->trigger();
    CHECK(!oldCanvas->renderOptions().showForeground);
    CHECK(!old->document().isModified());

    // Profile choice is per tab and never changes bytes.
    auto *profileCombo = window.findChild<QComboBox *>("profileCombo");
    profileCombo->setCurrentIndex(profileCombo->findData(static_cast<int>(GameProfile::speedyBlupi2)));
    CHECK(old->profile() == GameProfile::speedyBlupi2);
    CHECK(current(old) == oldInput);

    // PNG export through the File menu: writes the image, reports failure,
    // never touches the document (also for a read-only one).
    {
        PngExportOptions exportOptions;
        exportOptions.pixelsPerCell = 4;
        const auto pngPath = dir.filePath("export.png");
        std::optional<std::pair<PngExportOptions, QString>> exportAnswer;
        int exportPrompts = 0;
        window.prompts.exportPng = [&](DocumentTab *) {
            ++exportPrompts;
            return exportAnswer;
        };
        CHECK(window.action("fileExportPng")->isEnabled());
        window.action("fileExportPng")->trigger(); // cancelled
        CHECK_EQ(exportPrompts, 1);
        CHECK(!QFile::exists(pngPath));
        exportAnswer = std::pair{exportOptions, pngPath};
        window.action("fileExportPng")->trigger();
        const QImage exported(pngPath);
        CHECK_EQ(exported.size(), QSize(400, 400));
        // Same pixels as the renderer (the loaded PNG only differs in its
        // in-memory pixel format).
        CHECK(exported.convertToFormat(QImage::Format_ARGB32_Premultiplied) ==
              renderWorldForExport(old->document().view(), exportOptions));
        const auto errorsBefore = errors.size();
        CHECK(!window.exportPng(old, exportOptions, pngPath + "/nope.png"));
        CHECK_EQ(errors.size(), errorsBefore + 1);
        CHECK(!old->document().isModified());
        CHECK(current(old) == oldInput);

        PngExportDialog dialog(old->canvas()->renderOptions());
        CHECK(dialog.estimateText().contains("6400 x 6400"));
        CHECK(dialog.estimateText().contains("about 156.")); // 163,840,000 bytes
        dialog.findChild<QComboBox *>("pngScaleCombo")->setCurrentIndex(3);
        CHECK(dialog.estimateText().contains("800 x 800"));
        CHECK_EQ(dialog.options().pixelsPerCell, 8);
        CHECK(!dialog.options().render.showForeground); // follows the canvas view
        dialog.findChild<QCheckBox *>("pngTransparent")->setChecked(true);
        CHECK(dialog.options().render.transparentBackground);
    }

    // Closing the window with only clean tabs succeeds.
    CHECK(window.close());
    CHECK_EQ(window.tabCount(), 0);
    return test::finish("main_window");
}
