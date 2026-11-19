# Savestate lifetime tests

No games, BIOS, graphics driver or display server are needed. The reference
core retains CPU objects in place of GPU resources and checks every load,
release and unload. These tests cover the API and frontend ownership;
renderer accuracy and physical GPU loss need separate tests.

From the repository root, with GCC and GNU Make:

```sh
make -C samples/runahead/lifetime check
make -C samples/runahead/lifetime check THREADS=0 BUILD_DIR=no-threads
make -C samples/runahead/lifetime check SANITIZER=address,undefined BUILD_DIR=sanitized
```

The direct tests include the shipping `runahead.c`, without extracting or
copying its functions. Expected output is one `PASS <name>` per test:

- `release_once`: copied states, repeated loads and one final release.
- `save_in_place`: a buffer's state is released before the next save goes
  into it, so no more than one state per buffer is ever live.
- `refused_save_owns_nothing`: a refused save leaves its buffer released and
  unowned.
- `registration_after_init`: late registration is rejected.
- `preempt_deinit_releases_states`: populated history is released once.
- `history_clear_resets_replay_window`: released history cannot be replayed.

The test includes the whole of `runahead.c`; the linker drops what no test
calls (`--gc-sections`), and the few symbols a linker still asks for are
stubbed in the test.

With this branch built as `retroarch`, run the actual frontend too:

```sh
make -C samples/runahead/lifetime check FRONTEND="$PWD/retroarch"
```

On Windows, with MinGW GCC, GNU Make and `sh.exe` on `PATH`, use PowerShell:

```powershell
make -C samples/runahead/lifetime CC=gcc PYTHON=python SHELL=sh.exe check "FRONTEND=$((Resolve-Path .\retroarch.exe).Path)"
```

The core uses `.dll` automatically. Sanitizers require a compiler/runtime
that supports them.

The frontend tests print sixteen additional `PASS` lines and
`lifetime integration: 16 passed`. They cover normal and refused saves, a
managed load refused once (tried again, Run-Ahead carries on) and twice (the
core is reset and Run-Ahead turned off), late registration,
secondary-instance exclusion and its unregistered control, PF
shutdown/save/load errors, pending options, reset, an unknown API version, a
video driver reinit (which must release every managed state before the
core's context goes), rewind under Preemptive Frames (whose pushes must not
land in the managed ring), and a slot save from a core that asks to be saved
in the background, whose managed calls stay on the core thread. Run-Ahead
must report only retained and speculative runs, Preemptive Frames only
retained runs and replays. Refusal tests prove the injected branch executed.
Trace order checks every owner, and replay-save recovery checks consecutive
retained frames.

Use `test.py --case <name>` to select a frontend test, with the same
`--frontend` and `--core` paths printed by Make. `--output <directory>`
retains logs and traces there; otherwise a new temporary directory is used.
CI runs the frontend with threads enabled and disabled, and the direct
ownership tests under ASan and UBSan. It also cross-compiles the
Windows sample; Windows frontend runs are local.
The existing Linux platform workflow builds with default run-ahead support;
the headless lifecycle job enables it explicitly.
