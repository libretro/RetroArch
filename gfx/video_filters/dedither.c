#include "softfilter.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <retro_inline.h>

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation dedither_get_implementation
#define softfilter_thread_data dedither_softfilter_thread_data
#define filter_data dedither_filter_data
#endif

/* Vector tiers (see softfilter_simd.h): each works out every test of a
 * run of pixels at once, branch-free, and gives the plain code's output
 * to the bit. */
#include "softfilter_simd.h"

/* The threshold below which two XRGB8888 pixels count as the same colour:
 * the sum of their red, green and blue differences. */
#define DEDITHER_THRESHOLD 40

struct softfilter_thread_data {
   void *out_data;
   const void *in_data;
   size_t out_pitch;
   size_t in_pitch;
   unsigned width;
   unsigned height;
   /* This worker's first row in the frame */
   unsigned first;
};

/* A vector run: pixels x up to x_end (exclusive) of one interior row,
 * whole vectors only; returns the first pixel it left. */
typedef unsigned (*dedither_run_xrgb8888_t)(const uint32_t *line,
      ptrdiff_t stride, uint32_t *out, unsigned x, unsigned x_end);
typedef unsigned (*dedither_run_rgb565_t)(const uint16_t *line,
      ptrdiff_t stride, uint16_t *out, unsigned x, unsigned x_end);

struct filter_data {
   unsigned threads;
   struct softfilter_thread_data *workers;
   unsigned in_fmt;
   /* Rows in the whole frame, which the 6x6 pattern is bounded by
    * whichever slice a worker has. */
   unsigned frame_height;
   dedither_run_xrgb8888_t run_xrgb8888;
   dedither_run_rgb565_t run_rgb565;
};

static unsigned dedither_generic_input_fmts(void) {
   return SOFTFILTER_FMT_XRGB8888 | SOFTFILTER_FMT_RGB565;
}

static unsigned dedither_generic_output_fmts(unsigned input_fmts) {
   return input_fmts;
}

static unsigned dedither_generic_threads(void *data) {
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

/* ---- The plain code: one pixel ---- */

static INLINE int pix_equal(uint32_t c1, uint32_t c2, int threshold)
{
   int r = abs((int)((c1 >> 16) & 0xFF) - (int)((c2 >> 16) & 0xFF));
   int g = abs((int)((c1 >> 8) & 0xFF) - (int)((c2 >> 8) & 0xFF));
   int b = abs((int)(c1 & 0xFF) - (int)(c2 & 0xFF));
   return (r + g + b) < threshold;
}

/* An interior pixel: an A-B-A-B-A-B run through it, across or down, is
 * dither, and the pixel is blended 1:2:1 with its two neighbours in that
 * direction (across wins); anything else is kept sharp. The pattern
 * reaches two pixels before and three after. */
static INLINE uint32_t dedither_pixel_xrgb8888(const uint32_t *line,
      ptrdiff_t stride, unsigned x)
{
   uint32_t h1 = line[x - 2], h2 = line[x - 1], h3 = line[x];
   uint32_t h4 = line[x + 1], h5 = line[x + 2], h6 = line[x + 3];
   uint32_t v1 = line[x - 2 * stride], v2 = line[x - stride];
   uint32_t v4 = line[x + stride], v5 = line[x + 2 * stride];
   uint32_t v6 = line[x + 3 * stride];
   int h_dit = (pix_equal(h1, h3, DEDITHER_THRESHOLD)
         && pix_equal(h3, h5, DEDITHER_THRESHOLD)
         && pix_equal(h2, h4, DEDITHER_THRESHOLD)
         && pix_equal(h4, h6, DEDITHER_THRESHOLD)
         && !pix_equal(h3, h4, DEDITHER_THRESHOLD));
   int v_dit = (pix_equal(v1, h3, DEDITHER_THRESHOLD)
         && pix_equal(h3, v5, DEDITHER_THRESHOLD)
         && pix_equal(v2, v4, DEDITHER_THRESHOLD)
         && pix_equal(v4, v6, DEDITHER_THRESHOLD)
         && !pix_equal(h3, v4, DEDITHER_THRESHOLD));
   if (h_dit || v_dit)
   {
      uint32_t avg;
      if (h_dit)
         avg = (((h2 & 0xFEFEFEFE) >> 1) + ((h4 & 0xFEFEFEFE) >> 1));
      else
         avg = (((v2 & 0xFEFEFEFE) >> 1) + ((v4 & 0xFEFEFEFE) >> 1));
      /* Apply soft blend (1:2:1 weighting) */
      return (((avg & 0xFEFEFEFE) >> 1) + ((h3 & 0xFEFEFEFE) >> 1));
   }
   return h3; /* Keep original sharp pixel */
}

/* RGB565 takes exact equality, and its vertical run tests the pixel
 * against the one above. */
static INLINE uint16_t dedither_pixel_rgb565(const uint16_t *line,
      ptrdiff_t stride, unsigned x)
{
   uint16_t h1 = line[x - 2], h2 = line[x - 1], h3 = line[x];
   uint16_t h4 = line[x + 1], h5 = line[x + 2], h6 = line[x + 3];
   uint16_t v1 = line[x - 2 * stride], v2 = line[x - stride];
   uint16_t v4 = line[x + stride], v5 = line[x + 2 * stride];
   uint16_t v6 = line[x + 3 * stride];
   int h_dit = (h1 == h3 && h3 == h5 && h2 == h4 && h4 == h6 && h3 != h4);
   int v_dit = (v1 == h3 && h3 == v5 && v2 == v4 && v4 == v6 && h3 != v2);
   if (h_dit || v_dit)
   {
      uint16_t avg_s = (h_dit)
         ? (((h2 & 0xF7DE) >> 1) + ((h4 & 0xF7DE) >> 1))
         : (((v2 & 0xF7DE) >> 1) + ((v4 & 0xF7DE) >> 1));
      return (((avg_s & 0xF7DE) >> 1) + ((h3 & 0xF7DE) >> 1));
   }
   return h3;
}

/* ---- SSE2 ---- */

#ifdef SOFTFILTER_HAVE_SSE2
/* pix_equal of four pixel pairs: all ones where the colours count as the
 * same. The byte differences are summed over red, green and blue in each
 * pixel's 32 bits. */
static INLINE __m128i dedither_eq_sse2(__m128i a, __m128i b)
{
   __m128i d = _mm_and_si128(_mm_or_si128(_mm_subs_epu8(a, b),
            _mm_subs_epu8(b, a)), _mm_set1_epi32(0x00FFFFFF));
   __m128i t = _mm_add_epi32(_mm_and_si128(d, _mm_set1_epi32(0x00FF00FF)),
         _mm_and_si128(_mm_srli_epi32(d, 8), _mm_set1_epi32(0x00FF00FF)));
   __m128i s = _mm_add_epi32(_mm_and_si128(t, _mm_set1_epi32(0xFFFF)),
         _mm_srli_epi32(t, 16));
   return _mm_cmplt_epi32(s, _mm_set1_epi32(DEDITHER_THRESHOLD));
}

/* ((a & mask) >> 1) + ((b & mask) >> 1), lane-wise */
#define DEDITHER_HALF_SUM_32(a, b) _mm_add_epi32( \
      _mm_srli_epi32(_mm_and_si128(a, _mm_set1_epi32((int)0xFEFEFEFE)), 1), \
      _mm_srli_epi32(_mm_and_si128(b, _mm_set1_epi32((int)0xFEFEFEFE)), 1))
#define DEDITHER_HALF_SUM_16(a, b) _mm_add_epi16( \
      _mm_srli_epi16(_mm_and_si128(a, _mm_set1_epi16((short)0xF7DE)), 1), \
      _mm_srli_epi16(_mm_and_si128(b, _mm_set1_epi16((short)0xF7DE)), 1))
#define DEDITHER_SELECT(m, a, b) \
      _mm_or_si128(_mm_and_si128(m, a), _mm_andnot_si128(m, b))
#define DEDITHER_LD(p) _mm_loadu_si128((const __m128i*)(p))

static unsigned dedither_run_xrgb8888_sse2(const uint32_t *line,
      ptrdiff_t stride, uint32_t *out, unsigned x, unsigned x_end)
{
   for (; x + 4 <= x_end; x += 4)
   {
      const uint32_t *p = line + x;
      __m128i h1 = DEDITHER_LD(p - 2), h2 = DEDITHER_LD(p - 1);
      __m128i h3 = DEDITHER_LD(p),     h4 = DEDITHER_LD(p + 1);
      __m128i h5 = DEDITHER_LD(p + 2), h6 = DEDITHER_LD(p + 3);
      __m128i v1 = DEDITHER_LD(p - 2 * stride), v2 = DEDITHER_LD(p - stride);
      __m128i v4 = DEDITHER_LD(p + stride), v5 = DEDITHER_LD(p + 2 * stride);
      __m128i v6 = DEDITHER_LD(p + 3 * stride);
      __m128i h_dit = _mm_andnot_si128(dedither_eq_sse2(h3, h4),
            _mm_and_si128(
               _mm_and_si128(dedither_eq_sse2(h1, h3), dedither_eq_sse2(h3, h5)),
               _mm_and_si128(dedither_eq_sse2(h2, h4), dedither_eq_sse2(h4, h6))));
      __m128i v_dit = _mm_andnot_si128(dedither_eq_sse2(h3, v4),
            _mm_and_si128(
               _mm_and_si128(dedither_eq_sse2(v1, h3), dedither_eq_sse2(h3, v5)),
               _mm_and_si128(dedither_eq_sse2(v2, v4), dedither_eq_sse2(v4, v6))));
      __m128i avg   = DEDITHER_SELECT(h_dit, DEDITHER_HALF_SUM_32(h2, h4),
            DEDITHER_HALF_SUM_32(v2, v4));
      __m128i blend = DEDITHER_HALF_SUM_32(avg, h3);
      _mm_storeu_si128((__m128i*)(out + x),
            DEDITHER_SELECT(_mm_or_si128(h_dit, v_dit), blend, h3));
   }
   return x;
}

static unsigned dedither_run_rgb565_sse2(const uint16_t *line,
      ptrdiff_t stride, uint16_t *out, unsigned x, unsigned x_end)
{
   for (; x + 8 <= x_end; x += 8)
   {
      const uint16_t *p = line + x;
      __m128i h1 = DEDITHER_LD(p - 2), h2 = DEDITHER_LD(p - 1);
      __m128i h3 = DEDITHER_LD(p),     h4 = DEDITHER_LD(p + 1);
      __m128i h5 = DEDITHER_LD(p + 2), h6 = DEDITHER_LD(p + 3);
      __m128i v1 = DEDITHER_LD(p - 2 * stride), v2 = DEDITHER_LD(p - stride);
      __m128i v4 = DEDITHER_LD(p + stride), v5 = DEDITHER_LD(p + 2 * stride);
      __m128i v6 = DEDITHER_LD(p + 3 * stride);
      __m128i h_dit = _mm_andnot_si128(_mm_cmpeq_epi16(h3, h4),
            _mm_and_si128(
               _mm_and_si128(_mm_cmpeq_epi16(h1, h3), _mm_cmpeq_epi16(h3, h5)),
               _mm_and_si128(_mm_cmpeq_epi16(h2, h4), _mm_cmpeq_epi16(h4, h6))));
      __m128i v_dit = _mm_andnot_si128(_mm_cmpeq_epi16(h3, v2),
            _mm_and_si128(
               _mm_and_si128(_mm_cmpeq_epi16(v1, h3), _mm_cmpeq_epi16(h3, v5)),
               _mm_and_si128(_mm_cmpeq_epi16(v2, v4), _mm_cmpeq_epi16(v4, v6))));
      __m128i avg   = DEDITHER_SELECT(h_dit, DEDITHER_HALF_SUM_16(h2, h4),
            DEDITHER_HALF_SUM_16(v2, v4));
      __m128i blend = DEDITHER_HALF_SUM_16(avg, h3);
      _mm_storeu_si128((__m128i*)(out + x),
            DEDITHER_SELECT(_mm_or_si128(h_dit, v_dit), blend, h3));
   }
   return x;
}
#endif /* SOFTFILTER_HAVE_SSE2 */

/* ---- AVX2: the SSE2 runs, twice as wide ---- */

#ifdef SOFTFILTER_HAVE_AVX2
SOFTFILTER_TARGET_AVX2
static INLINE __m256i dedither_eq_avx2(__m256i a, __m256i b)
{
   __m256i d = _mm256_and_si256(_mm256_or_si256(_mm256_subs_epu8(a, b),
            _mm256_subs_epu8(b, a)), _mm256_set1_epi32(0x00FFFFFF));
   __m256i t = _mm256_add_epi32(
         _mm256_and_si256(d, _mm256_set1_epi32(0x00FF00FF)),
         _mm256_and_si256(_mm256_srli_epi32(d, 8), _mm256_set1_epi32(0x00FF00FF)));
   __m256i s = _mm256_add_epi32(_mm256_and_si256(t, _mm256_set1_epi32(0xFFFF)),
         _mm256_srli_epi32(t, 16));
   return _mm256_cmpgt_epi32(_mm256_set1_epi32(DEDITHER_THRESHOLD), s);
}

#define DEDITHER_HALF_SUM_32_256(a, b) _mm256_add_epi32( \
      _mm256_srli_epi32(_mm256_and_si256(a, _mm256_set1_epi32((int)0xFEFEFEFE)), 1), \
      _mm256_srli_epi32(_mm256_and_si256(b, _mm256_set1_epi32((int)0xFEFEFEFE)), 1))
#define DEDITHER_HALF_SUM_16_256(a, b) _mm256_add_epi16( \
      _mm256_srli_epi16(_mm256_and_si256(a, _mm256_set1_epi16((short)0xF7DE)), 1), \
      _mm256_srli_epi16(_mm256_and_si256(b, _mm256_set1_epi16((short)0xF7DE)), 1))
#define DEDITHER_SELECT_256(m, a, b) \
      _mm256_or_si256(_mm256_and_si256(m, a), _mm256_andnot_si256(m, b))
#define DEDITHER_LD_256(p) _mm256_loadu_si256((const __m256i*)(p))

SOFTFILTER_TARGET_AVX2
static unsigned dedither_run_xrgb8888_avx2(const uint32_t *line,
      ptrdiff_t stride, uint32_t *out, unsigned x, unsigned x_end)
{
   for (; x + 8 <= x_end; x += 8)
   {
      const uint32_t *p = line + x;
      __m256i h1 = DEDITHER_LD_256(p - 2), h2 = DEDITHER_LD_256(p - 1);
      __m256i h3 = DEDITHER_LD_256(p),     h4 = DEDITHER_LD_256(p + 1);
      __m256i h5 = DEDITHER_LD_256(p + 2), h6 = DEDITHER_LD_256(p + 3);
      __m256i v1 = DEDITHER_LD_256(p - 2 * stride), v2 = DEDITHER_LD_256(p - stride);
      __m256i v4 = DEDITHER_LD_256(p + stride), v5 = DEDITHER_LD_256(p + 2 * stride);
      __m256i v6 = DEDITHER_LD_256(p + 3 * stride);
      __m256i h_dit = _mm256_andnot_si256(dedither_eq_avx2(h3, h4),
            _mm256_and_si256(
               _mm256_and_si256(dedither_eq_avx2(h1, h3), dedither_eq_avx2(h3, h5)),
               _mm256_and_si256(dedither_eq_avx2(h2, h4), dedither_eq_avx2(h4, h6))));
      __m256i v_dit = _mm256_andnot_si256(dedither_eq_avx2(h3, v4),
            _mm256_and_si256(
               _mm256_and_si256(dedither_eq_avx2(v1, h3), dedither_eq_avx2(h3, v5)),
               _mm256_and_si256(dedither_eq_avx2(v2, v4), dedither_eq_avx2(v4, v6))));
      __m256i avg   = DEDITHER_SELECT_256(h_dit, DEDITHER_HALF_SUM_32_256(h2, h4),
            DEDITHER_HALF_SUM_32_256(v2, v4));
      __m256i blend = DEDITHER_HALF_SUM_32_256(avg, h3);
      _mm256_storeu_si256((__m256i*)(out + x),
            DEDITHER_SELECT_256(_mm256_or_si256(h_dit, v_dit), blend, h3));
   }
   SOFTFILTER_ZEROUPPER();
   return x;
}

SOFTFILTER_TARGET_AVX2
static unsigned dedither_run_rgb565_avx2(const uint16_t *line,
      ptrdiff_t stride, uint16_t *out, unsigned x, unsigned x_end)
{
   for (; x + 16 <= x_end; x += 16)
   {
      const uint16_t *p = line + x;
      __m256i h1 = DEDITHER_LD_256(p - 2), h2 = DEDITHER_LD_256(p - 1);
      __m256i h3 = DEDITHER_LD_256(p),     h4 = DEDITHER_LD_256(p + 1);
      __m256i h5 = DEDITHER_LD_256(p + 2), h6 = DEDITHER_LD_256(p + 3);
      __m256i v1 = DEDITHER_LD_256(p - 2 * stride), v2 = DEDITHER_LD_256(p - stride);
      __m256i v4 = DEDITHER_LD_256(p + stride), v5 = DEDITHER_LD_256(p + 2 * stride);
      __m256i v6 = DEDITHER_LD_256(p + 3 * stride);
      __m256i h_dit = _mm256_andnot_si256(_mm256_cmpeq_epi16(h3, h4),
            _mm256_and_si256(
               _mm256_and_si256(_mm256_cmpeq_epi16(h1, h3), _mm256_cmpeq_epi16(h3, h5)),
               _mm256_and_si256(_mm256_cmpeq_epi16(h2, h4), _mm256_cmpeq_epi16(h4, h6))));
      __m256i v_dit = _mm256_andnot_si256(_mm256_cmpeq_epi16(h3, v2),
            _mm256_and_si256(
               _mm256_and_si256(_mm256_cmpeq_epi16(v1, h3), _mm256_cmpeq_epi16(h3, v5)),
               _mm256_and_si256(_mm256_cmpeq_epi16(v2, v4), _mm256_cmpeq_epi16(v4, v6))));
      __m256i avg   = DEDITHER_SELECT_256(h_dit, DEDITHER_HALF_SUM_16_256(h2, h4),
            DEDITHER_HALF_SUM_16_256(v2, v4));
      __m256i blend = DEDITHER_HALF_SUM_16_256(avg, h3);
      _mm256_storeu_si256((__m256i*)(out + x),
            DEDITHER_SELECT_256(_mm256_or_si256(h_dit, v_dit), blend, h3));
   }
   SOFTFILTER_ZEROUPPER();
   return x;
}
#endif /* SOFTFILTER_HAVE_AVX2 */

/* ---- NEON ---- */

#ifdef SOFTFILTER_HAVE_NEON
static INLINE uint32x4_t dedither_eq_neon(uint32x4_t a, uint32x4_t b)
{
   /* Byte differences, alpha dropped, summed per pixel */
   uint8x16_t d = vandq_u8(vabdq_u8(vreinterpretq_u8_u32(a),
            vreinterpretq_u8_u32(b)),
         vreinterpretq_u8_u32(vdupq_n_u32(0x00FFFFFF)));
   uint32x4_t s = vpaddlq_u16(vpaddlq_u8(d));
   return vcltq_u32(s, vdupq_n_u32(DEDITHER_THRESHOLD));
}

#define DEDITHER_HALF_SUM_32_NEON(a, b) vaddq_u32( \
      vshrq_n_u32(vandq_u32(a, vdupq_n_u32(0xFEFEFEFE)), 1), \
      vshrq_n_u32(vandq_u32(b, vdupq_n_u32(0xFEFEFEFE)), 1))
#define DEDITHER_HALF_SUM_16_NEON(a, b) vaddq_u16( \
      vshrq_n_u16(vandq_u16(a, vdupq_n_u16(0xF7DE)), 1), \
      vshrq_n_u16(vandq_u16(b, vdupq_n_u16(0xF7DE)), 1))

static unsigned dedither_run_xrgb8888_neon(const uint32_t *line,
      ptrdiff_t stride, uint32_t *out, unsigned x, unsigned x_end)
{
   for (; x + 4 <= x_end; x += 4)
   {
      const uint32_t *p = line + x;
      uint32x4_t h1 = vld1q_u32(p - 2), h2 = vld1q_u32(p - 1);
      uint32x4_t h3 = vld1q_u32(p),     h4 = vld1q_u32(p + 1);
      uint32x4_t h5 = vld1q_u32(p + 2), h6 = vld1q_u32(p + 3);
      uint32x4_t v1 = vld1q_u32(p - 2 * stride), v2 = vld1q_u32(p - stride);
      uint32x4_t v4 = vld1q_u32(p + stride), v5 = vld1q_u32(p + 2 * stride);
      uint32x4_t v6 = vld1q_u32(p + 3 * stride);
      uint32x4_t h_dit = vbicq_u32(
            vandq_u32(vandq_u32(dedither_eq_neon(h1, h3), dedither_eq_neon(h3, h5)),
               vandq_u32(dedither_eq_neon(h2, h4), dedither_eq_neon(h4, h6))),
            dedither_eq_neon(h3, h4));
      uint32x4_t v_dit = vbicq_u32(
            vandq_u32(vandq_u32(dedither_eq_neon(v1, h3), dedither_eq_neon(h3, v5)),
               vandq_u32(dedither_eq_neon(v2, v4), dedither_eq_neon(v4, v6))),
            dedither_eq_neon(h3, v4));
      uint32x4_t avg   = vbslq_u32(h_dit, DEDITHER_HALF_SUM_32_NEON(h2, h4),
            DEDITHER_HALF_SUM_32_NEON(v2, v4));
      uint32x4_t blend = DEDITHER_HALF_SUM_32_NEON(avg, h3);
      vst1q_u32(out + x, vbslq_u32(vorrq_u32(h_dit, v_dit), blend, h3));
   }
   return x;
}

static unsigned dedither_run_rgb565_neon(const uint16_t *line,
      ptrdiff_t stride, uint16_t *out, unsigned x, unsigned x_end)
{
   for (; x + 8 <= x_end; x += 8)
   {
      const uint16_t *p = line + x;
      uint16x8_t h1 = vld1q_u16(p - 2), h2 = vld1q_u16(p - 1);
      uint16x8_t h3 = vld1q_u16(p),     h4 = vld1q_u16(p + 1);
      uint16x8_t h5 = vld1q_u16(p + 2), h6 = vld1q_u16(p + 3);
      uint16x8_t v1 = vld1q_u16(p - 2 * stride), v2 = vld1q_u16(p - stride);
      uint16x8_t v4 = vld1q_u16(p + stride), v5 = vld1q_u16(p + 2 * stride);
      uint16x8_t v6 = vld1q_u16(p + 3 * stride);
      uint16x8_t h_dit = vbicq_u16(
            vandq_u16(vandq_u16(vceqq_u16(h1, h3), vceqq_u16(h3, h5)),
               vandq_u16(vceqq_u16(h2, h4), vceqq_u16(h4, h6))),
            vceqq_u16(h3, h4));
      uint16x8_t v_dit = vbicq_u16(
            vandq_u16(vandq_u16(vceqq_u16(v1, h3), vceqq_u16(h3, v5)),
               vandq_u16(vceqq_u16(v2, v4), vceqq_u16(v4, v6))),
            vceqq_u16(h3, v2));
      uint16x8_t avg   = vbslq_u16(h_dit, DEDITHER_HALF_SUM_16_NEON(h2, h4),
            DEDITHER_HALF_SUM_16_NEON(v2, v4));
      uint16x8_t blend = DEDITHER_HALF_SUM_16_NEON(avg, h3);
      vst1q_u16(out + x, vbslq_u16(vorrq_u16(h_dit, v_dit), blend, h3));
   }
   return x;
}
#endif /* SOFTFILTER_HAVE_NEON */

static void *dedither_generic_create(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata) {
   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   (void)config;
   (void)userdata;
   (void)simd;
   if (!filt)
      return NULL;
   if (threads < 1)
      threads = 1;
   if (!(filt->workers = (struct softfilter_thread_data*)
            calloc(threads, sizeof(struct softfilter_thread_data))))
   {
      free(filt);
      return NULL;
   }
   filt->threads = threads;
   filt->in_fmt  = in_fmt;
#if defined(SOFTFILTER_HAVE_SSE2)
   filt->run_xrgb8888 = dedither_run_xrgb8888_sse2;
   filt->run_rgb565   = dedither_run_rgb565_sse2;
#ifdef SOFTFILTER_HAVE_AVX2
   if (simd & SOFTFILTER_SIMD_AVX2)
   {
      filt->run_xrgb8888 = dedither_run_xrgb8888_avx2;
      filt->run_rgb565   = dedither_run_rgb565_avx2;
   }
#endif
#elif defined(SOFTFILTER_HAVE_NEON)
   filt->run_xrgb8888 = dedither_run_xrgb8888_neon;
   filt->run_rgb565   = dedither_run_rgb565_neon;
#endif
   return filt;
}

static void dedither_generic_destroy(void *data)
{
   struct filter_data *filt = (struct filter_data*)data;
   if (filt)
   {
      free(filt->workers);
      free(filt);
   }
}

static void dedither_generic_output(void *data, unsigned *out_width, unsigned *out_height, unsigned width, unsigned height)
{
   *out_width  = width;
   *out_height = height;
}

/* A worker's rows. Pixels whose 6x6 pattern would leave the frame (the
 * first two and last three rows and columns) are copied; the interior
 * goes through the vector run where there is one, and the plain code
 * for what is left of a row. */
#define DEDITHER_ROWS(TYPE, FMT, RUN) \
   { \
      int fw = (int)thr->width; \
      int fh = (int)filt->frame_height; \
      unsigned y; \
      for (y = 0; y < thr->height; y++) \
      { \
         int fy             = (int)(thr->first + y); \
         const TYPE *line   = in  + (size_t)y * in_stride; \
         TYPE *o            = out + (size_t)y * out_stride; \
         if (fy < 2 || fy >= fh - 3 || fw < 6) \
            memcpy(o, line, thr->width * sizeof(TYPE)); \
         else \
         { \
            unsigned x       = 2; \
            unsigned x_end   = (unsigned)(fw - 3); \
            o[0] = line[0]; \
            o[1] = line[1]; \
            if (RUN) \
               x = RUN(line, (ptrdiff_t)in_stride, o, x, x_end); \
            for (; x < x_end; x++) \
               o[x] = dedither_pixel_##FMT(line, (ptrdiff_t)in_stride, x); \
            o[x_end]     = line[x_end]; \
            o[x_end + 1] = line[x_end + 1]; \
            o[x_end + 2] = line[x_end + 2]; \
         } \
      } \
   }

static void dedither_work_cb_xrgb8888(void *data, void *thread_data)
{
   struct filter_data *filt           = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint32_t *in = (const uint32_t*)thr->in_data;
   uint32_t *out      = (uint32_t*)thr->out_data;
   size_t in_stride   = thr->in_pitch  / sizeof(uint32_t);
   size_t out_stride  = thr->out_pitch / sizeof(uint32_t);
   DEDITHER_ROWS(uint32_t, xrgb8888, filt->run_xrgb8888)
}

static void dedither_work_cb_rgb565(void *data, void *thread_data)
{
   struct filter_data *filt           = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint16_t *in = (const uint16_t*)thr->in_data;
   uint16_t *out      = (uint16_t*)thr->out_data;
   size_t in_stride   = thr->in_pitch  / sizeof(uint16_t);
   size_t out_stride  = thr->out_pitch / sizeof(uint16_t);
   DEDITHER_ROWS(uint16_t, rgb565, filt->run_rgb565)
}

static void dedither_generic_packets(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride, const void *input,
      unsigned width, unsigned height, size_t input_stride)
{
   unsigned i;
   struct filter_data *filt = (struct filter_data*)data;

   filt->frame_height = height;

   for (i = 0; i < filt->threads; i++)
   {
      struct softfilter_thread_data *thr = &filt->workers[i];
      unsigned y_start = (height * i) / filt->threads;
      unsigned y_end   = (height * (i + 1)) / filt->threads;

      thr->out_data  = (uint8_t*)output + y_start * output_stride;
      thr->in_data   = (const uint8_t*)input + y_start * input_stride;
      thr->out_pitch = output_stride;
      thr->in_pitch  = input_stride;
      thr->width     = width;
      thr->height    = y_end - y_start;
      thr->first     = y_start;

      if (filt->in_fmt == SOFTFILTER_FMT_XRGB8888)
         packets[i].work = dedither_work_cb_xrgb8888;
      else if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
         packets[i].work = dedither_work_cb_rgb565;
      packets[i].thread_data = thr;
   }
}

static const struct softfilter_implementation dedither_generic = {
   dedither_generic_input_fmts, dedither_generic_output_fmts,
   dedither_generic_create, dedither_generic_destroy,
   dedither_generic_threads, dedither_generic_output, dedither_generic_packets,
   SOFTFILTER_API_VERSION, "Master De-Dither 2D (6x6)", "dedither",
};

const struct softfilter_implementation *softfilter_get_implementation(softfilter_simd_mask_t simd)
{
   return &dedither_generic;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
