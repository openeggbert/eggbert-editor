// SPDX-License-Identifier: GPL-3.0-only
//
// The Mobile Eggbert text world window: painting, the header fields and the
// object table each change one token of the file, undo restores the exact
// text, and saving is atomic and refuses when the file changed on disk.

#include "app/main_window.hpp"
#include "app/text_world_editor.hpp"

#include "check.hpp"

#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>

using namespace eggbert;

namespace {

QByteArray fixture() {
    QByteArray text = "DescFile: posDecor=0;0 dimDecor=100;100 world=0 music=3 region=7 "
                      "blupiPos=66;774 blupiDir=2 \n";
    for (const auto *section : {"Decor: \n", "BigDecor: \n"}) {
        text += section;
        for (int x = 0; x < 100; ++x) {
            text += QByteArray(100, ',') + "\n";
        }
    }
    text += "MoveObject: type=5 stepAdvance=1 stepRecede=1 timeStopStart=0 timeStopEnd=0 "
            "posStart=258;196 posEnd=258;196 posCurrent=258;196 step=1 time=0 phase=0 "
            "channel=10 icon=0 \n";
    text += "MoveObject: type=1 posStart=640;832 icon=29 \n"; // a short record
    return text;
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

QByteArray text(const TextWorldEditor *editor) {
    return QByteArray::fromStdString(editor->document().text());
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("eggbert-editor-test");
    QCoreApplication::setApplicationName("text-world-editor");
    QTemporaryDir dir;
    const auto original = fixture();
    const auto path = dir.filePath("world011.txt");
    writeAll(path, original);

    MainWindow window;
    QStringList errors;
    window.prompts.error = [&](const QString &, const QString &message) { errors << message; };
    window.show();
    auto *editor = window.openTextWorld(path);
    CHECK(editor != nullptr);
    QStringList editorErrors;
    editor->showError = [&](const QString &message) { editorErrors << message; };
    CHECK(editor->isVisible());
    CHECK(editor->windowTitle().startsWith("world011.txt - "));
    CHECK(text(editor) == original);
    CHECK(!editor->saveButton()->isEnabled() && !editor->undoButton()->isEnabled());
    CHECK_EQ(editor->objectTable()->rowCount(), 2);
    CHECK_EQ(editor->problemList()->count(), 3); // the short record lacks three fields
    CHECK(!window.openTextWorld(dir.filePath("missing.txt")));
    CHECK(errors.size() == 1 && errors[0].contains("cannot be read"));

    // Painting a Decor cell writes one number into one entry.
    editor->valueSpin()->setValue(40);
    CHECK(editor->paintCell(3, 4));
    CHECK_EQ(text(editor).size(), original.size() + 2);
    CHECK_EQ(*editor->document().world().tile(SectionKind::foreground, 3, 4), 40);
    CHECK(editor->windowTitle().contains("*"));
    CHECK(editor->saveButton()->isEnabled() && editor->undoButton()->isEnabled());
    CHECK(!editor->paintCell(3, 4)); // the same number again is no edit
    // The other block, chosen in the combo.
    editor->layerCombo()->setCurrentIndex(1);
    editor->valueSpin()->setValue(20);
    CHECK(editor->paintCell(9, 9));
    CHECK_EQ(*editor->document().world().tile(SectionKind::bigDecor, 9, 9), 20);
    CHECK_EQ(*editor->document().world().tile(SectionKind::foreground, 9, 9), -1);
    // Picking takes a cell's number without changing the file.
    editor->layerCombo()->setCurrentIndex(0);
    const auto beforePick = text(editor);
    editor->pickCell(3, 4);
    CHECK_EQ(editor->valueSpin()->value(), 40);
    editor->pickCell(50, 50);
    CHECK_EQ(editor->valueSpin()->value(), -1);
    CHECK(text(editor) == beforePick);
    // -1 empties the cell again.
    CHECK(editor->paintCell(3, 4));
    CHECK_EQ(*editor->document().world().tile(SectionKind::foreground, 3, 4), -1);

    // Header fields.
    editor->findChild<QSpinBox *>("textWorldMusic")->setValue(9);
    CHECK_EQ(*editor->document().world().headerValue("music"), 9);
    editor->findChild<QSpinBox *>("textWorldStartX")->setValue(130);
    CHECK(*editor->document().world().headerPoint("blupiPos") == (Point{130, 774}));

    // The object table: a number, one half of a point, and what is refused.
    auto *table = editor->objectTable();
    CHECK_EQ(table->item(0, 0)->text(), QString("5"));
    table->item(0, 10)->setText("8"); // icon
    CHECK_EQ(*editor->document().world().objectValue(0, "icon"), 8);
    table->item(0, 7)->setText("400"); // posEnd x
    CHECK(*editor->document().world().objectPoint(0, "posEnd") == (Point{400, 196}));
    const auto beforeBad = text(editor);
    table->item(0, 0)->setText("not a number");
    CHECK(text(editor) == beforeBad);
    CHECK_EQ(table->item(0, 0)->text(), QString("5")); // the stored number is shown again
    // A field the file does not have is not editable.
    CHECK(table->item(1, 1)->text().isEmpty());
    CHECK((table->item(1, 1)->flags() & Qt::ItemIsEditable) == 0);
    CHECK((table->item(1, 0)->flags() & Qt::ItemIsEditable) != 0);

    // Undo all the way restores the file's text; redo brings the edits back.
    const auto edited = text(editor);
    while (editor->undoButton()->isEnabled()) {
        editor->undoButton()->click();
    }
    CHECK(text(editor) == original);
    CHECK(!editor->windowTitle().contains("*"));
    while (editor->redoButton()->isEnabled()) {
        editor->redoButton()->click();
    }
    CHECK(text(editor) == edited);

    // Saving writes exactly the text; nothing else appears in the folder.
    CHECK(readAll(path) == original); // nothing was written so far
    editor->saveButton()->click();
    CHECK(editorErrors.isEmpty());
    CHECK(readAll(path) == edited);
    CHECK(!editor->saveButton()->isEnabled());
    CHECK_EQ(QDir(dir.path()).entryList(QDir::Files).size(), qsizetype{1});
    CHECK(!editor->save()); // nothing to save

    // A file changed behind the window is not overwritten.
    editor->valueSpin()->setValue(12);
    CHECK(editor->paintCell(60, 61));
    const auto foreign = edited + "# someone else was here\n";
    writeAll(path, foreign);
    CHECK(!editor->save());
    CHECK(editorErrors.size() == 1 && editorErrors[0].contains("changed or removed on disk"));
    CHECK(readAll(path) == foreign);
    // Save As keeps the user's version under another name.
    const auto otherPath = dir.filePath("copy.txt");
    CHECK(editor->saveAs(otherPath));
    CHECK(readAll(otherPath) == text(editor));
    CHECK(readAll(path) == foreign);
    CHECK(editor->path() == otherPath);
    CHECK(editor->saveAs(dir.filePath("no/such/dir/x.txt")) == false);
    CHECK_EQ(editorErrors.size(), qsizetype{2});

    // Closing with unsaved changes asks; the main window asks through it.
    CHECK(editor->paintCell(70, 70));
    bool discard = false;
    int asked = 0;
    editor->confirmDiscard = [&] {
        ++asked;
        return discard;
    };
    CHECK(!editor->close());
    CHECK_EQ(asked, 1);
    CHECK(editor->isVisible());
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    CHECK(!window.close()); // refused by the text world's question
    CHECK_EQ(asked, 2);
    discard = true;
    CHECK(window.close());
    return test::finish("text_world_editor");
}
