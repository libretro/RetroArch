#ifndef RECORD_METAL_CAPTURE_TEST_REFERENCE_H
#define RECORD_METAL_CAPTURE_TEST_REFERENCE_H
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
#ifndef WIDTH
#define WIDTH 162
#endif
#ifndef HEIGHT
#define HEIGHT 130
#endif
#include "../../../record/shaders/capture_metal.h"
/* Fixture-only cold diagnostics; production capture keeps startup reporting
 * with its controller and performs no frame-path shader logging. */
static void diagnose_shader(id<MTLDevice> device)
{
   NSError *error=nil;
   NSString *source=[[NSString alloc] initWithBytes:record_capture_metal_shader
         length:sizeof(record_capture_metal_shader)-1 encoding:NSUTF8StringEncoding];
   id<MTLLibrary> library=[device newLibraryWithSource:source options:nil error:&error];
   fprintf(stderr,"Capture startup: %s\n",library?"shader compiled; inspect buffer/view setup"
         :error?[[error localizedDescription] UTF8String]:"shader compilation failed without error text");
   RARCH_RELEASE(library); RARCH_RELEASE(source);
}
#define SOURCE_W (WIDTH+4)
#define SOURCE_H (HEIGHT+4)
static unsigned reference_frame;
static double component(unsigned x,unsigned y,unsigned c,unsigned mode)
{
   static const double linear[]={0,.25,.5,1,2,4,8,-.25};
   return mode==2?linear[(x+3*y+c*2+reference_frame)%8]
      :(double)((x*17+y*13+c*(mode?127:67)+reference_frame*11)%(mode?1024:256))/(mode?1023:255);
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
static id<MTLTexture> source_new_extent(id<MTLDevice> device,unsigned mode,bool reverse,unsigned width,unsigned height)
{
   unsigned target_w=width+4,target_h=height+4;
   static const uint16_t halves[]={0,0x3400,0x3800,0x3c00,0x4000,0x4400,0x4800,0xb400};
   MTLPixelFormat format=mode==2?MTLPixelFormatRGBA16Float:mode==1
      ?(reverse?MTLPixelFormatBGR10A2Unorm:MTLPixelFormatRGB10A2Unorm)
      :(reverse?MTLPixelFormatBGRA8Unorm:MTLPixelFormatRGBA8Unorm);
   MTLTextureDescriptor *desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
         width:target_w height:target_h mipmapped:NO];
   size_t pixel=mode==2?8:4, stride=target_w*pixel;
   uint8_t *bytes=(uint8_t*)calloc(target_h,stride);
   unsigned x,y,c;
   id<MTLTexture> texture;
   assert(bytes);
   desc.storageMode=MTLStorageModeShared; desc.usage=MTLTextureUsageShaderRead;
   texture=[device newTextureWithDescriptor:desc]; assert(texture);
   for (y=0;y<target_h;y++)
      for (x=0;x<target_w;x++)
      {
         uint8_t *at=bytes+y*stride+x*pixel;
         unsigned sx=x>WIDTH+1?WIDTH+1:x,sy=y>HEIGHT+1?HEIGHT+1:y;
         if (mode==2)
         {
            for (c=0;c<3;c++) ((uint16_t*)at)[c]=halves[(sx+3*sy+c*2+reference_frame)%8];
            ((uint16_t*)at)[3]=0x3c00;
         }
         else if (mode==1)
         {
            uint32_t value=3u<<30;
            for (c=0;c<3;c++) value|=(uint32_t)floor(component(sx,sy,c,mode)*1023+.5)<<((reverse?2-c:c)*10);
            *(uint32_t*)at=value;
         }
         else
         {
            for (c=0;c<3;c++) at[reverse?2-c:c]=(uint8_t)floor(component(sx,sy,c,mode)*255+.5);
            at[3]=255;
         }
      }
   [texture replaceRegion:MTLRegionMake2D(0,0,target_w,target_h) mipmapLevel:0
         withBytes:bytes bytesPerRow:stride];
   free(bytes); return texture;
}
static id<MTLTexture> source_new(id<MTLDevice> device,unsigned mode,bool reverse)
{ return source_new_extent(device,mode,reverse,WIDTH,HEIGHT); }
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

#endif
