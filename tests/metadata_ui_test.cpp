// SPDX-License-Identifier: GPL-3.0-only
#include "app/game_assets.hpp"
#include "app/main_window.hpp"
#include "app/metadata_dialog.hpp"

#include "check.hpp"
#include "fixture.hpp"
#include "generated_game.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
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

    {
        const WorldView world(bytes);
        MetadataDialog dialog(world, GameProfile::mobileEggbert);
        dialog.setTextWorld(true);
        dialog.setGame(nullptr, nullptr);
        auto *music = dialog.findChild<QSpinBox *>("musicSpin");
        auto *scroll = dialog.findChild<QCheckBox *>("scrollXCheck");
        CHECK(music->isHidden() && dialog.musicList()->isHidden() && scroll->isHidden());
        music->setValue(7);
        scroll->setChecked(!scroll->isChecked());
        const auto unchanged = readMetadata(world);
        CHECK(dialog.metadata()->music == unchanged->music);
        CHECK(dialog.metadata()->scrollsHorizontally == unchanged->scrollsHorizontally);
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

    // A stored title without a terminator does not block other edits: it is
    // kept as long as the title text is untouched.
    {
        auto full = bytes;
        putText(full, 376, std::string(100, 'T'));
        const WorldView world(full);
        MetadataDialog dialog(world, GameProfile::speedyBlupi1);
        CHECK(dialog.isValid());
        CHECK(dialog.messages().contains("without a terminator"));
        dialog.findChild<QSpinBox *>("musicSpin")->setValue(5);
        const auto edited = dialog.metadata().value();
        CHECK_EQ(edited.name.size(), std::size_t{100});
        CHECK_EQ(edited.music, std::int16_t{5});
        // Typing a different over-long title is refused; a short one is fine.
        dialog.findChild<QLineEdit *>("nameEdit")->setText(QString(100, 'U'));
        CHECK(!dialog.isValid());
        dialog.findChild<QLineEdit *>("nameEdit")->setText("Short");
        CHECK(dialog.isValid());
        CHECK_EQ(dialog.metadata()->name.size(), std::size_t{5});
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
    // Background and music are chosen by picture and from a list, with the
    // numbers following, and the music can be listened to.
    {
        QTemporaryDir gameDir;
        const auto game = makeGame(gameDir.filePath("game"));
        addBackdrops(game, 3);
        addMusic(game);
        // Music 2 is a real tune: a note of half a second.
        {
            QFile tune(game.root + "/SOUND/MUSIC001.BLP");
            CHECK(tune.open(QIODevice::WriteOnly | QIODevice::Truncate));
            tune.write(QByteArray("MThd\0\0\0\x06\0\0\0\x01\x01\xE0MTrk\0\0\0\x0D"
                                  "\0\x90\x3C\x50\x83\x60\x80\x3C\0\0\xFF\x2F\0",
                                  35));
        }
        const auto assets = GameAssets::load(game.root, GameProfile::speedyBlupi1);
        const WorldView world(bytes);
        MetadataDialog dialog(world, GameProfile::speedyBlupi1);
        auto *music = dialog.findChild<QSpinBox *>("musicSpin");
        auto *region = dialog.findChild<QSpinBox *>("regionSpin");
        // Without a game directory: numbered swatches and plain names.
        CHECK_EQ(dialog.backgroundList()->count(), 24); // regions 0 to 23
        CHECK_EQ(dialog.musicList()->count(), 11);      // none and 1 to 10
        CHECK_EQ(dialog.musicList()->item(0)->text(), QString("No music"));
        CHECK_EQ(dialog.musicList()->item(2)->text(), QString("Music 2"));
        dialog.setGame(assets, assets);
        CHECK_EQ(dialog.musicList()->item(2)->text(), QString("Music 2 - 0:01"));
        CHECK_EQ(dialog.musicList()->item(3)->text(), QString("Music 3"));
        // The picture of region 1 is the game's backdrop, scaled.
        const auto picture = dialog.backgroundList()->item(1)->icon().pixmap(96, 72).toImage();
        CHECK(picture.pixelColor(10, 36) == backdropColor(1, false));
        CHECK(picture.pixelColor(86, 36) == backdropColor(1, true));
        // The lists show the stored numbers and set them.
        // The stored region (40) is outside the game's range: no picture is chosen.
        CHECK_EQ(region->value(), 40);
        CHECK_EQ(dialog.backgroundList()->currentRow(), -1);
        CHECK_EQ(dialog.musicList()->currentRow(), music->value());
        dialog.backgroundList()->setCurrentRow(2);
        CHECK_EQ(region->value(), 2);
        dialog.musicList()->setCurrentRow(2);
        CHECK_EQ(music->value(), 2);
        region->setValue(3);
        CHECK_EQ(dialog.backgroundList()->currentRow(), 3);
        music->setValue(5);
        CHECK_EQ(dialog.musicList()->currentRow(), 5);
        CHECK(dialog.metadata().has_value() && dialog.metadata()->region == 3 &&
              dialog.metadata()->music == 5);
        // Listening: Play asks for the chosen number, Stop and closing end it.
        auto *play = dialog.findChild<QPushButton *>("metadataMusicPlay");
        auto *stop = dialog.findChild<QPushButton *>("metadataMusicStop");
        CHECK(play != nullptr && !play->isEnabled());
        std::vector<int> played;
        int stopped = 0;
        dialog.setMusicControls(
            [&](int number) {
                played.push_back(number);
                return true;
            },
            [&] { ++stopped; }, [&] { return QString("line %1").arg(played.size()); });
        CHECK(play->isEnabled() && stop->isEnabled());
        dialog.musicList()->setCurrentRow(2);
        play->click();
        CHECK((played == std::vector<int>{2}));
        CHECK_EQ(dialog.findChild<QLabel *>("metadataMusicLine")->text(), QString("line 1"));
        stop->click();
        CHECK_EQ(stopped, 1);
        dialog.musicList()->setCurrentRow(0); // no music: Play only stops
        play->click();
        CHECK_EQ(played.size(), std::size_t{1});
        CHECK_EQ(stopped, 2);
        dialog.reject();
        CHECK_EQ(stopped, 3);
        // Without a game profile there is nothing to choose from.
        MetadataDialog unknown(world, GameProfile::unknown);
        CHECK(unknown.backgroundList()->isHidden() && unknown.musicList()->isHidden());
    }
    return test::finish("metadata_ui");
}
