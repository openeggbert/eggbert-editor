# Eggbert Editor user guide

Eggbert Editor edits individual world files of *Speedy Blupi* / *Speedy Eggbert* and *Speedy Blupi II* / *Speedy Eggbert 2*: the `.blp` world files and the exported `.xch` files, which have the same layout. It changes only the bytes you edit. Everything else in the file — reserved fields, unused object slots, text after the title, data after the end of the layout — is written back exactly as it was read.

It is an editor for one world at a time. It does not manage campaigns, does not install worlds into a game, does not start the game and does not convert worlds between the two games.

## Opening a world

**File → Open World** (Ctrl+O) opens one or more files, each in its own tab. Files named on the command line open too.

A world file carries no signature. The editor reads the revision field at the start of the file and selects a layout:

| Revision | Layers | Object slots | Size | In this editor |
| --- | --- | ---: | ---: | --- |
| 1.0 | foreground | 100 | 25,668 bytes | view and inspect |
| 1.1 | foreground, bigDecor | 100 | 45,668 bytes | view and inspect |
| 1.2, 1.3 | foreground, bigDecor | 200 | 50,468 bytes | edit and save |

A matching revision and size mean the file is *consistent* with a world; they are not proof. A file that is too short opens with the complete parts visible and is read-only; extra bytes after the layout are kept and never modified; an unknown revision opens for hex inspection only. A yellow banner states why a document is read-only. **File → Document Properties** shows the full structural report.

Not every `.blp` file is a world. In a game's `DATA` folder the worlds are the `world*.blp` files; `demo*.blp` (recordings), `info*.blp`, `time.blp` and `bench.blp` are other data. Because nothing marks a world, the editor cannot tell: such a file opens as a very short, read-only "world" or for hex inspection only, and is never modified. The games' picture files in `IMAGE08` also end in `.blp`; they are bitmaps.

### Choosing the game

The file does not say which game it belongs to. Choose **Speedy Blupi / Eggbert 1** or **Speedy Blupi II / Eggbert 2** in the tool bar. The choice is per tab, is not stored in the file and never changes a byte. It selects the object presets, the adaptive brushes, the ranges offered for music and region, and the rules World Doctor applies. Until you choose, those game-specific features are unavailable; raw tile editing, starts, title and inspection work regardless.

## The canvas

By default the canvas draws the world schematically, without any game artwork: every stored tile number has its own stable colour, bigDecor tiles cover two by two cells, objects are outlined boxes with their type and slot, player starts are numbered discs, and motion paths are dashed lines with an end box. It can also draw with the artwork of a game you have (see *Game artwork*).

- Pan: middle mouse button, Alt+drag, the mouse wheel (Shift for horizontal), or the **Browse** tool.
- Zoom: Ctrl+wheel zooms around the pointer; **View → Zoom In / Zoom Out / Actual Pixels / Fit World / Fit Selection**.
- Keyboard: the arrow keys move a highlighted cell cursor (Shift for ten cells, Ctrl to pan); Space or Enter applies the active tool at the cursor.
- **View** switches the grid, value labels, each layer, objects, starts and paths on and off.

The status bar shows the cell under the pointer with the stored value and file offset of each layer, the object pool usage, the revision and the zoom.

## Game artwork (optional)

The editor contains no game artwork and works fully without it. If you have a game, **View → Game Artwork Directory** lets you choose its directory (the one containing `DATA`, `IMAGE08` and the game's `.exe`). The editor shows what it recognised — a first-game directory has `DATA/demo200.blp`, a second-game directory `DATA/demo300.blp` — and you can correct the game. One directory is remembered per game as a local preference; it is never stored in a world file.

The directory is only read, never changed. The editor reads the sprite sheets (`object.blp`, `element.blp`, `explo.blp`, `blupi000.blp` to `blupi003.blp`, which are ordinary bitmaps) and looks up the position of each sprite inside the game's own executable. A table of positions is used only when all of it fits the sheet it belongs to. **View → Game Artwork Report** lists what was found and what was not.

**View → Draw with Game Artwork** switches between artwork and the schematic drawing. A tab uses artwork only when its chosen game matches a loaded directory. Tiles, bigDecor, objects and starts are drawn in the games' order, with pure blue as the transparent colour; anything the artwork does not provide keeps its schematic placeholder. Animated tiles are drawn with the frame that is stored, and the scrolling backdrop picture is not drawn. The minimap always stays schematic.

Artwork drawing has been checked with installations of Speedy Eggbert 1 and Speedy Eggbert 2: every tile, bigDecor picture, object and start stored in their worlds is found and drawn. Other editions have not been tried. If something is drawn in the wrong place, switch to the schematic drawing, which shows exactly what is stored, and please report it.

## Editing tiles

Tools act on the **active layer** (tool bar, or **Layer → Edit Foreground / Edit bigDecor**) and write stored tile numbers. The value -1 is an empty cell.

| Tool | Key | What it does |
| --- | --- | --- |
| Browse | H | pans; edits nothing |
| Select | S | drag a rectangle of cells |
| Pencil | B | writes the Tile ID into the cells you drag over |
| Eraser | E | writes -1 into the active layer only |
| Eyedropper | I | takes the stored value under the pointer as Tile ID |
| Line | L | drag a straight line |
| Rectangle | R | drag a filled rectangle |
| Fill | F | replaces the connected area of equal values |
| Adaptive | A | paints and recomputes border forms (see below) |

These tools are *raw*: they never touch the other layer or any object. A selection limits every tool to its cells. **Edit** has Cut, Copy, Paste (at the cell cursor), Delete, Select All / None / Invert; **Layer** has Fill Selection, Replace Value, Clear Selection and Flip. A paste that reaches past the map edge is clipped and says how many cells were dropped. Tiles can be pasted within a tab, or between tabs set to the same game; tile numbers mean different things in the two games, so other pastes are refused.

A layer can be locked (**Layer → Lock**) or shown alone (**Isolate Active Layer**). Before an edit that changes more than 64 cells the editor shows how many cells and bytes, and which offsets, will change.

### Adaptive brush

Some tile families have several forms depending on their neighbours: rock, triangular rock, grass and bars in both games; bricks, green slime, palace and pipes in Speedy Blupi II. The games' own level editors pick the form automatically. The Adaptive tool does the same: it writes the family at the cell and then recomputes the forms of that cell and its four direct neighbours with the chosen game's rules, including its random variations.

In Speedy Blupi II, cheese and cave tiles put border pieces into the empty cells next to them, and a lightning emitter fills up to four empty cells below it with its beam; the Adaptive tool does that as well. The brush list also offers *Tile ID, refresh neighbours* and *Erase, refresh neighbours*.

Hovering shows every cell a click would change, including cells outside the brush. One stroke is one undo step and Redo restores exactly the same tiles. Unlike the games' editors, this tool never deletes an object. As in the games, only the direct neighbours are refreshed, and nothing is recomputed when a world is opened or saved.

## Title, music, region and starts

**World → Metadata** edits the title, music, region (background), whether the world scrolls horizontally and vertically, and the saved scroll position. **World → Player Starts** edits the four start positions and directions.

- The title is stored as single bytes (Windows-1252) in a 100-byte field. The games let a player type 40 characters; a longer title is allowed with a note. Bytes after the end of the title are left as they are. The games read the title only from revision 1.3 files.
- Music and region are limited to the values the chosen game offers (music 0–10 with 0 meaning none; region 0–23 in the first game and 0–31 in the second). A stored value outside that range is kept until you change it.
- A world that does not scroll in a direction is one screen in that direction: 10 cells wide or 480 pixels high.
- The **Start** tool (P) moves the start selected in the tool bar to the clicked cell, at the same position inside the cell that the games' editors use; clicking the start's own cell turns it around. **World → Copy Selected Start to All Four** copies one start to the others. Single-player games use start 1.

## Objects

A world stores up to 200 object records (100 in revisions 1.0 and 1.1). A record whose type is 0 is free. The **Objects** panel lists the records in use with slot, type, start, end and current position, channel and icon, and shows how many slots are stored, in use and free.

- **Place** (N) creates the object chosen in the preset list on the clicked cell. For moving objects, press on the start cell and drag to the end cell. Presets exist only for object types that the chosen game's own level editor can place, and a new object gets exactly the values that editor writes. It goes into the lowest free slot; the slot's previous leftover bytes are replaced. When the pool is full nothing is created.
- **Object** (O) selects a record by clicking it; clicking again cycles through overlapping records in slot order. Drag to move it by whole cells; drag the red end box of a path to move only the end. **Object → Move One Cell** (Alt+arrows) nudges, **Duplicate** (Ctrl+D) copies a record one cell to the right, **Delete Object** (Shift+Delete) frees the slot by clearing only its type, as the games do.
- The Objects panel edits exact numbers: travel durations, waits, the three positions, channel and icon. The stored type, the two unknown fields and the run-time fields are shown but not edited. For the two enemies drawn like a player (in a helicopter, in a tank) the channel is the colour: the games' editors offer 11, 12 and 13.
- A record whose type the chosen game's editor cannot place (types created only while playing, or unknown numbers) is shown with its raw values and can be deleted but not changed.

The 200-slot limit is a hard limit of the format. The games also need free slots while playing, for explosions, splashes, projectiles and similar effects; the panel and World Doctor point out a nearly full pool as an *estimate*.

## Undo, save and files changed on disk

Each tab has its own Undo/Redo history (Ctrl+Z, Ctrl+Shift+Z); every stroke, shape, paste, dialog or command is one step. Undo works past the last save.

**Save** (Ctrl+S) writes the file through a temporary file that replaces the original in one step, so a failed save leaves the original intact. Before writing, the editor compares the file on disk with what it last read or wrote. If something else changed, shortened, lengthened or removed it, nothing is written and you choose between reloading the disk version, saving your version under another name, or cancelling. **Save As** writes to another file, **Save All** saves every modified tab, **Revert** reloads from disk.

Read-only documents can be copied with Save As; the copy is byte-identical.

## Minimap and statistics

The **Minimap** shows the whole world and outlines the part visible on the canvas; click or drag in it to move the canvas. It can show both layers or one.

**World Statistics** lists file size and revision, occupied and empty cells and the values used in each layer, object slot usage and counts by stored type, channel and icon, the four starts, the bounds of used cells, and the byte ranges that differ from the saved file and from the file as opened. Double-click a row to find its cells, records or bytes on the canvas. These are stored facts; they are not interpreted for a game.

**Edit → Find** (Ctrl+F) jumps to a cell coordinate, a stored tile value in the active layer, an object type or an object slot.

## World Doctor

**World → Validate** (F8) opens the Diagnostics panel. World Doctor checks structure and known limits; it never changes the world and it does not judge whether a world can be completed or is fun. The list refreshes after every edit, and a summary is shown with every save without blocking it.

Each finding has a rule ID, a severity (error, warning or note), the game it was judged for and the basis of the rule: a structural fact, something read in the game's source, a value outside a verified range, something reported but not re-checked, or an estimate. Double-clicking a finding shows it on the canvas, in the Objects panel and in the Binary Inspector.

Findings include: a truncated file, extra bytes, an unknown revision; music or region outside the game's range; a start outside the world or with an unusual direction; a title without a terminator, longer than 40 bytes or ignored by the game; tile numbers below -1, without a picture or without collision data in the chosen game; Speedy Blupi II's replacement of stored values 48–67 by 215 when it loads a world; border tiles whose form differs from what the game's editor would produce; objects of a type created only while playing or unknown to the game, with an unexpected channel, outside the world, or with a path and a zero duration; a full object pool and a nearly full one.

Only one repair is offered: ending a title that fills its whole field. It is shown with the exact byte it changes and applied as one undo step after you confirm. **World → Compare with Original** and **Compare with Saved** list every changed byte range with old and new bytes.

## Binary Inspector

**View → Binary Inspector** (Ctrl+B) shows the whole file as a hex dump. It cannot change anything. Header, foreground, bigDecor, object records and any extra bytes are tinted differently; bytes changed since the file was opened are highlighted and unsaved bytes underlined.

For the byte under the cursor it shows the offset, the structural name (for example `header.music`, `foreground[3][70]`, `objects[137].posEnd.y`), the arithmetic that leads there, the value as signed and unsigned little-endian number, and the original and saved values. Cells are stored column by column: cell (x, y) of a layer is at `layer start + 2 × (100 × x + y)`.

Type an offset (`1608` or `0x648`), `cell 3,70`, `big 3,70` or `slot 137` to jump; pick a section from the list; search for hex bytes or ASCII text; set bookmarks; choose 8 to 32 bytes per row; view the original bytes or original and current side by side. The inspector follows the cell cursor and the selected object, and clicking a byte moves the canvas there.

## Exporting a picture

**File → Export World PNG** (Ctrl+E) writes one image of the complete world: 6,400 × 6,400 pixels at the native 64 pixels per cell, or smaller. Choose the layers and overlays and a solid or transparent background; the dialog shows the size and the memory needed first. The picture is drawn with the same code as the canvas and records the game profile, the kind of drawing and the settings in its metadata. It is schematic unless you tick **Draw with game artwork** (available when artwork for the tab's game is loaded). Pictures drawn with artwork contain copyrighted material of the game's owners: keep them for your own use unless you have permission to share them.

## Appearance and layout

Panels can be moved, floated, stacked and closed; **View** lists them. **View → Layout** arranges them for mapping, object work or validation, and **Reset Layout** restores the default. **View → Theme** switches between the system, light and dark appearance. Window layout, theme and recent files are remembered.

**Help → Diagnostics Bundle** saves a text report (versions, the structural report, World Doctor findings, changed byte ranges) to attach to a problem report. It contains no world data beyond those descriptions.

## What the editor does not do

It does not edit revisions 1.0 and 1.1, does not edit bytes in the hex view, does not write values it has no evidence for, does not create or renumber campaigns, does not copy worlds into a game installation, does not launch a game, and does not convert between Speedy Blupi 1 and 2 or to the Mobile Eggbert format.
