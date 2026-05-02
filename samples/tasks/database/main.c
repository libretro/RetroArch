#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <queues/task_queue.h>

#include "../../../core_info.h"
#include "../../../tasks/tasks_internal.h"

static bool loop_active = true;

/* Stubs for symbols referenced by the retroarch-tree sources we pull
 * in.  The real definitions live in intl/msg_hash_us.c and
 * configuration.c, but those files transitively require RARCH_INTERNAL
 * which drags in the entire frontend subsystem.  This sample only
 * exercises task_push_dbscan; none of these symbols are actually
 * invoked on the path through task_push_dbscan / task_queue_check.
 *
 * The `msg` parameter is declared as int rather than
 * `enum msg_hash_enums` because that enum lives in msg_hash.h, which
 * is not on the include path for this sample.  At the link level the
 * signatures match since enum values promote to int. */
int msg_hash_get_help_us_enum(int msg, char *s, size_t len)
{
   (void)msg;
   if (s && len)
      s[0] = '\0';
   return 0;
}

const char *msg_hash_to_str_us(int msg)
{
   (void)msg;
   return "";
}

void *config_get_ptr(void)
{
   /* core_info_current_supports_savestate_level dereferences the
    * return as a settings_t*.  That code path is never exercised
    * during a dbscan, but returning NULL would SEGV if it ever were.
    * A single static zeroed buffer is enough to keep dereferences
    * from crashing for any read -- the sample doesn't write to it. */
   static long long zeros[1024];  /* ~8 KiB, covers settings_t */
   return zeros;
}

/* Additional stubs for retroarch-core symbols referenced transitively.
 * None of these are exercised on the dbscan path; they're link-time
 * stubs to avoid pulling in retroarch.c, runloop.c, frontend drivers,
 * and the UI/video subsystems. */
void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration,
      bool flush, char *title, unsigned icon, unsigned category)
{
   (void)msg; (void)len; (void)prio; (void)duration;
   (void)flush; (void)title; (void)icon; (void)category;
}

bool retroarch_override_setting_is_set(unsigned enum_idx, void *data)
{
   (void)enum_idx; (void)data;
   return false;
}

void ui_companion_driver_notify_refresh(void)
{
}

void video_display_server_set_window_progress(int progress, bool finished)
{
   (void)progress; (void)finished;
}

/* task_database.c's progress_cb is now the shared task_window_progress_cb,
 * whose definition lives in tasks/task_file_transfer.c.  Pulling that
 * file in would drag in nbio, the audio mixer, and the image-task
 * machinery, none of which the dbscan path exercises.  Stub it here
 * to satisfy the linker; the function is never called on this code
 * path because no progress_cb is invoked unless a worker thread
 * publishes progress, and this sample never reaches that state. */
void task_window_progress_cb(retro_task_t *task)
{
   (void)task;
}

uint64_t frontend_driver_get_free_memory(void)
{
   return 0;
}

/* dir_list_new_special lives in retroarch.c, which we cannot link
 * without dragging in the world.  manual_content_scan calls this to
 * walk the scan directory; returning NULL causes the scan to bail
 * without producing results, which is fine for a standalone demo. */
void *dir_list_new_special(const char *input_dir, unsigned type,
      const char *filter, bool show_hidden_files)
{
   (void)input_dir; (void)type; (void)filter; (void)show_hidden_files;
   return NULL;
}

static void main_msg_queue_push(retro_task_t *task,
      const char *msg,
      unsigned prio, unsigned duration,
      bool flush)
{
   (void)task;
   fprintf(stderr, "MSGQ: %s\n", msg);
}

/*
 * return codes -
 * graceful exit: 1
 * normal   exit: 0
 * error    exit: -1
 */

static void main_db_cb(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   (void)task;
   fprintf(stderr, "DB CB: %s\n", err);
   loop_active = false;
}

int main(int argc, char *argv[])
{
   const char *db_dir        = NULL;
   const char *core_info_dir = NULL;
   const char *core_dir      = NULL;
   const char *input_dir     = NULL;
   const char *playlist_dir  = NULL;
#if defined(_WIN32)
   const char *exts          = "dll";
#elif defined(__MACH__)
   const char *exts          = "dylib";
#else
   const char *exts          = "so";
#endif

   if (argc < 6)
   {
      fprintf(stderr, "Usage: %s <database dir> <core dir> <core info dir> <input dir> <playlist dir>\n", argv[0]);
      return 1;
   }

   db_dir        = argv[1];
   core_dir      = argv[2];
   core_info_dir = argv[3];
   input_dir     = argv[4];
   playlist_dir  = argv[5];

   fprintf(stderr, "RDB database dir: %s\n", db_dir);
   fprintf(stderr, "Core         dir: %s\n", core_dir);
   fprintf(stderr, "Core info    dir: %s\n", core_info_dir);
   fprintf(stderr, "Input        dir: %s\n", input_dir);
   fprintf(stderr, "Playlist     dir: %s\n", playlist_dir);
#ifdef HAVE_THREADS
   task_queue_init(true /* threaded enable */, main_msg_queue_push);
#else
   task_queue_init(false /* threaded enable */, main_msg_queue_push);
#endif
   core_info_init_list(core_info_dir, core_dir, exts, true, false, NULL);

   task_push_dbscan(playlist_dir, db_dir, input_dir, true,
         true, main_db_cb);

   while (loop_active)
      task_queue_check();

   fprintf(stderr, "Exit loop\n");

   core_info_deinit_list();
   task_queue_deinit();

   return 0;
}
