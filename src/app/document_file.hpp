// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QByteArray>
#include <QString>

#include <expected>
#include <functional>
#include <memory>

#include "world/world_document.hpp"

namespace eggbert {

enum class LoadError { cannotOpen, tooLarge, readFailed };

struct LoadFailure {
    LoadError error{};
    QString detail;
};

enum class DiskState {
    // The file holds exactly the bytes last loaded or saved.
    unchanged,
    // The file exists but its bytes differ.
    modified,
    // The file no longer exists.
    missing,
    // The file exists but cannot be read.
    unreadable,
};

enum class SaveError {
    none,
    // The destination changed, disappeared or became unreadable since the
    // document last read or wrote it. Nothing was written.
    conflict,
    // Save As onto another existing file without overwrite consent.
    destinationExists,
    openFailed,
    writeFailed,
    // A test hook asked for a failure just before the atomic replacement.
    abortedBeforeCommit,
    commitFailed,
};

struct SaveResult {
    SaveError error = SaveError::none;
    DiskState diskState = DiskState::unchanged;
    QString detail;

    [[nodiscard]] bool ok() const { return error == SaveError::none; }
};

struct SaveHooks {
    // Called after all bytes are in the temporary file and before it replaces
    // the destination. Returning false abandons the save.
    std::function<bool()> beforeCommit;
};

// A world document bound to a file path. Writing always goes through an
// atomic replacement in the destination directory; a failed save leaves the
// destination and the document's modified state untouched.
class DocumentFile {
public:
    // Largest input accepted; a world is about 50 KiB.
    static constexpr qint64 maximumBytes = 64 * 1024 * 1024;

    [[nodiscard]] static std::expected<std::unique_ptr<DocumentFile>, LoadFailure>
    load(const QString &path);

    [[nodiscard]] WorldDocument &document() { return document_; }
    [[nodiscard]] const WorldDocument &document() const { return document_; }
    [[nodiscard]] const QString &path() const { return path_; }

    // Compares the file at path() with the last saved bytes. Never writes.
    [[nodiscard]] DiskState diskState() const;

    // Writes the current bytes to path() unless the file changed externally.
    SaveResult save(const SaveHooks &hooks = {});
    // Writes the current bytes to another path and adopts it on success.
    // Saving onto path() itself behaves like save().
    SaveResult saveAs(const QString &newPath, bool overwriteExisting,
                      const SaveHooks &hooks = {});

private:
    DocumentFile(QString path, std::vector<std::byte> bytes);
    SaveResult write(const QString &destination, const SaveHooks &hooks);

    QString path_;
    WorldDocument document_;
};

[[nodiscard]] QByteArray toByteArray(std::span<const std::byte> bytes);
[[nodiscard]] std::vector<std::byte> toByteVector(const QByteArray &bytes);

} // namespace eggbert
