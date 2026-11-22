# Research evidence

Observations that Eggbert Editor's behavior relies on, with the exact source revision. They are evidence, not code: everything in this repository is written independently and tested with synthetic fixtures. Each line says how the observation was obtained:

- **read** — the cited lines were read in this repository's development sessions;
- **reported** — taken from another project's notes and not re-measured here;
- **untested** — not yet checked in an original executable or against a retail sample.

Unless stated otherwise nothing below has been tested in an original executable.

## Sources

- `blupi_recon`, <https://github.com/openeggbert/blupi_recon>, commit `e05e22b990abb782c37a0d9fafa864d86a27e29f`. Speedy Blupi/Eggbert 1 (SB1) is `src/`, Speedy Blupi II/Eggbert 2 (SB2) is `games/sb2/src/`. Its manifests mark many cited functions as compiling to the original machine code.
- Archived `eggbert-generator` bundle, HEAD `069c409611eb779e9066e8d492faed4550dac9a8`, bundle SHA-256 `8bed90c38f4646061d50f8a130f8b303b064d0b49a237a81433598c793f5f668`.

## File layout (both games)

| Observation | Where | How |
| --- | --- | --- |
| 868-byte `DescFile` header with the field order in `plan.md`; `Cellule` is one `short`; `MoveObject` is 48 bytes | `src/decor.h:27-51`, `:99-115` | read |
| Reader skips `bigDecor` only when the revision is exactly 1.0; reads 100 object records when `majRev == 1 && minRev <= 1`, else 200; copies the title only when `majRev >= 1 && minRev >= 3` | `src/decor.cpp:360-433` | read |
| Before reading a world the games fill both layers with `0xFFFF` and zero every object type; the reader for revisions 1.0 and 1.1 zeroes all 200 records before reading 100. So a 1.0 world in memory has an empty `bigDecor` layer and a 1.0 or 1.1 world has slots 100–199 all zero — the image `convertRevision` writes when upgrading | `src/decblock.cpp:863-888`; `src/decor.cpp:371`, `:409-425`; `games/sb2/src/decor.cpp:686-703` | read (first game); second-game branch conditions read at `:686`, `:692` |
| Writer zero-fills the header, writes revision 1.3 (1.2 and `reserve1[0] = 1` in the June 1998 demo build), then both layers and all 200 records | `src/decor.cpp:297-357` | read |
| Stock SB1 worlds are revision 1.2 with an all-zero title; stock SB2 worlds are 1.3; demo and Valueware editions use a non-zero `reserve1[0]` (256 in shipped demo worlds); `world` is 0 in all measured files | generator `docs/formats/BLP_SB1_WORLD.md` | reported |
| An empty cell is stored as -1 | `src/decblock.cpp:878-879` (cited by research notes); generator format notes | reported |

## Geometry and header fields (both games unless noted)

| Observation | Where | How |
| --- | --- | --- |
| Cells are 64 × 64 world pixels, the map is 100 × 100 cells, the view is 640 × 480 | `src/def.h:38-39`, `:51-52`, `:57-58`; `games/sb2/src/def.h:36-37`, `:49-50`, `:55-56` | read |
| `DIR_LEFT` is 1, `DIR_RIGHT` is 2 | `src/def.h:726-727`; `games/sb2/src/def.h:724-725` | read |
| The in-game editor places a start at `cell * 64 + (2, 6)`; clicking the cell that already holds it toggles left/right; Ctrl copies start 1 to starts 2–4 | `src/decdesign.cpp:762-783` | read |
| A single-player game starts at `blupiPos[0]`/`blupiDir[0]`; only a multiplayer game uses the start of the player's team | `src/decblock.cpp:976-986` | read |
| Music choices are 0–10 (11 buttons, value `message - WM_BUTTON1`); 0 plays nothing | `src/event.cpp:416-426`, `:3086-3090` | read (meaning of 0: reported) |
| Regions are 0–23 in SB1 and 0–31 in SB2 | `src/event.cpp:3093`; `games/sb2/src/event.cpp:3679` | read |
| `dimDecor` is only tested for zero/non-zero per axis; the editor writes 100 or 0 | research notes citing `src/event.cpp:3098-3130`, `src/decblupi.cpp:3707-3725` | reported |
| `posDecor` is the saved scroll position and is recomputed during play; `world` is never read or written | research notes citing `src/decor.cpp:324`, `:388`, `src/decblupi.cpp:3685-3688` | reported |
| Title entry accepts bytes 0x20–0xFF while the length is below 40; storage is `char name[100]` copied with `strcpy` | `src/event.cpp:3976-3990`; `src/decor.h:112` | read |
| Tiles with collision data: SB1 0–236 (`MAXQUART` 237), SB2 0–440 (`MAXQUART` 441) | `src/decor.h:13`; `games/sb2/src/decor.h:13` | read |
| SB2's loader rewrites stored foreground values 48–67 to 215 in memory; SB1 has no such step | `games/sb2/src/decor.cpp:675-684` | read |

## Objects

| Observation | Where | How |
| --- | --- | --- |
| A slot is free when `type == 0`; the editors take the lowest free slot and zero all 48 bytes before filling it; with a full pool the editor places nothing | `src/decmove.cpp:1668-1679` (`MoveObjectFree`) | read |
| Deleting clears only `type` for every record whose start or end cell is the cell; the other 46 bytes stay | `src/decmove.cpp:1650-1666` (`MoveObjectDelete`) | read |
| Every placement writes `posEnd = posCurrent = posStart`, `step = 1` (`STEP_STOPSTART`), `time = 0`, `phase = 0` | `src/decdesign.cpp` placement blocks (extracted and compared line by line); `src/def.h:732-735` | read |
| SB1 presets: type, step durations, waits, position offset in the cell, channel and icon for lifts (4 speeds), crates, bulldozer, fish, bird, moving bomb, enemy helicopter/tank, treasure, egg, power, shield, drink, bullets, charger, goal, floor/ceiling bombs, personal bombs 1–4, helicopter, skateboard, jeep, tank, key | `src/decdesign.cpp:313-1054`; exact line ranges are in each preset's `provenance` in `src/world/object_catalog.cpp` | read |
| SB2 presets: the SB1 set plus homing bomb, inverter, conveyor lifts (types 47/48), wasp, creature, dynamite, coloured keys 1–3, hovercraft | `games/sb2/src/decdesign.cpp:333-1316`; line ranges in `object_catalog.cpp` | read |
| Channels: 1 tile sheet, 2 and 11–13 player sheets, 10 element sheet (`CHELEMENT` 10, `CHBLUPI1..3` 11–13) | `src/def.h:699-702`; `games/sb2/src/def.h:697-700` | read |
| Setting a mover's end: `posEnd = cell * 64`; `stepAdvance` and `stepRecede` are multiplied by `max(|dx|, |dy|) / 64` | `src/decdesign.cpp:283-311` | read |
| Default lift icon: first entry of the table selected by the initial editor state: 153 in SB1 (`{153, 74, 75}`), 29 in SB2 (`{29, 10, 2, ...}`) | `src/decdesign.cpp:262-281`, `src/decblock.cpp:208-231`; `games/sb2/src/decdesign.cpp:449-477`, `games/sb2/src/decblock.cpp:266-276` | read |
| Crate icons offered by the editors: plain `{32, 33, 34}`; disguised SB1 `{2, 4, 79, 84, 48, 50, 143, 219, 230}`, SB2 `{2, 4, 79, 84, 143, 219, 230, 246, 341, 396}` | `src/decblock.cpp:233-239`; `games/sb2/src/decblock.cpp:302-306` | read |
| Which types are created only during play (explosions, projectiles, splashes, doors, trails, SB2 additions) | research notes over `src/decblupi.cpp`, `src/decmove.cpp`, `games/sb2/src/...` | reported |
| The games recompute `posCurrent`, `step`, `time`, `phase` when entering the editor or starting play, so stored values of these fields do not affect behavior | research notes citing `src/decblock.cpp:1019-1104` | reported |

Eggbert Editor does not copy two side effects of the games' editors: placing an object does not delete other objects on the same cell, and placing a goal or key does not free existing goals and keys. Both would silently remove content; they are left to the user and to World Doctor.

## Adaptive borders

| Observation | Where | How |
| --- | --- | --- |
| After an edit the games' editors refresh five cells in this order: the cell, its right, left, lower and upper neighbour; nothing else, and never on load or at game start | `src/decor.cpp:180-186`; `games/sb2/src/decor.cpp:414-420` (call sites: research notes) | read (order), reported (no other callers) |
| A cell's form is `table[row * 16 + mask]` with mask bit 1 = lower neighbour joins, 2 = upper, 4 = right, 8 = left; the row is the first one containing the stored value | `src/decor.cpp:158-178`; `games/sb2/src/decor.cpp:268-346` | read |
| Rows (16 forms each): rock, rock with 156 as full form, triangular rock, grass, bar in both games; bricks, green slime, palace, pipes in SB2 only | `src/decblock.cpp:757-763`; `games/sb2/src/decblock.cpp:919-929` | read |
| "Joins" is a per-neighbour, direction-dependent rule list; outside the playfield counts as joined, and the playfield is 10 cells wide / 8 high on an axis whose `dimDecor` is 0 | `src/decor.cpp:116-156`; `games/sb2/src/decor.cpp:190-248` | read |
| SB2 only: a palace cell (386–397, 400) joins only palace, a pipe cell (250–260) only pipes; extra rules for 317, 309/310, 410–420, 334–336, 264–282, 378, 404, 421–440; 182/183 join vertically (never in SB1) | `games/sb2/src/decor.cpp:196-246` | read |
| Random looks: 35 ↔ 156 by coin in both games (SB1 keeps a stored 156 while surrounded; SB2 folds 156, 252/253, 255, 362, 363 and 342–346 back before lookup and draws again); SB2 251 → 251–253, 254 → 255, 347 → 362, 348 → 363, 341 → 341–346 | `src/decor.cpp:173`; `games/sb2/src/decor.cpp:290-342`; `games/sb2/src/misc.cpp:99-109` | read |
| SB2 cheese (246–249, 339) and cave (284, 301, 337): the solid keeps its value; an empty cell or existing border cell next to it gets a border tile from a 16-entry table by which neighbours are cheese/cave (map edge does not count); some border tiles have a coin-flip alternative | `games/sb2/src/decor.cpp:250-266`, `:347-411`; `games/sb2/src/decblock.cpp:931-937` | read |
| SB2 lightning: an emitter (304) fills up to four empty cells below with 305; a cell that ends empty clears up to four consecutive 305 cells below | `games/sb2/src/decor.cpp:435-469` | read |

Deliberate differences from the games' editors, so that an adaptive edit never removes unrelated content: Eggbert Editor does not delete objects standing on a cell that becomes solid, and it does not reproduce the source quirk by which a freshly written four-cell lightning beam also clears beam cells five to eight rows below the emitter. Every cell an adaptive stroke would change is previewed before the click.

## Drawing

| Observation | Where | How |
| --- | --- | --- |
| A sprite is found through a table per sheet: a count, then six 16-bit values per sprite `{srcX, srcY, offsetX, offsetY, width, height}`; the sprite is drawn at the requested position plus the offset; a number at or beyond the count is not drawn | `src/pixmap.cpp:781-800` | read |
| Channel 1 uses the tile sheet table, 10 the element table, 9 the explosion/bigDecor table, 2 and 11–13 the player table | `src/pixmap.cpp:788-830` (first branches read; the rest per research notes) | read / reported |
| Table sizes: first game tiles 246, elements 187, bigDecor/effects 54 (its last record lacks the height), player 291; second game 441, 289, 100, 336 | `src/pixtables.h:19-20`, `:314-315`, `:564-565`, `:755-756`, `:809-811`; `games/sb2/src/pixtables.h:20-21`, `:137`, `:289-290`, `:390` | read (counts only; the tables themselves are not copied) |
| Sprite sheets are loaded as ordinary bitmaps from `image08\` (`object.blp`, `element.blp`, `explo.blp`, `blupi000.blp`–`blupi003.blp`) and drawn with pure blue RGB(0, 0, 255) as the transparent colour | `src/blupi.cpp:760-858` (transparency lines read; file names per research notes) | read / reported |
| bigDecor cell (x, y) is drawn from channel 9 at `(x * 64, y * 64 + 2)`; value 203 is drawn from channel 1 instead; second game: values 66–68 are drawn 13 pixels higher and 87–89 two pixels higher | `src/decblock.cpp:1209-1231`; `games/sb2/src/decblock.cpp:1643-1647` | read |
| Objects are drawn at `posCurrent` with their stored channel and icon, highest slot first, before the foreground; types 4, 32 and 33 are shifted by (2, 6); second game also type 54 by (0, 6); lifts (type 1) are drawn after the foreground | `src/decblock.cpp:1359-1377`; `games/sb2/src/decblock.cpp:1809-1817` | read (lift pass and second-game lift types 47/48: reported) |
| Foreground cell (x, y) is drawn from channel 1 at `(x * 64, y * 64)`; values 107–109 and 157 seven pixels higher; second game value 364 two pixels higher | `src/decblock.cpp:1378-1410`; `games/sb2/src/decblock.cpp:1846` | read |
| In the games' editors a start is drawn with player sheet 2, 11, 12 or 13 by slot, picture 0 when facing right and 4 otherwise | `src/decblock.cpp:1234-1250` | read |
| Attract-mode recordings are `data\demo2NN.blp` in the first game and `data\demo3NN.blp` in the second | `src/event.cpp:3723`; `games/sb2/src/event.cpp:4372` | read |

Eggbert Editor does not ship the sprite tables. At run time it searches the executable in the user's chosen game directory for a table with the expected count whose every rectangle fits the sprite sheet found there, and uses it only then. The automated tests exercise this with generated sheets and a generated stand-in executable. The results with two original installations are in the next section.

## Checks against original installations (2026-11-22)

Inputs, kept outside the repository and only read: the installed files of *Speedy Eggbert 1* (game executable `egames.exe`, 534,528 bytes) and *Speedy Eggbert 2* (`Eggbert2.exe`, 602,624 bytes), supplied by the project owner as two archives. Archive checksums are in local, uncommitted notes. No game binary was executed. Tools: the repository's opt-in `sample_matrix` test and an uncommitted survey program linked against the editor's libraries.

| Observation | Result | How |
| --- | --- | --- |
| Stock worlds | first game: 59 `world*.blp`, all revision 1.2, 50,468 bytes; second game: 96, all revision 1.3, 50,468 bytes; none has bytes after the layout; no world of revision 1.0 or 1.1 exists in either installation | tested |
| Lossless save | 155 of 155: untouched Save As byte-identical; one-cell edit, save, reopen changes exactly two bytes; undo and save restores the original bytes | tested (`sample_matrix`) |
| Other `.blp` files in `DATA` | 17 recordings (`demo*.blp`) and 2 `info*.blp` begin with zero bytes where a world has its revision, so they probe as truncated revision 1.0 and stay read-only; `time.blp` and `bench.blp` (16 bytes) probe as unknown revision; all are saved back byte-identically | tested |
| World Doctor on stock worlds | 0 errors and 0 warnings in all 155; notes only: `OBJ-FREE-STALE` in every world, `ADAPT-BORDER-FORM` in 34, `OBJ-LOW-HEADROOM` in 3 second-game worlds | tested |
| Header | `world` is 0 in every stock world; `music` 0–10 in both games; `region` 0–23 (first) and 0–31 (second), matching 24 and 32 `decorNNN.blp` backdrops and 10 music files; each `dimDecor` axis is 0 or 100, nothing else; all 620 starts sit at `cell * 64 + (2, 6)`; start direction is 1 or 2; only 7 second-game worlds have a title | tested |
| Tiles | foreground values 1–236 (first) and 1–440 (second). bigDecor values: 16–31, 36–47 and 203 in the first game; 16–31, 36, 37, 39, 41–47, 66–69, 87–89 and 203 in the second. No second-game world stores a foreground value 48–67 (the first game's worlds store them in 8,010 cells) | tested |
| Objects: types | 3,578 + 5,972 active records, every one of a type the catalog marks placeable for its game | tested |
| Objects: fields compared with the presets | each record was compared with every preset of its type: `stepAdvance`/`stepRecede` (for movers: preset value × path length in cells; not compared when a mover's path has zero length), both waits, path end (equal to the start for static objects, a multiple of 64 away for movers), in-cell offset of `posStart`, and channel. The icon was not compared (the games animate it). 9,409 of 9,550 match in all of these: 3,560 of 3,578 and 5,849 of 5,972 | tested |
| Objects: the 141 that differ | (a) 116 second-game records of types 32 and 33 (81 and 35) match in everything except the channel, which is 11 or 13 instead of 12: the editors cycle it through 11–13 on a repeated click. (b) 25 records of type 6 (18 in six first-game worlds, 7 in three second-game worlds) match in everything except the in-cell offset, which is (2, 4) instead of the (2, 8) both released editors write; nothing in the reconstructed source produces (2, 4) for this type, so the cause is unknown (possibly an earlier editor build). No other difference exists | tested; channel cycling read at `src/decdesign.cpp:286-292`, `games/sb2/src/decdesign.cpp:482-487`; type-6 placement read at `src/decdesign.cpp:636-655` |
| Stored run-time state | stock worlds were saved mid-animation: `step`, `time`, `phase` and `posCurrent` differ from a fresh placement in many records, `unknown1`/`unknown2` are non-zero in 450 records, and 20,350 free slots keep non-zero bytes. This matches deletion clearing only `type`, and is why those fields are preserved and not edited | tested |
| Pool usage | at most 187 of 200 slots in use in the first game, 197 of 200 in the second; so original worlds do go below the estimated head-room (hence a note, not a warning) | tested |
| Adaptive borders | of the cells that belong to a table family, 26,860 of 26,904 (first game) and 67,451 of 67,677 (second) store exactly the form `expectedBorderForm` computes for their neighbours (variants folded). The rest are scattered single cells, as left by painting with a raw tile | tested |
| Sprite sheets | uncompressed 8-bit palette bitmaps in `IMAGE08` (all seven files of both games). First game: `object.blp` 896 × 987, `element.blp` 608 × 591, `explo.blp` 784 × 868, `blupi00N.blp` 784 × 770. Second game: 1024 × 1327, 896 × 644, 496 × 1885, 800 × 906 | tested |
| Sprite tables in the executables | all four tables are located in each executable with the expected counts (246/187/54/291 and 441/289/100/336). In `egames.exe` the 54-entry search first also matched 13 slices of the player table, whose sheet has a similar size; since bytes inside an accepted table cannot be another table, those are set aside (`AtlasSearch::excluded`), leaving one candidate. The value read as its last record's height (95) fits the sheet and the cell, so no clamping was needed | tested |
| Located tables compared with the reconstruction | each of the eight tables the editor locates (four per executable) equals the corresponding `table_icon_*` array of `blupi_recon` value for value: 0 differing values in 1,746 + 1,476 + 1,122 + 323 (first game) and 2,016 + 2,646 + 1,734 + 600 (second game). The first game's effects array lists 323 values for 54 sprites, i.e. no height for the last sprite; the 95 the editor reads there is the next data in the executable, which is also what a game reading its own array past the end would use | tested against `src/pixtables.h:19,314,564,755` and `games/sb2/src/pixtables.h:20,137,289,390` |
| Artwork coverage | every stored foreground value (306,107 cells), bigDecor value (1,832 cells), object channel/icon (9,550) and start picture in the stock worlds resolves to a sprite | tested |
| Artwork placement | six rendered scenes (rock, slime, machinery with water, town with trees and grass, lightning, vehicles and lifts) look right: tiles join, grass sits on its soil, bigDecor stands on the ground, items and vehicles rest on floors, water is see-through over the fish. Not compared pixel by pixel with a running game | inspected |

Untested: other editions and languages, the demo releases, and anything that needs the games to run.

## The games' editor menus, backdrop and music

| Observation | Where | How |
| --- | --- | --- |
| The first game's editor has eight buttons: delete, then seven that call `PlaceDecor1`–`PlaceDecor7` with the index chosen in the button's menu; `PlaceDecor8`–`10` exist but are empty and unreachable. The second game has eleven buttons calling `PlaceDecor1`–`PlaceDecor10` | `src/event.cpp:3404-3445`; `games/sb2/src/event.cpp` dispatch near 4039 | read (first game) / reported (second game) |
| The menus hold 68 entries in the first game and 97 in the second; each entry writes tiles from a table (repeated clicks cycle through it), a literal tile, or creates an object. The complete lists are in `src/world/editor_groups.cpp`, each with its line citation | `src/decdesign.cpp`, `src/decblock.cpp` tables; `games/sb2/src/decdesign.cpp`, `games/sb2/src/decblock.cpp` | reported by a scripted extraction; these entries re-read here: Tree `{20, 21, 22}`, toy background, Dream `48–67`, Fan `{126, 129, 132, 135}`; second game: spider's web `{403, 401, 402}`, teleporter `330–333`, cave `284`/`301`, secret cave `337`, cheese `246–249`, secret cheese `339` |
| Every object entry of both menus corresponds to presets of the object catalog and every preset to exactly one entry (12 + 8 + 10 + 1 presets in four first-game buttons; 4 + 14 + 12 + 5 + 5 + 2 in six second-game buttons) | comparison of the extraction's line ranges with the catalog's | tested (`editor_groups` test) |
| The buttons have no names of their own (a button shows the tooltip of its selected entry); the group names in the Palette are this project's | `src/event.cpp` button table | reported |
| The backdrop is `image08\decor%.3d.blp` for the stored region, or the `image16` copy when the game runs in true colour | `src/decblock.cpp:832-847` | read |
| The backdrop is a 640 × 480 picture, tiled, and scrolled at two thirds of the map's scroll position | `src/decblock.cpp:1184-1207`; `src/def.h:908-909` | read |
| Music number n (1 and up) plays `sound\music%.3d.blp` with n − 1; 0 plays nothing | `src/event.cpp:2795-2796`, `:3751-3752` | read |
| The music files are standard MIDI files; `MUSIC000.BLP` of Speedy Eggbert 1 was rendered to 184 seconds of non-silent audio with TiMidity++ and the freepats set | file contents; local run | tested |
| While its editor is open the game keeps stepping the objects (`MoveObjectStep`), so objects move and animate in the editor and their run-time fields are saved as they are at that moment | `src/blupi.cpp:211-218`; `src/decblock.cpp:1111-1150` | read |

## Animation

| Observation | Where | How |
| --- | --- | --- |
| One tick is one drawn frame; the timer period is 50 ms by default, so 20 ticks a second; the tick counter is reset when the editor is entered | `src/blupi.cpp:60`, `:646-650`; `src/decblock.cpp:1545-1546` | reported (timer lines); `m_time++` read |
| Tiles animated when drawn, first game: spring 211; lava 68; water 92 and 91; wind heads 110, 114, 118, 122 (one frame per tick); fan heads 126, 129, 132, 135 (one frame per two ticks); bigDecor 203. Index expressions and frame tables as encoded in `src/world/animation.cpp` | `src/decblock.cpp:26-56` (tables), `:1221-1226`, `:1393-1396`, `:1461-1515` | read (tables, 211, 68); the remaining index expressions reported by a scripted extraction |
| Second game adds: lava with eight frames, slime trap 373, drip 404/410 (nine pixels higher, with blank frames), crusher 317, saw 378, vanishing block 324 (blank frames), lightning beam 305 (in the editor drawn on even ticks only) | `games/sb2/src/decblock.cpp:44-62` (tables), `:1558-1560`, `:1923-1936`, `:1962-1970` | read (tables, drip, beam); the rest reported |
| Lava, water, wind, fans and the second game's traps are drawn in a second pass, after lifts; ordinary tiles before lifts; the second game draws 384 and 385 before all objects | `src/decblock.cpp:1378-1515`; `games/sb2/src/decblock.cpp:1823-2015` | reported |
| In the editor a crate (type 12) whose picture is not 32–34 gets picture 214 drawn over it | `src/decblock.cpp:1373-1376` | reported |
| Object motion, identical in both games: an object with different start and end waits at the start while `time < timeStopStart`, then advances with `posCurrent = (posEnd − posStart) × time / stepAdvance + posStart` (integer, truncating) until it equals the end, waits while `time < timeStopEnd`, and returns the same way with `stepRecede`; each change of state takes one tick, so a cycle is the four values plus four ticks | `src/decmove.cpp:518-600` | read |
| On entering the editor every object is put at `posStart`, waiting, with `time` and `phase` zero | `src/decblock.cpp:1085-1092` | read |
| The picture of an object is chosen every tick from its type, motion state, direction (`posStart.x > posEnd.x`) and `time` or `phase`; `phase` grows by one per tick. 60 rules for the types first-game worlds store, 85 for the second game, as encoded in `src/world/object_animation_data.cpp` with a citation each | `src/decmove.cpp:619-1126`; `games/sb2/src/decmove.cpp` | reported by a scripted extraction; re-read here: treasure chest (`:657-668`), bulldozer waiting at the start (`:802-812`, table `src/decblock.cpp:606-611`), key table (`src/decblock.cpp:712-716`), `phase++` (`:1125`) |
| Enemies do not shoot and nothing collides while the editor is open | `src/decmove.cpp:968-976`, `:1018-1057` | reported |

Deliberate differences of Eggbert Editor's display: stored tile numbers that the games never draw (the frames between the wind and fan heads) are shown as their picture; the second game rewrites a stored drip tile (404 or 410) while drawing it, the editor never writes; for the slime trap the table used while the player is active is shown; object types that only exist while playing have no rule and keep their stored picture. In the first game `phase` is not wrapped by the source; the editor wraps it as the second game does.
