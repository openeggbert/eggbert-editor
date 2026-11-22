// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDialog>

#include "app/png_export.hpp"

class QCheckBox;
class QComboBox;
class QLabel;

namespace eggbert {

// Collects PNG export settings and shows the resulting dimensions and
// memory estimate before anything is rendered.
class PngExportDialog final : public QDialog {
    Q_OBJECT

public:
    explicit PngExportDialog(const RenderOptions &initial, QWidget *parent = nullptr);

    [[nodiscard]] PngExportOptions options() const;
    [[nodiscard]] QString estimateText() const;

private:
    void refresh();

    QComboBox *scale_ = nullptr;
    QCheckBox *foreground_ = nullptr;
    QCheckBox *bigDecor_ = nullptr;
    QCheckBox *objects_ = nullptr;
    QCheckBox *starts_ = nullptr;
    QCheckBox *paths_ = nullptr;
    QCheckBox *grid_ = nullptr;
    QCheckBox *labels_ = nullptr;
    QCheckBox *transparent_ = nullptr;
    QLabel *estimate_ = nullptr;
};

} // namespace eggbert
