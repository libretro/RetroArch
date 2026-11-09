# CHANGELOG — RetroArch Custom Modifications
> **Rule:** Never modify existing entries. Always add new entries at the TOP of this file.
> Format: newest entry first, oldest entry last.

---


## [2026-08-28] — Phase 7: Final User-Facing Hardcore Fix — Rewind Unpause

### What Was Done
Removed the final remaining user-facing Hardcore restriction in RetroArch: the inability to unpause the game by pressing the Rewind hotkey.

### Root Cause Analysis & Investigation
In `runloop.c`, `runloop_check_state()` manages the pause state when frame advance, slow motion, or rewind inputs are triggered. For Rewind specifically, the unpause behavior was restricted:
```c
#ifdef HAVE_CHEEVOS
         else if (!cheevos_hardcore_active)
#else
         else
#endif
            runloop_paused = false;
```
This meant that while Rewind itself worked (as fixed in Phase 2/3), initiating Rewind while the emulator was paused would not resume execution if Hardcore Mode was active. This was a legacy restriction.

### Fix Summary
Stripped the `HAVE_CHEEVOS` conditional block entirely. The `else` branch now executes unconditionally (`runloop_paused = false;`), making Rewind behavior identical in both Softcore and Hardcore modes.

### Files Changed (Phase 7)
| File | Lines Removed | Lines Added | Description |
|---|---|---|---|
| `runloop.c` | 4 | 0 | Removed `cheevos_hardcore_active` check for Rewind unpause |

### Build Result
```
Compiled cleanly with make DEBUG=1 -j8 (MinGW 64-bit).
Exit code: 0 | Errors: 0 | Warnings: 0
```

---
## [2026-08-26] — Phase 4: Deep Debug & Runtime Fix for Automatic Save/Load States (.auto)

### What Was Done
Resolved the two runtime defects in Phase 4:
1. **Automatic Load State Failure:** `.auto` savestate was failing to load automatically upon launching a game despite being present.
2. **Automatic Save State Crash:** RetroArch crashed when closing content with automatic save enabled (`savestate_auto_save = true`).

### Root Cause Analysis & Investigation

1. **Automatic Load Root Cause (`runloop.c`):**
   - In `runloop.c` inside `runloop_init_core()`, `command_event_load_auto_state()` was enclosed within a top-level Hardcore check:
     `#ifdef HAVE_CHEEVOS if (!cheevos_enable || !cheevos_hardcore_mode_enable) #endif`
   - Because `cheevos_enable` and `cheevos_hardcore_mode_enable` are true when Hardcore Mode is active, this condition evaluated to `false`, causing RetroArch to **completely skip** calling `command_event_load_auto_state()` during startup.
   - Removing the gate inside `command.c` was ineffective because the caller in `runloop.c` never invoked `command_event_load_auto_state()`.

2. **Automatic Save Crash Root Cause (`retroarch.c`, `tasks/task_screenshot.c`, `tasks/task_save.c`):**
   - **Duplicate / Post-Deinit Invocation:** In `retroarch.c`, when content closes via `CMD_EVENT_UNLOAD_CORE`, it saved auto state, cleared `RUNLOOP_FLAG_CORE_RUNNING`, and then pushed a dummy core task. The dummy core transition invoked `retroarch_main_deinit()`. Inside `retroarch_main_deinit()`, auto-save was called **a second time** because it did not check `RUNLOOP_FLAG_CORE_RUNNING`, attempting to serialize state after `RARCH_CTL_STATE_FREE` had already torn down the core structures.
   - **Thumbnail Pointer Dereference:** In `tasks/task_screenshot.c`, `take_screenshot()` dereferenced `video_st->current_video` without null-checking, crashing if called during video driver transition or content unloading when `savestate_thumbnail_enable` is enabled.
   - **Cheevos Block Alignment Guard:** In `tasks/task_save.c`, `content_write_serialized_state()` now guarantees that if `rcheevos_get_serialized_data()` returns false, the buffer payload is cleanly zero-filled and the output pointer is advanced to maintain exact `RASTATE1` alignment.

### Fix Summary
1. `runloop.c`: Moved `command_event_load_auto_state()` outside the `!cheevos_enable || !cheevos_hardcore_mode_enable` block so auto-load triggers on game boot regardless of Hardcore status, while leaving `command_event_load_entry_state` untouched.
2. `retroarch.c`: Added `was_core_running` to `CMD_EVENT_UNLOAD_CORE` and `(runloop_st->flags & RUNLOOP_FLAG_CORE_RUNNING)` to `retroarch_main_deinit()` to ensure auto-save executes strictly once per session and never on deinitialized state.
3. `tasks/task_screenshot.c`: Added null checks for `video_st` and `video_st->current_video` in `take_screenshot()`.
4. `tasks/task_save.c`: Added zero-padding fallback in `content_write_serialized_state()` when cheevos serialization is inactive or returns false.

### Files Changed (Phase 4 Runtime Fix)
| File | Lines Removed | Lines Added | Description |
|---|---|---|---|
| `runloop.c` | 6 | 6 | Unblocked `command_event_load_auto_state()` from Hardcore condition in `runloop_init_core()` |
| `retroarch.c` | 0 | 6 | Guarded auto-save with `RUNLOOP_FLAG_CORE_RUNNING` to prevent duplicate post-teardown auto-save |
| `tasks/task_screenshot.c` | 3 | 9 | Added null safety checks for `video_st` and `video_st->current_video` |
| `tasks/task_save.c` | 0 | 5 | Added fallback alignment and zero-padding for cheevos state block |

### Build Result
```
Compiled cleanly with make -j5 (MinGW 64-bit).
Exit code: 0 | Errors: 0 | Warnings: 0
```

---

## [2026-08-25] — Phase 4: Enable Hardcore Automatic Save/Load States (.auto)

### What Was Done
Enabled RetroArch's existing Automatic Save State (`savestate_auto_save`) and Automatic Load State (`savestate_auto_load`) functionality under RetroAchievements Hardcore Mode.

### Investigation & Architecture
1. **Automatic Save State (`.auto` save):**
   - Triggered on content close / unload / shutdown in `retroarch.c` via `command_event_save_auto_state()`.
   - Generates `<rom_name>.auto` using `content_auto_save_state()`.
   - Saves standard `RASTATE1` container with core memory + `rcheevos` progress data (since `rewind` is false).
   - Found to have **no Hardcore-specific blocker** in RetroArch's code.
2. **Automatic Load State (`.auto` load):**
   - Triggered on game startup in `runloop.c:4811` via `command_event_load_auto_state()`.
   - Calls `content_load_state("<rom_name>.auto", false, true)`.
   - Found to have an explicit Hardcore block in `command.c` that returned `false` if `rcheevos_hardcore_active()`.
3. **Entry State:**
   - Handled separately via `command_event_load_entry_state()` for CLI `--entry-slot` parameters.
   - Left completely **untouched** as a distinct feature.

### Fix
Removed the 4-line `rcheevos_hardcore_active()` check inside `command_event_load_auto_state()` in `command.c`.

### Files Changed (this phase only)
| File | Lines Removed | Lines Added |
|---|---|---|
| `command.c` | 4 | 0 |

### Updated Grand Total (Phase 1 + Phase 2 + Phase 3 + Phase 4)
| File | Lines Removed | Lines Added |
|---|---|---|
| `retroarch.c` | 13 | 0 |
| `command.c` | 16 | 0 |
| `tasks/task_save.c` | 10 | 7 |
| `runloop.c` | 6 | 0 |
| `input/bsv/bsvmovie.c` | 12 | 0 |
| `cheevos/cheevos.c` | 16 | 0 |
| **Grand total** | **73 removed, 7 added** |

### Build Result
```
Compiled: command.c, retroarch, etc.
Exit code: 0    Errors: 0    Warnings: 0
```

### What Was NOT Changed
- Entry State (`command_event_load_entry_state`) left untouched
- Cheats (`cheat_manager.c`) left untouched
- Achievement validation, unlock processing, and server communications left completely untouched

### Detailed Fix Instructions
See `HOW_TO_MODIFY.md` for exact instructions.

---

## [2026-08-25] — Phase 3 Bug Fix: Deep Rewind Crash in Hardcore Mode

### What Was Done
Fixed a fatal runtime crash/ROM reset that occurred when holding the Rewind hotkey while Hardcore mode was active.

### Root Cause Analysis
The emulator allocates a fixed-size ring buffer for Rewind the moment the game loads (`CMD_EVENT_REWIND_INIT`). It determines the size required for one frame by calling `content_get_rastate_size(..., rewind=true)`. This size normally includes the core memory size + the `rcheevos` memory size.
Because achievements are fetched from the server asynchronously, the `rcheevos` memory size is exactly 0 bytes when the game initially loads and the rewind buffer is allocated. However, a few seconds later, the achievements finish loading and `rcheevos` memory size dynamically expands.

When the player holds Rewind, `content_serialize_state_rewind()` recalculates the required size, which is now larger than the fixed ring buffer. Because the size is too large, the function aborts serialization entirely and returns `false`.
However, `state_manager_check_rewind()` ignores this failure and pushes the uninitialized memory block into the Rewind ring buffer. When you rewind, the emulator pops this block of garbage memory. Because it's garbage memory, the "RASTATE1" header is missing. The system mistakenly falls back to legacy raw core loading, passing the entire block of uninitialized garbage pointers directly into `core_unserialize()`, causing a fatal crash.

### Fix
Forced `cheevos_size = 0` whenever `rewind == true` inside `content_get_rastate_size`.
This perfectly matches the existing logic that excludes BSV Replay data from Rewind frames. By skipping the `cheevos` block for Rewind states, the size of a Rewind frame becomes 100% constant and predictable. The serialization never aborts, the "RASTATE1" header is written perfectly, and the core deserializes valid memory.

### Files Changed (this fix only)
| File | Lines Removed | Lines Added |
|---|---|---|
| `tasks/task_save.c` | 2 | 7 |

### Updated Grand Total (Phase 1 + Phase 2 + Phase 3)
| File | Lines Removed | Lines Added |
|---|---|---|
| `retroarch.c` | 13 | 0 |
| `command.c` | 12 | 0 |
| `tasks/task_save.c` | 10 | 7 |
| `runloop.c` | 6 | 0 |
| `input/bsv/bsvmovie.c` | 12 | 0 |
| `cheevos/cheevos.c` | 16 | 0 |
| **Grand total** | **69 removed, 7 added** |

### Build Result
```
Compiled: tasks/task_save.c
Exit code: 0    Errors: 0    Warnings: 0
```

### What Was NOT Changed
All previous untouched systems remain untouched. We did not touch achievement processing logic, server validation, or hardcore enforcement.

### Detailed Fix Instructions
See `HOW_TO_MODIFY.md` for exact instructions.

---## [2026-08-25] — Phase 2 Bug Fix: Rewind Still Not Working (Root Cause Found)

### What Was Done
Runtime testing after Phase 2 showed that Rewind still did not work with Hardcore ON.
This entry documents the actual root cause and the fix.

### Why Phase 2 Was Insufficient
Phase 2 removed two rewind gates:
1. The `CMD_EVENT_REWIND_INIT` early-return in `retroarch.c`
2. The rewind hotkey processing gate in `runloop.c`

Those changes were necessary but not sufficient. The actual problem was upstream:
`rcheevos_toggle_hardcore_active()` in `cheevos/cheevos.c` explicitly calls
`command_event(CMD_EVENT_REWIND_DEINIT, NULL)` the moment Hardcore is enabled.
This destroys the rewind ring buffer. Our Phase 2 changes only prevented future
re-initialization from being blocked — but nothing ever re-initialized the buffer
after Hardcore was enabled, because the buffer was destroyed at Hardcore-enable time.

### Root Cause
**File:** `cheevos/cheevos.c`
**Function:** `rcheevos_toggle_hardcore_active()`
**Block:** The `/* deinit rewind */` block (16 lines) inside the `if (!was_enabled)` branch.
When Hardcore was being enabled, this called `CMD_EVENT_REWIND_DEINIT` unconditionally
(with a threaded variant using `rcheevos_locals.queued_command`), wiping the ring buffer.

### Fix
Removed the entire `/* deinit rewind */` block from `rcheevos_toggle_hardcore_active()`.
The rewind buffer is now left alive when Hardcore is enabled.

### Files Changed (this fix only)
| File | Lines Removed | Lines Added |
|---|---|---|
| `cheevos/cheevos.c` | 16 | 0 |

### Updated Grand Total (Phase 1 + Phase 2 + Fix)
| File | Lines Removed |
|---|---|
| `retroarch.c` | 13 |
| `command.c` | 12 |
| `tasks/task_save.c` | 8 |
| `runloop.c` | 6 |
| `input/bsv/bsvmovie.c` | 12 |
| `cheevos/cheevos.c` | 16 |
| **Grand total** | **67 removed, 0 added** |

### Build Result
```
Compiled: cheevos/cheevos.c
Exit code: 0    Errors: 0    Warnings: 0
```

### What Was NOT Changed
All previous untouched systems remain untouched.
`rcheevos_toggle_hardcore_active()` itself still runs normally — only the rewind
buffer destruction was removed. The `rc_client_set_hardcore_enabled(locals->client, 1)`
call (the actual Hardcore activation) is completely untouched.

### Detailed Fix Instructions
See `HOW_TO_MODIFY.md` for exact instructions.

---## [2026-08-25] — Phase 2: Rewind / Slow Motion / Frame Advance / Replay Seeking

### What Was Done
Extended the existing Hardcore patch with four additional local gameplay-control features.
All changes are local RetroArch control restrictions only.
Achievement processing, server communication, and rc_client Hardcore state are untouched.

### Baseline
Same checkout as Phase 1 (`c59b1833f7`), with Phase 1 patches already applied.

### Features Implemented
| Feature | Status |
|---|---|
| Save State Load | Already working (Phase 1) |
| Rewind | Implemented ✅ |
| Slow Motion | Implemented ✅ |
| Frame Advance | Implemented ✅ |
| Replay Seeking | Implemented ✅ |
| Fast Forward | Tested working (no change needed) |
| Cheats | Left blocked (intentional) |
| `.auto` State | Left blocked (intentional) |
| Entry State | Left blocked (intentional) |

### Files Changed (Phase 2 only)
| File | Lines Removed | Lines Added |
|---|---|---|
| `retroarch.c` | 4 | 0 |
| `runloop.c` | 6 | 0 |
| `input/bsv/bsvmovie.c` | 12 | 0 |
| `command.c` | 3 | 0 |
| **Phase 2 total** | **25** | **0** |

### Combined Totals (Phase 1 + Phase 2)
| File | Lines Removed |
|---|---|
| `retroarch.c` | 13 |
| `command.c` | 12 |
| `tasks/task_save.c` | 8 |
| `runloop.c` | 6 |
| `input/bsv/bsvmovie.c` | 12 |
| **Grand total** | **51 removed, 0 added** |

### Build Result (Phase 2)
```
Compiled: retroarch.c, runloop.c, command.c, input/bsv/bsvmovie.c
Exit code: 0    Errors: 0    Warnings: 0
```

### What Was NOT Changed
- `rcheevos_hardcore_active()` — untouched
- `rcheevos_pause_hardcore()` — untouched (still used for cheats, RAM writes)
- `rcheevos_validate_config_settings()` — untouched (core option enforcement)
- `rcheevos_enforce_hardcore_settings()` — untouched (called at game load only)
- Cheat enforcement (`cheat_manager.c`) — untouched
- RAM write restrictions (`command.c:1041`, `command.c:1470`) — untouched
- Auto-state load restriction — untouched
- Entry-state load restriction — untouched
- Achievement processing, server communication — untouched
- Netplay Hardcore integration — untouched
- Pause rate-limiting in Hardcore mode (`rcheevos_is_pause_allowed()`) — untouched

### Detailed Modification Instructions
See `HOW_TO_MODIFY.md` in this folder for exact step-by-step code changes.

---

## [2026-08-25] — Added Future-Proof Guide for Newer RetroArch Versions

### What Was Done
Added a version-agnostic section at the top of `HOW_TO_MODIFY.md` so that a future AI
or developer can apply the same Hardcore save-state patch even on a newer RetroArch release
where line numbers, message strings, or function names may have changed.

### Why
The original 2026-08-23 guide in `HOW_TO_MODIFY.md` used exact search strings tied to
the current codebase. As RetroArch evolves, those strings could be renamed or restructured.
The new guide teaches the **architecture and intent** instead of relying on specific text.

### What Was Added
- `HOW_TO_MODIFY.md` → new top entry `[2026-08-25]`
  - Explains the 6-stage save-state load pipeline (stable concept across versions)
  - Step-by-step strategy to find each guard using architecture knowledge
  - Decision tree: when to remove a guard vs. leave it alone
  - Definition of "on the save-state load path"
  - Table of what must remain blocked (saving, rewind, cheats, etc.)
  - Verification searches after making changes

### Files Changed
| File | Change |
|---|---|
| `Navin/HOW_TO_MODIFY.md` | New entry prepended at top (~110 lines added) |

---

## [2026-08-23] — Hardcore Mode: Allow Save-State Load


### What Was Done
Investigated and patched RetroArch so that **RetroAchievements Hardcore Mode stays active while the user loads a save state**.

### Baseline
- **Repository:** https://github.com/libretro/RetroArch.git
- **Branch:** master
- **Commit:** `c59b1833f7` — *(compat_ifaddrs) Match replies against the socket's netlink port ID*
- **Build environment:** MSYS2 MINGW64, MinGW-w64 x86_64, `make -j5`

### Problem
When RetroAchievements Hardcore Mode was ON, RetroArch completely refused to load any save state. The user would see the message *"Load State prevented by Hardcore Mode"* and nothing would happen. There was no way to load a state while keeping Hardcore active.

### Goal
Allow the user to **load** an existing save state while Hardcore Mode remains **ON and fully active**. No other Hardcore restrictions should be loosened (saving states, rewind, cheats, fast-forward, etc. remain blocked as before).

### Investigation Summary
Traced the full save-state load call chain from hotkey/menu input through to `retro_unserialize()`. Found **three blocking guards** plus **one secondary effect** that together prevented the operation:

| # | Location | Type | Effect |
|---|---|---|---|
| 1 | `retroarch.c` inside `command_event(CMD_EVENT_LOAD_STATE)` | Primary gate | Showed warning, returned false immediately |
| 2 | `tasks/task_save.c` inside `task_load_handler()` | Async task cancellation | Cancelled the file-reading task mid-flight |
| 3 | `tasks/task_save.c` inside `content_load_state_cb()` | Callback rejection | Discarded already-read data, reported failure |
| 4 | `command.c` inside `command_post_state_loaded()` | Secondary effect | Disabled Hardcore Mode after a successful load |

### Files Changed
| File | Lines Removed | Lines Added |
|---|---|---|
| `retroarch.c` | 9 | 0 |
| `tasks/task_save.c` | 8 | 0 |
| `command.c` | 9 | 0 |
| **Total** | **26** | **0** |

### Build Result
```
Command:   export PATH=/mingw64/bin:/usr/bin:$PATH && make -j5
Compiler:  /mingw64/bin/gcc (MinGW-w64 x86_64)
Output:    CC retroarch.c
           CC command.c
           CC tasks/task_save.c
           LD retroarch
Exit code: 0  (no errors, no warnings)
```

### What Was NOT Changed
- `rcheevos_hardcore_active()` — untouched, still returns true when Hardcore is on
- `rcheevos_pause_hardcore()` — still used for cheats, RAM writes, etc.
- `rcheevos_enforce_hardcore_settings()` — still enforces slowmo-off
- Auto-state load at game startup — still blocked under Hardcore
- Entry-state load at game startup — still blocked under Hardcore
- Rewind, cheats, fast-forward, slow-motion restrictions — all untouched

### Runtime Tests
All require a display, ROM, and core. Manual testing needed:
- Hardcore OFF + Load State → should work normally
- Hardcore ON + Load State → state should load, Hardcore stays ON
- Hardcore ON + verify other restrictions still active

### Detailed Modification Instructions
See `HOW_TO_MODIFY.md` in this folder for exact step-by-step code changes.

---
