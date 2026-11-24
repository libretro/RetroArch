#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <simd/simd.h>
#include <math.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "hdr_viewer_colour.h"

int main(int argc, const char **argv)
{
   @autoreleasepool
   {
      static const float colors[8][3] = {{0,0,0},{1,1,1},{1,0,0},{0,1,0},
         {0,0,1},{0,1,1},{1,0,1},{.23f,.58f,.91f}};
      static const float whites[4] = {20,80,203,1000};
      static const double inverse2020[9] = {1.660491,-.5876411,-.0728499,
         -.1245505,1.1328999,-.0083494,-.0181508,-.1005789,1.1187297};
      id<MTLDevice> device = MTLCreateSystemDefaultDevice();
      if (!device) return 77;
      if (argc != 2) return 1;
      NSError *error = nil;
      NSString *source = [NSString stringWithContentsOfFile:[NSString stringWithUTF8String:argv[1]]
            encoding:NSUTF8StringEncoding error:&error];
      id<MTLLibrary> library = source ? [device newLibraryWithSource:source options:nil error:&error] : nil;
      if (!library)
      {
         fprintf(stderr, "Viewer MSL: %s\n", error.localizedDescription.UTF8String);
         return 1;
      }
      MTLRenderPipelineDescriptor *pd = [MTLRenderPipelineDescriptor new];
      pd.vertexFunction = [library newFunctionWithName:@"views_vertex"];
      pd.fragmentFunction = [library newFunctionWithName:@"views_media"];
      pd.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA32Float;
      id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:pd error:&error];
      if (!pipeline)
      {
         fprintf(stderr, "Viewer pipeline: %s\n", error.localizedDescription.UTF8String);
         return 1;
      }
      MTLTextureDescriptor *td = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
            width:8 height:1 mipmapped:NO];
      td.storageMode = MTLStorageModeShared;
      td.usage = MTLTextureUsageShaderRead;
      id<MTLTexture> input = [device newTextureWithDescriptor:td];
      td.usage = MTLTextureUsageRenderTarget;
      id<MTLTexture> output = [device newTextureWithDescriptor:td];
      MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new];
      sd.minFilter = sd.magFilter = MTLSamplerMinMagFilterNearest;
      sd.sAddressMode = sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
      id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:sd];
      id<MTLCommandQueue> queue = [device newCommandQueue];
      if (!input || !output || !sampler || !queue) return 1;
      struct { matrix_float4x4 mvp; vector_float4 rect, clamp; } u;
      memset(&u, 0, sizeof(u));
      u.mvp.columns[2][2] = u.mvp.columns[3][3] = 1;
      u.mvp.columns[0][0] = u.mvp.columns[1][1] = 2;
      u.mvp.columns[3][0] = u.mvp.columns[3][1] = -1;
      u.rect = (vector_float4){0,0,1,1};
      u.clamp = (vector_float4){.5f/8,.5f,1-.5f/8,.5f};
      unsigned failures = 0;
      for (unsigned mode = 1; mode <= 2; mode++)
      for (unsigned gamut = 0; gamut < 4; gamut++)
      for (unsigned white = 0; white < 4; white++)
      {
         float pixels[8][4], expected[8][4], actual[8][4], c[16] = {0};
         hdr_viewer_colour_rows(c, true, whites[white], gamut);
         c[12] = mode;
         for (unsigned i = 0; i < 8; i++)
         {
            double linear[3] = {0,0,0};
            for (unsigned ch = 0; ch < 3; ch++)
            {
               for (unsigned j = 0; j < 3; j++) linear[ch] += inverse2020[3*ch+j]*colors[i][j]*12.5;
               double p = pow(colors[i][ch]*.1, 2610.0/16384.0);
               pixels[i][ch] = mode == 1 ? pow((3424.0/4096.0+2413.0/128.0*p)
                     /(1+2392.0/128.0*p),2523.0/32.0) : linear[ch];
            }
            for (unsigned ch = 0; ch < 3; ch++)
            {
               double x = 0;
               for (unsigned j = 0; j < 3; j++) x += c[4*ch+j]*linear[j];
               expected[i][ch] = copysign(pow(fabs(x), 1.0/2.4), x);
            }
            pixels[i][3] = (float)(i%4+1)/4;
            expected[i][3] = 1;
         }
         [input replaceRegion:MTLRegionMake2D(0,0,8,1) mipmapLevel:0
               withBytes:pixels bytesPerRow:sizeof(pixels)];
         MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
         pass.colorAttachments[0].texture = output;
         pass.colorAttachments[0].loadAction = MTLLoadActionDontCare;
         pass.colorAttachments[0].storeAction = MTLStoreActionStore;
         id<MTLCommandBuffer> cb = [queue commandBuffer];
         id<MTLRenderCommandEncoder> encoder = [cb renderCommandEncoderWithDescriptor:pass];
         [encoder setRenderPipelineState:pipeline];
         [encoder setVertexBytes:&u length:sizeof(u) atIndex:0];
         [encoder setFragmentBytes:&u length:sizeof(u) atIndex:0];
         [encoder setFragmentBytes:c length:sizeof(c) atIndex:1];
         [encoder setFragmentTexture:input atIndex:0];
         [encoder setFragmentSamplerState:sampler atIndex:0];
         [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4];
         [encoder endEncoding];
         [cb commit];
         [cb waitUntilCompleted];
         if (cb.status != MTLCommandBufferStatusCompleted) return 1;
         [output getBytes:actual bytesPerRow:sizeof(actual) fromRegion:MTLRegionMake2D(0,0,8,1) mipmapLevel:0];
         for (unsigned i = 0; i < 8; i++)
         for (unsigned ch = 0; ch < 4; ch++)
            if (!isfinite(actual[i][ch]) || fabs(actual[i][ch]-expected[i][ch])
                  > .003+.003*fabs(expected[i][ch])) failures++;
      }
      printf("Native Metal viewer: 256 PQ/scRGB GPU vectors, %u failures\n", failures);
      return failures ? 1 : 0;
   }
}
