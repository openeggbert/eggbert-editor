// SPDX-License-Identifier: GPL-3.0-only
//
// Campaign packs through the main window, on generated installations: the
// game's own files are left out of an export unless asked for, an import
// places every world or none, and the overview orders worlds by the games'
// numbering.

#include "app/installation_panel.hpp"
#include "app/main_window.hpp"

#include "check.hpp"
#include "fixture.hpp"
#include "generated_game.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QListWidget>
#include <QSettings>
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

QByteArray readAll(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

Bytes worldWith(int tile) {
    auto bytes = blankWorld(fxRevision12);
    put16(bytes, fxCellOffset(fxForegroundOffset, 5, 5), tile);
    return bytes;
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("eggbert-editor-test");
    QCoreApplication::setApplicationName("campaign-ui");
    QSettings().clear();
    QDir(DeployStore::defaultDirectory()).removeRecursively();
    QTemporaryDir dir;

    // The source installation: three of "the game's" worlds and two designed.
    const auto game = makeGame(dir.filePath("game"));
    const QDir data(game.root + "/DATA");
    write(data.filePath("WORLD001.BLP"), raw(worldWith(1)));
    write(data.filePath("WORLD010.BLP"), raw(worldWith(2)));
    write(data.filePath("WORLD011.BLP"), raw(worldWith(3)));
    write(data.filePath("u001-001.blp"), raw(worldWith(4)));
    write(data.filePath("u001-002.blp"), raw(worldWith(5)));
    write(data.filePath("WORLD030.BLP"), raw(worldWith(6)).left(100)); // not a world
    // The target installation: one designed world that will be replaced.
    const auto other = makeGame(dir.filePath("other"));
    const QDir otherData(other.root + "/DATA");
    write(otherData.filePath("U001-001.BLP"), raw(worldWith(40)));
    write(otherData.filePath("WORLD001.BLP"), raw(worldWith(41)));

    const auto minePath = dir.filePath("mine.blp");
    write(minePath, raw(worldWith(7)));

    MainWindow window;
    QStringList errors;
    QStringList reports;
    window.prompts.error = [&](const QString &, const QString &text) { errors << text; };
    window.prompts.showReport = [&](const QString &, const QString &text) { reports << text; };
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    window.resize(1200, 800);
    window.show();
    const auto exportDialog = window.prompts.exportPack;

    auto *overview = window.findChild<QAction *>("fileCampaignOverview");
    auto *exportAction = window.findChild<QAction *>("fileExportPack");
    auto *importAction = window.findChild<QAction *>("fileImportPack");
    CHECK(overview != nullptr && exportAction != nullptr && importAction != nullptr);
    CHECK(!overview->isEnabled() && !exportAction->isEnabled() && !importAction->isEnabled());
    CHECK(!window.exportCampaignPack());
    CHECK(errors.size() == 1);
    errors.clear();

    auto *tab = window.openPath(minePath);
    CHECK(tab != nullptr);
    tab->setProfile(GameProfile::speedyBlupi1);
    CHECK(window.loadArtwork(game.root, GameProfile::speedyBlupi1));
    QCoreApplication::processEvents();
    CHECK(overview->isEnabled() && exportAction->isEnabled() && importAction->isEnabled());

    // The editor's own deploy to an official number is not "the game's own".
    window.prompts.deploySlot = [](DocumentTab *, WorldSlot, const QString &) {
        return std::optional(WorldSlot{false, 1, 12});
    };
    CHECK(window.deployWorld(tab).has_value());
    const auto candidates = window.packCandidates();
    CHECK_EQ(candidates.size(), std::size_t{6}); // the cut file is no candidate
    const auto candidate = [&](const QString &name) -> const PackCandidate & {
        for (const auto &item : candidates) {
            if (QFileInfo(item.path).fileName().compare(name, Qt::CaseInsensitive) == 0) {
                return item;
            }
        }
        CHECK(false);
        return candidates.front();
    };
    CHECK(candidate("world001.blp").gameOwn);
    CHECK(candidate("world010.blp").gameOwn);
    CHECK(candidate("world011.blp").gameOwn);
    CHECK(!candidate("world012.blp").gameOwn);
    CHECK(!candidate("u001-001.blp").gameOwn);
    CHECK(!candidate("u001-002.blp").gameOwn);

    // The overview: ordered, with roles and what the numbering shows.
    overview->trigger();
    CHECK_EQ(reports.size(), qsizetype{1});
    {
        const auto &text = reports.front();
        const auto at = [&](const char *what) { return text.indexOf(what); };
        CHECK(at("world001.blp  first world") >= 0);
        CHECK(at("world010.blp  group hub") > at("world001.blp"));
        CHECK(at("    world011.blp  mission") > at("world010.blp"));
        CHECK(at("    world012.blp  mission") > at("world011.blp"));
        CHECK(at("u001-001.blp  player 1, world 1") > at("world012.blp"));
        CHECK(text.contains("[note] World 99"));
        CHECK(!text.contains("world030")); // not a complete world
        CHECK(text.contains("Basis: numbering rules"));
    }

    // Export: what is not the game's own, as one ZIP file.
    const auto zipPath = dir.filePath("campaign.zip");
    std::vector<PackCandidate> offered;
    window.prompts.exportPack = [&](const std::vector<PackCandidate> &list) {
        offered = list;
        PackExportChoice choice;
        choice.name = "Cellars";
        for (std::size_t index = 0; index < list.size(); ++index) {
            if (!list[index].gameOwn) {
                choice.chosen.push_back(index);
            }
        }
        choice.path = zipPath;
        choice.zip = true;
        return std::optional(choice);
    };
    exportAction->trigger();
    CHECK(errors.isEmpty());
    CHECK_EQ(offered.size(), std::size_t{6});
    const auto pack = readPack(zipPath);
    CHECK(pack.has_value());
    CHECK(pack && pack->campaign.name == "Cellars");
    CHECK(pack && pack->campaign.game == GameProfile::speedyBlupi1);
    CHECK(pack && pack->worlds.size() == 3);
    if (pack) {
        QStringList names;
        for (const auto &world : pack->worlds) {
            names << world.name;
        }
        names.sort();
        CHECK(names == (QStringList{"u001-001.blp", "u001-002.blp", "world012.blp"}));
        CHECK(pack->worlds[0].bytes == raw(worldWith(7)) ||
              pack->worlds[1].bytes == raw(worldWith(7)) ||
              pack->worlds[2].bytes == raw(worldWith(7)));
    }
    // As a folder, and never over something that exists.
    const auto folderPath = dir.filePath("campaign-folder");
    window.prompts.exportPack = [&](const std::vector<PackCandidate> &) {
        return std::optional(PackExportChoice{"Folder", {3, 4}, folderPath, false});
    };
    CHECK(window.exportCampaignPack());
    CHECK(readPack(folderPath).has_value());
    errors.clear();
    CHECK(!window.exportCampaignPack());
    CHECK(errors.size() == 1 && errors[0].contains("exists already"));
    errors.clear();

    // Import into the other installation.
    CHECK(window.loadArtwork(other.root, GameProfile::speedyBlupi1));
    QCoreApplication::processEvents();
    CHECK(window.installation()->dataDirectory() == otherData.absolutePath());
    const auto otherBefore = directoryDigest(other.root);
    QStringList replacedAsked;
    bool accept = false;
    std::size_t worldsAsked = 0;
    window.prompts.confirmImportPack = [&](const CampaignPack &asked,
                                           const QStringList &replaced) {
        worldsAsked = asked.worlds.size();
        replacedAsked = replaced;
        return accept;
    };
    CHECK_EQ(window.importCampaignPack(zipPath), 0); // declined
    CHECK_EQ(worldsAsked, std::size_t{3});
    CHECK(replacedAsked == QStringList{"U001-001.BLP"});
    CHECK(directoryDigest(other.root) == otherBefore);

    // A failure at the second world: nothing stays.
    accept = true;
    int writes = 0;
    window.deployStore().failAt = [&](const char *step) {
        return QByteArray(step) == "write" && ++writes == 2;
    };
    errors.clear();
    CHECK_EQ(window.importCampaignPack(zipPath), 0);
    CHECK(errors.size() == 1 && errors[0].contains("Nothing was imported"));
    CHECK(directoryDigest(other.root) == otherBefore);
    window.deployStore().failAt = nullptr;

    // And for real: every world is in its slot, the replaced file keeps its
    // spelling, and each can be rolled back.
    const auto recordsBefore = window.deployStore().records().size();
    errors.clear();
    CHECK_EQ(window.importCampaignPack(zipPath), 3);
    CHECK(errors.isEmpty());
    CHECK(readAll(otherData.filePath("U001-001.BLP")) == raw(worldWith(4)));
    CHECK(readAll(otherData.filePath("u001-002.blp")) == raw(worldWith(5)));
    CHECK(readAll(otherData.filePath("world012.blp")) == raw(worldWith(7)));
    CHECK(readAll(otherData.filePath("WORLD001.BLP")) == raw(worldWith(41)));
    CHECK_EQ(window.deployStore().records().size(), recordsBefore + 3);
    CHECK_EQ(window.installation()->list()->count(), 4);
    window.prompts.confirmRollBack = [](const DeployRecord &) { return true; };
    CHECK(window.rollBackLastDeploy() && window.rollBackLastDeploy() &&
          window.rollBackLastDeploy());
    CHECK(directoryDigest(other.root) == otherBefore);
    // A folder pack is imported by its folder or by the manifest inside it.
    CHECK_EQ(window.importCampaignPack(folderPath + "/eggbert-campaign.json"), 2);
    CHECK(window.rollBackLastDeploy() && window.rollBackLastDeploy());
    CHECK_EQ(window.importCampaignPack(folderPath), 2);
    CHECK(window.rollBackLastDeploy() && window.rollBackLastDeploy());
    CHECK(directoryDigest(other.root) == otherBefore);

    // Packs that are not imported.
    {
        errors.clear();
        const auto bad = dir.filePath("bad.zip");
        write(bad, "this is not an archive");
        CHECK_EQ(window.importCampaignPack(bad), 0);
        CHECK(errors.size() == 1 && errors[0].contains("not a ZIP"));
        // For the other game.
        const auto second = makePack("Second", GameProfile::speedyBlupi2, {{true, 1, 3}},
                                     {raw(blankWorld(fxRevision13))});
        CHECK(second.has_value());
        const auto secondPath = dir.filePath("second.zip");
        CHECK(writePack(*second, secondPath, true).has_value());
        errors.clear();
        CHECK_EQ(window.importCampaignPack(secondPath), 0);
        CHECK(errors.size() == 1 && errors[0].contains("for the other game"));
        // Onto a world that is open in the editor.
        auto *open = window.openPath(otherData.filePath("U001-001.BLP"));
        CHECK(open != nullptr);
        errors.clear();
        CHECK_EQ(window.importCampaignPack(zipPath), 0);
        CHECK(errors.size() == 1 && errors[0].contains("is open in the editor"));
        CHECK(directoryDigest(other.root) == otherBefore);
    }

    // The export dialog: the game's own worlds cannot be ticked until the
    // box that says what that means is.
    {
        bool seen = false;
        QTimer::singleShot(0, &window, [&] {
            auto *dialog = qobject_cast<QDialog *>(QApplication::activeModalWidget());
            CHECK(dialog != nullptr);
            if (dialog == nullptr) {
                return;
            }
            seen = true;
            auto *list = dialog->findChild<QListWidget *>("packWorlds");
            auto *own = dialog->findChild<QCheckBox *>("packGameOwn");
            CHECK_EQ(list->count(), 6);
            int enabled = 0;
            int checked = 0;
            for (int row = 0; row < list->count(); ++row) {
                enabled += (list->item(row)->flags() & Qt::ItemIsEnabled) ? 1 : 0;
                checked += list->item(row)->checkState() == Qt::Checked ? 1 : 0;
            }
            CHECK_EQ(enabled, 3);
            CHECK_EQ(checked, 3);
            CHECK(list->item(0)->text().contains("the game's own file"));
            own->setChecked(true);
            enabled = 0;
            for (int row = 0; row < list->count(); ++row) {
                enabled += (list->item(row)->flags() & Qt::ItemIsEnabled) ? 1 : 0;
            }
            CHECK_EQ(enabled, 6);
            list->item(0)->setCheckState(Qt::Checked);
            own->setChecked(false); // and unticking takes the selection back
            CHECK(list->item(0)->checkState() == Qt::Unchecked);
            dialog->reject();
        });
        CHECK(!exportDialog(candidates).has_value());
        CHECK(seen);
    }
    QSettings().clear();
    return test::finish("campaign_ui");
}
