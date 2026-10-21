/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <retro_atomic.h>
#include "../../../record/record_gpu_worker_vt.h"
#define FRAMES 5000
typedef struct model
{
   record_gpu_queue_t *queue;
   record_gpu_worker_vt_t *worker;
   retro_atomic_int_t ready[2];
   uint64_t values[2];
   unsigned slots[2], count, tries, polls, acknowledgements, releases, received;
} model_t;
static int ready(void *data,unsigned slot)
{ return retro_atomic_load_acquire_int(&((model_t*)data)->ready[slot]); }
static void *buffer(void *data,unsigned slot) { return &((model_t*)data)->values[slot]; }
static bool release(void *data,unsigned slot)
{
   model_t *m=(model_t*)data;
   if (!(++m->releases%11)) return false;
   assert(retro_atomic_load_acquire_int(&m->ready[slot])==1);
   retro_atomic_store_release_int(&m->ready[slot],0); return true;
}
static int submit(void *data,unsigned slot,void *image,uint64_t pts,uint64_t duration)
{
   model_t *m=(model_t*)data;
   assert(image==&m->values[slot] && *(uint64_t*)image==pts && duration==1001);
   if (++m->tries&1) return 0;
   assert(m->count<2); m->slots[m->count++]=slot; return 1;
}
static int peek(void *data,unsigned *slot,void **sample)
{
   model_t *m=(model_t*)data;
   if (!m->count || ++m->polls%3) return 0;
   *slot=m->slots[0]; *sample=&m->values[*slot]; return 1;
}
static bool ack(void *data)
{
   model_t *m=(model_t*)data;
   if (!(++m->acknowledgements%5)) return false;
   assert(m->count); m->count--; m->slots[0]=m->slots[1]; return true;
}
static int finish(void *data) { (void)data; return 1; }
static void *produce(void *data)
{
   model_t *m=(model_t*)data;
   unsigned i;
   for (i=0;i<FRAMES;i++)
   {
      record_gpu_frame_t frame;
      int result;
      memset(&frame,0,sizeof(frame));
      do { result=record_gpu_queue_acquire(m->queue,&frame.slot); if (!result) sched_yield(); }
      while (result==RECORD_GPU_QUEUE_BUSY);
      assert(result==RECORD_GPU_QUEUE_OK);
      assert(!retro_atomic_load_acquire_int(&m->ready[frame.slot]));
      m->values[frame.slot]=(uint64_t)i*1001;
      frame.timestamp=m->values[frame.slot]; frame.duration=1001;
      assert(record_gpu_queue_submit(m->queue,&frame));
      if (i%7==0) sched_yield(); /* Expose descriptor-before-GPU-completion. */
      retro_atomic_store_release_int(&m->ready[frame.slot],1);
   }
   assert(record_gpu_queue_finish(m->queue)); return NULL;
}
static void *consume(void *data)
{
   model_t *m=(model_t*)data;
   int result;
   do
   {
      void *sample=NULL;
      result=record_gpu_worker_vt_step(m->worker,&sample);
      assert(result!=RECORD_GPU_VT_ERROR);
      if (result==RECORD_GPU_VT_PACKET)
      {
         assert(*(uint64_t*)sample==(uint64_t)m->received*1001);
         while (!record_gpu_worker_vt_release(m->worker)) sched_yield();
         m->received++;
      }
      else if (result==RECORD_GPU_VT_BUSY) sched_yield();
   } while (result!=RECORD_GPU_VT_END);
   return NULL;
}
int main(void)
{
   model_t m;
   record_gpu_worker_vt_ops_t ops;
   pthread_t producer,worker;
   memset(&m,0,sizeof(m));
   retro_atomic_int_init(&m.ready[0],0); retro_atomic_int_init(&m.ready[1],0);
   ops.ready=ready; ops.buffer=buffer; ops.release=release;
   ops.submit=submit; ops.peek=peek; ops.ack=ack; ops.finish=finish;
   m.queue=record_gpu_queue_new(2); assert(m.queue);
   m.worker=record_gpu_worker_vt_new(m.queue,&m,&m,&ops); assert(m.worker);
   assert(!pthread_create(&worker,NULL,consume,&m));
   assert(!pthread_create(&producer,NULL,produce,&m));
   assert(!pthread_join(producer,NULL)); assert(!pthread_join(worker,NULL));
   assert(m.received==FRAMES && !m.count);
   assert(record_gpu_worker_vt_free(m.worker)); assert(record_gpu_queue_free(m.queue));
   puts("PASS: 5000 two-slot VT worker frames, delayed GPU publication, BUSY and partial release");
   return 0;
}
