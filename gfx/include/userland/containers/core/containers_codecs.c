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

#include <stdlib.h>
#include <string.h>

#include "containers/containers.h"
#include "containers/containers_codecs.h"
#include "containers/core/containers_utils.h"

/*****************************************************************************/
static struct {
   VC_CONTAINER_FOURCC_T codec;
   uint16_t id;
} codec_to_wf_table[] =
{
   {VC_CONTAINER_CODEC_PCM_SIGNED_LE, WAVE_FORMAT_PCM},
   {VC_CONTAINER_CODEC_ALAW, WAVE_FORMAT_ALAW},
   {VC_CONTAINER_CODEC_MULAW, WAVE_FORMAT_MULAW},
   {VC_CONTAINER_CODEC_ADPCM_MS, WAVE_FORMAT_ADPCM},
   {VC_CONTAINER_CODEC_MPGA, WAVE_FORMAT_MPEG},
   {VC_CONTAINER_CODEC_MPGA, WAVE_FORMAT_MPEGLAYER3},
   {VC_CONTAINER_CODEC_WMA1, WAVE_FORMAT_WMAUDIO1},
   {VC_CONTAINER_CODEC_WMA2, WAVE_FORMAT_WMAUDIO2},
   {VC_CONTAINER_CODEC_WMAP, WAVE_FORMAT_WMAUDIOPRO},
   {VC_CONTAINER_CODEC_WMAL, WAVE_FORMAT_WMAUDIO_LOSSLESS},
   {VC_CONTAINER_CODEC_WMAV, WAVE_FORMAT_WMAUDIO_VOICE},
   {VC_CONTAINER_CODEC_AC3,  WAVE_FORMAT_DVM},
   {VC_CONTAINER_CODEC_AC3,  WAVE_FORMAT_DOLBY_AC3_SPDIF}, /**< AC-3 padded for S/PDIF */
   {VC_CONTAINER_CODEC_AC3,  WAVE_FORMAT_RAW_SPORT},       /**< AC-3 padded for S/PDIF */
   {VC_CONTAINER_CODEC_AC3,  WAVE_FORMAT_ESST_AC3},        /**< AC-3 padded for S/PDIF */
   {VC_CONTAINER_CODEC_EAC3, WAVE_FORMAT_DVM},
   {VC_CONTAINER_CODEC_DTS, WAVE_FORMAT_DTS},
#if 0
   {CODEC_G726, WAVE_FORMAT_G726_ADPCM},
   {CODEC_G726, WAVE_FORMAT_DF_G726},
   {CODEC_G726, WAVE_FORMAT_G726ADPCM},
   {CODEC_G726, WAVE_FORMAT_PANASONIC_G726},
#endif
   {VC_CONTAINER_CODEC_MP4A, WAVE_FORMAT_AAC},
   {VC_CONTAINER_CODEC_MP4A, WAVE_FORMAT_MP4A},
   {VC_CONTAINER_CODEC_ATRAC3, WAVE_FORMAT_SONY_SCX},
   {VC_CONTAINER_CODEC_UNKNOWN, WAVE_FORMAT_UNKNOWN}
};

VC_CONTAINER_FOURCC_T waveformat_to_codec(uint16_t waveformat_id)
{
   unsigned int i;
   for(i = 0; codec_to_wf_table[i].codec != VC_CONTAINER_CODEC_UNKNOWN; i++)
      if(codec_to_wf_table[i].id == waveformat_id) break;
   return codec_to_wf_table[i].codec;
}

uint16_t codec_to_waveformat(VC_CONTAINER_FOURCC_T codec)
{
   unsigned int i;
   for(i = 0; codec_to_wf_table[i].codec != VC_CONTAINER_CODEC_UNKNOWN; i++)
      if(codec_to_wf_table[i].codec == codec) break;
   return codec_to_wf_table[i].id;
}

static struct {
   VC_CONTAINER_FOURCC_T codec;
   uint32_t fourcc;
} codec_to_vfw_table[] =
{
#if defined(ENABLE_CONTAINERS_STANDALONE) || !defined(NDEBUG)
   /* We are legally required to not play DivX in RELEASE mode. See Jira SW-3138 */
   {VC_CONTAINER_CODEC_DIV3,             VC_FOURCC('D','I','V','3')},
   {VC_CONTAINER_CODEC_DIV3,             VC_FOURCC('d','i','v','3')},
   {VC_CONTAINER_CODEC_DIV4,             VC_FOURCC('D','I','V','4')},
   {VC_CONTAINER_CODEC_DIV4,             VC_FOURCC('d','i','v','4')},
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('D','X','5','0')},
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('D','I','V','X')},
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('d','i','v','x')},
#endif /* ENABLE_CONTAINERS_STANDALONE || !NDEBUG */
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('M','P','4','V')},
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('m','p','4','v')},
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('M','P','4','S')},
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('m','p','4','s')},
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('M','4','S','2')},
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('m','4','s','2')},
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('F','M','P','4')},
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('X','V','I','D')},
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('x','v','i','d')},
   {VC_CONTAINER_CODEC_DIV3,             VC_FOURCC('M','P','4','3')},
   {VC_CONTAINER_CODEC_DIV3,             VC_FOURCC('m','p','4','3')},
   {VC_CONTAINER_CODEC_MP1V,             VC_FOURCC('m','p','g','1')},
   {VC_CONTAINER_CODEC_MP1V,             VC_FOURCC('M','P','G','1')},
   {VC_CONTAINER_CODEC_MP2V,             VC_FOURCC('m','p','g','2')},
   {VC_CONTAINER_CODEC_MP2V,             VC_FOURCC('M','P','G','2')},
   {VC_CONTAINER_CODEC_MJPEG,            VC_FOURCC('M','J','P','G')},
   {VC_CONTAINER_CODEC_MJPEG,            VC_FOURCC('m','j','p','g')},
   {VC_CONTAINER_CODEC_WMV1,             VC_FOURCC('W','M','V','1')},
   {VC_CONTAINER_CODEC_WMV1,             VC_FOURCC('w','m','v','1')},
   {VC_CONTAINER_CODEC_WMV2,             VC_FOURCC('W','M','V','2')},
   {VC_CONTAINER_CODEC_WMV2,             VC_FOURCC('w','m','v','2')},
   {VC_CONTAINER_CODEC_WMV3,             VC_FOURCC('W','M','V','3')},
   {VC_CONTAINER_CODEC_WMV3,             VC_FOURCC('w','m','v','3')},
   {VC_CONTAINER_CODEC_WVC1,             VC_FOURCC('W','V','C','1')},
   {VC_CONTAINER_CODEC_WVC1,             VC_FOURCC('w','v','c','1')},
   {VC_CONTAINER_CODEC_WMVA,             VC_FOURCC('w','m','v','a')},
   {VC_CONTAINER_CODEC_WMVA,             VC_FOURCC('W','M','V','A')},
   {VC_CONTAINER_CODEC_VP6,              VC_FOURCC('V','P','6','F')},
   {VC_CONTAINER_CODEC_VP6,              VC_FOURCC('v','p','6','f')},
   {VC_CONTAINER_CODEC_VP7,              VC_FOURCC('V','P','7','0')},
   {VC_CONTAINER_CODEC_VP7,              VC_FOURCC('v','p','7','0')},
   {VC_CONTAINER_CODEC_H263,             VC_FOURCC('H','2','6','3')},
   {VC_CONTAINER_CODEC_H263,             VC_FOURCC('h','2','6','3')},
   {VC_CONTAINER_CODEC_H264,             VC_FOURCC('H','2','6','4')},
   {VC_CONTAINER_CODEC_H264,             VC_FOURCC('h','2','6','4')},
   {VC_CONTAINER_CODEC_H264,             VC_FOURCC('A','V','C','1')},
   {VC_CONTAINER_CODEC_H264,             VC_FOURCC('a','v','c','1')},
   {VC_CONTAINER_CODEC_SPARK,            VC_FOURCC('F','L','V','1')},
   {VC_CONTAINER_CODEC_SPARK,            VC_FOURCC('f','l','v','1')},
   {VC_CONTAINER_CODEC_UNKNOWN, 0}
};

VC_CONTAINER_FOURCC_T vfw_fourcc_to_codec(uint32_t fourcc)
{
   unsigned int i;
   for(i = 0; codec_to_vfw_table[i].codec != VC_CONTAINER_CODEC_UNKNOWN; i++)
      if(codec_to_vfw_table[i].fourcc == fourcc) break;

   if(codec_to_vfw_table[i].codec == VC_CONTAINER_CODEC_UNKNOWN)
      return fourcc;

   return codec_to_vfw_table[i].codec;
}

uint32_t codec_to_vfw_fourcc(VC_CONTAINER_FOURCC_T codec)
{
   unsigned int i;
   for(i = 0; codec_to_vfw_table[i].codec != VC_CONTAINER_CODEC_UNKNOWN; i++)
      if(codec_to_vfw_table[i].codec == codec) break;

   return codec_to_vfw_table[i].fourcc;
}

static struct {
   VC_CONTAINER_FOURCC_T codec;
   uint32_t fourcc;
} codec_to_fourcc_table[] =
{
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('M','P','4','S')},
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('M','4','S','2')},
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('m','p','4','s')},
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('m','4','s','2')},
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('M','P','4','V')},
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('m','p','4','v')},
   {VC_CONTAINER_CODEC_MP4V,             VC_FOURCC('F','M','P','4')},
   {VC_CONTAINER_CODEC_DIV3,             VC_FOURCC('M','P','4','3')},
   {VC_CONTAINER_CODEC_DIV3,             VC_FOURCC('m','p','4','3')},
   {VC_CONTAINER_CODEC_WMV1,             VC_FOURCC('W','M','V','1')},
   {VC_CONTAINER_CODEC_WMV1,             VC_FOURCC('w','m','v','1')},
   {VC_CONTAINER_CODEC_WMV2,             VC_FOURCC('W','M','V','2')},
   {VC_CONTAINER_CODEC_WMV2,             VC_FOURCC('w','m','v','2')},
   {VC_CONTAINER_CODEC_WMV3,             VC_FOURCC('W','M','V','3')},
   {VC_CONTAINER_CODEC_WMV3,             VC_FOURCC('w','m','v','3')},
   {VC_CONTAINER_CODEC_MP1V,             VC_FOURCC('m','p','g','1')},
   {VC_CONTAINER_CODEC_MP1V,             VC_FOURCC('M','P','G','1')},
   {VC_CONTAINER_CODEC_MP2V,             VC_FOURCC('m','p','g','2')},
   {VC_CONTAINER_CODEC_MP2V,             VC_FOURCC('M','P','G','2')},
   {VC_CONTAINER_CODEC_MJPEG,            VC_FOURCC('M','J','P','G')},
   {VC_CONTAINER_CODEC_MJPEG,            VC_FOURCC('m','j','p','g')},
   {VC_CONTAINER_CODEC_UNKNOWN, 0}
};

VC_CONTAINER_FOURCC_T fourcc_to_codec(uint32_t fourcc)
{
   unsigned int i;
   for(i = 0; codec_to_fourcc_table[i].codec != VC_CONTAINER_CODEC_UNKNOWN; i++)
      if(codec_to_fourcc_table[i].fourcc == fourcc) break;

   return codec_to_fourcc_table[i].codec;
}

uint32_t codec_to_fourcc(VC_CONTAINER_FOURCC_T codec)
{
   unsigned int i;
   for(i = 0; codec_to_fourcc_table[i].codec != VC_CONTAINER_CODEC_UNKNOWN; i++)
      if(codec_to_fourcc_table[i].codec == codec) break;

   return codec_to_fourcc_table[i].fourcc;
}
