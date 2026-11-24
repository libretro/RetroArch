/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdlib.h>
#include <string.h>
#ifdef ANDROID
#include <stdio.h>
#include <unistd.h>
#endif

#include <compat/strl.h>
#include <file/file_path.h>
#include <queues/task_queue.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#include "core_load_check.h"
#include "core_load_file.h"
#include "core_trust_runtime.h"
#include "msg_hash.h"
#include "retroarch.h"
#include "runloop.h"
#include "tasks/task_content.h"
#include "tasks/tasks_internal.h"
#include "verbosity.h"
#ifdef ANDROID
#include "frontend/drivers/platform_unix.h"
#endif

/* A check's outcome, kept for the load it was made for */
struct core_load_slot
{
   struct core_check c;
   /* Open while it alone binds the pass to the file (Load Once, or a
    * filesystem whose ctime a writer could set), or while a refusal
    * waits for the user */
   core_file_t *f;
   /* A refusal's trust set: it answers only while that is current */
   core_trust_set_t *s;
   char path[PATH_MAX_LENGTH];
   bool used;
   bool pass;
   bool hashed;
};

struct core_load_check_task
{
   struct core_check c;
   struct core_hash h;
   core_trust_set_t *s;
   core_file_t *f;
   char *path;
   char *check_path;
   unsigned gen;
   bool hash_live;
#ifdef ANDROID
   bool rehash;   /* f hashed again after the enable */
   bool verity;   /* fs-verity on */
#endif
};

enum core_load_grant
{
   CORE_LOAD_GRANT_NONE = 0,
   CORE_LOAD_GRANT_SLOT,
   CORE_LOAD_GRANT_QUICK,
   CORE_LOAD_GRANT_LOADED
};

enum core_load_stage
{
   CORE_LOAD_STAGE_NONE = 0,
   CORE_LOAD_STAGE_SEED,
   CORE_LOAD_STAGE_CHECK,
   CORE_LOAD_STAGE_OPEN,
   CORE_LOAD_STAGE_DONE
};

enum core_load_park_state
{
   CORE_LOAD_PARK_NONE = 0,
   CORE_LOAD_PARK_SEED,
   CORE_LOAD_PARK_CHECK,
   CORE_LOAD_PARK_USER,
   CORE_LOAD_PARK_IDLE,
   CORE_LOAD_PARK_CANCELLED
};

/* A refused user-started load, waiting to run again */
struct core_load_park
{
   struct core_check c;
   core_file_t *f;              /* the file the user is asked about */
   void *userdata;
   core_load_rerun_t rerun;
   core_load_rerun_t free_cb;
   core_load_park_end_t end;
   char path[PATH_MAX_LENGTH];
   enum core_load_park_state state;
   unsigned serial;
   bool startup;                /* the startup load's */
};

/* Opens the checked file on a worker: the loader maps the core and
 * runs its constructors off the frame loop */
struct core_load_open_task
{
   struct core_identity id;   /* the file checked */
   core_file_t *f;
   char *path;
   char *check_path;
   dylib_t lib;
   unsigned gen;
   bool moved;                /* not the file checked any more */
};

static struct
{
   struct core_load_park park;
   struct core_load_slot slot;    /* the latest check */
   struct core_load_slot loaded;  /* the core loaded last */
   struct core_load_slot quick;   /* the last quick pass */
   struct core_check c;
   struct core_check stage_c;     /* the stage's own quick pass */
   core_file_t *stage_f;          /* the checked file the open uses */
   dylib_t stage_lib;
   char path[PATH_MAX_LENGTH];
   char check_path[PATH_MAX_LENGTH];
   char refused[PATH_MAX_LENGTH];
   char checking[PATH_MAX_LENGTH];
   char stage_path[PATH_MAX_LENGTH];
   enum core_load_stage stage;
   enum core_load_grant stage_grant;
   enum core_load_grant granted;
   enum core_load_startup startup_state;
   unsigned gen;
   unsigned stage_gen;
   bool stage_lent;               /* stage_f is the slot's own */
   bool stage_refused;
   bool stage_moved;              /* its re-check was logged */
   bool startup;
   bool notified;
   bool refusal_held;
   bool refused_seed;  /* refused only until the seed is done */
   bool running;  /* loaded and not yet unloaded */
} core_load_rt;

/* A user-facing request waits for path's check: its refusal keeps the
 * file, and a quiet result never replaces its slot */
static bool core_load_check_waited(const char *path)
{
   return (core_load_rt.stage == CORE_LOAD_STAGE_CHECK
         && string_is_equal(core_load_rt.stage_path, path))
      ||  (core_load_rt.park.state == CORE_LOAD_PARK_CHECK
         && string_is_equal(core_load_rt.park.path, path));
}

static void core_load_slot_clear(struct core_load_slot *sl)
{
   core_file_close(sl->f);
   core_trust_set_put(sl->s);
   memset(sl, 0, sizeof(*sl));
}

/* A refusal takes the reference to the set it was checked against.
 * The trust store publishes a new set on every change (a record,
 * Always Trust, the switch, the seed) and none for a refusal, and the
 * reference keeps the address from being reused. */
static void core_load_slot_keep_store(struct core_load_slot *sl,
      core_trust_set_t **s)
{
   if (!sl->used || sl->pass)
      return;
   sl->s = *s;
   *s    = NULL;
}

/* The slot's refusal still answers: nothing changed in the store */
static bool core_load_slot_refusal_holds(const core_trust_set_t *s)
{
   return !core_load_rt.slot.pass && core_load_rt.slot.s == s;
}

static bool core_load_slot_is(const struct core_load_slot *sl,
      const char *path)
{
   return sl->used && string_is_equal(sl->path, path);
}

#ifdef _WIN32
/* LoadLibraryW loads <name>.dll for this name */
static bool core_load_dll_appended(const char *path)
{
   return !strchr(path_basename_nocompression(path), '.');
}

/* CreateFileW opens this name's trailing '.' as written; LoadLibraryW
 * may drop it and map another file */
bool core_load_verbatim_dot(const char *path)
{
   size_t n = strlen(path);
   return   n > 4
         && path[n - 1] == '.'
         && PATH_CHAR_IS_SLASH(path[0])
         && (PATH_CHAR_IS_SLASH(path[1]) || path[1] == '?')
         && path[2] == '?'
         && PATH_CHAR_IS_SLASH(path[3]);
}
#endif

/* Absolute, as the loader is given it */
static void core_load_abs(const char *path, char *s, size_t len)
{
   strlcpy(s, path, len);
#ifdef _WIN32
   /* As given: _wfullpath drops the '.' the check refuses */
   if (core_load_verbatim_dot(s))
      return;
   /* Named in full, so the check reads the file loaded */
   if (core_load_dll_appended(s))
      strlcat(s, ".dll", len);
#endif
   path_resolve_realpath(s, len, false);
#ifdef _WIN32
   /* Its trailing dots went: one keeps ".dll" off */
   if (core_load_dll_appended(s))
      strlcat(s, ".", len);
#endif
}

/* The file the check reads: Apple's dylib_load opens a .framework's
 * binary, so the check does too. False: none it can vouch for. */
static bool core_load_check_file_path(const char *path, char *s,
      size_t len)
{
#ifdef _WIN32
   if (core_load_verbatim_dot(path))
      return false;
#endif
#ifdef __APPLE__
   if (core_load_framework_binary(path, s, len))
      return true;
   if (strstr(path, ".framework"))
      return false;
#endif
   strlcpy(s, path, len);
   return true;
}

/* f is the file id describes; its fs-verity digest is measured, not
 * read, so it isn't compared */
static bool core_load_same_file(const struct core_identity *id,
      core_file_t *f)
{
   struct core_identity now;
   if (!core_file_identity(f, &now))
      return false;
   memcpy(now.verity, id->verity, sizeof(now.verity));
   now.verity_len = id->verity_len;
   return core_identity_equal(id, &now);
}

/* As core_load_same_file(), the file held open since its check. On
 * Windows that handle (no write or delete share) pins the file, and
 * its first load as a DLL moves its ChangeTime (Code Integrity writes
 * an extended attribute), so that isn't compared. */
static bool core_load_same_held(const struct core_identity *id,
      core_file_t *f)
{
#ifdef _WIN32
   struct core_identity now;
   if (!core_file_identity(f, &now))
      return false;
   memcpy(now.verity, id->verity, sizeof(now.verity));
   now.verity_len = id->verity_len;
   now.ctime      = id->ctime;
   now.ctime_nsec = id->ctime_nsec;
   return core_identity_equal(id, &now);
#else
   return core_load_same_file(id, f);
#endif
}

/* The slot's own file, or check_path opened again if it is still the
 * file the slot was checked from */
static core_file_t *core_load_slot_file(const struct core_load_slot *sl,
      const char *check_path, bool *lent)
{
   core_file_t *f;
   bool same;
   *lent = false;
   if (!(f = core_file_open(check_path)))
      return NULL;
   same = sl->f ? core_load_same_held(&sl->c.rec.id, f)
      : core_load_same_file(&sl->c.rec.id, f);
   if (sl->f)
   {
      /* Loaded by path outside Android: the path must still name
       * the file kept. Windows' kept handle pins the file, not a folder
       * above it. */
      core_file_close(f);
      *lent = true;
#ifdef ANDROID
      /* Loaded from it: a write in place moves its identity */
      return core_load_same_held(&sl->c.rec.id, sl->f) ? sl->f : NULL;
#else
      return same ? sl->f : NULL;
#endif
   }
   if (same)
      return f;
   core_file_close(f);
   return NULL;
}

/* The core loaded last, while its held file or its identity binds its
 * pass: the file to open it from (*lent: the record's own), or NULL */
static core_file_t *core_load_loaded_file(const char *path, bool *lent)
{
   *lent = false;
   if (     !core_load_slot_is(&core_load_rt.loaded, path)
         || !core_load_rt.loaded.pass)
      return NULL;
   return core_load_slot_file(&core_load_rt.loaded,
         core_load_rt.check_path, lent);
}

static void core_load_slot_set(struct core_load_slot *sl, const char *path,
      const struct core_check *c, core_file_t **f, bool user)
{
   bool keep;
   core_load_slot_clear(sl);
   sl->c      = *c;
   sl->used   = true;
   sl->pass   = (c->result == CORE_CHECK_OK);
   sl->hashed = true;
   strlcpy(sl->path, path, sizeof(sl->path));
   keep       = sl->pass ? !c->rec.id.ctime_kept : user;
   if (keep)
   {
      sl->f = *f;
      *f    = NULL;
   }
}

/* Any thread: the check once h has read f to its end (step 0) or
 * failed (step -1). Frees what h holds. */
static void core_load_check_complete(const core_trust_set_t *s,
      struct core_hash *h, int step, const char *check_path,
      struct core_check *c)
{
   size_t num_keys     = 0;
   const uint8_t *keys = core_sig_builtin_keys(&num_keys);
   core_check_hashed(s, keys, num_keys, h, step, check_path, 0, c);
}

static void core_load_check_task_free(struct core_load_check_task *t)
{
   if (t->hash_live)
      core_hash_free(&t->h);
   core_file_close(t->f);
   if (t->s)
      core_trust_set_put(t->s);
   free(t->path);
   free(t->check_path);
   free(t);
}

#ifdef ANDROID
/* The second hash ended (step 0) or failed (step -1): only the file as
 * it was checked is recorded */
static void core_load_check_verity_end(struct core_load_check_task *t,
      int step)
{
   uint8_t sha256[CORE_SHA256_LEN];
   t->hash_live = false;
   t->rehash    = false;
   if (step < 0)
      core_hash_free(&t->h);
   else
   {
      core_hash_final(&t->h, sha256);
      /* Enabling may move ctime: the file as it is now, recorded, or
       * if it changed, what Always Trust records. Without fs-verity
       * nothing held it while it was hashed. */
      if (t->verity
            ? (     !core_file_identity(t->f, &t->c.rec.id)
                 || !core_file_verity_measure(t->f, &t->c.rec.id))
            : !core_load_same_file(&t->c.rec.id, t->f))
         t->c.record = false;
      if (!memcmp(sha256, t->c.rec.sha256, sizeof(sha256)))
         return;
      /* Changed since its hash and now kept so: a Load Once opens
       * these bytes, so Always Trust takes their hash */
      memcpy(t->c.rec.sha256, sha256, sizeof(sha256));
      t->c.rec.build_time = 0;
      RARCH_WARN("[Core] \"%s\" changed while it was checked.\n", t->path);
   }
   t->c.result       = (step < 0) ? CORE_CHECK_ERROR : CORE_CHECK_MODIFIED;
   t->c.record       = false;
   t->c.raise_newest = false;
}

/* After a full check that passed: fs-verity, so the kernel keeps the
 * file from changing, and its digest goes in the record. Building the
 * tree reads the whole file: on a task worker only, never the main
 * thread or the startup's in-place check. True: f is hashed again. */
static bool core_load_check_verity(struct core_load_check_task *t)
{
   int on;
   if (t->c.result != CORE_CHECK_OK || !t->c.record)
      return false;
   /* Can't here (no fs-verity, not our file, a writer has it, an old
    * kernel's page size): the identity record works as before, unless
    * the attempt moved the file (f2fs's rollback truncates it) */
   on = core_file_verity_enable(t->f);
   if (on <= 0 && core_load_same_file(&t->c.rec.id, t->f))
      return false;
   t->verity = (on > 0);
   /* A write since the hash is kept now, or the file moved: it must
    * hash the same, the moved one as it is from now */
   if (     (!t->verity && !core_file_identity(t->f, &t->c.rec.id))
         || lseek(core_file_fd(t->f), 0, SEEK_SET) != 0
         || !(t->hash_live = core_hash_init(&t->h)))
   {
      core_load_check_verity_end(t, -1);
      return false;
   }
   t->rehash = true;
   return true;
}
#endif

static void core_load_check_handler(retro_task_t *task)
{
   struct core_load_check_task *t = (struct core_load_check_task*)
      task->state;
   nbio_budget_t budget;
   int step = 1;
   if (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED)
   {
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }
   task_nbio_slice_open(&budget);
   while (step > 0 && task_nbio_slice_within_budget(&budget, 0, 0))
      step = core_hash_step(&t->h, t->f);
   task_nbio_slice_close(&budget);
   if (step > 0)
      return;
#ifdef ANDROID
   if (t->rehash)
   {
      core_load_check_verity_end(t, step);
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }
#endif
   core_load_check_complete(t->s, &t->h, step, t->check_path, &t->c);
   t->hash_live = false;
#ifdef ANDROID
   /* The enable reads the whole core in one call, and this handler runs
    * on the main thread when tasks aren't threaded */
   if (!task_is_on_main_thread() && core_load_check_verity(t))
      return;
#endif
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void core_load_check_stage_result(const char *path, bool pass);
static void core_load_check_stage_start(void);
static void core_load_check_stage_file_drop(void);
static void core_load_park_result(const char *path, bool checked);

static void core_load_check_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   struct core_load_check_task *t = (struct core_load_check_task*)
      task->state;
   bool user;
   (void)task_data;
   (void)user_data;
   (void)err;
   if (!t || t->gen != core_load_rt.gen)
      return;
   if (string_is_equal(core_load_rt.checking, t->path))
      core_load_rt.checking[0] = '\0';
   /* Its hash may not have ended: no verdict, and a load waiting on it
    * is refused */
   if (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED)
   {
      core_load_check_stage_result(t->path, false);
      core_load_park_result(t->path, false);
      return;
   }
   user = core_load_check_waited(t->path);
   if (!core_trust_runtime_apply(&t->c))
      RARCH_WARN("[Core] Could not record the check of \"%s\".\n",
            t->path);
   /* Only a held handle could bind this pass (no kept ctime, as on
    * Windows), and one held for a core nobody waits on would block the
    * Core Updater's rename: not kept, the load checks it again */
   if (     !user
         &&  t->c.result == CORE_CHECK_OK
         && !t->c.rec.id.ctime_kept)
   {
      if (     core_load_slot_is(&core_load_rt.slot, t->path)
            && !core_load_rt.slot.pass)
         core_load_slot_clear(&core_load_rt.slot);
      return;
   }
   /* A quiet result never takes the place of a pass a load waits on */
   if (     !user
         &&  core_load_rt.slot.used
         &&  core_load_rt.slot.pass
         && !string_is_equal(core_load_rt.slot.path, t->path))
      return;
   core_load_slot_set(&core_load_rt.slot, t->path, &t->c, &t->f, user);
   core_load_slot_keep_store(&core_load_rt.slot, &t->s);
   core_load_check_stage_result(t->path, core_load_rt.slot.pass);
   core_load_park_result(t->path, true);
}

static void core_load_check_cleanup(retro_task_t *task)
{
   struct core_load_check_task *t = (struct core_load_check_task*)
      task->state;
   if (t)
      core_load_check_task_free(t);
   task->state = NULL;
}

/* Hashes f (which it takes) in a task; the result goes to the slot.
 * A check of path already running is joined instead. False: no check
 * will answer. */
static bool core_load_check_push(core_file_t *f, const char *path,
      const char *check_path, const struct core_check *c)
{
   struct core_load_check_task *t = NULL;
   retro_task_t *task             = NULL;
   if (string_is_equal(core_load_rt.checking, path))
   {
      core_file_close(f);
      return true;
   }
   if (!(t = (struct core_load_check_task*)calloc(1, sizeof(*t))))
   {
      core_file_close(f);
      return false;
   }
   t->f          = f;
   t->c          = *c;
   /* Only a finished hash gives a pass */
   t->c.result   = CORE_CHECK_ERROR;
   t->gen        = core_load_rt.gen;
   t->s          = core_trust_runtime_get();
   t->path       = strdup(path);
   t->check_path = strdup(check_path);
   t->hash_live  = core_hash_init(&t->h);
   if (     !t->s || !t->path || !t->check_path || !t->hash_live
         || !(task = task_init()))
   {
      core_load_check_task_free(t);
      return false;
   }
   task->handler  = core_load_check_handler;
   task->callback = core_load_check_cb;
   task->cleanup  = core_load_check_cleanup;
   task->state    = t;
   task->flags   |= RETRO_TASK_FLG_MUTE;
   strlcpy(core_load_rt.checking, path, sizeof(core_load_rt.checking));
   task_queue_push(task);
   return true;
}

static void core_load_check_osd_checking(const char *path)
{
   char msg[NAME_MAX_LENGTH + 64];
   size_t _len = strlcpy(msg, msg_hash_to_str(MSG_CHECKING_CORE),
         sizeof(msg));
   if (_len < sizeof(msg))
      strlcpy(msg + _len, path_basename_nocompression(path),
            sizeof(msg) - _len);
   runloop_msg_queue_push(msg, strlen(msg), 1, 180, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
}

/* Worker: loaded by path outside Android, so only while the path still
 * names the file checked, unchanged, which o->f holds; on Android from
 * o->f, so only while it is unchanged. A pass with checks off records
 * no identity: nothing to compare. */
static bool core_load_open_names(const struct core_load_open_task *o)
{
   struct core_identity none;
#ifndef ANDROID
   core_file_t *f = NULL;
   bool same;
#endif
   memset(&none, 0, sizeof(none));
   if (core_identity_equal(&o->id, &none))
      return true;
#ifdef ANDROID
   return core_load_same_held(&o->id, o->f);
#else
   same = (f = core_file_open(o->check_path))
      && core_load_same_held(&o->id, f);
   core_file_close(f);
   return same;
#endif
}

/* Once a stage: a file that keeps moving is checked again each time */
static void core_load_check_stage_moved(void)
{
   if (!core_load_rt.stage_moved)
      RARCH_LOG("[Core] \"%s\" changed after its check: checking it again.\n",
            core_load_rt.stage_path);
   core_load_rt.stage_moved = true;
}

static void core_load_open_handler(retro_task_t *task)
{
   struct core_load_open_task *o = (struct core_load_open_task*)
      task->state;
   o->moved = !core_load_open_names(o);
   if (!o->moved)
      o->lib = core_load_file(o->f, o->path);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void core_load_open_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   struct core_load_open_task *o = (struct core_load_open_task*)
      task->state;
   (void)task_data;
   (void)user_data;
   (void)err;
   /* Superseded: the cleanup closes both */
   if (     !o
         || o->gen != core_load_rt.stage_gen
         || core_load_rt.stage != CORE_LOAD_STAGE_OPEN)
      return;
   if (o->moved)
   {
      /* Its pass went with the file: the path is checked again */
      core_load_check_stage_file_drop();
      core_load_check_stage_moved();
      core_load_check_stage_start();
      return;
   }
   core_load_rt.stage_lib = o->lib;
   core_load_rt.stage_f   = o->f;
   o->lib                 = NULL;
   o->f                   = NULL;
   core_load_rt.stage     = CORE_LOAD_STAGE_DONE;
}

static void core_load_open_cleanup(retro_task_t *task)
{
   struct core_load_open_task *o = (struct core_load_open_task*)
      task->state;
   if (!o)
      return;
   if (o->lib)
      dylib_close(o->lib);
   core_file_close(o->f);
   free(o->path);
   free(o->check_path);
   free(o);
   task->state = NULL;
}

static void core_load_check_stage_open(void)
{
   struct core_load_open_task *o = NULL;
   retro_task_t *task            = NULL;
   if (     !core_load_rt.stage_f
         || !core_load_check_file_path(core_load_rt.stage_path,
            core_load_rt.check_path, sizeof(core_load_rt.check_path))
         || !(o = (struct core_load_open_task*)calloc(1, sizeof(*o))))
      return;
   if (     !(o->path       = strdup(core_load_rt.stage_path))
         || !(o->check_path = strdup(core_load_rt.check_path))
         || !(task          = task_init()))
   {
      free(o->path);
      free(o->check_path);
      free(o);
      return;
   }
   o->id                = (core_load_rt.stage_grant == CORE_LOAD_GRANT_SLOT
         ? &core_load_rt.slot : &core_load_rt.quick)->c.rec.id;
   o->f                 = core_load_rt.stage_f;
   o->gen               = core_load_rt.stage_gen;
   core_load_rt.stage_f = NULL;
   task->handler        = core_load_open_handler;
   task->callback       = core_load_open_cb;
   task->cleanup        = core_load_open_cleanup;
   task->state          = o;
   task->flags         |= RETRO_TASK_FLG_MUTE;
   core_load_rt.stage   = CORE_LOAD_STAGE_OPEN;
   task_queue_push(task);
}

/* The stage's file back to the slot that lent it, else closed */
static void core_load_check_stage_file_drop(void)
{
   if (     core_load_rt.stage_lent
         && core_load_slot_is(&core_load_rt.slot, core_load_rt.stage_path)
         && !core_load_rt.slot.f)
   {
      if (core_load_rt.stage_f)
      {
         core_load_rt.slot.f  = core_load_rt.stage_f;
         core_load_rt.stage_f = NULL;
      }
      /* It went with a superseded open: no pass without its file */
      else
         core_load_slot_clear(&core_load_rt.slot);
   }
   core_file_close(core_load_rt.stage_f);
   core_load_rt.stage_f    = NULL;
   core_load_rt.stage_lent = false;
}

/* The stage's core has its answer: with a worker, a pass opens there
 * from the file checked */
static void core_load_check_stage_settle(bool pass,
      enum core_load_grant grant)
{
   bool lent                  = false;
   core_load_rt.stage         = CORE_LOAD_STAGE_DONE;
   core_load_rt.stage_refused = !pass;
   core_load_rt.stage_grant   = grant;
   /* Else the core stage opens it */
   if (!pass || !task_queue_is_threaded())
   {
      core_load_check_stage_file_drop();
      return;
   }
   if (grant == CORE_LOAD_GRANT_SLOT && !core_load_rt.stage_f)
   {
      if (core_load_rt.slot.f)
      {
         /* The pass's own file, back to the slot at the take */
         core_load_rt.stage_f    = core_load_rt.slot.f;
         core_load_rt.slot.f     = NULL;
         core_load_rt.stage_lent = true;
      }
      else if (core_load_check_file_path(core_load_rt.stage_path,
               core_load_rt.check_path, sizeof(core_load_rt.check_path))
            && !(core_load_rt.stage_f = core_load_slot_file(
                  &core_load_rt.slot, core_load_rt.check_path, &lent)))
      {
         /* Changed since its check: checked again */
         core_load_check_stage_moved();
         core_load_check_stage_start();
         return;
      }
   }
   core_load_check_stage_open();
}

/* In order: the quick check, a pass the slot holds, the seed's wait, a
 * refusal the slot holds for this file and store, or a check task */
static void core_load_check_stage_start(void)
{
   const char *path    = core_load_rt.stage_path;
   core_trust_set_t *s = core_trust_runtime_get();
   struct core_identity id;
   core_file_t *f      = NULL;
   core_file_t *g      = NULL;   /* a record's file */
   bool lent           = false;
   /* No store, no file the check can vouch for, the core loaded last
    * (the gate passes it from its record: nothing to hash or keep), or
    * a file that can't be opened: the core stage's own open answers */
   if (     !s
         || !core_load_check_file_path(path, core_load_rt.check_path,
            sizeof(core_load_rt.check_path))
         || (g = core_load_loaded_file(path, &lent))
         || !(f = core_file_open_id(core_load_rt.check_path, &id)))
   {
      core_load_rt.stage = CORE_LOAD_STAGE_DONE;
      goto end;
   }
   if (core_check_quick_id(s, f, core_load_rt.check_path, &id,
            &core_load_rt.c))
   {
      bool pass = (core_load_rt.c.result == CORE_CHECK_OK);
      if (pass)
      {
         core_load_slot_clear(&core_load_rt.quick);
         core_load_rt.quick.c    = core_load_rt.c;
         core_load_rt.quick.used = true;
         core_load_rt.quick.pass = true;
         strlcpy(core_load_rt.quick.path, path,
               sizeof(core_load_rt.quick.path));
         core_load_rt.stage_c    = core_load_rt.c;
         core_load_rt.stage_f    = f;
         f                       = NULL;
      }
      core_load_check_stage_settle(pass, CORE_LOAD_GRANT_QUICK);
      goto end;
   }
   if (core_load_slot_is(&core_load_rt.slot, path))
      g = core_load_slot_file(&core_load_rt.slot, core_load_rt.check_path,
            &lent);
   if (g && core_load_rt.slot.pass)
   {
      /* The file the gate would open it from */
      core_load_rt.stage_f    = g;
      core_load_rt.stage_lent = lent;
      if (lent)
         core_load_rt.slot.f  = NULL;
      g                       = NULL;
      core_load_check_stage_settle(true, CORE_LOAD_GRANT_SLOT);
      goto end;
   }
   /* The seed may trust it; that it covers it is only a guess */
   if (     core_trust_runtime_seed_pending()
         && core_trust_runtime_seed_covers(path))
   {
      core_load_rt.stage = CORE_LOAD_STAGE_SEED;
      goto end;
   }
   if (g && core_load_slot_refusal_holds(s))
   {
      core_load_check_stage_settle(false, CORE_LOAD_GRANT_SLOT);
      goto end;
   }
   if (core_load_slot_is(&core_load_rt.slot, path))
   {
      /* Replaced, or the store changed, since its check */
      if (lent)
         g = NULL;
      core_load_slot_clear(&core_load_rt.slot);
   }
   core_load_rt.stage = CORE_LOAD_STAGE_CHECK;
   /* No check will answer: refused, as a cancelled one is */
   if (!core_load_check_push(f, path, core_load_rt.check_path,
            &core_load_rt.c))
      core_load_check_stage_settle(false, CORE_LOAD_GRANT_SLOT);
   f = NULL;
end:
   /* A lent g went to the stage or stays its record's */
   if (g && !lent)
      core_file_close(g);
   core_file_close(f);
   core_trust_set_put(s);
}

static void core_load_check_stage_result(const char *path, bool pass)
{
   if (     core_load_rt.stage != CORE_LOAD_STAGE_CHECK
         || !string_is_equal(core_load_rt.stage_path, path))
      return;
   core_load_check_stage_settle(pass, CORE_LOAD_GRANT_SLOT);
}

static void core_load_park_drop(void)
{
   struct core_load_park *p = &core_load_rt.park;
   core_load_park_end_t end = p->end;
   unsigned serial          = p->serial;
   core_file_close(p->f);
   if (p->free_cb)
      p->free_cb(p->userdata);
   memset(p, 0, sizeof(*p));
   p->serial                = serial;
   runloop_frame_work_set(RUNLOOP_WORK_CORE_PARK, false);
   if (end)
      end(CORE_LOAD_PARK_END_DROPPED);
}

/* Ended as the user's Cancel ends it: a warning up for it closes
 * through its own Cancel, which drops the park */
static void core_load_park_cancel(void)
{
   if (core_load_rt.park.state == CORE_LOAD_PARK_USER)
      core_warning_choose(CORE_WARNING_CANCEL);
   if (core_load_rt.park.state != CORE_LOAD_PARK_NONE)
      core_load_park_drop();
}

static void core_load_check_refusal_shown(void)
{
   const char *msg = msg_hash_to_str(MSG_CORE_LOAD_REFUSED_NO_MENU);
   runloop_msg_queue_push(msg, strlen(msg), 2, 360, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
}

/* No screen to ask on. The load ends, saying how to approve it. */
static void core_load_check_refusal(const char *path,
      enum core_check_result result)
{
   RARCH_WARN("[Core] Not loading \"%s\": %s Start RetroArch once with a menu driver and choose Always Trust to approve it.\n",
         path, msg_hash_to_str(core_warning_reason(result)));
   if (core_load_rt.notified)
      core_load_check_refusal_shown();
   else
      core_load_rt.refusal_held = true;
}

static bool core_load_check_can_ask(void)
{
#ifdef HAVE_MENU
   return menu_core_warning_can_open();
#else
   return false;
#endif
}

/* The parked load won't run. A startup load with no menu to fall back
 * to then ends RetroArch, as its failure before the frame loop would. */
static void core_load_park_fail(void)
{
   bool quit = core_load_rt.park.startup && !core_load_check_can_ask();
   core_load_park_drop();
   if (quit)
      retroarch_main_quit_failure();
}

static void core_load_park_choice(enum core_warning_choice choice,
      const struct core_check *c, const char *core_path, void *userdata)
{
   struct core_load_park *p = &core_load_rt.park;
   unsigned serial          = *(unsigned*)userdata;
   bool took;
   (void)core_path;
   free(userdata);
   /* A newer push runs this with CANCEL inside itself, its request
    * already in the warning's slot: only our own park may go, and
    * nothing here pushes or chooses. The re-run starts from the frame
    * loop. */
   if (serial != p->serial || p->state != CORE_LOAD_PARK_USER)
      return;
   /* Either way the re-run opens only the file the user was asked
    * about: core_load_park_rerun() binds the pass to it */
   switch (choice)
   {
      case CORE_WARNING_LOAD_ONCE:
         p->state = CORE_LOAD_PARK_IDLE;
         break;
      case CORE_WARNING_ALWAYS_TRUST:
         /* Not recorded, or not kept, so this once */
         if (     !(took = core_trust_runtime_always(c))
               || (  !core_trust_runtime_can_save()
                  && !core_trust_runtime_waits(c)))
         {
            const char *msg = msg_hash_to_str(took
                  ? MSG_CORE_LOAD_TRUST_CANT_SAVE
                  : MSG_CORE_LOAD_TRUST_LIST_FULL);
            RARCH_WARN("[Core] Could not trust \"%s\" for good: loading it once.\n",
                  p->path);
            runloop_msg_queue_push(msg, strlen(msg), 2, 360, false, NULL,
                  MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
            if (took)
               core_trust_runtime_cant_save_told();
         }
         p->state = CORE_LOAD_PARK_IDLE;
         break;
      default:
         /* Pending inside a CANCEL only for a newer push's request */
         if (core_warning_pending(NULL, NULL))
            RARCH_WARN("[Core] Not loading \"%s\": another Security Warning replaced its warning.\n",
                  p->path);
         /* Its end runs from the frame loop, outside this warning */
         if (p->end)
            p->state = CORE_LOAD_PARK_CANCELLED;
         else
            core_load_park_drop();
         break;
   }
}

/* The parked core has its answer: run again, or ask */
static void core_load_park_answer(void)
{
   struct core_load_park *p = &core_load_rt.park;
   unsigned *token;
   if (p->c.result == CORE_CHECK_OK)
   {
      p->state = CORE_LOAD_PARK_IDLE;
      return;
   }
   p->state = CORE_LOAD_PARK_USER;
   if ((token = (unsigned*)malloc(sizeof(*token))))
   {
      *token = p->serial;
      if (core_warning_push(&p->c, CORE_WARNING_LOAD, p->path,
               core_load_park_choice, token))
         return;
      free(token);
   }
   core_load_check_refusal(p->path, p->c.result);
   core_load_park_fail();
}

/* The park takes the slot's file g; the slot keeps the answer */
static void core_load_park_take_slot(core_file_t *g, bool lent)
{
   struct core_load_park *p = &core_load_rt.park;
   p->c = core_load_rt.slot.c;
   if (lent)
      core_load_rt.slot.f = NULL;
   p->f = g;
   core_load_park_answer();
}

/* In order: the quick check, a pass the slot holds, the seed's wait, a
 * refusal the slot holds for this file and store, or a check task */
static void core_load_park_start(void)
{
   struct core_load_park *p = &core_load_rt.park;
   core_trust_set_t *s      = core_trust_runtime_get();
   struct core_identity id;
   core_file_t *f           = NULL;
   core_file_t *g           = NULL;   /* the slot's file */
   bool lent                = false;
   /* Unopenable: dropped, so core_load_check_park() returns false */
   if (     !s
         || !core_load_check_file_path(p->path, core_load_rt.check_path,
            sizeof(core_load_rt.check_path))
         || !(f = core_file_open_id(core_load_rt.check_path, &id)))
   {
      core_load_park_fail();
      goto end;
   }
   if (core_check_quick_id(s, f, core_load_rt.check_path, &id, &p->c))
   {
      p->f = f;
      f    = NULL;
      core_load_park_answer();
      goto end;
   }
   if (core_load_slot_is(&core_load_rt.slot, p->path))
      g = core_load_slot_file(&core_load_rt.slot, core_load_rt.check_path,
            &lent);
   if (g && core_load_rt.slot.pass)
   {
      core_load_park_take_slot(g, lent);
      g = NULL;
      goto end;
   }
   if (     core_trust_runtime_seed_pending()
         && core_trust_runtime_seed_covers(p->path))
   {
      p->state = CORE_LOAD_PARK_SEED;
      goto end;
   }
   /* Only the file hashed may be answered, so g must be held or bound
    * by its ctime */
   if (     g
         && (lent || core_load_rt.slot.c.rec.id.ctime_kept)
         && core_load_slot_refusal_holds(s))
   {
      core_load_park_take_slot(g, lent);
      g = NULL;
      goto end;
   }
   if (core_load_slot_is(&core_load_rt.slot, p->path))
   {
      /* Replaced, or the store changed, since its check */
      if (lent)
         g = NULL;
      core_load_slot_clear(&core_load_rt.slot);
   }
   p->state = CORE_LOAD_PARK_CHECK;
   /* No check will answer: the load won't run */
   if (!core_load_check_push(f, p->path, core_load_rt.check_path, &p->c))
      core_load_park_fail();
   f = NULL;
end:
   if (g && !lent)
      core_file_close(g);
   core_file_close(f);
   core_trust_set_put(s);
}

/* checked: false for a check cancelled before its verdict */
static void core_load_park_result(const char *path, bool checked)
{
   struct core_load_park *p = &core_load_rt.park;
   if (     p->state != CORE_LOAD_PARK_CHECK
         || !string_is_equal(p->path, path))
      return;
   if (!checked)
   {
      /* Only quitting cancels a check: the quit sets the exit status */
      core_load_park_drop();
      return;
   }
   /* The stage's re-check of a moved file cleared it: joined */
   if (!core_load_slot_is(&core_load_rt.slot, path))
   {
      core_load_park_start();
      return;
   }
   p->c                = core_load_rt.slot.c;
   p->f                = core_load_rt.slot.f;
   core_load_rt.slot.f = NULL;
   core_load_park_answer();
}

static void core_load_park_rerun(void)
{
   struct core_load_park *p  = &core_load_rt.park;
   core_load_rerun_t rerun   = p->rerun;
   core_load_rerun_t free_cb = p->free_cb;
   core_load_park_end_t end  = p->end;
   void *userdata            = p->userdata;
   /* The pass for the re-run's open, bound to the file checked: kept
    * open for a refused result (the user chose), else by identity */
   core_load_slot_set(&core_load_rt.slot, p->path, &p->c, &p->f, true);
   core_load_rt.slot.pass    = true;
   p->free_cb                = NULL;
   p->end                    = NULL;
   core_load_park_drop();
   /* Only the re-run's own open may refuse it again */
   core_load_rt.refused[0]   = '\0';
   if (end)
      end(CORE_LOAD_PARK_END_RERUN);
   rerun(userdata);
   if (end)
      end(CORE_LOAD_PARK_END_RERAN);
   if (free_cb)
      free_cb(userdata);
}

/* The startup load runs every stage in one pass before the frame loop
 * (rarch_main), so nothing can wait for a task: the core is hashed in
 * place. Unreachable once the frame loop runs. */
static enum core_load_gate core_load_check_inline(
      core_trust_set_t **s, core_file_t **f, const char *path,
      dylib_t *lib)
{
   struct core_hash h;
   int step = -1;
   if (core_hash_init(&h))
   {
      do
         step = core_hash_step(&h, *f);
      while (step > 0);
      core_load_check_complete(*s, &h, step, core_load_rt.check_path,
            &core_load_rt.c);
   }
   else
      core_load_rt.c.result = CORE_CHECK_ERROR;
   if (!core_trust_runtime_apply(&core_load_rt.c))
      RARCH_WARN("[Core] Could not record the check of \"%s\".\n", path);
   core_load_slot_set(&core_load_rt.slot, path, &core_load_rt.c, f, true);
   core_load_slot_keep_store(&core_load_rt.slot, s);
   if (!core_load_rt.slot.pass)
      return CORE_LOAD_GATE_REFUSED;
   *lib = core_load_file(core_load_rt.slot.f
         ? core_load_rt.slot.f : *f, path);
   core_load_rt.granted = CORE_LOAD_GRANT_SLOT;
   return CORE_LOAD_GATE_OPENED;
}

enum core_load_gate core_load_check_open(const char *path, dylib_t *lib)
{
   enum core_load_gate ret = CORE_LOAD_GATE_REFUSED;
   core_trust_set_t *s     = NULL;
   struct core_identity id;
   core_file_t *f          = NULL;
   core_file_t *g          = NULL;   /* a kept record's file */
   bool lent               = false;
   bool checked            = false;
   bool seed               = false;

   *lib                    = NULL;
   core_load_rt.granted    = CORE_LOAD_GRANT_NONE;
   if (!(s = core_trust_runtime_get()))
      return CORE_LOAD_GATE_OFF;
   core_load_abs(path, core_load_rt.path, sizeof(core_load_rt.path));
   path = core_load_rt.path;
   if (!core_load_check_file_path(path, core_load_rt.check_path,
            sizeof(core_load_rt.check_path)))
   {
#ifdef _WIN32
      RARCH_ERR("[Core] Not loading \"%s\": a \\\\?\\ path ending in '.' may load another file.\n",
            path);
#else
      RARCH_ERR("[Core] Not loading \"%s\": no binary in the framework.\n",
            path);
#endif
      goto end;
   }
   /* The core loaded last, opened again for its system info or its
    * reload */
   if ((g = core_load_loaded_file(path, &lent)))
   {
      *lib                 = core_load_file(g, path);
      core_load_rt.granted = CORE_LOAD_GRANT_LOADED;
      ret                  = CORE_LOAD_GATE_OPENED;
      goto end;
   }
   /* Unreadable or gone: no check's refusal, so the caller reports it */
   if (!(f = core_file_open_id(core_load_rt.check_path, &id)))
   {
      RARCH_ERR("[Core] Could not open \"%s\".\n", path);
      goto end;
   }
   checked = true;
   /* First: checks off, a record or an exemption answer whatever the
    * slot holds */
   if (core_check_quick_id(s, f, core_load_rt.check_path, &id,
            &core_load_rt.c))
   {
      if (core_load_rt.c.result == CORE_CHECK_OK)
      {
         *lib = core_load_file(f, path);
         core_load_slot_clear(&core_load_rt.quick);
         core_load_rt.quick.c    = core_load_rt.c;
         core_load_rt.quick.used = true;
         core_load_rt.quick.pass = true;
         strlcpy(core_load_rt.quick.path, path,
               sizeof(core_load_rt.quick.path));
         core_load_rt.granted    = CORE_LOAD_GRANT_QUICK;
         ret                     = CORE_LOAD_GATE_OPENED;
      }
      goto end;
   }
   if (     core_load_slot_is(&core_load_rt.slot, path)
         && (g = core_load_slot_file(&core_load_rt.slot,
               core_load_rt.check_path, &lent))
         &&  core_load_rt.slot.pass)
   {
      *lib                 = core_load_file(g, path);
      core_load_rt.granted = CORE_LOAD_GRANT_SLOT;
      ret                  = CORE_LOAD_GATE_OPENED;
      goto end;
   }
   /* The seed decides first. That it covers the core is a guess,
    * so nothing is checked or kept before it. */
   if (     core_trust_runtime_seed_pending()
         && core_trust_runtime_seed_covers(path))
   {
      RARCH_LOG("[Core] Not loading \"%s\" before the first run's check of its folder.\n",
            path);
      seed = true;
      /* The startup load can't wait for the seed: it runs again once
       * the seed is done */
      if (core_load_rt.startup)
         core_load_rt.startup_state = CORE_LOAD_STARTUP_SEED;
      goto end;
   }
   /* Refused for this file against this store: no second hash */
   if (g && core_load_slot_refusal_holds(s))
      goto end;
   if (core_load_rt.startup)
   {
      ret = core_load_check_inline(&s, &f, path, lib);
      if (ret == CORE_LOAD_GATE_REFUSED)
      {
         /* Asked from the menu the failure falls back to; with no menu
          * it fails, saying why */
         if (core_load_check_can_ask())
            core_load_rt.startup_state = CORE_LOAD_STARTUP_REFUSED;
         else
         {
            core_load_check_refusal(path, core_load_rt.slot.c.result);
            core_load_rt.startup_state = CORE_LOAD_STARTUP_FAILED;
         }
      }
      goto end;
   }
   if (core_load_slot_is(&core_load_rt.slot, path))
   {
      /* Replaced, or the store changed, since its check */
      if (lent)
         g = NULL;
      core_load_slot_clear(&core_load_rt.slot);
   }
   if (!string_is_equal(core_load_rt.checking, path))
      RARCH_LOG("[Core] Checking \"%s\" before it loads.\n", path);
   core_load_check_push(f, path, core_load_rt.check_path,
         &core_load_rt.c);
   f = NULL;
end:
   if (g && !lent)
      core_file_close(g);
   core_file_close(f);
   core_trust_set_put(s);
   if (ret == CORE_LOAD_GATE_REFUSED && checked)
      strlcpy(core_load_rt.refused, path, sizeof(core_load_rt.refused));
   else
      core_load_rt.refused[0] = '\0';
   core_load_rt.refused_seed = seed;
   return ret;
}

bool core_load_check_refused(const char *path)
{
   if (!path || !*core_load_rt.refused)
      return false;
#ifdef ANDROID
   /* The gate never sees a path it can't resolve */
   if (!android_core_path_resolve(path, core_load_rt.check_path,
            sizeof(core_load_rt.check_path)))
      return false;
   path = core_load_rt.check_path;
#endif
   core_load_abs(path, core_load_rt.path, sizeof(core_load_rt.path));
   return string_is_equal(core_load_rt.path, core_load_rt.refused);
}

bool core_load_check_refused_seed(const char *path)
{
   return core_load_rt.refused_seed && core_load_check_refused(path);
}

void core_load_check_forget(const char *path)
{
   if (!path || !*path)
      return;
#ifdef ANDROID
   if (android_core_path_resolve(path, core_load_rt.check_path,
            sizeof(core_load_rt.check_path)))
      path = core_load_rt.check_path;
#endif
   core_load_abs(path, core_load_rt.path, sizeof(core_load_rt.path));
   if (     core_load_rt.slot.pass
         && core_load_slot_is(&core_load_rt.slot, core_load_rt.path))
      core_load_slot_clear(&core_load_rt.slot);
}

void core_load_check_loaded(void)
{
   struct core_load_slot *from = NULL;
   core_load_rt.running        = true;
   /* A load while a park waits, the same core's too: the re-run would
    * replace that session. The park's own re-run dropped it first. */
   if (core_load_rt.park.state != CORE_LOAD_PARK_NONE)
   {
      RARCH_WARN("[Core] Not loading \"%s\": another load replaced it.\n",
            core_load_rt.park.path);
      core_load_park_cancel();
   }
   /* Any other pass the slot keeps, such as a Load Once whose re-run
    * only probed, ends when a core loads */
   if (     core_load_rt.slot.pass
         && core_load_rt.granted != CORE_LOAD_GRANT_SLOT)
      core_load_slot_clear(&core_load_rt.slot);
   if (core_load_rt.granted == CORE_LOAD_GRANT_LOADED)
      return;
   core_load_slot_clear(&core_load_rt.loaded);
   if (core_load_rt.granted == CORE_LOAD_GRANT_SLOT)
      from = &core_load_rt.slot;
   else if (core_load_rt.granted == CORE_LOAD_GRANT_QUICK)
      from = &core_load_rt.quick;
   core_load_rt.granted = CORE_LOAD_GRANT_NONE;
   if (!from)
      return;
   core_load_rt.loaded = *from;
   memset(from, 0, sizeof(*from));
   /* Loaded: the identity binds it from now on where it can, else only
    * its held file; with neither, no probe passes on it */
   if (core_load_rt.loaded.c.rec.id.ctime_kept)
   {
      core_file_close(core_load_rt.loaded.f);
      core_load_rt.loaded.f = NULL;
   }
   else if (!core_load_rt.loaded.f)
      core_load_rt.loaded.pass = false;
}

void core_load_check_builtin_loaded(void)
{
   /* Opened for no library: a probe's grant isn't this load's */
   core_load_rt.granted = CORE_LOAD_GRANT_NONE;
   core_load_check_loaded();
}

void core_load_check_unloaded(void)
{
   core_load_rt.running = false;
   /* Kept while it passes, Load Once too: the reload after Close
    * Content and menu or driver init pass it */
   if (!core_load_rt.loaded.pass)
      core_load_slot_clear(&core_load_rt.loaded);
}

void core_load_check_release_held(void)
{
   core_file_close(core_load_rt.loaded.f);
   core_load_rt.loaded.f    = NULL;
   core_load_rt.loaded.pass = false;
   /* A running core's record stays for run-ahead's source */
   if (!core_load_rt.running)
      core_load_slot_clear(&core_load_rt.loaded);
   /* A pass or a refusal: no kept file may block the task */
   if (core_load_rt.slot.f)
      core_load_slot_clear(&core_load_rt.slot);
}

void core_load_check_untrust(const char *path)
{
   if (!path)
      return;
#ifdef ANDROID
   if (!android_core_path_resolve(path, core_load_rt.check_path,
            sizeof(core_load_rt.check_path)))
      return;
   path = core_load_rt.check_path;
#endif
   core_load_abs(path, core_load_rt.path, sizeof(core_load_rt.path));
   path = core_load_rt.path;
   if (core_load_slot_is(&core_load_rt.slot, path))
      core_load_slot_clear(&core_load_rt.slot);
   if (core_load_slot_is(&core_load_rt.quick, path))
      core_load_slot_clear(&core_load_rt.quick);
   if (!core_load_slot_is(&core_load_rt.loaded, path))
      return;
   core_file_close(core_load_rt.loaded.f);
   core_load_rt.loaded.f    = NULL;
   core_load_rt.loaded.pass = false;
   if (!core_load_rt.running)
      core_load_slot_clear(&core_load_rt.loaded);
}

void core_load_check_set_startup(bool on)
{
   core_load_rt.startup       = on;
   core_load_rt.startup_state = CORE_LOAD_STARTUP_NONE;
}

enum core_load_startup core_load_check_startup_take(void)
{
   enum core_load_startup state = core_load_rt.startup_state;
   core_load_rt.startup_state   = CORE_LOAD_STARTUP_NONE;
   return state;
}

enum core_load_copy core_load_check_copy_source(const char *path,
      struct core_load_source *src)
{
   memset(src, 0, sizeof(*src));
   if (!core_trust_runtime_active() || !core_trust_runtime_checks_on())
      return CORE_LOAD_COPY_OFF;
#ifdef ANDROID
   if (android_core_path_resolve(path, core_load_rt.check_path,
            sizeof(core_load_rt.check_path)))
      path = core_load_rt.check_path;
#endif
   core_load_abs(path, core_load_rt.path, sizeof(core_load_rt.path));
   /* The record can outlive its core's unload */
   if (     !core_load_rt.running
         || !core_load_slot_is(&core_load_rt.loaded, core_load_rt.path))
      return CORE_LOAD_COPY_NONE;
   src->id     = core_load_rt.loaded.c.rec.id;
   src->hashed = core_load_rt.loaded.hashed;
   if (src->hashed)
      memcpy(src->sha256, core_load_rt.loaded.c.rec.sha256,
            CORE_SHA256_LEN);
   return CORE_LOAD_COPY_CHECK;
}

#ifdef ANDROID
core_file_t *core_load_check_instance_file(const char *path,
      char **load_path)
{
   char fd_path[32];
   core_file_t *f    = NULL;
   core_file_t *own  = NULL;
   bool lent;
   *load_path = NULL;
   /* As the running core was loaded: by its real path */
   if (android_core_path_resolve(path, core_load_rt.check_path,
            sizeof(core_load_rt.check_path)))
      path = core_load_rt.check_path;
   if (!core_trust_runtime_active() || !core_trust_runtime_checks_on())
      own = core_file_open(path);
   else
   {
      core_load_abs(path, core_load_rt.path, sizeof(core_load_rt.path));
      if (     !core_load_rt.running
            || !core_load_rt.loaded.pass
            || !core_load_slot_is(&core_load_rt.loaded, core_load_rt.path)
            || !(f = core_load_slot_file(&core_load_rt.loaded,
                  core_load_rt.check_path, &lent)))
         return NULL;
      if (!lent)
         own = f;
      else
      {
         /* The held file, opened again: an offset of the caller's own */
         snprintf(fd_path, sizeof(fd_path), "/proc/self/fd/%d",
               core_file_fd(f));
         if (     (own = core_file_open(fd_path))
               && !core_load_same_held(&core_load_rt.loaded.c.rec.id, own))
         {
            core_file_close(own);
            own = NULL;
         }
      }
   }
   if (own && !(*load_path = strdup(path)))
   {
      core_file_close(own);
      own = NULL;
   }
   return own;
}
#endif

struct core_load_match
{
   struct core_hash h;
   struct core_load_source src;
   struct core_identity id;      /* the copy's, as the match began */
   uint8_t sha256[CORE_SHA256_LEN];
   core_file_t *copy;
   core_file_t *source;          /* open while it is hashed */
   char *copy_path;
   bool hash_live;
};

void core_load_match_free(core_load_match_t *m)
{
   if (!m)
      return;
   if (m->hash_live)
      core_hash_free(&m->h);
   core_file_close(m->copy);
   core_file_close(m->source);
   free(m->copy_path);
   free(m);
}

core_load_match_t *core_load_match_new(const char *copy_path,
      const char *src_path, const struct core_load_source *src)
{
   core_load_match_t *m = (core_load_match_t*)calloc(1, sizeof(*m));
   if (!m)
      return NULL;
   m->src = *src;
   if (     !(m->copy_path = strdup(copy_path))
         || !(m->copy      = core_file_open(copy_path))
         || !core_file_identity(m->copy, &m->id))
      goto fail;
   /* No hash from its check: hash the running core's file, if it is
    * still the one loaded */
   if (     !src->hashed
         && (     !(m->source = core_file_open(src_path))
               || !core_load_same_file(&src->id, m->source)))
      goto fail;
   if (!(m->hash_live = core_hash_init(&m->h)))
      goto fail;
   return m;
fail:
   core_load_match_free(m);
   return NULL;
}

int core_load_match_step(core_load_match_t *m)
{
   core_file_t *f                 = m->source ? m->source : m->copy;
   const struct core_identity *id = m->source ? &m->src.id : &m->id;
   int step                       = core_hash_step(&m->h, f);
   if (step > 0)
      return 1;
   /* Written since the match began: not the bytes hashed */
   if (step < 0 || !core_load_same_file(id, f) || m->h.done != id->size)
      return -1;
   m->hash_live = false;
   if (m->source)
   {
      core_hash_final(&m->h, m->src.sha256);
      core_file_close(m->source);
      m->source    = NULL;
      m->hash_live = core_hash_init(&m->h);
      return m->hash_live ? 1 : -1;
   }
   core_hash_final(&m->h, m->sha256);
   return memcmp(m->sha256, m->src.sha256, CORE_SHA256_LEN) ? -1 : 0;
}

dylib_t core_load_match_load(core_load_match_t *m)
{
#ifndef ANDROID
   core_file_t *f;
   bool same;
#endif
#ifdef _WIN32
   if (     core_load_dll_appended(m->copy_path)
         || core_load_verbatim_dot(m->copy_path))
      return NULL;
#endif
#ifndef ANDROID
   /* Loaded by path outside Android: only while the path still names
    * the copy hashed, unchanged */
   f    = core_file_open(m->copy_path);
   same = f && core_load_same_file(&m->id, f);
   core_file_close(f);
   if (!same)
      return NULL;
#endif
   return core_load_file(m->copy, m->copy_path);
}

bool core_load_check_stage_begin(const char *path)
{
   if (!core_trust_runtime_active())
      return false;
   core_load_check_stage_cancel();
   /* Only an open after this counts as this load's refusal */
   core_load_rt.refused[0] = '\0';
   core_load_abs(path, core_load_rt.stage_path,
         sizeof(core_load_rt.stage_path));
   core_load_check_stage_start();
   if (     core_load_rt.stage == CORE_LOAD_STAGE_SEED
         || core_load_rt.stage == CORE_LOAD_STAGE_CHECK)
      core_load_check_osd_checking(core_load_rt.stage_path);
   return true;
}

bool core_load_check_stage_ready(void)
{
   if (     core_load_rt.stage == CORE_LOAD_STAGE_SEED
         && !core_trust_runtime_seed_pending())
      core_load_check_stage_start();
   return core_load_rt.stage == CORE_LOAD_STAGE_DONE
      ||  core_load_rt.stage == CORE_LOAD_STAGE_NONE;
}

bool core_load_check_stage_refused(void)
{
   return core_load_rt.stage == CORE_LOAD_STAGE_DONE
      &&  core_load_rt.stage_refused;
}

dylib_t core_load_check_stage_take(const char *path)
{
   dylib_t lib = NULL;
   if (core_load_rt.stage == CORE_LOAD_STAGE_DONE && core_load_rt.stage_lib)
   {
      core_load_abs(path, core_load_rt.path, sizeof(core_load_rt.path));
      if (string_is_equal(core_load_rt.path, core_load_rt.stage_path))
      {
         lib                    = core_load_rt.stage_lib;
         core_load_rt.stage_lib = NULL;
         /* Not a grant a probe made while it opened */
         core_load_rt.granted   = CORE_LOAD_GRANT_NONE;
         if (core_load_rt.stage_grant == CORE_LOAD_GRANT_QUICK)
         {
            /* A probe's quick pass may have taken its place */
            core_load_slot_clear(&core_load_rt.quick);
            core_load_rt.quick.c    = core_load_rt.stage_c;
            core_load_rt.quick.used = true;
            core_load_rt.quick.pass = true;
            strlcpy(core_load_rt.quick.path, core_load_rt.stage_path,
                  sizeof(core_load_rt.quick.path));
            core_load_rt.granted    = CORE_LOAD_GRANT_QUICK;
         }
         else if (core_load_slot_is(&core_load_rt.slot,
                  core_load_rt.stage_path))
            core_load_rt.granted = CORE_LOAD_GRANT_SLOT;
      }
   }
   core_load_check_stage_cancel();
   return lib;
}

void core_load_check_stage_cancel(void)
{
   if (core_load_rt.stage_lib)
   {
      dylib_close(core_load_rt.stage_lib);
      core_load_rt.stage_lib = NULL;
   }
   core_load_check_stage_file_drop();
   core_load_rt.stage_refused = false;
   core_load_rt.stage_moved   = false;
   core_load_rt.stage         = CORE_LOAD_STAGE_NONE;
   core_load_rt.stage_gen++;
}

bool core_load_check_park(const char *path, core_load_rerun_t rerun,
      core_load_rerun_t free_cb, void *userdata)
{
   struct core_load_park *p = &core_load_rt.park;
   if (!core_trust_runtime_active() || !rerun || !path)
      return false;
#ifdef ANDROID
   /* Checked as the open will be: the real path, in the app's folders */
   if (!android_core_path_resolve(path, core_load_rt.check_path,
            sizeof(core_load_rt.check_path)))
      return false;
   path = core_load_rt.check_path;
#endif
   if (p->state != CORE_LOAD_PARK_NONE)
   {
      RARCH_WARN("[Core] Not loading \"%s\": another load replaced it.\n",
            p->path);
      core_load_park_drop();
   }
   core_load_abs(path, p->path, sizeof(p->path));
   /* Before the start can push: the older request's CANCEL, run inside
    * that push, must not match this park */
   p->serial++;
   p->rerun    = rerun;
   p->startup  = core_load_rt.startup;
   /* Set once parked: a drop during the start leaves userdata to the
    * caller (nothing re-runs before the next frame) */
   p->free_cb  = NULL;
   p->userdata = userdata;
   core_load_park_start();
   if (p->state == CORE_LOAD_PARK_NONE)
      return false;
   /* Polled each frame until it re-runs or is dropped */
   runloop_frame_work_set(RUNLOOP_WORK_CORE_PARK, true);
   p->free_cb  = free_cb;
   if (     p->state == CORE_LOAD_PARK_SEED
         || p->state == CORE_LOAD_PARK_CHECK)
      core_load_check_osd_checking(p->path);
   return true;
}

void core_load_check_park_on_end(core_load_park_end_t end)
{
   if (core_load_rt.park.state != CORE_LOAD_PARK_NONE)
      core_load_rt.park.end = end;
}

void core_load_check_iterate(void)
{
   switch (core_load_rt.park.state)
   {
      case CORE_LOAD_PARK_SEED:
         if (!core_trust_runtime_seed_pending())
            core_load_park_start();
         break;
      case CORE_LOAD_PARK_IDLE:
         /* A refused staged load ends its own way first */
         if (!task_content_load_pending())
            core_load_park_rerun();
         break;
      case CORE_LOAD_PARK_CANCELLED:
         if (!task_content_load_pending())
         {
            core_load_park_end_t end = core_load_rt.park.end;
            core_load_rt.park.end    = NULL;
            core_load_park_drop();
            if (end)
               end(CORE_LOAD_PARK_END_CANCELLED);
         }
         break;
      default:
         break;
   }
}

void core_load_check_notify(void)
{
   core_load_rt.notified = true;
   if (core_load_rt.refusal_held)
   {
      core_load_rt.refusal_held = false;
      core_load_check_refusal_shown();
   }
}

void core_load_check_deinit(void)
{
   core_load_check_stage_cancel();
   if (core_load_rt.park.state != CORE_LOAD_PARK_NONE)
      core_load_park_drop();
   core_load_rt.gen++;
   core_load_slot_clear(&core_load_rt.slot);
   core_load_slot_clear(&core_load_rt.loaded);
   core_load_slot_clear(&core_load_rt.quick);
   core_load_rt.checking[0]  = '\0';
   core_load_rt.refused[0]   = '\0';
   core_load_rt.granted      = CORE_LOAD_GRANT_NONE;
   core_load_rt.startup      = false;
   core_load_rt.notified     = false;
   core_load_rt.refusal_held = false;
   core_load_rt.running      = false;
}
