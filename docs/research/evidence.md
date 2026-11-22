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
| Writer zero-fills the header, writes revision 1.3 (1.2 and `reserve1[0] = 1` in the June 1998 demo build), then both layers and all 200 records | `src/decor.cpp:297-357` | read |
| Stock SB1 worlds are revision 1.2 with an all-zero title; stock SB2 worlds are 1.3; demo and Valueware editions use a non-zero `reserve1[0]` (256 in shipped demo worlds); `world` is 0 in all measured files | generator `docs/formats/BLP_SB1_WORLD.md` | reported |
| An empty cell is stored as -1 | `src/decblock.cpp:878-879` (cited by research notes); generator format notes | reported |

## Geometry and header fields (both games unless noted)

| Observation | Where | How |
| --- | --- | --- |
| Cells are 64 × 64 world pixels, the map is 100 × 100 cells, the view is 640 × 480 | `src/def.h:38-39`, `:51-52`, `:57-58`; `games/sb2/src/def.h:36-37`, `:49-50`, `:55-56` | read |
| `DIR_LEFT` is 1, `DIR_RIGHT` is 2 | `src/def.h:726-727`; `games/sb2/src/def.h:724-725` | read |
| The in-game editor places a start at `cell * 64 + (2, 6)`; clicking the cell that already holds it toggles left/right; Ctrl copies start 1 to starts 2–4 | `src/decdesign.cpp:762-783` | read |
| Music choices are 0–10 (11 buttons, value `message - WM_BUTTON1`); 0 plays nothing | `src/event.cpp:416-426`, `:3086-3090` | read (meaning of 0: reported) |
| Regions are 0–23 in SB1 and 0–31 in SB2 | `src/event.cpp:3093`; `games/sb2/src/event.cpp:3679` | read |
| `dimDecor` is only tested for zero/non-zero per axis; the editor writes 100 or 0 | research notes citing `src/event.cpp:3098-3130`, `src/decblupi.cpp:3707-3725` | reported |
| `posDecor` is the saved scroll position and is recomputed during play; `world` is never read or written | research notes citing `src/decor.cpp:324`, `:388`, `src/decblupi.cpp:3685-3688` | reported |
| Title entry accepts bytes 0x20–0xFF while the length is below 40; storage is `char name[100]` copied with `strcpy` | `src/event.cpp:3976-3990`; `src/decor.h:112` | read |
| Tiles with collision data: SB1 0–236 (`MAXQUART` 237), SB2 0–440 (`MAXQUART` 441) | `src/decor.h:13`; `games/sb2/src/decor.h:13` | read |
| SB2's loader rewrites stored foreground values 48–67 to 215 in memory; SB1 has no such step | `games/sb2/src/decor.cpp:675-684` | read |
