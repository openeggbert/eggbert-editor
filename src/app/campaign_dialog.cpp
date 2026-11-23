// SPDX-License-Identifier: GPL-3.0-only
#include "app/campaign_dialog.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "app/catalog_texts.hpp"

namespace eggbert {

namespace {

constexpr int numberRole = Qt::UserRole;
constexpr int presentRole = Qt::UserRole + 1;
constexpr int needsSignRole = Qt::UserRole + 2;
constexpr int missionRole = Qt::UserRole + 3;

} // namespace

CampaignDialog::CampaignDialog(const CampaignPlan &plan, const QStringList &findings,
                               QWidget *parent)
    : QDialog(parent), plan_(plan) {
    setWindowTitle(tr("Campaign Builder"));
    auto *layout = new QVBoxLayout(this);

    int hubs = 0;
    int missions = 0;
    for (const auto &hub : plan_.hubs) {
        hubs += hub.present ? 1 : 0;
        for (const auto &mission : hub.missions) {
            missions += mission.present ? 1 : 0;
        }
    }
    auto *summary = new QLabel(
        tr("This game's campaign holds up to %1 hubs (worlds 10 to %2) with %3 missions each. "
           "Here: %4 hub(s) and %5 mission(s). World 1 has %6 barrier(s); the game opens one "
           "for every hub that is won.")
            .arg(plan_.shape.hubCount)
            .arg(plan_.shape.hubCount * 10)
            .arg(plan_.shape.missionsPerHub)
            .arg(hubs)
            .arg(missions)
            .arg(plan_.barriers),
        this);
    summary->setObjectName(QStringLiteral("campaignSummary"));
    summary->setWordWrap(true);
    layout->addWidget(summary);

    tree_ = new QTreeWidget(this);
    tree_->setObjectName(QStringLiteral("campaignTree"));
    tree_->setAccessibleName(tr("Worlds of the campaign"));
    tree_->setHeaderLabels({tr("World"), tr("State"), tr("What it has and needs")});
    tree_->setRootIsDecorated(true);
    tree_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    tree_->header()->setStretchLastSection(true);
    const auto row = [&](QTreeWidgetItem *parentItem, int number, const QString &name,
                         bool present, const QStringList &notes, bool needsSign, bool mission) {
        auto *item = parentItem != nullptr ? new QTreeWidgetItem(parentItem)
                                           : new QTreeWidgetItem(tree_);
        item->setText(0, tr("World %1 - %2").arg(number).arg(name));
        item->setText(1, present ? tr("present") : tr("free"));
        item->setText(2, notes.join(QStringLiteral("; ")));
        item->setToolTip(2, item->text(2));
        item->setData(0, numberRole, number);
        item->setData(0, presentRole, present);
        item->setData(0, needsSignRole, needsSign);
        item->setData(0, missionRole, mission);
        if (!present) {
            for (int column = 0; column < 3; ++column) {
                item->setForeground(column, palette().brush(QPalette::Disabled, QPalette::Text));
            }
        }
        return item;
    };
    row(nullptr, 1, catalogText(worldRoleName(WorldRole::start)), plan_.firstWorld,
        plan_.firstWorld ? QStringList{} : QStringList{tr("the game starts every campaign here")},
        false, false);
    for (std::size_t index = 0; index < plan_.hubs.size(); ++index) {
        const auto &hub = plan_.hubs[index];
        QStringList notes;
        bool needsSign = false;
        if (hub.present) {
            if (!hub.signInFirstWorld) {
                notes << tr("no sign in world 1 leads here");
                needsSign = plan_.firstWorld;
            }
            int keys = 0;
            int present = 0;
            for (const auto &mission : hub.missions) {
                keys += mission.hasKey ? 1 : 0;
                present += mission.present ? 1 : 0;
            }
            notes << tr("%1 of %2 missions").arg(present).arg(hub.missions.size());
            if (present != 0 && keys == 0) {
                notes << tr("no mission ends with the key");
            }
        } else if (hub.signInFirstWorld) {
            notes << tr("world 1 has a sign leading here");
        }
        auto *hubItem = row(nullptr, hub.number,
                            tr("hub %1").arg(static_cast<int>(index) + 1), hub.present, notes,
                            needsSign, false);
        for (std::size_t slot = 0; slot < hub.missions.size(); ++slot) {
            const auto &mission = hub.missions[slot];
            QStringList missionNotes;
            bool missionNeedsSign = false;
            if (mission.present) {
                missionNotes << (mission.hasKey ? tr("ends with the key (completes the hub)")
                                 : mission.hasGoal ? tr("ends with the goal")
                                                   : tr("has neither goal nor key"));
                if (!mission.signInHub) {
                    missionNotes << tr("no sign in the hub leads here");
                    missionNeedsSign = hub.present;
                } else if (slot != 0 && !mission.doorInHub) {
                    missionNotes << tr("its sign has no door: open from the start");
                }
            } else if (mission.signInHub) {
                missionNotes << tr("the hub has a sign leading here");
            }
            row(hubItem, mission.number, tr("mission %1").arg(static_cast<int>(slot) + 1),
                mission.present, missionNotes, missionNeedsSign, true);
        }
        hubItem->setExpanded(hub.present);
    }
    row(nullptr, plan_.shape.finale, catalogText(worldRoleName(WorldRole::finale)), plan_.finale,
        plan_.finale ? QStringList{}
                     : QStringList{tr("the game enters it after world 1 is won")},
        false, false);
    layout->addWidget(tree_, 1);

    auto *checks = new QPlainTextEdit(this);
    checks->setObjectName(QStringLiteral("campaignFindings"));
    checks->setAccessibleName(tr("What the checks found"));
    checks->setReadOnly(true);
    checks->setPlainText(findings.isEmpty() ? tr("The checks found nothing to report.")
                                            : findings.join(QLatin1Char('\n')));
    checks->setMaximumHeight(130);
    layout->addWidget(checks);

    hint_ = new QLabel(this);
    hint_->setObjectName(QStringLiteral("campaignHint"));
    hint_->setWordWrap(true);
    layout->addWidget(hint_);

    auto *buttons = new QHBoxLayout;
    open_ = new QPushButton(tr("Open"), this);
    open_->setObjectName(QStringLiteral("campaignOpen"));
    create_ = new QPushButton(tr("Create World Here"), this);
    create_->setObjectName(QStringLiteral("campaignCreate"));
    createKey_ = new QPushButton(tr("Create Mission with the Key"), this);
    createKey_->setObjectName(QStringLiteral("campaignCreateKey"));
    createKey_->setToolTip(tr("The mission that completes its hub: it ends with the key instead "
                              "of the goal. A hub needs one."));
    sign_ = new QPushButton(tr("Place Its Sign..."), this);
    sign_->setObjectName(QStringLiteral("campaignSign"));
    sign_->setToolTip(tr("Opens the world that leads to this one with the sign ready to place: "
                         "click where it goes."));
    buttons->addWidget(open_);
    buttons->addWidget(create_);
    buttons->addWidget(createKey_);
    buttons->addWidget(sign_);
    buttons->addStretch(1);
    auto *close = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->addWidget(close);
    layout->addLayout(buttons);

    connect(close, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(open_, &QPushButton::clicked, this, [this] { finish(CampaignRequest::open); });
    connect(create_, &QPushButton::clicked, this, [this] { finish(CampaignRequest::create); });
    connect(createKey_, &QPushButton::clicked, this,
            [this] { finish(CampaignRequest::createWithKey); });
    connect(sign_, &QPushButton::clicked, this, [this] { finish(CampaignRequest::placeSign); });
    connect(tree_, &QTreeWidget::currentItemChanged, this, [this] { updateButtons(); });
    connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem *item) {
        if (item != nullptr && item->data(0, presentRole).toBool()) {
            finish(CampaignRequest::open);
        }
    });
    updateButtons();
    resize(820, 640);
}

bool CampaignDialog::selectWorld(int number) {
    for (QTreeWidgetItemIterator item(tree_); *item != nullptr; ++item) {
        if ((*item)->data(0, numberRole).toInt() == number) {
            tree_->setCurrentItem(*item);
            return true;
        }
    }
    return false;
}

void CampaignDialog::updateButtons() {
    const auto *item = tree_->currentItem();
    const bool chosen = item != nullptr;
    const bool present = chosen && item->data(0, presentRole).toBool();
    const bool mission = chosen && item->data(0, missionRole).toBool();
    const int number = chosen ? item->data(0, numberRole).toInt() : 0;
    // World 1 and the final world are ordinary worlds made with New World.
    const bool place = chosen && number != 1 && number != plan_.shape.finale;
    open_->setEnabled(present);
    create_->setEnabled(place && !present);
    createKey_->setEnabled(place && !present && mission);
    sign_->setEnabled(present && item->data(0, needsSignRole).toBool());
    hint_->setText(!chosen ? tr("Choose a world: one that is present can be opened, a free "
                                "number can get a new world.")
                   : !present && place
                         ? (mission ? tr("A new mission starts as the game's own editor starts "
                                         "one. Its sign for the hub is then offered for placing.")
                                    : tr("A new hub starts as an empty platform. Its sign for "
                                         "world 1 is then offered for placing."))
                         : QString());
}

void CampaignDialog::finish(CampaignRequest::Kind kind) {
    const auto *item = tree_->currentItem();
    if (item == nullptr) {
        return;
    }
    request_ = CampaignRequest{kind, item->data(0, numberRole).toInt()};
    accept();
}

} // namespace eggbert
