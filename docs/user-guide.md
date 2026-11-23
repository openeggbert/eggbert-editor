# Eggbert Editor user guide

Eggbert Editor edits individual world files of *Speedy Blupi* / *Speedy Eggbert* and *Speedy Blupi II* / *Speedy Eggbert 2*: the `.blp` world files and the exported `.xch` files, which have the same layout. It changes only the bytes you edit. Everything else in the file — reserved fields, unused object slots, text after the title, data after the end of the layout — is written back exactly as it was read.

It is an editor for one world at a time. It can list the worlds of a game installation, put a world into it and take it out again, and start the game with a command you give; it can review a campaign's numbering and exchange worlds as packs.

## Opening a world

**File → Open World** (Ctrl+O) opens one or more files, each in its own tab. Files named on the command line open too, and so do files dropped onto the window. While nothing is open, the window shows a welcome page with **Open World**, **New World**, **Choose Game Directory** and the worlds you opened recently.

**File → New World** (Ctrl+N) asks for the game and for a file, and writes the world the game's own editor starts a new mission with: a full-size empty map with a three-tile platform, the four starts, one treasure and the goal, music 1 and backdrop 2. The file is written in the revision that game's stock worlds use (1.2 for the first game, 1.3 for the second) and opened with the game selected. A file that is open in the editor is not replaced.

A world file carries no signature. The editor reads the revision field at the start of the file and selects a layout:

| Revision | Layers | Object slots | Size | In this editor |
| --- | --- | ---: | ---: | --- |
| 1.0 | foreground | 100 | 25,668 bytes | view and inspect |
| 1.1 | foreground, bigDecor | 100 | 45,668 bytes | view and inspect |
| 1.2, 1.3 | foreground, bigDecor | 200 | 50,468 bytes | edit and save |

A matching revision and size mean the file is *consistent* with a world; they are not proof. A file that is too short opens with the complete parts visible and is read-only; extra bytes after the layout are kept and never modified; an unknown revision opens for hex inspection only. A yellow banner states why a document is read-only. **File → Document Properties** shows the full structural report.

Not every `.blp` file is a world. In a game's `DATA` folder the worlds are the `world*.blp` files; `demo*.blp` (recordings), `info*.blp`, `time.blp` and `bench.blp` are other data. A recording is recognised by its content and opens in a window of its own (see **Demo recordings**). For the others nothing marks what they are, so the editor cannot tell: such a file opens as a very short, read-only "world" or for hex inspection only, and is never modified. The games' picture files in `IMAGE08` also end in `.blp`; they are bitmaps.

### Choosing the game

The file does not say which game it belongs to. When you open a world that lies in a game's `DATA` folder, the editor recognises the game from that folder (the same way as for game artwork), selects it, and uses that game directory for its graphics and music unless you already chose one for that game; the folder is only read. For any other file, choose **Speedy Blupi / Eggbert 1** or **Speedy Blupi II / Eggbert 2** in the tool bar. A choice you make there is remembered for that file on this computer and takes precedence over the folder next time. The choice is per tab, is not stored in the file and never changes a byte. It selects the object presets, the adaptive brushes, the ranges offered for music and region, and the rules World Doctor applies. Until you choose, those game-specific features are unavailable; raw tile editing, starts, title and inspection work regardless.

## The canvas

By default the canvas draws the world schematically, without any game artwork: every stored tile number has its own stable colour (numbers below -1 share one warning colour), bigDecor tiles cover two by two cells, objects are outlined boxes with their type and slot, player starts are numbered discs, and motion paths are dashed lines with an end box. While motion paths are shown, the two ends of a path are marked **1** (where the object starts) and **2** (where it turns back): schematically by numbered badges, down to about five screen pixels per cell, and with game artwork by the games' own two pictures — the yellow frames numbered 1 and 2 that the games' editors show for moving objects — drawn at the stored positions, over the object. It can also draw with the artwork of a game you have (see *Game artwork*).

The canvas is **animated**, as the games' own level editors are: the games' clock runs at 20 ticks a second. With game artwork, lava, water, wind, fans, springs and the other animated tiles cycle through their pictures (most change every second to fourth tick) and objects change their picture as they do in the games; in both kinds of drawing objects travel along their paths with their stored durations and waits. This is for looking only. Time passing never changes the world: the file stores each object's start, end and timing, and those are what you edit. As in the games' editors, every object starts from its start point when a world is opened, and an object you have just edited starts again from there. While animation is on, a moving object can be picked with the Object tool where it currently is, at its start cell, or at its stored position (the centre of the clicked cell must lie on the object); with animation off, at its stored position. **View → Animate** switches animation off and on; switched off, the canvas shows exactly the stored state. Exported pictures always show the stored state.

- Pan: middle mouse button, Alt+drag, the mouse wheel (Shift for horizontal), or the **Browse** tool.
- Zoom: Ctrl+wheel zooms around the pointer; the slider at the right end of the status bar covers the whole range; **View → Zoom In / Zoom Out / Actual Pixels / Fit World / Fit Selection**.
- Keyboard: the arrow keys move a highlighted cell cursor (Shift for ten cells, Ctrl to pan); Space or Enter applies the active tool at the cursor.
- **View** switches the grid, value labels, each layer, objects, starts and paths on and off.

The status bar shows the cell under the pointer with the stored value and file offset of each layer, the object pool usage, the revision and the zoom.

## Game artwork (optional)

The editor contains no game artwork and works fully without it. If you have a game, **View → Game Artwork Directory** lets you choose its directory (the one containing `DATA`, `IMAGE08` and the game's `.exe`). The editor shows what it recognised — a first-game directory has `DATA/demo200.blp`, a second-game directory `DATA/demo300.blp` — and you can correct the game. One directory is remembered per game as a local preference; it is never stored in a world file.

The directory is only read, never changed. The editor reads the sprite sheets (`object.blp`, `element.blp`, `explo.blp`, `blupi000.blp` to `blupi003.blp`, which are ordinary bitmaps) and looks up the position of each sprite inside the game's own executable. A table of positions is used only when it fits the sheet it belongs to (the height of a table's last entry may be shortened to fit). **View → Game Artwork Report** lists what was found and what was not.

**View → Draw with Game Artwork** switches between artwork and the schematic drawing. A tab uses artwork only when its chosen game matches a loaded directory. The backdrop picture of the world's background, bigDecor, objects, tiles and starts are drawn in the games' order (ordinary tiles, lifts, then lava, water, wind, fans and traps on top; in the second game two tile numbers go behind the objects), with pure blue as the transparent colour; anything the artwork does not provide keeps its schematic placeholder. The backdrop is tiled and, on the canvas, scrolls at two thirds of the map's speed as in the games; **View → Show Backdrop Picture** switches it off on the canvas. The minimap always stays schematic.

Artwork drawing has been checked with installations of Speedy Eggbert 1 and Speedy Eggbert 2: every tile, bigDecor picture, object and start stored in their worlds is found and drawn. Other editions have not been tried. If something is drawn in the wrong place, switch to the schematic drawing, which shows exactly what is stored, and please report it.

## The Palette: choosing by picture

The **Palette** panel on the left (**View → Palette**) is where you pick what the tools paint and place, by picture rather than by number. It has four tabs.

**Tiles.** The strip at the top lists groups; the grid below shows the tiles of the chosen group, starting with the empty cell. Once a game is chosen, there is one group for each button of that game's own level editor that offers tiles — in the first game for example *Scenery* (trees, houses and other big pictures), *Rock and tech blocks*, *Buildings and decor* — with their kinds in the same order, followed by *Other tiles* (tiles in none of the groups). *All tiles* and *All bigDecor* are always there. Hover over a tile to see its kind and number. Click a tile to paint with it: the layer it belongs to becomes the active layer, and the Pencil is selected unless a tile-painting tool is already active. Tiles that the games' editors fit to their neighbours (rock, grass, cheese and so on) are painted with the adaptive brush, which does the same, when they are picked from one of the editor's groups; in *Other tiles* and *All tiles* every number is a raw tile.

**Objects.** The same way for everything the chosen game's editor can place: *All objects*, then one group for each editor button that offers objects, such as *Lifts, cases and enemies* or *Bonuses and goal* in the first game. Click an object, then click a cell on the map to place it (drag for objects that move along a path).

**Borders.** The chosen game's adaptive border brushes. Click one to paint with it.

**World.** Settings of the whole world; each choice is one undo step.

- *World size*: one screen, wide (scrolls horizontally), tall (scrolls vertically) or large (scrolls both ways).
- *Background*: the backdrop picture the game shows behind the world. Click one to use it.
- *Music*: no music or one of the game's pieces. With **Play when chosen** ticked, choosing a piece also plays it; **Play**/**Stop** does so for the marked piece. The games' music is MIDI, and the editor has no synthesizer of its own: it uses the command stored under `music/command` in the editor's settings file if there is one (`%f` stands for the file), otherwise the first of FluidSynth (with a sound font in a system sound-font directory), TiMidity++ and WildMIDI that is installed. The player is looked for when the editor starts. Without one the panel says so. Listening needs the game directory to be chosen (View → Game Artwork Directory), because the music files are the game's; its pictures need not be usable.

With game artwork loaded and **View → Draw with Game Artwork** switched on, the pictures are the game's own graphics, and in *Other tiles*, *All tiles* and *All bigDecor* numbers without a visible picture are left out; otherwise each entry is a coloured swatch with its number, in the same colours as the schematic canvas, and, once a game is chosen, the panel offers a **Choose Game Directory** button. The same pictures appear in the object chooser of the tool bar, in **Object → Create Verified Type** and in front of every record in the Objects panel. Choosing a tile, object or brush never changes the world; only the tools and the World tab do.

The groups are a way to find things, not a limit: the **Tile ID** box in the tool bar remains for typing any raw number. Player starts are placed with the Start tool.

### Element Encyclopedia

**Help → Element Encyclopedia** lists everything the editor's catalogs know about the current world's game: every tile entry and object of the game editor's groups and every adaptive border family. Type in the search box to filter by name or group; a number finds the entries that store exactly that tile value or object type. For the chosen entry the window shows what is stored for it (tile numbers, or the object's type, channel, icon, placement and timing) and the evidence it rests on. **Select in Editor** (or a double click) chooses the entry as a click in the Palette does: a plain tile for the Pencil, a tile the game reshapes to fit its neighbours as its Adaptive brush, an object for Place, a border family for the Adaptive brush. Pictures are the ones the Palette shows: the game's own while **View → Draw with Game Artwork** is on and a game directory is chosen, numbered placeholders otherwise. The window stays open while you work and follows the current tab's game.

## Editing tiles

Tools act on the **active layer** (tool bar, or **Layer → Edit Foreground / Edit bigDecor**) and write stored tile numbers; the Adaptive tool always works on the foreground. The value -1 is an empty cell. Pick the tile in the **Palette** or type its number.

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

Except for Adaptive these tools are *raw*: they never touch the other layer or any object. A selection limits the cells a tool paints (Adaptive may still re-form neighbours outside it, and Paste is not limited by it). **Edit** has Cut, Copy, Paste (at the cell cursor), Delete, Select All / None / Invert; **Layer** has Fill Selection, Replace Value, Clear Selection and Flip. A paste that reaches past the map edge is clipped and says how many cells were dropped. Tiles can be pasted within a tab, or between tabs set to the same game; tile numbers mean different things in the two games, so other pastes are refused.

A layer can be locked (**Layer → Lock**) or shown alone (**Isolate Active Layer**). Before an edit that changes more than 64 cells the editor shows how many cells and bytes, and which offsets, will change.

### Stamps

A stamp is a piece of a world you keep for reuse. Select the cells, then **Edit → Save Selection as Stamp** stores the bounding box of the selection under a name: both tile layers and, if you leave the box ticked, the objects that start in it (with their paths). Saving changes nothing in the world. **Edit → Place Stamp** shows the stored stamps with a small schematic picture each (tiles in the editor's colours, a dot per object; never game artwork). The chosen stamp is then *armed*: an outline of the cells it would touch is shown on the map and follows the pointer or the keyboard cursor, whichever moved last, and nothing is written until you click, or press Enter or Space at the cursor cell, where its top-left corner should go. That places it as one undo step. Esc (with the map focused), or choosing any tool, gives the stamp up; a placement that is refused leaves it armed so you can try another spot. Empty cells of a stamp are holes: they leave what is there. Tiles that would fall outside the map are clipped and counted. The objects of a stamp are placed all or not at all — if the pool has too few free slots, or an object would lie outside the world, nothing is placed and the reason is given. A stamp made under one game is not placed into a world of the other. A stamp's objects are copies of the stored records, whatever their type, including types the editor has no preset for. Stamps are small text files in your user profile (`stamps` in the editor's data folder), one per stamp; a file that cannot be read as a stamp is not offered.

### Replacing a value in all open worlds

**Layer → Replace Value in All Open Worlds** asks for the value and its replacement like **Replace Value**, then shows, for every open world, how many cells of the current layer hold the value — or why the world is skipped (another game, read-only, layer locked). This per-world list takes the place of the question the editor otherwise asks before an edit of more than 64 cells. After you confirm, each affected world gets one undo step of its own; nothing is saved. The whole layer is searched, whatever is selected.

### Adaptive brush

Some tile families have several forms depending on their neighbours: rock, triangular rock, grass and bars in both games; bricks, green slime, palace and pipes in Speedy Blupi II. The games' own level editors pick the form automatically. The Adaptive tool does the same: it writes the family at the cell and then recomputes the forms of that cell and its four direct neighbours with the chosen game's rules, including its random variations.

In Speedy Blupi II, cheese and cave tiles put border pieces into the empty cells next to them, and a lightning emitter fills up to four empty cells below it with its beam; the Adaptive tool does that as well. The brush list also offers *Tile ID, refresh neighbours* and *Erase, refresh neighbours*.

Hovering shows every cell a click would change, including cells outside the brush. One stroke is one undo step and Redo restores exactly the same tiles. Unlike the games' editors, this tool never deletes an object. As in the games, only the direct neighbours are refreshed, and nothing is recomputed when a world is opened or saved.

## Title, music, region and starts

**World → Metadata** edits the title, music, region (background), whether the world scrolls horizontally and vertically, and the saved scroll position. **World → Player Starts** edits the four start positions and directions.

- The title is stored as single bytes (Windows-1252) in a 100-byte field. The games let a player type 40 characters; a longer title is allowed with a note. Bytes after the end of the title are left as they are. The games read the title only from revision 1.3 files.
- Music and region are limited to the values the chosen game offers (music 0–10 with 0 meaning none; region 0–23 in the first game and 0–31 in the second). A stored value outside that range is kept until you change it.
- A world that does not scroll in a direction is one screen in that direction: 10 cells wide or 480 pixels high.
- The **Start** tool (P) moves the start selected in the tool bar to the clicked cell, at the same position inside the cell that the games' editors use; clicking the cell where the start already sits at that position turns it around. **World → Copy Selected Start to All Four** copies one start to the others. Single-player games use start 1.

## Objects

A world stores up to 200 object records (100 in revisions 1.0 and 1.1). A record whose type is 0 is free. The **Objects** panel lists the records in use with slot, type, start, end and current position, channel and icon, and shows how many slots are stored, in use and free.

- **Place** (N) creates the object chosen in the Palette (or in the tool bar's list) on the clicked cell. For moving objects, press on the start cell and drag to the end cell. Presets exist only for object types that the chosen game's own level editor can place, and a new object gets exactly the values that editor writes. It goes into the lowest free slot; the slot's previous leftover bytes are replaced. When the pool is full nothing is created.
- **Object** (O) selects a record by clicking it; clicking again cycles through overlapping records in slot order. Drag to move it by whole cells; drag the red end box of a path to move only the end. Moving keeps the position inside the cell exactly as stored (a few original worlds store eggs four pixels higher than the games' editors place them; they stay that way unless you change the numbers). **Object → Move One Cell** (Alt+arrows) nudges, **Duplicate** (Ctrl+D) copies a record one cell to the right, **Delete Object** (Shift+Delete) frees the slot by clearing only its type, as the games do.
- The Objects panel edits exact numbers: travel durations, waits, the three positions, channel and icon. The stored type, the two unknown fields and the run-time fields are shown but not edited. For the two enemies drawn like a player (in a helicopter, in a tank) the channel is the colour: the games' editors offer 11, 12 and 13.
- A record whose type the chosen game's editor cannot place (types created only while playing, or unknown numbers) is shown with its raw values and can be deleted but not changed.

The 200-slot limit is a hard limit of the format. The games also need free slots while playing, for explosions, splashes, projectiles and similar effects; the panel and World Doctor point out a nearly full pool as an *estimate*.

### Aligning objects

**Object → Align Objects in Selection** moves the objects whose start lies in the selected cells as a group: to the leftmost, rightmost, topmost or bottommost start among them, or spread evenly between the outermost two (at least three objects). An object's start, end and current position move by the same amount, so its path keeps its shape. As everywhere else, only objects of a type verified for the chosen game are moved; others in the selection stay as stored and are counted in the status bar. It is one undo step.

## Undo, save and files changed on disk

Each tab has its own Undo/Redo history (Ctrl+Z, Ctrl+Shift+Z); every stroke, shape, paste, dialog or command is one step. Undo works past the last save.

**Save** (Ctrl+S) writes the file through a temporary file that replaces the original in one step, so a failed save leaves the original intact. Before writing, the editor compares the file on disk with what it last read or wrote. If something else changed, shortened, lengthened or removed it, nothing is written and you choose between reloading the disk version (if the file still exists), saving your version under another name, or cancelling. **Save As** writes to another file, **Save All** saves every modified tab, **Revert** reloads from disk.

Read-only documents can be copied with Save As; the copy is byte-identical.

### Converting to another revision

**File → Convert Revision** writes a copy of the current world (with its unsaved changes) in revision 1.0, 1.1, 1.2 or 1.3 to a new file and opens it. The file you started from is not changed, and the copy can never be written over it or over any file that is open in the editor. This is also how a revision 1.0 or 1.1 world is edited: convert it to 1.2 or 1.3, edit the copy, and convert back if the old revision is needed.

The header, the tiles and the object records are copied byte for byte; only the revision number changes. Going up, what the older revision does not store is added the way the games fill it when they load such a world: an empty back layer for 1.0, a hundred free object slots for 1.0 and 1.1. Going down, the dialog lists what the target cannot store — back-layer tiles in 1.0, objects or leftover bytes in slots 100 to 199 in 1.0 and 1.1 — and converts only after you tick **Convert anyway and lose what the target cannot store**. A back layer that is empty and slots that are completely zero are simply left out, so an upgrade can always be undone exactly. Bytes after the end of the world are carried over. Between 1.2 and 1.3 only the number differs; the games read the title only from revision 1.3. Truncated files and unknown revisions cannot be converted.

### Converting for the other game

The two games read the same file layout and share almost every stored number, so a world of one game mostly works in the other. **File → Convert for Other Game** (available once the tab's game is chosen) writes a copy for the other game to a new file, in the revision that game's stock worlds use, and opens it with that game selected. The file you started from is not changed.

Before anything is written a list shows every stored value the other game does not know or treats differently, with how often and where it first occurs (the evidence is in each line's tooltip):

- **unknown** — the other game has no picture or code for it. Converting for the first game: second-game tiles 246 to 440, back-layer pictures 54 to 99, the object types only the second game has (the coloured keys, wasp, creature, conveyor lifts, hovercraft, dynamite, homing bomb and others) and backdrops 24 to 31. Converting for the second game: only object type 58, an effect the first game creates while playing and that a world file does not normally store.
- **differs** — both games have the number but show it differently: tile numbers 48 to 67 (blocks in the first game; the second game shows tile 215 for them), in either direction; a lift or crate drawn with a tile picture the other game lacks; and the seven backdrop numbers that are different pictures in the two games.

When something is unknown you choose between keeping those values stored (the other game ignores them, and converting back restores the world exactly) and removing them (those cells are emptied, those object slots freed, the backdrop set to 2). Values that merely differ are never changed. Nothing else is touched: apart from the revision number the copy is byte-identical to the source.

### Merging another copy's changes

When two copies of a world were edited separately, **File → Merge Changes** brings the other copy's changes into the current tab. You choose the common starting point both copies were made from and the other copy; the current tab, with its unsaved changes, is your side. The three must be complete worlds of the same revision and length.

The merge works element by element — one header value, one cell, one whole object record: what only the other copy changed is taken, what only you changed stays, and what both changed alike is simply kept. Elements both changed differently are listed with their name, offset and both versions; tick the ones to take from the other copy (two buttons tick or clear them all). Nothing is decided for you. The result is applied as one undo step and nothing is saved. Bytes the editor preserves (reserved header fields and bytes after the world) are never taken from another file: they keep this world's values, and the status bar says how many differed. The same merge without a window is the `merge` command.

### If the editor stops unexpectedly

A few seconds after an edit, the unsaved state of every modified world is copied to a recovery folder in your user profile (never next to the world file). The copy is removed when you save, revert or close the world. If the editor or the computer stops before that, the next start asks what to do with each copy: **Restore** opens the world and puts the changes back as one undo step, still unsaved; **Save a Copy As** writes them to a file of your choice; **Discard** deletes them; **Ask Again Later** keeps them. Restore is offered only when the world file is byte for byte what it was when the copy was made; if the file changed or vanished, the changes can still be saved as a copy. A damaged copy is reported and never used.

### Snapshots

**File → Take Snapshot** stores the current state of the world under a name you give, in the same folder in your user profile. **File → Snapshots** lists the snapshots of the world, newest first; restoring one is a single undo step and does not touch the file on disk, and a snapshot can be deleted there. The twenty newest snapshots of each world file are kept.

## Minimap and statistics

The **Minimap** shows the whole world and outlines the part visible on the canvas; click or drag in it to move the canvas. It can show both layers or one.

**World Statistics** lists file size and revision, occupied and empty cells and the values used in each layer, object slot usage and counts by stored type, channel and icon, the four starts, the bounds of used cells, and the byte ranges that differ from the saved file and from the file as opened. Double-click a row to find its cells, records or bytes on the canvas. These are stored facts; they are not interpreted for a game.

**Edit → Find** (Ctrl+F) jumps to a cell coordinate, a stored tile value in the active layer, an object type or an object slot.

## World Doctor

**World → Validate** (F8) opens the Diagnostics panel. World Doctor checks structure and known limits; it never changes the world and it does not judge whether a world can be completed (one labelled estimate apart) or is fun. The list refreshes after every edit, and a summary is shown with every Save and Save As without blocking it.

Each finding has a rule ID, a severity (error, warning or note), the game it was judged for and the basis of the rule: a structural fact, something read in the game's source, a value outside a verified range, something reported but not re-checked, or an estimate. Double-clicking a finding shows it on the canvas, in the Objects panel and in the Binary Inspector.

Findings include: a truncated file, extra bytes, an unknown revision; music or region outside the game's range; a start outside the world or with an unusual direction; a title without a terminator, longer than 40 bytes or ignored by the game; tile numbers below -1, without a picture or without collision data in the chosen game; Speedy Blupi II's replacement of stored values 48–67 by 215 when it loads a world; border tiles whose form differs from what the game's editor would produce; objects of a type created only while playing or unknown to the game, with an unexpected channel, outside the world, or with a path and a zero duration; a full object pool and a nearly full one.

One finding is an estimate about play rather than about the file: **REACH-CUT-OFF** reports a treasure or goal whose cell is not connected to any start by cells free of fully solid tiles (up, down, left, right). Such an object can only be collected if the game changes the map while playing. The estimate knows nothing of jumping, falling, vehicles or lifts, so the absence of this note does not mean a world can be completed; and it is not made for Speedy Blupi II worlds that contain teleporters or key doors. It is a note, labelled as an estimate, and reports nothing on the original worlds of either game.

Only one repair is offered: ending a title that fills its whole field. It is shown with the exact byte it changes and applied as one undo step after you confirm. **World → Compare with Original** and **Compare with Saved** list every changed byte range with old and new bytes.

## Binary Inspector

**View → Binary Inspector** (Ctrl+B) shows the whole file as a hex dump. It cannot change anything. Header, foreground, bigDecor, object records and any extra bytes are tinted differently; bytes changed since the file was opened are highlighted and unsaved bytes underlined.

For the byte under the cursor it shows the offset, the structural name (for example `header.music`, `foreground[3][70]`, `objects[137].posEnd.y`), the arithmetic that leads there, the value as signed and unsigned little-endian number, the original value and, for a byte with unsaved changes, the saved value. Cells are stored column by column: cell (x, y) of a layer is at `layer start + 2 × (100 × x + y)`.

Type an offset (`1608` or `0x648`), `cell 3,70`, `big 3,70` or `slot 137` to jump; pick a section from the list; search for hex bytes or ASCII text; set bookmarks; choose 8 to 32 bytes per row; view the original bytes or original and current side by side. The inspector follows the cell cursor and the selected object, and clicking a byte moves the canvas there.

## Working with a game installation

**View → Installation** shows a dock (tabbed with the Palette) for the game directory you chose for the current world's game (**View → Game Artwork Directory**). It lists every world in the directory's `DATA` folder with a small picture of its used part: the game's own worlds (`world012.blp`) with the role their number has in the game — first world, group hub, mission, final world, network game, demo — and the worlds a player designed (`u001-003.blp`: player 1, world 3), with the title where the file stores one. A file that is not a complete world is marked. Below the worlds the dock lists the installation's demo recordings with the world each is replayed on and its length; double-click one to see its keys (see **Demo recordings**). Double-click a world to open it. The games keep no list of designed worlds: a file with the right name is all it takes for the game's design screen to find it.

**Deploy Current World** (also **File → Deploy to Installation**) puts the world of the current tab — as it is now, unsaved changes included — into that folder. You choose the place: a designed world of player 1 to 8, number 1 to 20 (the place the tab's own file name stands for is offered if it has one, otherwise the first free number of player 1), or one of the game's own world numbers, which makes the game play your world in its place. The dialog says whether a file is created or replaced. A replaced file is first copied to a folder in your user profile (never inside the game directory) and checked; your world is then written through a temporary file that takes the place of the target in one step, and read back. If any step fails, the game directory holds exactly what it held before. Deploying does not save the tab's own file. A file that is open in the editor is not a deploy target.

**Roll Back Last Deploy** undoes the newest deploy after asking: the kept file is put back and verified, or the created file is removed. If the file was changed since the deploy (for instance by the game's own editor), it is left alone and you are told.

**Playtest** (F5) starts the command you gave for the game — its program, or for example `wine` followed by it — in the game directory. The command is asked for the first time and can be changed in **Edit → Playtest Command**. The games have no option that opens a particular world, so you pick it in the game: on the title screen choose the player, then the design button, and step to the world's number.

### Campaigns and packs

**File → Campaign Overview** lists the worlds of the installation in the order the game reaches them — the first world, each group's hub followed by its missions, the final world, network and demo worlds, other numbers, then designed worlds — and says what the numbering alone shows: a missing world 1, a hub whose missions are absent or missions whose hub is absent, numbers outside the scheme. Below that it follows the door tiles of the official worlds — from the first world a door leads to its number times ten, elsewhere to the group's hub plus its number, and the exit door back to the first world — and reports a door that leads to a world file that is not there, and a hub or mission that no door leads to. Before those findings the overview lists the connections themselves, one line per world that has doors: `World 1 -> 10, 20` means the first world's doors lead to the hubs 10 and 20.

**File → Export Campaign Pack** writes worlds of the installation, with a manifest (name, game, and for each world its place, size and SHA-256), as one ZIP file or as a folder. The designed worlds and the worlds this editor itself deployed are offered ticked. Files in the game's own world numbers that the editor did not put there are most likely the game's originals: they are listed but cannot be ticked until you tick the box saying that a pack containing them must not be passed on. The ZIP is written without compression (worlds are small) and opens in any ZIP program.

**File → Import Campaign Pack** reads a ZIP pack, or a folder pack when you choose the `eggbert-campaign.json` inside it, and checks everything before anything is written: the manifest, that every member is listed and matches its size and checksum, that every world is complete and sits under the file name of a place the games use. Member names may contain only letters, digits, dot, hyphen and underscore, so nothing in a pack can point outside the pack. Members may be stored as they are or compressed with Deflate, which is what ZIP programs do by default, so a pack whose files someone unpacked and zipped again is read too, as long as the files are at the top level of the archive (a folder inside it is refused like any other path); while decompressing, a member never produces more than the size the archive states for it. Encrypted or oversized members, other compression methods, and split or ZIP64 archives are refused with the reason. A pack for the other game is refused. You then see the worlds and which existing files they replace; on confirmation each world is deployed exactly like **Deploy Current World** (kept copy, one-step replacement, read-back). If one world cannot be placed, the ones already placed are taken out again, so an import is all or nothing; afterwards **Roll Back Last Deploy** removes the imported worlds one at a time.

## Experimental editing

Off by default. **Edit → Preferences → Experimental editing** switches it on; the status bar then says so. It lets you write values the editor has no evidence for, so the result may be something no game expects. Three things become available:

- **Edit → Edit Bytes** replaces bytes at an offset you type (decimal, or hexadecimal with `0x`; it starts at the Binary Inspector's cursor) with the hex bytes you type, and shows what is stored there now. The edit is one undo step like any other. If it touches a range the editor otherwise preserves — the reserved header fields, the `world` field or bytes after the end of the world — it names those offsets and asks again before changing them. The revision field (offsets 0 to 3) is never edited, because it decides how the rest of the file is read, and the file never changes length.
- **World → Metadata** accepts any music and region number, and says when a number is outside the range verified for the game.
- **Edit → Load Custom Border Rules** reads a text file with border rules of your own and adds them to the Adaptive brush chooser as **Custom: name**. A rule is sixteen tile numbers, one for each combination of joined neighbours:

  ```
  # mask order: +1 the cell below joins, +2 above, +4 right, +8 left
  [Stone wall]
  forms = 153 147 148 146 40 151 150 144 39 152 149 145 38 36 37 35
  ```

  Painting with it writes the form for no neighbours and then gives the painted cell and its four neighbours the form that matches which of their neighbours hold a number of the same rule; the edge of the map counts as joined. Only the foreground layer changes. These rules are yours, not a game's: nothing checks that a game draws them as a border. The file is remembered and read again at the next start while experimental editing stays on.

Read-only worlds stay read-only.

## Exporting a picture

**File → Export World PNG** (Ctrl+E) writes one image of the complete world: 6,400 × 6,400 pixels at the native 64 pixels per cell, or smaller. Choose the whole world or only the selected cells, the layers and overlays, a solid or transparent background and, with artwork, whether the world's backdrop picture is drawn (tiled from the top-left corner, without the canvas's scrolling shift); the dialog shows the size and the memory needed first. The picture is drawn with the same code as the canvas and records the game profile, the kind of drawing and the settings in its metadata. It is schematic unless you tick **Draw with game artwork** (available when artwork for the tab's game is loaded). Pictures drawn with artwork contain copyrighted material of the game's owners: keep them for your own use unless you have permission to share them.

**File → Export World SVG (Schematic)** writes a scalable drawing instead: one square per occupied cell, outlines for bigDecor, a circle per object with a dashed line along its path, and the four starts. It never contains game artwork, so it can be shared freely, and the drawn elements say what they stand for — `data-tile` on a tile's square, `data-slot` and `data-type` on an object's circle, `data-path-of` (the slot) on the line of its path, `data-start` on a start — which makes the file usable by other tools. A file without a complete tile layer cannot be drawn.

## Regression scenarios

A scenario is a small text file of things that should stay true about a world while you keep working on it: how many treasures it holds, that World Doctor finds no error, where the first start is. **File → Record Scenario** writes everything that is true of the world in the tab right now, one expectation per line:

```
# Eggbert Editor world scenario: delete or loosen (>=, <=) lines as needed.
doctor.errors = 0
foreground.occupied = 412
objects.type.5 = 12
start.1.x = 194
```

Delete the lines you do not care about and loosen others with `>=` or `<=` (for example `objects.type.5 >= 10`). **File → Check Against Scenario** then compares the world in the tab — unsaved edits included — with such a file and lists each expectation that no longer holds with its line and both numbers. A name the world has nothing for (an object type that does not occur, a rule that reports nothing) counts as 0. The facts recorded are the revision, the file size, music, region, scrolling and the four starts from the header, the occupied cells of each tile layer, the objects in use, free and per stored type, and the World Doctor findings per severity and per rule (they depend on the game chosen for the tab). The commands `scenario-record` and `scenario-check` do the same without a window, so a folder of worlds can be checked by a script.

## Appearance and layout

Panels can be moved, floated, stacked and closed; **View** lists them. **View → Layout** arranges them for mapping, object work or validation, and **Reset Layout** restores the default. **View → Theme** switches between the system, light and dark appearance. Window layout, theme and recent files are remembered.

**Edit → Preferences → Language** chooses the language of the interface: the system's, English or Czech (Čeština). It takes effect the next time the editor starts. The Czech translation covers the interface's own texts, the names of tiles, objects, groups and world roles from the game catalogs, and the sentences the checking and converting code composes: the findings of World Doctor and of the Campaign Overview, the notes of conversions, the encyclopedia's facts, the problems of a Mobile Eggbert text world, the lines of a scenario check and the messages about packs and deploys. The standard buttons of dialogs (OK, Cancel, Save, Discard and the like), the menu of an input field and the buttons of the docks are Czech as well. What stays English are the structural names and arithmetic of the Binary Inspector (they are the format's own terms), citations of evidence, a file name that is refused as a pack entry, the diagnostics file and the command line. With Czech chosen, **Help → User Guide** shows this guide in Czech.

Every control of the main window that can take the keyboard focus has a name for screen readers, including the picture lists of the Palette and the fields of the Objects panel, and so do the lists and choosers of the conversion, snapshot and pack dialogs. Other dialogs rely on their visible labels.

**Help → Diagnostics Bundle** saves a text report to attach to a problem report: program, Qt and platform versions, the structural report of the current document (with its file name but not its folder, and with the decoded title and header values), the World Doctor findings, and the byte ranges changed since opening and since the last save with their old and new bytes. The rest of the world's data is not included.

## Command line

Besides opening files (`eggbert-editor world.blp ...`), the program has commands that open no window and never change the world file they read:

- `eggbert-editor info FILE [--events]` prints the structural report and the decoded summary. For a Mobile Eggbert text world or a demo recording it describes that file instead; `--events` adds every event of a recording.
- `eggbert-editor validate FILE [--game 1|2]` prints the World Doctor findings, one per line with rule, byte range and message; the exit code is 1 when there are errors. A Mobile Eggbert text world is checked as a world of that game and takes no `--game`; the same holds for `export-png`, `export-batch` and the scenario commands.
- `eggbert-editor diff FILE OTHER` lists the differing byte ranges with their structural names and old and new bytes; the exit code is 1 when the files differ.
- `eggbert-editor export-png FILE OUT.png` writes the picture **File → Export World PNG** writes. Options: `--scale N` (pixels per cell, 1 to 64), `--crop x,y,width,height` (in cells), `--game 1|2`, `--game-data DIR` (draw with that game's artwork; for a Mobile Eggbert text world the port's folder), `--no-foreground`, `--no-bigdecor`, `--no-objects`, `--no-starts`, `--no-paths`, `--no-backdrop`, `--grid`, `--labels`, `--transparent`.
- `eggbert-editor convert FILE OUT --to 1.0|1.1|1.2|1.3 [--allow-loss]` writes a copy in another revision, as **File → Convert Revision** does. `OUT` must not exist. When the target cannot store everything, nothing is written and the exit code is 1 unless `--allow-loss` is given.
- `eggbert-editor convert-game FILE OUT --from 1|2 --to 2|1 [--unknown refuse|keep|remove]` writes a copy for the other game, as **File → Convert for Other Game** does, and prints the list. `OUT` must not exist. Without `--unknown keep` or `--unknown remove`, a world with values unknown to the target game is not converted: the list goes to the error output and the exit code is 1.
- `eggbert-editor merge BASE MINE THEIRS OUT [--prefer mine|theirs]` combines what two worlds changed starting from the same base. The three files must be complete worlds of one revision and the same length. The merge goes element by element — one header value, one cell, one whole object record, or one byte elsewhere: an element changed on one side takes that side's bytes, one changed alike on both takes them, and one changed differently on both is a conflict. With conflicts and no `--prefer`, they are listed (offset, name, the three versions), nothing is written and the exit code is 1; with `--prefer`, that side's whole element is taken for each conflict. `OUT` must not exist. **File → Merge Changes** does the same in the window, with each conflict decided separately. Three Mobile Eggbert text worlds are merged value by value in the same way, and `OUT` is then `MINE`'s text with the merged values; text worlds and `.blp` worlds are not mixed.
- `eggbert-editor new OUT --game 1|2|mobile` writes the world **File → New World** writes; with `mobile` a Mobile Eggbert text world. `OUT` must not exist.
- `eggbert-editor export-batch OUTDIR FILE... [options of export-png]` writes one picture per world into an existing directory, each named after its world (`world001.blp` becomes `world001.png`) and identical to what `export-png` writes for it. Two worlds with the same name are refused. A world that cannot be drawn does not stop the others; the exit code is then 1. A wrong option is reported once, nothing is written and the exit code is 2.
- `eggbert-editor export-svg FILE OUT.svg [--scale N] [--no-foreground] [--no-bigdecor] [--no-objects] [--no-starts]` writes the schematic drawing **File → Export World SVG** writes; `--scale` is the size of one cell (1 to 64, 16 when not given).
- `eggbert-editor scenario-record FILE OUT.txt [--game 1|2]` writes the scenario **File → Record Scenario** writes. `OUT` must not exist.
- `eggbert-editor scenario-check FILE SCENARIO.txt [--game 1|2]` checks a world against a scenario and prints each expectation that fails; the exit code is 1 when some fail and 2 when the file is not a scenario.

Exit codes: 0 success, 1 findings, differences, a refused loss, merge conflicts, pictures that could not be written or failed expectations, 2 wrong usage or a file that cannot be read or written. `eggbert-editor --help` lists everything.

## Demo recordings

The games ship recordings of someone playing (`demo200.blp` and following in the first game, `demo300.blp` and following in the second) and replay them from the title screen. Opening one — by File → Open World, on the command line or by dropping it — does not make a tab: a read-only window shows what it holds.

- A **timeline** with one row per key (Left, Right, Up, Down, Jump, Fire) and a bar for every stretch the key is held. Move the pointer over it, or give it the keyboard focus and use the Left and Right arrow keys (Page Up/Down move by a twentieth of the recording, Home/End to its ends), and the line below names the step, the approximate time and the keys held there.
- A **table** with how often each key was pressed and for how long it was held.
- The **world** it belongs to: the games replay `demoNNN.blp` on `worldNNN.blp` of the same folder, and **Open World NNN** opens that file when it is there.

A recording stores the keys, not where the player was, so no path through the world and no heat map can be drawn from it: that would need the games' movement rules, which the editor does not have. Times are in game steps; seconds are given as "about", for the games' normal pace of 50 ms a step. The file is recognised by its content: a 204-byte header carrying revision 1.0 and nothing else, then at most 10,000 four-byte events whose steps start at 0 or later and never run backwards and whose keys are among the six. Anything else — and any complete world of revision 1.0 — is treated as a world file. Recordings cannot be edited or written.

`eggbert-editor info demo200.blp` prints the same numbers, and with `--events` every recorded event.

## Mobile Eggbert worlds

Mobile Eggbert, the port of the phone game, stores its worlds as text files (`world011.txt`): one `DescFile:` line of `name=value` fields, a `Decor:` and a `BigDecor:` block with one comma-separated line per map column, and one `MoveObject:` line per object. Such a file holds what a world of the Windows games holds — two layers of 100 × 100 tile numbers, up to 200 objects with the same thirteen numbers each, a start, music, backdrop and scroll position — so the editor opens it as an ordinary tab, and everything described above works on it: the Palette, the pencil, shapes, fill and selections, border brushes, objects and their paths, stamps, the minimap, statistics, World Doctor, the encyclopedia, snapshots, recovery after a crash, Merge Changes, PNG and SVG export and scenarios.

- **The game** of such a tab is always **Mobile Eggbert**, and the game box in the toolbar is locked. The port carries the same tile and object tables as the second Windows game, so the names, groups, border brushes and object presets are that game's. Three differences are established and applied: the port does not replace tiles 48 to 67 when it loads a world, tiles 49 to 67 are not solid in it, and it reads tile number 0 as an empty cell, which World Doctor warns about. Durations of moving objects are shown as the file stores them; the port scales them to its own pace when it loads a world and gives objects of type 54 a fixed wait at both ends.
- **Pictures** come from the port's own `Content` folder (`icons` and `backgrounds`). When the world lies in the port's `worlds` folder they are found by themselves; otherwise choose the port's folder with **View → Game Artwork Directory**. The port has no music files, so there is nothing to play.
- **One start.** The file has one start and one direction: World Metadata edits the first start only, the start tool moves it, and the title cannot be typed, because the file has no place for one.
- **Saving writes text.** Only the values you changed are rewritten, each in its place; everything else — spacing, unknown fields and lines, the order of the object lines — is written back byte for byte, and saving a world you did not change writes the identical file. Deleting an object removes its line; a new object gets a line at the end. Save goes through a temporary file and is refused when the file changed on disk, as for any world. The copies kept for recovery and as snapshots are text as well. A file with Windows line ends (CR LF) keeps them, and its tab shows a caution: the port's world reader cuts lines at line feeds only, so unless the game removes carriage returns before that, it does not read the last value of each tile line, which is the bottom row of the map.
- **The Installation dock** lists the worlds of the port's `worlds` folder with a small picture and their role (the port numbers its campaign as the second game does: world 1, the hubs 10, 20 and so on with their missions, and 199 as the last). **Deploy Current World** writes the tab's world there as `worldNNN.txt` under the number you give and keeps what it replaces for **Roll Back Last Deploy**. **Playtest** starts the command you gave for this game in the folder that contains `Content`. **File → Campaign Overview** lists these worlds and checks their numbering and doors by the port's rules, which are the second game's.
- **File → New World** offers Mobile Eggbert as a third game and writes the same small starting world as a text file.
- **Not offered** for these worlds, because they concern the `.blp` formats: Convert Revision, Convert for Other Game, the Binary Inspector, typed byte edits and campaign packs. A stamp made in a world of another game is not placed here, as between the two Windows games.
- **A file the editor cannot lay out** — more than 200 object lines, an object of type 0 in the list, a number too large for the layout, or a missing block — opens in a plain window of its own instead, and the status bar says why. That window shows numbered squares, paints, draws lines and rectangles and fills a selection with a tile number, edits the header and each object line in tables, lists what is wrong with its line number on a Problems page, and replaces exactly one token of the text per edit.

`eggbert-editor info FILE` describes such a file without a window, and the other commands take it as said under **Command line**. A text world is never written as a `.blp` file, nor a `.blp` world as text: the two formats belong to different games. The editor only holds the text world in memory in the layout it uses for every world; that layout is never saved.

## What the editor does not do

It does not edit revisions 1.0 and 1.1 in place (convert a copy instead), does not edit bytes in the hex view itself (see **Experimental editing** for typed byte edits), does not offer objects it has no evidence for (raw tile numbers and the numeric fields of a verified object are always yours to type; anything else only as typed bytes with experimental editing on), does not offer border brushes or music and region values it has no evidence for unless experimental editing is on, does not renumber campaigns, and does not convert to or from the Mobile Eggbert format.
