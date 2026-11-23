// SPDX-License-Identifier: GPL-3.0-only
//
// File -> New World through the main window: the game is asked for, the
// world the games' editors start with is written to the chosen file and
// opened with that game selected; nothing is written when cancelled or when
// the file is open in the editor.

#include "app/main_window.hpp"
#include "world/new_world.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QAction>
#include <QApplication>
#include <QFile>
#include <QPushButton>
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

QByteArray readAll(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("eggbert-editor-test");
    QCoreApplication::setApplicationName("new-world-ui");
    QTemporaryDir dir;

    MainWindow window;
    QStringList errors;
    window.prompts.error = [&](const QString &, const QString &text) { errors << text; };
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    window.resize(1100, 800);
    window.show();

    auto *action = window.findChild<QAction *>("fileNew");
    CHECK(action != nullptr && action->isEnabled()); // works with nothing open
    auto *button = window.findChild<QPushButton *>("welcomeNew");
    CHECK(button != nullptr);
    CHECK(window.welcomeVisible());

    // From the welcome page, for the first game.
    GameProfile suggestedGame = GameProfile::unknown;
    QString suggestedPath;
    const auto firstPath = dir.filePath("first.blp");
    window.prompts.newWorldGame = [&](GameProfile suggested) {
        suggestedGame = suggested;
        return std::optional(GameProfile::speedyBlupi1);
    };
    window.prompts.saveAsPath = [&](const QString &suggested) {
        suggestedPath = suggested;
        return firstPath;
    };
    button->click();
    CHECK(suggestedGame == GameProfile::speedyBlupi2);
    CHECK(suggestedPath == "new-world.blp");
    CHECK(readAll(firstPath) == raw(newWorld(GameProfile::speedyBlupi1)));
    CHECK_EQ(window.tabCount(), 1);
    CHECK(!window.welcomeVisible());
    auto *first = window.currentTab();
    CHECK(first->file().path() == firstPath);
    CHECK(first->profile() == GameProfile::speedyBlupi1);
    CHECK(first->document().isWritable());
    CHECK(!first->document().isModified());
    CHECK(MainWindow::rememberedProfile(firstPath) == GameProfile::speedyBlupi1);

    // From the menu, for the second game: the open world's game and folder
    // are suggested.
    const auto secondPath = dir.filePath("second.blp");
    window.prompts.newWorldGame = [&](GameProfile suggested) {
        suggestedGame = suggested;
        return std::optional(GameProfile::speedyBlupi2);
    };
    window.prompts.saveAsPath = [&](const QString &suggested) {
        suggestedPath = suggested;
        return secondPath;
    };
    action->trigger();
    CHECK(suggestedGame == GameProfile::speedyBlupi1);
    CHECK(suggestedPath == dir.filePath("new-world.blp"));
    CHECK(readAll(secondPath) == raw(newWorld(GameProfile::speedyBlupi2)));
    CHECK_EQ(window.tabCount(), 2);
    CHECK(window.currentTab()->profile() == GameProfile::speedyBlupi2);
    // The new world is an ordinary document: an edit and a save work.
    {
        auto *tab = window.currentTab();
        EditBuilder builder(tab->document());
        builder.setCell(SectionKind::foreground, 6, 4, 38);
        CHECK(tab->applyTransaction(builder.build("Edit")));
        CHECK(window.saveTab(tab));
        auto expected = newWorld(GameProfile::speedyBlupi2);
        put16(expected, fxCellOffset(fxForegroundOffset, 6, 4), 38);
        CHECK(readAll(secondPath) == raw(expected));
    }

    // A file that is open in the editor is not replaced.
    const auto before = readAll(firstPath);
    window.prompts.saveAsPath = [&](const QString &) { return firstPath; };
    errors.clear();
    CHECK(window.createWorld() == nullptr);
    CHECK_EQ(errors.size(), qsizetype{1});
    CHECK(errors[0].contains("is open in the editor"));
    CHECK(readAll(firstPath) == before);

    // Cancelling either question creates nothing.
    const auto thirdPath = dir.filePath("third.blp");
    window.prompts.newWorldGame = [](GameProfile) { return std::optional<GameProfile>(); };
    window.prompts.saveAsPath = [&](const QString &) { return thirdPath; };
    CHECK(window.createWorld() == nullptr);
    window.prompts.newWorldGame = [](GameProfile) {
        return std::optional(GameProfile::speedyBlupi2);
    };
    window.prompts.saveAsPath = [](const QString &) { return QString(); };
    CHECK(window.createWorld() == nullptr);
    CHECK(!QFile::exists(thirdPath));
    CHECK_EQ(window.tabCount(), 2);

    // A destination that cannot be written.
    window.prompts.saveAsPath = [&](const QString &) { return dir.filePath("no/such/dir/x.blp"); };
    errors.clear();
    CHECK(window.createWorld() == nullptr);
    CHECK_EQ(errors.size(), qsizetype{1});
    CHECK(errors[0].contains("was not written"));

    return test::finish("new_world_ui");
}
