/* Reset must restore fresh native output without replacing any allocation. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <audio/audio_resampler.h>
#include <audio/sinc_resampler.h>
#include <audio/sinc_resampler_int16.h>
#include <audio/nearest_resampler_int16.h>
#include <audio/cc_resampler_int16.h>

static unsigned failures, cases, heap_calls;
static int guarded;
void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void*, size_t);
void __real_free(void*);
void *__wrap_malloc(size_t n) { if (guarded) heap_calls++; return __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t z) { if (guarded) heap_calls++; return __real_calloc(n, z); }
void *__wrap_realloc(void *p, size_t n) { if (guarded) heap_calls++; return __real_realloc(p, n); }
void __wrap_free(void *p) { if (guarded) heap_calls++; __real_free(p); }
#define CHECK(x) do { if (!(x)) { printf("FAIL %u: %s\n", (unsigned)__LINE__, #x); failures++; } } while (0)

union native_buffer { float f[8192]; int16_t i[8192]; };
static union native_buffer input, output[2];
struct backend
{
   const retro_resampler_t *floating;
   void (*process)(void*, struct resampler_data_int16*);
   void (*reset)(void*);
   void (*free)(void*);
};
static const struct backend backends[] = {
   { &sinc_resampler, sinc_resampler_int16_process, sinc_resampler_int16_reset, sinc_resampler_int16_free },
   { &nearest_resampler, nearest_resampler_int16_process, nearest_resampler_int16_reset, nearest_resampler_int16_free },
   { &CC_resampler, cc_resampler_int16_process, cc_resampler_int16_reset, cc_resampler_int16_free }
};

static void *create(unsigned backend, unsigned floating, double ratio, unsigned hq)
{
   if (floating)
   {
      if (!backend) return sinc_resampler_init_hq(ratio, RESAMPLER_QUALITY_NORMAL,
#ifdef AUDIO_STRETCH_SCALAR
            0,
#else
            RESAMPLER_SIMD_SSE,
#endif
            hq);
      return backends[backend].floating->init(NULL, ratio, RESAMPLER_QUALITY_NORMAL, 0);
   }
   if (!backend) return sinc_resampler_int16_init_hq(ratio, SINC_INT16_QUALITY_NORMAL, hq);
   if (backend == 1) return nearest_resampler_int16_init();
   return cc_resampler_int16_init(ratio);
}

static size_t process(const struct backend *b, unsigned floating, void *state,
      size_t frames, double ratio, unsigned slot)
{
   size_t count, sample = floating ? sizeof(float) : sizeof(int16_t);
   size_t bound = (size_t)(frames * ratio + 32);
   unsigned i;
   memset(&output[slot], 0x5a, sizeof(output[slot]));
   if (floating)
   {
      struct resampler_data io;
      io.data_in = input.f; io.data_out = output[slot].f;
      io.input_frames = frames; io.output_frames = 0; io.ratio = ratio;
      b->floating->process(state, &io); count = io.output_frames;
   }
   else
   {
      struct resampler_data_int16 io;
      io.data_in = input.i; io.data_out = output[slot].i;
      io.input_frames = frames; io.output_frames = 0; io.ratio = ratio;
      b->process(state, &io); count = io.output_frames;
   }
   CHECK(count <= bound);
   for (i = 0; i < 16; i++)
      CHECK(((unsigned char*)&output[slot])[bound * 2 * sample + i] == 0x5a);
   return count;
}

static void run(unsigned backend, unsigned floating, double ratio, unsigned hq)
{
   static const size_t chunks[] = { 0, 1, 7, 31, 257, 3, 127 };
   const struct backend *b = &backends[backend];
   void *used = create(backend, floating, ratio, hq);
   void *fresh = create(backend, floating, ratio, hq);
   void (*reset)(void*) = floating ? b->floating->reset : b->reset;
   void (*release)(void*) = floating ? b->floating->free : b->free;
   unsigned i, round;
   CHECK(used && fresh && reset);
   if (!used || !fresh || !reset) { if (used) release(used); if (fresh) release(fresh); return; }
   for (i = 0; i < 1026; i++)
   {
      int16_t value = (int16_t)((int)((i * 7919u) % 65536) - 32768);
      if (floating) input.f[i] = value / 32768.0f;
      else input.i[i] = value;
   }
   guarded = 1;
   for (round = 0; round < 3; round++)
   {
      if (round)
      {
         guarded = 0;
         release(fresh);
         fresh = create(backend, floating, ratio, hq);
         CHECK(fresh != NULL);
         if (!fresh) { release(used); return; }
         guarded = 1;
      }
      process(b, floating, used, 513, ratio * 1.001, 0);
      reset(used); reset(used);
      for (i = 0; i < sizeof(chunks) / sizeof(chunks[0]); i++)
      {
         double step = ratio * (i & 1 ? 0.999 : 1.001);
         size_t a = process(b, floating, used, chunks[i], step, 0);
         size_t z = process(b, floating, fresh, chunks[i], step, 1);
         CHECK(a == z);
         if (a == z) CHECK(!memcmp(&output[0], &output[1], a * 2 * (floating ? sizeof(float) : sizeof(int16_t))));
      }
   }
   guarded = 0;
   release(used); release(fresh); cases++;
}

int main(void)
{
   static const double ratios[] = { 0.5, 0.749, 0.75, 1.0, 2.0, 4.0 };
   unsigned backend, floating, ratio, hq;
   nearest_resampler_int16_reset(NULL);
   cc_resampler_int16_reset(NULL);
   sinc_resampler_int16_reset(NULL);
   for (backend = 0; backend < 3; backend++)
      for (floating = 0; floating < 2; floating++)
         for (ratio = 0; ratio < 6; ratio++)
            for (hq = 0; hq < 2; hq++)
               run(backend, floating, ratios[ratio], hq);
   CHECK(!heap_calls);
   printf("native resampler reset: %u cases, %u failures, %u guarded heap calls\n", cases, failures, heap_calls);
   return failures ? 1 : 0;
}
