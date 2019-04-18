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

#ifndef MMAL_ENCODINGS_H
#define MMAL_ENCODINGS_H

#include "mmal_common.h"

/** \defgroup MmalEncodings List of pre-defined encodings
 * This defines a list of common encodings. This list isn't exhaustive and is only
 * provided as a convenience to avoid clients having to use FourCC codes directly.
 * However components are allowed to define and use their own FourCC codes. */
/* @{ */

/** \name Pre-defined video encodings */
/* @{ */
#define MMAL_ENCODING_H264             MMAL_FOURCC('H','2','6','4')
#define MMAL_ENCODING_MVC              MMAL_FOURCC('M','V','C',' ')
#define MMAL_ENCODING_H263             MMAL_FOURCC('H','2','6','3')
#define MMAL_ENCODING_MP4V             MMAL_FOURCC('M','P','4','V')
#define MMAL_ENCODING_MP2V             MMAL_FOURCC('M','P','2','V')
#define MMAL_ENCODING_MP1V             MMAL_FOURCC('M','P','1','V')
#define MMAL_ENCODING_WMV3             MMAL_FOURCC('W','M','V','3')
#define MMAL_ENCODING_WMV2             MMAL_FOURCC('W','M','V','2')
#define MMAL_ENCODING_WMV1             MMAL_FOURCC('W','M','V','1')
#define MMAL_ENCODING_WVC1             MMAL_FOURCC('W','V','C','1')
#define MMAL_ENCODING_VP8              MMAL_FOURCC('V','P','8',' ')
#define MMAL_ENCODING_VP7              MMAL_FOURCC('V','P','7',' ')
#define MMAL_ENCODING_VP6              MMAL_FOURCC('V','P','6',' ')
#define MMAL_ENCODING_THEORA           MMAL_FOURCC('T','H','E','O')
#define MMAL_ENCODING_SPARK            MMAL_FOURCC('S','P','R','K')
#define MMAL_ENCODING_MJPEG            MMAL_FOURCC('M','J','P','G')

#define MMAL_ENCODING_JPEG             MMAL_FOURCC('J','P','E','G')
#define MMAL_ENCODING_GIF              MMAL_FOURCC('G','I','F',' ')
#define MMAL_ENCODING_PNG              MMAL_FOURCC('P','N','G',' ')
#define MMAL_ENCODING_PPM              MMAL_FOURCC('P','P','M',' ')
#define MMAL_ENCODING_TGA              MMAL_FOURCC('T','G','A',' ')
#define MMAL_ENCODING_BMP              MMAL_FOURCC('B','M','P',' ')

#define MMAL_ENCODING_I420             MMAL_FOURCC('I','4','2','0')
#define MMAL_ENCODING_I420_SLICE       MMAL_FOURCC('S','4','2','0')
#define MMAL_ENCODING_YV12             MMAL_FOURCC('Y','V','1','2')
#define MMAL_ENCODING_I422             MMAL_FOURCC('I','4','2','2')
#define MMAL_ENCODING_I422_SLICE       MMAL_FOURCC('S','4','2','2')
#define MMAL_ENCODING_YUYV             MMAL_FOURCC('Y','U','Y','V')
#define MMAL_ENCODING_YVYU             MMAL_FOURCC('Y','V','Y','U')
#define MMAL_ENCODING_UYVY             MMAL_FOURCC('U','Y','V','Y')
#define MMAL_ENCODING_VYUY             MMAL_FOURCC('V','Y','U','Y')
#define MMAL_ENCODING_NV12             MMAL_FOURCC('N','V','1','2')
#define MMAL_ENCODING_NV21             MMAL_FOURCC('N','V','2','1')
#define MMAL_ENCODING_ARGB             MMAL_FOURCC('A','R','G','B')
#define MMAL_ENCODING_ARGB_SLICE       MMAL_FOURCC('a','r','g','b')
#define MMAL_ENCODING_RGBA             MMAL_FOURCC('R','G','B','A')
#define MMAL_ENCODING_RGBA_SLICE       MMAL_FOURCC('r','g','b','a')
#define MMAL_ENCODING_ABGR             MMAL_FOURCC('A','B','G','R')
#define MMAL_ENCODING_ABGR_SLICE       MMAL_FOURCC('a','b','g','r')
#define MMAL_ENCODING_BGRA             MMAL_FOURCC('B','G','R','A')
#define MMAL_ENCODING_BGRA_SLICE       MMAL_FOURCC('b','g','r','a')
#define MMAL_ENCODING_RGB16            MMAL_FOURCC('R','G','B','2')
#define MMAL_ENCODING_RGB16_SLICE      MMAL_FOURCC('r','g','b','2')
#define MMAL_ENCODING_RGB24            MMAL_FOURCC('R','G','B','3')
#define MMAL_ENCODING_RGB24_SLICE      MMAL_FOURCC('r','g','b','3')
#define MMAL_ENCODING_RGB32            MMAL_FOURCC('R','G','B','4')
#define MMAL_ENCODING_RGB32_SLICE      MMAL_FOURCC('r','g','b','4')
#define MMAL_ENCODING_BGR16            MMAL_FOURCC('B','G','R','2')
#define MMAL_ENCODING_BGR16_SLICE      MMAL_FOURCC('b','g','r','2')
#define MMAL_ENCODING_BGR24            MMAL_FOURCC('B','G','R','3')
#define MMAL_ENCODING_BGR24_SLICE      MMAL_FOURCC('b','g','r','3')
#define MMAL_ENCODING_BGR32            MMAL_FOURCC('B','G','R','4')
#define MMAL_ENCODING_BGR32_SLICE      MMAL_FOURCC('b','g','r','4')

/** YUV 4:2:0 planar, 16bit/component.
*/
#define MMAL_ENCODING_I420_16          MMAL_FOURCC('i','4','2','0')
/** YUV 4:2:0 planar, 10bit/component as least sig 10bits of 16 bit words.
*/
#define MMAL_ENCODING_I420_10          MMAL_FOURCC('i','4','1','0')

/** YUV 4:2:0 planar but with U and V in side-by-side format
 *   So U and V have same pitch as Y, but V = U + pitch/2
*/
#define MMAL_ENCODING_I420_S           MMAL_FOURCC('I','4','2','S')

//Bayer formats
//FourCC values copied from V4L2 where defined.
//10 bit per pixel packed Bayer formats.
#define MMAL_ENCODING_BAYER_SBGGR10P   MMAL_FOURCC('p','B','A','A')  //BGGR
#define MMAL_ENCODING_BAYER_SGRBG10P   MMAL_FOURCC('p','g','A','A')  //GRBG
#define MMAL_ENCODING_BAYER_SGBRG10P   MMAL_FOURCC('p','G','A','A')  //GBRG
#define MMAL_ENCODING_BAYER_SRGGB10P   MMAL_FOURCC('p','R','A','A')  //RGGB

//8 bit per pixel Bayer formats.
#define MMAL_ENCODING_BAYER_SBGGR8     MMAL_FOURCC('B','A','8','1')  //BGGR
#define MMAL_ENCODING_BAYER_SGBRG8     MMAL_FOURCC('G','B','R','G')  //GBRG
#define MMAL_ENCODING_BAYER_SGRBG8     MMAL_FOURCC('G','R','B','G')  //GRBG
#define MMAL_ENCODING_BAYER_SRGGB8     MMAL_FOURCC('R','G','G','B')  //RGGB

//12 bit per pixel Bayer formats - not defined in V4L2, only 12bit expanded to 16.
//Copy 10bpp packed 4CC pattern
#define MMAL_ENCODING_BAYER_SBGGR12P   MMAL_FOURCC('p','B','1','2')  //BGGR
#define MMAL_ENCODING_BAYER_SGRBG12P   MMAL_FOURCC('p','g','1','2')  //GRBG
#define MMAL_ENCODING_BAYER_SGBRG12P   MMAL_FOURCC('p','G','1','2')  //GBRG
#define MMAL_ENCODING_BAYER_SRGGB12P   MMAL_FOURCC('p','R','1','2')  //RGGB

//16 bit per pixel Bayer formats.
#define MMAL_ENCODING_BAYER_SBGGR16    MMAL_FOURCC('B','G','1','6')  //BGGR
#define MMAL_ENCODING_BAYER_SGBRG16    MMAL_FOURCC('G','B','1','6')  //GBRG
#define MMAL_ENCODING_BAYER_SGRBG16    MMAL_FOURCC('G','R','1','6')  //GRBG
#define MMAL_ENCODING_BAYER_SRGGB16    MMAL_FOURCC('R','G','1','6')  //RGGB

//10 bit per pixel DPCM compressed to 8bits Bayer formats.
#define MMAL_ENCODING_BAYER_SBGGR10DPCM8 MMAL_FOURCC('b','B','A','8')  //BGGR
#define MMAL_ENCODING_BAYER_SGBRG10DPCM8 MMAL_FOURCC('b','G','A','8')  //GBRG
#define MMAL_ENCODING_BAYER_SGRBG10DPCM8 MMAL_FOURCC('B','D','1','0')  //GRBG
#define MMAL_ENCODING_BAYER_SRGGB10DPCM8 MMAL_FOURCC('b','R','A','8')  //RGGB

/** SAND Video (YUVUV128) format, native format understood by VideoCore.
 * This format is *not* opaque - if requested you will receive full frames
 * of YUV_UV video.
 */
#define MMAL_ENCODING_YUVUV128         MMAL_FOURCC('S','A','N','D')
/** 16 bit SAND Video (YUVUV64_16) format.
 * This format is *not* opaque - if requested you will receive full frames
 * of YUV_UV_16 video.
 */
#define MMAL_ENCODING_YUVUV64_16      MMAL_FOURCC('S','A','1','6')
/** 10 bit SAND Video format, packed as least sig 10 bits of 16 bit words.
 */
#define MMAL_ENCODING_YUVUV64_10      MMAL_FOURCC('S','A','1','0')

/** VideoCore opaque image format, image handles are returned to
 * the host but not the actual image data.
 */
#define MMAL_ENCODING_OPAQUE           MMAL_FOURCC('O','P','Q','V')

/** An EGL image handle
 */
#define MMAL_ENCODING_EGL_IMAGE        MMAL_FOURCC('E','G','L','I')

/* }@ */

/** \name Pre-defined audio encodings */
/* @{ */
#define MMAL_ENCODING_PCM_UNSIGNED_BE  MMAL_FOURCC('P','C','M','U')
#define MMAL_ENCODING_PCM_UNSIGNED_LE  MMAL_FOURCC('p','c','m','u')
#define MMAL_ENCODING_PCM_SIGNED_BE    MMAL_FOURCC('P','C','M','S')
#define MMAL_ENCODING_PCM_SIGNED_LE    MMAL_FOURCC('p','c','m','s')
#define MMAL_ENCODING_PCM_FLOAT_BE     MMAL_FOURCC('P','C','M','F')
#define MMAL_ENCODING_PCM_FLOAT_LE     MMAL_FOURCC('p','c','m','f')
/* Defines for native endianness */
#ifdef MMAL_IS_BIG_ENDIAN
#define MMAL_ENCODING_PCM_UNSIGNED     MMAL_ENCODING_PCM_UNSIGNED_BE
#define MMAL_ENCODING_PCM_SIGNED       MMAL_ENCODING_PCM_SIGNED_BE
#define MMAL_ENCODING_PCM_FLOAT        MMAL_ENCODING_PCM_FLOAT_BE
#else
#define MMAL_ENCODING_PCM_UNSIGNED     MMAL_ENCODING_PCM_UNSIGNED_LE
#define MMAL_ENCODING_PCM_SIGNED       MMAL_ENCODING_PCM_SIGNED_LE
#define MMAL_ENCODING_PCM_FLOAT        MMAL_ENCODING_PCM_FLOAT_LE
#endif

#define MMAL_ENCODING_MP4A             MMAL_FOURCC('M','P','4','A')
#define MMAL_ENCODING_MPGA             MMAL_FOURCC('M','P','G','A')
#define MMAL_ENCODING_ALAW             MMAL_FOURCC('A','L','A','W')
#define MMAL_ENCODING_MULAW            MMAL_FOURCC('U','L','A','W')
#define MMAL_ENCODING_ADPCM_MS         MMAL_FOURCC('M','S',0x0,0x2)
#define MMAL_ENCODING_ADPCM_IMA_MS     MMAL_FOURCC('M','S',0x0,0x1)
#define MMAL_ENCODING_ADPCM_SWF        MMAL_FOURCC('A','S','W','F')
#define MMAL_ENCODING_WMA1             MMAL_FOURCC('W','M','A','1')
#define MMAL_ENCODING_WMA2             MMAL_FOURCC('W','M','A','2')
#define MMAL_ENCODING_WMAP             MMAL_FOURCC('W','M','A','P')
#define MMAL_ENCODING_WMAL             MMAL_FOURCC('W','M','A','L')
#define MMAL_ENCODING_WMAV             MMAL_FOURCC('W','M','A','V')
#define MMAL_ENCODING_AMRNB            MMAL_FOURCC('A','M','R','N')
#define MMAL_ENCODING_AMRWB            MMAL_FOURCC('A','M','R','W')
#define MMAL_ENCODING_AMRWBP           MMAL_FOURCC('A','M','R','P')
#define MMAL_ENCODING_AC3              MMAL_FOURCC('A','C','3',' ')
#define MMAL_ENCODING_EAC3             MMAL_FOURCC('E','A','C','3')
#define MMAL_ENCODING_DTS              MMAL_FOURCC('D','T','S',' ')
#define MMAL_ENCODING_MLP              MMAL_FOURCC('M','L','P',' ')
#define MMAL_ENCODING_FLAC             MMAL_FOURCC('F','L','A','C')
#define MMAL_ENCODING_VORBIS           MMAL_FOURCC('V','O','R','B')
#define MMAL_ENCODING_SPEEX            MMAL_FOURCC('S','P','X',' ')
#define MMAL_ENCODING_ATRAC3           MMAL_FOURCC('A','T','R','3')
#define MMAL_ENCODING_ATRACX           MMAL_FOURCC('A','T','R','X')
#define MMAL_ENCODING_ATRACL           MMAL_FOURCC('A','T','R','L')
#define MMAL_ENCODING_MIDI             MMAL_FOURCC('M','I','D','I')
#define MMAL_ENCODING_EVRC             MMAL_FOURCC('E','V','R','C')
#define MMAL_ENCODING_NELLYMOSER       MMAL_FOURCC('N','E','L','Y')
#define MMAL_ENCODING_QCELP            MMAL_FOURCC('Q','C','E','L')
#define MMAL_ENCODING_MP4V_DIVX_DRM    MMAL_FOURCC('M','4','V','D')
/* @} */

/* @} MmalEncodings List */

/** \defgroup MmalEncodingVariants List of pre-defined encoding variants
 * This defines a list of common encoding variants. This list isn't exhaustive and is only
 * provided as a convenience to avoid clients having to use FourCC codes directly.
 * However components are allowed to define and use their own FourCC codes. */
/* @{ */

/** \name Pre-defined H264 encoding variants */
/* @{ */
/** ISO 14496-10 Annex B byte stream format */
#define MMAL_ENCODING_VARIANT_H264_DEFAULT   0
/** ISO 14496-15 AVC stream format */
#define MMAL_ENCODING_VARIANT_H264_AVC1      MMAL_FOURCC('A','V','C','1')
/** Implicitly delineated NAL units without emulation prevention */
#define MMAL_ENCODING_VARIANT_H264_RAW       MMAL_FOURCC('R','A','W',' ')
/* @} */

/** \name Pre-defined MPEG4 audio encoding variants */
/* @{ */
/** Raw stream format */
#define MMAL_ENCODING_VARIANT_MP4A_DEFAULT   0
/** ADTS stream format */
#define MMAL_ENCODING_VARIANT_MP4A_ADTS      MMAL_FOURCC('A','D','T','S')
/* @} */

/* @} MmalEncodingVariants List */

/** \defgroup MmalColorSpace List of pre-defined video color spaces
 * This defines a list of common color spaces. This list isn't exhaustive and is only
 * provided as a convenience to avoid clients having to use FourCC codes directly.
 * However components are allowed to define and use their own FourCC codes. */
/* @{ */

/** Unknown color space */
#define MMAL_COLOR_SPACE_UNKNOWN       0
/** ITU-R BT.601-5 [SDTV] */
#define MMAL_COLOR_SPACE_ITUR_BT601    MMAL_FOURCC('Y','6','0','1')
/** ITU-R BT.709-3 [HDTV] */
#define MMAL_COLOR_SPACE_ITUR_BT709    MMAL_FOURCC('Y','7','0','9')
/** JPEG JFIF */
#define MMAL_COLOR_SPACE_JPEG_JFIF     MMAL_FOURCC('Y','J','F','I')
/** Title 47 Code of Federal Regulations (2003) 73.682 (a) (20) */
#define MMAL_COLOR_SPACE_FCC           MMAL_FOURCC('Y','F','C','C')
/** Society of Motion Picture and Television Engineers 240M (1999) */
#define MMAL_COLOR_SPACE_SMPTE240M     MMAL_FOURCC('Y','2','4','0')
/** ITU-R BT.470-2 System M */
#define MMAL_COLOR_SPACE_BT470_2_M     MMAL_FOURCC('Y','_','_','M')
/** ITU-R BT.470-2 System BG */
#define MMAL_COLOR_SPACE_BT470_2_BG    MMAL_FOURCC('Y','_','B','G')
/** JPEG JFIF, but with 16..255 luma */
#define MMAL_COLOR_SPACE_JFIF_Y16_255  MMAL_FOURCC('Y','Y','1','6')
/* @} MmalColorSpace List */

#endif /* MMAL_ENCODINGS_H */
