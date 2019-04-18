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
#ifndef VC_CONTAINERS_CODECS_H
#define VC_CONTAINERS_CODECS_H

/** \file containers_codecs.h
 * Codec helpers
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "containers/containers_types.h"

/* Video */
#define VC_CONTAINER_CODEC_MP1V        VC_FOURCC('m','p','1','v')
#define VC_CONTAINER_CODEC_MP2V        VC_FOURCC('m','p','2','v')
#define VC_CONTAINER_CODEC_MP4V        VC_FOURCC('m','p','4','v')
#define VC_CONTAINER_CODEC_DIV3        VC_FOURCC('d','i','v','3')
#define VC_CONTAINER_CODEC_DIV4        VC_FOURCC('d','i','v','4')
#define VC_CONTAINER_CODEC_H263        VC_FOURCC('h','2','6','3')
#define VC_CONTAINER_CODEC_H264        VC_FOURCC('h','2','6','4')
#define VC_CONTAINER_CODEC_MVC         VC_FOURCC('m','v','c',' ')
#define VC_CONTAINER_CODEC_WMV1        VC_FOURCC('w','m','v','1')
#define VC_CONTAINER_CODEC_WMV2        VC_FOURCC('w','m','v','2')
#define VC_CONTAINER_CODEC_WMV3        VC_FOURCC('w','m','v','3')
#define VC_CONTAINER_CODEC_WVC1        VC_FOURCC('w','v','c','1')
#define VC_CONTAINER_CODEC_WMVA        VC_FOURCC('w','m','v','a')
#define VC_CONTAINER_CODEC_MJPEG       VC_FOURCC('m','j','p','g')
#define VC_CONTAINER_CODEC_MJPEGA      VC_FOURCC('m','j','p','a')
#define VC_CONTAINER_CODEC_MJPEGB      VC_FOURCC('m','j','p','b')
#define VC_CONTAINER_CODEC_THEORA      VC_FOURCC('t','h','e','o')
#define VC_CONTAINER_CODEC_VP3         VC_FOURCC('v','p','3',' ')
#define VC_CONTAINER_CODEC_VP6         VC_FOURCC('v','p','6',' ')
#define VC_CONTAINER_CODEC_VP7         VC_FOURCC('v','p','7',' ')
#define VC_CONTAINER_CODEC_VP8         VC_FOURCC('v','p','8',' ')
#define VC_CONTAINER_CODEC_RV10        VC_FOURCC('r','v','1','0')
#define VC_CONTAINER_CODEC_RV20        VC_FOURCC('r','v','2','0')
#define VC_CONTAINER_CODEC_RV30        VC_FOURCC('r','v','3','0')
#define VC_CONTAINER_CODEC_RV40        VC_FOURCC('r','v','4','0')
#define VC_CONTAINER_CODEC_AVS         VC_FOURCC('a','v','s',' ')
#define VC_CONTAINER_CODEC_SPARK       VC_FOURCC('s','p','r','k')
#define VC_CONTAINER_CODEC_DIRAC       VC_FOURCC('d','r','a','c')

#define VC_CONTAINER_CODEC_YUV         VC_FOURCC('y','u','v',' ')
#define VC_CONTAINER_CODEC_I420        VC_FOURCC('I','4','2','0')
#define VC_CONTAINER_CODEC_YV12        VC_FOURCC('Y','V','1','2')
#define VC_CONTAINER_CODEC_I422        VC_FOURCC('I','4','2','2')
#define VC_CONTAINER_CODEC_YUYV        VC_FOURCC('Y','U','Y','V')
#define VC_CONTAINER_CODEC_YVYU        VC_FOURCC('Y','V','Y','U')
#define VC_CONTAINER_CODEC_UYVY        VC_FOURCC('U','Y','V','Y')
#define VC_CONTAINER_CODEC_VYUY        VC_FOURCC('V','Y','U','Y')
#define VC_CONTAINER_CODEC_NV12        VC_FOURCC('N','V','1','2')
#define VC_CONTAINER_CODEC_NV21        VC_FOURCC('N','V','2','1')
#define VC_CONTAINER_CODEC_ARGB        VC_FOURCC('A','R','G','B')
#define VC_CONTAINER_CODEC_RGBA        VC_FOURCC('R','G','B','A')
#define VC_CONTAINER_CODEC_ABGR        VC_FOURCC('A','B','G','R')
#define VC_CONTAINER_CODEC_BGRA        VC_FOURCC('B','G','R','A')
#define VC_CONTAINER_CODEC_RGB16       VC_FOURCC('R','G','B','2')
#define VC_CONTAINER_CODEC_RGB24       VC_FOURCC('R','G','B','3')
#define VC_CONTAINER_CODEC_RGB32       VC_FOURCC('R','G','B','4')
#define VC_CONTAINER_CODEC_BGR16       VC_FOURCC('B','G','R','2')
#define VC_CONTAINER_CODEC_BGR24       VC_FOURCC('B','G','R','3')
#define VC_CONTAINER_CODEC_BGR32       VC_FOURCC('B','G','R','4')
#define VC_CONTAINER_CODEC_YUVUV128    VC_FOURCC('S','A','N','D')

#define VC_CONTAINER_CODEC_JPEG        VC_FOURCC('j','p','e','g')
#define VC_CONTAINER_CODEC_PNG         VC_FOURCC('p','n','g',' ')
#define VC_CONTAINER_CODEC_GIF         VC_FOURCC('g','i','f',' ')
#define VC_CONTAINER_CODEC_PPM         VC_FOURCC('p','p','m',' ')
#define VC_CONTAINER_CODEC_TGA         VC_FOURCC('t','g','a',' ')
#define VC_CONTAINER_CODEC_BMP         VC_FOURCC('b','m','p',' ')

/* Audio */
#define VC_CONTAINER_CODEC_PCM_UNSIGNED_BE  VC_FOURCC('P','C','M','U')
#define VC_CONTAINER_CODEC_PCM_UNSIGNED_LE  VC_FOURCC('p','c','m','u')
#define VC_CONTAINER_CODEC_PCM_SIGNED_BE    VC_FOURCC('P','C','M','S')
#define VC_CONTAINER_CODEC_PCM_SIGNED_LE    VC_FOURCC('p','c','m','s')
#define VC_CONTAINER_CODEC_PCM_FLOAT_BE     VC_FOURCC('P','C','M','F')
#define VC_CONTAINER_CODEC_PCM_FLOAT_LE     VC_FOURCC('p','c','m','f')
/* Defines for native endianness */
#ifdef VC_CONTAINER_IS_BIG_ENDIAN
#define VC_CONTAINER_CODEC_PCM_UNSIGNED     VC_CONTAINER_CODEC_PCM_UNSIGNED_BE
#define VC_CONTAINER_CODEC_PCM_SIGNED       VC_CONTAINER_CODEC_PCM_SIGNED_BE
#define VC_CONTAINER_CODEC_PCM_FLOAT        VC_CONTAINER_CODEC_PCM_FLOAT_BE
#else
#define VC_CONTAINER_CODEC_PCM_UNSIGNED     VC_CONTAINER_CODEC_PCM_UNSIGNED_LE
#define VC_CONTAINER_CODEC_PCM_SIGNED       VC_CONTAINER_CODEC_PCM_SIGNED_LE
#define VC_CONTAINER_CODEC_PCM_FLOAT        VC_CONTAINER_CODEC_PCM_FLOAT_LE
#endif

#define VC_CONTAINER_CODEC_MPGA        VC_FOURCC('m','p','g','a')
#define VC_CONTAINER_CODEC_MP4A        VC_FOURCC('m','p','4','a')
#define VC_CONTAINER_CODEC_ALAW        VC_FOURCC('a','l','a','w')
#define VC_CONTAINER_CODEC_MULAW       VC_FOURCC('u','l','a','w')
#define VC_CONTAINER_CODEC_ADPCM_MS    VC_FOURCC('m','s',0x0,0x2)
#define VC_CONTAINER_CODEC_ADPCM_IMA_MS VC_FOURCC('m','s',0x0,0x1)
#define VC_CONTAINER_CODEC_ADPCM_SWF   VC_FOURCC('a','s','w','f')
#define VC_CONTAINER_CODEC_WMA1        VC_FOURCC('w','m','a','1')
#define VC_CONTAINER_CODEC_WMA2        VC_FOURCC('w','m','a','2')
#define VC_CONTAINER_CODEC_WMAP        VC_FOURCC('w','m','a','p')
#define VC_CONTAINER_CODEC_WMAL        VC_FOURCC('w','m','a','l')
#define VC_CONTAINER_CODEC_WMAV        VC_FOURCC('w','m','a','v')
#define VC_CONTAINER_CODEC_AMRNB       VC_FOURCC('a','m','r','n')
#define VC_CONTAINER_CODEC_AMRWB       VC_FOURCC('a','m','r','w')
#define VC_CONTAINER_CODEC_AMRWBP      VC_FOURCC('a','m','r','p')
#define VC_CONTAINER_CODEC_AC3         VC_FOURCC('a','c','3',' ')
#define VC_CONTAINER_CODEC_EAC3        VC_FOURCC('e','a','c','3')
#define VC_CONTAINER_CODEC_DTS         VC_FOURCC('d','t','s',' ')
#define VC_CONTAINER_CODEC_MLP         VC_FOURCC('m','l','p',' ')
#define VC_CONTAINER_CODEC_FLAC        VC_FOURCC('f','l','a','c')
#define VC_CONTAINER_CODEC_VORBIS      VC_FOURCC('v','o','r','b')
#define VC_CONTAINER_CODEC_SPEEX       VC_FOURCC('s','p','x',' ')
#define VC_CONTAINER_CODEC_ATRAC3      VC_FOURCC('a','t','r','3')
#define VC_CONTAINER_CODEC_ATRACX      VC_FOURCC('a','t','r','x')
#define VC_CONTAINER_CODEC_ATRACL      VC_FOURCC('a','t','r','l')
#define VC_CONTAINER_CODEC_MIDI        VC_FOURCC('m','i','d','i')
#define VC_CONTAINER_CODEC_EVRC        VC_FOURCC('e','v','r','c')
#define VC_CONTAINER_CODEC_NELLYMOSER  VC_FOURCC('n','e','l','y')
#define VC_CONTAINER_CODEC_QCELP       VC_FOURCC('q','c','e','l')

/* Text */
#define VC_CONTAINER_CODEC_TEXT        VC_FOURCC('t','e','x','t')
#define VC_CONTAINER_CODEC_SSA         VC_FOURCC('s','s','a',' ')
#define VC_CONTAINER_CODEC_USF         VC_FOURCC('u','s','f',' ')
#define VC_CONTAINER_CODEC_VOBSUB      VC_FOURCC('v','s','u','b')

#define VC_CONTAINER_CODEC_UNKNOWN     VC_FOURCC('u','n','k','n')

/* Codec variants */

/** ISO 14496-10 Annex B byte stream format */
#define VC_CONTAINER_VARIANT_H264_DEFAULT    0
/** ISO 14496-15 AVC format (used in mp4/mkv and other containers) */
#define VC_CONTAINER_VARIANT_H264_AVC1       VC_FOURCC('a','v','c','C')
/** Implicitly delineated NAL units without emulation prevention */
#define VC_CONTAINER_VARIANT_H264_RAW        VC_FOURCC('r','a','w',' ')

/** MPEG 1/2 Audio - Layer unknown */
#define VC_CONTAINER_VARIANT_MPGA_DEFAULT    0
/** MPEG 1/2 Audio - Layer 1 */
#define VC_CONTAINER_VARIANT_MPGA_L1         VC_FOURCC('l','1',' ',' ')
/** MPEG 1/2 Audio - Layer 2 */
#define VC_CONTAINER_VARIANT_MPGA_L2         VC_FOURCC('l','2',' ',' ')
/** MPEG 1/2 Audio - Layer 3 */
#define VC_CONTAINER_VARIANT_MPGA_L3         VC_FOURCC('l','3',' ',' ')

/** Converts a WaveFormat ID into a VC_CONTAINER_FOURCC_T.
 *
 * \param  waveformat_id WaveFormat ID to convert
 * \return a valid VC_CONTAINER_FOURCC_T or VC_CONTAINER_CODEC_UNKNOWN if no mapping was found.
 */
VC_CONTAINER_FOURCC_T waveformat_to_codec(uint16_t waveformat_id);

/** Converts a VC_CONTAINER_FOURCC_T into a WaveFormat ID.
 *
 * \param  codec VC_CONTAINER_FOURCC_T to convert
 * \return a valid WaveFormat ID of 0 if no mapping was found.
 */
uint16_t codec_to_waveformat(VC_CONTAINER_FOURCC_T codec);

/** Tries to convert a generic fourcc into a VC_CONTAINER_FOURCC_T.
 *
 * \param  fourcc fourcc to convert
 * \return a valid VC_CONTAINER_FOURCC_T or VC_CONTAINER_CODEC_UNKNOWN if no mapping was found.
 */
VC_CONTAINER_FOURCC_T fourcc_to_codec(uint32_t fourcc);

uint32_t codec_to_fourcc(VC_CONTAINER_FOURCC_T codec);

/** Tries to convert VideoForWindows fourcc into a VC_CONTAINER_FOURCC_T.
 *
 * \param  fourcc vfw fourcc to convert
 * \return a valid VC_CONTAINER_FOURCC_T or VC_CONTAINER_CODEC_UNKNOWN if no mapping was found.
 */
VC_CONTAINER_FOURCC_T vfw_fourcc_to_codec(uint32_t fourcc);

/** Tries to convert a VC_CONTAINER_FOURCC_T into a VideoForWindows fourcc.
 *
 * \param  codec VC_CONTAINER_FOURCC_T to convert
 * \return a valid vfw fourcc or 0 if no mapping was found.
 */
uint32_t codec_to_vfw_fourcc(VC_CONTAINER_FOURCC_T codec);

#ifdef __cplusplus
}
#endif

#endif /* VC_CONTAINERS_CODECS_H */
