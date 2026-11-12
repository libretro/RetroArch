/* Copyright (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------
 * The following license statement only applies to this file (rmpeg2_test.c).
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

/* MPEG-2 video through rmpeg1_video.
 *
 * With no arguments: three small streams FFmpeg wrote (progressive;
 * interlaced with field DCT and field motion; Table B.15, alternate scan,
 * the non-linear quantiser and 10-bit intra DC), each decoded whole and
 * fed one byte at a time, must give every frame, no error and no skipped
 * picture, and the same pixels as when they were checked against FFmpeg.
 *
 * With a stream and a reference (raw 4:2:0 frames, one per decoded
 * picture, as `ffmpeg -fps_mode passthrough -f rawvideo` writes them):
 * every frame within 3 of the reference and within 0.5 on average, as
 * two IEEE 1180 inverse transforms may differ, with no slice error. A
 * third argument skips that many reference frames first: leading B
 * pictures of an open GOP, which other decoders show and this one, with
 * no reference before them, counts as skipped.
 *
 * The CI workflow runs the second form over FFmpeg-written streams and
 * the field-picture, dual-prime and mixed streams of the ISO/IEC 13818-4
 * conformance suite.
 *
 * Build and run:  make && ./rmpeg2_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <formats/rmpeg1_video.h>

#include "rmpeg2_fixtures.h"

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

static unsigned long frame_hash(unsigned long h, const rmpeg1_video_frame_t *f)
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

/* The stream fed @chunk bytes at a time; its frames' hash */
static unsigned long decode_all(const unsigned char *data, size_t len, size_t chunk,
      unsigned *frames, unsigned *errors, unsigned *skipped)
{
   rmpeg1_video_t      *v = rmpeg1_video_init();
   rmpeg1_video_frame_t f;
   unsigned long        h = 2166136261UL;
   size_t               at = 0;

   *frames = 0;
   if (!v)
      return 0;
   while (at < len)
   {
      size_t n = len - at < chunk ? len - at : chunk;
      at += rmpeg1_video_write(v, data + at, n);
      while (rmpeg1_video_decode(v, &f))
      {
         h = frame_hash(h, &f);
         (*frames)++;
      }
   }
   rmpeg1_video_flush(v);
   while (rmpeg1_video_decode(v, &f))
   {
      h = frame_hash(h, &f);
      (*frames)++;
   }
   *errors  = rmpeg1_video_errors(v);
   *skipped = rmpeg1_video_skipped(v);
   rmpeg1_video_free(v);
   return h;
}

/* Access unit by access unit - a picture with the headers before it,
 * each tagged with its number - every frame comes out with the tag of
 * the picture it is (a field pair, its first field's): that picture's
 * temporal reference and coding type, read from its header here, each
 * once */
static void test_tags(const char *name, const unsigned char *data, size_t len)
{
   rmpeg1_video_t      *v = rmpeg1_video_init();
   rmpeg1_video_frame_t f;
   unsigned             tref[64], type[64], aus = 0, frames = 0, bad = 0, reordered = 0, last = 0;
   unsigned char        seen[64];
   size_t               at = 0, i;
   int                  after_slice = 1;

   memset(seen, 0, sizeof(seen));
   CHECK(v != NULL);
   if (!v)
      return;
   for (i = 0; i <= len; i++)
   {
      unsigned code = i + 3 < len && !data[i] && !data[i + 1] && data[i + 2] == 1 ? data[i + 3] : 0x100;
      /* a picture, GOP or sequence header after slices starts the next
       * unit; the end of the data ends the last */
      if (i == len || ((code == 0x00 || code == 0xB3 || code == 0xB8) && after_slice))
      {
         if (i > at)
         {
            CHECK(rmpeg1_video_write(v, data + at, i - at) == i - at);
            while (rmpeg1_video_decode(v, &f))
            {
               frames++;
               if (f.tag < 1 || f.tag > aus || seen[f.tag] || tref[f.tag] != f.temporal_ref
                     || type[f.tag] != f.coding_type)
                  bad++;
               else
                  seen[f.tag] = 1;
               reordered += f.tag < last;
               last = (unsigned)f.tag;
            }
         }
         at = i;
         after_slice = 0;
         if (i < len && aus + 1 < 64)
            rmpeg1_video_set_tag(v, ++aus);
      }
      if (code == 0x00 && i + 6 < len)
      {
         tref[aus] = ((unsigned)data[i + 4] << 2) | (data[i + 5] >> 6);
         type[aus] = (data[i + 5] >> 3) & 7;
      }
      if (code >= 0x01 && code <= 0xAF)
         after_slice = 1;
   }
   rmpeg1_video_flush(v);
   while (rmpeg1_video_decode(v, &f))
   {
      frames++;
      if (f.tag < 1 || f.tag > aus || seen[f.tag] || tref[f.tag] != f.temporal_ref
            || type[f.tag] != f.coding_type)
         bad++;
      else
         seen[f.tag] = 1;
      reordered += f.tag < last;
      last = (unsigned)f.tag;
   }
   printf("%s: %u units tagged, %u frames back with theirs, %u out of decode order\n",
         name, aus, frames - bad, reordered);
   CHECK(bad == 0 && frames == 8);
   rmpeg1_video_free(v);
}

static void test_fixture(const char *name, const unsigned char *data, size_t len,
      unsigned long want)
{
   unsigned      frames, errors, skipped, frames1, errors1, skipped1;
   unsigned long whole = decode_all(data, len, len, &frames, &errors, &skipped);
   unsigned long bytes = decode_all(data, len, 1, &frames1, &errors1, &skipped1);

   printf("%s: %u frames, hash %08lx\n", name, frames, whole);
   CHECK(frames == 8 && errors == 0 && skipped == 0);
   CHECK(whole == want);
   CHECK(frames1 == frames && errors1 == 0 && skipped1 == 0 && bytes == whole);
}

static void test_reference(const char *path, const char *reference, long lead)
{
   FILE                *input  = fopen(path, "rb");
   FILE                *oracle = fopen(reference, "rb");
   rmpeg1_video_t      *v      = rmpeg1_video_init();
   rmpeg1_video_frame_t frame;
   unsigned char        bytes[4096];
   unsigned             frames = 0, peak = 0;
   unsigned long        samples = 0, difference = 0;
   int                  finished = 0, started = 0;

   CHECK(input && oracle && v);
   if (!input || !oracle || !v)
      return;
   while (!finished)
   {
      size_t n = fread(bytes, 1, sizeof(bytes), input);
      if (n)
         CHECK(rmpeg1_video_write(v, bytes, n) == n);
      else
      {
         rmpeg1_video_flush(v);
         finished = 1;
      }
      while (rmpeg1_video_decode(v, &frame))
      {
         unsigned plane, y, x;
         if (!started)
         {
            /* the reference's leading frames, as many frames as asked */
            long skip = lead * (long)frame.width * (long)frame.height * 3 / 2;
            CHECK(fseek(oracle, skip, SEEK_SET) == 0);
            started = 1;
         }
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
   }
done:
   CHECK(frames > 0 && fgetc(oracle) == EOF);
   CHECK(rmpeg1_video_errors(v) == 0);
   CHECK(peak <= 3 && difference * 2 <= samples);
   printf("%s: frames=%u skipped=%u peak=%u mean=%f\n", path, frames,
         (unsigned)rmpeg1_video_skipped(v), peak,
         samples ? (double)difference / samples : 0.0);
   fclose(input);
   fclose(oracle);
   rmpeg1_video_free(v);
}

int main(int argc, char **argv)
{
   if (argc >= 3)
      test_reference(argv[1], argv[2], argc > 3 ? atol(argv[3]) : 0);
   else
   {
      test_fixture("progressive", m2_prog,  sizeof(m2_prog),  0x595061e8UL);
      test_fixture("interlaced",  m2_ilace, sizeof(m2_ilace), 0xe0cab4caUL);
      test_fixture("tools",       m2_tools, sizeof(m2_tools), 0x5c9b314eUL);
      test_tags("progressive tags", m2_prog,  sizeof(m2_prog));
      test_tags("interlaced tags",  m2_ilace, sizeof(m2_ilace));
      test_tags("tools tags",       m2_tools, sizeof(m2_tools));
   }
   if (failures)
      printf("\n%u check(s) failed\n", failures);
   else
      printf("\nall checks passed\n");
   return failures ? 1 : 0;
}
