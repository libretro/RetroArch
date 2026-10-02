/* gfx/drivers/hub75_frames.h, the three canvases between the HUB75
 * driver's video thread and its panel refresh thread.
 *
 * The refresh thread scans a frame onto the panel continuously, and a
 * scan of a large chain is long next to a frame. What is asserted:
 *  - a scan never sees a canvas the video thread is drawing into
 *    (every word of a scanned canvas carries the same frame number),
 *  - scans never go back to an older frame,
 *  - the last frame drawn is the one the panel ends up showing,
 *  - the video thread never waits on a scan: handing a frame over
 *    while a long scan is in progress returns at once. */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#include "gfx/drivers/hub75_frames.h"

#define WORDS   (128 * 64)
#define FRAMES  3000

static unsigned failures;
#define CHECK(cond, ...) \
   do { if (!(cond)) { printf("FAIL line %d: ", __LINE__); \
        printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static uint32_t canvas[3][WORDS];
static hub75_frames_t frames;
static retro_atomic_int_t running;
static retro_atomic_int_t scans;
static unsigned torn, backwards;
static retro_atomic_int_t last_seen;   /* polled by the main thread */

static uint64_t now_ns(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* A scan: read every word of the front canvas, slowly. */
static void *refresh_main(void *arg)
{
   (void)arg;
   while (retro_atomic_load_acquire_int(&running))
   {
      const volatile uint32_t *px =
            (const volatile uint32_t*)hub75_frames_front(&frames);
      uint32_t first = px[0];
      unsigned i;
      for (i = 0; i < WORDS; i++)
      {
         if (px[i] != first)
         {
            torn++;
            break;
         }
         if (!(i & 255))
         {
            /* A long scan, next to a frame. */
            struct timespec d = { 0, 2000 };
            nanosleep(&d, NULL);
         }
      }
      if ((int)first < retro_atomic_load_acquire_int(&last_seen))
         backwards++;
      retro_atomic_store_release_int(&last_seen, (int)first);
      retro_atomic_fetch_add_int(&scans, 1);
   }
   return NULL;
}

int main(void)
{
   pthread_t thread;
   unsigned f, i;
   uint64_t worst = 0;
   int settle;

   hub75_frames_init(&frames, canvas[0], canvas[1], canvas[2]);
   retro_atomic_store_release_int(&running, 1);
   pthread_create(&thread, NULL, refresh_main, NULL);

   for (f = 1; f <= FRAMES; f++)
   {
      uint32_t *px = (uint32_t*)hub75_frames_back(&frames);
      uint64_t t0;
      for (i = 0; i < WORDS; i++)
         px[i] = f;
      t0 = now_ns();
      hub75_frames_publish(&frames);
      t0 = now_ns() - t0;
      if (t0 > worst)
         worst = t0;
   }

   /* Let the panel catch up with the last frame. */
   for (settle = 0; settle < 2000
         && retro_atomic_load_acquire_int(&last_seen) != FRAMES; settle++)
   {
      struct timespec d = { 0, 1000000 };
      nanosleep(&d, NULL);
   }
   retro_atomic_store_release_int(&running, 0);
   pthread_join(thread, NULL);

   CHECK(torn == 0, "%u scans saw a canvas being drawn into", torn);
   CHECK(backwards == 0, "%u scans went back to an older frame", backwards);
   CHECK(retro_atomic_load_acquire_int(&last_seen) == FRAMES,
         "the panel shows frame %d, not the last (%u)",
         retro_atomic_load_acquire_int(&last_seen), FRAMES);
   CHECK(retro_atomic_load_acquire_int(&scans) > 0, "no scan ran");
   /* A scan here is milliseconds; a handover that waited for one would
    * show it. */
   CHECK(worst < 1000000ull,
         "a frame handover took %llu us: it waited on a scan",
         (unsigned long long)(worst / 1000));

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] hub75_frames_test (%d scans over %u frames, slowest handover %llu ns)\n",
         retro_atomic_load_acquire_int(&scans), FRAMES,
         (unsigned long long)worst);
   return 0;
}
