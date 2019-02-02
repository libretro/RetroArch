/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef KHRN_INT_IMAGE_H
#define KHRN_INT_IMAGE_H

#include "interface/khronos/common/khrn_int_util.h"

/******************************************************************************
formats
******************************************************************************/

typedef enum {
   /*
      layout of KHRN_IMAGE_FORMAT_T bits

      When changing this, remember to change
      middleware/khronos/common/2708/khrn_image_4.inc too
   */

   IMAGE_FORMAT_MEM_LAYOUT_MASK = 0x7 << 0,
   IMAGE_FORMAT_MEM_LAYOUT_SHIFT = 0,
   IMAGE_FORMAT_MEM_LAYOUT_BC = 3,

   IMAGE_FORMAT_RSO   = 0 << 0, /* raster scan order */
   IMAGE_FORMAT_BRCM1    = 1 << 0,
   IMAGE_FORMAT_BRCM2    = 2 << 0,
   IMAGE_FORMAT_BRCM3 = 3 << 0,
   IMAGE_FORMAT_BRCM4 = 4 << 0,

   IMAGE_FORMAT_COMP_MASK = 0x3 << 6,

   IMAGE_FORMAT_UNCOMP      = 0 << 6,
   IMAGE_FORMAT_ETC1        = 1 << 6,
   IMAGE_FORMAT_YUYV        = 3 << 6,

   /* uncomp */

   IMAGE_FORMAT_PIXEL_SIZE_MASK = 0x7 << 3,
   IMAGE_FORMAT_PIXEL_SIZE_SHIFT = 3,

   IMAGE_FORMAT_1  = 0 << 3,
   IMAGE_FORMAT_4  = 1 << 3,
   IMAGE_FORMAT_8  = 2 << 3,
   IMAGE_FORMAT_16 = 3 << 3,
   IMAGE_FORMAT_24 = 4 << 3,
   IMAGE_FORMAT_32 = 5 << 3,
   IMAGE_FORMAT_64 = 6 << 3,

   IMAGE_FORMAT_PIXEL_TYPE_MASK = 0x3 << 8,

   IMAGE_FORMAT_COLOR   = 0 << 8,
   IMAGE_FORMAT_PALETTE = 1 << 8,
   IMAGE_FORMAT_SAMPLE  = 2 << 8,
   IMAGE_FORMAT_DEPTH   = 3 << 8, /* packed z and stencil */

   /* uncomp, color */

   IMAGE_FORMAT_RGB = 1 << 10,
   IMAGE_FORMAT_L   = 1 << 11,
   IMAGE_FORMAT_A   = 1 << 12,

   IMAGE_FORMAT_XBGRX = 0 << 13, /* r in low bits */
   IMAGE_FORMAT_XRGBX = 1 << 13, /* r in high bits */

   IMAGE_FORMAT_YUYV_STD = 0 << 13, /* Standard ordering */
   IMAGE_FORMAT_YUYV_REV = 1 << 13, /* Reversed (UYVY) ordering */

   IMAGE_FORMAT_AX = 0 << 14, /* alpha/x in high bits */
   IMAGE_FORMAT_XA = 1 << 14, /* alpha/x in low bits */

   IMAGE_FORMAT_PIXEL_LAYOUT_MASK = 0x7 << 15,

   /* pixel size used to differentiate between eg 4444 and 8888 */
   /* unspecified (0) means one channel occupying entire pixel */
   /* IMAGE_FORMAT_32 */
   IMAGE_FORMAT_8888 = 1 << 15,
   /* IMAGE_FORMAT_24 */
   IMAGE_FORMAT_888  = 1 << 15,
   /* IMAGE_FORMAT_16 */
   IMAGE_FORMAT_4444 = 1 << 15,
   IMAGE_FORMAT_5551 = 2 << 15, /* (or 1555) */
   IMAGE_FORMAT_565  = 3 << 15,
   IMAGE_FORMAT_88   = 4 << 15,

   IMAGE_FORMAT_PRE = 1 << 18, /* premultiplied (for vg) */
   IMAGE_FORMAT_LIN = 1 << 19, /* linear (for vg) */

   /* uncomp, depth */

   IMAGE_FORMAT_Z       = 1 << 10,
   IMAGE_FORMAT_STENCIL = 1 << 11,

   /*
      some IMAGE_FORMAT_Ts

      Components are listed with the most significant bits first.
      
      On little-endian systems, it is as if a pixel was loaded as a little-endian
      integer. This means they are in the *opposite* order to how they appear in memory (e.g. in ABGR_8888
      format, the first byte would be the red component of the first pixel).

      On big-endian systems, it is as if a pixel was loaded as a big-endian integer.
      This means that the components are written in the same order that they appear in
      memory (e.g. in ABGR_8888 format, the first byte would be alpha).
   */

   #define IMAGE_FORMAT_PACK(A, B, C, D, E, F, G, H, I) \
      (IMAGE_FORMAT_##A | IMAGE_FORMAT_##B | IMAGE_FORMAT_##C | IMAGE_FORMAT_##D | IMAGE_FORMAT_##E | \
      IMAGE_FORMAT_##F | IMAGE_FORMAT_##G | IMAGE_FORMAT_##H | IMAGE_FORMAT_##I)
   #define IMAGE_FORMAT__ 0

   RGBA_8888 = IMAGE_FORMAT_PACK(_, UNCOMP, 32, COLOR  , RGB, A, XRGBX, XA, 8888),
   BGRA_8888 = IMAGE_FORMAT_PACK(_, UNCOMP, 32, COLOR  , RGB, A, XBGRX, XA, 8888),
   ARGB_8888 = IMAGE_FORMAT_PACK(_, UNCOMP, 32, COLOR  , RGB, A, XRGBX, AX, 8888),
   ABGR_8888 = IMAGE_FORMAT_PACK(_, UNCOMP, 32, COLOR  , RGB, A, XBGRX, AX, 8888),
   RGBX_8888 = IMAGE_FORMAT_PACK(_, UNCOMP, 32, COLOR  , RGB, _, XRGBX, XA, 8888),
   BGRX_8888 = IMAGE_FORMAT_PACK(_, UNCOMP, 32, COLOR  , RGB, _, XBGRX, XA, 8888),
   XRGB_8888 = IMAGE_FORMAT_PACK(_, UNCOMP, 32, COLOR  , RGB, _, XRGBX, AX, 8888),
   XBGR_8888 = IMAGE_FORMAT_PACK(_, UNCOMP, 32, COLOR  , RGB, _, XBGRX, AX, 8888),
   RGB_888   = IMAGE_FORMAT_PACK(_, UNCOMP, 24, COLOR  , RGB, _, XRGBX, _ , 888 ),
   BGR_888   = IMAGE_FORMAT_PACK(_, UNCOMP, 24, COLOR  , RGB, _, XBGRX, _ , 888 ),
   RGBA_4444 = IMAGE_FORMAT_PACK(_, UNCOMP, 16, COLOR  , RGB, A, XRGBX, XA, 4444),
   BGRA_4444 = IMAGE_FORMAT_PACK(_, UNCOMP, 16, COLOR  , RGB, A, XBGRX, XA, 4444),
   ARGB_4444 = IMAGE_FORMAT_PACK(_, UNCOMP, 16, COLOR  , RGB, A, XRGBX, AX, 4444),
   ABGR_4444 = IMAGE_FORMAT_PACK(_, UNCOMP, 16, COLOR  , RGB, A, XBGRX, AX, 4444),
   RGBA_5551 = IMAGE_FORMAT_PACK(_, UNCOMP, 16, COLOR  , RGB, A, XRGBX, XA, 5551),
   BGRA_5551 = IMAGE_FORMAT_PACK(_, UNCOMP, 16, COLOR  , RGB, A, XBGRX, XA, 5551),
   ARGB_1555 = IMAGE_FORMAT_PACK(_, UNCOMP, 16, COLOR  , RGB, A, XRGBX, AX, 5551),
   ABGR_1555 = IMAGE_FORMAT_PACK(_, UNCOMP, 16, COLOR  , RGB, A, XBGRX, AX, 5551),
   RGB_565   = IMAGE_FORMAT_PACK(_, UNCOMP, 16, COLOR  , RGB, _, XRGBX, _ , 565 ),
   BGR_565   = IMAGE_FORMAT_PACK(_, UNCOMP, 16, COLOR  , RGB, _, XBGRX, _ , 565 ),
   LA_88     = IMAGE_FORMAT_PACK(_, UNCOMP, 16, COLOR  , L  , A, _    , XA, 88  ),
   AL_88     = IMAGE_FORMAT_PACK(_, UNCOMP, 16, COLOR  , L  , A, _    , AX, 88  ),
   L_8       = IMAGE_FORMAT_PACK(_, UNCOMP, 8 , COLOR  , L  , _, _    , _ , _   ),
   L_1       = IMAGE_FORMAT_PACK(_, UNCOMP, 1 , COLOR  , L  , _, _    , _ , _   ),
   A_8       = IMAGE_FORMAT_PACK(_, UNCOMP, 8 , COLOR  , _  , A, _    , _ , _   ),
   A_4       = IMAGE_FORMAT_PACK(_, UNCOMP, 4 , COLOR  , _  , A, _    , _ , _   ),
   A_1       = IMAGE_FORMAT_PACK(_, UNCOMP, 1 , COLOR  , _  , A, _    , _ , _   ),
   PALETTE_4 = IMAGE_FORMAT_PACK(_, UNCOMP, 4 , PALETTE, _  , _, _    , _ , _   ),
   SAMPLE_16 = IMAGE_FORMAT_PACK(_, UNCOMP, 16, SAMPLE , _  , _, _    , _ , _   ),
   SAMPLE_8  = IMAGE_FORMAT_PACK(_, UNCOMP, 8 , SAMPLE , _  , _, _    , _ , _   ),
   DEPTH_32  = IMAGE_FORMAT_PACK(_, UNCOMP, 32, DEPTH  , Z, STENCIL, _, _ , _   ),
   DEPTH_16  = IMAGE_FORMAT_PACK(_, UNCOMP, 16, DEPTH  , Z, _      , _, _ , _   ),
   ETC1      = IMAGE_FORMAT_PACK(_, ETC1  , _ , _      , _  , _, _    , _ , _   ),
   DEPTH_64  = IMAGE_FORMAT_PACK(_, UNCOMP, 64, DEPTH  , Z, STENCIL, _, _ , _   ),
   YUV_422     = IMAGE_FORMAT_PACK(_, YUYV  , _ , _      , _  , _, YUYV_STD , _ , _   ),
   YUV_422_REV = IMAGE_FORMAT_PACK(_, YUYV  , _ , _      , _  , _, YUYV_REV , _ , _   ),

   #undef IMAGE_FORMAT__
   #undef IMAGE_FORMAT_PACK

   /* texture unit formats */
   ABGR_8888_TF = ABGR_8888 | IMAGE_FORMAT_BRCM1,
   ARGB_8888_TF = ARGB_8888 | IMAGE_FORMAT_BRCM1,
   RGBA_8888_TF = RGBA_8888 | IMAGE_FORMAT_BRCM1,
   XBGR_8888_TF = XBGR_8888 | IMAGE_FORMAT_BRCM1,
   XRGB_8888_TF = XRGB_8888 | IMAGE_FORMAT_BRCM1,
   RGBX_8888_TF = RGBX_8888 | IMAGE_FORMAT_BRCM1,
   RGBA_4444_TF = RGBA_4444 | IMAGE_FORMAT_BRCM1,
   RGBA_5551_TF = RGBA_5551 | IMAGE_FORMAT_BRCM1,
   RGB_565_TF   = RGB_565   | IMAGE_FORMAT_BRCM1,
   BGR_565_TF   = BGR_565   | IMAGE_FORMAT_BRCM1,
   L_8_TF       = L_8       | IMAGE_FORMAT_BRCM1,
   A_8_TF       = A_8       | IMAGE_FORMAT_BRCM1,
   AL_88_TF     = AL_88     | IMAGE_FORMAT_BRCM1,
   LA_88_TF     = LA_88     | IMAGE_FORMAT_BRCM1,
   ETC1_TF      = ETC1      | IMAGE_FORMAT_BRCM1,
   PALETTE_4_TF = PALETTE_4 | IMAGE_FORMAT_BRCM1,
   SAMPLE_8_TF  = SAMPLE_8  | IMAGE_FORMAT_BRCM1,
   SAMPLE_16_TF = SAMPLE_16 | IMAGE_FORMAT_BRCM1,
   L_1_TF       = L_1       | IMAGE_FORMAT_BRCM1,
   A_4_TF       = A_4       | IMAGE_FORMAT_BRCM1,
   A_1_TF       = A_1       | IMAGE_FORMAT_BRCM1,
   DEPTH_16_TF  = DEPTH_16  | IMAGE_FORMAT_BRCM1,
   DEPTH_32_TF  = DEPTH_32  | IMAGE_FORMAT_BRCM1,

   /* their linear T equivalents */
   ABGR_8888_LT = ABGR_8888 | IMAGE_FORMAT_BRCM2,
   ARGB_8888_LT = ARGB_8888 | IMAGE_FORMAT_BRCM2,
   RGBA_8888_LT = RGBA_8888 | IMAGE_FORMAT_BRCM2,
   XBGR_8888_LT = XBGR_8888 | IMAGE_FORMAT_BRCM2,
   XRGB_8888_LT = XRGB_8888 | IMAGE_FORMAT_BRCM2,
   RGBX_8888_LT = RGBX_8888 | IMAGE_FORMAT_BRCM2,
   RGBA_4444_LT = RGBA_4444 | IMAGE_FORMAT_BRCM2,
   RGBA_5551_LT = RGBA_5551 | IMAGE_FORMAT_BRCM2,
   RGB_565_LT   = RGB_565   | IMAGE_FORMAT_BRCM2,
   L_8_LT       = L_8       | IMAGE_FORMAT_BRCM2,
   A_8_LT       = A_8       | IMAGE_FORMAT_BRCM2,
   AL_88_LT     = AL_88     | IMAGE_FORMAT_BRCM2,
   LA_88_LT     = LA_88     | IMAGE_FORMAT_BRCM2,
   ETC1_LT      = ETC1      | IMAGE_FORMAT_BRCM2,
   PALETTE_4_LT = PALETTE_4 | IMAGE_FORMAT_BRCM2,
   SAMPLE_8_LT  = SAMPLE_8  | IMAGE_FORMAT_BRCM2,
   SAMPLE_16_LT = SAMPLE_16 | IMAGE_FORMAT_BRCM2,
   L_1_LT       = L_1       | IMAGE_FORMAT_BRCM2,
   A_4_LT       = A_4       | IMAGE_FORMAT_BRCM2,
   A_1_LT       = A_1       | IMAGE_FORMAT_BRCM2,
   DEPTH_16_LT  = DEPTH_16  | IMAGE_FORMAT_BRCM2,
   DEPTH_32_LT  = DEPTH_32  | IMAGE_FORMAT_BRCM2,

   /* some raster order formats */
   RGBA_8888_RSO = RGBA_8888 | IMAGE_FORMAT_RSO,
   BGRA_8888_RSO = BGRA_8888 | IMAGE_FORMAT_RSO,
   ARGB_8888_RSO = ARGB_8888 | IMAGE_FORMAT_RSO,
   ABGR_8888_RSO = ABGR_8888 | IMAGE_FORMAT_RSO,
   RGBX_8888_RSO = RGBX_8888 | IMAGE_FORMAT_RSO,
   BGRX_8888_RSO = BGRX_8888 | IMAGE_FORMAT_RSO,
   XRGB_8888_RSO = XRGB_8888 | IMAGE_FORMAT_RSO,
   XBGR_8888_RSO = XBGR_8888 | IMAGE_FORMAT_RSO,
   BGR_888_RSO   = BGR_888   | IMAGE_FORMAT_RSO,
   RGB_888_RSO   = RGB_888   | IMAGE_FORMAT_RSO,
   RGBA_4444_RSO = RGBA_4444 | IMAGE_FORMAT_RSO,
   BGRA_4444_RSO = BGRA_4444 | IMAGE_FORMAT_RSO,
   ARGB_4444_RSO = ARGB_4444 | IMAGE_FORMAT_RSO,
   ABGR_4444_RSO = ABGR_4444 | IMAGE_FORMAT_RSO,
   RGBA_5551_RSO = RGBA_5551 | IMAGE_FORMAT_RSO,
   BGRA_5551_RSO = BGRA_5551 | IMAGE_FORMAT_RSO,
   ARGB_1555_RSO = ARGB_1555 | IMAGE_FORMAT_RSO,
   ABGR_1555_RSO = ABGR_1555 | IMAGE_FORMAT_RSO,
   RGB_565_RSO   = RGB_565   | IMAGE_FORMAT_RSO,
   BGR_565_RSO   = BGR_565   | IMAGE_FORMAT_RSO,
   AL_88_RSO     = AL_88     | IMAGE_FORMAT_RSO,
   LA_88_RSO     = LA_88     | IMAGE_FORMAT_RSO,
   L_8_RSO       = L_8       | IMAGE_FORMAT_RSO,
   L_1_RSO       = L_1       | IMAGE_FORMAT_RSO,
   A_8_RSO       = A_8       | IMAGE_FORMAT_RSO,
   A_4_RSO       = A_4       | IMAGE_FORMAT_RSO,
   A_1_RSO       = A_1       | IMAGE_FORMAT_RSO,

   YUV_422_RSO     = YUV_422     | IMAGE_FORMAT_RSO,
   YUV_422_REV_RSO = YUV_422_REV | IMAGE_FORMAT_RSO,

   ARGB_8888_PRE = ARGB_8888 | IMAGE_FORMAT_PRE,

   /* TLB dump formats */
   DEPTH_32_TLBD = DEPTH_32  | IMAGE_FORMAT_BRCM4,
   DEPTH_COL_64_TLBD = DEPTH_64 | IMAGE_FORMAT_BRCM4,
   COL_32_TLBD = BGRA_8888 | IMAGE_FORMAT_BRCM4,

   IMAGE_FORMAT_INVALID = 0xffffffff
} KHRN_IMAGE_FORMAT_T;

static INLINE bool khrn_image_is_rso(KHRN_IMAGE_FORMAT_T format)
{
   vcos_assert(format != IMAGE_FORMAT_INVALID);
   return (format & IMAGE_FORMAT_MEM_LAYOUT_MASK) == IMAGE_FORMAT_RSO;
}

static INLINE bool khrn_image_is_brcm1(KHRN_IMAGE_FORMAT_T format)
{
   vcos_assert(format != IMAGE_FORMAT_INVALID);
   return (format & IMAGE_FORMAT_MEM_LAYOUT_MASK) == IMAGE_FORMAT_BRCM1;
}

static INLINE bool khrn_image_is_brcm2(KHRN_IMAGE_FORMAT_T format)
{
   vcos_assert(format != IMAGE_FORMAT_INVALID);
   return (format & IMAGE_FORMAT_MEM_LAYOUT_MASK) == IMAGE_FORMAT_BRCM2;
}

static INLINE KHRN_IMAGE_FORMAT_T khrn_image_to_rso_format(KHRN_IMAGE_FORMAT_T format)
{
   vcos_assert(format != IMAGE_FORMAT_INVALID);
   return (KHRN_IMAGE_FORMAT_T)((format & ~IMAGE_FORMAT_MEM_LAYOUT_MASK) | IMAGE_FORMAT_RSO);
}

static INLINE KHRN_IMAGE_FORMAT_T khrn_image_to_tf_format(KHRN_IMAGE_FORMAT_T format)
{
   vcos_assert(format != IMAGE_FORMAT_INVALID);
   return (KHRN_IMAGE_FORMAT_T)((format & ~IMAGE_FORMAT_MEM_LAYOUT_MASK) | IMAGE_FORMAT_BRCM1);
}

static INLINE KHRN_IMAGE_FORMAT_T khrn_image_to_lt_format(KHRN_IMAGE_FORMAT_T format)
{
   vcos_assert(format != IMAGE_FORMAT_INVALID);
   return (KHRN_IMAGE_FORMAT_T)((format & ~IMAGE_FORMAT_MEM_LAYOUT_MASK) | IMAGE_FORMAT_BRCM2);
}

static INLINE bool khrn_image_is_uncomp(KHRN_IMAGE_FORMAT_T format)
{
   vcos_assert(format != IMAGE_FORMAT_INVALID);
   return (format & IMAGE_FORMAT_COMP_MASK) == IMAGE_FORMAT_UNCOMP;
}

static INLINE bool khrn_image_is_color(KHRN_IMAGE_FORMAT_T format)
{
   vcos_assert(format != IMAGE_FORMAT_INVALID);
   return (format & (IMAGE_FORMAT_COMP_MASK | IMAGE_FORMAT_PIXEL_TYPE_MASK)) == (IMAGE_FORMAT_UNCOMP | IMAGE_FORMAT_COLOR);
}

static INLINE bool khrn_image_is_gray(KHRN_IMAGE_FORMAT_T format)
{
   vcos_assert(format != IMAGE_FORMAT_INVALID);
   return (format & (IMAGE_FORMAT_L)) != 0;
}

static INLINE bool khrn_image_is_paletted(KHRN_IMAGE_FORMAT_T format)
{
   vcos_assert(format != IMAGE_FORMAT_INVALID);
   return (format & (IMAGE_FORMAT_COMP_MASK | IMAGE_FORMAT_PIXEL_TYPE_MASK)) == (IMAGE_FORMAT_UNCOMP | IMAGE_FORMAT_PALETTE);
}

static INLINE bool khrn_image_is_depth(KHRN_IMAGE_FORMAT_T format)
{
   vcos_assert(format != IMAGE_FORMAT_INVALID);
   return (format & (IMAGE_FORMAT_COMP_MASK | IMAGE_FORMAT_PIXEL_TYPE_MASK)) == (IMAGE_FORMAT_UNCOMP | IMAGE_FORMAT_DEPTH);
}

static INLINE bool khrn_image_is_etc1(KHRN_IMAGE_FORMAT_T format)
{
   vcos_assert(format != IMAGE_FORMAT_INVALID);
   return (format & IMAGE_FORMAT_COMP_MASK) == IMAGE_FORMAT_ETC1;
}

static INLINE bool khrn_image_is_yuv422(KHRN_IMAGE_FORMAT_T format)
{
   vcos_assert(format != IMAGE_FORMAT_INVALID);
   return (format & IMAGE_FORMAT_COMP_MASK) == IMAGE_FORMAT_YUYV;
}

extern uint32_t khrn_image_get_bpp(KHRN_IMAGE_FORMAT_T format);

extern uint32_t khrn_image_get_red_size(KHRN_IMAGE_FORMAT_T format);
extern uint32_t khrn_image_get_green_size(KHRN_IMAGE_FORMAT_T format);
extern uint32_t khrn_image_get_blue_size(KHRN_IMAGE_FORMAT_T format);
extern uint32_t khrn_image_get_alpha_size(KHRN_IMAGE_FORMAT_T format);
extern uint32_t khrn_image_get_z_size(KHRN_IMAGE_FORMAT_T format);
extern uint32_t khrn_image_get_stencil_size(KHRN_IMAGE_FORMAT_T format);

extern uint32_t khrn_image_get_log2_brcm2_width(KHRN_IMAGE_FORMAT_T format);
extern uint32_t khrn_image_get_log2_brcm2_height(KHRN_IMAGE_FORMAT_T format);
extern uint32_t khrn_image_get_log2_brcm1_width(KHRN_IMAGE_FORMAT_T format);
extern uint32_t khrn_image_get_log2_brcm1_height(KHRN_IMAGE_FORMAT_T format);

/******************************************************************************
image handling
******************************************************************************/

typedef struct {
   KHRN_IMAGE_FORMAT_T format;

   uint16_t width;
   uint16_t height;

   int32_t stride; /* in bytes */

   void *aux;
   void *storage;
} KHRN_IMAGE_WRAP_T;

extern uint32_t khrn_image_pad_width(KHRN_IMAGE_FORMAT_T format, uint32_t width);
extern uint32_t khrn_image_pad_height(KHRN_IMAGE_FORMAT_T format, uint32_t height);
extern uint32_t khrn_image_get_stride(KHRN_IMAGE_FORMAT_T format, uint32_t width);
extern uint32_t khrn_image_get_size(KHRN_IMAGE_FORMAT_T format, uint32_t width, uint32_t height);

extern void khrn_image_wrap(KHRN_IMAGE_WRAP_T *wrap, KHRN_IMAGE_FORMAT_T format, uint32_t width, uint32_t height, int32_t stride, void *storage);

#endif
