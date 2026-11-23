// SPDX-License-Identifier: GPL-3.0-only
#pragma once

// Generates a stand-in for a Mobile Eggbert folder for tests: picture
// sheets drawn here on the port's grid (solid colours, one per picture),
// backdrops, and a text world. Nothing in it comes from the port.

#include "check.hpp"
#include "generated_game.hpp"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QPainter>

#include <string>

namespace eggbert::test {

// Colour of picture `index` of a generated port sheet (0 tiles, 1 elements,
// 2 bigDecor, 3 player, 4 the second player sheet).
inline QColor portColor(int sheet, int index) {
    return QColor(30 + (index * 53 + sheet * 17) % 210, 20 + (index * 29) % 220,
                  10 + (index * 71 + sheet * 90) % 230);
}

// A sheet of `columns` x `rows` pictures of `cell` pixels, each preceded by
// a `frame` of transparent pixels, as the port lays its sheets out.
inline QImage portSheet(int sheet, int columns, int rows, int cell, int frame) {
    const int pitch = cell + frame;
    QImage image(frame + columns * pitch, frame + rows * pitch, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    for (int index = 0; index < columns * rows; ++index) {
        painter.fillRect(frame + index % columns * pitch, frame + index / columns * pitch, cell,
                         cell, portColor(sheet, index));
    }
    return image;
}

inline const char *const portTreasure =
    "MoveObject: type=5 stepAdvance=1 stepRecede=1 timeStopStart=0 timeStopEnd=0 "
    "posStart=258;196 posEnd=258;196 posCurrent=258;196 step=1 time=0 phase=0 channel=10 "
    "icon=0 \n";
inline const char *const portLift =
    "MoveObject: type=1 stepAdvance=20 stepRecede=20 timeStopStart=5 timeStopEnd=5 "
    "posStart=640;832 posEnd=640;448 posCurrent=640;832 step=1 time=0 phase=0 channel=1 "
    "icon=29 \n";

// A world with tile 40 at column 3, row 4, a bigDecor picture 20 at column
// 9, row 0, a treasure and a lift.
inline std::string portWorldText() {
    std::string text = "DescFile: posDecor=0;550 dimDecor=100;100 world=0 music=3 region=7 "
                       "blupiPos=66;774 blupiDir=2 \n";
    for (const auto *section : {"Decor: \n", "BigDecor: \n"}) {
        text += section;
        for (int x = 0; x < 100; ++x) {
            std::string line(99, ',');
            if (x == 3 && std::string(section) == "Decor: \n") {
                line = ",,,,40" + std::string(95, ',');
            }
            if (x == 9 && std::string(section) == "BigDecor: \n") {
                line = "20" + std::string(99, ',');
            }
            text += line + "\n";
        }
    }
    return text + portTreasure + portLift;
}

struct Port {
    QString root;
    QString content;
    QString worldPath;
};

// `root`/Content/icons, `root`/Content/backgrounds and `root`/worlds.
inline Port makePort(const QString &root) {
    Port port;
    port.root = root;
    port.content = root + "/Content";
    QDir().mkpath(port.content + "/icons");
    QDir().mkpath(port.content + "/backgrounds");
    QDir().mkpath(root + "/worlds");
    // Sizes as the port's own sheets have them: tiles 20 x 22 with a frame,
    // elements and players 10 columns of 60, bigDecor 10 x 10 of 144.
    CHECK(portSheet(0, 20, 22, 64, 1).save(port.content + "/icons/object-m.png"));
    CHECK(portSheet(1, 10, 29, 60, 0).save(port.content + "/icons/element.png"));
    CHECK(portSheet(2, 10, 10, 144, 0).save(port.content + "/icons/explo.png"));
    CHECK(portSheet(3, 10, 34, 60, 0).save(port.content + "/icons/blupi.png"));
    CHECK(portSheet(4, 10, 34, 60, 0).save(port.content + "/icons/blupi1.png"));
    QImage backdrop(640, 480, QImage::Format_RGB32);
    backdrop.fill(QColor(12, 90, 160));
    CHECK(backdrop.save(port.content + "/backgrounds/decor007.png"));
    port.worldPath = root + "/worlds/world011.txt";
    QFile file(port.worldPath);
    CHECK(file.open(QIODevice::WriteOnly));
    file.write(QByteArray::fromStdString(portWorldText()));
    return port;
}

} // namespace eggbert::test
