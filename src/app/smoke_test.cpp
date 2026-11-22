// SPDX-License-Identifier: GPL-3.0-only
#include "smoke_test.hpp"

#include <QApplication>
#include <QFile>
#include <QImageReader>
#include <QTemporaryDir>

#include <cstdio>
#include <vector>

#include "app/document_file.hpp"
#include "app/main_window.hpp"
#include "app/png_export.hpp"
#include "world/doctor.hpp"

namespace eggbert {

namespace {

// A minimal revision 1.3 world: zero header with the revision and scroll
// flags set, both layers empty (-1), all object slots free.
QByteArray syntheticWorld() {
    QByteArray bytes(50'468, '\0');
    bytes[0] = 1;
    bytes[2] = 3;
    bytes[212] = 100;
    bytes[216] = 100;
    bytes[222] = 1;
    for (int offset = 868; offset < 40'868; ++offset) {
        bytes[offset] = static_cast<char>(0xFF);
    }
    return bytes;
}

bool step(bool passed, const char *what) {
    if (!passed) {
        std::fprintf(stderr, "smoke test failed: %s\n", what);
    }
    return passed;
}

} // namespace

int runSmokeTest() {
    QTemporaryDir dir;
    if (!step(dir.isValid(), "create a temporary directory")) {
        return 1;
    }
    const auto path = dir.filePath(QStringLiteral("smoke.blp"));
    const auto original = syntheticWorld();
    {
        QFile file(path);
        if (!step(file.open(QIODevice::WriteOnly) && file.write(original) == original.size(),
                  "write the synthetic world")) {
            return 1;
        }
    }

    // Headless path: load, probe, edit, save, reload.
    auto loaded = DocumentFile::load(path);
    if (!step(loaded.has_value(), "load the world")) {
        return 1;
    }
    auto &file = *loaded.value();
    if (!step(file.document().probe().status == ProbeStatus::sizeMatches &&
                  file.document().isWritable(),
              "probe the world as an editable revision 1.3 file")) {
        return 1;
    }
    EditBuilder builder(file.document());
    builder.setCell(SectionKind::foreground, 3, 70, 153);
    if (!step(file.document().apply(builder.build("Smoke edit")).has_value(), "apply an edit") ||
        !step(file.save().ok(), "save atomically")) {
        return 1;
    }
    auto reloaded = DocumentFile::load(path);
    if (!step(reloaded.has_value(), "reload the saved world")) {
        return 1;
    }
    const auto saved = toByteArray(reloaded.value()->document().currentBytes());
    auto expected = original;
    expected[1608] = static_cast<char>(153); // 868 + 2 * (100 * 3 + 70)
    expected[1609] = 0;
    if (!step(saved == expected, "only the edited cell changed on disk")) {
        return 1;
    }
    const auto report =
        runDoctor(reloaded.value()->document().view(), GameProfile::speedyBlupi1);
    if (!step(report.count(Severity::error) == 0, "World Doctor reports no error")) {
        return 1;
    }

    // Rendering path.
    PngExportOptions options;
    options.pixelsPerCell = 4;
    const auto pngPath = dir.filePath(QStringLiteral("smoke.png"));
    const auto exported = exportWorldPng(reloaded.value()->document().view(),
                                         GameProfile::speedyBlupi1, options, pngPath);
    if (!step(exported.ok && QImageReader(pngPath).size() == QSize(400, 400),
              "export a PNG of the world")) {
        return 1;
    }

    // Window path: open the document in the real main window and close it.
    MainWindow window;
    window.prompts.error = [](const QString &, const QString &) {};
    window.show();
    QApplication::processEvents();
    auto *tab = window.openPath(path);
    QApplication::processEvents();
    if (!step(tab != nullptr && window.tabCount() == 1, "open the world in the main window") ||
        !step(window.grab().width() > 0, "paint the main window") ||
        !step(!MainWindow::resourceText(QStringLiteral("user-guide.md")).isEmpty(),
              "find the embedded user guide") ||
        !step(window.close(), "close the main window")) {
        return 1;
    }
    std::printf("Eggbert Editor %s: smoke test passed\n", EGGBERT_EDITOR_VERSION);
    return 0;
}

} // namespace eggbert
