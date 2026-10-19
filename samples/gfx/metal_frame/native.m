#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <CoreFoundation/CoreFoundation.h>
#include <stdio.h>
#include <dispatch/dispatch.h>

/* SHIPPING_LEASE */
/* COREVIDEO */
/* SDR_CONVERT */

typedef struct native_copy
{
   void *textures[2];
   unsigned count;
} native_copy_t;

static bool encode_copy(void *user, const metal_frame_view_t *view, void *commands)
{
   native_copy_t *copy = user;
   id<MTLCommandBuffer> buffer = (__bridge id<MTLCommandBuffer>)commands;
   id<MTLBlitCommandEncoder> encoder = [buffer blitCommandEncoder];
   unsigned i;
   assert(view->width == 7 && view->height == 5);
   if (!encoder) return false;
   for (i = 0; i < copy->count; i++)
   {
      id<MTLTexture> input = (__bridge id<MTLTexture>)view->planes[i];
      id<MTLTexture> output = (__bridge id<MTLTexture>)copy->textures[i];
      [encoder copyFromTexture:input sourceSlice:0 sourceLevel:0
         sourceOrigin:MTLOriginMake(0, 0, 0)
         sourceSize:MTLSizeMake(input.width, input.height, 1)
         toTexture:output destinationSlice:0 destinationLevel:0
         destinationOrigin:MTLOriginMake(0, 0, 0)];
   }
   [encoder endEncoding];
   return true;
}

static void test(id<MTLDevice> device, unsigned format, bool discard)
{
   @autoreleasepool
   {
      id<MTLCommandQueue> producer = [device newCommandQueue];
      id<MTLCommandQueue> consumer = [device newCommandQueue];
      id<MTLSharedEvent> ready = [device newSharedEvent];
      id<MTLSharedEvent> done = [device newSharedEvent];
      MTLSharedEventListener *listener = [[MTLSharedEventListener alloc]
         initWithDispatchQueue:dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0)];
      metal_frame_source_t source;
      video_hw_frame_t frame;
      video_frame_import_t imported;
      native_copy_t copy;
      unsigned char bytes[512], actual[512];
      unsigned i, count = format == IMAGE_PIXFMT_NV12 || format == IMAGE_PIXFMT_P010 ? 2 : 1;
      id<MTLTexture> inputs[2], outputs[2];
      unsigned bpp[2];
      memset(&source, 0, sizeof(source)); memset(&copy, 0, sizeof(copy));
      for (i = 0; i < sizeof(bytes); i++) bytes[i] = (unsigned char)(i * 17);
      if (format == IMAGE_PIXFMT_FP16)
         for (i = 0; i < sizeof(bytes); i += 2)
         { bytes[i] = 0; bytes[i + 1] = (unsigned char)(0x3c + ((i / 8) % 8) * 2); }
      assert(producer && consumer && ready && done && listener);
      for (i = 0; i < count; i++)
      {
         MTLPixelFormat pixel;
         if (format == IMAGE_PIXFMT_NV12)
         { pixel = i ? MTLPixelFormatRG8Unorm : MTLPixelFormatR8Unorm; bpp[i] = i ? 2 : 1; }
         else if (format == IMAGE_PIXFMT_P010)
         { pixel = i ? MTLPixelFormatRG16Unorm : MTLPixelFormatR16Unorm; bpp[i] = i ? 4 : 2; }
         else
         { pixel = format == IMAGE_PIXFMT_FP16 ? MTLPixelFormatRGBA16Float : MTLPixelFormatBGRA8Unorm; bpp[i] = format == IMAGE_PIXFMT_FP16 ? 8 : 4; }
         MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:pixel
            width:i ? 4 : 9 height:i ? 3 : 7 mipmapped:NO];
         if (count == 2) { desc.width = i ? 4 : 7; desc.height = i ? 3 : 5; }
         desc.storageMode = MTLStorageModeShared;
         inputs[i] = [device newTextureWithDescriptor:desc];
         if (count == 1) { desc.width = 7; desc.height = 5; }
         outputs[i] = [device newTextureWithDescriptor:desc];
         assert(inputs[i] && outputs[i]);
         [inputs[i] replaceRegion:MTLRegionMake2D(0, 0, inputs[i].width, inputs[i].height)
            mipmapLevel:0 withBytes:bytes bytesPerRow:inputs[i].width * bpp[i]];
         source.planes[i] = (__bridge void *)inputs[i];
         copy.textures[i] = (__bridge void *)outputs[i];
      }
      source.device = (__bridge void *)device; source.ready = (__bridge void *)ready;
      source.done = (__bridge void *)done; source.listener = (__bridge void *)listener;
      if (count == 1) { source.crop_x = 2; source.crop_y = 2; }
      source.ready_value = 3; source.done_value = 7; source.width = 7; source.height = 5; source.pixfmt = format;
      CFMutableDataRef backing = CFDataCreateMutable(NULL, 0);
      source.backing[0] = backing;
      assert(metal_frame_create(&source, &frame));
      CFRelease(backing);
      copy.count = count;
      memset(&imported, 0, sizeof(imported));
      imported.image = frame.image; imported.wait = frame.wait; imported.done = frame.done;
      imported.wait_value = frame.wait_value; imported.done_value = frame.done_value;
      imported.owner = frame.owner; imported.abandon = frame.abandon;
      imported.crop_x = frame.crop_x; imported.crop_y = frame.crop_y;
      imported.output_pixfmt = format;
      imported.pixfmt = frame.pixfmt; imported.dims = VIDEO_SCALE_PACK(7, 5);
      id<MTLCommandBuffer> buffer = [consumer commandBuffer];
      dispatch_semaphore_t finished = dispatch_semaphore_create(0);
      [done notifyListener:listener atValue:7 block:^(id<MTLSharedEvent> event, uint64_t value) {
         (void)event; (void)value; dispatch_semaphore_signal(finished);
      }];
      if (discard)
         video_hw_frame_abandon(&frame);
      else
      {
         if (count == 1)
            assert(metal_frame_copy(&imported, (__bridge void *)device,
               (__bridge void *)buffer, copy.textures[0]));
         else
            assert(metal_frame_encode(&imported, (__bridge void *)device,
               (__bridge void *)buffer, encode_copy, &copy));
         [buffer commit];
      }
      /* Test-only waits and readback: production code has neither. */
      assert(done.signaledValue == 0);
      id<MTLCommandBuffer> write = [producer commandBuffer];
      [write encodeSignalEvent:ready value:3]; [write commit];
      assert(!dispatch_semaphore_wait(finished, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC)));
      if (!discard)
      {
         [buffer waitUntilCompleted];
         assert(buffer.status == MTLCommandBufferStatusCompleted);
         for (i = 0; i < count; i++)
         {
            memset(actual, 0, sizeof(actual));
            [outputs[i] getBytes:actual bytesPerRow:outputs[i].width * bpp[i]
               fromRegion:MTLRegionMake2D(0, 0, outputs[i].width, outputs[i].height) mipmapLevel:0];
            for (NSUInteger row = 0; row < outputs[i].height; row++)
            {
               NSUInteger x = count == 1 ? 2 : 0, y = count == 1 ? 2 : 0;
               assert(!memcmp(bytes + ((row + y) * inputs[i].width + x) * bpp[i],
                  actual + row * outputs[i].width * bpp[i], outputs[i].width * bpp[i]));
            }
         }
      }
   }
}
int main(void)
{
   @autoreleasepool
   {
      id<MTLDevice> device = MTLCreateSystemDefaultDevice();
      if (!device || ![device respondsToSelector:@selector(newSharedEvent)])
      { puts("SKIP: Metal shared events unavailable"); return 77; }
      test_sdr_native(device);
      test_cv_native(device);
      unsigned formats[] = { IMAGE_PIXFMT_8888, IMAGE_PIXFMT_FP16, IMAGE_PIXFMT_NV12, IMAGE_PIXFMT_P010 };
      for (unsigned i = 0; i < 4; i++) { test(device, formats[i], false); test(device, formats[i], true); }
      puts("Metal lease native: eight GPU/discard cases passed");
      test_import(device, MTLPixelFormatBGRA8Unorm, 7, 5);
      test_import(device, MTLPixelFormatBGRA8Unorm, 5, 5);
      test_import(device, MTLPixelFormatBGRA8Unorm, 7, 3);
      test_import(device, MTLPixelFormatRGBA8Unorm, 7, 5);
      test_import(device, MTLPixelFormatRGBA16Float, 7, 5);
      puts("Metal importer native: reuse, width, height, channel order and FP16 replacement passed");
   }
   return 0;
}
