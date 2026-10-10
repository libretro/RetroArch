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

/* The frame, two rows at a time, with the chroma of pixel pair @x
 * fetched by @cb_at / @cr_at as the layout dictates. */
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
      const uint8_t *y0  = y + (size_t)row * y_stride;
      const uint8_t *y1  = y0 + y_stride;
      const uint8_t *cb  = c0 + (size_t)(row >> 1) * c0_stride;
      const uint8_t *cr  = c1 + (size_t)(row >> 1) * c1_stride;
      uint32_t *d0       = dst + (size_t)row * dst_stride;
      uint32_t *d1       = d0 + dst_stride;
      unsigned rows      = (row + 1 < h) ? 2 : 1;
      unsigned x;

      for (x = 0; x < w; x += 2)
      {
         unsigned n = (x + 1 < w) ? 2 : 1;
         int u      = (int)cb[(x >> 1) * c0_step] - 128;
         int v      = (int)cr[(x >> 1) * c1_step] - 128;
         image_yuv_pair(d0 + x, n, y0 + x, u, v, c, rgba);
         if (rows == 2)
            image_yuv_pair(d1 + x, n, y1 + x, u, v, c, rgba);
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
