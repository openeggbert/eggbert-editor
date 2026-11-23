// SPDX-License-Identifier: GPL-3.0-only
#include "document_tab.hpp"

#include "app/catalog_texts.hpp"
#include "app/core_texts.hpp"

#include <QFileInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QVBoxLayout>

#include <algorithm>

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
    case GameProfile::mobileEggbert:
        return DocumentTab::tr("Mobile Eggbert");
    }
    return {};
}

DocumentTab::DocumentTab(std::unique_ptr<DocumentFile> file, QWidget *parent)
    : QWidget(parent), file_(std::move(file)) {
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    banner_ = new QLabel(this);
    banner_->setObjectName(QStringLiteral("documentBanner"));
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

    connect(canvas_, &WorldCanvas::cursorCellChanged, this, [this] {
        if (pendingStamp_) {
            showStampGhost(canvas_->cursorCell());
        }
    });
    // Hovering with the adaptive brush shows every cell a click would change.
    connect(canvas_, &WorldCanvas::hoveredCellChanged, this, [this] {
        if (pendingStamp_) {
            showStampGhost(canvas_->hoveredCell().value_or(canvas_->cursorCell()));
            return;
        }
        if (tool_ != Tool::adaptive || gestureStart_) {
            return;
        }
        const auto hovered = canvas_->hoveredCell();
        if (!hovered) {
            canvas_->setPreviewCells({});
            return;
        }
        const auto preview = previewAdaptive(*hovered);
        auto cells = preview.changedFootprint;
        cells.insert(cells.end(), preview.sideEffects.begin(), preview.sideEffects.end());
        canvas_->setPreviewCells(std::move(cells));
    });
    variants_ = std::make_unique<SeededVariants>(
        static_cast<std::uint32_t>(reinterpret_cast<quintptr>(this) >> 4) ^ 0x5EED1234u);

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
    canvas_->setProfile(profile);
    canvas_->update();
    emit profileChanged();
}

void DocumentTab::setTool(Tool tool) {
    // Choosing a tool, even the current one, gives an armed stamp up.
    cancelStampPlacement();
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

bool DocumentTab::replaceBytes(std::span<const std::byte> wanted, const QString &label) {
    const auto current = document().currentBytes();
    if (wanted.size() != current.size()) {
        return false;
    }
    Transaction transaction;
    transaction.label = label.toStdString();
    for (const auto &range : differingRanges(current, wanted)) {
        ByteChange change;
        change.offset = range.offset;
        change.before.assign(current.begin() + static_cast<std::ptrdiff_t>(range.offset),
                             current.begin() + static_cast<std::ptrdiff_t>(range.offset + range.size));
        change.after.assign(wanted.begin() + static_cast<std::ptrdiff_t>(range.offset),
                            wanted.begin() + static_cast<std::ptrdiff_t>(range.offset + range.size));
        transaction.changes.push_back(std::move(change));
    }
    if (transaction.changes.empty()) {
        return false;
    }
    return applyTransaction(std::move(transaction));
}

bool DocumentTab::applyTransaction(Transaction transaction) {
    return applyWithPolicy(std::move(transaction), EditPolicy::guarded);
}

bool DocumentTab::applyExpertTransaction(Transaction transaction) {
    return applyWithPolicy(std::move(transaction), EditPolicy::expert);
}

bool DocumentTab::applyWithPolicy(Transaction transaction, EditPolicy policy) {
    const auto result = document().apply(std::move(transaction), policy);
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
    // An undo or delete may have freed the selected slot.
    if (selectedSlot_) {
        const auto objects = document().view().objects();
        const auto record = objects ? objects->record(*selectedSlot_) : std::nullopt;
        if (!record || record->isFree()) {
            setSelectedSlot(std::nullopt);
        }
    }
    canvas_->update();
    emit documentChanged();
}

void DocumentTab::replaceFile(std::unique_ptr<DocumentFile> file) {
    file_ = std::move(file);
    canvas_->setDocument(&file_->document());
    emit documentReplaced();
    if (!document().view().layer(activeLayer_)) {
        activeLayer_ = SectionKind::foreground;
    }
    gestureKey_ = 0;
    gestureStart_.reset();
    gestureLast_.reset();
    grab_ = Grab::none;
    selectNone();
    setSelectedSlot(std::nullopt);
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

bool DocumentTab::applyMetadataEdit(const Metadata &metadata) {
    if (!document().isWritable()) {
        emit statusMessage(fromView(writeAccessReason(document().writeAccess())));
        return false;
    }
    auto wanted = metadata;
    if (file_->isTextWorld()) {
        // A Mobile Eggbert world has no title and one start.
        wanted.name.clear();
        for (std::size_t index = 1; index < wanted.starts.size(); ++index) {
            wanted.starts[index] = wanted.starts[0];
        }
    }
    EditBuilder builder(document());
    if (!applyMetadata(builder, wanted) || !builder.hasChanges()) {
        return false;
    }
    return applyTransaction(builder.build(tr("Edit Metadata").toStdString()));
}

void DocumentTab::setActiveStart(std::size_t index) {
    if (file_->isTextWorld()) {
        return; // one start
    }
    if (index < 4 && index != activeStart_) {
        activeStart_ = index;
        emit toolStateChanged();
    }
}

bool DocumentTab::placeStartAt(CellPos cell) {
    if (!document().isWritable()) {
        emit statusMessage(fromView(writeAccessReason(document().writeAccess())));
        return false;
    }
    const auto header = document().view().header();
    if (!header) {
        return false;
    }
    EditBuilder builder(document());
    QString label;
    if (*header->blupiPos(activeStart_) == startPositionForCell(cell)) {
        toggleStartDirection(builder, activeStart_);
        label = tr("Turn Start %1").arg(activeStart_ + 1);
    } else {
        placeStart(builder, activeStart_, cell);
        label = tr("Move Start %1").arg(activeStart_ + 1);
    }
    if (file_->isTextWorld()) {
        // The one start of a Mobile Eggbert world, kept in all four places.
        copyStartToAll(builder, activeStart_);
        label = tr("Move Start");
    }
    if (!builder.hasChanges()) {
        return false;
    }
    return applyTransaction(builder.build(label.toStdString()));
}

bool DocumentTab::copyActiveStartToAll() {
    if (!document().isWritable()) {
        emit statusMessage(fromView(writeAccessReason(document().writeAccess())));
        return false;
    }
    EditBuilder builder(document());
    if (!copyStartToAll(builder, activeStart_) || !builder.hasChanges()) {
        return false;
    }
    return applyTransaction(
        builder.build(tr("Copy Start %1 to All").arg(activeStart_ + 1).toStdString()));
}

void DocumentTab::setAdaptiveBrush(const QString &id) {
    if (adaptiveBrush_ != id) {
        adaptiveBrush_ = id;
        emit toolStateChanged();
    }
}

void DocumentTab::setCustomBorderRules(
    std::shared_ptr<const std::vector<CustomBorderRule>> rules) {
    customRules_ = std::move(rules);
    emit toolStateChanged();
}

const CustomBorderRule *DocumentTab::customBorderRule() const {
    const QLatin1String prefix(customBrushPrefix);
    if (customRules_ == nullptr || !adaptiveBrush_.startsWith(prefix)) {
        return nullptr;
    }
    bool ok = false;
    const auto index = adaptiveBrush_.mid(prefix.size()).toULongLong(&ok);
    return ok && index < customRules_->size() ? &(*customRules_)[index] : nullptr;
}

void DocumentTab::setVariantSource(std::unique_ptr<VariantSource> variants) {
    variants_ = std::move(variants);
}

std::optional<std::int16_t> DocumentTab::adaptiveValue() const {
    if (adaptiveBrush_ == QLatin1String(adaptiveTileBrush)) {
        return tileId_;
    }
    if (adaptiveBrush_ == QLatin1String(adaptiveEraseBrush)) {
        return layout::emptyCell;
    }
    const auto *family = findAdaptiveFamily(profile_, adaptiveBrush_.toStdString());
    return family != nullptr ? std::optional<std::int16_t>(family->seed) : std::nullopt;
}

AdaptiveResult DocumentTab::previewAdaptive(CellPos cell) const {
    if (const auto *rule = customBorderRule()) {
        if (!document().isWritable() || isLayerLocked(SectionKind::foreground) ||
            (!selection_.empty() && !selection_.test(cell))) {
            return {};
        }
        EditBuilder builder(document());
        return customBorderStroke(builder, *rule, {&cell, 1}, false);
    }
    const auto value = adaptiveValue();
    if (!value || profile_ == GameProfile::unknown || !document().isWritable() ||
        isLayerLocked(SectionKind::foreground) ||
        (!selection_.empty() && !selection_.test(cell))) {
        return {};
    }
    // Looks are not predicted: the preview only shows which cells change.
    EditBuilder builder(document());
    NoVariants plain;
    return adaptiveStroke(builder, profile_, {&cell, 1}, *value, plain);
}

bool DocumentTab::adaptiveAt(std::span<const CellPos> cells, std::uint64_t mergeKey) {
    if (!requireWritable()) {
        return false;
    }
    const auto *customRule = customBorderRule();
    if (customRule == nullptr && profile_ == GameProfile::unknown) {
        emit statusMessage(tr("Choose a game profile first: border rules are verified per game."));
        return false;
    }
    if (isLayerLocked(SectionKind::foreground)) {
        emit statusMessage(tr("The %1 layer is locked.").arg(layerDisplayName(SectionKind::foreground)));
        return false;
    }
    const auto value = adaptiveValue();
    if (customRule == nullptr && !value) {
        emit statusMessage(tr("Choose an adaptive brush of the chosen game first."));
        return false;
    }
    // The footprint obeys the selection; neighbours outside it may still
    // take a new border form, which is the point of the tool.
    std::vector<CellPos> footprint;
    for (const auto &cell : cells) {
        if (selection_.empty() || selection_.test(cell)) {
            footprint.push_back(cell);
        }
    }
    if (footprint.empty()) {
        return false;
    }
    EditBuilder builder(document());
    const auto result =
        customRule != nullptr
            ? customBorderStroke(builder, *customRule, footprint, false)
            : adaptiveStroke(builder, profile_, footprint, *value, *variants_);
    if (!builder.hasChanges()) {
        return false;
    }
    const auto label = customRule != nullptr ? tr("Custom Border Brush") : tr("Adaptive Brush");
    if (!applyTransaction(builder.build(label.toStdString(), mergeKey))) {
        return false;
    }
    if (!result.sideEffects.empty()) {
        emit statusMessage(tr("%n neighbouring cell(s) outside the brush took a new form", nullptr,
                              static_cast<int>(result.sideEffects.size())));
    }
    return true;
}

bool DocumentTab::requireWritable() {
    if (document().isWritable()) {
        return true;
    }
    emit statusMessage(fromView(writeAccessReason(document().writeAccess())));
    return false;
}

void DocumentTab::reportObjectError(ObjectOpError error) {
    const auto capacity = poolCapacity(document().view());
    switch (error) {
    case ObjectOpError::poolFull:
        emit statusMessage(tr("The object pool is full (%1 of %2 slots in use); nothing was "
                              "created.")
                               .arg(capacity.active)
                               .arg(capacity.stored));
        break;
    case ObjectOpError::noObjectPool:
        emit statusMessage(tr("This document has no complete object pool."));
        break;
    case ObjectOpError::invalidSlot:
    case ObjectOpError::slotIsFree:
        emit statusMessage(tr("That slot holds no object."));
        break;
    case ObjectOpError::outsideMap:
        emit statusMessage(tr("The position is outside the representable range."));
        break;
    case ObjectOpError::notApplicable:
        emit statusMessage(tr("That operation does not apply to this object."));
        break;
    }
}

bool DocumentTab::applyObjectEdit(EditBuilder &builder, const QString &label) {
    if (!builder.hasChanges()) {
        return false;
    }
    return applyTransaction(builder.build(label.toStdString()));
}

void DocumentTab::setSelectedSlot(std::optional<std::size_t> slot) {
    if (slot) {
        const auto objects = document().view().objects();
        if (!objects || *slot >= objects->slotCount()) {
            slot.reset();
        }
    }
    if (selectedSlot_ == slot) {
        return;
    }
    selectedSlot_ = slot;
    canvas_->setSelectedSlot(slot);
    emit objectSelectionChanged();
}

const ObjectPreset *DocumentTab::placePreset() const {
    return findObjectPreset(profile_, placePresetId_.toStdString());
}

void DocumentTab::setPlacePreset(const QString &id) {
    if (placePresetId_ != id) {
        placePresetId_ = id;
        emit toolStateChanged();
    }
}

QString DocumentTab::objectTypeText(std::int16_t type) const {
    const auto info = objectTypeInfo(profile_, type);
    switch (info.status) {
    case ObjectTypeStatus::placeable:
        return tr("%1 (%2)").arg(catalogText(info.name)).arg(type);
    case ObjectTypeStatus::runtimeOnly:
        return tr("%1 (%2, created during play)").arg(catalogText(info.name)).arg(type);
    case ObjectTypeStatus::unknown:
        break;
    }
    return profile_ == GameProfile::unknown ? tr("Type %1").arg(type)
                                            : tr("Type %1 (not verified for this game)").arg(type);
}

bool DocumentTab::canDeleteObject(std::size_t slot) const {
    const auto objects = document().view().objects();
    const auto record = objects ? objects->record(slot) : std::nullopt;
    return document().isWritable() && record && !record->isFree();
}

bool DocumentTab::canEditObject(std::size_t slot) const {
    if (!canDeleteObject(slot)) {
        return false;
    }
    const auto record = *document().view().objects()->record(slot);
    return objectTypeInfo(profile_, record.type).status == ObjectTypeStatus::placeable;
}

bool DocumentTab::isEditableObjectField(ObjectField field) {
    switch (field) {
    case ObjectField::stepAdvance:
    case ObjectField::stepRecede:
    case ObjectField::timeStopStart:
    case ObjectField::timeStopEnd:
    case ObjectField::posStart:
    case ObjectField::posEnd:
    case ObjectField::posCurrent:
    case ObjectField::channel:
    case ObjectField::icon:
        return true;
    case ObjectField::type:
    case ObjectField::unknown1:
    case ObjectField::step:
    case ObjectField::time:
    case ObjectField::phase:
    case ObjectField::unknown2:
        break;
    }
    return false;
}

std::optional<std::size_t> DocumentTab::createObjectAt(CellPos cell,
                                                       std::optional<CellPos> pathEnd) {
    if (!requireWritable()) {
        return std::nullopt;
    }
    const auto *preset = placePreset();
    if (preset == nullptr) {
        emit statusMessage(profile_ == GameProfile::unknown
                               ? tr("Choose a game profile first: object presets are verified "
                                    "per game.")
                               : tr("Choose an object preset first."));
        return std::nullopt;
    }
    ObjectRequest request{preset, cell, {}, {}};
    if (preset->hasPath && pathEnd && *pathEnd != cell) {
        request.pathEnd = pathEnd;
    }
    EditBuilder builder(document());
    const auto slot = createObject(builder, request);
    if (!slot) {
        reportObjectError(slot.error());
        return std::nullopt;
    }
    if (!applyTransaction(
            builder.build(tr("Create %1").arg(catalogText(preset->name)).toStdString()))) {
        return std::nullopt;
    }
    setSelectedSlot(*slot);
    emit statusMessage(tr("Created %1 in slot %2").arg(catalogText(preset->name)).arg(*slot));
    return *slot;
}

bool DocumentTab::deleteSelectedObject() {
    if (!selectedSlot_ || !requireWritable()) {
        return false;
    }
    EditBuilder builder(document());
    const auto result = deleteObject(builder, *selectedSlot_);
    if (!result) {
        reportObjectError(result.error());
        return false;
    }
    const auto slot = *selectedSlot_;
    if (!applyObjectEdit(builder, tr("Delete Object in Slot %1").arg(slot))) {
        return false;
    }
    setSelectedSlot(std::nullopt);
    return true;
}

bool DocumentTab::duplicateSelectedObject() {
    if (!selectedSlot_ || !requireWritable()) {
        return false;
    }
    if (!canEditObject(*selectedSlot_)) {
        emit statusMessage(tr("Only objects of a type verified for the chosen game can be "
                              "duplicated."));
        return false;
    }
    EditBuilder builder(document());
    const auto slot = duplicateObject(builder, *selectedSlot_, {layout::cellPixels, 0});
    if (!slot) {
        reportObjectError(slot.error());
        return false;
    }
    if (!applyObjectEdit(builder, tr("Duplicate Object in Slot %1").arg(*selectedSlot_))) {
        return false;
    }
    setSelectedSlot(*slot);
    return true;
}

bool DocumentTab::moveSelectedObject(Point delta) {
    if (!selectedSlot_ || !requireWritable()) {
        return false;
    }
    if (!canEditObject(*selectedSlot_)) {
        emit statusMessage(tr("Only objects of a type verified for the chosen game can be moved."));
        return false;
    }
    EditBuilder builder(document());
    const auto result = moveObject(builder, *selectedSlot_, delta);
    if (!result) {
        reportObjectError(result.error());
        return false;
    }
    return applyObjectEdit(builder, tr("Move Object in Slot %1").arg(*selectedSlot_));
}

bool DocumentTab::moveSelectedStart(Point position) {
    if (!selectedSlot_ || !requireWritable() || !canEditObject(*selectedSlot_)) {
        return false;
    }
    EditBuilder builder(document());
    if (const auto result = moveStart(builder, *selectedSlot_, position); !result) {
        reportObjectError(result.error());
        return false;
    }
    return applyObjectEdit(builder, tr("Move Start of Slot %1").arg(*selectedSlot_));
}

bool DocumentTab::moveSelectedEnd(Point position) {
    if (!selectedSlot_ || !requireWritable() || !canEditObject(*selectedSlot_)) {
        return false;
    }
    EditBuilder builder(document());
    if (const auto result = moveEnd(builder, *selectedSlot_, position); !result) {
        reportObjectError(result.error());
        return false;
    }
    return applyObjectEdit(builder, tr("Move End of Slot %1").arg(*selectedSlot_));
}

bool DocumentTab::setObjectNumber(std::size_t slot, ObjectField field, std::int16_t value) {
    if (!requireWritable() || !canEditObject(slot) || !isEditableObjectField(field)) {
        return false;
    }
    EditBuilder builder(document());
    if (!builder.setObjectValue(slot, field, value)) {
        return false;
    }
    return applyObjectEdit(
        builder, tr("Set %1 of Slot %2").arg(fromView(objectField(field).name)).arg(slot));
}

bool DocumentTab::setObjectPosition(std::size_t slot, ObjectField field, Point value) {
    if (!requireWritable() || !canEditObject(slot) || !isEditableObjectField(field)) {
        return false;
    }
    EditBuilder builder(document());
    if (!builder.setObjectPoint(slot, field, value)) {
        return false;
    }
    return applyObjectEdit(
        builder, tr("Set %1 of Slot %2").arg(fromView(objectField(field).name)).arg(slot));
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

std::size_t DocumentTab::countValue(SectionKind layer, std::int16_t from) const {
    if (!document().isWritable() || isLayerLocked(layer)) {
        return 0;
    }
    EditBuilder builder(document());
    // Counted by replacing with a value that differs, in a scratch builder.
    return replaceValue(builder, layer, from, static_cast<std::int16_t>(from == 0 ? 1 : 0));
}

bool DocumentTab::replaceEverywhere(SectionKind layer, std::int16_t from, std::int16_t to,
                                    const QString &label) {
    if (!requireWritable()) {
        return false;
    }
    if (isLayerLocked(layer)) {
        emit statusMessage(tr("The %1 layer is locked.").arg(layerDisplayName(layer)));
        return false;
    }
    EditBuilder builder(document());
    replaceValue(builder, layer, from, to);
    if (!builder.hasChanges()) {
        return false;
    }
    return applyTransaction(builder.build(label.toStdString()));
}

std::optional<Stamp> DocumentTab::stampFromSelection(const QString &name,
                                                     bool withObjects) const {
    const auto bounds = selection_.bounds();
    if (!bounds) {
        return std::nullopt;
    }
    return makeStamp(document().view(), *bounds, name.toStdString(), profile_, withObjects);
}

bool DocumentTab::placeStampAt(const Stamp &stamp, CellPos at) {
    if (!requireWritable()) {
        return false;
    }
    if (stamp.game != GameProfile::unknown && stamp.game != profile_) {
        emit statusMessage(tr("This stamp was made under another game profile; placing it here "
                              "is not available."));
        return false;
    }
    for (const auto layer : {SectionKind::foreground, SectionKind::bigDecor}) {
        if (isLayerLocked(layer)) {
            emit statusMessage(tr("The %1 layer is locked.").arg(layerDisplayName(layer)));
            return false;
        }
    }
    EditBuilder builder(document());
    // A text world has no place for the two unexplained numbers of an object.
    auto plain = stamp;
    if (file_->isTextWorld()) {
        for (auto &object : plain.objects) {
            object.unknown1 = 0;
            object.unknown2 = 0;
        }
    }
    const auto placed = placeStamp(builder, plain, at);
    if (!placed) {
        emit statusMessage(coreText(placed.error()));
        return false;
    }
    if (!builder.hasChanges()) {
        emit statusMessage(tr("The stamp equals what is already there."));
        return false;
    }
    if (!commit(builder, placed->tilesChanged,
                tr("Place Stamp %1").arg(QString::fromStdString(stamp.name)))) {
        return false;
    }
    emit statusMessage(tr("%1 tile(s) written, %2 object(s) created%3")
                           .arg(placed->tilesChanged)
                           .arg(placed->objectsCreated)
                           .arg(placed->tilesClipped != 0
                                    ? tr("; %1 fell outside the map").arg(placed->tilesClipped)
                                    : QString()));
    return true;
}

bool DocumentTab::beginStampPlacement(Stamp stamp) {
    if (!requireWritable()) {
        return false;
    }
    if (stamp.game != GameProfile::unknown && stamp.game != profile_) {
        emit statusMessage(tr("This stamp was made under another game profile; placing it here "
                              "is not available."));
        return false;
    }
    pendingStamp_ = std::move(stamp);
    canvas_->setToolActive(true);
    // Under the pointer when it is over the map, else at the keyboard cursor.
    showStampGhost(canvas_->hoveredCell().value_or(canvas_->cursorCell()));
    emit statusMessage(tr("Stamp \"%1\": click where its top-left corner goes, or move the "
                          "cursor there and press Enter. Esc gives up.")
                           .arg(QString::fromStdString(pendingStamp_->name)));
    return true;
}

void DocumentTab::cancelStampPlacement() {
    if (!pendingStamp_) {
        return;
    }
    endStampPlacement();
    emit statusMessage(tr("The stamp was not placed."));
}

void DocumentTab::endStampPlacement() {
    pendingStamp_.reset();
    canvas_->setToolActive(tool_ != Tool::browse);
    canvas_->setPreviewCells({});
}

void DocumentTab::showStampGhost(CellPos at) {
    stampGhostAt_ = at;
    canvas_->setPreviewCells(stampFootprint(*pendingStamp_, at));
}

void DocumentTab::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Escape && pendingStamp_) {
        cancelStampPlacement();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

std::vector<std::size_t> DocumentTab::objectsInSelection() const {
    std::vector<std::size_t> found;
    const auto pool = document().view().objects();
    if (!pool || selection_.empty()) {
        return found;
    }
    for (std::size_t slot = 0; slot < pool->slotCount(); ++slot) {
        const auto record = *pool->record(slot);
        if (record.isFree() || record.posStart.x < 0 || record.posStart.y < 0) {
            continue;
        }
        const CellPos cell{static_cast<std::size_t>(record.posStart.x) / layout::cellPixels,
                           static_cast<std::size_t>(record.posStart.y) / layout::cellPixels};
        if (TileLayerView::contains(cell.x, cell.y) && selection_.test(cell)) {
            found.push_back(slot);
        }
    }
    return found;
}

bool DocumentTab::alignSelectedObjects(AlignMode mode) {
    if (!requireWritable()) {
        return false;
    }
    // Like every other object edit: only records of a type verified for the
    // chosen game are moved; the others stay exactly as stored.
    auto found = objectsInSelection();
    const auto all = found.size();
    std::erase_if(found, [this](std::size_t slot) { return !canEditObject(slot); });
    const auto skipped = all - found.size();
    EditBuilder builder(document());
    const auto changed = alignObjects(builder, found, mode);
    if (changed == 0 || !builder.hasChanges()) {
        emit statusMessage(
            found.size() < 2
                ? (skipped != 0
                       ? tr("Select an area with at least two objects of a type verified for the "
                            "chosen game first.")
                       : tr("Select an area with at least two objects first."))
                : tr("The objects are already placed that way."));
        return false;
    }
    if (!applyTransaction(builder.build(tr("Align Objects").toStdString()))) {
        return false;
    }
    emit statusMessage(
        skipped != 0
            ? tr("%1 object(s) moved; %2 of a type not verified for the chosen game left as stored")
                  .arg(changed)
                  .arg(skipped)
            : tr("%n object(s) moved", nullptr, static_cast<int>(changed)));
    return true;
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
    if (pendingStamp_) {
        // A refused placement (an object outside the world, say) keeps the
        // stamp armed so another place can be tried.
        stampGhostAt_ = cell;
        if (placeStampAt(*pendingStamp_, cell)) {
            endStampPlacement();
        }
        return;
    }
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
    case Tool::adaptive: {
        gestureKey_ = nextGestureKey_++;
        canvas_->setPreviewCells({});
        const CellPos cells[] = {cell};
        adaptiveAt(cells, gestureKey_);
        break;
    }
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
    case Tool::start:
        placeStartAt(cell);
        break;
    case Tool::object: {
        const Point centre{static_cast<std::int32_t>(cell.x) * layout::cellPixels + 32,
                           static_cast<std::int32_t>(cell.y) * layout::cellPixels + 32};
        const auto objects = document().view().objects();
        grab_ = Grab::none;
        // The end box of the selected mover takes priority, so a path can be
        // reshaped even where other objects overlap it.
        if (selectedSlot_ && objects) {
            const auto record = *objects->record(*selectedSlot_);
            const auto size = spriteBoxSize(record.channel);
            if (!record.isFree() && record.posEnd != record.posStart &&
                centre.x >= record.posEnd.x && centre.x < record.posEnd.x + size &&
                centre.y >= record.posEnd.y && centre.y < record.posEnd.y + size) {
                grab_ = Grab::end;
                break;
            }
        }
        auto hits = objectsAt(document().view(), centre);
        // While the canvas animates, an object is drawn where the animation
        // has taken it (starting from its posStart), so it can be picked
        // there too, and at its start cell.
        if (canvas_->isAnimated() && objects) {
            for (std::size_t slot = 0; slot < objects->slotCount(); ++slot) {
                const auto record = *objects->record(slot);
                if (record.isFree() || std::ranges::find(hits, slot) != hits.end()) {
                    continue;
                }
                const auto size = spriteBoxSize(record.channel);
                const auto inside = [&](Point position) {
                    return centre.x >= position.x && centre.x < position.x + size &&
                           centre.y >= position.y && centre.y < position.y + size;
                };
                if (inside(canvas_->shownPosition(slot)) || inside(record.posStart)) {
                    hits.push_back(slot);
                }
            }
            std::ranges::sort(hits);
        }
        if (hits.empty()) {
            setSelectedSlot(std::nullopt);
            break;
        }
        // Repeated clicks cycle through overlapping records in slot order.
        auto next = hits.front();
        if (selectedSlot_) {
            const auto found = std::ranges::find(hits, *selectedSlot_);
            if (found != hits.end()) {
                next = found + 1 != hits.end() ? *(found + 1) : hits.front();
            }
        }
        setSelectedSlot(next);
        grab_ = Grab::body;
        break;
    }
    case Tool::placeObject:
        canvas_->setPreviewCells({cell});
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
    case Tool::adaptive:
        if (gestureKey_ != 0) {
            // New cells only: the previous sample was already painted.
            auto cells = lineCells(gestureLast_.value_or(cell), cell);
            if (cells.size() > 1) {
                cells.erase(cells.begin());
            }
            adaptiveAt(cells, gestureKey_);
        }
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
    case Tool::object:
        if (grab_ != Grab::none) {
            canvas_->setPreviewCells({cell});
        }
        break;
    case Tool::placeObject: {
        const auto *preset = placePreset();
        canvas_->setPreviewCells(preset != nullptr && preset->hasPath
                                     ? lineCells(*gestureStart_, cell)
                                     : std::vector<CellPos>{*gestureStart_});
        break;
    }
    case Tool::browse:
    case Tool::fill:
    case Tool::start:
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
    if (pendingStamp_) {
        showStampGhost(stampGhostAt_); // still armed after a refused placement
    }
    if (!start || !last) {
        return;
    }
    if (tool_ == Tool::line) {
        drawLine(*start, *last);
    } else if (tool_ == Tool::rectangle) {
        drawRectangle(*start, *last);
    } else if (tool_ == Tool::placeObject) {
        createObjectAt(*start, *last);
    } else if (tool_ == Tool::object) {
        const auto grab = grab_;
        grab_ = Grab::none;
        const Point delta{
            (static_cast<std::int32_t>(last->x) - static_cast<std::int32_t>(start->x)) *
                layout::cellPixels,
            (static_cast<std::int32_t>(last->y) - static_cast<std::int32_t>(start->y)) *
                layout::cellPixels};
        if ((delta.x == 0 && delta.y == 0) || !selectedSlot_) {
            return;
        }
        if (grab == Grab::body) {
            moveSelectedObject(delta);
        } else if (grab == Grab::end) {
            const auto record = *document().view().objects()->record(*selectedSlot_);
            moveSelectedEnd({record.posEnd.x + delta.x, record.posEnd.y + delta.y});
        }
    }
}

bool DocumentTab::applyFix(const DoctorFix &fix) {
    if (!requireWritable() || fix.changes.empty()) {
        return false;
    }
    Transaction transaction;
    transaction.label = fix.label;
    transaction.changes = fix.changes;
    return applyTransaction(std::move(transaction));
}

void DocumentTab::toggleBookmark(std::size_t offset) {
    const auto found = std::ranges::find(bookmarks_, offset);
    if (found != bookmarks_.end()) {
        bookmarks_.erase(found);
    } else {
        bookmarks_.push_back(offset);
        std::ranges::sort(bookmarks_);
    }
}

QString DocumentTab::describeCell(CellPos cell) const {
    const auto view = document().view();
    QStringList parts;
    parts << tr("Cell (%1, %2)").arg(cell.x).arg(cell.y);
    if (file_->isTextWorld()) {
        // A text world has no byte offsets worth showing: its two blocks.
        parts << tr("Decor %1").arg(*view.foreground()->cell(cell.x, cell.y));
        parts << tr("BigDecor %1").arg(*view.bigDecor()->cell(cell.x, cell.y));
        return parts.join(QStringLiteral("   "));
    }
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
        // The port's own reader cuts lines at line feeds and needs a value
        // to be a number to its end (evidence.md, "Mobile Eggbert as a game
        // profile"); whether a carriage return reaches it was not established.
        if (file_->hasWindowsLineEnds()) {
            banner_->setText(tr("This file has Windows line ends (CR LF), which are kept. Mobile "
                                "Eggbert's world reader cuts lines at line feeds only: unless "
                                "the game removes carriage returns before that, it does not read "
                                "the last value of each tile line, the bottom row of the map."));
            banner_->show();
            return;
        }
        banner_->hide();
        return;
    }
    banner_->setText(tr("Read-only: %1").arg(fromView(writeAccessReason(access))));
    banner_->show();
}

} // namespace eggbert
