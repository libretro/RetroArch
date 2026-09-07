/* Oracle for menu/menu_dirwalk.c: the budgeted, cancellable
 * directory enumeration behind interactive menu list building.
 *
 * Lanes:
 *   parity     - a consumed result equals dir_list_new() + the same
 *                sort, byte for byte, across the flag and sort-mode
 *                combinations the displaylist callers use.
 *   fast path  - a small directory completes DONE on the first
 *                request: no task, no refresh, single tick.
 *   deferred   - a large directory goes PENDING, the walk finishes
 *                on the task queue, the refresh callback fires on
 *                the main thread, and the re-issued request consumes
 *                a result with full parity.
 *   cancel     - superseding a pending walk by a different request,
 *                and cancelling outright, leak nothing (LSan is the
 *                judge) and never deliver a stale result.
 *   pacing     - on the plain build, no single request or
 *                task_queue_check() attributable to the walk exceeds
 *                the shared window plus slack on the large fixture.
 *   bench      - the HARD PRECONDITION: wall-clock from first
 *                request to consumed result on the large fixture
 *                stays within BENCH_FACTOR of blocking
 *                dir_list_new() + sort.  A non-blocking walk that is
 *                slower overall is a regression, not a feature.
 *   threaded   - the deferred lane again under the threaded task
 *                queue (the TSan target: the handler runs on a
 *                worker, hand-off happens in the main-thread
 *                callback).
 *
 * 'sanitize' argv runs every lane but skips the wall-clock
 * assertions, which instrumented builds cannot honestly meet. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>

#include <lists/dir_list.h>
#include <lists/string_list.h>
#include <queues/task_queue.h>
#include <features/features_cpu.h>
#include <file/file_path.h>

#include "../../../menu/menu_dirwalk.h"

#define SMALL_FILES        200
#define SMALL_DIRS         8
#define BIG_FILES          40000
#define BIG_DIRS           25

/* Same rationale as the scanner oracle: the shared window is ~4ms;
 * a single entry straddling the deadline plus bookkeeping sits on
 * top.  4x keeps the discriminator sharp (an unbudgeted 40k-entry
 * walk is an order of magnitude past it) while absorbing runner
 * jitter. */
#define PACING_SLACK_MULTIPLIER 4

/* Wall-clock to a consumed result vs blocking walk + sort, both
 * sides warmed and taken as the best of three so the page cache and
 * the scheduler stop voting.  The incremental path's honest cost at
 * 40k entries - per-entry budget checks, slice bookkeeping, task
 * scheduling, and a mergesort in place of qsort - measures ~10-15%;
 * the factor rejects a mechanism that regresses to a second
 * enumeration or a quadratic anywhere (those measure 2x and up),
 * while the pacing lane, not this one, owns the interactivity
 * claim. */
#define BENCH_FACTOR 1.35
#define BENCH_ROUNDS 3

static char fixture_root[256];

/* ------------------------------------------------------------------ */
/* The clock is the test's instrument (save_state_io_test precedent):
 * cpu_features_get_time_usec() is provided here rather than linked.
 * With clock_step 0 it reads the real monotonic clock, for the
 * pacing and bench lanes.  With clock_step set, every observation
 * advances virtual time by a fixed amount, so "the shared window is
 * exhausted" becomes a deterministic event after a known number of
 * budget checks - which is what makes the deferred and cancel
 * mechanics assertable on any machine at any cache temperature. */
/* ------------------------------------------------------------------ */

#include <time.h>

static retro_time_t clock_virtual_now = 1000000;
static retro_time_t clock_step        = 0;

retro_time_t cpu_features_get_time_usec(void)
{
   if (clock_step)
   {
      retro_time_t t     = clock_virtual_now;
      clock_virtual_now += clock_step;
      return t;
   }
   {
      struct timespec ts;
      clock_gettime(CLOCK_MONOTONIC, &ts);
      return (retro_time_t)ts.tv_sec * 1000000
           + (retro_time_t)(ts.tv_nsec / 1000);
   }
}

/* Unused half of features_cpu on this path. */
uint64_t cpu_features_get(void) { return 0; }

/* NBIO_XFER_TICK_USEC is 4000; one third of it per observation
 * exhausts the window after a couple of budget checks, forcing the
 * deferred path on even the small fixture. */
#define VIRTUAL_CLOCK_STEP 1500

/* ------------------------------------------------------------------ */
/* Stubs the task queue wants                                          */
/* ------------------------------------------------------------------ */

static void msgq_push(retro_task_t *task, const char *msg,
      unsigned prio, unsigned duration, bool flush)
{
   (void)task; (void)msg; (void)prio; (void)duration; (void)flush;
}

void ui_companion_driver_notify_refresh(void)
{
}

/* ------------------------------------------------------------------ */
/* Fixture                                                             */
/* ------------------------------------------------------------------ */

static bool write_stub_file(const char *path)
{
   FILE *f = fopen(path, "w");
   if (!f)
      return false;
   fputs("x", f);
   fclose(f);
   return true;
}

/* The walk under test is non-recursive - the menu browses one
 * directory level - so the files must sit FLAT in the walked
 * directory; that is also the real-world stress case (a ROM
 * directory with tens of thousands of files).  A handful of
 * subdirectories cover include_dirs and the dirs-first sort, a
 * hidden file and a hidden directory cover include_hidden, and
 * alternating extensions give the ext filter something to reject. */
static bool build_tree(const char *root, size_t files, size_t dirs)
{
   size_t i;
   char path[512];

   if (!path_mkdir(root))
      return false;

   for (i = 0; i < dirs; i++)
   {
      snprintf(path, sizeof(path), "%s/d%02u", root, (unsigned)i);
      if (!path_mkdir(path))
         return false;
   }

   snprintf(path, sizeof(path), "%s/.hidden_dir", root);
   if (!path_mkdir(path))
      return false;
   snprintf(path, sizeof(path), "%s/.hidden_file", root);
   if (!write_stub_file(path))
      return false;

   for (i = 0; i < files; i++)
   {
      snprintf(path, sizeof(path), "%s/f%05u.%s",
            root, (unsigned)i, (i & 1) ? "bin" : "txt");
      if (!write_stub_file(path))
         return false;
   }
   return true;
}

static void rm_rf(const char *path)
{
   DIR *d = opendir(path);
   struct dirent *e;
   char sub[768];

   if (!d)
   {
      remove(path);
      return;
   }
   while ((e = readdir(d)))
   {
      if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
         continue;
      snprintf(sub, sizeof(sub), "%s/%s", path, e->d_name);
      rm_rf(sub);
   }
   closedir(d);
   remove(path);
}

/* ------------------------------------------------------------------ */
/* Blocking reference                                                  */
/* ------------------------------------------------------------------ */

static struct string_list *reference_list(const char *dir,
      const char *ext, bool include_dirs, bool include_hidden,
      bool include_compressed, enum menu_dirwalk_sort sort_mode)
{
   struct string_list *list = dir_list_new(dir, ext, include_dirs,
         include_hidden, include_compressed, false);
   if (!list)
      return NULL;
   switch (sort_mode)
   {
      case MENU_DIRWALK_SORT_DIR_FIRST:
         dir_list_sort(list, true);
         break;
      case MENU_DIRWALK_SORT_IGNORE_EXT:
         dir_list_sort_ignore_ext(list, true);
         break;
      default:
         break;
   }
   return list;
}

static bool lists_equal(const struct string_list *a,
      const struct string_list *b, const char *lane)
{
   size_t i;
   if (a->size != b->size)
   {
      fprintf(stderr, "%s: size %u vs %u\n", lane,
            (unsigned)a->size, (unsigned)b->size);
      return false;
   }
   for (i = 0; i < a->size; i++)
   {
      if (   strcmp(a->elems[i].data, b->elems[i].data)
          || a->elems[i].attr.i != b->elems[i].attr.i)
      {
         fprintf(stderr, "%s: entry %u differs: \"%s\"/%d vs \"%s\"/%d\n",
               lane, (unsigned)i,
               a->elems[i].data, a->elems[i].attr.i,
               b->elems[i].data, b->elems[i].attr.i);
         return false;
      }
   }
   return true;
}

/* ------------------------------------------------------------------ */
/* Refresh callback probe                                              */
/* ------------------------------------------------------------------ */

static unsigned refresh_fired;
static unsigned refresh_last_tag;

static void on_refresh(unsigned tag)
{
   refresh_fired++;
   refresh_last_tag = tag;
}

/* Pump the queue until the refresh callback fires (or give up). */
static bool pump_until_refresh(unsigned had_fired,
      retro_time_t *max_check_usec)
{
   unsigned spins = 0;
   while (refresh_fired == had_fired)
   {
      retro_time_t t0 = cpu_features_get_time_usec();
      retro_time_t dt;
      task_queue_check();
      dt = cpu_features_get_time_usec() - t0;
      if (max_check_usec && dt > *max_check_usec)
         *max_check_usec = dt;
      if (++spins > 60000000)
      {
         fprintf(stderr, "refresh never fired\n");
         return false;
      }
   }
   return true;
}

/* Drive one request through to a consumed result, whichever path it
 * takes, recording the worst single main-thread stall. */
static struct string_list *drive_to_done(const char *dir,
      const char *ext, bool include_dirs, bool include_hidden,
      bool include_compressed, enum menu_dirwalk_sort sort_mode,
      unsigned tag, retro_time_t *max_stall_usec, bool *was_deferred)
{
   struct string_list *out = NULL;
   retro_time_t t0         = cpu_features_get_time_usec();
   enum menu_dirwalk_status st = menu_dirwalk_request(dir, ext,
         include_dirs, include_hidden, include_compressed,
         sort_mode, tag, &out);
   retro_time_t dt         = cpu_features_get_time_usec() - t0;
   unsigned had_fired      = refresh_fired;

   if (max_stall_usec && dt > *max_stall_usec)
      *max_stall_usec = dt;
   if (was_deferred)
      *was_deferred = (st == MENU_DIRWALK_PENDING);

   if (st == MENU_DIRWALK_DONE)
      return out;
   if (st == MENU_DIRWALK_FAILED)
   {
      fprintf(stderr, "request failed for \"%s\"\n", dir);
      return NULL;
   }

   /* PENDING: identical re-request must stay PENDING without
    * restarting, then the refresh must fire, then it must consume. */
   if (menu_dirwalk_request(dir, ext, include_dirs, include_hidden,
         include_compressed, sort_mode, tag, &out)
         != MENU_DIRWALK_PENDING)
   {
      fprintf(stderr, "re-request did not stay PENDING\n");
      return NULL;
   }
   if (!menu_dirwalk_pending())
   {
      fprintf(stderr, "pending() false while PENDING\n");
      return NULL;
   }
   if (!pump_until_refresh(had_fired, max_stall_usec))
      return NULL;
   if (refresh_last_tag != tag)
   {
      fprintf(stderr, "refresh tag %u, wanted %u\n",
            refresh_last_tag, tag);
      return NULL;
   }

   t0 = cpu_features_get_time_usec();
   st = menu_dirwalk_request(dir, ext, include_dirs, include_hidden,
         include_compressed, sort_mode, tag, &out);
   dt = cpu_features_get_time_usec() - t0;
   if (max_stall_usec && dt > *max_stall_usec)
      *max_stall_usec = dt;

   if (st != MENU_DIRWALK_DONE)
   {
      fprintf(stderr, "post-refresh request returned %d, not DONE\n",
            (int)st);
      return NULL;
   }
   return out;
}

/* ------------------------------------------------------------------ */
/* Lanes                                                               */
/* ------------------------------------------------------------------ */

static bool check_parity(const char *dir, const char *lane)
{
   /* The flag/sort combinations the displaylist callers use:
    * filebrowser (dirs, hidden x2, compressed, DIR_FIRST, ext on/off)
    * and the playlist-directory walks (no dirs, "lpl", DIR_FIRST or
    * IGNORE_EXT). */
   static const struct
   {
      const char *ext;
      bool include_dirs;
      bool include_hidden;
      bool include_compressed;
      enum menu_dirwalk_sort sort_mode;
   } cases[] = {
      { NULL,  true,  false, true,  MENU_DIRWALK_SORT_DIR_FIRST },
      { NULL,  true,  true,  true,  MENU_DIRWALK_SORT_DIR_FIRST },
      { "bin", true,  false, true,  MENU_DIRWALK_SORT_DIR_FIRST },
      { "bin", true,  false, false, MENU_DIRWALK_SORT_DIR_FIRST },
      { "txt", false, false, false, MENU_DIRWALK_SORT_DIR_FIRST },
      { "txt", false, false, false, MENU_DIRWALK_SORT_IGNORE_EXT },
      { NULL,  true,  false, true,  MENU_DIRWALK_SORT_NONE },
   };
   size_t ci;

   for (ci = 0; ci < sizeof(cases) / sizeof(cases[0]); ci++)
   {
      struct string_list *ref = reference_list(dir, cases[ci].ext,
            cases[ci].include_dirs, cases[ci].include_hidden,
            cases[ci].include_compressed, cases[ci].sort_mode);
      struct string_list *got = drive_to_done(dir, cases[ci].ext,
            cases[ci].include_dirs, cases[ci].include_hidden,
            cases[ci].include_compressed, cases[ci].sort_mode,
            (unsigned)ci, NULL, NULL);
      bool equal;

      if (!ref || !got)
      {
         if (ref)
            string_list_free(ref);
         if (got)
            string_list_free(got);
         fprintf(stderr, "%s: case %u did not produce lists\n",
               lane, (unsigned)ci);
         return false;
      }
      /* SORT_NONE parity holds because dir_list_iter documents its
       * completed list as pre-sort-identical to dir_list_new(). */
      equal = lists_equal(ref, got, lane);
      string_list_free(ref);
      string_list_free(got);
      if (!equal)
      {
         fprintf(stderr, "%s: case %u parity failed\n",
               lane, (unsigned)ci);
         return false;
      }
   }
   return true;
}

int main(int argc, char *argv[])
{
   char small_dir[320];
   char big_dir[320];
   bool sanitize  = (argc > 1 && !strcmp(argv[1], "sanitize"));
   int rc         = 1;
   retro_time_t blocking_usec = 0;

   snprintf(fixture_root, sizeof(fixture_root),
         "/tmp/dirwalk_fixture_%ld", (long)getpid());
   snprintf(small_dir, sizeof(small_dir), "%s/small", fixture_root);
   snprintf(big_dir, sizeof(big_dir), "%s/big", fixture_root);

   if (   !path_mkdir(fixture_root)
       || !build_tree(small_dir, SMALL_FILES, SMALL_DIRS)
       || !build_tree(big_dir, BIG_FILES, BIG_DIRS))
   {
      fprintf(stderr, "fixture build failed\n");
      goto out;
   }

   /* Blocking reference timing: one unmeasured warmup walk, then
    * best of BENCH_ROUNDS, before any queue exists. */
   {
      int round;
      struct string_list *warm = reference_list(big_dir, NULL, true,
            false, true, MENU_DIRWALK_SORT_DIR_FIRST);
      if (!warm)
      {
         fprintf(stderr, "blocking reference failed\n");
         goto out;
      }
      string_list_free(warm);

      for (round = 0; round < BENCH_ROUNDS; round++)
      {
         retro_time_t t0 = cpu_features_get_time_usec();
         retro_time_t dt;
         struct string_list *ref = reference_list(big_dir, NULL, true,
               false, true, MENU_DIRWALK_SORT_DIR_FIRST);
         dt = cpu_features_get_time_usec() - t0;
         if (!ref)
         {
            fprintf(stderr, "blocking reference failed\n");
            goto out;
         }
         string_list_free(ref);
         if (!blocking_usec || dt < blocking_usec)
            blocking_usec = dt;
      }
   }

   task_queue_init(false, msgq_push);
   menu_dirwalk_set_refresh_cb(on_refresh);

   /* Fast path: the small tree completes on the first request.
    * SORT_NONE, because the build shrinks MENU_DIRWALK_SORT_SYNC_MAX
    * to 64 so that sort-only deferral is testable below; an unsorted
    * request is the one whose fast path is size-independent. */
   {
      struct string_list *out = NULL;
      unsigned had_fired      = refresh_fired;
      if (menu_dirwalk_request(small_dir, NULL, true, false, true,
            MENU_DIRWALK_SORT_NONE, 0, &out) != MENU_DIRWALK_DONE)
      {
         fprintf(stderr, "small dir did not take the fast path\n");
         goto out_queue;
      }
      string_list_free(out);
      if (menu_dirwalk_pending() || refresh_fired != had_fired)
      {
         fprintf(stderr, "fast path left residue\n");
         goto out_queue;
      }
   }
   fprintf(stderr, "[pass] fast path lane\n");

   /* Sort-only deferral: under the real clock the small tree's walk
    * fits one window, but with the shrunken threshold its 200+
    * entries are "too many to sort inline".  The request must go
    * PENDING carrying a completed walk, the task must do nothing but
    * the resumable sort, and the consumed listing must have full
    * parity - which, with the tiny run/batch constants, drives the
    * run-sorting and merge machinery across many invocations. */
   {
      struct string_list *ref = reference_list(small_dir, NULL, true,
            false, true, MENU_DIRWALK_SORT_DIR_FIRST);
      struct string_list *got = NULL;
      bool deferred           = false;
      bool equal;

      got = drive_to_done(small_dir, NULL, true, false, true,
            MENU_DIRWALK_SORT_DIR_FIRST, 1, NULL, &deferred);
      if (!ref || !got)
      {
         if (ref)
            string_list_free(ref);
         if (got)
            string_list_free(got);
         goto out_queue;
      }
      equal = lists_equal(ref, got, "sort-deferral");
      string_list_free(ref);
      string_list_free(got);
      if (!equal)
         goto out_queue;
      if (!deferred)
      {
         fprintf(stderr, "sort-only path was not deferred\n");
         goto out_queue;
      }
   }
   fprintf(stderr, "[pass] sort-only deferral lane\n");

   if (!check_parity(small_dir, "parity/small"))
      goto out_queue;
   if (!check_parity(big_dir, "parity/big"))
      goto out_queue;
   fprintf(stderr, "[pass] parity lanes (small + big, 7 cases each)\n");

   /* Unreadable directory: FAILED, nothing pending. */
   {
      struct string_list *out = NULL;
      char nodir[384];
      snprintf(nodir, sizeof(nodir), "%s/does_not_exist", fixture_root);
      if (menu_dirwalk_request(nodir, NULL, true, false, true,
            MENU_DIRWALK_SORT_DIR_FIRST, 0, &out) != MENU_DIRWALK_FAILED
            || menu_dirwalk_pending())
      {
         fprintf(stderr, "unreadable dir not FAILED-and-idle\n");
         goto out_queue;
      }
   }
   fprintf(stderr, "[pass] unreadable-directory lane\n");

   /* Deferred mechanics under the virtual clock: with every clock
    * observation costing a fixed slice of the window, even the small
    * tree cannot finish in one request, deterministically, on any
    * machine.  This is where PENDING, the pending re-request, the
    * refresh hand-off and full parity of a deferred result are
    * pinned. */
   clock_step = VIRTUAL_CLOCK_STEP;
   {
      struct string_list *ref = NULL;
      struct string_list *got = NULL;
      bool deferred           = false;
      bool equal;

      ref = reference_list(small_dir, NULL, true, false, true,
            MENU_DIRWALK_SORT_DIR_FIRST);
      got = drive_to_done(small_dir, NULL, true, false, true,
            MENU_DIRWALK_SORT_DIR_FIRST, 3, NULL, &deferred);
      if (!ref || !got)
      {
         if (ref)
            string_list_free(ref);
         if (got)
            string_list_free(got);
         goto out_queue;
      }
      equal = lists_equal(ref, got, "deferred");
      string_list_free(ref);
      string_list_free(got);
      if (!equal)
         goto out_queue;
      if (!deferred)
      {
         fprintf(stderr,
               "deferred lane: virtual clock did not force PENDING\n");
         goto out_queue;
      }
   }
   fprintf(stderr, "[pass] deferred mechanics lane (virtual clock)\n");

   /* Cancel semantics, same deterministic footing.  Start a walk,
    * supersede it with a different request; start again and cancel
    * outright; then drain the cancelled tasks and prove their
    * completions are no-ops.  LSan owns the leak verdict. */
   {
      struct string_list *out = NULL;
      unsigned had_fired;

      if (menu_dirwalk_request(small_dir, NULL, true, false, true,
            MENU_DIRWALK_SORT_DIR_FIRST, 7, &out) != MENU_DIRWALK_PENDING)
      {
         fprintf(stderr, "cancel lane: walk not PENDING\n");
         goto out_queue;
      }
      /* Supersede with a different identity (ext filter differs). */
      if (menu_dirwalk_request(small_dir, "bin", true, false, true,
            MENU_DIRWALK_SORT_DIR_FIRST, 8, &out) != MENU_DIRWALK_PENDING)
      {
         fprintf(stderr, "cancel lane: superseding request not PENDING\n");
         goto out_queue;
      }

      /* Cancel outright */
      menu_dirwalk_cancel();
      if (menu_dirwalk_pending())
      {
         fprintf(stderr, "cancel lane: pending after cancel\n");
         goto out_queue;
      }

      /* Drain the two cancelled tasks; no refresh may fire for them
       * and no result may appear. */
      had_fired = refresh_fired;
      {
         unsigned spins;
         for (spins = 0; spins < 1000; spins++)
            task_queue_check();
      }
      task_queue_wait(NULL, NULL);
      task_queue_check();
      if (refresh_fired != had_fired || menu_dirwalk_pending())
      {
         fprintf(stderr, "cancel lane: stale completion leaked through\n");
         goto out_queue;
      }
   }
   fprintf(stderr, "[pass] cancel lane (virtual clock)\n");
   clock_step = 0;

   /* Deferred + pacing + bench on the big tree: parity checked on
    * every round, best-of-rounds total against the best blocking
    * reference, worst stall across all rounds (pacing must hold in
    * every round, not the friendliest one). */
   {
      struct string_list *ref = NULL;
      retro_time_t max_stall  = 0;
      retro_time_t total      = 0;
      bool deferred           = false;
      int round;

      ref = reference_list(big_dir, NULL, true, false, true,
            MENU_DIRWALK_SORT_DIR_FIRST);
      if (!ref)
         goto out_queue;

      for (round = 0; round < BENCH_ROUNDS; round++)
      {
         struct string_list *got = NULL;
         retro_time_t t0, dt;
         bool this_deferred      = false;
         bool equal;

         t0  = cpu_features_get_time_usec();
         got = drive_to_done(big_dir, NULL, true, false, true,
               MENU_DIRWALK_SORT_DIR_FIRST, 3, &max_stall,
               &this_deferred);
         dt  = cpu_features_get_time_usec() - t0;

         if (!got)
         {
            string_list_free(ref);
            goto out_queue;
         }
         equal = lists_equal(ref, got, "bench");
         string_list_free(got);
         if (!equal)
         {
            string_list_free(ref);
            goto out_queue;
         }
         deferred = deferred || this_deferred;
         if (!total || dt < total)
            total = dt;
      }
      string_list_free(ref);

      /* Whether this machine's real clock took the fast or the
       * deferred path (any round deferring counts), both asserts
       * below apply: the worst stall is bounded by the window either
       * way, and the total may not regress past the blocking
       * baseline.  The deterministic deferred coverage lives in the
       * virtual-clock lanes above. */
      fprintf(stderr,
            "[metrics] big=%u files path=%s total=%.1fms "
            "max_stall=%.2fms blocking_ref=%.1fms ratio=%.2f\n",
            (unsigned)BIG_FILES, deferred ? "deferred" : "fast",
            total / 1000.0, max_stall / 1000.0,
            blocking_usec / 1000.0, (double)total / (double)blocking_usec);

      if (!sanitize)
      {
         retro_time_t limit = 4000 * PACING_SLACK_MULTIPLIER;
         if (max_stall > limit)
         {
            fprintf(stderr,
                  "FAIL pacing: worst stall %.2fms exceeds %.2fms\n",
                  max_stall / 1000.0, limit / 1000.0);
            goto out_queue;
         }
         if ((double)total > (double)blocking_usec * BENCH_FACTOR)
         {
            fprintf(stderr,
                  "FAIL bench: %.1fms vs blocking %.1fms (limit x%.2f)\n",
                  total / 1000.0, blocking_usec / 1000.0, BENCH_FACTOR);
            goto out_queue;
         }
      }
   }
   fprintf(stderr, "[pass] pacing + bench lane%s\n",
         sanitize ? " (wall-clock asserts skipped)" : "");

   menu_dirwalk_cancel();
   menu_dirwalk_set_refresh_cb(NULL);
   task_queue_deinit();

   /* Threaded lane: same deferred flow, handler on a worker.  The
    * virtual clock forces the fast path to yield, so the hand-off
    * from worker handler to main-thread callback happens every run
    * (the TSan target). */
   clock_step = VIRTUAL_CLOCK_STEP;
   task_queue_init(true, msgq_push);
   menu_dirwalk_set_refresh_cb(on_refresh);
   {
      struct string_list *ref = reference_list(small_dir, NULL, true,
            false, true, MENU_DIRWALK_SORT_DIR_FIRST);
      struct string_list *got = drive_to_done(small_dir, NULL, true,
            false, true, MENU_DIRWALK_SORT_DIR_FIRST, 4, NULL, NULL);
      bool equal = false;
      if (ref && got)
         equal = lists_equal(ref, got, "threaded");
      if (ref)
         string_list_free(ref);
      if (got)
         string_list_free(got);
      if (!equal)
         goto out_queue;
   }
   fprintf(stderr, "[pass] threaded lane\n");
   clock_step = 0;

   menu_dirwalk_cancel();
   menu_dirwalk_set_refresh_cb(NULL);
   task_queue_deinit();

   fprintf(stderr, "PASS dirwalk_budget_test\n");
   rc = 0;
   goto out;

out_queue:
   menu_dirwalk_cancel();
   menu_dirwalk_set_refresh_cb(NULL);
   task_queue_deinit();
out:
   rm_rf(fixture_root);
   return rc;
}
