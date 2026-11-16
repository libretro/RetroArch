/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef CORE_LOAD_CHECK_H__
#define CORE_LOAD_CHECK_H__

#include <boolean.h>
#include <dynamic/dylib.h>
#include <retro_common_api.h>

#include "core_signature.h"

RETRO_BEGIN_DECLS

/* Every core is checked before it is opened. Main thread, unless a
 * function says otherwise. */

enum core_load_gate
{
   /* No trust store: open the core as a build without the check would */
   CORE_LOAD_GATE_OFF = 0,
   /* Opened from the file that was checked; *lib is NULL when the
    * system loader failed, and dylib_error() says why */
   CORE_LOAD_GATE_OPENED,
   /* Not trusted, or its check still runs: nothing was opened */
   CORE_LOAD_GATE_REFUSED
};

/* path as the loader takes it (Android: the real path) */
enum core_load_gate core_load_check_open(const char *path, dylib_t *lib);
#ifdef _WIN32
/* A \\?\ path ending in '.': the open refuses it as given */
bool core_load_verbatim_dot(const char *path);
#endif
/* The last open of path was refused by the check, not the loader */
bool core_load_check_refused(const char *path);
/* That refusal waits only for the first run's seed */
bool core_load_check_refused_seed(const char *path);
/* The core the last core_load_check_open() opened is now running. A
 * waiting park ends, as the user's Cancel would end it. */
void core_load_check_loaded(void);
/* A built-in core is now running: it ends what another core's load
 * ends */
void core_load_check_builtin_loaded(void);
/* Its pass outlives the unload while its held file or its identity
 * binds it, until another core loads or a release */
void core_load_check_unloaded(void);
/* Before a task replaces, deletes or restores a core file: ends the
 * pass of the core loaded last and closes every file a record keeps.
 * Never from a worker. */
void core_load_check_release_held(void);
/* While the startup load runs before the frame loop: a core the quick
 * check doesn't pass is checked whole, in place */
void core_load_check_set_startup(bool on);
void core_load_check_deinit(void);

/* The check stage of a staged load (runloop_core_preload_*), path as
 * the core stage opens it. False: no trust store. */
bool core_load_check_stage_begin(const char *path);
/* Polled each frame: true once the core stage may run */
bool core_load_check_stage_ready(void);
/* The stage's core was refused */
bool core_load_check_stage_refused(void);
/* The library a worker opened from the checked file, or NULL; ends the
 * stage */
dylib_t core_load_check_stage_take(const char *path);
void core_load_check_stage_cancel(void);

typedef void (*core_load_rerun_t)(void *userdata);

/* Parks a user-started load the check holds back. It re-runs once the
 * core passes, or after the user picks Load Once or Always Trust on
 * the Security Warning. With no menu to ask, the refusal message is
 * shown, or held until the drivers are up, and a park the startup load
 * made that can't run quits as a failed startup does.
 * True: free_cb(userdata) runs once, after the re-run or when the
 * request is dropped; it may run inside a warning's CANCEL, so it must
 * not push or choose a warning. A newer park replaces it, and any
 * other load ends it, the same core's too.
 * False: nothing taken (no runtime, the core can't be opened, or the
 * park ended at once). Park only after core_load_check_refused(): a
 * false then means the user was told (barring out of memory or the
 * file going between the gate and the park), so the caller adds no
 * load error beyond its failed probe's. */
bool core_load_check_park(const char *path, core_load_rerun_t rerun,
      core_load_rerun_t free_cb, void *userdata);
/* Each frame while RUNLOOP_WORK_CORE_PARK is up, which a park raises
 * and its drop lowers: the parked request's waits */
void core_load_check_iterate(void);
/* The drivers are up: a message held for them shows */
void core_load_check_notify(void);

enum core_load_startup
{
   CORE_LOAD_STARTUP_NONE = 0,
   /* The pending seed covers it: run again once the seed is done */
   CORE_LOAD_STARTUP_SEED,
   /* Refused, with a menu to ask on */
   CORE_LOAD_STARTUP_REFUSED,
   /* Refused with no menu to ask on: the startup fails */
   CORE_LOAD_STARTUP_FAILED
};

/* What a refused startup load left, for its failure path; read once */
enum core_load_startup core_load_check_startup_take(void);

struct core_load_source
{
   struct core_identity id;
   uint8_t sha256[CORE_SHA256_LEN];
   bool hashed;                 /* sha256 is the running core's */
};

enum core_load_copy
{
   CORE_LOAD_COPY_OFF = 0,      /* no store, or checks off: as before */
   CORE_LOAD_COPY_CHECK,        /* the copy must match the source */
   CORE_LOAD_COPY_NONE          /* the running core isn't known: no copy */
};

/* Main thread: what a copy of the running core at path must match */
enum core_load_copy core_load_check_copy_source(const char *path,
      struct core_load_source *src);

typedef struct core_load_match core_load_match_t;
/* Any thread. NULL when the copy can't be opened, or the source (hashed
 * only when src has no hash) is no longer the file loaded. */
core_load_match_t *core_load_match_new(const char *copy_path,
      const char *src_path, const struct core_load_source *src);
/* Any thread: 1 while there is more, 0 when the copy matches, -1 not */
int core_load_match_step(core_load_match_t *m);
/* Any thread, after 0: loads the copy from the file hashed */
dylib_t core_load_match_load(core_load_match_t *m);
void core_load_match_free(core_load_match_t *m);

RETRO_END_DECLS

#endif
