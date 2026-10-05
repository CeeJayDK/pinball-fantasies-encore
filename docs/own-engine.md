# The engine of our own

The engine on `main` (`src/assets`, `src/table`, `src/intro`, `src/sound`, and the tables that
`tools/gen_game_tables.py` generates) is a translation of another project's. On the branch
`own-engine` it is being written again from the original game's programs, to replace it whole.

## The rule

New code is written from the disassembly of the game's own files (`re/fantasy`, made by
`python3 re/disasm.py <the game's folder>`; it stays on the machine, like everything else made
from the game's files) and from our own notes. Every address, constant and rule in it says
where in the original it comes from. The engine being replaced is not read while writing: it is
only *run*, to compare what the two do with the same recorded game.

## The order

1. **Foundation.** The branch, the disassembly of the supported files.
2. **Comparing.** A tool that plays one recording through both engines and reports the first
   frame at which the ball, the score, the lights or the display differ.
3. **The table's data**, read at addresses found in the disassembly (starting from the loaders
   in `re/legacy-engine`, which were ours and were written for another release's layout).
4. **The ball**: physics, flippers, plunger, nudge and tilt, compared frame by frame.
5. **What the tables share**: the script interpreter, tasks, lights, display, sounds, scores,
   bonus, match, high scores.
6. **The four tables' rules**, one table at a time.
7. **Intro, menu and music.**
8. **Changing over**: every recording kept on the server played through the new engine; then the
   old engine, its generator and every mention of it removed.

A difference found in step 2 is settled by the disassembly, not by either engine.

## The referee

`encore-oracle` (tools/oracle) runs the game's own table programs in a small machine of ours:
an 80186, the video card's planes and registers, the DOS and launcher calls the tables make,
and the silent sound driver (NOSOUND.SDR) done by hand from its listing. It is a test tool and
is never part of the game. What it does with the same keys is what the new engine must do.

## How the tables' logic is written

The original keeps everything a table knows in one data segment, and its scripts, lists and
tables are data in that segment too. The new engine keeps the same memory: a table starts from
the bytes of the program's data segment, and each routine of the original is written again by
hand as a C++ function that reads and writes it. That has three consequences:

- **Nothing is transcribed.** Scripts, trigger lists, light tables and starting values are read
  from the player's own files, where the original reads them.
- **Everything can be checked.** After every frame the engine's memory is compared with the
  referee's, byte for byte; a difference names the variable and the frame.
- **One engine, four tables.** The four programs hold the same engine at different addresses.
  Names (`re/symbols`) are given to Party Land's addresses as they are understood, and
  `re/align.py` carries them to the other three.

Drawing is the exception: the original draws through the video card's registers, and the engine
draws from what the table knows instead (which lights are lit, where the ball and flippers
are, the display's dots), as the HD pictures need anyway. The ball's physics (`src/engine/sim`)
is also written on its own, and checked against the referee ball for ball.

## Where it stands

All four tables' rules are written (`src/engine/table`: `PartyLand`, `SpeedDevils`, `Gameshow`,
`StonesNBones`, on the `Flow` they share and the `Engine` under it) and are the same as the
original, frame for frame, from the engine's own start-up:

```bash
build/encore-oracle-table <the game's folder> <table 1-4> 100000 --all --start --wild --rough --flip <seed>
```

`--flip` plays a game at random; `--wild` adds nudges, pause, typed letters and more players;
`--rough` throws the ball into the table's own trigger zones, shakes the table until it tilts,
switches lights the rules remember, and starts the best scores at nought so that initials are
asked for. All of it is done alike to the original and to ours. `ENCORE_COVER=1` lists the
routines of ours a run never reached.

One thing the original does is not followed past the point where it goes wrong: while more
players may still join, the start keys also let the number of players be put *below* the player
whose turn it is. The original then counts players on past the eighth and reads and writes past
their records, and what follows is whatever was in that memory. The test stops there. The
game will refuse that key instead.

The game now plays its tables on it. `TableGame` (`src/engine/game`) joins a table with the
sound driver written from SB16.SDR (`src/engine/audio/MusicDriver`) and with its picture
(`src/engine/view/TableScreen`, the same as the original's dot for dot), takes keys, and adds
what this version has that the original has not: the options changed while paused, the steeper
angle, the whole table on one screen, what the high-resolution pictures need, and the question
whether a best score goes online. A game is recorded as its start and its keys
(`Recording`, format 4) and plays again to the same end:

```bash
build/encore-play <the game's folder> <table 1-4> 60000 <seed>     # games by keys at random, twice, and from their recording
build/encore-play <the game's folder> --verify <file.RPL>          # as the server checks one
```

Two things are not as the original has them, on purpose. Its source of chance is a count of
the turns of its own loop, which depends on the machine: here it goes on evenly, eight turns a
frame, from a number each game is given. And the screen mode a table is started in decides its
speeds, as in the original, but the picture can be changed to another size while it plays: the
speeds then stay as they were until the next game.

Still to do: the intro and the menu, which are still the old engine's (from INTRO.PRG:
`re/fantasy/INTRO_seg364c.lst`); recordings made before this engine (format 3), which it
cannot play; and then taking the old engine out, with every mention of it.
