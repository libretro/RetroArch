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
- `failed_save_preserves_state`: a refused save overwrites its destination;
  the prior state stays intact, live and loadable, with no release.
- `unusable_stops_and_reports`: execution stops and queues one recovery message.
- `registration_after_init`: late registration is rejected.
- `preempt_deinit_releases_states`: populated history is released once.
- `history_clear_resets_replay_window`: released history cannot be replayed.

Check that these tests reject broken implementations:

```sh
make -C samples/runahead/lifetime mutants
make -C samples/runahead/lifetime check MUTANT=missing-release
```

Each check first requires the normal tests to pass, then prints
`PASS rejects <name>` only for its expected assertion failure. The six
mutants cover one direct test each: missing release, overwritten state,
missing halt message, late registration, missing PF teardown releases and
stale replay history. They use private build copies; compile errors and
unrelated failures fail the check. Source matching is deliberately exact:
edits to the matched code require updating its mutation.

Only the direct tests are mutation-tested. The frontend cases below are
integration checks; no frontend mutations are run.

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

The frontend tests print twelve additional `PASS` lines and
`lifetime integration: 12 passed`. They cover normal and refused saves,
halting, late registration, secondary-instance exclusion and its unregistered
control, PF shutdown/save/load errors, pending options, reset and an older
API version. Refusal tests prove the injected branch executed. Trace order
checks every owner, and replay-save recovery checks consecutive canonical frames.
The halt case primes a memory descriptor, then checks memory and persistence
refusal after failure. The autosave consumer is tested separately:

```sh
make -C samples/runloop/autosave_snapshot check
```

Use `test.py --case <name>` to select a frontend test, with the same
`--frontend` and `--core` paths printed by Make. `--output <directory>`
retains logs and traces there; otherwise a new temporary directory is used.
CI runs the frontend with threads enabled and disabled, and the direct
ownership/autosave tests under ASan and UBSan. It also checks the mutants
and cross-compiles the Windows sample; Windows frontend runs are local.
The existing Linux platform workflow builds with default run-ahead support;
the headless lifecycle job enables it explicitly.
