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
