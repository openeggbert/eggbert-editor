// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QByteArray>
#include <QString>

#include <expected>
#include <vector>

#include "world/campaign.hpp"

namespace eggbert {

// A campaign pack: a manifest and the world files it lists, as a directory
// or as one ZIP file. Everything read from a pack is validated before any
// of it is used; nothing in a pack can name a place outside the pack.

inline constexpr int packFormatVersion = 1;
inline constexpr char packManifestName[] = "eggbert-campaign.json";
inline constexpr qint64 packLargestMember = 1024 * 1024; // a world is about 50 KiB
inline constexpr int packMostMembers = 1200;             // 999 official + 160 designed + manifest

struct PackFile {
    QString name;
    QByteArray bytes;

    friend bool operator==(const PackFile &, const PackFile &) = default;
};

struct CampaignPack {
    Campaign campaign;
    // The world files in manifest order.
    std::vector<PackFile> worlds;
};

// True for a name a pack may contain: plain ASCII letters, digits, dot,
// hyphen and underscore, not starting with a dot, at most 64 characters.
[[nodiscard]] bool validPackMemberName(const QString &name);

// The manifest text for a campaign (stable: the same campaign gives the
// same bytes), and back.
[[nodiscard]] QByteArray writePackManifest(const Campaign &campaign);
[[nodiscard]] std::expected<Campaign, QString> readPackManifest(const QByteArray &manifest);

// ZIP archives without compression (method "stored"), with CRC-32.
[[nodiscard]] std::expected<QByteArray, QString> writeZip(const std::vector<PackFile> &files);
// Refuses compressed, encrypted, split or ZIP64 archives, names that are
// not valid member names, duplicates, wrong checksums, and anything that
// does not lie inside the archive.
[[nodiscard]] std::expected<std::vector<PackFile>, QString> readZip(const QByteArray &archive);

// Builds the pack of a campaign from its world files (`worlds[i]` belongs
// to `campaign.entries[i]`): sizes, checksums and file names are filled in
// from the bytes and the places.
[[nodiscard]] std::expected<CampaignPack, QString> makePack(QString name, GameProfile game,
                                                            std::vector<WorldSlot> places,
                                                            std::vector<QByteArray> worlds);
// Checks that the files are exactly what the manifest lists (names, sizes,
// checksums), that every entry is in a valid slot under its slot's file
// name, and that every world is a complete world of a known revision.
[[nodiscard]] std::expected<CampaignPack, QString> assemblePack(const std::vector<PackFile> &files);

// A pack on disk: a `.zip` file, or a directory.
[[nodiscard]] std::expected<void, QString> writePack(const CampaignPack &pack, const QString &path,
                                                     bool zip);
[[nodiscard]] std::expected<CampaignPack, QString> readPack(const QString &path);

} // namespace eggbert
