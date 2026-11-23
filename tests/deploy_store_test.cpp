// SPDX-License-Identifier: GPL-3.0-only
//
// Deploying into a generated installation directory: the previous file is
// kept outside the installation, the replacement and the rollback are
// byte-exact, and a failure at any step leaves the installation as it was.

#include "app/deploy_store.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <map>

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

QByteArray raw(const Bytes &bytes) {
    return QByteArray(reinterpret_cast<const char *>(bytes.data()),
                      static_cast<qsizetype>(bytes.size()));
}

// Every file under a directory with its content.
std::map<QString, QByteArray> snapshot(const QString &root) {
    std::map<QString, QByteArray> result;
    const QDir directory(root);
    for (const auto &name : directory.entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot)) {
        result[name] = readAll(directory.filePath(name));
    }
    return result;
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir root;
    const auto dataPath = QDir(root.path()).filePath("game/DATA");
    CHECK(QDir().mkpath(dataPath));
    const QDir data(dataPath);
    const auto storePath = QDir(root.path()).filePath("store");

    const auto oldWorld = raw(patternedWorld(fxRevision13));
    const auto newWorldBytes = blankWorld(fxRevision13);
    const auto newWorld = raw(newWorldBytes);
    writeAll(data.filePath("WORLD012.BLP"), oldWorld); // upper case, as some installations are
    writeAll(data.filePath("info001.blp"), QByteArray(528, 'i'));
    writeAll(data.filePath("config.def"), "CD-Rom=\r\n");
    const auto untouched = snapshot(dataPath);

    DeployStore store(storePath);
    CHECK(store.records().empty());

    // Replacing an existing world: its spelling is kept, the old bytes are
    // kept in the store, nothing else in the installation changes.
    const auto replaced = store.deploy(dataPath, "world012.blp", newWorldBytes);
    CHECK(replaced.has_value());
    CHECK(replaced->targetPath == data.filePath("WORLD012.BLP"));
    CHECK(replaced->hadPrevious);
    CHECK(replaced->previousSha256 ==
          QCryptographicHash::hash(oldWorld, QCryptographicHash::Sha256));
    CHECK_EQ(replaced->previousSize, oldWorld.size());
    CHECK(readAll(data.filePath("WORLD012.BLP")) == newWorld);
    {
        auto expected = untouched;
        expected["WORLD012.BLP"] = newWorld;
        CHECK(snapshot(dataPath) == expected); // no stray or temporary file
    }
    CHECK_EQ(store.records().size(), std::size_t{1});
    CHECK(store.records().front() == *replaced);

    // A new user world: nothing to keep.
    const auto created = store.deploy(dataPath, "u001-003.blp", newWorldBytes);
    CHECK(created.has_value());
    CHECK(!created->hadPrevious);
    CHECK(readAll(data.filePath("u001-003.blp")) == newWorld);
    CHECK_EQ(store.records().size(), std::size_t{2});
    CHECK(store.records().front().id == created->id || store.records()[1].id == created->id);

    // A second store object sees the same records (they are on disk).
    CHECK_EQ(DeployStore(storePath).records().size(), std::size_t{2});

    // Rolling back the new file removes it; rolling back the replacement
    // restores the old bytes exactly.
    CHECK(store.rollBack(*created).has_value());
    CHECK(!QFile::exists(data.filePath("u001-003.blp")));
    CHECK(store.rollBack(*replaced).has_value());
    CHECK(snapshot(dataPath) == untouched);
    CHECK(store.records().empty());
    CHECK(QDir(storePath).entryList(QDir::Files).isEmpty()); // kept bytes are gone too

    // A file changed since the deploy is left alone unless asked.
    {
        const auto record = store.deploy(dataPath, "world012.blp", newWorldBytes);
        CHECK(record.has_value());
        auto changed = newWorld;
        changed[900] = 'x';
        writeAll(data.filePath("WORLD012.BLP"), changed);
        const auto refused = store.rollBack(*record);
        CHECK(!refused.has_value() && refused.error().contains("no longer holds"));
        CHECK(readAll(data.filePath("WORLD012.BLP")) == changed);
        CHECK_EQ(store.records().size(), std::size_t{1});
        CHECK(store.rollBack(*record, true).has_value());
        CHECK(snapshot(dataPath) == untouched);
    }
    // Kept bytes that were damaged are not written back.
    {
        const auto record = store.deploy(dataPath, "world012.blp", newWorldBytes);
        CHECK(record.has_value());
        const auto kept = QDir(storePath).filePath(record->id + ".previous");
        auto damaged = readAll(kept);
        damaged[10] = static_cast<char>(damaged[10] ^ 0x55);
        writeAll(kept, damaged);
        const auto refused = store.rollBack(*record);
        CHECK(!refused.has_value() && refused.error().contains("missing or damaged"));
        CHECK(readAll(data.filePath("WORLD012.BLP")) == newWorld);
        writeAll(kept, oldWorld);
        CHECK(store.rollBack(*record).has_value());
        CHECK(snapshot(dataPath) == untouched);
    }
    // Forgetting a deploy leaves the installation as deployed.
    {
        const auto record = store.deploy(dataPath, "world012.blp", newWorldBytes);
        CHECK(record.has_value() && store.forget(*record));
        CHECK(store.records().empty());
        CHECK(readAll(data.filePath("WORLD012.BLP")) == newWorld);
        writeAll(data.filePath("WORLD012.BLP"), oldWorld);
    }

    // A failure at any step of a deploy changes nothing in the installation
    // and leaves nothing in the store.
    for (const auto *step : {"backup", "write", "verify", "record"}) {
        for (const auto *name : {"world012.blp", "u002-001.blp"}) {
            DeployStore failing(storePath);
            failing.failAt = [step](const char *at) { return QByteArray(at) == step; };
            const auto result = failing.deploy(dataPath, name, newWorldBytes);
            if (QByteArray(step) == "backup" && QByteArray(name) == "u002-001.blp") {
                // Nothing to back up, so that step is not reached.
                CHECK(result.has_value());
                CHECK(failing.rollBack(*result).has_value());
            } else {
                CHECK(!result.has_value());
                CHECK(!result.error().isEmpty());
            }
            CHECK(snapshot(dataPath) == untouched);
            CHECK(failing.records().empty());
            CHECK(QDir(storePath).entryList(QDir::Files).isEmpty());
        }
    }
    {
        DeployStore failing(storePath);
        failing.failAt = [](const char *step) {
            return QByteArray(step) == "record" || QByteArray(step) == "restore";
        };
        const auto result = failing.deploy(dataPath, "world012.blp", newWorldBytes);
        CHECK(!result);
        const auto backups = QDir(storePath).entryList({"*.previous"}, QDir::Files);
        CHECK_EQ(backups.size(), 1);
        if (!backups.isEmpty()) {
            const auto path = QDir(storePath).filePath(backups.front());
            CHECK(result.error().contains(path));
            CHECK(readAll(path) == oldWorld);
            QFile::remove(path);
        }
        writeAll(data.filePath("WORLD012.BLP"), oldWorld);
        CHECK(!store.deploy(dataPath, "world012.blp", newWorldBytes, DeployMode::createOnly));
        CHECK(snapshot(dataPath) == untouched);
    }
    // A failed restore leaves the deployed file and the record in place.
    {
        const auto record = store.deploy(dataPath, "world012.blp", newWorldBytes);
        CHECK(record.has_value());
        store.failAt = [](const char *at) { return QByteArray(at) == "restore"; };
        CHECK(!store.rollBack(*record).has_value());
        CHECK(readAll(data.filePath("WORLD012.BLP")) == newWorld);
        CHECK_EQ(store.records().size(), std::size_t{1});
        store.failAt = nullptr;
        CHECK(store.rollBack(*record).has_value());
        CHECK(snapshot(dataPath) == untouched);
    }

    // Another writer claims the free name between the scan and publication.
    {
        const auto raced = data.filePath("WORLD015.BLP");
        DeployStore racing(storePath);
        racing.failAt = [&](const char *step) {
            if (QByteArray(step) == "write") {
                writeAll(raced, "another writer");
            }
            return false;
        };
        CHECK(!racing.deploy(dataPath, "world015.blp", newWorldBytes, DeployMode::createOnly));
        CHECK(readAll(raced) == "another writer");
        QFile::remove(raced);
    }

    // Names that would leave the directory, and directories that are not
    // there, are refused before anything is written.
    CHECK(!store.deploy(dataPath, "../escape.blp", newWorldBytes).has_value());
    CHECK(!store.deploy(dataPath, "sub/world001.blp", newWorldBytes).has_value());
    CHECK(!store.deploy(dataPath, "", newWorldBytes).has_value());
    CHECK(!store.deploy(QDir(root.path()).filePath("nowhere"), "world001.blp", newWorldBytes)
               .has_value());
    CHECK(!QFile::exists(QDir(root.path()).filePath("game/escape.blp")));
    CHECK(snapshot(dataPath) == untouched);

    // Foreign files in the store are ignored.
    writeAll(QDir(storePath).filePath("garbage.json"), "{not json");
    writeAll(QDir(storePath).filePath("other.json"), "{\"id\":\"someone-else\"}");
    CHECK(store.records().empty());
    return test::finish("deploy_store");
}
