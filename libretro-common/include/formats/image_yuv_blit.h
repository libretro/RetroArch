/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (image_yuv_blit.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_IMAGE_YUV_BLIT_H__
#define __LIBRETRO_SDK_FORMAT_IMAGE_YUV_BLIT_H__

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <retro_common_api.h>
#include <retro_inline.h>

RETRO_BEGIN_DECLS

/* 8-bit YCbCr 4:2:0 to 32-bit RGB, in fixed point. Strides are in
 * samples (bytes for the planes, pixels for the destination). A frame
 * of odd width or height takes its last column and row from the
 * chroma sample they share with their neighbour. Rows are independent
 * in pairs, so a band starting on an even row converts on its own
 * (image_blit_bands with an align of 2). */

/* Chroma order and colour space of the source, and the word order of
 * the destination. The default is BT.601 limited range into XRGB8888,
 * which is what a camera preview and the libretro frontend speak. */
#define IMAGE_YUV_FLAG_VU         (1 << 0) /* interleaved plane is V,U (NV21) */
#define IMAGE_YUV_FLAG_FULL_RANGE (1 << 1) /* Y 0..255, not 16..235 */
#define IMAGE_YUV_FLAG_BT709      (1 << 2) /* HD matrix, not BT.601 */
#define IMAGE_YUV_FLAG_RGBA       (1 << 3) /* R in the low byte, A set */

/* Any 4:2:0 layout: Cb and Cr each by their own base pointer and row
 * stride, with @chroma_step bytes between horizontally adjacent
 * samples of a plane (1 for I420, 2 for NV12/NV21, whose two planes
 * are the one interleaved plane at offsets 0 and 1). */
void image_yuv_420_to_rgb32(uint32_t *dst, unsigned dst_stride,
      const uint8_t *y, unsigned y_stride,
      const uint8_t *cb, unsigned cb_stride,
      const uint8_t *cr, unsigned cr_stride,
      unsigned chroma_step, unsigned w, unsigned h, unsigned flags);

/* Y plane and one interleaved chroma plane (NV12; NV21 with FLAG_VU). */
void image_yuv_nv12_to_rgb32(uint32_t *dst, unsigned dst_stride,
      const uint8_t *y, unsigned y_stride,
      const uint8_t *uv, unsigned uv_stride,
      unsigned w, unsigned h, unsigned flags);

/* Three planes (I420; FLAG_VU swaps the two chroma planes). */
void image_yuv_i420_to_rgb32(uint32_t *dst, unsigned dst_stride,
      const uint8_t *y, unsigned y_stride,
      const uint8_t *u, unsigned u_stride,
      const uint8_t *v, unsigned v_stride,
      unsigned w, unsigned h, unsigned flags);

/* The same conversion into pixels of four bytes in memory order, on any
 * host: @order is IMAGE_YUV_BYTES_RGBA (R, G, B, A),
 * IMAGE_YUV_BYTES_ARGB (A, R, G, B), IMAGE_YUV_BYTES_BGRA (B, G, R, A;
 * XRGB8888 as a little-endian host stores it) or IMAGE_YUV_BYTES_ABGR
 * (A, B, G, R), the alpha byte @alpha. @dst_stride is in bytes.
 * FLAG_RGBA has no meaning here. */
#define IMAGE_YUV_BYTES_RGBA 0
#define IMAGE_YUV_BYTES_ARGB 1
#define IMAGE_YUV_BYTES_BGRA 2
#define IMAGE_YUV_BYTES_ABGR 3
void image_yuv_420_to_bytes(uint8_t *dst, size_t dst_stride,
      const uint8_t *y, unsigned y_stride,
      const uint8_t *cb, unsigned cb_stride,
      const uint8_t *cr, unsigned cr_stride,
      unsigned chroma_step, unsigned w, unsigned h, unsigned flags,
      unsigned order, uint8_t alpha);

/* The other way, for an encoder: pixels of four bytes in @order (the
 * alpha byte ignored), @src_stride in bytes, to 4:2:0 in the colour
 * space and range @flags name (BT.601 limited range by default, what
 * MPEG-4, H.264 and JPEG encoders take), each chroma sample the average
 * of the 2x2 pixels it covers (centred, as JPEG and MPEG-1 site it). An
 * odd last column or row averages the pixels it has. The planes are
 * laid out as for image_yuv_420_to_bytes; nothing is written past
 * (@w + 1) / 2 chroma samples or @w luma samples of a row. Rows are
 * independent in pairs, so a band starting on an even row converts on
 * its own. */
void image_yuv_420_from_bytes(uint8_t *y, unsigned y_stride,
      uint8_t *cb, unsigned cb_stride,
      uint8_t *cr, unsigned cr_stride, unsigned chroma_step,
      const uint8_t *src, size_t src_stride,
      unsigned w, unsigned h, unsigned flags, unsigned order);

/* 4:2:0 to packed 4:2:2 UYVY (U, Y0, V, Y1 for each pair of pixels),
 * each chroma row serving the two luma rows it covers; @dst_stride in
 * bytes. An odd last column repeats its luma sample. */
void image_yuv_420_to_uyvy(uint8_t *dst, size_t dst_stride,
      const uint8_t *y, unsigned y_stride,
      const uint8_t *cb, unsigned cb_stride,
      const uint8_t *cr, unsigned cr_stride,
      unsigned w, unsigned h);

/* The matrix @flags names (BT709, FULL_RANGE), for a GPU to apply to
 * samples normalised to 0..1: with chroma less 128/255,
 *    R = c[0] * Y + c[1] + c[2] * Cr
 *    G = c[0] * Y + c[1] + c[3] * Cb + c[4] * Cr
 *    B = c[0] * Y + c[1] + c[5] * Cb
 * the same matrix the conversions above use. */
void image_yuv_coefficients(unsigned flags, float c[6]);

/* An 8-bit 4:2:0 frame's planes copied into one packed I420 frame of
 * @dst_w x @dst_h (IMAGE_PIXFMT_FRAME_SIZE of IMAGE_PIXFMT_I420): Y
 * rows @dst_w bytes apart, then Cb, then Cr, rows (@dst_w + 1) / 2
 * apart. @w x @h of it are written, clipped to the frame; @uv_stride
 * is both chroma planes'. */
static INLINE void image_yuv_420_pack(uint8_t *dst,
      unsigned dst_w, unsigned dst_h,
      const uint8_t *y, unsigned y_stride,
      const uint8_t *cb, const uint8_t *cr, unsigned uv_stride,
      unsigned w, unsigned h)
{
   unsigned row;
   unsigned dcw  = (dst_w + 1) >> 1;
   size_t   csz  = (size_t)dcw * ((dst_h + 1) >> 1);
   uint8_t *ob   = dst + (size_t)dst_w * dst_h;
   uint8_t *oc   = ob + csz;
   unsigned cw, ch;
   if (w > dst_w)
      w = dst_w;
   if (h > dst_h)
      h = dst_h;
   cw = (w + 1) >> 1;
   ch = (h + 1) >> 1;
   if (y_stride == dst_w && w == dst_w)
      memcpy(dst, y, (size_t)dst_w * h);
   else
      for (row = 0; row < h; row++)
         memcpy(dst + (size_t)row * dst_w, y + (size_t)row * y_stride, w);
   for (row = 0; row < ch; row++)
   {
      memcpy(ob + (size_t)row * dcw, cb + (size_t)row * uv_stride, cw);
      memcpy(oc + (size_t)row * dcw, cr + (size_t)row * uv_stride, cw);
   }
}

/* Packed 4:2:2 Y0 Cb Y1 Cr (YUYV) rows @src_pitch apart into NV12:
 * @w x @h luma at @y, rows @w apart, then (@h + 1) / 2 rows of
 * interleaved CbCr at @uv, also @w apart. A row pair's chroma is
 * averaged; a lone last row keeps its own. @w is even, as YUYV's
 * macropixels are. */
static INLINE void image_yuv_422_to_nv12(uint8_t *y, uint8_t *uv,
      const uint8_t *src, size_t src_pitch, unsigned w, unsigned h)
{
   unsigned row, x;
   unsigned pairs = w >> 1;
   for (row = 0; row < h; row += 2)
   {
      const uint8_t *s0 = src + (size_t)row * src_pitch;
      const uint8_t *s1 = row + 1 < h ? s0 + src_pitch : s0;
      uint8_t       *y0 = y  + (size_t)row * w;
      uint8_t       *y1 = row + 1 < h ? y0 + w : y0;
      uint8_t       *c  = uv + (size_t)(row >> 1) * w;
      for (x = 0; x < pairs; x++)
      {
         const uint8_t *a = s0 + 4 * x;
         const uint8_t *b = s1 + 4 * x;
         y0[2 * x]     = a[0];
         y0[2 * x + 1] = a[2];
         y1[2 * x]     = b[0];
         y1[2 * x + 1] = b[2];
         c[2 * x]      = (uint8_t)((a[1] + b[1] + 1) >> 1);
         c[2 * x + 1]  = (uint8_t)((a[3] + b[3] + 1) >> 1);
      }
   }
}

RETRO_END_DECLS

#endif
