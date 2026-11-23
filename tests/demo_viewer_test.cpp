// SPDX-License-Identifier: GPL-3.0-only
//
// The demo recording window: opened instead of a "damaged world" tab, the
// timeline's geometry and cursor, the key table, and the world it belongs to.
#include "app/demo_viewer.hpp"
#include "app/main_window.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QApplication>
#include <QFile>
#include <QImage>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTest>

using namespace eggbert;
using namespace eggbert::test;

namespace {

Bytes recording(std::initializer_list<std::pair<int, int>> events) {
    Bytes bytes(204, std::byte{0});
    bytes[0] = std::byte{1};
    for (const auto &[time, input] : events) {
        for (const int value : {time, input}) {
            bytes.push_back(static_cast<std::byte>(value & 0xff));
            bytes.push_back(static_cast<std::byte>((value >> 8) & 0xff));
        }
    }
    return bytes;
}

void writeAll(const QString &path, const Bytes &bytes) {
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<qint64>(bytes.size()));
}

QByteArray readAll(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

} // namespace

int main(int argc, char **argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QStandardPaths::setTestModeEnabled(true);
    QApplication app(argc, argv);
    QApplication::setOrganizationName("eggbert-editor-test");
    QApplication::setApplicationName("demo-viewer-test");
    QSettings().clear();

    QTemporaryDir dir;
    CHECK(dir.isValid());
    // right 100..500, jump 200..240, left 600..700, end at 1000
    const auto bytes = recording({{100, 2}, {200, 18}, {240, 2}, {500, 0}, {600, 1}, {700, 0},
                                  {1000, 0}});
    const auto demoPath = dir.filePath("DEMO200.BLP");
    writeAll(demoPath, bytes);
    const auto worldPath = dir.filePath("World200.blp");
    writeAll(worldPath, blankWorld(fxRevision13));

    CHECK_EQ(demoDuration(1000), QString("about 50.0 s"));
    CHECK_EQ(demoDuration(1), QString("about 0.1 s"));
    CHECK_EQ(demoInputLabel(0), QString("no key"));
    CHECK_EQ(demoInputLabel(18), QString("Right + Jump"));

    MainWindow window;
    QStringList errors;
    window.prompts.error = [&](const QString &, const QString &text) { errors << text; };
    window.prompts.unsavedChanges = [](DocumentTab *) { return UnsavedChoice::discard; };

    // Opening a recording makes no tab; it gets a window of its own.
    CHECK(window.openPath(demoPath) == nullptr);
    CHECK(errors.isEmpty());
    CHECK_EQ(window.tabCount(), 0);
    CHECK_EQ(window.demoViewers().size(), std::size_t{1});
    CHECK(window.statusBar()->currentMessage().contains("demo recording"));
    auto *viewer = window.demoViewers().front();
    CHECK(viewer->isVisible());
    CHECK(viewer->windowTitle().contains("DEMO200.BLP"));
    CHECK_EQ(viewer->recording().events.size(), std::size_t{7});
    CHECK(viewer->summaryLabel()->text().contains("7 recorded event(s) over 1000 game step(s)"));
    CHECK(viewer->summaryLabel()->text().contains("about 50.0 s"));
    // Opening it again raises the same window.
    CHECK(window.openPath(demoPath) == nullptr);
    CHECK_EQ(window.demoViewers().size(), std::size_t{1});

    // The key table.
    auto *table = viewer->keyTable();
    CHECK_EQ(table->rowCount(), 6);
    CHECK(!table->accessibleName().isEmpty());
    CHECK_EQ(table->item(1, 0)->text(), QString("Right"));
    CHECK_EQ(table->item(1, 1)->text(), QString("1 time(s)"));
    CHECK_EQ(table->item(1, 2)->text(), QString("400 step(s), about 20.0 s"));
    CHECK_EQ(table->item(4, 0)->text(), QString("Jump"));
    CHECK_EQ(table->item(4, 2)->text(), QString("40 step(s), about 2.0 s"));
    CHECK_EQ(table->item(5, 1)->text(), QString("0 time(s)"));

    // The timeline: steps map onto the axis and back, the cursor names the
    // keys, and the keyboard moves it.
    auto *timeline = viewer->timeline();
    CHECK(!timeline->accessibleName().isEmpty());
    CHECK(timeline->focusPolicy() == Qt::StrongFocus);
    timeline->resize(800, timeline->sizeHint().height());
    const auto row = timeline->rowRect(1);
    CHECK_EQ(timeline->xOfStep(0), row.left());
    CHECK_EQ(timeline->xOfStep(1000), row.left() + row.width());
    CHECK(timeline->xOfStep(500) > timeline->xOfStep(100));
    CHECK_EQ(timeline->stepAt(row.left() - 1), -1);
    CHECK_EQ(timeline->stepAt(row.left()), 0);
    CHECK_EQ(timeline->stepAt(row.left() + row.width()), -1);
    {
        const auto step = timeline->stepAt(timeline->xOfStep(220) + 1);
        CHECK(step >= 220 && step <= 222);
    }
    CHECK(timeline->rowRect(0).bottom() < timeline->rowRect(1).top());
    CHECK(viewer->cursorLabel()->text().startsWith("Step 0 (about 0.0 s): no key"));
    timeline->setCursorStep(220);
    CHECK_EQ(viewer->cursorLabel()->text(), QString("Step 220 (about 11.0 s): Right + Jump"));
    QTest::keyClick(timeline, Qt::Key_Right);
    CHECK_EQ(timeline->cursorStep(), 221);
    QTest::keyClick(timeline, Qt::Key_End);
    CHECK_EQ(timeline->cursorStep(), 1000);
    CHECK(viewer->cursorLabel()->text().endsWith(": no key"));
    QTest::keyClick(timeline, Qt::Key_Right); // clamped
    CHECK_EQ(timeline->cursorStep(), 1000);
    QTest::keyClick(timeline, Qt::Key_Home);
    CHECK_EQ(timeline->cursorStep(), 0);
    QTest::keyClick(timeline, Qt::Key_PageDown);
    CHECK_EQ(timeline->cursorStep(), 50);
    QTest::mouseMove(timeline, QPoint(timeline->xOfStep(650) + 1, row.center().y()));
    QTest::mouseClick(timeline, Qt::LeftButton, {},
                      QPoint(timeline->xOfStep(650) + 1, row.center().y()));
    CHECK(viewer->cursorLabel()->text().endsWith(": Left"));
    // The held stretch is painted in the key's row and nowhere else in it.
    {
        timeline->setCursorStep(0);
        QImage image(timeline->size(), QImage::Format_ARGB32);
        timeline->render(&image);
        const auto y = timeline->rowRect(1).center().y();
        const auto held = image.pixelColor(timeline->xOfStep(300), y);
        const auto idle = image.pixelColor(timeline->xOfStep(800), y);
        CHECK(held != idle);
        CHECK(held == timeline->palette().color(QPalette::Highlight));
        const auto otherRow = image.pixelColor(timeline->xOfStep(300),
                                               timeline->rowRect(5).center().y());
        CHECK(otherRow == idle);
    }

    // The world the games replay it on is found beside it, whatever the case.
    CHECK(viewer->worldLabel()->text().contains("world 200"));
    CHECK_EQ(viewer->worldPath(), worldPath);
    CHECK(viewer->openWorldButton()->isEnabled());
    viewer->openWorldButton()->click();
    CHECK_EQ(window.tabCount(), 1);
    // Nothing was written.
    CHECK(readAll(demoPath) ==
          QByteArray(reinterpret_cast<const char *>(bytes.data()),
                     static_cast<qsizetype>(bytes.size())));

    // A recording without its world, and one under a name the games do not use.
    {
        const auto lonely = dir.filePath("demo207.blp");
        writeAll(lonely, bytes);
        auto *second = window.openDemoRecording(lonely);
        CHECK(second != nullptr);
        CHECK(second->worldPath().isEmpty());
        CHECK(!second->openWorldButton()->isEnabled());
        CHECK(second->worldLabel()->text().contains("world 207"));
        CHECK(second->worldLabel()->text().contains("no file"));
        const auto renamed = dir.filePath("my-run.bin");
        writeAll(renamed, bytes);
        CHECK(window.openPath(renamed) == nullptr);
        CHECK_EQ(window.demoViewers().size(), std::size_t{3});
        auto *third = window.demoViewers().back();
        CHECK(!third->openWorldButton()->isEnabled());
        CHECK(third->worldLabel()->text().contains("not known"));
    }
    // A file that is not a recording is refused by the viewer with the reason...
    {
        const auto notDemo = dir.filePath("not-demo.blp");
        writeAll(notDemo, blankWorld(fxRevision13));
        CHECK(window.openDemoRecording(notDemo) == nullptr);
        CHECK_EQ(errors.size(), qsizetype{1});
        CHECK(errors.last().contains("is not a demo recording"));
        CHECK(window.openDemoRecording(dir.filePath("missing.blp")) == nullptr);
        CHECK_EQ(errors.size(), qsizetype{2});
        // ...and a world, complete or cut short, still opens as a world.
        const auto tabsBefore = window.tabCount();
        CHECK(window.openPath(notDemo) != nullptr);
        auto cut = blankWorld(fxRevision10);
        cut.resize(1356);
        const auto cutPath = dir.filePath("cut.blp");
        writeAll(cutPath, cut);
        CHECK(window.openPath(cutPath) != nullptr);
        CHECK_EQ(window.tabCount(), tabsBefore + 2);
        CHECK_EQ(window.demoViewers().size(), std::size_t{3});
    }
    // Closing a viewer forgets it.
    viewer->close();
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    CHECK_EQ(window.demoViewers().size(), std::size_t{2});
    return test::finish("demo_viewer");
}
