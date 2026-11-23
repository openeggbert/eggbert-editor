// SPDX-License-Identifier: GPL-3.0-only
#include "app/doctor_texts.hpp"

#include <QCoreApplication>

#include "app/catalog_texts.hpp"

namespace eggbert {

namespace {

// Keep in step with the patterns in src/world/doctor.cpp (the `translation`
// test fails when one is missing).
constexpr const char *texts[] = {
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "The file has %1 byte(s); at least 4 are needed for the revision field."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "Revision field %1.%2 is later than every known revision; its layout is "
        "unverified."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "Revision field %1.%2 is not a known revision; this may not be a world file."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "The file ends at offset %1; %2 byte(s) of the revision %3.%4 layout are "
        "missing."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "%1 byte(s) follow the layout; they are kept unchanged."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "Revision %1.%2 can be viewed and inspected but not edited in this editor."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "Music %1 is outside %2 to %3."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "Region %1 is outside %2 to %3; the game would look for a background picture "
        "that it does not offer."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "Start %1 at (%2, %3) is outside the 6400 x 6400 pixel world."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "Start %1 has direction %2; the games treat anything but 1 as facing right."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "The title fills all 100 bytes without a terminator; the game copies it as a C "
        "string and would read past the field."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "The title fills all 100 bytes without a terminator."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "The title has %1 bytes; a player can type at most 40."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "Non-zero bytes follow the title's terminator; they are kept unchanged."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "This file is not revision 1.3, so the games do not read its stored title."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "%1 is not all zero; it is kept unchanged (demo editions use its first value as "
        "a marker)."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "%1 is not all zero; it is kept unchanged."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "dimDecor holds %1; the games only test it for zero, so it behaves like 100."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "%1 %2 cell(s) store %3: values below -1 are not tile numbers."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "%1 %2 cell(s) store %3: the chosen game's picture table has no such entry, so "
        "nothing is drawn."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "%1 %2 cell(s) store %3: the chosen game has a picture but no collision data "
        "for it and treats it as passable."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "%1 foreground cell(s) store 48 to 67; Speedy Blupi 2 shows them as 215 after "
        "loading. The file keeps the stored values."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "%1 foreground cell(s) store %2 where the game's editor would currently give a "
        "different border form (%3 at the first of them). This can be intentional."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "%1 (%2, %3) is outside the 6400 x 6400 pixel world."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "start and end differ but a travel duration is 0; the engines skip the movement "
        "when the duration is 0, so the object never arrives."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "type %1 (%2) is created by the game only while playing."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "type %1 is not an object type of the chosen game."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "channel %1 is not one the game's editor writes for %2."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "icon %1 has no entry in the chosen game's tile picture table, so nothing is "
        "drawn."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "All %1 object slots are in use. This is the format's hard limit: nothing can "
        "be added and effects the game creates while playing find no slot."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "Only %1 object slot(s) are free; fewer than %2 may leave no room for effects "
        "created while playing. This is an estimate, not a proven limit."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "%1 free slot(s) hold other non-zero bytes; they are kept unchanged until the "
        "slot is reused."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "The goal in slot %1 at (%2, %3) is in a cell that no chain of cells free of "
        "fully solid tiles connects to a start. Unless the game changes the map while "
        "playing (a block destroyed, a crate pushed), it cannot be collected. Jumps, "
        "falls and lifts are not judged."),
    QT_TRANSLATE_NOOP("eggbert::Doctor",
        "The treasure in slot %1 at (%2, %3) is in a cell that no chain of cells free "
        "of fully solid tiles connects to a start. Unless the game changes the map "
        "while playing (a block destroyed, a crate pushed), it cannot be collected. "
        "Jumps, falls and lifts are not judged."),
};

// Keep in step with the fact patterns in src/world/encyclopedia.cpp.
constexpr const char *factTexts[] = {
    QT_TRANSLATE_NOOP("eggbert::Encyclopedia",
        "Foreground tile %1"),
    QT_TRANSLATE_NOOP("eggbert::Encyclopedia",
        "Foreground tiles %1"),
    QT_TRANSLATE_NOOP("eggbert::Encyclopedia",
        "Back layer (bigDecor) tile %1"),
    QT_TRANSLATE_NOOP("eggbert::Encyclopedia",
        "Back layer (bigDecor) tiles %1"),
    QT_TRANSLATE_NOOP("eggbert::Encyclopedia",
        "; the game's editor cycles through them in this order"),
    QT_TRANSLATE_NOOP("eggbert::Encyclopedia",
        "; reshaped to fit its neighbours (adaptive family %1)"),
    QT_TRANSLATE_NOOP("eggbert::Encyclopedia",
        "."),
    QT_TRANSLATE_NOOP("eggbert::Encyclopedia",
        "Object type %1 (%2); channel %3, icon %4; placed %5, %6 pixels into its "
        "cell"),
    QT_TRANSLATE_NOOP("eggbert::Encyclopedia",
        "; travels between its start and its end, %1 tick(s) per cell out and %2 "
        "back"),
    QT_TRANSLATE_NOOP("eggbert::Encyclopedia",
        ", waiting %1 tick(s) at the start and %2 at the end"),
    QT_TRANSLATE_NOOP("eggbert::Encyclopedia",
        "; the stored icon is a tile number of your choice"),
    QT_TRANSLATE_NOOP("eggbert::Encyclopedia",
        "Border family painted from tile %1; its own cells keep their value and "
        "border tiles are written into the empty cells around them."),
    QT_TRANSLATE_NOOP("eggbert::Encyclopedia",
        "Border family painted from tile %1; each cell takes the form that fits its "
        "four neighbours."),
};

const char *const slotPrefix = QT_TRANSLATE_NOOP("eggbert::Doctor", "Slot %1: %2");

QString translated(std::string_view text) {
    const std::string source(text);
    return QCoreApplication::translate("eggbert::Doctor", source.c_str());
}

// All placeholders at once, so an argument that contains a percent sign is
// never taken for one.
QString filled(const QString &pattern, const QStringList &arguments) {
    QString text;
    text.reserve(pattern.size() + 16);
    for (qsizetype index = 0; index < pattern.size(); ++index) {
        const auto digit = index + 1 < pattern.size() ? pattern[index + 1] : QChar();
        if (pattern[index] == QLatin1Char('%') && digit >= QLatin1Char('1') &&
            digit <= QLatin1Char('9')) {
            const auto number = digit.unicode() - u'1';
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

} // namespace

QString findingText(const Finding &finding) {
    if (finding.messagePattern.empty()) {
        return QString::fromStdString(finding.message);
    }
    QStringList arguments;
    for (const auto &argument : finding.messageArguments) {
        // Numbers and field names have no translation and stay as they are.
        arguments << catalogText(argument);
    }
    const auto sentence = filled(translated(finding.messagePattern), arguments);
    if (!finding.slotPrefix || !finding.slot) {
        return sentence;
    }
    return filled(translated(slotPrefix),
                  {QString::number(static_cast<qulonglong>(*finding.slot)), sentence});
}

QString confidenceText(Confidence confidence) {
    switch (confidence) {
    case Confidence::structural:
        return QCoreApplication::translate("eggbert::Doctor", "structural fact");
    case Confidence::provenLimit:
        return QCoreApplication::translate("eggbert::Doctor", "read in the game's source");
    case Confidence::verifiedRange:
        return QCoreApplication::translate("eggbert::Doctor",
                                           "outside the game's verified range");
    case Confidence::reported:
        return QCoreApplication::translate("eggbert::Doctor", "reported, not re-checked");
    case Confidence::estimate:
        return QCoreApplication::translate("eggbert::Doctor", "estimate");
    }
    return {};
}

std::span<const char *const> doctorTexts() {
    return texts;
}

QString encyclopediaFactsText(const EncyclopediaEntry &entry) {
    if (entry.factParts.empty()) {
        return QString::fromStdString(entry.facts);
    }
    QString sentence;
    for (const auto &part : entry.factParts) {
        QStringList arguments;
        for (const auto &argument : part.arguments) {
            arguments << QString::fromStdString(argument);
        }
        const std::string pattern(part.pattern);
        sentence += filled(
            QCoreApplication::translate("eggbert::Encyclopedia", pattern.c_str()), arguments);
    }
    return sentence;
}

std::span<const char *const> encyclopediaTexts() {
    return factTexts;
}

} // namespace eggbert
