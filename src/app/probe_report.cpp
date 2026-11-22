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
    lines << tr("Editing and saving are not available.");
    return lines.join(QLatin1Char('\n'));
}

} // namespace eggbert
