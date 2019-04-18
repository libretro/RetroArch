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

// Common image types used by the vc_image library

#ifndef INTERFACE_VC_IMAGE_TYPES_H
#define INTERFACE_VC_IMAGE_TYPES_H

/* This file gets included by the VCE compiler, which gets confused
 * easily by the VCOS headers. So cannot include vcos.h here.
 */
#include "interface/vcos/vcos_stdint.h"

/* We have so many rectangle types; let's try to introduce a common one. */
typedef struct tag_VC_RECT_T {
   int32_t x;
   int32_t y;
   int32_t width;
   int32_t height;
} VC_RECT_T;

struct VC_IMAGE_T;
typedef struct VC_IMAGE_T VC_IMAGE_T;

/* Types of image supported. */
/* Please add any new types to the *end* of this list.  Also update
 * case_VC_IMAGE_ANY_xxx macros (below), and the vc_image_type_info table in
 * vc_image/vc_image_helper.c.
 */
typedef enum
{
   VC_IMAGE_MIN = 0, //bounds for error checking

   VC_IMAGE_RGB565 = 1,
   VC_IMAGE_1BPP,
   VC_IMAGE_YUV420,
   VC_IMAGE_48BPP,
   VC_IMAGE_RGB888,
   VC_IMAGE_8BPP,
   VC_IMAGE_4BPP,    // 4bpp palettised image
   VC_IMAGE_3D32,    /* A separated format of 16 colour/light shorts followed by 16 z values */
   VC_IMAGE_3D32B,   /* 16 colours followed by 16 z values */
   VC_IMAGE_3D32MAT, /* A separated format of 16 material/colour/light shorts followed by 16 z values */
   VC_IMAGE_RGB2X9,   /* 32 bit format containing 18 bits of 6.6.6 RGB, 9 bits per short */
   VC_IMAGE_RGB666,   /* 32-bit format holding 18 bits of 6.6.6 RGB */
   VC_IMAGE_PAL4_OBSOLETE,     // 4bpp palettised image with embedded palette
   VC_IMAGE_PAL8_OBSOLETE,     // 8bpp palettised image with embedded palette
   VC_IMAGE_RGBA32,   /* RGB888 with an alpha byte after each pixel */ /* xxx: isn't it BEFORE each pixel? */
   VC_IMAGE_YUV422,   /* a line of Y (32-byte padded), a line of U (16-byte padded), and a line of V (16-byte padded) */
   VC_IMAGE_RGBA565,  /* RGB565 with a transparent patch */
   VC_IMAGE_RGBA16,   /* Compressed (4444) version of RGBA32 */
   VC_IMAGE_YUV_UV,   /* VCIII codec format */
   VC_IMAGE_TF_RGBA32, /* VCIII T-format RGBA8888 */
   VC_IMAGE_TF_RGBX32,  /* VCIII T-format RGBx8888 */
   VC_IMAGE_TF_FLOAT, /* VCIII T-format float */
   VC_IMAGE_TF_RGBA16, /* VCIII T-format RGBA4444 */
   VC_IMAGE_TF_RGBA5551, /* VCIII T-format RGB5551 */
   VC_IMAGE_TF_RGB565, /* VCIII T-format RGB565 */
   VC_IMAGE_TF_YA88, /* VCIII T-format 8-bit luma and 8-bit alpha */
   VC_IMAGE_TF_BYTE, /* VCIII T-format 8 bit generic sample */
   VC_IMAGE_TF_PAL8, /* VCIII T-format 8-bit palette */
   VC_IMAGE_TF_PAL4, /* VCIII T-format 4-bit palette */
   VC_IMAGE_TF_ETC1, /* VCIII T-format Ericsson Texture Compressed */
   VC_IMAGE_BGR888,  /* RGB888 with R & B swapped */
   VC_IMAGE_BGR888_NP,  /* RGB888 with R & B swapped, but with no pitch, i.e. no padding after each row of pixels */
   VC_IMAGE_BAYER,  /* Bayer image, extra defines which variant is being used */
   VC_IMAGE_CODEC,  /* General wrapper for codec images e.g. JPEG from camera */
   VC_IMAGE_YUV_UV32,   /* VCIII codec format */
   VC_IMAGE_TF_Y8,   /* VCIII T-format 8-bit luma */
   VC_IMAGE_TF_A8,   /* VCIII T-format 8-bit alpha */
   VC_IMAGE_TF_SHORT,/* VCIII T-format 16-bit generic sample */
   VC_IMAGE_TF_1BPP, /* VCIII T-format 1bpp black/white */
   VC_IMAGE_OPENGL,
   VC_IMAGE_YUV444I, /* VCIII-B0 HVS YUV 4:4:4 interleaved samples */
   VC_IMAGE_YUV422PLANAR,  /* Y, U, & V planes separately (VC_IMAGE_YUV422 has them interleaved on a per line basis) */
   VC_IMAGE_ARGB8888,   /* 32bpp with 8bit alpha at MS byte, with R, G, B (LS byte) */
   VC_IMAGE_XRGB8888,   /* 32bpp with 8bit unused at MS byte, with R, G, B (LS byte) */

   VC_IMAGE_YUV422YUYV,  /* interleaved 8 bit samples of Y, U, Y, V */
   VC_IMAGE_YUV422YVYU,  /* interleaved 8 bit samples of Y, V, Y, U */
   VC_IMAGE_YUV422UYVY,  /* interleaved 8 bit samples of U, Y, V, Y */
   VC_IMAGE_YUV422VYUY,  /* interleaved 8 bit samples of V, Y, U, Y */

   VC_IMAGE_RGBX32,      /* 32bpp like RGBA32 but with unused alpha */
   VC_IMAGE_RGBX8888,    /* 32bpp, corresponding to RGBA with unused alpha */
   VC_IMAGE_BGRX8888,    /* 32bpp, corresponding to BGRA with unused alpha */

   VC_IMAGE_YUV420SP,    /* Y as a plane, then UV byte interleaved in plane with with same pitch, half height */

   VC_IMAGE_YUV444PLANAR,  /* Y, U, & V planes separately 4:4:4 */

   VC_IMAGE_TF_U8,   /* T-format 8-bit U - same as TF_Y8 buf from U plane */
   VC_IMAGE_TF_V8,   /* T-format 8-bit U - same as TF_Y8 buf from V plane */

   VC_IMAGE_YUV420_16,  /* YUV4:2:0 planar, 16bit values */
   VC_IMAGE_YUV_UV_16,  /* YUV4:2:0 codec format, 16bit values */
   VC_IMAGE_YUV420_S,   /* YUV4:2:0 with U,V in side-by-side format */

   VC_IMAGE_MAX,     //bounds for error checking
   VC_IMAGE_FORCE_ENUM_16BIT = 0xffff,
} VC_IMAGE_TYPE_T;

/* Image transformations (flips and 90 degree rotations).
   These are made out of 3 primitives (transpose is done first).
   These must match the DISPMAN and Media Player definitions. */

#define TRANSFORM_HFLIP     (1<<0)
#define TRANSFORM_VFLIP     (1<<1)
#define TRANSFORM_TRANSPOSE (1<<2)

typedef enum {
   VC_IMAGE_ROT0           = 0,
   VC_IMAGE_MIRROR_ROT0    = TRANSFORM_HFLIP,
   VC_IMAGE_MIRROR_ROT180  = TRANSFORM_VFLIP,
   VC_IMAGE_ROT180         = TRANSFORM_HFLIP|TRANSFORM_VFLIP,
   VC_IMAGE_MIRROR_ROT90   = TRANSFORM_TRANSPOSE,
   VC_IMAGE_ROT270         = TRANSFORM_TRANSPOSE|TRANSFORM_HFLIP,
   VC_IMAGE_ROT90          = TRANSFORM_TRANSPOSE|TRANSFORM_VFLIP,
   VC_IMAGE_MIRROR_ROT270  = TRANSFORM_TRANSPOSE|TRANSFORM_HFLIP|TRANSFORM_VFLIP,
} VC_IMAGE_TRANSFORM_T;

typedef enum
{ //defined to be identical to register bits
   VC_IMAGE_BAYER_RGGB     = 0,
   VC_IMAGE_BAYER_GBRG     = 1,
   VC_IMAGE_BAYER_BGGR     = 2,
   VC_IMAGE_BAYER_GRBG     = 3
} VC_IMAGE_BAYER_ORDER_T;

typedef enum
{ //defined to be identical to register bits
   VC_IMAGE_BAYER_RAW6     = 0,
   VC_IMAGE_BAYER_RAW7     = 1,
   VC_IMAGE_BAYER_RAW8     = 2,
   VC_IMAGE_BAYER_RAW10    = 3,
   VC_IMAGE_BAYER_RAW12    = 4,
   VC_IMAGE_BAYER_RAW14    = 5,
   VC_IMAGE_BAYER_RAW16    = 6,
   VC_IMAGE_BAYER_RAW10_8  = 7,
   VC_IMAGE_BAYER_RAW12_8  = 8,
   VC_IMAGE_BAYER_RAW14_8  = 9,
   VC_IMAGE_BAYER_RAW10L   = 11,
   VC_IMAGE_BAYER_RAW12L   = 12,
   VC_IMAGE_BAYER_RAW14L   = 13,
   VC_IMAGE_BAYER_RAW16_BIG_ENDIAN = 14,
   VC_IMAGE_BAYER_RAW4    = 15,
} VC_IMAGE_BAYER_FORMAT_T;

#endif /* __VC_INCLUDE_IMAGE_TYPES_H__ */
