// SPDX-License-Identifier: GPL-3.0-only
#include "game_assets.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <algorithm>

#include "app/document_file.hpp"

namespace eggbert {

namespace {

QString tr(const char *text) {
    return QCoreApplication::translate("GameAssets", text);
}

// Entry of `directory` whose name equals `name` ignoring case (the games
// ran on case-insensitive file systems). Empty when absent.
QString findEntry(const QString &directory, const QString &name, QDir::Filters filters) {
    const QDir dir(directory);
    for (const auto &entry : dir.entryList(filters | QDir::NoDotAndDotDot)) {
        if (entry.compare(name, Qt::CaseInsensitive) == 0) {
            return dir.filePath(entry);
        }
    }
    return {};
}

std::optional<QByteArray> readLimited(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > GameAssets::maximumFileBytes) {
        return std::nullopt;
    }
    auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        return std::nullopt;
    }
    return bytes;
}

struct TableSpec {
    const char *name;
    std::size_t firstGameCount;
    std::size_t secondGameCount;
    // Logical cell of a sprite, with a little slack.
    int extentX;
    int extentY;
};

// Sprite counts read from the reconstruction (docs/research/evidence.md):
// first game 246 / 187 / 54 / 291, second game 441 / 289 / 100 / 336.
constexpr TableSpec tableSpecs[4] = {
    {"tiles", 246, 441, 64, 64},
    {"elements", 187, 289, 64, 64},
    {"bigDecor and effects", 54, 100, 128, 160},
    {"player", 291, 336, 64, 64},
};

struct SheetSpec {
    const char *fileName;
    int table;
};

constexpr SheetSpec sheetSpecs[spriteSheetCount] = {
    {"object.blp", 0},   {"element.blp", 1},  {"explo.blp", 2},    {"blupi000.blp", 3},
    {"blupi001.blp", 3}, {"blupi002.blp", 3}, {"blupi003.blp", 3},
};

// The games draw sprites with pure blue as the transparent colour.
void makeBlueTransparent(QImage &image) {
    const QRgb key = qRgb(0, 0, 255);
    for (int y = 0; y < image.height(); ++y) {
        auto *line = reinterpret_cast<QRgb *>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            if ((line[x] & 0x00FFFFFFu) == (key & 0x00FFFFFFu)) {
                line[x] = 0;
            }
        }
    }
}

} // namespace

QString GameAssets::mobileContentDirectory(const QString &directory) {
    for (const auto &candidate :
         {directory, findEntry(directory, QStringLiteral("Content"), QDir::Dirs)}) {
        if (candidate.isEmpty()) {
            continue;
        }
        const auto icons = findEntry(candidate, QStringLiteral("icons"), QDir::Dirs);
        if (!icons.isEmpty() &&
            !findEntry(icons, QStringLiteral("object-m.png"), QDir::Files).isEmpty()) {
            return QFileInfo(candidate).absoluteFilePath();
        }
    }
    return {};
}

ProfileDetection detectGameProfile(const QString &directory) {
    ProfileDetection detection;
    if (!GameAssets::mobileContentDirectory(directory).isEmpty()) {
        detection.profile = GameProfile::mobileEggbert;
        detection.evidence << tr("icons/object-m.png exists: the picture sheet of Mobile Eggbert.");
        return detection;
    }
    const auto data = findEntry(directory, QStringLiteral("data"), QDir::Dirs);
    if (data.isEmpty()) {
        detection.evidence << tr("No DATA folder was found, so the game cannot be recognised.");
        return detection;
    }
    // The games name their attract-mode recordings demo2NN (first game) and
    // demo3NN (second game).
    const bool first = !findEntry(data, QStringLiteral("demo200.blp"), QDir::Files).isEmpty();
    const bool second = !findEntry(data, QStringLiteral("demo300.blp"), QDir::Files).isEmpty();
    if (first && !second) {
        detection.profile = GameProfile::speedyBlupi1;
        detection.evidence << tr("DATA/demo200.blp exists: the first game's recordings are "
                                 "numbered from 200.");
    } else if (second && !first) {
        detection.profile = GameProfile::speedyBlupi2;
        detection.evidence << tr("DATA/demo300.blp exists: the second game's recordings are "
                                 "numbered from 300.");
    } else if (first && second) {
        detection.evidence << tr("Both DATA/demo200.blp and DATA/demo300.blp exist; choose the "
                                 "game yourself.");
    } else {
        detection.evidence << tr("Neither DATA/demo200.blp nor DATA/demo300.blp exists; choose "
                                 "the game yourself.");
    }
    return detection;
}

std::optional<SpriteSheet> GameAssets::sheetForChannel(int channel) {
    switch (channel) {
    case 1:
        return SpriteSheet::tiles;
    case 10:
        return SpriteSheet::elements;
    case 9:
        return SpriteSheet::bigDecor;
    case 2:
        return SpriteSheet::player1;
    case 11:
        return SpriteSheet::player2;
    case 12:
        return SpriteSheet::player3;
    case 13:
        return SpriteSheet::player4;
    default:
        break;
    }
    return std::nullopt;
}

std::shared_ptr<const GameAssets> GameAssets::load(const QString &directory,
                                                   GameProfile profile) {
    if (profile == GameProfile::mobileEggbert) {
        return loadMobile(directory);
    }
    auto assets = std::make_shared<GameAssets>();
    assets->directory_ = QFileInfo(directory).absoluteFilePath();
    assets->profile_ = profile;
    auto &report = assets->report_;
    if (profile == GameProfile::unknown) {
        report << tr("No game was chosen for this directory.");
        return assets;
    }
    const bool second = profile == GameProfile::speedyBlupi2;

    QString imageDirectory;
    for (const char *candidate : {"image08", "image16", "image"}) {
        imageDirectory = findEntry(assets->directory_, QString::fromLatin1(candidate), QDir::Dirs);
        if (!imageDirectory.isEmpty()) {
            break;
        }
    }
    assets->imageDirectory_ = imageDirectory;
    if (imageDirectory.isEmpty()) {
        report << tr("No IMAGE08, IMAGE16 or IMAGE folder was found.");
        return assets;
    }
    report << tr("Sprite sheets are read from %1.").arg(QDir(imageDirectory).dirName());

    // Sheets: ordinary uncompressed bitmaps with another file extension.
    for (std::size_t index = 0; index < spriteSheetCount; ++index) {
        const auto name = QString::fromLatin1(sheetSpecs[index].fileName);
        const auto path = findEntry(imageDirectory, name, QDir::Files);
        if (path.isEmpty()) {
            report << tr("%1: not found.").arg(name);
            continue;
        }
        const auto bytes = readLimited(path);
        auto image = bytes ? QImage::fromData(*bytes, "BMP") : QImage();
        if (image.isNull() || image.width() > 8192 || image.height() > 8192) {
            report << tr("%1: not a readable bitmap.").arg(name);
            continue;
        }
        image = image.convertToFormat(QImage::Format_ARGB32);
        makeBlueTransparent(image);
        assets->sheets_[index].image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        assets->sheets_[index].table = sheetSpecs[index].table;
    }

    // Sprite tables: searched in the executables next to the folders.
    QList<QByteArray> executables;
    QStringList executableNames;
    for (const auto &entry : QDir(assets->directory_).entryList(QDir::Files, QDir::Name)) {
        if (entry.endsWith(QLatin1String(".exe"), Qt::CaseInsensitive)) {
            if (const auto bytes = readLimited(QDir(assets->directory_).filePath(entry))) {
                executables << *bytes;
                executableNames << entry;
            }
        }
    }
    if (executables.isEmpty()) {
        report << tr("No executable (*.exe) was found next to the folders, so sprite positions "
                     "are unknown and nothing can be drawn with artwork.");
    }
    // Two passes: tables that are found unambiguously first; then the ones
    // with several candidates again, ignoring candidates that lie inside
    // bytes an accepted table of the same executable already occupies.
    struct TableSearch {
        bool wanted = false;
        AtlasSearch search;
        qsizetype executable = -1; // where it was found
        qsizetype ambiguousIn = -1;
        bool secondPass = false;
    };
    std::array<TableSearch, 4> searches;
    std::vector<std::vector<std::byte>> executableBytes;
    executableBytes.reserve(static_cast<std::size_t>(executables.size()));
    for (const auto &executable : executables) {
        executableBytes.push_back(toByteVector(executable));
    }
    for (std::size_t table = 0; table < 4; ++table) {
        // The first loaded sheet that uses this table defines the size it
        // has to fit.
        const Sheet *reference = nullptr;
        for (const auto &sheet : assets->sheets_) {
            if (sheet.table == static_cast<int>(table) && !sheet.image.isNull()) {
                reference = &sheet;
                break;
            }
        }
        if (reference == nullptr || executables.isEmpty()) {
            continue;
        }
        const auto &spec = tableSpecs[table];
        auto &state = searches[table];
        state.wanted = true;
        state.search = AtlasSearch{second ? spec.secondGameCount : spec.firstGameCount,
                                   reference->image.width(), reference->image.height(),
                                   spec.extentX, spec.extentY};
        for (qsizetype index = 0; index < executables.size(); ++index) {
            const auto found =
                findAtlasTable(executableBytes[static_cast<std::size_t>(index)], state.search);
            if (found) {
                assets->tables_[table] = *found;
                assets->tableFound_[table] = true;
                state.executable = index;
                break;
            }
            if (found.error() == AtlasSearchError::ambiguous && state.ambiguousIn < 0) {
                state.ambiguousIn = index;
            }
        }
    }
    for (std::size_t table = 0; table < 4; ++table) {
        auto &state = searches[table];
        if (!state.wanted || state.executable >= 0 || state.ambiguousIn < 0) {
            continue;
        }
        std::vector<ByteRange> occupied;
        for (std::size_t other = 0; other < 4; ++other) {
            if (searches[other].executable == state.ambiguousIn && !searches[other].secondPass) {
                occupied.push_back(assets->tables_[other].byteRange());
            }
        }
        if (occupied.empty()) {
            continue;
        }
        auto narrowed = state.search;
        narrowed.excluded = occupied;
        const auto found = findAtlasTable(
            executableBytes[static_cast<std::size_t>(state.ambiguousIn)], narrowed);
        if (found) {
            assets->tables_[table] = *found;
            assets->tableFound_[table] = true;
            state.executable = state.ambiguousIn;
            state.secondPass = true;
        }
    }
    for (std::size_t table = 0; table < 4; ++table) {
        const auto &state = searches[table];
        if (!state.wanted) {
            continue;
        }
        QString outcome = tr("not found in any executable");
        if (state.executable >= 0) {
            outcome = tr("%1 sprites located in %2 at offset %3")
                          .arg(assets->tables_[table].sprites.size())
                          .arg(executableNames[state.executable])
                          .arg(assets->tables_[table].fileOffset);
            if (state.secondPass) {
                outcome += tr(" (look-alikes inside the other sprite tables were set aside)");
            }
        } else if (state.ambiguousIn >= 0) {
            outcome = tr("more than one candidate in %1; none is used")
                          .arg(executableNames[state.ambiguousIn]);
        }
        report << tr("Sprite positions for %1: %2.")
                      .arg(QString::fromLatin1(tableSpecs[table].name), outcome);
    }

    // A sheet smaller than its table's extent cannot be drawn safely.
    for (std::size_t index = 0; index < spriteSheetCount; ++index) {
        auto &sheet = assets->sheets_[index];
        const auto name = QString::fromLatin1(sheetSpecs[index].fileName);
        if (sheet.image.isNull()) {
            continue;
        }
        const auto table = static_cast<std::size_t>(sheet.table);
        if (!assets->tableFound_[table]) {
            report << tr("%1: %2 x %3 pixels, but its sprite positions are unknown; not used.")
                          .arg(name)
                          .arg(sheet.image.width())
                          .arg(sheet.image.height());
            sheet.image = QImage();
            continue;
        }
        const bool fits = std::ranges::all_of(
            assets->tables_[table].sprites, [&](const SpriteRect &sprite) {
                return sprite.srcX + sprite.width <= sheet.image.width() &&
                       sprite.srcY + sprite.height <= sheet.image.height();
            });
        if (!fits) {
            report << tr("%1: smaller than its sprite positions require; not used.").arg(name);
            sheet.image = QImage();
            continue;
        }
        report << tr("%1: %2 x %3 pixels, usable.")
                      .arg(name)
                      .arg(sheet.image.width())
                      .arg(sheet.image.height());
    }
    report << (assets->usable()
                   ? tr("Tiles can be drawn with this artwork. Anything not listed as usable is "
                        "drawn schematically.")
                   : tr("The tile sheet is not usable, so the world is drawn schematically."));
    return assets;
}

// The port draws every picture from a regular grid: the tile sheet in cells
// of 64 with a one-pixel frame around each, the element and player sheets in
// cells of 60, the bigDecor sheet in cells of 144 (mobile-eggbert dd0a592
// src/WindowsPhoneSpeedyBlupi/Pixmap.cpp:496-560, :649-663). Nothing is read
// from an executable.
std::shared_ptr<const GameAssets> GameAssets::loadMobile(const QString &directory) {
    auto assets = std::make_shared<GameAssets>();
    assets->profile_ = GameProfile::mobileEggbert;
    auto &report = assets->report_;
    const auto content = mobileContentDirectory(directory);
    assets->directory_ =
        content.isEmpty() ? QFileInfo(directory).absoluteFilePath() : content;
    if (content.isEmpty()) {
        report << tr("No icons folder with object-m.png was found here or in a Content folder.");
        return assets;
    }
    const auto icons = findEntry(content, QStringLiteral("icons"), QDir::Dirs);
    assets->imageDirectory_ = findEntry(content, QStringLiteral("backgrounds"), QDir::Dirs);
    report << tr("Pictures are read from %1; their places follow the port's grid.").arg(icons);

    struct Grid {
        const char *fileName;
        int table;
        int cell;
        int frame;
    };
    // Sheets in the order of SpriteSheet; the three further player channels
    // share one sheet.
    constexpr Grid grids[spriteSheetCount] = {
        {"object-m.png", 0, 64, 1}, {"element.png", 1, 60, 0}, {"explo.png", 2, 144, 0},
        {"blupi.png", 3, 60, 0},    {"blupi1.png", 3, 60, 0},  {"blupi1.png", 3, 60, 0},
        {"blupi1.png", 3, 60, 0},
    };
    for (std::size_t index = 0; index < spriteSheetCount; ++index) {
        const auto &grid = grids[index];
        const auto name = QString::fromLatin1(grid.fileName);
        const auto path = findEntry(icons, name, QDir::Files);
        const auto bytes = path.isEmpty() ? std::nullopt : readLimited(path);
        auto image = bytes ? QImage::fromData(*bytes, "PNG") : QImage();
        if (image.isNull() || image.width() > 8192 || image.height() > 8192) {
            report << tr("%1: not found or not a readable picture.").arg(name);
            continue;
        }
        // Column and row as the port computes them: the sheet's width
        // divided by the cell, each cell preceded by its frame.
        const int columns = image.width() / grid.cell;
        const int pitch = grid.cell + grid.frame;
        const int rows = (image.height() - grid.frame) / pitch;
        if (columns <= 0 || rows <= 0 || grid.frame + (columns - 1) * pitch + grid.cell >
                                             image.width()) {
            report << tr("%1: too small for the port's grid.").arg(name);
            continue;
        }
        assets->sheets_[index].image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        assets->sheets_[index].table = grid.table;
        auto &table = assets->tables_[static_cast<std::size_t>(grid.table)];
        if (!assets->tableFound_[static_cast<std::size_t>(grid.table)]) {
            for (int icon = 0; icon < columns * rows; ++icon) {
                SpriteRect rect;
                rect.srcX = static_cast<std::int16_t>(grid.frame + icon % columns * pitch);
                rect.srcY = static_cast<std::int16_t>(grid.frame + icon / columns * pitch);
                rect.width = static_cast<std::int16_t>(grid.cell);
                rect.height = static_cast<std::int16_t>(grid.cell);
                table.sprites.push_back(rect);
            }
            assets->tableFound_[static_cast<std::size_t>(grid.table)] = true;
        }
        report << tr("%1: %2 x %3 pictures of %4 pixels.")
                      .arg(name)
                      .arg(columns)
                      .arg(rows)
                      .arg(grid.cell);
    }
    report << (assets->usable()
                   ? tr("Tiles can be drawn with this artwork.")
                   : tr("The tile sheet is not usable, so the world is drawn schematically."));
    return assets;
}

bool GameAssets::usable() const {
    return !sheets_[static_cast<std::size_t>(SpriteSheet::tiles)].image.isNull();
}

std::size_t GameAssets::spriteCount(SpriteSheet sheet) const {
    const auto &entry = sheets_[static_cast<std::size_t>(sheet)];
    return entry.image.isNull() ? 0 : tables_[static_cast<std::size_t>(entry.table)].sprites.size();
}

QImage GameAssets::readBackdrop(int region) const {
    if (region < 0 || region > 999 || imageDirectory_.isEmpty()) {
        return {};
    }
    if (profile_ == GameProfile::mobileEggbert) {
        const auto path = findEntry(
            imageDirectory_, QStringLiteral("decor%1.png").arg(region, 3, 10, QLatin1Char('0')),
            QDir::Files);
        const auto bytes = path.isEmpty() ? std::nullopt : readLimited(path);
        auto image = bytes ? QImage::fromData(*bytes, "PNG") : QImage();
        if (image.width() < 16 || image.height() < 16 || image.width() > 4096 ||
            image.height() > 4096) {
            return {};
        }
        return image.convertToFormat(QImage::Format_RGB32);
    }
    const auto name = QStringLiteral("decor%1.blp").arg(region, 3, 10, QLatin1Char('0'));
    // The games take the true-colour copy in IMAGE16 when the display
    // allows and the palette copy otherwise; prefer the better one.
    auto path =
        findEntry(findEntry(directory_, QStringLiteral("image16"), QDir::Dirs), name, QDir::Files);
    if (path.isEmpty()) {
        path = findEntry(imageDirectory_, name, QDir::Files);
    }
    if (path.isEmpty()) {
        return {};
    }
    const auto bytes = readLimited(path);
    if (!bytes) {
        return {};
    }
    auto image = QImage::fromData(*bytes, "BMP");
    // A backdrop is a screen-sized picture; anything else is not what this
    // slot is for.
    if (image.width() < 16 || image.height() < 16 || image.width() > 4096 ||
        image.height() > 4096) {
        return {};
    }
    return image.convertToFormat(QImage::Format_RGB32);
}

const QImage *GameAssets::backdrop(int region) const {
    auto found = backdrops_.find(region);
    if (found == backdrops_.end()) {
        found = backdrops_.emplace(region, readBackdrop(region)).first;
    }
    return found->second.isNull() ? nullptr : &found->second;
}

QString GameAssets::musicFile(int music) const {
    if (music < 1 || music > 999 || profile_ == GameProfile::mobileEggbert) {
        return {}; // the port has no music files
    }
    const auto sound = findEntry(directory_, QStringLiteral("sound"), QDir::Dirs);
    if (sound.isEmpty()) {
        return {};
    }
    return findEntry(sound, QStringLiteral("music%1.blp").arg(music - 1, 3, 10, QLatin1Char('0')),
                     QDir::Files);
}

std::optional<GameAssets::Sprite> GameAssets::sprite(int channel, int icon) const {
    const auto which = sheetForChannel(channel);
    if (!which || icon < 0) {
        return std::nullopt;
    }
    const auto &sheet = sheets_[static_cast<std::size_t>(*which)];
    if (sheet.image.isNull()) {
        return std::nullopt;
    }
    const auto &sprites = tables_[static_cast<std::size_t>(sheet.table)].sprites;
    if (static_cast<std::size_t>(icon) >= sprites.size() ||
        sprites[static_cast<std::size_t>(icon)].empty()) {
        return std::nullopt;
    }
    const auto &rect = sprites[static_cast<std::size_t>(icon)];
    return Sprite{&sheet.image, QRect(rect.srcX, rect.srcY, rect.width, rect.height),
                  QPoint(rect.offsetX, rect.offsetY)};
}

} // namespace eggbert
