/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (ratrac3_test.c).
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

/* ratrac3 against a reference decoder's output for two crafted streams:
 * a stereo one (gain control, tonal components in all their codings,
 * the band limits, fewer coded bands, an empty unit) and a joint stereo
 * one (every matrix and weight change, padding, side units of one to
 * four bands). Then the s16 output, the fmt chunk, bad frames, reset,
 * and garbage. */

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <formats/ratrac3.h>

#include "ratrac3_test_data.h"

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

#define FRAMES 12

static double compare(const uint8_t *stream, unsigned block_align,
      bool joint, const float (*ref)[2], unsigned nref, const char *name)
{
   static float pcm[FRAMES * RATRAC3_SAMPLES * 2];
   ratrac3_t   *d = ratrac3_new(2, block_align, joint);
   unsigned     f, i;
   double       err = 0.0, sig = 0.0, worst = 0.0;

   CHECK(d != NULL, "%s: new", name);
   if (!d)
      return 0.0;
   for (f = 0; f < FRAMES; f++)
   {
      int r = ratrac3_decode_float(d, stream + f * block_align, block_align,
            pcm + f * RATRAC3_SAMPLES * 2);
      CHECK(r == RATRAC3_SAMPLES, "%s: frame %u returned %d", name, f, r);
   }
   for (i = 0; i < nref; i++)
   {
      unsigned c;
      for (c = 0; c < 2; c++)
      {
         double a = pcm[i * STREAM_STEP * 2 + c], b = ref[i][c];
         double e = fabs(a - b);
         err += (a - b) * (a - b);
         sig += b * b;
         if (e > worst)
            worst = e;
      }
   }
   ratrac3_free(d);
   CHECK(worst < 2e-5, "%s: worst sample error %g", name, worst);
   return 10.0 * log10(sig / (err > 0.0 ? err : 1e-30));
}

static void lane_reference(void)
{
   double a = compare(stream_a, 384, false, ref_a,
         sizeof(ref_a) / sizeof(ref_a[0]), "stereo");
   double b = compare(stream_b, 192, true, ref_b,
         sizeof(ref_b) / sizeof(ref_b[0]), "joint stereo");
   printf("stereo %.1f dB, joint stereo %.1f dB against the reference\n",
         a, b);
   CHECK(a > 100.0 && b > 100.0, "reference match");
}

/* s16 is the float output times 32768, rounded and clamped */
static void lane_s16(void)
{
   static float   pf[RATRAC3_SAMPLES * 2];
   static int16_t ps[RATRAC3_SAMPLES * 2];
   ratrac3_t *df = ratrac3_new(2, 384, false);
   ratrac3_t *ds = ratrac3_new(2, 384, false);
   unsigned   f, i, clipped = 0, bad = 0;
   for (f = 0; f < FRAMES; f++)
   {
      ratrac3_decode_float(df, stream_a + f * 384, 384, pf);
      ratrac3_decode_s16(ds, stream_a + f * 384, 384, ps);
      for (i = 0; i < RATRAC3_SAMPLES * 2; i++)
      {
         double v = pf[i] * 32768.0;
         long   e;
         if (v >= 32767.0)
         {
            e = 32767;
            clipped++;
         }
         else if (v <= -32768.0)
         {
            e = -32768;
            clipped++;
         }
         else
            e = (long)(v < 0 ? v - 0.5 : v + 0.5);
         if (labs(e - ps[i]) > 1)
            bad++;
      }
   }
   CHECK(bad == 0, "s16: %u samples off", bad);
   CHECK(clipped > 0, "s16: the stereo stream should clip somewhere");
   ratrac3_free(df);
   ratrac3_free(ds);
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

static void make_fmt(uint8_t *fmt, unsigned tag, unsigned ch,
      unsigned ba, unsigned mode)
{
   memset(fmt, 0, 32);
   put16(fmt, tag);
   put16(fmt + 2, ch);
   put32(fmt + 4, 44100);
   put32(fmt + 8, (unsigned long)ba * 44100 / 1024);
   put16(fmt + 12, ba);
   put16(fmt + 16, 14);
   put16(fmt + 18, 1);
   put32(fmt + 20, 0x1000);
   put16(fmt + 24, mode);
   put16(fmt + 26, mode);
   put16(fmt + 28, 1);
}

static void lane_fmt(void)
{
   uint8_t        fmt[32];
   ratrac3_info_t info;

   make_fmt(fmt, 0x270, 2, 192, 1);
   CHECK(ratrac3_parse_wave_fmt(fmt, 32, &info), "fmt: joint");
   CHECK(info.channels == 2 && info.block_align == 192 && info.joint_stereo
         && info.sample_rate == 44100, "fmt: joint fields");
   CHECK(info.bitrate == 192u * 44100 / 1024 * 8, "fmt: bitrate %u",
         info.bitrate);

   make_fmt(fmt, 0x270, 2, 384, 0);
   CHECK(ratrac3_parse_wave_fmt(fmt, 32, &info) && !info.joint_stereo
         && info.block_align == 384, "fmt: stereo");

   make_fmt(fmt, 0x270, 1, 192, 0);
   CHECK(ratrac3_parse_wave_fmt(fmt, 32, &info) && info.channels == 1,
         "fmt: mono");
   /* No extra data: not joint */
   CHECK(ratrac3_parse_wave_fmt(fmt, 16, &info) && !info.joint_stereo,
         "fmt: bare");

   make_fmt(fmt, 0xFFFE, 2, 384, 0);
   CHECK(!ratrac3_parse_wave_fmt(fmt, 32, &info), "fmt: other tag");
   make_fmt(fmt, 0x270, 1, 192, 1);
   CHECK(!ratrac3_parse_wave_fmt(fmt, 32, &info), "fmt: mono joint");
   make_fmt(fmt, 0x270, 2, 191, 0);
   CHECK(!ratrac3_parse_wave_fmt(fmt, 32, &info), "fmt: odd block");
   make_fmt(fmt, 0x270, 3, 384, 0);
   CHECK(!ratrac3_parse_wave_fmt(fmt, 32, &info), "fmt: three channels");
   CHECK(!ratrac3_parse_wave_fmt(fmt, 15, &info), "fmt: short");
   CHECK(!ratrac3_parse_wave_fmt(NULL, 32, &info), "fmt: NULL");

   CHECK(ratrac3_new(0, 384, false) == NULL, "new: no channels");
   CHECK(ratrac3_new(3, 384, false) == NULL, "new: three channels");
   CHECK(ratrac3_new(2, 0, false) == NULL, "new: no block");
   CHECK(ratrac3_new(2, 383, false) == NULL, "new: odd block");
   CHECK(ratrac3_new(1, 192, true) == NULL, "new: mono joint");
}

/* A frame that does not parse gives silence and -1 and leaves the
 * history alone: what follows decodes as if it were not there */
static void lane_bad_frame(void)
{
   static float a[RATRAC3_SAMPLES * 2], b[RATRAC3_SAMPLES * 2];
   unsigned   pass;
   for (pass = 0; pass < 2; pass++)
   {
      const uint8_t *s  = pass ? stream_b : stream_a;
      unsigned       ba = pass ? 192 : 384;
      unsigned       at = pass ? 3 : 4;
      ratrac3_t     *d1 = ratrac3_new(2, ba, pass != 0);
      ratrac3_t     *d2 = ratrac3_new(2, ba, pass != 0);
      uint8_t        bad[384];
      unsigned       f, i;
      int            r;

      memcpy(bad, s + at * ba, ba);
      if (pass)
         bad[ba - 2] &= 0xF3; /* the side unit's id, after the weights */
      else
         bad[0] ^= 0x80;      /* the unit id */

      for (f = 0; f < FRAMES; f++)
      {
         if (f == at)
         {
            r = ratrac3_decode_float(d2, bad, ba, b);
            CHECK(r == -1, "bad frame: returned %d", r);
            for (i = 0; i < RATRAC3_SAMPLES * 2; i++)
               if (b[i] != 0.0f)
                  break;
            CHECK(i == RATRAC3_SAMPLES * 2, "bad frame: not silent");
            continue;
         }
         ratrac3_decode_float(d1, s + f * ba, ba, a);
         ratrac3_decode_float(d2, s + f * ba, ba, b);
         CHECK(!memcmp(a, b, sizeof(a)), "bad frame: frame %u differs", f);
      }
      /* A short buffer is a bad frame */
      r = ratrac3_decode_float(d1, s, ba - 1, a);
      CHECK(r == -1, "short frame: returned %d", r);
      r = ratrac3_decode_float(d1, NULL, ba, a);
      CHECK(r == -1, "NULL frame: returned %d", r);
      ratrac3_free(d1);
      ratrac3_free(d2);
   }
}

static void lane_reset(void)
{
   static float a[RATRAC3_SAMPLES * 2], b[RATRAC3_SAMPLES * 2];
   ratrac3_t *d1 = ratrac3_new(2, 192, true);
   ratrac3_t *d2 = ratrac3_new(2, 192, true);
   unsigned   f;
   for (f = 0; f < FRAMES; f++)
      ratrac3_decode_float(d1, stream_b + f * 192, 192, a);
   ratrac3_reset(d1);
   for (f = 0; f < FRAMES; f++)
   {
      ratrac3_decode_float(d1, stream_b + f * 192, 192, a);
      ratrac3_decode_float(d2, stream_b + f * 192, 192, b);
      CHECK(!memcmp(a, b, sizeof(a)), "reset: frame %u differs", f);
   }
   ratrac3_free(d1);
   ratrac3_free(d2);
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
   static float pcm[RATRAC3_SAMPLES * 2];
   unsigned     mode;
   for (mode = 0; mode < 3; mode++)
   {
      unsigned   ba = mode == 1 ? 192 : 384, ch = mode == 2 ? 1 : 2;
      ratrac3_t *d  = ratrac3_new(ch, ba, mode == 1);
      unsigned   n, ok = 0, nonfinite = 0;
      for (n = 0; n < 4000; n++)
      {
         uint8_t  frame[384];
         unsigned i, k = rng() % 3;
         if (k == 0)
            for (i = 0; i < ba; i++)
               frame[i] = (uint8_t)rng();
         else
         {
            const uint8_t *src = mode == 1 ? stream_b + (n % FRAMES) * 192
               : stream_a + (n % FRAMES) * 384;
            memcpy(frame, src, ba);
            for (i = 0; i < 1 + rng() % 8; i++)
               frame[rng() % ba] ^= (uint8_t)(1u << (rng() % 8));
            if (k == 2)
               frame[0] = (uint8_t)(0xA0 | (frame[0] & 3));
         }
         if (ratrac3_decode_float(d, frame, ba, pcm) > 0)
            ok++;
         for (i = 0; i < RATRAC3_SAMPLES * ch; i++)
            if (!(pcm[i] == pcm[i]) || fabs(pcm[i]) > 1e30)
               nonfinite++;
      }
      CHECK(nonfinite == 0, "garbage: %u non-finite samples", nonfinite);
      printf("garbage mode %u: %u of 4000 frames decoded\n", mode, ok);
      ratrac3_free(d);
   }
}

int main(void)
{
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
   printf("ratrac3: all tests passed\n");
   return 0;
}
