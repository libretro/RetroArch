/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------
 * The following license statement only applies to this file (rwebm_video.c).
 * ---------------------------------------------------------------------
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
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* WebM video-to-image glue: rwebm demuxer + rvp8/rvp9 decoders exposed
 * through the still-image and streaming-animation contracts that
 * image_transfer.c dispatches on (see rwebm_video.h). */

#include <stdlib.h>
#include <string.h>
#include <math.h>

#if defined(__SSE2__)
#include <emmintrin.h>
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#include <retro_inline.h>

#include <formats/image.h>
#include <formats/rwebm.h>
#include <formats/rvp8.h>
#ifdef HAVE_RVP9
#include <formats/rvp9.h>
#endif
#include <formats/rwebm_video.h>

/* Per-packet timestamps are pre-scanned at open so every frame's display
 * duration is known without lookahead decoding; cap the table so a
 * pathological stream cannot balloon it. Frames past the cap reuse the
 * last stored delta. */
#define RWEBM_VIDEO_MAX_TS       8192

/* A VP9 superframe carries at most 8 sub-frames. */
#define RWEBM_VIDEO_MAX_SUPER    8

struct rwebm_video_stream
{
   rwebm_t     *demux;
   rvp8_video  *vp8;
#ifdef HAVE_RVP9
   rvp9_dec    *vp9;
#endif
   uint32_t    *frame;      /* width * height ABGR words              */
   int64_t     *ts;         /* pre-scanned packet timestamps (ns)     */
   int          ts_count;   /* entries stored in ts                   */
   int          num_frames; /* total video packets in the stream      */
   int          pkt_idx;    /* ordinal of the next video packet       */
   int          track;      /* index of the chosen video track        */
   enum rwebm_codec codec;
   unsigned     width;
   unsigned     height;
};

struct rwebm_video
{
   const uint8_t *buf;
   size_t         len;
};

/* ------------------------------------------------------------------ */
/* 8-bit limited-range YCbCr coefficient sets, <<8: {re, gd, ge, bd}.  */
/* Untagged content defaults to BT.601 below 720 lines and BT.709 at   */
/* or above it, matching industry convention.                          */
/* ------------------------------------------------------------------ */
static const int16_t rwebm_video_coef_601[4]  = { 409, 100, 208, 516 };
static const int16_t rwebm_video_coef_709[4]  = { 459,  55, 136, 541 };
static const int16_t rwebm_video_coef_2020[4] = { 431,  48, 167, 548 };

static const int16_t *rwebm_video_coefs(unsigned matrix, unsigned height)
{
   switch (matrix)
   {
      case 1:            return rwebm_video_coef_709;
      case 5: case 6:    return rwebm_video_coef_601;
      case 9: case 10:   return rwebm_video_coef_2020;
      default:           return height >= 720
                            ? rwebm_video_coef_709 : rwebm_video_coef_601;
   }
}

/* ------------------------------------------------------------------ */
/* BT.601 limited-range I420 -> ABGR words (memory R,G,B,A on LE),     */
/* the packing the animated-WebP stream emits.                         */
/* ------------------------------------------------------------------ */
static INLINE uint32_t rwebm_video_yuv_px(int y, int u, int v,
      const int16_t *k)
{
   int c = 298 * (y - 16);
   int d = u - 128;
   int e = v - 128;
   int r = (c + k[0] * e + 128) >> 8;
   int g = (c - k[1] * d - k[2] * e + 128) >> 8;
   int b = (c + k[3] * d + 128) >> 8;
   if (r < 0)
      r = 0;
   else if (r > 255)
      r = 255;
   if (g < 0)
      g = 0;
   else if (g > 255)
      g = 255;
   if (b < 0)
      b = 0;
   else if (b > 255)
      b = 255;
   return 0xFF000000u
        | ((uint32_t)b << 16)
        | ((uint32_t)g << 8)
        |  (uint32_t)r;
}

#if defined(__SSE2__)
/* 8 pixels per iteration with pmaddwd pairs. Bit-exact with the scalar
 * path: pmaddwd/paddd/psrad reproduce the integer arithmetic (psrad is
 * an arithmetic shift, as the scalar's >> is on int), and the
 * packs/packus saturation chain is exactly the scalar's clamp - the
 * pre-clamp channel range (about -223..481 for 8-bit input) fits int16
 * without distortion. */
static void rwebm_video_yuv_row_sse2(uint32_t *dr,
      const uint8_t *yr, const uint8_t *ur, const uint8_t *vr, unsigned w, const int16_t *k)
{
   const __m128i k16   = _mm_set1_epi16(16);
   const __m128i k128  = _mm_set1_epi16(128);
   const __m128i zero  = _mm_setzero_si128();
   const __m128i ones  = _mm_set1_epi16(1);
   const __m128i a255  = _mm_set1_epi8((char)0xFF);
   /* Packs two int16 coefficients into the int32 lane pmaddwd expects,
    * without shifting a negative value (all arithmetic unsigned). */
#define RWEBM_PAIR16(hi, lo) \
   ((int32_t)(((uint32_t)(uint16_t)(int16_t)(hi) << 16) \
            |  (uint32_t)(uint16_t)(int16_t)(lo)))
   const __m128i c_r   = _mm_set1_epi32(RWEBM_PAIR16( k[0], 298));
   const __m128i c_g1  = _mm_set1_epi32(RWEBM_PAIR16(-k[1], 298));
   const __m128i c_g2  = _mm_set1_epi32(RWEBM_PAIR16( 128, -k[2]));
   const __m128i c_b   = _mm_set1_epi32(RWEBM_PAIR16( k[3], 298));
#undef RWEBM_PAIR16
   const __m128i rnd   = _mm_set1_epi32(128);
   unsigned i;

   for (i = 0; i + 8 <= w; i += 8)
   {
      int32_t utmp, vtmp;
      __m128i y8, ysub, u4, v4, d, e;
      __m128i ye_lo, ye_hi, yd_lo, yd_hi, e1_lo, e1_hi;
      __m128i r_lo, r_hi, g_lo, g_hi, b_lo, b_hi;
      __m128i r16, g16, b16, r8, g8, b8, rg, ba;

      /* ysub: 8 x i16 = y - 16 */
      y8   = _mm_loadl_epi64((const __m128i*)(yr + i));
      ysub = _mm_sub_epi16(_mm_unpacklo_epi8(y8, zero), k16);
      /* d/e: 4 chroma samples each duplicated to 8 x i16, minus 128
       * (memcpy avoids an unaligned int load) */
      memcpy(&utmp, ur + (i >> 1), sizeof(utmp));
      memcpy(&vtmp, vr + (i >> 1), sizeof(vtmp));
      u4 = _mm_cvtsi32_si128(utmp);
      v4 = _mm_cvtsi32_si128(vtmp);
      d  = _mm_sub_epi16(
            _mm_unpacklo_epi8(_mm_unpacklo_epi8(u4, u4), zero), k128);
      e  = _mm_sub_epi16(
            _mm_unpacklo_epi8(_mm_unpacklo_epi8(v4, v4), zero), k128);

      ye_lo = _mm_unpacklo_epi16(ysub, e);
      ye_hi = _mm_unpackhi_epi16(ysub, e);
      yd_lo = _mm_unpacklo_epi16(ysub, d);
      yd_hi = _mm_unpackhi_epi16(ysub, d);
      e1_lo = _mm_unpacklo_epi16(e, ones);
      e1_hi = _mm_unpackhi_epi16(e, ones);

      /* r = (298*ysub + 409*e + 128) >> 8 */
      r_lo = _mm_srai_epi32(_mm_add_epi32(
            _mm_madd_epi16(ye_lo, c_r), rnd), 8);
      r_hi = _mm_srai_epi32(_mm_add_epi32(
            _mm_madd_epi16(ye_hi, c_r), rnd), 8);
      /* g = (298*ysub - 100*d - 208*e + 128) >> 8
       *   = (madd(ysub,d; 298,-100) + madd(e,1; -208,128)) >> 8 */
      g_lo = _mm_srai_epi32(_mm_add_epi32(
            _mm_madd_epi16(yd_lo, c_g1), _mm_madd_epi16(e1_lo, c_g2)), 8);
      g_hi = _mm_srai_epi32(_mm_add_epi32(
            _mm_madd_epi16(yd_hi, c_g1), _mm_madd_epi16(e1_hi, c_g2)), 8);
      /* b = (298*ysub + 516*d + 128) >> 8 */
      b_lo = _mm_srai_epi32(_mm_add_epi32(
            _mm_madd_epi16(yd_lo, c_b), rnd), 8);
      b_hi = _mm_srai_epi32(_mm_add_epi32(
            _mm_madd_epi16(yd_hi, c_b), rnd), 8);

      /* Saturating packs implement the 0..255 clamp */
      r16 = _mm_packs_epi32(r_lo, r_hi);
      g16 = _mm_packs_epi32(g_lo, g_hi);
      b16 = _mm_packs_epi32(b_lo, b_hi);
      r8  = _mm_packus_epi16(r16, r16);
      g8  = _mm_packus_epi16(g16, g16);
      b8  = _mm_packus_epi16(b16, b16);

      /* Interleave to memory order R,G,B,A (ABGR words) */
      rg  = _mm_unpacklo_epi8(r8, g8);
      ba  = _mm_unpacklo_epi8(b8, a255);
      _mm_storeu_si128((__m128i*)(dr + i),
            _mm_unpacklo_epi16(rg, ba));
      _mm_storeu_si128((__m128i*)(dr + i + 4),
            _mm_unpackhi_epi16(rg, ba));
   }
   for (; i < w; i++)
      dr[i] = rwebm_video_yuv_px(yr[i], ur[i >> 1], vr[i >> 1], k);
}
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
/* NEON translation of the SSE2 kernel above: identical integer
 * arithmetic (widening multiply-accumulate into i32, arithmetic shift,
 * saturating narrows for the clamp), so results are byte-identical to
 * the scalar path. */
static void rwebm_video_yuv_row_neon(uint32_t *dr,
      const uint8_t *yr, const uint8_t *ur, const uint8_t *vr, unsigned w,
      const int16_t *kc)
{
   const int16x8_t k16  = vdupq_n_s16(16);
   const int16x8_t k128 = vdupq_n_s16(128);
   const int32x4_t rnd  = vdupq_n_s32(128);
   unsigned i;

   for (i = 0; i + 8 <= w; i += 8)
   {
      uint8x8_t y8, u8, v8;
      int16x8_t ysub, d, e;
      int32x4_t c_lo, c_hi, r_lo, r_hi, g_lo, g_hi, b_lo, b_hi;
      int16x8_t r16, g16, b16;
      uint8x8x4_t out;
      uint8_t utmp[8], vtmp[8];
      unsigned k;

      y8   = vld1_u8(yr + i);
      ysub = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(y8)), k16);
      for (k = 0; k < 4; k++)
      {
         utmp[2*k] = utmp[2*k+1] = ur[(i >> 1) + k];
         vtmp[2*k] = vtmp[2*k+1] = vr[(i >> 1) + k];
      }
      u8 = vld1_u8(utmp);
      v8 = vld1_u8(vtmp);
      d  = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(u8)), k128);
      e  = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(v8)), k128);

      /* c = 298*ysub + 128 (rounding folded in) */
      c_lo = vmlal_n_s16(rnd, vget_low_s16(ysub),  298);
      c_hi = vmlal_n_s16(rnd, vget_high_s16(ysub), 298);

      r_lo = vshrq_n_s32(vmlal_n_s16(c_lo, vget_low_s16(e),  kc[0]), 8);
      r_hi = vshrq_n_s32(vmlal_n_s16(c_hi, vget_high_s16(e), kc[0]), 8);
      g_lo = vshrq_n_s32(vmlsl_n_s16(vmlsl_n_s16(c_lo,
               vget_low_s16(d), kc[1]), vget_low_s16(e), kc[2]), 8);
      g_hi = vshrq_n_s32(vmlsl_n_s16(vmlsl_n_s16(c_hi,
               vget_high_s16(d), kc[1]), vget_high_s16(e), kc[2]), 8);
      b_lo = vshrq_n_s32(vmlal_n_s16(c_lo, vget_low_s16(d),  kc[3]), 8);
      b_hi = vshrq_n_s32(vmlal_n_s16(c_hi, vget_high_s16(d), kc[3]), 8);

      /* Saturating narrows implement the 0..255 clamp */
      r16 = vcombine_s16(vqmovn_s32(r_lo), vqmovn_s32(r_hi));
      g16 = vcombine_s16(vqmovn_s32(g_lo), vqmovn_s32(g_hi));
      b16 = vcombine_s16(vqmovn_s32(b_lo), vqmovn_s32(b_hi));

      out.val[0] = vqmovun_s16(r16);
      out.val[1] = vqmovun_s16(g16);
      out.val[2] = vqmovun_s16(b16);
      out.val[3] = vdup_n_u8(0xFF);
      vst4_u8((uint8_t*)(dr + i), out);
   }
   for (; i < w; i++)
      dr[i] = rwebm_video_yuv_px(yr[i], ur[i >> 1], vr[i >> 1], kc);
}
#endif

static void rwebm_video_blit_i420(uint32_t *dst, unsigned dst_stride,
      unsigned w, unsigned h,
      const uint8_t *y, int ys,
      const uint8_t *u, const uint8_t *v, int uvs,
      unsigned matrix)
{
   const int16_t *k = rwebm_video_coefs(matrix, h);
   unsigned j;
   for (j = 0; j < h; j++)
   {
      const uint8_t *yr = y + (size_t)j * ys;
      const uint8_t *ur = u + (size_t)(j >> 1) * uvs;
      const uint8_t *vr = v + (size_t)(j >> 1) * uvs;
      uint32_t      *dr = dst + (size_t)j * dst_stride;
#if defined(__SSE2__)
      rwebm_video_yuv_row_sse2(dr, yr, ur, vr, w, k);
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
      rwebm_video_yuv_row_neon(dr, yr, ur, vr, w, k);
#else
      {
         unsigned i;
         for (i = 0; i < w; i++)
            dr[i] = rwebm_video_yuv_px(yr[i], ur[i >> 1], vr[i >> 1], k);
      }
#endif
   }
}


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
      rwebm_hbd_out_lut[i] = (uint8_t)(s * 255.0 + 0.5);
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
/* VP9 superframe index (parsed from the trailing marker byte).        */
/* Returns the number of sub-frames and their sizes; 1 = whole chunk.  */
/* ------------------------------------------------------------------ */
#ifdef HAVE_RVP9
static int rwebm_video_vp9_superframe(const uint8_t *data, size_t size,
      size_t *sizes, int max_frames)
{
   uint8_t marker;
   int frames, bytes_per_size, i, k;
   size_t index_size, total;
   const uint8_t *x;

   if (size < 2)
      goto whole;
   marker = data[size - 1];
   if ((marker & 0xe0) != 0xc0)
      goto whole;
   frames         = (marker & 0x7) + 1;
   bytes_per_size = ((marker >> 3) & 0x3) + 1;
   index_size     = 2 + (size_t)frames * bytes_per_size;
   if (size < index_size || data[size - index_size] != marker)
      goto whole;
   if (frames > max_frames)
      goto whole;
   x     = data + size - index_size + 1;
   total = 0;
   for (i = 0; i < frames; i++)
   {
      size_t sz = 0;
      for (k = 0; k < bytes_per_size; k++)
         sz |= (size_t)*x++ << (k * 8);
      sizes[i] = sz;
      total   += sz;
   }
   if (total > size - index_size)
      goto whole;
   return frames;

whole:
   sizes[0] = size;
   return 1;
}
#endif

/* ------------------------------------------------------------------ */
/* Decoder lifecycle                                                   */
/* ------------------------------------------------------------------ */
static bool rwebm_video_stream_open_decoder(rwebm_video_stream_t *s)
{
   switch (s->codec)
   {
      case RWEBM_CODEC_VP8:
         if (!(s->vp8 = rvp8_video_open()))
            return false;
         return true;
#ifdef HAVE_RVP9
      case RWEBM_CODEC_VP9:
         if (!(s->vp9 = (rvp9_dec*)calloc(1, sizeof(*s->vp9))))
            return false;
         return true;
#endif
      default:
         break;
   }
   return false;
}

static void rwebm_video_stream_close_decoder(rwebm_video_stream_t *s)
{
   if (s->vp8)
   {
      rvp8_video_close(s->vp8);
      s->vp8 = NULL;
   }
#ifdef HAVE_RVP9
   if (s->vp9)
   {
      rvp9_free(s->vp9);
      free(s->vp9);
      s->vp9 = NULL;
   }
#endif
}

/* ------------------------------------------------------------------ */
/* Streaming animation                                                 */
/* ------------------------------------------------------------------ */
rwebm_video_stream_t *rwebm_video_stream_open(const uint8_t *buf,
      size_t len)
{
   rwebm_video_stream_t *s;
   const rwebm_track *trk = NULL;
   rwebm_packet pkt;
   int i, num_tracks;

   if (!buf || !len)
      return NULL;

   if (!(s = (rwebm_video_stream_t*)calloc(1, sizeof(*s))))
      return NULL;

   if (!(s->demux = rwebm_open_memory(buf, len)))
      goto fail;

   /* Pick the first video track whose codec we can decode. */
   s->track   = -1;
   num_tracks = rwebm_num_tracks(s->demux);
   for (i = 0; i < num_tracks; i++)
   {
      const rwebm_track *t = rwebm_get_track(s->demux, i);
      if (!t || t->type != RWEBM_TRACK_VIDEO)
         continue;
      if (t->width < 1 || t->height < 1)
         continue;
      if (t->codec != RWEBM_CODEC_VP8
#ifdef HAVE_RVP9
          && t->codec != RWEBM_CODEC_VP9
#endif
         )
         continue;
      s->track  = i;
      s->codec  = t->codec;
      trk       = t;
      break;
   }
   if (s->track < 0)
      goto fail;

   s->width  = trk->width;
   s->height = trk->height;

   /* Pre-scan: count the track's packets and record their timestamps so
    * frame durations come straight from the container. This walks block
    * headers only (no decode). */
   if (!(s->ts = (int64_t*)malloc(RWEBM_VIDEO_MAX_TS * sizeof(int64_t))))
      goto fail;
   while (rwebm_read_packet(s->demux, &pkt) == 1)
   {
      if (pkt.track != s->track)
         continue;
      if (s->num_frames < RWEBM_VIDEO_MAX_TS)
         s->ts[s->ts_count++] = pkt.timestamp;
      s->num_frames++;
   }
   if (s->num_frames < 1)
      goto fail;
   rwebm_rewind(s->demux);

   if (!rwebm_video_stream_open_decoder(s))
      goto fail;

   if (!(s->frame = (uint32_t*)malloc(
         (size_t)s->width * s->height * sizeof(uint32_t))))
      goto fail;

   return s;

fail:
   rwebm_video_stream_close(s);
   return NULL;
}

void rwebm_video_stream_close(rwebm_video_stream_t *s)
{
   if (!s)
      return;
   rwebm_video_stream_close_decoder(s);
   if (s->demux)
      rwebm_close(s->demux);
   free(s->ts);
   free(s->frame);
   free(s);
}

void rwebm_video_stream_get_info(const rwebm_video_stream_t *s,
      unsigned *width, unsigned *height, int *num_frames, int *loop_count)
{
   if (!s)
      return;
   if (width)
      *width      = s->width;
   if (height)
      *height     = s->height;
   if (num_frames)
      *num_frames = s->num_frames;
   if (loop_count)
      *loop_count = 0;   /* video loops indefinitely */
}

/* Display duration of packet 'idx', in ms, from the pre-scanned
 * timestamp table; 0 when unknown (caller applies its default). */
static int rwebm_video_duration_ms(const rwebm_video_stream_t *s, int idx)
{
   int64_t delta_ns = 0;
   if (idx + 1 < s->ts_count)
      delta_ns = s->ts[idx + 1] - s->ts[idx];
   else if (s->ts_count >= 2)
      delta_ns = s->ts[s->ts_count - 1] - s->ts[s->ts_count - 2];
   if (delta_ns <= 0)
      return 0;
   return (int)(delta_ns / 1000000);
}

/* Decode one demuxed packet into s->frame. Returns 1 when a picture was
 * produced, 0 when the packet decoded but is not displayed, -1 on a
 * decode error. */
static int rwebm_video_decode_packet(rwebm_video_stream_t *s,
      const rwebm_packet *pkt)
{
   /* A decoder can be absent if the re-open in rewind hit OOM. */
   if (!s->vp8
#ifdef HAVE_RVP9
       && !s->vp9
#endif
      )
      return -1;
#ifdef HAVE_RVP9
   if (s->codec == RWEBM_CODEC_VP9)
   {
      size_t sizes[RWEBM_VIDEO_MAX_SUPER];
      const uint8_t *frame = pkt->data;
      int nf = rwebm_video_vp9_superframe(pkt->data, pkt->size,
            sizes, RWEBM_VIDEO_MAX_SUPER);
      int i, r, show, last_show = -1;
      for (i = 0; i < nf; i++)
      {
         r = rvp9_decode_frame(s->vp9, frame, sizes[i], &show);
         if (r < 0)
            return -1;
         if (show >= 0)
            last_show = show;
         frame += sizes[i];
      }
      if (last_show >= 0)
      {
         const rvp9_fb *fb = &s->vp9->fbs[last_show];
         unsigned w = (unsigned)fb->w < s->width  ? (unsigned)fb->w : s->width;
         unsigned h = (unsigned)fb->h < s->height ? (unsigned)fb->h : s->height;
         if (s->vp9->hd.bit_depth == 10)
         {
            const rwebm_track *ct = rwebm_get_track(s->demux, s->track);
            rwebm_video_blit_i420_hbd(s->frame, s->width, w, h,
                  (const uint16_t*)fb->y, s->vp9->ys,
                  (const uint16_t*)fb->u, (const uint16_t*)fb->v,
                  s->vp9->uvs,
                  ct ? ct->matrix_coefficients : 0,
                  ct ? ct->transfer_characteristics : 0,
                  ct ? ct->colour_range : 0,
                  ct ? ct->max_cll : 0, 1);
         }
         else
         {
            const rwebm_track *ct = rwebm_get_track(s->demux, s->track);
            rwebm_video_blit_i420(s->frame, s->width, w, h,
                  fb->y, s->vp9->ys, fb->u, fb->v, s->vp9->uvs,
                  ct ? ct->matrix_coefficients : 0);
         }
         return 1;
      }
      return 0;
   }
#endif
   if (s->codec == RWEBM_CODEC_VP8)
   {
      const uint8_t *y, *u, *v;
      int ys, uvs, w, h, cw, ch;
      /* VP8 frame tag: bit 4 of byte 0 is show_frame. */
      int shown = (pkt->size > 0) && (pkt->data[0] & 0x10);
      if (rvp8_video_decode(s->vp8, pkt->data, pkt->size) != 0)
         return -1;
      if (!shown)
         return 0;
      y = rvp8_video_plane(s->vp8, 0, &ys, &w, &h);
      u = rvp8_video_plane(s->vp8, 1, &uvs, &cw, &ch);
      v = rvp8_video_plane(s->vp8, 2, &uvs, &cw, &ch);
      if (!y || !u || !v)
         return -1;
      if ((unsigned)w > s->width)
         w = (int)s->width;
      if ((unsigned)h > s->height)
         h = (int)s->height;
      {
         const rwebm_track *ct = rwebm_get_track(s->demux, s->track);
         rwebm_video_blit_i420(s->frame, s->width,
               (unsigned)w, (unsigned)h, y, ys, u, v, uvs,
               ct ? ct->matrix_coefficients : 0);
      }
      return 1;
   }
   return -1;
}

const uint32_t *rwebm_video_stream_next(rwebm_video_stream_t *s,
      int *duration_ms)
{
   rwebm_packet pkt;

   if (!s)
      return NULL;

   while (rwebm_read_packet(s->demux, &pkt) == 1)
   {
      int idx, r;
      if (pkt.track != s->track)
         continue;
      idx = s->pkt_idx++;
      r   = rwebm_video_decode_packet(s, &pkt);
      if (r < 0)
         return NULL;    /* decode error: end the animation */
      if (r == 0)
         continue;       /* non-shown frame: keep going      */
      if (duration_ms)
         *duration_ms = rwebm_video_duration_ms(s, idx);
      return s->frame;
   }
   return NULL;           /* end of one pass */
}

void rwebm_video_stream_rewind(rwebm_video_stream_t *s)
{
   if (!s)
      return;
   rwebm_rewind(s->demux);
   s->pkt_idx = 0;
   /* The stream restarts at a key frame, so a fresh decoder (empty
    * reference chain, default probabilities) is the correct state. */
   rwebm_video_stream_close_decoder(s);
   rwebm_video_stream_open_decoder(s);
}

/* ------------------------------------------------------------------ */
/* Still image (first displayed frame)                                 */
/* ------------------------------------------------------------------ */
rwebm_video_t *rwebm_video_alloc(void)
{
   return (rwebm_video_t*)calloc(1, sizeof(rwebm_video_t));
}

void rwebm_video_free(rwebm_video_t *webm)
{
   free(webm);
}

bool rwebm_video_set_buf_ptr(rwebm_video_t *webm, void *data, size_t len)
{
   if (!webm)
      return false;
   webm->buf = (const uint8_t*)data;
   webm->len = len;
   return true;
}

int rwebm_video_process_image(rwebm_video_t *webm, void **buf,
      size_t len, unsigned *width, unsigned *height, bool supports_rgba)
{
   rwebm_video_stream_t *s;
   const uint32_t *frame;
   uint32_t *out;
   size_t i, n;
   int duration_ms = 0;

   (void)len;

   if (!webm || !webm->buf || !buf)
      return IMAGE_PROCESS_ERROR;

   if (!(s = rwebm_video_stream_open(webm->buf, webm->len)))
      return IMAGE_PROCESS_ERROR;

   if (!(frame = rwebm_video_stream_next(s, &duration_ms)))
   {
      rwebm_video_stream_close(s);
      return IMAGE_PROCESS_ERROR;
   }

   n = (size_t)s->width * s->height;
   if (!(out = (uint32_t*)malloc(n * sizeof(uint32_t))))
   {
      rwebm_video_stream_close(s);
      return IMAGE_PROCESS_ERROR;
   }

   if (supports_rgba)
      memcpy(out, frame, n * sizeof(uint32_t));
   else
   {
      /* ABGR words -> ARGB words (swap R and B channels). */
      for (i = 0; i < n; i++)
      {
         uint32_t px = frame[i];
         out[i]      = (px & 0xFF00FF00u)
                     | ((px & 0xFFu) << 16)
                     | ((px >> 16) & 0xFFu);
      }
   }

   if (width)
      *width  = s->width;
   if (height)
      *height = s->height;
   *buf = out;

   rwebm_video_stream_close(s);
   return IMAGE_PROCESS_END;
}
