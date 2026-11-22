// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QString>

namespace eggbert {

// End-to-end self check used by packaging tests: builds a synthetic world,
// probes, edits, saves, reloads, validates, exports a PNG and drives the
// main window once. Needs no game data. Returns 0 on success; on failure
// prints the failed step to stderr and returns 1.
//
// With a game directory the check additionally recognises the game, loads
// its artwork read-only and exports the world drawn with it; it fails when
// the directory is not recognised or its tiles cannot be drawn.
int runSmokeTest(const QString &gameDirectory = {});

} // namespace eggbert
