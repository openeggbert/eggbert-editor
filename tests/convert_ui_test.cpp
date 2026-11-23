// SPDX-License-Identifier: GPL-3.0-only
//
// File -> Convert Revision through the main window: a copy in another
// revision is written to a new file and opened; the source file and its tab
// stay as they were; a loss must be accepted; the source is never replaced.

#include "app/main_window.hpp"
#include "world/revision_convert.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QPushButton>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

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

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("eggbert-editor-test");
    QCoreApplication::setApplicationName("convert-ui");
    QTemporaryDir dir;

    MainWindow window;
    QStringList errors;
    window.prompts.error = [&](const QString &, const QString &text) { errors << text; };
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    window.resize(1100, 800);
    window.show();

    const auto dialogPrompt = window.prompts.convertRevision;
    auto *action = window.findChild<QAction *>("fileConvertRevision");
    CHECK(action != nullptr);
    CHECK(!action->isEnabled()); // nothing is open

    // A read-only 1.0 world becomes an editable 1.3 world in a new file.
    const auto oldWorld = patternedWorld(fxRevision10);
    const auto oldPath = dir.filePath("old.blp");
    writeAll(oldPath, raw(oldWorld));
    auto *oldTab = window.openPath(oldPath);
    CHECK(oldTab != nullptr);
    CHECK(action->isEnabled());
    CHECK(!oldTab->document().isWritable());
    oldTab->setProfile(GameProfile::speedyBlupi1);

    QString suggestedPath;
    const auto newPath = dir.filePath("new.blp");
    window.prompts.convertRevision = [](DocumentTab *) {
        return std::optional(std::make_pair(FormatRevision::v1_3, false));
    };
    window.prompts.saveAsPath = [&](const QString &suggested) {
        suggestedPath = suggested;
        return newPath;
    };
    action->trigger();
    CHECK(suggestedPath.endsWith("old-rev1.3.blp"));
    CHECK(errors.isEmpty());
    const auto expected = convertRevision(oldWorld, FormatRevision::v1_3);
    CHECK(expected.has_value());
    CHECK(readAll(newPath) == raw(expected->bytes));
    CHECK(readAll(oldPath) == raw(oldWorld));
    CHECK_EQ(window.tabCount(), 2);
    auto *newTab = window.currentTab();
    CHECK(newTab != oldTab);
    CHECK(newTab->file().path() == newPath);
    CHECK(newTab->document().isWritable());
    CHECK(!newTab->document().isModified());
    CHECK(newTab->profile() == GameProfile::speedyBlupi1); // carried over
    CHECK(current(oldTab) == raw(oldWorld));

    // Unsaved edits are converted too (the current state), and the source
    // tab keeps them unsaved.
    {
        EditBuilder builder(newTab->document());
        builder.setCell(SectionKind::foreground, 7, 8, 42);
        CHECK(newTab->applyTransaction(builder.build("Edit")));
    }
    const auto editedState = current(newTab);
    const auto thirdPath = dir.filePath("third.blp");
    window.prompts.convertRevision = [](DocumentTab *) {
        return std::optional(std::make_pair(FormatRevision::v1_2, false));
    };
    window.prompts.saveAsPath = [&](const QString &) { return thirdPath; };
    auto *third = window.convertRevisionOf(newTab);
    CHECK(third != nullptr);
    auto expectedThird = editedState;
    expectedThird[2] = 2;
    CHECK(readAll(thirdPath) == expectedThird);
    CHECK(newTab->document().isModified());
    CHECK(readAll(newPath) == raw(expected->bytes));

    // The source is never the output.
    window.prompts.saveAsPath = [&](const QString &) { return newPath; };
    errors.clear();
    CHECK(window.convertRevisionOf(newTab) == nullptr);
    CHECK_EQ(errors.size(), qsizetype{1});
    CHECK(errors[0].contains("never replaces its source"));
    CHECK(readAll(newPath) == raw(expected->bytes));
    // Nor a file open in another tab.
    window.prompts.saveAsPath = [&](const QString &) { return oldPath; };
    errors.clear();
    CHECK(window.convertRevisionOf(newTab) == nullptr);
    CHECK(errors.size() == 1 && errors[0].contains("is open in the editor"));
    CHECK(readAll(oldPath) == raw(oldWorld));

    // Something the target cannot store: refused, nothing written, unless
    // the loss was accepted in the dialog.
    {
        EditBuilder builder(third->document());
        builder.setCell(SectionKind::bigDecor, 5, 6, 20);
        CHECK(third->applyTransaction(builder.build("Edit")));
    }
    const auto lossPath = dir.filePath("loss.blp");
    window.prompts.saveAsPath = [&](const QString &) { return lossPath; };
    window.prompts.convertRevision = [](DocumentTab *) {
        return std::optional(std::make_pair(FormatRevision::v1_0, false));
    };
    errors.clear();
    CHECK(window.convertRevisionOf(third) == nullptr);
    CHECK_EQ(errors.size(), qsizetype{1});
    CHECK(errors[0].contains("bigDecor"));
    CHECK(!QFile::exists(lossPath));
    window.prompts.convertRevision = [](DocumentTab *) {
        return std::optional(std::make_pair(FormatRevision::v1_0, true));
    };
    errors.clear();
    auto *lossy = window.convertRevisionOf(third);
    CHECK(lossy != nullptr);
    CHECK(errors.isEmpty());
    CHECK_EQ(readAll(lossPath).size(), qsizetype{25'668});
    CHECK(!lossy->document().isWritable()); // 1.0 stays read-only in place

    // Cancelling either question writes nothing.
    const auto tabs = window.tabCount();
    window.prompts.convertRevision = [](DocumentTab *) {
        return std::optional<std::pair<FormatRevision, bool>>();
    };
    CHECK(window.convertRevisionOf(third) == nullptr);
    window.prompts.convertRevision = [](DocumentTab *) {
        return std::optional(std::make_pair(FormatRevision::v1_3, false));
    };
    window.prompts.saveAsPath = [](const QString &) { return QString(); };
    CHECK(window.convertRevisionOf(third) == nullptr);
    CHECK_EQ(window.tabCount(), tabs);
    CHECK(window.convertRevisionOf(nullptr) == nullptr);

    // The dialog itself: `third` is a 1.2 world with one bigDecor cell.
    {
        bool seen = false;
        QTimer::singleShot(0, &window, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            CHECK(dialog != nullptr);
            if (dialog == nullptr) {
                return;
            }
            seen = true;
            auto *target = dialog->findChild<QComboBox *>();
            auto *allowLoss = dialog->findChild<QCheckBox *>();
            auto *ok = dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
            // Every revision except the world's own is offered.
            CHECK_EQ(target->count(), 3);
            CHECK(target->findData(static_cast<int>(FormatRevision::v1_2)) < 0);
            const auto outcome = [&] {
                QStringList texts;
                for (const auto *label : dialog->findChildren<QLabel *>()) {
                    texts << label->text();
                }
                return texts.join('\n');
            };
            // 1.3 loses nothing.
            target->setCurrentIndex(target->findData(static_cast<int>(FormatRevision::v1_3)));
            CHECK(ok->isEnabled());
            CHECK(!allowLoss->isEnabled());
            // 1.0 has no bigDecor layer: OK only after the loss is accepted.
            target->setCurrentIndex(target->findData(static_cast<int>(FormatRevision::v1_0)));
            CHECK(!ok->isEnabled());
            CHECK(allowLoss->isEnabled() && !allowLoss->isChecked());
            CHECK(outcome().contains("1 bigDecor cell"));
            allowLoss->setChecked(true);
            CHECK(ok->isEnabled());
            CHECK(outcome().contains("Lost: 1 bigDecor cell"));
            // Another target clears the acceptance.
            target->setCurrentIndex(target->findData(static_cast<int>(FormatRevision::v1_1)));
            CHECK(!allowLoss->isChecked());
            CHECK(ok->isEnabled());
            target->setCurrentIndex(target->findData(static_cast<int>(FormatRevision::v1_0)));
            allowLoss->setChecked(true);
            dialog->accept();
        });
        const auto choice = dialogPrompt(third);
        CHECK(seen);
        CHECK(choice.has_value());
        CHECK(choice && choice->first == FormatRevision::v1_0 && choice->second);
    }

    return test::finish("convert_ui");
}
