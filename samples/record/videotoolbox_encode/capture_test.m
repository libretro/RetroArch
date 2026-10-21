/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "capture_reference.h"
static void test(id<MTLDevice> device,unsigned depth,unsigned mode,bool reverse)
{
   CVPixelBufferRef images[2]={NULL,NULL};
   void *buffers[2];
   record_gpu_capture_metal_t *capture;
   NSDictionary *attrs=@{(__bridge NSString*)kCVPixelBufferMetalCompatibilityKey:@YES,
         (__bridge NSString*)kCVPixelBufferIOSurfacePropertiesKey:@{}};
   id<MTLCommandQueue> queue=[device newCommandQueue];
   id<MTLTexture> source=source_new(device,mode,reverse);
   id<MTLCommandBuffer> commands=[queue commandBuffer];
   unsigned i,peak=0;
   assert(queue && commands);
   for (i=0;i<2;i++)
   {
      assert(!CVPixelBufferCreate(NULL,WIDTH,HEIGHT,depth==10
               ?kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
               (__bridge CFDictionaryRef)attrs,&images[i])); buffers[i]=images[i];
   }
   capture=record_gpu_capture_metal_new((__bridge void*)device,buffers,2,WIDTH,HEIGHT,depth,mode); if (!capture) diagnose_shader(device);
      assert(capture);
   assert(record_gpu_capture_metal_encode(capture,2,(__bridge void*)commands,(__bridge void*)source,2,2)==-1);
   assert(record_gpu_capture_metal_encode(capture,0,(__bridge void*)commands,(__bridge void*)source,SOURCE_W,2)==-1);
   for (i=0;i<2;i++)
   {
      assert(!record_gpu_capture_metal_buffer(capture,i));
      assert(record_gpu_capture_metal_encode(capture,i,(__bridge void*)commands,(__bridge void*)source,2,2)==1);
      assert(!record_gpu_capture_metal_ready(capture,i));
      assert(!record_gpu_capture_metal_release(capture,i));
      assert(record_gpu_capture_metal_encode(capture,i,(__bridge void*)commands,(__bridge void*)source,2,2)==0);
   }
   assert(!record_gpu_capture_metal_free(capture));
   [commands commit]; [commands waitUntilCompleted]; /* Oracle only. */
   assert(commands.status==MTLCommandBufferStatusCompleted);
   for (i=0;i<2;i++)
   {
      unsigned error;
      assert(record_gpu_capture_metal_ready(capture,i)==1);
      assert(record_gpu_capture_metal_buffer(capture,i)==images[i]);
      error=check_pixels(images[i],depth,mode); if (error>peak) peak=error;
      assert(!record_gpu_capture_metal_free(capture));
      assert(record_gpu_capture_metal_release(capture,i));
      assert(!record_gpu_capture_metal_release(capture,i));
   }
   assert(record_gpu_capture_metal_free(capture));
   for (i=0;i<2;i++) CVPixelBufferRelease(images[i]);
   RARCH_RELEASE(source); RARCH_RELEASE(queue);
   printf("PASS: Metal capture depth=%u mode=%u reverse=%u crop/two-slot lifetime, peak=%u\n",depth,mode,reverse,peak);
}
int main(void)
{
   @autoreleasepool
   {
      id<MTLDevice> device=MTLCreateSystemDefaultDevice();
      if (!device) { puts("SKIP: no native Metal device"); return 77; }
      test(device,8,0,false); test(device,8,0,true);
      test(device,10,0,false); test(device,10,1,false); test(device,10,1,true); test(device,10,2,false);
      RARCH_RELEASE(device);
   }
   return 0;
}
