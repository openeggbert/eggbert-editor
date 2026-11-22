// SPDX-License-Identifier: GPL-3.0-only
#include "app/main_window.hpp"

#include <QApplication>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("openeggbert");
    QApplication::setApplicationName("Eggbert Editor");
    QApplication::setApplicationVersion(EGGBERT_EDITOR_VERSION);
    eggbert::MainWindow mainWindow;
    mainWindow.show();
    // Files named on the command line open in tabs.
    const auto arguments = QApplication::arguments();
    for (qsizetype index = 1; index < arguments.size(); ++index) {
        mainWindow.openPath(arguments.at(index));
    }
    return app.exec();
}
