// SPDX-License-Identifier: GPL-3.0-only
//
// Campaign packs: the manifest round-trips, archives are read back exactly,
// and hostile or damaged input is refused before anything is used.

#include "app/campaign_pack.hpp"

#include "check.hpp"
#include "fixture.hpp"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

using namespace eggbert;
using namespace eggbert::test;

namespace {

QByteArray raw(const Bytes &bytes) {
    return QByteArray(reinterpret_cast<const char *>(bytes.data()),
                      static_cast<qsizetype>(bytes.size()));
}

void put16(QByteArray &out, quint32 value) {
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
}

void put32(QByteArray &out, quint32 value) {
    put16(out, value & 0xFFFF);
    put16(out, value >> 16);
}

quint32 crcOf(const QByteArray &bytes) {
    quint32 crc = 0xFFFFFFFFu;
    for (const char byte : bytes) {
        crc ^= static_cast<quint8>(byte);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 1u) != 0 ? 0xEDB88320u ^ (crc >> 1) : crc >> 1;
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

// An archive member as a hostile or foreign tool might write it.
struct RawMember {
    QByteArray name;
    QByteArray data;
    quint32 method = 0;
    quint32 flags = 0;
    // Added to the true checksum.
    quint32 crcError = 0;
    // Declared size; -1 for the real one.
    qint64 declaredSize = -1;
};

// Builds a ZIP with exactly these members, written independently of the
// editor's writer.
QByteArray rawZip(const std::vector<RawMember> &members, quint32 countOverride = 0,
                  const QByteArray &comment = {}) {
    QByteArray archive;
    QByteArray central;
    for (const auto &member : members) {
        const auto size =
            member.declaredSize < 0 ? static_cast<quint32>(member.data.size())
                                    : static_cast<quint32>(member.declaredSize);
        const auto crc = crcOf(member.data) + member.crcError;
        const auto offset = static_cast<quint32>(archive.size());
        put32(archive, 0x04034B50u);
        put16(archive, 20);
        put16(archive, member.flags);
        put16(archive, member.method);
        put32(archive, 0);
        put32(archive, crc);
        put32(archive, size);
        put32(archive, size);
        put16(archive, static_cast<quint32>(member.name.size()));
        put16(archive, 0);
        archive.append(member.name);
        archive.append(member.data);
        put32(central, 0x02014B50u);
        put16(central, 20);
        put16(central, 20);
        put16(central, member.flags);
        put16(central, member.method);
        put32(central, 0);
        put32(central, crc);
        put32(central, size);
        put32(central, size);
        put16(central, static_cast<quint32>(member.name.size()));
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put32(central, 0);
        put32(central, offset);
        central.append(member.name);
    }
    const auto centralOffset = static_cast<quint32>(archive.size());
    archive.append(central);
    const auto count = countOverride != 0 ? countOverride : static_cast<quint32>(members.size());
    put32(archive, 0x06054B50u);
    put16(archive, 0);
    put16(archive, 0);
    put16(archive, count);
    put16(archive, count);
    put32(archive, static_cast<quint32>(central.size()));
    put32(archive, centralOffset);
    put16(archive, static_cast<quint32>(comment.size()));
    archive.append(comment);
    return archive;
}

QString errorOf(const std::expected<std::vector<PackFile>, QString> &result) {
    return result ? QString() : result.error();
}

QString errorOf(const std::expected<CampaignPack, QString> &result) {
    return result ? QString() : result.error();
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;

    auto titled = blankWorld(fxRevision13);
    putText(titled, 376, "The cellar");
    const auto worldA = raw(titled);
    const auto worldB = raw(patternedWorld(fxRevision12));
    const auto worldC = raw(blankWorld(fxRevision13, 5)); // with bytes after the world

    // Building a pack fills in names, sizes, checksums and titles.
    const auto made = makePack("My campaign é", GameProfile::speedyBlupi2,
                               {{false, 1, 1}, {false, 1, 10}, {true, 2, 7}},
                               {worldA, worldB, worldC});
    CHECK(made.has_value());
    const auto &pack = *made;
    CHECK_EQ(pack.campaign.entries.size(), std::size_t{3});
    CHECK_EQ(pack.campaign.entries[0].fileName, std::string("world001.blp"));
    CHECK_EQ(pack.campaign.entries[2].fileName, std::string("u002-007.blp"));
    CHECK_EQ(pack.campaign.entries[0].title, std::string("The cellar"));
    CHECK(pack.campaign.entries[1].title.empty()); // revision 1.2 stores none the games read
    CHECK_EQ(pack.campaign.entries[2].size, std::uint64_t{50'473});
    CHECK_EQ(pack.campaign.entries[1].sha256,
             QCryptographicHash::hash(worldB, QCryptographicHash::Sha256).toHex().toStdString());
    CHECK(pack.worlds[1].bytes == worldB);

    // The manifest round-trips and is stable.
    const auto manifest = writePackManifest(pack.campaign);
    CHECK(manifest == writePackManifest(pack.campaign));
    const auto read = readPackManifest(manifest);
    CHECK(read.has_value());
    CHECK(read && *read == pack.campaign);
    CHECK(read && writePackManifest(*read) == manifest);
    CHECK(manifest.contains("\"format\": \"eggbert-campaign\""));
    CHECK(manifest.contains("\"game\": \"speedy-blupi-2\""));

    // Manifests that are refused.
    {
        const auto refuse = [&](QByteArray text, const char *from, const char *to) {
            CHECK(text.contains(from));
            text.replace(from, to);
            const auto result = readPackManifest(text);
            return result ? QString() : result.error();
        };
        CHECK(!readPackManifest("not json").has_value());
        CHECK(!readPackManifest("[1, 2]").has_value());
        CHECK(refuse(manifest, "eggbert-campaign", "something-else").contains("not a campaign"));
        CHECK(refuse(manifest, "\"version\": 1", "\"version\": 2").contains("format version"));
        CHECK(refuse(manifest, "speedy-blupi-2", "speedy-blupi-9").contains("which game"));
        CHECK(refuse(manifest, "\"file\": \"world010.blp\"", "\"file\": \"../world010.blp\"")
                  .contains("not its slot's"));
        CHECK(refuse(manifest, "\"file\": \"world010.blp\"", "\"file\": \"world011.blp\"")
                  .contains("not its slot's"));
        CHECK(refuse(manifest, "\"player\": 2", "\"player\": 9").contains("slot the games use"));
        CHECK(refuse(manifest, "\"kind\": \"designed\"", "\"kind\": \"secret\"")
                  .contains("no valid kind"));
        CHECK(refuse(manifest, "\"size\": 50473", "\"size\": 99999999").contains("size or checksum"));
        CHECK(refuse(manifest, QByteArray(pack.campaign.entries[0].sha256.c_str()).constData(),
                     "zz")
                  .contains("size or checksum"));
        CHECK(readPackManifest("{\"format\":\"eggbert-campaign\",\"version\":1,"
                               "\"game\":\"speedy-blupi-1\",\"worlds\":[]}")
                  .error()
                  .contains("lists no worlds"));
    }

    // Member names.
    for (const auto *good : {"world001.blp", "u001-020.blp", "eggbert-campaign.json", "A_b-1.x"}) {
        CHECK(validPackMemberName(good));
    }
    for (const auto *bad : {"", ".hidden", "../x", "a/b", "a\\b", "C:x", "/etc/passwd", "a..b",
                            "name with space", "café.blp", "a\nb"}) {
        CHECK(!validPackMemberName(QString::fromUtf8(bad)));
    }
    CHECK(!validPackMemberName(QString(65, 'a')));

    // The archive: written, read back exactly, and readable when produced by
    // another writer (with a comment after the end record).
    std::vector<PackFile> files{{packManifestName, manifest}};
    files.insert(files.end(), pack.worlds.begin(), pack.worlds.end());
    const auto archive = writeZip(files);
    CHECK(archive.has_value());
    CHECK(archive && *archive == *writeZip(files)); // stable bytes
    const auto members = readZip(*archive);
    CHECK(members.has_value());
    CHECK(members && *members == files);
    {
        std::vector<RawMember> rawMembers;
        for (const auto &file : files) {
            rawMembers.push_back({file.name.toLatin1(), file.bytes});
        }
        const auto foreign = readZip(rawZip(rawMembers, 0, "made elsewhere"));
        CHECK(foreign.has_value());
        CHECK(foreign && *foreign == files);
        // And a pack assembles from it.
        CHECK(assemblePack(*foreign).has_value());
    }
    CHECK(!writeZip({{"../x", "a"}}).has_value());
    CHECK(!writeZip({{"a.blp", "a"}, {"A.BLP", "b"}}).has_value());
    CHECK(readZip(*writeZip({})).has_value()); // an empty archive is an archive

    // Archives that are refused, each for its reason.
    {
        const QByteArray data("0123456789");
        CHECK(errorOf(readZip("not a zip at all")).contains("not a ZIP"));
        CHECK(errorOf(readZip(QByteArray())).contains("not a ZIP"));
        for (const auto *name : {"../escape.blp", "/abs.blp", "dir/inner.blp", "back\\slash.blp",
                                 "C:evil.blp", ".profile", "trailing ", ""}) {
            CHECK(errorOf(readZip(rawZip({{name, data}}))).contains("not allowed"));
        }
        CHECK(errorOf(readZip(rawZip({{"a.blp", data}, {"A.blp", data}})))
                  .contains("more than once"));
        CHECK(errorOf(readZip(rawZip({{"a.blp", data, 8}}))).contains("compressed"));
        CHECK(errorOf(readZip(rawZip({{"a.blp", data, 0, 1}}))).contains("encrypted"));
        CHECK(errorOf(readZip(rawZip({{"a.blp", data, 0, 0, 1}}))).contains("checksum"));
        CHECK(errorOf(readZip(rawZip({{"a.blp", data, 0, 0, 0, 5000}})))
                  .contains("does not lie inside"));
        CHECK(errorOf(readZip(rawZip({{"a.blp", data, 0, 0, 0, 0xFFFFFFFFll}})))
                  .contains("too large"));
        CHECK(errorOf(readZip(rawZip({{"a.blp", data, 0, 0, 0, 2 * 1024 * 1024}})))
                  .contains("too large"));
        CHECK(errorOf(readZip(rawZip({{"a.blp", data}}, 0xFFFF))).contains("ZIP64"));
        CHECK(errorOf(readZip(rawZip({{"a.blp", data}}, 5000))).contains("more members"));
        CHECK(errorOf(readZip(rawZip({{"a.blp", data}}, 3))).contains("damaged"));
        // Cut anywhere, the archive is refused (never read past its end).
        const auto whole = rawZip({{"a.blp", data}, {"b.blp", data}});
        for (qsizetype length = 0; length < whole.size(); ++length) {
            CHECK(!readZip(whole.left(length)).has_value());
        }
        // A directory offset pointing outside.
        auto moved = whole;
        moved[moved.size() - 6] = '\x7F';
        CHECK(!readZip(moved).has_value());
        // A local header that does not match its directory entry.
        auto renamed = whole;
        renamed[30] = 'z';
        CHECK(errorOf(readZip(renamed)).contains("damaged header"));
    }

    // Packs that are refused when assembled.
    {
        const auto without = [&](const QString &name) {
            auto copy = files;
            std::erase_if(copy, [&](const PackFile &file) { return file.name == name; });
            return copy;
        };
        CHECK(errorOf(assemblePack(without(packManifestName))).contains("no manifest"));
        CHECK(errorOf(assemblePack(without("world010.blp"))).contains("not in the pack"));
        auto extra = files;
        extra.push_back({"notes.txt", "hello"});
        CHECK(errorOf(assemblePack(extra)).contains("does not list"));
        auto changed = files;
        changed[2].bytes[1000] = 'x';
        CHECK(errorOf(assemblePack(changed)).contains("size or checksum differs"));
        auto shorter = files;
        shorter[2].bytes.chop(1);
        CHECK(errorOf(assemblePack(shorter)).contains("size or checksum differs"));
        auto upper = files;
        upper[1].name = "WORLD001.BLP"; // the manifest says world001.blp
        CHECK(errorOf(assemblePack(upper)).contains("not in the pack"));
        auto badName = files;
        badName.push_back({"../x", "y"});
        CHECK(errorOf(assemblePack(badName)).contains("not a name a pack may contain"));
        // A file that is not a world, correctly listed, is still refused.
        const auto junk = makePack("Junk", GameProfile::speedyBlupi1, {{true, 1, 1}},
                                   {QByteArray(300, 'j')});
        CHECK(errorOf(junk).contains("not a complete world"));
        CHECK(errorOf(makePack("Cut", GameProfile::speedyBlupi1, {{true, 1, 1}},
                               {worldA.left(40'000)}))
                  .contains("not a complete world"));
        CHECK(!makePack("None", GameProfile::speedyBlupi1, {}, {}).has_value());
        CHECK(!makePack("Bad slot", GameProfile::speedyBlupi1, {{true, 9, 1}}, {worldA})
                   .has_value());
        CHECK(!makePack("Twice", GameProfile::speedyBlupi1, {{true, 1, 1}, {true, 1, 1}},
                        {worldA, worldA})
                   .has_value());
    }

    // On disk, as an archive and as a directory.
    {
        const auto zipPath = dir.filePath("pack.zip");
        CHECK(writePack(pack, zipPath, true).has_value());
        const auto fromZip = readPack(zipPath);
        CHECK(fromZip.has_value());
        CHECK(fromZip && fromZip->campaign == pack.campaign && fromZip->worlds == pack.worlds);
        CHECK(!writePack(pack, zipPath, true).has_value()); // never over an existing file

        const auto folder = dir.filePath("pack-folder");
        CHECK(writePack(pack, folder, false).has_value());
        CHECK_EQ(QDir(folder).entryList(QDir::Files).size(), qsizetype{4});
        const auto fromFolder = readPack(folder);
        CHECK(fromFolder.has_value());
        CHECK(fromFolder && fromFolder->campaign == pack.campaign &&
              fromFolder->worlds == pack.worlds);
        CHECK(!writePack(pack, folder, false).has_value());

        // A directory with a stray file or a link is not a pack.
        QFile stray(QDir(folder).filePath("stray.txt"));
        CHECK(stray.open(QIODevice::WriteOnly));
        stray.write("x");
        stray.close();
        CHECK(errorOf(readPack(folder)).contains("does not list"));
        stray.remove();
        CHECK(readPack(folder).has_value());
        if (QFile::link(zipPath, QDir(folder).filePath("link.blp"))) {
            CHECK(errorOf(readPack(folder)).contains("a link"));
            QFile::remove(QDir(folder).filePath("link.blp"));
        }
        CHECK(!readPack(dir.filePath("missing.zip")).has_value());
        CHECK(!readPack(dir.filePath("missing-folder")).has_value());
    }
    return test::finish("campaign_pack");
}
