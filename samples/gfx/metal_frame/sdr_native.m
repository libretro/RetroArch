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
/* NATIVE_FRAME_VIEW */
/* HDR_NATIVE */
static unsigned sdr_native_finish(id<MTLCommandBuffer> buffer,
      id<MTLCommandQueue> producer, id<MTLSharedEvent> __strong const *ready_events,
      id<MTLSharedEvent> __strong const *done_events, id<MTLTexture> __strong const *readbacks,
      const unsigned *widths, const unsigned *heights, unsigned first, unsigned count)
{
   unsigned worst = 0;
   /* Test-only bounded completion; every import is pending together. */
   dispatch_semaphore_t finished = dispatch_semaphore_create(0);
   [buffer addCompletedHandler:^(id<MTLCommandBuffer> completed) {
      (void)completed; dispatch_semaphore_signal(finished);
   }];
   [buffer commit];
   for (unsigned i = first; i < first + count; i++) assert(!done_events[i].signaledValue);
   id<MTLCommandBuffer> write = [producer commandBuffer];
   for (unsigned i = first; i < first + count; i++) [write encodeSignalEvent:ready_events[i] value:3];
   [write commit];
   assert(!dispatch_semaphore_wait(finished, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC)));
   assert(buffer.status == MTLCommandBufferStatusCompleted);
   for (unsigned i = first; i < first + count; i++)
   {
      unsigned char actual[140];
      unsigned width = widths[i], height = heights[i];
      assert(done_events[i].signaledValue == 7);
      [readbacks[i] getBytes:actual bytesPerRow:width * 4
         fromRegion:MTLRegionMake2D(0, 0, width, height) mipmapLevel:0];
      for (unsigned y = 0; y < height; y++) for (unsigned x = 0; x < width; x++)
         for (unsigned channel = 0; channel < 4; channel++)
      {
         unsigned offset = (y * width + x) * 4 + channel;
         unsigned expected = sdr_expected[i][(y * 7 + x) * 4 + channel];
         int delta = (int)actual[offset] - (int)expected;
         unsigned error = (unsigned)(delta < 0 ? -delta : delta);
         if (error > worst) worst = error;
         if (error > 1) fprintf(stderr, "SDR batch frame %u pixel %u,%u channel %u: %u expected %u\n",
            i, x, y, channel, (unsigned)actual[offset], expected);
         assert(error <= 1);
      }
   }
   return worst;
}

static void test_sdr_native_mode(id<MTLDevice> device, bool batch, bool movie)
{
   const OSType formats[] = { kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
      kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,
      kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange,
      kCVPixelFormatType_420YpCbCr10BiPlanarFullRange };
   CVMetalTextureCacheRef cache = NULL;
   void *pipeline = metal_frame_sdr_pipeline_create((__bridge void *)device);
   unsigned worst = 0;
   const unsigned widths[8] = { 7, batch ? 6u : 7u, 7, 7, batch ? 6u : 7u, 7, 7, 7 };
   const unsigned heights[8] = { 5, 5, batch ? 4u : 5u, 5, batch ? 4u : 5u, 5, 5, 5 };
   id<MTLTexture> readbacks[8];
   id<MTLSharedEvent> ready_events[8], done_events[8];
   id<MTLCommandQueue> producer = [device newCommandQueue], consumer = [device newCommandQueue];
   id<MTLCommandBuffer> buffer = [consumer commandBuffer];
   NativeContext *context = [NativeContext new];
   MetalDriver *driver = [MetalDriver new];
   FrameView *movie_view = [FrameView new];
   struct frame_shader history = { 2 };
   movie_view->_context = context; movie_view->_shader = &history;
   movie_view->init_history = YES;
   uintptr_t handle = 0, original = 0;
   __weak id<MTLTexture> previous = nil;
   context->_device = device; context->_frameSDRPipeline = pipeline;
   driver.context = context;
   context->_buffer = buffer;
   assert(producer && consumer && buffer.retainedReferences);
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
         unsigned index = f * 2 + srgb;
         unsigned width = widths[index], height = heights[index];
         metal_frame_source_t source;
         video_hw_frame_t frame;
         video_frame_import_t imported;
         id<MTLSharedEvent> ready = [device newSharedEvent], done = [device newSharedEvent];
         MTLSharedEventListener *listener = [[MTLSharedEventListener alloc]
            initWithDispatchQueue:dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0)];
         assert(ready && done && listener);
         ready_events[index] = ready; done_events[index] = done;
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
         source.width = width; source.height = height; source.crop_x = 2; source.crop_y = 2;
         source.pixfmt = f < 2 ? IMAGE_PIXFMT_NV12 : IMAGE_PIXFMT_P010;
         source.color.primaries = 1; source.color.matrix = 1; source.color.transfer = srgb ? 13 : 1;
         CVBufferSetAttachment(pixels, kCVImageBufferColorPrimariesKey,
            kCVImageBufferColorPrimaries_ITU_R_709_2, kCVAttachmentMode_ShouldPropagate);
         CVBufferSetAttachment(pixels, kCVImageBufferTransferFunctionKey,
            srgb ? kCVImageBufferTransferFunction_sRGB : kCVImageBufferTransferFunction_ITU_R_709_2,
            kCVAttachmentMode_ShouldPropagate);
         CVBufferSetAttachment(pixels, kCVImageBufferYCbCrMatrixKey,
            kCVImageBufferYCbCrMatrix_ITU_R_709_2, kCVAttachmentMode_ShouldPropagate);
         source.color.primaries = source.color.transfer = source.color.matrix = 0;
         assert(metal_frame_create_cv(&source, pixels, cache, &frame));
         assert(frame.color.primaries == 1 && frame.color.transfer == (srgb ? 13u : 1u)
            && frame.color.matrix == 1);
         CFRelease(pixels);
         memset(&imported, 0, sizeof(imported));
         imported.image = frame.image; imported.owner = frame.owner; imported.abandon = frame.abandon;
         imported.wait = frame.wait; imported.wait_value = frame.wait_value;
         imported.done = frame.done; imported.done_value = frame.done_value;
         imported.pixfmt = frame.pixfmt; imported.output_pixfmt = IMAGE_PIXFMT_8888;
         imported.dims = VIDEO_SCALE_PACK(width, height); imported.crop_x = 2; imported.crop_y = 2;
         MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
            width:width height:height mipmapped:NO];
         desc.storageMode = MTLStorageModeShared; desc.usage = MTLTextureUsageShaderRead;
         id<MTLTexture> readback = [device newTextureWithDescriptor:desc];
         assert(readback && buffer.retainedReferences);
         readbacks[index] = readback;
         Texture *texture = nil;
         id<MTLTexture> output;
         if (movie)
         {
            assert([movie_view importMovieFrame:&imported] == VIDEO_TEXTURE_UPDATE_DONE);
            output = movie_view->_engine.frame.texture[0].view;
            if (index) assert(movie_view->_engine.frame.texture[1].view == previous);
         }
         else
         {
            assert(metal_import_texture((__bridge void *)driver, &handle, &imported,
               TEXTURE_FILTER_LINEAR) == VIDEO_TEXTURE_UPDATE_DONE);
            if (!original) original = handle;
            assert(original == handle);
            texture = (__bridge Texture *)(void *)handle; output = texture.texture;
         }
         if (index)
         {
            bool replaced = movie || width != widths[index - 1] || height != heights[index - 1];
            assert(previous && (output != previous) == replaced);
         }
         previous = output;
         assert(output.pixelFormat == MTLPixelFormatRGBA8Unorm
            && (movie || output.storageMode == MTLStorageModePrivate)
            && output.width == width && output.height == height
            && (output.usage & MTLTextureUsageShaderWrite));
         if (!movie && f == 3 && srgb) { CFRelease((CFTypeRef)(void *)handle); handle = 0; }

         id<MTLBlitCommandEncoder> blit = [buffer blitCommandEncoder];
         assert(blit);
         [blit copyFromTexture:output sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
            sourceSize:MTLSizeMake(width, height, 1) toTexture:readback destinationSlice:0 destinationLevel:0
            destinationOrigin:MTLOriginMake(0, 0, 0)];
         [blit endEncoding];
         output = nil; texture = nil;
         if (!batch)
         {
            unsigned error = sdr_native_finish(buffer, producer, ready_events, done_events,
               readbacks, widths, heights, index, 1);
            if (error > worst) worst = error;
            if (index < 7)
            {
               buffer = [consumer commandBuffer];
               context->_buffer = buffer;
            }
         }
      }
   }
   assert(!handle);
   if (movie)
   {
      id<MTLTexture> previous_front = movie_view->_engine.frame.texture[0].view;
      assert([movie_view importMovieFrame:NULL] == VIDEO_TEXTURE_UPDATE_DONE);
      movie_view->_format = RPixelFormatBGRA8Unorm; [movie_view _ensureFront];
      assert(movie_view->_engine.frame.texture[0].view.pixelFormat == MTLPixelFormatBGRA8Unorm);
      assert(movie_view->_engine.frame.texture[0].view != previous_front);
      movie_view->_format = RPixelFormatBGR10A2Unorm; [movie_view _ensureFront];
      assert(movie_view->_engine.frame.texture[0].view.pixelFormat == MTLPixelFormatBGR10A2Unorm);
   }
   context->_frameSDRPipeline = NULL;
   CFRelease((CFTypeRef)pipeline);
   if (batch)
   {
      assert(previous);
      worst = sdr_native_finish(buffer, producer, ready_events, done_events,
         readbacks, widths, heights, 0, 8);
   }
   CFRelease(cache);
   if (movie)
      printf("Metal movie native: eight cropped conversions, history and same-size CPU transitions passed (%s), max error %u/255\n", batch ? "batch" : "serial", worst);
   else if (batch)
      printf("Metal SDR batch native: eight pending leases, five replacements, two reuses and unload passed, max error %u/255\n", worst);
   else
      printf("Metal SDR importer native: eight cropped NV12/P010 conversions passed, max error %u/255\n", worst);
}

static void test_sdr_native(id<MTLDevice> device)
{
   test_hdr_native(device);
   test_sdr_native_mode(device, false, false);
   test_sdr_native_mode(device, true, false);
   test_sdr_native_mode(device, false, true);
   test_sdr_native_mode(device, true, true);
}
