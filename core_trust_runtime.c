/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if defined(HAVE_CORE_LOAD_CHECK) && !defined(_WIN32)
#include <sys/stat.h>
#endif

#include <compat/strl.h>
#include <file/file_path.h>
#include <queues/task_queue.h>
#include <retro_dirent.h>
#include <retro_miscellaneous.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "core_trust_runtime.h"
#ifdef HAVE_CORE_LOAD_CHECK
#include "core_load_check.h"
#endif
#include "runloop.h"
#include "tasks/tasks_internal.h"
#include "verbosity.h"

static struct
{
   core_trust_t *t;
   /* Tells a task finishing after deinit it belongs to an older store */
   unsigned gen;
   bool read_only;
   /* Read-only: a newer version's store */
   bool newer;
   /* Read-only: a store file that couldn't be read */
   bool unreadable;
   /* Changed since the last write that went through */
   bool unsaved;
   /* The user changed it, and no write has taken that out yet */
   bool user_unsaved;
   /* The user was told the list can't be saved */
   bool cant_save_told;
   bool notify_ready;
   /* Deinit is cancelling the warning: no new one */
   bool closing;
   /* From a failed write until one goes through */
   bool write_failed;
   bool seed_pending;
   bool seed_notice;
   bool seed_key_ok;
   uint8_t seed_key[CORE_SHA256_LEN];
#ifdef HAVE_CORE_LOAD_CHECK
   /* Kept for the sweep */
   char seed_dir[PATH_MAX_LENGTH];
   /* The cores directory setting's: the sweep walks it too */
   char cores_dir[PATH_MAX_LENGTH];
   char core_ext[16];
   /* What Always Trust let in this session, oldest first: no sweep
    * drops it */
   uint8_t session[CORE_TRUST_MAX_HASHES][CORE_SHA256_LEN];
   unsigned num_session;
   /* Refused only for a full list, waiting on the sweep running */
   struct core_check sweep_wait[CORE_TRUST_SWEEP_WAIT_MAX];
   unsigned num_sweep_wait;
   bool sweeping;
#endif
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   bool test_key_shown;
#endif
   char path[PATH_MAX_LENGTH];
} core_trust_rt;

static struct
{
   struct core_check c;
   char *path;
   core_warning_cb_t cb;
   void *userdata;
   enum core_warning_kind kind;
   unsigned serial;
   bool pending;
   /* A newer push is running the replaced request's CANCEL */
   bool cancelling;
} core_warning_req;

struct core_trust_seed;

struct core_trust_save
{
   core_trust_write_t *w;
   char *path;
   /* The seed's own write: it walks once this went through */
   struct core_trust_seed *sd;
   unsigned gen;
   /* Carries a change the user made */
   bool user;
   bool written;
};

static bool core_trust_runtime_save(struct core_trust_seed *sd);
#ifdef HAVE_CORE_LOAD_CHECK
static void core_trust_seed_saved(struct core_trust_seed *sd, bool walk);
static void core_trust_seed_free(struct core_trust_seed *sd);
#endif

/* Logged each time; on screen once a session, for a change the user
 * made, unless they were told the list can't be saved */
static void core_trust_save_failed(const char *path, bool user)
{
   const char *msg;
   RARCH_WARN("[Core] Could not save \"%s\".\n", path);
   if (!user)
      return;
   if (core_trust_rt.cant_save_told)
      return;
   core_trust_rt.cant_save_told = true;
   msg = msg_hash_to_str(MSG_CORE_TRUST_SAVE_FAILED);
   runloop_msg_queue_push(msg, strlen(msg), 2, 360, false, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
}

static void core_trust_save_handler(retro_task_t *task)
{
   struct core_trust_save *st = (struct core_trust_save*)task->state;
   /* Runs even when cancelled at quit: one small file */
   st->written = core_trust_write_run(st->w, st->path);
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void core_trust_save_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   struct core_trust_save *st = (struct core_trust_save*)task->state;
   bool again;
   if (!core_trust_rt.t || st->gen != core_trust_rt.gen)
      return;
   again = core_trust_write_end(core_trust_rt.t, st->w, st->written);
   st->w = NULL;
   core_trust_rt.write_failed = !st->written;
   /* The seed's own is logged below */
   if (!st->written && !st->sd)
      core_trust_save_failed(st->path, st->user);
#ifdef HAVE_CORE_LOAD_CHECK
   if (st->sd)
   {
      /* Unseeded in memory too, or a later write or the exit write
       * would put SEEDED on disk with nothing seeded */
      if (!st->written)
      {
         RARCH_WARN("[Core] Could not save \"%s\": the cores already "
               "installed are not trusted.\n", st->path);
         core_trust_set_flag(core_trust_rt.t, CORE_TRUST_SEEDED, false);
      }
      core_trust_seed_saved(st->sd, st->written
            && !(task_get_flags(task) & RETRO_TASK_FLG_CANCELLED));
      st->sd = NULL;
   }
#endif
   if (again)
      core_trust_runtime_save(NULL);
   else if (st->written)
      core_trust_rt.unsaved = false;
}

static void core_trust_save_cleanup(retro_task_t *task)
{
   struct core_trust_save *st = (struct core_trust_save*)task->state;
#ifdef HAVE_CORE_LOAD_CHECK
   /* Its callback never ran */
   if (st->sd)
      core_trust_seed_free(st->sd);
#endif
   free(st->path);
   free(st);
   task->state = NULL;
}

/* True: the task is out, and owns sd */
static bool core_trust_runtime_save(struct core_trust_seed *sd)
{
   struct core_trust_save *st = NULL;
   retro_task_t *task         = NULL;
   core_trust_write_t *w;
   if (core_trust_rt.read_only)
      return false;
   core_trust_rt.unsaved = true;
   /* NULL: out of memory, or a write is out and runs again after it */
   if (!(w = core_trust_write_begin(core_trust_rt.t)))
      return false;
   if (     !(st = (struct core_trust_save*)calloc(1, sizeof(*st)))
         || !(st->path = strdup(core_trust_rt.path))
         || !(task = task_init()))
   {
      if (st)
         free(st->path);
      free(st);
      core_trust_write_end(core_trust_rt.t, w, false);
      return false;
   }
   st->w          = w;
   st->sd         = sd;
   st->gen        = core_trust_rt.gen;
   st->user       = core_trust_rt.user_unsaved;
   task->state    = st;
   task->handler  = core_trust_save_handler;
   task->callback = core_trust_save_cb;
   task->cleanup  = core_trust_save_cleanup;
   task->flags   |= RETRO_TASK_FLG_MUTE;
   core_trust_rt.user_unsaved = false;
   task_queue_push(task);
   return true;
}

/* Saves when before is no longer the store's set; user when the user
 * made the change */
static bool core_trust_runtime_changed(core_trust_set_t *before, bool user)
{
   core_trust_set_t *now = core_trust_get(core_trust_rt.t);
   bool changed          = now != before;
   core_trust_set_put(now);
   core_trust_set_put(before);
   if (changed)
   {
      if (user)
         core_trust_rt.user_unsaved = true;
      core_trust_runtime_save(NULL);
   }
   return changed;
}

static unsigned core_trust_runtime_flags(void)
{
   core_trust_set_t *s = core_trust_get(core_trust_rt.t);
   unsigned flags      = core_trust_flags(s);
   core_trust_set_put(s);
   return flags;
}

static void core_trust_runtime_show(void)
{
   if (!core_trust_rt.t || !core_trust_rt.notify_ready)
      return;
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   if (!core_trust_rt.test_key_shown)
   {
      core_trust_rt.test_key_shown = true;
      RARCH_LOG("[Core] This build trusts a test signing key: "
            "cores anyone can sign will run.\n");
      if (config_get_ptr()->bools.notification_show_core_signing_test_key)
      {
         const char *msg = msg_hash_to_str(MSG_CORE_SIGNING_TEST_KEY);
         runloop_msg_queue_push(msg, strlen(msg), 2, 180, false, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
      }
   }
#endif
   if (core_trust_rt.seed_notice)
   {
      const char *msg = msg_hash_to_str(MSG_CORE_TRUST_SEEDED);
      core_trust_rt.seed_notice = false;
      runloop_msg_queue_push(msg, strlen(msg), 1, 600, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
}

#ifdef HAVE_CORE_LOAD_CHECK
struct core_trust_seed
{
   struct core_hash h;
   char dir[PATH_MAX_LENGTH];
   /* The sweep's folder after dir, or empty */
   char next_dir[PATH_MAX_LENGTH];
   char path[PATH_MAX_LENGTH];
#ifdef CORE_TRUST_FRAMEWORKS
   /* The entry's .framework folder, path its binary */
   char framework[PATH_MAX_LENGTH];
#endif
   char ext[16];
   uint8_t folder[CORE_SHA256_LEN];
   struct RDIR *rdir;
   core_file_t *f;
   core_trust_set_t *set;
   /* At most CORE_TRUST_MAX_HASHES: no more can be trusted. The check
    * is built in found[num_found], counted once it hashed in full; a
    * sweep's one is never counted */
   struct core_check *found;
   /* The sweep's: which of set's hashes a core or this session holds,
    * then the rest, to drop */
   bool *live;
   uint8_t (*drop)[CORE_SHA256_LEN];
   unsigned num_drop;
   unsigned num_found;
   unsigned gen;
   bool opened;
   bool sweep;
   /* The entry's check carries its own folder key: a framework's */
   bool own_folder;
};

static void core_trust_sweep_live(struct core_trust_seed *sd,
      const uint8_t *sha256)
{
   unsigned i;
   unsigned n = core_trust_num_hashes(sd->set);
   for (i = 0; i < n; i++)
      if (!memcmp(core_trust_hash_at(sd->set, i), sha256, CORE_SHA256_LEN))
         sd->live[i] = true;
}

/* What the record of the file open in sd says, unread: true when that
 * settles it */
static bool core_trust_sweep_known(struct core_trust_seed *sd,
      struct core_check *c)
{
   uint8_t sha256[CORE_SHA256_LEN];
   if (!sd->own_folder)
      memcpy(c->folder, sd->folder, CORE_SHA256_LEN);
   if (core_check_wants_verity(sd->set, c))
      core_file_verity_measure(sd->f, &c->rec.id);
   switch (core_trust_state_of(sd->set, c, sha256))
   {
      case CORE_TRUST_STATE_TRUSTED:
         core_trust_sweep_live(sd, sha256);
         return true;
      case CORE_TRUST_STATE_SIGNED:
         return true;
      default:
         break;
   }
   return false;
}

/* The walk's end: what nothing kept */
static void core_trust_sweep_drops(struct core_trust_seed *sd)
{
   unsigned i;
   unsigned n = core_trust_num_hashes(sd->set);
   /* A folder that didn't open proves nothing */
   if (!sd->rdir)
      return;
   for (i = 0; i < n; i++)
      if (!sd->live[i])
         memcpy(sd->drop[sd->num_drop++], core_trust_hash_at(sd->set, i),
               CORE_SHA256_LEN);
}

/* One read, one directory entry or the folder's open: false when the
 * walk is over */
static bool core_trust_seed_step(struct core_trust_seed *sd)
{
   const char *name;
   bool is_dir;
   struct core_check *c = &sd->found[sd->num_found];
   if (sd->f)
   {
      uint8_t sha256[CORE_SHA256_LEN];
      int step = core_hash_step(&sd->h, sd->f);
      if (step > 0)
         return true;
      if (step == 0 && sd->h.done == c->rec.id.size)
      {
         core_hash_final(&sd->h, sha256);
         if (sd->sweep)
            core_trust_sweep_live(sd, sha256);
         else
         {
            core_check_finish(sd->set, NULL, 0, sha256, NULL, 0, 0, c);
            sd->num_found++;
         }
      }
      else
         core_hash_free(&sd->h);
      core_file_close(sd->f);
      sd->f = NULL;
      return sd->sweep || sd->num_found < CORE_TRUST_MAX_HASHES;
   }
   if (!sd->opened)
   {
      sd->opened = true;
      if (sd->sweep)
      {
         /* Any name: the key is of the folder holding it */
         fill_pathname_join_special(sd->path, sd->dir, "core",
               sizeof(sd->path));
         core_folder_key(sd->path, sd->folder);
      }
      sd->rdir   = retro_opendir(sd->dir);
      return sd->rdir != NULL;
   }
   if (!sd->rdir)
      return false;
   if (!retro_readdir(sd->rdir))
   {
      if (!*sd->next_dir)
         return false;
      retro_closedir(sd->rdir);
      sd->rdir   = NULL;
      sd->opened = false;
      strlcpy(sd->dir, sd->next_dir, sizeof(sd->dir));
      *sd->next_dir = '\0';
      return true;
   }
   if (     !(name = retro_dirent_get_name(sd->rdir))
         || !string_is_equal_noncase(path_get_extension(name), sd->ext))
      return true;
   is_dir         = retro_dirent_is_dir(sd->rdir, NULL);
   sd->own_folder = false;
#ifndef CORE_TRUST_FRAMEWORKS
   if (is_dir)
      return true;
#endif
   fill_pathname_join_special(sd->path, sd->dir, name, sizeof(sd->path));
#ifdef CORE_TRUST_FRAMEWORKS
   /* A framework is its binary, as the load check reads it */
   if (is_dir)
   {
#ifndef _WIN32
      struct stat st;
      if (lstat(sd->path, &st) != 0 || !S_ISDIR(st.st_mode))
         return true;
#endif
      strlcpy(sd->framework, sd->path, sizeof(sd->framework));
      if (!core_load_framework_binary(sd->framework, sd->path,
               sizeof(sd->path)))
         return true;
      sd->own_folder = true;
   }
#endif
#ifndef _WIN32
   {
      /* What a symlink names lives outside the folder */
      struct stat st;
      if (lstat(sd->path, &st) != 0 || !S_ISREG(st.st_mode))
         return true;
   }
#endif
   memset(c, 0, sizeof(*c));
   strlcpy(c->rec.name, path_basename_nocompression(sd->path),
         sizeof(c->rec.name));
   if (sd->own_folder && !core_folder_key(sd->path, c->folder))
      return true;
   /* Not a regular file, or gone: core_file_open refuses it */
   if (!(sd->f = core_file_open(sd->path)))
      return true;
   if (     !core_file_identity(sd->f, &c->rec.id)
         || (sd->sweep && core_trust_sweep_known(sd, c))
         || !core_hash_init(&sd->h))
   {
      core_file_close(sd->f);
      sd->f = NULL;
   }
   return true;
}

static void core_trust_seed_handler(retro_task_t *task)
{
   struct core_trust_seed *sd = (struct core_trust_seed*)task->state;
   nbio_budget_t budget;
   if (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED)
   {
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
      return;
   }
   task_nbio_slice_open(&budget);
   while (task_nbio_slice_within_budget(&budget, 0, 0))
   {
      if (!core_trust_seed_step(sd))
      {
         if (sd->sweep)
            core_trust_sweep_drops(sd);
         task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
         break;
      }
   }
   task_nbio_slice_close(&budget);
}

static void core_trust_seed_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   struct core_trust_seed *sd = (struct core_trust_seed*)task->state;
   core_trust_set_t *before;
   unsigned trusted;
   if (!core_trust_rt.t || sd->gen != core_trust_rt.gen)
      return;
   core_trust_rt.seed_pending = false;
   /* Cut short at quit too: what it found is kept */
   before  = core_trust_get(core_trust_rt.t);
   trusted = core_trust_always_all(core_trust_rt.t, sd->found,
         sd->num_found, NULL);
   if (     trusted
         && !(core_trust_runtime_flags() & CORE_TRUST_NOTICE_SHOWN))
   {
      core_trust_set_flag(core_trust_rt.t, CORE_TRUST_NOTICE_SHOWN, true);
      core_trust_rt.seed_notice = true;
   }
   core_trust_runtime_changed(before, false);
   core_trust_runtime_show();
}

static void core_trust_seed_free(struct core_trust_seed *sd)
{
   if (sd->f)
   {
      core_hash_free(&sd->h);
      core_file_close(sd->f);
   }
   if (sd->rdir)
      retro_closedir(sd->rdir);
   core_trust_set_put(sd->set);
   free(sd->found);
   free(sd->live);
   free(sd->drop);
   free(sd);
}

static void core_trust_seed_cleanup(retro_task_t *task)
{
   core_trust_seed_free((struct core_trust_seed*)task->state);
   task->state = NULL;
}

/* The seed's own write ended: nothing is hashed before SEEDED is on disk */
static void core_trust_seed_saved(struct core_trust_seed *sd, bool walk)
{
   retro_task_t *task = NULL;
   if (!walk || !(task = task_init()))
   {
      core_trust_rt.seed_pending = false;
      core_trust_seed_free(sd);
      return;
   }
   task->state    = sd;
   task->handler  = core_trust_seed_handler;
   task->callback = core_trust_seed_cb;
   task->cleanup  = core_trust_seed_cleanup;
   task->flags   |= RETRO_TASK_FLG_MUTE;
   task_queue_push(task);
}

static void core_trust_seed_start(const char *seed_dir,
      const char *core_ext)
{
   struct core_trust_seed *sd;
   if (     core_trust_rt.read_only
         || string_is_empty(seed_dir)
         || string_is_empty(core_ext)
         || (core_trust_runtime_flags() & CORE_TRUST_SEEDED)
         || !(sd = (struct core_trust_seed*)calloc(1, sizeof(*sd))))
      return;
   /* Seeded on disk before anything is hashed, so a seed cut short
    * never runs again */
   if (     !(sd->found = (struct core_check*)malloc(
               CORE_TRUST_MAX_HASHES * sizeof(*sd->found)))
         || !core_trust_set_flag(core_trust_rt.t, CORE_TRUST_SEEDED, true))
   {
      free(sd->found);
      free(sd);
      return;
   }
   strlcpy(sd->dir, seed_dir, sizeof(sd->dir));
   strlcpy(sd->ext, core_ext, sizeof(sd->ext));
   sd->set = core_trust_get(core_trust_rt.t);
   sd->gen = core_trust_rt.gen;
   /* Any name: the key is of the folder holding it */
   fill_pathname_join_special(sd->path, seed_dir, "core", sizeof(sd->path));
   core_trust_rt.seed_key_ok  = core_folder_key_with(sd->path, true,
         core_trust_rt.seed_key);
   core_trust_rt.seed_pending = core_trust_runtime_save(sd);
   if (!core_trust_rt.seed_pending)
      core_trust_seed_free(sd);
}

static void core_trust_session_add(const uint8_t *sha256)
{
   unsigned i;
   for (i = 0; i < core_trust_rt.num_session; i++)
      if (!memcmp(core_trust_rt.session[i], sha256, CORE_SHA256_LEN))
         return;
   if (core_trust_rt.num_session == CORE_TRUST_MAX_HASHES)
      memmove(core_trust_rt.session[0], core_trust_rt.session[1],
            (CORE_TRUST_MAX_HASHES - 1) * CORE_SHA256_LEN);
   else
      core_trust_rt.num_session++;
   memcpy(core_trust_rt.session[core_trust_rt.num_session - 1], sha256,
         CORE_SHA256_LEN);
}

static void core_trust_session_drop(const uint8_t *sha256)
{
   unsigned i;
   for (i = 0; i < core_trust_rt.num_session; i++)
      if (!memcmp(core_trust_rt.session[i], sha256, CORE_SHA256_LEN))
      {
         memmove(core_trust_rt.session[i], core_trust_rt.session[i + 1],
               (core_trust_rt.num_session - i - 1) * CORE_SHA256_LEN);
         core_trust_rt.num_session--;
         return;
      }
}

static void core_trust_sweep_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   struct core_trust_seed *sd = (struct core_trust_seed*)task->state;
   core_trust_set_t *before;
   bool took[CORE_TRUST_SWEEP_WAIT_MAX];
   unsigned i, n;
   unsigned refused = 0;
   if (!core_trust_rt.t || sd->gen != core_trust_rt.gen)
      return;
   n                            = core_trust_rt.num_sweep_wait;
   core_trust_rt.num_sweep_wait = 0;
   core_trust_rt.sweeping       = false;
   /* Cut short at quit: the waiting cores ask again next time */
   if (task_get_flags(task) & RETRO_TASK_FLG_CANCELLED)
      return;
   before = core_trust_get(core_trust_rt.t);
   core_trust_forget(core_trust_rt.t, sd->drop[0], sd->num_drop);
   core_trust_always_all(core_trust_rt.t, core_trust_rt.sweep_wait, n,
         took);
   for (i = 0; i < n; i++)
   {
      if (took[i])
         core_trust_session_add(core_trust_rt.sweep_wait[i].rec.sha256);
      else
         refused++;
   }
   if (refused)
   {
      const char *msg = msg_hash_to_str(MSG_CORE_LOAD_TRUST_LIST_FULL);
      RARCH_WARN("[Core] Could not trust %u core(s) for good: the trusted "
            "cores list is still full.\n", refused);
      runloop_msg_queue_push(msg, strlen(msg), 2, 360, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
   }
   if (refused < n && !core_trust_runtime_can_save())
   {
      const char *msg = msg_hash_to_str(MSG_CORE_LOAD_TRUST_CANT_SAVE);
      runloop_msg_queue_push(msg, strlen(msg), 2, 360, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
      core_trust_runtime_cant_save_told();
   }
   core_trust_runtime_changed(before, refused < n);
}

/* Walks the seed folder and the cores directory for the hashes no core
 * in either has */
static bool core_trust_sweep_start(void)
{
   struct core_trust_seed *sd;
   retro_task_t *task = NULL;
   unsigned i, j, n;
   if (     (  string_is_empty(core_trust_rt.seed_dir)
            && string_is_empty(core_trust_rt.cores_dir))
         || string_is_empty(core_trust_rt.core_ext)
         || !(sd = (struct core_trust_seed*)calloc(1, sizeof(*sd))))
      return false;
   sd->set = core_trust_get(core_trust_rt.t);
   n       = core_trust_num_hashes(sd->set);
   if (     !(sd->found = (struct core_check*)malloc(sizeof(*sd->found)))
         || !(sd->live = (bool*)calloc(n, sizeof(*sd->live)))
         || !(sd->drop = (uint8_t (*)[CORE_SHA256_LEN])malloc(
               n * CORE_SHA256_LEN))
         || !(task = task_init()))
   {
      core_trust_seed_free(sd);
      return false;
   }
   if (string_is_empty(core_trust_rt.seed_dir))
      strlcpy(sd->dir, core_trust_rt.cores_dir, sizeof(sd->dir));
   else
   {
      strlcpy(sd->dir, core_trust_rt.seed_dir, sizeof(sd->dir));
      if (!string_is_equal(sd->dir, core_trust_rt.cores_dir))
         strlcpy(sd->next_dir, core_trust_rt.cores_dir,
               sizeof(sd->next_dir));
   }
   strlcpy(sd->ext, core_trust_rt.core_ext, sizeof(sd->ext));
   sd->gen   = core_trust_rt.gen;
   sd->sweep = true;
   /* This session's trusts, as they are now */
   for (i = 0; i < n; i++)
      for (j = 0; j < core_trust_rt.num_session && !sd->live[i]; j++)
         sd->live[i] = !memcmp(core_trust_hash_at(sd->set, i),
               core_trust_rt.session[j], CORE_SHA256_LEN);
   task->state    = sd;
   task->handler  = core_trust_seed_handler;
   task->callback = core_trust_sweep_cb;
   task->cleanup  = core_trust_seed_cleanup;
   task->flags   |= RETRO_TASK_FLG_MUTE;
   task_queue_push(task);
   core_trust_rt.sweeping = true;
   return true;
}

/* c, refused by s: it waits on a sweep when only a full list refused it */
static bool core_trust_sweep_wait(const core_trust_set_t *s,
      const struct core_check *c)
{
   if (     !core_trust_full_refuses(s, c)
         || core_trust_rt.num_sweep_wait == CORE_TRUST_SWEEP_WAIT_MAX
         || (!core_trust_rt.sweeping && !core_trust_sweep_start()))
      return false;
   core_trust_rt.sweep_wait[core_trust_rt.num_sweep_wait++] = *c;
   return true;
}
#endif

static bool core_trust_runtime_default_path(char *s, size_t len)
{
#if defined(__APPLE__) && TARGET_OS_OSX
   /* The user's own folder, never beside the app as portable.txt,
    * RAPortableInstall or a Steam build would have it */
   const char *home = getenv("HOME");
   if (string_is_empty(home))
      return false;
   fill_pathname_join_special(s, home,
         "Library/Application Support/RetroArch", len);
   path_mkdir(s);
   fill_pathname_slash(s, len);
   strlcat(s, CORE_TRUST_FILE_NAME, len);
#else
   if (!core_trust_default_path(s, len))
      return false;
#endif
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   /* What the test key verified stays out of release builds' store */
   strlcat(s, ".testkey", len);
#endif
   return true;
}

/* Seconds: a save in flight is far younger */
#define CORE_TRUST_TEMP_AGE 3600

/* What a save killed before its rename left */
static void core_trust_temp_sweep(void)
{
   const char *name;
   struct RDIR *rdir;
   int64_t mtime;
   int flags;
   char path[PATH_MAX_LENGTH];
   const char *store = path_basename_nocompression(core_trust_rt.path);
   int64_t before    = (int64_t)time(NULL) - CORE_TRUST_TEMP_AGE;
   size_t n          = fill_pathname_basedir(path, core_trust_rt.path,
         sizeof(path));
   if (!(rdir = retro_opendir(path)))
      return;
   while (retro_readdir(rdir))
   {
      if (     !(name = retro_dirent_get_name(rdir))
            || !core_trust_temp_of(name, store))
         continue;
      flags = retro_dirent_stat(rdir, NULL, &mtime);
      if (     (flags & RETRO_VFS_STAT_IS_VALID)
            && !(flags & RETRO_VFS_STAT_IS_DIRECTORY)
            && mtime < before
            && strlcpy(path + n, name, sizeof(path) - n) < sizeof(path) - n)
         filestream_delete(path);
   }
   retro_closedir(rdir);
}

bool core_trust_runtime_init(const char *path, const char *seed_dir,
      const char *core_ext)
{
   enum core_trust_load load;
   size_t num_keys;
   if (core_trust_rt.t)
      return true;
   if (     !core_sig_builtin_keys(&num_keys)
         || !(core_trust_rt.t = core_trust_new()))
      return false;
   core_trust_rt.unsaved = false;
   if (path)
      strlcpy(core_trust_rt.path, path, sizeof(core_trust_rt.path));
   /* No file to keep it in: checks stay on, for this session */
   if (!path && !core_trust_runtime_default_path(core_trust_rt.path,
            sizeof(core_trust_rt.path)))
      core_trust_rt.read_only = true;
   else
   {
      load = core_trust_load_file(core_trust_rt.t, core_trust_rt.path);
      core_trust_rt.newer      = load == CORE_TRUST_LOAD_NEWER;
      core_trust_rt.unreadable = load == CORE_TRUST_LOAD_UNREADABLE;
      core_trust_rt.read_only  = core_trust_rt.newer
         || core_trust_rt.unreadable;
   }
   if (!core_trust_rt.read_only)
      core_trust_temp_sweep();
#ifdef HAVE_CORE_LOAD_CHECK
   if (seed_dir)
      strlcpy(core_trust_rt.seed_dir, seed_dir,
            sizeof(core_trust_rt.seed_dir));
   if (core_ext)
      strlcpy(core_trust_rt.core_ext, core_ext,
            sizeof(core_trust_rt.core_ext));
   core_trust_seed_start(seed_dir, core_ext);
#else
   (void)seed_dir;
   (void)core_ext;
#endif
   return true;
}

void core_trust_runtime_deinit(void)
{
   core_trust_write_t *w;
   unsigned gen = core_trust_rt.gen + 1;
   if (!core_trust_rt.t)
      return;
   core_trust_rt.closing = true;
   if (core_warning_req.pending)
      core_warning_choose(CORE_WARNING_CANCEL);
   /* At quit, as retroarch.cfg is: the queue is drained by now,
    * within the drain's limit */
   if (     core_trust_rt.unsaved
         && !core_trust_rt.read_only
         && (w = core_trust_write_begin(core_trust_rt.t)))
   {
      bool written = core_trust_write_run(w, core_trust_rt.path);
      if (!written)
         core_trust_save_failed(core_trust_rt.path, false);
      core_trust_write_end(core_trust_rt.t, w, written);
   }
   core_trust_free(core_trust_rt.t);
   memset(&core_trust_rt, 0, sizeof(core_trust_rt));
   core_trust_rt.gen = gen;
}

void core_trust_runtime_notify(void)
{
   core_trust_rt.notify_ready = true;
   core_trust_runtime_show();
}

bool core_trust_runtime_active(void)
{
   return core_trust_rt.t != NULL;
}

bool core_trust_runtime_read_only(void)
{
   return core_trust_rt.read_only;
}

bool core_trust_runtime_newer(void)
{
   return core_trust_rt.newer;
}

bool core_trust_runtime_unreadable(void)
{
   return core_trust_rt.unreadable;
}

bool core_trust_runtime_can_save(void)
{
   return   core_trust_rt.t
         && !core_trust_rt.read_only
         && !core_trust_rt.write_failed;
}

void core_trust_runtime_cant_save_told(void)
{
   if (core_trust_rt.t)
      core_trust_rt.cant_save_told = true;
}

core_trust_set_t *core_trust_runtime_get(void)
{
   return core_trust_rt.t ? core_trust_get(core_trust_rt.t) : NULL;
}

bool core_trust_runtime_apply(const struct core_check *c)
{
   core_trust_set_t *before;
   if (!core_trust_rt.t)
      return false;
   before = core_trust_get(core_trust_rt.t);
   if (!core_trust_apply(core_trust_rt.t, c))
   {
      core_trust_set_put(before);
      return false;
   }
   core_trust_runtime_changed(before, false);
   return true;
}

bool core_trust_runtime_always(const struct core_check *c)
{
   core_trust_set_t *before;
   if (!core_trust_rt.t)
      return false;
   before = core_trust_get(core_trust_rt.t);
   if (!core_trust_always(core_trust_rt.t, c))
   {
      bool waits = false;
#ifdef HAVE_CORE_LOAD_CHECK
      waits = core_trust_sweep_wait(before, c);
#endif
      core_trust_set_put(before);
      return waits;
   }
#ifdef HAVE_CORE_LOAD_CHECK
   core_trust_session_add(c->rec.sha256);
#endif
   core_trust_runtime_changed(before, true);
   return true;
}

void core_trust_runtime_set_cores_dir(const char *dir)
{
#ifdef HAVE_CORE_LOAD_CHECK
   if (dir)
      strlcpy(core_trust_rt.cores_dir, dir,
            sizeof(core_trust_rt.cores_dir));
   else
      *core_trust_rt.cores_dir = '\0';
#else
   (void)dir;
#endif
}

bool core_trust_runtime_waits(const struct core_check *c)
{
#ifdef HAVE_CORE_LOAD_CHECK
   unsigned i;
   for (i = 0; i < core_trust_rt.num_sweep_wait; i++)
      if (!memcmp(core_trust_rt.sweep_wait[i].rec.sha256, c->rec.sha256,
               CORE_SHA256_LEN))
         return true;
#else
   (void)c;
#endif
   return false;
}

bool core_trust_runtime_forget(const char *core_path)
{
   uint8_t sha256[CORE_SHA256_LEN];
   core_trust_set_t *before;
   if (!core_trust_rt.t || !core_path)
      return false;
   before = core_trust_get(core_trust_rt.t);
   if (     core_trust_state_file(before, core_path, sha256)
            != CORE_TRUST_STATE_TRUSTED
         || !core_trust_forget(core_trust_rt.t, sha256, 1))
   {
      core_trust_set_put(before);
      return false;
   }
   core_trust_runtime_changed(before, true);
#ifdef HAVE_CORE_LOAD_CHECK
   core_trust_session_drop(sha256);
   core_load_check_untrust(core_path);
#endif
   return true;
}

enum core_trust_state core_trust_runtime_state(const char *core_path)
{
   enum core_trust_state state;
   core_trust_set_t *s;
   if (!core_trust_rt.t || !core_path)
      return CORE_TRUST_STATE_UNCHECKED;
   s     = core_trust_get(core_trust_rt.t);
   state = core_trust_state_file(s, core_path, NULL);
   core_trust_set_put(s);
   return state;
}

bool core_trust_runtime_checks_on(void)
{
   return core_trust_rt.t
      && !(core_trust_runtime_flags() & CORE_TRUST_CHECK_OFF);
}

bool core_trust_runtime_set_checks(bool on)
{
   core_trust_set_t *before;
   if (!core_trust_rt.t || core_trust_rt.read_only)
      return false;
   before = core_trust_get(core_trust_rt.t);
   if (!core_trust_set_flag(core_trust_rt.t, CORE_TRUST_CHECK_OFF, !on))
   {
      core_trust_set_put(before);
      return false;
   }
   core_trust_runtime_changed(before, true);
   return true;
}

enum core_trust_toggle core_trust_runtime_toggle_checks(void)
{
   if (!core_trust_rt.t || core_trust_rt.read_only)
      return CORE_TRUST_TOGGLE_READ_ONLY;
   if (core_trust_runtime_checks_on())
      return CORE_TRUST_TOGGLE_CONFIRM_OFF;
   core_trust_runtime_set_checks(true);
   return CORE_TRUST_TOGGLE_DONE;
}

bool core_trust_runtime_seed_pending(void)
{
   return core_trust_rt.seed_pending;
}

bool core_trust_runtime_seed_covers(const char *core_path)
{
   uint8_t key[CORE_SHA256_LEN];
   /* The per-folder rule everywhere: Android's core_folder_key() gives
    * every folder one key */
   return   core_trust_rt.seed_pending
         && core_trust_rt.seed_key_ok
         && core_path
         && core_folder_key_with(core_path, true, key)
         && !memcmp(key, core_trust_rt.seed_key, CORE_SHA256_LEN);
}

bool core_warning_push(const struct core_check *c,
      enum core_warning_kind kind, const char *core_path,
      core_warning_cb_t cb, void *userdata)
{
#ifdef HAVE_MENU
   struct core_check old_c;
   char *path;
   char *old_path;
   core_warning_cb_t old_cb;
   void *old_userdata;
   bool had;
   if (     !core_trust_rt.t
         || core_trust_rt.closing
         || !c
         || !cb
         || !core_path
         || c->result == CORE_CHECK_OK
         || !menu_core_warning_can_open()
         || !(path = strdup(core_path)))
      return false;
   had          = core_warning_req.pending;
   old_c        = core_warning_req.c;
   old_path     = core_warning_req.path;
   old_cb       = core_warning_req.cb;
   old_userdata = core_warning_req.userdata;
   core_warning_req.c        = *c;
   core_warning_req.path     = path;
   core_warning_req.cb       = cb;
   core_warning_req.userdata = userdata;
   core_warning_req.kind     = kind;
   core_warning_req.pending  = true;
   if (!++core_warning_req.serial)
      core_warning_req.serial++;
   menu_core_warning_open();
   if (had)
   {
      core_warning_req.cancelling = true;
      old_cb(CORE_WARNING_CANCEL, &old_c, old_path, old_userdata);
      core_warning_req.cancelling = false;
      free(old_path);
   }
   return true;
#else
   (void)c;
   (void)kind;
   (void)core_path;
   (void)cb;
   (void)userdata;
   return false;
#endif
}

bool core_warning_pending(enum core_warning_kind *kind,
      enum core_check_result *result)
{
   if (!core_warning_req.pending)
      return false;
   if (kind)
      *kind = core_warning_req.kind;
   if (result)
      *result = core_warning_req.c.result;
   return true;
}

unsigned core_warning_serial(void)
{
   return core_warning_req.pending ? core_warning_req.serial : 0;
}

unsigned core_warning_choices(enum core_warning_choice *choices)
{
   unsigned n   = 0;
   choices[n++] = CORE_WARNING_CANCEL;
   /* Nothing was hashed, so there is nothing to trust */
   if (     !core_warning_req.pending
         || core_warning_req.c.result == CORE_CHECK_ERROR)
      return n;
   if (core_warning_req.kind == CORE_WARNING_INSTALL)
      choices[n++] = CORE_WARNING_INSTALL_AND_TRUST;
   else
   {
      choices[n++] = CORE_WARNING_LOAD_ONCE;
      choices[n++] = CORE_WARNING_ALWAYS_TRUST;
   }
   return n;
}

bool core_warning_choose(enum core_warning_choice choice)
{
   enum core_warning_choice choices[CORE_WARNING_CHOICES_MAX];
   struct core_check c;
   core_warning_cb_t cb;
   void *userdata;
   char *path;
   unsigned n;
   unsigned i = 0;
   /* What is pending then is the newer request, not the one cancelled */
   if (!core_warning_req.pending || core_warning_req.cancelling)
      return false;
   n = core_warning_choices(choices);
   while (i < n && choices[i] != choice)
      i++;
   if (i == n)
      return false;
   c        = core_warning_req.c;
   path     = core_warning_req.path;
   cb       = core_warning_req.cb;
   userdata = core_warning_req.userdata;
   core_warning_req.pending = false;
   core_warning_req.path    = NULL;
   cb(choice, &c, path, userdata);
   free(path);
   return true;
}

enum msg_hash_enums core_warning_reason(enum core_check_result result)
{
   switch (result)
   {
      case CORE_CHECK_UNSIGNED:
         return MSG_CORE_WARNING_UNSIGNED;
      case CORE_CHECK_MODIFIED:
         return MSG_CORE_WARNING_MODIFIED;
      case CORE_CHECK_OLDER:
         return MSG_CORE_WARNING_OLDER;
      case CORE_CHECK_UNKNOWN_KEY:
         return MSG_CORE_WARNING_UNKNOWN_KEY;
      default:
         break;
   }
   return MSG_CORE_WARNING_ERROR;
}

bool core_warning_same_core(const struct core_check *warned,
      const struct core_check *now)
{
   static const uint8_t none[CORE_SHA256_LEN];
   return   memcmp(warned->rec.sha256, none, CORE_SHA256_LEN)
         && !memcmp(warned->rec.sha256, now->rec.sha256, CORE_SHA256_LEN)
         && string_is_equal(warned->rec.name, now->rec.name);
}
