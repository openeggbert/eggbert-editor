// SPDX-License-Identifier: GPL-3.0-only
#include "app/text_world_editor.hpp"
#include "app/document_file.hpp"

#include <QCloseEvent>
#include <QComboBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QShortcut>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include "app/core_texts.hpp"
namespace eggbert {

namespace {

constexpr qint64 largestFile = 4 * 1024 * 1024;

struct ObjectColumn {
    const char *field;
    // 0 a single value, 1 the x of a point, 2 its y.
    int part;
    const char *title;
};

constexpr ObjectColumn objectColumns[] = {
    {"type", 0, "type"},           {"stepAdvance", 0, "stepAdvance"},
    {"stepRecede", 0, "stepRecede"}, {"timeStopStart", 0, "timeStopStart"},
    {"timeStopEnd", 0, "timeStopEnd"}, {"posStart", 1, "posStart x"},
    {"posStart", 2, "posStart y"}, {"posEnd", 1, "posEnd x"},
    {"posEnd", 2, "posEnd y"},     {"channel", 0, "channel"},
    {"icon", 0, "icon"},
};

QColor tileColor(long long value) {
    return QColor::fromHsv(static_cast<int>((value * 47) % 360), 150, 200);
}

std::optional<std::string> readText(const QString &path) {
    QFile file(path);
    if (file.size() > largestFile || !file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    const auto bytes = file.readAll();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}

} // namespace

TextWorldCanvas::TextWorldCanvas(QWidget *parent) : QWidget(parent) {
    setCellSize(cell_);
    setMouseTracking(false);
    setAccessibleName(tr("Map of the text world"));
}

void TextWorldCanvas::setWorld(const TextWorld *world) {
    world_ = world;
    update();
}

void TextWorldCanvas::setCellSize(int pixels) {
    cell_ = std::clamp(pixels, 4, 48);
    setFixedSize(static_cast<int>(layout::mapWidth) * cell_,
                 static_cast<int>(layout::mapHeight) * cell_);
    update();
}

void TextWorldCanvas::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    painter.fillRect(event->rect(), QColor(24, 26, 32));
    if (world_ == nullptr) {
        return;
    }
    const int first = std::max(0, event->rect().left() / cell_);
    const int last = std::min(static_cast<int>(layout::mapWidth) - 1, event->rect().right() / cell_);
    auto font = painter.font();
    font.setPixelSize(std::max(6, cell_ / 2));
    painter.setFont(font);
    for (int x = first; x <= last; ++x) {
        for (int y = 0; y < static_cast<int>(layout::mapHeight); ++y) {
            const QRect box(x * cell_, y * cell_, cell_, cell_);
            const auto back = world_->tile(SectionKind::bigDecor, static_cast<std::size_t>(x),
                                           static_cast<std::size_t>(y));
            const auto tile = world_->tile(SectionKind::foreground, static_cast<std::size_t>(x),
                                           static_cast<std::size_t>(y));
            if (tile && *tile != -1) {
                painter.fillRect(box.adjusted(0, 0, -1, -1), tileColor(*tile));
                if (cell_ >= 20) {
                    painter.setPen(Qt::black);
                    painter.drawText(box, Qt::AlignCenter, QString::number(*tile));
                }
            }
            if (back && *back != -1) {
                painter.setPen(QPen(tileColor(*back).lighter(140), 1, Qt::DashLine));
                painter.drawRect(box.adjusted(1, 1, -2, -2));
            }
        }
    }
    painter.setRenderHint(QPainter::Antialiasing);
    const auto mark = [&](Point position, const QString &label, const QColor &color, bool diamond) {
        const QPointF centre((position.x + 32.0) * cell_ / 64.0, (position.y + 32.0) * cell_ / 64.0);
        const double radius = std::max(3.0, cell_ * 0.45);
        painter.setPen(QPen(Qt::white, 1));
        painter.setBrush(color);
        if (diamond) {
            const QPointF points[] = {centre + QPointF(0, -radius), centre + QPointF(radius, 0),
                                      centre + QPointF(0, radius), centre + QPointF(-radius, 0)};
            painter.drawPolygon(points, 4);
        } else {
            painter.drawEllipse(centre, radius, radius);
        }
        if (cell_ >= 16) {
            painter.setPen(Qt::black);
            painter.drawText(QRectF(centre.x() - radius, centre.y() - radius, 2 * radius, 2 * radius),
                             Qt::AlignCenter, label);
        }
    };
    for (std::size_t index = 0; index < world_->objectCount(); ++index) {
        const auto start = world_->objectPoint(index, "posStart");
        const auto type = world_->objectValue(index, "type");
        if (start) {
            mark(*start, type ? QString::number(*type) : QStringLiteral("?"), QColor(255, 200, 60),
                 false);
        }
    }
    if (const auto start = world_->headerPoint("blupiPos")) {
        mark(*start, QStringLiteral("S"), QColor(120, 220, 255), true);
    }
    // What a drag in progress would write, and the marked rectangle.
    painter.setRenderHint(QPainter::Antialiasing, false);
    for (const auto &cell : preview_) {
        painter.fillRect(QRect(static_cast<int>(cell.x) * cell_, static_cast<int>(cell.y) * cell_,
                               cell_, cell_),
                         QColor(255, 255, 255, 110));
    }
    if (selection_) {
        const QRect box(static_cast<int>(selection_->min.x) * cell_,
                        static_cast<int>(selection_->min.y) * cell_,
                        static_cast<int>(selection_->max.x - selection_->min.x + 1) * cell_,
                        static_cast<int>(selection_->max.y - selection_->min.y + 1) * cell_);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::black, 3));
        painter.drawRect(box.adjusted(1, 1, -2, -2));
        painter.setPen(QPen(Qt::white, 1, Qt::DashLine));
        painter.drawRect(box.adjusted(1, 1, -2, -2));
    }
}

void TextWorldCanvas::setPreview(std::vector<CellPos> cells) {
    preview_ = std::move(cells);
    update();
}

void TextWorldCanvas::setSelection(std::optional<CellRect> selection) {
    selection_ = selection;
    update();
}

void TextWorldCanvas::mousePressEvent(QMouseEvent *event) {
    const auto position = event->position().toPoint();
    const int x = position.x() / cell_;
    const int y = position.y() / cell_;
    if (x >= 0 && y >= 0 && x < static_cast<int>(layout::mapWidth) &&
        y < static_cast<int>(layout::mapHeight)) {
        lastDragged_ = CellPos{static_cast<std::size_t>(x), static_cast<std::size_t>(y)};
        emit cellPressed(x, y, event->button() == Qt::RightButton);
    }
}

void TextWorldCanvas::mouseReleaseEvent(QMouseEvent *event) {
    if (event->button() == Qt::LeftButton) {
        lastDragged_.reset();
        emit dragReleased();
    }
}

void TextWorldCanvas::mouseMoveEvent(QMouseEvent *event) {
    // Each further cell a drag with the left button reaches.
    if ((event->buttons() & Qt::LeftButton) != 0) {
        const auto position = event->position().toPoint();
        const int x = position.x() / cell_;
        const int y = position.y() / cell_;
        if (x >= 0 && y >= 0 && x < static_cast<int>(layout::mapWidth) &&
            y < static_cast<int>(layout::mapHeight)) {
            const CellPos cell{static_cast<std::size_t>(x), static_cast<std::size_t>(y)};
            if (lastDragged_ != cell) {
                lastDragged_ = cell;
                emit cellDragged(x, y);
            }
        }
    }
}

TextWorldEditor *TextWorldEditor::open(const QString &path, QWidget *parent, QString *error) {
    auto text = readText(path);
    if (!text) {
        if (error != nullptr) {
            *error = tr("%1 cannot be read (or is larger than 4 MiB).").arg(path);
        }
        return nullptr;
    }
    return new TextWorldEditor(path, std::move(*text), parent);
}

TextWorldEditor::TextWorldEditor(QString path, std::string text, QWidget *parent)
    : QWidget(parent, Qt::Window), path_(std::move(path)),
      document_(std::make_unique<TextWorldDocument>(text)), disk_(std::move(text)) {
    setAttribute(Qt::WA_DeleteOnClose);
    setObjectName(QStringLiteral("textWorldEditor"));
    showError = [this](const QString &message) {
        QMessageBox::warning(this, tr("Mobile Eggbert Text World"), message);
    };
    confirmDiscard = [this] {
        return QMessageBox::question(this, tr("Mobile Eggbert Text World"),
                                     tr("%1 has unsaved changes. Close without saving?")
                                         .arg(QFileInfo(path_).fileName())) == QMessageBox::Yes;
    };
    saveAsPath = [this](const QString &suggested) {
        return QFileDialog::getSaveFileName(this, tr("Save Text World As"), suggested,
                                            tr("Text worlds (*.txt);;All files (*)"));
    };

    auto *layout = new QVBoxLayout(this);
    auto *bar = new QHBoxLayout;
    save_ = new QPushButton(tr("Save"), this);
    save_->setObjectName(QStringLiteral("textWorldSave"));
    auto *saveAsButton = new QPushButton(tr("Save As..."), this);
    undo_ = new QPushButton(tr("Undo"), this);
    undo_->setObjectName(QStringLiteral("textWorldUndo"));
    redo_ = new QPushButton(tr("Redo"), this);
    redo_->setObjectName(QStringLiteral("textWorldRedo"));
    layer_ = new QComboBox(this);
    layer_->setObjectName(QStringLiteral("textWorldLayer"));
    layer_->setAccessibleName(tr("Block the map paints into"));
    layer_->addItem(QStringLiteral("Decor"), static_cast<int>(SectionKind::foreground));
    layer_->addItem(QStringLiteral("BigDecor"), static_cast<int>(SectionKind::bigDecor));
    value_ = new QSpinBox(this);
    value_->setObjectName(QStringLiteral("textWorldValue"));
    value_->setAccessibleName(tr("Tile number to paint (-1 empties the cell)"));
    value_->setRange(static_cast<int>(TextWorldDocument::smallestTile),
                     static_cast<int>(TextWorldDocument::largestTile));
    value_->setPrefix(tr("Tile "));
    auto *zoom = new QSpinBox(this);
    zoom->setAccessibleName(tr("Cell size in pixels"));
    zoom->setRange(4, 48);
    zoom->setSuffix(tr(" px"));
    tool_ = new QComboBox(this);
    tool_->setObjectName(QStringLiteral("textWorldTool"));
    tool_->setAccessibleName(tr("What a drag on the map does"));
    tool_->addItem(tr("Paint"), static_cast<int>(TextTool::paint));
    tool_->addItem(tr("Line"), static_cast<int>(TextTool::line));
    tool_->addItem(tr("Rectangle"), static_cast<int>(TextTool::rectangle));
    tool_->addItem(tr("Select"), static_cast<int>(TextTool::select));
    fillSelection_ = new QPushButton(tr("Fill Selection"), this);
    fillSelection_->setObjectName(QStringLiteral("textWorldFillSelection"));
    fillSelection_->setToolTip(tr("Writes the tile number into every cell of the marked "
                                  "rectangle of the chosen block (-1 empties them)."));
    fillSelection_->setEnabled(false);
    for (auto *widget : {static_cast<QWidget *>(save_), static_cast<QWidget *>(saveAsButton),
                         static_cast<QWidget *>(undo_), static_cast<QWidget *>(redo_),
                         static_cast<QWidget *>(layer_), static_cast<QWidget *>(value_),
                         static_cast<QWidget *>(tool_), static_cast<QWidget *>(fillSelection_),
                         static_cast<QWidget *>(zoom)}) {
        bar->addWidget(widget);
    }
    bar->addStretch(1);
    layout->addLayout(bar);
    auto *hint = new QLabel(
        tr("Click or drag on the map to write the tile number into the chosen block; right-click "
           "picks a cell's number. Line and Rectangle write on release; Select marks a "
           "rectangle for Fill Selection. Tiles are shown as numbered colours: no pictures or "
           "rules of the Windows games are applied, and nothing is converted."),
        this);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *columns = new QHBoxLayout;
    canvas_ = new TextWorldCanvas(this);
    zoom->setValue(canvas_->cellSize());
    auto *scroll = new QScrollArea(this);
    scroll->setWidget(canvas_);
    columns->addWidget(scroll, 3);

    auto *tabs = new QTabWidget(this);
    auto *worldPage = new QWidget(tabs);
    auto *form = new QFormLayout(worldPage);
    const auto spin = [&](const QString &name, const QString &label) {
        auto *box = new QSpinBox(worldPage);
        box->setObjectName(name);
        box->setAccessibleName(label);
        box->setRange(-1'000'000, 1'000'000);
        box->setKeyboardTracking(false);
        form->addRow(label, box);
        return box;
    };
    region_ = spin(QStringLiteral("textWorldRegion"), QStringLiteral("region"));
    direction_ = spin(QStringLiteral("textWorldDirection"), QStringLiteral("blupiDir"));
    startX_ = spin(QStringLiteral("textWorldStartX"), QStringLiteral("blupiPos x"));
    startY_ = spin(QStringLiteral("textWorldStartY"), QStringLiteral("blupiPos y"));
    tabs->addTab(worldPage, tr("World"));
    auto *objectPage = new QWidget(tabs);
    auto *objectLayout = new QVBoxLayout(objectPage);
    objectLayout->setContentsMargins(0, 0, 0, 0);
    objects_ = new QTableWidget(objectPage);
    objects_->setObjectName(QStringLiteral("textWorldObjects"));
    objects_->setAccessibleName(tr("Objects of the text world"));
    objects_->setColumnCount(static_cast<int>(std::size(objectColumns)));
    QStringList titles;
    for (const auto &column : objectColumns) {
        titles << QString::fromLatin1(column.title);
    }
    objects_->setHorizontalHeaderLabels(titles);
    objects_->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    objects_->setSelectionBehavior(QAbstractItemView::SelectRows);
    objects_->setSelectionMode(QAbstractItemView::SingleSelection);
    objectLayout->addWidget(objects_, 1);
    auto *objectButtons = new QHBoxLayout;
    addObject_ = new QPushButton(tr("&Add Object"), objectPage);
    addObject_->setObjectName(QStringLiteral("textWorldAddObject"));
    addObject_->setToolTip(tr("Appends an object line with type 1 at the start position; give "
                              "it its numbers in the table."));
    duplicateObject_ = new QPushButton(tr("&Duplicate"), objectPage);
    duplicateObject_->setObjectName(QStringLiteral("textWorldDuplicateObject"));
    duplicateObject_->setToolTip(tr("Inserts an exact copy of the chosen object's line after it."));
    removeObject_ = new QPushButton(tr("&Remove"), objectPage);
    removeObject_->setObjectName(QStringLiteral("textWorldRemoveObject"));
    removeObject_->setToolTip(tr("Removes the chosen object's line; the objects after it move "
                                 "up by one."));
    objectButtons->addWidget(addObject_);
    objectButtons->addWidget(duplicateObject_);
    objectButtons->addWidget(removeObject_);
    objectButtons->addStretch(1);
    objectLayout->addLayout(objectButtons);
    tabs->addTab(objectPage, tr("Objects"));
    connect(addObject_, &QPushButton::clicked, this, [this] { addObject(); });
    connect(duplicateObject_, &QPushButton::clicked, this, [this] { duplicateSelectedObject(); });
    connect(removeObject_, &QPushButton::clicked, this, [this] { removeSelectedObject(); });
    connect(objects_, &QTableWidget::currentCellChanged, this, [this] {
        const bool chosen = objects_->currentRow() >= 0;
        duplicateObject_->setEnabled(
            chosen && document_->world().objectCount() < textWorldMostObjects);
        removeObject_->setEnabled(chosen);
    });
    problems_ = new QListWidget(tabs);
    problems_->setObjectName(QStringLiteral("textWorldProblems"));
    problems_->setAccessibleName(tr("Shape problems of the file"));
    problems_->setWordWrap(true);
    tabs->addTab(problems_, tr("Problems"));
    columns->addWidget(tabs, 2);
    layout->addLayout(columns, 1);
    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("textWorldStatus"));
    layout->addWidget(status_);

    connect(save_, &QPushButton::clicked, this, [this] { save(); });
    connect(saveAsButton, &QPushButton::clicked, this, [this] {
        const auto target = saveAsPath(path_);
        if (!target.isEmpty()) {
            saveAs(target);
        }
    });
    connect(undo_, &QPushButton::clicked, this, [this] {
        if (document_->undo()) {
            refresh();
        }
    });
    connect(redo_, &QPushButton::clicked, this, [this] {
        if (document_->redo()) {
            refresh();
        }
    });
    connect(zoom, &QSpinBox::valueChanged, canvas_, &TextWorldCanvas::setCellSize);
    connect(canvas_, &TextWorldCanvas::cellPressed, this, [this](int x, int y, bool pick) {
        const CellPos cell{static_cast<std::size_t>(x), static_cast<std::size_t>(y)};
        if (pick) {
            pickCell(cell.x, cell.y);
            return;
        }
        dragStart_ = cell;
        dragLast_ = cell;
        switch (tool()) {
        case TextTool::paint:
            paintCell(cell.x, cell.y);
            break;
        case TextTool::line:
        case TextTool::rectangle:
            canvas_->setPreview({cell});
            break;
        case TextTool::select:
            setSelection(CellRect{cell, cell});
            break;
        }
    });
    connect(canvas_, &TextWorldCanvas::cellDragged, this, [this](int x, int y) {
        if (!dragStart_) {
            return;
        }
        const CellPos cell{static_cast<std::size_t>(x), static_cast<std::size_t>(y)};
        switch (tool()) {
        case TextTool::paint:
            // Every cell between two samples, so a fast drag leaves no gaps.
            for (const auto &between : lineCells(dragLast_.value_or(cell), cell)) {
                paintCell(between.x, between.y);
            }
            break;
        case TextTool::line:
            canvas_->setPreview(lineCells(*dragStart_, cell));
            break;
        case TextTool::rectangle:
            canvas_->setPreview(CellMask::fromRect(rectFromCorners(*dragStart_, cell)).cells());
            break;
        case TextTool::select:
            setSelection(rectFromCorners(*dragStart_, cell));
            break;
        }
        dragLast_ = cell;
    });
    connect(canvas_, &TextWorldCanvas::dragReleased, this, [this] {
        const auto start = dragStart_;
        const auto last = dragLast_;
        dragStart_.reset();
        dragLast_.reset();
        canvas_->setPreview({});
        if (!start || !last) {
            return;
        }
        if (tool() == TextTool::line) {
            drawLine(*start, *last);
        } else if (tool() == TextTool::rectangle) {
            drawRectangle(*start, *last);
        }
    });
    connect(fillSelection_, &QPushButton::clicked, this, [this] { fillSelection(); });
    const auto header = [this](QSpinBox *box, const char *name) {
        connect(box, &QSpinBox::valueChanged, this, [this, name](int value) {
            if (!refreshing_ && document_->setHeaderValue(name, value)) {
                refresh();
            }
        });
    };
    header(region_, "region");
    header(direction_, "blupiDir");
    const auto startChanged = [this] {
        if (!refreshing_ && document_->setHeaderPoint("blupiPos", {startX_->value(), startY_->value()})) {
            refresh();
        }
    };
    connect(startX_, &QSpinBox::valueChanged, this, startChanged);
    connect(startY_, &QSpinBox::valueChanged, this, startChanged);
    connect(objects_, &QTableWidget::cellChanged, this, [this](int row, int column) {
        if (refreshing_) {
            return;
        }
        const auto &what = objectColumns[column];
        bool ok = false;
        const auto value = objects_->item(row, column)->text().trimmed().toInt(&ok);
        const auto index = static_cast<std::size_t>(row);
        if (ok && what.part == 0) {
            document_->setObjectValue(index, what.field, value);
        } else if (ok) {
            if (auto point = document_->world().objectPoint(index, what.field)) {
                (what.part == 1 ? point->x : point->y) = value;
                document_->setObjectPoint(index, what.field, *point);
            }
        }
        refresh(); // also puts back the stored number after a rejected entry
    });
    new QShortcut(QKeySequence::Save, this, [this] { save(); });
    new QShortcut(QKeySequence::Undo, this, [this] { undo_->click(); });
    new QShortcut(QKeySequence::Redo, this, [this] { redo_->click(); });
    resize(1200, 800);
    refresh();
}

SectionKind TextWorldEditor::currentLayer() const {
    return static_cast<SectionKind>(layer_->currentData().toInt());
}

TextTool TextWorldEditor::tool() const {
    return static_cast<TextTool>(tool_->currentData().toInt());
}

void TextWorldEditor::setTool(TextTool tool) {
    tool_->setCurrentIndex(tool_->findData(static_cast<int>(tool)));
}

bool TextWorldEditor::drawLine(CellPos from, CellPos to) {
    const auto cells = lineCells(from, to);
    if (!document_->setTiles(currentLayer(), cells, value_->value())) {
        return false;
    }
    refresh();
    return true;
}

bool TextWorldEditor::drawRectangle(CellPos first, CellPos second) {
    const auto cells = CellMask::fromRect(rectFromCorners(first, second)).cells();
    if (!document_->setTiles(currentLayer(), cells, value_->value())) {
        return false;
    }
    refresh();
    return true;
}

void TextWorldEditor::setSelection(std::optional<CellRect> selection) {
    selection_ = selection;
    canvas_->setSelection(selection_);
    fillSelection_->setEnabled(selection_.has_value());
}

bool TextWorldEditor::fillSelection() {
    return selection_ && drawRectangle(selection_->min, selection_->max);
}

bool TextWorldEditor::paintCell(std::size_t x, std::size_t y) {
    if (!document_->setTile(currentLayer(), x, y, value_->value())) {
        return false;
    }
    refresh();
    return true;
}

void TextWorldEditor::pickCell(std::size_t x, std::size_t y) {
    if (const auto stored = document_->world().tile(currentLayer(), x, y)) {
        value_->setValue(static_cast<int>(*stored));
    }
}

bool TextWorldEditor::write(const QString &path, bool checkDisk) {
    if (checkDisk) {
        // Something else changed or removed the file since it was read.
        const auto now = readText(path);
        if (!now || *now != disk_) {
            showError(tr("%1 was changed or removed on disk since it was opened here. Nothing was "
                         "written; use Save As to keep your version under another name.")
                          .arg(path));
            return false;
        }
    }
    const auto text = document_->text();
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) ||
        file.write(text.data(), static_cast<qint64>(text.size())) !=
            static_cast<qint64>(text.size()) ||
        !file.commit()) {
        showError(tr("%1 could not be written; the file is as it was.").arg(path));
        return false;
    }
    path_ = path;
    disk_ = text;
    document_->markSaved();
    refresh();
    return true;
}

bool TextWorldEditor::addObject() {
    const auto start = document_->world().headerPoint("blupiPos").value_or(Point{0, 0});
    if (!document_->addObject(1, start, 0, 0)) {
        status_->setText(tr("No object was added: the game reads at most %1 object lines.")
                             .arg(textWorldMostObjects));
        return false;
    }
    refresh();
    objects_->setCurrentCell(objects_->rowCount() - 1, 0);
    status_->setText(tr("Object %1 added with type 1 at the start position. Set its numbers in "
                        "the table.")
                         .arg(objects_->rowCount()));
    return true;
}

bool TextWorldEditor::duplicateSelectedObject() {
    const auto row = objects_->currentRow();
    if (row < 0 || !document_->duplicateObject(static_cast<std::size_t>(row))) {
        return false;
    }
    refresh();
    objects_->setCurrentCell(row + 1, 0);
    status_->setText(tr("Object %1 copied as object %2.").arg(row + 1).arg(row + 2));
    return true;
}

bool TextWorldEditor::removeSelectedObject() {
    const auto row = objects_->currentRow();
    if (row < 0 || !document_->removeObject(static_cast<std::size_t>(row))) {
        return false;
    }
    refresh();
    if (objects_->rowCount() > 0) {
        objects_->setCurrentCell(std::min(row, objects_->rowCount() - 1), 0);
    }
    status_->setText(tr("Object %1 removed; the objects after it moved up by one.").arg(row + 1));
    return true;
}

bool TextWorldEditor::save() {
    return document_->isModified() && write(path_, true);
}

bool TextWorldEditor::saveAs(const QString &path) {
    return (!canSaveAs || canSaveAs(path)) && write(path, sameWorldPath(path, path_));
}

void TextWorldEditor::closeEvent(QCloseEvent *event) {
    if (document_->isModified() && !confirmDiscard()) {
        event->ignore();
        return;
    }
    event->accept();
}

void TextWorldEditor::refresh() {
    refreshing_ = true;
    const auto &world = document_->world();
    canvas_->setWorld(&world);
    setWindowTitle(tr("%1%2 - Mobile Eggbert text world")
                       .arg(QFileInfo(path_).fileName(),
                            document_->isModified() ? QStringLiteral(" *") : QString()));
    save_->setEnabled(document_->isModified());
    undo_->setEnabled(document_->canUndo());
    redo_->setEnabled(document_->canRedo());
    undo_->setToolTip(coreText(document_->undoLabel()));
    const auto show = [&](QSpinBox *box, std::optional<long long> value) {
        box->setEnabled(value.has_value());
        box->setValue(value ? static_cast<int>(*value) : 0);
    };
    show(region_, world.headerValue("region"));
    show(direction_, world.headerValue("blupiDir"));
    const auto start = world.headerPoint("blupiPos");
    show(startX_, start ? std::optional<long long>(start->x) : std::nullopt);
    show(startY_, start ? std::optional<long long>(start->y) : std::nullopt);

    const auto chosenRow = objects_->currentRow();
    objects_->setRowCount(static_cast<int>(world.objectCount()));
    for (std::size_t index = 0; index < world.objectCount(); ++index) {
        for (int column = 0; column < static_cast<int>(std::size(objectColumns)); ++column) {
            const auto &what = objectColumns[column];
            std::optional<long long> value;
            if (what.part == 0) {
                value = world.objectValue(index, what.field);
            } else if (const auto point = world.objectPoint(index, what.field)) {
                value = what.part == 1 ? point->x : point->y;
            }
            auto *item = new QTableWidgetItem(value ? QString::number(*value) : QString());
            // A field the file does not have cannot be typed into existence.
            item->setFlags(value ? item->flags() | Qt::ItemIsEditable
                                 : item->flags() & ~Qt::ItemIsEditable);
            objects_->setItem(static_cast<int>(index), column, item);
        }
    }
    // Filling the table must not lose the chosen row.
    if (chosenRow >= 0 && chosenRow < objects_->rowCount() && objects_->currentRow() != chosenRow) {
        objects_->setCurrentCell(chosenRow, 0);
    }
    const bool full = world.objectCount() >= textWorldMostObjects;
    addObject_->setEnabled(!full);
    duplicateObject_->setEnabled(!full && objects_->currentRow() >= 0);
    removeObject_->setEnabled(objects_->currentRow() >= 0);
    problems_->clear();
    const auto found = world.problems();
    for (const auto &problem : found) {
        problems_->addItem(coreText(problem));
    }
    if (found.empty()) {
        problems_->addItem(tr("The file has the expected shape."));
    }
    status_->setText(tr("%1 object(s), %2 shape problem(s)%3")
                         .arg(world.objectCount())
                         .arg(found.size())
                         .arg(document_->isModified() ? tr(" - unsaved changes") : QString()));
    refreshing_ = false;
}

} // namespace eggbert
