# Pinball Fantasies Remastered

A native macOS version of *Pinball Fantasies* (Digital Illusions / 21st Century Entertainment,
1994 PC release) in C++20 on SDL3 and OpenGL.

The engine is a C++ translation of [wanda-phi/pfr](https://github.com/wanda-phi/pfr), a
faithful recreation of the DOS game in Rust, whose authors gave permission to use their
work. Physics, table rules, scripts, the dot matrix, music sequencing, the intro and the menu
all follow pfr function by function; the platform layer, renderer and shader pipeline are
this project's own.

## Game files

Nothing from the original is included, apart from the redrawn intro and menu pictures in
`assets/hd`. The game reads the pictures, collision maps, scripts, music and effects from your
own copy of the DOS files, and it needs **exactly** the release pfr supports, because it reads
the data at fixed addresses. At start-up every file is checked
against these SHA-256 sums, and a folder holding any other release is refused:

```
619723e39acc003c64ae5f10159ae9da6192a28642c348f455bac447a1184967  INTRO.PRG
3b897533f11163934b8e4da038143e8f7339224421803ab4108c9d10f0a7bb4a  TABLE1.PRG
37019f7bd41d896a8f5a6383a2dffc3b3e4fc63fdf1aec1cc15db99110e581eb  TABLE2.PRG
da83ef5a7a471e6a6ad759126907076c81e92ffde6dec8e3de8e6052c6a98858  TABLE3.PRG
88f63edd4c7b50bd057397016d7aa962f0ed1c858f4a746f1ccf976f67494ebf  TABLE4.PRG
aa5003c275b494062f37f44e8c77105b8a420555f4bd6ff53d7698f89c540f21  MOD2.MOD
a0877e4372abe64b70d9e361bf257ea5a84c948771f0eace3433d5f6399060b5  TABLE1.MOD
728629c54311386781271308e181ac0435f0582e90870accff0a42270d467529  TABLE2.MOD
fb7bfd1c96a462cb03999d2e6f843a20d3de69ba05fcbd384a9f1c131b9a563a  TABLE3.MOD
31ad7e671ae77c07c3d075e2f1fecd3d918fd921fa23acd9a1b0b6fc07fbbcea  TABLE4.MOD
```

`INTRO.MOD` is also needed but not checked: the DOS game rewrites it as part of its copy
protection. The supported release is the original disk version, archived at
<https://archive.org/details/000323-PinballFantasies>. Re-releases differ; a cracked copy in
particular has a different `INTRO.PRG`, `TABLE1.PRG` and `TABLE2.PRG`.

The app finds the files by itself when they are in a folder near it (the folder given with
`--data`, the one used last time, the working folder, the folders around the application, and
their subfolders), and otherwise asks for the folder. Options and high scores are kept in the
DOS formats (`PINBALL.CFG`, `TABLEn.HI`) under `~/Library/Application Support/pfr/Pinball
Fantasies/`; the first time, they are imported from the game folder, which is never written to.

## Building

Requirements: Xcode command-line tools, CMake 3.24 or later, SDL3 (`brew install sdl3`).

```bash
cmake -S . -B build
cmake --build build -j
./build/pfr-tests
open "build/Pinball Fantasies.app"
```

## Controls

The original layout.

| Key | Action |
| --- | --- |
| F1 to F4 | choose a table in the menu |
| F5 | options (in the menu) |
| Enter | start a game; again, before plunging, to add a player (or F1 to F8 for 1 to 8 players) |
| Left and right Shift, Ctrl or Alt | flippers |
| Down arrow | pull the plunger, release to shoot |
| Space | nudge the table (too often tilts it) |
| P | pause; while paused, F7 cycles every lamp on, off and back to normal, and the arrows scroll the table (for checking artwork); A angle (low, high, or higher: a steeper table with stronger flippers), S scrolling, M music, R resolution |
| M | music on or off |
| F9 | CRT look on or off |
| F10 | high-resolution pictures in the intro and menu, or the originals |
| Escape | with the ball at the plunger, abandon the game; in attract mode, leave the table (Y to confirm); in the menu, quit |
| Command+F | fullscreen |

## Command line

| Option | Effect |
| --- | --- |
| `--data <dir>` | folder with the game files |
| `--table <1-4>` | open a table directly |
| `--skip-intro` | go straight to the table menu |
| `--res normal\|high\|full` | screen mode: 320x240, 320x350, or the whole table at once |
| `--crt`, `--no-crt` | CRT look (scanlines, shadow mask, glow); remembered |
| `--hd`, `--no-hd` | high-resolution pictures in the intro and menu, or the originals; remembered |
| `--hd-dir <dir>` | pictures to use instead of the application's own |
| `--smooth` | soften the one pixel that straddles two source pixels; steadies scrolling |
| `--square-pixels` | show the picture unstretched instead of filling a 4:3 screen |
| `--fullscreen`, `--scale <n>` | window options |
| `--screenshot <file>`, `--screenshot-frame <n>` | render one frame to a PNG and quit |
| `--stats` | log, once a second, how long each frame takes |

## Tools

| Tool | Purpose |
| --- | --- |
| `pfr-play <dir> <table> [frames] [seed] [out.png]` | plays a game headlessly with a simple autopilot and prints the ball, the score, every trigger and (with `PFR_DM=1`) the dot matrix |
| `pfr-assets <dir>` | loads every table and prints what was extracted |
| `tools/hd_import.py <name>=<picture> ...` | prepares redrawn intro and menu pictures (trims, resizes to 3x the original) into `assets/hd` |
| `tools/gen_pfr_tables.py <pfr> <out>` | regenerates `src/assets/PfrTables.inc`, the lookup tables copied mechanically from pfr's source |

## Layout

| Path | Purpose |
| --- | --- |
| `src/assets` | Reading a table executable: artwork, collision maps, gates, triggers, lights, sounds, fonts and the table script (pfr's `assets/table`) |
| `src/table` | A table in play: physics, script interpreter, tasks, game flow, and one file per table's rules (pfr's `table`) |
| `src/intro` | The slideshow and the table menu with its text, high-score and options pages (pfr's `intro`) |
| `src/sound` | The four-channel module player and the jingle sequencer (pfr's `sound`) |
| `src/game` | Application shell and the options and high-score files |
| `assets/hd` | Redrawn, high-resolution versions of the intro's slides, the menu's side panel, table banners and high-score heading, drawn in place of the originals |
| `src/gfx`, `shaders` | Indexed framebuffer, palette, OpenGL renderer; `post.frag` is the hook for CRT-style effects and is hot-reloaded |
| `src/platform` | SDL3 window, audio device, finding the game files |
| `src/core`, `src/data` | Types, files, PNG and SHA-256, IFF pictures, the game-version check |
| `docs` | Notes from the reverse-engineering work |
| `re/legacy-engine` | The earlier engine this translation replaced, kept for reference; not built |
