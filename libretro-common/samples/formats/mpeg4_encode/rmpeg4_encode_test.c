/* Copyright (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------
 * The following license statement only applies to this file (rmpeg4_encode_test.c).
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

/* rmpeg4_encode, checked by rmpeg4_video. Frames made here - gradients,
 * a box moving faster than the search reaches, noise in a corner, a
 * scene cut - at a size of whole macroblocks and one of neither, with
 * a constant quantiser and with a bit rate:
 *   - rmpeg4_video decodes every VOP to exactly the encoder's own
 *     reconstruction, with no error, from the VOL the header gives;
 *   - the reconstruction is within the PSNR the quantiser gives;
 *   - I-VOPs come every gop frames, flagged as keys;
 *   - the stream is the bytes it was when FFmpeg decoded it within 2 of
 *     rmpeg4_video;
 *   - with a bit rate, the stream lands within 3% of it;
 *   - a buffer too small for a frame gives 0.
 *
 * With a directory argument the streams are written there, for the CI
 * workflow to decode with FFmpeg.
 *
 * Build and run:  make && ./rmpeg4_encode_test [dir]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <boolean.h>
#include <formats/rmpeg4_encode.h>
#include <formats/rmpeg4_video.h>

static unsigned failures;

#define CHECK(c) do { if (!(c)) { printf("  FAIL  %s (line %u)\n", #c, \
      (unsigned)__LINE__); failures++; } } while (0)

static unsigned char clip(int v)
{
   return (unsigned char)(v < 16 ? 16 : v > 235 ? 235 : v);
}

/* Frame @f: a moving gradient, a box crossing it 21 samples a frame,
 * noise in the top left, and a different picture from frame 30 on */
static void make_frame(unsigned char *y, unsigned char *cb, unsigned char *cr,
      unsigned w, unsigned h, unsigned f)
{
   unsigned cw = (w + 1) / 2, ch = (h + 1) / 2, i, j;
   unsigned long seed = 1 + f;
   int      cut = f >= 30;
   int      bx = (int)((f * 21) % (w + 32)) - 16, by = (int)(h / 3);
   for (j = 0; j < h; j++)
      for (i = 0; i < w; i++)
      {
         int v = cut ? (int)((i * 7 + j * 3) & 0xFF) / 2 + 40
                     : 60 + (int)((i + f * 2) % w) * 120 / (int)w + (int)j / 4;
         if ((int)i >= bx && (int)i < bx + 16 && (int)j >= by && (int)j < by + 12)
            v = 220;
         if (i < w / 4 && j < h / 4)
         {
            seed = seed * 1103515245UL + 12345UL;
            v += (int)((seed >> 16) & 31) - 16;
         }
         y[j * w + i] = clip(v);
      }
   for (j = 0; j < ch; j++)
      for (i = 0; i < cw; i++)
      {
         cb[j * cw + i] = clip(cut ? 90 : 100 + (int)(i * 40 / cw));
         cr[j * cw + i] = clip(cut ? 170 : 150 - (int)(j * 40 / ch));
      }
}

static unsigned long fnv(unsigned long h, const unsigned char *p, size_t n)
{
   size_t i;
   for (i = 0; i < n; i++)
      h = ((h ^ p[i]) * 16777619UL) & 0xFFFFFFFFUL;
   return h;
}

static void run(const char *dir, const char *name, unsigned w, unsigned h,
      unsigned frames, unsigned qp, uint32_t bitrate, double min_psnr,
      unsigned long want)
{
   rmpeg4_encoder_config_t c;
   rmpeg4_encoder_t       *e;
   rmpeg4_video_t         *d = rmpeg4_video_init();
   unsigned cw = (w + 1) / 2, ch = (h + 1) / 2, f, mismatch = 0, keys_ok = 1;
   size_t   cap, total = 0, n;
   unsigned char *y  = (unsigned char*)malloc((size_t)w * h);
   unsigned char *cb = (unsigned char*)malloc((size_t)cw * ch);
   unsigned char *cr = (unsigned char*)malloc((size_t)cw * ch);
   unsigned char *out;
   unsigned long  hash = 2166136261UL;
   double         se = 0;
   FILE          *file = NULL;
   uint8_t        hdr[64];

   memset(&c, 0, sizeof(c));
   c.width   = w;
   c.height  = h;
   c.fps_num = 25;
   c.fps_den = 1;
   c.qp      = qp;
   c.bitrate = bitrate;
   c.gop     = 12;
   e   = rmpeg4_encoder_new(&c);
   cap = rmpeg4_encoder_bound(e);
   out = (unsigned char*)malloc(cap);
   CHECK(e && d && y && cb && cr && out);
   if (!e || !d || !y || !cb || !cr || !out)
      return;

   /* the header alone gives the decoder the size */
   n = rmpeg4_encoder_header(e, hdr, sizeof(hdr));
   CHECK(n > 0);
   {
      size_t used;
      rmpeg4_video_frame_t fr;
      CHECK(!rmpeg4_video_decode(d, hdr, n, &used, &fr));
      CHECK(rmpeg4_video_has_vol(d) && rmpeg4_video_width(d) == w
            && rmpeg4_video_height(d) == h && rmpeg4_video_time_resolution(d) == 25);
   }

   if (dir)
   {
      char path[1024];
      sprintf(path, "%.900s/%s.m4v", dir, name);
      file = fopen(path, "wb");
   }

   for (f = 0; f < frames; f++)
   {
      rmpeg4_video_frame_t fr;
      const uint8_t *ry, *rcb, *rcr;
      unsigned       rys, rcs, i, j;
      size_t         used;
      bool           key = false;

      make_frame(y, cb, cr, w, h, f);
      n = rmpeg4_encoder_encode(e, out, cap, y, w, cb, cw, cr, cw, &key);
      CHECK(n > 0);
      if (!n)
         break;
      if (key != (f % 12 == 0))
         keys_ok = 0;
      total += n;
      hash   = fnv(hash, out, n);
      if (file)
         fwrite(out, 1, n, file);
      rmpeg4_encoder_reconstruction(e, &ry, &rcb, &rcr, &rys, &rcs);
      for (j = 0; j < h; j++)
         for (i = 0; i < w; i++)
         {
            double dd = (double)y[j * w + i] - ry[j * rys + i];
            se += dd * dd;
         }
      if (rmpeg4_video_decode(d, out, n, &used, &fr) && used == n
            && fr.width == w && fr.height == h)
      {
         for (j = 0; j < h; j++)
            mismatch += memcmp(fr.y + j * fr.y_stride, ry + j * rys, w) != 0;
         for (j = 0; j < ch; j++)
            mismatch += memcmp(fr.cb + j * fr.c_stride, rcb + j * rcs, cw) != 0
                     || memcmp(fr.cr + j * fr.c_stride, rcr + j * rcs, cw) != 0;
      }
      else
         mismatch++;

      /* a buffer half the size the next frame needs: refused (the
       * encoder moves on, so only after the last frame) */
      if (f + 1 == frames)
      {
         unsigned char *tight = (unsigned char*)malloc(n);
         make_frame(y, cb, cr, w, h, f + 1);
         CHECK(tight && rmpeg4_encoder_encode(e, tight, n / 2, y, w, cb, cw, cr, cw, &key) == 0);
         free(tight);
      }
   }
   if (file)
      fclose(file);

   {
      double psnr = 10 * log10(255.0 * 255.0 / (se / ((double)w * h * frames)));
      double kbps = total * 8.0 * 25 / frames / 1000;
      printf("%s: %ux%u, %u frames, %lu bytes (%.0f kbit/s), hash %08lx, PSNR %.2f, "
            "rows not as reconstructed %u\n", name, w, h, frames, (unsigned long)total,
            kbps, hash, psnr, mismatch);
      CHECK(mismatch == 0 && rmpeg4_video_errors(d) == 0 && rmpeg4_video_skipped(d) == 0);
      CHECK(keys_ok);
      CHECK(psnr >= min_psnr);
      CHECK(!want || hash == want);
      if (bitrate)
         CHECK(fabs(kbps * 1000 - bitrate) <= bitrate * 0.03);
   }
   rmpeg4_encoder_free(e);
   rmpeg4_video_free(d);
   free(y);
   free(cb);
   free(cr);
   free(out);
}

int main(int argc, char **argv)
{
   const char *dir = argc > 1 ? argv[1] : NULL;

   run(dir, "whole",   64, 48,  48,  4,      0, 40.5, 0xe093411fUL);
   run(dir, "odd",     70, 38,  48,  6,      0, 37.0, 0x7b96e026UL);
   run(dir, "fine",   176, 144, 24,  1,      0, 50.0, 0x0e03c629UL);
   run(dir, "coarse", 176, 144, 24, 31,      0, 36.0, 0x08112e5bUL);
   run(dir, "rate",   176, 144, 120, 8, 200000, 38.5, 0x73caffecUL);

   if (failures)
      printf("\n%u check(s) failed\n", failures);
   else
      printf("\nall checks passed\n");
   return failures ? 1 : 0;
}
