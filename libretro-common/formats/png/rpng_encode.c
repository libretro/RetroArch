/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rpng_encode.c).
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <encodings/crc32.h>
#include <streams/interface_stream.h>
#include <streams/trans_stream.h>

/* SIMD acceleration: SSE2 on x86/x86-64, NEON on ARM.  Same gating as
 * the decoder in rpng.c. */
#if defined(__SSE2__)
#include <emmintrin.h>
#define RPNG_ENC_SSE2 1
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#if !defined(VITA) && !defined(WEBOS) && !defined(HAVE_LIBNX)
#include <arm_neon.h>
#define RPNG_ENC_NEON 1
#endif
#endif

#include "rpng_internal.h"

#undef GOTO_END_ERROR
#define GOTO_END_ERROR() do { \
   fprintf(stderr, "[RPNG] Error in line %d.\n", __LINE__); \
   ret = false; \
   goto end; \
} while (0)

static const double DEFLATE_PADDING = 1.1;
static const int PNG_ROUGH_HEADER  = 100;

static void dword_write_be(uint8_t *buf, uint32_t val)
{
   *buf++ = (uint8_t)(val >> 24);
   *buf++ = (uint8_t)(val >> 16);
   *buf++ = (uint8_t)(val >>  8);
   *buf++ = (uint8_t)(val >>  0);
}

static bool png_write_crc_string(intfstream_t *intf_s, const uint8_t *data, size_t len)
{
   uint8_t crc_raw[4] = {0};
   uint32_t crc       = encoding_crc32(0, data, len);

   dword_write_be(crc_raw, crc);
   return intfstream_write(intf_s, crc_raw, sizeof(crc_raw)) == sizeof(crc_raw);
}

static bool png_write_ihdr_string(intfstream_t *intf_s, const struct png_ihdr *ihdr)
{
   uint8_t ihdr_raw[21];

   ihdr_raw[0]  = '0';                 /* Size */
   ihdr_raw[1]  = '0';
   ihdr_raw[2]  = '0';
   ihdr_raw[3]  = '0';
   ihdr_raw[4]  = 'I';
   ihdr_raw[5]  = 'H';
   ihdr_raw[6]  = 'D';
   ihdr_raw[7]  = 'R';
   ihdr_raw[8]  =   0;                 /* Width */
   ihdr_raw[9]  =   0;
   ihdr_raw[10] =   0;
   ihdr_raw[11] =   0;
   ihdr_raw[12] =   0;                 /* Height */
   ihdr_raw[13] =   0;
   ihdr_raw[14] =   0;
   ihdr_raw[15] =   0;
   ihdr_raw[16] =   ihdr->depth;       /* Depth */
   ihdr_raw[17] =   ihdr->color_type;
   ihdr_raw[18] =   ihdr->compression;
   ihdr_raw[19] =   ihdr->filter;
   ihdr_raw[20] =   ihdr->interlace;

   dword_write_be(ihdr_raw +  0, sizeof(ihdr_raw) - 8);
   dword_write_be(ihdr_raw +  8, ihdr->width);
   dword_write_be(ihdr_raw + 12, ihdr->height);
   if (intfstream_write(intf_s, ihdr_raw, sizeof(ihdr_raw)) != sizeof(ihdr_raw))
      return false;

   return png_write_crc_string(intf_s, ihdr_raw + sizeof(uint32_t),
         sizeof(ihdr_raw) - sizeof(uint32_t));
}

/* --- PNG 3rd-edition HDR colour-space chunks --------------------------
 * These are ancillary chunks placed after IHDR and before IDAT. They only
 * label the colour space / light levels; they never touch the pixel data.
 * Each is framed exactly like IHDR: [len(4)][type(4)][payload][crc(4)],
 * with the CRC computed over type+payload. */

static bool png_write_cicp_string(intfstream_t *intf_s,
      const struct rpng_hdr_metadata *hdr)
{
   /* 4-byte payload: primaries, transfer, matrix (0 for PNG), full range. */
   uint8_t raw[12];
   dword_write_be(raw + 0, 4);
   raw[4]  = 'c'; raw[5] = 'I'; raw[6] = 'C'; raw[7] = 'P';
   raw[8]  = hdr->colour_primaries;
   raw[9]  = hdr->transfer_function;
   raw[10] = hdr->matrix_coefficients;
   raw[11] = hdr->video_full_range_flag;
   if (intfstream_write(intf_s, raw, sizeof(raw)) != sizeof(raw))
      return false;
   return png_write_crc_string(intf_s, raw + 4, sizeof(raw) - 4);
}

static bool png_write_clli_string(intfstream_t *intf_s,
      const struct rpng_hdr_metadata *hdr)
{
   /* 8-byte payload: MaxCLL, MaxFALL, each a 4-byte unsigned integer in
    * units of 0.0001 cd/m^2 (i.e. cd/m^2 * 10000). */
   uint8_t raw[16];
   dword_write_be(raw + 0, 8);
   raw[4] = 'c'; raw[5] = 'L'; raw[6] = 'L'; raw[7] = 'I';
   dword_write_be(raw +  8, (uint32_t)(hdr->max_cll  * 10000.0f + 0.5f));
   dword_write_be(raw + 12, (uint32_t)(hdr->max_fall * 10000.0f + 0.5f));
   if (intfstream_write(intf_s, raw, sizeof(raw)) != sizeof(raw))
      return false;
   return png_write_crc_string(intf_s, raw + 4, sizeof(raw) - 4);
}

static void word_write_be(uint8_t *buf, uint16_t val)
{
   buf[0] = (uint8_t)(val >> 8);
   buf[1] = (uint8_t)(val >> 0);
}

static bool png_write_mdcv_string(intfstream_t *intf_s,
      const struct rpng_hdr_metadata *hdr)
{
   /* 24-byte payload: R,G,B chromaticity pairs then white point, each xy as
    * a 2-byte unsigned integer in units of 0.00002 (value * 50000); then max
    * and min luminance as 4-byte unsigned integers in units of 0.0001
    * cd/m^2 (value * 10000). */
   uint8_t raw[8 + 24];
   int c;
   dword_write_be(raw + 0, 24);
   raw[4] = 'm'; raw[5] = 'D'; raw[6] = 'C'; raw[7] = 'V';
   for (c = 0; c < 3; c++)
   {
      word_write_be(raw + 8 + c * 4 + 0,
            (uint16_t)(hdr->primary_chromaticity[c][0] * 50000.0f + 0.5f));
      word_write_be(raw + 8 + c * 4 + 2,
            (uint16_t)(hdr->primary_chromaticity[c][1] * 50000.0f + 0.5f));
   }
   word_write_be(raw + 20, (uint16_t)(hdr->white_point[0] * 50000.0f + 0.5f));
   word_write_be(raw + 22, (uint16_t)(hdr->white_point[1] * 50000.0f + 0.5f));
   dword_write_be(raw + 24, (uint32_t)(hdr->max_luminance * 10000.0f + 0.5f));
   dword_write_be(raw + 28, (uint32_t)(hdr->min_luminance * 10000.0f + 0.5f));
   if (intfstream_write(intf_s, raw, sizeof(raw)) != sizeof(raw))
      return false;
   return png_write_crc_string(intf_s, raw + 4, sizeof(raw) - 4);
}

static bool png_write_hdr_chunks(intfstream_t *intf_s,
      const struct rpng_hdr_metadata *hdr)
{
   if (!png_write_cicp_string(intf_s, hdr))
      return false;
   if (hdr->max_cll != 0.0f || hdr->max_fall != 0.0f)
      if (!png_write_clli_string(intf_s, hdr))
         return false;
   if (hdr->write_mdcv)
      if (!png_write_mdcv_string(intf_s, hdr))
         return false;
   return true;
}

static bool png_write_idat_string(intfstream_t* intf_s, const uint8_t *data, size_t len)
{
   if (intfstream_write(intf_s, data, len) != (ssize_t)len)
      return false;
   return png_write_crc_string(intf_s, data + sizeof(uint32_t), len - sizeof(uint32_t));
}

static bool png_write_iend_string(intfstream_t* intf_s)
{
   const uint8_t data[] = {
      0, 0, 0, 0,
      'I', 'E', 'N', 'D',
   };

   if (intfstream_write(intf_s, data, sizeof(data)) != sizeof(data))
      return false;

   return png_write_crc_string(intf_s, data + sizeof(uint32_t),
         sizeof(data) - sizeof(uint32_t));
}

/* Source pixel format.  Bytes-per-pixel alone cannot select the 32-bit
 * path: ARGB32 and RGBA32 are both four bytes per pixel and differ only
 * in channel order, so the format is carried separately and the depth
 * derived from it. */
enum rpng_pixfmt
{
   RPNG_PIXFMT_BGR24 = 0,
   RPNG_PIXFMT_ARGB32,
   RPNG_PIXFMT_RGBA32,
   RPNG_PIXFMT_RGB48
};

static unsigned rpng_pixfmt_bpp(enum rpng_pixfmt fmt)
{
   switch (fmt)
   {
      case RPNG_PIXFMT_ARGB32:
      case RPNG_PIXFMT_RGBA32:
         return 4;
      case RPNG_PIXFMT_RGB48:
         return 6;
      case RPNG_PIXFMT_BGR24:
      default:
         break;
   }
   return 3;
}

static void copy_argb_line(uint8_t *dst, const uint32_t *src, unsigned width)
{
   unsigned i;
   for (i = 0; i < width; i++)
   {
      uint32_t col = src[i];
      *dst++ = (uint8_t)(col >> 16);
      *dst++ = (uint8_t)(col >>  8);
      *dst++ = (uint8_t)(col >>  0);
      *dst++ = (uint8_t)(col >> 24);
   }
}

/* RGBA32 is already the on-wire channel order for colour type 6 - R,G,B,A,
 * one byte each, in memory order - so the row needs no swizzle at all,
 * only the move into the filter scratch buffer.
 *
 * That move cannot be elided by pointing rgba_line at the caller's row:
 * the winning filter's tag byte is written at chosen_filtered[-1], and
 * when the "none" filter wins chosen_filtered *is* rgba_line, which would
 * scribble one byte into the caller's surface. */
static void copy_rgba_line(uint8_t *dst, const uint8_t *src, unsigned width)
{
   memcpy(dst, src, (size_t)width * 4);
}

static void copy_bgr24_line(uint8_t *dst, const uint8_t *src, unsigned width)
{
   unsigned i;
   for (i = 0; i < width; i++, dst += 3, src += 3)
   {
      dst[2] = src[0];
      dst[1] = src[1];
      dst[0] = src[2];
   }
}

/* 16-bit-per-channel RGB. PNG stores 16-bit samples big-endian, so each
 * host-order R,G,B sample is written most-significant byte first. Input is
 * three uint16_t per pixel in R,G,B order (already the PNG channel order,
 * unlike the BGR 8-bit path). */
static void copy_rgb48_line(uint8_t *dst, const uint16_t *src, unsigned width)
{
   unsigned i;
   for (i = 0; i < width; i++, dst += 6, src += 3)
   {
      dst[0] = (uint8_t)(src[0] >> 8); dst[1] = (uint8_t)(src[0] & 0xff);
      dst[2] = (uint8_t)(src[1] >> 8); dst[3] = (uint8_t)(src[1] & 0xff);
      dst[4] = (uint8_t)(src[2] >> 8); dst[5] = (uint8_t)(src[2] & 0xff);
   }
}

/* Forward filtering, fused with the sum-of-absolute-differences score
 * that picks between the results.
 *
 * The encode direction has no serial dependency - every predictor input
 * is a *raw* byte, not a byte this pass just produced - so unlike the
 * decoder these loops vectorise across the full register width.  Fusing
 * the score in removes four extra passes over the row: the old shape
 * wrote each filtered row and then read it back to score it.
 *
 * The score is sum(|(int8)b|), the standard minimum-sum-of-absolute-
 * differences heuristic.  |(int8)b| is min(b, 256 - b) on unsigned
 * bytes, which is one min and one subtract, and psadbw then accumulates
 * a whole register with no widening. */

#if defined(RPNG_ENC_SSE2)
static INLINE __m128i rpng_abs_i8(__m128i v)
{
   return _mm_min_epu8(v, _mm_sub_epi8(_mm_setzero_si128(), v));
}

static INLINE unsigned rpng_sad_final(__m128i acc)
{
   return (unsigned)_mm_cvtsi128_si32(acc)
        + (unsigned)_mm_cvtsi128_si32(_mm_srli_si128(acc, 8));
}

/* Branch-free Paeth predictor over 8 unsigned 16-bit lanes; same
 * identity and selection order as the decoder's. */
static INLINE __m128i rpng_paeth_pred_epi16(__m128i a, __m128i b, __m128i c)
{
   __m128i bc     = _mm_sub_epi16(b, c);
   __m128i ac     = _mm_sub_epi16(a, c);
   __m128i sm     = _mm_add_epi16(bc, ac);
   __m128i z      = _mm_setzero_si128();
   __m128i pa     = _mm_max_epi16(bc, _mm_sub_epi16(z, bc));
   __m128i pb     = _mm_max_epi16(ac, _mm_sub_epi16(z, ac));
   __m128i pc     = _mm_max_epi16(sm, _mm_sub_epi16(z, sm));
   __m128i not_a  = _mm_or_si128(_mm_cmpgt_epi16(pa, pb),
                                 _mm_cmpgt_epi16(pa, pc));
   __m128i pick_c = _mm_cmpgt_epi16(pb, pc);
   __m128i bc_sel = _mm_or_si128(_mm_andnot_si128(pick_c, b),
                                 _mm_and_si128(   pick_c, c));
   return           _mm_or_si128(_mm_andnot_si128(not_a,  a),
                                 _mm_and_si128(   not_a,  bc_sel));
}
#endif

#if defined(RPNG_ENC_NEON)
static INLINE uint8x16_t rpng_abs_i8(uint8x16_t v)
{
   return vminq_u8(v, vsubq_u8(vdupq_n_u8(0), v));
}

static INLINE uint16x8_t rpng_paeth_pred_u16(
      uint16x8_t a, uint16x8_t b, uint16x8_t c)
{
   int16x8_t  bc     = vsubq_s16(vreinterpretq_s16_u16(b),
                                 vreinterpretq_s16_u16(c));
   int16x8_t  ac     = vsubq_s16(vreinterpretq_s16_u16(a),
                                 vreinterpretq_s16_u16(c));
   int16x8_t  sm     = vaddq_s16(bc, ac);
   uint16x8_t pa     = vreinterpretq_u16_s16(vabsq_s16(bc));
   uint16x8_t pb     = vreinterpretq_u16_s16(vabsq_s16(ac));
   uint16x8_t pc     = vreinterpretq_u16_s16(vabsq_s16(sm));
   uint16x8_t not_a  = vorrq_u16(vcgtq_u16(pa, pb), vcgtq_u16(pa, pc));
   uint16x8_t pick_c = vcgtq_u16(pb, pc);
   uint16x8_t bc_sel = vbslq_u16(pick_c, c, b);
   return              vbslq_u16(not_a,  bc_sel, a);
}
#endif

/* |(int8)b| without a branch, for the scalar paths.  The conditional
 * form the old code used compiled to a branch that mispredicts on image
 * data as often as not. */
static INLINE unsigned rpng_abs8(uint8_t b)
{
   int v = (int)(int8_t)b;
   int m = v >> (sizeof(int) * 8 - 1);
   return (unsigned)((v + m) ^ m);
}

static INLINE int rpng_paeth_pred(int a, int b, int c)
{
   int pa   = b - c;
   int pb   = a - c;
   int pc   = pa + pb;
   int apa  = pa < 0 ? -pa : pa;
   int apb  = pb < 0 ? -pb : pb;
   int apc  = pc < 0 ? -pc : pc;
   int nota = -(int)((apa > apb) | (apa > apc));
   int pick = -(int)(apb > apc);
   int bc   = (b & ~pick) | (c & pick);
   return (a & ~nota) | (bc & nota);
}

static unsigned count_sad(const uint8_t *data, size_t len)
{
   size_t i = 0;
   unsigned cnt = 0;
#if defined(RPNG_ENC_SSE2)
   {
      __m128i acc = _mm_setzero_si128();
      for (; i + 16 <= len; i += 16)
         acc = _mm_add_epi64(acc, _mm_sad_epu8(
                  rpng_abs_i8(_mm_loadu_si128((const __m128i*)(data + i))),
                  _mm_setzero_si128()));
      cnt = rpng_sad_final(acc);
   }
#elif defined(RPNG_ENC_NEON)
   {
      uint32x4_t acc = vdupq_n_u32(0);
      for (; i + 16 <= len; i += 16)
         acc = vpadalq_u16(acc, vpaddlq_u8(rpng_abs_i8(vld1q_u8(data + i))));
      cnt = vgetq_lane_u32(acc, 0) + vgetq_lane_u32(acc, 1)
          + vgetq_lane_u32(acc, 2) + vgetq_lane_u32(acc, 3);
   }
#endif
   for (; i < len; i++)
      cnt += rpng_abs8(data[i]);
   return cnt;
}

static unsigned filter_up(uint8_t *target, const uint8_t *line,
      const uint8_t *prev, unsigned width, unsigned bpp)
{
   size_t i = 0, len = (size_t)width * bpp;
   unsigned cnt = 0;
#if defined(RPNG_ENC_SSE2)
   {
      __m128i acc = _mm_setzero_si128();
      for (; i + 16 <= len; i += 16)
      {
         __m128i v = _mm_sub_epi8(
               _mm_loadu_si128((const __m128i*)(line + i)),
               _mm_loadu_si128((const __m128i*)(prev + i)));
         _mm_storeu_si128((__m128i*)(target + i), v);
         acc = _mm_add_epi64(acc,
               _mm_sad_epu8(rpng_abs_i8(v), _mm_setzero_si128()));
      }
      cnt = rpng_sad_final(acc);
   }
#elif defined(RPNG_ENC_NEON)
   {
      uint32x4_t acc = vdupq_n_u32(0);
      for (; i + 16 <= len; i += 16)
      {
         uint8x16_t v = vsubq_u8(vld1q_u8(line + i), vld1q_u8(prev + i));
         vst1q_u8(target + i, v);
         acc = vpadalq_u16(acc, vpaddlq_u8(rpng_abs_i8(v)));
      }
      cnt = vgetq_lane_u32(acc, 0) + vgetq_lane_u32(acc, 1)
          + vgetq_lane_u32(acc, 2) + vgetq_lane_u32(acc, 3);
   }
#endif
   for (; i < len; i++)
   {
      target[i] = (uint8_t)(line[i] - prev[i]);
      cnt      += rpng_abs8(target[i]);
   }
   return cnt;
}

static unsigned filter_sub(uint8_t *target, const uint8_t *line,
      unsigned width, unsigned bpp)
{
   size_t i, len = (size_t)width * bpp;
   unsigned cnt = 0;
   for (i = 0; i < bpp && i < len; i++)
   {
      target[i] = line[i];
      cnt      += rpng_abs8(target[i]);
   }
#if defined(RPNG_ENC_SSE2)
   {
      __m128i acc = _mm_setzero_si128();
      for (; i + 16 <= len; i += 16)
      {
         __m128i v = _mm_sub_epi8(
               _mm_loadu_si128((const __m128i*)(line + i)),
               _mm_loadu_si128((const __m128i*)(line + i - bpp)));
         _mm_storeu_si128((__m128i*)(target + i), v);
         acc = _mm_add_epi64(acc,
               _mm_sad_epu8(rpng_abs_i8(v), _mm_setzero_si128()));
      }
      cnt += rpng_sad_final(acc);
   }
#elif defined(RPNG_ENC_NEON)
   {
      uint32x4_t acc = vdupq_n_u32(0);
      for (; i + 16 <= len; i += 16)
      {
         uint8x16_t v = vsubq_u8(vld1q_u8(line + i), vld1q_u8(line + i - bpp));
         vst1q_u8(target + i, v);
         acc = vpadalq_u16(acc, vpaddlq_u8(rpng_abs_i8(v)));
      }
      cnt += vgetq_lane_u32(acc, 0) + vgetq_lane_u32(acc, 1)
           + vgetq_lane_u32(acc, 2) + vgetq_lane_u32(acc, 3);
   }
#endif
   for (; i < len; i++)
   {
      target[i] = (uint8_t)(line[i] - line[i - bpp]);
      cnt      += rpng_abs8(target[i]);
   }
   return cnt;
}

static unsigned filter_avg(uint8_t *target, const uint8_t *line,
      const uint8_t *prev, unsigned width, unsigned bpp)
{
   size_t i, len = (size_t)width * bpp;
   unsigned cnt = 0;
   for (i = 0; i < bpp && i < len; i++)
   {
      target[i] = (uint8_t)(line[i] - (prev[i] >> 1));
      cnt      += rpng_abs8(target[i]);
   }
#if defined(RPNG_ENC_SSE2)
   {
      /* floor((l + p) / 2) == pavgb(l, p) - ((l ^ p) & 1), pavgb being
       * the rounding-up form. */
      const __m128i one = _mm_set1_epi8(1);
      __m128i acc = _mm_setzero_si128();
      for (; i + 16 <= len; i += 16)
      {
         __m128i l = _mm_loadu_si128((const __m128i*)(line + i - bpp));
         __m128i p = _mm_loadu_si128((const __m128i*)(prev + i));
         __m128i m = _mm_sub_epi8(_mm_avg_epu8(l, p),
               _mm_and_si128(_mm_xor_si128(l, p), one));
         __m128i v = _mm_sub_epi8(
               _mm_loadu_si128((const __m128i*)(line + i)), m);
         _mm_storeu_si128((__m128i*)(target + i), v);
         acc = _mm_add_epi64(acc,
               _mm_sad_epu8(rpng_abs_i8(v), _mm_setzero_si128()));
      }
      cnt += rpng_sad_final(acc);
   }
#elif defined(RPNG_ENC_NEON)
   {
      /* vhaddq_u8 is already the flooring halving add. */
      uint32x4_t acc = vdupq_n_u32(0);
      for (; i + 16 <= len; i += 16)
      {
         uint8x16_t m = vhaddq_u8(vld1q_u8(line + i - bpp), vld1q_u8(prev + i));
         uint8x16_t v = vsubq_u8(vld1q_u8(line + i), m);
         vst1q_u8(target + i, v);
         acc = vpadalq_u16(acc, vpaddlq_u8(rpng_abs_i8(v)));
      }
      cnt += vgetq_lane_u32(acc, 0) + vgetq_lane_u32(acc, 1)
           + vgetq_lane_u32(acc, 2) + vgetq_lane_u32(acc, 3);
   }
#endif
   for (; i < len; i++)
   {
      target[i] = (uint8_t)(line[i] - ((line[i - bpp] + prev[i]) >> 1));
      cnt      += rpng_abs8(target[i]);
   }
   return cnt;
}

static unsigned filter_paeth(uint8_t *target,
      const uint8_t *line, const uint8_t *prev,
      unsigned width, unsigned bpp)
{
   size_t i, len = (size_t)width * bpp;
   unsigned cnt = 0;
   for (i = 0; i < bpp && i < len; i++)
   {
      target[i] = (uint8_t)(line[i] - prev[i]);  /* paeth(0, b, 0) == b */
      cnt      += rpng_abs8(target[i]);
   }
#if defined(RPNG_ENC_SSE2)
   {
      const __m128i z = _mm_setzero_si128();
      __m128i acc = _mm_setzero_si128();
      for (; i + 8 <= len; i += 8)
      {
         __m128i a  = _mm_unpacklo_epi8(
               _mm_loadl_epi64((const __m128i*)(line + i - bpp)), z);
         __m128i b  = _mm_unpacklo_epi8(
               _mm_loadl_epi64((const __m128i*)(prev + i)), z);
         __m128i c  = _mm_unpacklo_epi8(
               _mm_loadl_epi64((const __m128i*)(prev + i - bpp)), z);
         __m128i pr = _mm_packus_epi16(rpng_paeth_pred_epi16(a, b, c), z);
         __m128i v  = _mm_sub_epi8(
               _mm_loadl_epi64((const __m128i*)(line + i)), pr);
         _mm_storel_epi64((__m128i*)(target + i), v);
         acc = _mm_add_epi64(acc,
               _mm_sad_epu8(rpng_abs_i8(_mm_unpacklo_epi64(v, z)), z));
      }
      cnt += rpng_sad_final(acc);
   }
#elif defined(RPNG_ENC_NEON)
   {
      uint32x4_t acc = vdupq_n_u32(0);
      for (; i + 8 <= len; i += 8)
      {
         uint16x8_t a  = vmovl_u8(vld1_u8(line + i - bpp));
         uint16x8_t b  = vmovl_u8(vld1_u8(prev + i));
         uint16x8_t c  = vmovl_u8(vld1_u8(prev + i - bpp));
         uint8x8_t  pr = vmovn_u16(rpng_paeth_pred_u16(a, b, c));
         uint8x8_t  v  = vsub_u8(vld1_u8(line + i), pr);
         vst1_u8(target + i, v);
         acc = vpadalq_u16(acc, vpaddlq_u8(
                  rpng_abs_i8(vcombine_u8(v, vdup_n_u8(0)))));
      }
      cnt += vgetq_lane_u32(acc, 0) + vgetq_lane_u32(acc, 1)
           + vgetq_lane_u32(acc, 2) + vgetq_lane_u32(acc, 3);
   }
#endif
   for (; i < len; i++)
   {
      target[i] = (uint8_t)(line[i] - rpng_paeth_pred(
               line[i - bpp], prev[i], prev[i - bpp]));
      cnt      += rpng_abs8(target[i]);
   }
   return cnt;
}

/* Size of the per-chunk deflate output buffer.  A screenshot-sized
 * encode will fill this many times over and produce multiple IDAT
 * chunks; smaller than zlib's default window (32 KiB) to keep
 * peak memory low while large enough to amortise chunk-header
 * overhead (12 bytes per IDAT). */
#define IDAT_CHUNK_SIZE 16384

/* Emit one IDAT chunk.  `chunk_buf` is laid out as:
 *     bytes [0..4): length field (filled in here, big-endian)
 *     bytes [4..8): literal "IDAT"
 *     bytes [8..8+payload_len): the deflate output produced by
 *                               this chunk's worth of trans() calls
 * Matches the layout png_write_idat_string expects. */
static bool flush_idat_chunk(intfstream_t *intf_s,
      uint8_t *chunk_buf, size_t payload_len)
{
   if (payload_len == 0)
      return true; /* empty chunk -- nothing to emit, not an error */
   dword_write_be(chunk_buf + 0, (uint32_t)payload_len);
   memcpy(chunk_buf + 4, "IDAT", 4);
   return png_write_idat_string(intf_s, chunk_buf, payload_len + 8);
}

static bool rpng_save_image_stream_fmt(const uint8_t *data,
      intfstream_t* intf_s, unsigned width, unsigned height, signed pitch,
      enum rpng_pixfmt fmt, const struct rpng_hdr_metadata *hdr)
{
   unsigned h;
   unsigned bpp = rpng_pixfmt_bpp(fmt);
   struct png_ihdr ihdr = {0};
   bool ret = true;
   const struct trans_stream_backend *stream_backend = NULL;
   uint8_t *rgba_line        = NULL;
   uint8_t *prev_base        = NULL;
   uint8_t *rgba_base        = NULL;
   uint8_t *up_base          = NULL;
   uint8_t *sub_base         = NULL;
   uint8_t *avg_base         = NULL;
   uint8_t *paeth_base       = NULL;
   uint8_t *up_filtered      = NULL;
   uint8_t *sub_filtered     = NULL;
   uint8_t *avg_filtered     = NULL;
   uint8_t *paeth_filtered   = NULL;
   uint8_t *prev_encoded     = NULL;
   /* chunk_buf is the IDAT-chunk staging buffer:
    *   [0..4):        length field (filled in at flush time)
    *   [4..8):        "IDAT"
    *   [8..8+IDAT_CHUNK_SIZE): deflate output */
   uint8_t *chunk_buf        = NULL;
   void *stream              = NULL;
   size_t line_len           = (size_t)width * bpp;
   /* How many bytes deflate has produced into the current chunk_buf
    * since the last set_out.  Reset to 0 after every flush_idat_chunk. */
   size_t chunk_fill         = 0;
   enum trans_stream_error err = TRANS_STREAM_ERROR_NONE;

   if (!intf_s)
      GOTO_END_ERROR();

   stream_backend = trans_stream_get_zlib_deflate_backend();

   if (intfstream_write(intf_s, png_magic, sizeof(png_magic)) != sizeof(png_magic))
      GOTO_END_ERROR();

   ihdr.width      = width;
   ihdr.height     = height;
   /* bpp is bytes per pixel: 6 = 16-bit RGB, 4 = 8-bit RGBA, 3 = 8-bit RGB. */
   ihdr.depth      = (bpp == 6) ? 16 : 8;
   ihdr.color_type = (bpp == sizeof(uint32_t)) ? 6 : 2; /* RGBA or RGB */
   if (!png_write_ihdr_string(intf_s, &ihdr))
      GOTO_END_ERROR();

   /* HDR colour-space chunks (cICP / cLLI / mDCV) belong after IHDR and
    * before IDAT. Only written when the caller supplied metadata. */
   if (hdr)
      if (!png_write_hdr_chunks(intf_s, hdr))
         GOTO_END_ERROR();

   /* Per-row scratch.  ~width*bpp each -- trivial compared to the
    * frame-sized encode_buf the old full-buffer path allocated. */
   /* Every row buffer carries one spare byte in front so the PNG filter
    * tag can be written immediately before the filtered data and the
    * whole thing handed to deflate as one span.  The old shape copied
    * the winning row into a separate tag+data buffer, a full extra pass
    * over the row for nothing. */
   prev_base      = (uint8_t*)calloc(1, line_len + 1);
   rgba_base      = (uint8_t*)malloc(line_len + 1);
   up_base        = (uint8_t*)malloc(line_len + 1);
   sub_base       = (uint8_t*)malloc(line_len + 1);
   avg_base       = (uint8_t*)malloc(line_len + 1);
   paeth_base     = (uint8_t*)malloc(line_len + 1);
   chunk_buf      = (uint8_t*)malloc(IDAT_CHUNK_SIZE + 8);
   if (!prev_base || !rgba_base || !up_base || !sub_base
         || !avg_base || !paeth_base || !chunk_buf)
      GOTO_END_ERROR();

   prev_encoded   = prev_base  + 1;
   rgba_line      = rgba_base  + 1;
   up_filtered    = up_base    + 1;
   sub_filtered   = sub_base   + 1;
   avg_filtered   = avg_base   + 1;
   paeth_filtered = paeth_base + 1;

   stream = stream_backend->stream_new();

   /* Both deflate backends default to level 9, which is the wrong
    * trade for a screenshot: it is a foreground action the user waits
    * on, and on the content emulator screenshots actually contain the
    * top levels buy nothing.  Measured at 1920x1080 and 3840x2160,
    * BGR24, against libpng at its own default for scale:
    *
    *                  libpng        level 6        level 9
    *   2D pixel art    59 ms/0.23    67 ms/0.23    591 ms/0.23
    *   pixel art 4K   242 ms/0.88   272 ms/0.88   2221 ms/0.86
    *   smooth ramp    460 ms/1.00   432 ms/1.00   2649 ms/0.92
    *   3D scene       707 ms/2.82   458 ms/3.13    448 ms/3.13
    *   3D scene 4K   2929 ms/11.26 1843 ms/12.49  1830 ms/12.49
    *   menu frame      48 ms/0.01    56 ms/0.01     70 ms/0.01
    *
    * Level 9's deep chain search collapses on the repetitive content -
    * flat runs, upscaled pixel art, smooth ramps - which is most of
    * what gets captured, and returns 0-8% for 6-9x the time.  Where it
    * does earn its keep, on noisy 3D output, level 6 matches it
    * exactly.  Worst case here is 80 KB against 2.2 seconds.
    *
    * 6 is also zlib's and libpng's own default, so a screenshot is no
    * longer larger or slower than what every other PNG writer emits. */
   if (stream)
      stream_backend->define(stream, "level", 6);

   /* The filtered match strategy is deliberately not requested here.
    * It does close the size gap against libpng on noisy 3D output -
    * 3.13 MB down to 2.82 MB at 1080p, matching libpng exactly - but at
    * level 6 it costs 390 ms to 647 ms for it, because Z_FILTERED
    * discards matches of five bytes or fewer *after* the level-6 match
    * finder has walked a 128-deep hash chain to find them.  The search
    * is thrown away, not skipped.
    *
    * That cost is not inherent.  Level 4, whose chain is 16 deep,
    * reaches the same 2.81 MB in 447 ms - within noise of the 436 ms
    * the default strategy takes at level 6.  But level 4 gives up 18%
    * on smooth content (1.00 MB to 1.18 MB on a gradient), so the two
    * settings have to be chosen together and there is no cell that wins
    * on both axes for every shape:
    *
    *                     3D scene      gradient    pixel art   menu
    *   6 + default       436 ms/3.13   456 ms/1.00  52 ms/0.23  38 ms/0.01
    *   6 + filtered      721 ms/2.82   466 ms/1.00  51 ms/0.23  48 ms/0.01
    *   5 + filtered      659 ms/2.82   231 ms/1.09  46 ms/0.23  38 ms/0.01
    *   4 + filtered      447 ms/2.81   150 ms/1.18  45 ms/0.23  37 ms/0.01
    *   libpng default    812 ms/2.82   511 ms/1.00  87 ms/0.23  72 ms/0.01
    *
    * The only shape arguing for the higher level is the gradient, and
    * that test image is a pure mathematical ramp - the least like a real
    * screenshot of the set, since real fades carry dithering that puts
    * them nearer the 3D row.  Deciding a size-for-time trade on it is
    * not sound.  Default strategy is the state that gives nothing away:
    * fastest everywhere, and still ahead of libpng on time for every
    * shape, at the cost of 11% on noisy 3D output.
    *
    * Both backends implement the strategy property, so turning it on -
    * with a level chosen alongside it, against real captures rather than
    * synthetic ones - is a one-line change whenever that measurement
    * exists. */
   if (!stream)
      GOTO_END_ERROR();

   /* Point deflate's output at our chunk staging area (after the
    * 8-byte chunk header).  We re-point it every time we flush
    * a chunk so the driver doesn't need to know the chunk layout. */
   stream_backend->set_out(stream,
         chunk_buf + 8, (uint32_t)IDAT_CHUNK_SIZE);

   for (h = 0; h < height; h++, data += pitch)
   {
      uint32_t rd, wn;
      uint8_t filter;
      unsigned none_score, up_score, sub_score, avg_score, paeth_score;
      unsigned min_sad;
      uint8_t *chosen_filtered;

      switch (fmt)
      {
         case RPNG_PIXFMT_ARGB32:
            copy_argb_line(rgba_line, (const uint32_t*)data, width);
            break;
         case RPNG_PIXFMT_RGBA32:
            copy_rgba_line(rgba_line, data, width);
            break;
         case RPNG_PIXFMT_RGB48:
            copy_rgb48_line(rgba_line, (const uint16_t*)data, width);
            break;
         case RPNG_PIXFMT_BGR24:
         default:
            copy_bgr24_line(rgba_line, data, width);
            break;
      }

      /* Filter selection unchanged from the previous implementation:
       * try every filter, pick the one with lowest sum-of-abs-deviation. */
      none_score  = count_sad(rgba_line, line_len);
      up_score    = filter_up   (up_filtered,    rgba_line, prev_encoded, width, bpp);
      sub_score   = filter_sub  (sub_filtered,   rgba_line,               width, bpp);
      avg_score   = filter_avg  (avg_filtered,   rgba_line, prev_encoded, width, bpp);
      paeth_score = filter_paeth(paeth_filtered, rgba_line, prev_encoded, width, bpp);

      filter          = 0;
      min_sad         = none_score;
      chosen_filtered = rgba_line;
      if (sub_score < min_sad)   { filter = 1; chosen_filtered = sub_filtered;   min_sad = sub_score;   }
      if (up_score < min_sad)    { filter = 2; chosen_filtered = up_filtered;    min_sad = up_score;    }
      if (avg_score < min_sad)   { filter = 3; chosen_filtered = avg_filtered;   min_sad = avg_score;   }
      if (paeth_score < min_sad) { filter = 4; chosen_filtered = paeth_filtered;                        }

      /* Tag goes in the spare byte ahead of the winning buffer, so the
       * row is fed to deflate in place. */
      chosen_filtered[-1] = filter;

      /* Feed this row into deflate. The loop handles the case where
       * our chunk buffer fills mid-row (BUFFER_FULL): flush IDAT,
       * point deflate at a fresh output buffer, and keep going.
       *
       * When trans() returns success with err=AGAIN, zlib has
       * consumed what we gave it but hasn't finalized (no Z_FINISH
       * was requested) -- that's the normal "ok, send more data
       * next time" signal.  We break out and feed the next row. */
      stream_backend->set_in(stream, chosen_filtered - 1,
            (uint32_t)(line_len + 1));
      for (;;)
      {
         bool ok = stream_backend->trans(stream, false, &rd, &wn, &err);
         chunk_fill += wn;

         if (ok)
         {
            /* All input consumed.  If the output buffer also happens
             * to be exactly full (avail_in=0 AND avail_out=0 on the
             * same call, which the trans API reports as success
             * with AGAIN rather than BUFFER_FULL), flush proactively
             * -- otherwise the next row's trans() would find
             * avail_out=0 and error out. */
            if (chunk_fill >= IDAT_CHUNK_SIZE)
            {
               if (!flush_idat_chunk(intf_s, chunk_buf, chunk_fill))
                  GOTO_END_ERROR();
               chunk_fill = 0;
               stream_backend->set_out(stream,
                     chunk_buf + 8, (uint32_t)IDAT_CHUNK_SIZE);
            }
            break;
         }

         if (err != TRANS_STREAM_ERROR_BUFFER_FULL)
            GOTO_END_ERROR();

         /* Output filled mid-row.  chunk_fill should equal
          * IDAT_CHUNK_SIZE.  Flush and re-point. */
         if (!flush_idat_chunk(intf_s, chunk_buf, chunk_fill))
            GOTO_END_ERROR();
         chunk_fill = 0;
         stream_backend->set_out(stream,
               chunk_buf + 8, (uint32_t)IDAT_CHUNK_SIZE);
      }

      /* This row becomes the next row's predictor.  Swapping the two
       * buffers replaces a second full-row copy; both were allocated
       * the same way, so either can serve as either. */
      {
         uint8_t *tmp = prev_encoded;
         prev_encoded = rgba_line;
         rgba_line    = tmp;
      }
   }

   /* All rows consumed.  Drain deflate with Z_FINISH, emitting IDATs
    * on BUFFER_FULL, final partial on NONE (Z_STREAM_END). */
   stream_backend->set_in(stream, NULL, 0);
   for (;;)
   {
      uint32_t rd = 0, wn = 0;
      bool ok = stream_backend->trans(stream, true, &rd, &wn, &err);
      chunk_fill += wn;

      if (!ok)
      {
         /* BUFFER_FULL during flush-drain with avail_in=0 shouldn't
          * strictly be reachable, but handle defensively. */
         if (err != TRANS_STREAM_ERROR_BUFFER_FULL)
            GOTO_END_ERROR();
         if (!flush_idat_chunk(intf_s, chunk_buf, chunk_fill))
            GOTO_END_ERROR();
         chunk_fill = 0;
         stream_backend->set_out(stream,
               chunk_buf + 8, (uint32_t)IDAT_CHUNK_SIZE);
         continue;
      }
      if (err == TRANS_STREAM_ERROR_AGAIN)
      {
         /* Z_OK during Z_FINISH with avail_in=0 means deflate has
          * more output to emit but our buffer ran out of space.
          * Flush the full chunk and give it more room. */
         if (!flush_idat_chunk(intf_s, chunk_buf, chunk_fill))
            GOTO_END_ERROR();
         chunk_fill = 0;
         stream_backend->set_out(stream,
               chunk_buf + 8, (uint32_t)IDAT_CHUNK_SIZE);
         continue;
      }
      /* err == NONE: Z_STREAM_END.  Flush whatever's in the buffer
       * and we're done.  flush_idat_chunk tolerates chunk_fill==0. */
      if (!flush_idat_chunk(intf_s, chunk_buf, chunk_fill))
         GOTO_END_ERROR();
      break;
   }

   if (!png_write_iend_string(intf_s))
      GOTO_END_ERROR();

end:
   free(rgba_base);
   free(prev_base);
   free(up_base);
   free(sub_base);
   free(avg_base);
   free(paeth_base);
   free(chunk_buf);

   if (stream_backend)
   {
      if (stream)
      {
         if (stream_backend->stream_free)
            stream_backend->stream_free(stream);
      }
   }
   return ret;
}

/* Bytes-per-pixel entry point kept for callers outside this file, which
 * predate the format enum.  bpp is unambiguous for every format it could
 * already express; RGBA32 is reachable only through the fmt worker or
 * rpng_save_image_rgba(). */
bool rpng_save_image_stream(const uint8_t *data, intfstream_t* intf_s,
      unsigned width, unsigned height, signed pitch, unsigned bpp,
      const struct rpng_hdr_metadata *hdr)
{
   enum rpng_pixfmt fmt;

   switch (bpp)
   {
      case 4:
         fmt = RPNG_PIXFMT_ARGB32;
         break;
      case 6:
         fmt = RPNG_PIXFMT_RGB48;
         break;
      case 3:
         fmt = RPNG_PIXFMT_BGR24;
         break;
      default:
         return false;
   }

   return rpng_save_image_stream_fmt(data, intf_s, width, height,
         pitch, fmt, hdr);
}

/* Straight RGBA byte order - R,G,B,A ascending in memory, which on a
 * little-endian host is the uint32_t 0xAABBGGRR, the mirror of what
 * rpng_save_image_argb() takes.  Callers holding GL_RGBA / VK_FORMAT_
 * R8G8B8A8 surfaces would otherwise have to swizzle a whole frame into a
 * scratch buffer just to have the encoder swizzle it back. */
bool rpng_save_image_rgba(const char *path, const uint8_t *data,
      unsigned width, unsigned height, unsigned pitch)
{
   bool ret                      = false;
   intfstream_t* intf_s          = NULL;

   intf_s = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   ret = rpng_save_image_stream_fmt(data, intf_s,
                                width, height,
                                (signed) pitch, RPNG_PIXFMT_RGBA32, NULL);
   intfstream_close(intf_s);
   free(intf_s);
   return ret;
}

bool rpng_save_image_argb(const char *path, const uint32_t *data,
      unsigned width, unsigned height, unsigned pitch)
{
   bool ret                      = false;
   intfstream_t* intf_s          = NULL;

   intf_s = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);

   ret = rpng_save_image_stream_fmt((const uint8_t*) data, intf_s,
                                width, height,
                                (signed) pitch, RPNG_PIXFMT_ARGB32, NULL);
   intfstream_close(intf_s);
   free(intf_s);
   return ret;
}

bool rpng_save_image_bgr24(const char *path, const uint8_t *data,
      unsigned width, unsigned height, unsigned pitch)
{
   bool ret                      = false;
   intfstream_t* intf_s          = NULL;

   intf_s = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   ret = rpng_save_image_stream_fmt(data, intf_s, width, height,
                                (signed) pitch, RPNG_PIXFMT_BGR24, NULL);
   intfstream_close(intf_s);
   free(intf_s);
   return ret;
}

bool rpng_save_image_rgb48_hdr(const char *path, const uint16_t *data,
      unsigned width, unsigned height, unsigned pitch,
      const struct rpng_hdr_metadata *hdr)
{
   bool ret                      = false;
   intfstream_t* intf_s          = NULL;

   intf_s = intfstream_open_file(path,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
   ret = rpng_save_image_stream_fmt((const uint8_t*)data, intf_s, width, height,
                                (signed) pitch, RPNG_PIXFMT_RGB48, hdr);
   intfstream_close(intf_s);
   free(intf_s);
   return ret;
}


uint8_t* rpng_save_image_bgr24_hdr_string(const uint8_t *data,
      unsigned width, unsigned height, signed pitch,
      const struct rpng_hdr_metadata *hdr, uint64_t* bytes)
{
   bool ret             = false;
   intfstream_t *intf_s = NULL;
   /* cICP(16) + cLLI(20) + mDCV(36) = 72 bytes of extra chunks at most. */
   size_t hdr_extra     = hdr ? 72 : 0;
   size_t _len          = (size_t)(width * height * 3 * DEFLATE_PADDING) + PNG_ROUGH_HEADER + hdr_extra;
   uint8_t *buf         = (uint8_t*)malloc(_len * sizeof(uint8_t));
   if (!buf)
      GOTO_END_ERROR();

   intf_s = intfstream_open_memory(buf,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE,
         _len);

   ret    = rpng_save_image_stream_fmt((const uint8_t*)data,
            intf_s, width, height, pitch, RPNG_PIXFMT_BGR24, hdr);
   *bytes = intfstream_get_ptr(intf_s);

   /* Trim the buffer to the actual written size instead of
    * allocating a second buffer and copying. */
   if (ret && *bytes > 0)
   {
      uint8_t *trimmed = (uint8_t*)realloc(buf, (size_t)*bytes);
      if (trimmed)
         buf = trimmed;
      /* If realloc fails, the original (oversized) buf is still valid */
   }

end:
   if (intf_s)
   {
      intfstream_close(intf_s);
      free(intf_s);
   }
   if (!ret)
   {
      if (buf)
         free(buf);
      return NULL;
   }
   return buf;
}

uint8_t* rpng_save_image_bgr24_string(const uint8_t *data,
      unsigned width, unsigned height, signed pitch, uint64_t* bytes)
{
   return rpng_save_image_bgr24_hdr_string(data, width, height, pitch,
         NULL, bytes);
}

uint8_t* rpng_save_image_rgb48_hdr_string(const uint16_t *data,
      unsigned width, unsigned height, signed pitch,
      const struct rpng_hdr_metadata *hdr, uint64_t* bytes)
{
   bool ret             = false;
   intfstream_t *intf_s = NULL;
   /* cICP(16) + cLLI(20) + mDCV(36) = 72 bytes of extra chunks at most. */
   size_t hdr_extra     = hdr ? 72 : 0;
   /* 6 bytes/pixel for 16-bit RGB. */
   size_t _len          = (size_t)(width * height * 6 * DEFLATE_PADDING) + PNG_ROUGH_HEADER + hdr_extra;
   uint8_t *buf         = (uint8_t*)malloc(_len * sizeof(uint8_t));
   if (!buf)
      GOTO_END_ERROR();

   intf_s = intfstream_open_memory(buf,
         RETRO_VFS_FILE_ACCESS_WRITE,
         RETRO_VFS_FILE_ACCESS_HINT_NONE,
         _len);

   ret    = rpng_save_image_stream_fmt((const uint8_t*)data,
            intf_s, width, height, pitch, RPNG_PIXFMT_RGB48, hdr);
   *bytes = intfstream_get_ptr(intf_s);

   if (ret && *bytes > 0)
   {
      uint8_t *trimmed = (uint8_t*)realloc(buf, (size_t)*bytes);
      if (trimmed)
         buf = trimmed;
   }

end:
   if (intf_s)
   {
      intfstream_close(intf_s);
      free(intf_s);
   }
   if (!ret)
   {
      if (buf)
         free(buf);
      return NULL;
   }
   return buf;
}

