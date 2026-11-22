# Implementation handoff

## Current verified state

Milestones 0 (scaffold) and 1 (probe and read-only views) are complete. Nothing can be edited or saved yet.

Implemented and covered by asset-free tests:

- `src/world/layout.hpp`: immutable `RevisionDescriptor` entries for header revisions 1.0–1.3 (header size, layers, object count/width, contiguous section table, expected size, 1.0 write policy, rule); `headerFields` and `objectFields` tables with exact offsets and wire types; `ByteRange` and `Point`. Only the four exact `(major, minor)` pairs are recognized.
- `src/world/le_reader.hpp`: `readU16/I16/U32/I32` over `std::span<const std::byte>` returning `std::expected<Value, ReadError>` with the first unavailable offset; bounds use subtraction only (`rangeFits`).
- `src/world/world_probe.hpp`: `probeWorld()` reports raw revision field, candidate revision, expected size, per-section available bytes, first missing offset, missing/surplus counts and whether structured inspection is safe, partial or unavailable. It never presents a result as proof: the format has no signature.
- `src/world/world_view.hpp`: `WorldView` with `HeaderView` (every header field, indexed reserved fields, raw name bytes, text before the first NUL and retained padding after it), `TileLayerView` (`[x][y]` cells with absolute byte ranges), `ObjectPoolView` (decoded 48-byte records, slot/field ranges, raw slot bytes, active/free counts) and the opaque suffix range. A section view exists only when that section is complete; out-of-range indices return `nullopt`.
- `src/app/probe_report.cpp`: plain-text probe report plus decoded summary (name with escaped non-printable bytes, music/region/world, `posDecor`/`dimDecor`, four starts, non-zero reserved fields, per-layer count of cells that differ from -1, object slot counts). **File → Inspect World File** shows both, read-only.
- Build: `eggbert_world` (no Qt) and `eggbert_app` (Qt Widgets, everything except `main`); `-Wall -Wextra -Wpedantic`; optional `-DEGGBERT_SANITIZE=ON`.
- Tests: `tests/fixture.hpp` builds synthetic worlds from literal offsets and includes no production header.

Verification on 2026-11-22, Ubuntu 24.04, GCC 13.3.0, CMake 3.28.3, Ninja, Qt 6.4.2, no game data present:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel              # no warnings
ctest --test-dir build --output-on-failure  # 6/6 passed: world_file, layout, le_reader, world_probe, world_view, probe_report
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DEGGBERT_SANITIZE=ON
cmake --build build-asan --parallel && ctest --test-dir build-asan   # 6/6 passed under ASan/UBSan
```

## Research evidence log

- `blupi_recon` at commit `e05e22b990abb782c37a0d9fafa864d86a27e29f`: `src/decor.h` (`DescFile`, `Cellule`, `MoveObject`), `src/decor.cpp` (`CDecor::Read`, `CDecor::Write`). The reader skips `bigDecor` only for exactly 1.0, reads 100 records only when `majRev == 1 && minRev <= 1`, and copies the title only for 1.3 or later; the writer zero-fills the header and emits 1.3 (1.2 with `reserve1[0] = 1` in the June 1998 demo build).
- Archived `eggbert-generator` bundle (SHA-256 `8bed90c38f4646061d50f8a130f8b303b064d0b49a237a81433598c793f5f668`, HEAD `069c409611eb779e9066e8d492faed4550dac9a8`, the commit pinned in `plan.md`), `docs/formats/BLP_SB1_WORLD.md`. Its authors report (not re-measured here): stock SB1 worlds are revision 1.2 with an all-zero title; stock SB2 worlds are 1.3; an empty cell is -1; the `world` field is written by neither game; demo and Valueware editions use `reserve1[0]` as a non-zero marker (256 in shipped demo worlds); SB1 tile icons with collision data are 0–236 and SB2 0–440. These observations guide fixtures (non-zero `reserve1[0]` must survive) but each still needs its own recorded evidence before a feature is called verified for a profile.
- No original executable or retail sample has been tested in this repository yet.

## Next task: lossless document core (milestone 2a)

Add the headless, Qt-free editing core. No file I/O and no UI in this slice.

1. `src/world/world_document.hpp/.cpp`: owns original bytes, last-saved bytes and current bytes. Exposes `WorldView` over each, write access state with a reason (writable only for a complete 1.2/1.3 layout; 1.0/1.1, truncated and unknown inputs are read-only), and `apply(Transaction)`.
2. A `Transaction` is a label (semantic intent) plus byte changes with old and new bytes. `apply` validates every change before mutating anything: inside the layout, old bytes equal current bytes, and no overlap with protected ranges (`majRev`, `minRev`, `reserve1`, `world`, `reserve2`, `reserve3`, and the opaque suffix). A rejected transaction changes nothing.
3. `EditBuilder`: semantic operations that produce minimal byte changes — set cell, set header scalar/POINT (verified fields only), set name text (bytes after the new terminator stay as they are), set object field/record. Writing a value equal to the current one produces no change.
4. Linear history: undo, redo, clean index that undo may cross, redo branch discarded by a new edit, optional merge key so one drag becomes one step. `changedRangesFromOriginal()` and `unsavedRanges()` return merged byte ranges.
5. Tests with hand-built fixtures: untouched document equals input byte for byte; a one-cell edit changes exactly its two bytes at the literal offset; each header/object edit changes only the named range; non-zero reserved bytes, name padding, free-slot bytes and suffix survive every operation; 1.0/1.1/truncated/unknown documents reject edits; stale or overlapping or protected changes are rejected atomically; undo/redo/clean-index/merge behavior.

## Slice roadmap toward the 1.0 gate

Completed slices are ticked only when their tests are green and committed.

- [x] 0 Scaffold
- [x] 1a Revision descriptors, bounded readers, `WorldProbe`, GUI probe report
- [x] 1b Read-only header/layer/object views and decoded inspector summary
- [ ] 2a Lossless document: byte-range transactions, changed ranges, linear undo/redo with clean index, write gating (1.2/1.3 only, complete layout only)
- [ ] 2b Save/Save As with atomic replacement, external-change detection and failure injection
- [ ] 3a Tabbed workspace shell, schematic canvas, pan/zoom, layer visibility
- [ ] 3b Minimap and World Statistics
- [ ] 3c Manual read-only asset directory and artwork rendering
- [ ] 4a Layer tools: pencil, eraser, eyedropper, line, rectangle, fill, selection, clipboard, find
- [ ] 4b Metadata and player starts
- [ ] 4c Verified object catalogs, object panel, create/duplicate/delete/relocate, capacity budgets
- [ ] 5 Verified adaptive brushes
- [ ] 6a Read-only binary inspector
- [ ] 6b Basic World Doctor
- [ ] 7a Full-world PNG export
- [ ] 7b User guide, notices, Linux packaging, smoke tests, opt-in local-sample matrix

## Explicit non-goals

Campaign/hub management, installation discovery or deployment, playtest launch, SB1↔SB2 conversion, Mobile Eggbert support, writing revisions 1.0/1.1, raw hex writes, ZIP handling and procedural generation stay out of scope (see **Post-1.0 roadmap** in `plan.md`).
