// SPDX-License-Identifier: GPL-3.0-only
//
// Campaign order and the review by numbering.
#include "world/campaign.hpp"

#include "check.hpp"

#include <algorithm>

using namespace eggbert;

namespace {

CampaignEntry official(int number) {
    return {{false, 1, number}, worldSlotFileName({false, 1, number}), 50'468, "", ""};
}

CampaignEntry designed(int gamer, int number) {
    return {{true, gamer, number}, worldSlotFileName({true, gamer, number}), 50'468, "", ""};
}

bool mentions(const std::vector<CampaignFinding> &findings, std::string_view text,
              CampaignSeverity severity) {
    return std::ranges::any_of(findings, [&](const CampaignFinding &finding) {
        return finding.severity == severity && finding.text.find(text) != std::string::npos;
    });
}

} // namespace

int main() {
    // Order: start, groups (hub then missions), finale, network, demo,
    // other numbers, designed worlds.
    {
        Campaign campaign{"Test", GameProfile::speedyBlupi1, {}};
        for (const int number : {200, 99, 12, 101, 10, 1, 21, 20, 11, 95}) {
            campaign.entries.push_back(official(number));
        }
        campaign.entries.push_back(designed(2, 1));
        campaign.entries.push_back(designed(1, 3));
        campaign.entries.insert(campaign.entries.begin(), designed(1, 2));
        std::vector<std::string> names;
        for (const auto &entry : campaignOrder(campaign)) {
            names.push_back(entry.fileName);
        }
        const std::vector<std::string> expected{
            "world001.blp", "world010.blp", "world011.blp", "world012.blp", "world020.blp",
            "world021.blp", "world099.blp", "world101.blp", "world200.blp", "world095.blp",
            "u001-002.blp", "u001-003.blp", "u002-001.blp"};
        CHECK(names == expected);
        // The same numbers mean something else in the second game: 95 and
        // 101 are missions there and 99 is outside the scheme.
        campaign.game = GameProfile::speedyBlupi2;
        names.clear();
        for (const auto &entry : campaignOrder(campaign)) {
            names.push_back(entry.fileName);
        }
        CHECK(names[6] == "world095.blp"); // a mission of the ninth group there
        CHECK(names[7] == "world101.blp"); // and one of the tenth
        CHECK(std::ranges::find(names, "world101.blp") < std::ranges::find(names, "world099.blp"));
    }

    // A complete little campaign has nothing to report.
    {
        Campaign campaign{"Complete", GameProfile::speedyBlupi1, {}};
        for (const int number : {1, 10, 11, 12, 99}) {
            campaign.entries.push_back(official(number));
        }
        campaign.entries.push_back(designed(1, 1));
        CHECK(reviewCampaign(campaign).empty());
    }
    // What the numbering shows.
    {
        Campaign campaign{"Gaps", GameProfile::speedyBlupi1, {}};
        for (const int number : {10, 21, 22, 30, 95, 11}) {
            campaign.entries.push_back(official(number));
        }
        campaign.entries.push_back(official(11)); // twice
        campaign.entries.push_back({{true, 9, 1}, "bad.blp", 1, "", ""});
        const auto findings = reviewCampaign(campaign);
        CHECK(mentions(findings, "World 1 is missing", CampaignSeverity::warning));
        CHECK(mentions(findings, "Hub 20 is missing", CampaignSeverity::warning));
        CHECK(mentions(findings, "Hub 30 has none of its missions", CampaignSeverity::note));
        CHECK(mentions(findings, "World 95 is outside the campaign numbering",
                       CampaignSeverity::note));
        CHECK(mentions(findings, "world011.blp is used by more than one entry",
                       CampaignSeverity::warning));
        CHECK(mentions(findings, "not in a slot the games use (bad.blp)",
                       CampaignSeverity::warning));
        // No finale note without world 1 (the game never gets there).
        CHECK(!mentions(findings, "World 99", CampaignSeverity::note));
        CHECK(!mentions(findings, "Hub 10", CampaignSeverity::note));
        for (const auto &finding : findings) {
            CHECK(!finding.basis.empty()); // every finding carries its basis
        }
    }
    {
        Campaign campaign{"No finale", GameProfile::speedyBlupi2, {official(1), official(10),
                                                                   official(11)}};
        const auto findings = reviewCampaign(campaign);
        CHECK_EQ(findings.size(), std::size_t{1});
        CHECK(mentions(findings, "World 199", CampaignSeverity::note));
    }
    // Designed worlds only, or no game: nothing to say about numbering.
    {
        Campaign campaign{"Mine", GameProfile::speedyBlupi1, {designed(1, 1), designed(1, 2)}};
        CHECK(reviewCampaign(campaign).empty());
        Campaign unknown{"?", GameProfile::unknown, {official(5)}};
        CHECK(reviewCampaign(unknown).empty());
    }
    return test::finish("campaign");
}
