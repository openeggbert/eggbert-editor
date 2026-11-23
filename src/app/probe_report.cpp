// SPDX-License-Identifier: GPL-3.0-only
#include "probe_report.hpp"

#include <QCoreApplication>
#include <QLocale>
#include <QStringList>

namespace eggbert {

namespace {

QString tr(const char *text) {
    return QCoreApplication::translate("ProbeReport", text);
}

QString count(std::size_t value) {
    return QLocale::c().toString(static_cast<qulonglong>(value));
}

QString text(std::string_view view) {
    return QString::fromUtf8(view.data(), static_cast<qsizetype>(view.size()));
}

} // namespace

QString sectionDisplayName(SectionKind kind) {
    switch (kind) {
    case SectionKind::header:
        return tr("Header");
    case SectionKind::foreground:
        return tr("Foreground layer");
    case SectionKind::bigDecor:
        return tr("bigDecor layer");
    case SectionKind::objects:
        return tr("Object records");
    }
    return tr("Unknown section");
}

QString formatProbeReport(const QString &fileName, const WorldProbe &probe) {
    QStringList lines;
    lines << fileName;
    lines << tr("%1 bytes").arg(count(probe.inputBytes));

    if (probe.headerMajor && probe.headerMinor) {
        lines << tr("Revision field: %1.%2").arg(*probe.headerMajor).arg(*probe.headerMinor);
    } else {
        lines << tr("Revision field: unavailable");
    }

    switch (probe.status) {
    case ProbeStatus::revisionUnavailable:
        lines << tr("Not inspectable as a world.");
        break;
    case ProbeStatus::unknownRevision:
        lines << (probe.laterThanKnownRevisions
                      ? tr("Unverified later revision; not inspected as a world.")
                      : tr("Unrecognized revision; not inspected as a world."));
        break;
    case ProbeStatus::truncated:
        lines << tr("Candidate revision %1.%2, truncated.")
                     .arg(probe.descriptor->major)
                     .arg(probe.descriptor->minor);
        break;
    case ProbeStatus::sizeMatches:
        lines << tr("Candidate revision %1.%2; size is consistent.")
                     .arg(probe.descriptor->major)
                     .arg(probe.descriptor->minor);
        break;
    case ProbeStatus::surplus:
        lines << tr("Candidate revision %1.%2 with an opaque suffix.")
                     .arg(probe.descriptor->major)
                     .arg(probe.descriptor->minor);
        break;
    }

    if (probe.expectedBytes) {
        lines << tr("Expected size: %1 bytes").arg(count(*probe.expectedBytes));
    }
    if (probe.firstMissingOffset) {
        lines << tr("First missing offset: %1 (%2 bytes missing)")
                     .arg(count(*probe.firstMissingOffset), count(probe.missingBytes));
    }
    if (probe.surplusBytes != 0) {
        lines << tr("Surplus: %1 bytes after offset %2, retained unchanged")
                     .arg(count(probe.surplusBytes), count(*probe.expectedBytes));
    }
    for (const auto &section : probe.sections()) {
        lines << tr("%1: offset %2, %3 of %4 bytes, %5")
                     .arg(sectionDisplayName(section.kind), count(section.offset),
                          count(section.availableBytes), count(section.size),
                          section.complete() ? tr("complete") : tr("incomplete"));
    }
    if (probe.descriptor != nullptr) {
        lines << text(probe.descriptor->rule);
    }

    lines << QString();
    lines << (probe.structuredInspectionSafe
                  ? tr("Structured inspection: available.")
                  : probe.partialInspectionAvailable
                        ? tr("Structured inspection: complete sections only.")
                        : tr("Structured inspection: unavailable."));
    lines << text(probeStatusReason(probe.status));
    lines << tr("The file extension is not evidence, and the game profile (Speedy Blupi 1 or 2) "
                "cannot be determined from these bytes.");
    lines << (probe.descriptor != nullptr &&
                      probe.descriptor->writePolicy == WritePolicy::losslessEdit &&
                      (probe.status == ProbeStatus::sizeMatches ||
                       probe.status == ProbeStatus::surplus)
                  ? tr("Editing and saving are available for this revision.")
                  : tr("Editing and saving are not available."));
    return lines.join(QLatin1Char('\n'));
}

QString displayNameBytes(std::span<const std::byte> bytes) {
    QString result;
    for (const auto value : bytes) {
        const auto code = std::to_integer<unsigned>(value);
        if (code == '\\') {
            result += QStringLiteral("\\\\");
        } else if ((code >= 0x20 && code < 0x7F) || code >= 0xA0) {
            result += QChar(static_cast<char16_t>(code));
        } else {
            result += QStringLiteral("\\x%1").arg(code, 2, 16, QLatin1Char('0'));
        }
    }
    return result;
}

QString formatWorldSummary(const WorldView &view) {
    QStringList lines;
    const auto point = [](Point value) {
        return QStringLiteral("(%1, %2)").arg(value.x).arg(value.y);
    };
    if (const auto header = view.header()) {
        lines << tr("Name: \"%1\" (%2 bytes before the terminator%3)")
                     .arg(displayNameBytes(header->nameText()), count(header->nameLength()),
                          header->nameTerminated()
                              ? (header->namePaddingIsZero()
                                     ? QString()
                                     : tr(", non-zero bytes retained after it"))
                              : tr(", no terminator"));
        lines << tr("music %1, region %2, world %3")
                     .arg(header->music())
                     .arg(header->region())
                     .arg(header->world());
        lines << tr("posDecor %1, dimDecor %2")
                     .arg(point(header->posDecor()), point(header->dimDecor()));
        for (std::size_t index = 0; index < 4; ++index) {
            lines << tr("Start %1: position %2, direction %3")
                         .arg(index + 1)
                         .arg(point(*header->blupiPos(index)))
                         .arg(*header->blupiDir(index));
        }
        QStringList nonZero;
        for (const auto field :
             {HeaderField::reserve1, HeaderField::reserve2, HeaderField::reserve3}) {
            if (!header->reservedIsZero(field)) {
                nonZero << text(headerField(field).name);
            }
        }
        lines << (nonZero.isEmpty()
                      ? tr("Reserved header fields are all zero.")
                      : tr("Non-zero reserved header fields (retained): %1")
                            .arg(nonZero.join(QStringLiteral(", "))));
    }
    for (const auto kind : {SectionKind::foreground, SectionKind::bigDecor}) {
        if (const auto layer = view.layer(kind)) {
            std::size_t stored = 0;
            for (std::size_t x = 0; x < layout::mapWidth; ++x) {
                for (std::size_t y = 0; y < layout::mapHeight; ++y) {
                    stored += *layer->cell(x, y) != -1 ? 1 : 0;
                }
            }
            lines << tr("%1: %2 of 10000 cells differ from -1")
                         .arg(sectionDisplayName(kind), count(stored));
        }
    }
    if (const auto objects = view.objects()) {
        lines << tr("Object records: %1 stored, %2 with a non-zero type, %3 free")
                     .arg(count(objects->slotCount()), count(objects->activeCount()),
                          count(objects->freeCount()));
    }
    return lines.join(QLatin1Char('\n'));
}

} // namespace eggbert
