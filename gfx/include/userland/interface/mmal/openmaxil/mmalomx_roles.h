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

/** \file
 * OpenMAX IL adaptation layer for MMAL - Role specific functions
 */

#define MMALOMX_MAX_ROLES 16

typedef enum MMALOMX_ROLE_T {
   MMALOMX_ROLE_UNDEFINED = 0,
   MMALOMX_ROLE_VIDEO_DECODER_H263,
   MMALOMX_ROLE_VIDEO_DECODER_MPEG4,
   MMALOMX_ROLE_VIDEO_DECODER_AVC,
   MMALOMX_ROLE_VIDEO_DECODER_MPEG2,
   MMALOMX_ROLE_VIDEO_DECODER_WMV,
   MMALOMX_ROLE_VIDEO_DECODER_VPX,

   MMALOMX_ROLE_VIDEO_ENCODER_H263,
   MMALOMX_ROLE_VIDEO_ENCODER_MPEG4,
   MMALOMX_ROLE_VIDEO_ENCODER_AVC,

   MMALOMX_ROLE_AUDIO_DECODER_AAC,
   MMALOMX_ROLE_AUDIO_DECODER_MPGA_L1,
   MMALOMX_ROLE_AUDIO_DECODER_MPGA_L2,
   MMALOMX_ROLE_AUDIO_DECODER_MPGA_L3,
   MMALOMX_ROLE_AUDIO_DECODER_DDP,

   MMALOMX_ROLE_AIV_PLAY_101,
   MMALOMX_ROLE_AIV_PLAY_AVCDDP,

   MMALOMX_ROLE_MAX
} MMALOMX_ROLE_T;

const char *mmalomx_role_to_name(MMALOMX_ROLE_T role);
MMALOMX_ROLE_T mmalomx_role_from_name(const char *name);
OMX_ERRORTYPE mmalomx_role_set(struct MMALOMX_COMPONENT_T *component, const char *name);
