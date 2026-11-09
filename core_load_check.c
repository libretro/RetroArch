/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <queues/task_queue.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#include "core_load_check.h"
#include "core_load_file.h"
#include "core_trust_runtime.h"
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
};

enum core_load_grant
{
   CORE_LOAD_GRANT_NONE = 0,
   CORE_LOAD_GRANT_SLOT,
   CORE_LOAD_GRANT_QUICK,
   CORE_LOAD_GRANT_LOADED
};

static struct
{
   struct core_load_slot slot;    /* the latest check */
   struct core_load_slot loaded;  /* the core loaded last */
   struct core_load_slot quick;   /* the last quick pass */
   struct core_check c;
   char path[PATH_MAX_LENGTH];
   char check_path[PATH_MAX_LENGTH];
   char refused[PATH_MAX_LENGTH];
   char checking[PATH_MAX_LENGTH];
   enum core_load_grant granted;
   unsigned gen;
   bool startup;
   bool refused_seed;  /* refused only until the seed is done */
} core_load_rt;

/* A user-facing request waits for path's check: its refusal keeps the
 * file, and a quiet result never replaces its slot */
static bool core_load_check_waited(const char *path)
{
   (void)path;
   return false;
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
   core_load_check_complete(t->s, &t->h, step, t->check_path, &t->c);
   t->hash_live = false;
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

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
   /* Its hash may not have ended: no verdict */
   if (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED)
      return;
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
 * A check of path already running is joined instead. */
static void core_load_check_push(core_file_t *f, const char *path,
      const char *check_path, const struct core_check *c)
{
   struct core_load_check_task *t = NULL;
   retro_task_t *task             = NULL;
   if (string_is_equal(core_load_rt.checking, path))
   {
      core_file_close(f);
      return;
   }
   if (!(t = (struct core_load_check_task*)calloc(1, sizeof(*t))))
   {
      core_file_close(f);
      return;
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
      return;
   }
   task->handler  = core_load_check_handler;
   task->callback = core_load_check_cb;
   task->cleanup  = core_load_check_cleanup;
   task->state    = t;
   task->flags   |= RETRO_TASK_FLG_MUTE;
   strlcpy(core_load_rt.checking, path, sizeof(core_load_rt.checking));
   task_queue_push(task);
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
   /* The running core, opened again for its system info */
   if (     core_load_slot_is(&core_load_rt.loaded, path)
         && (g = core_load_slot_file(&core_load_rt.loaded,
               core_load_rt.check_path, &lent)))
   {
      *lib                 = core_load_file(g, path);
      core_load_rt.granted = CORE_LOAD_GRANT_LOADED;
      ret                  = CORE_LOAD_GATE_OPENED;
      goto end;
   }
   /* Unreadable or gone: no check's refusal, so the caller reports it */
   if (!(f = core_file_open(core_load_rt.check_path)))
   {
      RARCH_ERR("[Core] Could not open \"%s\".\n", path);
      goto end;
   }
   checked = true;
   /* First: checks off, a record or an exemption answer whatever the
    * slot holds */
   if (core_check_quick(s, f, core_load_rt.check_path, &core_load_rt.c))
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
      goto end;
   }
   /* Refused for this file against this store: no second hash */
   if (g && core_load_slot_refusal_holds(s))
      goto end;
   if (core_load_rt.startup)
   {
      ret = core_load_check_inline(&s, &f, path, lib);
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

void core_load_check_loaded(void)
{
   struct core_load_slot *from = NULL;
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
   /* Loaded: the identity binds it from now on, where it can */
   if (core_load_rt.loaded.f && core_load_rt.loaded.c.rec.id.ctime_kept)
   {
      core_file_close(core_load_rt.loaded.f);
      core_load_rt.loaded.f = NULL;
   }
}

void core_load_check_unloaded(void)
{
   core_load_slot_clear(&core_load_rt.loaded);
}

void core_load_check_set_startup(bool on)
{
   core_load_rt.startup = on;
}

void core_load_check_deinit(void)
{
   core_load_rt.gen++;
   core_load_slot_clear(&core_load_rt.slot);
   core_load_slot_clear(&core_load_rt.loaded);
   core_load_slot_clear(&core_load_rt.quick);
   core_load_rt.checking[0] = '\0';
   core_load_rt.refused[0]  = '\0';
   core_load_rt.granted     = CORE_LOAD_GRANT_NONE;
   core_load_rt.startup     = false;
}
