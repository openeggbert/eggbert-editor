// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <QStringList>
#include <QTextStream>

namespace eggbert {

// Headless commands: `info`, `validate`, `diff`, `export-png` and `convert`.
// They read world files and never change them (export-png and convert write
// only their output file).
//
// Exit codes: 0 success (validate: no error finding; diff: files equal),
// 1 the command ran and found something (validate: errors; diff: files
// differ), 2 wrong usage or a file that cannot be read or written.

// True when `name` is one of the commands, so `main` can choose this path.
[[nodiscard]] bool isCommandLineCommand(const QString &name);
// True for commands that draw and therefore need a QGuiApplication.
[[nodiscard]] bool commandNeedsGraphics(const QString &name);
// `arguments` without the program name; the first one is the command.
int runCommandLine(const QStringList &arguments, QTextStream &out, QTextStream &err);
[[nodiscard]] QString commandLineUsage();

} // namespace eggbert
