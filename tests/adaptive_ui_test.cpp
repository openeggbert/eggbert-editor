// SPDX-License-Identifier: GPL-3.0-only
#include "app/main_window.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include <set>

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

std::int16_t fg(const DocumentTab *tab, std::size_t x, std::size_t y) {
    return *tab->document().view().foreground()->cell(x, y);
}

using CellSet = std::set<std::pair<std::size_t, std::size_t>>;
CellSet asSet(const std::vector<CellPos> &cells) {
    CellSet result;
    for (const auto &cell : cells) {
        result.insert({cell.x, cell.y});
    }
    return result;
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setOrganizationName("eggbert-editor-tests");
    QApplication::setApplicationName("adaptive-ui-test");
    QSettings().clear();

    QTemporaryDir dir;
    auto bytes = blankWorld(fxRevision13);
    put32(bytes, 212, 100);
    put32(bytes, 216, 100);
    // An object and a bigDecor cell that no adaptive edit may touch.
    put16(bytes, fxSlotOffset(fxRevision13, 0), 5);
    put16(bytes, fxCellOffset(fxRevision13.bigDecorOffset, 20, 30), 20);
    const auto path = dir.filePath("adaptive.blp");
    writeWorld(path, bytes);
    const auto oldPath = dir.filePath("old.blp");
    writeWorld(oldPath, blankWorld(fxRevision11));

    MainWindow window;
    window.prompts.error = [](const QString &, const QString &) {};
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    window.resize(1400, 950);
    window.show();
    CHECK(QTest::qWaitForWindowExposed(&window));

    auto *tab = window.openPath(path);
    CHECK(tab != nullptr);
    tab->setVariantSource(std::make_unique<NoVariants>());
    QStringList status;
    QObject::connect(tab, &DocumentTab::statusMessage,
                     [&](const QString &message) { status << message; });
    QApplication::processEvents();
    auto *canvas = tab->canvas();
    auto *combo = window.findChild<QComboBox *>("adaptiveCombo");
    const auto enabled = [&](const char *name) { return window.action(name)->isEnabled(); };

    // No game chosen: the tool is unavailable and refuses with a reason.
    CHECK(!enabled("toolAdaptive"));
    CHECK_EQ(combo->count(), 2);
    const CellPos first{20, 30};
    CHECK(!tab->adaptiveAt({&first, 1}, 0));
    CHECK(status.join('\n').contains("Choose a game profile"));
    CHECK(tab->previewAdaptive(first).changedCount() == 0);
    CHECK(!tab->document().isModified());

    // Families follow the chosen game.
    tab->setProfile(GameProfile::speedyBlupi2);
    CHECK_EQ(combo->count(), 2 + 17);
    tab->setProfile(GameProfile::speedyBlupi1);
    CHECK_EQ(combo->count(), 2 + 4);
    CHECK(combo->findData("bricks") < 0);
    CHECK(enabled("toolAdaptive"));

    // Paint rock: a click, then a drag as one undo step.
    window.action("toolAdaptive")->trigger();
    CHECK(tab->tool() == Tool::adaptive);
    combo->setCurrentIndex(combo->findData("rock"));
    CHECK_EQ(tab->adaptiveBrush(), QString("rock"));
    CHECK_EQ(tab->adaptiveValue().value(), std::int16_t{153});
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 20, 30));
    CHECK_EQ(fg(tab, 20, 30), std::int16_t{153});
    CHECK_EQ(tab->document().undoLabel(), std::string_view("Adaptive Brush"));
    QTest::mousePress(canvas, Qt::LeftButton, {}, cellCentre(canvas, 21, 30));
    QTest::mouseMove(canvas, cellCentre(canvas, 23, 30));
    QTest::mouseRelease(canvas, Qt::LeftButton, {}, cellCentre(canvas, 23, 30));
    CHECK_EQ(fg(tab, 20, 30), std::int16_t{40});
    CHECK_EQ(fg(tab, 21, 30), std::int16_t{38});
    CHECK_EQ(fg(tab, 22, 30), std::int16_t{38});
    CHECK_EQ(fg(tab, 23, 30), std::int16_t{39});
    CHECK_EQ(tab->document().historySize(), std::size_t{2});
    // Only foreground bytes changed: object and bigDecor are intact.
    {
        const auto current = tab->document().currentBytes();
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            if (index < 868 || index >= 20'868) {
                CHECK(current[index] == bytes[index]);
            }
        }
    }

    // Hover preview: the cell and the neighbour that would change.
    {
        const auto preview = tab->previewAdaptive({24, 30});
        CHECK((asSet(preview.changedFootprint) == CellSet{{24, 30}}));
        CHECK((asSet(preview.sideEffects) == CellSet{{23, 30}}));
        // Let pending layout settle first: docks may still be adjusting the
        // canvas size, and the canvas keeps the world fitted to it.
        QTest::qWait(50);
        QTest::mouseMove(canvas, cellCentre(canvas, 24, 30));
        CHECK((asSet(canvas->previewCells()) == CellSet{{24, 30}, {23, 30}}));
        CHECK_EQ(fg(tab, 24, 30), std::int16_t{-1}); // previewing writes nothing
        QTest::mouseMove(canvas, cellCentre(canvas, 80, 80));
        CHECK((asSet(canvas->previewCells()) == CellSet{{80, 80}}));
    }

    // Undo restores the previous forms exactly.
    window.action("editUndo")->trigger();
    CHECK_EQ(fg(tab, 20, 30), std::int16_t{153});
    CHECK_EQ(fg(tab, 21, 30), std::int16_t{-1});
    window.action("editRedo")->trigger();
    CHECK_EQ(fg(tab, 22, 30), std::int16_t{38});

    // Erase brush: neighbours re-form.
    combo->setCurrentIndex(combo->findData(DocumentTab::adaptiveEraseBrush));
    status.clear();
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 22, 30));
    CHECK_EQ(fg(tab, 22, 30), std::int16_t{-1});
    CHECK_EQ(fg(tab, 21, 30), std::int16_t{39});
    CHECK_EQ(fg(tab, 23, 30), std::int16_t{153});
    CHECK(status.join('\n').contains("2 neighbouring cell(s)"));

    // Tile-ID brush: an ordinary tile next to rock updates the rock.
    combo->setCurrentIndex(combo->findData(DocumentTab::adaptiveTileBrush));
    tab->setTileId(5);
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 23, 31));
    CHECK_EQ(fg(tab, 23, 31), std::int16_t{5});
    CHECK_EQ(fg(tab, 23, 30), std::int16_t{147}); // rock now joined below

    // A selection limits the footprint; neighbours outside may still re-form.
    combo->setCurrentIndex(combo->findData("rock"));
    tab->setSelection(CellMask::fromRect({{50, 50}, {50, 50}}));
    const std::vector<CellPos> two{{50, 50}, {51, 50}};
    CHECK(tab->adaptiveAt(two, 0));
    CHECK_EQ(fg(tab, 50, 50), std::int16_t{153});
    CHECK_EQ(fg(tab, 51, 50), std::int16_t{-1});
    const CellPos outsideSelection{60, 60};
    CHECK(!tab->adaptiveAt({&outsideSelection, 1}, 0));
    CHECK_EQ(tab->previewAdaptive(outsideSelection).changedCount(), std::size_t{0});
    tab->selectNone();

    // A locked foreground refuses.
    tab->setLayerLocked(SectionKind::foreground, true);
    status.clear();
    const CellPos locked{70, 70};
    CHECK(!tab->adaptiveAt({&locked, 1}, 0));
    CHECK(status.join('\n').contains("locked"));
    CHECK_EQ(tab->previewAdaptive(locked).changedCount(), std::size_t{0});
    tab->setLayerLocked(SectionKind::foreground, false);

    // Switching to a game without the chosen family falls back to the tile brush.
    tab->setProfile(GameProfile::speedyBlupi2);
    combo->setCurrentIndex(combo->findData("cheese-246"));
    CHECK_EQ(tab->adaptiveValue().value(), std::int16_t{246});
    const CellPos cheese{30, 60};
    CHECK(tab->adaptiveAt({&cheese, 1}, 0));
    CHECK_EQ(fg(tab, 30, 61), std::int16_t{264});
    tab->setProfile(GameProfile::speedyBlupi1);
    CHECK_EQ(tab->adaptiveBrush(), QString(DocumentTab::adaptiveTileBrush));

    // Random looks come from the tab's variant source and are stored: redo
    // reproduces the same bytes without drawing again.
    tab->setVariantSource(std::make_unique<RecordedVariants>(std::vector<int>{1}));
    combo->setCurrentIndex(combo->findData("rock"));
    for (const auto &cell : {CellPos{40, 80}, {40, 79}, {40, 81}, {39, 80}, {41, 80}}) {
        tab->setTileId(1);
        tab->setAdaptiveBrush(DocumentTab::adaptiveTileBrush);
        if (!(cell == CellPos{40, 80})) {
            CHECK(tab->adaptiveAt({&cell, 1}, 0));
        }
    }
    tab->setAdaptiveBrush("rock");
    const CellPos surrounded{40, 80};
    CHECK(tab->adaptiveAt({&surrounded, 1}, 0));
    CHECK_EQ(fg(tab, 40, 80), std::int16_t{156}); // the recorded coin chose the variant
    window.action("editUndo")->trigger();
    CHECK_EQ(fg(tab, 40, 80), std::int16_t{-1});
    window.action("editRedo")->trigger();
    CHECK_EQ(fg(tab, 40, 80), std::int16_t{156});

    // Read-only revision: unavailable.
    auto *old = window.openPath(oldPath);
    CHECK(old != nullptr);
    old->setProfile(GameProfile::speedyBlupi1);
    QApplication::processEvents();
    CHECK(!enabled("toolAdaptive"));
    old->setAdaptiveBrush("rock");
    CHECK(!old->adaptiveAt({&first, 1}, 0));
    CHECK_EQ(old->previewAdaptive(first).changedCount(), std::size_t{0});
    CHECK(!old->document().isModified());

    CHECK(window.close());
    return test::finish("adaptive_ui");
}
