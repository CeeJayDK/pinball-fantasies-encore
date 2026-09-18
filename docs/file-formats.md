# Original file formats

Everything below was established by reading the 1994 PC binaries. Offsets are given for
`TABLE1.PRG`; the other tables use the same block types in a slightly different order,
which is why `TableData` classifies blocks by size and signature rather than by position.

## Executables (`INTRO.PRG`, `TABLEn.PRG`)

16-bit MZ executables. We never execute them; the relocation table gives every segment value
the code references, and sorting those values yields the exact block map of the file
(`MzImage::segments()`). File offset = header size (512 bytes for the tables, 1024 for
`INTRO.PRG`) + segment × 16.

Block sequence of a table executable:

| Block | Size | Content |
| --- | --- | --- |
| code | ~40 KB | Engine + table logic, DMD strings near the end |
| shared code | 5248 B | Score/decimal helpers, far-called |
| frame table | ~55 KB | Animation / frame directory (see gfx notes) |
| data segment | ~27 KB | Variables with initial values, element tables, flipper records |
| sprite bank | 45–62 KB | Sprite directory + pixel data |
| ball graphics | 24448 B | Ball sprite data and background save area |
| ramp mask | 24192 B | Upper-level region data |
| collision masks | 3 × 23040 B | 320×576 1-bpp: walls, dynamic elements, ramps |
| flipper stacks | 2 × 9760 B + one smaller | Rotated flipper frames as 1-bpp masks, 64 px (8 B/row) or 48 px (6 B/row) wide |
| image strips | 4 × IFF `FORM PBM ` | 320×144 each, top to bottom = the 320×576 playfield |
| occlusion masks | 3 × 23040 B | 320×576 1-bpp, copied to VRAM by the original for ball compositing |
| small images | 1760 B, 230 B | 8-bit pixel data |

## Pictures

Deluxe Paint IFF. `PBM ` forms are chunky 8-bit with ByteRun1 compression and rows padded
to an even width; `ILBM` forms (intro screens) are planar. Every strip carries a 256-colour
`CMAP` and 16 `CRNG` colour-cycling ranges. `TABLE4.PRG` strip 3 has a corrupt height field
(1219); the decoder derives the true height from the compressed body length.

## Masks

Rows of 40 bytes for 320 pixels, most significant bit first, 1 = solid.

## Music and sound effects

ProTracker `M.K.` modules, 4 channels. Sound effects are samples inside each table's
module (e.g. `BUMPER`, `FLIPPERUPP`, `NEWBALL2`, `UPPSKUTARE` in `TABLE1.MOD`); the original
plays them through the music replayer on one of the four channels, and so do we.

## High scores (`TABLEn.HI`)

64 bytes: four entries of 16 bytes. Bytes 0–11 are the twelve decimal digits of the score,
one digit per byte, most significant first; bytes 12–14 are the three-letter name; byte 15
is zero. Defaults: TSP 5,000,000 · ICE 2,500,000 · ANY 1,000,000 · J L 500,000.

## Configuration (`PINBALL.CFG`)

Six bytes written by the original setup program; meaning documented in `engine.md`.
