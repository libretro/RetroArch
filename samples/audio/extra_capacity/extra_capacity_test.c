#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <audio/sinc_resampler.h>
#ifndef EXTRA_TEST_SIMD
#define EXTRA_TEST_SIMD 0
#endif
static unsigned heap_calls;
static unsigned fail_at;
static struct { void *ptr; size_t size; } allocations[128];
static void *tracked_malloc(size_t size)
{
   unsigned i;
   void *p;
   heap_calls++;
   if (fail_at && (heap_calls == fail_at || size > 1024 * 1024)) return NULL;
   p = malloc(size);
   if (!p) return NULL;
   for (i = 0; i < 128; i++)
      if (!allocations[i].ptr || allocations[i].ptr == p)
      {
         allocations[i].ptr = p;
         allocations[i].size = size;
         return p;
      }
   abort();
   return NULL;
}
/* Retire tracked buffers even when the allocator does not reuse addresses.
 * Keep the linker wrapper visible despite -fwhole-program. */
void __real_free(void *p);
__attribute__((externally_visible)) void __wrap_free(void *p)
{
   unsigned i;
   if (p)
      for (i = 0; i < 128; i++)
         if (allocations[i].ptr == p)
         {
            allocations[i].ptr = NULL;
            allocations[i].size = 0;
            break;
         }
   __real_free(p);
}
static unsigned live_allocations(void)
{
   unsigned i, count = 0;
   for (i = 0; i < 128; i++)
      if (allocations[i].ptr) count++;
   return count;
}
static size_t allocation_size(void *p)
{
   unsigned i;
   for (i = 0; i < 128; i++)
      if (allocations[i].ptr == p) return allocations[i].size;
   return 0;
}
static unsigned realloc_calls;
static bool record_fail_alloc;
static void *tracked_realloc(void *ptr, size_t bytes)
{
   realloc_calls++;
   if (record_fail_alloc) return NULL;
   return realloc(ptr, bytes);
}
#define realloc tracked_realloc
#define malloc tracked_malloc
#include "../../../audio/audio_driver.c"
#undef malloc
#undef realloc

/* Only the resampler factory is stubbed; preparation and processing are real. */
bool retro_resampler_realloc_hq(void **re, const retro_resampler_t **backend,
      const char *ident, enum resampler_quality quality, double ratio, bool hq)
{
   static retro_resampler_t alternate;
   if (*re && *backend) (*backend)->free(*re);
   *re = NULL;
   *backend = NULL;
   if (ident && strcmp(ident, "fail") == 0) return false;
   *backend = &sinc_resampler;
   if (ident && strcmp(ident, "other") == 0)
   {
      alternate = sinc_resampler;
      *backend = &alternate;
      hq = false;
   }
   *re = sinc_resampler_init_hq(ratio, quality, EXTRA_TEST_SIMD, hq);
   return *re != NULL;
}

static unsigned failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL %d: %s\n", __LINE__, #x); failures++; } } while (0)

static void check_lane(int integer)
{
   static audio_driver_state_t st;
   unsigned step, i;
   void *front;
   static float output_f[16384 * 2];
   static int16_t output_i[16384 * 2];
   const size_t counts[] = {32, 512, 1024, 16, 1024};
   memset(&st, 0, sizeof(st));
   st.resampler = &sinc_resampler;
   st.resampler_quality = RESAMPLER_QUALITY_NORMAL;
   st.src_ratio_orig = 8.0;
   st.output_samples_buf_length = 16384 * 2 * sizeof(float);
   st.output_samples_int16_length = 16384 * 2 * sizeof(int16_t);
   st.resampler_int16_process = sinc_resampler_int16_process;
   st.resampler_int16_free = sinc_resampler_int16_free;
   front = integer ? sinc_resampler_int16_init(8.0, SINC_INT16_QUALITY_NORMAL)
      : sinc_resampler.init(NULL, 8.0, RESAMPLER_QUALITY_NORMAL, EXTRA_TEST_SIMD);
   if (!front) exit(2);
   for (step = 0; step < 5; step++)
   {
      unsigned before = heap_calls;
      size_t produced;
      CHECK(audio_driver_extra_prepare(&st, 2, 0, counts[step], !integer, integer));
      CHECK(st.extra.cap_out >= 16384);
      if (step >= 3) CHECK(heap_calls == before);
      CHECK(!st.extra.pair_in && !st.extra.pair_in_i);
      CHECK(!st.extra.pair_out && !st.extra.pair_out_i);
      CHECK(allocation_size(st.extra.in_f) >= counts[step] * 2 * sizeof(float));
      CHECK(allocation_size(st.extra.in_i) >= counts[step] * 2 * sizeof(int16_t));
      if (allocation_size(st.extra.in_f) < counts[step] * 2 * sizeof(float)
            || allocation_size(st.extra.in_i) < counts[step] * 2 * sizeof(int16_t)) break;
      /* Refuse to exercise unsafe allocations when run against the old code. */
      if (st.extra.cap_out < 16384) break;
      for (i = 0; i < counts[step] * 2; i++)
      {
         if (integer) st.extra.in_i[i] = (int16_t)(i * 53);
         else st.extra.in_f[i] = (float)(i % 256) / 512.0f;
      }
      if (integer)
      {
         struct resampler_data_int16 d;
         d.data_in = st.extra.in_i; d.data_out = output_i;
         d.input_frames = counts[step]; d.ratio = 8.0;
         sinc_resampler_int16_process(front, &d); produced = d.output_frames;
      }
      else
      {
         struct resampler_data d;
         d.data_in = st.extra.in_f; d.data_out = output_f;
         d.input_frames = counts[step]; d.ratio = 8.0;
         sinc_resampler.process(front, &d); produced = d.output_frames;
      }
      before = heap_calls;
      st.extra.pending = true;
      audio_driver_extra_resample(&st, 8.0, counts[step], false, integer);
      CHECK(st.extra.out_frames >= counts[step] * 8);
      CHECK(st.extra.out_frames <= st.extra.cap_out);
      CHECK(heap_calls == before);
      CHECK(st.extra.out_frames == produced);
      if (integer) CHECK(memcmp(st.extra.out_i, output_i, produced * 2 * sizeof(int16_t)) == 0);
      else CHECK(memcmp(st.extra.out_f, output_f, produced * 2 * sizeof(float)) == 0);
   }
   audio_driver_extra_free(&st);
   if (integer) sinc_resampler_int16_free(front);
   else sinc_resampler.free(front);
}

static unsigned reset_calls;
static void count_reset(void *state)
{
   reset_calls++;
   sinc_resampler.reset(state);
}

static void check_bypass(void)
{
   static audio_driver_state_t st;
   static float front_input[256 * 2], front_output[1024 * 2];
   retro_resampler_t backend = sinc_resampler;
   void *front;
   unsigned pass, i, before;
   backend.reset = count_reset;
   memset(&st, 0, sizeof(st));
   st.resampler = &backend;
   st.resampler_quality = RESAMPLER_QUALITY_NORMAL;
   st.src_ratio_orig = 1.0;
   st.output_samples_buf_length = sizeof(front_output);
   /* Four extras exercise two independently owned histories. */
   CHECK(audio_driver_extra_prepare(&st, 4, 0, 256, true, false));
   front = sinc_resampler.init(NULL, 1.0, RESAMPLER_QUALITY_NORMAL, EXTRA_TEST_SIMD);
   if (!front) exit(2);
   for (i = 0; i < 256; i++)
   {
      unsigned ch;
      for (ch = 0; ch < 4; ch++)
         st.extra.in_f[4 * i + ch] = (float)((i + ch) % 23) / 32.0f;
   }
   st.extra.pending = true;
   audio_driver_extra_resample(&st, 1.5, 256, false, false);
   reset_calls = 0;
   before = heap_calls;
   for (pass = 0; pass < 3; pass++)
   {
      st.extra.pending = true;
      audio_driver_extra_resample(&st, 1.0, 256, true, false);
      CHECK(st.extra.out_frames == 256);
      CHECK(memcmp(st.extra.out_f, st.extra.in_f, 256 * 4 * sizeof(float)) == 0);
      CHECK(reset_calls == 2);
   }
   memset(st.extra.in_f, 0, 256 * 4 * sizeof(float));
   memset(front_input, 0, sizeof(front_input));
   for (pass = 0; pass < 2; pass++)
   {
      struct resampler_data d;
      d.data_in = front_input; d.data_out = front_output;
      d.input_frames = 256; d.ratio = 1.5;
      sinc_resampler.process(front, &d);
      st.extra.pending = true;
      audio_driver_extra_resample(&st, 1.5, 256, false, false);
      CHECK(st.extra.out_frames == d.output_frames);
      for (i = 0; i < d.output_frames * 4; i++)
         CHECK(st.extra.out_f[i] == front_output[(i / 4) * 2 + (i % 2)]);
      CHECK(reset_calls == 2);
   }
   CHECK(heap_calls == before);
   audio_driver_extra_free(&st);
   sinc_resampler.free(front);
}

static void check_direct_bypass(void)
{
   static audio_driver_state_t st;
   static float expected_f[32 * 5];
   static int16_t expected_i[32 * 5];
   unsigned ch, integer, source_float, i, pass;
   const size_t counts[] = {0, 1, 17, 32, 40};
   const uint32_t special[] = {0x00000000u, 0x80000000u, 0x7f800000u,
      0xff800000u, 0x7fc12345u, 0x37800000u, 0xb7800000u};
   for (ch = 1; ch <= 5; ch++)
      for (integer = 0; integer < 2; integer++)
         for (source_float = 0; source_float < 2; source_float++)
         {
            memset(&st, 0, sizeof(st));
            st.resampler = &sinc_resampler;
            st.resampler_quality = RESAMPLER_QUALITY_NORMAL;
            st.src_ratio_orig = 1;
            st.resampler_int16_free = sinc_resampler_int16_free;
            CHECK(audio_driver_extra_prepare(&st, ch, 0, 32, source_float != 0, integer != 0));
            /* Also cover bypass with no front resampler, as in bitstreaming. */
            st.resampler = NULL;
            st.extra.cap_out = 19;
            for (i = 0; i < 32 * ch; i++)
            {
               st.extra.in_i[i] = (int16_t)((int)(i * 977) - 32768);
               st.extra.in_f[i] = ((int)(i % 13) - 6) / 4.0f;
            }
            for (i = 0; i < sizeof(special) / sizeof(special[0]); i++)
               memcpy(st.extra.in_f + i, special + i, sizeof(float));
            for (pass = 0; pass < sizeof(counts) / sizeof(counts[0]); pass++)
            {
               size_t cap = pass == 4 ? 37 : 19;
               size_t n = counts[pass] < cap ? counts[pass] : cap;
               unsigned before = heap_calls;
               if (n > 32) n = 32;
               st.extra.cap_out = cap;
               if (ch != 2)
               {
                  if (st.extra.pair_in) memset(st.extra.pair_in, 0x5a, 32 * 2 * sizeof(float));
                  if (st.extra.pair_in_i) memset(st.extra.pair_in_i, 0x5a, 32 * 2 * sizeof(int16_t));
                  if (st.extra.pair_out) memset(st.extra.pair_out, 0x5a, 32 * 2 * sizeof(float));
                  if (st.extra.pair_out_i) memset(st.extra.pair_out_i, 0x5a, 32 * 2 * sizeof(int16_t));
               }
               memset(st.extra.out_f, 0x5a, 32 * ch * sizeof(float));
               memset(st.extra.out_i, 0x5a, 32 * ch * sizeof(int16_t));
               if (source_float)
               {
                  memcpy(expected_f, st.extra.in_f, n * ch * sizeof(float));
                  convert_float_to_s16(expected_i, st.extra.in_f, n * ch);
               }
               else
               {
                  memcpy(expected_i, st.extra.in_i, n * ch * sizeof(int16_t));
                  convert_s16_to_float(expected_f, st.extra.in_i, n * ch, 1.0f);
               }
               st.extra.pending = true;
               audio_driver_extra_resample(&st, 1, counts[pass], true, integer != 0);
               CHECK(!st.extra.pending && st.extra.out_frames == n);
               CHECK(heap_calls == before);
               if (integer) CHECK(memcmp(st.extra.out_i, expected_i, n * ch * sizeof(int16_t)) == 0);
               else CHECK(memcmp(st.extra.out_f, expected_f, n * ch * sizeof(float)) == 0);
               if (ch != 2)
               {
                  for (i = 0; !integer && i < 32 * 2 * sizeof(float); i++)
                  {
                     CHECK(((unsigned char*)st.extra.pair_in)[i] == 0x5a);
                     CHECK(((unsigned char*)st.extra.pair_out)[i] == 0x5a);
                  }
                  for (i = 0; integer && i < 32 * 2 * sizeof(int16_t); i++)
                  {
                     CHECK(((unsigned char*)st.extra.pair_in_i)[i] == 0x5a);
                     CHECK(((unsigned char*)st.extra.pair_out_i)[i] == 0x5a);
                  }
               }
               else
               {
                  CHECK(!st.extra.pair_in && !st.extra.pair_out);
                  CHECK(!st.extra.pair_in_i && !st.extra.pair_out_i);
               }
               for (i = (unsigned)(n * ch * sizeof(float)); i < 32 * ch * sizeof(float); i++)
                  CHECK(((unsigned char*)st.extra.out_f)[i] == 0x5a);
               for (i = (unsigned)(n * ch * sizeof(int16_t)); i < 32 * ch * sizeof(int16_t); i++)
                  CHECK(((unsigned char*)st.extra.out_i)[i] == 0x5a);
            }
            st.resampler = &sinc_resampler;
            audio_driver_extra_free(&st);
         }
}

static void check_direct_pair(void)
{
   static audio_driver_state_t st;
   static float expected_f[512 * 2];
   static int16_t expected_i[512 * 2];
   static float reference_f[127 * 2];
   static int16_t reference_i[127 * 2];
   unsigned integer, source_float, pass, i;
   const double ratios[] = {2.0, 1.999, 2.001, 1.0, 2.0};
   const size_t counts[] = {0, 1, 127, 128, 127};
   for (integer = 0; integer < 2; integer++)
      for (source_float = 0; source_float < 2; source_float++)
      {
         void *reference;
         memset(&st, 0, sizeof(st));
         st.resampler = &sinc_resampler;
         st.resampler_quality = RESAMPLER_QUALITY_NORMAL;
         st.src_ratio_orig = 2;
         st.resampler_int16_free = sinc_resampler_int16_free;
         st.resampler_int16_process = sinc_resampler_int16_process;
         CHECK(audio_driver_extra_prepare(&st, 2, 0, 127, source_float != 0, integer != 0));
         reference = integer ? sinc_resampler_int16_init(2, SINC_INT16_QUALITY_NORMAL)
            : sinc_resampler.init(NULL, 2, RESAMPLER_QUALITY_NORMAL, EXTRA_TEST_SIMD);
         if (!reference) exit(2);
         for (pass = 0; pass < sizeof(ratios) / sizeof(ratios[0]); pass++)
         {
            size_t produced;
            size_t frames = counts[pass] > 127 ? 127 : counts[pass];
            unsigned before = heap_calls;
            for (i = 0; i < 127 * 2; i++)
            {
               st.extra.in_i[i] = (int16_t)((int)(i % 53) * 977 - 25000);
               st.extra.in_f[i] = ((int)(i % 37) - 18) / 16.0f;
            }
            if (integer)
            {
               struct resampler_data_int16 d;
               if (source_float) convert_float_to_s16(reference_i, st.extra.in_f, frames * 2);
               else memcpy(reference_i, st.extra.in_i, frames * 2 * sizeof(int16_t));
               d.data_in = reference_i; d.data_out = expected_i;
               d.input_frames = frames; d.output_frames = 0; d.ratio = ratios[pass];
               sinc_resampler_int16_process(reference, &d);
               produced = d.output_frames;
            }
            else
            {
               struct resampler_data d;
               if (!source_float) convert_s16_to_float(reference_f, st.extra.in_i, frames * 2, 1.0f);
               else memcpy(reference_f, st.extra.in_f, frames * 2 * sizeof(float));
               d.data_in = reference_f; d.data_out = expected_f;
               d.input_frames = frames; d.output_frames = 0; d.ratio = ratios[pass];
               sinc_resampler.process(reference, &d);
               produced = d.output_frames;
            }
            st.extra.pending = true;
            audio_driver_extra_resample(&st, ratios[pass], counts[pass], false, integer != 0);
            CHECK(!st.extra.pending && st.extra.out_frames == produced);
            CHECK(heap_calls == before);
            CHECK(!st.extra.pair_in && !st.extra.pair_out);
            CHECK(!st.extra.pair_in_i && !st.extra.pair_out_i);
            if (integer) CHECK(memcmp(expected_i, st.extra.out_i, produced * 2 * sizeof(int16_t)) == 0);
            else CHECK(memcmp(expected_f, st.extra.out_f, produced * 2 * sizeof(float)) == 0);
         }
         if (integer) sinc_resampler_int16_free(reference);
         else sinc_resampler.free(reference);
         audio_driver_extra_free(&st);
      }
}

static void check_multichannel_alignment(bool hq)
{
   static audio_driver_state_t st;
   static float source_f[8][127], expected_f[8][512];
   static int16_t source_i[8][127], expected_i[8][512];
   static float pair_f[127 * 2], output_f[512 * 2];
   static int16_t pair_i[127 * 2], output_i[512 * 2];
   const size_t counts[] = {0, 1, 31, 127, 128, 63, 17, 127};
   const double ratios[] = {2, 2, 1.999, 2.001, 0.5, 1, 4, 1.25};
   unsigned ch, lane, source_float, pass, c, i, pair;
   for (ch = 1; ch <= 8; ch++)
      for (lane = 0; lane < 2; lane++)
         for (source_float = 0; source_float < 2; source_float++)
         {
            void *reference[4];
            unsigned pairs = (ch + 1) / 2;
            memset(&st, 0, sizeof(st));
            st.resampler = &sinc_resampler;
            st.resampler_quality = RESAMPLER_QUALITY_NORMAL;
            st.src_ratio_orig = 2;
            st.resampler_int16_free = sinc_resampler_int16_free;
            st.resampler_int16_process = sinc_resampler_int16_process;
            CHECK(audio_driver_resampler_realloc(&st, hq));
            CHECK(st.resampler_hq == hq);
            CHECK(audio_driver_extra_prepare(&st, ch, 0, 127, source_float, lane));
            for (pair = 0; pair < pairs; pair++)
            {
               reference[pair] = lane
                  ? sinc_resampler_int16_init_hq(2, SINC_INT16_QUALITY_NORMAL, hq)
                  : sinc_resampler_init_hq(2, RESAMPLER_QUALITY_NORMAL, EXTRA_TEST_SIMD, hq);
               if (!reference[pair]) exit(2);
            }
            for (pass = 0; pass < sizeof(counts) / sizeof(counts[0]); pass++)
            {
               size_t frames = counts[pass] > 127 ? 127 : counts[pass];
               size_t produced = 0, bytes, tail;
               unsigned before = heap_calls;
               memset(st.extra.in_f, 0x5a, 127 * ch * sizeof(float));
               memset(st.extra.in_i, 0x5a, 127 * ch * sizeof(int16_t));
               for (c = 0; c < ch; c++)
                  for (i = 0; i < 127; i++)
                  {
                     int value = (int)((i * 997 + c * 7919 + pass * 3571) % 65536) - 32768;
                     source_i[c][i] = (int16_t)value;
                     source_f[c][i] = value / 24576.0f;
                     if (source_float) st.extra.in_f[i * ch + c] = source_f[c][i];
                     else st.extra.in_i[i * ch + c] = source_i[c][i];
                  }
               /* Separate planar sources keep the oracle independent of
                * any conversion or staging performed by the frontend. */
               for (pair = 0; pair < pairs; pair++)
               {
                  size_t n;
                  unsigned left = pair * 2;
                  unsigned right = left + 1 < ch ? left + 1 : left;
                  for (i = 0; i < frames; i++)
                  {
                     pair_f[2 * i] = source_f[left][i];
                     pair_f[2 * i + 1] = source_f[right][i];
                     pair_i[2 * i] = source_i[left][i];
                     pair_i[2 * i + 1] = source_i[right][i];
                  }
                  if (lane)
                  {
                     struct resampler_data_int16 d;
                     if (source_float) convert_float_to_s16(pair_i, pair_f, frames * 2);
                     d.data_in = pair_i; d.data_out = output_i;
                     d.input_frames = frames; d.output_frames = 0; d.ratio = ratios[pass];
                     sinc_resampler_int16_process(reference[pair], &d);
                     n = d.output_frames;
                  }
                  else
                  {
                     struct resampler_data d;
                     if (!source_float) convert_s16_to_float(pair_f, pair_i, frames * 2, 1.0f);
                     d.data_in = pair_f; d.data_out = output_f;
                     d.input_frames = frames; d.output_frames = 0; d.ratio = ratios[pass];
                     sinc_resampler.process(reference[pair], &d);
                     n = d.output_frames;
                  }
                  CHECK(n <= 512);
                  if (n > 512) exit(2);
                  if (pair) CHECK(n == produced);
                  produced = n;
                  for (i = 0; i < n; i++)
                  {
                     expected_f[left][i] = output_f[2 * i];
                     expected_i[left][i] = output_i[2 * i];
                     if (right != left)
                     {
                        expected_f[right][i] = output_f[2 * i + 1];
                        expected_i[right][i] = output_i[2 * i + 1];
                     }
                  }
               }
               bytes = st.extra.cap_out * ch * (lane ? sizeof(int16_t) : sizeof(float));
               memset(lane ? (void*)st.extra.out_i : (void*)st.extra.out_f, 0xa5, bytes);
               st.extra.pending = true;
               audio_driver_extra_resample(&st, ratios[pass], counts[pass], false, lane);
               CHECK(!st.extra.pending && st.extra.out_frames == produced);
               CHECK(heap_calls == before);
               for (c = 0; c < ch; c++)
                  for (i = 0; i < produced; i++)
                  {
                     if (lane) CHECK(st.extra.out_i[i * ch + c] == expected_i[c][i]);
                     else CHECK(memcmp(&st.extra.out_f[i * ch + c], &expected_f[c][i], sizeof(float)) == 0);
                  }
               for (tail = produced * ch * (lane ? sizeof(int16_t) : sizeof(float)); tail < bytes; tail++)
                  CHECK(((unsigned char*)(lane ? (void*)st.extra.out_i : (void*)st.extra.out_f))[tail] == 0xa5);
            }
            for (pair = 0; pair < pairs; pair++)
            {
               if (lane) sinc_resampler_int16_free(reference[pair]);
               else sinc_resampler.free(reference[pair]);
            }
            audio_driver_extra_free(&st);
            st.resampler->free(st.resampler_data);
         }
}

static void check_scratch_lifecycle(void)
{
   static audio_driver_state_t st;
   const unsigned layouts[] = {2, 4, 2, 1, 2, 5, 2};
   unsigned integer, step, slot, i, ch, pass;
   for (integer = 0; integer < 2; integer++)
   {
      memset(&st, 0, sizeof(st));
      st.resampler = &sinc_resampler;
      st.resampler_quality = RESAMPLER_QUALITY_NORMAL;
      st.src_ratio_orig = 1;
      st.resampler_int16_free = sinc_resampler_int16_free;
      st.resampler_int16_process = sinc_resampler_int16_process;
      for (step = 0; step < sizeof(layouts) / sizeof(layouts[0]); step++)
      {
         unsigned ch = layouts[step];
         unsigned before = heap_calls;
         CHECK(audio_driver_extra_prepare(&st, ch, step, 32, !integer, integer));
         CHECK(heap_calls - before == (ch == 2 ? 4 : 6));
         CHECK((st.extra.pair_in != NULL) == (ch != 2 && !integer));
         CHECK((st.extra.pair_out != NULL) == (ch != 2 && !integer));
         CHECK((st.extra.pair_in_i != NULL) == (ch != 2 && integer));
         CHECK((st.extra.pair_out_i != NULL) == (ch != 2 && integer));
         memset(st.extra.in_f, 0, 32 * ch * sizeof(float));
         memset(st.extra.in_i, 0, 32 * ch * sizeof(int16_t));
         st.extra.pending = true;
         audio_driver_extra_resample(&st, 1.25, 32, false, integer);
         CHECK(!st.extra.pending && st.extra.out_frames <= st.extra.cap_out);
         for (i = 0; i < st.extra.out_frames * ch; i++)
         {
            if (integer) CHECK(st.extra.out_i[i] == 0);
            else CHECK(st.extra.out_f[i] == 0.0f);
         }
         before = heap_calls;
         CHECK(audio_driver_extra_prepare(&st, ch, step, 32, !integer, integer));
         CHECK(heap_calls == before);
         /* Rebuild on a native-lane switch, then grow in that lane. */
         for (pass = 0; pass < 4; pass++)
         {
            bool lane = pass < 2 ? !integer : integer;
            size_t frames = (pass & 1) ? 65 : 33;
            before = heap_calls;
            CHECK(audio_driver_extra_prepare(&st, ch, step, frames, !lane, lane));
            CHECK(heap_calls - before == (ch == 2 ? 4 : 6));
            CHECK((st.extra.pair_in != NULL) == (ch != 2 && !lane));
            CHECK((st.extra.pair_out != NULL) == (ch != 2 && !lane));
            CHECK((st.extra.pair_in_i != NULL) == (ch != 2 && lane));
            CHECK((st.extra.pair_out_i != NULL) == (ch != 2 && lane));
            if (ch != 2)
            {
               size_t sample = lane ? sizeof(int16_t) : sizeof(float);
               CHECK(allocation_size(lane ? (void*)st.extra.pair_in_i
                        : (void*)st.extra.pair_in) == frames * 2 * sample);
               CHECK(allocation_size(lane ? (void*)st.extra.pair_out_i
                        : (void*)st.extra.pair_out) == st.extra.cap_out * 2 * sample);
            }
            memset(st.extra.in_f, 0, frames * ch * sizeof(float));
            memset(st.extra.in_i, 0, frames * ch * sizeof(int16_t));
            st.extra.pending = true;
            before = heap_calls;
            audio_driver_extra_resample(&st, 1.25, frames, false, lane);
            CHECK(!st.extra.pending && st.extra.out_frames <= st.extra.cap_out);
            for (i = 0; i < st.extra.out_frames * ch; i++)
            {
               if (lane) CHECK(st.extra.out_i[i] == 0);
               else CHECK(st.extra.out_f[i] == 0.0f);
            }
            CHECK(audio_driver_extra_prepare(&st, ch, step, frames, !lane, lane));
            CHECK(heap_calls == before);
         }
      }
      audio_driver_extra_free(&st);
      for (ch = 1; ch <= 5; ch++)
         for (slot = 1; slot <= (ch == 2 ? 4 : 6); slot++)
         {
            fail_at = heap_calls + slot;
            CHECK(!audio_driver_extra_prepare(&st, ch, 0, 32, !integer, integer));
            fail_at = 0;
            CHECK(live_allocations() == 0);
            CHECK(st.extra.channels == 0 && st.extra.nres == 0);
            CHECK(!st.extra.in_f && !st.extra.in_i && !st.extra.out_f && !st.extra.out_i);
            CHECK(!st.extra.pair_in && !st.extra.pair_out && !st.extra.pair_in_i && !st.extra.pair_out_i);
            CHECK(audio_driver_extra_prepare(&st, ch, 0, 32, !integer, integer));
            audio_driver_extra_free(&st);
         }
   }
}

static void check_size_limits(void)
{
   static audio_driver_state_t st;
   const size_t counts[] = {
      (size_t)-1, ((size_t)-1 - 1024) / 4 + 1,
      (size_t)-1 / (8 * sizeof(float)) + 1,
      ((size_t)-1 / (8 * sizeof(float)) - 1024) / 4 + 1
   };
   const unsigned channels[] = {0, 9, (unsigned)-1};
   unsigned lane, i, before;
   for (lane = 0; lane < 2; lane++)
   {
      memset(&st, 0, sizeof(st));
      st.resampler = &sinc_resampler;
      st.resampler_quality = RESAMPLER_QUALITY_NORMAL;
      st.src_ratio_orig = 1;
      st.resampler_int16_free = sinc_resampler_int16_free;
      st.resampler_int16_process = sinc_resampler_int16_process;
      before = heap_calls;
      CHECK(!audio_driver_extra_prepare(&st, 0, 0, 32, !lane, lane));
      CHECK(heap_calls == before && live_allocations() == 0);
      for (i = 0; i < sizeof(channels) / sizeof(channels[0]); i++)
      {
         CHECK(audio_driver_extra_prepare(&st, 2, 0, 32, !lane, lane));
         before = heap_calls;
         CHECK(!audio_driver_extra_prepare(&st, channels[i], 0, 32, !lane, lane));
         CHECK(heap_calls == before);
         CHECK(st.extra.channels == 0 && live_allocations() == 0);
      }
      for (i = 0; i < sizeof(counts) / sizeof(counts[0]); i++)
      {
         CHECK(audio_driver_extra_prepare(&st, 8, 0, 32, !lane, lane));
         /* Keep the test allocator from attempting enormous requests if
          * this fixture is run against code without the guards. */
         before = heap_calls;
         fail_at = before + 1;
         CHECK(!audio_driver_extra_prepare(&st, 8, 0, counts[i], !lane, lane));
         fail_at = 0;
         CHECK(heap_calls == before);
         CHECK(st.extra.channels == 0 && live_allocations() == 0);
      }
      CHECK(audio_driver_extra_prepare(&st, 8, 0, 32, !lane, lane));
      before = heap_calls;
      st.output_samples_buf_length = (size_t)-1;
      st.output_samples_int16_length = (size_t)-1;
      CHECK(!audio_driver_extra_prepare(&st, 8, 0, 32, !lane, lane));
      CHECK(heap_calls == before);
      CHECK(st.extra.channels == 0 && live_allocations() == 0);
      st.output_samples_buf_length = 0;
      st.output_samples_int16_length = 0;
      CHECK(audio_driver_extra_prepare(&st, 8, 0, 32, !lane, lane));
      before = heap_calls;
      CHECK(audio_driver_extra_prepare(&st, 8, 0, 32, !lane, lane));
      CHECK(heap_calls == before);
      audio_driver_extra_free(&st);
   }
}

static void check_hq_policy(void)
{
   static audio_driver_state_t st;
   static float input[64 * 2], actual[1024 * 2], expected[1024 * 2];
   const double ratios[] = {1, 1.999, 2, 4, 8};
   unsigned enabled, r, i;
   memset(&st, 0, sizeof(st));
   st.resampler_quality = RESAMPLER_QUALITY_NORMAL;
   for (i = 0; i < 64 * 2; i++) input[i] = (int)(i % 31) / 32.0f;
   for (enabled = 0; enabled < 2; enabled++)
      for (r = 0; r < sizeof(ratios) / sizeof(ratios[0]); r++)
      {
         struct resampler_data a, b;
         void *reference;
         st.src_ratio_orig = ratios[r];
         CHECK(audio_driver_resampler_realloc(&st, enabled));
         CHECK(st.resampler_hq == (enabled && ratios[r] >= 2));
         reference = sinc_resampler_init_hq(ratios[r], RESAMPLER_QUALITY_NORMAL,
               EXTRA_TEST_SIMD, enabled);
         if (!reference) exit(2);
         a.data_in = input; a.data_out = actual; a.input_frames = 64;
         a.output_frames = 0; a.ratio = ratios[r];
         b = a; b.data_out = expected;
         st.resampler->process(st.resampler_data, &a);
         sinc_resampler.process(reference, &b);
         CHECK(a.output_frames == b.output_frames);
         CHECK(memcmp(actual, expected, a.output_frames * 2 * sizeof(float)) == 0);
         sinc_resampler.free(reference);
      }
   /* The fallback must preserve the resolved policy; failed creation and
    * another backend must not leave software HQ marked active. */
   CHECK(audio_driver_resampler_realloc(&st, st.resampler_hq));
   CHECK(st.resampler_hq);
   strcpy(st.resampler_ident, "fail");
   CHECK(!audio_driver_resampler_realloc(&st, true));
   CHECK(!st.resampler_hq && !st.resampler_data);
   strcpy(st.resampler_ident, "other");
   CHECK(audio_driver_resampler_realloc(&st, true));
   CHECK(!st.resampler_hq);
   st.resampler_ident[0] = '\0';
   CHECK(audio_driver_resampler_realloc(&st, true));
   CHECK(st.resampler_hq);
   CHECK(audio_driver_resampler_realloc(&st, false));
   CHECK(!st.resampler_hq);
   st.resampler->free(st.resampler_data);

   memset(&audio_driver_st, 0, sizeof(audio_driver_st));
   audio_driver_st.src_ratio_orig = 2;
   audio_driver_st.resampler_quality = RESAMPLER_QUALITY_NORMAL;
   CHECK(audio_driver_resampler_realloc(&audio_driver_st, true));
   audio_driver_st.resampler_data_int16 = audio_driver_int16_resampler_new(&audio_driver_st);
   audio_driver_st.resampler_int16_free = sinc_resampler_int16_free;
   CHECK(audio_driver_st.resampler_hq && audio_driver_st.resampler_data_int16);
   audio_driver_deinit_resampler();
   CHECK(!audio_driver_st.resampler_hq && !audio_driver_st.resampler_data
         && !audio_driver_st.resampler_data_int16 && !audio_driver_st.resampler);
}

static void check_wide_stereo(void)
{
   static const uint32_t special[] = {
      0, 0x80000000u, 0x7f800000u, 0xff800000u, 0x7fc12345u,
      0xffc12345u, 0x7f7fffffu, 0xff7fffffu, 1, 0x80000001u
   };
   union { float f[2048]; int16_t i[4096]; uint8_t bytes[8192]; } input, saved, reference;
   union { float align; uint8_t bytes[8192 + 32]; } output;
   const unsigned pc = AUDIO_PIPE_CANON_CHANNELS;
   const size_t pass = AUDIO_PIPE_SLICE_INT16S / pc;
   unsigned from, to, base;
   unsigned before = heap_calls;
   for (from = 0; from < 2; from++)
      for (to = 0; to < 2; to++)
         for (base = 0; base < 65536; )
         {
            size_t frames = (65536 - base) / 2;
            size_t sample = to ? sizeof(float) : sizeof(int16_t);
            size_t f, c;
            if (frames > pass) frames = pass;
            memset(&input, 0x35, sizeof(input));
            for (f = 0; f < frames * 2; f++)
            {
               int32_t value = (int32_t)(base + f) - 32768;
               if (!from) input.i[f] = (int16_t)value;
               else if (base + f < sizeof(special) / sizeof(special[0]))
                  memcpy(&input.f[f], &special[base + f], sizeof(float));
               else input.f[f] = (value + 0.5f) / 32768.0f;
            }
            memcpy(&saved, &input, sizeof(input));
            if (from == to) memcpy(&reference, &input, sizeof(input));
            else if (to) convert_s16_to_float(reference.f, input.i, frames * 2, 1.0f);
            else convert_float_to_s16(reference.i, input.f, frames * 2);
            memset(&output, 0xa5, sizeof(output));
            audio_driver_pipe_widen_stereo(output.bytes + 16, &input, frames, pc, from, to);
            CHECK(!memcmp(&input, &saved, sizeof(input)));
            for (f = 0; f < frames; f++)
            {
               CHECK(!memcmp(output.bytes + 16 + f * pc * sample,
                        reference.bytes + f * 2 * sample, 2 * sample));
               for (c = 2 * sample; c < pc * sample; c++)
                  CHECK(output.bytes[16 + f * pc * sample + c] == 0);
            }
            for (f = 0; f < 16; f++) CHECK(output.bytes[f] == 0xa5);
            for (f = 16 + frames * pc * sample; f < sizeof(output); f++)
               CHECK(output.bytes[f] == 0xa5);
            base += (unsigned)frames * 2;
         }
   CHECK(heap_calls == before);
}

static recording_state_t record_state;
static size_t record_made, record_limit, record_calls;
static const int16_t *record_expected;
static const void *record_direct;
recording_state_t *recording_state_get_ptr(void) { return &record_state; }
static bool record_capture(void *data, const struct record_audio_data *io)
{
   CHECK(data == &record_state);
   CHECK(io->frames <= record_limit);
   if (record_direct) CHECK(io->data == record_direct);
   CHECK(!memcmp(io->data, record_expected + record_made * record_state.channels,
            io->frames * record_state.channels * sizeof(int16_t)));
   record_made += io->frames;
   record_calls++;
   return true;
}
static void check_record_chunks(void)
{
   static const unsigned layouts[] = { AUDIO_LAYOUT_STEREO, AUDIO_LAYOUT_5POINT1, AUDIO_LAYOUT_7POINT1 };
   static const size_t sizes[] = {1, 31, 1024, 5000};
   static float input_f[5000 * 8];
   static int16_t input_i[5000 * 8], narrow[5000 * 8], expected[5000 * 8];
   static audio_driver_state_t st;
   static record_driver_t driver;
   unsigned from, to, lane, size, before;
   size_t i;
   driver.push_audio = record_capture;
   record_state.driver = &driver; record_state.data = &record_state;
   for (i = 0; i < 5000 * 8; i++)
   {
      input_i[i] = (int16_t)((int)(i * 7919 % 65536) - 32768);
      input_f[i] = input_i[i] / 16384.0f;
   }
   for (from = 0; from < 3; from++)
      for (to = 0; to < 3; to++)
         for (lane = 0; lane < 2; lane++)
         {
            unsigned channels = audio_layout_channels(layouts[from]);
            const void *input = lane ? (const void*)input_f : (const void*)input_i;
            record_state.layout = layouts[to];
            record_state.channels = audio_layout_channels(layouts[to]);
            record_direct = !lane && from == to ? input : NULL;
            record_expected = expected;
            memset(&st, 0, sizeof(st));
            for (size = 0; size < 4; size++)
            {
               size_t frames = sizes[size];
               record_made = record_calls = 0;
               record_limit = record_direct ? frames : 1024;
               if (lane) convert_float_to_s16(narrow, input_f, frames * channels);
               audio_layout_remap_s16(expected, layouts[to], lane ? narrow : input_i, layouts[from], frames);
               audio_driver_record_push(&st, input, frames, channels, layouts[from], lane);
               CHECK(record_made == frames);
               CHECK(record_calls == (record_direct ? 1 : (frames + 1023) / 1024));
               CHECK(st.record_remap_frames <= 1024 * (channels > record_state.channels ? channels : record_state.channels));
               if (record_direct) CHECK(!st.record_remap);
            }
            before = realloc_calls;
            record_made = record_calls = 0;
            audio_driver_record_push(&st, input, 5000, channels, layouts[from], lane);
            CHECK(realloc_calls == before && record_made == 5000);
            free(st.record_remap);
         }
   memset(&st, 0, sizeof(st));
   record_state.layout = AUDIO_LAYOUT_STEREO; record_state.channels = 2;
   record_direct = NULL; record_made = record_calls = 0;
   record_fail_alloc = true;
   audio_driver_record_push(&st, input_f, 5000, 2, AUDIO_LAYOUT_STEREO, true);
   CHECK(!record_calls && !st.record_remap && !st.record_remap_frames);
   record_fail_alloc = false;
   convert_float_to_s16(expected, input_f, 10000);
   audio_driver_record_push(&st, input_f, 5000, 2, AUDIO_LAYOUT_STEREO, true);
   CHECK(record_made == 5000);
   free(st.record_remap);
   memset(&st, 0, sizeof(st));
   record_state.data = NULL;
   before = realloc_calls; record_calls = 0;
   audio_driver_record_push(&st, input_f, 5000, 2, AUDIO_LAYOUT_STEREO, true);
   CHECK(!record_calls && realloc_calls == before);
   memset(&record_state, 0, sizeof(record_state));
}

int main(void)
{
   check_record_chunks();
   check_wide_stereo();
   check_lane(0);
   check_lane(1);
   check_bypass();
   check_direct_bypass();
   check_direct_pair();
   check_multichannel_alignment(false);
   check_multichannel_alignment(true);
   check_scratch_lifecycle();
   check_size_limits();
   check_hq_policy();
   CHECK(live_allocations() == 0);
   printf("extra capacity: %u failures\n", failures);
   return failures != 0;
}
