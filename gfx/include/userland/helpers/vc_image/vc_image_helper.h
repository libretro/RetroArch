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

#ifndef VC_IMAGE_HELPER_H
#define VC_IMAGE_HELPER_H

#include "interface/vctypes/vc_image_structs.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief Image buffer object, with image data locked in memory and ready for access.
 *
 * This data type is fully compatible with \c VC_IMAGE_T for backwards
 * compatibility.  New code should use this type where the object refers to
 * a locked image.
 */
typedef VC_IMAGE_T VC_IMAGE_BUF_T;

/* Macros to determine which format a vc_image is */

typedef struct
{
   unsigned bits_per_pixel : 8,
   is_rgb            : 1,
   is_yuv            : 1,
   is_raster_order   : 1,
   is_tformat_order  : 1,
   has_alpha         : 1;
} VC_IMAGE_TYPE_INFO_T;

#define VC_IMAGE_COMPONENT_ORDER(red_lsb, green_lsb, blue_lsb, alpha_lsb) \
            ( (((red_lsb)   & 0x1f) <<  0) \
            | (((green_lsb) & 0x1f) <<  6) \
            | (((blue_lsb)  & 0x1f) << 12) \
            | (((alpha_lsb) & 0x1f) << 18) )
#define VC_IMAGE_RED_OFFSET(component_order)    (((component_order) >>  0) & 0x1f)
#define VC_IMAGE_GREEN_OFFSET(component_order)  (((component_order) >>  6) & 0x1f)
#define VC_IMAGE_BLUE_OFFSET(component_order)   (((component_order) >> 12) & 0x1f)
#define VC_IMAGE_ALPHA_OFFSET(component_order)  (((component_order) >> 18) & 0x1f)

extern const VC_IMAGE_TYPE_INFO_T vc_image_type_info[VC_IMAGE_MAX + 1];
extern const unsigned int vc_image_rgb_component_order[VC_IMAGE_MAX + 1];

#define VC_IMAGE_IS_YUV(type) (vc_image_type_info[type].is_yuv)
#define VC_IMAGE_IS_RGB(type) (vc_image_type_info[type].is_rgb)
#define VC_IMAGE_IS_RASTER(type) (vc_image_type_info[type].is_raster_order)
#define VC_IMAGE_IS_TFORMAT(type) (vc_image_type_info[type].is_tformat_order)
#define VC_IMAGE_BITS_PER_PIXEL(type) (vc_image_type_info[type].bits_per_pixel)
#define VC_IMAGE_HAS_ALPHA(type) (vc_image_type_info[type].has_alpha)

#define case_VC_IMAGE_ANY_YUV \
   case VC_IMAGE_YUV420:      \
   case VC_IMAGE_YUV420SP:    \
   case VC_IMAGE_YUV422:      \
   case VC_IMAGE_YUV_UV:      \
   case VC_IMAGE_YUV_UV32:    \
   case VC_IMAGE_YUV420_S:    \
   case VC_IMAGE_YUV422PLANAR: \
   case VC_IMAGE_YUV444PLANAR: \
   case VC_IMAGE_YUV420_16:   \
   case VC_IMAGE_YUV_UV_16:   \
   case VC_IMAGE_YUV422YUYV:  \
   case VC_IMAGE_YUV422YVYU:  \
   case VC_IMAGE_YUV422UYVY:  \
   case VC_IMAGE_YUV422VYUY

#define case_VC_IMAGE_ANY_RGB \
   case VC_IMAGE_RGB565:      \
   case VC_IMAGE_RGB2X9:      \
   case VC_IMAGE_RGB666:      \
   case VC_IMAGE_RGBA32:      \
   case VC_IMAGE_RGBX32:      \
   case VC_IMAGE_RGBA16:      \
   case VC_IMAGE_RGBA565:     \
   case VC_IMAGE_RGB888:      \
   case VC_IMAGE_TF_RGBA32:   \
   case VC_IMAGE_TF_RGBX32:   \
   case VC_IMAGE_TF_RGBA16:   \
   case VC_IMAGE_TF_RGBA5551: \
   case VC_IMAGE_TF_RGB565:   \
   case VC_IMAGE_BGR888:      \
   case VC_IMAGE_BGR888_NP:   \
   case VC_IMAGE_ARGB8888:    \
   case VC_IMAGE_XRGB8888:    \
   case VC_IMAGE_RGBX8888:    \
   case VC_IMAGE_BGRX8888

#define case_VC_IMAGE_ANY_RGB_NOT_TF \
   case VC_IMAGE_RGB565:      \
   case VC_IMAGE_RGB2X9:      \
   case VC_IMAGE_RGB666:      \
   case VC_IMAGE_RGBA32:      \
   case VC_IMAGE_RGBX32:      \
   case VC_IMAGE_RGBA16:      \
   case VC_IMAGE_RGBA565:     \
   case VC_IMAGE_RGB888:      \
   case VC_IMAGE_BGR888:      \
   case VC_IMAGE_BGR888_NP:   \
   case VC_IMAGE_ARGB8888:    \
   case VC_IMAGE_XRGB8888:    \
   case VC_IMAGE_RGBX8888:    \
   case VC_IMAGE_BGRX8888

#define case_VC_IMAGE_ANY_TFORMAT \
   case VC_IMAGE_TF_RGBA32:   \
   case VC_IMAGE_TF_RGBX32:   \
   case VC_IMAGE_TF_FLOAT:    \
   case VC_IMAGE_TF_RGBA16:   \
   case VC_IMAGE_TF_RGBA5551: \
   case VC_IMAGE_TF_RGB565:   \
   case VC_IMAGE_TF_YA88:     \
   case VC_IMAGE_TF_BYTE:     \
   case VC_IMAGE_TF_PAL8:     \
   case VC_IMAGE_TF_PAL4:     \
   case VC_IMAGE_TF_ETC1:     \
   case VC_IMAGE_TF_Y8:       \
   case VC_IMAGE_TF_A8:       \
   case VC_IMAGE_TF_SHORT:    \
   case VC_IMAGE_TF_1BPP:     \
   case VC_IMAGE_TF_U8:       \
   case VC_IMAGE_TF_V8

/******************************************************************************
General functions.
******************************************************************************/

int vc_image_bits_per_pixel(VC_IMAGE_TYPE_T type);

int calculate_pitch(VC_IMAGE_TYPE_T type, int width, int height, uint8_t num_channels, VC_IMAGE_INFO_T *info, VC_IMAGE_EXTRA_T *extra);

/* Check if an image will use an alternate memory layout, in order to cope with
 * codec limitation. Applies to YUV_UV images taller than 1344 lines. */
int vc_image_is_tall_yuv_uv(VC_IMAGE_TYPE_T type, int height);

/******************************************************************************
Data member access.
******************************************************************************/

/* Set the type of the VC_IMAGE_T. */
void vc_image_set_type(VC_IMAGE_T *image, VC_IMAGE_TYPE_T type);

/* Set the image_data field, noting how big it is. */
void vc_image_set_image_data(VC_IMAGE_BUF_T *image, int size, void *image_data);

/* Set the image data with added u and v pointers */
void vc_image_set_image_data_yuv(VC_IMAGE_BUF_T *image, int size, void *image_y, void *image_u, void *image_v);

/* Set the dimensions of the image. */
void vc_image_set_dimensions(VC_IMAGE_T *image, int width, int height);

/* Check the integrity of a VC_IMAGE_T structure - checks structure values + data ptr */
int vc_image_verify(const VC_IMAGE_T *image);

/* Set the pitch (internal_width) of the image. */
void vc_image_set_pitch(VC_IMAGE_T *image, int pitch);

/* Specify the vertical pitch for YUV planar images */
void vc_image_set_vpitch(VC_IMAGE_T *image, int vpitch);

/* Specify that the YUV image is V/U interleaved. */
void vc_image_set_is_vu(VC_IMAGE_T *image);

/* Return 1 if the YUV image is V/U interleaved, else return 0. */
int vc_image_get_is_vu(const VC_IMAGE_T *image);
int vc_image_info_get_is_vu(const VC_IMAGE_INFO_T *info);

/* Reset the shape of an image */
int vc_image_reshape(VC_IMAGE_T *image, VC_IMAGE_TYPE_T type, int width, int height);

/* Return the space required (in bytes) for an image of this type and dimensions. */
int vc_image_required_size(VC_IMAGE_T *image);

/* Return the space required (in bytes) for an image of this type's palette. */
int vc_image_palette_size (VC_IMAGE_T *image);

/* Return 1 if image is high-definition, else return 0. */
int vc_image_is_high_definition(const VC_IMAGE_T *image);

/* Return true if palette is 32bpp */
int vc_image_palette_is_32bit(VC_IMAGE_T *image);

/* Retrieve Y, U and V pointers from a YUV image. Note that these are macros. */

#define vc_image_get_y(p) ((unsigned char *)((p)->image_data))

// replaced with functions to allow assert - revert to #define when fixed
//#define vc_image_get_u(p) ((unsigned char *)((p)->extra.uv.u))
//#define vc_image_get_v(p) ((unsigned char *)((p)->extra.uv.v))
unsigned char *vc_image_get_u(const VC_IMAGE_BUF_T *image);
unsigned char *vc_image_get_v(const VC_IMAGE_BUF_T *image);

/* Calculate the address of the given pixel coordinate -- the result may point
 * to a word containing data for several pixels (ie., for sub-8bpp and
 * compressed formats).
 */
void *vc_image_pixel_addr(VC_IMAGE_BUF_T *image, int x, int y);
void *vc_image_pixel_addr_mm(VC_IMAGE_BUF_T *image, int x, int y, int miplevel);
void *vc_image_pixel_addr_u(VC_IMAGE_BUF_T *image, int x, int y);
void *vc_image_pixel_addr_v(VC_IMAGE_BUF_T *image, int x, int y);

/* As above, but with (0,0) in the bottom-left corner */
void *vc_image_pixel_addr_gl(VC_IMAGE_BUF_T *image, int x, int y, int miplevel);

#define vc_image_get_y_422(p) vc_image_get_y(p)
#define vc_image_get_u_422(p) vc_image_get_u(p)
#define vc_image_get_v_422(p) vc_image_get_v(p)

#define vc_image_get_y_422planar(p) vc_image_get_y(p)
#define vc_image_get_u_422planar(p) vc_image_get_u(p)
#define vc_image_get_v_422planar(p) vc_image_get_v(p)

/* Mipmap-related functions. Image must be t-format. */

/* Return the pitch of the selected mipmap */
unsigned int vc_image_get_mipmap_pitch(VC_IMAGE_T *image, int miplvl);

/* Return the padded height of the selected mipmap (mipmaps must be padded to a
 * power of 2) */
unsigned int vc_image_get_mipmap_padded_height(VC_IMAGE_T *image, int miplvl);

/* Return the offset, in bytes, of the selected mipmap. */
int vc_image_get_mipmap_offset(VC_IMAGE_T *image, int miplvl);

/* Return whether the selected mipmap is stored in t-format or linear microtile
 * format. */
#define VC_IMAGE_MIPMAP_TFORMAT 0
#define VC_IMAGE_MIPMAP_LINEAR_TILE 1
int vc_image_get_mipmap_type(VC_IMAGE_T const *image, int miplvl);

#ifdef __cplusplus
}
#endif

#endif
