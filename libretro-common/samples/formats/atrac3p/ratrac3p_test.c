/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (ratrac3p_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* ratrac3p against a reference decoder's output for a stereo stream
 * (tonal components, gain points in both channels, explicit and
 * all-band swap and negate masks), a mono one and a synthetic one for
 * the second channel's tonal coding. Then the s16
 * output, the fmt chunk, bad frames, reset, and garbage. */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <formats/ratrac3p.h>

#include "ratrac3p_test_data.h"

static int failures = 0;

static void CHECK(int cond, const char *fmt, ...)
{
   va_list ap;
   if (cond)
      return;
   va_start(ap, fmt);
   printf("FAIL: ");
   vprintf(fmt, ap);
   printf("\n");
   va_end(ap);
   failures++;
}

static const uint8_t *stream_of(unsigned ch)
{
   return ch == 2 ? stream_st : stream_mo;
}

static unsigned ba_of(unsigned ch)
{
   return ch == 2 ? STEREO_BA : MONO_BA;
}

static unsigned n_of(unsigned ch)
{
   return ch == 2 ? STEREO_N : MONO_N;
}

static double compare(unsigned ch, const uint8_t *s, unsigned ba,
      unsigned n, const float *ref, unsigned nref, const char *name)
{
   static float   pcm[GHA_N * RATRAC3P_SAMPLES * 2];
   unsigned       f, i;
   ratrac3p_t    *d  = ratrac3p_new(ch, ba);
   double         err = 0.0, sig = 0.0, worst = 0.0;

   CHECK(d != NULL, "%s: new", name);
   if (!d)
      return 0.0;
   for (f = 0; f < n; f++)
   {
      int r = ratrac3p_decode_float(d, s + f * ba, ba,
            pcm + f * RATRAC3P_SAMPLES * ch);
      CHECK(r == RATRAC3P_SAMPLES, "%s: frame %u returned %d", name, f, r);
   }
   for (i = 0; i < nref * ch; i++)
   {
      unsigned at = (i / ch) * STREAM_STEP * ch + i % ch;
      double   a  = pcm[at], b = ref[i];
      double   e  = fabs(a - b);
      err += (a - b) * (a - b);
      sig += b * b;
      if (e > worst)
         worst = e;
   }
   ratrac3p_free(d);
   CHECK(worst < 1e-5, "%s: worst sample error %g", name, worst);
   return 10.0 * log10(sig / (err > 0.0 ? err : 1e-30));
}

static void lane_reference(void)
{
   double a = compare(2, stream_st, STEREO_BA, STEREO_N, &ref_st[0][0],
         sizeof(ref_st) / sizeof(ref_st[0]), "stereo");
   double b = compare(1, stream_mo, MONO_BA, MONO_N, &ref_mo[0][0],
         sizeof(ref_mo) / sizeof(ref_mo[0]), "mono");
   double c = compare(2, stream_gha, GHA_BA, GHA_N, &ref_gha[0][0],
         sizeof(ref_gha) / sizeof(ref_gha[0]), "tonal");
   printf("stereo %.1f dB, mono %.1f dB, tonal %.1f dB against the reference\n",
         a, b, c);
   CHECK(a > 100.0 && b > 100.0 && c > 100.0, "reference match");
}

/* s16 is the float output times 32768, rounded and clamped */
static void lane_s16(void)
{
   static float   pf[RATRAC3P_SAMPLES * 2];
   static int16_t ps[RATRAC3P_SAMPLES * 2];
   unsigned       ch;
   for (ch = 1; ch <= 2; ch++)
   {
      unsigned    ba = ba_of(ch), f, i, bad = 0;
      ratrac3p_t *df = ratrac3p_new(ch, ba);
      ratrac3p_t *ds = ratrac3p_new(ch, ba);
      for (f = 0; f < n_of(ch); f++)
      {
         ratrac3p_decode_float(df, stream_of(ch) + f * ba, ba, pf);
         ratrac3p_decode_s16(ds, stream_of(ch) + f * ba, ba, ps);
         for (i = 0; i < RATRAC3P_SAMPLES * ch; i++)
         {
            double v = pf[i] * 32768.0;
            long   e;
            if (v >= 32767.0)
               e = 32767;
            else if (v <= -32768.0)
               e = -32768;
            else
               e = (long)(v < 0 ? v - 0.5 : v + 0.5);
            if (labs(e - ps[i]) > 1)
               bad++;
         }
      }
      CHECK(bad == 0, "s16 %u ch: %u samples off", ch, bad);
      ratrac3p_free(df);
      ratrac3p_free(ds);
   }
}

static void put16(uint8_t *p, unsigned v)
{
   p[0] = (uint8_t)v;
   p[1] = (uint8_t)(v >> 8);
}

static void put32(uint8_t *p, unsigned long v)
{
   put16(p, (unsigned)(v & 0xFFFF));
   put16(p + 2, (unsigned)(v >> 16));
}

static void make_fmt(uint8_t *fmt, unsigned tag, unsigned ch, unsigned ba)
{
   static const uint8_t guid[16] = {
      0xBF, 0xAA, 0x23, 0xE9, 0x58, 0xCB, 0x71, 0x44,
      0xA1, 0x19, 0xFF, 0xFA, 0x01, 0xE4, 0xCE, 0x62
   };
   memset(fmt, 0, 52);
   put16(fmt, tag);
   put16(fmt + 2, ch);
   put32(fmt + 4, 44100);
   put32(fmt + 8, (unsigned long)ba * 44100 / 2048);
   put16(fmt + 12, ba);
   put16(fmt + 16, 34);
   memcpy(fmt + 24, guid, 16);
}

static void lane_fmt(void)
{
   uint8_t         fmt[52];
   ratrac3p_info_t info;

   make_fmt(fmt, 0xFFFE, 2, 376);
   CHECK(ratrac3p_parse_wave_fmt(fmt, 52, &info), "fmt: stereo");
   CHECK(info.channels == 2 && info.block_align == 376
         && info.sample_rate == 44100, "fmt: stereo fields");
   CHECK(info.bitrate == 376u * 44100 / 2048 * 8, "fmt: bitrate %u",
         info.bitrate);
   make_fmt(fmt, 0xFFFE, 8, 2048);
   CHECK(ratrac3p_parse_wave_fmt(fmt, 52, &info) && info.channels == 8,
         "fmt: 7.1");

   make_fmt(fmt, 0x270, 2, 376);
   CHECK(!ratrac3p_parse_wave_fmt(fmt, 52, &info), "fmt: ATRAC3 tag");
   make_fmt(fmt, 0xFFFE, 2, 376);
   fmt[24] ^= 1;
   CHECK(!ratrac3p_parse_wave_fmt(fmt, 52, &info), "fmt: other GUID");
   make_fmt(fmt, 0xFFFE, 9, 376);
   CHECK(!ratrac3p_parse_wave_fmt(fmt, 52, &info), "fmt: nine channels");
   make_fmt(fmt, 0xFFFE, 0, 376);
   CHECK(!ratrac3p_parse_wave_fmt(fmt, 52, &info), "fmt: no channels");
   make_fmt(fmt, 0xFFFE, 2, 376);
   CHECK(!ratrac3p_parse_wave_fmt(fmt, 39, &info), "fmt: short");
   CHECK(!ratrac3p_parse_wave_fmt(NULL, 52, &info), "fmt: NULL");

   CHECK(ratrac3p_new(0, 376) == NULL, "new: no channels");
   CHECK(ratrac3p_new(9, 376) == NULL, "new: nine channels");
   CHECK(ratrac3p_new(2, 0) == NULL, "new: no block");
}

/* A frame that does not parse gives silence and -1 and leaves the
 * history alone: what follows decodes as if it were not there */
static void lane_bad_frame(void)
{
   static float a[RATRAC3P_SAMPLES * 2], b[RATRAC3P_SAMPLES * 2];
   static uint8_t bad[STEREO_BA];
   unsigned ch;
   for (ch = 1; ch <= 2; ch++)
   {
      unsigned       ba = ba_of(ch), at = 1, f, i;
      const uint8_t *s  = stream_of(ch);
      ratrac3p_t    *d1 = ratrac3p_new(ch, ba);
      ratrac3p_t    *d2 = ratrac3p_new(ch, ba);
      int            r;

      memcpy(bad, s + at * ba, ba);
      bad[0] ^= 0x80;              /* the reserved bit */
      for (f = 0; f < n_of(ch); f++)
      {
         if (f == at)
         {
            r = ratrac3p_decode_float(d2, bad, ba, b);
            CHECK(r == -1, "bad frame: returned %d", r);
            for (i = 0; i < RATRAC3P_SAMPLES * ch; i++)
               if (b[i] != 0.0f)
                  break;
            CHECK(i == RATRAC3P_SAMPLES * ch, "bad frame: not silent");
            continue;
         }
         ratrac3p_decode_float(d1, s + f * ba, ba, a);
         ratrac3p_decode_float(d2, s + f * ba, ba, b);
         CHECK(!memcmp(a, b, sizeof(float) * RATRAC3P_SAMPLES * ch),
               "bad frame: %u ch frame %u differs", ch, f);
      }
      r = ratrac3p_decode_float(d1, s, ba - 1, a);
      CHECK(r == -1, "short frame: returned %d", r);
      r = ratrac3p_decode_float(d1, NULL, ba, a);
      CHECK(r == -1, "NULL frame: returned %d", r);
      ratrac3p_free(d1);
      ratrac3p_free(d2);
   }
}

static void lane_reset(void)
{
   static float a[RATRAC3P_SAMPLES * 2], b[RATRAC3P_SAMPLES * 2];
   ratrac3p_t *d1 = ratrac3p_new(2, STEREO_BA);
   ratrac3p_t *d2 = ratrac3p_new(2, STEREO_BA);
   unsigned    f;
   for (f = 0; f < STEREO_N; f++)
      ratrac3p_decode_float(d1, stream_st + f * STEREO_BA, STEREO_BA, a);
   ratrac3p_reset(d1);
   for (f = 0; f < STEREO_N; f++)
   {
      ratrac3p_decode_float(d1, stream_st + f * STEREO_BA, STEREO_BA, a);
      ratrac3p_decode_float(d2, stream_st + f * STEREO_BA, STEREO_BA, b);
      CHECK(!memcmp(a, b, sizeof(a)), "reset: frame %u differs", f);
   }
   ratrac3p_free(d1);
   ratrac3p_free(d2);
}

/* Garbage and mutations must not read or write out of bounds (run
 * under a sanitizer) and must keep the output finite */
static unsigned long rng_state = 12345;
static unsigned rng(void)
{
   rng_state = rng_state * 1103515245UL + 12345UL;
   return (unsigned)(rng_state >> 16) & 0x7FFF;
}

static void lane_garbage(void)
{
   static float   pcm[RATRAC3P_SAMPLES * 4];
   static uint8_t frame[STEREO_BA * 2];
   unsigned       mode;
   for (mode = 0; mode < 3; mode++)
   {
      /* mono, stereo, and stereo + mono + mono over the stereo frames */
      unsigned    ch = mode ? (mode == 1 ? 2 : 4) : 1;
      unsigned    ba = mode ? STEREO_BA : MONO_BA;
      ratrac3p_t *d  = ratrac3p_new(ch, ba);
      unsigned    n, ok = 0, nonfinite = 0;
      for (n = 0; n < 2000; n++)
      {
         unsigned i, k = rng() % 3;
         if (k == 0)
            for (i = 0; i < ba; i++)
               frame[i] = (uint8_t)rng();
         else
         {
            const uint8_t *src = mode ? stream_st + (n % STEREO_N) * ba
               : stream_mo + (n % MONO_N) * ba;
            memcpy(frame, src, ba);
            for (i = 0; i < 1 + rng() % 8; i++)
               frame[(rng() << 15 | rng()) % ba] ^= (uint8_t)(1u << (rng() % 8));
            if (k == 2)
               frame[0] &= 0x7F;
         }
         if (ratrac3p_decode_float(d, frame, ba, pcm) > 0)
            ok++;
         for (i = 0; i < RATRAC3P_SAMPLES * ch; i++)
            if (!(pcm[i] == pcm[i]) || fabs(pcm[i]) > 1e30)
               nonfinite++;
      }
      CHECK(nonfinite == 0, "garbage: %u non-finite samples", nonfinite);
      printf("garbage mode %u: %u of 2000 frames decoded\n", mode, ok);
      ratrac3p_free(d);
   }
}

/* Mono, one coded QU, master code-table mode 3 with no indices.
 * Two frames decode to silence in the reference decoder. */
static void lane_master_table_default(void)
{
   static const uint8_t frame[16] = {
      0x00, 0x04, 0x00, 0xC0, 0x30
   };
   float pcm[RATRAC3P_SAMPLES];
   int16_t pcm16[RATRAC3P_SAMPLES];
   ratrac3p_t *d = ratrac3p_new(1, sizeof(frame));
   unsigned i, f;
   CHECK(d != NULL, "master default: allocate");
   if (!d)
      return;
   for (f = 0; f < 2; f++)
   {
      CHECK(ratrac3p_decode_float(d, frame, sizeof(frame), pcm)
            == RATRAC3P_SAMPLES, "master default: frame %u", f);
      for (i = 0; i < RATRAC3P_SAMPLES; i++)
         if (pcm[i] != 0.0f)
            break;
      CHECK(i == RATRAC3P_SAMPLES, "master default: not silent");
   }
   ratrac3p_reset(d);
   CHECK(ratrac3p_decode_s16(d, frame, sizeof(frame), pcm16)
         == RATRAC3P_SAMPLES, "master default: s16");
   for (i = 0; i < RATRAC3P_SAMPLES; i++)
      if (pcm16[i] != 0)
         break;
   CHECK(i == RATRAC3P_SAMPLES, "master default: s16 not silent");
   ratrac3p_free(d);
}

/* Two declared subbands, one coded subband. Stereo flags cover only
 * the coded band; window shapes still cover both declared bands. */
static void coded_band_flags(void)
{
   static const uint8_t frame[31] = {
      0x28, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x19, 0xB2, 0x06, 0xD9, 0x81, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
   };
   int16_t pcm[2 * RATRAC3P_SAMPLES];
   ratrac3p_t *d = ratrac3p_new(2, sizeof(frame));
   unsigned f, i;
   int audible = 0;
   CHECK(d != NULL, "coded bands: allocate");
   if (!d)
      return;
   for (f = 0; f < 3; f++)
   {
      CHECK(ratrac3p_decode_s16(d, frame, sizeof(frame), pcm)
            == RATRAC3P_SAMPLES, "coded bands: frame %u", f);
      for (i = 0; i < RATRAC3P_SAMPLES; i++)
      {
         if (pcm[2 * i] != 0)
            audible = 1;
         if (pcm[2 * i] != pcm[2 * i + 1])
            break;
      }
      CHECK(i == RATRAC3P_SAMPLES, "coded bands: copied stereo differs");
   }
   CHECK(audible, "coded bands: no decoded audio");
   ratrac3p_free(d);
}

int main(void)
{
   lane_master_table_default();
   coded_band_flags();
   lane_reference();
   lane_s16();
   lane_fmt();
   lane_bad_frame();
   lane_reset();
   lane_garbage();
   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("ratrac3p: all tests passed\n");
   return 0;
}
