/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_mediacodec_stream_config.h"
#include <stdlib.h>
#include <string.h>

rflv_mux_t *record_mediacodec_stream_mux_new(record_mediacodec_encoder_t *video,
      record_mediacodec_encoder_t *audio, const record_mediacodec_stream_config_t *config)
{
   rflv_config_t mux_config;
   rflv_mux_t *mux;
   const void *first = NULL, *second = NULL;
   size_t first_size = 0, second_size = 0;
   uint8_t *joined = NULL;
   if (!video || !config || (unsigned)config->codec > RECORD_MC_STREAM_AV1
         || (config->bit_depth != 8 && config->bit_depth != 10)
         || (config->codec == RECORD_MC_STREAM_AVC && config->bit_depth != 8)
         || !config->max_packet_size
         || config->max_packet_size > (config->codec == RECORD_MC_STREAM_HEVC
               ? 0xfffff7u : 0xfffffau))
      return NULL;
   memset(&mux_config, 0, sizeof(mux_config));
   if (!record_mediacodec_encoder_video_config(video, 0, &first, &first_size))
      return NULL;
   mux_config.avcc = first;
   mux_config.avcc_size = first_size;
   if (config->codec == RECORD_MC_STREAM_AVC
         && record_mediacodec_encoder_video_config(video, 1, &second, &second_size))
   {
      if (!first || !second || !first_size || !second_size
            || first_size > 65535 || second_size > 65535)
         return NULL;
      joined = (uint8_t*)malloc(first_size + second_size);
      if (!joined)
         return NULL;
      memcpy(joined, first, first_size);
      memcpy(joined + first_size, second, second_size);
      mux_config.avcc = joined;
      mux_config.avcc_size = first_size + second_size;
   }
   mux_config.video_timescale = 1000000;
   mux_config.max_packet_size = config->max_packet_size;
   if (audio)
   {
      if (!record_mediacodec_encoder_audio_config(audio, &mux_config.asc, &mux_config.asc_size))
      {
         free(joined);
         return NULL;
      }
      mux_config.audio_timescale = 1000000;
   }
   if (config->codec == RECORD_MC_STREAM_AVC)
      mux = rflv_mux_new_annexb(&mux_config);
   else if (config->codec == RECORD_MC_STREAM_HEVC)
      mux = rflv_mux_new_hevc_annexb(&mux_config);
   else
      mux = rflv_mux_new_av1(&mux_config);
   free(joined);
   if (mux && config->codec != RECORD_MC_STREAM_AVC
         && !rflv_mux_set_color(mux, &config->color, config->bit_depth))
   {
      rflv_mux_free(mux);
      return NULL;
   }
   return mux;
}
