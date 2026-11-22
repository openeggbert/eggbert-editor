// SPDX-License-Identifier: GPL-3.0-only
#include "app/main_window.hpp"
#include "app/smoke_test.hpp"

#include <QApplication>

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
                        "       eggbert-editor --smoke-test   (self check, needs no game data)\n");
            return 0;
        }
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
        return eggbert::runSmokeTest();
    }

    eggbert::MainWindow mainWindow;
    mainWindow.show();
    // Files named on the command line open in tabs.
    for (qsizetype index = 1; index < arguments.size(); ++index) {
        if (!arguments.at(index).startsWith(QLatin1String("--"))) {
            mainWindow.openPath(arguments.at(index));
        }
    }
    return app.exec();
}
