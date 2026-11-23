// SPDX-License-Identifier: GPL-3.0-only
#include "cli.hpp"

#include <QDir>
#include <QSet>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

#include <algorithm>
#include <optional>

#include "app/document_file.hpp"
#include "app/game_assets.hpp"
#include "app/png_export.hpp"
#include "app/probe_report.hpp"
#include "app/svg_export.hpp"
#include "world/doctor.hpp"
#include "world/game_convert.hpp"
#include "world/hex_model.hpp"
#include "world/merge.hpp"
#include "world/new_world.hpp"
#include "world/offset_map.hpp"
#include "world/demo_recording.hpp"
#include "world/revision_convert.hpp"
#include "world/scenario.hpp"
#include "world/text_world.hpp"

namespace eggbert {

namespace {

constexpr int exitOk = 0;
constexpr int exitFound = 1;
constexpr int exitUsage = 2;

std::optional<GameProfile> parseGame(const QString &value) {
    if (value == QLatin1String("1")) {
        return GameProfile::speedyBlupi1;
    }
    if (value == QLatin1String("2")) {
        return GameProfile::speedyBlupi2;
    }
    return std::nullopt;
}

// The game a loaded world is read under. A Mobile Eggbert text world is
// always that game; --game chooses between the Windows games for the rest.
std::optional<GameProfile> gameOf(const DocumentFile &file, const QString &option,
                                  QTextStream &err) {
    if (file.isTextWorld()) {
        if (!option.isNull()) {
            err << file.path() << ": a Mobile Eggbert text world takes no --game\n";
            return std::nullopt;
        }
        return GameProfile::mobileEggbert;
    }
    if (option.isNull()) {
        return GameProfile::unknown;
    }
    const auto game = parseGame(option);
    if (!game) {
        err << "eggbert-editor: --game takes 1 or 2\n";
    }
    return game;
}

// Splits arguments into positional ones and options; an option that takes a
// value consumes the next argument. Unknown options are an error.
struct Parsed {
    QStringList positional;
    QMap<QString, QString> values;
    QSet<QString> flags;
    QString error;
};

Parsed parse(const QStringList &arguments, const QStringList &valueOptions,
             const QStringList &flagOptions) {
    Parsed parsed;
    for (qsizetype index = 0; index < arguments.size(); ++index) {
        const auto &argument = arguments[index];
        if (!argument.startsWith(QLatin1String("--"))) {
            parsed.positional << argument;
        } else if (valueOptions.contains(argument)) {
            if (index + 1 >= arguments.size()) {
                parsed.error = QStringLiteral("%1 needs a value").arg(argument);
                return parsed;
            }
            parsed.values.insert(argument, arguments[++index]);
        } else if (flagOptions.contains(argument)) {
            parsed.flags.insert(argument);
        } else {
            parsed.error = QStringLiteral("unknown option %1").arg(argument);
            return parsed;
        }
    }
    return parsed;
}

// Set by usageError: export-batch tells a wrong option (exit 2, once) from
// a world that cannot be drawn (the batch goes on, exit 1).
bool usageErrorReported = false;

int usageError(QTextStream &err, const QString &message) {
    usageErrorReported = true;
    err << "eggbert-editor: " << message << "\n" << commandLineUsage();
    return exitUsage;
}

std::optional<QByteArray> readFile(const QString &path);

int info(const QStringList &arguments, QTextStream &out, QTextStream &err) {
    const auto parsed = parse(arguments, {}, {QStringLiteral("--events")});
    if (!parsed.error.isEmpty() || parsed.positional.size() != 1) {
        return usageError(err, parsed.error.isEmpty() ? QStringLiteral("info needs one file")
                                                      : parsed.error);
    }
    // A Mobile Eggbert text world is another kind of file: described, never
    // treated as BLP.
    if (const auto bytes = readFile(parsed.positional[0]);
        bytes && bytes->size() <= 4 * 1024 * 1024 &&
        looksLikeTextWorld(std::string_view(bytes->constData(),
                                            static_cast<std::size_t>(bytes->size())))) {
        out << QFileInfo(parsed.positional[0]).fileName() << "\n"
            << QString::fromStdString(
                   TextWorld::parse(std::string_view(bytes->constData(),
                                                     static_cast<std::size_t>(bytes->size())))
                       .describe());
        return exitOk;
    }
    // So is a demo recording of the games, which starts with the same four
    // bytes as a revision 1.0 world.
    if (const auto bytes = readFile(parsed.positional[0]); bytes) {
        const std::span<const std::byte> raw(
            reinterpret_cast<const std::byte *>(bytes->constData()),
            static_cast<std::size_t>(bytes->size()));
        if (looksLikeDemoRecording(raw)) {
            const auto recording = *DemoRecording::parse(raw);
            out << QFileInfo(parsed.positional[0]).fileName() << "\n"
                << bytes->size() << " bytes\n"
                << QString::fromStdString(recording.describe());
            if (parsed.flags.contains(QStringLiteral("--events"))) {
                for (const auto &event : recording.events) {
                    out << "step " << event.time << ": "
                        << QString::fromStdString(demoInputName(event.input)) << "\n";
                }
            }
            return exitOk;
        }
    }
    auto loaded = DocumentFile::load(parsed.positional[0]);
    if (!loaded) {
        err << parsed.positional[0] << ": " << loaded.error().detail << "\n";
        return exitUsage;
    }
    const auto &document = loaded.value()->document();
    out << formatProbeReport(QFileInfo(parsed.positional[0]).fileName(), document.probe()) << "\n";
    out << formatWorldSummary(document.view()) << "\n";
    return exitOk;
}

int validate(const QStringList &arguments, QTextStream &out, QTextStream &err) {
    const auto parsed = parse(arguments, {QStringLiteral("--game")}, {});
    if (!parsed.error.isEmpty() || parsed.positional.size() != 1) {
        return usageError(err, parsed.error.isEmpty() ? QStringLiteral("validate needs one file")
                                                      : parsed.error);
    }
    if (parsed.values.contains(QStringLiteral("--game")) &&
        !parseGame(parsed.values[QStringLiteral("--game")])) {
        return usageError(err, QStringLiteral("--game takes 1 or 2"));
    }
    auto loaded = DocumentFile::load(parsed.positional[0]);
    if (!loaded) {
        err << parsed.positional[0] << ": " << loaded.error().detail << "\n";
        return exitUsage;
    }
    const auto game = gameOf(*loaded.value(), parsed.values.value(QStringLiteral("--game")), err);
    if (!game) {
        return exitUsage;
    }
    const auto profile = *game;
    const auto report = runDoctor(loaded.value()->document().view(), profile);
    for (const auto &finding : report.findings) {
        out << "[" << QString::fromUtf8(severityName(finding.severity).data()) << "] "
            << QString::fromUtf8(finding.ruleId.data(),
                                 static_cast<qsizetype>(finding.ruleId.size()))
            << " @" << finding.range.offset << "+" << finding.range.size << ": "
            << QString::fromStdString(finding.message) << "\n";
    }
    out << report.count(Severity::error) << " error(s), " << report.count(Severity::warning)
        << " warning(s), " << report.count(Severity::note) << " note(s)\n";
    return report.count(Severity::error) == 0 ? exitOk : exitFound;
}

std::optional<QByteArray> readFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > DocumentFile::maximumBytes) {
        return std::nullopt;
    }
    return file.readAll();
}

int diff(const QStringList &arguments, QTextStream &out, QTextStream &err) {
    const auto parsed = parse(arguments, {}, {});
    if (!parsed.error.isEmpty() || parsed.positional.size() != 2) {
        return usageError(err, parsed.error.isEmpty() ? QStringLiteral("diff needs two files")
                                                      : parsed.error);
    }
    const auto left = readFile(parsed.positional[0]);
    const auto right = readFile(parsed.positional[1]);
    if (!left || !right) {
        err << (left ? parsed.positional[1] : parsed.positional[0]) << ": cannot be read\n";
        return exitUsage;
    }
    const auto bytes = [](const QByteArray &data) {
        return std::span<const std::byte>(reinterpret_cast<const std::byte *>(data.constData()),
                                          static_cast<std::size_t>(data.size()));
    };
    const auto common = std::min(left->size(), right->size());
    const auto ranges = differingRanges(bytes(*left).first(static_cast<std::size_t>(common)),
                                        bytes(*right).first(static_cast<std::size_t>(common)));
    // Offsets are named by the first file's layout.
    const auto probe = probeWorld(bytes(*left));
    std::size_t total = 0;
    const std::size_t listLimit = 500;
    std::size_t listed = 0;
    for (const auto &range : ranges) {
        total += range.size;
        if (listed++ >= listLimit) {
            continue;
        }
        const auto shown = std::min<std::size_t>(range.size, 16);
        out << "offset " << range.offset << ", " << range.size << " byte(s), "
            << QString::fromStdString(offsetLabel(probe, range.offset)) << ": "
            << QString::fromStdString(formatHex(bytes(*left).subspan(range.offset, shown)))
            << (shown < range.size ? " ..." : "") << " -> "
            << QString::fromStdString(formatHex(bytes(*right).subspan(range.offset, shown)))
            << (shown < range.size ? " ..." : "") << "\n";
    }
    if (ranges.size() > listLimit) {
        out << "... and " << ranges.size() - listLimit << " more range(s)\n";
    }
    if (left->size() != right->size()) {
        out << "sizes differ: " << left->size() << " and " << right->size() << " bytes\n";
    }
    const bool equal = ranges.empty() && left->size() == right->size();
    out << (equal ? QStringLiteral("identical\n")
                  : QStringLiteral("%1 byte(s) in %2 range(s) differ\n").arg(total).arg(ranges.size()));
    return equal ? exitOk : exitFound;
}

std::shared_ptr<const GameAssets> exportArtwork(const QString &directory, const DocumentFile &file,
                                                GameProfile &profile, QTextStream &err) {
    const auto found = detectGameProfile(directory).profile;
    // The pictures of the port are laid out for its text worlds only,
    // and those of the Windows games for theirs.
    if (found != GameProfile::unknown &&
        (found == GameProfile::mobileEggbert) != file.isTextWorld()) {
        err << directory << ": "
            << (file.isTextWorld()
                    ? "a Mobile Eggbert text world is drawn with the pictures of Mobile "
                      "Eggbert\n"
                    : "the pictures of Mobile Eggbert are for its text worlds\n");
        return nullptr;
    }
    if (profile == GameProfile::unknown) {
        profile = found;
    }
    if (profile == GameProfile::unknown) {
        err << directory << ": the game was not recognised; say --game 1 or --game 2\n";
        return nullptr;
    }
    const auto assets = GameAssets::load(directory, profile);
    if (!assets->usable()) {
        err << directory << ": its tiles cannot be drawn\n";
        for (const auto &line : assets->report()) {
            err << "  " << line << "\n";
        }
        return nullptr;
    }
    return assets;
}

int exportPng(const QStringList &arguments, QTextStream &out, QTextStream &err) {
    const QStringList off{QStringLiteral("--no-foreground"), QStringLiteral("--no-bigdecor"),
                          QStringLiteral("--no-objects"),    QStringLiteral("--no-starts"),
                          QStringLiteral("--no-paths"),      QStringLiteral("--no-backdrop")};
    const QStringList on{QStringLiteral("--grid"), QStringLiteral("--labels"),
                         QStringLiteral("--transparent")};
    const auto parsed = parse(arguments,
                              {QStringLiteral("--scale"), QStringLiteral("--crop"),
                               QStringLiteral("--game"), QStringLiteral("--game-data")},
                              off + on);
    if (!parsed.error.isEmpty() || parsed.positional.size() != 2) {
        return usageError(err, parsed.error.isEmpty()
                                   ? QStringLiteral("export-png needs a world file and an output file")
                                   : parsed.error);
    }
    if (sameWorldPath(parsed.positional[0], parsed.positional[1])) {
        return usageError(err, QStringLiteral("An image cannot replace its source world."));
    }
    PngExportOptions options;
    options.render.showLabels = parsed.flags.contains(QStringLiteral("--labels"));
    options.render.showGrid = parsed.flags.contains(QStringLiteral("--grid"));
    options.render.transparentBackground = parsed.flags.contains(QStringLiteral("--transparent"));
    options.render.showForeground = !parsed.flags.contains(QStringLiteral("--no-foreground"));
    options.render.showBigDecor = !parsed.flags.contains(QStringLiteral("--no-bigdecor"));
    options.render.showObjects = !parsed.flags.contains(QStringLiteral("--no-objects"));
    options.render.showStarts = !parsed.flags.contains(QStringLiteral("--no-starts"));
    options.render.showPaths = !parsed.flags.contains(QStringLiteral("--no-paths"));
    options.render.showBackdrop = !parsed.flags.contains(QStringLiteral("--no-backdrop"));
    if (parsed.values.contains(QStringLiteral("--scale"))) {
        bool ok = false;
        options.pixelsPerCell = parsed.values[QStringLiteral("--scale")].toInt(&ok);
        if (!ok || options.pixelsPerCell < 1 || options.pixelsPerCell > 64) {
            return usageError(err, QStringLiteral("--scale takes pixels per cell, 1 to 64"));
        }
    }
    if (parsed.values.contains(QStringLiteral("--crop"))) {
        const auto parts = parsed.values[QStringLiteral("--crop")].split(QLatin1Char(','));
        bool ok = parts.size() == 4;
        std::array<int, 4> numbers{};
        for (qsizetype index = 0; ok && index < 4; ++index) {
            numbers[static_cast<std::size_t>(index)] = parts[index].toInt(&ok);
        }
        if (!ok || numbers[0] < 0 || numbers[1] < 0 || numbers[2] < 1 || numbers[3] < 1 ||
            numbers[0] > 99 || numbers[1] > 99 || numbers[0] + numbers[2] > 100 ||
            numbers[1] + numbers[3] > 100) {
            return usageError(err, QStringLiteral("--crop takes x,y,width,height in cells inside "
                                                  "the 100 x 100 map"));
        }
        options.crop = CellRect{{static_cast<std::size_t>(numbers[0]),
                                 static_cast<std::size_t>(numbers[1])},
                                {static_cast<std::size_t>(numbers[0] + numbers[2] - 1),
                                 static_cast<std::size_t>(numbers[1] + numbers[3] - 1)}};
    }
    if (parsed.values.contains(QStringLiteral("--game")) &&
        !parseGame(parsed.values[QStringLiteral("--game")])) {
        return usageError(err, QStringLiteral("--game takes 1 or 2"));
    }
    auto loaded = DocumentFile::load(parsed.positional[0]);
    if (!loaded) {
        err << parsed.positional[0] << ": " << loaded.error().detail << "\n";
        return exitUsage;
    }
    const auto game = gameOf(*loaded.value(), parsed.values.value(QStringLiteral("--game")), err);
    if (!game) {
        return exitUsage;
    }
    auto profile = *game;
    if (parsed.values.contains(QStringLiteral("--game-data"))) {
        const auto directory = parsed.values[QStringLiteral("--game-data")];
        options.render.artwork = exportArtwork(directory, *loaded.value(), profile, err);
        if (!options.render.artwork) {
            return exitUsage;
        }
    }
    options.render.profile = profile;
    const auto result =
        exportWorldPng(loaded.value()->document().view(), profile, options, parsed.positional[1]);
    if (!result.ok) {
        err << parsed.positional[1] << ": " << result.error << "\n";
        return exitUsage;
    }
    out << parsed.positional[1] << ": " << result.width << " x " << result.height << " pixels\n";
    return exitOk;
}

bool writeNewFile(const QString &path, const std::vector<std::byte> &bytes) {
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    const auto size = static_cast<qint64>(bytes.size());
    return file.open(QIODevice::WriteOnly) &&
           file.write(reinterpret_cast<const char *>(bytes.data()), size) == size &&
           file.commit();
}

int create(const QStringList &arguments, QTextStream &out, QTextStream &err) {
    const auto parsed = parse(arguments, {QStringLiteral("--game")}, {});
    auto game = GameProfile::unknown;
    if (parsed.values.contains(QStringLiteral("--game"))) {
        const auto wanted = parsed.values[QStringLiteral("--game")];
        game = wanted == QLatin1String("mobile")
                   ? GameProfile::mobileEggbert
                   : parseGame(wanted).value_or(GameProfile::unknown);
    }
    if (!parsed.error.isEmpty() || parsed.positional.size() != 1 ||
        game == GameProfile::unknown) {
        return usageError(err,
                          parsed.error.isEmpty()
                              ? QStringLiteral("new needs an output file and --game 1|2|mobile")
                              : parsed.error);
    }
    const auto &path = parsed.positional[0];
    if (QFileInfo::exists(path)) {
        err << path << ": already exists; new never overwrites a file\n";
        return exitUsage;
    }
    if (game == GameProfile::mobileEggbert) {
        const auto text = newTextWorld();
        const auto *data = reinterpret_cast<const std::byte *>(text.data());
        if (!writeNewFile(path, std::vector<std::byte>(data, data + text.size()))) {
            err << path << ": cannot be written\n";
            return exitUsage;
        }
        out << path << ": new Mobile Eggbert text world, " << text.size() << " bytes\n";
        return exitOk;
    }
    const auto bytes = newWorld(game);
    if (!writeNewFile(path, bytes)) {
        err << path << ": cannot be written\n";
        return exitUsage;
    }
    const auto &revision = *findRevision(newWorldRevision(game));
    out << path << ": new world, revision " << revision.major << "." << revision.minor << ", "
        << bytes.size() << " bytes\n";
    return exitOk;
}

int convert(const QStringList &arguments, QTextStream &out, QTextStream &err) {
    const auto parsed = parse(arguments, {QStringLiteral("--to")}, {QStringLiteral("--allow-loss")});
    if (!parsed.error.isEmpty() || parsed.positional.size() != 2 ||
        !parsed.values.contains(QStringLiteral("--to"))) {
        return usageError(err, parsed.error.isEmpty()
                                   ? QStringLiteral("convert needs a world file, an output file "
                                                    "and --to 1.0|1.1|1.2|1.3")
                                   : parsed.error);
    }
    const auto wanted = parsed.values[QStringLiteral("--to")];
    auto target = FormatRevision::unknown;
    for (const auto &revision : knownRevisions) {
        if (wanted == QStringLiteral("%1.%2").arg(revision.major).arg(revision.minor)) {
            target = revision.revision;
        }
    }
    if (target == FormatRevision::unknown) {
        return usageError(err, QStringLiteral("--to takes 1.0, 1.1, 1.2 or 1.3"));
    }
    const auto source = readFile(parsed.positional[0]);
    if (!source) {
        err << parsed.positional[0] << ": cannot be read\n";
        return exitUsage;
    }
    if (QFileInfo::exists(parsed.positional[1])) {
        err << parsed.positional[1] << ": already exists; convert never overwrites a file\n";
        return exitUsage;
    }
    const auto converted = convertRevision(
        std::span<const std::byte>(reinterpret_cast<const std::byte *>(source->constData()),
                                   static_cast<std::size_t>(source->size())),
        target, parsed.flags.contains(QStringLiteral("--allow-loss")));
    if (!converted) {
        err << parsed.positional[0] << ": " << QString::fromStdString(converted.error().detail)
            << (converted.error().error == ConversionError::wouldLoseData
                    ? " Use --allow-loss to convert anyway.\n"
                    : "\n");
        return converted.error().error == ConversionError::wouldLoseData ? exitFound : exitUsage;
    }
    if (!writeNewFile(parsed.positional[1], converted->bytes)) {
        err << parsed.positional[1] << ": cannot be written\n";
        return exitUsage;
    }
    out << parsed.positional[1] << ": revision " << wanted << ", " << converted->bytes.size()
        << " bytes\n";
    for (const auto &note : converted->notes) {
        out << "  " << QString::fromStdString(note) << "\n";
    }
    return exitOk;
}

QString itemName(GameItem item) {
    switch (item) {
    case GameItem::foregroundTile:
        return QStringLiteral("foreground tile");
    case GameItem::bigDecorTile:
        return QStringLiteral("bigDecor tile");
    case GameItem::objectType:
        return QStringLiteral("object type");
    case GameItem::objectIcon:
        return QStringLiteral("object picture");
    case GameItem::region:
        return QStringLiteral("region");
    }
    return {};
}

} // namespace

QString describeGameNote(const GameConversionNote &note) {
    auto where = QString();
    if (note.firstCell) {
        where = QStringLiteral(", first at cell %1,%2").arg(note.firstCell->x).arg(note.firstCell->y);
    } else if (note.firstSlot) {
        where = QStringLiteral(", first in slot %1").arg(*note.firstSlot);
    }
    return QStringLiteral("[%1] %2 %3 (%4 place(s)%5): %6")
        .arg(note.kind == CarryKind::unknownToTarget ? QStringLiteral("unknown")
                                                     : QStringLiteral("differs"),
             itemName(note.item))
        .arg(note.value)
        .arg(note.count)
        .arg(where, QString::fromStdString(note.text));
}

namespace {

int convertGameCommand(const QStringList &arguments, QTextStream &out, QTextStream &err) {
    const auto parsed = parse(arguments,
                              {QStringLiteral("--from"), QStringLiteral("--to"),
                               QStringLiteral("--unknown")},
                              {});
    const auto game = [&](const QString &option) {
        return parsed.values.contains(option)
                   ? parseGame(parsed.values[option]).value_or(GameProfile::unknown)
                   : GameProfile::unknown;
    };
    const auto from = game(QStringLiteral("--from"));
    const auto to = game(QStringLiteral("--to"));
    auto policy = UnknownPolicy::refuse;
    const auto wanted = parsed.values.value(QStringLiteral("--unknown"), QStringLiteral("refuse"));
    if (wanted == QLatin1String("keep")) {
        policy = UnknownPolicy::keep;
    } else if (wanted == QLatin1String("remove")) {
        policy = UnknownPolicy::remove;
    } else if (wanted != QLatin1String("refuse")) {
        return usageError(err, QStringLiteral("--unknown takes refuse, keep or remove"));
    }
    if (!parsed.error.isEmpty() || parsed.positional.size() != 2 ||
        from == GameProfile::unknown || to == GameProfile::unknown || from == to) {
        return usageError(err, parsed.error.isEmpty()
                                   ? QStringLiteral("convert-game needs a world file, an output "
                                                    "file, --from 1|2 and --to with the other "
                                                    "game")
                                   : parsed.error);
    }
    const auto source = readFile(parsed.positional[0]);
    if (!source) {
        err << parsed.positional[0] << ": cannot be read\n";
        return exitUsage;
    }
    if (QFileInfo::exists(parsed.positional[1])) {
        err << parsed.positional[1] << ": already exists; convert-game never overwrites a file\n";
        return exitUsage;
    }
    const std::span<const std::byte> bytes(
        reinterpret_cast<const std::byte *>(source->constData()),
        static_cast<std::size_t>(source->size()));
    const auto converted = convertGame(bytes, from, to, policy);
    if (!converted) {
        err << parsed.positional[0] << ": " << QString::fromStdString(converted.error().detail)
            << "\n";
        if (converted.error().error != ConversionError::wouldLoseData) {
            return exitUsage;
        }
        // Say what they are, so the choice between keep and remove is informed.
        if (const auto kept = convertGame(bytes, from, to, UnknownPolicy::keep)) {
            for (const auto &note : kept->notes) {
                err << "  " << describeGameNote(note) << "\n";
            }
        }
        err << "Use --unknown keep to leave them stored or --unknown remove to clear them.\n";
        return exitFound;
    }
    if (!writeNewFile(parsed.positional[1], converted->bytes)) {
        err << parsed.positional[1] << ": cannot be written\n";
        return exitUsage;
    }
    out << parsed.positional[1] << ": for game " << (to == GameProfile::speedyBlupi1 ? 1 : 2)
        << ", " << converted->bytes.size() << " bytes";
    if (converted->removedCount != 0) {
        out << ", " << converted->removedCount << " place(s) cleared";
    }
    out << "\n";
    for (const auto &note : converted->revisionNotes) {
        out << "  " << QString::fromStdString(note) << "\n";
    }
    for (const auto &note : converted->notes) {
        out << "  " << describeGameNote(note) << "\n";
    }
    return exitOk;
}

// The facts of a world file for the scenario commands; the game profile
// matters for what World Doctor reports.
std::optional<std::vector<WorldFact>> scenarioFacts(const Parsed &parsed, QTextStream &err,
                                                    int &code) {
    if (parsed.values.contains(QStringLiteral("--game")) &&
        !parseGame(parsed.values[QStringLiteral("--game")])) {
        code = usageError(err, QStringLiteral("--game takes 1 or 2"));
        return std::nullopt;
    }
    auto loaded = DocumentFile::load(parsed.positional[0]);
    if (!loaded) {
        err << parsed.positional[0] << ": " << loaded.error().detail << "\n";
        code = exitUsage;
        return std::nullopt;
    }
    const auto game = gameOf(*loaded.value(), parsed.values.value(QStringLiteral("--game")), err);
    if (!game) {
        code = exitUsage;
        return std::nullopt;
    }
    return worldFacts(loaded.value()->document().view(), *game);
}

int scenarioRecord(const QStringList &arguments, QTextStream &out, QTextStream &err) {
    const auto parsed = parse(arguments, {QStringLiteral("--game")}, {});
    if (!parsed.error.isEmpty() || parsed.positional.size() != 2) {
        return usageError(err, parsed.error.isEmpty()
                                   ? QStringLiteral("scenario-record needs a world file and an "
                                                    "output file")
                                   : parsed.error);
    }
    const auto &path = parsed.positional[1];
    if (QFileInfo::exists(path)) {
        err << path << ": already exists; scenario-record never overwrites a file\n";
        return exitUsage;
    }
    int code = exitUsage;
    const auto facts = scenarioFacts(parsed, err, code);
    if (!facts) {
        return code;
    }
    const auto text = recordScenario(*facts);
    const auto *data = reinterpret_cast<const std::byte *>(text.data());
    if (!writeNewFile(path, std::vector<std::byte>(data, data + text.size()))) {
        err << path << ": cannot be written\n";
        return exitUsage;
    }
    out << path << ": " << facts->size() << " expectation(s) recorded\n";
    return exitOk;
}

int scenarioCheck(const QStringList &arguments, QTextStream &out, QTextStream &err) {
    const auto parsed = parse(arguments, {QStringLiteral("--game")}, {});
    if (!parsed.error.isEmpty() || parsed.positional.size() != 2) {
        return usageError(err, parsed.error.isEmpty()
                                   ? QStringLiteral("scenario-check needs a world file and a "
                                                    "scenario file")
                                   : parsed.error);
    }
    const auto scenario = readFile(parsed.positional[1]);
    if (!scenario) {
        err << parsed.positional[1] << ": cannot be read\n";
        return exitUsage;
    }
    int code = exitUsage;
    const auto facts = scenarioFacts(parsed, err, code);
    if (!facts) {
        return code;
    }
    const auto result = checkScenario(
        std::string_view(scenario->constData(), static_cast<std::size_t>(scenario->size())),
        *facts);
    if (!result) {
        err << parsed.positional[1] << ": " << QString::fromStdString(result.error()) << "\n";
        return exitUsage;
    }
    for (const auto &failure : result->failures) {
        out << QString::fromStdString(failure) << "\n";
    }
    out << result->checked << " expectation(s) checked, " << result->failures.size()
        << " failed\n";
    return result->failures.empty() ? exitOk : exitFound;
}

int exportSvg(const QStringList &arguments, QTextStream &out, QTextStream &err) {
    const QStringList off{QStringLiteral("--no-foreground"), QStringLiteral("--no-bigdecor"),
                          QStringLiteral("--no-objects"), QStringLiteral("--no-starts"),
                          QStringLiteral("--schematic")};
    const auto parsed = parse(arguments, {QStringLiteral("--scale"), QStringLiteral("--game"),
                                          QStringLiteral("--game-data")}, off);
    if (!parsed.error.isEmpty() || parsed.positional.size() != 2) {
        return usageError(err, parsed.error.isEmpty()
                                   ? QStringLiteral("export-svg needs a world file and an output "
                                                    "file")
                                   : parsed.error);
    }
    if (sameWorldPath(parsed.positional[0], parsed.positional[1])) {
        return usageError(err, QStringLiteral("An image cannot replace its source world."));
    }
    SvgExportOptions options;
    if (parsed.values.contains(QStringLiteral("--scale"))) {
        bool ok = false;
        options.cellSize = parsed.values[QStringLiteral("--scale")].toInt(&ok);
        if (!ok || options.cellSize < 1 || options.cellSize > 64) {
            return usageError(err, QStringLiteral("--scale takes 1 to 64"));
        }
    }
    options.showForeground = !parsed.flags.contains(QStringLiteral("--no-foreground"));
    options.showBigDecor = !parsed.flags.contains(QStringLiteral("--no-bigdecor"));
    options.showObjects = !parsed.flags.contains(QStringLiteral("--no-objects"));
    options.showStarts = !parsed.flags.contains(QStringLiteral("--no-starts"));
    auto loaded = DocumentFile::load(parsed.positional[0]);
    if (!loaded) {
        err << parsed.positional[0] << ": " << loaded.error().detail << "\n";
        return exitUsage;
    }
    const auto &documentFile = *loaded.value();
    if (!documentFile.document().view().foreground()) {
        return usageError(err, QStringLiteral("The world has no complete tile layer."));
    }
    auto profile = gameOf(documentFile, parsed.values.value(QStringLiteral("--game")), err);
    if (!profile) {
        return exitUsage;
    }
    QString directory = parsed.values.value(QStringLiteral("--game-data"));
    if (parsed.flags.contains(QStringLiteral("--schematic"))) {
        if (!directory.isNull()) {
            return usageError(err, QStringLiteral("Choose either --schematic or --game-data."));
        }
    } else {
        if (directory.isNull()) {
            auto folder = QFileInfo(documentFile.path()).dir();
            if (documentFile.isTextWorld()) {
                for (int level = 0; level < 3; ++level) {
                    if (!GameAssets::mobileContentDirectory(folder.absolutePath()).isEmpty()) {
                        directory = folder.absolutePath();
                        break;
                    }
                    if (!folder.cdUp()) {
                        break;
                    }
                }
            } else if (folder.dirName().compare(QLatin1String("DATA"), Qt::CaseInsensitive) == 0 &&
                       folder.cdUp()) {
                directory = folder.absolutePath();
            }
        }
        if (directory.isEmpty()) {
            return usageError(err, QStringLiteral("Supply the installation with --game-data DIR "
                                                  "or use --schematic for a diagram."));
        }
        options.artwork = exportArtwork(directory, documentFile, *profile, err);
        if (!options.artwork) {
            return exitUsage;
        }
    }
    const auto svg = renderWorldSvg(loaded.value()->document().view(), options);
    if (svg.isEmpty()) {
        err << parsed.positional[0] << ": could not render the SVG\n";
        return exitUsage;
    }
    QSaveFile file(parsed.positional[1]);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(svg) != svg.size() || !file.commit()) {
        err << parsed.positional[1] << ": cannot be written\n";
        return exitUsage;
    }
    out << parsed.positional[1] << (options.artwork ? ": artwork SVG, " : ": schematic SVG, ") << svg.size() << " bytes\n";
    return exitOk;
}

// export-png for several worlds: OUTDIR, then the files; every option of
// export-png applies to each.
int exportBatch(const QStringList &arguments, QTextStream &out, QTextStream &err) {
    QStringList options;
    QStringList positional;
    static const QStringList withValue{QStringLiteral("--scale"), QStringLiteral("--crop"),
                                       QStringLiteral("--game"), QStringLiteral("--game-data")};
    for (qsizetype index = 0; index < arguments.size(); ++index) {
        if (arguments[index].startsWith(QLatin1String("--"))) {
            options << arguments[index];
            if (withValue.contains(arguments[index]) && index + 1 < arguments.size()) {
                options << arguments[++index];
            }
        } else {
            positional << arguments[index];
        }
    }
    if (positional.size() < 2) {
        return usageError(err, QStringLiteral("export-batch needs an output directory and at "
                                              "least one world file"));
    }
    const QDir directory(positional.takeFirst());
    if (!directory.exists()) {
        err << directory.path() << ": not an existing directory\n";
        return exitUsage;
    }
    // Two inputs must not write the same picture.
    QSet<QString> names;
    for (const auto &path : positional) {
        const auto name = QFileInfo(path).completeBaseName().toLower();
        if (names.contains(name)) {
            err << path << ": another input has the same name; nothing was written\n";
            return exitUsage;
        }
        const auto target = directory.filePath(QFileInfo(path).completeBaseName() + QStringLiteral(".png"));
        for (const auto &source : positional) {
            if (sameWorldPath(source, target)) {
                return usageError(err, QStringLiteral("An image cannot replace an input world."));
            }
        }
        names.insert(name);
    }
    int failed = 0;
    for (const auto &path : positional) {
        const auto target =
            directory.filePath(QFileInfo(path).completeBaseName() + QStringLiteral(".png"));
        usageErrorReported = false;
        if (exportPng(QStringList{path, target} + options, out, err) != exitOk) {
            if (usageErrorReported) {
                // The options are the same for every world: nothing to go on with.
                return exitUsage;
            }
            ++failed;
        }
    }
    out << positional.size() - failed << " of " << positional.size() << " picture(s) written to "
        << directory.path() << "\n";
    return failed == 0 ? exitOk : exitFound;
}

int merge(const QStringList &arguments, QTextStream &out, QTextStream &err) {
    const auto parsed = parse(arguments, {QStringLiteral("--prefer")}, {});
    auto preference = MergePreference::none;
    const auto wanted = parsed.values.value(QStringLiteral("--prefer"));
    if (wanted == QLatin1String("mine")) {
        preference = MergePreference::mine;
    } else if (wanted == QLatin1String("theirs")) {
        preference = MergePreference::theirs;
    } else if (!wanted.isEmpty()) {
        return usageError(err, QStringLiteral("--prefer takes mine or theirs"));
    }
    if (!parsed.error.isEmpty() || parsed.positional.size() != 4) {
        return usageError(err, parsed.error.isEmpty()
                                   ? QStringLiteral("merge needs BASE, MINE, THEIRS and an "
                                                    "output file")
                                   : parsed.error);
    }
    std::optional<QByteArray> files[3];
    for (int index = 0; index < 3; ++index) {
        files[index] = readFile(parsed.positional[index]);
        if (!files[index]) {
            err << parsed.positional[index] << ": cannot be read\n";
            return exitUsage;
        }
    }
    const auto bytes = [](const QByteArray &file) {
        return std::span<const std::byte>(reinterpret_cast<const std::byte *>(file.constData()),
                                          static_cast<std::size_t>(file.size()));
    };
    const auto &target = parsed.positional[3];
    if (QFileInfo::exists(target)) {
        err << target << ": already exists; merge never overwrites a file\n";
        return exitUsage;
    }
    // Mobile Eggbert text worlds are merged value by value like the rest,
    // on the layout the editor holds them in, and the result is MINE's text
    // with the merged values.
    std::unique_ptr<DocumentFile> textMine;
    std::vector<std::byte> images[3];
    std::span<const std::byte> sides[3] = {bytes(*files[0]), bytes(*files[1]), bytes(*files[2])};
    if (looksLikeTextWorld(std::string_view(files[1]->constData(),
                                            static_cast<std::size_t>(files[1]->size())))) {
        auto loaded = DocumentFile::load(parsed.positional[1]);
        if (!loaded) {
            err << parsed.positional[1] << ": " << loaded.error().detail << "\n";
            return exitUsage;
        }
        textMine = std::move(loaded.value());
        for (int index = 0; index < 3; ++index) {
            auto image = textMine->documentBytesOf(*files[index]);
            if (!image) {
                err << parsed.positional[index] << ": " << image.error() << "\n";
                return exitUsage;
            }
            images[index] = std::move(*image);
            sides[index] = images[index];
        }
    }
    const auto merged = mergeWorlds(sides[0], sides[1], sides[2], preference);
    if (!merged) {
        err << QString::fromStdString(merged.error()) << "\n";
        return exitUsage;
    }
    const auto probe = probeWorld(sides[0]);
    auto &list = merged->conflicts.empty() || preference != MergePreference::none ? out : err;
    for (const auto &conflict : merged->conflicts) {
        const auto shown = std::min<std::size_t>(conflict.range.size, 16);
        const auto hex = [&](std::span<const std::byte> side) {
            return QString::fromStdString(
                       formatHex(side.subspan(conflict.range.offset, shown))) +
                   (shown < conflict.range.size ? QStringLiteral(" ...") : QString());
        };
        list << "conflict at offset " << conflict.range.offset << ", " << conflict.range.size
             << " byte(s), " << QString::fromStdString(offsetLabel(probe, conflict.range.offset))
             << ": base " << hex(sides[0]) << " | mine " << hex(sides[1]) << " | theirs "
             << hex(sides[2]) << "\n";
    }
    if (!merged->conflicts.empty() && preference == MergePreference::none) {
        err << merged->conflicts.size()
            << " conflict(s); nothing was written. Use --prefer mine or --prefer theirs to "
               "take one side for them.\n";
        return exitFound;
    }
    if (textMine) {
        auto wanted = merged->bytes;
        mirrorTextWorldStart(wanted);
        // What the editor preserves keeps MINE's values, as in its window.
        for (const auto &range : textMine->document().protectedRanges()) {
            std::copy_n(sides[1].begin() + static_cast<std::ptrdiff_t>(range.offset), range.size,
                        wanted.begin() + static_cast<std::ptrdiff_t>(range.offset));
        }
        EditBuilder builder(textMine->document());
        for (const auto &range : differingRanges(sides[1], wanted)) {
            builder.setBytes(range.offset,
                             std::span<const std::byte>(wanted).subspan(range.offset, range.size));
        }
        if (builder.hasChanges() && !textMine->document().apply(builder.build("Merge"))) {
            err << target << ": the merged world cannot be written as text\n";
            return exitUsage;
        }
        if (const auto saved = textMine->saveAs(target, false); !saved.ok()) {
            err << target << ": cannot be written"
                << (saved.detail.isEmpty() ? QString() : QStringLiteral(": ") + saved.detail)
                << "\n";
            return exitUsage;
        }
    } else if (!writeNewFile(target, merged->bytes)) {
        err << target << ": cannot be written\n";
        return exitUsage;
    }
    out << target << ": " << merged->fromMine << " element(s) from mine, " << merged->fromTheirs
        << " from theirs, " << merged->agreed << " changed alike, " << merged->conflicts.size()
        << " conflict(s)"
        << (merged->conflicts.empty()
                ? ""
                : preference == MergePreference::mine ? " resolved for mine" : " resolved for theirs")
        << "\n";
    return exitOk;
}

} // namespace

bool isCommandLineCommand(const QString &name) {
    return name == QLatin1String("info") || name == QLatin1String("validate") ||
           name == QLatin1String("diff") || name == QLatin1String("export-png") ||
           name == QLatin1String("convert") || name == QLatin1String("new") ||
           name == QLatin1String("convert-game") || name == QLatin1String("merge") ||
           name == QLatin1String("export-svg") || name == QLatin1String("export-batch") ||
           name == QLatin1String("scenario-record") || name == QLatin1String("scenario-check");
}

bool commandNeedsGraphics(const QString &name) {
    return name == QLatin1String("export-png") || name == QLatin1String("export-batch") ||
           name == QLatin1String("export-svg");
}

QString commandLineUsage() {
    return QStringLiteral(
        "Commands (read-only; no window is opened):\n"
        "  eggbert-editor info FILE [--events]\n"
        "      structural report and decoded summary of a world file; a Mobile\n"
        "      Eggbert text world is described instead, and so is a demo recording of\n"
        "      the games (its keys, with --events every recorded event)\n"
        "  eggbert-editor validate FILE [--game 1|2]\n"
        "      World Doctor findings; exit 1 when there are errors\n"
        "  eggbert-editor diff FILE OTHER\n"
        "      differing byte ranges with their structural names; exit 1 when different\n"
        "  eggbert-editor export-png FILE OUT.png [--scale 1..64] [--crop x,y,w,h]\n"
        "      [--game 1|2] [--game-data DIR] [--no-foreground] [--no-bigdecor]\n"
        "      [--no-objects] [--no-starts] [--no-paths] [--no-backdrop] [--grid]\n"
        "      [--labels] [--transparent]\n"
        "      picture of the world, or of a crop given in cells\n"
        "  eggbert-editor export-batch OUTDIR FILE... [options of export-png]\n"
        "      one picture per world, named after it, in an existing directory; exit 1\n"
        "      when some could not be written\n"
        "  eggbert-editor export-svg FILE OUT.svg [--scale 1..64] [--no-foreground]\n"
        "      [--no-bigdecor] [--no-objects] [--no-starts]\n"
        "      [--game 1|2] [--game-data DIR | --schematic]\n"
        "      embeds native-resolution game artwork or writes a labelled schematic\n"
        "  eggbert-editor convert FILE OUT --to 1.0|1.1|1.2|1.3 [--allow-loss]\n"
        "      a copy of the world in another revision; OUT must not exist; exit 1 when\n"
        "      the target cannot store everything and --allow-loss is not given\n"
        "  eggbert-editor convert-game FILE OUT --from 1|2 --to 2|1\n"
        "      [--unknown refuse|keep|remove]\n"
        "      a copy for the other game, with every value that game does not know or\n"
        "      treats differently listed; OUT must not exist; exit 1 when values the\n"
        "      game does not know exist and neither keep nor remove was chosen\n"
        "  eggbert-editor merge BASE MINE THEIRS OUT [--prefer mine|theirs]\n"
        "      combines the changes two worlds made to a common base, element by\n"
        "      element; exit 1 and nothing written when both changed the same element\n"
        "      differently and no side is preferred\n"
        "  eggbert-editor new OUT --game 1|2|mobile\n"
        "      the world the game's own editor starts with; OUT must not exist; for\n"
        "      mobile a Mobile Eggbert text world\n"
        "  eggbert-editor scenario-record FILE OUT.txt [--game 1|2]\n"
        "      writes what is true of a world now (counts of tiles and objects, header\n"
        "      values, World Doctor findings) as expectations; OUT must not exist\n"
        "  eggbert-editor scenario-check FILE SCENARIO.txt [--game 1|2]\n"
        "      checks a world against recorded expectations; exit 1 when some fail\n"
        "A Mobile Eggbert text world is taken by validate, the export commands,\n"
        "merge and the scenario commands under its own game: --game is not given,\n"
        "--game-data is the port's folder, and merge writes text. convert and\n"
        "convert-game are for .blp worlds only.\n"
        "Exit codes: 0 success, 1 findings, differences, refused loss, merge\n"
        "conflicts, pictures that could not be written or failed expectations,\n"
        "2 usage or file error.\n");
}

int runCommandLine(const QStringList &arguments, QTextStream &out, QTextStream &err) {
    if (arguments.isEmpty() || !isCommandLineCommand(arguments[0])) {
        return usageError(err, QStringLiteral("unknown command"));
    }
    const auto rest = arguments.mid(1);
    int code = exitUsage;
    if (arguments[0] == QLatin1String("info")) {
        code = info(rest, out, err);
    } else if (arguments[0] == QLatin1String("validate")) {
        code = validate(rest, out, err);
    } else if (arguments[0] == QLatin1String("diff")) {
        code = diff(rest, out, err);
    } else if (arguments[0] == QLatin1String("convert")) {
        code = convert(rest, out, err);
    } else if (arguments[0] == QLatin1String("new")) {
        code = create(rest, out, err);
    } else if (arguments[0] == QLatin1String("convert-game")) {
        code = convertGameCommand(rest, out, err);
    } else if (arguments[0] == QLatin1String("merge")) {
        code = merge(rest, out, err);
    } else if (arguments[0] == QLatin1String("export-svg")) {
        code = exportSvg(rest, out, err);
    } else if (arguments[0] == QLatin1String("export-batch")) {
        code = exportBatch(rest, out, err);
    } else if (arguments[0] == QLatin1String("scenario-record")) {
        code = scenarioRecord(rest, out, err);
    } else if (arguments[0] == QLatin1String("scenario-check")) {
        code = scenarioCheck(rest, out, err);
    } else {
        code = exportPng(rest, out, err);
    }
    out.flush();
    err.flush();
    return code;
}

} // namespace eggbert
