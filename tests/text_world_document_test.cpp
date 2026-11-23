// SPDX-License-Identifier: GPL-3.0-only
//
// The editable text world: every edit is one undo step that changes one
// token, and undo and redo restore the exact text.
#include "world/text_world_document.hpp"

#include "check.hpp"

using namespace eggbert;

namespace {

std::string fixture() {
    std::string text = "DescFile: posDecor=0;0 dimDecor=100;100 world=0 music=3 region=7 "
                       "blupiPos=66;774 blupiDir=2 \n";
    for (const auto *section : {"Decor: \n", "BigDecor: \n"}) {
        text += section;
        for (int x = 0; x < 100; ++x) {
            text += std::string(100, ',') + "\n";
        }
    }
    text += "MoveObject: type=5 posStart=258;196 posEnd=258;196 posCurrent=258;196 channel=10 "
            "icon=0 \n";
    return text;
}

} // namespace

int main() {
    const auto original = fixture();
    TextWorldDocument document(original);
    CHECK(document.text() == original);
    CHECK(!document.isModified() && !document.canUndo() && !document.canRedo());
    CHECK(document.undoLabel().empty());

    // Edits, each one step.
    CHECK(document.setTile(SectionKind::foreground, 3, 4, 40));
    CHECK(document.isModified() && document.canUndo());
    CHECK_EQ(document.undoLabel(), std::string("Decor (3, 4)"));
    const auto afterTile = document.text();
    CHECK_EQ(afterTile.size(), original.size() + 2);
    CHECK(document.setHeaderValue("music", 9));
    CHECK(document.setObjectPoint(0, "posEnd", {400, 196}));
    CHECK_EQ(document.undoLabel(), std::string("object 1 posEnd"));
    CHECK(document.setTile(SectionKind::bigDecor, 9, 9, 20));
    CHECK(document.setHeaderPoint("blupiPos", {130, 774}));
    CHECK(document.setObjectValue(0, "icon", 8));
    CHECK_EQ(*document.world().tile(SectionKind::foreground, 3, 4), 40);
    CHECK_EQ(*document.world().headerValue("music"), 9);
    CHECK_EQ(*document.world().objectValue(0, "icon"), 8);

    // Undo back to the start, redo forward again: exact texts.
    const auto edited = document.text();
    int steps = 0;
    while (document.undo()) {
        ++steps;
    }
    CHECK_EQ(steps, 6);
    CHECK(document.text() == original);
    CHECK(!document.isModified());
    CHECK(document.redo());
    CHECK(document.text() == afterTile);
    while (document.redo()) {
    }
    CHECK(document.text() == edited);
    // A new edit after an undo drops the redo branch.
    CHECK(document.undo());
    CHECK(document.setHeaderValue("region", 1));
    CHECK(!document.canRedo());

    // What changes nothing is no step.
    {
        TextWorldDocument fresh(original);
        CHECK(!fresh.setTile(SectionKind::foreground, 3, 4, -1)); // already empty
        CHECK(!fresh.setHeaderValue("music", 3));                 // already 3
        CHECK(!fresh.setTile(SectionKind::foreground, 3, 4, 40'000)); // out of range
        CHECK(!fresh.setTile(SectionKind::foreground, 3, 4, -2));
        CHECK(!fresh.setTile(SectionKind::foreground, 100, 0, 1)); // no such column
        CHECK(!fresh.setHeaderValue("nothing", 1));
        CHECK(!fresh.setObjectValue(3, "icon", 1));
        CHECK(!fresh.canUndo() && fresh.text() == original);
        // Saving is remembered.
        CHECK(fresh.setHeaderValue("music", 4));
        CHECK(fresh.isModified());
        fresh.markSaved();
        CHECK(!fresh.isModified());
        CHECK(fresh.undo());
        CHECK(fresh.isModified()); // the saved text is the edited one
    }
    // The history is bounded.
    {
        TextWorldDocument busy(original);
        for (long long value = 0; value < 520; ++value) {
            CHECK(busy.setHeaderValue("music", 100 + value));
        }
        std::size_t undone = 0;
        while (busy.undo()) {
            ++undone;
        }
        CHECK_EQ(undone, TextWorldDocument::historyLimit);
    }
    // Several cells at once are one step.
    {
        TextWorldDocument shapes(original);
        const std::vector<CellPos> cells{{10, 10}, {11, 10}, {12, 10}};
        CHECK(shapes.setTiles(SectionKind::foreground, cells, 55));
        CHECK_EQ(shapes.undoLabel(), std::string("Decor: 3 cell(s)"));
        for (const auto &cell : cells) {
            CHECK_EQ(*shapes.world().tile(SectionKind::foreground, cell.x, cell.y), 55);
        }
        CHECK_EQ(*shapes.world().tile(SectionKind::foreground, 13, 10), -1);
        CHECK(!shapes.setTiles(SectionKind::foreground, cells, 55)); // nothing changes
        CHECK(!shapes.setTiles(SectionKind::foreground, {}, 55));
        CHECK(!shapes.setTiles(SectionKind::foreground, cells, 40'000));
        CHECK(shapes.setTiles(SectionKind::bigDecor, cells, 7));
        CHECK_EQ(shapes.undoLabel(), std::string("BigDecor: 3 cell(s)"));
        CHECK(shapes.undo() && shapes.undo());
        CHECK(shapes.text() == original);
        CHECK(!shapes.undo());
    }
    // Whole object lines are steps too, undone to the exact text.
    {
        TextWorldDocument lines(original);
        const auto count = lines.world().objectCount();
        CHECK(count >= 1);
        CHECK(lines.duplicateObject(0));
        CHECK_EQ(lines.undoLabel(), std::string("Duplicate object 1"));
        CHECK(lines.addObject(12, {640, 128}, 10, 33));
        CHECK_EQ(lines.undoLabel(), std::string("Add object"));
        CHECK(lines.removeObject(0));
        CHECK_EQ(lines.undoLabel(), std::string("Remove object 1"));
        CHECK_EQ(lines.world().objectCount(), count + 1);
        CHECK(!lines.removeObject(count + 5));
        CHECK(!lines.addObject(0, {0, 0}, 1, 1));
        CHECK(lines.undo() && lines.undo() && lines.undo());
        CHECK(lines.text() == original && !lines.isModified());
        CHECK(lines.redo() && lines.redo() && lines.redo());
        CHECK_EQ(lines.world().objectCount(), count + 1);
    }
    // A file that is not a world still loads and writes back unchanged.
    {
        TextWorldDocument odd("just some text\nwith two lines");
        CHECK(odd.text() == "just some text\nwith two lines");
        CHECK(!odd.setTile(SectionKind::foreground, 0, 0, 1));
        CHECK(!odd.world().problems().empty());
    }
    return test::finish("text_world_document");
}
