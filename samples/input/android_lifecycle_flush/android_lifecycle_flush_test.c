/* android_lifecycle_flush_test.c -- the background flush handshake from
 * input/drivers/android_input.c and frontend/drivers/platform_unix.c,
 * reproduced with a real UI thread and app thread.
 *
 * The protocol: onPause()/onStop() write APP_CMD_PAUSE/APP_CMD_STOP to
 * the command pipe and wait (bounded) for the app thread to store the
 * command as the acknowledged state. The app thread reads it from the
 * input poll, which a core reaches from inside retro_run(), so the save
 * of SRAM, core options and config runs at the top of the next runloop
 * iteration instead - and the acknowledgement is held back until that
 * save has finished. Android removes a swiped-away task by killing the
 * process once onPause()/onStop() has returned, so the claims a
 * regression would break are:
 *
 *   1. When onPause() returns acknowledged, the settings current at the
 *      time it was called are on disk.
 *   2. The save is one-time: the STOP that follows does not write
 *      again; a resume re-arms it.
 *   3. The startup pump, which runs before anything is loaded,
 *      acknowledges at once and leaves the save to the first runloop
 *      iteration.
 *   4. A PAUSE whose wait timed out while the app thread was held
 *      elsewhere still gets its save before the following onStop()
 *      returns.
 *   5. A resume that arrives after such a timeout stays in effect.
 *
 * Two sabotage modes reproduce the bugs the lanes exist for and are
 * asserted to be caught: acknowledging before the save (the previous
 * behaviour) and holding the acknowledgement in the startup pump.
 *
 * The acknowledgement and its wait are the driver's own, from
 * frontend/drivers/android_lifecycle.h; the command handling around
 * them is reproduced here, because the driver only builds against the
 * NDK. */

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#include <boolean.h>

#include "../../../frontend/drivers/android_lifecycle.h"

/* APP_CMD_*, as far as the handshake cares. */
enum
{
   CMD_START = 0,
   CMD_RESUME,
   CMD_PAUSE,
   CMD_STOP,
   CMD_DESTROY
};

#define FLUSH_MS          30
#define ACK_TIMEOUT_MS  3000

/* Sabotage hooks: 0 = shipping behaviour. */
static int sab_ack_before_flush = 0;
static int sab_hold_in_pump     = 0;

/* struct android_app, as far as the handshake cares. Everything the two
 * threads share sits behind mtx. */
static pthread_mutex_t mtx  = PTHREAD_MUTEX_INITIALIZER;
/* The acknowledged activity state and the exit flag: the driver's own
 * frontend/drivers/android_lifecycle.h. */
static android_lifecycle_t lc;
static int  msgread, msgwrite;

/* Test controls and observations, also behind mtx. */
static int  settings_gen;   /* bumped by the "user" while foreground */
static int  disk_gen;       /* what the last completed save wrote */
static int  flushes;
static int  pump_mode;      /* app thread is in android_run_events() */
static int  stall_ms;       /* app thread held outside the looper */

/* App-thread-only state: android_input.c's statics. */
static bool state_flushed;
static bool state_flush_pending;
static int  state_ack_cmd;

static int  failures;
static int  quiet;          /* sabotage runs: failures are expected */

static void sleep_ms(int ms)
{
   struct timespec ts;
   ts.tv_sec  = ms / 1000;
   ts.tv_nsec = (long)(ms % 1000) * 1000000L;
   while (nanosleep(&ts, &ts) != 0 && errno == EINTR)
      ;
}

/* android_input_release_state_ack() */
static void release_state_ack(void)
{
   if (state_ack_cmd < 0)
      return;
   android_lifecycle_set_state(&lc, state_ack_cmd);
   state_ack_cmd = -1;
}

/* android_input_flush_state(): the write takes a while, and what it
 * writes is the settings as they were when it started. */
static void flush_state(void)
{
   int gen;
   pthread_mutex_lock(&mtx);
   gen = settings_gen;
   pthread_mutex_unlock(&mtx);

   sleep_ms(FLUSH_MS);

   pthread_mutex_lock(&mtx);
   disk_gen = gen;
   flushes++;
   pthread_mutex_unlock(&mtx);
}

/* android_input_flush_pending_state() */
static void flush_pending_state(void)
{
   if (!state_flush_pending)
      return;
   state_flush_pending = false;

   if (!state_flushed)
   {
      state_flushed = true;
      flush_state();
   }

   release_state_ack();
}

/* android_input_poll_main_cmd(), lifecycle cases */
static void poll_main_cmd(int cmd)
{
   switch (cmd)
   {
      case CMD_RESUME:
      case CMD_START:
      case CMD_PAUSE:
      {
         bool hold_ack = (cmd == CMD_PAUSE) && !state_flushed
            && !sab_ack_before_flush;

         if (!hold_ack)
            android_lifecycle_set_state(&lc, cmd);

         if (cmd == CMD_PAUSE)
         {
            state_flush_pending = true;
            if (hold_ack)
               state_ack_cmd    = CMD_PAUSE;
         }
         else
         {
            state_flush_pending = false;
            state_flushed       = false;
            state_ack_cmd       = -1;
         }
         break;
      }
      case CMD_STOP:
         if (!state_flushed && !sab_ack_before_flush)
         {
            state_flush_pending = true;
            state_ack_cmd       = CMD_STOP;
         }
         else
            android_lifecycle_set_state(&lc, cmd);
         break;
      default:
         break;
   }
}

/* rarch_main(): the startup pump while pump_mode is set, the runloop
 * after. A runloop iteration flushes first, then its poll drains every
 * command that is readable, as android_input_poll() does; the pump
 * handles one command per call, as android_run_events() does. */
static void *app_thread(void *arg)
{
   bool done = false;
   (void)arg;

   while (!done)
   {
      struct pollfd pfd;
      int timeout = 5;
      int pump, stall;

      pthread_mutex_lock(&mtx);
      pump     = pump_mode;
      stall    = stall_ms;
      stall_ms = 0;
      pthread_mutex_unlock(&mtx);

      if (stall)
         sleep_ms(stall);

      if (!pump)
         flush_pending_state();

      pfd.fd     = msgread;
      pfd.events = POLLIN;
      while (poll(&pfd, 1, timeout) > 0)
      {
         char cmd;
         timeout = 0;
         if (read(msgread, &cmd, 1) != 1)
            break;
         if (cmd == CMD_DESTROY)
         {
            done = true;
            break;
         }
         poll_main_cmd(cmd);
         if (pump)
         {
            if (!sab_hold_in_pump)
               release_state_ack();
            break;
         }
      }
   }

   android_lifecycle_set_flags(&lc, ANDROID_LC_EXITED);
   return NULL;
}

/* android_app_set_activity_state(): true when acknowledged. */
static bool set_activity_state(int cmd, int timeout_ms)
{
   char c = (char)cmd;
   if (write(msgwrite, &c, 1) != 1)
      return false;
   return android_lifecycle_wait(&lc, ANDROID_LC_UNTIL_STATE, cmd, true,
         (int64_t)timeout_ms * 1000);
}

static int read_locked(const int *v)
{
   int r;
   pthread_mutex_lock(&mtx);
   r = *v;
   pthread_mutex_unlock(&mtx);
   return r;
}

static void write_locked(int *v, int val)
{
   pthread_mutex_lock(&mtx);
   *v = val;
   pthread_mutex_unlock(&mtx);
}

static void expect(const char *what, int got, int want)
{
   if (got == want)
      return;
   if (!quiet)
      printf("[fail] %s: got %d, expected %d\n", what, got, want);
   failures++;
}

static int start_app(pthread_t *t, int pump)
{
   int fds[2];

   if (pipe(fds) != 0)
      return 0;
   msgread             = fds[0];
   msgwrite            = fds[1];
   if (!android_lifecycle_init(&lc))
      return 0;
   settings_gen        = 0;
   disk_gen            = 0;
   flushes             = 0;
   pump_mode           = pump;
   stall_ms            = 0;
   state_flushed       = false;
   state_flush_pending = false;
   state_ack_cmd       = -1;
   return pthread_create(t, NULL, app_thread, NULL) == 0;
}

static void stop_app(pthread_t t)
{
   char c = CMD_DESTROY;
   if (write(msgwrite, &c, 1) != 1)
      failures++;
   pthread_join(t, NULL);
   android_lifecycle_free(&lc);
   close(msgread);
   close(msgwrite);
}

/* 1 + 2: swipe from the foreground. The settings on disk when onPause()
 * returns are the ones that were live when it was called. */
static int lane_swipe(void)
{
   pthread_t t;
   int before = failures;

   if (!start_app(&t, 0))
      return ++failures - before;

   expect("start acked",  set_activity_state(CMD_START,  ACK_TIMEOUT_MS), 1);
   expect("resume acked", set_activity_state(CMD_RESUME, ACK_TIMEOUT_MS), 1);
   write_locked(&settings_gen, 1);
   expect("pause acked",  set_activity_state(CMD_PAUSE,  ACK_TIMEOUT_MS), 1);
   /* The process may be killed from here on. */
   expect("on disk when onPause() returns", read_locked(&disk_gen), 1);
   expect("stop acked",   set_activity_state(CMD_STOP,   ACK_TIMEOUT_MS), 1);
   expect("stop does not write again", read_locked(&flushes), 1);

   expect("start acked",  set_activity_state(CMD_START,  ACK_TIMEOUT_MS), 1);
   expect("resume acked", set_activity_state(CMD_RESUME, ACK_TIMEOUT_MS), 1);
   write_locked(&settings_gen, 2);
   expect("pause acked",  set_activity_state(CMD_PAUSE,  ACK_TIMEOUT_MS), 1);
   expect("resume re-arms the save", read_locked(&disk_gen), 2);
   expect("one write per background", read_locked(&flushes), 2);

   stop_app(t);
   return failures - before;
}

/* 3: the startup pump acknowledges at once and defers the save. */
static int lane_pump(void)
{
   pthread_t t;
   int before = failures;

   if (!start_app(&t, 1))
      return ++failures - before;

   expect("pump: pause acked", set_activity_state(CMD_PAUSE, 500), 1);
   expect("pump: nothing written", read_locked(&flushes), 0);
   expect("pump: stop acked",  set_activity_state(CMD_STOP,  500), 1);

   write_locked(&pump_mode, 0);
   sleep_ms(FLUSH_MS * 4);
   expect("first iteration saves", read_locked(&flushes), 1);

   stop_app(t);
   return failures - before;
}

/* 4: a PAUSE that timed out still gets its save before onStop()
 * returns. */
static int lane_stop_backstop(void)
{
   pthread_t t;
   int before = failures;

   if (!start_app(&t, 0))
      return ++failures - before;

   expect("resume acked", set_activity_state(CMD_RESUME, ACK_TIMEOUT_MS), 1);
   write_locked(&settings_gen, 3);
   write_locked(&stall_ms, 400);
   sleep_ms(20);
   expect("pause times out while held",
         set_activity_state(CMD_PAUSE, 100), 0);
   expect("stop acked", set_activity_state(CMD_STOP, ACK_TIMEOUT_MS), 1);
   expect("on disk when onStop() returns", read_locked(&disk_gen), 3);

   stop_app(t);
   return failures - before;
}

/* 5: a resume after a timed-out PAUSE stays in effect. */
static int lane_supersede(void)
{
   pthread_t t;
   int before = failures;

   if (!start_app(&t, 0))
      return ++failures - before;

   expect("resume acked", set_activity_state(CMD_RESUME, ACK_TIMEOUT_MS), 1);
   write_locked(&stall_ms, 400);
   sleep_ms(20);
   expect("pause times out while held",
         set_activity_state(CMD_PAUSE, 100), 0);
   expect("resume acked", set_activity_state(CMD_RESUME, ACK_TIMEOUT_MS), 1);
   sleep_ms(FLUSH_MS * 4);
   expect("state stays resumed", android_lifecycle_state(&lc), CMD_RESUME);

   stop_app(t);
   return failures - before;
}

int main(void)
{
   int shipping;

   shipping  = lane_swipe();
   shipping += lane_pump();
   shipping += lane_stop_backstop();
   shipping += lane_supersede();
   if (!shipping)
      printf("[pass] shipping handshake\n");

   /* Sabotage runs are expected to fail; only a run that does not is a
    * failure of the harness. */
   quiet                = 1;
   sab_ack_before_flush = 1;
   if (lane_swipe() == 0)
   {
      printf("[fail] acknowledging before the save was not caught\n");
      shipping++;
   }
   else
      printf("[pass] acknowledging before the save is caught\n");
   sab_ack_before_flush = 0;

   sab_hold_in_pump = 1;
   if (lane_pump() == 0)
   {
      printf("[fail] holding the acknowledgement in the pump was not caught\n");
      shipping++;
   }
   else
      printf("[pass] holding the acknowledgement in the pump is caught\n");
   sab_hold_in_pump = 0;
   quiet            = 0;

   if (shipping)
   {
      printf("%d failure(s)\n", shipping);
      return 1;
   }
   printf("all lanes passed\n");
   return 0;
}
