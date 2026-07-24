#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <queues/task_queue.h>

#include "../../../core_info.h"
#include "../../../tasks/tasks_internal.h"

#define SCAN_TIMEOUT_SECONDS 120

#include <time.h>

#include <retro_timers.h>

#include "../../../manual_content_scan.h"
#include "../../../configuration.h"
#include "../../../verbosity.h"

static bool loop_active = true;

/* Stubs for symbols referenced by the retroarch-tree sources we pull
 * in.  The real definitions live in intl/msg_hash_us.c and
 * configuration.c, but those files transitively require RARCH_INTERNAL
 * which drags in the entire frontend subsystem.  This sample only
 * exercises task_push_dbscan; none of these symbols are actually
 * invoked on the path through task_push_dbscan / task_queue_check.
 *
 * These take enum msg_hash_enums now that configuration.h - included
 * for settings_t - brings msg_hash.h with it.  They used to be
 * declared with int, which matched at the link level but conflicts
 * once the real prototypes are visible. */
int msg_hash_get_help_us_enum(enum msg_hash_enums msg, char *s, size_t len)
{
   (void)msg;
   if (s && len)
      s[0] = '\0';
   return 0;
}

const char *msg_hash_to_str_us(enum msg_hash_enums msg)
{
   (void)msg;
   return "";
}

/* The string-table index builder (added by the msg_hash strtab refactor).
 * With msg_hash_to_str_us() stubbed above, the index is never consulted, so
 * this can be an empty stub rather than linking intl/msg_hash_us.c. */
void msg_hash_us_index_init(void)
{
}

settings_t *config_get_ptr(void)
{
   /* A real settings_t, not a zeroed blob.
    *
    * The scan reads settings->paths.directory_playlist before it does
    * anything else and refuses to start if it is empty - so a stub that
    * returned zeros meant no task was ever created, the completion
    * callback never fired, and this sample sat in its loop until the CI
    * runner killed it.  The playlist directory argument it accepts on
    * the command line went nowhere.
    *
    * Static so it is zero-initialised; main() fills in the one field
    * the scan actually consults. */
   static settings_t settings;
   return &settings;
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

static bool scan_completed = false;
static bool scan_errored   = false;

static void main_db_cb(retro_task_t *task,
      void *task_data, void *user_data, const char *err)
{
   (void)task;
   (void)task_data;
   (void)user_data;
   if (err && *err)
   {
      fprintf(stderr, "scan reported an error: %s\n", err);
      scan_errored = true;
   }
   scan_completed = true;
   loop_active    = false;
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
   /* The scanner traces its state through RARCH_LOG/RARCH_DBG; without
    * this the sample runs blind. */
   verbosity_enable();
   retro_main_log_file_init(NULL, false);

#ifdef HAVE_THREADS
   task_queue_init(true /* threaded enable */, main_msg_queue_push);
#else
   task_queue_init(false /* threaded enable */, main_msg_queue_push);
#endif
   core_info_init_list(core_info_dir, core_dir, exts, true, false, NULL);

   /* The scan reads its playlist directory from settings, not from the
    * argument below, so put it where the scan will look. */
   strlcpy(config_get_ptr()->paths.directory_playlist, playlist_dir,
         sizeof(config_get_ptr()->paths.directory_playlist));

   /* A scan needs a system name to build its playlist from.  Without
    * one manual_content_scan_get_task_config refuses and no task is
    * created - which is what this sample used to do, silently, before
    * spinning in the loop below until CI killed it. */
   if (!manual_content_scan_set_menu_system_name(
            MANUAL_CONTENT_SCAN_SYSTEM_NAME_CUSTOM, "SampleScan"))
   {
      fprintf(stderr, "could not set a system name for the scan\n");
      goto done;
   }

   /* The return value matters: false means no task exists, so nothing
    * will ever call main_db_cb and the loop below would never end. */
   if (!task_push_dbscan(playlist_dir, db_dir, input_dir, true,
            true, main_db_cb))
   {
      fprintf(stderr, "task_push_dbscan refused to start a scan\n");
      goto done;
   }

   /* Bounded, so a scanner that stalls is a reportable failure rather
    * than a job that hangs until the runner times out. */
   {
      time_t started = time(NULL);
      while (loop_active)
      {
         task_queue_check();
         if (difftime(time(NULL), started) > SCAN_TIMEOUT_SECONDS)
         {
            fprintf(stderr, "scan did not finish within %d seconds\n",
                  SCAN_TIMEOUT_SECONDS);
            break;
         }
         retro_sleep(1);
      }
   }

done:
   if (!scan_completed)
      fprintf(stderr, "FAILED: the scan never ran to completion\n");
   else if (scan_errored)
      fprintf(stderr, "FAILED: the scan completed with an error\n");
   else
      fprintf(stderr, "PASS: scan completed\n");

   core_info_deinit_list();
   task_queue_deinit();

   return (scan_completed && !scan_errored) ? 0 : 1;
}
