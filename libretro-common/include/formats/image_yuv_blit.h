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

#include <stdint.h>
#include <retro_common_api.h>

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

RETRO_END_DECLS

#endif
