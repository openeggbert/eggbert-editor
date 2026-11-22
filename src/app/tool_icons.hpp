// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QColor>
#include <QIcon>

#include "app/document_tab.hpp"

namespace eggbert {

// Pictograms for the tools, drawn by the project (no external artwork) in
// the given ink colour so they suit the light and the dark theme.
[[nodiscard]] QIcon toolIcon(Tool tool, const QColor &ink);

} // namespace eggbert
