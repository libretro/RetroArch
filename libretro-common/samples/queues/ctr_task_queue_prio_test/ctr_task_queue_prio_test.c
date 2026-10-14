/* The threaded task queue's worker on 3DS, against the main thread.
 *
 * A 3DS homebrew (.3dsx) for hardware or an emulator in Old 3DS mode;
 * on New 3DS the worker gets a core of its own and the lanes pass
 * whatever its priority.  The kernel never time-slices, so on Old 3DS
 * the worker and the main thread share a core and the higher priority
 * of the two keeps it until it waits.
 *
 * The main thread plays the frontend: each frame it does some work,
 * checks the queue and waits for the vblank.  The task plays one that
 * keeps working - every pass a slice of CPU work and nothing to wait
 * on - like an archive extraction or a hash.
 *
 * Lanes:
 *  - frames keep coming while the task works: no gap between two
 *    vblanks the main thread sees is much over a frame;
 *  - the task still gets the core: it finishes in the time the main
 *    thread spends waiting, well inside the budget;
 *  - task_queue_wait() from the main thread returns once the task is
 *    done: the worker runs while the main thread waits on it.
 *
 * Results are printed on the top screen and written to
 * sdmc:/ctr_task_queue_prio_test.txt; the last line is
 * "all lanes passed" or "N failure(s)".
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include <3ds.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <queues/task_queue.h>

#define TICKS_PER_MS      268111ULL
/* CPU work per handler pass, and the passes a task makes: a second
 * of work all told, in passes longer than a frame, as a chunk of an
 * archive extraction can be. */
#define TASK_SLICE_MS     25
#define TASK_PASSES       40
/* CPU work the main thread does per frame, a quarter of one. */
#define FRAME_WORK_MS     4
/* A frame is 16.7 ms; a gap past this means frames were dropped. */
#define MAX_GAP_MS        40
/* The task needs a second of the core; the main thread leaves it
 * three quarters of every frame. */
#define TASK_BUDGET_MS    3000

static unsigned failures = 0;
static FILE    *out      = NULL;

static void report(const char *fmt, ...)
{
   va_list ap;
   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);
   if (out)
   {
      va_start(ap, fmt);
      vfprintf(out, fmt, ap);
      va_end(ap);
      fflush(out);
   }
}

static void check(bool cond, const char *what)
{
   report("  [%s] %s\n", cond ? "pass" : "FAIL", what);
   if (!cond)
      failures++;
}

static u64 now_ms(void)
{
   return svcGetSystemTick() / TICKS_PER_MS;
}

static void spin_ms(unsigned ms)
{
   u64 end = svcGetSystemTick() + (u64)ms * TICKS_PER_MS;
   while (svcGetSystemTick() < end) { }
}

/* ---- the task ---- */

static retro_atomic_int_t passes;
static retro_atomic_int_t done;

static void busy_handler(retro_task_t *task)
{
   spin_ms(TASK_SLICE_MS);
   if (retro_atomic_fetch_add_int(&passes, 1) + 1 >= TASK_PASSES)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void busy_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   (void)task; (void)task_data; (void)user_data; (void)err;
   retro_atomic_store_release_int(&done, 1);
}

static bool push_busy(void)
{
   retro_task_t *task = task_init();
   if (!task)
      return false;
   retro_atomic_store_release_int(&passes, 0);
   retro_atomic_store_release_int(&done, 0);
   task->handler  = busy_handler;
   task->callback = busy_callback;
   return task_queue_push(task);
}

/* ---- the lanes ---- */

static void lane_frames_keep_coming(void)
{
   char line[96];
   u64  start;
   u64  last;
   u64  max_gap = 0;
   u64  took    = 0;
   int  frames  = 0;

   report("frames while a task works:\n");
   /* From before the push: a worker above the main thread takes the
    * core as soon as it is handed the task. */
   gspWaitForVBlank();
   start = last = now_ms();
   if (!push_busy())
   {
      check(false, "the task was pushed");
      return;
   }
   while (now_ms() - start < TASK_BUDGET_MS + 1000)
   {
      u64 t;
      spin_ms(FRAME_WORK_MS);
      task_queue_check();
      if (retro_atomic_load_acquire_int(&done) && !took)
         took = now_ms() - start;
      gspWaitForVBlank();
      t = now_ms();
      if (t - last > max_gap)
         max_gap = t - last;
      last = t;
      frames++;
      if (took)
         break;
   }

   snprintf(line, sizeof(line),
         "longest gap between frames %llu ms (%d frames)",
         (unsigned long long)max_gap, frames);
   check(max_gap <= MAX_GAP_MS, line);
   snprintf(line, sizeof(line),
         "the task finished in %llu ms, %d passes",
         (unsigned long long)took,
         retro_atomic_load_acquire_int(&passes));
   check(took && took <= TASK_BUDGET_MS, line);
}

static void lane_wait_returns(void)
{
   char line[96];
   u64  start;

   report("task_queue_wait on a task that works:\n");
   start = now_ms();
   if (!push_busy())
   {
      check(false, "the task was pushed");
      return;
   }
   task_queue_wait(NULL, NULL);
   snprintf(line, sizeof(line), "the wait returned after %llu ms",
         (unsigned long long)(now_ms() - start));
   check(retro_atomic_load_acquire_int(&done) == 1
         && now_ms() - start <= TASK_BUDGET_MS, line);
}

int main(void)
{
   bool new3ds = false;

   gfxInitDefault();
   consoleInit(GFX_TOP, NULL);
   out = fopen("sdmc:/ctr_task_queue_prio_test.txt", "w");

   APT_CheckNew3DS(&new3ds);
   report("ctr_task_queue_prio_test on %s\n", new3ds ? "New 3DS" : "Old 3DS");

   task_queue_init(true, NULL);
   lane_frames_keep_coming();
   lane_wait_returns();
   task_queue_deinit();

   if (failures)
      report("%u failure(s)\n", failures);
   else
      report("all lanes passed\n");
   if (out)
      fclose(out);

   while (aptMainLoop())
   {
      hidScanInput();
      if (hidKeysDown() & KEY_START)
         break;
      gspWaitForVBlank();
   }
   gfxExit();
   return failures ? 1 : 0;
}
