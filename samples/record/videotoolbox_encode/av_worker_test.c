/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_avfoundation_worker.h"
#include "../../../record/record_mux_avfoundation.h"
struct fixture
{
   unsigned slot,submits,videos,audios,video_calls,audio_calls,finish_calls,fail;
   unsigned video_busy,audio_busy,finish_busy;
   int sample;
   bool packet,with_audio;
   const int16_t *borrow;
};
static int ready(void *data,unsigned slot) { (void)data; assert(slot<2); return 1; }
static void *buffer(void *data,unsigned slot) { assert(slot<2); return data; }
static bool release(void *data,unsigned slot) { (void)data; assert(slot<2); return true; }
static int submit(void *data,unsigned slot,void *input,uint64_t pts,uint64_t duration)
{
   struct fixture *f=(struct fixture*)data;
   assert(input==data && !f->packet && duration==1001 && pts==(uint64_t)f->submits*1001);
   f->slot=slot; f->sample=(int)++f->submits; f->packet=true; return 1;
}
static int peek(void *data,unsigned *slot,void **sample)
{
   struct fixture *f=(struct fixture*)data;
   if (!f->packet) return 0;
   *slot=f->slot; *sample=&f->sample; return 1;
}
static bool ack(void *data)
{ struct fixture *f=(struct fixture*)data; assert(f->packet); f->packet=false; return true; }
static int finish_video(void *data) { (void)data; return 1; }
static int mux_video(void *data,void *sample)
{
   struct fixture *f=(struct fixture*)data;
   f->video_calls++; assert(sample==&f->sample && *(int*)sample==(int)f->videos+1);
   if (f->fail==1) return RECORD_AVF_ERROR;
   if (f->video_busy) { f->video_busy--; return RECORD_AVF_BUSY; }
   f->videos++; return RECORD_AVF_OK;
}
static int mux_audio(void *data,const int16_t *samples,size_t frames,uint64_t pts)
{
   struct fixture *f=(struct fixture*)data;
   unsigned i;
   f->audio_calls++; assert(f->with_audio && frames==4 && pts==(f->audios?200u:100u));
   if (f->borrow) assert(samples==f->borrow);
   for (i=0;i<8;i++) assert(samples[i]==(int16_t)(f->audios+1));
   if (f->fail==2) return RECORD_AVF_ERROR;
   if (f->audio_busy) { f->audio_busy--; f->borrow=samples; return RECORD_AVF_BUSY; }
   f->audios++; f->borrow=NULL; return RECORD_AVF_OK;
}
static int mux_finish(void *data)
{
   struct fixture *f=(struct fixture*)data;
   f->finish_calls++; assert(!f->packet && f->videos==2 && f->audios==(f->with_audio?2u:0u));
   if (f->fail==3) return RECORD_AVF_ERROR;
   if (f->finish_busy) { f->finish_busy--; return RECORD_AVF_BUSY; }
   return RECORD_AVF_OK;
}
static void run(unsigned failure,bool with_audio)
{
   struct fixture f;
   record_gpu_worker_vt_ops_t video_ops;
   record_avfoundation_worker_ops_t mux_ops;
   record_gpu_queue_t *queue=record_gpu_queue_new(2);
   record_gpu_worker_vt_t *video;
   record_audio_queue_t *audio=with_audio?record_audio_queue_new(2,4,64):NULL;
   record_avfoundation_worker_t *worker;
   int16_t pcm[8];
   unsigned i,j,calls;
   int result=RECORD_AVF_WORKER_BUSY;
   memset(&f,0,sizeof(f)); f.fail=failure; f.with_audio=with_audio;
   f.video_busy=2; f.audio_busy=3; f.finish_busy=2;
   video_ops.ready=ready; video_ops.buffer=buffer; video_ops.release=release;
   video_ops.submit=submit; video_ops.peek=peek; video_ops.ack=ack; video_ops.finish=finish_video;
   mux_ops.video=mux_video; mux_ops.audio=mux_audio; mux_ops.finish=mux_finish;
   assert(queue && (!with_audio || audio));
   video=record_gpu_worker_vt_new(queue,&f,&f,&video_ops); assert(video);
   for (i=0;i<2;i++)
   {
      record_gpu_frame_t frame;
      memset(&frame,0,sizeof(frame));
      assert(record_gpu_queue_acquire(queue,&frame.slot)==RECORD_GPU_QUEUE_OK);
      frame.timestamp=(uint64_t)i*1001; frame.duration=1001;
      assert(record_gpu_queue_submit(queue,&frame));
      for (j=0;j<8;j++) pcm[j]=(int16_t)(i+1);
      if (audio) assert(record_audio_queue_push(audio,pcm,4,i?200:100)==RECORD_AUDIO_OK);
   }
   assert(record_gpu_queue_finish(queue)); if (audio) assert(record_audio_queue_finish(audio));
   worker=record_avfoundation_worker_new(video,audio,&f,&mux_ops); assert(worker);
   /* Verify constructor copies the operation table rather than borrowing it. */
   memset(&mux_ops,0,sizeof(mux_ops));
   for (i=0;i<100;i++)
   {
      result=record_avfoundation_worker_step(worker);
      if (result==RECORD_AVF_WORKER_ERROR || result==RECORD_AVF_WORKER_END) break;
      assert(result==RECORD_AVF_WORKER_BUSY || result==RECORD_AVF_WORKER_PROGRESS);
      assert(!record_avfoundation_worker_free(worker));
   }
   assert(i<100 && result==(failure?RECORD_AVF_WORKER_ERROR:RECORD_AVF_WORKER_END));
   calls=f.video_calls+f.audio_calls+f.finish_calls;
   assert(record_avfoundation_worker_step(worker)==result);
   assert(calls==f.video_calls+f.audio_calls+f.finish_calls);
   assert(record_avfoundation_worker_free(worker));
   if (failure)
   {
      f.packet=false; /* Encoder teardown precedes input lease abandonment. */
      assert(record_gpu_worker_vt_abandon(video));
      if (audio)
      {
         const int16_t *samples;
         size_t frames;
         uint64_t pts;
         while (record_audio_queue_peek(audio,&samples,&frames,&pts)==RECORD_AUDIO_OK)
            assert(record_audio_queue_ack(audio));
      }
   }
   else
   {
      assert(record_gpu_worker_vt_free(video));
      assert(f.video_calls==4 && f.audio_calls==(with_audio?5u:0u) && f.finish_calls==3);
   }
   assert(record_gpu_queue_free(queue)); assert(record_audio_queue_free(audio));
}
int main(void)
{
   unsigned i;
   assert(!record_avfoundation_worker_new(NULL,NULL,NULL,NULL));
   assert(record_avfoundation_worker_step(NULL)==RECORD_AVF_WORKER_ERROR);
   assert(record_avfoundation_worker_free(NULL));
   run(0,true); run(0,false);
   for (i=1;i<=3;i++) run(i,true);
   puts("PASS: A/V worker fair BUSY service, stable borrows, source clocks, dual drain, finish polling and sticky failures");
   return 0;
}
