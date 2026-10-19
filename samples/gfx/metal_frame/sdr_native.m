/* SHIPPING_SDR */
#include "sdr_pixels.h"

static unsigned sdr_code(unsigned f, unsigned x, unsigned y, unsigned plane, unsigned channel)
{
   unsigned scale = f >= 2 ? 4 : 1, maximum = f >= 2 ? 1023 : 255;
   unsigned black = f & 1 ? 0 : 16 * scale, white = f & 1 ? maximum : 235 * scale;
   unsigned center = 128 * scale, low = f & 1 ? 0 : 16 * scale, high = f & 1 ? maximum : 240 * scale;
   unsigned ys[10] = { black, black + 1, black + 4 * scale, black + 18 * scale,
      (black + white) / 2, white - 1, white, white < maximum ? white + 1 : maximum, maximum, 0 };
   unsigned cb[5] = { center, center - 40 * scale, center + 40 * scale, low, high };
   unsigned cr[5] = { center, center + 24 * scale, center - 48 * scale, high, low };
   if (!plane) return ys[(x + y * 3) % 10];
   return channel ? cr[(x * 2 + y * 3) % 5] : cb[(x + y) % 5];
}
/* NATIVE_IMPORT */
static void test_sdr_native(id<MTLDevice> device)
{
   const OSType formats[] = { kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
      kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,
      kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange,
      kCVPixelFormatType_420YpCbCr10BiPlanarFullRange };
   CVMetalTextureCacheRef cache = NULL;
   void *pipeline = metal_frame_sdr_pipeline_create((__bridge void *)device);
   unsigned worst = 0;
   NativeContext *context = [NativeContext new];
   MetalDriver *driver = [MetalDriver new];
   uintptr_t handle = 0, original = 0;
   __weak id<MTLTexture> previous = nil;
   context->_device = device; context->_frameSDRPipeline = pipeline;
   driver.context = context;
   MTLSamplerDescriptor *sampler = [MTLSamplerDescriptor new];
   context->_samplers[TEXTURE_FILTER_LINEAR] = [device newSamplerStateWithDescriptor:sampler];
   assert(pipeline && !metal_frame_sdr_pipeline_create(NULL));
   assert(CVMetalTextureCacheCreate(NULL, NULL, device, NULL, &cache) == kCVReturnSuccess);
   for (unsigned f = 0; f < 4; f++) for (unsigned srgb = 0; srgb < 2; srgb++)
   {
      @autoreleasepool
      {
         CVPixelBufferRef pixels = NULL;
         NSDictionary *attrs = @{ (__bridge id)kCVPixelBufferMetalCompatibilityKey: @YES,
            (__bridge id)kCVPixelBufferIOSurfacePropertiesKey: @{} };
         metal_frame_source_t source;
         video_hw_frame_t frame;
         video_frame_import_t imported;
         id<MTLCommandQueue> producer = [device newCommandQueue], consumer = [device newCommandQueue];
         id<MTLSharedEvent> ready = [device newSharedEvent], done = [device newSharedEvent];
         MTLSharedEventListener *listener = [[MTLSharedEventListener alloc]
            initWithDispatchQueue:dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0)];
         assert(producer && consumer && ready && done && listener);
         assert(CVPixelBufferCreate(NULL, 10, 8, formats[f], (__bridge CFDictionaryRef)attrs, &pixels) == kCVReturnSuccess);
         assert(CVPixelBufferLockBaseAddress(pixels, 0) == kCVReturnSuccess);
         for (unsigned plane = 0; plane < 2; plane++)
         {
            unsigned width = (unsigned)CVPixelBufferGetWidthOfPlane(pixels, plane);
            unsigned height = (unsigned)CVPixelBufferGetHeightOfPlane(pixels, plane);
            size_t stride = CVPixelBufferGetBytesPerRowOfPlane(pixels, plane);
            unsigned char *base = CVPixelBufferGetBaseAddressOfPlane(pixels, plane);
            assert(base);
            for (unsigned y = 0; y < height; y++) for (unsigned x = 0; x < width; x++)
               for (unsigned channel = 0; channel < (plane ? 2u : 1u); channel++)
            {
               unsigned code = sdr_code(f, x, y, plane, channel);
               unsigned index = x * (plane ? 2u : 1u) + channel;
               if (f < 2) base[y * stride + index] = (unsigned char)code;
               else { uint16_t sample = (uint16_t)(code << 6); memcpy(base + y * stride + index * 2, &sample, 2); }
            }
         }
         assert(CVPixelBufferUnlockBaseAddress(pixels, 0) == kCVReturnSuccess);
         memset(&source, 0, sizeof(source));
         source.device = (__bridge void *)device; source.ready = (__bridge void *)ready;
         source.done = (__bridge void *)done; source.listener = (__bridge void *)listener;
         source.ready_value = 3; source.done_value = 7;
         source.width = 7; source.height = 5; source.crop_x = 2; source.crop_y = 2;
         source.pixfmt = f < 2 ? IMAGE_PIXFMT_NV12 : IMAGE_PIXFMT_P010;
         source.color.primaries = 1; source.color.matrix = 1; source.color.transfer = srgb ? 13 : 1;
         assert(metal_frame_create_cv(&source, pixels, cache, &frame));
         CFRelease(pixels);
         memset(&imported, 0, sizeof(imported));
         imported.image = frame.image; imported.owner = frame.owner; imported.abandon = frame.abandon;
         imported.wait = frame.wait; imported.wait_value = frame.wait_value;
         imported.done = frame.done; imported.done_value = frame.done_value;
         imported.pixfmt = frame.pixfmt; imported.output_pixfmt = IMAGE_PIXFMT_8888;
         imported.dims = VIDEO_SCALE_PACK(7, 5); imported.crop_x = 2; imported.crop_y = 2;
         MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
            width:7 height:5 mipmapped:NO];
         desc.storageMode = MTLStorageModePrivate;
         desc.usage = MTLTextureUsageShaderWrite | MTLTextureUsageShaderRead;

         desc.storageMode = MTLStorageModeShared; desc.usage = MTLTextureUsageShaderRead;
         id<MTLTexture> readback = [device newTextureWithDescriptor:desc];
         id<MTLCommandBuffer> buffer = [consumer commandBuffer];
         assert(readback && buffer.retainedReferences);
         context->_buffer = buffer;
         assert(metal_import_texture((__bridge void *)driver, &handle, &imported,
            TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_DONE);
         if (!original) original = handle;
         assert(original == handle);
         Texture *texture = (__bridge Texture *)(void *)handle;
         id<MTLTexture> output = texture.texture;
         if (f || srgb) assert(output == previous);
         previous = output;
         assert(output.pixelFormat == MTLPixelFormatRGBA8Unorm
            && output.storageMode == MTLStorageModePrivate);
         if (f == 3 && srgb) { CFRelease((CFTypeRef)(void *)handle); handle = 0; }

         id<MTLBlitCommandEncoder> blit = [buffer blitCommandEncoder];
         assert(blit);
         [blit copyFromTexture:output sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
            sourceSize:MTLSizeMake(7, 5, 1) toTexture:readback destinationSlice:0 destinationLevel:0
            destinationOrigin:MTLOriginMake(0, 0, 0)];
         [blit endEncoding];
         output = nil; texture = nil;
         dispatch_semaphore_t finished = dispatch_semaphore_create(0);
         [buffer addCompletedHandler:^(id<MTLCommandBuffer> completed) {
            (void)completed; dispatch_semaphore_signal(finished);
         }];
         [buffer commit];
         assert(!done.signaledValue);
         id<MTLCommandBuffer> write = [producer commandBuffer];
         [write encodeSignalEvent:ready value:3]; [write commit];
         assert(!dispatch_semaphore_wait(finished, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC)));
         assert(buffer.status == MTLCommandBufferStatusCompleted && done.signaledValue == 7);
         unsigned char actual[140];
         [readback getBytes:actual bytesPerRow:28 fromRegion:MTLRegionMake2D(0, 0, 7, 5) mipmapLevel:0];
         for (unsigned i = 0; i < sizeof(actual); i++)
         {
            int delta = (int)actual[i] - (int)sdr_expected[f * 2 + srgb][i];
            unsigned error = (unsigned)(delta < 0 ? -delta : delta);
            if (error > worst) worst = error;
            if (error > 1) fprintf(stderr, "SDR format %u transfer %u byte %u: %u expected %u\n",
               f, srgb ? 13 : 1, i, (unsigned)actual[i], (unsigned)sdr_expected[f * 2 + srgb][i]);
            assert(error <= 1);
         }
      }
   }
   assert(!handle);
   CFRelease(cache); CFRelease((CFTypeRef)pipeline);
   printf("Metal SDR importer native: eight cropped NV12/P010 conversions passed, max error %u/255\n", worst);
}
