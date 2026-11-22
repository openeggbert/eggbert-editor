// SPDX-License-Identifier: GPL-3.0-only
//
// Crash recovery and snapshots through the main window: unsaved bytes are
// kept in the per-user store, offered at the next start and restored
// byte-exactly as one undo step; nothing is written beside the world file.

#include "app/main_window.hpp"

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

#include <memory>

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

QByteArray current(DocumentTab *tab) {
    const auto bytes = tab->document().currentBytes();
    return QByteArray(reinterpret_cast<const char *>(bytes.data()),
                      static_cast<qsizetype>(bytes.size()));
}

bool edit(DocumentTab *tab, std::size_t x, std::size_t y, int value) {
    EditBuilder builder(tab->document());
    builder.setCell(SectionKind::foreground, x, y, static_cast<std::int16_t>(value));
    return tab->applyTransaction(builder.build("Edit"));
}

std::unique_ptr<MainWindow> makeWindow() {
    auto window = std::make_unique<MainWindow>();
    window->prompts.error = [](const QString &, const QString &) {};
    window->prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    window->prompts.confirmRevert = [](DocumentTab *) { return true; };
    window->prompts.recover = [](const RecoveryEntry &, bool, const QString &) {
        return RecoveryChoice::keep;
    };
    window->resize(1100, 800);
    window->show();
    return window;
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setOrganizationName("eggbert-editor-tests");
    QApplication::setApplicationName("recovery-ui-test");
    QSettings().clear();
    QDir(RecoveryStore::defaultDirectory()).removeRecursively();

    QTemporaryDir dir;
    QDir().mkpath(dir.filePath("worlds"));
    const auto path = dir.filePath("worlds/a.blp");
    const auto base = raw(patternedWorld(fxRevision13));
    writeAll(path, base);
    QByteArray edited;

    // A session that ends without closing its window.
    {
        auto window = makeWindow();
        CHECK(window->action("fileSnapshot") != nullptr);
        CHECK(window->action("fileSnapshots") != nullptr);
        CHECK(window->recoveryStore().directory() == RecoveryStore::defaultDirectory());
        CHECK_EQ(window->checkRecovery(), 0); // nothing to recover yet
        auto *tab = window->openPath(path);
        CHECK(tab != nullptr);
        CHECK(edit(tab, 3, 70, 153));
        CHECK(window->recoveryStore().autosaves().empty()); // not at once ...
        CHECK(QTest::qWaitFor([&] { return !window->recoveryStore().autosaves().empty(); },
                              6000)); // ... but a few seconds after the edit
        CHECK(edit(tab, 4, 70, 7));
        window->flushAutosave();
        edited = current(tab);
        CHECK(edited != base);
        const auto entries = window->recoveryStore().autosaves();
        CHECK_EQ(entries.size(), std::size_t{1});
        CHECK(*window->recoveryStore().read(entries[0]) == edited);
        // The world file and its folder are untouched.
        CHECK(readAll(path) == base);
        CHECK_EQ(QDir(dir.filePath("worlds")).entryList(QDir::Files).size(), 1);
        // Undoing everything leaves nothing to recover.
        CHECK(tab->document().undo());
        CHECK(tab->document().undo());
        window->flushAutosave();
        CHECK(window->recoveryStore().autosaves().empty());
        CHECK(tab->document().redo());
        CHECK(tab->document().redo());
        window->flushAutosave();
        CHECK_EQ(window->recoveryStore().autosaves().size(), std::size_t{1});
        // The window object goes away without a close: a crash.
    }

    // Next start: "ask again later" keeps the copy; Restore applies it.
    {
        auto window = makeWindow();
        int asked = 0;
        bool offeredRestore = false;
        window->prompts.recover = [&](const RecoveryEntry &entry, bool canRestore,
                                      const QString &detail) {
            ++asked;
            offeredRestore = canRestore;
            CHECK(entry.originalPath.endsWith("worlds/a.blp"));
            CHECK(detail.contains("unchanged"));
            return RecoveryChoice::keep;
        };
        CHECK_EQ(window->checkRecovery(), 0);
        CHECK_EQ(asked, 1);
        CHECK(offeredRestore);
        CHECK_EQ(window->tabCount(), 0);
        CHECK_EQ(window->recoveryStore().autosaves().size(), std::size_t{1});

        window->prompts.recover = [](const RecoveryEntry &, bool, const QString &) {
            return RecoveryChoice::restore;
        };
        CHECK_EQ(window->checkRecovery(), 1);
        CHECK_EQ(window->tabCount(), 1);
        auto *tab = window->currentTab();
        CHECK(tab->document().isModified());
        CHECK(current(tab) == edited);
        CHECK(readAll(path) == base); // still only in memory
        // One undo step takes the restored changes back.
        CHECK_EQ(tab->document().historySize(), std::size_t{1});
        CHECK(tab->document().undo());
        CHECK(current(tab) == base);
        CHECK(!tab->document().isModified());
        CHECK(tab->document().redo());
        // Saving makes the file equal and empties the store.
        CHECK(window->saveTab(tab));
        CHECK(readAll(path) == edited);
        CHECK(window->recoveryStore().autosaves().empty());
        CHECK_EQ(window->checkRecovery(), 0);
        // A clean close with discarded changes leaves nothing behind either.
        CHECK(edit(tab, 9, 9, 1));
        window->flushAutosave();
        CHECK_EQ(window->recoveryStore().autosaves().size(), std::size_t{1});
        CHECK(window->closeTab(tab));
        CHECK(window->recoveryStore().autosaves().empty());
        // So does Revert.
        tab = window->openPath(path);
        CHECK(edit(tab, 9, 9, 1));
        window->flushAutosave();
        CHECK(window->revertTab(tab));
        CHECK(window->recoveryStore().autosaves().empty());
        window->close();
    }

    // The world file changed after the crash: no Restore, a copy instead.
    {
        {
            auto window = makeWindow();
            auto *tab = window->openPath(path);
            CHECK(edit(tab, 20, 20, 9));
            window->flushAutosave();
            edited = current(tab);
        }
        auto onDisk = readAll(path);
        onDisk[1000] = static_cast<char>(onDisk[1000] ^ 0x01);
        writeAll(path, onDisk);

        auto window = makeWindow();
        bool offeredRestore = true;
        QString said;
        window->prompts.recover = [&](const RecoveryEntry &, bool canRestore, const QString &detail) {
            offeredRestore = canRestore;
            said = detail;
            return RecoveryChoice::restore; // even if asked for, it must not happen
        };
        CHECK_EQ(window->checkRecovery(), 0);
        CHECK(!offeredRestore);
        CHECK(said.contains("changed"));
        CHECK_EQ(window->tabCount(), 0);
        CHECK(readAll(path) == onDisk);
        CHECK_EQ(window->recoveryStore().autosaves().size(), std::size_t{1});
        const auto copyPath = dir.filePath("recovered.blp");
        QString suggested;
        window->prompts.saveAsPath = [&](const QString &suggestion) {
            suggested = suggestion;
            return copyPath;
        };
        window->prompts.recover = [](const RecoveryEntry &, bool, const QString &) {
            return RecoveryChoice::saveCopy;
        };
        CHECK_EQ(window->checkRecovery(), 0);
        CHECK(suggested.endsWith("a.blp.recovered"));
        CHECK(readAll(copyPath) == edited);
        CHECK(window->recoveryStore().autosaves().empty());
        CHECK(readAll(path) == onDisk);
        window->close();
    }

    // A vanished world file, Discard, and a damaged copy.
    {
        const auto gone = dir.filePath("worlds/gone.blp");
        writeAll(gone, base);
        {
            auto window = makeWindow();
            CHECK(edit(window->openPath(gone), 1, 1, 2));
            window->flushAutosave();
        }
        CHECK(QFile::remove(gone));
        auto window = makeWindow();
        QString said;
        bool offeredRestore = true;
        window->prompts.recover = [&](const RecoveryEntry &, bool canRestore, const QString &detail) {
            offeredRestore = canRestore;
            said = detail;
            return RecoveryChoice::discard;
        };
        CHECK_EQ(window->checkRecovery(), 0);
        CHECK(!offeredRestore);
        CHECK(said.contains("no longer exists"));
        CHECK(window->recoveryStore().autosaves().empty());

        writeAll(gone, base);
        {
            auto second = makeWindow();
            CHECK(edit(second->openPath(gone), 1, 1, 2));
            second->flushAutosave();
        }
        const auto entry = window->recoveryStore().autosaves().at(0);
        auto damagedBytes = readAll(entry.storePath);
        damagedBytes[damagedBytes.size() - 5] =
            static_cast<char>(damagedBytes[damagedBytes.size() - 5] ^ 0x10);
        writeAll(entry.storePath, damagedBytes);
        int errors = 0;
        int asked = 0;
        window->prompts.error = [&](const QString &, const QString &text) {
            ++errors;
            CHECK(text.contains("damaged"));
        };
        window->prompts.recover = [&](const RecoveryEntry &, bool, const QString &) {
            ++asked;
            return RecoveryChoice::restore;
        };
        CHECK_EQ(window->checkRecovery(), 0);
        CHECK_EQ(errors, 1);
        CHECK_EQ(asked, 0); // a damaged copy is never offered
        CHECK_EQ(window->tabCount(), 0);
        CHECK(readAll(gone) == base);
        QFile::remove(entry.storePath);
        window->close();
    }

    // Snapshots.
    {
        writeAll(path, base);
        auto window = makeWindow();
        auto *tab = window->openPath(path);
        CHECK(!window->manageSnapshots(tab)); // none yet
        std::optional<QString> name = QString("before the bridge");
        window->prompts.snapshotName = [&](DocumentTab *) { return name; };
        CHECK(edit(tab, 30, 30, 5));
        const auto atSnapshot = current(tab);
        CHECK(window->takeSnapshot(tab));
        name = std::nullopt;
        CHECK(!window->takeSnapshot(tab)); // cancelled
        CHECK(edit(tab, 31, 30, 6));
        CHECK(edit(tab, 32, 30, 7));
        const auto before = tab->document().historySize();
        std::vector<RecoveryEntry> listed;
        window->prompts.chooseSnapshot = [&](DocumentTab *, const std::vector<RecoveryEntry> &entries)
            -> std::optional<std::pair<RecoveryEntry, SnapshotAction>> {
            listed = entries;
            return std::make_pair(entries.front(), SnapshotAction::restore);
        };
        CHECK(window->manageSnapshots(tab));
        CHECK_EQ(listed.size(), std::size_t{1});
        CHECK_EQ(listed[0].name, QString("before the bridge"));
        CHECK(current(tab) == atSnapshot);
        CHECK_EQ(tab->document().historySize(), before + 1); // one undo step
        CHECK(tab->document().undo());
        CHECK(current(tab) != atSnapshot);
        // Restoring a state the world already has changes nothing.
        CHECK(tab->document().redo());
        CHECK(!window->manageSnapshots(tab));
        // The file on disk is not touched by any of this.
        CHECK(readAll(path) == base);
        CHECK_EQ(QDir(dir.filePath("worlds")).entryList(QDir::Files).size(), 2);
        // Cancel, then Delete.
        window->prompts.chooseSnapshot = [](DocumentTab *, const std::vector<RecoveryEntry> &)
            -> std::optional<std::pair<RecoveryEntry, SnapshotAction>> { return std::nullopt; };
        CHECK(!window->manageSnapshots(tab));
        window->prompts.chooseSnapshot = [](DocumentTab *, const std::vector<RecoveryEntry> &entries)
            -> std::optional<std::pair<RecoveryEntry, SnapshotAction>> {
            return std::make_pair(entries.front(), SnapshotAction::remove);
        };
        CHECK(window->manageSnapshots(tab));
        CHECK(window->recoveryStore().snapshots(path).empty());
        // A read-only world can be snapshotted but not changed by a restore.
        const auto oldPath = dir.filePath("worlds/old.blp");
        writeAll(oldPath, raw(blankWorld(fxRevision10)));
        auto *old = window->openPath(oldPath);
        name = QString("old");
        CHECK(window->takeSnapshot(old));
        window->prompts.chooseSnapshot = [](DocumentTab *, const std::vector<RecoveryEntry> &entries)
            -> std::optional<std::pair<RecoveryEntry, SnapshotAction>> {
            return std::make_pair(entries.front(), SnapshotAction::restore);
        };
        CHECK(!window->manageSnapshots(old));
        CHECK(!old->document().isModified());
        window->close();
    }
    QDir(RecoveryStore::defaultDirectory()).removeRecursively();
    return test::finish("recovery_ui");
}
