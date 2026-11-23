// SPDX-License-Identifier: GPL-3.0-only
//
// Stamps, aligning objects and replacing a value in all open worlds through
// the main window: each is one undo step per document, and nothing is done
// to read-only worlds or worlds of another game.

#include "app/main_window.hpp"
#include "app/world_canvas.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace eggbert;
using namespace eggbert::test;

namespace {

QByteArray raw(const Bytes &bytes) {
    return QByteArray(reinterpret_cast<const char *>(bytes.data()),
                      static_cast<qsizetype>(bytes.size()));
}

QByteArray current(DocumentTab *tab) {
    const auto bytes = tab->document().currentBytes();
    return QByteArray(reinterpret_cast<const char *>(bytes.data()),
                      static_cast<qsizetype>(bytes.size()));
}

void writeAll(const QString &path, const Bytes &bytes) {
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(raw(bytes));
}

void object(Bytes &bytes, std::size_t slot, int type, int x, int y) {
    const auto at = fxSlotOffset(fxRevision13, slot);
    put16(bytes, at, type);
    for (const std::size_t field : {12u, 20u, 28u}) {
        put32(bytes, at + field, x);
        put32(bytes, at + field + 4, y);
    }
}

CellMask rectangle(std::size_t left, std::size_t top, std::size_t right, std::size_t bottom) {
    CellMask mask;
    for (std::size_t x = left; x <= right; ++x) {
        for (std::size_t y = top; y <= bottom; ++y) {
            mask.set({x, y});
        }
    }
    return mask;
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("eggbert-editor-test");
    QCoreApplication::setApplicationName("stamps-ui");
    QSettings().clear();
    QDir(MainWindow::stampDirectory()).removeRecursively();
    QTemporaryDir dir;

    auto worldA = blankWorld(fxRevision13);
    put16(worldA, fxCellOffset(fxForegroundOffset, 10, 20), 30);
    put16(worldA, fxCellOffset(fxForegroundOffset, 11, 21), 31);
    put16(worldA, fxCellOffset(fxRevision13.bigDecorOffset, 10, 21), 20);
    object(worldA, 0, 5, 10 * 64 + 2, 20 * 64 + 4);
    object(worldA, 1, 5, 11 * 64 + 40, 20 * 64 + 30);
    object(worldA, 2, 5, 60 * 64, 60 * 64);
    auto worldB = blankWorld(fxRevision13);
    put16(worldB, fxCellOffset(fxForegroundOffset, 1, 1), 30);
    put16(worldB, fxCellOffset(fxForegroundOffset, 2, 2), 30);
    auto worldC = blankWorld(fxRevision13);
    put16(worldC, fxCellOffset(fxForegroundOffset, 3, 3), 30);
    const auto pathA = dir.filePath("a.blp");
    const auto pathB = dir.filePath("b.blp");
    const auto pathC = dir.filePath("c.blp");
    const auto pathOld = dir.filePath("old.blp");
    writeAll(pathA, worldA);
    writeAll(pathB, worldB);
    writeAll(pathC, worldC);
    auto oldWorld = blankWorld(fxRevision10);
    put16(oldWorld, fxCellOffset(fxForegroundOffset, 4, 4), 30);
    writeAll(pathOld, oldWorld);

    MainWindow window;
    QStringList errors;
    window.prompts.error = [&](const QString &, const QString &text) { errors << text; };
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    window.prompts.confirmBroadEdit = [](DocumentTab *, const BroadEditPreview &) { return true; };
    window.resize(1200, 800);
    window.show();
    auto *a = window.openPath(pathA);
    CHECK(a != nullptr);
    a->setProfile(GameProfile::speedyBlupi1);

    // --- Stamps ---------------------------------------------------------
    auto *save = window.findChild<QAction *>("editSaveStamp");
    auto *place = window.findChild<QAction *>("editPlaceStamp");
    CHECK(save != nullptr && place != nullptr && save->isEnabled() && place->isEnabled());
    CHECK(window.stamps().empty());
    // Nothing selected, nothing stored.
    CHECK(!window.saveSelectionAsStamp(a));
    CHECK(errors.size() == 1 && errors[0].contains("Select the cells"));
    errors.clear();
    CHECK(!window.placeStamp(a));
    CHECK(errors.size() == 1 && errors[0].contains("No stamp is stored"));
    errors.clear();

    a->setSelection(rectangle(10, 20, 11, 21));
    window.prompts.stampName = [](DocumentTab *) {
        return std::optional(std::make_pair(QString("Corner piece"), true));
    };
    save->trigger();
    CHECK(errors.isEmpty());
    const auto stored = window.stamps();
    CHECK_EQ(stored.size(), std::size_t{1});
    CHECK(stored[0].name == "Corner piece" && stored[0].game == GameProfile::speedyBlupi1);
    CHECK(stored[0].width == 2 && stored[0].height == 2 && stored[0].objects.size() == 2);
    CHECK(QFile::exists(QDir(MainWindow::stampDirectory()).filePath("Corner-piece.stamp")));
    // The same name again is refused, not overwritten.
    CHECK(!window.saveSelectionAsStamp(a));
    CHECK(errors.size() == 1 && errors[0].contains("already stored"));
    errors.clear();
    CHECK(current(a) == raw(worldA)); // saving a stamp changes nothing

    // Placed at the cursor cell as one undo step.
    std::size_t offered = 0;
    window.prompts.chooseStamp = [&](DocumentTab *, const std::vector<Stamp> &list) {
        offered = list.size();
        return std::optional(std::size_t{0});
    };
    a->canvas()->setCursorCell({40, 50});
    place->trigger();
    CHECK_EQ(offered, std::size_t{1});
    {
        const auto view = a->document().view();
        CHECK_EQ(*view.foreground()->cell(40, 50), 30);
        CHECK_EQ(*view.foreground()->cell(41, 51), 31);
        CHECK_EQ(*view.foreground()->cell(41, 50), -1); // a hole stays empty
        CHECK_EQ(*view.bigDecor()->cell(40, 51), 20);
        CHECK_EQ(view.objects()->activeCount(), std::size_t{5});
        CHECK(view.objects()->record(3)->posStart == (Point{40 * 64 + 2, 50 * 64 + 4}));
    }
    CHECK(a->undo());
    CHECK(current(a) == raw(worldA));
    // Cancelled, or onto a world of another game: nothing.
    window.prompts.chooseStamp = [](DocumentTab *, const std::vector<Stamp> &) {
        return std::optional<std::size_t>();
    };
    CHECK(!window.placeStamp(a));
    window.prompts.chooseStamp = [](DocumentTab *, const std::vector<Stamp> &) {
        return std::optional(std::size_t{0});
    };
    auto *c = window.openPath(pathC);
    c->setProfile(GameProfile::speedyBlupi2);
    CHECK(!window.placeStamp(c));
    CHECK(current(c) == raw(worldC));
    // A damaged stamp file is not offered.
    {
        QFile broken(QDir(MainWindow::stampDirectory()).filePath("broken.stamp"));
        CHECK(broken.open(QIODevice::WriteOnly));
        broken.write("eggbert-stamp 1\nname x\ngame 1\nsize 2\n");
        broken.close();
        CHECK_EQ(window.stamps().size(), std::size_t{1});
    }

    // --- Aligning objects ----------------------------------------------
    window.setCurrentTab(a);
    auto *alignTop = window.findChild<QAction *>("objectAlignTop");
    CHECK(alignTop != nullptr && alignTop->isEnabled());
    a->setSelection(rectangle(10, 20, 11, 21));
    CHECK(a->objectsInSelection() == (std::vector<std::size_t>{0, 1}));
    alignTop->trigger();
    {
        const auto pool = a->document().view().objects();
        CHECK_EQ(pool->record(0)->posStart.y, 20 * 64 + 4);
        CHECK(pool->record(1)->posStart == (Point{11 * 64 + 40, 20 * 64 + 4}));
        CHECK(pool->record(1)->posEnd == (Point{11 * 64 + 40, 20 * 64 + 4}));
        CHECK(pool->record(2)->posStart == (Point{60 * 64, 60 * 64})); // not selected
    }
    CHECK(a->undo());
    CHECK(current(a) == raw(worldA));
    // Objects of a type not verified for the chosen game are left as stored.
    {
        EditBuilder builder(a->document());
        builder.setObjectValue(1, ObjectField::type, 77); // no such verified type
        CHECK(a->applyTransaction(builder.build("Unknown type")));
        const auto before = current(a);
        a->setSelection(rectangle(10, 20, 11, 21));
        CHECK(!a->alignSelectedObjects(AlignMode::top)); // only one alignable object left
        CHECK(current(a) == before);
        CHECK(a->undo());
        CHECK(current(a) == raw(worldA));
    }
    // One object, or none: nothing to align.
    a->setSelection(rectangle(10, 20, 10, 20));
    CHECK(!a->alignSelectedObjects(AlignMode::left));
    a->setSelection(CellMask{});
    CHECK(!a->alignSelectedObjects(AlignMode::left));
    CHECK(current(a) == raw(worldA));

    // --- Replace in all open worlds --------------------------------------
    auto *b = window.openPath(pathB);
    b->setProfile(GameProfile::speedyBlupi1);
    auto *old = window.openPath(pathOld);
    old->setProfile(GameProfile::speedyBlupi1);
    window.setCurrentTab(a);
    QStringList shown;
    bool confirm = false;
    window.prompts.replaceValues = [](DocumentTab *) {
        return std::optional(std::make_pair(30, 44));
    };
    window.prompts.confirmBatch = [&](const QString &, const QStringList &lines) {
        shown = lines;
        return confirm;
    };
    auto *replaceAll = window.findChild<QAction *>("layerReplaceAll");
    CHECK(replaceAll != nullptr && replaceAll->isEnabled());
    replaceAll->trigger(); // declined
    CHECK(shown.size() == 5);
    CHECK(shown.join('\n').contains("a.blp: 1 cell(s)"));
    CHECK(shown.join('\n').contains("b.blp: 2 cell(s)"));
    CHECK(shown.join('\n').contains("c.blp: skipped (another game)"));
    CHECK(shown.join('\n').contains("old.blp: skipped (read-only"));
    CHECK(current(a) == raw(worldA) && current(b) == raw(worldB));
    confirm = true;
    CHECK_EQ(window.replaceInAllOpenWorlds(), 2);
    CHECK_EQ(*a->document().view().foreground()->cell(10, 20), 44);
    CHECK_EQ(*b->document().view().foreground()->cell(1, 1), 44);
    CHECK_EQ(*b->document().view().foreground()->cell(2, 2), 44);
    CHECK(current(c) == raw(worldC));
    CHECK(current(old) == raw(oldWorld));
    // One undo step in each world.
    CHECK(a->undo() && b->undo());
    CHECK(current(a) == raw(worldA) && current(b) == raw(worldB));
    CHECK(!a->document().canUndo() || current(a) == raw(worldA));
    // A value no world holds: nothing asked, nothing done.
    shown.clear();
    window.prompts.replaceValues = [](DocumentTab *) {
        return std::optional(std::make_pair(77, 44));
    };
    CHECK_EQ(window.replaceInAllOpenWorlds(), 0);
    CHECK(shown.isEmpty());
    window.prompts.replaceValues = [](DocumentTab *) {
        return std::optional<std::pair<int, int>>();
    };
    CHECK_EQ(window.replaceInAllOpenWorlds(), 0);

    QDir(MainWindow::stampDirectory()).removeRecursively();
    QSettings().clear();
    return test::finish("stamps_ui");
}
