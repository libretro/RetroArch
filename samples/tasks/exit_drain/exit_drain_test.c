/* In-flight tasks retire before the exit path tears anything down.
 *
 * Links the shipping RetroArch objects with only main() replaced and
 * drives retroarch_drain_tasks_for_exit() - the first thing
 * main_exit() now runs - against the task queue's own machinery:
 * task_queue_deinit() abandons whatever is still queued (the worker
 * breaks out mid-queue; nothing runs remaining handlers, finish
 * callbacks, or cleanup callbacks), and the exit path used to reach
 * it only after the drivers were gone.
 *
 * Lanes:
 *  - a cancellation-aware task (its handler winds down when it sees
 *    RETRO_TASK_FLG_CANCELLED) must have its finish and cleanup
 *    callbacks run by the drain;
 *  - a task that completes on its own before the drain must retire
 *    through the same gather;
 *  - a scheduled-for-later task (a 'when' in the future) must not
 *    hold the wait: the drain returns promptly and that task meets
 *    the old abandonment, as designed.
 *
 * The old-order lane (MODE=old) skips the drain and goes straight
 * to task_queue_deinit() the way the exit path used to: the
 * cancellation-aware task's callbacks never run, which is the
 * abandonment this change demotes from rule to fallback.
 *
 * Requires a completed non-Qt build:
 *
 *   ./configure --disable-qt && make
 *   samples/tasks/exit_drain/build.sh
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <time/rtime.h>
#include <file/config_file.h>
#include <streams/file_stream.h>
#include <queues/task_queue.h>
#include <features/features_cpu.h>
#include <retro_timers.h>

#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../frontend/frontend_driver.h"

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

/* Witnesses, one per lane. */
static volatile int aware_finished   = 0;
static volatile int aware_callback   = 0;
static volatile int aware_cleanup    = 0;
static volatile int quick_callback   = 0;
static volatile int deferred_touched = 0;

/* Cancellation-aware: polls its flag between chunks, like a
 * transfer. Capped so the old-order lane - which never cancels -
 * models a long task rather than an unbounded one: deinit joins
 * the worker mid-handler, and an uncappable spin would turn the
 * abandonment being demonstrated into a hang of the harness. */
static void aware_handler(retro_task_t *task)
{
   int spins = 0;
   while (   !(task_get_flags(task) & RETRO_TASK_FLG_CANCELLED)
          && spins++ < 8000)
      retro_sleep(1);
   aware_finished = 1;
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void aware_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   (void)task; (void)task_data; (void)user_data; (void)error;
   aware_callback = 1;
}

static void aware_cleanup_cb(retro_task_t *task)
{
   (void)task;
   aware_cleanup = 1;
}

/* Completes on its own, immediately. */
static void quick_handler(retro_task_t *task)
{
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void quick_cb(retro_task_t *task, void *task_data,
      void *user_data, const char *error)
{
   (void)task; (void)task_data; (void)user_data; (void)error;
   quick_callback = 1;
}

/* Scheduled for later: must not hold the drain. */
static void deferred_handler(retro_task_t *task)
{
   deferred_touched = 1;
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static retro_task_t *make_task(retro_task_handler_t handler,
      retro_task_callback_t cb, retro_task_handler_t cleanup)
{
   retro_task_t *t = task_init();
   if (!t)
      return NULL;
   t->handler  = handler;
   t->callback = cb;
   t->cleanup  = cleanup;
   return t;
}

int main(int argc, char *argv[])
{
   char fixture_dir[512];
   char cmd[700];
   static char cfg_path[640];
   char *rarch_argv[8];
   int rarch_argc  = 0;
   bool old_order  = false;
   retro_task_t *t;
   retro_time_t t0;
   retro_time_t drain_us = 0;

   (void)argc;
   (void)argv;

   {
      const char *mode = getenv("MODE");
      old_order = (mode && strcmp(mode, "old") == 0);
   }

   snprintf(fixture_dir, sizeof(fixture_dir),
         "/tmp/exit_drain_%ld", (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", fixture_dir);
   if (system(cmd) != 0)
      return 1;

   rarch_argv[rarch_argc++] = (char*)"retroarch";
   rarch_argv[rarch_argc++] = (char*)"--menu";
   rarch_argv[rarch_argc++] = (char*)"--config";
   rarch_argv[rarch_argc++] = cfg_path;

   config_file_set_io_default(config_file_io_filestream());
   rtime_init();
   retroarch_config_init();
   retroarch_ctl(RARCH_CTL_STATE_FREE, NULL);
   frontend_driver_init_first(NULL);
   {
      FILE *cfg;
      snprintf(cfg_path, sizeof(cfg_path), "%s/harness.cfg",
            fixture_dir);
      if ((cfg = fopen(cfg_path, "wb")))
      {
         fprintf(cfg, "video_driver = \"null\"\n");
         fprintf(cfg, "audio_driver = \"null\"\n");
         fprintf(cfg, "input_driver = \"null\"\n");
         fprintf(cfg, "input_joypad_driver = \"null\"\n");
         fprintf(cfg, "menu_driver = \"rgui\"\n");
         fprintf(cfg, "threaded_data_runloop_enable = \"true\"\n");
         fclose(cfg);
      }
   }

   if (!retroarch_main_init(rarch_argc, rarch_argv))
   {
      fprintf(stderr, "FAIL: retroarch_main_init failed\n");
      return 1;
   }

   /* The in-flight population at "quit": one cancellation-aware,
    * one already finishing, one scheduled for later. */
   t = make_task(aware_handler, aware_cb, aware_cleanup_cb);
   CHECK(t != NULL, "fixture: task_init failed");
   if (t) task_queue_push(t);

   t = make_task(quick_handler, quick_cb, NULL);
   if (t) task_queue_push(t);

   t = make_task(deferred_handler, NULL, NULL);
   if (t)
   {
      t->when = cpu_features_get_time_usec() + 30 * 1000 * 1000;
      task_queue_push(t);
   }

   /* Let the worker pick the aware task up so cancellation lands on
    * a running handler, the harder case. */
   retro_sleep(50);

   if (!old_order)
   {
      t0 = cpu_features_get_time_usec();
      retroarch_drain_tasks_for_exit();
      drain_us = cpu_features_get_time_usec() - t0;

      CHECK(aware_finished == 1,
            "the cancellation-aware handler never wound down");
      CHECK(aware_callback == 1,
            "the aware task's finish callback did not run");
      CHECK(aware_cleanup == 1,
            "the aware task's cleanup callback did not run");
      CHECK(quick_callback == 1,
            "the completing task's callback did not run");
      CHECK(deferred_touched == 0,
            "the scheduled-for-later task ran during the drain");
      CHECK(drain_us < 2 * 1000 * 1000,
            "a deferred 'when' held the drain (%ld us)",
            (long)drain_us);
   }

   /* The rest of the old exit order; on both paths the deferred
    * task meets abandonment here, and on the old path everything
    * does. */
   task_queue_deinit();

   if (old_order)
   {
      CHECK(aware_callback == 0 && aware_cleanup == 0,
            "old order: expected abandonment, but callbacks ran");
      printf("exit_drain: old order abandons, as documented\n");
   }
   else
      printf("exit_drain: all lanes passed (drain %ld us)\n",
            (long)drain_us);

   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { /* fixture dir left behind; harmless */ }

   if (failures)
   {
      fprintf(stderr, "FAILURES (%u)\n", failures);
      return 1;
   }
   return 0;
}
