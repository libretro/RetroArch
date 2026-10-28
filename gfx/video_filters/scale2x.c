/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2018 - Daniel De Matteis
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

/* Compile: gcc -o scale2x.so -shared scale2x.c -std=c99 -O3 -Wall -pedantic -fPIC */

#include "softfilter.h"
#include <stdlib.h>
#include <string.h>

/* Vector runs (see softfilter_simd.h), which give the per-pixel code's
 * output to the bit. */
#include "softfilter_simd.h"

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation scale2x_get_implementation
#define softfilter_thread_data scale2x_softfilter_thread_data
#define filter_data scale2x_filter_data
#endif

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
   int last;
};

/* A vector run over one row: output pixels for source pixels x up to
 * x_end (exclusive), whole vectors only; returns the first pixel it
 * left. n, p and s are the rows above, of and below the pixels, out0
 * and out1 the two output rows. x must be at least 1 and the vectors
 * must end before the last pixel, so that both side neighbours are
 * inside the row. */
typedef unsigned (*scale2x_run_xrgb8888_t)(const uint32_t *n,
      const uint32_t *p, const uint32_t *s, uint32_t *out0, uint32_t *out1,
      unsigned x, unsigned x_end);
typedef unsigned (*scale2x_run_rgb565_t)(const uint16_t *n,
      const uint16_t *p, const uint16_t *s, uint16_t *out0, uint16_t *out1,
      unsigned x, unsigned x_end);

struct filter_data
{
   unsigned threads;
   struct softfilter_thread_data *workers;
   unsigned in_fmt;
   /* Rows in the whole frame, against which the rows above and below
    * are clamped whichever band a worker has. */
   unsigned frame_height;
   scale2x_run_xrgb8888_t run_xrgb8888;
   scale2x_run_rgb565_t run_rgb565;
};

static unsigned scale2x_generic_input_fmts(void)
{
   return SOFTFILTER_FMT_XRGB8888 | SOFTFILTER_FMT_RGB565;
}

static unsigned scale2x_generic_output_fmts(unsigned input_fmts)
{
   return input_fmts;
}

static unsigned scale2x_generic_threads(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

/* Scale2x for one pixel C with A above, B left, D right and E below:
 * where A != E and B != D, each output corner takes A or E when it
 * equals the side neighbour of that corner, and C otherwise; elsewhere
 * all four are C. The runs do that for a vector of pixels, branch-free. */

#ifdef SOFTFILTER_HAVE_SSE2
#define SCALE2X_LD(ptr) _mm_loadu_si128((const __m128i*)(ptr))
#define SCALE2X_SEL(m, a, b) _mm_or_si128(_mm_and_si128(m, a), _mm_andnot_si128(m, b))
#define SCALE2X_CORNERS_SSE2(EQ, A, B, C, D, E, O0, O1, O2, O3) \
   { \
      __m128i ones_ = _mm_cmpeq_epi32(C, C); \
      __m128i m_    = _mm_and_si128(_mm_xor_si128(EQ(A, E), ones_), \
            _mm_xor_si128(EQ(B, D), ones_)); \
      O0 = SCALE2X_SEL(_mm_and_si128(m_, EQ(A, B)), A, C); \
      O1 = SCALE2X_SEL(_mm_and_si128(m_, EQ(A, D)), A, C); \
      O2 = SCALE2X_SEL(_mm_and_si128(m_, EQ(E, B)), E, C); \
      O3 = SCALE2X_SEL(_mm_and_si128(m_, EQ(E, D)), E, C); \
   }

static unsigned scale2x_run_xrgb8888_sse2(const uint32_t *n,
      const uint32_t *p, const uint32_t *s, uint32_t *out0, uint32_t *out1,
      unsigned x, unsigned x_end)
{
   for (; x + 4 <= x_end; x += 4)
   {
      __m128i A = SCALE2X_LD(n + x), B = SCALE2X_LD(p + x - 1);
      __m128i C = SCALE2X_LD(p + x), D = SCALE2X_LD(p + x + 1);
      __m128i E = SCALE2X_LD(s + x);
      __m128i o0, o1, o2, o3;
      SCALE2X_CORNERS_SSE2(_mm_cmpeq_epi32, A, B, C, D, E, o0, o1, o2, o3)
      _mm_storeu_si128((__m128i*)(out0 + 2 * x),     _mm_unpacklo_epi32(o0, o1));
      _mm_storeu_si128((__m128i*)(out0 + 2 * x + 4), _mm_unpackhi_epi32(o0, o1));
      _mm_storeu_si128((__m128i*)(out1 + 2 * x),     _mm_unpacklo_epi32(o2, o3));
      _mm_storeu_si128((__m128i*)(out1 + 2 * x + 4), _mm_unpackhi_epi32(o2, o3));
   }
   return x;
}

static unsigned scale2x_run_rgb565_sse2(const uint16_t *n,
      const uint16_t *p, const uint16_t *s, uint16_t *out0, uint16_t *out1,
      unsigned x, unsigned x_end)
{
   for (; x + 8 <= x_end; x += 8)
   {
      __m128i A = SCALE2X_LD(n + x), B = SCALE2X_LD(p + x - 1);
      __m128i C = SCALE2X_LD(p + x), D = SCALE2X_LD(p + x + 1);
      __m128i E = SCALE2X_LD(s + x);
      __m128i o0, o1, o2, o3;
      SCALE2X_CORNERS_SSE2(_mm_cmpeq_epi16, A, B, C, D, E, o0, o1, o2, o3)
      _mm_storeu_si128((__m128i*)(out0 + 2 * x),     _mm_unpacklo_epi16(o0, o1));
      _mm_storeu_si128((__m128i*)(out0 + 2 * x + 8), _mm_unpackhi_epi16(o0, o1));
      _mm_storeu_si128((__m128i*)(out1 + 2 * x),     _mm_unpacklo_epi16(o2, o3));
      _mm_storeu_si128((__m128i*)(out1 + 2 * x + 8), _mm_unpackhi_epi16(o2, o3));
   }
   return x;
}
#endif /* SOFTFILTER_HAVE_SSE2 */

#ifdef SOFTFILTER_HAVE_AVX2
#define SCALE2X_LD_256(ptr) _mm256_loadu_si256((const __m256i*)(ptr))
#define SCALE2X_SEL_256(m, a, b) \
      _mm256_or_si256(_mm256_and_si256(m, a), _mm256_andnot_si256(m, b))
#define SCALE2X_CORNERS_AVX2(EQ, A, B, C, D, E, O0, O1, O2, O3) \
   { \
      __m256i ones_ = _mm256_cmpeq_epi32(C, C); \
      __m256i m_    = _mm256_and_si256(_mm256_xor_si256(EQ(A, E), ones_), \
            _mm256_xor_si256(EQ(B, D), ones_)); \
      O0 = SCALE2X_SEL_256(_mm256_and_si256(m_, EQ(A, B)), A, C); \
      O1 = SCALE2X_SEL_256(_mm256_and_si256(m_, EQ(A, D)), A, C); \
      O2 = SCALE2X_SEL_256(_mm256_and_si256(m_, EQ(E, B)), E, C); \
      O3 = SCALE2X_SEL_256(_mm256_and_si256(m_, EQ(E, D)), E, C); \
   }
/* unpacklo/unpackhi interleave within each 128-bit half; the permutes
 * put the interleaved pixels back in order. */
#define SCALE2X_STORE_PAIRS_256(dst, lo, hi, cnt) \
   { \
      _mm256_storeu_si256((__m256i*)(dst), \
            _mm256_permute2x128_si256(lo, hi, 0x20)); \
      _mm256_storeu_si256((__m256i*)((dst) + (cnt)), \
            _mm256_permute2x128_si256(lo, hi, 0x31)); \
   }

SOFTFILTER_TARGET_AVX2
static unsigned scale2x_run_xrgb8888_avx2(const uint32_t *n,
      const uint32_t *p, const uint32_t *s, uint32_t *out0, uint32_t *out1,
      unsigned x, unsigned x_end)
{
   for (; x + 8 <= x_end; x += 8)
   {
      __m256i A = SCALE2X_LD_256(n + x), B = SCALE2X_LD_256(p + x - 1);
      __m256i C = SCALE2X_LD_256(p + x), D = SCALE2X_LD_256(p + x + 1);
      __m256i E = SCALE2X_LD_256(s + x);
      __m256i o0, o1, o2, o3;
      SCALE2X_CORNERS_AVX2(_mm256_cmpeq_epi32, A, B, C, D, E, o0, o1, o2, o3)
      SCALE2X_STORE_PAIRS_256(out0 + 2 * x, _mm256_unpacklo_epi32(o0, o1),
            _mm256_unpackhi_epi32(o0, o1), 8)
      SCALE2X_STORE_PAIRS_256(out1 + 2 * x, _mm256_unpacklo_epi32(o2, o3),
            _mm256_unpackhi_epi32(o2, o3), 8)
   }
   SOFTFILTER_ZEROUPPER();
   return x;
}

SOFTFILTER_TARGET_AVX2
static unsigned scale2x_run_rgb565_avx2(const uint16_t *n,
      const uint16_t *p, const uint16_t *s, uint16_t *out0, uint16_t *out1,
      unsigned x, unsigned x_end)
{
   for (; x + 16 <= x_end; x += 16)
   {
      __m256i A = SCALE2X_LD_256(n + x), B = SCALE2X_LD_256(p + x - 1);
      __m256i C = SCALE2X_LD_256(p + x), D = SCALE2X_LD_256(p + x + 1);
      __m256i E = SCALE2X_LD_256(s + x);
      __m256i o0, o1, o2, o3;
      SCALE2X_CORNERS_AVX2(_mm256_cmpeq_epi16, A, B, C, D, E, o0, o1, o2, o3)
      SCALE2X_STORE_PAIRS_256(out0 + 2 * x, _mm256_unpacklo_epi16(o0, o1),
            _mm256_unpackhi_epi16(o0, o1), 16)
      SCALE2X_STORE_PAIRS_256(out1 + 2 * x, _mm256_unpacklo_epi16(o2, o3),
            _mm256_unpackhi_epi16(o2, o3), 16)
   }
   SOFTFILTER_ZEROUPPER();
   return x;
}
#endif /* SOFTFILTER_HAVE_AVX2 */

#ifdef SOFTFILTER_HAVE_NEON
#define SCALE2X_CORNERS_NEON(EQ, AND, BIC, ORR, BSL, A, B, C, D, E, O0, O1, O2, O3) \
   { \
      O0 = BSL(BIC(BIC(EQ(A, B), EQ(A, E)), EQ(B, D)), A, C); \
      O1 = BSL(BIC(BIC(EQ(A, D), EQ(A, E)), EQ(B, D)), A, C); \
      O2 = BSL(BIC(BIC(EQ(E, B), EQ(A, E)), EQ(B, D)), E, C); \
      O3 = BSL(BIC(BIC(EQ(E, D), EQ(A, E)), EQ(B, D)), E, C); \
   }

static unsigned scale2x_run_xrgb8888_neon(const uint32_t *n,
      const uint32_t *p, const uint32_t *s, uint32_t *out0, uint32_t *out1,
      unsigned x, unsigned x_end)
{
   for (; x + 4 <= x_end; x += 4)
   {
      uint32x4_t A = vld1q_u32(n + x), B = vld1q_u32(p + x - 1);
      uint32x4_t C = vld1q_u32(p + x), D = vld1q_u32(p + x + 1);
      uint32x4_t E = vld1q_u32(s + x);
      uint32x4_t o0, o1, o2, o3;
      uint32x4x2_t r0, r1;
      SCALE2X_CORNERS_NEON(vceqq_u32, vandq_u32, vbicq_u32, vorrq_u32, vbslq_u32,
            A, B, C, D, E, o0, o1, o2, o3)
      r0 = vzipq_u32(o0, o1);
      r1 = vzipq_u32(o2, o3);
      vst1q_u32(out0 + 2 * x,     r0.val[0]);
      vst1q_u32(out0 + 2 * x + 4, r0.val[1]);
      vst1q_u32(out1 + 2 * x,     r1.val[0]);
      vst1q_u32(out1 + 2 * x + 4, r1.val[1]);
   }
   return x;
}

static unsigned scale2x_run_rgb565_neon(const uint16_t *n,
      const uint16_t *p, const uint16_t *s, uint16_t *out0, uint16_t *out1,
      unsigned x, unsigned x_end)
{
   for (; x + 8 <= x_end; x += 8)
   {
      uint16x8_t A = vld1q_u16(n + x), B = vld1q_u16(p + x - 1);
      uint16x8_t C = vld1q_u16(p + x), D = vld1q_u16(p + x + 1);
      uint16x8_t E = vld1q_u16(s + x);
      uint16x8_t o0, o1, o2, o3;
      uint16x8x2_t r0, r1;
      SCALE2X_CORNERS_NEON(vceqq_u16, vandq_u16, vbicq_u16, vorrq_u16, vbslq_u16,
            A, B, C, D, E, o0, o1, o2, o3)
      r0 = vzipq_u16(o0, o1);
      r1 = vzipq_u16(o2, o3);
      vst1q_u16(out0 + 2 * x,     r0.val[0]);
      vst1q_u16(out0 + 2 * x + 8, r0.val[1]);
      vst1q_u16(out1 + 2 * x,     r1.val[0]);
      vst1q_u16(out1 + 2 * x + 8, r1.val[1]);
   }
   return x;
}
#endif /* SOFTFILTER_HAVE_NEON */

static void *scale2x_generic_create(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata)
{
   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   if (!filt)
      return NULL;
   if (!(filt->workers = (struct softfilter_thread_data*)calloc(threads ? threads : 1, sizeof(struct softfilter_thread_data))))
   {
      free(filt);
      return NULL;
   }
   /* Each output row pair needs only the source row and the ones
    * either side of it, so the rows are split between the workers. */
   filt->threads = threads ? threads : 1;
   filt->in_fmt  = in_fmt;
   (void)simd;
#if defined(SOFTFILTER_HAVE_SSE2)
   filt->run_xrgb8888 = scale2x_run_xrgb8888_sse2;
   filt->run_rgb565   = scale2x_run_rgb565_sse2;
#ifdef SOFTFILTER_HAVE_AVX2
   if (simd & SOFTFILTER_SIMD_AVX2)
   {
      filt->run_xrgb8888 = scale2x_run_xrgb8888_avx2;
      filt->run_rgb565   = scale2x_run_rgb565_avx2;
   }
#endif
#elif defined(SOFTFILTER_HAVE_NEON)
   filt->run_xrgb8888 = scale2x_run_xrgb8888_neon;
   filt->run_rgb565   = scale2x_run_rgb565_neon;
#endif
   return filt;
}

static void scale2x_generic_output(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   *out_width  = width << 1;
   *out_height = height << 1;
}

static void scale2x_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   if (!filt)
      return;
   free(filt->workers);
   free(filt);
}

/* A worker's rows. The rows above and below are clamped against the
 * frame, the side neighbours against the row; the first and last pixel
 * of a row and whatever a vector run leaves go one by one. */
#define SCALE2X_ROWS(TYPE, RUN) \
   { \
      unsigned y, x; \
      unsigned width        = thr->width; \
      unsigned frame_height = filt->frame_height; \
      for (y = 0; y < thr->height; y++) \
      { \
         unsigned fy    = (unsigned)thr->first + y; \
         const TYPE *pr = input + (size_t)y * in_stride; \
         const TYPE *nr = fy > 0 ? pr - in_stride : pr; \
         const TYPE *sr = fy + 1 < frame_height ? pr + in_stride : pr; \
         TYPE *o0       = output + (size_t)y * 2 * out_stride; \
         TYPE *o1       = o0 + out_stride; \
         for (x = 0; x < width; ) \
         { \
            TYPE A = nr[x]; \
            TYPE B = x > 0 ? pr[x - 1] : pr[x]; \
            TYPE C = pr[x]; \
            TYPE D = x + 1 < width ? pr[x + 1] : pr[x]; \
            TYPE E = sr[x]; \
            if (A != E && B != D) \
            { \
               o0[2 * x]     = (A == B ? A : C); \
               o0[2 * x + 1] = (A == D ? A : C); \
               o1[2 * x]     = (E == B ? E : C); \
               o1[2 * x + 1] = (E == D ? E : C); \
            } \
            else \
            { \
               o0[2 * x]     = C; \
               o0[2 * x + 1] = C; \
               o1[2 * x]     = C; \
               o1[2 * x + 1] = C; \
            } \
            x++; \
            if (RUN && x == 1 && width > 2) \
               x = RUN(nr, pr, sr, o0, o1, 1, width - 1); \
         } \
      } \
   }

static void scale2x_work_cb_xrgb8888(void *data, void *thread_data)
{
   struct filter_data *filt           = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   size_t in_stride                   = thr->in_pitch  >> 2;
   size_t out_stride                  = thr->out_pitch >> 2;
   const uint32_t *input              = (const uint32_t*)thr->in_data;
   uint32_t *output                   = (uint32_t*)thr->out_data;
   SCALE2X_ROWS(uint32_t, filt->run_xrgb8888)
}

static void scale2x_work_cb_rgb565(void *data, void *thread_data)
{
   struct filter_data *filt           = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   size_t in_stride                   = thr->in_pitch  >> 1;
   size_t out_stride                  = thr->out_pitch >> 1;
   const uint16_t *input              = (const uint16_t*)thr->in_data;
   uint16_t *output                   = (uint16_t*)thr->out_data;
   SCALE2X_ROWS(uint16_t, filt->run_rgb565)
}

static void scale2x_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride)
{
   unsigned i;
   struct filter_data *filt = (struct filter_data*)data;

   filt->frame_height = height;

   for (i = 0; i < filt->threads; i++)
   {
      struct softfilter_thread_data *thr = &filt->workers[i];
      unsigned y_start = (height * i) / filt->threads;
      unsigned y_end   = (height * (i + 1)) / filt->threads;

      thr->out_data  = (uint8_t*)output + (size_t)y_start * 2 * output_stride;
      thr->in_data   = (const uint8_t*)input + (size_t)y_start * input_stride;
      thr->out_pitch = output_stride;
      thr->in_pitch  = input_stride;
      thr->width     = width;
      thr->height    = y_end - y_start;
      thr->first     = (int)y_start;
      thr->last      = y_end == height;

      if (filt->in_fmt == SOFTFILTER_FMT_XRGB8888)
         packets[i].work = scale2x_work_cb_xrgb8888;
      else if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
         packets[i].work = scale2x_work_cb_rgb565;
      packets[i].thread_data = thr;
   }
}

static const struct softfilter_implementation scale2x_generic = {
   scale2x_generic_input_fmts,
   scale2x_generic_output_fmts,

   scale2x_generic_create,
   scale2x_generic_destroy,

   scale2x_generic_threads,
   scale2x_generic_output,
   scale2x_generic_packets,

   SOFTFILTER_API_VERSION,
   "Scale2x",
   "scale2x",
};

const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd)
{
   return &scale2x_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
