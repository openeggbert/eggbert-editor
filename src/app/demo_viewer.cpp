// SPDX-License-Identifier: GPL-3.0-only
#include "demo_viewer.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

#include "app/core_texts.hpp"
#include "world/installation.hpp"

namespace eggbert {

namespace {

constexpr int rowHeight = 22;
constexpr int rowGap = 4;
constexpr int topMargin = 6;
constexpr int rightMargin = 8;
// Demo files are small; this bounds what is read before it is checked.
constexpr qint64 maximumFileBytes = 1024 * 1024;

} // namespace

QString demoKeyLabel(std::size_t key) {
    switch (key) {
    case 0:
        return DemoViewer::tr("Left");
    case 1:
        return DemoViewer::tr("Right");
    case 2:
        return DemoViewer::tr("Up");
    case 3:
        return DemoViewer::tr("Down");
    case 4:
        return DemoViewer::tr("Jump");
    case 5:
        return DemoViewer::tr("Fire");
    default:
        return {};
    }
}

QString demoInputLabel(std::int16_t input) {
    QStringList names;
    for (std::size_t key = 0; key < demoKeyCount; ++key) {
        if ((input & (1 << key)) != 0) {
            names << demoKeyLabel(key);
        }
    }
    return names.isEmpty() ? DemoViewer::tr("no key") : names.join(QStringLiteral(" + "));
}

QString demoDuration(int steps) {
    const auto tenths = (static_cast<long long>(steps) * demoStepMilliseconds + 50) / 100;
    return DemoViewer::tr("about %1 s").arg(QString::number(static_cast<double>(tenths) / 10.0,
                                                           'f', 1));
}

DemoTimeline::DemoTimeline(const DemoRecording &recording, QWidget *parent)
    : QWidget(parent), recording_(recording) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(tr("Timeline of the keys held"));
    setAccessibleDescription(
        tr("One row per key; the arrow keys move the step whose keys are named below."));
    setMinimumHeight(sizeHint().height());
}

QSize DemoTimeline::sizeHint() const {
    return {640, topMargin * 2 + static_cast<int>(demoKeyCount) * (rowHeight + rowGap)};
}

int DemoTimeline::axisLeft() const {
    int widest = 0;
    for (std::size_t key = 0; key < demoKeyCount; ++key) {
        widest = std::max(widest, fontMetrics().horizontalAdvance(demoKeyLabel(key)));
    }
    return widest + 16;
}

int DemoTimeline::axisWidth() const { return std::max(1, width() - axisLeft() - rightMargin); }

int DemoTimeline::xOfStep(int step) const {
    const auto last = std::max(1, recording_.lastTime());
    return axisLeft() + static_cast<int>(static_cast<long long>(step) * axisWidth() / last);
}

int DemoTimeline::stepAt(int x) const {
    if (x < axisLeft() || x >= axisLeft() + axisWidth()) {
        return -1;
    }
    const auto last = std::max(1, recording_.lastTime());
    return static_cast<int>(static_cast<long long>(x - axisLeft()) * last / axisWidth());
}

QRect DemoTimeline::rowRect(std::size_t key) const {
    return {axisLeft(), topMargin + static_cast<int>(key) * (rowHeight + rowGap), axisWidth(),
            rowHeight};
}

void DemoTimeline::setCursorStep(int step) {
    step = std::clamp(step, 0, recording_.lastTime());
    if (step == cursor_) {
        return;
    }
    cursor_ = step;
    update();
    emit cursorStepChanged(cursor_);
}

void DemoTimeline::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    const auto &colors = palette();
    painter.fillRect(rect(), colors.window());
    for (std::size_t key = 0; key < demoKeyCount; ++key) {
        const auto row = rowRect(key);
        painter.setPen(colors.color(QPalette::WindowText));
        painter.drawText(QRect(0, row.top(), axisLeft() - 8, row.height()),
                         Qt::AlignRight | Qt::AlignVCenter, demoKeyLabel(key));
        painter.fillRect(row, colors.base());
        for (const auto &interval : recording_.held(key)) {
            const auto left = xOfStep(interval.from);
            // Even the shortest press stays visible.
            const auto right = std::max(left + 1, xOfStep(interval.to));
            painter.fillRect(QRect(left, row.top() + 3, right - left, row.height() - 6),
                             colors.highlight());
        }
        painter.setPen(colors.color(QPalette::Mid));
        painter.drawRect(row.adjusted(0, 0, -1, -1));
    }
    painter.setPen(QPen(colors.color(QPalette::WindowText), hasFocus() ? 2 : 1));
    const auto x = xOfStep(cursor_);
    painter.drawLine(x, topMargin - 3, x, rowRect(demoKeyCount - 1).bottom() + 3);
}

void DemoTimeline::mouseMoveEvent(QMouseEvent *event) {
    if (const auto step = stepAt(event->position().toPoint().x()); step >= 0) {
        setCursorStep(step);
    }
}

void DemoTimeline::mousePressEvent(QMouseEvent *event) { mouseMoveEvent(event); }

void DemoTimeline::keyPressEvent(QKeyEvent *event) {
    const auto page = std::max(1, recording_.lastTime() / 20);
    switch (event->key()) {
    case Qt::Key_Left:
        setCursorStep(cursor_ - 1);
        break;
    case Qt::Key_Right:
        setCursorStep(cursor_ + 1);
        break;
    case Qt::Key_PageUp:
        setCursorStep(cursor_ - page);
        break;
    case Qt::Key_PageDown:
        setCursorStep(cursor_ + page);
        break;
    case Qt::Key_Home:
        setCursorStep(0);
        break;
    case Qt::Key_End:
        setCursorStep(recording_.lastTime());
        break;
    default:
        QWidget::keyPressEvent(event);
    }
}

DemoViewer *DemoViewer::open(const QString &path, QWidget *parent, QString *error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > maximumFileBytes) {
        if (error != nullptr) {
            *error = tr("%1 could not be read.").arg(path);
        }
        return nullptr;
    }
    const auto bytes = file.readAll();
    auto recording = DemoRecording::parse(
        {reinterpret_cast<const std::byte *>(bytes.constData()),
         static_cast<std::size_t>(bytes.size())});
    if (!recording) {
        if (error != nullptr) {
            *error = tr("%1 is not a demo recording. %2")
                         .arg(path, coreText(recording.error()));
        }
        return nullptr;
    }
    return new DemoViewer(path, std::move(*recording), parent);
}

DemoViewer::DemoViewer(const QString &path, DemoRecording recording, QWidget *parent)
    : QWidget(parent, Qt::Window), path_(path), recording_(std::move(recording)) {
    setAttribute(Qt::WA_DeleteOnClose);
    const QFileInfo info(path);
    setWindowTitle(tr("Demo Recording - %1").arg(info.fileName()));

    auto *layout = new QVBoxLayout(this);
    summary_ = new QLabel(tr("%1 recorded event(s) over %2 game step(s), %3 at the games' normal "
                             "speed. The file is only read, never changed.")
                              .arg(recording_.events.size())
                              .arg(recording_.lastTime())
                              .arg(demoDuration(recording_.lastTime())),
                          this);
    summary_->setWordWrap(true);
    layout->addWidget(summary_);

    // The games replay demo NNN on world NNN of the same folder.
    const auto name = classifyDataFileName(info.fileName().toStdString());
    worldLabel_ = new QLabel(this);
    worldLabel_->setWordWrap(true);
    openWorld_ = new QPushButton(this);
    if (name.kind == DataFileKind::demoRecording) {
        const auto wanted = QString::fromStdString(worldSlotFileName({false, 0, name.number}));
        for (const auto &candidate : info.dir().entryList(QDir::Files)) {
            if (!wanted.isEmpty() && candidate.compare(wanted, Qt::CaseInsensitive) == 0) {
                worldPath_ = info.dir().filePath(candidate);
                break;
            }
        }
        worldLabel_->setText(worldPath_.isEmpty()
                                 ? tr("The games replay it on world %1; no file for that world "
                                      "is in this folder.")
                                       .arg(name.number)
                                 : tr("The games replay it on world %1.").arg(name.number));
        openWorld_->setText(tr("&Open World %1").arg(name.number));
    } else {
        worldLabel_->setText(tr("The file name is not one the games use (demo and three "
                                "digits), so the world it belongs to is not known."));
        openWorld_->setText(tr("&Open World"));
    }
    openWorld_->setEnabled(!worldPath_.isEmpty());
    connect(openWorld_, &QPushButton::clicked, this, [this] {
        if (openWorldFile && !worldPath_.isEmpty()) {
            openWorldFile(worldPath_);
        }
    });
    layout->addWidget(worldLabel_);

    timeline_ = new DemoTimeline(recording_, this);
    layout->addWidget(timeline_);
    cursorLabel_ = new QLabel(this);
    layout->addWidget(cursorLabel_);
    connect(timeline_, &DemoTimeline::cursorStepChanged, this, &DemoViewer::describeCursor);
    describeCursor(0);

    keys_ = new QTableWidget(static_cast<int>(demoKeyCount), 3, this);
    keys_->setAccessibleName(tr("Keys of the recording"));
    keys_->setHorizontalHeaderLabels({tr("Key"), tr("Pressed"), tr("Held")});
    keys_->verticalHeader()->hide();
    keys_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    keys_->setSelectionBehavior(QAbstractItemView::SelectRows);
    const auto presses = recording_.presses();
    const auto steps = recording_.heldSteps();
    for (std::size_t key = 0; key < demoKeyCount; ++key) {
        const auto row = static_cast<int>(key);
        keys_->setItem(row, 0, new QTableWidgetItem(demoKeyLabel(key)));
        keys_->setItem(row, 1, new QTableWidgetItem(tr("%1 time(s)").arg(presses[key])));
        keys_->setItem(row, 2,
                       new QTableWidgetItem(tr("%1 step(s), %2")
                                                .arg(steps[key])
                                                .arg(demoDuration(static_cast<int>(steps[key])))));
    }
    keys_->horizontalHeader()->setStretchLastSection(true);
    keys_->resizeColumnsToContents();
    layout->addWidget(keys_, 1);

    auto *note = new QLabel(tr("A recording holds the keys the player held, not where the "
                               "player was: a path through the world cannot be drawn from it."),
                            this);
    note->setWordWrap(true);
    layout->addWidget(note);
    layout->addWidget(openWorld_, 0, Qt::AlignLeft);
    resize(720, 520);
}

void DemoViewer::describeCursor(int step) {
    cursorLabel_->setText(tr("Step %1 (%2): %3")
                              .arg(step)
                              .arg(demoDuration(step), demoInputLabel(recording_.inputAt(step))));
}

} // namespace eggbert
