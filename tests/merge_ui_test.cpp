// SPDX-License-Identifier: GPL-3.0-only
//
// File -> Merge Changes: what another copy changed since a common base is
// brought into the current tab as one undo step, every conflict decided by
// the user; bytes the editor preserves are never taken from the other file.

#include "app/main_window.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QAction>
#include <QApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
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

std::size_t cell(std::size_t x, std::size_t y) {
    return fxCellOffset(fxForegroundOffset, x, y);
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("eggbert-editor-test");
    QCoreApplication::setApplicationName("merge-ui");
    QTemporaryDir dir;

    const auto base = blankWorld(fxRevision13);
    auto mine = base;
    put16(mine, cell(1, 1), 30);   // only here
    put16(mine, cell(5, 5), 40);   // conflict
    put16(mine, 222, 3);           // conflict (music)
    auto theirs = base;
    put16(theirs, cell(2, 2), 31); // only there
    put16(theirs, cell(5, 5), 41);
    put16(theirs, 222, 4);
    put16(theirs, 226, 0x7777);    // a reserved field: preserved here
    const auto basePath = dir.filePath("base.blp");
    const auto minePath = dir.filePath("mine.blp");
    const auto theirsPath = dir.filePath("theirs.blp");
    writeAll(basePath, base);
    writeAll(minePath, mine);
    writeAll(theirsPath, theirs);

    MainWindow window;
    QStringList errors;
    window.prompts.error = [&](const QString &, const QString &text) { errors << text; };
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    window.resize(1100, 800);
    window.show();
    const auto filesDialog = window.prompts.mergeFiles;
    const auto resolveDialog = window.prompts.resolveMerge;

    auto *action = window.findChild<QAction *>("fileMerge");
    CHECK(action != nullptr && !action->isEnabled());
    auto *tab = window.openPath(minePath);
    CHECK(tab != nullptr && action->isEnabled());

    window.prompts.mergeFiles = [&](DocumentTab *) {
        return std::optional(std::make_pair(basePath, theirsPath));
    };
    MergeResult shown;
    std::vector<bool> answer{true, false}; // music from the other copy, the cell stays
    window.prompts.resolveMerge = [&](DocumentTab *, const MergeResult &result,
                                      std::span<const std::byte>, std::span<const std::byte>) {
        shown = result;
        return std::optional(answer);
    };
    action->trigger();
    CHECK(errors.isEmpty());
    // The reserved field is an element of its own that only the other copy
    // changed; it is counted but not taken.
    CHECK_EQ(shown.fromTheirs, std::size_t{2});
    CHECK_EQ(shown.fromMine, std::size_t{1});
    CHECK_EQ(shown.conflicts.size(), std::size_t{2});
    auto expected = mine;
    put16(expected, cell(2, 2), 31);
    put16(expected, 222, 4);
    CHECK(current(tab) == raw(expected));
    CHECK(tab->document().isModified());
    // One undo step; the files on disk are untouched.
    CHECK(tab->undo());
    CHECK(current(tab) == raw(mine));
    CHECK(!tab->document().canUndo());
    {
        QFile file(theirsPath);
        CHECK(file.open(QIODevice::ReadOnly) && file.readAll() == raw(theirs));
    }

    // Everything from the other copy; cancelling; and nothing new to take.
    answer = {true, true};
    CHECK(window.mergeInto(tab));
    put16(expected, cell(5, 5), 41);
    CHECK(current(tab) == raw(expected));
    CHECK(tab->undo());
    window.prompts.resolveMerge = [](DocumentTab *, const MergeResult &,
                                     std::span<const std::byte>, std::span<const std::byte>) {
        return std::optional<std::vector<bool>>();
    };
    CHECK(!window.mergeInto(tab));
    CHECK(current(tab) == raw(mine));
    int asked = 0;
    window.prompts.resolveMerge = [&](DocumentTab *, const MergeResult &,
                                      std::span<const std::byte>, std::span<const std::byte>) {
        ++asked;
        return std::optional(std::vector<bool>{});
    };
    window.prompts.mergeFiles = [&](DocumentTab *) {
        return std::optional(std::make_pair(basePath, basePath)); // the other copy is the base
    };
    CHECK(!window.mergeInto(tab));
    CHECK_EQ(asked, 0);

    // Files that cannot be merged are refused with the reason.
    const auto otherRevision = dir.filePath("rev12.blp");
    writeAll(otherRevision, blankWorld(fxRevision12));
    window.prompts.mergeFiles = [&](DocumentTab *) {
        return std::optional(std::make_pair(basePath, otherRevision));
    };
    errors.clear();
    CHECK(!window.mergeInto(tab));
    CHECK(errors.size() == 1 && errors[0].contains("same revision"));
    window.prompts.mergeFiles = [&](DocumentTab *) {
        return std::optional(std::make_pair(basePath, dir.filePath("missing.blp")));
    };
    errors.clear();
    CHECK(!window.mergeInto(tab));
    CHECK(errors.size() == 1 && errors[0].contains("cannot be read"));
    window.prompts.mergeFiles = [](DocumentTab *) {
        return std::optional<std::pair<QString, QString>>();
    };
    CHECK(!window.mergeInto(tab));
    CHECK(current(tab) == raw(mine));

    // A read-only world takes no merge.
    const auto oldPath = dir.filePath("old.blp");
    writeAll(oldPath, blankWorld(fxRevision10));
    auto *oldTab = window.openPath(oldPath);
    CHECK(oldTab != nullptr && !action->isEnabled());
    CHECK(!window.mergeInto(oldTab));
    window.setCurrentTab(tab);

    // The conflict dialog itself.
    {
        const auto result = mergeWorlds(base, mine, theirs, MergePreference::none);
        CHECK(result.has_value());
        bool seen = false;
        QTimer::singleShot(0, &window, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            CHECK(dialog != nullptr);
            if (dialog == nullptr) {
                return;
            }
            seen = true;
            auto *list = dialog->findChild<QListWidget *>("mergeConflicts");
            CHECK_EQ(list->count(), 2);
            CHECK(list->item(0)->text().contains("music"));
            CHECK(list->item(0)->text().contains("here: 03 00"));
            CHECK(list->item(0)->text().contains("other: 04 00"));
            CHECK(list->item(1)->checkState() == Qt::Unchecked); // this world's by default
            CHECK(dialog->findChild<QLabel *>("mergeSummary")->text().contains("2 element(s) "
                                                                               "were changed"));
            dialog->findChild<QPushButton *>("mergeAllTheirs")->click();
            CHECK(list->item(0)->checkState() == Qt::Checked);
            dialog->findChild<QPushButton *>("mergeAllMine")->click();
            CHECK(list->item(1)->checkState() == Qt::Unchecked);
            list->item(1)->setCheckState(Qt::Checked);
            dialog->accept();
        });
        const auto choices = resolveDialog(tab, *result, mine, theirs);
        CHECK(seen);
        CHECK(choices == (std::vector<bool>{false, true}));
    }
    // The file chooser dialog: nothing chosen means cancelled.
    {
        bool seen = false;
        QTimer::singleShot(0, &window, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            CHECK(dialog != nullptr);
            if (dialog != nullptr) {
                seen = true;
                dialog->accept();
            }
        });
        CHECK(!filesDialog(tab).has_value());
        CHECK(seen);
    }
    return test::finish("merge_ui");
}
