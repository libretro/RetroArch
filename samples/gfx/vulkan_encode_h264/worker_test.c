/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_gpu_worker_vulkan.h"
struct vulkan_encode_h264
{
   vulkan_encode_packet_t packets[16];
   unsigned head,count,submits,polls,releases,finishes;
   int submit_fault,poll_fault,finish_fault;
   bool done,locked,closed,release_fault,dead,invalid_slot;
};
int vulkan_encode_h264_submit(vulkan_encode_h264_t *e,unsigned slot,
      uint64_t pts,uint64_t duration,uint64_t ready,bool keyframe)
{
   vulkan_encode_packet_t *p;
   int fault=e->submit_fault;
   assert(!e->dead); e->submits++; e->submit_fault=0;
   if (fault) return fault;
   assert(!e->closed && e->count<16 && ready==pts+7 && duration==3);
   p=&e->packets[(e->head+e->count++)%16]; p->data="packet"; p->size=6;
   p->slot=slot; p->timestamp=pts; p->duration=duration; p->keyframe=keyframe;
   return VULKAN_ENCODE_OK;
}
int vulkan_encode_h264_poll(vulkan_encode_h264_t *e,vulkan_encode_packet_t *p)
{
   assert(!e->dead); e->polls++;
   if (e->poll_fault) return e->poll_fault;
   if (!e->count) return e->closed ? VULKAN_ENCODE_END : VULKAN_ENCODE_BUSY;
   if (!e->done) return VULKAN_ENCODE_BUSY;
   *p=e->packets[e->head]; e->locked=true;
   if (e->invalid_slot) p->slot=16;
   return VULKAN_ENCODE_OK;
}
bool vulkan_encode_h264_release(vulkan_encode_h264_t *e)
{
   assert(!e->dead); e->releases++;
   if (e->release_fault) return false;
   assert(e->locked && e->count); e->count--; e->head=(e->head+1)%16; e->locked=false;
   return true;
}
int vulkan_encode_h264_finish(vulkan_encode_h264_t *e)
{
   assert(!e->dead); e->finishes++;
   if (e->finish_fault) return e->finish_fault;
   e->closed=true; return VULKAN_ENCODE_OK;
}
static void publish(record_gpu_queue_t *q,unsigned ordinal)
{
   record_gpu_frame_t frame;
   assert(record_gpu_queue_acquire(q,&frame.slot)==RECORD_GPU_QUEUE_OK);
   frame.timestamp=ordinal*10; frame.duration=3; frame.ready_value=frame.timestamp+7;
   frame.keyframe=!ordinal; assert(record_gpu_queue_submit(q,&frame));
}
static void normal(void)
{
   vulkan_encode_h264_t e;
   record_gpu_queue_t *q;
   record_gpu_worker_vulkan_t *w;
   vulkan_encode_packet_t packet,held;
   unsigned slot,i,calls;
   memset(&e,0,sizeof(e)); q=record_gpu_queue_new(2); assert(q);
   assert(!record_gpu_worker_vulkan_new(NULL,&e) && !record_gpu_worker_vulkan_new(q,NULL));
   w=record_gpu_worker_vulkan_new(q,&e); assert(w);
   assert(record_gpu_worker_vulkan_step(w,&packet)==RECORD_GPU_VULKAN_BUSY);
   assert(!record_gpu_worker_vulkan_retry_needed(w)); publish(q,0); publish(q,1);
   e.submit_fault=VULKAN_ENCODE_BUSY;
   assert(record_gpu_worker_vulkan_step(w,&packet)==RECORD_GPU_VULKAN_PROGRESS && !e.count);
   assert(record_gpu_worker_vulkan_retry_needed(w));
   assert(record_gpu_queue_acquire(q,&slot)==RECORD_GPU_QUEUE_BUSY);
   assert(record_gpu_worker_vulkan_step(w,&packet)==RECORD_GPU_VULKAN_PROGRESS && e.count==1);
   assert(record_gpu_worker_vulkan_step(w,&packet)==RECORD_GPU_VULKAN_PROGRESS && e.count==2);
   assert(!record_gpu_worker_vulkan_free(w) && !record_gpu_queue_free(q));
   e.done=true;
   for (i=0;i<2;i++)
   {
      assert(record_gpu_worker_vulkan_step(w,&packet)==RECORD_GPU_VULKAN_PACKET);
      assert(packet.timestamp==i*10 && packet.duration==3 && packet.slot==i && packet.keyframe==!i);
      calls=e.polls; assert(record_gpu_worker_vulkan_step(w,&held)==RECORD_GPU_VULKAN_PACKET);
      assert(calls==e.polls && packet.data==held.data && packet.size==held.size);
      if (!i)
      {
         assert(record_gpu_queue_acquire(q,&slot)==RECORD_GPU_QUEUE_BUSY);
         e.release_fault=true; assert(!record_gpu_worker_vulkan_release(w) && e.count==2 && e.locked);
         e.release_fault=false;
      }
      assert(record_gpu_worker_vulkan_release(w) && !record_gpu_worker_vulkan_release(w));
      if (!i)
      {
         assert(record_gpu_queue_acquire(q,&slot)==RECORD_GPU_QUEUE_OK && slot==0);
         assert(record_gpu_queue_cancel(q));
      }
   }
   assert(record_gpu_queue_finish(q));
   assert(record_gpu_worker_vulkan_step(w,&packet)==RECORD_GPU_VULKAN_PROGRESS && e.finishes==1);
   assert(record_gpu_worker_vulkan_step(w,&packet)==RECORD_GPU_VULKAN_END);
   calls=e.polls; assert(record_gpu_worker_vulkan_step(w,&packet)==RECORD_GPU_VULKAN_END && calls==e.polls);
   assert(!record_gpu_worker_vulkan_retry_needed(w));
   assert(record_gpu_worker_vulkan_free(w) && record_gpu_queue_free(q));
}
static void failures(void)
{
   vulkan_encode_h264_t e;
   record_gpu_queue_t *q;
   record_gpu_worker_vulkan_t *w;
   vulkan_encode_packet_t packet;
   unsigned mode,calls;
   for (mode=0;mode<5;mode++)
   {
      memset(&e,0,sizeof(e)); q=record_gpu_queue_new(16); assert(q);
      w=record_gpu_worker_vulkan_new(q,&e); assert(w);
      publish(q,0); publish(q,1);
      if (!mode) e.submit_fault=VULKAN_ENCODE_ERROR;
      if (mode==4) e.submit_fault=VULKAN_ENCODE_BUSY;
      if (!mode) assert(record_gpu_worker_vulkan_step(w,&packet)==RECORD_GPU_VULKAN_ERROR);
      else
      {
         assert(record_gpu_worker_vulkan_step(w,&packet)==RECORD_GPU_VULKAN_PROGRESS);
         if (mode==1) e.poll_fault=VULKAN_ENCODE_ERROR;
         if (mode==4) e.poll_fault=VULKAN_ENCODE_END;
         if (mode==2)
            e.done=e.invalid_slot=true;
         if (mode==3)
         {
            assert(record_gpu_worker_vulkan_step(w,&packet)==RECORD_GPU_VULKAN_PROGRESS);
            assert(record_gpu_queue_finish(q)); e.finish_fault=VULKAN_ENCODE_ERROR;
         }
         assert(record_gpu_worker_vulkan_step(w,&packet)==RECORD_GPU_VULKAN_ERROR);
      }
      calls=e.polls+e.submits+e.finishes;
      assert(record_gpu_worker_vulkan_step(w,&packet)==RECORD_GPU_VULKAN_ERROR);
      assert(calls==e.polls+e.submits+e.finishes && !record_gpu_worker_vulkan_free(w));
      /* Simulate retired producer GPU work and destroyed encoder. */
      e.dead=true; assert(record_gpu_queue_finish(q));
      assert(record_gpu_worker_vulkan_abandon(w) && record_gpu_queue_free(q));
   }
}
static void empty(void)
{
   vulkan_encode_h264_t e;
   record_gpu_queue_t *q;
   record_gpu_worker_vulkan_t *w;
   vulkan_encode_packet_t packet;
   memset(&e,0,sizeof(e)); q=record_gpu_queue_new(1); assert(q);
   w=record_gpu_worker_vulkan_new(q,&e); assert(w);
   assert(!record_gpu_worker_vulkan_release(w));
   assert(record_gpu_worker_vulkan_step(w,NULL)==RECORD_GPU_VULKAN_ERROR);
   assert(record_gpu_worker_vulkan_free(w));
   w=record_gpu_worker_vulkan_new(q,&e); assert(w && record_gpu_queue_finish(q));
   assert(record_gpu_worker_vulkan_step(w,&packet)==RECORD_GPU_VULKAN_PROGRESS);
   assert(record_gpu_worker_vulkan_step(w,&packet)==RECORD_GPU_VULKAN_END);
   assert(!e.submits && e.finishes==1 && record_gpu_worker_vulkan_free(w) && record_gpu_queue_free(q));
}
int main(void)
{
   normal(); failures(); empty(); puts("Vulkan recording worker borrow/backpressure/EOS/sticky failures PASS"); return 0;
}
