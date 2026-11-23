// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>

#include <span>

#include "world/game_convert.hpp"

namespace eggbert {

// One line for a note of a conversion between the two games, in the
// interface's language: what kind of value, where, and what the target game
// does with it. Without a translation it equals the command line's English
// line (`describeGameNote`).
[[nodiscard]] QString conversionNoteLine(const GameConversionNote &note);

// The notes' sentences, marked for the translation tools. A test checks
// that none the core can produce is missing here.
[[nodiscard]] std::span<const char *const> conversionTexts();

} // namespace eggbert
