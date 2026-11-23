// SPDX-License-Identifier: GPL-3.0-only
#include "app/main_window.hpp"
#include "app/minimap_widget.hpp"
#include "app/statistics_panel.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

#include <cmath>

using namespace eggbert;
using namespace eggbert::test;

namespace {

bool near(double left, double right, double tolerance = 1.5) {
    return std::abs(left - right) <= tolerance;
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setOrganizationName("eggbert-editor-tests");
    QApplication::setApplicationName("docks-test");
    QSettings().clear();

    auto bytes = blankWorld(fxRevision13);
    put16(bytes, fxCellOffset(fxForegroundOffset, 3, 70), 5);
    put16(bytes, fxCellOffset(fxForegroundOffset, 4, 70), 5);
    put16(bytes, fxCellOffset(fxForegroundOffset, 90, 2), 9);
    put16(bytes, fxCellOffset(fxRevision13.bigDecorOffset, 60, 60), 21);
    const auto slot = fxSlotOffset(fxRevision13, 7);
    put16(bytes, slot, 12);
    put32(bytes, slot + 12, 80 * 64);
    put32(bytes, slot + 16, 50 * 64);
    put32(bytes, slot + 20, 80 * 64);
    put32(bytes, slot + 24, 50 * 64);
    put32(bytes, slot + 28, 80 * 64);
    put32(bytes, slot + 32, 50 * 64);
    put16(bytes, slot + 42, 1);
    for (int index = 0; index < 4; ++index) {
        put32(bytes, 328 + 8 * index, 20 * 64);
        put32(bytes, 332 + 8 * index, 30 * 64);
    }
    QTemporaryDir dir;
    const auto path = dir.filePath("stats.blp");
    {
        QFile file(path);
        CHECK(file.open(QIODevice::WriteOnly));
        file.write(reinterpret_cast<const char *>(bytes.data()),
                   static_cast<qsizetype>(bytes.size()));
    }

    MainWindow window;
    window.prompts.error = [](const QString &, const QString &) {};
    window.resize(1200, 800);
    window.show();
    CHECK(QTest::qWaitForWindowExposed(&window));

    auto *minimap = window.minimap();
    auto *statistics = window.statistics();
    CHECK(minimap != nullptr && statistics != nullptr);
    CHECK(minimap->viewportRect().isEmpty()); // no document yet
    CHECK_EQ(statistics->tree()->topLevelItemCount(), 0);

    auto *tab = window.openPath(path);
    CHECK(tab != nullptr);
    QApplication::processEvents();
    auto *canvas = tab->canvas();

    // Minimap: the outlined viewport is the canvas's visible world rectangle
    // scaled into the minimap's world area.
    canvas->actualPixels();
    canvas->centerOnCell({50, 50});
    QApplication::processEvents();
    {
        const auto area = minimap->worldArea();
        const auto scale = area.width() / 6400.0;
        const auto visible = canvas->visibleWorldRect();
        const auto viewport = minimap->viewportRect();
        CHECK(near(viewport.left(), area.left() + visible.left() * scale, 0.01));
        CHECK(near(viewport.top(), area.top() + visible.top() * scale, 0.01));
        CHECK(near(viewport.width(), visible.width() * scale, 0.01));
        CHECK(near(viewport.height(), visible.height() * scale, 0.01));
        CHECK(near(area.width(), area.height(), 0.01));
        // World position mapping is the inverse of the scale.
        const auto world = minimap->worldAt(QPointF(area.left() + area.width() * 0.25,
                                                    area.top() + area.height() * 0.75));
        CHECK(near(world.x(), 1600, 0.01));
        CHECK(near(world.y(), 4800, 0.01));
    }
    // Clicking the minimap centres the canvas on that world position.
    {
        const auto area = minimap->worldArea();
        const QPoint click(static_cast<int>(area.left() + area.width() * 0.25),
                           static_cast<int>(area.top() + area.height() * 0.75));
        const auto target = minimap->worldAt(click);
        QTest::mouseClick(minimap, Qt::LeftButton, {}, click);
        const auto visible = canvas->visibleWorldRect();
        CHECK(near(visible.center().x(), target.x()));
        CHECK(near(visible.center().y(), target.y()));
        // Dragging keeps panning.
        const QPoint dragTo(static_cast<int>(area.left() + area.width() * 0.5),
                            static_cast<int>(area.top() + area.height() * 0.5));
        QTest::mousePress(minimap, Qt::LeftButton, {}, click);
        QTest::mouseMove(minimap, dragTo);
        QTest::mouseRelease(minimap, Qt::LeftButton, {}, dragTo);
        CHECK(near(canvas->visibleWorldRect().center().x(), minimap->worldAt(dragTo).x()));
        CHECK(near(canvas->visibleWorldRect().center().y(), minimap->worldAt(dragTo).y()));
    }
    // Minimap content: foreground cell visible in composite and foreground
    // modes, hidden in bigDecor-only mode.
    {
        const auto area = minimap->worldArea();
        // First pixel of a cell exactly as the renderer snaps it: the image
        // starts at the integer world area and cell x begins at
        // floor(x * scale).
        const auto origin = area.toRect().topLeft();
        const auto scale = area.toRect().width() / 100.0;
        const auto cellPoint = [&](int x, int y) {
            return origin + QPoint(static_cast<int>(std::floor(x * scale)),
                                   static_cast<int>(std::floor(y * scale)));
        };
        canvas->fitWorld(); // viewport outline moves to the border
        QApplication::processEvents();
        const auto fg = schematicTileColor(SectionKind::foreground, 9);
        const auto background = schematicBackgroundColor();
        CHECK(minimap->grab().toImage().pixelColor(cellPoint(90, 2)) == fg);
        CHECK(minimap->grab().toImage().pixelColor(cellPoint(2, 90)) == background);
        auto *modeCombo = window.findChild<QComboBox *>("minimapModeCombo");
        modeCombo->setCurrentIndex(2);
        CHECK(minimap->mode() == MinimapWidget::Mode::bigDecor);
        CHECK(minimap->grab().toImage().pixelColor(cellPoint(90, 2)) == background);
        CHECK(minimap->grab().toImage().pixelColor(cellPoint(60, 60)) ==
              schematicTileColor(SectionKind::bigDecor, 21));
        modeCombo->setCurrentIndex(0);
    }

    // Statistics rows show the headless numbers.
    const auto stats = computeWorldStats(tab->document().view());
    CHECK_EQ(statistics->valueOf("Size"), QString("50468 bytes"));
    // In a narrow dock the value stays whole and the name gives way; the
    // tooltip of a row says both in full. Checked on a panel of its own, so
    // that its width is the test's and not a dock layout's.
    {
        StatisticsPanel own;
        own.setTab(tab);
        own.resize(300, 500);
        own.show();
        QCoreApplication::processEvents();
        auto *tree = own.findChild<QTreeWidget *>("statisticsTree");
        CHECK(tree != nullptr);
        const auto metrics = tree->fontMetrics();
        QTreeWidgetItem *sizeRow = nullptr;
        for (QTreeWidgetItemIterator item(tree); *item != nullptr; ++item) {
            if ((*item)->text(0) == "Size") {
                sizeRow = *item;
            }
            CHECK(tree->columnWidth(1) >= metrics.horizontalAdvance((*item)->text(1)));
        }
        CHECK(sizeRow != nullptr && sizeRow->toolTip(0) == "Size: 50468 bytes");
        CHECK(sizeRow != nullptr && sizeRow->toolTip(1) == "Size: 50468 bytes");
        // Both columns fit the table: nothing is pushed out of view.
        CHECK(tree->columnWidth(0) + tree->columnWidth(1) <= tree->viewport()->width());
    }
    CHECK_EQ(statistics->valueOf("Revision field"), QString("1.3"));
    CHECK_EQ(statistics->valueOf("Occupied cells"), QString::number(stats.foreground.occupied));
    CHECK_EQ(statistics->valueOf("Occupied cells"), QString("3"));
    CHECK_EQ(statistics->valueOf("Active (type not 0)"), QString("1"));
    CHECK_EQ(statistics->valueOf("Free (type 0)"), QString("199"));
    CHECK_EQ(statistics->valueOf("Value 5"), QString("2 cells"));
    CHECK_EQ(statistics->valueOf("Type 12"), QString("1"));
    CHECK(statistics->valueOf("Unsaved byte ranges").startsWith("0 bytes"));
    CHECK(statistics->valueOf("Start 1").contains("(1280, 1920)"));

    // Activating a value row selects the value for the pencil and marks cells.
    statistics->tree()->itemActivated(statistics->findRow("Value 5"), 0);
    CHECK_EQ(tab->tileId(), std::int16_t{5});
    CHECK(tab->activeLayer() == SectionKind::foreground);
    CHECK_EQ(canvas->highlightedCells().size(), std::size_t{2});
    CHECK((canvas->highlightedCells().front() == CellPos{3, 70}));
    CHECK((canvas->cursorCell() == CellPos{3, 70}));
    statistics->tree()->itemActivated(statistics->findRow("Value 21"), 0);
    CHECK(tab->activeLayer() == SectionKind::bigDecor);
    CHECK_EQ(tab->tileId(), std::int16_t{21});
    CHECK_EQ(canvas->highlightedCells().size(), std::size_t{1});
    // Object rows mark slots and move the view to the object.
    canvas->actualPixels();
    statistics->tree()->itemActivated(statistics->findRow("Type 12"), 0);
    CHECK((canvas->highlightedSlots() == std::vector<std::size_t>{7}));
    CHECK(canvas->visibleWorldRect().contains(QPointF(80 * 64 + 10, 50 * 64 + 10)));
    statistics->tree()->itemActivated(statistics->findRow("Start 1"), 0);
    CHECK(canvas->visibleWorldRect().contains(QPointF(20 * 64 + 30, 30 * 64 + 30)));
    // Locate commands report counts; none of this dirties the document.
    CHECK_EQ(window.locateTileValue(SectionKind::foreground, 123), std::size_t{0});
    CHECK_EQ(window.locateObjects(ObjectKey::channel, 1), std::size_t{1});
    CHECK(!window.locateStart(4));
    CHECK(window.locateOffsetRange(1608, 2));
    CHECK((canvas->cursorCell() == CellPos{3, 70}));
    CHECK(window.locateOffsetRange(222, 2));
    CHECK(!window.locateOffsetRange(60'000, 1));
    CHECK(!tab->document().isModified());
    CHECK_EQ(tab->document().historySize(), std::size_t{0});

    // After an edit the statistics follow the document and list the range.
    tab->setActiveLayer(SectionKind::foreground);
    tab->setTileId(5);
    CHECK(tab->pencilAt({10, 10}, 0));
    CHECK_EQ(statistics->valueOf("Occupied cells"), QString("4"));
    CHECK_EQ(statistics->valueOf("Value 5"), QString("3 cells"));
    CHECK(statistics->valueOf("Unsaved byte ranges").contains("1 range"));
    CHECK(statistics->findRow("Offset 2888") != nullptr); // 868 + 200*10 + 2*10
    CHECK(canvas->highlightedCells().empty()); // stale marks are dropped
    statistics->tree()->itemActivated(statistics->findRow("Offset 2888"), 0);
    CHECK((canvas->cursorCell() == CellPos{10, 10}));
    tab->undo();
    CHECK_EQ(statistics->valueOf("Occupied cells"), QString("3"));

    // Dock toggles and Reset Layout.
    auto *minimapDock = window.findChild<QDockWidget *>("minimapDock");
    auto *statisticsDock = window.findChild<QDockWidget *>("statisticsDock");
    window.action("viewMinimap")->trigger();
    CHECK(!minimapDock->isVisible());
    window.action("viewStatistics")->trigger();
    CHECK(!statisticsDock->isVisible());
    window.action("viewResetLayout")->trigger();
    CHECK(minimapDock->isVisible());
    CHECK(statisticsDock->isVisible());

    // Closing the tab empties both docks.
    CHECK(window.closeTab(tab));
    QApplication::processEvents();
    CHECK_EQ(statistics->tree()->topLevelItemCount(), 0);
    CHECK(minimap->viewportRect().isEmpty());
    return test::finish("docks");
}
