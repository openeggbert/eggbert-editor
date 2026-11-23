// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QWidget>

#include <functional>
#include <memory>

#include "world/text_world_document.hpp"

class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTableWidget;

namespace eggbert {

class TextWorldCanvas;

// A window that edits one Mobile Eggbert text world: tiles of both blocks,
// the header values and the numbers of each object. Every edit replaces one
// token of the file and is one undo step; saving writes the text through a
// temporary file and refuses when the file changed on disk meanwhile.
//
// It is deliberately separate from the BLP editor: no catalogs, pictures or
// rules of the Windows games are applied, tiles are shown as numbered
// colours, and nothing is converted.
class TextWorldEditor final : public QWidget {
    Q_OBJECT

public:
    // Null, with `error` set, when the file cannot be read.
    static TextWorldEditor *open(const QString &path, QWidget *parent, QString *error);

    [[nodiscard]] const QString &path() const { return path_; }
    [[nodiscard]] const TextWorldDocument &document() const { return *document_; }

    // The layer and tile number the canvas paints with.
    [[nodiscard]] QComboBox *layerCombo() const { return layer_; }
    [[nodiscard]] QSpinBox *valueSpin() const { return value_; }
    [[nodiscard]] QTableWidget *objectTable() const { return objects_; }
    [[nodiscard]] QListWidget *problemList() const { return problems_; }
    [[nodiscard]] QPushButton *saveButton() const { return save_; }
    [[nodiscard]] QPushButton *undoButton() const { return undo_; }
    [[nodiscard]] QPushButton *redoButton() const { return redo_; }
    [[nodiscard]] TextWorldCanvas *canvas() const { return canvas_; }

    // What a click on the canvas does: paint the chosen number into a cell
    // of the chosen layer, or (right button) pick the cell's number.
    bool paintCell(std::size_t x, std::size_t y);
    void pickCell(std::size_t x, std::size_t y);
    bool save();
    bool saveAs(const QString &path);

    // Replaceable for tests.
    std::function<void(const QString &)> showError;
    std::function<bool()> confirmDiscard;
    std::function<QString(const QString &suggested)> saveAsPath;

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    TextWorldEditor(QString path, std::string text, QWidget *parent);
    void refresh();
    [[nodiscard]] SectionKind currentLayer() const;
    bool write(const QString &path, bool checkDisk);

    QString path_;
    std::unique_ptr<TextWorldDocument> document_;
    // The text as last read from or written to the file.
    std::string disk_;
    bool refreshing_ = false;
    TextWorldCanvas *canvas_ = nullptr;
    QComboBox *layer_ = nullptr;
    QSpinBox *value_ = nullptr;
    QTableWidget *objects_ = nullptr;
    QListWidget *problems_ = nullptr;
    QLabel *status_ = nullptr;
    QPushButton *save_ = nullptr;
    QPushButton *undo_ = nullptr;
    QPushButton *redo_ = nullptr;
    QSpinBox *music_ = nullptr;
    QSpinBox *region_ = nullptr;
    QSpinBox *direction_ = nullptr;
    QSpinBox *startX_ = nullptr;
    QSpinBox *startY_ = nullptr;
};

// The map: Decor cells as coloured squares (the number inside when there is
// room), BigDecor cells as outlines, objects as circles with their type and
// the start as a diamond.
class TextWorldCanvas final : public QWidget {
    Q_OBJECT

public:
    explicit TextWorldCanvas(QWidget *parent = nullptr);
    void setWorld(const TextWorld *world);
    void setCellSize(int pixels);
    [[nodiscard]] int cellSize() const { return cell_; }

signals:
    void cellClicked(int x, int y, bool pick);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    const TextWorld *world_ = nullptr;
    int cell_ = 12;
};

} // namespace eggbert
