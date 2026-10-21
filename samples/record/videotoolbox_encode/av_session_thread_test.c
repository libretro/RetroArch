/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_avfoundation_session.h"
#include "../../../record/record_mux_avfoundation.h"
#define COUNT 10000
struct fixture
{
   record_gpu_queue_t *queue;
   record_avfoundation_session_t *session;
   unsigned slot,submitted,videos,audios,video_calls,audio_calls,finishes,opened,destroyed,retired;
   bool packet;
};
static int ready(void *data,unsigned slot) { (void)data; assert(slot<2); return 1; }
static void *buffer(void *data,unsigned slot) { assert(slot<2); return data; }
static bool release(void *data,unsigned slot) { (void)data; assert(slot<2); return true; }
static int submit(void *data,unsigned slot,void *pixels,uint64_t pts,uint64_t duration)
{
   struct fixture *f=(struct fixture*)data;
   assert(!f->packet && pixels==data && duration==1001 && pts==(uint64_t)f->submitted*1001);
   f->submitted++; f->slot=slot; f->packet=true; return 1;
}
static int peek(void *data,unsigned *slot,void **sample)
{
   struct fixture *f=(struct fixture*)data;
   if (!f->packet) return 0;
   *slot=f->slot; *sample=&f->submitted; return 1;
}
static bool ack(void *data)
{ struct fixture *f=(struct fixture*)data; assert(f->packet); f->packet=false; return true; }
static int video_finish(void *data) { (void)data; return 1; }
static void *open_mux(void *data,void *sample)
{
   struct fixture *f=(struct fixture*)data;
   assert(!f->opened++ && sample==&f->submitted && f->submitted==1); return f;
}
static int video(void *data,void *sample)
{
   struct fixture *f=(struct fixture*)data;
   assert(sample==&f->submitted && *(unsigned*)sample==f->videos+1);
   if (++f->video_calls%3) return RECORD_AVF_BUSY;
   f->videos++; return RECORD_AVF_OK;
}
static int audio(void *data,const int16_t *samples,size_t frames,uint64_t pts)
{
   struct fixture *f=(struct fixture*)data;
   unsigned i;
   assert(frames==4 && pts==(uint64_t)f->audios*4);
   for (i=0;i<8;i++) assert(samples[i]==(int16_t)(f->audios%30000));
   if (++f->audio_calls%5) return RECORD_AVF_BUSY;
   f->audios++; return RECORD_AVF_OK;
}
static int finish(void *data)
{
   struct fixture *f=(struct fixture*)data;
   assert(f->videos==COUNT && f->audios==COUNT && !f->packet);
   return ++f->finishes<3?RECORD_AVF_BUSY:RECORD_AVF_OK;
}
static bool cancel(void *data) { (void)data; assert(0); return false; }
static bool free_mux(void *data)
{ struct fixture *f=(struct fixture*)data; assert(f->finishes==3); return true; }
static void destroy_encoder(void *data)
{ struct fixture *f=(struct fixture*)data; assert(!f->packet && !f->destroyed++); }
static bool destroy_context(void *data)
{ struct fixture *f=(struct fixture*)data; assert(f->destroyed==1 && !f->retired++); return true; }
static void *produce_video(void *data)
{
   struct fixture *f=(struct fixture*)data;
   unsigned i;
   for (i=0;i<COUNT;i++)
   {
      record_gpu_frame_t frame;
      int result;
      memset(&frame,0,sizeof(frame));
      while ((result=record_gpu_queue_acquire(f->queue,&frame.slot))==RECORD_GPU_QUEUE_BUSY) sched_yield();
      assert(result==RECORD_GPU_QUEUE_OK);
      frame.timestamp=(uint64_t)i*1001; frame.duration=1001;
      assert(record_gpu_queue_submit(f->queue,&frame));
   }
   assert(record_avfoundation_session_video_finish(f->session)); return NULL;
}
static void *produce_audio(void *data)
{
   struct fixture *f=(struct fixture*)data;
   unsigned i,j;
   for (i=0;i<COUNT;i++)
   {
      int result;
      int16_t pcm[8];
      for (j=0;j<8;j++) pcm[j]=(int16_t)(i%30000);
      while ((result=record_avfoundation_session_audio(f->session,pcm,4,(uint64_t)i*4))==RECORD_AUDIO_BUSY) sched_yield();
      assert(result==RECORD_AUDIO_OK);
   }
   assert(record_avfoundation_session_audio_finish(f->session)); return NULL;
}
static void *consume(void *data)
{
   struct fixture *f=(struct fixture*)data;
   int result;
   do
   {
      result=record_avfoundation_session_step(f->session);
      assert(result!=RECORD_AVF_WORKER_ERROR);
      if (result==RECORD_AVF_WORKER_BUSY) sched_yield();
   } while (result!=RECORD_AVF_WORKER_END);
   return NULL;
}
int main(void)
{
   struct fixture f;
   record_gpu_worker_vt_ops_t v;
   record_avfoundation_session_ops_t ops;
   record_gpu_worker_vt_t *worker;
   record_audio_queue_t *pcm;
   pthread_t video_thread,audio_thread,worker_thread;
   memset(&f,0,sizeof(f)); memset(&ops,0,sizeof(ops));
   f.queue=record_gpu_queue_new(2); pcm=record_audio_queue_new(2,4,256); assert(f.queue && pcm);
   v.ready=ready; v.buffer=buffer; v.release=release; v.submit=submit; v.peek=peek; v.ack=ack; v.finish=video_finish;
   worker=record_gpu_worker_vt_new(f.queue,&f,&f,&v); assert(worker);
   ops.open=open_mux; ops.mux.video=video; ops.mux.audio=audio; ops.mux.finish=finish;
   ops.cancel=cancel; ops.free_mux=free_mux; ops.destroy_encoder=destroy_encoder; ops.destroy_context=destroy_context;
   f.session=record_avfoundation_session_new(f.queue,worker,pcm,&f,&ops); assert(f.session);
   assert(!pthread_create(&video_thread,NULL,produce_video,&f));
   assert(!pthread_create(&audio_thread,NULL,produce_audio,&f));
   assert(!pthread_create(&worker_thread,NULL,consume,&f));
   assert(!pthread_join(video_thread,NULL) && !pthread_join(audio_thread,NULL) && !pthread_join(worker_thread,NULL));
   assert(f.videos==COUNT && f.audios==COUNT && f.opened==1 && f.destroyed==1 && f.retired==1);
   assert(record_avfoundation_session_free(f.session));
   puts("PASS: owned session three threads, 10000 video frames/PCM packets, independent BUSY, close and retirement");
   return 0;
}
