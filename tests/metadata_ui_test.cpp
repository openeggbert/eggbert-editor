// SPDX-License-Identifier: GPL-3.0-only
#include "app/main_window.hpp"
#include "app/metadata_dialog.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

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

bool okEnabled(const MetadataDialog &dialog) {
    return dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->isEnabled();
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setOrganizationName("eggbert-editor-tests");
    QApplication::setApplicationName("metadata-ui-test");
    QSettings().clear();

    auto bytes = blankWorld(fxRevision13);
    putText(bytes, 376, "Caf\xE9");
    put16(bytes, 222, 3);
    put16(bytes, 224, 40); // outside both games' region ranges
    put32(bytes, 212, 100);
    put32(bytes, 216, 100);
    for (int index = 0; index < 4; ++index) {
        put32(bytes, 328 + 8 * index, 194);
        put32(bytes, 332 + 8 * index, 198);
        put32(bytes, 360 + 4 * index, index == 3 ? 9 : 2);
    }

    // The dialog on its own.
    {
        const WorldView world(bytes);
        MetadataDialog dialog(world, GameProfile::speedyBlupi1);
        auto *name = dialog.findChild<QLineEdit *>("nameEdit");
        auto *music = dialog.findChild<QSpinBox *>("musicSpin");
        auto *region = dialog.findChild<QSpinBox *>("regionSpin");
        CHECK_EQ(name->text(), QString::fromUtf8("Café"));
        CHECK_EQ(music->value(), 3);
        CHECK_EQ(region->value(), 40);
        // Unchanged values, including the out-of-range stored region, are valid
        // and reproduce the stored metadata exactly.
        CHECK(dialog.isValid());
        CHECK(okEnabled(dialog));
        CHECK(dialog.metadata().value() == readMetadata(world).value());
        CHECK(dialog.messages().contains("stored region 40"));
        // The unusual stored direction stays selectable.
        auto *direction4 = dialog.findChild<QComboBox *>("start4Direction");
        CHECK_EQ(direction4->count(), 3);
        CHECK_EQ(direction4->currentData().toInt(), 9);
        CHECK_EQ(dialog.findChild<QComboBox *>("start1Direction")->count(), 2);

        // A changed region must be in the profile's range.
        region->setValue(24);
        CHECK(!dialog.isValid());
        CHECK(!okEnabled(dialog));
        CHECK(dialog.messages().contains("Region 24 is outside the verified range 0 to 23"));
        region->setValue(23);
        CHECK(dialog.isValid());
        music->setValue(11);
        CHECK(!dialog.isValid());
        music->setValue(0);
        CHECK(dialog.isValid());

        // Title rules.
        name->setText(QString::fromUtf8("Čaj")); // not in Windows-1252
        CHECK(!dialog.isValid());
        CHECK(dialog.messages().contains("cannot be stored"));
        name->setText(QString(100, 'x'));
        CHECK(!dialog.isValid());
        name->setText(QString(60, 'x'));
        CHECK(dialog.isValid());
        CHECK(dialog.messages().contains("at most 40 characters"));
        name->setText(QString::fromUtf8("€ 5"));
        CHECK(dialog.isValid());
        CHECK((dialog.metadata()->name ==
               Bytes{std::byte{0x80}, std::byte{' '}, std::byte{'5'}}));

        dialog.findChild<QCheckBox *>("scrollYCheck")->setChecked(false);
        dialog.findChild<QSpinBox *>("start2X")->setValue(642);
        dialog.findChild<QComboBox *>("start2Direction")->setCurrentIndex(0);
        const auto edited = dialog.metadata().value();
        CHECK(!edited.scrollsVertically);
        CHECK(edited.scrollsHorizontally);
        CHECK_EQ(edited.music, std::int16_t{0});
        CHECK_EQ(edited.region, std::int16_t{23});
        CHECK((edited.starts[1] == StartInfo{{642, 198}, 1}));
        CHECK_EQ(edited.starts[3].direction, 9);
    }
    // SB2 allows regions up to 31; without a profile music/region are locked.
    {
        const WorldView world(bytes);
        MetadataDialog sb2(world, GameProfile::speedyBlupi2);
        sb2.findChild<QSpinBox *>("regionSpin")->setValue(31);
        CHECK(sb2.isValid());
        sb2.findChild<QSpinBox *>("regionSpin")->setValue(32);
        CHECK(!sb2.isValid());
        MetadataDialog unknown(world, GameProfile::unknown);
        CHECK(!unknown.findChild<QSpinBox *>("musicSpin")->isEnabled());
        CHECK(!unknown.findChild<QSpinBox *>("regionSpin")->isEnabled());
        CHECK(unknown.findChild<QLineEdit *>("nameEdit")->isEnabled());
        CHECK(unknown.isValid());
        CHECK(unknown.messages().contains("choose a game profile"));
        // A revision 1.2 file gets the "title is ignored" note.
        auto older = bytes;
        put16(older, 2, 2);
        MetadataDialog revision12(WorldView(older), GameProfile::speedyBlupi1);
        CHECK(revision12.messages().contains("ignore its stored title"));
        CHECK(!MetadataDialog(world, GameProfile::speedyBlupi1)
                   .messages()
                   .contains("ignore its stored title"));
    }

    // Through the window.
    QTemporaryDir dir;
    const auto path = dir.filePath("meta.blp");
    writeWorld(path, bytes);
    const auto oldPath = dir.filePath("old.blp");
    writeWorld(oldPath, blankWorld(fxRevision10));

    MainWindow window;
    window.prompts.error = [](const QString &, const QString &) {};
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    std::optional<Metadata> answer;
    int lastPage = -1;
    window.prompts.editMetadata = [&](DocumentTab *, int page) {
        lastPage = page;
        return answer;
    };
    window.resize(1100, 900);
    window.show();
    CHECK(QTest::qWaitForWindowExposed(&window));
    CHECK(!window.action("worldMetadata")->isEnabled());

    auto *tab = window.openPath(path);
    CHECK(tab != nullptr);
    QApplication::processEvents();
    const auto current = [&] {
        return Bytes(tab->document().currentBytes().begin(), tab->document().currentBytes().end());
    };
    const auto diff = [&] {
        std::vector<std::size_t> positions;
        const auto now = current();
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            if (now[index] != bytes[index]) {
                positions.push_back(index);
            }
        }
        return positions;
    };

    // Cancelled dialog: nothing happens.
    answer.reset();
    window.action("worldMetadata")->trigger();
    CHECK_EQ(lastPage, 0);
    CHECK(!tab->document().isModified());
    window.action("worldStarts")->trigger();
    CHECK_EQ(lastPage, 1);

    // Accepted dialog: one undo step named Edit Metadata, only music changes.
    auto edited = readMetadata(tab->document().view()).value();
    edited.music = 7;
    answer = edited;
    window.action("worldMetadata")->trigger();
    CHECK_EQ(tab->document().historySize(), std::size_t{1});
    CHECK_EQ(tab->document().undoLabel(), std::string_view("Edit Metadata"));
    CHECK((diff() == std::vector<std::size_t>{222}));
    // Unchanged answer: no new step.
    window.action("worldMetadata")->trigger();
    CHECK_EQ(tab->document().historySize(), std::size_t{1});
    window.action("editUndo")->trigger();
    CHECK(diff().empty());

    // Start tool: move start 2, then turn it around on its own cell.
    auto *canvas = tab->canvas();
    window.action("toolStart")->trigger();
    CHECK(tab->tool() == Tool::start);
    window.findChild<QSpinBox *>("startSpin")->setValue(2);
    CHECK_EQ(tab->activeStart(), std::size_t{1});
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 10, 20));
    {
        const auto header = tab->document().view().header().value();
        CHECK((header.blupiPos(1).value() == Point{642, 1286}));
        CHECK((header.blupiPos(0).value() == Point{194, 198}));
        CHECK_EQ(header.blupiDir(1).value(), 2);
        for (const auto position : diff()) {
            CHECK(position >= 336 && position < 344); // blupiPos[1] only
        }
        CHECK_EQ(tab->document().undoLabel(), std::string_view("Move Start 2"));
    }
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 10, 20));
    CHECK_EQ(tab->document().view().header()->blupiDir(1).value(), 1);
    CHECK_EQ(tab->document().undoLabel(), std::string_view("Turn Start 2"));
    QTest::mouseClick(canvas, Qt::LeftButton, {}, cellCentre(canvas, 10, 20));
    CHECK_EQ(tab->document().view().header()->blupiDir(1).value(), 2);
    // A drag does not smear the start across cells.
    QTest::mousePress(canvas, Qt::LeftButton, {}, cellCentre(canvas, 30, 30));
    QTest::mouseMove(canvas, cellCentre(canvas, 35, 30));
    QTest::mouseRelease(canvas, Qt::LeftButton, {}, cellCentre(canvas, 35, 30));
    CHECK((tab->document().view().header()->blupiPos(1).value() == Point{30 * 64 + 2, 30 * 64 + 6}));

    // Copy the selected start to all four.
    window.action("worldCopyStart")->trigger();
    for (std::size_t index = 0; index < 4; ++index) {
        CHECK((tab->document().view().header()->blupiPos(index).value() ==
               Point{30 * 64 + 2, 30 * 64 + 6}));
        CHECK_EQ(tab->document().view().header()->blupiDir(index).value(), 2);
    }
    CHECK_EQ(tab->document().undoLabel(), std::string_view("Copy Start 2 to All"));
    for (const auto position : diff()) {
        CHECK(position >= 328 && position < 376);
    }
    // The start tool never touched tiles or objects.
    CHECK(std::ranges::equal(tab->document().currentBytes().subspan(868),
                             std::span<const std::byte>(bytes).subspan(868)));

    // Read-only revision: metadata commands are unavailable and refuse.
    auto *old = window.openPath(oldPath);
    CHECK(old != nullptr);
    QApplication::processEvents();
    CHECK(!window.action("worldMetadata")->isEnabled());
    CHECK(!window.action("worldStarts")->isEnabled());
    CHECK(!window.action("toolStart")->isEnabled());
    CHECK(!window.editMetadata(0));
    CHECK(!old->placeStartAt({5, 5}));
    CHECK(!old->copyActiveStartToAll());
    CHECK(!old->applyMetadataEdit(Metadata{}));
    CHECK(!old->document().isModified());

    CHECK(window.close());
    return test::finish("metadata_ui");
}
