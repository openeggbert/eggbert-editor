// SPDX-License-Identifier: GPL-3.0-only
//
// Opt-in compatibility check against real world files that live outside the
// repository. Set EGGBERT_SAMPLE_DIRS to one or more directories separated
// by ':' (each entry may be prefixed with "sb1=" or "sb2=" to state the
// game). Without it the test is skipped. Nothing is ever written into the
// sample directories: every file is copied to a temporary directory first.

#include "app/document_file.hpp"
#include "app/png_export.hpp"
#include "world/doctor.hpp"

#include "check.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QGuiApplication>
#include <QTemporaryDir>

#include <cstdio>

using namespace eggbert;

namespace {

QByteArray readAll(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.readAll();
}

struct Totals {
    int files = 0;
    int editable = 0;
    int readOnly = 0;
    int notWorlds = 0;
};

void checkFile(const QString &samplePath, GameProfile profile, const QTemporaryDir &scratch,
               Totals &totals) {
    const auto original = readAll(samplePath);
    const auto name = QFileInfo(samplePath).fileName();
    const auto digest =
        QCryptographicHash::hash(original, QCryptographicHash::Sha256).toHex().left(16);
    const auto workPath = scratch.filePath(QStringLiteral("%1-%2").arg(totals.files).arg(name));
    {
        QFile copy(workPath);
        CHECK(copy.open(QIODevice::WriteOnly));
        copy.write(original);
    }
    ++totals.files;

    auto loaded = DocumentFile::load(workPath);
    CHECK(loaded.has_value());
    if (!loaded) {
        return;
    }
    auto &file = *loaded.value();
    const auto &probe = file.document().probe();
    const auto revision = probe.headerMajor
                              ? QStringLiteral("%1.%2").arg(*probe.headerMajor).arg(*probe.headerMinor)
                              : QStringLiteral("-");

    // Whatever the file is: an untouched Save As is byte-identical.
    const auto copyPath = workPath + QStringLiteral(".copy");
    CHECK(file.saveAs(copyPath, false).ok());
    CHECK(readAll(copyPath) == original);
    // Inspection paths must cope with any content.
    const auto report = runDoctor(file.document().view(), profile);
    PngExportOptions options;
    options.pixelsPerCell = 2;
    CHECK(!renderWorldForExport(file.document().view(), options).isNull());

    QString outcome;
    if (probe.descriptor == nullptr) {
        ++totals.notWorlds;
        outcome = QStringLiteral("not a world");
    } else if (!file.document().isWritable()) {
        ++totals.readOnly;
        outcome = QStringLiteral("read-only");
        EditBuilder builder(file.document());
        builder.setCell(SectionKind::foreground, 1, 1, 7);
        if (builder.hasChanges()) {
            CHECK(!file.document().apply(builder.build("edit")).has_value());
        }
    } else {
        ++totals.editable;
        outcome = QStringLiteral("edit/save/reopen ok");
        // Edit one cell, save, reopen: exactly its two bytes differ.
        const std::size_t x = 97;
        const std::size_t y = 3;
        const auto offset = static_cast<qsizetype>(868 + 2 * (100 * x + y));
        const auto old = *file.document().view().foreground()->cell(x, y);
        const auto replacement = static_cast<std::int16_t>(old == 1 ? 2 : 1);
        EditBuilder builder(file.document());
        builder.setCell(SectionKind::foreground, x, y, replacement);
        CHECK(file.document().apply(builder.build("Sample edit")).has_value());
        CHECK(file.save().ok());
        const auto edited = readAll(copyPath);
        CHECK_EQ(edited.size(), original.size());
        for (qsizetype index = 0; index < std::min(edited.size(), original.size()); ++index) {
            if (index != offset && index != offset + 1 && edited[index] != original[index]) {
                CHECK(false); // an unrelated byte changed
                outcome = QStringLiteral("UNRELATED BYTE CHANGED at %1").arg(index);
                break;
            }
        }
        auto reopened = DocumentFile::load(copyPath);
        CHECK(reopened.has_value());
        if (reopened) {
            CHECK_EQ(*reopened.value()->document().view().foreground()->cell(x, y), replacement);
        }
        // Undo and save again: the original bytes are back.
        CHECK(file.document().undo());
        CHECK(file.save().ok());
        CHECK(readAll(copyPath) == original);
    }
    std::printf("%-28s %7lld  %s  rev %-5s %-22s doctor: %zu error(s), %zu warning(s)\n",
                name.toLocal8Bit().constData(), static_cast<long long>(original.size()),
                digest.constData(), revision.toLocal8Bit().constData(),
                outcome.toLocal8Bit().constData(), report.count(Severity::error),
                report.count(Severity::warning));
}

} // namespace

int main(int argc, char **argv) {
    QGuiApplication app(argc, argv);
    const auto setting = qEnvironmentVariable("EGGBERT_SAMPLE_DIRS");
    if (setting.isEmpty()) {
        std::printf("SKIPPED: EGGBERT_SAMPLE_DIRS is not set. Point it at directories of world "
                    "files kept outside the repository to run this check.\n");
        return 77;
    }
    QTemporaryDir scratch;
    CHECK(scratch.isValid());
    Totals totals;
    for (auto entry : setting.split(QLatin1Char(':'), Qt::SkipEmptyParts)) {
        auto profile = GameProfile::unknown;
        if (entry.startsWith(QLatin1String("sb1="))) {
            profile = GameProfile::speedyBlupi1;
            entry = entry.mid(4);
        } else if (entry.startsWith(QLatin1String("sb2="))) {
            profile = GameProfile::speedyBlupi2;
            entry = entry.mid(4);
        }
        if (!QDir(entry).exists()) {
            std::printf("SKIPPED directory (not found): %s\n", entry.toLocal8Bit().constData());
            continue;
        }
        std::printf("== %s (profile: %s)\n", entry.toLocal8Bit().constData(),
                    profile == GameProfile::speedyBlupi1   ? "Speedy Blupi 1"
                    : profile == GameProfile::speedyBlupi2 ? "Speedy Blupi 2"
                                                           : "not stated");
        QStringList paths;
        QDirIterator iterator(entry, {QStringLiteral("*.blp"), QStringLiteral("*.BLP"),
                                      QStringLiteral("*.xch"), QStringLiteral("*.XCH")},
                              QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            paths << iterator.next();
        }
        paths.sort();
        for (const auto &path : paths) {
            checkFile(path, profile, scratch, totals);
        }
    }
    std::printf("%d file(s): %d editable and round-tripped, %d read-only, %d not worlds\n",
                totals.files, totals.editable, totals.readOnly, totals.notWorlds);
    if (totals.files == 0) {
        std::printf("SKIPPED: no .blp or .xch files found.\n");
        return 77;
    }
    return test::finish("sample_matrix");
}
