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
                        "       eggbert-editor --smoke-test   (self check, needs no game data)\n"
                        "       eggbert-editor --smoke-test --game-data DIR\n"
                        "                      (self check that also draws with the game in DIR;\n"
                        "                       DIR is only read)\n");
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
        const auto option = arguments.indexOf(QStringLiteral("--game-data"));
        if (option >= 0 && option + 1 >= arguments.size()) {
            std::fprintf(stderr, "--game-data needs a directory\n");
            return 2;
        }
        return eggbert::runSmokeTest(option >= 0 ? arguments.at(option + 1) : QString());
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
