// SPDX-License-Identifier: GPL-3.0-only
//
// The run-time catalogue: it loads from the resources, keeps placeholders
// and accelerators intact, falls back to English for what is not
// translated, and turns the main window's menus Czech. Also an
// accessibility walk: every focusable control of the main window can be
// named by a screen reader.

#include "app/catalog_texts.hpp"
#include "app/catalog_translator.hpp"
#include "app/cli.hpp"
#include "app/conversion_texts.hpp"
#include "app/core_texts.hpp"
#include "app/doctor_texts.hpp"
#include "app/main_window.hpp"

#include "world/adaptive.hpp"
#include "world/doctor.hpp"
#include "world/editor_groups.hpp"
#include "world/encyclopedia.hpp"
#include "world/text_world.hpp"
#include "world/scenario.hpp"
#include "world/revision_convert.hpp"
#include "world/merge.hpp"
#include "world/demo_recording.hpp"
#include "world/campaign.hpp"
#include "world/game_convert.hpp"
#include "world/installation.hpp"
#include "world/object_catalog.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QRegularExpression>
#include <QSet>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTreeWidget>

using namespace eggbert;

namespace {

QStringList placeholders(const QString &text) {
    QStringList found;
    static const QRegularExpression pattern(QStringLiteral("%(n|[1-9])"));
    auto matches = pattern.globalMatch(text);
    while (matches.hasNext()) {
        found << matches.next().captured(0);
    }
    found.sort();
    found.removeDuplicates();
    return found;
}

int accelerators(QString text) {
    text.remove(QStringLiteral("&&"));
    return static_cast<int>(text.count(QLatin1Char('&')));
}

} // namespace

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("eggbert-editor-test");
    QCoreApplication::setApplicationName("translation");
    QSettings().clear();

    // Language choice.
    CHECK(availableLanguages() == QStringList{"cs"});
    CHECK_EQ(resolveLanguage("system", "cs_CZ"), QString("cs"));
    CHECK_EQ(resolveLanguage("", "cs_CZ"), QString("cs"));
    CHECK_EQ(resolveLanguage("system", "de_DE"), QString("en"));
    CHECK_EQ(resolveLanguage("en", "cs_CZ"), QString("en"));
    CHECK_EQ(resolveLanguage("cs", "en_US"), QString("cs"));
    CHECK_EQ(resolveLanguage("xx", "en_US"), QString("en"));
    // Plural forms.
    CHECK_EQ(CatalogTranslator::pluralForm("cs", 1), 0);
    CHECK_EQ(CatalogTranslator::pluralForm("cs", 2), 1);
    CHECK_EQ(CatalogTranslator::pluralForm("cs", 4), 1);
    CHECK_EQ(CatalogTranslator::pluralForm("cs", 5), 2);
    CHECK_EQ(CatalogTranslator::pluralForm("cs", 0), 2);
    CHECK_EQ(CatalogTranslator::pluralForm("en", 1), 0);
    CHECK_EQ(CatalogTranslator::pluralForm("en", 7), 1);

    // A catalogue from text.
    {
        CatalogTranslator small;
        CHECK(small.isEmpty());
        CHECK(small.loadCatalog(
            "<?xml version=\"1.0\"?><TS version=\"2.1\" language=\"cs_CZ\"><context>"
            "<name>C</name>"
            "<message><source>Open</source><translation>Otev\u0159\u00edt</translation></message>"
            "<message><source>Later</source><translation type=\"unfinished\">x</translation></message>"
            "<message><source>Blank</source><translation> </translation></message>"
            "<message numerus=\"yes\"><source>%n file(s)</source><translation>"
            "<numerusform>%n soubor</numerusform><numerusform>%n soubory</numerusform>"
            "<numerusform>%n soubor\u016f</numerusform></translation></message>"
            "</context></TS>"));
        CHECK_EQ(small.messageCount(), 4);
        CHECK_EQ(small.translatedCount(), 2);
        CHECK_EQ(small.language(), QString("cs"));
        CHECK_EQ(small.translate("C", "Open"), QString::fromUtf8("Otev\u0159\u00edt"));
        CHECK(small.translate("C", "Later").isNull());   // unfinished: English stays
        CHECK(small.translate("C", "Blank").isNull());
        CHECK(small.translate("C", "Unknown").isNull());
        CHECK(small.translate("Other", "Open").isNull()); // the context matters
        CHECK_EQ(small.translate("C", "%n file(s)", nullptr, 1), QString("%n soubor"));
        CHECK_EQ(small.translate("C", "%n file(s)", nullptr, 3), QString("%n soubory"));
        CHECK_EQ(small.translate("C", "%n file(s)", nullptr, 12),
                 QString::fromUtf8("%n soubor\u016f"));
        // Damaged input leaves an empty catalogue.
        CHECK(!small.loadCatalog("<TS><context><name>C</name><message>"));
        CHECK(small.isEmpty());
        CHECK(!small.loadCatalog("plain text"));
        CHECK(!small.loadCatalog("<other/>"));
    }

    // Every catalog name the interface shows is marked for translation.
    {
        QSet<QString> marked;
        for (const auto *text : catalogTexts()) {
            marked.insert(QString::fromUtf8(text));
        }
        const auto covered = [&](std::string_view name) {
            const auto text = QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
            if (!text.isEmpty() && !marked.contains(text)) {
                std::fprintf(stderr, "catalog name not marked: %s\n", qPrintable(text));
                return false;
            }
            return true;
        };
        for (const auto profile : {GameProfile::speedyBlupi1, GameProfile::speedyBlupi2}) {
            for (const auto &group : editorGroups(profile)) {
                CHECK(covered(group.name));
                for (const auto &tile : group.tiles) {
                    CHECK(covered(tile.name));
                }
            }
            for (const auto &preset : objectPresets(profile)) {
                CHECK(covered(preset.name));
            }
            for (const auto &family : adaptiveFamilies(profile)) {
                CHECK(covered(family.name));
            }
            for (int type = 0; type < 256; ++type) {
                CHECK(covered(objectTypeInfo(profile, static_cast<std::int16_t>(type)).name));
            }
        }
        // The roles of the official world numbers are shown the same way.
        for (const auto role : {WorldRole::start, WorldRole::hub, WorldRole::mission,
                                WorldRole::finale, WorldRole::multiplayer, WorldRole::demo}) {
            CHECK(!worldRoleName(role).empty() && covered(worldRoleName(role)));
        }
        // Untranslated, a name is shown as it is.
        CHECK_EQ(catalogText("Treasure chest"), QString("Treasure chest"));
        CHECK_EQ(catalogText("no such name"), QString("no such name"));
    }

    // The shipped Czech catalogue.
    CatalogTranslator czech;
    CHECK(!czech.loadLanguage("xx"));
    CHECK(czech.loadLanguage("cs"));
    CHECK(czech.messageCount() >= 800);
    // The shipped catalogue has no unfinished entry (identical texts in
    // several contexts count once each, so compare with a small margin).
    CHECK(czech.translatedCount() >= czech.messageCount() - 5);
    for (const auto &message : czech.translatedMessages()) {
        const auto source = QString::fromUtf8(message.source);
        for (const auto &form : message.forms) {
            // The same placeholders, and an accelerator where the source has one.
            if (placeholders(form) != placeholders(source)) {
                std::fprintf(stderr, "placeholders differ: %s\n", message.source.constData());
            }
            CHECK(placeholders(form) == placeholders(source));
            if (accelerators(form) != accelerators(source)) {
                std::fprintf(stderr, "accelerators differ: %s\n", message.source.constData());
            }
            CHECK_EQ(accelerators(form), accelerators(source));
            CHECK(form.endsWith("...") == source.endsWith("..."));
        }
    }

    // World Doctor's sentences: every pattern the core can produce is marked
    // for translation, and without a catalogue a finding reads as the core
    // wrote it.
    eggbert::test::Bytes troubled = eggbert::test::blankWorld(eggbert::test::fxRevision13);
    {
        using namespace eggbert::test;
        put32(troubled, 212, 100);
        put32(troubled, 216, 100);
        put16(troubled, 222, 77); // a music no game offers
        put16(troubled, 224, 2);
        for (int index = 0; index < 4; ++index) {
            put32(troubled, 328 + 8 * index, 194);
            put32(troubled, 332 + 8 * index, 198);
            put32(troubled, 360 + 4 * index, 2);
        }
        const auto slot = fxSlotOffset(fxRevision13, 3);
        put16(troubled, slot, 5);      // a treasure chest
        put16(troubled, slot + 42, 9); // in a channel the games' editors do not write
        for (const std::size_t field : {12u, 20u, 28u}) {
            put32(troubled, slot + field, 640);
            put32(troubled, slot + field + 4, 640);
        }
    }
    const WorldView troubledView(troubled);
    const auto findings = runDoctor(troubledView, GameProfile::speedyBlupi1);
    const auto *music = findings.first("HDR-MUSIC-RANGE");
    const auto *channel = findings.first("OBJ-CHANNEL");
    CHECK(music != nullptr && channel != nullptr);
    {
        QSet<QString> marked;
        for (const auto *text : doctorTexts()) {
            marked.insert(QString::fromUtf8(text));
        }
        for (const auto pattern : doctorMessagePatterns()) {
            const auto text =
                QString::fromUtf8(pattern.data(), static_cast<qsizetype>(pattern.size()));
            if (!marked.contains(text)) {
                std::fprintf(stderr, "doctor pattern not marked: %s\n", qPrintable(text));
            }
            CHECK(marked.contains(text));
        }
        CHECK_EQ(marked.size(), static_cast<qsizetype>(doctorMessagePatterns().size()));
        for (const auto &finding : findings.findings) {
            CHECK_EQ(findingText(finding), QString::fromStdString(finding.message));
        }
        if (music != nullptr && channel != nullptr) {
            CHECK_EQ(findingText(*music), QString("Music 77 is outside 0 to 10."));
            CHECK(findingText(*channel).startsWith("Slot 3: channel 9 is not one"));
        }
        CHECK_EQ(confidenceText(Confidence::estimate), QString("estimate"));
    }

    // The notes of a conversion between the games: every sentence is marked,
    // and without a catalogue a line equals the command line's.
    eggbert::test::Bytes modern = eggbert::test::blankWorld(eggbert::test::fxRevision13);
    eggbert::test::put16(
        modern, eggbert::test::fxCellOffset(eggbert::test::fxForegroundOffset, 2, 1), 50);
    eggbert::test::put16(
        modern, eggbert::test::fxCellOffset(eggbert::test::fxForegroundOffset, 4, 1), 300);
    const WorldView modernView(modern);
    const auto notes = reviewGameConversion(modernView, GameProfile::speedyBlupi2,
                                            GameProfile::speedyBlupi1);
    CHECK_EQ(notes.size(), std::size_t{2});
    {
        QSet<QString> marked;
        for (const auto *text : conversionTexts()) {
            marked.insert(QString::fromUtf8(text));
        }
        for (const auto pattern : gameConversionPatterns()) {
            CHECK(marked.contains(
                QString::fromUtf8(pattern.data(), static_cast<qsizetype>(pattern.size()))));
        }
        CHECK_EQ(marked.size(), static_cast<qsizetype>(gameConversionPatterns().size()));
        for (const auto &note : notes) {
            CHECK_EQ(conversionNoteLine(note), describeGameNote(note));
        }
        CHECK(conversionNoteLine(notes[0]).startsWith("[unknown] foreground tile 300 (1 place(s), "
                                                      "first at cell 4,1): The first game has"));
        CHECK(conversionNoteLine(notes[1]).endsWith("draws its own block 50 here."));
    }

    // The encyclopedia's facts: every part's pattern is marked, and without
    // a catalogue the text is the core's.
    {
        QSet<QString> marked;
        for (const auto *text : encyclopediaTexts()) {
            marked.insert(QString::fromUtf8(text));
        }
        for (const auto pattern : encyclopediaFactPatterns()) {
            CHECK(marked.contains(
                QString::fromUtf8(pattern.data(), static_cast<qsizetype>(pattern.size()))));
        }
        CHECK_EQ(marked.size(), static_cast<qsizetype>(encyclopediaFactPatterns().size()));
        for (const auto &entry : encyclopedia(GameProfile::speedyBlupi2)) {
            CHECK_EQ(encyclopediaFactsText(entry), QString::fromStdString(entry.facts));
        }
    }

    // Other sentences of the core are looked up by their pattern: the
    // sentence with its numbers replaced by placeholders.
    {
        QStringList numbers;
        CHECK_EQ(corePattern("Hub 10 has none of its missions (11 to 18).", &numbers),
                 QString("Hub %1 has none of its missions (%2 to %3)."));
        CHECK(numbers == (QStringList{"10", "11", "18"}));
        // A minus sign belongs to a number only after a space, a parenthesis
        // or at the start.
        CHECK_EQ(corePattern("values below -1 and (-2), u001-002.blp, 204-byte", &numbers),
                 QString("values below %1 and (%2), u%3-%4.blp, %5-byte"));
        CHECK(numbers == (QStringList{"-1", "-2", "001", "002", "204"}));
        CHECK_EQ(corePattern("-7 first", &numbers), QString("%1 first"));
        CHECK_EQ(corePattern("no numbers"), QString("no numbers"));
        // Without a catalogue, and for sentences nobody listed, the text is
        // the core's own.
        for (const auto *sentence :
             {"Hub 10 has none of its missions (11 to 18).", "Something nobody listed 5.",
              "1 2 3 4 5 6 7 8 9 10 numbers are too many", ""}) {
            CHECK_EQ(coreText(sentence), QString(sentence));
        }
        // Every listed pattern is a pattern (numbering it again changes nothing).
        for (const auto *pattern : coreTexts()) {
            QString renumbered(QString::fromUtf8(pattern));
            for (int number = 1; number <= 9; ++number) {
                renumbered.replace(QStringLiteral("%%1").arg(number), QStringLiteral("0"));
            }
            CHECK_EQ(corePattern(renumbered.toStdString()), QString::fromUtf8(pattern));
        }
    }

    // The guide exists in Czech with the same headings and list items, so
    // the two stay in step.
    {
        const auto english = MainWindow::resourceText("user-guide.md");
        const auto czechGuide = MainWindow::resourceText("user-guide.cs.md");
        CHECK(!english.isEmpty() && !czechGuide.isEmpty());
        const auto count = [](const QString &text, const QString &start) {
            int found = 0;
            for (const auto &line : text.split(QLatin1Char('\n'))) {
                found += line.startsWith(start) ? 1 : 0;
            }
            return found;
        };
        CHECK_EQ(count(czechGuide, "#"), count(english, "#"));
        CHECK_EQ(count(czechGuide, "- "), count(english, "- "));
        CHECK_EQ(czechGuide.count("```"), english.count("```"));
        CHECK_EQ(czechGuide.split(QLatin1Char('\n')).size(), english.split(QLatin1Char('\n')).size());
        CHECK(czechGuide.startsWith(QString::fromUtf8("# U\u017eivatelsk\u00e1 p\u0159\u00edru\u010dka")));
    }

    // English window first: the reference texts.
    QStringList englishMenus;
    {
        MainWindow window;
        // Help shows the English guide.
        {
            QString shown;
            window.prompts.showDocument = [&](const QString &, const QString &text, bool) {
                shown = text;
            };
            window.findChild<QAction *>("helpGuide")->trigger();
            CHECK(shown.startsWith("# Eggbert Editor user guide"));
        }
        for (auto *action : window.menuBar()->actions()) {
            englishMenus << action->text();
        }
        CHECK(englishMenus.contains("&File"));
        CHECK_EQ(window.findChild<QAction *>("fileOpen")->text(), QString("&Open World..."));
    }

    // With the catalogue installed the window is Czech where translated and
    // English elsewhere.
    QCoreApplication::installTranslator(&czech);
    // Sentences the core really produces come out translated, with their
    // numbers in place.
    {
        CHECK_EQ(coreText("Hub 10 has none of its missions (11 to 18)."),
                 QString::fromUtf8("Rozcestn\u00edk 10 nem\u00e1 \u017e\u00e1dnou ze sv\u00fdch mis\u00ed (11 a\u017e 18)."));
        CHECK_EQ(coreText("Something nobody listed 5."), QString("Something nobody listed 5."));
        // A campaign with a hub but no missions, and a missing first world.
        Campaign campaign;
        campaign.game = GameProfile::speedyBlupi1;
        campaign.entries.push_back({WorldSlot{false, 0, 10}, "world010.blp", 0, {}, {}});
        campaign.entries.push_back({WorldSlot{false, 0, 55}, "world055.blp", 0, {}, {}});
        const auto found = reviewCampaign(campaign);
        CHECK(found.size() >= 2);
        for (const auto &finding : found) {
            CHECK(coreText(finding.text) != QString::fromStdString(finding.text));
            CHECK(coreText(finding.basis) != QString::fromUtf8(finding.basis.data()));
        }
        // A text world with shape problems and the labels of its undo steps.
        const auto broken = TextWorld::parse(
            "DescFile: music=x \nDecor: \n1,2\nMoveObject: type=0 \nMoveObject: type=5 \n");
        const auto problems = broken.problems();
        CHECK(problems.size() >= 6);
        for (const auto &problem : problems) {
            if (coreText(problem) == QString::fromStdString(problem)) {
                std::fprintf(stderr, "not translated: %s\n", problem.c_str());
            }
            CHECK(coreText(problem) != QString::fromStdString(problem));
        }
        for (const auto *label : {"Remove object 2", "Duplicate object 1", "Add object"}) {
            CHECK(coreText(label) != QString(label));
        }
        CHECK_EQ(coreText("BigDecor (0, 99)"), QString("BigDecor (0, 99)")); // a name and numbers
        // Revision conversion: what would be lost, and the notes.
        auto crowded = eggbert::test::blankWorld(eggbert::test::fxRevision13);
        eggbert::test::put16(
            crowded, eggbert::test::fxCellOffset(eggbert::test::fxRevision13.bigDecorOffset, 1, 1), 20);
        eggbert::test::put16(crowded, eggbert::test::fxSlotOffset(eggbert::test::fxRevision13, 150), 5);
        const auto refused = convertRevision(crowded, FormatRevision::v1_0, false);
        CHECK(!refused.has_value() && refused.error().sentences.size() == 2);
        if (!refused.has_value()) {
            for (const auto &sentence : refused.error().sentences) {
                CHECK(coreText(sentence) != QString::fromStdString(sentence));
            }
        }
        const auto upgraded = convertRevision(eggbert::test::blankWorld(eggbert::test::fxRevision10),
                                              FormatRevision::v1_3, false);
        CHECK(upgraded.has_value() && !upgraded->notes.empty());
        if (upgraded.has_value()) {
            for (const auto &note : upgraded->notes) {
                CHECK(coreText(note) != QString::fromStdString(note));
            }
        }
        // Merge, stamp and recording refusals.
        const eggbert::test::Bytes whole = eggbert::test::blankWorld(eggbert::test::fxRevision13);
        const eggbert::test::Bytes other = eggbert::test::blankWorld(eggbert::test::fxRevision12);
        auto longer = whole;
        longer.push_back(std::byte{0});
        for (const auto &error :
             {mergeWorlds(whole, whole, eggbert::test::Bytes(10), MergePreference::none).error(),
              mergeWorlds(whole, other, whole, MergePreference::none).error(),
              mergeWorlds(whole, longer, whole, MergePreference::none).error()}) {
            CHECK(coreText(error) != QString::fromStdString(error));
        }
        for (const auto *text :
             {"The stamp is empty or damaged.", "This world has no object records.",
              "The stamp has 3 object(s) but only 2 object slot(s) are free.",
              "An object of the stamp would lie outside the world here. Place the stamp further "
              "from the edge."}) {
            CHECK(coreText(text) != QString(text));
        }
        for (const auto &bad : {eggbert::test::Bytes(10), eggbert::test::Bytes(205)}) {
            const auto parsed = DemoRecording::parse(bad);
            CHECK(!parsed.has_value());
            if (!parsed.has_value()) {
                CHECK(coreText(parsed.error()) != QString::fromStdString(parsed.error()));
            }
        }
        const auto scenarioError = checkScenario("nonsense\n", {}).error();
        CHECK(coreText(scenarioError) != QString::fromStdString(scenarioError));
    }
    for (const auto &entry : encyclopedia(GameProfile::speedyBlupi1)) {
        const auto facts = encyclopediaFactsText(entry);
        CHECK(facts != QString::fromStdString(entry.facts));
        CHECK(!facts.contains('%') && facts.endsWith('.'));
    }
    CHECK(catalogText("group hub") != QString("group hub"));
    CHECK(catalogText("first world") != QString("first world"));
    for (const auto &note : notes) {
        CHECK(conversionNoteLine(note) != describeGameNote(note));
        CHECK(!conversionNoteLine(note).contains("%"));
    }
    if (notes.size() == 2) {
        CHECK(conversionNoteLine(notes[1]).contains(" 50 "));
        CHECK(conversionNoteLine(notes[0]).contains("4,1"));
    }
    // The same findings in Czech: the numbers in their places, the object's
    // catalog name translated, and the slot in front.
    if (music != nullptr && channel != nullptr) {
        CHECK_EQ(findingText(*music), QString::fromUtf8("Hudba 77 je mimo rozsah 0 a\u017e 10."));
        const auto czechChannel = findingText(*channel);
        CHECK(czechChannel.startsWith(QString::fromUtf8("Slot 3: kan\u00e1l 9 ")));
        CHECK(!czechChannel.contains("Treasure chest"));
        CHECK(czechChannel.contains(catalogText("Treasure chest")));
        CHECK(catalogText("Treasure chest") != QString("Treasure chest"));
        CHECK(confidenceText(Confidence::estimate) != QString("estimate"));
        for (const auto &finding : findings.findings) {
            CHECK(findingText(finding) != QString::fromStdString(finding.message));
        }
    }
    {
        MainWindow window;
        window.resize(1200, 800);
        window.show();
        QStringList menus;
        for (auto *action : window.menuBar()->actions()) {
            menus << action->text();
        }
        // Help shows the Czech guide.
        {
            QString shown;
            window.prompts.showDocument = [&](const QString &, const QString &text, bool) {
                shown = text;
            };
            window.findChild<QAction *>("helpGuide")->trigger();
            CHECK(shown.startsWith(QString::fromUtf8("# U\u017eivatelsk\u00e1 p\u0159\u00edru\u010dka")));
        }
        CHECK_EQ(menus.size(), englishMenus.size());
        for (qsizetype index = 0; index < menus.size(); ++index) {
            CHECK(menus[index] != englishMenus[index]); // every menu title is translated
        }
        CHECK(menus.contains("&Soubor"));
        CHECK_EQ(window.findChild<QAction *>("fileOpen")->text(),
                 QString::fromUtf8("&Otev\u0159\u00edt sv\u011bt..."));
        CHECK_EQ(window.findChild<QAction *>("viewInstallation")->text(), QString("&Instalace"));
        CHECK_EQ(QCoreApplication::translate("eggbert::MainWindow", "%n world(s) changed", nullptr,
                                             3),
                 QString::fromUtf8("Zm\u011bn\u011bny 3 sv\u011bty"));
        // Something not translated yet stays as written.
        CHECK_EQ(QCoreApplication::translate("eggbert::MainWindow", "No such text anywhere"),
                 QString("No such text anywhere"));

        // Accessibility: every focusable control has a name a screen reader
        // can speak (its own text, an accessible name or a tool tip).
        int unnamed = 0;
        for (auto *widget : window.findChildren<QWidget *>()) {
            if (widget->focusPolicy() == Qt::NoFocus) {
                continue;
            }
            const bool control =
                qobject_cast<QAbstractButton *>(widget) != nullptr ||
                qobject_cast<QComboBox *>(widget) != nullptr ||
                qobject_cast<QLineEdit *>(widget) != nullptr ||
                qobject_cast<QAbstractSpinBox *>(widget) != nullptr ||
                qobject_cast<QSlider *>(widget) != nullptr ||
                qobject_cast<QListWidget *>(widget) != nullptr ||
                qobject_cast<QTreeWidget *>(widget) != nullptr;
            // Parts of composite widgets (the line edit inside a spin box or
            // an editable combo box) are named by their owner.
            if (!control || qobject_cast<QAbstractSpinBox *>(widget->parentWidget()) != nullptr ||
                qobject_cast<QComboBox *>(widget->parentWidget()) != nullptr ||
                widget->objectName().startsWith("qt_")) {
                continue;
            }
            QString name = widget->accessibleName();
            if (name.isEmpty()) {
                if (const auto *button = qobject_cast<QAbstractButton *>(widget)) {
                    name = button->text();
                }
            }
            if (name.isEmpty()) {
                name = widget->toolTip();
            }
            if (name.isEmpty()) {
                if (const auto *edit = qobject_cast<QLineEdit *>(widget)) {
                    name = edit->placeholderText();
                }
            }
            if (name.isEmpty()) {
                ++unnamed;
                std::fprintf(stderr, "unnamed control: %s (%s)\n",
                             qPrintable(widget->objectName()), widget->metaObject()->className());
            }
        }
        CHECK_EQ(unnamed, 0);
    }
    QCoreApplication::removeTranslator(&czech);
    QSettings().clear();
    return test::finish("translation");
}
