/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "softfilter.h"
#include <stdlib.h>
#include <string.h>

/* Vector runs over blocks that are only copied (see TWOXSAI_VEC_ROWS
 * and softfilter_simd.h) */
#include "softfilter_simd.h"

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation twoxsai_get_implementation
#define softfilter_thread_data twoxsai_softfilter_thread_data
#define filter_data twoxsai_filter_data
#endif

#define TWOXSAI_SCALE 2

#if defined(SOFTFILTER_HAVE_SSE2) || defined(SOFTFILTER_HAVE_NEON)
#define TWOXSAI_VECTOR 1
#endif

/* One vector of pixels from x, which must end before the row's last
 * pixel: writes every pixel twice across on both output rows, as if it
 * equalled the pixel after it and the one below it, and returns a bit
 * per pixel (lowest first) for those that do not, which the caller then
 * runs the full rule on. r0 is the pixels' row and r1 the one below it
 * (the same row on the frame's last); dst the first of the two output
 * rows. */
typedef unsigned (*twoxsai_run_rgb565_t)(const uint16_t *r0,
      const uint16_t *r1, uint16_t *dst, size_t dst_stride, unsigned x);
typedef unsigned (*twoxsai_run_xrgb8888_t)(const uint32_t *r0,
      const uint32_t *r1, uint32_t *dst, size_t dst_stride, unsigned x);

struct softfilter_thread_data
{
   void *out_data;
   const void *in_data;
   size_t out_pitch;
   size_t in_pitch;
   unsigned colfmt;
   unsigned width;
   unsigned height;
   int first;
   /* frame rows below this worker's slice, which its neighbourhood
    * may read into */
   unsigned below;
};

struct filter_data
{
   unsigned threads;
   struct softfilter_thread_data *workers;
   unsigned in_fmt;
   /* The vector runs for this machine, or NULL, and their widths */
   twoxsai_run_rgb565_t run_rgb565;
   twoxsai_run_xrgb8888_t run_xrgb8888;
   unsigned lanes_rgb565;
   unsigned lanes_xrgb8888;
};

static unsigned twoxsai_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_RGB565 | SOFTFILTER_FMT_XRGB8888;
}

static unsigned twoxsai_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned twoxsai_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

/* A pixel A equal to the pixel B after it and the pixel C below it
 * comes out as four copies of it, whatever D (below B) is: with
 * A == D the full rule takes its third case, where A == B makes all
 * three products A; otherwise its second (B == C, A != D), whose
 * products are B, C or a blend of A with B or C, all A. The runs test
 * that for a vector of pixels. */

#ifdef SOFTFILTER_HAVE_SSE2
#define TWOXSAI_LD(p) _mm_loadu_si128((const __m128i*)(p))

static unsigned twoxsai_run_rgb565_sse2(const uint16_t *r0,
      const uint16_t *r1, uint16_t *dst, size_t dst_stride, unsigned x)
{
   __m128i a  = TWOXSAI_LD(r0 + x);
   __m128i ok = _mm_and_si128(_mm_cmpeq_epi16(a, TWOXSAI_LD(r0 + x + 1)),
         _mm_cmpeq_epi16(a, TWOXSAI_LD(r1 + x)));
   __m128i lo = _mm_unpacklo_epi16(a, a);
   __m128i hi = _mm_unpackhi_epi16(a, a);
   _mm_storeu_si128((__m128i*)(dst + 2 * x),     lo);
   _mm_storeu_si128((__m128i*)(dst + 2 * x + 8), hi);
   _mm_storeu_si128((__m128i*)(dst + dst_stride + 2 * x),     lo);
   _mm_storeu_si128((__m128i*)(dst + dst_stride + 2 * x + 8), hi);
   return ~(unsigned)_mm_movemask_epi8(
         _mm_packs_epi16(ok, _mm_setzero_si128())) & 0xFFu;
}

static unsigned twoxsai_run_xrgb8888_sse2(const uint32_t *r0,
      const uint32_t *r1, uint32_t *dst, size_t dst_stride, unsigned x)
{
   __m128i a  = TWOXSAI_LD(r0 + x);
   __m128i ok = _mm_and_si128(_mm_cmpeq_epi32(a, TWOXSAI_LD(r0 + x + 1)),
         _mm_cmpeq_epi32(a, TWOXSAI_LD(r1 + x)));
   __m128i lo = _mm_unpacklo_epi32(a, a);
   __m128i hi = _mm_unpackhi_epi32(a, a);
   _mm_storeu_si128((__m128i*)(dst + 2 * x),     lo);
   _mm_storeu_si128((__m128i*)(dst + 2 * x + 4), hi);
   _mm_storeu_si128((__m128i*)(dst + dst_stride + 2 * x),     lo);
   _mm_storeu_si128((__m128i*)(dst + dst_stride + 2 * x + 4), hi);
   return ~(unsigned)_mm_movemask_ps(_mm_castsi128_ps(ok)) & 0xFu;
}
#endif /* SOFTFILTER_HAVE_SSE2 */

#ifdef SOFTFILTER_HAVE_AVX2
#define TWOXSAI_LD_256(p) _mm256_loadu_si256((const __m256i*)(p))

/* unpacklo/unpackhi double each 128-bit half; the permutes put the
 * doubled pixels back in order. */
#define TWOXSAI_STORE_DOUBLED_256(dst, x, lo, hi, n) { \
      __m256i a_ = _mm256_permute2x128_si256(lo, hi, 0x20); \
      __m256i b_ = _mm256_permute2x128_si256(lo, hi, 0x31); \
      _mm256_storeu_si256((__m256i*)((dst) + 2 * (x)),     a_); \
      _mm256_storeu_si256((__m256i*)((dst) + 2 * (x) + n), b_); \
      _mm256_storeu_si256((__m256i*)((dst) + dst_stride + 2 * (x)),     a_); \
      _mm256_storeu_si256((__m256i*)((dst) + dst_stride + 2 * (x) + n), b_); \
}

SOFTFILTER_TARGET_AVX2
static unsigned twoxsai_run_rgb565_avx2(const uint16_t *r0,
      const uint16_t *r1, uint16_t *dst, size_t dst_stride, unsigned x)
{
   __m256i a  = TWOXSAI_LD_256(r0 + x);
   __m256i ok = _mm256_and_si256(_mm256_cmpeq_epi16(a, TWOXSAI_LD_256(r0 + x + 1)),
         _mm256_cmpeq_epi16(a, TWOXSAI_LD_256(r1 + x)));
   __m256i lo = _mm256_unpacklo_epi16(a, a);
   __m256i hi = _mm256_unpackhi_epi16(a, a);
   unsigned m;
   TWOXSAI_STORE_DOUBLED_256(dst, x, lo, hi, 16)
   m = ~(unsigned)_mm_movemask_epi8(_mm_packs_epi16(
            _mm256_castsi256_si128(ok), _mm256_extracti128_si256(ok, 1)))
      & 0xFFFFu;
   SOFTFILTER_ZEROUPPER();
   return m;
}

SOFTFILTER_TARGET_AVX2
static unsigned twoxsai_run_xrgb8888_avx2(const uint32_t *r0,
      const uint32_t *r1, uint32_t *dst, size_t dst_stride, unsigned x)
{
   __m256i a  = TWOXSAI_LD_256(r0 + x);
   __m256i ok = _mm256_and_si256(_mm256_cmpeq_epi32(a, TWOXSAI_LD_256(r0 + x + 1)),
         _mm256_cmpeq_epi32(a, TWOXSAI_LD_256(r1 + x)));
   __m256i lo = _mm256_unpacklo_epi32(a, a);
   __m256i hi = _mm256_unpackhi_epi32(a, a);
   unsigned m;
   TWOXSAI_STORE_DOUBLED_256(dst, x, lo, hi, 8)
   m = ~(unsigned)_mm256_movemask_ps(_mm256_castsi256_ps(ok)) & 0xFFu;
   SOFTFILTER_ZEROUPPER();
   return m;
}
#endif /* SOFTFILTER_HAVE_AVX2 */

#ifdef SOFTFILTER_HAVE_NEON
/* A bit per byte of an all-ones-or-zero mask of 8 bytes, set where zero */
static INLINE unsigned twoxsai_zero_bytes_neon(uint8x8_t m)
{
   uint64_t v = vget_lane_u64(vreinterpret_u64_u8(m), 0);
   unsigned bits = 0, j;
   for (j = 0; j < 8; j++)
      if (!((v >> (8 * j)) & 0xFF))
         bits |= 1u << j;
   return bits;
}

static unsigned twoxsai_run_rgb565_neon(const uint16_t *r0,
      const uint16_t *r1, uint16_t *dst, size_t dst_stride, unsigned x)
{
   uint16x8_t a  = vld1q_u16(r0 + x);
   uint16x8_t ok = vandq_u16(vceqq_u16(a, vld1q_u16(r0 + x + 1)),
         vceqq_u16(a, vld1q_u16(r1 + x)));
   uint16x8x2_t d = vzipq_u16(a, a);
   vst1q_u16(dst + 2 * x,     d.val[0]);
   vst1q_u16(dst + 2 * x + 8, d.val[1]);
   vst1q_u16(dst + dst_stride + 2 * x,     d.val[0]);
   vst1q_u16(dst + dst_stride + 2 * x + 8, d.val[1]);
   return twoxsai_zero_bytes_neon(vmovn_u16(ok));
}

static unsigned twoxsai_run_xrgb8888_neon(const uint32_t *r0,
      const uint32_t *r1, uint32_t *dst, size_t dst_stride, unsigned x)
{
   uint32x4_t a  = vld1q_u32(r0 + x);
   uint32x4_t ok = vandq_u32(vceqq_u32(a, vld1q_u32(r0 + x + 1)),
         vceqq_u32(a, vld1q_u32(r1 + x)));
   uint32x4x2_t d = vzipq_u32(a, a);
   vst1q_u32(dst + 2 * x,     d.val[0]);
   vst1q_u32(dst + 2 * x + 4, d.val[1]);
   vst1q_u32(dst + dst_stride + 2 * x,     d.val[0]);
   vst1q_u32(dst + dst_stride + 2 * x + 4, d.val[1]);
   /* four 32-bit lanes narrowed twice to bytes; the upper four are 0 */
   return twoxsai_zero_bytes_neon(vmovn_u16(vcombine_u16(vmovn_u32(ok),
               vdup_n_u16(0xFFFF)))) & 0xFu;
}
#endif /* SOFTFILTER_HAVE_NEON */

static void *twoxsai_generic_create(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata)
{
   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   if (!filt)
      return NULL;
   if (!(filt->workers = (struct softfilter_thread_data*)
      calloc(threads, sizeof(struct softfilter_thread_data))))
   {
      free(filt);
      return NULL;
   }
   /* Apparently the code is not thread-safe,
    * so force single threaded operation... */
   filt->threads = threads;
   filt->in_fmt  = in_fmt;
   #if defined(SOFTFILTER_HAVE_SSE2)
   filt->run_rgb565     = twoxsai_run_rgb565_sse2;
   filt->run_xrgb8888   = twoxsai_run_xrgb8888_sse2;
   filt->lanes_rgb565   = 8;
   filt->lanes_xrgb8888 = 4;
#ifdef SOFTFILTER_HAVE_AVX2
   if (simd & SOFTFILTER_SIMD_AVX2)
   {
      filt->run_rgb565     = twoxsai_run_rgb565_avx2;
      filt->run_xrgb8888   = twoxsai_run_xrgb8888_avx2;
      filt->lanes_rgb565   = 16;
      filt->lanes_xrgb8888 = 8;
   }
#endif
#elif defined(SOFTFILTER_HAVE_NEON)
   filt->run_rgb565     = twoxsai_run_rgb565_neon;
   filt->run_xrgb8888   = twoxsai_run_xrgb8888_neon;
   filt->lanes_rgb565   = 8;
   filt->lanes_xrgb8888 = 4;
#endif
   return filt;
}

static void twoxsai_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width  = width * TWOXSAI_SCALE;
   *out_height = height * TWOXSAI_SCALE;
}

static void twoxsai_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;

   if (!filt)
      return;

   free(filt->workers);
   free(filt);
}

#define twoxsai_interpolate_xrgb8888(A, B) ((((A) & 0xFEFEFEFE) >> 1) + (((B) & 0xFEFEFEFE) >> 1) + ((A) & (B) & 0x01010101))

#define twoxsai_interpolate2_xrgb8888(A, B, C, D) ((((A) & 0xFCFCFCFC) >> 2) + (((B) & 0xFCFCFCFC) >> 2) + (((C) & 0xFCFCFCFC) >> 2) + (((D) & 0xFCFCFCFC) >> 2) + (((((A) & 0x03030303) + ((B) & 0x03030303) + ((C) & 0x03030303) + ((D) & 0x03030303)) >> 2) & 0x03030303))

#define twoxsai_interpolate_rgb565(A, B) ((((A) & 0xF7DE) >> 1) + (((B) & 0xF7DE) >> 1) + ((A) & (B) & 0x0821))

#define twoxsai_interpolate2_rgb565(A, B, C, D) ((((A) & 0xE79C) >> 2) + (((B) & 0xE79C) >> 2) + (((C) & 0xE79C) >> 2) + (((D) & 0xE79C) >> 2)  + (((((A) & 0x1863) + ((B) & 0x1863) + ((C) & 0x1863) + ((D) & 0x1863)) >> 2) & 0x1863))

#define twoxsai_interpolate_4444(A, B) (((A & 0xEEEE) >> 1) + ((B & 0xEEEE) >> 1) + (A & B & 0x1111))
#define twoxsai_interpolate2_4444(A, B, C, D) (((A & 0xCCCC) >> 2) + ((B & 0xCCCC) >> 2) + ((C & 0xCCCC) >> 2) + ((D & 0xCCCC) >> 2) + ((((A & 0x3333) + (B & 0x3333) + (C & 0x3333) + (D & 0x3333)) >> 2) & 0x3333))

#define twoxsai_result(A, B, C, D) (((A) != (C) || (A) != (D)) - ((B) != (C) || (B) != (D)));

/* prevline/nextline/nextline2 are row offsets already clamped against
 * the top and bottom of the frame, and xm1/xp1/xp2 are column offsets
 * clamped against its left and right edge, so the 4x4 neighbourhood
 * never reaches outside the source. */
#define twoxsai_declare_variables(typename_t, in, prevline, nextline, nextline2, xm1, xp1, xp2) \
         typename_t product, product1, product2; \
         typename_t colorI = *(in - prevline + xm1); \
         typename_t colorE = *(in - prevline); \
         typename_t colorF = *(in - prevline + xp1); \
         typename_t colorJ = *(in - prevline + xp2); \
         typename_t colorG = *(in + xm1); \
         typename_t colorA = *(in); \
         typename_t colorB = *(in + xp1); \
         typename_t colorK = *(in + xp2); \
         typename_t colorH = *(in + nextline + xm1); \
         typename_t colorC = *(in + nextline); \
         typename_t colorD = *(in + nextline + xp1); \
         typename_t colorL = *(in + nextline + xp2); \
         typename_t colorM = *(in + nextline2 + xm1); \
         typename_t colorN = *(in + nextline2); \
         typename_t colorO = *(in + nextline2 + xp1);

#ifndef twoxsai_function
#define twoxsai_function(result_cb, interpolate_cb, interpolate2_cb) \
         if (colorA == colorD && colorB != colorC) \
         { \
            if ((colorA == colorE && colorB == colorL) || (colorA == colorC && colorA == colorF && colorB != colorE && colorB == colorJ)) \
               product = colorA; \
            else \
               product = interpolate_cb(colorA, colorB); \
            if ((colorA == colorG && colorC == colorO) || (colorA == colorB && colorA == colorH && colorG != colorC && colorC == colorM)) \
               product1 = colorA; \
            else \
               product1 = interpolate_cb(colorA, colorC); \
            product2 = colorA; \
         } else if (colorB == colorC && colorA != colorD) \
         { \
            if ((colorB == colorF && colorA == colorH) || (colorB == colorE && colorB == colorD && colorA != colorF && colorA == colorI)) \
               product = colorB; \
            else \
               product = interpolate_cb(colorA, colorB); \
            if ((colorC == colorH && colorA == colorF) || (colorC == colorG && colorC == colorD && colorA != colorH && colorA == colorI)) \
               product1 = colorC; \
            else \
               product1 = interpolate_cb(colorA, colorC); \
            product2 = colorB; \
         } \
         else if (colorA == colorD && colorB == colorC) \
         { \
            if (colorA == colorB) \
            { \
               product  = colorA; \
               product1 = colorA; \
               product2 = colorA; \
            } \
            else \
            { \
               int r    = 0; \
               product1 = interpolate_cb(colorA, colorC); \
               product  = interpolate_cb(colorA, colorB); \
               r       += result_cb(colorA, colorB, colorG, colorE); \
               r       += result_cb(colorB, colorA, colorK, colorF); \
               r       += result_cb(colorB, colorA, colorH, colorN); \
               r       += result_cb(colorA, colorB, colorL, colorO); \
               if (r > 0) \
                  product2 = colorA; \
               else if (r < 0) \
                  product2 = colorB; \
               else \
                  product2 = interpolate2_cb(colorA, colorB, colorC, colorD); \
            } \
         } \
         else \
         { \
            product2 = interpolate2_cb(colorA, colorB, colorC, colorD); \
            if (colorA == colorC && colorA == colorF && colorB != colorE && colorB == colorJ) \
               product = colorA; \
            else if (colorB == colorE && colorB == colorD && colorA != colorF && colorA == colorI) \
               product = colorB; \
            else \
               product = interpolate_cb(colorA, colorB); \
            if (colorA == colorB && colorA == colorH && colorG != colorC && colorC == colorM) \
               product1 = colorA; \
            else if (colorC == colorG && colorC == colorD && colorA != colorH && colorA == colorI) \
               product1 = colorC; \
            else \
               product1 = interpolate_cb(colorA, colorC); \
         } \
         out[0]              = colorA; \
         out[1]              = product; \
         out[dst_stride]     = product1; \
         out[dst_stride + 1] = product2; \
         ++in; \
         out += 2
#endif

#ifdef TWOXSAI_VECTOR
/* The full rule for pixel x of a row, with its neighbourhood clamped as
 * in the plain loops below. One function per format, for both the
 * vector and the one-by-one paths. */
#define TWOXSAI_FULL_FN(NAME, TYPE, INTERP, INTERP2) \
static SOFTFILTER_FORCE_INLINE void NAME(const TYPE *row, unsigned x, \
      unsigned width, unsigned prevline, unsigned nextline, \
      unsigned nextline2, TYPE *dst, unsigned dst_stride) \
{ \
   const TYPE *in  = row + x; \
   TYPE *out       = dst + 2 * x; \
   unsigned finish = width - x; \
   int xm1         = (finish < width) ? -1 : 0; \
   int xp1         = (finish > 1) ? 1 : 0; \
   int xp2         = (finish > 2) ? 2 : (int)finish - 1; \
   twoxsai_declare_variables(TYPE, in, prevline, nextline, nextline2, \
         xm1, xp1, xp2); \
   twoxsai_function(twoxsai_result, INTERP, INTERP2); \
   (void)in; \
   (void)out; \
}

TWOXSAI_FULL_FN(twoxsai_full_rgb565, uint16_t,
      twoxsai_interpolate_rgb565, twoxsai_interpolate2_rgb565)
TWOXSAI_FULL_FN(twoxsai_full_xrgb8888, uint32_t,
      twoxsai_interpolate_xrgb8888, twoxsai_interpolate2_xrgb8888)

/* The full rule over pixels x0 up to x1 of a row, in the plain loop's
 * own form: what the vector path runs for stretches of pixels it does
 * not try vectors on, so that detail costs what it did. */
#define TWOXSAI_SEG_FN(NAME, TYPE, INTERP, INTERP2) \
static void NAME(const TYPE *row, unsigned x0, unsigned x1, \
      unsigned width, unsigned prevline, unsigned nextline, \
      unsigned nextline2, TYPE *dst, unsigned dst_stride) \
{ \
   const TYPE *in = row + x0; \
   TYPE *out      = dst + 2 * x0; \
   unsigned finish; \
   for (finish = width - x0; finish > width - x1; finish -= 1) \
   { \
      int xm1 = (finish < width) ? -1 : 0; \
      int xp1 = (finish > 1) ? 1 : 0; \
      int xp2 = (finish > 2) ? 2 : (int)finish - 1; \
      twoxsai_declare_variables(TYPE, in, prevline, nextline, nextline2, \
            xm1, xp1, xp2); \
      twoxsai_function(twoxsai_result, INTERP, INTERP2); \
   } \
}

TWOXSAI_SEG_FN(twoxsai_seg_rgb565, uint16_t,
      twoxsai_interpolate_rgb565, twoxsai_interpolate2_rgb565)
TWOXSAI_SEG_FN(twoxsai_seg_xrgb8888, uint32_t,
      twoxsai_interpolate_xrgb8888, twoxsai_interpolate2_xrgb8888)

/* A worker's rows, the same as the plain loops below but for blocks of
 * one colour: a vector run writes a vector of pixels as copied and the
 * full rule then runs on those of them that are not, and stretches of
 * pixels too near the end of the row for a vector go one by one (SEG). After a
 * vector in which no pixel was copied, the next 1, 3, 7 and up to 63
 * vectors' worth of pixels go one by one, running on into the rows
 * below when a row ends first, so detail (where the vector would be
 * wasted work) costs no more than it did. */
#define TWOXSAI_VEC_ROWS(TYPE, FULL, SEG, RUN, LANES) \
   { \
      unsigned misses_  = 0; \
      unsigned pending_ = 0; /* one-by-one pixels left from the row above */ \
      unsigned row      = 0; \
      for (; height; height--, row++) \
      { \
         unsigned prevline   = (first == 0 && row == 0) ? 0 : src_stride; \
         unsigned rows_below = (height - 1) + below; \
         unsigned nextline   = rows_below >= 1 ? src_stride : 0; \
         unsigned nextline2  = rows_below >= 2 ? 2 * src_stride : nextline; \
         unsigned try_       = pending_; \
         unsigned x          = 0; \
         for (;;) \
         { \
            unsigned m_, j_; \
            unsigned seg_ = try_ < width ? try_ : width; \
            if (x < seg_) \
            { \
               SEG(src, x, seg_, width, prevline, nextline, nextline2, \
                     dst, dst_stride); \
               x = seg_; \
            } \
            if (x >= width) \
               break; \
            if (x + (LANES) >= width) \
            { \
               try_ = width; /* too near the end for a vector */ \
               continue; \
            } \
            m_ = RUN(src, src + nextline, dst, dst_stride, x); \
            misses_ = m_ != ((1u << (LANES)) - 1) ? 0 \
               : misses_ < 6 ? misses_ + 1 : 6; \
            for (j_ = x; m_; m_ >>= 1, j_++) \
               if (m_ & 1) \
                  FULL(src, j_, width, prevline, nextline, nextline2, \
                        dst, dst_stride); \
            x   += (LANES); \
            try_ = x + (LANES) * ((1u << misses_) - 1); \
         } \
         pending_ = try_ > width ? try_ - width : 0; \
         src += src_stride; \
         dst += 2 * dst_stride; \
      } \
   }

static void twoxsai_vec_xrgb8888(const struct filter_data *filt,
      unsigned width, unsigned height,
      int first, unsigned below, const uint32_t *src,
      unsigned src_stride, uint32_t *dst, unsigned dst_stride)
{
   TWOXSAI_VEC_ROWS(uint32_t, twoxsai_full_xrgb8888, twoxsai_seg_xrgb8888,
         filt->run_xrgb8888,
         filt->lanes_xrgb8888)
}

static void twoxsai_vec_rgb565(const struct filter_data *filt,
      unsigned width, unsigned height,
      int first, unsigned below, const uint16_t *src,
      unsigned src_stride, uint16_t *dst, unsigned dst_stride)
{
   TWOXSAI_VEC_ROWS(uint16_t, twoxsai_full_rgb565, twoxsai_seg_rgb565,
         filt->run_rgb565,
         filt->lanes_rgb565)
}
#endif /* TWOXSAI_VECTOR */

#ifndef TWOXSAI_VECTOR
static void twoxsai_generic_xrgb8888(unsigned width, unsigned height,
      int first, unsigned below, uint32_t *src,
      unsigned src_stride, uint32_t *dst, unsigned dst_stride)
{
   unsigned finish;
   unsigned row = 0;

   for (; height; height--, row++)
   {
      uint32_t *in            = (uint32_t*)src;
      uint32_t *out           = (uint32_t*)dst;
      /* Rows above are only unavailable on the frame's very first row;
       * a worker that does not start at the top can always read back
       * into the preceding slice. */
      unsigned prevline   = (first == 0 && row == 0) ? 0 : src_stride;
      /* Rows below this one in the frame: the rest of this slice and
       * the slices after it. Clamped against the frame, not the
       * slice: a worker whose slice ends a row above the bottom,
       * behind a one-row slice, would otherwise read past it. */
      unsigned rows_below = (height - 1) + below;
      unsigned nextline   = rows_below >= 1 ? src_stride : 0;
      unsigned nextline2  = rows_below >= 2 ? 2 * src_stride : nextline;

      for (finish = width; finish; finish -= 1)
      {
         /* finish counts down from width, so it doubles as the distance
          * to the right edge. */
         int xm1 = (finish < width) ? -1 : 0;
         int xp1 = (finish > 1) ? 1 : 0;
         int xp2 = (finish > 2) ? 2 : (int)finish - 1;

         twoxsai_declare_variables(uint32_t, in, prevline, nextline, nextline2,
               xm1, xp1, xp2);

         /*
          * Map of the pixels:           I|E F|J
          *                              G|A B|K
          *                              H|C D|L
          *                              M|N O|P
          */

         twoxsai_function(twoxsai_result, twoxsai_interpolate_xrgb8888,
               twoxsai_interpolate2_xrgb8888);
      }

      src += src_stride;
      dst += 2 * dst_stride;
   }
}

static void twoxsai_generic_rgb565(unsigned width, unsigned height,
      int first, unsigned below, uint16_t *src,
      unsigned src_stride, uint16_t *dst, unsigned dst_stride)
{
   unsigned finish;
   unsigned row = 0;

   for (; height; height--, row++)
   {
      uint16_t *in            = (uint16_t*)src;
      uint16_t *out           = (uint16_t*)dst;
      /* Rows above are only unavailable on the frame's very first row;
       * a worker that does not start at the top can always read back
       * into the preceding slice. */
      unsigned prevline   = (first == 0 && row == 0) ? 0 : src_stride;
      /* Rows below this one in the frame: the rest of this slice and
       * the slices after it. Clamped against the frame, not the
       * slice: a worker whose slice ends a row above the bottom,
       * behind a one-row slice, would otherwise read past it. */
      unsigned rows_below = (height - 1) + below;
      unsigned nextline   = rows_below >= 1 ? src_stride : 0;
      unsigned nextline2  = rows_below >= 2 ? 2 * src_stride : nextline;

      for (finish = width; finish; finish -= 1)
      {
         /* finish counts down from width, so it doubles as the distance
          * to the right edge. */
         int xm1 = (finish < width) ? -1 : 0;
         int xp1 = (finish > 1) ? 1 : 0;
         int xp2 = (finish > 2) ? 2 : (int)finish - 1;

         twoxsai_declare_variables(uint16_t, in, prevline, nextline, nextline2,
               xm1, xp1, xp2);

         /*
          * Map of the pixels:           I|E F|J
          *                              G|A B|K
          *                              H|C D|L
          *                              M|N O|P
          */

         twoxsai_function(twoxsai_result, twoxsai_interpolate_rgb565,
               twoxsai_interpolate2_rgb565);
      }

      src += src_stride;
      dst += 2 * dst_stride;
   }
}

#endif

static void twoxsai_work_cb_rgb565(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr =
      (struct softfilter_thread_data*)thread_data;
   uint16_t *input                    = (uint16_t*)thr->in_data;
   uint16_t *output                   = (uint16_t*)thr->out_data;
   unsigned width                     = thr->width;
   unsigned height                    = thr->height;
#ifdef TWOXSAI_VECTOR
   twoxsai_vec_rgb565((const struct filter_data*)data, width, height,
         thr->first, thr->below, input,
         (unsigned)(thr->in_pitch / SOFTFILTER_BPP_RGB565),
         output,
         (unsigned)(thr->out_pitch / SOFTFILTER_BPP_RGB565));
#else
   twoxsai_generic_rgb565(width, height,
         thr->first, thr->below, input,
         (unsigned)(thr->in_pitch / SOFTFILTER_BPP_RGB565),
         output,
         (unsigned)(thr->out_pitch / SOFTFILTER_BPP_RGB565));
#endif
}

static void twoxsai_work_cb_xrgb8888(void *data, void *thread_data)
{
   struct softfilter_thread_data *thr =
      (struct softfilter_thread_data*)thread_data;
   uint32_t *input                    = (uint32_t*)thr->in_data;
   uint32_t *output                   = (uint32_t*)thr->out_data;
   unsigned width                     = thr->width;
   unsigned height                    = thr->height;
#ifdef TWOXSAI_VECTOR
   twoxsai_vec_xrgb8888((const struct filter_data*)data, width, height,
         thr->first, thr->below, input,
         (unsigned)(thr->in_pitch / SOFTFILTER_BPP_XRGB8888),
         output,
         (unsigned)(thr->out_pitch / SOFTFILTER_BPP_XRGB8888));
#else
   twoxsai_generic_xrgb8888(width, height,
         thr->first, thr->below, input,
         (unsigned)(thr->in_pitch / SOFTFILTER_BPP_XRGB8888),
         output,
         (unsigned)(thr->out_pitch / SOFTFILTER_BPP_XRGB8888));
#endif
}

static void twoxsai_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width,
      unsigned height, size_t input_stride)
{
   unsigned i;
   struct filter_data *filt  = (struct filter_data*)data;

   for (i = 0; i < filt->threads; i++)
   {
      struct softfilter_thread_data *thr =
         (struct softfilter_thread_data*)&filt->workers[i];

      unsigned y_start       = (height * i) / filt->threads;
      unsigned y_end         = (height * (i + 1)) / filt->threads;

      thr->out_data          = (uint8_t*)output + y_start *
         TWOXSAI_SCALE * output_stride;
      thr->in_data           = (const uint8_t*)input + y_start * input_stride;
      thr->out_pitch         = output_stride;
      thr->in_pitch          = input_stride;
      thr->width             = width;
      thr->height            = y_end - y_start;

      /* Workers need to know if they can access pixels
       * outside their given buffer.
       */
      thr->first             = y_start;
      thr->below             = height - y_end;

      if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
         packets[i].work     = twoxsai_work_cb_rgb565;
      else if (filt->in_fmt == SOFTFILTER_FMT_XRGB8888)
         packets[i].work     = twoxsai_work_cb_xrgb8888;
      packets[i].thread_data = thr;
   }
}

static const struct softfilter_implementation twoxsai_generic = {
   twoxsai_generic_input_fmts,
   twoxsai_generic_output_fmts,

   twoxsai_generic_create,
   twoxsai_generic_destroy,

   twoxsai_generic_threads,
   twoxsai_generic_output,
   twoxsai_generic_packets,
   SOFTFILTER_API_VERSION,
   "2xSaI",
   "2xsai",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   return &twoxsai_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
