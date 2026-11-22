// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>

#include "world/world_probe.hpp"
#include "world/world_view.hpp"

namespace eggbert {

// Plain-text, read-only description of a probe result, including what the
// probe cannot establish.
[[nodiscard]] QString formatProbeReport(const QString &fileName, const WorldProbe &probe);

[[nodiscard]] QString sectionDisplayName(SectionKind kind);

// Plain-text summary of the decoded, complete sections of a world. Empty
// when no section is complete. Read-only: nothing is interpreted per game.
[[nodiscard]] QString formatWorldSummary(const WorldView &view);

// Name bytes shown as Latin-1 with every non-printable byte escaped as \xNN,
// so the display never hides or alters stored bytes.
[[nodiscard]] QString displayNameBytes(std::span<const std::byte> bytes);

} // namespace eggbert
