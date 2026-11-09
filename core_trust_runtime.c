/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdlib.h>
#include <string.h>
#if defined(HAVE_CORE_LOAD_CHECK) && !defined(_WIN32)
#include <sys/stat.h>
#endif

#include <compat/strl.h>
#include <file/file_path.h>
#include <queues/task_queue.h>
#include <retro_dirent.h>
#include <retro_miscellaneous.h>
#include <string/stdstring.h>

#include "core_trust_runtime.h"
#include "runloop.h"
#include "tasks/tasks_internal.h"

static struct
{
   core_trust_t *t;
   /* Tells a task finishing after deinit it belongs to an older store */
   unsigned gen;
   bool read_only;
   /* Changed since the last write that went through */
   bool unsaved;
   bool notify_ready;
   bool seed_pending;
   bool seed_notice;
   bool seed_key_ok;
   uint8_t seed_key[CORE_SHA256_LEN];
#ifdef HAVE_CORE_SIGNING_TEST_KEY
   bool test_key_shown;
#endif
   char path[PATH_MAX_LENGTH];
} core_trust_rt;

struct core_trust_save
{
   core_trust_write_t *w;
   char *path;
   unsigned gen;
   bool written;
};

static void core_trust_runtime_save(void);

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
   if (again)
      core_trust_runtime_save();
   else if (st->written)
      core_trust_rt.unsaved = false;
}

static void core_trust_save_cleanup(retro_task_t *task)
{
   struct core_trust_save *st = (struct core_trust_save*)task->state;
   free(st->path);
   free(st);
   task->state = NULL;
}

static void core_trust_runtime_save(void)
{
   struct core_trust_save *st = NULL;
   retro_task_t *task         = NULL;
   core_trust_write_t *w;
   if (core_trust_rt.read_only)
      return;
   core_trust_rt.unsaved = true;
   /* NULL: out of memory, or a write is out and runs again after it */
   if (!(w = core_trust_write_begin(core_trust_rt.t)))
      return;
   if (     !(st = (struct core_trust_save*)calloc(1, sizeof(*st)))
         || !(st->path = strdup(core_trust_rt.path))
         || !(task = task_init()))
   {
      if (st)
         free(st->path);
      free(st);
      core_trust_write_end(core_trust_rt.t, w, false);
      return;
   }
   st->w          = w;
   st->gen        = core_trust_rt.gen;
   task->state    = st;
   task->handler  = core_trust_save_handler;
   task->callback = core_trust_save_cb;
   task->cleanup  = core_trust_save_cleanup;
   task->flags   |= RETRO_TASK_FLG_MUTE;
   task_queue_push(task);
}

/* Saves when before is no longer the store's set */
static bool core_trust_runtime_changed(core_trust_set_t *before)
{
   core_trust_set_t *now = core_trust_get(core_trust_rt.t);
   bool changed          = now != before;
   core_trust_set_put(now);
   core_trust_set_put(before);
   if (changed)
      core_trust_runtime_save();
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
      const char *msg = msg_hash_to_str(MSG_CORE_SIGNING_TEST_KEY);
      core_trust_rt.test_key_shown = true;
      runloop_msg_queue_push(msg, strlen(msg), 2, 600, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
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
   char path[PATH_MAX_LENGTH];
   char ext[16];
   struct RDIR *rdir;
   core_file_t *f;
   core_trust_set_t *set;
   /* At most CORE_TRUST_MAX_HASHES: no more can be trusted. The check
    * is built in found[num_found], counted once it hashed in full */
   struct core_check *found;
   unsigned num_found;
   unsigned gen;
   bool opened;
   /* The folder is there but couldn't be opened: seeded at a later start */
   bool unopened;
   bool done;
};

/* One read, one directory entry or the folder's open: false when the
 * walk is over */
static bool core_trust_seed_step(struct core_trust_seed *sd)
{
   const char *name;
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
         core_check_finish(sd->set, NULL, 0, sha256, NULL, 0, 0, c);
         sd->num_found++;
      }
      else
         core_hash_free(&sd->h);
      core_file_close(sd->f);
      sd->f = NULL;
      return sd->num_found < CORE_TRUST_MAX_HASHES;
   }
   if (!sd->opened)
   {
      sd->opened = true;
      if (!(sd->rdir = retro_opendir(sd->dir)))
         sd->unopened = path_is_valid(sd->dir);
      return sd->rdir != NULL;
   }
   if (!sd->rdir || !retro_readdir(sd->rdir))
      return false;
   if (     !(name = retro_dirent_get_name(sd->rdir))
         || retro_dirent_is_dir(sd->rdir, NULL)
         || !string_is_equal_noncase(path_get_extension(name), sd->ext))
      return true;
   fill_pathname_join_special(sd->path, sd->dir, name, sizeof(sd->path));
#ifndef _WIN32
   {
      /* What a symlink names lives outside the folder */
      struct stat st;
      if (lstat(sd->path, &st) != 0 || !S_ISREG(st.st_mode))
         return true;
   }
#endif
   memset(c, 0, sizeof(*c));
   strlcpy(c->rec.name, name, sizeof(c->rec.name));
   /* Not a regular file, or gone: core_file_open refuses it */
   if (!(sd->f = core_file_open(sd->path)))
      return true;
   if (     !core_file_identity(sd->f, &c->rec.id)
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
         sd->done = !sd->unopened;
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
   unsigned i;
   unsigned trusted           = 0;
   if (!core_trust_rt.t || sd->gen != core_trust_rt.gen)
      return;
   core_trust_rt.seed_pending = false;
   /* Cancelled at quit, or the folder unopened: seeded on a later run */
   if (!sd->done)
      return;
   before = core_trust_get(core_trust_rt.t);
   for (i = 0; i < sd->num_found; i++)
      if (core_trust_always(core_trust_rt.t, &sd->found[i]))
         trusted++;
   core_trust_set_flag(core_trust_rt.t, CORE_TRUST_SEEDED, true);
   if (     trusted
         && !(core_trust_runtime_flags() & CORE_TRUST_NOTICE_SHOWN))
   {
      core_trust_set_flag(core_trust_rt.t, CORE_TRUST_NOTICE_SHOWN, true);
      core_trust_rt.seed_notice = true;
   }
   core_trust_runtime_changed(before);
   core_trust_runtime_show();
}

static void core_trust_seed_cleanup(retro_task_t *task)
{
   struct core_trust_seed *sd = (struct core_trust_seed*)task->state;
   if (sd->f)
   {
      core_hash_free(&sd->h);
      core_file_close(sd->f);
   }
   if (sd->rdir)
      retro_closedir(sd->rdir);
   core_trust_set_put(sd->set);
   free(sd->found);
   free(sd);
   task->state = NULL;
}

static void core_trust_seed_start(const char *seed_dir,
      const char *core_ext)
{
   struct core_trust_seed *sd;
   retro_task_t *task = NULL;
   if (     core_trust_rt.read_only
         || string_is_empty(seed_dir)
         || string_is_empty(core_ext)
         || (core_trust_runtime_flags() & CORE_TRUST_SEEDED)
         || !(sd = (struct core_trust_seed*)calloc(1, sizeof(*sd))))
      return;
   if (     !(sd->found = (struct core_check*)malloc(
               CORE_TRUST_MAX_HASHES * sizeof(*sd->found)))
         || !(task = task_init()))
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
   core_trust_rt.seed_pending = true;
   task->state    = sd;
   task->handler  = core_trust_seed_handler;
   task->callback = core_trust_seed_cb;
   task->cleanup  = core_trust_seed_cleanup;
   task->flags   |= RETRO_TASK_FLG_MUTE;
   task_queue_push(task);
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

bool core_trust_runtime_init(const char *path, const char *seed_dir,
      const char *core_ext)
{
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
      core_trust_rt.read_only = core_trust_load_file(core_trust_rt.t,
            core_trust_rt.path) == CORE_TRUST_LOAD_UNREADABLE;
#ifdef HAVE_CORE_LOAD_CHECK
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
   /* At quit, as retroarch.cfg is: the queue is drained by now,
    * within the drain's limit */
   if (     core_trust_rt.unsaved
         && !core_trust_rt.read_only
         && (w = core_trust_write_begin(core_trust_rt.t)))
      core_trust_write_end(core_trust_rt.t, w,
            core_trust_write_run(w, core_trust_rt.path));
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
   core_trust_runtime_changed(before);
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
      core_trust_set_put(before);
      return false;
   }
   core_trust_runtime_changed(before);
   return true;
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
   core_trust_runtime_changed(before);
   return true;
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
