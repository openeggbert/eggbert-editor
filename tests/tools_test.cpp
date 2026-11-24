// SPDX-License-Identifier: GPL-3.0-only
#include "app/main_window.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QMenuBar>
#include <QMenu>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace eggbert;
using namespace eggbert::test;

namespace {

void writeWorld(const QString &path, const Bytes &bytes) {
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<qsizetype>(bytes.size()));
}

QPoint cellCentre(const WorldCanvas *canvas, std::size_t x, std::size_t y) {
    const auto &transform = canvas->transform();
    return QPoint(static_cast<int>(transform.toScreenX(static_cast<double>(x) * 64 + 32)),
                  static_cast<int>(transform.toScreenY(static_cast<double>(y) * 64 + 32)));
}

void drag(WorldCanvas *canvas, CellPos from, CellPos to) {
    QTest::mousePress(canvas, Qt::LeftButton, {}, cellCentre(canvas, from.x, from.y));
    QTest::mouseMove(canvas, cellCentre(canvas, to.x, to.y));
    QTest::mouseRelease(canvas, Qt::LeftButton, {}, cellCentre(canvas, to.x, to.y));
}

std::int16_t fg(const DocumentTab *tab, std::size_t x, std::size_t y) {
    return *tab->document().view().foreground()->cell(x, y);
}

std::int16_t big(const DocumentTab *tab, std::size_t x, std::size_t y) {
    return *tab->document().view().bigDecor()->cell(x, y);
}

// Number of foreground cells differing from -1.
std::size_t occupied(const DocumentTab *tab) {
    std::size_t count = 0;
    for (std::size_t x = 0; x < 100; ++x) {
        for (std::size_t y = 0; y < 100; ++y) {
            count += fg(tab, x, y) != -1 ? 1 : 0;
        }
    }
    return count;
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setOrganizationName("eggbert-editor-tests");
    QApplication::setApplicationName("tools-test");
    QSettings().clear();

    QTemporaryDir dir;
    auto bytes = blankLargeWorld(fxRevision13);
    // Opaque data that no tile tool may touch.
    put16(bytes, fxSlotOffset(fxRevision13, 3) + 44, 0x1234);
    put32(bytes, 4, 256);
    const auto path = dir.filePath("tools.blp");
    writeWorld(path, bytes);
    const auto otherPath = dir.filePath("other.blp");
    writeWorld(otherPath, blankWorld(fxRevision12));
    const auto oldPath = dir.filePath("old.blp");
    writeWorld(oldPath, blankWorld(fxRevision11));

    MainWindow window;
    QStringList status;
    int broadPrompts = 0;
    bool broadAnswer = true;
    BroadEditPreview lastPreview;
    std::optional<std::pair<int, int>> replaceAnswer;
    window.prompts.error = [](const QString &, const QString &) {};
    window.prompts.confirmBroadEdit = [&](DocumentTab *, const BroadEditPreview &preview) {
        ++broadPrompts;
        lastPreview = preview;
        return broadAnswer;
    };
    window.prompts.replaceValues = [&](DocumentTab *) { return replaceAnswer; };
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    window.resize(1100, 900);
    window.show();
    CHECK(QTest::qWaitForWindowExposed(&window));

    // Menus appear in the planned order.
    {
        QStringList titles;
        for (const auto *menuAction : window.menuBar()->actions()) {
            titles << menuAction->text().remove('&');
        }
        CHECK((titles == QStringList{"File", "Edit", "View", "World", "Layer", "Object", "Help"}));
    }

    auto *tab = window.openPath(path);
    CHECK(tab != nullptr);
    QObject::connect(tab, &DocumentTab::statusMessage,
                     [&](const QString &message) { status << message; });
    QApplication::processEvents();
    auto *canvas = tab->canvas();
    const auto trigger = [&](const char *name) {
        auto *item = window.action(name);
        CHECK(item != nullptr);
        if (item != nullptr) {
            CHECK(item->isEnabled());
            item->trigger();
        }
    };
    const auto enabled = [&](const char *name) { return window.action(name)->isEnabled(); };

    // Pencil drag with a single far move leaves no gaps.
    trigger("toolPencil");
    tab->setTileId(7);
    drag(canvas, {20, 20}, {26, 23});
    for (const auto &cell : lineCells({20, 20}, {26, 23})) {
        CHECK_EQ(fg(tab, cell.x, cell.y), std::int16_t{7});
    }
    CHECK_EQ(occupied(tab), std::size_t{7});
    CHECK_EQ(tab->document().historySize(), std::size_t{1});

    // Eraser writes -1 into the active layer only.
    tab->setActiveLayer(SectionKind::bigDecor);
    tab->pencilAt({22, 21}, 0);
    CHECK_EQ(big(tab, 22, 21), std::int16_t{7});
    tab->setActiveLayer(SectionKind::foreground);
    trigger("toolEraser");
    CHECK(tab->tool() == Tool::eraser);
    drag(canvas, {21, 20}, {23, 22});
    CHECK_EQ(fg(tab, 22, 21), std::int16_t{-1});
    CHECK_EQ(big(tab, 22, 21), std::int16_t{7}); // other layer untouched
    CHECK_EQ(fg(tab, 20, 20), std::int16_t{7});
    CHECK_EQ(tab->document().undoLabel(), std::string_view("Eraser"));

    // Eyedropper picks the stored value of the active layer.
    trigger("toolEyedropper");
    tab->setTileId(0);
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 26, 23));
    CHECK_EQ(tab->tileId(), std::int16_t{7});
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 60, 60));
    CHECK_EQ(tab->tileId(), std::int16_t{-1});
    CHECK(!tab->document().canRedo());
    const auto historyBeforeLine = tab->document().historySize();

    // Line: preview while dragging, one step on release.
    trigger("toolLine");
    tab->setTileId(9);
    QTest::mousePress(canvas, Qt::LeftButton, {}, cellCentre(canvas, 40, 40));
    QTest::mouseMove(canvas, cellCentre(canvas, 44, 40));
    CHECK_EQ(canvas->previewCells().size(), std::size_t{5});
    CHECK_EQ(fg(tab, 42, 40), std::int16_t{-1}); // nothing written yet
    QTest::mouseRelease(canvas, Qt::LeftButton, {}, cellCentre(canvas, 44, 40));
    CHECK(canvas->previewCells().empty());
    for (std::size_t x = 40; x <= 44; ++x) {
        CHECK_EQ(fg(tab, x, 40), std::int16_t{9});
    }
    CHECK_EQ(fg(tab, 40, 44), std::int16_t{-1});
    CHECK_EQ(tab->document().historySize(), historyBeforeLine + 1);
    CHECK_EQ(tab->document().undoLabel(), std::string_view("Line"));

    // Rectangle below the broad threshold: no prompt.
    trigger("toolRectangle");
    drag(canvas, {50, 50}, {53, 52});
    CHECK_EQ(broadPrompts, 0);
    CHECK_EQ(fg(tab, 53, 52), std::int16_t{9});
    CHECK_EQ(fg(tab, 52, 53), std::int16_t{-1});
    // A broad rectangle asks first; declining changes nothing.
    broadAnswer = false;
    const auto beforeBroad = toByteArray(tab->document().currentBytes());
    drag(canvas, {60, 60}, {69, 69});
    CHECK_EQ(broadPrompts, 1);
    CHECK_EQ(lastPreview.cellsChanged, std::size_t{100});
    CHECK_EQ(lastPreview.bytesChanged, std::size_t{200});
    CHECK_EQ(lastPreview.firstOffset, std::size_t{868 + 200 * 60 + 2 * 60});
    CHECK_EQ(lastPreview.lastOffset, std::size_t{868 + 200 * 69 + 2 * 69 + 1});
    CHECK(toByteArray(tab->document().currentBytes()) == beforeBroad);
    broadAnswer = true;
    drag(canvas, {60, 60}, {69, 69});
    CHECK_EQ(broadPrompts, 2);
    CHECK_EQ(fg(tab, 69, 69), std::int16_t{9});
    CHECK_EQ(tab->document().undoLabel(), std::string_view("Rectangle"));
    trigger("editUndo");
    CHECK_EQ(fg(tab, 69, 69), std::int16_t{-1});

    // Fill: bounded by a box drawn with the tools.
    tab->setTileId(3);
    CHECK(tab->drawLine({80, 10}, {84, 10}));
    CHECK(tab->drawLine({80, 14}, {84, 14}));
    CHECK(tab->drawLine({80, 10}, {80, 14}));
    CHECK(tab->drawLine({84, 10}, {84, 14}));
    trigger("toolFill");
    tab->setTileId(4);
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 82, 12));
    for (std::size_t x = 81; x <= 83; ++x) {
        for (std::size_t y = 11; y <= 13; ++y) {
            CHECK_EQ(fg(tab, x, y), std::int16_t{4});
        }
    }
    CHECK_EQ(fg(tab, 85, 12), std::int16_t{-1});
    CHECK_EQ(fg(tab, 80, 12), std::int16_t{3});
    CHECK_EQ(tab->document().undoLabel(), std::string_view("Fill"));

    // Selection by dragging; edits are limited to it.
    CHECK(!enabled("editCopy"));
    trigger("toolSelect");
    drag(canvas, {40, 40}, {42, 41});
    CHECK_EQ(tab->selection().count(), std::size_t{6});
    CHECK((tab->selection().bounds().value() == CellRect{{40, 40}, {42, 41}}));
    CHECK(canvas->selectionMask() == tab->selection());
    CHECK(enabled("editCopy"));
    const auto historyBeforeLimited = tab->document().historySize();
    tab->setTileId(5);
    CHECK(!tab->pencilAt({45, 45}, 0)); // outside the selection
    CHECK_EQ(fg(tab, 45, 45), std::int16_t{-1});
    CHECK(tab->pencilAt({41, 41}, 0));
    CHECK_EQ(fg(tab, 41, 41), std::int16_t{5});
    CHECK_EQ(tab->document().historySize(), historyBeforeLimited + 1);

    // Copy, cut and paste through the Edit menu.
    trigger("editCopy");
    CHECK_EQ(TileClipboard::shared().patch.width, std::size_t{3});
    CHECK((TileClipboard::shared().patch.values ==
           std::vector<std::int16_t>{9, 9, 9, -1, 5, -1}));
    trigger("editCut");
    CHECK_EQ(fg(tab, 40, 40), std::int16_t{-1});
    CHECK_EQ(fg(tab, 41, 41), std::int16_t{-1});
    CHECK_EQ(fg(tab, 43, 40), std::int16_t{9}); // outside the selection stays
    CHECK_EQ(tab->document().undoLabel(), std::string_view("Cut"));
    canvas->setCursorCell({10, 70});
    CHECK(enabled("editPaste"));
    trigger("editPaste");
    CHECK_EQ(fg(tab, 10, 70), std::int16_t{9});
    CHECK_EQ(fg(tab, 12, 70), std::int16_t{9});
    CHECK_EQ(fg(tab, 11, 71), std::int16_t{5});
    CHECK_EQ(fg(tab, 70, 10), std::int16_t{-1});
    CHECK((tab->selection().bounds().value() == CellRect{{10, 70}, {12, 71}}));
    CHECK_EQ(tab->document().undoLabel(), std::string_view("Paste"));
    // Paste clipped at the map edge reports the clipped cells.
    canvas->setCursorCell({98, 99});
    status.clear();
    trigger("editPaste");
    CHECK_EQ(fg(tab, 98, 99), std::int16_t{9});
    CHECK_EQ(fg(tab, 99, 99), std::int16_t{9});
    CHECK(status.join('\n').contains("4 cell(s) fell outside the map"));
    CHECK_EQ(tab->selection().count(), std::size_t{2});

    // Delete, Select All / None / Invert, Fill Selection.
    trigger("editDelete");
    CHECK_EQ(fg(tab, 98, 99), std::int16_t{-1});
    trigger("editSelectNone");
    CHECK(tab->selection().empty());
    CHECK(!enabled("editDelete"));
    trigger("editSelectAll");
    CHECK_EQ(tab->selection().count(), std::size_t{10'000});
    tab->setSelection(CellMask::fromRect({{0, 0}, {1, 0}}));
    trigger("editSelectInverse");
    CHECK_EQ(tab->selection().count(), std::size_t{9'998});
    CHECK(!tab->selection().test({1, 0}));
    tab->setSelection(CellMask::fromRect({{0, 0}, {1, 1}}));
    tab->setTileId(6);
    trigger("layerFillSelection");
    CHECK_EQ(fg(tab, 1, 1), std::int16_t{6});
    CHECK_EQ(fg(tab, 2, 1), std::int16_t{-1});

    // Replace: inside the selection, then in the whole layer.
    replaceAnswer = std::pair{6, 8};
    tab->setSelection(CellMask::fromRect({{0, 0}, {0, 1}}));
    trigger("layerReplace");
    CHECK_EQ(fg(tab, 0, 0), std::int16_t{8});
    CHECK_EQ(fg(tab, 0, 1), std::int16_t{8});
    CHECK_EQ(fg(tab, 1, 0), std::int16_t{6});
    tab->selectNone();
    replaceAnswer = std::pair{6, 8};
    trigger("layerReplace");
    CHECK_EQ(fg(tab, 1, 0), std::int16_t{8});
    CHECK_EQ(fg(tab, 1, 1), std::int16_t{8});
    replaceAnswer.reset(); // cancelled dialog
    const auto historyBeforeCancel = tab->document().historySize();
    trigger("layerReplace");
    CHECK_EQ(tab->document().historySize(), historyBeforeCancel);
    CHECK_EQ(tab->selectValue(8), std::size_t{4});

    // Flip the selection.
    tab->setTileId(1);
    tab->selectNone();
    CHECK(tab->pencilAt({30, 90}, 0));
    tab->setTileId(2);
    CHECK(tab->pencilAt({32, 91}, 0));
    tab->setSelection(CellMask::fromRect({{30, 90}, {32, 91}}));
    trigger("layerFlipHorizontal");
    CHECK_EQ(fg(tab, 32, 90), std::int16_t{1});
    CHECK_EQ(fg(tab, 30, 91), std::int16_t{2});
    CHECK_EQ(fg(tab, 30, 90), std::int16_t{-1});
    trigger("layerFlipVertical");
    CHECK_EQ(fg(tab, 32, 91), std::int16_t{1});
    CHECK_EQ(fg(tab, 30, 90), std::int16_t{2});
    trigger("editUndo");
    trigger("editUndo");
    CHECK_EQ(fg(tab, 30, 90), std::int16_t{1});
    CHECK_EQ(fg(tab, 32, 91), std::int16_t{2});
    tab->selectNone();

    // Layer lock refuses edits with a reason; unlock restores them.
    window.action("layerLockForeground")->setChecked(true);
    CHECK(tab->isLayerLocked(SectionKind::foreground));
    status.clear();
    CHECK(!tab->pencilAt({5, 50}, 0));
    CHECK(!tab->fillAt({5, 50}));
    CHECK(!tab->drawRectangle({5, 50}, {6, 51}));
    CHECK(status.join('\n').contains("locked"));
    CHECK_EQ(fg(tab, 5, 50), std::int16_t{-1});
    trigger("layerActiveBigDecor");
    CHECK(tab->activeLayer() == SectionKind::bigDecor);
    CHECK(tab->pencilAt({5, 50}, 0)); // the other layer is not locked
    trigger("layerActiveForeground");
    window.action("layerLockForeground")->setChecked(false);
    CHECK(tab->pencilAt({5, 50}, 0));

    // Isolate shows only the active layer and restores the previous view.
    const auto optionsBefore = canvas->renderOptions();
    window.action("layerIsolate")->setChecked(true);
    CHECK(canvas->renderOptions().showForeground);
    CHECK(!canvas->renderOptions().showBigDecor);
    CHECK(!canvas->renderOptions().showObjects);
    window.action("layerIsolate")->setChecked(false);
    CHECK(canvas->renderOptions() == optionsBefore);

    // Find.
    CHECK(window.find({FindQuery::Kind::coordinate, 77, 12}));
    CHECK((canvas->cursorCell() == CellPos{77, 12}));
    CHECK(!window.find({FindQuery::Kind::coordinate, 100, 0}));
    CHECK(!window.find({FindQuery::Kind::coordinate, -1, 0}));
    CHECK(window.find({FindQuery::Kind::tileValue, 9, 0}));
    CHECK(!canvas->highlightedCells().empty());
    CHECK(!window.find({FindQuery::Kind::tileValue, 31'000, 0}));
    CHECK(!window.find({FindQuery::Kind::objectType, 12, 0}));
    CHECK(window.find({FindQuery::Kind::objectSlot, 3, 0}));
    CHECK((canvas->highlightedSlots() == std::vector<std::size_t>{3}));
    CHECK(!window.find({FindQuery::Kind::objectSlot, 200, 0}));

    // Throughout, nothing outside the two tile layers changed.
    {
        const auto current = tab->document().currentBytes();
        bool onlyLayers = true;
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            if ((index < 868 || index >= 40'868) && current[index] != bytes[index]) {
                onlyLayers = false;
            }
        }
        CHECK(onlyLayers);
    }

    // Clipboard across tabs: refused while profiles are unknown or differ,
    // allowed between equal known profiles.
    tab->setSelection(CellMask::fromRect({{40, 40}, {44, 40}}));
    CHECK(tab->copySelection());
    auto *other = window.openPath(otherPath);
    CHECK(other != nullptr);
    QApplication::processEvents();
    CHECK(!other->canPaste());
    CHECK(!enabled("editPaste"));
    CHECK(!other->paste());
    other->setProfile(GameProfile::speedyBlupi2);
    CHECK(!other->canPaste()); // source profile was not chosen
    tab->setProfile(GameProfile::speedyBlupi1);
    CHECK(tab->copySelection());
    CHECK(!other->canPaste()); // different games
    tab->setProfile(GameProfile::speedyBlupi2);
    CHECK(tab->copySelection());
    CHECK(other->canPaste());
    other->canvas()->setCursorCell({1, 2});
    CHECK(other->paste());
    CHECK_EQ(*other->document().view().foreground()->cell(1, 2),
             fg(tab, 40, 40));

    // Read-only documents: editing tools and commands are unavailable.
    auto *old = window.openPath(oldPath);
    CHECK(old != nullptr);
    QApplication::processEvents();
    for (const char *name : {"toolPencil", "toolEraser", "toolLine", "toolRectangle", "toolFill",
                             "editPaste", "layerReplace"}) {
        CHECK(!enabled(name));
    }
    CHECK(enabled("toolSelect"));
    CHECK(enabled("toolEyedropper"));
    old->setSelection(CellMask::fromRect({{0, 0}, {3, 3}}));
    CHECK(enabled("editCopy"));
    CHECK(!enabled("editCut"));
    CHECK(!old->drawLine({0, 0}, {5, 5}));
    CHECK(!old->fillAt({0, 0}));
    CHECK(!old->deleteSelection());
    CHECK(!old->cutSelection());
    CHECK(!old->document().isModified());

    // An overlapping section move preserves slot identity, paths, both layers and starts.
    auto room = blankLargeWorld(fxRevision13);
    auto putTile = [&](std::size_t layer, int x, int value) {
        put16(room, fxCellOffset(layer, x, 8), value);
    };
    putTile(fxForegroundOffset, 8, 31);
    putTile(fxForegroundOffset, 9, 45);
    putTile(fxForegroundOffset, 10, 72);
    putTile(fxRevision13.bigDecorOffset, 8, 20);
    const auto slot = fxSlotOffset(fxRevision13, 7);
    put16(room, slot, 1);
    put16(room, slot + 42, 1);
    put16(room, slot + 44, 29);
    for (int offset : {12, 20, 28}) {
        put32(room, slot + offset, (offset == 20 ? 12 : 8) * 64 + 2);
        put32(room, slot + offset + 4, 8 * 64 + 4);
    }
    // Two overlapping records make it possible to distinguish picking from cycling.
    std::copy_n(room.begin() + slot, 48, room.begin() + fxSlotOffset(fxRevision13, 8));
    put32(room, 328, 8 * 64 + 2);
    put32(room, 332, 8 * 64 + 4);
    const auto roomPath = dir.filePath("room.blp");
    writeWorld(roomPath, room);
    auto *section = window.openPath(roomPath);
    section->setProfile(GameProfile::speedyBlupi2);
    auto *map = section->canvas();
    map->fitCells({4,4}, {16,16});
    section->setTool(Tool::select);
    drag(map, {8,8}, {9,8});
    const auto before = section->selection();
    CHECK_EQ(before.count(), 2u);
    const auto hits = map->hitsAt(cellCentre(map, 8, 8));
    const auto has = [&](SectionKind kind, int value) {
        return std::ranges::any_of(hits, [&](const auto &h) { return h.kind == kind && h.value == value; });
    };
    CHECK(has(SectionKind::foreground, 31) && has(SectionKind::bigDecor, 20));
    CHECK(has(SectionKind::header, 0) && has(SectionKind::objects, 7));
    const auto adjacent = map->hitsAt(cellCentre(map, 9, 8));
    CHECK(std::ranges::any_of(adjacent, [](const auto &h) {
        return h.kind == SectionKind::bigDecor && h.anchor == CellPos{8,8};
    }));
    emit map->customContextMenuRequested(cellCentre(map, 8, 8));
    auto *atPoint = map->findChild<QMenu *>("worldAtPoint");
    CHECK(atPoint != nullptr);
    if (atPoint) {
        for (auto *item : atPoint->actions()) {
            if (item->text().contains("slot 7")) {
                item->trigger();
            }
        }
        CHECK(section->selectedSlot() == 7 && section->tool() == Tool::object);
        atPoint->close();
        drag(map, {8,8}, {8,9});
        CHECK_EQ(section->document().view().objects()->record(7)->posStart.y, 9*64+4);
        CHECK_EQ(section->document().view().objects()->record(8)->posStart.y, 8*64+4);
        CHECK(section->undo());
    }
    section->setTool(Tool::select);
    broadAnswer = false;
    drag(map, {8,8}, {9,8});
    CHECK(!section->document().isModified() && section->selection() == before);
    broadAnswer = true;
    drag(map, {8,8}, {9,8});
    CHECK_EQ(fg(section,8,8), -1);
    CHECK_EQ(fg(section,9,8), 31);
    CHECK_EQ(fg(section,10,8), 45);
    CHECK_EQ(big(section,9,8), 20);
    const auto moved = *section->document().view().objects()->record(7);
    CHECK_EQ(moved.posStart.x, 9*64+2);
    CHECK_EQ(moved.posEnd.x, 13*64+2);
    CHECK_EQ(moved.posCurrent.x, 9*64+2);
    CHECK_EQ(section->document().view().header()->blupiPos(0)->x, 9*64+2);
    CHECK(section->undo());
    CHECK(!section->document().isModified());
    CHECK(section->redo());
    const auto snapshot = toVector(section->document().currentBytes());
    section->setLayerLocked(SectionKind::bigDecor, true);
    CHECK(!section->moveSection({9,8},{10,8}));
    section->setLayerLocked(SectionKind::bigDecor, false);
    CHECK(!section->moveSection({9,8},{99,8}));
    CHECK(!section->moveSection({9,8},{97,8})); // path end would be outside
    CHECK(snapshot == toVector(section->document().currentBytes()));
    CHECK(window.close());
    return test::finish("tools");
}
