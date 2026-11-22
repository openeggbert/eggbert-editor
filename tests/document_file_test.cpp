// SPDX-License-Identifier: GPL-3.0-only
#include "app/document_file.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <algorithm>

using namespace eggbert;
using namespace eggbert::test;

namespace {

QByteArray raw(const Bytes &bytes) {
    return QByteArray(reinterpret_cast<const char *>(bytes.data()),
                      static_cast<qsizetype>(bytes.size()));
}

void writeRaw(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    CHECK_EQ(file.write(bytes), bytes.size());
}

QByteArray readRaw(const QString &path) {
    QFile file(path);
    CHECK(file.open(QIODevice::ReadOnly));
    return file.readAll();
}

// Positions where two byte arrays of equal size differ.
std::vector<qsizetype> diffPositions(const QByteArray &left, const QByteArray &right) {
    std::vector<qsizetype> positions;
    CHECK_EQ(left.size(), right.size());
    for (qsizetype index = 0; index < std::min(left.size(), right.size()); ++index) {
        if (left[index] != right[index]) {
            positions.push_back(index);
        }
    }
    return positions;
}

bool setCell(DocumentFile &file, std::size_t x, std::size_t y, std::int16_t value) {
    EditBuilder builder(file.document());
    builder.setCell(SectionKind::foreground, x, y, value);
    return file.document().apply(builder.build("Set cell")).has_value();
}

void checkLoad(const QTemporaryDir &dir) {
    const auto missing = DocumentFile::load(dir.filePath("missing.blp"));
    CHECK(!missing.has_value());
    CHECK(missing.error().error == LoadError::cannotOpen);
    const auto directory = DocumentFile::load(dir.path());
    CHECK(!directory.has_value());

    // Any bytes load; unknown content is simply not writable.
    writeRaw(dir.filePath("empty.blp"), {});
    const auto empty = DocumentFile::load(dir.filePath("empty.blp"));
    CHECK(empty.has_value());
    CHECK(empty.value()->document().writeAccess() == WriteAccess::unknownLayout);
    CHECK(empty.value()->diskState() == DiskState::unchanged);
}

void checkUntouchedRoundTrip(const QTemporaryDir &dir) {
    for (const auto &layout : fxRevisions) {
        const auto input = raw(patternedWorld(layout, 17));
        const auto path = dir.filePath(QString("untouched-%1.blp").arg(layout.minor));
        writeRaw(path, input);
        auto loaded = DocumentFile::load(path);
        CHECK(loaded.has_value());
        auto &file = *loaded.value();
        CHECK(toByteArray(file.document().originalBytes()) == input);
        CHECK(file.diskState() == DiskState::unchanged);

        // Save in place and Save As both produce identical bytes, also for
        // the read-only revisions 1.0 and 1.1.
        CHECK(file.save().ok());
        CHECK(readRaw(path) == input);
        const auto copy = dir.filePath(QString("copy-%1.blp").arg(layout.minor));
        CHECK(file.saveAs(copy, false).ok());
        CHECK(readRaw(copy) == input);
        CHECK(readRaw(path) == input);
        CHECK_EQ(file.path(), copy);
        CHECK(!file.document().isModified());
    }
}

void checkEditSaveReopen(const QTemporaryDir &dir) {
    const auto input = raw(patternedWorld(fxRevision13, 5));
    const auto path = dir.filePath("edit.blp");
    writeRaw(path, input);
    auto loaded = DocumentFile::load(path);
    auto &file = *loaded.value();

    CHECK(setCell(file, 3, 70, 4321));
    CHECK(file.document().isModified());
    CHECK(readRaw(path) == input); // nothing written yet
    CHECK(file.save().ok());
    CHECK(!file.document().isModified());

    const auto saved = readRaw(path);
    const auto positions = diffPositions(saved, input);
    CHECK(!positions.empty());
    CHECK(std::ranges::all_of(positions, [](qsizetype position) {
        return position >= 1608 && position < 1610; // 868 + 200*3 + 2*70
    }));

    // Reopen: the edit is there, the document is clean and writable.
    auto reopened = DocumentFile::load(path);
    CHECK_EQ(*reopened.value()->document().view().foreground()->cell(3, 70), std::int16_t{4321});
    CHECK(toByteArray(reopened.value()->document().currentBytes()) == saved);

    // Repeated save without changes rewrites the same bytes.
    CHECK(file.save().ok());
    CHECK(file.save().ok());
    CHECK(readRaw(path) == saved);

    // Undo across the clean index, save again: back to the input bytes.
    CHECK(file.document().undo());
    CHECK(file.document().isModified());
    CHECK(file.save().ok());
    CHECK(readRaw(path) == input);
    CHECK(!file.document().isModified());
    CHECK(file.document().redo());
    CHECK(file.document().isModified());
    CHECK(file.save().ok());
    CHECK(readRaw(path) == saved);

    // No temporary files are left next to the document.
    const auto entries = QDir(dir.path()).entryList({"edit.blp*"}, QDir::Files | QDir::Hidden);
    CHECK_EQ(entries.size(), qsizetype{1});
}

void checkConflicts(const QTemporaryDir &dir) {
    const auto input = raw(patternedWorld(fxRevision12));
    const auto path = dir.filePath("conflict.blp");

    // Externally modified content of the same size.
    {
        writeRaw(path, input);
        auto loaded = DocumentFile::load(path);
        auto &file = *loaded.value();
        CHECK(setCell(file, 1, 1, 9));
        auto external = input;
        external[30'000] = static_cast<char>(external[30'000] ^ 0x55);
        writeRaw(path, external);
        CHECK(file.diskState() == DiskState::modified);
        const auto result = file.save();
        CHECK(result.error == SaveError::conflict);
        CHECK(result.diskState == DiskState::modified);
        CHECK(!result.detail.isEmpty());
        CHECK(readRaw(path) == external); // the foreign bytes are untouched
        CHECK(file.document().isModified());
        // Save As elsewhere remains possible and does not touch the conflict.
        const auto other = dir.filePath("conflict-resolved.blp");
        CHECK(file.saveAs(other, false).ok());
        CHECK(readRaw(path) == external);
        CHECK(!file.document().isModified());
        CHECK_EQ(*DocumentFile::load(other).value()->document().view().foreground()->cell(1, 1),
                 std::int16_t{9});
    }
    // Externally truncated and externally extended.
    for (const int delta : {-1, 1}) {
        writeRaw(path, input);
        auto loaded = DocumentFile::load(path);
        auto &file = *loaded.value();
        CHECK(setCell(file, 2, 2, 8));
        auto external = input;
        if (delta < 0) {
            external.chop(1);
        } else {
            external.append('x');
        }
        writeRaw(path, external);
        CHECK(file.save().error == SaveError::conflict);
        CHECK(readRaw(path) == external);
        CHECK(file.document().isModified());
    }
    // Externally deleted.
    {
        writeRaw(path, input);
        auto loaded = DocumentFile::load(path);
        auto &file = *loaded.value();
        CHECK(setCell(file, 3, 3, 7));
        CHECK(QFile::remove(path));
        CHECK(file.diskState() == DiskState::missing);
        const auto result = file.save();
        CHECK(result.error == SaveError::conflict);
        CHECK(result.diskState == DiskState::missing);
        CHECK(!QFile::exists(path)); // not silently recreated
        CHECK(file.document().isModified());
    }
    // Replaced by a directory: unreadable, still a conflict.
    {
        writeRaw(path, input);
        auto loaded = DocumentFile::load(path);
        auto &file = *loaded.value();
        CHECK(setCell(file, 4, 4, 6));
        CHECK(QFile::remove(path));
        CHECK(QDir().mkdir(path));
        CHECK(file.save().error == SaveError::conflict);
        CHECK(file.document().isModified());
        CHECK(QDir().rmdir(path));
    }
}

void checkFailures(const QTemporaryDir &dir) {
    const auto input = raw(patternedWorld(fxRevision13));
    const auto path = dir.filePath("failure.blp");
    writeRaw(path, input);
    auto loaded = DocumentFile::load(path);
    auto &file = *loaded.value();
    CHECK(setCell(file, 9, 9, 99));
    const auto edited = toByteArray(file.document().currentBytes());

    // Injected failure after the temporary file is complete and before the
    // atomic replacement.
    bool hookRan = false;
    SaveHooks failing;
    failing.beforeCommit = [&] {
        hookRan = true;
        CHECK(readRaw(path) == input); // destination still intact at this point
        return false;
    };
    const auto aborted = file.save(failing);
    CHECK(hookRan);
    CHECK(aborted.error == SaveError::abortedBeforeCommit);
    CHECK(readRaw(path) == input);
    CHECK(file.document().isModified());
    CHECK(toByteArray(file.document().currentBytes()) == edited);
    CHECK_EQ(QDir(dir.path()).entryList({"failure.blp*"}, QDir::Files | QDir::Hidden).size(),
             qsizetype{1});

    // Save As with the same injected failure creates nothing.
    const auto other = dir.filePath("failure-copy.blp");
    CHECK(file.saveAs(other, false, failing).error == SaveError::abortedBeforeCommit);
    CHECK(!QFile::exists(other));
    CHECK_EQ(file.path(), path);

    // Unwritable destinations: the parent is a regular file, or missing.
    // (Permission bits are not used because tests may run as root.)
    const auto underFile = path + "/child.blp";
    const auto openFailure = file.saveAs(underFile, false);
    CHECK(openFailure.error == SaveError::openFailed);
    CHECK(!openFailure.detail.isEmpty());
    CHECK(file.saveAs(dir.filePath("no-such-dir/x.blp"), false).error == SaveError::openFailed);
    CHECK(readRaw(path) == input);
    CHECK(file.document().isModified());
    CHECK_EQ(file.path(), path);

    // Save As onto another existing file needs consent.
    const auto existing = dir.filePath("existing.blp");
    writeRaw(existing, "keep me");
    CHECK(file.saveAs(existing, false).error == SaveError::destinationExists);
    CHECK(readRaw(existing) == QByteArray("keep me"));
    CHECK(file.document().isModified());
    CHECK(file.saveAs(existing, true).ok());
    CHECK(readRaw(existing) == edited);
    CHECK(readRaw(path) == input); // the old path keeps its bytes
    CHECK_EQ(file.path(), existing);
    CHECK(!file.document().isModified());

    // A successful hook lets the save through.
    CHECK(setCell(file, 9, 9, 100));
    SaveHooks passing;
    passing.beforeCommit = [] { return true; };
    CHECK(file.save(passing).ok());
    CHECK(readRaw(existing) == toByteArray(file.document().currentBytes()));

    // Save As onto the document's own path is an ordinary, conflict-checked save.
    CHECK(setCell(file, 9, 9, 101));
    writeRaw(existing, "changed elsewhere");
    CHECK(file.saveAs(existing, true).error == SaveError::conflict);
    CHECK(readRaw(existing) == QByteArray("changed elsewhere"));
}

} // namespace

int main() {
    QTemporaryDir dir;
    CHECK(dir.isValid());
    checkLoad(dir);
    checkUntouchedRoundTrip(dir);
    checkEditSaveReopen(dir);
    checkConflicts(dir);
    checkFailures(dir);
    return test::finish("document_file");
}
