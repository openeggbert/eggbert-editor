// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QDialog>

#include <optional>

#include "world/metadata.hpp"
#include "world/profile.hpp"

class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QTabWidget;

namespace eggbert {

// Edits the verified header fields of one document. It only collects values;
// the caller applies them as one transaction.
class MetadataDialog final : public QDialog {
    Q_OBJECT

public:
    enum Page { worldPage = 0, startsPage = 1 };

    MetadataDialog(const WorldView &world, GameProfile profile, QWidget *parent = nullptr);

    void setPage(Page page);
    // Experimental editing: music and region numbers the chosen game is not
    // known to offer are accepted as typed (and said to be so).
    void setExperimental(bool experimental);
    // A Mobile Eggbert world: no title, and one start (the other three rows
    // follow the first and cannot be edited).
    void setTextWorld(bool textWorld);
    // The edited values, or nullopt while something cannot be stored.
    [[nodiscard]] std::optional<Metadata> metadata() const;
    [[nodiscard]] bool isValid() const { return metadata().has_value(); }
    // Current problems and warnings, one per line.
    [[nodiscard]] QString messages() const;

private:
    void refresh();

    Metadata original_;
    GameProfile profile_;
    bool titleRead_ = true;
    bool experimental_ = false;
    bool textWorld_ = false;
    QTabWidget *pages_ = nullptr;
    QLineEdit *name_ = nullptr;
    QSpinBox *music_ = nullptr;
    QSpinBox *region_ = nullptr;
    QCheckBox *scrollX_ = nullptr;
    QCheckBox *scrollY_ = nullptr;
    QSpinBox *posX_ = nullptr;
    QSpinBox *posY_ = nullptr;
    QSpinBox *startX_[4] = {};
    QSpinBox *startY_[4] = {};
    QComboBox *startDirection_[4] = {};
    QLabel *messages_ = nullptr;
    QDialogButtonBox *buttons_ = nullptr;
};

[[nodiscard]] QString decodeTitle(std::span<const std::byte> bytes);
[[nodiscard]] std::optional<std::vector<std::byte>> encodeTitle(const QString &text);

} // namespace eggbert
