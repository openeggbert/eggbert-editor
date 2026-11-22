# Eggbert Editor agent guide

This repository is intended to be implementable from a fresh agent context. Read, in order:

1. `README.md` for current capabilities and build commands.
2. `NEXT.md` for the next bounded task and the last verified state.
3. all of `plan.md` for product scope, binary facts, safety requirements and acceptance criteria.
4. the current code and tests before proposing changes.

The product is a lossless desktop world editor for Speedy Blupi / Speedy Eggbert 1 and 2. It is not a procedural world or campaign generator. Blank worlds and campaigns are manually authored by the user.

Use C++23, CMake and Qt 6 Widgets. Keep the binary world codec, document transactions and validation independent of Qt in `src/world`. UI code belongs in `src/app`; later campaign, installation and rendering modules should retain the boundaries in `plan.md`. All project code and build files use `SPDX-License-Identifier: GPL-3.0-only`. Do not add proprietary game assets or copied code.

Losslessness is the primary invariant. Preserve the exact input bytes, including unknown/reserved fields, inactive slots, padding, legacy representations and trailing data. Ordinary Save patches only user-edited fields. Put canonicalization, profile conversion and original-editor emulation behind explicit exports with a preview of every affected byte range. If a structure is ambiguous, truncated or unsupported, keep inspection available and structured writes disabled.

The reconstructed games and `/rv/data/archive/trash/2026/eggbert-generator.bundle` may be inspected as research evidence when available. Pin revisions and record observations, but independently write Eggbert Editor code and fixtures. Do not edit sibling projects and do not transplant source, tests or assets from them.

Work in small vertical slices. Begin from `NEXT.md`, write asset-free tests for on-disk behavior, implement the headless core, then connect the UI. Run:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Before stopping, keep the checkout buildable and update `NEXT.md` with completed behavior, commands and results, unresolved evidence, and one concrete next task. Update `plan.md` only for stable requirements or decisions. Never mark a feature complete because its menu placeholder exists.
