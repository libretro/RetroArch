/* Oracle for the playlist manager tasks' load phase
 * (tasks/task_playlist_manager.c), compiled from the shipping
 * translation unit against the real task queue and the real shared
 * per-frame I/O window.
 *
 * Both handlers were already per-entry state machines - one playlist
 * entry per invocation - with one exception: PL_MANAGER_BEGIN read
 * the whole playlist in a single blocking playlist_init().  For a
 * large collection that is tens of milliseconds of JSON in one
 * handler call, and with Threaded Tasks off that call runs on the
 * thread driving the frame loop, while the progress bar the user is
 * watching cannot repaint.  The load now runs through the resumable
 * parse under the shared window.
 *
 * What these lanes pin:
 *
 *   completeness - the task still sees the whole playlist: the
 *                  entry count it iterates over matches the file,
 *                  whatever the pacing does.
 *   pacing       - with a clock that exhausts the window quickly,
 *                  the load spreads over several invocations instead
 *                  of landing in one.
 *   progress     - a window that is already exhausted still makes
 *                  progress, so the load cannot stall.
 *   cancel       - cancelling while the playlist is still being read
 *                  releases the in-flight parse (LSan is the
 *                  assertion here).
 *
 * Time is a virtual clock advancing a fixed step per observation, so
 * the pacing assertions are exact rather than dependent on the speed
 * of the machine running them.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <features/features_cpu.h>
#include <queues/task_queue.h>
#include <string/stdstring.h>

#include "../../../playlist.h"
#include "../../../msg_hash.h"
#include "../../../retroarch.h"
#include "../../../menu/menu_driver.h"
#include "../../../tasks/tasks_internal.h"

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

/* ------------------------------------------------------------------ */
/* Virtual clock                                                       */
/* ------------------------------------------------------------------ */

static retro_time_t clock_now;
static retro_time_t clock_step;

retro_time_t cpu_features_get_time_usec(void)
{
   clock_now += clock_step;
   return clock_now;
}

uint64_t cpu_features_get(void) { return 0; }
unsigned cpu_features_get_core_amount(void) { return 1; }

/* NBIO_XFER_TICK_USEC is 4000; a third of it per observation
 * exhausts the window after a couple of budget checks. */
#define VIRTUAL_CLOCK_STEP 1500

/* ------------------------------------------------------------------ */
/* Stubs                                                               */
/* ------------------------------------------------------------------ */

bool core_info_find(const char *core_path, core_info_t **core_info)
{
   if (core_info)
      *core_info = NULL;
   return false;
}

bool core_info_core_file_id_is_equal(const char *a, const char *b)
{
   return false;
}

bool play_feature_delivery_enabled(void)
{
   return false;
}

void frontend_driver_attach_console(void) { }
void frontend_driver_detach_console(void) { }

const char *msg_hash_to_str(enum msg_hash_enums msg)
{
   return "";
}

void runloop_msg_queue_push(const char *msg, size_t len,
      unsigned prio, unsigned duration, bool flush,
      char *title, enum message_queue_icon icon,
      enum message_queue_category category)
{
}

void ui_companion_driver_notify_refresh(void) { }

bool menu_driver_ctl(enum rarch_menu_ctl_state state, void *data)
{
   return false;
}

/* ------------------------------------------------------------------ */
/* Fixture                                                             */
/* ------------------------------------------------------------------ */

static char fixture_dir[256];

static bool write_playlist(const char *path, unsigned entries)
{
   unsigned i;
   FILE *f = fopen(path, "wb");
   if (!f)
      return false;

   fprintf(f, "{\n  \"version\": \"1.5\",\n  \"items\": [\n");
   for (i = 0; i < entries; i++)
      fprintf(f,
            "    { \"path\": \"%s/game%05u.bin\", \"label\": \"Game %05u\","
            " \"core_path\": \"DETECT\", \"core_name\": \"DETECT\","
            " \"crc32\": \"00000000|crc\", \"db_name\": \"Test.lpl\" }%s\n",
            fixture_dir, i, i, (i + 1 < entries) ? "," : "");
   fprintf(f, "  ]\n}\n");
   fclose(f);
   return true;
}

/* The content files the validate pass checks for; without them the
 * task deletes entries, which is a different behaviour to measure. */
static bool write_content_files(unsigned entries)
{
   unsigned i;
   for (i = 0; i < entries; i++)
   {
      char path[512];
      FILE *f;
      snprintf(path, sizeof(path), "%s/game%05u.bin", fixture_dir, i);
      if (!(f = fopen(path, "wb")))
         return false;
      fputc(0, f);
      fclose(f);
   }
   return true;
}

/* ------------------------------------------------------------------ */
/* Driving the task                                                    */
/* ------------------------------------------------------------------ */

static bool any_finder(retro_task_t *task, void *userdata)
{
   return true;
}

static bool queue_busy(void)
{
   task_finder_data_t find_data;
   find_data.func     = any_finder;
   find_data.userdata = NULL;
   return task_queue_find(&find_data);
}

static void config_for(playlist_config_t *config, const char *path)
{
   memset(config, 0, sizeof(*config));
   config->capacity            = 8192;
   config->old_format          = false;
   config->compress            = false;
   config->fuzzy_archive_match = false;
   config->autofix_paths       = false;
   playlist_config_set_path(config, path);
}

/* Runs a clean-playlist task to completion, returning the number of
 * handler invocations it took. */
static unsigned run_clean(unsigned entries, retro_time_t step,
      bool cancel_early)
{
   char path[512];
   playlist_config_t config;
   unsigned ticks = 0;

   snprintf(path, sizeof(path), "%s/test.lpl", fixture_dir);
   if (!write_playlist(path, entries))
   {
      CHECK(false, "fixture write failed");
      return 0;
    }

   task_queue_init(false, NULL);

   clock_now  = 0;
   clock_step = step;

   config_for(&config, path);

   if (!task_push_pl_manager_clean_playlist(&config))
   {
      CHECK(false, "task push failed");
      task_queue_deinit();
      return 0;
   }

   while (queue_busy() && ticks < 100000)
   {
      task_queue_check();
      ticks++;

      /* Abandon the task while the playlist is still being read.
       *
       * task_queue_reset() only FLAGS the task cancelled; the queue
       * still has to retire it for its cleanup to run, and the
       * unthreaded deinit does not drain.  Simply stopping here
       * would leave the task - and everything it owns - reachable
       * from the queue's own list, where LeakSanitizer cannot see a
       * leak at all.  So keep pumping until the queue is empty and
       * the cleanup has actually run. */
      if (cancel_early && ticks == 2)
      {
         task_queue_reset();
         cancel_early = false;
      }
   }

   task_queue_deinit();
   return ticks;
}

/* ------------------------------------------------------------------ */
/* Lanes                                                               */
/* ------------------------------------------------------------------ */

/* The playlist the task rewrites is the observable: after a clean
 * run over content that all exists, every entry must survive. */
static unsigned surviving_entries(void)
{
   char path[512];
   playlist_config_t config;
   playlist_t *pl;
   unsigned n;

   snprintf(path, sizeof(path), "%s/test.lpl", fixture_dir);
   config_for(&config, path);
   if (!(pl = playlist_init(&config)))
      return 0;
   n = (unsigned)playlist_size(pl);
   playlist_free(pl);
   return n;
}

static void lane_completeness(void)
{
   unsigned had = failures;

   /* Step 0: the window never expires, so the load runs unpaced. */
   run_clean(64, 0, false);
   CHECK(surviving_entries() == 64,
         "unpaced run left %u entries, wanted 64",
         surviving_entries());

   if (failures == had)
      fprintf(stderr, "[pass] completeness lane\n");
}

static void lane_pacing(void)
{
   unsigned had = failures;
   unsigned unpaced, paced;

   /* A playlist large enough that reading it is a measurable share
    * of the task.  The per-entry iteration that follows costs the
    * same number of invocations either way - it was already one
    * entry per invocation - so the difference between these two
    * numbers is the load phase, which is the thing under test.
    *
    * The entries deliberately point at content that does not exist,
    * so both runs delete them identically; this lane measures how
    * the load is spread, not what the clean pass decides. */
   unpaced = run_clean(3000, 0, false);
   paced   = run_clean(3000, VIRTUAL_CLOCK_STEP, false);

   CHECK(paced > unpaced,
         "pacing did not spread the load: %u ticks paced vs %u "
         "unpaced", paced, unpaced);

   if (failures == had)
      fprintf(stderr, "[pass] pacing lane (%u paced vs %u unpaced)\n",
            paced, unpaced);
}

static void lane_exhausted_window_progresses(void)
{
   unsigned had = failures;
   unsigned ticks;

   /* A step larger than the whole window: every budget check fails
    * immediately, so only the guaranteed work per invocation is
    * done.  The task must still finish. */
   ticks = run_clean(32, 100000, false);

   CHECK(ticks > 0 && ticks < 100000, "task did not terminate");
   CHECK(surviving_entries() == 32,
         "exhausted-window run left %u entries, wanted 32",
         surviving_entries());

   if (failures == had)
      fprintf(stderr, "[pass] exhausted-window lane (%u ticks)\n",
            ticks);
}

static void lane_cancel_during_parse(void)
{
   unsigned had = failures;

   /* Cancelled two invocations in, with the playlist still being
    * read: the in-flight parse and everything it has built must be
    * released.  LSan is the assertion. */
   run_clean(4096, VIRTUAL_CLOCK_STEP, true);

   if (failures == had)
      fprintf(stderr, "[pass] cancel-during-parse lane\n");
}

int main(void)
{
   char cmd[600];

   snprintf(fixture_dir, sizeof(fixture_dir),
         "/tmp/plmanager_fixture_%ld", (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", fixture_dir);
   if (system(cmd) != 0)
   {
      fprintf(stderr, "fixture mkdir failed\n");
      return 1;
   }
   if (!write_content_files(64))
   {
      fprintf(stderr, "content fixture write failed\n");
      return 1;
   }

   lane_completeness();
   lane_pacing();
   lane_exhausted_window_progresses();
   lane_cancel_during_parse();

   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { }

   if (failures)
   {
      fprintf(stderr, "FAIL pl_manager_budget_test: %u failures\n",
            failures);
      return 1;
   }
   fprintf(stderr, "PASS pl_manager_budget_test\n");
   return 0;
}
