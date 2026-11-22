// SPDX-License-Identifier: GPL-3.0-only
#include "main_window.hpp"

#include <QAction>
#include <QActionGroup>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGuiApplication>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSaveFile>
#include <QSpinBox>
#include <QSysInfo>
#include <QTextBrowser>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>

#include <QDockWidget>

#include "app/appearance.hpp"
#include "app/diagnostics_panel.hpp"
#include "app/inspector_panel.hpp"
#include "app/metadata_dialog.hpp"
#include "app/minimap_widget.hpp"
#include "app/objects_panel.hpp"
#include "app/png_export_dialog.hpp"
#include "app/probe_report.hpp"
#include "app/statistics_panel.hpp"
#include "world/adaptive.hpp"
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
    rebuildPresets(GameProfile::unknown);
    setWindowIcon(applicationIcon());
    applyPreferences();
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
    prompts.exportPng =
        [this](DocumentTab *tab) -> std::optional<std::pair<PngExportOptions, QString>> {
        PngExportDialog dialog(tab->canvas()->renderOptions(), this);
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }
        const QFileInfo source(tab->file().path());
        const auto path = QFileDialog::getSaveFileName(
            this, tr("Export World PNG"),
            source.dir().filePath(source.completeBaseName() + QStringLiteral(".png")),
            tr("PNG images (*.png)"));
        if (path.isEmpty()) {
            return std::nullopt;
        }
        return std::pair{dialog.options(), path};
    };
    prompts.confirmFix = [this](DocumentTab *, const QString &description) {
        QMessageBox box(QMessageBox::Question, tr("Proposed Fix"),
                        tr("Apply this change? It is one undo step and nothing else is touched."),
                        QMessageBox::Apply | QMessageBox::Cancel, this);
        box.setInformativeText(description);
        box.setDefaultButton(QMessageBox::Cancel);
        return box.exec() == QMessageBox::Apply;
    };
    prompts.showDocument = [this](const QString &title, const QString &text, bool markdown) {
        QDialog dialog(this);
        dialog.setWindowTitle(title);
        auto *layout = new QVBoxLayout(&dialog);
        auto *view = new QTextBrowser(&dialog);
        view->setOpenExternalLinks(true);
        if (markdown) {
            view->setMarkdown(text);
        } else {
            view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
            view->setPlainText(text);
        }
        layout->addWidget(view);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);
        dialog.resize(860, 640);
        dialog.exec();
    };
    prompts.saveTextPath = [this](const QString &suggestedName) {
        return QFileDialog::getSaveFileName(this, tr("Save Text File"), suggestedName,
                                            tr("Text files (*.txt);;All files (*)"));
    };
    prompts.editPreferences = [this] {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Preferences"));
        auto *form = new QFormLayout(&dialog);
        QSettings settings;
        auto *theme = new QComboBox(&dialog);
        theme->addItem(tr("Follow the system"), QStringLiteral("system"));
        theme->addItem(tr("Light"), QStringLiteral("light"));
        theme->addItem(tr("Dark"), QStringLiteral("dark"));
        theme->setCurrentIndex(theme->findData(
            settings.value(QStringLiteral("appearance/theme"), QStringLiteral("system"))));
        form->addRow(tr("Theme"), theme);
        auto *grid = new QCheckBox(tr("Show the grid in newly opened worlds"), &dialog);
        grid->setChecked(settings.value(QStringLiteral("view/grid"), false).toBool());
        form->addRow(grid);
        auto *labels = new QCheckBox(tr("Show value labels in newly opened worlds"), &dialog);
        labels->setChecked(settings.value(QStringLiteral("view/labels"), true).toBool());
        form->addRow(labels);
        auto *width = new QComboBox(&dialog);
        for (const int bytes : {8, 16, 24, 32}) {
            width->addItem(tr("%1 bytes per row").arg(bytes), bytes);
        }
        width->setCurrentIndex(
            width->findData(settings.value(QStringLiteral("inspector/bytesPerRow"), 16)));
        form->addRow(tr("Binary Inspector"), width);
        auto *buttons =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        form->addRow(buttons);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        settings.setValue(QStringLiteral("appearance/theme"), theme->currentData());
        settings.setValue(QStringLiteral("view/grid"), grid->isChecked());
        settings.setValue(QStringLiteral("view/labels"), labels->isChecked());
        settings.setValue(QStringLiteral("inspector/bytesPerRow"), width->currentData());
        return true;
    };
    prompts.showReport = [this](const QString &title, const QString &text) {
        QDialog dialog(this);
        dialog.setWindowTitle(title);
        auto *layout = new QVBoxLayout(&dialog);
        auto *view = new QPlainTextEdit(text, &dialog);
        view->setReadOnly(true);
        view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        layout->addWidget(view);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        layout->addWidget(buttons);
        dialog.resize(760, 520);
        dialog.exec();
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
    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
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
    connect(addAction(fileMenu, QStringLiteral("fileExportPng"), tr("&Export World PNG..."),
                      QKeySequence(Qt::CTRL | Qt::Key_E)),
            &QAction::triggered, this, [this] {
                auto *tab = currentTab();
                if (tab == nullptr) {
                    return;
                }
                if (const auto request = prompts.exportPng(tab)) {
                    exportPng(tab, request->first, request->second);
                }
            });
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

    connect(addAction(worldMenu, QStringLiteral("worldValidate"),
                      tr("&Validate (World Doctor)"), QKeySequence(Qt::Key_F8)),
            &QAction::triggered, this, [this] {
                diagnosticsDock_->show();
                diagnosticsDock_->raise();
                diagnostics_->validate();
                statusBar()->showMessage(diagnostics_->summaryText().section(QLatin1Char('.'), 0, 0) +
                                             QLatin1Char('.'),
                                         8000);
            });
    connect(addAction(worldMenu, QStringLiteral("worldCompareOriginal"),
                      tr("Compare with &Original...")),
            &QAction::triggered, this, [this] {
                if (auto *tab = currentTab()) {
                    prompts.showReport(tr("Differences from the File as Opened"),
                                       formatByteDifferences(tab->document(), true));
                }
            });
    connect(addAction(worldMenu, QStringLiteral("worldCompareSaved"),
                      tr("Compare with &Saved...")),
            &QAction::triggered, this, [this] {
                if (auto *tab = currentTab()) {
                    prompts.showReport(tr("Unsaved Differences"),
                                       formatByteDifferences(tab->document(), false));
                }
            });
    worldMenu->addSeparator();
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

    editMenu->addSeparator();
    connect(addAction(editMenu, QStringLiteral("editPreferences"), tr("Pre&ferences..."),
                      QKeySequence::Preferences),
            &QAction::triggered, this, [this] {
                if (prompts.editPreferences()) {
                    applyPreferences();
                }
            });

    connect(addAction(helpMenu, QStringLiteral("helpGuide"), tr("&User Guide"),
                      QKeySequence::HelpContents),
            &QAction::triggered, this, [this] {
                prompts.showDocument(tr("User Guide"),
                                     resourceText(QStringLiteral("user-guide.md")), true);
            });
    connect(addAction(helpMenu, QStringLiteral("helpShortcuts"), tr("&Keyboard Shortcuts")),
            &QAction::triggered, this, [this] {
                prompts.showDocument(tr("Keyboard Shortcuts"), shortcutsText(), false);
            });
    connect(addAction(helpMenu, QStringLiteral("helpFormat"), tr("&Format Notes")),
            &QAction::triggered, this, [this] {
                prompts.showDocument(tr("Format Notes"),
                                     resourceText(QStringLiteral("format-notes.md")), true);
            });
    helpMenu->addSeparator();
    connect(addAction(helpMenu, QStringLiteral("helpDiagnosticsBundle"),
                      tr("&Diagnostics Bundle...")),
            &QAction::triggered, this, [this] {
                const auto path = prompts.saveTextPath(QStringLiteral("eggbert-editor-diagnostics.txt"));
                if (!path.isEmpty()) {
                    saveDiagnosticsBundle(path);
                }
            });
    connect(addAction(helpMenu, QStringLiteral("helpLicense"), tr("&License and Notices")),
            &QAction::triggered, this, [this] {
                prompts.showDocument(tr("License and Notices"),
                                     resourceText(QStringLiteral("NOTICE.md")) +
                                         QStringLiteral("\n\n----\n\n") +
                                         resourceText(QStringLiteral("COPYING")),
                                     false);
            });
    connect(addAction(helpMenu, QStringLiteral("helpAbout"), tr("&About Eggbert Editor")),
            &QAction::triggered, this,
            [this] { prompts.showDocument(tr("About Eggbert Editor"), aboutText(), false); });

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
    connect(addAction(viewMenu, QStringLiteral("viewFitSelection"), tr("Fit &Selection"),
                      QKeySequence(Qt::CTRL | Qt::Key_9)),
            &QAction::triggered, this, [this] { fitSelection(); });
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
    // Two bars so the game profile and the brush settings never fall into
    // the overflow menu: settings on the first row, tools on the second.
    auto *documentBar = addToolBar(tr("Document Settings"));
    documentBar->setObjectName(QStringLiteral("documentToolBar"));
    addToolBarBreak();
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
    addTool(QStringLiteral("toolAdaptive"), tr("Adaptive"), Tool::adaptive,
            QKeySequence(Qt::Key_A))
        ->setToolTip(tr("Adaptive brush: paints the chosen family and recomputes the border "
                        "forms of the cell and its four neighbours as the chosen game's own "
                        "editor does; cells that would change are previewed (A)"));
    adaptiveCombo_ = new QComboBox(this);
    adaptiveCombo_->setObjectName(QStringLiteral("adaptiveCombo"));
    adaptiveCombo_->setToolTip(tr("What the adaptive brush paints"));
    adaptiveCombo_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    
    connect(adaptiveCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (updating_ || index < 0) {
            return;
        }
        if (auto *tab = currentTab()) {
            tab->setAdaptiveBrush(adaptiveCombo_->itemData(index).toString());
        }
    });
    addTool(QStringLiteral("toolStart"), tr("Start"), Tool::start, QKeySequence(Qt::Key_P))
        ->setToolTip(tr("Player start: click a cell to move the selected start there; click its "
                        "own cell to turn it around (P)"));
    startSpin_ = new QSpinBox(this);
    startSpin_->setObjectName(QStringLiteral("startSpin"));
    startSpin_->setRange(1, 4);
    startSpin_->setPrefix(tr("Start "));
    startSpin_->setToolTip(tr("Player start edited by the Start tool"));
    
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
    
    connect(presetCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (updating_ || index < 0) {
            return;
        }
        if (auto *tab = currentTab()) {
            tab->setPlacePreset(presetCombo_->itemData(index).toString());
        }
    });
    layerCombo_ = new QComboBox(this);
    layerCombo_->setObjectName(QStringLiteral("layerCombo"));
    layerCombo_->setToolTip(tr("Layer edited by the tools"));
    
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
    
    connect(tileSpin_, &QSpinBox::valueChanged, this, [this](int value) {
        if (updating_) {
            return;
        }
        if (auto *tab = currentTab()) {
            tab->setTileId(static_cast<std::int16_t>(value));
        }
    });
    profileCombo_ = new QComboBox(this);
    profileCombo_->setObjectName(QStringLiteral("profileCombo"));
    profileCombo_->setToolTip(
        tr("Game profile for catalogs and previews. The world file does not say which game it "
           "belongs to, and choosing a profile never changes its bytes."));
    for (const auto profile :
         {GameProfile::unknown, GameProfile::speedyBlupi1, GameProfile::speedyBlupi2}) {
        profileCombo_->addItem(profileDisplayName(profile), static_cast<int>(profile));
    }
    
    connect(profileCombo_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (updating_ || index < 0) {
            return;
        }
        if (auto *tab = currentTab()) {
            tab->setProfile(static_cast<GameProfile>(profileCombo_->itemData(index).toInt()));
        }
    });

    const auto labelled = [&](const QString &text, QWidget *widget) {
        auto *label = new QLabel(text, this);
        label->setBuddy(widget);
        label->setContentsMargins(6, 0, 2, 0);
        documentBar->addWidget(label);
        documentBar->addWidget(widget);
    };
    labelled(tr("Game"), profileCombo_);
    documentBar->addSeparator();
    labelled(tr("Layer"), layerCombo_);
    documentBar->addWidget(tileSpin_);
    documentBar->addSeparator();
    labelled(tr("Adaptive"), adaptiveCombo_);
    labelled(tr("Object"), presetCombo_);
    documentBar->addWidget(startSpin_);
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

    inspectorDock_ = new QDockWidget(tr("Binary Inspector"), this);
    inspectorDock_->setObjectName(QStringLiteral("inspectorDock"));
    inspector_ = new InspectorPanel(inspectorDock_);
    inspectorDock_->setWidget(inspector_);
    addDockWidget(Qt::BottomDockWidgetArea, inspectorDock_);
    inspectorDock_->hide();
    connect(inspector_, &InspectorPanel::offsetChosenByUser, this, [this](qulonglong offset) {
        if (syncing_) {
            return;
        }
        syncing_ = true;
        locateOffsetRange(static_cast<std::size_t>(offset), 1);
        syncing_ = false;
    });
    auto *inspectorToggle = inspectorDock_->toggleViewAction();
    inspectorToggle->setObjectName(QStringLiteral("viewInspector"));
    inspectorToggle->setText(tr("&Binary Inspector"));
    inspectorToggle->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));

    diagnosticsDock_ = new QDockWidget(tr("Diagnostics"), this);
    diagnosticsDock_->setObjectName(QStringLiteral("diagnosticsDock"));
    diagnostics_ = new DiagnosticsPanel(diagnosticsDock_);
    diagnosticsDock_->setWidget(diagnostics_);
    addDockWidget(Qt::BottomDockWidgetArea, diagnosticsDock_);
    tabifyDockWidget(inspectorDock_, diagnosticsDock_);
    diagnosticsDock_->hide();
    connect(diagnostics_, &DiagnosticsPanel::findingActivated, this, [this](int index) {
        const auto &findings = diagnostics_->report().findings;
        if (index >= 0 && static_cast<std::size_t>(index) < findings.size()) {
            locateFinding(findings[static_cast<std::size_t>(index)]);
        }
    });
    connect(diagnostics_, &DiagnosticsPanel::fixRequested, this, [this](int index) {
        const auto findings = diagnostics_->report().findings; // copy: applying revalidates
        if (index >= 0 && static_cast<std::size_t>(index) < findings.size()) {
            reviewFix(findings[static_cast<std::size_t>(index)]);
        }
    });
    auto *diagnosticsToggle = diagnosticsDock_->toggleViewAction();
    diagnosticsToggle->setObjectName(QStringLiteral("viewDiagnostics"));
    diagnosticsToggle->setText(tr("&Diagnostics"));

    auto *minimapToggle = minimapDock_->toggleViewAction();
    minimapToggle->setObjectName(QStringLiteral("viewMinimap"));
    minimapToggle->setText(tr("&Minimap"));
    viewMenu->addAction(minimapToggle);
    auto *statisticsToggle = statisticsDock_->toggleViewAction();
    statisticsToggle->setObjectName(QStringLiteral("viewStatistics"));
    statisticsToggle->setText(tr("World S&tatistics"));
    viewMenu->addAction(statisticsToggle);
    viewMenu->addAction(objectsToggle);
    viewMenu->addAction(inspectorToggle);
    viewMenu->addAction(diagnosticsToggle);
    auto *layoutMenu = viewMenu->addMenu(tr("La&yout"));
    connect(addAction(layoutMenu, QStringLiteral("viewLayoutMapping"), tr("&Mapping")),
            &QAction::triggered, this, [this] { applyLayout(DockLayout::mapping); });
    connect(addAction(layoutMenu, QStringLiteral("viewLayoutObjects"), tr("&Objects")),
            &QAction::triggered, this, [this] { applyLayout(DockLayout::objects); });
    connect(addAction(layoutMenu, QStringLiteral("viewLayoutValidation"), tr("&Validation")),
            &QAction::triggered, this, [this] { applyLayout(DockLayout::validation); });
    connect(addAction(viewMenu, QStringLiteral("viewResetLayout"), tr("&Reset Layout")),
            &QAction::triggered, this, [this] { resetLayout(); });

    auto *themeMenu = viewMenu->addMenu(tr("&Theme"));
    auto *themeGroup = new QActionGroup(this);
    const auto addTheme = [&](const QString &name, const QString &text, Theme theme) {
        auto *item = addAction(themeMenu, name, text);
        item->setCheckable(true);
        themeGroup->addAction(item);
        connect(item, &QAction::triggered, this, [this, theme] { setTheme(themeKey(theme)); });
    };
    addTheme(QStringLiteral("viewThemeSystem"), tr("&System"), Theme::system);
    addTheme(QStringLiteral("viewThemeLight"), tr("&Light"), Theme::light);
    addTheme(QStringLiteral("viewThemeDark"), tr("&Dark"), Theme::dark);
}

void MainWindow::applyLayout(DockLayout layout) {
    const auto show = [](QDockWidget *dock, bool visible) {
        dock->setVisible(visible);
        if (visible) {
            dock->raise();
        }
    };
    switch (layout) {
    case DockLayout::mapping:
        show(objectsDock_, false);
        show(inspectorDock_, false);
        show(diagnosticsDock_, false);
        show(minimapDock_, true);
        show(statisticsDock_, true);
        break;
    case DockLayout::objects:
        show(statisticsDock_, false);
        show(inspectorDock_, false);
        show(diagnosticsDock_, false);
        show(minimapDock_, true);
        show(objectsDock_, true);
        break;
    case DockLayout::validation:
        show(objectsDock_, false);
        show(minimapDock_, false);
        show(statisticsDock_, true);
        show(inspectorDock_, true);
        show(diagnosticsDock_, true);
        diagnostics_->validate();
        break;
    }
}

void MainWindow::setTheme(const QString &key) {
    const auto theme = themeFromKey(key);
    applyTheme(theme);
    QSettings().setValue(QStringLiteral("appearance/theme"), themeKey(theme));
    const char *name = theme == Theme::light  ? "viewThemeLight"
                       : theme == Theme::dark ? "viewThemeDark"
                                              : "viewThemeSystem";
    if (auto *item = action(QString::fromLatin1(name))) {
        item->setChecked(true);
    }
}

void MainWindow::applyPreferences() {
    const QSettings settings;
    setTheme(settings.value(QStringLiteral("appearance/theme"), QStringLiteral("system")).toString());
    inspector_->setBytesPerRow(settings.value(QStringLiteral("inspector/bytesPerRow"), 16).toInt());
}

bool MainWindow::fitSelection() {
    auto *tab = currentTab();
    if (tab == nullptr) {
        return false;
    }
    if (const auto bounds = tab->selection().bounds()) {
        tab->canvas()->fitCells(bounds->min, bounds->max);
        return true;
    }
    if (const auto slot = tab->selectedSlot()) {
        const auto record = tab->document().view().objects()->record(*slot);
        const auto first = cellOfPosition(record->posStart);
        const auto second = cellOfPosition(record->posEnd);
        if (first && second) {
            tab->canvas()->fitCells(*first, *second);
            return true;
        }
    }
    return false;
}

QString MainWindow::resourceText(const QString &name) {
    QFile file(QStringLiteral(":/docs/") + name);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QString::fromUtf8(file.readAll());
}

QString MainWindow::shortcutsText() const {
    QStringList lines;
    for (const auto *item : findChildren<QAction *>()) {
        if (item->shortcut().isEmpty() || item->objectName().isEmpty()) {
            continue;
        }
        lines << QStringLiteral("%1\t%2")
                     .arg(item->shortcut().toString(QKeySequence::NativeText).leftJustified(18),
                          item->text().remove(QLatin1Char('&')));
    }
    lines.sort(Qt::CaseInsensitive);
    lines << QString()
          << tr("Canvas: arrow keys move the cell cursor (Shift: ten cells, Ctrl: pan); Space or "
                "Enter applies the tool; Ctrl+wheel zooms; middle button or Alt+drag pans.");
    return lines.join(QLatin1Char('\n'));
}

QString MainWindow::aboutText() {
    return tr("Eggbert Editor %1\n\nA lossless desktop editor for individual world files of "
              "Speedy Blupi / Speedy Eggbert and Speedy Blupi II / Speedy Eggbert 2.\n\n"
              "Licensed under the GNU General Public License version 3 only. It comes with no "
              "warranty. It contains no game artwork or game data.\n\nBuilt with Qt %2, "
              "running on Qt %3.")
        .arg(QStringLiteral(EGGBERT_EDITOR_VERSION), QStringLiteral(QT_VERSION_STR),
             QString::fromLatin1(qVersion()));
}

QString MainWindow::diagnosticsBundleText(const DocumentTab *tab) {
    QStringList lines;
    lines << QStringLiteral("Eggbert Editor diagnostics bundle");
    lines << QStringLiteral("Editor version: %1").arg(QStringLiteral(EGGBERT_EDITOR_VERSION));
    lines << QStringLiteral("Qt: built with %1, running %2")
                 .arg(QStringLiteral(QT_VERSION_STR), QString::fromLatin1(qVersion()));
    lines << QStringLiteral("Platform: %1, %2, %3")
                 .arg(QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture(),
                      QGuiApplication::platformName());
    if (tab == nullptr) {
        lines << QString() << QStringLiteral("No document is open.");
        return lines.join(QLatin1Char('\n'));
    }
    lines << QString() << QStringLiteral("== Document ==");
    // The file name only: a full path can reveal personal folder names.
    lines << propertiesText(*tab).replace(tab->file().path(),
                                          QFileInfo(tab->file().path()).fileName());
    lines << QString() << QStringLiteral("== World Doctor ==");
    const auto report = runDoctor(tab->document().view(), tab->profile());
    for (const auto &finding : report.findings) {
        const auto severity = severityName(finding.severity);
        lines << QStringLiteral("[%1] %2 @%3+%4: %5")
                     .arg(QString::fromUtf8(severity.data(), static_cast<qsizetype>(severity.size())),
                          QString::fromUtf8(finding.ruleId.data(),
                                            static_cast<qsizetype>(finding.ruleId.size())))
                     .arg(finding.range.offset)
                     .arg(finding.range.size)
                     .arg(QString::fromStdString(finding.message));
    }
    if (report.findings.empty()) {
        lines << QStringLiteral("No findings.");
    }
    lines << QString() << QStringLiteral("== Changes since opened ==");
    lines << formatByteDifferences(tab->document(), true);
    lines << QString() << QStringLiteral("== Unsaved changes ==");
    lines << formatByteDifferences(tab->document(), false);
    return lines.join(QLatin1Char('\n'));
}

bool MainWindow::saveDiagnosticsBundle(const QString &path) {
    QSaveFile file(path);
    const auto text = diagnosticsBundleText(currentTab()).toUtf8();
    if (!file.open(QIODevice::WriteOnly) || file.write(text) != text.size() || !file.commit()) {
        prompts.error(tr("Diagnostics Bundle"),
                      tr("%1 was not written.\n%2").arg(path, file.errorString()));
        return false;
    }
    statusBar()->showMessage(tr("Diagnostics bundle saved to %1").arg(path), 6000);
    return true;
}

void MainWindow::resetLayout() {
    restoreState(defaultState_);
    minimapDock_->show();
    statisticsDock_->show();
    objectsDock_->show();
    inspectorDock_->hide();
    diagnosticsDock_->hide();
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
        tab->setSelectedSlot(location.slot);
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
    showInInspector(offset, size);
    return true;
}

void MainWindow::showInInspector(std::size_t offset, std::size_t size) {
    if (syncing_) {
        return; // the inspector itself asked for this location
    }
    syncing_ = true;
    inspector_->goToOffset(offset, size);
    syncing_ = false;
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

bool MainWindow::exportPng(DocumentTab *tab, const PngExportOptions &options,
                           const QString &path) {
    if (tab == nullptr) {
        return false;
    }
    const auto result = exportWorldPng(tab->document().view(), tab->profile(), options, path);
    if (!result.ok) {
        prompts.error(tr("Export World PNG"),
                      tr("%1 was not written.\n%2").arg(path, result.error));
        return false;
    }
    statusBar()->showMessage(
        tr("Exported %1 x %2 pixels to %3").arg(result.width).arg(result.height).arg(path), 8000);
    return true;
}

bool MainWindow::locateFinding(const Finding &finding) {
    auto *tab = currentTab();
    if (tab == nullptr) {
        return false;
    }
    if (!finding.cells.empty() && finding.layer) {
        tab->setActiveLayer(*finding.layer);
        tab->canvas()->setCursorCell(finding.cells.front());
        tab->canvas()->centerOnCell(finding.cells.front());
        tab->canvas()->setHighlights(finding.cells, {});
    } else if (finding.slot) {
        locateSlot(*finding.slot);
        tab->setSelectedSlot(*finding.slot);
    } else if (finding.headerField == HeaderField::blupiPos ||
               finding.headerField == HeaderField::blupiDir) {
        locateStart(finding.elementIndex);
    }
    // Missing bytes cannot be shown; everything else is selected in the
    // inspector.
    const auto size = tab->document().currentBytes().size();
    if (finding.range.offset < size) {
        showInInspector(finding.range.offset,
                        std::min(finding.range.size, size - finding.range.offset));
    }
    statusBar()->showMessage(QString::fromStdString(finding.message), 10000);
    return true;
}

bool MainWindow::reviewFix(const Finding &finding) {
    auto *tab = currentTab();
    if (tab == nullptr || !finding.fix || !tab->document().isWritable()) {
        return false;
    }
    if (!prompts.confirmFix(tab, formatFix(tab->document(), *finding.fix))) {
        return false;
    }
    return tab->applyFix(*finding.fix);
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
    connect(tab->canvas(), &WorldCanvas::cursorCellChanged, this, [this, tab] {
        updateStatus();
        if (tab != currentTab() || tab->document().probe().descriptor == nullptr) {
            return;
        }
        if (const auto range = cellByteRange(*tab->document().probe().descriptor,
                                             tab->activeLayer(), tab->canvas()->cursorCell())) {
            showInInspector(range->offset, range->size);
        }
    });
    connect(tab, &DocumentTab::objectSelectionChanged, this, [this, tab] {
        if (tab != currentTab() || !tab->selectedSlot() ||
            tab->document().probe().descriptor == nullptr) {
            return;
        }
        if (const auto range =
                slotByteRange(*tab->document().probe().descriptor, *tab->selectedSlot())) {
            showInInspector(range->offset, range->size);
        }
    });
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
    {
        const QSettings settings;
        auto options = tab->canvas()->renderOptions();
        options.showGrid = settings.value(QStringLiteral("view/grid"), options.showGrid).toBool();
        options.showLabels =
            settings.value(QStringLiteral("view/labels"), options.showLabels).toBool();
        tab->canvas()->setRenderOptions(options);
    }
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
        // A full audit runs with every save; it informs and never blocks.
        const auto report = runDoctor(tab->document().view(), tab->profile());
        statusBar()->showMessage(tr("Saved %1 - World Doctor: %2 error(s), %3 warning(s)")
                                     .arg(tab->file().path())
                                     .arg(report.count(Severity::error))
                                     .arg(report.count(Severity::warning)),
                                 8000);
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
    adaptiveCombo_->clear();
    adaptiveCombo_->addItem(tr("Tile ID, refresh neighbours"),
                            QString::fromLatin1(DocumentTab::adaptiveTileBrush));
    adaptiveCombo_->addItem(tr("Erase, refresh neighbours"),
                            QString::fromLatin1(DocumentTab::adaptiveEraseBrush));
    for (const auto &family : adaptiveFamilies(profile)) {
        adaptiveCombo_->addItem(
            QString::fromUtf8(family.name.data(), static_cast<qsizetype>(family.name.size())),
            QString::fromUtf8(family.id.data(), static_cast<qsizetype>(family.id.size())));
        adaptiveCombo_->setItemData(
            adaptiveCombo_->count() - 1,
            tr("Writes %1, then recomputes border forms. Evidence: %2")
                .arg(family.seed)
                .arg(QString::fromUtf8(family.provenance.data(),
                                       static_cast<qsizetype>(family.provenance.size()))),
            Qt::ToolTipRole);
    }
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
    enable("fileExportPng", hasTab);
    enable("viewFitSelection",
           hasTab && (!tab->selection().empty() || tab->selectedSlot().has_value()));
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
    const bool adaptiveAvailable = writable && profile != GameProfile::unknown;
    enable("toolAdaptive", adaptiveAvailable);
    adaptiveCombo_->setEnabled(adaptiveAvailable);
    if (hasTab) {
        auto index = adaptiveCombo_->findData(tab->adaptiveBrush());
        if (index < 0) {
            // The brush belonged to another game: fall back to the raw tile.
            tab->setAdaptiveBrush(QString::fromLatin1(DocumentTab::adaptiveTileBrush));
            index = adaptiveCombo_->findData(tab->adaptiveBrush());
        }
        adaptiveCombo_->setCurrentIndex(index);
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
    enable("worldValidate", hasTab);
    enable("worldCompareOriginal", hasTab);
    enable("worldCompareSaved", hasTab);
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
            {Tool::placeObject, "toolPlaceObject"}, {Tool::adaptive, "toolAdaptive"},
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
    inspector_->setTab(tab);
    diagnostics_->setTab(tab);
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
