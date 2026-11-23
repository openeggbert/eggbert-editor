// SPDX-License-Identifier: GPL-3.0-only
//
// A Mobile Eggbert text world as the editor's image: every value lands in
// its place, writing back changes only the tokens that changed, objects
// gain and lose whole lines, and what the text cannot store is refused.
#include "world/new_world.hpp"
#include "world/text_world_image.hpp"

#include "world/world_probe.hpp"
#include "world/world_view.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <algorithm>
#include <string>

using namespace eggbert;
using namespace eggbert::test;

namespace {

const char *const firstObject =
    "MoveObject: type=5 stepAdvance=1 stepRecede=1 timeStopStart=0 timeStopEnd=0 "
    "posStart=258;196 posEnd=258;196 posCurrent=258;196 step=1 time=0 phase=0 channel=10 "
    "icon=0 \n";
const char *const secondObject =
    "MoveObject: type=1 stepAdvance=20 stepRecede=30 timeStopStart=5 timeStopEnd=6 "
    "posStart=640;832 posEnd=832;704 posCurrent=650;820 step=4 time=26 phase=7 channel=1 "
    "icon=29 \n";

std::string fixture() {
    std::string text = "DescFile: posDecor=0;550 dimDecor=100;0 world=3 music=10 region=7 "
                       "blupiPos=66;774 blupiDir=2 \n";
    for (const auto *section : {"Decor: \n", "BigDecor: \n"}) {
        text += section;
        for (int x = 0; x < 100; ++x) {
            std::string line(99, ',');
            if (x == 3 && std::string(section) == "Decor: \n") {
                line = ",,,,40" + std::string(95, ',');
            }
            if (x == 9 && std::string(section) == "BigDecor: \n") {
                line = "20" + std::string(99, ',');
            }
            text += line + "\n";
        }
    }
    return text + firstObject + secondObject;
}

WorldView viewOf(const Bytes &bytes) { return WorldView(bytes, probeWorld(bytes)); }

void set16(Bytes &bytes, std::size_t offset, int value) { put16(bytes, offset, value); }

} // namespace

int main() {
    const auto text = fixture();
    auto world = TextWorld::parse(text);
    CHECK(world.problems().empty());

    // Every value lands where a revision 1.3 world keeps it.
    auto made = makeTextWorldImage(world);
    CHECK(made.has_value());
    auto image = *made;
    {
        const auto probe = probeWorld(image.bytes);
        CHECK(probe.status == ProbeStatus::sizeMatches);
        CHECK(probe.candidateRevision == FormatRevision::v1_3);
        CHECK_EQ(image.bytes.size(), std::size_t{50'468});
        const auto view = viewOf(image.bytes);
        const auto header = *view.header();
        CHECK(header.posDecor() == (Point{0, 550}));
        CHECK(header.dimDecor() == (Point{100, 0}));
        CHECK_EQ(header.world(), 3);
        CHECK_EQ(header.music(), 10);
        CHECK_EQ(header.region(), 7);
        for (std::size_t index = 0; index < 4; ++index) {
            CHECK(*header.blupiPos(index) == (Point{66, 774}));
            CHECK_EQ(*header.blupiDir(index), 2);
        }
        CHECK_EQ(header.nameLength(), std::size_t{0});
        CHECK_EQ(*view.foreground()->cell(3, 4), 40);
        CHECK_EQ(*view.foreground()->cell(3, 5), -1);
        CHECK_EQ(*view.foreground()->cell(0, 0), -1);
        CHECK_EQ(*view.bigDecor()->cell(9, 0), 20);
        CHECK_EQ(view.objects()->activeCount(), std::size_t{2});
        const auto mover = *view.objects()->record(1);
        CHECK(mover.type == 1 && mover.stepAdvance == 20 && mover.stepRecede == 30);
        CHECK(mover.timeStopStart == 5 && mover.timeStopEnd == 6);
        CHECK(mover.posStart == (Point{640, 832}) && mover.posEnd == (Point{832, 704}));
        CHECK(mover.posCurrent == (Point{650, 820}));
        CHECK(mover.step == 4 && mover.time == 26 && mover.phase == 7);
        CHECK(mover.channel == 1 && mover.icon == 29);
        CHECK(image.lineOfSlot[0] == std::optional<std::size_t>(0));
        CHECK(image.lineOfSlot[1] == std::optional<std::size_t>(1));
        CHECK(!image.lineOfSlot[2].has_value());
        CHECK(image.pristine == image.bytes);
    }

    // Written back unchanged, the text is the same text.
    CHECK(storeTextWorldImage(world, image, image.bytes).has_value());
    CHECK(world.write() == text);

    // One cell, one header value: one token each.
    {
        auto bytes = image.bytes;
        set16(bytes, fxCellOffset(fxForegroundOffset, 3, 5), 41);
        set16(bytes, fxCellOffset(fxRevision13.bigDecorOffset, 50, 99), 7);
        set16(bytes, 222, 4); // music
        put32(bytes, 328, 130);
        put32(bytes, 332, 262);
        for (std::size_t index = 1; index < 4; ++index) { // the one start, in all four slots
            put32(bytes, 328 + 8 * index, 130);
            put32(bytes, 332 + 8 * index, 262);
        }
        auto edited = world;
        auto mapping = image;
        CHECK(storeTextWorldImage(edited, mapping, bytes).has_value());
        const auto written = edited.write();
        CHECK(written != text);
        CHECK_EQ(*edited.tile(SectionKind::foreground, 3, 5), 41);
        CHECK_EQ(*edited.tile(SectionKind::bigDecor, 50, 99), 7);
        CHECK_EQ(*edited.headerValue("music"), 4);
        CHECK(*edited.headerPoint("blupiPos") == (Point{130, 262}));
        // 41 (2), 7 (1), 10 -> 4 (-1), 66;774 -> 130;262 (+1)
        CHECK_EQ(written.size(), text.size() + 3);
        CHECK(written.find(firstObject) != std::string::npos);
        CHECK(written.find(secondObject) != std::string::npos);
        // And the image made from the new text is the edited image.
        CHECK(makeTextWorldImage(edited)->bytes == bytes);
        // Writing the old image back restores the old text exactly.
        CHECK(storeTextWorldImage(edited, mapping, image.bytes).has_value());
        CHECK(edited.write() == text);
    }

    // Objects: a changed number changes its token; a freed slot loses its
    // line; a new object gets a line after the others; the slots keep
    // their lines through further saves.
    {
        auto bytes = image.bytes;
        const auto slot1 = fxSlotOffset(fxRevision13, 1);
        set16(bytes, slot1 + 44, 31); // the mover's icon
        auto edited = world;
        auto mapping = image;
        CHECK(storeTextWorldImage(edited, mapping, bytes).has_value());
        CHECK_EQ(*edited.objectValue(1, "icon"), 31);
        CHECK_EQ(edited.write().size(), text.size());

        // Slot 0 freed, a new object in slot 7.
        for (std::size_t offset = 0; offset < 48; ++offset) {
            bytes[fxSlotOffset(fxRevision13, 0) + offset] = std::byte{0};
        }
        const auto slot7 = fxSlotOffset(fxRevision13, 7);
        set16(bytes, slot7, 12);
        set16(bytes, slot7 + 2, 1);
        set16(bytes, slot7 + 4, 1);
        for (const std::size_t field : {12u, 20u, 28u}) {
            put32(bytes, slot7 + field, 1280);
            put32(bytes, slot7 + field + 4, 64);
        }
        set16(bytes, slot7 + 36, 1);
        set16(bytes, slot7 + 42, 1);
        set16(bytes, slot7 + 44, 30);
        CHECK(storeTextWorldImage(edited, mapping, bytes).has_value());
        CHECK_EQ(edited.objectCount(), std::size_t{2});
        CHECK_EQ(*edited.objectValue(0, "type"), 1);
        CHECK_EQ(*edited.objectValue(1, "type"), 12);
        CHECK(*edited.objectPoint(1, "posEnd") == (Point{1280, 64}));
        CHECK(!mapping.lineOfSlot[0].has_value());
        CHECK(mapping.lineOfSlot[1] == std::optional<std::size_t>(0));
        CHECK(mapping.lineOfSlot[7] == std::optional<std::size_t>(1));
        const auto afterObjects = edited.write();
        CHECK(afterObjects.find(firstObject) == std::string::npos);
        CHECK(afterObjects.ends_with(
            "MoveObject: type=12 stepAdvance=1 stepRecede=1 timeStopStart=0 timeStopEnd=0 "
            "posStart=1280;64 posEnd=1280;64 posCurrent=1280;64 step=1 time=0 phase=0 channel=1 "
            "icon=30 \n"));
        // Saving again changes nothing, and a later change of the new
        // object finds its line.
        CHECK(storeTextWorldImage(edited, mapping, bytes).has_value());
        CHECK(edited.write() == afterObjects);
        set16(bytes, slot7 + 44, 33);
        CHECK(storeTextWorldImage(edited, mapping, bytes).has_value());
        CHECK_EQ(*edited.objectValue(1, "icon"), 33);
        CHECK_EQ(*edited.objectValue(0, "icon"), 31);
        // Read again, the objects are the same, in line order.
        const auto reread = makeTextWorldImage(edited);
        CHECK(reread.has_value());
        const auto pool = *viewOf(reread->bytes).objects();
        CHECK_EQ(pool.activeCount(), std::size_t{2});
        CHECK(*pool.record(0) == *viewOf(bytes).objects()->record(1));
        CHECK(*pool.record(1) == *viewOf(bytes).objects()->record(7));
    }

    // What a text world has no place for is refused, and nothing changes.
    {
        const auto refused = [&](const Bytes &bytes, std::string_view reason) {
            auto edited = world;
            auto mapping = image;
            const auto result = storeTextWorldImage(edited, mapping, bytes);
            return !result.has_value() && result.error().find(reason) != std::string::npos &&
                   edited.write() == text && mapping.lineOfSlot == image.lineOfSlot;
        };
        auto titled = image.bytes;
        titled[376] = std::byte{'A'};
        CHECK(refused(titled, "no title"));
        auto secondStart = image.bytes;
        put32(secondStart, 328 + 8, 999);
        CHECK(refused(secondStart, "one start"));
        auto direction = image.bytes;
        put32(direction, 360 + 4, 1);
        CHECK(refused(direction, "one start"));
        auto reserved = image.bytes;
        reserved[10] = std::byte{1};
        CHECK(refused(reserved, "reserved"));
        auto unknown = image.bytes;
        set16(unknown, fxSlotOffset(fxRevision13, 0) + 10, 1);
        CHECK(refused(unknown, "cannot store"));
        CHECK(refused(blankWorld(fxRevision12), "not the image"));
        CHECK(refused(Bytes(100), "not the image"));
    }

    // Texts the editor's layout cannot hold are left to the text editor.
    {
        const auto reason = [](const std::string &input) {
            const auto result = makeTextWorldImage(TextWorld::parse(input));
            return result ? std::string() : result.error();
        };
        CHECK(!reason("DescFile: \n").empty());
        auto large = text;
        large.replace(large.find(",,,,40"), 6, ",,,,40000");
        CHECK(reason(large).find("too large") != std::string::npos);
        auto stopped = text;
        stopped.replace(stopped.find("type=5"), 6, "type=0");
        CHECK(!reason(stopped).empty());
        auto wide = text;
        wide.replace(wide.find("channel=10"), 10, "channel=70000");
        CHECK(reason(wide).find("too large") != std::string::npos);
        std::string crowded = text;
        for (int index = 0; index < 199; ++index) {
            crowded += firstObject;
        }
        CHECK(reason(crowded).find("201 object lines") != std::string::npos);
        // Exactly 200 objects fit.
        crowded.erase(crowded.size() - std::string(firstObject).size());
        CHECK(reason(crowded).empty());
    }

    // A missing final line break survives a save; with Windows line ends
    // the last entry of a line is not a number as the game reads it, so
    // such a file is left to the text editor.
    {
        const auto open = text.substr(0, text.size() - 1);
        auto unfinished = TextWorld::parse(open);
        auto mapping = makeTextWorldImage(unfinished);
        CHECK(mapping.has_value());
        if (mapping.has_value()) {
            CHECK(storeTextWorldImage(unfinished, *mapping, mapping->bytes).has_value());
            CHECK(unfinished.write() == open);
            auto bytes = mapping->bytes;
            set16(bytes, fxCellOffset(fxForegroundOffset, 0, 0), 5);
            CHECK(storeTextWorldImage(unfinished, *mapping, bytes).has_value());
            CHECK_EQ(unfinished.write().size(), open.size() + 1);
            CHECK_EQ(*unfinished.tile(SectionKind::foreground, 0, 0), 5);
        }
        std::string dos;
        for (const char character : text) {
            dos += character == '\n' ? std::string("\r\n") : std::string(1, character);
        }
        // Windows line ends belong to the line breaks: the same image, and
        // a store that keeps every carriage return where it was.
        auto windows = TextWorld::parse(dos);
        CHECK(windows.hasCarriageReturns() && !TextWorld::parse(text).hasCarriageReturns());
        CHECK(windows.problems().empty());
        auto fromDos = makeTextWorldImage(windows);
        const auto fromUnix = makeTextWorldImage(TextWorld::parse(text));
        CHECK(fromDos.has_value() && fromUnix.has_value() && fromDos->bytes == fromUnix->bytes);
        if (fromDos) {
            CHECK(storeTextWorldImage(windows, *fromDos, fromDos->bytes).has_value());
            CHECK(windows.write() == dos);
            // The last entry of a line is written before its carriage return,
            // and a new object line ends like the others.
            auto bytes = fromDos->bytes;
            set16(bytes, fxCellOffset(fxForegroundOffset, 0, 99), 41);
            set16(bytes, fxSlotOffset(fxRevision13, 150), 5);
            CHECK(storeTextWorldImage(windows, *fromDos, bytes).has_value());
            const auto written = windows.write();
            CHECK_EQ(*TextWorld::parse(written).tile(SectionKind::foreground, 0, 99), 41);
            CHECK(written.find(",41\r\n") != std::string::npos);
            CHECK_EQ(std::ranges::count(written, '\n'), std::ranges::count(written, '\r'));
            CHECK_EQ(std::ranges::count(written, '\n'), std::ranges::count(dos, '\n') + 1);
        }
    }
    // A new world: what the Windows games' editors start with, as text.
    {
        const auto text = newTextWorld();
        CHECK(text.starts_with("DescFile: posDecor=0;0 dimDecor=100;100 world=0 music=1 region=2 "
                               "blupiPos=194;198 blupiDir=2 \nDecor: \n"));
        const auto world = TextWorld::parse(text);
        CHECK(world.problems().empty());
        CHECK_EQ(world.objectCount(), std::size_t{2});
        CHECK_EQ(*world.tile(SectionKind::foreground, 3, 4), 40);
        CHECK_EQ(*world.tile(SectionKind::foreground, 5, 4), 39);
        CHECK_EQ(*world.objectValue(1, "type"), 7);
        CHECK_EQ(*world.objectValue(1, "icon"), 29);
        const auto image = makeTextWorldImage(world);
        CHECK(image.has_value() && image->bytes == newWorld(GameProfile::mobileEggbert));
        // 1 header line, 2 x (1 + 100) layer lines, 2 object lines.
        CHECK_EQ(std::ranges::count(text, '\n'), 205);
    }
    // One start: the first is copied over the other three.
    {
        auto bytes = newWorld(GameProfile::mobileEggbert);
        put32(bytes, 328 + 8, 77);  // second start, x
        put32(bytes, 360 + 12, 1);  // fourth direction
        CHECK(bytes != newWorld(GameProfile::mobileEggbert));
        mirrorTextWorldStart(bytes);
        CHECK(bytes == newWorld(GameProfile::mobileEggbert));
        Bytes other(100, std::byte{1});
        mirrorTextWorldStart(other);
        CHECK(other == Bytes(100, std::byte{1}));
    }
    return test::finish("text_world_image");
}
