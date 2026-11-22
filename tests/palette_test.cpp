// SPDX-License-Identifier: GPL-3.0-only
//
// The visual chooser: tiles, verified objects and border brushes as
// pictures, schematic without game data and drawn from generated sheets
// with it. Choosing must only change the tool state, never a document.

#include "app/main_window.hpp"
#include "app/music_player.hpp"
#include "app/palette_panel.hpp"
#include "app/sprite_icons.hpp"
#include "app/world_renderer.hpp"
#include "world/adaptive.hpp"
#include "world/editor_groups.hpp"
#include "world/object_catalog.hpp"

#include "check.hpp"
#include "fixture.hpp"
#include "generated_game.hpp"

#include <QAction>
#include <QApplication>
#include <QAbstractButton>
#include <QComboBox>
#include <QDockWidget>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>

#include <set>

using namespace eggbert;
using namespace eggbert::test;

namespace {

QListWidgetItem *itemWith(QListWidget *list, const QVariant &data) {
    for (int row = 0; row < list->count(); ++row) {
        if (list->item(row)->data(Qt::UserRole) == data) {
            return list->item(row);
        }
    }
    return nullptr;
}

QColor iconPixel(const QListWidgetItem *item, int size, int x, int y) {
    return item->icon().pixmap(size, size).toImage().pixelColor(x, y);
}

void click(QListWidget *list, QListWidgetItem *item) {
    list->scrollToItem(item);
    QApplication::processEvents();
    QTest::mouseClick(list->viewport(), Qt::LeftButton, Qt::NoModifier,
                      list->visualItemRect(item).center());
    QApplication::processEvents();
}

void writeWorld(const QString &path, const Bytes &bytes) {
    write(path, QByteArray(reinterpret_cast<const char *>(bytes.data()),
                           static_cast<qsizetype>(bytes.size())));
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setOrganizationName("eggbert-editor-tests");
    QApplication::setApplicationName("palette-test");
    QSettings().clear();

    QTemporaryDir dir;
    auto game = makeGame(dir.filePath("game"));
    // Make one tile's picture blank, as unused entries of real sheets are.
    {
        QPainter painter(&game.tiles.image);
        const auto &blankSprite = game.tiles.sprites[30];
        painter.fillRect(blankSprite.srcX, blankSprite.srcY, blankSprite.width,
                         blankSprite.height, blue);
    }
    saveBitmap(game.tiles.image, game.root + "/IMAGE08/OBJECT.BLP");
    addBackdrops(game, 3);
    addMusic(game);
    const auto digest = directoryDigest(game.root);
    const auto assets = GameAssets::load(game.root, GameProfile::speedyBlupi1);
    CHECK(assets->usable());

    // Thumbnails on their own.
    {
        const int size = 48;
        const auto schematic = tileThumbnail(SectionKind::foreground, 9, nullptr, size).toImage();
        CHECK(schematic.size() == QSize(size, size));
        CHECK(schematic.pixelColor(2, 2) == schematicTileColor(SectionKind::foreground, 9));
        const auto drawn =
            tileThumbnail(SectionKind::foreground, 9, assets.get(), size).toImage();
        CHECK(drawn.pixelColor(2, 2) == spriteColor(0, 9));
        CHECK(drawn.pixelColor(size - 3, size - 3) == spriteColor(0, 9));
        // The trimmed sprite keeps its offset inside the cell: 40 x 20 at (12, 34).
        const auto trimmed =
            tileThumbnail(SectionKind::foreground, 5, assets.get(), 64).toImage();
        CHECK(trimmed.pixelColor(5, 5) == schematicBackgroundColor());
        CHECK(trimmed.pixelColor(30, 44) == spriteColor(0, 5));
        // A number the artwork lacks falls back to the numbered swatch.
        CHECK(tileThumbnail(SectionKind::foreground, 400, assets.get(), size)
                  .toImage()
                  .pixelColor(2, 2) == schematicTileColor(SectionKind::foreground, 400));
        CHECK(tileHasArtwork(SectionKind::foreground, 30, assets.get()));
        CHECK(!tileHasVisibleArtwork(SectionKind::foreground, 30, assets.get()));
        CHECK(tileHasVisibleArtwork(SectionKind::foreground, 6, assets.get()));
        CHECK(!tileHasVisibleArtwork(SectionKind::foreground, 9, nullptr));
        CHECK(!tileHasArtwork(SectionKind::foreground, -1, assets.get()));
        // bigDecor comes from its own sheet; a big picture is scaled to fit.
        const auto big = tileThumbnail(SectionKind::bigDecor, 20, assets.get(), size).toImage();
        CHECK(big.pixelColor(size / 2, size / 2) == spriteColor(2, 20));
        // Objects: stored channel and icon, or a swatch naming the type.
        CHECK(objectThumbnail(5, 10, 3, assets.get(), size).toImage().pixelColor(24, 24) ==
              spriteColor(1, 3));
        CHECK(objectThumbnail(5, 10, 3, nullptr, size).toImage().pixelColor(2, 2) ==
              schematicObjectColor(5));
        CHECK(objectHasArtwork(10, 3, assets.get()));
        CHECK(!objectHasArtwork(10, 3, nullptr));
        CHECK(!objectHasArtwork(7, 3, assets.get()));
    }

    // The panel alone.
    {
        PalettePanel panel;
        CHECK(panel.hintVisible());
        CHECK(panel.hintText().contains("Choose the game"));
        CHECK_EQ(panel.tileList()->count(), 1 + 256);
        CHECK_EQ(panel.objectList()->count(), 0);
        CHECK(!panel.tabs()->isTabEnabled(1));
        CHECK(!panel.tabs()->isTabEnabled(2));

        panel.setContext(GameProfile::speedyBlupi1, SectionKind::foreground, nullptr);
        CHECK(panel.hintVisible());
        CHECK(panel.hintText().contains("schematic"));
        CHECK(!panel.showsArtwork());
        CHECK_EQ(panel.tileList()->count(), 1 + 246);
        CHECK_EQ(panel.objectList()->count(),
                 static_cast<int>(objectPresets(GameProfile::speedyBlupi1).size()));
        CHECK_EQ(panel.brushList()->count(),
                 static_cast<int>(adaptiveFamilies(GameProfile::speedyBlupi1).size()));
        CHECK(panel.tabs()->isTabEnabled(1));
        CHECK_EQ(panel.tileList()->item(0)->data(Qt::UserRole).toInt(), -1);
        CHECK(iconPixel(itemWith(panel.tileList(), 9), 48, 2, 2) ==
              schematicTileColor(SectionKind::foreground, 9));
        CHECK(itemWith(panel.tileList(), 9)->toolTip().contains("9"));
        CHECK(!itemWith(panel.objectList(), "treasure")->text().isEmpty());

        // bigDecor: its own numbers plus 203.
        panel.setContext(GameProfile::speedyBlupi1, SectionKind::bigDecor, nullptr);
        CHECK_EQ(panel.tileList()->count(), 1 + 54 + 1);
        CHECK(itemWith(panel.tileList(), 203) != nullptr);
        CHECK(itemWith(panel.tileList(), 54) == nullptr);

        // With artwork: pictures, no hint, blank entries left out.
        panel.setContext(GameProfile::speedyBlupi1, SectionKind::foreground, assets);
        CHECK(panel.showsArtwork());
        CHECK(!panel.hintVisible());
        CHECK_EQ(panel.tileList()->count(), 1 + 245);
        CHECK(itemWith(panel.tileList(), 30) == nullptr);
        CHECK(iconPixel(itemWith(panel.tileList(), 9), 48, 2, 2) == spriteColor(0, 9));
        const auto *treasure = findObjectPreset(GameProfile::speedyBlupi1, "treasure");
        CHECK(treasure != nullptr);
        CHECK(iconPixel(itemWith(panel.objectList(), "treasure"), 56, 28, 28) ==
              spriteColor(1, treasure->icon));

        // Marking the current choice emits nothing.
        int emitted = 0;
        QObject::connect(&panel, &PalettePanel::tileChosen, [&](int) { ++emitted; });
        QObject::connect(&panel, &PalettePanel::presetChosen, [&](const QString &) { ++emitted; });
        panel.setCurrentTile(12);
        panel.setCurrentPreset("treasure");
        CHECK_EQ(emitted, 0);
        CHECK_EQ(panel.tileList()->currentItem()->data(Qt::UserRole).toInt(), 12);
        panel.setCurrentTile(30); // not offered: nothing is marked
        CHECK(panel.tileList()->currentItem() == nullptr);

        // Groups: one per button of the game's own editor, then the rest.
        panel.setContext(GameProfile::speedyBlupi1, SectionKind::foreground, nullptr);
        CHECK_EQ(panel.tileGroupList()->count(), 5 + 3);
        CHECK_EQ(panel.objectGroupList()->count(), 1 + 4);
        CHECK(itemWith(panel.tileGroupList(), "scenery") != nullptr);
        CHECK(itemWith(panel.tileGroupList(), "lifts-enemies") == nullptr); // objects only
        CHECK(itemWith(panel.objectGroupList(), "lifts-enemies") != nullptr);
        CHECK_EQ(panel.currentTileGroup(), QString(PalettePanel::allForegroundGroup));
        CHECK(!panel.showTileGroup("no-such-group"));

        int chosenLayer = -1;
        int chosenValue = -99;
        QString chosenBrush;
        QObject::connect(&panel, &PalettePanel::tileChosen, [&](int layer, int value) {
            chosenLayer = layer;
            chosenValue = value;
        });
        QObject::connect(&panel, &PalettePanel::brushChosen,
                         [&](const QString &id) { chosenBrush = id; });
        panel.resize(320, 800);
        panel.show();
        QApplication::processEvents();
        // A bigDecor group: its seven kinds, 29 pictures, after the empty cell.
        CHECK(panel.showTileGroup("scenery"));
        CHECK_EQ(panel.tileList()->count(), 1 + 29);
        CHECK(itemWith(panel.tileList(), 20)->toolTip().startsWith("Tree"));
        click(panel.tileList(), itemWith(panel.tileList(), 20));
        CHECK_EQ(chosenLayer, static_cast<int>(SectionKind::bigDecor));
        CHECK_EQ(chosenValue, 20);
        // A tile the games' editors reshape is painted with its brush.
        CHECK(panel.showTileGroup("rock-tech"));
        chosenValue = -99;
        click(panel.tileList(), itemWith(panel.tileList(), 153));
        CHECK_EQ(chosenBrush, QString("rock"));
        CHECK_EQ(chosenValue, -99);
        // ... while a plain tile of the same group is just picked.
        click(panel.tileList(), itemWith(panel.tileList(), 185));
        CHECK_EQ(chosenValue, 185);
        CHECK_EQ(chosenLayer, static_cast<int>(SectionKind::foreground));
        // Clicking a group in the strip shows it.
        click(panel.tileGroupList(), itemWith(panel.tileGroupList(), "buildings"));
        CHECK_EQ(panel.currentTileGroup(), QString("buildings"));
        CHECK(itemWith(panel.tileList(), 48) != nullptr);
        // "Other tiles": foreground numbers no group has.
        {
            std::set<int> grouped;
            for (const auto &group : editorGroups(GameProfile::speedyBlupi1)) {
                for (const auto &entry : group.tiles) {
                    if (entry.layer == SectionKind::foreground) {
                        grouped.insert(entry.values.begin(), entry.values.end());
                    }
                }
            }
            CHECK(panel.showTileGroup(PalettePanel::otherForegroundGroup));
            CHECK_EQ(panel.tileList()->count(), 1 + 246 - static_cast<int>(grouped.size()));
            CHECK(itemWith(panel.tileList(), 153) == nullptr);
        }
        // A layer change made elsewhere moves a plain group along, but
        // leaves an editor group alone.
        CHECK(panel.showTileGroup(PalettePanel::allForegroundGroup));
        panel.setContext(GameProfile::speedyBlupi1, SectionKind::bigDecor, nullptr);
        CHECK_EQ(panel.currentTileGroup(), QString(PalettePanel::allBigDecorGroup));
        CHECK(panel.showTileGroup("rock-tech"));
        panel.setContext(GameProfile::speedyBlupi1, SectionKind::foreground, nullptr);
        CHECK_EQ(panel.currentTileGroup(), QString("rock-tech"));
        // Object groups.
        CHECK(panel.showObjectGroup("bonuses"));
        CHECK_EQ(panel.objectList()->count(), 8);
        CHECK(itemWith(panel.objectList(), "treasure") != nullptr);
        CHECK(itemWith(panel.objectList(), "bulldozer") == nullptr);
        CHECK(panel.showObjectGroup(PalettePanel::allObjectsGroup));
        CHECK_EQ(panel.objectList()->count(), 31);

        // The World tab.
        CHECK(!panel.sizeButton(true, true)->isEnabled()); // no document yet
        CHECK_EQ(panel.backdropList()->count(), 0);
        PalettePanel::WorldState state;
        state.available = true;
        state.scrollsHorizontally = true;
        state.region = 3;
        state.music = 2;
        panel.setWorldState(state);
        CHECK(panel.sizeButton(true, false)->isChecked());
        CHECK(!panel.sizeButton(true, true)->isChecked());
        CHECK_EQ(panel.backdropList()->count(), 24);
        CHECK_EQ(panel.backdropList()->currentItem()->data(Qt::UserRole).toInt(), 3);
        CHECK_EQ(panel.musicList()->count(), 11);
        CHECK_EQ(panel.musicList()->item(0)->text(), QString("No music"));
        CHECK_EQ(panel.musicList()->currentItem()->data(Qt::UserRole).toInt(), 2);
        int sizeRequest = -1;
        int regionRequest = -1;
        int musicRequest = -1;
        int playRequest = -1;
        int stopRequests = 0;
        QObject::connect(&panel, &PalettePanel::worldSizeChosen, [&](bool h, bool v) {
            sizeRequest = (h ? 1 : 0) | (v ? 2 : 0);
        });
        QObject::connect(&panel, &PalettePanel::regionChosen, [&](int r) { regionRequest = r; });
        QObject::connect(&panel, &PalettePanel::musicChosen, [&](int m) { musicRequest = m; });
        QObject::connect(&panel, &PalettePanel::musicPlayRequested, [&](int m) { playRequest = m; });
        QObject::connect(&panel, &PalettePanel::musicStopRequested, [&] { ++stopRequests; });
        panel.tabs()->setCurrentIndex(3);
        QApplication::processEvents();
        panel.sizeButton(false, true)->click();
        CHECK_EQ(sizeRequest, 2);
        click(panel.backdropList(), itemWith(panel.backdropList(), 1));
        CHECK_EQ(regionRequest, 1);
        click(panel.musicList(), itemWith(panel.musicList(), 4));
        CHECK_EQ(musicRequest, 4);
        // The play button plays the marked music, or stops what plays.
        panel.setMusicStatus("ready", 0, true);
        CHECK_EQ(panel.playButton()->text(), QString("Play"));
        panel.playButton()->click();
        CHECK_EQ(playRequest, 4);
        panel.setMusicStatus("playing", 4, true);
        CHECK_EQ(panel.playButton()->text(), QString("Stop"));
        CHECK_EQ(panel.musicStatus(), QString("playing"));
        panel.playButton()->click();
        CHECK_EQ(stopRequests, 1);
        panel.setMusicStatus("no player", 0, false);
        CHECK(!panel.playButton()->isEnabled());
        // A stored value outside the game's range marks nothing.
        state.region = 77;
        panel.setWorldState(state);
        CHECK(panel.backdropList()->currentItem() == nullptr);
        // Backdrop thumbnails come from the game's pictures when loaded.
        panel.setContext(GameProfile::speedyBlupi1, SectionKind::foreground, assets);
        CHECK(itemWith(panel.backdropList(), 1)
                  ->icon()
                  .pixmap(PalettePanel::backdropThumbnailWidth,
                          PalettePanel::backdropThumbnailHeight)
                  .toImage()
                  .pixelColor(10, 10) == backdropColor(1, false));
        // Without a game chosen background and music cannot be offered.
        panel.setContext(GameProfile::unknown, SectionKind::foreground, nullptr);
        CHECK_EQ(panel.backdropList()->count(), 0);
        CHECK_EQ(panel.musicList()->count(), 0);
        CHECK(panel.sizeButton(true, true)->isEnabled());
        panel.tabs()->setCurrentIndex(0);
        panel.hide();

        // The second game has its own objects and brushes.
        panel.setContext(GameProfile::speedyBlupi2, SectionKind::foreground, nullptr);
        CHECK_EQ(panel.tileList()->count(), 1 + 441);
        CHECK_EQ(panel.objectList()->count(),
                 static_cast<int>(objectPresets(GameProfile::speedyBlupi2).size()));
        CHECK(itemWith(panel.objectList(), "treasure") != nullptr);
    }

    // In the main window.
    auto bytes = blankWorld(fxRevision13);
    const auto path = dir.filePath("palette.blp");
    writeWorld(path, bytes);
    const auto readOnlyPath = dir.filePath("old.blp");
    writeWorld(readOnlyPath, blankWorld(fxRevision10));
    {
        MainWindow window;
        window.prompts.error = [](const QString &, const QString &) {};
        window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
        int directoryRequests = 0;
        window.prompts.chooseArtworkDirectory =
            [&](GameProfile) -> std::optional<std::pair<QString, GameProfile>> {
            ++directoryRequests;
            return std::nullopt;
        };
        window.resize(1400, 950);
        window.show();
        CHECK(QTest::qWaitForWindowExposed(&window));
        auto *palette = window.palette();
        auto *dock = window.findChild<QDockWidget *>("paletteDock");
        CHECK(palette != nullptr && dock != nullptr);
        CHECK(window.action("viewPalette") != nullptr);
        window.applyLayout(MainWindow::DockLayout::mapping);
        CHECK(dock->isVisible());
        window.applyLayout(MainWindow::DockLayout::validation);
        CHECK(!dock->isVisible());
        window.applyLayout(MainWindow::DockLayout::objects);
        CHECK(dock->isVisible());
        window.action("viewPalette")->trigger();
        CHECK(!dock->isVisible());
        window.applyLayout(MainWindow::DockLayout::mapping);
        QApplication::processEvents();

        auto *tab = window.openPath(path);
        CHECK(tab != nullptr);
        QApplication::processEvents();
        // No game chosen yet: tiles only.
        CHECK(palette->hintText().contains("Choose the game"));
        CHECK_EQ(palette->objectList()->count(), 0);

        tab->setProfile(GameProfile::speedyBlupi1);
        QApplication::processEvents();
        CHECK_EQ(palette->objectList()->count(), 31);
        CHECK(palette->hintVisible());
        auto *presetCombo = window.findChild<QComboBox *>("objectPresetCombo");
        CHECK(presetCombo != nullptr && !presetCombo->itemIcon(0).isNull());
        CHECK(!window.action("objectCreate_treasure")->icon().isNull());
        const auto comboIconBefore = presetCombo->itemIcon(0).pixmap(24, 24).toImage();

        // The button in the hint asks for the game directory.
        auto *choose = palette->findChild<QPushButton *>("paletteChooseDirectory");
        CHECK(choose != nullptr && choose->isVisible());
        choose->click();
        CHECK_EQ(directoryRequests, 1);

        // Picking a tile sets the tile ID and switches to the pencil ...
        CHECK(tab->tool() == Tool::browse);
        click(palette->tileList(), itemWith(palette->tileList(), 9));
        CHECK_EQ(static_cast<int>(tab->tileId()), 9);
        CHECK(tab->tool() == Tool::pencil);
        CHECK_EQ(window.findChild<QSpinBox *>("tileSpin")->value(), 9);
        // ... but keeps a tool that already paints tiles.
        tab->setTool(Tool::line);
        click(palette->tileList(), itemWith(palette->tileList(), 12));
        CHECK_EQ(static_cast<int>(tab->tileId()), 12);
        CHECK(tab->tool() == Tool::line);
        // The empty cell can be picked too.
        click(palette->tileList(), palette->tileList()->item(0));
        CHECK_EQ(static_cast<int>(tab->tileId()), -1);
        // The spin box and the palette stay in step.
        window.findChild<QSpinBox *>("tileSpin")->setValue(17);
        QApplication::processEvents();
        CHECK_EQ(palette->tileList()->currentItem()->data(Qt::UserRole).toInt(), 17);

        // Picking an object selects its preset and the Place tool.
        palette->tabs()->setCurrentIndex(1);
        QApplication::processEvents();
        click(palette->objectList(), itemWith(palette->objectList(), "treasure"));
        CHECK(tab->placePreset() != nullptr && tab->placePreset()->id == "treasure");
        CHECK(tab->tool() == Tool::placeObject);
        CHECK_EQ(presetCombo->currentData().toString(), QString("treasure"));
        // Choosing in the tool bar marks the palette entry.
        presetCombo->setCurrentIndex(presetCombo->findData("egg"));
        QApplication::processEvents();
        CHECK_EQ(palette->objectList()->currentItem()->data(Qt::UserRole).toString(),
                 QString("egg"));

        // Picking a border brush selects it and the Adaptive tool.
        palette->tabs()->setCurrentIndex(2);
        QApplication::processEvents();
        auto *brush = palette->brushList()->item(0);
        click(palette->brushList(), brush);
        CHECK_EQ(tab->adaptiveBrush(), brush->data(Qt::UserRole).toString());
        CHECK(tab->tool() == Tool::adaptive);

        // The tile list follows the active layer.
        palette->tabs()->setCurrentIndex(0);
        window.action("layerActiveBigDecor")->trigger();
        QApplication::processEvents();
        CHECK_EQ(palette->tileList()->count(), 1 + 54 + 1);
        window.action("layerActiveForeground")->trigger();
        QApplication::processEvents();
        CHECK_EQ(palette->tileList()->count(), 1 + 246);

        // Loading artwork turns every chooser's pictures into sprites.
        CHECK(window.loadArtwork(game.root, GameProfile::speedyBlupi1));
        QApplication::processEvents();
        CHECK(palette->showsArtwork());
        CHECK(!palette->hintVisible());
        CHECK(iconPixel(itemWith(palette->tileList(), 9), 48, 2, 2) == spriteColor(0, 9));
        CHECK(presetCombo->itemIcon(0).pixmap(24, 24).toImage() != comboIconBefore);
        // ... and switching artwork off returns to the swatches.
        window.setArtworkEnabled(false);
        QApplication::processEvents();
        CHECK(!palette->showsArtwork());
        CHECK(iconPixel(itemWith(palette->tileList(), 9), 48, 2, 2) ==
              schematicTileColor(SectionKind::foreground, 9));
        CHECK(presetCombo->itemIcon(0).pixmap(24, 24).toImage() == comboIconBefore);
        window.setArtworkEnabled(true);

        // Nothing above touched the document.
        CHECK(!tab->document().isModified());
        CHECK(tab->document().currentBytes().size() == bytes.size());
        CHECK(std::equal(bytes.begin(), bytes.end(), tab->document().currentBytes().begin()));

        // Groups in the window: a bigDecor picture switches the layer too,
        // and a reshaped tile selects the adaptive brush.
        CHECK(palette->showTileGroup("scenery"));
        tab->setTool(Tool::browse);
        click(palette->tileList(), itemWith(palette->tileList(), 20));
        CHECK(tab->activeLayer() == SectionKind::bigDecor);
        CHECK_EQ(static_cast<int>(tab->tileId()), 20);
        CHECK(tab->tool() == Tool::pencil);
        CHECK(palette->showTileGroup("rock-tech"));
        click(palette->tileList(), itemWith(palette->tileList(), 153));
        CHECK(tab->tool() == Tool::adaptive);
        CHECK_EQ(tab->adaptiveBrush(), QString("rock"));
        click(palette->tileList(), itemWith(palette->tileList(), 185));
        CHECK(tab->activeLayer() == SectionKind::foreground);
        CHECK_EQ(static_cast<int>(tab->tileId()), 185);
        CHECK(!tab->document().isModified());

        // The World tab changes the header, one undo step per choice.
        palette->tabs()->setCurrentIndex(3);
        QApplication::processEvents();
        const auto headerOf = [&] {
            const auto current = tab->document().currentBytes();
            return Bytes(current.begin(), current.begin() + 868);
        };
        const auto headerBefore = headerOf();
        CHECK(palette->sizeButton(false, false)->isChecked());
        palette->sizeButton(true, true)->click();
        QApplication::processEvents();
        auto header = headerOf();
        CHECK_EQ(get8(header, 212), 100u);
        CHECK_EQ(get8(header, 216), 100u);
        CHECK(palette->sizeButton(true, true)->isChecked());
        click(palette->backdropList(), itemWith(palette->backdropList(), 2));
        CHECK_EQ(get8(headerOf(), 224), 2u);
        CHECK_EQ(palette->backdropList()->currentItem()->data(Qt::UserRole).toInt(), 2);

        // Music: choosing stores the number and plays it with the system's
        // player (a stand-in here).
        window.musicPlayer()->setBackend(
            MusicPlayer::Backend{"/bin/sh", {"-c", "exec sleep 30", "%f"}, "Stand-in"});
        CHECK(palette->playWhenChosen());
        click(palette->musicList(), itemWith(palette->musicList(), 1));
        CHECK_EQ(get8(headerOf(), 222), 1u);
        CHECK(window.musicPlayer()->isPlaying());
        CHECK_EQ(window.musicPlayer()->playingNumber(), 1);
        CHECK(palette->musicStatus().contains("Playing music 1"));
        CHECK_EQ(palette->playButton()->text(), QString("Stop"));
        palette->playButton()->click();
        CHECK(!window.musicPlayer()->isPlaying());
        CHECK_EQ(get8(headerOf(), 222), 1u); // stopping changes nothing stored
        palette->playButton()->click();
        CHECK(window.musicPlayer()->isPlaying());
        // "No music" stores 0 and stops.
        click(palette->musicList(), itemWith(palette->musicList(), 0));
        CHECK_EQ(get8(headerOf(), 222), 0u);
        CHECK(!window.musicPlayer()->isPlaying());
        // A number the directory has no file for is stored but not played.
        click(palette->musicList(), itemWith(palette->musicList(), 5));
        CHECK_EQ(get8(headerOf(), 222), 5u);
        CHECK(!window.musicPlayer()->isPlaying());
        CHECK(palette->musicStatus().contains("not found"));
        // Only those header fields changed, and undo takes each step back.
        header = headerOf();
        for (std::size_t offset = 0; offset < header.size(); ++offset) {
            if (offset != 212 && offset != 216 && offset != 222 && offset != 224) {
                CHECK(header[offset] == headerBefore[offset]);
            }
        }
        int undone = 0;
        while (tab->document().isModified() && tab->document().undo()) {
            ++undone;
        }
        CHECK_EQ(undone, 5); // size, background, music 1, no music, music 5
        CHECK(headerOf() == headerBefore);
        palette->tabs()->setCurrentIndex(0);
        QApplication::processEvents();

        // A read-only document: the palette is shown but cannot choose.
        auto *old = window.openPath(readOnlyPath);
        CHECK(old != nullptr);
        old->setProfile(GameProfile::speedyBlupi1);
        QApplication::processEvents();
        CHECK(!palette->tabs()->isEnabled());
        CHECK(old->tool() == Tool::browse);
        window.close();
    }
    // The game directory was only read.
    CHECK(directoryDigest(game.root) == digest);
    return test::finish("palette");
}
