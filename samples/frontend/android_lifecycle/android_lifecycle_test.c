/* frontend/drivers/android_lifecycle.h: what the NativeActivity
 * callbacks on the Java UI thread wait for from the app thread, and the
 * window a third thread holds across a call.
 *
 * Run with a real UI thread, app thread and holder thread; commands go
 * through a pipe, as on the device. What is asserted:
 *  - a ticketed wait returns only once the app thread has finished
 *    that command, never on an earlier one;
 *  - while a thread holds the window, retiring it waits, and a window
 *    retired is never seen by a holder afterwards (the test poisons
 *    each retired window and the holder checks it never reads poison);
 *  - an acknowledgement that never comes ends the state wait at its
 *    bound, and one that does wakes it at once;
 *  - once the app thread has left, every wait that stops on exit
 *    returns, and the create wait ends on either flag.
 * Built under TSan by its CI lane; halt_on_error makes any report a
 * failure. A watchdog turns a lost wake into a failure, not a hang. */

#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "../../../frontend/drivers/android_lifecycle.h"

#define CMD_INIT_WINDOW 1
#define CMD_TERM_WINDOW 2
#define CMD_STATE       3
#define CMD_QUIT        4

#define WINDOW_LIVE   0x11223344
#define WINDOW_POISON 0x0DEADBAD
#define CHANGES       400

typedef struct { void *arg; int cmd; } msg_t;

typedef struct { retro_atomic_int_t magic; } fake_window_t;

static android_lifecycle_t lc;
static int fds[2];
static retro_atomic_int_t processed;   /* commands the app thread finished */
static retro_atomic_int_t stop_holder;
static unsigned failures;

#define CHECK(cond, ...) do { if (!(cond)) { printf("FAIL line %d: ", __LINE__); \
   printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static void post(int cmd, void *arg)
{
   msg_t m;
   memset(&m, 0, sizeof(m));
   m.cmd = cmd;
   m.arg = arg;
   if (write(fds[1], &m, sizeof(m)) != (ssize_t)sizeof(m))
      abort();
}

static void sleep_us(unsigned us)
{
   struct timespec d;
   d.tv_sec  = us / 1000000;
   d.tv_nsec = (long)(us % 1000000) * 1000;
   nanosleep(&d, NULL);
}

/* The app thread: what android_input_poll_main_cmd() does. */
static void *app_thread(void *data)
{
   unsigned n = 0;
   (void)data;
   for (;;)
   {
      msg_t m;
      if (read(fds[0], &m, sizeof(m)) != (ssize_t)sizeof(m))
         break;
      if ((++n & 7) == 0)
         sleep_us(200);   /* a frame in between */
      switch (m.cmd)
      {
         case CMD_INIT_WINDOW:
            retro_atomic_store_release_int(
                  &((fake_window_t*)m.arg)->magic, WINDOW_LIVE);
            android_lifecycle_window_set(&lc, m.arg);
            retro_atomic_fetch_add_int(&processed, 1);
            android_lifecycle_done(&lc);
            break;
         case CMD_TERM_WINDOW:
         {
            fake_window_t *w = (fake_window_t*)android_lifecycle_window(&lc);
            android_lifecycle_window_retire(&lc);
            /* the framework may now reuse it: poison it */
            if (w)
               retro_atomic_store_release_int(&w->magic, WINDOW_POISON);
            retro_atomic_fetch_add_int(&processed, 1);
            android_lifecycle_done(&lc);
            break;
         }
         case CMD_STATE:
            android_lifecycle_set_state(&lc, (int)(size_t)m.arg);
            break;
         case CMD_QUIT:
            android_lifecycle_set_flags(&lc, ANDROID_LC_EXITED);
            return NULL;
      }
   }
   return NULL;
}

/* The video context: holds the window across a call. */
static void *holder_thread(void *data)
{
   (void)data;
   while (!retro_atomic_load_acquire_int(&stop_holder))
   {
      fake_window_t *w = (fake_window_t*)android_lifecycle_window_acquire(&lc);
      if (w)
      {
         int i;
         for (i = 0; i < 50; i++)
            if (retro_atomic_load_acquire_int(&w->magic) != WINDOW_LIVE)
            {
               printf("FAIL: a held window was handed back under its holder\n");
               failures++;
               break;
            }
         android_lifecycle_window_release(&lc);
      }
   }
   return NULL;
}

static void watchdog(int sig)
{
   (void)sig;
   static const char m[] = "FAIL: a wait never woke (watchdog)\n";
   if (write(1, m, sizeof(m) - 1) < 0) { }
   _exit(1);
}

int main(void)
{
   pthread_t app, holder;
   fake_window_t *windows;
   unsigned i;
   retro_time_t t0;

   signal(SIGALRM, watchdog);
   alarm(120);

   if (pipe(fds) || !android_lifecycle_init(&lc))
      return 1;
   windows = (fake_window_t*)calloc(CHANGES, sizeof(*windows));
   pthread_create(&app, NULL, app_thread, NULL);
   pthread_create(&holder, NULL, holder_thread, NULL);

   /* window changes, each a TERM of the last and an INIT of the next,
    * waited on the ticket of the last - android_app_set_window() */
   for (i = 0; i < CHANGES; i++)
   {
      unsigned ticket = android_lifecycle_last_ticket(&lc);
      if (i)
      {
         post(CMD_TERM_WINDOW, NULL);
         ticket = android_lifecycle_ticket(&lc);
      }
      post(CMD_INIT_WINDOW, &windows[i]);
      ticket = android_lifecycle_ticket(&lc);
      android_lifecycle_wait(&lc, ANDROID_LC_UNTIL_DONE, (int)ticket, true, -1);
      CHECK(retro_atomic_load_acquire_int(&processed) >= (int)ticket,
            "the wait for ticket %u returned with %d done", ticket,
            retro_atomic_load_acquire_int(&processed));
   }
   post(CMD_TERM_WINDOW, NULL);
   android_lifecycle_wait(&lc, ANDROID_LC_UNTIL_DONE,
         (int)android_lifecycle_ticket(&lc), true, -1);
   CHECK(android_lifecycle_window_acquire(&lc) == NULL,
         "a retired window can still be acquired");
   retro_atomic_store_release_int(&stop_holder, 1);
   pthread_join(holder, NULL);
   printf("ok    %u window changes against a thread holding the window\n",
         CHANGES);

   /* an acknowledgement that never comes: the bound ends the wait */
   t0 = cpu_features_get_time_usec();
   CHECK(!android_lifecycle_wait(&lc, ANDROID_LC_UNTIL_STATE, 7, true,
            100000), "a state never acknowledged was reported acknowledged");
   CHECK(cpu_features_get_time_usec() - t0 >= 90000,
         "the bounded wait returned before its bound");
   /* one that does */
   post(CMD_STATE, (void*)(size_t)5);
   t0 = cpu_features_get_time_usec();
   CHECK(android_lifecycle_wait(&lc, ANDROID_LC_UNTIL_STATE, 5, true,
            5000000), "an acknowledged state timed out");
   /* woken by the acknowledgement, not by the bound */
   CHECK(cpu_features_get_time_usec() - t0 < 1000000,
         "an acknowledged state waited out its bound: no wake");
   printf("ok    the state wait: bounded when unanswered, true when answered\n");

   /* the app thread leaves: every wait that stops on exit returns */
   post(CMD_QUIT, NULL);
   android_lifecycle_wait(&lc, ANDROID_LC_UNTIL_FLAGS,
         ANDROID_LC_RUNNING | ANDROID_LC_EXITED, false, -1);
   pthread_join(app, NULL);
   CHECK(!android_lifecycle_wait(&lc, ANDROID_LC_UNTIL_DONE,
            (int)android_lifecycle_ticket(&lc), true, -1),
         "a ticket nobody can answer was reported done");
   CHECK(!android_lifecycle_wait(&lc, ANDROID_LC_UNTIL_STATE, 9, true, -1),
         "a state nobody can acknowledge was reported acknowledged");
   printf("ok    after the app thread left, the waits return\n");

   /* the permission flags, from the UI thread */
   android_lifecycle_set_flags(&lc, ANDROID_LC_PERM_RESOLVED);
   CHECK((android_lifecycle_flags(&lc) & ANDROID_LC_PERM_RESOLVED)
         && !(android_lifecycle_flags(&lc) & ANDROID_LC_PERM_GRANTED),
         "permission flags");

   android_lifecycle_free(&lc);
   close(fds[0]);
   close(fds[1]);
   free(windows);
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] android_lifecycle_test\n");
   return 0;
}
