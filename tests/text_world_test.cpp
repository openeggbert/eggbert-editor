// SPDX-License-Identifier: GPL-3.0-only
//
// Mobile Eggbert text worlds: an untouched file is written back byte for
// byte, an edit changes exactly one token, unknown content survives, and
// what does not have the expected shape is reported with its line. The
// fixtures are built here; nothing is taken from that project.
#include "world/text_world.hpp"

#include "check.hpp"

#include <algorithm>

using namespace eggbert;

namespace {

// A world with the given tiles (x, y, value) in Decor and one BigDecor tile.
std::string fixture() {
    std::string text =
        "DescFile: posDecor=0;550 dimDecor=100;0 world=0 music=3 region=7 blupiPos=66;774 "
        "blupiDir=2 \n";
    const auto block = [&](std::string_view name, int column, int row, int value) {
        text += std::string(name) + ": \n";
        for (int x = 0; x < 100; ++x) {
            for (int y = 0; y < 100; ++y) {
                if (x == column && y == row) {
                    text += std::to_string(value);
                }
                text += ',';
            }
            text += '\n';
        }
    };
    block("Decor", 3, 4, 40);
    block("BigDecor", 9, 9, 20);
    text += "MoveObject: type=5 stepAdvance=1 stepRecede=1 timeStopStart=0 timeStopEnd=0 "
            "posStart=258;196 posEnd=258;196 posCurrent=258;196 step=1 time=0 phase=0 channel=10 "
            "icon=0 \n";
    text += "MoveObject: type=1 stepAdvance=30 stepRecede=30 timeStopStart=10 timeStopEnd=10 "
            "posStart=640;832 posEnd=832;704 posCurrent=650;820 step=4 time=26 phase=7 channel=1 "
            "icon=29 \n";
    return text;
}

// The two texts differ in exactly one run of characters; returns the old and
// the new run.
std::pair<std::string, std::string> difference(const std::string &before,
                                               const std::string &after) {
    std::size_t prefix = 0;
    while (prefix < before.size() && prefix < after.size() && before[prefix] == after[prefix]) {
        ++prefix;
    }
    std::size_t suffix = 0;
    while (suffix < before.size() - prefix && suffix < after.size() - prefix &&
           before[before.size() - 1 - suffix] == after[after.size() - 1 - suffix]) {
        ++suffix;
    }
    return {before.substr(prefix, before.size() - prefix - suffix),
            after.substr(prefix, after.size() - prefix - suffix)};
}

bool mentions(const std::vector<std::string> &problems, std::string_view text) {
    return std::ranges::any_of(problems, [&](const std::string &problem) {
        return problem.find(text) != std::string::npos;
    });
}

} // namespace

int main() {
    const auto text = fixture();
    CHECK(looksLikeTextWorld(text));
    CHECK(!looksLikeTextWorld(""));
    CHECK(!looksLikeTextWorld("Decor: \n"));
    CHECK(!looksLikeTextWorld("DescFile without a colon\n"));
    CHECK(!looksLikeTextWorld(std::string_view("\x01\x00\x03\x00", 4)));

    // Untouched: byte for byte, whatever the ending.
    {
        const auto world = TextWorld::parse(text);
        CHECK(world.write() == text);
        CHECK_EQ(world.lineCount(), std::size_t{1 + 101 + 101 + 2 + 1}); // and the final break
        for (const std::string &variant :
             {text.substr(0, text.size() - 1), text + "\n\n", std::string(), std::string("\n"),
              std::string("no line break at all"), text + "Extra: something new=1 \n"}) {
            CHECK(TextWorld::parse(variant).write() == variant);
        }
        // Windows line ends and stray bytes are kept as they are.
        std::string windows;
        for (const char character : text) {
            windows += character == '\n' ? std::string("\r\n") : std::string(1, character);
        }
        CHECK(TextWorld::parse(windows).write() == windows);
        const std::string binary("DescFile: a=1 \n\x00\xff\x01,,\n", 22);
        CHECK(TextWorld::parse(binary).write() == binary);
    }

    // Reading.
    const auto world = TextWorld::parse(text);
    CHECK(world.problems().empty());
    CHECK_EQ(*world.headerValue("music"), 3);
    CHECK_EQ(*world.headerValue("region"), 7);
    CHECK_EQ(*world.headerValue("blupiDir"), 2);
    CHECK(*world.headerPoint("dimDecor") == (Point{100, 0}));
    CHECK(*world.headerPoint("blupiPos") == (Point{66, 774}));
    CHECK(!world.headerValue("nothing").has_value());
    CHECK(!world.headerValue("posDecor").has_value()); // two numbers, not one
    CHECK(!world.headerPoint("music").has_value());
    CHECK_EQ(*world.tile(SectionKind::foreground, 3, 4), 40);
    CHECK_EQ(*world.tile(SectionKind::foreground, 3, 5), -1);
    CHECK_EQ(*world.tile(SectionKind::foreground, 0, 0), -1);
    CHECK_EQ(*world.tile(SectionKind::foreground, 99, 99), -1);
    CHECK_EQ(*world.tile(SectionKind::bigDecor, 9, 9), 20);
    CHECK_EQ(*world.tile(SectionKind::bigDecor, 3, 4), -1);
    CHECK(!world.tile(SectionKind::foreground, 100, 0).has_value());
    CHECK(!world.tile(SectionKind::foreground, 0, 101).has_value());
    CHECK_EQ(world.objectCount(), std::size_t{2});
    CHECK_EQ(*world.objectValue(0, "type"), 5);
    CHECK_EQ(*world.objectValue(1, "stepAdvance"), 30);
    CHECK_EQ(*world.objectValue(1, "time"), 26);
    CHECK_EQ(*world.objectValue(1, "icon"), 29);
    CHECK(*world.objectPoint(1, "posEnd") == (Point{832, 704}));
    CHECK(!world.objectValue(2, "type").has_value());
    // "type" must not be found inside another field's name.
    CHECK_EQ(*world.objectValue(1, "step"), 4);

    // Each edit changes exactly one token.
    {
        auto edited = world;
        CHECK(edited.setHeaderValue("music", 10));
        CHECK(difference(text, edited.write()) == (std::pair<std::string, std::string>{"3", "10"}));
        CHECK_EQ(*edited.headerValue("music"), 10);
    }
    {
        auto edited = world;
        CHECK(edited.setHeaderPoint("blupiPos", {130, 900}));
        const auto change = difference(text, edited.write());
        CHECK(change.first == "66;774" && change.second == "130;900");
    }
    {
        auto edited = world;
        CHECK(edited.setTile(SectionKind::foreground, 50, 60, 12));
        CHECK(difference(text, edited.write()) == (std::pair<std::string, std::string>{"", "12"}));
        CHECK_EQ(*edited.tile(SectionKind::foreground, 50, 60), 12);
        CHECK_EQ(*edited.tile(SectionKind::foreground, 50, 61), -1);
        CHECK_EQ(*edited.tile(SectionKind::bigDecor, 50, 60), -1); // the other block is untouched
        // Emptying it again restores the original text.
        CHECK(edited.setTile(SectionKind::foreground, 50, 60, -1));
        CHECK(edited.write() == text);
    }
    {
        auto edited = world;
        CHECK(edited.setTile(SectionKind::bigDecor, 9, 9, 21));
        CHECK(difference(text, edited.write()) == (std::pair<std::string, std::string>{"0", "1"}));
        CHECK_EQ(*edited.tile(SectionKind::foreground, 3, 4), 40);
    }
    {
        auto edited = world;
        CHECK(edited.setObjectValue(1, "icon", 30));
        CHECK(edited.setObjectPoint(0, "posStart", {300, 200}));
        CHECK_EQ(*edited.objectValue(1, "icon"), 30);
        CHECK(*edited.objectPoint(0, "posStart") == (Point{300, 200}));
        CHECK(*edited.objectPoint(0, "posEnd") == (Point{258, 196}));
        CHECK_EQ(*edited.objectValue(0, "type"), 5);
    }
    // What cannot be edited is refused and nothing changes.
    {
        auto edited = world;
        CHECK(!edited.setHeaderValue("nothing", 1));
        CHECK(!edited.setTile(SectionKind::foreground, 100, 0, 1));
        CHECK(!edited.setTile(SectionKind::foreground, 0, 500, 1));
        CHECK(!edited.setObjectValue(5, "icon", 1));
        CHECK(!edited.setObjectValue(0, "nothing", 1));
        CHECK(edited.write() == text);
    }

    // Unknown fields and lines are kept through an edit.
    {
        auto extended = text;
        extended.replace(extended.find("music=3"), 7, "music=3 futureField=abc");
        extended += "FutureSection: x=1 \n# a remark\n";
        auto parsed = TextWorld::parse(extended);
        CHECK(parsed.problems().empty());
        CHECK(parsed.setHeaderValue("region", 8));
        auto expected = extended;
        expected.replace(expected.find("region=7"), 8, "region=8");
        CHECK(parsed.write() == expected);
    }

    // Shape problems, each with its line.
    {
        CHECK(mentions(TextWorld::parse("").problems(), "no DescFile line"));
        const auto headerless = TextWorld::parse("Decor: ");
        CHECK(mentions(headerless.problems(), "no DescFile line"));
        CHECK(mentions(headerless.problems(), "The Decor block ends after 0 of 100 lines"));
        CHECK(mentions(headerless.problems(), "no BigDecor block"));

        auto noSpace = text;
        noSpace.replace(noSpace.find("blupiDir=2 \n"), 12, "blupiDir=2\n");
        CHECK(mentions(TextWorld::parse(noSpace).problems(),
                       "Line 1: the field blupiDir is missing or is not a number followed by"));

        auto wordy = text;
        wordy.replace(wordy.find("music=3"), 7, "music=loud");
        CHECK(mentions(TextWorld::parse(wordy).problems(), "Line 1: the field music"));

        // A column with too few entries, and one with a word in it.
        auto lines = text;
        const auto third = lines.find('\n', lines.find('\n', lines.find('\n') + 1) + 1);
        const auto fourth = lines.find('\n', third + 1);
        lines.replace(third + 1, fourth - third - 1, "1,2,3");
        CHECK(mentions(TextWorld::parse(lines).problems(),
                       "Line 4: the line has 3 entries; a column has 100 cells."));
        auto worded = text;
        worded.replace(worded.find(",,,40"), 5, ",,x,40");
        CHECK(mentions(TextWorld::parse(worded).problems(), "is not a number"));

        // A block cut short by the next section.
        auto shortBlock = text;
        const auto big = shortBlock.find("BigDecor: \n");
        const auto lastRow = shortBlock.rfind('\n', big - 2);
        shortBlock.erase(lastRow + 1, big - lastRow - 1);
        CHECK(mentions(TextWorld::parse(shortBlock).problems(),
                       "the Decor block has only 99 of 100 lines before the next section"));

        auto badObject = text;
        badObject.replace(badObject.find("posEnd=832;704"), 14, "posEnd=832");
        CHECK(mentions(TextWorld::parse(badObject).problems(), "the object field posEnd"));
    }

    // Whole object lines: removing, duplicating and adding change exactly
    // those lines, whatever the line endings and the end of the file.
    {
        const std::string first = text.substr(text.find("MoveObject: type=5"));
        const auto firstLine = first.substr(0, first.find('\n') + 1);
        const auto secondLine = first.substr(firstLine.size());
        const auto head = text.substr(0, text.size() - first.size());
        CHECK_EQ(textWorldMostObjects, std::size_t{200});

        auto removed = TextWorld::parse(text);
        CHECK(removed.removeObject(0));
        CHECK(removed.write() == head + secondLine);
        CHECK_EQ(removed.objectCount(), std::size_t{1});
        CHECK_EQ(*removed.objectValue(0, "type"), 1);
        CHECK(!removed.removeObject(1));
        CHECK(removed.removeObject(0));
        CHECK(removed.write() == head);
        CHECK(!removed.removeObject(0));

        auto doubled = TextWorld::parse(text);
        CHECK(doubled.duplicateObject(0));
        CHECK(doubled.write() == head + firstLine + firstLine + secondLine);
        CHECK_EQ(doubled.objectCount(), std::size_t{3});
        CHECK(!doubled.duplicateObject(3));
        CHECK(doubled.problems().empty());

        auto grown = TextWorld::parse(text);
        CHECK(grown.addObject(12, {640, 128}, 10, 33));
        const std::string added = "MoveObject: type=12 stepAdvance=1 stepRecede=1 timeStopStart=0 "
                                  "timeStopEnd=0 posStart=640;128 posEnd=640;128 "
                                  "posCurrent=640;128 step=1 time=0 phase=0 channel=10 icon=33 \n";
        CHECK(grown.write() == text + added);
        CHECK_EQ(*grown.objectValue(2, "type"), 12);
        CHECK(*grown.objectPoint(2, "posCurrent") == (Point{640, 128}));
        CHECK_EQ(*grown.objectValue(2, "icon"), 33);
        CHECK(grown.problems().empty());
        CHECK(!grown.addObject(0, {0, 0}, 1, 1)); // type 0 would end the game's list

        // A world without objects, with and without a final line break.
        auto bare = TextWorld::parse(head);
        CHECK(bare.addObject(12, {640, 128}, 10, 33));
        CHECK(bare.write() == head + added);
        const auto open = head.substr(0, head.size() - 1);
        auto unfinished = TextWorld::parse(open);
        CHECK(unfinished.addObject(12, {640, 128}, 10, 33));
        CHECK(unfinished.write() == head + added.substr(0, added.size() - 1));
        // The last line of a file without a final break.
        const auto cut = text.substr(0, text.size() - 1);
        auto tail = TextWorld::parse(cut);
        CHECK(tail.duplicateObject(1));
        CHECK(tail.write() == text + secondLine.substr(0, secondLine.size() - 1));
        CHECK(tail.removeObject(2) && tail.write() == cut);
        CHECK(tail.addObject(12, {640, 128}, 10, 33));
        CHECK(tail.write() == text + added.substr(0, added.size() - 1));
        CHECK(tail.removeObject(2) && tail.removeObject(1));
        CHECK(tail.write() == head + firstLine.substr(0, firstLine.size() - 1));

        // Windows line ends are continued.
        const auto windows = [](const std::string &unix) {
            std::string converted;
            for (const char character : unix) {
                converted += character == '\n' ? std::string("\r\n") : std::string(1, character);
            }
            return converted;
        };
        auto dos = TextWorld::parse(windows(text));
        CHECK(dos.addObject(12, {640, 128}, 10, 33));
        CHECK(dos.write() == windows(text + added));
        CHECK(dos.duplicateObject(2) && dos.write() == windows(text + added + added));
        CHECK(dos.removeObject(3) && dos.removeObject(0));
        CHECK(dos.write() == windows(head + secondLine + added));
        auto dosCut = TextWorld::parse(windows(cut));
        CHECK(dosCut.duplicateObject(1));
        CHECK(dosCut.write() ==
              windows(text) + secondLine.substr(0, secondLine.size() - 1));
        CHECK(dosCut.removeObject(2) && dosCut.write() == windows(cut));
        CHECK(dosCut.addObject(12, {640, 128}, 10, 33));
        CHECK(dosCut.write() == windows(text) + added.substr(0, added.size() - 1));

        // The pool of the game: no more than 200 object lines are made, and
        // what the game would not read is reported.
        auto full = TextWorld::parse(text);
        while (full.objectCount() < 200) {
            CHECK(full.duplicateObject(0));
        }
        CHECK(!full.duplicateObject(0));
        CHECK(!full.addObject(12, {0, 0}, 1, 1));
        CHECK(full.problems().empty());
        const auto over = TextWorld::parse(full.write() + firstLine);
        CHECK(mentions(over.problems(), "There are 201 object lines; the game reads the first 200."));
        auto stopped = TextWorld::parse(text);
        CHECK(stopped.setObjectValue(0, "type", 0));
        CHECK(mentions(stopped.problems(), "the game stops reading objects here and ignores the 1 "
                                           "object line(s) after it"));
        // A last object of type 0 hides nothing.
        auto ending = TextWorld::parse(text);
        CHECK(ending.setObjectValue(1, "type", 0));
        CHECK(!mentions(ending.problems(), "stops reading"));
    }

    // The description.
    {
        const auto report = world.describe();
        CHECK(report.find("Mobile Eggbert text world") != std::string::npos);
        CHECK(report.find("music 3; region 7; blupiPos 66, 774; blupiDir 2") != std::string::npos);
        CHECK(report.find("Decor: 1 occupied cell(s)") != std::string::npos);
        CHECK(report.find("BigDecor: 1 occupied cell(s)") != std::string::npos);
        CHECK(report.find("objects: 2, type 1 x 1, type 5 x 1") != std::string::npos);
        CHECK(report.find("The file has the expected shape.") != std::string::npos);
        CHECK(report.find("never converted to or from BLP") != std::string::npos);
        CHECK(TextWorld::parse("DescFile: \n").describe().find("problem(s):") != std::string::npos);
    }
    return test::finish("text_world");
}
