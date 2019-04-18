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

#include "mmalomx.h"
#include "mmalomx_roles.h"
#include "mmalomx_registry.h"
#include "mmalomx_logging.h"

static const struct {
   const char *name;
   MMALOMX_ROLE_T role;
} mmalomx_roles[] =
{
   {"video_decoder.h263",     MMALOMX_ROLE_VIDEO_DECODER_H263},
   {"video_decoder.mpeg4",    MMALOMX_ROLE_VIDEO_DECODER_MPEG4},
   {"video_decoder.avc",      MMALOMX_ROLE_VIDEO_DECODER_AVC},
   {"video_decoder.mpeg2",    MMALOMX_ROLE_VIDEO_DECODER_MPEG2},
   {"video_decoder.wmv",      MMALOMX_ROLE_VIDEO_DECODER_WMV},
   {"video_decoder.vpx",      MMALOMX_ROLE_VIDEO_DECODER_VPX},

   {"video_encoder.h263",     MMALOMX_ROLE_VIDEO_ENCODER_H263},
   {"video_encoder.mpeg4",    MMALOMX_ROLE_VIDEO_ENCODER_MPEG4},
   {"video_encoder.avc",      MMALOMX_ROLE_VIDEO_ENCODER_AVC},

   {"audio_decoder.aac",      MMALOMX_ROLE_AUDIO_DECODER_AAC},
   {"audio_decoder.mp1",      MMALOMX_ROLE_AUDIO_DECODER_MPGA_L1},
   {"audio_decoder.mp2",      MMALOMX_ROLE_AUDIO_DECODER_MPGA_L2},
   {"audio_decoder.mp3",      MMALOMX_ROLE_AUDIO_DECODER_MPGA_L3},
   {"audio_decoder.ddp",      MMALOMX_ROLE_AUDIO_DECODER_DDP},

   {"AIV.play.101",           MMALOMX_ROLE_AIV_PLAY_101},
   {"play.avcddp",            MMALOMX_ROLE_AIV_PLAY_AVCDDP},

   {0, 0}
};

const char *mmalomx_role_to_name(MMALOMX_ROLE_T role)
{
    unsigned int i;
    for (i = 0; mmalomx_roles[i].name; i++)
       if (mmalomx_roles[i].role == role)
          break;
    return mmalomx_roles[i].name;
}

MMALOMX_ROLE_T mmalomx_role_from_name(const char *name)
{
   unsigned int i;
   for (i = 0; mmalomx_roles[i].name; i++)
      if (!strcmp(mmalomx_roles[i].name, name))
         break;
   return mmalomx_roles[i].role;
}

static void mmalomx_format_encoding_from_role(MMALOMX_ROLE_T role,
   MMAL_FOURCC_T *encoding, MMAL_ES_TYPE_T *es_type, unsigned int *port)
{
   switch (role)
   {
   case MMALOMX_ROLE_VIDEO_DECODER_MPEG4:
   case MMALOMX_ROLE_VIDEO_ENCODER_MPEG4:
      *encoding = MMAL_ENCODING_MP4V;
      *es_type = MMAL_ES_TYPE_VIDEO;
      break;
   case MMALOMX_ROLE_VIDEO_DECODER_AVC:
   case MMALOMX_ROLE_VIDEO_ENCODER_AVC:
      *encoding = MMAL_ENCODING_H264;
      *es_type = MMAL_ES_TYPE_VIDEO;
      break;
   case MMALOMX_ROLE_VIDEO_DECODER_MPEG2:
      *encoding = MMAL_ENCODING_MP2V;
      *es_type = MMAL_ES_TYPE_VIDEO;
      break;
   case MMALOMX_ROLE_VIDEO_DECODER_WMV:
      *encoding = MMAL_ENCODING_WMV3;
      *es_type = MMAL_ES_TYPE_VIDEO;
      break;
   case MMALOMX_ROLE_VIDEO_DECODER_VPX:
      *encoding = MMAL_ENCODING_VP8;
      *es_type = MMAL_ES_TYPE_VIDEO;
      break;
   case MMALOMX_ROLE_VIDEO_DECODER_H263:
   case MMALOMX_ROLE_VIDEO_ENCODER_H263:
      *encoding = MMAL_ENCODING_H263;
      *es_type = MMAL_ES_TYPE_VIDEO;
      break;
   case MMALOMX_ROLE_AUDIO_DECODER_AAC:
      *encoding = MMAL_ENCODING_MP4A;
      *es_type = MMAL_ES_TYPE_AUDIO;
      break;
   case MMALOMX_ROLE_AUDIO_DECODER_MPGA_L1:
   case MMALOMX_ROLE_AUDIO_DECODER_MPGA_L2:
   case MMALOMX_ROLE_AUDIO_DECODER_MPGA_L3:
      *encoding = MMAL_ENCODING_MPGA;
      *es_type = MMAL_ES_TYPE_AUDIO;
      break;

   case MMALOMX_ROLE_AUDIO_DECODER_DDP:
      *encoding = MMAL_ENCODING_AC3;
      *es_type = MMAL_ES_TYPE_AUDIO;
      break;

   default:
      *encoding = MMAL_ENCODING_UNKNOWN;
      *es_type = MMAL_ES_TYPE_UNKNOWN;
      break;
   }

   switch (role)
   {
   case MMALOMX_ROLE_VIDEO_ENCODER_H263:
   case MMALOMX_ROLE_VIDEO_ENCODER_MPEG4:
   case MMALOMX_ROLE_VIDEO_ENCODER_AVC:
      *port = 1;
      break;
   default:
      *port = 0;
      break;
   }
}

OMX_ERRORTYPE mmalomx_role_set(MMALOMX_COMPONENT_T *component, const char *name)
{
   const MMALOMX_ROLE_T role = mmalomx_role_from_name(name);
   MMAL_FOURCC_T encoding = MMAL_ENCODING_UNKNOWN;
   MMAL_ES_TYPE_T es_type = MMAL_ES_TYPE_UNKNOWN;
   unsigned int port;
   MMAL_ES_FORMAT_T *format;

   if (!role || !mmalomx_registry_component_supports_role(component->registry_id, role))
      return OMX_ErrorUnsupportedSetting;

   component->role = role;

   mmalomx_format_encoding_from_role(role, &encoding, &es_type, &port);
   if (encoding == MMAL_ENCODING_UNKNOWN)
      return OMX_ErrorNone;

   format = component->ports[port].mmal->format;
   format->type = es_type;
   format->encoding = encoding;
   format->bitrate = 64000;
   switch (es_type)
   {
   case MMAL_ES_TYPE_VIDEO:
      format->es->video.width = 176;
      format->es->video.height = 144;
      format->es->video.frame_rate.num = 15;
      format->es->video.frame_rate.den = 1;
      break;
   default:
      break;
   }

   switch (role)
   {
   case MMALOMX_ROLE_VIDEO_DECODER_H263:
   case MMALOMX_ROLE_VIDEO_ENCODER_H263:
      component->ports[port].format_param.h263.eProfile = OMX_VIDEO_H263ProfileBaseline;
      component->ports[port].format_param.h263.eLevel = OMX_VIDEO_H263Level10;
      component->ports[port].format_param.h263.bPLUSPTYPEAllowed = OMX_FALSE;
      component->ports[port].format_param.h263.bForceRoundingTypeToZero = OMX_TRUE;
      break;
   case MMALOMX_ROLE_VIDEO_DECODER_MPEG4:
   case MMALOMX_ROLE_VIDEO_ENCODER_MPEG4:
      component->ports[port].format_param.mpeg4.eProfile = OMX_VIDEO_MPEG4ProfileSimple;
      component->ports[port].format_param.mpeg4.eLevel = OMX_VIDEO_MPEG4Level1;
      break;
   case MMALOMX_ROLE_VIDEO_DECODER_AVC:
   case MMALOMX_ROLE_VIDEO_ENCODER_AVC:
      component->ports[port].format_param.avc.eProfile = OMX_VIDEO_AVCProfileBaseline;
      component->ports[port].format_param.avc.eLevel = OMX_VIDEO_AVCLevel1;
      break;
   case MMALOMX_ROLE_VIDEO_DECODER_WMV:
      component->ports[port].format_param.wmv.eFormat = OMX_VIDEO_WMVFormat9;
      break;
   default:
      break;
   }

   if (mmal_port_format_commit(component->ports[port].mmal) != MMAL_SUCCESS)
      LOG_ERROR("failed to commit format to %s for role %s",
                component->ports[port].mmal->name, name);

   return OMX_ErrorNone;
}
