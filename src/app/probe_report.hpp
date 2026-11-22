// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>

#include "world/world_probe.hpp"

namespace eggbert {

// Plain-text, read-only description of a probe result, including what the
// probe cannot establish.
[[nodiscard]] QString formatProbeReport(const QString &fileName, const WorldProbe &probe);

[[nodiscard]] QString sectionDisplayName(SectionKind kind);

} // namespace eggbert
