// SPDX-License-Identifier: GPL-3.0-only
//
// Help -> Element Encyclopedia: lists the catalogs of the current tab's
// game, filters as typed, shows the evidence, and "Select in Editor" makes
// the same choice the Palette makes.

#include "app/encyclopedia_dialog.hpp"
#include "app/main_window.hpp"
#include "world/object_catalog.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace eggbert;
using namespace eggbert::test;

namespace {

void writeAll(const QString &path, const Bytes &bytes) {
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<qint64>(bytes.size()));
}

int rowOf(EncyclopediaDialog *dialog, EntryKind kind) {
    for (int row = 0; row < dialog->list()->count(); ++row) {
        dialog->list()->setCurrentRow(row);
        if (dialog->currentEntry()->kind == kind) {
            return row;
        }
    }
    return -1;
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("eggbert-editor-test");
    QCoreApplication::setApplicationName("encyclopedia-ui");
    QTemporaryDir dir;
    const auto path = dir.filePath("a.blp");
    writeAll(path, blankWorld(fxRevision13));

    MainWindow window;
    window.prompts.error = [](const QString &, const QString &) {};
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    window.resize(1100, 800);
    window.show();

    auto *action = window.findChild<QAction *>("helpEncyclopedia");
    CHECK(action != nullptr && action->isEnabled());
    CHECK(window.encyclopediaDialog() == nullptr);

    // Without a world there is no game: an explanation, no entries.
    action->trigger();
    auto *dialog = window.encyclopediaDialog();
    CHECK(dialog != nullptr && dialog->isVisible());
    CHECK_EQ(dialog->list()->count(), 0);
    CHECK(!dialog->selectButton()->isEnabled());
    CHECK(dialog->currentEntry() == nullptr);

    // With a world of the first game the dialog follows it.
    auto *tab = window.openPath(path);
    CHECK(tab != nullptr);
    tab->setProfile(GameProfile::speedyBlupi1);
    QCoreApplication::processEvents();
    CHECK(dialog->game() == GameProfile::speedyBlupi1);
    const auto all = static_cast<int>(encyclopedia(GameProfile::speedyBlupi1).size());
    CHECK_EQ(dialog->list()->count(), all);
    CHECK(dialog->selectButton()->isEnabled());
    // Every row has a picture (a numbered placeholder without game data).
    for (int row = 0; row < dialog->list()->count(); ++row) {
        CHECK(!dialog->list()->item(row)->icon().isNull());
    }

    // The details name the element, what is stored and the evidence.
    const auto objectRow = rowOf(dialog, EntryKind::object);
    CHECK(objectRow >= 0);
    const auto object = *dialog->currentEntry();
    CHECK(dialog->details()->text().contains(QString::fromStdString(object.title).toHtmlEscaped()));
    CHECK(dialog->details()->text().contains("Object type"));
    CHECK(dialog->details()->text().contains("Evidence"));
    CHECK(dialog->details()->text().contains(
        QString::fromStdString(object.provenance).toHtmlEscaped()));

    // Select in Editor: an object chooses its preset and the Place tool.
    dialog->selectButton()->click();
    CHECK(tab->tool() == Tool::placeObject);
    CHECK(tab->placePreset() != nullptr && tab->placePreset()->id == object.id);

    // A tile sets the layer, the tile number and a painting tool.
    const auto tileRow = rowOf(dialog, EntryKind::tile);
    CHECK(tileRow >= 0);
    const auto tile = *dialog->currentEntry();
    emit dialog->list()->itemActivated(dialog->list()->currentItem()); // double click / Enter
    CHECK(tab->tool() == Tool::pencil);
    CHECK(tab->activeLayer() == tile.layer);
    CHECK_EQ(tab->tileId(), tile.values.front());

    // A border family selects the adaptive brush.
    const auto borderRow = rowOf(dialog, EntryKind::border);
    CHECK(borderRow >= 0);
    const auto border = *dialog->currentEntry();
    dialog->selectButton()->click();
    CHECK(tab->tool() == Tool::adaptive);
    CHECK(tab->adaptiveBrush() == QString::fromStdString(border.id));

    // Typing filters; the count is shown; clearing restores everything.
    dialog->searchBox()->setText(QString::fromStdString(object.title));
    CHECK(dialog->list()->count() >= 1 && dialog->list()->count() < all);
    bool found = false;
    for (int row = 0; row < dialog->list()->count(); ++row) {
        found = found || dialog->list()->item(row)->text() == QString::fromStdString(object.title);
    }
    CHECK(found);
    dialog->searchBox()->setText("no such element anywhere");
    CHECK_EQ(dialog->list()->count(), 0);
    CHECK(!dialog->selectButton()->isEnabled());
    CHECK(dialog->details()->text().isEmpty());
    dialog->searchBox()->clear();
    CHECK_EQ(dialog->list()->count(), all);

    // Changing the game of the tab changes the list.
    tab->setProfile(GameProfile::speedyBlupi2);
    QCoreApplication::processEvents();
    CHECK(dialog->game() == GameProfile::speedyBlupi2);
    CHECK_EQ(dialog->list()->count(),
             static_cast<int>(encyclopedia(GameProfile::speedyBlupi2).size()));

    // On a read-only world the choice is not applied.
    const auto oldPath = dir.filePath("old.blp");
    writeAll(oldPath, blankWorld(fxRevision10));
    auto *oldTab = window.openPath(oldPath);
    CHECK(oldTab != nullptr && !oldTab->document().isWritable());
    oldTab->setProfile(GameProfile::speedyBlupi1);
    QCoreApplication::processEvents();
    const auto toolBefore = oldTab->tool();
    CHECK(rowOf(dialog, EntryKind::tile) >= 0);
    dialog->selectButton()->click();
    CHECK(oldTab->tool() == toolBefore);

    return test::finish("encyclopedia_ui");
}
