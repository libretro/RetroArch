/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_gpu_capture_metal.h"
#define RECORD_GPU_WORKER_VT_NATIVE_H
#define VIDEOTOOLBOX_ENCODE_SLOTS 16
#define VT_ENCODE_AVAILABLE
typedef void *CMVideoFormatDescriptionRef;
typedef void *CMSampleBufferRef;
typedef struct videotoolbox_encode
{
   struct { unsigned timescale,bit_depth; } config;
} videotoolbox_encode_t;
struct record_gpu_capture_metal { unsigned unused; };
static unsigned native_format_refs,native_capture_frees,native_cancel_calls;
static bool native_format_missing;
static CMVideoFormatDescriptionRef CMSampleBufferGetFormatDescription(CMSampleBufferRef sample)
{
   return native_format_missing?NULL:sample;
}
static const void *CFRetain(const void *format)
{
   assert(format && !native_format_refs++);
   return format;
}
static void CFRelease(const void *format)
{
   assert(format && native_format_refs==1 && encoder_dead==1 && gpu_retirement==2);
   native_format_refs--;
}
static void videotoolbox_encode_cancel(videotoolbox_encode_t *encoder)
{
   assert(encoder && !encoder_dead++);
   native_cancel_calls++;
   free(encoder);
}
bool record_gpu_capture_metal_free(record_gpu_capture_metal_t *capture)
{
   assert(capture && encoder_dead==1 && gpu_retirement==2);
   native_capture_frees++;
   free(capture);
   return true;
}
static record_gpu_worker_vt_t *record_gpu_worker_vt_new_native_visible(
      record_gpu_queue_t *queue,record_gpu_capture_metal_t *capture,
      videotoolbox_encode_t *encoder,unsigned width,unsigned height)
{
   assert(queue && capture && encoder && width==64 && height==48 && !native_session_worker);
   native_session_worker=(record_gpu_worker_vt_t*)calloc(1,sizeof(*native_session_worker));
   assert(native_session_worker);
   return native_session_worker;
}
bool record_gpu_worker_vt_free(record_gpu_worker_vt_t *worker)
{
   assert(worker==native_session_worker && !worker->made && !worker->held);
   free(worker);
   native_session_worker=NULL;
   return true;
}
#include "../../../record/record_stream_session_vt_native.h"

/* 0 drain; 1 cancel with an SRC borrow; 2 missing format; 3 cancel unbound. */
static void native_session_test(unsigned mode)
{
   record_gpu_queue_t *queue=record_gpu_queue_new(2);
   record_gpu_capture_metal_t *capture=(record_gpu_capture_metal_t*)calloc(1,sizeof(*capture));
   videotoolbox_encode_t *encoder=(videotoolbox_encode_t*)calloc(1,sizeof(*encoder));
   record_audio_queue_t *source=record_audio_queue_new(2,1537,16384);
   record_at_audio_t *aac=(record_at_audio_t*)calloc(1,sizeof(*aac));
   record_at_resample_t *src=(record_at_resample_t*)calloc(1,sizeof(*src));
   net_rtmp_client_t *client=(net_rtmp_client_t*)calloc(1,sizeof(*client));
   record_avfoundation_session_t *session;
   record_stream_vt_config_t config;
   record_gpu_worker_vt_t temporary;
   static int16_t pcm[3074];
   unsigned i;
   int result=RECORD_AVF_WORKER_BUSY;
   assert(queue && capture && encoder && source && aac && src && client);
   setup(client,&temporary,aac,&config);
   encoder->config.timescale=config.video_timescale;
   encoder->config.bit_depth=config.bit_depth;
   src->source=source; src->channels=2; src->rate=48000;
   aac_frees=src_frees=client_frees=encoder_dead=gpu_retirement=0;
   native_capture_frees=native_format_refs=native_cancel_calls=0;
   reset_failures=dispose_failures=src_cancel_failures=client_dispose_failures=0;
   native_format_missing=mode==2;
   config.audio_fifo_frames=0;
   assert(!record_stream_session_vt_new_native(queue,capture,encoder,64,48,
            source,aac,src,client,&config));
   assert(!native_session_worker && !aac_frees && !src_frees && !client_frees);
   config.format=NULL;
   aac->consumed=1;
   assert(!record_stream_session_vt_new_native(queue,capture,encoder,64,48,
            source,aac,src,client,&config));
   aac->consumed=0;
   client->cancelled=true;
   assert(!record_stream_session_vt_new_native(queue,capture,encoder,64,48,
            source,aac,src,client,&config));
   client->cancelled=false;
   src->accepted=1;
   assert(!record_stream_session_vt_new_native(queue,capture,encoder,64,48,
            source,aac,src,client,&config));
   src->accepted=0;
   session=record_stream_session_vt_new_native(queue,capture,encoder,64,48,
         source,aac,src,client,&config);
   assert(session && native_session_worker);
   for (i=0;i<3074;i++) pcm[i]=1234;
   assert(record_avfoundation_session_audio(session,pcm,1537,0)==RECORD_AUDIO_OK);
   assert(record_avfoundation_session_video_finish(session));
   if (mode==1)
   {
      for (i=0;i<10000 && !aac->consumed;i++)
         assert(record_avfoundation_session_step(session)>=0);
      assert(aac->consumed && src->samples && native_format_refs==1);
      record_avfoundation_session_cancel(session);
      src_cancel_failures=reset_failures=dispose_failures=client_dispose_failures=1;
   }
   if (mode==3)
   {
      record_avfoundation_session_cancel(session);
      src_cancel_failures=reset_failures=dispose_failures=client_dispose_failures=1;
   }
   if (mode==1 || mode==3)
   {
      for (i=0;i<8;i++)
         assert(record_avfoundation_session_step(session)==RECORD_AVF_WORKER_BUSY);
      assert(!encoder_dead && !native_capture_frees && !client_frees);
   }
   assert(record_avfoundation_session_audio_finish(session));
   for (i=0;i<10000;i++)
   {
      result=record_avfoundation_session_step(session);
      if (result==RECORD_AVF_WORKER_END || result==RECORD_AVF_WORKER_ERROR) break;
   }
   assert(i<10000 && (mode?result==RECORD_AVF_WORKER_ERROR:result==RECORD_AVF_WORKER_END));
   assert(record_avfoundation_session_error(session)==(mode==0?RECORD_AVF_SESSION_NONE:
            mode==2?RECORD_AVF_SESSION_MUX:RECORD_AVF_SESSION_CANCELLED));
   assert(aac_frees==1 && src_frees==1 && client_frees==1 && encoder_dead==1
         && native_cancel_calls==1 && native_capture_frees==1 && !native_session_worker && !native_format_refs);
   assert(record_avfoundation_session_free(session));
   puts("PASS: native session binding, format lifetime and adopted/unbound retirement (SDK stand-ins)");
}
