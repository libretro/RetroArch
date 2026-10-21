/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <CoreVideo/CoreVideo.h>
#include <stdlib.h>
#include <retro_atomic.h>
#include <defines/cocoa_defines.h>
#include "record_gpu_capture_metal.h"
#include "shaders/capture_metal.h"
#if defined(HAVE_THREADS) && defined(RETRO_ATOMIC_LOCK_FREE)
struct record_gpu_capture_metal
{
   CFTypeRef device, pipeline;
   CVMetalTextureCacheRef cache;
   CVPixelBufferRef buffers[16];
   CVMetalTextureRef views[16][2];
   retro_atomic_int_t state[16]; /* 0 reusable, 1 GPU, 2 encoder, 3 failed GPU */
   unsigned count, width, height, write_width, write_height, buffer_width, buffer_height, depth, mode;
};
bool record_gpu_capture_metal_free(record_gpu_capture_metal_t *capture)
{
   unsigned i, plane;
   if (!capture) return true;
   for (i=0;i<capture->count;i++)
      if (retro_atomic_load_acquire_int(&capture->state[i])) return false;
   for (i=0;i<capture->count;i++)
   {
      for (plane=0;plane<2;plane++)
         if (capture->views[i][plane]) CFRelease(capture->views[i][plane]);
      if (capture->buffers[i]) CVPixelBufferRelease(capture->buffers[i]);
   }
   if (capture->cache) CFRelease(capture->cache);
   if (capture->pipeline) CFRelease(capture->pipeline);
   if (capture->device) CFRelease(capture->device);
   free(capture); return true;
}
record_gpu_capture_metal_t *record_gpu_capture_metal_new(void *device_ptr,
      void *const *buffers, unsigned count, unsigned width, unsigned height,
      unsigned depth, unsigned mode)
{
   record_gpu_capture_metal_t *capture;
   unsigned i, j, plane;
   id<MTLDevice> device=(__bridge id<MTLDevice>)device_ptr;
   if (!device || !buffers || !count || count>16 || !width || !height
         || width>16384 || height>16384 || (width&1) || (height&1)
         || (depth!=8 && depth!=10) || mode>2 || (mode && depth!=10)) return NULL;
   capture=(record_gpu_capture_metal_t*)calloc(1,sizeof(*capture));
   if (!capture) return NULL;
   capture->count=count; capture->width=width; capture->height=height;
   capture->depth=depth; capture->mode=mode;
   for (i=0;i<count;i++) retro_atomic_int_init(&capture->state[i],0);
   capture->device=(CFTypeRef)RARCH_BRIDGE_RETAINED(device);
   @autoreleasepool
   {
      NSError *error=nil;
      NSString *source=[[NSString alloc] initWithBytes:record_capture_metal_shader
            length:sizeof(record_capture_metal_shader)-1 encoding:NSUTF8StringEncoding];
      id<MTLLibrary> library=source?[device newLibraryWithSource:source options:nil error:&error]:nil;
      id<MTLFunction> function=library?[library newFunctionWithName:@"capture_metal"]:nil;
      id<MTLComputePipelineState> pipeline=function?[device newComputePipelineStateWithFunction:function error:&error]:nil;
      NSDictionary *usage=@{(__bridge NSString*)kCVMetalTextureUsage:@(MTLTextureUsageShaderWrite)};
      if (pipeline) capture->pipeline=(CFTypeRef)RARCH_BRIDGE_RETAINED(pipeline);
      RARCH_RELEASE(pipeline); RARCH_RELEASE(function); RARCH_RELEASE(library); RARCH_RELEASE(source);
      if (!capture->pipeline || CVMetalTextureCacheCreate(NULL,NULL,device,
               (__bridge CFDictionaryRef)usage,&capture->cache)!=kCVReturnSuccess) goto fail;
      for (i=0;i<count;i++)
      {
         CVPixelBufferRef image=(CVPixelBufferRef)buffers[i];
         size_t left,right,top,bottom,buffer_width,buffer_height;
         if (!image) goto fail;
         buffer_width=CVPixelBufferGetWidth(image); buffer_height=CVPixelBufferGetHeight(image);
         if (!image || !CVPixelBufferIsPlanar(image) || CVPixelBufferGetPlaneCount(image)!=2
               || buffer_width<width || buffer_height<height || buffer_width>16384 || buffer_height>16384
               || ((buffer_width|buffer_height)&1)
               || CVPixelBufferGetPixelFormatType(image)!=(depth==10
                  ? kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange
                  : kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange)) goto fail;
         CVPixelBufferGetExtendedPixels(image,&left,&right,&top,&bottom);
         if (left || top || right>16384-buffer_width || bottom>16384-buffer_height
               || (right&1) || (bottom&1)) goto fail;
         if (!i)
         {
            capture->buffer_width=(unsigned)buffer_width; capture->buffer_height=(unsigned)buffer_height;
            capture->write_width=(unsigned)(buffer_width+right); capture->write_height=(unsigned)(buffer_height+bottom);
         }
         else if (capture->buffer_width!=buffer_width || capture->buffer_height!=buffer_height
               || capture->write_width!=buffer_width+right || capture->write_height!=buffer_height+bottom) goto fail;
         if (buffer_width!=width || buffer_height!=height)
         {
            NSDictionary *aperture=@{(__bridge NSString*)kCVImageBufferCleanApertureWidthKey:@(width),
               (__bridge NSString*)kCVImageBufferCleanApertureHeightKey:@(height),
               (__bridge NSString*)kCVImageBufferCleanApertureHorizontalOffsetKey:@(((int)width-(int)buffer_width)/2),
               (__bridge NSString*)kCVImageBufferCleanApertureVerticalOffsetKey:@(((int)height-(int)buffer_height)/2)};
            CVBufferSetAttachment(image,kCVImageBufferCleanApertureKey,(__bridge CFDictionaryRef)aperture,kCVAttachmentMode_ShouldPropagate);
         }
         for (j=0;j<i;j++) if (image==capture->buffers[j]) goto fail;
         capture->buffers[i]=CVPixelBufferRetain(image);
         for (plane=0;plane<2;plane++)
         {
            MTLPixelFormat format=depth==10
               ? (plane?MTLPixelFormatRG16Unorm:MTLPixelFormatR16Unorm)
               : (plane?MTLPixelFormatRG8Unorm:MTLPixelFormatR8Unorm);
            id<MTLTexture> texture;
            if (CVMetalTextureCacheCreateTextureFromImage(NULL,capture->cache,image,NULL,
                     format,capture->write_width>>plane,capture->write_height>>plane,plane,&capture->views[i][plane])!=kCVReturnSuccess) goto fail;
            texture=CVMetalTextureGetTexture(capture->views[i][plane]);
            if (!texture || texture.device!=device || !(texture.usage&MTLTextureUsageShaderWrite)) goto fail;
         }
      }
   }
   return capture;
fail:
   record_gpu_capture_metal_free(capture); return NULL;
}
int record_gpu_capture_metal_encode(record_gpu_capture_metal_t *capture,
      unsigned slot, void *command_ptr, void *source_ptr, unsigned x, unsigned y)
{
   id<MTLCommandBuffer> commands=(__bridge id<MTLCommandBuffer>)command_ptr;
   id<MTLTexture> source=(__bridge id<MTLTexture>)source_ptr;
   id<MTLDevice> device;
   uint32_t params[8];
   MTLPixelFormat format;
   if (!capture || slot>=capture->count || !commands || !source) return -1;
   if (retro_atomic_load_acquire_int(&capture->state[slot])) return 0;
   device=(__bridge id<MTLDevice>)capture->device;
   format=source.pixelFormat;
   if (commands.device!=device || source.device!=device || !commands.retainedReferences
         || commands.status>MTLCommandBufferStatusEnqueued || source.framebufferOnly
         || source.textureType!=MTLTextureType2D || source.sampleCount!=1
         || (source.usage && !(source.usage&MTLTextureUsageShaderRead))
         || x>source.width || y>source.height || capture->width>source.width-x
         || capture->height>source.height-y
         || (capture->mode==0 && format!=MTLPixelFormatRGBA8Unorm && format!=MTLPixelFormatBGRA8Unorm)
         || (capture->mode==1 && format!=MTLPixelFormatRGB10A2Unorm && format!=MTLPixelFormatBGR10A2Unorm)
         || (capture->mode==2 && format!=MTLPixelFormatRGBA16Float)) return -1;
   @autoreleasepool
   {
      id<MTLComputeCommandEncoder> encoder=[commands computeCommandEncoder];
      if (!encoder) return -1;
      params[0]=x; params[1]=y; params[2]=capture->width; params[3]=capture->height;
      params[4]=capture->mode; params[5]=capture->depth; params[6]=params[7]=0;
      [encoder setComputePipelineState:(__bridge id<MTLComputePipelineState>)capture->pipeline];
      [encoder setTexture:source atIndex:0];
      [encoder setTexture:CVMetalTextureGetTexture(capture->views[slot][0]) atIndex:1];
      [encoder setTexture:CVMetalTextureGetTexture(capture->views[slot][1]) atIndex:2];
      [encoder setBytes:params length:sizeof(params) atIndex:0];
      [encoder dispatchThreadgroups:MTLSizeMake((capture->write_width/2+7)/8,(capture->write_height/2+7)/8,1)
            threadsPerThreadgroup:MTLSizeMake(8,8,1)];
      [encoder endEncoding];
      retro_atomic_store_release_int(&capture->state[slot],1);
      [commands addCompletedHandler:^(id<MTLCommandBuffer> done) {
         retro_atomic_store_release_int(&capture->state[slot],
               done.status==MTLCommandBufferStatusCompleted?2:3);
      }];
   }
   return 1;
}
int record_gpu_capture_metal_ready(record_gpu_capture_metal_t *capture,unsigned slot)
{
   int state;
   if (!capture || slot>=capture->count) return -1;
   state=retro_atomic_load_acquire_int(&capture->state[slot]);
   return state==3?-1:state==2?1:0;
}
void *record_gpu_capture_metal_buffer(record_gpu_capture_metal_t *capture,unsigned slot)
{
   return record_gpu_capture_metal_ready(capture,slot)==1?(void*)capture->buffers[slot]:NULL;
}
bool record_gpu_capture_metal_release(record_gpu_capture_metal_t *capture,unsigned slot)
{
   int state;
   if (!capture || slot>=capture->count) return false;
   state=retro_atomic_load_acquire_int(&capture->state[slot]);
   if (state!=2 && state!=3) return false;
   retro_atomic_store_release_int(&capture->state[slot],0); return true;
}
#endif
