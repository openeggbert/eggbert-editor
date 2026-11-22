// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QMainWindow>

#include <functional>

#include "app/document_tab.hpp"
#include "world/world_stats.hpp"

class QAction;
class QComboBox;
class QLabel;
class QMenu;
class QSpinBox;
class QTabWidget;
class QDockWidget;

namespace eggbert {

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
};

class MinimapWidget;
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

    [[nodiscard]] MinimapWidget *minimap() const { return minimap_; }
    [[nodiscard]] StatisticsPanel *statistics() const { return statistics_; }
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
    void showProperties();
    QAction *addAction(QMenu *menu, const QString &objectName, const QString &text,
                       const QKeySequence &shortcut = {});

    QTabWidget *tabs_ = nullptr;
    QMenu *recentMenu_ = nullptr;
    QComboBox *layerCombo_ = nullptr;
    QComboBox *profileCombo_ = nullptr;
    QSpinBox *tileSpin_ = nullptr;
    QLabel *cellLabel_ = nullptr;
    QLabel *zoomLabel_ = nullptr;
    QLabel *accessLabel_ = nullptr;
    MinimapWidget *minimap_ = nullptr;
    StatisticsPanel *statistics_ = nullptr;
    QDockWidget *minimapDock_ = nullptr;
    QDockWidget *statisticsDock_ = nullptr;
    QByteArray defaultState_;
    bool updating_ = false;
};

} // namespace eggbert
