// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <span>

namespace eggbert {

// Texts of Qt's own widgets that appear in the editor's dialogs and menus
// (standard buttons, the context menu of an input field, dock buttons),
// as {context, text} pairs. They are marked for the translation tools so
// that the editor's catalogue can translate them where Qt's own
// translation files are not installed.
struct QtText {
    const char *context;
    const char *text;
};

[[nodiscard]] std::span<const QtText> qtTexts();

} // namespace eggbert
