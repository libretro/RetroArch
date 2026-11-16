/* rflac's 16-bit output, against the PCM each fixture was encoded from:
 *
 *   - a stream of 16 bits comes out exactly as it went in;
 *   - one of 24 or 32 bits comes out rounded to the nearest 16-bit step
 *     (halves up) and clamped at the top - not cut down, which left every
 *     sample half a step low on average;
 *   - both, through every way a stereo frame's channels are coded
 *     (independent, left/side, side/right, mid/side, which the stereo
 *     fixtures hold each of), a 32-bit stream's 33-bit side channel, mono
 *     and six channels, and the SIMD loops and their scalar tails (the
 *     fixtures are 5003 frames).
 *
 * Built twice: as the target builds (SSE2 on x86-64, NEON on ARM) and with
 * RFLAC_NO_SIMD, the scalar loops. The PCM is rebuilt here exactly as
 * mkfixtures.py builds it, with integers only. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <formats/rflac.h>
#include <streams/file_stream.h>

#define FRAMES 5003

static unsigned failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; \
   fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
   fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static int64_t tri(int64_t i, int64_t period, int64_t amp)
{
   int64_t p    = i % period;
   int64_t half = period / 2;
   int64_t v    = p < half ? p : period - p;
   return (v * 2 * amp) / half - amp;
}

/* mkfixtures.py's sample(), step for step */
static int64_t sample(const char *kind, unsigned bps, unsigned c, int64_t i,
      uint32_t *seed)
{
   int64_t top = ((int64_t)1 << (bps - 1)) - 1;
   int64_t n, v;
   *seed = *seed * 1103515245u + 12345u;
   n     = (int64_t)(*seed >> 20);
   if (!strcmp(kind, "stereo"))
   {
      int64_t base = tri(i, 397, top / 2);
      int64_t seg  = i / 1024;
      if (seg == 0)
         v = base + (c ? n : -n);
      else if (seg == 1)
         v = c == 0 ? base : base + n;
      else if (seg == 2)
         v = c == 0 ? base + n : base;
      else
         v = tri(i, 397 + 113 * c, top / 3) + n * (c + 1);
   }
   else if (!strcmp(kind, "wide"))
   {
      v = c == 0 ? top - (n & 255) : -top + (n & 255);
      if (i % 7 == 0)
         v = -v;
   }
   else
      v = tri(i, 211 + 37 * c, top / 2) + n;
   if (v > top)
      v = top;
   if (v < -top - 1)
      v = -top - 1;
   return v;
}

/* The 16-bit value a sample of @bps bits should give: its value at 16
 * bits, rounded to the nearest step, halves up, the top clamped */
static int16_t expect(int64_t v, unsigned bps)
{
   int64_t r;
   if (bps <= 16)
      return (int16_t)(v * ((int64_t)1 << (16 - bps)));
   r = (v + ((int64_t)1 << (bps - 17))) >> (bps - 16);
   return (int16_t)(r > 32767 ? 32767 : r);
}

static void run(const char *name, const char *kind, unsigned bps, unsigned ch)
{
   char      path[256];
   void     *buf = NULL;
   int64_t   len = 0;
   rflac_t  *f   = rflac_new();
   int16_t  *out = (int16_t*)calloc((size_t)(FRAMES + 64) * ch, sizeof(int16_t));
   size_t    got = 0, i;
   uint32_t  seed = bps * 131 + ch;
   unsigned  bad = 0, idle = 0;
   long      sum_err = 0;
   int       result;

   snprintf(path, sizeof(path), "fixtures/%s", name);
   if (!filestream_read_file(path, &buf, &len) || len <= 0)
   {
      fprintf(stderr, "cannot read %s (run from the sample directory)\n", path);
      exit(2);
   }
   CHECK(f != NULL, "%s: rflac_new", name);
   rflac_set_in(f, (const uint8_t*)buf, (size_t)len);
   rflac_set_eof(f);
   do
   {
      size_t wrote = 0;
      rflac_set_out_s16(f, out + got * ch, FRAMES + 64 - got);
      result = rflac_process(f, NULL, &wrote);
      got   += wrote;
      idle   = wrote ? 0 : idle + 1;
   } while (result == RFLAC_PROCESS_NEXT && idle < 4);

   CHECK(got == FRAMES, "%s: %lu frames, %d wanted", name, (unsigned long)got,
         FRAMES);
   for (i = 0; i < got * ch && i < (size_t)FRAMES * ch; i++)
   {
      int64_t v      = sample(kind, bps, (unsigned)(i % ch), (int64_t)(i / ch), &seed);
      int16_t want   = expect(v, bps);
      if (out[i] != want)
      {
         if (!bad)
            fprintf(stderr, "  %s: sample %lu is %d, %d wanted (from %lld)\n",
                  name, (unsigned long)i, out[i], want, (long long)v);
         bad++;
      }
      sum_err += out[i] - want;
   }
   CHECK(bad == 0, "%s: %u of %lu samples off", name, bad,
         (unsigned long)(got * ch));
   (void)sum_err;
   rflac_free(f);
   free(out);
   free(buf);
}

int main(void)
{
   run("s16_stereo.flac", "stereo", 16, 2);
   run("s24_stereo.flac", "stereo", 24, 2);
   run("s24_mono.flac",   "mono",   24, 1);
   run("s24_6ch.flac",    "multi",  24, 6);
   run("s32_wide.flac",   "wide",   32, 2);

   if (failures)
   {
      fprintf(stderr, "%u failure(s)\n", failures);
      return 1;
   }
   printf("[pass] rflac_s16_test\n");
   return 0;
}
