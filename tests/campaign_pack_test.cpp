// SPDX-License-Identifier: GPL-3.0-only
//
// Campaign packs: the manifest round-trips, archives are read back exactly,
// and hostile or damaged input is refused before anything is used.

#include "app/campaign_pack.hpp"
#include "world/text_world_image.hpp"

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

// A ZIP written by another program (Python's zipfile, zlib 1.3) with two
// Deflate members and a stored one: notes.txt (the 8,100 bytes of
// `worldList()`), empty.bin (nothing) and kept.bin. Test data, not code.
const char *const deflatedZip =
    "504b030414000000080000002150017f874168030000a41f0000090000006e6f7465732e747874ed98418e18210c04bf"
    "04c618a37c27c74891f6b2df8f94a53d1bea0591e63662c060b70d257ffefef8f5b3b5f6e3f3eba3ebc3f5b1cf478ff3"
    "61f37c0c8db8e68456e5b1d365b99b46fccce97156f57dec982cdbd4489e39c3ceaa11c78ecbb2bb46f2cc997656cd75"
    "ec842c874696e62cad4ad9d9b2bcb557b3f24b679e38b3bc18f2cbbdce73ec842c87e2b3a2763fabf6b8e3dc15799316"
    "43eab8f49a6559234b7352ab3614ecdacbb4fbd0793cef332f7991f26bcbd3060587e2e38a5828862bee383745be4b8b"
    "21751c0a2e8d6ccd695a657ee7c6d45e4bbba7ced3a0e09017537eadf2d4ee7c36c5c715b1500c130a9a22efd2221e75"
    "aae264b99517b2ac5509054d7bb9768fca28d4e09017d34b0be5061474c52714b16d55b9779c43914f69d1a5ce8082a9"
    "91674efa65a772e3d94bbb3fe781822e2fcaafe67ef95ef9fcc4a7725e315c507028f2a5c5963a861a2c4dfb53a7b20c"
    "0587f6aafcd93a8fa1062b0fadee16799a50702a3e95f366b5ea8e73d5ceccca79ad82825b2355a7a1550d3558f5dead"
    "ee169d190a4e795177cba85c450dd61d9515b1aa0228e88a7cdd87f6d4d75d8375afe6a3b22c4341d75e75875bdd09a8"
    "c17a0b766566dd365030149f7a77a6d5dd7bc7b9de2fcb8a985e2b28d834526fe5ceaa823b37eacd4dabccd499a160c8"
    "8b7adfa35e7cd46071c22c72500c27149c157969311f4ab96bf0dbc84d32130a4e90cc04c90448264032010517482641"
    "321b24b341320d0a76908c81641c24334132010517486683643a48c640320e050324932099069231908c43c100c92448"
    "a6836406486642c104c974908c83640224b3a1a0816426486683640c2433a16082640c241320990d9219507081643a48"
    "66826436486640c10592e9209900c934908c43c10d927190cc06c90c904c42c1019249908c83643648c6a1e006c94c90"
    "4c07c90448c6a06082641c24d340320192312898201907c97490cc02c90c28d840320b24e3209906925950d041321d24"
    "b3413201921950b0816412241320990192e95070836416486682640648a643c10d9259209900c93848c6a06007c93490"
    "cc06c92c904c40c1099271908c83640648c6a06007c974904c07c934904c83820d24d340326f4fe67b26bc3d99efb7c4"
    "db93f9787b3227086f4fe6edc9fcfdf5f664de9eccd7166f4fe6edc9fc9bf36f4fe6e3edc91ce1de9eccffd693f90350"
    "4b03041400000008000000215000000000020000000000000009000000656d7074792e62696e0300504b030414000000"
    "0000000021500f36a2260d0000000d000000080000006b6570742e62696e6b657074206173206974206973504b010214"
    "0314000000080000002150017f874168030000a41f00000900000000000000000000008001000000006e6f7465732e74"
    "7874504b010214031400000008000000215000000000020000000000000009000000000000000000000080018f030000"
    "656d7074792e62696e504b01021403140000000000000021500f36a2260d0000000d0000000800000000000000000000"
    "008001b80300006b6570742e62696e504b05060000000003000300a4000000eb0300000000";

QByteArray worldList() {
    QByteArray text;
    for (int index = 0; index < 900; ++index) {
        text += QByteArray("world") +
                QByteArray::number((index * index) % 1000).rightJustified(3, '0') + ";";
    }
    return text;
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

    // A ZIP compressed by another program is read.
    {
        const auto foreign = QByteArray::fromHex(deflatedZip);
        const auto members = readZip(foreign);
        CHECK(members.has_value());
        CHECK_EQ(members->size(), std::size_t{3});
        CHECK((*members)[0].name == "notes.txt" && (*members)[0].bytes == worldList());
        CHECK((*members)[1].name == "empty.bin" && (*members)[1].bytes.isEmpty());
        CHECK((*members)[2].name == "kept.bin" && (*members)[2].bytes == "kept as it is");
        // The directory entry of notes.txt: its stated size is what may be
        // produced. A smaller one stops the decompression, a larger one is
        // not reached, and a changed byte of the data fails the checksum or
        // the decompression.
        const auto entry = foreign.indexOf(QByteArray::fromHex("504b0102"));
        CHECK(entry > 0);
        CHECK(static_cast<quint8>(foreign[entry + 24]) == 0xA4 &&
              static_cast<quint8>(foreign[entry + 25]) == 0x1F); // 8100
        auto smaller = foreign;
        smaller[entry + 24] = static_cast<char>(0xA3);
        CHECK(errorOf(readZip(smaller)).contains("more than its stated size"));
        auto larger = foreign;
        larger[entry + 24] = static_cast<char>(0xA5);
        CHECK(errorOf(readZip(larger)).contains("less than its stated size"));
        auto huge = foreign;
        huge[entry + 26] = static_cast<char>(0x7F); // about 8 MiB stated
        CHECK(errorOf(readZip(huge)).contains("too large"));
        int refused = 0;
        for (qsizetype offset = 39; offset < 39 + 852; offset += 7) {
            auto changed = foreign;
            changed[offset] = static_cast<char>(changed[offset] ^ 0x10);
            const auto result = readZip(changed);
            CHECK(!result.has_value());
            refused += result.has_value() ? 0 : 1;
        }
        CHECK(refused > 100);
    }

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
        // Deflate is read; data that is not a Deflate stream and other
        // methods (12 is bzip2) are refused.
        CHECK(errorOf(readZip(rawZip({{"a.blp", data, 8}}))).contains("cannot be decompressed"));
        CHECK(errorOf(readZip(rawZip({{"a.blp", data, 12}}))).contains("other than Deflate"));
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

        auto damaged = pack;
        damaged.worlds.back().bytes.append('x');
        const auto incomplete = dir.filePath("incomplete-pack");
        CHECK(!writePack(damaged, incomplete, false));
        CHECK(!QFileInfo::exists(incomplete));
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
        // A link is refused as a link where the system has symbolic links;
        // elsewhere what QFile::link makes is at least not accepted.
        const auto linkPath = QDir(folder).filePath("link.blp");
        if (QFile::link(zipPath, linkPath)) {
            CHECK(!readPack(folder).has_value());
            if (QFileInfo(linkPath).isSymLink()) {
                CHECK(errorOf(readPack(folder)).contains("a link"));
            }
            QFile::remove(linkPath);
        }
        CHECK(!readPack(dir.filePath("missing.zip")).has_value());
        CHECK(!readPack(dir.filePath("missing-folder")).has_value());
    }
    // A pack of Mobile Eggbert worlds: text files under the port's names.
    {
        const auto text = QByteArray::fromStdString(newTextWorld());
        auto other = text;
        other.replace("music=1", "music=4");
        const auto port = makePack("Port", GameProfile::mobileEggbert,
                                   {{false, 1, 1}, {false, 1, 10}}, {text, other});
        CHECK(port.has_value());
        if (port) {
            CHECK(port->campaign.game == GameProfile::mobileEggbert);
            CHECK_EQ(port->worlds.size(), std::size_t{2});
            CHECK_EQ(port->worlds[1].name, QString("world010.txt"));
            CHECK(port->worlds[1].bytes == other);
            CHECK(port->campaign.entries[0].title.empty());
            const auto manifest = writePackManifest(port->campaign);
            CHECK(manifest.contains("\"mobile-eggbert\""));
            const auto again = readPackManifest(manifest);
            CHECK(again.has_value() && *again == port->campaign);
            // Through a ZIP file and a folder, byte for byte.
            const auto zipPath = dir.filePath("port.zip");
            CHECK(writePack(*port, zipPath, true).has_value());
            const auto fromZip = readPack(zipPath);
            CHECK(fromZip.has_value() && fromZip->campaign == port->campaign &&
                  fromZip->worlds == port->worlds);
            // A .blp world under a text name, or text that is not a world.
            std::vector<PackFile> files{{packManifestName, manifest},
                                        {"world001.txt", text},
                                        {"world010.txt", other}};
            CHECK(assemblePack(files).has_value());
        }
        // The port has no designed worlds, and its worlds are text.
        CHECK(!makePack("Port", GameProfile::mobileEggbert, {{true, 1, 1}}, {text}).has_value());
        CHECK(errorOf(makePack("Port", GameProfile::mobileEggbert, {{false, 1, 1}}, {worldA}))
                  .contains("not a Mobile Eggbert world"));
        CHECK(errorOf(makePack("Port", GameProfile::mobileEggbert, {{false, 1, 1}},
                               {QByteArray("DescFile: nothing \n")}))
                  .contains("not a Mobile Eggbert world"));
        // And a text world is not a world of the Windows games.
        CHECK(errorOf(makePack("Two", GameProfile::speedyBlupi2, {{false, 1, 1}}, {text}))
                  .contains("not a complete world"));
    }
    return test::finish("campaign_pack");
}
