// SPDX-License-Identifier: GPL-3.0-only
#include "app/core_texts.hpp"

#include <QCoreApplication>

namespace eggbert {

namespace {

// Patterns of sentences composed in src/world. A sentence that is not
// listed is simply shown in English.
constexpr const char *texts[] = {
    QT_TRANSLATE_NOOP("eggbert::Core",
        "world%1.blp is used by more than one entry."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "u%1-%2.blp is used by more than one entry."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "world%1.txt is used by more than one entry."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "World %1 is missing: the game always starts a campaign there."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "World %1 is outside the campaign numbering; the game reaches it only by its "
        "number."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Hub %1 is missing, but missions of its group (%2 to %3) are present."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Hub %1 has none of its missions (%2 to %3)."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "World %1, which the game enters after world %2 is won, is missing."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "numbering rules of the games (hubs are world %1 and multiples of ten, a "
        "group has missions hub+%2 to hub+%3)"),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Mission %1 has neither a goal nor a key, so it cannot be won."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The sign of mission %1 in hub %2 has no door (tile %3) one or two cells beside "
        "it, so the mission can be entered from the start."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "No mission of hub %1 ends with a key (an object of type %2), so the hub can never "
        "be completed and no barrier of world %3 opens for it."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "%1 missions of hub %2 end with a key; winning any one of them completes the hub."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "World %1 has %2 barriers (tile %3) but only %4 hubs are present; the game removes "
        "one barrier for every hub that is won."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The stamp is empty or damaged."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "An object of the stamp would lie outside the world here. Place the stamp "
        "further from the edge."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "This world has no object records."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The stamp has %1 object(s) but only %2 object slot(s) are free."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The file lists more objects than the game reads."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The tile number at column %1, row %2 is too large for the editor."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The type of object line %1 is %2, where the game stops reading objects."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The start or its direction is missing."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "This is not the image of a text world."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "A Mobile Eggbert world has no title; remove it to save."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "A reserved part of the header was changed; a Mobile Eggbert world cannot store "
        "it."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "A Mobile Eggbert world has one start; starts %1 to %2 must equal start %3."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The object in slot %1 has values a Mobile Eggbert world cannot store."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The game reads at most %1 objects."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "A value could not be written into the text; nothing was changed."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "There is no DescFile line."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Line %1: the field dimDecor is missing or is not two numbers followed by a "
        "space."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Line %1: the field posDecor is missing or is not two numbers followed by a "
        "space."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Line %1: the field blupiPos is missing or is not two numbers followed by a "
        "space."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Line %1: the field music is missing or is not a number followed by a space."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Line %1: the field region is missing or is not a number followed by a space."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Line %1: the field blupiDir is missing or is not a number followed by a "
        "space."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "There is no Decor block."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "There is no BigDecor block."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The Decor block ends after %1 of %2 lines."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The BigDecor block ends after %1 of %2 lines."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Line %1: the Decor block has only %2 of %3 lines before the next section."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Line %1: the BigDecor block has only %2 of %3 lines before the next section."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Line %1: the line has %2 entries; a column has %3 cells."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Line %1: entry %2 is not a number."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Line %1: the object field type is missing or is not a number followed by a "
        "space."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Line %1: the object field channel is missing or is not a number followed by "
        "a space."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Line %1: the object field icon is missing or is not a number followed by a "
        "space."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Line %1: the object field posStart is missing or is not two numbers followed "
        "by a space."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Line %1: the object field posEnd is missing or is not two numbers followed "
        "by a space."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Line %1: the object field posCurrent is missing or is not two numbers "
        "followed by a space."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Line %1: the object's type is %2 or not readable; the game stops reading "
        "objects here and ignores the %3 object line(s) after it."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "There are %1 object lines; the game reads the first %2."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Only a complete world of revision %1.%2 to %3.%4 can be converted."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Choose a revision other than the world's own."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The title is kept in the file, but the games read it only from revision "
        "%1.%2."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "An empty bigDecor layer was added."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "%1 bigDecor cell(s) are not empty; revision %2.%3 has no bigDecor layer."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The empty bigDecor layer was left out."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "%1 free object slots were added."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "%1 object(s) are stored in slots %2 to %3; the target revision stores only "
        "%4 slots."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "%1 free slot(s) beyond the target's %2 hold leftover bytes that would be "
        "dropped."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "%1 empty object slots were left out."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "%1 byte(s) after the layout were carried over."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "All three files must be complete worlds of a known revision."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The three files are not of the same revision. Convert them to one revision "
        "first."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The three files differ in length (bytes after the world); they cannot be "
        "merged position by position."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Choose the two different games to convert between."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "%1 stored value(s) in %2 place(s) are unknown to the target game."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The values unknown to the target game could not be removed."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The file is shorter than a recording's %1-byte header."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The header does not carry revision %1.%2."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The reserved part of the header is not zero."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "The events do not fill whole four-byte records."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "There are more than the %1,%2 events the games record."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Event %1 is earlier than the one before it."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Event %1 holds keys the games do not have."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Line %1: expected a name, one of =, >= or <=, and a number."),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Decor (%1, %2)"),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "BigDecor (%1, %2)"),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Remove object %1"),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Duplicate object %1"),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Add object"),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "Decor: %1 cell(s)"),
    QT_TRANSLATE_NOOP("eggbert::Core",
        "BigDecor: %1 cell(s)"),
};

bool isDigit(QChar character) {
    return character >= QLatin1Char('0') && character <= QLatin1Char('9');
}

} // namespace

QString corePattern(std::string_view english, QStringList *numbers) {
    const auto text = QString::fromUtf8(english.data(), static_cast<qsizetype>(english.size()));
    QString pattern;
    QStringList found;
    for (qsizetype index = 0; index < text.size();) {
        if (!isDigit(text[index])) {
            pattern += text[index++];
            continue;
        }
        auto end = index;
        while (end < text.size() && isDigit(text[end])) {
            ++end;
        }
        auto number = text.mid(index, end - index);
        const bool minus = index > 0 && text[index - 1] == QLatin1Char('-') &&
                           (index == 1 || text[index - 2] == QLatin1Char(' ') ||
                            text[index - 2] == QLatin1Char('('));
        if (minus) {
            pattern.chop(1);
            number.prepend(QLatin1Char('-'));
        }
        found << number;
        pattern += QLatin1Char('%') + QString::number(found.size());
        index = end;
    }
    if (numbers != nullptr) {
        *numbers = found;
    }
    return pattern;
}

QString coreText(std::string_view english) {
    const auto original = QString::fromUtf8(english.data(), static_cast<qsizetype>(english.size()));
    QStringList numbers;
    const auto pattern = corePattern(english, &numbers);
    if (numbers.size() > 9) {
        return original;
    }
    const auto key = pattern.toUtf8();
    const auto translated = QCoreApplication::translate("eggbert::Core", key.constData());
    if (translated == pattern) {
        return original; // no translation: exactly the core's sentence
    }
    QString text;
    text.reserve(translated.size() + 16);
    for (qsizetype index = 0; index < translated.size(); ++index) {
        const auto digit = index + 1 < translated.size() ? translated[index + 1] : QChar();
        if (translated[index] == QLatin1Char('%') && digit >= QLatin1Char('1') &&
            digit <= QLatin1Char('9')) {
            const auto number = digit.unicode() - u'1';
            if (number < numbers.size()) {
                text += numbers[number];
            }
            ++index;
        } else {
            text += translated[index];
        }
    }
    return text;
}

std::span<const char *const> coreTexts() {
    return texts;
}

} // namespace eggbert
