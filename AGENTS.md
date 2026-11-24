AGENTS.md

## Project

RetroArch is primarily written in C89. Preserve the existing language and style of the file being edited.

- New C code must be C89-compatible unless the surrounding code explicitly requires otherwise.
- Do not introduce C99/C11 features into C89 code.
- Existing files written in C++, Objective-C, or another language should remain in that language.
- Follow the conventions of surrounding code rather than introducing a new style.
- Prefer the smallest change that completely solves the task.
- Preserve existing architecture

Read CODING-GUIDELINES before changing code: brace style, declarations,
stack budgets and the never-block rule for the main thread are there.
Subdirectories with rules of their own carry their own AGENTS.md.
`tools/compile_commands.py`, after ./configure, writes compile_commands.json
for clangd and other code navigation.

## Build dependencies

Normal RetroArch builds must not require Python. In particular, do not add
Python dependencies for shader generation, embedding or validation. Commit
the precompiled shader assets and the C headers needed to embed them, and
compile those committed files directly.
Keep regeneration and asset validation in explicit developer tools and CI;
never run them implicitly from ordinary Make or MSBuild targets. Do not fix
a missing Python error by adding Python as a normal build dependency.

## Copies

Remove a copy wherever one can be removed, on every change: look for one
each time a data path is written or touched, not only when asked.

- Hand data over by borrow or lend. Have the producer write into the memory
  the consumer reads (driver frame memory, a ring's write span, a device
  buffer), or have the consumer read the producer's memory in place.
- Give a borrow a stated owner and lifetime, and retire it through the rings,
  atomics and fences above, never a lock or a wait.
- A lend is an offer: it can be declined, so the copy stays as its fallback.
- Keep a copy only where nothing else can work (the owner frees or rewrites
  the memory before the reader is done, or uncached memory would be read
  back).

## Real time

RetroArch is a soft real-time system. The main, video and audio paths
must stay within a frame's budget, and a lock, a sleep or a blocking wait
on them shows up as jitter, audio underruns or extra frames of latency -
which run-ahead, hard sync and the low-latency modes cannot absorb.

The primitives allowed are the ones the concurrency actually needs:

- SPSC rings for a producer-consumer handoff (core to video, the audio
  callbacks and the like).
- Atomics for flags and simple shared state.
- Eventcounts for a wait that neither spins nor puts a sleep in front of
  the caller.

Spinlocks and busy-waiting are out: they burn power, hurt the caches and
invert priorities. Needing to block means the design is wrong; reshape it
around lends and borrows, slots allocated ahead and progress that never
waits, as the Copies section does for data. Pacing follows the same rule:
frames are timed by the audio clock or the display's vsync, never by
wall-clock sleeps, so nothing drifts and no latency is added.

## Threads

- Nothing may block: no sleeping, busy-waiting, wait-for-idle or round-trip
  waits, least of all in the threaded video wrapper and the audio pipeline.
  A design that needs to block is shaped wrong.
- Do not add locks. Use retro_atomic.h, the SPSC rings (retro_spsc.h,
  retro_waitable_spsc.h) and retro_eventcount.h.
- Guard threaded code with HAVE_THREADS, and check it with a full
  `./configure --disable-threads` build that links, not a single TU.
- No logging inside pacing or sync paths.

## Clocks

No wall clocks and no spinning, as far as there is any way to avoid them.
Each exception must be the only way, and say why in a comment.

- Do not read the clock (`cpu_features_get_time_usec()`) on a per-frame or
  real-time path. A frame has one reading, taken by `video_driver_frame()`
  and carried in `video_frame_info_t::frame_time`; whatever draws or hands
  over that frame takes "now" from it, moved on by a mark where it needs
  a later instant. A runloop iteration likewise has one reading,
  `runloop_state_t::iter_time`, and `RUNLOOP_NOW()` moves it on.
- Time a span with `CPU_FEATURES_MARK()` at each end and
  `CPU_FEATURES_MARK_TO_USEC()` of the difference, not with two clock
  reads: a mark is the perf counter where that is a register read, the
  clock where it is not.
- Do not spin on the clock, or on anything else: no loop that polls a
  time, a flag or a counter until it changes. Wait on the event that
  ends the wait (a fence, a vblank, the device's notification, an
  eventcount); a clock deadline is only a watchdog on that wait.
- Do not pace on a timer where the hardware gives a signal to pace on.

RetroArch contains deliberately complex, cross-file systems. Do not simplify, consolidate, refactor, or redesign an existing subsystem merely because its structure appears unnecessarily complicated.

Complexity may exist for compatibility, platform, driver, or architectural reasons.

When implementing a feature, find an existing feature with similar behavior and follow its implementation pattern. The current source tree takes precedence over historical examples.

Do not make unrelated cleanup or refactoring changes.

## Portability

Code that builds with GCC or Clang on Linux can still break MSVC, UWP,
griffin or old-SDK builds that you never ran. Check for these when
writing the code:

- Do not use a name that a Windows SDK header defines as a macro for a
  variable, parameter, field or function. `rpcndr.h`, which windows.h,
  D3D, DXGI, EGL and UWP headers all pull in, has `#define small char`
  and `#define hyper __int64`. `windef.h` defines `near`, `far`, `IN`,
  `OUT` and `OPTIONAL` as nothing, `objbase.h` defines `interface` as
  `struct`, and windows.h defines `min` and `max` as function-like
  macros. mingw-w64 guards some of these defines, so a mingw build is
  no proof. `tools/win_macro_names_check.py` covers `small` and
  `hyper`. To check a file by hand, compile it with `-Dsmall=char
  -Dhyper=__int64`.
- The griffin builds (MSVC, UWP and most consoles) compile almost every
  .c file as one translation unit through `griffin/griffin.c`. So
  file-scope `static` names must be unique across all the files griffin
  joins, not just within one file. A header that defines data (shader
  source strings and the like) needs an include guard, and a name of its
  own if a sibling header for another backend defines similar data.
  Check by running `x86_64-w64-mingw32-gcc -fsyntax-only` on
  griffin.c with the build's defines, before and after the change,
  and compare the two error lists.
- Shared GL code does not spell framebuffer-object names directly.
  macOS SDKs before 10.12 (the PPC build) have only the `_EXT` names,
  and PSGL has the `_OES` names. Use the per-SDK macros the way gl2.c
  and gl_common.c do.
- A platform you cannot build still gets compiled. Force the
  platform's branch of the `#if` on and compile it on Linux, and say
  in the commit that it was checked only that way.
- Some samples read a shipping file as text and build it from a copy in
  a temporary directory. The Metal sample does this with
  `Shaders.metal`, for example. Before adding an `#include` to such a
  file, run `tools/ci_local.py which` on it and run the samples it
  lists.

## Comments

Keep comments as short as possible.

- Prefer no comment when the code is self-explanatory.
- When a comment is necessary, prefer a few words or a few lines at most.
- Explain why something is non-obvious, rather than describing what obvious code does.
- Do not add comments merely because code was changed.
- Do not describe the previous implementation or behavior unless that historical context is necessary to understand the current code.
- Do not use comments as miniature documentation or change logs.
- Prefer clear names and straightforward code over explanatory comments.

Comments should describe information that will remain useful after the current change is forgotten.

## Commit messages

Keep commit messages concise and focused.

- Explain what changed and why.
- Prefer a short subject line and a brief body when additional context is useful.
- Do not enumerate every changed file or implementation detail.
- Do not describe intermediate attempts, discarded approaches, or the development process.
- Do not explain the previous implementation unless it is necessary to understand the change.
- Keep the complete commit message below approximately 600 words.
- A commit message is not a changelog or development diary.

## Menu changes

Adding or modifying a menu option commonly requires changes in several parts of the codebase.

Do not assume that a menu option can be implemented by modifying only the file where it is displayed.

### Before adding a menu option:

- Find an existing menu option with similar behavior.
- Trace its complete implementation through the menu, settings, configuration, and message systems.
- Identify every part of that pattern that applies to the new option.
- Implement the new option consistently across all required layers.
- Do not modify unrelated menu code.

#### Useful reference commits:

- 6e9aa66aea2b2b3d0687585e41bac658ac9ae252 — SMB timeout/context settings exposed as menu options.
- 434f41b935232325ed13024786653f9dfef49643 — CRT menu additions.
- 23170b82ec58c2b283a7132b85fa73e18050a5ae — MIDI device dropdown menu items.
05faba73e3fed69a5e118df2cc28d31309089243 — menu implementation example.

These are reference implementations, not fixed lists of files to modify. The required files depend on the feature.

## Shader and video backend changes

Shader functionality frequently crosses shared shader code and multiple video backends.

### When adding or modifying shader functionality:

- Find an existing shader feature with similar behavior.
- Trace the feature through the shared shader infrastructure.
- Identify every applicable shader implementation and video backend.
- Keep backend implementations consistent where the feature is supported.
- Do not update only the backend currently being tested if the feature is expected to work elsewhere.
- Check platform/backend-specific differences rather than blindly duplicating code.

#### Useful reference commits:

- da5ecaa45a862e51783eca1280f46fbfb7984ede — OriginalAspect and OriginalAspectRot uniforms.
- 2a56a827e8a97a3838a9d455f1dca442c0ffbb07 — frametime/original-FPS shader uniforms.
- 37cfa4857d064464754e800bad89ce69e81230fe — sensor shader uniforms across multiple shader and video/input backends.

These commits demonstrate the breadth of changes that may be required. They are examples of implementation patterns, not instructions to reproduce their exact file lists.

## Historical commits

Historical commits may be used to understand established implementation patterns.

- Treat examples as references, not specifications.
- Prefer the current source tree when it differs from an historical commit.
- Do not copy historical changes mechanically.
- Do not reproduce obsolete workarounds merely because they appear in an old commit.
- Inspect surrounding current code before applying a historical pattern.

## Before every commit

Run CI's tree-wide checks before committing; most broken CI runs come
from commits that skipped them.

- Once per clone: `git config core.hooksPath tools/hooks`. The
  pre-commit hook then runs `tools/precommit.sh` on the staged files:
  the Windows SDK macro names, overlapping copies, the Vulkan and D3D12
  wait, submit and teardown rules, the msg_hash and sublabel tables,
  the settings table, sample object directories and workflow paths.
  Each is the check its workflow runs.
- A failing hook means CI would fail. Fix the cause; do not commit with
  `--no-verify`, and do not change a checker or its allow list to let
  the commit through.
- The hook ends by listing the CI jobs the staged files reach
  (`tools/ci_local.py which`). Run each of them with
  `tools/ci_local.py run` before pushing, as the next section says.
- Without the hook, run `sh tools/precommit.sh <changed files>` by hand.

## Regression tests

Regression tests must build on both Windows and Linux.

- Do not use POSIX-only or Win32-only headers or APIs in test code without a working path for the other platform; prefer libretro-common facilities.
- Check that the test builds with a Linux toolchain and with mingw-w64 (MSYS2 MINGW64 or a cross-compiler) before submitting.

If a change touches code already covered by the GitHub CI workflow regression tests (`.github/workflows/`), run those tests locally and make sure they still pass before presenting the change.

- `tools/ci_local.py which <changed files>` lists the jobs that name them (through a sample's Makefile or a tools/ script too), the full-tree builds and the tree-wide checks.
- `tools/ci_local.py run <workflow> --job <job>` runs that job's own steps, with its configure line, flags, matrix and environment. `--dry-run` prints them, `--clean-copy` runs on a fresh worktree of HEAD plus your changes, and container jobs run in their image through docker or podman.
- Do not present a change while any of those tests fail.
- A workflow construct `ci_local.py` cannot read fails `tools/ci_local.py selftest`, which CI runs; extend the script in the same change.
- A job's Ubuntu packages go in its `APT_PKGS` and are installed once, through `.github/workflows/scripts/apt-install.sh` and its cache steps; `tools/ci_apt_check.py` fails any other `apt-get` in a runner job.

### Adding or changing a regression test

- Show the test failing without the fix and passing with it. A test never seen to fail proves nothing.
- Put a new step in the job that already runs its area: `tools/ci_local.py which` on a neighbouring sample names it. Within a job, steps run in order, so a step goes after any step whose output it uses. A step that takes more than a couple of minutes gets a job of its own instead of lengthening a shared one; jobs run side by side.
- A new directory under `samples/` or `libretro-common/samples/` is claimed by a workflow step in the same commit, or the unclaimed-samples job builds it on its own.
- After adding or changing a workflow step, or making one sample use another's files, run `tools/ci_paths.py` and commit the paths filters it rewrites. CI runs `tools/ci_paths.py --check` and fails on a stale filter.
- Harnesses list their sources by hand. When a change makes a shipping file call a function defined in another file, run `tools/ci_local.py which` on the calling file and build every harness it lists. Each one that links the caller needs the callee's source, or a stand-in that does what the real function does, added in the same change.

The runner image moves under the tree: Ubuntu releases bring new Mesa, Wine, ffmpeg, compilers, mingw-w64 headers and validation layers. Test what the code does, not what the current image happens to do:

- Find tools by name on the `PATH`. When a package moves one, fix it once in `apt-install.sh`, not in each step.
- Match a Vulkan validation message by its ID, which the driver's log line carries as `[ VUID-... ]`, never by its wording.
- A fixture made with an external tool (ffmpeg and the like) builds the property under test explicitly, so the tool cannot choose it. For example, give the encoder frames of its own length so timestamps reach it unchanged. Where you can, have the generator check the property, so a tool that changes fails there with a clear message rather than as a wrong result in the test.
- System headers can change dialect: newer SPA headers declare variables in `for` loops, and mingw-w64 13 hides APIs newer than the `_WIN32_WINNT` a build targets. Resolve newer Windows APIs at runtime; do not depend on what an SDK happens to declare.
- Start an X server with `xvfb-run -a`, or on a display number no other step of the job uses.
- With threads, assert only states that cannot have moved on yet: a load queued on a worker may already be finished when the next line runs.
- Sanitizer builds at `-O0` have larger stack frames than optimised ones, and they grow with the compiler. A function close to the frame limit (2 KiB in libretro-common, 4 KiB elsewhere) is split; the limit is not raised.

## Never commit build output

Nothing built is ever committed: no executables, objects, libraries,
archives, generated images or captures. This includes every sample and
tool harness built under samples/, libretro-common/samples/ and tools/.

Committed shader bytecode and its C embedding headers are the intentional
exception described under Build dependencies.

- Stage files by name (`git add path/file.c`), never a directory or
  `git add -A`, so a build that ran after a `make clean` cannot slip in.
- Before every commit run `git status --short` and check that each
  staged path is source, a Makefile, a script or documentation.
- A new harness whose executable name the `.gitignore` patterns do not
  cover (`*_test`, `*_test_*`) gets its own ignore line in the same
  commit that adds the harness.

## Scope of changes

Keep changes focused.

### Do not:

- reformat unrelated code;
- rename unrelated symbols;
- modernize C89 code;
- replace established patterns with preferred patterns;
- refactor neighboring code without a direct need;
- add documentation or comments solely to explain the current change;
- make speculative improvements.

If a task requires a broad change, make the breadth necessary for the feature rather than artificially minimizing the number of files changed.

A change touching many files is not inherently a problem in RetroArch. Missing a required layer of an established multi-file system is a problem.
