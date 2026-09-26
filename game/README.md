# Put your game files here

Copy the original *Pinball Fantasies* files into this folder and the game finds them by
itself, with no folder to choose and nothing pointed at an existing installation:

```
INTRO.PRG   TABLE1.PRG   TABLE2.PRG   TABLE3.PRG   TABLE4.PRG
INTRO.MOD   TABLE1.MOD   TABLE2.MOD   TABLE3.MOD   TABLE4.MOD   MOD2.MOD
```

Copies, not the files themselves: the folder is only ever read, but a copy of your own is
one less thing to lose. The release has to be the original disk version -- every file is
checked against the SHA-256 sums in the main README, and anything else is refused.

The files are not part of this repository and are not distributed with it; this folder is
ignored by git apart from these instructions.
