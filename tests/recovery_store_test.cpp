// SPDX-License-Identifier: GPL-3.0-only
#include "app/recovery_store.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace eggbert;
using namespace eggbert::test;

namespace {

QByteArray readAll(const QString &path) {
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void writeAll(const QString &path, const QByteArray &bytes) {
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write(bytes);
}

bool same(const QByteArray &stored, const Bytes &bytes) {
    return stored.size() == static_cast<qsizetype>(bytes.size()) &&
           std::equal(bytes.begin(), bytes.end(),
                      reinterpret_cast<const std::byte *>(stored.constData()));
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    const auto storeDir = dir.filePath("store");
    const auto world = dir.filePath("worlds/a.blp");
    const auto other = dir.filePath("worlds/b.blp");
    QDir().mkpath(dir.filePath("worlds"));
    const auto base = patternedWorld(fxRevision13);
    auto edited = base;
    put16(edited, fxCellOffset(fxForegroundOffset, 3, 70), 153);
    writeAll(world, QByteArray(reinterpret_cast<const char *>(base.data()),
                               static_cast<qsizetype>(base.size())));

    RecoveryStore store(storeDir);
    CHECK(store.autosaves().empty());
    CHECK(store.snapshots(world).empty());
    CHECK(store.damagedFiles().isEmpty());
    CHECK(!QDir(storeDir).exists()); // nothing is created until something is stored

    // An autosave: exact bytes, base size and checksum, one per world file.
    CHECK(store.writeAutosave(world, base, edited));
    auto autosaves = store.autosaves();
    CHECK_EQ(autosaves.size(), std::size_t{1});
    CHECK(autosaves[0].originalPath.endsWith("worlds/a.blp"));
    CHECK(!autosaves[0].snapshot);
    CHECK(autosaves[0].name.isEmpty());
    CHECK_EQ(autosaves[0].baseSize, static_cast<qint64>(base.size()));
    CHECK(autosaves[0].baseSha256 == RecoveryStore::sha256(base));
    CHECK_EQ(autosaves[0].payloadSize, static_cast<qint64>(edited.size()));
    CHECK(autosaves[0].saved.isValid());
    const auto stored = store.read(autosaves[0]);
    CHECK(stored.has_value());
    CHECK(same(*stored, edited));
    auto later = edited;
    put16(later, fxCellOffset(fxForegroundOffset, 4, 70), 7);
    CHECK(store.writeAutosave(world, base, later));
    CHECK_EQ(store.autosaves().size(), std::size_t{1});
    CHECK(same(*store.read(store.autosaves()[0]), later));
    CHECK(store.writeAutosave(other, base, base)); // a path that does not exist yet
    CHECK_EQ(store.autosaves().size(), std::size_t{2});
    // Nothing was written beside the world file.
    CHECK_EQ(QDir(dir.filePath("worlds")).entryList(QDir::Files).size(), 1);
    CHECK(same(readAll(world), base));
    store.removeAutosave(other);
    store.removeAutosave(other); // again: harmless
    CHECK_EQ(store.autosaves().size(), std::size_t{1});

    // Snapshots: named, newest first, per world file, bounded.
    CHECK(store.writeSnapshot(world, "  before the tower  ", base, edited));
    CHECK(store.writeSnapshot(world, "after", base, later));
    CHECK(store.writeSnapshot(other, "elsewhere", base, base));
    auto snapshots = store.snapshots(world);
    CHECK_EQ(snapshots.size(), std::size_t{2});
    CHECK_EQ(snapshots[0].name, QString("after"));
    CHECK_EQ(snapshots[1].name, QString("before the tower"));
    CHECK(snapshots[0].snapshot);
    CHECK(same(*store.read(snapshots[1]), edited));
    CHECK_EQ(store.snapshots(other).size(), std::size_t{1});
    CHECK_EQ(store.autosaves().size(), std::size_t{1}); // snapshots are not autosaves
    for (int index = 0; index < RecoveryStore::maximumSnapshotsPerFile + 5; ++index) {
        CHECK(store.writeSnapshot(world, QString("n%1").arg(index), base, edited));
    }
    snapshots = store.snapshots(world);
    CHECK_EQ(static_cast<int>(snapshots.size()), RecoveryStore::maximumSnapshotsPerFile);
    CHECK_EQ(snapshots.front().name,
             QString("n%1").arg(RecoveryStore::maximumSnapshotsPerFile + 4));
    CHECK(store.remove(snapshots.front()));
    CHECK_EQ(static_cast<int>(store.snapshots(world).size()),
             RecoveryStore::maximumSnapshotsPerFile - 1);

    // Damage: every kind is reported and never handed out.
    const auto good = store.autosaves()[0];
    const auto goodBytes = readAll(good.storePath);
    const auto damage = [&](const QByteArray &bytes) {
        writeAll(good.storePath, bytes);
        const bool listed = !store.autosaves().empty();
        const bool readable = store.read(good).has_value();
        const bool reported = store.damagedFiles().contains(good.storePath);
        writeAll(good.storePath, goodBytes);
        return !listed && !readable && reported;
    };
    CHECK(damage(goodBytes.left(goodBytes.size() - 1)));    // truncated
    CHECK(damage(goodBytes + QByteArray(1, 'x')));          // extra data
    CHECK(damage(QByteArray("something else entirely")));   // foreign file
    CHECK(damage(QByteArray()));                            // empty
    {
        auto flipped = goodBytes;
        flipped[flipped.size() - 10] = static_cast<char>(flipped[flipped.size() - 10] ^ 0x01);
        CHECK(damage(flipped)); // a payload bit: checksum fails
        auto header = goodBytes;
        header[20] = static_cast<char>(header[20] ^ 0x7F);
        CHECK(damage(header)); // the version field
    }
    CHECK(store.read(good).has_value()); // restored for the checks below
    // Only files inside the store are read or removed.
    auto outside = good;
    outside.storePath = world;
    CHECK(!store.read(outside).has_value());
    CHECK(!store.remove(outside));
    CHECK(QFile::exists(world));
    // Too large to store.
    CHECK(RecoveryStore::maximumPayloadBytes == 64 * 1024 * 1024);
    // A second store object sees the same content (it is all on disk).
    CHECK_EQ(RecoveryStore(storeDir).autosaves().size(), std::size_t{1});
    CHECK(RecoveryStore::defaultDirectory().endsWith("/recovery"));
    return test::finish("recovery_store");
}
