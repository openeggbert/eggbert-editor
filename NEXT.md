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
- `src/world/tile_tools.hpp`: `CellRect`, `CellMask` (selection), `lineCells`, `outlineCells`, `paintCells`, `fillRect`, `fillMask`, bounded 4-connected `floodFill`/`floodRegion`, `replaceValue`, `TilePatch` with holes, `copyPatch`, clipped `pastePatch`/`pasteFootprint`, flips. Every tool edits one layer through `EditBuilder` and returns the number of cells changed.
- Tile tools in the UI (`document_tab.cpp`, `main_window.cpp`): Browse, Select, Pencil, Eraser, Eyedropper, Line, Rectangle, Fill; Edit menu Cut/Copy/Paste/Delete, Select All/None/Invert, Find; Layer menu active layer, lock, isolate, Fill Selection, Replace Value, Clear Selection, Flip. Selection limits tools; strokes interpolate between mouse samples; edits above 64 cells go through `prompts.confirmBroadEdit` with cell/byte/offset counts; the tile clipboard is refused across different or unknown profiles (same tab always allowed).
- `src/world/profile.hpp`: per-game facts with provenance (music 0–10, region 0–23 / 0–31, tiles with collision data 0–236 / 0–440, SB2 loader remap of 48–67). `src/world/metadata.hpp`: `Metadata` read/apply (a `dimDecor` axis is rewritten only when its zero/non-zero meaning changes), `placeStart` (`cell * 64 + (2, 6)`), `toggleStartDirection`, `copyStartToAll`, Windows-1252 title encode/decode with round-trip for every byte.
- `src/world/object_catalog.hpp`: per-profile verified presets (SB1 31, SB2 42) with type, step durations, waits, in-cell offset, channel, icon, path flag, icon-is-choice flag and source line ranges; `objectTypeInfo` classifies a stored type as placeable, runtime-only or unknown for a profile (never inferred across games). `src/world/object_ops.hpp`: `poolCapacity`, `freeSlots` (lowest first), `recordForRequest` (engine-exact record incl. path end and step multiplication), all-or-nothing `createObjects`, `deleteObject` (type only), `duplicateObject` (raw 48-byte copy plus shift), `relocateStatic`, `moveStart`, `moveEnd`, `translatePath`, `objectsAt`.
- Object UI (`document_tab.cpp`, `objects_panel.cpp`, `main_window.cpp`): Objects dock (pool usage with hard-limit and estimated low-headroom notes, record list, optional free slots, exact numeric editors for durations, waits, positions, channel and icon), Object tool (select, cycle overlapping records by slot, drag by whole cells, drag a path's end box), Place tool with a per-profile preset chooser (drag sets a mover's path), Object menu (Create Verified Type, Duplicate, Delete, cell nudges, Properties and Pool Usage), pool usage in the status bar. Records whose type is not placeable for the chosen profile are view-and-delete only; `type`, `unknown1`, `unknown2`, `step`, `time`, `phase` are never offered for editing.
- `src/world/hex_model.hpp`: `offsetLabel`, `offsetCalculation` (spelled-out arithmetic), `decodeScalarAt` (signed/unsigned, POINT components, name bytes), `parseHexPattern`, `formatHex`, `findBytes` with wrap, `sectionJumps`. `src/app/hex_view.cpp`: virtualized read-only hex widget (section tints, changed/unsaved marks, selection, keyboard, copy). `src/app/inspector_panel.cpp`: Binary Inspector dock with jump (offset, `cell x,y`, `big x,y`, `slot n`, section), hex/ASCII search, bookmarks per tab, row width, original/current/side-by-side, details strip; two-way synchronisation with canvas cursor and object selection guarded against echo.
- `src/app/metadata_dialog.cpp`: World and Player Starts pages; OK is disabled for an unstorable title or a changed music/region outside the chosen profile's verified range (unknown profile locks both); notes for titles over 40 bytes and for files whose title the games ignore. World menu: Metadata, Player Starts, Copy Selected Start to All Four. Start tool with a start selector.
- `src/app/minimap_widget.cpp`, `statistics_panel.cpp`: Minimap dock (composite/foreground/bigDecor, viewport outline, click/drag pans the canvas) and World Statistics dock (tree of the headless numbers plus unsaved and changed-since-opened byte ranges; activating a row calls `MainWindow::locateTileValue/locateObjects/locateStart/locateOffsetRange`, which move the view and mark results without touching the document). View menu toggles and Reset Layout. The canvas keeps the world fitted until the user changes the view.
- `src/app/probe_report.cpp`: plain-text probe report plus decoded summary (name with escaped non-printable bytes, music/region/world, `posDecor`/`dimDecor`, four starts, non-zero reserved fields, per-layer count of cells that differ from -1, object slot counts). Shown by **File → Document Properties**.
- Build: `eggbert_world` (no Qt) and `eggbert_app` (Qt Widgets, everything except `main`); `-Wall -Wextra -Wpedantic`; optional `-DEGGBERT_SANITIZE=ON`.
- Tests: `tests/fixture.hpp` builds synthetic worlds from literal offsets and includes no production header.

Verification on 2026-11-22, Ubuntu 24.04, GCC 13.3.0, CMake 3.28.3, Ninja, Qt 6.4.2, no game data present:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel              # no warnings
ctest --test-dir build --output-on-failure  # 21/21 passed: layout, le_reader, world_probe, world_view, world_document, world_stats, offset_map, tile_tools, metadata, object_ops, hex_model, view_transform, probe_report, document_file, world_renderer, main_window, docks, tools, metadata_ui, objects_ui, inspector
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DEGGBERT_SANITIZE=ON
cmake --build build-asan --parallel && ctest --test-dir build-asan   # 21/21 passed under ASan/UBSan
```

`main_window` drives the real window offscreen: open through the action, pencil click (exactly offsets 1608–1609 change), drag as one undo step, layer switch, keyboard cursor and Space, save, external-change conflict (cancel / Save As / reload), revert, close with unsaved changes, read-only refusal, zoom about the pointer and view toggles. A screenshot of a synthetic world under Xvfb was inspected manually. The earlier `WorldFile` class and its test were removed; `world_document` covers byte retention.

## Research evidence log

Verified and reported observations are recorded with citations in `docs/research/evidence.md` (sources: `blupi_recon` at `e05e22b990abb782c37a0d9fafa864d86a27e29f`; archived `eggbert-generator` bundle at `069c409611eb779e9066e8d492faed4550dac9a8`). Uncommitted background notes on object placement, adaptive borders and rendering exist only in the development scratch area; each fact must be re-read at its citation and entered in `evidence.md` when the slice that depends on it is implemented. No original executable or retail sample has been tested yet; the research-data host `drive.openeggbert.com` is on the environment allowlist, but the automated download was refused by the session's permission layer and awaits the user's decision.

## Next task: basic World Doctor (milestone 6b)

1. Headless `src/world/doctor.hpp/.cpp`: `runDoctor(const WorldView &, GameProfile)` returns findings, never mutating. A finding has a stable rule ID, severity (error, warning, note), the profile it was judged under, an evidence/confidence note (proven limit, verified range or estimate), a semantic location (header field, cell, slot/field, file) and the exact byte range. Optional declarative fix: a list of byte replacements with a label, only for individually verified safe proposals.
2. Rule set for 1.0, each with positive, negative and boundary fixtures:
   - structure: truncation with first missing offset, surplus bytes, unknown/later revision, revision that is read-only in this product;
   - header: music/region outside the profile's verified range, start outside the map, start direction not 1 or 2, title unterminated (a C-string reader would run past the field), title over 40 bytes, title ignored for revision 1.2, non-zero bytes after the terminator (note);
   - tiles: values below -1, foreground values outside the profile's tile range, SB2 loader rewriting 48–67 to 215;
   - objects: type not placeable for the profile (runtime-only or unknown), positions outside the world, channel not one a preset of that type uses, path with zero duration, pool full (hard limit) and low headroom (estimate), free slots holding stale bytes (note);
   - no rule claims solvability or balance.
3. UI: Diagnostics dock listing findings with filtering by severity; activating one synchronises canvas, objects and inspector; World → Validate; an incremental run after edits and a full run before save (never blocking, never repairing); fixes shown with their byte diff and applied as one named undo step only after confirmation.

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
- [x] 4a Layer tools: pencil, eraser, eyedropper, line, rectangle, fill, selection, clipboard, find
- [x] 4b Metadata and player starts
- [x] 4c Verified object catalogs, object panel, create/duplicate/delete/relocate, capacity budgets
- [ ] 5 Verified adaptive brushes
- [x] 6a Read-only binary inspector
- [ ] 6b Basic World Doctor
- [ ] 7a Full-world PNG export
- [ ] 7a2 Workspace polish: themes, Mapping/Objects/Validation layouts, Reset Layout, shortcuts overview, preferences
- [ ] 7b User guide, notices, Linux packaging, smoke tests, opt-in local-sample matrix

## Explicit non-goals

Campaign/hub management, installation discovery or deployment, playtest launch, SB1↔SB2 conversion, Mobile Eggbert support, writing revisions 1.0/1.1, raw hex writes, ZIP handling and procedural generation stay out of scope (see **Post-1.0 roadmap** in `plan.md`).
