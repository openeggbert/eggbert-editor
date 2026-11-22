// SPDX-License-Identifier: GPL-3.0-only
#include "main_window.hpp"

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>

#include "world/layout.hpp"

namespace eggbert {

MainWindow::MainWindow() {
    setWindowTitle(tr("Eggbert Editor"));
    resize(760, 520);

    auto *details = new QLabel(tr("Choose a world file to inspect its size."), this);
    details->setAlignment(Qt::AlignCenter);
    details->setTextFormat(Qt::PlainText);
    details->setTextInteractionFlags(Qt::TextSelectableByMouse);
    setCentralWidget(details);

    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    auto *inspectAction = fileMenu->addAction(tr("&Inspect World File..."));
    inspectAction->setShortcut(QKeySequence::Open);
    connect(inspectAction, &QAction::triggered, this, [this, details] {
        const auto path = QFileDialog::getOpenFileName(
            this, tr("Inspect World File"), {},
            tr("Blupi world files (*.blp *.BLP *.xch *.XCH);;All files (*)"));
        if (path.isEmpty()) {
            return;
        }
        const QFileInfo file(path);
        if (!file.isFile() || !file.isReadable()) {
            details->setText(tr("This file is unavailable."));
            return;
        }
        const auto size = file.size();
        const auto sizeHint = size == static_cast<qint64>(v1_3::worldBytes)
            ? tr("Matches the canonical v1.3 file size.")
            : tr("Differs from the canonical v1.3 file size.");
        details->setText(tr("%1\n%2 bytes\n%3")
                             .arg(file.fileName(), QString::number(size), sizeHint));
    });

    auto *quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
}

} // namespace eggbert
