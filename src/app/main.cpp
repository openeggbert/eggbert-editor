// SPDX-License-Identifier: GPL-3.0-only
#include "main_window.hpp"

#include <QApplication>

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Eggbert Editor");
    eggbert::MainWindow mainWindow;
    mainWindow.show();
    return app.exec();
}
