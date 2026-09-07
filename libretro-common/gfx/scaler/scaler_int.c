/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (scaler_int.c).
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

#include <gfx/scaler/scaler_int.h>

#include <string.h>

#include <retro_inline.h>

#ifdef SCALER_NO_SIMD
#undef __SSE2__
#endif

#if defined(__SSE2__)
#include <emmintrin.h>
#ifdef _WIN32
#include <intrin.h>
#endif

/* Widen a pair of adjacent taps to [c0 c0 c0 c0 | c1 c1 c1 c1].
 *
 * The generic passes built this with two _mm_set1_epi16 and a join,
 * which is a movd and a pair of shuffles each.  The taps are adjacent
 * int16_t, so one 32-bit load carries both, and two unpacks spread
 * them - three instructions against roughly seven, for the identical
 * register.
 *
 * memcpy rather than a cast: reading an int16_t array through an
 * int32_t lvalue breaks aliasing.  Every compiler we build with folds
 * this to the load it is. */
static INLINE __m128i scaler_coeff_pair(const int16_t *filter)
{
   int32_t pair;
   __m128i c;

   memcpy(&pair, filter, sizeof(pair));

   c = _mm_cvtsi32_si128(pair);
   c = _mm_unpacklo_epi16(c, c);
   return _mm_unpacklo_epi32(c, c);
}

/* Vertical taps are the same for every pixel in an output row, so they
 * are built once per row into this and reused across the row rather
 * than rebuilt per pixel.  32 pairs covers a 64-tap filter, which is
 * scaler_gen_filter()'s sinc_size for downscale ratios up to 8:1;
 * beyond that the per-pixel path still applies. */
#define SCALER_MAX_HOISTED_PAIRS 32
#endif

/* ARGB8888 scaler is split in two:
 *
 * First, horizontal scaler is applied.
 * Here, all 8-bit channels are expanded to 16-bit. Values are then shifted 7
 * to left to occupy 15 bits.
 *
 * The sign bit is kept empty as we have to do signed multiplication for the
 * filter.
 *
 * A mulhi [(a * b) >> 16] is applied which loses some precision, but is
 * very efficient for SIMD.
 * It is accurate enough for 8-bit purposes.
 *
 * The fixed point 1.0 for filter is (1 << 14). After horizontal scale,
 * the output is kept with 16-bit channels, and will now have 13 bits
 * of precision as [(a * (1 << 14)) >> 16] is effectively a right shift by 2.
 *
 * Vertical scaler takes the 13 bit channels, and performs the
 * same mulhi steps.
 * Another 2 bits of precision is lost, which ends up as 11 bits.
 * Scaling is now complete. Channels are shifted right by 3, and saturated
 * into 8-bit values.
 *
 * The C version of scalers perform the exact same operations as the
 * SIMD code for testing purposes.
 */

/* Specialised vertical pass for a 2-tap filter, i.e. bilinear.
 *
 * The generic loop below is written for an arbitrary tap count, and
 * pays for that generality once per *output pixel*: it rebuilds the
 * coefficient vectors from filter_vert[] inside the innermost loop,
 * even though those coefficients depend only on the output row.  It
 * also spends both halves of the register on one pixel's two source
 * rows, so a 128-bit machine retires one 32-bit pixel per pass.
 *
 * With the tap count known there is no reason for either.  The two
 * coefficient broadcasts hoist out to the row, and both lanes can
 * carry a pixel instead, which halves the iteration count.  Output is
 * bit-exact against the generic path - the arithmetic is the same
 * saturating add of the same two mulhi terms, only reassociated
 * across lanes rather than across the accumulator - so this is purely
 * a matter of not doing avoidable work.
 *
 * This is the path the menu thumbnail upscaler takes, where it is
 * also the dominant cost: filtering a 360x256 state screenshot to
 * 1440x1024 spends 3.9 ms of 4.8 ms here.
 *
 * SSE2 only, deliberately.  The same specialisation written in C
 * measured ~20% *slower* than the generic loop (7.4 ms -> 9.2 ms on
 * 256x256 -> 1024x1024), because there the win does not exist to
 * begin with: a two-iteration loop is something the compiler already
 * unrolls, and hand-unrolling it only adds truncations.  Scalar
 * builds keep the generic path. */
#if defined(__SSE2__)
static void scaler_argb8888_vert_2tap(const struct scaler_ctx *ctx,
      void *output_, int stride)
{
   int h, w;
   const uint64_t      *input = ctx->scaled.frame;
   uint32_t           *output = (uint32_t*)output_;
   const int16_t *filter_vert = ctx->vert.filter;
   const int       row_stride = ctx->scaled.stride >> 3;

   for (h = 0; h < ctx->out_height; h++,
         filter_vert += ctx->vert.filter_stride, output += stride >> 2)
   {
      const uint64_t *r0 = input + ctx->vert.filter_pos[h] * row_stride;
      const uint64_t *r1 = r0 + row_stride;
      const __m128i   c0 = _mm_set1_epi16(filter_vert[0]);
      const __m128i   c1 = _mm_set1_epi16(filter_vert[1]);

      for (w = 0; (w + 1) < ctx->out_width; w += 2)
      {
         __m128i a   = _mm_loadu_si128((const __m128i*)(r0 + w));
         __m128i b   = _mm_loadu_si128((const __m128i*)(r1 + w));
         __m128i res = _mm_adds_epi16(_mm_mulhi_epi16(a, c0),
                                      _mm_mulhi_epi16(b, c1));

         res         = _mm_srai_epi16(res, (7 - 2 - 2));

         _mm_storel_epi64((__m128i*)(output + w),
               _mm_packus_epi16(res, res));
      }

      for (; w < ctx->out_width; w++)
      {
         __m128i a   = _mm_loadl_epi64((const __m128i*)(r0 + w));
         __m128i b   = _mm_loadl_epi64((const __m128i*)(r1 + w));
         __m128i res = _mm_adds_epi16(_mm_mulhi_epi16(a, c0),
                                      _mm_mulhi_epi16(b, c1));

         res         = _mm_srai_epi16(res, (7 - 2 - 2));

         output[w]   = _mm_cvtsi128_si32(_mm_packus_epi16(res, res));
      }
   }
}
#endif

void scaler_argb8888_vert(const struct scaler_ctx *ctx, void *output_, int stride)
{
   int h, w, y;
   const uint64_t      *input = ctx->scaled.frame;
   uint32_t           *output = (uint32_t*)output_;

   const int16_t *filter_vert = ctx->vert.filter;

#if defined(__SSE2__)
   /* fixup_filter_sub() guarantees filter_pos[h] + filter_len stays
    * inside the source, so a 2-tap filter always has its second row
    * to read. */
   if (ctx->vert.filter_len == 2)
   {
      scaler_argb8888_vert_2tap(ctx, output_, stride);
      return;
   }
#endif

   for (h = 0; h < ctx->out_height; h++,
         filter_vert += ctx->vert.filter_stride, output += stride >> 2)
   {
      const uint64_t *input_base = input + ctx->vert.filter_pos[h]
         * (ctx->scaled.stride >> 3);
#if defined(__SSE2__)
      __m128i coeffs[SCALER_MAX_HOISTED_PAIRS];
      int pairs = ctx->vert.filter_len >> 1;
      int p;

      if (pairs > SCALER_MAX_HOISTED_PAIRS)
         pairs = 0;

      for (p = 0; p < pairs; p++)
         coeffs[p] = scaler_coeff_pair(filter_vert + p * 2);
#endif

      for (w = 0; w < ctx->out_width; w++)
      {
         const uint64_t *input_base_y = input_base + w;
#if defined(__SSE2__)
         __m128i final;
         __m128i res = _mm_setzero_si128();

         for (p = 0, y = 0; (y + 1) < ctx->vert.filter_len; y += 2, p++,
               input_base_y += (ctx->scaled.stride >> 2))
         {
            __m128i coeff = (p < pairs)
                  ? coeffs[p]
                  : scaler_coeff_pair(filter_vert + y);
            __m128i col   = _mm_set_epi64x(input_base_y[ctx->scaled.stride >> 3], input_base_y[0]);

            res           = _mm_adds_epi16(_mm_mulhi_epi16(col, coeff), res);
         }

         for (; y < ctx->vert.filter_len; y++, input_base_y += (ctx->scaled.stride >> 3))
         {
            __m128i coeff = _mm_set1_epi16(filter_vert[y]);
            __m128i col   = _mm_set_epi64x(0, input_base_y[0]);

            res           = _mm_adds_epi16(_mm_mulhi_epi16(col, coeff), res);
         }

         res       = _mm_adds_epi16(_mm_srli_si128(res, 8), res);
         res       = _mm_srai_epi16(res, (7 - 2 - 2));

         final     = _mm_packus_epi16(res, res);

         output[w] = _mm_cvtsi128_si32(final);
#else
         int16_t res_a = 0;
         int16_t res_r = 0;
         int16_t res_g = 0;
         int16_t res_b = 0;

         for (y = 0; y < ctx->vert.filter_len; y++,
               input_base_y += (ctx->scaled.stride >> 3))
         {
            uint64_t col   = *input_base_y;

            int16_t a      = (col >> 48) & 0xffff;
            int16_t r      = (col >> 32) & 0xffff;
            int16_t g      = (col >> 16) & 0xffff;
            int16_t b      = (col >>  0) & 0xffff;

            int16_t coeff  = filter_vert[y];

            res_a         += (a * coeff) >> 16;
            res_r         += (r * coeff) >> 16;
            res_g         += (g * coeff) >> 16;
            res_b         += (b * coeff) >> 16;
         }

         res_a           >>= (7 - 2 - 2);
         res_r           >>= (7 - 2 - 2);
         res_g           >>= (7 - 2 - 2);
         res_b           >>= (7 - 2 - 2);

         output[w]         =
            ((uint32_t)clamp_8bit(res_a) << 24) |
            ((uint32_t)clamp_8bit(res_r) << 16) |
            ((uint32_t)clamp_8bit(res_g) << 8)  |
            ((uint32_t)clamp_8bit(res_b) << 0);
#endif
      }
   }
}

/* Specialised horizontal pass for a 2-tap filter, i.e. bilinear.
 *
 * The same generality tax as the vertical pass, in three places.  The
 * coefficient pair is rebuilt with two _mm_set1_epi16 and a join for
 * every output pixel of every row, though it depends only on the
 * column.  The two source pixels are assembled into a register with a
 * pair of 32-bit loads, a shift and an or, though they are adjacent
 * in memory and could simply be loaded together.  And the result is
 * accumulated into a zeroed register, though with one tap pair there
 * is nothing to accumulate against.
 *
 * So: build the coefficients from a single 32-bit load of the tap
 * pair, widened by two unpacks; load the source pair with one movq;
 * and use the product directly.  Bit-exact - same taps, same mulhi,
 * same lane fold, minus an add of zero.
 *
 * The coefficients could be precomputed for the whole image rather
 * than per row, which measured 2.56x against this version's 2.49x on
 * 360x256 -> 1440x1024.  Not worth an allocation and a failure path
 * in a hot function for 3%.
 *
 * SSE2 only, matching scaler_argb8888_vert_2tap(): the equivalent
 * scalar specialisation measured slower than the generic loop there,
 * and there is no reason to expect otherwise here. */
#if defined(__SSE2__)
static void scaler_argb8888_horiz_2tap(const struct scaler_ctx *ctx,
      const void *input_, int stride)
{
   int h, w;
   const uint32_t *input = (const uint32_t*)input_;
   uint64_t *output      = ctx->scaled.frame;
   const __m128i zero    = _mm_setzero_si128();

   for (h = 0; h < ctx->scaled.height; h++, input += stride >> 2,
         output += ctx->scaled.stride >> 3)
   {
      const int16_t *filter_horiz = ctx->horiz.filter;

      for (w = 0; w < ctx->scaled.width; w++, filter_horiz += 2)
      {
         const uint32_t *input_base_x = input + ctx->horiz.filter_pos[w];
         __m128i coeff;
         __m128i col;
         __m128i res;
         int32_t pair;

         /* memcpy rather than a cast: the taps are int16_t and
          * reading them through an int32_t lvalue would break
          * aliasing.  Every compiler we build with folds this to the
          * load it is. */
         memcpy(&pair, filter_horiz, sizeof(pair));

         coeff     = _mm_cvtsi32_si128(pair);
         coeff     = _mm_unpacklo_epi16(coeff, coeff);
         coeff     = _mm_unpacklo_epi32(coeff, coeff);

         /* fixup_filter_sub() keeps filter_pos[w] + filter_len inside
          * the source, so both pixels of the pair are in bounds. */
         col       = _mm_unpacklo_epi8(
               _mm_loadl_epi64((const __m128i*)input_base_x), zero);
         col       = _mm_slli_epi16(col, 7);

         res       = _mm_mulhi_epi16(col, coeff);
         res       = _mm_adds_epi16(_mm_srli_si128(res, 8), res);

         /* movq, so no 64-bit GPR and no alignment requirement -
          * unlike _mm_cvtsi128_si64 this needs no 32-bit fallback. */
         _mm_storel_epi64((__m128i*)(output + w), res);
      }
   }
}
#endif

void scaler_argb8888_horiz(const struct scaler_ctx *ctx, const void *input_, int stride)
{
   int h, w, x;
   const uint32_t *input = (uint32_t*)input_;
   uint64_t *output      = ctx->scaled.frame;

#if defined(__SSE2__)
   if (ctx->horiz.filter_len == 2)
   {
      scaler_argb8888_horiz_2tap(ctx, input_, stride);
      return;
   }
#endif

   for (h = 0; h < ctx->scaled.height; h++, input += stride >> 2,
         output += ctx->scaled.stride >> 3)
   {
      const int16_t *filter_horiz = ctx->horiz.filter;

      for (w = 0; w < ctx->scaled.width; w++,
            filter_horiz += ctx->horiz.filter_stride)
      {
         const uint32_t *input_base_x = input + ctx->horiz.filter_pos[w];
#if defined(__SSE2__)
         __m128i res = _mm_setzero_si128();
#ifndef __x86_64__
         union
         {
            uint32_t *u32;
            uint64_t *u64;
         } u;
#endif
         for (x = 0; (x + 1) < ctx->horiz.filter_len; x += 2)
         {
            __m128i coeff = scaler_coeff_pair(filter_horiz + x);

            /* The two source pixels of a tap pair are adjacent, and
             * fixup_filter_sub() keeps filter_pos[w] + filter_len
             * inside the row, so one movq fetches both. */
            __m128i col   = _mm_unpacklo_epi8(_mm_loadl_epi64(
                     (const __m128i*)(input_base_x + x)), _mm_setzero_si128());

            col           = _mm_slli_epi16(col, 7);
            res           = _mm_adds_epi16(_mm_mulhi_epi16(col, coeff), res);
         }

         for (; x < ctx->horiz.filter_len; x++)
         {
            __m128i coeff = _mm_set1_epi16(filter_horiz[x]);
            __m128i col   = _mm_unpacklo_epi8(_mm_set_epi32(0, 0, 0, input_base_x[x]), _mm_setzero_si128());

            col           = _mm_slli_epi16(col, 7);
            res           = _mm_adds_epi16(_mm_mulhi_epi16(col, coeff), res);
         }

         res              = _mm_adds_epi16(_mm_srli_si128(res, 8), res);

#ifdef __x86_64__
         output[w]        = _mm_cvtsi128_si64(res);
#else /* 32-bit doesn't have si64. Do it in two steps. */
         u.u64    = output + w;
         u.u32[0] = _mm_cvtsi128_si32(res);
         u.u32[1] = _mm_cvtsi128_si32(_mm_srli_si128(res, 4));
#endif
#else
         int16_t res_a = 0;
         int16_t res_r = 0;
         int16_t res_g = 0;
         int16_t res_b = 0;

         for (x = 0; x < ctx->horiz.filter_len; x++)
         {
            uint32_t col   = input_base_x[x];

            int16_t a      = (col >> (24 - 7)) & (0xff << 7);
            int16_t r      = (col >> (16 - 7)) & (0xff << 7);
            int16_t g      = (col >> ( 8 - 7)) & (0xff << 7);
            int16_t b      = (col << ( 0 + 7)) & (0xff << 7);

            int16_t coeff  = filter_horiz[x];

            res_a         += (a * coeff) >> 16;
            res_r         += (r * coeff) >> 16;
            res_g         += (g * coeff) >> 16;
            res_b         += (b * coeff) >> 16;
         }

         output[w]         = (
               (uint64_t)res_a  << 48)  |
               ((uint64_t)res_r << 32)  |
               ((uint64_t)res_g << 16)  |
               ((uint64_t)res_b << 0);
#endif
      }
   }
}

void scaler_argb8888_point_special(const struct scaler_ctx *ctx,
      void *output_, const void *input_,
      int out_width, int out_height,
      int in_width, int in_height,
      int out_stride, int in_stride)
{
   int h, w;
   int x_pos             = (1 << 15) * in_width / out_width - (1 << 15);
   int x_step            = (1 << 16) * in_width / out_width;
   int y_pos             = (1 << 15) * in_height / out_height - (1 << 15);
   int y_step            = (1 << 16) * in_height / out_height;
   const uint32_t *input = (const uint32_t*)input_;
   uint32_t *output      = (uint32_t*)output_;

   if (x_pos < 0)
      x_pos = 0;
   if (y_pos < 0)
      y_pos = 0;

   for (h = 0; h < out_height; h++, y_pos += y_step, output += out_stride >> 2)
   {
      int               x = x_pos;
      const uint32_t *inp = input + (y_pos >> 16) * (in_stride >> 2);

      for (w = 0; w < out_width; w++, x += x_step)
         output[w] = inp[x >> 16];
   }
}

/* XRGB2101010 scalers.
 *
 * Same fixed-point chain as the 8-bit pair above, retuned for 10-bit
 * channels.  A channel is expanded to occupy 15 bits with the sign bit
 * left empty for the signed multiply, so the shift is 5 rather than 7
 * (1023 << 5 == 32736, which still fits int16).  The mulhi chain costs
 * 2 bits per pass exactly as before - 15 -> 13 after horiz, 13 -> 11
 * after vert - so the final shift is (5 - 2 - 2) == 1, landing 10 bits.
 *
 * The packed layout is the one the 10-bit upload paths and rpng agree
 * on: A in [31:30], R in [29:20], G in [19:10], B in [9:0].  Alpha is
 * only two bits, so it is not filtered - it is carried through as fully
 * opaque, matching conv/blit behaviour for this format.
 *
 * The intermediate (ctx->scaled.frame) holds 16 bits per channel and is
 * shared with the 8-bit path unchanged; only the pack/unpack ends
 * differ. */

static INLINE uint16_t clamp_10bit(int val)
{
   if (val > 1023)
      return 1023;
   if (val < 0)
      return 0;
   return (uint16_t)val;
}

/* Vectorised across taps, not across output pixels.
 *
 * Going across pixels needs a per-pixel _mm_set_epi16 of three
 * shifted-and-masked fields, and that lane assembly costs more than the
 * multiply it feeds - measured, it loses.  Going across taps needs no
 * lane assembly at all: eight consecutive taps are eight consecutive
 * input pixels, so the three 10-bit fields fall out of shifts and masks
 * on packed dwords, and (r * coeff) >> 16 is exactly _mm_mulhi_epi16.
 *
 * The per-pixel reduction has to stay in registers.  Spilling the
 * accumulators to a scratch array and summing them scalar-side gives
 * back more than the vector loop wins, because sinc runs only 8 or 16
 * taps and the reduction is then most of the work. */
void scaler_xrgb2101010_horiz(const struct scaler_ctx *ctx,
      const void *input_, int stride)
{
   int h, w, x;
   const uint32_t *input = (const uint32_t*)input_;
   uint64_t *output      = ctx->scaled.frame;

#if defined(__SSE2__)
   /* Sinc runs 8 or 16 taps; bilinear runs 2 and point runs 1.  The
    * choice is made out here rather than inside the pixel loop so the
    * short-filter path keeps exactly the code it had - a per-pixel
    * branch on filter_len costs bilinear about 7%. */
   if (ctx->horiz.filter_len >= 8)
   {
      const int filter_len = ctx->horiz.filter_len;
      const __m128i mask10 = _mm_set1_epi32(0x3ff);
      const __m128i ones   = _mm_set1_epi16(1);

      for (h = 0; h < ctx->scaled.height; h++, input += stride >> 2,
            output += ctx->scaled.stride >> 3)
      {
         const int16_t *filter_horiz = ctx->horiz.filter;

         for (w = 0; w < ctx->scaled.width; w++,
               filter_horiz += ctx->horiz.filter_stride)
         {
            const uint32_t *input_base_x = input + ctx->horiz.filter_pos[w];
            __m128i acc_r = _mm_setzero_si128();
            __m128i acc_g = _mm_setzero_si128();
            __m128i acc_b = _mm_setzero_si128();
            __m128i sum;
            int16_t res_r, res_g, res_b;

            for (x = 0; x + 8 <= filter_len; x += 8)
            {
               __m128i p0 = _mm_loadu_si128(
                     (const __m128i*)(const void*)(input_base_x + x));
               __m128i p1 = _mm_loadu_si128(
                     (const __m128i*)(const void*)(input_base_x + x + 4));
               __m128i c  = _mm_loadu_si128(
                     (const __m128i*)(const void*)(filter_horiz + x));
               /* every field is below 1024 << 5, so packs_epi32 never
                * saturates and the int16 view is exact */
               __m128i r  = _mm_packs_epi32(
                     _mm_slli_epi32(_mm_and_si128(
                           _mm_srli_epi32(p0, 20), mask10), 5),
                     _mm_slli_epi32(_mm_and_si128(
                           _mm_srli_epi32(p1, 20), mask10), 5));
               __m128i g  = _mm_packs_epi32(
                     _mm_slli_epi32(_mm_and_si128(
                           _mm_srli_epi32(p0, 10), mask10), 5),
                     _mm_slli_epi32(_mm_and_si128(
                           _mm_srli_epi32(p1, 10), mask10), 5));
               __m128i b  = _mm_packs_epi32(
                     _mm_slli_epi32(_mm_and_si128(p0, mask10), 5),
                     _mm_slli_epi32(_mm_and_si128(p1, mask10), 5));

               /* mulhi_epi16 is exactly (a * b) >> 16 for signed 16-bit */
               acc_r = _mm_add_epi16(acc_r, _mm_mulhi_epi16(r, c));
               acc_g = _mm_add_epi16(acc_g, _mm_mulhi_epi16(g, c));
               acc_b = _mm_add_epi16(acc_b, _mm_mulhi_epi16(b, c));
            }

            /* summing the lanes in 32-bit and truncating leaves the same
             * low 16 bits as the scalar accumulator's wrapping adds */
            sum   = _mm_madd_epi16(acc_r, ones);
            sum   = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x4e));
            sum   = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0xb1));
            res_r = (int16_t)_mm_cvtsi128_si32(sum);

            sum   = _mm_madd_epi16(acc_g, ones);
            sum   = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x4e));
            sum   = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0xb1));
            res_g = (int16_t)_mm_cvtsi128_si32(sum);

            sum   = _mm_madd_epi16(acc_b, ones);
            sum   = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0x4e));
            sum   = _mm_add_epi32(sum, _mm_shuffle_epi32(sum, 0xb1));
            res_b = (int16_t)_mm_cvtsi128_si32(sum);

            for (; x < filter_len; x++)
            {
               uint32_t col   = input_base_x[x];
               int16_t r      = (int16_t)(((col >> 20) & 0x3ff) << 5);
               int16_t g      = (int16_t)(((col >> 10) & 0x3ff) << 5);
               int16_t b      = (int16_t)(( col        & 0x3ff) << 5);
               int16_t coeff  = filter_horiz[x];
               res_r         += (r * coeff) >> 16;
               res_g         += (g * coeff) >> 16;
               res_b         += (b * coeff) >> 16;
            }

            output[w]         = (
                  (uint64_t)0     << 48)  |
                  ((uint64_t)(uint16_t)res_r << 32)  |
                  ((uint64_t)(uint16_t)res_g << 16)  |
                  ((uint64_t)(uint16_t)res_b << 0);
         }
      }
      return;
   }
#endif

   for (h = 0; h < ctx->scaled.height; h++, input += stride >> 2,
         output += ctx->scaled.stride >> 3)
   {
      const int16_t *filter_horiz = ctx->horiz.filter;

      for (w = 0; w < ctx->scaled.width; w++,
            filter_horiz += ctx->horiz.filter_stride)
      {
         const uint32_t *input_base_x = input + ctx->horiz.filter_pos[w];
         int16_t res_r = 0;
         int16_t res_g = 0;
         int16_t res_b = 0;

         for (x = 0; x < ctx->horiz.filter_len; x++)
         {
            uint32_t col   = input_base_x[x];

            int16_t r      = (int16_t)(((col >> 20) & 0x3ff) << 5);
            int16_t g      = (int16_t)(((col >> 10) & 0x3ff) << 5);
            int16_t b      = (int16_t)(( col        & 0x3ff) << 5);

            int16_t coeff  = filter_horiz[x];

            res_r         += (r * coeff) >> 16;
            res_g         += (g * coeff) >> 16;
            res_b         += (b * coeff) >> 16;
         }

         output[w]         = (
               (uint64_t)0     << 48)  |
               ((uint64_t)(uint16_t)res_r << 32)  |
               ((uint64_t)(uint16_t)res_g << 16)  |
               ((uint64_t)(uint16_t)res_b << 0);

      }
   }
}

void scaler_xrgb2101010_vert(const struct scaler_ctx *ctx,
      void *output_, int stride)
{
   int h, w, y;
   const uint64_t      *input = ctx->scaled.frame;
   uint32_t           *output = (uint32_t*)output_;

   const int16_t *filter_vert = ctx->vert.filter;

   for (h = 0; h < ctx->out_height; h++,
         filter_vert += ctx->vert.filter_stride, output += stride >> 2)
   {
      const uint64_t *input_base = input + ctx->vert.filter_pos[h]
         * (ctx->scaled.stride >> 3);

      for (w = 0; w < ctx->out_width; w++)
      {
         const uint64_t *input_base_y = input_base + w;
#if defined(__SSE2__)
         int16_t res_r, res_g, res_b;
         __m128i res = _mm_setzero_si128();

         /* Two source rows per iteration, as the 8-bit path does.
          * The result cannot go through _mm_packus_epi16 - that
          * saturates to 8 bits - so the channels are extracted and
          * clamped to 10 bits individually below. */
         for (y = 0; (y + 1) < ctx->vert.filter_len; y += 2,
               input_base_y += (ctx->scaled.stride >> 2))
         {
            __m128i coeff = _mm_unpacklo_epi64(
                  _mm_set1_epi16(filter_vert[y + 0]),
                  _mm_set1_epi16(filter_vert[y + 1]));
            __m128i col   = _mm_set_epi64x(
                  input_base_y[ctx->scaled.stride >> 3], input_base_y[0]);

            res           = _mm_adds_epi16(_mm_mulhi_epi16(col, coeff), res);
         }

         for (; y < ctx->vert.filter_len; y++,
               input_base_y += (ctx->scaled.stride >> 3))
         {
            __m128i coeff = _mm_set1_epi16(filter_vert[y]);
            __m128i col   = _mm_set_epi64x(0, input_base_y[0]);

            res           = _mm_adds_epi16(_mm_mulhi_epi16(col, coeff), res);
         }

         res   = _mm_adds_epi16(_mm_srli_si128(res, 8), res);
         res   = _mm_srai_epi16(res, (5 - 2 - 2));

         res_b = (int16_t)_mm_extract_epi16(res, 0);
         res_g = (int16_t)_mm_extract_epi16(res, 1);
         res_r = (int16_t)_mm_extract_epi16(res, 2);
#else
         int16_t res_r = 0;
         int16_t res_g = 0;
         int16_t res_b = 0;

         for (y = 0; y < ctx->vert.filter_len; y++,
               input_base_y += (ctx->scaled.stride >> 3))
         {
            uint64_t col   = *input_base_y;

            int16_t r      = (int16_t)((col >> 32) & 0xffff);
            int16_t g      = (int16_t)((col >> 16) & 0xffff);
            int16_t b      = (int16_t)((col >>  0) & 0xffff);

            int16_t coeff  = filter_vert[y];

            res_r         += (r * coeff) >> 16;
            res_g         += (g * coeff) >> 16;
            res_b         += (b * coeff) >> 16;
         }

         res_r           >>= (5 - 2 - 2);
         res_g           >>= (5 - 2 - 2);
         res_b           >>= (5 - 2 - 2);
#endif

         output[w]         =
            (0x3u                  << 30) |
            (clamp_10bit(res_r)    << 20) |
            (clamp_10bit(res_g)    << 10) |
            (clamp_10bit(res_b)    <<  0);
      }
   }
}
