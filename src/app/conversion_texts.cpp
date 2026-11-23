// SPDX-License-Identifier: GPL-3.0-only
#include "app/conversion_texts.hpp"

#include <QCoreApplication>

namespace eggbert {

namespace {

// Keep in step with the sentences in src/world/game_convert.cpp (the
// `translation` test fails when one is missing).
constexpr const char *texts[] = {
    QT_TRANSLATE_NOOP("eggbert::Conversion",
        "The first game has no such tile: it draws nothing there and nothing is "
        "solid."),
    QT_TRANSLATE_NOOP("eggbert::Conversion",
        "The second game shows tile 215 for this number; the first game draws its own "
        "block %1 here."),
    QT_TRANSLATE_NOOP("eggbert::Conversion",
        "The second game has no picture for this block: when it loads the world it "
        "shows tile 215 instead."),
    QT_TRANSLATE_NOOP("eggbert::Conversion",
        "The first game has no such back-layer picture and draws nothing."),
    QT_TRANSLATE_NOOP("eggbert::Conversion",
        "The first game's code never refers to this object type."),
    QT_TRANSLATE_NOOP("eggbert::Conversion",
        "The second game's code never refers to this object type."),
    QT_TRANSLATE_NOOP("eggbert::Conversion",
        "This lift or crate uses a tile picture the first game does not have and "
        "would be invisible."),
    QT_TRANSLATE_NOOP("eggbert::Conversion",
        "This lift or crate uses a picture number the second game has no picture for; "
        "the first game draws its block there."),
    QT_TRANSLATE_NOOP("eggbert::Conversion",
        "This lift or crate uses a block picture the second game does not have (its "
        "loader replaces such tiles only in the tile layer)."),
    QT_TRANSLATE_NOOP("eggbert::Conversion",
        "The first game's editor offers backdrops 0 to 23 only."),
    QT_TRANSLATE_NOOP("eggbert::Conversion",
        "This backdrop number is a different picture in the other game."),
};

QString translated(const char *text) {
    return QCoreApplication::translate("eggbert::Conversion", text);
}

QString itemText(GameItem item) {
    switch (item) {
    case GameItem::foregroundTile:
        return translated(QT_TRANSLATE_NOOP("eggbert::Conversion", "foreground tile"));
    case GameItem::bigDecorTile:
        return translated(QT_TRANSLATE_NOOP("eggbert::Conversion", "bigDecor tile"));
    case GameItem::objectType:
        return translated(QT_TRANSLATE_NOOP("eggbert::Conversion", "object type"));
    case GameItem::objectIcon:
        return translated(QT_TRANSLATE_NOOP("eggbert::Conversion", "object picture"));
    case GameItem::region:
        return translated(QT_TRANSLATE_NOOP("eggbert::Conversion", "region"));
    }
    return {};
}

} // namespace

QString conversionNoteLine(const GameConversionNote &note) {
    QString where;
    if (note.firstCell) {
        where = translated(QT_TRANSLATE_NOOP("eggbert::Conversion", ", first at cell %1,%2"))
                    .arg(note.firstCell->x)
                    .arg(note.firstCell->y);
    } else if (note.firstSlot) {
        where = translated(QT_TRANSLATE_NOOP("eggbert::Conversion", ", first in slot %1"))
                    .arg(*note.firstSlot);
    }
    // The sentence: its pattern translated and filled with the value, or the
    // core's English text when it has no pattern.
    QString sentence = QString::fromStdString(note.text);
    if (!note.textPattern.empty()) {
        const std::string pattern(note.textPattern);
        sentence = translated(pattern.c_str());
        sentence.replace(QStringLiteral("%1"), QString::number(note.value));
    }
    return translated(QT_TRANSLATE_NOOP("eggbert::Conversion", "[%1] %2 %3 (%4 place(s)%5): %6"))
        .arg(note.kind == CarryKind::unknownToTarget
                 ? translated(QT_TRANSLATE_NOOP("eggbert::Conversion", "unknown"))
                 : translated(QT_TRANSLATE_NOOP("eggbert::Conversion", "differs")),
             itemText(note.item))
        .arg(note.value)
        .arg(note.count)
        .arg(where, sentence);
}

std::span<const char *const> conversionTexts() {
    return texts;
}

} // namespace eggbert
