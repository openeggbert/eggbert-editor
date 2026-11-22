# Eggbert Editor agent guide

This repository is intended to be implementable from a fresh agent context. Read, in order:

1. `README.md` for current capabilities and build commands.
2. `NEXT.md` for the next bounded task and the last verified state.
3. all of `plan.md` for product scope, binary facts, safety requirements and acceptance criteria.
4. the current code and tests before proposing changes.

The 1.0 product is a lossless desktop editor for individual Speedy Blupi / Speedy Eggbert 1 and 2 world files. It is not a procedural generator, campaign manager or game-installation deployment tool. Do not implement items from the Post-1.0 roadmap unless the user explicitly promotes one and the fixed scope is updated first.

Post-1.0 Mobile Eggbert support is a separate text-world document family, not another BLP profile. BLP and Mobile Eggbert conversion is intentionally unsupported in both directions. Do not add Convert/Open As/Export To commands, geometry-only draft import, cross-family clipboard transfer or other workflows that imply that a world retains gameplay semantics across those engines. Shared UI and document abstractions may be reused, but their codecs, catalogs, validators and editing sessions remain isolated. This restriction does not prohibit the separately planned, explicitly loss-aware SB1↔SB2 BLP conversion.

Use C++23, CMake and Qt 6 Widgets. Keep the binary world codec, document transactions and validation independent of Qt in `src/world`. UI, rendering, asset resolution and dialogs belong outside the core according to `plan.md`. All project code and build files use `SPDX-License-Identifier: GPL-3.0-only`. Do not add proprietary game assets or copied code.

Losslessness is the primary invariant. Preserve the exact input bytes, including unknown/reserved fields, inactive slots, padding, legacy representations and trailing data. Ordinary Save patches only user-edited fields. Put canonicalization, supported BLP profile conversion and original-editor emulation behind explicit exports with a preview of every affected byte range. If a structure is ambiguous, truncated or unsupported, keep inspection available and structured writes disabled.

The reconstructed games and `/rv/data/archive/trash/2026/eggbert-generator.bundle` may be inspected as research evidence when available. Pin revisions and record observations, but independently write Eggbert Editor code and fixtures. Do not edit sibling projects and do not transplant source, tests or assets from them.

In Claude Code Cloud, follow `docs/claude-code-cloud.md`. Optional retail data remains outside the clone and read-only. The build, tests and synthetic fixtures must not depend on network access or proprietary data.

Work in small vertical slices. Begin from `NEXT.md`, write asset-free tests for on-disk behavior, implement the headless core, then connect the UI. Run:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Before stopping, keep the checkout buildable and update `NEXT.md` with completed behavior, commands and results, unresolved evidence, and one concrete next task. Update `plan.md` only for stable requirements or decisions. Never mark a feature complete because its menu placeholder exists.
