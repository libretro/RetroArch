# Real-game proofs

The scenarios here run real cores, games and saves in the link harness.
Nothing in this folder is a ROM, a save or a core, and no scenario names a
file on any machine: every name a scenario uses is bound on the command
line. So they are not part of `make check`; each one is run with
`make proof`.

## Running a proof

1. Extract each ROM the proof names from your own copy, to a file of its
   own. The section for its system names the dump and its SHA-1.
2. Build the core with link support, and have ready any save the proof
   binds. The harness reads cores, ROMs and saves and never writes them:
   each console's save RAM is filled from a copy in its output folder.
3. From the repository root, bind each name the scenario uses, core, ROM
   and save alike, to an absolute path:

   ```sh
   make -C samples/link/harness proof \
      PROOF=$PWD/samples/link/proofs/<system>/<scenario>.scn \
      BIND='<name>=/abs/path/core_libretro.so <name>=/abs/path/game.rom'
   ```

   `PROOF` is an absolute path. Each `BIND` word, `NAME=FILE` with no
   spaces in it, binds one name.

`make proof` builds the harness, then runs the scenario six times, as
`make check` runs each scenario in `../scenarios/` (see `../README.md`), and
stops at the first run that fails. The runs go to `proof/<scenario>/<run>/`
under `BUILD_DIR`, which is `samples/link/harness/build` unless you set it.

The six runs show:

- `first`: the scenario reaches its `end` line, every `expect` line holds,
  every bus count is the one the scenario expects and no core logs an error;
- `again`: a second run gives the same trace, frame by frame, so nothing
  outside the scenario, such as the host's clock, changes it;
- `reversed` and `shuffled`: the same trace when the consoles run in
  another order, so what they exchange does not depend on which runs first;
- `from`: a new process loads the state saved at the scenario's `save mid`
  line and continues the same trace, so a state holds the link as well as
  the game;
- `rewound`: the session that has run to the scenario's last frame loads
  that state, each core in place, and gives the same trace again from the
  `save mid` line, so, for a load at that frame, nothing of the session it
  replaces reaches the trace.

Right after the load, before any core runs, the `from` and `rewound` runs
save every core's state again. It must be the state the core loaded, and
saving it must not call `send_msg`, `recv_msg`, `peek`, `advance` or
`advance_multi` (`attach`, `detach` and `peers` on its own handle are not
checked): a person can load a state and save another while paused, and a
core whose load leaves something stale that only a save reads shows it
there.

The trace has, for each console and frame, a hash of its picture, its audio
and its save state, and each run's `messages.log`, the messages sent, must
match the first run's too. Each `checkpoint` line leaves `c<n>.png`, the
picture of each console, for a person to look at.

## Game Boy

The Game Boy proofs need Gambatte with support for the link interface,
which is not yet merged upstream. They run it built with link support
(`HAVE_NETWORK`, on by default on desktop platforms), with Game Link Mode
set to Link Cable on every console.

`gb/tetris.scn` binds `gambatte`, the core, and `tetris`, Tetris (World)
(Rev 1), whose `.gb` has the SHA-1
74591cc9501af93873f9a5d3eb12da12c0723bbc. Extract it from your copy of
the No-Intro zip; `bsdtar` also reads zips compressed with zstd, which
`unzip` may not:

```sh
bsdtar -xOf 'Tetris (World) (Rev 1).zip' 'Tetris (World) (Rev 1).gb' > tetris.gb
```

Two consoles play Tetris against each other over the cable. Both choose
2PLAYER on the title screen, console 0 presses START first and starts the
game, and then each moves its own pieces. `menu` shows both on the
two-player menu, `in_flight` the start of the game with messages in flight
on the cable, and `game` both fields in play. After `game` the cable is
pulled and plugged back in, then console 1 is reset.

```sh
make -C samples/link/harness proof \
   PROOF=$PWD/samples/link/proofs/gb/tetris.scn \
   BIND='gambatte=/abs/path/gambatte_libretro.so tetris=/abs/path/tetris.gb'
```

`gb/tetris_gbc.scn` binds `gambatte` and `tetrisdx`, Tetris DX (World)
(SGB Enhanced) (GB Compatible), whose `.gbc` has the SHA-1
7183bcb54dd35f3a07d8fe63339b768f13b8168d:

```sh
bsdtar -xOf 'Tetris DX (World) (SGB Enhanced) (GB Compatible).zip' \
   'Tetris DX (World) (SGB Enhanced) (GB Compatible).gbc' > tetrisdx.gbc
```

Two Game Boy Colors play Tetris DX against each other over the cable, with
their infrared ports joined as well, so each console has two ports on two
buses and advances both together. Each enters a player name, both choose
2PLAYER and go through the menus, and then each moves its own pieces.
`menu` shows both on the two-player menu, `in_flight` the game with
messages in flight on the cable, and `game` both fields in play. After
`game` the cable is pulled and plugged back in, then the infrared ports
are turned away from each other and back, then console 1 is reset. Tetris DX
sends nothing over the infrared port: the proof shows that port attached, on
its bus and advanced with the cable's.

```sh
make -C samples/link/harness proof \
   PROOF=$PWD/samples/link/proofs/gb/tetris_gbc.scn \
   BIND='gambatte=/abs/path/gambatte_libretro.so tetrisdx=/abs/path/tetrisdx.gbc'
```

`gb/infrared.scn` binds `gambatte` and `smbdx`, Super Mario Bros. Deluxe
(USA, Europe), whose `.gbc` has the SHA-1
f84f26f22751b58ed57bd332274e18131660729a:

```sh
bsdtar -xOf 'Super Mario Bros. Deluxe (USA, Europe).zip' \
   'Super Mario Bros. Deluxe (USA, Europe).gbc' > smbdx.gbc
```

Two Game Boy Colors trade their rankings over the infrared port, with the
cable plugged in as well. Each opens RECORDS and chooses Trade; console 0
presses A and sends its ranking, and console 1 answers with its own: 2920
changes of the LED each way, a message for each. The game runs the
processor at double speed and times the other console's flashes with its
own instructions, so the trade completes only if every change arrives as
far from the last as it was sent. `ready` shows both waiting to trade,
`in_flight` the trade with a change of console 0's LED in flight, `traded`
the merged ranking each console offers to keep, and `kept` the ranking
after both keep it. The `expect` lines read the second place of each
ranking: PEACH before the trade, the other console's LUIGI after it, in
work RAM and then in the save. After `kept` console 1 is reset.

```sh
make -C samples/link/harness proof \
   PROOF=$PWD/samples/link/proofs/gb/infrared.scn \
   BIND='gambatte=/abs/path/gambatte_libretro.so smbdx=/abs/path/smbdx.gbc'
```

`gb/infrared_in_step.scn` binds `gambatte` and `pinball`, Pokemon Pinball
(USA, Australia) (Rumble Version) (SGB Enhanced) (GB Compatible), whose
`.gbc` has the SHA-1 9402014d14969432142abfde728c6f1a10ee4dac:

```sh
bsdtar -xOf 'Pokemon Pinball (USA, Australia) (Rumble Version) (SGB Enhanced) (GB Compatible).zip' \
   'Pokemon Pinball (USA, Australia) (Rumble Version) (SGB Enhanced) (GB Compatible).gbc' > pinball.gbc
```

Two Game Boy Colors exchange their high scores over the infrared port.
Both take the same path to the high scores and choose SEND, console 0
thirty frames before console 1, so the two stay in step, frame for frame,
and each takes its interrupts at the moment the other does. Each calls out
until the other answers, and the one that answers has to catch the
caller's first flash before it ends, so the exchange completes only if a
flash arrives much less than its own length after it was sent. This game
runs the processor at normal speed, where `infrared.scn`'s runs it at
double speed. `menu` shows both on the SEND menu, `in_flight` the exchange
with a change of console 0's LED in flight, and `done` both back on the
high scores; after calling out with no answer the game shows the menu
again. The `expect` lines read the other console's first place as
received, and the game's own error byte: 0 after an exchange, 2 after no
answer.

```sh
make -C samples/link/harness proof \
   PROOF=$PWD/samples/link/proofs/gb/infrared_in_step.scn \
   BIND='gambatte=/abs/path/gambatte_libretro.so pinball=/abs/path/pinball.gbc'
```
