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

#ifndef INTERFACE_VC_IMAGE_STRUCTS_H
#define INTERFACE_VC_IMAGE_STRUCTS_H

/* This file gets included by the VCE compiler, which gets confused
 * easily by the VCOS headers. So cannot include vcos.h here.
 */
#include "interface/vcos/vcos_stdint.h"
#include "interface/vcos/vcos_attr.h"

#include "helpers/debug_utils/debug_writer.h"

#include "interface/vctypes/vc_image_types.h"

   /* Format specific infos for vc images */

   /* YUV information, co-sited h/v flags & colour space words */
   typedef enum {
      VC_IMAGE_YUVINFO_UNSPECIFIED    = 0,   /* Unknown or unset - defaults to BT601 interstitial */

      /* colour-space conversions data [4 bits] */
      /* Note that colour conversions for SMPTE 170M are identical to BT.601 */
      VC_IMAGE_YUVINFO_CSC_ITUR_BT601      = 1,   /* ITU-R BT.601-5 [SDTV] (compatible with VideoCore-II) */
      VC_IMAGE_YUVINFO_CSC_ITUR_BT709      = 2,   /* ITU-R BT.709-3 [HDTV] */
      VC_IMAGE_YUVINFO_CSC_JPEG_JFIF       = 3,   /* JPEG JFIF */
      VC_IMAGE_YUVINFO_CSC_FCC             = 4,   /* Title 47 Code of Federal Regulations (2003) 73.682 (a) (20) */
      VC_IMAGE_YUVINFO_CSC_SMPTE_240M      = 5,   /* Society of Motion Picture and Television Engineers 240M (1999) */
      VC_IMAGE_YUVINFO_CSC_ITUR_BT470_2_M  = 6,  /* ITU-R BT.470-2 System M */
      VC_IMAGE_YUVINFO_CSC_ITUR_BT470_2_BG = 7,  /* ITU-R BT.470-2 System B,G */
      VC_IMAGE_YUVINFO_CSC_JPEG_JFIF_Y16_255 = 8, /* JPEG JFIF, but with 16..255 luma */
      VC_IMAGE_YUVINFO_CSC_CUSTOM          = 15,  /* Custom colour matrix follows header */
      VC_IMAGE_YUVINFO_CSC_SMPTE_170M      = VC_IMAGE_YUVINFO_CSC_ITUR_BT601,

      /* co-sited flags, assumed interstitial if not co-sited [2 bits] */
      VC_IMAGE_YUVINFO_H_COSITED      = 256,
      VC_IMAGE_YUVINFO_V_COSITED      = 512,

      VC_IMAGE_YUVINFO_TOP_BOTTOM     = 1024,
      VC_IMAGE_YUVINFO_DECIMATED      = 2048,
      VC_IMAGE_YUVINFO_PACKED         = 4096,

      /* Certain YUV image formats can either be V/U interleaved or U/V interleaved */
      VC_IMAGE_YUVINFO_IS_VU          = 0x8000,

      /* Force Metaware to use 16 bits */
      VC_IMAGE_YUVINFO_FORCE_ENUM_16BIT = 0xffff,
   } VC_IMAGE_YUVINFO_T;

#define VC_IMAGE_YUV_UV_STRIPE_WIDTH_LOG2 7
#define VC_IMAGE_YUV_UV_STRIPE_WIDTH (1 << VC_IMAGE_YUV_UV_STRIPE_WIDTH_LOG2)

#define VC_IMAGE_YUV_UV32_STRIPE_WIDTH_LOG2 5
#define VC_IMAGE_YUV_UV32_STRIPE_WIDTH (1 << VC_IMAGE_YUV_UV32_STRIPE_WIDTH_LOG2)

/* 64 pixel wide stripes, 128 byte wide as 16bits/component */
#define VC_IMAGE_YUV_UV_16_STRIPE_WIDTH_LOG2 6
#define VC_IMAGE_YUV_UV_16_STRIPE_WIDTH (1 << VC_IMAGE_YUV_UV_16_STRIPE_WIDTH_LOG2)
#define VC_IMAGE_YUV_UV_16_STRIPE_STRIDE_LOG2 7
#define VC_IMAGE_YUV_UV_16_STRIPE_STRIDE (1 << VC_IMAGE_YUV_UV_16_STRIPE_STRIDE_LOG2)

   /* The image structure. */
   typedef struct vc_image_extra_uv_s {
      void *u, *v;
      int vpitch;
   } VC_IMAGE_EXTRA_UV_T;

   typedef struct vc_image_extra_rgba_s {
      unsigned component_order   : 24, /* diagnostic use only */
               normalised_alpha  : 1,
               transparent_colour: 1,
               unused_26_31      : 6;
      unsigned int arg;
      int vpitch;
   } VC_IMAGE_EXTRA_RGBA_T;

   typedef struct vc_image_extra_pal_s {
      short *palette;
      int palette32 : 1;
   } VC_IMAGE_EXTRA_PAL_T;

// These fields are subject to change / being moved around
   typedef struct vc_image_extra_tf_s {
signed int mipmap_levels        : 8;
unsigned int xxx                : 23;
unsigned int cube_map           : 1;
      void *palette;
   } VC_IMAGE_EXTRA_TF_T;

   typedef struct vc_image_extra_bayer_s {
      unsigned short order;
      unsigned short format;
      int block_length;
   } VC_IMAGE_EXTRA_BAYER_T;

//The next block can be used with Visual C++
//which treats enums as long ints
   typedef struct vc_image_extra_msbayer_s {
      unsigned char order;
      unsigned char format;
      unsigned char dummy1;
      unsigned char dummy2;
      int block_length;
   } VC_IMAGE_EXTRA_MSBAYER_T;

   typedef struct vc_image_extra_codec_s {
      int fourcc;
      int maxsize;  //NB this will be copied to image.size in parmalloc()
   } VC_IMAGE_EXTRA_CODEC_T;

#define VC_IMAGE_OPENGL_RGBA32    0x14011908       //GL_UNSIGNED_BYTE GL_RGBA
#define VC_IMAGE_OPENGL_RGB24     0x14011907       //GL_UNSIGNED_BYTE GL_RGB
#define VC_IMAGE_OPENGL_RGBA16    0x80331908       //GL_UNSIGNED_SHORT_4_4_4_4 GL_RGBA
#define VC_IMAGE_OPENGL_RGBA5551  0x80341908       //GL_UNSIGNED_SHORT_5_5_5_1 GL_RGBA
#define VC_IMAGE_OPENGL_RGB565    0x83631907       //GL_UNSIGNED_SHORT_5_6_5 GL_RGB
#define VC_IMAGE_OPENGL_YA88      0x1401190A       //GL_UNSIGNED_BYTE GL_LUMINANCE_ALPHA
#define VC_IMAGE_OPENGL_Y8        0x14011909       //GL_UNSIGNED_BYTE GL_LUMINANCE
#define VC_IMAGE_OPENGL_A8        0x14011906       //GL_UNSIGNED_BYTE GL_ALPHA
#define VC_IMAGE_OPENGL_ETC1      0x8D64           //GL_ETC1_RGB8_OES
#define VC_IMAGE_OPENGL_PALETTE4_RGB24             0x8B90   //GL_PALETTE4_RGB8_OES
#define VC_IMAGE_OPENGL_PALETTE4_RGBA32            0x8B91   //GL_PALETTE4_RGBA8_OES
#define VC_IMAGE_OPENGL_PALETTE4_RGB565            0x8B92   //GL_PALETTE4_R5_G6_B5_OES
#define VC_IMAGE_OPENGL_PALETTE4_RGBA16            0x8B93   //GL_PALETTE4_RGBA4_OES
#define VC_IMAGE_OPENGL_PALETTE4_RGB5551           0x8B94   //GL_PALETTE4_RGB5_A1_OES
#define VC_IMAGE_OPENGL_PALETTE8_RGB24             0x8B95   //GL_PALETTE8_RGB8_OES
#define VC_IMAGE_OPENGL_PALETTE8_RGBA32            0x8B96   //GL_PALETTE8_RGBA8_OES
#define VC_IMAGE_OPENGL_PALETTE8_RGB565            0x8B97   //GL_PALETTE8_R5_G6_B5_OES
#define VC_IMAGE_OPENGL_PALETTE8_RGBA16            0x8B98   //GL_PALETTE8_RGBA4_OES
#define VC_IMAGE_OPENGL_PALETTE8_RGB5551           0x8B99   //GL_PALETTE8_RGB5_A1_OES

   typedef struct vc_image_extra_opengl_s {
      unsigned int format_and_type;
      void const *palette;
   } VC_IMAGE_EXTRA_OPENGL_T;

   typedef union {
      VC_IMAGE_EXTRA_UV_T uv;
      VC_IMAGE_EXTRA_RGBA_T rgba;
      VC_IMAGE_EXTRA_PAL_T pal;
      VC_IMAGE_EXTRA_TF_T tf;
      VC_IMAGE_EXTRA_BAYER_T bayer;
      VC_IMAGE_EXTRA_MSBAYER_T msbayer;
      VC_IMAGE_EXTRA_CODEC_T codec;
      VC_IMAGE_EXTRA_OPENGL_T opengl;
   } VC_IMAGE_EXTRA_T;

   /* structure containing various colour meta-data for each format */
   typedef union {
#ifdef __HIGHC__
      VC_IMAGE_YUVINFO_T      yuv;   /* We know Metaware will use 16 bits for this enum, so use the correct type for debug info */
#else
      unsigned short          yuv;   /* Information pertinent to all YUV implementations */
#endif
      unsigned short          info;  /* dummy, force size to min 16 bits */
   } VC_IMAGE_INFO_T;

   /**
    * Image handle object, which must be locked before image data becomes
    * accessible.
    *
    * A handle to an image where the image data does not have a guaranteed
    * storage location.  A call to \c vc_image_lock() must be made to convert
    * this into a \c VC_IMAGE_BUF_T, which guarantees that image data can
    * be accessed safely.
    *
    * This type will also be used in cases where it's unclear whether or not
    * the buffer is already locked, and in legacy code.
    */
   struct VC_IMAGE_T {
#ifdef __HIGHC__
      VC_IMAGE_TYPE_T                 type;           /* Metaware will use 16 bits for this enum
                                                         so use the correct type for debug info */
#else
      unsigned short                  type;           /* should restrict to 16 bits */
#endif
      VC_IMAGE_INFO_T                 info;           /* format-specific info; zero for VC02 behaviour */
      unsigned short                  width;          /* width in pixels */
      unsigned short                  height;         /* height in pixels */
      int                             pitch;          /* pitch of image_data array in bytes */
      int                             size;           /* number of bytes available in image_data array */
      void                           *image_data;     /* pixel data */
      VC_IMAGE_EXTRA_T                extra;          /* extra data like palette pointer */
      struct vc_metadata_header_s    *metadata;       /* metadata header for the image */
      struct opaque_vc_pool_object_s *pool_object;    /* nonNULL if image was allocated from a vc_pool */
      uint32_t                        mem_handle;     /* the mem handle for relocatable memory storage */
      int                             metadata_size;  /* size of metadata of each channel in bytes */
      int                             channel_offset; /* offset of consecutive channels in bytes */
      uint32_t                        video_timestamp;/* 90000 Hz RTP times domain - derived from audio timestamp */
      uint8_t                         num_channels;   /* number of channels (2 for stereo) */
      uint8_t                         current_channel;/* the channel this header is currently pointing to */
      uint8_t                         linked_multichann_flag;/* Indicate the header has the linked-multichannel structure*/
      uint8_t                         is_channel_linked;     /* Track if the above structure is been used to link the header
                                                                into a linked-mulitchannel image */
      uint8_t                         channel_index;         /* index of the channel this header represents while
                                                                it is being linked. */
      uint8_t                         _dummy[3];      /* pad struct to 64 bytes */
   };

# ifdef __COVERITY__
   /* Currently battling with the size of enums when running through static analysis stage */
   typedef int vc_image_t_size_check[(sizeof(VC_IMAGE_T) == 68) * 2 - 1];
# else
   /* compile time assert to ensure size of VC_IMAGE_T is as expected, if the
      compiler kicks out a "negative subscript" message then the size
      of VC_IMAGE_T is *not* 64 bytes, which is a problem ... */
   typedef int vc_image_t_size_check[(sizeof(VC_IMAGE_T) == 64) * 2 - 1];
#endif

/******************************************************************************
 Debugging rules (defined in camera_debug.c)
 *****************************************************************************/
extern DEBUG_WRITE_ENUM_LOOKUP_T vc_image_type_lookup[];
extern DEBUG_WRITE_ENUM_LOOKUP_T vc_image_bayer_order_lookup[];
extern DEBUG_WRITE_ENUM_LOOKUP_T vc_image_bayer_format_lookup[];
extern DEBUG_WRITE_RULE_T vc_image_info_rule[];
extern DEBUG_WRITE_RULE_T vc_image_rule[];

#endif /* __VC_INCLUDE_IMAGE_TYPES_H__ */
