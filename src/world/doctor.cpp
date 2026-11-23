// SPDX-License-Identifier: GPL-3.0-only
#include "world/doctor.hpp"

#include "world/reachability.hpp"

#include <algorithm>
#include <map>

#include "world/adaptive.hpp"
#include "world/metadata.hpp"
#include "world/object_catalog.hpp"
#include "world/object_ops.hpp"
#include "world/offset_map.hpp"
#include "world/profile.hpp"

namespace eggbert {

namespace {

constexpr RuleInfo rules[] = {
    {"STRUCT-SHORT", Severity::error, "The file ends inside the four-byte revision field."},
    {"STRUCT-UNKNOWN-REVISION", Severity::error, "The revision field is not 1.0 to 1.3."},
    {"STRUCT-TRUNCATED", Severity::error, "The file ends before the layout its revision selects."},
    {"STRUCT-SURPLUS", Severity::note, "Bytes follow the layout; they are kept unchanged."},
    {"STRUCT-READONLY-REVISION", Severity::note,
     "Revisions 1.0 and 1.1 are read-only in this editor."},
    {"HDR-MUSIC-RANGE", Severity::warning, "Music is outside the chosen game's range."},
    {"HDR-REGION-RANGE", Severity::warning, "Region is outside the chosen game's range."},
    {"HDR-START-OUTSIDE", Severity::warning, "A player start lies outside the world."},
    {"HDR-START-DIRECTION", Severity::note, "A start direction is neither left (1) nor right (2)."},
    {"HDR-NAME-UNTERMINATED", Severity::error, "The title has no terminator inside its field."},
    {"HDR-NAME-LONG", Severity::note, "The title is longer than a player can type."},
    {"HDR-NAME-IGNORED", Severity::note, "The games ignore the title of this revision."},
    {"HDR-NAME-PADDING", Severity::note, "Non-zero bytes follow the title's terminator."},
    {"HDR-RESERVED-NONZERO", Severity::note, "A reserved header field is not all zero."},
    {"HDR-SCROLL-UNUSUAL", Severity::note, "A scroll flag holds a value other than 0 or 100."},
    {"TILE-BELOW-EMPTY", Severity::warning, "A cell stores a value below -1."},
    {"TILE-NO-COLLISION-DATA", Severity::warning,
     "A foreground value has no collision data in the chosen game."},
    {"TILE-NOT-DRAWN", Severity::warning, "A stored tile number has no picture in the chosen game."},
    {"TILE-SB2-REMAP", Severity::note,
     "Speedy Blupi 2 shows stored foreground values 48 to 67 as 215."},
    {"ADAPT-BORDER-FORM", Severity::note,
     "A border tile's form differs from what the game's editor would produce here."},
    {"OBJ-TYPE-RUNTIME", Severity::warning,
     "A stored object is of a type the game only creates while playing."},
    {"OBJ-TYPE-UNKNOWN", Severity::warning, "A stored object type is not known for the chosen game."},
    {"OBJ-POSITION-OUTSIDE", Severity::warning, "An object position lies outside the world."},
    {"OBJ-CHANNEL", Severity::warning,
     "An object's stored channel is not one the game's editor writes for its type."},
    {"OBJ-ICON-RANGE", Severity::warning, "A lift or crate icon has no picture in the chosen game."},
    {"OBJ-PATH-ZERO-DURATION", Severity::warning, "A moving object has a zero travel duration."},
    {"OBJ-POOL-FULL", Severity::warning, "Every object slot is in use."},
    {"OBJ-LOW-HEADROOM", Severity::note, "Few object slots are free (estimate)."},
    {"OBJ-FREE-STALE", Severity::note, "Free object slots hold other non-zero bytes."},
    {"REACH-CUT-OFF", Severity::note,
     "A treasure or goal lies where no path of free cells leads from a start (estimate)."},
};

// Every sentence a finding can carry, as a pattern. Interfaces in other
// languages translate the pattern and fill in the same arguments, so a new
// sentence must be added here (a test compares what the rules use with it).
namespace pattern {
constexpr std::string_view structShort =
    "The file has %1 byte(s); at least 4 are needed for the revision field.";
constexpr std::string_view revisionLater =
    "Revision field %1.%2 is later than every known revision; its layout is unverified.";
constexpr std::string_view revisionUnknown =
    "Revision field %1.%2 is not a known revision; this may not be a world file.";
constexpr std::string_view truncated =
    "The file ends at offset %1; %2 byte(s) of the revision %3.%4 layout are missing.";
constexpr std::string_view surplus = "%1 byte(s) follow the layout; they are kept unchanged.";
constexpr std::string_view readOnly =
    "Revision %1.%2 can be viewed and inspected but not edited in this editor.";
constexpr std::string_view musicRange = "Music %1 is outside %2 to %3.";
constexpr std::string_view regionRange =
    "Region %1 is outside %2 to %3; the game would look for a background picture that it does "
    "not offer.";
constexpr std::string_view startOutside =
    "Start %1 at (%2, %3) is outside the 6400 x 6400 pixel world.";
constexpr std::string_view startDirection =
    "Start %1 has direction %2; the games treat anything but 1 as facing right.";
constexpr std::string_view nameUnterminatedRead =
    "The title fills all 100 bytes without a terminator; the game copies it as a C string and "
    "would read past the field.";
constexpr std::string_view nameUnterminated =
    "The title fills all 100 bytes without a terminator.";
constexpr std::string_view nameLong = "The title has %1 bytes; a player can type at most 40.";
constexpr std::string_view namePadding =
    "Non-zero bytes follow the title's terminator; they are kept unchanged.";
constexpr std::string_view nameIgnored =
    "This file is not revision 1.3, so the games do not read its stored title.";
constexpr std::string_view reservedMarker =
    "%1 is not all zero; it is kept unchanged (demo editions use its first value as a marker).";
constexpr std::string_view reserved = "%1 is not all zero; it is kept unchanged.";
constexpr std::string_view scrollUnusual =
    "dimDecor holds %1; the games only test it for zero, so it behaves like 100.";
constexpr std::string_view tileBelowEmpty =
    "%1 %2 cell(s) store %3: values below -1 are not tile numbers.";
constexpr std::string_view tileNotDrawn =
    "%1 %2 cell(s) store %3: the chosen game's picture table has no such entry, so nothing is "
    "drawn.";
constexpr std::string_view tileNoCollision =
    "%1 %2 cell(s) store %3: the chosen game has a picture but no collision data for it and "
    "treats it as passable.";
constexpr std::string_view remapped =
    "%1 foreground cell(s) store 48 to 67; Speedy Blupi 2 shows them as 215 after loading. The "
    "file keeps the stored values.";
constexpr std::string_view borderForm =
    "%1 foreground cell(s) store %2 where the game's editor would currently give a different "
    "border form (%3 at the first of them). This can be intentional.";
constexpr std::string_view positionOutside =
    "%1 (%2, %3) is outside the 6400 x 6400 pixel world.";
constexpr std::string_view zeroDuration =
    "start and end differ but a travel duration is 0; the engines skip the movement when the "
    "duration is 0, so the object never arrives.";
constexpr std::string_view typeRuntime =
    "type %1 (%2) is created by the game only while playing.";
constexpr std::string_view typeUnknown = "type %1 is not an object type of the chosen game.";
constexpr std::string_view channel =
    "channel %1 is not one the game's editor writes for %2.";
constexpr std::string_view iconRange =
    "icon %1 has no entry in the chosen game's tile picture table, so nothing is drawn.";
constexpr std::string_view poolFull =
    "All %1 object slots are in use. This is the format's hard limit: nothing can be added and "
    "effects the game creates while playing find no slot.";
constexpr std::string_view lowHeadroom =
    "Only %1 object slot(s) are free; fewer than %2 may leave no room for effects created while "
    "playing. This is an estimate, not a proven limit.";
constexpr std::string_view freeStale =
    "%1 free slot(s) hold other non-zero bytes; they are kept unchanged until the slot is "
    "reused.";
constexpr std::string_view cutOffGoal =
    "The goal in slot %1 at (%2, %3) is in a cell that no chain of cells free of fully solid "
    "tiles connects to a start. Unless the game changes the map while playing (a block "
    "destroyed, a crate pushed), it cannot be collected. Jumps, falls and lifts are not judged.";
constexpr std::string_view cutOffTreasure =
    "The treasure in slot %1 at (%2, %3) is in a cell that no chain of cells free of fully solid "
    "tiles connects to a start. Unless the game changes the map while playing (a block "
    "destroyed, a crate pushed), it cannot be collected. Jumps, falls and lifts are not judged.";
} // namespace pattern

constexpr std::string_view patterns[] = {
    pattern::structShort,      pattern::revisionLater,    pattern::revisionUnknown,
    pattern::truncated,        pattern::surplus,          pattern::readOnly,
    pattern::musicRange,       pattern::regionRange,      pattern::startOutside,
    pattern::startDirection,   pattern::nameUnterminatedRead, pattern::nameUnterminated,
    pattern::nameLong,         pattern::namePadding,      pattern::nameIgnored,
    pattern::reservedMarker,   pattern::reserved,         pattern::scrollUnusual,
    pattern::tileBelowEmpty,   pattern::tileNotDrawn,     pattern::tileNoCollision,
    pattern::remapped,         pattern::borderForm,       pattern::positionOutside,
    pattern::zeroDuration,     pattern::typeRuntime,      pattern::typeUnknown,
    pattern::channel,          pattern::iconRange,        pattern::poolFull,
    pattern::lowHeadroom,      pattern::freeStale,        pattern::cutOffGoal,
    pattern::cutOffTreasure,
};

// A sentence: its pattern and what fills %1, %2, ...
struct Text {
    std::string_view pattern;
    std::vector<std::string> arguments = {};
};

const RuleInfo &rule(std::string_view id) {
    for (const auto &info : rules) {
        if (info.id == id) {
            return info;
        }
    }
    return rules[0];
}

class Report {
public:
    Report(const WorldView &world, GameProfile profile) : world_(world), profile_(profile) {}

    Finding &add(std::string_view id, Confidence confidence, Text text,
                 std::string_view evidence, ByteRange range, bool profileSpecific) {
        Finding finding;
        const auto &info = rule(id);
        finding.ruleId = info.id;
        finding.severity = info.severity;
        finding.confidence = confidence;
        finding.profile = profileSpecific ? profile_ : GameProfile::unknown;
        finding.message = fillPattern(text.pattern, text.arguments);
        finding.messagePattern = text.pattern;
        finding.messageArguments = std::move(text.arguments);
        finding.evidence = evidence;
        finding.range = range;
        report_.findings.push_back(std::move(finding));
        return report_.findings.back();
    }

    DoctorReport take() { return std::move(report_); }
    [[nodiscard]] const WorldView &world() const { return world_; }
    [[nodiscard]] GameProfile profile() const { return profile_; }

private:
    const WorldView &world_;
    GameProfile profile_;
    DoctorReport report_;
};

std::string text(std::string_view view) {
    return std::string(view);
}

std::string num(long long value) {
    return std::to_string(value);
}

void checkStructure(Report &report) {
    const auto &probe = report.world().probe();
    switch (probe.status) {
    case ProbeStatus::revisionUnavailable:
        report.add("STRUCT-SHORT", Confidence::structural,
                   {pattern::structShort, {num(static_cast<long long>(probe.inputBytes))}},
                   "plan.md, binary layout", {0, probe.inputBytes}, false);
        return;
    case ProbeStatus::unknownRevision:
        report.add("STRUCT-UNKNOWN-REVISION", Confidence::structural,
                   {probe.laterThanKnownRevisions ? pattern::revisionLater
                                                  : pattern::revisionUnknown,
                    {num(*probe.headerMajor), num(*probe.headerMinor)}},
                   "plan.md, compatibility table", {0, layout::revisionFieldBytes}, false)
            .headerField = HeaderField::majRev;
        return;
    case ProbeStatus::truncated:
        report.add("STRUCT-TRUNCATED", Confidence::structural,
                   {pattern::truncated,
                    {num(static_cast<long long>(probe.inputBytes)),
                     num(static_cast<long long>(probe.missingBytes)),
                     num(probe.descriptor->major), num(probe.descriptor->minor)}},
                   "plan.md, compatibility table",
                   {probe.firstMissingOffset.value_or(probe.inputBytes), probe.missingBytes},
                   false);
        break;
    case ProbeStatus::surplus:
        report.add("STRUCT-SURPLUS", Confidence::structural,
                   {pattern::surplus, {num(static_cast<long long>(probe.surplusBytes))}},
                   "plan.md, losslessness", {*probe.expectedBytes, probe.surplusBytes}, false);
        break;
    case ProbeStatus::sizeMatches:
        break;
    }
    if (probe.descriptor != nullptr && probe.descriptor->writePolicy == WritePolicy::readOnly) {
        report.add("STRUCT-READONLY-REVISION", Confidence::structural,
                   {pattern::readOnly,
                    {num(probe.descriptor->major), num(probe.descriptor->minor)}},
                   "plan.md, fixed 1.0 scope", {0, layout::revisionFieldBytes}, false)
            .headerField = HeaderField::minRev;
    }
}

void checkHeader(Report &report) {
    const auto header = report.world().header();
    if (!header) {
        return;
    }
    const auto *info = profileInfo(report.profile());
    const auto fieldRange = [](HeaderField field) { return headerField(field).range(); };

    if (info != nullptr) {
        if (!info->music.contains(header->music())) {
            report.add("HDR-MUSIC-RANGE", Confidence::verifiedRange,
                       {pattern::musicRange,
                        {num(header->music()), num(info->music.minimum),
                         num(info->music.maximum)}},
                       info->music.provenance, fieldRange(HeaderField::music), true)
                .headerField = HeaderField::music;
        }
        if (!info->region.contains(header->region())) {
            report.add("HDR-REGION-RANGE", Confidence::verifiedRange,
                       {pattern::regionRange,
                        {num(header->region()), num(info->region.minimum),
                         num(info->region.maximum)}},
                       info->region.provenance, fieldRange(HeaderField::region), true)
                .headerField = HeaderField::region;
        }
    }

    for (std::size_t index = 0; index < 4; ++index) {
        const auto position = *header->blupiPos(index);
        if (!cellOfPosition(position)) {
            auto &finding = report.add(
                "HDR-START-OUTSIDE", Confidence::structural,
                {pattern::startOutside,
                 {num(static_cast<long long>(index) + 1), num(position.x), num(position.y)}},
                "docs/research/evidence.md, geometry",
                headerField(HeaderField::blupiPos).elementRange(index), false);
            finding.headerField = HeaderField::blupiPos;
            finding.elementIndex = index;
        }
        const auto direction = *header->blupiDir(index);
        if (direction != startDirectionLeft && direction != startDirectionRight) {
            auto &finding = report.add(
                "HDR-START-DIRECTION", Confidence::provenLimit,
                {pattern::startDirection,
                 {num(static_cast<long long>(index) + 1), num(direction)}},
                "docs/research/evidence.md, geometry",
                headerField(HeaderField::blupiDir).elementRange(index), false);
            finding.headerField = HeaderField::blupiDir;
            finding.elementIndex = index;
        }
    }

    // Title.
    const auto nameRange = fieldRange(HeaderField::name);
    const bool titleRead = report.world().probe().candidateRevision == FormatRevision::v1_3;
    if (!header->nameTerminated()) {
        auto &finding = report.add(
            "HDR-NAME-UNTERMINATED", Confidence::provenLimit,
            {titleRead ? pattern::nameUnterminatedRead : pattern::nameUnterminated},
            "docs/research/evidence.md, title handling (strcpy)", nameRange, false);
        finding.headerField = HeaderField::name;
        if (!titleRead) {
            finding.severity = Severity::warning;
        }
        // The one safe repair: terminate the field at its last byte.
        const auto last = nameRange.end() - 1;
        DoctorFix fix;
        fix.label = "Terminate Title";
        fix.changes.push_back({last, {report.world().bytes()[last]}, {std::byte{0}}});
        finding.fix = std::move(fix);
    } else {
        if (header->nameLength() > typedTitleLimit) {
            report.add("HDR-NAME-LONG", Confidence::provenLimit,
                       {pattern::nameLong,
                        {num(static_cast<long long>(header->nameLength()))}},
                       "docs/research/evidence.md, title handling", nameRange, false)
                .headerField = HeaderField::name;
        }
        if (!header->namePaddingIsZero()) {
            const auto start = nameRange.offset + header->nameLength() + 1;
            report.add("HDR-NAME-PADDING", Confidence::structural,
                       {pattern::namePadding},
                       "plan.md, losslessness", {start, nameRange.end() - start}, false)
                .headerField = HeaderField::name;
        }
    }
    if (!titleRead && header->nameLength() > 0) {
        report.add("HDR-NAME-IGNORED", Confidence::provenLimit,
                   {pattern::nameIgnored},
                   "docs/research/evidence.md, file layout", nameRange, false)
            .headerField = HeaderField::name;
    }

    for (const auto field : {HeaderField::reserve1, HeaderField::reserve2, HeaderField::reserve3}) {
        if (!header->reservedIsZero(field)) {
            report.add("HDR-RESERVED-NONZERO", Confidence::structural,
                       {field == HeaderField::reserve1 ? pattern::reservedMarker
                                                       : pattern::reserved,
                        {text(headerField(field).name)}},
                       "docs/research/evidence.md, file layout", fieldRange(field), false)
                .headerField = field;
        }
    }
    const auto dim = header->dimDecor();
    for (const auto component : {dim.x, dim.y}) {
        if (component != 0 && component != 100) {
            report.add("HDR-SCROLL-UNUSUAL", Confidence::reported,
                       {pattern::scrollUnusual, {num(component)}},
                       "docs/research/evidence.md, geometry", fieldRange(HeaderField::dimDecor),
                       false)
                .headerField = HeaderField::dimDecor;
            break;
        }
    }
}

void checkLayer(Report &report, SectionKind kind) {
    const auto layer = report.world().layer(kind);
    if (!layer) {
        return;
    }
    const auto *info = profileInfo(report.profile());
    // Group offending cells by stored value so a large area is one finding.
    std::map<std::int16_t, std::vector<CellPos>> below;
    std::map<std::int16_t, std::vector<CellPos>> noCollision;
    std::map<std::int16_t, std::vector<CellPos>> notDrawn;
    std::vector<CellPos> remapped;
    for (std::size_t x = 0; x < layout::mapWidth; ++x) {
        for (std::size_t y = 0; y < layout::mapHeight; ++y) {
            const auto value = *layer->cell(x, y);
            if (value == layout::emptyCell) {
                continue;
            }
            if (value < layout::emptyCell) {
                below[value].push_back({x, y});
                continue;
            }
            if (info == nullptr) {
                continue;
            }
            if (kind == SectionKind::foreground) {
                if (!info->foregroundAtlas.contains(value)) {
                    notDrawn[value].push_back({x, y});
                } else if (!info->foregroundTiles.contains(value)) {
                    noCollision[value].push_back({x, y});
                }
                if (info->loaderRemaps48To67 && value >= 48 && value <= 67) {
                    remapped.push_back({x, y});
                }
            } else if (!info->bigDecorAtlas.contains(value) && value != bigDecorAnimatedPlant) {
                notDrawn[value].push_back({x, y});
            }
        }
    }
    const auto layerName = kind == SectionKind::foreground ? "foreground" : "bigDecor";
    const auto emit = [&](std::string_view id, Confidence confidence,
                          std::map<std::int16_t, std::vector<CellPos>> &groups,
                          std::string_view what, std::string_view evidence, bool specific) {
        for (auto &[value, cells] : groups) {
            auto &finding = report.add(
                id, confidence,
                {what, {num(static_cast<long long>(cells.size())), layerName, num(value)}},
                evidence, *layer->cellRange(cells.front().x, cells.front().y), specific);
            finding.layer = kind;
            finding.cells = std::move(cells);
        }
    };
    emit("TILE-BELOW-EMPTY", Confidence::structural, below,
         pattern::tileBelowEmpty, "docs/research/evidence.md, file layout", false);
    if (info != nullptr) {
        emit("TILE-NOT-DRAWN", Confidence::provenLimit, notDrawn,
             pattern::tileNotDrawn,
             kind == SectionKind::foreground ? info->foregroundAtlas.provenance
                                             : info->bigDecorAtlas.provenance,
             true);
        emit("TILE-NO-COLLISION-DATA", Confidence::provenLimit, noCollision,
             pattern::tileNoCollision,
             info->foregroundTiles.provenance, true);
        if (!remapped.empty()) {
            auto &finding = report.add(
                "TILE-SB2-REMAP", Confidence::provenLimit,
                {pattern::remapped, {num(static_cast<long long>(remapped.size()))}},
                "docs/research/evidence.md (games/sb2/src/decor.cpp:675-684)",
                *layer->cellRange(remapped.front().x, remapped.front().y), true);
            finding.layer = kind;
            finding.cells = std::move(remapped);
        }
    }
}

// Table-family cells whose stored form is not the one the game's editor
// would compute from the current neighbours. The games only refresh five
// cells per edit, so such cells also occur in levels made with them; this
// is a note, never a repair.
void checkBorders(Report &report) {
    const auto layer = report.world().foreground();
    const auto profile = report.profile();
    if (!layer || profile == GameProfile::unknown) {
        return;
    }
    std::map<std::int16_t, std::vector<CellPos>> groups;
    std::map<std::int16_t, std::int16_t> firstExpected;
    for (std::size_t x = 0; x < layout::mapWidth; ++x) {
        for (std::size_t y = 0; y < layout::mapHeight; ++y) {
            const auto expected = expectedBorderForm(report.world(), profile, {x, y});
            if (!expected) {
                continue;
            }
            const auto stored = *layer->cell(x, y);
            if (baseBorderForm(profile, stored) != *expected) {
                groups[stored].push_back({x, y});
                firstExpected.try_emplace(stored, *expected);
            }
        }
    }
    for (auto &[value, cells] : groups) {
        auto &finding = report.add(
            "ADAPT-BORDER-FORM", Confidence::provenLimit,
            {pattern::borderForm,
             {num(static_cast<long long>(cells.size())), num(value), num(firstExpected[value])}},
            "docs/research/evidence.md, adaptive borders",
            *layer->cellRange(cells.front().x, cells.front().y), true);
        finding.layer = SectionKind::foreground;
        finding.cells = std::move(cells);
    }
}

// Channels the game's editor writes for a type: the presets' channels, plus
// 11 to 13 for the two enemy types whose channel the editor cycles.
bool channelExpected(GameProfile profile, std::int16_t type, std::int16_t channel) {
    if ((type == 32 || type == 33) && channel >= 11 && channel <= 13) {
        return true;
    }
    for (const auto &preset : objectPresets(profile)) {
        if (preset.type == type && preset.channel == channel) {
            return true;
        }
    }
    return false;
}

void checkObjects(Report &report) {
    const auto pool = report.world().objects();
    if (!pool) {
        return;
    }
    const auto profile = report.profile();
    const auto *info = profileInfo(profile);
    std::size_t stale = 0;
    std::optional<std::size_t> firstStale;
    for (std::size_t slot = 0; slot < pool->slotCount(); ++slot) {
        const auto record = *pool->record(slot);
        const auto fieldRange = [&](ObjectField field) { return *pool->fieldRange(slot, field); };
        const auto add = [&](std::string_view id, Confidence confidence, Text sentence,
                             std::string_view evidence, ObjectField field,
                             bool specific) -> Finding & {
            auto &finding = report.add(id, confidence, std::move(sentence), evidence,
                                       fieldRange(field), specific);
            finding.message = "Slot " + num(static_cast<long long>(slot)) + ": " + finding.message;
            finding.slotPrefix = true;
            finding.slot = slot;
            finding.objectField = field;
            return finding;
        };
        if (record.isFree()) {
            const auto raw = pool->slotBytes(slot);
            if (std::ranges::any_of(raw, [](std::byte value) { return value != std::byte{0}; })) {
                ++stale;
                if (!firstStale) {
                    firstStale = slot;
                }
            }
            continue;
        }
        for (const auto field : {ObjectField::posStart, ObjectField::posEnd}) {
            const auto position = field == ObjectField::posStart ? record.posStart : record.posEnd;
            if (!cellOfPosition(position)) {
                add("OBJ-POSITION-OUTSIDE", Confidence::structural,
                    {pattern::positionOutside,
                     {text(objectField(field).name), num(position.x), num(position.y)}},
                    "docs/research/evidence.md, geometry", field, false);
            }
        }
        if (record.posStart != record.posEnd &&
            (record.stepAdvance == 0 || record.stepRecede == 0)) {
            add("OBJ-PATH-ZERO-DURATION", Confidence::provenLimit,
                {pattern::zeroDuration},
                "blupi_recon e05e22b src/decmove.cpp:541-551",
                record.stepAdvance == 0 ? ObjectField::stepAdvance : ObjectField::stepRecede,
                false);
        }
        if (info == nullptr) {
            continue;
        }
        const auto typeInfo = objectTypeInfo(profile, record.type);
        if (typeInfo.status == ObjectTypeStatus::runtimeOnly) {
            add("OBJ-TYPE-RUNTIME", Confidence::reported,
                {pattern::typeRuntime, {num(record.type), text(typeInfo.name)}},
                "docs/research/evidence.md, objects", ObjectField::type, true);
            continue;
        }
        if (typeInfo.status == ObjectTypeStatus::unknown) {
            add("OBJ-TYPE-UNKNOWN", Confidence::verifiedRange,
                {pattern::typeUnknown, {num(record.type)}},
                "docs/research/evidence.md, objects", ObjectField::type, true);
            continue;
        }
        if (!channelExpected(profile, record.type, record.channel)) {
            add("OBJ-CHANNEL", Confidence::verifiedRange,
                {pattern::channel, {num(record.channel), text(typeInfo.name)}},
                "docs/research/evidence.md, objects", ObjectField::channel, true);
        }
        const auto *preset = presetForType(profile, record.type);
        if (preset != nullptr && preset->iconIsChoice && record.channel == 1 &&
            !info->foregroundAtlas.contains(record.icon)) {
            add("OBJ-ICON-RANGE", Confidence::provenLimit,
                {pattern::iconRange, {num(record.icon)}},
                info->foregroundAtlas.provenance, ObjectField::icon, true);
        }
    }

    const auto capacity = poolCapacity(report.world());
    const auto poolRange = ByteRange{pool->fileOffset(), pool->slotCount() * layout::objectRecordBytes};
    if (capacity.free == 0) {
        report.add("OBJ-POOL-FULL", Confidence::provenLimit,
                   {pattern::poolFull, {num(static_cast<long long>(capacity.stored))}},
                   "docs/research/evidence.md, objects (MoveObjectFree)", poolRange, false);
    } else if (info != nullptr &&
               capacity.free < static_cast<std::size_t>(info->lowHeadroomFreeSlots)) {
        report.add("OBJ-LOW-HEADROOM", Confidence::estimate,
                   {pattern::lowHeadroom,
                    {num(static_cast<long long>(capacity.free)),
                     num(info->lowHeadroomFreeSlots)}},
                   info->headroomRationale, poolRange, true);
    }
    if (stale != 0) {
        report.add("OBJ-FREE-STALE", Confidence::structural,
                   {pattern::freeStale, {num(static_cast<long long>(stale))}},
                   "docs/research/evidence.md, objects (MoveObjectDelete)",
                   *pool->slotRange(*firstStale), false)
            .slot = *firstStale;
    }
}

} // namespace

std::size_t DoctorReport::count(Severity severity) const {
    return static_cast<std::size_t>(std::ranges::count_if(
        findings, [&](const Finding &finding) { return finding.severity == severity; }));
}

std::size_t DoctorReport::countRule(std::string_view ruleId) const {
    return static_cast<std::size_t>(std::ranges::count_if(
        findings, [&](const Finding &finding) { return finding.ruleId == ruleId; }));
}

const Finding *DoctorReport::first(std::string_view ruleId) const {
    for (const auto &finding : findings) {
        if (finding.ruleId == ruleId) {
            return &finding;
        }
    }
    return nullptr;
}

void checkReachability(Report &report) {
    const auto estimate = estimateReachability(report.world(), report.profile());
    if (!estimate.made) {
        return;
    }
    const auto objects = report.world().objects();
    for (const auto slot : estimate.cutOffSlots) {
        const auto record = *objects->record(slot);
        auto &finding = report.add(
            "REACH-CUT-OFF", Confidence::estimate,
            {record.type == 7 ? pattern::cutOffGoal : pattern::cutOffTreasure,
             {num(static_cast<long long>(slot)), num(record.posStart.x),
              num(record.posStart.y)}},
            "docs/research/evidence.md, reachability", *objects->slotRange(slot), true);
        finding.slot = slot;
    }
}

DoctorReport runDoctor(const WorldView &world, GameProfile profile) {
    Report report(world, profile);
    checkStructure(report);
    checkHeader(report);
    checkLayer(report, SectionKind::foreground);
    checkLayer(report, SectionKind::bigDecor);
    checkBorders(report);
    checkObjects(report);
    checkReachability(report);
    return report.take();
}

std::span<const RuleInfo> doctorRules() {
    return rules;
}

std::span<const std::string_view> doctorMessagePatterns() {
    return patterns;
}

std::string fillPattern(std::string_view pattern, std::span<const std::string> arguments) {
    std::string text;
    text.reserve(pattern.size() + 16);
    for (std::size_t index = 0; index < pattern.size(); ++index) {
        const auto digit = index + 1 < pattern.size() ? pattern[index + 1] : '\0';
        if (pattern[index] == '%' && digit >= '1' && digit <= '9') {
            const auto number = static_cast<std::size_t>(digit - '1');
            if (number < arguments.size()) {
                text += arguments[number];
            }
            ++index;
        } else {
            text += pattern[index];
        }
    }
    return text;
}

std::string_view severityName(Severity severity) {
    switch (severity) {
    case Severity::note:
        return "note";
    case Severity::warning:
        return "warning";
    case Severity::error:
        return "error";
    }
    return {};
}

std::string_view confidenceName(Confidence confidence) {
    switch (confidence) {
    case Confidence::structural:
        return "structural fact";
    case Confidence::provenLimit:
        return "read in the game's source";
    case Confidence::verifiedRange:
        return "outside the game's verified range";
    case Confidence::reported:
        return "reported, not re-checked";
    case Confidence::estimate:
        return "estimate";
    }
    return {};
}

} // namespace eggbert
