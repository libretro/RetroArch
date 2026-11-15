/* Exercise the shipping reader under deterministic retirement at hazard arm. */
#include <stdio.h>
#include <stdlib.h>
#include <boolean.h>

#define RETRO_HW_FRAME_BUFFER_VALID ((const void*)-1)
#ifndef TEST_FALLBACK
#define FRAME_CACHE_HAZARDS
#endif

static unsigned char pixel = 0x5a;
static const void *frame_cache_data = &pixel;
static unsigned frame_cache_dims = 17;
static size_t frame_cache_pitch = 32;
static unsigned snapshots, arms, releases, callbacks, held;
static bool collide, full;
#ifdef FRAME_CACHE_HAZARDS
static int frame_cache_generation;
#endif
static const void *observed;
static unsigned observed_dims;
static size_t observed_pitch;

static bool frame_cache_snapshot(const void **data, unsigned *dims, size_t *pitch)
{
   snapshots++;
   *data = frame_cache_data;
   *dims = frame_cache_dims;
   *pitch = frame_cache_pitch;
   return true;
}
#ifdef FRAME_CACHE_HAZARDS
static int retro_atomic_load_acquire_int(const int *p) { return *p; }
static int frame_cache_hazard_acquire(const void *p)
{
   (void)p;
   arms++;
   /* Bound a broken reader so the parent revision reports failure, not a hang. */
   if (full || arms > 100)
      return -1;
   held++;
   if (collide)
      frame_cache_generation++;
   return 0;
}
static void frame_cache_hazard_release(int slot)
{
   if (slot >= 0) { releases++; held--; }
}
#else
static void cached_frame_lock_acquire(void) { held++; }
static void cached_frame_lock_release(void) { held--; }
#define frame_cache_hazard_release(slot) ((void)0)
#endif

#include "reader.inc"

static void consume(void *userdata, const void *data, unsigned dims, size_t pitch)
{
   unsigned *calls = (unsigned*)userdata;
   (*calls)++;
   observed = data;
   observed_dims = dims;
   observed_pitch = pitch;
   if (data && (!held || *(const unsigned char*)data != pixel))
      abort();
}

static int read_case(const char *name, const void *data, bool race, bool busy,
      bool pixels)
{
   frame_cache_data = data;
   collide = race;
   full = busy;
   snapshots = arms = releases = callbacks = held = 0;
   video_driver_cached_frame_read(&callbacks, consume);
   if (callbacks != 1 || snapshots != 1 || held ||
       observed != (pixels ? data : NULL) ||
       observed_dims != (pixels ? frame_cache_dims : 0) ||
       observed_pitch != (pixels ? frame_cache_pitch : 0))
   {
      fprintf(stderr, "FAIL %s: callbacks=%u snapshots=%u arms=%u held=%u\n",
            name, callbacks, snapshots, arms, held);
      return 1;
   }
#ifdef FRAME_CACHE_HAZARDS
   if (arms != (data && data != RETRO_HW_FRAME_BUFFER_VALID ? 1u : 0u) ||
       releases != (arms && !busy ? 1u : 0u))
      return 1;
#endif
   return 0;
}

int main(void)
{
   int failures = 0;
   failures += read_case("pixels", &pixel, false, false, true);
   failures += read_case("empty", NULL, false, false, false);
   failures += read_case("hardware", RETRO_HW_FRAME_BUFFER_VALID, false, false, false);
#ifdef FRAME_CACHE_HAZARDS
   failures += read_case("retire at every arm", &pixel, true, false, false);
   failures += read_case("all slots busy", &pixel, false, true, false);
#endif
   snapshots = arms = callbacks = held = 0;
   video_driver_cached_frame_read(NULL, NULL);
   if (snapshots || arms || callbacks || held)
      failures++;
   printf("cached_frame_progress: %s\n", failures ? "FAILED" : "passed");
   return failures ? 1 : 0;
}
