/* Copyright (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------
 * The following license statement only applies to this file (rh264_encode_test.c).
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

/* rh264_encode, checked by rh264. Frames made here - gradients, a box
 * moving faster than the search reaches, noise in a corner, colour that
 * shifts, a scene cut - at a size of whole macroblocks and one cropped,
 * with fine, middling and coarse quantisers and with a bit rate:
 *   - rh264 decodes every picture to exactly the encoder's own
 *     reconstruction, from Annex B and from MP4 samples with the avcC
 *     record the encoder gives;
 *   - the reconstruction is within the PSNR the quantiser gives;
 *   - IDR pictures come every gop frames, flagged as keys;
 *   - rh264 reads the frame rate back from the VUI's timing;
 *   - with a constant quantiser the stream is the bytes it was when
 *     FFmpeg decoded it to the same frames;
 *   - with a bit rate, the stream lands within 3% of it;
 *   - at every quantiser, rh264 decodes the reconstruction too;
 *   - a buffer too small gives 0, a size the encoder cannot take no
 *     encoder.
 *
 * With a directory argument the streams are written there, Annex B,
 * with the reconstruction next to each, for the CI workflow to decode
 * with FFmpeg and compare.
 *
 * Build and run:  make && ./rh264_encode_test [dir]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <boolean.h>
#include <formats/rh264.h>
#include <formats/rh264_encode.h>

static unsigned failures;
static bool     quiet;

#define CHECK(c) do { if (!(c)) { printf("  FAIL  %s (line %u)\n", #c, \
      (unsigned)__LINE__); failures++; } } while (0)

static unsigned char clip(int v)
{
   return (unsigned char)(v < 16 ? 16 : v > 235 ? 235 : v);
}

/* Frame @f: a moving gradient, a box crossing it 21 samples a frame,
 * noise in the top left, colour drifting, and a different picture from
 * frame 30 on */
static void make_frame(unsigned char *y, unsigned char *cb, unsigned char *cr,
      unsigned w, unsigned h, unsigned f)
{
   unsigned cw = w / 2, ch = h / 2, i, j;
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
         cb[j * cw + i] = clip(cut ? 90 : 100 + (int)(i * 40 / cw) + (int)(f % 7));
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

/* rows of the decoder's picture that are not the reconstruction's */
static unsigned compare(rh264_video *d, unsigned w, unsigned h, const uint8_t *ry,
      const uint8_t *rcb, const uint8_t *rcr, unsigned rys, unsigned rcs)
{
   int ys, cs, pw, ph, cw2, ch2;
   const uint8_t *dy = rh264_video_plane(d, 0, &ys, &pw, &ph);
   const uint8_t *du = rh264_video_plane(d, 1, &cs, &cw2, &ch2);
   const uint8_t *dv = rh264_video_plane(d, 2, &cs, &cw2, &ch2);
   unsigned j, bad = 0;
   if (!dy || !du || !dv || pw != (int)w || ph != (int)h)
      return h;
   for (j = 0; j < h; j++)
      bad += memcmp(dy + j * ys, ry + j * rys, w) != 0;
   for (j = 0; j < h / 2; j++)
      bad += memcmp(du + j * cs, rcb + j * rcs, w / 2) != 0
          || memcmp(dv + j * cs, rcr + j * rcs, w / 2) != 0;
   return bad;
}

static void run(const char *dir, const char *name, unsigned w, unsigned h,
      unsigned frames, unsigned qp, uint32_t bitrate, bool annexb, double min_psnr,
      unsigned long want)
{
   rh264_encoder_config_t c;
   rh264_encoder_t       *e;
   rh264_video           *d = rh264_video_open();
   unsigned cw = w / 2, f, mismatch = 0, keys_ok = 1, j;
   size_t   cap, total = 0, n;
   unsigned char *y  = (unsigned char*)malloc((size_t)w * h);
   unsigned char *cb = (unsigned char*)malloc((size_t)cw * (h / 2));
   unsigned char *cr = (unsigned char*)malloc((size_t)cw * (h / 2));
   unsigned char *out;
   unsigned long  hash = 2166136261UL;
   double         se = 0;
   FILE          *file = NULL, *file_r = NULL;

   memset(&c, 0, sizeof(c));
   c.width   = w;
   c.height  = h;
   c.fps_num = 25;
   c.fps_den = 1;
   c.qp      = qp;
   c.bitrate = bitrate;
   c.gop     = 24;
   e   = rh264_encoder_new(&c);
   cap = rh264_encoder_bound(e);
   out = (unsigned char*)malloc(cap);
   CHECK(e && d && y && cb && cr && out);
   if (!e || !d || !y || !cb || !cr || !out)
      return;

   /* MP4 samples: the decoder takes the parameter sets from the avcC */
   if (!annexb)
   {
      uint8_t avcc[128];
      n = rh264_encoder_avcc(e, avcc, sizeof(avcc));
      CHECK(n > 11 && avcc[0] == 1 && avcc[1] == 66 && avcc[2] == 0xC0
            && avcc[4] == 0xFF && avcc[5] == 0xE1);
      CHECK(rh264_encoder_avcc(e, avcc, n - 1) == 0);
      CHECK(rh264_video_set_extradata(d, avcc, n) == 0);
   }

   if (dir && annexb)
   {
      char path[1024];
      sprintf(path, "%.900s/%s.264", dir, name);
      file   = fopen(path, "wb");
      sprintf(path, "%.900s/%s.yuv", dir, name);
      file_r = fopen(path, "wb");
   }

   for (f = 0; f < frames; f++)
   {
      const uint8_t *ry, *rcb, *rcr;
      unsigned       rys, rcs, i;
      bool           key = false;

      make_frame(y, cb, cr, w, h, f);
      n = rh264_encoder_encode(e, out, cap, y, w, cb, cw, cr, cw, annexb, &key);
      CHECK(n > 0);
      if (!n)
         break;
      if (key != (f % 24 == 0))
         keys_ok = 0;
      total += n;
      hash   = fnv(hash, out, n);
      if (file)
         fwrite(out, 1, n, file);
      rh264_encoder_reconstruction(e, &ry, &rcb, &rcr, &rys, &rcs);
      if (file_r)
      {
         for (j = 0; j < h; j++)
            fwrite(ry + j * rys, 1, w, file_r);
         for (j = 0; j < h / 2; j++)
            fwrite(rcb + j * rcs, 1, cw, file_r);
         for (j = 0; j < h / 2; j++)
            fwrite(rcr + j * rcs, 1, cw, file_r);
      }
      for (j = 0; j < h; j++)
         for (i = 0; i < w; i++)
         {
            double dd = (double)y[j * w + i] - ry[j * rys + i];
            se += dd * dd;
         }
      if (rh264_video_decode(d, out, n) == 1)
         mismatch += compare(d, w, h, ry, rcb, rcr, rys, rcs);
      else
         mismatch += h;

      /* the VUI's timing, 25 frames a second, is read back */
      if (f == 0)
      {
         unsigned num = 0, den = 0;
         CHECK(rh264_video_framerate(d, &num, &den) == 0 && num == 25 && den == 1);
      }

      /* a buffer too small for any picture: refused (the encoder moves
       * on, so only after the last frame) */
      if (f + 1 == frames)
      {
         unsigned char tight[8];
         make_frame(y, cb, cr, w, h, f + 1);
         CHECK(rh264_encoder_encode(e, tight, sizeof(tight), y, w, cb, cw, cr, cw,
                  annexb, &key) == 0);
      }
   }
   if (file)
      fclose(file);
   if (file_r)
      fclose(file_r);

   {
      double psnr = 10 * log10(255.0 * 255.0 / (se / ((double)w * h * frames)));
      double kbps = total * 8.0 * 25 / frames / 1000;
      if (!quiet || mismatch)
         printf("%s: %ux%u, %u frames, %lu bytes (%.0f kbit/s), hash %08lx, PSNR %.2f, "
               "rows not as reconstructed %u\n", name, w, h, frames, (unsigned long)total,
               kbps, hash, psnr, mismatch);
      CHECK(mismatch == 0);
      CHECK(keys_ok);
      CHECK(psnr >= min_psnr);
      CHECK(!want || hash == want);
      if (bitrate)
         CHECK(fabs(kbps * 1000 - bitrate) <= bitrate * 0.03);
   }
   rh264_encoder_free(e);
   rh264_video_close(d);
   free(y);
   free(cb);
   free(cr);
   free(out);
}

int main(int argc, char **argv)
{
   const char *dir = argc > 1 ? argv[1] : NULL;
   rh264_encoder_config_t c;

   /* sizes 4:2:0 cannot crop to, none at all, too big */
   memset(&c, 0, sizeof(c));
   c.fps_num = 25;
   c.fps_den = 1;
   c.width   = 71;
   c.height  = 38;
   CHECK(!rh264_encoder_new(&c));
   c.width   = 70;
   c.height  = 0;
   CHECK(!rh264_encoder_new(&c));
   c.width   = 4098;
   c.height  = 64;
   CHECK(!rh264_encoder_new(&c));

   run(dir, "whole",   64,  48, 48, 26,      0, true,  40.0, 0x465e27e8UL);
   run(dir, "cropped", 70,  38, 48, 30,      0, true,  36.0, 0xc76eaa44UL);
   run(dir, "fine",   176, 144, 30, 10,      0, true,  50.0, 0x1fbb1464UL);
   run(dir, "coarse", 176, 144, 30, 46,      0, true,  24.0, 0x9a3bc98cUL);
   run(dir, "rate",   176, 144, 96, 26, 200000, true,  34.0, 0);
   run(NULL, "mp4",    96,  64, 30, 28,      0, false, 38.0, 0x8f121a0cUL);

   /* every quantiser, an IDR and two P pictures each: the filter's and
    * the scales' tables at every index */
   {
      unsigned q, before = failures;
      quiet = true;
      for (q = 1; q <= 51; q++)
         run(NULL, "sweep", 48, 32, 3, q, 0, true, 0.0, 0);
      quiet = false;
      printf("sweep: quantisers 1 to 51, %s\n", failures == before ? "all as reconstructed"
            : "FAILED");
   }

   if (failures)
      printf("\n%u check(s) failed\n", failures);
   else
      printf("\nall checks passed\n");
   return failures ? 1 : 0;
}
