// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>

#include <span>

#include "world/doctor.hpp"

namespace eggbert {

// World Doctor composes its findings in the Qt-free core, in English, as a
// pattern and the texts that fill it. These functions show them in the
// interface's language: the pattern is looked up in the catalogue and filled
// with the same arguments (catalog names among them translated too). Without
// a translation the result is the core's English sentence.
[[nodiscard]] QString findingText(const Finding &finding);
// "structural fact", "estimate" and the other bases of a finding.
[[nodiscard]] QString confidenceText(Confidence confidence);

// Every pattern, marked for the translation tools. A test checks that none
// the core can produce is missing here.
[[nodiscard]] std::span<const char *const> doctorTexts();

} // namespace eggbert
