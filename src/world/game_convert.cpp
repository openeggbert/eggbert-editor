// SPDX-License-Identifier: GPL-3.0-only
#include "world/game_convert.hpp"

#include <algorithm>
#include <map>

#include "world/new_world.hpp"
#include "world/world_document.hpp"
#include "world/world_probe.hpp"

namespace eggbert {

namespace {

constexpr std::string_view recon = "blupi_recon e05e22b ";

bool between(int value, int low, int high) {
    return value >= low && value <= high;
}

// Object types the first game's code never refers to (it defines the names).
bool typeUnknownToFirst(int type) {
    return type == 18 || between(type, 39, 44) || between(type, 46, 57) || between(type, 90, 100);
}

// Regions whose backdrop carries a different name in the two games' editors.
bool regionDiffers(int region) {
    return region == 4 || region == 6 || region == 9 || region == 10 || region == 18 ||
           region == 21 || region == 23;
}

struct Key {
    GameItem item;
    CarryKind kind;
    int value;
    auto operator<=>(const Key &) const = default;
};

class Collector {
public:
    void cell(GameItem item, CarryKind kind, int value, CellPos at, std::string text,
              std::string_view provenance) {
        auto &note = entry(item, kind, value, std::move(text), provenance);
        if (!note.firstCell) {
            note.firstCell = at;
        }
        ++note.count;
    }
    void slot(GameItem item, CarryKind kind, int value, std::size_t at, std::string text,
              std::string_view provenance) {
        auto &note = entry(item, kind, value, std::move(text), provenance);
        if (!note.firstSlot) {
            note.firstSlot = at;
        }
        ++note.count;
    }
    void header(GameItem item, CarryKind kind, int value, std::string text,
                std::string_view provenance) {
        ++entry(item, kind, value, std::move(text), provenance).count;
    }

    [[nodiscard]] std::vector<GameConversionNote> take() {
        std::vector<GameConversionNote> notes;
        for (auto &[key, note] : notes_) {
            notes.push_back(std::move(note));
        }
        std::ranges::stable_sort(notes, [](const GameConversionNote &a,
                                            const GameConversionNote &b) {
            return a.kind == CarryKind::unknownToTarget && b.kind != CarryKind::unknownToTarget;
        });
        return notes;
    }

private:
    GameConversionNote &entry(GameItem item, CarryKind kind, int value, std::string text,
                              std::string_view provenance) {
        auto [position, inserted] = notes_.try_emplace(Key{item, kind, value});
        if (inserted) {
            position->second.item = item;
            position->second.kind = kind;
            position->second.value = value;
            position->second.text = std::move(text);
            position->second.provenance = std::string(recon) + std::string(provenance);
        }
        return position->second;
    }

    std::map<Key, GameConversionNote> notes_;
};

bool knownPair(GameProfile from, GameProfile to) {
    return from != to && from != GameProfile::unknown && to != GameProfile::unknown;
}

} // namespace

std::vector<GameConversionNote> reviewGameConversion(const WorldView &world, GameProfile from,
                                                     GameProfile to) {
    if (!knownPair(from, to)) {
        return {};
    }
    const bool toFirst = to == GameProfile::speedyBlupi1;
    Collector notes;

    if (const auto foreground = world.foreground()) {
        for (std::size_t x = 0; x < layout::mapWidth; ++x) {
            for (std::size_t y = 0; y < layout::mapHeight; ++y) {
                const int value = *foreground->cell(x, y);
                if (toFirst && between(value, 246, 440)) {
                    notes.cell(GameItem::foregroundTile, CarryKind::unknownToTarget, value, {x, y},
                               "The first game has no such tile: it draws nothing there and "
                               "nothing is solid.",
                               "src/pixmap.cpp:788-790, src/decblupi.cpp:3805");
                } else if (between(value, 48, 67)) {
                    notes.cell(GameItem::foregroundTile, CarryKind::differsInTarget, value, {x, y},
                               toFirst ? "The second game shows tile 215 for this number; the "
                                         "first game draws its own block " +
                                             std::to_string(value) + " here."
                                       : std::string(
                                             "The second game has no picture for this block: "
                                             "when it loads the world it shows tile 215 instead."),
                               "games/sb2/src/decor.cpp:675-684, games/sb2/src/pixtables.h:154-160");
                }
            }
        }
    }
    if (const auto bigDecor = world.bigDecor()) {
        for (std::size_t x = 0; x < layout::mapWidth; ++x) {
            for (std::size_t y = 0; y < layout::mapHeight; ++y) {
                const int value = *bigDecor->cell(x, y);
                if (toFirst && between(value, 54, 99)) {
                    notes.cell(GameItem::bigDecorTile, CarryKind::unknownToTarget, value, {x, y},
                               "The first game has no such back-layer picture and draws nothing.",
                               "src/pixmap.cpp:818-820, src/pixtables.h:755-756");
                }
            }
        }
    }
    if (const auto objects = world.objects()) {
        for (std::size_t slot = 0; slot < objects->slotCount(); ++slot) {
            const auto record = *objects->record(slot);
            if (record.isFree()) {
                continue;
            }
            if (toFirst && typeUnknownToFirst(record.type)) {
                notes.slot(GameItem::objectType, CarryKind::unknownToTarget, record.type, slot,
                           "The first game's code never refers to this object type.",
                           "scripted search of src/*.cpp for the names of src/def.h:995-1066");
            } else if (!toFirst && record.type == 58) {
                notes.slot(GameItem::objectType, CarryKind::unknownToTarget, record.type, slot,
                           "The second game's code never refers to this object type.",
                           "scripted search of games/sb2/src/*.cpp for the names of "
                           "games/sb2/src/def.h:993-1064");
            }
            // Lifts and crates are drawn with their stored tile picture.
            const bool storedPicture = (record.type == 1 || record.type == 12) && record.channel == 1;
            if (storedPicture && toFirst && between(record.icon, 246, 440)) {
                notes.slot(GameItem::objectIcon, CarryKind::differsInTarget, record.icon, slot,
                           "This lift or crate uses a tile picture the first game does not "
                           "have and would be invisible.",
                           "src/pixmap.cpp:788-790, src/decblock.cpp:1414-1424");
            } else if (storedPicture && between(record.icon, 48, 67)) {
                notes.slot(GameItem::objectIcon, CarryKind::differsInTarget, record.icon, slot,
                           toFirst ? "This lift or crate uses a picture number the second game "
                                     "has no picture for; the first game draws its block there."
                                   : "This lift or crate uses a block picture the second game "
                                     "does not have (its loader replaces such tiles only in the "
                                     "tile layer).",
                           "games/sb2/src/pixtables.h:154-160, games/sb2/src/decor.cpp:675-684");
            }
        }
    }
    if (const auto header = world.header()) {
        const int region = header->region();
        if (toFirst && between(region, 24, 31)) {
            notes.header(GameItem::region, CarryKind::unknownToTarget, region,
                         "The first game's editor offers backdrops 0 to 23 only.",
                         "src/event.cpp:3093-3095");
        } else if (regionDiffers(region)) {
            notes.header(GameItem::region, CarryKind::differsInTarget, region,
                         "This backdrop number is a different picture in the other game.",
                         "src/res/blupi.rc:286-300, games/sb2/res/sb2-eggbert-en.rc:288-302");
        }
    }
    return notes.take();
}

std::expected<GameConversion, ConversionFailure>
convertGame(std::span<const std::byte> source, GameProfile from, GameProfile to,
            UnknownPolicy policy) {
    if (!knownPair(from, to)) {
        return std::unexpected(ConversionFailure{
            ConversionError::invalidTarget, "Choose the two different games to convert between."});
    }
    const auto probe = probeWorld(source);
    if (probe.descriptor == nullptr ||
        (probe.status != ProbeStatus::sizeMatches && probe.status != ProbeStatus::surplus)) {
        return std::unexpected(ConversionFailure{
            ConversionError::sourceNotConvertible,
            "Only a complete world of revision 1.0 to 1.3 can be converted."});
    }
    GameConversion result;
    const auto targetRevision = newWorldRevision(to);
    if (probe.descriptor->revision == targetRevision) {
        result.bytes.assign(source.begin(), source.end());
    } else {
        // Both stock revisions hold everything any revision holds.
        auto converted = convertRevision(source, targetRevision);
        if (!converted) {
            return std::unexpected(converted.error());
        }
        result.bytes = std::move(converted->bytes);
        result.revisionNotes = std::move(converted->notes);
    }
    {
        const auto targetProbe = probeWorld(result.bytes);
        result.notes = reviewGameConversion(WorldView(result.bytes, targetProbe), from, to);
    }
    const auto unknown = std::ranges::count_if(result.notes, [](const GameConversionNote &note) {
        return note.kind == CarryKind::unknownToTarget;
    });
    if (unknown == 0 || policy == UnknownPolicy::keep) {
        return result;
    }
    if (policy == UnknownPolicy::refuse) {
        std::size_t places = 0;
        for (const auto &note : result.notes) {
            places += note.kind == CarryKind::unknownToTarget ? note.count : 0;
        }
        return std::unexpected(ConversionFailure{
            ConversionError::wouldLoseData,
            std::to_string(unknown) + " stored value(s) in " + std::to_string(places) +
                " place(s) are unknown to the target game."});
    }

    // UnknownPolicy::remove, through the ordinary edit path so only the
    // named cells, slots and the region field change.
    const bool toFirst = to == GameProfile::speedyBlupi1;
    WorldDocument document{std::move(result.bytes)};
    EditBuilder builder(document);
    const auto view = document.view();
    for (const auto kind : {SectionKind::foreground, SectionKind::bigDecor}) {
        const auto layer = view.layer(kind);
        if (!layer) {
            continue;
        }
        for (std::size_t x = 0; x < layout::mapWidth; ++x) {
            for (std::size_t y = 0; y < layout::mapHeight; ++y) {
                const int value = *layer->cell(x, y);
                const bool drop = toFirst && (kind == SectionKind::foreground
                                                  ? between(value, 246, 440)
                                                  : between(value, 54, 99));
                if (drop && builder.setCell(kind, x, y, layout::emptyCell)) {
                    ++result.removedCount;
                }
            }
        }
    }
    if (const auto objects = view.objects()) {
        for (std::size_t slot = 0; slot < objects->slotCount(); ++slot) {
            const auto record = *objects->record(slot);
            const bool drop = !record.isFree() &&
                              (toFirst ? typeUnknownToFirst(record.type) : record.type == 58);
            if (drop && builder.setObjectRecord(slot, ObjectRecord{})) {
                ++result.removedCount;
            }
        }
    }
    if (const auto header = view.header();
        header && toFirst && between(header->region(), 24, 31) && builder.setRegion(2)) {
        ++result.removedCount;
    }
    const auto applied = document.apply(builder.build("Convert game"));
    if (!applied && result.removedCount != 0) {
        return std::unexpected(ConversionFailure{
            ConversionError::sourceNotConvertible,
            "The values unknown to the target game could not be removed."});
    }
    const auto bytes = document.currentBytes();
    result.bytes.assign(bytes.begin(), bytes.end());
    // What is left to say after the removal.
    result.notes = reviewGameConversion(document.view(), from, to);
    return result;
}

} // namespace eggbert
