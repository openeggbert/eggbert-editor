// SPDX-License-Identifier: GPL-3.0-only
#include "main_window.hpp"

#include <QAction>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>

#include <vector>

#include "probe_report.hpp"
#include "world/world_file.hpp"
#include "world/world_probe.hpp"

namespace eggbert {

MainWindow::MainWindow() {
    setWindowTitle(tr("Eggbert Editor"));
    resize(760, 520);

    auto *details = new QLabel(tr("Choose a world file to probe its structure. Nothing is modified."), this);
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
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            details->setText(tr("This file is unavailable."));
            return;
        }
        // A world is about 50 KiB; refuse to load something absurdly large
        // into memory just to probe it.
        constexpr qint64 inspectionLimit = 64 * 1024 * 1024;
        if (file.size() > inspectionLimit) {
            details->setText(tr("%1\n%2 bytes\nToo large to inspect as a world file.")
                                 .arg(QFileInfo(path).fileName(), QString::number(file.size())));
            return;
        }
        const QByteArray contents = file.readAll();
        if (file.error() != QFileDevice::NoError) {
            details->setText(tr("This file could not be read."));
            return;
        }
        const WorldFile world(std::vector<std::byte>(
            reinterpret_cast<const std::byte *>(contents.constData()),
            reinterpret_cast<const std::byte *>(contents.constData()) + contents.size()));
        details->setText(
            formatProbeReport(QFileInfo(path).fileName(), probeWorld(world.originalBytes())));
    });

    auto *quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);
}

} // namespace eggbert
