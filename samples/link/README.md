# Link harness and test core

The link harness runs the consoles of a scenario on one link bus, frame by
frame, and checks that every run of the scenario gives the same result. Each
console loads its own copy of a core. The test core is a small deterministic
machine on the link interface: core options set its ports, clocks, promises
and sends, and it logs an error whenever the bus breaks a promise the header
makes to it. No games, BIOS, display or sound are needed, and nothing reads a
clock: a run goes as fast as it can.

From the repository root, with GCC and GNU Make:

```sh
make -C samples/link/harness check
make -C samples/link/harness check SANITIZER=address,undefined BUILD_DIR=build-sanitized
```

For Windows, built with mingw-w64 and run under Wine:

```sh
make -C samples/link/harness check CC=x86_64-w64-mingw32-gcc RUNNER=wine BUILD_DIR=build-windows
```

Everything built, and every run's output, goes under `BUILD_DIR` (`build` by
default); a run's output is in `out/<scenario>/<run>/`.

## The five runs

`check` runs each scenario in `scenarios/` five times:

- `first`, whose trace the other runs are compared with;
- `again`, the same run again;
- `reversed`, where the scheduler resumes the highest-numbered console that
  can run, not the lowest;
- `shuffled`, in a seeded shuffled order;
- `from`, in a new process from the state the first run saved at its
  `save mid` line.

Each later run must match the first run's trace line for line and write the
same `messages.log` and `stuck.txt`. The `from` run must start at the frame
of the `save mid` line without running that line again. In a scenario that
sets `link_test_restart_ticks` on, the `from` run's `messages.log` is
compared without the stamps and landings, since the load renumbers that
core's ticks; the trace compares them relative to each port's anchor. A
scenario whose save is taken inside an exchange says so with a
`checkpoint in_flight` line right after its `save mid` line, and that
checkpoint's `bus.txt` must list a message in every run.

Then the runs that must fail: every scenario in `scenarios/bad/`, a truncated
state and a state of another scenario must each exit 2, and every scenario in
`scenarios/fail/` and a run against an edited trace must each exit 1. Each
must also print its error: the one a scenario's `# error: <text>` line gives,
with its line number where it has one.

`make proof` makes the same five runs, with the same comparisons, of one
scenario that binds real cores, such as those in `proofs/`. It builds only
the harness, passes each `BIND` word (`NAME=FILE`, no spaces) as a `--path`
and writes the runs to `proof/<scenario>/<run>/` under `BUILD_DIR`.
`proofs/README.md` says how to run those.

```sh
make -C samples/link/harness proof PROOF=/abs/path/scenario.scn \
   BIND='core=/abs/path/core_libretro.so rom=/abs/path/game.rom'
```

## Running the harness

```
link_harness --scenario FILE --out DIR [--path NAME=FILE]...
             [--pick lowest|reversed|shuffled:SEED] [--from STATE]
             [--expect TRACE] [--min-stack BYTES]
```

- `--path NAME=FILE` binds a name the scenario uses for a core, ROM, save or
  system folder. An unbound name is the file of that name beside the
  scenario. `check` binds `test` to the test core it built.
- `--pick` sets the order in which the scheduler resumes consoles that can
  run.
- `--from STATE` starts from a state a `save` line wrote. The state is
  checked against the scenario before anything runs.
- `--expect TRACE` compares the trace with an earlier run's, from this run's
  first frame, and stops at the first difference.
- `--min-stack` gives every console at least this much stack. Sanitizer
  builds use 8 MiB.

The exit code is:

- 0 when the run reached its `end` line, every `expect` held, no core
  logged an error, the bus counts and the frame of any release were those
  the scenario expects, every compared trace line matched, every console
  that ran the frame before a checkpoint kept its picture, and no core, ROM,
  save or state file the run read has changed.
- 1 when any of those failed. The first trace difference is printed as
  `trace differs at frame F console C: expected <line> got <line>`.
- 2 for a usage or input error: an unknown option, a scenario syntax error
  (with its line number), an unbound name, a missing file, or a state that is
  bad or belongs to another scenario. Nothing has run.

## Scenarios

A scenario names its consoles and their options, then says what happens at
which frame. `#` starts a comment.

```
calendar <sec> <utc_offset>         the calendar time the cores start at
system <name>                       the system folder (else an empty out/system/)
console <n> core=<name> [rom=<name>] [save=<name>] [stack=<bytes>]
option <n> <key> <value>            a core option of console n
input <n> <port> <file>             an input script for an input port of console n
expect-count <counter> <value>      a bus count the run ends with
expect-stuck <frame>                the run ends in a release in this frame
<frame> cable <c>:<p> <c>:<p>
<frame> uncable <c>:<p>
<frame> hold <c>
<frame> unhold <c>
<frame> reset <c>
<frame> save <name>                 writes <name>.state
<frame> checkpoint <name>           writes <name>/
<frame> expect <c> save|rtc|system|video <offset> <byte>
<frame> end
```

Consoles are numbered from 0 without gaps. Frame lines come in frame order
and act in file order at the edge before their frame runs, so frame 0's act
once every core is loaded, and an `expect`, `save` or `checkpoint` at frame
F sees the session after F frames. A run from a state starts just after the
`save` line that wrote it: the lines before that line are in the state, and
the lines after it at the same frame still act. A frame has at most one
`save` line. With `expect-stuck F`, no line but `end` comes after frame F,
where the run ends.

A counter not named in an `expect-count` line must end at 0. The counters are
`lowered_promise`, `low_stamp`, `request_past_promise`,
`request_past_frame_end`, `request_past_earliest`, `skipped_publish`,
`left_out` and `misplaced_call`. A membership line the bus refuses, or an
option its core did not declare, fails the run. An input script has a
`<frame> <hex joypad mask>` line for each change, held until the next.

The scenarios here:

- `two_unequal`: two consoles with unequal rates, steps and lookaheads, one
  of them with a save and an input script.
- `capped_step`: two ports on each console, where a step's grant can fall a
  tick short of its end.
- `four_one_bus`: four consoles chained on one bus.
- `ring_three`: three consoles with two ports each, cabled in a ring, and an
  uncabled third port.
- `radio_three`: three radios on a shared medium.
- `membership`: a cable pulled and plugged back in, a core's own detach and
  re-attach, and a hold with messages for the held console.
- `renumber`: a console whose ticks restart at every load and move at every
  reset.
- `repairs`: a core that breaks the rules the bus repairs, and skips a
  frame-end publish.
- `broken_never_advance` and `broken_no_progress`: broken cores, which end
  in a release.
- `save_frame`: lines before and after a save line at its frame.
- `save_then_hold`: a hold after the save line at its frame, so a run from
  the state holds a console it has just loaded.
- `dupe_checkpoint`: a dupe in the frame before a checkpoint, which keeps the
  picture of the frame before that.
- `bad/`: scenarios the harness refuses before anything runs.
- `fail/`: scenarios that run and fail: a core that logs an error, a count
  the bus does not reach, an `expect` line that does not hold, a release the
  scenario does not expect and two dupes in a row before a checkpoint.

## Output

- `trace.txt`, one line per console and frame, then one for the bus:

  ```
  F <frame> C <n> V <hex> A <hex> S <hex>
  F <frame> C <n> HELD
  F <frame> BUS <hex>
  ```

  V, A and S are the xxh64 of the picture rows, the audio and the
  `retro_serialize` output; BUS hashes the bus contents with every tick
  taken from its port's anchor, so a core that renumbers its ticks on a load
  still hashes the same.
- `messages.log`, a line for each message sent, by frame, receiving port,
  sending port and send order:
  `F <frame> <c>:<p> -> <c>:<p> stamp <hex> landing <hex> len <n> head <hex>`.
- `<name>.state` for each `save` line: the frame, a hash of the scenario
  file, each core's state and the bus contents.
- `<name>/` for each `checkpoint`: `c<n>.png`, each console's picture of the
  frame before, for each console that ran it; `bus.txt`, the bus contents;
  `counts.txt`, the bus counts.
- `stuck.txt` after a release: each console that was parked, with the port
  and peer that bound it, or `no progress` for one parked for advancing
  without progress.
- `c<n>/`, the console's copy of its core, and `c<n>/save/`, its save
  folder with a copy of its save. Cores, ROMs and saves are read, never
  written.

## Test core options

The test core is built as `link_test_libretro.so`, or `.dll` on Windows.
`P` is a port number, 0 to 2.

| Option | Values | Default |
|---|---|---|
| `link_test_ports` | 1-3 | 1 |
| `link_test_rate_P` | ticks a second, a multiple of 60 | 61440 |
| `link_test_steps` | steps a frame, 1-64 | 16 |
| `link_test_lookahead_P` | steps the promise runs ahead, 0-64 | 1 |
| `link_test_protocol_P` | protocol id | test-cable |
| `link_test_max_ports_P` | 0-255 | 2 |
| `link_test_shared_P` | off, on | off |
| `link_test_send_P` | none, broadcast, or an index 0-254 | broadcast |
| `link_test_seed` | a number | 1 |
| `link_test_send_frames` | `A-B[,A-B]...`, the frames that may send | 0-999999 |
| `link_test_dupe_frames` | `A-B[,A-B]...`, the frames that pass no picture (dupes); set, the core needs `GET_CAN_DUPE` | empty |
| `link_test_detach_P`, `link_test_reattach_P` | the frame in whose `retro_run` it happens, or -1 | -1 |
| `link_test_restart_ticks` | off, on | off |
| `link_test_mode` | conform, nonconform, never_advance, no_progress | conform |
| `link_test_bad_frame` | the first frame of the mode | 0 |

In each `retro_run` the core advances its ports at the frame start, at each
of its steps and at the frame end. It takes every message it can, applies
each one once it has passed the message's landing tick, and on seeded
events sends a payload made from its state, stamped at its promise. With
`link_test_restart_ticks` on, its ticks start from a new number after every
load and move at every reset, while its saved state stays the same.

From `link_test_bad_frame` on, the modes break the rules:

- `nonconform`: in every frame it lowers its promise, stamps a message below
  it, asks one step past it and asks past its frame end, each once and in a
  call of its own. In the fourth frame of the mode it makes no frame-end
  publish. It starts at frame 1 or later.
- `never_advance`: it makes no advance at all.
- `no_progress`: in that frame alone, it advances again and again without
  progress until the bus is released.

Its system RAM, for `expect` lines: 0x00-0x07 the state hash; 0x08 the ports
attached; 0x09-0x0B each port's peer count; 0x0C-0x0E each port's index, 255
when it is cabled to nothing; 0x0F the errors it logged; 0x10-0x13 the
messages received and 0x14-0x17 those sent, little-endian; 0x18 and 0x19 the
topology and detached reports it was given.
