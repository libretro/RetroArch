/* input_sensor_snap.h: the sensor snapshot the poll publishes and the
 * shader backends read on the video thread. A reader that never waits
 * and never sees two publications mixed in one read, and that gets the
 * newest once the writer stops. Run under TSan as well. */
#include <stdio.h>
#include <retro_atomic.h>
#include <rthreads/rthreads.h>

#include "input/input_sensor_snap.h"

#define PUBLICATIONS 200000

static input_sensor_snap_t snap;
static retro_atomic_int_t done;
static unsigned failures;

#define CHECK(c, msg) do { if (!(c)) { \
   fprintf(stderr, "FAIL line %d: %s\n", __LINE__, msg); failures++; } } while (0)

/* publication n: every float n, so a mixed read shows */
static void writer(void *data)
{
   unsigned n;
   (void)data;
   for (n = 1; n <= PUBLICATIONS; n++)
   {
      float v[3];
      v[0] = v[1] = v[2] = (float)n;
      input_sensor_snap_publish(&snap, v, v, v);
   }
   retro_atomic_store_release_int(&done, 1);
}

int main(void)
{
   sthread_t *t;
   float g[3], a[3], r[3];
   float last  = 0.0f;
   unsigned reads = 0, mixed = 0, backwards = 0, i;

   /* zeroed: noughts, and no slot shared */
   input_sensor_snap_take(&snap, g, a, r);
   CHECK(g[0] == 0.0f && a[2] == 0.0f && r[1] == 0.0f,
         "the zeroed snapshot is not noughts");

   /* one publication reads back whole, and again with nothing new */
   g[0] = 1.0f; g[1] = 2.0f; g[2] = 3.0f;
   a[0] = 4.0f; a[1] = 5.0f; a[2] = 6.0f;
   r[0] = 7.0f; r[1] = 8.0f; r[2] = 9.0f;
   input_sensor_snap_publish(&snap, g, a, r);
   for (i = 0; i < 2; i++)
   {
      float gg[3], aa[3], rr[3];
      input_sensor_snap_take(&snap, gg, aa, rr);
      CHECK(gg[0] == 1.0f && gg[2] == 3.0f && aa[1] == 5.0f && rr[2] == 9.0f,
            "a publication did not read back whole");
   }

   /* a writer publishing as fast as it can, a reader reading; from
    * a publication of noughts */
   g[0] = g[1] = g[2] = 0.0f;
   input_sensor_snap_publish(&snap, g, g, g);
   t = sthread_create(writer, NULL);
   CHECK(t != NULL, "no thread");
   if (!t)
      return 1;
   while (!retro_atomic_load_acquire_int(&done))
   {
      unsigned k;
      input_sensor_snap_take(&snap, g, a, r);
      reads++;
      for (k = 0; k < 3; k++)
         if (g[k] != g[0] || a[k] != g[0] || r[k] != g[0])
            mixed++;
      if (g[0] < last)
         backwards++;
      last = g[0];
   }
   sthread_join(t);
   input_sensor_snap_take(&snap, g, a, r);
   CHECK(!mixed, "a read mixed two publications");
   CHECK(!backwards, "a read went back to an older publication");
   CHECK(g[0] == (float)PUBLICATIONS && r[2] == (float)PUBLICATIONS,
         "the newest was not read once the writer stopped");
   CHECK(snap.published == PUBLICATIONS + 2, "publications not counted");
   printf("%u reads during %u publications\n", reads, PUBLICATIONS);
   puts(failures ? "FAIL sensor_snap_test" : "PASS sensor_snap_test");
   return failures ? 1 : 0;
}
