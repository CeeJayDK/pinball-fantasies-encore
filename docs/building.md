# Building, options and layout

## Building

The game needs CMake 3.24 or later, a C++20 compiler, SDL3 and OpenGL 4.1. Nothing else: the
pictures are decoded by this project's own PNG reader, and every OpenGL function past 1.1 is
asked of the driver through SDL, so there is no image library and no loader to install.

**macOS** (Xcode command-line tools, `brew install sdl3`; for a build that runs on both Apple
Silicon and Intel, point `-DENCORE_SDL3_FRAMEWORK=` at the official universal `SDL3.framework`
and set `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`):

```bash
cmake -S . -B build
cmake --build build -j
./build/encore-tests
open "build/Pinball Fantasies.app"
```

**Linux** (SDL3 from your distribution, or built from source, plus `libgl1-mesa-dev`):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j
"./build/Pinball Fantasies" --data /path/to/FANTASY
```

**Windows** (Visual Studio 2022, and the SDL3 development files unpacked somewhere):

```bat
cmake -S . -B build -A x64 -DCMAKE_PREFIX_PATH=C:\SDL3-3.4.16\cmake
cmake --build build --config RelWithDebInfo
```

On macOS the shaders and pictures go inside the application bundle; elsewhere they are copied
next to the executable, which is where the game looks for them. Settings and high scores live
in the folder SDL keeps for the platform (`~/Library/Application Support`, `~/.local/share`,
`%APPDATA%`).

## Command line

| Option | Effect |
| --- | --- |
| `--data <dir>` | folder with the game files |
| `--table <1-4>` | open a table directly |
| `--skip-intro` | go straight to the table menu |
| `--res normal\|high\|full` | screen mode: 320x240, 320x350, or the whole table at once |
| `--crt`, `--no-crt` | CRT look (scanlines, shadow mask, glow); remembered |
| `--hd`, `--no-hd` | the remastered pictures, or the originals; remembered |
| `--trail`, `--no-trail` | the fading ghosts behind the ball; remembered |
| `--hd-dir <dir>` | pictures to use instead of the application's own |
| `--smooth` | soften the one pixel that straddles two source pixels; steadies scrolling |
| `--square-pixels` | show the picture unstretched instead of filling a 4:3 screen |
| `--fullscreen`, `--scale <n>` | window options |
| `--screenshot <file>`, `--screenshot-frame <n>` | render one frame to a PNG and quit |
| `--stats` | log, once a second, how long each frame takes |
| `--verbose` | log every step, not only what matters |
| `--help` | the options, and what they do |

## Tools

| Tool | Purpose |
| --- | --- |
| `encore-play <dir> <table> [frames] [seed] [out.png]` | plays a game headlessly with a simple autopilot and prints the ball, the score, every trigger and (with `ENCORE_DM=1`) the dot matrix |
| `encore-assets <dir>` | loads every table and prints what was extracted, including each flipper's rectangle, hinge and sweep |
| `encore-extract <dir> <out>` | writes every table's artwork, collision maps, flipper frames, ball and plunger out as PNGs |
| `tools/hd_import.py <name>=<picture> ...` | prepares redrawn pictures (trims, resizes to 3x the original) into `assets/hd` |
| `tools/hd_unlit.py <lit.png> <unlit.png> <table dir>` | derives a playfield's lights-off picture from its lights-on one, using the original's lamps (needs `encore-extract` output) |

## Layout

| Path | Purpose |
| --- | --- |
| `src/assets` | Reading a table executable: artwork, collision maps, gates, triggers, lights, sounds, fonts and the table script |
| `src/table` | A table in play: physics, script interpreter, tasks, game flow, and one file per table's rules |
| `src/intro` | The slideshow and the table menu with its text, high-score and options pages |
| `src/sound` | The four-channel module player and the jingle sequencer |
| `src/game` | Application shell and the options and high-score files |
| `assets/hd` | Redrawn, high-resolution pictures drawn in place of the originals: the intro's slides, the menu's side panel, table banners and high-score heading, each table's playfield lit and unlit, the flippers and the ball |
| `assets/app` | The application icon, built into `icon.icns` at build time |
| `src/gfx`, `shaders` | Indexed framebuffer, palette, OpenGL renderer; `post.frag` is the hook for CRT-style effects and is hot-reloaded |
| `src/platform` | SDL3 window, audio device, finding the game files, fetching over HTTP |
| `src/core`, `src/data` | Types, files, PNG, deflate and zip, SHA-256, IFF pictures, the game-version check |
| `tests` | Pure-logic tests, and tests that play full games when the game files are present |
| `docs` | Notes from the reverse-engineering work |
| `re/legacy-engine` | The earlier engine this translation replaced, kept for reference; not built |
