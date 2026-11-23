// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QMainWindow>

#include <functional>
#include <optional>
#include <utility>
#include <vector>
#include <optional>
#include <utility>

#include "app/recovery_store.hpp"
#include "app/document_tab.hpp"
#include "app/png_export.hpp"
#include "app/campaign_pack.hpp"
#include "app/deploy_store.hpp"
#include "world/game_convert.hpp"
#include "world/installation.hpp"
#include "world/world_stats.hpp"

class QAction;
class QComboBox;
class QLabel;
class QMenu;
class QSpinBox;
class QTabWidget;
class QDockWidget;

class QListWidget;
class QSlider;
class QStackedWidget;
class QTimer;

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
// What to do with unsaved changes found from an earlier session.
enum class RecoveryChoice { restore, saveCopy, discard, keep };
enum class SnapshotAction { restore, remove };

// A world of the installation that can go into a campaign pack.
struct PackCandidate {
    WorldSlot slot;
    QString path;
    QString title;
    // True for a file in one of the game's own world numbers that the editor
    // did not put there: most likely the game's original, which must not be
    // passed on. Not selected unless the user asks.
    bool gameOwn = false;
};

struct PackExportChoice {
    QString name;
    // Indices into the candidates.
    std::vector<std::size_t> chosen;
    QString path;
    bool zip = true;
};

struct Prompts {
    // `canRestore` is false when the world file changed or vanished since
    // the copy was made; `detail` says so.
    std::function<RecoveryChoice(const RecoveryEntry &, bool canRestore, const QString &detail)>
        recover;
    // Experimental editing: the offset and the new bytes; nullopt cancels.
    std::function<std::optional<std::pair<qulonglong, QByteArray>>(DocumentTab *,
                                                                    qulonglong offset)>
        editBytes;
    // File with user-written border rules; empty cancels.
    std::function<QString()> borderRulesPath;
    // Second confirmation before bytes in a preserved range are changed.
    std::function<bool(DocumentTab *, const QString &description)> confirmProtectedEdit;
    std::function<std::optional<PackExportChoice>(const std::vector<PackCandidate> &)>
        exportPack;
    // A pack file or directory to import; empty cancels.
    std::function<QString()> importPackPath;
    // Shown with the pack's contents and what they would replace.
    std::function<bool(const CampaignPack &, const QStringList &replaced)> confirmImportPack;
    // Where in the installation the tab's world goes; nullopt cancels.
    // `dataDirectory` is the installation's data directory.
    std::function<std::optional<WorldSlot>(DocumentTab *, WorldSlot suggested,
                                           const QString &dataDirectory)>
        deploySlot;
    std::function<bool(const DeployRecord &)> confirmRollBack;
    // The command that starts a game; nullopt cancels.
    std::function<std::optional<QString>(GameProfile, const QString &current)> playtestCommand;
    // Starts a detached process; replaced in tests, which never run a game.
    std::function<bool(const QString &program, const QStringList &arguments,
                       const QString &workingDirectory)>
        startProcess;
    // Shown with everything the target game does not know or treats
    // differently; returns what to do with the unknown values (keep or
    // remove), or nullopt to cancel.
    std::function<std::optional<UnknownPolicy>(DocumentTab *, GameProfile target,
                                               const std::vector<GameConversionNote> &)>
        convertGame;
    // The game a new world is for; nullopt cancels.
    std::function<std::optional<GameProfile>(GameProfile suggested)> newWorldGame;
    // The revision to convert a copy to and whether loss is accepted.
    std::function<std::optional<std::pair<FormatRevision, bool>>(DocumentTab *)> convertRevision;
    std::function<std::optional<QString>(DocumentTab *)> snapshotName;
    std::function<std::optional<std::pair<RecoveryEntry, SnapshotAction>>(
        DocumentTab *, const std::vector<RecoveryEntry> &)>
        chooseSnapshot;
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
    // Displays a read-only document; `markdown` selects rich rendering.
    std::function<void(const QString &title, const QString &text, bool markdown)> showDocument;
    // Destination for a text file the user asked to save; empty cancels.
    std::function<QString(const QString &suggestedName)> saveTextPath;
    // Runs the preferences dialog; false when cancelled.
    std::function<bool()> editPreferences;
    // A game directory for artwork and the game it belongs to. The default
    // shows what was recognised and lets the user correct it. nullopt cancels.
    std::function<std::optional<std::pair<QString, GameProfile>>(GameProfile suggested)>
        chooseArtworkDirectory;
    // Displays a read-only text report (byte differences).
    std::function<void(const QString &title, const QString &text)> showReport;
};

class DiagnosticsPanel;
class InspectorPanel;
class MinimapWidget;
class ObjectsPanel;
class MusicPlayer;
class PalettePanel;
class EncyclopediaDialog;
class InstallationPanel;
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
    [[nodiscard]] PalettePanel *palette() const { return palette_; }
    // Opens (creating it on first use) the element encyclopedia for the
    // current tab's game.
    EncyclopediaDialog *showEncyclopedia();
    [[nodiscard]] EncyclopediaDialog *encyclopediaDialog() const { return encyclopedia_; }
    [[nodiscard]] MusicPlayer *musicPlayer() const { return music_; }
    // Plays the music file of a stored music number from the current tab's
    // game directory; false when there is no directory, file or player.
    bool playMusic(int number);
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

    // Help content and reports.
    [[nodiscard]] static QString resourceText(const QString &name);
    [[nodiscard]] QString shortcutsText() const;
    [[nodiscard]] static QString aboutText();
    // Versions, structural report, World Doctor findings and changed byte
    // ranges of a document; no world bytes beyond those descriptions.
    [[nodiscard]] static QString diagnosticsBundleText(const DocumentTab *tab);
    bool saveDiagnosticsBundle(const QString &path);

    // Game artwork: read-only directories per game, remembered as a local
    // preference. Loading never writes to the directory.
    bool loadArtwork(const QString &directory, GameProfile profile);
    [[nodiscard]] std::shared_ptr<const GameAssets> artworkFor(GameProfile profile) const;
    // The chosen game directory of a profile even when its pictures cannot
    // be used (its music can still be played); null when none was chosen.
    [[nodiscard]] std::shared_ptr<const GameAssets> gameDirectoryFor(GameProfile profile) const;
    [[nodiscard]] bool artworkEnabled() const { return artworkEnabled_; }
    void setArtworkEnabled(bool enabled);
    // Display-only animation on every canvas (View -> Animate).
    void setAnimated(bool animated);
    [[nodiscard]] bool animated() const { return animate_; }
    [[nodiscard]] QString artworkReport() const;

    enum class DockLayout { mapping, objects, validation };
    void applyLayout(DockLayout layout);
    void setTheme(const QString &key);
    // Applies stored preferences (theme, view defaults, inspector width).
    void applyPreferences();
    bool fitSelection();

    [[nodiscard]] QStringList recentFiles() const;
    // Probe report and decoded summary of a tab's current bytes.
    [[nodiscard]] static QString propertiesText(const DocumentTab &tab);
    // Looks up a menu or toolbar command by its stable object name.
    [[nodiscard]] QAction *action(const QString &objectName) const;

    // Crash recovery and snapshots. The store is a per-user directory; no
    // file is ever written beside a world.
    [[nodiscard]] RecoveryStore &recoveryStore() { return recovery_; }
    // Writes the unsaved bytes of every modified tab to the store and drops
    // the copies of unmodified ones (a timer does this a few seconds after
    // an edit).
    void flushAutosave();
    // Offers the unsaved changes of earlier sessions; returns how many were
    // restored into tabs. Called once after start.
    int checkRecovery();
    // Experimental editing (off by default, kept in the settings): raw byte
    // edits and music or region numbers outside the verified ranges.
    [[nodiscard]] bool experimental() const { return experimental_; }
    void setExperimental(bool enabled);
    // Replaces bytes of the tab's world as one undo step; bytes in a
    // preserved range need a second confirmation; the revision field and the
    // length never change. Needs experimental editing. True when applied.
    bool editBytes(DocumentTab *tab);
    // Reads user-written border rules (see custom_border.hpp) and offers
    // them in the Adaptive brush chooser while experimental editing is on.
    // False, with the reason shown, when the file cannot be used.
    bool loadCustomBorderRules(const QString &path);
    [[nodiscard]] std::size_t customBorderRuleCount() const {
        return customRules_ != nullptr ? customRules_->size() : 0;
    }
    // Asks for the game and a file, writes the world the games' editors
    // start with and opens it; returns the new tab or null.
    DocumentTab *createWorld();
    [[nodiscard]] InstallationPanel *installation() const { return installation_; }
    // The complete worlds of the current installation, with the game's own
    // files marked.
    [[nodiscard]] std::vector<PackCandidate> packCandidates() const;
    // Writes the chosen worlds as a pack; true when written.
    bool exportCampaignPack();
    // Reads and checks a pack and deploys its worlds to their slots in the
    // current installation; if one cannot be placed the ones already placed
    // are rolled back. Returns the number of worlds placed.
    int importCampaignPack(const QString &path);
    // Order and numbering review of the installation's worlds, as text.
    [[nodiscard]] QString campaignOverview() const;
    [[nodiscard]] DeployStore &deployStore() { return deployStore_; }
    // Puts the tab's current bytes into the installation of its game at a
    // slot the user chooses; the replaced file is kept for a rollback.
    std::optional<DeployRecord> deployWorld(DocumentTab *tab);
    // Undoes the newest deploy after a confirmation. True when undone.
    bool rollBackLastDeploy();
    // The slot offered first: the world's own when its file name is one,
    // else the first free user world of player 1.
    [[nodiscard]] WorldSlot suggestedSlot(DocumentTab *tab, const QString &dataDirectory) const;
    struct PlaytestCommand {
        QString program;
        QStringList arguments;
        QString workingDirectory;
    };
    // What Playtest would start for a game: the configured command, run in
    // the game directory. nullopt when nothing is configured or no game
    // directory is known.
    [[nodiscard]] std::optional<PlaytestCommand> playtestCommandFor(GameProfile profile) const;
    // Starts it (asking for the command when none is configured).
    bool playtest();
    // Writes a copy of the tab's world for the other game (never in place)
    // and opens it with that game selected; returns the new tab or null.
    DocumentTab *convertGameOf(DocumentTab *tab);
    // Writes a copy of the tab's world in another revision (never in place)
    // and opens it; returns the new tab or null.
    DocumentTab *convertRevisionOf(DocumentTab *tab);
    bool takeSnapshot(DocumentTab *tab);
    bool manageSnapshots(DocumentTab *tab);

    // The page shown while no world is open (open, recent worlds, game
    // directory).
    [[nodiscard]] QWidget *welcomePage() const { return welcome_; }
    [[nodiscard]] bool welcomeVisible() const;
    // The game remembered for a file from an earlier session, or unknown.
    [[nodiscard]] static GameProfile rememberedProfile(const QString &path);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

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
    void applyArtworkToTabs();
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
    PalettePanel *palette_ = nullptr;
    EncyclopediaDialog *encyclopedia_ = nullptr;
    QDockWidget *installationDock_ = nullptr;
    InstallationPanel *installation_ = nullptr;
    DeployStore deployStore_;
    bool haveDeploys_ = false;
    void refreshInstallationActions();
    bool experimental_ = false;
    std::shared_ptr<const std::vector<CustomBorderRule>> customRules_;
    void shareCustomRules();
    QLabel *experimentalLabel_ = nullptr;
    RecoveryStore recovery_;
    QTimer *autosaveTimer_ = nullptr;
    QStackedWidget *central_ = nullptr;
    QWidget *welcome_ = nullptr;
    QListWidget *welcomeRecent_ = nullptr;
    QSlider *zoomSlider_ = nullptr;
    std::vector<std::pair<Tool, QAction *>> toolActions_;
    void refreshToolIcons();
    void refreshWelcome();
    static void rememberProfile(const QString &path, GameProfile profile);
    // Profile and artwork from the folder a world lies in (a game's DATA
    // folder); only reads.
    void adoptGameOfFolder(DocumentTab *tab);
    MusicPlayer *music_ = nullptr;
    // Why music cannot be played right now, when that is not the player's fault.
    QString musicNote_;
    void updateMusicStatus();
    InspectorPanel *inspector_ = nullptr;
    DiagnosticsPanel *diagnostics_ = nullptr;
    QDockWidget *diagnosticsDock_ = nullptr;
    QDockWidget *inspectorDock_ = nullptr;
    bool syncing_ = false;
    std::shared_ptr<const GameAssets> artwork_[2];
    bool artworkEnabled_ = false;
    bool animate_ = true;
    QDockWidget *objectsDock_ = nullptr;
    QDockWidget *paletteDock_ = nullptr;
    QMenu *createMenu_ = nullptr;
    QComboBox *presetCombo_ = nullptr;
    QComboBox *adaptiveCombo_ = nullptr;
    QLabel *poolLabel_ = nullptr;
    GameProfile presetProfile_ = GameProfile::unknown;
    // Artwork the preset choosers' icons were drawn with.
    const GameAssets *presetArtwork_ = nullptr;
    QDockWidget *minimapDock_ = nullptr;
    QDockWidget *statisticsDock_ = nullptr;
    QByteArray defaultState_;
    bool updating_ = false;
};

} // namespace eggbert
