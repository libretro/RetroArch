/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <queues/task_queue.h>
#include <retro_miscellaneous.h>

#include "core_trust_runtime.h"
#include "runloop.h"

static struct
{
   core_trust_t *t;
   /* Tells a task finishing after deinit it belongs to an older store */
   unsigned gen;
   bool read_only;
   /* Changed since the last write that went through */
   bool unsaved;
   bool notify_ready;
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
}

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
   (void)seed_dir;
   (void)core_ext;
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
