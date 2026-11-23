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
#include "app/doctor_texts.hpp"
#include "app/main_window.hpp"

#include "world/adaptive.hpp"
#include "world/doctor.hpp"
#include "world/editor_groups.hpp"
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

    // English window first: the reference texts.
    QStringList englishMenus;
    {
        MainWindow window;
        for (auto *action : window.menuBar()->actions()) {
            englishMenus << action->text();
        }
        CHECK(englishMenus.contains("&File"));
        CHECK_EQ(window.findChild<QAction *>("fileOpen")->text(), QString("&Open World..."));
    }

    // With the catalogue installed the window is Czech where translated and
    // English elsewhere.
    QCoreApplication::installTranslator(&czech);
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
