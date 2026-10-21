/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <CoreVideo/CoreVideo.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <defines/cocoa_defines.h>
#include "../../../record/record_gpu_capture_metal.h"
#define WIDTH 162
#define HEIGHT 130
#define SOURCE_W (WIDTH+4)
#define SOURCE_H (HEIGHT+4)
static double component(unsigned x,unsigned y,unsigned c,unsigned mode)
{
   static const double linear[]={0,.25,.5,1,2,4,8,-.25};
   return mode==2?linear[(x+3*y+c*2)%8]
      :(double)((x*17+y*13+c*(mode?127:67))%(mode?1024:256))/(mode?1023:255);
}
static double clamp01(double v) { return v<0?0:v>1?1:v; }
static double pq(double light)
{
   double power=pow(clamp01(light),2610.0/16384.0);
   return pow((3424.0/4096.0+(2413.0/128.0)*power)/(1+(2392.0/128.0)*power),2523.0/32.0);
}
static void reference(unsigned x,unsigned y,unsigned mode,double *out)
{
   double r=component(x+2,y+2,0,mode),g=component(x+2,y+2,1,mode),b=component(x+2,y+2,2,mode),a[3];
   if (mode==2)
   {
      a[0]=pq((.627404*r+.329283*g+.043313*b)*80/10000);
      a[1]=pq((.069097*r+.919540*g+.011362*b)*80/10000);
      a[2]=pq((.016391*r+.088013*g+.895595*b)*80/10000);
      r=a[0]; g=a[1]; b=a[2];
   }
   r=clamp01(r); g=clamp01(g); b=clamp01(b);
   out[0]=mode?.2627*r+.6780*g+.0593*b:.2126*r+.7152*g+.0722*b;
   out[1]=(b-out[0])/(mode?1.8814:1.8556);
   out[2]=(r-out[0])/(mode?1.4746:1.5748);
}
static id<MTLTexture> source_new(id<MTLDevice> device,unsigned mode,bool reverse)
{
   static const uint16_t halves[]={0,0x3400,0x3800,0x3c00,0x4000,0x4400,0x4800,0xb400};
   MTLPixelFormat format=mode==2?MTLPixelFormatRGBA16Float:mode==1
      ?(reverse?MTLPixelFormatBGR10A2Unorm:MTLPixelFormatRGB10A2Unorm)
      :(reverse?MTLPixelFormatBGRA8Unorm:MTLPixelFormatRGBA8Unorm);
   MTLTextureDescriptor *desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
         width:SOURCE_W height:SOURCE_H mipmapped:NO];
   size_t pixel=mode==2?8:4, stride=SOURCE_W*pixel;
   uint8_t *bytes=(uint8_t*)calloc(SOURCE_H,stride);
   unsigned x,y,c;
   id<MTLTexture> texture;
   assert(bytes);
   desc.storageMode=MTLStorageModeShared; desc.usage=MTLTextureUsageShaderRead;
   texture=[device newTextureWithDescriptor:desc]; assert(texture);
   for (y=0;y<SOURCE_H;y++)
      for (x=0;x<SOURCE_W;x++)
      {
         uint8_t *at=bytes+y*stride+x*pixel;
         if (mode==2)
         {
            for (c=0;c<3;c++) ((uint16_t*)at)[c]=halves[(x+3*y+c*2)%8];
            ((uint16_t*)at)[3]=0x3c00;
         }
         else if (mode==1)
         {
            uint32_t value=3u<<30;
            for (c=0;c<3;c++) value|=(uint32_t)floor(component(x,y,c,mode)*1023+.5)<<((reverse?2-c:c)*10);
            *(uint32_t*)at=value;
         }
         else
         {
            for (c=0;c<3;c++) at[reverse?2-c:c]=(uint8_t)floor(component(x,y,c,mode)*255+.5);
            at[3]=255;
         }
      }
   [texture replaceRegion:MTLRegionMake2D(0,0,SOURCE_W,SOURCE_H) mipmapLevel:0
         withBytes:bytes bytesPerRow:stride];
   free(bytes); return texture;
}
static unsigned read_code(CVPixelBufferRef image,unsigned plane,unsigned x,unsigned y,unsigned depth)
{
   const uint8_t *row=(const uint8_t*)CVPixelBufferGetBaseAddressOfPlane(image,plane);
   assert(row); row+=y*CVPixelBufferGetBytesPerRowOfPlane(image,plane);
   if (depth==10) { unsigned value=((const uint16_t*)row)[x]; assert(!(value&63)); return value>>6; }
   return row[x];
}
static unsigned check_pixels(CVPixelBufferRef image,unsigned depth,unsigned mode)
{
   unsigned x,y,peak=0,c;
   assert(!CVPixelBufferLockBaseAddress(image,kCVPixelBufferLock_ReadOnly));
   for (y=0;y<HEIGHT;y++)
      for (x=0;x<WIDTH;x++)
      {
         double color[3];
         unsigned expected,actual,error;
         reference(x,y,mode,color);
         expected=(unsigned)floor((depth==10?64:16)+(depth==10?876:219)*color[0]+.5);
         actual=read_code(image,0,x,y,depth); error=actual>expected?actual-expected:expected-actual;
         if (error>peak) peak=error;
      }
   for (y=0;y<HEIGHT;y+=2)
      for (x=0;x<WIDTH;x+=2)
      {
         double a[3],b[3],d[3],e[3],f[3],g[3];
         reference(x,y,mode,a); reference(x+1,y,mode,b);
         reference(x,y+1,mode,d); reference(x+1,y+1,mode,e);
         reference(x?x-1:0,y,mode,f); reference(x?x-1:0,y+1,mode,g);
         for (c=1;c<3;c++)
         {
            double uv=(f[c]+g[c]+2*(a[c]+d[c])+b[c]+e[c])/8;
            unsigned expected=(unsigned)floor((depth==10?512:128)+(depth==10?896:224)*uv+.5);
            unsigned actual=read_code(image,1,x+c-1,y/2,depth);
            unsigned error=actual>expected?actual-expected:expected-actual;
            if (error>peak) peak=error;
         }
      }
   CVPixelBufferUnlockBaseAddress(image,kCVPixelBufferLock_ReadOnly);
   assert(peak<=2); return peak;
}
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
   capture=record_gpu_capture_metal_new((__bridge void*)device,buffers,2,WIDTH,HEIGHT,depth,mode); assert(capture);
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
