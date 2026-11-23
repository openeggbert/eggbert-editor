// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDialog>
#include <QStringList>

#include <optional>

#include "world/campaign_plan.hpp"

class QLabel;
class QPushButton;
class QTreeWidget;

namespace eggbert {

// What the user asked for in the Campaign Builder.
struct CampaignRequest {
    enum Kind {
        // Open the world in a tab.
        open,
        // Create the world in a free number: a hub, or a mission ending
        // with the goal.
        create,
        // Create a mission ending with the hub's key.
        createWithKey,
        // Offer the world's sign for placing in the world that leads to it.
        placeSign,
    };
    Kind kind = open;
    int number = 0;

    friend bool operator==(const CampaignRequest &, const CampaignRequest &) = default;
};

// The campaign of an installation as a tree: world 1, every hub the game
// can hold with its eight mission places, and the final world; which exist,
// which are free, and what each still needs. It only collects a request;
// the main window carries it out.
class CampaignDialog final : public QDialog {
    Q_OBJECT

public:
    CampaignDialog(const CampaignPlan &plan, const QStringList &findings,
                   QWidget *parent = nullptr);

    [[nodiscard]] std::optional<CampaignRequest> request() const { return request_; }
    [[nodiscard]] QTreeWidget *tree() const { return tree_; }
    // Selects the row of a world number; false when the tree has none.
    bool selectWorld(int number);

private:
    void updateButtons();
    void finish(CampaignRequest::Kind kind);

    CampaignPlan plan_;
    std::optional<CampaignRequest> request_;
    QTreeWidget *tree_ = nullptr;
    QLabel *hint_ = nullptr;
    QPushButton *open_ = nullptr;
    QPushButton *create_ = nullptr;
    QPushButton *createKey_ = nullptr;
    QPushButton *sign_ = nullptr;
};

} // namespace eggbert
