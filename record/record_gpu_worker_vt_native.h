/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RECORD_GPU_WORKER_VT_NATIVE_H
#define RECORD_GPU_WORKER_VT_NATIVE_H
#include "record_gpu_worker_vt.h"
#include "record_gpu_capture_metal.h"
#include "../gfx/common/videotoolbox_encode.h"
#ifdef VIDEOTOOLBOX_ENCODE_SLOTS
static INLINE int record_vt_native_ready(void *capture,unsigned slot)
{ return record_gpu_capture_metal_ready((record_gpu_capture_metal_t*)capture,slot); }
static INLINE void *record_vt_native_buffer(void *capture,unsigned slot)
{ return record_gpu_capture_metal_buffer((record_gpu_capture_metal_t*)capture,slot); }
static INLINE bool record_vt_native_release(void *capture,unsigned slot)
{ return record_gpu_capture_metal_release((record_gpu_capture_metal_t*)capture,slot); }
static INLINE VT_ENCODE_AVAILABLE int record_vt_native_submit(void *encoder,unsigned slot,
      void *buffer,uint64_t pts,uint64_t duration)
{
   return videotoolbox_encode_submit((videotoolbox_encode_t*)encoder,slot,
         (CVPixelBufferRef)buffer,(int64_t)pts,(int64_t)duration);
}
static INLINE int record_vt_native_peek(void *data,unsigned *slot,void **sample)
{
   videotoolbox_encode_t *encoder=(videotoolbox_encode_t*)data;
   const videotoolbox_encode_slot_t *packet;
   if (!encoder || encoder->error) return -1;
   packet=videotoolbox_encode_peek(encoder,slot);
   if (!packet) return 0;
   if (packet->status || !packet->sample || (packet->flags&kVTEncodeInfo_FrameDropped)) return -1;
   *sample=(void*)packet->sample; return 1;
}
static INLINE bool record_vt_native_ack(void *encoder)
{ return videotoolbox_encode_ack((videotoolbox_encode_t*)encoder); }
static INLINE VT_ENCODE_AVAILABLE int record_vt_native_finish(void *encoder)
{ return videotoolbox_encode_finish((videotoolbox_encode_t*)encoder)?1:-1; }
static INLINE VT_ENCODE_AVAILABLE record_gpu_worker_vt_t *record_gpu_worker_vt_new_native(
      record_gpu_queue_t *queue,record_gpu_capture_metal_t *capture,videotoolbox_encode_t *encoder)
{
   record_gpu_worker_vt_ops_t ops;
   ops.ready=record_vt_native_ready; ops.buffer=record_vt_native_buffer; ops.release=record_vt_native_release;
   ops.submit=record_vt_native_submit; ops.peek=record_vt_native_peek; ops.ack=record_vt_native_ack;
   ops.finish=record_vt_native_finish;
   return record_gpu_worker_vt_new(queue,capture,encoder,&ops);
}
#endif
#endif
