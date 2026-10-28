#include "softfilter.h"
#include <stdlib.h>
#include <string.h>
#include <retro_inline.h>

/* Vector runs (see softfilter_simd.h), which give the per-pixel code's
 * output to the bit. */
#include "softfilter_simd.h"

#ifdef RARCH_INTERNAL
#define softfilter_get_implementation paa_get_implementation
#define softfilter_thread_data paa_softfilter_thread_data
#define filter_data paa_filter_data
#endif

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

/* A vector run over one row: output pixels for source pixels x up to
 * x_end (exclusive), whole vectors only; returns the first pixel it
 * left. n, p and s are the rows above, of and below the pixels, out0
 * and out1 the two output rows. x must be at least 1 and the vectors
 * must end before the last pixel, so that both side neighbours are
 * inside the row. */
typedef unsigned (*paa_run_xrgb8888_t)(const uint32_t *n, const uint32_t *p,
      const uint32_t *s, uint32_t *out0, uint32_t *out1,
      unsigned x, unsigned x_end);
typedef unsigned (*paa_run_rgb565_t)(const uint16_t *n, const uint16_t *p,
      const uint16_t *s, uint16_t *out0, uint16_t *out1,
      unsigned x, unsigned x_end);

struct filter_data {
   unsigned threads;
   struct softfilter_thread_data *workers;
   unsigned in_fmt;
   /* Rows in the whole frame, against which the rows above and below
    * are clamped whichever band a worker has. */
   unsigned frame_height;
   paa_run_xrgb8888_t run_xrgb8888;
   paa_run_rgb565_t run_rgb565;
};

/* Support the 2 RetroArch formats  */
static unsigned paa_query_input_formats(void) {
   return SOFTFILTER_FMT_XRGB8888 | SOFTFILTER_FMT_RGB565;
}

static unsigned paa_query_output_formats(unsigned input_format) {
   return input_format;
}

static unsigned paa_query_num_threads(void *data) {
   struct filter_data *filt = (struct filter_data*)data;
   return filt->threads;
}

/* Helper RGB565 */
static INLINE uint16_t mix_565(uint16_t c1, uint16_t c2) {
   return (((c1 & 0xF7DE) >> 1) + ((c2 & 0xF7DE) >> 1));
}

/* Helper XRGB8888 */
static INLINE uint32_t mix_8888(uint32_t c1, uint32_t c2) {
   return (((c1 & 0xFEFEFEFE) >> 1) + ((c2 & 0xFEFEFEFE) >> 1));
}

/* One source pixel p and its neighbours north, south, west and east
 * (the pixel itself past the frame's edge) to its 2x2 output: each
 * output corner takes a 50:50 mix of p with the neighbour above or
 * below it when that neighbour equals the one beside the corner and
 * both differ from p, and p otherwise. */
#define PAA_PIXEL(TYPE, MIX, p, n, s, w, e, o0, o1) \
   { \
      TYPE c0 = p, c1 = p, c2 = p, c3 = p; \
      if (n != p && w != p && n == w) \
         c0 = MIX(p, n); \
      if (n != p && e != p && n == e) \
         c1 = MIX(p, n); \
      if (s != p && w != p && s == w) \
         c2 = MIX(p, s); \
      if (s != p && e != p && s == e) \
         c3 = MIX(p, s); \
      (o0)[0] = c0; (o0)[1] = c1; \
      (o1)[0] = c2; (o1)[1] = c3; \
   }

/* ---- SSE2 ---- */

#ifdef SOFTFILTER_HAVE_SSE2
#define PAA_LD(ptr) _mm_loadu_si128((const __m128i*)(ptr))
#define PAA_SEL(m, a, b) _mm_or_si128(_mm_and_si128(m, a), _mm_andnot_si128(m, b))
/* The corner rule for four corners at once, given which pixels equal p
 * (EQ is the lane compare) */
#define PAA_CORNERS_SSE2(EQ, MIXV, P, N, S, W, E, O0, O1, O2, O3) \
   { \
      __m128i ones_ = _mm_cmpeq_epi32(P, P); \
      __m128i nd_   = _mm_xor_si128(EQ(N, P), ones_); \
      __m128i sd_   = _mm_xor_si128(EQ(S, P), ones_); \
      __m128i wd_   = _mm_xor_si128(EQ(W, P), ones_); \
      __m128i ed_   = _mm_xor_si128(EQ(E, P), ones_); \
      __m128i mn_   = MIXV(P, N); \
      __m128i ms_   = MIXV(P, S); \
      O0 = PAA_SEL(_mm_and_si128(_mm_and_si128(nd_, wd_), EQ(N, W)), mn_, P); \
      O1 = PAA_SEL(_mm_and_si128(_mm_and_si128(nd_, ed_), EQ(N, E)), mn_, P); \
      O2 = PAA_SEL(_mm_and_si128(_mm_and_si128(sd_, wd_), EQ(S, W)), ms_, P); \
      O3 = PAA_SEL(_mm_and_si128(_mm_and_si128(sd_, ed_), EQ(S, E)), ms_, P); \
   }
#define PAA_MIX32_SSE2(a, b) _mm_add_epi32( \
      _mm_srli_epi32(_mm_and_si128(a, _mm_set1_epi32((int)0xFEFEFEFE)), 1), \
      _mm_srli_epi32(_mm_and_si128(b, _mm_set1_epi32((int)0xFEFEFEFE)), 1))
#define PAA_MIX16_SSE2(a, b) _mm_add_epi16( \
      _mm_srli_epi16(_mm_and_si128(a, _mm_set1_epi16((short)0xF7DE)), 1), \
      _mm_srli_epi16(_mm_and_si128(b, _mm_set1_epi16((short)0xF7DE)), 1))

static unsigned paa_run_xrgb8888_sse2(const uint32_t *n, const uint32_t *p,
      const uint32_t *s, uint32_t *out0, uint32_t *out1,
      unsigned x, unsigned x_end)
{
   for (; x + 4 <= x_end; x += 4)
   {
      __m128i P = PAA_LD(p + x), N = PAA_LD(n + x), S = PAA_LD(s + x);
      __m128i W = PAA_LD(p + x - 1), E = PAA_LD(p + x + 1);
      __m128i o0, o1, o2, o3;
      PAA_CORNERS_SSE2(_mm_cmpeq_epi32, PAA_MIX32_SSE2, P, N, S, W, E,
            o0, o1, o2, o3)
      _mm_storeu_si128((__m128i*)(out0 + 2 * x),     _mm_unpacklo_epi32(o0, o1));
      _mm_storeu_si128((__m128i*)(out0 + 2 * x + 4), _mm_unpackhi_epi32(o0, o1));
      _mm_storeu_si128((__m128i*)(out1 + 2 * x),     _mm_unpacklo_epi32(o2, o3));
      _mm_storeu_si128((__m128i*)(out1 + 2 * x + 4), _mm_unpackhi_epi32(o2, o3));
   }
   return x;
}

static unsigned paa_run_rgb565_sse2(const uint16_t *n, const uint16_t *p,
      const uint16_t *s, uint16_t *out0, uint16_t *out1,
      unsigned x, unsigned x_end)
{
   for (; x + 8 <= x_end; x += 8)
   {
      __m128i P = PAA_LD(p + x), N = PAA_LD(n + x), S = PAA_LD(s + x);
      __m128i W = PAA_LD(p + x - 1), E = PAA_LD(p + x + 1);
      __m128i o0, o1, o2, o3;
      PAA_CORNERS_SSE2(_mm_cmpeq_epi16, PAA_MIX16_SSE2, P, N, S, W, E,
            o0, o1, o2, o3)
      _mm_storeu_si128((__m128i*)(out0 + 2 * x),     _mm_unpacklo_epi16(o0, o1));
      _mm_storeu_si128((__m128i*)(out0 + 2 * x + 8), _mm_unpackhi_epi16(o0, o1));
      _mm_storeu_si128((__m128i*)(out1 + 2 * x),     _mm_unpacklo_epi16(o2, o3));
      _mm_storeu_si128((__m128i*)(out1 + 2 * x + 8), _mm_unpackhi_epi16(o2, o3));
   }
   return x;
}
#endif /* SOFTFILTER_HAVE_SSE2 */

/* ---- AVX2: the SSE2 runs, twice as wide ---- */

#ifdef SOFTFILTER_HAVE_AVX2
#define PAA_LD_256(ptr) _mm256_loadu_si256((const __m256i*)(ptr))
#define PAA_SEL_256(m, a, b) \
      _mm256_or_si256(_mm256_and_si256(m, a), _mm256_andnot_si256(m, b))
#define PAA_CORNERS_AVX2(EQ, MIXV, P, N, S, W, E, O0, O1, O2, O3) \
   { \
      __m256i ones_ = _mm256_cmpeq_epi32(P, P); \
      __m256i nd_   = _mm256_xor_si256(EQ(N, P), ones_); \
      __m256i sd_   = _mm256_xor_si256(EQ(S, P), ones_); \
      __m256i wd_   = _mm256_xor_si256(EQ(W, P), ones_); \
      __m256i ed_   = _mm256_xor_si256(EQ(E, P), ones_); \
      __m256i mn_   = MIXV(P, N); \
      __m256i ms_   = MIXV(P, S); \
      O0 = PAA_SEL_256(_mm256_and_si256(_mm256_and_si256(nd_, wd_), EQ(N, W)), mn_, P); \
      O1 = PAA_SEL_256(_mm256_and_si256(_mm256_and_si256(nd_, ed_), EQ(N, E)), mn_, P); \
      O2 = PAA_SEL_256(_mm256_and_si256(_mm256_and_si256(sd_, wd_), EQ(S, W)), ms_, P); \
      O3 = PAA_SEL_256(_mm256_and_si256(_mm256_and_si256(sd_, ed_), EQ(S, E)), ms_, P); \
   }
#define PAA_MIX32_AVX2(a, b) _mm256_add_epi32( \
      _mm256_srli_epi32(_mm256_and_si256(a, _mm256_set1_epi32((int)0xFEFEFEFE)), 1), \
      _mm256_srli_epi32(_mm256_and_si256(b, _mm256_set1_epi32((int)0xFEFEFEFE)), 1))
#define PAA_MIX16_AVX2(a, b) _mm256_add_epi16( \
      _mm256_srli_epi16(_mm256_and_si256(a, _mm256_set1_epi16((short)0xF7DE)), 1), \
      _mm256_srli_epi16(_mm256_and_si256(b, _mm256_set1_epi16((short)0xF7DE)), 1))
/* unpacklo/unpackhi interleave within each 128-bit half; the permutes
 * put the interleaved pixels back in order. */
#define PAA_STORE_PAIRS_256(dst, lo, hi, n) \
   { \
      _mm256_storeu_si256((__m256i*)(dst), \
            _mm256_permute2x128_si256(lo, hi, 0x20)); \
      _mm256_storeu_si256((__m256i*)((dst) + (n)), \
            _mm256_permute2x128_si256(lo, hi, 0x31)); \
   }

SOFTFILTER_TARGET_AVX2
static unsigned paa_run_xrgb8888_avx2(const uint32_t *n, const uint32_t *p,
      const uint32_t *s, uint32_t *out0, uint32_t *out1,
      unsigned x, unsigned x_end)
{
   for (; x + 8 <= x_end; x += 8)
   {
      __m256i P = PAA_LD_256(p + x), N = PAA_LD_256(n + x), S = PAA_LD_256(s + x);
      __m256i W = PAA_LD_256(p + x - 1), E = PAA_LD_256(p + x + 1);
      __m256i o0, o1, o2, o3;
      PAA_CORNERS_AVX2(_mm256_cmpeq_epi32, PAA_MIX32_AVX2, P, N, S, W, E,
            o0, o1, o2, o3)
      PAA_STORE_PAIRS_256(out0 + 2 * x, _mm256_unpacklo_epi32(o0, o1),
            _mm256_unpackhi_epi32(o0, o1), 8)
      PAA_STORE_PAIRS_256(out1 + 2 * x, _mm256_unpacklo_epi32(o2, o3),
            _mm256_unpackhi_epi32(o2, o3), 8)
   }
   SOFTFILTER_ZEROUPPER();
   return x;
}

SOFTFILTER_TARGET_AVX2
static unsigned paa_run_rgb565_avx2(const uint16_t *n, const uint16_t *p,
      const uint16_t *s, uint16_t *out0, uint16_t *out1,
      unsigned x, unsigned x_end)
{
   for (; x + 16 <= x_end; x += 16)
   {
      __m256i P = PAA_LD_256(p + x), N = PAA_LD_256(n + x), S = PAA_LD_256(s + x);
      __m256i W = PAA_LD_256(p + x - 1), E = PAA_LD_256(p + x + 1);
      __m256i o0, o1, o2, o3;
      PAA_CORNERS_AVX2(_mm256_cmpeq_epi16, PAA_MIX16_AVX2, P, N, S, W, E,
            o0, o1, o2, o3)
      PAA_STORE_PAIRS_256(out0 + 2 * x, _mm256_unpacklo_epi16(o0, o1),
            _mm256_unpackhi_epi16(o0, o1), 16)
      PAA_STORE_PAIRS_256(out1 + 2 * x, _mm256_unpacklo_epi16(o2, o3),
            _mm256_unpackhi_epi16(o2, o3), 16)
   }
   SOFTFILTER_ZEROUPPER();
   return x;
}
#endif /* SOFTFILTER_HAVE_AVX2 */

/* ---- NEON ---- */

#ifdef SOFTFILTER_HAVE_NEON
#define PAA_CORNERS_NEON(T, EQ, AND, BIC, BSL, MIXV, P, N, S, W, E, O0, O1, O2, O3) \
   { \
      T neq_ = EQ(N, P), seq_ = EQ(S, P), weq_ = EQ(W, P), eeq_ = EQ(E, P); \
      T mn_  = MIXV(P, N), ms_ = MIXV(P, S); \
      O0 = BSL(BIC(BIC(EQ(N, W), neq_), weq_), mn_, P); \
      O1 = BSL(BIC(BIC(EQ(N, E), neq_), eeq_), mn_, P); \
      O2 = BSL(BIC(BIC(EQ(S, W), seq_), weq_), ms_, P); \
      O3 = BSL(BIC(BIC(EQ(S, E), seq_), eeq_), ms_, P); \
   }
#define PAA_MIX32_NEON(a, b) vaddq_u32( \
      vshrq_n_u32(vandq_u32(a, vdupq_n_u32(0xFEFEFEFE)), 1), \
      vshrq_n_u32(vandq_u32(b, vdupq_n_u32(0xFEFEFEFE)), 1))
#define PAA_MIX16_NEON(a, b) vaddq_u16( \
      vshrq_n_u16(vandq_u16(a, vdupq_n_u16(0xF7DE)), 1), \
      vshrq_n_u16(vandq_u16(b, vdupq_n_u16(0xF7DE)), 1))

static unsigned paa_run_xrgb8888_neon(const uint32_t *n, const uint32_t *p,
      const uint32_t *s, uint32_t *out0, uint32_t *out1,
      unsigned x, unsigned x_end)
{
   for (; x + 4 <= x_end; x += 4)
   {
      uint32x4_t P = vld1q_u32(p + x), N = vld1q_u32(n + x), S = vld1q_u32(s + x);
      uint32x4_t W = vld1q_u32(p + x - 1), E = vld1q_u32(p + x + 1);
      uint32x4_t o0, o1, o2, o3;
      uint32x4x2_t r0, r1;
      PAA_CORNERS_NEON(uint32x4_t, vceqq_u32, vandq_u32, vbicq_u32, vbslq_u32,
            PAA_MIX32_NEON, P, N, S, W, E, o0, o1, o2, o3)
      r0 = vzipq_u32(o0, o1);
      r1 = vzipq_u32(o2, o3);
      vst1q_u32(out0 + 2 * x,     r0.val[0]);
      vst1q_u32(out0 + 2 * x + 4, r0.val[1]);
      vst1q_u32(out1 + 2 * x,     r1.val[0]);
      vst1q_u32(out1 + 2 * x + 4, r1.val[1]);
   }
   return x;
}

static unsigned paa_run_rgb565_neon(const uint16_t *n, const uint16_t *p,
      const uint16_t *s, uint16_t *out0, uint16_t *out1,
      unsigned x, unsigned x_end)
{
   for (; x + 8 <= x_end; x += 8)
   {
      uint16x8_t P = vld1q_u16(p + x), N = vld1q_u16(n + x), S = vld1q_u16(s + x);
      uint16x8_t W = vld1q_u16(p + x - 1), E = vld1q_u16(p + x + 1);
      uint16x8_t o0, o1, o2, o3;
      uint16x8x2_t r0, r1;
      PAA_CORNERS_NEON(uint16x8_t, vceqq_u16, vandq_u16, vbicq_u16, vbslq_u16,
            PAA_MIX16_NEON, P, N, S, W, E, o0, o1, o2, o3)
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

static void *paa_create(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata) {
   struct filter_data *filt = (struct filter_data*)calloc(1, sizeof(*filt));
   (void)simd;
   if (!filt) return NULL;
   if (threads < 1)
      threads = 1;
   if (!(filt->workers = (struct softfilter_thread_data*)
            calloc(threads, sizeof(struct softfilter_thread_data))))
   {
      free(filt);
      return NULL;
   }
   /* Each output row pair needs only the source row and the ones
    * either side of it, so the rows are split between the workers. */
   filt->threads = threads;
   filt->in_fmt = in_fmt;
#if defined(SOFTFILTER_HAVE_SSE2)
   filt->run_xrgb8888 = paa_run_xrgb8888_sse2;
   filt->run_rgb565   = paa_run_rgb565_sse2;
#ifdef SOFTFILTER_HAVE_AVX2
   if (simd & SOFTFILTER_SIMD_AVX2)
   {
      filt->run_xrgb8888 = paa_run_xrgb8888_avx2;
      filt->run_rgb565   = paa_run_rgb565_avx2;
   }
#endif
#elif defined(SOFTFILTER_HAVE_NEON)
   filt->run_xrgb8888 = paa_run_xrgb8888_neon;
   filt->run_rgb565   = paa_run_rgb565_neon;
#endif
   return filt;
}

static void paa_destroy(void *data) {
   struct filter_data *filt = (struct filter_data*)data;
   if (filt) { free(filt->workers); free(filt); }
}

static void paa_query_output_size(void *data, unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height) {
   *out_width = width << 1;
   *out_height = height << 1;
}

/* A worker's rows: the first and last pixel of each row (whose side
 * neighbours clamp) and whatever a vector run leaves go one by one. */
#define PAA_ROWS(TYPE, MIX, RUN) \
   { \
      unsigned y, x; \
      unsigned width        = thr->width; \
      unsigned frame_height = filt->frame_height; \
      for (y = 0; y < thr->height; y++) \
      { \
         unsigned fy    = thr->first + y; \
         const TYPE *pr = in + (size_t)y * in_stride; \
         const TYPE *nr = fy > 0 ? pr - in_stride : pr; \
         const TYPE *sr = fy + 1 < frame_height ? pr + in_stride : pr; \
         TYPE *o0       = out + (size_t)y * 2 * out_stride; \
         TYPE *o1       = o0 + out_stride; \
         for (x = 0; x < width; ) \
         { \
            TYPE p = pr[x]; \
            TYPE n = nr[x]; \
            TYPE s = sr[x]; \
            TYPE w = x > 0 ? pr[x - 1] : p; \
            TYPE e = x + 1 < width ? pr[x + 1] : p; \
            PAA_PIXEL(TYPE, MIX, p, n, s, w, e, o0 + 2 * x, o1 + 2 * x) \
            x++; \
            if (RUN && x == 1 && width > 2) \
               x = RUN(nr, pr, sr, o0, o1, 1, width - 1); \
         } \
      } \
   }

static void paa_work_cb_xrgb8888(void *data, void *thread_data)
{
   struct filter_data *filt           = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint32_t *in  = (const uint32_t*)thr->in_data;
   uint32_t *out       = (uint32_t*)thr->out_data;
   size_t in_stride    = thr->in_pitch  >> 2;
   size_t out_stride   = thr->out_pitch >> 2;
   PAA_ROWS(uint32_t, mix_8888, filt->run_xrgb8888)
}

static void paa_work_cb_rgb565(void *data, void *thread_data)
{
   struct filter_data *filt           = (struct filter_data*)data;
   struct softfilter_thread_data *thr = (struct softfilter_thread_data*)thread_data;
   const uint16_t *in  = (const uint16_t*)thr->in_data;
   uint16_t *out       = (uint16_t*)thr->out_data;
   size_t in_stride    = thr->in_pitch  >> 1;
   size_t out_stride   = thr->out_pitch >> 1;
   PAA_ROWS(uint16_t, mix_565, filt->run_rgb565)
}

static void paa_get_work_packets(void *data,
		struct softfilter_work_packet *packets,
		void *output, size_t output_stride,
		const void *input, unsigned width, unsigned height,
		size_t input_stride)
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
      thr->first     = y_start;

      if (filt->in_fmt == SOFTFILTER_FMT_XRGB8888)
         packets[i].work = paa_work_cb_xrgb8888;
      else if (filt->in_fmt == SOFTFILTER_FMT_RGB565)
         packets[i].work = paa_work_cb_rgb565;
      packets[i].thread_data = thr;
   }
}

static const struct softfilter_implementation paa_impl = {
   paa_query_input_formats, paa_query_output_formats,
   paa_create, paa_destroy, paa_query_num_threads,
   paa_query_output_size, paa_get_work_packets,
   SOFTFILTER_API_VERSION, "Pixel Art AA", "pixel_art_aa",
};

const struct softfilter_implementation *softfilter_get_implementation(softfilter_simd_mask_t simd) {
   return &paa_impl;
}

#ifdef RARCH_INTERNAL
#undef softfilter_get_implementation
#undef softfilter_thread_data
#undef filter_data
#endif
