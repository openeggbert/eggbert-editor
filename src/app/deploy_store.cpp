// SPDX-License-Identifier: GPL-3.0-only
#include "app/deploy_store.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QUuid>

#include <algorithm>
#include <optional>

namespace eggbert {

namespace {

QByteArray hashOf(const QByteArray &bytes) {
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256);
}

std::optional<QByteArray> readAll(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > DeployStore::maximumFileBytes) {
        return std::nullopt;
    }
    const auto size = file.size();
    const auto bytes = file.readAll();
    if (file.error() != QFileDevice::NoError || bytes.size() != size) {
        return std::nullopt;
    }
    return bytes;
}

bool writeAtomically(const QString &path, const QByteArray &bytes) {
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

} // namespace

DeployStore::DeployStore(QString directory) : directory_(std::move(directory)) {}

QString DeployStore::defaultDirectory() {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("deploys"));
}

QString DeployStore::backupPath(const QString &id) const {
    return QDir(directory_).filePath(id + QStringLiteral(".previous"));
}

QString DeployStore::recordPath(const QString &id) const {
    return QDir(directory_).filePath(id + QStringLiteral(".json"));
}

std::expected<DeployRecord, QString> DeployStore::deploy(const QString &dataDirectory,
                                                         const QString &fileName,
                                                         std::span<const std::byte> bytes,
                                                         DeployMode mode) {
    const QDir data(dataDirectory);
    if (fileName.isEmpty() || QFileInfo(fileName).fileName() != fileName || !data.exists()) {
        return std::unexpected(QCoreApplication::translate("eggbert::Files", "%1 is not a file name in an existing directory.")
                                   .arg(data.filePath(fileName)));
    }
    if (!QDir().mkpath(directory_)) {
        return std::unexpected(
            QCoreApplication::translate("eggbert::Files", "The backup directory %1 cannot be created.").arg(directory_));
    }
    // An existing entry keeps its own spelling (WORLD001.BLP stays that).
    QString target = data.filePath(fileName);
    for (const auto &entry : data.entryList(QDir::Files)) {
        if (entry.compare(fileName, Qt::CaseInsensitive) == 0) {
            target = data.filePath(entry);
            break;
        }
    }
    const QByteArray payload(reinterpret_cast<const char *>(bytes.data()),
                             static_cast<qsizetype>(bytes.size()));
    DeployRecord record;
    record.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    record.targetPath = target;
    record.time = QDateTime::currentDateTimeUtc();
    record.deployedSha256 = hashOf(payload);
    record.deployedSize = payload.size();

    QByteArray previous;
    if (QFileInfo::exists(target)) {
        if (mode == DeployMode::createOnly) {
            return std::unexpected(QCoreApplication::translate("eggbert::Files", "%1 already exists.").arg(target));
        }
        const auto read = readAll(target);
        if (!read) {
            return std::unexpected(
                QCoreApplication::translate("eggbert::Files", "%1 cannot be read, so it cannot be backed up.").arg(target));
        }
        previous = *read;
        record.hadPrevious = true;
        record.previousSha256 = hashOf(previous);
        record.previousSize = previous.size();
        const auto backup = backupPath(record.id);
        const bool written = !fails("backup") && writeAtomically(backup, previous);
        const auto check = written ? readAll(backup) : std::nullopt;
        if (!check || *check != previous) {
            QFile::remove(backup);
            return std::unexpected(
                QCoreApplication::translate("eggbert::Files", "The backup of %1 could not be written and verified.").arg(target));
        }
    }
    const auto undo = [&](QString reason) -> std::expected<DeployRecord, QString> {
        const bool restored = !fails("restore") &&
            (record.hadPrevious ? writeAtomically(target, previous) && readAll(target) == previous
                                : QFile::remove(target) && !QFileInfo::exists(target));
        if (!restored) {
            reason += record.hadPrevious
                ? QCoreApplication::translate("eggbert::Files", " Recovery failed for %1. Original bytes are in %2.").arg(target, backupPath(record.id))
                : QCoreApplication::translate("eggbert::Files", " The newly written file could not be removed: %1.").arg(target);
        } else {
            QFile::remove(backupPath(record.id));
            reason += QCoreApplication::translate("eggbert::Files", " The previous state was restored.");
        }
        return std::unexpected(reason);
    };
    bool written = false;
    if (!fails("write")) {
        if (mode == DeployMode::createOnly) {
            // QFile's rename refuses to replace an existing target. Stage beside it
            // so readers only see the finished world when it is published.
            QTemporaryFile staged(data.filePath(QStringLiteral(".eggbert-world-XXXXXX")));
            if (staged.open() && staged.write(payload) == payload.size() && staged.flush()) {
                staged.close();
                // Check again for a case variant added during staging on a case-sensitive disk.
                const auto names = QDir(dataDirectory).entryList(QDir::Files | QDir::System);
                written = !names.contains(fileName, Qt::CaseInsensitive) && staged.rename(target);
                if (written) {
                    staged.setAutoRemove(false);
                }
            }
        } else {
            written = writeAtomically(target, payload);
        }
    }
    if (!written) {
        QFile::remove(backupPath(record.id));
        return std::unexpected(QCoreApplication::translate("eggbert::Files", "%1 could not be written.").arg(target));
    }
    const auto check = fails("verify") ? std::nullopt : readAll(target);
    if (!check || *check != payload) {
        return undo(QCoreApplication::translate("eggbert::Files", "%1 did not read back as written.").arg(target));
    }
    QJsonObject json;
    json[QStringLiteral("id")] = record.id;
    json[QStringLiteral("target")] = record.targetPath;
    json[QStringLiteral("time")] = record.time.toString(Qt::ISODateWithMs);
    json[QStringLiteral("deployedSha256")] = QString::fromLatin1(record.deployedSha256.toHex());
    json[QStringLiteral("deployedSize")] = static_cast<double>(record.deployedSize);
    json[QStringLiteral("hadPrevious")] = record.hadPrevious;
    json[QStringLiteral("previousSha256")] = QString::fromLatin1(record.previousSha256.toHex());
    json[QStringLiteral("previousSize")] = static_cast<double>(record.previousSize);
    if (fails("record") ||
        !writeAtomically(recordPath(record.id), QJsonDocument(json).toJson())) {
        return undo(QCoreApplication::translate("eggbert::Files", "The deploy of %1 could not be recorded.").arg(target));
    }
    return record;
}

std::vector<DeployRecord> DeployStore::records() const {
    std::vector<DeployRecord> result;
    const QDir directory(directory_);
    for (const auto &name : directory.entryList({QStringLiteral("*.json")}, QDir::Files)) {
        const auto bytes = readAll(directory.filePath(name));
        if (!bytes) {
            continue;
        }
        const auto json = QJsonDocument::fromJson(*bytes).object();
        DeployRecord record;
        record.id = json[QStringLiteral("id")].toString();
        record.targetPath = json[QStringLiteral("target")].toString();
        record.time =
            QDateTime::fromString(json[QStringLiteral("time")].toString(), Qt::ISODateWithMs);
        record.deployedSha256 =
            QByteArray::fromHex(json[QStringLiteral("deployedSha256")].toString().toLatin1());
        record.deployedSize = static_cast<qint64>(json[QStringLiteral("deployedSize")].toDouble());
        record.hadPrevious = json[QStringLiteral("hadPrevious")].toBool();
        record.previousSha256 =
            QByteArray::fromHex(json[QStringLiteral("previousSha256")].toString().toLatin1());
        record.previousSize = static_cast<qint64>(json[QStringLiteral("previousSize")].toDouble());
        // The file name is the identifier: a record that says otherwise, or
        // that lost its kept bytes, is not offered.
        if (record.id + QStringLiteral(".json") != name || record.targetPath.isEmpty() ||
            !record.time.isValid() || record.deployedSha256.size() != 32 ||
            (record.hadPrevious && !QFileInfo::exists(backupPath(record.id)))) {
            continue;
        }
        result.push_back(std::move(record));
    }
    std::ranges::sort(result, [](const DeployRecord &a, const DeployRecord &b) {
        return a.time != b.time ? a.time > b.time : a.id > b.id;
    });
    return result;
}

std::expected<void, QString> DeployStore::rollBack(const DeployRecord &record,
                                                   bool evenIfChanged) {
    const auto current = readAll(record.targetPath);
    const bool unchanged = current && hashOf(*current) == record.deployedSha256;
    if (!unchanged && !evenIfChanged) {
        return std::unexpected(
            QCoreApplication::translate("eggbert::Files", "%1 no longer holds what was deployed (it was changed or removed "
                           "since), so it was left alone.")
                .arg(record.targetPath));
    }
    if (record.hadPrevious) {
        const auto previous = readAll(backupPath(record.id));
        if (!previous || hashOf(*previous) != record.previousSha256 ||
            previous->size() != record.previousSize) {
            return std::unexpected(
                QCoreApplication::translate("eggbert::Files", "The kept copy of %1 is missing or damaged; nothing was changed.")
                    .arg(record.targetPath));
        }
        if (fails("restore") || !writeAtomically(record.targetPath, *previous)) {
            return std::unexpected(
                QCoreApplication::translate("eggbert::Files", "%1 could not be written; it is as it was.").arg(record.targetPath));
        }
        const auto check = readAll(record.targetPath);
        if (!check || *check != *previous) {
            return std::unexpected(
                QCoreApplication::translate("eggbert::Files", "%1 did not read back as restored.").arg(record.targetPath));
        }
    } else if (QFileInfo::exists(record.targetPath) &&
               (fails("restore") || !QFile::remove(record.targetPath))) {
        return std::unexpected(
            QCoreApplication::translate("eggbert::Files", "%1 could not be removed; it is as it was.").arg(record.targetPath));
    }
    forget(record);
    return {};
}

bool DeployStore::forget(const DeployRecord &record) {
    QFile::remove(backupPath(record.id));
    return QFile::remove(recordPath(record.id));
}

} // namespace eggbert
