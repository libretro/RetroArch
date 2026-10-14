/* The threaded task queue's worker against the main thread, scheduled
 * as Horizon schedules them.
 *
 * Horizon on 3DS and Switch never time-slices, and the worker shares a
 * core with the main thread.  A host gets the same rules from
 * SCHED_FIFO with everything pinned to one CPU, so each lane runs in a
 * child process set up that way, with the main thread at the
 * priority the Switch gives it.  task_queue.c is built with
 * TASK_QUEUE_FORCE_SWITCH_PRIO and the priority calls stood in over
 * SCHED_FIFO (fifo_svc_shim.c), so its Switch code is what places the
 * worker.
 *
 * The main thread plays the frontend: it checks the queue, then waits
 * for the next frame.
 *
 * Lanes:
 *  - a handler that waits on a callback the main thread runs, by
 *    returning until the callback has been seen and not by parking,
 *    still finishes: the main thread gets the core to run it;
 *  - frames keep coming while a task works: no gap between two frames
 *    is much over a frame.
 *
 * A lane that livelocks is killed by the parent and fails.  Setting
 * SCHED_FIFO takes CAP_SYS_NICE (run as root); without it every lane
 * reports itself skipped.
 */
#if defined(__linux__)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <queues/task_queue.h>
#include <switch/kernel/svc.h>

#if defined(__linux__)

#define LANE_SKIPPED  77
/* Horizon 0x2C, the Switch main thread, through the shim's mapping. */
#define MAIN_FIFO     36
/* How long the parent gives a lane before calling it livelocked. */
#define LANE_LIMIT_S  10
#define FRAME_US      5000
/* Half a second of work, well inside the 950 ms a second Linux lets
 * real-time threads have of a CPU by default: past it they are all
 * held back for the rest of the second, which would read as a gap. */
#define TASK_SLICE_US 4000
#define TASK_PASSES   120
#define MAX_GAP_US    50000

static long long now_us(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (long long)ts.tv_sec * 1000000LL + ts.tv_nsec / 1000;
}

static void spin_us(long long us)
{
   long long end = now_us() + us;
   while (now_us() < end) { }
}

/* The frame: blocks, as the wait for the vblank does. */
static void wait_frame(void)
{
   struct timespec ts;
   ts.tv_sec  = 0;
   ts.tv_nsec = FRAME_US * 1000L;
   nanosleep(&ts, NULL);
}

/* ---- lane: a handler that polls for a main-thread callback ---- */

static retro_atomic_int_t child_seen;
static retro_atomic_int_t parent_done;
static bool               child_pushed;

static void child_handler(retro_task_t *task)
{
   task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

/* Runs on the main thread, from task_queue_check(). */
static void child_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   (void)task; (void)task_data; (void)user_data; (void)err;
   retro_atomic_store_release_int(&child_seen, 1);
}

static void polling_handler(retro_task_t *task)
{
   if (!child_pushed)
   {
      retro_task_t *child = task_init();
      child_pushed        = true;
      if (child)
      {
         child->handler   = child_handler;
         child->callback  = child_callback;
         task_queue_push(child);
      }
      return;
   }
   if (retro_atomic_load_acquire_int(&child_seen))
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void polling_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   (void)task; (void)task_data; (void)user_data; (void)err;
   retro_atomic_store_release_int(&parent_done, 1);
}

static int lane_polling_handler_finishes(void)
{
   retro_task_t *task = task_init();
   int frames;

   if (!task)
      return 1;
   task->handler  = polling_handler;
   task->callback = polling_callback;
   task_queue_push(task);

   for (frames = 0; frames < 400; frames++)
   {
      task_queue_check();
      if (retro_atomic_load_acquire_int(&parent_done))
      {
         printf("    finished after %d frames\n", frames);
         return 0;
      }
      wait_frame();
   }
   printf("    not finished after %d frames\n", frames);
   return 1;
}

/* ---- lane: frames while a task works ---- */

static retro_atomic_int_t passes;
static retro_atomic_int_t busy_done;

static void busy_handler(retro_task_t *task)
{
   spin_us(TASK_SLICE_US);
   if (retro_atomic_fetch_add_int(&passes, 1) + 1 >= TASK_PASSES)
      task_set_flags(task, RETRO_TASK_FLG_FINISHED, true);
}

static void busy_callback(retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   (void)task; (void)task_data; (void)user_data; (void)err;
   retro_atomic_store_release_int(&busy_done, 1);
}

static int lane_frames_keep_coming(void)
{
   retro_task_t *task;
   long long last;
   long long max_gap = 0;
   int frames        = 0;

   wait_frame();
   last = now_us();
   if (!(task = task_init()))
      return 1;
   task->handler  = busy_handler;
   task->callback = busy_callback;
   task_queue_push(task);

   while (!retro_atomic_load_acquire_int(&busy_done) && frames < 2000)
   {
      long long t;
      task_queue_check();
      wait_frame();
      t = now_us();
      if (t - last > max_gap)
         max_gap = t - last;
      last = t;
      frames++;
   }
   printf("    longest gap between frames %lld us over %d frames, "
         "%d passes\n", max_gap, frames,
         retro_atomic_load_acquire_int(&passes));
   return (retro_atomic_load_acquire_int(&busy_done)
         && max_gap <= MAX_GAP_US) ? 0 : 1;
}

/* ---- running a lane as Horizon would ---- */

static int run_lane(const char *name, int (*lane)(void))
{
   pid_t pid;
   int status = 0;
   long long deadline;

   fflush(stdout);
   if ((pid = fork()) < 0)
      return 1;
   if (pid == 0)
   {
      cpu_set_t cpus;
      struct sched_param sp;
      s32 prio = -1;
      int rc;

      CPU_ZERO(&cpus);
      CPU_SET(0, &cpus);
      sp.sched_priority = MAIN_FIFO;
      if (     sched_setaffinity(0, sizeof(cpus), &cpus) != 0
            || sched_setscheduler(0, SCHED_FIFO, &sp) != 0)
         _exit(LANE_SKIPPED);
      /* The shim reads this thread as the Switch main thread. */
      if (svcGetThreadPriority(&prio, CUR_THREAD_HANDLE) != 0
            || prio != 0x2C)
         _exit(1);
      task_queue_init(true, NULL);
      rc = lane();
      task_queue_deinit();
      fflush(stdout);
      _exit(rc);
   }

   deadline = now_us() + (long long)LANE_LIMIT_S * 1000000LL;
   for (;;)
   {
      pid_t r = waitpid(pid, &status, WNOHANG);
      if (r == pid)
         break;
      if (now_us() > deadline)
      {
         kill(pid, SIGKILL);
         waitpid(pid, &status, 0);
         printf("  [FAIL] %s: livelocked, killed after %d s\n",
               name, LANE_LIMIT_S);
         return 1;
      }
      usleep(20000);
   }

   if (WIFEXITED(status) && WEXITSTATUS(status) == LANE_SKIPPED)
   {
      printf("  [skip] %s: SCHED_FIFO not permitted\n", name);
      return -1;
   }
   if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
   {
      printf("  [pass] %s\n", name);
      return 0;
   }
   printf("  [FAIL] %s\n", name);
   return 1;
}

int main(void)
{
   unsigned failures = 0;
   unsigned skipped  = 0;
   int r;

   r = run_lane("a handler polling for a main-thread callback finishes",
         lane_polling_handler_finishes);
   failures += (r > 0);
   skipped  += (r < 0);
   r = run_lane("frames keep coming while a task works",
         lane_frames_keep_coming);
   failures += (r > 0);
   skipped  += (r < 0);

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   if (skipped)
      printf("lanes skipped: SCHED_FIFO needs CAP_SYS_NICE "
            "(run as root)\n");
   else
      printf("all lanes passed\n");
   return 0;
}

#else

int main(void)
{
   printf("lanes skipped: SCHED_FIFO and CPU pinning are Linux's\n");
   return 0;
}

#endif
