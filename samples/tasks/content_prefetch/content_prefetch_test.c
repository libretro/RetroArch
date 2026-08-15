/* Oracle for the content prefetch task (tasks/task_content_prefetch.c),
 * compiled from the shipping translation unit against the real task
 * queue and the real data_transfer spine.
 *
 * This is the path a menu content load defers to: the bytes stream in
 * a budgeted slice per task tick, while the frontend keeps running,
 * and the load then takes the finished buffer instead of performing a
 * blocking read.  Delivery is by ownership transfer, from the task's
 * completion callback, which makes correctness here a memory question
 * as much as a behavioural one - hence the sanitizer sweeps.
 *
 * What these lanes pin:
 *
 *   contents   - the deposited buffer is byte-identical to the file,
 *                for a file large enough to need several ticks.
 *   budgeted   - it genuinely takes several ticks: the handler moves
 *                a bounded number of bytes per invocation rather than
 *                draining the file in one call.
 *   progress   - the reported percentage starts low, never goes
 *                backwards, and finishes at 100.
 *   multi      - several files all arrive, each with its own bytes,
 *                and progress still ends at 100 across the set.
 *   skip       - a path that cannot be read is skipped rather than
 *                failing the run, and reports not-all-ok; the load's
 *                ordinary read path is the authority for it.
 *   cancel     - abandoning the task mid-stream deposits nothing that
 *                was not already complete and leaks nothing.
 *
 * Ownership: the deposit callback takes the buffer, so the test frees
 * what it is handed.  Anything the task still holds is freed by its
 * own cleanup - which the cancel lane exercises under LeakSanitizer.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <boolean.h>
#include <queues/task_queue.h>
#include <retro_timers.h>

#include "../../../tasks/task_content_prefetch.h"

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

static char fixture_dir[256];

/* Deterministic content, so a byte comparison means something. */
static uint8_t byte_at(size_t i)
{
   return (uint8_t)((i * 31u + (i >> 8) * 7u) & 0xff);
}

static bool write_fixture(const char *path, size_t size)
{
   size_t i;
   FILE *f = fopen(path, "wb");
   if (!f)
      return false;
   for (i = 0; i < size; i++)
      fputc(byte_at(i), f);
   fclose(f);
   return true;
}

/* ------------------------------------------------------------------ */
/* Deposit bookkeeping                                                 */
/* ------------------------------------------------------------------ */

#define MAX_DEPOSITS 8

static struct
{
   char    *path;
   uint8_t *data;
   size_t   size;
} deposits[MAX_DEPOSITS];

static unsigned deposit_count;
static bool     done_called;
static bool     done_all_ok;

static void on_deposit(void *ud, const char *path, uint8_t *data,
      size_t size)
{
   if (deposit_count >= MAX_DEPOSITS)
   {
      free(data);
      return;
   }
   deposits[deposit_count].path = strdup(path);
   deposits[deposit_count].data = data;   /* ownership taken */
   deposits[deposit_count].size = size;
   deposit_count++;
}

static void on_done(void *ud, bool all_ok)
{
   done_called = true;
   done_all_ok = all_ok;
}

static void release_deposits(void)
{
   unsigned i;
   for (i = 0; i < deposit_count; i++)
   {
      free(deposits[i].path);
      free(deposits[i].data);
   }
   memset(deposits, 0, sizeof(deposits));
   deposit_count = 0;
}

/* ------------------------------------------------------------------ */
/* Progress observation                                                */
/* ------------------------------------------------------------------ */

static int8_t progress_first;
static int8_t progress_last;
static bool   progress_seen;
static bool   progress_went_backwards;
static bool   progress_saw_intermediate;

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

/* The queue exposes progress on the task itself; sample it each tick
 * rather than intercepting the setter. */
static bool sample_progress_finder(retro_task_t *task, void *userdata)
{
   /* Through the accessor, not the field: task_set_progress() writes
    * it under property_lock and the worker may be doing exactly that
    * while this runs.  Reading it raw is a data race - one TSan
    * catches, as it did here. */
   int8_t p = task_get_progress(task);

   if (p < 0)
      return false;

   if (!progress_seen)
   {
      progress_first = p;
      progress_seen  = true;
   }
   else if (p < progress_last)
      progress_went_backwards = true;

   /* The point of the feature: a value that is neither the initial
    * nothing-yet nor the final done.  Without one, a "percentage"
    * would just be a two-state flag. */
   if (p > 0 && p < 100)
      progress_saw_intermediate = true;

   progress_last = p;
   return false;   /* never claims a task */
}

static void sample_progress(void)
{
   task_finder_data_t find_data;
   find_data.func     = sample_progress_finder;
   find_data.userdata = NULL;
   task_queue_find(&find_data);
}

static void reset_observations(void)
{
   release_deposits();
   done_called             = false;
   done_all_ok             = false;
   progress_seen           = false;
   progress_first          = 0;
   progress_last           = -1;
   progress_went_backwards = false;
   progress_saw_intermediate = false;
}

/* Runs a prefetch to completion, returning the tick count.  When
 * @cancel_at is non-zero the run is abandoned at that tick and then
 * drained so cleanup actually executes. */
static unsigned run_prefetch(const char **paths, size_t count,
      unsigned cancel_at)
{
   unsigned ticks = 0;
   bool cancelled = false;

   /* The threading mode is a GLOBAL that task_queue_check()
    * reconciles against on every call, so a lane that leaves it set
    * silently converts every later lane.  State it here rather than
    * inheriting whatever ran before. */
   task_queue_unset_threaded();
   task_queue_init(false, NULL);
   reset_observations();

   if (!task_push_content_prefetch(paths, count, on_deposit, on_done,
         NULL))
   {
      CHECK(false, "prefetch push failed");
      task_queue_deinit();
      return 0;
    }

   while (queue_busy() && ticks < 100000)
   {
      sample_progress();
      task_queue_check();
      ticks++;

      /* task_queue_reset only FLAGS cancellation; the queue still has
       * to retire the task for its cleanup to run, and the unthreaded
       * deinit does not drain.  Keep pumping. */
      if (cancel_at && !cancelled && ticks == cancel_at)
      {
         task_queue_reset();
         cancelled = true;
      }
   }

   task_queue_deinit();
   return ticks;
}

/* ------------------------------------------------------------------ */
/* Lanes                                                               */
/* ------------------------------------------------------------------ */

/* Comfortably more than one CONTENT_PREFETCH_TICK_BYTES tick. */
#define BIG_FILE_SIZE (10u * 1024u * 1024u)

static bool deposit_matches(unsigned idx, size_t size)
{
   size_t i;
   if (deposits[idx].size != size)
      return false;
   for (i = 0; i < size; i++)
      if (deposits[idx].data[i] != byte_at(i))
         return false;
   return true;
}

static void lane_contents_and_budget(void)
{
   unsigned had = failures;
   char path[512];
   const char *paths[1];
   unsigned ticks;

   snprintf(path, sizeof(path), "%s/big.bin", fixture_dir);
   CHECK(write_fixture(path, BIG_FILE_SIZE), "fixture write failed");
   paths[0] = path;

   ticks = run_prefetch(paths, 1, 0);

   CHECK(done_called && done_all_ok, "run did not complete cleanly");
   CHECK(deposit_count == 1, "expected 1 deposit, got %u",
         deposit_count);
   if (deposit_count == 1)
      CHECK(deposit_matches(0, BIG_FILE_SIZE),
            "deposited bytes differ from the file");

   /* A file several times the per-tick budget cannot arrive in one
    * handler invocation; if it does, the budget stopped applying. */
   CHECK(ticks > 2, "the whole file arrived in %u ticks - the "
         "per-tick byte budget is not being honoured", ticks);

   if (failures == had)
      fprintf(stderr, "[pass] contents + budget lane (%u ticks)\n",
            ticks);
   release_deposits();
}

static void lane_progress(void)
{
   unsigned had = failures;
   char path[512];
   const char *paths[1];

   snprintf(path, sizeof(path), "%s/big.bin", fixture_dir);
   CHECK(write_fixture(path, BIG_FILE_SIZE), "fixture write failed");
   paths[0] = path;

   run_prefetch(paths, 1, 0);

   CHECK(progress_seen, "no progress was ever reported");
   CHECK(!progress_went_backwards, "progress went backwards");
   CHECK(progress_saw_intermediate,
         "progress never reported a value between 0 and 100 - it "
         "jumped straight to done, which is not a progress "
         "indication");
   CHECK(progress_last == 100,
         "final progress was %d, wanted 100", (int)progress_last);

   if (failures == had)
      fprintf(stderr, "[pass] progress lane (saw intermediate, "
            "ended at %d%%)\n", (int)progress_last);
   release_deposits();
}

static void lane_multiple_files(void)
{
   unsigned had = failures;
   char p1[512], p2[512];
   const char *paths[2];

   snprintf(p1, sizeof(p1), "%s/a.bin", fixture_dir);
   snprintf(p2, sizeof(p2), "%s/b.bin", fixture_dir);
   CHECK(write_fixture(p1, 6u * 1024u * 1024u), "fixture write");
   CHECK(write_fixture(p2, 5u * 1024u * 1024u), "fixture write");
   paths[0] = p1;
   paths[1] = p2;

   run_prefetch(paths, 2, 0);

   CHECK(done_called && done_all_ok, "run did not complete cleanly");
   CHECK(deposit_count == 2, "expected 2 deposits, got %u",
         deposit_count);
   if (deposit_count == 2)
   {
      CHECK(deposits[0].size == 6u * 1024u * 1024u
            && deposits[1].size == 5u * 1024u * 1024u,
            "deposited sizes are wrong (%u, %u)",
            (unsigned)deposits[0].size, (unsigned)deposits[1].size);
      CHECK(deposit_matches(0, 6u * 1024u * 1024u),
            "first file's bytes differ");
   }
   CHECK(progress_last == 100,
         "final progress across two files was %d, wanted 100",
         (int)progress_last);

   if (failures == had)
      fprintf(stderr, "[pass] multiple-files lane\n");
   release_deposits();
}

static void lane_unreadable_is_skipped(void)
{
   unsigned had = failures;
   char good[512], missing[512];
   const char *paths[2];

   snprintf(good, sizeof(good), "%s/a.bin", fixture_dir);
   snprintf(missing, sizeof(missing), "%s/not_here.bin", fixture_dir);
   CHECK(write_fixture(good, 2u * 1024u * 1024u), "fixture write");
   paths[0] = good;
   paths[1] = missing;

   run_prefetch(paths, 2, 0);

   CHECK(done_called, "done was not called");
   CHECK(!done_all_ok,
         "a skipped path should report not-all-ok so the load knows "
         "to read it the ordinary way");
   CHECK(deposit_count == 1,
         "expected only the readable file to deposit, got %u",
         deposit_count);

   if (failures == had)
      fprintf(stderr, "[pass] skip lane\n");
   release_deposits();
}

static void lane_cancel_midstream(void)
{
   unsigned had = failures;
   char path[512];
   const char *paths[1];

   snprintf(path, sizeof(path), "%s/big.bin", fixture_dir);
   CHECK(write_fixture(path, BIG_FILE_SIZE), "fixture write failed");
   paths[0] = path;

   /* Abandon on the second tick, with the transfer part way through
    * and its buffer held by the task.  LeakSanitizer is the
    * assertion; the deposit count only records what was already
    * complete. */
   run_prefetch(paths, 1, 2);

   CHECK(deposit_count == 0,
         "a transfer cancelled mid-stream deposited %u buffers",
         deposit_count);

   if (failures == had)
      fprintf(stderr, "[pass] cancel lane\n");
   release_deposits();
}

/* The lanes above drive the unthreaded queue, which is the
 * configuration the pacing matters for - but it runs the handler on
 * the calling thread, so a TSan run over them proves nothing about
 * the threaded queue, where the handler runs on a worker and the
 * deposit/done callbacks run on the thread pumping the queue.  That
 * hand-off is where a data race would actually live, so drive it
 * once for real. */
static void lane_threaded_queue(void)
{
   unsigned had = failures;
   char path[512];
   const char *paths[1];
   unsigned ticks = 0;

   snprintf(path, sizeof(path), "%s/big.bin", fixture_dir);
   CHECK(write_fixture(path, BIG_FILE_SIZE), "fixture write failed");
   paths[0] = path;

   task_queue_set_threaded();
   task_queue_init(true, NULL);
   reset_observations();

   if (!task_push_content_prefetch(paths, 1, on_deposit, on_done,
         NULL))
   {
      CHECK(false, "threaded prefetch push failed");
      task_queue_deinit();
      return;
   }

   /* Yield between polls.  A busy spin here starves the worker on a
    * single-core machine - which is what CI runners and this
    * container are - and the effect is worse under a sanitizer,
    * where everything is slower.  That produced a lane that passed
    * plain and under TSan but failed under ASan, which looked like a
    * product bug and was purely the test hogging the CPU. */
   while (!done_called && ticks < 20000)
   {
      task_queue_check();
      retro_sleep(1);
      ticks++;
   }

   task_queue_deinit();
   task_queue_unset_threaded();

   CHECK(done_called && done_all_ok,
         "threaded run did not complete cleanly");
   CHECK(deposit_count == 1, "expected 1 deposit, got %u",
         deposit_count);
   if (deposit_count == 1)
      CHECK(deposit_matches(0, BIG_FILE_SIZE),
            "threaded run deposited bytes differing from the file");

   if (failures == had)
      fprintf(stderr, "[pass] threaded-queue lane\n");
   release_deposits();
}

/* The progress CALLBACK - the path the launch notification uses -
 * is separate from the task's own progress field, so it needs its
 * own coverage: a caller that never sees a callback shows nothing,
 * however correct the field is. */
static unsigned cb_progress_count;
static int8_t   cb_progress_last;
static bool     cb_progress_backwards;
static bool     cb_progress_intermediate;

static void on_progress(void *ud, int8_t progress)
{
   if (cb_progress_count && progress < cb_progress_last)
      cb_progress_backwards = true;
   if (progress > 0 && progress < 100)
      cb_progress_intermediate = true;
   cb_progress_last = progress;
   cb_progress_count++;
}

static void lane_progress_callback(void)
{
   unsigned had = failures;
   char path[512];
   const char *paths[1];
   unsigned ticks = 0;

   snprintf(path, sizeof(path), "%s/big.bin", fixture_dir);
   CHECK(write_fixture(path, BIG_FILE_SIZE), "fixture write failed");
   paths[0] = path;

   task_queue_unset_threaded();
   task_queue_init(false, NULL);
   reset_observations();
   cb_progress_count        = 0;
   cb_progress_last         = -1;
   cb_progress_backwards    = false;
   cb_progress_intermediate = false;

   if (!task_push_content_prefetch_progress(paths, 1, on_deposit,
         on_done, on_progress, NULL))
   {
      CHECK(false, "prefetch push with progress failed");
      task_queue_deinit();
      return;
   }

   while (queue_busy() && ticks < 100000)
   {
      task_queue_check();
      ticks++;
   }
   task_queue_deinit();

   CHECK(done_called && done_all_ok, "run did not complete cleanly");
   CHECK(cb_progress_count > 1,
         "the progress callback fired %u times - a caller cannot "
         "show a moving percentage from that", cb_progress_count);
   CHECK(!cb_progress_backwards, "progress callback went backwards");
   CHECK(cb_progress_intermediate,
         "the callback never reported a value between 0 and 100");
   CHECK(cb_progress_last == 100,
         "the callback's last value was %d, wanted 100",
         (int)cb_progress_last);

   if (failures == had)
      fprintf(stderr, "[pass] progress-callback lane (%u updates)\n",
            cb_progress_count);
   release_deposits();
}

/* A NULL progress callback must be accepted and simply not called -
 * that is the state for every caller that does not want it, and the
 * behaviour the plain push keeps. */
static void lane_null_progress_callback(void)
{
   unsigned had = failures;
   char path[512];
   const char *paths[1];

   snprintf(path, sizeof(path), "%s/a.bin", fixture_dir);
   CHECK(write_fixture(path, 2u * 1024u * 1024u), "fixture write");
   paths[0] = path;

   cb_progress_count = 0;
   run_prefetch(paths, 1, 0);   /* plain push: no progress callback */

   CHECK(done_called && done_all_ok, "run did not complete cleanly");
   CHECK(cb_progress_count == 0,
         "the progress callback fired %u times without being asked "
         "for", cb_progress_count);

   if (failures == had)
      fprintf(stderr, "[pass] null-progress-callback lane\n");
   release_deposits();
}

int main(void)
{
   char cmd[600];

   snprintf(fixture_dir, sizeof(fixture_dir),
         "/tmp/prefetch_fixture_%ld", (long)getpid());
   snprintf(cmd, sizeof(cmd), "mkdir -p %s", fixture_dir);
   if (system(cmd) != 0)
   {
      fprintf(stderr, "fixture mkdir failed\n");
      return 1;
   }

   lane_contents_and_budget();
   lane_progress();
   lane_multiple_files();
   lane_unreadable_is_skipped();
   lane_cancel_midstream();
   lane_progress_callback();
   lane_null_progress_callback();
   /* Last: it is the only lane that turns threading on, and the mode
    * is global.  Running it last means a mistake in restoring it
    * cannot silently convert the lanes above into something other
    * than what they claim to test - which is precisely what happened
    * before, and what TSan surfaced. */
   lane_threaded_queue();

   snprintf(cmd, sizeof(cmd), "rm -rf %s", fixture_dir);
   if (system(cmd) != 0) { }

   if (failures)
   {
      fprintf(stderr, "FAIL content_prefetch_test: %u failures\n",
            failures);
      return 1;
   }
   fprintf(stderr, "PASS content_prefetch_test\n");
   return 0;
}
