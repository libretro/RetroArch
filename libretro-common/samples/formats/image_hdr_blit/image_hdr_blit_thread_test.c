/* Concurrent streams must retain their own peak-dependent tone curves. */
#include <stdio.h>
#include <string.h>
#include <formats/rwebm_video.h>
#include <rthreads/rthreads.h>
#include <retro_atomic.h>

typedef struct
{
   unsigned peak, transfer, failures;
   uint32_t expected8[4], expected10[4];
   rwebm_video_hdr_tone_t tone;
} worker_t;

static worker_t workers[6];
static retro_atomic_int_t start;
static const uint16_t y[4] = {320, 448, 600, 800};
static const uint16_t u[1] = {448}, v[1] = {576};

static void convert(worker_t *w, uint32_t *out8, uint32_t *out10)
{
   rwebm_video_blit_i420_hbd_color(out8, 2, 2, 2, y, 2, u, v, 1,
         9, w->transfer, 1, w->peak, 9, 0);
   rwebm_video_blit_i420_10bit_color(out10, 2, 2, 2, y, 2, u, v, 1,
         9, w->transfer, 1, w->peak, 9);
}

static void run(void *arg)
{
   worker_t *w = (worker_t*)arg;
   unsigned i;
   uint32_t out8[4], out10[4];
   while (!retro_atomic_load_acquire_int(&start)) { }
   rwebm_video_hdr_tone_init(&w->tone, w->peak);
   for (i = 0; i < 128; i++)
   {
      convert(w, out8, out10);
      if (memcmp(out8, w->expected8, sizeof(out8))
            || memcmp(out10, w->expected10, sizeof(out10)))
         w->failures++;
      rwebm_video_blit_i420_hbd_tone(out8, 2, 2, 2, y, 2, u, v, 1,
            9, w->transfer, 1, w->peak, 9, 0, &w->tone);
      rwebm_video_blit_i420_10bit_tone(out10, 2, 2, 2, y, 2, u, v, 1,
            9, w->transfer, 1, w->peak, 9, &w->tone);
      if (memcmp(out8, w->expected8, sizeof(out8))
            || memcmp(out10, w->expected10, sizeof(out10)))
         w->failures++;
   }
}

int main(void)
{
   static const unsigned peaks[6] = {0, 400, 1000, 4000, 10000, 0};
   sthread_t *threads[6];
   unsigned i, created = 0, failures = 0;
   for (i = 0; i < 6; i++)
   {
      workers[i].peak = peaks[i];
      workers[i].transfer = i == 5 ? 18 : 16;
      convert(&workers[i], workers[i].expected8, workers[i].expected10);
   }
   for (i = 0; i < 6; i++)
   {
      threads[i] = sthread_create(run, &workers[i]);
      if (!threads[i]) break;
      created++;
   }
   retro_atomic_store_release_int(&start, 1);
   for (i = 0; i < created; i++)
   {
      sthread_join(threads[i]);
      failures += workers[i].failures;
   }
   if (created != 6) failures++;
   printf("Concurrent HDR peak isolation: %u failures\n", failures);
   return failures ? 1 : 0;
}
