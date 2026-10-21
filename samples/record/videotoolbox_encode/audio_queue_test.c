/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_audio_queue.h"
#ifdef RECORD_AUDIO_THREAD_TEST
#include <pthread.h>
#define PACKETS 20000
static record_audio_queue_t *shared;
static void *produce(void *unused)
{
   unsigned packet,frame,channel;
   uint64_t timestamp=123;
   int16_t pcm[23*6];
   (void)unused;
   for (packet=0;packet<PACKETS;packet++)
   {
      size_t frames=1+packet%23;
      int result;
      if (!(packet%17)) timestamp+=3;
      for (frame=0;frame<frames;frame++)
         for (channel=0;channel<6;channel++)
            pcm[frame*6+channel]=(int16_t)(packet*7+frame*6+channel);
      do { result=record_audio_queue_push(shared,pcm,frames,timestamp); }
      while (result==RECORD_AUDIO_BUSY);
      assert(result==RECORD_AUDIO_OK); timestamp+=frames;
   }
   assert(record_audio_queue_finish(shared)); return NULL;
}
static void stress(void)
{
   pthread_t producer;
   unsigned packet,frame,channel;
   uint64_t timestamp=123,pts;
   const int16_t *pcm,*again;
   size_t frames,repeated;
   uint64_t same_pts;
   int result;
   shared=record_audio_queue_new(6,23,2*(16+23*12)); assert(shared);
   assert(!pthread_create(&producer,NULL,produce,NULL));
   for (packet=0;packet<PACKETS;packet++)
   {
      do { result=record_audio_queue_peek(shared,&pcm,&frames,&pts); }
      while (result==RECORD_AUDIO_BUSY);
      assert(result==RECORD_AUDIO_OK);
      if (!(packet%17)) timestamp+=3;
      assert(frames==1+packet%23 && pts==timestamp);
      for (frame=0;frame<frames;frame++)
         for (channel=0;channel<6;channel++)
            assert(pcm[frame*6+channel]==(int16_t)(packet*7+frame*6+channel));
      assert(record_audio_queue_peek(shared,&again,&repeated,&same_pts)==RECORD_AUDIO_OK);
      assert(again==pcm && repeated==frames && same_pts==pts);
      assert(record_audio_queue_ack(shared)); timestamp+=frames;
   }
   assert(!pthread_join(producer,NULL));
   assert(record_audio_queue_peek(shared,&pcm,&frames,&pts)==RECORD_AUDIO_END);
   assert(record_audio_queue_free(shared));
   puts("PASS: 20000 PCM packets, six channels, wrap/BUSY, stable borrows, gaps and threaded close");
}
#endif
int main(void)
{
   record_audio_queue_t *q;
   const int16_t *pcm,*again;
   size_t frames,repeat;
   uint64_t pts,same_pts;
   int16_t source[8]={1,2,3,4,5,6,7,8};
   unsigned i;
   assert(!record_audio_queue_new(0,4,32));
   assert(!record_audio_queue_new(9,4,32));
   assert(!record_audio_queue_new(2,0,32));
   assert(!record_audio_queue_new(2,4,31));
   assert(!record_audio_queue_new(2,4,(size_t)-1));
   q=record_audio_queue_new(2,4,33); assert(q);
   assert(record_audio_queue_peek(q,&pcm,&frames,&pts)==RECORD_AUDIO_BUSY);
   assert(!pcm && !frames && !pts && !record_audio_queue_ack(q));
   assert(record_audio_queue_push(q,source,4,100)==RECORD_AUDIO_OK);
   memset(source,0,sizeof(source));
   /* Rounded capacity is 64, but the explicit 33-byte budget permits no
    * second packet. A refusal must not advance its timestamp. */
   assert(record_audio_queue_push(q,source,4,104)==RECORD_AUDIO_BUSY);
   assert(!record_audio_queue_free(q));
   assert(record_audio_queue_peek(q,&pcm,&frames,&pts)==RECORD_AUDIO_OK);
   assert(frames==4 && pts==100);
   for (i=0;i<8;i++) assert(pcm[i]==(int16_t)(i+1));
   assert(record_audio_queue_push(q,source,4,104)==RECORD_AUDIO_OK);
   assert(record_audio_queue_peek(q,&again,&repeat,&same_pts)==RECORD_AUDIO_OK);
   assert(pcm==again && repeat==4 && same_pts==100 && pcm[0]==1);
   assert(record_audio_queue_push(q,source,1,103)==RECORD_AUDIO_ERROR);
   assert(record_audio_queue_push(q,source,5,108)==RECORD_AUDIO_ERROR);
   assert(record_audio_queue_push(q,source,1,((uint64_t)1<<63)-1)==RECORD_AUDIO_ERROR);
   assert(record_audio_queue_finish(q) && record_audio_queue_finish(q));
   assert(record_audio_queue_push(q,source,1,108)==RECORD_AUDIO_ERROR);
   assert(!record_audio_queue_free(q)); assert(record_audio_queue_ack(q));
   assert(record_audio_queue_peek(q,&pcm,&frames,&pts)==RECORD_AUDIO_OK);
   assert(frames==4 && pts==104);
   for (i=0;i<8;i++) assert(!pcm[i]);
   assert(!record_audio_queue_free(q)); assert(record_audio_queue_ack(q));
   assert(record_audio_queue_peek(q,&pcm,&frames,&pts)==RECORD_AUDIO_END);
   assert(!pcm && !frames && !pts && record_audio_queue_free(q));
   q=record_audio_queue_new(8,65536,1048592); assert(q); assert(record_audio_queue_free(q));
   assert(record_audio_queue_free(NULL) && !record_audio_queue_finish(NULL));
   puts("PASS: PCM whole-packet budget, copied source, retained borrow, timestamp overflow/overlap and EOF");
#ifdef RECORD_AUDIO_THREAD_TEST
   stress();
#endif
   return 0;
}
