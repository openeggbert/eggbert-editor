// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>
#include <QStringList>

#include <span>
#include <string_view>

namespace eggbert {

// Sentences the Qt-free core composes in English (problems of a text world,
// reasons a stamp cannot be placed, notes of a revision conversion, findings
// about a campaign's numbering and the like), shown in the interface's
// language. A sentence is looked up by its pattern: the sentence with every
// number replaced by %1, %2, ... in order. The translated pattern is filled
// with the same numbers. A sentence without a translated pattern, or with
// more than nine numbers, is shown as the core wrote it.
[[nodiscard]] QString coreText(std::string_view english);

// The pattern of a sentence and, when asked for, its numbers. A minus sign
// belongs to a number only at the start of the sentence or after a space or
// an opening parenthesis.
[[nodiscard]] QString corePattern(std::string_view english, QStringList *numbers = nullptr);

// The patterns marked for the translation tools.
[[nodiscard]] std::span<const char *const> coreTexts();

} // namespace eggbert
