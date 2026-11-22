#include <retro_posix_source.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static unsigned attempts, live, fail_at;
static void *test_malloc(size_t size)
{
   void *p;
   if (++attempts == fail_at) return NULL;
   p = malloc(size);
   if (p) live++;
   return p;
}
static void *test_calloc(size_t count, size_t size)
{
   void *p = test_malloc(count * size);
   if (p) memset(p, 0, count * size);
   return p;
}
static void test_free(void *p)
{
   if (p) { assert(live); live--; }
   free(p);
}
#define malloc test_malloc
#define calloc test_calloc
#define free test_free
#include "../../../libretro-common/audio/dsp_filters/fft/fft.c"
#include "../../../libretro-common/queues/retro_spsc.c"
#include "../../../gfx/gfx_music_visualizer.c"
#undef malloc
#undef calloc
#undef free

#define WIDTH GFX_MUSIC_VISUALIZER_WIDTH
#define HEIGHT GFX_MUSIC_VISUALIZER_HEIGHT
#define STRIDE (WIDTH + 4)
static uint32_t loan[HEIGHT * STRIDE + 2];
static int16_t pcm[MUSIC_FFT_SIZE * 6];
static float floating_pcm[MUSIC_FFT_SIZE * 2];
static const struct gfx_music_row *last_row(gfx_music_visualizer_t *v)
{
   const void *first, *second;
   size_t first_bytes;
   size_t bytes = retro_spsc_read_begin_pair(&v->rows, &first, &first_bytes,
         &second, MUSIC_QUEUE_BYTES);
   size_t offset;
   const struct gfx_music_row *row;
   assert(bytes >= MUSIC_ROW_BYTES);
   offset = bytes - MUSIC_ROW_BYTES;
   row = (const struct gfx_music_row*)(offset < first_bytes
         ? (const uint8_t*)first + offset
         : (const uint8_t*)second + offset - first_bytes);
   /* Test is single-threaded; no producer runs while this row is inspected. */
   retro_spsc_read_end(&v->rows, 0);
   return row;
}

static void spectrum(gfx_music_visualizer_t *v)
{
   unsigned i;
   const struct gfx_music_row *row;
   static fft_complex_t input[MUSIC_FFT_SIZE], reference[MUSIC_FFT_SIZE];
   for (i = 0; i < MUSIC_FFT_SIZE; i++)
   {
      pcm[i * 2] = (int16_t)(24000 * sin(2.0 * MUSIC_PI * 32 * i / MUSIC_FFT_SIZE));
      pcm[i * 2 + 1] = 0;
      floating_pcm[i * 2] = pcm[i * 2] / 32768.0f;
      floating_pcm[i * 2 + 1] = 0;
   }
   gfx_music_visualizer_feed(v, pcm, MUSIC_FFT_SIZE, false, 48000);
   row = last_row(v);
   assert(row->height[8][0] > 20);
   assert(row->height[8][2] < -39);
   assert(row->height[8][0] > row->height[40][0] + 30);
   for (i = 0; i < MUSIC_FFT_SIZE; i++)
   {
      unsigned source = (v->position + i) & (MUSIC_FFT_SIZE - 1);
      input[i].real = v->pcm[source * 2] * v->window[i];
      input[i].imag = v->pcm[source * 2 + 1] * v->window[i];
   }
   fft_process_forward_complex(v->fft, reference, input, 1);
   fft_process_forward_stereo_window(v->fft, v->output, v->pcm, v->window, v->position);
   assert(!memcmp(reference, v->output, sizeof(reference)));
   gfx_music_visualizer_reset(v);
   for (i = 0; i < MUSIC_FFT_SIZE; i++) pcm[i * 2 + 1] = -pcm[i * 2];
   gfx_music_visualizer_feed(v, pcm, MUSIC_FFT_SIZE, false, 48000);
   row = last_row(v);
   assert(row->height[8][1] < -39);
   assert(row->height[8][3] > 20);
}

int main(void)
{
   gfx_music_visualizer_t *v;
   unsigned i, row, col, allocated;
   for (i = 1; i <= 5; i++)
   {
      attempts = 0;
      fail_at = i;
      v = gfx_music_visualizer_new();
      assert(!v && !live);
   }
   fail_at = 0;
   v = gfx_music_visualizer_new();
   assert(v);
   allocated = attempts;
   spectrum(v);
   gfx_music_visualizer_reset(v);
   for (i = 0; i < sizeof(loan) / sizeof(*loan); i++) loan[i] = 0xabcdef01u;
   gfx_music_visualizer_feed(v, pcm, MUSIC_FFT_SIZE, false, 48000);
   assert(gfx_music_visualizer_render(v, loan + 1, STRIDE * sizeof(uint32_t), false));
   assert(loan[0] == 0xabcdef01u && loan[HEIGHT * STRIDE + 1] == 0xabcdef01u);
   for (row = 0; row < HEIGHT; row++)
   {
      for (col = 0; col < WIDTH; col++)
         assert((loan[1 + row * STRIDE + col] >> 24) == 255);
      for (col = WIDTH; col < STRIDE; col++)
         assert(loan[1 + row * STRIDE + col] == 0xabcdef01u);
   }
   assert(!gfx_music_visualizer_render(v, loan + 1, STRIDE * sizeof(uint32_t), false));
   assert(!gfx_music_visualizer_render(v, loan + 1, 1, false));
   assert(!gfx_music_visualizer_render(v, loan + 1, (size_t)-1, false));
   assert(!gfx_music_visualizer_render(v, (uint32_t*)((uint8_t*)loan + 1), WIDTH * 4, false));
   gfx_music_visualizer_reset(v);
   for (i = 0; i < MUSIC_FFT_SIZE; i++) pcm[i * 2 + 1] = 0;
   gfx_music_visualizer_feed(v, pcm, MUSIC_FFT_SIZE, false, 48000);
   assert(gfx_music_visualizer_render(v, loan, WIDTH * 4, false));
   gfx_music_visualizer_reset(v);
   gfx_music_visualizer_feed(v, floating_pcm, MUSIC_FFT_SIZE, true, 48000);
   {
      static uint32_t other[WIDTH * HEIGHT];
      assert(gfx_music_visualizer_render(v, other, WIDTH * 4, false));
      assert(!memcmp(loan, other, sizeof(other)));
      gfx_music_visualizer_reset(v);
      gfx_music_visualizer_feed(v, pcm, MUSIC_FFT_SIZE, false, 48000);
      assert(gfx_music_visualizer_render(v, other, WIDTH * 4, true));
      for (i = 0; i < WIDTH * HEIGHT; i++)
         assert(other[i] == ((loan[i] & 0xff00ff00u)
               | ((loan[i] & 0xffu) << 16) | ((loan[i] >> 16) & 0xffu)));
   }
   gfx_music_visualizer_reset(v);
   gfx_music_visualizer_feed(v, pcm, 713, false, 48000);
   gfx_music_visualizer_feed(v, pcm + 1426, MUSIC_FFT_SIZE - 713, false, 48000);
   assert(!v->position && v->pcm[1424] == pcm[1424] / 32768.0f);
   assert(v->pcm[1426] == pcm[1426] / 32768.0f);
   /* A stalled display exhausts a bounded loan pool; audio keeps running. */
   for (i = 0; i < 600; i++) gfx_music_visualizer_feed(v, pcm, 800, false, 48000);
   assert(v->sequence == MUSIC_HISTORY * 2);
   assert(gfx_music_visualizer_render(v, loan, WIDTH * 4, false));
   gfx_music_visualizer_feed(v, pcm, 800, false, 48000);
   assert(v->sequence == MUSIC_HISTORY * 2 + 1);
   assert(gfx_music_visualizer_render(v, loan, WIDTH * 4, false));
   assert(attempts == allocated);
   gfx_music_visualizer_free(v);
   assert(!live);
   puts("PASS: stereo spectrum, fused FFT, reset, circular PCM, float PCM, lent rows/pitched output, pause, bounded stalls and allocation failures");
   return 0;
}
