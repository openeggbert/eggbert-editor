// SPDX-License-Identifier: GPL-3.0-only
#include "app/appearance.hpp"
#include "app/inspector_panel.hpp"
#include "app/main_window.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QFile>
#include <QMap>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace eggbert;
using namespace eggbert::test;

int main(int argc, char **argv) {
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setOrganizationName("eggbert-editor-tests");
    QApplication::setApplicationName("workspace-test");
    QSettings().clear();

    QTemporaryDir dir;
    auto bytes = blankWorld(fxRevision13);
    put16(bytes, 224, 99); // a region no game offers, for a Doctor finding
    put16(bytes, fxCellOffset(fxForegroundOffset, 3, 70), 5);
    const auto path = dir.filePath("private-folder-name") + "-world.blp";
    {
        QFile file(path);
        CHECK(file.open(QIODevice::WriteOnly));
        file.write(reinterpret_cast<const char *>(bytes.data()),
                   static_cast<qsizetype>(bytes.size()));
    }

    MainWindow window;
    window.prompts.error = [](const QString &, const QString &) {};
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };
    QString shownTitle;
    QString shownText;
    bool shownMarkdown = false;
    window.prompts.showDocument = [&](const QString &title, const QString &text, bool markdown) {
        shownTitle = title;
        shownText = text;
        shownMarkdown = markdown;
    };
    window.resize(1300, 950);
    window.show();
    CHECK(QTest::qWaitForWindowExposed(&window));
    CHECK(!window.windowIcon().isNull());
    CHECK(!applicationIcon().pixmap(64, 64).isNull());

    // Every command has a unique shortcut: an ambiguous one would disable both.
    {
        QMap<QString, QString> seen;
        for (const auto *item : window.findChildren<QAction *>()) {
            if (item->shortcut().isEmpty()) {
                continue;
            }
            const auto key = item->shortcut().toString();
            if (seen.contains(key)) {
                std::cerr << "shortcut " << key.toStdString() << " is used by "
                          << seen.value(key).toStdString() << " and "
                          << item->objectName().toStdString() << '\n';
            }
            CHECK(!seen.contains(key));
            seen.insert(key, item->objectName());
        }
        CHECK(seen.size() > 30);
    }

    // Help documents are embedded and shown.
    for (const char *name : {"user-guide.md", "format-notes.md", "NOTICE.md", "COPYING"}) {
        CHECK(MainWindow::resourceText(name).size() > 500);
    }
    window.action("helpGuide")->trigger();
    CHECK_EQ(shownTitle, QString("User Guide"));
    CHECK(shownMarkdown);
    CHECK(shownText.contains("# Eggbert Editor user guide"));
    window.action("helpFormat")->trigger();
    CHECK(shownText.contains("layer start + 2 × (100 × x + y)"));
    window.action("helpLicense")->trigger();
    CHECK(shownText.contains("GNU GENERAL PUBLIC LICENSE"));
    CHECK(shownText.contains("Qt 6"));
    CHECK(!shownMarkdown);
    window.action("helpAbout")->trigger();
    CHECK(shownText.contains(QString("Eggbert Editor %1").arg(EGGBERT_EDITOR_VERSION)));
    CHECK(shownText.contains("no warranty"));
    window.action("helpShortcuts")->trigger();
    CHECK(shownText.contains("Save"));
    CHECK(shownText.contains("Ctrl+S"));
    CHECK(shownText.contains("Validate (World Doctor)"));

    // The guide only names commands that exist: each of these is an action
    // the guide refers to.
    {
        const auto guide = MainWindow::resourceText("user-guide.md");
        const std::pair<const char *, const char *> named[] = {
            {"Open World", "fileOpen"},
            {"Document Properties", "fileProperties"},
            {"Export World PNG", "fileExportPng"},
            {"Fit Selection", "viewFitSelection"},
            {"Fit World", "viewFitWorld"},
            {"Actual Pixels", "viewActualPixels"},
            {"Binary Inspector", "viewInspector"},
            {"Reset Layout", "viewResetLayout"},
            {"Validate", "worldValidate"},
            {"Compare with Original", "worldCompareOriginal"},
            {"Compare with Saved", "worldCompareSaved"},
            {"Metadata", "worldMetadata"},
            {"Player Starts", "worldStarts"},
            {"Copy Selected Start to All Four", "worldCopyStart"},
            {"Replace Value", "layerReplace"},
            {"Fill Selection", "layerFillSelection"},
            {"Clear Selection", "layerClearSelection"},
            {"Isolate Active Layer", "layerIsolate"},
            {"Duplicate", "objectDuplicate"},
            {"Delete Object", "objectDelete"},
            {"Diagnostics Bundle", "helpDiagnosticsBundle"},
            {"Find", "editFind"},
            {"Save All", "fileSaveAll"},
            {"Revert", "fileRevert"},
        };
        for (const auto &[text, name] : named) {
            CHECK(guide.contains(QLatin1String(text)));
            auto *item = window.action(QLatin1String(name));
            CHECK(item != nullptr);
            if (item != nullptr) {
                CHECK(item->text().remove('&').contains(QLatin1String(text)));
            }
        }
        for (const char *tool : {"toolBrowse", "toolSelect", "toolPencil", "toolEraser",
                                 "toolEyedropper", "toolLine", "toolRectangle", "toolFill",
                                 "toolAdaptive", "toolStart", "toolObject", "toolPlaceObject"}) {
            auto *item = window.action(tool);
            CHECK(item != nullptr);
            if (item != nullptr) {
                CHECK(guide.contains(item->text()));
            }
        }
    }

    // Themes: applied, checked in the menu and remembered.
    const auto systemWindow = QApplication::palette().color(QPalette::Window);
    window.action("viewThemeDark")->trigger();
    CHECK(currentTheme() == Theme::dark);
    CHECK(QApplication::palette().color(QPalette::Window).lightness() < 100);
    CHECK(window.action("viewThemeDark")->isChecked());
    CHECK_EQ(QSettings().value("appearance/theme").toString(), QString("dark"));
    window.action("viewThemeLight")->trigger();
    CHECK(QApplication::palette().color(QPalette::Window).lightness() > 200);
    window.action("viewThemeSystem")->trigger();
    CHECK(currentTheme() == Theme::system);
    CHECK(QApplication::palette().color(QPalette::Window) == systemWindow);
    CHECK(themeFromKey("nonsense") == Theme::system);

    // Preferences: stored values are applied to the inspector and new tabs.
    window.prompts.editPreferences = [] {
        QSettings settings;
        settings.setValue("appearance/theme", "dark");
        settings.setValue("view/grid", true);
        settings.setValue("view/labels", false);
        settings.setValue("inspector/bytesPerRow", 32);
        return true;
    };
    window.action("editPreferences")->trigger();
    CHECK(currentTheme() == Theme::dark);
    CHECK_EQ(window.inspector()->currentView()->bytesPerRow(), 32);

    auto *tab = window.openPath(path);
    CHECK(tab != nullptr);
    tab->setProfile(GameProfile::speedyBlupi1);
    QApplication::processEvents();
    CHECK(tab->canvas()->renderOptions().showGrid);
    CHECK(!tab->canvas()->renderOptions().showLabels);

    // Layouts.
    const auto visible = [&](const char *name) {
        return window.findChild<QDockWidget *>(name)->isVisible();
    };
    window.action("viewLayoutValidation")->trigger();
    CHECK(visible("diagnosticsDock"));
    CHECK(visible("inspectorDock"));
    CHECK(visible("statisticsDock"));
    CHECK(!visible("minimapDock"));
    CHECK(!visible("objectsDock"));
    window.action("viewLayoutObjects")->trigger();
    CHECK(visible("objectsDock"));
    CHECK(visible("minimapDock"));
    CHECK(!visible("diagnosticsDock"));
    CHECK(!visible("statisticsDock"));
    window.action("viewLayoutMapping")->trigger();
    CHECK(visible("minimapDock"));
    CHECK(visible("statisticsDock"));
    CHECK(!visible("objectsDock"));
    CHECK(!visible("inspectorDock"));
    window.action("viewResetLayout")->trigger();
    CHECK(visible("minimapDock"));
    CHECK(!visible("inspectorDock"));

    // Fit Selection: needs a selection or a selected object.
    CHECK(!window.action("viewFitSelection")->isEnabled());
    CHECK(!window.fitSelection());
    tab->setSelection(CellMask::fromRect({{3, 70}, {6, 72}}));
    CHECK(window.action("viewFitSelection")->isEnabled());
    tab->canvas()->actualPixels();
    window.action("viewFitSelection")->trigger();
    {
        const auto shown = tab->canvas()->visibleWorldRect();
        CHECK(shown.contains(QRectF(3 * 64, 70 * 64, 4 * 64, 3 * 64)));
        CHECK(tab->canvas()->zoom() > 1.0); // a small selection fills the view
    }

    // Diagnostics bundle: versions, findings and changes; the file name but
    // not its folder.
    tab->setTileId(9);
    tab->selectNone();
    CHECK(tab->pencilAt({3, 70}, 0));
    {
        const auto bundle = MainWindow::diagnosticsBundleText(tab);
        CHECK(bundle.contains(QString("Editor version: %1").arg(EGGBERT_EDITOR_VERSION)));
        CHECK(bundle.contains("Qt: built with"));
        CHECK(bundle.contains("Revision field: 1.3"));
        CHECK(bundle.contains("HDR-REGION-RANGE"));
        CHECK(bundle.contains("foreground[3][70]"));
        CHECK(bundle.contains("-world.blp"));
        CHECK(!bundle.contains(dir.path()));
        CHECK(MainWindow::diagnosticsBundleText(nullptr).contains("No document is open."));
        const auto bundlePath = dir.filePath("bundle.txt");
        window.prompts.saveTextPath = [&](const QString &) { return bundlePath; };
        window.action("helpDiagnosticsBundle")->trigger();
        QFile saved(bundlePath);
        CHECK(saved.open(QIODevice::ReadOnly));
        CHECK(QString::fromUtf8(saved.readAll()) == bundle);
        CHECK(!window.saveDiagnosticsBundle(bundlePath + "/nope.txt"));
    }

    window.setTheme("system");
    CHECK(window.close());
    return test::finish("workspace");
}
