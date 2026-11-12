/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../record/record_stream_vt.h"
#include "../../../record/record_vt_packet.h"
#include "../../../record/record_audio_queue.h"
#include <stdlib.h>
#define main session_reference_main
#include "../../../libretro-common/samples/net/rtmp/rtmp_session_test.c"
#undef main
struct net_rtmp_client
{
   net_rtmp_session_t *session;
   bool blocked, wire_live, delay_discard, cancelled;
};
static uint8_t wire[65536];
static size_t wire_size;
enum net_rtmp_client_state net_rtmp_client_step(net_rtmp_client_t *c,
      uint32_t time, net_rtmp_client_view_t *view)
{
   net_rtmp_span_t span;
   (void)time;
   view->session = c->session;
   view->fd = -1;
   view->read = true;
   view->write = view->again = false;
   if (c->cancelled)
      return NET_RTMP_CLIENT_FAILED;
   if (c->blocked)
   {
      c->wire_live = net_rtmp_session_get(c->session, &span);
      return NET_RTMP_CLIENT_READY;
   }
   output_size = 0;
   pump(c->session);
   assert(output_size<=sizeof(wire)-wire_size);
   memcpy(wire+wire_size,output,output_size); wire_size+=output_size;
   c->wire_live = false;
   return net_rtmp_session_status(c->session, NULL, NULL) == NET_RTMP_SESSION_FINISHED
      ? NET_RTMP_CLIENT_FINISHED : NET_RTMP_CLIENT_READY;
}
void net_rtmp_client_cancel(net_rtmp_client_t *c)
{
   c->cancelled = true;
   net_rtmp_session_cancel(c->session);
   if (c->delay_discard)
   {
      c->delay_discard = false;
      return;
   }
   assert(net_rtmp_session_discard(c->session));
   c->wire_live = false;
}


struct record_gpu_worker_vt { unsigned made, releases; bool held, release_fail; };
struct record_vt_packet { struct record_gpu_worker_vt *worker; bool live; };
struct record_at_audio
{
   uint64_t consumed, produced;
   int16_t input[2048];
   unsigned rate, channels;
   size_t pending;
   bool loan, held, finishing, ended;
   uint8_t asc[2], bytes[8];
};
struct record_at_resample
{
   record_audio_queue_t *source;
   const int16_t *samples;
   uint64_t accepted, produced;
   size_t frames, offset;
   unsigned rate, channels;
   bool failed, cancelled, ended;
};
static unsigned src_cancel_failures;
bool record_at_resample_config(const record_at_resample_t *e,
      record_audio_queue_t **q,unsigned *rate,unsigned *channels)
{
   if(!e || e->failed || e->cancelled) return false;
   *q=e->source; *rate=e->rate; *channels=e->channels; return true;
}
bool record_at_resample_timing(const record_at_resample_t *e,uint64_t *in,uint64_t *out)
{ *in=e->accepted; *out=e->produced; return true; }
int record_at_resample_read(record_at_resample_t *e,int16_t *output,size_t capacity,size_t *frames)
{
   uint64_t timestamp;
   size_t n,i;
   int result;
   *frames=0;
   if(e->failed || e->cancelled) return RECORD_AT_RESAMPLE_ERROR;
   if(e->samples && e->offset==e->frames)
   { assert(record_audio_queue_ack(e->source)); e->samples=NULL; }
   if(!e->samples)
   {
      result=record_audio_queue_peek_lend(e->source,&e->samples,&e->frames,&timestamp);
      if(result==RECORD_AUDIO_END) { e->ended=true; return RECORD_AT_RESAMPLE_END; }
      if(result==RECORD_AUDIO_BUSY) return RECORD_AT_RESAMPLE_BUSY;
      assert(result==RECORD_AUDIO_OK);
      e->offset=0;
      if(timestamp!=e->accepted) { e->failed=true; return RECORD_AT_RESAMPLE_ERROR; }
   }
   n=e->frames-e->offset;
   if(n>capacity/2) n=capacity/2;
   for(i=0;i<n*e->channels;i++) assert(e->samples[e->offset*e->channels+i]==1234);
   for(i=0;i<n*2*e->channels;i++) output[i]=1234;
   e->offset+=n; e->accepted+=n; e->produced+=n*2; *frames=n*2;
   return RECORD_AT_RESAMPLE_PROGRESS;
}
bool record_at_resample_cancel(record_at_resample_t *e)
{
   if(src_cancel_failures) { src_cancel_failures--; return false; }
   if(e->samples) { assert(record_audio_queue_ack(e->source)); e->samples=NULL; }
   e->cancelled=true; return true;
}
static record_vt_packet_t *active_view;
static unsigned audio_releases;
static const uint8_t headers[]={1,66,0,30,255,225,0,4,103,66,0,30,1,0,1,104};
static const uint8_t picture[]={0,0,0,1,101};
record_vt_packet_t *record_vt_packet_new(void *format, unsigned scale, size_t capacity)
{
   record_vt_packet_t *p;
   assert(scale==60000 && capacity==65536);
   p=(record_vt_packet_t*)calloc(1,sizeof(*p)); assert(p);
   p->worker=(record_gpu_worker_vt_t*)format; active_view=p; return p;
}
bool record_vt_packet_config(record_vt_packet_t *p, const void **data,size_t *size,bool *hevc)
{ assert(p); *data=headers; *size=sizeof(headers); *hevc=false; return true; }
int record_vt_packet_read(record_vt_packet_t *p,void *sample,record_vt_packet_view_t *v)
{
   assert(sample==p->worker && p->worker->held);
   p->live=true; v->data=picture; v->size=sizeof(picture);
   v->dts=v->pts=p->worker->made*1001; v->keyframe=true; return 1;
}
bool record_vt_packet_release(record_vt_packet_t *p)
{ assert(p->live); p->live=false; return true; }
bool record_vt_packet_free(record_vt_packet_t *p)
{ if(p) { assert(!p->live); free(p); active_view=NULL; } return true; }
int record_gpu_worker_vt_step(record_gpu_worker_vt_t *w,void **sample)
{
   if(w->made==2) return RECORD_GPU_VT_END;
   w->held=true; *sample=w; return RECORD_GPU_VT_PACKET;
}
bool record_gpu_worker_vt_release(record_gpu_worker_vt_t *w)
{
   assert(w->held && !active_view->live);
   if(w->release_fail) { w->release_fail=false; return false; }
   w->held=false; w->made++; w->releases++; return true;
}
bool record_gpu_worker_vt_retry_needed(const record_gpu_worker_vt_t *w)
{ (void)w; return false; }
bool record_at_audio_config(record_at_audio_t *e,const void **data,size_t *size)
{ *data=e->asc; *size=2; return true; }
bool record_at_audio_timing(record_at_audio_t *e,unsigned *leading,unsigned *trailing,uint64_t *frames)
{ *leading=2112; *trailing=0; *frames=e->consumed; return true; }
int record_at_audio_input(record_at_audio_t *e,int16_t **samples,size_t *frames)
{
   assert(!e->held && !e->pending && !e->finishing);
   e->loan=true; *samples=e->input; *frames=1024; return RECORD_AT_AUDIO_PROGRESS;
}
int record_at_audio_submit(record_at_audio_t *e,size_t frames,uint64_t index)
{
   size_t i;
   assert(e->loan && !e->pending && frames<=1024 && frames && index==e->consumed);
   for(i=0;i<frames*e->channels;i++) assert(e->input[i]==1234);
   e->loan=false; e->pending=frames; e->consumed+=frames; return RECORD_AT_AUDIO_PROGRESS;
}
bool record_at_audio_input_release(record_at_audio_t *e)
{ if(!e->loan) return false; e->loan=false; return true; }
int record_at_audio_pump(record_at_audio_t *e,record_at_audio_packet_t *packet)
{
   if(e->pending || (e->finishing && e->produced<e->consumed+2112))
   {
      e->held=true; packet->data=e->bytes; packet->size=sizeof(e->bytes);
      packet->timestamp=(int64_t)e->produced-2112; packet->frames=1024;
      return RECORD_AT_AUDIO_PACKET;
   }
   if(e->finishing) { e->ended=true; return RECORD_AT_AUDIO_END; }
   return RECORD_AT_AUDIO_BUSY;
}
bool record_at_audio_release(record_at_audio_t *e)
{ if(!e->held) return false; e->held=false; e->pending=0; e->produced+=1024; audio_releases++; return true; }
int record_at_audio_finish(record_at_audio_t *e)
{ assert(!e->held && !e->pending && !e->loan); e->finishing=true; return RECORD_AT_AUDIO_PROGRESS; }
static void setup(struct net_rtmp_client *c,record_gpu_worker_vt_t *w,
      record_at_audio_t *a,record_stream_vt_config_t *cfg)
{
   memset(c,0,sizeof(*c)); memset(w,0,sizeof(*w)); memset(a,0,sizeof(*a)); memset(cfg,0,sizeof(*cfg));
   c->session=ready(); wire_size=0; cfg->format=w; cfg->max_packet_size=65536; cfg->video_timescale=60000;
   cfg->bit_depth=8; cfg->audio_rate=a->rate=48000; cfg->channels=a->channels=2;
   cfg->audio_fifo_frames=2048; cfg->audio_epoch_frames=2112; cfg->video_epoch_ticks=2640;
   a->asc[0]=17; a->asc[1]=144; audio_releases=0;
}
#ifdef VT_STREAM_QUEUE_NEGATIVE
static record_stream_vt_t *ignored_queue(record_gpu_worker_vt_t *worker,
      record_at_audio_t *audio,net_rtmp_client_t *client,
      const record_stream_vt_config_t *config,record_audio_queue_t *queue)
{
   record_stream_vt_config_t copy=*config;
   (void)queue;
   copy.audio_fifo_frames=2048;
   return record_stream_vt_new(worker,audio,client,&copy);
}
#define record_stream_vt_new_queued ignored_queue
#endif
static void queued_source(void)
{
   struct net_rtmp_client client;
   record_gpu_worker_vt_t worker;
   static record_at_audio_t aac;
   static int16_t pcm[3074];
   record_stream_vt_config_t config;
   record_stream_vt_wait_t wait;
   record_stream_vt_t *stream;
   record_audio_queue_t *source=record_audio_queue_new(2,1537,16384);
   unsigned i;
   int result=RECORD_STREAM_VT_BUSY;
   assert(source);
   for(i=0;i<3074;i++) pcm[i]=1234;
   assert(record_audio_queue_push(source,pcm,1537,0)==RECORD_AUDIO_OK);
   assert(record_audio_queue_push(source,pcm,512,1537)==RECORD_AUDIO_OK);
   assert(record_audio_queue_finish(source));
   setup(&client,&worker,&aac,&config);
   config.audio_fifo_frames=0;
#ifndef VT_STREAM_QUEUE_NEGATIVE
   {
      record_audio_queue_t *wrong=record_audio_queue_new(1,64,1024);
      assert(wrong && record_audio_queue_channels(wrong)==1 && !record_audio_queue_channels(NULL));
      assert(!record_stream_vt_new_queued(&worker,&aac,&client,&config,wrong));
      assert(!record_stream_vt_new_queued(&worker,NULL,&client,&config,source));
      assert(!record_stream_vt_new_queued(&worker,&aac,&client,&config,NULL));
      config.audio_fifo_frames=2048;
      assert(!record_stream_vt_new_queued(&worker,&aac,&client,&config,source));
      config.audio_fifo_frames=0;
      assert(record_audio_queue_free(wrong));
   }
#endif
   stream=record_stream_vt_new_queued(&worker,&aac,&client,&config,source);
   assert(stream && !record_stream_vt_free(stream));
#ifdef VT_STREAM_QUEUE_NEGATIVE
   assert(record_stream_vt_audio_finish(stream));
#else
   assert(record_stream_vt_audio(stream,pcm,1)==RECORD_STREAM_VT_ERROR);
   assert(!record_stream_vt_audio_finish(stream));
#endif
   for(i=0;i<10000 && result!=RECORD_STREAM_VT_END;i++)
   { result=record_stream_vt_step(stream,0,&wait); assert(result!=RECORD_STREAM_VT_ERROR); }
   assert(result==RECORD_STREAM_VT_END && aac.consumed==2049 && aac.ended);
   assert(record_stream_vt_free(stream) && record_audio_queue_free(source));
   assert(net_rtmp_session_free(client.session));
   setup(&client,&worker,&aac,&config); config.audio_fifo_frames=0;
   source=record_audio_queue_new(2,1537,16384); assert(source);
   assert(record_audio_queue_push(source,pcm,1537,0)==RECORD_AUDIO_OK);
   stream=record_stream_vt_new_queued(&worker,&aac,&client,&config,source); assert(stream);
   for(i=0;i<1000 && !aac.consumed;i++)
      assert(record_stream_vt_step(stream,0,&wait)!=RECORD_STREAM_VT_ERROR);
   assert(aac.consumed==1024 && !record_audio_queue_free(source));
   record_stream_vt_abort(stream);
   for(i=0;i<20 && !record_stream_vt_free(stream);i++)
      assert(record_stream_vt_step(stream,0,&wait)<=RECORD_STREAM_VT_PROGRESS);
   assert(i<20 && net_rtmp_session_free(client.session));
   assert(record_audio_queue_push(source,pcm,2,1537)==RECORD_AUDIO_OK);
   assert(record_audio_queue_finish(source));
   {
      const int16_t *samples;
      size_t frames;
      uint64_t timestamp;
      assert(record_audio_queue_peek(source,&samples,&frames,&timestamp)==RECORD_AUDIO_OK);
      assert(frames==2 && timestamp==1537 && samples[0]==1234);
      assert(record_audio_queue_ack(source) && record_audio_queue_free(source));
   }
   setup(&client,&worker,&aac,&config); config.audio_fifo_frames=0;
   source=record_audio_queue_new(2,257,2048); assert(source);
   assert(record_audio_queue_push(source,pcm,257,1)==RECORD_AUDIO_OK);
   stream=record_stream_vt_new_queued(&worker,&aac,&client,&config,source); assert(stream);
   result=RECORD_STREAM_VT_BUSY;
   for(i=0;i<1000 && result!=RECORD_STREAM_VT_ERROR;i++)
      result=record_stream_vt_step(stream,0,&wait);
   assert(result==RECORD_STREAM_VT_ERROR && !aac.consumed);
   assert(record_stream_vt_error(stream)==RECORD_STREAM_VT_ERROR_AUDIO);
   assert(record_stream_vt_step(stream,0,&wait)==RECORD_STREAM_VT_ERROR);
   assert(record_stream_vt_free(stream) && net_rtmp_session_free(client.session));
   assert(record_audio_queue_finish(source) && record_audio_queue_free(source));
   puts("PASS prebuffered PCM source, native-input chunk boundaries and exact source clock");
}
static void resampled_source(void)
{
   struct net_rtmp_client client;
   record_gpu_worker_vt_t worker;
   static record_at_audio_t aac;
   static int16_t pcm[3074];
   record_stream_vt_config_t config;
   record_stream_vt_wait_t wait;
   record_at_resample_t src;
   record_stream_vt_t *stream;
   record_audio_queue_t *source=record_audio_queue_new(2,1537,16384);
   unsigned i;
   int result=RECORD_STREAM_VT_BUSY;
   assert(source);
   for(i=0;i<3074;i++) pcm[i]=1234;
   memset(&src,0,sizeof(src)); src.source=source; src.rate=48000; src.channels=2;
   setup(&client,&worker,&aac,&config); config.audio_fifo_frames=0;
   assert(!record_stream_vt_new_resampled(&worker,&aac,&client,&config,NULL));
   src.rate=44100;
   assert(!record_stream_vt_new_resampled(&worker,&aac,&client,&config,&src));
   src.rate=48000; src.accepted=1;
   assert(!record_stream_vt_new_resampled(&worker,&aac,&client,&config,&src));
   src.accepted=0;
   config.audio_fifo_frames=2048;
   assert(!record_stream_vt_new_resampled(&worker,&aac,&client,&config,&src));
   config.audio_fifo_frames=0;
   stream=record_stream_vt_new_resampled(&worker,&aac,&client,&config,&src); assert(stream);
   for(i=0;i<100;i++) assert(record_stream_vt_step(stream,0,&wait)>=0);
   assert(!aac.loan && !aac.consumed && !src.accepted);
   assert(record_audio_queue_push(source,pcm,1537,0)==RECORD_AUDIO_OK);
   assert(record_audio_queue_push(source,pcm,512,1537)==RECORD_AUDIO_OK);
   assert(record_audio_queue_finish(source));
   for(i=0;i<10000 && result!=RECORD_STREAM_VT_END;i++)
   { result=record_stream_vt_step(stream,0,&wait); assert(result>=0); }
   assert(result==RECORD_STREAM_VT_END && aac.consumed==4098 && aac.ended);
   assert(src.accepted==2049 && src.produced==4098 && !src.samples);
   assert(record_stream_vt_free(stream) && net_rtmp_session_free(client.session));
   assert(record_audio_queue_free(source));

   source=record_audio_queue_new(2,1537,16384); assert(source);
   assert(record_audio_queue_push(source,pcm,1537,0)==RECORD_AUDIO_OK);
   memset(&src,0,sizeof(src)); src.source=source; src.rate=48000; src.channels=2;
   setup(&client,&worker,&aac,&config); config.audio_fifo_frames=0;
   stream=record_stream_vt_new_resampled(&worker,&aac,&client,&config,&src); assert(stream);
   for(i=0;i<1000 && !src.samples;i++) assert(record_stream_vt_step(stream,0,&wait)>=0);
   assert(src.samples && src.accepted==512 && aac.consumed==1024);
   record_stream_vt_abort(stream);
   assert(record_stream_vt_step(stream,0,&wait)==RECORD_STREAM_VT_ERROR);
   assert(record_stream_vt_free(stream) && src.samples && !record_audio_queue_free(source));
   assert(record_at_resample_cancel(&src));
   assert(record_audio_queue_push(source,pcm,1,1537)==RECORD_AUDIO_OK);
   {
      const int16_t *samples;
      size_t frames;
      uint64_t timestamp;
      assert(record_audio_queue_peek_lend(source,&samples,&frames,&timestamp)==RECORD_AUDIO_OK);
      assert(frames==1 && timestamp==1537 && record_audio_queue_ack(source));
   }
   assert(record_audio_queue_finish(source) && record_audio_queue_free(source));
   assert(net_rtmp_session_free(client.session));
   puts("PASS resampled source/output clocks, direct AAC loans, empty input and caller-owned cancellation");
}
#ifdef VT_STREAM_THREAD_TEST
#include <pthread.h>
#include <sched.h>
struct vt_pcm_producer
{ record_stream_vt_t *stream; const int16_t *samples; record_audio_queue_t *source; };
static void *vt_pcm_produce(void *data)
{
   struct vt_pcm_producer *p=(struct vt_pcm_producer*)data;
   size_t sent=0;
   unsigned retries=0;
   while(sent<32769)
   {
      size_t count=32769-sent>257?257:32769-sent;
      int result=p->source?record_audio_queue_push(p->source,p->samples,count,sent)
         :record_stream_vt_audio(p->stream,p->samples,count);
      assert(result!=RECORD_STREAM_VT_ERROR);
      if(result==RECORD_STREAM_VT_BUSY)
      { assert(++retries<1000000); sched_yield(); }
      else { assert(result==RECORD_STREAM_VT_PROGRESS); sent+=count; retries=0; }
   }
   assert(p->source?record_audio_queue_finish(p->source):record_stream_vt_audio_finish(p->stream));
   return NULL;
}
#endif
#ifndef VT_STREAM_TEST_MAIN
#define VT_STREAM_TEST_MAIN main
#endif
int VT_STREAM_TEST_MAIN(int argc,char **argv)
{
   struct net_rtmp_client client;
   record_gpu_worker_vt_t worker;
   static record_at_audio_t aac;
   record_stream_vt_config_t cfg;
   record_stream_vt_t *stream;
   record_stream_vt_wait_t wait;
   static int16_t pcm[4096];
   unsigned i;
   int result;
   for(i=0;i<4096;i++) pcm[i]=1234;
   setup(&client,&worker,&aac,&cfg);
   cfg.audio_epoch_frames=2111; assert(!record_stream_vt_new(&worker,&aac,&client,&cfg));
   cfg.audio_epoch_frames=2112; cfg.video_epoch_ticks=2639; assert(!record_stream_vt_new(&worker,&aac,&client,&cfg));
   cfg.video_epoch_ticks=2640;
   stream=record_stream_vt_new(&worker,&aac,&client,&cfg); assert(stream && !record_stream_vt_free(stream));
   assert(record_stream_vt_audio(stream,pcm,2048)==RECORD_STREAM_VT_PROGRESS);
   assert(record_stream_vt_audio(stream,pcm,1)==RECORD_STREAM_VT_BUSY);
   assert(record_stream_vt_audio_finish(stream));
   for(i=0;i<1000 && !worker.held;i++) assert(record_stream_vt_step(stream,0,&wait)==RECORD_STREAM_VT_PROGRESS);
   assert(worker.held && !worker.releases); client.blocked=true;
   for(i=0;i<20;i++) assert(record_stream_vt_step(stream,0,&wait)!=RECORD_STREAM_VT_ERROR);
   assert(client.wire_live && worker.held && active_view->live && !worker.releases);
   client.blocked=false; result=RECORD_STREAM_VT_BUSY;
   for(i=0;i<10000 && result!=RECORD_STREAM_VT_END;i++)
   { result=record_stream_vt_step(stream,0,&wait); assert(result!=RECORD_STREAM_VT_ERROR); }
   assert(result==RECORD_STREAM_VT_END && worker.releases==2 && aac.ended && audio_releases==5);
   assert(record_stream_vt_free(stream) && net_rtmp_session_free(client.session));
   if(argc==2)
   {
      FILE *dump=fopen(argv[1],"wb");
      assert(dump && fwrite(wire,1,wire_size,dump)==wire_size && !fclose(dump));
   }
   setup(&client,&worker,&aac,&cfg); stream=record_stream_vt_new(&worker,&aac,&client,&cfg); assert(stream);
   assert(record_stream_vt_audio(stream,pcm,1025)==RECORD_STREAM_VT_PROGRESS && record_stream_vt_audio_finish(stream));
   result=RECORD_STREAM_VT_BUSY;
   for(i=0;i<10000 && result!=RECORD_STREAM_VT_END;i++)
   { result=record_stream_vt_step(stream,0,&wait); assert(result!=RECORD_STREAM_VT_ERROR); }
   assert(result==RECORD_STREAM_VT_END && aac.consumed==1025 && aac.ended);
   assert(record_stream_vt_free(stream) && net_rtmp_session_free(client.session));
   setup(&client,&worker,&aac,&cfg);
   stream=record_stream_vt_new(&worker,&aac,&client,&cfg); assert(stream);
   for(i=0;i<1000 && !worker.held;i++) assert(record_stream_vt_step(stream,0,&wait)==RECORD_STREAM_VT_PROGRESS);
   client.blocked=true;
   for(i=0;i<20;i++) assert(record_stream_vt_step(stream,0,&wait)!=RECORD_STREAM_VT_ERROR);
   assert(client.wire_live && worker.held && active_view->live);
   record_stream_vt_abort(stream); client.delay_discard=true;
   assert(record_stream_vt_step(stream,0,&wait)==RECORD_STREAM_VT_BUSY);
   assert(worker.held && active_view->live && !worker.releases && !record_stream_vt_free(stream));
   assert(record_stream_vt_step(stream,0,&wait)==RECORD_STREAM_VT_ERROR);
   assert(!worker.held && worker.releases==1 && !active_view->live && !record_stream_vt_free(stream));
   assert(record_stream_vt_audio_finish(stream) && record_stream_vt_free(stream));
   assert(net_rtmp_session_free(client.session));
   setup(&client,&worker,&aac,&cfg);
   cfg.audio_rate=aac.rate=44100; cfg.channels=aac.channels=1; aac.asc[0]=18; aac.asc[1]=8;
   cfg.video_epoch_ticks=2874;
   stream=record_stream_vt_new(&worker,&aac,&client,&cfg); assert(stream);
   assert(record_stream_vt_audio(stream,pcm,1025)==RECORD_STREAM_VT_PROGRESS && record_stream_vt_audio_finish(stream));
   for(i=0;i<1000 && !aac.held;i++) assert(record_stream_vt_step(stream,0,&wait)!=RECORD_STREAM_VT_ERROR);
   assert(aac.held); client.blocked=true;
   for(i=0;i<20;i++) assert(record_stream_vt_step(stream,0,&wait)!=RECORD_STREAM_VT_ERROR);
   assert(client.wire_live && aac.held);
   record_stream_vt_abort(stream); client.delay_discard=true;
   assert(record_stream_vt_step(stream,0,&wait)==RECORD_STREAM_VT_BUSY && aac.held);
   assert(record_stream_vt_step(stream,0,&wait)==RECORD_STREAM_VT_ERROR && !aac.held);
   assert(record_stream_vt_free(stream) && net_rtmp_session_free(client.session));
   setup(&client,&worker,&aac,&cfg); worker.release_fail=true;
   stream=record_stream_vt_new(&worker,&aac,&client,&cfg); assert(stream);
   assert(record_stream_vt_audio_finish(stream)); result=RECORD_STREAM_VT_BUSY;
   for(i=0;i<1000 && result!=RECORD_STREAM_VT_ERROR;i++) result=record_stream_vt_step(stream,0,&wait);
   assert(result==RECORD_STREAM_VT_ERROR && worker.held && !active_view->live && !worker.releases);
   assert(record_stream_vt_error(stream)==RECORD_STREAM_VT_ERROR_VIDEO);
   assert(record_stream_vt_step(stream,0,&wait)==RECORD_STREAM_VT_ERROR);
   assert(record_stream_vt_free(stream) && worker.held && net_rtmp_session_free(client.session));
   worker.held=false; /* Caller retires the failed native worker after teardown. */
#ifdef VT_STREAM_THREAD_TEST
   {
      unsigned queued;
      for(queued=0;queued<3;queued++)
      {
         pthread_t producer;
         struct vt_pcm_producer context;
         record_audio_queue_t *source=queued?record_audio_queue_new(2,257,8192):NULL;
         record_at_resample_t src;
         assert(!queued || source);
         setup(&client,&worker,&aac,&cfg);
         if(queued) cfg.audio_fifo_frames=0;
         memset(&src,0,sizeof(src)); src.source=source; src.rate=48000; src.channels=2;
         stream=queued==2?record_stream_vt_new_resampled(&worker,&aac,&client,&cfg,&src)
            :queued?record_stream_vt_new_queued(&worker,&aac,&client,&cfg,source)
            :record_stream_vt_new(&worker,&aac,&client,&cfg); assert(stream);
         context.stream=stream; context.samples=pcm; context.source=source;
         assert(!pthread_create(&producer,NULL,vt_pcm_produce,&context));
         result=RECORD_STREAM_VT_BUSY;
         for(i=0;i<1000000 && result!=RECORD_STREAM_VT_END;i++)
         {
            result=record_stream_vt_step(stream,0,&wait); assert(result!=RECORD_STREAM_VT_ERROR);
            if(result==RECORD_STREAM_VT_BUSY) sched_yield();
         }
         assert(result==RECORD_STREAM_VT_END && aac.consumed==(queued==2?65538u:32769u) && aac.ended);
         assert(!pthread_join(producer,NULL));
         assert(record_stream_vt_free(stream) && net_rtmp_session_free(client.session));
         assert(record_audio_queue_free(source));
         printf("PASS concurrent %s PCM producer and streaming I/O owner, 32769 source frames\n",
               queued==2?"resampled":queued?"queued":"internal-ring");
      }
   }
#endif
   queued_source();
   resampled_source();
   puts("PASS VT/AAC stream explicit priming epoch, network-held borrows, source continuity, bounded PCM, partial drain and cancellation retirement");
   return 0;
}
