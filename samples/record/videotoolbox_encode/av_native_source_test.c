/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <retro_inline.h>
#define main src_contract_main
#include "../audiotoolbox_audio/resample_test.c"
#undef main
#include "../../../record/record_avfoundation_session.h"
#include "../../../record/record_mux_avfoundation.h"
#define RECORD_AVFOUNDATION_WORKER_NATIVE_H
#define RECORD_GPU_WORKER_VT_NATIVE_H
#define VIDEOTOOLBOX_ENCODE_SLOTS 2
#define VT_ENCODE_AVAILABLE

typedef struct record_gpu_capture_metal record_gpu_capture_metal_t;
typedef struct videotoolbox_encode
{
   struct
   {
      unsigned timescale;
   } config;
} videotoolbox_encode_t;
typedef void *CMSampleBufferRef;
static int sample_token, mux_token;
static bool fail_mux;
static const void *CMSampleBufferGetFormatDescription(CMSampleBufferRef sample)
{
   assert(sample==&sample_token);
   return sample;
}
void videotoolbox_encode_cancel(videotoolbox_encode_t *encoder);
bool record_gpu_capture_metal_free(record_gpu_capture_metal_t *capture);
record_gpu_worker_vt_t *record_gpu_worker_vt_new_native_visible(record_gpu_queue_t *q,
      record_gpu_capture_metal_t *capture,videotoolbox_encode_t *encoder,
      unsigned width,unsigned height);
int record_avf_native_video(void *mux,void *sample);
int record_avf_native_audio(void *mux,const int16_t *samples,size_t frames,uint64_t timestamp);
int record_avf_native_finish(void *mux);
#include "../../../record/record_avfoundation_session_native.h"

record_mux_avfoundation_t *record_mux_avfoundation_new(
      const record_mux_avfoundation_config_t *config,const char *path)
{
   assert(config->source_rate==0 && config->audio_rate==48000);
   assert(config->video_format==&sample_token && !strcmp(path,"test.mp4"));
   return fail_mux?NULL:(record_mux_avfoundation_t*)&mux_token;
}
static void native_source(unsigned channels,unsigned cancellation)
{
   record_avf_session_native_t context;
   record_audio_queue_t *q=record_audio_queue_new(channels,8,8*channels*2+32);
   int16_t input[64];
   const int16_t *samples,*again;
   size_t frames,again_frames;
   uint64_t timestamp,again_timestamp;
   unsigned i;
   int result;
   assert(q);
   memset(&context,0,sizeof(context));
   context.source=q;
   context.path="test.mp4";
   context.output.source_rate=32040.5;
   context.output.audio_rate=48000;
   context.output.channels=channels;
   for (i=0;i<8*channels;i++) input[i]=(int16_t)(100+i);
   assert(record_audio_queue_push(q,input,8,0)==RECORD_AUDIO_OK);
   fail_mux=cancellation==2;
   property_failure=cancellation==3;
   assert((record_avf_session_native_open(&context,&sample_token)!=NULL)
         ==!(fail_mux || property_failure));
   assert(context.converted && (context.converter!=NULL)==!property_failure);
   property_failure=0;
   expected_output=context.converted;
   expected=NULL;
   if (!fail_mux && cancellation!=3)
   {
      result=record_avf_session_native_audio_peek(&context,&samples,&frames,&timestamp);
      assert(result==RECORD_AUDIO_OK && frames==8 && timestamp==0);
      assert(samples==context.converted && !memcmp(samples,input,frames*channels*2));
      assert(record_avf_session_native_audio_peek(&context,&again,&again_frames,
               &again_timestamp)==RECORD_AUDIO_OK);
      assert(again==samples && again_frames==frames && again_timestamp==timestamp);
      assert(!record_audio_queue_free(q));
      if (!cancellation)
      {
         assert(record_avf_session_native_audio_ack(&context));
         assert(!record_avf_session_native_audio_ack(&context));
         assert(record_audio_queue_finish(q));
         assert(record_avf_session_native_audio_peek(&context,&samples,&frames,
                  &timestamp)==RECORD_AUDIO_END);
         assert(context.converted_timestamp==8);
      }
   }
   if (cancellation==1)
   {
      reset_failure=1;
      assert(!record_avf_session_native_audio_retire(&context));
      assert(context.converter && context.converted && !record_audio_queue_free(q));
      reset_failure=0;
   }
   assert(record_avf_session_native_audio_retire(&context));
   assert(!context.converter && !context.converted);
   assert(record_avf_session_native_audio_retire(&context));
   assert(record_audio_queue_finish(q));
   while (record_audio_queue_peek_lend(q,&samples,&frames,&timestamp)==RECORD_AUDIO_OK)
      assert(record_audio_queue_ack(q));
   assert(record_audio_queue_free(q));
}
int main(void)
{
   unsigned channels,cancellation;
   for (channels=1;channels<=8;channels++)
      for (cancellation=0;cancellation<4;cancellation++)
         native_source(channels,cancellation);
   puts("PASS actual native file SRC binding, cached packets, mux failure and reset-retirement retry; native APIs mocked");
   return 0;
}
