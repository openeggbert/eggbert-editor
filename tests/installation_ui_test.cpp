// SPDX-License-Identifier: GPL-3.0-only
//
// The Installation dock on a generated game directory: the worlds are
// listed with their roles, a world is deployed to a slot and rolled back
// byte-exactly, and Playtest only composes and hands over a command (no
// program is started by this test).

#include "app/installation_panel.hpp"
#include "app/demo_viewer.hpp"
#include "app/main_window.hpp"

#include "check.hpp"
#include "fixture.hpp"
#include "generated_game.hpp"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
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

QByteArray current(DocumentTab *tab) {
    const auto bytes = tab->document().currentBytes();
    return QByteArray(reinterpret_cast<const char *>(bytes.data()),
                      static_cast<qsizetype>(bytes.size()));
}

int rowOf(QListWidget *list, const QString &fileName) {
    for (int row = 0; row < list->count(); ++row) {
        if (list->item(row)->text().startsWith(fileName)) {
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
    QCoreApplication::setApplicationName("installation-ui");
    QSettings().clear();
    QDir(DeployStore::defaultDirectory()).removeRecursively();
    QTemporaryDir dir;

    const auto game = makeGame(dir.filePath("game"));
    const QDir data(game.root + "/DATA");
    auto titled = blankWorld(fxRevision13);
    putText(titled, 376, "First steps");
    const auto hub = raw(patternedWorld(fxRevision12));
    write(data.filePath("WORLD001.BLP"), raw(blankWorld(fxRevision12)));
    write(data.filePath("WORLD010.BLP"), hub);
    write(data.filePath("WORLD011.BLP"), raw(titled));
    write(data.filePath("WORLD020.BLP"), raw(blankWorld(fxRevision12)).left(9000)); // cut short
    write(data.filePath("u001-001.blp"), raw(blankWorld(fxRevision12)));
    write(data.filePath("u001-999.blp"), raw(blankWorld(fxRevision12)));
    write(data.filePath("S001-000.BLP"), "save");
    write(data.filePath("INFO001.BLP"), QByteArray(528, 'i'));
    write(data.filePath("notes.txt"), "mine");

    auto mine = blankWorld(fxRevision12);
    put16(mine, fxCellOffset(fxForegroundOffset, 9, 9), 30);
    const auto minePath = dir.filePath("mine.blp");
    write(minePath, raw(mine));

    MainWindow window;
    QStringList errors;
    window.prompts.error = [&](const QString &, const QString &text) { errors << text; };
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    window.resize(1200, 800);
    window.show();
    auto *panel = window.installation();
    CHECK(panel != nullptr);
    const auto dialogPrompt = window.prompts.deploySlot;
    auto *toggle = window.findChild<QAction *>("viewInstallation");
    CHECK(toggle != nullptr && !toggle->isChecked()); // hidden until asked for

    // Without a game directory: an explanation, nothing to deploy to.
    auto *tab = window.openPath(minePath);
    CHECK(tab != nullptr);
    tab->setProfile(GameProfile::speedyBlupi1);
    QCoreApplication::processEvents();
    CHECK(panel->dataDirectory().isEmpty());
    CHECK_EQ(panel->list()->count(), 0);
    CHECK(!panel->deployButton()->isEnabled());
    CHECK(window.deployWorld(tab) == std::nullopt);
    CHECK(errors.size() == 1 && errors[0].contains("DATA folder"));
    errors.clear();

    // With it: the worlds, sorted, with roles and titles.
    CHECK(window.loadArtwork(game.root, GameProfile::speedyBlupi1));
    QCoreApplication::processEvents();
    CHECK(panel->dataDirectory() == data.absolutePath());
    CHECK_EQ(panel->list()->count(), 6);
    CHECK_EQ(panel->entries().size(), std::size_t{6});
    CHECK(panel->list()->item(0)->text().startsWith("WORLD001.BLP - first world"));
    CHECK(panel->list()->item(1)->text().startsWith("WORLD010.BLP - group hub"));
    CHECK(panel->list()->item(2)->text().startsWith("WORLD011.BLP - mission"));
    CHECK(panel->list()->item(2)->text().contains("First steps"));
    CHECK(panel->list()->item(3)->text().contains("(not a complete world)"));
    CHECK(panel->list()->item(4)->text().startsWith("u001-001.blp - player 1, world 1"));
    CHECK(panel->list()->item(5)->text().contains("play-test copy"));
    CHECK(panel->entries()[0].complete && !panel->entries()[3].complete);
    CHECK(!panel->list()->item(0)->icon().isNull());
    CHECK(panel->summary()->text().contains("4 official world(s), 2 user world(s), 1 saved "
                                            "game(s), 1 player file(s), 1 recording(s)"));
    CHECK(panel->deployButton()->isEnabled());
    CHECK(!panel->rollBackButton()->isEnabled());
    // The recordings have a list of their own: a file under a recording's
    // name that is not one says so, a real one says how long it is and
    // opens in the recording window.
    {
        auto *recordings = panel->recordingList();
        CHECK(recordings != nullptr && !recordings->isHidden());
        CHECK(!recordings->accessibleName().isEmpty());
        CHECK_EQ(recordings->count(), 1);
        CHECK_EQ(recordings->item(0)->text(),
                 QString("DEMO200.BLP - replayed on world 200 - not a recording the games "
                         "would write"));
        QByteArray demo(204, '\0');
        demo[0] = 1;
        for (const int value : {10, 2, 600, 0}) {
            demo.append(static_cast<char>(value & 0xff));
            demo.append(static_cast<char>((value >> 8) & 0xff));
        }
        write(data.filePath("demo201.blp"), demo);
        panel->refresh();
        CHECK_EQ(recordings->count(), 2);
        CHECK_EQ(recordings->item(1)->text(),
                 QString("demo201.blp - replayed on world 201 - 600 step(s), about 30.0 s"));
        CHECK_EQ(panel->list()->count(), 6); // the world list is unchanged
        emit recordings->itemActivated(recordings->item(1));
        CHECK_EQ(window.demoViewers().size(), std::size_t{1});
        CHECK(window.demoViewers().front()->path() == data.filePath("demo201.blp"));
        CHECK(errors.isEmpty());
        window.demoViewers().front()->close();
        QFile::remove(data.filePath("demo201.blp"));
        panel->refresh();
        CHECK_EQ(recordings->count(), 1);
    }
    const auto untouched = directoryDigest(game.root);

    // Deploy as a user world: the first free number of player 1 is offered.
    WorldSlot offered;
    QString offeredDirectory;
    window.prompts.deploySlot = [&](DocumentTab *, WorldSlot suggested, const QString &where) {
        offered = suggested;
        offeredDirectory = where;
        return std::optional(suggested);
    };
    // Unsaved changes are deployed too: the world as it is now.
    {
        EditBuilder builder(tab->document());
        builder.setCell(SectionKind::foreground, 10, 9, 31);
        CHECK(tab->applyTransaction(builder.build("Edit")));
    }
    panel->deployButton()->click();
    CHECK(offered == (WorldSlot{true, 1, 2}));
    CHECK(offeredDirectory == data.absolutePath());
    CHECK(errors.isEmpty());
    CHECK(readAll(data.filePath("u001-002.blp")) == current(tab));
    CHECK(readAll(minePath) == raw(mine));  // the tab's own file is not saved by a deploy
    CHECK(tab->document().isModified());
    CHECK_EQ(panel->list()->count(), 7);
    CHECK(rowOf(panel->list(), "u001-002.blp") >= 0);
    CHECK(panel->rollBackButton()->isEnabled());
    CHECK(window.findChild<QAction *>("fileRollBack")->isEnabled());
    CHECK_EQ(window.deployStore().records().size(), std::size_t{1});

    // Roll back: asked first; "no" keeps it, "yes" removes the created file.
    bool confirm = false;
    DeployRecord asked;
    window.prompts.confirmRollBack = [&](const DeployRecord &record) {
        asked = record;
        return confirm;
    };
    panel->rollBackButton()->click();
    CHECK(asked.targetPath == data.filePath("u001-002.blp") && !asked.hadPrevious);
    CHECK(QFile::exists(data.filePath("u001-002.blp")));
    confirm = true;
    CHECK(window.rollBackLastDeploy());
    CHECK(directoryDigest(game.root) == untouched);
    CHECK_EQ(panel->list()->count(), 6);
    CHECK(!panel->rollBackButton()->isEnabled());
    CHECK(!window.rollBackLastDeploy()); // nothing left

    // In place of an official world: the existing spelling is kept, the old
    // bytes come back exactly.
    window.prompts.deploySlot = [](DocumentTab *, WorldSlot, const QString &) {
        return std::optional(WorldSlot{false, 1, 10});
    };
    const auto record = window.deployWorld(tab);
    CHECK(record.has_value());
    CHECK(record && record->targetPath == data.filePath("WORLD010.BLP") && record->hadPrevious);
    CHECK(readAll(data.filePath("WORLD010.BLP")) == current(tab));
    CHECK(!QFile::exists(data.filePath("world010.blp")) ||
          QFileInfo(data.filePath("world010.blp")).canonicalFilePath() ==
              QFileInfo(data.filePath("WORLD010.BLP")).canonicalFilePath());
    CHECK(window.rollBackLastDeploy());
    CHECK(readAll(data.filePath("WORLD010.BLP")) == hub);
    CHECK(directoryDigest(game.root) == untouched);

    // A slot the games do not use, and a file open in the editor.
    window.prompts.deploySlot = [](DocumentTab *, WorldSlot, const QString &) {
        return std::optional(WorldSlot{true, 9, 1});
    };
    errors.clear();
    CHECK(window.deployWorld(tab) == std::nullopt);
    CHECK(errors.size() == 1 && errors[0].contains("not a slot"));
    // Double click opens a listed world.
    emit panel->list()->itemActivated(panel->list()->item(rowOf(panel->list(), "WORLD011.BLP")));
    CHECK_EQ(window.tabCount(), 2);
    auto *official = window.currentTab();
    CHECK(official->file().path() == data.filePath("WORLD011.BLP"));
    // Its own slot is the one offered for it.
    CHECK(window.suggestedSlot(official, data.absolutePath()) == (WorldSlot{false, 1, 11}));
    window.prompts.deploySlot = [](DocumentTab *, WorldSlot, const QString &) {
        return std::optional(WorldSlot{false, 1, 11});
    };
    errors.clear();
    CHECK(window.deployWorld(tab) == std::nullopt);
    CHECK(errors.size() == 1 && errors[0].contains("is open in the editor"));
    // Cancelling.
    window.prompts.deploySlot = [](DocumentTab *, WorldSlot, const QString &) {
        return std::optional<WorldSlot>();
    };
    CHECK(window.deployWorld(tab) == std::nullopt);
    CHECK(directoryDigest(game.root) == untouched);
    CHECK(window.deployStore().records().empty());

    // Playtest hands over the configured command; nothing is started here.
    {
        QString program;
        QStringList arguments;
        QString workingDirectory;
        int started = 0;
        bool startResult = true;
        window.prompts.startProcess = [&](const QString &p, const QStringList &a,
                                          const QString &w) {
            ++started;
            program = p;
            arguments = a;
            workingDirectory = w;
            return startResult;
        };
        int askedForCommand = 0;
        window.prompts.playtestCommand = [&](GameProfile,
                                             const QString &) -> std::optional<QString> {
            ++askedForCommand;
            return QStringLiteral("wine \"C:/Games/Speedy Game/game.exe\" -nocd");
        };
        CHECK(!window.playtestCommandFor(GameProfile::speedyBlupi1).has_value());
        auto *action = window.findChild<QAction *>("filePlaytest");
        CHECK(action != nullptr && action->isEnabled());
        action->trigger();
        CHECK_EQ(askedForCommand, 1);
        CHECK_EQ(started, 1);
        CHECK(program == "wine");
        CHECK(arguments == (QStringList{"C:/Games/Speedy Game/game.exe", "-nocd"}));
        CHECK(workingDirectory == game.root);
        // Remembered per game: not asked again; the other game has none.
        panel->playtestButton()->click();
        CHECK_EQ(askedForCommand, 1);
        CHECK_EQ(started, 2);
        const auto command = window.playtestCommandFor(GameProfile::speedyBlupi1);
        CHECK(command && command->program == "wine" &&
              command->workingDirectory == game.root);
        CHECK(!window.playtestCommandFor(GameProfile::speedyBlupi2).has_value());
        // A command that does not start is reported.
        startResult = false;
        errors.clear();
        CHECK(!window.playtest());
        CHECK(errors.size() == 1 && errors[0].contains("could not be started"));
        // Cancelling the question starts nothing.
        QSettings().remove("playtest/command1");
        window.prompts.playtestCommand = [](GameProfile, const QString &) {
            return std::optional<QString>();
        };
        const auto before = started;
        CHECK(!window.playtest());
        CHECK_EQ(started, before);
    }
    CHECK(directoryDigest(game.root) == untouched);

    // The deploy dialog itself.
    {
        bool seen = false;
        QTimer::singleShot(0, &window, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            CHECK(dialog != nullptr);
            if (dialog == nullptr) {
                return;
            }
            seen = true;
            auto *user = dialog->findChild<QRadioButton *>("deployUser");
            auto *officialRadio = dialog->findChild<QRadioButton *>("deployOfficial");
            auto *gamer = dialog->findChild<QSpinBox *>("deployGamer");
            auto *number = dialog->findChild<QSpinBox *>("deployNumber");
            auto *outcome = dialog->findChild<QLabel *>("deployOutcome");
            CHECK(user->isChecked() && gamer->value() == 1 && number->value() == 2);
            CHECK(outcome->text().contains("u001-002.blp is created"));
            number->setValue(1);
            CHECK(outcome->text().contains("u001-001.blp exists and is replaced"));
            CHECK_EQ(number->maximum(), 20);
            CHECK_EQ(gamer->maximum(), 8);
            officialRadio->setChecked(true);
            CHECK(!gamer->isEnabled());
            number->setValue(10);
            CHECK(outcome->text().contains("world010.blp exists and is replaced"));
            CHECK(outcome->text().contains("game's own world numbers"));
            dialog->accept();
        });
        const auto slot = dialogPrompt(tab, WorldSlot{true, 1, 2}, data.absolutePath());
        CHECK(seen);
        CHECK(slot == (WorldSlot{false, 1, 10}));
    }
    QSettings().clear();
    return test::finish("installation_ui");
}
