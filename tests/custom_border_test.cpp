// SPDX-License-Identifier: GPL-3.0-only
//
// User-written border rules: the text format with its errors, and the
// stroke: forms follow the joined neighbours, only rule cells are reshaped,
// only the foreground changes.
#include "world/custom_border.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <algorithm>

using namespace eggbert;
using namespace eggbert::test;

namespace {

// Form for mask m is 500 + m, so a stored value shows its mask.
constexpr std::string_view wall =
    "# a test rule\n"
    "[Wall]\n"
    "forms = 500 501 502 503 504 505 506 507 508 509 510 511 512 513 514 515\n";

std::int16_t cellOf(const EditBuilder &builder, std::size_t x, std::size_t y) {
    return *builder.view().foreground()->cell(x, y);
}

bool has(const std::vector<CellPos> &cells, CellPos cell) {
    return std::ranges::find(cells, cell) != cells.end();
}

} // namespace

int main() {
    // Parsing.
    {
        const auto rules = parseCustomBorderRules(wall);
        CHECK(rules.has_value());
        CHECK_EQ(rules->size(), std::size_t{1});
        CHECK_EQ(rules->front().name, std::string("Wall"));
        CHECK_EQ(rules->front().forms[0], 500);
        CHECK_EQ(rules->front().forms[15], 515);
        CHECK(rules->front().contains(507) && !rules->front().contains(516));

        const auto two = parseCustomBorderRules(
            "[A]\r\nforms = 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1\r\n\r\n"
            "  [ B b ]  \n\tforms\t=\t2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 32767\n");
        CHECK(two.has_value() && two->size() == 2);
        CHECK(two.has_value() && (*two)[1].name == "B b" && (*two)[1].forms[15] == 32'767);

        const auto error = [](std::string_view text) {
            const auto parsed = parseCustomBorderRules(text);
            return parsed ? std::string() : parsed.error();
        };
        CHECK(error("") == "The file contains no rule.");
        CHECK(error("# only a comment\n") == "The file contains no rule.");
        CHECK(error("forms = 1\n").starts_with("Line 1: forms appear before"));
        CHECK(error("[A]\n").starts_with("Line 1: this rule has no"));
        CHECK(error("[A]\n[B]\nforms = 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1\n")
                  .starts_with("Line 1: this rule has no"));
        CHECK(error("[A]\nforms = 1 2 3\n").find("exactly sixteen forms, this line has 3") !=
              std::string::npos);
        CHECK(error("[A]\nforms = 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1\n").find("has 17") !=
              std::string::npos);
        CHECK(error("[A]\nforms = 1 1 1 1 1 1 1 x 1 1 1 1 1 1 1 1\n").starts_with("Line 2: \"x\""));
        CHECK(error("[A]\nforms = 1 1 1 1 1 1 1 -1 1 1 1 1 1 1 1 1\n").starts_with("Line 2: \"-1\""));
        CHECK(error("[A]\nforms = 1 1 1 1 1 1 1 32768 1 1 1 1 1 1 1 1\n").starts_with("Line 2:"));
        CHECK(error("[A]\nshape = 1\n").starts_with("Line 2: expected"));
        CHECK(error("[]\n").starts_with("Line 1: a rule starts"));
        CHECK(error("[A\n").starts_with("Line 1: a rule starts"));
        CHECK(error("[A]\nforms = 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1\n"
                    "forms = 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1 1\n")
                  .starts_with("Line 3: this rule already"));
    }

    const auto rule = parseCustomBorderRules(wall)->front();
    const auto world = blankWorld(fxRevision13);

    // One cell in the open: no neighbour joins.
    {
        WorldDocument document{Bytes(world)};
        EditBuilder builder(document);
        const CellPos cell{50, 50};
        const auto result = customBorderStroke(builder, rule, {&cell, 1}, false);
        CHECK_EQ(cellOf(builder, 50, 50), 500);
        CHECK_EQ(result.changedFootprint.size(), std::size_t{1});
        CHECK(result.sideEffects.empty());
        // Exactly one cell of the foreground changed and nothing else.
        const auto transaction = builder.build("Custom border");
        CHECK_EQ(transaction.changedByteCount(), std::size_t{2});
    }
    // A horizontal run of three: ends join one side, the middle both.
    {
        WorldDocument document{Bytes(world)};
        EditBuilder builder(document);
        const CellPos cells[] = {{50, 50}, {51, 50}, {52, 50}};
        const auto result = customBorderStroke(builder, rule, cells, false);
        CHECK_EQ(cellOf(builder, 50, 50), 500 + 4);     // joins right
        CHECK_EQ(cellOf(builder, 51, 50), 500 + 4 + 8); // right and left
        CHECK_EQ(cellOf(builder, 52, 50), 500 + 8);     // joins left
        CHECK_EQ(result.changedFootprint.size(), std::size_t{3});
        CHECK(result.sideEffects.empty());

        // A cell above the middle: it joins below, the middle now joins above
        // as a side effect.
        CHECK(document.apply(builder.build("Run")).has_value());
        EditBuilder second(document);
        const CellPos above{51, 49};
        const auto more = customBorderStroke(second, rule, {&above, 1}, false);
        CHECK_EQ(cellOf(second, 51, 49), 500 + 1);
        CHECK_EQ(cellOf(second, 51, 50), 500 + 2 + 4 + 8);
        CHECK(has(more.changedFootprint, above));
        CHECK(has(more.sideEffects, CellPos{51, 50}));
        CHECK_EQ(more.sideEffects.size(), std::size_t{1});

        // Erasing the middle: the three around it lose that join.
        CHECK(document.apply(second.build("Above")).has_value());
        EditBuilder third(document);
        const CellPos middle{51, 50};
        const auto erased = customBorderStroke(third, rule, {&middle, 1}, true);
        CHECK_EQ(cellOf(third, 51, 50), -1);
        CHECK_EQ(cellOf(third, 50, 50), 500);
        CHECK_EQ(cellOf(third, 52, 50), 500);
        CHECK_EQ(cellOf(third, 51, 49), 500);
        CHECK_EQ(erased.sideEffects.size(), std::size_t{3});
    }
    // The edge of the map counts as joined; cells outside are ignored.
    {
        WorldDocument document{Bytes(world)};
        EditBuilder builder(document);
        const CellPos cells[] = {{0, 0}, {99, 99}, {100, 5}, {5, 100}};
        const auto result = customBorderStroke(builder, rule, cells, false);
        CHECK_EQ(cellOf(builder, 0, 0), 500 + 2 + 8);   // above and left are outside
        CHECK_EQ(cellOf(builder, 99, 99), 500 + 1 + 4); // below and right are outside
        CHECK_EQ(result.changedFootprint.size(), std::size_t{2});
    }
    // Tiles of other kinds neither join nor are reshaped; the back layer and
    // the objects are untouched.
    {
        auto bytes = world;
        put16(bytes, fxCellOffset(fxForegroundOffset, 51, 50), 30); // some other tile
        put16(bytes, fxCellOffset(fxRevision13.bigDecorOffset, 50, 50), 20);
        put16(bytes, fxSlotOffset(fxRevision13, 3), 5);
        WorldDocument document{Bytes(bytes)};
        EditBuilder builder(document);
        const CellPos cell{50, 50};
        customBorderStroke(builder, rule, {&cell, 1}, false);
        CHECK_EQ(cellOf(builder, 50, 50), 500);
        CHECK_EQ(cellOf(builder, 51, 50), 30);
        const auto transaction = builder.build("Custom border");
        CHECK_EQ(transaction.changes.size(), std::size_t{1});
        CHECK_EQ(transaction.changes.front().offset, fxCellOffset(fxForegroundOffset, 50, 50));
        CHECK_EQ(transaction.changedByteCount(), std::size_t{2});
    }
    // A revision without the layer present (truncated) does nothing.
    {
        auto truncated = world;
        truncated.resize(900);
        WorldDocument document{std::move(truncated)};
        EditBuilder builder(document);
        const CellPos cell{50, 50};
        const auto result = customBorderStroke(builder, rule, {&cell, 1}, false);
        CHECK_EQ(result.changedCount(), std::size_t{0});
        CHECK(!builder.hasChanges());
    }
    return test::finish("custom_border");
}
