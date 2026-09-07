/* Copyright  (C) 2010-2024 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image_hdr_blit.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* Shared 10-bit / HDR 4:2:0 -> RGB blits used by the webm (rvp9) and mp4
 * (rvp9-in-ISO-BMFF) thumbnail/video paths. These are generic YCbCr
 * converters with no demuxer dependency, so both formats link the same
 * implementation instead of one depending on the other's object. The public
 * names keep the rwebm_video_ prefix for API stability (they are declared in
 * formats/rwebm_video.h and were originally defined there). */

#include <stdint.h>
#include <stddef.h>
#include <math.h>

#include <formats/rwebm_video.h>

/* ------------------------------------------------------------------ */
/* 10-bit (HDR10 / SDR10) 4:2:0 -> 8-bit RGB conversion.               */
/*                                                                     */
/* PQ pipeline: YCbCr -> PQ-coded R'G'B' (integer matrix), a 1024-entry*/
/* LUT folding the ST.2084 EOTF, a hable tone map with auto exposure   */
/* (SDR reference white lands at 0.8 of display peak; 1000-nit assumed */
/* source peak), then a BT.2020 -> 709 gamut matrix applied on the     */
/* tone-mapped linear values (an approximation: strictly the gamut     */
/* conversion belongs before the per-channel tone map, but doing it    */
/* after keeps the whole EOTF+EETF per-channel and LUT-foldable, and   */
/* the error is small for playback purposes), and a linear -> sRGB     */
/* output LUT.  SDR 10-bit takes a direct matrix + round-to-8 path.    */
/* ------------------------------------------------------------------ */
static uint16_t rwebm_hbd_pq_lut[1024];   /* PQ' -> tone-mapped linear */
static uint8_t  rwebm_hbd_out_lut[8193];  /* linear -> sRGB' 8-bit     */
static uint16_t rwebm_hbd_out_lut10[8193];/* linear -> sRGB' 10-bit    */
static int      rwebm_hbd_lut_ready;
static unsigned rwebm_hbd_lut_peak;       /* nits the PQ LUT was built for */

static double rwebm_hbd_hable(double x)
{
   const double A = 0.15, B = 0.50, C = 0.10, D = 0.20, E = 0.02, F = 0.30;
   return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

static void rwebm_hbd_init_luts(unsigned peak_cll)
{
   const double m1 = 0.1593017578125, m2 = 78.84375;
   const double c1 = 0.8359375, c2 = 18.8515625, c3 = 18.6875;
   const double sdr_white = 203.0;
   /* MaxCLL when signalled, else assume a 1000-nit grade; clamp to a
    * sane window so absurd metadata cannot wreck the curve */
   const double peak_nits =
      (peak_cll >= 400 && peak_cll <= 10000) ? (double)peak_cll : 1000.0;
   const double peak = peak_nits / sdr_white;
   double glo = 1.0, ghi = 8.0, g = 2.0;
   int i, it;

   /* auto exposure: bisect gain so SDR reference white maps to 0.8 */
   for (it = 0; it < 48; it++)
   {
      g = (glo + ghi) * 0.5;
      if (rwebm_hbd_hable(g) / rwebm_hbd_hable(g * peak) < 0.80)
         glo = g;
      else
         ghi = g;
   }

   for (i = 0; i < 1024; i++)
   {
      double ep = pow(i / 1023.0, 1.0 / m2);
      double num = ep - c1;
      double y, l, o;
      if (num < 0.0) num = 0.0;
      y = pow(num / (c2 - c3 * ep), 1.0 / m1);        /* 0..1 of 10000 */
      l = (10000.0 * y) / sdr_white;                  /* SDR-white rel */
      o = rwebm_hbd_hable(g * l) / rwebm_hbd_hable(g * peak);
      if (o > 1.0) o = 1.0;
      rwebm_hbd_pq_lut[i] = (uint16_t)(o * 65535.0 + 0.5);
   }
   for (i = 0; i <= 8192; i++)
   {
      double lin = i / 8192.0;
      double s   = lin <= 0.0031308
         ? 12.92 * lin : 1.055 * pow(lin, 1.0 / 2.4) - 0.055;
      rwebm_hbd_out_lut[i]   = (uint8_t)(s * 255.0 + 0.5);
      rwebm_hbd_out_lut10[i] = (uint16_t)(s * 1023.0 + 0.5);
   }
   rwebm_hbd_lut_ready = 1;
   rwebm_hbd_lut_peak  = peak_cll;
}

/* limited-range 10-bit YCbCr coefficients, <<8 */
static void rwebm_hbd_matrix(unsigned matrix, int *re, int *gd, int *ge, int *bd)
{
   switch (matrix)
   {
      case 5: case 6:            /* BT.601 */
         *re = 409; *gd = 100; *ge = 208; *bd = 517; break;
      case 9: case 10:           /* BT.2020 */
         *re = 431; *gd =  48; *ge = 167; *bd = 548; break;
      case 1: default:           /* BT.709 / unspecified HD */
         *re = 459; *gd =  55; *ge = 136; *bd = 541; break;
   }
}

void rwebm_video_blit_i420_hbd(uint32_t *dst, unsigned dst_stride,
      unsigned w, unsigned h, const uint16_t *y, int ys,
      const uint16_t *u, const uint16_t *v, int uvs,
      unsigned matrix, unsigned transfer, unsigned range,
      unsigned max_cll, int abgr)
{
   int re, gd, ge, bd;
   int is_pq   = (transfer == 16);
   int full    = (range == 2);
   unsigned j, i;

   if (!rwebm_hbd_lut_ready || (is_pq && max_cll != rwebm_hbd_lut_peak))
      rwebm_hbd_init_luts(max_cll);
   /* HDR10 without an explicit matrix is BT.2020-ncl in practice */
   rwebm_hbd_matrix(matrix ? matrix : (is_pq ? 9u : 1u), &re, &gd, &ge, &bd);

   for (j = 0; j < h; j++)
   {
      const uint16_t *yr = y + (size_t)j * ys;
      const uint16_t *ur = u + (size_t)(j >> 1) * uvs;
      const uint16_t *vr = v + (size_t)(j >> 1) * uvs;
      uint32_t       *dr = dst + (size_t)j * dst_stride;
      for (i = 0; i < w; i++)
      {
         int c, d, e, r, g, b;
         if (full)
         {
            c = ((int)yr[i] * 877) >> 10;
            d = (((int)ur[i >> 1] - 512) * 897) >> 10;
            e = (((int)vr[i >> 1] - 512) * 897) >> 10;
         }
         else
         {
            c = (int)yr[i] - 64;
            d = (int)ur[i >> 1] - 512;
            e = (int)vr[i >> 1] - 512;
         }
         r = (298 * c + re * e + 128) >> 8;
         g = (298 * c - gd * d - ge * e + 128) >> 8;
         b = (298 * c + bd * d + 128) >> 8;
         if (r < 0) r = 0; else if (r > 1023) r = 1023;
         if (g < 0) g = 0; else if (g > 1023) g = 1023;
         if (b < 0) b = 0; else if (b > 1023) b = 1023;
         if (is_pq)
         {
            /* tone-mapped linear, then 2020 -> 709 gamut, then sRGB */
            int lr = rwebm_hbd_pq_lut[r];
            int lg = rwebm_hbd_pq_lut[g];
            int lb = rwebm_hbd_pq_lut[b];
            int xr = (6801 * lr - 2407 * lg -  298 * lb) >> 12;
            int xg = ( -510 * lr + 4640 * lg -   34 * lb) >> 12;
            int xb = (  -75 * lr -  412 * lg + 4583 * lb) >> 12;
            if (xr < 0) xr = 0; else if (xr > 65535) xr = 65535;
            if (xg < 0) xg = 0; else if (xg > 65535) xg = 65535;
            if (xb < 0) xb = 0; else if (xb > 65535) xb = 65535;
            r = rwebm_hbd_out_lut[xr >> 3];
            g = rwebm_hbd_out_lut[xg >> 3];
            b = rwebm_hbd_out_lut[xb >> 3];
         }
         else
         {
            r = (r + 2) >> 2; if (r > 255) r = 255;
            g = (g + 2) >> 2; if (g > 255) g = 255;
            b = (b + 2) >> 2; if (b > 255) b = 255;
         }
         dr[i] = abgr
            ? ((uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16)
               | 0xFF000000u)
            : (((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b
               | 0xFF000000u);
      }
   }
}

/* ------------------------------------------------------------------ */
/* Native 10-bit I420 -> XRGB2101010 (packed 2-10-10-10, native endian) */
/* Produces the SAME SDR-encoded colour as the _hbd path - PQ/HDR10 is  */
/* tone-mapped (hable), BT.2020 -> 709 gamut, sRGB transfer - but keeps  */
/* 10-bit precision instead of narrowing to 8, so gradients are smoother */
/* with less banding. The frontend's HDR pipeline expects an SDR-encoded */
/* source (it inverse-tone-maps SDR -> HDR for the display), so this     */
/* deliberately does NOT pass PQ through; it only raises the source bit  */
/* depth. Used when the frontend accepts RETRO_PIXEL_FORMAT_XRGB2101010. */
/* ------------------------------------------------------------------ */
void rwebm_video_blit_i420_10bit(uint32_t *dst, unsigned dst_stride,
      unsigned w, unsigned h, const uint16_t *y, int ys,
      const uint16_t *u, const uint16_t *v, int uvs,
      unsigned matrix, unsigned transfer, unsigned range,
      unsigned max_cll)
{
   int re, gd, ge, bd;
   int is_pq   = (transfer == 16);
   int full    = (range == 2);
   unsigned j, i;

   if (!rwebm_hbd_lut_ready || (is_pq && max_cll != rwebm_hbd_lut_peak))
      rwebm_hbd_init_luts(max_cll);
   rwebm_hbd_matrix(matrix ? matrix : (is_pq ? 9u : 1u), &re, &gd, &ge, &bd);

   for (j = 0; j < h; j++)
   {
      const uint16_t *yr = y + (size_t)j * ys;
      const uint16_t *ur = u + (size_t)(j >> 1) * uvs;
      const uint16_t *vr = v + (size_t)(j >> 1) * uvs;
      uint32_t       *dr = dst + (size_t)j * dst_stride;
      for (i = 0; i < w; i++)
      {
         int c, d, e, r, g, b;
         if (full)
         {
            c = ((int)yr[i] * 877) >> 10;
            d = (((int)ur[i >> 1] - 512) * 897) >> 10;
            e = (((int)vr[i >> 1] - 512) * 897) >> 10;
         }
         else
         {
            c = (int)yr[i] - 64;
            d = (int)ur[i >> 1] - 512;
            e = (int)vr[i >> 1] - 512;
         }
         r = (298 * c + re * e + 128) >> 8;
         g = (298 * c - gd * d - ge * e + 128) >> 8;
         b = (298 * c + bd * d + 128) >> 8;
         if (r < 0) r = 0; else if (r > 1023) r = 1023;
         if (g < 0) g = 0; else if (g > 1023) g = 1023;
         if (b < 0) b = 0; else if (b > 1023) b = 1023;
         if (is_pq)
         {
            /* tone-mapped linear, then 2020 -> 709 gamut, then sRGB,
             * emitted at 10-bit precision. */
            int lr = rwebm_hbd_pq_lut[r];
            int lg = rwebm_hbd_pq_lut[g];
            int lb = rwebm_hbd_pq_lut[b];
            int xr = (6801 * lr - 2407 * lg -  298 * lb) >> 12;
            int xg = ( -510 * lr + 4640 * lg -   34 * lb) >> 12;
            int xb = (  -75 * lr -  412 * lg + 4583 * lb) >> 12;
            if (xr < 0) xr = 0; else if (xr > 65535) xr = 65535;
            if (xg < 0) xg = 0; else if (xg > 65535) xg = 65535;
            if (xb < 0) xb = 0; else if (xb > 65535) xb = 65535;
            r = rwebm_hbd_out_lut10[xr >> 3];
            g = rwebm_hbd_out_lut10[xg >> 3];
            b = rwebm_hbd_out_lut10[xb >> 3];
         }
         /* non-PQ: r/g/b are already 10-bit SDR-encoded, keep as-is */
         /* Pack XRGB2101010: bits [29:20]=R [19:10]=G [9:0]=B; top 2
          * bits set so the value reads opaque under an A2 interpretation. */
         dr[i] = ((uint32_t)r << 20)
               | ((uint32_t)g << 10)
               |  (uint32_t)b
               | 0xC0000000u;
      }
   }
}
