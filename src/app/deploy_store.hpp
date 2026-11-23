// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include <cstddef>
#include <expected>
#include <functional>
#include <span>
#include <vector>

namespace eggbert {

// One world put into an installation.
struct DeployRecord {
    // Identifier inside the store.
    QString id;
    // The file that was written, inside the installation's data directory.
    QString targetPath;
    QDateTime time;
    QByteArray deployedSha256;
    qint64 deployedSize = 0;
    // What the target held before; false when the file did not exist.
    bool hadPrevious = false;
    QByteArray previousSha256;
    qint64 previousSize = 0;

    friend bool operator==(const DeployRecord &, const DeployRecord &) = default;
};

// Puts world bytes into a game installation and can take them out again.
//
// The file being replaced is first copied, verified, into a per-user
// directory (never into the installation); the new bytes then replace the
// target in one step through a temporary file in the same directory and are
// read back. If any step fails the installation holds what it held before.
// A rollback restores the kept bytes the same way (or removes a file that
// did not exist before) and verifies them.
class DeployStore {
public:
    static constexpr qint64 maximumFileBytes = 64 * 1024 * 1024;

    explicit DeployStore(QString directory = defaultDirectory());
    [[nodiscard]] static QString defaultDirectory();
    [[nodiscard]] const QString &directory() const { return directory_; }

    // `fileName` is created in `dataDirectory`, or replaces the entry of
    // that name there whatever its letter case. The error says which step
    // failed; nothing in the installation has changed then.
    std::expected<DeployRecord, QString> deploy(const QString &dataDirectory,
                                                const QString &fileName,
                                                std::span<const std::byte> bytes);

    // Deploys not rolled back yet, newest first.
    [[nodiscard]] std::vector<DeployRecord> records() const;

    // Refused when the target no longer holds what was deployed (the game
    // or the user changed it since), unless `evenIfChanged`.
    std::expected<void, QString> rollBack(const DeployRecord &record, bool evenIfChanged = false);
    // Forgets a deploy and its kept bytes without touching the installation.
    bool forget(const DeployRecord &record);

    // Test hook: called before each step with its name; returning true
    // makes that step fail. Steps: "backup", "write", "verify", "record",
    // "restore".
    std::function<bool(const char *step)> failAt;

private:
    [[nodiscard]] QString backupPath(const QString &id) const;
    [[nodiscard]] QString recordPath(const QString &id) const;
    [[nodiscard]] bool fails(const char *step) const { return failAt && failAt(step); }

    QString directory_;
};

} // namespace eggbert
