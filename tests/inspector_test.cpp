// SPDX-License-Identifier: GPL-3.0-only
#include "app/inspector_panel.hpp"
#include "app/main_window.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace eggbert;
using namespace eggbert::test;

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setOrganizationName("eggbert-editor-tests");
    QApplication::setApplicationName("inspector-test");
    QSettings().clear();

    auto bytes = blankLargeWorld(fxRevision13, 6);
    put16(bytes, fxCellOffset(fxForegroundOffset, 3, 70), 0x1234);
    put16(bytes, 222, -3);
    putText(bytes, 376, "Name");
    const auto slot = fxSlotOffset(fxRevision13, 137);
    put16(bytes, slot, 5);
    put32(bytes, slot + 12, 80 * 64);
    put32(bytes, slot + 16, 20 * 64);
    put32(bytes, slot + 28, 80 * 64);
    put32(bytes, slot + 32, 20 * 64);
    put16(bytes, slot + 42, 10);
    put16(bytes, slot + 44, 311);
    QTemporaryDir dir;
    const auto path = dir.filePath("inspect.blp");
    {
        QFile file(path);
        CHECK(file.open(QIODevice::WriteOnly));
        file.write(reinterpret_cast<const char *>(bytes.data()),
                   static_cast<qsizetype>(bytes.size()));
    }
    auto cut = bytes;
    cut.resize(1000);
    const auto cutPath = dir.filePath("cut.blp");
    {
        QFile file(cutPath);
        CHECK(file.open(QIODevice::WriteOnly));
        file.write(reinterpret_cast<const char *>(cut.data()), static_cast<qsizetype>(cut.size()));
    }

    MainWindow window;
    window.prompts.error = [](const QString &, const QString &) {};
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    window.resize(1300, 950);
    window.show();
    CHECK(QTest::qWaitForWindowExposed(&window));

    auto *inspector = window.inspector();
    auto *dock = window.findChild<QDockWidget *>("inspectorDock");
    CHECK(!dock->isVisible());
    window.action("viewInspector")->trigger();
    CHECK(dock->isVisible());
    CHECK(inspector->detailsText().isEmpty());

    auto *tab = window.openPath(path);
    CHECK(tab != nullptr);
    QApplication::processEvents();
    auto *view = inspector->currentView();
    auto *canvas = tab->canvas();

    // The whole file is addressable but only visible rows are laid out.
    CHECK_EQ(view->bytes().size(), std::size_t{50'474});
    CHECK_EQ(view->bytesPerRow(), 16);
    CHECK_EQ(view->rowCount(), std::size_t{3155}); // ceil(50474 / 16)
    CHECK(view->visibleRowCount() < 200);
    CHECK_EQ(view->verticalScrollBar()->maximum(),
             static_cast<int>(view->rowCount()) - view->visibleRowCount());

    // Jump to an offset: details show label, calculation and decoded value.
    CHECK(inspector->goToText("1609"));
    CHECK_EQ(inspector->cursorOffset(), std::size_t{1609});
    {
        const auto details = inspector->detailsText();
        CHECK(details.contains("Offset 1609 (0x649) - foreground[3][70] - offset in section 741"));
        CHECK(details.contains("868 + 2 x (100 x 3 + 70) = 1608"));
        CHECK(details.contains("Value at 1608, 2 byte(s) little-endian: signed 4660, unsigned "
                               "4660, bytes 34 12"));
        CHECK(details.contains("Original: signed 4660"));
        CHECK(!details.contains("differs from the original"));
        CHECK(details.contains("revision field 1.3"));
    }
    // The cursor row scrolled into view and the canvas followed.
    CHECK(view->firstVisibleRow() <= 1609 / 16);
    CHECK(view->firstVisibleRow() + static_cast<std::size_t>(view->visibleRowCount()) > 1609 / 16);
    CHECK((canvas->cursorCell() == CellPos{3, 70}));

    // Other jump forms.
    CHECK(inspector->goToText("0x648"));
    CHECK_EQ(inspector->cursorOffset(), std::size_t{1608});
    CHECK(inspector->goToText("cell 99, 99"));
    CHECK_EQ(view->selection().offset, std::size_t{20'866});
    CHECK_EQ(view->selection().size, std::size_t{2});
    CHECK(inspector->goToText("big 70,3"));
    CHECK_EQ(view->selection().offset, std::size_t{34'874});
    CHECK(inspector->goToText("slot 137"));
    CHECK_EQ(view->selection().offset, slot);
    CHECK_EQ(view->selection().size, std::size_t{48});
    CHECK_EQ(tab->selectedSlot().value(), std::size_t{137}); // canvas side followed
    CHECK(!inspector->goToText("slot 200"));
    CHECK(!inspector->goToText("cell 100,0"));
    CHECK(!inspector->goToText("60000"));
    CHECK(!inspector->goToText("nonsense"));
    CHECK(inspector->goToSection("objects"));
    CHECK_EQ(inspector->cursorOffset(), std::size_t{40'868});
    CHECK(inspector->goToSection("suffix"));
    CHECK_EQ(inspector->cursorOffset(), std::size_t{50'468});
    CHECK(inspector->detailsText().contains("suffix+0"));
    CHECK(!inspector->goToSection("nothing"));
    CHECK_EQ(window.findChild<QComboBox *>("inspectorSectionCombo")->count(), 5);

    // Signed/unsigned header value.
    CHECK(inspector->goToText("222"));
    CHECK(inspector->detailsText().contains("header.music"));
    CHECK(inspector->detailsText().contains("signed -3, unsigned 65533, bytes fd ff"));

    // Search: hex and ASCII, with wrap-around from after the cursor.
    CHECK(inspector->findNext("34 12", false));
    CHECK_EQ(view->selection().offset, std::size_t{1608});
    CHECK_EQ(view->selection().size, std::size_t{2});
    CHECK(inspector->findNext("3412", false)); // only one occurrence: wraps to itself
    CHECK_EQ(view->selection().offset, std::size_t{1608});
    CHECK(inspector->findNext("Name", true));
    CHECK_EQ(view->selection().offset, std::size_t{376});
    CHECK(!inspector->findNext("zz", false));
    CHECK(!inspector->findNext("de ad be ef 01", false));
    CHECK(!inspector->findNext("", true));
    // Through the widgets.
    window.findChild<QLineEdit *>("inspectorSearchEdit")->setText("ZZZZ");
    window.findChild<QComboBox *>("inspectorSearchMode")->setCurrentIndex(1);
    window.findChild<QPushButton *>("inspectorFindButton")->click();
    CHECK_EQ(window.findChild<QLabel *>("inspectorMessage")->text(), QString("Not found."));

    // Copy as hex.
    CHECK(inspector->goToText("cell 3,70"));
    CHECK_EQ(view->selectionAsHex(), QString("34 12"));
    window.findChild<QPushButton *>("inspectorCopyButton")->click();
    CHECK_EQ(QApplication::clipboard()->text(), QString("34 12"));

    // Mouse and keyboard navigation in the hex view.
    view->verticalScrollBar()->setValue(0);
    QApplication::processEvents();
    QTest::mouseClick(view->viewport(), Qt::LeftButton, {}, view->byteRect(37).center());
    CHECK_EQ(inspector->cursorOffset(), std::size_t{37});
    CHECK(inspector->detailsText().contains("header.reserve1[8]"));
    QTest::keyClick(view, Qt::Key_Right);
    CHECK_EQ(inspector->cursorOffset(), std::size_t{38});
    QTest::keyClick(view, Qt::Key_Down);
    CHECK_EQ(inspector->cursorOffset(), std::size_t{54});
    QTest::keyClick(view, Qt::Key_Home);
    CHECK_EQ(inspector->cursorOffset(), std::size_t{48});
    QTest::keyClick(view, Qt::Key_End, Qt::ShiftModifier);
    CHECK_EQ(view->selection().size, std::size_t{16});
    QTest::keyClick(view, Qt::Key_End, Qt::ControlModifier);
    CHECK_EQ(inspector->cursorOffset(), std::size_t{50'473});
    QTest::keyClick(view, Qt::Key_Right); // cannot leave the file
    CHECK_EQ(inspector->cursorOffset(), std::size_t{50'473});
    QTest::keyClick(view, Qt::Key_Home, Qt::ControlModifier);
    CHECK_EQ(inspector->cursorOffset(), std::size_t{0});
    // Typing never edits: there is no editing path.
    QTest::keyClicks(view, "ff00");
    CHECK(!tab->document().isModified());

    // Row width.
    inspector->setBytesPerRow(32);
    CHECK_EQ(view->rowCount(), std::size_t{1578});
    CHECK_EQ(inspector->originalView()->bytesPerRow(), 32);
    inspector->setBytesPerRow(16);

    // Bookmarks.
    CHECK(inspector->bookmarks().empty());
    inspector->goToOffset(1608);
    inspector->toggleBookmark();
    inspector->goToOffset(222);
    inspector->toggleBookmark();
    CHECK((inspector->bookmarks() == std::vector<std::size_t>{222, 1608}));
    auto *bookmarkCombo = window.findChild<QComboBox *>("inspectorBookmarkCombo");
    CHECK_EQ(bookmarkCombo->count(), 2);
    CHECK(bookmarkCombo->itemText(1).contains("foreground[3][70]"));
    inspector->toggleBookmark(); // removes 222
    CHECK((inspector->bookmarks() == std::vector<std::size_t>{1608}));

    // Canvas to inspector: moving the cell cursor and selecting an object.
    canvas->setCursorCell({99, 0});
    CHECK_EQ(view->selection().offset, std::size_t{868 + 19'800});
    tab->setActiveLayer(SectionKind::bigDecor);
    canvas->setCursorCell({0, 99});
    CHECK_EQ(view->selection().offset, std::size_t{20'868 + 198});
    tab->setActiveLayer(SectionKind::foreground);
    tab->setSelectedSlot(std::nullopt);
    tab->setSelectedSlot(137);
    CHECK_EQ(view->selection().offset, slot);
    // Statistics-style location also lands in the inspector.
    CHECK(window.locateOffsetRange(1608, 2));
    CHECK_EQ(view->selection().offset, std::size_t{1608});

    // Nothing so far modified the document.
    CHECK(!tab->document().isModified());
    CHECK_EQ(tab->document().historySize(), std::size_t{0});
    CHECK(std::ranges::equal(tab->document().currentBytes(), bytes));

    // After an edit: changed byte reported, original shown, compare modes.
    tab->setTileId(0x1235);
    CHECK(tab->pencilAt({3, 70}, 0));
    inspector->goToOffset(1608);
    {
        const auto details = inspector->detailsText();
        CHECK(details.contains("signed 4661"));
        CHECK(details.contains("Original: signed 4660"));
        CHECK(details.contains("differs from the original"));
        CHECK(details.contains("unsaved changes"));
    }
    CHECK(!inspector->originalView()->isVisible());
    inspector->setCompare(InspectorPanel::Compare::sideBySide);
    CHECK(inspector->originalView()->isVisible());
    CHECK(inspector->originalView()->source() == HexView::Source::original);
    CHECK_EQ(std::to_integer<int>(inspector->originalView()->bytes()[1608]), 0x34);
    CHECK_EQ(std::to_integer<int>(view->bytes()[1608]), 0x35);
    // The two views move together.
    inspector->goToOffset(40'000);
    CHECK_EQ(inspector->originalView()->cursorOffset(), std::size_t{40'000});
    CHECK_EQ(inspector->originalView()->verticalScrollBar()->value(),
             view->verticalScrollBar()->value());
    inspector->setCompare(InspectorPanel::Compare::original);
    CHECK(!inspector->originalView()->isVisible());
    CHECK_EQ(std::to_integer<int>(view->bytes()[1608]), 0x34);
    inspector->setCompare(InspectorPanel::Compare::current);
    CHECK_EQ(std::to_integer<int>(view->bytes()[1608]), 0x35);
    CHECK_EQ(tab->document().historySize(), std::size_t{1}); // only the pencil edit

    // Revert replaces the document object: the views must follow it rather
    // than keep reading the old one.
    window.prompts.confirmRevert = [](DocumentTab *) { return true; };
    CHECK(window.revertTab(tab));
    CHECK(!tab->document().isModified());
    CHECK(view->bytes().data() == tab->document().currentBytes().data());
    CHECK(inspector->originalView()->bytes().data() == tab->document().originalBytes().data());
    CHECK_EQ(std::to_integer<int>(view->bytes()[1608]), 0x34);
    inspector->goToOffset(1608);
    CHECK(inspector->detailsText().contains("signed 4660"));
    CHECK(!inspector->detailsText().contains("differs from the original"));

    // A truncated file: bytes shown, section list reflects what exists.
    auto *cutTab = window.openPath(cutPath);
    CHECK(cutTab != nullptr);
    QApplication::processEvents();
    CHECK_EQ(view->bytes().size(), std::size_t{1000});
    CHECK_EQ(window.findChild<QComboBox *>("inspectorSectionCombo")->count(), 2);
    CHECK(inspector->bookmarks().empty()); // bookmarks belong to a tab
    CHECK(inspector->goToText("999"));
    CHECK(inspector->detailsText().contains("foreground[0][65]"));
    CHECK(!inspector->goToText("1000"));

    CHECK(window.close());
    return test::finish("inspector");
}
