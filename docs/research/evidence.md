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

## The two games compared

Basis of **Convert for Other Game** (`src/world/game_convert.cpp`). "Scripted" rows were produced by scripts over the two source trees and re-run for this entry; "reported" rows come from a source comparison whose citations were machine-checked but not all re-read by eye.

| Observation | Where | How |
| --- | --- | --- |
| The header, cell and object structures are declared identically, and both loaders read the same header fields | `src/decor.h:27-51`, `:99-115`; `games/sb2/src/decor.h:27-51`, `:99-115`; `src/decor.cpp:383-403`; `games/sb2/src/decor.cpp:654-669` | reported |
| The object type constants (72 names, values 1–58, 90–100, 200–203) and the channel constants are the same in both `def.h` files | `src/def.h:995-1066`, `:699`; `games/sb2/src/def.h:993-1064`, `:697` | scripted |
| The first game's code never names types 18, 39–44, 46–57 and 90–100; the second game's code never names type 58 (an effect the first game creates while playing) | search of `src/*.cpp` and `games/sb2/src/*.cpp` for each constant name; `src/decblupi.cpp:2320` | scripted |
| The collision entries of tiles 0–236 are identical in both games; tiles 237–245 have none in either | `table_decor_quart`: `src/decblock.cpp:480-600`, `games/sb2/src/decblock.cpp:408-630` | scripted |
| The first game draws a tile only when its number is below the table's count (246) and tests collisions only below `MAXQUART` (237): second-game tiles 246–440 are neither drawn nor solid there | `src/pixmap.cpp:788-790`; `src/pixtables.h:314-315`; `src/decblupi.cpp:3805` | read |
| The first game's back-layer table has 54 pictures, the second's 100; a larger number is skipped | `src/pixmap.cpp:818-820`; `src/pixtables.h:755-756`; `games/sb2/src/pixtables.h:390-391` | read (first game), reported |
| The second game's tile table gives numbers 48–67 a one-pixel rectangle, and its loader rewrites stored foreground 48–67 to 215; object records are not rewritten. In the first game 48–67 are twenty ordinary solid blocks (editor group "Dream") | `games/sb2/src/pixtables.h:154-160`; `games/sb2/src/decor.cpp:675-684`; `src/decdesign.cpp:212-217` | read (second game), reported |
| A lift (type 1) is drawn with its stored channel and icon; so is a crate (type 12) | `src/decblock.cpp:1414-1424`; `games/sb2/src/decblock.cpp:1861-1870` | read (lift, first game), reported |
| The first game's editor offers regions 0–23, the second's 0–31; the tooltip names differ for 4, 6, 9, 10, 18, 21 and 23 | `src/event.cpp:3093-3095`; `games/sb2/src/event.cpp:3680-3682`; `src/res/blupi.rc:286-300`; `games/sb2/res/sb2-eggbert-en.rc:288-302`, `:414-421` | read (ranges), reported (names) |
| Other differences that change no stored value and are not reported by the converter: lava animation length, the exit door 184 leading to world 99 or 199 in the campaign, door states of official missions, landing sounds, chest sparkles and shield frames | comparison notes | reported |

Checked against the two original installations (2026-11-23, private, read only): the ten music files are byte-identical between the games, so a music number plays the same tune; backdrops 0, 3, 7, 8, 11–14, 16, 17 and 22 are pixel-identical and the other shared numbers differ (many only in colour depth); all 59 stock worlds of the first game convert for the second with nothing unknown (differences: the 48–67 blocks, 19 lift or crate pictures, 11 backdrops) and all 96 of the second convert for the first when unknown values are kept (16 need no choice at all); every one of the 155 is byte-identical again after converting there and back.

## Files of an installation

Basis of the **Installation** dock (`src/world/installation.hpp`). Identical in both games unless noted.

| Observation | Where | How |
| --- | --- | --- |
| Official worlds are `data\world%.3d.blp`; worlds a player designed are `data\u%.3d-%.3d.blp` (player, number) | `src/decor.cpp:279-295`; `games/sb2/src/decor.cpp:560-572` | read |
| Saved games are `data\s%.3d-%.3d.blp`, player files `data\info%.3d.blp`, recordings `data\demo%.3d.blp`; `data\config.def` and `data\time.blp` hold settings (the second game also writes `data\bench.blp`) | `src/decor.cpp:454`; `src/event.cpp:3553`, `:3692`; `src/blupi.cpp:99`, `:623`; `games/sb2/src/decor.cpp:731`; `games/sb2/src/event.cpp:4201`, `:4341`; `games/sb2/src/pixmap.cpp:1605` | read |
| There are 8 player slots; a player's designed worlds are numbered 1 to 20 | `src/def.h:23`; `src/event.cpp:2417-2423`; `games/sb2/src/def.h:21`; `games/sb2/src/event.cpp:3029-3035` | read |
| The games' own play-test writes the world being designed to user world 999 and reads it back, so `uGGG-999.blp` is a scratch file | `src/event.cpp:2356` | read (write); read-back reported |
| A network game loads official world `m_multi + 100` (first game, 1–10) or `+ 200` (second, 1–12); recordings belong to worlds from 200 (first) or 300 (second) | `src/event.cpp:1968-1973`, `:3723`; `games/sb2/src/event.cpp:2581-2586`, `:4372` | read (offsets), reported (ranges 1–10, 1–12) |
| Hubs are recognised by number alone: world 1 and multiples of ten; a group's missions are hub + 1 to hub + 8; hubs run to 80 (first game) or 140 (second); winning world 1 leads to world 99 or 199 | `src/decblock.cpp:1598`; `src/decblupi.cpp:3572-3615`, `:4075-4086`; `games/sb2/src/decblupi.cpp:4158-4201`, `:4804-4818` | read (hub test), reported |
| The games keep no list of designed worlds: the design screen reads one numbered file at a time, and only that file has to exist | `src/event.cpp:3596-3610`; `games/sb2/src/event.cpp:4244-4258` | reported |
| No command-line option selects a world or a player (only `-nocd` in some builds); `data\config.def` has `FullScreen=` among others | `src/blupi.cpp`; `games/sb2/src/blupi.cpp` | reported |

The role shown for an official world is derived from its number by these rules and from nothing else; what a particular installation ships is not assumed.

## Doors between worlds

Basis of the door part of **Campaign Overview** (`src/world/campaign_doors.cpp`).

| Observation | Where | How |
| --- | --- | --- |
| A door is a foreground tile under the player (30 pixels in): tiles 158–165, 166–173 and 174–181 each stand for 1–8 and tile 184 for 99 in the first game; the second game adds 309 and 310 for 9, 411–415 and 416–420 for 10–14, and 184 stands for 199 | `src/decblupi.cpp:4075-4086`; `games/sb2/src/decblupi.cpp:4804-4818` | read |
| Leaving through a door in world 1 continues at that number times ten; elsewhere number 99 leads to world 1 and any other number to the world's group hub (`mission / 10 * 10`) plus the number | `src/decblupi.cpp:3597-3615` | read (first game); the second game's arm with 199 reported |

Checked against the two original installations (2026-11-23, private, read only): applying these rules to every official world, every door of the first world, the hubs and the missions leads to a world file that exists, and every hub and mission has a door leading to it — 59 worlds of the first game and 96 of the second, no exception.

## Reachability

Basis of World Doctor's `REACH-CUT-OFF` (`src/world/reachability.cpp`), which is an estimate.

| Observation | Where | How |
| --- | --- | --- |
| The games call a tile a block when all sixteen of its collision quarters are set | `src/decblupi.cpp:4171-4179` (`IsBlocIcon`); table `src/decblock.cpp:480-600`, `games/sb2/src/decblock.cpp:408-630` | read (test), scripted (table) |
| By that test the blocks are tiles 0–14, 23–27, 30–44, 48–67, 78–84, 96–107, 139–153, 156–157, 186–197 and 209–234 in both games, and in the second also 246–250, 254–255, 257–258, 261–263, 283–284, 301, 304, 338, 340–363, 373–374 and 386–397 | the same tables | scripted |
| The second game has teleporters (tiles 330–333) and doors opened by keys (334–336) | `games/sb2/src/decblupi.cpp:5032-5044`, `:5021` | read |

The estimate connects cells that hold no block, horizontally and vertically, starting from the four starts, and reports a treasure (type 5) or goal (type 7) whose cell is not connected. It does not model jumping, falling, vehicles or lifts, so it can only say "cut off", never "reachable"; and the games can change the map while playing (explosions, pushed crates, key doors), which is why it is labelled an estimate and reported as a note. It is not made for a second-game world with teleporters or key doors. Reviewed on the original worlds (2026-11-23, private): without that exception ten second-game worlds are reported, each of which contains teleporters; with it the estimate is made for all 59 worlds of the first game and 72 of 96 of the second and reports nothing.

A model that also limits how far up the player can get was measured and not adopted (2026-11-23, private prototype, original worlds only). The largest upward speed a jump sets is 29 pixels a step, reduced by 2 each step (`src/decblupi.cpp:160-176`, `:249`), which is about 225 pixels, under four cells. A search that lets the player rise four cells from any cell with a tile in or under it — and treats every object's path, with the row above it, as carrying the player — still reports 9 treasures or goals in 7 worlds of the first game and 36 in 4 of the second, all of which can be collected in the games; only a rise of twelve cells reports none. So jumps do not bound where the player gets: helicopters, springs and other things lift it, and each would need its own cited rule. No such rule set could be verified here, and the estimate stays with connectivity.

## Demo recordings

Basis of `src/world/demo_recording.hpp` and the recording window. Identical in both games.

| Observation | Where | How |
| --- | --- | --- |
| A recording is a `DemoHeader` (two shorts, major and minor revision, and `short reserve[100]`: 204 bytes) followed by `DemoEvent` records of two shorts, `time` and `input` | `src/event.h:34-47`; `games/sb2/src/event.h:34-47` | read |
| The games write the header zeroed with revision 1.0, then the events recorded | `src/event.cpp:3697-3701`; `games/sb2/src/event.cpp:4346-4350` | read |
| The buffer holds 10,000 events: recording stops when it is full and playback reads no more | `src/def.h:33`; `src/event.cpp:3734`, `:3834`; `games/sb2/src/def.h:31`; `games/sb2/src/event.cpp:4383`, `:4482` | read |
| `input` is the set of keys held: left 1, right 2, up 4, down 8, jump 16, fire 32 | `src/def.h:741-747`; `games/sb2/src/def.h:739-745` | read |
| An event is written for each key press or release message the game handles, with the step counter of that moment; the counter advances once per game step. Consecutive events with unchanged keys occur in the original recordings | `src/event.cpp:4548-4549`, `:3827-3838`, `:3824`; `src/blupi.cpp:215-220` | read; measured (repeats) |
| Playback applies each event once its time is not after the step counter, and moves on to the next recording when the last event has been applied — so the last event's time is the length | `src/event.cpp:3799-3822`; `games/sb2/src/event.cpp:4447-4470` | read |
| Recording `demoNNN.blp` is replayed on official world NNN; the first is 200 (first game) or 300 (second) | `src/event.cpp:3723-3742`; `games/sb2/src/event.cpp:4372-4391` | read |
| The games record while a world numbered 200–219 (first game) or 300–319 (second) is played | `src/event.cpp:2094-2103`; `games/sb2/src/event.cpp:2715-2717` | read |
| One game step per timer tick at normal speed; the timer's default interval is 50 ms and a configuration line can change it | `src/blupi.cpp:60`, `:162-164`, `:215-220`; `games/sb2/src/blupi.cpp:63`, `:168-170` | read |
| All 17 recordings of the two installations checked (7 and 10 files) have revision 1.0, a zero reserved header, whole events, times that never run backwards and keys within the six | local survey, 2026-11-23 | measured |

The files hold no positions. Turning keys into a path would mean re-running the games' movement code, which the editor does not contain; nothing of the kind is shown. The duration in seconds assumes the default 50 ms step and is labelled "about".

## Mobile Eggbert text worlds

Basis of `src/world/text_world.cpp`. Source: `mobile-eggbert` at `dd0a592d878c7b678934945fb0e8ef8f76b63800`, read only; nothing was copied from it and its world files are not used as fixtures.

| Observation | Where | How |
| --- | --- | --- |
| A world is the text file `worlds/world%03d.txt`; it is read whole and split at line feeds | `src/WindowsPhoneSpeedyBlupi/Worlds.cpp:146`, `:181`, `:184-193` | read |
| A section is a line beginning with its name and a colon; the n-th such line is the n-th record (used for `MoveObject`) | `Worlds.cpp:393-398`, `:497` | read |
| A field's value is the text between `name=` and the next space; without that space the field is not read | `Worlds.cpp:386-418` | read |
| A point field is `name=x;y` followed by a space | `Worlds.cpp:486-540` | read |
| A tile of `Decor` (and `BigDecor`) at column x, row y is entry y of the comma-separated line x + 1 lines below the section line; an empty entry is -1 | `Worlds.cpp:543-575` | read |
| A world's objects are read from the first 200 `MoveObject:` lines in order, line n into object n, stopping at the first line whose `type` is 0; the thirteen fields read are `type`, `stepAdvance`, `stepRecede`, `timeStopStart`, `timeStopEnd`, `posStart`, `posEnd`, `posCurrent`, `step`, `time`, `phase`, `channel`, `icon` | `src/WindowsPhoneSpeedyBlupi/Decor.cpp:11342-11370`; `include/WindowsPhoneSpeedyBlupi/Decor.hpp:185` | read |
| A number field that is missing or not a number reads as 0 | `Worlds.cpp:443-463` | read |

Checked against that repository's 78 world files (2026-11-23, private, read only): each consists of one `DescFile:` line, a `Decor:` and a `BigDecor:` block of 100 lines and 3,560 `MoveObject:` lines in total; every file is written back byte for byte by the codec and none has a shape problem.

## New world

| Observation | Where | How |
| --- | --- | --- |
| Both games' editors start a new mission from the same state: scroll position (0, 0), `dimDecor` (100, 100), music 1, region 2, empty title; both layers filled with `0xFFFF` except foreground (3, 4) = 40, (4, 4) = 38, (5, 4) = 39; every object type 0, then slot 0 a treasure (type 5, steps 1/1, waits 0/0, start = end = current = (258, 196), step 1, time 0, phase 0, channel 10, icon 0) and slot 1 the goal (type 7, the same fields with x = 322, icon 29); the four starts at (194, 198) facing right (2) | `src/decblock.cpp:863-921`; `games/sb2/src/decblock.cpp:1227-1288`; constants `src/def.h:699`, `:727`, `:732`, `:999`, `:1001` and the same names in `games/sb2/src/def.h` | read |
| The writer zero-fills the header before setting the fields above and writes both layers and all 200 records | `src/decor.cpp:297-357` | read |

`newWorld` writes that state with every byte the games do not set as zero (the games write whatever the unused fields of slots 2–199 held in memory). The revision is the one the game's stock worlds use: 1.2 for the first game (all 59 stock worlds of Speedy Eggbert 1; the reconstructed writer writes 1.3 except in its June 1998 demo build, and both readers accept either), 1.3 for the second.

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
| In design mode the games draw tile-sheet picture 30 at the start and 31 at the end of a moving object's path, after the object (they are yellow frames holding the digits 1 and 2), for lifts, moving bombs, bulldozers, fish, birds and the helicopter and tank (the second game adds its other lifts, wasps and creatures) | `src/decblock.cpp:1424-1447`; `games/sb2/src/decblock.cpp:1871-1896` | read; pictures looked at (first game) |

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
