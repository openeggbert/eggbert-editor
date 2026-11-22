// SPDX-License-Identifier: GPL-3.0-only
#include "png_export_dialog.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace eggbert {

PngExportDialog::PngExportDialog(const RenderOptions &initial,
                                 std::shared_ptr<const GameAssets> available, QWidget *parent)
    : QDialog(parent), available_(std::move(available)) {
    setWindowTitle(tr("Export World PNG"));
    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    layout->addLayout(form);

    scale_ = new QComboBox(this);
    scale_->setObjectName(QStringLiteral("pngScaleCombo"));
    for (const int pixels : {64, 32, 16, 8, 4, 2}) {
        scale_->addItem(pixels == 64 ? tr("Native: 64 pixels per cell")
                                     : tr("%1 pixels per cell").arg(pixels),
                        pixels);
    }
    form->addRow(tr("Size"), scale_);

    const auto addBox = [&](const QString &name, const QString &text, bool checked) {
        auto *box = new QCheckBox(text, this);
        box->setObjectName(name);
        box->setChecked(checked);
        form->addRow(box);
        connect(box, &QCheckBox::toggled, this, [this] { refresh(); });
        return box;
    };
    foreground_ = addBox(QStringLiteral("pngForeground"), tr("Foreground layer"),
                         initial.showForeground);
    bigDecor_ = addBox(QStringLiteral("pngBigDecor"), tr("bigDecor layer"), initial.showBigDecor);
    objects_ = addBox(QStringLiteral("pngObjects"), tr("Objects"), initial.showObjects);
    starts_ = addBox(QStringLiteral("pngStarts"), tr("Player starts"), initial.showStarts);
    paths_ = addBox(QStringLiteral("pngPaths"), tr("Motion paths"), initial.showPaths);
    grid_ = addBox(QStringLiteral("pngGrid"), tr("Grid"), initial.showGrid);
    labels_ = addBox(QStringLiteral("pngLabels"), tr("Value labels (large sizes only)"),
                     initial.showLabels);
    transparent_ = addBox(QStringLiteral("pngTransparent"), tr("Transparent background"), false);
    artwork_ = addBox(QStringLiteral("pngArtwork"), tr("Draw with game artwork"),
                      available_ != nullptr && initial.artwork != nullptr);
    artwork_->setEnabled(available_ != nullptr && available_->usable());
    backdrop_ = addBox(QStringLiteral("pngBackdrop"), tr("Backdrop picture (with game artwork)"),
                       initial.showBackdrop);
    const auto syncBackdrop = [this] {
        backdrop_->setEnabled(artwork_->isEnabled() && artwork_->isChecked() &&
                              !transparent_->isChecked());
    };
    connect(artwork_, &QCheckBox::toggled, this, syncBackdrop);
    connect(transparent_, &QCheckBox::toggled, this, syncBackdrop);
    syncBackdrop();

    estimate_ = new QLabel(this);
    estimate_->setObjectName(QStringLiteral("pngEstimate"));
    estimate_->setWordWrap(true);
    layout->addWidget(estimate_);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    connect(scale_, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    refresh();
}

PngExportOptions PngExportDialog::options() const {
    PngExportOptions options;
    options.pixelsPerCell = scale_->currentData().toInt();
    options.render.showForeground = foreground_->isChecked();
    options.render.showBigDecor = bigDecor_->isChecked();
    options.render.showObjects = objects_->isChecked();
    options.render.showStarts = starts_->isChecked();
    options.render.showPaths = paths_->isChecked();
    options.render.showGrid = grid_->isChecked();
    options.render.showLabels = labels_->isChecked();
    options.render.transparentBackground = transparent_->isChecked();
    options.render.showBackdrop = backdrop_->isChecked();
    if (artwork_->isChecked() && artwork_->isEnabled()) {
        options.render.artwork = available_;
    }
    return options;
}

QString PngExportDialog::estimateText() const {
    return estimate_->text();
}

void PngExportDialog::refresh() {
    const auto estimate = estimatePngExport(options());
    auto text = tr("The image will be %1 x %2 pixels and needs about %3 MiB of memory while it "
                   "is written.")
                    .arg(estimate.width)
                    .arg(estimate.height)
                    .arg(QString::number(
                        static_cast<double>(estimate.imageBytes) / (1024.0 * 1024.0), 'f', 1));
    text += QLatin1Char(' ') +
            (options().render.artwork != nullptr
                 ? tr("It will contain artwork from your game directory. That artwork is "
                      "copyrighted by its owners: keep such pictures for your own use unless you "
                      "have permission to share them.")
                 : tr("It is drawn with the schematic renderer, which uses no game artwork."));
    estimate_->setText(text);
}

} // namespace eggbert
