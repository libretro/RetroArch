/* The Steam core DLC install task, on the threaded task queue.
 *
 * The suite includes tasks/task_steam.c, built against a stand-in for
 * mist's header, and plays Steam: a download that is in progress for a
 * while and then complete.  Steam gives the task nothing to wake it
 * with, so it asks for the download's progress at a progress bar's
 * rate, parked in between: a handful of asks over the download, not
 * one per worker spin.  A cancelled install reports nothing. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_timers.h>
#include <features/features_cpu.h>

#include "../../../tasks/task_steam.c"

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
         fprintf(stderr, __VA_ARGS__); \
         fprintf(stderr, "\n"); \
         failures++; \
      } \
   } while (0)

/* ---- Steam ---- */

static retro_atomic_int_t asks;
static retro_atomic_int_t complete;

MistResult mist_steam_apps_get_dlc_download_progress(AppId app_id,
      bool *downloading, uint64_t *bytes_downloaded, uint64_t *bytes_total)
{
   (void)app_id;
   retro_atomic_fetch_add_int(&asks, 1);
   *downloading      = !retro_atomic_load_acquire_int(&complete);
   *bytes_downloaded = *downloading ? 50 : 100;
   *bytes_total      = 100;
   return MistResult_Success;
}

/* ---- the rest of the frontend, stubbed ---- */

const char *msg_hash_to_str(enum msg_hash_enums msg) { (void)msg; return ""; }

static retro_atomic_int_t installed_msgs;
void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration, bool flush, char *title,
      enum message_queue_icon icon, enum message_queue_category category)
{
   (void)msg; (void)len; (void)prio; (void)duration; (void)flush;
   (void)title; (void)icon; (void)category;
   retro_atomic_fetch_add_int(&installed_msgs, 1);
}

/* ---- the lane ---- */

static bool any_task(retro_task_t *task, void *user_data)
{
   (void)task;
   (void)user_data;
   return true;
}

static bool find_install(retro_task_t *task, void *user_data)
{
   if (task->handler != task_steam_core_dlc_install_handler)
      return false;
   *(retro_task_t**)user_data = task;
   return true;
}

static void lane_progress_polled_parked(void)
{
   task_finder_data_t find_data;
   unsigned           had = failures;
   retro_time_t       start;
   int                n;

   retro_atomic_store_release_int(&asks, 0);
   retro_atomic_store_release_int(&complete, 0);
   task_push_steam_core_dlc_install(1234, "Lane Core");

   start = cpu_features_get_time_usec();
   while (cpu_features_get_time_usec() - start < 600000)
   {
      task_queue_check();
      retro_sleep(1);
   }
   n = retro_atomic_load_acquire_int(&asks);
   /* 600 ms at one ask per 250 ms park: three or four. */
   CHECK(n >= 2 && n <= 6, "%d progress asks in 600 ms", n);

   retro_atomic_store_release_int(&complete, 1);
   find_data.func     = any_task;
   find_data.userdata = NULL;
   start = cpu_features_get_time_usec();
   while (task_queue_find(&find_data)
         && cpu_features_get_time_usec() - start < 2000000)
   {
      task_queue_check();
      retro_sleep(1);
   }
   CHECK(!task_queue_find(&find_data), "the task did not finish");
   CHECK(retro_atomic_load_acquire_int(&installed_msgs) == 1,
         "the install was reported %d times",
         retro_atomic_load_acquire_int(&installed_msgs));

   if (failures == had)
      fprintf(stderr, "[pass] progress asked at a progress bar's rate: %d asks\n", n);
}

/* A cancelled install ends without reporting the core installed. */
static void lane_cancel_reports_nothing(void)
{
   task_finder_data_t find_data;
   unsigned           had = failures;
   retro_task_t      *task = NULL;
   retro_time_t       start;

   retro_atomic_store_release_int(&complete, 0);
   retro_atomic_store_release_int(&installed_msgs, 0);
   task_push_steam_core_dlc_install(1234, "Lane Core");

   find_data.func     = find_install;
   find_data.userdata = &task;
   task_queue_find(&find_data);
   CHECK(task != NULL, "the install task was not found");
   if (task)
      task_queue_cancel_task(task);

   find_data.func     = any_task;
   find_data.userdata = NULL;
   start = cpu_features_get_time_usec();
   while (task_queue_find(&find_data)
         && cpu_features_get_time_usec() - start < 2000000)
   {
      task_queue_check();
      retro_sleep(1);
   }
   CHECK(!task_queue_find(&find_data), "the cancelled task did not finish");
   CHECK(retro_atomic_load_acquire_int(&installed_msgs) == 0,
         "a cancelled install was reported installed");

   if (failures == had)
      fprintf(stderr, "[pass] a cancelled install reports nothing\n");
}

int main(void)
{
   task_queue_init(true, NULL);
   lane_progress_polled_parked();
   lane_cancel_reports_nothing();
   task_queue_deinit();

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   fprintf(stderr, "all lanes passed\n");
   return 0;
}
