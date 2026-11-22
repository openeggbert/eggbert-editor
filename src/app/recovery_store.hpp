// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>
#include <QStringList>

#include <cstddef>
#include <expected>
#include <span>
#include <vector>

namespace eggbert {

// One stored copy of a document's bytes.
struct RecoveryEntry {
    // File inside the store.
    QString storePath;
    // The world file the bytes belong to (canonical path when it existed).
    QString originalPath;
    // Empty for an autosave; the user's name for a snapshot.
    QString name;
    bool snapshot = false;
    QDateTime saved;
    // Size and SHA-256 of the bytes that were on disk when the copy was made.
    qint64 baseSize = 0;
    QByteArray baseSha256;
    qint64 payloadSize = 0;

    friend bool operator==(const RecoveryEntry &, const RecoveryEntry &) = default;
};

// A per-user directory of autosaves (unsaved bytes of open documents, for
// recovery after a crash) and named snapshots. Nothing is ever written next
// to a world file. Every file carries checksums and is validated before its
// bytes are handed out; a damaged file is reported, never applied.
class RecoveryStore {
public:
    // Snapshots kept per world file; the oldest goes when one more is taken.
    static constexpr int maximumSnapshotsPerFile = 20;
    static constexpr qint64 maximumPayloadBytes = 64 * 1024 * 1024;

    explicit RecoveryStore(QString directory = defaultDirectory());
    [[nodiscard]] static QString defaultDirectory();
    [[nodiscard]] const QString &directory() const { return directory_; }

    // One autosave per world file; writing replaces the previous one
    // atomically. `baseBytes` are the bytes last read from or written to disk.
    bool writeAutosave(const QString &originalPath, std::span<const std::byte> baseBytes,
                       std::span<const std::byte> currentBytes);
    void removeAutosave(const QString &originalPath);
    [[nodiscard]] std::vector<RecoveryEntry> autosaves() const;

    bool writeSnapshot(const QString &originalPath, const QString &name,
                       std::span<const std::byte> baseBytes,
                       std::span<const std::byte> currentBytes);
    // Newest first.
    [[nodiscard]] std::vector<RecoveryEntry> snapshots(const QString &originalPath) const;

    // The stored bytes, or why they cannot be used.
    [[nodiscard]] std::expected<QByteArray, QString> read(const RecoveryEntry &entry) const;
    bool remove(const RecoveryEntry &entry);

    // Files in the store that could not be understood (damaged, truncated,
    // foreign), by path.
    [[nodiscard]] QStringList damagedFiles() const;

    [[nodiscard]] static QByteArray sha256(std::span<const std::byte> bytes);

private:
    [[nodiscard]] QString keyFor(const QString &originalPath) const;
    bool write(const QString &target, const RecoveryEntry &entry,
               std::span<const std::byte> bytes) const;
    [[nodiscard]] std::vector<RecoveryEntry> list(bool snapshots, QStringList *damaged) const;

    QString directory_;
};

} // namespace eggbert
