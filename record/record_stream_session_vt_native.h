/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_STREAM_SESSION_VT_NATIVE_H
#define RECORD_STREAM_SESSION_VT_NATIVE_H
#include "record_stream_output_vt.h"
#include "record_gpu_worker_vt_native.h"
#include "record_gpu_session_metal.h"
#include <stdlib.h>
#include <string.h>
#ifdef VIDEOTOOLBOX_ENCODE_SLOTS
typedef struct record_stream_session_vt_native
{
   record_stream_vt_config_t config;
   record_gpu_capture_metal_t *capture;
   videotoolbox_encode_t *encoder;
   record_at_audio_t *audio;
   record_at_resample_t *converter;
   net_rtmp_client_t *client;
   CMVideoFormatDescriptionRef format;
} record_stream_session_vt_native_t;

static INLINE void *record_stream_session_vt_native_open(void *context,
      void *sample,record_gpu_worker_vt_t *video,record_audio_queue_t *source)
{
   record_stream_session_vt_native_t *c=(record_stream_session_vt_native_t*)context;
   record_stream_output_vt_t *output;
   CMVideoFormatDescriptionRef format;
   if (!sample || c->format)
      return NULL;
   format=CMSampleBufferGetFormatDescription((CMSampleBufferRef)sample);
   if (!format)
      return NULL;
   c->format=(CMVideoFormatDescriptionRef)CFRetain(format);
   c->config.format=(void*)c->format;
   output=record_stream_output_vt_new(video,c->audio,c->converter,c->client,&c->config,source);
   if (output)
   {
      c->audio=NULL;
      c->converter=NULL;
      c->client=NULL;
   }
   return output;
}

static INLINE VT_ENCODE_AVAILABLE void record_stream_session_vt_native_encoder_destroy(void *context)
{
   record_stream_session_vt_native_t *c=(record_stream_session_vt_native_t*)context;
   videotoolbox_encode_cancel(c->encoder);
   c->encoder=NULL;
}

static INLINE bool record_stream_session_vt_native_context_destroy(void *context)
{
   record_stream_session_vt_native_t *c=(record_stream_session_vt_native_t*)context;
   /* These remain fresh if first-sample binding never adopted them. */
   if (c->converter)
   {
      if (!record_at_resample_cancel(c->converter) || !record_at_resample_free(c->converter))
         return false;
      c->converter=NULL;
   }
   if (c->audio)
   {
      if (!record_at_audio_cancel(c->audio) || !record_at_audio_free(c->audio))
         return false;
      c->audio=NULL;
   }
   if (c->client)
   {
      net_rtmp_client_cancel(c->client);
      if (!net_rtmp_client_free(c->client))
         return false;
      c->client=NULL;
   }
   if (!record_gpu_capture_metal_free(c->capture))
      return false;
   if (c->format)
      CFRelease(c->format);
   free(c);
   return true;
}

/* Cold worker only. Success adopts every supplied resource; failure adopts
 * none. AAC/SRC must be fresh, client READY, and format NULL. Explicit epochs
 * use the AAC output clock. Source PCM may already be queued. */
static INLINE VT_ENCODE_AVAILABLE record_avfoundation_session_t *record_stream_session_vt_new_native(
      record_gpu_queue_t *queue,record_gpu_capture_metal_t *capture,
      videotoolbox_encode_t *encoder,unsigned width,unsigned height,
      record_audio_queue_t *source,record_at_audio_t *audio,
      record_at_resample_t *converter,net_rtmp_client_t *client,
      const record_stream_vt_config_t *config)
{
   record_stream_session_vt_native_t *c;
   record_gpu_worker_vt_t *video;
   record_avfoundation_session_t *session;
   record_avfoundation_session_output_ops_t ops;
   record_audio_queue_t *configured;
   net_rtmp_client_view_t view;
   uint64_t input,output;
   unsigned rate,channels,leading,trailing;
   if (!queue || !capture || !encoder || !client || !config || config->format
         || config->video_timescale!=encoder->config.timescale
         || config->bit_depth!=encoder->config.bit_depth
         || config->audio_fifo_frames || (audio?!source:(source || converter)))
      return NULL;
   if (net_rtmp_client_step(client,0,&view)!=NET_RTMP_CLIENT_READY || !view.session)
      return NULL;
   if (audio && (!record_at_audio_timing(audio,&leading,&trailing,&input) || input
            || record_audio_queue_channels(source)!=config->channels))
      return NULL;
   if (converter && (!record_at_resample_config(converter,&configured,&rate,&channels)
            || configured!=source || rate!=config->audio_rate || channels!=config->channels
            || !record_at_resample_timing(converter,&input,&output) || input || output))
      return NULL;
   c=(record_stream_session_vt_native_t*)calloc(1,sizeof(*c));
   if (!c)
      return NULL;
   c->config=*config;
   c->capture=capture;
   c->encoder=encoder;
   c->audio=audio;
   c->converter=converter;
   c->client=client;
   video=record_gpu_worker_vt_new_native_visible(queue,capture,encoder,width,height);
   if (!video)
   {
      free(c);
      return NULL;
   }
   memset(&ops,0,sizeof(ops));
   record_stream_output_vt_set_ops(&ops);
   ops.open=record_stream_session_vt_native_open;
   ops.destroy_encoder=record_stream_session_vt_native_encoder_destroy;
   ops.destroy_context=record_stream_session_vt_native_context_destroy;
   session=record_avfoundation_session_new_output(queue,video,source,c,&ops);
   if (!session)
   {
      record_gpu_worker_vt_free(video);
      free(c);
   }
   return session;
}

typedef struct record_stream_session_vt_binding
{
   const record_stream_vt_config_t *config;
   record_audio_queue_t *source;
   record_at_audio_t *audio;
   record_at_resample_t *converter;
   net_rtmp_client_t *client;
} record_stream_session_vt_binding_t;

static INLINE VT_ENCODE_AVAILABLE record_avfoundation_session_t *record_stream_session_vt_bind(
      void *context,record_gpu_queue_t *queue,record_gpu_capture_metal_t *capture,
      struct videotoolbox_encode *encoder,unsigned width,unsigned height)
{
   record_stream_session_vt_binding_t *binding=(record_stream_session_vt_binding_t*)context;
   return record_stream_session_vt_new_native(queue,capture,encoder,width,height,
         binding->source,binding->audio,binding->converter,binding->client,binding->config);
}

/* NULL preserves caller-owned audio/SRC/client/source; the cold bootstrap
 * retires its own partial video resources. Success adopts all of them. */
#if defined(__APPLE__) && defined(__clang__)
static INLINE __attribute__((availability(macos,introduced=10.14)))
#else
static INLINE
#endif
record_avfoundation_session_t *record_stream_session_vt_new_metal(
      void *device,const record_gpu_session_metal_config_t *video,
      record_audio_queue_t *source,record_at_audio_t *audio,
      record_at_resample_t *converter,net_rtmp_client_t *client,
      const record_stream_vt_config_t *config,record_gpu_capture_metal_t **capture,
      record_gpu_queue_t **queue,enum record_gpu_session_metal_startup *stage,int32_t *status)
{
   record_stream_session_vt_binding_t binding;
   binding.config=config;
   binding.source=source;
   binding.audio=audio;
   binding.converter=converter;
   binding.client=client;
   return record_gpu_session_metal_new_output(device,video,record_stream_session_vt_bind,&binding,
         capture,queue,stage,status);
}
#endif
#endif
