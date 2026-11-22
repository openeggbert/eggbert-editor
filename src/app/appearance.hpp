// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QIcon>
#include <QString>

namespace eggbert {

enum class Theme { system, light, dark };

[[nodiscard]] QString themeKey(Theme theme);
[[nodiscard]] Theme themeFromKey(const QString &key);

// Applies a theme to the whole application. `system` restores the palette
// and style the application started with.
void applyTheme(Theme theme);
[[nodiscard]] Theme currentTheme();

// The application icon, drawn by the project (no external artwork).
[[nodiscard]] QIcon applicationIcon();

} // namespace eggbert
