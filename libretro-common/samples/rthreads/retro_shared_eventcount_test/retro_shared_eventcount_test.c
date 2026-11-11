/* retro_shared_eventcount_test.c - does the eventcount wake across
 * processes, and lose nothing?
 *
 * 1. Ping-pong.  Two sides take turns on a counter in shared memory,
 *    each parking on its own object until the other has moved it:
 *    200000 handoffs, every one of which needs a wake to arrive.  A
 *    lost wake parks a side for good, and the watchdog reports it.
 *    On Linux the two sides are a parent and a forked child sharing an
 *    anonymous mapping; elsewhere (Windows) two threads, each with its
 *    own handle opened under the object's name, as two processes do.
 *
 * 2. A notify between prepare and commit makes the commit return at
 *    once, timed or not.
 *
 * 3. A timed wait with nothing published returns false, after about
 *    the time asked for and not much less.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rthreads/retro_shared_eventcount.h>
#include <rthreads/rthreads.h>
#include <retro_timers.h>
#include <features/features_cpu.h>

#if defined(__linux__)
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#endif

#define ROUNDS 200000

typedef struct
{
   retro_shared_eventcount_t ping;
   retro_shared_eventcount_t pong;
   retro_atomic_int_t        turn; /* even: the first side's move */
} shared_t;

static unsigned failures = 0;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s\n", msg); \
         failures++; \
      } \
      else \
         printf("ok   %s\n", msg); \
   } while (0)

/* Plays one side: waits for its turn on @mine, moves, wakes @theirs */
static void play(shared_t *s, int side, const char *name_mine,
      const char *name_theirs)
{
   retro_shared_eventcount_handle_t mine, theirs;
   int round;
   retro_shared_eventcount_attach(&mine, side ? &s->pong : &s->ping, name_mine);
   retro_shared_eventcount_attach(&theirs, side ? &s->ping : &s->pong, name_theirs);
   for (round = side; round < ROUNDS; round += 2)
   {
      for (;;)
      {
         int key;
         if (retro_atomic_load_acquire_int(&s->turn) == round)
            break;
         key = retro_shared_eventcount_prepare_wait(&mine);
         if (retro_atomic_load_acquire_int(&s->turn) == round)
         {
            retro_shared_eventcount_cancel_wait(&mine);
            break;
         }
         retro_shared_eventcount_commit_wait(&mine, key);
      }
      retro_atomic_store_release_int(&s->turn, round + 1);
      retro_shared_eventcount_notify(&theirs);
   }
   retro_shared_eventcount_detach(&mine);
   retro_shared_eventcount_detach(&theirs);
}

#if !defined(__linux__)
static shared_t *g_shared;
static void second_side(void *arg)
{
   (void)arg;
   play(g_shared, 1, "retro_sec_test_pong", "retro_sec_test_ping");
}
#endif

static void test_ping_pong(shared_t *s)
{
   retro_time_t start = cpu_features_get_time_usec();
   char msg[128];
#if defined(__linux__)
   pid_t child = fork();
   int status = 0;
   if (child == 0)
   {
      alarm(60);
      play(s, 1, "retro_sec_test_pong", "retro_sec_test_ping");
      _exit(0);
   }
   alarm(60);
   play(s, 0, "retro_sec_test_ping", "retro_sec_test_pong");
   waitpid(child, &status, 0);
   alarm(0);
   CHECK(WIFEXITED(status) && WEXITSTATUS(status) == 0, "ping-pong: the other process finished");
#else
   sthread_t *thread;
   g_shared = s;
   thread   = sthread_create(second_side, NULL);
   play(s, 0, "retro_sec_test_ping", "retro_sec_test_pong");
   sthread_join(thread);
#endif
   snprintf(msg, sizeof(msg), "ping-pong: %d handoffs, none lost (%.0f ns each)",
         ROUNDS, (double)(cpu_features_get_time_usec() - start) * 1000.0 / ROUNDS);
   CHECK(retro_atomic_load_acquire_int(&s->turn) == ROUNDS, msg);
}

static void test_notify_in_window(shared_t *s)
{
   retro_shared_eventcount_handle_t h;
   retro_time_t start;
   int key;
   retro_shared_eventcount_attach(&h, &s->ping, "retro_sec_test_ping");
   key = retro_shared_eventcount_prepare_wait(&h);
   retro_shared_eventcount_notify(&h);
   start = cpu_features_get_time_usec();
   retro_shared_eventcount_commit_wait(&h, key);
   CHECK(cpu_features_get_time_usec() - start < 100000, "notify in the window: commit returns");
   key = retro_shared_eventcount_prepare_wait(&h);
   retro_shared_eventcount_notify(&h);
   CHECK(retro_shared_eventcount_commit_wait_timeout(&h, key, 5000000),
         "notify in the window: timed commit returns true");
   retro_shared_eventcount_detach(&h);
}

static void test_timeout(shared_t *s)
{
   retro_shared_eventcount_handle_t h;
   retro_time_t start, took;
   bool notified;
   int key;
   retro_shared_eventcount_attach(&h, &s->pong, "retro_sec_test_pong_t");
   key      = retro_shared_eventcount_prepare_wait(&h);
   start    = cpu_features_get_time_usec();
   notified = retro_shared_eventcount_commit_wait_timeout(&h, key, 30000);
   took     = cpu_features_get_time_usec() - start;
   CHECK(!notified, "timeout: false with nothing published");
   CHECK(took >= 29000 && took < 1000000, "timeout: about the time asked for");
   retro_shared_eventcount_detach(&h);
}

int main(void)
{
   shared_t *s;
   retro_shared_eventcount_handle_t probe;
#if defined(__linux__)
   s = (shared_t*)mmap(NULL, sizeof(*s), PROT_READ | PROT_WRITE,
         MAP_SHARED | MAP_ANONYMOUS, -1, 0);
   if (s == MAP_FAILED)
      return 1;
#else
   s = (shared_t*)calloc(1, sizeof(*s));
#endif
   retro_shared_eventcount_init(&s->ping);
   retro_shared_eventcount_init(&s->pong);
   retro_atomic_int_init(&s->turn, 0);

   if (!retro_shared_eventcount_attach(&probe, &s->ping, "retro_sec_test_probe"))
   {
      printf("skip: no wait between processes on this platform\n");
      return 0;
   }
   retro_shared_eventcount_detach(&probe);

   test_notify_in_window(s);
   test_timeout(s);
   test_ping_pong(s);

   if (failures)
   {
      printf("\n%u check(s) failed\n", failures);
      return 1;
   }
   printf("\nall checks passed\n");
   return 0;
}
