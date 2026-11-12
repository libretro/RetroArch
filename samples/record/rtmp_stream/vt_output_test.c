/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define VT_STREAM_TEST_MAIN existing_stream_contract_main
#include "vt_contract_test.c"
#include "../../../record/record_stream_output_vt.h"
static unsigned aac_frees,src_frees,client_frees,reset_failures,dispose_failures;
static unsigned client_dispose_failures;
static unsigned encoder_dead,gpu_retirement;
bool record_gpu_worker_vt_abandon(record_gpu_worker_vt_t *worker)
{ (void)worker; assert(encoder_dead==1); return ++gpu_retirement>1; }
bool record_at_audio_cancel(record_at_audio_t *a)
{
   if(a->held) return false;
   if(reset_failures) { reset_failures--; return false; }
   a->loan=false; a->pending=0; a->ended=true; return true;
}
bool record_at_audio_free(record_at_audio_t *a)
{
   if(a->held || a->loan || !a->ended) return false;
   if(dispose_failures) { dispose_failures--; return false; }
   aac_frees++; free(a); return true;
}
bool record_at_resample_free(record_at_resample_t *r)
{
   if(r->samples || (!r->ended && !r->cancelled)) return false;
   src_frees++; free(r); return true;
}
bool net_rtmp_client_free(net_rtmp_client_t *c)
{
   assert(!c->wire_live && (c->cancelled
         || net_rtmp_session_status(c->session,NULL,NULL)==NET_RTMP_SESSION_FINISHED));
   if(client_dispose_failures) { client_dispose_failures--; return false; }
   if(!net_rtmp_session_free(c->session)) return false;
   client_frees++; free(c); return true;
}
static void *native_open(void *context,void *sample,
      record_gpu_worker_vt_t *video,record_audio_queue_t *audio)
{ (void)context; (void)sample; (void)video; (void)audio; return NULL; }
static void native_destroy(void *context) { (void)context; }
static bool native_free(void *context) { (void)context; return true; }
struct output_owner
{
   record_stream_vt_config_t config;
   net_rtmp_client_t *client;
   record_at_audio_t *aac;
   record_at_resample_t *src;
   unsigned opened,retired;
};
static void *owner_open(void *context,void *sample,
      record_gpu_worker_vt_t *video,record_audio_queue_t *audio)
{
   struct output_owner *owner=(struct output_owner*)context;
   record_stream_output_vt_t *output;
   assert(sample==video && !owner->opened++);
   owner->config.format=sample;
   output=record_stream_output_vt_new(video,owner->aac,owner->src,owner->client,&owner->config,audio);
   assert(output);
   owner->aac=NULL; owner->src=NULL; owner->client=NULL;
   return output;
}
static void owner_encoder_destroy(void *context)
{
   struct output_owner *owner=(struct output_owner*)context;
   assert(owner->opened==1 && !owner->aac && !owner->src && !owner->client);
   assert(aac_frees==1 && src_frees==1 && client_frees==1 && !encoder_dead++);
}
static bool owner_free(void *context)
{
   struct output_owner *owner=(struct output_owner*)context;
   assert(encoder_dead==1 && gpu_retirement==2 && !owner->retired++);
   return true;
}
static void owned_test(bool cancel)
{
   struct output_owner owner;
   record_gpu_worker_vt_t video;
   record_gpu_queue_t *queue=record_gpu_queue_new(2);
   record_audio_queue_t *source=record_audio_queue_new(2,1537,16384);
   record_avfoundation_session_t *session;
   record_avfoundation_session_output_ops_t ops;
   net_rtmp_client_t *client=(net_rtmp_client_t*)calloc(1,sizeof(*client));
   record_at_audio_t *aac=(record_at_audio_t*)calloc(1,sizeof(*aac));
   record_at_resample_t *src=(record_at_resample_t*)calloc(1,sizeof(*src));
   static int16_t pcm[3074];
   unsigned i;
   int result=RECORD_AVF_WORKER_BUSY;
   assert(client && aac && src && queue && source);
   memset(&owner,0,sizeof(owner)); memset(&ops,0,sizeof(ops));
   setup(client,&video,aac,&owner.config); owner.config.audio_fifo_frames=0;
   src->source=source; src->rate=48000; src->channels=2;
   owner.client=client; owner.aac=aac; owner.src=src;
   ops.open=owner_open; ops.destroy_encoder=owner_encoder_destroy; ops.destroy_context=owner_free;
   assert(record_stream_output_vt_set_ops(&ops));
   aac_frees=src_frees=client_frees=reset_failures=dispose_failures=encoder_dead=gpu_retirement=0;
   src_cancel_failures=0; client_dispose_failures=1;
   session=record_avfoundation_session_new_output(queue,&video,source,&owner,&ops); assert(session);
   for(i=0;i<3074;i++) pcm[i]=1234;
   assert(record_avfoundation_session_audio(session,pcm,1537,0)==RECORD_AUDIO_OK);
   if(!cancel) assert(record_avfoundation_session_audio(session,pcm,512,1537)==RECORD_AUDIO_OK);
   assert(record_avfoundation_session_video_finish(session));
   if(cancel)
   {
      for(i=0;i<10000 && !aac->consumed;i++) assert(record_avfoundation_session_step(session)>=0);
      assert(aac->consumed && src->samples);
      client->blocked=true;
      for(i=0;i<20;i++) assert(record_avfoundation_session_step(session)>=0);
      assert(client->wire_live); client->delay_discard=true;
      record_avfoundation_session_cancel(session);
      for(i=0;i<8;i++) assert(record_avfoundation_session_step(session)==RECORD_AVF_WORKER_BUSY);
      assert(!aac_frees && !src_frees && !client_frees && !encoder_dead);
      assert(!record_avfoundation_session_free(session));
      reset_failures=dispose_failures=1;
      src_cancel_failures=1;
   }
   assert(record_avfoundation_session_audio_finish(session));
   for(i=0;i<10000;i++)
   {
      result=record_avfoundation_session_step(session);
      if(result==RECORD_AVF_WORKER_END || result==RECORD_AVF_WORKER_ERROR) break;
   }
   assert(i<10000 && owner.retired==1 && encoder_dead==1 && gpu_retirement==2);
   assert(cancel?result==RECORD_AVF_WORKER_ERROR:result==RECORD_AVF_WORKER_END);
   assert(record_avfoundation_session_free(session));
}
static void output_test(bool converted,bool cancel)
{
   net_rtmp_client_t *client=(net_rtmp_client_t*)calloc(1,sizeof(*client));
   record_at_audio_t *aac=(record_at_audio_t*)calloc(1,sizeof(*aac));
   record_at_resample_t *src=NULL;
   record_gpu_worker_vt_t video;
   record_stream_vt_config_t config;
   record_avfoundation_session_output_ops_t ops;
   record_stream_output_vt_t *output;
   record_audio_queue_t *source=record_audio_queue_new(2,1537,16384);
   static int16_t pcm[3074];
   unsigned i;
   int result=RECORD_AVF_WORKER_BUSY;
   assert(client && aac && source);
   aac_frees=src_frees=client_frees=reset_failures=dispose_failures=0;
   src_cancel_failures=0; client_dispose_failures=1;
   setup(client,&video,aac,&config); config.audio_fifo_frames=0;
   for(i=0;i<3074;i++) pcm[i]=1234;
   assert(record_audio_queue_push(source,pcm,1537,0)==RECORD_AUDIO_OK);
   if(!cancel)
   {
      assert(record_audio_queue_push(source,pcm,512,1537)==RECORD_AUDIO_OK);
      assert(record_audio_queue_finish(source));
   }
   if(converted)
   {
      src=(record_at_resample_t*)calloc(1,sizeof(*src)); assert(src);
      src->source=source; src->rate=48000; src->channels=2;
   }
   assert(!record_stream_output_vt_new(&video,aac,src,client,&config,NULL));
   assert(!aac_frees && !src_frees && !client_frees);
   output=record_stream_output_vt_new(&video,aac,src,client,&config,source); assert(output);
   memset(&ops,0,sizeof(ops)); ops.open=native_open;
   ops.destroy_encoder=native_destroy; ops.destroy_context=native_free;
   assert(!record_stream_output_vt_set_ops(NULL) && record_stream_output_vt_set_ops(&ops));
   assert(ops.open==native_open && ops.destroy_encoder==native_destroy && ops.destroy_context==native_free);
   assert(!ops.free_output(output));
   if(cancel)
   {
      for(i=0;i<10000 && !aac->consumed;i++) assert(ops.step(output)>=0);
      assert(aac->consumed && (!converted || src->samples));
      client->blocked=true;
      for(i=0;i<20;i++) assert(ops.step(output)>=0);
      assert(client->wire_live);
      client->delay_discard=true;
      assert(!ops.cancel(output) && !aac_frees && !src_frees && !client_frees);
      assert(ops.retry_needed(output));
      assert(record_audio_queue_finish(source));
      for(i=0;i<100 && !ops.cancel(output);i++) {}
      assert(i<100 && record_stream_output_vt_error(output)==RECORD_STREAM_VT_ERROR_ABORTED);
      reset_failures=dispose_failures=1;
      src_cancel_failures=converted?1:0;
   }
   else
   {
      for(i=0;i<10000 && result!=RECORD_AVF_WORKER_END;i++)
      { result=ops.step(output); assert(result>=0); }
      assert(result==RECORD_AVF_WORKER_END && aac->consumed==(converted?4098u:2049u));
      assert(record_stream_output_vt_error(output)==RECORD_STREAM_VT_ERROR_NONE);
      dispose_failures=1;
   }
   for(i=0;i<100 && !ops.free_output(output);i++) {}
   assert(i<100 && aac_frees==1 && client_frees==1 && src_frees==(converted?1u:0u));
   {
      const int16_t *samples;
      size_t frames;
      uint64_t timestamp;
      while(record_audio_queue_peek_lend(source,&samples,&frames,&timestamp)==RECORD_AUDIO_OK)
         assert(record_audio_queue_ack(source));
   }
   assert(record_audio_queue_free(source));
}
static void video_only_test(void)
{
   net_rtmp_client_t *client=(net_rtmp_client_t*)calloc(1,sizeof(*client));
   static record_at_audio_t ignored;
   record_gpu_worker_vt_t video;
   record_stream_vt_config_t config;
   record_stream_output_vt_t *output;
   record_avfoundation_session_output_ops_t ops;
   unsigned i;
   int result=RECORD_AVF_WORKER_BUSY;
   assert(client);
   setup(client,&video,&ignored,&config);
   config.audio_rate=config.channels=config.audio_epoch_frames=0;
   config.audio_fifo_frames=0; config.video_epoch_ticks=0;
   aac_frees=src_frees=client_frees=0; client_dispose_failures=1;
   output=record_stream_output_vt_new(&video,NULL,NULL,client,&config,NULL); assert(output);
   memset(&ops,0,sizeof(ops)); assert(record_stream_output_vt_set_ops(&ops));
   for(i=0;i<10000 && result!=RECORD_AVF_WORKER_END;i++)
   { result=ops.step(output); assert(result>=0); }
   assert(result==RECORD_AVF_WORKER_END);
   for(i=0;i<100 && !ops.free_output(output);i++) {}
   assert(i<100 && !aac_frees && !src_frees && client_frees==1);
}
int main(void)
{
   output_test(false,false); output_test(true,false);
   output_test(false,true); output_test(true,true);
   owned_test(false); owned_test(true);
   video_only_test();
   puts("PASS: VT output ownership, network-held cancellation, SRC/AAC reset/dispose retry and exactly-once free");
   return 0;
}
