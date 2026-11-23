// SPDX-License-Identifier: GPL-3.0-only
#include "world/new_world.hpp"

#include <cstdint>

namespace eggbert {

namespace {

class Writer {
public:
    explicit Writer(std::vector<std::byte> &bytes) : bytes_(bytes) {}

    void int16(std::size_t offset, std::int32_t value) { put(offset, value, 2); }
    void int32(std::size_t offset, std::int32_t value) { put(offset, value, 4); }
    void point(std::size_t offset, Point value) {
        int32(offset, value.x);
        int32(offset + 4, value.y);
    }

private:
    void put(std::size_t offset, std::int32_t value, std::size_t size) {
        const auto raw = static_cast<std::uint32_t>(value);
        for (std::size_t index = 0; index < size; ++index) {
            bytes_[offset + index] = static_cast<std::byte>((raw >> (8 * index)) & 0xFF);
        }
    }

    std::vector<std::byte> &bytes_;
};

struct InitialObject {
    std::int16_t type;
    Point position;
    std::int16_t icon;
};

// TYPE_TRESOR and TYPE_GOAL as the games' InitDecor places them.
constexpr InitialObject initialObjects[] = {{5, {258, 196}, 0}, {7, {322, 196}, 29}};
constexpr std::int16_t elementChannel = 10; // CHELEMENT
constexpr std::int16_t stepStopStart = 1;   // STEP_STOPSTART
constexpr std::int32_t directionRight = 2;  // DIR_RIGHT
constexpr Point initialStart{194, 198};

struct InitialTile {
    std::size_t x;
    std::size_t y;
    std::int16_t value;
};

constexpr InitialTile initialTiles[] = {{3, 4, 40}, {4, 4, 38}, {5, 4, 39}};

} // namespace

FormatRevision newWorldRevision(GameProfile profile) {
    return profile == GameProfile::speedyBlupi1 ? FormatRevision::v1_2 : FormatRevision::v1_3;
}

std::vector<std::byte> newWorld(GameProfile profile) {
    const auto &revision = *findRevision(newWorldRevision(profile));
    std::vector<std::byte> bytes(revision.expectedBytes, std::byte{0});
    Writer write(bytes);

    write.int16(headerField(HeaderField::majRev).offset, revision.major);
    write.int16(headerField(HeaderField::minRev).offset, revision.minor);
    write.point(headerField(HeaderField::dimDecor).offset, {100, 100});
    write.int16(headerField(HeaderField::music).offset, 1);
    write.int16(headerField(HeaderField::region).offset, 2);
    for (std::size_t player = 0; player < 4; ++player) {
        write.point(headerField(HeaderField::blupiPos).elementRange(player).offset, initialStart);
        write.int32(headerField(HeaderField::blupiDir).elementRange(player).offset,
                    directionRight);
    }

    const auto foreground = revision.section(SectionKind::foreground)->offset;
    const auto objects = revision.section(SectionKind::objects)->offset;
    for (std::size_t offset = foreground; offset < objects; ++offset) {
        bytes[offset] = std::byte{0xFF}; // every cell of both layers empty (-1)
    }
    for (const auto &tile : initialTiles) {
        // Layers are stored [x][y].
        write.int16(foreground + (tile.x * layout::mapHeight + tile.y) * layout::tileBytes,
                    tile.value);
    }

    std::size_t slot = 0;
    for (const auto &object : initialObjects) {
        const auto record = objects + slot++ * layout::objectRecordBytes;
        const auto at = [&](ObjectField field) { return record + objectField(field).offset; };
        write.int16(at(ObjectField::type), object.type);
        write.int16(at(ObjectField::stepAdvance), 1);
        write.int16(at(ObjectField::stepRecede), 1);
        write.point(at(ObjectField::posStart), object.position);
        write.point(at(ObjectField::posEnd), object.position);
        write.point(at(ObjectField::posCurrent), object.position);
        write.int16(at(ObjectField::step), stepStopStart);
        write.int16(at(ObjectField::channel), elementChannel);
        write.int16(at(ObjectField::icon), object.icon);
    }
    return bytes;
}

} // namespace eggbert
