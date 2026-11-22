// SPDX-License-Identifier: GPL-3.0-only
//
// The visual chooser: tiles, verified objects and border brushes as
// pictures, schematic without game data and drawn from generated sheets
// with it. Choosing must only change the tool state, never a document.

#include "app/main_window.hpp"
#include "app/palette_panel.hpp"
#include "app/sprite_icons.hpp"
#include "app/world_renderer.hpp"
#include "world/adaptive.hpp"
#include "world/object_catalog.hpp"

#include "check.hpp"
#include "fixture.hpp"
#include "generated_game.hpp"

#include <QAction>
#include <QApplication>
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
