/* A resampler's state() carries the whole stream: saved at any point and
 * loaded into another instance of the configuration, the stream goes on
 * bit for bit as the unbroken one; a load of the wrong size is refused
 * and changes nothing. For every driver that sets it, at the ratios the
 * sinc runs plain and with its decimation stages engaged and fading. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <audio/audio_resampler.h>

extern retro_resampler_t sinc_resampler;
extern retro_resampler_t nearest_resampler;

static unsigned failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, \
   #x); failures++; } } while (0)

#define BLOCK  512
#define BLOCKS 64
#define CAP    (BLOCK * BLOCKS * 8 + 4096)

static float input[BLOCKS][BLOCK * 2];
static float ref[CAP * 2], cont[CAP * 2], junk[CAP * 2];

static void make_input(void)
{
   unsigned b, i, seed = 12345;
   for (b = 0; b < BLOCKS; b++)
      for (i = 0; i < BLOCK; i++)
      {
         double t = (double)(b * BLOCK + i);
         seed = seed * 1103515245u + 12345u;
         input[b][2 * i]     = (float)(0.5 * sin(t * 0.031)
               + ((seed >> 16) & 0xff) / 2048.0);
         input[b][2 * i + 1] = (float)(0.4 * sin(t * 0.0173 + 1.0));
      }
}

static size_t run(const retro_resampler_t *r, void *re, double ratio,
      unsigned first, unsigned last, float *out)
{
   unsigned b;
   size_t total = 0;
   for (b = first; b < last; b++)
   {
      struct resampler_data d;
      d.data_in       = input[b];
      d.input_frames  = BLOCK;
      d.data_out      = out + 2 * total;
      d.output_frames = 0;
      d.ratio         = ratio;
      r->process(re, &d);
      total          += d.output_frames;
   }
   return total;
}

/* @design builds the instances, @ratio drives them, the save comes after
 * @split blocks. */
static void check(const retro_resampler_t *r, double design, double ratio,
      unsigned split)
{
   void *a, *b;
   unsigned char *state, *bad;
   size_t n_ref, n_a, n_b, size, bad_size;

   a     = r->init(NULL, design, RESAMPLER_QUALITY_HIGHER, 0);
   CHECK(a != NULL);
   n_ref = run(r, a, ratio, 0, BLOCKS, ref);

   /* reset is the fresh instance */
   r->reset(a);
   CHECK(run(r, a, ratio, 0, BLOCKS, cont) == n_ref);
   CHECK(memcmp(cont, ref, n_ref * 2 * sizeof(float)) == 0);
   r->free(a);

   a     = r->init(NULL, design, RESAMPLER_QUALITY_HIGHER, 0);
   n_a   = run(r, a, ratio, 0, split, cont);
   size  = r->state(a, NULL, 0, 0);
   CHECK(size > 0);
   state = (unsigned char*)malloc(size);
   CHECK(r->state(a, state, size, 0) == size);

   /* the wrong size is refused and the stream goes on as before */
   bad_size = size + 4;
   bad      = (unsigned char*)calloc(bad_size, 1);
   CHECK(r->state(a, bad, bad_size, 1) == 0);
   CHECK(r->state(a, bad, size - 1, 1) == 0);
   CHECK(r->state(a, bad, size, 0) == size);
   CHECK(memcmp(bad, state, size) == 0);
   free(bad);
   r->free(a);

   /* into an instance with a stream of its own */
   b = r->init(NULL, design, RESAMPLER_QUALITY_HIGHER, 0);
   run(r, b, ratio, 7, 7 + split / 2 + 2, junk);
   CHECK(r->state(b, NULL, 0, 0) == size);
   CHECK(r->state(b, state, size, 1) == size);
   n_b = run(r, b, ratio, split, BLOCKS, cont + 2 * n_a);
   r->free(b);
   free(state);

   CHECK(n_a + n_b == n_ref);
   CHECK(memcmp(cont, ref, n_ref * 2 * sizeof(float)) == 0);
   printf("%-8s design %.3f ratio %.3f split %2u: %u frames, state %u bytes%s\n",
         r->ident, design, ratio, split, (unsigned)n_ref, (unsigned)size,
         failures ? " FAIL" : "");
}

int main(void)
{
   make_input();
   /* fixed ratios */
   check(&sinc_resampler, 48000.0 / 22050.0, 48000.0 / 22050.0, 35);
   check(&sinc_resampler, 48000.0 / 44100.0, 48000.0 / 44100.0, 35);
   check(&sinc_resampler, 22050.0 / 48000.0, 22050.0 / 48000.0, 35);
   /* below design: stages engage; a save mid-crossfade and one settled */
   check(&sinc_resampler, 1.0, 0.45, 1);
   check(&sinc_resampler, 1.0, 0.45, 3);
   check(&sinc_resampler, 1.0, 0.45, 40);
   check(&sinc_resampler, 1.0, 0.2, 2);
   check(&sinc_resampler, 1.0, 0.2, 40);
   check(&nearest_resampler, 1.5, 1.5, 20);
   check(&nearest_resampler, 0.7, 0.7, 20);
   printf(failures ? "RESULT: %u failures\n" : "RESULT: ALL OK\n", failures);
   return failures ? 1 : 0;
}
