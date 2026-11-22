# Implementation handoff

## Current verified state

Milestone 0 (scaffold) is complete and milestone 1 (probe and read-only views) is in progress.

Implemented and covered by asset-free tests:

- `src/world/layout.hpp`: immutable `RevisionDescriptor` entries for header revisions 1.0, 1.1, 1.2 and 1.3 with header size, layer presence/count, object count and record width, contiguous section table, expected payload size, 1.0 write policy and the revision rule. Only the four exact `(major, minor)` pairs are recognized; later pairs are flagged but not assigned a layout.
- `src/world/le_reader.hpp`: `readU16`, `readI16`, `readU32`, `readI32` over `std::span<const std::byte>`. They return `std::expected<Value, ReadError>`; the error carries the requested offset/width and the first unavailable offset. Bounds use subtraction only (`rangeFits`), so offsets near `SIZE_MAX` cannot wrap.
- `src/world/world_probe.hpp`: `probeWorld()` reports the raw revision field, candidate revision, expected size, per-section available bytes, complete-section count, first missing offset, missing/surplus byte counts and whether structured inspection is safe, partial or unavailable, with a reason per status. It is `noexcept`, never modifies the input and never presents a result as proof: the format has no signature.
- `src/app/probe_report.cpp`: plain-text report of a probe. **File → Inspect World File** loads the selected file (up to 64 MiB) into `WorldFile` and shows this read-only report. No decoding, editing, rendering or saving exists yet.
- Build: `eggbert_world` (no Qt) and `eggbert_app` (Qt Widgets, everything except `main`) static libraries; `-Wall -Wextra -Wpedantic`; optional `-DEGGBERT_SANITIZE=ON` for ASan/UBSan.

Verification on 2026-11-22, Ubuntu 24.04, GCC 13.3.0, CMake 3.28.3, Ninja, Qt 6.4.2, no game data present:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel            # no warnings
ctest --test-dir build --output-on-failure  # 5/5 passed: world_file, layout, le_reader, world_probe, probe_report
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DEGGBERT_SANITIZE=ON
cmake --build build-asan --parallel && ctest --test-dir build-asan   # 5/5 passed under ASan/UBSan
xvfb-run -a timeout 5s ./build/eggbert-editor   # exit 124 (window stayed open)
```

A deliberate off-by-one mutation of the truncation comparison in `probeWorld()` made `world_probe` fail (24 checks), then was reverted.

Research evidence used so far: `blupi_recon` at commit `e05e22b990abb782c37a0d9fafa864d86a27e29f`, `src/decor.h` (`DescFile`, `Cellule`, `MoveObject`) and `src/decor.cpp` (`CDecor::Read`, `CDecor::Write`). Observed there: the reader skips `bigDecor` only when the pair is exactly 1.0 and reads 100 records only when `majRev == 1 && minRev <= 1`; the writer zero-fills the header and emits 1.3. This agrees with the layout table in `plan.md`. No original executable or retail sample has been tested yet.

## Next task: read-only decoded views

Finish milestone 1 without adding write support.

1. Add `src/world/world_view.hpp/.cpp`: bounded, read-only views over a probed input.
   - `HeaderView`: every field of the 868-byte header from `plan.md` with its exact absolute byte range: `majRev`, `minRev`, `reserve1[50]`, `posDecor`, `dimDecor`, `world`, `music`, `region`, `reserve2[51]`, `blupiPos[4]`, `blupiDir[4]`, `name[100]` (raw bytes, plus the bytes before the first NUL and the retained remainder) and `reserve3[196]`.
   - `TileLayerView`: signed 16-bit cell access in file order `[x][y]` with `cellOffset(x, y) = layerStart + 2 * (100 * x + y)`.
   - `ObjectPoolView`: per-slot 48-byte records decoded into the twelve signed 16-bit fields and three POINTs, each with its byte range; `type == 0` is reported as a free slot without hiding its other bytes.
   - A view exists only for sections the probe reports complete; a truncated input exposes exactly its complete sections. Surplus bytes are exposed as an opaque suffix range.
2. Tests build fixtures by hand with literal offsets: asymmetric layer content that detects `[x][y]`/`[y][x]` transposition, corner cells, signed limits, nonzero reserved bytes, name padding after the terminator, inactive slots with opaque bytes, all four revisions (1.0 has no `bigDecor` view and 100 slots), truncation inside each section and surplus bytes. The input must be byte-identical after every access.
3. Show the decoded header summary and counts in the GUI inspector, still read-only.

Acceptance evidence: every decoded value maps to a tested byte range; no view can be created for an incomplete section; tests do not use production code to build fixtures; Release build, CTest and the sanitizer build stay green with no game data.

## Slice roadmap toward the 1.0 gate

Completed slices are ticked only when their tests are green and committed.

- [x] 0 Scaffold
- [x] 1a Revision descriptors, bounded readers, `WorldProbe`, GUI probe report
- [ ] 1b Read-only header/layer/object views
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
