// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Generates a stand-in for a game directory for tests: sprite sheets drawn
// here (solid colours on the transparent blue), saved as bitmaps under the
// games' file names, and an "executable" holding the matching sprite
// tables between unrelated bytes. Nothing in it comes from a game.

#include "check.hpp"
#include "world/atlas_table.hpp"

#include <QColor>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QImage>
#include <QPainter>

#include <vector>

namespace eggbert::test {

inline const QColor blue(0, 0, 255);

// Colour of generated sprite `index` of a sheet; never pure blue.
inline QColor spriteColor(int sheet, int index) {
    return QColor(40 + (index * 37) % 200, 30 + (index * 61 + sheet * 40) % 200,
                  (index * 13 + sheet * 70) % 200);
}

struct GeneratedSheet {
    QImage image;
    std::vector<SpriteRect> sprites;
};

// A sheet of `count` sprites on a grid of `cellSize` cells. Sprite 5 is
// trimmed (smaller than its cell, with an offset) and sprite 6 has a blue
// hole in its middle, to exercise offsets and the transparent colour.
inline GeneratedSheet generateSheet(int sheetId, int count, int cellSize, int columns) {
    GeneratedSheet sheet;
    const int rows = (count + columns - 1) / columns;
    sheet.image = QImage(columns * cellSize, rows * cellSize, QImage::Format_RGB32);
    sheet.image.fill(blue);
    QPainter painter(&sheet.image);
    for (int index = 0; index < count; ++index) {
        SpriteRect sprite;
        const int cellX = (index % columns) * cellSize;
        const int cellY = (index / columns) * cellSize;
        if (index == 5) {
            sprite = {static_cast<std::int16_t>(cellX + 4), static_cast<std::int16_t>(cellY + 6),
                      12, 34, 40, 20};
        } else {
            sprite = {static_cast<std::int16_t>(cellX), static_cast<std::int16_t>(cellY), 0, 0,
                      static_cast<std::int16_t>(cellSize), static_cast<std::int16_t>(cellSize)};
        }
        painter.fillRect(sprite.srcX, sprite.srcY, sprite.width, sprite.height,
                         spriteColor(sheetId, index));
        if (index == 6) {
            painter.fillRect(sprite.srcX + 20, sprite.srcY + 20, 10, 10, blue);
        }
        sheet.sprites.push_back(sprite);
    }
    return sheet;
}

inline void appendTable(QByteArray &bytes, const std::vector<SpriteRect> &sprites) {
    const auto put = [&](int value) {
        bytes.append(static_cast<char>(value & 0xFF));
        bytes.append(static_cast<char>((value >> 8) & 0xFF));
    };
    put(static_cast<int>(sprites.size()));
    for (const auto &sprite : sprites) {
        put(sprite.srcX);
        put(sprite.srcY);
        put(sprite.offsetX);
        put(sprite.offsetY);
        put(sprite.width);
        put(sprite.height);
    }
}

inline void write(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(bytes);
}

inline void saveBitmap(const QImage &image, const QString &path) {
    // Saved as BMP under the games' .blp name.
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    CHECK(image.save(&file, "BMP"));
}

// Digest of every file name and content below a directory.
inline QByteArray directoryDigest(const QString &directory) {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    QStringList paths;
    QDirIterator iterator(directory, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        paths << iterator.next();
    }
    paths.sort();
    for (const auto &path : paths) {
        QFile file(path);
        CHECK(file.open(QIODevice::ReadOnly));
        hash.addData(path.toUtf8());
        hash.addData(file.readAll());
    }
    return hash.result();
}

struct Game {
    QString root;
    GeneratedSheet tiles;
    GeneratedSheet elements;
    GeneratedSheet bigDecor;
    GeneratedSheet players;
};

// A generated stand-in for a first-game directory: upper-case names, sheets
// saved as bitmaps, and an "executable" holding the four sprite tables
// between unrelated bytes.
inline Game makeGame(const QString &root, bool secondGame = false) {
    Game game;
    game.root = root;
    QDir().mkpath(root + "/IMAGE08");
    QDir().mkpath(root + "/DATA");
    write(root + (secondGame ? "/DATA/DEMO300.BLP" : "/DATA/DEMO200.BLP"), "recording");
    game.tiles = generateSheet(0, secondGame ? 441 : 246, 64, 16);
    game.elements = generateSheet(1, secondGame ? 289 : 187, 60, 16);
    game.bigDecor = generateSheet(2, secondGame ? 100 : 54, 128, 8);
    game.players = generateSheet(3, secondGame ? 336 : 291, 60, 16);
    saveBitmap(game.tiles.image, root + "/IMAGE08/OBJECT.BLP");
    saveBitmap(game.elements.image, root + "/IMAGE08/ELEMENT.BLP");
    // One sheet as an 8-bit palette bitmap, as the original IMAGE08 files are.
    saveBitmap(game.bigDecor.image.convertToFormat(QImage::Format_Indexed8,
                                                   Qt::ThresholdDither | Qt::AvoidDither),
               root + "/IMAGE08/EXPLO.BLP");
    for (int index = 0; index < 4; ++index) {
        saveBitmap(game.players.image, root + QString("/IMAGE08/BLUPI00%1.BLP").arg(index));
    }
    QByteArray executable(5000, '\xEE');
    appendTable(executable, game.players.sprites);
    executable.append(QByteArray(333, '\xF1'));
    appendTable(executable, game.tiles.sprites);
    executable.append(QByteArray(1000, '\xE7'));
    appendTable(executable, game.elements.sprites);
    appendTable(executable, game.bigDecor.sprites);
    executable.append(QByteArray(2000, '\xFA'));
    write(root + "/GAME.EXE", executable);
    return game;
}

} // namespace eggbert::test
