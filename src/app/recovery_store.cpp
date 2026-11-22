// SPDX-License-Identifier: GPL-3.0-only
#include "recovery_store.hpp"

#include <QCryptographicHash>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>

namespace eggbert {

namespace {

constexpr char magic[] = "EGGBERT-RECOVERY\n";
constexpr quint32 formatVersion = 1;

struct Parsed {
    RecoveryEntry entry;
    QByteArray payload;
};

// Reads and validates a whole store file. `withPayload` false stops after
// the header checks that need no payload copy beyond the checksum.
std::expected<Parsed, QString> parse(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::unexpected(QStringLiteral("cannot be opened"));
    }
    if (file.size() > RecoveryStore::maximumPayloadBytes + 1024 * 1024) {
        return std::unexpected(QStringLiteral("is too large"));
    }
    if (file.read(sizeof(magic) - 1) != QByteArray(magic)) {
        return std::unexpected(QStringLiteral("is not a recovery file"));
    }
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);
    quint32 version = 0;
    quint8 kind = 0;
    Parsed parsed;
    qint64 savedMsecs = 0;
    QByteArray payloadSha;
    stream >> version;
    if (stream.status() != QDataStream::Ok || version != formatVersion) {
        return std::unexpected(QStringLiteral("has an unknown format version"));
    }
    stream >> kind >> parsed.entry.originalPath >> parsed.entry.name >> parsed.entry.baseSize >>
        parsed.entry.baseSha256 >> savedMsecs >> parsed.entry.payloadSize >> payloadSha;
    if (stream.status() != QDataStream::Ok || kind > 1 || parsed.entry.baseSize < 0 ||
        parsed.entry.payloadSize < 0 ||
        parsed.entry.payloadSize > RecoveryStore::maximumPayloadBytes ||
        parsed.entry.baseSha256.size() != 32 || payloadSha.size() != 32 ||
        parsed.entry.originalPath.isEmpty()) {
        return std::unexpected(QStringLiteral("has a damaged header"));
    }
    parsed.payload = file.read(parsed.entry.payloadSize);
    if (parsed.payload.size() != parsed.entry.payloadSize || !file.atEnd()) {
        return std::unexpected(QStringLiteral("is truncated or has extra data"));
    }
    if (QCryptographicHash::hash(parsed.payload, QCryptographicHash::Sha256) != payloadSha) {
        return std::unexpected(QStringLiteral("fails its checksum"));
    }
    parsed.entry.snapshot = kind == 1;
    parsed.entry.saved = QDateTime::fromMSecsSinceEpoch(savedMsecs);
    parsed.entry.storePath = path;
    return parsed;
}

QByteArray view(std::span<const std::byte> bytes) {
    return QByteArray::fromRawData(reinterpret_cast<const char *>(bytes.data()),
                                   static_cast<qsizetype>(bytes.size()));
}

} // namespace

RecoveryStore::RecoveryStore(QString directory) : directory_(std::move(directory)) {}

QString RecoveryStore::defaultDirectory() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
           QStringLiteral("/recovery");
}

QByteArray RecoveryStore::sha256(std::span<const std::byte> bytes) {
    return QCryptographicHash::hash(view(bytes), QCryptographicHash::Sha256);
}

QString RecoveryStore::keyFor(const QString &originalPath) const {
    const QFileInfo info(originalPath);
    const auto canonical = info.exists() ? info.canonicalFilePath() : info.absoluteFilePath();
    return QString::fromLatin1(
        QCryptographicHash::hash(canonical.toUtf8(), QCryptographicHash::Sha1).toHex());
}

bool RecoveryStore::write(const QString &target, const RecoveryEntry &entry,
                          std::span<const std::byte> bytes) const {
    if (static_cast<qint64>(bytes.size()) > maximumPayloadBytes || !QDir().mkpath(directory_)) {
        return false;
    }
    QSaveFile file(target);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(magic, sizeof(magic) - 1);
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);
    const auto payload = view(bytes);
    stream << formatVersion << static_cast<quint8>(entry.snapshot ? 1 : 0) << entry.originalPath
           << entry.name << entry.baseSize << entry.baseSha256
           << entry.saved.toMSecsSinceEpoch() << static_cast<qint64>(payload.size())
           << QCryptographicHash::hash(payload, QCryptographicHash::Sha256);
    if (stream.status() != QDataStream::Ok || file.write(payload) != payload.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

bool RecoveryStore::writeAutosave(const QString &originalPath, std::span<const std::byte> baseBytes,
                                  std::span<const std::byte> currentBytes) {
    RecoveryEntry entry;
    const QFileInfo info(originalPath);
    entry.originalPath = info.exists() ? info.canonicalFilePath() : info.absoluteFilePath();
    entry.saved = QDateTime::currentDateTime();
    entry.baseSize = static_cast<qint64>(baseBytes.size());
    entry.baseSha256 = sha256(baseBytes);
    return write(QDir(directory_).filePath(keyFor(originalPath) + QStringLiteral(".autosave")),
                 entry, currentBytes);
}

void RecoveryStore::removeAutosave(const QString &originalPath) {
    QFile::remove(QDir(directory_).filePath(keyFor(originalPath) + QStringLiteral(".autosave")));
}

bool RecoveryStore::writeSnapshot(const QString &originalPath, const QString &name,
                                  std::span<const std::byte> baseBytes,
                                  std::span<const std::byte> currentBytes) {
    RecoveryEntry entry;
    const QFileInfo info(originalPath);
    entry.originalPath = info.exists() ? info.canonicalFilePath() : info.absoluteFilePath();
    entry.name = name.trimmed().left(80);
    entry.snapshot = true;
    entry.saved = QDateTime::currentDateTime();
    entry.baseSize = static_cast<qint64>(baseBytes.size());
    entry.baseSha256 = sha256(baseBytes);
    // A unique name: time, then a counter for several in one millisecond.
    const auto stem = keyFor(originalPath) + QLatin1Char('-') +
                      QString::number(entry.saved.toMSecsSinceEpoch());
    QString target;
    for (int attempt = 0; attempt < 1000; ++attempt) {
        target = QDir(directory_).filePath(stem + QLatin1Char('-') + QString::number(attempt) +
                                           QStringLiteral(".snapshot"));
        if (!QFile::exists(target)) {
            break;
        }
    }
    if (!write(target, entry, currentBytes)) {
        return false;
    }
    auto existing = snapshots(originalPath);
    while (static_cast<int>(existing.size()) > maximumSnapshotsPerFile) {
        QFile::remove(existing.back().storePath);
        existing.pop_back();
    }
    return true;
}

std::vector<RecoveryEntry> RecoveryStore::list(bool snapshots, QStringList *damaged) const {
    std::vector<RecoveryEntry> entries;
    const QDir dir(directory_);
    const auto names = dir.entryList({QStringLiteral("*.autosave"), QStringLiteral("*.snapshot")},
                                     QDir::Files, QDir::Name);
    for (const auto &name : names) {
        const auto parsed = parse(dir.filePath(name));
        if (!parsed) {
            if (damaged != nullptr) {
                damaged->append(dir.filePath(name));
            }
            continue;
        }
        if (parsed->entry.snapshot == snapshots) {
            entries.push_back(parsed->entry);
        }
    }
    return entries;
}

std::vector<RecoveryEntry> RecoveryStore::autosaves() const {
    return list(false, nullptr);
}

std::vector<RecoveryEntry> RecoveryStore::snapshots(const QString &originalPath) const {
    const QFileInfo info(originalPath);
    const auto wanted = info.exists() ? info.canonicalFilePath() : info.absoluteFilePath();
    auto entries = list(true, nullptr);
    std::erase_if(entries, [&](const RecoveryEntry &entry) { return entry.originalPath != wanted; });
    std::ranges::sort(entries, [](const RecoveryEntry &left, const RecoveryEntry &right) {
        return left.saved != right.saved ? left.saved > right.saved
                                         : left.storePath > right.storePath;
    });
    return entries;
}

QStringList RecoveryStore::damagedFiles() const {
    QStringList damaged;
    const auto ignored = list(false, &damaged);
    static_cast<void>(ignored);
    return damaged;
}

std::expected<QByteArray, QString> RecoveryStore::read(const RecoveryEntry &entry) const {
    // Only files of this store are read.
    if (QFileInfo(entry.storePath).absolutePath() != QFileInfo(directory_).absoluteFilePath() &&
        QFileInfo(entry.storePath).canonicalPath() != QFileInfo(directory_).canonicalFilePath()) {
        return std::unexpected(QStringLiteral("is not in the recovery store"));
    }
    auto parsed = parse(entry.storePath);
    if (!parsed) {
        return std::unexpected(parsed.error());
    }
    return parsed->payload;
}

bool RecoveryStore::remove(const RecoveryEntry &entry) {
    if (QFileInfo(entry.storePath).absolutePath() != QFileInfo(directory_).absoluteFilePath() &&
        QFileInfo(entry.storePath).canonicalPath() != QFileInfo(directory_).canonicalFilePath()) {
        return false;
    }
    return QFile::remove(entry.storePath);
}

} // namespace eggbert
