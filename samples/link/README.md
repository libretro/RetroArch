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

With `HAVE_EGL=1` the harness also hosts consoles whose cores draw in
OpenGL, on Linux, and `check` ends with the scenarios in `scenarios/gl/`
("GPU consoles" below):

```sh
make -C samples/link/harness check HAVE_EGL=1 BUILD_DIR=build-gl
```

Everything built, and every run's output, goes under `BUILD_DIR` (`build` by
default); a run's output is in `out/<scenario>/<run>/`. Objects are not
rebuilt when `CC`, `SANITIZER`, `HAVE_EGL` or `HAVE_VULKAN` changes, so
each set of them needs a `BUILD_DIR` of its own.

## The six runs

`check` runs each scenario in `scenarios/` six times:

- `first`, whose trace the other runs are compared with;
- `again`, the same run again;
- `reversed`, where the scheduler resumes the highest-numbered console that
  can run, not the lowest;
- `shuffled`, in a seeded shuffled order;
- `from`, in a new process from the state the first run saved at its
  `save mid` line;
- `rewound`, which runs to the scenario's last frame, loads that state into
  the running session and runs on from the `save mid` line.

Each later run must match the first run's trace line for line and write the
same `messages.log` and `stuck.txt`. The `from` run must start at the frame
of the `save mid` line without running that line again. The `rewound` run
must give the first run's trace and `messages.log` up to the frame it loads
at, then again from the `save mid` line on, and leave each checkpoint's
pictures as the first run's. In a scenario that sets
`link_test_restart_ticks` on, the `messages.log` of the `from` and `rewound`
runs is compared without the stamps and landings, since the load renumbers
that core's ticks; the trace compares them relative to each port's anchor. A
scenario whose save is taken inside an exchange says so with a
`checkpoint in_flight` line right after its `save mid` line, and that
checkpoint's `bus.txt` must list a message in every run.

A save can come while consoles are inside a frame, parked in an advance, if
the core of each of them sets `RETRO_LINK_SAVE_IN_ADVANCE`. The state then
holds each console's own frame count, whether it is inside an advance and the
audio its frame made before the save. The `from` run starts such a console
inside that advance: its core makes the advance again when it next runs, and
the frame's audio hash goes on from the kept audio. The `rewound` run first
abandons the frames of the consoles inside one at its last step: the bus
releases them, each core ends its frame, and an abandoned frame leaves no
trace line, audio or picture. Every run abandons them the same way before it
unloads the cores.

Some scenarios are checked further. In `spans_waiting` and
`spans_membership` console 0 must finish one frame every step; in
`spans_waiting` console 1 must be inside a frame at checkpoint `parked`,
in `spans_save_in_advance` and `warm_save_in_advance` consoles 1 and 2 at
checkpoints `in_flight` and `last`, and in `spans_membership` console 2 at
checkpoints `in_flight`, `left`, `back` and `pull`, and console 1 at `plugged`
but not at `pull`; in `two_spans` console 1 must finish 598 frames
to console 0's 600, the last of them its frame 597 in step 599. Each of
`render_one`'s 120 frames must have a picture hash of its own;
`render_grow`'s picture must be 64 pixels wide at checkpoint `small` and 128
at `mid`; console 1 of `render_two` must keep its picture of frame 99 over
frame 100, which passes none, and `render_dupe`'s console its picture of
frame 39 over frame 40; and in `render_spans` consoles 1 and 2 must be
inside a frame at checkpoints `in_flight` and `last`.

Then the runs that must fail: every scenario in `scenarios/bad/`, a truncated
state and a state of another scenario must each exit 2, and every scenario in
`scenarios/fail/` and a run against an edited trace (the first run of
`two_unequal` with the picture hash of console 1's line in step 60 changed)
must each exit 1. Each must also print its error: the one a scenario's
`# error: <text>` line gives, with its line number where it has one.

Right after its load, before any core runs again, a `from` or `rewound` run
saves each console's state again, as a frontend may while paused. The run
fails when those bytes are not the state the console loaded, naming the
console and the first byte that differs, or when a core calls `send_msg`,
`recv_msg`, `peek`, `advance` or `advance_multi` while saving, which the bus
counts as `misplaced_call`. An `attach`, `detach` or `peers` call on the
console's own handle is not counted then. A field that a load leaves stale
and the next `retro_run` overwrites shows there and nowhere else.

A new process starts every field of a core at its first value, so the
`from` run passes a core whose `retro_unserialize` leaves a field alone
that is still at that value at the save. The `rewound` run loads into cores
that have run to the scenario's end, where such a field has moved on. Every
scenario in `scenarios/stale/` has such a core: its `from` run must pass,
and its `rewound` run must exit 1 and print the error its `# error:` line
gives.

A core may have no machine to load a state into before its first
`retro_run`: its `retro_unserialize` then loads nothing, and may still say
it did. `warm=1` on its console line makes the `from` run run one frame of
that console before the load; "Scenarios" says what that frame is.
`warm_load` has such a core and passes its six runs. The scenario in
`scenarios/cold/` is `warm_load` without the word: its first run must pass,
and its `from` run must exit 1 and print the error its `# error:` line
gives.

A core's state may hold something of its host that no two processes share,
a clock read at start-up for one. Its S field then differs between any two
runs that start a new machine, while its pictures and audio repeat. A
scenario says so with a `compare-fresh` line, which names the trace fields
compared for the frames a run makes before it loads a state: every frame of
`again`, `reversed` and `shuffled`, and those of `rewound` up to its load.
The `from` run, and the `rewound` run from its load on, are compared in
every field, since the state brings the first run's bytes back.
`host_stamp` has such a core and the line `compare-fresh VA`: its `again`
run's S fields must not be the first run's. One more `from` run of it,
against the first run's trace with an S field edited, must exit 1, and so
must two runs with no state, against the first run's trace with console
1's V field, then its A field, of frame 20 edited.

`make proof` makes the same six runs, with the same comparisons, of one
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
             [--pick lowest|reversed|shuffled:SEED]
             [--from STATE | --rewind STATE] [--expect TRACE]
             [--min-stack BYTES] [--require-renderer TEXT]
             [--no-context-follow]
```

- `--path NAME=FILE` binds a name the scenario uses for a core, ROM, save or
  system folder. An unbound name is the file of that name beside the
  scenario. `check` binds `test` to the test core it built.
- `--pick` sets the order in which the scheduler resumes consoles that can
  run.
- `--from STATE` starts from a state a `save` line wrote. The state is
  checked against the scenario before anything runs. Right after the load,
  each console's state is saved again: it must be the state the console
  loaded, and saving it must not call `send_msg`, `recv_msg`, `peek`,
  `advance` or `advance_multi` (`attach`, `detach` and `peers` on its own
  handle are not checked). A console whose line has `warm=1` runs one frame
  before the load.
- `--rewind STATE` runs from the first frame to the scenario's last frame
  edge, before the frame of its `end` line or of its `expect-stuck` line.
  There, in place of that edge's lines, it abandons the frame of each console
  inside one, loads the state into the running session, the bus's contents
  and each console's state into its core, saves each console's state again
  and goes on just after the `save` line that wrote the state, as a `--from`
  run does. A state the bus or a core refuses there fails the run.
- `--expect TRACE` compares the trace with an earlier run's, from this run's
  first frame, and stops at the first difference. After a `--rewind` load
  it goes on from the lines of the state's frame. Until the run loads a
  state, a scenario's `compare-fresh` line names the fields compared.
- `--min-stack` gives every console at least this much stack. Sanitizer
  builds use 8 MiB.
- `--require-renderer TEXT` fails the start of a console with `render=`
  whose driver does not name TEXT in its renderer string:
  `console <c>: renderer "<name>" is not <TEXT>`.
- `--no-context-follow` leaves the last OpenGL context made current where
  it is when another console's code runs. It is there for the check that
  must fail.

The exit code is:

- 0 when the run reached its `end` line, every `expect` held, no core
  logged an error, not even finishing a frame abandoned at the close or
  while it was unloaded, the bus counts and the frame of any release were
  those the scenario expects, every compared trace line matched, every
  console that ran the frame before a checkpoint kept its picture, each
  state saved again right after a load was the state loaded, with no
  `send_msg`, `recv_msg`, `peek`, `advance` or `advance_multi` call, and no
  core, ROM, save or state file the run read has changed.
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
            [render=gl|vulkan] [warm=1]
option <n> <key> <value>            a core option of console n
input <n> <port> <file>             an input script for an input port of console n
expect-count <counter> <value>      a bus count the run ends with
expect-stuck <frame>                the run ends in a release in this frame
compare-fresh <fields>              the trace fields compared before a load
<frame> cable <c>:<p> <c>:<p>
<frame> uncable <c>:<p>
<frame> hold <c>
<frame> unhold <c>
<frame> reset <c>
<frame> save <name>                 writes <name>.state
<frame> checkpoint <name>           writes <name>/
<frame> expect <c> save|rtc|system|video <offset> <byte>
<frame> expect <c> addr <address> <byte>
<frame> end
```

Consoles are numbered from 0 without gaps. `render=` names the one graphics
API a console's core may ask for with `SET_HW_RENDER`; without it the
harness refuses that call, and "GPU consoles" says what it does with it. A
scenario's frames are steps, counted in console 0's frames. Frame lines come
in frame order and act in file order at the edge before their step runs, so
frame 0's act once every core is loaded, and an `expect`, `save` or
`checkpoint` at frame F sees the session after F steps. Step F runs console
0 from its edge F to its edge F + 1, and every other console to its own
first edge at or after that instant, or until it is parked inside the frame
that crosses it, waiting on a peer. A console whose frame span is not known
yet, at step 0 or after a join, is run one frame; when that leaves console 0
short of its edge, the step runs again to the edges the spans now known
give. With equal spans every console finishes one frame a step. While
console 0 is held it finishes no frame, and a step runs every console that
is not held one frame. A run from a state starts just after the `save` line
that wrote it: the lines before that line are in the state, and the lines
after it at the same frame still act. A `--rewind` load goes on from there
too, and the lines after it act again; the bus is then as the state has it,
with the cables and holds of the save. A frame has at most one `save` line.
With `expect-stuck F`, no line but `end` comes after frame F, where the run
ends.

`warm=1` is for a core that builds its machine in its first `retro_run` and
loads no state before it. In a `--from` run, each console with it runs one
frame before the load: its frame 0, with the buttons its input scripts hold
at frame 0. It runs alone: no line of the scenario has acted, so it is
cabled to nothing and what it sends reaches no console, and no other
console runs. That frame is no frame of the run. It has no line in
`trace.txt` or `messages.log`, the frames the run says it passed do not
count it, and it keeps no picture or audio. It must finish: a console left
parked inside it, or a release of the bus, fails the run, as does an error
the core logs in it. The load then replaces the bus, its counts and what
the frame sent with it, and every console's state, so the run goes on as
the first run did from the save. The word changes no other run: `first`,
`again` and the pick-order runs load nothing, and a `--rewind` load comes
after the console has run.

`compare-fresh` takes letters of `V`, `A` and `S`, each once, as in
`compare-fresh VA`. With `--expect`, the trace lines of frames the run made
before it loaded a state are then compared in those fields alone, for every
console. Without the line all three are compared, as they always are from a
`--from` or `--rewind` load on. `trace.txt` has every field either way.

At an edge where a console is inside a frame, a `save` line fails the run
unless that console's core set `RETRO_LINK_SAVE_IN_ADVANCE`, and a `reset`
line fails it, as does an `expect` line that reads `save`, `rtc`,
`system` or `video`: the harness calls no entry point of a core inside a
frame but `retro_serialize_size` and `retro_serialize`. An `expect ... addr`
line reads the copy of the memory map and is allowed. A checkpoint's
`consoles.txt` says which consoles are inside a frame there.

A counter not named in an `expect-count` line must end at 0. The counters are
`lowered_promise`, `low_stamp`, `request_past_promise`,
`request_past_frame_end`, `request_past_earliest`, `skipped_publish`,
`left_out` and `misplaced_call`. A membership line the bus refuses, or an
option its core did not declare, fails the run. An input script has a
`<frame> <hex joypad mask>` line for each change, held until the next, its
frames counted in the console's own frames.

An `expect <c> addr` line reads the byte at an address of the console's
own address space, given in decimal, through the memory map its core gave
with `SET_MEMORY_MAPS`. The harness keeps a copy of the map, which a later
call replaces, and completes it as RetroArch does, through libretro-common's
`memory/memdesc.h`: a descriptor without a `select` gets one from its
`len` and `disconnect`, one without a `len` gets one, and address bits
too high to reach its memory become `disconnect` bits. It stops at the
first descriptor it cannot complete and leaves those after it as they
were. Then the first descriptor that claims the address decides. One with
a `select` claims the addresses whose `select` bits equal its `start`'s
and whose place is below `len`, the place being the address less `start`
with each `disconnect` bit taken out and the bits above it moved down. One
left without a `select` claims those from `start` up to `start + len`,
at the address less `start`. The byte is at that place after
`ptr + offset`. An address that no descriptor claims, or that one with a
NULL `ptr` claims, fails the run.

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
- `last_checkpoint`: a checkpoint at the last frame, whose pictures the
  `rewound` run must not show at the checkpoint after its load.
- `memory_map`: bytes read at addresses through the test core's memory
  map, and one of them by region as well.
- `corners`: the interface's corners at 60000/1001 frames a second: empty
  and largest payloads, every undefined flag bit set, radios that join no
  bus, and a cable pulled after the save, plugged back and pulled again.
- `lan_eight`: eight consoles on one Ethernet medium with no cable line,
  sending full Ethernet frames, half to every console and half to one.
- `two_spans`: two consoles of different frame rates on one cable.
- `spans_waiting`: a console nearly three times as fast as console 0,
  whose span step 0 does not know, so that step runs again; it ends most
  steps inside a frame.
- `spans_lookahead_zero`: a port that promises no step ahead beside a peer
  of another span.
- `spans_save_in_advance`: a save and checkpoints with two consoles inside
  a frame, as at the last step, whose input scripts give each frame around
  the save a mask of its own.
- `spans_membership`: membership changes between consoles of three spans: a
  cable plugged with a console inside a frame and a save before the join it
  makes, a cable pulled, a hold, a port that leaves a shared medium and
  joins it again, and a late join that makes a step run again.
- `unhold_dupe`: a dupe right after a hold ends.
- `render_one`: a picture drawn in two parts, around the frame's advances
  (`link_test_render`), every frame's its own.
- `render_two`: two consoles that park between the two parts of their
  pictures, each while the other runs, and a run of dupes on one of them.
- `render_spans`: `spans_save_in_advance` with two-part pictures, so two
  consoles are saved after the first part of a frame and before its second.
- `render_grow`: a picture that grows from 64 x 48 to 128 x 96, the core
  telling the frontend first (`link_test_grow`).
- `render_dupe`: a run of dupes, each after the first part of a picture
  was drawn.
- `render_held`: a hold of console 0 to the end, so console 1 is the last
  to run before the cores are unloaded.
- `warm_load`: a core that loads no state before its first frame, on a
  console with `warm=1`.
- `warm_save_in_advance`: `spans_save_in_advance` with such a core on
  console 1, which has `warm=1` and is saved inside an advance.
- `host_stamp`: a core whose state holds a digest of its save directory's
  path, which is another in every run, with a `compare-fresh VA` line.
- `gl/`: scenarios with OpenGL consoles, each the twin of one above; run
  only by a harness built with `HAVE_EGL=1` ("GPU consoles").
- `bad/`: scenarios the harness refuses before anything runs, a
  `render=` value it does not know, `render=` given twice, a `warm=` that
  is not 1 and a `compare-fresh` letter that names no field among them.
- `unbuilt/`: a console that asks for OpenGL and one that asks for Vulkan.
  While the harness is built without that API the scenario is refused as
  those in `bad/` are.
- `fail/`: scenarios that run and fail: a core that logs an error, one
  that logs it while it finishes a frame abandoned at the close or while it
  is unloaded, a count the bus does not reach, an `expect` line that does
  not hold, a release the scenario does not expect, two dupes in a row
  before a checkpoint, addresses the memory map does not map, a frame rate
  that makes a port's frame no whole number of ticks, a `save`, a `reset`
  and an `expect` line with a console inside a frame, values of
  `link_test_render`, `link_test_grow` and `link_test_gl_context` the test
  core does not know, an option of the OpenGL picture on a console that
  draws in software, and a core that asks for OpenGL on a console with no
  `render=`.
- `stale/`: scenarios whose `from` run passes and whose `rewound` run
  fails: a core that keeps a latch over a load, one whose save right after a
  load writes a copy the load left stale, one whose save there makes a link
  call, and one that keeps over a load a latch that a release sets.
- `cold/`: a scenario whose first run passes and whose `from` run fails:
  `warm_load` without `warm=1`.

## GPU consoles

A console whose line has `render=gl` gets the OpenGL context its core asks
for. The harness serves an API only when it was built for it, with a make
variable that is 0 unless given:

- `HAVE_EGL=1` serves `render=gl`. The harness then links libEGL and
  nothing else of the driver's: it uses no GL header and no GL library, and
  takes every entry point from `eglGetProcAddress`. With a mingw compiler
  it is a make error.
- `HAVE_VULKAN=1` takes `render=vulkan` lines, but this harness has no
  Vulkan host: a core's `SET_HW_RENDER` on such a console is refused, as it
  is on a console with no `render=`.

A scenario with a `render=` the harness was built without exits 2 with
`<scenario>:<line>: this harness was built without HAVE_EGL` (or
`HAVE_VULKAN`). A console with `render=` gets a stack of at least 8388608
bytes, since driver code runs on its cothread.

For a `render=gl` console the harness does what a frontend does, with no
window, X server or Wayland display:

- `SET_HW_RENDER` makes a surfaceless EGL context of the type, version and
  profile asked for: an OpenGL core context, the driver's compatibility
  context, or OpenGL ES. It is refused, with the reason on stderr, when the
  driver has no such context, and after the core's load has returned, which
  fails the run. The display is EGL's surfaceless platform or, without it,
  a device, those that draw in software tried first.
- After `retro_load_game` the context is made current and gets a
  framebuffer of the largest size `retro_get_system_av_info` gives, with
  the depth and stencil the core asked for; then `context_reset` is called.
  `get_current_framebuffer` names that framebuffer. `SET_SYSTEM_AV_INFO`
  with a larger size grows it, with no context reset and the core's
  bindings put back.
- A frame passed as `RETRO_HW_FRAME_BUFFER_VALID` is read back with
  `glReadPixels` straight into the console's picture, turned the right way
  up when the core set `bottom_left_origin`, and hashed as the XRGB8888
  rows of a software picture are. The read uses bindings and pixel-pack
  state of its own, puts the core's back, and drops the GL errors the core
  left so that one it finds is its own. A frame larger than the framebuffer
  is an error, said once.
- EGL keeps one current context for a thread, and every console runs on
  one thread. The harness makes a console's context current each time that
  console's code runs again: when the scheduler switches into its frame,
  and around every other call into its core, `context_destroy`,
  `retro_unload_game` and `retro_serialize` among them.
- At the close `context_destroy` is called before `retro_unload_game`, and
  the context is destroyed after it.

`make check` and `make check-gl` choose the driver with `GPU_DRIVER`:

- `software`, the default, runs every GPU console on Mesa's llvmpipe
  (`LIBGL_ALWAYS_SOFTWARE=1 GALLIUM_DRIVER=llvmpipe`, with `DISPLAY` and
  `WAYLAND_DISPLAY` unset) and passes `--require-renderer llvmpipe`, so a
  run on any other renderer fails and never passes for want of the driver.
- `host` runs them on the machine's own driver and requires no renderer.

`check-gl` makes the six runs of every scenario in `scenarios/gl/`, and
`check` ends with it when `HAVE_EGL` is 1. A scenario there starts with
`# twin: <name>`, the scenario in `scenarios/` that it differs from only in
its `render=` fields, its `link_test_render` values and the options of the
OpenGL picture. After its six runs, its first run's `trace.txt`,
`messages.log` and every checkpoint picture must be byte for byte the
twin's first run's: a GPU console is compared on the hash of every frame it
passes, the checkpoint pictures, and everything a console that draws in
software is compared on. A scenario with no twin line fails the check. Then:

- each scenario in `gl/fail/` must exit 1 and print its `# error:` text: a
  context version no driver has, a context asked for after the core's
  load has returned, and a picture that grows past its
  framebuffer without the core telling the frontend;
- one more run with `--require-renderer no-such-renderer` must exit 1;
- `gl/two` with `--no-context-follow` must fail the first run's trace: with
  the context of the console started last current for both, console 0's
  first picture is already another;
- one more run of `gl/two` must print a count of context switches above
  400, which shows only that the count is kept;
- with `GPU_DRIVER=software`, `gl/gles2` runs once more with
  `MESA_GLES_VERSION_OVERRIDE=2.0` and must give the first run's trace in a
  context whose `gpu.txt` says OpenGL ES 2.0;
- consoles 1 and 2 of `gl/spans` and `gl/mixed` must be inside a frame at
  checkpoints `in_flight` and `last`.

The scenarios in `gl/`:

- `origin_top` and `origin_bottom` (twin `render_one`): a 3.3 core context,
  the framebuffer's first row the picture's top or its bottom.
- `compat` (`render_one`): the driver's compatibility context, with a depth
  buffer and no stencil.
- `gles2` (`render_one`): a core that asks for OpenGL ES 2, with neither
  depth nor stencil. A driver may answer with a later version, and Mesa
  gives OpenGL ES 3.2; the run held to 2.0 is the one above.
- `dirty` (`render_one`): a core that leaves its context with an unread GL
  error and pixel-pack state of its own (`link_test_gl_state`).
- `dupe` (`render_dupe`): dupes, with the first part of each frame in the
  framebuffer and no picture passed.
- `grow` (`render_grow`): a told growth, which grows the framebuffer.
- `two` (`render_two`): a 3.3 core context and an OpenGL ES one, each
  console parked between the two parts of its picture while the other
  draws.
- `spans` (`render_spans`): three OpenGL consoles, two of them saved
  inside a frame, between the two parts of a picture, and abandoned inside
  one at the close.
- `mixed` (`render_spans`): one OpenGL console among two that draw in
  software.
- `held` (`render_held`): console 0, whose context has pixel-pack state of
  its own, is unloaded after console 1 alone has run, and its
  `context_destroy` must find its own context current.

What is not served or not checked:

- `RETRO_ENVIRONMENT_GET_HW_SHARED_CONTEXT_INTERFACE` is not answered, so a
  core that draws on a thread of its own is not hosted.
- `SET_HW_RENDER` after the core's load is refused, and no scenario loses a
  context: `context_reset` is called once for a console.
- No GPU console runs under Wine.
- No scenario loads a state saved before a told growth into a core that
  has grown, so the core telling the smaller size again is not run.
- Nothing checks the stack a `render=` console is given.
- `gl/dirty` shows the pixel-pack state put back only where the context
  has all of it; a context that knows the alignment alone is not run.

## Output

- `trace.txt`, for each step a line for each frame a console finished in it,
  by console and then frame, N being the console's own frame; `HELD` for a
  held console, which finishes none; no line for a console that spent the
  step inside a frame; then a line for the bus:

  ```
  F <step> C <n> N <frame> V <hex> A <hex> S <hex>
  F <step> C <n> HELD
  F <step> BUS <hex>
  ```

  V, A and S are the xxh64 of the picture rows, the audio and the
  `retro_serialize` output; BUS hashes the bus contents with every tick
  taken from its port's anchor, so a core that renumbers its ticks on a load
  still hashes the same.
- `messages.log`, a line for each message sent, by step, receiving port,
  sending port and send order:
  `F <step> <c>:<p> -> <c>:<p> stamp <hex> landing <hex> len <n> head <hex>`.
  A message with no landing by the step's end, such as one dropped when
  sent, shows `ffffffffffffffff`.
- `<name>.state` for each `save` line, version 2: the frame, a hash of the
  scenario file, for each console its core's state, its own frames
  finished, whether it is inside an advance and the audio of that frame
  before the save, then the bus contents.
- `<name>/` for each `checkpoint`: `c<n>.png`, each console's picture of the
  last frame it finished, or over a dupe of the frame before, for each
  console not held that has finished one since it started, loaded or was
  held; `bus.txt`, the bus contents; `counts.txt`, the bus counts;
  `consoles.txt`, a line `c<n> frame <frames finished> in_frame <0|1>` for
  each console, from which a scenario's steps are chosen.
- `stuck.txt` after a release: each console that was parked, with the port
  and peer that bound it, or `no progress` for one parked for advancing
  without progress.
- `c<n>/`, the console's copy of its core, and `c<n>/save/`, its save
  folder with a copy of its save. Cores, ROMs and saves are read, never
  written.
- `c<n>/resaved.state`, only when a console's state saved again right
  after a load is not the state it loaded: the bytes it saved.
- `c<n>/gpu.txt`, for a console with a context: one line,
  `gl <GL_VERSION> | <GL_RENDERER>`. No run compares it.

A run with an OpenGL console prints `link_harness: <n> GL context switches`
before its pass line: the times a context was made current that was not the
last one. The number depends on the pick order, and no run compares it.

## Test core options

The test core is built as `link_test_libretro.so`, or `.dll` on Windows.
`P` is a port number, 0 to 2.

| Option | Values | Default |
|---|---|---|
| `link_test_ports` | 1-3 | 1 |
| `link_test_fps` | frames a second, `N` or `N/D` | 60 |
| `link_test_rate_P` | ticks a second; with `link_test_fps` N/D, rate * D / N whole | 61440 |
| `link_test_steps` | steps a frame, 1-64 | 16 |
| `link_test_lookahead_P` | steps the promise runs ahead, 0-64 | 1 |
| `link_test_protocol_P` | protocol id | test-cable |
| `link_test_max_ports_P` | 0-255 | 2 |
| `link_test_shared_P` | off, on | off |
| `link_test_send_P` | none, broadcast, or an index 0-254 | broadcast |
| `link_test_seed` | a number | 1 |
| `link_test_send_frames` | `A-B[,A-B]...`, the frames that may send | 0-999999 |
| `link_test_payload_len` | `seeded`, or a payload length 0-4096 for every message | seeded |
| `link_test_dupe_frames` | `A-B[,A-B]...`, the frames that pass no picture (dupes); set, the core needs `GET_CAN_DUPE` | empty |
| `link_test_detach_P`, `link_test_reattach_P` | the frame in whose `retro_run` it happens, or -1 | -1 |
| `link_test_restart_ticks` | off, on | off |
| `link_test_extra_flags` | off, on: every undefined bit set in its port and core flags | off |
| `link_test_save_in_advance` | off, on | off |
| `link_test_mode` | conform, nonconform, never_advance, no_progress, stale_load, stale_save, stale_send, stale_release, release_error, unload_error, cold_load, host_stamp | conform |
| `link_test_bad_frame` | the first frame of the mode | 0 |
| `link_test_render` | off, soft, gl: the picture in two parts, and what draws it | off |
| `link_test_grow` | off, `told:N` or `untold:N`: the frame the two-part picture grows in | off |
| `link_test_gl_context` | core33, compat, gles2, core99: the OpenGL context it asks for | core33 |
| `link_test_gl_origin` | top, bottom: the picture's row that is the framebuffer's first | top |
| `link_test_gl_state` | clean, dirty: the state it leaves its context in | clean |
| `link_test_gl_ask` | load, run: whether it asks for its context in `retro_load_game` or in its first `retro_run` | load |

In each `retro_run` the core advances its ports at the frame start, at each
of its steps and at the frame end. It takes every message it can, applies
each one once it has passed the message's landing tick, and on seeded
events sends a payload made from its state, stamped at its promise. With
`link_test_restart_ticks` on, its ticks start from a new number after every
load and move at every reset, while its saved state stays the same.

A `seeded` payload is 8 to 64 bytes long, by the seed; a
`link_test_payload_len` of 0 sends a NULL buffer. In `conform` mode the core
also checks, after each advance and without changing its state, the calls a
core may make with NULL arguments: `peek` with every out argument NULL
agrees with a full peek, `recv_msg` with `len` NULL takes nothing, messages
are taken with `tick` and `from` NULL after a peek, and `peers` with `count`
NULL agrees with a full call. A failed check is a logged error.

With `link_test_save_in_advance` on, the core sets
`RETRO_LINK_SAVE_IN_ADVANCE`: its state holds the advance it is inside,
and its first `retro_run` after a load makes that advance again and goes
on from there. It then also sends 16 audio frames at each of its steps, so a
save inside a frame has audio of that frame to keep, and folds its input
into its state at each step, so a frame finished after a load must get that
frame's input. With `link_test_restart_ticks`, `nonconform` or `no_progress`
it logs an error.

With `link_test_render` off the core draws its picture at the end of the
frame, from its state hash. With `soft` or `gl` it draws a picture in two
parts: three bands of eight stripes before the frame's first advance, from
its state hash there, and three more after its last, from the hash the
frame ends with. A console parked inside a frame has then drawn half a
picture, and its state holds what that half was drawn from, so a frame
finished after a load draws it again. A dupe frame draws the first part and
passes no picture. The picture is 64 x 48; from frame N of
`link_test_grow` it is 128 x 96. With `told:N` the core tells the
frontend, with `SET_SYSTEM_AV_INFO` at the start of a frame, each size
that is not the one it told last; its state does not hold what it told, so
a run from a state saved after the growth tells it again. With `untold:N`
it tells nothing. `link_test_grow` with `link_test_render` off is a logged
error.

With `gl` the core asks for a context with `SET_HW_RENDER` in
`retro_load_game`, which fails if that is refused, and draws the same
picture with scissored clears in the frontend's framebuffer, through
`get_proc_address` alone. `link_test_gl_context` picks what it asks for:
an OpenGL 3.3 core context with depth and stencil, the driver's
compatibility context with depth, an OpenGL ES 2 context with neither, or
an OpenGL 9.9 core context, which no driver has. `link_test_gl_origin`
sets `bottom_left_origin` and the row the core starts its picture at.
With `link_test_gl_state` dirty the core leaves its context with a GL error
nobody read and with pixel-pack state of its own (an alignment of 8, rows
of 200 pixels, one row and one pixel skipped), and logs an error when that
state is not there at the start of a frame or in `context_destroy`: a
frontend must read the picture by state of its own, put the core's back,
and have the core's context current whenever it calls the core. With
`link_test_gl_ask` run the core asks in its first `retro_run` instead;
refused, it passes dupes and logs no error, so that only the frontend
fails the run. One of these four options off its default with
`link_test_render` not `gl` is a logged error.

From `link_test_bad_frame` on, the modes break the rules:

- `nonconform`: in every frame it lowers its promise, stamps a message below
  it, asks one step past it and asks past its frame end, each once and in a
  call of its own. In the fourth frame of the mode it makes no frame-end
  publish. It starts at frame 1 or later.
- `never_advance`: it makes no advance at all.
- `no_progress`: in that frame alone, it advances again and again without
  progress until the bus is released.
- `stale_load`: it sets a latch, and while the latch is set it folds the
  frame number into its state every frame. The latch is in no save state,
  and no load clears it.
- `stale_save`: it keeps a copy of its state hash as each `retro_run`
  leaves it, and `retro_serialize` writes the copy in place of the hash.
  No load refreshes the copy.
- `stale_send`: it keeps the same copy, and `retro_serialize` sends a
  message when the copy is not its state hash: a link call outside
  `retro_run`.
- `stale_release`: an advance that reports `RETRO_LINK_WAKE_DETACHED` sets
  a latch, and while the latch is set it folds the frame number into its
  state every frame. The latch is in no save state, and no load clears it.
- `release_error`: an advance that reports `RETRO_LINK_WAKE_DETACHED`
  logs an error.
- `unload_error`: `retro_unload_game` logs an error.
- `cold_load`: whatever `link_test_bad_frame` says, until its first
  `retro_run` its `retro_unserialize` loads nothing and returns true.
- `host_stamp`: whatever `link_test_bad_frame` says, `retro_load_game`
  writes a digest of the save directory's path into system RAM, so every
  state holds it. No picture or audio depends on it.

Its system RAM, for `expect` lines: 0x00-0x07 the state hash; 0x08 the ports
attached; 0x09-0x0B each port's peer count; 0x0C-0x0E each port's index, 255
when it is cabled to nothing; 0x0F the errors it logged; 0x10-0x13 the
messages received and 0x14-0x17 those sent, little-endian; 0x18 and 0x19 the
topology and detached reports it was given; 0x20-0x27, in mode
`host_stamp`, the digest of the save directory's path.

Its memory map, for `expect ... addr` lines, gives five views of that RAM:
0x10000000-0x1000003F maps all of it; every address from 0x20000000 to
0x2FFFFFFF maps its bytes 8-15, through a `select` of 0xF0000000, an
`offset` of 8 and a `len` of 8, which clears the address's high bits;
0x30000000-0x3000000F has no memory; every address from 0x40000000 to
0x4FFFFFFF maps bytes 8-15 again in the same way, but with a `disconnect`
of 2, so address bit 1 is ignored and the bits above it move down; and
from 0x60000000, bytes 8-15 once more through a descriptor with no
`select`, a `len` of 8 and a `disconnect` of 2, which RetroArch reads as
claiming the addresses up to 0x6000000D whose bit 1 is clear. The core
builds the map on its stack and clears it after the call, so a frontend
must keep a copy.
