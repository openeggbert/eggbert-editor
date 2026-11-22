# Implementation handoff

## Current verified state

Milestones 0–2 are complete and milestone 3 (canvas and assets) has its workspace shell: worlds open in tabs, render schematically, can be edited with a raw pencil, undone, saved and reverted.

Implemented and covered by asset-free tests:

- `src/world/layout.hpp`: immutable `RevisionDescriptor` entries for header revisions 1.0–1.3 (header size, layers, object count/width, contiguous section table, expected size, 1.0 write policy, rule); `headerFields` and `objectFields` tables with exact offsets and wire types; `ByteRange` and `Point`. Only the four exact `(major, minor)` pairs are recognized.
- `src/world/le_reader.hpp`: `readU16/I16/U32/I32` over `std::span<const std::byte>` returning `std::expected<Value, ReadError>` with the first unavailable offset; bounds use subtraction only (`rangeFits`).
- `src/world/world_probe.hpp`: `probeWorld()` reports raw revision field, candidate revision, expected size, per-section available bytes, first missing offset, missing/surplus counts and whether structured inspection is safe, partial or unavailable. It never presents a result as proof: the format has no signature.
- `src/world/world_view.hpp`: `WorldView` with `HeaderView` (every header field, indexed reserved fields, raw name bytes, text before the first NUL and retained padding after it), `TileLayerView` (`[x][y]` cells with absolute byte ranges), `ObjectPoolView` (decoded 48-byte records, slot/field ranges, raw slot bytes, active/free counts) and the opaque suffix range. A section view exists only when that section is complete; out-of-range indices return `nullopt`.
- `src/world/world_document.hpp`: `WorldDocument` owns original, last-saved and current bytes of fixed length. `apply(Transaction)` validates everything before mutating: document writable (complete 1.2/1.3 only), changes sorted and non-overlapping, `before` bytes equal to current bytes, inside the layout, and outside the protected ranges (`majRev`, `minRev`, `reserve1`, `world`, `reserve2`, `reserve3`, opaque suffix). Linear undo/redo, clean index that undo may cross, redo branch discarded by a new edit, merge keys for drags, `changedRangesFromOriginal()` and `unsavedRanges()`. `EditBuilder` turns semantic edits (cell, music, region, `posDecor`, `dimDecor`, starts, name, object field/record, raw slot copy) into a minimal byte-level transaction; equal values produce no change, and a renamed title keeps the bytes after its new terminator.
- `src/app/document_file.hpp` (Qt Core only): `DocumentFile::load` (64 MiB limit), `diskState()`, `save()` and `saveAs()`. Writes use `QSaveFile` with the direct-write fallback disabled. `save()` first re-reads the destination and compares it byte for byte with the last saved bytes; modified, truncated, extended, deleted or unreadable destinations are a conflict and nothing is written. Save As onto another existing file needs explicit consent. Any failure leaves the destination bytes and the modified state unchanged; success calls `markSaved()`. A test hook can fail the save just before the atomic replacement.
- `src/app/view_transform.hpp`: the single world-pixel ↔ screen mapping (zoom about an anchor, pan, fit, clamp) and `cellAtWorld` (64-pixel cells).
- `src/app/world_renderer.cpp`: asset-free schematic renderer used by the canvas (and later minimap/PNG): background, `bigDecor` (2 × 2 cells from the anchor, 2 px down), starts, object bodies, foreground, then overlays (paths, outlines, labels, grid), with visibility options. Colours are fixed, 8-bit exact and stable per stored value; values below -1 get a warning colour.
- `src/app/world_canvas.cpp`, `document_tab.cpp`, `main_window.cpp`: tabbed workspace. File: Open World, Open Recent, Save, Save As, Save All, Revert, Document Properties, Close, Quit. Edit: Undo/Redo with labels. View: Zoom In/Out, Actual Pixels, Fit World, grid, labels and layer/object/start/path visibility. Tool bar: Browse, raw Pencil, active layer, raw tile ID, per-tab game profile. Every modal question goes through `MainWindow::prompts` hooks. Read-only documents show a banner and refuse every edit path. Window geometry and recent files persist in `QSettings`.
- `src/world/world_stats.hpp`: `computeWorldStats` (facts only: size, revision, per-layer occupied/empty/value counts/bounds, object slot usage and counts by stored type/channel/icon, starts, used bounds), `findCells`, `findActiveSlots`. `src/world/offset_map.hpp`: `locateOffset` maps any file offset to header field/element, layer cell, object slot/field or suffix; `cellByteRange`, `slotByteRange`.
- `src/app/minimap_widget.cpp`, `statistics_panel.cpp`: Minimap dock (composite/foreground/bigDecor, viewport outline, click/drag pans the canvas) and World Statistics dock (tree of the headless numbers plus unsaved and changed-since-opened byte ranges; activating a row calls `MainWindow::locateTileValue/locateObjects/locateStart/locateOffsetRange`, which move the view and mark results without touching the document). View menu toggles and Reset Layout. The canvas keeps the world fitted until the user changes the view.
- `src/app/probe_report.cpp`: plain-text probe report plus decoded summary (name with escaped non-printable bytes, music/region/world, `posDecor`/`dimDecor`, four starts, non-zero reserved fields, per-layer count of cells that differ from -1, object slot counts). Shown by **File → Document Properties**.
- Build: `eggbert_world` (no Qt) and `eggbert_app` (Qt Widgets, everything except `main`); `-Wall -Wextra -Wpedantic`; optional `-DEGGBERT_SANITIZE=ON`.
- Tests: `tests/fixture.hpp` builds synthetic worlds from literal offsets and includes no production header.

Verification on 2026-11-22, Ubuntu 24.04, GCC 13.3.0, CMake 3.28.3, Ninja, Qt 6.4.2, no game data present:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel              # no warnings
ctest --test-dir build --output-on-failure  # 13/13 passed: layout, le_reader, world_probe, world_view, world_document, world_stats, offset_map, view_transform, probe_report, document_file, world_renderer, main_window, docks
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DEGGBERT_SANITIZE=ON
cmake --build build-asan --parallel && ctest --test-dir build-asan   # 13/13 passed under ASan/UBSan
```

`main_window` drives the real window offscreen: open through the action, pencil click (exactly offsets 1608–1609 change), drag as one undo step, layer switch, keyboard cursor and Space, save, external-change conflict (cancel / Save As / reload), revert, close with unsaved changes, read-only refusal, zoom about the pointer and view toggles. A screenshot of a synthetic world under Xvfb was inspected manually. The earlier `WorldFile` class and its test were removed; `world_document` covers byte retention.

## Research evidence log

- `blupi_recon` at commit `e05e22b990abb782c37a0d9fafa864d86a27e29f`: `src/decor.h` (`DescFile`, `Cellule`, `MoveObject`), `src/decor.cpp` (`CDecor::Read`, `CDecor::Write`). The reader skips `bigDecor` only for exactly 1.0, reads 100 records only when `majRev == 1 && minRev <= 1`, and copies the title only for 1.3 or later; the writer zero-fills the header and emits 1.3 (1.2 with `reserve1[0] = 1` in the June 1998 demo build).
- Archived `eggbert-generator` bundle (SHA-256 `8bed90c38f4646061d50f8a130f8b303b064d0b49a237a81433598c793f5f668`, HEAD `069c409611eb779e9066e8d492faed4550dac9a8`, the commit pinned in `plan.md`), `docs/formats/BLP_SB1_WORLD.md`. Its authors report (not re-measured here): stock SB1 worlds are revision 1.2 with an all-zero title; stock SB2 worlds are 1.3; an empty cell is -1; the `world` field is written by neither game; demo and Valueware editions use `reserve1[0]` as a non-zero marker (256 in shipped demo worlds); SB1 tile icons with collision data are 0–236 and SB2 0–440. These observations guide fixtures (non-zero `reserve1[0]` must survive) but each still needs its own recorded evidence before a feature is called verified for a profile.
- Background research notes compiled from `blupi_recon` (same commit) on object placement, adaptive borders and rendering are not yet reconciled or recorded here; they will be entered with citations when the slices that depend on them are implemented.
- No original executable or retail sample has been tested in this repository yet.

## Next task: layer tools (milestone 4a)

Headless first, in `src/world/tile_tools.hpp/.cpp`, each producing one transaction through `EditBuilder` and touching only the selected layer:

1. Shapes: line (Bresenham), filled and outlined rectangle, bounded flood fill (4-connected, replaces the contiguous region of the start cell's value, limited to an optional cell rectangle), replace-all of one value inside a rectangle, clear rectangle to -1. Everything is clipped to the map; results report cells changed.
2. Clipboard: copy a rectangle of one layer to a `TilePatch` (width, height, values, source layer); paste at a target cell with clipping at map edges and an optional "skip -1" mode; cut = copy + clear as one transaction. Patches carry the profile they were copied under; pasting across profiles is refused in the UI.
3. Selection transforms on a patch: flip horizontally/vertically (no tile remapping is claimed; it moves stored IDs only).
4. UI: tools Eraser (raw, writes -1 to the active layer only), Eyedropper, Line, Rectangle, Fill, Select; Edit menu Cut/Copy/Paste/Delete/Select All/None; Layer menu fill/replace/clear with a before/after cell count preview when more than 64 cells change; Find by coordinate, tile value, object type or slot. Layer lock and isolate.
5. Tests: exact changed-cell sets for every shape including edges and corners, fill boundaries and limits, paste clipping, cut/paste round trip, one undo step per operation, other layer and objects byte-identical, and offscreen gestures for each tool.

## Slice roadmap toward the 1.0 gate

Completed slices are ticked only when their tests are green and committed.

- [x] 0 Scaffold
- [x] 1a Revision descriptors, bounded readers, `WorldProbe`, GUI probe report
- [x] 1b Read-only header/layer/object views and decoded inspector summary
- [x] 2a Lossless document: byte-range transactions, changed ranges, linear undo/redo with clean index, write gating (1.2/1.3 only, complete layout only)
- [x] 2b Save/Save As with atomic replacement, external-change detection and failure injection
- [x] 3a Tabbed workspace shell, schematic canvas, pan/zoom, layer visibility, raw pencil
- [x] 3b Minimap and World Statistics
- [ ] 3c Manual read-only asset directory and artwork rendering
- [ ] 4a Layer tools: pencil, eraser, eyedropper, line, rectangle, fill, selection, clipboard, find
- [ ] 4b Metadata and player starts
- [ ] 4c Verified object catalogs, object panel, create/duplicate/delete/relocate, capacity budgets
- [ ] 5 Verified adaptive brushes
- [ ] 6a Read-only binary inspector
- [ ] 6b Basic World Doctor
- [ ] 7a Full-world PNG export
- [ ] 7a2 Workspace polish: themes, Mapping/Objects/Validation layouts, Reset Layout, shortcuts overview, preferences
- [ ] 7b User guide, notices, Linux packaging, smoke tests, opt-in local-sample matrix

## Explicit non-goals

Campaign/hub management, installation discovery or deployment, playtest launch, SB1↔SB2 conversion, Mobile Eggbert support, writing revisions 1.0/1.1, raw hex writes, ZIP handling and procedural generation stay out of scope (see **Post-1.0 roadmap** in `plan.md`).
