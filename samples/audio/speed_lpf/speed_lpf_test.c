/* Copyright (C) 2026 - The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "../../../audio/audio_speed_lpf.h"

static unsigned failures, allocations, cases;
static int guarded;
void *__real_malloc(size_t n);
void *__real_calloc(size_t n, size_t s);
void *__real_realloc(void *p, size_t n);
void __real_free(void *p);
void *__wrap_malloc(size_t n)
{ if (guarded) allocations++; return __real_malloc(n); }
void *__wrap_calloc(size_t n, size_t s)
{ if (guarded) allocations++; return __real_calloc(n, s); }
void *__wrap_realloc(void *p, size_t n)
{ if (guarded) allocations++; return __real_realloc(p, n); }
void __wrap_free(void *p)
{ if (guarded) allocations++; __real_free(p); }
#define CHECK(x) do { if (!(x)) { failures++; \
   if (failures < 20) fprintf(stderr, "line %u: %s\n", (unsigned)__LINE__, #x); } } while (0)

#define N 21000
static float fa[N * AUDIO_SPEED_LPF_CHANNELS], fb[N * AUDIO_SPEED_LPF_CHANNELS];
static int16_t ia[N * AUDIO_SPEED_LPF_CHANNELS], ib[N * AUDIO_SPEED_LPF_CHANNELS];

static void contracts(void)
{
   audio_speed_lpf_t s, old;
   union { float f[8]; uint32_t u[8]; } dry, copy;
   unsigned j;
   double bad;
   uint64_t bits = UINT64_C(0x7ff8000000000001);
   memcpy(&bad, &bits, sizeof(bad));
   memset(&s, 0x55, sizeof(s)); old = s;
   CHECK(!audio_speed_lpf_init(&s, 7999, 2, true));
   CHECK(!audio_speed_lpf_init(&s, 192001, 2, true));
   CHECK(!audio_speed_lpf_init(&s, 48000, 12, true));
   CHECK(!audio_speed_lpf_init(&s, 48000, 0, true));
   CHECK(memcmp(&s, &old, sizeof(s)) == 0);
   CHECK(audio_speed_lpf_init(&s, 48000, 2, true)); old = s;
   CHECK(!audio_speed_lpf_set(&s, true, bad));
   CHECK(!audio_speed_lpf_set(&s, true, -1));
   CHECK(!audio_speed_lpf_set(&s, true, 0));
   CHECK(!audio_speed_lpf_process(&s, NULL, 1));
   CHECK(!audio_speed_lpf_process(&s, (char*)fa + 1, 1));
   CHECK(!audio_speed_lpf_process(&s, fa, SIZE_MAX));
   CHECK(audio_speed_lpf_process(&s, NULL, 0));
   CHECK(memcmp(&s, &old, sizeof(s)) == 0);
   for (j = 0; j < 8; j++) dry.u[j] = UINT32_C(0x7fc00001) + j;
   dry.u[0] = UINT32_C(0x80000000); copy = dry;
   CHECK(audio_speed_lpf_process(&s, dry.f, 4));
   CHECK(memcmp(&dry, &copy, sizeof(dry)) == 0);
   CHECK(memcmp(&s, &old, sizeof(s)) == 0);
   CHECK(audio_speed_lpf_set(&s, true, 1000));
   old = s;
   CHECK(audio_speed_lpf_process(&s, NULL, 0));
   CHECK(memcmp(&s, &old, sizeof(s)) == 0);
   memset(fa, 0, sizeof(fa));
   CHECK(audio_speed_lpf_process(&s, fa, s.ramp_frames));
   CHECK(s.wet == 65536);
   CHECK(audio_speed_lpf_set(&s, true, 2000));
   CHECK(audio_speed_lpf_process(&s, fa, 1));
   CHECK(s.alpha_progress == 1 && s.alpha == s.alpha_start);
   CHECK(audio_speed_lpf_set(&s, true, 2000));
   CHECK(s.alpha_progress == 1);
   CHECK(audio_speed_lpf_process(&s, fa, s.ramp_frames - 2));
   CHECK(s.alpha_progress == s.ramp_frames - 1 && s.alpha != s.alpha_target);
   CHECK(audio_speed_lpf_process(&s, fa, 1));
   CHECK(s.alpha == s.alpha_target && s.alpha_f == s.alpha_target_f);
   CHECK(audio_speed_lpf_set(&s, false, 2000));
   CHECK(audio_speed_lpf_process(&s, fa, s.ramp_frames));
   CHECK(audio_speed_lpf_quiescent(&s) && !s.primed);
   old = s;
   CHECK(audio_speed_lpf_process(&s, dry.f, 4));
   CHECK(memcmp(&dry, &copy, sizeof(dry)) == 0);
   CHECK(memcmp(&s, &old, sizeof(s)) == 0);
   cases++;
}

/* Same source-position controls, radically different callback sizes. */
static void run_chunks(void *data, unsigned rate, unsigned channels,
      bool floating, unsigned chunk)
{
   static const unsigned edges[] = {0, 10001, 10072, 10501, 10701, 11003, 21000};
   static const double cutoff[] = {20, 3000, 500, 500, 6000, 1000};
   static const bool on[] = {true, true, false, true, true, false};
   audio_speed_lpf_t s;
   unsigned e, at, step;
   size_t stride = channels * (floating ? sizeof(float) : sizeof(int16_t));
   CHECK(audio_speed_lpf_init(&s, rate, channels, floating));
   for (e = 0; e < 6; e++)
   {
      CHECK(audio_speed_lpf_set(&s, on[e], cutoff[e]));
      if (e == 4) audio_speed_lpf_reset(&s);
      for (at = edges[e]; at < edges[e+1]; at += step)
      {
         step = edges[e+1] - at;
         if (step > chunk) step = chunk;
         CHECK(audio_speed_lpf_process(&s, (char*)data + at * stride, step));
      }
   }
   CHECK(audio_speed_lpf_quiescent(&s));
}

static void native_cases(unsigned rate, unsigned channels)
{
   audio_speed_lpf_t sf, si;
   unsigned j, n = N * channels;
   uint32_t rng = 1;
   for (j = 0; j < n; j++)
   {
      rng = rng * UINT32_C(1664525) + UINT32_C(1013904223);
      ia[j] = ib[j] = (int16_t)((int)(rng >> 16) - 32768);
      fa[j] = fb[j] = ia[j] / 32768.0f;
   }
   run_chunks(fa, rate, channels, true, N);
   run_chunks(fb, rate, channels, true, 7);
   run_chunks(ia, rate, channels, false, N);
   run_chunks(ib, rate, channels, false, 1);
   CHECK(memcmp(fa, fb, n * sizeof(float)) == 0);
   CHECK(memcmp(ia, ib, n * sizeof(int16_t)) == 0);
   for (j = 0; j < n; j++) CHECK(fabs(fa[j] * 32768.0 - ia[j]) < 1.1);
   CHECK(audio_speed_lpf_init(&sf, rate, channels, true));
   CHECK(audio_speed_lpf_init(&si, rate, channels, false));
   CHECK(audio_speed_lpf_set(&sf, true, 0.1));
   CHECK(audio_speed_lpf_set(&si, true, 20));
   CHECK(sf.alpha == si.alpha && si.alpha > 0 && si.alpha < UINT32_C(1073741824));
   for (j = 0; j < n; j++)
   {
      ia[j] = (j % channels) & 1 ? 32767 : -32768;
      fa[j] = ia[j] / 32768.0f;
   }
   CHECK(audio_speed_lpf_process(&si, ia, N));
   CHECK(audio_speed_lpf_process(&sf, fa, N));
   for (j = 0; j < n; j++)
   {
      int expected = (j % channels) & 1 ? 32767 : -32768;
      CHECK(ia[j] == expected && fa[j] * 32768.0 == expected);
   }
   /* Decay from full scale at the slowest pole, then remain silent. */
   for (j = 0; j < 30; j++)
   {
      memset(ia, 0, n * sizeof(int16_t)); memset(fa, 0, n * sizeof(float));
      CHECK(audio_speed_lpf_process(&si, ia, N));
      CHECK(audio_speed_lpf_process(&sf, fa, N));
   }
   for (j = 0; j < n; j++) CHECK(ia[j] == 0 && fa[j] == 0);
   CHECK(audio_speed_lpf_set(&si, true, rate));
   CHECK(audio_speed_lpf_set(&sf, true, rate * 0.45));
   CHECK(sf.alpha_target == si.alpha_target && si.alpha_target < UINT32_C(1073741824));
   for (j = 0; j < n; j++) ia[j] = (j / channels) & 1 ? 32767 : -32768;
   CHECK(audio_speed_lpf_process(&si, ia, N));
   for (j = 0; j < channels; j++)
   {
      CHECK(si.history.i[0][j] >= -INT64_C(2147483648));
      CHECK(si.history.i[0][j] <= INT64_C(2147418112));
      CHECK(si.history.i[1][j] >= -INT64_C(2147483648));
      CHECK(si.history.i[1][j] <= INT64_C(2147418112));
   }
   cases++;
}

static void response(void)
{
   audio_speed_lpf_t sf, si;
   unsigned j, frequency;
   for (frequency = 1000; frequency <= 4000; frequency *= 4)
   {
      double ef = 0, ei = 0, ex = 0;
      CHECK(audio_speed_lpf_init(&sf, 48000, 1, true));
      CHECK(audio_speed_lpf_init(&si, 48000, 1, false));
      CHECK(audio_speed_lpf_set(&sf, true, 1000));
      CHECK(audio_speed_lpf_set(&si, true, 1000));
      for (j = 0; j < N; j++)
      {
         double x = 0.5 * sin(6.283185307179586 * frequency * j / 48000);
         fa[j] = (float)x; ia[j] = (int16_t)(x * 32768);
         if (j >= 9000) ex += x*x;
      }
      CHECK(audio_speed_lpf_process(&sf, fa, N));
      CHECK(audio_speed_lpf_process(&si, ia, N));
      for (j = 9000; j < N; j++)
      { ef += fa[j]*fa[j]; ei += (ia[j]/32768.0)*(ia[j]/32768.0); }
      if (frequency == 1000)
      { CHECK(fabs(ef/ex - 0.5) < 0.001); CHECK(fabs(ei/ex - 0.5) < 0.001); }
      else { CHECK(ef/ex < 0.02); CHECK(ei/ex < 0.02); }
      cases++;
   }
}

static void reset_cases(void)
{
   audio_speed_lpf_t s, fresh;
   unsigned mode, j;
   for (mode = 0; mode < 2; mode++)
   {
      size_t bytes = 2 * N * (mode ? sizeof(float) : sizeof(int16_t));
      void *a = mode ? (void*)fa : (void*)ia;
      void *b = mode ? (void*)fb : (void*)ib;
      CHECK(audio_speed_lpf_init(&s, 48000, 2, mode != 0));
      CHECK(audio_speed_lpf_set(&s, true, 2000));
      for (j = 0; j < 2*N; j++)
      { fa[j] = 0.5f; ia[j] = 16384; }
      CHECK(audio_speed_lpf_process(&s, a, N));
      CHECK(audio_speed_lpf_set(&s, true, 20));
      CHECK(audio_speed_lpf_process(&s, a, 73));
      audio_speed_lpf_reset(&s);
      CHECK(!s.primed && s.wet == 0 && s.wet_target == 65536);
      CHECK(audio_speed_lpf_init(&fresh, 48000, 2, mode != 0));
      CHECK(audio_speed_lpf_set(&fresh, true, 20));
      memset(a, 0, bytes); memset(b, 0, bytes);
      if (mode) fa[4] = fb[4] = -1.0f;
      else ia[4] = ib[4] = -32768;
      CHECK(audio_speed_lpf_process(&s, a, N));
      CHECK(audio_speed_lpf_process(&fresh, b, N));
      CHECK(memcmp(a, b, bytes) == 0);
      /* Impulse never leaks to the other channel. */
      for (j = 1; j < 2*N; j += 2)
         CHECK(mode ? fa[j] == 0 : ia[j] == 0);
      CHECK(audio_speed_lpf_set(&s, false, 20));
      audio_speed_lpf_reset(&s);
      CHECK(audio_speed_lpf_quiescent(&s));
      cases++;
   }
}

static void separate_output(void)
{
   unsigned mode, j;
   for (mode = 0; mode < 2; mode++)
   {
      audio_speed_lpf_t s, reference, old;
      void *src = mode ? (void*)fa : (void*)ia;
      void *dst = mode ? (void*)fb : (void*)ib;
      size_t sample = mode ? sizeof(float) : sizeof(int16_t);
      for (j = 0; j < N; j++)
      { ia[j] = (j & 1) ? 32767 : -32768; fa[j] = ia[j] / 32768.0f; }
      CHECK(audio_speed_lpf_init(&s, 48000, 1, mode != 0));
      CHECK(audio_speed_lpf_set(&s, true, 1000));
      reference = old = s;
      CHECK(!audio_speed_lpf_process_into(&s, src, (char*)src + sample, 8));
      CHECK(!audio_speed_lpf_process_into(&s, src, NULL, 8));
      CHECK(memcmp(&s, &old, sizeof(s)) == 0);
      CHECK(audio_speed_lpf_process_into(&s, src, dst, 6000));
      CHECK(audio_speed_lpf_set(&s, false, 1000));
      CHECK(audio_speed_lpf_process_into(&s, (char*)src + 6000*sample,
               (char*)dst + 6000*sample, N-6000));
      for (j = 0; j < N; j++)
      {
         int16_t x = (j & 1) ? 32767 : -32768;
         float y = x / 32768.0f;
         CHECK(mode ? fa[j] == y : ia[j] == x);
         if (j == 6000) CHECK(audio_speed_lpf_set(&reference, false, 1000));
         CHECK(audio_speed_lpf_process(&reference, mode ? (void*)&y : (void*)&x, 1));
         CHECK(mode ? fb[j] == y : ib[j] == x);
      }
      CHECK(audio_speed_lpf_quiescent(&s));
      cases++;
   }
}

static void cutoff_policy(void)
{
   static const unsigned rates[] = {8000, 44100, 48000, 96000, 192000};
   unsigned r, step;
   CHECK(audio_speed_lpf_cutoff(7999, 131072) == 0);
   CHECK(audio_speed_lpf_cutoff(192001, 131072) == 0);
   for (r = 0; r < sizeof(rates) / sizeof(rates[0]); r++)
   {
      uint32_t previous = rates[r];
      CHECK(audio_speed_lpf_cutoff(rates[r], 0) == 0);
      CHECK(audio_speed_lpf_cutoff(rates[r], 16384) == 0);
      CHECK(audio_speed_lpf_cutoff(rates[r], 65536) == 0);
      CHECK(audio_speed_lpf_cutoff(rates[r], UINT32_MAX) == 20);
      for (step = 1; step <= 4096; step++)
      {
         uint32_t speed = 65536 + step * 496;
         uint32_t got = audio_speed_lpf_cutoff(rates[r], speed);
         double exact = rates[r] * 0.45 / (speed / 65536.0);
         CHECK(got >= 20 && got <= previous);
         CHECK(got <= exact + 1e-8 && exact - got < 1.00000001);
         previous = got;
      }
      cases++;
   }
   CHECK(audio_speed_lpf_cutoff(48000, 131072) == 10800);
   CHECK(audio_speed_lpf_cutoff(48000, 2097152) == 675);
}

int main(void)
{
   static const unsigned rates[] = {8000, 44100, 48000, 96000, 192000};
   unsigned r, c;
   guarded = 1;
   contracts(); response(); reset_cases(); separate_output(); cutoff_policy();
   for (r = 0; r < sizeof(rates)/sizeof(rates[0]); r++)
      for (c = 1; c <= 11; c++) native_cases(rates[r], c);
   guarded = 0;
   CHECK(allocations == 0);
   printf("native speed LPF: %u cases, %u failures, %u heap calls\n",
         cases, failures, allocations);
   return failures ? 1 : 0;
}
