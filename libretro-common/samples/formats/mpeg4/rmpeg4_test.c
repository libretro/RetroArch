/* Copyright (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------
 * The following license statement only applies to this file (rmpeg4_test.c).
 * ---------------------------------------------------------------------------
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

/* MPEG-4 Part 2 video through rmpeg4_video.
 *
 * With no arguments: five small streams (rmpeg4_fixtures.h says what each
 * holds), each decoded whole and a start code unit at a time - every unit
 * in a buffer of its own size, so that a read past the unit is caught -
 * must give every frame, no error and no skipped VOP, and the same pixels
 * as when they were checked against FFmpeg. Then each, damaged (cut short,
 * bits flipped), must decode without a fault.
 *
 * With a stream and a reference (raw 4:2:0 frames, one per decoded VOP,
 * as `ffmpeg -threads 1 -fps_mode passthrough -f rawvideo` writes them):
 * every frame within 3 of the reference and within 0.5 on average, as
 * two IEEE 1180 inverse transforms may differ, with no packet error. A
 * frame-threaded FFmpeg decode is not a reference: its threads do not
 * share the field vectors that interlaced direct mode reads.
 *
 * The CI workflow runs the second form over streams FFmpeg's and Xvid's
 * encoders write.
 *
 * Build and run:  make && ./rmpeg4_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <formats/rmpeg4_video.h>

#include "rmpeg4_fixtures.h"

static unsigned failures;

#define CHECK(c) do { if (!(c)) { printf("  FAIL  %s (line %u)\n", #c, \
      (unsigned)__LINE__); failures++; } } while (0)

static unsigned long fnv(unsigned long h, const unsigned char *p, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
      h = (h ^ p[i]) * 16777619UL;
   return h & 0xFFFFFFFFUL;
}

static unsigned long frame_hash(unsigned long h, const rmpeg4_video_frame_t *f)
{
   unsigned y;
   for (y = 0; y < f->height; y++)
      h = fnv(h, f->y + (size_t)y * f->y_stride, f->width);
   for (y = 0; y < (f->height + 1) / 2; y++)
      h = fnv(h, f->cb + (size_t)y * f->c_stride, (f->width + 1) / 2);
   for (y = 0; y < (f->height + 1) / 2; y++)
      h = fnv(h, f->cr + (size_t)y * f->c_stride, (f->width + 1) / 2);
   return h;
}

typedef struct
{
   unsigned long hash;
   unsigned      frames;
   unsigned      errors;
   unsigned      skipped;
} result_t;

/* Everything in one buffer, as a container would hand out a packed
 * access unit */
static void feed(rmpeg4_video_t *v, const unsigned char *p, size_t len, result_t *r)
{
   rmpeg4_video_frame_t f;
   size_t at = 0;
   while (at < len)
   {
      size_t used;
      if (rmpeg4_video_decode(v, p + at, len - at, &used, &f))
      {
         r->hash = frame_hash(r->hash, &f);
         r->frames++;
      }
      if (!used)
         break;
      at += used;
   }
}

/* Where the next unit starts: an MPEG-4 start code, or a short header
 * picture start code */
static size_t unit_end(const unsigned char *p, size_t from, size_t len)
{
   size_t i;
   for (i = from; i + 3 <= len; i++)
      if (!p[i] && !p[i + 1] && (p[i + 2] == 1 || (p[i + 2] & 0xFC) == 0x80))
         return i;
   return len;
}

static result_t decode(const unsigned char *data, size_t len, int by_unit)
{
   rmpeg4_video_t      *v = rmpeg4_video_init();
   rmpeg4_video_frame_t f;
   result_t             r;

   r.hash = 2166136261UL;
   r.frames = r.errors = r.skipped = 0;
   if (!v)
      return r;
   if (!by_unit)
      feed(v, data, len, &r);
   else
   {
      size_t at = unit_end(data, 0, len);
      while (at < len)
      {
         size_t         end  = unit_end(data, at + 3, len);
         unsigned char *unit = (unsigned char*)malloc(end - at);
         if (!unit)
            break;
         memcpy(unit, data + at, end - at);
         feed(v, unit, end - at, &r);
         free(unit);
         at = end;
      }
   }
   while (rmpeg4_video_flush(v, &f))
   {
      r.hash = frame_hash(r.hash, &f);
      r.frames++;
   }
   r.errors  = rmpeg4_video_errors(v);
   r.skipped = rmpeg4_video_skipped(v);
   rmpeg4_video_free(v);
   return r;
}

/* Cut short and bit-flipped copies: the decoder must survive them */
static void damage(const unsigned char *data, size_t len)
{
   unsigned char *copy = (unsigned char*)malloc(len);
   unsigned long  seed = 12345;
   size_t         cut;
   int            k;

   if (!copy)
      return;
   for (cut = 1; cut < len; cut += len / 13 + 1)
   {
      unsigned char *part = (unsigned char*)malloc(cut);
      if (!part)
         break;
      memcpy(part, data, cut);
      (void)decode(part, cut, 0);
      free(part);
   }
   for (k = 0; k < 24; k++)
   {
      int flips;
      memcpy(copy, data, len);
      for (flips = 0; flips < 1 + k; flips++)
      {
         seed = seed * 1103515245UL + 12345UL;
         copy[(seed >> 8) % len] ^= (unsigned char)(1u << ((seed >> 4) & 7));
      }
      (void)decode(copy, len, k & 1);
   }
   free(copy);
}

static void test_fixture(const char *name, const unsigned char *data, size_t len,
      unsigned long want)
{
   result_t whole = decode(data, len, 0);
   result_t units = decode(data, len, 1);

   printf("%s: %u frames, hash %08lx\n", name, whole.frames, whole.hash);
   CHECK(whole.frames == 8 && whole.errors == 0 && whole.skipped == 0);
   CHECK(whole.hash == want);
   CHECK(units.frames == whole.frames && units.errors == 0 && units.skipped == 0
         && units.hash == whole.hash);
   damage(data, len);
}

static void test_reference(const char *path, const char *reference)
{
   FILE                *input  = fopen(path, "rb");
   FILE                *oracle = fopen(reference, "rb");
   rmpeg4_video_t      *v      = rmpeg4_video_init();
   rmpeg4_video_frame_t frame;
   unsigned char       *data   = NULL;
   long                 len    = 0;
   size_t               at     = 0;
   unsigned             frames = 0, peak = 0;
   unsigned long        samples = 0, difference = 0;
   int                  flushing = 0;

   CHECK(input && oracle && v);
   if (!input || !oracle || !v)
      return;
   if (fseek(input, 0, SEEK_END) == 0 && (len = ftell(input)) > 0)
   {
      data = (unsigned char*)malloc((size_t)len);
      rewind(input);
      if (data && fread(data, 1, (size_t)len, input) != (size_t)len)
         len = 0;
   }
   CHECK(data && len > 0);

   for (;;)
   {
      unsigned plane, y, x;
      if (!flushing)
      {
         size_t used = 0;
         int    got  = at < (size_t)len
                     && rmpeg4_video_decode(v, data + at, (size_t)len - at, &used, &frame);
         at += used;
         if (!got)
         {
            if (at < (size_t)len && used)
               continue;
            flushing = 1;
            continue;
         }
      }
      else if (!rmpeg4_video_flush(v, &frame))
         break;
      frames++;
      for (plane = 0; plane < 3; plane++)
      {
         const unsigned char *p = plane == 0 ? frame.y : plane == 1 ? frame.cb : frame.cr;
         unsigned w      = plane == 0 ? frame.width  : (frame.width + 1) / 2;
         unsigned h      = plane == 0 ? frame.height : (frame.height + 1) / 2;
         unsigned stride = plane == 0 ? frame.y_stride : frame.c_stride;
         for (y = 0; y < h; y++)
            for (x = 0; x < w; x++)
            {
               int      expected = fgetc(oracle);
               unsigned error;
               if (expected == EOF)
               {
                  CHECK(expected != EOF);
                  goto done;
               }
               error = (unsigned)abs((int)p[(size_t)y * stride + x] - expected);
               if (error > peak)
                  peak = error;
               difference += error;
               samples++;
            }
      }
   }
done:
   CHECK(frames > 0 && fgetc(oracle) == EOF);
   CHECK(rmpeg4_video_errors(v) == 0 && rmpeg4_video_skipped(v) == 0);
   CHECK(peak <= 3 && difference * 2 <= samples);
   printf("%s: frames=%u skipped=%u peak=%u mean=%f\n", path, frames,
         (unsigned)rmpeg4_video_skipped(v), peak,
         samples ? (double)difference / samples : 0.0);
   free(data);
   fclose(input);
   fclose(oracle);
   rmpeg4_video_free(v);
}

int main(int argc, char **argv)
{
   if (argc >= 3)
      test_reference(argv[1], argv[2]);
   else
   {
      test_fixture("asp",    m4_asp,   sizeof(m4_asp),   0x5dcfa72bUL);
      test_fixture("ilace",  m4_ilace, sizeof(m4_ilace), 0x8467c975UL);
      test_fixture("gmc",    m4_gmc,   sizeof(m4_gmc),   0xf99e38ddUL);
      test_fixture("resil",  m4_resil, sizeof(m4_resil), 0xd15d6c66UL);
      test_fixture("shortv", m4_short, sizeof(m4_short), 0x59be255aUL);
   }
   if (failures)
      printf("\n%u check(s) failed\n", failures);
   else
      printf("\nall checks passed\n");
   return failures ? 1 : 0;
}
