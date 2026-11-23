// SPDX-License-Identifier: GPL-3.0-only
//
// A Mobile Eggbert text world in the full editor: opened as a tab with the
// port's pictures, edited with the ordinary tools, and saved back into its
// text with only the changed values touched.
#include "app/document_file.hpp"
#include "app/game_assets.hpp"
#include "app/installation_panel.hpp"
#include "app/main_window.hpp"
#include "app/recovery_store.hpp"
#include "app/text_world_editor.hpp"
#include "app/world_canvas.hpp"
#include "world/doctor.hpp"
#include "world/editor_groups.hpp"
#include "world/encyclopedia.hpp"
#include "world/merge.hpp"
#include "world/object_catalog.hpp"
#include "world/profile.hpp"
#include "world/reachability.hpp"
#include "world/stamp.hpp"
#include "world/text_world.hpp"
#include "world/text_world_image.hpp"

#include "check.hpp"
#include "fixture.hpp"
#include "generated_port.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTemporaryDir>

using namespace eggbert;
using namespace eggbert::test;

namespace {

QByteArray readAll(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void writeFile(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(bytes);
}

} // namespace

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("eggbert-editor-test");
    QCoreApplication::setApplicationName("mobile-world-ui");
    QSettings().clear();
    QDir(RecoveryStore::defaultDirectory()).removeRecursively();
    QTemporaryDir dir;
    const auto port = makePort(dir.filePath("port"));
    const auto original = QByteArray::fromStdString(portWorldText());
    const auto mobile = GameProfile::mobileEggbert;

    // The profile: the second game's catalog, without its loader's remap,
    // and with the port's own collision exceptions.
    CHECK(tablesOf(mobile) == GameProfile::speedyBlupi2);
    CHECK_EQ(editorGroups(mobile).size(), editorGroups(GameProfile::speedyBlupi2).size());
    CHECK_EQ(objectPresets(mobile).size(), objectPresets(GameProfile::speedyBlupi2).size());
    CHECK(profileInfo(mobile) != nullptr && !profileInfo(mobile)->loaderRemaps48To67);
    CHECK(tileFullySolid(GameProfile::speedyBlupi2, 50) && !tileFullySolid(mobile, 50));
    CHECK(tileFullySolid(mobile, 48) && tileFullySolid(mobile, 30) && tileFullySolid(mobile, 246));
    CHECK(!tileFullySolid(mobile, 67) && tileFullySolid(mobile, 78));

    // The port's pictures: found by the grid rule, in the port's folder or
    // its Content folder.
    {
        CHECK(detectGameProfile(port.root).profile == mobile);
        CHECK(detectGameProfile(port.content).profile == mobile);
        CHECK_EQ(GameAssets::mobileContentDirectory(port.root), port.content);
        CHECK(GameAssets::mobileContentDirectory(dir.path()).isEmpty());
        const auto assets = GameAssets::load(port.root, mobile);
        CHECK(assets->usable() && assets->profile() == mobile);
        CHECK_EQ(assets->spriteCount(SpriteSheet::tiles), std::size_t{440});
        CHECK_EQ(assets->spriteCount(SpriteSheet::elements), std::size_t{290});
        CHECK_EQ(assets->spriteCount(SpriteSheet::bigDecor), std::size_t{100});
        // Tile 23: column 3 of row 1, each cell preceded by its one-pixel frame.
        const auto tile = assets->sprite(1, 23);
        CHECK(tile.has_value() && tile->source == QRect(1 + 3 * 65, 1 + 65, 64, 64));
        CHECK(tile.has_value() && tile->offset == QPoint(0, 0));
        CHECK(tile.has_value() &&
              tile->sheet->pixelColor(tile->source.center()) == portColor(0, 23));
        const auto element = assets->sprite(10, 12);
        CHECK(element.has_value() && element->source == QRect(2 * 60, 60, 60, 60));
        const auto big = assets->sprite(9, 20);
        CHECK(big.has_value() && big->source == QRect(0, 2 * 144, 144, 144));
        // The three further player channels share the second sheet.
        CHECK(assets->sprite(2, 5)->sheet != assets->sprite(11, 5)->sheet);
        CHECK(assets->sprite(11, 5)->sheet->pixelColor(assets->sprite(11, 5)->source.center()) ==
              portColor(4, 5));
        CHECK(!assets->sprite(1, 440).has_value());
        CHECK(assets->backdrop(7) != nullptr && assets->backdrop(7)->size() == QSize(640, 480));
        CHECK(assets->backdrop(8) == nullptr);
        CHECK(assets->musicFile(3).isEmpty());
        CHECK(!GameAssets::load(dir.path(), mobile)->usable());
    }

    MainWindow window;
    QStringList errors;
    window.prompts.error = [&](const QString &, const QString &text) { errors << text; };
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    window.prompts.confirmBroadEdit = [](DocumentTab *, const BroadEditPreview &) { return true; };
    window.resize(1300, 850);
    window.show();

    // Opening the text world makes an ordinary tab.
    auto *tab = window.openPath(port.worldPath);
    CHECK(tab != nullptr && errors.isEmpty());
    if (tab == nullptr) {
        return test::finish("mobile_world_ui");
    }
    CHECK(window.textWorldEditors().empty());
    CHECK(tab->file().isTextWorld());
    CHECK(tab->profile() == mobile);
    CHECK(tab->document().isWritable() && !tab->document().isModified());
    auto *combo = window.findChild<QComboBox *>("profileCombo");
    CHECK(combo != nullptr && !combo->isEnabled());
    CHECK_EQ(combo->currentText(), QString("Mobile Eggbert"));
    // Its pictures were found next to the worlds folder.
    CHECK(window.artworkFor(mobile) != nullptr);
    CHECK(window.gameDirectoryFor(mobile) != nullptr);
    // What the text holds is on the map.
    {
        const auto view = tab->document().view();
        CHECK_EQ(*view.foreground()->cell(3, 4), 40);
        CHECK_EQ(*view.bigDecor()->cell(9, 0), 20);
        CHECK_EQ(view.objects()->activeCount(), std::size_t{2});
        CHECK_EQ(view.header()->music(), 3);
    }
    // Nothing of the Windows formats is offered for it.
    for (const auto *name : {"fileConvertRevision", "fileConvertGame", "editBytes",
                             "viewInspector"}) {
        auto *action = window.findChild<QAction *>(name);
        CHECK(action != nullptr && !action->isEnabled());
    }
    CHECK(!window.findChild<QSpinBox *>("startSpin")->isEnabled());
    CHECK(window.statusBar()->findChildren<QLabel *>().size() > 0);
    CHECK(tab->describeCell({3, 4}).contains("Decor 40"));
    CHECK(!tab->describeCell({3, 4}).contains("offset"));
    CHECK(runDoctor(tab->document().view(), tab->profile()).count(Severity::error) == 0);

    // Ordinary tools: a tile, a shape, an object, the start.
    tab->setTool(Tool::pencil);
    tab->setTileId(41);
    {
        const std::vector<CellPos> cells{{3, 5}};
        CHECK(tab->paintStroke(cells, 41, QStringLiteral("Pencil"), 0));
    }
    CHECK(tab->drawRectangle({20, 20}, {21, 21}));
    CHECK(tab->document().isModified());
    tab->setSelectedSlot(0);
    CHECK(tab->moveSelectedObject({64, 0}));
    tab->setTool(Tool::start);
    tab->setActiveStart(2); // ignored: one start
    CHECK_EQ(tab->activeStart(), std::size_t{0});
    CHECK(tab->placeStartAt({5, 9}));
    {
        const auto header = *tab->document().view().header();
        for (std::size_t index = 1; index < 4; ++index) {
            CHECK(*header.blupiPos(index) == *header.blupiPos(0));
        }
    }
    const auto startAfter = *tab->document().view().header()->blupiPos(0);

    // Saving writes text: the changed values and nothing else.
    CHECK(window.saveTab(tab));
    CHECK(errors.isEmpty());
    CHECK(!tab->document().isModified());
    const auto saved = readAll(port.worldPath);
    CHECK(saved != original);
    CHECK(saved.startsWith("DescFile: posDecor=0;550 dimDecor=100;100 world=0 music=3 region=7 "));
    {
        const auto world = TextWorld::parse(saved.toStdString());
        CHECK(world.problems().empty());
        CHECK_EQ(*world.tile(SectionKind::foreground, 3, 5), 41);
        CHECK_EQ(*world.tile(SectionKind::foreground, 3, 4), 40);
        CHECK_EQ(*world.tile(SectionKind::foreground, 21, 21), 41);
        CHECK_EQ(*world.tile(SectionKind::bigDecor, 9, 0), 20);
        CHECK(*world.objectPoint(0, "posStart") == (Point{258 + 64, 196}));
        CHECK(*world.headerPoint("blupiPos") == startAfter);
        CHECK_EQ(world.objectCount(), std::size_t{2});
        CHECK(saved.contains(portLift)); // the untouched object line, byte for byte
        // 41 (2 chars), four cells of 41 (8), 258 -> 322 (0), the start (digits may differ)
        CHECK(qAbs(saved.size() - original.size()) < 20);
    }
    // Undo goes back past the save; saving again restores the first text.
    while (tab->document().canUndo()) {
        CHECK(tab->undo());
    }
    CHECK(window.saveTab(tab));
    CHECK(readAll(port.worldPath) == original);

    // A deleted object loses its line; a new one gets a line.
    tab->setSelectedSlot(0);
    CHECK(tab->deleteSelectedObject());
    CHECK(window.saveTab(tab));
    {
        const auto world = TextWorld::parse(readAll(port.worldPath).toStdString());
        CHECK_EQ(world.objectCount(), std::size_t{1});
        CHECK_EQ(*world.objectValue(0, "type"), 1);
    }
    CHECK(tab->undo());
    CHECK(window.saveTab(tab));
    {
        // The treasure is back, now as the last line.
        const auto world = TextWorld::parse(readAll(port.worldPath).toStdString());
        CHECK_EQ(world.objectCount(), std::size_t{2});
        CHECK_EQ(*world.objectValue(1, "type"), 5);
        CHECK(*world.objectPoint(1, "posStart") == (Point{258, 196}));
    }

    // Changed on disk by something else: the save is refused like any other.
    {
        QFile file(port.worldPath);
        CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(original + "\n");
    }
    {
        const std::vector<CellPos> cells{{50, 50}};
        CHECK(tab->paintStroke(cells, 41, QStringLiteral("Pencil"), 0));
    }
    bool asked = false;
    window.prompts.saveConflict = [&](DocumentTab *, const SaveResult &) {
        asked = true;
        return ConflictChoice::cancel;
    };
    CHECK(!window.saveTab(tab));
    CHECK(asked);
    CHECK(readAll(port.worldPath) == original + "\n");
    // Save As writes text elsewhere.
    const auto copyPath = dir.filePath("copy.txt");
    window.prompts.saveTextWorldAsPath = [&](const QString &) { return copyPath; };
    CHECK(window.saveTabAs(tab));
    CHECK(looksLikeTextWorld(readAll(copyPath).toStdString()));
    CHECK_EQ(*TextWorld::parse(readAll(copyPath).toStdString()).tile(SectionKind::foreground, 50, 50),
             41);
    CHECK(tab->file().isTextWorld());

    // Unsaved changes are kept for recovery as text, never as the image.
    {
        writeFile(port.worldPath, original);
        MainWindow second;
        second.prompts.error = [&](const QString &, const QString &text) { errors << text; };
        second.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
        auto *edited = second.openPath(port.worldPath);
        CHECK(edited != nullptr);
        if (edited != nullptr) {
            const std::vector<CellPos> cells{{60, 61}};
            CHECK(edited->paintStroke(cells, 41, QStringLiteral("Pencil"), 0));
            second.flushAutosave();
            const auto entries = second.recoveryStore().autosaves();
            CHECK_EQ(entries.size(), std::size_t{1});
            const auto kept = *second.recoveryStore().read(entries.at(0));
            CHECK(looksLikeTextWorld(kept.toStdString()));
            CHECK_EQ(*TextWorld::parse(kept.toStdString()).tile(SectionKind::foreground, 60, 61),
                     41);
            CHECK(kept.size() == original.size() + 2);
            CHECK_EQ(entries.at(0).baseSize, original.size());

            // A snapshot is text as well, and restoring one is one undo step.
            second.prompts.snapshotName = [](DocumentTab *) {
                return std::optional<QString>(QStringLiteral("one tile"));
            };
            CHECK(second.takeSnapshot(edited));
            const auto snapshots = second.recoveryStore().snapshots(port.worldPath);
            CHECK_EQ(snapshots.size(), std::size_t{1});
            CHECK(looksLikeTextWorld(second.recoveryStore().read(snapshots.at(0))->toStdString()));
            const auto atSnapshot = toVector(edited->document().currentBytes());
            edited->setSelectedSlot(1);
            CHECK(edited->deleteSelectedObject());
            second.prompts.chooseSnapshot =
                [](DocumentTab *, const std::vector<RecoveryEntry> &listed)
                -> std::optional<std::pair<RecoveryEntry, SnapshotAction>> {
                return std::make_pair(listed.front(), SnapshotAction::restore);
            };
            CHECK(second.manageSnapshots(edited));
            CHECK(toVector(edited->document().currentBytes()) == atSnapshot);
            second.prompts.chooseSnapshot =
                [](DocumentTab *, const std::vector<RecoveryEntry> &listed)
                -> std::optional<std::pair<RecoveryEntry, SnapshotAction>> {
                return std::make_pair(listed.front(), SnapshotAction::remove);
            };
            CHECK(second.manageSnapshots(edited));
            second.flushAutosave();
        }
        // The window goes away without a clean close: the copy stays.
        CHECK_EQ(second.recoveryStore().autosaves().size(), std::size_t{1});
    }
    {
        MainWindow third;
        third.prompts.error = [&](const QString &, const QString &text) { errors << text; };
        third.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
        bool offered = false;
        third.prompts.recover = [&](const RecoveryEntry &, bool canRestore, const QString &) {
            offered = canRestore;
            return RecoveryChoice::restore;
        };
        CHECK_EQ(third.checkRecovery(), 1);
        CHECK(offered);
        auto *restored = third.currentTab();
        CHECK(restored != nullptr && restored->file().isTextWorld());
        if (restored != nullptr) {
            CHECK(restored->document().isModified());
            CHECK_EQ(*restored->document().view().foreground()->cell(60, 61), 41);
            CHECK(third.saveTab(restored));
            CHECK_EQ(*TextWorld::parse(readAll(port.worldPath).toStdString())
                          .tile(SectionKind::foreground, 60, 61),
                     41);
            // A stamp carrying the two unexplained numbers of an object is
            // placed without them, so the world can still be written.
            Stamp stamp;
            stamp.name = "lift";
            stamp.game = mobile;
            stamp.width = 1;
            stamp.height = 1;
            stamp.foreground = {-1};
            stamp.bigDecor = {-1};
            ObjectRecord record = *restored->document().view().objects()->record(1);
            record.posStart = record.posEnd = record.posCurrent = {0, 0};
            record.unknown1 = 7;
            record.unknown2 = 9;
            stamp.objects.push_back(record);
            CHECK(restored->placeStampAt(stamp, {70, 70}));
            CHECK(third.saveTab(restored));
            CHECK_EQ(TextWorld::parse(readAll(port.worldPath).toStdString()).objectCount(),
                     std::size_t{3});
            CHECK(third.closeTab(restored));
        }
        CHECK(third.recoveryStore().autosaves().empty());
        CHECK(errors.isEmpty());
    }

    // The port's worlds folder in the Installation dock, deploying into it
    // and putting the replaced file back, and the playtest command.
    {
        writeFile(port.worldPath, original);
        writeFile(port.root + "/worlds/world020.txt", original);
        writeFile(port.root + "/worlds/notes.txt", "not a world");
        MainWindow fourth;
        fourth.prompts.error = [&](const QString &, const QString &text) { errors << text; };
        fourth.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
        auto *world = fourth.openPath(port.worldPath);
        CHECK(world != nullptr);
        auto *panel = fourth.installation();
        CHECK(panel->profile() == mobile);
        CHECK_EQ(panel->dataDirectory(), QDir::cleanPath(port.root + "/worlds"));
        CHECK_EQ(panel->entries().size(), std::size_t{2});
        CHECK_EQ(panel->list()->count(), 2);
        CHECK(panel->entries().at(0).complete && panel->entries().at(0).name.number == 11);
        CHECK(panel->list()->item(0)->text().startsWith("world011.txt - mission"));
        CHECK(panel->list()->item(1)->text().startsWith("world020.txt - group hub"));
        CHECK(!panel->list()->item(0)->icon().isNull());
        // The small pictures start schematic and are then drawn with the
        // port's pictures, one world per turn of the event loop.
        {
            const auto holds = [&](const QColor &colour) {
                const auto image =
                    panel->list()->item(0)->icon().pixmap(64, 64).toImage().convertToFormat(
                        QImage::Format_ARGB32);
                for (int y = 0; y < image.height(); ++y) {
                    for (int x = 0; x < image.width(); ++x) {
                        if (image.pixelColor(x, y) == colour) {
                            return true;
                        }
                    }
                }
                return false;
            };
            CHECK_EQ(panel->pendingThumbnails(), std::size_t{2});
            CHECK(!holds(portColor(0, 40)));
            for (int turn = 0; turn < 100 && panel->pendingThumbnails() != 0; ++turn) {
                QCoreApplication::processEvents();
            }
            CHECK_EQ(panel->pendingThumbnails(), std::size_t{0});
            CHECK(holds(portColor(0, 40)));
            // Unchanged files keep their pictures; a changed one is drawn again.
            panel->refresh();
            CHECK_EQ(panel->pendingThumbnails(), std::size_t{0});
            CHECK(holds(portColor(0, 40)));
            auto other = original;
            other.replace(",,,,40,", ",,,,41,");
            writeFile(port.worldPath, other + "\n");
            panel->refresh();
            CHECK_EQ(panel->pendingThumbnails(), std::size_t{1});
            panel->finishThumbnails();
            CHECK(holds(portColor(0, 41)) && !holds(portColor(0, 40)));
            writeFile(port.worldPath, original);
            panel->refresh();
            panel->finishThumbnails();
        }
        CHECK(panel->summary()->text().contains("2 world(s) of Mobile Eggbert"));
        CHECK(panel->deployButton()->isEnabled());
        // Campaign packs hold .blp worlds; the overview works for the port.
        for (const auto *name : {"fileExportPack", "fileImportPack"}) {
            CHECK(!fourth.findChild<QAction *>(name)->isEnabled());
        }
        CHECK(fourth.findChild<QAction *>("fileCampaignOverview")->isEnabled());
        {
            // World 20 (a hub) holds door tile 160, which stands for 3: it
            // leads to world 23, which is not there. World 1 is missing too.
            auto hub = original;
            hub.replace(",,,,40,", ",,,,160,");
            writeFile(port.root + "/worlds/world020.txt", hub);
            panel->refresh();
            const auto overview = fourth.campaignOverview();
            CHECK(overview.contains("world020.txt  group hub"));
            CHECK(overview.contains("    world011.txt  mission"));
            CHECK(overview.contains("World 1 is missing"));
            CHECK(overview.contains("Hub 10 is missing"));
            CHECK(overview.contains("World 20 -> 23"));
            CHECK(overview.contains("World 20 has a door (tile 160 at cell 3, 4) to world 23"));
            CHECK(!overview.contains(".blp"));
            writeFile(port.root + "/worlds/world020.txt", original);
            panel->refresh();
        }
        CHECK(fourth.findChild<QAction *>("fileDeploy")->isEnabled());
        CHECK(fourth.findChild<QAction *>("fileMerge")->isEnabled());
        if (world != nullptr) {
            const std::vector<CellPos> cells{{8, 8}};
            CHECK(world->paintStroke(cells, 41, QStringLiteral("Pencil"), 0));
            int suggested = -1;
            fourth.prompts.deployPortNumber = [&](DocumentTab *, int number, const QString &) {
                suggested = number;
                return std::optional<int>(20);
            };
            const auto record = fourth.deployWorld(world);
            CHECK(record.has_value());
            CHECK_EQ(suggested, 11); // the file's own number
            const auto deployed = readAll(port.root + "/worlds/world020.txt");
            CHECK(looksLikeTextWorld(deployed.toStdString()));
            CHECK_EQ(*TextWorld::parse(deployed.toStdString()).tile(SectionKind::foreground, 8, 8),
                     41);
            CHECK(readAll(port.worldPath) == original); // the tab's own file is not saved by it
            // Deploying onto a world that is open is refused.
            fourth.prompts.deployPortNumber = [](DocumentTab *, int, const QString &) {
                return std::optional<int>(11);
            };
            CHECK(!fourth.deployWorld(world).has_value());
            CHECK_EQ(errors.size(), 1);
            errors.clear();
            fourth.prompts.confirmRollBack = [](const DeployRecord &) { return true; };
            CHECK(fourth.rollBackLastDeploy());
            CHECK(readAll(port.root + "/worlds/world020.txt") == original);

            // The playtest command is this game's own and runs next to Content.
            QString startedIn;
            fourth.prompts.playtestCommand = [](GameProfile, const QString &) {
                return std::optional<QString>(QStringLiteral("eggbert-port --windowed"));
            };
            fourth.prompts.startProcess = [&](const QString &program, const QStringList &arguments,
                                              const QString &directory) {
                startedIn = directory;
                return program == "eggbert-port" && arguments == QStringList{"--windowed"};
            };
            CHECK(fourth.playtest());
            CHECK_EQ(startedIn, port.root);
            CHECK_EQ(QSettings().value("playtest/command-mobile").toString(),
                     QString("eggbert-port --windowed"));
            CHECK(!QSettings().contains("playtest/command2"));

            // Merge Changes: another copy changed a tile and the music; this
            // tab changed (8, 8). Both end up in the tab, as one undo step.
            auto theirs = original;
            theirs.replace("music=3", "music=5");
            theirs.replace(",,,,40,", ",,,,40,42");
            writeFile(dir.filePath("theirs.txt"), theirs);
            writeFile(dir.filePath("base.txt"), original);
            fourth.prompts.mergeFiles = [&](DocumentTab *) {
                return std::optional(
                    std::make_pair(dir.filePath("base.txt"), dir.filePath("theirs.txt")));
            };
            fourth.prompts.resolveMerge = [](DocumentTab *, const MergeResult &merged,
                                             std::span<const std::byte>,
                                             std::span<const std::byte>) {
                return std::optional(std::vector<bool>(merged.conflicts.size(), false));
            };
            const auto steps = world->document().historySize();
            CHECK(fourth.mergeInto(world));
            CHECK_EQ(world->document().historySize(), steps + 1);
            const auto view = world->document().view();
            CHECK_EQ(view.header()->music(), 5);
            CHECK_EQ(*view.foreground()->cell(3, 5), 42);
            CHECK_EQ(*view.foreground()->cell(8, 8), 41);
            CHECK(fourth.saveTab(world));
            const auto merged = TextWorld::parse(readAll(port.worldPath).toStdString());
            CHECK(merged.problems().empty());
            CHECK_EQ(*merged.headerValue("music"), 5);
            // A .blp file is no copy of a text world.
            const auto blp = blankWorld(fxRevision13);
            writeFile(dir.filePath("other.blp"),
                      QByteArray(reinterpret_cast<const char *>(blp.data()),
                                 static_cast<qsizetype>(blp.size())));
            fourth.prompts.mergeFiles = [&](DocumentTab *) {
                return std::optional(
                    std::make_pair(dir.filePath("base.txt"), dir.filePath("other.blp")));
            };
            CHECK(!fourth.mergeInto(world));
            CHECK_EQ(errors.size(), 1);
            errors.clear();
        }

        // File > New World for Mobile Eggbert writes a text world and opens it.
        const auto newPath = port.root + "/worlds/world030.txt";
        GameProfile offered = GameProfile::unknown;
        fourth.prompts.newWorldGame = [&](GameProfile suggestedGame) {
            offered = suggestedGame;
            return std::optional(mobile);
        };
        QString suggestedPath;
        fourth.prompts.saveTextWorldAsPath = [&](const QString &suggestion) {
            suggestedPath = suggestion;
            return newPath;
        };
        auto *created = fourth.createWorld();
        CHECK(offered == mobile);
        CHECK(suggestedPath.endsWith("/worlds/new-world.txt"));
        CHECK(created != nullptr && created->file().isTextWorld() && created->profile() == mobile);
        CHECK(readAll(newPath) == QByteArray::fromStdString(newTextWorld()));
        if (created != nullptr) {
            CHECK_EQ(created->document().view().objects()->activeCount(), std::size_t{2});
            CHECK(runDoctor(created->document().view(), mobile).count(Severity::error) == 0);
        }
        CHECK(errors.isEmpty());
    }

    // A file with Windows line ends opens as a tab too: the carriage returns
    // stay where they are, and the tab says what they may mean for the game.
    {
        QByteArray dos = original;
        dos.replace("\n", "\r\n");
        const auto dosPath = dir.filePath("dos.txt");
        writeFile(dosPath, dos);
        auto *dosTab = window.openPath(dosPath);
        CHECK(dosTab != nullptr && dosTab->file().isTextWorld());
        if (dosTab != nullptr) {
            CHECK(dosTab->file().hasWindowsLineEnds());
            CHECK(dosTab->document().isWritable());
            auto *banner = dosTab->findChild<QLabel *>("documentBanner");
            CHECK(banner != nullptr && !banner->isHidden());
            CHECK(banner != nullptr && banner->text().contains("Windows line ends"));
            const std::vector<CellPos> cells{{0, 99}};
            CHECK(dosTab->paintStroke(cells, 41, QStringLiteral("Pencil"), 0));
            CHECK(window.saveTab(dosTab));
            const auto written = readAll(dosPath);
            CHECK_EQ(written.count('\n'), written.count('\r'));
            CHECK_EQ(written.size(), dos.size() + 2);
            CHECK(written.contains(",41\r\n"));
            CHECK(dosTab->undo());
            CHECK(window.saveTab(dosTab));
            CHECK(readAll(dosPath) == dos);
            CHECK(window.closeTab(dosTab));
        }
        auto *unixBanner = tab->findChild<QLabel *>("documentBanner");
        CHECK(unixBanner != nullptr && unixBanner->isHidden());
        CHECK(!tab->file().hasWindowsLineEnds());
    }

    // A text world the layout cannot hold opens in the text editor instead.
    {
        auto odd = portWorldText();
        odd.replace(odd.find(",,,,40"), 6, ",,,,40000");
        const auto oddPath = dir.filePath("odd.txt");
        QFile file(oddPath);
        CHECK(file.open(QIODevice::WriteOnly));
        file.write(QByteArray::fromStdString(odd));
        file.close();
        const auto tabs = window.tabCount();
        CHECK(window.openPath(oddPath) == nullptr);
        CHECK_EQ(window.tabCount(), tabs);
        CHECK_EQ(window.textWorldEditors().size(), std::size_t{1});
        CHECK(window.statusBar()->currentMessage().contains("only be edited as text"));
        CHECK(errors.isEmpty());
        const auto failed = DocumentFile::load(oddPath);
        CHECK(!failed.has_value() && failed.error().error == LoadError::textWorldShape);
    }
    // A .blp world next to it is unaffected: its game can be chosen, but
    // never as Mobile Eggbert.
    {
        const auto blpPath = dir.filePath("plain.blp");
        QFile file(blpPath);
        CHECK(file.open(QIODevice::WriteOnly));
        const auto bytes = blankWorld(fxRevision13);
        file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<qint64>(bytes.size()));
        file.close();
        auto *plain = window.openPath(blpPath);
        CHECK(plain != nullptr && !plain->file().isTextWorld());
        CHECK(combo->isEnabled());
        combo->setCurrentIndex(combo->findData(static_cast<int>(mobile)));
        CHECK(plain->profile() != mobile);
        CHECK(window.findChild<QAction *>("fileConvertRevision")->isEnabled());
        // Tiles are not carried between the two kinds of world, in either
        // direction; within one text world copy and paste work as usual.
        plain->setProfile(GameProfile::speedyBlupi2);
        plain->selectAll();
        CHECK(plain->copySelection());
        CHECK(!tab->canPaste());
        CHECK(!tab->paste());
        tab->selectAll();
        CHECK(tab->copySelection());
        CHECK(tab->canPaste());
        CHECK(!plain->canPaste());
        CHECK(!plain->paste());
    }
    CHECK_EQ(encyclopedia(mobile).size(), encyclopedia(GameProfile::speedyBlupi2).size());
    return test::finish("mobile_world_ui");
}
