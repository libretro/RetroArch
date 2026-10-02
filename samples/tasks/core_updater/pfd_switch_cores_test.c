/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (pfd_switch_cores_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Regression test for the Play Feature Delivery 'switch installed
 * cores' task (tasks/task_core_updater.c), against the shipping
 * switch and install tasks, list parser and task queue.  The tasks
 * are Android-only; task_core_updater.c is built here with ANDROID
 * defined, and the five play_feature_delivery_* calls they make are
 * the stand-ins below.
 *
 * The switch task waits on each install task it pushes.  The queue
 * frees a finished task in the same gather pass that retires it, so
 * the wait must not read the install task: completion and the
 * install's error come through a record shared with the install's
 * callback.  A wait that polls the task reads freed memory - a
 * heap-use-after-free under ASan, in both queue modes.
 *
 * Lanes, each threaded and unthreaded:
 *   - every installed core is switched: one install per core;
 *   - a failing install ends the switch early, with its error;
 *   - the switch task cancelled while an install runs: the install's
 *     callback fires after the switch's handle is gone (LeakSan
 *     covers a record nobody frees). */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <boolean.h>
#include <retro_miscellaneous.h>
#include <queues/task_queue.h>
#include <lists/string_list.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#include <retro_timers.h>
#include <file/file_path.h>
#include <compat/strl.h>

#include "../../../configuration.h"
#include "../../../retroarch.h"
#include "../../../play_feature_delivery/play_feature_delivery.h"
#include "../../../tasks/tasks_internal.h"

#define NUM_CORES 8

/* Declared in tasks_internal.h under ANDROID, which only
 * task_core_updater.c is built with here */
void task_push_play_feature_delivery_switch_installed_cores(
      const char *path_dir_libretro,
      const char *path_libretro_info);

static int checks   = 0;
static int failures = 0;

#define CHECK(cond, name) \
   do \
   { \
      checks++; \
      if (cond) \
         printf("  ok   %s\n", name); \
      else \
      { \
         failures++; \
         printf("  FAIL %s\n", name); \
      } \
   } while (0)

/* ---------------- play feature delivery stand-ins ---------------- */

/* Each install 'downloads' at once and reports INSTALLED, unless
 * pfd_fail is set (FAILED) or pfd_hold is (DOWNLOADING, active). */
static retro_atomic_int_t pfd_downloads;
static retro_atomic_int_t pfd_fail;
static retro_atomic_int_t pfd_hold;

bool play_feature_delivery_enabled(void) { return true; }

struct string_list *play_feature_delivery_available_cores(void)
{
   union string_list_elem_attr attr;
   struct string_list *list = string_list_new();
   int i;
   attr.i = 0;
   if (!list)
      return NULL;
   for (i = 0; i < NUM_CORES; i++)
   {
      char name[64];
      snprintf(name, sizeof(name), "pfd%02d_libretro_android.so", i);
      string_list_append(list, name, attr);
   }
   return list;
}

bool play_feature_delivery_core_installed(const char *core_file)
{
   (void)core_file;
   return false;
}

bool play_feature_delivery_download(const char *core_file)
{
   (void)core_file;
   retro_atomic_fetch_add_int(&pfd_downloads, 1);
   return true;
}

bool play_feature_delivery_download_status(
      enum play_feature_delivery_install_status *status,
      unsigned *progress)
{
   if (retro_atomic_load_acquire_int(&pfd_hold))
   {
      *status   = PLAY_FEATURE_DELIVERY_DOWNLOADING;
      *progress = 50;
      return true;
   }
   *status   = retro_atomic_load_acquire_int(&pfd_fail)
         ? PLAY_FEATURE_DELIVERY_FAILED
         : PLAY_FEATURE_DELIVERY_INSTALLED;
   *progress = 100;
   return false;
}

/* The install push asks whether the core it replaces is running */
bool retroarch_ctl(enum rarch_ctl_state state, void *data)
{
   (void)state;
   (void)data;
   return false;
}

/* ---------------- helpers ---------------------------------------- */

static char g_dir[] = "/tmp/pfd_switch_cores_XXXXXX";

static bool make_cores(void)
{
   int i;
   for (i = 0; i < NUM_CORES; i++)
   {
      char name[64];
      char path[PATH_MAX_LENGTH];
      FILE *f;
      snprintf(name, sizeof(name), "pfd%02d_libretro_android.so", i);
      fill_pathname_join_special(path, g_dir, name, sizeof(path));
      if (!(f = fopen(path, "wb")))
         return false;
      fputs("installed outside play feature delivery", f);
      fclose(f);
   }
   return true;
}

static bool any_task(retro_task_t *task, void *user_data)
{
   (void)task;
   (void)user_data;
   return true;
}

static bool is_task(retro_task_t *task, void *user_data)
{
   return task == (retro_task_t*)user_data;
}

/* The switch task is the one task here pushed unmuted */
static bool find_switch(retro_task_t *task, void *user_data)
{
   if (task_get_flags(task) & RETRO_TASK_FLG_MUTE)
      return false;
   *(retro_task_t**)user_data = task;
   return true;
}

static bool pump_until_idle(int max_ms)
{
   task_finder_data_t find_data;
   int i;
   find_data.func     = any_task;
   find_data.userdata = NULL;
   for (i = 0; i < max_ms; i++)
   {
      task_queue_check();
      if (!task_queue_find(&find_data))
         return true;
      retro_sleep(1);
   }
   return false;
}

static void lane_begin(bool threaded, const char *name)
{
   printf("[lane threaded=%d: %s]\n", (int)threaded, name);
   retro_atomic_store_release_int(&pfd_downloads, 0);
   retro_atomic_store_release_int(&pfd_fail, 0);
   retro_atomic_store_release_int(&pfd_hold, 0);
   task_queue_init(threaded, NULL);
}

static void lane_end(void)
{
   task_queue_deinit();
   task_queue_unset_threaded();
}

/* ---------------- lanes ------------------------------------------ */

static void lane_switch_all(bool threaded)
{
   lane_begin(threaded, "every installed core is switched");

   if (!make_cores())
   {
      printf("  SKIP: could not create fixture\n");
      lane_end();
      return;
   }

   task_push_play_feature_delivery_switch_installed_cores(g_dir, g_dir);
   CHECK(pump_until_idle(10000), "switch task completed");
   CHECK(retro_atomic_load_acquire_int(&pfd_downloads) == NUM_CORES,
         "one install per installed core");

   lane_end();
}

static void lane_install_error(bool threaded)
{
   lane_begin(threaded, "a failing install ends the switch");

   if (!make_cores())
   {
      printf("  SKIP: could not create fixture\n");
      lane_end();
      return;
   }

   retro_atomic_store_release_int(&pfd_fail, 1);
   task_push_play_feature_delivery_switch_installed_cores(g_dir, g_dir);
   CHECK(pump_until_idle(10000), "switch task completed");
   CHECK(retro_atomic_load_acquire_int(&pfd_downloads) == 1,
         "switch stopped after the failed install");

   lane_end();
}

static void lane_cancel_during_install(bool threaded)
{
   task_finder_data_t find_data;
   retro_task_t *switch_task = NULL;
   int i;

   lane_begin(threaded, "switch cancelled while an install runs");

   if (!make_cores())
   {
      printf("  SKIP: could not create fixture\n");
      lane_end();
      return;
   }

   retro_atomic_store_release_int(&pfd_hold, 1);
   task_push_play_feature_delivery_switch_installed_cores(g_dir, g_dir);

   for (i = 0; i < 5000
         && !retro_atomic_load_acquire_int(&pfd_downloads); i++)
   {
      task_queue_check();
      retro_sleep(1);
   }
   CHECK(i < 5000, "install started");

   find_data.func     = find_switch;
   find_data.userdata = &switch_task;
   task_queue_find(&find_data);
   CHECK(switch_task != NULL, "switch task found");
   if (switch_task)
      task_queue_cancel_task(switch_task);

   /* Retire the switch task with its install still running */
   find_data.func     = is_task;
   find_data.userdata = switch_task;
   for (i = 0; i < 5000; i++)
   {
      task_queue_check();
      if (!switch_task || !task_queue_find(&find_data))
         break;
      retro_sleep(1);
   }
   CHECK(i < 5000, "cancelled switch task retired first");

   find_data.func     = any_task;
   find_data.userdata = NULL;
   CHECK(task_queue_find(&find_data),
         "install still pending after the switch retired");

   /* Now let the install finish; its callback runs after the switch
    * task's handle is gone */
   retro_atomic_store_release_int(&pfd_hold, 0);
   CHECK(pump_until_idle(5000), "install completed after the switch");

   lane_end();
}

int main(void)
{
   char cmd[128];
   int threaded;

   setvbuf(stdout, NULL, _IOLBF, 0);
   printf("play feature delivery switch-cores test\n\n");

   if (!mkdtemp(g_dir))
   {
      printf("SKIP: could not create temp dir\n");
      return 0;
   }

   for (threaded = 0; threaded <= 1; threaded++)
   {
      lane_switch_all(threaded != 0);
      lane_install_error(threaded != 0);
      lane_cancel_during_install(threaded != 0);
   }

   snprintf(cmd, sizeof(cmd), "rm -rf %s", g_dir);
   if (system(cmd)) { /* best effort */ }

   printf("\n%s (%d check%s, %d failure%s)\n",
         failures ? "FAILED" : "PASSED",
         checks,   checks   == 1 ? "" : "s",
         failures, failures == 1 ? "" : "s");
   return failures ? 1 : 0;
}
