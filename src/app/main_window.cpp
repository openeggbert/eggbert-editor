// SPDX-License-Identifier: GPL-3.0-only
#include "main_window.hpp"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>

#include <QDockWidget>

#include "app/minimap_widget.hpp"
#include "app/probe_report.hpp"
#include "app/statistics_panel.hpp"
#include "world/offset_map.hpp"

namespace eggbert {

namespace {

constexpr int maximumRecentFiles = 10;
const char *const worldFilter =
    QT_TRANSLATE_NOOP("eggbert::MainWindow",
                      "Blupi world files (*.blp *.BLP *.xch *.XCH);;All files (*)");

QString samePathKey(const QString &path) {
    const QFileInfo info(path);
    return info.exists() ? info.canonicalFilePath() : info.absoluteFilePath();
}

} // namespace

MainWindow::MainWindow() {
    setObjectName(QStringLiteral("mainWindow"));
    setWindowTitle(tr("Eggbert Editor"));
    resize(1100, 760);

    tabs_ = new QTabWidget(this);
    tabs_->setDocumentMode(true);
    tabs_->setTabsClosable(true);
    tabs_->setMovable(true);
    setCentralWidget(tabs_);
    connect(tabs_, &QTabWidget::tabCloseRequested, this,
            [this](int index) { closeTab(tabAt(index)); });
    connect(tabs_, &QTabWidget::currentChanged, this, [this] { updateUi(); });

    cellLabel_ = new QLabel(this);
    zoomLabel_ = new QLabel(this);
    accessLabel_ = new QLabel(this);
    statusBar()->addWidget(cellLabel_, 1);
    statusBar()->addPermanentWidget(accessLabel_);
    statusBar()->addPermanentWidget(zoomLabel_);

    installDefaultPrompts();
    createActions();

    defaultState_ = saveState();
    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());
    restoreState(settings.value(QStringLiteral("window/state")).toByteArray());
    rebuildRecentMenu();
    updateUi();
}

int MainWindow::tabCount() const {
    return tabs_->count();
}

DocumentTab *MainWindow::tabAt(int index) const {
    return qobject_cast<DocumentTab *>(tabs_->widget(index));
}

DocumentTab *MainWindow::currentTab() const {
    return qobject_cast<DocumentTab *>(tabs_->currentWidget());
}

QAction *MainWindow::action(const QString &objectName) const {
    return findChild<QAction *>(objectName);
}

QAction *MainWindow::addAction(QMenu *menu, const QString &objectName, const QString &text,
                               const QKeySequence &shortcut) {
    auto *result = new QAction(text, this);
    result->setObjectName(objectName);
    if (!shortcut.isEmpty()) {
        result->setShortcut(shortcut);
    }
    menu->addAction(result);
    return result;
}

void MainWindow::installDefaultPrompts() {
    prompts.openPaths = [this] {
        return QFileDialog::getOpenFileNames(this, tr("Open World"), {}, tr(worldFilter));
    };
    prompts.saveAsPath = [this](const QString &suggested) {
        return QFileDialog::getSaveFileName(this, tr("Save World As"), suggested,
                                            tr(worldFilter));
    };
    prompts.unsavedChanges = [this](DocumentTab *tab) {
        QMessageBox box(QMessageBox::Warning, tr("Unsaved Changes"),
                        tr("%1 has unsaved changes.")
                            .arg(QFileInfo(tab->file().path()).fileName()),
                        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
        box.setDefaultButton(QMessageBox::Save);
        switch (box.exec()) {
        case QMessageBox::Save:
            return UnsavedChoice::save;
        case QMessageBox::Discard:
            return UnsavedChoice::discard;
        default:
            return UnsavedChoice::cancel;
        }
    };
    prompts.saveConflict = [this](DocumentTab *tab, const SaveResult &result) {
        QMessageBox box(QMessageBox::Warning, tr("File Changed on Disk"),
                        tr("%1 was not saved.\n%2")
                            .arg(QFileInfo(tab->file().path()).fileName(), result.detail),
                        QMessageBox::Cancel, this);
        box.setInformativeText(
            tr("Reload discards your %n unsaved byte range(s) and shows the file on disk. "
               "Save As keeps your version in another file. Nothing has been written.",
               nullptr, static_cast<int>(tab->document().unsavedRanges().size())));
        auto *reload = result.diskState == DiskState::modified
                           ? box.addButton(tr("Reload from Disk"), QMessageBox::DestructiveRole)
                           : nullptr;
        auto *saveAs = box.addButton(tr("Save As..."), QMessageBox::AcceptRole);
        box.exec();
        if (box.clickedButton() == saveAs) {
            return ConflictChoice::saveAs;
        }
        if (reload != nullptr && box.clickedButton() == reload) {
            return ConflictChoice::reload;
        }
        return ConflictChoice::cancel;
    };
    prompts.confirmRevert = [this](DocumentTab *tab) {
        return QMessageBox::question(
                   this, tr("Revert"),
                   tr("Discard all unsaved changes to %1 and reload it from disk?")
                       .arg(QFileInfo(tab->file().path()).fileName())) == QMessageBox::Yes;
    };
    prompts.error = [this](const QString &title, const QString &text) {
        QMessageBox::warning(this, title, text);
    };
}

void MainWindow::createActions() {
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    auto *open = addAction(fileMenu, QStringLiteral("fileOpen"), tr("&Open World..."),
                           QKeySequence::Open);
    connect(open, &QAction::triggered, this, [this] {
        for (const auto &path : prompts.openPaths()) {
            openPath(path);
        }
    });
    recentMenu_ = fileMenu->addMenu(tr("Open &Recent"));
    fileMenu->addSeparator();
    connect(addAction(fileMenu, QStringLiteral("fileSave"), tr("&Save"), QKeySequence::Save),
            &QAction::triggered, this, [this] { saveTab(currentTab()); });
    connect(addAction(fileMenu, QStringLiteral("fileSaveAs"), tr("Save &As..."),
                      QKeySequence::SaveAs),
            &QAction::triggered, this, [this] { saveTabAs(currentTab()); });
    connect(addAction(fileMenu, QStringLiteral("fileSaveAll"), tr("Save A&ll"),
                      QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_S)),
            &QAction::triggered, this, [this] { saveAll(); });
    connect(addAction(fileMenu, QStringLiteral("fileRevert"), tr("Re&vert")),
            &QAction::triggered, this, [this] { revertTab(currentTab()); });
    fileMenu->addSeparator();
    connect(addAction(fileMenu, QStringLiteral("fileProperties"), tr("Document &Properties...")),
            &QAction::triggered, this, [this] { showProperties(); });
    fileMenu->addSeparator();
    connect(addAction(fileMenu, QStringLiteral("fileClose"), tr("&Close"), QKeySequence::Close),
            &QAction::triggered, this, [this] { closeTab(currentTab()); });
    connect(addAction(fileMenu, QStringLiteral("fileQuit"), tr("&Quit"), QKeySequence::Quit),
            &QAction::triggered, this, &QWidget::close);

    auto *editMenu = menuBar()->addMenu(tr("&Edit"));
    connect(addAction(editMenu, QStringLiteral("editUndo"), tr("&Undo"), QKeySequence::Undo),
            &QAction::triggered, this, [this] {
                if (auto *tab = currentTab()) {
                    tab->undo();
                }
            });
    connect(addAction(editMenu, QStringLiteral("editRedo"), tr("&Redo"), QKeySequence::Redo),
            &QAction::triggered, this, [this] {
                if (auto *tab = currentTab()) {
                    tab->redo();
                }
            });

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    const auto canvasCommand = [this](void (WorldCanvas::*command)()) {
        return [this, command] {
            if (auto *tab = currentTab()) {
                (tab->canvas()->*command)();
            }
        };
    };
    connect(addAction(viewMenu, QStringLiteral("viewZoomIn"), tr("Zoom &In"),
                      QKeySequence::ZoomIn),
            &QAction::triggered, this, canvasCommand(&WorldCanvas::zoomIn));
    connect(addAction(viewMenu, QStringLiteral("viewZoomOut"), tr("Zoom &Out"),
                      QKeySequence::ZoomOut),
            &QAction::triggered, this, canvasCommand(&WorldCanvas::zoomOut));
    connect(addAction(viewMenu, QStringLiteral("viewActualPixels"), tr("&Actual Pixels"),
                      QKeySequence(Qt::CTRL | Qt::Key_1)),
            &QAction::triggered, this, canvasCommand(&WorldCanvas::actualPixels));
    connect(addAction(viewMenu, QStringLiteral("viewFitWorld"), tr("&Fit World"),
                      QKeySequence(Qt::CTRL | Qt::Key_0)),
            &QAction::triggered, this, canvasCommand(&WorldCanvas::fitWorld));
    viewMenu->addSeparator();

    struct Toggle {
        const char *name;
        QString text;
        bool RenderOptions::*member;
        QKeySequence shortcut;
    };
    const Toggle toggles[] = {
        {"viewGrid", tr("Show &Grid"), &RenderOptions::showGrid,
         QKeySequence(Qt::CTRL | Qt::Key_G)},
        {"viewLabels", tr("Show Value &Labels"), &RenderOptions::showLabels, {}},
        {"viewForeground", tr("Show F&oreground Layer"), &RenderOptions::showForeground, {}},
        {"viewBigDecor", tr("Show &bigDecor Layer"), &RenderOptions::showBigDecor, {}},
        {"viewObjects", tr("Show O&bjects"), &RenderOptions::showObjects, {}},
        {"viewStarts", tr("Show Player &Starts"), &RenderOptions::showStarts, {}},
        {"viewPaths", tr("Show Motion &Paths"), &RenderOptions::showPaths, {}},
    };
    for (const auto &toggle : toggles) {
        auto *item = addAction(viewMenu, QString::fromLatin1(toggle.name), toggle.text,
                               toggle.shortcut);
        item->setCheckable(true);
        item->setChecked(RenderOptions{}.*toggle.member);
        const auto member = toggle.member;
        connect(item, &QAction::toggled, this, [this, member](bool checked) {
            if (updating_) {
                return;
            }
            if (auto *tab = currentTab()) {
                auto options = tab->canvas()->renderOptions();
                options.*member = checked;
                tab->canvas()->setRenderOptions(options);
            }
        });
    }

    viewMenu->addSeparator();
    createDocks(viewMenu);

    // Tool bar: tool, active layer, raw tile ID and the game profile.
    auto *toolBar = addToolBar(tr("Tools"));
    toolBar->setObjectName(QStringLiteral("toolsToolBar"));
    auto *toolGroup = new QActionGroup(this);
    const auto addTool = [&](const QString &name, const QString &text, Tool tool,
                             const QKeySequence &shortcut) {
        auto *item = new QAction(text, this);
        item->setObjectName(name);
        item->setCheckable(true);
        item->setShortcut(shortcut);
        toolGroup->addAction(item);
        toolBar->addAction(item);
        connect(item, &QAction::triggered, this, [this, tool] {
            if (auto *tab = currentTab()) {
                tab->setTool(tool);
            }
        });
        return item;
    };
    addTool(QStringLiteral("toolBrowse"), tr("Browse"), Tool::browse, QKeySequence(Qt::Key_H))
        ->setToolTip(tr("Browse: drag to pan, nothing is edited (H)"));
    addTool(QStringLiteral("toolPencil"), tr("Pencil"), Tool::pencil, QKeySequence(Qt::Key_B))
        ->setToolTip(tr("Raw pencil: writes exactly the tile ID into one cell of the active "
                        "layer, with no neighbour or object effects (B)"));
    toolBar->addSeparator();

    layerCombo_ = new QComboBox(this);
    layerCombo_->setObjectName(QStringLiteral("layerCombo"));
    layerCombo_->setToolTip(tr("Layer edited by the tools"));
    toolBar->addWidget(layerCombo_);
    connect(layerCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (updating_ || index < 0) {
            return;
        }
        if (auto *tab = currentTab()) {
            tab->setActiveLayer(static_cast<SectionKind>(layerCombo_->itemData(index).toInt()));
        }
    });

    tileSpin_ = new QSpinBox(this);
    tileSpin_->setObjectName(QStringLiteral("tileSpin"));
    tileSpin_->setRange(-32768, 32767);
    tileSpin_->setPrefix(tr("Tile ID "));
    tileSpin_->setToolTip(tr("Raw stored tile ID written by the pencil (-1 is an empty cell)"));
    toolBar->addWidget(tileSpin_);
    connect(tileSpin_, &QSpinBox::valueChanged, this, [this](int value) {
        if (updating_) {
            return;
        }
        if (auto *tab = currentTab()) {
            tab->setTileId(static_cast<std::int16_t>(value));
        }
    });
    toolBar->addSeparator();

    profileCombo_ = new QComboBox(this);
    profileCombo_->setObjectName(QStringLiteral("profileCombo"));
    profileCombo_->setToolTip(
        tr("Game profile for catalogs and previews. The world file does not say which game it "
           "belongs to, and choosing a profile never changes its bytes."));
    for (const auto profile :
         {GameProfile::unknown, GameProfile::speedyBlupi1, GameProfile::speedyBlupi2}) {
        profileCombo_->addItem(profileDisplayName(profile), static_cast<int>(profile));
    }
    toolBar->addWidget(profileCombo_);
    connect(profileCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (updating_ || index < 0) {
            return;
        }
        if (auto *tab = currentTab()) {
            tab->setProfile(static_cast<GameProfile>(profileCombo_->itemData(index).toInt()));
        }
    });
}

void MainWindow::createDocks(QMenu *viewMenu) {
    minimapDock_ = new QDockWidget(tr("Minimap"), this);
    minimapDock_->setObjectName(QStringLiteral("minimapDock"));
    auto *minimapHost = new QWidget(minimapDock_);
    auto *minimapLayout = new QVBoxLayout(minimapHost);
    minimapLayout->setContentsMargins(4, 4, 4, 4);
    auto *modeCombo = new QComboBox(minimapHost);
    modeCombo->setObjectName(QStringLiteral("minimapModeCombo"));
    modeCombo->addItem(tr("Composite"), static_cast<int>(MinimapWidget::Mode::composite));
    modeCombo->addItem(tr("Foreground only"), static_cast<int>(MinimapWidget::Mode::foreground));
    modeCombo->addItem(tr("bigDecor only"), static_cast<int>(MinimapWidget::Mode::bigDecor));
    minimap_ = new MinimapWidget(minimapHost);
    minimapLayout->addWidget(modeCombo);
    minimapLayout->addWidget(minimap_, 1);
    connect(modeCombo, &QComboBox::currentIndexChanged, this, [this, modeCombo](int index) {
        minimap_->setMode(static_cast<MinimapWidget::Mode>(modeCombo->itemData(index).toInt()));
    });
    minimapDock_->setWidget(minimapHost);
    addDockWidget(Qt::RightDockWidgetArea, minimapDock_);

    statisticsDock_ = new QDockWidget(tr("World Statistics"), this);
    statisticsDock_->setObjectName(QStringLiteral("statisticsDock"));
    statistics_ = new StatisticsPanel(statisticsDock_);
    statisticsDock_->setWidget(statistics_);
    addDockWidget(Qt::RightDockWidgetArea, statisticsDock_);
    connect(statistics_, &StatisticsPanel::tileValueActivated, this,
            [this](SectionKind layer, int value) {
                locateTileValue(layer, static_cast<std::int16_t>(value));
            });
    connect(statistics_, &StatisticsPanel::objectKeyActivated, this,
            [this](ObjectKey key, int value) {
                locateObjects(key, static_cast<std::int16_t>(value));
            });
    connect(statistics_, &StatisticsPanel::startActivated, this,
            [this](int index) { locateStart(static_cast<std::size_t>(index)); });
    connect(statistics_, &StatisticsPanel::byteRangeActivated, this,
            [this](qulonglong offset, qulonglong size) {
                locateOffsetRange(static_cast<std::size_t>(offset),
                                  static_cast<std::size_t>(size));
            });

    auto *minimapToggle = minimapDock_->toggleViewAction();
    minimapToggle->setObjectName(QStringLiteral("viewMinimap"));
    minimapToggle->setText(tr("&Minimap"));
    viewMenu->addAction(minimapToggle);
    auto *statisticsToggle = statisticsDock_->toggleViewAction();
    statisticsToggle->setObjectName(QStringLiteral("viewStatistics"));
    statisticsToggle->setText(tr("World S&tatistics"));
    viewMenu->addAction(statisticsToggle);
    connect(addAction(viewMenu, QStringLiteral("viewResetLayout"), tr("&Reset Layout")),
            &QAction::triggered, this, [this] { resetLayout(); });
}

void MainWindow::resetLayout() {
    restoreState(defaultState_);
    minimapDock_->show();
    statisticsDock_->show();
}

std::size_t MainWindow::locateTileValue(SectionKind layer, std::int16_t value) {
    auto *tab = currentTab();
    if (tab == nullptr) {
        return 0;
    }
    auto cells = findCells(tab->document().view(), layer, value);
    const auto count = cells.size();
    if (tab->setActiveLayer(layer)) {
        tab->setTileId(value);
    }
    if (count != 0) {
        tab->canvas()->setCursorCell(cells.front());
        tab->canvas()->centerOnCell(cells.front());
    }
    tab->canvas()->setHighlights(std::move(cells), {});
    updateUi();
    statusBar()->showMessage(
        tr("%n cell(s) store value %1 in the %2 layer", nullptr, static_cast<int>(count))
            .arg(value)
            .arg(layer == SectionKind::foreground ? tr("foreground") : tr("bigDecor")),
        8000);
    return count;
}

std::size_t MainWindow::locateObjects(ObjectKey key, std::int16_t value) {
    auto *tab = currentTab();
    if (tab == nullptr) {
        return 0;
    }
    const auto view = tab->document().view();
    auto matches = findActiveSlots(view, key, value);
    const auto count = matches.size();
    if (count != 0) {
        const auto record = *view.objects()->record(matches.front());
        tab->canvas()->centerOnWorld(record.posCurrent.x, record.posCurrent.y);
    }
    tab->canvas()->setHighlights({}, std::move(matches));
    statusBar()->showMessage(tr("%n active object record(s) match", nullptr,
                                static_cast<int>(count)),
                             8000);
    return count;
}

bool MainWindow::locateStart(std::size_t index) {
    auto *tab = currentTab();
    if (tab == nullptr) {
        return false;
    }
    const auto header = tab->document().view().header();
    if (!header || index >= 4) {
        return false;
    }
    const auto position = *header->blupiPos(index);
    tab->canvas()->centerOnWorld(position.x + 30, position.y + 30);
    return true;
}

bool MainWindow::locateOffsetRange(std::size_t offset, std::size_t size) {
    auto *tab = currentTab();
    if (tab == nullptr) {
        return false;
    }
    const auto view = tab->document().view();
    const auto location = locateOffset(view.probe(), offset);
    QString where;
    switch (location.area) {
    case OffsetArea::foreground:
    case OffsetArea::bigDecor:
        tab->canvas()->setCursorCell(location.cell);
        tab->canvas()->centerOnCell(location.cell);
        tab->canvas()->setHighlights({location.cell}, {});
        where = tr("%1 cell (%2, %3)")
                    .arg(location.area == OffsetArea::foreground ? tr("foreground")
                                                                 : tr("bigDecor"))
                    .arg(location.cell.x)
                    .arg(location.cell.y);
        break;
    case OffsetArea::objects:
        if (const auto record = view.objects() ? view.objects()->record(location.slot)
                                               : std::nullopt) {
            tab->canvas()->centerOnWorld(record->posCurrent.x, record->posCurrent.y);
        }
        tab->canvas()->setHighlights({}, {location.slot});
        where = tr("object slot %1").arg(location.slot);
        break;
    case OffsetArea::header:
        where = location.headerField
                    ? tr("header field %1")
                          .arg(QString::fromUtf8(headerField(*location.headerField).name.data(),
                                                 static_cast<qsizetype>(
                                                     headerField(*location.headerField).name.size())))
                    : tr("header");
        break;
    case OffsetArea::suffix:
        where = tr("opaque suffix");
        break;
    case OffsetArea::unmapped:
        return false;
    }
    statusBar()->showMessage(tr("Offset %1, %2 byte(s): %3").arg(offset).arg(size).arg(where),
                             8000);
    return true;
}

void MainWindow::connectTab(DocumentTab *tab) {
    connect(tab, &DocumentTab::documentChanged, tab,
            [tab] { tab->canvas()->clearHighlights(); });
    connect(tab, &DocumentTab::documentChanged, this, [this] { updateUi(); });
    connect(tab, &DocumentTab::toolStateChanged, this, [this] { updateUi(); });
    connect(tab, &DocumentTab::profileChanged, this, [this] { updateUi(); });
    connect(tab, &DocumentTab::statusMessage, this,
            [this](const QString &message) { statusBar()->showMessage(message, 6000); });
    connect(tab->canvas(), &WorldCanvas::hoveredCellChanged, this, [this] { updateStatus(); });
    connect(tab->canvas(), &WorldCanvas::cursorCellChanged, this, [this] { updateStatus(); });
    connect(tab->canvas(), &WorldCanvas::viewChanged, this, [this] { updateStatus(); });
}

DocumentTab *MainWindow::openPath(const QString &path) {
    const auto key = samePathKey(path);
    for (int index = 0; index < tabCount(); ++index) {
        if (samePathKey(tabAt(index)->file().path()) == key) {
            tabs_->setCurrentIndex(index);
            return tabAt(index);
        }
    }
    auto loaded = DocumentFile::load(path);
    if (!loaded) {
        prompts.error(tr("Open World"),
                      tr("%1 could not be opened.\n%2").arg(path, loaded.error().detail));
        return nullptr;
    }
    auto *tab = new DocumentTab(std::move(loaded.value()), tabs_);
    connectTab(tab);
    const auto index = tabs_->addTab(tab, tab->title());
    tabs_->setTabToolTip(index, tab->file().path());
    tabs_->setCurrentIndex(index);
    addRecentFile(tab->file().path());
    tab->canvas()->setFocus();
    updateUi();
    return tab;
}

bool MainWindow::saveTab(DocumentTab *tab) {
    if (tab == nullptr) {
        return false;
    }
    const auto result = tab->file().save();
    if (result.ok()) {
        tab->notifyDocumentChanged();
        statusBar()->showMessage(tr("Saved %1").arg(tab->file().path()), 4000);
        return true;
    }
    if (result.error == SaveError::conflict) {
        switch (prompts.saveConflict(tab, result)) {
        case ConflictChoice::saveAs:
            return saveTabAs(tab);
        case ConflictChoice::reload: {
            auto loaded = DocumentFile::load(tab->file().path());
            if (!loaded) {
                prompts.error(tr("Reload"), loaded.error().detail);
                return false;
            }
            tab->replaceFile(std::move(loaded.value()));
            return false; // the user's changes were not saved
        }
        case ConflictChoice::cancel:
            return false;
        }
    }
    prompts.error(tr("Save"), tr("%1 was not saved. The file on disk is unchanged.\n%2")
                                  .arg(tab->file().path(), result.detail));
    return false;
}

bool MainWindow::saveTabAs(DocumentTab *tab) {
    if (tab == nullptr) {
        return false;
    }
    const auto path = prompts.saveAsPath(tab->file().path());
    if (path.isEmpty()) {
        return false;
    }
    // The path prompt is responsible for overwrite consent.
    const auto result = tab->file().saveAs(path, true);
    if (!result.ok()) {
        prompts.error(tr("Save As"), tr("%1 was not written.\n%2").arg(path, result.detail));
        return false;
    }
    addRecentFile(tab->file().path());
    tabs_->setTabToolTip(tabs_->indexOf(tab), tab->file().path());
    tab->notifyDocumentChanged();
    statusBar()->showMessage(tr("Saved %1").arg(tab->file().path()), 4000);
    return true;
}

bool MainWindow::saveAll() {
    bool allSaved = true;
    for (int index = 0; index < tabCount(); ++index) {
        auto *tab = tabAt(index);
        if (tab->document().isModified()) {
            allSaved = saveTab(tab) && allSaved;
        }
    }
    return allSaved;
}

bool MainWindow::revertTab(DocumentTab *tab) {
    if (tab == nullptr) {
        return false;
    }
    if (tab->document().isModified() && !prompts.confirmRevert(tab)) {
        return false;
    }
    auto loaded = DocumentFile::load(tab->file().path());
    if (!loaded) {
        prompts.error(tr("Revert"), tr("%1 could not be reloaded.\n%2")
                                        .arg(tab->file().path(), loaded.error().detail));
        return false;
    }
    tab->replaceFile(std::move(loaded.value()));
    return true;
}

bool MainWindow::closeTab(DocumentTab *tab) {
    if (tab == nullptr) {
        return false;
    }
    if (tab->document().isModified()) {
        tabs_->setCurrentWidget(tab);
        switch (prompts.unsavedChanges(tab)) {
        case UnsavedChoice::save:
            if (!saveTab(tab)) {
                return false;
            }
            break;
        case UnsavedChoice::discard:
            break;
        case UnsavedChoice::cancel:
            return false;
        }
    }
    tabs_->removeTab(tabs_->indexOf(tab));
    tab->deleteLater();
    updateUi();
    return true;
}

void MainWindow::closeEvent(QCloseEvent *event) {
    while (tabCount() > 0) {
        if (!closeTab(tabAt(tabCount() - 1))) {
            event->ignore();
            return;
        }
    }
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("window/state"), saveState());
    event->accept();
}

QStringList MainWindow::recentFiles() const {
    return QSettings().value(QStringLiteral("recentFiles")).toStringList();
}

void MainWindow::addRecentFile(const QString &path) {
    auto files = recentFiles();
    files.removeAll(path);
    files.prepend(path);
    while (files.size() > maximumRecentFiles) {
        files.removeLast();
    }
    QSettings().setValue(QStringLiteral("recentFiles"), files);
    rebuildRecentMenu();
}

void MainWindow::rebuildRecentMenu() {
    recentMenu_->clear();
    const auto files = recentFiles();
    for (const auto &path : files) {
        connect(recentMenu_->addAction(path), &QAction::triggered, this,
                [this, path] { openPath(path); });
    }
    recentMenu_->setEnabled(!files.isEmpty());
}

QString MainWindow::propertiesText(const DocumentTab &tab) {
    const auto view = tab.document().view();
    auto text = formatProbeReport(tab.file().path(), view.probe());
    if (const auto summary = formatWorldSummary(view); !summary.isEmpty()) {
        text += QStringLiteral("\n\n") + summary;
    }
    const auto access = writeAccessReason(tab.document().writeAccess());
    text += QStringLiteral("\n\n") +
            QString::fromUtf8(access.data(), static_cast<qsizetype>(access.size()));
    text += QStringLiteral("\n") + tr("Profile: %1").arg(profileDisplayName(tab.profile()));
    return text;
}

void MainWindow::showProperties() {
    auto *tab = currentTab();
    if (tab == nullptr) {
        return;
    }
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Document Properties"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *text = new QPlainTextEdit(propertiesText(*tab), &dialog);
    text->setReadOnly(true);
    text->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    layout->addWidget(text);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.resize(720, 560);
    dialog.exec();
}

void MainWindow::updateUi() {
    updating_ = true;
    auto *tab = currentTab();
    for (int index = 0; index < tabCount(); ++index) {
        tabs_->setTabText(index, tabAt(index)->title());
    }
    setWindowTitle(tab != nullptr ? tr("%1 - Eggbert Editor").arg(tab->title())
                                  : tr("Eggbert Editor"));

    const bool hasTab = tab != nullptr;
    const bool writable = hasTab && tab->document().isWritable();
    bool anyModified = false;
    for (int index = 0; index < tabCount(); ++index) {
        anyModified = anyModified || tabAt(index)->document().isModified();
    }
    const auto enable = [this](const char *name, bool enabled) {
        if (auto *item = action(QString::fromLatin1(name))) {
            item->setEnabled(enabled);
        }
    };
    // Save writes the current bytes back; it is offered for read-only
    // documents only through Save As, as an identical copy.
    enable("fileSave", writable);
    enable("fileSaveAs", hasTab);
    enable("fileSaveAll", anyModified);
    enable("fileRevert", hasTab);
    enable("fileProperties", hasTab);
    enable("fileClose", hasTab);
    enable("editUndo", hasTab && tab->document().canUndo());
    enable("editRedo", hasTab && tab->document().canRedo());
    if (auto *undo = action(QStringLiteral("editUndo"))) {
        const auto label = hasTab ? tab->document().undoLabel() : std::string_view{};
        undo->setText(label.empty() ? tr("&Undo")
                                    : tr("&Undo %1").arg(QString::fromUtf8(
                                          label.data(), static_cast<qsizetype>(label.size()))));
    }
    if (auto *redo = action(QStringLiteral("editRedo"))) {
        const auto label = hasTab ? tab->document().redoLabel() : std::string_view{};
        redo->setText(label.empty() ? tr("&Redo")
                                    : tr("&Redo %1").arg(QString::fromUtf8(
                                          label.data(), static_cast<qsizetype>(label.size()))));
    }
    for (const char *name : {"viewZoomIn", "viewZoomOut", "viewActualPixels", "viewFitWorld",
                             "viewGrid", "viewLabels", "viewForeground", "viewBigDecor",
                             "viewObjects", "viewStarts", "viewPaths", "toolBrowse"}) {
        enable(name, hasTab);
    }
    enable("toolPencil", writable);

    if (hasTab) {
        const auto &options = tab->canvas()->renderOptions();
        const std::pair<const char *, bool> checks[] = {
            {"viewGrid", options.showGrid},
            {"viewLabels", options.showLabels},
            {"viewForeground", options.showForeground},
            {"viewBigDecor", options.showBigDecor},
            {"viewObjects", options.showObjects},
            {"viewStarts", options.showStarts},
            {"viewPaths", options.showPaths},
        };
        for (const auto &[name, checked] : checks) {
            action(QString::fromLatin1(name))->setChecked(checked);
        }
        action(tab->tool() == Tool::pencil ? QStringLiteral("toolPencil")
                                           : QStringLiteral("toolBrowse"))
            ->setChecked(true);
    }

    layerCombo_->clear();
    if (hasTab) {
        const auto view = tab->document().view();
        if (view.foreground()) {
            layerCombo_->addItem(tr("Foreground"), static_cast<int>(SectionKind::foreground));
        }
        if (view.bigDecor()) {
            layerCombo_->addItem(tr("bigDecor"), static_cast<int>(SectionKind::bigDecor));
        }
        layerCombo_->setCurrentIndex(
            layerCombo_->findData(static_cast<int>(tab->activeLayer())));
        tileSpin_->setValue(tab->tileId());
        profileCombo_->setCurrentIndex(profileCombo_->findData(static_cast<int>(tab->profile())));
    }
    layerCombo_->setEnabled(writable && layerCombo_->count() > 0);
    tileSpin_->setEnabled(writable);
    profileCombo_->setEnabled(hasTab);
    updating_ = false;
    minimap_->setTab(tab);
    statistics_->setTab(tab);
    updateStatus();
}

void MainWindow::updateStatus() {
    auto *tab = currentTab();
    if (tab == nullptr) {
        cellLabel_->setText(tr("Open a world file to begin."));
        zoomLabel_->clear();
        accessLabel_->clear();
        return;
    }
    const auto cell = tab->canvas()->hoveredCell().value_or(tab->canvas()->cursorCell());
    cellLabel_->setText(tab->describeCell(cell));
    zoomLabel_->setText(tr("%1%").arg(qRound(tab->canvas()->zoom() * 100)));
    const auto &probe = tab->document().probe();
    const auto revision = probe.descriptor != nullptr
                              ? tr("Revision %1.%2")
                                    .arg(probe.descriptor->major)
                                    .arg(probe.descriptor->minor)
                              : tr("Unknown revision");
    accessLabel_->setText(tab->document().isWritable() ? revision
                                                       : tr("%1, read-only").arg(revision));
}

} // namespace eggbert
