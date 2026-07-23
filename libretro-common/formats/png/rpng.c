/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rpng.c).
 * ---------------------------------------------------------------------------------------
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

#ifdef DEBUG
#include <stdio.h>
#endif
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifdef GEKKO
#include <malloc.h>
#endif

/* SIMD acceleration: SSE2 on x86/x86-64, NEON on ARM */
#if defined(__SSE2__)
#include <emmintrin.h>
#define RPNG_SIMD_SSE2 1
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#if !defined(VITA) && !defined(WEBOS) && !defined(HAVE_LIBNX)
#include <arm_neon.h>
#define RPNG_SIMD_NEON 1
#endif
#endif

#include <boolean.h>
#include <retro_endianness.h>
#include <formats/image.h>
#include <formats/rpng.h>
#include <streams/trans_stream.h>

#include "rpng_internal.h"

enum png_ihdr_color_type
{
   PNG_IHDR_COLOR_GRAY       = 0,
   PNG_IHDR_COLOR_RGB        = 2,
   PNG_IHDR_COLOR_PLT        = 3,
   PNG_IHDR_COLOR_GRAY_ALPHA = 4,
   PNG_IHDR_COLOR_RGBA       = 6
};

enum png_line_filter
{
   PNG_FILTER_NONE = 0,
   PNG_FILTER_SUB,
   PNG_FILTER_UP,
   PNG_FILTER_AVERAGE,
   PNG_FILTER_PAETH
};

enum png_chunk_type
{
   PNG_CHUNK_NOOP = 0,
   PNG_CHUNK_ERROR,
   PNG_CHUNK_IHDR,
   PNG_CHUNK_IDAT,
   PNG_CHUNK_PLTE,
   PNG_CHUNK_tRNS,
   PNG_CHUNK_cICP,
   PNG_CHUNK_cLLI,
   PNG_CHUNK_mDCV,
   PNG_CHUNK_IEND
};

struct adam7_pass
{
   unsigned x;
   unsigned y;
   unsigned stride_x;
   unsigned stride_y;
};

struct idat_buffer
{
   uint8_t *data;
   size_t size;
   size_t capacity;
};

enum rpng_process_flags
{
   RPNG_PROCESS_FLAG_INFLATE_INITIALIZED    = (1 << 0),
   RPNG_PROCESS_FLAG_ADAM7_PASS_INITIALIZED = (1 << 1),
   RPNG_PROCESS_FLAG_PASS_INITIALIZED       = (1 << 2)
};

struct rpng_process
{
   uint32_t *data;
   uint32_t *palette;
   void *stream;
   const struct trans_stream_backend *stream_backend;
   uint8_t *prev_scanline;
   uint8_t *decoded_scanline;
   uint8_t *inflate_buf;
   size_t restore_buf_size;
   size_t adam7_restore_buf_size;
   size_t data_restore_buf_size;
   size_t inflate_buf_size;
   size_t avail_in;
   size_t avail_out;
   size_t total_out;
   size_t pass_size;
   struct png_ihdr ihdr; /* uint32_t alignment */
   unsigned bpp;
   unsigned pitch;
   unsigned h;
   unsigned pass_width;
   unsigned pass_height;
   unsigned pass_pos;
   uint8_t flags;
   bool supports_rgba;
   bool want_10bit;
};

enum rpng_flags
{
   RPNG_FLAG_HAS_IHDR = (1 << 0),
   RPNG_FLAG_HAS_IDAT = (1 << 1),
   RPNG_FLAG_HAS_IEND = (1 << 2),
   RPNG_FLAG_HAS_PLTE = (1 << 3),
   RPNG_FLAG_HAS_TRNS = (1 << 4),
   RPNG_FLAG_HAS_HDR  = (1 << 5),
   RPNG_FLAG_AVAIL_SET = (1 << 6)  /* rpng_set_avail called: frontier
                                      is caller-driven, not whole-buffer */
};

struct rpng
{
   struct rpng_process *process;
   uint8_t *buff_data;
   uint8_t *buff_start;  /* fixed buffer base (buff_data advances)     */
   uint8_t *buff_end;
   /* Resident frontier for prefix decoding: the chunk walk reads only
    * bytes at or before avail_end, even when buff_end (the true file
    * end, known from the file size) lies further ahead.  A chunk that
    * extends past avail_end while more of the file is still to arrive
    * sets need_more instead of ending the walk, so a caller feeding a
    * growing buffer can retry.  Defaults to buff_end (whole buffer
    * resident) when never set. */
   uint8_t *avail_end;
   bool     need_more;   /* last iterate stopped at the resident wall */
   struct idat_buffer idat_buf; /* ptr alignment */
   struct png_ihdr ihdr; /* uint32 alignment */
   uint32_t palette[256];
   /* Populated from cICP / cLLI / mDCV when present (RPNG_FLAG_HAS_HDR). */
   struct rpng_hdr_metadata hdr;
   uint8_t flags;
   bool supports_rgba;
   /* When set and the source is 16-bit, decode to packed XRGB2101010
    * (10-bit) instead of narrowing to 8-bit ARGB, so HDR PNGs can reach a
    * 10-bit display path. Ignored for 8-bit sources. */
   bool want_10bit;
};

static const struct adam7_pass rpng_passes[] = {
   { 0, 0, 8, 8 },
   { 4, 0, 8, 8 },
   { 0, 4, 4, 8 },
   { 2, 0, 4, 4 },
   { 0, 2, 2, 4 },
   { 1, 0, 2, 2 },
   { 0, 1, 1, 2 },
};

static INLINE uint32_t rpng_dword_be(const uint8_t *buf)
{
   return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | (buf[3] << 0);
}

static INLINE uint16_t rpng_word_be(const uint8_t *buf)
{
   return (uint16_t)((buf[0] << 8) | (buf[1] << 0));
}

/* ---------------------------------------------------------------------------
 * SIMD-accelerated PNG filter reconstruction helpers
 * -------------------------------------------------------------------------*/

/* PNG Filter Up: out[i] = raw[i] + prior[i]
 * This is a pure vector add with no data dependency between bytes, making
 * it the most parallelisable of all PNG filters. */
static void rpng_filter_up(uint8_t *out,
      const uint8_t *raw,
      const uint8_t *prior,
      size_t len)
{
#if defined(RPNG_SIMD_SSE2)
   size_t i  = 0;
   size_t n  = len & ~15UL;         /* floor to multiple of 16 */
   for (; i < n; i += 16)
   {
      __m128i r  = _mm_loadu_si128((const __m128i*)(raw   + i));
      __m128i p  = _mm_loadu_si128((const __m128i*)(prior + i));
      _mm_storeu_si128((__m128i*)(out + i), _mm_add_epi8(r, p));
   }
   for (; i < len; i++)
      out[i] = raw[i] + prior[i];
#elif defined(RPNG_SIMD_NEON)
   size_t i  = 0;
   size_t n  = len & ~15UL;
   for (; i < n; i += 16)
   {
      uint8x16_t r  = vld1q_u8(raw   + i);
      uint8x16_t p  = vld1q_u8(prior + i);
      vst1q_u8(out + i, vaddq_u8(r, p));
   }
   for (; i < len; i++)
      out[i] = raw[i] + prior[i];
#else
   size_t i;
   for (i = 0; i < len; i++)
      out[i] = raw[i] + prior[i];
#endif
}

/* --- PNG reverse filter SIMD paths for RGBA (bpp == 4) ------------------
 *
 * The SUB, AVERAGE and PAETH filters all have a per-pixel recurrence:
 *   decoded[i] depends on decoded[i - bpp] from the same scanline.
 *
 * For RGBA (bpp == 4) the recurrence distance equals one SIMD "pixel", so
 * we can process the 4 channels of each pixel in parallel within a single
 * vector register while still respecting the pixel-to-pixel chain.  This
 * eliminates the per-byte branch and scalar dependency chain that costs
 * most in the scalar versions (PAETH especially, where each byte has two
 * unpredictable branches).
 *
 * All three helpers assume:
 *   - bpp == 4, pitch is a multiple of 4 (guaranteed by PNG spec for RGBA)
 *   - prev is a valid prev_scanline pointer (zero-initialised on row 0
 *     by rpng_reverse_filter_init -> calloc)
 *   - raw and decoded may alias (the original code memcpy's raw->decoded
 *     first; we do the filter in-place from raw directly)
 *
 * For bpp != 4 we keep the scalar path.  The payoff would be smaller there
 * (palette/gray images are smaller to begin with) and the SIMD layout is
 * more awkward.
 */
#if defined(RPNG_SIMD_SSE2)

/* Load 4 bytes (one RGBA pixel) and zero-extend each to 16 bits so that
 * the subsequent arithmetic has one byte of headroom (additions, shifts,
 * signed subtraction for Paeth) without overflowing. */
static INLINE __m128i rpng_load4_u8_to_u16(const uint8_t *p)
{
   /* memcpy into a properly-aligned temporary avoids UB from an
    * unaligned dereference of int32_t*.  The scanline buffer
    * (pngp->inflate_buf) is not guaranteed 4-byte aligned at the
    * start of every filter step, so casting directly would be
    * unsafe.  Compilers fold this to a single movd at -O2. */
   int32_t tmp;
   memcpy(&tmp, p, sizeof(tmp));
   return _mm_unpacklo_epi8(_mm_cvtsi32_si128(tmp), _mm_setzero_si128());
}

/* Write the low 4 lanes (bytes) of a 16-bit-lane register back to memory
 * as packed u8.  We mask to 0x00FF before packus so wrap-around (the PNG
 * filter arithmetic is mod 256) is preserved rather than saturated. */
static INLINE void rpng_store4_u16_to_u8(uint8_t *p, __m128i v)
{
   __m128i packed = _mm_packus_epi16(v, _mm_setzero_si128());
   int32_t tmp    = _mm_cvtsi128_si32(packed);
   memcpy(p, &tmp, sizeof(tmp));
}

static void rpng_filter_sub_rgba(uint8_t *decoded,
      const uint8_t *raw, size_t pitch)
{
   size_t i;
   __m128i prev_pixel = _mm_setzero_si128();
   const __m128i mask = _mm_set1_epi16(0x00FF);
   for (i = 0; i + 4 <= pitch; i += 4)
   {
      __m128i r   = rpng_load4_u8_to_u16(raw + i);
      __m128i out = _mm_and_si128(_mm_add_epi16(r, prev_pixel), mask);
      rpng_store4_u16_to_u8(decoded + i, out);
      prev_pixel  = out;
   }
}

static void rpng_filter_avg_rgba(uint8_t *decoded,
      const uint8_t *raw, const uint8_t *prev, size_t pitch)
{
   size_t i;
   __m128i prev_pixel = _mm_setzero_si128();
   const __m128i mask = _mm_set1_epi16(0x00FF);
   for (i = 0; i + 4 <= pitch; i += 4)
   {
      __m128i r   = rpng_load4_u8_to_u16(raw  + i);
      __m128i pv  = rpng_load4_u8_to_u16(prev + i);
      __m128i avg = _mm_srli_epi16(_mm_add_epi16(prev_pixel, pv), 1);
      __m128i out = _mm_and_si128(_mm_add_epi16(r, avg), mask);
      rpng_store4_u16_to_u8(decoded + i, out);
      prev_pixel  = out;
   }
}

/* Branch-free Paeth predictor for 16-bit lanes, following the identity
 *   pa = |b - c|
 *   pb = |a - c|
 *   pc = |(b - c) + (a - c)| = |a + b - 2c|
 * PNG selection rule (in priority order): a if pa <= pb && pa <= pc,
 * else b if pb <= pc, else c. */
static INLINE __m128i rpng_paeth_predictor_epi16(
      __m128i a, __m128i b, __m128i c)
{
   __m128i bc = _mm_sub_epi16(b, c);
   __m128i ac = _mm_sub_epi16(a, c);
   __m128i sm = _mm_add_epi16(bc, ac);
   __m128i z  = _mm_setzero_si128();
   /* SSE2 lacks abs_epi16; max(x, -x) is the standard substitute. */
   __m128i pa = _mm_max_epi16(bc, _mm_sub_epi16(z, bc));
   __m128i pb = _mm_max_epi16(ac, _mm_sub_epi16(z, ac));
   __m128i pc = _mm_max_epi16(sm, _mm_sub_epi16(z, sm));
   /* cmpgt returns 0xFFFF on "greater than" — mask of "don't pick a/b". */
   __m128i not_a  = _mm_or_si128(_mm_cmpgt_epi16(pa, pb),
                                 _mm_cmpgt_epi16(pa, pc));
   __m128i pick_c = _mm_cmpgt_epi16(pb, pc);
   __m128i bc_sel = _mm_or_si128(_mm_andnot_si128(pick_c, b),
                                 _mm_and_si128(   pick_c, c));
   return            _mm_or_si128(_mm_andnot_si128(not_a, a),
                                  _mm_and_si128(   not_a, bc_sel));
}

static void rpng_filter_paeth_rgba(uint8_t *decoded,
      const uint8_t *raw, const uint8_t *prev, size_t pitch)
{
   size_t i;
   __m128i prev_pixel      = _mm_setzero_si128();  /* decoded[i-4] */
   __m128i prev_upper_left = _mm_setzero_si128();  /* prev[i-4]    */
   const __m128i mask      = _mm_set1_epi16(0x00FF);
   for (i = 0; i + 4 <= pitch; i += 4)
   {
      __m128i r    = rpng_load4_u8_to_u16(raw  + i);
      __m128i pv   = rpng_load4_u8_to_u16(prev + i);
      __m128i pred = rpng_paeth_predictor_epi16(
            prev_pixel, pv, prev_upper_left);
      __m128i out  = _mm_and_si128(_mm_add_epi16(r, pred), mask);
      rpng_store4_u16_to_u8(decoded + i, out);
      prev_pixel      = out;
      prev_upper_left = pv;
   }
}

#elif defined(RPNG_SIMD_NEON)

static INLINE uint16x4_t rpng_load4_u8_to_u16(const uint8_t *p)
{
   uint32_t v;
   memcpy(&v, p, 4);
   return vget_low_u16(vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(v))));
}

static INLINE void rpng_store4_u16_to_u8(uint8_t *p, uint16x4_t v)
{
   /* Narrow 4x16 -> 4x8, reinterpret as u32 lane, then memcpy to dst.
    * memcpy handles any alignment and compiles to a single str at -O2. */
   uint8x8_t b    = vmovn_u16(vcombine_u16(v, v));
   uint32_t  word = vget_lane_u32(vreinterpret_u32_u8(b), 0);
   memcpy(p, &word, sizeof(word));
}

static void rpng_filter_sub_rgba(uint8_t *decoded,
      const uint8_t *raw, size_t pitch)
{
   size_t i;
   uint16x4_t prev_pixel = vdup_n_u16(0);
   const uint16x4_t mask = vdup_n_u16(0xFF);
   for (i = 0; i + 4 <= pitch; i += 4)
   {
      uint16x4_t r   = rpng_load4_u8_to_u16(raw + i);
      uint16x4_t out = vand_u16(vadd_u16(r, prev_pixel), mask);
      rpng_store4_u16_to_u8(decoded + i, out);
      prev_pixel     = out;
   }
}

static void rpng_filter_avg_rgba(uint8_t *decoded,
      const uint8_t *raw, const uint8_t *prev, size_t pitch)
{
   size_t i;
   uint16x4_t prev_pixel = vdup_n_u16(0);
   const uint16x4_t mask = vdup_n_u16(0xFF);
   for (i = 0; i + 4 <= pitch; i += 4)
   {
      uint16x4_t r   = rpng_load4_u8_to_u16(raw  + i);
      uint16x4_t pv  = rpng_load4_u8_to_u16(prev + i);
      uint16x4_t avg = vshr_n_u16(vadd_u16(prev_pixel, pv), 1);
      uint16x4_t out = vand_u16(vadd_u16(r, avg), mask);
      rpng_store4_u16_to_u8(decoded + i, out);
      prev_pixel     = out;
   }
}

static INLINE uint16x4_t rpng_paeth_predictor_u16(
      uint16x4_t a, uint16x4_t b, uint16x4_t c)
{
   int16x4_t bc = vsub_s16(vreinterpret_s16_u16(b), vreinterpret_s16_u16(c));
   int16x4_t ac = vsub_s16(vreinterpret_s16_u16(a), vreinterpret_s16_u16(c));
   int16x4_t sm = vadd_s16(bc, ac);
   uint16x4_t pa = vreinterpret_u16_s16(vabs_s16(bc));
   uint16x4_t pb = vreinterpret_u16_s16(vabs_s16(ac));
   uint16x4_t pc = vreinterpret_u16_s16(vabs_s16(sm));
   uint16x4_t not_a  = vorr_u16(vcgt_u16(pa, pb), vcgt_u16(pa, pc));
   uint16x4_t pick_c = vcgt_u16(pb, pc);
   uint16x4_t bc_sel = vbsl_u16(pick_c, c, b);
   return              vbsl_u16(not_a,  bc_sel, a);
}

static void rpng_filter_paeth_rgba(uint8_t *decoded,
      const uint8_t *raw, const uint8_t *prev, size_t pitch)
{
   size_t i;
   uint16x4_t prev_pixel      = vdup_n_u16(0);
   uint16x4_t prev_upper_left = vdup_n_u16(0);
   const uint16x4_t mask      = vdup_n_u16(0xFF);
   for (i = 0; i + 4 <= pitch; i += 4)
   {
      uint16x4_t r    = rpng_load4_u8_to_u16(raw  + i);
      uint16x4_t pv   = rpng_load4_u8_to_u16(prev + i);
      uint16x4_t pred = rpng_paeth_predictor_u16(
            prev_pixel, pv, prev_upper_left);
      uint16x4_t out  = vand_u16(vadd_u16(r, pred), mask);
      rpng_store4_u16_to_u8(decoded + i, out);
      prev_pixel      = out;
      prev_upper_left = pv;
   }
}

#endif /* RPNG_SIMD_SSE2 / RPNG_SIMD_NEON */

/* ---------------------------------------------------------------------------
 * SIMD pixel format conversion helpers
 * -------------------------------------------------------------------------*/

/* Pack 8-bit RGB triples into ARGB32/ABGR32 words (alpha = 0xFF).
 * SSE2 version expands 4 pixels (12 input bytes) per 16-byte load:
 * SSE2 has no byte shuffle (pshufb is SSSE3), but the fixed 3->4 byte
 * expansion falls out of whole-register byte shifts plus dword masks -
 *    w = (v & M0) | (v<<1B & M1) | (v<<2B & M2) | (v<<3B & M3)
 * places triple k at output byte 4k, giving memory order R,G,B after
 * the alpha OR (the ABGR32 supports_rgba layout on LE); the ARGB
 * layout additionally swaps R and B inside each word. The load reads
 * 4 bytes past the 12 consumed, so the vector loop requires at least
 * 6 pixels (18 bytes) of remaining scanline. */
#if defined(RPNG_SIMD_SSE2)
static void rpng_copy_line_rgb_sse2(uint32_t *data,
      const uint8_t *src, unsigned width, bool supports_rgba)
{
   unsigned i = 0;
   const __m128i m0   = _mm_setr_epi32((int)0x00FFFFFF, 0, 0, 0);
   const __m128i m1   = _mm_setr_epi32(0, (int)0x00FFFFFF, 0, 0);
   const __m128i m2   = _mm_setr_epi32(0, 0, (int)0x00FFFFFF, 0);
   const __m128i m3   = _mm_setr_epi32(0, 0, 0, (int)0x00FFFFFF);
   const __m128i ma   = _mm_set1_epi32((int)0xFF000000u);
   const __m128i keep = _mm_set1_epi32((int)0xFF00FF00u);
   const __m128i lowm = _mm_set1_epi32(0xFF);

   for (; (int)(width - i) >= 6; i += 4)
   {
      __m128i v = _mm_loadu_si128((const __m128i*)(src + (size_t)i * 3));
      __m128i w = _mm_or_si128(
            _mm_or_si128(_mm_and_si128(v, m0),
                         _mm_and_si128(_mm_slli_si128(v, 1), m1)),
            _mm_or_si128(_mm_and_si128(_mm_slli_si128(v, 2), m2),
                         _mm_and_si128(_mm_slli_si128(v, 3), m3)));
      if (!supports_rgba)
      {
         /* memory R,G,B -> B,G,R: swap the low and high channel bytes */
         __m128i lo = _mm_slli_epi32(_mm_and_si128(w, lowm), 16);
         __m128i hi = _mm_and_si128(_mm_srli_epi32(w, 16), lowm);
         w = _mm_or_si128(_mm_and_si128(w, keep), _mm_or_si128(lo, hi));
      }
      _mm_storeu_si128((__m128i*)(data + i), _mm_or_si128(w, ma));
   }

   if (supports_rgba)
   {
      for (; i < width; i++)
         data[i] = 0xFF000000u
                 | ((unsigned)src[i*3+2] << 16)
                 | ((unsigned)src[i*3+1] <<  8)
                 | ((unsigned)src[i*3+0]      );
   }
   else
   {
      for (; i < width; i++)
         data[i] = 0xFF000000u
                 | ((unsigned)src[i*3+0] << 16)
                 | ((unsigned)src[i*3+1] <<  8)
                 | ((unsigned)src[i*3+2]      );
   }
}
#endif /* RPNG_SIMD_SSE2 */

/* Pack 8-bit RGBA bytes into ARGB32 or ABGR32 words.
 * Each input pixel is 4 bytes: R G B A
 * ARGB output: (A<<24)|(R<<16)|(G<<8)|B
 * ABGR output: (A<<24)|(B<<16)|(G<<8)|R  (when supports_rgba)
 * On LE (implied by SSE2/x86) the ABGR layout is the input bytes
 * verbatim, so that case is a straight row copy; the ARGB layout is a
 * vectorized R/B swap within each word. */
#if defined(RPNG_SIMD_SSE2)
static void rpng_copy_line_rgba_sse2(uint32_t *data,
      const uint8_t *src, unsigned width, bool supports_rgba)
{
   unsigned i = 0;
   if (supports_rgba)
   {
      memcpy(data, src, (size_t)width * 4);
      return;
   }
   {
      const __m128i keep = _mm_set1_epi32((int)0xFF00FF00u);
      const __m128i lowm = _mm_set1_epi32(0xFF);
      for (; (int)(width - i) >= 4; i += 4)
      {
         __m128i w  = _mm_loadu_si128((const __m128i*)(src + (size_t)i * 4));
         __m128i lo = _mm_slli_epi32(_mm_and_si128(w, lowm), 16);
         __m128i hi = _mm_and_si128(_mm_srli_epi32(w, 16), lowm);
         _mm_storeu_si128((__m128i*)(data + i),
               _mm_or_si128(_mm_and_si128(w, keep), _mm_or_si128(lo, hi)));
      }
   }
   for (; i < width; i++)
      data[i] = ((unsigned)src[i*4+3] << 24) | ((unsigned)src[i*4+0] << 16)
              | ((unsigned)src[i*4+1] <<  8) | ((unsigned)src[i*4+2]);
}
#endif /* RPNG_SIMD_SSE2 */

/* NEON RGBA → ARGB32/ABGR32 conversion: vld4_u8 de-interleaves all 4 channels. */
#if defined(RPNG_SIMD_NEON)
static void rpng_copy_line_rgba_neon(uint32_t *data,
      const uint8_t *src, unsigned width, bool supports_rgba)
{
   unsigned i = 0;
   for (; (int)(width - i) >= 8; i += 8)
   {
      uint8x8x4_t px  = vld4_u8(src + i * 4); /* de-interleave R,G,B,A */
      /* When supports_rgba, swap r and b to produce ABGR instead of ARGB */
      uint8x8_t   hi  = supports_rgba ? px.val[2] : px.val[0]; /* R or B → byte 2 */
      uint8x8_t   g   = px.val[1];
      uint8x8_t   lo  = supports_rgba ? px.val[0] : px.val[2]; /* B or R → byte 0 */
      uint8x8_t   a   = px.val[3];
      uint32x4_t lo_a  = vshlq_n_u32(vmovl_u16(vget_low_u16(vmovl_u8(a))),  24);
      uint32x4_t lo_hi = vshll_n_u16(vget_low_u16(vmovl_u8(hi)), 16);
      uint32x4_t lo_g  = vshll_n_u16(vget_low_u16(vmovl_u8(g)),   8);
      uint32x4_t lo_lo = vmovl_u16(vget_low_u16(vmovl_u8(lo)));
      uint32x4_t lo_px = vorrq_u32(vorrq_u32(lo_a, lo_hi), vorrq_u32(lo_g, lo_lo));
      uint32x4_t hi_a  = vshlq_n_u32(vmovl_u16(vget_high_u16(vmovl_u8(a))), 24);
      uint32x4_t hi_hi = vshll_n_u16(vget_high_u16(vmovl_u8(hi)), 16);
      uint32x4_t hi_g  = vshll_n_u16(vget_high_u16(vmovl_u8(g)),  8);
      uint32x4_t hi_lo = vmovl_u16(vget_high_u16(vmovl_u8(lo)));
      uint32x4_t hi_px = vorrq_u32(vorrq_u32(hi_a, hi_hi), vorrq_u32(hi_g, hi_lo));
      vst1q_u32(data + i,     lo_px);
      vst1q_u32(data + i + 4, hi_px);
   }
   if (supports_rgba)
   {
      for (; i < width; i++)
         data[i] = ((unsigned)src[i*4+3] << 24) | ((unsigned)src[i*4+2] << 16)
                 | ((unsigned)src[i*4+1] <<  8) | ((unsigned)src[i*4+0]);
   }
   else
   {
      for (; i < width; i++)
         data[i] = ((unsigned)src[i*4+3] << 24) | ((unsigned)src[i*4+0] << 16)
                 | ((unsigned)src[i*4+1] <<  8) | ((unsigned)src[i*4+2]);
   }
}

/* NEON RGB → ARGB32/ABGR32 conversion using vld3 de-interleave */
static void rpng_copy_line_rgb_neon(uint32_t *data,
      const uint8_t *src, unsigned width, bool supports_rgba)
{
   unsigned i = 0;
   for (; (int)(width - i) >= 8; i += 8)
   {
      uint8x8x3_t px  = vld3_u8(src + i * 3);
      uint8x8_t   hi  = supports_rgba ? px.val[2] : px.val[0];
      uint8x8_t   g   = px.val[1];
      uint8x8_t   lo  = supports_rgba ? px.val[0] : px.val[2];
      uint32x4_t lo_hi_v = vshll_n_u16(vget_low_u16(vmovl_u8(hi)),  16);
      uint32x4_t lo_g    = vshll_n_u16(vget_low_u16(vmovl_u8(g)),    8);
      uint32x4_t lo_lo_v = vmovl_u16(vget_low_u16(vmovl_u8(lo)));
      uint32x4_t lo_a    = vdupq_n_u32(0xFF000000u);
      uint32x4_t lo_px   = vorrq_u32(vorrq_u32(lo_a, lo_hi_v), vorrq_u32(lo_g, lo_lo_v));
      uint32x4_t hi_hi_v = vshll_n_u16(vget_high_u16(vmovl_u8(hi)), 16);
      uint32x4_t hi_g    = vshll_n_u16(vget_high_u16(vmovl_u8(g)),   8);
      uint32x4_t hi_lo_v = vmovl_u16(vget_high_u16(vmovl_u8(lo)));
      uint32x4_t hi_a    = vdupq_n_u32(0xFF000000u);
      uint32x4_t hi_px   = vorrq_u32(vorrq_u32(hi_a, hi_hi_v), vorrq_u32(hi_g, hi_lo_v));
      vst1q_u32(data + i,     lo_px);
      vst1q_u32(data + i + 4, hi_px);
   }
   if (supports_rgba)
   {
      for (; i < width; i++)
         data[i] = 0xFF000000u
                 | ((unsigned)src[i*3+2] << 16)
                 | ((unsigned)src[i*3+1] <<  8)
                 | ((unsigned)src[i*3+0]      );
   }
   else
   {
      for (; i < width; i++)
         data[i] = 0xFF000000u
                 | ((unsigned)src[i*3+0] << 16)
                 | ((unsigned)src[i*3+1] <<  8)
                 | ((unsigned)src[i*3+2]      );
   }
}
#endif /* RPNG_SIMD_NEON */

#if defined(DEBUG) || defined(RPNG_TEST)
#include <stdio.h>

static bool rpng_process_ihdr(struct png_ihdr *ihdr)
{
   uint8_t ihdr_depth = ihdr->depth;

   switch (ihdr->color_type)
   {
      case PNG_IHDR_COLOR_RGB:
      case PNG_IHDR_COLOR_GRAY_ALPHA:
      case PNG_IHDR_COLOR_RGBA:
         if (ihdr_depth != 8 && ihdr_depth != 16)
         {
            fprintf(stderr, "[RPNG] Error in line %d.\n", __LINE__);
            return false;
         }
         break;
      case PNG_IHDR_COLOR_GRAY:
         /* Valid bitdepths are: 1, 2, 4, 8, 16 */
         if (ihdr_depth > 16 || (0x977F7FFF << ihdr_depth) & 0x80000000)
         {
            fprintf(stderr, "[RPNG] Error in line %d.\n", __LINE__);
            return false;
         }
         break;
      case PNG_IHDR_COLOR_PLT:
         /* Valid bitdepths are: 1, 2, 4, 8 */
         if (ihdr_depth > 8 || (0x977F7FFF << ihdr_depth)  & 0x80000000)
         {
            fprintf(stderr, "[RPNG] Error in line %d.\n", __LINE__);
            return false;
         }
         break;
      default:
         fprintf(stderr, "[RPNG] Error in line %d.\n", __LINE__);
         return false;
   }

   /* On 32-bit hosts the per-row decode mallocs cannot fit much
    * more than 1 GiB of decoded RGBA, and an undersized malloc
    * combined with attacker-controlled dimensions has historically
    * been the heap-overflow primitive prompting the 0x4000 caps
    * in rbmp.c, rtga.c and rwebp.c.  Keep the tight cap there.
    *
    * On 64-bit the (size_t) casts in rpng_reverse_filter_init and
    * the final allocator make the per-row arithmetic overflow-safe
    * regardless of dimensions, and the 4 GiB output guard further
    * down in rpng_iterate_image already rejects images whose
    * decoded buffer cannot be addressed.  Loading a 30000x30000
    * RGBA image on a desktop with the RAM to spare is a legitimate
    * use case (cf. IrfanView), so do not impose the 0x4000 cap
    * there. */
#if SIZE_MAX <= 0xFFFFFFFFu
   if (ihdr->width > 0x4000u || ihdr->height > 0x4000u)
   {
      fprintf(stderr, "[RPNG] Error in line %d.\n", __LINE__);
      return false;
   }
#endif

#ifdef RPNG_TEST
   fprintf(stderr, "IHDR: (%u x %u), bpc = %u, palette = %s, color = %s, alpha = %s, adam7 = %s.\n",
         ihdr->width, ihdr->height,
         ihdr_depth, (ihdr->color_type == PNG_IHDR_COLOR_PLT) ? "yes" : "no",
         (ihdr->color_type & PNG_IHDR_COLOR_RGB)              ? "yes" : "no",
         (ihdr->color_type & PNG_IHDR_COLOR_GRAY_ALPHA)       ? "yes" : "no",
         ihdr->interlace == 1 ? "yes" : "no");
#endif

   return true;
}
#else
static bool rpng_process_ihdr(struct png_ihdr *ihdr)
{
   uint8_t ihdr_depth = ihdr->depth;

   switch (ihdr->color_type)
   {
      case PNG_IHDR_COLOR_RGB:
      case PNG_IHDR_COLOR_GRAY_ALPHA:
      case PNG_IHDR_COLOR_RGBA:
         if (ihdr_depth != 8 && ihdr_depth != 16)
            return false;
         break;
      case PNG_IHDR_COLOR_GRAY:
         /* Valid bitdepths are: 1, 2, 4, 8, 16 */
         if (ihdr_depth > 16 || (0x977F7FFF << ihdr_depth) & 0x80000000)
            return false;
         break;
      case PNG_IHDR_COLOR_PLT:
         /* Valid bitdepths are: 1, 2, 4, 8 */
         if (ihdr_depth > 8 || (0x977F7FFF << ihdr_depth)  & 0x80000000)
            return false;
         break;
      default:
         return false;
   }

   /* See the matching comment in the RPNG_TEST/DEBUG variant
    * above.  Cap only on 32-bit; 64-bit lets the (size_t)
    * widening + 4 GiB output guard handle large legitimate
    * images. */
#if SIZE_MAX <= 0xFFFFFFFFu
   if (ihdr->width > 0x4000u || ihdr->height > 0x4000u)
      return false;
#endif

   return true;
}
#endif

static void rpng_reverse_filter_copy_line_rgb(uint32_t *data,
      const uint8_t *decoded, unsigned width, unsigned bpp,
      bool supports_rgba, bool want_10bit)
{
   int i;

   /* bpp here is ihdr->depth: bits per SAMPLE (8 or 16), not bits per
    * pixel - the scalar loop below strides bpp/8 bytes per channel.
    * Fast path for 8-bit depth: each pixel is exactly 3 bytes. */
   if (bpp == 8)
   {
#if defined(RPNG_SIMD_NEON)
      rpng_copy_line_rgb_neon(data, decoded, width, supports_rgba);
      return;
#elif defined(RPNG_SIMD_SSE2)
      rpng_copy_line_rgb_sse2(data, decoded, width, supports_rgba);
      return;
#endif
   }

   /* 16-bit source requested as 10-bit output: pack XRGB2101010
    * (R in bits [29:20], G [19:10], B [9:0]) from the full 16-bit samples,
    * scaled 16->10 bit by >> 6. Independent of supports_rgba: the packed
    * layout is a fixed R-high ordering the 10-bit upload paths expect. */
   if (want_10bit && bpp == 16)
   {
      for (i = 0; i < (int)width; i++, decoded += 6)
      {
         uint32_t r = (((uint32_t)decoded[0] << 8) | decoded[1]) >> 6;
         uint32_t g = (((uint32_t)decoded[2] << 8) | decoded[3]) >> 6;
         uint32_t b = (((uint32_t)decoded[4] << 8) | decoded[5]) >> 6;
         /* Top 2 bits = alpha 3 (opaque), matching the video 10-bit blit;
          * A2R10G10B10_UNORM samples these as alpha, so leaving them 0 would
          * render the image fully transparent. */
         data[i]    = (r << 20) | (g << 10) | b | 0xC0000000u;
      }
      return;
   }

   bpp /= 8;

   if (supports_rgba)
   {
      for (i = 0; i < (int)width; i++)
      {
         uint32_t r, g, b;
         r        = *decoded;
         decoded += bpp;
         g        = *decoded;
         decoded += bpp;
         b        = *decoded;
         decoded += bpp;
         data[i]  = (0xffu << 24) | (b << 16) | (g << 8) | (r << 0);
      }
   }
   else
   {
      for (i = 0; i < (int)width; i++)
      {
         uint32_t r, g, b;
         r        = *decoded;
         decoded += bpp;
         g        = *decoded;
         decoded += bpp;
         b        = *decoded;
         decoded += bpp;
         data[i]  = (0xffu << 24) | (r << 16) | (g << 8) | (b << 0);
      }
   }
}

static void rpng_reverse_filter_copy_line_rgba(uint32_t *data,
      const uint8_t *decoded, unsigned width, unsigned bpp,
      bool supports_rgba)
{
   int i;

   /* bpp here is ihdr->depth: bits per SAMPLE (8 or 16), not bits per
    * pixel - the scalar loop below strides bpp/8 bytes per channel.
    * Fast paths for 8-bit depth: each pixel is exactly 4 bytes. */
   if (bpp == 8)
   {
#if !defined(MSB_FIRST)
      /* The unfiltered scanline bytes are already R,G,B,A in memory
       * order, which on a little-endian host is exactly the ABGR32
       * word layout the supports_rgba output wants: the conversion is
       * the identity, so copy the row wholesale. */
      if (supports_rgba)
      {
         memcpy(data, decoded, (size_t)width * 4);
         return;
      }
#endif
#if defined(RPNG_SIMD_NEON)
      rpng_copy_line_rgba_neon(data, decoded, width, supports_rgba);
      return;
#elif defined(RPNG_SIMD_SSE2)
      rpng_copy_line_rgba_sse2(data, decoded, width, supports_rgba);
      return;
#endif
   }

   bpp /= 8;

   if (supports_rgba)
   {
      for (i = 0; i < (int)width; i++)
      {
         uint32_t r, g, b, a;
         r        = *decoded;
         decoded += bpp;
         g        = *decoded;
         decoded += bpp;
         b        = *decoded;
         decoded += bpp;
         a        = *decoded;
         decoded += bpp;
         data[i]  = (a << 24) | (b << 16) | (g << 8) | (r << 0);
      }
   }
   else
   {
      for (i = 0; i < (int)width; i++)
      {
         uint32_t r, g, b, a;
         r        = *decoded;
         decoded += bpp;
         g        = *decoded;
         decoded += bpp;
         b        = *decoded;
         decoded += bpp;
         a        = *decoded;
         decoded += bpp;
         data[i]  = (a << 24) | (r << 16) | (g << 8) | (b << 0);
      }
   }
}

static void rpng_reverse_filter_copy_line_bw(uint32_t *data,
      const uint8_t *decoded, unsigned width, unsigned depth)
{
   int i;
   unsigned bit;
   static const unsigned mul_table[] = { 0, 0xff, 0x55, 0, 0x11, 0, 0, 0, 0x01 };
   unsigned mul, mask;

   if (depth == 16)
   {
      for (i = 0; i < (int)width; i++)
      {
         uint32_t val = decoded[i << 1];
         data[i]      = (val * 0x010101) | (0xffu << 24);
      }
      return;
   }

   mul  = mul_table[depth];
   mask = (1 << depth) - 1;
   bit  = 0;

   for (i = 0; i < (int)width; i++, bit += depth)
   {
      unsigned byte = bit >> 3;
      unsigned val  = decoded[byte] >> (8 - depth - (bit & 7));

      val          &= mask;
      val          *= mul;
      data[i]       = (val * 0x010101) | (0xffu << 24);
   }
}

static void rpng_reverse_filter_copy_line_gray_alpha(uint32_t *data,
      const uint8_t *decoded, unsigned width,
      unsigned bpp)
{
   int i;

   bpp /= 8;

   for (i = 0; i < (int)width; i++)
   {
      uint32_t gray, alpha;

      gray     = *decoded;
      decoded += bpp;
      alpha    = *decoded;
      decoded += bpp;

      data[i]  = (gray * 0x010101) | (alpha << 24);
   }
}

static void rpng_reverse_filter_copy_line_plt(uint32_t *data,
      const uint8_t *decoded, unsigned width,
      unsigned depth, const uint32_t *palette)
{
   switch (depth)
   {
      case 1:
         {
            int i;
            unsigned w = width / 8;
            for (i = 0; i < (int)w; i++, decoded++)
            {
               *data++ = palette[(*decoded >> 7) & 1];
               *data++ = palette[(*decoded >> 6) & 1];
               *data++ = palette[(*decoded >> 5) & 1];
               *data++ = palette[(*decoded >> 4) & 1];
               *data++ = palette[(*decoded >> 3) & 1];
               *data++ = palette[(*decoded >> 2) & 1];
               *data++ = palette[(*decoded >> 1) & 1];
               *data++ = palette[*decoded & 1];
            }

            switch (width & 7)
            {
               case 7:
                  data[6] = palette[(*decoded >> 1) & 1];
                  /* fall-through */
               case 6:
                  data[5] = palette[(*decoded >> 2) & 1];
                  /* fall-through */
               case 5:
                  data[4] = palette[(*decoded >> 3) & 1];
                  /* fall-through */
               case 4:
                  data[3] = palette[(*decoded >> 4) & 1];
                  /* fall-through */
               case 3:
                  data[2] = palette[(*decoded >> 5) & 1];
                  /* fall-through */
               case 2:
                  data[1] = palette[(*decoded >> 6) & 1];
                  /* fall-through */
               case 1:
                  data[0] = palette[(*decoded >> 7) & 1];
                  break;
            }
         }
         break;

      case 2:
         {
            int i;
            unsigned w = width / 4;
            for (i = 0; i < (int)w; i++, decoded++)
            {
               *data++ = palette[(*decoded >> 6) & 3];
               *data++ = palette[(*decoded >> 4) & 3];
               *data++ = palette[(*decoded >> 2) & 3];
               *data++ = palette[*decoded & 3];
            }

            switch (width & 3)
            {
               case 3:
                  data[2] = palette[(*decoded >> 2) & 3];
                  /* fall-through */
               case 2:
                  data[1] = palette[(*decoded >> 4) & 3];
                  /* fall-through */
               case 1:
                  data[0] = palette[(*decoded >> 6) & 3];
                  break;
            }
         }
         break;

      case 4:
         {
            int i;
            unsigned w = width / 2;
            for (i = 0; i < (int)w; i++, decoded++)
            {
               *data++ = palette[*decoded >> 4];
               *data++ = palette[*decoded & 0x0f];
            }

            if (width & 1)
               *data = palette[*decoded >> 4];
         }
         break;

      case 8:
         {
            int i;
            for (i = 0; i < (int)width; i++, decoded++, data++)
               *data = palette[*decoded];
         }
         break;
   }
}

static void rpng_pass_geom(const struct png_ihdr *ihdr,
      unsigned width, unsigned height,
      unsigned *bpp_out, unsigned *pitch_out, size_t *pass_size)
{
   /* Perform pitch and pass_size arithmetic in size_t.  Previously these
    * were done in unsigned int, which can silently wrap on 32-bit at a
    * width of ~67M for 16bpc RGBA (pitch = width*8) or, more plausibly,
    * at a pitch*height product exceeding ~4 GiB — reachable today with
    * a 30000x30000 16bpc-RGBA image that passes the IHDR output-size
    * cap (based on the RGBA-8 output buffer) but whose 16bpc intermediate
    * scanline buffer is ~6.7 GiB.  A wrapped pass_size underallocates the
    * inflate buffer and exposes a heap overflow during decode.
    *
    * The `(size_t)ihdr->width * ihdr->depth` leading term forces the
    * whole expression to size_t width.  Callers with `unsigned *pitch_out`
    * still receive a narrowed value — safe on 64-bit where size_t is 64
    * bits, since realistic pitches fit comfortably.  On 32-bit targets
    * pitch_out itself has no headroom beyond UINT32_MAX, but the caller
    * won't reach any allocation using pitch if the IHDR check further
    * down rejects such images for their overall size. */
   size_t   bpp   = 0;
   size_t   pitch = 0;

   switch (ihdr->color_type)
   {
      case PNG_IHDR_COLOR_GRAY:
         bpp   = ((size_t)ihdr->depth + 7) / 8;
         pitch = ((size_t)ihdr->width * ihdr->depth + 7) / 8;
         break;
      case PNG_IHDR_COLOR_RGB:
         bpp   = ((size_t)ihdr->depth * 3 + 7) / 8;
         pitch = ((size_t)ihdr->width * ihdr->depth * 3 + 7) / 8;
         break;
      case PNG_IHDR_COLOR_PLT:
         bpp   = ((size_t)ihdr->depth + 7) / 8;
         pitch = ((size_t)ihdr->width * ihdr->depth + 7) / 8;
         break;
      case PNG_IHDR_COLOR_GRAY_ALPHA:
         bpp   = ((size_t)ihdr->depth * 2 + 7) / 8;
         pitch = ((size_t)ihdr->width * ihdr->depth * 2 + 7) / 8;
         break;
      case PNG_IHDR_COLOR_RGBA:
         bpp   = ((size_t)ihdr->depth * 4 + 7) / 8;
         pitch = ((size_t)ihdr->width * ihdr->depth * 4 + 7) / 8;
         break;
      default:
         break;
   }

   if (pass_size)
      *pass_size = (pitch + 1) * (size_t)ihdr->height;
   if (bpp_out)
      *bpp_out   = (unsigned)bpp;
   if (pitch_out)
      *pitch_out = (unsigned)pitch;
}

static void rpng_reverse_filter_adam7_deinterlace_pass(uint32_t *data,
      const struct png_ihdr *ihdr,
      const uint32_t *input, unsigned pass_width, unsigned pass_height,
      const struct adam7_pass *pass)
{
   unsigned x, y;

   data += pass->y * ihdr->width + pass->x;

   for (y = 0; y < pass_height;
         y++, data += ihdr->width * pass->stride_y, input += pass_width)
   {
      uint32_t *out = data;

      for (x = 0; x < pass_width; x++, out += pass->stride_x)
         *out = input[x];
   }
}

static void rpng_reverse_filter_deinit(struct rpng_process *pngp)
{
   if (!pngp)
      return;
   if (pngp->decoded_scanline)
      free(pngp->decoded_scanline);
   pngp->decoded_scanline = NULL;
   if (pngp->prev_scanline)
      free(pngp->prev_scanline);
   pngp->prev_scanline    = NULL;

   pngp->flags           &= ~RPNG_PROCESS_FLAG_PASS_INITIALIZED;
   pngp->h                = 0;
}

static int rpng_reverse_filter_init(const struct png_ihdr *ihdr,
      struct rpng_process *pngp)
{
   size_t pass_size;

   if (   !(pngp->flags & RPNG_PROCESS_FLAG_ADAM7_PASS_INITIALIZED)
         && ihdr->interlace)
   {
      if (     ihdr->width  <= rpng_passes[pngp->pass_pos].x
            || ihdr->height <= rpng_passes[pngp->pass_pos].y) /* Empty pass */
         return 1;

      pngp->pass_width  = (ihdr->width -
            rpng_passes[pngp->pass_pos].x + rpng_passes[pngp->pass_pos].stride_x
- 1) / rpng_passes[pngp->pass_pos].stride_x;
      pngp->pass_height = (ihdr->height - rpng_passes[pngp->pass_pos].y +
            rpng_passes[pngp->pass_pos].stride_y - 1) / rpng_passes[pngp->pass_pos].stride_y;

      if (!(pngp->data = (uint32_t*)malloc(
            (size_t)pngp->pass_width * (size_t)pngp->pass_height * sizeof(uint32_t))))
         return -1;

      pngp->ihdr        = *ihdr;
      pngp->ihdr.width  = pngp->pass_width;
      pngp->ihdr.height = pngp->pass_height;

      rpng_pass_geom(&pngp->ihdr, pngp->pass_width,
            pngp->pass_height, NULL, NULL, &pngp->pass_size);

      if (pngp->pass_size > pngp->total_out)
      {
         free(pngp->data);
         pngp->data = NULL;
         return -1;
      }

      pngp->flags |= RPNG_PROCESS_FLAG_ADAM7_PASS_INITIALIZED;

      return 0;
   }

   if (pngp->flags & RPNG_PROCESS_FLAG_PASS_INITIALIZED)
      return 0;

   rpng_pass_geom(ihdr, ihdr->width, ihdr->height, &pngp->bpp, &pngp->pitch, &pass_size);

   if (pngp->total_out < pass_size)
      return -1;

   pngp->restore_buf_size      = 0;
   pngp->data_restore_buf_size = 0;
   pngp->prev_scanline         = (uint8_t*)calloc(1, pngp->pitch);
   pngp->decoded_scanline      = (uint8_t*)calloc(1, pngp->pitch);

   if (!pngp->prev_scanline || !pngp->decoded_scanline)
      goto error;

   pngp->h                    = 0;
   pngp->flags               |= RPNG_PROCESS_FLAG_PASS_INITIALIZED;

   return 0;

error:
   rpng_reverse_filter_deinit(pngp);
   return -1;
}

/* ---------------------------------------------------------------------------*/

static int rpng_reverse_filter_copy_line(uint32_t *data,
      const struct png_ihdr *ihdr,
      struct rpng_process *pngp, unsigned filter)
{
   unsigned i;

   switch (filter)
   {
      case PNG_FILTER_NONE:
         memcpy(pngp->decoded_scanline, pngp->inflate_buf, pngp->pitch);
         break;
      case PNG_FILTER_SUB:
#if defined(RPNG_SIMD_SSE2) || defined(RPNG_SIMD_NEON)
         if (pngp->bpp == 4)
         {
            rpng_filter_sub_rgba(pngp->decoded_scanline,
                  pngp->inflate_buf, pngp->pitch);
            break;
         }
#endif
         memcpy(pngp->decoded_scanline, pngp->inflate_buf, pngp->pitch);
         for (i = pngp->bpp; i < pngp->pitch; i++)
            pngp->decoded_scanline[i] += pngp->decoded_scanline[i - pngp->bpp];
         break;
      case PNG_FILTER_UP:
         /* Filter Up is a pure vector add—no inter-byte dependency. */
         rpng_filter_up(pngp->decoded_scanline,
               pngp->inflate_buf, pngp->prev_scanline, pngp->pitch);
         break;
      case PNG_FILTER_AVERAGE:
#if defined(RPNG_SIMD_SSE2) || defined(RPNG_SIMD_NEON)
         if (pngp->bpp == 4)
         {
            rpng_filter_avg_rgba(pngp->decoded_scanline,
                  pngp->inflate_buf, pngp->prev_scanline, pngp->pitch);
            break;
         }
#endif
         memcpy(pngp->decoded_scanline, pngp->inflate_buf, pngp->pitch);
         for (i = 0; i < pngp->bpp; i++)
         {
            uint8_t avg = pngp->prev_scanline[i] >> 1;
            pngp->decoded_scanline[i] += avg;
         }
         for (i = pngp->bpp; i < pngp->pitch; i++)
         {
            uint8_t avg = (pngp->decoded_scanline[i - pngp->bpp] + pngp->prev_scanline[i]) >> 1;
            pngp->decoded_scanline[i] += avg;
         }
         break;
      case PNG_FILTER_PAETH:
#if defined(RPNG_SIMD_SSE2) || defined(RPNG_SIMD_NEON)
         if (pngp->bpp == 4)
         {
            rpng_filter_paeth_rgba(pngp->decoded_scanline,
                  pngp->inflate_buf, pngp->prev_scanline, pngp->pitch);
            break;
         }
#endif
         memcpy(pngp->decoded_scanline, pngp->inflate_buf, pngp->pitch);
         for (i = 0; i < pngp->bpp; i++)
            pngp->decoded_scanline[i] += pngp->prev_scanline[i];
         for (i = pngp->bpp; i < pngp->pitch; i++)
            pngp->decoded_scanline[i] += paeth(
                  pngp->decoded_scanline[i - pngp->bpp],
                  pngp->prev_scanline[i],
                  pngp->prev_scanline[i - pngp->bpp]);
         break;
      default:
         return IMAGE_PROCESS_ERROR_END;
   }

   switch (ihdr->color_type)
   {
      case PNG_IHDR_COLOR_GRAY:
         rpng_reverse_filter_copy_line_bw(data, pngp->decoded_scanline, ihdr->width, ihdr->depth);
         break;
      case PNG_IHDR_COLOR_RGB:
         rpng_reverse_filter_copy_line_rgb(data, pngp->decoded_scanline, ihdr->width, ihdr->depth,
               pngp->supports_rgba, pngp->want_10bit);
         break;
      case PNG_IHDR_COLOR_PLT:
         rpng_reverse_filter_copy_line_plt(
               data, pngp->decoded_scanline, ihdr->width,
               ihdr->depth, pngp->palette);
         break;
      case PNG_IHDR_COLOR_GRAY_ALPHA:
         rpng_reverse_filter_copy_line_gray_alpha(
               data, pngp->decoded_scanline, ihdr->width,
               ihdr->depth);
         break;
      case PNG_IHDR_COLOR_RGBA:
         rpng_reverse_filter_copy_line_rgba(
               data, pngp->decoded_scanline, ihdr->width, ihdr->depth,
               pngp->supports_rgba);
         break;
   }

   /* Swap scanline pointers instead of copying — the current decoded
    * scanline becomes the previous scanline for the next row.
    * Both buffers are the same size (pitch bytes), allocated in
    * rpng_reverse_filter_init, so swapping is always safe. */
   {
      uint8_t *tmp           = pngp->prev_scanline;
      pngp->prev_scanline    = pngp->decoded_scanline;
      pngp->decoded_scanline = tmp;
   }

   return IMAGE_PROCESS_NEXT;
}

static int rpng_reverse_filter_regular_iterate(
      uint32_t **data, const struct png_ihdr *ihdr,
      struct rpng_process *pngp)
{
   int ret = IMAGE_PROCESS_END;
   if (pngp->h < ihdr->height)
   {
      unsigned filter         = *pngp->inflate_buf++;
      pngp->restore_buf_size += 1;
      ret                     = rpng_reverse_filter_copy_line(*data,
            ihdr, pngp, filter);
      if (ret == IMAGE_PROCESS_END || ret == IMAGE_PROCESS_ERROR_END)
         goto end;
   }
   else
      goto end;

   pngp->h++;
   pngp->inflate_buf           += pngp->pitch;
   pngp->restore_buf_size      += pngp->pitch;

   *data                       += ihdr->width;
   pngp->data_restore_buf_size += ihdr->width;

   return IMAGE_PROCESS_NEXT;

end:
   rpng_reverse_filter_deinit(pngp);

   pngp->inflate_buf -= pngp->restore_buf_size;
   *data             -= pngp->data_restore_buf_size;
   pngp->data_restore_buf_size = 0;
   return ret;
}

static int rpng_reverse_filter_adam7_iterate(uint32_t **data_,
      const struct png_ihdr *ihdr,
      struct rpng_process *pngp)
{
   int        ret = 0;
   bool   to_next = pngp->pass_pos < ARRAY_SIZE(rpng_passes);
   uint32_t *data = *data_;

   if (!to_next)
      return IMAGE_PROCESS_END;

   if ((ret = rpng_reverse_filter_init(ihdr, pngp)) == 1)
      return IMAGE_PROCESS_NEXT;
   else if (ret == -1)
      return IMAGE_PROCESS_ERROR_END;

   if (rpng_reverse_filter_init(&pngp->ihdr, pngp) == -1)
      return IMAGE_PROCESS_ERROR;

   do
   {
      ret = rpng_reverse_filter_regular_iterate(&pngp->data,
            &pngp->ihdr, pngp);
   } while (ret == IMAGE_PROCESS_NEXT);

   if (ret == IMAGE_PROCESS_ERROR || ret == IMAGE_PROCESS_ERROR_END)
      return IMAGE_PROCESS_ERROR;

   pngp->inflate_buf            += pngp->pass_size;
   pngp->adam7_restore_buf_size += pngp->pass_size;

   pngp->total_out              -= pngp->pass_size;

   rpng_reverse_filter_adam7_deinterlace_pass(data,
         ihdr, pngp->data, pngp->pass_width, pngp->pass_height,
         &rpng_passes[pngp->pass_pos]);

   free(pngp->data);

   pngp->data                   = NULL;
   pngp->pass_width             = 0;
   pngp->pass_height            = 0;
   pngp->pass_size              = 0;
   pngp->flags                 &= ~RPNG_PROCESS_FLAG_ADAM7_PASS_INITIALIZED;

   return IMAGE_PROCESS_NEXT;
}

static int rpng_reverse_filter_adam7(uint32_t **data_,
      const struct png_ihdr *ihdr,
      struct rpng_process *pngp)
{
   int ret = rpng_reverse_filter_adam7_iterate(data_,
         ihdr, pngp);

   switch (ret)
   {
      case IMAGE_PROCESS_ERROR_END:
      case IMAGE_PROCESS_END:
         break;
      case IMAGE_PROCESS_NEXT:
         pngp->pass_pos++;
         return 0;
      case IMAGE_PROCESS_ERROR:
         if (pngp->data)
         {
            free(pngp->data);
            pngp->data = NULL;
         }
         pngp->inflate_buf -= pngp->adam7_restore_buf_size;
         pngp->adam7_restore_buf_size = 0;
         return -1;
   }

   pngp->inflate_buf            -= pngp->adam7_restore_buf_size;
   pngp->adam7_restore_buf_size  = 0;
   return ret;
}

static int rpng_load_image_argb_process_inflate_init(
      rpng_t *rpng, uint32_t **data)
{
   bool zstatus;
   enum trans_stream_error err;
   uint32_t rd, wn;
   struct rpng_process *process = (struct rpng_process*)rpng->process;
   bool to_continue             = (process->avail_in  > 0
                                && process->avail_out > 0);

   if (!to_continue)
      goto end;

   zstatus = process->stream_backend->trans(
      process->stream, false, &rd, &wn, &err);

   if (!zstatus && err != TRANS_STREAM_ERROR_BUFFER_FULL)
      goto error;

   process->avail_in -= rd;
   process->avail_out -= wn;
   process->total_out += wn;

   if (err)
      return 0;

end:
   process->stream_backend->stream_free(process->stream);
   process->stream = NULL;

#ifdef GEKKO
   /* We often use these in textures, make sure 
    * they're 32-byte aligned */
   *data = (uint32_t*)memalign(32, (size_t)rpng->ihdr.width *
         (size_t)rpng->ihdr.height * sizeof(uint32_t));
#else
   *data = (uint32_t*)malloc((size_t)rpng->ihdr.width *
         (size_t)rpng->ihdr.height * sizeof(uint32_t));
#endif
   if (!*data)
      goto false_end;

   process->adam7_restore_buf_size = 0;
   process->restore_buf_size       = 0;
   process->palette                = rpng->palette;

   if (rpng->ihdr.interlace != 1)
      if (rpng_reverse_filter_init(&rpng->ihdr, process) == -1)
         goto false_end;

   process->flags |=  RPNG_PROCESS_FLAG_INFLATE_INITIALIZED;
   return 1;

error:
false_end:
   process->flags &= ~RPNG_PROCESS_FLAG_INFLATE_INITIALIZED;
   return -1;
}

/* Ceiling on the accumulated IDAT stream.  The PNG specification sets
 * no limit here: IDAT may repeat without bound and the accumulated
 * compressed stream can legitimately be very large, so this is not a
 * policy number - a 320 MiB screenshot or scan is a real file, not a
 * hostile one.  What the arithmetic below genuinely needs is a value
 * to subtract from so the running total, the per-chunk addition and
 * the capacity doubling cannot overflow size_t.  Use the largest
 * quantity that can actually be addressed: SIZE_MAX/2 leaves the
 * doubling loop headroom (new_cap *= 2 stays representable) and still
 * rejects only what malloc could never satisfy.
 *
 * The decompression-bomb concern the old, much lower cap also served
 * is covered independently: the IHDR guards reject any image whose
 * decoded output or intermediate inflate buffer would exceed 4 GiB, so
 * an IDAT stream far larger than its declared geometry is refused at
 * inflate time regardless of how much of it accumulated. */
#define RPNG_IDAT_MAX ((size_t)-1 / 2)

/* When the whole file is in the buffer (every in-tree caller: the
 * task spine and the synchronous loader decode at completion), the
 * compressed size is knowable before accumulating: walk the chunk
 * headers from the first IDAT and sum them.  One exact allocation
 * replaces the doubling - no copies during accumulation, no
 * capacity overshoot held across the decode.  A walk that runs off
 * the end (a truncated or genuinely streaming buffer) returns 0 and
 * the doubling below stays the fallback. */
static size_t rpng_idat_presize(const uint8_t *at, const uint8_t *end)
{
   size_t total = 0;
   /* 'at' points at the length field of the first IDAT chunk;
    * 'end' at the last byte of the input. */
   while (at + 8 <= end + 1)
   {
      uint32_t len =   ((uint32_t)at[0] << 24) | ((uint32_t)at[1] << 16)
                     | ((uint32_t)at[2] <<  8) |  (uint32_t)at[3];
      if (len > RPNG_IDAT_MAX - total)
         return 0;
      if (!memcmp(at + 4, "IDAT", 4))
         total += len;
      else if (!memcmp(at + 4, "IEND", 4))
         return total;
      /* length + type + payload + CRC */
      if ((size_t)(end + 1 - at) < (size_t)len + 12)
         return 0;             /* truncated: fall back to doubling */
      at += (size_t)len + 12;
   }
   return 0;
}

static bool rpng_realloc_idat(struct idat_buffer *buf, uint32_t chunk_size)
{
   size_t required;

   /* Pre-patch: buf->size + chunk_size was size_t + uint32_t.  On
    * 32-bit size_t the sum could wrap (accumulated IDAT plus a
    * near-UINT32_MAX chunk_size), making "required > capacity"
    * false, the realloc skipped, and the subsequent memcpy writing
    * past the existing buffer.  Detect overflow explicitly and
    * cap total growth. */
   if (chunk_size > RPNG_IDAT_MAX - buf->size)
      return false;
   required = buf->size + chunk_size;

   if (required > buf->capacity)
   {
      uint8_t *new_buffer = NULL;
      size_t new_cap      = buf->capacity ? buf->capacity : 4096;

      /* Cap the doubling too so a malicious chunk at the edge of
       * RPNG_IDAT_MAX cannot drive new_cap past SIZE_MAX / 2. */
      while (new_cap < required)
      {
         if (new_cap > RPNG_IDAT_MAX / 2)
         {
            new_cap = RPNG_IDAT_MAX;
            break;
         }
         new_cap *= 2;
      }
      if (new_cap < required)
         return false;

      new_buffer = (uint8_t*)realloc(buf->data, new_cap);

      if (!new_buffer)
         return false;

      buf->data     = new_buffer;
      buf->capacity = new_cap;
   }

   return true;
}

static struct rpng_process *rpng_process_init(rpng_t *rpng)
{
   uint8_t *inflate_buf            = NULL;
   /* calloc zeroes all fields (pointers, integers, flags) in one call */
   struct rpng_process *process    = (struct rpng_process*)calloc(1, sizeof(*process));

   if (!process)
      return NULL;

   process->stream_backend         = trans_stream_get_zlib_inflate_backend();

   /* A build without an inflate backend (no HAVE_ZLIB) gets a stub
    * whose members are NULL; without this check the call below jumps
    * to address zero.  Such a build cannot decode a PNG at all, so
    * fail the open honestly. */
   if (   !process->stream_backend
       || !process->stream_backend->stream_new)
   {
      free(process);
      return NULL;
   }

   rpng_pass_geom(&rpng->ihdr, rpng->ihdr.width,
         rpng->ihdr.height, NULL, NULL, &process->inflate_buf_size);
   if (rpng->ihdr.interlace == 1) /* To be sure. */
      process->inflate_buf_size *= 2;

   process->stream = process->stream_backend->stream_new();

   if (!process->stream)
   {
      free(process);
      return NULL;
   }

   inflate_buf = (uint8_t*)malloc(process->inflate_buf_size);
   if (!inflate_buf)
      goto error;

   process->inflate_buf = inflate_buf;
   process->avail_in    = rpng->idat_buf.size;
   process->avail_out   = process->inflate_buf_size;

   process->stream_backend->set_in(
         process->stream,
         rpng->idat_buf.data,
         (uint32_t)rpng->idat_buf.size);
   process->stream_backend->set_out(
         process->stream,
         process->inflate_buf,
         (uint32_t)process->inflate_buf_size);

   return process;

error:
   if (process)
   {
      if (process->stream)
         process->stream_backend->stream_free(process->stream);
      free(process);
   }
   return NULL;
}

/**
 * rpng_read_chunk_header:
 *
 * Leaf function.
 *
 * @return The PNG type of the memory chunk (i.e. IHDR, IDAT, IEND,
   PLTE, and/or tRNS)
 **/
static enum png_chunk_type rpng_read_chunk_header(
      uint8_t *buf, uint32_t chunk_size)
{
   int i;
   /* Read chunk type as a big-endian 32-bit word for fast comparison */
   uint32_t tag = rpng_dword_be(buf + 4);

   /* Validate: all four bytes must be ASCII letters (65-90 or 97-122) */
   for (i = 0; i < 4; i++)
   {
      uint8_t byte = (uint8_t)(tag >> (24 - i * 8));
      if ((byte < 65) || ((byte > 90) && (byte < 97)) || (byte > 122))
         return PNG_CHUNK_ERROR;
   }

   /* IDAT is the most common chunk type — check it first */
   if (tag == 0x49444154) /* "IDAT" */
      return PNG_CHUNK_IDAT;
   if (tag == 0x49484452) /* "IHDR" */
      return PNG_CHUNK_IHDR;
   if (tag == 0x49454E44) /* "IEND" */
      return PNG_CHUNK_IEND;
   if (tag == 0x504C5445) /* "PLTE" */
      return PNG_CHUNK_PLTE;
   if (tag == 0x74524E53) /* "tRNS" */
      return PNG_CHUNK_tRNS;
   if (tag == 0x63494350) /* "cICP" */
      return PNG_CHUNK_cICP;
   if (tag == 0x634C4C49) /* "cLLI" */
      return PNG_CHUNK_cLLI;
   if (tag == 0x6D444356) /* "mDCV" */
      return PNG_CHUNK_mDCV;

   return PNG_CHUNK_NOOP;
}

bool rpng_iterate_image(rpng_t *rpng)
{
   uint8_t *buf             = (uint8_t*)rpng->buff_data;
   uint32_t chunk_size      = 0;
   size_t   remaining;

   rpng->need_more = false;

   /* Check whether data buffer pointer is valid */
   if (buf > rpng->buff_end)
      return false;

   /* The read cursor may have advanced past the resident frontier (the
    * previous chunk ended near it): that is the wall, not EOF.  Guard
    * before the size subtractions below, which are unsigned and would
    * otherwise wrap to a huge "remaining" and wave a non-resident
    * chunk through. */
   if (buf > rpng->avail_end)
   {
      if (rpng->avail_end < rpng->buff_end)
         rpng->need_more = true;
      return false;
   }

   /* The chunk header (length + type = 8 bytes) must lie within the
    * resident frontier.  If it does not but more of the file is still
    * to arrive, this is the resident wall, not a malformed header:
    * flag need_more so the caller retries after feeding. */
   if ((size_t)(rpng->avail_end - buf) + 1 < 8)
   {
      if (rpng->avail_end < rpng->buff_end)
         rpng->need_more = true;
      return false;
   }

   chunk_size = rpng_dword_be(buf);

   /* Check whether chunk will overflow the data buffer.
    *
    * Pre-patch:
    *    if (buf + 8 + chunk_size > rpng->buff_end) return false;
    * is pointer arithmetic on a uint8_t * with an attacker-
    * controlled 32-bit chunk_size.  For a value near UINT32_MAX
    * the sum wraps the pointer address (UB per C99; on 32-bit the
    * arithmetic genuinely rolls over and the compare defeats the
    * check, letting the memcpy at the IDAT handler read ~4 GiB
    * past the end of the input).  Compare sizes instead of
    * pointers, and reject chunk_size that cannot possibly fit
    * even before accounting for the type/CRC overhead.
    *
    * 'remaining' counts only RESIDENT bytes (to avail_end): a chunk
    * whose body has not fully arrived yet is a wall, not an
    * overflow.  A chunk that cannot fit within the TRUE end
    * (buff_end) even when fully resident is genuinely malformed. */
   remaining = (size_t)(rpng->avail_end - buf) + 1;
   if (chunk_size > remaining || remaining - chunk_size < 12)
   {
      /* Would the chunk fit if the rest of the file were resident?
       * If so, we are only at the resident wall - ask for more. */
      size_t true_remaining = (size_t)(rpng->buff_end - buf) + 1;
      if (rpng->avail_end < rpng->buff_end
            && chunk_size <= true_remaining
            && true_remaining - chunk_size >= 12)
      {
         rpng->need_more = true;
         return false;
      }
      return false;
   }

   switch (rpng_read_chunk_header(buf, chunk_size))
   {
      case PNG_CHUNK_NOOP:
      default:
         break;

      case PNG_CHUNK_ERROR:
         return false;

      case PNG_CHUNK_IHDR:
         if (rpng->flags & (
                    RPNG_FLAG_HAS_IHDR 
                  | RPNG_FLAG_HAS_IDAT
                  | RPNG_FLAG_HAS_IEND))
            return false;

         if (chunk_size != 13)
            return false;

         buf                    += 4 + 4;

         rpng->ihdr.width        = rpng_dword_be(buf + 0);
         rpng->ihdr.height       = rpng_dword_be(buf + 4);
         rpng->ihdr.depth        = buf[8];
         rpng->ihdr.color_type   = buf[9];
         rpng->ihdr.compression  = buf[10];
         rpng->ihdr.filter       = buf[11];
         rpng->ihdr.interlace    = buf[12];

         /* Validate color_type + depth combination before any size
          * arithmetic; rpng_pass_geom's switch relies on color_type
          * being one of the five legal values. */
         if (!rpng_process_ihdr(&rpng->ihdr))
            return false;

         if (rpng->ihdr.width == 0 || rpng->ihdr.height == 0)
            return false;

         /* Two independent size caps, both at 4 GiB:
          *
          *   1) Output buffer — rpng always decodes to ARGB32 regardless
          *      of the source depth, so the final buffer is always
          *      width * height * 4 bytes.
          *
          *   2) Intermediate inflate buffer — sized by rpng_pass_geom
          *      as (pitch + 1) * height.  For 8bpc RGBA this matches
          *      the output (~4 bytes/pixel), but for 16bpc RGBA it is
          *      2x (8 bytes/pixel), and palette/gray paths are smaller.
          *      A 30000x30000 16bpc-RGBA image passes the output cap
          *      (3.35 GiB) but needs a 7 GiB intermediate — reject it
          *      here rather than relying on malloc to fail downstream.
          *
          * Both caps use 64-bit arithmetic; the ULL literal keeps the
          * constant unambiguously 64-bit on LLP64 (Windows) where
          * unsigned long is 32-bit.  rpng_pass_geom's arithmetic is
          * itself size_t-wide after the prior widening commit, so the
          * pass_size returned here is trustworthy.
          *
          * On ILP32 platforms (e.g. 32-bit PPC / i686), size_t is 32-bit
          * and pass_size can never reach 2^32, so GCC warns that the
          * pass_size cap is always false.  Preprocessor-gate it on
          * 64-bit size_t; the output-size cap remains active on both
          * 32-bit and 64-bit (width*height*4 can overflow 32-bit even
          * when each factor is 32-bit). */
         {
            size_t pass_size = 0;
            rpng_pass_geom(&rpng->ihdr, rpng->ihdr.width,
                           rpng->ihdr.height, NULL, NULL, &pass_size);
            if ((uint64_t)rpng->ihdr.width * rpng->ihdr.height
                     * sizeof(uint32_t) >= 0x100000000ULL
#if SIZE_MAX > 0xFFFFFFFFULL
                  || (uint64_t)pass_size >= 0x100000000ULL
#endif
               )
               return false;
         }

         if (rpng->ihdr.compression != 0)
         {
#if defined(DEBUG) || defined(RPNG_TEST)
            fprintf(stderr, "[RPNG] Error in line %d.\n", __LINE__);
#endif
            return false;
         }

         rpng->flags   |= RPNG_FLAG_HAS_IHDR;
         break;

      case PNG_CHUNK_PLTE:
         {
            int i;
            unsigned entries = chunk_size / 3;

            if (entries > 256)
               return false;
            if (chunk_size % 3)
               return false;

            if (    !(rpng->flags & RPNG_FLAG_HAS_IHDR) 
                  || (rpng->flags & (
                        RPNG_FLAG_HAS_PLTE 
                      | RPNG_FLAG_HAS_IEND 
                      | RPNG_FLAG_HAS_IDAT
                      | RPNG_FLAG_HAS_TRNS)))
               return false;

            buf += 8;

            for (i = 0; i < (int)entries; i++)
            {
               uint32_t r       = buf[3 * i + 0];
               uint32_t g       = buf[3 * i + 1];
               uint32_t b       = buf[3 * i + 2];
               rpng->palette[i] = (r << 16) | (g << 8) | (b << 0) | (0xffu << 24);
            }

            rpng->flags        |= RPNG_FLAG_HAS_PLTE;
         }
         break;

      case PNG_CHUNK_tRNS:
         if (rpng->flags & RPNG_FLAG_HAS_IDAT)
            return false;

         if (rpng->ihdr.color_type == PNG_IHDR_COLOR_PLT)
         {
            int i;
            uint32_t *palette;
            /* we should compare with the number of palette entries */
            if (chunk_size > 256)
               return false;

            buf    += 8;
            palette = rpng->palette;

            for (i = 0; i < (int)chunk_size; i++, buf++, palette++)
               *palette = (*palette & 0x00ffffff) | (unsigned)*buf << 24;
         }
         /* TODO: support colorkey in grayscale and truecolor images */

         rpng->flags         |= RPNG_FLAG_HAS_TRNS;
         break;

      case PNG_CHUNK_cICP:
         /* Coding-independent code points: 4-byte payload
          * (primaries, transfer, matrix, full-range flag). Must
          * precede IDAT. Ignore malformed sizes rather than failing
          * the whole decode over an ancillary chunk. */
         if (!(rpng->flags & RPNG_FLAG_HAS_IDAT) && chunk_size == 4)
         {
            buf += 8;
            rpng->hdr.colour_primaries      = buf[0];
            rpng->hdr.transfer_function     = buf[1];
            rpng->hdr.matrix_coefficients   = buf[2];
            rpng->hdr.video_full_range_flag = buf[3];
            rpng->flags |= RPNG_FLAG_HAS_HDR;
         }
         break;

      case PNG_CHUNK_cLLI:
         /* Content light level: MaxCLL, MaxFALL as 4-byte unsigned
          * integers in units of 0.0001 cd/m^2. */
         if (!(rpng->flags & RPNG_FLAG_HAS_IDAT) && chunk_size == 8)
         {
            buf += 8;
            rpng->hdr.max_cll  = (float)rpng_dword_be(buf + 0) / 10000.0f;
            rpng->hdr.max_fall = (float)rpng_dword_be(buf + 4) / 10000.0f;
            rpng->flags |= RPNG_FLAG_HAS_HDR;
         }
         break;

      case PNG_CHUNK_mDCV:
         /* Mastering display colour volume: R,G,B then white
          * chromaticity pairs (2-byte, units of 0.00002), then max
          * and min luminance (4-byte, units of 0.0001 cd/m^2). */
         if (!(rpng->flags & RPNG_FLAG_HAS_IDAT) && chunk_size == 24)
         {
            int c;
            buf += 8;
            for (c = 0; c < 3; c++)
            {
               rpng->hdr.primary_chromaticity[c][0] =
                  (float)rpng_word_be(buf + c * 4 + 0) / 50000.0f;
               rpng->hdr.primary_chromaticity[c][1] =
                  (float)rpng_word_be(buf + c * 4 + 2) / 50000.0f;
            }
            rpng->hdr.white_point[0] = (float)rpng_word_be(buf + 12) / 50000.0f;
            rpng->hdr.white_point[1] = (float)rpng_word_be(buf + 14) / 50000.0f;
            rpng->hdr.max_luminance  = (float)rpng_dword_be(buf + 16) / 10000.0f;
            rpng->hdr.min_luminance  = (float)rpng_dword_be(buf + 20) / 10000.0f;
            rpng->hdr.write_mdcv     = 1;
            rpng->flags |= RPNG_FLAG_HAS_HDR;
         }
         break;

      case PNG_CHUNK_IDAT:
         if (!rpng->idat_buf.data && !rpng->idat_buf.capacity)
         {
            /* Scan only resident bytes: on a partial buffer the IDAT
             * run past avail_end has not arrived, and presize reads
             * chunk headers forward.  It already returns 0 (fall back
             * to realloc doubling) on truncation, so a prefix simply
             * gets the incremental path until the buffer completes. */
            size_t exact = rpng_idat_presize(buf, rpng->avail_end);
            if (exact)
            {
               if (!(rpng->idat_buf.data = (uint8_t*)malloc(exact)))
                  return false;
               rpng->idat_buf.capacity = exact;
            }
         }
         if (     !(rpng->flags & RPNG_FLAG_HAS_IHDR)
               ||  (rpng->flags & RPNG_FLAG_HAS_IEND)
               ||  (rpng->ihdr.color_type == PNG_IHDR_COLOR_PLT
                  &&
                  !(rpng->flags & RPNG_FLAG_HAS_PLTE)))
            return false;

         if (!rpng_realloc_idat(&rpng->idat_buf, chunk_size))
            return false;

         buf += 8;

         memcpy(rpng->idat_buf.data + rpng->idat_buf.size, buf, chunk_size);

         rpng->idat_buf.size += chunk_size;

         rpng->flags         |= RPNG_FLAG_HAS_IDAT;
         break;

      case PNG_CHUNK_IEND:
         if ((rpng->flags & (RPNG_FLAG_HAS_IHDR | RPNG_FLAG_HAS_IDAT)) != (RPNG_FLAG_HAS_IHDR | RPNG_FLAG_HAS_IDAT))
            return false;

         rpng->flags         |= RPNG_FLAG_HAS_IEND;
         return false;
   }

   /* chunk_size + 12 is a uint32_t + int, promoted to uint32_t,
    * which wraps for chunk_size near UINT32_MAX.  The
    * per-chunk-size overflow guard at the top of this function
    * already rejects values that large, but keep the arithmetic
    * explicit in size_t here so readers (and future callers who
    * might loosen that guard) don't trip the wrap. */
   rpng->buff_data += (size_t)chunk_size + 12;

   /* Check whether data buffer pointer is valid */
   if (rpng->buff_data > rpng->buff_end)
      return false;
   return true;
}

int rpng_process_image(rpng_t *rpng,
      void **_data, size_t len, unsigned *width, unsigned *height,
      bool supports_rgba)
{
   uint32_t **data = (uint32_t**)_data;

   rpng->supports_rgba = supports_rgba;

   if (!rpng->process)
   {
      struct rpng_process *process;

      /* Pre-swizzle palette entries for ABGR output.
       * The palette was assembled as ARGB during PLTE chunk parsing;
       * for supports_rgba we need ABGR. Swap R↔B once here (max 256
       * entries) instead of per-pixel in the copy_line_plt path.
       * Done inside the !process guard so it runs exactly once. */
      if (supports_rgba && (rpng->flags & RPNG_FLAG_HAS_PLTE))
      {
         int pi;
         for (pi = 0; pi < 256; pi++)
         {
            uint32_t c  = rpng->palette[pi];
            rpng->palette[pi] = (c & 0xFF00FF00u)
                               | ((c & 0x00FF0000u) >> 16)
                               | ((c & 0x000000FFu) << 16);
         }
      }

      process = rpng_process_init(rpng);

      if (!process)
         goto error;

      rpng->process = process;
      rpng->process->supports_rgba = supports_rgba;
      rpng->process->want_10bit    = rpng->want_10bit;
      return IMAGE_PROCESS_NEXT;
   }

   if (!(rpng->process->flags & RPNG_PROCESS_FLAG_INFLATE_INITIALIZED))
   {
      if (rpng_load_image_argb_process_inflate_init(rpng, data) == -1)
         goto error;
      return IMAGE_PROCESS_NEXT;
   }

   *width  = rpng->ihdr.width;
   *height = rpng->ihdr.height;

   if (rpng->ihdr.interlace && rpng->process)
      return rpng_reverse_filter_adam7(data, &rpng->ihdr, rpng->process);
   return rpng_reverse_filter_regular_iterate(data,
      &rpng->ihdr, rpng->process);

error:
   if (rpng->process)
   {
      if (rpng->process->inflate_buf)
         free(rpng->process->inflate_buf);
      if (rpng->process->stream)
         rpng->process->stream_backend->stream_free(rpng->process->stream);
      free(rpng->process);
      rpng->process = NULL;
   }
   return IMAGE_PROCESS_ERROR;
}

void rpng_free(rpng_t *rpng)
{
   if (!rpng)
      return;

   if (rpng->idat_buf.data)
      free(rpng->idat_buf.data);
   if (rpng->process)
   {
      if (rpng->process->inflate_buf)
         free(rpng->process->inflate_buf);
      if (rpng->process->stream)
      {
         if (   rpng->process->stream_backend 
             && rpng->process->stream_backend->stream_free)
            rpng->process->stream_backend->stream_free(rpng->process->stream);
         else
            free(rpng->process->stream);
      }
      free(rpng->process);
   }

   free(rpng);
}

bool rpng_start(rpng_t *rpng)
{
   if (!rpng)
      return false;

   /* Check whether reading the header will overflow
    * the data buffer */
   if (rpng->buff_end - rpng->buff_data < 8)
      return false;

   if (memcmp(rpng->buff_data, png_magic, sizeof(png_magic)) != 0)
      return false;

   rpng->buff_data += 8;

   return true;
}

/* Prefix early-start gate: return true once the resident bytes contain
 * the 8-byte signature and the whole IHDR chunk, so the chunk walk can
 * begin (it parses IHDR before anything else, then gathers IDAT with
 * per-chunk need_more waits as the read progresses).  IHDR is a fixed
 * 13-byte payload: signature (8) + length (4) + "IHDR" (4) + data (13)
 * + CRC (4) = 33 bytes.  No allocation, no decode. */
bool rpng_header_ready(const uint8_t *data, size_t len)
{
   if (!data || len < 33)
      return false;
   if (memcmp(data, png_magic, sizeof(png_magic)) != 0)
      return false;
   /* Bytes 12..15 are the chunk type of the first chunk after the
    * signature; per spec it must be IHDR. */
   if (memcmp(data + 12, "IHDR", 4) != 0)
      return false;
   return true;
}

/**
 * rpng_is_valid:
 *
 * Check if @rpng is a valid PNG image.
 * Must contain an IHDR chunk, one or more IDAT
 * chunks, and an IEND chunk.
 *
 * Leaf function.
 *
 * @return true if it's a valid PNG image, otherwise false.
 **/
bool rpng_is_valid(rpng_t *rpng)
{
   const uint8_t valid_mask = RPNG_FLAG_HAS_IHDR
                            | RPNG_FLAG_HAS_IDAT
                            | RPNG_FLAG_HAS_IEND;
   return (rpng && ((rpng->flags & valid_mask) == valid_mask));
}

bool rpng_get_hdr_metadata(rpng_t *rpng, struct rpng_hdr_metadata *out)
{
   if (!rpng || !out || !(rpng->flags & RPNG_FLAG_HAS_HDR))
      return false;
   *out = rpng->hdr;
   return true;
}

void rpng_set_want_10bit(rpng_t *rpng, int want)
{
   if (rpng)
      rpng->want_10bit = (want != 0);
}

bool rpng_is_10bit(const rpng_t *rpng)
{
   /* True only when 10-bit output was requested and the source is a 16-bit
    * RGB image, i.e. the decode actually produced packed XRGB2101010. Only
    * the RGB (colour type 2) path packs 10-bit; 16-bit RGBA still narrows to
    * 8-bit, so it must not report 10-bit here. */
   return rpng
      && rpng->want_10bit
      && (rpng->flags & RPNG_FLAG_HAS_IHDR)
      && rpng->ihdr.depth == 16
      && rpng->ihdr.color_type == PNG_IHDR_COLOR_RGB;
}

bool rpng_set_buf_ptr(rpng_t *rpng, void *data, size_t len)
{
   if (!rpng || (len < 1))
      return false;

   rpng->buff_data = (uint8_t*)data;
   rpng->buff_start = rpng->buff_data;
   rpng->buff_end  = rpng->buff_data + (len - 1);
   /* Default: the whole buffer is resident.  A prefix-feeding caller
    * lowers the frontier with rpng_set_avail after this. */
   rpng->avail_end = rpng->buff_end;
   rpng->need_more = false;

   return true;
}

/* Prefix decoding: declare how many bytes from buff_data are actually
 * resident.  'avail' is a byte count; the frontier is clamped to the
 * true buffer end and only ever advances.  While the frontier is below
 * buff_end, rpng_iterate_image treats a chunk that reaches past it as
 * "need more" (rpng_need_more() returns true) rather than end-of-file,
 * so a caller feeding a growing read can retry.  With avail == full
 * length (the default) the walk is exactly the classic whole-buffer
 * one. */
void rpng_set_avail(rpng_t *rpng, size_t avail)
{
   uint8_t *front;
   size_t   full;
   if (!rpng || !rpng->buff_start || !rpng->buff_end)
      return;
   /* Clamp in the size domain first: a caller signalling "whole buffer
    * resident" passes (size_t)-1, and buff_start + (avail - 1) would
    * overflow the pointer (UB) before the buff_end clamp below could
    * catch it.  full = total length = (buff_end - buff_start) + 1. */
   full = (size_t)(rpng->buff_end - rpng->buff_start) + 1;
   if (avail > full)
      avail = full;
   if (avail == 0)   /* nothing resident yet: keep the frontier unset */
      return;
   /* Anchor on buff_start (the fixed buffer base captured at
    * set_buf_ptr): buff_data advances as chunks are consumed, so
    * deriving the frontier from it would place the wall 'avail' bytes
    * past the CURSOR instead of past the start. */
   front = rpng->buff_start + avail - 1;
   if (front > rpng->buff_end)
      front = rpng->buff_end;
   /* The default frontier is the whole buffer (for callers that never
    * feed a prefix).  The first set_avail switches to caller-driven
    * mode and sets the frontier absolutely - it is lower than the
    * default - after which it is strictly monotonic. */
   if (!(rpng->flags & RPNG_FLAG_AVAIL_SET))
   {
      rpng->flags    |= RPNG_FLAG_AVAIL_SET;
      rpng->avail_end = front;
   }
   else if (front > rpng->avail_end)
      rpng->avail_end = front;
}

/* True when the last rpng_iterate_image stopped because a chunk lay
 * past the resident frontier (not EOF, not malformed): raise the
 * frontier with rpng_set_avail and iterate again. */
bool rpng_need_more(const rpng_t *rpng)
{
   return rpng ? rpng->need_more : false;
}

rpng_t *rpng_alloc(void)
{
   rpng_t *rpng = (rpng_t*)calloc(1, sizeof(*rpng));
   if (!rpng)
      return NULL;
   return rpng;
}
