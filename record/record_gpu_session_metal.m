/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#if defined(__APPLE__) && !defined(__cplusplus) \
      && (!defined(__STDC_VERSION__) || __STDC_VERSION__<199901L) && !defined(inline)
#define inline __inline__
#define RECORD_METAL_UNDEF_INLINE
#endif
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <CoreVideo/CoreVideo.h>
#include <TargetConditionals.h>
#ifdef RECORD_METAL_UNDEF_INLINE
#undef inline
#undef RECORD_METAL_UNDEF_INLINE
#endif
#include <string.h>
#include "record_gpu_session_metal.h"
#include "record_avfoundation_session_native.h"
#if defined(VIDEOTOOLBOX_ENCODE_SLOTS) && TARGET_OS_OSX \
      && defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED>=101400
/* Enumerated IDs prevent silent default/software/other-GPU selection. A
 * hardware encoder with no advertised GPU is a separate explicit category,
 * as used by the hosted paravirtualized media engines. It is not evidence of
 * physical same-GPU encoding; CVPixelBuffer/Metal interop remains gated. */
static __attribute__((availability(macos,introduced=10.14))) videotoolbox_encode_t *record_metal_select_encoder(
      const videotoolbox_encode_config_t *config,uint64_t renderer_gpu,OSStatus *error)
{
   CFArrayRef list=NULL;
   CFIndex i,count;
   unsigned priority;
   videotoolbox_encode_t *encoder=NULL;
   OSStatus status=VTCopyVideoEncoderList(NULL,&list);
   if (status || !list)
   { if (list) CFRelease(list); *error=status?status:kVTCouldNotFindVideoEncoderErr; return NULL; }
   count=CFArrayGetCount(list); status=kVTCouldNotFindVideoEncoderErr;
   if (count<0 || count>4096) goto done;
   for (priority=2;priority && !encoder;priority--) for (i=0;i<count;i++)
   {
      CFDictionaryRef entry=(CFDictionaryRef)CFArrayGetValueAtIndex(list,i);
      CFTypeRef hardware,codec,gpu,identifier;
      int64_t codec_value=0,gpu_value=0;
      unsigned rank;
      if (!entry || CFGetTypeID(entry)!=CFDictionaryGetTypeID()) continue;
      hardware=CFDictionaryGetValue(entry,kVTVideoEncoderList_IsHardwareAccelerated);
      codec=CFDictionaryGetValue(entry,kVTVideoEncoderList_CodecType);
      gpu=CFDictionaryGetValue(entry,kVTVideoEncoderList_GPURegistryID);
      identifier=CFDictionaryGetValue(entry,kVTVideoEncoderList_EncoderID);
      if (!hardware || CFGetTypeID(hardware)!=CFBooleanGetTypeID() || !CFBooleanGetValue((CFBooleanRef)hardware)
            || !codec || CFGetTypeID(codec)!=CFNumberGetTypeID()
            || !CFNumberGetValue((CFNumberRef)codec,kCFNumberSInt64Type,&codec_value)
            || codec_value!=(int64_t)(config->hevc?kCMVideoCodecType_HEVC:kCMVideoCodecType_H264)
            || !identifier || CFGetTypeID(identifier)!=CFStringGetTypeID()
            || !CFStringGetLength((CFStringRef)identifier)) continue;
      if (gpu && (CFGetTypeID(gpu)!=CFNumberGetTypeID()
               || !CFNumberGetValue((CFNumberRef)gpu,kCFNumberSInt64Type,&gpu_value))) continue;
      rank=!gpu_value?1:(uint64_t)gpu_value==renderer_gpu?2:0;
      if (rank!=priority) continue;
      encoder=videotoolbox_encode_new_selected(config,(CFStringRef)identifier,rank==2?renderer_gpu:0,&status);
      if (encoder) break;
   }
done:
   CFRelease(list); *error=status; return encoder;
}
#endif
record_avfoundation_session_t *record_gpu_session_metal_new(void *device,
      const record_gpu_session_metal_config_t *c,const record_mux_avfoundation_config_t *output,
      size_t audio_capacity_bytes,const char *path,record_gpu_capture_metal_t **capture_out,
      record_gpu_queue_t **queue_out,enum record_gpu_session_metal_startup *stage,int32_t *native_status)
{
   if (capture_out) *capture_out=NULL;
   if (queue_out) *queue_out=NULL;
   if (stage) *stage=RECORD_METAL_STARTUP_UNSUPPORTED;
   if (native_status) *native_status=0;
#if defined(VIDEOTOOLBOX_ENCODE_SLOTS) && TARGET_OS_OSX \
      && defined(__MAC_OS_X_VERSION_MAX_ALLOWED) && __MAC_OS_X_VERSION_MAX_ALLOWED>=101400
   @autoreleasepool
   {
      videotoolbox_encode_config_t config;
      videotoolbox_encode_t *encoder=NULL;
      record_gpu_capture_metal_t *capture=NULL;
      record_gpu_queue_t *queue=NULL;
      record_avfoundation_session_t *session=NULL;
      CVPixelBufferRef images[VIDEOTOOLBOX_ENCODE_SLOTS];
      void *buffers[VIDEOTOOLBOX_ENCODE_SLOTS];
      CVPixelBufferPoolRef pool;
      OSStatus status=0;
      uint64_t gpu;
      unsigned i;
      enum record_gpu_session_metal_startup failure=RECORD_METAL_STARTUP_CONFIG;
      memset(images,0,sizeof(images)); memset(&config,0,sizeof(config));
      if (!device || !c || !output || !capture_out || !queue_out || !path || !*path
            || output->video_format || !c->width || !c->height || c->width>16384 || c->height>16384
            || ((c->width|c->height)&1) || c->slots<2 || c->slots>VIDEOTOOLBOX_ENCODE_SLOTS
            || c->mode>2 || (c->mode && c->bit_depth!=10)
            || c->timescale!=output->timescale) goto done;
      gpu=[(__bridge id<MTLDevice>)device registryID];
      if (!gpu) goto done;
      config.width=c->width; config.height=c->height; config.slots=c->slots;
      config.bitrate=c->bitrate; config.key_interval=c->key_interval; config.timescale=c->timescale;
      config.bit_depth=c->bit_depth; config.hevc=c->hevc; config.hdr=c->mode!=0;
      if (config.hevc && config.bit_depth==10)
      { config.width=(config.width+31)&~31u; config.height=(config.height+31)&~31u; }
      failure=RECORD_METAL_STARTUP_ENCODER;
      encoder=record_metal_select_encoder(&config,gpu,&status);
      if (!encoder) goto done;
      failure=RECORD_METAL_STARTUP_POOL;
      pool=VTCompressionSessionGetPixelBufferPool(encoder->session); if (!pool) goto done;
      for (i=0;i<c->slots;i++)
      {
         status=CVPixelBufferPoolCreatePixelBuffer(NULL,pool,&images[i]);
         if (status || !images[i]) goto done;
         buffers[i]=(void*)images[i];
      }
      failure=RECORD_METAL_STARTUP_CAPTURE;
      capture=record_gpu_capture_metal_new(device,buffers,c->slots,c->width,c->height,c->bit_depth,c->mode);
      if (!capture) goto done;
      failure=RECORD_METAL_STARTUP_SESSION;
      queue=record_gpu_queue_new(c->slots); if (!queue) goto done;
      session=record_avfoundation_session_new_native(queue,capture,encoder,c->width,c->height,
            output,audio_capacity_bytes,path);
      if (!session) goto done;
      *capture_out=capture; *queue_out=queue;
      capture=NULL; queue=NULL; encoder=NULL; failure=RECORD_METAL_STARTUP_NONE;
done:
      for (i=0;i<VIDEOTOOLBOX_ENCODE_SLOTS;i++) if (images[i]) CVPixelBufferRelease(images[i]);
      record_gpu_capture_metal_free(capture); record_gpu_queue_free(queue); videotoolbox_encode_cancel(encoder);
      if (stage) *stage=failure;
      if (native_status) *native_status=(int32_t)status;
      return session;
   }
#else
   (void)device; (void)c; (void)output; (void)audio_capacity_bytes; (void)path;
   return NULL;
#endif
}
