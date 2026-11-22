// SPDX-License-Identifier: GPL-3.0-only
#include "world/object_catalog.hpp"
#include "world/object_ops.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <algorithm>
#include <set>
#include <string>

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

// Fills `count` slots from slot 0 with a simple active record (type 5).
void occupy(Bytes &bytes, const FixtureLayout &layout, std::size_t count) {
    for (std::size_t slot = 0; slot < count; ++slot) {
        put16(bytes, fxSlotOffset(layout, slot), 5);
        put16(bytes, fxSlotOffset(layout, slot) + 42, 10);
    }
}

std::int16_t get16(std::span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(
        std::to_integer<unsigned>(bytes[offset]) | (std::to_integer<unsigned>(bytes[offset + 1]) << 8)));
}

std::int32_t get32(std::span<const std::byte> bytes, std::size_t offset) {
    std::uint32_t value = 0;
    for (unsigned index = 0; index < 4; ++index) {
        value |= std::to_integer<std::uint32_t>(bytes[offset + index]) << (8 * index);
    }
    return static_cast<std::int32_t>(value);
}

void checkCatalog() {
    const auto sb1 = objectPresets(GameProfile::speedyBlupi1);
    const auto sb2 = objectPresets(GameProfile::speedyBlupi2);
    CHECK_EQ(sb1.size(), std::size_t{31});
    CHECK_EQ(sb2.size(), std::size_t{42});
    CHECK(objectPresets(GameProfile::unknown).empty());

    for (const auto profile : {GameProfile::speedyBlupi1, GameProfile::speedyBlupi2}) {
        std::set<std::string> ids;
        for (const auto &preset : objectPresets(profile)) {
            CHECK(ids.insert(std::string(preset.id)).second); // unique
            CHECK(!preset.name.empty() && !preset.sourceName.empty());
            CHECK(!preset.provenance.empty());
            CHECK(preset.type != 0);
            CHECK(preset.offsetX == 0 || preset.offsetX == 2);
            CHECK(preset.offsetY >= 0 && preset.offsetY <= 8);
            CHECK(preset.channel == 1 || preset.channel == 2 ||
                  (preset.channel >= 10 && preset.channel <= 13));
            CHECK(preset.stepAdvance >= 1 && preset.stepRecede >= 1);
            if (preset.hasPath) {
                CHECK(preset.offsetX == 0 && preset.offsetY == 0);
            }
            CHECK(findObjectPreset(profile, preset.id) == &preset);
            CHECK(objectTypeInfo(profile, preset.type).status == ObjectTypeStatus::placeable);
        }
    }

    // Spot values typed from the recorded evidence.
    const auto expect = [](GameProfile profile, const char *id, int type, int advance, int recede,
                           int stopStart, int stopEnd, int offsetX, int offsetY, int channel,
                           int icon, bool hasPath) {
        const auto *preset = findObjectPreset(profile, id);
        CHECK(preset != nullptr);
        if (preset == nullptr) {
            return;
        }
        CHECK_EQ(preset->type, type);
        CHECK_EQ(preset->stepAdvance, advance);
        CHECK_EQ(preset->stepRecede, recede);
        CHECK_EQ(preset->timeStopStart, stopStart);
        CHECK_EQ(preset->timeStopEnd, stopEnd);
        CHECK_EQ(preset->offsetX, offsetX);
        CHECK_EQ(preset->offsetY, offsetY);
        CHECK_EQ(preset->channel, channel);
        CHECK_EQ(preset->icon, icon);
        CHECK_EQ(preset->hasPath, hasPath);
    };
    const auto one = GameProfile::speedyBlupi1;
    const auto two = GameProfile::speedyBlupi2;
    expect(one, "treasure", 5, 1, 1, 0, 0, 2, 4, 10, 0, false);
    expect(one, "egg", 6, 1, 1, 0, 0, 2, 8, 10, 0x15, false);
    expect(one, "goal", 7, 1, 1, 0, 0, 2, 4, 10, 0x1d, false);
    expect(one, "lift-fast-out", 1, 5, 22, 2, 2, 0, 0, 1, 153, true);
    expect(one, "bulldozer", 4, 30, 30, 20, 20, 0, 0, 10, 0x39, true);
    expect(one, "fish", 17, 15, 15, 47, 47, 0, 0, 10, 0x4f, true);
    expect(one, "enemy-tank", 33, 10, 10, 23, 23, 0, 0, 12, 0xf0, true);
    expect(one, "bomb-ceiling", 3, 1, 1, 0, 0, 2, 0, 10, 0x30, false);
    expect(one, "personal-bomb-3", 202, 1, 1, 0, 0, 2, 6, 12, 0x101, false);
    expect(one, "skateboard", 24, 1, 1, 0, 0, 2, 5, 10, 0x81, false);
    expect(one, "charger", 31, 1, 1, 0, 0, 0, 0, 1, 0xee, false);
    expect(two, "lift-slow", 1, 20, 20, 20, 20, 0, 0, 1, 29, true);
    expect(two, "conveyor-lift-b", 48, 10, 10, 10, 10, 0, 0, 1, 0x137, true);
    expect(two, "wasp", 44, 6, 6, 4, 4, 0, 0, 10, 0xc3, true);
    expect(two, "creature", 54, 20, 20, 200, 200, 0, 0, 10, 0xf4, true);
    expect(two, "homing-bomb", 96, 1, 1, 0, 0, 0, 0, 10, 0x100, false);
    expect(two, "dynamite", 55, 1, 1, 0, 0, 2, 6, 10, 0xfc, false);
    expect(two, "key-colour-2", 50, 1, 1, 0, 0, 2, 4, 10, 0x7d, false);
    expect(two, "hovercraft", 46, 1, 1, 0, 0, 2, 4, 10, 0xd0, false);
    expect(two, "inverter", 40, 1, 1, 0, 0, 2, 8, 10, 0xbb, false);

    // No support is inferred across games from a matching number.
    CHECK(findObjectPreset(one, "wasp") == nullptr);
    CHECK(findObjectPreset(one, "dynamite") == nullptr);
    CHECK(presetForType(one, 44) == nullptr);
    CHECK(presetForType(two, 44) != nullptr);
    CHECK(objectTypeInfo(one, 44).status == ObjectTypeStatus::unknown);
    CHECK(objectTypeInfo(one, 96).status == ObjectTypeStatus::unknown);
    CHECK(objectTypeInfo(one, 90).status == ObjectTypeStatus::unknown);
    CHECK(objectTypeInfo(two, 90).status == ObjectTypeStatus::runtimeOnly);
    CHECK(objectTypeInfo(one, 8).status == ObjectTypeStatus::runtimeOnly);
    CHECK(objectTypeInfo(two, 23).status == ObjectTypeStatus::runtimeOnly);
    CHECK(objectTypeInfo(two, 45).status == ObjectTypeStatus::unknown);
    CHECK(objectTypeInfo(two, 0).status == ObjectTypeStatus::unknown);
    CHECK(objectTypeInfo(two, -5).status == ObjectTypeStatus::unknown);
    CHECK(objectTypeInfo(GameProfile::unknown, 5).status == ObjectTypeStatus::unknown);
    CHECK(objectTypeInfo(one, 1).name == "Lift");
    CHECK(objectTypeInfo(one, 12).name == "Crate");
    CHECK(objectTypeInfo(one, 201).name == "Personal bomb, player 2");
}

void checkRecords() {
    const auto *treasure = findObjectPreset(GameProfile::speedyBlupi1, "treasure");
    const auto *lift = findObjectPreset(GameProfile::speedyBlupi1, "lift-fast-out");
    const auto *crate = findObjectPreset(GameProfile::speedyBlupi2, "crate");

    // A static item on an asymmetric cell.
    const auto item = recordForRequest({treasure, {3, 70}, {}, {}}).value();
    const ObjectRecord wantItem{5, 1, 1, 0, 0, 0, {194, 4484}, {194, 4484}, {194, 4484},
                                1, 0, 0, 10, 0, 0};
    CHECK(item == wantItem);

    // A mover without an end keeps its per-cell durations and does not move.
    const auto still = recordForRequest({lift, {10, 20}, {}, {}}).value();
    CHECK((still.posStart == Point{640, 1280}));
    CHECK((still.posEnd == Point{640, 1280}));
    CHECK_EQ(still.stepAdvance, std::int16_t{5});
    CHECK_EQ(still.stepRecede, std::int16_t{22});
    CHECK_EQ(still.icon, std::int16_t{153});
    // An end on the same cell is the same thing.
    CHECK(recordForRequest({lift, {10, 20}, CellPos{10, 20}, {}}).value() == still);

    // Horizontal, vertical and diagonal paths: durations times the longer axis.
    const auto right = recordForRequest({lift, {10, 20}, CellPos{15, 20}, {}}).value();
    CHECK((right.posEnd == Point{960, 1280}));
    CHECK((right.posCurrent == Point{640, 1280}));
    CHECK_EQ(right.stepAdvance, std::int16_t{25});
    CHECK_EQ(right.stepRecede, std::int16_t{110});
    CHECK_EQ(right.timeStopStart, std::int16_t{2});
    const auto up = recordForRequest({lift, {10, 20}, CellPos{10, 13}, {}}).value();
    CHECK((up.posEnd == Point{640, 832}));
    CHECK_EQ(up.stepAdvance, std::int16_t{35});
    const auto diagonal = recordForRequest({lift, {10, 20}, CellPos{12, 29}, {}}).value();
    CHECK((diagonal.posEnd == Point{768, 1856}));
    CHECK_EQ(diagonal.stepAdvance, std::int16_t{45});
    CHECK_EQ(diagonal.stepRecede, std::int16_t{198});
    // Longest possible path stays inside 16 bits.
    const auto *bulldozer = findObjectPreset(GameProfile::speedyBlupi1, "bulldozer");
    const auto longest = recordForRequest({bulldozer, {0, 0}, CellPos{99, 0}, {}}).value();
    CHECK_EQ(longest.stepAdvance, std::int16_t{2970});

    // Icon choice only where the icon is a stored choice.
    CHECK_EQ(recordForRequest({crate, {1, 1}, {}, std::int16_t{33}}).value().icon,
             std::int16_t{33});
    CHECK(recordForRequest({treasure, {1, 1}, {}, std::int16_t{33}}).error() ==
          ObjectOpError::notApplicable);
    CHECK(recordForRequest({treasure, {1, 1}, CellPos{2, 1}, {}}).error() ==
          ObjectOpError::notApplicable);
    CHECK(recordForRequest({treasure, {100, 1}, {}, {}}).error() == ObjectOpError::outsideMap);
    CHECK(recordForRequest({lift, {1, 1}, CellPos{1, 100}, {}}).error() ==
          ObjectOpError::outsideMap);
    CHECK(recordForRequest({nullptr, {1, 1}, {}, {}}).error() == ObjectOpError::notApplicable);
}

void checkCreateAndCapacity() {
    const auto *treasure = findObjectPreset(GameProfile::speedyBlupi2, "treasure");
    const auto *lift = findObjectPreset(GameProfile::speedyBlupi2, "lift-slow");

    // Lowest free slot is used and its stale bytes are replaced completely;
    // every other slot, including later free slots with opaque bytes, stays.
    {
        auto input = patternedWorld(fxRevision13, 5);
        for (std::size_t slot = 0; slot < 200; ++slot) {
            put16(input, fxSlotOffset(fxRevision13, slot), slot == 4 || slot == 9 ? 0 : 5);
        }
        WorldDocument document(input);
        CHECK_EQ(poolCapacity(document.view()).stored, std::size_t{200});
        CHECK_EQ(poolCapacity(document.view()).active, std::size_t{198});
        CHECK_EQ(poolCapacity(document.view()).free, std::size_t{2});
        CHECK((freeSlots(document.view(), 5) == std::vector<std::size_t>{4, 9}));
        CHECK((freeSlots(document.view(), 1) == std::vector<std::size_t>{4}));

        EditBuilder builder(document);
        const auto slot = createObject(builder, {lift, {7, 8}, CellPos{7, 3}, {}});
        CHECK_EQ(slot.value(), std::size_t{4});
        CHECK(document.apply(builder.build("Create Lift")).has_value());
        const auto base = fxSlotOffset(fxRevision13, 4); // 40868 + 192
        CHECK_EQ(base, std::size_t{41'060});
        CHECK(within(diffPositions(document.currentBytes(), input), base, 48));
        const auto bytes = document.currentBytes();
        // Literal field offsets inside the record.
        CHECK_EQ(get16(bytes, base + 0), std::int16_t{1});
        CHECK_EQ(get16(bytes, base + 2), std::int16_t{100}); // 20 * 5 cells
        CHECK_EQ(get16(bytes, base + 4), std::int16_t{100});
        CHECK_EQ(get16(bytes, base + 6), std::int16_t{20});
        CHECK_EQ(get16(bytes, base + 8), std::int16_t{20});
        CHECK_EQ(get16(bytes, base + 10), std::int16_t{0}); // unknown1 zeroed
        CHECK_EQ(get32(bytes, base + 12), 448);
        CHECK_EQ(get32(bytes, base + 16), 512);
        CHECK_EQ(get32(bytes, base + 20), 448);
        CHECK_EQ(get32(bytes, base + 24), 192);
        CHECK_EQ(get32(bytes, base + 28), 448);
        CHECK_EQ(get32(bytes, base + 32), 512);
        CHECK_EQ(get16(bytes, base + 36), std::int16_t{1}); // step
        CHECK_EQ(get16(bytes, base + 38), std::int16_t{0});
        CHECK_EQ(get16(bytes, base + 40), std::int16_t{0});
        CHECK_EQ(get16(bytes, base + 42), std::int16_t{1});
        CHECK_EQ(get16(bytes, base + 44), std::int16_t{29});
        CHECK_EQ(get16(bytes, base + 46), std::int16_t{0}); // unknown2 zeroed

        // The second free slot takes the next object; then the pool is full.
        EditBuilder second(document);
        CHECK_EQ(createObject(second, {treasure, {1, 1}, {}, {}}).value(), std::size_t{9});
        CHECK(document.apply(second.build("Create Treasure")).has_value());
        CHECK_EQ(poolCapacity(document.view()).free, std::size_t{0});
        const auto full = toVector(document.currentBytes());
        EditBuilder third(document);
        CHECK(createObject(third, {treasure, {2, 2}, {}, {}}).error() == ObjectOpError::poolFull);
        CHECK(!third.hasChanges());
        CHECK(std::ranges::equal(document.currentBytes(), full));
        // Undo and redo across the full pool restore exact bytes.
        CHECK(document.undo());
        CHECK(document.undo());
        CHECK(std::ranges::equal(document.currentBytes(), input));
        CHECK(document.redo());
        CHECK(document.redo());
        CHECK(std::ranges::equal(document.currentBytes(), full));
    }

    // 199 / 200 / 201 requested objects in a 200-slot pool.
    for (const std::size_t requested : {std::size_t{199}, std::size_t{200}, std::size_t{201}}) {
        const auto input = blankWorld(fxRevision13);
        WorldDocument document(input);
        std::vector<ObjectRequest> requests(requested, ObjectRequest{treasure, {5, 5}, {}, {}});
        EditBuilder builder(document);
        const auto result = createObjects(builder, requests);
        if (requested <= 200) {
            CHECK(result.has_value());
            CHECK_EQ(result->size(), requested);
            CHECK_EQ(result->back(), requested - 1);
            CHECK(document.apply(builder.build("Create many")).has_value());
            CHECK_EQ(poolCapacity(document.view()).active, requested);
            CHECK_EQ(document.historySize(), std::size_t{1});
        } else {
            CHECK(result.error() == ObjectOpError::poolFull);
            CHECK(!builder.hasChanges()); // nothing partially created
        }
    }
    // 99 / 100 / 101 in a 100-slot revision (computed only; such revisions
    // are read-only, so applying is refused).
    for (const std::size_t requested : {std::size_t{99}, std::size_t{100}, std::size_t{101}}) {
        WorldDocument document(blankWorld(fxRevision11));
        CHECK_EQ(poolCapacity(document.view()).stored, std::size_t{100});
        std::vector<ObjectRequest> requests(requested, ObjectRequest{treasure, {5, 5}, {}, {}});
        EditBuilder builder(document);
        const auto result = createObjects(builder, requests);
        CHECK_EQ(result.has_value(), requested <= 100);
        if (!result) {
            CHECK(result.error() == ObjectOpError::poolFull);
            CHECK(!builder.hasChanges());
        } else {
            CHECK(document.apply(builder.build("Create many")).error().error ==
                  EditError::readOnly);
        }
    }
    // A batch needing two slots with one free creates nothing.
    {
        auto input = blankWorld(fxRevision13);
        occupy(input, fxRevision13, 199);
        WorldDocument document(input);
        const ObjectRequest two[] = {{treasure, {1, 1}, {}, {}}, {treasure, {2, 2}, {}, {}}};
        EditBuilder builder(document);
        CHECK(createObjects(builder, two).error() == ObjectOpError::poolFull);
        CHECK(!builder.hasChanges());
        const ObjectRequest invalid[] = {{treasure, {1, 1}, {}, {}}, {treasure, {200, 2}, {}, {}}};
        CHECK(createObjects(builder, invalid).error() == ObjectOpError::outsideMap);
        CHECK(!builder.hasChanges());
        CHECK_EQ(createObject(builder, {treasure, {1, 1}, {}, {}}).value(), std::size_t{199});
    }
    // No pool at all (truncated before the objects).
    {
        auto cut = blankWorld(fxRevision13);
        cut.resize(41'000);
        const WorldDocument document(cut);
        CHECK_EQ(poolCapacity(document.view()).stored, std::size_t{0});
        CHECK(freeSlots(document.view(), 3).empty());
        EditBuilder builder(document);
        CHECK(createObject(builder, {treasure, {1, 1}, {}, {}}).error() ==
              ObjectOpError::noObjectPool);
        CHECK(deleteObject(builder, 0).error() == ObjectOpError::noObjectPool);
    }
}

void checkEditOperations() {
    auto input = patternedWorld(fxRevision12, 3);
    // Slot 20: static object; slot 21: object with a path; slot 22: free.
    const auto write = [&](std::size_t slot, int type, Point start, Point end, Point current,
                           int channel) {
        const auto base = fxSlotOffset(fxRevision12, slot);
        put16(input, base, type);
        put32(input, base + 12, start.x);
        put32(input, base + 16, start.y);
        put32(input, base + 20, end.x);
        put32(input, base + 24, end.y);
        put32(input, base + 28, current.x);
        put32(input, base + 32, current.y);
        put16(input, base + 42, channel);
    };
    write(20, 5, {642, 1284}, {642, 1284}, {642, 1284}, 10);
    write(21, 1, {1280, 640}, {1280, 320}, {1280, 500}, 1);
    write(22, 0, {7, 7}, {8, 8}, {9, 9}, 10);
    for (std::size_t slot = 23; slot < 200; ++slot) {
        put16(input, fxSlotOffset(fxRevision12, slot), 5); // everything else active
    }
    const auto base20 = fxSlotOffset(fxRevision12, 20);
    const auto base21 = fxSlotOffset(fxRevision12, 21);
    const auto base22 = fxSlotOffset(fxRevision12, 22);

    // Delete: only the two type bytes change; the rest stays as stale data.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        CHECK(deleteObject(builder, 21).has_value());
        CHECK(deleteObject(builder, 22).error() == ObjectOpError::slotIsFree);
        CHECK(deleteObject(builder, 200).error() == ObjectOpError::invalidSlot);
        CHECK(document.apply(builder.build("Delete")).has_value());
        CHECK(within(diffPositions(document.currentBytes(), input), base21, 2));
        CHECK(document.view().objects()->record(21)->isFree());
        CHECK((document.view().objects()->record(21)->posEnd == Point{1280, 320}));
    }
    // Relocate a static object: exactly the three positions (24 bytes).
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        CHECK(relocateStatic(builder, 20, {64, -128}).has_value());
        CHECK(relocateStatic(builder, 21, {64, 0}).error() == ObjectOpError::notApplicable);
        CHECK(relocateStatic(builder, 22, {64, 0}).error() == ObjectOpError::slotIsFree);
        CHECK(document.apply(builder.build("Relocate")).has_value());
        CHECK(within(diffPositions(document.currentBytes(), input), base20 + 12, 24));
        const auto record = document.view().objects()->record(20).value();
        CHECK((record.posStart == Point{706, 1156}));
        CHECK((record.posEnd == Point{706, 1156}));
        CHECK((record.posCurrent == Point{706, 1156}));
    }
    // Move one endpoint only.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        CHECK(moveStart(builder, 21, {1344, 640}).has_value());
        CHECK(document.apply(builder.build("Move start")).has_value());
        CHECK(within(diffPositions(document.currentBytes(), input), base21 + 12, 8));
        EditBuilder end(document);
        CHECK(moveEnd(end, 21, {1280, 256}).has_value());
        CHECK(moveEnd(end, 22, {0, 0}).error() == ObjectOpError::slotIsFree);
        CHECK(document.apply(end.build("Move end")).has_value());
        const auto record = document.view().objects()->record(21).value();
        CHECK((record.posStart == Point{1344, 640}));
        CHECK((record.posEnd == Point{1280, 256}));
        CHECK((record.posCurrent == Point{1280, 500})); // runtime field untouched
        const auto positions = diffPositions(document.currentBytes(), input);
        CHECK(within(positions, base21 + 12, 16));
    }
    // Translate a path: one delta on start, end and current.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        CHECK(translatePath(builder, 21, {-128, 64}).has_value());
        CHECK(moveObject(builder, 20, {64, 0}).has_value());
        CHECK(document.apply(builder.build("Translate")).has_value());
        const auto record = document.view().objects()->record(21).value();
        CHECK((record.posStart == Point{1152, 704}));
        CHECK((record.posEnd == Point{1152, 384}));
        CHECK((record.posCurrent == Point{1152, 564}));
        for (const auto position : diffPositions(document.currentBytes(), input)) {
            const bool inPositions = (position >= base20 + 12 && position < base20 + 36) ||
                                     (position >= base21 + 12 && position < base21 + 36);
            CHECK(inPositions);
        }
    }
    // Arithmetic overflow is refused rather than wrapped.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        CHECK(translatePath(builder, 20, {2'147'483'647, 0}).error() ==
              ObjectOpError::outsideMap);
        CHECK(!builder.hasChanges());
    }
    // Duplicate: raw copy into the lowest free slot with shifted positions;
    // opaque fields (unknown1, unknown2, runtime fields) come along.
    {
        WorldDocument document(input);
        EditBuilder builder(document);
        const auto slot = duplicateObject(builder, 21, {64, 64});
        CHECK_EQ(slot.value(), std::size_t{22});
        CHECK(document.apply(builder.build("Duplicate")).has_value());
        CHECK(within(diffPositions(document.currentBytes(), input), base22, 48));
        const auto source = document.view().objects()->record(21).value();
        const auto copy = document.view().objects()->record(22).value();
        CHECK_EQ(copy.type, source.type);
        CHECK_EQ(copy.unknown1, source.unknown1);
        CHECK_EQ(copy.unknown2, source.unknown2);
        CHECK_EQ(copy.stepAdvance, source.stepAdvance);
        CHECK_EQ(copy.phase, source.phase);
        CHECK((copy.posStart == Point{1344, 704}));
        CHECK((copy.posEnd == Point{1344, 384}));
        CHECK((copy.posCurrent == Point{1344, 564}));
        // Now the pool is full: a second duplicate changes nothing.
        EditBuilder again(document);
        CHECK(duplicateObject(again, 21, {0, 0}).error() == ObjectOpError::poolFull);
        CHECK(duplicateObject(again, 199 + 1, {0, 0}).error() == ObjectOpError::invalidSlot);
        CHECK(!again.hasChanges());
    }

    // Hit testing by sprite box, ascending by slot.
    {
        auto bytes = blankWorld(fxRevision13);
        const auto place = [&](std::size_t slot, Point current, int channel) {
            const auto base = fxSlotOffset(fxRevision13, slot);
            put16(bytes, base, 5);
            put32(bytes, base + 28, current.x);
            put32(bytes, base + 32, current.y);
            put16(bytes, base + 42, channel);
        };
        place(3, {100, 100}, 1);   // 64 box
        place(7, {130, 130}, 10);  // 60 box, overlaps slot 3
        place(9, {1000, 1000}, 9); // 128 box
        const WorldView view(bytes);
        CHECK((objectsAt(view, {140, 140}) == std::vector<std::size_t>{3, 7}));
        CHECK((objectsAt(view, {100, 100}) == std::vector<std::size_t>{3}));
        CHECK((objectsAt(view, {163, 163}) == std::vector<std::size_t>{3, 7}));
        CHECK((objectsAt(view, {164, 164}) == std::vector<std::size_t>{7}));
        CHECK((objectsAt(view, {189, 189}) == std::vector<std::size_t>{7}));
        CHECK(objectsAt(view, {190, 190}).empty());
        CHECK((objectsAt(view, {1127, 1127}) == std::vector<std::size_t>{9}));
        CHECK(objectsAt(view, {1128, 1000}).empty());
        CHECK(objectsAt(view, {99, 100}).empty());
        CHECK_EQ(spriteBoxSize(1), 64);
        CHECK_EQ(spriteBoxSize(9), 128);
        CHECK_EQ(spriteBoxSize(12), 60);
    }
}

} // namespace

int main() {
    checkCatalog();
    checkRecords();
    checkCreateAndCapacity();
    checkEditOperations();
    return test::finish("object_ops");
}
