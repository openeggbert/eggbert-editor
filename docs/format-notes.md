# World file format notes

These notes describe the layout Eggbert Editor reads and writes. Evidence and source revisions are in `docs/research/evidence.md` in the source tree.

All numbers are little-endian. A POINT is two signed 32-bit values, x then y.

## Header: 868 bytes

| Offset | Size | Field | Meaning |
| ---: | ---: | --- | --- |
| 0 | 2 | `majRev` | revision, major part |
| 2 | 2 | `minRev` | revision, minor part |
| 4 | 200 | `reserve1` | 50 × 32 bits, preserved; demo editions use the first value as a marker |
| 204 | 8 | `posDecor` | saved scroll position in pixels |
| 212 | 8 | `dimDecor` | per axis: 0 = one screen, anything else = scrolls |
| 220 | 2 | `world` | not read by the games as far as their reconstructed source shows; 0 in every original world checked; preserved |
| 222 | 2 | `music` | 0 = none, 1–10 |
| 224 | 2 | `region` | background picture number |
| 226 | 102 | `reserve2` | 51 × 16 bits, preserved |
| 328 | 32 | `blupiPos` | four start positions (POINT, pixels) |
| 360 | 16 | `blupiDir` | four start directions: 1 = left, 2 = right |
| 376 | 100 | `name` | title, NUL-terminated single-byte text |
| 476 | 392 | `reserve3` | 196 × 16 bits, preserved |

## Sections after the header

| Revision | Foreground | bigDecor | Objects | Total |
| --- | --- | --- | --- | ---: |
| 1.0 | 868, 20,000 bytes | absent | 20,868, 100 × 48 bytes | 25,668 |
| 1.1 | 868, 20,000 bytes | 20,868, 20,000 bytes | 40,868, 100 × 48 bytes | 45,668 |
| 1.2, 1.3 | 868, 20,000 bytes | 20,868, 20,000 bytes | 40,868, 200 × 48 bytes | 50,468 |

A layer is 100 × 100 signed 16-bit tile numbers stored column by column: cell (x, y) is at `layer start + 2 × (100 × x + y)`. -1 is an empty cell. A cell is 64 × 64 pixels; a bigDecor tile covers two by two cells from its cell.

## Object record: 48 bytes

| Offset | Size | Field |
| ---: | ---: | --- |
| 0 | 2 | `type` (0 = free slot) |
| 2 | 2 | `stepAdvance` (ticks from start to end) |
| 4 | 2 | `stepRecede` (ticks from end to start) |
| 6 | 2 | `timeStopStart` (wait at start) |
| 8 | 2 | `timeStopEnd` (wait at end) |
| 10 | 2 | `unknown1` (preserved) |
| 12 | 8 | `posStart` (POINT, pixels) |
| 20 | 8 | `posEnd` |
| 28 | 8 | `posCurrent` |
| 36 | 2 | `step` (run-time state) |
| 38 | 2 | `time` (run-time state) |
| 40 | 2 | `phase` (run-time state) |
| 42 | 2 | `channel` (sprite sheet) |
| 44 | 2 | `icon` (sprite number) |
| 46 | 2 | `unknown2` (preserved) |

The file does not identify the game it belongs to.
