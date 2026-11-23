// SPDX-License-Identifier: GPL-3.0-only
#include "world/text_world_image.hpp"

#include "world/new_world.hpp"

#include <algorithm>
#include <limits>

#include "world/world_probe.hpp"
#include "world/world_view.hpp"

namespace eggbert {

namespace {

const RevisionDescriptor &imageLayout() {
    return *findRevision(FormatRevision::v1_3);
}

void put16(std::vector<std::byte> &bytes, std::size_t offset, long long value) {
    const auto raw = static_cast<std::uint16_t>(static_cast<std::int16_t>(value));
    bytes[offset] = static_cast<std::byte>(raw & 0xffu);
    bytes[offset + 1] = static_cast<std::byte>(raw >> 8);
}

void put32(std::vector<std::byte> &bytes, std::size_t offset, long long value) {
    const auto raw = static_cast<std::uint32_t>(static_cast<std::int32_t>(value));
    for (std::size_t index = 0; index < 4; ++index) {
        bytes[offset + index] = static_cast<std::byte>((raw >> (8 * index)) & 0xffu);
    }
}

bool fits16(long long value) {
    return value >= std::numeric_limits<std::int16_t>::min() &&
           value <= std::numeric_limits<std::int16_t>::max();
}

std::size_t sectionOffset(SectionKind kind) {
    for (const auto &section : imageLayout().sections()) {
        if (section.kind == kind) {
            return section.offset;
        }
    }
    return 0;
}

std::size_t cellOffset(SectionKind layer, std::size_t x, std::size_t y) {
    return sectionOffset(layer) + (x * layout::mapHeight + y) * 2;
}

struct ScalarField {
    ObjectField field;
    const char *name;
    long long TextWorld::ObjectLine::*member;
    std::int16_t ObjectRecord::*stored;
};
struct PointField {
    ObjectField field;
    const char *name;
    Point TextWorld::ObjectLine::*member;
    Point ObjectRecord::*stored;
};

constexpr ScalarField scalarFields[] = {
    {ObjectField::type, "type", &TextWorld::ObjectLine::type, &ObjectRecord::type},
    {ObjectField::stepAdvance, "stepAdvance", &TextWorld::ObjectLine::stepAdvance,
     &ObjectRecord::stepAdvance},
    {ObjectField::stepRecede, "stepRecede", &TextWorld::ObjectLine::stepRecede,
     &ObjectRecord::stepRecede},
    {ObjectField::timeStopStart, "timeStopStart", &TextWorld::ObjectLine::timeStopStart,
     &ObjectRecord::timeStopStart},
    {ObjectField::timeStopEnd, "timeStopEnd", &TextWorld::ObjectLine::timeStopEnd,
     &ObjectRecord::timeStopEnd},
    {ObjectField::step, "step", &TextWorld::ObjectLine::step, &ObjectRecord::step},
    {ObjectField::time, "time", &TextWorld::ObjectLine::time, &ObjectRecord::time},
    {ObjectField::phase, "phase", &TextWorld::ObjectLine::phase, &ObjectRecord::phase},
    {ObjectField::channel, "channel", &TextWorld::ObjectLine::channel, &ObjectRecord::channel},
    {ObjectField::icon, "icon", &TextWorld::ObjectLine::icon, &ObjectRecord::icon},
};
constexpr PointField pointFields[] = {
    {ObjectField::posStart, "posStart", &TextWorld::ObjectLine::posStart, &ObjectRecord::posStart},
    {ObjectField::posEnd, "posEnd", &TextWorld::ObjectLine::posEnd, &ObjectRecord::posEnd},
    {ObjectField::posCurrent, "posCurrent", &TextWorld::ObjectLine::posCurrent,
     &ObjectRecord::posCurrent},
};

std::string lineName(std::size_t index) {
    return "object line " + std::to_string(index + 1);
}

} // namespace

std::expected<TextWorldImage, std::string> makeTextWorldImage(const TextWorld &world) {
    if (const auto problems = world.problems(); !problems.empty()) {
        return std::unexpected(problems.front());
    }
    if (world.objectCount() > textWorldMostObjects) {
        return std::unexpected(std::string("The file lists more objects than the game reads."));
    }
    const auto &revision = imageLayout();
    TextWorldImage image;
    image.bytes.assign(revision.expectedBytes, std::byte{0});
    auto &bytes = image.bytes;
    put16(bytes, headerField(HeaderField::majRev).offset, revision.major);
    put16(bytes, headerField(HeaderField::minRev).offset, revision.minor);

    // The header: each value in its field; the one start and direction of
    // the text in all four start slots.
    const auto scalar = [&](HeaderField field, const char *name) -> std::expected<void, std::string> {
        const auto value = world.headerValue(name);
        if (!value || !fits16(*value)) {
            return std::unexpected("The header value " + std::string(name) +
                                   " is missing or too large for the editor.");
        }
        put16(bytes, headerField(field).offset, *value);
        return {};
    };
    for (const auto &[field, name] : {std::pair{HeaderField::world, "world"},
                                      std::pair{HeaderField::music, "music"},
                                      std::pair{HeaderField::region, "region"}}) {
        if (const auto stored = scalar(field, name); !stored) {
            return std::unexpected(stored.error());
        }
    }
    for (const auto &[field, name] : {std::pair{HeaderField::posDecor, "posDecor"},
                                      std::pair{HeaderField::dimDecor, "dimDecor"}}) {
        const auto value = world.headerPoint(name);
        if (!value) {
            return std::unexpected("The header value " + std::string(name) + " is missing.");
        }
        put32(bytes, headerField(field).offset, value->x);
        put32(bytes, headerField(field).offset + 4, value->y);
    }
    const auto start = world.headerPoint("blupiPos");
    const auto direction = world.headerValue("blupiDir");
    if (!start || !direction ||
        *direction < std::numeric_limits<std::int32_t>::min() ||
        *direction > std::numeric_limits<std::int32_t>::max()) {
        return std::unexpected(std::string("The start or its direction is missing."));
    }
    for (std::size_t index = 0; index < 4; ++index) {
        put32(bytes, headerField(HeaderField::blupiPos).offset + index * 8, start->x);
        put32(bytes, headerField(HeaderField::blupiPos).offset + index * 8 + 4, start->y);
        put32(bytes, headerField(HeaderField::blupiDir).offset + index * 4, *direction);
    }

    for (const auto layer : {SectionKind::foreground, SectionKind::bigDecor}) {
        for (std::size_t x = 0; x < layout::mapWidth; ++x) {
            for (std::size_t y = 0; y < layout::mapHeight; ++y) {
                const auto value = world.tile(layer, x, y);
                if (!value || !fits16(*value)) {
                    return std::unexpected("The tile number at column " + std::to_string(x) +
                                           ", row " + std::to_string(y) +
                                           " is too large for the editor.");
                }
                put16(bytes, cellOffset(layer, x, y), *value);
            }
        }
    }

    const auto objects = sectionOffset(SectionKind::objects);
    for (std::size_t index = 0; index < world.objectCount(); ++index) {
        const auto at = objects + index * layout::objectRecordBytes;
        for (const auto &field : scalarFields) {
            const auto value = world.objectValue(index, field.name);
            if (!value || !fits16(*value)) {
                return std::unexpected("The value " + std::string(field.name) + " of " +
                                       lineName(index) + " is missing or too large.");
            }
            put16(bytes, at + objectField(field.field).offset, *value);
        }
        if (*world.objectValue(index, "type") == 0) {
            return std::unexpected("The type of " + lineName(index) +
                                   " is 0, where the game stops reading objects.");
        }
        for (const auto &field : pointFields) {
            const auto value = world.objectPoint(index, field.name);
            if (!value) {
                return std::unexpected("The position " + std::string(field.name) + " of " +
                                       lineName(index) + " is missing.");
            }
            put32(bytes, at + objectField(field.field).offset, value->x);
            put32(bytes, at + objectField(field.field).offset + 4, value->y);
        }
        image.lineOfSlot[index] = index;
    }
    image.pristine = image.bytes;
    return image;
}

std::expected<void, std::string> storeTextWorldImage(TextWorld &world, TextWorldImage &image,
                                                     std::span<const std::byte> current) {
    const auto &revision = imageLayout();
    const auto probe = probeWorld(current);
    if (current.size() != revision.expectedBytes || probe.descriptor != &revision ||
        image.pristine.size() != current.size()) {
        return std::unexpected(std::string("This is not the image of a text world."));
    }
    const WorldView view(current, probe);
    const auto header = *view.header();
    const auto pool = *view.objects();

    // What a text world has no place for must be as it was made.
    const auto unchanged = [&](std::size_t offset, std::size_t size) {
        return std::equal(current.begin() + static_cast<std::ptrdiff_t>(offset),
                          current.begin() + static_cast<std::ptrdiff_t>(offset + size),
                          image.pristine.begin() + static_cast<std::ptrdiff_t>(offset));
    };
    for (const auto field : {HeaderField::reserve1, HeaderField::reserve2, HeaderField::name,
                             HeaderField::reserve3}) {
        const auto range = headerField(field).range();
        if (!unchanged(range.offset, range.size)) {
            return std::unexpected(
                field == HeaderField::name
                    ? std::string("A Mobile Eggbert world has no title; remove it to save.")
                    : std::string("A reserved part of the header was changed; a Mobile Eggbert "
                                  "world cannot store it."));
        }
    }
    for (std::size_t index = 1; index < 4; ++index) {
        if (*header.blupiPos(index) != *header.blupiPos(0) ||
            *header.blupiDir(index) != *header.blupiDir(0)) {
            return std::unexpected(std::string(
                "A Mobile Eggbert world has one start; starts 2 to 4 must equal start 1."));
        }
    }
    std::size_t active = 0;
    for (std::size_t slot = 0; slot < pool.slotCount(); ++slot) {
        const auto record = *pool.record(slot);
        if (record.isFree()) {
            continue;
        }
        ++active;
        if (record.unknown1 != 0 || record.unknown2 != 0) {
            return std::unexpected("The object in slot " + std::to_string(slot) +
                                   " has values a Mobile Eggbert world cannot store.");
        }
    }
    if (active > textWorldMostObjects) {
        return std::unexpected(std::string("The game reads at most 200 objects."));
    }

    auto edited = world;
    auto lines = image.lineOfSlot;
    bool ok = true;
    const auto setValue = [&](const char *name, long long value) {
        if (edited.headerValue(name) != value) {
            ok = edited.setHeaderValue(name, value) && ok;
        }
    };
    const auto setPoint = [&](const char *name, Point value) {
        if (edited.headerPoint(name) != value) {
            ok = edited.setHeaderPoint(name, value) && ok;
        }
    };
    setValue("world", header.world());
    setValue("music", header.music());
    setValue("region", header.region());
    setPoint("posDecor", header.posDecor());
    setPoint("dimDecor", header.dimDecor());
    setPoint("blupiPos", *header.blupiPos(0));
    setValue("blupiDir", *header.blupiDir(0));

    for (const auto layer : {SectionKind::foreground, SectionKind::bigDecor}) {
        const auto cells = *view.layer(layer);
        for (std::size_t x = 0; x < layout::mapWidth; ++x) {
            for (std::size_t y = 0; y < layout::mapHeight; ++y) {
                const long long value = *cells.cell(x, y);
                if (edited.tile(layer, x, y) != value) {
                    ok = edited.setTile(layer, x, y, value) && ok;
                }
            }
        }
    }

    // Objects that stay in their line, then lines to remove (last first, so
    // the indices of the others are adjusted once each), then new lines.
    std::vector<std::size_t> removed;
    for (std::size_t slot = 0; slot < pool.slotCount() && slot < lines.size(); ++slot) {
        const auto record = *pool.record(slot);
        if (!lines[slot]) {
            continue;
        }
        if (record.isFree()) {
            removed.push_back(slot);
            continue;
        }
        for (const auto &field : scalarFields) {
            const long long value = record.*field.stored;
            if (edited.objectValue(*lines[slot], field.name) != value) {
                ok = edited.setObjectValue(*lines[slot], field.name, value) && ok;
            }
        }
        for (const auto &field : pointFields) {
            if (edited.objectPoint(*lines[slot], field.name) != record.*field.stored) {
                ok = edited.setObjectPoint(*lines[slot], field.name, record.*field.stored) && ok;
            }
        }
    }
    std::ranges::sort(removed, [&](std::size_t a, std::size_t b) { return *lines[a] > *lines[b]; });
    for (const auto slot : removed) {
        const auto line = *lines[slot];
        ok = edited.removeObject(line) && ok;
        lines[slot].reset();
        for (auto &other : lines) {
            if (other && *other > line) {
                --*other;
            }
        }
    }
    for (std::size_t slot = 0; slot < pool.slotCount() && slot < lines.size(); ++slot) {
        const auto record = *pool.record(slot);
        if (lines[slot] || record.isFree()) {
            continue;
        }
        TextWorld::ObjectLine fields;
        for (const auto &field : scalarFields) {
            fields.*field.member = record.*field.stored;
        }
        for (const auto &field : pointFields) {
            fields.*field.member = record.*field.stored;
        }
        ok = edited.addObject(fields) && ok;
        lines[slot] = edited.objectCount() - 1;
    }
    if (!ok) {
        return std::unexpected(
            std::string("A value could not be written into the text; nothing was changed."));
    }
    world = std::move(edited);
    image.lineOfSlot = lines;
    return {};
}

void mirrorTextWorldStart(std::span<std::byte> image) {
    const auto &revision = *findRevision(FormatRevision::v1_3);
    if (image.size() != revision.expectedBytes) {
        return;
    }
    const auto position = headerField(HeaderField::blupiPos).offset;
    const auto direction = headerField(HeaderField::blupiDir).offset;
    for (std::size_t index = 1; index < 4; ++index) {
        std::copy_n(image.begin() + static_cast<std::ptrdiff_t>(position), 8,
                    image.begin() + static_cast<std::ptrdiff_t>(position + index * 8));
        std::copy_n(image.begin() + static_cast<std::ptrdiff_t>(direction), 4,
                    image.begin() + static_cast<std::ptrdiff_t>(direction + index * 4));
    }
}

std::string newTextWorld() {
    return newTextWorld(newWorld(GameProfile::mobileEggbert));
}

std::string newTextWorld(std::span<const std::byte> wanted) {
    std::string blank = "DescFile: posDecor=0;0 dimDecor=100;100 world=0 music=0 region=0 "
                        "blupiPos=0;0 blupiDir=0 \n";
    for (const auto *section : {"Decor: \n", "BigDecor: \n"}) {
        blank += section;
        for (std::size_t line = 0; line < layout::mapWidth; ++line) {
            blank += std::string(layout::mapHeight - 1, ',') + "\n";
        }
    }
    auto world = TextWorld::parse(blank);
    auto image = makeTextWorldImage(world);
    if (!image || !storeTextWorldImage(world, *image, wanted)) {
        return {};
    }
    return world.write();
}

} // namespace eggbert
