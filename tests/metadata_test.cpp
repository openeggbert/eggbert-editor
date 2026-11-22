// SPDX-License-Identifier: GPL-3.0-only
#include "world/metadata.hpp"
#include "world/profile.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <algorithm>

using namespace eggbert;
using namespace eggbert::test;

namespace {

std::vector<std::size_t> diffPositions(std::span<const std::byte> left, const Bytes &right) {
    std::vector<std::size_t> positions;
    for (std::size_t index = 0; index < right.size(); ++index) {
        if (left[index] != right[index]) {
            positions.push_back(index);
        }
    }
    return positions;
}

bool within(const std::vector<std::size_t> &positions, std::size_t offset, std::size_t size) {
    return !positions.empty() && std::ranges::all_of(positions, [&](std::size_t position) {
        return position >= offset && position < offset + size;
    });
}

std::vector<std::byte> ascii(std::string_view text) {
    std::vector<std::byte> bytes;
    for (const char character : text) {
        bytes.push_back(static_cast<std::byte>(character));
    }
    return bytes;
}

} // namespace

int main() {
    // Start geometry.
    CHECK((startPositionForCell({0, 0}) == Point{2, 6}));
    CHECK((startPositionForCell({3, 3}) == Point{194, 198}));
    CHECK((startPositionForCell({99, 0}) == Point{6338, 6}));
    CHECK((startPositionForCell({0, 99}) == Point{2, 6342}));
    CHECK((cellOfPosition({194, 198}).value() == CellPos{3, 3}));
    CHECK((cellOfPosition({6399, 0}).value() == CellPos{99, 0}));
    CHECK((cellOfPosition({64, 6399}).value() == CellPos{1, 99}));
    CHECK(!cellOfPosition({-1, 0}).has_value());
    CHECK(!cellOfPosition({0, 6400}).has_value());

    // Windows-1252: every byte round-trips; unrepresentable text is refused.
    {
        Bytes all;
        for (unsigned value = 0x20; value <= 0xFF; ++value) {
            all.push_back(static_cast<std::byte>(value));
        }
        const auto text = decodeWindows1252(all);
        CHECK_EQ(text.size(), all.size());
        CHECK(encodeWindows1252(text).value() == all);
        CHECK_EQ(text[0x80 - 0x20], char32_t{0x20AC}); // euro sign
        CHECK_EQ(text[0x9C - 0x20], char32_t{0x0153}); // oe ligature
        CHECK_EQ(text[0xE9 - 0x20], char32_t{0x00E9}); // e acute
        CHECK_EQ(text[0x81 - 0x20], char32_t{0x0081}); // undefined byte kept
        CHECK((encodeWindows1252(U"Café €").value() ==
               Bytes{std::byte{'C'}, std::byte{'a'}, std::byte{'f'}, std::byte{0xE9},
                     std::byte{' '}, std::byte{0x80}}));
        CHECK(!encodeWindows1252(U"č").has_value());  // c caron: not in 1252
        CHECK(!encodeWindows1252(U"a\nb").has_value());    // control character
        CHECK(!encodeWindows1252(std::u32string_view(U"a\0b", 3)).has_value());
        CHECK(!encodeWindows1252(U"\U0001F600").has_value());
        CHECK(encodeWindows1252(U"").value().empty());
    }

    // Reading decoded metadata from literal offsets.
    auto input = patternedWorld(fxRevision13, 7);
    putText(input, 376, "Title");
    put8(input, 381, 0);
    put16(input, 222, 4);  // music
    put16(input, 224, 17); // region
    put32(input, 204, 640);
    put32(input, 208, 1280);
    put32(input, 212, 37); // unusual non-zero dimDecor.x
    put32(input, 216, 0);
    for (int index = 0; index < 4; ++index) {
        put32(input, 328 + 8 * index, 100 * (index + 1) + 2);
        put32(input, 332 + 8 * index, 200 * (index + 1) + 6);
        put32(input, 360 + 4 * index, index == 2 ? 7 : 1 + index % 2);
    }
    {
        const WorldView view(input);
        const auto metadata = readMetadata(view).value();
        CHECK(metadata.name == ascii("Title"));
        CHECK_EQ(metadata.music, std::int16_t{4});
        CHECK_EQ(metadata.region, std::int16_t{17});
        CHECK(metadata.scrollsHorizontally);
        CHECK(!metadata.scrollsVertically);
        CHECK((metadata.posDecor == Point{640, 1280}));
        CHECK((metadata.starts[0] == StartInfo{{102, 206}, 1}));
        CHECK((metadata.starts[1] == StartInfo{{202, 406}, 2}));
        CHECK((metadata.starts[2] == StartInfo{{302, 606}, 7}));
        CHECK((metadata.starts[3] == StartInfo{{402, 806}, 2}));
        auto cut = input;
        cut.resize(500);
        CHECK(!readMetadata(WorldView(cut)).has_value());
    }

    // Applying identical metadata changes nothing.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        CHECK(applyMetadata(builder, readMetadata(document.view()).value()));
        CHECK(!builder.hasChanges());
    }

    // Each field alone touches only its own bytes.
    struct Case {
        const char *label;
        std::size_t offset;
        std::size_t size;
        void (*edit)(Metadata &);
    };
    const Case cases[] = {
        {"music", 222, 2, [](Metadata &m) { m.music = 9; }},
        {"region", 224, 2, [](Metadata &m) { m.region = 3; }},
        {"posDecor", 204, 8, [](Metadata &m) { m.posDecor = {0, 50}; }},
        {"scroll y on", 216, 4, [](Metadata &m) { m.scrollsVertically = true; }},
        {"scroll x off", 212, 4, [](Metadata &m) { m.scrollsHorizontally = false; }},
        {"start 1 position", 328, 8, [](Metadata &m) { m.starts[0].position = {66, 70}; }},
        {"start 4 position", 352, 8, [](Metadata &m) { m.starts[3].position = {-5, 6405}; }},
        {"start 2 direction", 364, 4, [](Metadata &m) { m.starts[1].direction = 1; }},
        {"start 3 direction", 368, 4, [](Metadata &m) { m.starts[2].direction = 2; }},
        {"name", 376, 9, [](Metadata &m) { m.name = ascii("New name"); }},
    };
    for (const auto &entry : cases) {
        WorldDocument document(input);
        auto metadata = readMetadata(document.view()).value();
        entry.edit(metadata);
        EditBuilder builder(document);
        CHECK(applyMetadata(builder, metadata));
        CHECK(document.apply(builder.build(entry.label)).has_value());
        CHECK(within(diffPositions(document.currentBytes(), input), entry.offset, entry.size));
        CHECK(readMetadata(document.view()).value() == metadata);
    }

    // dimDecor: an unusual non-zero value survives unrelated edits and an
    // unchanged axis; only a changed axis is rewritten, to 100 or 0.
    {
        WorldDocument document(input);
        auto metadata = readMetadata(document.view()).value();
        metadata.music = 2;
        metadata.scrollsVertically = true; // y: 0 -> 100
        EditBuilder builder(document);
        CHECK(applyMetadata(builder, metadata));
        CHECK(document.apply(builder.build("Metadata")).has_value());
        const auto header = document.view().header().value();
        CHECK((header.dimDecor() == Point{37, 100}));
        CHECK(readMetadata(document.view())->scrollsHorizontally);

        auto off = readMetadata(document.view()).value();
        off.scrollsHorizontally = false; // x: 37 -> 0
        EditBuilder second(document);
        CHECK(applyMetadata(second, off));
        CHECK(document.apply(second.build("Metadata")).has_value());
        CHECK((document.view().header()->dimDecor() == Point{0, 100}));
        auto on = readMetadata(document.view()).value();
        on.scrollsHorizontally = true; // x: 0 -> 100 (canonical)
        EditBuilder third(document);
        CHECK(applyMetadata(third, on));
        CHECK(document.apply(third.build("Metadata")).has_value());
        CHECK((document.view().header()->dimDecor() == Point{100, 100}));
    }

    // Name rules: padding after the new terminator stays; limits enforced.
    {
        auto padded = blankWorld(fxRevision13);
        putText(padded, 376, "Long old title");
        putText(padded, 391, "tail");
        WorldDocument document(padded);
        auto metadata = readMetadata(document.view()).value();
        metadata.name = ascii("Ab");
        EditBuilder builder(document);
        CHECK(applyMetadata(builder, metadata));
        CHECK(document.apply(builder.build("Rename")).has_value());
        CHECK(within(diffPositions(document.currentBytes(), padded), 376, 3));
        CHECK(document.view().header()->nameBytes()[5] == std::byte{'o'}); // old byte kept

        auto tooLong = metadata;
        tooLong.name = Bytes(100, std::byte{'x'});
        EditBuilder rejected(document);
        CHECK(!applyMetadata(rejected, tooLong));
        CHECK(!rejected.hasChanges());
        auto withNul = metadata;
        withNul.name = Bytes{std::byte{'a'}, std::byte{0}};
        CHECK(!applyMetadata(rejected, withNul));
        auto longest = metadata;
        longest.name = Bytes(99, std::byte{'x'});
        CHECK(applyMetadata(rejected, longest));
        CHECK_EQ(readMetadata(rejected.view())->name.size(), std::size_t{99});
    }

    // Start helpers.
    {
        WorldDocument document(input);
        {
            EditBuilder builder(document);
            CHECK(placeStart(builder, 1, {10, 20}));
            CHECK(!placeStart(builder, 4, {10, 20}));
            CHECK(!placeStart(builder, 0, {100, 0}));
            CHECK(document.apply(builder.build("Place start")).has_value());
            CHECK(within(diffPositions(document.currentBytes(), input), 336, 8));
            CHECK((document.view().header()->blupiPos(1).value() == Point{642, 1286}));
        }
        {
            EditBuilder builder(document);
            CHECK(toggleStartDirection(builder, 0)); // 1 -> 2
            CHECK(toggleStartDirection(builder, 1)); // 2 -> 1
            CHECK(toggleStartDirection(builder, 2)); // 7 -> 1 (anything not left becomes left)
            CHECK(!toggleStartDirection(builder, 4));
            const auto header = builder.view().header().value();
            CHECK_EQ(header.blupiDir(0).value(), 2);
            CHECK_EQ(header.blupiDir(1).value(), 1);
            CHECK_EQ(header.blupiDir(2).value(), 1);
            CHECK_EQ(header.blupiDir(3).value(), 2);
        }
        {
            EditBuilder builder(document);
            CHECK(copyStartToAll(builder, 3));
            CHECK(!copyStartToAll(builder, 4));
            CHECK(document.apply(builder.build("Copy start")).has_value());
            const auto metadata = readMetadata(document.view()).value();
            for (const auto &start : metadata.starts) {
                CHECK((start == StartInfo{{402, 806}, 2}));
            }
            // Only blupiPos and blupiDir changed since the input.
            CHECK(within(diffPositions(document.currentBytes(), input), 328, 48));
        }
    }

    // Reserved fields, layers, objects and suffix survive a full metadata edit.
    {
        WorldDocument document(input);
        Metadata metadata;
        metadata.name = ascii("Everything");
        metadata.music = 10;
        metadata.region = 23;
        metadata.scrollsHorizontally = false;
        metadata.scrollsVertically = true;
        metadata.posDecor = {1, 2};
        for (std::size_t index = 0; index < 4; ++index) {
            metadata.starts[index] = {startPositionForCell({index, index + 1}), 2};
        }
        EditBuilder builder(document);
        CHECK(applyMetadata(builder, metadata));
        CHECK(document.apply(builder.build("Edit Metadata")).has_value());
        CHECK_EQ(document.historySize(), std::size_t{1});
        for (const auto position : diffPositions(document.currentBytes(), input)) {
            const bool allowed = (position >= 204 && position < 220) ||
                                 (position >= 222 && position < 226) ||
                                 (position >= 328 && position < 376) ||
                                 (position >= 376 && position < 387);
            CHECK(allowed);
        }
        CHECK(readMetadata(document.view()).value() == metadata);
    }
    // Read-only revisions compute a change but cannot apply it.
    {
        WorldDocument old(patternedWorld(fxRevision11));
        auto metadata = readMetadata(old.view()).value();
        metadata.music = 1;
        EditBuilder builder(old);
        CHECK(applyMetadata(builder, metadata));
        CHECK(old.apply(builder.build("Edit Metadata")).error().error == EditError::readOnly);
    }

    // Profile facts.
    CHECK(profileInfo(GameProfile::unknown) == nullptr);
    const auto *sb1 = profileInfo(GameProfile::speedyBlupi1);
    const auto *sb2 = profileInfo(GameProfile::speedyBlupi2);
    CHECK(sb1 != nullptr && sb2 != nullptr);
    CHECK(sb1->music.contains(0) && sb1->music.contains(10) && !sb1->music.contains(11));
    CHECK(sb1->region.contains(23) && !sb1->region.contains(24) && !sb1->region.contains(-1));
    CHECK(sb2->region.contains(31) && !sb2->region.contains(32));
    CHECK(sb1->foregroundTiles.contains(236) && !sb1->foregroundTiles.contains(237));
    CHECK(sb2->foregroundTiles.contains(440) && !sb2->foregroundTiles.contains(441));
    CHECK(!sb1->loaderRemaps48To67 && sb2->loaderRemaps48To67);
    CHECK(!sb1->music.provenance.empty() && !sb2->region.provenance.empty());
    return test::finish("metadata");
}
