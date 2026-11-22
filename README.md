# Eggbert Editor

A desktop editor in development for 2D worlds from the Windows games Speedy Blupi and Speedy Blupi II, also released as Speedy Eggbert and Speedy Eggbert 2. Version 1.0 is intentionally scoped to lossless editing of individual world files. Installation deployment, campaigns and loss-aware SB1↔SB2 BLP conversion are post-1.0 roadmap items. A separate post-1.0 Mobile Eggbert text-world profile is planned, but conversion between BLP and Mobile Eggbert worlds is intentionally unsupported in both directions.

What works today:

- **Open** any number of `.blp`/`.xch` files in tabs. A structural probe selects the layout from the revision field (1.0–1.3) and reports truncation or surplus bytes; **File → Document Properties** shows the probe and a decoded summary. The world format has no signature, so this is consistency, never proof, and the game (Speedy Blupi 1 or 2) cannot be read from the file: you choose a profile per tab, which never changes bytes.
- **View** the world on an asset-free schematic canvas: foreground, `bigDecor`, player starts, objects and motion paths, with pan (middle button, Alt+drag or the Browse tool), Ctrl+wheel zoom about the pointer, Zoom In/Out, Actual Pixels, Fit World, grid, value labels and per-layer visibility. Arrow keys move a visible cell cursor; Space applies the active tool.
- **Minimap** dock: the whole world (composite, foreground only or `bigDecor` only) with the canvas viewport outlined; click or drag to pan. **World Statistics** dock: size and revision, occupied/empty cells and value frequencies per layer, object slot usage and counts by stored type/channel/icon, starts, bounds, unsaved and changed byte ranges. Double-clicking a row locates its cells, slots or bytes on the canvas. Both are read-only; **View → Reset Layout** restores the docks.
- **Edit** the tile layers of revision 1.2/1.3 worlds with raw tools that write stored tile IDs into the active layer only: Pencil, Eraser (writes -1; the other layer and objects are untouched), Eyedropper, Line, Rectangle and bounded Fill. Select a rectangle, then Cut/Copy/Paste/Delete, Select All/None/Invert, Fill Selection, Replace Value and Flip; a selection limits every tool to its cells. Paste clips at the map edge and reports it. Tiles paste only within one tab or between tabs with the same chosen game profile. Layers can be locked or isolated. An edit that changes more than 64 cells first shows its cell, byte and offset counts. **Edit → Find** jumps to a coordinate, a stored tile value, an object type or an object slot. Undo/Redo are per document; one stroke, shape, paste or command is one step.
- **Adaptive brush**: paints a tile family verified for the chosen game — rock, triangular rock, grass and bars in both games; bricks, green slime, palace, pipes, cheese, cave and the lightning emitter in Speedy Blupi 2 — or the raw tile ID or an erase, and then recomputes the border forms of the painted cell and its four neighbours exactly as that game's own editor does, including its random looks. Hovering shows every cell a click would change, also those outside the brush. A stroke is one undo step and redo never re-rolls. Objects are never deleted by it. The raw Pencil still writes exactly one value and nothing else.
- **World → Metadata / Player Starts**: title (stored as Windows-1252 bytes, with the text after an old terminator preserved), music, region, horizontal/vertical scrolling, saved scroll position, and the four start positions and directions. Music and region are limited to the range verified for the chosen game profile; an unusual stored value is kept until you change it. The **Start** tool moves the selected start to a cell (at the same in-cell offset the games' editors use) or turns it around.
- **Objects**: an Objects dock lists every active record (slot, type, start, end, current position, channel, icon), the pool usage (stored / in use / free, the 200-record hard limit and an estimated low-headroom note) and edits exact numeric fields of the selected record. The **Place** tool creates objects from presets verified for the chosen game — exactly what that game's own level editor writes, including path end and travel durations for movers — in the lowest free slot, and fails as a whole when the pool is full. The **Object** tool selects (repeated clicks cycle overlapping records), moves by whole cells and reshapes a path's end; the Object menu duplicates, deletes (only the type field is cleared, as in the games) and nudges. Records of a type not verified for the chosen game stay visible with their raw numbers and can be deleted but not edited.
- **Binary Inspector** (View menu, Ctrl+B): a read-only hex view of the whole file that paints only visible rows. Sections are tinted, bytes changed since opening are highlighted and unsaved bytes underlined. The details strip gives the absolute and section-relative offset, the structural name (`header.music`, `foreground[3][70]`, `objects[137].posEnd.y`, `suffix+5`), the offset arithmetic, the little-endian value signed and unsigned, and the original and saved values. Jump to an offset, section, cell or slot; search hex or ASCII; copy as hex; bookmarks; 8–32 bytes per row; original, current or side-by-side. It follows the canvas cursor and the selected object, and clicking a byte moves the canvas. It has no editing path.
- **World Doctor** (World → Validate, F8; Diagnostics dock): a non-mutating audit that reruns after every edit and with every save. Each finding has a stable rule ID, a severity, the game it was judged for, the basis of the rule (structural fact, read in the game's source, outside a verified range, reported, or estimate) and its exact byte range; double-clicking one moves the canvas, the object selection and the Binary Inspector to it. Rules cover truncation, surplus and unknown revisions; music, region, starts and title; tile values with no picture or no collision data in the chosen game and Speedy Blupi 2's load-time rewrite of 48–67; object types, channels, lift/crate icons, positions, zero-duration paths, a full pool and low headroom. It does not judge whether a world can be completed. The single offered repair (terminating a title that fills its field) is shown with its byte diff and applied as one undo step only after confirmation. **World → Compare with Original / Saved** lists every changed byte range.
- **File → Export World PNG**: one image of the complete 100 × 100 world at the native 64 pixels per cell (6,400 × 6,400) or smaller, with the layers, objects, starts, paths, grid and labels you choose and a solid or transparent background. The dialog shows the pixel size and memory estimate first. The export uses the same drawing code and order as the canvas, is written atomically, is byte-identical for identical input and carries the profile, renderer version and settings as PNG text metadata. It currently draws schematically, without game artwork.
- **Workspace**: dockable panels with Mapping, Objects and Validation layouts and Reset Layout; system, light and dark themes; preferences; a Help menu with the user guide, keyboard shortcuts, format notes, license and notices, and a diagnostics bundle (versions, structural report, findings and changed ranges, no world data).
- **Save / Save As / Save All / Revert** with atomic replacement. Save refuses to overwrite a file that changed, shrank, grew or disappeared on disk since it was read and offers Reload or Save As instead. Only edited bytes change: unknown and reserved fields, inactive object slots, name padding and trailing data are preserved exactly.
- Revisions 1.0 and 1.1, truncated files and unknown content open **read-only** with the reason shown.

Not implemented yet: artwork from a game-data directory (the canvas and PNG export are schematic). The format notes, compatibility matrix and implementation milestones are in [plan.md](plan.md). A fresh implementation session should also read [AGENTS.md](AGENTS.md) and continue from [NEXT.md](NEXT.md).

Claude Code Cloud development is supported. Configure Qt and the optional research-data domain using [the cloud setup guide](docs/claude-code-cloud.md); retail data is never required by the normal build or tests.

## Build and run

Install a C++23-capable compiler, CMake 3.21 or newer and the Qt 6 base development package (Widgets and Test), version 6.2 or newer. From the project directory, using a single-configuration generator on Linux, run

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/eggbert-editor
```

If CMake cannot find Qt, set `CMAKE_PREFIX_PATH` to your Qt kit directory when configuring. For a multi-configuration generator such as Visual Studio, build with `--config Release` and pass `-C Release` to CTest. On macOS, launch the generated application bundle. Only Linux is tested and packaged.

`eggbert-editor world.blp ...` opens files given on the command line. `eggbert-editor --version` and `--help` work without a display. `eggbert-editor --smoke-test` runs an end-to-end self check that needs no game data.

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
| `src/world/` | Revision descriptors, bounded little-endian readers, structural probe, read-only decoded views and the lossless document core (transactions, undo, changed ranges). Independent of Qt. |
| `src/app/` | Qt application: file load/save, tabbed main window, document tab, canvas, schematic renderer and reports. |
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

Eggbert Editor is licensed under the GNU General Public License version 3 only, SPDX identifier `GPL-3.0-only`. Qt and the original games have their own licenses. No game assets are required to build or run this scaffold. [License text](COPYING)
