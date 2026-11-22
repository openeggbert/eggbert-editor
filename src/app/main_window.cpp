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
#include <QFormLayout>
#include <QInputDialog>
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

#include "app/metadata_dialog.hpp"
#include "app/minimap_widget.hpp"
#include "app/objects_panel.hpp"
#include "app/probe_report.hpp"
#include "app/statistics_panel.hpp"
#include "world/object_catalog.hpp"
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
    poolLabel_ = new QLabel(this);
    poolLabel_->setObjectName(QStringLiteral("poolLabel"));
    statusBar()->addWidget(cellLabel_, 1);
    statusBar()->addPermanentWidget(poolLabel_);
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
    prompts.confirmBroadEdit = [this](DocumentTab *, const BroadEditPreview &preview) {
        QMessageBox box(QMessageBox::Question, preview.label,
                        tr("%1 will change %n cell(s).", nullptr,
                           static_cast<int>(preview.cellsChanged))
                            .arg(preview.label),
                        QMessageBox::Apply | QMessageBox::Cancel, this);
        box.setInformativeText(
            tr("%1 byte(s) in %2 range(s) between offsets %3 and %4 will be rewritten. "
               "Nothing else in the file changes, and the edit is one undo step.")
                .arg(preview.bytesChanged)
                .arg(preview.byteRanges)
                .arg(preview.firstOffset)
                .arg(preview.lastOffset));
        box.setDefaultButton(QMessageBox::Apply);
        return box.exec() == QMessageBox::Apply;
    };
    prompts.replaceValues = [this](DocumentTab *tab) -> std::optional<std::pair<int, int>> {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Replace Value"));
        auto *form = new QFormLayout(&dialog);
        auto *from = new QSpinBox(&dialog);
        auto *to = new QSpinBox(&dialog);
        from->setRange(-32768, 32767);
        to->setRange(-32768, 32767);
        const auto cursor = tab->canvas()->cursorCell();
        if (const auto layer = tab->document().view().layer(tab->activeLayer())) {
            from->setValue(*layer->cell(cursor.x, cursor.y));
        }
        to->setValue(tab->tileId());
        form->addRow(tr("Replace stored value"), from);
        form->addRow(tr("With"), to);
        form->addRow(new QLabel(tab->selection().empty()
                                    ? tr("In the whole %1 layer").arg(layerDisplayName(tab->activeLayer()))
                                    : tr("Inside the selection, %1 layer").arg(layerDisplayName(tab->activeLayer())),
                                &dialog));
        auto *buttons =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        form->addRow(buttons);
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }
        return std::pair{from->value(), to->value()};
    };
    prompts.editMetadata = [this](DocumentTab *tab, int page) -> std::optional<Metadata> {
        MetadataDialog dialog(tab->document().view(), tab->profile(), this);
        dialog.setPage(static_cast<MetadataDialog::Page>(page));
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }
        return dialog.metadata();
    };
    prompts.findQuery = [this](DocumentTab *tab) -> std::optional<FindQuery> {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Find"));
        auto *form = new QFormLayout(&dialog);
        auto *kind = new QComboBox(&dialog);
        kind->addItem(tr("Cell coordinate (x, y)"), static_cast<int>(FindQuery::Kind::coordinate));
        kind->addItem(tr("Stored tile value in the active layer"),
                      static_cast<int>(FindQuery::Kind::tileValue));
        kind->addItem(tr("Stored object type"), static_cast<int>(FindQuery::Kind::objectType));
        kind->addItem(tr("Object slot"), static_cast<int>(FindQuery::Kind::objectSlot));
        auto *first = new QSpinBox(&dialog);
        auto *second = new QSpinBox(&dialog);
        first->setRange(-32768, 32767);
        second->setRange(0, 99);
        first->setValue(static_cast<int>(tab->canvas()->cursorCell().x));
        second->setValue(static_cast<int>(tab->canvas()->cursorCell().y));
        form->addRow(tr("Find"), kind);
        form->addRow(tr("Value / x"), first);
        form->addRow(tr("y (coordinates only)"), second);
        connect(kind, &QComboBox::currentIndexChanged, &dialog,
                [second](int index) { second->setEnabled(index == 0); });
        auto *buttons =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        form->addRow(buttons);
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }
        return FindQuery{static_cast<FindQuery::Kind>(kind->currentData().toInt()),
                         first->value(), second->value()};
    };
}

void MainWindow::createActions() {
    // Top-level menus in their fixed order.
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    auto *editMenu = menuBar()->addMenu(tr("&Edit"));
    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    auto *worldMenu = menuBar()->addMenu(tr("&World"));
    auto *layerMenu = menuBar()->addMenu(tr("&Layer"));
    auto *objectMenu = menuBar()->addMenu(tr("&Object"));
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

    editMenu->addSeparator();
    const auto tabCommand = [this](auto command) {
        return [this, command] {
            if (auto *tab = currentTab()) {
                (tab->*command)();
                updateUi();
            }
        };
    };
    connect(addAction(editMenu, QStringLiteral("editCut"), tr("Cu&t"), QKeySequence::Cut),
            &QAction::triggered, this, tabCommand(&DocumentTab::cutSelection));
    connect(addAction(editMenu, QStringLiteral("editCopy"), tr("&Copy"), QKeySequence::Copy),
            &QAction::triggered, this, tabCommand(&DocumentTab::copySelection));
    connect(addAction(editMenu, QStringLiteral("editPaste"), tr("&Paste"), QKeySequence::Paste),
            &QAction::triggered, this, tabCommand(&DocumentTab::paste));
    connect(addAction(editMenu, QStringLiteral("editDelete"), tr("&Delete"),
                      QKeySequence::Delete),
            &QAction::triggered, this, tabCommand(&DocumentTab::deleteSelection));
    editMenu->addSeparator();
    connect(addAction(editMenu, QStringLiteral("editSelectAll"), tr("Select &All"),
                      QKeySequence::SelectAll),
            &QAction::triggered, this, tabCommand(&DocumentTab::selectAll));
    connect(addAction(editMenu, QStringLiteral("editSelectNone"), tr("Select &None"),
                      QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A)),
            &QAction::triggered, this, tabCommand(&DocumentTab::selectNone));
    connect(addAction(editMenu, QStringLiteral("editSelectInverse"), tr("&Invert Selection"),
                      QKeySequence(Qt::CTRL | Qt::Key_I)),
            &QAction::triggered, this, tabCommand(&DocumentTab::invertSelection));
    editMenu->addSeparator();
    connect(addAction(editMenu, QStringLiteral("editFind"), tr("&Find..."), QKeySequence::Find),
            &QAction::triggered, this, [this] {
                auto *tab = currentTab();
                if (tab == nullptr) {
                    return;
                }
                if (const auto query = prompts.findQuery(tab)) {
                    if (!find(*query)) {
                        statusBar()->showMessage(tr("Nothing found."), 6000);
                    }
                }
            });

    connect(addAction(worldMenu, QStringLiteral("worldMetadata"), tr("&Metadata...")),
            &QAction::triggered, this, [this] { editMetadata(MetadataDialog::worldPage); });
    connect(addAction(worldMenu, QStringLiteral("worldStarts"), tr("Player &Starts...")),
            &QAction::triggered, this, [this] { editMetadata(MetadataDialog::startsPage); });
    connect(addAction(worldMenu, QStringLiteral("worldCopyStart"),
                      tr("&Copy Selected Start to All Four")),
            &QAction::triggered, this, [this] {
                if (auto *tab = currentTab()) {
                    tab->copyActiveStartToAll();
                }
            });

    auto *layerGroup = new QActionGroup(this);
    const auto addLayerChoice = [&](const QString &name, const QString &text, SectionKind layer,
                                    const QKeySequence &shortcut) {
        auto *item = addAction(layerMenu, name, text, shortcut);
        item->setCheckable(true);
        layerGroup->addAction(item);
        connect(item, &QAction::triggered, this, [this, layer] {
            if (auto *tab = currentTab()) {
                tab->setActiveLayer(layer);
            }
        });
    };
    addLayerChoice(QStringLiteral("layerActiveForeground"), tr("Edit &Foreground"),
                   SectionKind::foreground, QKeySequence(Qt::Key_1));
    addLayerChoice(QStringLiteral("layerActiveBigDecor"), tr("Edit &bigDecor"),
                   SectionKind::bigDecor, QKeySequence(Qt::Key_2));
    layerMenu->addSeparator();
    const auto addLock = [&](const QString &name, const QString &text, SectionKind layer) {
        auto *item = addAction(layerMenu, name, text);
        item->setCheckable(true);
        connect(item, &QAction::toggled, this, [this, layer](bool locked) {
            if (updating_) {
                return;
            }
            if (auto *tab = currentTab()) {
                tab->setLayerLocked(layer, locked);
            }
        });
    };
    addLock(QStringLiteral("layerLockForeground"), tr("Lock Foreground"),
            SectionKind::foreground);
    addLock(QStringLiteral("layerLockBigDecor"), tr("Lock bigDecor"), SectionKind::bigDecor);
    auto *isolate = addAction(layerMenu, QStringLiteral("layerIsolate"),
                              tr("&Isolate Active Layer"));
    isolate->setCheckable(true);
    connect(isolate, &QAction::toggled, this, [this](bool isolated) {
        if (updating_) {
            return;
        }
        if (auto *tab = currentTab()) {
            tab->setIsolated(isolated);
            updateUi();
        }
    });
    layerMenu->addSeparator();
    connect(addAction(layerMenu, QStringLiteral("layerFillSelection"),
                      tr("Fill &Selection with Tile ID")),
            &QAction::triggered, this, tabCommand(&DocumentTab::fillSelection));
    connect(addAction(layerMenu, QStringLiteral("layerReplace"), tr("&Replace Value...")),
            &QAction::triggered, this, [this] {
                auto *tab = currentTab();
                if (tab == nullptr) {
                    return;
                }
                if (const auto values = prompts.replaceValues(tab)) {
                    tab->replaceInSelection(static_cast<std::int16_t>(values->first),
                                            static_cast<std::int16_t>(values->second));
                }
            });
    connect(addAction(layerMenu, QStringLiteral("layerClearSelection"),
                      tr("&Clear Selection to Empty (-1)")),
            &QAction::triggered, this, tabCommand(&DocumentTab::deleteSelection));
    connect(addAction(layerMenu, QStringLiteral("layerFlipHorizontal"),
                      tr("Flip Selection &Horizontally")),
            &QAction::triggered, this, [this] {
                if (auto *tab = currentTab()) {
                    tab->flipSelection(true);
                }
            });
    connect(addAction(layerMenu, QStringLiteral("layerFlipVertical"),
                      tr("Flip Selection &Vertically")),
            &QAction::triggered, this, [this] {
                if (auto *tab = currentTab()) {
                    tab->flipSelection(false);
                }
            });

    createMenu_ = objectMenu->addMenu(tr("&Create Verified Type"));
    createMenu_->setObjectName(QStringLiteral("objectCreateMenu"));
    connect(addAction(objectMenu, QStringLiteral("objectDuplicate"), tr("D&uplicate"),
                      QKeySequence(Qt::CTRL | Qt::Key_D)),
            &QAction::triggered, this, tabCommand(&DocumentTab::duplicateSelectedObject));
    connect(addAction(objectMenu, QStringLiteral("objectDelete"), tr("&Delete Object"),
                      QKeySequence(Qt::SHIFT | Qt::Key_Delete)),
            &QAction::triggered, this, tabCommand(&DocumentTab::deleteSelectedObject));
    objectMenu->addSeparator();
    const auto addNudge = [&](const QString &name, const QString &text, Qt::Key key, int dx,
                              int dy) {
        connect(addAction(objectMenu, name, text, QKeySequence(Qt::ALT | key)),
                &QAction::triggered, this, [this, dx, dy] {
                    if (auto *tab = currentTab()) {
                        tab->moveSelectedObject(
                            {dx * layout::cellPixels, dy * layout::cellPixels});
                    }
                });
    };
    addNudge(QStringLiteral("objectNudgeLeft"), tr("Move One Cell &Left"), Qt::Key_Left, -1, 0);
    addNudge(QStringLiteral("objectNudgeRight"), tr("Move One Cell &Right"), Qt::Key_Right, 1, 0);
    addNudge(QStringLiteral("objectNudgeUp"), tr("Move One Cell &Up"), Qt::Key_Up, 0, -1);
    addNudge(QStringLiteral("objectNudgeDown"), tr("Move One Cell Do&wn"), Qt::Key_Down, 0, 1);
    objectMenu->addSeparator();
    connect(addAction(objectMenu, QStringLiteral("objectProperties"),
                      tr("&Properties and Pool Usage")),
            &QAction::triggered, this, [this] {
                objectsDock_->show();
                objectsDock_->raise();
                if (auto *tab = currentTab()) {
                    statusBar()->showMessage(objects_->capacityText().section('\n', 0, 0), 8000);
                    Q_UNUSED(tab);
                }
            });

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
    addTool(QStringLiteral("toolSelect"), tr("Select"), Tool::select, QKeySequence(Qt::Key_S))
        ->setToolTip(tr("Select: drag a rectangle of cells; edits are then limited to it (S)"));
    addTool(QStringLiteral("toolPencil"), tr("Pencil"), Tool::pencil, QKeySequence(Qt::Key_B))
        ->setToolTip(tr("Raw pencil: writes exactly the tile ID into one cell of the active "
                        "layer, with no neighbour or object effects (B)"));
    addTool(QStringLiteral("toolEraser"), tr("Eraser"), Tool::eraser, QKeySequence(Qt::Key_E))
        ->setToolTip(tr("Raw eraser: writes -1 into cells of the active layer only; the other "
                        "layer and all objects are untouched (E)"));
    addTool(QStringLiteral("toolEyedropper"), tr("Eyedropper"), Tool::eyedropper,
            QKeySequence(Qt::Key_I))
        ->setToolTip(tr("Eyedropper: picks the stored value under the pointer as the tile ID (I)"));
    addTool(QStringLiteral("toolLine"), tr("Line"), Tool::line, QKeySequence(Qt::Key_L))
        ->setToolTip(tr("Line: drag to draw a straight line of the tile ID (L)"));
    addTool(QStringLiteral("toolRectangle"), tr("Rectangle"), Tool::rectangle,
            QKeySequence(Qt::Key_R))
        ->setToolTip(tr("Rectangle: drag to fill a rectangle with the tile ID (R)"));
    addTool(QStringLiteral("toolFill"), tr("Fill"), Tool::fill, QKeySequence(Qt::Key_F))
        ->setToolTip(tr("Fill: replaces the connected region of equal values under the pointer, "
                        "bounded by the selection (F)"));
    addTool(QStringLiteral("toolStart"), tr("Start"), Tool::start, QKeySequence(Qt::Key_P))
        ->setToolTip(tr("Player start: click a cell to move the selected start there; click its "
                        "own cell to turn it around (P)"));
    startSpin_ = new QSpinBox(this);
    startSpin_->setObjectName(QStringLiteral("startSpin"));
    startSpin_->setRange(1, 4);
    startSpin_->setPrefix(tr("Start "));
    startSpin_->setToolTip(tr("Player start edited by the Start tool"));
    toolBar->addWidget(startSpin_);
    connect(startSpin_, &QSpinBox::valueChanged, this, [this](int value) {
        if (updating_) {
            return;
        }
        if (auto *tab = currentTab()) {
            tab->setActiveStart(static_cast<std::size_t>(value - 1));
        }
    });
    addTool(QStringLiteral("toolObject"), tr("Object"), Tool::object, QKeySequence(Qt::Key_O))
        ->setToolTip(tr("Object: click to select a record (click again to cycle overlapping "
                        "ones by slot), drag to move it by whole cells, drag a path's end box to "
                        "reshape the path (O)"));
    addTool(QStringLiteral("toolPlaceObject"), tr("Place"), Tool::placeObject,
            QKeySequence(Qt::Key_N))
        ->setToolTip(tr("Place: creates the chosen verified object on a cell; for movers, drag "
                        "to the path's end (N)"));
    presetCombo_ = new QComboBox(this);
    presetCombo_->setObjectName(QStringLiteral("objectPresetCombo"));
    presetCombo_->setToolTip(tr("Verified object presets of the chosen game profile"));
    presetCombo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    toolBar->addWidget(presetCombo_);
    connect(presetCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (updating_ || index < 0) {
            return;
        }
        if (auto *tab = currentTab()) {
            tab->setPlacePreset(presetCombo_->itemData(index).toString());
        }
    });
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

    objectsDock_ = new QDockWidget(tr("Objects"), this);
    objectsDock_->setObjectName(QStringLiteral("objectsDock"));
    objects_ = new ObjectsPanel(objectsDock_);
    objectsDock_->setWidget(objects_);
    addDockWidget(Qt::RightDockWidgetArea, objectsDock_);
    tabifyDockWidget(statisticsDock_, objectsDock_);
    statisticsDock_->raise();
    connect(objects_, &ObjectsPanel::slotActivated, this,
            [this](qulonglong slot) { locateSlot(static_cast<std::size_t>(slot)); });
    auto *objectsToggle = objectsDock_->toggleViewAction();
    objectsToggle->setObjectName(QStringLiteral("viewObjectsDock"));
    objectsToggle->setText(tr("Ob&jects Panel"));

    auto *minimapToggle = minimapDock_->toggleViewAction();
    minimapToggle->setObjectName(QStringLiteral("viewMinimap"));
    minimapToggle->setText(tr("&Minimap"));
    viewMenu->addAction(minimapToggle);
    auto *statisticsToggle = statisticsDock_->toggleViewAction();
    statisticsToggle->setObjectName(QStringLiteral("viewStatistics"));
    statisticsToggle->setText(tr("World S&tatistics"));
    viewMenu->addAction(statisticsToggle);
    viewMenu->addAction(objectsToggle);
    connect(addAction(viewMenu, QStringLiteral("viewResetLayout"), tr("&Reset Layout")),
            &QAction::triggered, this, [this] { resetLayout(); });
}

void MainWindow::resetLayout() {
    restoreState(defaultState_);
    minimapDock_->show();
    statisticsDock_->show();
    objectsDock_->show();
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

bool MainWindow::locateCell(CellPos cell) {
    auto *tab = currentTab();
    if (tab == nullptr || cell.x >= layout::mapWidth || cell.y >= layout::mapHeight) {
        return false;
    }
    tab->canvas()->setCursorCell(cell);
    tab->canvas()->centerOnCell(cell);
    tab->canvas()->setHighlights({cell}, {});
    updateStatus();
    return true;
}

bool MainWindow::locateSlot(std::size_t slot) {
    auto *tab = currentTab();
    if (tab == nullptr) {
        return false;
    }
    const auto objects = tab->document().view().objects();
    const auto record = objects ? objects->record(slot) : std::nullopt;
    if (!record) {
        return false;
    }
    tab->canvas()->centerOnWorld(record->posCurrent.x, record->posCurrent.y);
    tab->canvas()->setHighlights({}, {slot});
    statusBar()->showMessage(record->isFree()
                                 ? tr("Slot %1 is free (type 0)").arg(slot)
                                 : tr("Slot %1: stored type %2").arg(slot).arg(record->type),
                             8000);
    return true;
}

bool MainWindow::editMetadata(int page) {
    auto *tab = currentTab();
    if (tab == nullptr || !tab->document().isWritable()) {
        return false;
    }
    const auto edited = prompts.editMetadata(tab, page);
    return edited && tab->applyMetadataEdit(*edited);
}

bool MainWindow::find(const FindQuery &query) {
    auto *tab = currentTab();
    if (tab == nullptr) {
        return false;
    }
    switch (query.kind) {
    case FindQuery::Kind::coordinate:
        if (query.first < 0 || query.second < 0) {
            return false;
        }
        return locateCell({static_cast<std::size_t>(query.first),
                           static_cast<std::size_t>(query.second)});
    case FindQuery::Kind::tileValue:
        return locateTileValue(tab->activeLayer(), static_cast<std::int16_t>(query.first)) != 0;
    case FindQuery::Kind::objectType:
        return locateObjects(ObjectKey::type, static_cast<std::int16_t>(query.first)) != 0;
    case FindQuery::Kind::objectSlot:
        return query.first >= 0 && locateSlot(static_cast<std::size_t>(query.first));
    }
    return false;
}

void MainWindow::connectTab(DocumentTab *tab) {
    connect(tab, &DocumentTab::documentChanged, tab,
            [tab] { tab->canvas()->clearHighlights(); });
    connect(tab, &DocumentTab::documentChanged, this, [this] { updateUi(); });
    connect(tab, &DocumentTab::toolStateChanged, this, [this] { updateUi(); });
    connect(tab, &DocumentTab::selectionChanged, this, [this] { updateUi(); });
    connect(tab, &DocumentTab::objectSelectionChanged, this, [this] { updateUi(); });
    tab->confirmBroadEdit = [this, tab](const BroadEditPreview &preview) {
        return prompts.confirmBroadEdit(tab, preview);
    };
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

void MainWindow::rebuildPresets(GameProfile profile) {
    presetProfile_ = profile;
    presetCombo_->clear();
    createMenu_->clear();
    for (const auto &preset : objectPresets(profile)) {
        const auto id =
            QString::fromUtf8(preset.id.data(), static_cast<qsizetype>(preset.id.size()));
        const auto name =
            QString::fromUtf8(preset.name.data(), static_cast<qsizetype>(preset.name.size()));
        presetCombo_->addItem(name, id);
        const auto provenance = QString::fromUtf8(
            preset.provenance.data(), static_cast<qsizetype>(preset.provenance.size()));
        presetCombo_->setItemData(presetCombo_->count() - 1,
                                  tr("Stored type %1. Evidence: %2").arg(preset.type).arg(provenance),
                                  Qt::ToolTipRole);
        auto *item = createMenu_->addAction(name);
        item->setObjectName(QStringLiteral("objectCreate_") + id);
        connect(item, &QAction::triggered, this, [this, id] { choosePreset(id); });
    }
}

bool MainWindow::choosePreset(const QString &id) {
    auto *tab = currentTab();
    if (tab == nullptr || findObjectPreset(tab->profile(), id.toStdString()) == nullptr) {
        return false;
    }
    tab->setPlacePreset(id);
    tab->setTool(Tool::placeObject);
    return true;
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
    for (const char *name : {"toolPencil", "toolEraser", "toolLine", "toolRectangle",
                             "toolFill"}) {
        enable(name, writable);
    }
    const auto profile = hasTab ? tab->profile() : GameProfile::unknown;
    if (profile != presetProfile_ || (presetCombo_->count() == 0 && profile != GameProfile::unknown)) {
        rebuildPresets(profile);
    }
    if (hasTab && tab->placePreset() == nullptr && presetCombo_->count() > 0) {
        tab->setPlacePreset(presetCombo_->itemData(0).toString());
    }
    if (hasTab && tab->placePreset() != nullptr) {
        presetCombo_->setCurrentIndex(presetCombo_->findData(
            QString::fromUtf8(tab->placePreset()->id.data(),
                              static_cast<qsizetype>(tab->placePreset()->id.size()))));
    }
    const bool hasPresets = writable && presetCombo_->count() > 0;
    presetCombo_->setEnabled(hasPresets);
    createMenu_->setEnabled(hasPresets);
    enable("toolPlaceObject", hasPresets);
    enable("toolObject", hasTab);
    const bool hasObject = hasTab && tab->selectedSlot().has_value();
    const bool objectEditable = hasObject && tab->canEditObject(*tab->selectedSlot());
    enable("objectDelete", hasObject && tab->canDeleteObject(*tab->selectedSlot()));
    enable("objectDuplicate", objectEditable);
    for (const char *name :
         {"objectNudgeLeft", "objectNudgeRight", "objectNudgeUp", "objectNudgeDown"}) {
        enable(name, objectEditable);
    }
    enable("objectProperties", hasTab);
    enable("toolStart", writable);
    enable("worldMetadata", writable);
    enable("worldStarts", writable);
    enable("worldCopyStart", writable);
    startSpin_->setEnabled(writable);
    if (hasTab) {
        startSpin_->setValue(static_cast<int>(tab->activeStart()) + 1);
    }
    enable("toolSelect", hasTab);
    enable("toolEyedropper", hasTab);
    const bool hasSelection = hasTab && !tab->selection().empty();
    enable("editCut", writable && hasSelection);
    enable("editCopy", hasSelection);
    enable("editPaste", writable && tab->canPaste());
    enable("editDelete", writable && hasSelection);
    enable("editSelectAll", hasTab);
    enable("editSelectNone", hasSelection);
    enable("editSelectInverse", hasTab);
    enable("editFind", hasTab);
    enable("layerFillSelection", writable && hasSelection);
    enable("layerClearSelection", writable && hasSelection);
    enable("layerReplace", writable);
    enable("layerFlipHorizontal", writable && hasSelection);
    enable("layerFlipVertical", writable && hasSelection);
    enable("layerIsolate", hasTab);
    enable("layerLockForeground", hasTab);
    enable("layerLockBigDecor", hasTab && tab->document().view().bigDecor().has_value());
    enable("layerActiveForeground", hasTab);
    enable("layerActiveBigDecor", hasTab && tab->document().view().bigDecor().has_value());

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
        static constexpr std::pair<Tool, const char *> toolActions[] = {
            {Tool::browse, "toolBrowse"},       {Tool::select, "toolSelect"},
            {Tool::pencil, "toolPencil"},       {Tool::eraser, "toolEraser"},
            {Tool::eyedropper, "toolEyedropper"}, {Tool::line, "toolLine"},
            {Tool::rectangle, "toolRectangle"}, {Tool::fill, "toolFill"},
            {Tool::start, "toolStart"},         {Tool::object, "toolObject"},
            {Tool::placeObject, "toolPlaceObject"},
        };
        for (const auto &[tool, name] : toolActions) {
            if (tab->tool() == tool) {
                action(QString::fromLatin1(name))->setChecked(true);
            }
        }
        action(QStringLiteral("layerLockForeground"))
            ->setChecked(tab->isLayerLocked(SectionKind::foreground));
        action(QStringLiteral("layerLockBigDecor"))
            ->setChecked(tab->isLayerLocked(SectionKind::bigDecor));
        action(QStringLiteral("layerIsolate"))->setChecked(tab->isIsolated());
        action(tab->activeLayer() == SectionKind::bigDecor
                   ? QStringLiteral("layerActiveBigDecor")
                   : QStringLiteral("layerActiveForeground"))
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
    objects_->setTab(tab);
    updateStatus();
}

void MainWindow::updateStatus() {
    auto *tab = currentTab();
    if (tab == nullptr) {
        cellLabel_->setText(tr("Open a world file to begin."));
        zoomLabel_->clear();
        accessLabel_->clear();
        poolLabel_->clear();
        return;
    }
    const auto cell = tab->canvas()->hoveredCell().value_or(tab->canvas()->cursorCell());
    cellLabel_->setText(tab->describeCell(cell));
    zoomLabel_->setText(tr("%1%").arg(qRound(tab->canvas()->zoom() * 100)));
    const auto capacity = poolCapacity(tab->document().view());
    poolLabel_->setText(capacity.stored == 0 ? QString()
                                             : tr("Objects %1/%2, %3 free")
                                                   .arg(capacity.active)
                                                   .arg(capacity.stored)
                                                   .arg(capacity.free));
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
