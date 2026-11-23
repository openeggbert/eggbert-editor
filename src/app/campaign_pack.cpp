// SPDX-License-Identifier: GPL-3.0-only
#include "app/campaign_pack.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QHash>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include <array>
#include <optional>
#include <span>

#include "app/inflate.hpp"
#include "world/world_probe.hpp"
#include "world/world_view.hpp"

namespace eggbert {

namespace {

QString text(const std::string &value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

std::unexpected<QString> fail(const QString &message) {
    return std::unexpected(message);
}

quint32 crc32(const QByteArray &bytes) {
    static const auto table = [] {
        std::array<quint32, 256> values{};
        for (quint32 index = 0; index < 256; ++index) {
            quint32 value = index;
            for (int bit = 0; bit < 8; ++bit) {
                value = (value & 1u) != 0 ? 0xEDB88320u ^ (value >> 1) : value >> 1;
            }
            values[index] = value;
        }
        return values;
    }();
    quint32 crc = 0xFFFFFFFFu;
    for (const char byte : bytes) {
        crc = table[(crc ^ static_cast<quint8>(byte)) & 0xFFu] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

void put16(QByteArray &out, quint32 value) {
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
}

void put32(QByteArray &out, quint32 value) {
    put16(out, value & 0xFFFF);
    put16(out, value >> 16);
}

// Bounded little-endian reads; nullopt outside the archive.
std::optional<quint32> get(const QByteArray &bytes, qint64 offset, int size) {
    if (offset < 0 || offset + size > bytes.size()) {
        return std::nullopt;
    }
    quint32 value = 0;
    for (int index = 0; index < size; ++index) {
        value |= static_cast<quint32>(static_cast<quint8>(bytes[offset + index])) << (8 * index);
    }
    return value;
}

constexpr quint32 localSignature = 0x04034B50u;
constexpr quint32 centralSignature = 0x02014B50u;
constexpr quint32 endSignature = 0x06054B50u;

QString gameKey(GameProfile game) {
    return game == GameProfile::speedyBlupi1   ? QStringLiteral("speedy-blupi-1")
           : game == GameProfile::speedyBlupi2 ? QStringLiteral("speedy-blupi-2")
                                               : QString();
}

QByteArray sha256Hex(const QByteArray &bytes) {
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}

} // namespace

bool validPackMemberName(const QString &name) {
    if (name.isEmpty() || name.size() > 64 || name.startsWith(QLatin1Char('.'))) {
        return false;
    }
    for (const auto character : name) {
        const auto code = character.unicode();
        const bool allowed = (code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z') ||
                             (code >= '0' && code <= '9') || code == '.' || code == '-' ||
                             code == '_';
        if (!allowed) {
            return false;
        }
    }
    return !name.contains(QLatin1String(".."));
}

QByteArray writePackManifest(const Campaign &campaign) {
    QJsonArray entries;
    for (const auto &entry : campaign.entries) {
        QJsonObject item;
        item[QStringLiteral("file")] = text(entry.fileName);
        item[QStringLiteral("kind")] =
            entry.slot.user ? QStringLiteral("designed") : QStringLiteral("official");
        if (entry.slot.user) {
            item[QStringLiteral("player")] = entry.slot.gamer;
        }
        item[QStringLiteral("number")] = entry.slot.number;
        item[QStringLiteral("size")] = static_cast<double>(entry.size);
        item[QStringLiteral("sha256")] = text(entry.sha256);
        if (!entry.title.empty()) {
            item[QStringLiteral("title")] = text(entry.title);
        }
        entries.append(item);
    }
    QJsonObject root;
    root[QStringLiteral("format")] = QStringLiteral("eggbert-campaign");
    root[QStringLiteral("version")] = packFormatVersion;
    root[QStringLiteral("name")] = text(campaign.name);
    root[QStringLiteral("game")] = gameKey(campaign.game);
    root[QStringLiteral("worlds")] = entries;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

std::expected<Campaign, QString> readPackManifest(const QByteArray &manifest) {
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(manifest, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        return fail(QStringLiteral("The manifest is not valid JSON (%1).").arg(error.errorString()));
    }
    const auto root = document.object();
    if (root[QStringLiteral("format")].toString() != QLatin1String("eggbert-campaign")) {
        return fail(QStringLiteral("The manifest is not a campaign manifest."));
    }
    if (root[QStringLiteral("version")].toInt(-1) != packFormatVersion) {
        return fail(QStringLiteral("The manifest has format version %1; this editor reads "
                                   "version %2.")
                        .arg(root[QStringLiteral("version")].toVariant().toString())
                        .arg(packFormatVersion));
    }
    Campaign campaign;
    campaign.name = root[QStringLiteral("name")].toString().left(200).toStdString();
    const auto game = root[QStringLiteral("game")].toString();
    campaign.game = game == gameKey(GameProfile::speedyBlupi1)   ? GameProfile::speedyBlupi1
                    : game == gameKey(GameProfile::speedyBlupi2) ? GameProfile::speedyBlupi2
                                                                 : GameProfile::unknown;
    if (campaign.game == GameProfile::unknown) {
        return fail(QStringLiteral("The manifest does not say which game the campaign is for."));
    }
    const auto worlds = root[QStringLiteral("worlds")];
    if (!worlds.isArray() || worlds.toArray().isEmpty()) {
        return fail(QStringLiteral("The manifest lists no worlds."));
    }
    if (worlds.toArray().size() >= packMostMembers) {
        return fail(QStringLiteral("The manifest lists more worlds than a game can hold."));
    }
    for (const auto &value : worlds.toArray()) {
        const auto item = value.toObject();
        CampaignEntry entry;
        const auto kind = item[QStringLiteral("kind")].toString();
        if (kind != QLatin1String("designed") && kind != QLatin1String("official")) {
            return fail(QStringLiteral("A world in the manifest has no valid kind."));
        }
        entry.slot.user = kind == QLatin1String("designed");
        entry.slot.gamer = entry.slot.user ? item[QStringLiteral("player")].toInt(0) : 1;
        entry.slot.number = item[QStringLiteral("number")].toInt(0);
        const auto slotName = worldSlotFileName(entry.slot);
        const auto file = item[QStringLiteral("file")].toString();
        if (slotName.empty()) {
            return fail(QStringLiteral("%1 is not in a slot the games use.").arg(file));
        }
        if (file != text(slotName)) {
            return fail(QStringLiteral("%1 is listed under a name that is not its slot's (%2).")
                            .arg(file, text(slotName)));
        }
        entry.fileName = slotName;
        const auto size = item[QStringLiteral("size")].toDouble(-1);
        const auto hash = item[QStringLiteral("sha256")].toString();
        static const QRegularExpression hex(QStringLiteral("^[0-9a-f]{64}$"));
        if (size < 0 || size > static_cast<double>(packLargestMember) ||
            !hex.match(hash).hasMatch()) {
            return fail(QStringLiteral("%1 has no valid size or checksum in the manifest.").arg(file));
        }
        entry.size = static_cast<std::uint64_t>(size);
        entry.sha256 = hash.toStdString();
        entry.title = item[QStringLiteral("title")].toString().left(100).toStdString();
        campaign.entries.push_back(std::move(entry));
    }
    return campaign;
}

std::expected<QByteArray, QString> writeZip(const std::vector<PackFile> &files) {
    QByteArray archive;
    QByteArray central;
    QSet<QString> names;
    if (files.size() > static_cast<std::size_t>(packMostMembers)) {
        return fail(QStringLiteral("Too many files for one pack."));
    }
    for (const auto &file : files) {
        if (!validPackMemberName(file.name) || names.contains(file.name.toLower())) {
            return fail(QStringLiteral("%1 cannot be a member of a pack.").arg(file.name));
        }
        if (file.bytes.size() > packLargestMember) {
            return fail(QStringLiteral("%1 is too large for a pack.").arg(file.name));
        }
        names.insert(file.name.toLower());
        const auto name = file.name.toLatin1();
        const auto crc = crc32(file.bytes);
        const auto size = static_cast<quint32>(file.bytes.size());
        const auto offset = static_cast<quint32>(archive.size());
        // Local header. Time and date are fixed (1 January 1980) so the same
        // pack always gives the same bytes.
        put32(archive, localSignature);
        put16(archive, 10); // version needed
        put16(archive, 0);  // flags
        put16(archive, 0);  // method: stored
        put16(archive, 0);  // time
        put16(archive, 0x21); // date
        put32(archive, crc);
        put32(archive, size);
        put32(archive, size);
        put16(archive, static_cast<quint32>(name.size()));
        put16(archive, 0);
        archive.append(name);
        archive.append(file.bytes);
        put32(central, centralSignature);
        put16(central, 10); // made by
        put16(central, 10); // needed
        put16(central, 0);
        put16(central, 0);
        put16(central, 0);
        put16(central, 0x21);
        put32(central, crc);
        put32(central, size);
        put32(central, size);
        put16(central, static_cast<quint32>(name.size()));
        put16(central, 0); // extra
        put16(central, 0); // comment
        put16(central, 0); // disk
        put16(central, 0); // internal attributes
        put32(central, 0); // external attributes
        put32(central, offset);
        central.append(name);
    }
    const auto centralOffset = static_cast<quint32>(archive.size());
    archive.append(central);
    put32(archive, endSignature);
    put16(archive, 0);
    put16(archive, 0);
    put16(archive, static_cast<quint32>(files.size()));
    put16(archive, static_cast<quint32>(files.size()));
    put32(archive, static_cast<quint32>(central.size()));
    put32(archive, centralOffset);
    put16(archive, 0);
    return archive;
}

std::expected<std::vector<PackFile>, QString> readZip(const QByteArray &archive) {
    // The end record is the last 22 bytes unless a comment follows it.
    qint64 end = -1;
    for (qint64 offset = archive.size() - 22;
         offset >= 0 && offset >= archive.size() - 22 - 65'535; --offset) {
        if (get(archive, offset, 4) == endSignature) {
            end = offset;
            break;
        }
    }
    if (end < 0) {
        return fail(QStringLiteral("This is not a ZIP archive (no end record)."));
    }
    const auto disk = *get(archive, end + 4, 2);
    const auto startDisk = *get(archive, end + 6, 2);
    const auto countHere = *get(archive, end + 8, 2);
    const auto count = *get(archive, end + 10, 2);
    const auto centralSize = *get(archive, end + 12, 4);
    const auto centralOffset = *get(archive, end + 16, 4);
    if (disk != 0 || startDisk != 0 || countHere != count) {
        return fail(QStringLiteral("Split ZIP archives are not supported."));
    }
    if (count == 0xFFFF || centralSize == 0xFFFFFFFFu || centralOffset == 0xFFFFFFFFu) {
        return fail(QStringLiteral("ZIP64 archives are not supported."));
    }
    if (count > static_cast<quint32>(packMostMembers)) {
        return fail(QStringLiteral("The archive has more members than a pack can have."));
    }
    if (static_cast<qint64>(centralOffset) + centralSize > end) {
        return fail(QStringLiteral("The archive's directory lies outside the archive."));
    }
    std::vector<PackFile> files;
    QSet<QString> names;
    qint64 position = centralOffset;
    for (quint32 index = 0; index < count; ++index) {
        if (get(archive, position, 4) != centralSignature || position + 46 > end) {
            return fail(QStringLiteral("The archive's directory is damaged."));
        }
        const auto flags = *get(archive, position + 8, 2);
        const auto method = *get(archive, position + 10, 2);
        const auto crc = *get(archive, position + 16, 4);
        const auto packed = *get(archive, position + 20, 4);
        const auto size = *get(archive, position + 24, 4);
        const auto nameLength = *get(archive, position + 28, 2);
        const auto extraLength = *get(archive, position + 30, 2);
        const auto commentLength = *get(archive, position + 32, 2);
        const auto localOffset = *get(archive, position + 42, 4);
        if (position + 46 + nameLength + extraLength + commentLength > end) {
            return fail(QStringLiteral("The archive's directory is damaged."));
        }
        const auto name = QString::fromLatin1(archive.mid(position + 46, nameLength));
        position += 46 + nameLength + extraLength + commentLength;
        if (!validPackMemberName(name)) {
            return fail(QStringLiteral("The archive contains a member whose name is not allowed "
                                       "in a pack: \"%1\".")
                            .arg(name.left(80)));
        }
        if (names.contains(name.toLower())) {
            return fail(QStringLiteral("The archive contains %1 more than once.").arg(name));
        }
        names.insert(name.toLower());
        if ((flags & 0x0001u) != 0) {
            return fail(QStringLiteral("%1 is encrypted.").arg(name));
        }
        // Stored, or compressed the way ZIP programs do by default.
        if (method != 0 && method != 8) {
            return fail(QStringLiteral("%1 is compressed with a method other than Deflate, "
                                       "which is the only one read.")
                            .arg(name));
        }
        if (method == 0 && packed != size) {
            return fail(QStringLiteral("%1 has a damaged header.").arg(name));
        }
        if (size == 0xFFFFFFFFu || size > static_cast<quint32>(packLargestMember) ||
            packed == 0xFFFFFFFFu || packed > static_cast<quint32>(packLargestMember)) {
            return fail(QStringLiteral("%1 is too large for a pack.").arg(name));
        }
        // The local header must agree and the data must lie before the
        // directory.
        const auto localName = get(archive, localOffset + 26, 2);
        const auto localExtra = get(archive, localOffset + 28, 2);
        if (get(archive, localOffset, 4) != localSignature || !localName || !localExtra ||
            *localName != nameLength ||
            archive.mid(localOffset + 30, nameLength) != name.toLatin1()) {
            return fail(QStringLiteral("%1 has a damaged header.").arg(name));
        }
        const qint64 dataOffset = static_cast<qint64>(localOffset) + 30 + *localName + *localExtra;
        if (dataOffset + packed > centralOffset) {
            return fail(QStringLiteral("%1 does not lie inside the archive.").arg(name));
        }
        auto bytes = archive.mid(dataOffset, packed);
        if (method == 8) {
            // Never more than the stated size is produced, whatever the data says.
            auto inflated = inflateRaw(bytes, size);
            if (!inflated) {
                return fail(QStringLiteral("%1 cannot be decompressed. %2")
                                .arg(name, inflated.error()));
            }
            bytes = std::move(*inflated);
        }
        if (crc32(bytes) != crc) {
            return fail(QStringLiteral("%1 does not match its checksum.").arg(name));
        }
        files.push_back({name, std::move(bytes)});
    }
    return files;
}

std::expected<CampaignPack, QString> makePack(QString name, GameProfile game,
                                              std::vector<WorldSlot> places,
                                              std::vector<QByteArray> worlds) {
    if (places.size() != worlds.size() || places.empty()) {
        return fail(QStringLiteral("A pack needs at least one world."));
    }
    std::vector<PackFile> files;
    Campaign campaign;
    campaign.name = name.toStdString();
    campaign.game = game;
    for (std::size_t index = 0; index < places.size(); ++index) {
        CampaignEntry entry;
        entry.slot = places[index];
        entry.fileName = worldSlotFileName(entry.slot);
        entry.size = static_cast<std::uint64_t>(worlds[index].size());
        entry.sha256 = sha256Hex(worlds[index]).toStdString();
        const std::span<const std::byte> span(
            reinterpret_cast<const std::byte *>(worlds[index].constData()),
            static_cast<std::size_t>(worlds[index].size()));
        const auto probe = probeWorld(span);
        if (probe.descriptor != nullptr && probe.descriptor->minor >= 3) {
            if (const auto header = WorldView(span, probe).header()) {
                const auto title = header->nameText();
                entry.title = QString::fromLatin1(reinterpret_cast<const char *>(title.data()),
                                                  static_cast<qsizetype>(title.size()))
                                  .toStdString();
            }
        }
        files.push_back({text(entry.fileName), worlds[index]});
        campaign.entries.push_back(std::move(entry));
    }
    files.insert(files.begin(), {QString::fromLatin1(packManifestName), writePackManifest(campaign)});
    // Through the same checks a pack from elsewhere gets.
    return assemblePack(files);
}

std::expected<CampaignPack, QString> assemblePack(const std::vector<PackFile> &files) {
    const PackFile *manifest = nullptr;
    QHash<QString, const PackFile *> byName;
    for (const auto &file : files) {
        if (!validPackMemberName(file.name)) {
            return fail(QStringLiteral("\"%1\" is not a name a pack may contain.")
                            .arg(file.name.left(80)));
        }
        if (file.bytes.size() > packLargestMember) {
            return fail(QStringLiteral("%1 is too large for a pack.").arg(file.name));
        }
        if (file.name == QLatin1String(packManifestName)) {
            manifest = &file;
        } else if (byName.contains(file.name.toLower())) {
            return fail(QStringLiteral("The pack contains %1 more than once.").arg(file.name));
        } else {
            byName.insert(file.name.toLower(), &file);
        }
    }
    if (manifest == nullptr) {
        return fail(QStringLiteral("The pack has no manifest (%1).")
                        .arg(QString::fromLatin1(packManifestName)));
    }
    auto campaign = readPackManifest(manifest->bytes);
    if (!campaign) {
        return std::unexpected(campaign.error());
    }
    CampaignPack pack;
    QSet<QString> used;
    for (const auto &entry : campaign->entries) {
        const auto name = text(entry.fileName);
        if (used.contains(name)) {
            return fail(QStringLiteral("The manifest lists %1 more than once.").arg(name));
        }
        used.insert(name);
        const auto found = byName.find(name.toLower());
        if (found == byName.end() || (*found)->name != name) {
            return fail(QStringLiteral("%1 is listed in the manifest but not in the pack.").arg(name));
        }
        const auto &bytes = (*found)->bytes;
        if (static_cast<std::uint64_t>(bytes.size()) != entry.size ||
            sha256Hex(bytes).toStdString() != entry.sha256) {
            return fail(QStringLiteral("%1 is not the file the manifest describes (size or "
                                       "checksum differs).")
                            .arg(name));
        }
        const auto probe = probeWorld(std::span<const std::byte>(
            reinterpret_cast<const std::byte *>(bytes.constData()),
            static_cast<std::size_t>(bytes.size())));
        if (probe.descriptor == nullptr || (probe.status != ProbeStatus::sizeMatches &&
                                            probe.status != ProbeStatus::surplus)) {
            return fail(QStringLiteral("%1 is not a complete world of a known revision.").arg(name));
        }
        pack.worlds.push_back({name, bytes});
    }
    if (used.size() != byName.size()) {
        for (auto item = byName.begin(); item != byName.end(); ++item) {
            if (!used.contains(item.value()->name)) {
                return fail(QStringLiteral("The pack contains %1, which the manifest does not list.")
                                .arg(item.value()->name));
            }
        }
    }
    pack.campaign = std::move(*campaign);
    return pack;
}

std::expected<void, QString> writePack(const CampaignPack &pack, const QString &path, bool zip) {
    std::vector<PackFile> files;
    files.push_back({QString::fromLatin1(packManifestName), writePackManifest(pack.campaign)});
    files.insert(files.end(), pack.worlds.begin(), pack.worlds.end());
    const auto save = [](const QString &target, const QByteArray &bytes) {
        QSaveFile file(target);
        file.setDirectWriteFallback(false);
        return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() &&
               file.commit();
    };
    if (zip) {
        const auto archive = writeZip(files);
        if (!archive) {
            return std::unexpected(archive.error());
        }
        if (QFileInfo::exists(path) || !save(path, *archive)) {
            return fail(QStringLiteral("%1 exists already or cannot be written.").arg(path));
        }
        return {};
    }
    // A new directory only: an existing one may hold something else.
    if (QFileInfo::exists(path) || !QDir().mkpath(path)) {
        return fail(QStringLiteral("%1 exists already or cannot be created.").arg(path));
    }
    for (const auto &file : files) {
        if (!save(QDir(path).filePath(file.name), file.bytes)) {
            return fail(QStringLiteral("%1 could not be written.").arg(file.name));
        }
    }
    return {};
}

std::expected<CampaignPack, QString> readPack(const QString &path) {
    const QFileInfo info(path);
    std::vector<PackFile> files;
    if (info.isDir()) {
        const QDir directory(path);
        const auto names = directory.entryList(QDir::Files | QDir::Hidden | QDir::System);
        if (names.size() > packMostMembers) {
            return fail(QStringLiteral("%1 holds more files than a pack can have.").arg(path));
        }
        for (const auto &name : names) {
            QFile file(directory.filePath(name));
            if (QFileInfo(file).isSymLink() || file.size() > packLargestMember ||
                !file.open(QIODevice::ReadOnly)) {
                return fail(QStringLiteral("%1 cannot be part of a pack (a link, too large or "
                                           "unreadable).")
                                .arg(name));
            }
            files.push_back({name, file.readAll()});
        }
    } else {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) ||
            file.size() > packLargestMember * static_cast<qint64>(packMostMembers)) {
            return fail(QStringLiteral("%1 cannot be read.").arg(path));
        }
        auto members = readZip(file.readAll());
        if (!members) {
            return std::unexpected(members.error());
        }
        files = std::move(*members);
    }
    return assemblePack(files);
}

} // namespace eggbert
