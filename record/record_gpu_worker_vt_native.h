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
/* Cold visible-rectangle binding. Low-level encoder dimensions remain the
 * physical surface size. Context ownership belongs to the generic worker. */
#include "../gfx/common/videotoolbox_encode_crop.h"
typedef struct record_vt_visible
{
   videotoolbox_encode_t *encoder;
   record_gpu_worker_vt_ops_t base;
   CMVideoFormatDescriptionRef original,format;
   CMSampleBufferRef sample;
   unsigned width,height,slot;
   bool error;
} record_vt_visible_t;
static INLINE void record_vt_visible_destroy(void *data)
{
   record_vt_visible_t *c=(record_vt_visible_t*)data;
   if (c->sample) CFRelease(c->sample);
   if (c->original) CFRelease(c->original);
   if (c->format) CFRelease(c->format);
   free(c); /* Encoder may already have been destroyed: do not access it. */
}
static INLINE int record_vt_visible_submit(void *data,unsigned slot,void *buffer,uint64_t pts,uint64_t duration)
{
   record_vt_visible_t *c=(record_vt_visible_t*)data;
   return c->error?-1:c->base.submit(c->encoder,slot,buffer,pts,duration);
}
static INLINE int record_vt_visible_peek(void *data,unsigned *slot,void **sample)
{
   record_vt_visible_t *c=(record_vt_visible_t*)data;
   void *original=NULL;
   CMVideoFormatDescriptionRef format,replacement=NULL;
   CGRect aperture;
   int result;
   if (c->error) return -1;
   if (!c->sample)
   {
      result=c->base.peek(c->encoder,&c->slot,&original);
      if (result!=1) return result;
      if (!original) goto failed;
      format=CMSampleBufferGetFormatDescription((CMSampleBufferRef)original);
      if (!format) goto failed;
      aperture=CMVideoFormatDescriptionGetCleanAperture(format,true);
      if (aperture.origin.x!=0 || aperture.origin.y!=0 || aperture.size.width!=c->width
            || aperture.size.height!=c->height) goto failed;
      if (!c->original || !CMFormatDescriptionEqual(c->original,format))
      {
         if (!videotoolbox_encode_crop_format(format,c->encoder->config.width,c->encoder->config.height,
                  c->width,c->height,&replacement)) goto failed;
         if (c->original) CFRelease(c->original);
         if (c->format) CFRelease(c->format);
         c->original=(CMVideoFormatDescriptionRef)CFRetain(format); c->format=replacement;
      }
      if (!videotoolbox_encode_crop_sample((CMSampleBufferRef)original,c->format,&c->sample)) goto failed;
   }
   *slot=c->slot; *sample=(void*)c->sample; return 1;
failed:
   c->error=true; return -1;
}
static INLINE bool record_vt_visible_ack(void *data)
{
   record_vt_visible_t *c=(record_vt_visible_t*)data;
   if (!c->sample || !c->base.ack(c->encoder)) return false;
   CFRelease(c->sample); c->sample=NULL; return true;
}
static INLINE int record_vt_visible_finish(void *data)
{
   record_vt_visible_t *c=(record_vt_visible_t*)data;
   return c->error?-1:c->base.finish(c->encoder);
}
static INLINE VT_ENCODE_AVAILABLE record_gpu_worker_vt_t *record_gpu_worker_vt_new_native_visible_ops(
      record_gpu_queue_t *queue,record_gpu_capture_metal_t *capture,videotoolbox_encode_t *encoder,
      unsigned width,unsigned height,const record_gpu_worker_vt_ops_t *base)
{
   record_vt_visible_t *c;
   record_gpu_worker_vt_ops_t ops;
   record_gpu_worker_vt_t *worker;
   CFMutableDictionaryRef aperture;
   bool configured;
   if (!queue || !capture || !encoder || encoder->submitted || encoder->closed || encoder->error || !base
         || !base->ready || !base->buffer || !base->release || !base->submit || !base->peek || !base->ack || !base->finish
         || !width || !height || ((width|height)&1)
         || width>encoder->config.width || height>encoder->config.height) return NULL;
   if (width==encoder->config.width && height==encoder->config.height)
      return record_gpu_worker_vt_new(queue,capture,encoder,base);
   if (!encoder->config.hevc || (encoder->config.bit_depth==10
            && ((encoder->config.width|encoder->config.height)&31))) return NULL;
   c=(record_vt_visible_t*)calloc(1,sizeof(*c)); if (!c) return NULL;
   c->encoder=encoder; c->base=*base; c->width=width; c->height=height;
   aperture=CFDictionaryCreateMutable(NULL,0,&kCFTypeDictionaryKeyCallBacks,&kCFTypeDictionaryValueCallBacks);
   if (!aperture) { free(c); return NULL; }
   configured=videotoolbox_encode_number(aperture,kCVImageBufferCleanApertureWidthKey,(int32_t)width)
      && videotoolbox_encode_number(aperture,kCVImageBufferCleanApertureHeightKey,(int32_t)height)
      && videotoolbox_encode_number(aperture,kCVImageBufferCleanApertureHorizontalOffsetKey,
            ((int32_t)width-(int32_t)encoder->config.width)/2)
      && videotoolbox_encode_number(aperture,kCVImageBufferCleanApertureVerticalOffsetKey,
            ((int32_t)height-(int32_t)encoder->config.height)/2);
   if (configured) configured=VTSessionSetProperty(encoder->session,kVTCompressionPropertyKey_CleanAperture,aperture)==0;
   CFRelease(aperture);
   if (!configured) { free(c); return NULL; }
   ops=*base; ops.submit=record_vt_visible_submit; ops.peek=record_vt_visible_peek;
   ops.ack=record_vt_visible_ack; ops.finish=record_vt_visible_finish;
   worker=record_gpu_worker_vt_new_owned(queue,capture,c,&ops,c,record_vt_visible_destroy);
   if (!worker) record_vt_visible_destroy(c);
   return worker;
}
static INLINE VT_ENCODE_AVAILABLE record_gpu_worker_vt_t *record_gpu_worker_vt_new_native_visible(
      record_gpu_queue_t *queue,record_gpu_capture_metal_t *capture,videotoolbox_encode_t *encoder,
      unsigned width,unsigned height)
{
   record_gpu_worker_vt_ops_t ops;
   ops.ready=record_vt_native_ready; ops.buffer=record_vt_native_buffer; ops.release=record_vt_native_release;
   ops.submit=record_vt_native_submit; ops.peek=record_vt_native_peek; ops.ack=record_vt_native_ack; ops.finish=record_vt_native_finish;
   return record_gpu_worker_vt_new_native_visible_ops(queue,capture,encoder,width,height,&ops);
}
#endif
#endif
