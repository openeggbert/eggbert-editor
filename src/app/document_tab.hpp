// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QWidget>

#include <memory>

#include "app/document_file.hpp"
#include "app/world_canvas.hpp"

class QLabel;

namespace eggbert {

enum class Tool { browse, pencil };

// One open world: its file, the user-chosen game profile (never stored in
// the file), tool state and canvas. All edits go through applyTransaction().
class DocumentTab final : public QWidget {
    Q_OBJECT

public:
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
    void setTileId(std::int16_t id) { tileId_ = id; }

    // The single mutation entry point for every tool, panel and command.
    // Returns false and reports why when the document refuses the change.
    bool applyTransaction(Transaction transaction);
    bool undo();
    bool redo();
    // Call after the file was saved or otherwise changed outside apply().
    void notifyDocumentChanged();
    // Replaces the document with a freshly loaded file (Revert).
    void replaceFile(std::unique_ptr<DocumentFile> file);

    // Raw pencil: writes exactly the selected tile ID into one cell of the
    // active layer, with no neighbour or object effects.
    bool pencilAt(CellPos cell, std::uint64_t mergeKey);

    // Human-readable description of what is stored at a cell.
    [[nodiscard]] QString describeCell(CellPos cell) const;

signals:
    // Bytes, history or modified state changed.
    void documentChanged();
    void profileChanged();
    void toolStateChanged();
    void statusMessage(const QString &message);

private:
    void updateBanner();

    std::unique_ptr<DocumentFile> file_;
    GameProfile profile_ = GameProfile::unknown;
    Tool tool_ = Tool::browse;
    SectionKind activeLayer_ = SectionKind::foreground;
    std::int16_t tileId_ = 0;
    std::uint64_t gestureKey_ = 0;
    std::uint64_t nextGestureKey_ = 1;
    QLabel *banner_ = nullptr;
    WorldCanvas *canvas_ = nullptr;
};

[[nodiscard]] QString editErrorText(const EditFailure &failure);
[[nodiscard]] QString profileDisplayName(GameProfile profile);

} // namespace eggbert
