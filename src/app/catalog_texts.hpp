// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>

#include <span>
#include <string_view>

namespace eggbert {

// The names of the game catalogs (editor groups, tile entries, objects and
// border families) live in the Qt-free core as plain English. They are shown
// through this function so a catalogue can translate them; a name without a
// translation is shown as it is.
[[nodiscard]] QString catalogText(std::string_view name);

// Every catalog name, marked for the translation tools. A test checks that
// no catalog name is missing here.
[[nodiscard]] std::span<const char *const> catalogTexts();

} // namespace eggbert
