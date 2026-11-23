// SPDX-License-Identifier: GPL-3.0-only
//
// Experimental editing through the main window: off by default and then
// nothing is offered; switched on, Edit Bytes writes raw bytes as one undo
// step, asks a second time for preserved ranges, never touches the revision
// field or the length, and the metadata dialog accepts unverified numbers.

#include "app/main_window.hpp"
#include "app/metadata_dialog.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
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

QByteArray current(DocumentTab *tab) {
    const auto bytes = tab->document().currentBytes();
    return QByteArray(reinterpret_cast<const char *>(bytes.data()),
                      static_cast<qsizetype>(bytes.size()));
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
    QCoreApplication::setApplicationName("experimental-ui");
    QSettings().clear();
    QTemporaryDir dir;
    const auto world = blankWorld(fxRevision13, 8); // eight bytes after the layout
    const auto path = dir.filePath("a.blp");
    writeAll(path, raw(world));

    MainWindow window;
    QStringList errors;
    QStringList confirmations;
    bool confirm = false;
    window.prompts.error = [&](const QString &, const QString &text) { errors << text; };
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    window.prompts.confirmProtectedEdit = [&](DocumentTab *, const QString &text) {
        confirmations << text;
        return confirm;
    };
    window.resize(1100, 800);
    window.show();
    auto *tab = window.openPath(path);
    CHECK(tab != nullptr);
    tab->setProfile(GameProfile::speedyBlupi1);
    const auto dialogPrompt = window.prompts.editBytes;

    auto *action = window.findChild<QAction *>("editBytes");
    auto *label = window.findChild<QLabel *>("experimentalLabel");
    CHECK(action != nullptr && label != nullptr);

    // Off by default: not offered, and the entry point does nothing.
    CHECK(!window.experimental());
    CHECK(!action->isVisible() && !action->isEnabled());
    CHECK(!label->isVisible());
    int asked = 0;
    window.prompts.editBytes = [&](DocumentTab *, qulonglong) {
        ++asked;
        return std::optional(std::make_pair(qulonglong{222}, QByteArray::fromHex("0700")));
    };
    CHECK(!window.editBytes(tab));
    CHECK_EQ(asked, 0);
    CHECK(current(tab) == raw(world));
    {
        // The metadata dialog refuses a music number no game offers.
        MetadataDialog dialog(tab->document().view(), tab->profile());
        dialog.findChild<QSpinBox *>("musicSpin")->setValue(77);
        CHECK(!dialog.isValid());
    }

    // Switched on: said in the status bar, kept in the settings.
    window.setExperimental(true);
    CHECK(window.experimental());
    CHECK(action->isVisible() && action->isEnabled());
    CHECK(label->isVisible());
    CHECK(QSettings().value("experimental/enabled").toBool());

    // An ordinary field: one undo step, no second question.
    action->trigger();
    CHECK_EQ(asked, 1);
    auto expected = world;
    put16(expected, 222, 7);
    CHECK(current(tab) == raw(expected));
    CHECK(confirmations.isEmpty());
    CHECK(tab->undo());
    CHECK(current(tab) == raw(world));

    // A reserved field: asked again; "no" changes nothing, "yes" applies.
    window.prompts.editBytes = [&](DocumentTab *, qulonglong) {
        return std::optional(std::make_pair(qulonglong{226}, QByteArray::fromHex("ABCD")));
    };
    confirm = false;
    CHECK(!window.editBytes(tab));
    CHECK_EQ(confirmations.size(), qsizetype{1});
    CHECK(confirmations[0].contains("offsets 226 to 227 (reserve2)"));
    CHECK(current(tab) == raw(world));
    confirm = true;
    CHECK(window.editBytes(tab));
    expected = world;
    expected[226] = std::byte{0xAB};
    expected[227] = std::byte{0xCD};
    CHECK(current(tab) == raw(expected));
    CHECK(tab->document().isModified());
    CHECK(tab->undo());
    CHECK(current(tab) == raw(world));
    CHECK(tab->redo());
    CHECK(current(tab) == raw(expected));
    // It is saved like any other edit.
    CHECK(window.saveTab(tab));
    {
        QFile file(path);
        CHECK(file.open(QIODevice::ReadOnly));
        CHECK(file.readAll() == raw(expected));
    }

    // The bytes after the world: named in the question.
    confirmations.clear();
    window.prompts.editBytes = [&](DocumentTab *, qulonglong) {
        return std::optional(std::make_pair(qulonglong{50'470}, QByteArray::fromHex("11")));
    };
    CHECK(window.editBytes(tab));
    CHECK(confirmations.size() == 1 && confirmations[0].contains("bytes after the world"));
    CHECK_EQ(current(tab).size(), raw(world).size());
    CHECK(tab->undo());

    // The revision field is refused before any question.
    confirmations.clear();
    errors.clear();
    const auto before = current(tab);
    window.prompts.editBytes = [&](DocumentTab *, qulonglong) {
        return std::optional(std::make_pair(qulonglong{2}, QByteArray::fromHex("02")));
    };
    CHECK(!window.editBytes(tab));
    CHECK(errors.size() == 1 && errors[0].contains("revision field"));
    CHECK(confirmations.isEmpty());
    // Bytes that do not fit: the length never changes.
    errors.clear();
    window.prompts.editBytes = [&](DocumentTab *, qulonglong) {
        return std::optional(std::make_pair(qulonglong{50'475}, QByteArray::fromHex("0102")));
    };
    CHECK(!window.editBytes(tab));
    CHECK(errors.size() == 1 && errors[0].contains("do not fit"));
    // The same bytes as stored, and a cancelled dialog: nothing happens.
    window.prompts.editBytes = [&](DocumentTab *, qulonglong) {
        return std::optional(std::make_pair(qulonglong{226}, QByteArray::fromHex("ABCD")));
    };
    CHECK(!window.editBytes(tab));
    window.prompts.editBytes = [](DocumentTab *, qulonglong) {
        return std::optional<std::pair<qulonglong, QByteArray>>();
    };
    CHECK(!window.editBytes(tab));
    CHECK(current(tab) == before);

    // The dialog itself.
    {
        bool seen = false;
        QTimer::singleShot(0, &window, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            CHECK(dialog != nullptr);
            if (dialog == nullptr) {
                return;
            }
            seen = true;
            auto *offset = dialog->findChild<QLineEdit *>("editBytesOffset");
            auto *value = dialog->findChild<QLineEdit *>("editBytesValue");
            auto *summary = dialog->findChild<QLabel *>("editBytesSummary");
            auto *ok = dialog->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok);
            CHECK_EQ(offset->text(), QString("226")); // the offset it was given
            CHECK(!ok->isEnabled());                  // no bytes yet
            CHECK(summary->text().contains("AB CD")); // what is stored there
            value->setText("1");
            CHECK(!ok->isEnabled());
            value->setText("zz");
            CHECK(!ok->isEnabled());
            value->setText("01 02");
            CHECK(ok->isEnabled());
            offset->setText("0xC526"); // 50470: six bytes before the end
            CHECK(ok->isEnabled());
            value->setText("01 02 03 04 05 06 07");
            CHECK(!ok->isEnabled());
            CHECK(summary->text().contains("do not fit"));
            offset->setText("nonsense");
            CHECK(!ok->isEnabled());
            // Decimal or 0x hex only: a leading zero is not octal.
            offset->setText("0100");
            value->setText("01");
            CHECK(ok->isEnabled());
            CHECK(summary->text().contains("Stored there now"));
            offset->setText("0b11");
            CHECK(!ok->isEnabled());
            offset->setText("-5");
            CHECK(!ok->isEnabled());
            offset->setText("0xDE");
            value->setText("0a 00");
            CHECK(ok->isEnabled());
            dialog->accept();
        });
        const auto choice = dialogPrompt(tab, 226);
        CHECK(seen);
        CHECK(choice.has_value());
        CHECK(choice && choice->first == 222 && choice->second == QByteArray::fromHex("0a00"));
    }

    // The metadata dialog now takes the unverified number and says so.
    {
        MetadataDialog dialog(tab->document().view(), tab->profile());
        dialog.setExperimental(true);
        dialog.findChild<QSpinBox *>("musicSpin")->setValue(77);
        CHECK(dialog.isValid());
        CHECK(dialog.messages().contains("Experimental: music 77"));
        CHECK_EQ(dialog.metadata()->music, 77);
        dialog.findChild<QSpinBox *>("regionSpin")->setValue(60);
        CHECK(dialog.isValid());
        CHECK(dialog.messages().contains("Experimental: region 60"));
    }

    // Custom border rules: loaded from a text file, offered in the Adaptive
    // brush chooser, painted as one undo step.
    {
        auto *rulesAction = window.findChild<QAction *>("editBorderRules");
        CHECK(rulesAction != nullptr && rulesAction->isVisible() && rulesAction->isEnabled());
        auto *combo = window.findChild<QComboBox *>("adaptiveCombo");
        CHECK(combo != nullptr);
        const auto countBefore = combo->count();

        // A broken file is refused with its line, and nothing is offered.
        const auto badPath = dir.filePath("bad.rules");
        writeAll(badPath, "[Wall]\nforms = 1 2 3\n");
        errors.clear();
        CHECK(!window.loadCustomBorderRules(badPath));
        CHECK(errors.size() == 1 && errors[0].contains("Line 2"));
        CHECK_EQ(combo->count(), countBefore);
        CHECK(!window.loadCustomBorderRules(dir.filePath("missing.rules")));

        const auto rulesPath = dir.filePath("my.rules");
        writeAll(rulesPath,
                 "[Wall]\nforms = 500 501 502 503 504 505 506 507 508 509 510 511 512 513 514 "
                 "515\n");
        window.prompts.borderRulesPath = [&] { return rulesPath; };
        rulesAction->trigger();
        CHECK_EQ(window.customBorderRuleCount(), std::size_t{1});
        CHECK_EQ(combo->count(), countBefore + 1);
        const auto index = combo->findData(QString("custom:0"));
        CHECK(index >= 0);
        CHECK(combo->itemText(index) == "Custom: Wall");

        const auto beforeStroke = current(tab);
        tab->setTool(Tool::adaptive);
        tab->setAdaptiveBrush("custom:0");
        CHECK(tab->customBorderRule() != nullptr);
        const CellPos cells[] = {{40, 40}, {41, 40}};
        CHECK(tab->previewAdaptive(cells[0]).changedCount() == 1);
        CHECK(current(tab) == beforeStroke); // a preview writes nothing
        CHECK(tab->adaptiveAt(cells, 0));
        CHECK_EQ(*tab->document().view().foreground()->cell(40, 40), 504);
        CHECK_EQ(*tab->document().view().foreground()->cell(41, 40), 508);
        CHECK(tab->undo());
        CHECK(current(tab) == beforeStroke);

        // Without experimental editing the rules are not offered and the
        // brush falls back.
        window.setExperimental(false);
        CHECK_EQ(combo->count(), countBefore);
        CHECK(tab->customBorderRule() == nullptr);
        window.setExperimental(true);
        CHECK_EQ(combo->count(), countBefore + 1);
    }

    // A read-only world gets no byte editing either.
    const auto oldPath = dir.filePath("old.blp");
    writeAll(oldPath, raw(blankWorld(fxRevision10)));
    auto *oldTab = window.openPath(oldPath);
    CHECK(oldTab != nullptr);
    CHECK(action->isVisible() && !action->isEnabled());
    window.prompts.editBytes = [&](DocumentTab *, qulonglong) {
        return std::optional(std::make_pair(qulonglong{222}, QByteArray::fromHex("0700")));
    };
    CHECK(!window.editBytes(oldTab));

    // Switched off again.
    window.setExperimental(false);
    CHECK(!action->isVisible());
    CHECK(!label->isVisible());
    CHECK(!QSettings().value("experimental/enabled").toBool());
    QSettings().clear();
    return test::finish("experimental_ui");
}
