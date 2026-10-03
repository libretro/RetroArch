/* output_store_test.c -- what a core asks of a rumble motor or an LED,
 * between the core's call and the frontend's write to the device.
 *
 * A core's rumble and LED calls used to be driver writes on the core's
 * own stack, as many a frame as the core called. They now post into a
 * store, and the frontend takes the stored values once the core has
 * run: one write per output that was set, with the frame's last value.
 *
 * The claims:
 *
 *   1. A post writes nothing. Only a take writes.
 *   2. However many times an output is set in a frame, it is written
 *      once, with the last value.
 *   3. An output set to the value it already had is still written
 *      once that frame: drivers whose effects run out rely on a core
 *      repeating itself.
 *   4. An output nobody set is not written, and a take with nothing
 *      posted writes nothing and finds nothing pending.
 *   5. Outputs are written lowest slot first, each with its own value.
 *   6. Dropping forgets what was posted: the next take writes nothing.
 *   7. A slot outside the store is ignored.
 *   8. With threads posting while the frame loop takes, no post is
 *      lost: once the posters stop, every output ends on the last
 *      value posted to it, and nothing is written that was not posted.
 *
 * The store is the frontend's own, from input_output_store.h. A
 * sabotage mode drops the taker's wake-up, the way a stored stop that
 * nobody wrote would leave a motor running, and is asserted to be
 * caught.
 *
 * Paced with short sleeps so a single-processor machine interleaves
 * the threads. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#if defined(_WIN32)
#include <windows.h>
static void sleep_us(unsigned us) { Sleep(us / 1000u + 1u); }
#else
#include <unistd.h>
static void sleep_us(unsigned us) { usleep(us); }
#endif

#include "input_output_store.h"

static unsigned failures = 0;
static bool     quiet    = false;

#define CHECK(cond, msg) \
   do { \
      if (!(cond)) \
      { \
         if (!quiet) \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, msg); \
         failures++; \
      } \
   } while (0)

/* The device: what was written to each output, and in what order. */
struct device
{
   int      value[OUTPUT_STORE_SLOTS];
   unsigned writes[OUTPUT_STORE_SLOTS];
   unsigned order[OUTPUT_STORE_SLOTS * 4];
   unsigned total;
};

static void device_write(unsigned slot, int value, void *userdata)
{
   struct device *d = (struct device*)userdata;
   d->value[slot]   = value;
   d->writes[slot]++;
   if (d->total < OUTPUT_STORE_SLOTS * 4)
      d->order[d->total] = slot;
   d->total++;
}

/* 1 to 7 */
static void lane_semantics(void)
{
   static output_store_t st;
   struct device d;
   unsigned had = failures;

   memset(&st, 0, sizeof(st));
   memset(&d, 0, sizeof(d));

   /* 4 */
   CHECK(!output_store_pending(&st), "an empty store has something pending");
   CHECK(output_store_take(&st, device_write, &d) == 0 && d.total == 0,
         "a take from an empty store wrote something");

   /* 1, 2 */
   output_store_post(&st, 3, 100);
   output_store_post(&st, 3, 200);
   output_store_post(&st, 3, 300);
   CHECK(d.total == 0, "a post wrote to the device");
   CHECK(output_store_pending(&st), "a post left nothing pending");
   CHECK(output_store_take(&st, device_write, &d) == 1,
         "three posts to one output were not one write");
   CHECK(d.writes[3] == 1 && d.value[3] == 300,
         "the write was not the frame's last value");
   CHECK(!output_store_pending(&st), "a take left something pending");
   CHECK(output_store_take(&st, device_write, &d) == 0 && d.total == 1,
         "a second take wrote the same post again");

   /* 3 */
   output_store_post(&st, 3, 300);
   CHECK(output_store_take(&st, device_write, &d) == 1 && d.writes[3] == 2,
         "an output set to the value it already had was not written");

   /* 5 */
   memset(&d, 0, sizeof(d));
   output_store_post(&st, 9, 90);
   output_store_post(&st, 0, 10);
   output_store_post(&st, 31, 310);
   output_store_post(&st, 4, 0);
   CHECK(output_store_take(&st, device_write, &d) == 4, "four outputs were not four writes");
   CHECK(   d.order[0] == 0 && d.order[1] == 4
         && d.order[2] == 9 && d.order[3] == 31,
         "outputs were not written lowest slot first");
   CHECK(   d.value[0] == 10 && d.value[4] == 0
         && d.value[9] == 90 && d.value[31] == 310,
         "an output was written with another's value");
   CHECK(d.writes[3] == 0, "an output nobody set this frame was written");

   /* 6 */
   memset(&d, 0, sizeof(d));
   output_store_post(&st, 2, 65535);
   output_store_drop(&st);
   CHECK(!output_store_pending(&st), "a drop left something pending");
   CHECK(output_store_take(&st, device_write, &d) == 0 && d.total == 0,
         "a dropped post was written");
   output_store_post(&st, 2, 7);
   CHECK(output_store_take(&st, device_write, &d) == 1 && d.value[2] == 7,
         "a post after a drop was not written");

   /* 7 */
   memset(&d, 0, sizeof(d));
   output_store_post(&st, OUTPUT_STORE_SLOTS, 1);
   output_store_post(&st, 1000, 1);
   CHECK(output_store_take(&st, device_write, &d) == 0 && d.total == 0,
         "a slot outside the store was written");

   if (failures == had && !quiet)
      fprintf(stderr, "[pass] posts are stored, takes write once with the last value\n");
}

/* 8 */
#define N_POSTERS 3
#define N_POSTS   20000

static output_store_t     g_st;
static retro_atomic_int_t g_done;
static bool               sabotage_miss_wakeup;

struct poster
{
   unsigned slot_base; /* this thread's two slots: base and base + 1 */
   int      last[2];
};

static void poster_thread(void *arg)
{
   struct poster *p = (struct poster*)arg;
   unsigned i;

   for (i = 1; i <= N_POSTS; i++)
   {
      /* a core's frame: both motors, the second set twice */
      int v = (int)(p->slot_base * 100000 + i);
      output_store_post(&g_st, p->slot_base, v);
      output_store_post(&g_st, p->slot_base + 1, -v);
      output_store_post(&g_st, p->slot_base + 1, v);
      p->last[0] = v;
      p->last[1] = v;
      if ((i & 255) == 0)
         sleep_us(20);
   }
   retro_atomic_fetch_add_int(&g_done, 1);
}

static void lane_threads(void)
{
   unsigned had = failures;
   unsigned i;
   struct device d;
   sthread_t *threads[N_POSTERS];
   struct poster post[N_POSTERS];
   bool foreign = false;

   memset(&g_st, 0, sizeof(g_st));
   memset(&d, 0, sizeof(d));
   retro_atomic_store_release_int(&g_done, 0);

   for (i = 0; i < N_POSTERS; i++)
   {
      post[i].slot_base = i * 2;
      post[i].last[0]   = 0;
      post[i].last[1]   = 0;
      threads[i]        = sthread_create(poster_thread, &post[i]);
   }

   /* the frame loop */
   for (;;)
   {
      bool done = retro_atomic_load_acquire_int(&g_done) == N_POSTERS;

      if (sabotage_miss_wakeup && output_store_pending(&g_st))
      {
         /* the taker notes the posts as seen, then looks at nothing */
         g_st.seen = retro_atomic_load_acquire_int(&g_st.posts);
      }
      else
         output_store_take(&g_st, device_write, &d);

      if (done)
         break;
      sleep_us(30);
   }

   for (i = 0; i < N_POSTERS; i++)
      sthread_join(threads[i]);

   /* the frame after the last post */
   output_store_take(&g_st, device_write, &d);

   for (i = 0; i < N_POSTERS; i++)
   {
      CHECK(d.value[i * 2]     == post[i].last[0],
            "an output did not end on the last value posted to it");
      CHECK(d.value[i * 2 + 1] == post[i].last[1],
            "an output set twice in a frame did not end on its last value");
   }
   for (i = N_POSTERS * 2; i < OUTPUT_STORE_SLOTS; i++)
      if (d.writes[i])
         foreign = true;
   CHECK(!foreign, "an output nobody posted to was written");
   CHECK(!output_store_pending(&g_st), "something is pending after the last take");

   if (failures == had && !quiet)
      fprintf(stderr, "[pass] %u threads, %u frames each: every output ends on"
            " its last value (%u writes)\n",
            (unsigned)N_POSTERS, (unsigned)N_POSTS, d.total);
}

int main(void)
{
   lane_semantics();
   lane_threads();

   if (failures)
   {
      fprintf(stderr, "FAIL output_store_test: %u failures\n", failures);
      return 1;
   }

   /* A take that sees the posts and writes nothing has to be caught. */
   quiet                = true;
   sabotage_miss_wakeup = true;
   lane_threads();
   quiet                = false;
   sabotage_miss_wakeup = false;

   if (!failures)
   {
      fprintf(stderr, "FAIL output_store_test: posts that were never"
            " written went unnoticed\n");
      return 1;
   }

   fprintf(stderr, "PASS output_store_test (sabotage caught by %u checks)\n",
         failures);
   return 0;
}
