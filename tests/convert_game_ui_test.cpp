// SPDX-License-Identifier: GPL-3.0-only
//
// File -> Convert for Other Game: the notes are shown before anything is
// written, the copy goes to a new file and opens with the other game
// selected; the source file and its tab are untouched.

#include "app/main_window.hpp"
#include "world/game_convert.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

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

void writeAll(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(bytes);
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("eggbert-editor-test");
    QCoreApplication::setApplicationName("convert-game-ui");
    QTemporaryDir dir;

    // A world of the second game with a tile and an object the first game
    // does not have, and a lift drawn with a tile picture it lacks.
    auto modern = blankWorld(fxRevision13);
    put16(modern, fxCellOffset(fxForegroundOffset, 7, 7), 300);
    put16(modern, fxSlotOffset(fxRevision13, 2), 44);
    put16(modern, fxSlotOffset(fxRevision13, 3), 1);
    put16(modern, fxSlotOffset(fxRevision13, 3) + 42, 1);
    put16(modern, fxSlotOffset(fxRevision13, 3) + 44, 311);
    const auto path = dir.filePath("modern.blp");
    writeAll(path, raw(modern));

    MainWindow window;
    QStringList errors;
    window.prompts.error = [&](const QString &, const QString &text) { errors << text; };
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    window.resize(1100, 800);
    window.show();
    const auto dialogPrompt = window.prompts.convertGame;

    auto *action = window.findChild<QAction *>("fileConvertGame");
    CHECK(action != nullptr && !action->isEnabled());
    auto *tab = window.openPath(path);
    CHECK(tab != nullptr);
    // Without a chosen game there is nothing to convert from.
    tab->setProfile(GameProfile::unknown);
    QCoreApplication::processEvents();
    CHECK(!action->isEnabled());
    CHECK(window.convertGameOf(tab) == nullptr);
    tab->setProfile(GameProfile::speedyBlupi2);
    QCoreApplication::processEvents();
    CHECK(action->isEnabled());

    // The prompt receives the target and every note; "keep" writes the
    // library's bytes.
    GameProfile askedTarget = GameProfile::unknown;
    std::vector<GameConversionNote> askedNotes;
    QString suggestedPath;
    const auto keptPath = dir.filePath("kept.blp");
    window.prompts.convertGame = [&](DocumentTab *, GameProfile target,
                                     const std::vector<GameConversionNote> &notes) {
        askedTarget = target;
        askedNotes = notes;
        return std::optional(UnknownPolicy::keep);
    };
    window.prompts.saveAsPath = [&](const QString &suggested) {
        suggestedPath = suggested;
        return keptPath;
    };
    action->trigger();
    CHECK(askedTarget == GameProfile::speedyBlupi1);
    CHECK_EQ(askedNotes.size(), std::size_t{3});
    CHECK(suggestedPath.endsWith("modern-game1.blp"));
    const auto expectedKept = convertGame(modern, GameProfile::speedyBlupi2,
                                          GameProfile::speedyBlupi1, UnknownPolicy::keep);
    CHECK(expectedKept.has_value());
    CHECK(readAll(keptPath) == raw(expectedKept->bytes));
    CHECK(readAll(path) == raw(modern));
    CHECK_EQ(window.tabCount(), 2);
    auto *kept = window.currentTab();
    CHECK(kept != tab && kept->file().path() == keptPath);
    CHECK(kept->profile() == GameProfile::speedyBlupi1);
    CHECK(MainWindow::rememberedProfile(keptPath) == GameProfile::speedyBlupi1);
    CHECK(tab->profile() == GameProfile::speedyBlupi2);
    CHECK(!tab->document().isModified());
    CHECK(errors.isEmpty());

    // "Remove" clears exactly what the first game does not know.
    const auto removedPath = dir.filePath("removed.blp");
    window.prompts.convertGame = [](DocumentTab *, GameProfile,
                                    const std::vector<GameConversionNote> &) {
        return std::optional(UnknownPolicy::remove);
    };
    window.prompts.saveAsPath = [&](const QString &) { return removedPath; };
    auto *removed = window.convertGameOf(tab);
    CHECK(removed != nullptr);
    const auto expectedRemoved = convertGame(modern, GameProfile::speedyBlupi2,
                                             GameProfile::speedyBlupi1, UnknownPolicy::remove);
    CHECK(expectedRemoved.has_value() && expectedRemoved->removedCount == 2);
    CHECK(readAll(removedPath) == raw(expectedRemoved->bytes));
    CHECK_EQ(*removed->document().view().foreground()->cell(7, 7), -1);

    // The copy converts back; nothing is unknown to the second game.
    const auto backPath = dir.filePath("back.blp");
    window.prompts.convertGame = [&](DocumentTab *, GameProfile target,
                                     const std::vector<GameConversionNote> &notes) {
        askedTarget = target;
        askedNotes = notes;
        return std::optional(UnknownPolicy::keep);
    };
    window.prompts.saveAsPath = [&](const QString &) { return backPath; };
    auto *back = window.convertGameOf(removed);
    CHECK(back != nullptr);
    CHECK(askedTarget == GameProfile::speedyBlupi2);
    CHECK(back->profile() == GameProfile::speedyBlupi2);
    CHECK(back->document().probe().descriptor->revision == FormatRevision::v1_3);

    // Never onto an open file (the source included); cancel paths.
    const auto tabs = window.tabCount();
    window.prompts.saveAsPath = [&](const QString &) { return path; };
    errors.clear();
    CHECK(window.convertGameOf(tab) == nullptr);
    CHECK(errors.size() == 1 && errors[0].contains("never replaces its source"));
    CHECK(readAll(path) == raw(modern));
    window.prompts.saveAsPath = [](const QString &) { return QString(); };
    CHECK(window.convertGameOf(tab) == nullptr);
    window.prompts.convertGame = [](DocumentTab *, GameProfile,
                                    const std::vector<GameConversionNote> &) {
        return std::optional<UnknownPolicy>();
    };
    window.prompts.saveAsPath = [&](const QString &) { return dir.filePath("never.blp"); };
    CHECK(window.convertGameOf(tab) == nullptr);
    CHECK(!QFile::exists(dir.filePath("never.blp")));
    CHECK_EQ(window.tabCount(), tabs);

    // The dialog itself.
    {
        const auto notes = convertGame(modern, GameProfile::speedyBlupi2,
                                       GameProfile::speedyBlupi1, UnknownPolicy::keep)
                               ->notes;
        bool seen = false;
        QTimer::singleShot(0, &window, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            CHECK(dialog != nullptr);
            if (dialog == nullptr) {
                return;
            }
            seen = true;
            auto *list = dialog->findChild<QListWidget *>("convertGameNotes");
            CHECK_EQ(list->count(), 3);
            CHECK(list->item(0)->text().contains("[unknown]"));
            CHECK(list->item(2)->text().contains("[differs] object picture 311"));
            CHECK(!list->item(0)->toolTip().isEmpty()); // the evidence
            auto *keep = dialog->findChild<QRadioButton *>("convertGameKeep");
            auto *remove = dialog->findChild<QRadioButton *>("convertGameRemove");
            CHECK(keep->isVisible() && keep->isChecked() && remove->isVisible());
            remove->setChecked(true);
            dialog->accept();
        });
        const auto choice = dialogPrompt(tab, GameProfile::speedyBlupi1, notes);
        CHECK(seen);
        CHECK(choice == UnknownPolicy::remove);
        // With nothing to say there is nothing to choose.
        seen = false;
        QTimer::singleShot(0, &window, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            CHECK(dialog != nullptr);
            if (dialog == nullptr) {
                return;
            }
            seen = true;
            CHECK_EQ(dialog->findChild<QListWidget *>("convertGameNotes")->count(), 1);
            CHECK(!dialog->findChild<QRadioButton *>("convertGameKeep")->isVisible());
            dialog->reject();
        });
        CHECK(!dialogPrompt(tab, GameProfile::speedyBlupi1, {}).has_value());
        CHECK(seen);
    }
    return test::finish("convert_game_ui");
}
