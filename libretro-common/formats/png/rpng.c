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

/* rpng -- PNG decoder.
 *
 * What it implements: all PNG colour types at their legal bit depths
 * (greyscale 1-16, palette 1-8, RGB / greyscale+alpha / RGBA at 8 and
 * 16 bits), tRNS transparency, Adam7 interlacing, incremental decoding
 * over partially resident buffers (rpng_set_avail), and 32-bit XRGB
 * output with an optional packed-XRGB2101010 path for 16-bit sources.
 * APNG animation streaming lives in rpng_apng.c and a PNG encoder in
 * rpng_encode.c.
 *
 * What it does not implement: colour management (gAMA/cHRM/iCCP and
 * other ancillary chunks are skipped), 16-bit-per-channel output
 * beyond the XRGB2101010 path, and MNG/JNG.
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

/* Branchless Paeth predictor.
 *
 * filters.h's paeth() is shared with the scaler and resampler and is
 * left alone; this is the same function written without the two
 * unpredictable branches.  Image data makes the a/b/c choice close to
 * random, so those branches mispredict constantly - dropping them is
 * worth 1.4x-1.8x on the scalar path depending on stride, which is what
 * platforms without SSE2 or NEON run.
 *
 * Identity used, as in the vector kernels:
 *   pa = |b - c|, pb = |a - c|, pc = |(b - c) + (a - c)|
 * Selection order is a, then b, then c, matching the spec. */
static INLINE int rpng_paeth(int a, int b, int c)
{
   int pa   = b - c;
   int pb   = a - c;
   int pc   = pa + pb;
   int apa  = pa < 0 ? -pa : pa;
   int apb  = pb < 0 ? -pb : pb;
   int apc  = pc < 0 ? -pc : pc;
   /* All-ones masks: "do not pick a" and "pick c over b". */
   int nota = -(int)((apa > apb) | (apa > apc));
   int pick = -(int)(apb > apc);
   int bc   = (b & ~pick) | (c & pick);
   return (a & ~nota) | (bc & nota);
}

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

/* One IDAT payload's position in the caller's buffer.  The compressed
 * stream is no longer copied out of that buffer: the pointer handed to
 * rpng_set_buf_ptr is stable for the life of the decode (task_image
 * documents this - every nbio backend sizes or maps it up front, and
 * only the resident frontier moves), and the APNG path synthesises
 * each frame's PNG into a buffer that is only ever reallocated between
 * frames.  Offsets are kept relative to buff_start rather than as raw
 * pointers because buff_data advances as chunks are consumed. */
struct idat_span
{
   size_t   off;             /* payload start, relative to buff_start   */
   uint32_t len;
};

struct idat_spans
{
   struct idat_span *v;
   size_t n;
   size_t cap;               /* elements allocated                      */
   size_t total;             /* summed payload bytes across all spans   */
};

enum rpng_process_flags
{
   RPNG_PROCESS_FLAG_INFLATE_INITIALIZED    = (1 << 0),
   RPNG_PROCESS_FLAG_ADAM7_PASS_INITIALIZED = (1 << 1),
   RPNG_PROCESS_FLAG_PASS_INITIALIZED       = (1 << 2),
   RPNG_PROCESS_FLAG_OUTPUT_INITIALIZED     = (1 << 3),
   RPNG_PROCESS_FLAG_FILTER_STARVED         = (1 << 4)
};

/* Internal signal from the reverse-filter layer: not enough of the
 * stream has been inflated yet for the next unit of work (a scanline,
 * or a whole Adam7 pass) - come back after another inflate slice.
 * Deliberately -3: it travels the same int returns as
 * enum image_process_code, where -2 is already IMAGE_PROCESS_ERROR
 * and -1 IMAGE_PROCESS_ERROR_END. */
#define RPNG_FILTER_WAIT (-3)

struct rpng_process
{
   uint32_t *data;
   uint32_t *palette;
   void *stream;
   const struct trans_stream_backend *stream_backend;
   /* Unfiltering runs in place inside the inflate ring wherever a
    * kernel's stores cannot clobber raw bytes a later step still
    * needs: None, Up, the prefix and stride-exact Sub widths, and
    * Average/Paeth at stride == window (bpp 4 and 8).  For those, a
    * line's decoded bytes overwrite its raw bytes and the previous
    * line's ring slot doubles as the prev pointer; held is that
    * slot's byte size, excluded from restore_buf_size until the next
    * line no longer needs it, so the ring cannot recycle it.  The
    * remaining filter/bpp combinations keep their wide window stores
    * and decode out of place into three rotating scratch lines -
    * three, because the wavefront pair needs two fresh lines while a
    * third may still be the pair's prev.  scratch[0] starts zeroed
    * and serves as row 0's prev; scratch_cur indexes the most
    * recently written line, so (cur + 1) % 3 and (cur + 2) % 3 are
    * always free. */
   uint8_t       *scratch[3];
   unsigned       scratch_cur;
   const uint8_t *prev_line;
   size_t         held;
   uint8_t *inflate_buf;      /* walking read cursor for the filters     */
   uint8_t *inflate_base;     /* fixed allocation base: inflate writes at
                               * inflate_base + inflated_total, immune to
                               * the cursor walk and to Adam7's per-pass
                               * total_out consumption accounting        */
   size_t restore_buf_size;
   size_t adam7_restore_buf_size;
   /* Where the next unfiltered scanline's pixels land.  This is the
    * walking cursor that used to live in the CALLER'S pointer
    * (*data += width per line, subtracted back at the end): any
    * externally abandoned decode - a cancelled thumbnail task being
    * the everyday case - left the caller holding an interior
    * pointer, and freeing it corrupts the heap.  Manifested on
    * Windows as STATUS_HEAP_CORRUPTION (c0000374) at a later free in
    * task_image_load_free.  The caller's pointer now never moves. */
   uint32_t *out_cursor;
   size_t inflate_buf_size;   /* expected total inflated bytes (geometry) */
   size_t ring_size;          /* allocation size of inflate_base.  Equal to
                               * inflate_buf_size for interlaced images and
                               * images at or below the ring floor; smaller
                               * for regular images, where the window
                               * recycles: writes land at
                               * inflated_total % ring_size and the read
                               * cursor wraps at the ring end.  Sized as a
                               * whole number of scanlines so a line never
                               * straddles the wrap.                       */
   size_t avail_in;
   size_t avail_out;
   size_t span_idx;          /* current IDAT span being fed             */
   uint32_t span_pos;        /* bytes of it already consumed            */
   size_t inflated_total;    /* monotone bytes produced by the stream   */
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
   struct idat_spans idat_buf; /* ptr alignment */
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
   /* Each byte promotes to int, so a leading byte >= 0x80 shifted left by
    * 24 overflows a 32-bit signed int.  That is undefined behavior, and
    * UBSan flags it on the chunk-header path for any chunk whose length
    * or type byte has the high bit set.  Widen to uint32_t first. */
   return  ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16)
         | ((uint32_t)buf[2] <<  8) | ((uint32_t)buf[3] <<  0);
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
   /* Four vectors per iteration: the loop is bound by load/store
    * throughput, and the wider body halves the per-16-byte loop
    * overhead against the single-vector form. */
   size_t i  = 0;
   for (; i + 64 <= len; i += 64)
   {
      __m128i r0 = _mm_loadu_si128((const __m128i*)(raw   + i));
      __m128i p0 = _mm_loadu_si128((const __m128i*)(prior + i));
      __m128i r1 = _mm_loadu_si128((const __m128i*)(raw   + i + 16));
      __m128i p1 = _mm_loadu_si128((const __m128i*)(prior + i + 16));
      __m128i r2 = _mm_loadu_si128((const __m128i*)(raw   + i + 32));
      __m128i p2 = _mm_loadu_si128((const __m128i*)(prior + i + 32));
      __m128i r3 = _mm_loadu_si128((const __m128i*)(raw   + i + 48));
      __m128i p3 = _mm_loadu_si128((const __m128i*)(prior + i + 48));
      _mm_storeu_si128((__m128i*)(out + i),      _mm_add_epi8(r0, p0));
      _mm_storeu_si128((__m128i*)(out + i + 16), _mm_add_epi8(r1, p1));
      _mm_storeu_si128((__m128i*)(out + i + 32), _mm_add_epi8(r2, p2));
      _mm_storeu_si128((__m128i*)(out + i + 48), _mm_add_epi8(r3, p3));
   }
   for (; i + 16 <= len; i += 16)
   {
      __m128i r  = _mm_loadu_si128((const __m128i*)(raw   + i));
      __m128i p  = _mm_loadu_si128((const __m128i*)(prior + i));
      _mm_storeu_si128((__m128i*)(out + i), _mm_add_epi8(r, p));
   }
   for (; i < len; i++)
      out[i] = raw[i] + prior[i];
#elif defined(RPNG_SIMD_NEON)
   size_t i  = 0;
   for (; i + 64 <= len; i += 64)
   {
      uint8x16_t r0 = vld1q_u8(raw   + i);
      uint8x16_t p0 = vld1q_u8(prior + i);
      uint8x16_t r1 = vld1q_u8(raw   + i + 16);
      uint8x16_t p1 = vld1q_u8(prior + i + 16);
      uint8x16_t r2 = vld1q_u8(raw   + i + 32);
      uint8x16_t p2 = vld1q_u8(prior + i + 32);
      uint8x16_t r3 = vld1q_u8(raw   + i + 48);
      uint8x16_t p3 = vld1q_u8(prior + i + 48);
      vst1q_u8(out + i,      vaddq_u8(r0, p0));
      vst1q_u8(out + i + 16, vaddq_u8(r1, p1));
      vst1q_u8(out + i + 32, vaddq_u8(r2, p2));
      vst1q_u8(out + i + 48, vaddq_u8(r3, p3));
   }
   for (; i + 16 <= len; i += 16)
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
 *   - prev is a valid previous-line pointer (a zero line on row 0
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

/* Load 8 bytes and zero-extend each to 16 bits, filling all 8 lanes.
 * Used by the Paeth kernel, which steps by the pixel stride (bpp) rather
 * than by the register width, so it wants the widest window it can get. */
static INLINE __m128i rpng_load8_u8_to_u16(const uint8_t *p)
{
   __m128i v = _mm_loadl_epi64((const __m128i*)p);
   return _mm_unpacklo_epi8(v, _mm_setzero_si128());
}

/* Store all 8 lanes back as packed u8.  The caller guarantees 8 bytes of
 * room; lanes past the stride are overwritten by later iterations or by
 * the scalar tail. */
static INLINE void rpng_store8_u16_to_u8(uint8_t *p, __m128i v)
{
   _mm_storel_epi64((__m128i*)p, _mm_packus_epi16(v, v));
}

/* Sub and Average both fit in 8-bit lanes, so unlike Paeth they never
 * need the widening load.  Sub is a plain mod-256 add.  Average wants
 * floor((a + b) / 2) and pavgb gives (a + b + 1) >> 1, but
 *   floor((a + b) / 2) = pavgb(a, b) - ((a ^ b) & 1)
 * so the rounding correction is two extra ops off the critical path.
 * Staying in u8 removes the unpack from the loop-carried chain and
 * doubles the useful lanes; see rpng_filter_paeth_simd for why a fixed
 * window may be stepped by an arbitrary stride. */
static INLINE __m128i rpng_load4_u8(const uint8_t *p)
{
   int32_t tmp;
   memcpy(&tmp, p, sizeof(tmp));
   return _mm_cvtsi32_si128(tmp);
}

static INLINE void rpng_store4_u8(uint8_t *p, __m128i v)
{
   int32_t tmp = _mm_cvtsi128_si32(v);
   memcpy(p, &tmp, sizeof(tmp));
}

static void rpng_filter_sub_prefix1(uint8_t *decoded,
      const uint8_t *raw, size_t pitch)
{
   size_t i      = 0;
   __m128i carry = _mm_setzero_si128();
   for (; i + 16 <= pitch; i += 16)
   {
      __m128i x = _mm_loadu_si128((const __m128i*)(raw + i));
      x = _mm_add_epi8(x, _mm_slli_si128(x, 1));
      x = _mm_add_epi8(x, _mm_slli_si128(x, 2));
      x = _mm_add_epi8(x, _mm_slli_si128(x, 4));
      x = _mm_add_epi8(x, _mm_slli_si128(x, 8));
      x = _mm_add_epi8(x, carry);
      _mm_storeu_si128((__m128i*)(decoded + i), x);
      /* broadcast byte 15 (SSE2 has no byte splat) */
      carry = _mm_unpackhi_epi8(x, x);
      carry = _mm_shufflehi_epi16(carry, 0xFF);
      carry = _mm_shuffle_epi32(carry, 0xEE);
   }
   {
      uint8_t c = i ? decoded[i - 1] : 0;
      for (; i < pitch; i++)
      {
         c          = (uint8_t)(raw[i] + c);
         decoded[i] = c;
      }
   }
}

static void rpng_filter_sub_prefix2(uint8_t *decoded,
      const uint8_t *raw, size_t pitch)
{
   size_t i      = 0;
   __m128i carry = _mm_setzero_si128();
   for (; i + 16 <= pitch; i += 16)
   {
      __m128i x = _mm_loadu_si128((const __m128i*)(raw + i));
      x = _mm_add_epi8(x, _mm_slli_si128(x, 2));
      x = _mm_add_epi8(x, _mm_slli_si128(x, 4));
      x = _mm_add_epi8(x, _mm_slli_si128(x, 8));
      x = _mm_add_epi8(x, carry);
      _mm_storeu_si128((__m128i*)(decoded + i), x);
      carry = _mm_shufflehi_epi16(x, 0xFF);
      carry = _mm_shuffle_epi32(carry, 0xEE);
   }
   for (; i < pitch; i++)
      decoded[i] = (uint8_t)(raw[i] + (i >= 2 ? decoded[i - 2] : 0));
}

static void rpng_filter_sub_prefix4(uint8_t *decoded,
      const uint8_t *raw, size_t pitch)
{
   size_t i      = 0;
   __m128i carry = _mm_setzero_si128();
   for (; i + 16 <= pitch; i += 16)
   {
      __m128i x = _mm_loadu_si128((const __m128i*)(raw + i));
      x = _mm_add_epi8(x, _mm_slli_si128(x, 4));
      x = _mm_add_epi8(x, _mm_slli_si128(x, 8));
      x = _mm_add_epi8(x, carry);
      _mm_storeu_si128((__m128i*)(decoded + i), x);
      carry = _mm_shuffle_epi32(x, 0xFF);
   }
   for (; i < pitch; i++)
      decoded[i] = (uint8_t)(raw[i] + (i >= 4 ? decoded[i - 4] : 0));
}

static void rpng_filter_sub_simd(uint8_t *decoded,
      const uint8_t *raw, size_t pitch, unsigned bpp)
{
   size_t i           = 0;
   __m128i prev_pixel = _mm_setzero_si128();
   switch (bpp)
   {
      case 1:
         rpng_filter_sub_prefix1(decoded, raw, pitch);
         return;
      case 2:
         rpng_filter_sub_prefix2(decoded, raw, pitch);
         return;
      case 4:
         rpng_filter_sub_prefix4(decoded, raw, pitch);
         return;
   }
   if (bpp <= 4)
   {
      for (; i + 4 <= pitch; i += bpp)
      {
         __m128i out = _mm_add_epi8(rpng_load4_u8(raw + i), prev_pixel);
         rpng_store4_u8(decoded + i, out);
         prev_pixel  = out;
      }
   }
   else
   {
      for (; i + 8 <= pitch; i += bpp)
      {
         __m128i out = _mm_add_epi8(
               _mm_loadl_epi64((const __m128i*)(raw + i)), prev_pixel);
         _mm_storel_epi64((__m128i*)(decoded + i), out);
         prev_pixel  = out;
      }
   }
   if (decoded != raw)
      memcpy(decoded + i, raw + i, pitch - i);
   /* Leading bpp bytes have no left neighbour and stay as-is. */
   if (i < bpp)
      i = bpp < pitch ? bpp : pitch;
   for (; i < pitch; i++)
      decoded[i] += decoded[i - bpp];
}

static void rpng_filter_avg_simd(uint8_t *decoded,
      const uint8_t *raw, const uint8_t *prev, size_t pitch, unsigned bpp)
{
   size_t i           = 0;
   __m128i prev_pixel = _mm_setzero_si128();
   const __m128i one  = _mm_set1_epi8(1);
   if (bpp <= 4)
   {
      for (; i + 4 <= pitch; i += bpp)
      {
         __m128i pv  = rpng_load4_u8(prev + i);
         __m128i avg = _mm_sub_epi8(_mm_avg_epu8(prev_pixel, pv),
               _mm_and_si128(_mm_xor_si128(prev_pixel, pv), one));
         __m128i out = _mm_add_epi8(rpng_load4_u8(raw + i), avg);
         rpng_store4_u8(decoded + i, out);
         prev_pixel  = out;
      }
   }
   else
   {
      for (; i + 8 <= pitch; i += bpp)
      {
         __m128i pv  = _mm_loadl_epi64((const __m128i*)(prev + i));
         __m128i avg = _mm_sub_epi8(_mm_avg_epu8(prev_pixel, pv),
               _mm_and_si128(_mm_xor_si128(prev_pixel, pv), one));
         __m128i out = _mm_add_epi8(
               _mm_loadl_epi64((const __m128i*)(raw + i)), avg);
         _mm_storel_epi64((__m128i*)(decoded + i), out);
         prev_pixel  = out;
      }
   }
   if (decoded != raw)
      memcpy(decoded + i, raw + i, pitch - i);
   for (; i < bpp && i < pitch; i++)
      decoded[i] += prev[i] >> 1;
   for (; i < pitch; i++)
      decoded[i] += (uint8_t)((decoded[i - bpp] + prev[i]) >> 1);
}

/* Branch-free Paeth predictor for 16-bit lanes, following the identity
 *   pa = |b - c|
 *   pb = |a - c|
 *   pc = |(b - c) + (a - c)| = |a + b - 2c|
 * PNG selection rule (in priority order): a if pa <= pb && pa <= pc,
 * else b if pb <= pc, else c. */
/* One Paeth pixel, selecting among speculative sums.  The three
 * candidate outputs (r + a) & FF, (r + b) & FF and (r + c) & FF are
 * formed up front: sb and sc do not involve a, so they leave the
 * loop-carried chain entirely, and the final add and mask move off the
 * chain as well.  The "don't pick a" test compares pa against
 * min(pb, pc), one comparison instead of two plus an or.  The chain a
 * -> ac -> pb -> min -> compare -> select is nine ops; the plain
 * predictor's is eleven.
 *
 * SSE2 lacks abs_epi16; max(x, -x) is the standard substitute. */
static INLINE __m128i rpng_paeth_step_epi16(__m128i a, __m128i b,
      __m128i c, __m128i r, __m128i sb, __m128i sc, __m128i mask)
{
   __m128i bc = _mm_sub_epi16(b, c);
   __m128i ac = _mm_sub_epi16(a, c);
   __m128i sm = _mm_add_epi16(bc, ac);
   __m128i z  = _mm_setzero_si128();
   __m128i pa = _mm_max_epi16(bc, _mm_sub_epi16(z, bc));
   __m128i pb = _mm_max_epi16(ac, _mm_sub_epi16(z, ac));
   __m128i pc = _mm_max_epi16(sm, _mm_sub_epi16(z, sm));
   __m128i not_a  = _mm_cmpgt_epi16(pa, _mm_min_epi16(pb, pc));
   __m128i pick_c = _mm_cmpgt_epi16(pb, pc);
   __m128i sa     = _mm_and_si128(_mm_add_epi16(r, a), mask);
   __m128i bc_sel = _mm_or_si128(_mm_andnot_si128(pick_c, sb),
                                 _mm_and_si128(   pick_c, sc));
   return            _mm_or_si128(_mm_andnot_si128(not_a, sa),
                                  _mm_and_si128(   not_a, bc_sel));
}

/* Paeth reverse filter for any pixel stride from 1 to 8 bytes, i.e.
 * every bpp rpng can produce: 1 (gray/palette 8), 2 (gray+alpha 8,
 * gray 16), 3 (RGB8), 4 (RGBA8), 6 (RGB16), 8 (RGBA16).
 *
 * The loop advances by bpp but loads and stores a fixed window (4 bytes
 * for bpp <= 4, 8 for the 16-bit colour types), so lanes at or past bpp
 * hold values that are not yet final.  That is harmless: byte j only
 * ever depends on decoded[j-bpp], prev[j] and prev[j-bpp], so lane
 * k < bpp of a step reads lane k of the *previous* step's output, which
 * was final.  Lanes >= bpp feed only lanes >= bpp, and every byte they
 * write is either rewritten by a later step or fixed up by the scalar
 * tail below.
 *
 * The window is also why the loop guard is i + 4 / i + 8 <= pitch rather
 * than i + bpp <= pitch - reading or writing past the scanline would be
 * out of bounds even though the extra lanes are discarded.
 *
 * Stride is a runtime value on purpose.  The chain through
 * rpng_paeth_predictor_epi16 is latency-bound, so the loop has cycles to
 * spare for a variable increment: measured throughput matches
 * per-stride specialized copies to within 1% at every bpp, without the
 * code size of six kernels. */
static void rpng_filter_paeth_simd(uint8_t *decoded,
      const uint8_t *raw, const uint8_t *prev, size_t pitch, unsigned bpp)
{
   size_t i                = 0;
   __m128i prev_pixel      = _mm_setzero_si128();  /* decoded[i-bpp] */
   __m128i prev_upper_left = _mm_setzero_si128();  /* prev[i-bpp]    */
   const __m128i mask      = _mm_set1_epi16(0x00FF);
   if (bpp <= 4)
   {
      /* Only lanes below bpp are ever consumed, so a 4-byte window is
       * enough here and is measurably cheaper than the 8-byte one
       * (~4% end-to-end on RGBA8 Paeth). */
      for (; i + 4 <= pitch; i += bpp)
      {
         __m128i r    = rpng_load4_u8_to_u16(raw  + i);
         __m128i pv   = rpng_load4_u8_to_u16(prev + i);
         __m128i sb   = _mm_and_si128(_mm_add_epi16(r, pv), mask);
         __m128i sc   = _mm_and_si128(_mm_add_epi16(r, prev_upper_left),
               mask);
         __m128i out  = rpng_paeth_step_epi16(prev_pixel, pv,
               prev_upper_left, r, sb, sc, mask);
         rpng_store4_u16_to_u8(decoded + i, out);
         prev_pixel      = out;
         prev_upper_left = pv;
      }
   }
   else
   {
      for (; i + 8 <= pitch; i += bpp)
      {
         __m128i r    = rpng_load8_u8_to_u16(raw  + i);
         __m128i pv   = rpng_load8_u8_to_u16(prev + i);
         __m128i sb   = _mm_and_si128(_mm_add_epi16(r, pv), mask);
         __m128i sc   = _mm_and_si128(_mm_add_epi16(r, prev_upper_left),
               mask);
         __m128i out  = rpng_paeth_step_epi16(prev_pixel, pv,
               prev_upper_left, r, sb, sc, mask);
         rpng_store8_u16_to_u8(decoded + i, out);
         prev_pixel      = out;
         prev_upper_left = pv;
      }
   }
   if (decoded != raw)
      memcpy(decoded + i, raw + i, pitch - i);
   /* A scanline shorter than the vector window leaves i == 0, so the
    * leading bpp bytes still have to go through the a == c == 0 case
    * (Paeth(0, b, 0) == b) before the general tail can index i - bpp. */
   for (; i < bpp && i < pitch; i++)
      decoded[i] += prev[i];
   for (; i < pitch; i++)
      decoded[i] += (uint8_t)rpng_paeth(decoded[i - bpp], prev[i],
            prev[i - bpp]);
}

#define RPNG_HAVE_PAIR_KERNELS 1

/* --- Wavefront pair kernels --------------------------------------------
 *
 * Average and Paeth are bound by the serial chain through the left
 * neighbour: one pixel per chain length, however wide the machine.
 * But two consecutive rows carrying the same filter can be unfiltered
 * in one interleaved pass along the classic diagonal wavefront: row 1
 * runs one pixel behind row 0, so its up neighbour is the row 0 output
 * produced in the *previous* iteration and the two carried chains
 * never wait on each other inside an iteration.  Two pixels per chain
 * length, on the two filters where the chain is the whole cost.
 * (A row-at-a-time decoder cannot do this, whatever its ISA level.)
 *
 * Window and stride semantics are exactly those of the single-row
 * kernels above: a fixed 4- or 8-byte window stepped by bpp, lanes at
 * or past the stride rewritten by later steps or the scalar tails.
 * Both rows share the frontier i at loop exit - row 1 trails by one
 * step inside the loop and settles its last window in the epilogue -
 * so both tails resume from i, row 0 first since row 1's tail reads
 * dec0. */
static void rpng_filter_paeth_pair(uint8_t *dec0, uint8_t *dec1,
      const uint8_t *raw0, const uint8_t *raw1, const uint8_t *prev,
      size_t pitch, unsigned bpp)
{
   size_t i           = 0;
   const __m128i mask = _mm_set1_epi16(0x00FF);
   __m128i a0         = _mm_setzero_si128();
   __m128i c0         = _mm_setzero_si128();
   __m128i a1         = _mm_setzero_si128();
   __m128i c1         = _mm_setzero_si128();
   __m128i a0p        = _mm_setzero_si128();
   if (bpp <= 4)
   {
      if (4 <= pitch)
      {
         __m128i b0 = rpng_load4_u8_to_u16(prev);
         __m128i r0 = rpng_load4_u8_to_u16(raw0);
         __m128i sb = _mm_and_si128(_mm_add_epi16(r0, b0), mask);
         __m128i sc = _mm_and_si128(_mm_add_epi16(r0, c0), mask);
         a0  = rpng_paeth_step_epi16(a0, b0, c0, r0, sb, sc, mask);
         rpng_store4_u16_to_u8(dec0, a0);
         c0  = b0;
         a0p = a0;
         for (i = bpp; i + 4 <= pitch; i += bpp)
         {
            __m128i r1;
            b0 = rpng_load4_u8_to_u16(prev + i);
            r0 = rpng_load4_u8_to_u16(raw0 + i);
            sb = _mm_and_si128(_mm_add_epi16(r0, b0), mask);
            sc = _mm_and_si128(_mm_add_epi16(r0, c0), mask);
            a0 = rpng_paeth_step_epi16(a0, b0, c0, r0, sb, sc, mask);
            rpng_store4_u16_to_u8(dec0 + i, a0);
            c0 = b0;
            r1 = rpng_load4_u8_to_u16(raw1 + i - bpp);
            sb = _mm_and_si128(_mm_add_epi16(r1, a0p), mask);
            sc = _mm_and_si128(_mm_add_epi16(r1, c1),  mask);
            a1 = rpng_paeth_step_epi16(a1, a0p, c1, r1, sb, sc, mask);
            rpng_store4_u16_to_u8(dec1 + i - bpp, a1);
            c1  = a0p;
            a0p = a0;
         }
         {
            __m128i r1 = rpng_load4_u8_to_u16(raw1 + i - bpp);
            __m128i s1 = _mm_and_si128(_mm_add_epi16(r1, a0p), mask);
            __m128i s2 = _mm_and_si128(_mm_add_epi16(r1, c1),  mask);
            a1 = rpng_paeth_step_epi16(a1, a0p, c1, r1, s1, s2, mask);
            rpng_store4_u16_to_u8(dec1 + i - bpp, a1);
         }
      }
   }
   else
   {
      if (8 <= pitch)
      {
         __m128i b0 = rpng_load8_u8_to_u16(prev);
         __m128i r0 = rpng_load8_u8_to_u16(raw0);
         __m128i sb = _mm_and_si128(_mm_add_epi16(r0, b0), mask);
         __m128i sc = _mm_and_si128(_mm_add_epi16(r0, c0), mask);
         a0  = rpng_paeth_step_epi16(a0, b0, c0, r0, sb, sc, mask);
         rpng_store8_u16_to_u8(dec0, a0);
         c0  = b0;
         a0p = a0;
         for (i = bpp; i + 8 <= pitch; i += bpp)
         {
            __m128i r1;
            b0 = rpng_load8_u8_to_u16(prev + i);
            r0 = rpng_load8_u8_to_u16(raw0 + i);
            sb = _mm_and_si128(_mm_add_epi16(r0, b0), mask);
            sc = _mm_and_si128(_mm_add_epi16(r0, c0), mask);
            a0 = rpng_paeth_step_epi16(a0, b0, c0, r0, sb, sc, mask);
            rpng_store8_u16_to_u8(dec0 + i, a0);
            c0 = b0;
            r1 = rpng_load8_u8_to_u16(raw1 + i - bpp);
            sb = _mm_and_si128(_mm_add_epi16(r1, a0p), mask);
            sc = _mm_and_si128(_mm_add_epi16(r1, c1),  mask);
            a1 = rpng_paeth_step_epi16(a1, a0p, c1, r1, sb, sc, mask);
            rpng_store8_u16_to_u8(dec1 + i - bpp, a1);
            c1  = a0p;
            a0p = a0;
         }
         {
            __m128i r1 = rpng_load8_u8_to_u16(raw1 + i - bpp);
            __m128i s1 = _mm_and_si128(_mm_add_epi16(r1, a0p), mask);
            __m128i s2 = _mm_and_si128(_mm_add_epi16(r1, c1),  mask);
            a1 = rpng_paeth_step_epi16(a1, a0p, c1, r1, s1, s2, mask);
            rpng_store8_u16_to_u8(dec1 + i - bpp, a1);
         }
      }
   }
   {
      size_t j;
      for (j = i; j < pitch; j++)
      {
         if (j < bpp)
            dec0[j] = (uint8_t)(raw0[j] + prev[j]);
         else
            dec0[j] = (uint8_t)(raw0[j] + rpng_paeth(dec0[j - bpp],
                  prev[j], prev[j - bpp]));
      }
      for (j = i; j < pitch; j++)
      {
         if (j < bpp)
            dec1[j] = (uint8_t)(raw1[j] + dec0[j]);
         else
            dec1[j] = (uint8_t)(raw1[j] + rpng_paeth(dec1[j - bpp],
                  dec0[j], dec0[j - bpp]));
      }
   }
}

static void rpng_filter_avg_pair(uint8_t *dec0, uint8_t *dec1,
      const uint8_t *raw0, const uint8_t *raw1, const uint8_t *prev,
      size_t pitch, unsigned bpp)
{
   size_t i          = 0;
   const __m128i one = _mm_set1_epi8(1);
   __m128i a0        = _mm_setzero_si128();
   __m128i a1        = _mm_setzero_si128();
   __m128i a0p       = _mm_setzero_si128();
   if (bpp <= 4)
   {
      if (4 <= pitch)
      {
         __m128i b0 = rpng_load4_u8(prev);
         __m128i av = _mm_sub_epi8(_mm_avg_epu8(a0, b0),
               _mm_and_si128(_mm_xor_si128(a0, b0), one));
         a0  = _mm_add_epi8(rpng_load4_u8(raw0), av);
         rpng_store4_u8(dec0, a0);
         a0p = a0;
         for (i = bpp; i + 4 <= pitch; i += bpp)
         {
            b0 = rpng_load4_u8(prev + i);
            av = _mm_sub_epi8(_mm_avg_epu8(a0, b0),
                  _mm_and_si128(_mm_xor_si128(a0, b0), one));
            a0 = _mm_add_epi8(rpng_load4_u8(raw0 + i), av);
            rpng_store4_u8(dec0 + i, a0);
            av = _mm_sub_epi8(_mm_avg_epu8(a1, a0p),
                  _mm_and_si128(_mm_xor_si128(a1, a0p), one));
            a1 = _mm_add_epi8(rpng_load4_u8(raw1 + i - bpp), av);
            rpng_store4_u8(dec1 + i - bpp, a1);
            a0p = a0;
         }
         {
            __m128i av1 = _mm_sub_epi8(_mm_avg_epu8(a1, a0p),
                  _mm_and_si128(_mm_xor_si128(a1, a0p), one));
            a1 = _mm_add_epi8(rpng_load4_u8(raw1 + i - bpp), av1);
            rpng_store4_u8(dec1 + i - bpp, a1);
         }
      }
   }
   else
   {
      if (8 <= pitch)
      {
         __m128i b0 = _mm_loadl_epi64((const __m128i*)prev);
         __m128i av = _mm_sub_epi8(_mm_avg_epu8(a0, b0),
               _mm_and_si128(_mm_xor_si128(a0, b0), one));
         a0  = _mm_add_epi8(_mm_loadl_epi64((const __m128i*)raw0), av);
         _mm_storel_epi64((__m128i*)dec0, a0);
         a0p = a0;
         for (i = bpp; i + 8 <= pitch; i += bpp)
         {
            b0 = _mm_loadl_epi64((const __m128i*)(prev + i));
            av = _mm_sub_epi8(_mm_avg_epu8(a0, b0),
                  _mm_and_si128(_mm_xor_si128(a0, b0), one));
            a0 = _mm_add_epi8(
                  _mm_loadl_epi64((const __m128i*)(raw0 + i)), av);
            _mm_storel_epi64((__m128i*)(dec0 + i), a0);
            av = _mm_sub_epi8(_mm_avg_epu8(a1, a0p),
                  _mm_and_si128(_mm_xor_si128(a1, a0p), one));
            a1 = _mm_add_epi8(
                  _mm_loadl_epi64((const __m128i*)(raw1 + i - bpp)), av);
            _mm_storel_epi64((__m128i*)(dec1 + i - bpp), a1);
            a0p = a0;
         }
         {
            __m128i av1 = _mm_sub_epi8(_mm_avg_epu8(a1, a0p),
                  _mm_and_si128(_mm_xor_si128(a1, a0p), one));
            a1 = _mm_add_epi8(
                  _mm_loadl_epi64((const __m128i*)(raw1 + i - bpp)), av1);
            _mm_storel_epi64((__m128i*)(dec1 + i - bpp), a1);
         }
      }
   }
   {
      size_t j;
      for (j = i; j < pitch; j++)
      {
         if (j < bpp)
            dec0[j] = (uint8_t)(raw0[j] + (prev[j] >> 1));
         else
            dec0[j] = (uint8_t)(raw0[j]
                  + ((dec0[j - bpp] + prev[j]) >> 1));
      }
      for (j = i; j < pitch; j++)
      {
         if (j < bpp)
            dec1[j] = (uint8_t)(raw1[j] + (dec0[j] >> 1));
         else
            dec1[j] = (uint8_t)(raw1[j]
                  + ((dec1[j - bpp] + dec0[j]) >> 1));
      }
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

/* u8-lane Sub/Average, mirroring the SSE2 side.  NEON's vhadd_u8 is
 * already floor((a + b) / 2), so Average needs no rounding correction
 * here. */
static INLINE uint8x8_t rpng_load4_u8(const uint8_t *p)
{
   uint32_t v;
   memcpy(&v, p, sizeof(v));
   return vreinterpret_u8_u32(vdup_n_u32(v));
}

static INLINE void rpng_store4_u8(uint8_t *p, uint8x8_t v)
{
   uint32_t word = vget_lane_u32(vreinterpret_u32_u8(v), 0);
   memcpy(p, &word, sizeof(word));
}

static void rpng_filter_sub_prefix1(uint8_t *decoded,
      const uint8_t *raw, size_t pitch)
{
   size_t i         = 0;
   uint8x16_t carry = vdupq_n_u8(0);
   const uint8x16_t zero = vdupq_n_u8(0);
   for (; i + 16 <= pitch; i += 16)
   {
      uint8x16_t x = vld1q_u8(raw + i);
      x = vaddq_u8(x, vextq_u8(zero, x, 15));
      x = vaddq_u8(x, vextq_u8(zero, x, 14));
      x = vaddq_u8(x, vextq_u8(zero, x, 12));
      x = vaddq_u8(x, vextq_u8(zero, x, 8));
      x = vaddq_u8(x, carry);
      vst1q_u8(decoded + i, x);
      carry = vdupq_lane_u8(vget_high_u8(x), 7);
   }
   {
      uint8_t c = i ? decoded[i - 1] : 0;
      for (; i < pitch; i++)
      {
         c          = (uint8_t)(raw[i] + c);
         decoded[i] = c;
      }
   }
}

static void rpng_filter_sub_prefix2(uint8_t *decoded,
      const uint8_t *raw, size_t pitch)
{
   size_t i         = 0;
   uint8x16_t carry = vdupq_n_u8(0);
   const uint8x16_t zero = vdupq_n_u8(0);
   for (; i + 16 <= pitch; i += 16)
   {
      uint8x16_t x = vld1q_u8(raw + i);
      x = vaddq_u8(x, vextq_u8(zero, x, 14));
      x = vaddq_u8(x, vextq_u8(zero, x, 12));
      x = vaddq_u8(x, vextq_u8(zero, x, 8));
      x = vaddq_u8(x, carry);
      vst1q_u8(decoded + i, x);
      carry = vreinterpretq_u8_u16(vdupq_lane_u16(
            vreinterpret_u16_u8(vget_high_u8(x)), 3));
   }
   for (; i < pitch; i++)
      decoded[i] = (uint8_t)(raw[i] + (i >= 2 ? decoded[i - 2] : 0));
}

static void rpng_filter_sub_prefix4(uint8_t *decoded,
      const uint8_t *raw, size_t pitch)
{
   size_t i         = 0;
   uint8x16_t carry = vdupq_n_u8(0);
   const uint8x16_t zero = vdupq_n_u8(0);
   for (; i + 16 <= pitch; i += 16)
   {
      uint8x16_t x = vld1q_u8(raw + i);
      x = vaddq_u8(x, vextq_u8(zero, x, 12));
      x = vaddq_u8(x, vextq_u8(zero, x, 8));
      x = vaddq_u8(x, carry);
      vst1q_u8(decoded + i, x);
      carry = vreinterpretq_u8_u32(vdupq_lane_u32(
            vreinterpret_u32_u8(vget_high_u8(x)), 1));
   }
   for (; i < pitch; i++)
      decoded[i] = (uint8_t)(raw[i] + (i >= 4 ? decoded[i - 4] : 0));
}

static void rpng_filter_sub_simd(uint8_t *decoded,
      const uint8_t *raw, size_t pitch, unsigned bpp)
{
   size_t i             = 0;
   uint8x8_t prev_pixel = vdup_n_u8(0);
   switch (bpp)
   {
      case 1:
         rpng_filter_sub_prefix1(decoded, raw, pitch);
         return;
      case 2:
         rpng_filter_sub_prefix2(decoded, raw, pitch);
         return;
      case 4:
         rpng_filter_sub_prefix4(decoded, raw, pitch);
         return;
   }
   if (bpp <= 4)
   {
      for (; i + 4 <= pitch; i += bpp)
      {
         uint8x8_t out = vadd_u8(rpng_load4_u8(raw + i), prev_pixel);
         rpng_store4_u8(decoded + i, out);
         prev_pixel    = out;
      }
   }
   else
   {
      for (; i + 8 <= pitch; i += bpp)
      {
         uint8x8_t out = vadd_u8(vld1_u8(raw + i), prev_pixel);
         vst1_u8(decoded + i, out);
         prev_pixel    = out;
      }
   }
   if (decoded != raw)
      memcpy(decoded + i, raw + i, pitch - i);
   if (i < bpp)
      i = bpp < pitch ? bpp : pitch;
   for (; i < pitch; i++)
      decoded[i] += decoded[i - bpp];
}

static void rpng_filter_avg_simd(uint8_t *decoded,
      const uint8_t *raw, const uint8_t *prev, size_t pitch, unsigned bpp)
{
   size_t i             = 0;
   uint8x8_t prev_pixel = vdup_n_u8(0);
   if (bpp <= 4)
   {
      for (; i + 4 <= pitch; i += bpp)
      {
         uint8x8_t pv  = rpng_load4_u8(prev + i);
         uint8x8_t out = vadd_u8(rpng_load4_u8(raw + i),
               vhadd_u8(prev_pixel, pv));
         rpng_store4_u8(decoded + i, out);
         prev_pixel    = out;
      }
   }
   else
   {
      for (; i + 8 <= pitch; i += bpp)
      {
         uint8x8_t pv  = vld1_u8(prev + i);
         uint8x8_t out = vadd_u8(vld1_u8(raw + i),
               vhadd_u8(prev_pixel, pv));
         vst1_u8(decoded + i, out);
         prev_pixel    = out;
      }
   }
   if (decoded != raw)
      memcpy(decoded + i, raw + i, pitch - i);
   for (; i < bpp && i < pitch; i++)
      decoded[i] += prev[i] >> 1;
   for (; i < pitch; i++)
      decoded[i] += (uint8_t)((decoded[i - bpp] + prev[i]) >> 1);
}

/* One Paeth pixel, selecting among speculative sums; see the SSE2
 * side for the derivation.  The masked sums sb = (r + b) & FF and
 * sc = (r + c) & FF do not involve a, so they run off the carried
 * chain, and pa > min(pb, pc) replaces the two-compare-plus-or
 * "don't pick a" test. */
static INLINE uint16x4_t rpng_paeth_step_u16(
      uint16x4_t a, uint16x4_t b, uint16x4_t c,
      uint16x4_t r, uint16x4_t sb, uint16x4_t sc, uint16x4_t mask)
{
   int16x4_t bc = vsub_s16(vreinterpret_s16_u16(b), vreinterpret_s16_u16(c));
   int16x4_t ac = vsub_s16(vreinterpret_s16_u16(a), vreinterpret_s16_u16(c));
   int16x4_t sm = vadd_s16(bc, ac);
   uint16x4_t pa = vreinterpret_u16_s16(vabs_s16(bc));
   uint16x4_t pb = vreinterpret_u16_s16(vabs_s16(ac));
   uint16x4_t pc = vreinterpret_u16_s16(vabs_s16(sm));
   uint16x4_t not_a  = vcgt_u16(pa, vmin_u16(pb, pc));
   uint16x4_t pick_c = vcgt_u16(pb, pc);
   uint16x4_t sa     = vand_u16(vadd_u16(r, a), mask);
   uint16x4_t bc_sel = vbsl_u16(pick_c, sc, sb);
   return              vbsl_u16(not_a,  bc_sel, sa);
}

/* 8-lane forms of the load/store/predict trio, mirroring the SSE2 side. */
static INLINE uint16x8_t rpng_load8_u8_to_u16(const uint8_t *p)
{
   return vmovl_u8(vld1_u8(p));
}

static INLINE void rpng_store8_u16_to_u8(uint8_t *p, uint16x8_t v)
{
   vst1_u8(p, vmovn_u16(v));
}

static INLINE uint16x8_t rpng_paeth_step_u16q(
      uint16x8_t a, uint16x8_t b, uint16x8_t c,
      uint16x8_t r, uint16x8_t sb, uint16x8_t sc, uint16x8_t mask)
{
   int16x8_t bc      = vsubq_s16(vreinterpretq_s16_u16(b),
                                 vreinterpretq_s16_u16(c));
   int16x8_t ac      = vsubq_s16(vreinterpretq_s16_u16(a),
                                 vreinterpretq_s16_u16(c));
   int16x8_t sm      = vaddq_s16(bc, ac);
   uint16x8_t pa     = vreinterpretq_u16_s16(vabsq_s16(bc));
   uint16x8_t pb     = vreinterpretq_u16_s16(vabsq_s16(ac));
   uint16x8_t pc     = vreinterpretq_u16_s16(vabsq_s16(sm));
   uint16x8_t not_a  = vcgtq_u16(pa, vminq_u16(pb, pc));
   uint16x8_t pick_c = vcgtq_u16(pb, pc);
   uint16x8_t sa     = vandq_u16(vaddq_u16(r, a), mask);
   uint16x8_t bc_sel = vbslq_u16(pick_c, sc, sb);
   return              vbslq_u16(not_a,  bc_sel, sa);
}

/* See the SSE2 rpng_filter_paeth_simd above for why an 8-byte window can
 * be stepped by an arbitrary stride, and why the guard is i + 8. */
static void rpng_filter_paeth_simd(uint8_t *decoded,
      const uint8_t *raw, const uint8_t *prev, size_t pitch, unsigned bpp)
{
   size_t i                   = 0;
   uint16x8_t prev_pixel      = vdupq_n_u16(0);
   uint16x8_t prev_upper_left = vdupq_n_u16(0);
   const uint16x8_t mask      = vdupq_n_u16(0xFF);
   if (bpp <= 4)
   {
      uint16x4_t pp  = vdup_n_u16(0);
      uint16x4_t pul = vdup_n_u16(0);
      const uint16x4_t m4 = vdup_n_u16(0xFF);
      for (; i + 4 <= pitch; i += bpp)
      {
         uint16x4_t r    = rpng_load4_u8_to_u16(raw  + i);
         uint16x4_t pv   = rpng_load4_u8_to_u16(prev + i);
         uint16x4_t sb   = vand_u16(vadd_u16(r, pv),  m4);
         uint16x4_t sc   = vand_u16(vadd_u16(r, pul), m4);
         uint16x4_t out  = rpng_paeth_step_u16(pp, pv, pul, r, sb, sc, m4);
         rpng_store4_u16_to_u8(decoded + i, out);
         pp  = out;
         pul = pv;
      }
   }
   else
   {
      for (; i + 8 <= pitch; i += bpp)
      {
         uint16x8_t r    = rpng_load8_u8_to_u16(raw  + i);
         uint16x8_t pv   = rpng_load8_u8_to_u16(prev + i);
         uint16x8_t sb   = vandq_u16(vaddq_u16(r, pv), mask);
         uint16x8_t sc   = vandq_u16(vaddq_u16(r, prev_upper_left), mask);
         uint16x8_t out  = rpng_paeth_step_u16q(
               prev_pixel, pv, prev_upper_left, r, sb, sc, mask);
         rpng_store8_u16_to_u8(decoded + i, out);
         prev_pixel      = out;
         prev_upper_left = pv;
      }
   }
   if (decoded != raw)
      memcpy(decoded + i, raw + i, pitch - i);
   /* A scanline shorter than the vector window leaves i == 0, so the
    * leading bpp bytes still have to go through the a == c == 0 case
    * (Paeth(0, b, 0) == b) before the general tail can index i - bpp. */
   for (; i < bpp && i < pitch; i++)
      decoded[i] += prev[i];
   for (; i < pitch; i++)
      decoded[i] += (uint8_t)rpng_paeth(decoded[i - bpp], prev[i],
            prev[i - bpp]);
}

#define RPNG_HAVE_PAIR_KERNELS 1

/* Wavefront pair kernels; see the SSE2 side for the derivation.  Row 1
 * trails row 0 by one step, so its up neighbour comes from a register
 * written in the previous iteration and the two serial chains overlap.
 * NEON's vhadd_u8 is floor((a + b) / 2) directly, so the Average pair
 * needs no rounding correction. */
static void rpng_filter_paeth_pair(uint8_t *dec0, uint8_t *dec1,
      const uint8_t *raw0, const uint8_t *raw1, const uint8_t *prev,
      size_t pitch, unsigned bpp)
{
   size_t i = 0;
   if (bpp <= 4)
   {
      const uint16x4_t m4 = vdup_n_u16(0xFF);
      uint16x4_t a0  = vdup_n_u16(0);
      uint16x4_t c0  = vdup_n_u16(0);
      uint16x4_t a1  = vdup_n_u16(0);
      uint16x4_t c1  = vdup_n_u16(0);
      uint16x4_t a0p = vdup_n_u16(0);
      if (4 <= pitch)
      {
         uint16x4_t b0 = rpng_load4_u8_to_u16(prev);
         uint16x4_t r0 = rpng_load4_u8_to_u16(raw0);
         uint16x4_t sb = vand_u16(vadd_u16(r0, b0), m4);
         uint16x4_t sc = vand_u16(vadd_u16(r0, c0), m4);
         a0  = rpng_paeth_step_u16(a0, b0, c0, r0, sb, sc, m4);
         rpng_store4_u16_to_u8(dec0, a0);
         c0  = b0;
         a0p = a0;
         for (i = bpp; i + 4 <= pitch; i += bpp)
         {
            uint16x4_t r1;
            b0 = rpng_load4_u8_to_u16(prev + i);
            r0 = rpng_load4_u8_to_u16(raw0 + i);
            sb = vand_u16(vadd_u16(r0, b0), m4);
            sc = vand_u16(vadd_u16(r0, c0), m4);
            a0 = rpng_paeth_step_u16(a0, b0, c0, r0, sb, sc, m4);
            rpng_store4_u16_to_u8(dec0 + i, a0);
            c0 = b0;
            r1 = rpng_load4_u8_to_u16(raw1 + i - bpp);
            sb = vand_u16(vadd_u16(r1, a0p), m4);
            sc = vand_u16(vadd_u16(r1, c1),  m4);
            a1 = rpng_paeth_step_u16(a1, a0p, c1, r1, sb, sc, m4);
            rpng_store4_u16_to_u8(dec1 + i - bpp, a1);
            c1  = a0p;
            a0p = a0;
         }
         {
            uint16x4_t r1 = rpng_load4_u8_to_u16(raw1 + i - bpp);
            uint16x4_t s1 = vand_u16(vadd_u16(r1, a0p), m4);
            uint16x4_t s2 = vand_u16(vadd_u16(r1, c1),  m4);
            a1 = rpng_paeth_step_u16(a1, a0p, c1, r1, s1, s2, m4);
            rpng_store4_u16_to_u8(dec1 + i - bpp, a1);
         }
      }
   }
   else
   {
      const uint16x8_t m8 = vdupq_n_u16(0xFF);
      uint16x8_t a0  = vdupq_n_u16(0);
      uint16x8_t c0  = vdupq_n_u16(0);
      uint16x8_t a1  = vdupq_n_u16(0);
      uint16x8_t c1  = vdupq_n_u16(0);
      uint16x8_t a0p = vdupq_n_u16(0);
      if (8 <= pitch)
      {
         uint16x8_t b0 = rpng_load8_u8_to_u16(prev);
         uint16x8_t r0 = rpng_load8_u8_to_u16(raw0);
         uint16x8_t sb = vandq_u16(vaddq_u16(r0, b0), m8);
         uint16x8_t sc = vandq_u16(vaddq_u16(r0, c0), m8);
         a0  = rpng_paeth_step_u16q(a0, b0, c0, r0, sb, sc, m8);
         rpng_store8_u16_to_u8(dec0, a0);
         c0  = b0;
         a0p = a0;
         for (i = bpp; i + 8 <= pitch; i += bpp)
         {
            uint16x8_t r1;
            b0 = rpng_load8_u8_to_u16(prev + i);
            r0 = rpng_load8_u8_to_u16(raw0 + i);
            sb = vandq_u16(vaddq_u16(r0, b0), m8);
            sc = vandq_u16(vaddq_u16(r0, c0), m8);
            a0 = rpng_paeth_step_u16q(a0, b0, c0, r0, sb, sc, m8);
            rpng_store8_u16_to_u8(dec0 + i, a0);
            c0 = b0;
            r1 = rpng_load8_u8_to_u16(raw1 + i - bpp);
            sb = vandq_u16(vaddq_u16(r1, a0p), m8);
            sc = vandq_u16(vaddq_u16(r1, c1),  m8);
            a1 = rpng_paeth_step_u16q(a1, a0p, c1, r1, sb, sc, m8);
            rpng_store8_u16_to_u8(dec1 + i - bpp, a1);
            c1  = a0p;
            a0p = a0;
         }
         {
            uint16x8_t r1 = rpng_load8_u8_to_u16(raw1 + i - bpp);
            uint16x8_t s1 = vandq_u16(vaddq_u16(r1, a0p), m8);
            uint16x8_t s2 = vandq_u16(vaddq_u16(r1, c1),  m8);
            a1 = rpng_paeth_step_u16q(a1, a0p, c1, r1, s1, s2, m8);
            rpng_store8_u16_to_u8(dec1 + i - bpp, a1);
         }
      }
   }
   {
      size_t j;
      for (j = i; j < pitch; j++)
      {
         if (j < bpp)
            dec0[j] = (uint8_t)(raw0[j] + prev[j]);
         else
            dec0[j] = (uint8_t)(raw0[j] + rpng_paeth(dec0[j - bpp],
                  prev[j], prev[j - bpp]));
      }
      for (j = i; j < pitch; j++)
      {
         if (j < bpp)
            dec1[j] = (uint8_t)(raw1[j] + dec0[j]);
         else
            dec1[j] = (uint8_t)(raw1[j] + rpng_paeth(dec1[j - bpp],
                  dec0[j], dec0[j - bpp]));
      }
   }
}

static void rpng_filter_avg_pair(uint8_t *dec0, uint8_t *dec1,
      const uint8_t *raw0, const uint8_t *raw1, const uint8_t *prev,
      size_t pitch, unsigned bpp)
{
   size_t i       = 0;
   uint8x8_t a0   = vdup_n_u8(0);
   uint8x8_t a1   = vdup_n_u8(0);
   uint8x8_t a0p  = vdup_n_u8(0);
   if (bpp <= 4)
   {
      if (4 <= pitch)
      {
         uint8x8_t b0 = rpng_load4_u8(prev);
         a0  = vadd_u8(rpng_load4_u8(raw0), vhadd_u8(a0, b0));
         rpng_store4_u8(dec0, a0);
         a0p = a0;
         for (i = bpp; i + 4 <= pitch; i += bpp)
         {
            b0 = rpng_load4_u8(prev + i);
            a0 = vadd_u8(rpng_load4_u8(raw0 + i), vhadd_u8(a0, b0));
            rpng_store4_u8(dec0 + i, a0);
            a1 = vadd_u8(rpng_load4_u8(raw1 + i - bpp),
                  vhadd_u8(a1, a0p));
            rpng_store4_u8(dec1 + i - bpp, a1);
            a0p = a0;
         }
         a1 = vadd_u8(rpng_load4_u8(raw1 + i - bpp), vhadd_u8(a1, a0p));
         rpng_store4_u8(dec1 + i - bpp, a1);
      }
   }
   else
   {
      if (8 <= pitch)
      {
         uint8x8_t b0 = vld1_u8(prev);
         a0  = vadd_u8(vld1_u8(raw0), vhadd_u8(a0, b0));
         vst1_u8(dec0, a0);
         a0p = a0;
         for (i = bpp; i + 8 <= pitch; i += bpp)
         {
            b0 = vld1_u8(prev + i);
            a0 = vadd_u8(vld1_u8(raw0 + i), vhadd_u8(a0, b0));
            vst1_u8(dec0 + i, a0);
            a1 = vadd_u8(vld1_u8(raw1 + i - bpp), vhadd_u8(a1, a0p));
            vst1_u8(dec1 + i - bpp, a1);
            a0p = a0;
         }
         a1 = vadd_u8(vld1_u8(raw1 + i - bpp), vhadd_u8(a1, a0p));
         vst1_u8(dec1 + i - bpp, a1);
      }
   }
   {
      size_t j;
      for (j = i; j < pitch; j++)
      {
         if (j < bpp)
            dec0[j] = (uint8_t)(raw0[j] + (prev[j] >> 1));
         else
            dec0[j] = (uint8_t)(raw0[j]
                  + ((dec0[j - bpp] + prev[j]) >> 1));
      }
      for (j = i; j < pitch; j++)
      {
         if (j < bpp)
            dec1[j] = (uint8_t)(raw1[j] + (dec0[j] >> 1));
         else
            dec1[j] = (uint8_t)(raw1[j]
                  + ((dec1[j - bpp] + dec0[j]) >> 1));
      }
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

/* Greyscale expansion, vector fast paths for the byte-aligned depths.
 * A grey sample g becomes the word (0xFF<<24)|(g<<16)|(g<<8)|g; ORing
 * the splatted word with 0xFF000000 forces the alpha lane regardless
 * of g.  Depth 16 keeps the high byte only, matching the scalar path
 * (PNG stores samples big-endian, so the high byte is the even one).
 * Sub-byte depths (1/2/4) stay on the scalar bit walker: they multiply
 * out through mul_table and are rare enough not to matter. */
#if defined(RPNG_SIMD_SSE2)
static unsigned rpng_copy_line_bw8_sse2(uint32_t *data,
      const uint8_t *decoded, unsigned width)
{
   unsigned i = 0;
   const __m128i amask = _mm_set1_epi32((int)0xFF000000u);
   for (; (int)(width - i) >= 16; i += 16)
   {
      __m128i g  = _mm_loadu_si128((const __m128i*)(decoded + i));
      __m128i lo = _mm_unpacklo_epi8(g, g);  /* g0 g0 g1 g1 ..  */
      __m128i hi = _mm_unpackhi_epi8(g, g);
      _mm_storeu_si128((__m128i*)(data + i + 0),
            _mm_or_si128(_mm_unpacklo_epi16(lo, lo), amask));
      _mm_storeu_si128((__m128i*)(data + i + 4),
            _mm_or_si128(_mm_unpackhi_epi16(lo, lo), amask));
      _mm_storeu_si128((__m128i*)(data + i + 8),
            _mm_or_si128(_mm_unpacklo_epi16(hi, hi), amask));
      _mm_storeu_si128((__m128i*)(data + i + 12),
            _mm_or_si128(_mm_unpackhi_epi16(hi, hi), amask));
   }
   return i;
}

static unsigned rpng_copy_line_bw16_sse2(uint32_t *data,
      const uint8_t *decoded, unsigned width)
{
   unsigned i = 0;
   const __m128i amask = _mm_set1_epi32((int)0xFF000000u);
   const __m128i bmask = _mm_set1_epi16(0x00FF);
   for (; (int)(width - i) >= 8; i += 8)
   {
      /* 8 big-endian 16-bit samples: high bytes sit in the low byte
       * of each LE 16-bit lane. */
      __m128i v  = _mm_loadu_si128((const __m128i*)(decoded + (size_t)i * 2));
      __m128i g  = _mm_packus_epi16(_mm_and_si128(v, bmask), _mm_setzero_si128());
      __m128i lo = _mm_unpacklo_epi8(g, g);
      _mm_storeu_si128((__m128i*)(data + i + 0),
            _mm_or_si128(_mm_unpacklo_epi16(lo, lo), amask));
      _mm_storeu_si128((__m128i*)(data + i + 4),
            _mm_or_si128(_mm_unpackhi_epi16(lo, lo), amask));
   }
   return i;
}
#elif defined(RPNG_SIMD_NEON)
static unsigned rpng_copy_line_bw8_neon(uint32_t *data,
      const uint8_t *decoded, unsigned width)
{
   unsigned i = 0;
   for (; (int)(width - i) >= 8; i += 8)
   {
      uint8x8x4_t o;
      uint8x8_t g = vld1_u8(decoded + i);
      o.val[0]    = g;
      o.val[1]    = g;
      o.val[2]    = g;
      o.val[3]    = vdup_n_u8(0xFF);
      vst4_u8((uint8_t*)(data + i), o);
   }
   return i;
}

static unsigned rpng_copy_line_bw16_neon(uint32_t *data,
      const uint8_t *decoded, unsigned width)
{
   unsigned i = 0;
   for (; (int)(width - i) >= 8; i += 8)
   {
      /* De-interleave (hi,lo) byte pairs; val[0] is the high bytes. */
      uint8x8x2_t v = vld2_u8(decoded + (size_t)i * 2);
      uint8x8x4_t o;
      o.val[0]      = v.val[0];
      o.val[1]      = v.val[0];
      o.val[2]      = v.val[0];
      o.val[3]      = vdup_n_u8(0xFF);
      vst4_u8((uint8_t*)(data + i), o);
   }
   return i;
}
#endif /* RPNG_SIMD_SSE2 / RPNG_SIMD_NEON */

static void rpng_reverse_filter_copy_line_bw(uint32_t *data,
      const uint8_t *decoded, unsigned width, unsigned depth)
{
   int i;
   unsigned bit;
   static const unsigned mul_table[] = { 0, 0xff, 0x55, 0, 0x11, 0, 0, 0, 0x01 };
   unsigned mul, mask;

   if (depth == 16)
   {
      unsigned j = 0;
#if defined(RPNG_SIMD_SSE2)
      j = rpng_copy_line_bw16_sse2(data, decoded, width);
#elif defined(RPNG_SIMD_NEON)
      j = rpng_copy_line_bw16_neon(data, decoded, width);
#endif
      for (i = (int)j; i < (int)width; i++)
      {
         uint32_t val = decoded[i << 1];
         data[i]      = (val * 0x010101) | (0xffu << 24);
      }
      return;
   }

   if (depth == 8)
   {
      unsigned j = 0;
#if defined(RPNG_SIMD_SSE2)
      j = rpng_copy_line_bw8_sse2(data, decoded, width);
#elif defined(RPNG_SIMD_NEON)
      j = rpng_copy_line_bw8_neon(data, decoded, width);
#endif
      for (i = (int)j; i < (int)width; i++)
         data[i] = ((uint32_t)decoded[i] * 0x010101) | (0xffu << 24);
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

/* Exact size of an Adam7 stream: the seven passes are seven independent
 * sub-images, each with its own scanline rounded up to a whole byte and
 * its own filter byte per row, so the total is not the non-interlaced
 * size and cannot be approximated from it.
 *
 * It used to be approximated - the non-interlaced size doubled, "to be
 * sure".  That holds at 8 bits and up, where the per-pass rounding adds
 * a byte or two per row, but not below: a 16x1 1-bit image is 3 bytes
 * non-interlaced and 8 interlaced, because all four non-empty passes
 * round their 2-to-8 pixel scanlines up to one byte apiece.  The
 * undersized buffer failed the decode outright, so 1- and 2-bit
 * interlaced images at a range of sizes did not load at all. */
static size_t rpng_adam7_buf_size(const struct png_ihdr *ihdr)
{
   size_t total = 0;
   unsigned i;
   for (i = 0; i < ARRAY_SIZE(rpng_passes); i++)
   {
      struct png_ihdr sub;
      size_t pass_size = 0;
      if (     ihdr->width  <= rpng_passes[i].x
            || ihdr->height <= rpng_passes[i].y)
         continue;   /* empty pass, contributes nothing */
      sub        = *ihdr;
      sub.width  = (ihdr->width  - rpng_passes[i].x
            + rpng_passes[i].stride_x - 1) / rpng_passes[i].stride_x;
      sub.height = (ihdr->height - rpng_passes[i].y
            + rpng_passes[i].stride_y - 1) / rpng_passes[i].stride_y;
      rpng_pass_geom(&sub, sub.width, sub.height, NULL, NULL, &pass_size);
      total += pass_size;
   }
   return total;
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
   if (pngp->scratch[0])
      free(pngp->scratch[0]);
   if (pngp->scratch[1])
      free(pngp->scratch[1]);
   if (pngp->scratch[2])
      free(pngp->scratch[2]);
   pngp->scratch[0] = NULL;
   pngp->scratch[1] = NULL;
   pngp->scratch[2] = NULL;
   pngp->prev_line  = NULL;

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
         /* Pass not fully inflated yet.  While the stream is still
          * being fed this is a wait, not an error; once the stream
          * has ended it is a truncation (and the inflate end path
          * has already refused an under-produced stream, so this
          * arm is belt and braces). */
         free(pngp->data);
         pngp->data = NULL;
         if (!(pngp->flags & RPNG_PROCESS_FLAG_INFLATE_INITIALIZED))
            return RPNG_FILTER_WAIT;
         return -1;
      }

      pngp->flags |= RPNG_PROCESS_FLAG_ADAM7_PASS_INITIALIZED;

      return 0;
   }

   if (pngp->flags & RPNG_PROCESS_FLAG_PASS_INITIALIZED)
      return 0;

   rpng_pass_geom(ihdr, ihdr->width, ihdr->height, &pngp->bpp, &pngp->pitch, &pass_size);

   /* Interleaved decode: this init now runs after the first inflate
    * slice rather than after the whole stream, so the whole-image
    * sufficiency check only applies once the stream has ended (the
    * inflate end path enforces full production; this arm keeps the
    * old failure for a completed-but-short stream). */
   if (      pngp->total_out < pass_size
         && (pngp->flags & RPNG_PROCESS_FLAG_INFLATE_INITIALIZED))
      return -1;

   pngp->restore_buf_size      = 0;
   pngp->held                  = 0;
   /* scratch[0] doubles as row 0's zero prev; scratch_cur starts at 0
    * so the first out-of-place line (cur ^= 1) lands in scratch[1],
    * leaving the zeroes intact while anything still points at them. */
   pngp->scratch[0]            = (uint8_t*)calloc(1, pngp->pitch);
   pngp->scratch[1]            = (uint8_t*)malloc(pngp->pitch);
   pngp->scratch[2]            = (uint8_t*)malloc(pngp->pitch);
   pngp->scratch_cur           = 0;

   if (!pngp->scratch[0] || !pngp->scratch[1] || !pngp->scratch[2])
      goto error;
   pngp->prev_line             = pngp->scratch[0];

   pngp->h                    = 0;
   pngp->flags               |= RPNG_PROCESS_FLAG_PASS_INITIALIZED;

   return 0;

error:
   rpng_reverse_filter_deinit(pngp);
   return -1;
}

/* ---------------------------------------------------------------------------*/

/* Pixel-format conversion of one unfiltered scanline into the 32-bit
 * output row, factored out of the per-line path so the wavefront pair
 * path below can convert two rows from two buffers. */
static void rpng_reverse_filter_convert(uint32_t *data,
      const struct png_ihdr *ihdr,
      struct rpng_process *pngp, const uint8_t *decoded)
{
   switch (ihdr->color_type)
   {
      case PNG_IHDR_COLOR_GRAY:
         rpng_reverse_filter_copy_line_bw(data, decoded, ihdr->width,
               ihdr->depth);
         break;
      case PNG_IHDR_COLOR_RGB:
         rpng_reverse_filter_copy_line_rgb(data, decoded, ihdr->width,
               ihdr->depth, pngp->supports_rgba, pngp->want_10bit);
         break;
      case PNG_IHDR_COLOR_PLT:
         rpng_reverse_filter_copy_line_plt(
               data, decoded, ihdr->width, ihdr->depth, pngp->palette);
         break;
      case PNG_IHDR_COLOR_GRAY_ALPHA:
         rpng_reverse_filter_copy_line_gray_alpha(
               data, decoded, ihdr->width, ihdr->depth);
         break;
      case PNG_IHDR_COLOR_RGBA:
         rpng_reverse_filter_copy_line_rgba(
               data, decoded, ihdr->width, ihdr->depth,
               pngp->supports_rgba);
         break;
   }
}

static int rpng_reverse_filter_copy_line(uint32_t *data,
      const struct png_ihdr *ihdr,
      struct rpng_process *pngp, unsigned filter)
{
   /* In-place when the kernel tolerates dec aliasing raw: None does
    * no work, Up and the prefix Subs load before every store, and at
    * stride == window the window kernels' spill lanes land exactly on
    * their own next input.  At stride < window a wide store would
    * clobber raw bytes the next step re-reads, and width-exact stores
    * measure worse (a 1-3 byte store feeding an overlapping 4-byte
    * load stalls forwarding every pixel), so those combinations keep
    * their wide stores and decode into alternating scratch lines
    * instead. */
   uint8_t       *dec = pngp->inflate_buf;
   const uint8_t *raw = pngp->inflate_buf;
#if defined(RPNG_SIMD_SSE2) || defined(RPNG_SIMD_NEON)
   if (!(   filter == PNG_FILTER_NONE
         || filter == PNG_FILTER_UP
         || (filter == PNG_FILTER_SUB
               && pngp->bpp != 3 && pngp->bpp != 6)
         || (pngp->bpp == 4 || pngp->bpp == 8)))
   {
      pngp->scratch_cur = (pngp->scratch_cur + 1) % 3;
      dec = pngp->scratch[pngp->scratch_cur];
   }
#endif
#if !defined(RPNG_SIMD_SSE2) && !defined(RPNG_SIMD_NEON)
   unsigned i;
#endif

   switch (filter)
   {
      case PNG_FILTER_NONE:
         break;
      case PNG_FILTER_SUB:
#if defined(RPNG_SIMD_SSE2) || defined(RPNG_SIMD_NEON)
         rpng_filter_sub_simd(dec, raw, pngp->pitch, pngp->bpp);
         break;
#else
         for (i = pngp->bpp; i < pngp->pitch; i++)
            dec[i] += dec[i - pngp->bpp];
         break;
#endif
      case PNG_FILTER_UP:
         /* Filter Up is a pure vector add—no inter-byte dependency. */
         rpng_filter_up(dec, raw, pngp->prev_line, pngp->pitch);
         break;
      case PNG_FILTER_AVERAGE:
#if defined(RPNG_SIMD_SSE2) || defined(RPNG_SIMD_NEON)
         rpng_filter_avg_simd(dec, raw, pngp->prev_line,
               pngp->pitch, pngp->bpp);
         break;
#else
         for (i = 0; i < pngp->bpp; i++)
            dec[i] += (uint8_t)(pngp->prev_line[i] >> 1);
         for (i = pngp->bpp; i < pngp->pitch; i++)
            dec[i] += (uint8_t)((dec[i - pngp->bpp]
                  + pngp->prev_line[i]) >> 1);
         break;
#endif
      case PNG_FILTER_PAETH:
#if defined(RPNG_SIMD_SSE2) || defined(RPNG_SIMD_NEON)
         /* Stride-generic: every bpp benefits, not just RGBA8. */
         rpng_filter_paeth_simd(dec, raw, pngp->prev_line,
               pngp->pitch, pngp->bpp);
         break;
#else
         for (i = 0; i < pngp->bpp; i++)
            dec[i] += pngp->prev_line[i];
         for (i = pngp->bpp; i < pngp->pitch; i++)
            dec[i] += (uint8_t)rpng_paeth(dec[i - pngp->bpp],
                  pngp->prev_line[i], pngp->prev_line[i - pngp->bpp]);
         break;
#endif
      default:
         return IMAGE_PROCESS_ERROR_END;
   }

rpng_reverse_filter_convert(data, ihdr, pngp, dec);

   /* The line just unfiltered becomes the previous line; its ring
    * slot stays protected through pngp->held until the next line has
    * consumed it. */
   pngp->prev_line = dec;

   return IMAGE_PROCESS_NEXT;
}

static int rpng_reverse_filter_regular_iterate(
      const struct png_ihdr *ihdr,
      struct rpng_process *pngp)
{
   int ret = IMAGE_PROCESS_END;
#if defined(RPNG_HAVE_PAIR_KERNELS)
   /* Wavefront pair: when the next two scanlines are fully inflated,
    * contiguous in the ring and carry the same chain-bound filter,
    * unfilter them in one interleaved pass (see the pair kernels),
    * in place, the second row's up neighbour being the first row's
    * bytes as they decode.  Residency: total_out - restore_buf_size
    * - held is exactly the inflated, not yet unfiltered byte count on
    * the regular path; the interlaced path only filters once its
    * whole pass is resident, so the check is conservative there.
    * Contiguity: the ring recycles only at whole-line boundaries, so
    * two lines are contiguous unless the second would start past the
    * ring end. */
   if (pngp->h + 2 <= ihdr->height)
   {
      size_t line = (size_t)pngp->pitch + 1;
      if (      pngp->total_out - pngp->restore_buf_size - pngp->held
             >= 2 * line
          && (   pngp->ring_size >= pngp->inflate_buf_size
              ||    pngp->inflate_buf + 2 * line
                 <= pngp->inflate_base + pngp->ring_size))
      {
         unsigned f0 = pngp->inflate_buf[0];
         unsigned f1 = pngp->inflate_buf[line];
         if (   f0 == f1
             && (f0 == PNG_FILTER_PAETH || f0 == PNG_FILTER_AVERAGE))
         {
            const uint8_t *raw0 = pngp->inflate_buf + 1;
            const uint8_t *raw1 = raw0 + line;
            uint8_t *dec0;
            uint8_t *dec1;
            int in_place = (pngp->bpp == 4 || pngp->bpp == 8);
            if (in_place)
            {
               /* Stride == window: the wide stores land exactly on
                * their own next input, so decode in the ring. */
               dec0 = pngp->inflate_buf + 1;
               dec1 = dec0 + line;
            }
            else
            {
               /* Stride < window: wide stores would clobber raw, so
                * take the next two free scratch lines. */
               unsigned i0 = (pngp->scratch_cur + 1) % 3;
               unsigned i1 = (pngp->scratch_cur + 2) % 3;
               dec0 = pngp->scratch[i0];
               dec1 = pngp->scratch[i1];
               pngp->scratch_cur = i1;
            }
            if (f0 == PNG_FILTER_PAETH)
               rpng_filter_paeth_pair(dec0, dec1, raw0, raw1,
                     pngp->prev_line, pngp->pitch, pngp->bpp);
            else
               rpng_filter_avg_pair(dec0, dec1, raw0, raw1,
                     pngp->prev_line, pngp->pitch, pngp->bpp);
            rpng_reverse_filter_convert(pngp->out_cursor, ihdr, pngp,
                  dec0);
            rpng_reverse_filter_convert(pngp->out_cursor + ihdr->width,
                  ihdr, pngp, dec1);
            if (in_place)
            {
               /* Release the line the pair's first row used as prev
                * and the first row itself; the second row is the new
                * prev and stays held so the ring cannot recycle its
                * slot. */
               pngp->restore_buf_size += pngp->held + line;
               pngp->held              = line;
            }
            else
            {
               /* Scratch lines need no ring protection: release both
                * raw lines and anything held. */
               pngp->restore_buf_size += pngp->held + 2 * line;
               pngp->held              = 0;
            }
            pngp->prev_line         = dec1;
            pngp->h                += 2;
            pngp->inflate_buf      += 2 * line;
            if (    pngp->ring_size < pngp->inflate_buf_size
                &&  pngp->inflate_buf == pngp->inflate_base + pngp->ring_size)
               pngp->inflate_buf = pngp->inflate_base;
            pngp->out_cursor    += 2 * (size_t)ihdr->width;
            return IMAGE_PROCESS_NEXT;
         }
      }
   }
#endif
   if (pngp->h < ihdr->height)
   {
      unsigned filter         = *pngp->inflate_buf++;
      ret                     = rpng_reverse_filter_copy_line(pngp->out_cursor,
            ihdr, pngp, filter);
      if (ret == IMAGE_PROCESS_END || ret == IMAGE_PROCESS_ERROR_END)
         goto end;
   }
   else
      goto end;

   pngp->h++;
   /* Consume-late: a line unfiltered in place is the next line's
    * prev, living in the ring, so the previously held line is
    * released and this one held in its stead.  A line decoded into
    * scratch needs no ring protection: release it and any held line
    * at once. */
   if (pngp->prev_line == pngp->inflate_buf)
   {
      pngp->restore_buf_size   += pngp->held;
      pngp->held                = (size_t)pngp->pitch + 1;
   }
   else
   {
      pngp->restore_buf_size   += pngp->held + pngp->pitch + 1;
      pngp->held                = 0;
   }
   pngp->inflate_buf           += pngp->pitch;

   /* Recycling window: the ring is a whole number of scanlines, so
    * the cursor lands exactly on the ring end between lines and
    * never mid-line. */
   if (    pngp->ring_size < pngp->inflate_buf_size
       &&  pngp->inflate_buf == pngp->inflate_base + pngp->ring_size)
      pngp->inflate_buf = pngp->inflate_base;

   pngp->out_cursor            += ihdr->width;

   return IMAGE_PROCESS_NEXT;

end:
   /* Nothing will use the held line as prev any more; fold it into
    * the consumed count so the full-buffer rewind below and the
    * ring's free-space arithmetic both see every processed byte. */
   pngp->restore_buf_size += pngp->held;
   pngp->held              = 0;
   rpng_reverse_filter_deinit(pngp);

   if (pngp->ring_size < pngp->inflate_buf_size)
      pngp->inflate_buf = pngp->inflate_base;
   else
      pngp->inflate_buf -= pngp->restore_buf_size;
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
   else if (ret == RPNG_FILTER_WAIT)
      return RPNG_FILTER_WAIT;
   else if (ret == -1)
      return IMAGE_PROCESS_ERROR_END;

   if (rpng_reverse_filter_init(&pngp->ihdr, pngp) == -1)
      return IMAGE_PROCESS_ERROR;

   pngp->out_cursor = pngp->data;

   do
   {
      ret = rpng_reverse_filter_regular_iterate(&pngp->ihdr, pngp);
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
      case RPNG_FILTER_WAIT:
         /* More inflate needed before this pass can start; do not
          * advance pass_pos and do not disturb the restore
          * bookkeeping.  Tell the driver to pull a slice. */
         pngp->flags |= RPNG_PROCESS_FLAG_FILTER_STARVED;
         return 0;
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

/* Output slice fed to the inflate backend per call.  Inflating with the
 * whole image as avail_out makes zlib verify the stream's Adler-32 in
 * one cold pass over the entire decompressed buffer at stream end -- on
 * a 256 KB last-level cache that is a guaranteed full re-read from DRAM
 * (measured: 16K line fills for a 1 MB image, ~17% of the decode's
 * total LL misses).  Bounded slices keep the checksum running over
 * output that the same inflate call just wrote, so it stays cache-warm;
 * the slice must comfortably exceed the 32 KB deflate window and small
 * enough to sit in L1/L2 alongside the window.  64 KB does both.  This
 * also restores the incremental pacing the nbio callers were written
 * for: one bounded step per rpng_process_image call instead of one
 * unbounded one. */
#define RPNG_INFLATE_SLICE 32768

static int rpng_load_image_argb_process_inflate_init(
      rpng_t *rpng, uint32_t **data)
{
   bool zstatus;
   enum trans_stream_error err;
   uint32_t rd, wn, slice;
   struct rpng_process *process = (struct rpng_process*)rpng->process;
   /* Keep going while output is still owed, even once the compressed
    * input is exhausted.  An inflate backend buffers whole input bytes
    * into its bit accumulator ahead of use and reports them as read, so
    * "input consumed" does not mean "output finished" - there can be a
    * bit accumulator's worth of decodable data left after the last byte
    * has been handed over.  Requiring avail_in > 0 here truncated those
    * trailing bytes and then failed the whole image on the
    * under-produced check below.  Progress is enforced after the call
    * instead. */
   bool to_continue             = (process->avail_out > 0);

   if (!to_continue)
      goto end;

   /* Feed the compressed stream straight from the caller's buffer,
    * one IDAT span at a time (the accumulated idat_buf copy this
    * replaces was ~20% of a decode's DRAM traffic on a small cache,
    * and its allocation the decode's largest after the inflate
    * buffer).  Skip any spans already fully consumed, then hand the
    * backend the remainder of the current one; set_in only
    * resets the backend's input window, so re-declaring the remainder each
    * call is the normal streaming usage. */
   while (   process->span_idx < rpng->idat_buf.n
          && process->span_pos >= rpng->idat_buf.v[process->span_idx].len)
   {
      process->span_idx++;
      process->span_pos = 0;
   }
   if (process->span_idx < rpng->idat_buf.n)
   {
      const struct idat_span *sp = &rpng->idat_buf.v[process->span_idx];
      process->stream_backend->set_in(process->stream,
            rpng->buff_start + sp->off + process->span_pos,
            sp->len - process->span_pos);
   }
   else
   {
      /* All spans consumed: declare an empty input window so the
       * backend drains whatever it still holds internally rather than
       * re-reading the previous one. */
      process->stream_backend->set_in(process->stream,
            rpng->buff_start, 0);
   }

   {
      size_t wpos    = process->inflated_total % process->ring_size;
      size_t contig  = process->ring_size - wpos;
      /* Free ring space: what has been produced but not yet consumed
       * by the filters stays untouchable.  restore_buf_size counts
       * consumed bytes exactly on the regular path; on the interlaced
       * path the ring equals the full buffer, where this bound is
       * provably never the minimum, whatever the pass-local counter
       * holds. */
      size_t free_sp = process->ring_size
            - (process->inflated_total - process->restore_buf_size);
      size_t bound   = process->avail_out;
      if (bound > RPNG_INFLATE_SLICE)
         bound = RPNG_INFLATE_SLICE;
      if (bound > contig)
         bound = contig;
      if (bound > free_sp)
         bound = free_sp;
      slice = (uint32_t)bound;
      if (!slice)  /* ring momentarily full; filters must drain first */
         return 0;
      process->stream_backend->set_out(process->stream,
            process->inflate_base + wpos, slice);
   }

   zstatus = process->stream_backend->trans(
      process->stream, false, &rd, &wn, &err);

   if (!zstatus && err != TRANS_STREAM_ERROR_BUFFER_FULL)
      goto error;

   process->avail_in      -= rd;
   process->span_pos      += rd;
   process->avail_out     -= wn;
   process->total_out     += wn;
   process->inflated_total += wn;

   /* No input taken and no output made means the backend cannot get any
    * further - a truncated or corrupt stream.  Without this the
    * avail_in > 0 condition above was doing double duty as the
    * termination guarantee. */
   if (rd == 0 && wn == 0)
      goto error;

   if (err)
      return 0;

end:
   /* Stream complete.  An under-produced stream (declared geometry
    * not fully covered) was previously caught by the reverse-filter
    * init's whole-image check, which now runs before the stream ends;
    * enforce it here instead, in the one place that knows the stream
    * is finished. */
   if (process->avail_out > 0 && rpng->ihdr.interlace != 1)
      goto error;
   process->stream_backend->stream_free(process->stream);
   process->stream = NULL;

   process->flags |=  RPNG_PROCESS_FLAG_INFLATE_INITIALIZED;
   return 1;

error:
   process->flags &= ~RPNG_PROCESS_FLAG_INFLATE_INITIALIZED;
   return -1;
}

/* One-time output-side setup, run before the first reverse-filter
 * step rather than after the whole stream has inflated (the decode is
 * interleaved: unfiltering consumes scanlines while later slices are
 * still being inflated, so each slice is unfiltered while it is still
 * cache-warm instead of being re-read cold after a full-image inflate
 * pass). */
static int rpng_load_image_argb_process_output_init(
      rpng_t *rpng, uint32_t **data)
{
   struct rpng_process *process = (struct rpng_process*)rpng->process;

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
      return -1;

   process->adam7_restore_buf_size = 0;
   process->restore_buf_size       = 0;
   process->palette                = rpng->palette;

   if (rpng->ihdr.interlace != 1)
      if (rpng_reverse_filter_init(&rpng->ihdr, process) == -1)
         return -1;

   process->out_cursor = *data;

   process->flags |= RPNG_PROCESS_FLAG_OUTPUT_INITIALIZED;
   return 0;
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
/* Record one IDAT payload.  Both caps mirror the copying path this
 * replaces: total payload bytes stay bounded by RPNG_IDAT_MAX exactly
 * as before (the overflow-guarded accumulation was the subject of a
 * past hardening fix), and the span array itself is held to the same
 * byte bound so a malicious stream of millions of tiny IDAT chunks
 * cannot make the bookkeeping allocation exceed what the old payload
 * copy could ever have reached. */
static bool rpng_idat_append_span(struct idat_spans *sp,
      size_t off, uint32_t chunk_size)
{
   if (chunk_size > RPNG_IDAT_MAX - sp->total)
      return false;
   if (sp->n >= sp->cap)
   {
      struct idat_span *nv;
      size_t ncap = sp->cap ? sp->cap * 2 : 64;
      if (sp->n >= RPNG_IDAT_MAX / sizeof(*sp->v))
         return false;
      if (ncap > RPNG_IDAT_MAX / sizeof(*sp->v))
         ncap = RPNG_IDAT_MAX / sizeof(*sp->v);
      if (!(nv = (struct idat_span*)realloc(sp->v, ncap * sizeof(*nv))))
         return false;
      sp->v   = nv;
      sp->cap = ncap;
   }
   sp->v[sp->n].off = off;
   sp->v[sp->n].len = chunk_size;
   sp->n++;
   sp->total += chunk_size;
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

   if (rpng->ihdr.interlace == 1)
      process->inflate_buf_size = rpng_adam7_buf_size(&rpng->ihdr);
   else
      rpng_pass_geom(&rpng->ihdr, rpng->ihdr.width,
            rpng->ihdr.height, NULL, NULL, &process->inflate_buf_size);

   /* Interleaved decode lets the inflate window recycle: filtering
    * consumes scanlines as slices arrive, so for regular images the
    * buffer only needs to cover the in-flight region, not the whole
    * image.  A whole number of scanlines at least two lines and at
    * least two inflate slices deep keeps lines contiguous across the
    * wrap and guarantees progress (if no whole line is available, at
    * least a line's worth of ring is free).  Adam7 consumes whole
    * passes and rewinds across them, so interlaced images keep the
    * full-size buffer, which makes every ring expression below an
    * identity. */
   if (rpng->ihdr.interlace == 1)
      process->ring_size = process->inflate_buf_size;
   else
   {
      unsigned pitch_l = 0;
      size_t   line, k;
      rpng_pass_geom(&rpng->ihdr, rpng->ihdr.width,
            rpng->ihdr.height, NULL, &pitch_l, NULL);
      line = (size_t)pitch_l + 1;
      k    = ((size_t)(2 * RPNG_INFLATE_SLICE) + line - 1) / line;
      if (k < 2)
         k = 2;
      process->ring_size = k * line;
      if (process->ring_size > process->inflate_buf_size)
         process->ring_size = process->inflate_buf_size;
   }

   process->stream = process->stream_backend->stream_new();

   if (!process->stream)
   {
      free(process);
      return NULL;
   }

   inflate_buf = (uint8_t*)malloc(process->ring_size);
   if (!inflate_buf)
      goto error;

   process->inflate_buf  = inflate_buf;
   process->inflate_base = inflate_buf;
   process->avail_in     = rpng->idat_buf.total;
   process->avail_out   = process->inflate_buf_size;
   process->span_idx    = 0;
   process->span_pos    = 0;

   /* Input is fed span by span from the caller's buffer in
    * rpng_load_image_argb_process_inflate_init; output likewise in
    * bounded slices there.  Nothing to hand the backend yet. */

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
            if (rpng->ihdr.interlace == 1)
               pass_size = rpng_adam7_buf_size(&rpng->ihdr);
            else
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
         if (     !(rpng->flags & RPNG_FLAG_HAS_IHDR)
               ||  (rpng->flags & RPNG_FLAG_HAS_IEND)
               ||  (rpng->ihdr.color_type == PNG_IHDR_COLOR_PLT
                  &&
                  !(rpng->flags & RPNG_FLAG_HAS_PLTE)))
            return false;

         buf += 8;

         /* Zero-length IDAT chunks are legal; they contribute no
          * payload, so no span - the HAS_IDAT flag alone records
          * them. */
         if (chunk_size)
         {
            if (!rpng_idat_append_span(&rpng->idat_buf,
                  (size_t)(buf - rpng->buff_start), chunk_size))
               return false;
         }

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

   /* Interleaved decode: inflate is demand-driven.  A slice is only
    * pulled when the next unit of filtering work (a scanline, or an
    * Adam7 pass) has not fully arrived, so consumption tracks
    * production and each slice is unfiltered while still cache-warm
    * rather than re-read cold after a whole-image inflate pass. */
   if (!(rpng->process->flags & RPNG_PROCESS_FLAG_OUTPUT_INITIALIZED))
   {
      if (rpng_load_image_argb_process_output_init(rpng, data) == -1)
         goto error;
   }

   *width  = rpng->ihdr.width;
   *height = rpng->ihdr.height;

   if (rpng->ihdr.interlace && rpng->process)
   {
      int ret;
      rpng->process->flags &= ~RPNG_PROCESS_FLAG_FILTER_STARVED;
      ret = rpng_reverse_filter_adam7(data, &rpng->ihdr, rpng->process);
      if (   (rpng->process->flags & RPNG_PROCESS_FLAG_FILTER_STARVED)
          && !(rpng->process->flags & RPNG_PROCESS_FLAG_INFLATE_INITIALIZED))
      {
         if (rpng_load_image_argb_process_inflate_init(rpng, data) == -1)
            goto error;
      }
      return ret;
   }

   /* A scanline is one filter byte plus pitch bytes; pull one inflate
    * slice when the next one has not fully arrived.  held bytes are
    * already unfiltered in place - they lag restore_buf_size only to
    * keep their ring slot out of the recycling window - so they do
    * not count as an available raw line. */
   if (   !(rpng->process->flags & RPNG_PROCESS_FLAG_INFLATE_INITIALIZED)
       &&   rpng->process->total_out - rpng->process->restore_buf_size
              - rpng->process->held
          < (size_t)rpng->process->pitch + 1)
   {
      if (rpng_load_image_argb_process_inflate_init(rpng, data) == -1)
         goto error;
      if (   rpng->process->total_out - rpng->process->restore_buf_size
               - rpng->process->held
           < (size_t)rpng->process->pitch + 1)
         return IMAGE_PROCESS_NEXT;
   }

   return rpng_reverse_filter_regular_iterate(&rpng->ihdr, rpng->process);

error:
   if (rpng->process)
   {
      /* An externally abandoned decode (cancelled task) can be torn
       * down at any machine state: the per-pass output, the scanline
       * buffers and the inflate window may all still be live here. */
      if (rpng->process->data)
         free(rpng->process->data);
      if (rpng->process->scratch[0])
         free(rpng->process->scratch[0]);
      if (rpng->process->scratch[1])
         free(rpng->process->scratch[1]);
      if (rpng->process->scratch[2])
         free(rpng->process->scratch[2]);
      if (rpng->process->inflate_base)
         free(rpng->process->inflate_base);
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

   if (rpng->idat_buf.v)
      free(rpng->idat_buf.v);
   if (rpng->process)
   {
      /* An externally abandoned decode (cancelled task) is torn down
       * here at an arbitrary machine state: the per-pass output and
       * the scanline buffers may still be live alongside the inflate
       * window. */
      if (rpng->process->data)
         free(rpng->process->data);
      if (rpng->process->scratch[0])
         free(rpng->process->scratch[0]);
      if (rpng->process->scratch[1])
         free(rpng->process->scratch[1]);
      if (rpng->process->scratch[2])
         free(rpng->process->scratch[2]);
      if (rpng->process->inflate_base)
         free(rpng->process->inflate_base);
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
