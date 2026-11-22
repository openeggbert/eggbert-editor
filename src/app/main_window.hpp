// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QMainWindow>

#include <functional>
#include <optional>
#include <utility>

#include "app/document_tab.hpp"
#include "app/png_export.hpp"
#include "world/world_stats.hpp"

class QAction;
class QComboBox;
class QLabel;
class QMenu;
class QSpinBox;
class QTabWidget;
class QDockWidget;

namespace eggbert {

struct FindQuery {
    enum class Kind { coordinate, tileValue, objectType, objectSlot };
    Kind kind = Kind::coordinate;
    // coordinate: x, y. tileValue/objectType/objectSlot: first only.
    int first = 0;
    int second = 0;
};

enum class UnsavedChoice { save, discard, cancel };
enum class ConflictChoice { reload, saveAs, cancel };

// Every modal question the window asks goes through these hooks, so tests
// can answer without opening dialogs. The defaults show native dialogs.
struct Prompts {
    std::function<QStringList()> openPaths;
    std::function<QString(const QString &suggestedPath)> saveAsPath;
    std::function<UnsavedChoice(DocumentTab *)> unsavedChanges;
    std::function<ConflictChoice(DocumentTab *, const SaveResult &)> saveConflict;
    std::function<bool(DocumentTab *)> confirmRevert;
    std::function<void(const QString &title, const QString &text)> error;
    // Shown before an edit that changes many cells.
    std::function<bool(DocumentTab *, const BroadEditPreview &)> confirmBroadEdit;
    // Value to replace and its replacement, for Layer > Replace Value.
    std::function<std::optional<std::pair<int, int>>(DocumentTab *)> replaceValues;
    std::function<std::optional<FindQuery>(DocumentTab *)> findQuery;
    // Edited header values; page 0 is the world page, 1 the starts page.
    std::function<std::optional<Metadata>(DocumentTab *, int page)> editMetadata;
    // Shown with the exact byte diff before a World Doctor fix is applied.
    std::function<bool(DocumentTab *, const QString &description)> confirmFix;
    // PNG export settings and destination; nullopt cancels.
    std::function<std::optional<std::pair<PngExportOptions, QString>>(DocumentTab *)> exportPng;
    // Displays a read-only text report (byte differences).
    std::function<void(const QString &title, const QString &text)> showReport;
};

class DiagnosticsPanel;
class InspectorPanel;
class MinimapWidget;
class ObjectsPanel;
class StatisticsPanel;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    MainWindow();

    Prompts prompts;

    [[nodiscard]] int tabCount() const;
    [[nodiscard]] DocumentTab *tabAt(int index) const;
    [[nodiscard]] DocumentTab *currentTab() const;

    // Opens the file in a new tab, or activates the tab that already has it.
    DocumentTab *openPath(const QString &path);
    bool saveTab(DocumentTab *tab);
    bool saveTabAs(DocumentTab *tab);
    bool saveAll();
    bool revertTab(DocumentTab *tab);
    // Asks about unsaved changes; false when the user cancels.
    bool closeTab(DocumentTab *tab);

    // Locate commands used by statistics and search: they move the view and
    // mark results, and never modify the document. Each returns the number
    // of results.
    std::size_t locateTileValue(SectionKind layer, std::int16_t value);
    std::size_t locateObjects(ObjectKey key, std::int16_t value);
    bool locateStart(std::size_t index);
    bool locateOffsetRange(std::size_t offset, std::size_t size);
    bool locateCell(CellPos cell);
    bool locateSlot(std::size_t slot);
    // Exports the tab's current world; never modifies the document.
    bool exportPng(DocumentTab *tab, const PngExportOptions &options, const QString &path);
    // Opens the metadata editor on a page and applies the result.
    bool editMetadata(int page);
    // Runs a search; false when nothing matches.
    bool find(const FindQuery &query);

    [[nodiscard]] MinimapWidget *minimap() const { return minimap_; }
    [[nodiscard]] StatisticsPanel *statistics() const { return statistics_; }
    [[nodiscard]] ObjectsPanel *objectsPanel() const { return objects_; }
    [[nodiscard]] InspectorPanel *inspector() const { return inspector_; }
    [[nodiscard]] DiagnosticsPanel *diagnostics() const { return diagnostics_; }
    // Synchronises canvas, objects and inspector with a finding.
    bool locateFinding(const Finding &finding);
    // Shows a fix's byte diff and applies it when confirmed.
    bool reviewFix(const Finding &finding);
    // Chooses a verified preset of the current tab's profile and switches to
    // the place tool. False when the profile has no such preset.
    bool choosePreset(const QString &id);
    void resetLayout();

    [[nodiscard]] QStringList recentFiles() const;
    // Probe report and decoded summary of a tab's current bytes.
    [[nodiscard]] static QString propertiesText(const DocumentTab &tab);
    // Looks up a menu or toolbar command by its stable object name.
    [[nodiscard]] QAction *action(const QString &objectName) const;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void createActions();
    void createDocks(QMenu *viewMenu);
    void installDefaultPrompts();
    void connectTab(DocumentTab *tab);
    void updateUi();
    void updateStatus();
    void addRecentFile(const QString &path);
    void rebuildRecentMenu();
    void rebuildPresets(GameProfile profile);
    // Shows an offset in the inspector without echoing back to the canvas.
    void showInInspector(std::size_t offset, std::size_t size);
    void showProperties();
    QAction *addAction(QMenu *menu, const QString &objectName, const QString &text,
                       const QKeySequence &shortcut = {});

    QTabWidget *tabs_ = nullptr;
    QMenu *recentMenu_ = nullptr;
    QComboBox *layerCombo_ = nullptr;
    QComboBox *profileCombo_ = nullptr;
    QSpinBox *tileSpin_ = nullptr;
    QSpinBox *startSpin_ = nullptr;
    QLabel *cellLabel_ = nullptr;
    QLabel *zoomLabel_ = nullptr;
    QLabel *accessLabel_ = nullptr;
    MinimapWidget *minimap_ = nullptr;
    StatisticsPanel *statistics_ = nullptr;
    ObjectsPanel *objects_ = nullptr;
    InspectorPanel *inspector_ = nullptr;
    DiagnosticsPanel *diagnostics_ = nullptr;
    QDockWidget *diagnosticsDock_ = nullptr;
    QDockWidget *inspectorDock_ = nullptr;
    bool syncing_ = false;
    QDockWidget *objectsDock_ = nullptr;
    QMenu *createMenu_ = nullptr;
    QComboBox *presetCombo_ = nullptr;
    QComboBox *adaptiveCombo_ = nullptr;
    QLabel *poolLabel_ = nullptr;
    GameProfile presetProfile_ = GameProfile::unknown;
    QDockWidget *minimapDock_ = nullptr;
    QDockWidget *statisticsDock_ = nullptr;
    QByteArray defaultState_;
    bool updating_ = false;
};

} // namespace eggbert
