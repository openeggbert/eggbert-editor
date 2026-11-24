# Eggbert Editor

A desktop editor for 2D worlds from the Windows games Speedy Blupi and Speedy Blupi II, also released as Speedy Eggbert and Speedy Eggbert 2. It supports lossless editing of individual worlds, installation-wide editing, Mobile Eggbert text worlds, conversion between the two Windows games, recovery and command-line tools. Mobile Eggbert text worlds open in the same editing workspace with their own format and game settings.

## Screenshots

The mapping workspace shows the scenery palette on the left and the minimap and tile counts on the right. The example is a Speedy Eggbert 2 world.

![Terraced Garden in the desktop editor, with Blupi on a grassy platform, the scenery palette and the world statistics](docs/screenshots/desktop-workspace.png)

In the Mobile Eggbert example, selecting the lift shows both ends of its path on the map and its coordinates and timing in the Objects panel.

![Mobile Eggbert workshop with a selected lift, numbered path endpoints and editable object properties](docs/screenshots/mobile-objects.png)

Both example levels were made for these screenshots. The pictured game artwork is © Daniel Roux and EPSITEC SA, licensed under GPLv3. [Original artwork and license](https://github.com/blupi-games/speedyblupi-data/tree/7f5cbffb925e06951fff65a71b275e8be76ce12b)

## Features

- **Open** any number of `.blp`/`.xch` files in tabs. A structural probe selects the layout from the revision field (1.0–1.3) and reports truncation or surplus bytes; **File → Document Properties** shows the probe and a decoded summary. The world format has no signature, so this is consistency, never proof, and the game (Speedy Blupi 1 or 2) cannot be read from the file: you choose a profile per tab, which never changes bytes.
- **View** the world on a canvas that needs no game assets: foreground, `bigDecor`, player starts, objects and motion paths are drawn schematically, with pan (middle button, Alt+drag or the Browse tool), Ctrl+wheel zoom about the pointer, Zoom In/Out, Actual Pixels, Fit World, Fit Selection, grid, value labels and per-layer visibility. Arrow keys move a visible cell cursor; Space applies the active tool.
- **Palette**: pick tiles, the chosen game's placeable objects and its border brushes by picture in a dock on the left, grouped like the buttons of the game's own level editor; one click selects the tile or object, its layer and the matching tool. Its **World** tab sets the world size (one screen, wide, tall, large), the background picture and the music, and plays the music through a MIDI player installed on the system. Pictures are the game's own sprites when its artwork is loaded and Draw with Game Artwork is on, and numbered schematic swatches otherwise. The object chooser, the Object menu and the object list show the same pictures.
- **Game artwork (optional)**: point the editor at a game directory you own (**View → Game Artwork Directory**) and it draws tiles, bigDecor, objects and starts with that game's sprites over the world's backdrop picture, in the games' draw order. The directory is only read; its path is a local preference. The editor ships no sprite data: it finds each sprite's position in the game's own executable and uses it only when it fits the sprite sheets (the last entry's height may be shortened to fit), falling back to schematic placeholders otherwise. Checked with installations of Speedy Eggbert 1 and Speedy Eggbert 2 (see **Verification status**).
- **Animation**: like the games' own editors, the canvas is alive — on the games' clock of 20 ticks a second objects travel along their paths in both kinds of drawing, and with game artwork animated tiles cycle through their pictures and objects change theirs, by the games' rules. It is display only (no stored byte changes because time passes) and can be switched off with **View → Animate**.
- **Minimap** dock: the whole world (composite, foreground only or `bigDecor` only) with the canvas viewport outlined; click or drag to pan. **World Statistics** dock: size and revision, occupied/empty cells and value frequencies per layer, object slot usage and counts by stored type/channel/icon, starts, bounds, unsaved and changed byte ranges. Double-clicking a row locates its cells, slots or bytes on the canvas. Both are read-only; **View → Reset Layout** restores the docks.
- **Edit** the tile layers of revision 1.2/1.3 worlds with raw tools that write stored tile IDs into the active layer only: Pencil, Eraser (writes -1; the other layer and objects are untouched), Eyedropper, Line, Rectangle and bounded Fill. Select a rectangle, then Cut/Copy/Paste/Delete, Select All/None/Invert, Fill Selection, Replace Value and Flip; a selection limits the cells a tool paints (the Adaptive brush may still re-form neighbours outside it, and Paste is not limited by it). Paste clips at the map edge and reports it. Tiles paste only within one tab or between tabs with the same chosen game profile. Layers can be locked or isolated. An edit that changes more than 64 cells first shows its cell, byte and offset counts (Replace Value in All Open Worlds shows a per-world count instead). **Edit → Find** jumps to a coordinate, a stored tile value, an object type or an object slot. Undo/Redo are per document; one stroke, shape, paste or command is one step.
- **Adaptive brush**: paints a tile family verified for the chosen game — rock, triangular rock, grass and bars in both games; bricks, green slime, palace, pipes, cheese, cave and the lightning emitter in Speedy Blupi 2 — or the raw tile ID or an erase, and then recomputes the border forms of the painted cell and its four neighbours exactly as that game's own editor does, including its random looks. Hovering shows every cell a click would change, also those outside the brush. A stroke is one undo step and redo never re-rolls. Objects are never deleted by it. The raw Pencil still writes exactly one value and nothing else.
- **World → Metadata / Player Starts**: title (stored as Windows-1252 bytes, with the text after an old terminator preserved), music, region, horizontal/vertical scrolling, saved scroll position, and the four start positions and directions. Music and region are limited to the range verified for the chosen game profile; an unusual stored value is kept until you change it. The **Start** tool moves the selected start to a cell (at the same in-cell offset the games' editors use) or turns it around.
- **Objects**: an Objects dock lists every active record (slot, type, start, end, current position, channel, icon), the pool usage (stored / in use / free, the 200-record hard limit and an estimated low-headroom note) and edits exact numeric fields of the selected record. The **Place** tool creates objects from presets verified for the chosen game — exactly what that game's own level editor writes, including path end and travel durations for movers — in the lowest free slot, and fails as a whole when the pool is full. The **Object** tool selects (repeated clicks cycle overlapping records), moves by whole cells and reshapes a path's end; the Object menu duplicates, deletes (only the type field is cleared, as in the games) and nudges. Records of a type not verified for the chosen game stay visible with their raw numbers and can be deleted but not edited.
- **Binary Inspector** (View menu, Ctrl+B): a read-only hex view of the whole file that paints only visible rows. Sections are tinted, bytes changed since opening are highlighted and unsaved bytes underlined. The details strip gives the absolute and section-relative offset, the structural name (`header.music`, `foreground[3][70]`, `objects[137].posEnd.y`, `suffix+5`), the offset arithmetic, the little-endian value signed and unsigned, the original value and, for an unsaved byte, the saved value. Jump to an offset, section, cell or slot; search hex or ASCII; copy as hex; bookmarks; 8–32 bytes per row; original, current or side-by-side. It follows the canvas cursor and the selected object, and clicking a byte moves the canvas. It has no editing path.
- **World Doctor** (World → Validate, F8; Diagnostics dock): a non-mutating audit that reruns after every edit and with every save. Each finding has a stable rule ID, a severity, the game it was judged for, the basis of the rule (structural fact, read in the game's source, outside a verified range, reported, or estimate) and its exact byte range; double-clicking one moves the canvas, the object selection and the Binary Inspector to it. Rules cover truncation, surplus and unknown revisions; music, region, starts and title; tile values with no picture or no collision data in the chosen game and Speedy Blupi 2's load-time rewrite of 48–67; object types, channels, lift/crate icons, positions, zero-duration paths, a full pool and low headroom. It does not judge whether a world can be completed, apart from one labelled estimate: a treasure or goal in a cell that no chain of non-solid cells connects to a start. The single offered repair (terminating a title that fills its field) is shown with its byte diff and applied as one undo step only after confirmation. **World → Compare with Original / Saved** lists every changed byte range.
- **File → Export World PNG**: one image of the complete 100 × 100 world at the native 64 pixels per cell (6,400 × 6,400) or smaller, with the layers, objects, starts, paths, grid and labels you choose a solid or transparent background and, with artwork, the backdrop picture; the export can be limited to the selected cells. The dialog shows the pixel size and memory estimate first. The export uses the same drawing code and order as the canvas, is written atomically, is byte-identical for identical input and carries the profile, renderer version and settings as PNG text metadata. It is schematic unless you choose to draw it with loaded game artwork, in which case the dialog reminds you that the artwork is copyrighted.
- **Getting started**: a welcome page with Open World, New World, Choose Game Directory and recent worlds; files can be dropped onto the window; a world opened from a game's `DATA` folder gets that game's profile, graphics and music automatically (read only), and a game chosen by hand is remembered per file. Tools have project-drawn pictograms, and the status bar has a zoom slider.
- **Workspace**: dockable panels with Mapping, Objects and Validation layouts and Reset Layout; system, light and dark themes; preferences; a Help menu with the user guide, keyboard shortcuts, format notes, license and notices, and a diagnostics bundle (versions, structural report with the decoded header, findings, and the changed byte ranges with their old and new bytes; nothing else of the world).
- **Save / Save As / Save All / Revert** with atomic replacement. Save refuses to overwrite a file that changed, shrank, grew or disappeared on disk since it was read and offers Reload (when the file still exists) or Save As instead. Only edited bytes change: unknown and reserved fields, inactive object slots, name padding and trailing data are preserved exactly.
- **Recovery and snapshots**: unsaved changes are copied to a checksummed store in the user profile a few seconds after an edit and offered at the next start if the editor stopped unexpectedly (restored as one undo step, only onto an unchanged file); named snapshots of a world can be taken and restored the same way. Nothing is written beside a world file.
- Revisions 1.0 and 1.1, truncated files and unknown content open **read-only** with the reason shown.
- **Element Encyclopedia** shows named elements alongside their variants, animation frames and adaptive borders. Browse the complete numbered sprite sheets, runtime objects and backdrops for the active game. Pick an individual terrain tile or the exact appearance of a disguised crate for placement.
- Right-click the map to inspect overlapping elements in draw order. Press **S** and select an area, then drag it to move both tile layers, anchored objects with their paths, and player starts in one undo step.
- **Stamps** (a selection saved for reuse, with its objects; chosen by picture, shown as an outline under the pointer and placed as an overlay in one undo step), **Align Objects in Selection**, and **Replace Value in All Open Worlds** with a per-world preview.
- **Mobile Eggbert worlds** (the text files of the phone game's port) open as ordinary tabs under their own game profile, drawn with the port's own pictures, and every tool works on them: Palette, shapes, objects, stamps, World Doctor, snapshots, merging, deploying into the port's `worlds` folder, PNG and SVG export. Saving writes text and rewrites only the values you changed; a world you did not change is saved byte for byte. They are never converted to or from BLP, and tiles are not pasted between the two kinds of world. A file with an unexpected shape opens in a plain token-editing window instead.
- **Campaign Builder**: the campaign of an installation as a tree of hubs and missions with the free numbers the game still has room for, new worlds created in them, their signs offered for placing, and checks of what a hub needs (a mission ending with the key, doors, barriers).
- **Merge Changes**: a three-way merge of another copy's changes into the current world, by header value, cell and object record, with every conflict decided by you and the result applied as one undo step (also as the `merge` command).
- **Experimental editing** (off by default): typed byte edits as undoable transactions with a second confirmation for preserved ranges, unverified music and region numbers, and border rules of your own for the Adaptive brush. The revision field and the file length never change.
- **Export World SVG** (and the `export-svg` command): a standalone picture with embedded game artwork. A separate schematic export labels tiles, objects, paths and starts; `export-batch` writes the PNG pictures of many worlds at once.
- **Demo recordings** of the games (`demo200.blp`, `demo300.blp` and following) open in a read-only window with a timeline of the keys held, per-key totals and a button for the world they are replayed on; `info` describes them on the command line. They hold keys, not positions, so no path or heat map is drawn.
- **Regression scenarios**: **Record Scenario** writes what is true of a world now (tiles, objects per type, header values, World Doctor findings) as editable expectations, and **Check Against Scenario** reports which no longer hold (also `scenario-record` and `scenario-check`).
- **New World** (and the `new` command) writes the world the games' own editors start a mission with, for either game.
- **Installation** dock: use **File → Open Game Installation** to choose the game or its DATA folder. Search by name or number, filter by role and open all visible worlds in tabs. Save All can back up the originals before writing. **Deploy** puts the current world into a slot with a verified backup of what it replaces, **Roll Back** restores it byte for byte, **Playtest** starts a command you configure.
- **Campaign Overview** orders an installation's worlds by the games' numbering, says what is missing and follows the door tiles between the official worlds; **Export / Import Campaign Pack** (ZIP, also ones compressed by other programs, or a folder) exchange worlds with a checksummed manifest as a ZIP file or folder. The game's own files stay out of a pack unless explicitly chosen, hostile archives are refused, and an import places every world or none.
- **Convert for Other Game** (and the `convert-game` command) writes a copy of a world for the other game and lists every stored value that game does not know or shows differently; unknown values are kept or removed by your choice, never silently.
- **Convert Revision** (and the `convert` command) writes a copy of a world in revision 1.0, 1.1, 1.2 or 1.3 to a new file; anything the target cannot store is listed and dropped only on request.

Download [Eggbert Editor 0.2.0](https://github.com/openeggbert/eggbert-editor/releases/tag/v0.2.0) for Windows, Linux or macOS.

**Verification status.** The automated tests use only independent synthetic worlds and generated sprite sheets. In addition, on 2026-11-22 the editor was checked against original installations of Speedy Eggbert 1 and Speedy Eggbert 2 (kept outside the repository):

- All 155 stock worlds (59 of the first game, revision 1.2; 96 of the second, revision 1.3) are byte-identical after an untouched Save As, differ in exactly two bytes after a one-cell edit, save and reopen, and are back to the original bytes after undo and save. World Doctor reports no error and no warning for any of them under its game's profile.
- All 9,550 stored objects are of a type the object catalog offers for that game. 9,409 of them store exactly the durations, waits, path, in-cell offset and channel a catalog preset writes; 116 (two enemy types in the second game) differ only in the channel, which the games' editors let the designer cycle; 25 eggs sit four pixels higher in their cell than the released editors place them, which the evidence at hand does not explain. Every stored music, region, tile and bigDecor number lies inside the profile ranges; more than 99.6 % of border tiles have exactly the form the adaptive brushes compute.
- Game artwork: all four sprite tables are located in each executable, and every tile, bigDecor picture, object and start stored in the stock worlds is drawn from the sheets. Rendered scenes were inspected by eye and against the reconstructed draw code; they were not compared pixel by pixel with a running game.

- Later features were checked against the same installations (2026-11-23): all 155 worlds convert for the other game and are byte-identical after converting back with unknown values kept; by the games' door rule every door of the official worlds leads to a world that exists and every hub and mission has a door leading to it; the reachability estimate reports nothing on the 131 worlds it applies to. The 78 worlds of the Mobile Eggbert project are read and written back byte for byte by the text-world reader.

Not checked: other editions (Speedy Blupi I/II, demo and localized releases), worlds of revision 1.0/1.1 from an original source (the stock worlds contain none; those layouts are covered by synthetic files only), and behaviour inside the running games. Details and numbers are in `docs/research/evidence.md`.

The format notes, compatibility matrix and implementation milestones are in [plan.md](plan.md). A fresh implementation session should also read [AGENTS.md](AGENTS.md) and continue from [NEXT.md](NEXT.md).

Claude Code Cloud development is supported. Configure Qt and the optional research-data domain using [the cloud setup guide](docs/claude-code-cloud.md); retail data is never required by the normal build or tests.

## Build and run

Install a C++23-capable compiler, CMake 3.21 or newer and the Qt 6 base development package (Widgets and Test), version 6.2 or newer. From the project directory, using a single-configuration generator on Linux, run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/eggbert-editor
```

If CMake cannot find Qt, set `CMAKE_PREFIX_PATH` to your Qt kit directory when configuring. For a multi-configuration generator such as Visual Studio, build with `--config Release` and pass `-C Release` to CTest. On macOS, launch the generated application bundle. Linux is where the editor is developed and where the packages have been installed and tried by hand. Windows (MSVC, Qt 6.5) and macOS (Apple clang, Qt 6.10) are built, tested and packaged by the CI workflow on hosted runners — a ZIP and a disk image that carry Qt along (`cpack`); nobody has yet run those two packages by hand. On macOS use Qt 6.9.2 or newer: older releases link a framework current macOS SDKs no longer have. `CMakePresets.json` (CMake 3.25 or newer) has the presets `linux`, `linux-sanitizers`, `windows-msvc` and `macos`.

The interface is available in English and Czech (**Edit → Preferences → Language**); the Czech catalogue in `translations/` covers the interface's texts, the catalog names and the sentences the checking and converting code composes (World Doctor, conversions, campaign findings, the encyclopedia, text-world problems, packs and deploys), and the user guide exists in Czech (`docs/user-guide.cs.md`, shown by Help in a Czech interface). The Binary Inspector's structural names, citations, the diagnostics file and the command line stay English. Catalogues are read at run time, so building needs no Qt Linguist tools.

`eggbert-editor world.blp ...` opens files given on the command line. Twelve commands work without a window and never change a world they read: `info FILE`, `validate FILE [--game 1|2]`, `diff FILE OTHER`, `export-png FILE OUT.png [--scale N] [--crop x,y,w,h] ...`, `export-batch OUTDIR FILE...`, `export-svg FILE OUT.svg`, `scenario-record FILE OUT.txt`, `scenario-check FILE SCENARIO.txt`, `convert FILE OUT --to 1.0|1.1|1.2|1.3 [--allow-loss]`, `convert-game FILE OUT --from 1|2 --to 2|1 [--unknown refuse|keep|remove]`, `merge BASE MINE THEIRS OUT [--prefer mine|theirs]` (a three-way merge by header value, cell and object record) and `new OUT --game 1|2` (exit codes 0 success, 1 findings, differences, a refused loss, merge conflicts, pictures that could not be written or failed expectations, 2 usage or file error; see `--help`). `eggbert-editor --version` and `--help` work without a display. `eggbert-editor --smoke-test` runs an end-to-end self check that needs no game data; `--smoke-test --game-data DIR` additionally recognises the game in `DIR` (read only) and draws with its artwork.

The tests need no game installation, no game assets and no network. Add `-DEGGBERT_SANITIZE=ON` to build with the address and undefined-behavior sanitizers.

### Install and package (Linux)

```sh
cmake --install build --prefix /usr/local     # program, desktop entry, icon, documents
cd build && cpack -G "TGZ;DEB"                # eggbert-editor-<version>-Linux.tar.gz and .deb
```

The packages depend on the distribution's Qt 6 libraries and contain no Qt binaries, fonts or game data. `NOTICE.md` lists third-party components.

### Checking real world files

The ordinary tests use only synthetic data. To check world files you own, point the opt-in test at directories outside the repository; it copies each file to a temporary directory and never writes to yours:

```sh
EGGBERT_SAMPLE_DIRS="sb1=/path/to/game1/DATA:sb2=/path/to/game2/DATA" \
    ctest --test-dir build -R sample_matrix --output-on-failure
```

For every file it reports size, checksum prefix, revision and whether an untouched copy is byte-identical and a one-cell edit changes exactly two bytes after save and reopen. Without the variable the test is skipped. Never commit such files.

## Project layout

| Location | Contents |
| --- | --- |
| `src/world/` | Revision descriptors, bounded little-endian readers, structural probe, read-only decoded views and the lossless document core (transactions, undo, changed ranges); catalogs, World Doctor, conversions, merge, stamps, campaign and installation rules, the Mobile Eggbert text-world reader. Independent of Qt. |
| `src/app/` | Qt application: file load/save, tabbed main window, document tab, canvas, renderer, palette, game assets, music player, panels and reports; recovery, deploy and pack stores, the command line, the run-time translator. |
| `translations/` | Interface catalogues (Czech), read at run time; see its README. |
| `CMakePresets.json` | Build presets for Linux, Windows and macOS. |
| `tests/` | Asset-free tests: headless core tests, offscreen UI tests, smoke and install tests, and the opt-in local-sample check. |
| `docs/user-guide.md`, `docs/format-notes.md` | User documentation, also embedded in the Help menu. |
| `resources/` | Desktop entry and the project's own icon. |
| `NOTICE.md` | Third-party notices. |
| `plan.md` | Fixed 1.0 scope, binary layout, feature design, release criteria and compact post-1.0 roadmap. |
| `NEXT.md` | Current implementation state and the next bounded, testable slice. |
| `docs/research/evidence.md` | Observations from the reconstructed games that behavior relies on, with pinned revisions and how each was obtained. |
| `AGENTS.md` | Engineering invariants and fresh-context handoff rules. |
| `CLAUDE.md` | Claude Code entry point that enforces the same scope and reading order. |
| `docs/claude-code-cloud.md` | Reproducible hosted environment, build and optional research-data procedure. |

## License

Eggbert Editor is licensed under the GNU General Public License version 3 only, SPDX identifier `GPL-3.0-only`. Qt and the original games have their own licenses. No game assets are required to build or run the editor. [License text](COPYING)
