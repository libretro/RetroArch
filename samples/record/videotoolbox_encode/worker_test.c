/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_gpu_worker_vt.h"
typedef struct model
{
   int ready[16], values[16];
   unsigned slots[16], count, submits, acks, releases, finishes;
   unsigned submit_busy, ack_fail, release_fail, finish_busy;
   bool output;
} model_t;
static int ready(void *data,unsigned slot) { return ((model_t*)data)->ready[slot]; }
static void *buffer(void *data,unsigned slot) { return &((model_t*)data)->values[slot]; }
static bool release(void *data,unsigned slot)
{
   model_t *m=(model_t*)data;
   if (m->release_fail) { m->release_fail--; return false; }
   if (!m->ready[slot]) return false;
   m->ready[slot]=0; m->releases++; return true;
}
static int submit(void *data,unsigned slot,void *image,uint64_t pts,uint64_t duration)
{
   model_t *m=(model_t*)data;
   assert(image==&m->values[slot] && pts==slot*1001 && duration==1001);
   if (m->submit_busy) { m->submit_busy--; return 0; }
   m->slots[m->count++]=slot; m->submits++; return 1;
}
static int peek(void *data,unsigned *slot,void **sample)
{
   model_t *m=(model_t*)data;
   if (!m->output || !m->count) return 0;
   *slot=m->slots[0]; *sample=&m->values[*slot]; return 1;
}
static bool ack(void *data)
{
   model_t *m=(model_t*)data;
   if (m->ack_fail) { m->ack_fail--; return false; }
   assert(m->count); m->count--; m->acks++;
   memmove(m->slots,m->slots+1,m->count*sizeof(m->slots[0])); return true;
}
static int finish(void *data)
{
   model_t *m=(model_t*)data;
   if (m->finish_busy) { m->finish_busy--; return 0; }
   m->finishes++; m->output=true; return 1;
}
static void publish(record_gpu_queue_t *q,unsigned count)
{
   unsigned i;
   for (i=0;i<count;i++)
   {
      record_gpu_frame_t frame;
      memset(&frame,0,sizeof(frame));
      assert(record_gpu_queue_acquire(q,&frame.slot)==RECORD_GPU_QUEUE_OK);
      frame.timestamp=(uint64_t)frame.slot*1001; frame.duration=1001;
      assert(record_gpu_queue_submit(q,&frame));
   }
   assert(record_gpu_queue_finish(q));
}
static unsigned destroyed;
static void destroy_context(void *context) { assert(context==&destroyed); destroyed++; }
int main(void)
{
   record_gpu_worker_vt_ops_t ops;
   record_gpu_worker_vt_t *w;
   record_gpu_queue_t *q;
   model_t m;
   void *sample, *borrow;
   unsigned i, packets=0;
   int result=0;
   memset(&m,0,sizeof(m));
   ops.ready=ready; ops.buffer=buffer; ops.release=release;
   ops.submit=submit; ops.peek=peek; ops.ack=ack; ops.finish=finish;
   q=record_gpu_queue_new(2); assert(q);
   assert(!record_gpu_worker_vt_new(q,&m,&m,NULL));
   w=record_gpu_worker_vt_new_owned(q,&m,&m,&ops,&destroyed,destroy_context); assert(w);
   assert(!destroyed);
   publish(q,2);
   assert(record_gpu_worker_vt_step(w,&sample)==RECORD_GPU_VT_PROGRESS);
   assert(record_gpu_worker_vt_step(w,&sample)==RECORD_GPU_VT_BUSY && !m.submits);
   assert(!record_gpu_worker_vt_free(w) && !record_gpu_queue_free(q));
   m.ready[0]=m.ready[1]=1; m.submit_busy=1; m.finish_busy=1;
   assert(record_gpu_worker_vt_step(w,&sample)==RECORD_GPU_VT_BUSY);
   for (i=0;i<30;i++)
   {
      result=record_gpu_worker_vt_step(w,&sample);
      assert(result!=RECORD_GPU_VT_ERROR);
      if (result==RECORD_GPU_VT_END) break;
      if (result!=RECORD_GPU_VT_PACKET) continue;
      borrow=sample;
      assert(record_gpu_worker_vt_step(w,&sample)==RECORD_GPU_VT_PACKET && sample==borrow);
      if (!packets)
      {
         m.ack_fail=1; assert(!record_gpu_worker_vt_release(w));
         assert(record_gpu_worker_vt_step(w,&sample)==RECORD_GPU_VT_PACKET && sample==borrow);
         m.release_fail=1; assert(!record_gpu_worker_vt_release(w));
         assert(record_gpu_worker_vt_step(w,&sample)==RECORD_GPU_VT_ERROR && !sample);
      }
      assert(record_gpu_worker_vt_release(w)); packets++;
   }
   assert(result==RECORD_GPU_VT_END && packets==2 && m.submits==2 && m.acks==2
         && m.releases==2 && m.finishes==1);
   assert(record_gpu_worker_vt_free(w)); assert(destroyed==1); assert(record_gpu_queue_free(q));
   /* Cancellation holds GPU-pending inputs and makes no more encoder calls. */
   memset(&m,0,sizeof(m)); q=record_gpu_queue_new(2); assert(q);
   w=record_gpu_worker_vt_new_owned(q,&m,&m,&ops,&destroyed,destroy_context); assert(w); publish(q,2);
   assert(record_gpu_worker_vt_step(w,&sample)==RECORD_GPU_VT_PROGRESS);
   assert(!record_gpu_worker_vt_abandon(w)); assert(destroyed==1);
   assert(record_gpu_worker_vt_step(w,&sample)==RECORD_GPU_VT_ERROR);
   m.ready[0]=1; m.ready[1]=-1;
   assert(record_gpu_worker_vt_abandon(w)); assert(destroyed==2); assert(!m.submits && !m.finishes);
   assert(record_gpu_queue_free(q));
   q=record_gpu_queue_new(2); assert(q);
   assert(!record_gpu_worker_vt_new_owned(q,&m,&m,NULL,&destroyed,destroy_context));
   assert(destroyed==2);
   w=record_gpu_worker_vt_new_owned(q,&m,&m,&ops,&destroyed,destroy_context); assert(w);
   assert(record_gpu_worker_vt_free(w) && destroyed==3); assert(record_gpu_queue_free(q));
   puts("PASS: VT worker GPU readiness, encoder BUSY, output borrow, partial release, drain and cancellation");
   return 0;
}
