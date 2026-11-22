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
