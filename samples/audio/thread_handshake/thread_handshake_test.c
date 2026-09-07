/* The audio thread wrapper's handshakes with the main thread.
 *
 * The wrapped thread parks in one of two waits: the one before its
 * first start, and the one inside its loop when it is stopped. The
 * main thread's block() waits for stopped_ack whichever wait the
 * thread is in, so both have to give it. A start() that clears the
 * stop flag and signals, followed by a stop() that sets it again
 * before the thread has re-checked, leaves the thread in the first
 * wait; block() then waits forever if that wait does not acknowledge.
 *
 * The window is the gap between unblock's signal and the thread's
 * re-check, so a tight start/stop loop hits it within a few hundred
 * iterations. The wrapped driver is scripted so a device that stops
 * returning is producible too: a stop must still complete once the
 * write does. A watchdog turns a wait that never completes into an
 * abort rather than a hung suite. Links the shipping wrapper. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "../../../audio/audio_driver.h"
#include "../../../audio/audio_thread_wrapper.h"

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: ", __FILE__, __LINE__); \
         printf(__VA_ARGS__); \
         printf("\n"); \
         failures++; \
      } \
   } while (0)

/* Counted by the RARCH_WARN stub in stubs_retroarch.c: the wrapper's
 * only report that a handshake has run long. */
extern retro_atomic_int_t warn_count;

static retro_atomic_int_t stage = RETRO_ATOMIC_INT_INITIALIZER(0);
#define STAGE(v) retro_atomic_store_release_int(&stage, (v))

static void *watchdog(void *arg)
{
   int last = retro_atomic_load_acquire_int(&stage);
   int cur;
   (void)arg;
   for (;;)
   {
      sleep(20);
      cur = retro_atomic_load_acquire_int(&stage);
      if (cur == last && cur >= 0)
      {
         fprintf(stderr, "WATCHDOG: stage %d never completed\n", cur);
         abort();
      }
      last = cur;
      if (cur < 0)
         return NULL;
   }
}

/* --- scripted wrapped driver ----------------------------------------- */

/* How long init() and write() stall, in microseconds. Set before the
 * call that should see it. */
static retro_atomic_int_t stall_init_us  = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t stall_write_us = RETRO_ATOMIC_INT_INITIALIZER(0);
/* While set, wait_writable() reports no space is coming (0), as a
 * driver does for a stream that is not running. */
static retro_atomic_int_t no_space       = RETRO_ATOMIC_INT_INITIALIZER(0);
static retro_atomic_int_t n_wait_writable = RETRO_ATOMIC_INT_INITIALIZER(0);

static void sleep_us(int us)
{
   struct timespec ts;
   if (us <= 0)
      return;
   ts.tv_sec  = us / 1000000;
   ts.tv_nsec = (long)(us % 1000000) * 1000L;
   nanosleep(&ts, NULL);
}

static void *fake_init(const char *device, unsigned rate, unsigned latency,
      unsigned block_frames, unsigned *new_rate)
{
   static int handle = 1;
   (void)device; (void)latency; (void)block_frames;
   sleep_us(retro_atomic_load_acquire_int(&stall_init_us));
   if (new_rate)
      *new_rate = rate;
   return &handle;
}

static ssize_t fake_write(void *data, const void *buf, size_t size)
{
   (void)data; (void)buf;
   /* Stands in for a device that has stopped draining: the audio
    * thread is inside this call and cannot reach the loop to
    * acknowledge a stop. */
   sleep_us(retro_atomic_load_acquire_int(&stall_write_us));
   return (ssize_t)size;
}

static bool fake_stop(void *data) { (void)data; return true; }
static bool fake_start(void *data, bool is_shutdown)
{
   (void)data; (void)is_shutdown;
   return true;
}
static bool fake_alive(void *data) { (void)data; return true; }
static void fake_set_nonblock(void *data, bool state)
{
   (void)data; (void)state;
}
static void fake_free(void *data) { (void)data; }
static bool fake_use_float(void *data) { (void)data; return true; }
static size_t fake_write_avail(void *data) { (void)data; return 4096; }
static size_t fake_wait_writable(void *data, size_t len)
{
   (void)data;
   retro_atomic_fetch_add_int(&n_wait_writable, 1);
   if (retro_atomic_load_acquire_int(&no_space))
      return 0;
   return len;
}
static size_t fake_buffer_size(void *data) { (void)data; return 8192; }

static audio_driver_t fake_driver = {
   fake_init,
   fake_write,
   fake_stop,
   fake_start,
   fake_alive,
   fake_set_nonblock,
   fake_free,
   fake_use_float,
   "fake",
   NULL,
   NULL,
   fake_write_avail,
   fake_buffer_size,
   NULL,
   fake_wait_writable
};

/* The wrapper's loop calls audio_driver_callback(), which in the
 * frontend feeds the device; here it just does one write, so the
 * thread spends its time exactly where a real one would. */
/* Set by main to the wrapper's driver table once init has returned,
 * so the loop can go through the wrapper's own wait_writable() the
 * way the threaded pipeline does. */
static const audio_driver_t *wrapper_drv = NULL;
static void                 *wrapper_ctx = NULL;

bool audio_driver_callback(void)
{
   char buf[64];
   memset(buf, 0, sizeof(buf));
   if (wrapper_drv && wrapper_drv->wait_writable)
   {
      if (!wrapper_drv->wait_writable(wrapper_ctx, sizeof(buf)))
      {
         sleep_us(1000);
         return true;
      }
   }
   fake_write(NULL, buf, sizeof(buf));
   return true;
}

static double now_ms(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
}

int main(void)
{
   pthread_t dog;
   const audio_driver_t *drv = NULL;
   void *data                = NULL;
   unsigned new_rate         = 0;
   unsigned i;
   double t0, t1;

   pthread_create(&dog, NULL, watchdog, NULL);

   /* 1. Init, then a stop before any start: the thread is in its
    *    initial wait and block() has to come back from it. */
   STAGE(1);
   CHECK(audio_init_thread(&drv, &data, "fake", 48000, &new_rate, 64,
            512, false, false, &fake_driver),
         "init against a responsive device failed");
   wrapper_drv = drv;
   wrapper_ctx = data;
   CHECK(drv && drv->stop(data), "stop before first start failed");
   CHECK(drv && drv->start(data, false), "first start failed");
   CHECK(retro_atomic_load_acquire_int(&warn_count) == 0,
         "a prompt init or stop was reported as running long");

   /* 2. start()/stop() back to back, many times: the stop lands in
    *    the initial wait or the loop wait depending on scheduling,
    *    and must be acknowledged from either. */
   STAGE(2);
   for (i = 0; i < 2000; i++)
   {
      if (!drv->stop(data) || !drv->start(data, false))
      {
         CHECK(false, "stop/start round-trip %u failed", i);
         break;
      }
      if ((i & 255) == 255)
         STAGE(2 + (int)(i >> 8));
   }

   CHECK(retro_atomic_load_acquire_int(&warn_count) == 0,
         "prompt round-trips were reported as running long");

   /* 3. The device stops returning from writes for longer than the
    *    report threshold. The stop must still complete once the write
    *    returns - not before, since callers then treat the thread as
    *    parked - and it must have been reported once meanwhile. */
   STAGE(20);
   retro_atomic_store_release_int(&stall_write_us, 3 * 1000 * 1000);
   /* Let the thread get into the stalled write before asking it to
    * stop; a stop that arrives first is acknowledged promptly, which
    * is correct but is not the case under test. */
   sleep_us(300 * 1000);
   t0 = now_ms();
   CHECK(drv && drv->stop(data), "stop failed against a stalled device");
   t1 = now_ms();
   CHECK(t1 - t0 >= 1000.0,
         "stop returned before the device did (%f ms)", t1 - t0);
   CHECK(t1 - t0 < 15000.0, "stop against a stalled device took %f ms",
         t1 - t0);
   CHECK(retro_atomic_load_acquire_int(&warn_count) == 1,
         "a stop that ran %f ms was reported %d times, expected once",
         t1 - t0, retro_atomic_load_acquire_int(&warn_count));
   retro_atomic_store_release_int(&stall_write_us, 0);
   CHECK(drv && drv->start(data, false), "start after a stalled stop failed");

   /* 4. The device reports no space is coming, pass after pass, as a
    *    stream that is not running does. That is not the device gone:
    *    the thread stays, alive() still answers, and a stop is still
    *    acknowledged. Then space returns and writes resume. */
   STAGE(21);
   retro_atomic_store_release_int(&n_wait_writable, 0);
   retro_atomic_store_release_int(&no_space, 1);
   sleep_us(200 * 1000);
   CHECK(retro_atomic_load_acquire_int(&n_wait_writable) > 5,
         "the loop did not keep asking the device for space");
   CHECK(drv && drv->alive(data),
         "a device with no space was reported as gone");
   CHECK(drv && drv->stop(data), "stop failed while the device had no space");
   CHECK(drv && drv->start(data, false), "start failed after a no-space stop");
   retro_atomic_store_release_int(&no_space, 0);
   sleep_us(50 * 1000);
   CHECK(drv && drv->stop(data), "stop failed after space returned");
   CHECK(drv && drv->start(data, false), "start failed after space returned");

   /* 5. Teardown joins the thread. */
   STAGE(22);
   if (drv)
      drv->free(data);
   drv  = NULL;
   data = NULL;
   wrapper_drv = NULL;
   wrapper_ctx = NULL;

   /* 5. A device slow to open: init still completes, reported once. */
   STAGE(23);
   retro_atomic_store_release_int(&warn_count, 0);
   retro_atomic_store_release_int(&stall_init_us, 3 * 1000 * 1000);
   CHECK(audio_init_thread(&drv, &data, "fake", 48000, &new_rate, 64,
            512, false, false, &fake_driver),
         "init against a slow device failed");
   CHECK(retro_atomic_load_acquire_int(&warn_count) == 1,
         "a slow init was reported %d times, expected once",
         retro_atomic_load_acquire_int(&warn_count));
   retro_atomic_store_release_int(&stall_init_us, 0);
   if (drv)
      drv->free(data);

   STAGE(-1);
   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("audio thread handshakes: every stop acknowledged, stalls reported once\n");
   return 0;
}
