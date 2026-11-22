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
    // `available` is the artwork that may be used for this document (null
    // when none is loaded for its game).
    explicit PngExportDialog(const RenderOptions &initial,
                             std::shared_ptr<const GameAssets> available = nullptr,
                             QWidget *parent = nullptr);

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
    QCheckBox *artwork_ = nullptr;
    QCheckBox *backdrop_ = nullptr;
    std::shared_ptr<const GameAssets> available_;
    QLabel *estimate_ = nullptr;
};

} // namespace eggbert
