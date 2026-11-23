// SPDX-License-Identifier: GPL-3.0-only
#include "app/diagnostics_panel.hpp"
#include "app/inspector_panel.hpp"
#include "app/main_window.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

using namespace eggbert;
using namespace eggbert::test;

namespace {

void writeWorld(const QString &path, const Bytes &bytes) {
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<qsizetype>(bytes.size()));
}

Bytes problemWorld(const FixtureLayout &layout) {
    auto bytes = blankWorld(layout);
    put32(bytes, 212, 100);
    put32(bytes, 216, 100);
    put16(bytes, 222, 1);
    put16(bytes, 224, 40); // region outside both games
    for (int index = 0; index < 4; ++index) {
        put32(bytes, 328 + 8 * index, 194);
        put32(bytes, 332 + 8 * index, 198);
        put32(bytes, 360 + 4 * index, 2);
    }
    putText(bytes, 376, std::string(100, 'T')); // unterminated title
    put16(bytes, fxCellOffset(fxForegroundOffset, 3, 70), 300); // no picture in SB1
    put16(bytes, fxCellOffset(fxForegroundOffset, 4, 70), 300);
    // Slot 7: a type unknown to SB1.
    const auto slot = fxSlotOffset(layout, 7);
    put16(bytes, slot, 150);
    put32(bytes, slot + 12, 30 * 64);
    put32(bytes, slot + 16, 40 * 64);
    put32(bytes, slot + 20, 30 * 64);
    put32(bytes, slot + 24, 40 * 64);
    put32(bytes, slot + 28, 30 * 64);
    put32(bytes, slot + 32, 40 * 64);
    put16(bytes, slot + 42, 10);
    return bytes;
}

int rowOfRule(const DiagnosticsPanel *panel, const char *rule) {
    for (int row = 0; row < panel->tree()->topLevelItemCount(); ++row) {
        if (panel->tree()->topLevelItem(row)->text(1) == QLatin1String(rule)) {
            return row;
        }
    }
    return -1;
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setOrganizationName("eggbert-editor-tests");
    QApplication::setApplicationName("doctor-ui-test");
    QSettings().clear();

    QTemporaryDir dir;
    const auto bytes = problemWorld(fxRevision13);
    const auto path = dir.filePath("problems.blp");
    writeWorld(path, bytes);
    const auto oldPath = dir.filePath("old.blp");
    writeWorld(oldPath, problemWorld(fxRevision11));

    MainWindow window;
    window.prompts.error = [](const QString &, const QString &) {};
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    bool confirmAnswer = false;
    QString confirmText;
    window.prompts.confirmFix = [&](DocumentTab *, const QString &description) {
        confirmText = description;
        return confirmAnswer;
    };
    QString reportTitle;
    QString reportText;
    window.prompts.showReport = [&](const QString &title, const QString &text) {
        reportTitle = title;
        reportText = text;
    };
    window.resize(1300, 950);
    window.show();
    CHECK(QTest::qWaitForWindowExposed(&window));

    auto *panel = window.diagnostics();
    auto *dock = window.findChild<QDockWidget *>("diagnosticsDock");
    CHECK(!dock->isVisible());
    CHECK(!window.action("worldValidate")->isEnabled());

    auto *tab = window.openPath(path);
    CHECK(tab != nullptr);
    QApplication::processEvents();
    window.action("worldValidate")->trigger();
    CHECK(dock->isVisible());

    // Without a profile only profile-free rules report.
    CHECK(panel->report().first("HDR-NAME-UNTERMINATED") != nullptr);
    CHECK(panel->report().first("HDR-REGION-RANGE") == nullptr);
    CHECK(panel->summaryText().contains("game-specific rules were skipped"));
    CHECK(panel->summaryText().contains("does not judge whether a world can be completed"));

    // With a profile the list equals the headless audit.
    tab->setProfile(GameProfile::speedyBlupi1);
    const auto expected = runDoctor(tab->document().view(), GameProfile::speedyBlupi1);
    CHECK_EQ(panel->report().findings.size(), expected.findings.size());
    CHECK_EQ(panel->tree()->topLevelItemCount(), static_cast<int>(expected.findings.size()));
    CHECK(panel->summaryText().startsWith(
        QString("%1 error(s), %2 warning(s), %3 note(s).")
            .arg(expected.count(Severity::error))
            .arg(expected.count(Severity::warning))
            .arg(expected.count(Severity::note))));
    CHECK_EQ(expected.count(Severity::error), std::size_t{1});
    for (const char *rule : {"HDR-NAME-UNTERMINATED", "HDR-REGION-RANGE", "TILE-NOT-DRAWN",
                             "OBJ-TYPE-UNKNOWN"}) {
        CHECK(rowOfRule(panel, rule) >= 0);
    }

    // Severity filters only hide rows.
    panel->setShown(Severity::warning, false);
    CHECK(rowOfRule(panel, "HDR-REGION-RANGE") < 0);
    CHECK(rowOfRule(panel, "HDR-NAME-UNTERMINATED") >= 0);
    CHECK_EQ(panel->report().findings.size(), expected.findings.size());
    panel->setShown(Severity::warning, true);
    panel->setShown(Severity::error, false);
    CHECK(rowOfRule(panel, "HDR-NAME-UNTERMINATED") < 0);
    panel->setShown(Severity::error, true);

    // Activating a tile finding synchronises canvas and inspector.
    {
        const auto row = rowOfRule(panel, "TILE-NOT-DRAWN");
        panel->tree()->itemActivated(panel->tree()->topLevelItem(row), 0);
        CHECK((tab->canvas()->cursorCell() == CellPos{3, 70}));
        CHECK_EQ(tab->canvas()->highlightedCells().size(), std::size_t{2});
        CHECK_EQ(window.inspector()->currentView()->selection().offset, std::size_t{1608});
        CHECK_EQ(window.inspector()->currentView()->selection().size, std::size_t{2});
    }
    // An object finding selects the slot.
    {
        const auto row = rowOfRule(panel, "OBJ-TYPE-UNKNOWN");
        panel->tree()->itemActivated(panel->tree()->topLevelItem(row), 0);
        CHECK_EQ(tab->selectedSlot().value(), std::size_t{7});
        CHECK_EQ(window.inspector()->currentView()->selection().offset,
                 std::size_t{40'868 + 48 * 7});
    }
    // A header finding selects its bytes.
    {
        const auto row = rowOfRule(panel, "HDR-REGION-RANGE");
        panel->tree()->itemActivated(panel->tree()->topLevelItem(row), 0);
        CHECK_EQ(window.inspector()->currentView()->selection().offset, std::size_t{224});
        CHECK_EQ(window.inspector()->currentView()->selection().size, std::size_t{2});
    }
    // None of that changed the document.
    CHECK(!tab->document().isModified());
    CHECK_EQ(tab->document().historySize(), std::size_t{0});

    // The fix: reviewed with its byte diff, declined, then applied as one
    // named undo step.
    auto *fixButton = window.findChild<QPushButton *>("fixButton");
    panel->tree()->setCurrentItem(panel->tree()->topLevelItem(rowOfRule(panel, "TILE-NOT-DRAWN")));
    CHECK(!fixButton->isEnabled()); // this finding has no verified fix
    panel->tree()->setCurrentItem(
        panel->tree()->topLevelItem(rowOfRule(panel, "HDR-NAME-UNTERMINATED")));
    CHECK(fixButton->isEnabled());
    confirmAnswer = false;
    fixButton->click();
    CHECK(confirmText.contains("Terminate Title"));
    CHECK(confirmText.contains("Offset 475, 1 byte(s), header.name[99]: 54 -> 00"));
    CHECK(!tab->document().isModified());
    confirmAnswer = true;
    panel->tree()->setCurrentItem(
        panel->tree()->topLevelItem(rowOfRule(panel, "HDR-NAME-UNTERMINATED")));
    fixButton->click();
    CHECK_EQ(tab->document().historySize(), std::size_t{1});
    CHECK_EQ(tab->document().undoLabel(), std::string_view("Terminate Title"));
    {
        const auto current = tab->document().currentBytes();
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            if (index != 475) {
                CHECK(current[index] == bytes[index]);
            }
        }
        CHECK(current[475] == std::byte{0});
    }
    // The list re-ran by itself: the error is gone, the long-title note is there.
    CHECK(rowOfRule(panel, "HDR-NAME-UNTERMINATED") < 0);
    CHECK(rowOfRule(panel, "HDR-NAME-LONG") >= 0);
    CHECK_EQ(panel->report().count(Severity::error), std::size_t{0});

    // Byte-difference reports.
    window.action("worldCompareOriginal")->trigger();
    CHECK(reportTitle.contains("File as Opened"));
    CHECK(reportText.contains("1 byte(s) in 1 range(s) differ from the file as opened."));
    CHECK(reportText.contains("Offset 475, 1 byte(s), header.name[99]: 54 -> 00"));
    window.action("worldCompareSaved")->trigger();
    CHECK(reportText.contains("differ from the saved file"));

    // Saving runs the audit and reports it without blocking.
    CHECK(window.saveTab(tab));
    CHECK(window.statusBar()->currentMessage().contains("World Doctor: 0 error(s)"));
    window.action("worldCompareSaved")->trigger();
    CHECK(reportText.startsWith("0 byte(s) in 0 range(s)"));

    // Edits rerun the audit: undo brings the error back, a metadata edit
    // removes the region warning.
    window.action("editUndo")->trigger();
    CHECK(rowOfRule(panel, "HDR-NAME-UNTERMINATED") >= 0);
    auto metadata = readMetadata(tab->document().view()).value();
    metadata.region = 5;
    CHECK(tab->applyMetadataEdit(metadata));
    CHECK(rowOfRule(panel, "HDR-REGION-RANGE") < 0);
    // Switching the game changes what is judged: 300 has a picture in SB2.
    tab->setProfile(GameProfile::speedyBlupi2);
    CHECK(rowOfRule(panel, "TILE-NOT-DRAWN") < 0);

    // A read-only document is validated but offers no fix.
    auto *old = window.openPath(oldPath);
    CHECK(old != nullptr);
    old->setProfile(GameProfile::speedyBlupi1);
    QApplication::processEvents();
    CHECK(rowOfRule(panel, "STRUCT-READONLY-REVISION") >= 0);
    const auto unterminated = rowOfRule(panel, "HDR-NAME-UNTERMINATED");
    CHECK(unterminated >= 0);
    panel->tree()->setCurrentItem(panel->tree()->topLevelItem(unterminated));
    CHECK(!fixButton->isEnabled());
    CHECK(!window.reviewFix(*panel->findingAtRow(unterminated)));
    CHECK(!old->document().isModified());

    // A finding is read in the table itself: it wraps inside the width the
    // table has instead of running out of it, and a narrower table makes
    // its row taller. Checked on a panel of its own, so that its width is
    // the test's and not a dock layout's.
    {
        DiagnosticsPanel own;
        own.setTab(old);
        own.resize(1600, 400);
        own.show();
        QApplication::processEvents();
        auto *tree = own.tree();
        CHECK(tree->topLevelItemCount() > 0);
        const auto longest = [&] {
            int row = 0;
            for (int index = 0; index < tree->topLevelItemCount(); ++index) {
                if (tree->topLevelItem(index)->text(2).size() >
                    tree->topLevelItem(row)->text(2).size()) {
                    row = index;
                }
            }
            return row;
        }();
        const auto rect = [&] { return tree->visualItemRect(tree->topLevelItem(longest)); };
        const auto oneLine = tree->fontMetrics().height();
        CHECK(tree->topLevelItem(longest)->text(2).size() > 60);
        const auto fits = [&] {
            int columns = 0;
            for (int column = 0; column < 4; ++column) {
                columns += tree->columnWidth(column);
            }
            return columns <= tree->viewport()->width();
        };
        const auto wide = rect().height();
        CHECK(fits());
        own.resize(460, 400);
        QApplication::processEvents();
        const auto narrow = rect().height();
        CHECK(narrow > wide);
        CHECK(narrow >= 2 * oneLine);
        // All four columns fit the table: nothing is pushed out of view.
        CHECK(fits());
        CHECK(tree->wordWrap());
    }

    CHECK(window.close());
    return test::finish("doctor_ui");
}
