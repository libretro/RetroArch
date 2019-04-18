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

#include "interface/khronos/common/khrn_int_common.h"

#include "interface/khronos/common/khrn_int_image.h"
#include "interface/khronos/common/khrn_int_util.h"

/******************************************************************************
formats
******************************************************************************/

uint32_t khrn_image_get_bpp(KHRN_IMAGE_FORMAT_T format)
{
   vcos_assert(format != IMAGE_FORMAT_INVALID);

   switch (format & IMAGE_FORMAT_COMP_MASK) {
   case IMAGE_FORMAT_UNCOMP:
      switch (format & IMAGE_FORMAT_PIXEL_SIZE_MASK) {
         case IMAGE_FORMAT_1:  return 1;
         case IMAGE_FORMAT_4:  return 4;
         case IMAGE_FORMAT_8:  return 8;
         case IMAGE_FORMAT_16: return 16;
         case IMAGE_FORMAT_24: return 24;
         case IMAGE_FORMAT_32: return 32;
         case IMAGE_FORMAT_64: return 64;
         default:              UNREACHABLE(); return 0;
      }
   case IMAGE_FORMAT_ETC1: return 4;
   case IMAGE_FORMAT_YUYV: return 16;
   default:                UNREACHABLE(); return 0;
   }
}

/*
   returns the number of red bits in each pixel of an image of the
   specified format, or zero if not an RGB or RGBA format
*/

uint32_t khrn_image_get_red_size(KHRN_IMAGE_FORMAT_T format)
{
   if (khrn_image_is_color(format) && (format & IMAGE_FORMAT_RGB)) {
      switch (format & (IMAGE_FORMAT_PIXEL_SIZE_MASK | IMAGE_FORMAT_PIXEL_LAYOUT_MASK)) {
      case (IMAGE_FORMAT_32 | IMAGE_FORMAT_8888): return 8;
      case (IMAGE_FORMAT_24 | IMAGE_FORMAT_888):  return 8;
      case (IMAGE_FORMAT_16 | IMAGE_FORMAT_4444): return 4;
      case (IMAGE_FORMAT_16 | IMAGE_FORMAT_5551): return 5;
      case (IMAGE_FORMAT_16 | IMAGE_FORMAT_565):  return 5;
      default:                                    UNREACHABLE(); return 0;
      }
   } else {
      return 0;
   }
}

/*
   returns the number of green bits in each pixel of an image of the
   specified format, or zero if not an RGB or RGBA format
*/

uint32_t khrn_image_get_green_size(KHRN_IMAGE_FORMAT_T format)
{
   if (khrn_image_is_color(format) && (format & IMAGE_FORMAT_RGB)) {
      switch (format & (IMAGE_FORMAT_PIXEL_SIZE_MASK | IMAGE_FORMAT_PIXEL_LAYOUT_MASK)) {
      case (IMAGE_FORMAT_32 | IMAGE_FORMAT_8888): return 8;
      case (IMAGE_FORMAT_24 | IMAGE_FORMAT_888):  return 8;
      case (IMAGE_FORMAT_16 | IMAGE_FORMAT_4444): return 4;
      case (IMAGE_FORMAT_16 | IMAGE_FORMAT_5551): return 5;
      case (IMAGE_FORMAT_16 | IMAGE_FORMAT_565):  return 6;
      default:                                    UNREACHABLE(); return 0;
      }
   } else {
      return 0;
   }
}

/*
   returns the number of blue bits in each pixel of an image of the
   specified format, or zero if not an RGB or RGBA format
*/

uint32_t khrn_image_get_blue_size(KHRN_IMAGE_FORMAT_T format)
{
   if (khrn_image_is_color(format) && (format & IMAGE_FORMAT_RGB)) {
      switch (format & (IMAGE_FORMAT_PIXEL_SIZE_MASK | IMAGE_FORMAT_PIXEL_LAYOUT_MASK)) {
      case (IMAGE_FORMAT_32 | IMAGE_FORMAT_8888): return 8;
      case (IMAGE_FORMAT_24 | IMAGE_FORMAT_888):  return 8;
      case (IMAGE_FORMAT_16 | IMAGE_FORMAT_4444): return 4;
      case (IMAGE_FORMAT_16 | IMAGE_FORMAT_5551): return 5;
      case (IMAGE_FORMAT_16 | IMAGE_FORMAT_565):  return 5;
      default:                                    UNREACHABLE(); return 0;
      }
   } else {
      return 0;
   }
}

/*
   returns the number of alpha bits in each pixel of an image of the
   specified format, or zero if not an alpha or RGBA format
*/

uint32_t khrn_image_get_alpha_size(KHRN_IMAGE_FORMAT_T format)
{
   if (khrn_image_is_color(format) && (format & IMAGE_FORMAT_A)) {
      switch (format & (IMAGE_FORMAT_PIXEL_SIZE_MASK | IMAGE_FORMAT_PIXEL_LAYOUT_MASK)) {
      case (IMAGE_FORMAT_32 | IMAGE_FORMAT_8888): return 8;
      case (IMAGE_FORMAT_16 | IMAGE_FORMAT_4444): return 4;
      case (IMAGE_FORMAT_16 | IMAGE_FORMAT_5551): return 1;
      case (IMAGE_FORMAT_16 | IMAGE_FORMAT_88):   return 8;
      case IMAGE_FORMAT_8:                        return 8;
      case IMAGE_FORMAT_4:                        return 4;
      case IMAGE_FORMAT_1:                        return 1;
      default:                                    UNREACHABLE(); return 0;
      }
   } else {
      return 0;
   }
}

/*
   returns the number of depth bits in each pixel of an image of the
   specified format, or zero if not a depth or depth+stencil format
*/

uint32_t khrn_image_get_z_size(KHRN_IMAGE_FORMAT_T format)
{
   if (khrn_image_is_depth(format) && (format & IMAGE_FORMAT_Z)) {
      if (format == DEPTH_32_TLBD || format == DEPTH_COL_64_TLBD)
         return 24;
      switch (format & IMAGE_FORMAT_PIXEL_SIZE_MASK) {
      case IMAGE_FORMAT_32: return 24;
      case IMAGE_FORMAT_16: return 16;
      default:              UNREACHABLE(); return 0;
      }
   } else {
      return 0;
   }
}

/*
   returns the number of stencil bits in each pixel of an image of the
   specified format, or zero if not a depth+stencil format
*/

uint32_t khrn_image_get_stencil_size(KHRN_IMAGE_FORMAT_T format)
{
   if (khrn_image_is_depth(format) && (format & IMAGE_FORMAT_STENCIL)) {
      if (format == DEPTH_32_TLBD || format == DEPTH_COL_64_TLBD)
         return 8;
      vcos_assert((format & IMAGE_FORMAT_PIXEL_SIZE_MASK) == IMAGE_FORMAT_32);
      return 8;
   } else {
      return 0;
   }
}

uint32_t khrn_image_get_log2_brcm2_width(KHRN_IMAGE_FORMAT_T format)
{
   vcos_assert(khrn_image_is_brcm1(format) || khrn_image_is_brcm2(format));

   switch (format & IMAGE_FORMAT_COMP_MASK) {
   case IMAGE_FORMAT_UNCOMP:
   {
      switch (format & IMAGE_FORMAT_PIXEL_SIZE_MASK) {
      case IMAGE_FORMAT_1:  return 6;
      case IMAGE_FORMAT_4:  return 4;
      case IMAGE_FORMAT_8:  return 3;
      case IMAGE_FORMAT_16: return 3;
      case IMAGE_FORMAT_32: return 2;
      default:              UNREACHABLE(); return 0;
      }
   }
   case IMAGE_FORMAT_ETC1: return 3;
   case IMAGE_FORMAT_YUYV: return 3;
   default:                UNREACHABLE(); return 0;
   }
}

uint32_t khrn_image_get_log2_brcm2_height(KHRN_IMAGE_FORMAT_T format)
{
   vcos_assert(khrn_image_is_brcm1(format) || khrn_image_is_brcm2(format));

   switch (format & IMAGE_FORMAT_COMP_MASK) {
   case IMAGE_FORMAT_UNCOMP:
   {
      switch (format & IMAGE_FORMAT_PIXEL_SIZE_MASK) {
      case IMAGE_FORMAT_1:  return 3;
      case IMAGE_FORMAT_4:  return 3;
      case IMAGE_FORMAT_8:  return 3;
      case IMAGE_FORMAT_16: return 2;
      case IMAGE_FORMAT_32: return 2;
      default:              UNREACHABLE(); return 0;
      }
   }
   case IMAGE_FORMAT_ETC1: return 4;
   case IMAGE_FORMAT_YUYV: return 2;
   default:                UNREACHABLE(); return 0;
   }
}

uint32_t khrn_image_get_log2_brcm1_width(KHRN_IMAGE_FORMAT_T format)
{
   return khrn_image_get_log2_brcm2_width(format) + 3;
}

uint32_t khrn_image_get_log2_brcm1_height(KHRN_IMAGE_FORMAT_T format)
{
   return khrn_image_get_log2_brcm2_height(format) + 3;
}

/******************************************************************************
image handling
******************************************************************************/

uint32_t khrn_image_pad_width(KHRN_IMAGE_FORMAT_T format, uint32_t width)
{
   vcos_assert(format != IMAGE_FORMAT_INVALID);

   switch (format & IMAGE_FORMAT_MEM_LAYOUT_MASK) {
   case IMAGE_FORMAT_RSO:
      vcos_assert(!(khrn_image_get_bpp(format) & 7));
      return width;
   case IMAGE_FORMAT_BRCM1:  return round_up(width, (uint32_t) (1 << khrn_image_get_log2_brcm1_width(format)));
   case IMAGE_FORMAT_BRCM2:  return round_up(width, (uint32_t) (1 << khrn_image_get_log2_brcm2_width(format)));
   case IMAGE_FORMAT_BRCM4: return round_up(width, 64);
   default:               UNREACHABLE(); return 0;
   }
}

uint32_t khrn_image_pad_height(KHRN_IMAGE_FORMAT_T format, uint32_t height)
{
   vcos_assert(format != IMAGE_FORMAT_INVALID);

   switch (format & IMAGE_FORMAT_MEM_LAYOUT_MASK) {
   case IMAGE_FORMAT_RSO: return height;
   case IMAGE_FORMAT_BRCM1:  return round_up(height, (uint32_t) (1 << khrn_image_get_log2_brcm1_height(format)));
   case IMAGE_FORMAT_BRCM2:  return round_up(height, (uint32_t) (1 << khrn_image_get_log2_brcm2_height(format)));
   case IMAGE_FORMAT_BRCM4: return round_up(height, 64);
   default:               UNREACHABLE(); return 0;
   }
}

uint32_t khrn_image_get_stride(KHRN_IMAGE_FORMAT_T format, uint32_t width)
{
   return (khrn_image_pad_width(format, width) * khrn_image_get_bpp(format)) >> 3;
}

uint32_t khrn_image_get_size(KHRN_IMAGE_FORMAT_T format, uint32_t width, uint32_t height)
{
   uint32_t size = khrn_image_get_stride(format, width) * khrn_image_pad_height(format, height);
#ifdef WORKAROUND_HW2038
   if (khrn_image_is_brcm1(format) || khrn_image_is_brcm2(format)) {
      uint32_t log2_brcm2_width = khrn_image_get_log2_brcm2_width(format);
      uint32_t log2_brcm2_height = khrn_image_get_log2_brcm2_height(format);
      uint32_t width_in_brcm2s = (width + ((1 << log2_brcm2_width) - 1)) >> log2_brcm2_width;
      uint32_t height_in_brcm2s = (height + ((1 << log2_brcm2_height) - 1)) >> log2_brcm2_height;
      uint32_t hw2038_size =
         ((((height_in_brcm2s - 1) >> 3) << 7) +
         ((width_in_brcm2s - 1) >> 3) + 1) << 6;
      size = _max(size, hw2038_size);
   } /* else: can't bind it as a texture */
#endif
   return size;
}

void khrn_image_wrap(KHRN_IMAGE_WRAP_T *wrap, KHRN_IMAGE_FORMAT_T format, uint32_t width, uint32_t height, int32_t stride, void *storage)
{
   wrap->format = format;
   wrap->width = (uint16_t)width;
   wrap->height = (uint16_t)height;
   wrap->stride = stride;
   wrap->aux = NULL;
   wrap->storage = storage;
}
