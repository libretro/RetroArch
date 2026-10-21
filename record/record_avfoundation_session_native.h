/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_AVFOUNDATION_SESSION_NATIVE_H
#define RECORD_AVFOUNDATION_SESSION_NATIVE_H
#include "record_avfoundation_session.h"
#include "record_avfoundation_worker_native.h"
#include "record_gpu_worker_vt_native.h"
#include <stdlib.h>
#include <string.h>
#ifdef VIDEOTOOLBOX_ENCODE_SLOTS
typedef struct record_avf_session_native
{
   record_gpu_capture_metal_t *capture;
   videotoolbox_encode_t *encoder;
   record_mux_avfoundation_config_t output;
   char *path;
} record_avf_session_native_t;
static INLINE void *record_avf_session_native_open(void *context,void *sample)
{
   record_avf_session_native_t *c=(record_avf_session_native_t*)context;
   record_mux_avfoundation_config_t output=c->output;
   if (!sample) return NULL;
   output.video_format=CMSampleBufferGetFormatDescription((CMSampleBufferRef)sample);
   return record_mux_avfoundation_new(&output,c->path);
}
static INLINE bool record_avf_session_native_cancel(void *mux)
{ return record_mux_avfoundation_cancel((record_mux_avfoundation_t*)mux); }
static INLINE bool record_avf_session_native_mux_free(void *mux)
{ return record_mux_avfoundation_free((record_mux_avfoundation_t*)mux); }
static INLINE VT_ENCODE_AVAILABLE void record_avf_session_native_encoder_destroy(void *context)
{
   record_avf_session_native_t *c=(record_avf_session_native_t*)context;
   videotoolbox_encode_cancel(c->encoder); c->encoder=NULL;
}
static INLINE bool record_avf_session_native_context_destroy(void *context)
{
   record_avf_session_native_t *c=(record_avf_session_native_t*)context;
   if (!record_gpu_capture_metal_free(c->capture)) return false;
   free(c->path); free(c); return true;
}
/* Cold I/O-worker binding. Success adopts a fresh matching queue, capture and
 * encoder; NULL leaves all three with the caller. The renderer may borrow the
 * capture/queue only until its own video_finish. Device/output changes must
 * close/cancel and retire this session before constructing another one.
 * width/height are logical dimensions; encoder dimensions may be aligned.
 * output.video_format must be NULL: the actual first sample supplies it.
 * PCM queue is allocated here from the declared layout/chunk and byte budget.
 * There is no implicit audio/video rebasing, resampling or producer closure. */
static INLINE VT_ENCODE_AVAILABLE record_avfoundation_session_t *record_avfoundation_session_new_native(
      record_gpu_queue_t *queue,record_gpu_capture_metal_t *capture,
      videotoolbox_encode_t *encoder,unsigned width,unsigned height,
      const record_mux_avfoundation_config_t *output,size_t audio_capacity_bytes,const char *path)
{
   record_avf_session_native_t *c;
   record_gpu_worker_vt_t *video=NULL;
   record_audio_queue_t *audio=NULL;
   record_avfoundation_session_t *session=NULL;
   record_avfoundation_session_ops_t ops;
   size_t length;
   unsigned mask,count=0;
   if (!queue || !capture || !encoder || !output || output->video_format
         || output->epoch<0 || !output->timescale || output->timescale!=encoder->config.timescale
         || !path || !*path) return NULL;
   mask=output->audio_layout;
   while (mask) { count+=mask&1; mask>>=1; }
   if (output->channels)
   {
      if (output->channels>8 || count!=output->channels || (output->audio_layout&~0x63fu)
            || output->audio_rate<8000 || output->audio_rate>192000
            || output->audio_bitrate<32000 || output->audio_bitrate>512000
            || !output->max_audio_frames || output->max_audio_frames>65536) return NULL;
   }
   else if (audio_capacity_bytes || output->audio_rate || output->audio_layout
         || output->audio_bitrate || output->max_audio_frames) return NULL;
   for (length=0;path[length] && length<32768;length++) {}
   if (length==32768) return NULL;
   c=(record_avf_session_native_t*)calloc(1,sizeof(*c)); if (!c) return NULL;
   c->path=(char*)malloc(length+1); if (!c->path) { free(c); return NULL; }
   memcpy(c->path,path,length+1); c->output=*output; c->capture=capture; c->encoder=encoder;
   if (output->channels)
   {
      audio=record_audio_queue_new(output->channels,output->max_audio_frames,audio_capacity_bytes);
      if (!audio) goto failed;
   }
   video=record_gpu_worker_vt_new_native_visible(queue,capture,encoder,width,height);
   if (!video) goto failed;
   ops.open=record_avf_session_native_open;
   ops.mux.video=record_avf_native_video; ops.mux.audio=record_avf_native_audio; ops.mux.finish=record_avf_native_finish;
   ops.cancel=record_avf_session_native_cancel; ops.free_mux=record_avf_session_native_mux_free;
   ops.destroy_encoder=record_avf_session_native_encoder_destroy;
   ops.destroy_context=record_avf_session_native_context_destroy;
   session=record_avfoundation_session_new(queue,video,audio,c,&ops);
   if (session) return session;
failed:
   record_gpu_worker_vt_free(video); record_audio_queue_free(audio);
   free(c->path); free(c); return NULL;
}
#endif
#endif
