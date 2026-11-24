#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <retro_atomic.h>
#include "../../../gfx/gfx_music_visualizer.h"

static gfx_music_visualizer_t *visualizer;
static retro_atomic_int_t finished;
static uint32_t pixels[GFX_MUSIC_VISUALIZER_WIDTH * GFX_MUSIC_VISUALIZER_HEIGHT];
static int16_t samples[1600];

static void *produce(void *unused)
{
   unsigned i, frame;
   (void)unused;
   for (i = 0; i < 800; i++)
   {
      samples[i * 2] = (int16_t)((i % 64) * 800 - 25000);
      samples[i * 2 + 1] = (int16_t)((i % 43) * 1100 - 23000);
   }
   for (frame = 0; frame < 2000; frame++)
      gfx_music_visualizer_feed(visualizer, samples, 800, false, 48000);
   retro_atomic_store_release_int(&finished, 1);
   return NULL;
}

int main(void)
{
   pthread_t producer;
   unsigned i;
   visualizer = gfx_music_visualizer_new();
   assert(visualizer);
   retro_atomic_int_init(&finished, 0);
   assert(!pthread_create(&producer, NULL, produce, NULL));
   for (i = 0; i < 2000; i++)
   {
      gfx_music_visualizer_render(visualizer, pixels,
            GFX_MUSIC_VISUALIZER_WIDTH * sizeof(uint32_t), false);
      if (retro_atomic_load_acquire_int(&finished)) break;
   }
   assert(!pthread_join(producer, NULL));
   gfx_music_visualizer_render(visualizer, pixels,
         GFX_MUSIC_VISUALIZER_WIDTH * sizeof(uint32_t), false);
   for (i = 0; i < sizeof(pixels) / sizeof(*pixels); i++) assert((pixels[i] >> 24) == 255);
   gfx_music_visualizer_free(visualizer);
   puts("PASS: audio producer and display consumer share bounded spectrum loans without races");
   return 0;
}
