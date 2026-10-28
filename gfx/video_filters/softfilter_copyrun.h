/*  RetroArch - A frontend for libretro.
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

/* Copy runs for the 2x scalers of the 2xSaI family, whose output for a
 * pixel whose 2x2 block (it, the pixel after it and the two below them)
 * is one colour is that colour four times over; for 2xSaI and Super
 * 2xSaI, the pixel after it and the one below it already settle that
 * (their rules then only produce those pixels and blends of them with
 * each other), so their runs (softfilter_copy3_*) leave the fourth out,
 * while SuperEagle's (softfilter_copy2x2_*) test all four. A run takes
 * a vector of pixels, writes each of them twice across on both output
 * rows as if its block were one colour, and returns a bit per pixel
 * (lowest first) for those whose block is not, which the caller then
 * runs its full rule on; SOFTFILTER_COPYRUN_ROWS is the row loop around
 * that. */

#ifndef SOFTFILTER_COPYRUN_H__
#define SOFTFILTER_COPYRUN_H__

#include <stddef.h>
#include <stdint.h>

#include "softfilter.h"
#include "softfilter_simd.h"

#if defined(SOFTFILTER_HAVE_SSE2) || defined(SOFTFILTER_HAVE_NEON)
#define SOFTFILTER_HAVE_COPYRUN 1
#endif

/* r0 is the pixels' row and r1 the one below it (the same row on the
 * frame's last); dst the first of the two output rows. The vector must
 * end before the row's last pixel. */
typedef unsigned (*softfilter_copyrun_rgb565_t)(const uint16_t *r0,
      const uint16_t *r1, uint16_t *dst, size_t dst_stride, unsigned x);
typedef unsigned (*softfilter_copyrun_xrgb8888_t)(const uint32_t *r0,
      const uint32_t *r1, uint32_t *dst, size_t dst_stride, unsigned x);

/* A filter's runs and their widths in pixels, or NULL runs */
struct softfilter_copyrun
{
   softfilter_copyrun_rgb565_t rgb565;
   softfilter_copyrun_xrgb8888_t xrgb8888;
   unsigned lanes_rgb565;
   unsigned lanes_xrgb8888;
};

#ifdef SOFTFILTER_HAVE_SSE2
#define SOFTFILTER_COPYRUN_LD(p) _mm_loadu_si128((const __m128i*)(p))

static unsigned softfilter_copy2x2_rgb565_sse2(const uint16_t *r0,
      const uint16_t *r1, uint16_t *dst, size_t dst_stride, unsigned x)
{
   __m128i a  = SOFTFILTER_COPYRUN_LD(r0 + x);
   __m128i ok = _mm_and_si128(
         _mm_cmpeq_epi16(a, SOFTFILTER_COPYRUN_LD(r0 + x + 1)),
         _mm_and_si128(_mm_cmpeq_epi16(a, SOFTFILTER_COPYRUN_LD(r1 + x)),
            _mm_cmpeq_epi16(a, SOFTFILTER_COPYRUN_LD(r1 + x + 1))));
   __m128i lo = _mm_unpacklo_epi16(a, a);
   __m128i hi = _mm_unpackhi_epi16(a, a);
   _mm_storeu_si128((__m128i*)(dst + 2 * x),     lo);
   _mm_storeu_si128((__m128i*)(dst + 2 * x + 8), hi);
   _mm_storeu_si128((__m128i*)(dst + dst_stride + 2 * x),     lo);
   _mm_storeu_si128((__m128i*)(dst + dst_stride + 2 * x + 8), hi);
   return ~(unsigned)_mm_movemask_epi8(
         _mm_packs_epi16(ok, _mm_setzero_si128())) & 0xFFu;
}

static unsigned softfilter_copy2x2_xrgb8888_sse2(const uint32_t *r0,
      const uint32_t *r1, uint32_t *dst, size_t dst_stride, unsigned x)
{
   __m128i a  = SOFTFILTER_COPYRUN_LD(r0 + x);
   __m128i ok = _mm_and_si128(
         _mm_cmpeq_epi32(a, SOFTFILTER_COPYRUN_LD(r0 + x + 1)),
         _mm_and_si128(_mm_cmpeq_epi32(a, SOFTFILTER_COPYRUN_LD(r1 + x)),
            _mm_cmpeq_epi32(a, SOFTFILTER_COPYRUN_LD(r1 + x + 1))));
   __m128i lo = _mm_unpacklo_epi32(a, a);
   __m128i hi = _mm_unpackhi_epi32(a, a);
   _mm_storeu_si128((__m128i*)(dst + 2 * x),     lo);
   _mm_storeu_si128((__m128i*)(dst + 2 * x + 4), hi);
   _mm_storeu_si128((__m128i*)(dst + dst_stride + 2 * x),     lo);
   _mm_storeu_si128((__m128i*)(dst + dst_stride + 2 * x + 4), hi);
   return ~(unsigned)_mm_movemask_ps(_mm_castsi128_ps(ok)) & 0xFu;
}
static unsigned softfilter_copy3_rgb565_sse2(const uint16_t *r0,
      const uint16_t *r1, uint16_t *dst, size_t dst_stride, unsigned x)
{
   __m128i a  = SOFTFILTER_COPYRUN_LD(r0 + x);
   __m128i ok = _mm_and_si128(
         _mm_cmpeq_epi16(a, SOFTFILTER_COPYRUN_LD(r0 + x + 1)),
         _mm_cmpeq_epi16(a, SOFTFILTER_COPYRUN_LD(r1 + x)));
   __m128i lo = _mm_unpacklo_epi16(a, a);
   __m128i hi = _mm_unpackhi_epi16(a, a);
   _mm_storeu_si128((__m128i*)(dst + 2 * x),     lo);
   _mm_storeu_si128((__m128i*)(dst + 2 * x + 8), hi);
   _mm_storeu_si128((__m128i*)(dst + dst_stride + 2 * x),     lo);
   _mm_storeu_si128((__m128i*)(dst + dst_stride + 2 * x + 8), hi);
   return ~(unsigned)_mm_movemask_epi8(
         _mm_packs_epi16(ok, _mm_setzero_si128())) & 0xFFu;
}

static unsigned softfilter_copy3_xrgb8888_sse2(const uint32_t *r0,
      const uint32_t *r1, uint32_t *dst, size_t dst_stride, unsigned x)
{
   __m128i a  = SOFTFILTER_COPYRUN_LD(r0 + x);
   __m128i ok = _mm_and_si128(
         _mm_cmpeq_epi32(a, SOFTFILTER_COPYRUN_LD(r0 + x + 1)),
         _mm_cmpeq_epi32(a, SOFTFILTER_COPYRUN_LD(r1 + x)));
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
#define SOFTFILTER_COPYRUN_LD_256(p) _mm256_loadu_si256((const __m256i*)(p))

/* unpacklo/unpackhi double each 128-bit half; the permutes put the
 * doubled pixels back in order. */
#define SOFTFILTER_COPYRUN_STORE_256(dst, x, lo, hi, n) { \
      __m256i a_ = _mm256_permute2x128_si256(lo, hi, 0x20); \
      __m256i b_ = _mm256_permute2x128_si256(lo, hi, 0x31); \
      _mm256_storeu_si256((__m256i*)((dst) + 2 * (x)),     a_); \
      _mm256_storeu_si256((__m256i*)((dst) + 2 * (x) + n), b_); \
      _mm256_storeu_si256((__m256i*)((dst) + dst_stride + 2 * (x)),     a_); \
      _mm256_storeu_si256((__m256i*)((dst) + dst_stride + 2 * (x) + n), b_); \
}

SOFTFILTER_TARGET_AVX2
static unsigned softfilter_copy2x2_rgb565_avx2(const uint16_t *r0,
      const uint16_t *r1, uint16_t *dst, size_t dst_stride, unsigned x)
{
   __m256i a  = SOFTFILTER_COPYRUN_LD_256(r0 + x);
   __m256i ok = _mm256_and_si256(
         _mm256_cmpeq_epi16(a, SOFTFILTER_COPYRUN_LD_256(r0 + x + 1)),
         _mm256_and_si256(_mm256_cmpeq_epi16(a, SOFTFILTER_COPYRUN_LD_256(r1 + x)),
            _mm256_cmpeq_epi16(a, SOFTFILTER_COPYRUN_LD_256(r1 + x + 1))));
   __m256i lo = _mm256_unpacklo_epi16(a, a);
   __m256i hi = _mm256_unpackhi_epi16(a, a);
   unsigned m;
   SOFTFILTER_COPYRUN_STORE_256(dst, x, lo, hi, 16)
   m = ~(unsigned)_mm_movemask_epi8(_mm_packs_epi16(
            _mm256_castsi256_si128(ok), _mm256_extracti128_si256(ok, 1)))
      & 0xFFFFu;
   SOFTFILTER_ZEROUPPER();
   return m;
}

SOFTFILTER_TARGET_AVX2
static unsigned softfilter_copy2x2_xrgb8888_avx2(const uint32_t *r0,
      const uint32_t *r1, uint32_t *dst, size_t dst_stride, unsigned x)
{
   __m256i a  = SOFTFILTER_COPYRUN_LD_256(r0 + x);
   __m256i ok = _mm256_and_si256(
         _mm256_cmpeq_epi32(a, SOFTFILTER_COPYRUN_LD_256(r0 + x + 1)),
         _mm256_and_si256(_mm256_cmpeq_epi32(a, SOFTFILTER_COPYRUN_LD_256(r1 + x)),
            _mm256_cmpeq_epi32(a, SOFTFILTER_COPYRUN_LD_256(r1 + x + 1))));
   __m256i lo = _mm256_unpacklo_epi32(a, a);
   __m256i hi = _mm256_unpackhi_epi32(a, a);
   unsigned m;
   SOFTFILTER_COPYRUN_STORE_256(dst, x, lo, hi, 8)
   m = ~(unsigned)_mm256_movemask_ps(_mm256_castsi256_ps(ok)) & 0xFFu;
   SOFTFILTER_ZEROUPPER();
   return m;
}
SOFTFILTER_TARGET_AVX2
static unsigned softfilter_copy3_rgb565_avx2(const uint16_t *r0,
      const uint16_t *r1, uint16_t *dst, size_t dst_stride, unsigned x)
{
   __m256i a  = SOFTFILTER_COPYRUN_LD_256(r0 + x);
   __m256i ok = _mm256_and_si256(
         _mm256_cmpeq_epi16(a, SOFTFILTER_COPYRUN_LD_256(r0 + x + 1)),
         _mm256_cmpeq_epi16(a, SOFTFILTER_COPYRUN_LD_256(r1 + x)));
   __m256i lo = _mm256_unpacklo_epi16(a, a);
   __m256i hi = _mm256_unpackhi_epi16(a, a);
   unsigned m;
   SOFTFILTER_COPYRUN_STORE_256(dst, x, lo, hi, 16)
   m = ~(unsigned)_mm_movemask_epi8(_mm_packs_epi16(
            _mm256_castsi256_si128(ok), _mm256_extracti128_si256(ok, 1)))
      & 0xFFFFu;
   SOFTFILTER_ZEROUPPER();
   return m;
}

SOFTFILTER_TARGET_AVX2
static unsigned softfilter_copy3_xrgb8888_avx2(const uint32_t *r0,
      const uint32_t *r1, uint32_t *dst, size_t dst_stride, unsigned x)
{
   __m256i a  = SOFTFILTER_COPYRUN_LD_256(r0 + x);
   __m256i ok = _mm256_and_si256(
         _mm256_cmpeq_epi32(a, SOFTFILTER_COPYRUN_LD_256(r0 + x + 1)),
         _mm256_cmpeq_epi32(a, SOFTFILTER_COPYRUN_LD_256(r1 + x)));
   __m256i lo = _mm256_unpacklo_epi32(a, a);
   __m256i hi = _mm256_unpackhi_epi32(a, a);
   unsigned m;
   SOFTFILTER_COPYRUN_STORE_256(dst, x, lo, hi, 8)
   m = ~(unsigned)_mm256_movemask_ps(_mm256_castsi256_ps(ok)) & 0xFFu;
   SOFTFILTER_ZEROUPPER();
   return m;
}
#endif /* SOFTFILTER_HAVE_AVX2 */

#ifdef SOFTFILTER_HAVE_NEON
/* A bit per byte of an all-ones-or-zero mask of 8 bytes, set where zero */
static INLINE unsigned softfilter_copyrun_zero_bytes_neon(uint8x8_t m)
{
   uint64_t v = vget_lane_u64(vreinterpret_u64_u8(m), 0);
   unsigned bits = 0, j;
   for (j = 0; j < 8; j++)
      if (!((v >> (8 * j)) & 0xFF))
         bits |= 1u << j;
   return bits;
}

static unsigned softfilter_copy2x2_rgb565_neon(const uint16_t *r0,
      const uint16_t *r1, uint16_t *dst, size_t dst_stride, unsigned x)
{
   uint16x8_t a  = vld1q_u16(r0 + x);
   uint16x8_t ok = vandq_u16(vceqq_u16(a, vld1q_u16(r0 + x + 1)),
         vandq_u16(vceqq_u16(a, vld1q_u16(r1 + x)),
            vceqq_u16(a, vld1q_u16(r1 + x + 1))));
   uint16x8x2_t d = vzipq_u16(a, a);
   vst1q_u16(dst + 2 * x,     d.val[0]);
   vst1q_u16(dst + 2 * x + 8, d.val[1]);
   vst1q_u16(dst + dst_stride + 2 * x,     d.val[0]);
   vst1q_u16(dst + dst_stride + 2 * x + 8, d.val[1]);
   return softfilter_copyrun_zero_bytes_neon(vmovn_u16(ok));
}

static unsigned softfilter_copy2x2_xrgb8888_neon(const uint32_t *r0,
      const uint32_t *r1, uint32_t *dst, size_t dst_stride, unsigned x)
{
   uint32x4_t a  = vld1q_u32(r0 + x);
   uint32x4_t ok = vandq_u32(vceqq_u32(a, vld1q_u32(r0 + x + 1)),
         vandq_u32(vceqq_u32(a, vld1q_u32(r1 + x)),
            vceqq_u32(a, vld1q_u32(r1 + x + 1))));
   uint32x4x2_t d = vzipq_u32(a, a);
   vst1q_u32(dst + 2 * x,     d.val[0]);
   vst1q_u32(dst + 2 * x + 4, d.val[1]);
   vst1q_u32(dst + dst_stride + 2 * x,     d.val[0]);
   vst1q_u32(dst + dst_stride + 2 * x + 4, d.val[1]);
   /* four 32-bit lanes narrowed twice to bytes; the upper four are 0 */
   return softfilter_copyrun_zero_bytes_neon(vmovn_u16(vcombine_u16(
               vmovn_u32(ok), vdup_n_u16(0xFFFF)))) & 0xFu;
}
static unsigned softfilter_copy3_rgb565_neon(const uint16_t *r0,
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
   return softfilter_copyrun_zero_bytes_neon(vmovn_u16(ok));
}

static unsigned softfilter_copy3_xrgb8888_neon(const uint32_t *r0,
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
   return softfilter_copyrun_zero_bytes_neon(vmovn_u16(vcombine_u16(
               vmovn_u32(ok), vdup_n_u16(0xFFFF)))) & 0xFu;
}
#endif /* SOFTFILTER_HAVE_NEON */

/* The 2x2-block runs for this machine (AVX2 where the SIMD mask has it),
 * or NULL runs where none are built. */
static INLINE void softfilter_copy2x2_pick(struct softfilter_copyrun *cr,
      softfilter_simd_mask_t simd)
{
   (void)simd;
   cr->rgb565         = NULL;
   cr->xrgb8888       = NULL;
   cr->lanes_rgb565   = 0;
   cr->lanes_xrgb8888 = 0;
#if defined(SOFTFILTER_HAVE_SSE2)
   cr->rgb565         = softfilter_copy2x2_rgb565_sse2;
   cr->xrgb8888       = softfilter_copy2x2_xrgb8888_sse2;
   cr->lanes_rgb565   = 8;
   cr->lanes_xrgb8888 = 4;
#ifdef SOFTFILTER_HAVE_AVX2
   if (simd & SOFTFILTER_SIMD_AVX2)
   {
      cr->rgb565         = softfilter_copy2x2_rgb565_avx2;
      cr->xrgb8888       = softfilter_copy2x2_xrgb8888_avx2;
      cr->lanes_rgb565   = 16;
      cr->lanes_xrgb8888 = 8;
   }
#endif
#elif defined(SOFTFILTER_HAVE_NEON)
   cr->rgb565         = softfilter_copy2x2_rgb565_neon;
   cr->xrgb8888       = softfilter_copy2x2_xrgb8888_neon;
   cr->lanes_rgb565   = 8;
   cr->lanes_xrgb8888 = 4;
#endif
}

/* The three-pixel runs for this machine (AVX2 where the SIMD mask has it),
 * or NULL runs where none are built. */
static INLINE void softfilter_copy3_pick(struct softfilter_copyrun *cr,
      softfilter_simd_mask_t simd)
{
   (void)simd;
   cr->rgb565         = NULL;
   cr->xrgb8888       = NULL;
   cr->lanes_rgb565   = 0;
   cr->lanes_xrgb8888 = 0;
#if defined(SOFTFILTER_HAVE_SSE2)
   cr->rgb565         = softfilter_copy3_rgb565_sse2;
   cr->xrgb8888       = softfilter_copy3_xrgb8888_sse2;
   cr->lanes_rgb565   = 8;
   cr->lanes_xrgb8888 = 4;
#ifdef SOFTFILTER_HAVE_AVX2
   if (simd & SOFTFILTER_SIMD_AVX2)
   {
      cr->rgb565         = softfilter_copy3_rgb565_avx2;
      cr->xrgb8888       = softfilter_copy3_xrgb8888_avx2;
      cr->lanes_rgb565   = 16;
      cr->lanes_xrgb8888 = 8;
   }
#endif
#elif defined(SOFTFILTER_HAVE_NEON)
   cr->rgb565         = softfilter_copy3_rgb565_neon;
   cr->xrgb8888       = softfilter_copy3_xrgb8888_neon;
   cr->lanes_rgb565   = 8;
   cr->lanes_xrgb8888 = 4;
#endif
}

/* A worker's rows for a 2xSaI-family filter with a copy run: the run
 * writes a vector of pixels as copied and the full rule (FULL, one
 * pixel) then runs on those of them that are not, and stretches of
 * pixels too near the end of the row for a vector go through SEG, the
 * filter's plain loop over a range of one row. After a vector in which
 * no pixel was copied, the next 1, 3, 7 and up to 63 vectors' worth of
 * pixels go one by one, running on into the rows below when a row ends
 * first, so detail (where the vector would be wasted work) costs no
 * more than it did. Rows are clamped against the frame as in the plain
 * loops: width, height, first, below, src, src_stride, dst and
 * dst_stride are the worker's. FULL and SEG take
 * (row, x, [x_end,] width, prevline, nextline, nextline2, dst,
 * dst_stride). */
#define SOFTFILTER_COPYRUN_ROWS(TYPE, FULL, SEG, RUN, LANES) \
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

#endif
