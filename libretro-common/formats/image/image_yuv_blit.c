/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image_yuv_blit.c).
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

#include <stddef.h>
#include <string.h>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define IMAGE_YUV_SSE2
#include <emmintrin.h>
#elif (defined(__ARM_NEON) || defined(__ARM_NEON__)) && !defined(__ARM_BIG_ENDIAN)
#define IMAGE_YUV_NEON
#include <arm_neon.h>
#endif

#include <formats/image_yuv_blit.h>

/* The matrices in 18.14 fixed point, the luma term pre-biased so the
 * rounding and the black level are one add. Indexed by
 * (BT709 << 1) | FULL_RANGE. */
typedef struct
{
   int ym;    /* luma scale */
   int yb;    /* luma bias, with the rounding half in it */
   int rv;    /* Cr -> R */
   int gu;    /* Cb -> G (negative) */
   int gv;    /* Cr -> G (negative) */
   int bu;    /* Cb -> B */
} image_yuv_coef_t;

static const image_yuv_coef_t image_yuv_coefs[4] =
{
   /* BT.601 limited: 1.164, 1.596, -0.392, -0.813, 2.017 */
   { 19077, -19077 * 16 + 8192, 26149,  -6419, -13320, 33050 },
   /* BT.601 full:    1.000, 1.402, -0.344, -0.714, 1.772 */
   { 16384,               8192, 22970,  -5638, -11700, 29032 },
   /* BT.709 limited: 1.164, 1.793, -0.213, -0.533, 2.112 */
   { 19077, -19077 * 16 + 8192, 29372,  -3494,  -8731, 34610 },
   /* BT.709 full:    1.000, 1.575, -0.187, -0.468, 1.856 */
   { 16384,               8192, 25802,  -3069,  -7670, 30402 }
};

#define IMAGE_YUV_SHIFT 14

/* The other way, RGB to YCbCr, in the same fixed point, indexed the
 * same way: each luma row sums to the luma range (white is 235, or 255)
 * and each chroma row to 0 (grey is 128 exactly). */
typedef struct
{
   int y[3];  /* R, G, B -> Y */
   int u[3];  /* R, G, B -> Cb */
   int v[3];  /* R, G, B -> Cr */
   int y0;    /* black level */
} image_yuv_fwd_t;

static const image_yuv_fwd_t image_yuv_fwds[4] =
{
   /* BT.601 limited: Y 0.299, 0.587, 0.114 by 219/255, chroma by 224/255 */
   { { 4207,  8260, 1604 }, { -2428, -4768, 7196 }, { 7196, -6026, -1170 }, 16 },
   /* BT.601 full */
   { { 4899,  9617, 1868 }, { -2765, -5427, 8192 }, { 8192, -6860, -1332 },  0 },
   /* BT.709 limited: Y 0.2126, 0.7152, 0.0722 by 219/255 */
   { { 2991, 10064, 1016 }, { -1649, -5547, 7196 }, { 7196, -6536,  -660 }, 16 },
   /* BT.709 full */
   { { 3483, 11718, 1183 }, { -1877, -6315, 8192 }, { 8192, -7441,  -751 },  0 }
};

/* where R, G, B and A lie in a pixel of each IMAGE_YUV_BYTES_ order */
static const uint8_t image_yuv_order[4][4] =
{
   { 0, 1, 2, 3 },   /* RGBA */
   { 1, 2, 3, 0 },   /* ARGB */
   { 2, 1, 0, 3 },   /* BGRA */
   { 3, 2, 1, 0 }    /* ABGR */
};

#define IMAGE_YUV_CLAMP(v) ((v) < 0 ? 0 : ((v) > 255 ? 255 : (v)))

/* Two luma pixels sharing one chroma sample, into @d[0] and @d[1]
 * (or @d[0] alone when @n is 1). */
static void image_yuv_pair(uint32_t *d, unsigned n,
      const uint8_t *yp, int cb, int cr,
      const image_yuv_coef_t *c, unsigned rgba)
{
   int rr = c->rv * cr;
   int gg = c->gu * cb + c->gv * cr;
   int bb = c->bu * cb;
   unsigned i;
   for (i = 0; i < n; i++)
   {
      int yy = c->ym * yp[i] + c->yb;
      int r  = IMAGE_YUV_CLAMP((yy + rr) >> IMAGE_YUV_SHIFT);
      int g  = IMAGE_YUV_CLAMP((yy + gg) >> IMAGE_YUV_SHIFT);
      int b  = IMAGE_YUV_CLAMP((yy + bb) >> IMAGE_YUV_SHIFT);
      d[i]   = rgba
         ? (0xff000000u | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r)
         : (0xff000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b);
   }
}

/* Pixels @x on of one row, a pair at a time, with the chroma of pair
 * @x fetched by step as the layout dictates. */
static void image_yuv_row(uint32_t *d, const uint8_t *yp,
      const uint8_t *cb, unsigned cb_step,
      const uint8_t *cr, unsigned cr_step,
      unsigned x, unsigned w, const image_yuv_coef_t *c, unsigned rgba)
{
   for (; x < w; x += 2)
   {
      unsigned n = (x + 1 < w) ? 2 : 1;
      int u      = (int)cb[(x >> 1) * cb_step] - 128;
      int v      = (int)cr[(x >> 1) * cr_step] - 128;
      image_yuv_pair(d + x, n, yp + x, u, v, c, rgba);
   }
}

/* Eight pixels at a time, the same arithmetic in the same fixed point
 * as image_yuv_pair, so every pixel is the one it gives. Returns how
 * many pixels were done, a multiple of eight; the row's rest is
 * image_yuv_row's. Interleaved chroma (@step 2) is read eight bytes at
 * a time, one past the last sample a group uses, so a group runs only
 * where the row has a sample after it. */
#if defined(IMAGE_YUV_SSE2)
static unsigned image_yuv_row_simd(uint32_t *d, const uint8_t *yp,
      const uint8_t *cb, const uint8_t *cr, unsigned step, unsigned w,
      const image_yuv_coef_t *c, unsigned rgba)
{
   const __m128i zero  = _mm_setzero_si128();
   const __m128i lo8   = _mm_set1_epi16(0xFF);
   const __m128i c128  = _mm_set1_epi16(128);
   const __m128i alpha = _mm_set1_epi8(-1);
   const __m128i ym    = _mm_set1_epi16((short)c->ym);
   const __m128i yb    = _mm_set1_epi32(c->yb);
   /* on (Cb, Cr) pairs; Cb -> B is even and past int16, so halved */
   const __m128i kr    = _mm_set_epi16((short)c->rv, 0, (short)c->rv, 0,
         (short)c->rv, 0, (short)c->rv, 0);
   const __m128i kg    = _mm_set_epi16((short)c->gv, (short)c->gu,
         (short)c->gv, (short)c->gu, (short)c->gv, (short)c->gu,
         (short)c->gv, (short)c->gu);
   const __m128i kb    = _mm_set_epi16(0, (short)(c->bu >> 1),
         0, (short)(c->bu >> 1), 0, (short)(c->bu >> 1),
         0, (short)(c->bu >> 1));
   unsigned need       = step == 2 ? 9 : 8;
   unsigned x          = 0;

   for (; x + need <= w; x += 8)
   {
      __m128i yv = _mm_unpacklo_epi8(
            _mm_loadl_epi64((const __m128i*)(yp + x)), zero);
      __m128i u, v, uv, rr, gg, bb, ylo, yhi, lo, hi, r, g, b, p, q;
      if (step == 2)
      {
         u = _mm_and_si128(_mm_loadl_epi64((const __m128i*)(cb + x)), lo8);
         v = _mm_and_si128(_mm_loadl_epi64((const __m128i*)(cr + x)), lo8);
      }
      else
      {
         uint32_t cu, cv;
         memcpy(&cu, cb + (x >> 1), 4);
         memcpy(&cv, cr + (x >> 1), 4);
         u = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)cu), zero);
         v = _mm_unpacklo_epi8(_mm_cvtsi32_si128((int)cv), zero);
      }
      uv  = _mm_unpacklo_epi16(_mm_sub_epi16(u, c128),
            _mm_sub_epi16(v, c128));
      rr  = _mm_madd_epi16(uv, kr);
      gg  = _mm_madd_epi16(uv, kg);
      bb  = _mm_slli_epi32(_mm_madd_epi16(uv, kb), 1);
      lo  = _mm_mullo_epi16(yv, ym);
      hi  = _mm_mulhi_epi16(yv, ym);
      ylo = _mm_add_epi32(_mm_unpacklo_epi16(lo, hi), yb);
      yhi = _mm_add_epi32(_mm_unpackhi_epi16(lo, hi), yb);
#define IMAGE_YUV_SSE2_CH(t) _mm_packus_epi16(_mm_packs_epi32( \
         _mm_srai_epi32(_mm_add_epi32(ylo, \
               _mm_shuffle_epi32(t, _MM_SHUFFLE(1, 1, 0, 0))), IMAGE_YUV_SHIFT), \
         _mm_srai_epi32(_mm_add_epi32(yhi, \
               _mm_shuffle_epi32(t, _MM_SHUFFLE(3, 3, 2, 2))), IMAGE_YUV_SHIFT)), \
         zero)
      r   = IMAGE_YUV_SSE2_CH(rr);
      g   = IMAGE_YUV_SSE2_CH(gg);
      b   = IMAGE_YUV_SSE2_CH(bb);
#undef IMAGE_YUV_SSE2_CH
      /* ARGB words are B, G, R, A in memory; FLAG_RGBA R, G, B, A */
      p   = _mm_unpacklo_epi8(rgba ? r : b, g);
      q   = _mm_unpacklo_epi8(rgba ? b : r, alpha);
      _mm_storeu_si128((__m128i*)(d + x),     _mm_unpacklo_epi16(p, q));
      _mm_storeu_si128((__m128i*)(d + x + 4), _mm_unpackhi_epi16(p, q));
   }
   return x;
}
#elif defined(IMAGE_YUV_NEON)
static unsigned image_yuv_row_simd(uint32_t *d, const uint8_t *yp,
      const uint8_t *cb, const uint8_t *cr, unsigned step, unsigned w,
      const image_yuv_coef_t *c, unsigned rgba)
{
   const int32x4_t yb = vdupq_n_s32(c->yb);
   const int16x4_t mid = vdup_n_s16(128);
   unsigned need       = step == 2 ? 9 : 8;
   unsigned x          = 0;

   for (; x + need <= w; x += 8)
   {
      int16x8_t yv = vreinterpretq_s16_u16(vmovl_u8(vld1_u8(yp + x)));
      int16x4_t u, v;
      int32x4_t rr, gg, bb, ylo, yhi;
      int32x4x2_t r2, g2, b2;
      uint8x8x4_t px;
      uint8x8_t r, g, b;
      if (step == 2)
      {
         const uint16x4_t lo8 = vdup_n_u16(0xFF);
         u = vreinterpret_s16_u16(vand_u16(
               vreinterpret_u16_u8(vld1_u8(cb + x)), lo8));
         v = vreinterpret_s16_u16(vand_u16(
               vreinterpret_u16_u8(vld1_u8(cr + x)), lo8));
      }
      else
      {
         uint32_t cu, cv;
         memcpy(&cu, cb + (x >> 1), 4);
         memcpy(&cv, cr + (x >> 1), 4);
         u = vreinterpret_s16_u16(vget_low_u16(
               vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(cu)))));
         v = vreinterpret_s16_u16(vget_low_u16(
               vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(cv)))));
      }
      u   = vsub_s16(u, mid);
      v   = vsub_s16(v, mid);
      rr  = vmull_n_s16(v, (int16_t)c->rv);
      gg  = vmlal_n_s16(vmull_n_s16(u, (int16_t)c->gu), v, (int16_t)c->gv);
      bb  = vmulq_n_s32(vmovl_s16(u), c->bu);
      ylo = vaddq_s32(vmull_n_s16(vget_low_s16(yv), (int16_t)c->ym), yb);
      yhi = vaddq_s32(vmull_n_s16(vget_high_s16(yv), (int16_t)c->ym), yb);
      r2  = vzipq_s32(rr, rr);
      g2  = vzipq_s32(gg, gg);
      b2  = vzipq_s32(bb, bb);
#define IMAGE_YUV_NEON_CH(t) vqmovun_s16(vcombine_s16( \
         vqmovn_s32(vshrq_n_s32(vaddq_s32(ylo, (t).val[0]), IMAGE_YUV_SHIFT)), \
         vqmovn_s32(vshrq_n_s32(vaddq_s32(yhi, (t).val[1]), IMAGE_YUV_SHIFT))))
      r   = IMAGE_YUV_NEON_CH(r2);
      g   = IMAGE_YUV_NEON_CH(g2);
      b   = IMAGE_YUV_NEON_CH(b2);
#undef IMAGE_YUV_NEON_CH
      /* ARGB words are B, G, R, A in memory; FLAG_RGBA R, G, B, A */
      px.val[0] = rgba ? r : b;
      px.val[1] = g;
      px.val[2] = rgba ? b : r;
      px.val[3] = vdup_n_u8(0xFF);
      vst4_u8((uint8_t*)(d + x), px);
   }
   return x;
}
#endif

/* The frame, two rows at a time sharing their chroma row. */
static void image_yuv_frame(uint32_t *dst, unsigned dst_stride,
      const uint8_t *y, unsigned y_stride,
      const uint8_t *c0, unsigned c0_stride, unsigned c0_step,
      const uint8_t *c1, unsigned c1_stride, unsigned c1_step,
      unsigned w, unsigned h, unsigned flags)
{
   const image_yuv_coef_t *c =
      &image_yuv_coefs[((flags & IMAGE_YUV_FLAG_BT709) ? 2 : 0)
                     | ((flags & IMAGE_YUV_FLAG_FULL_RANGE) ? 1 : 0)];
   unsigned rgba = flags & IMAGE_YUV_FLAG_RGBA;
   unsigned row;

   for (row = 0; row < h; row += 2)
   {
      const uint8_t *cb  = c0 + (size_t)(row >> 1) * c0_stride;
      const uint8_t *cr  = c1 + (size_t)(row >> 1) * c1_stride;
      unsigned rows      = (row + 1 < h) ? 2 : 1;
      unsigned i;

      for (i = 0; i < rows; i++)
      {
         const uint8_t *yp = y + (size_t)(row + i) * y_stride;
         uint32_t *d       = dst + (size_t)(row + i) * dst_stride;
         unsigned x        = 0;
#if defined(IMAGE_YUV_SSE2) || defined(IMAGE_YUV_NEON)
         if (c0_step == c1_step && c0_step <= 2)
            x = image_yuv_row_simd(d, yp, cb, cr, c0_step, w, c, rgba);
#endif
         image_yuv_row(d, yp, cb, c0_step, cr, c1_step, x, w, c, rgba);
      }
   }
}

void image_yuv_420_to_rgb32(uint32_t *dst, unsigned dst_stride,
      const uint8_t *y, unsigned y_stride,
      const uint8_t *cb, unsigned cb_stride,
      const uint8_t *cr, unsigned cr_stride,
      unsigned chroma_step, unsigned w, unsigned h, unsigned flags)
{
   image_yuv_frame(dst, dst_stride, y, y_stride,
         cb, cb_stride, chroma_step, cr, cr_stride, chroma_step,
         w, h, flags);
}

void image_yuv_nv12_to_rgb32(uint32_t *dst, unsigned dst_stride,
      const uint8_t *y, unsigned y_stride,
      const uint8_t *uv, unsigned uv_stride,
      unsigned w, unsigned h, unsigned flags)
{
   const uint8_t *cb = uv;
   const uint8_t *cr = uv + 1;
   if (flags & IMAGE_YUV_FLAG_VU)
   {
      cb = uv + 1;
      cr = uv;
   }
   image_yuv_frame(dst, dst_stride, y, y_stride,
         cb, uv_stride, 2, cr, uv_stride, 2, w, h, flags);
}

void image_yuv_i420_to_rgb32(uint32_t *dst, unsigned dst_stride,
      const uint8_t *y, unsigned y_stride,
      const uint8_t *u, unsigned u_stride,
      const uint8_t *v, unsigned v_stride,
      unsigned w, unsigned h, unsigned flags)
{
   if (flags & IMAGE_YUV_FLAG_VU)
      image_yuv_frame(dst, dst_stride, y, y_stride,
            v, v_stride, 1, u, u_stride, 1, w, h, flags);
   else
      image_yuv_frame(dst, dst_stride, y, y_stride,
            u, u_stride, 1, v, v_stride, 1, w, h, flags);
}

void image_yuv_420_to_bytes(uint8_t *dst, size_t dst_stride,
      const uint8_t *y, unsigned y_stride,
      const uint8_t *cb, unsigned cb_stride,
      const uint8_t *cr, unsigned cr_stride,
      unsigned chroma_step, unsigned w, unsigned h, unsigned flags,
      unsigned order, uint8_t alpha)
{
   const image_yuv_coef_t *c =
      &image_yuv_coefs[((flags & IMAGE_YUV_FLAG_BT709) ? 2 : 0)
                     | ((flags & IMAGE_YUV_FLAG_FULL_RANGE) ? 1 : 0)];
   /* where R, G, B and A go in each pixel */
   const uint8_t *at = image_yuv_order[order & 3];
   unsigned ri = at[0], gi = at[1], bi = at[2], ai = at[3];
   unsigned row;

   for (row = 0; row < h; row++)
   {
      const uint8_t *yr = y  + (size_t)row * y_stride;
      const uint8_t *cu = cb + (size_t)(row >> 1) * cb_stride;
      const uint8_t *cv = cr + (size_t)(row >> 1) * cr_stride;
      uint8_t       *d  = dst + (size_t)row * dst_stride;
      unsigned x;

      for (x = 0; x < w; x++, d += 4)
      {
         int u  = (int)cu[(x >> 1) * chroma_step] - 128;
         int v  = (int)cv[(x >> 1) * chroma_step] - 128;
         int yy = c->ym * yr[x] + c->yb;
         int r  = (yy + c->rv * v) >> IMAGE_YUV_SHIFT;
         int g  = (yy + c->gu * u + c->gv * v) >> IMAGE_YUV_SHIFT;
         int b  = (yy + c->bu * u) >> IMAGE_YUV_SHIFT;
         d[ri] = (uint8_t)IMAGE_YUV_CLAMP(r);
         d[gi] = (uint8_t)IMAGE_YUV_CLAMP(g);
         d[bi] = (uint8_t)IMAGE_YUV_CLAMP(b);
         d[ai] = alpha;
      }
   }
}

void image_yuv_420_from_bytes(uint8_t *y, unsigned y_stride,
      uint8_t *cb, unsigned cb_stride,
      uint8_t *cr, unsigned cr_stride, unsigned chroma_step,
      const uint8_t *src, size_t src_stride,
      unsigned w, unsigned h, unsigned flags, unsigned order)
{
   const image_yuv_fwd_t *c =
      &image_yuv_fwds[((flags & IMAGE_YUV_FLAG_BT709) ? 2 : 0)
                    | ((flags & IMAGE_YUV_FLAG_FULL_RANGE) ? 1 : 0)];
   const uint8_t *at = image_yuv_order[order & 3];
   unsigned ri = at[0], gi = at[1], bi = at[2];
   int      yb = (c->y0 << IMAGE_YUV_SHIFT) + (1 << (IMAGE_YUV_SHIFT - 1));
   /* the four pixels' sum carries two more bits */
   int      cbias = (128 << (IMAGE_YUV_SHIFT + 2)) + (1 << (IMAGE_YUV_SHIFT + 1));
   unsigned row;

   for (row = 0; row < h; row += 2)
   {
      /* an odd last row is its own pair */
      unsigned       row1 = row + 1 < h ? row + 1 : row;
      const uint8_t *s0   = src + (size_t)row  * src_stride;
      const uint8_t *s1   = src + (size_t)row1 * src_stride;
      uint8_t       *y0   = y + (size_t)row  * y_stride;
      uint8_t       *y1   = y + (size_t)row1 * y_stride;
      uint8_t       *u    = cb + (size_t)(row >> 1) * cb_stride;
      uint8_t       *v    = cr + (size_t)(row >> 1) * cr_stride;
      unsigned       x;

      for (x = 0; x < w; x += 2)
      {
         unsigned       x1   = x + 1 < w ? x + 1 : x;
         const uint8_t *p[4];
         int            r = 0, g = 0, b = 0, cu, cv;
         unsigned       k;
         p[0] = s0 + (size_t)x * 4;
         p[1] = s0 + (size_t)x1 * 4;
         p[2] = s1 + (size_t)x * 4;
         p[3] = s1 + (size_t)x1 * 4;
         for (k = 0; k < 4; k++)
         {
            int pr = p[k][ri], pg = p[k][gi], pb = p[k][bi];
            int yy = (c->y[0] * pr + c->y[1] * pg + c->y[2] * pb + yb) >> IMAGE_YUV_SHIFT;
            r += pr;
            g += pg;
            b += pb;
            /* the duplicates of an odd edge write the same sample twice */
            (k < 2 ? y0 : y1)[k & 1 ? x1 : x] = (uint8_t)yy;
         }
         /* the 2x2 average, centred between the four */
         cu = (c->u[0] * r + c->u[1] * g + c->u[2] * b + cbias) >> (IMAGE_YUV_SHIFT + 2);
         cv = (c->v[0] * r + c->v[1] * g + c->v[2] * b + cbias) >> (IMAGE_YUV_SHIFT + 2);
         u[(x >> 1) * chroma_step] = (uint8_t)IMAGE_YUV_CLAMP(cu);
         v[(x >> 1) * chroma_step] = (uint8_t)IMAGE_YUV_CLAMP(cv);
      }
   }
}

void image_yuv_420_to_uyvy(uint8_t *dst, size_t dst_stride,
      const uint8_t *y, unsigned y_stride,
      const uint8_t *cb, unsigned cb_stride,
      const uint8_t *cr, unsigned cr_stride,
      unsigned w, unsigned h)
{
   unsigned row;

   for (row = 0; row < h; row++)
   {
      const uint8_t *yr = y  + (size_t)row * y_stride;
      const uint8_t *cu = cb + (size_t)(row >> 1) * cb_stride;
      const uint8_t *cv = cr + (size_t)(row >> 1) * cr_stride;
      uint8_t       *d  = dst + (size_t)row * dst_stride;
      unsigned x;

      for (x = 0; x < w; x += 2, d += 4)
      {
         d[0] = cu[x >> 1];
         d[1] = yr[x];
         d[2] = cv[x >> 1];
         d[3] = yr[x + 1 < w ? x + 1 : x];
      }
   }
}

void image_yuv_coefficients(unsigned flags, float c[6])
{
   const image_yuv_coef_t *k =
      &image_yuv_coefs[((flags & IMAGE_YUV_FLAG_BT709) ? 2 : 0)
                     | ((flags & IMAGE_YUV_FLAG_FULL_RANGE) ? 1 : 0)];
   const float one = (float)(1 << IMAGE_YUV_SHIFT);
   c[0] = (float)k->ym / one;
   /* the bias less its rounding half, in 0..1 */
   c[1] = (float)(k->yb - (1 << (IMAGE_YUV_SHIFT - 1))) / one / 255.0f;
   c[2] = (float)k->rv / one;
   c[3] = (float)k->gu / one;
   c[4] = (float)k->gv / one;
   c[5] = (float)k->bu / one;
}
