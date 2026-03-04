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

#include <boolean.h>
#include <formats/image.h>
#include <formats/rpng.h>
#include <streams/trans_stream.h>
#include <string/stdstring.h>

#include "rpng_internal.h"

/* ---------------------------------------------------------------------------
 * SSE2 acceleration
 * Guarded so the file compiles cleanly on non-x86 targets as well.
 * ---------------------------------------------------------------------------*/
#if defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(_M_X64)
#  include <emmintrin.h>
#  define RPNG_HAVE_SSE2 1
#else
#  define RPNG_HAVE_SSE2 0
#endif

/* ---------------------------------------------------------------------------
 * ARM NEON acceleration
 * Covers AArch64 (NEON always present) and AArch32 with __ARM_NEON defined.
 * RPNG_HAVE_NEON is mutually exclusive with RPNG_HAVE_SSE2: on a given build
 * exactly one SIMD back-end is active.
 * ---------------------------------------------------------------------------*/
#if !RPNG_HAVE_SSE2 && (defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__) || defined(HAVE_NEON))
#  include <arm_neon.h>
#  define RPNG_HAVE_NEON 1
#else
#  define RPNG_HAVE_NEON 0
#endif

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
};

enum rpng_flags
{
   RPNG_FLAG_HAS_IHDR = (1 << 0),
   RPNG_FLAG_HAS_IDAT = (1 << 1),
   RPNG_FLAG_HAS_IEND = (1 << 2),
   RPNG_FLAG_HAS_PLTE = (1 << 3),
   RPNG_FLAG_HAS_TRNS = (1 << 4)
};

struct rpng
{
   struct rpng_process *process;
   uint8_t *buff_data;
   uint8_t *buff_end;
   struct idat_buffer idat_buf; /* ptr alignment */
   struct png_ihdr ihdr; /* uint32 alignment */
   uint32_t palette[256];
   uint8_t flags;
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
   return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16)
        | ((uint32_t)buf[2] <<  8) |  (uint32_t)buf[3];
}

#if defined(DEBUG) || defined(RPNG_TEST)
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

   return true;
}
#endif

static void rpng_reverse_filter_copy_line_rgb(uint32_t *data,
      const uint8_t *decoded, unsigned width, unsigned bpp)
{
   unsigned i;
   unsigned step = bpp / 8;

#if RPNG_HAVE_SSE2
   /* Fast path: 8-bit-per-channel RGB only (step == 1).
    * Process 4 pixels per iteration using SSE2 byte-shuffle.
    *
    * Source layout per 4 pixels: R0 G0 B0 R1 G1 B1 R2 G2 B2 R3 G3 B3
    * Target: 0xFF_R0_G0_B0  0xFF_R1_G1_B1  0xFF_R2_G2_B2  0xFF_R3_G3_B3
    */
   if (step == 1 && width >= 4)
   {
      /* Constant alpha mask: each 32-bit lane gets 0xFF000000 */
      const __m128i alpha_mask = _mm_set1_epi32((int)0xFF000000u);
      /* Zero vector for byte-unpacking */
      const __m128i zero = _mm_setzero_si128();
      unsigned w4 = width & ~3u; /* rounded down to multiple of 4 */

      for (i = 0; i < w4; i += 4, decoded += 12)
      {
         /* Load 12 bytes (only lower 12 matter; upper 4 are don't-care) */
         __m128i raw = _mm_loadu_si128((const __m128i *)decoded);

         /* Separate channels.
          * _mm_shuffle_epi32 / manual extraction in SSE2:
          * We extract each byte by shifting and masking. */

         /* raw bytes: [R0][G0][B0][R1][G1][B1][R2][G2][B2][R3][G3][B3][??][??][??][??] */

         /* Unpack lo 8 bytes to 16-bit words (zero-extend) */
         __m128i lo16 = _mm_unpacklo_epi8(raw, zero); /* 8x u16: R0 G0 B0 R1 G1 B1 R2 G2 */
         __m128i hi16 = _mm_unpackhi_epi8(raw, zero); /* 8x u16: B2 R3 G3 B3 .. .. .. .. */

         /* Pixels 0 and 1 live in lo16 words [0..5]:
          *   lo16[0]=R0 lo16[1]=G0 lo16[2]=B0 lo16[3]=R1 lo16[4]=G1 lo16[5]=B1
          * Pixels 2 and 3 in lo16[6]=R2 lo16[7]=G2 and hi16[0]=B2 hi16[1]=R3 hi16[2]=G3 hi16[3]=B3
          */

         /* Build pixel 0: 0x00RR 0x00GG 0x00BB -> shift-or to 32 bits */
         {
            uint32_t r0 = (uint32_t)_mm_extract_epi16(lo16, 0);
            uint32_t g0 = (uint32_t)_mm_extract_epi16(lo16, 1);
            uint32_t b0 = (uint32_t)_mm_extract_epi16(lo16, 2);
            data[i + 0] = 0xFF000000u | (r0 << 16) | (g0 << 8) | b0;
         }
         {
            uint32_t r1 = (uint32_t)_mm_extract_epi16(lo16, 3);
            uint32_t g1 = (uint32_t)_mm_extract_epi16(lo16, 4);
            uint32_t b1 = (uint32_t)_mm_extract_epi16(lo16, 5);
            data[i + 1] = 0xFF000000u | (r1 << 16) | (g1 << 8) | b1;
         }
         {
            uint32_t r2 = (uint32_t)_mm_extract_epi16(lo16, 6);
            uint32_t g2 = (uint32_t)_mm_extract_epi16(lo16, 7);
            uint32_t b2 = (uint32_t)_mm_extract_epi16(hi16, 0);
            data[i + 2] = 0xFF000000u | (r2 << 16) | (g2 << 8) | b2;
         }
         {
            uint32_t r3 = (uint32_t)_mm_extract_epi16(hi16, 1);
            uint32_t g3 = (uint32_t)_mm_extract_epi16(hi16, 2);
            uint32_t b3 = (uint32_t)_mm_extract_epi16(hi16, 3);
            data[i + 3] = 0xFF000000u | (r3 << 16) | (g3 << 8) | b3;
         }
      }

      /* Scalar tail */
      for (; i < width; i++)
      {
         uint32_t r = *decoded++;
         uint32_t g = *decoded++;
         uint32_t b = *decoded++;
         data[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
      }
      return;
   }
#endif /* RPNG_HAVE_SSE2 */

#if RPNG_HAVE_NEON
   /*
    * NEON fast path: 8-bit RGB → ARGB32, 8 pixels per iteration.
    *
    * vld3_u8 performs a de-interleave load: given source bytes
    *   R0 G0 B0 R1 G1 B1 R2 G2 B2 R3 G3 B3 R4 G4 B4 R5 G5 B5 R6 G6 B6 R7 G7 B7
    * it fills three 8-lane uint8x8 registers:
    *   .val[0] = { R0..R7 }
    *   .val[1] = { G0..G7 }
    *   .val[2] = { B0..B7 }
    * We then widen each to uint16x8, shift/combine into uint32x4 pairs,
    * and store 8 ARGB32 dwords.
    */
   if (step == 1 && width >= 8)
   {
      const uint8x8_t  alpha8  = vdup_n_u8(0xFF);
      unsigned w8 = width & ~7u;

      for (i = 0; i < w8; i += 8, decoded += 24)
      {
         /* De-interleaved load: 3 channels × 8 pixels */
         uint8x8x3_t rgb = vld3_u8(decoded);

         /* Widen each channel to 16-bit */
         uint16x8_t r16 = vmovl_u8(rgb.val[0]);
         uint16x8_t g16 = vmovl_u8(rgb.val[1]);
         uint16x8_t b16 = vmovl_u8(rgb.val[2]);
         uint16x8_t a16 = vmovl_u8(alpha8);

         /* Widen to 32-bit — process low 4 and high 4 pixels separately */
         uint32x4_t r_lo = vmovl_u16(vget_low_u16(r16));
         uint32x4_t r_hi = vmovl_u16(vget_high_u16(r16));
         uint32x4_t g_lo = vmovl_u16(vget_low_u16(g16));
         uint32x4_t g_hi = vmovl_u16(vget_high_u16(g16));
         uint32x4_t b_lo = vmovl_u16(vget_low_u16(b16));
         uint32x4_t b_hi = vmovl_u16(vget_high_u16(b16));
         uint32x4_t a_lo = vmovl_u16(vget_low_u16(a16));
         uint32x4_t a_hi = vmovl_u16(vget_high_u16(a16));

         /* Assemble ARGB32: A<<24 | R<<16 | G<<8 | B */
         uint32x4_t out_lo = vorrq_u32(
               vorrq_u32(vshlq_n_u32(a_lo, 24), vshlq_n_u32(r_lo, 16)),
               vorrq_u32(vshlq_n_u32(g_lo,  8), b_lo));
         uint32x4_t out_hi = vorrq_u32(
               vorrq_u32(vshlq_n_u32(a_hi, 24), vshlq_n_u32(r_hi, 16)),
               vorrq_u32(vshlq_n_u32(g_hi,  8), b_hi));

         vst1q_u32(data + i,     out_lo);
         vst1q_u32(data + i + 4, out_hi);
      }

      /* Scalar tail */
      for (; i < width; i++)
      {
         uint32_t r = *decoded++;
         uint32_t g = *decoded++;
         uint32_t b = *decoded++;
         data[i] = 0xFF000000u | (r << 16) | (g << 8) | b;
      }
      return;
   }
#endif /* RPNG_HAVE_NEON */

   for (i = 0; i < width; i++)
   {
      uint32_t r = *decoded; decoded += step;
      uint32_t g = *decoded; decoded += step;
      uint32_t b = *decoded; decoded += step;
      data[i] = (0xffu << 24) | (r << 16) | (g << 8) | b;
   }
}

static void rpng_reverse_filter_copy_line_rgba(uint32_t *data,
      const uint8_t *decoded, unsigned width, unsigned bpp)
{
   unsigned i;
   unsigned step = bpp / 8;

#if RPNG_HAVE_SSE2
   /*
    * Fast path: 8-bit RGBA (step == 1).
    * Source: R0 G0 B0 A0  R1 G1 B1 A1  R2 G2 B2 A2  R3 G3 B3 A3
    * Target: A0 R0 G0 B0  A1 R1 G1 B1  A2 R2 G2 B2  A3 R3 G3 B3  (ARGB32)
    *
    * Strategy: load 16 source bytes, unpack to u16, then shift/or.
    * A is already the 4th byte (index 3,7,11,15) → shift left 24.
    * R is byte 0,4,8,12 → shift left 16.
    * G is byte 1,5,9,13 → shift left 8.
    * B is byte 2,6,10,14 → keep in place.
    *
    * We use _mm_and_si128 + _mm_slli_epi32 on the packed dwords directly.
    * Source dword layout: [R][G][B][A] (little-endian in register = A<<24|B<<16|G<<8|R)
    * We need output:       [B][G][R][A] (little-endian = A<<24|R<<16|G<<8|B)
    *
    * Byte-swap within each 32-bit lane: we want bytes rotated:
    *   in:  [0]=R [1]=G [2]=B [3]=A
    *   out: [0]=B [1]=G [2]=R [3]=A
    * That's just swapping bytes 0 and 2 within each dword.
    * SSE2 has no byte-shuffle within dwords, so we use shift+mask+or.
    */
   if (step == 1 && width >= 4)
   {
      const __m128i mask_rb   = _mm_set1_epi32((int)0x00FF00FFu); /* isolate R and B bytes */
      const __m128i mask_ga   = _mm_set1_epi32((int)0xFF00FF00u); /* isolate G and A bytes */
      unsigned w4 = width & ~3u;

      for (i = 0; i < w4; i += 4, decoded += 16)
      {
         /* Load 16 bytes = 4 RGBA pixels */
         __m128i src = _mm_loadu_si128((const __m128i *)decoded);
         /* src dword[k] little-endian = A<<24 | B<<16 | G<<8 | R  (PNG is big-endian bytes) */
         /* But in memory: decoded[0]=R decoded[1]=G decoded[2]=B decoded[3]=A
          * In little-endian __m128i lane: bits 0-7=R, 8-15=G, 16-23=B, 24-31=A
          * We want output ARGB32 stored as uint32_t = A<<24|R<<16|G<<8|B
          * i.e. bits 0-7=B, 8-15=G, 16-23=R, 24-31=A
          * So swap byte 0 (R) and byte 2 (B) within each dword.
          */
         __m128i rb  = _mm_and_si128(src, mask_rb);  /* keep bytes 0,2: [R][.][B][.] */
         __m128i ga  = _mm_and_si128(src, mask_ga);  /* keep bytes 1,3: [.][G][.][A] */
         /* Swap R (bits 0-7) into bits 16-23, and B (bits 16-23) into bits 0-7 */
         __m128i r   = _mm_slli_epi32(rb, 16);       /* R now in bits 16-23, B gone up to 32 (lost) */
         __m128i b   = _mm_srli_epi32(rb, 16);       /* B now in bits 0-7,  R gone to zero */
         /* Recombine: b | r gives byte2=R byte0=B; ga stays as byte3=A byte1=G */
         __m128i out = _mm_or_si128(_mm_or_si128(r, b), ga);
         _mm_storeu_si128((__m128i *)(data + i), out);
      }

      for (; i < width; i++)
      {
         uint32_t r = *decoded++;
         uint32_t g = *decoded++;
         uint32_t b = *decoded++;
         uint32_t a = *decoded++;
         data[i] = (a << 24) | (r << 16) | (g << 8) | b;
      }
      return;
   }
#endif /* RPNG_HAVE_SSE2 */

#if RPNG_HAVE_NEON
   /*
    * NEON fast path: 8-bit RGBA → ARGB32, 8 pixels per iteration.
    *
    * vld4_u8 de-interleaves 4 channels from 32 source bytes:
    *   .val[0] = { R0..R7 }
    *   .val[1] = { G0..G7 }
    *   .val[2] = { B0..B7 }
    *   .val[3] = { A0..A7 }
    * Same widen-and-combine strategy as the RGB path.
    */
   if (step == 1 && width >= 8)
   {
      unsigned w8 = width & ~7u;

      for (i = 0; i < w8; i += 8, decoded += 32)
      {
         uint8x8x4_t rgba = vld4_u8(decoded);

         uint16x8_t r16 = vmovl_u8(rgba.val[0]);
         uint16x8_t g16 = vmovl_u8(rgba.val[1]);
         uint16x8_t b16 = vmovl_u8(rgba.val[2]);
         uint16x8_t a16 = vmovl_u8(rgba.val[3]);

         uint32x4_t r_lo = vmovl_u16(vget_low_u16(r16));
         uint32x4_t r_hi = vmovl_u16(vget_high_u16(r16));
         uint32x4_t g_lo = vmovl_u16(vget_low_u16(g16));
         uint32x4_t g_hi = vmovl_u16(vget_high_u16(g16));
         uint32x4_t b_lo = vmovl_u16(vget_low_u16(b16));
         uint32x4_t b_hi = vmovl_u16(vget_high_u16(b16));
         uint32x4_t a_lo = vmovl_u16(vget_low_u16(a16));
         uint32x4_t a_hi = vmovl_u16(vget_high_u16(a16));

         uint32x4_t out_lo = vorrq_u32(
               vorrq_u32(vshlq_n_u32(a_lo, 24), vshlq_n_u32(r_lo, 16)),
               vorrq_u32(vshlq_n_u32(g_lo,  8), b_lo));
         uint32x4_t out_hi = vorrq_u32(
               vorrq_u32(vshlq_n_u32(a_hi, 24), vshlq_n_u32(r_hi, 16)),
               vorrq_u32(vshlq_n_u32(g_hi,  8), b_hi));

         vst1q_u32(data + i,     out_lo);
         vst1q_u32(data + i + 4, out_hi);
      }

      for (; i < width; i++)
      {
         uint32_t r = *decoded++;
         uint32_t g = *decoded++;
         uint32_t b = *decoded++;
         uint32_t a = *decoded++;
         data[i] = (a << 24) | (r << 16) | (g << 8) | b;
      }
      return;
   }
#endif /* RPNG_HAVE_NEON */

   for (i = 0; i < width; i++)
   {
      uint32_t r = *decoded; decoded += step;
      uint32_t g = *decoded; decoded += step;
      uint32_t b = *decoded; decoded += step;
      uint32_t a = *decoded; decoded += step;
      data[i] = (a << 24) | (r << 16) | (g << 8) | b;
   }
}

static void rpng_reverse_filter_copy_line_bw(uint32_t *data,
      const uint8_t *decoded, unsigned width, unsigned depth)
{
   unsigned i;
   static const unsigned mul_table[] = { 0, 0xff, 0x55, 0, 0x11, 0, 0, 0, 0x01 };
   unsigned mul, mask, bit;

   if (depth == 16)
   {
#if RPNG_HAVE_SSE2
      /*
       * Each gray16 pixel: high byte at decoded[2i], low byte at decoded[2i+1].
       * We only need the high byte (top 8 bits), expanded to 0xFFgggggg.
       * Source stride is 2 bytes per pixel; gather every other byte.
       *
       * Process 8 pixels per iteration:
       * Load 16 bytes: G0_hi G0_lo G1_hi G1_lo ... G7_hi G7_lo
       * Extract even bytes (indices 0,2,4,6,8,10,12,14) → 8 gray values.
       * Expand each to 0xFF_gray_gray_gray using multiply trick.
       */
      if (width >= 8)
      {
         const __m128i zero      = _mm_setzero_si128();
         const __m128i alpha_ff  = _mm_set1_epi32((int)0xFF000000u);
         const __m128i mask_even = _mm_set1_epi16((short)0x00FFu); /* keep low byte of each u16 */
         unsigned w8 = width & ~7u;

         for (i = 0; i < w8; i += 8, decoded += 16)
         {
            /* Load 16 bytes = 8 gray16 pixels */
            __m128i raw16 = _mm_loadu_si128((const __m128i *)decoded);
            /* raw16 u16 lanes: [G0_lo | G0_hi<<8] [G1_lo | G1_hi<<8] ...
             * (little-endian: byte at lower address is low bits)
             * decoded[0]=G0_hi decoded[1]=G0_lo in memory, so in u16 lane:
             *   bits 0-7  = decoded[0] = high byte of gray16
             *   bits 8-15 = decoded[1] = low byte of gray16
             * We want just the high byte (bits 0-7 of each u16 lane).
             */
            __m128i gray8_16 = _mm_and_si128(raw16, mask_even); /* 8x u16: gray value in bits 0-7 */

            /* Unpack to two groups of 4 x u32 */
            __m128i lo4 = _mm_unpacklo_epi16(gray8_16, zero); /* 4x u32 */
            __m128i hi4 = _mm_unpackhi_epi16(gray8_16, zero); /* 4x u32 */

            /* Expand gray byte to 0x00_gg_gg_gg via multiply by 0x010101.
             * SSE2 has no 32-bit multiply in epi32 (that arrives in SSE4.1).
             * Use the identity: g * 0x010101 = g | (g<<8) | (g<<16).
             */
            {
               __m128i g0 = lo4;
               __m128i g8  = _mm_slli_epi32(lo4,  8);
               __m128i g16 = _mm_slli_epi32(lo4, 16);
               __m128i rgb_lo = _mm_or_si128(_mm_or_si128(g0, g8), g16);
               __m128i out_lo = _mm_or_si128(rgb_lo, alpha_ff);
               _mm_storeu_si128((__m128i *)(data + i), out_lo);
            }
            {
               __m128i g0 = hi4;
               __m128i g8  = _mm_slli_epi32(hi4,  8);
               __m128i g16 = _mm_slli_epi32(hi4, 16);
               __m128i rgb_hi = _mm_or_si128(_mm_or_si128(g0, g8), g16);
               __m128i out_hi = _mm_or_si128(rgb_hi, alpha_ff);
               _mm_storeu_si128((__m128i *)(data + i + 4), out_hi);
            }
         }
         for (; i < width; i++)
         {
            uint32_t val = decoded[i << 1];
            data[i] = (val * 0x010101u) | (0xffu << 24);
         }
         return;
      }
#endif /* RPNG_HAVE_SSE2 */

#if RPNG_HAVE_NEON
      /*
       * NEON fast path: gray16 → ARGB32, 8 pixels per iteration.
       *
       * Source: G0_hi G0_lo G1_hi G1_lo ... (2 bytes/pixel, big-endian).
       * We need the high byte of each 16-bit sample: decoded[2*i].
       * vld2_u8 de-interleaves pairs: .val[0] = high bytes, .val[1] = low bytes.
       * Expand high byte to 0xFF_gg_gg_gg.
       */
      if (width >= 8)
      {
         const uint8x8_t  alpha8 = vdup_n_u8(0xFF);
         unsigned w8 = width & ~7u;

         for (i = 0; i < w8; i += 8, decoded += 16)
         {
            /* De-interleave: high bytes into val[0], low bytes into val[1] */
            uint8x8x2_t pairs = vld2_u8(decoded);
            uint8x8_t   gray8 = pairs.val[0]; /* high byte = 8-bit gray value */

            /* Widen gray to 16 then 32 bits */
            uint16x8_t g16 = vmovl_u8(gray8);
            uint32x4_t g_lo = vmovl_u16(vget_low_u16(g16));
            uint32x4_t g_hi = vmovl_u16(vget_high_u16(g16));

            /* Expand gray * 0x010101 = g | (g<<8) | (g<<16) */
            uint32x4_t rgb_lo = vorrq_u32(vorrq_u32(g_lo,
                                    vshlq_n_u32(g_lo,  8)),
                                    vshlq_n_u32(g_lo, 16));
            uint32x4_t rgb_hi = vorrq_u32(vorrq_u32(g_hi,
                                    vshlq_n_u32(g_hi,  8)),
                                    vshlq_n_u32(g_hi, 16));

            /* OR in alpha=0xFF000000 */
            uint16x8_t a16   = vmovl_u8(alpha8);
            uint32x4_t a_lo  = vmovl_u16(vget_low_u16(a16));
            uint32x4_t a_hi  = vmovl_u16(vget_high_u16(a16));
            uint32x4_t out_lo = vorrq_u32(rgb_lo, vshlq_n_u32(a_lo, 24));
            uint32x4_t out_hi = vorrq_u32(rgb_hi, vshlq_n_u32(a_hi, 24));

            vst1q_u32(data + i,     out_lo);
            vst1q_u32(data + i + 4, out_hi);
         }

         for (; i < width; i++)
         {
            uint32_t val = decoded[i << 1];
            data[i] = (val * 0x010101u) | (0xffu << 24);
         }
         return;
      }
#endif /* RPNG_HAVE_NEON */
      for (i = 0; i < width; i++)
      {
         uint32_t val = decoded[i << 1];
         data[i]      = (val * 0x010101u) | (0xffu << 24);
      }
      return;
   }

   mul  = mul_table[depth];
   mask = (1u << depth) - 1u;
   bit  = 0;

   for (i = 0; i < width; i++, bit += depth)
   {
      unsigned byte = bit >> 3;
      unsigned val  = decoded[byte] >> (8 - depth - (bit & 7));
      val          &= mask;
      val          *= mul;
      data[i]       = (val * 0x010101u) | (0xffu << 24);
   }
}

static void rpng_reverse_filter_copy_line_gray_alpha(uint32_t *data,
      const uint8_t *decoded, unsigned width,
      unsigned bpp)
{
   unsigned i;
   unsigned step = bpp / 8;

#if RPNG_HAVE_SSE2
   /*
    * Fast path: 8-bit gray+alpha (step == 1, 2 bytes per pixel).
    * Source: G0 A0 G1 A1 G2 A2 G3 A3 ... (8 bytes = 4 pixels)
    * Output: 0xAA_gg_gg_gg per pixel = A<<24 | G*0x010101
    *
    * Process 8 pixels (16 source bytes) per SSE2 iteration.
    */
   if (step == 1 && width >= 8)
   {
      const __m128i zero     = _mm_setzero_si128();
      const __m128i mask_lo  = _mm_set1_epi16((short)0x00FFu); /* keep gray (low byte) */
      unsigned w8 = width & ~7u;

      for (i = 0; i < w8; i += 8, decoded += 16)
      {
         __m128i raw = _mm_loadu_si128((const __m128i *)decoded);
         /* raw u16 lanes: bits 0-7 = G, bits 8-15 = A */

         /* Extract gray: low byte of each u16 */
         __m128i gray16 = _mm_and_si128(raw, mask_lo);     /* 8x u16: gray */
         /* Extract alpha: high byte of each u16 → shift right 8 */
         __m128i alph16 = _mm_srli_epi16(raw, 8);          /* 8x u16: alpha */

         /* Expand to 32-bit lanes: first 4 and last 4 pixels */
         __m128i g_lo = _mm_unpacklo_epi16(gray16, zero);  /* 4x u32: gray */
         __m128i g_hi = _mm_unpackhi_epi16(gray16, zero);
         __m128i a_lo = _mm_unpacklo_epi16(alph16, zero);  /* 4x u32: alpha */
         __m128i a_hi = _mm_unpackhi_epi16(alph16, zero);

         /* Build gray * 0x010101 = g | (g<<8) | (g<<16) */
         {
            __m128i rgb_lo = _mm_or_si128(_mm_or_si128(g_lo,
                                 _mm_slli_epi32(g_lo, 8)),
                                 _mm_slli_epi32(g_lo, 16));
            __m128i ashift_lo = _mm_slli_epi32(a_lo, 24);
            __m128i out_lo = _mm_or_si128(rgb_lo, ashift_lo);
            _mm_storeu_si128((__m128i *)(data + i), out_lo);
         }
         {
            __m128i rgb_hi = _mm_or_si128(_mm_or_si128(g_hi,
                                 _mm_slli_epi32(g_hi, 8)),
                                 _mm_slli_epi32(g_hi, 16));
            __m128i ashift_hi = _mm_slli_epi32(a_hi, 24);
            __m128i out_hi = _mm_or_si128(rgb_hi, ashift_hi);
            _mm_storeu_si128((__m128i *)(data + i + 4), out_hi);
         }
      }

      for (; i < width; i++)
      {
         uint32_t gray  = *decoded++;
         uint32_t alpha = *decoded++;
         data[i] = (gray * 0x010101u) | (alpha << 24);
      }
      return;
   }
#endif /* RPNG_HAVE_SSE2 */

#if RPNG_HAVE_NEON
   /*
    * NEON fast path: 8-bit gray+alpha → ARGB32, 8 pixels per iteration.
    *
    * Source: G0 A0 G1 A1 ... (2 bytes/pixel).
    * vld2_u8 de-interleaves: .val[0] = grays, .val[1] = alphas.
    * Expand gray to 0xAA_gg_gg_gg.
    */
   if (step == 1 && width >= 8)
   {
      unsigned w8 = width & ~7u;

      for (i = 0; i < w8; i += 8, decoded += 16)
      {
         uint8x8x2_t ga   = vld2_u8(decoded);
         uint8x8_t   gray8 = ga.val[0];
         uint8x8_t   alph8 = ga.val[1];

         uint16x8_t g16 = vmovl_u8(gray8);
         uint16x8_t a16 = vmovl_u8(alph8);

         uint32x4_t g_lo = vmovl_u16(vget_low_u16(g16));
         uint32x4_t g_hi = vmovl_u16(vget_high_u16(g16));
         uint32x4_t a_lo = vmovl_u16(vget_low_u16(a16));
         uint32x4_t a_hi = vmovl_u16(vget_high_u16(a16));

         /* gray * 0x010101 */
         uint32x4_t rgb_lo = vorrq_u32(vorrq_u32(g_lo,
                                 vshlq_n_u32(g_lo,  8)),
                                 vshlq_n_u32(g_lo, 16));
         uint32x4_t rgb_hi = vorrq_u32(vorrq_u32(g_hi,
                                 vshlq_n_u32(g_hi,  8)),
                                 vshlq_n_u32(g_hi, 16));

         uint32x4_t out_lo = vorrq_u32(rgb_lo, vshlq_n_u32(a_lo, 24));
         uint32x4_t out_hi = vorrq_u32(rgb_hi, vshlq_n_u32(a_hi, 24));

         vst1q_u32(data + i,     out_lo);
         vst1q_u32(data + i + 4, out_hi);
      }

      for (; i < width; i++)
      {
         uint32_t gray  = *decoded++;
         uint32_t alpha = *decoded++;
         data[i] = (gray * 0x010101u) | (alpha << 24);
      }
      return;
   }
#endif /* RPNG_HAVE_NEON */

   for (i = 0; i < width; i++)
   {
      uint32_t gray, alpha;
      gray     = *decoded; decoded += step;
      alpha    = *decoded; decoded += step;
      data[i]  = (gray * 0x010101u) | (alpha << 24);
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
            unsigned i;
            unsigned w = width / 8;
            for (i = 0; i < w; i++, decoded++)
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
               case 6:
                  data[5] = palette[(*decoded >> 2) & 1];
               case 5:
                  data[4] = palette[(*decoded >> 3) & 1];
               case 4:
                  data[3] = palette[(*decoded >> 4) & 1];
               case 3:
                  data[2] = palette[(*decoded >> 5) & 1];
               case 2:
                  data[1] = palette[(*decoded >> 6) & 1];
               case 1:
                  data[0] = palette[(*decoded >> 7) & 1];
                  break;
            }
         }
         break;

      case 2:
         {
            unsigned i;
            unsigned w = width / 4;
            for (i = 0; i < w; i++, decoded++)
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
               case 2:
                  data[1] = palette[(*decoded >> 4) & 3];
               case 1:
                  data[0] = palette[(*decoded >> 6) & 3];
                  break;
            }
         }
         break;

      case 4:
         {
            unsigned i;
            unsigned w = width / 2;
            for (i = 0; i < w; i++, decoded++)
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
            unsigned i;
            for (i = 0; i < width; i++, decoded++, data++)
               *data = palette[*decoded];
         }
         break;
   }
}

static void rpng_pass_geom(const struct png_ihdr *ihdr,
      unsigned width, unsigned height,
      unsigned *bpp_out, unsigned *pitch_out, size_t *pass_size)
{
   unsigned bpp      = 0;
   unsigned pitch    = 0;
   unsigned depth    = ihdr->depth;
   unsigned w        = ihdr->width;

   (void)width;
   (void)height;

   switch (ihdr->color_type)
   {
      case PNG_IHDR_COLOR_GRAY:
         bpp   = (depth     + 7) / 8;
         pitch = (w * depth + 7) / 8;
         break;
      case PNG_IHDR_COLOR_RGB:
         bpp   = (depth * 3     + 7) / 8;
         pitch = (w * depth * 3 + 7) / 8;
         break;
      case PNG_IHDR_COLOR_PLT:
         bpp   = (depth     + 7) / 8;
         pitch = (w * depth + 7) / 8;
         break;
      case PNG_IHDR_COLOR_GRAY_ALPHA:
         bpp   = (depth * 2     + 7) / 8;
         pitch = (w * depth * 2 + 7) / 8;
         break;
      case PNG_IHDR_COLOR_RGBA:
         bpp   = (depth * 4     + 7) / 8;
         pitch = (w * depth * 4 + 7) / 8;
         break;
      default:
         break;
   }

   if (pass_size)
      *pass_size = (pitch + 1) * ihdr->height;
   if (bpp_out)
      *bpp_out   = bpp;
   if (pitch_out)
      *pitch_out = pitch;
}

static void rpng_reverse_filter_adam7_deinterlace_pass(uint32_t *data,
      const struct png_ihdr *ihdr,
      const uint32_t *input, unsigned pass_width, unsigned pass_height,
      const struct adam7_pass *pass)
{
   unsigned x, y;
   unsigned stride_x = pass->stride_x;
   unsigned stride_y = pass->stride_y;
   unsigned img_w    = ihdr->width;

   data += pass->y * img_w + pass->x;

   for (y = 0; y < pass_height; y++, data += img_w * stride_y, input += pass_width)
   {
      uint32_t *out = data;
      for (x = 0; x < pass_width; x++, out += stride_x)
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
            pngp->pass_width * pngp->pass_height * sizeof(uint32_t))))
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

static int rpng_reverse_filter_copy_line(uint32_t *data,
      const struct png_ihdr *ihdr,
      struct rpng_process *pngp, unsigned filter)
{

   switch (filter)
   {
      case PNG_FILTER_NONE:
         memcpy(pngp->decoded_scanline, pngp->inflate_buf, pngp->pitch);
         break;

      case PNG_FILTER_SUB:
         /*
          * SUB: dst[j] = src[j] + dst[j-bpp]
          * Sequential data-dependency — not vectorisable with SSE2 or NEON.
          */
         {
            unsigned j;
            const uint8_t *src = pngp->inflate_buf;
            uint8_t       *dst = pngp->decoded_scanline;
            unsigned       bpp = pngp->bpp;
            unsigned       pit = pngp->pitch;
            for (j = 0; j < bpp; j++)
               dst[j] = src[j];
            for (j = bpp; j < pit; j++)
               dst[j] = src[j] + dst[j - bpp];
         }
         break;

      case PNG_FILTER_UP:
         /*
          * UP: dst[j] = src[j] + prev[j]
          * No data dependency → fully vectorisable on both SSE2 and NEON.
          */
         {
            const uint8_t *src  = pngp->inflate_buf;
            uint8_t       *dst  = pngp->decoded_scanline;
            const uint8_t *prev = pngp->prev_scanline;
            unsigned       pit  = pngp->pitch;
#if RPNG_HAVE_SSE2
            {
               unsigned j   = 0;
               unsigned pit16 = pit & ~15u;
               for (; j < pit16; j += 16)
               {
                  __m128i s = _mm_loadu_si128((const __m128i *)(src  + j));
                  __m128i p = _mm_loadu_si128((const __m128i *)(prev + j));
                  /* _mm_add_epi8 wraps modulo 256, exactly what PNG UP filter needs */
                  _mm_storeu_si128((__m128i *)(dst + j), _mm_add_epi8(s, p));
               }
               for (; j < pit; j++)
                  dst[j] = src[j] + prev[j];
            }
#elif RPNG_HAVE_NEON
            /*
             * NEON: vaddq_u8 on 16-byte (128-bit) vectors.
             * Identical semantics to SSE2 _mm_add_epi8: wraps mod 256.
             * Process 16 bytes per iteration; scalar tail for remainder.
             */
            {
               unsigned j     = 0;
               unsigned pit16 = pit & ~15u;
               for (; j < pit16; j += 16)
               {
                  uint8x16_t s = vld1q_u8(src  + j);
                  uint8x16_t p = vld1q_u8(prev + j);
                  vst1q_u8(dst + j, vaddq_u8(s, p));
               }
               /* Handle 8-byte chunk if remaining >= 8 */
               if (j + 8 <= pit)
               {
                  uint8x8_t s8 = vld1_u8(src  + j);
                  uint8x8_t p8 = vld1_u8(prev + j);
                  vst1_u8(dst + j, vadd_u8(s8, p8));
                  j += 8;
               }
               for (; j < pit; j++)
                  dst[j] = src[j] + prev[j];
            }
#else
            {
               unsigned j;
               for (j = 0; j < pit; j++)
                  dst[j] = src[j] + prev[j];
            }
#endif
         }
         break;

      case PNG_FILTER_AVERAGE:
         /*
          * AVERAGE: sequential output dependency — not vectorisable
          * with SSE2 or NEON in the general case.
          */
         {
            const uint8_t *src  = pngp->inflate_buf;
            uint8_t       *dst  = pngp->decoded_scanline;
            const uint8_t *prev = pngp->prev_scanline;
            unsigned       bpp  = pngp->bpp;
            unsigned       pit  = pngp->pitch;
            unsigned j;
            for (j = 0; j < bpp; j++)
               dst[j] = src[j] + (prev[j] >> 1);
            for (j = bpp; j < pit; j++)
               dst[j] = src[j] + (uint8_t)((dst[j - bpp] + prev[j]) >> 1);
         }
         break;

      case PNG_FILTER_PAETH:
         /*
          * PAETH: sequential dependency at distance bpp.
          * Paeth predictor is a non-linear function; not vectorisable.
          */
         {
            unsigned j;
            const uint8_t *src  = pngp->inflate_buf;
            uint8_t       *dst  = pngp->decoded_scanline;
            const uint8_t *prev = pngp->prev_scanline;
            unsigned       bpp  = pngp->bpp;
            unsigned       pit  = pngp->pitch;
            for (j = 0; j < bpp; j++)
               dst[j] = src[j] + prev[j];
            for (j = bpp; j < pit; j++)
               dst[j] = src[j] + paeth(dst[j - bpp], prev[j], prev[j - bpp]);
         }
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
         rpng_reverse_filter_copy_line_rgb(data, pngp->decoded_scanline, ihdr->width, ihdr->depth);
         break;
      case PNG_IHDR_COLOR_PLT:
         rpng_reverse_filter_copy_line_plt(
               data, pngp->decoded_scanline, ihdr->width,
               ihdr->depth, pngp->palette);
         break;
      case PNG_IHDR_COLOR_GRAY_ALPHA:
         rpng_reverse_filter_copy_line_gray_alpha(data, pngp->decoded_scanline, ihdr->width,
               ihdr->depth);
         break;
      case PNG_IHDR_COLOR_RGBA:
         rpng_reverse_filter_copy_line_rgba(data, pngp->decoded_scanline, ihdr->width, ihdr->depth);
         break;
   }

   memcpy(pngp->prev_scanline, pngp->decoded_scanline, pngp->pitch);

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
   bool to_continue        = (process->avail_in > 0
         && process->avail_out > 0);

   if (!to_continue)
      goto end;

   zstatus = process->stream_backend->trans(process->stream, false, &rd, &wn, &err);

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
   /* we often use these in textures, make sure they're 32-byte aligned */
   *data = (uint32_t*)memalign(32, rpng->ihdr.width *
         rpng->ihdr.height * sizeof(uint32_t));
#else
   *data = (uint32_t*)malloc(rpng->ihdr.width *
         rpng->ihdr.height * sizeof(uint32_t));
#endif
   if (!*data)
      goto false_end;

   process->adam7_restore_buf_size = 0;
   process->restore_buf_size       = 0;
   process->palette                = rpng->palette;

   if (rpng->ihdr.interlace != 1)
      if (rpng_reverse_filter_init(&rpng->ihdr, process) == -1)
         goto false_end;

   process->flags              |=  RPNG_PROCESS_FLAG_INFLATE_INITIALIZED;
   return 1;

error:
false_end:
   process->flags              &= ~RPNG_PROCESS_FLAG_INFLATE_INITIALIZED;
   return -1;
}

static bool rpng_realloc_idat(struct idat_buffer *buf, uint32_t chunk_size)
{
   uint8_t *new_buffer = (uint8_t*)realloc(buf->data, buf->size + chunk_size);

   if (!new_buffer)
      return false;

   buf->data  = new_buffer;
   return true;
}

static struct rpng_process *rpng_process_init(rpng_t *rpng)
{
   uint8_t *inflate_buf            = NULL;
   struct rpng_process *process    = (struct rpng_process*)malloc(sizeof(*process));

   if (!process)
      return NULL;

   memset(process, 0, sizeof(*process));

   process->stream_backend         = trans_stream_get_zlib_inflate_backend();

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
   uint8_t b0, b1, b2, b3;
   uint32_t tag;

   b0 = buf[4]; b1 = buf[5]; b2 = buf[6]; b3 = buf[7];

   /* All four bytes must be ASCII letters (65-90 or 97-122) */
   if (   ((b0 < 65) || ((b0 > 90) && (b0 < 97)) || (b0 > 122))
       || ((b1 < 65) || ((b1 > 90) && (b1 < 97)) || (b1 > 122))
       || ((b2 < 65) || ((b2 > 90) && (b2 < 97)) || (b2 > 122))
       || ((b3 < 65) || ((b3 > 90) && (b3 < 97)) || (b3 > 122)))
      return PNG_CHUNK_ERROR;

   /* Pack four bytes into one word for fast comparison */
   tag = ((uint32_t)b0 << 24) | ((uint32_t)b1 << 16)
       | ((uint32_t)b2 <<  8) |  (uint32_t)b3;

#define MAKE_TAG(a,b,c,d) \
   (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | ((uint32_t)(c) << 8) | (uint32_t)(d))

   switch (tag)
   {
      case MAKE_TAG('I','H','D','R'): return PNG_CHUNK_IHDR;
      case MAKE_TAG('I','D','A','T'): return PNG_CHUNK_IDAT;
      case MAKE_TAG('I','E','N','D'): return PNG_CHUNK_IEND;
      case MAKE_TAG('P','L','T','E'): return PNG_CHUNK_PLTE;
      case MAKE_TAG('t','R','N','S'): return PNG_CHUNK_tRNS;
      default:                        break;
   }

#undef MAKE_TAG

   return PNG_CHUNK_NOOP;
}

bool rpng_iterate_image(rpng_t *rpng)
{
   uint8_t *buf             = (uint8_t*)rpng->buff_data;
   uint32_t chunk_size      = 0;

   /* Check whether data buffer pointer is valid */
   if (buf > rpng->buff_end)
      return false;

   /* Check whether reading the header will overflow
    * the data buffer */
   if (rpng->buff_end - buf < 8)
      return false;

   chunk_size = rpng_dword_be(buf);

   /* Check whether chunk will overflow the data buffer */
   if (buf + 8 + chunk_size > rpng->buff_end)
      return false;

   switch (rpng_read_chunk_header(buf, chunk_size))
   {
      case PNG_CHUNK_NOOP:
      default:
         break;

      case PNG_CHUNK_ERROR:
         return false;

      case PNG_CHUNK_IHDR:
         if (     (rpng->flags & RPNG_FLAG_HAS_IHDR)
               || (rpng->flags & RPNG_FLAG_HAS_IDAT)
               || (rpng->flags & RPNG_FLAG_HAS_IEND))
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

         if (     rpng->ihdr.width  == 0
               || rpng->ihdr.height == 0
               /* ensure multiplications don't overflow and wrap around, that'd give buffer overflow crashes */
               || (uint64_t)rpng->ihdr.width*rpng->ihdr.height*sizeof(uint32_t) >= 0x80000000)
            return false;

         if (!rpng_process_ihdr(&rpng->ihdr))
            return false;

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

            if (     !(rpng->flags & RPNG_FLAG_HAS_IHDR)
                  ||  (rpng->flags & RPNG_FLAG_HAS_PLTE)
                  ||  (rpng->flags & RPNG_FLAG_HAS_IEND)
                  ||  (rpng->flags & RPNG_FLAG_HAS_IDAT)
                  ||  (rpng->flags & RPNG_FLAG_HAS_TRNS))
               return false;

            buf += 8;

            for (i = 0; i < (int)entries; i++, buf += 3)
            {
               uint32_t r       = buf[0];
               uint32_t g       = buf[1];
               uint32_t b       = buf[2];
               rpng->palette[i] = (0xffu << 24) | (r << 16) | (g << 8) | b;
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

      case PNG_CHUNK_IDAT:
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
         if (     !(rpng->flags & RPNG_FLAG_HAS_IHDR)
               || !(rpng->flags & RPNG_FLAG_HAS_IDAT))
            return false;

         rpng->flags         |= RPNG_FLAG_HAS_IEND;
         return false;
   }

   rpng->buff_data += chunk_size + 12;

   /* Check whether data buffer pointer is valid */
   if (rpng->buff_data > rpng->buff_end)
      return false;
   return true;
}

int rpng_process_image(rpng_t *rpng,
      void **_data, size_t len, unsigned *width, unsigned *height)
{
   uint32_t **data = (uint32_t**)_data;

   if (!rpng->process)
   {
      struct rpng_process *process = rpng_process_init(rpng);

      if (!process)
         goto error;

      rpng->process = process;
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
   return rpng_reverse_filter_regular_iterate(data, &rpng->ihdr, rpng->process);

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
         if (rpng->process->stream_backend && rpng->process->stream_backend->stream_free)
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
   return (rpng && ((rpng->flags & (RPNG_FLAG_HAS_IHDR | RPNG_FLAG_HAS_IDAT |
RPNG_FLAG_HAS_IEND)) > 0));
}

bool rpng_set_buf_ptr(rpng_t *rpng, void *data, size_t len)
{
   if (!rpng || (len < 1))
      return false;

   rpng->buff_data = (uint8_t*)data;
   rpng->buff_end  = rpng->buff_data + (len - 1);

   return true;
}

rpng_t *rpng_alloc(void)
{
   rpng_t *rpng = (rpng_t*)calloc(1, sizeof(*rpng));
   if (!rpng)
      return NULL;
   return rpng;
}
