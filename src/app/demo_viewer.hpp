// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QWidget>

#include <functional>

#include "world/demo_recording.hpp"

class QLabel;
class QPushButton;
class QTableWidget;

namespace eggbert {

// The keys of a demo recording over time: one row per key, a bar for every
// stretch the key is held. Moving the pointer (or the arrow keys) over it
// names the step and the keys held there.
class DemoTimeline final : public QWidget {
    Q_OBJECT

public:
    explicit DemoTimeline(const DemoRecording &recording, QWidget *parent = nullptr);

    // The step under a widget x coordinate, or -1 outside the time axis.
    [[nodiscard]] int stepAt(int x) const;
    // Left edge of a step on the time axis.
    [[nodiscard]] int xOfStep(int step) const;
    // The row of a key, in widget coordinates.
    [[nodiscard]] QRect rowRect(std::size_t key) const;
    [[nodiscard]] int cursorStep() const { return cursor_; }
    // Clamped to the recording.
    void setCursorStep(int step);
    [[nodiscard]] QSize sizeHint() const override;

signals:
    void cursorStepChanged(int step);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    [[nodiscard]] int axisLeft() const;
    [[nodiscard]] int axisWidth() const;

    const DemoRecording &recording_;
    int cursor_ = 0;
};

// A read-only window for one demo recording of the games: what it holds,
// the timeline of keys, and the world it is replayed on. Recordings hold
// keys, not positions, so no path through the world is shown.
class DemoViewer final : public QWidget {
    Q_OBJECT

public:
    // Null, with `error` set, when the file is not a recording.
    static DemoViewer *open(const QString &path, QWidget *parent, QString *error);

    [[nodiscard]] const QString &path() const { return path_; }
    [[nodiscard]] const DemoRecording &recording() const { return recording_; }
    [[nodiscard]] DemoTimeline *timeline() const { return timeline_; }
    [[nodiscard]] QLabel *summaryLabel() const { return summary_; }
    [[nodiscard]] QLabel *cursorLabel() const { return cursorLabel_; }
    [[nodiscard]] QLabel *worldLabel() const { return worldLabel_; }
    [[nodiscard]] QTableWidget *keyTable() const { return keys_; }
    [[nodiscard]] QPushButton *openWorldButton() const { return openWorld_; }
    // The world file beside the recording the games replay it on; empty
    // when the recording's name has no number or no such file exists.
    [[nodiscard]] const QString &worldPath() const { return worldPath_; }

    // Called with `worldPath()` by the Open World button.
    std::function<void(const QString &)> openWorldFile;

private:
    DemoViewer(const QString &path, DemoRecording recording, QWidget *parent);
    void describeCursor(int step);

    QString path_;
    DemoRecording recording_;
    QString worldPath_;
    DemoTimeline *timeline_ = nullptr;
    QLabel *summary_ = nullptr;
    QLabel *cursorLabel_ = nullptr;
    QLabel *worldLabel_ = nullptr;
    QTableWidget *keys_ = nullptr;
    QPushButton *openWorld_ = nullptr;
};

// "about 40 s" for a number of game steps at the games' normal speed.
[[nodiscard]] QString demoDuration(int steps);
// Translated name of a key (bit 0..5) and of a set of keys.
[[nodiscard]] QString demoKeyLabel(std::size_t key);
[[nodiscard]] QString demoInputLabel(std::int16_t input);

} // namespace eggbert
