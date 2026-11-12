/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_stream_mediacodec.h"
#include <stdlib.h>

struct record_stream_mediacodec
{
   record_mediacodec_encoder_t *video, *audio;
   net_rtmp_client_t *client;
   net_rtmp_session_t *session;
   rflv_mux_t *mux;
   record_mediacodec_stream_config_t config;
   const record_mediacodec_packet_t *pending;
   bool format[2], end[2];
   bool ready, sending, pending_audio, accepted, finishing, finished;
   bool failed, retired;
};

static bool record_mcs_stream_track(void *context, bool audio, void *format)
{
   record_stream_mediacodec_t *s = (record_stream_mediacodec_t*)context;
   unsigned at = audio ? 1 : 0;
   if (s->failed || s->retired || s->format[at] || !format
         || (audio && !s->audio))
      return false;
   s->format[at] = true;
   return true;
}

static int record_mcs_stream_start(void *context)
{
   record_stream_mediacodec_t *s = (record_stream_mediacodec_t*)context;
   if (s->failed || s->retired)
      return -1;
   if (!s->format[0] || !s->format[1] || !s->session)
      return 0;
   if (!s->mux)
   {
      s->mux = record_mediacodec_stream_mux_new(s->video, s->audio, &s->config);
      if (!s->mux)
      {
         s->failed = true;
         return -1;
      }
   }
   return s->ready ? 1 : 0;
}

static int record_mcs_stream_write(void *context, bool audio,
      const record_mediacodec_packet_t *packet)
{
   record_stream_mediacodec_t *s = (record_stream_mediacodec_t*)context;
   int result;
   if (s->failed || s->retired || !s->ready || s->finishing || !packet
         || s->end[audio ? 1 : 0] || (audio && !s->audio))
      return -1;
   if (s->pending)
   {
      if (s->pending != packet || s->pending_audio != audio || !s->accepted)
         return 0;
      s->pending = NULL;
      s->accepted = false;
      return 1;
   }
   if (!packet->size && (packet->flags & 4))
      return 1;
   if (!packet->data || !packet->size || packet->pts_us < 0 || (packet->flags & ~5u))
      return -1;
   if (!audio && s->config.codec != RECORD_MC_STREAM_AV1)
      result = rflv_mux_push_annexb(s->mux, packet->data, packet->size,
            (uint64_t)packet->pts_us, (uint64_t)packet->pts_us, !!(packet->flags & 1));
   else
      result = rflv_mux_push(s->mux, audio ? 2 : 1, packet->data, packet->size,
            (uint64_t)packet->pts_us, (uint64_t)packet->pts_us, !!(packet->flags & 1));
   if (result == RFLV_BUSY)
      return 0;
   if (result != RFLV_PROGRESS)
      return -1;
   s->pending = packet;
   s->pending_audio = audio;
   return 0;
}

static bool record_mcs_stream_end(void *context, bool audio, int64_t end_us)
{
   record_stream_mediacodec_t *s = (record_stream_mediacodec_t*)context;
   if (s->failed || s->retired || end_us < 0
         || (s->pending && s->pending_audio == audio))
      return false;
   s->end[audio ? 1 : 0] = true;
   return true;
}

static int record_mcs_stream_finish(void *context)
{
   record_stream_mediacodec_t *s = (record_stream_mediacodec_t*)context;
   if (s->failed || s->retired || !s->end[0] || !s->end[1])
      return -1;
   if (s->pending || s->sending)
      return 0;
   if (!s->finishing)
   {
      if (!rflv_mux_finish(s->mux))
         return -1;
      if (!net_rtmp_session_finish(s->session))
         return 0;
      s->finishing = true;
   }
   return s->finished ? 1 : 0;
}

static bool record_mcs_stream_retire(void *context)
{
   record_stream_mediacodec_t *s = (record_stream_mediacodec_t*)context;
   if (s->retired)
      return true;
   net_rtmp_client_cancel(s->client);
   if (s->sending && !net_rtmp_session_media_release(s->session))
      return false;
   s->sending = false;
   rflv_mux_free(s->mux);
   s->mux = NULL;
   s->pending = NULL;
   s->retired = true;
   return true;
}

record_stream_mediacodec_t *record_stream_mediacodec_new(
      record_mediacodec_encoder_t *video, record_mediacodec_encoder_t *audio,
      net_rtmp_client_t *client, const record_mediacodec_stream_config_t *config)
{
   record_stream_mediacodec_t *s;
   if (!video || !client || !config || !config->max_packet_size
         || (unsigned)config->codec > RECORD_MC_STREAM_AV1)
      return NULL;
   s = (record_stream_mediacodec_t*)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
   s->video = video;
   s->audio = audio;
   s->client = client;
   s->config = *config;
   s->format[1] = s->end[1] = !audio;
   return s;
}

const record_mediacodec_output_ops_t *record_stream_mediacodec_ops(void)
{
   static const record_mediacodec_output_ops_t ops = {
      record_mcs_stream_track, record_mcs_stream_start, record_mcs_stream_write,
      record_mcs_stream_end, record_mcs_stream_finish, record_mcs_stream_retire
   };
   return &ops;
}

int record_stream_mediacodec_step(record_stream_mediacodec_t *s,
      uint32_t receive_time, net_rtmp_client_view_t *wait)
{
   enum net_rtmp_client_state state;
   rflv_tag_t tag;
   net_rtmp_span_t spans[2];
   int result;
   if (!s || !wait || s->failed || s->retired)
      return -1;
   state = net_rtmp_client_step(s->client, receive_time, wait);
   if (state == NET_RTMP_CLIENT_FAILED)
      goto failed;
   if (state == NET_RTMP_CLIENT_FINISHED)
   {
      if (!s->finishing)
         goto failed;
      s->finished = true;
      return 2;
   }
   if (state != NET_RTMP_CLIENT_READY)
      return wait->again ? 1 : 0;
   s->session = wait->session;
   if (!s->session)
      goto failed;
   if (!s->mux || s->accepted)
      return wait->again ? 1 : 0;
   if (s->sending)
   {
      if (!net_rtmp_session_media_release(s->session))
         return wait->again ? 1 : 0;
      s->sending = false;
      if (!rflv_mux_release(s->mux))
         goto failed;
      s->accepted = s->pending != NULL;
      return 1;
   }
   result = rflv_mux_step(s->mux);
   if (result == RFLV_ERROR)
      goto failed;
   if (result == RFLV_PACKET)
   {
      if (!rflv_mux_get_tag(s->mux, &tag))
         return rflv_mux_release(s->mux) ? 1 : -1;
      spans[0].data = tag.spans[0].data;
      spans[0].size = tag.spans[0].size;
      spans[1].data = tag.spans[1].data;
      spans[1].size = tag.spans[1].size;
      if (net_rtmp_session_media(s->session, tag.type, tag.timestamp, spans, 2))
      {
         s->sending = true;
         return 1;
      }
   }
   else if (result == RFLV_BUSY)
      s->ready = true;
   return result == RFLV_PROGRESS || wait->again ? 1 : 0;
failed:
   s->failed = true;
   return -1;
}

bool record_stream_mediacodec_free(record_stream_mediacodec_t *s)
{
   if (s && !record_mcs_stream_retire(s))
      return false;
   free(s);
   return true;
}
