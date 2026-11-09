# HOW TO MODIFY — RetroArch Hardcore Save-State Patch
> **Rule:** Never modify existing entries. Always add new entries at the TOP of this file.
> Each entry is a self-contained, human-readable guide to reproduce one set of changes.

---


## [2026-08-28] — Phase 7: Final User-Facing Hardcore Fix — Rewind Unpause

### Overview
This final step removes the last remaining user-facing restriction imposed by Hardcore Mode: the inability to unpause the emulator by holding the Rewind hotkey.

### Prerequisites
- RetroArch source with Phases 1 through 6 applied.

---

### Change 24 — Remove Rewind Unpause Restriction

**File:** `runloop.c`

**How to find it:** Search for `runloop_paused = false;` inside the `runloop_check_state()` function where input triggers are processed (near `if (!frameadvance_trigger)` and `focused = false`).

**What to do:** Remove the `#ifdef HAVE_CHEEVOS` block that checks `!cheevos_hardcore_active`.

**Before:**
```c
         if (!frameadvance_trigger)
#endif
            focused = false;
#ifdef HAVE_CHEEVOS
         else if (!cheevos_hardcore_active)
#else
         else
#endif
            runloop_paused = false;
```

**After:**
```c
         if (!frameadvance_trigger)
#endif
            focused = false;
         else
            runloop_paused = false;
```

---
## [2026-08-26] — Phase 4: Deep Debug & Runtime Fix for Automatic Save/Load States (.auto)

### Overview
This step fixes the two runtime issues encountered during Phase 4:
1. Auto-load failing on startup because `runloop.c` had a Hardcore gate wrapping `command_event_load_auto_state()`.
2. Auto-save crashing on close content because `retroarch_main_deinit()` triggered duplicate serialization on torn-down core state, `take_screenshot()` lacked video null checks, and `tasks/task_save.c` needed clean fallback alignment.

### Prerequisites
- RetroArch source with Phases 1, 2, 3, and initial Phase 4 applied.

---

### Change 16 — Unblock Automatic Savestate Loading in Core Runloop

**File:** `runloop.c`

**How to find it:** Search for:
```c
/*
 * Since the operations are asynchronous we can't
 * guarantee users will not use auto_load_state to cheat on
 * achievements so we forbid auto_load_state from happening
 * if cheevos_enable and cheevos_hardcode_mode_enable
 * are true.
 */
```

**What to do:**
Close the `#ifdef HAVE_CHEEVOS if (!cheevos_enable || !cheevos_hardcore_mode_enable)` block after `command_event_load_entry_state`, so `command_event_load_auto_state()` runs unconditionally for both Hardcore ON and OFF.

**Before:**
```c
#ifdef HAVE_CHEEVOS
   if (     !cheevos_enable
         || !cheevos_hardcore_mode_enable)
#endif
   {
#ifdef HAVE_BSV_MOVIE
      /* Ignore entry state if we're doing bsv playback (we do want it
         for bsv recording though) */
      if (!(input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_START_PLAYBACK))
#endif
      {
         if (     entry_state_load
               && !command_event_load_entry_state(settings))
         {
            /* Loading the state failed, reset entry slot */
            runloop_st->entry_state_slot = -1;
         }
      }

#ifdef HAVE_BSV_MOVIE
      /* Ignore autoload state if we're doing bsv playback or recording */
      if (!(input_st->bsv_movie_state.flags & (BSV_FLAG_MOVIE_START_RECORDING | BSV_FLAG_MOVIE_START_PLAYBACK)))
#endif
      {
         if (     runloop_st->entry_state_slot < 0
               && settings->bools.savestate_auto_load)
            command_event_load_auto_state();
      }
   }
```

**After:**
```c
#ifdef HAVE_CHEEVOS
   if (     !cheevos_enable
         || !cheevos_hardcore_mode_enable)
#endif
   {
#ifdef HAVE_BSV_MOVIE
      /* Ignore entry state if we're doing bsv playback (we do want it
         for bsv recording though) */
      if (!(input_st->bsv_movie_state.flags & BSV_FLAG_MOVIE_START_PLAYBACK))
#endif
      {
         if (     entry_state_load
               && !command_event_load_entry_state(settings))
         {
            /* Loading the state failed, reset entry slot */
            runloop_st->entry_state_slot = -1;
         }
      }
   }

#ifdef HAVE_BSV_MOVIE
   /* Ignore autoload state if we're doing bsv playback or recording */
   if (!(input_st->bsv_movie_state.flags & (BSV_FLAG_MOVIE_START_RECORDING | BSV_FLAG_MOVIE_START_PLAYBACK)))
#endif
   {
      if (     runloop_st->entry_state_slot < 0
            && settings->bools.savestate_auto_load)
         command_event_load_auto_state();
   }
```

---

### Change 17 — Prevent Duplicate and Post-Deinit Auto-Save on Close Content

**File:** `retroarch.c`

**How to find it:**
1. Search for `case CMD_EVENT_UNLOAD_CORE:` inside `command_event()`.
2. Search for `bool retroarch_main_quit(void)` or `retroarch_main_deinit`.

**In `CMD_EVENT_UNLOAD_CORE`:**
Record whether the core was active before clearing `RUNLOOP_FLAG_CORE_RUNNING` and only save auto-state if `was_core_running`:
```c
            bool was_core_running           = (runloop_st->flags & RUNLOOP_FLAG_CORE_RUNNING);

            runloop_st->flags              &= ~RUNLOOP_FLAG_CORE_RUNNING;

            /* The platform that uses ram_state_save calls it when the content
             * ends and writes it to a file */
            ram_state_to_file();

            /* Save auto state */
            if (     settings->bools.savestate_auto_save
                  && was_core_running
                  && runloop_st->current_core_type != CORE_TYPE_DUMMY)
               command_event_save_auto_state();
```

**In `retroarch_main_deinit`:**
Guard `command_event_save_auto_state()` with `RUNLOOP_FLAG_CORE_RUNNING` and clear the flag immediately after saving:
```c
   if (!(runloop_st->flags & RUNLOOP_FLAG_SHUTDOWN_INITIATED))
   {
      if (   settings->bools.savestate_auto_save
          && (runloop_st->flags & RUNLOOP_FLAG_CORE_RUNNING)
          && runloop_st->current_core_type != CORE_TYPE_DUMMY)
      {
         command_event_save_auto_state();
         runloop_st->flags &= ~RUNLOOP_FLAG_CORE_RUNNING;
      }
```

---

### Change 18 — Add Video Driver Null Checks in `take_screenshot`

**File:** `tasks/task_screenshot.c`

**How to find it:** Search for:
```c
bool take_screenshot(
```

**What to add:**
Before accessing `video_st->current_video->read_viewport`, add:
```c
   if (!video_st || !video_st->current_video)
      return false;
```

---

### Change 19 — Fallback Alignment in Cheevos State Block Serialization

**File:** `tasks/task_save.c`

**How to find it:** Search for `RASTATE_CHEEVOS_BLOCK` inside `content_write_serialized_state()`.

**What to change:**
If `rcheevos_get_serialized_data()` returns false, zero-fill the padding and advance `output` so container block alignment remains exact:
```c
#ifdef HAVE_CHEEVOS
   if (size->cheevos_size)
   {
      content_write_block_header(output,
            RASTATE_CHEEVOS_BLOCK, size->cheevos_size);
      if (rcheevos_get_serialized_data(output + 8))
      {
         CONTENT_ZERO_PADDING(output + 8, size->cheevos_size);
         output += CONTENT_ALIGN_SIZE(size->cheevos_size) + 8;
      }
      else
      {
         memset(output + 8, 0, CONTENT_ALIGN_SIZE(size->cheevos_size));
         output += CONTENT_ALIGN_SIZE(size->cheevos_size) + 8;
      }
   }
#endif
```

---

## [2026-08-25] — Phase 4: Enable Hardcore Automatic Save/Load States (.auto)

### Overview
This enables RetroArch's existing Automatic Save State (`savestate_auto_save`) and Automatic Load State (`savestate_auto_load`) features while Hardcore Mode is active.
Automatic Save State had no blocker in code. Automatic Load State was blocked by an explicit Hardcore check in `command.c`. This guide removes that restriction.

### Prerequisites
- RetroArch source at commit `c59b1833f7` with Phases 1, 2, and 3 applied
- A text editor capable of searching in files

---

### Change 15 of 8+3+1+1+1 — Remove Hardcore Gate from Auto State Loading

**File:** `command.c`

**How to find it:** Search for:
```
bool command_event_load_auto_state(void)
```
Inside this function, find the Hardcore check:
```c
#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active())
      return false;
#endif
```

**What to delete (4 lines):**
```c
#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active())
      return false;
#endif
```

**Before (what you see):**
```c
bool command_event_load_auto_state(void)
{
   size_t _len;
   char savestate_name_auto[PATH_MAX_LENGTH];
   runloop_state_t *runloop_st     = runloop_state_get_ptr();
   const char *name_savestate      = runloop_st->name.savestate;
   bool ret                        = false;

   /* No early save-capability gate here: content_load_state() decides,
    * and an existing .auto state file outranks stale metadata. */

#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active())
      return false;
#endif
#ifdef HAVE_NETWORKING
   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
      return false;
#endif
```

**After (what it should look like):**
```c
bool command_event_load_auto_state(void)
{
   size_t _len;
   char savestate_name_auto[PATH_MAX_LENGTH];
   runloop_state_t *runloop_st     = runloop_state_get_ptr();
   const char *name_savestate      = runloop_st->name.savestate;
   bool ret                        = false;

   /* No early save-capability gate here: content_load_state() decides,
    * and an existing .auto state file outranks stale metadata. */

#ifdef HAVE_NETWORKING
   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_ENABLED, NULL))
      return false;
#endif
```

**Why:** Hardcore mode prohibited automatic state loading on content boot. Removing this gate allows RetroArch's native `.auto` loading path to function identically to the manual state loading path we enabled in Phase 1, keeping Hardcore mode active and preserving achievement unlocking.

---

### Verification After Phase 4

Run this search and confirm:

```
Search: "rcheevos_hardcore_active" in command_event_load_auto_state in command.c
   Expected: 0 hits
```

---

### Build After Phase 4

```bash
# MSYS2 MINGW64 shell
export PATH=/mingw64/bin:/usr/bin:$PATH
make -j5
```

Expected compiled files: `command.c`
Expected: Exit code 0, no errors, no warnings.

---

## [2026-08-25] — Phase 3 Bug Fix: Deep Rewind Crash in Hardcore Mode

### Overview
This fixes a fatal runtime crash / ROM reset that occurs when Rewind is used while Hardcore Mode is active. 
The crash happens because the rewind buffer size is calculated immediately upon game load, when `rcheevos` size is 0. Once achievements load from the server, `rcheevos` size expands. The rewind serializer aborts due to a size mismatch, but the uninitialized buffer is still pushed. When the user rewinds, this garbage block is parsed as a legacy raw core state, causing a crash.
This patch bypasses `cheevos` state serialization exclusively for Rewind frames, fixing the size mismatch and stopping the crash.

### Prerequisites
- RetroArch source at commit `c59b1833f7` with previous patches in place
- A text editor capable of searching in files

---

### Change 14 of 8+3+1+1 — Fix Rewind Frame Serialization Size

**File:** `tasks/task_save.c`

**How to find it:** Search for:
```
if ((size->cheevos_size = rcheevos_get_serialize_size()) > 0)
```
You will find this inside the function `content_get_rastate_size()`. 

**What to do:** Wrap the `cheevos_size` calculation in an `if (!rewind)` check. 

**Before (what you see):**
```c
#ifdef HAVE_CHEEVOS
   /* 8-byte block header + content */
   if ((size->cheevos_size = rcheevos_get_serialize_size()) > 0)
      size->total_size += 8 + CONTENT_ALIGN_SIZE(size->cheevos_size);
#endif
```

**After (what it should look like):**
```c
#ifdef HAVE_CHEEVOS
   /* 8-byte block header + content */
   if (!rewind)
   {
      if ((size->cheevos_size = rcheevos_get_serialize_size()) > 0)
         size->total_size += 8 + CONTENT_ALIGN_SIZE(size->cheevos_size);
   }
   else
      size->cheevos_size = 0;
#endif
```

**Why:** By skipping the `cheevos` block for Rewind states, the size of a Rewind frame becomes 100% constant and predictable. The serialization never aborts, the "RASTATE1" header is written perfectly, and the core deserializes valid memory.

---

### Build After Phase 3 Bug Fix

```bash
# MSYS2 MINGW64 shell
export PATH=/mingw64/bin:/usr/bin:$PATH
make -j5
```

Expected compiled files: `tasks/task_save.c`
Expected: Exit code 0, no errors, no warnings.

---## [2026-08-25] — Phase 2 Bug Fix: Rewind Still Not Working (Root Cause Found)

### Overview
This fixes an issue where Phase 2 alone was insufficient to enable Rewind in Hardcore mode.
While Phase 2 unblocked the hotkeys and initialization gates, `cheevos/cheevos.c` was actively destroying the rewind buffer (`CMD_EVENT_REWIND_DEINIT`) the moment Hardcore mode was turned on.
This change removes that destruction block so the buffer stays alive.

### Prerequisites
- Phase 1 and Phase 2 already applied
- RetroArch source at commit `c59b1833f7` with previous patches in place
- A text editor capable of searching in files

---

### Change 13 of 8+3+1 — Remove Rewind Buffer Destruction on Hardcore Enable

**File:** `cheevos/cheevos.c`

**How to find it:** Search for:
```
/* deinit rewind */
```
You will find this inside the function `rcheevos_toggle_hardcore_active()`.
It appears inside the `if (!was_enabled)` block (which runs when Hardcore is being turned ON).

**What to delete (16 lines):**
```c
      /* deinit rewind */
      if (rewind_enable)
      {
#ifdef HAVE_THREADS
         if (!task_is_on_main_thread())
         {
            /* have to "schedule" this.
             * CMD_EVENT_REWIND_DEINIT should
             * only be called on the main thread */
            retro_atomic_store_release_int(&rcheevos_locals.queued_command,
                  CMD_EVENT_REWIND_DEINIT);
         }
         else
#endif
            command_event(CMD_EVENT_REWIND_DEINIT, NULL);
      }
```

**What to do:** Delete this entire 16-line block.

**Before (what you see):**
```c
         rcheevos_enforce_hardcore_settings();
      }

      /* deinit rewind */
      if (rewind_enable)
      {
#ifdef HAVE_THREADS
         if (!task_is_on_main_thread())
         {
            /* have to "schedule" this.
             * CMD_EVENT_REWIND_DEINIT should
             * only be called on the main thread */
            retro_atomic_store_release_int(&rcheevos_locals.queued_command,
                  CMD_EVENT_REWIND_DEINIT);
         }
         else
#endif
            command_event(CMD_EVENT_REWIND_DEINIT, NULL);
      }

      locals->hardcore_being_enabled = false;
      rc_client_set_hardcore_enabled(locals->client, 1);
```

**After (what it should look like):**
```c
         rcheevos_enforce_hardcore_settings();
      }

      locals->hardcore_being_enabled = false;
      rc_client_set_hardcore_enabled(locals->client, 1);
```

**Why:** Hardcore mode used to explicitly wipe out the rewind buffer when activating. With our previous Phase 2 patches, we allowed the rewind input to be processed, but since the buffer was wiped out and never recreated, rewind still did nothing. Removing this DEINIT block leaves the buffer fully functional.

---

### Verification After Phase 2 Bug Fix

Run these searches and confirm results:

```
Search: "/* deinit rewind */" in cheevos/cheevos.c
   Expected: 0 hits
```

---

### Build After Phase 2 Bug Fix

```bash
# MSYS2 MINGW64 shell
export PATH=/mingw64/bin:/usr/bin:$PATH
make -j5
```

Expected compiled files: `cheevos/cheevos.c`
Expected: Exit code 0, no errors, no warnings.

---

## [2026-08-25] — Phase 2: Rewind, Slow Motion, Frame Advance, Replay Seeking

### Overview
This is the Phase 2 modification guide. Phase 1 (Save State Load) must already be applied.
These changes unlock four additional local gameplay controls under Hardcore Mode.
All changes are **deletions only** — zero lines added.

### Prerequisites
- Phase 1 already applied (see the `[2026-08-23]` entry below)
- RetroArch source at commit `c59b1833f7` with Phase 1 patches already in place
- A text editor capable of searching in files

---

### Change 5 of 8 — Remove Rewind buffer allocation gate

**File:** `retroarch.c`

**How to find it:** Search for:
```
CMD_EVENT_REWIND_INIT
```
Find the `case CMD_EVENT_REWIND_INIT:` block inside `command_event()`.
Inside it, after the `if (core_type_is_dummy) return false;` line, you will see:

**What to delete:**
```c
#ifdef HAVE_CHEEVOS
            if (rcheevos_hardcore_active())
               return false;
#endif
```

**What to do:** Delete those 4 lines. The line before them is:
```c
            if (core_type_is_dummy)
               return false;
```
The line after them (keep it) is:
```c
#ifdef HAVE_NETWORKING
```

**Why:** This prevented the rewind ring buffer from being allocated at all when Hardcore is active. Without the buffer, the rewind hotkey has nothing to work with, so rewind silently does nothing.

---

### Change 6 of 8 — Remove Rewind hotkey processing gate in runloop

**File:** `runloop.c`

**How to find it:** Search for:
```
cheevos_hardcore_active = rcheevos_hardcore_active();
```
You will land in the `runloop_check_state()` function, inside a `#ifdef HAVE_CHEEVOS` block.
Immediately after that assignment line, you will see:

**What to delete (2 lines only):**
```c

   if (!cheevos_hardcore_active)
```
(That is: one blank line, then the `if (!cheevos_hardcore_active)` line.)

**What to do:** Delete those 2 lines. The assignment line before them stays. The `#endif` line after them stays.

**Before (what you see):**
```c
   cheevos_hardcore_active = rcheevos_hardcore_active();

   if (!cheevos_hardcore_active)
#endif
   {
      /* Check rewind hotkey */
```

**After (what it should look like):**
```c
   cheevos_hardcore_active = rcheevos_hardcore_active();
#endif
   {
      /* Check rewind hotkey */
```

**Why:** This `if` gate prevented the entire rewind hotkey processing block from running when Hardcore is active. Removing it makes the block unconditional — rewind input is always processed.

---

### Change 7 of 8 — Remove Slow Motion hotkey gate in runloop

**File:** `runloop.c`

**How to find it:** Search for:
```
Check slowmotion hotkeys
```
Immediately before that comment line (which is inside a block `{ {`), you will find:

**What to delete (3 lines):**
```c
#ifdef HAVE_CHEEVOS
   if (!cheevos_hardcore_active)
#endif
```

**What to do:** Delete those 3 lines. The `{` block that follows them stays as-is.

**Before:**
```c
#ifdef HAVE_CHEEVOS
   if (!cheevos_hardcore_active)
#endif
   {
      {
         /* Check slowmotion hotkeys */
```

**After:**
```c
   {
      {
         /* Check slowmotion hotkeys */
```

**Why:** This gate prevented slow-motion hotkey processing (both toggle and hold keys) from running when Hardcore is active.

---

### Change 8 of 8 — Remove Frame Advance gate in runloop

**File:** `runloop.c`

**How to find it:** Search for:
```
frame advance not allowed in hardcore
```
You will find an `else /* frame advance not allowed in hardcore */` comment immediately before `#endif`.

**What to delete (1 line only):**
```c
      else /* frame advance not allowed in hardcore */
```

**What to do:** Delete that 1 line only. Everything else stays.

**Before:**
```c
      }
      else /* frame advance not allowed in hardcore */
#endif
      {
         frameadvance_pressed = BIT256_GET(current_bits, RARCH_FRAMEADVANCE);
```

**After:**
```c
      }
#endif
      {
         frameadvance_pressed = BIT256_GET(current_bits, RARCH_FRAMEADVANCE);
```

**Why:** The `else` made the frame advance block exclusive with the Hardcore pause-rate-limiting block — only one would run. Removing `else` means both always run (the Hardcore pause throttle still runs, AND frame advance is always processed).

---

### Changes 9–11 of 8+3 — Remove Replay Seeking guards in bsvmovie.c

**File:** `input/bsv/bsvmovie.c`

**How to find it:** Search for `movie_seek_to_frame`. Inside that function, after the early `return false` for no handle, you will see:

**Identical block to delete — appears 3 times in 3 consecutive functions:**
```c
#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active())
      return false;
#endif
```

The three functions are:
1. `movie_seek_to_frame()` — seek to a specific frame number
2. `movie_skip_to_next_checkpoint()` — jump to next checkpoint
3. `movie_skip_to_prev_checkpoint()` — jump to previous checkpoint

**What to do:** Delete the above 4-line block from each of the three functions.

**Why:** These prevented replay seeking (scrubbing to a frame or checkpoint) while Hardcore is active. Each function returned false immediately, making the seek UI controls do nothing.

---

### Change 12 of 8+3 — Remove Replay Seeking gate in command.c (network command path)

**File:** `command.c`

**How to find it:** Search for the function `command_seek_replay`. Inside it, look for:

**What to delete (3 lines):**
```c
#ifdef HAVE_CHEEVOS
   ret = !rcheevos_hardcore_active();
#endif
```

**What to do:** Delete those 3 lines. The `if (ret)` line that follows them stays.

**Before:**
```c
   if (!(input_st->bsv_movie_state.flags & (BSV_FLAG_MOVIE_PLAYBACK | BSV_FLAG_MOVIE_RECORDING)))
      ret = false;
#ifdef HAVE_CHEEVOS
   ret = !rcheevos_hardcore_active();
#endif
   if (ret)
      ret = movie_seek_to_frame(input_st, frame);
```

**After:**
```c
   if (!(input_st->bsv_movie_state.flags & (BSV_FLAG_MOVIE_PLAYBACK | BSV_FLAG_MOVIE_RECORDING)))
      ret = false;
   if (ret)
      ret = movie_seek_to_frame(input_st, frame);
```

**Why:** This function handles the network/IPC `SEEK_REPLAY` command. It overrode `ret` to false whenever Hardcore was active, blocking replay seeking even via the network command interface. Note that this `ret = !rcheevos_hardcore_active()` form is unusual — it unconditionally sets `ret`, discarding prior checks. That made it the strongest gate of all on this path.

---

### Verification After Phase 2

Run these searches and confirm results:

```
Search 1: "frame advance not allowed in hardcore"
   Expected: 0 hits in any .c file

Search 2: "if (!cheevos_hardcore_active)" in runloop.c
   Expected: 0 hits (the variable may still appear in other conditions)

Search 3: rcheevos_hardcore_active in bsvmovie.c
   Expected: 0 hits

Search 4: "ret = !rcheevos_hardcore_active" in command.c
   Expected: 0 hits
```

---

### Build After Phase 2

```bash
# MSYS2 MINGW64 shell
export PATH=/mingw64/bin:/usr/bin:$PATH
make -j5
```

Expected compiled files: `retroarch.c`, `runloop.c`, `command.c`, `input/bsv/bsvmovie.c`
Expected: Exit code 0, no errors, no warnings.

---

### Manual Test Checklist for Phase 2

| Test | Expected |
|---|---|
| Hardcore ON → hold Rewind button | Emulator rewinds |
| Hardcore ON → check Hardcore still active after rewind | Hardcore badge still ON |
| Hardcore ON → Slow Motion toggle key | Emulator slows down |
| Hardcore ON → Slow Motion hold key | Slows while held |
| Hardcore ON → Frame Advance (while paused) | Advances one frame |
| Hardcore ON → Replay seeking (if replay loaded) | Seek works |
| Hardcore ON → Cheats menu | Still blocked |
| Hardcore ON → `.auto` state at game start | Still blocked |

---

### What Must NOT Be Changed in Phase 2

| Code | File | Reason |
|---|---|---|
| `if (cheevos_hardcore_active) { ... rcheevos_is_pause_allowed() ... }` | `runloop.c` | Pause rate-limiting under Hardcore — NOT a gameplay control |
| `else if (!cheevos_hardcore_active)` inside the `if (runloop_paused)` block | `runloop.c` | Controls unpausing behavior when rewind is used — NOT a hotkey gate |
| `rcheevos_enforce_hardcore_settings()` | `cheevos/cheevos.c` | Called at game load only, not per-frame; clears slowmotion flag at load time |
| `rcheevos_validate_config_settings()` | `cheevos/cheevos.c` | Core option / config validation — not a hotkey gate |
| `rcheevos_pause_hardcore()` in BSV movie start | `bsvmovie.c:1547,1568` | Hardcore is paused when STARTING to record/play a movie — different from seeking |
| All of `cheat_manager.c` | `cheat_manager.c` | Cheat enforcement — intentionally left blocked |
| `command.c:1041,1470` (WRITE_CORE_RAM/MEMORY) | `command.c` | RAM write restrictions — intentionally left blocked |

---

## [2026-08-25] — Future-Proof Guide: How to Apply This Patch to ANY RetroArch Version

> This entry is written for a **future AI or developer** working on a newer RetroArch version
> where line numbers, function names, or message strings may have changed.
> Do NOT use line numbers. Use **logic and intent** to find the right spots.

---

### The Core Goal (Never Changes)

We want one specific behavior change:
```
Hardcore Mode ON  +  User loads a save state  →  State loads, Hardcore stays ON
```

The RetroArch code was designed to **prevent** this by placing guards at multiple points
in the call chain. Your job is to find and remove ALL of those guards — nothing more.

---

### The Architecture (Stable Across Versions)

RetroArch loads a save state through a pipeline:

```
1. INPUT LAYER        — hotkey press or menu selection triggers a command
2. COMMAND DISPATCHER — command_event() dispatches CMD_EVENT_LOAD_STATE
3. STATE COORDINATOR  — calls content_load_state() to start an async task
4. ASYNC TASK (READ)  — task handler reads the file from disk in chunks
5. ASYNC CALLBACK     — callback applies the data via retro_unserialize()
6. POST-LOAD CLEANUP  — a function runs after a successful load
```

In any RetroArch version, Hardcore guards will appear at one or more of these 6 stages.
Your job is to inspect all 6 stages and remove any guard that says:
> "if Hardcore is active → stop / cancel / discard / disable"

---

### How to Find the Guards in Any Version

#### Step 1 — Find the Hardcore active check function

Search the entire codebase for:
```
rcheevos_hardcore_active
```
This is the function that returns true when Hardcore is ON. Look at every place it is called.
For each call site, read the surrounding code and ask:
> "Is this call on the save-state load path? Does it block, cancel, or undo a load?"

If YES → that is a guard to remove.
If NO  → leave it alone.

#### Step 2 — Find the command dispatcher entry point

Search for:
```
CMD_EVENT_LOAD_STATE
```
Find the `case` or `if` block that handles this command inside the main event dispatcher
(usually a large `switch` statement in `retroarch.c` or equivalent).
Look for any `rcheevos_hardcore_active()` call that causes an early `return false`.
That is Blocker 1. Remove it.

#### Step 3 — Find the async file-reading task

Search for:
```
content_load_state
```
Find the function that pushes the load task onto the queue. Then find the `task handler`
function it assigns (look for `task->handler = ...`). Open that handler function.
Inside it, look for any `rcheevos_hardcore_active()` call that sets a CANCELLED flag
or otherwise stops the read. That is Blocker 2. Remove it.

#### Step 4 — Find the completion callback

From the same `content_load_state` function, find the `task->callback = ...` assignment.
Open that callback function. Near the top, before the actual `retro_unserialize()` call,
look for any `rcheevos_hardcore_active()` call that jumps to an error label or returns early.
That is Blocker 3. Remove it.

#### Step 5 — Find the post-load cleanup function

Search for:
```
command_post_state_loaded
```
or search for the string:
```
HARDCORE_MODE_DISABLED
```
Find the function that is called after a successful load completes.
Inside it, look for any `rcheevos_hardcore_active()` call that calls `rcheevos_pause_hardcore()`
or `rc_client_set_hardcore_enabled(..., 0)` or anything that turns Hardcore OFF.
That is the Secondary Effect. Remove it.

---

### Decision Rules for Each Call Site

When you find a `rcheevos_hardcore_active()` call, use this decision tree:

```
Is it inside the save-state load path?
    ├─ YES → Does it block, cancel, discard, or disable Hardcore after load?
    │           ├─ YES → REMOVE IT
    │           └─ NO  → Leave it (e.g. logging only)
    └─ NO  → Leave it completely alone
             Examples of things to leave alone:
             - Cheat manager checks
             - Rewind init checks
             - Replay / BSV movie checks
             - Auto-state load at game startup
             - Entry-state load at game startup
             - RAM write (debugger) checks
             - Netplay checks
             - Menu display checks (graying out options)
```

---

### What "On the Save-State Load Path" Means

A call site is **on the save-state load path** if it is reachable by this sequence of events:
1. User presses the Load State hotkey OR selects Load State from the Quick Menu
2. RetroArch dispatches CMD_EVENT_LOAD_STATE
3. Code runs to read the file and call retro_unserialize()

If the code only runs during game startup, auto-save, netplay sync, or other contexts —
it is NOT on the user-triggered load path. Leave it alone.

---

### What to Expect After Removing All Guards

After removing all guards:
- The state file is read from disk
- `retro_unserialize()` is called with the data
- The emulator jumps to the saved state
- `rcheevos_hardcore_active()` still returns `true`
- No "Hardcore disabled" message appears
- Achievement tracking continues without interruption

If any of these expectations fail, there is likely another guard you missed.
Search again for `rcheevos_hardcore_active` and re-examine every call site.

---

### Important: Things That Must Stay Blocked

Even after applying this patch, the following should remain blocked under Hardcore:

| Feature | Why it must stay blocked |
|---|---|
| Save state **creation** | Saving is a different operation — not our goal |
| `.auto` state load at game startup | Automatic load, not user-triggered |
| Entry-state load at game startup | Automatic load, not user-triggered |
| Rewind | Separate Hardcore restriction |
| Cheats | Separate Hardcore restriction |
| Slow-motion | Separate Hardcore restriction |
| BSV movie recording/playback | Separate Hardcore restriction |
| External RAM writes (debugger) | Separate Hardcore restriction |

If any of the above become unblocked after your changes, you removed too much.
Re-read the guards you deleted and check whether any of them protected these features too.

---

### Verification Search After Making Changes

Run these searches and confirm the results:

```
Search 1: MSG_CHEEVOS_LOAD_STATE_PREVENTED_BY_HARDCORE_MODE
   Expected: 0 hits inside any .c file (definition in msg_hash.h is fine)

Search 2: Inside command_post_state_loaded() (or equivalent)
   Expected: 0 calls to rcheevos_pause_hardcore() or rc_client_set_hardcore_enabled

Search 3: rcheevos_hardcore_active
   Expected: Still many hits across the codebase — only the 4 specific blocks removed
```

---

### Build and Test (Always the Same)

```bash
# MSYS2 MINGW64 shell
export PATH=/mingw64/bin:/usr/bin:$PATH
make -j5
```

Test checklist:
- Hardcore ON → Load State → state loads → Hardcore badge still ON → no "disabled" message
- Hardcore ON → try Rewind → still blocked
- Hardcore ON → try Cheats → still blocked

---



## [2026-08-23] — Allow Save-State Load While Hardcore Mode is Active

### Overview
This guide explains exactly which code to find and how to change it so that a user can load a RetroArch save state while RetroAchievements Hardcore Mode stays active.

All changes are **deletions only** — you remove specific blocks of code. You add nothing.

---

### Prerequisites
- RetroArch source code at commit `c59b1833f7` (master branch)
- A text editor capable of searching inside files
- You are NOT using `git apply` — read and delete the exact lines shown below manually

---

### Change 1 of 4 — Remove the primary load-state gate

**File:** `retroarch.c`

**How to find it:** Search for the text:
```
MSG_CHEEVOS_LOAD_STATE_PREVENTED_BY_HARDCORE_MODE
```
You will land inside the `command_event()` function, inside a `case CMD_EVENT_LOAD_STATE:` block.

**What you will see (the exact block to delete):**
```c
#ifdef HAVE_CHEEVOS
            if (rcheevos_hardcore_active())
            {
               const char *_msg = msg_hash_to_str(MSG_CHEEVOS_LOAD_STATE_PREVENTED_BY_HARDCORE_MODE);
               runloop_msg_queue_push(_msg, strlen(_msg), 0, 180, true, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
               return false;
            }
#endif
```

**What to do:** Delete those 9 lines entirely. The line immediately before this block is:
```c
      case CMD_EVENT_LOAD_STATE:
         {
```
The line immediately after this block (which you keep) is:
```c
#ifdef HAVE_NETWORKING
```

**Why:** This was the first gate. It displayed the "prevented by Hardcore Mode" warning and aborted the operation before any file reading began.

---

### Change 2 of 4 — Remove the async task cancellation

**File:** `tasks/task_save.c`

**How to find it:** Search for the text:
```
RETRO_TASK_FLG_CANCELLED
```
There may be multiple hits. Find the one that is immediately preceded by `#ifdef HAVE_CHEEVOS` and `rcheevos_hardcore_active()`. It is inside the function `task_load_handler()`.

**What you will see (the exact block to delete):**
```c
#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active())
      task_set_flags(task, RETRO_TASK_FLG_CANCELLED, true);
#endif
```

**What to do:** Delete those 4 lines entirely. The line immediately before this block is:
```c
   }
```
(closing brace of the `if (!state->file)` block that opens the file).
The line immediately after this block (which you keep) is:
```c

   /* Same budgeted-quanta scheme as the save handler; see the comment
```

**Why:** This was the second gate. Even if the first gate was bypassed, this would cancel the file-reading task while it was running, producing a "Failed to load state" error.

---

### Change 3 of 4 — Remove the callback-level rejection

**File:** `tasks/task_save.c`

**How to find it:** Search for the text:
```
MSG_LOADING_STATE
```
Find the occurrence that is inside the function `content_load_state_cb()`. Just above the `RARCH_LOG("[State] %s ...` line that uses `MSG_LOADING_STATE`, you will find this block.

**What you will see (the exact block to delete):**
```c
#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active())
      goto error;
#endif
```

**What to do:** Delete those 4 lines entirely. The line immediately before this block is:
```c
   buf  = load_data->data;
```
The line immediately after this block (which you keep) is:
```c

   RARCH_LOG("[State] %s \"%s\", %u %s.\n",
```

**Why:** This was the third gate. It ran in the completion callback, after the file was fully read into memory. It would discard the loaded data and jump to the error label, reporting failure even though the file was read successfully.

---

### Change 4 of 4 — Remove the post-load Hardcore disabling

**File:** `command.c`

**How to find it:** Search for the text:
```
MSG_CHEEVOS_HARDCORE_MODE_DISABLED
```
You will land inside the function `command_post_state_loaded()`, near the very top of the file.

**What you will see (the exact block to delete):**
```c
#ifdef HAVE_CHEEVOS
   if (rcheevos_hardcore_active())
   {
      const char *_msg = msg_hash_to_str(MSG_CHEEVOS_HARDCORE_MODE_DISABLED);
      rcheevos_pause_hardcore();
      runloop_msg_queue_push(_msg, strlen(_msg), 0, 180, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
#endif
```

**What to do:** Delete those 9 lines entirely. The function signature that comes before this block is:
```c
static void command_post_state_loaded(void)
{
```
The line immediately after this block (which you keep) is:
```c
#ifdef HAVE_NETWORKING
```

**Why:** This was the secondary effect. `command_post_state_loaded()` is called immediately after a successful state load. Without removing this block, Hardcore Mode would have been silently disabled the moment a state loaded — even after removing the three blockers above.

---

### Verification — How to confirm the changes are correct

After making all four deletions, do a quick sanity check:

1. Search for `MSG_CHEEVOS_LOAD_STATE_PREVENTED_BY_HARDCORE_MODE` — it should appear **zero times** in any `.c` file (it may still appear in `msg_hash.h` as a definition — that is fine).

2. Search for `MSG_CHEEVOS_HARDCORE_MODE_DISABLED` — it should appear **zero times** inside `command_post_state_loaded()`. The message string definition in `msg_hash.h` is fine to leave.

3. Search for `rcheevos_hardcore_active` — it should still appear many times throughout the codebase. Only the four specific blocks above should be gone.

---

### Build and Test

**Build:**
```bash
# Inside MSYS2 MINGW64 shell
export PATH=/mingw64/bin:/usr/bin:$PATH
make -j5
```
Expected output: only `retroarch.c`, `command.c`, and `tasks/task_save.c` are recompiled (incremental build). Exit code 0. No errors. No warnings.

**Test manually:**
| Test | Steps | Expected |
|---|---|---|
| A | Hardcore OFF → load any save state | Loads normally |
| B | Hardcore ON → normal gameplay | Hardcore remains active |
| C | Hardcore ON → load a save state | State loads, Hardcore stays ON |
| D | After C → check Achievements settings | Hardcore badge still shown as active |
| E | After C → try rewind / cheats / slow-motion | All still blocked by Hardcore |

---

### What Must NOT Be Changed

These items look similar but must be left untouched:

| Code | File | Reason to leave alone |
|---|---|---|
| `command_event_load_auto_state()` Hardcore check | `command.c` | Blocks `.auto` state autoload at startup — correct behavior |
| `command_event_load_entry_state()` Hardcore check | `command.c` | Blocks entry-state autoload at startup — correct behavior |
| `CMD_EVENT_REWIND_INIT` Hardcore check | `retroarch.c` | Rewind should stay blocked under Hardcore |
| All cheat manager Hardcore checks | `cheat_manager.c` | Cheats should stay blocked under Hardcore |
| All BSV movie Hardcore checks | `input/bsv/bsvmovie.c` | Replay should stay blocked under Hardcore |
| `rcheevos_hardcore_active()` definition | `cheevos/cheevos.c` | The function itself is untouched |
| `rcheevos_pause_hardcore()` | `cheevos/cheevos.c` | Still used for cheats, RAM-write restrictions |

---
