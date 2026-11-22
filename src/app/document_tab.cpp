// SPDX-License-Identifier: GPL-3.0-only
#include "document_tab.hpp"

#include <QFileInfo>
#include <QLabel>
#include <QVBoxLayout>

#include "world/world_stats.hpp"

namespace eggbert {

namespace {

QString fromView(std::string_view text) {
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

} // namespace

QString editErrorText(const EditFailure &failure) {
    switch (failure.error) {
    case EditError::readOnly:
        return DocumentTab::tr("This document is read-only.");
    case EditError::emptyTransaction:
        return DocumentTab::tr("Nothing would change.");
    case EditError::malformedChange:
    case EditError::unsortedOrOverlapping:
        return DocumentTab::tr("Internal error: malformed change at offset %1.")
            .arg(failure.offset);
    case EditError::outsideLayout:
        return DocumentTab::tr("The change at offset %1 is outside the world layout.")
            .arg(failure.offset);
    case EditError::protectedRange:
        return DocumentTab::tr("Offset %1 is in a preserved range and cannot be edited.")
            .arg(failure.offset);
    case EditError::staleBeforeBytes:
        return DocumentTab::tr("The document changed underneath this edit (offset %1).")
            .arg(failure.offset);
    }
    return {};
}

QString layerDisplayName(SectionKind layer) {
    switch (layer) {
    case SectionKind::foreground:
        return DocumentTab::tr("foreground");
    case SectionKind::bigDecor:
        return DocumentTab::tr("bigDecor");
    case SectionKind::header:
    case SectionKind::objects:
        break;
    }
    return {};
}

QString profileDisplayName(GameProfile profile) {
    switch (profile) {
    case GameProfile::unknown:
        return DocumentTab::tr("Profile not chosen");
    case GameProfile::speedyBlupi1:
        return DocumentTab::tr("Speedy Blupi / Eggbert 1");
    case GameProfile::speedyBlupi2:
        return DocumentTab::tr("Speedy Blupi II / Eggbert 2");
    }
    return {};
}

DocumentTab::DocumentTab(std::unique_ptr<DocumentFile> file, QWidget *parent)
    : QWidget(parent), file_(std::move(file)) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    banner_ = new QLabel(this);
    banner_->setWordWrap(true);
    banner_->setTextFormat(Qt::PlainText);
    banner_->setMargin(6);
    banner_->setStyleSheet(
        QStringLiteral("QLabel { background: #5a4a12; color: #ffe9a8; }"));
    layout->addWidget(banner_);

    canvas_ = new WorldCanvas(&file_->document(), this);
    layout->addWidget(canvas_, 1);
    setFocusProxy(canvas_);

    connect(canvas_, &WorldCanvas::toolPressed, this, &DocumentTab::onToolPressed);
    connect(canvas_, &WorldCanvas::toolDragged, this, &DocumentTab::onToolDragged);
    connect(canvas_, &WorldCanvas::toolReleased, this, &DocumentTab::onToolReleased);

    canvas_->setToolActive(false);
    updateBanner();
}

QString DocumentTab::title() const {
    auto name = QFileInfo(file_->path()).fileName();
    if (document().isModified()) {
        name += QLatin1Char('*');
    }
    return name;
}

void DocumentTab::setProfile(GameProfile profile) {
    if (profile_ == profile) {
        return;
    }
    // The profile only selects catalogs and previews; bytes are untouched.
    profile_ = profile;
    canvas_->update();
    emit profileChanged();
}

void DocumentTab::setTool(Tool tool) {
    if (tool_ == tool) {
        return;
    }
    tool_ = tool;
    canvas_->setToolActive(tool != Tool::browse);
    emit toolStateChanged();
}

bool DocumentTab::setActiveLayer(SectionKind layer) {
    if (!document().view().layer(layer)) {
        return false;
    }
    if (activeLayer_ != layer) {
        activeLayer_ = layer;
        emit toolStateChanged();
    }
    return true;
}

bool DocumentTab::applyTransaction(Transaction transaction) {
    const auto result = document().apply(std::move(transaction));
    if (!result) {
        if (result.error().error == EditError::readOnly) {
            emit statusMessage(fromView(writeAccessReason(document().writeAccess())));
        } else if (result.error().error != EditError::emptyTransaction) {
            emit statusMessage(editErrorText(result.error()));
        }
        return false;
    }
    notifyDocumentChanged();
    return true;
}

bool DocumentTab::undo() {
    if (!document().undo()) {
        return false;
    }
    notifyDocumentChanged();
    return true;
}

bool DocumentTab::redo() {
    if (!document().redo()) {
        return false;
    }
    notifyDocumentChanged();
    return true;
}

void DocumentTab::notifyDocumentChanged() {
    canvas_->update();
    emit documentChanged();
}

void DocumentTab::replaceFile(std::unique_ptr<DocumentFile> file) {
    file_ = std::move(file);
    canvas_->setDocument(&file_->document());
    if (!document().view().layer(activeLayer_)) {
        activeLayer_ = SectionKind::foreground;
    }
    gestureKey_ = 0;
    gestureStart_.reset();
    gestureLast_.reset();
    selectNone();
    updateBanner();
    emit toolStateChanged();
    notifyDocumentChanged();
}

void DocumentTab::setTileId(std::int16_t id) {
    if (tileId_ != id) {
        tileId_ = id;
        emit toolStateChanged();
    }
}

bool DocumentTab::isLayerLocked(SectionKind layer) const {
    return layer == SectionKind::foreground ? foregroundLocked_
                                            : layer == SectionKind::bigDecor && bigDecorLocked_;
}

void DocumentTab::setLayerLocked(SectionKind layer, bool locked) {
    if (layer == SectionKind::foreground) {
        foregroundLocked_ = locked;
    } else if (layer == SectionKind::bigDecor) {
        bigDecorLocked_ = locked;
    }
    emit toolStateChanged();
}

void DocumentTab::setIsolated(bool isolated) {
    if (isolated == isIsolated()) {
        return;
    }
    if (isolated) {
        isolated_ = canvas_->renderOptions();
        auto options = *isolated_;
        options.showForeground = activeLayer_ == SectionKind::foreground;
        options.showBigDecor = activeLayer_ == SectionKind::bigDecor;
        options.showObjects = false;
        options.showStarts = false;
        options.showPaths = false;
        canvas_->setRenderOptions(options);
    } else {
        canvas_->setRenderOptions(*isolated_);
        isolated_.reset();
    }
    emit toolStateChanged();
}

bool DocumentTab::canEditActiveLayer() {
    if (!document().isWritable()) {
        emit statusMessage(fromView(writeAccessReason(document().writeAccess())));
        return false;
    }
    if (isLayerLocked(activeLayer_)) {
        emit statusMessage(
            tr("The %1 layer is locked.").arg(layerDisplayName(activeLayer_)));
        return false;
    }
    return true;
}

const CellMask *DocumentTab::limit() const {
    return selection_.empty() ? nullptr : &selection_;
}

bool DocumentTab::commit(EditBuilder &builder, std::size_t cellsChanged, const QString &label,
                         std::uint64_t mergeKey) {
    if (!builder.hasChanges()) {
        return false;
    }
    auto transaction = builder.build(label.toStdString(), mergeKey);
    if (cellsChanged > broadEditCells && confirmBroadEdit) {
        BroadEditPreview preview;
        preview.label = label;
        preview.cellsChanged = cellsChanged;
        preview.bytesChanged = transaction.changedByteCount();
        preview.byteRanges = transaction.changes.size();
        preview.firstOffset = transaction.changes.front().offset;
        preview.lastOffset = transaction.changes.back().range().end() - 1;
        if (!confirmBroadEdit(preview)) {
            return false;
        }
    }
    return applyTransaction(std::move(transaction));
}

bool DocumentTab::paintStroke(std::span<const CellPos> cells, std::int16_t value,
                              const QString &label, std::uint64_t mergeKey) {
    if (!canEditActiveLayer()) {
        return false;
    }
    EditBuilder builder(document());
    paintCells(builder, activeLayer_, cells, value, limit());
    if (!builder.hasChanges()) {
        return false;
    }
    // Strokes grow cell by cell and are never "broad" in one step.
    return applyTransaction(builder.build(label.toStdString(), mergeKey));
}

bool DocumentTab::pencilAt(CellPos cell, std::uint64_t mergeKey) {
    const CellPos cells[] = {cell};
    return paintStroke(cells, tileId_, tr("Pencil"), mergeKey);
}

bool DocumentTab::pickAt(CellPos cell) {
    const auto layer = document().view().layer(activeLayer_);
    if (!layer) {
        return false;
    }
    const auto value = layer->cell(cell.x, cell.y);
    if (!value) {
        return false;
    }
    setTileId(*value);
    emit statusMessage(tr("Picked value %1 from the %2 layer at (%3, %4)")
                           .arg(*value)
                           .arg(layerDisplayName(activeLayer_))
                           .arg(cell.x)
                           .arg(cell.y));
    return true;
}

bool DocumentTab::drawLine(CellPos from, CellPos to) {
    if (!canEditActiveLayer()) {
        return false;
    }
    EditBuilder builder(document());
    const auto cells = lineCells(from, to);
    const auto changed = paintCells(builder, activeLayer_, cells, tileId_, limit());
    return commit(builder, changed, tr("Line"));
}

bool DocumentTab::drawRectangle(CellPos first, CellPos second) {
    if (!canEditActiveLayer()) {
        return false;
    }
    EditBuilder builder(document());
    const auto changed =
        fillRect(builder, activeLayer_, rectFromCorners(first, second), tileId_, limit());
    return commit(builder, changed, tr("Rectangle"));
}

bool DocumentTab::fillAt(CellPos cell) {
    if (!canEditActiveLayer()) {
        return false;
    }
    EditBuilder builder(document());
    const auto changed = floodFill(builder, activeLayer_, cell, tileId_, limit());
    return commit(builder, changed, tr("Fill"));
}

void DocumentTab::setSelection(const CellMask &mask) {
    if (selection_ == mask) {
        return;
    }
    selection_ = mask;
    canvas_->setSelectionMask(selection_);
    emit selectionChanged();
}

void DocumentTab::selectAll() {
    setSelection(CellMask::fromRect(wholeMap()));
}

void DocumentTab::selectNone() {
    setSelection({});
}

void DocumentTab::invertSelection() {
    auto mask = selection_;
    mask.invert();
    setSelection(mask);
}

std::size_t DocumentTab::selectValue(std::int16_t value) {
    CellMask mask;
    for (const auto &cell : findCells(document().view(), activeLayer_, value)) {
        mask.set(cell);
    }
    setSelection(mask);
    return mask.count();
}

TileClipboard &TileClipboard::shared() {
    static TileClipboard clipboard;
    return clipboard;
}

bool DocumentTab::copySelection() {
    if (selection_.empty()) {
        emit statusMessage(tr("Nothing is selected."));
        return false;
    }
    auto patch = copyPatch(document().view(), activeLayer_, selection_);
    if (patch.empty()) {
        return false;
    }
    auto &clipboard = TileClipboard::shared();
    clipboard.patch = std::move(patch);
    clipboard.profile = profile_;
    clipboard.sourceTab = this;
    emit statusMessage(tr("Copied %n cell(s) from the %1 layer", nullptr,
                          static_cast<int>(clipboard.patch.presentCount()))
                           .arg(layerDisplayName(activeLayer_)));
    emit toolStateChanged();
    return true;
}

bool DocumentTab::cutSelection() {
    if (selection_.empty() || !canEditActiveLayer()) {
        return false;
    }
    if (!copySelection()) {
        return false;
    }
    EditBuilder builder(document());
    const auto changed = fillMask(builder, activeLayer_, selection_, layout::emptyCell);
    if (!builder.hasChanges()) {
        return true; // copied; the cells were already empty
    }
    return commit(builder, changed, tr("Cut"));
}

bool DocumentTab::canPaste() const {
    const auto &clipboard = TileClipboard::shared();
    if (clipboard.patch.empty()) {
        return false;
    }
    // Stored IDs mean different tiles in the two games; without a verified
    // mapping, tiles only move within one tab or between equal known profiles.
    return clipboard.sourceTab == this ||
           (profile_ != GameProfile::unknown && clipboard.profile == profile_);
}

bool DocumentTab::paste() {
    const auto &clipboard = TileClipboard::shared();
    if (clipboard.patch.empty()) {
        emit statusMessage(tr("The clipboard holds no tiles."));
        return false;
    }
    if (!canPaste()) {
        emit statusMessage(tr("These tiles were copied under a different or unknown game "
                              "profile; pasting them here is not available."));
        return false;
    }
    if (!canEditActiveLayer()) {
        return false;
    }
    const auto origin = canvas_->cursorCell();
    EditBuilder builder(document());
    const auto changed =
        pastePatch(builder, activeLayer_, clipboard.patch, static_cast<long long>(origin.x),
                   static_cast<long long>(origin.y));
    CellMask pasted;
    for (const auto &cell : pasteFootprint(clipboard.patch, static_cast<long long>(origin.x),
                                           static_cast<long long>(origin.y))) {
        pasted.set(cell);
    }
    const auto clipped = clipboard.patch.presentCount() - pasted.count();
    // The selection must not restrict a paste; it becomes the pasted area.
    if (!builder.hasChanges()) {
        setSelection(pasted);
        emit statusMessage(tr("The pasted tiles equal what is already there."));
        return false;
    }
    if (!commit(builder, changed, tr("Paste"))) {
        return false;
    }
    setSelection(pasted);
    if (clipped != 0) {
        emit statusMessage(
            tr("%n cell(s) fell outside the map and were not pasted", nullptr,
               static_cast<int>(clipped)));
    }
    return true;
}

bool DocumentTab::deleteSelection() {
    if (selection_.empty() || !canEditActiveLayer()) {
        return false;
    }
    EditBuilder builder(document());
    const auto changed = fillMask(builder, activeLayer_, selection_, layout::emptyCell);
    return commit(builder, changed, tr("Delete"));
}

bool DocumentTab::fillSelection() {
    if (selection_.empty() || !canEditActiveLayer()) {
        return false;
    }
    EditBuilder builder(document());
    const auto changed = fillMask(builder, activeLayer_, selection_, tileId_);
    return commit(builder, changed, tr("Fill Selection"));
}

bool DocumentTab::replaceInSelection(std::int16_t from, std::int16_t to) {
    if (!canEditActiveLayer()) {
        return false;
    }
    EditBuilder builder(document());
    const auto changed = replaceValue(builder, activeLayer_, from, to, limit());
    return commit(builder, changed, tr("Replace %1 with %2").arg(from).arg(to));
}

bool DocumentTab::flipSelection(bool horizontally) {
    const auto bounds = selection_.bounds();
    if (!bounds || !canEditActiveLayer()) {
        return false;
    }
    const auto patch = copyPatch(document().view(), activeLayer_, selection_);
    const auto flipped = horizontally ? flippedHorizontally(patch) : flippedVertically(patch);
    EditBuilder builder(document());
    // Clear the old cells, then write the mirrored block into the same box.
    fillMask(builder, activeLayer_, selection_, layout::emptyCell);
    pastePatch(builder, activeLayer_, flipped, static_cast<long long>(bounds->min.x),
               static_cast<long long>(bounds->min.y));
    CellMask mirrored;
    for (const auto &cell : pasteFootprint(flipped, static_cast<long long>(bounds->min.x),
                                           static_cast<long long>(bounds->min.y))) {
        mirrored.set(cell);
    }
    const auto changedBytes = builder.build({}).changedByteCount();
    if (!commit(builder, (changedBytes + 1) / 2,
                horizontally ? tr("Flip Horizontally") : tr("Flip Vertically"))) {
        return false;
    }
    setSelection(mirrored);
    return true;
}

void DocumentTab::onToolPressed(CellPos cell) {
    gestureStart_ = cell;
    gestureLast_ = cell;
    switch (tool_) {
    case Tool::browse:
        break;
    case Tool::pencil:
    case Tool::eraser: {
        gestureKey_ = nextGestureKey_++;
        const CellPos cells[] = {cell};
        paintStroke(cells, tool_ == Tool::pencil ? tileId_ : layout::emptyCell,
                    tool_ == Tool::pencil ? tr("Pencil") : tr("Eraser"), gestureKey_);
        break;
    }
    case Tool::eyedropper:
        pickAt(cell);
        break;
    case Tool::fill:
        fillAt(cell);
        break;
    case Tool::line:
        canvas_->setPreviewCells({cell});
        break;
    case Tool::rectangle:
        canvas_->setPreviewCells({cell});
        break;
    case Tool::select:
        setSelection(CellMask::fromRect({cell, cell}));
        break;
    }
}

void DocumentTab::onToolDragged(CellPos cell) {
    if (!gestureStart_) {
        return;
    }
    switch (tool_) {
    case Tool::pencil:
    case Tool::eraser:
        if (gestureKey_ != 0) {
            // Cover every cell between samples so fast drags leave no gaps.
            const auto cells = lineCells(gestureLast_.value_or(cell), cell);
            paintStroke(cells, tool_ == Tool::pencil ? tileId_ : layout::emptyCell,
                        tool_ == Tool::pencil ? tr("Pencil") : tr("Eraser"), gestureKey_);
        }
        break;
    case Tool::eyedropper:
        pickAt(cell);
        break;
    case Tool::line:
        canvas_->setPreviewCells(lineCells(*gestureStart_, cell));
        break;
    case Tool::rectangle:
        canvas_->setPreviewCells(CellMask::fromRect(rectFromCorners(*gestureStart_, cell)).cells());
        break;
    case Tool::select:
        setSelection(CellMask::fromRect(rectFromCorners(*gestureStart_, cell)));
        break;
    case Tool::browse:
    case Tool::fill:
        break;
    }
    gestureLast_ = cell;
}

void DocumentTab::onToolReleased() {
    const auto start = gestureStart_;
    const auto last = gestureLast_;
    gestureStart_.reset();
    gestureLast_.reset();
    gestureKey_ = 0;
    document().endMerge();
    canvas_->setPreviewCells({});
    if (!start || !last) {
        return;
    }
    if (tool_ == Tool::line) {
        drawLine(*start, *last);
    } else if (tool_ == Tool::rectangle) {
        drawRectangle(*start, *last);
    }
}

QString DocumentTab::describeCell(CellPos cell) const {
    const auto view = document().view();
    QStringList parts;
    parts << tr("Cell (%1, %2)").arg(cell.x).arg(cell.y);
    if (const auto layer = view.foreground()) {
        parts << tr("foreground %1 @ offset %2")
                     .arg(*layer->cell(cell.x, cell.y))
                     .arg(layer->cellRange(cell.x, cell.y)->offset);
    }
    if (const auto layer = view.bigDecor()) {
        parts << tr("bigDecor %1 @ offset %2")
                     .arg(*layer->cell(cell.x, cell.y))
                     .arg(layer->cellRange(cell.x, cell.y)->offset);
    }
    return parts.join(QStringLiteral("   "));
}

void DocumentTab::updateBanner() {
    const auto access = document().writeAccess();
    if (access == WriteAccess::writable) {
        banner_->hide();
        return;
    }
    banner_->setText(tr("Read-only: %1").arg(fromView(writeAccessReason(access))));
    banner_->show();
}

} // namespace eggbert
