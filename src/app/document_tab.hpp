// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QWidget>

#include <functional>
#include <memory>
#include <optional>

#include "app/document_file.hpp"
#include "app/world_canvas.hpp"
#include "world/metadata.hpp"
#include "world/tile_tools.hpp"

class QLabel;

namespace eggbert {

enum class Tool { browse, pencil, eraser, eyedropper, line, rectangle, fill, select, start };

// What a broad edit would do, shown before it is applied.
struct BroadEditPreview {
    QString label;
    std::size_t cellsChanged = 0;
    std::size_t bytesChanged = 0;
    std::size_t byteRanges = 0;
    std::size_t firstOffset = 0;
    std::size_t lastOffset = 0;
};

// Tiles copied from one layer, shared by all tabs of the application.
struct TileClipboard {
    TilePatch patch;
    GameProfile profile = GameProfile::unknown;
    // The tab the patch came from; pasting into the same tab is always
    // allowed because no cross-profile interpretation is involved.
    const void *sourceTab = nullptr;

    [[nodiscard]] static TileClipboard &shared();
};

// One open world: its file, the user-chosen game profile (never stored in
// the file), tool state, selection and canvas. All edits go through
// applyTransaction().
class DocumentTab final : public QWidget {
    Q_OBJECT

public:
    // Edits changing more cells than this ask for confirmation first.
    static constexpr std::size_t broadEditCells = 64;

    explicit DocumentTab(std::unique_ptr<DocumentFile> file, QWidget *parent = nullptr);

    [[nodiscard]] DocumentFile &file() { return *file_; }
    [[nodiscard]] const DocumentFile &file() const { return *file_; }
    [[nodiscard]] WorldDocument &document() { return file_->document(); }
    [[nodiscard]] const WorldDocument &document() const { return file_->document(); }
    [[nodiscard]] WorldCanvas *canvas() const { return canvas_; }

    // File name with a trailing asterisk while there are unsaved changes.
    [[nodiscard]] QString title() const;

    [[nodiscard]] GameProfile profile() const { return profile_; }
    void setProfile(GameProfile profile);

    [[nodiscard]] Tool tool() const { return tool_; }
    void setTool(Tool tool);
    [[nodiscard]] SectionKind activeLayer() const { return activeLayer_; }
    // Fails for a layer this document does not have.
    bool setActiveLayer(SectionKind layer);
    [[nodiscard]] std::int16_t tileId() const { return tileId_; }
    void setTileId(std::int16_t id);

    // A locked layer refuses every tile edit until unlocked.
    [[nodiscard]] bool isLayerLocked(SectionKind layer) const;
    void setLayerLocked(SectionKind layer, bool locked);
    // Shows only the active tile layer while on; restores the previous
    // visibility when turned off.
    [[nodiscard]] bool isIsolated() const { return isolated_.has_value(); }
    void setIsolated(bool isolated);

    // Asked before an edit that changes more than broadEditCells cells.
    // Unset means "apply".
    std::function<bool(const BroadEditPreview &)> confirmBroadEdit;

    // The single mutation entry point for every tool, panel and command.
    // Returns false and reports why when the document refuses the change.
    bool applyTransaction(Transaction transaction);
    bool undo();
    bool redo();
    // Call after the file was saved or otherwise changed outside apply().
    void notifyDocumentChanged();
    // Replaces the document with a freshly loaded file (Revert).
    void replaceFile(std::unique_ptr<DocumentFile> file);

    // Tool operations, also reachable without the mouse. Each is one undo
    // step (pencil/eraser strokes merge by key) and edits only the active
    // layer, inside the selection when there is one.
    bool pencilAt(CellPos cell, std::uint64_t mergeKey);
    bool paintStroke(std::span<const CellPos> cells, std::int16_t value, const QString &label,
                     std::uint64_t mergeKey);
    bool pickAt(CellPos cell);
    bool drawLine(CellPos from, CellPos to);
    bool drawRectangle(CellPos first, CellPos second);
    bool fillAt(CellPos cell);

    // Header edits. Each is one undo step.
    bool applyMetadataEdit(const Metadata &metadata);
    // Start edited by the Start tool: 0..3.
    [[nodiscard]] std::size_t activeStart() const { return activeStart_; }
    void setActiveStart(std::size_t index);
    // As in the games' editors: a click on the cell that already holds the
    // start toggles its direction; any other cell moves it there.
    bool placeStartAt(CellPos cell);
    bool copyActiveStartToAll();

    // Selection.
    [[nodiscard]] const CellMask &selection() const { return selection_; }
    void setSelection(const CellMask &mask);
    void selectAll();
    void selectNone();
    void invertSelection();
    // Selects the active layer's cells storing the value.
    std::size_t selectValue(std::int16_t value);

    // Clipboard and selection commands.
    bool copySelection();
    bool cutSelection();
    // Pastes with the patch's top-left at the cursor cell.
    bool paste();
    [[nodiscard]] bool canPaste() const;
    bool deleteSelection();
    bool fillSelection();
    bool replaceInSelection(std::int16_t from, std::int16_t to);
    bool flipSelection(bool horizontally);

    // Human-readable description of what is stored at a cell.
    [[nodiscard]] QString describeCell(CellPos cell) const;

signals:
    // Bytes, history or modified state changed.
    void documentChanged();
    void profileChanged();
    void toolStateChanged();
    void selectionChanged();
    void statusMessage(const QString &message);

private:
    void updateBanner();
    // Checks writability and the layer lock for a tile edit.
    bool canEditActiveLayer();
    [[nodiscard]] const CellMask *limit() const;
    // Applies the builder's pending changes, asking first when broad.
    bool commit(EditBuilder &builder, std::size_t cellsChanged, const QString &label,
                std::uint64_t mergeKey = 0);
    void onToolPressed(CellPos cell);
    void onToolDragged(CellPos cell);
    void onToolReleased();

    std::unique_ptr<DocumentFile> file_;
    GameProfile profile_ = GameProfile::unknown;
    Tool tool_ = Tool::browse;
    SectionKind activeLayer_ = SectionKind::foreground;
    std::int16_t tileId_ = 0;
    std::size_t activeStart_ = 0;
    bool foregroundLocked_ = false;
    bool bigDecorLocked_ = false;
    std::optional<RenderOptions> isolated_;
    CellMask selection_;
    std::uint64_t gestureKey_ = 0;
    std::uint64_t nextGestureKey_ = 1;
    std::optional<CellPos> gestureStart_;
    std::optional<CellPos> gestureLast_;
    QLabel *banner_ = nullptr;
    WorldCanvas *canvas_ = nullptr;
};

[[nodiscard]] QString editErrorText(const EditFailure &failure);
[[nodiscard]] QString profileDisplayName(GameProfile profile);
[[nodiscard]] QString layerDisplayName(SectionKind layer);

} // namespace eggbert
