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
#include <util/mmal_default_components.h>

#ifndef ENABLE_MMALOMX_AUDIO_HW_DECODER
# define ENABLE_MMALOMX_AUDIO_HW_DECODER 0
#endif
#ifndef ENABLE_MMALOMX_AUDIO_SPDIF
# define ENABLE_MMALOMX_AUDIO_SPDIF 1
#endif

static const struct {
   const char *omx;
   const char *omx_prefix;
   const char *mmal;
   MMALOMX_ROLE_T roles[MMALOMX_ROLE_MAX];
} mmalomx_components[] =
{
   {"video.hw.decoder", 0, MMAL_COMPONENT_DEFAULT_VIDEO_DECODER,
      {MMALOMX_ROLE_VIDEO_DECODER_H263, MMALOMX_ROLE_VIDEO_DECODER_MPEG2,
       MMALOMX_ROLE_VIDEO_DECODER_MPEG4, MMALOMX_ROLE_VIDEO_DECODER_AVC,
       MMALOMX_ROLE_VIDEO_DECODER_WMV, MMALOMX_ROLE_VIDEO_DECODER_VPX,
       MMALOMX_ROLE_UNDEFINED}},
   {"video.hw.decoder.secure", 0, "drm_alloc.video_decode",
      {MMALOMX_ROLE_VIDEO_DECODER_H263, MMALOMX_ROLE_VIDEO_DECODER_MPEG2,
       MMALOMX_ROLE_VIDEO_DECODER_MPEG4, MMALOMX_ROLE_VIDEO_DECODER_AVC,
       MMALOMX_ROLE_VIDEO_DECODER_WMV, MMALOMX_ROLE_VIDEO_DECODER_VPX,
       MMALOMX_ROLE_UNDEFINED}},
   {"video.hw.decoder.divx_drm", 0, "aggregator.pipeline:divx_drm:vc.video_decode",
      {MMALOMX_ROLE_VIDEO_DECODER_MPEG4, MMALOMX_ROLE_UNDEFINED}},
   {"video.vpx.decoder", 0, "libvpx",
      {MMALOMX_ROLE_VIDEO_DECODER_VPX, MMALOMX_ROLE_UNDEFINED}},

   {"video.hw.encoder", 0, MMAL_COMPONENT_DEFAULT_VIDEO_ENCODER,
      {MMALOMX_ROLE_VIDEO_ENCODER_H263, MMALOMX_ROLE_VIDEO_ENCODER_MPEG4,
       MMALOMX_ROLE_VIDEO_ENCODER_AVC, MMALOMX_ROLE_UNDEFINED}},

   {"AIV.play", "", "aivplay",
      {MMALOMX_ROLE_AIV_PLAY_101, MMALOMX_ROLE_AIV_PLAY_AVCDDP, MMALOMX_ROLE_UNDEFINED}},
   {"AIV.play.avcddp", "", "aivplay.ddp",
      {MMALOMX_ROLE_AIV_PLAY_AVCDDP, MMALOMX_ROLE_AIV_PLAY_101, MMALOMX_ROLE_UNDEFINED}},

#if ENABLE_MMALOMX_AUDIO_HW_DECODER
   {"audio.hw.decoder", 0, "vc.ril.audio_decode",
      {MMALOMX_ROLE_AUDIO_DECODER_AAC, MMALOMX_ROLE_AUDIO_DECODER_MPGA_L1,
       MMALOMX_ROLE_AUDIO_DECODER_MPGA_L2, MMALOMX_ROLE_AUDIO_DECODER_MPGA_L3,
       MMALOMX_ROLE_AUDIO_DECODER_DDP, MMALOMX_ROLE_UNDEFINED}},
#endif

#if ENABLE_MMALOMX_AUDIO_SPDIF
   {"audio.spdif", 0, "spdif",
      {MMALOMX_ROLE_AUDIO_DECODER_DDP, MMALOMX_ROLE_UNDEFINED}},
#endif

   {0, 0, 0, {MMALOMX_ROLE_UNDEFINED}}
};

int mmalomx_registry_find_component(const char *name)
{
   int i, prefix_size;
   const char *prefix;

   for (i = 0; mmalomx_components[i].omx; i++)
   {
      /* Check the prefix first */
      prefix = mmalomx_components[i].omx_prefix;
      if (!prefix)
         prefix = MMALOMX_COMPONENT_PREFIX;
      prefix_size = strlen(prefix);
      if (strncmp(name, prefix, prefix_size))
         continue;

      /* Check the rest of the name */
      if (!strcmp(name + prefix_size, mmalomx_components[i].omx))
         break;
   }

   return mmalomx_components[i].mmal ? i : -1;
}

const char *mmalomx_registry_component_mmal(int id)
{
   if (id >= (int)MMAL_COUNTOF(mmalomx_components) || id < 0)
      id = MMAL_COUNTOF(mmalomx_components) - 1;

   return mmalomx_components[id].mmal;
}

MMALOMX_ROLE_T mmalomx_registry_component_roles(int id, unsigned int index)
{
   unsigned int i;

   if (id >= (int)MMAL_COUNTOF(mmalomx_components) || id < 0)
      id = MMAL_COUNTOF(mmalomx_components) - 1;

   for (i = 0; i < index; i++)
      if (mmalomx_components[id].roles[i] == MMALOMX_ROLE_UNDEFINED)
         break;

   return mmalomx_components[id].roles[i];
}

MMAL_BOOL_T mmalomx_registry_component_supports_role(int id, MMALOMX_ROLE_T role)
{
   unsigned int i;

   if (id >= (int)MMAL_COUNTOF(mmalomx_components) || id < 0)
      id = MMAL_COUNTOF(mmalomx_components) - 1;

   for (i = 0; mmalomx_components[id].roles[i] != MMALOMX_ROLE_UNDEFINED; i++)
      if (mmalomx_components[id].roles[i] == role)
         return MMAL_TRUE;

   return MMAL_FALSE;
}

const char *mmalomx_registry_component_name(int id, const char **prefix)
{
   if (id >= (int)MMAL_COUNTOF(mmalomx_components) || id < 0)
      id = MMAL_COUNTOF(mmalomx_components) - 1;

   if (prefix)
   {
      *prefix = mmalomx_components[id].omx_prefix;
      if (!*prefix)
         *prefix = MMALOMX_COMPONENT_PREFIX;
   }

   return mmalomx_components[id].omx;
}
