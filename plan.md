# Eggbert Editor 1.0 development plan

Build a modern Qt desktop editor for individual 2D world files from Speedy Blupi / Speedy Eggbert (SB1) and Speedy Blupi II / Speedy Eggbert 2 (SB2). Version 1.0 is deliberately limited to trustworthy, lossless editing of one `.blp` or `.xch` world at a time. It is not a procedural generator, campaign manager or game-installation deployment tool.

The user must be able to open a supported world, inspect its exact bytes, edit tiles, metadata, starts and verified objects, save it without unrelated byte changes, export one complete PNG preview, and understand format or compatibility problems before writing. The editor may use artwork from a directory manually selected by the user, but it must remain usable with schematic placeholders and must never write to that directory.

Use C++23, CMake and Qt 6 Widgets. Release under GPL-3.0-only. The initial scaffold is complete; `README.md` states current implemented behavior and `NEXT.md` defines the next bounded task.

## Fresh-context implementation protocol

A new agent must read `AGENTS.md`, `README.md`, `NEXT.md`, this entire plan, and the current source/tests before editing. Implement one reviewable vertical slice at a time: define observable behavior and byte-preservation invariants, create independent synthetic fixtures, implement the headless core, connect UI only after core tests pass, then run configure, Release build and CTest. Before stopping, update `NEXT.md` with verified results, unresolved evidence and one concrete next slice.

Research sources are evidence, not code donors. Pin the exact `blupi_recon` or archived-generator revision behind an observation, reconcile it with owned local samples, independently implement the behavior, and write fresh fixtures. Never transplant their source, tests or assets. Missing evidence produces a diagnostic or read-only path, not a guessed writer.

## Fixed 1.0 scope

| Area | Included in 1.0 |
| --- | --- |
| Games | SB1/Eggbert 1 and SB2/Eggbert 2, represented by an explicit document profile because the BLP header does not identify the game |
| Documents | One explicitly selected `.blp` or `.xch` world per tab; multiple independent tabs are allowed |
| Revisions | 1.0 and 1.1 read-only; 1.2 and 1.3 editable when structurally safe |
| World content | Foreground, `bigDecor` where present, verified metadata, four player starts, and every object type verified for the selected profile |
| Rendering | Asset-free schematic view plus optional exact-profile artwork from a manually selected read-only game-data directory |
| Assistance | Basic structural World Doctor, resource-budget warnings, complete read-only hex inspector and one full-world PNG exporter |
| Platform | Linux source build and first packaged release; keep portable C++/Qt boundaries for later platforms |

Explicit 1.0 non-goals are campaign/hub management, Campaign Packs or ZIP, direct installation scanning or writes, deploy/rollback, automatic playtest launch, SB1↔SB2 conversion, writing BLP 1.0/1.1, raw hex editing, experimental/hack writes, custom autotile-rule authoring, batch/headless export, SVG export, semantic merge and Windows/macOS packages.

## Compatibility and revision handling

Track **format revision** independently from **game profile**. Revision controls byte layout; profile controls catalogs, rendering, validation and verified editor operations. A file size, title language or unfamiliar ID cannot uniquely identify SB1 versus SB2. Suggest a profile only with displayed evidence and require a user choice when ambiguous. Switching preview profile never changes document bytes.

| Header revision | Foreground | `bigDecor` | Stored object records | Expected payload size | 1.0 policy |
| --- | --- | --- | ---: | ---: | --- |
| 1.0 | 100 × 100 | Absent | 100 | 25,668 bytes | Read-only structured view and hex inspection |
| 1.1 | 100 × 100 | 100 × 100 | 100 | 45,668 bytes | Read-only structured view and hex inspection |
| 1.2 | 100 × 100 | 100 × 100 | 200 | 50,468 bytes | Lossless editing and save |
| 1.3 | 100 × 100 | 100 × 100 | 200 | 50,468 bytes | Lossless editing and save |

Retain surplus bytes. A truncated document can expose every complete field for inspection, but structured writes remain disabled. Show the first missing offset. Treat unknown later revisions as unverified even if a historical reader would follow the newest known branch. Do not offer automatic upgrade of 1.0/1.1 in 1.0; a future explicit export can handle it with a loss report.

The SB2 reader normalizes stored foreground IDs 48–67 to 215. Display stored and profile-preview values separately and preserve stored IDs during ordinary save. The reconstructed retail writer clears parts of the header and emits 1.3; Eggbert Editor must instead patch only explicit user changes and retain the input revision.

## Binary layout and losslessness

Read and write explicit little-endian fixed-width values. Never serialize a native C++ structure. Treat tile IDs as signed 16-bit integers and POINT components as signed 32-bit integers. Validate all offset arithmetic before reading.

The fixed header is 868 bytes:

| Offset | Field | Wire representation |
| ---: | --- | --- |
| 0, 2 | `majRev`, `minRev` | Two 16-bit integers |
| 4 | `reserve1` | 50 × 32 bits |
| 204, 212 | `posDecor`, `dimDecor` | Two POINTs |
| 220, 222, 224 | `world`, `music`, `region` | Three 16-bit integers |
| 226 | `reserve2` | 51 × 16 bits |
| 328 | `blupiPos` | Four POINTs |
| 360 | `blupiDir` | Four 32-bit values |
| 376 | `name` | 100 raw bytes |
| 476 | `reserve3` | 196 × 16 bits, ending at offset 868 |

Canonical 1.2/1.3 sections are header `0..867`, foreground at 868 for 20,000 bytes, `bigDecor` at 20,868 for 20,000 bytes, and 200 object records at 40,868 for 9,600 bytes, ending at 50,468. Each tile layer is `[x][y]`, not row-major `[y][x]`: cell `(x,y)` begins at `layerStart + 2 × (100 × x + y)`. Use asymmetric fixtures to detect transposition.

Each object record is 48 bytes: six signed 16-bit values (`type`, speeds, waits and `unknown1`), three POINTs (`posStart`, `posEnd`, `posCurrent`), then six signed 16-bit values (`step`, `time`, `phase`, `channel`, `icon`, `unknown2`). Slot order, nominally inactive record bytes, reserved fields, name padding, unusual IDs and trailing data all belong to the source document.

Retain the complete original byte sequence and represent decoded fields as bounded views with source ranges. Every edit is a transaction containing semantic intent plus old/new byte ranges. Saving an untouched document produces identical bytes. A one-cell edit changes only its two bytes unless the user explicitly selected an adaptive operation with previewed neighbor effects.

## Capacity limits and runtime headroom

The reconstructed SB1 and SB2 engines define `MAXMOVEOBJECT = 200` and regard `type == 0` as a free slot. Revisions 1.0/1.1 store only 100 records; revisions 1.2/1.3 store 200. The engine also allocates some objects during play, so 200 stored active objects is a format maximum, not a guaranteed safe gameplay maximum.

Show stored capacity, active objects, free persistent slots and evidence-based estimated runtime headroom in the status bar, object panel and statistics. Before paste, duplicate, object creation or a compound verified preset, calculate maximum slot demand. Reject the whole operation before mutation if it cannot fit; never partially create a recipe or overwrite an existing slot. Ordinary Save never compacts, reorders or clears inactive records.

World Doctor warns at the hard cap and at profile-specific low headroom, clearly distinguishing proven limits from estimates. Test 99/100/101 and 199/200/201 requested objects, opaque bytes in free records, full-pool undo/redo and operations needing multiple slots.

## Desktop workspace

Use a modern Qt desktop shell rather than copying the original game's editor UI: tabbed documents, native menu bar, configurable toolbars, dockable panels, keyboard shortcuts, status bar, light/dark/system themes, persistent layouts and high-DPI icons. Provide Mapping, Objects and Validation layouts plus Reset Layout.

The central canvas supports smooth pan, zoom around pointer/focus, Zoom In/Out, Actual Pixels, Fit World and Fit Selection. Keep file, world-pixel and screen transforms explicit. Use nearest-neighbor scaling at suitable zooms. Keyboard navigation and visible focus must work without a mouse.

Planned 1.0 menus:

| Menu | Core commands |
| --- | --- |
| **File** | Open World, recent files, Close, Save, Save As, Save All, Revert, Export World PNG, document properties, Quit |
| **Edit** | Undo/Redo, Cut/Copy/Paste, Duplicate, Delete, Select All/None/Inverse, Find, Preferences |
| **View** | Zoom and fit commands, grid/coordinates, layer visibility, minimap, statistics, diagnostics, binary inspector, themes, docks and Reset Layout |
| **World** | Validate/World Doctor, compare with original/saved, metadata, starts and byte diff |
| **Layer** | Active layer, show/hide, lock, isolate, clear with preview, fill/replace and selection transforms |
| **Object** | Create verified type, duplicate, delete, relocate, edit start/end/path, exact properties and pool usage |
| **Help** | User guide, shortcuts, format notes, diagnostics bundle, license/notices and About |

## Map, minimap and statistics

Render foreground, optional `bigDecor`, player starts, verified objects and paths with independent visibility and locking. Provide pencil, raw eraser, eyedropper, line, rectangle, bounded fill, rectangular selection, cut/copy/paste and search by coordinate, tile ID, object type or slot. Preview clipping at map edges. Cross-profile clipboard data requires an explicit verified mapping or remains unavailable.

The minimap shows the whole 100 × 100 world and outlines the main viewport. Clicking or dragging its viewport pans the canvas. It can show a layer or composite and uses schematic symbols without proprietary assets.

World Statistics shows revision and exact size, occupied/empty cells by layer, unique/frequent tile IDs, used/free object slots, counts by verified type/channel/icon, four starts, motion paths, used-content bounds, diagnostic totals and unsaved byte ranges. Clicking a statistic selects or filters its cells/slots. Facts and profile-derived interpretations are labeled separately.

## Metadata, layers and starts

Edit verified 1.2/1.3 metadata: name, music, region, background behavior and four player starts/directions. Preserve raw title bytes after a terminator and diagnose legacy encoding uncertainty. Treat each `dimDecor` axis as the engine does (zero/nonzero), while retaining an unusual raw value until the user explicitly changes it.

Layer tools change only the selected unlocked layer by default. Original-style erase that clears both layers or associated objects is not a default operation. Every fill, paste, transform and multi-cell drag is one undo step with a before/after count and byte-range preview when broad.

## Verified object editing and relocation

Support every object type whose storage, placement and profile behavior are verified. Unknown or insufficiently evidenced records remain visible with numeric fields and raw bytes but are not offered as writable presets. Never infer support from a matching numeric ID across games.

The object panel lists slot, type, start, end, current position, channel and icon. Select overlapping records by slot. Draw paths and endpoint handles. Distinguish these operations:

- **Relocate static object** updates the verified position fields needed to keep it static while preserving its slot and unrelated fields.
- **Move start** or **Move end** changes only one endpoint.
- **Translate path** applies one exact delta to start, end and appropriate current coordinates.
- **Move selected cells/object** relocates an explicit multi-selection; the source is cleared only after destination acceptance.

Show a destination ghost plus coordinate conversion, collisions, locked layers, overlaps, clipping, slot demand and all affected fields. Never silently delete unrelated content. Drag, keyboard nudge and numeric entry use the same transaction. Opening a world or moving an object must not reset runtime fields; an explicit reset, if verified and retained for 1.0, previews its effects.

## Adaptive decoration

Implement only verified, profile-specific adaptive brushes needed for ordinary SB1/SB2 editing. Keep a raw pencil that writes exactly one selected ID with no neighbor or object changes. Adaptive brushes recalculate the edited footprint and required orthogonal neighbors using provenance-tagged family rules. SB2 includes separately verified cheese/cave families and special compound behavior; do not assume SB1 equivalents without evidence.

Preview all changed cells and slots for side effects outside the brush footprint. A stroke is one deterministic undo transaction: record the actual selected random variants so redo does not reroll. Never retile on open, save, profile switch or PNG export. Test all 16 adjacency masks for every verified family, boundaries, multi-cell stroke ordering, random variants with injected deterministic input and any object deletion/compound side effect.

## Asset loading and rendering

The user manually selects an SB1 or SB2 game-data directory. Scan it read-only, show which profile/resources were recognized and permit correction. Never modify or deploy into it. Persist its path as a local preference, not in BLP files or distributable test data.

Separate profile catalog IDs from atlas coordinates. Verify tile size, object anchors, offsets, alpha and draw order against asymmetric owned samples. Missing assets and unknown values use labeled schematic placeholders, so editing and CI require no proprietary files. Ship only original or suitably licensed UI artwork and generated test atlases.

## Basic World Doctor

World Doctor is a non-mutating validator, not an automatic level designer. Run incremental checks after edits and a complete deterministic audit on request or before save/export. Findings include stable rule ID, severity, profile, evidence/confidence, semantic location and exact byte range. Clicking a finding synchronizes map, properties and binary inspector.

The 1.0 rule set covers truncation and section boundaries, revision/size disagreement, unsafe or unknown IDs, invalid coordinates/directions, object-pool inconsistencies, incompatible verified type/channel/icon combinations, invalid motion endpoints, suspicious verified adaptive borders, blocked starts where collision evidence is reliable, and hard-cap/low-headroom warnings. It does not claim to prove solvability, balance or enjoyment.

Repairs are limited to individually verified safe proposals. Show every affected field and raw-byte diff, then apply one named undo transaction only after confirmation. Never repair on open, save, profile switch or validation. Unknown evidence offers inspection, not a destructive fix. Headless rules return diagnostics and optional declarative fixes without mutating the document; each has positive, negative and boundary fixtures.

## Read-only binary inspector

Provide a virtualized full-file hex view synchronized with map cells, objects, header fields, diagnostics and property selection. Show absolute and section-relative offset, raw hex, little-endian decoded value, signed/unsigned interpretations where relevant, original/current value and profile/revision assumptions. Explain the `[x][y]` cell-offset calculation and object slot/field calculation.

Support jump to offset, section, coordinate or object slot; hex/ASCII search; copy as hex; bookmarks; configurable row width; changed-byte highlighting; and side-by-side original/current comparison. Label header, layers, object pool, truncation and opaque suffix. Navigation, search and copying never dirty the document. Version 1.0 performs no raw hex writes.

## Full-world PNG export

Export one composite PNG of the complete 100 × 100 world. At 64 pixels per cell, native size is 6,400 × 6,400. Offer native size or a smaller scale, selected visible layers/objects/starts/paths, and solid or transparent background. Use the same transforms and draw order as the canvas and fix animation time for reproducibility.

Allow either the selected installation assets or the schematic renderer. Show dimensions and estimated memory before export; a native RGBA surface is about 156 MiB before temporary surfaces. Use bounded/tiled rendering if needed. Include profile, renderer version and settings as PNG metadata. Exact installation-asset renders may reproduce copyrighted artwork, so warn the user and do not bundle them with the editor.

## Saving, undo and external changes

Keep Save, Save As and PNG Export distinct. Save and Save As preserve representation; no conversion exists in 1.0. Use atomic replacement in the destination directory, for example `QSaveFile` without unsafe direct-write fallback. Recheck file identity/hash before commit. On conflict offer reload, compare or Save As; on failure leave the original intact and document dirty.

Each document owns one linear undo stack and clean index. Group strokes, fills, pastes, object moves, adaptive changes and safe Doctor fixes semantically. Undo after save may cross the clean index; a new edit after undo discards only the redo branch. Version 1.0 does not require branching history, snapshots or session recovery.

## Architecture

Keep `src/world` free of Qt. It owns original bytes, revision descriptors, bounded little-endian views, profile-independent document transactions, changed ranges and diagnostic locations. Profile catalogs and adaptive rules are provenance-tagged data with tests. World Doctor rules consume read-only views and produce diagnostics/declarative fixes.

Qt application code owns documents, undo commands, canvas, renderer, asset resolver, docks, dialogs and preferences. Route canvas, object and property edits through the same core operations. Renderer input is a read-only document snapshot plus explicit profile/assets/settings. A headless test must probe, decode, validate, edit and compare bytes without opening a window.

## Milestones and exit criteria

| Stage | Deliverable | Required evidence |
| --- | --- | --- |
| 0. Scaffold | C++23/CMake/Qt app, core library, GPL text and CTest | Clean checkout builds; app reports selected file size. Complete. |
| 1. Probe and read-only views | Bounded readers and revision descriptors for 1.0–1.3 | Independent fixtures decode; unrelated BLP, truncation and surplus never cause unchecked reads. |
| 2. Lossless 1.2/1.3 document | Header/layers/objects, transactions, Save/Save As, undo and external-change safety | Untouched round trip is identical; one-field/cell edits touch only named ranges; 1.0/1.1 remain read-only. |
| 3. Canvas and assets | Schematic canvas, manual asset directory, layers, selection, zoom, minimap and statistics | Asymmetric reference scene proves transforms/order; missing assets remain usable. |
| 4. Editing | Metadata, starts, layer tools and all verified SB1/SB2 object types | Edit/save/reopen representative synthetic and locally owned samples without unrelated changes. |
| 5. Adaptive tools | Verified SB1/SB2 adaptive brushes and deterministic undo | All verified adjacency masks and side effects pass independent fixtures. |
| 6. Inspection and validation | Full read-only hex view, basic World Doctor and capacity budgets | Every diagnostic/cell/slot maps to exact bytes; validation and inspection never mutate input. |
| 7. Export and Linux release | Composite PNG, guide, notices, packaging and clean-machine smoke tests | Native/smaller PNG alignment passes; Linux package starts with and without game assets. |

## Test strategy and release gate

Generate fixtures independently of production serialization. Cover every section boundary, x/y asymmetry, corner cells, signed limits, inactive/full pools, unknown IDs, nonzero reserved bytes, name padding, opaque suffix, ambiguous revision and truncation. Compare complete byte sequences, then assert named changed ranges for every edit.

Test save/undo/redo, repeated save, drag merging, external replacement, unwritable destinations and injected failure before atomic replacement. Run generated parsers and render inputs under address/undefined-behavior sanitizers where available. Asset-free CI builds on Linux; portability warnings remain visible even though packaging is Linux-only.

Maintain an opt-in local-sample matrix by game, edition, locale, revision, size and checksum. Never commit retail assets or worlds. Record which behavior was tested in an original executable and what remains inferred. A feature is “verified for profile” only when its evidence and fixture are recorded.

Version 1.0 ships only when both profiles can open, edit and reopen supported 1.2/1.3 samples; 1.0/1.1 stay read-only; unknown data survives; file conflicts/failures do not destroy input; object hard caps are enforced; and the application works without proprietary assets.

## Post-1.0 roadmap

Deferred work, not part of current implementation scope:

- write/conversion support for revisions 1.0/1.1 and loss-aware SB1↔SB2 conversion;
- game-installation discovery, staged deploy, backups, rollback and automatic playtest launch;
- Campaign Workspace, main/sub-hubs, campaign graph, manifests and safe directory/ZIP Campaign Packs;
- advanced World Doctor reachability and cross-world validation;
- experimental raw hex editing, hidden/unknown-value writes and custom adaptive-rule authoring;
- compound stamps, advanced alignment/distribution, batch edits and cross-world refactoring;
- PNG layers/crops/batches, SVG and headless CLI;
- autosave recovery, snapshots, three-way semantic merge and collaboration workflows;
- playtest traces/heatmaps, world regression scenarios, element encyclopedia and installation gallery;
- Windows and macOS packaging, localization and expanded accessibility certification.

Promote a roadmap item only by updating this fixed scope, acceptance tests, `NEXT.md` ordering and effort estimate. Do not implement it opportunistically during 1.0 work.

## Research references and licensing

The cancelled GPL-3.0-only `/rv/data/archive/trash/2026/eggbert-generator.bundle` may be audited at pinned commit `069c409611eb779e9066e8d492faed4550dac9a8` for format observations and known failure modes. Do not transplant its code, tests or assets. Relevant material includes its SB1/SB2 BLP format notes, world reader tests, provenance, decisions and level-design audit.

For reconstructed behavior, pin a `blupi_recon` revision and inspect `src/decor.h`, `src/decor.cpp`, `src/decmove.cpp`, `src/decdesign.cpp` and `src/decblock.cpp` for SB1, with corresponding `games/sb2/src/` files for SB2. `def.h` defines the 200-slot engine pool. Verify reconstruction provenance and original-game samples before claiming compatibility.

### Development game-data acquisition

Game data for local compatibility research may already be available on the development computer at `/rv/data/library/openeggbert/drive.openeggbert.com/Blupi/Games/Speedy_Blupi_(Windows)/`. At the time this plan was written, that directory contains `Speedy_Blupi_I.7z`, `Speedy_Blupi_II.7z`, `Speedy_Eggbert_1.7z` and `Speedy_Eggbert_2.7z`, plus demo archives. Treat this as an optional machine-specific location: check it read-only and do not make builds or tests depend on its existence.

In an isolated/cloud development environment such as Claude Code Cloud, an authorized user or agent may obtain the required research copy from the user-designated [Open Eggbert drive index](https://drive.openeggbert.com/Blupi/Games/Speedy_Blupi_%28Windows%29/). Download only the edition needed for the current verification task into a temporary or external cache outside the repository. Record the exact source URL, retrieval date, byte size and a cryptographic checksum in local test notes. Do not silently fall back between Blupi/Eggbert, game 1/game 2, demo/retail or regional editions.

Use `docs/claude-code-cloud.md` for the hosted Ubuntu/Qt setup, network allowlist, build commands and safe optional-data procedure. `CLAUDE.md` is the fresh-session entry point; it does not replace this plan or `AGENTS.md`.

Treat downloaded archives and extracted contents as untrusted input. Use bounded extraction into a fresh temporary directory; reject absolute paths, parent traversal, links/special files, case-folded collisions and excessive expanded size. Keep the extracted tree read-only during inspection and never execute downloaded binaries as part of ordinary unit tests. Network absence or inaccessible data must skip opt-in compatibility tests with a clear reason, not fail the asset-free build.

Availability at that location does not by itself establish redistribution rights. Never commit, upload, cache in CI artifacts, package or redistribute the archives, executables, retail worlds, sprites, audio or derived exact-render screenshots. The application itself does not download game data in 1.0; end users manually choose a directory they are authorized to use. Synthetic fixtures and generated atlases remain the only required test inputs.

Release under GPL-3.0-only with exact license text, corresponding source and reproducible build instructions. Inventory Qt libraries/plugins, fonts and other dependencies and include applicable notices. Resolve retail artwork only from a user-provided directory. Do not distribute game executables, retail assets, sample retail worlds or screenshots without separately verified permission.
