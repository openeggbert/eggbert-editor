// SPDX-License-Identifier: GPL-3.0-only
#include "app/main_window.hpp"
#include "app/objects_panel.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QMenu>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

using namespace eggbert;
using namespace eggbert::test;

namespace {

void writeWorld(const QString &path, const Bytes &bytes) {
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<qsizetype>(bytes.size()));
}

QPoint cellCentre(const WorldCanvas *canvas, std::size_t x, std::size_t y) {
    const auto &transform = canvas->transform();
    return QPoint(static_cast<int>(transform.toScreenX(static_cast<double>(x) * 64 + 32)),
                  static_cast<int>(transform.toScreenY(static_cast<double>(y) * 64 + 32)));
}

void drag(WorldCanvas *canvas, CellPos from, CellPos to) {
    QTest::mousePress(canvas, Qt::LeftButton, {}, cellCentre(canvas, from.x, from.y));
    QTest::mouseMove(canvas, cellCentre(canvas, to.x, to.y));
    QTest::mouseRelease(canvas, Qt::LeftButton, {}, cellCentre(canvas, to.x, to.y));
}

ObjectRecord record(const DocumentTab *tab, std::size_t slot) {
    return tab->document().view().objects()->record(slot).value();
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setOrganizationName("eggbert-editor-tests");
    QApplication::setApplicationName("objects-ui-test");
    QSettings().clear();

    QTemporaryDir dir;
    auto bytes = blankWorld(fxRevision13);
    // Slot 0: an object of a type unknown to both games (type 150), carrying
    // opaque fields. Slot 1: free but with stale bytes.
    const auto slot0 = fxSlotOffset(fxRevision13, 0);
    put16(bytes, slot0, 150);
    put16(bytes, slot0 + 10, 0x7777);
    put32(bytes, slot0 + 12, 50 * 64);
    put32(bytes, slot0 + 16, 50 * 64);
    put32(bytes, slot0 + 20, 50 * 64);
    put32(bytes, slot0 + 24, 50 * 64);
    put32(bytes, slot0 + 28, 50 * 64);
    put32(bytes, slot0 + 32, 50 * 64);
    put16(bytes, slot0 + 42, 10);
    put16(bytes, fxSlotOffset(fxRevision13, 1) + 44, 0x1111);
    const auto path = dir.filePath("objects.blp");
    writeWorld(path, bytes);

    auto fullBytes = blankWorld(fxRevision13);
    for (std::size_t slot = 0; slot < 200; ++slot) {
        put16(fullBytes, fxSlotOffset(fxRevision13, slot), 5);
        put16(fullBytes, fxSlotOffset(fxRevision13, slot) + 42, 10);
    }
    const auto fullPath = dir.filePath("full.blp");
    writeWorld(fullPath, fullBytes);
    const auto oldPath = dir.filePath("old.blp");
    writeWorld(oldPath, blankWorld(fxRevision11));

    MainWindow window;
    window.prompts.error = [](const QString &, const QString &) {};
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    window.resize(1300, 900);
    window.show();
    CHECK(QTest::qWaitForWindowExposed(&window));

    auto *tab = window.openPath(path);
    CHECK(tab != nullptr);
    QStringList status;
    QObject::connect(tab, &DocumentTab::statusMessage,
                     [&](const QString &message) { status << message; });
    QApplication::processEvents();
    auto *canvas = tab->canvas();
    auto *panel = window.objectsPanel();
    auto *presetCombo = window.findChild<QComboBox *>("objectPresetCombo");
    auto *poolLabel = window.findChild<QLabel *>("poolLabel");
    const auto enabled = [&](const char *name) { return window.action(name)->isEnabled(); };

    // Without a chosen game there are no presets and nothing can be placed.
    CHECK_EQ(presetCombo->count(), 0);
    CHECK(!enabled("toolPlaceObject"));
    CHECK(enabled("toolObject"));
    tab->setTool(Tool::placeObject);
    CHECK(!tab->createObjectAt({5, 5}, {}).has_value());
    CHECK(status.join('\n').contains("Choose a game profile"));
    CHECK(!tab->document().isModified());
    CHECK(poolLabel->text().contains("Objects 1/200, 199 free"));
    CHECK(panel->capacityText().contains("in use 1"));
    CHECK_EQ(panel->tree()->topLevelItemCount(), 1);
    CHECK(panel->tree()->topLevelItem(0)->text(1).contains("Type 150"));

    // Choosing a profile offers exactly that game's presets.
    tab->setProfile(GameProfile::speedyBlupi1);
    CHECK_EQ(presetCombo->count(), 31);
    CHECK(presetCombo->findData("wasp") < 0);
    CHECK(enabled("toolPlaceObject"));
    CHECK(panel->tree()->topLevelItem(0)->text(1).contains("not verified for this game"));
    CHECK(!window.choosePreset("wasp"));
    tab->setProfile(GameProfile::speedyBlupi2);
    CHECK_EQ(presetCombo->count(), 42);
    CHECK(presetCombo->findData("wasp") >= 0);
    CHECK(window.findChild<QAction *>("objectCreate_wasp") != nullptr);

    // Place a static item with the tool: lowest free slot (1), engine-exact
    // record, the slot's stale bytes replaced.
    CHECK(window.choosePreset("treasure"));
    CHECK(tab->tool() == Tool::placeObject);
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 3, 70));
    CHECK_EQ(tab->selectedSlot().value(), std::size_t{1});
    {
        const ObjectRecord want{5, 1, 1, 0, 0, 0, {194, 4484}, {194, 4484}, {194, 4484},
                                1, 0, 0, 10, 0, 0};
        CHECK(record(tab, 1) == want);
        CHECK_EQ(tab->document().undoLabel(), std::string_view("Create Treasure chest"));
        // Only slot 1's 48 bytes changed.
        const auto current = tab->document().currentBytes();
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            const bool inSlot = index >= 40'916 && index < 40'964;
            if (!inSlot) {
                CHECK(current[index] == bytes[index]);
            }
        }
    }
    CHECK(poolLabel->text().contains("Objects 2/200"));
    CHECK_EQ(panel->tree()->topLevelItemCount(), 2);
    CHECK(panel->tree()->currentItem()->text(1).contains("Treasure chest"));

    // Place a mover by dragging to its end: path and durations as the game.
    CHECK(window.choosePreset("lift-slow"));
    drag(canvas, {10, 20}, {10, 15});
    CHECK_EQ(tab->selectedSlot().value(), std::size_t{2});
    {
        const auto lift = record(tab, 2);
        CHECK_EQ(lift.type, std::int16_t{1});
        CHECK((lift.posStart == Point{640, 1280}));
        CHECK((lift.posEnd == Point{640, 960}));
        CHECK((lift.posCurrent == Point{640, 1280}));
        CHECK_EQ(lift.stepAdvance, std::int16_t{100});
        CHECK_EQ(lift.icon, std::int16_t{29});
    }
    // A static preset ignores a drag.
    CHECK(window.choosePreset("egg"));
    drag(canvas, {30, 30}, {33, 30});
    CHECK((record(tab, 3).posEnd == record(tab, 3).posStart));
    CHECK((record(tab, 3).posStart == Point{30 * 64 + 2, 30 * 64 + 8}));

    // Object tool: select by clicking, move by dragging whole cells.
    window.action("toolObject")->trigger();
    CHECK(tab->tool() == Tool::object);
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 3, 70));
    CHECK_EQ(tab->selectedSlot().value(), std::size_t{1});
    CHECK_EQ(canvas->selectedSlot().value(), std::size_t{1});
    drag(canvas, {3, 70}, {5, 68});
    {
        const auto moved = record(tab, 1);
        CHECK((moved.posStart == Point{194 + 128, 4484 - 128}));
        CHECK((moved.posEnd == moved.posStart));
        CHECK((moved.posCurrent == moved.posStart));
        CHECK_EQ(tab->document().undoLabel(), std::string_view("Move Object in Slot 1"));
    }
    // Moving a path object translates start, end and current together.
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 10, 20));
    CHECK_EQ(tab->selectedSlot().value(), std::size_t{2});
    drag(canvas, {10, 20}, {12, 20});
    CHECK((record(tab, 2).posStart == Point{768, 1280}));
    CHECK((record(tab, 2).posEnd == Point{768, 960}));
    CHECK_EQ(record(tab, 2).stepAdvance, std::int16_t{100}); // durations untouched
    // Dragging the end box changes only the end point.
    drag(canvas, {12, 15}, {12, 12});
    CHECK((record(tab, 2).posEnd == Point{768, 768}));
    CHECK((record(tab, 2).posStart == Point{768, 1280}));
    CHECK_EQ(tab->document().undoLabel(), std::string_view("Move End of Slot 2"));
    // Clicking empty space clears the selection.
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 90, 5));
    CHECK(!tab->selectedSlot().has_value());

    // Overlapping records: repeated clicks cycle in slot order.
    CHECK(window.choosePreset("treasure"));
    CHECK(tab->createObjectAt({60, 60}, {}).has_value()); // slot 4
    CHECK(tab->createObjectAt({60, 60}, {}).has_value()); // slot 5
    window.action("toolObject")->trigger();
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 60, 60));
    CHECK_EQ(tab->selectedSlot().value(), std::size_t{4});
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 60, 60));
    CHECK_EQ(tab->selectedSlot().value(), std::size_t{5});
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 60, 60));
    CHECK_EQ(tab->selectedSlot().value(), std::size_t{4});

    // Keyboard nudge and duplicate through the Object menu.
    window.action("objectNudgeRight")->trigger();
    CHECK((record(tab, 4).posStart == Point{61 * 64 + 2, 60 * 64 + 4}));
    window.action("objectNudgeUp")->trigger();
    CHECK((record(tab, 4).posStart == Point{61 * 64 + 2, 59 * 64 + 4}));
    window.action("objectDuplicate")->trigger();
    CHECK_EQ(tab->selectedSlot().value(), std::size_t{6});
    CHECK((record(tab, 6).posStart == Point{62 * 64 + 2, 59 * 64 + 4}));
    CHECK_EQ(record(tab, 6).type, std::int16_t{5});

    // Numeric entry in the Objects panel: one field, one undo step.
    tab->setSelectedSlot(2);
    QApplication::processEvents();
    auto *advance = panel->fieldEditor(ObjectField::stepAdvance);
    CHECK_EQ(advance->value(), 100);
    CHECK(advance->isEnabled());
    const auto historyBeforeEntry = tab->document().historySize();
    advance->setValue(77);
    panel->commitEditors();
    CHECK_EQ(record(tab, 2).stepAdvance, std::int16_t{77});
    CHECK_EQ(tab->document().historySize(), historyBeforeEntry + 1);
    panel->fieldEditor(ObjectField::posEnd, 1)->setValue(640);
    panel->commitEditors();
    CHECK((record(tab, 2).posEnd == Point{768, 640}));
    panel->fieldEditor(ObjectField::icon)->setValue(153);
    panel->commitEditors();
    CHECK_EQ(record(tab, 2).icon, std::int16_t{153});
    panel->commitEditors(); // nothing differs: no new step
    CHECK_EQ(tab->document().historySize(), historyBeforeEntry + 3);
    CHECK(window.findChild<QLabel *>("storedFieldsLabel")->text().contains("type 1"));

    // The unknown-type record: visible, selectable, not editable or
    // movable, but deletable; deleting clears only its type.
    tab->setSelectedSlot(0);
    QApplication::processEvents();
    CHECK(!tab->canEditObject(0));
    CHECK(tab->canDeleteObject(0));
    CHECK(!panel->fieldEditor(ObjectField::stepAdvance)->isEnabled());
    CHECK(!enabled("objectDuplicate"));
    CHECK(!enabled("objectNudgeLeft"));
    CHECK(enabled("objectDelete"));
    CHECK(!tab->moveSelectedObject({64, 0}));
    CHECK(!tab->setObjectNumber(0, ObjectField::icon, 5));
    CHECK(!tab->duplicateSelectedObject());
    // Fields that are never offered for editing.
    CHECK(!tab->setObjectNumber(2, ObjectField::type, 9));
    CHECK(!tab->setObjectNumber(2, ObjectField::unknown1, 9));
    CHECK(!tab->setObjectNumber(2, ObjectField::phase, 9));
    const auto beforeDelete = toVector(tab->document().currentBytes());
    window.action("objectDelete")->trigger();
    CHECK(!tab->selectedSlot().has_value());
    {
        const auto current = tab->document().currentBytes();
        for (std::size_t index = 0; index < beforeDelete.size(); ++index) {
            if (index != 40'868 && index != 40'869) {
                CHECK(current[index] == beforeDelete[index]);
            }
        }
        CHECK(record(tab, 0).isFree());
        CHECK_EQ(record(tab, 0).unknown1, std::int16_t{0x7777}); // stale bytes stay
    }
    // The freed slot 0 is the next one used; undo restores the deleted record.
    CHECK(window.choosePreset("goal"));
    CHECK_EQ(tab->createObjectAt({1, 1}, {}).value(), std::size_t{0});
    window.action("editUndo")->trigger();
    window.action("editUndo")->trigger();
    CHECK_EQ(record(tab, 0).type, std::int16_t{150});
    CHECK(std::ranges::equal(tab->document().currentBytes(), beforeDelete));

    // Show free slots lists all 200.
    window.findChild<QCheckBox *>("showFreeCheck")->setChecked(true);
    CHECK_EQ(panel->tree()->topLevelItemCount(), 200);
    window.findChild<QCheckBox *>("showFreeCheck")->setChecked(false);

    // A full pool: creation and duplication fail as a whole with a reason.
    auto *full = window.openPath(fullPath);
    CHECK(full != nullptr);
    QStringList fullStatus;
    QObject::connect(full, &DocumentTab::statusMessage,
                     [&](const QString &message) { fullStatus << message; });
    full->setProfile(GameProfile::speedyBlupi1);
    QApplication::processEvents();
    CHECK(poolLabel->text().contains("Objects 200/200, 0 free"));
    CHECK(window.objectsPanel()->capacityText().contains("hard limit"));
    CHECK(window.choosePreset("treasure"));
    CHECK(!full->createObjectAt({9, 9}, {}).has_value());
    CHECK(fullStatus.join('\n').contains("pool is full (200 of 200"));
    full->setSelectedSlot(10);
    CHECK(!full->duplicateSelectedObject());
    CHECK(!full->document().isModified());
    // Deleting one makes room for exactly one.
    CHECK(full->deleteSelectedObject());
    CHECK(window.objectsPanel()->capacityText().contains("Low headroom"));
    CHECK_EQ(full->createObjectAt({9, 9}, {}).value(), std::size_t{10});
    CHECK(!full->createObjectAt({9, 10}, {}).has_value());

    // Read-only revision: object commands unavailable.
    auto *old = window.openPath(oldPath);
    CHECK(old != nullptr);
    old->setProfile(GameProfile::speedyBlupi1);
    QApplication::processEvents();
    CHECK(!enabled("toolPlaceObject"));
    CHECK(!enabled("objectDelete"));
    CHECK(poolLabel->text().contains("Objects 0/100"));
    CHECK(!old->createObjectAt({1, 1}, {}).has_value());
    CHECK(!old->document().isModified());

    CHECK(window.close());
    return test::finish("objects_ui");
}
