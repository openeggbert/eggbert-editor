// SPDX-License-Identifier: GPL-3.0-only
#include "main_window.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCryptographicHash>
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
#include <QLineEdit>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProcess>
#include <QRadioButton>
#include <QStandardPaths>
#include <QRegularExpression>
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
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMimeData>
#include <QSlider>
#include <QStackedWidget>
#include <QTimer>
#include <QUrl>

#include <cmath>

#include "app/appearance.hpp"
#include "app/diagnostics_panel.hpp"
#include "app/game_assets.hpp"
#include "app/inspector_panel.hpp"
#include "app/metadata_dialog.hpp"
#include "app/minimap_widget.hpp"
#include "app/objects_panel.hpp"
#include "app/music_player.hpp"
#include "app/palette_panel.hpp"
#include "app/sprite_icons.hpp"
#include "app/cli.hpp"
#include "app/encyclopedia_dialog.hpp"
#include "app/installation_panel.hpp"
#include "app/tool_icons.hpp"
#include "world/new_world.hpp"
#include "world/campaign_doors.hpp"
#include "world/hex_model.hpp"
#include "world/revision_convert.hpp"
#include "world/text_world.hpp"
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

int artworkIndex(GameProfile profile) {
    return profile == GameProfile::speedyBlupi1 ? 0 : profile == GameProfile::speedyBlupi2 ? 1 : -1;
}

QString artworkSettingKey(GameProfile profile) {
    return profile == GameProfile::speedyBlupi1 ? QStringLiteral("artwork/directory-game1")
                                                : QStringLiteral("artwork/directory-game2");
}

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
    // The central area: a welcome page while nothing is open, else the tabs.
    central_ = new QStackedWidget(this);
    welcome_ = new QWidget(central_);
    welcome_->setObjectName(QStringLiteral("welcomePage"));
    {
        auto *outer = new QVBoxLayout(welcome_);
        outer->addStretch(1);
        auto *column = new QVBoxLayout;
        column->setSpacing(10);
        auto *title = new QLabel(tr("Eggbert Editor"), welcome_);
        auto titleFont = title->font();
        titleFont.setPointSizeF(titleFont.pointSizeF() * 2.0);
        titleFont.setBold(true);
        title->setFont(titleFont);
        column->addWidget(title);
        auto *subtitle = new QLabel(
            tr("Lossless editor for the worlds of Speedy Blupi / Speedy Eggbert 1 and 2.\n"
               "Open a world file, or drop one onto this window."),
            welcome_);
        subtitle->setWordWrap(true);
        column->addWidget(subtitle);
        auto *buttons = new QHBoxLayout;
        auto *open = new QPushButton(tr("Open World..."), welcome_);
        open->setObjectName(QStringLiteral("welcomeOpen"));
        connect(open, &QPushButton::clicked, this, [this] {
            if (auto *item = action(QStringLiteral("fileOpen"))) {
                item->trigger();
            }
        });
        auto *create = new QPushButton(tr("New World..."), welcome_);
        create->setObjectName(QStringLiteral("welcomeNew"));
        connect(create, &QPushButton::clicked, this, [this] {
            if (auto *item = action(QStringLiteral("fileNew"))) {
                item->trigger();
            }
        });
        auto *game = new QPushButton(tr("Choose Game Directory..."), welcome_);
        game->setObjectName(QStringLiteral("welcomeGame"));
        game->setToolTip(tr("Select the directory of a game you have, so worlds are drawn with "
                            "its graphics. The directory is only read."));
        connect(game, &QPushButton::clicked, this, [this] {
            if (auto *item = action(QStringLiteral("viewArtworkDirectory"))) {
                item->trigger();
            }
        });
        buttons->addWidget(open);
        buttons->addWidget(create);
        buttons->addWidget(game);
        buttons->addStretch(1);
        column->addLayout(buttons);
        auto *recentLabel = new QLabel(tr("Recent worlds"), welcome_);
        column->addWidget(recentLabel);
        welcomeRecent_ = new QListWidget(welcome_);
        welcomeRecent_->setObjectName(QStringLiteral("welcomeRecent"));
        welcomeRecent_->setAccessibleName(tr("Recent worlds"));
        welcomeRecent_->setMaximumHeight(220);
        connect(welcomeRecent_, &QListWidget::itemActivated, this,
                [this](QListWidgetItem *item) { openPath(item->data(Qt::UserRole).toString()); });
        connect(welcomeRecent_, &QListWidget::itemClicked, this,
                [this](QListWidgetItem *item) { openPath(item->data(Qt::UserRole).toString()); });
        column->addWidget(welcomeRecent_);
        auto *row = new QHBoxLayout;
        row->addStretch(1);
        row->addLayout(column, 3);
        row->addStretch(1);
        outer->addLayout(row);
        outer->addStretch(2);
    }
    central_->addWidget(welcome_);
    central_->addWidget(tabs_);
    setCentralWidget(central_);
    setAcceptDrops(true);
    connect(tabs_, &QTabWidget::tabCloseRequested, this,
            [this](int index) { closeTab(tabAt(index)); });
    connect(tabs_, &QTabWidget::currentChanged, this, [this] { updateUi(); });
    // Unsaved changes are copied to the recovery store shortly after an edit.
    autosaveTimer_ = new QTimer(this);
    autosaveTimer_->setSingleShot(true);
    autosaveTimer_->setInterval(3000);
    connect(autosaveTimer_, &QTimer::timeout, this, [this] { flushAutosave(); });

    cellLabel_ = new QLabel(this);
    zoomLabel_ = new QLabel(this);
    accessLabel_ = new QLabel(this);
    poolLabel_ = new QLabel(this);
    poolLabel_->setObjectName(QStringLiteral("poolLabel"));
    statusBar()->addWidget(cellLabel_, 1);
    experimentalLabel_ = new QLabel(tr("Experimental editing"), this);
    experimentalLabel_->setObjectName(QStringLiteral("experimentalLabel"));
    experimentalLabel_->setToolTip(
        tr("Edit > Edit Bytes is available and music and region accept any number. Switch it "
           "off in Edit > Preferences."));
    experimentalLabel_->hide();
    statusBar()->addPermanentWidget(experimentalLabel_);
    statusBar()->addPermanentWidget(poolLabel_);
    statusBar()->addPermanentWidget(accessLabel_);
    // Zoom: the slider covers the whole range evenly in ratio steps.
    zoomSlider_ = new QSlider(Qt::Horizontal, this);
    zoomSlider_->setObjectName(QStringLiteral("zoomSlider"));
    zoomSlider_->setRange(0, 1000);
    zoomSlider_->setFixedWidth(140);
    zoomSlider_->setToolTip(tr("Zoom"));
    connect(zoomSlider_, &QSlider::valueChanged, this, [this](int value) {
        if (updating_) {
            return;
        }
        if (auto *tab = currentTab()) {
            const double ratio = ViewTransform::maximumScale / ViewTransform::minimumScale;
            tab->canvas()->setZoom(ViewTransform::minimumScale * std::pow(ratio, value / 1000.0));
            updateStatus();
        }
    });
    statusBar()->addPermanentWidget(zoomSlider_);
    statusBar()->addPermanentWidget(zoomLabel_);

    installDefaultPrompts();
    createActions();

    defaultState_ = saveState();
    rebuildPresets(GameProfile::unknown);
    {
        // Remembered artwork directories are read again; a directory that
        // disappeared simply leaves the schematic drawing.
        const QSettings stored;
        for (const auto profile : {GameProfile::speedyBlupi1, GameProfile::speedyBlupi2}) {
            const auto directory = stored.value(artworkSettingKey(profile)).toString();
            if (!directory.isEmpty()) {
                artwork_[artworkIndex(profile)] = GameAssets::load(directory, profile);
            }
        }
        artworkEnabled_ = stored.value(QStringLiteral("artwork/enabled"), false).toBool();
        animate_ = stored.value(QStringLiteral("view/animate"), true).toBool();
    }
    setWindowIcon(applicationIcon());
    applyPreferences();
    if (const auto rules =
            QSettings().value(QStringLiteral("experimental/borderRules")).toString();
        experimental_ && !rules.isEmpty()) {
        // Quietly: a rules file that vanished or broke is not worth a dialog
        // at every start; loading it again from the menu says why.
        QFile file(rules);
        if (file.open(QIODevice::ReadOnly) && file.size() <= 1024 * 1024) {
            const auto text = file.readAll();
            if (auto parsed = parseCustomBorderRules(
                    std::string_view(text.constData(), static_cast<std::size_t>(text.size())))) {
                customRules_ = std::make_shared<const std::vector<CustomBorderRule>>(
                    std::move(parsed.value()));
                shareCustomRules();
            }
        }
    }
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
    prompts.recover = [this](const RecoveryEntry &entry, bool canRestore, const QString &detail) {
        QMessageBox box(QMessageBox::Question, tr("Unsaved Changes Found"),
                        tr("Unsaved changes to %1 from %2 were kept when the editor last "
                           "stopped.\n%3")
                            .arg(QFileInfo(entry.originalPath).fileName(),
                                 QLocale().toString(entry.saved, QLocale::ShortFormat), detail),
                        QMessageBox::NoButton, this);
        auto *restore = canRestore ? box.addButton(tr("Restore"), QMessageBox::AcceptRole) : nullptr;
        auto *copy = box.addButton(tr("Save a Copy As..."), QMessageBox::ActionRole);
        auto *discard = box.addButton(tr("Discard"), QMessageBox::DestructiveRole);
        box.addButton(tr("Ask Again Later"), QMessageBox::RejectRole);
        box.exec();
        if (restore != nullptr && box.clickedButton() == restore) {
            return RecoveryChoice::restore;
        }
        if (box.clickedButton() == copy) {
            return RecoveryChoice::saveCopy;
        }
        return box.clickedButton() == discard ? RecoveryChoice::discard : RecoveryChoice::keep;
    };
    prompts.editBytes = [this](DocumentTab *tab, qulonglong offset)
        -> std::optional<std::pair<qulonglong, QByteArray>> {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Edit Bytes"));
        auto *form = new QFormLayout(&dialog);
        auto *offsetEdit = new QLineEdit(QString::number(offset), &dialog);
        offsetEdit->setObjectName(QStringLiteral("editBytesOffset"));
        form->addRow(tr("Offset (decimal or 0x hex)"), offsetEdit);
        auto *bytesEdit = new QLineEdit(&dialog);
        bytesEdit->setObjectName(QStringLiteral("editBytesValue"));
        bytesEdit->setPlaceholderText(tr("for example 2A 00 FF"));
        form->addRow(tr("New bytes (hex)"), bytesEdit);
        auto *summary = new QLabel(&dialog);
        summary->setObjectName(QStringLiteral("editBytesSummary"));
        summary->setWordWrap(true);
        form->addRow(summary);
        auto *buttons =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        const auto parsedOffset = [offsetEdit]() -> std::optional<qulonglong> {
            bool ok = false;
            // Decimal, or hexadecimal with 0x; never octal ("010" is ten).
            const auto text = offsetEdit->text().trimmed();
            const bool hex = text.startsWith(QLatin1String("0x"), Qt::CaseInsensitive);
            static const QRegularExpression digits(QStringLiteral("^[0-9]+$"));
            if (!hex && !digits.match(text).hasMatch()) {
                return std::nullopt;
            }
            const auto value = hex ? text.mid(2).toULongLong(&ok, 16) : text.toULongLong(&ok, 10);
            return ok ? std::optional(value) : std::nullopt;
        };
        const auto parsedBytes = [bytesEdit]() -> std::optional<QByteArray> {
            auto text = bytesEdit->text();
            text.remove(QLatin1Char(' '));
            static const QRegularExpression hex(QStringLiteral("^([0-9A-Fa-f]{2})+$"));
            if (!hex.match(text).hasMatch()) {
                return std::nullopt;
            }
            return QByteArray::fromHex(text.toLatin1());
        };
        const auto refresh = [&] {
            const auto at = parsedOffset();
            const auto bytes = parsedBytes();
            const auto size = tab->document().currentBytes().size();
            bool valid = at && bytes && *at < size &&
                         static_cast<qulonglong>(bytes->size()) <= size - *at;
            QString text;
            if (!at) {
                text = tr("Type an offset.");
            } else if (*at >= size) {
                text = tr("The file has %1 bytes.").arg(size);
            } else {
                const auto current = tab->document().currentBytes();
                const auto count = bytes ? static_cast<std::size_t>(bytes->size()) : 4u;
                QStringList now;
                for (std::size_t index = 0; index < count && *at + index < size; ++index) {
                    now << QStringLiteral("%1")
                               .arg(static_cast<unsigned>(current[*at + index]), 2, 16,
                                    QLatin1Char('0'))
                               .toUpper();
                }
                text = tr("Stored there now: %1").arg(now.join(QLatin1Char(' ')));
                if (!bytes) {
                    text += QLatin1Char('\n') + tr("Type whole bytes as pairs of hex digits.");
                } else if (!valid) {
                    text += QLatin1Char('\n') + tr("That many bytes do not fit in the file.");
                }
            }
            summary->setText(text);
            buttons->button(QDialogButtonBox::Ok)->setEnabled(valid);
        };
        connect(offsetEdit, &QLineEdit::textChanged, &dialog, refresh);
        connect(bytesEdit, &QLineEdit::textChanged, &dialog, refresh);
        refresh();
        dialog.resize(460, 180);
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }
        return std::make_pair(*parsedOffset(), *parsedBytes());
    };
    prompts.borderRulesPath = [this] {
        return QFileDialog::getOpenFileName(this, tr("Custom Border Rules"), QString(),
                                            tr("Border rules (*.txt *.rules);;All files (*)"));
    };
    prompts.confirmProtectedEdit = [this](DocumentTab *, const QString &description) {
        return QMessageBox::warning(this, tr("Edit Bytes"), description,
                                    QMessageBox::Yes | QMessageBox::Cancel,
                                    QMessageBox::Cancel) == QMessageBox::Yes;
    };
    prompts.mergeFiles = [this](DocumentTab *tab) -> std::optional<std::pair<QString, QString>> {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Merge Changes"));
        auto *form = new QFormLayout(&dialog);
        auto *intro = new QLabel(
            tr("Brings what another copy of this world changed into %1.\nChoose the common "
               "starting point both copies were made from, and the other copy.")
                .arg(QFileInfo(tab->file().path()).fileName()),
            &dialog);
        intro->setWordWrap(true);
        form->addRow(intro);
        const auto chooser = [&](const QString &label, const QString &name) {
            auto *edit = new QLineEdit(&dialog);
            edit->setObjectName(name);
            edit->setAccessibleName(label);
            auto *browse = new QPushButton(tr("Browse..."), &dialog);
            browse->setAccessibleName(tr("Browse for: %1").arg(label));
            connect(browse, &QPushButton::clicked, &dialog, [this, edit] {
                const auto path = QFileDialog::getOpenFileName(
                    this, tr("Merge Changes"), QFileInfo(edit->text()).absolutePath(),
                    tr("Blupi world files (*.blp *.BLP *.xch *.XCH);;All files (*)"));
                if (!path.isEmpty()) {
                    edit->setText(path);
                }
            });
            auto *row = new QHBoxLayout;
            row->addWidget(edit, 1);
            row->addWidget(browse);
            form->addRow(label, row);
            return edit;
        };
        auto *base = chooser(tr("Common base"), QStringLiteral("mergeBase"));
        auto *theirs = chooser(tr("Other copy"), QStringLiteral("mergeTheirs"));
        auto *buttons =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        dialog.resize(560, 180);
        if (dialog.exec() != QDialog::Accepted || base->text().isEmpty() ||
            theirs->text().isEmpty()) {
            return std::nullopt;
        }
        return std::make_pair(base->text(), theirs->text());
    };
    prompts.resolveMerge = [this](DocumentTab *tab, const MergeResult &result,
                                  std::span<const std::byte> mine,
                                  std::span<const std::byte> theirs)
        -> std::optional<std::vector<bool>> {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Merge Changes"));
        auto *layout = new QVBoxLayout(&dialog);
        auto *summary = new QLabel(
            tr("%1 element(s) changed only in the other copy are taken; %2 changed alike in "
               "both; %3 changed only here stay. %4 element(s) were changed differently in both "
               "copies: tick the ones to take from the other copy; the rest keep this world's "
               "bytes.")
                .arg(result.fromTheirs)
                .arg(result.agreed)
                .arg(result.fromMine)
                .arg(result.conflicts.size()),
            &dialog);
        summary->setObjectName(QStringLiteral("mergeSummary"));
        summary->setWordWrap(true);
        layout->addWidget(summary);
        auto *list = new QListWidget(&dialog);
        list->setObjectName(QStringLiteral("mergeConflicts"));
        list->setAccessibleName(tr("Conflicts: tick to take the other copy's bytes"));
        const auto hex = [](std::span<const std::byte> bytes, ByteRange range) {
            const auto shown = std::min<std::size_t>(range.size, 16);
            return QString::fromStdString(formatHex(bytes.subspan(range.offset, shown))) +
                   (shown < range.size ? QStringLiteral(" ...") : QString());
        };
        for (const auto &conflict : result.conflicts) {
            auto *item = new QListWidgetItem(
                tr("%1 (offset %2, %3 byte(s))\nhere: %4\nother: %5")
                    .arg(QString::fromStdString(
                             offsetLabel(tab->document().probe(), conflict.range.offset)))
                    .arg(conflict.range.offset)
                    .arg(conflict.range.size)
                    .arg(hex(mine, conflict.range), hex(theirs, conflict.range)),
                list);
            item->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            item->setCheckState(Qt::Unchecked);
        }
        list->setVisible(!result.conflicts.empty());
        layout->addWidget(list, 1);
        auto *all = new QHBoxLayout;
        auto *allMine = new QPushButton(tr("Keep All of This World"), &dialog);
        allMine->setObjectName(QStringLiteral("mergeAllMine"));
        auto *allTheirs = new QPushButton(tr("Take All from the Other Copy"), &dialog);
        allTheirs->setObjectName(QStringLiteral("mergeAllTheirs"));
        allMine->setVisible(!result.conflicts.empty());
        allTheirs->setVisible(!result.conflicts.empty());
        all->addWidget(allMine);
        all->addWidget(allTheirs);
        all->addStretch(1);
        layout->addLayout(all);
        const auto setAll = [list](Qt::CheckState state) {
            for (int row = 0; row < list->count(); ++row) {
                list->item(row)->setCheckState(state);
            }
        };
        connect(allMine, &QPushButton::clicked, &dialog, [setAll] { setAll(Qt::Unchecked); });
        connect(allTheirs, &QPushButton::clicked, &dialog, [setAll] { setAll(Qt::Checked); });
        auto *buttons =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        dialog.resize(640, 480);
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }
        std::vector<bool> choices;
        for (int row = 0; row < list->count(); ++row) {
            choices.push_back(list->item(row)->checkState() == Qt::Checked);
        }
        return choices;
    };
    prompts.stampName = [this](DocumentTab *) -> std::optional<std::pair<QString, bool>> {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Save Selection as Stamp"));
        auto *form = new QFormLayout(&dialog);
        auto *name = new QLineEdit(&dialog);
        name->setObjectName(QStringLiteral("stampNameEdit"));
        name->setMaxLength(60);
        form->addRow(tr("Name"), name);
        auto *objects = new QCheckBox(tr("Include the objects that start in the selection"),
                                      &dialog);
        objects->setObjectName(QStringLiteral("stampObjects"));
        objects->setChecked(true);
        form->addRow(objects);
        auto *buttons =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        form->addRow(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        if (dialog.exec() != QDialog::Accepted || name->text().trimmed().isEmpty()) {
            return std::nullopt;
        }
        return std::make_pair(name->text().trimmed(), objects->isChecked());
    };
    prompts.chooseStamp = [this](DocumentTab *tab,
                                 const std::vector<Stamp> &list) -> std::optional<std::size_t> {
        QStringList names;
        for (const auto &stamp : list) {
            names << tr("%1 (%2 x %3 cells, %4 object(s))")
                         .arg(QString::fromStdString(stamp.name))
                         .arg(stamp.width)
                         .arg(stamp.height)
                         .arg(stamp.objects.size());
        }
        const auto cursor = tab->canvas()->cursorCell();
        bool accepted = false;
        const auto chosen = QInputDialog::getItem(
            this, tr("Place Stamp"),
            tr("The stamp is placed with its top-left corner at the cursor cell (%1, %2). "
               "Empty cells of a stamp leave what is there.")
                .arg(cursor.x)
                .arg(cursor.y),
            names, 0, false, &accepted);
        const auto index = names.indexOf(chosen);
        if (!accepted || index < 0) {
            return std::nullopt;
        }
        return static_cast<std::size_t>(index);
    };
    prompts.confirmBatch = [this](const QString &title, const QStringList &lines) {
        return QMessageBox::question(this, title, lines.join(QLatin1Char('\n')),
                                     QMessageBox::Yes | QMessageBox::Cancel) == QMessageBox::Yes;
    };
    prompts.exportPack =
        [this](const std::vector<PackCandidate> &candidates) -> std::optional<PackExportChoice> {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Export Campaign Pack"));
        auto *layout = new QVBoxLayout(&dialog);
        auto *name = new QLineEdit(tr("My campaign"), &dialog);
        name->setObjectName(QStringLiteral("packName"));
        auto *form = new QFormLayout;
        form->addRow(tr("Name"), name);
        layout->addLayout(form);
        auto *list = new QListWidget(&dialog);
        list->setObjectName(QStringLiteral("packWorlds"));
        list->setAccessibleName(tr("Worlds to put into the pack"));
        for (const auto &candidate : candidates) {
            auto label = QFileInfo(candidate.path).fileName();
            if (!candidate.title.isEmpty()) {
                label += QStringLiteral(" - ") + candidate.title;
            }
            if (candidate.gameOwn) {
                label += QStringLiteral(" - ") + tr("the game's own file");
            }
            auto *item = new QListWidgetItem(label, list);
            item->setFlags(candidate.gameOwn ? Qt::ItemIsUserCheckable
                                             : Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            item->setCheckState(candidate.gameOwn ? Qt::Unchecked : Qt::Checked);
        }
        layout->addWidget(list, 1);
        auto *own = new QCheckBox(
            tr("Let me select the game's own worlds too (a pack with them must not be passed "
               "on: they belong to the game's publisher)"),
            &dialog);
        own->setObjectName(QStringLiteral("packGameOwn"));
        layout->addWidget(own);
        connect(own, &QCheckBox::toggled, &dialog, [&](bool allowed) {
            for (int row = 0; row < list->count(); ++row) {
                if (candidates[static_cast<std::size_t>(row)].gameOwn) {
                    auto *item = list->item(row);
                    item->setFlags(allowed ? Qt::ItemIsUserCheckable | Qt::ItemIsEnabled
                                           : Qt::ItemIsUserCheckable);
                    if (!allowed) {
                        item->setCheckState(Qt::Unchecked);
                    }
                }
            }
        });
        auto *zip = new QRadioButton(tr("One ZIP file"), &dialog);
        zip->setObjectName(QStringLiteral("packZip"));
        auto *folder = new QRadioButton(tr("A folder"), &dialog);
        zip->setChecked(true);
        layout->addWidget(zip);
        layout->addWidget(folder);
        auto *buttons =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        dialog.resize(560, 460);
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }
        PackExportChoice choice;
        choice.name = name->text().trimmed();
        choice.zip = zip->isChecked();
        for (int row = 0; row < list->count(); ++row) {
            if (list->item(row)->checkState() == Qt::Checked) {
                choice.chosen.push_back(static_cast<std::size_t>(row));
            }
        }
        if (choice.chosen.empty()) {
            return std::nullopt;
        }
        choice.path = choice.zip
                          ? QFileDialog::getSaveFileName(this, tr("Export Campaign Pack"),
                                                         choice.name + QStringLiteral(".zip"),
                                                         tr("ZIP archive (*.zip)"))
                          : QFileDialog::getSaveFileName(this, tr("New Folder for the Pack"),
                                                         choice.name);
        if (choice.path.isEmpty()) {
            return std::nullopt;
        }
        return choice;
    };
    prompts.importPackPath = [this] {
        return QFileDialog::getOpenFileName(this, tr("Import Campaign Pack"), QString(),
                                            tr("Campaign packs (*.zip eggbert-campaign.json);;"
                                               "All files (*)"));
    };
    prompts.confirmImportPack = [this](const CampaignPack &pack, const QStringList &replaced) {
        QStringList lines;
        for (const auto &entry : campaignOrder(pack.campaign)) {
            lines << QString::fromStdString(entry.fileName) +
                         (entry.title.empty()
                              ? QString()
                              : QStringLiteral(" - ") + QString::fromStdString(entry.title));
        }
        auto text = tr("\"%1\" holds %n world(s):", nullptr,
                       static_cast<int>(pack.campaign.entries.size()))
                        .arg(QString::fromStdString(pack.campaign.name)) +
                    QLatin1Char('\n') + lines.join(QLatin1Char('\n'));
        if (!replaced.isEmpty()) {
            text += QStringLiteral("\n\n") +
                    tr("These files exist and are replaced (each is kept and can be rolled "
                       "back):") +
                    QLatin1Char('\n') + replaced.join(QLatin1Char('\n'));
        }
        QMessageBox box(QMessageBox::Question, tr("Import Campaign Pack"),
                        tr("Put these worlds into the installation?"),
                        QMessageBox::Yes | QMessageBox::Cancel, this);
        box.setDetailedText(text);
        box.setInformativeText(tr("%n world(s), %1 replaced.", nullptr,
                                  static_cast<int>(pack.campaign.entries.size()))
                                   .arg(replaced.size()));
        return box.exec() == QMessageBox::Yes;
    };
    prompts.deploySlot = [this](DocumentTab *, WorldSlot suggested,
                                const QString &dataDirectory) -> std::optional<WorldSlot> {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Deploy Current World"));
        auto *layout = new QVBoxLayout(&dialog);
        layout->addWidget(new QLabel(
            tr("The world of the current tab, as it is now, is put into\n%1").arg(dataDirectory),
            &dialog));
        auto *user = new QRadioButton(tr("As a world you designed"), &dialog);
        user->setObjectName(QStringLiteral("deployUser"));
        auto *official = new QRadioButton(tr("In place of one of the game's own worlds"), &dialog);
        official->setObjectName(QStringLiteral("deployOfficial"));
        (suggested.user ? user : official)->setChecked(true);
        auto *form = new QFormLayout;
        auto *gamer = new QSpinBox(&dialog);
        gamer->setObjectName(QStringLiteral("deployGamer"));
        gamer->setRange(1, gamerCount);
        gamer->setValue(suggested.gamer);
        auto *number = new QSpinBox(&dialog);
        number->setObjectName(QStringLiteral("deployNumber"));
        form->addRow(tr("Player"), gamer);
        form->addRow(tr("World number"), number);
        layout->addWidget(user);
        layout->addWidget(official);
        layout->addLayout(form);
        auto *outcome = new QLabel(&dialog);
        outcome->setObjectName(QStringLiteral("deployOutcome"));
        outcome->setWordWrap(true);
        layout->addWidget(outcome);
        auto *buttons =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        const auto slot = [&] {
            return WorldSlot{user->isChecked(), gamer->value(), number->value()};
        };
        const auto refresh = [&] {
            gamer->setEnabled(user->isChecked());
            number->setRange(1, user->isChecked() ? userWorldCount : 999);
            const auto name = QString::fromStdString(worldSlotFileName(slot()));
            bool exists = false;
            for (const auto &entry : QDir(dataDirectory).entryList(QDir::Files)) {
                exists = exists || entry.compare(name, Qt::CaseInsensitive) == 0;
            }
            auto text = exists ? tr("%1 exists and is replaced. Its present content is kept and "
                                    "Roll Back Last Deploy puts it back.")
                                     .arg(name)
                               : tr("%1 is created.").arg(name);
            if (!user->isChecked()) {
                text += QLatin1Char('\n') +
                        tr("This is one of the game's own world numbers: the game will play "
                           "your world in its place.");
            }
            outcome->setText(text);
        };
        connect(user, &QRadioButton::toggled, &dialog, refresh);
        connect(gamer, &QSpinBox::valueChanged, &dialog, refresh);
        connect(number, &QSpinBox::valueChanged, &dialog, refresh);
        number->setRange(1, 999);
        number->setValue(suggested.number);
        refresh();
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }
        return slot();
    };
    prompts.confirmRollBack = [this](const DeployRecord &record) {
        return QMessageBox::question(
                   this, tr("Roll Back Last Deploy"),
                   record.hadPrevious
                       ? tr("Put back what %1 held before the deploy of %2?")
                             .arg(record.targetPath,
                                  QLocale().toString(record.time.toLocalTime(),
                                                     QLocale::ShortFormat))
                       : tr("Remove %1, which the deploy of %2 created?")
                             .arg(record.targetPath,
                                  QLocale().toString(record.time.toLocalTime(),
                                                     QLocale::ShortFormat))) ==
               QMessageBox::Yes;
    };
    prompts.playtestCommand = [this](GameProfile,
                                     const QString &current) -> std::optional<QString> {
        bool accepted = false;
        const auto text = QInputDialog::getText(
            this, tr("Playtest Command"),
            tr("The command that starts this game on your computer (for example the game's "
               "program, or wine followed by it).\nIt is started in the game directory. The "
               "games have no option to open a particular world, so you still choose it in "
               "the game."),
            QLineEdit::Normal, current, &accepted);
        if (!accepted) {
            return std::nullopt;
        }
        return text.trimmed();
    };
    prompts.startProcess = [](const QString &program, const QStringList &arguments,
                              const QString &workingDirectory) {
        return QProcess::startDetached(program, arguments, workingDirectory);
    };
    prompts.convertGame = [this](DocumentTab *tab, GameProfile target,
                                 const std::vector<GameConversionNote> &notes)
        -> std::optional<UnknownPolicy> {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Convert for Other Game"));
        auto *layout = new QVBoxLayout(&dialog);
        const auto *info = profileInfo(target);
        layout->addWidget(new QLabel(
            tr("A copy of %1 is written for %2; this file is not changed.")
                .arg(QFileInfo(tab->file().path()).fileName(),
                     QString::fromUtf8(info->name.data(),
                                       static_cast<qsizetype>(info->name.size()))),
            &dialog));
        const auto unknown = std::ranges::count_if(notes, [](const GameConversionNote &note) {
            return note.kind == CarryKind::unknownToTarget;
        });
        auto *list = new QListWidget(&dialog);
        list->setObjectName(QStringLiteral("convertGameNotes"));
        list->setAccessibleName(tr("What the other game does not know or shows differently"));
        list->setWordWrap(true);
        for (const auto &note : notes) {
            auto *item = new QListWidgetItem(describeGameNote(note), list);
            item->setToolTip(QString::fromStdString(note.provenance));
        }
        if (notes.empty()) {
            new QListWidgetItem(tr("Both games know everything this world stores."), list);
        }
        layout->addWidget(list, 1);
        auto *keep = new QRadioButton(
            tr("Keep the values the other game does not know (it ignores them)"), &dialog);
        keep->setObjectName(QStringLiteral("convertGameKeep"));
        auto *remove = new QRadioButton(
            tr("Remove them: empty those cells, free those object slots, backdrop 2"), &dialog);
        remove->setObjectName(QStringLiteral("convertGameRemove"));
        keep->setChecked(true);
        keep->setVisible(unknown != 0);
        remove->setVisible(unknown != 0);
        layout->addWidget(keep);
        layout->addWidget(remove);
        auto *buttons =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        dialog.resize(680, 380);
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }
        return remove->isChecked() ? UnknownPolicy::remove : UnknownPolicy::keep;
    };
    prompts.newWorldGame = [this](GameProfile suggested) -> std::optional<GameProfile> {
        const QStringList games{QString::fromUtf8(speedyBlupi1Info.name.data(),
                                                  static_cast<qsizetype>(speedyBlupi1Info.name.size())),
                                QString::fromUtf8(speedyBlupi2Info.name.data(),
                                                  static_cast<qsizetype>(speedyBlupi2Info.name.size()))};
        bool accepted = false;
        const auto chosen = QInputDialog::getItem(
            this, tr("New World"),
            tr("The new world starts as the game's own editor starts one:\n"
               "a small platform, one treasure and the goal.\n\nGame:"),
            games, suggested == GameProfile::speedyBlupi1 ? 0 : 1, false, &accepted);
        if (!accepted) {
            return std::nullopt;
        }
        return chosen == games[0] ? GameProfile::speedyBlupi1 : GameProfile::speedyBlupi2;
    };
    prompts.convertRevision =
        [this](DocumentTab *tab) -> std::optional<std::pair<FormatRevision, bool>> {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Convert Revision"));
        auto *layout = new QVBoxLayout(&dialog);
        layout->addWidget(new QLabel(
            tr("A copy of %1 is written in another revision; this file is not changed.")
                .arg(QFileInfo(tab->file().path()).fileName()),
            &dialog));
        auto *target = new QComboBox(&dialog);
        target->setAccessibleName(tr("Revision to convert to"));
        const auto *own = tab->document().probe().descriptor;
        for (const auto &revision : knownRevisions) {
            if (own == nullptr || revision.revision != own->revision) {
                target->addItem(tr("Revision %1.%2").arg(revision.major).arg(revision.minor),
                                static_cast<int>(revision.revision));
            }
        }
        layout->addWidget(target);
        auto *outcome = new QLabel(&dialog);
        outcome->setWordWrap(true);
        layout->addWidget(outcome);
        auto *allowLoss = new QCheckBox(tr("Convert anyway and lose what the target cannot store"),
                                        &dialog);
        layout->addWidget(allowLoss);
        auto *buttons =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        layout->addWidget(buttons);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        const auto refresh = [&] {
            const auto revision = static_cast<FormatRevision>(target->currentData().toInt());
            const auto result = convertRevision(tab->document().currentBytes(), revision,
                                                allowLoss->isChecked());
            QStringList lines;
            bool possible = result.has_value();
            if (result) {
                for (const auto &note : result->notes) {
                    lines << QString::fromStdString(note);
                }
                if (lines.isEmpty()) {
                    lines << tr("Nothing is added or dropped.");
                }
            } else {
                lines << QString::fromStdString(result.error().detail);
            }
            const bool lossQuestion =
                !result && result.error().error == ConversionError::wouldLoseData;
            allowLoss->setEnabled(lossQuestion || allowLoss->isChecked());
            outcome->setText(lines.join(QLatin1Char('\n')));
            buttons->button(QDialogButtonBox::Ok)->setEnabled(possible);
        };
        connect(target, &QComboBox::currentIndexChanged, &dialog, [&] {
            allowLoss->setChecked(false);
            refresh();
        });
        connect(allowLoss, &QCheckBox::toggled, &dialog, refresh);
        refresh();
        dialog.resize(520, 260);
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }
        return std::make_pair(static_cast<FormatRevision>(target->currentData().toInt()),
                              allowLoss->isChecked());
    };
    prompts.snapshotName = [this](DocumentTab *) -> std::optional<QString> {
        bool accepted = false;
        const auto name = QInputDialog::getText(this, tr("Take Snapshot"),
                                                tr("Name of this snapshot:"), QLineEdit::Normal,
                                                QString(), &accepted);
        return accepted ? std::optional<QString>(name) : std::nullopt;
    };
    prompts.chooseSnapshot =
        [this](DocumentTab *tab, const std::vector<RecoveryEntry> &entries)
        -> std::optional<std::pair<RecoveryEntry, SnapshotAction>> {
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Snapshots of %1").arg(QFileInfo(tab->file().path()).fileName()));
        auto *layout = new QVBoxLayout(&dialog);
        auto *list = new QListWidget(&dialog);
        list->setAccessibleName(tr("Snapshots of this world"));
        for (const auto &entry : entries) {
            list->addItem(tr("%1 - %2")
                              .arg(QLocale().toString(entry.saved, QLocale::ShortFormat),
                                   entry.name.isEmpty() ? tr("(unnamed)") : entry.name));
        }
        list->setCurrentRow(entries.empty() ? -1 : 0);
        layout->addWidget(list);
        auto *buttons = new QDialogButtonBox(&dialog);
        auto *restore = buttons->addButton(tr("Restore"), QDialogButtonBox::AcceptRole);
        auto *remove = buttons->addButton(tr("Delete"), QDialogButtonBox::DestructiveRole);
        buttons->addButton(QDialogButtonBox::Close);
        layout->addWidget(buttons);
        std::optional<SnapshotAction> chosen;
        connect(restore, &QPushButton::clicked, &dialog, [&] {
            chosen = SnapshotAction::restore;
            dialog.accept();
        });
        connect(remove, &QPushButton::clicked, &dialog, [&] {
            chosen = SnapshotAction::remove;
            dialog.accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        dialog.resize(460, 320);
        if (dialog.exec() != QDialog::Accepted || !chosen || list->currentRow() < 0) {
            return std::nullopt;
        }
        return std::make_pair(entries[static_cast<std::size_t>(list->currentRow())], *chosen);
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
        dialog.setExperimental(experimental_);
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }
        return dialog.metadata();
    };
    prompts.exportPng =
        [this](DocumentTab *tab) -> std::optional<std::pair<PngExportOptions, QString>> {
        PngExportDialog dialog(tab->canvas()->renderOptions(), artworkFor(tab->profile()), this);
        dialog.setSelectionBounds(tab->selection().bounds());
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
    prompts.chooseArtworkDirectory =
        [this](GameProfile suggested) -> std::optional<std::pair<QString, GameProfile>> {
        const auto directory = QFileDialog::getExistingDirectory(
            this, tr("Choose the Game Directory (it is only read)"));
        if (directory.isEmpty()) {
            return std::nullopt;
        }
        const auto detection = detectGameProfile(directory);
        QDialog dialog(this);
        dialog.setWindowTitle(tr("Game Artwork Directory"));
        auto *form = new QFormLayout(&dialog);
        auto *evidence = new QLabel(detection.evidence.join(QLatin1Char('\n')), &dialog);
        evidence->setWordWrap(true);
        form->addRow(new QLabel(directory, &dialog));
        form->addRow(evidence);
        auto *game = new QComboBox(&dialog);
        game->addItem(profileDisplayName(GameProfile::speedyBlupi1),
                      static_cast<int>(GameProfile::speedyBlupi1));
        game->addItem(profileDisplayName(GameProfile::speedyBlupi2),
                      static_cast<int>(GameProfile::speedyBlupi2));
        const auto preselected =
            detection.profile != GameProfile::unknown ? detection.profile : suggested;
        game->setCurrentIndex(std::max(0, game->findData(static_cast<int>(preselected))));
        form->addRow(tr("This directory belongs to"), game);
        auto *buttons =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        form->addRow(buttons);
        if (dialog.exec() != QDialog::Accepted) {
            return std::nullopt;
        }
        return std::pair{directory, static_cast<GameProfile>(game->currentData().toInt())};
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
        auto *language = new QComboBox(&dialog);
        language->setObjectName(QStringLiteral("languageCombo"));
        language->addItem(tr("Follow the system"), QStringLiteral("system"));
        language->addItem(QStringLiteral("English"), QStringLiteral("en"));
        language->addItem(QStringLiteral("Čeština"), QStringLiteral("cs"));
        language->setCurrentIndex(std::max(
            0, language->findData(settings.value(QStringLiteral("appearance/language"),
                                                 QStringLiteral("system")))));
        language->setToolTip(tr("Takes effect the next time the editor starts. Texts that are "
                                "not translated yet stay English."));
        form->addRow(tr("Language"), language);
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
        auto *experimental = new QCheckBox(
            tr("Experimental editing: Edit > Edit Bytes, and any music or region number"),
            &dialog);
        experimental->setObjectName(QStringLiteral("experimentalCheck"));
        experimental->setToolTip(
            tr("Lets you write values the editor has no evidence for. Every such edit is one "
               "undo step; preserved ranges ask again; the revision field and the file length "
               "never change."));
        experimental->setChecked(
            settings.value(QStringLiteral("experimental/enabled"), false).toBool());
        form->addRow(experimental);
        auto *buttons =
            new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
        form->addRow(buttons);
        if (dialog.exec() != QDialog::Accepted) {
            return false;
        }
        settings.setValue(QStringLiteral("appearance/theme"), theme->currentData());
        settings.setValue(QStringLiteral("appearance/language"), language->currentData());
        settings.setValue(QStringLiteral("view/grid"), grid->isChecked());
        settings.setValue(QStringLiteral("view/labels"), labels->isChecked());
        settings.setValue(QStringLiteral("inspector/bytesPerRow"), width->currentData());
        settings.setValue(QStringLiteral("experimental/enabled"), experimental->isChecked());
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
    connect(addAction(fileMenu, QStringLiteral("fileNew"), tr("&New World..."),
                      QKeySequence::New),
            &QAction::triggered, this, [this] { createWorld(); });
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
    connect(addAction(fileMenu, QStringLiteral("fileConvertRevision"),
                      tr("Convert Re&vision...")),
            &QAction::triggered, this, [this] { convertRevisionOf(currentTab()); });
    connect(addAction(fileMenu, QStringLiteral("fileMerge"), tr("&Merge Changes...")),
            &QAction::triggered, this, [this] { mergeInto(currentTab()); });
    connect(addAction(fileMenu, QStringLiteral("fileConvertGame"),
                      tr("Convert for Other &Game...")),
            &QAction::triggered, this, [this] { convertGameOf(currentTab()); });
    connect(addAction(fileMenu, QStringLiteral("fileDeploy"), tr("&Deploy to Installation...")),
            &QAction::triggered, this, [this] { deployWorld(currentTab()); });
    connect(addAction(fileMenu, QStringLiteral("fileRollBack"), tr("Roll &Back Last Deploy")),
            &QAction::triggered, this, [this] { rollBackLastDeploy(); });
    connect(addAction(fileMenu, QStringLiteral("filePlaytest"), tr("Pla&ytest"),
                      QKeySequence(Qt::Key_F5)),
            &QAction::triggered, this, [this] { playtest(); });
    connect(addAction(fileMenu, QStringLiteral("fileCampaignOverview"),
                      tr("Campaign O&verview")),
            &QAction::triggered, this,
            [this] { prompts.showReport(tr("Campaign Overview"), campaignOverview()); });
    connect(addAction(fileMenu, QStringLiteral("fileExportPack"),
                      tr("E&xport Campaign Pack...")),
            &QAction::triggered, this, [this] { exportCampaignPack(); });
    connect(addAction(fileMenu, QStringLiteral("fileImportPack"),
                      tr("&Import Campaign Pack...")),
            &QAction::triggered, this, [this] {
                const auto path = prompts.importPackPath();
                if (!path.isEmpty()) {
                    importCampaignPack(path);
                }
            });
    connect(addAction(fileMenu, QStringLiteral("fileSnapshot"), tr("Take S&napshot...")),
            &QAction::triggered, this, [this] { takeSnapshot(currentTab()); });
    connect(addAction(fileMenu, QStringLiteral("fileSnapshots"), tr("Snaps&hots...")),
            &QAction::triggered, this, [this] { manageSnapshots(currentTab()); });
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
    connect(addAction(editMenu, QStringLiteral("editSaveStamp"),
                      tr("Save Selection as S&tamp...")),
            &QAction::triggered, this, [this] { saveSelectionAsStamp(currentTab()); });
    connect(addAction(editMenu, QStringLiteral("editPlaceStamp"), tr("P&lace Stamp...")),
            &QAction::triggered, this, [this] { placeStamp(currentTab()); });
    connect(addAction(editMenu, QStringLiteral("editPlaytestCommand"),
                      tr("Pla&ytest Command...")),
            &QAction::triggered, this, [this] {
                auto *tab = currentTab();
                const auto profile = tab != nullptr ? tab->profile() : GameProfile::unknown;
                if (profile == GameProfile::unknown) {
                    return;
                }
                const auto key = QStringLiteral("playtest/command%1")
                                     .arg(profile == GameProfile::speedyBlupi1 ? 1 : 2);
                if (const auto text =
                        prompts.playtestCommand(profile, QSettings().value(key).toString())) {
                    QSettings().setValue(key, *text);
                }
            });
    connect(addAction(editMenu, QStringLiteral("editBytes"), tr("Edit &Bytes...")),
            &QAction::triggered, this, [this] { editBytes(currentTab()); });
    connect(addAction(editMenu, QStringLiteral("editBorderRules"),
                      tr("Load Custom Border R&ules...")),
            &QAction::triggered, this, [this] {
                const auto path = prompts.borderRulesPath();
                if (!path.isEmpty()) {
                    loadCustomBorderRules(path);
                }
            });
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
    connect(addAction(helpMenu, QStringLiteral("helpEncyclopedia"),
                      tr("Element &Encyclopedia...")),
            &QAction::triggered, this, [this] { showEncyclopedia(); });
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
    connect(addAction(layerMenu, QStringLiteral("layerReplaceAll"),
                      tr("Replace Value in &All Open Worlds...")),
            &QAction::triggered, this, [this] { replaceInAllOpenWorlds(); });
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
    {
        auto *alignMenu = objectMenu->addMenu(tr("&Align Objects in Selection"));
        alignMenu->setObjectName(QStringLiteral("objectAlignMenu"));
        const std::tuple<const char *, QString, AlignMode> modes[] = {
            {"objectAlignLeft", tr("&Left"), AlignMode::left},
            {"objectAlignRight", tr("&Right"), AlignMode::right},
            {"objectAlignTop", tr("&Top"), AlignMode::top},
            {"objectAlignBottom", tr("&Bottom"), AlignMode::bottom},
            {"objectDistributeH", tr("Distribute &Horizontally"),
             AlignMode::distributeHorizontally},
            {"objectDistributeV", tr("Distribute &Vertically"), AlignMode::distributeVertically},
        };
        for (const auto &[name, label, mode] : modes) {
            connect(addAction(alignMenu, QString::fromLatin1(name), label), &QAction::triggered,
                    this, [this, mode = mode] {
                        if (auto *tab = currentTab()) {
                            tab->alignSelectedObjects(mode);
                        }
                    });
        }
    }
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
        {"viewBackdrop", tr("Show Back&drop Picture"), &RenderOptions::showBackdrop, {}},
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
    auto *artworkToggle = addAction(viewMenu, QStringLiteral("viewArtwork"),
                                    tr("Draw with Game &Artwork"));
    artworkToggle->setCheckable(true);
    connect(artworkToggle, &QAction::toggled, this, [this](bool enabled) {
        if (!updating_) {
            setArtworkEnabled(enabled);
        }
    });
    connect(addAction(viewMenu, QStringLiteral("viewArtworkDirectory"),
                      tr("Game Artwork &Directory...")),
            &QAction::triggered, this, [this] {
                auto *tab = currentTab();
                const auto choice = prompts.chooseArtworkDirectory(
                    tab != nullptr ? tab->profile() : GameProfile::unknown);
                if (!choice) {
                    return;
                }
                loadArtwork(choice->first, choice->second);
                prompts.showDocument(tr("Game Artwork"), artworkReport(), false);
            });
    {
        auto *animate = addAction(viewMenu, QStringLiteral("viewAnimate"), tr("&Animate"));
        animate->setCheckable(true);
        animate->setChecked(animate_);
        animate->setStatusTip(tr("Show animated tiles and moving objects as time passes, as the "
                                 "games' own editors do. Nothing stored changes."));
        connect(animate, &QAction::toggled, this, [this](bool checked) {
            if (!updating_) {
                setAnimated(checked);
            }
        });
    }
    connect(addAction(viewMenu, QStringLiteral("viewArtworkReport"), tr("Game Artwork Re&port")),
            &QAction::triggered, this,
            [this] { prompts.showDocument(tr("Game Artwork"), artworkReport(), false); });
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
    toolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    toolBar->setIconSize(QSize(28, 28));
    auto *toolGroup = new QActionGroup(this);
    const auto addTool = [&](const QString &name, const QString &text, Tool tool,
                             const QKeySequence &shortcut) {
        auto *item = new QAction(text, this);
        item->setObjectName(name);
        item->setCheckable(true);
        item->setShortcut(shortcut);
        toolGroup->addAction(item);
        toolBar->addAction(item);
        toolActions_.emplace_back(tool, item);
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
            // An explicit choice is remembered for this file.
            rememberProfile(tab->file().path(), tab->profile());
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
    modeCombo->setAccessibleName(tr("What the minimap shows"));
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
    // Visual chooser for tiles, objects and border brushes, on the left.
    paletteDock_ = new QDockWidget(tr("Palette"), this);
    paletteDock_->setObjectName(QStringLiteral("paletteDock"));
    palette_ = new PalettePanel(paletteDock_);
    paletteDock_->setWidget(palette_);
    addDockWidget(Qt::LeftDockWidgetArea, paletteDock_);
    installationDock_ = new QDockWidget(tr("Installation"), this);
    installationDock_->setObjectName(QStringLiteral("installationDock"));
    installation_ = new InstallationPanel(installationDock_);
    installationDock_->setWidget(installation_);
    addDockWidget(Qt::LeftDockWidgetArea, installationDock_);
    tabifyDockWidget(paletteDock_, installationDock_);
    installationDock_->hide();
    paletteDock_->raise();
    connect(installation_, &InstallationPanel::openRequested, this,
            [this](const QString &path) { openPath(path); });
    connect(installation_, &InstallationPanel::deployRequested, this,
            [this] { deployWorld(currentTab()); });
    connect(installation_, &InstallationPanel::rollBackRequested, this,
            [this] { rollBackLastDeploy(); });
    connect(installation_, &InstallationPanel::playtestRequested, this, [this] { playtest(); });
    connect(installation_, &InstallationPanel::chooseDirectoryRequested, this, [this] {
        if (auto *item = action(QStringLiteral("viewArtworkDirectory"))) {
            item->trigger();
        }
    });
    haveDeploys_ = !deployStore_.records().empty();
    connect(palette_, &PalettePanel::tileChosen, this, [this](int layer, int value) {
        auto *tab = currentTab();
        if (tab == nullptr || !tab->document().isWritable()) {
            return;
        }
        // The group a tile was picked from decides the layer it is for.
        const auto wanted = static_cast<SectionKind>(layer);
        if (tab->activeLayer() != wanted && !tab->setActiveLayer(wanted)) {
            return;
        }
        tileSpin_->setValue(value);
        tab->setTileId(static_cast<std::int16_t>(value));
        // A picked tile is meant to be painted: keep a painting tool, or
        // switch to the pencil from a tool that does not paint tiles.
        const auto tool = tab->tool();
        // (The adaptive brush always works on the foreground, so it is kept
        // only for a foreground tile.)
        const bool adaptiveWithTile =
            tool == Tool::adaptive && wanted == SectionKind::foreground &&
            tab->adaptiveBrush() == QString::fromLatin1(DocumentTab::adaptiveTileBrush);
        if (tool != Tool::pencil && tool != Tool::line && tool != Tool::rectangle &&
            tool != Tool::fill && !adaptiveWithTile) {
            tab->setTool(Tool::pencil);
        }
        updateUi();
    });
    connect(palette_, &PalettePanel::presetChosen, this, [this](const QString &id) {
        if (choosePreset(id)) {
            updateUi();
        }
    });
    connect(palette_, &PalettePanel::brushChosen, this, [this](const QString &id) {
        auto *tab = currentTab();
        if (tab == nullptr || !tab->document().isWritable() ||
            tab->profile() == GameProfile::unknown) {
            return;
        }
        tab->setAdaptiveBrush(id);
        tab->setTool(Tool::adaptive);
        updateUi();
    });
    // The World tab: each choice is one undo step on the header.
    const auto editHeader = [this](const std::function<void(Metadata &)> &change) {
        auto *tab = currentTab();
        if (tab == nullptr) {
            return false;
        }
        auto metadata = readMetadata(tab->document().view());
        if (!metadata) {
            return false;
        }
        change(*metadata);
        const bool changed = tab->applyMetadataEdit(*metadata);
        updateUi();
        return changed;
    };
    connect(palette_, &PalettePanel::worldSizeChosen, this,
            [editHeader](bool horizontally, bool vertically) {
                editHeader([&](Metadata &metadata) {
                    metadata.scrollsHorizontally = horizontally;
                    metadata.scrollsVertically = vertically;
                });
            });
    connect(palette_, &PalettePanel::regionChosen, this, [editHeader](int region) {
        editHeader([&](Metadata &metadata) { metadata.region = static_cast<std::int16_t>(region); });
    });
    connect(palette_, &PalettePanel::musicChosen, this, [this, editHeader](int number) {
        editHeader([&](Metadata &metadata) { metadata.music = static_cast<std::int16_t>(number); });
        if (palette_->playWhenChosen()) {
            if (number > 0) {
                playMusic(number);
            } else {
                music_->stop();
            }
        }
    });
    connect(palette_, &PalettePanel::musicPlayRequested, this,
            [this](int number) { playMusic(number); });
    connect(palette_, &PalettePanel::musicStopRequested, this, [this] { music_->stop(); });
    music_ = new MusicPlayer(this);
    connect(music_, &MusicPlayer::stateChanged, this, [this] { updateMusicStatus(); });

    connect(palette_, &PalettePanel::artworkDirectoryRequested, this, [this] {
        if (auto *item = action(QStringLiteral("viewArtworkDirectory"))) {
            item->trigger();
        }
    });
    auto *paletteToggle = paletteDock_->toggleViewAction();
    paletteToggle->setObjectName(QStringLiteral("viewPalette"));
    paletteToggle->setText(tr("&Palette"));

    auto *installationToggle = installationDock_->toggleViewAction();
    installationToggle->setObjectName(QStringLiteral("viewInstallation"));
    installationToggle->setText(tr("&Installation"));

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
    viewMenu->addAction(paletteToggle);
    viewMenu->addAction(installationToggle);
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
        show(paletteDock_, true);
        show(objectsDock_, false);
        show(inspectorDock_, false);
        show(diagnosticsDock_, false);
        show(minimapDock_, true);
        show(statisticsDock_, true);
        break;
    case DockLayout::objects:
        show(paletteDock_, true);
        show(statisticsDock_, false);
        show(inspectorDock_, false);
        show(diagnosticsDock_, false);
        show(minimapDock_, true);
        show(objectsDock_, true);
        break;
    case DockLayout::validation:
        show(paletteDock_, false);
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
    refreshToolIcons();
}

void MainWindow::refreshToolIcons() {
    const auto ink = QApplication::palette().color(QPalette::WindowText);
    for (const auto &[tool, item] : toolActions_) {
        item->setIcon(toolIcon(tool, ink));
    }
}

bool MainWindow::welcomeVisible() const {
    return central_->currentWidget() == welcome_;
}

void MainWindow::refreshWelcome() {
    central_->setCurrentWidget(tabCount() == 0 ? welcome_ : static_cast<QWidget *>(tabs_));
    if (tabCount() != 0) {
        return;
    }
    const auto files = recentFiles();
    QStringList shown;
    for (int row = 0; row < welcomeRecent_->count(); ++row) {
        shown << welcomeRecent_->item(row)->data(Qt::UserRole).toString();
    }
    if (shown == files) {
        return;
    }
    welcomeRecent_->clear();
    for (const auto &path : files) {
        auto *item = new QListWidgetItem(QFileInfo(path).fileName(), welcomeRecent_);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
    }
}

GameProfile MainWindow::rememberedProfile(const QString &path) {
    const auto key = samePathKey(path);
    for (const auto &entry : QSettings().value(QStringLiteral("files/games")).toStringList()) {
        if (entry.mid(2) == key) {
            return entry.startsWith(QLatin1Char('2'))   ? GameProfile::speedyBlupi2
                   : entry.startsWith(QLatin1Char('1')) ? GameProfile::speedyBlupi1
                                                        : GameProfile::unknown;
        }
    }
    return GameProfile::unknown;
}

void MainWindow::rememberProfile(const QString &path, GameProfile profile) {
    const auto key = samePathKey(path);
    QSettings settings;
    auto entries = settings.value(QStringLiteral("files/games")).toStringList();
    entries.removeIf([&](const QString &entry) { return entry.mid(2) == key; });
    if (profile != GameProfile::unknown) {
        entries.prepend((profile == GameProfile::speedyBlupi2 ? QStringLiteral("2|")
                                                               : QStringLiteral("1|")) +
                        key);
    }
    while (entries.size() > 300) {
        entries.removeLast();
    }
    settings.setValue(QStringLiteral("files/games"), entries);
}

void MainWindow::adoptGameOfFolder(DocumentTab *tab) {
    // An explicit earlier choice for this file wins.
    if (const auto remembered = rememberedProfile(tab->file().path());
        remembered != GameProfile::unknown) {
        tab->setProfile(remembered);
        return;
    }
    // A world inside a game's DATA folder belongs to that game.
    const QFileInfo info(tab->file().path());
    const auto folder = info.dir();
    if (folder.dirName().compare(QLatin1String("data"), Qt::CaseInsensitive) != 0) {
        return;
    }
    auto gameDirectory = folder;
    if (!gameDirectory.cdUp()) {
        return;
    }
    const auto detected = detectGameProfile(gameDirectory.absolutePath());
    if (detected.profile == GameProfile::unknown) {
        return;
    }
    tab->setProfile(detected.profile);
    // Its graphics too, unless a directory was already chosen for that game.
    if (gameDirectoryFor(detected.profile) == nullptr) {
        loadArtwork(gameDirectory.absolutePath(), detected.profile);
    }
    statusBar()->showMessage(
        tr("%1: recognised from its folder as %2.")
            .arg(info.fileName(), profileDisplayName(detected.profile)),
        8000);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        for (const auto &url : event->mimeData()->urls()) {
            if (url.isLocalFile()) {
                event->acceptProposedAction();
                return;
            }
        }
    }
}

void MainWindow::dropEvent(QDropEvent *event) {
    bool opened = false;
    for (const auto &url : event->mimeData()->urls()) {
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isFile()) {
            opened = openPath(url.toLocalFile()) != nullptr || opened;
        }
    }
    if (opened) {
        event->acceptProposedAction();
    }
}

void MainWindow::applyPreferences() {
    const QSettings settings;
    setTheme(settings.value(QStringLiteral("appearance/theme"), QStringLiteral("system")).toString());
    inspector_->setBytesPerRow(settings.value(QStringLiteral("inspector/bytesPerRow"), 16).toInt());
    experimental_ = settings.value(QStringLiteral("experimental/enabled"), false).toBool();
    shareCustomRules();
    updateUi();
}

void MainWindow::setExperimental(bool enabled) {
    QSettings().setValue(QStringLiteral("experimental/enabled"), enabled);
    experimental_ = enabled;
    shareCustomRules();
    updateUi();
}

void MainWindow::shareCustomRules() {
    // Tabs see the rules only while experimental editing is on.
    const auto rules = experimental_ ? customRules_ : nullptr;
    for (int index = 0; index < tabCount(); ++index) {
        tabAt(index)->setCustomBorderRules(rules);
    }
    presetProfile_ = GameProfile::unknown; // the brush chooser is rebuilt
    presetArtwork_ = nullptr;
    rebuildPresets(currentTab() != nullptr ? currentTab()->profile() : GameProfile::unknown);
}

bool MainWindow::loadCustomBorderRules(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024) {
        prompts.error(tr("Custom Border Rules"),
                      tr("%1 cannot be read (or is larger than 1 MiB).").arg(path));
        return false;
    }
    const auto text = file.readAll();
    auto parsed = parseCustomBorderRules(
        std::string_view(text.constData(), static_cast<std::size_t>(text.size())));
    if (!parsed) {
        prompts.error(tr("Custom Border Rules"),
                      tr("%1\n%2").arg(path, QString::fromStdString(parsed.error())));
        return false;
    }
    customRules_ =
        std::make_shared<const std::vector<CustomBorderRule>>(std::move(parsed.value()));
    QSettings().setValue(QStringLiteral("experimental/borderRules"), path);
    shareCustomRules();
    updateUi();
    statusBar()->showMessage(
        tr("%n custom border rule(s) loaded", nullptr, static_cast<int>(customRules_->size())),
        6000);
    return true;
}

bool MainWindow::editBytes(DocumentTab *tab) {
    if (tab == nullptr || !experimental_ || !tab->document().isWritable()) {
        return false;
    }
    const auto choice =
        prompts.editBytes(tab, static_cast<qulonglong>(inspector_->cursorOffset()));
    if (!choice || choice->second.isEmpty()) {
        return false;
    }
    const auto offset = static_cast<std::size_t>(choice->first);
    const auto &bytes = choice->second;
    EditBuilder builder(tab->document());
    if (!builder.setBytes(offset, std::span<const std::byte>(
                                      reinterpret_cast<const std::byte *>(bytes.constData()),
                                      static_cast<std::size_t>(bytes.size())))) {
        prompts.error(tr("Edit Bytes"),
                      tr("%1 byte(s) at offset %2 do not fit in the file (%3 bytes); its "
                         "length never changes.")
                          .arg(bytes.size())
                          .arg(offset)
                          .arg(tab->document().currentBytes().size()));
        return false;
    }
    auto transaction =
        builder.build(tr("Edit bytes at offset %1").arg(offset).toStdString());
    if (transaction.empty()) {
        statusBar()->showMessage(tr("Nothing would change."), 5000);
        return false;
    }
    for (const auto &change : transaction.changes) {
        if (change.offset < layout::revisionFieldBytes) {
            prompts.error(tr("Edit Bytes"),
                          tr("The revision field (offsets 0 to 3) selects the layout of the "
                             "whole file and is never edited. Use File > Convert Revision."));
            return false;
        }
    }
    const auto parts = tab->document().protectedPartsOf(transaction);
    if (!parts.empty()) {
        QStringList lines;
        for (const auto &part : parts) {
            const auto location = locateOffset(tab->document().probe(), part.offset);
            const auto what =
                location.headerField
                    ? QString::fromUtf8(headerField(*location.headerField).name.data(),
                                        static_cast<qsizetype>(
                                            headerField(*location.headerField).name.size()))
                    : tr("bytes after the world");
            lines << tr("offsets %1 to %2 (%3)")
                         .arg(part.offset)
                         .arg(part.end() - 1)
                         .arg(what);
        }
        if (!prompts.confirmProtectedEdit(
                tab, tr("This edit changes bytes the editor normally preserves because their "
                        "meaning is not verified:\n%1\n\nChange them anyway?")
                         .arg(lines.join(QLatin1Char('\n'))))) {
            return false;
        }
        return tab->applyExpertTransaction(std::move(transaction));
    }
    return tab->applyTransaction(std::move(transaction));
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
    paletteDock_->show();
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

std::shared_ptr<const GameAssets> MainWindow::artworkFor(GameProfile profile) const {
    const auto index = artworkIndex(profile);
    if (index < 0 || artwork_[index] == nullptr || !artwork_[index]->usable()) {
        return nullptr;
    }
    return artwork_[index];
}

std::shared_ptr<const GameAssets> MainWindow::gameDirectoryFor(GameProfile profile) const {
    const auto index = artworkIndex(profile);
    return index < 0 ? nullptr : artwork_[index];
}

bool MainWindow::loadArtwork(const QString &directory, GameProfile profile) {
    const auto index = artworkIndex(profile);
    if (index < 0) {
        return false;
    }
    artwork_[index] = GameAssets::load(directory, profile);
    // The path is a local preference; it is never written into a world file.
    QSettings().setValue(artworkSettingKey(profile), artwork_[index]->directory());
    if (artwork_[index]->usable()) {
        setArtworkEnabled(true);
    }
    applyArtworkToTabs();
    updateUi();
    return artwork_[index]->usable();
}

void MainWindow::setArtworkEnabled(bool enabled) {
    artworkEnabled_ = enabled;
    QSettings().setValue(QStringLiteral("artwork/enabled"), enabled);
    applyArtworkToTabs();
    updateUi();
}

void MainWindow::setAnimated(bool animated) {
    animate_ = animated;
    QSettings().setValue(QStringLiteral("view/animate"), animated);
    applyArtworkToTabs();
    updateUi();
}

void MainWindow::applyArtworkToTabs() {
    for (int index = 0; index < tabCount(); ++index) {
        auto *tab = tabAt(index);
        tab->canvas()->setAnimated(animate_);
        auto options = tab->canvas()->renderOptions();
        options.artwork = artworkEnabled_ ? artworkFor(tab->profile()) : nullptr;
        tab->canvas()->setRenderOptions(options);
    }
}

QString MainWindow::artworkReport() const {
    QStringList lines;
    lines << tr("Artwork is read from a game directory you choose. The directory is never "
                "modified, nothing from it is copied into world files, and the editor works "
                "without it.");
    lines << tr("Sprite positions are located in the game's own executable and accepted only "
                "when they fit the sprite sheets. This has been checked with installations of "
                "Speedy Eggbert 1 and Speedy Eggbert 2; other editions may differ, and anything "
                "that cannot be validated is drawn schematically.");
    for (const auto profile : {GameProfile::speedyBlupi1, GameProfile::speedyBlupi2}) {
        lines << QString() << profileDisplayName(profile);
        const auto &assets = artwork_[artworkIndex(profile)];
        if (assets == nullptr) {
            lines << tr("  No directory chosen.");
            continue;
        }
        lines << QStringLiteral("  ") + assets->directory();
        for (const auto &line : assets->report()) {
            lines << QStringLiteral("  ") + line;
        }
    }
    return lines.join(QLatin1Char('\n'));
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
    connect(tab, &DocumentTab::documentChanged, autosaveTimer_, qOverload<>(&QTimer::start));
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
    // A Mobile Eggbert text world is a different kind of document: it is
    // described, not opened for editing, and never converted.
    {
        QFile file(path);
        if (file.size() <= 4 * 1024 * 1024 && file.open(QIODevice::ReadOnly)) {
            const auto bytes = file.readAll();
            const std::string_view text(bytes.constData(), static_cast<std::size_t>(bytes.size()));
            if (looksLikeTextWorld(text)) {
                prompts.showReport(
                    tr("Mobile Eggbert Text World"),
                    QFileInfo(path).fileName() + QLatin1Char('\n') +
                        QString::fromStdString(TextWorld::parse(text).describe()));
                return nullptr;
            }
        }
    }
    auto loaded = DocumentFile::load(path);
    if (!loaded) {
        prompts.error(tr("Open World"),
                      tr("%1 could not be opened.\n%2").arg(path, loaded.error().detail));
        return nullptr;
    }
    auto *tab = new DocumentTab(std::move(loaded.value()), tabs_);
    tab->setCustomBorderRules(experimental_ ? customRules_ : nullptr);
    {
        const QSettings settings;
        auto options = tab->canvas()->renderOptions();
        options.showGrid = settings.value(QStringLiteral("view/grid"), options.showGrid).toBool();
        options.showLabels =
            settings.value(QStringLiteral("view/labels"), options.showLabels).toBool();
        tab->canvas()->setRenderOptions(options);
    }
    connectTab(tab);
    adoptGameOfFolder(tab);
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
        flushAutosave();
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
    const auto previousPath = tab->file().path();
    const auto path = prompts.saveAsPath(previousPath);
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
    recovery_.removeAutosave(previousPath);
    flushAutosave();
    // As with Save: a full audit that informs and never blocks.
    const auto report = runDoctor(tab->document().view(), tab->profile());
    statusBar()->showMessage(tr("Saved %1 - World Doctor: %2 error(s), %3 warning(s)")
                                 .arg(tab->file().path())
                                 .arg(report.count(Severity::error))
                                 .arg(report.count(Severity::warning)),
                             8000);
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
    flushAutosave();
    return true;
}

void MainWindow::flushAutosave() {
    autosaveTimer_->stop();
    for (int index = 0; index < tabCount(); ++index) {
        auto *tab = tabAt(index);
        const auto &document = tab->document();
        if (document.isModified()) {
            recovery_.writeAutosave(tab->file().path(), document.savedBytes(),
                                    document.currentBytes());
        } else {
            recovery_.removeAutosave(tab->file().path());
        }
    }
}

int MainWindow::checkRecovery() {
    int restored = 0;
    const auto damaged = recovery_.damagedFiles();
    if (!damaged.isEmpty()) {
        prompts.error(tr("Recovery"),
                      tr("%n file(s) in the recovery folder are damaged and were not used:\n%1",
                         nullptr, static_cast<int>(damaged.size()))
                          .arg(damaged.join(QLatin1Char('\n'))));
    }
    for (const auto &entry : recovery_.autosaves()) {
        const auto payload = recovery_.read(entry);
        if (!payload) {
            continue;
        }
        // Restoring is offered only onto the very bytes the copy was made from.
        QString detail = tr("The world file is unchanged since then.");
        bool canRestore = false;
        {
            QFile file(entry.originalPath);
            if (!file.exists()) {
                detail = tr("The world file no longer exists; the changes can be saved as a copy.");
            } else if (!file.open(QIODevice::ReadOnly)) {
                detail = tr("The world file cannot be read; the changes can be saved as a copy.");
            } else {
                const auto onDisk = file.readAll();
                canRestore = onDisk.size() == entry.baseSize &&
                             QCryptographicHash::hash(onDisk, QCryptographicHash::Sha256) ==
                                 entry.baseSha256 &&
                             onDisk.size() == payload->size();
                if (!canRestore) {
                    detail = tr("The world file has changed since then; the changes can be saved "
                                "as a copy.");
                }
            }
        }
        const auto wanted = std::span<const std::byte>(
            reinterpret_cast<const std::byte *>(payload->constData()),
            static_cast<std::size_t>(payload->size()));
        switch (prompts.recover(entry, canRestore, detail)) {
        case RecoveryChoice::restore: {
            if (!canRestore) {
                break;
            }
            auto *tab = openPath(entry.originalPath);
            if (tab != nullptr &&
                tab->replaceBytes(wanted, tr("Restore Unsaved Changes"))) {
                ++restored;
                recovery_.remove(entry);
                flushAutosave(); // the restored state is unsaved again
            } else {
                prompts.error(tr("Recovery"),
                              tr("The changes to %1 could not be restored; they are kept in the "
                                 "recovery folder.")
                                  .arg(entry.originalPath));
            }
            break;
        }
        case RecoveryChoice::saveCopy: {
            const auto target = prompts.saveAsPath(entry.originalPath + QStringLiteral(".recovered"));
            if (target.isEmpty()) {
                break;
            }
            QSaveFile file(target);
            file.setDirectWriteFallback(false);
            if (file.open(QIODevice::WriteOnly) && file.write(*payload) == payload->size() &&
                file.commit()) {
                recovery_.remove(entry);
            } else {
                prompts.error(tr("Recovery"), tr("%1 was not written.").arg(target));
            }
            break;
        }
        case RecoveryChoice::discard:
            recovery_.remove(entry);
            break;
        case RecoveryChoice::keep:
            break;
        }
    }
    return restored;
}

EncyclopediaDialog *MainWindow::showEncyclopedia() {
    if (encyclopedia_ == nullptr) {
        encyclopedia_ = new EncyclopediaDialog(this);
        // The same choices the Palette makes, through the same handlers.
        connect(encyclopedia_, &EncyclopediaDialog::tileChosen, palette_,
                &PalettePanel::tileChosen);
        connect(encyclopedia_, &EncyclopediaDialog::presetChosen, palette_,
                &PalettePanel::presetChosen);
        connect(encyclopedia_, &EncyclopediaDialog::brushChosen, palette_,
                &PalettePanel::brushChosen);
    }
    auto *tab = currentTab();
    const auto profile = tab != nullptr ? tab->profile() : GameProfile::unknown;
    // The same pictures as the Palette: the game's, while drawing with game
    // artwork is on.
    encyclopedia_->setGame(profile, artworkEnabled_ ? artworkFor(profile) : nullptr);
    encyclopedia_->show();
    encyclopedia_->raise();
    encyclopedia_->activateWindow();
    return encyclopedia_;
}

DocumentTab *MainWindow::createWorld() {
    auto suggestedGame = GameProfile::speedyBlupi2;
    QString directory;
    if (auto *tab = currentTab()) {
        if (tab->profile() != GameProfile::unknown) {
            suggestedGame = tab->profile();
        }
        directory = QFileInfo(tab->file().path()).absolutePath();
    }
    const auto game = prompts.newWorldGame(suggestedGame);
    if (!game || *game == GameProfile::unknown) {
        return nullptr;
    }
    const auto name = QStringLiteral("new-world.blp");
    const auto path =
        prompts.saveAsPath(directory.isEmpty() ? name : QDir(directory).filePath(name));
    if (path.isEmpty()) {
        return nullptr;
    }
    const auto key = samePathKey(path);
    for (int index = 0; index < tabCount(); ++index) {
        if (samePathKey(tabAt(index)->file().path()) == key) {
            prompts.error(tr("New World"),
                          tr("%1 is open in the editor. Choose another file.").arg(path));
            return nullptr;
        }
    }
    const auto bytes = newWorld(*game);
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    const auto size = static_cast<qint64>(bytes.size());
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(reinterpret_cast<const char *>(bytes.data()), size) != size ||
        !file.commit()) {
        prompts.error(tr("New World"), tr("%1 was not written.").arg(path));
        return nullptr;
    }
    rememberProfile(path, *game);
    auto *opened = openPath(path);
    if (opened != nullptr) {
        opened->setProfile(*game);
        // The platform with the starts, the treasure and the goal.
        opened->canvas()->fitCells(CellPos{0, 0}, CellPos{12, 8});
    }
    return opened;
}

void MainWindow::refreshInstallationActions() {
    auto *tab = currentTab();
    const auto profile = tab != nullptr ? tab->profile() : GameProfile::unknown;
    const auto game = gameDirectoryFor(profile);
    const auto directory = game != nullptr ? game->directory() : QString();
    if (installation_->gameDirectory() != directory) {
        installation_->setInstallation(directory, profile);
    }
    bool complete = false;
    if (tab != nullptr) {
        const auto &probe = tab->document().probe();
        complete = probe.descriptor != nullptr && (probe.status == ProbeStatus::sizeMatches ||
                                                   probe.status == ProbeStatus::surplus);
    }
    QString hint;
    if (haveDeploys_) {
        const auto records = deployStore_.records();
        if (!records.empty()) {
            hint = tr("Puts back %1").arg(records.front().targetPath);
        }
    }
    installation_->setActions(complete, haveDeploys_, hint);
}

void MainWindow::setCurrentTab(DocumentTab *tab) {
    if (const auto index = tabs_->indexOf(tab); index >= 0) {
        tabs_->setCurrentIndex(index);
    }
}

bool MainWindow::mergeInto(DocumentTab *tab) {
    if (tab == nullptr || !tab->document().isWritable()) {
        return false;
    }
    const auto files = prompts.mergeFiles(tab);
    if (!files) {
        return false;
    }
    const auto read = [this](const QString &path) -> std::optional<QByteArray> {
        QFile file(path);
        if (file.size() > 64 * 1024 * 1024 || !file.open(QIODevice::ReadOnly)) {
            prompts.error(tr("Merge Changes"), tr("%1 cannot be read.").arg(path));
            return std::nullopt;
        }
        return file.readAll();
    };
    const auto base = read(files->first);
    const auto theirs = base ? read(files->second) : std::nullopt;
    if (!base || !theirs) {
        return false;
    }
    const auto span = [](const QByteArray &bytes) {
        return std::span<const std::byte>(reinterpret_cast<const std::byte *>(bytes.constData()),
                                          static_cast<std::size_t>(bytes.size()));
    };
    // The tab's current bytes, saved or not, are "mine".
    const auto mineBytes = tab->document().currentBytes();
    const std::vector<std::byte> mine(mineBytes.begin(), mineBytes.end());
    const auto merged = mergeWorlds(span(*base), mine, span(*theirs), MergePreference::none);
    if (!merged) {
        prompts.error(tr("Merge Changes"), QString::fromStdString(merged.error()));
        return false;
    }
    if (merged->fromTheirs == 0 && merged->conflicts.empty()) {
        statusBar()->showMessage(
            tr("The other copy changed nothing that this world does not already have."), 8000);
        return false;
    }
    const auto choices = prompts.resolveMerge(tab, *merged, mine, span(*theirs));
    if (!choices) {
        return false;
    }
    auto wanted = resolveMerge(*merged, mine, span(*theirs), *choices);
    // Ranges the editor preserves are never taken from another file here:
    // they keep this world's bytes, and the user is told.
    std::size_t preserved = 0;
    for (const auto &range : tab->document().protectedRanges()) {
        for (std::size_t offset = range.offset; offset < range.end() && offset < wanted.size();
             ++offset) {
            if (wanted[offset] != mine[offset]) {
                wanted[offset] = mine[offset];
                ++preserved;
            }
        }
    }
    if (!tab->replaceBytes(wanted, tr("Merge Changes from %1")
                                       .arg(QFileInfo(files->second).fileName()))) {
        statusBar()->showMessage(
            preserved != 0 ? tr("Nothing was merged: the other copy differs only in bytes the "
                                "editor preserves (%1 byte(s)).")
                                 .arg(preserved)
                           : tr("Nothing would change."),
            8000);
        return false;
    }
    statusBar()->showMessage(
        preserved != 0
            ? tr("Merged as one undo step; %1 byte(s) in preserved ranges kept this world's "
                 "values")
                  .arg(preserved)
            : tr("Merged as one undo step"),
        8000);
    return true;
}

QString MainWindow::stampDirectory() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("stamps"));
}

std::vector<Stamp> MainWindow::stamps() const {
    std::vector<Stamp> result;
    const QDir directory(stampDirectory());
    for (const auto &name :
         directory.entryList({QStringLiteral("*.stamp")}, QDir::Files, QDir::Name)) {
        QFile file(directory.filePath(name));
        if (file.size() > 4 * 1024 * 1024 || !file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const auto text = file.readAll();
        if (auto stamp = readStamp(
                std::string_view(text.constData(), static_cast<std::size_t>(text.size())))) {
            result.push_back(std::move(*stamp));
        }
    }
    return result;
}

bool MainWindow::saveSelectionAsStamp(DocumentTab *tab) {
    if (tab == nullptr) {
        return false;
    }
    if (tab->selection().empty()) {
        prompts.error(tr("Save Selection as Stamp"),
                      tr("Select the cells of the stamp first (the Select tool)."));
        return false;
    }
    const auto choice = prompts.stampName(tab);
    if (!choice || choice->first.trimmed().isEmpty()) {
        return false;
    }
    const auto stamp = tab->stampFromSelection(choice->first.trimmed(), choice->second);
    if (!stamp) {
        return false;
    }
    // A file name from the name's letters and digits.
    QString base;
    for (const auto character : choice->first.trimmed()) {
        base += character.isLetterOrNumber() && character.unicode() < 128 ? character
                                                                          : QLatin1Char('-');
    }
    const QDir directory(stampDirectory());
    const auto path = directory.filePath(base.left(60) + QStringLiteral(".stamp"));
    if (QFileInfo::exists(path)) {
        prompts.error(tr("Save Selection as Stamp"),
                      tr("A stamp is already stored as %1. Choose another name.").arg(path));
        return false;
    }
    const auto text = writeStamp(*stamp);
    QSaveFile file(path);
    if (!QDir().mkpath(directory.path()) || !file.open(QIODevice::WriteOnly) ||
        file.write(text.data(), static_cast<qint64>(text.size())) !=
            static_cast<qint64>(text.size()) ||
        !file.commit()) {
        prompts.error(tr("Save Selection as Stamp"), tr("%1 could not be written.").arg(path));
        return false;
    }
    statusBar()->showMessage(tr("Stamp \"%1\" saved: %2 x %3 cells, %4 object(s)")
                                 .arg(choice->first.trimmed())
                                 .arg(stamp->width)
                                 .arg(stamp->height)
                                 .arg(stamp->objects.size()),
                             8000);
    return true;
}

bool MainWindow::placeStamp(DocumentTab *tab) {
    if (tab == nullptr || !tab->document().isWritable()) {
        return false;
    }
    const auto list = stamps();
    if (list.empty()) {
        prompts.error(tr("Place Stamp"),
                      tr("No stamp is stored yet. Select cells and use Edit > Save Selection as "
                         "Stamp."));
        return false;
    }
    const auto index = prompts.chooseStamp(tab, list);
    if (!index || *index >= list.size()) {
        return false;
    }
    return tab->placeStampAt(list[*index], tab->canvas()->cursorCell());
}

int MainWindow::replaceInAllOpenWorlds() {
    auto *tab = currentTab();
    if (tab == nullptr) {
        return 0;
    }
    const auto values = prompts.replaceValues(tab);
    if (!values) {
        return 0;
    }
    const auto from = static_cast<std::int16_t>(values->first);
    const auto to = static_cast<std::int16_t>(values->second);
    const auto layer = tab->activeLayer();
    std::vector<std::pair<DocumentTab *, std::size_t>> targets;
    QStringList lines;
    for (int index = 0; index < tabCount(); ++index) {
        auto *other = tabAt(index);
        const auto name = QFileInfo(other->file().path()).fileName();
        if (other->profile() != tab->profile()) {
            lines << tr("%1: skipped (another game)").arg(name);
            continue;
        }
        if (!other->document().isWritable() || other->isLayerLocked(layer)) {
            lines << tr("%1: skipped (read-only or layer locked)").arg(name);
            continue;
        }
        const auto count = other->countValue(layer, from);
        lines << tr("%1: %n cell(s)", nullptr, static_cast<int>(count)).arg(name);
        if (count != 0 && from != to) {
            targets.emplace_back(other, count);
        }
    }
    if (targets.empty()) {
        statusBar()->showMessage(tr("No open world holds %1 in that layer.").arg(from), 6000);
        return 0;
    }
    lines.prepend(tr("Replace %1 with %2 in the whole %3 layer of every open world of this game, "
                     "whatever is selected (one undo step in each):")
                      .arg(from)
                      .arg(to)
                      .arg(layer == SectionKind::bigDecor ? tr("bigDecor") : tr("foreground")));
    if (!prompts.confirmBatch(tr("Replace in All Open Worlds"), lines)) {
        return 0;
    }
    int changed = 0;
    for (const auto &[target, count] : targets) {
        if (target->replaceEverywhere(layer, from, to,
                                      tr("Replace %1 with %2 (all open worlds)").arg(from).arg(to))) {
            ++changed;
        }
    }
    updateUi();
    statusBar()->showMessage(tr("%n world(s) changed", nullptr, changed), 8000);
    return changed;
}

std::vector<PackCandidate> MainWindow::packCandidates() const {
    std::vector<PackCandidate> candidates;
    const auto records = deployStore_.records();
    for (const auto &entry : installation_->entries()) {
        const auto slot = worldSlotOf(entry.name);
        if (!entry.complete || !slot) {
            continue;
        }
        PackCandidate candidate{*slot, entry.path, entry.title, false};
        if (!slot->user) {
            // Official numbers hold the game's own files unless the editor
            // put this very content there.
            candidate.gameOwn = true;
            QFile file(entry.path);
            if (file.open(QIODevice::ReadOnly)) {
                const auto hash =
                    QCryptographicHash::hash(file.readAll(), QCryptographicHash::Sha256);
                for (const auto &record : records) {
                    if (samePathKey(record.targetPath) == samePathKey(entry.path) &&
                        record.deployedSha256 == hash) {
                        candidate.gameOwn = false;
                    }
                }
            }
        }
        candidates.push_back(std::move(candidate));
    }
    return candidates;
}

bool MainWindow::exportCampaignPack() {
    const auto candidates = packCandidates();
    if (candidates.empty()) {
        prompts.error(tr("Export Campaign Pack"),
                      tr("The installation of the current world's game has no complete world "
                         "to pack. Choose the game directory first."));
        return false;
    }
    const auto choice = prompts.exportPack(candidates);
    if (!choice || choice->chosen.empty() || choice->path.isEmpty()) {
        return false;
    }
    std::vector<WorldSlot> places;
    std::vector<QByteArray> worlds;
    for (const auto index : choice->chosen) {
        if (index >= candidates.size()) {
            return false;
        }
        QFile file(candidates[index].path);
        if (!file.open(QIODevice::ReadOnly)) {
            prompts.error(tr("Export Campaign Pack"),
                          tr("%1 cannot be read.").arg(candidates[index].path));
            return false;
        }
        places.push_back(candidates[index].slot);
        worlds.push_back(file.readAll());
    }
    const auto pack = makePack(choice->name.isEmpty() ? tr("Campaign") : choice->name,
                               installation_->profile(), std::move(places), std::move(worlds));
    if (!pack) {
        prompts.error(tr("Export Campaign Pack"), pack.error());
        return false;
    }
    if (const auto written = writePack(*pack, choice->path, choice->zip); !written) {
        prompts.error(tr("Export Campaign Pack"), written.error());
        return false;
    }
    statusBar()->showMessage(tr("%n world(s) written to %1", nullptr,
                                static_cast<int>(pack->worlds.size()))
                                 .arg(choice->path),
                             8000);
    return true;
}

int MainWindow::importCampaignPack(const QString &path) {
    const auto dataDirectory = installation_->dataDirectory();
    if (dataDirectory.isEmpty()) {
        prompts.error(tr("Import Campaign Pack"),
                      tr("Open a world of the game the pack is for and choose that game's "
                         "directory first."));
        return 0;
    }
    // Choosing the manifest of a folder pack means the folder.
    const QFileInfo chosen(path);
    const auto packPath = chosen.isFile() && chosen.fileName() == QLatin1String(packManifestName)
                              ? chosen.absolutePath()
                              : path;
    const auto pack = readPack(packPath);
    if (!pack) {
        prompts.error(tr("Import Campaign Pack"), tr("%1\n%2").arg(packPath, pack.error()));
        return 0;
    }
    if (pack->campaign.game != installation_->profile()) {
        prompts.error(tr("Import Campaign Pack"),
                      tr("The pack is for the other game. Convert its worlds (File > Convert "
                         "for Other Game) or switch to that game's installation."));
        return 0;
    }
    const auto existing = QDir(dataDirectory).entryList(QDir::Files);
    QStringList replaced;
    for (const auto &world : pack->worlds) {
        const auto key = samePathKey(QDir(dataDirectory).filePath(world.name)).toLower();
        for (int index = 0; index < tabCount(); ++index) {
            if (samePathKey(tabAt(index)->file().path()).toLower() == key) {
                prompts.error(tr("Import Campaign Pack"),
                              tr("%1 is open in the editor. Close that tab first.")
                                  .arg(world.name));
                return 0;
            }
        }
        for (const auto &entry : existing) {
            if (entry.compare(world.name, Qt::CaseInsensitive) == 0) {
                replaced << entry;
            }
        }
    }
    if (!prompts.confirmImportPack(*pack, replaced)) {
        return 0;
    }
    std::vector<DeployRecord> done;
    for (const auto &world : pack->worlds) {
        const auto record = deployStore_.deploy(
            dataDirectory, world.name,
            std::span<const std::byte>(reinterpret_cast<const std::byte *>(world.bytes.constData()),
                                       static_cast<std::size_t>(world.bytes.size())));
        if (!record) {
            // All or nothing: take out what was already placed.
            QStringList stuck;
            for (auto placed = done.rbegin(); placed != done.rend(); ++placed) {
                if (!deployStore_.rollBack(*placed)) {
                    stuck << placed->targetPath;
                }
            }
            haveDeploys_ = !deployStore_.records().empty();
            installation_->refresh();
            updateUi();
            prompts.error(tr("Import Campaign Pack"),
                          stuck.isEmpty()
                              ? tr("%1\nNothing was imported: the worlds already placed were "
                                   "taken out again.")
                                    .arg(record.error())
                              : tr("%1\nThese files could not be put back and stay as "
                                   "imported:\n%2")
                                    .arg(record.error(), stuck.join(QLatin1Char('\n'))));
            return 0;
        }
        done.push_back(*record);
    }
    haveDeploys_ = true;
    installation_->refresh();
    updateUi();
    statusBar()->showMessage(
        tr("%n world(s) imported; Roll Back Last Deploy takes them out one at a time", nullptr,
           static_cast<int>(done.size())),
        10000);
    return static_cast<int>(done.size());
}

QString MainWindow::campaignOverview() const {
    Campaign campaign;
    campaign.game = installation_->profile();
    for (const auto &entry : installation_->entries()) {
        if (const auto slot = worldSlotOf(entry.name); slot && entry.complete) {
            campaign.entries.push_back(
                {*slot, worldSlotFileName(*slot), 0, {}, entry.title.toStdString()});
        }
    }
    if (campaign.entries.empty()) {
        return tr("No installation with complete worlds is selected.");
    }
    QStringList lines;
    lines << installation_->dataDirectory() << QString();
    for (const auto &entry : campaignOrder(campaign)) {
        QString role;
        if (entry.slot.user) {
            role = tr("player %1, world %2").arg(entry.slot.gamer).arg(entry.slot.number);
        } else {
            const auto name = worldRoleName(officialWorldRole(campaign.game, entry.slot.number));
            role = QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
        }
        const bool indent = !entry.slot.user &&
                            officialWorldRole(campaign.game, entry.slot.number) ==
                                WorldRole::mission;
        lines << QStringLiteral("%1%2  %3%4")
                     .arg(indent ? QStringLiteral("    ") : QString(),
                          QString::fromStdString(entry.fileName), role,
                          entry.title.empty()
                              ? QString()
                              : QStringLiteral("  \"%1\"").arg(QString::fromStdString(entry.title)));
    }
    const auto findings = reviewCampaign(campaign);
    lines << QString()
          << (findings.empty() ? tr("By their numbers the official worlds form a complete chain.")
                               : tr("What the numbering shows:"));
    for (const auto &finding : findings) {
        lines << QStringLiteral("[%1] %2")
                     .arg(finding.severity == CampaignSeverity::warning ? tr("warning")
                                                                         : tr("note"),
                          QString::fromStdString(finding.text));
    }
    if (!findings.empty()) {
        lines << QString()
              << tr("Basis: %1.")
                     .arg(QString::fromUtf8(findings.front().basis.data(),
                                            static_cast<qsizetype>(findings.front().basis.size())));
    }
    // Doors: which worlds the door tiles of the official worlds lead to.
    std::map<int, QByteArray> contents;
    for (const auto &entry : installation_->entries()) {
        QFile file(entry.path);
        if (entry.complete && entry.name.kind == DataFileKind::officialWorld &&
            file.open(QIODevice::ReadOnly)) {
            contents[entry.name.number] = file.readAll();
        }
    }
    std::map<int, WorldView> views;
    for (const auto &[number, bytes] : contents) {
        const std::span<const std::byte> span(
            reinterpret_cast<const std::byte *>(bytes.constData()),
            static_cast<std::size_t>(bytes.size()));
        views.emplace(number, WorldView(span, probeWorld(span)));
    }
    const auto doors = reviewDoors(campaign.game, views);
    lines << QString() << tr("Doors between the official worlds:");
    for (const auto &missing : doors.missingTargets) {
        lines << tr("[warning] World %1 has a door (tile %2 at cell %3, %4) to world %5, which "
                    "is not in the installation.")
                     .arg(missing.world)
                     .arg(missing.tile)
                     .arg(missing.cell.x)
                     .arg(missing.cell.y)
                     .arg(missing.target);
    }
    for (const auto number : doors.unreferenced) {
        lines << tr("[note] No door of these worlds leads to world %1.").arg(number);
    }
    if (doors.missingTargets.empty() && doors.unreferenced.empty()) {
        lines << tr("Every door leads to a world that is present, and every hub and mission "
                    "has a door leading to it.");
    }
    lines << tr("Basis: the games' door rule (from world 1 a door leads to its number times "
                "ten; elsewhere to the group's hub plus its number, and tile 184 back to "
                "world 1).");
    return lines.join(QLatin1Char('\n'));
}

WorldSlot MainWindow::suggestedSlot(DocumentTab *tab, const QString &dataDirectory) const {
    if (tab != nullptr) {
        const auto name = classifyDataFileName(
            QFileInfo(tab->file().path()).fileName().toStdString());
        if (const auto own = worldSlotOf(name)) {
            return *own;
        }
    }
    const auto existing = QDir(dataDirectory).entryList(QDir::Files);
    for (int number = 1; number <= userWorldCount; ++number) {
        const auto name = QString::fromStdString(worldSlotFileName({true, 1, number}));
        const bool taken = std::ranges::any_of(existing, [&](const QString &entry) {
            return entry.compare(name, Qt::CaseInsensitive) == 0;
        });
        if (!taken) {
            return {true, 1, number};
        }
    }
    return {true, 1, 1};
}

std::optional<DeployRecord> MainWindow::deployWorld(DocumentTab *tab) {
    if (tab == nullptr) {
        return std::nullopt;
    }
    const auto dataDirectory = installation_->dataDirectory();
    if (dataDirectory.isEmpty()) {
        prompts.error(tr("Deploy Current World"),
                      tr("Choose the directory of this world's game first (View > Game Artwork "
                         "Directory); it must contain a DATA folder."));
        return std::nullopt;
    }
    const auto &probe = tab->document().probe();
    if (probe.descriptor == nullptr ||
        (probe.status != ProbeStatus::sizeMatches && probe.status != ProbeStatus::surplus)) {
        prompts.error(tr("Deploy Current World"),
                      tr("Only a complete world can be deployed."));
        return std::nullopt;
    }
    const auto slot = prompts.deploySlot(tab, suggestedSlot(tab, dataDirectory), dataDirectory);
    if (!slot) {
        return std::nullopt;
    }
    const auto fileName = QString::fromStdString(worldSlotFileName(*slot));
    if (fileName.isEmpty()) {
        prompts.error(tr("Deploy Current World"),
                      tr("That is not a slot the games use (players 1 to 8, worlds 1 to 20)."));
        return std::nullopt;
    }
    // A file that is open in the editor would be changed behind its tab.
    const auto key = samePathKey(QDir(dataDirectory).filePath(fileName)).toLower();
    for (int index = 0; index < tabCount(); ++index) {
        if (samePathKey(tabAt(index)->file().path()).toLower() == key) {
            prompts.error(tr("Deploy Current World"),
                          tr("%1 is open in the editor. Close that tab or save into it instead.")
                              .arg(fileName));
            return std::nullopt;
        }
    }
    const auto result = deployStore_.deploy(dataDirectory, fileName, tab->document().currentBytes());
    if (!result) {
        prompts.error(tr("Deploy Current World"), result.error());
        return std::nullopt;
    }
    haveDeploys_ = true;
    installation_->refresh();
    updateUi();
    const auto report = runDoctor(tab->document().view(), tab->profile());
    statusBar()->showMessage(tr("Deployed to %1 - World Doctor: %2 error(s), %3 warning(s)")
                                 .arg(result->targetPath)
                                 .arg(report.count(Severity::error))
                                 .arg(report.count(Severity::warning)),
                             10000);
    return *result;
}

bool MainWindow::rollBackLastDeploy() {
    const auto records = deployStore_.records();
    if (records.empty()) {
        haveDeploys_ = false;
        updateUi();
        return false;
    }
    const auto &record = records.front();
    if (!prompts.confirmRollBack(record)) {
        return false;
    }
    const auto result = deployStore_.rollBack(record);
    if (!result) {
        prompts.error(tr("Roll Back Last Deploy"), result.error());
        return false;
    }
    haveDeploys_ = records.size() > 1;
    installation_->refresh();
    updateUi();
    statusBar()->showMessage(tr("%1 is as it was before the deploy").arg(record.targetPath),
                             8000);
    return true;
}

std::optional<MainWindow::PlaytestCommand>
MainWindow::playtestCommandFor(GameProfile profile) const {
    const auto game = gameDirectoryFor(profile);
    if (game == nullptr || profile == GameProfile::unknown) {
        return std::nullopt;
    }
    const auto text = QSettings()
                          .value(QStringLiteral("playtest/command%1")
                                     .arg(profile == GameProfile::speedyBlupi1 ? 1 : 2))
                          .toString();
    auto parts = QProcess::splitCommand(text);
    if (parts.isEmpty()) {
        return std::nullopt;
    }
    PlaytestCommand command;
    command.program = parts.takeFirst();
    command.arguments = parts;
    command.workingDirectory = game->directory();
    return command;
}

bool MainWindow::playtest() {
    auto *tab = currentTab();
    const auto profile = tab != nullptr ? tab->profile() : GameProfile::unknown;
    if (gameDirectoryFor(profile) == nullptr) {
        prompts.error(tr("Playtest"),
                      tr("Open a world, choose its game and that game's directory first."));
        return false;
    }
    const auto key =
        QStringLiteral("playtest/command%1").arg(profile == GameProfile::speedyBlupi1 ? 1 : 2);
    if (!playtestCommandFor(profile)) {
        const auto text = prompts.playtestCommand(profile, QSettings().value(key).toString());
        if (!text || text->isEmpty()) {
            return false;
        }
        QSettings().setValue(key, *text);
    }
    const auto command = playtestCommandFor(profile);
    if (!command) {
        return false;
    }
    if (!prompts.startProcess(command->program, command->arguments, command->workingDirectory)) {
        prompts.error(tr("Playtest"),
                      tr("%1 could not be started. Change the command in Edit > Playtest "
                         "Command.")
                          .arg(command->program));
        return false;
    }
    statusBar()->showMessage(tr("Started %1").arg(command->program), 6000);
    return true;
}

DocumentTab *MainWindow::convertGameOf(DocumentTab *tab) {
    if (tab == nullptr || tab->profile() == GameProfile::unknown) {
        return nullptr;
    }
    const auto from = tab->profile();
    const auto to = from == GameProfile::speedyBlupi1 ? GameProfile::speedyBlupi2
                                                      : GameProfile::speedyBlupi1;
    const auto current = tab->document().currentBytes();
    // Everything there is to say, before anything is decided.
    const auto preview = convertGame(current, from, to, UnknownPolicy::keep);
    if (!preview) {
        prompts.error(tr("Convert for Other Game"),
                      QString::fromStdString(preview.error().detail));
        return nullptr;
    }
    const auto policy = prompts.convertGame(tab, to, preview->notes);
    if (!policy || *policy == UnknownPolicy::refuse) {
        return nullptr;
    }
    const auto converted = convertGame(current, from, to, *policy);
    if (!converted) {
        prompts.error(tr("Convert for Other Game"),
                      QString::fromStdString(converted.error().detail));
        return nullptr;
    }
    const QFileInfo source(tab->file().path());
    const auto suggested = source.dir().filePath(
        QStringLiteral("%1-game%2.%3")
            .arg(source.completeBaseName())
            .arg(to == GameProfile::speedyBlupi1 ? 1 : 2)
            .arg(source.suffix().isEmpty() ? QStringLiteral("blp") : source.suffix()));
    const auto path = prompts.saveAsPath(suggested);
    if (path.isEmpty()) {
        return nullptr;
    }
    const auto key = samePathKey(path);
    for (int index = 0; index < tabCount(); ++index) {
        if (samePathKey(tabAt(index)->file().path()) == key) {
            prompts.error(tr("Convert for Other Game"),
                          tr("%1 is open in the editor. Choose another file: the conversion "
                             "never replaces its source.")
                              .arg(path));
            return nullptr;
        }
    }
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    const auto size = static_cast<qint64>(converted->bytes.size());
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(reinterpret_cast<const char *>(converted->bytes.data()), size) != size ||
        !file.commit()) {
        prompts.error(tr("Convert for Other Game"), tr("%1 was not written.").arg(path));
        return nullptr;
    }
    rememberProfile(path, to);
    auto *opened = openPath(path);
    if (opened != nullptr) {
        opened->setProfile(to);
    }
    statusBar()->showMessage(
        converted->removedCount != 0
            ? tr("Converted copy written to %1; %n place(s) cleared", nullptr,
                 static_cast<int>(converted->removedCount))
                  .arg(path)
            : tr("Converted copy written to %1").arg(path),
        8000);
    return opened;
}

DocumentTab *MainWindow::convertRevisionOf(DocumentTab *tab) {
    if (tab == nullptr) {
        return nullptr;
    }
    const auto choice = prompts.convertRevision(tab);
    if (!choice) {
        return nullptr;
    }
    // The current state is converted, saved or not; the result is a new file.
    const auto converted =
        convertRevision(tab->document().currentBytes(), choice->first, choice->second);
    if (!converted) {
        prompts.error(tr("Convert Revision"), QString::fromStdString(converted.error().detail));
        return nullptr;
    }
    const auto *descriptor = findRevision(choice->first);
    const QFileInfo source(tab->file().path());
    const auto suggested = source.dir().filePath(
        QStringLiteral("%1-rev%2.%3.%4")
            .arg(source.completeBaseName())
            .arg(descriptor->major)
            .arg(descriptor->minor)
            .arg(source.suffix().isEmpty() ? QStringLiteral("blp") : source.suffix()));
    const auto path = prompts.saveAsPath(suggested);
    if (path.isEmpty()) {
        return nullptr;
    }
    for (int index = 0; index < tabCount(); ++index) {
        if (samePathKey(tabAt(index)->file().path()) == samePathKey(path)) {
            prompts.error(tr("Convert Revision"),
                          tr("%1 is open in the editor. Choose another file: the conversion "
                             "never replaces its source.")
                              .arg(path));
            return nullptr;
        }
    }
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    const auto size = static_cast<qint64>(converted->bytes.size());
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(reinterpret_cast<const char *>(converted->bytes.data()), size) != size ||
        !file.commit()) {
        prompts.error(tr("Convert Revision"), tr("%1 was not written.").arg(path));
        return nullptr;
    }
    const auto profile = tab->profile();
    if (profile != GameProfile::unknown) {
        rememberProfile(path, profile);
    }
    auto *opened = openPath(path);
    if (opened != nullptr && profile != GameProfile::unknown) {
        opened->setProfile(profile);
    }
    statusBar()->showMessage(tr("Converted copy written to %1").arg(path), 8000);
    return opened;
}

bool MainWindow::takeSnapshot(DocumentTab *tab) {
    if (tab == nullptr) {
        return false;
    }
    const auto name = prompts.snapshotName(tab);
    if (!name) {
        return false;
    }
    const auto &document = tab->document();
    if (!recovery_.writeSnapshot(tab->file().path(), *name, document.savedBytes(),
                                 document.currentBytes())) {
        prompts.error(tr("Take Snapshot"), tr("The snapshot could not be stored in %1.")
                                               .arg(recovery_.directory()));
        return false;
    }
    statusBar()->showMessage(tr("Snapshot taken."), 5000);
    return true;
}

bool MainWindow::manageSnapshots(DocumentTab *tab) {
    if (tab == nullptr) {
        return false;
    }
    const auto entries = recovery_.snapshots(tab->file().path());
    if (entries.empty()) {
        statusBar()->showMessage(tr("This world has no snapshots."), 5000);
        return false;
    }
    const auto choice = prompts.chooseSnapshot(tab, entries);
    if (!choice) {
        return false;
    }
    if (choice->second == SnapshotAction::remove) {
        return recovery_.remove(choice->first);
    }
    const auto payload = recovery_.read(choice->first);
    if (!payload) {
        prompts.error(tr("Snapshots"), tr("The snapshot %1.").arg(payload.error()));
        return false;
    }
    const auto wanted = std::span<const std::byte>(
        reinterpret_cast<const std::byte *>(payload->constData()),
        static_cast<std::size_t>(payload->size()));
    if (wanted.size() != tab->document().currentBytes().size()) {
        prompts.error(tr("Snapshots"), tr("The snapshot has another size than this world."));
        return false;
    }
    if (!tab->replaceBytes(wanted, tr("Restore Snapshot"))) {
        statusBar()->showMessage(tr("The world already equals this snapshot, or it cannot be "
                                    "edited."),
                                 6000);
        return false;
    }
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
    // Whatever was chosen (saved or discarded), nothing is left to recover.
    recovery_.removeAutosave(tab->file().path());
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
    const auto shared = artworkEnabled_ ? artworkFor(profile) : nullptr;
    const auto *artwork = shared.get();
    presetArtwork_ = artwork;
    const int iconSize = 24;
    adaptiveCombo_->clear();
    adaptiveCombo_->addItem(tr("Tile ID, refresh neighbours"),
                            QString::fromLatin1(DocumentTab::adaptiveTileBrush));
    adaptiveCombo_->addItem(tr("Erase, refresh neighbours"),
                            QString::fromLatin1(DocumentTab::adaptiveEraseBrush));
    for (const auto &family : adaptiveFamilies(profile)) {
        adaptiveCombo_->addItem(
            QString::fromUtf8(family.name.data(), static_cast<qsizetype>(family.name.size())),
            QString::fromUtf8(family.id.data(), static_cast<qsizetype>(family.id.size())));
        adaptiveCombo_->setItemIcon(
            adaptiveCombo_->count() - 1,
            QIcon(tileThumbnail(SectionKind::foreground, family.seed, artwork, iconSize)));
        adaptiveCombo_->setItemData(
            adaptiveCombo_->count() - 1,
            tr("Writes %1, then recomputes border forms. Evidence: %2")
                .arg(family.seed)
                .arg(QString::fromUtf8(family.provenance.data(),
                                       static_cast<qsizetype>(family.provenance.size()))),
            Qt::ToolTipRole);
    }
    if (experimental_ && customRules_ != nullptr) {
        for (std::size_t index = 0; index < customRules_->size(); ++index) {
            const auto &rule = (*customRules_)[index];
            adaptiveCombo_->addItem(
                tr("Custom: %1").arg(QString::fromStdString(rule.name)),
                QString::fromLatin1(DocumentTab::customBrushPrefix) + QString::number(index));
            adaptiveCombo_->setItemIcon(
                adaptiveCombo_->count() - 1,
                QIcon(tileThumbnail(SectionKind::foreground, rule.forms[15], artwork, iconSize)));
            adaptiveCombo_->setItemData(
                adaptiveCombo_->count() - 1,
                tr("Your own rule, not a game's: writes one of its sixteen tile numbers "
                   "according to the joined neighbours."),
                Qt::ToolTipRole);
        }
    }
    presetCombo_->clear();
    createMenu_->clear();
    for (const auto &preset : objectPresets(profile)) {
        const auto id =
            QString::fromUtf8(preset.id.data(), static_cast<qsizetype>(preset.id.size()));
        const auto name =
            QString::fromUtf8(preset.name.data(), static_cast<qsizetype>(preset.name.size()));
        const QIcon icon(
            objectThumbnail(preset.type, preset.channel, preset.icon, artwork, iconSize));
        presetCombo_->addItem(icon, name, id);
        const auto provenance = QString::fromUtf8(
            preset.provenance.data(), static_cast<qsizetype>(preset.provenance.size()));
        presetCombo_->setItemData(presetCombo_->count() - 1,
                                  tr("Stored type %1. Evidence: %2").arg(preset.type).arg(provenance),
                                  Qt::ToolTipRole);
        auto *item = createMenu_->addAction(icon, name);
        item->setIconVisibleInMenu(true);
        item->setObjectName(QStringLiteral("objectCreate_") + id);
        connect(item, &QAction::triggered, this, [this, id] { choosePreset(id); });
    }
}

bool MainWindow::playMusic(int number) {
    auto *tab = currentTab();
    // The music belongs to the game directory, whether or not its pictures
    // are shown or usable.
    const auto assets = tab != nullptr ? gameDirectoryFor(tab->profile()) : nullptr;
    if (assets == nullptr) {
        music_->stop();
        musicNote_ = tr("Choose the game directory (View menu) to listen to its music.");
        updateMusicStatus();
        return false;
    }
    musicNote_.clear();
    const bool started = music_->play(assets->musicFile(number), number);
    updateMusicStatus();
    return started;
}

void MainWindow::updateMusicStatus() {
    auto *tab = currentTab();
    const bool haveGame = tab != nullptr && gameDirectoryFor(tab->profile()) != nullptr;
    auto text = music_->statusText();
    if (!music_->isPlaying() && !musicNote_.isEmpty()) {
        text = musicNote_;
    } else if (!music_->isPlaying() && !haveGame && music_->available()) {
        text = tr("Choose the game directory (View menu) to listen to its music.");
    }
    palette_->setMusicStatus(text, music_->playingNumber(), music_->available() && haveGame);
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
    refreshWelcome();
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
    enable("fileSnapshot", hasTab);
    enable("editBytes", writable && experimental_);
    if (auto *item = action(QStringLiteral("editBytes"))) {
        item->setVisible(experimental_);
    }
    if (auto *item = action(QStringLiteral("editBorderRules"))) {
        item->setVisible(experimental_);
        item->setEnabled(experimental_);
    }
    experimentalLabel_->setVisible(experimental_);
    refreshInstallationActions();
    enable("fileDeploy", installation_->deployButton()->isEnabled());
    enable("fileRollBack", haveDeploys_);
    {
        const bool installed = !installation_->dataDirectory().isEmpty();
        enable("fileCampaignOverview", installed);
        enable("fileExportPack", installed);
        enable("fileImportPack", installed);
    }
    enable("filePlaytest", hasTab && gameDirectoryFor(tab->profile()) != nullptr);
    enable("editPlaytestCommand", hasTab && tab->profile() != GameProfile::unknown);
    enable("fileConvertRevision", hasTab);
    enable("fileMerge", writable);
    enable("fileConvertGame", hasTab && tab->profile() != GameProfile::unknown);
    enable("fileSnapshots", hasTab);
    enable("fileProperties", hasTab);
    enable("fileExportPng", hasTab);
    if (auto *toggle = action(QStringLiteral("viewArtwork"))) {
        toggle->setChecked(artworkEnabled_);
        toggle->setEnabled(artwork_[0] != nullptr || artwork_[1] != nullptr);
    }
    if (hasTab) {
        auto options = tab->canvas()->renderOptions();
        const auto wanted = artworkEnabled_ ? artworkFor(tab->profile()) : nullptr;
        if (options.artwork != wanted) {
            options.artwork = wanted;
            tab->canvas()->setRenderOptions(options);
        }
        tab->canvas()->setProfile(tab->profile());
        tab->canvas()->setAnimated(animate_);
    }
    if (auto *item = action(QStringLiteral("viewAnimate"))) {
        item->setChecked(animate_);
    }
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
                             "viewObjects", "viewStarts", "viewPaths", "viewBackdrop",
                             "toolBrowse"}) {
        enable(name, hasTab);
    }
    for (const char *name : {"toolPencil", "toolEraser", "toolLine", "toolRectangle",
                             "toolFill"}) {
        enable(name, writable);
    }
    const auto profile = hasTab ? tab->profile() : GameProfile::unknown;
    const auto currentArtwork = artworkEnabled_ ? artworkFor(profile) : nullptr;
    if (profile != presetProfile_ || currentArtwork.get() != presetArtwork_ ||
        (presetCombo_->count() == 0 && profile != GameProfile::unknown)) {
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
    enable("layerReplaceAll", writable);
    enable("editSaveStamp", hasTab);
    enable("editPlaceStamp", writable);
    for (const auto *name : {"objectAlignLeft", "objectAlignRight", "objectAlignTop",
                             "objectAlignBottom", "objectDistributeH", "objectDistributeV"}) {
        enable(name, writable);
    }
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
            {"viewBackdrop", options.showBackdrop},
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
    palette_->setContext(profile, hasTab ? tab->activeLayer() : SectionKind::foreground,
                         currentArtwork);
    if (encyclopedia_ != nullptr && encyclopedia_->isVisible() &&
        (encyclopedia_->game() != profile || encyclopedia_->artwork() != currentArtwork)) {
        encyclopedia_->setGame(profile, currentArtwork);
    }
    palette_->setEditable(writable);
    {
        PalettePanel::WorldState world;
        if (hasTab) {
            if (const auto metadata = readMetadata(tab->document().view())) {
                world.available = true;
                world.scrollsHorizontally = metadata->scrollsHorizontally;
                world.scrollsVertically = metadata->scrollsVertically;
                world.region = metadata->region;
                world.music = metadata->music;
            }
        }
        palette_->setWorldState(world);
        updateMusicStatus();
    }
    if (hasTab) {
        palette_->setCurrentTile(tab->tileId());
        palette_->setCurrentBrush(tab->adaptiveBrush());
        if (tab->placePreset() != nullptr) {
            palette_->setCurrentPreset(QString::fromUtf8(
                tab->placePreset()->id.data(),
                static_cast<qsizetype>(tab->placePreset()->id.size())));
        }
    }
    minimap_->setTab(tab);
    statistics_->setTab(tab);
    objects_->setArtwork(currentArtwork);
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
        zoomSlider_->setEnabled(false);
        accessLabel_->clear();
        poolLabel_->clear();
        return;
    }
    const auto cell = tab->canvas()->hoveredCell().value_or(tab->canvas()->cursorCell());
    cellLabel_->setText(tab->describeCell(cell));
    zoomLabel_->setText(tr("%1%").arg(qRound(tab->canvas()->zoom() * 100)));
    {
        const QSignalBlocker blocker(zoomSlider_);
        const double ratio = ViewTransform::maximumScale / ViewTransform::minimumScale;
        zoomSlider_->setEnabled(true);
        zoomSlider_->setValue(qRound(
            1000.0 * std::log(tab->canvas()->zoom() / ViewTransform::minimumScale) /
            std::log(ratio)));
    }
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
