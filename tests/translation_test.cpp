// SPDX-License-Identifier: GPL-3.0-only
//
// The run-time catalogue: it loads from the resources, keeps placeholders
// and accelerators intact, falls back to English for what is not
// translated, and turns the main window's menus Czech. Also an
// accessibility walk: every focusable control of the main window can be
// named by a screen reader.

#include "app/catalog_texts.hpp"
#include "app/catalog_translator.hpp"
#include "app/main_window.hpp"

#include "world/adaptive.hpp"
#include "world/editor_groups.hpp"
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
