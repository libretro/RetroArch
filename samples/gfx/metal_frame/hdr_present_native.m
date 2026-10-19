#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <dispatch/dispatch.h>
/* HDR_METAL_HEADER */

static double movie_pq(double scrgb)
{
   double value = pow(scrgb / 125.0, 2610.0 / 16384.0);
   return pow((3424.0 / 4096.0 + 2413.0 / 128.0 * value) /
         (1.0 + 2392.0 / 128.0 * value), 2523.0 / 32.0);
}
static double movie_half(uint16_t value)
{
   unsigned exponent = (value >> 10) & 31, fraction = value & 1023;
   assert(exponent != 31);
   double result = exponent ? ldexp(1.0 + fraction / 1024.0, (int)exponent - 15)
      : ldexp((double)fraction, -24);
   return value & 32768 ? -result : result;
}
int main(void)
{
   @autoreleasepool
   {
      id<MTLDevice> device = MTLCreateSystemDefaultDevice();
      if (!device) { puts("SKIP: Metal unavailable"); return 77; }
      NSError *error = nil;
      NSString *source = [NSString stringWithContentsOfFile:@/* HDR_SOURCE_PATH */
         encoding:NSUTF8StringEncoding error:&error];
      assert(source && !error);
      id<MTLLibrary> library = [device newLibraryWithSource:source options:nil error:&error];
      if (!library) fprintf(stderr, "Movie HDR stock library: %s\n", error.localizedDescription.UTF8String);
      assert(library);
      id<MTLFunction> vertex = [library newFunctionWithName:@"hdr_composite_vertex"];
      id<MTLFunction> fragment = [library newFunctionWithName:@"hdr_movie_fragment"];
      assert(vertex && fragment);
      const float values[4] = { 0.5f, 1.0f, 12.5f, 125.0f };
      float pixels[16];
      for (unsigned i = 0; i < 4; i++)
      { pixels[i * 4] = pixels[i * 4 + 1] = pixels[i * 4 + 2] = values[i]; pixels[i * 4 + 3] = 1; }
      MTLTextureDescriptor *input_desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
         MTLPixelFormatRGBA32Float width:2 height:2 mipmapped:NO];
      input_desc.storageMode = MTLStorageModeShared;
      id<MTLTexture> input = [device newTextureWithDescriptor:input_desc]; assert(input);
      [input replaceRegion:MTLRegionMake2D(0, 0, 2, 2) mipmapLevel:0 withBytes:pixels bytesPerRow:32];
      MTLSamplerDescriptor *sampler_desc = [MTLSamplerDescriptor new];
      sampler_desc.minFilter = sampler_desc.magFilter = MTLSamplerMinMagFilterNearest;
      id<MTLSamplerState> sampler = [device newSamplerStateWithDescriptor:sampler_desc];
      id<MTLCommandQueue> queue = [device newCommandQueue]; assert(sampler && queue);
      for (unsigned scrgb = 0; scrgb < 2; scrgb++) for (unsigned rotation = 0; rotation < 4; rotation++)
      {
         MTLPixelFormat format = scrgb ? MTLPixelFormatRGBA16Float : MTLPixelFormatRGB10A2Unorm;
         MTLRenderPipelineDescriptor *pipeline_desc = [MTLRenderPipelineDescriptor new];
         pipeline_desc.vertexFunction = vertex; pipeline_desc.fragmentFunction = fragment;
         pipeline_desc.colorAttachments[0].pixelFormat = format;
         id<MTLRenderPipelineState> pipeline = [device newRenderPipelineStateWithDescriptor:pipeline_desc error:&error];
         assert(pipeline);
         MTLTextureDescriptor *output_desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
            format width:4 height:4 mipmapped:NO];
         output_desc.storageMode = MTLStorageModeShared; output_desc.usage = MTLTextureUsageRenderTarget;
         id<MTLTexture> output = [device newTextureWithDescriptor:output_desc]; assert(output);
         MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
         pass.colorAttachments[0].texture = output; pass.colorAttachments[0].loadAction = MTLLoadActionClear;
         pass.colorAttachments[0].storeAction = MTLStoreActionStore;
         HDRUniforms uniforms; memset(&uniforms, 0, sizeof(uniforms));
         uniforms.CoreViewport = simd_make_float4(1, 1, 2, 2);
         uniforms.HDRMode = scrgb ? 2 : 1; uniforms.Rotation = rotation;
         /* Movie luminance must ignore the core's inverse-tonemap/UI controls. */
         uniforms.InverseTonemap = uniforms.HDR10 = 1; uniforms.BrightnessNits = 203;
         uniforms.PaperWhiteNits = 100; uniforms.ExpandGamut = 3;
         id<MTLCommandBuffer> buffer = [queue commandBuffer];
         id<MTLRenderCommandEncoder> encoder = [buffer renderCommandEncoderWithDescriptor:pass]; assert(encoder);
         [encoder setRenderPipelineState:pipeline]; [encoder setFragmentBytes:&uniforms length:sizeof(uniforms) atIndex:0];
         [encoder setFragmentTexture:input atIndex:0]; [encoder setFragmentSamplerState:sampler atIndex:0];
         [encoder drawPrimitives:MTLPrimitiveTypeTriangleStrip vertexStart:0 vertexCount:4]; [encoder endEncoding];
         dispatch_semaphore_t complete = dispatch_semaphore_create(0);
         [buffer addCompletedHandler:^(id<MTLCommandBuffer> completed) { (void)completed; dispatch_semaphore_signal(complete); }];
         [buffer commit];
         assert(!dispatch_semaphore_wait(complete, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC)));
         assert(buffer.status == MTLCommandBufferStatusCompleted);
         uint16_t halves[64]; uint32_t packed[16];
         [output getBytes:scrgb ? (void*)halves : (void*)packed bytesPerRow:scrgb ? 32 : 16
            fromRegion:MTLRegionMake2D(0, 0, 4, 4) mipmapLevel:0];
         for (unsigned y = 0; y < 4; y++) for (unsigned x = 0; x < 4; x++)
         {
            unsigned at = y * 4 + x;
            if (!x || x == 3 || !y || y == 3)
            { assert(scrgb ? halves[at * 4 + 3] == 0 : packed[at] == 0); continue; }
            unsigned sx = x - 1, sy = y - 1, old = sx;
            if (rotation == 1) { sx = 1 - sy; sy = old; }
            if (rotation == 2) { sx = 1 - sx; sy = 1 - sy; }
            if (rotation == 3) { sx = sy; sy = 1 - old; }
            double expected = values[sy * 2 + sx];
            for (unsigned channel = 0; channel < 3; channel++)
            {
               double actual = scrgb ? movie_half(halves[at * 4 + channel])
                  : ((packed[at] >> (channel * 10)) & 1023) / 1023.0;
               assert(fabs(actual - (scrgb ? expected : movie_pq(expected))) <= (scrgb ? 0.001 : 1.1 / 1023.0));
            }
            assert(scrgb ? halves[at * 4 + 3] == 0x3c00 : (packed[at] >> 30) == 3);
         }
      }
      puts("Metal HDR movie presentation: stock MSL, absolute luminance, two outputs, viewport and four rotations passed");
   }
   return 0;
}
