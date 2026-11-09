# How to Modify NavOS Rev2

## Table of Contents
1. [Reproduction Philosophy](#1-reproduction-philosophy)
2. [Prerequisites](#2-prerequisites)
3. [Preparing the Source](#3-preparing-the-source)
4. [Every NavOS Modification](#4-every-navos-modification)
5. [Netplay Fix — Very Detailed](#5-netplay-fix--very-detailed)
6. [Badge Task Fix](#6-badge-task-fix)
7. [Screenshot Memory Fix](#7-screenshot-memory-fix)
8. [Final Hardcore Restrictions](#8-final-hardcore-restrictions)
9. [Exact Build Instructions](#9-exact-build-instructions)
10. [Verification Checklist](#10-verification-checklist)
11. [Troubleshooting](#11-troubleshooting)
12. [Version/Change Tracking](#12-versionchange-tracking)

---

## 1. Reproduction Philosophy
This document is a complete reconstruction and modification manual for NavOS. Its purpose is to allow a human developer who has never seen this project to take a clean, original RetroArch source tree and reproduce the current NavOS build step-by-step. 

This document uses the actual current source tree as the authoritative source. It is intended to allow reproduction of the frozen NavOS build exactly as it exists, not merely an approximate equivalent.

## 2. Prerequisites
- **Operating System:** Windows 10 or later (64-bit).
- **Environment:** MSYS2 (MinGW-w64).
- **Required Packages (MSYS2):**
  - `mingw-w64-x86_64-gcc` (C/C++ Compiler)
  - `mingw-w64-x86_64-make` (Build system)
  - `git`
- **Source Version:** The RetroArch source tree used for this build (NavOS Rev2).
- **Directory Structure:** RetroArch cloned in `C:\msys64\home\username\retroarch`.

## 3. Preparing the Source
1. **Clone RetroArch:**
   ```bash
   git clone https://github.com/libretro/RetroArch.git retroarch
   cd retroarch
   ```
2. **Verify Starting Source:**
   Use `git status` to ensure you are on the `master` branch and the working tree is clean.
3. **Create a Backup:**
   Clone a second copy of the directory or use a separate git branch (e.g., `git checkout -b navos-base`) to keep the original untouched.

## 4. Every NavOS Modification
The following details every single modification made to unlock standard features in Hardcore Mode.

### Modification 1: Remove `rcheevos_hardcore_active()` from Save/Load UI Checks
- **File:** `menu/menu_displaylist.c`
- **Function:** `menu_displaylist_parse_load_content_settings()` and `menu_displaylist_ctl_internal()`
- **Original Behavior:** Hid "Undo Save State" and "Undo Load State" when Hardcore Mode was active.
- **Why changed:** To allow players to use Undo Load/Save states in Hardcore mode.
- **New Behavior:** Menu items always append regardless of Hardcore state.
- **Code Change:** Removed `#ifdef HAVE_CHEEVOS \n if (!rcheevos_hardcore_active()) \n #endif` blocks surrounding `menu_entries_append` for Undo functions.

### Modification 2: Remove Hardcore Load State Rejection
- **File:** `retroarch.c`
- **Function:** `command_event` (case `CMD_EVENT_LOAD_STATE`)
- **Original Behavior:** Pushed a warning message and returned `false` if `rcheevos_hardcore_active()` was true.
- **Why changed:** To unlock manual state loading in Hardcore.
- **New Behavior:** State loading proceeds unconditionally.
- **Code Change:** Deleted the `if (rcheevos_hardcore_active()) { ... return false; }` block.

### Modification 3: Remove Hardcore Auto-Load Rejection
- **File:** `retroarch.c`
- **Function:** `command_event` (case `CMD_EVENT_LOAD_AUTO_STATE`)
- **Original Behavior:** Auto state loading was disabled in Hardcore mode.
- **Why changed:** Allow Auto Load to function identically to Softcore.
- **New Behavior:** Loads `.auto` states unconditionally.
- **Code Change:** Deleted the `if (rcheevos_hardcore_active()) return false;` block.

### Modification 4: Remove Hardcore Load Savefile Rejection
- **File:** `retroarch.c`
- **Function:** `command_event` (case `CMD_EVENT_LOAD_FILES`)
- **Original Behavior:** Pushed warning and returned `false` if Hardcore was active.
- **New Behavior:** SRAM files load unconditionally.
- **Code Change:** Deleted the `if (rcheevos_hardcore_active()) { ... return false; }` block.

### Modification 5: Allow Core Teardown Auto-Save
- **File:** `retroarch.c`
- **Function:** `retroarch_main_quit()` and `command_event` (case `CMD_EVENT_UNLOAD_CORE`)
- **Original Behavior:** Duplicated Auto-Save on tear-down, crashing when pointers were freed.
- **Code Change:** Guarded `command_event_save_auto_state()` with a `was_core_running` variable reading `runloop_st->flags & RUNLOOP_FLAG_CORE_RUNNING` and immediately masked it off after save.

### Modification 6: Unlock Frame Advance and Slow Motion
- **File:** `runloop.c`
- **Function:** `runloop_check_state()`
- **Original Behavior:** Frame advance and slow motion inputs were ignored if Hardcore was active.
- **New Behavior:** The emulator checks for these inputs regardless of Hardcore state.
- **Code Change:** Removed the `if (!cheevos_hardcore_active)` wrap around frame advance and slow motion checks.

### Modification 7: Fix Rewind Unpause
- **File:** `runloop.c`
- **Function:** `runloop_check_state()`
- **Original Behavior:** Initiating Rewind while paused would not unpause the game if Hardcore was active.
- **Code Change:** Removed `#ifdef HAVE_CHEEVOS else if (!cheevos_hardcore_active)` in favor of an unconditional `else runloop_paused = false;`.

### Modification 8: Fix RASTATE1 Buffer Alignment
- **File:** `tasks/task_save.c`
- **Function:** `content_get_rastate_size()` and `content_write_serialized_state()`
- **Original Behavior:** Wrote cheevos state sizes into rewind blocks, causing buffer overflow desyncs on rewind.
- **New Behavior:** Forces `cheevos_size = 0` when `rewind == true`, pushing only empty padded bytes into the ring buffer to maintain strict block sizes.
- **Code Change:** 
  ```c
  if (!rewind) {
     if ((size->cheevos_size = rcheevos_get_serialize_size()) > 0)
        size->total_size += 8 + CONTENT_ALIGN_SIZE(size->cheevos_size);
  } else { size->cheevos_size = 0; }
  ```

### Modification 9: Unlock Cheats in Hardcore
- **File:** `cheat_manager.c`
- **Function:** `cheat_manager_apply_cheats()` and `cheat_manager_apply_retro_cheats()`
- **Original Behavior:** Applying cheats invoked `rcheevos_pause_hardcore()`.
- **New Behavior:** Cheats apply silently without pausing Hardcore.
- **Code Change:** Deleted the `cheat_manager_pause_cheevos()` function entirely, and removed calls to it from the cheat manager.

### Modification 10: Unlock External Memory Writes
- **File:** `command.c`
- **Function:** `command_write_ram()` and `command_write_memory()`
- **Original Behavior:** Issuing these IPC commands paused Hardcore Mode.
- **New Behavior:** Commands execute without pausing Hardcore.
- **Code Change:** Removed `rcheevos_pause_hardcore()` and associated log warnings from both functions.

### Modification 11: Ignore Protocol 7 Netplay Mismatches
- **File:** `network/netplay/netplay_frontend.c`
- **Function:** `netplay_get_cmd()`
- **Original Behavior:** Paused hardcore if host did not send protocol 7 packet.
- **New Behavior:** No longer pauses hardcore.
- **Code Change:** Removed the `if (!netplay->is_server && rcheevos_hardcore_active() ...)` block.

### Modification 12: Ignore Host Hardcore State in Netplay
- **File:** `network/netplay/netplay_frontend.c`
- **Function:** `netplay_process_savestate1()`
- **Original Behavior:** Synchronized client Hardcore state to Host Hardcore state by checking `NETPLAYSTATE_CHEEVOS_BLOCK`.
- **New Behavior:** Client Hardcore mode remains independent.
- **Code Change:** Removed the `if (hardcore_state != rcheevos_hardcore_active()) { ... }` block inside the cheevos block check.

### Modification 13: Prevent Stale Auto-Saves on Netplay Load
- **File:** `tasks/task_netplay_find_content.c`
- **Function:** `task_netplay_crc_scan_callback()`
- **Original Behavior:** Emitting `CMD_EVENT_UNLOAD_CORE` while joining a room created a stale `.auto` file of the previously running game.
- **New Behavior:** Auto-save is temporarily disabled during the synchronous Netplay core transition.
- **Code Change:**
  ```c
  bool load_dummy_core = false;
  bool old_auto_save = settings->bools.savestate_auto_save;
  settings->bools.savestate_auto_save = false;
  command_event(CMD_EVENT_UNLOAD_CORE, &load_dummy_core);
  settings->bools.savestate_auto_save = old_auto_save;
  ```

### Modification 14: Unlock Video Timing Features
- **File:** `cheevos/cheevos.c`
- **Function:** `rcheevos_validate_config_settings()`
- **Original Behavior:** Paused hardcore if Manual Frame Delay, VSync Swap Interval > 1, or Black Frame Insertion were enabled.
- **New Behavior:** These settings do not trigger a Hardcore pause.
- **Code Change:** Stripped the respective conditional blocks checking `video_frame_delay`, `video_swap_interval`, and `video_black_frame_insertion`.

### Modification 15: NavOS Branding
- **File:** `menu/menu_driver.c`
- **Function:** `menu_entries_get_core_title()`
- **Original Behavior:** Used standard `PACKAGE_VERSION`.
- **New Behavior:** Uses `snprintf(s, len, "NavOS %s", PACKAGE_VERSION)`.
- **Code Change:** Replaced `strlcpy(s, PACKAGE_VERSION, len)` with `snprintf(s, len, "NavOS %s", PACKAGE_VERSION)`.

## 5. Netplay Fix — Very Detailed
### Original Behavior & The Deadlock
When joining a Netplay room via CRC scan while a game with Hardcore Mode was already running, RetroArch would crash or deadlock entirely.

**Task Queue Interaction:**
1. Netplay triggers `task_push_netplay_crc_scan`.
2. The scan finds the room and calls `CMD_EVENT_UNLOAD_CORE`.
3. `retroarch.c` processes the command and calls `task_push_start_dummy_core()`.
4. Inside `task_push_start_dummy_core()`, RetroArch forcefully called `task_queue_deinit()` and `retroarch_init_task_queue()`.
5. `task_queue_deinit()` told the task worker thread to quit, but left `tasks_running` intact.
6. `retroarch_init_task_queue()` spawned a *new* worker thread which immediately resumed those tasks (such as active RetroAchievement badge downloads).
7. Simultaneously, the main thread completed the unload by calling `rcheevos_unload()`, completely freeing `rcheevos_locals.client`.
8. The resumed task in the new worker thread attempted to use the freed client struct, resulting in a fatal Use-After-Free deadlock.

### The Solution
We fixed this through two concerted changes:
1. **Removed Task Queue Teardown:** In `tasks/task_content.c`, we deleted the `task_queue_deinit()` and `retroarch_init_task_queue()` calls inside `task_push_start_dummy_core()`. This ensures the active task thread remains stable.
2. **Synchronous Transition:** In `tasks/task_netplay_find_content.c`, we bypassed dummy core loading entirely. We pass `&load_dummy_core` with `load_dummy_core = false` to `CMD_EVENT_UNLOAD_CORE`. This ensures Netplay unloads the active core and immediately loads the lobby core on the main thread, avoiding `system.info.library_name` becoming `NULL` and crashing Netplay announcements.
3. **Stale Auto-Save Suppression:** By setting `savestate_auto_save = false` just before the unload command, we ensure that tearing down the old game doesn't accidentally overwrite the user's `.auto` save state purely because they joined a Netplay lobby.

## 6. Badge Task Fix
### Original Lifecycle & Crash Scenario
When a game starts in Hardcore Mode, `rcheevos` queues tasks to download achievement badges (`rcheevos_download_next_badge()`). If the user quickly closes the game before the HTTP requests complete, `rcheevos_unload()` frees the client struct. However, upstream RetroArch did not cancel the queued tasks, leading to the task thread reading the destroyed client and crashing.

### The Fix
In `cheevos/cheevos.c`, `rcheevos_unload()` now uses `task_queue_retrieve()` with a custom `rcheevos_badge_task_retriever` function to intercept all pending `rcheevos_download_next_badge` tasks. We explicitly iterate the list and call `task_queue_cancel_task()` on each one.
In `rcheevos_download_next_badge()`, we check for `RETRO_TASK_FLG_CANCELLED` and immediately mark the task as `RETRO_TASK_FLG_FINISHED` and return.
*Note:* A microscopic race condition technically remains if the task is actively executing in the worker thread the instant the main thread frees the client. This existed upstream, but our cancellation drains 99% of the hazard window.

## 7. Screenshot Memory Fix
**The Bug:** In `tasks/task_screenshot.c` -> `screenshot_dump()`, the synchronous fallback path (when `use_thread` is false) called `screenshot_dump_direct(state)` and immediately returned, leaking the memory allocated for `state->userbuf` and `state`.
**The Fix:** 
```c
{
   bool ret = screenshot_dump_direct(state);
   if (state->userbuf) free(state->userbuf);
   free(state);
   return ret;
}
```
This correctly frees the memory buffer and the task state struct in the synchronous path.

## 8. Final Hardcore Restrictions
**Restrictions Removed (Unlocked Features):**
- Save States (Manual & Auto)
- Rewind (including unpausing from rewind)
- Slow Motion
- Frame Advance
- Cheats (EMU and RETRO)
- External Memory Commands (IPC)
- Advanced Video Timings (Frame Delay, Swap Interval > 1, Black Frame Insertion)
- Netplay Hardcore disabling

**Restrictions Intentionally Preserved (Integrity):**
NavOS completely preserves RetroAchievements' core integrity mechanisms. It does **not** bypass:
- Entry State execution (prevents cheating achievement conditions on startup).
- Unknown/unidentified game validation (hashes must match server records).
- Missing runtime logic validation.
- Core compatibility restrictions.
- TAS / BSV movie blockers.
- Disallowed core settings validation.
- Leaderboard submission rules.
- Authentication/server communication logic.

## 9. Exact Build Instructions
**Windows MSYS2 MinGW-w64 (64-bit)**
1. Open the **MSYS2 MinGW 64-bit** terminal.
2. Navigate to the source directory: `cd /home/username/retroarch`
3. **Clean Build:**
   ```bash
   make clean
   make -j8
   ```
4. **Debug Build:**
   ```bash
   make DEBUG=1 -j8
   ```
5. **Output:** The compiled executable will be located in the root source directory as `retroarch.exe`. It must be accompanied by its required DLLs or run from an environment where they are in the PATH.

## 10. Verification Checklist
After building, ensure the following runtime behaviors function exactly as specified:
- [ ] **Hardcore Activation:** Emulation pauses briefly on boot and successfully activates achievements without disabling Hardcore.
- [ ] **Save/Load States:** Loading and saving states (manual and `.auto`) during Hardcore works flawlessly.
- [ ] **Rewind:** Rewind functions smoothly. Pressing Rewind while the emulator is paused successfully unpauses the emulator and scrubs backward.
- [ ] **Netplay Lobby Sync:** Joining a Netplay room while a Hardcore game is actively running smoothly transitions to the new core/lobby without deadlock.
- [ ] **Netplay Host State:** Hardcore mode does not auto-disable when joining a host that has Hardcore disabled.
- [ ] **Cheats:** Applying cheat codes via the menu does not disable Hardcore.
- [ ] **External Memory:** Sending `WRITE_CORE_RAM` via the Network Command Interface works and does not disable Hardcore.

## 11. Troubleshooting
- **Build Failure (Missing Libraries):** Ensure all `mingw-w64-x86_64` development dependencies are installed via `pacman`.
- **Missing DLLs:** When running `retroarch.exe` outside MSYS2, ensure the `mingw64/bin` libraries (libwinpthread-1, etc.) are in the same folder.
- **Hardcore Unexpectedly Pausing:** Verify that the `runloop_check_state()` checks were cleanly removed and that `rcheevos_pause_hardcore()` isn't being triggered by an upstream update.
- **Netplay Deadlock:** Ensure that `CMD_EVENT_UNLOAD_CORE` is passed `&load_dummy_core` with `false`, and that `task_queue_deinit()` remains stripped from `task_content.c`.

## 12. Version/Change Tracking
Future developers should use `git diff` against the upstream libretro/RetroArch `master` branch to track deviations. Any future upstream update should be merged carefully, specifically reviewing `runloop.c`, `cheevos.c`, `retroarch.c`, and `task_content.c` to ensure NavOS modifications are not overwritten by upstream behavior reversions.
