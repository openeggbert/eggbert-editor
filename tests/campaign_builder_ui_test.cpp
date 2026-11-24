// SPDX-License-Identifier: GPL-3.0-only
//
// The Campaign Builder: what a campaign holds and has free, creating a hub
// and missions in free numbers, and offering their signs for placing.
#include "app/campaign_dialog.hpp"
#include "app/deploy_store.hpp"
#include "app/main_window.hpp"
#include "app/world_canvas.hpp"
#include "world/campaign_plan.hpp"
#include "world/new_world.hpp"
#include "world/text_world.hpp"
#include "world/text_world_image.hpp"

#include "check.hpp"
#include "fixture.hpp"
#include "generated_game.hpp"
#include "generated_port.hpp"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

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

bool lists(const QStringList &lines, const QString &text) {
    for (const auto &line : lines) {
        if (line.contains(text)) {
            return true;
        }
    }
    return false;
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("eggbert-editor-test");
    QCoreApplication::setApplicationName("campaign-builder-ui");
    QSettings().clear();
    QDir(DeployStore::defaultDirectory()).removeRecursively();
    QTemporaryDir dir;

    // A first-game installation: world 1 with the sign of hub 10 and two
    // barriers, hub 10 with the sign of mission 1, and mission 11 (a goal).
    const auto game = makeGame(dir.filePath("game"));
    const QDir data(game.root + "/DATA");
    {
        auto first = blankLargeWorld(fxRevision12);
        put16(first, fxCellOffset(fxForegroundOffset, 20, 20), 158);
        put16(first, fxCellOffset(fxForegroundOffset, 30, 20), 183);
        put16(first, fxCellOffset(fxForegroundOffset, 40, 20), 183);
        write(data.filePath("WORLD001.BLP"), raw(first));
        auto hub = blankLargeWorld(fxRevision12);
        put16(hub, fxCellOffset(fxForegroundOffset, 8, 8), 174);
        write(data.filePath("WORLD010.BLP"), raw(hub));
        auto mission = blankWorld(fxRevision12);
        put16(mission, fxSlotOffset(fxRevision12, 0), 7);
        write(data.filePath("WORLD011.BLP"), raw(mission));
    }

    MainWindow window;
    QStringList errors;
    window.prompts.error = [&](const QString &, const QString &text) { errors << text; };
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    window.resize(1200, 800);
    window.show();
    auto *action = window.findChild<QAction *>("fileBuildCampaign");
    CHECK(action != nullptr && !action->isEnabled());
    CHECK(!window.buildCampaign()); // no installation yet
    CHECK_EQ(errors.size(), 1);
    errors.clear();

    auto *first = window.openPath(data.filePath("WORLD001.BLP"));
    CHECK(first != nullptr);
    CHECK(window.loadArtwork(game.root, GameProfile::speedyBlupi1));
    QCoreApplication::processEvents();
    CHECK(action->isEnabled());

    // The plan of what is there.
    {
        const auto plan = window.campaignPlan();
        CHECK(plan.game == GameProfile::speedyBlupi1 && plan.firstWorld && !plan.finale);
        CHECK_EQ(plan.barriers, 2);
        CHECK_EQ(plan.hubs.size(), std::size_t{8});
        CHECK(plan.hubs[0].present && plan.hubs[0].signInFirstWorld);
        CHECK(plan.hubs[0].missions[0].present && plan.hubs[0].missions[0].signInHub);
        CHECK(!plan.hubs[1].present);
        const auto findings = window.campaignFindings();
        CHECK(lists(findings, "No mission of hub 10 ends with a key"));
        CHECK(lists(findings, "World 99"));
        CHECK(lists(findings, "World 1 has 2 barriers (tile 183) but only 1 hubs"));
    }

    // The dialog: rows for every place, buttons by what is chosen.
    {
        CampaignDialog dialog(window.campaignPlan(), window.campaignFindings());
        // World 1, eight hubs, the final world; eight missions under each hub.
        CHECK_EQ(dialog.tree()->topLevelItemCount(), 10);
        CHECK_EQ(dialog.tree()->topLevelItem(1)->childCount(), 8);
        CHECK(dialog.tree()->topLevelItem(1)->text(0).contains("World 10"));
        CHECK_EQ(dialog.tree()->topLevelItem(1)->text(1), QString("present"));
        CHECK(dialog.tree()->topLevelItem(1)->text(2).contains("1 of 8 missions"));
        CHECK(dialog.tree()->topLevelItem(1)->text(2).contains("no mission ends with the key"));
        CHECK_EQ(dialog.tree()->topLevelItem(2)->text(1), QString("free"));
        auto *open = dialog.findChild<QPushButton *>("campaignOpen");
        auto *create = dialog.findChild<QPushButton *>("campaignCreate");
        auto *createKey = dialog.findChild<QPushButton *>("campaignCreateKey");
        auto *sign = dialog.findChild<QPushButton *>("campaignSign");
        CHECK(!open->isEnabled() && !create->isEnabled() && !sign->isEnabled());
        CHECK(dialog.selectWorld(11));
        CHECK(open->isEnabled() && !create->isEnabled() && !createKey->isEnabled());
        CHECK(dialog.selectWorld(12)); // a free mission
        CHECK(!open->isEnabled() && create->isEnabled() && createKey->isEnabled());
        CHECK(dialog.selectWorld(20)); // a free hub
        CHECK(create->isEnabled() && !createKey->isEnabled());
        CHECK(dialog.selectWorld(1));
        CHECK(open->isEnabled() && !create->isEnabled());
        CHECK(dialog.selectWorld(99)); // the final world is made with New World
        CHECK(!open->isEnabled() && !create->isEnabled());
        CHECK(!dialog.selectWorld(90));
        CHECK(dialog.selectWorld(12));
        createKey->click();
        CHECK(dialog.result() == QDialog::Accepted);
        CHECK(dialog.request() == (CampaignRequest{CampaignRequest::createWithKey, 12}));
    }

    // Creating the mission that ends hub 10: the file appears, opens in a
    // tab, and its door and sign are offered in the hub.
    std::optional<CampaignRequest> answer = CampaignRequest{CampaignRequest::createWithKey, 12};
    int asked = 0;
    window.prompts.campaignBuilder = [&](const CampaignPlan &, const QStringList &) {
        ++asked;
        return answer;
    };
    CHECK(window.buildCampaign());
    CHECK_EQ(asked, 1);
    CHECK(errors.isEmpty());
    const auto missionPath = data.filePath("world012.blp");
    CHECK(readAll(missionPath) ==
          raw(newCampaignWorld(GameProfile::speedyBlupi1, CampaignWorldKind::keyMission)));
    auto *hubTab = window.currentTab();
    CHECK(hubTab != nullptr && hubTab->file().path().endsWith("WORLD010.BLP"));
    CHECK(window.tabCount() == 3);
    if (hubTab != nullptr) {
        const auto *stamp = hubTab->pendingStamp();
        CHECK(stamp != nullptr && stamp->width == 2 && stamp->height == 1);
        CHECK(stamp != nullptr && stamp->foreground == (std::vector<std::int16_t>{182, 175}));
        CHECK(window.statusBar()->currentMessage().contains("click in hub 10 where its door goes"));
        // Placing it: the door at the cursor, the sign to its right.
        hubTab->canvas()->setCursorCell({20, 30});
        QTest::keyClick(hubTab->canvas(), Qt::Key_Return);
        CHECK(hubTab->pendingStamp() == nullptr);
        CHECK_EQ(*hubTab->document().view().foreground()->cell(20, 30), 182);
        CHECK_EQ(*hubTab->document().view().foreground()->cell(21, 30), 175);
        // The plan sees the unsaved hub: mission 12 has its sign and door,
        // and the hub now has a mission with the key.
        const auto plan = window.campaignPlan();
        CHECK(plan.hubs[0].missions[1].present && plan.hubs[0].missions[1].hasKey &&
              plan.hubs[0].missions[1].signInHub && plan.hubs[0].missions[1].doorInHub);
        CHECK(!lists(window.campaignFindings(), "No mission of hub 10"));
        CHECK(hubTab->undo());
        CHECK(!window.campaignPlan().hubs[0].missions[1].signInHub);
    }

    // A new hub: created with the exit, its sign offered in world 1.
    answer = CampaignRequest{CampaignRequest::create, 20};
    CHECK(window.buildCampaign());
    CHECK(readAll(data.filePath("world020.blp")) ==
          raw(newCampaignWorld(GameProfile::speedyBlupi1, CampaignWorldKind::hub)));
    CHECK(window.currentTab() == first);
    CHECK(first->pendingStamp() != nullptr &&
          first->pendingStamp()->foreground == (std::vector<std::int16_t>{159}));
    first->canvas()->setCursorCell({22, 20});
    QTest::keyClick(first->canvas(), Qt::Key_Return);
    CHECK_EQ(*first->document().view().foreground()->cell(22, 20), 159);
    CHECK(window.campaignPlan().hubs[1].present && window.campaignPlan().hubs[1].signInFirstWorld);
    CHECK(window.campaignPlan().hubs[1].hasExit);

    // The first mission of a hub gets a sign without a door.
    answer = CampaignRequest{CampaignRequest::create, 21};
    CHECK(window.buildCampaign());
    CHECK(readAll(data.filePath("world021.blp")) == raw(newWorld(GameProfile::speedyBlupi1)));
    {
        auto *hub20 = window.currentTab();
        CHECK(hub20 != nullptr && hub20->file().path().endsWith("world020.blp"));
        CHECK(hub20->pendingStamp() != nullptr &&
              hub20->pendingStamp()->foreground == (std::vector<std::int16_t>{174}));
        QTest::keyClick(hub20->canvas(), Qt::Key_Escape);
        // Asked for again from the dialog.
        answer = CampaignRequest{CampaignRequest::placeSign, 21};
        CHECK(window.buildCampaign());
        CHECK(window.currentTab() == hub20 && hub20->pendingStamp() != nullptr);
        QTest::keyClick(hub20->canvas(), Qt::Key_Escape);
    }

    // What cannot be created is refused with a reason; nothing is written.
    const auto before = data.entryList(QDir::Files);
    CHECK(window.createCampaignWorld(11, false) == nullptr); // exists
    CHECK(window.createCampaignWorld(1, false) == nullptr);  // not a hub or mission
    CHECK(window.createCampaignWorld(90, false) == nullptr); // the first game has eight hubs
    CHECK(window.createCampaignWorld(19, false) == nullptr); // a hub has eight missions
    CHECK_EQ(errors.size(), 4);
    CHECK(errors.value(0).contains("already exists"));
    errors.clear();
    CHECK(data.entryList(QDir::Files) == before);
    const auto external = data.filePath("WORLD022.BLP");
    write(external, "added after the panel was populated");
    CHECK(window.createCampaignWorld(22, false) == nullptr);
    CHECK(readAll(external) == "added after the panel was populated");
    CHECK(QFile::remove(external));
    errors.clear();
    // Opening from the dialog, and closing it.
    answer = CampaignRequest{CampaignRequest::open, 11};
    CHECK(window.buildCampaign());
    CHECK(window.currentTab()->file().path().endsWith("WORLD011.BLP"));
    answer = std::nullopt;
    CHECK(!window.buildCampaign());

    // A created world can be taken out again.
    window.prompts.confirmRollBack = [](const DeployRecord &) { return true; };
    {
        // Its tab is closed first: the file is put back behind an open tab otherwise.
        for (int index = window.tabCount() - 1; index >= 0; --index) {
            if (window.tabAt(index)->file().path().endsWith("world021.blp")) {
                CHECK(window.closeTab(window.tabAt(index)));
            }
        }
        CHECK(window.rollBackLastDeploy());
        CHECK(!QFile::exists(data.filePath("world021.blp")));
        CHECK(QFile::exists(data.filePath("world020.blp")));
    }

    // Mobile Eggbert: a new hub is a text world without the exit tile, and
    // the sign offered in world 1 is the one for its number.
    {
        const auto port = makePort(dir.filePath("port"));
        QFile::rename(port.worldPath, port.root + "/worlds/world001.txt");
        MainWindow second;
        second.prompts.error = [&](const QString &, const QString &text) { errors << text; };
        second.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
        auto *start = second.openPath(port.root + "/worlds/world001.txt");
        CHECK(start != nullptr && start->file().isTextWorld());
        QCoreApplication::processEvents();
        CHECK_EQ(second.campaignPlan().hubs.size(), std::size_t{14});
        auto *created = second.createCampaignWorld(130, false);
        CHECK(created != nullptr && created->file().isTextWorld());
        const auto text = readAll(port.root + "/worlds/world130.txt");
        CHECK(text.startsWith("DescFile: "));
        const auto world = TextWorld::parse(text.toStdString());
        CHECK(world.problems().empty() && world.objectCount() == 0);
        CHECK_EQ(*world.tile(SectionKind::foreground, 5, 3), -1);
        CHECK(second.currentTab() == start);
        CHECK(start->pendingStamp() != nullptr &&
              start->pendingStamp()->foreground == (std::vector<std::int16_t>{414}));
        CHECK(errors.isEmpty());
    }
    return test::finish("campaign_builder_ui");
}
