// SPDX-License-Identifier: GPL-3.0-only
#include "cli.hpp"

#include <QFile>
#include <QFileInfo>

#include <algorithm>
#include <optional>

#include "app/document_file.hpp"
#include "app/game_assets.hpp"
#include "app/png_export.hpp"
#include "app/probe_report.hpp"
#include "world/doctor.hpp"
#include "world/hex_model.hpp"
#include "world/offset_map.hpp"

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

int usageError(QTextStream &err, const QString &message) {
    err << "eggbert-editor: " << message << "\n" << commandLineUsage();
    return exitUsage;
}

int info(const QStringList &arguments, QTextStream &out, QTextStream &err) {
    const auto parsed = parse(arguments, {}, {});
    if (!parsed.error.isEmpty() || parsed.positional.size() != 1) {
        return usageError(err, parsed.error.isEmpty() ? QStringLiteral("info needs one file")
                                                      : parsed.error);
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
    auto profile = GameProfile::unknown;
    if (parsed.values.contains(QStringLiteral("--game"))) {
        const auto game = parseGame(parsed.values[QStringLiteral("--game")]);
        if (!game) {
            return usageError(err, QStringLiteral("--game takes 1 or 2"));
        }
        profile = *game;
    }
    auto loaded = DocumentFile::load(parsed.positional[0]);
    if (!loaded) {
        err << parsed.positional[0] << ": " << loaded.error().detail << "\n";
        return exitUsage;
    }
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
    auto profile = GameProfile::unknown;
    if (parsed.values.contains(QStringLiteral("--game"))) {
        const auto game = parseGame(parsed.values[QStringLiteral("--game")]);
        if (!game) {
            return usageError(err, QStringLiteral("--game takes 1 or 2"));
        }
        profile = *game;
    }
    if (parsed.values.contains(QStringLiteral("--game-data"))) {
        const auto directory = parsed.values[QStringLiteral("--game-data")];
        if (profile == GameProfile::unknown) {
            profile = detectGameProfile(directory).profile;
        }
        if (profile == GameProfile::unknown) {
            err << directory << ": the game was not recognised; say --game 1 or --game 2\n";
            return exitUsage;
        }
        const auto assets = GameAssets::load(directory, profile);
        if (!assets->usable()) {
            err << directory << ": its tiles cannot be drawn\n";
            for (const auto &line : assets->report()) {
                err << "  " << line << "\n";
            }
            return exitUsage;
        }
        options.render.artwork = assets;
    }
    options.render.profile = profile;
    auto loaded = DocumentFile::load(parsed.positional[0]);
    if (!loaded) {
        err << parsed.positional[0] << ": " << loaded.error().detail << "\n";
        return exitUsage;
    }
    const auto result =
        exportWorldPng(loaded.value()->document().view(), profile, options, parsed.positional[1]);
    if (!result.ok) {
        err << parsed.positional[1] << ": " << result.error << "\n";
        return exitUsage;
    }
    out << parsed.positional[1] << ": " << result.width << " x " << result.height << " pixels\n";
    return exitOk;
}

} // namespace

bool isCommandLineCommand(const QString &name) {
    return name == QLatin1String("info") || name == QLatin1String("validate") ||
           name == QLatin1String("diff") || name == QLatin1String("export-png");
}

bool commandNeedsGraphics(const QString &name) {
    return name == QLatin1String("export-png");
}

QString commandLineUsage() {
    return QStringLiteral(
        "Commands (read-only; no window is opened):\n"
        "  eggbert-editor info FILE\n"
        "      structural report and decoded summary of a world file\n"
        "  eggbert-editor validate FILE [--game 1|2]\n"
        "      World Doctor findings; exit 1 when there are errors\n"
        "  eggbert-editor diff FILE OTHER\n"
        "      differing byte ranges with their structural names; exit 1 when different\n"
        "  eggbert-editor export-png FILE OUT.png [--scale 1..64] [--crop x,y,w,h]\n"
        "      [--game 1|2] [--game-data DIR] [--no-foreground] [--no-bigdecor]\n"
        "      [--no-objects] [--no-starts] [--no-paths] [--no-backdrop] [--grid]\n"
        "      [--labels] [--transparent]\n"
        "      picture of the world, or of a crop given in cells\n"
        "Exit codes: 0 success, 1 findings or differences, 2 usage or file error.\n");
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
    } else {
        code = exportPng(rest, out, err);
    }
    out.flush();
    err.flush();
    return code;
}

} // namespace eggbert
