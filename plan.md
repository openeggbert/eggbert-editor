# Eggbert Editor development plan

Build a desktop editor for existing 2D Windows worlds from Speedy Blupi / Speedy Eggbert (SB1) and Speedy Blupi II / Speedy Eggbert 2 (SB2). A designer should be able to open a world, change one tile, save, and verify that exactly the intended bytes changed. They should be able to try the edit in their chosen game using a disposable copy and return to the same editing session.

Users can work on an individual file, an installed world's data, or a Campaign Workspace. In a campaign, authors can also start with a blank world and place every tile, object, start and connection themselves. Procedural level and campaign generation are outside the product scope. The author makes all decisions about geometry and gameplay.

Use C++23, CMake and Qt 6 Widgets. The initial scaffold provides a file-size inspector, original byte storage and layout constants. The reader, editing tools and exporters below are implementation milestones.

## Fresh-context implementation protocol

This document is the product and technical specification, not a claim that the listed features already exist. `README.md` describes what the current checkout can actually do, and `NEXT.md` identifies the next bounded implementation slice. A new implementation agent should read `AGENTS.md`, `README.md`, `NEXT.md` and this entire plan before editing. It should inspect the current source and tests rather than reconstructing them from prose.

Implement one reviewable vertical slice at a time. For each slice, first state the observable behavior and byte-preservation invariant, add independent synthetic fixtures, implement the smallest core API, connect UI only after the headless behavior is tested, and run the documented configure/build/CTest commands. Update `NEXT.md` with completed behavior, exact test evidence, unresolved questions and the next small slice before ending a work session. Keep stable design decisions here; keep transient status out of this plan.

Do not begin with drawing, asset extraction or campaign UI. The critical path is bounded probing, lossless decoding, exact byte-range editing and atomic save. A plausible preview backed by a lossy codec would undermine every later feature. Treat the archived generator and reconstructed games as research evidence only: record the exact revision and observation, reconcile it with local samples, then independently implement the editor behavior. Do not transplant their source, tests, artwork or other assets.

Every mergeable change must keep the application buildable, keep `src/world` independent of Qt, add or update asset-free tests, retain SPDX `GPL-3.0-only` notices in source/build files, and avoid introducing proprietary game data. Do not silently widen scope when evidence is missing. Add a diagnostic or inspection-only path, document the unknown, and leave writes disabled for the affected structure. A stage is complete only when its exit conditions below are demonstrated; merely adding classes or menu entries is not completion.

Recommended implementation order inside the milestones is:

1. Define revision descriptors and overflow-safe little-endian bounded reads; probe file kind, revision, expected sections and surplus/truncation without mutating input.
2. Decode the header, layers and object records into views that retain source offsets. Prove untouched round trips for every layout and opaque suffix.
3. Add byte-range transactions, semantic diffs, per-document undo/redo and atomic Save/Save As. Prove one-field and one-cell changes affect only named ranges.
4. Add an asset-free schematic canvas, coordinates, layers, selection, zoom/pan, minimap and statistics. Only then connect local installation assets and profile catalogs.
5. Complete verified SB1 behavior, then verified SB2 behavior, before enabling advanced/raw edits or conversions.
6. Add installation staging/deployment and recovery before playtest mutates any game directory.
7. Add Campaign Workspace, graph validation and Campaign Pack import/export after individual-world persistence is trustworthy.
8. Add deterministic image/statistics export, packaging and clean-machine release validation.

## Three ways to work

| Entry point | Typical task | Where changes go |
| --- | --- | --- |
| **Open World File** | Inspect and edit an explicitly selected `.blp` or `.xch` received from another author | Save to the selected file or use Save As for a copy. Track external changes. |
| **Open Game Installation** | Choose a concrete installed game and browse its complete world set by slot, role, name and revision | Stage changes against the scanned installation, review them, then explicitly deploy in place with a recoverable snapshot. |
| **Open Campaign Workspace** | Manually create and edit a main hub, themed or subordinate hubs, missions, and final or bonus worlds as a connected set | Save world files and a manifest in a working directory. Import or export a shareable Campaign Pack as a directory or ZIP archive. |

### Installed games

Accept a user-selected root. Discovery may suggest existing locations, including a Wine prefix, for the user to confirm. Scan it read-only first. Identify the actual data directory and executable, suggest an edition/profile with evidence, and allow the user to correct it. List recognized world files, ambiguous BLP resources, missing expected slots and duplicates. Preserve original spelling on disk. Resolve game filenames with case-insensitive lookup and reject ambiguous collisions such as names distinguished only by case on a case-sensitive host.

Keep edits staged while the user browses. Before deployment, list exact destination paths, replacements, collisions and profile warnings. Recompute file hashes from the original scan, including detecting an unexpected file at a previously unoccupied slot. On conflict, retain the pending edits and offer compare, reload, or Save As. Request an explicit overwrite choice using the current destination as the backup baseline.

For a multiworld deploy, validate and serialize every destination first, take a recoverable snapshot of existing files, and record which destinations were originally absent. Write temporary files on the same filesystem as their targets and atomically replace each file. Use a transaction journal with original and intended hashes to detect an interrupted deployment. Per-file replacement alone cannot guarantee whole-set atomicity. Prevent concurrent deployments by this editor, recheck each destination before commit, and offer rollback or completion on restart. Rollback must detect intervening external edits before replacing them. A final report identifies each applied or skipped file and its recovery snapshot.

### Campaign Workspace and Campaign Pack

Assign each world a stable workspace ID, a target slot/world number, a path and a role. Support custom main hubs, themed or subordinate hubs, missions, finales and bonus worlds. Let an author add an existing file or explicitly create a blank world for a verified profile/revision. Blank initialization sets encoding defaults and leaves placement and gameplay to the author. The graph view should link each edge to the associated source world, door or exit, destination world and optional key requirement. Allow manual edits to nodes, roles, slot numbering and relationships. Preserve existing references until the author accepts a concrete set of edits.

Keep manifest-only annotations distinguishable from references encoded by the game. For each supported profile, record how an actual door, key or exit encodes its target, what completion flags it observes and which slot numbers the engine reserves. Show unresolved relationships as such. Trace outbound and inbound links before moving a world to another number. Validate duplicate slots, missing endpoints, unconnected missions, unreachable hubs, missing keys and routes to finales. Cycles and optional branches can be intentional. Report them with locations and allow the author to annotate intent. Structural reachability alone cannot establish that a mission can be played to completion.

A **Campaign Pack** is the portable form of the workspace. Support a directory or ZIP with the same logical root layout. Define a versioned UTF-8 manifest and a stable schema for these entries.

| Manifest area | Information |
| --- | --- |
| Identity and attribution | Pack ID, schema version, pack version, title, author, description and license covering the submitted campaign content |
| Target | Game, edition/profile, supported world revisions, compatibility requirements and any required local installation resources |
| Worlds | Stable IDs, relative member paths, numbers, roles, display titles and SHA-256 digests of exact world payloads |
| Relationships | Source and destination IDs, transition kind, door/key/exit references, conditions and whether each reference is verified in a world |
| Evidence and previews | Validator version, target profile, recorded checks and manual playtest evidence. Allow optional tool-generated previews made with redistributable material. |

Limit pack members to the manifest, declared campaign worlds, permitted previews and attribution material. Exclude game executables and copyrighted game assets. Export first to staging and verify the manifest against the staged bytes. Establish one path spelling, stable entry ordering and manifest serialization, fixed ZIP timestamps/permissions and pinned compression settings for reproducibility. Report missing or extra members. Verify deterministic byte output for identical input and tool version.

Import into a fresh staging directory. Apply the same path policy to directory and ZIP inputs. Check normalized relative paths, case-folded duplicates, reserved filesystem names and path length before writing. Reject absolute paths, parent traversal, symlinks, special files and entries that resolve outside the staging root. Enforce member-count, per-file, total expanded-size and compression-ratio limits while streaming. Verify member sizes and checksums, schema compatibility and content kind before presenting an import preview. Preserve unknown optional metadata across a workspace save. Open a newer unsupported required feature or major schema for inspection only.

On import, compare existing pack IDs, world slots and filenames. Offer a fresh workspace, explicit rename/remap or reviewed replacement. Show every reference affected by a remap. Canceling or failing an import leaves the previous workspace intact. Importing a pack never deploys it into a game installation. Use the installation deployment flow when the user explicitly chooses a target.

## Compatibility model

Track two independent properties of each document. The **format revision** selects the byte layout. The **game profile** selects the tile catalog, object presets, preview behavior and allowed playtest target. There is no game identifier in the header, and both games can use v1.3. A matching size, a language-specific title or an unfamiliar tile ID cannot uniquely identify a game. Offer a profile choice when the evidence is ambiguous. Changing the preview profile must leave the bytes intact.

| Revision in header | Foreground | Background (`bigDecor`) | Object slots | Expected bytes with the 868-byte header |
| --- | --- | --- | ---: | ---: |
| 1.0 | 100 × 100 | Absent | 100 | 25,668 |
| 1.1 | 100 × 100 | 100 × 100 | 100 | 45,668 |
| 1.2 | 100 × 100 | 100 × 100 | 200 | 50,468 |
| 1.3 | 100 × 100 | 100 × 100 | 200 | 50,468 |

The historical readers take the larger object branch from 1.2 onwards. Treat a later unknown revision as unverified even if those readers use the same branch. Retain surplus bytes. A truncated file may expose complete fields for inspection but cannot support structured writes across a missing section. Show the offset of the first missing byte.

The reconstructed retail writer emits v1.3. The archived stock SB1 study found v1.2 at 50,468 bytes in all 140 first-game worlds it examined. The SB1 June demo writes v1.2 with `reserve1[0] = 1`. Its reader accepts a nonzero marker for official world numbers below 200, and the archived research records the value 256 in the wild. Preserve the full stored value. Treat the demo as a separate target where a world number and installation are known.

Verify regional, demo and retail compatibility using local samples before offering a compatibility claim. Retain the input revision by default. Make version conversion a separate export with a field-by-field loss report, especially for v1.0 without a background and earlier branches with only 100 slots.

### Cross-game conversion between SB1 and SB2

Support explicit conversion of a world from the SB1/Eggbert 1 profile to SB2/Eggbert 2 and back. This **game-profile conversion** is separate from converting the on-disk revision between 1.0, 1.1, 1.2 and 1.3. Numeric tile or object IDs that happen to match are not evidence of equivalent behavior. Build provenance-tagged mapping catalogs for tiles, adaptive material families, moving-object types, channels/icons, player starts, music, region and any verified door/key/world-link semantics. Each mapping must state whether engine behavior, appearance, collision and placement rules are equivalent or merely similar.

Run conversion as a wizard that creates a new target document or Campaign Workspace export; never overwrite or silently change the open source world. The preflight scans the complete source and classifies every relevant value and relationship:

| Classification | Required behavior |
| --- | --- |
| Exact | Verified target representation has equivalent stored and runtime behavior. Convert automatically and list it as lossless. |
| Equivalent remap | A different target ID or representation is verified to preserve the intended behavior. Show source and target values and locations. |
| Approximation | Only a visually or mechanically similar substitute exists. Require the author to accept a named replacement or choose another one. |
| Unsupported/drop | The target game has no safe representation, such as a verified SB2-only saw when converting to SB1. Show every affected cell/slot/path and require an explicit drop, replacement or cancellation decision. |
| Unknown/unsafe | Evidence is insufficient or the target could index an unsafe resource. Block playable export until the author resolves it; allow a report-only conversion analysis. |

The loss preview must summarize counts and also provide a navigable itemized list with source coordinates, object slots, raw IDs/fields, reason, proposed target, confidence and resulting byte ranges. Selecting an entry centers the map and synchronizes both property and binary inspectors. Offer source/target schematic previews and overlays so changed, substituted and removed content is visible. Include changes caused indirectly by object-pool limits, missing `bigDecor`, different catalog limits, adaptive retiling, normalized SB2 IDs, coordinate/anchor rules and campaign references. “No warnings” is not the same as proven playability.

For each unresolved item, allow only explicit policies: cancel conversion, select a verified target substitute, leave the destination empty, keep it as a non-playable diagnostic placeholder in the editor workspace, or handle the item manually after conversion. Never write a source-only ID into the target BLP merely to retain its number. A conversion can attach an optional versioned sidecar containing source identity, exact source checksum, chosen mappings and omitted source data so an editor-assisted reverse conversion can recover authoring information. The game-facing BLP remains clean and must work without this sidecar; loss is permanent when only that BLP is shared.

The final confirmation records a deterministic conversion report in JSON and human-readable form: source/target profiles and revisions, tool/catalog versions, checksums, exact/equivalent/approximate/dropped/blocked counts, all user decisions and validation results. The converted document starts with its own undo/history and path while the source remains byte-for-byte unchanged. For a campaign, preflight every world and cross-world reference before writing any output; slot remapping and loss decisions must be consistent across the set.

Create independent fixtures for the verified common subset and for each known one-game-only family, including an SB2 saw-to-SB1 case. Test exact round trips only for values explicitly classified as common and reversible. Test expected-loss conversions, canceled decisions, target object-pool overflow, revision downgrade interactions, optional sidecar-assisted recovery, deterministic reports and batch campaign rollback. No mapping is accepted solely because screenshots look similar or IDs coincide.

## On-disk layout

Read the Windows payload with explicit little-endian integers. Each stored POINT has two signed 32-bit components. Treat signed 16-bit tile IDs as signed values throughout the reader and validator. Record offsets and widths in one layout module. C++ structure padding, host endianness and platform `long` widths must have no effect on file IO.

| Header offset | Field | Wire representation |
| ---: | --- | --- |
| 0, 2 | `majRev`, `minRev` | Two 16-bit integers |
| 4 | `reserve1` | 50 × 32 bits |
| 204, 212 | `posDecor`, `dimDecor` | One POINT each |
| 220, 222, 224 | `world`, `music`, `region` | Three 16-bit integers |
| 226 | `reserve2` | 51 × 16 bits |
| 328 | `blupiPos` | Four POINTs, one per player start |
| 360 | `blupiDir` | Four 32-bit direction values |
| 376 | `name` | 100 bytes |
| 476 | `reserve3` | 196 × 16 bits, ending at byte 868 exclusive |

The fixed grid remains 100 × 100. The game treats each `dimDecor` axis as zero or nonzero. A control for that behavior must preserve unusual raw values until that axis is explicitly edited. Preserve the full name field including bytes after a terminator. Determine the appropriate legacy text encoding from regional samples. Show the raw bytes and handle unrepresentable edits before writing the field.

| Canonical v1.3 section | Start offset | Length |
| --- | ---: | ---: |
| Header | 0 | 868 |
| Foreground cells | 868 | 20,000 |
| Background cells | 20,868 | 20,000 |
| Moving-object pool | 40,868 | 9,600 |
| End of payload | 50,468 | |

Each tile layer stores signed 16-bit IDs in `[x][y]` order. For cell `(x, y)` the byte offset within the layer is `2 × (100 × x + y)`. Editing coordinate (1, 0) touches bytes 200–201 within that layer. Use a deliberately asymmetric map to catch transposition. The background's storage grid and the way the game draws background graphics are independent. Verify positioning, scrolling and clipping in each profile.

Each of the 200 v1.3 `MoveObject` slots occupies 48 bytes. The earlier 100-slot pool uses the same record width.

| Offset in slot | Fields in wire order | Representation |
| ---: | --- | --- |
| 0–11 | `type`, `stepAdvance`, `stepRecede`, `timeStopStart`, `timeStopEnd`, `unknown1` | Six signed 16-bit values |
| 12–35 | `posStart`, `posEnd`, `posCurrent` | Three POINTs |
| 36–47 | `step`, `time`, `phase`, `channel`, `icon`, `unknown2` | Six signed 16-bit values |

Slot order and inactive slot bytes belong to the document. Free-slot selection, creation and explicit deletion each need a defined field mask. Document intentional changes for a newly allocated slot. Preserve other slot bytes, including on moves and property edits.

### Capacity limits and runtime headroom

Treat every format and engine capacity as an explicit profile/revision budget. The reconstructed SB1 and SB2 engines define `MAXMOVEOBJECT` as 200 and regard a slot with `type == 0` as free. However, BLP revisions 1.0 and 1.1 store only the first 100 object records; their readers clear the full pool and load half of it. Revisions 1.2 and 1.3 store all 200 records. Therefore a lossless 1.0/1.1 document has a hard persistent capacity of 100 unless the user performs an explicit revision upgrade. Do not expose the engine's zero-filled upper half as 100 additional savable slots in an old-revision document.

The format ceiling is not necessarily a safe gameplay ceiling. Runtime code also calls `MoveObjectFree` for objects created during play, so a world that occupies every persistent slot may leave no capacity for bombs, effects, spawned actors or other profile-specific behavior. Maintain separately:

- **stored capacity**: 100 or 200 records according to the selected output revision;
- **active stored objects**: slots with a verified nonzero type, without discarding bytes in nominally free slots;
- **free persistent slots**: records available to authoring operations;
- **estimated runtime headroom**: free slots remaining after the world loads, with profile/evidence and no unsupported promise that a particular reserve is sufficient;
- **operation demand**: the maximum additional slots required by a paste, stamp, adaptive/compound recipe, conversion or repair before it starts.

Show these numbers in the status bar, object list and World Statistics, using warning and critical thresholds that remain distinguishable from the hard cap. A creation operation that cannot fit must be rejected before mutation and must identify its demand and available capacity. Multi-slot operations are all-or-nothing; never partially place a recipe or overwrite the oldest/last slot. Offer reviewed deletion, a different target revision, or an explicit compatible compaction/remap where proven safe. Ordinary Save never compacts, reorders or clears nominally inactive records.

Revision downgrade and SB1/SB2 conversion must preflight both the count and representability of every active slot. Moving active records from slots 100–199 into free lower slots is a separate, previewed remap because slot order can affect observed behavior; more than 100 active objects cannot fit a 1.0/1.1 payload without user-approved removal or substitution. Preserve the original document and produce a loss report when conversion cannot retain the pool.

Also expose other fixed budgets rather than scattering magic numbers: the 100 × 100 stored grid, foreground/background availability by revision, four player starts, fixed title byte width, coordinate ranges, Campaign Pack resource limits and any verified profile-specific singleton or cardinality rule. Add boundary tests for 99/100/101 and 199/200/201 requested objects, free slots containing opaque bytes, compound operations needing multiple records, old-revision upgrades/downgrades, pool compaction, undo/redo at capacity and runtime-headroom diagnostics. World Doctor should report a full pool and evidence-based low-headroom risk without deleting anything.

The games use the `.blp` extension for several kinds of resources, including images, audio and MIDI. An exchanged `.xch` world is a copied world payload. Probe the revision and expected sections against the file length and context before enabling structured edits. Cover unrelated resource files and matching-size arbitrary input in probe tests. Expose inconclusive input in an inspection view until there is sufficient evidence to decode it.

## Lossless document and file operations

1. Retain the complete source byte sequence. Decode known fields as typed views with coordinates, slot numbers and byte ranges. Keep an explicit reason for every default supplied to a preview when a field or layer is absent.
2. Apply edits as transactions with before/after bytes and a semantic label. Store changes relative to the original. Handle overlapping edits against the current state. A no-op transaction leaves the document clean. Save an untouched document byte-for-byte, including unknown IDs, reserved fields, name padding, inactive slots and trailing data.
3. Keep **Save**, **Save As** and **Export for…** distinct. The first two preserve representation. Export can apply a documented profile conversion or original-editor behavior after showing changed sections, IDs and discarded values. Give each conversion rule a fixture. Keep the open source document untouched by exports.
4. Use atomic replacement in the destination directory, for example through Qt's `QSaveFile` with direct-write fallback disabled. Recheck external changes since opening and handle explicit overwrite decisions. Keep the previous file intact and the document dirty if the commit fails. Associate Save As with its new path only after success.
5. Give each open document its own undo stack, clean index, revision, display profile and selected layer. Group drags, fills, object moves, paste operations and conversions into single undo steps. Undoing after a save can cross the clean index. A fresh edit after undo discards only the redo branch. Store recoverable work separately from the game files.

## What the original editor changes

| Operation in the games | Consequence for this editor |
| --- | --- |
| Retail saving clears the header before filling selected fields and writes revision 1.3. Reserved fields and the former `world` value can disappear. | Patch only edited fields. Expose canonicalization in an explicit export. |
| SB2 reading maps foreground IDs 48 through 67 to 215. | Show stored ID and SB2 preview mapping side by side. Preserve the stored ID on ordinary saves. |
| Entering build mode restores current object position from its start, sets `step` to `STOPSTART`, zeros time and phase, and deletes `TYPE_BALLE` projectiles. | Opening the document must leave these fields and slots untouched. Offer runtime previews using a separate copy. |
| Preparing to play resets runtime state, removes projectiles, sorts objects and can filter object types for multiplayer. | Preserve source slot order. Use disposable playtest input and expose predicted effects. |
| Original cell deletion clears both tile layers and removes every object whose start or end lies on that cell. | Default erasing targets the active layer or selected slot. An original-style erase previews all affected layers and slot numbers and forms a single undo step. |
| The user palette hides six final official items and the user editor mirrors team starting positions. | Distinguish palette visibility from file support. Enable independent inspection of the four spawns and offer linked editing as a choice. |

### Original-style adaptive decoration and compound placement

The reconstructed editors call `AdaptBorder` after deletion and many palette placement operations. This is not merely a visual preview: it rewrites foreground tile IDs. The known routine recalculates the edited cell plus its four orthogonal neighbors from a four-bit adjacency mask and a material-family lookup table. It treats out-of-bounds neighbors as connected and uses the effective small/large world dimensions in parts of the border test. Some results choose a random visual variant. SB2 adds distinct cheese and cave families, extra material ranges, and special vertical decoration behavior; placing a non-passable foreground tile can also delete a moving object associated with that cell. Palette recipes elsewhere can create or replace a tile and a moving-object record together. These are observable editor operations, not properties that should be re-applied when a file is merely opened.

Implement this as profile-versioned **adaptive brushes** and **compound placement recipes**, separate from the raw tile pencil:

- A raw pencil writes exactly the selected ID and changes no neighbor or object. An adaptive brush computes the complete proposed change set for the selected SB1/SB2 material family. A compound recipe can include tiles, decorative tails, moving-object allocation/deletion and preset fields.
- Highlight every affected cell and slot before a single click or drag is committed when the operation has side effects outside the brush footprint. The status bar and undo label should say, for example, “Paint cheese edge: 5 cells changed”. Property and byte-diff views must expose the chosen variants.
- Make original-compatible random variants reproducible inside an undoable transaction by recording the actual selected IDs and the operation's RNG inputs. Also offer a locked variant or deterministic authoring choice. Undo/redo must replay recorded results rather than draw new random values.
- Recalculate only the dependency neighborhood for an interactive stroke, but normalize the final stroke as one transaction so the result does not depend on mouse event frequency. For fill, paste, transform and imported layers, preview an explicit **Retile Selection/Region** operation. Never silently retile an existing world on open, save, profile switch or image export.
- Keep family membership, adjacency rules, lookup tables, random alternatives, small-world boundary behavior and compound recipes in provenance-tagged profile data where possible. Code should evaluate generic rules with bounds checks. An unknown family remains editable as raw IDs and does not borrow a visually similar rule from the other game.
- The current SB2 reconstruction exposes cheese/cave and extended border tables in `games/sb2/src/decor.cpp`, with tables in `games/sb2/src/decblock.cpp`; placement call sites are in `games/sb2/src/decdesign.cpp`. The analogous SB1 paths are `src/decor.cpp`, `src/decblock.cpp` and `src/decdesign.cpp`, but `src/PROVENANCE.md` currently marks the SB1 adaptive tables among values still requiring reconstruction/checking. Verify the pinned reconstruction revision and original-game samples before claiming exact SB1 compatibility.

Create independent mask fixtures for all 16 orthogonal combinations per verified family, corners and effective small-world bounds, deletion next to a family, multi-cell strokes and order independence. Add fixtures for each verified random alternative using an injected deterministic source, full object pools, object deletion side effects, SB2 cheese/cave cases and special vertical decorations. Compare the full before/after byte ranges and confirm that raw-pencil, open, ordinary save and preview-only operations never invoke adaptation.

## Editing workspace

- Use a central zoomable map with keyboard navigation, a tile/object palette, a property inspector and a diagnostics pane. Display the active profile, selected layer, grid position and raw ID. Preserve focus, selection and viewport after undo and profile changes. Zoom continuously around the pointer or keyboard focus, with commands for zoom in, zoom out, actual pixels, fit world, fit selection and reset view. Support smooth pan, temporary hand-tool panning, configurable zoom limits and exact numeric zoom. Keep editing coordinates stable at every scale and prevent a zoom gesture from painting accidentally.
- Provide visibility and locking per layer. Add pencil, eraser, eyedropper, line, rectangle, bounded fill, rectangular selection, copy, cut and paste. Define behavior at map edges and report clipped regions before applying a paste. Paste only compatible layers across game profiles or request an explicit mapping.
- Display a slot list with type, start, end and current position. Select overlapping objects by slot. Draw motion paths and handles for both endpoints. Keep moving an endpoint distinct from translating the full path. Offer snap and exact numeric entry. A full pool should identify its capacity and leave existing objects intact.
- Provide documented presets per profile with separate controls for movement speeds, waits and graphics. Show hidden and unknown IDs numerically. Edit world name, region, music, background behavior and starts with their corresponding raw values visible on demand. Layer isolation and search by tile ID, object type, slot, coordinate or diagnostic make dense worlds navigable.
- Support keyboard-only placement, a visible focus indicator, high-DPI zoom and distinguishable selection and warning markers. Verify terminology and designer conventions against the [SB1 manual](https://www.blupi.org/download/man/SpeedyBlupi-en.fr.de.pdf) and [SB2 manual](https://blupi.org/download/man/SpeedyBlupi-2-en.fr.de.pdf).

### Overview, minimap and statistics

Keep an always-available overview in a dock or corner overlay. It renders the whole 100 × 100 world and outlines the portion currently visible in the main canvas. Clicking or dragging its viewport rectangle pans the editor; zooming the main canvas updates it immediately. Allow the overview to be resized, detached, hidden or pinned, and let it follow the active layer/composite choice. Mark selected cells and objects, player starts, paths, campaign exits, errors and warnings without making a dense world unreadable. The minimap uses throttled incremental updates while painting and a full refresh after a transaction. It must remain usable without proprietary art through schematic colors and symbols.

Add a World Statistics dock and a reusable report/export model. At minimum, show document revision and exact byte size; occupied and empty cells by layer; unique tile IDs and the most frequent IDs; used, free, inactive and unknown object slots; counts by object type/channel/icon; player starts; moving paths; world and used-content bounds; unreachable or isolated regions where the selected collision model can establish them; diagnostic counts by severity; campaign inbound/outbound links; and unsaved byte ranges. Distinguish exact file facts from profile-based inferences. Clicking a statistic filters or selects the corresponding cells or slots. Compare current values with the opened/saved baseline and optionally export a machine-readable JSON or human-readable CSV report without modifying the world.

### Object and element relocation beyond the original editor

Allow direct relocation of any supported foreground/background element, player start or moving-object slot, including crates, chests, keys, enemies, lifts, hazards and decorative compound placements. The editor must determine whether the selected visual item is a tile ID, one `MoveObject`, multiple overlapping slots, or a verified recipe combining cells and slots. If selection is ambiguous, show the candidates instead of moving the first match. Drag-and-drop, keyboard nudging, cut/paste and exact coordinate entry all route through the same transaction model.

Expose distinct commands whose semantics cannot be confused:

- **Relocate object** moves a static object to a new cell or pixel position and updates the verified start/end/current fields required to keep it static, while preserving all unrelated fields and its slot identity.
- **Move start endpoint** or **move end endpoint** changes only that end of a motion path. **Translate whole path** applies one delta to start, end and current positions when appropriate. A separate command can reset current to start; merely moving or opening the object must not do so.
- **Move element/cells** relocates selected foreground and/or `bigDecor` tiles, optionally with verified attached slots. The source is cleared only after the destination preview is accepted. Raw tile movement never invokes adaptive decoration; an explicit adaptive move can preview retiling at both source and destination.
- **Move compound recipe** preserves the relative offsets and relationships of a verified multi-cell/multi-slot construction. If the editor cannot prove which pieces belong together, it offers ordinary multi-selection instead of guessing.

Before commit, draw a ghost at the destination and report clipping, locked layers, occupied cells, overlapping objects, object-pool requirements, coordinate-unit conversion, changed adaptive borders and any source/destination side effects. Offer cancel, allow verified overlap, replace selected content or choose another destination; never silently delete unrelated content. Multi-selection relocation should support align, distribute, mirror and an exact delta while retaining internal paths. Every relocation is one undoable action and its byte diff identifies all touched cells and slots.

Test static crates/chests and keys, objects with nontrivial paths, pixel-offset objects, overlapping slots, edge clipping, full pools, locked layers, legacy 100-slot worlds, source/destination adaptive borders and compound presets separately for SB1 and SB2. Save/reopen must retain the requested destination and all unrelated bytes. Playtest claims require a verified profile fixture because some apparent decorative elements may have engine-specific placement rules.

### Desktop application shell and commands

Use native Qt desktop conventions with a modern 2026 presentation: a menu bar, customizable toolbars, dockable panels, tabbed documents, recent files/installations/workspaces, command search, keyboard shortcuts, high-DPI icons, light/dark/system themes, persistent layouts and a status bar. Keep critical commands discoverable through both menus and context actions. Avoid imitating the original game's editor chrome; only its data semantics and verified designer terminology are relevant. Restore the last safe window layout, but provide **Reset Layout** and named task layouts such as Mapping, Objects, Campaign and Validation.

Plan the menu bar around these stable groups; platform conventions may relocate **Preferences** or **About**:

| Menu | Planned commands |
| --- | --- |
| **File** | New World, Open World File, Open Game Installation, Open Campaign Workspace, recent items, Close, Save, Save As, Save All, Revert, Import/Export Campaign Pack, Export Image/Statistics, Print, Playtest, document properties and Quit |
| **Edit** | Undo/Redo with operation names, Cut/Copy/Paste, Duplicate, Delete, Select All/None/Inverse, Find, Find Next/Previous, command history and Preferences |
| **View** | Zoom In/Out, Actual Pixels, Fit World/Selection, center on selection, grid/coordinates, layer visibility, minimap, statistics, diagnostics, fullscreen, themes, toolbars, docks, task layouts and Reset Layout |
| **World** | Validate, run World Doctor, compare with saved/original, byte diff, metadata, resize/crop proposals where the fixed 100 × 100 format permits only content moves, player starts, music/region/background settings, SB1 ↔ SB2 conversion, revision conversion export and advanced/raw inspection |
| **Layer** | Select, show/hide, lock/unlock, isolate, clear with preview, fill/replace, transform selection, import/export layer and composite controls |
| **Object** | Create, duplicate, delete, locate by slot/type, edit endpoints/path/runtime fields, reorder where safe, compact only as an explicit compatibility operation and show pool usage |
| **Campaign** | World list and graph, add/remove world, assign role/slot, link/unlink exits, find inbound/outbound references, validate reachability, import/export pack and deploy reviewed changes |
| **Tools** | Installation profiles, asset resolver, palette/catalog inspector, checksums, batch validation/export, recovery manager, settings migration and optional plugin management if a stable extension API is later justified |
| **Help** | Searchable user guide, keyboard shortcut reference, format/compatibility notes, report an issue, diagnostics bundle, license and third-party notices, and **About Eggbert Editor** |

Context menus operate on the item under the pointer without silently discarding the current multi-selection. Potentially broad commands such as clearing a layer, replacing IDs, compacting slots, renumbering campaign worlds or deploying to an installation must preview affected cells, slots, files and references before creating one undoable transaction. Provide repeat-last-operation where its scope is visible, and a non-modal operation history for auditing edits. Autosave recovery, session restore, external-change comparison, multi-document Save All, clipboard previews, palette favorites, reusable stamps, find/replace, selection transforms, layer import/export, batch validation and a read-only byte-diff view are deliberate improvements over the original editor.

### Additional improvements and their priority

These are planned candidates, not prerequisites for the first lossless editor. Promote one only with an explicit use case, UI design, file-compatibility analysis and acceptance test.

| Priority | Improvement | Guardrails and useful outcome |
| --- | --- | --- |
| After basic editing | Visual world comparison | Compare two documents by tiles, objects, metadata and raw byte ranges; synchronize viewports and jump between differences. Never treat runtime-normalized values as equal without explaining the rule. |
| After basic editing | Sidecar bookmarks and author notes | Attach named locations, TODOs, design intent and playtest notes without placing private editor metadata into the game file. Use a documented workspace-relative format and warn when moving a standalone world away from its sidecar. |
| After basic editing | Reusable stamps and patterns | Capture selected layers/objects with an explicit anchor, profile and dependency list. Preview clipping, ID mappings and slot consumption before paste. Ship only original redistributable examples. |
| After validation | Collision and navigation overlays | Show verified solid/hazard/interactive cells and conservative reachability hints. Name the catalog/profile used and never label geometric analysis as proof that a world is solvable or enjoyable. |
| After transactions | Snapshots and visual history | Store content-addressed editor snapshots outside installations, show semantic summaries and restore through normal document transactions. Apply retention limits and expose deletion. This complements, but does not replace, Git or deployment backups. |
| After campaign support | Cross-world refactoring | Rename or renumber a world and preview every manifest and verified in-world reference that changes. Refuse a partial automatic update when an encoding is unknown. |
| After stable core API | Headless command-line companion | Probe, validate, compare, export statistics/images and verify Campaign Packs in CI using the same core. Default to read-only; require explicit output paths and never deploy implicitly. |
| Release quality | Localization and accessibility | Use translatable strings, locale-aware presentation without changing on-disk bytes, scalable UI, screen-reader names, complete keyboard navigation, reduced-motion choices and color-blind-safe overlays. Test at high DPI and with long translations. |
| Release quality | Portable and managed settings | Export/import editor preferences and named layouts separately from world/campaign data. Version the settings schema, redact local installation paths from diagnostic bundles by default and provide a clean-start option. |
| Research later | Three-way semantic merge | Merge independently edited versions only after byte-range transactions and object identity rules are mature. Surface conflicts at field/cell/slot level and preserve all three originals; never guess through slot reuse or campaign renumbering. |
| Research later | Constrained extension API | Consider data-driven catalogs, validators and exporters before native plugins. Version and permission any API, isolate failures, display provenance and keep untrusted extensions from silently writing game installations. |

Also plan small but high-value conveniences: coordinate bookmarks; rulers and distance measurements; palette favorites and recent items; configurable shortcuts; selection statistics before an operation; quick profile-preview switching that cannot alter bytes; copyable diagnostic identifiers; deterministic diagnostic bundles with sensitive paths redacted; and per-document playtest checklists. Do not add analytics, network access, automatic uploads or update checks without a separate privacy design and an opt-in user decision.

## Advanced or “hack” mode

| Tier | Available edits | Save and playtest behavior |
| --- | --- | --- |
| Standard | Profile-verified palette and presets, ordinary coordinates, world settings and supported layer operations | Preserve unusual existing data. Report diagnostics for the chosen game. |
| Advanced | Known hidden items, independent player starts, off-grid endpoint placement, explicit runtime fields and unusual combinations of documented values | Show affected fields, compatibility warnings and a byte-diff preview. Allow an independent exported copy. |
| Experimental | Inspect unknown fields, reserved spans, unmapped IDs and inactive slots. Explicit, size-preserving raw edits on a copy | Show file offset, old and new bytes and affected display fields. Require a distinct output path for experimental export. Keep structural bounds checks active. |

Classify each capability separately as editor-supported, game-tested for a specific profile or unverified. Format validity and suitability for playtesting are separate decisions. The editor can preserve and display an unrecognized ID symbolically. Preview drawing must validate both ends of every range before using an ID as an asset or table index. Disable playtesting when validation finds an unsafe reference. Changing modes alone must never rewrite a document.

### Binary inspector and BLP debugging

Provide a synchronized, read-only-by-default binary inspector for debugging a selected cell, object, header field, unknown range or the complete world payload. Selecting an item in the map, property editor, object list, diagnostic panel or hex view highlights the same semantic value and exact byte range everywhere. The inspector shows absolute file offset, section-relative offset, length, raw hexadecimal bytes, little-endian decoded value, signed and unsigned interpretations where relevant, source revision/profile assumptions, and original/current values. It must explain that a foreground or background cell is a signed 16-bit value stored in `[x][y]` order and show the calculation used for its offset. Object fields similarly show slot base plus field offset.

The whole-file view should be virtualized so a 50,468-byte world and surplus data remain responsive. Support jump to offset, section, coordinate, object slot or diagnostic; hexadecimal and ASCII search; copy as hex; bookmarks; configurable row width; side-by-side original/current comparison; changed-byte highlighting; and export of a bounded diagnostic report. Clearly label header, foreground, optional `bigDecor`, object pool, truncated gaps and opaque trailing bytes. Never invent decoded values for unavailable bytes. A selection spanning fields displays each component instead of silently collapsing endianness or signedness.

Raw editing belongs only to Experimental mode. Keep it disabled for ordinary documents and read-only inspections. When explicitly enabled, require a separate output path, limit changes to existing bytes unless a future verified format operation defines resizing, preview the semantic fields invalidated by the edit, and record one undoable transaction with old/new bytes. Warn when a raw change touches the revision, section boundaries, tile IDs, object activity/type fields or text termination. Re-run the bounded decoder and World Doctor after each committed raw transaction; if the new bytes make structured interpretation unsafe, preserve access to the original and current raw buffers while disabling structured writes.

Test offset synchronization with asymmetric corner and interior cells, both layers, every header field, every field in the first and last available object slots, signed negative IDs, truncated scalars, legacy layouts and opaque suffixes. Verify that navigation, search, copying, report export and opening/closing the inspector do not dirty or mutate the document. Hex edits must round-trip through undo/redo and affect exactly the selected byte range.

## Rendering and assets

Start with a Qt Widgets canvas drawn with `QPainter`. Keep explicit transforms for file coordinates, world pixels and screen pixels. Verify tile size, object anchors, pixel offsets, background scale, transparency, draw order and scrolling against screenshots of asymmetric local worlds. Use nearest-neighbor scaling at integer zoom where available. Cache decoded atlases and repaint only intersecting cells and affected object bounds during drags.

Build separate catalog and asset resolvers for SB1 and SB2. Let users select local game directories and inspect which profile, region and resource each sprite came from. Keep catalog keys separate from atlas coordinates, since an ID can have different art or meaning by target. Map verified channels, variants and animation frames. Provide labeled placeholders for missing resources or unknown values so structural editing and tests can run without proprietary art. Use a generated test atlas for screenshot and clipping tests.

### Full-world image export

Export PNG as the primary image format. At native scale the 100 × 100 world spans 6,400 × 6,400 pixels at 64 pixels per cell. Offer a full canvas or a crop to used bounds, a chosen scale, and a solid or transparent background. Define used bounds from the enabled content, including visible moving-object endpoints when paths are selected. Exclude the backdrop and full-map grid from the crop calculation. Report an empty selection and show the output dimensions before export.

Offer an installation-asset render for a selected game and edition, and a schematic render using redistributable symbols and numeric IDs. Share transforms and draw order with the interactive canvas. Export a composite or individual layers at the same scale and origin. Give the user switches for foreground, `bigDecor`, moving objects, player starts, paths and endpoints, grid, coordinates, campaign links, diagnostics and a legend. Fix animation time for a reproducible image. Check transparency and alignment by recomposing separately exported art layers.

Derive stable filenames from the world ID or source stem, profile, scale and layer. Record crop origin, dimensions, renderer version, profile and chosen settings as reproducible metadata. Plan a headless export entry point that uses the same renderer and settings for campaign batches. An optional SVG can serve schematic maps and vector annotations. Consider tiled rendering or tiled output for large scales and batches. A single native-size RGBA buffer already uses about 156 MiB before temporary surfaces, so estimate peak memory and bound concurrent jobs.

An exact render needs user-supplied installation assets and may reproduce copyrighted artwork. Keep installation-asset renders out of Campaign Packs by default. Permit schematic previews with a redistributable symbol set and verify their provenance. Compare an asymmetric scene rendered at full size and used bounds, including clipped paths, edge cells, layer order, alpha and matching metadata.

## Validation and execution

Emit diagnostics with severity, game profile, source offset and location in the world. Clicking a diagnostic should select its cell, slot or header field. Validate on open, after edits and before export or playtest. Use structural errors for unreadable spans and unsupported layouts, compatibility warnings for target-specific behavior, and informational notices for preserved opaque content.

### World Doctor

World Doctor is the user-facing guided validation and repair workspace. It runs fast incremental checks after an edit and a complete deterministic audit on request, before deployment, Campaign Pack export or playtest. Group findings by world, category and severity; show a stable rule ID, affected cells/slots/fields, exact byte ranges, selected game profile, confidence and the evidence or compatibility rule behind the finding. Clicking a result centers the map and synchronizes the property and binary inspectors. Filters, suppression with an author reason, and JSON/HTML report export must preserve rule IDs so a team or later session can compare results.

Initial rule families should cover file structure and truncation, revision/size disagreement, unsafe or unknown IDs, invalid coordinates and directions, object-pool inconsistencies, incompatible type/channel/icon combinations, starts in blocked or dangerous locations when the collision catalog is verified, moving paths with invalid endpoints, suspicious adaptive borders, duplicate or missing campaign slots, broken door/key/exit references and conservative campaign reachability. Later collision/navigation checks may identify provably isolated regions or missing return routes. Clearly distinguish byte-level facts, profile-derived warnings and heuristic design advice. World Doctor must never claim that static analysis proves a world is solvable, balanced or enjoyable.

A finding may offer zero or more repair proposals. Each proposal states every field, cell, slot and file it would change, why the replacement is valid, whether information would be lost, and which profile/sample supports it. Preview semantic and raw-byte diffs before applying. Apply one repair or a reviewed set as a single named undoable transaction; never repair automatically on open, save, validation, profile switch or deployment. Do not offer a destructive repair when the rule lacks sufficient evidence. “Ignore once”, documented suppression and “open in binary inspector” remain valid outcomes.

Keep the rules and fix builders in the headless core, separate from their Qt presentation. Rules consume bounded decoded views and profile catalogs and return diagnostics plus optional declarative fixes; they must not mutate a document during analysis. Give each rule independent synthetic positive/negative fixtures, boundary and unknown-value cases, stable result ordering and proof that analysis preserves every input byte. Test conflicting fixes, stale findings after edits, partial/truncated documents, bulk preview cancellation, undo/redo and deterministic reports. Fuzzed input may produce diagnostics but must not produce an unchecked repair.

- Check offset arithmetic, available record counts, signed coordinate conversion, strings, recognized empty sentinels, both bounds on tile and sprite lookups, direction values, and profile-specific type/channel/icon combinations. The historical study records collision-table icon indices 0–236 for SB1 and 0–440 for SB2. Scope those limits to the collision table and handle empty sentinels separately. Maintain distinct known object-type catalogs for each game. Diagnose start/end positions using that profile's units. Offer to show the original bytes. Any repair needs its own undoable action.
- Run the chosen executable or compatible implementation only on explicit **Playtest**. Accept a user-selected local executable and optional Wine runner. Configure a profile-specific installation, working directory and world slot. Verify launch arguments, naming rules, asset lookup and save paths on a disposable installation. A game may require fixed slots in its data directory.
- Stage the current buffer in an isolated writable game workspace. Give concurrent runs separate paths. Avoid modifying the user's original levels, saves or installed executable. Launch via an argument list, capture available output, report launch/exit status, and give the user access to the test copy when diagnosing a failure. Keep runtime mutations out of document history.

## Code boundaries

Keep `src/world` free of Qt. It owns raw storage, version descriptors, bounded decoding, changed byte ranges and diagnostic locations. Add profile catalogs as data with provenance and tests. Keep a campaign model for stable IDs, graph validation, manifest migration and pack membership, and a separate installation service for scanning, deployment and recovery. Both consume the same world codec. Keep the renderer, asset loading, Qt undo commands, dialogs and process integration in the application layer. Route canvas and property-panel changes through the same document operation. A headless codec test must be able to compare output to input without opening a window.

## Milestones and acceptance checks

| Stage | Deliverable | Exit condition |
| --- | --- | --- |
| 0. Scaffold | Core library, application, CMake, GPLv3 notice and CTest target | A clean checkout builds and tests. The application reports a selected file's size. |
| 1. Fixtures and reader | Revision probes, header/layer/slot views, offset diagnostics, asset-free inspector | Synthetic files for all four layouts decode independently. Asymmetric tiles and nonzero padding survive unchanged. Incomplete and ambiguous input never causes an unchecked read. |
| 2. Lossless editing | Field transactions, save/save as, undo/redo and diff viewer | A one-tile edit affects its intended bytes only. Undo to the opening state restores exact input. A failed save leaves the original intact. |
| 3. SB1 editing | Palettes, two-layer canvas where available, object tools and metadata | Open an SB1 world, move two starts and one moving object, save and reopen it, and play a copy on the tested SB1 installation. |
| 4. SB2 editing | Sequel catalog, mappings, starts, preview and profile selection | Edit and play an SB2 world. Opening its normalized-ID sample and editing another cell preserves the original stored IDs. |
| 5. Advanced, Doctor and conversion support | Hidden IDs, synchronized binary inspection, World Doctor, SB1 ↔ SB2 conversion, legacy revision export and explicit compatibility operations | Each diagnostic links semantic locations to exact byte ranges and every repair previews one undoable transaction. Read-only hex inspection never dirties a document. Each conversion item is classified as exact, remapped, approximate, dropped or blocked before export. Tests cover SB2-only content such as a saw, object-pool limits, extra slots and missing background. Byte-preserving save passes for both source profiles. |
| 6. Installation workflow | Read-only scan, edition/profile inspection, explicit deployment and recovery | On a disposable installation, cancel a deploy, detect an external edit, interrupt a multiworld deploy and restore a verified snapshot. |
| 7. Campaign Workspace and Pack | Manual world and hub editing, graph validation, manifests, directory/ZIP import and export | Round-trip a manually authored branching campaign. Verify references after renumbering, identical pack bytes on repeated export, rejection of invalid archive members and recovery after an import collision. |
| 8. Distribution | Desktop packages, asset discovery, user guide, recovery and accessibility testing | Clean-machine smoke tests pass with and without local assets. The release names its tested game and OS versions and ships the corresponding source and license material. |

Schedule PNG export after both profiles have verified rendering. Ship the full-world exporter once the 6,400-pixel output, used-bounds origin, transparent composite and per-layer alignment pass the same reference-scene checks.

## Test matrix and remaining risks

- Generate fixtures from documented field values, independently of the production writer. Cover every section boundary, corner cells, x/y asymmetry, signed limits, inactive and full pools, unrelated BLP resources, nonzero reserved bytes, opaque suffixes, ambiguous revisions and truncation. Compare entire byte sequences, then diff only the named ranges for each edit.
- Test repeated saves and edits, transactional drag merging, save-undo-redo, branching history, external file replacement, unwritable destinations and canceling an export. Inject failures before the final replacement. Verify source integrity and clean-state reporting each time.
- Use reproducible generated inputs to exercise bounded parsing and rendering under memory and undefined-behavior sanitizers. Run asset-free CI on Linux, Windows and macOS. Exercise Qt shortcuts, dialog cancellation, the four starts, locked layers, independent endpoints and placeholder rendering with an offscreen platform where practical.
- Maintain an opt-in matrix of locally owned samples by game, edition, locale, revision, observed size and checksum. Record what was tested on the original engine and which behavior remains inferred. Keep distributable synthetic reproductions for regressions. Validate uncommon formats such as the June demo separately before enabling a writer.
- The main uncertainties are legacy encodings, undocumented field meaning, engine-specific visual rules and whether each executable will play an isolated copy. Track a sample or experiment alongside every unresolved assumption. Keep read-only access available wherever writing has insufficient evidence.

## Audit of the archived generator project

The cancelled GPL-3.0-only `eggbert-generator` project survives in `/rv/data/archive/trash/2026/eggbert-generator.bundle`. The historical reference for this plan is commit `069c409611eb779e9066e8d492faed4550dac9a8`. Its quality audit found that technically valid, solvable output could still fail as level design. Keep playability claims tied to observed engine behavior and leave judgments about pacing, difficulty and enjoyment to the human editor.

Audit its research before implementing each related reader or validator feature. Record the named source, observed field or behavior, game/edition, evidence type and confidence. Compare it with the reconstructed reader and original-game samples. Resolve discrepancies explicitly. Independently implement the agreed behavior and author fresh fixtures and assets. Respect the archive's contribution guidance against transplanting source or assets.

| Archive material at the pinned commit | What to verify for this editor |
| --- | --- |
| `docs/formats/BLP_SB1_WORLD.md`, `docs/formats/BLP_SB2_WORLD.md` | Layout, file-kind probes, stock-world observations, demo markers and game-specific field meaning |
| `include/EggbertGenerator/game2d/Sb1World.hpp`, `Sb2World.hpp` and corresponding `src/game2d/blp/` implementations | Byte retention, explicit game profile, bounded decoding and capability checks. Restate observable requirements in editor terminology. |
| `tests/game2d/test_sb1world.cpp`, `test_sb2world.cpp`, `test_reference_sb1.cpp`, `test_reference_sb2.cpp` | Independent round-trip, property and reference-data tests. Recheck corpus coverage and its limits. |
| `docs/LEVEL_DESIGN_QUALITY_AUDIT.md`, `docs/KNOWN_LIMITATIONS.md`, `docs/PROVENANCE.md`, `docs/DECISIONS.md`, `AGENTS.md` | Scope decisions, evidence quality, provenance boundaries and known failure modes |

## Reconstruction references and distribution

For the reconstructed Windows behavior, consult these upstream `blupi_recon` locations. Line numbers refer to the technical study and can move. Record an upstream revision when turning an observation into a fixture.

| Research question | Upstream locations |
| --- | --- |
| Wire fields, revision branches, sizes and SB2 ID remap | `games/sb2/src/decor.h` (Cellule, MoveObject, DescFile), `games/sb2/src/decor.cpp` lines 574–705, `port/tests/test_layout.cpp` lines 15–25 |
| SB1 retail and June-demo differences | `src/decor.h`, `src/decor.cpp` lines 297–420, `src/event.cpp` lines 3391–3453 |
| Build and play preparation, deletion and sorting | `games/sb2/src/decblock.cpp` lines 1338–1487, `games/sb2/src/decmove.cpp` lines 2287–2369, `games/sb2/src/decdesign.cpp` lines 70–97 and from 445 |
| Palette, copy exchange, save/play and input | `games/sb2/src/event.cpp` lines 1478–1495, 2844–3015, 3265–3342, 3834–3900 and 4024–4101 |

Release the editor under GPL-3.0-only. Include the exact license text, corresponding source and reproducible build instructions. Inventory the shipped Qt libraries, plugins, fonts and other dependencies and include their applicable notices. Use original or suitably licensed artwork for editor UI and test data. Resolve retail assets from a user-provided installation. Check redistribution terms separately before including any third-party media, executable, sample world or screenshot in a release. [GNU GPL version 3](https://www.gnu.org/licenses/gpl-3.0.html) and [Qt module licensing](https://doc.qt.io/qt-6/licensing.html)

Verify packaging on a clean system for each advertised platform. Include the required Qt platform plugins and libraries. Exercise startup, asset selection, save/reopen and a disposable playtest before publishing binaries. [Qt deployment guidance](https://doc.qt.io/qt-6/deployment.html)
