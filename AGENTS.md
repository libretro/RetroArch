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

## Threads

- Nothing may block: no sleeping, busy-waiting, wait-for-idle or round-trip
  waits, least of all in the threaded video wrapper and the audio pipeline.
  A design that needs to block is shaped wrong.
- Do not add locks. Use retro_atomic.h, the SPSC rings (retro_spsc.h,
  retro_waitable_spsc.h) and retro_eventcount.h.
- Guard threaded code with HAVE_THREADS, and check it with a full
  `./configure --disable-threads` build that links, not a single TU.
- No logging inside pacing or sync paths.

RetroArch contains deliberately complex, cross-file systems. Do not simplify, consolidate, refactor, or redesign an existing subsystem merely because its structure appears unnecessarily complicated.

Complexity may exist for compatibility, platform, driver, or architectural reasons.

When implementing a feature, find an existing feature with similar behavior and follow its implementation pattern. The current source tree takes precedence over historical examples.

Do not make unrelated cleanup or refactoring changes.

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
