/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_gpu_worker_vt.h"
#include <stdlib.h>
struct record_gpu_worker_vt
{
   record_gpu_queue_t *queue;
   void *capture, *encoder, *sample, *owned_context;
   void (*destroy)(void*);
   record_gpu_worker_vt_ops_t ops;
   record_gpu_frame_t frame;
   uint32_t owned, retired;
   unsigned packet_slot, release_stage;
   bool pending, packet, started, input_end, eos, ended, abandoning;
};
record_gpu_worker_vt_t *record_gpu_worker_vt_new(record_gpu_queue_t *queue,
      void *capture,void *encoder,const record_gpu_worker_vt_ops_t *ops)
{
   record_gpu_worker_vt_t *w;
   if (!queue || !capture || !encoder || !ops || !ops->ready || !ops->buffer
         || !ops->release || !ops->submit || !ops->peek || !ops->ack || !ops->finish) return NULL;
   w=(record_gpu_worker_vt_t*)calloc(1,sizeof(*w));
   if (w) { w->queue=queue; w->capture=capture; w->encoder=encoder; w->ops=*ops; }
   return w;
}
record_gpu_worker_vt_t *record_gpu_worker_vt_new_owned(record_gpu_queue_t *queue,
      void *capture,void *encoder,const record_gpu_worker_vt_ops_t *ops,
      void *owned_context,void (*destroy)(void*))
{
   record_gpu_worker_vt_t *w;
   if (!destroy) return NULL;
   w=record_gpu_worker_vt_new(queue,capture,encoder,ops);
   if (w) { w->owned_context=owned_context; w->destroy=destroy; }
   return w;
}
static void record_gpu_worker_vt_destroy(record_gpu_worker_vt_t *w)
{ if (w->destroy) w->destroy(w->owned_context); free(w); }
int record_gpu_worker_vt_step(record_gpu_worker_vt_t *w,void **sample)
{
   int result;
   unsigned slot;
   void *buffer;
   bool progress=false;
   if (sample) *sample=NULL;
   if (!w || !sample || w->abandoning) return RECORD_GPU_VT_ERROR;
   if (w->ended) return RECORD_GPU_VT_END;
   if (w->packet)
   {
      if (w->release_stage) return RECORD_GPU_VT_ERROR;
      *sample=w->sample; return RECORD_GPU_VT_PACKET;
   }
   result=w->ops.peek(w->encoder,&slot,&w->sample);
   if (result<0) return RECORD_GPU_VT_ERROR;
   if (result>0)
   {
      if (slot>=RECORD_GPU_QUEUE_SLOTS || !(w->owned&((uint32_t)1<<slot)) || !w->sample)
         return RECORD_GPU_VT_ERROR;
      w->packet_slot=slot; w->packet=true; *sample=w->sample;
      return RECORD_GPU_VT_PACKET;
   }
   if (w->eos && !w->owned && !w->pending)
   { w->ended=true; return RECORD_GPU_VT_END; }
   if (!w->pending && !w->input_end)
   {
      result=record_gpu_queue_take(w->queue,&w->frame);
      if (result==RECORD_GPU_QUEUE_ERROR) return RECORD_GPU_VT_ERROR;
      if (result==RECORD_GPU_QUEUE_OK)
      {
         if (w->frame.slot>=RECORD_GPU_QUEUE_SLOTS) return RECORD_GPU_VT_ERROR;
         w->owned|=(uint32_t)1<<w->frame.slot;
         w->pending=w->started=true; progress=true;
      }
      else if (result==RECORD_GPU_QUEUE_END) { w->input_end=true; progress=true; }
   }
   if (w->pending)
   {
      result=w->ops.ready(w->capture,w->frame.slot);
      if (result<0) return RECORD_GPU_VT_ERROR;
      if (result>0)
      {
         buffer=w->ops.buffer(w->capture,w->frame.slot);
         if (!buffer || !w->frame.duration || w->frame.timestamp>((uint64_t)1<<63)-1
               || w->frame.duration>((uint64_t)1<<63)-1-w->frame.timestamp) return RECORD_GPU_VT_ERROR;
         result=w->ops.submit(w->encoder,w->frame.slot,buffer,w->frame.timestamp,w->frame.duration);
         if (result<0) return RECORD_GPU_VT_ERROR;
         if (result>0) { w->pending=false; progress=true; }
      }
   }
   if (w->input_end && !w->pending && !w->eos)
   {
      result=w->ops.finish(w->encoder);
      if (result<0) return RECORD_GPU_VT_ERROR;
      if (result>0) { w->eos=true; progress=true; }
   }
   return progress?RECORD_GPU_VT_PROGRESS:RECORD_GPU_VT_BUSY;
}
bool record_gpu_worker_vt_release(record_gpu_worker_vt_t *w)
{
   if (!w || !w->packet || w->abandoning) return false;
   if (!w->release_stage)
   {
      if (!w->ops.ack(w->encoder)) return false;
      w->sample=NULL; w->release_stage=1;
   }
   if (w->release_stage==1)
   {
      if (!w->ops.release(w->capture,w->packet_slot)) return false;
      w->retired|=(uint32_t)1<<w->packet_slot;
      w->release_stage=2;
   }
   if (!record_gpu_queue_release(w->queue,w->packet_slot)) return false;
   w->owned&=~((uint32_t)1<<w->packet_slot);
   w->retired&=~((uint32_t)1<<w->packet_slot);
   w->packet=false; w->release_stage=0; return true;
}
bool record_gpu_worker_vt_free(record_gpu_worker_vt_t *w)
{
   if (!w) return true;
   if (w->owned || w->retired || w->pending || w->packet || w->abandoning || (w->started && !w->ended)) return false;
   record_gpu_worker_vt_destroy(w); return true;
}
bool record_gpu_worker_vt_abandon(record_gpu_worker_vt_t *w)
{
   unsigned slot, count;
   int result;
   bool all=true;
   if (!w) return true;
   w->abandoning=true; w->encoder=NULL;
   for (count=0;count<=RECORD_GPU_QUEUE_SLOTS;count++)
   {
      result=record_gpu_queue_take(w->queue,&w->frame);
      if (result!=RECORD_GPU_QUEUE_OK) break;
      w->owned|=(uint32_t)1<<w->frame.slot;
   }
   if (result!=RECORD_GPU_QUEUE_END) all=false;
   for (slot=0;slot<RECORD_GPU_QUEUE_SLOTS;slot++)
      if (w->owned&((uint32_t)1<<slot))
      {
         if (!(w->retired&((uint32_t)1<<slot)))
         {
            if (!w->ops.ready(w->capture,slot) || !w->ops.release(w->capture,slot))
            { all=false; continue; }
            w->retired|=(uint32_t)1<<slot;
         }
         if (!record_gpu_queue_release(w->queue,slot)) { all=false; continue; }
         w->owned&=~((uint32_t)1<<slot);
         w->retired&=~((uint32_t)1<<slot);
      }
   if (!all || w->owned) return false;
   record_gpu_worker_vt_destroy(w); return true;
}

bool record_gpu_worker_vt_retry_needed(const record_gpu_worker_vt_t *w)
{ return w && !w->ended && (w->pending || w->packet || w->owned || w->input_end || w->abandoning); }
