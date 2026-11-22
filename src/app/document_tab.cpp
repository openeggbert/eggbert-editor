// SPDX-License-Identifier: GPL-3.0-only
#include "document_tab.hpp"

#include <QFileInfo>
#include <QLabel>
#include <QVBoxLayout>

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

    connect(canvas_, &WorldCanvas::toolPressed, this, [this](CellPos cell) {
        if (tool_ != Tool::pencil) {
            return;
        }
        gestureKey_ = nextGestureKey_++;
        pencilAt(cell, gestureKey_);
    });
    connect(canvas_, &WorldCanvas::toolDragged, this, [this](CellPos cell) {
        if (tool_ == Tool::pencil && gestureKey_ != 0) {
            pencilAt(cell, gestureKey_);
        }
    });
    connect(canvas_, &WorldCanvas::toolReleased, this, [this] {
        gestureKey_ = 0;
        file_->document().endMerge();
    });

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
    updateBanner();
    emit toolStateChanged();
    notifyDocumentChanged();
}

bool DocumentTab::pencilAt(CellPos cell, std::uint64_t mergeKey) {
    if (!document().isWritable()) {
        emit statusMessage(fromView(writeAccessReason(document().writeAccess())));
        return false;
    }
    EditBuilder builder(document());
    if (!builder.setCell(activeLayer_, cell.x, cell.y, tileId_)) {
        return false;
    }
    if (!builder.hasChanges()) {
        return false;
    }
    return applyTransaction(builder.build(tr("Pencil").toStdString(), mergeKey));
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
