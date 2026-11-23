// SPDX-License-Identifier: GPL-3.0-only
#include "app/catalog_translator.hpp"
#include "app/cli.hpp"
#include "app/main_window.hpp"
#include "app/smoke_test.hpp"

#include <QSettings>
#include <QLibraryInfo>
#include <QLocale>
#include <QTranslator>
#include <QApplication>
#include <QTextStream>

#include <cstdio>
#include <string_view>

int main(int argc, char **argv) {
    // Informational options work without a display, so they are handled
    // before the application object exists.
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--version") {
            std::printf("Eggbert Editor %s\n", EGGBERT_EDITOR_VERSION);
            return 0;
        }
        if (argument == "--help") {
            std::printf("Usage: eggbert-editor [world files...]\n"
                        "       eggbert-editor --version\n"
                        "       eggbert-editor --smoke-test   (self check, needs no game data)\n"
                        "       eggbert-editor --smoke-test --game-data DIR\n"
                        "                      (self check that also draws with the game in DIR;\n"
                        "                       DIR is only read)\n\n%s",
                        eggbert::commandLineUsage().toLocal8Bit().constData());
            return 0;
        }
    }
    // Headless commands: no window, and a display only for the one that
    // draws (the offscreen platform unless another is asked for).
    if (argc > 1 && eggbert::isCommandLineCommand(QString::fromLocal8Bit(argv[1]))) {
        QStringList arguments;
        for (int index = 1; index < argc; ++index) {
            arguments << QString::fromLocal8Bit(argv[index]);
        }
        QTextStream out(stdout);
        QTextStream err(stderr);
        if (eggbert::commandNeedsGraphics(arguments[0])) {
            if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
                qputenv("QT_QPA_PLATFORM", "offscreen");
            }
            QGuiApplication graphics(argc, argv);
            return eggbert::runCommandLine(arguments, out, err);
        }
        QCoreApplication core(argc, argv);
        return eggbert::runCommandLine(arguments, out, err);
    }

    QApplication app(argc, argv);
    QApplication::setOrganizationName("openeggbert");
    QApplication::setApplicationName("Eggbert Editor");
    QApplication::setApplicationVersion(EGGBERT_EDITOR_VERSION);
    QApplication::setDesktopFileName("eggbert-editor");

    const auto arguments = QApplication::arguments();
    if (arguments.contains(QStringLiteral("--smoke-test"))) {
        // Keep the self check away from the user's settings.
        QApplication::setOrganizationName("eggbert-editor-smoke-test");
        const auto option = arguments.indexOf(QStringLiteral("--game-data"));
        if (option >= 0 && option + 1 >= arguments.size()) {
            std::fprintf(stderr, "--game-data needs a directory\n");
            return 2;
        }
        return eggbert::runSmokeTest(option >= 0 ? arguments.at(option + 1) : QString());
    }

    // The interface language: the preference, else the system's, else
    // English. Untranslated texts stay English.
    // Qt's own translation of its widgets, where it is installed; the
    // editor's catalogue, installed after it, is asked first.
    QTranslator qtTranslator;
    eggbert::CatalogTranslator translator;
    if (const auto language = eggbert::resolveLanguage(
            QSettings().value(QStringLiteral("appearance/language"), QStringLiteral("system"))
                .toString(),
            QLocale::system().name());
        language != QLatin1String("en") && translator.loadLanguage(language)) {
        if (qtTranslator.load(QStringLiteral("qtbase_") + language,
                              QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
            QCoreApplication::installTranslator(&qtTranslator);
        }
        QCoreApplication::installTranslator(&translator);
    }
    eggbert::MainWindow mainWindow;
    mainWindow.show();
    // Files named on the command line open in tabs.
    for (qsizetype index = 1; index < arguments.size(); ++index) {
        if (!arguments.at(index).startsWith(QLatin1String("--"))) {
            mainWindow.openPath(arguments.at(index));
        }
    }
    // Unsaved changes kept from a session that did not end normally.
    mainWindow.checkRecovery();
    return app.exec();
}
