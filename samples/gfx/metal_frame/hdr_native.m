/* GPU-only production conversion; uploads, readback and waits are fixture-only. */
#include <math.h>
#include "hdr_pixels.h"
static double hdr_half(uint16_t value)
{
   unsigned exponent = (value >> 10) & 31, fraction = value & 1023;
   double result;
   assert(exponent != 31);
   result = exponent ? ldexp(1.0 + fraction / 1024.0, (int)exponent - 15)
      : ldexp((double)fraction, -24);
   return value & 32768 ? -result : result;
}
static void test_hdr_native(id<MTLDevice> device)
{
   const OSType formats[4] = { kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
      kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,
      kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange,
      kCVPixelFormatType_420YpCbCr10BiPlanarFullRange };
   for (unsigned f = 0; f < 4; f++) for (unsigned hlg = 0; hlg < 2; hlg++)
      for (unsigned movie = 0; movie < 2; movie++)
      for (unsigned position = 0; position < 3; position++)
   {
      @autoreleasepool
      {
         CVPixelBufferRef pixels = NULL;
         CVMetalTextureCacheRef cache = NULL;
         NSDictionary *attrs = @{ (__bridge id)kCVPixelBufferMetalCompatibilityKey: @YES,
            (__bridge id)kCVPixelBufferIOSurfacePropertiesKey: @{} };
         assert(CVPixelBufferCreate(NULL, 10, 8, formats[f], (__bridge CFDictionaryRef)attrs, &pixels) == kCVReturnSuccess);
         assert(CVPixelBufferLockBaseAddress(pixels, 0) == kCVReturnSuccess);
         for (unsigned plane = 0; plane < 2; plane++)
         {
            size_t stride = CVPixelBufferGetBytesPerRowOfPlane(pixels, plane);
            unsigned char *base = CVPixelBufferGetBaseAddressOfPlane(pixels, plane);
            assert(base);
            for (unsigned y = 0; y < CVPixelBufferGetHeightOfPlane(pixels, plane); y++)
               for (unsigned x = 0; x < CVPixelBufferGetWidthOfPlane(pixels, plane); x++)
                  for (unsigned c = 0; c < (plane ? 2u : 1u); c++)
                  {
                     unsigned code = sdr_code(f, x, y, plane, c), at = x * (plane ? 2u : 1u) + c;
                     if (f < 2) base[y * stride + at] = (unsigned char)code;
                     else { uint16_t sample = (uint16_t)(code << 6); memcpy(base + y * stride + at * 2, &sample, 2); }
                  }
         }
         assert(CVPixelBufferUnlockBaseAddress(pixels, 0) == kCVReturnSuccess);
         CVBufferSetAttachment(pixels, kCVImageBufferColorPrimariesKey,
               kCVImageBufferColorPrimaries_ITU_R_2020, kCVAttachmentMode_ShouldPropagate);
         CVBufferSetAttachment(pixels, kCVImageBufferYCbCrMatrixKey,
               kCVImageBufferYCbCrMatrix_ITU_R_2020, kCVAttachmentMode_ShouldPropagate);
         CVBufferSetAttachment(pixels, kCVImageBufferTransferFunctionKey,
               hlg ? kCVImageBufferTransferFunction_ITU_R_2100_HLG : kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ,
               kCVAttachmentMode_ShouldPropagate);
         id<MTLSharedEvent> ready = [device newSharedEvent], done = [device newSharedEvent];
         MTLSharedEventListener *listener = [[MTLSharedEventListener alloc]
            initWithDispatchQueue:dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0)];
         metal_frame_source_t source;
         video_hw_frame_t frame;
         video_frame_import_t imported;
         memset(&source, 0, sizeof(source)); memset(&imported, 0, sizeof(imported));
         source.device = (__bridge void*)device; source.ready = (__bridge void*)ready;
         source.done = (__bridge void*)done; source.listener = (__bridge void*)listener;
         source.ready_value = 3; source.done_value = 7;
         source.width = 7; source.height = 5; source.crop_x = source.crop_y = 2;
         source.chroma_position = position;
         source.pixfmt = f < 2 ? IMAGE_PIXFMT_NV12 : IMAGE_PIXFMT_P010;
         assert(ready && done && listener);
         assert(CVMetalTextureCacheCreate(NULL, NULL, device, NULL, &cache) == kCVReturnSuccess);
         assert(metal_frame_create_cv(&source, pixels, cache, &frame));
         assert(frame.color.primaries == 9 && frame.color.matrix == 9
               && frame.color.transfer == (hlg ? 18u : 16u));
         CFRelease(pixels); CFRelease(cache);
         imported.image = frame.image; imported.owner = frame.owner; imported.abandon = frame.abandon;
         imported.wait = frame.wait; imported.wait_value = frame.wait_value;
         imported.done = frame.done; imported.done_value = frame.done_value;
         imported.pixfmt = frame.pixfmt; imported.output_pixfmt = IMAGE_PIXFMT_FP16;
         imported.dims = VIDEO_SCALE_PACK(7, 5); imported.crop_x = imported.crop_y = 2;
         imported.color = frame.color;
         id<MTLCommandQueue> queue = [device newCommandQueue];
         id<MTLCommandBuffer> buffer = [queue commandBuffer];
         void *pipeline = metal_frame_sdr_pipeline_create((__bridge void*)device);
         MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
            MTLPixelFormatRGBA16Float width:7 height:5 mipmapped:NO];
         desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
         desc.storageMode = MTLStorageModeShared;
         id<MTLTexture> output = [device newTextureWithDescriptor:desc];
         assert(queue && buffer.retainedReferences && pipeline && output);
         assert(!metal_frame_convert_sdr(&imported, (__bridge void*)device, (__bridge void*)buffer,
                  pipeline, (__bridge void*)output));
         if (movie)
         {
            NativeContext *context = [NativeContext new];
            FrameView *view = [FrameView new];
            struct frame_shader history = { 2 };
            context->_device = device; context->_buffer = buffer; context->_frameSDRPipeline = pipeline;
            view->_context = context; view->_shader = &history; view->init_history = YES;
            assert([view importMovieFrame:&imported] == VIDEO_TEXTURE_UPDATE_DONE && view->_movieHDR);
            for (unsigned i = 0; i < 3; i++)
               assert(view->_engine.frame.texture[i].view.pixelFormat == MTLPixelFormatRGBA16Float);
            id<MTLBlitCommandEncoder> copy = [buffer blitCommandEncoder]; assert(copy);
            [copy copyFromTexture:view->_engine.frame.texture[0].view sourceSlice:0 sourceLevel:0
               sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(7, 5, 1)
               toTexture:output destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
            [copy endEncoding];
            assert([view importMovieFrame:NULL] == VIDEO_TEXTURE_UPDATE_DONE && !view->_movieHDR);
            [view _ensureFront];
            assert(view->_engine.frame.texture[0].view.pixelFormat == MTLPixelFormatBGRA8Unorm);
            context->_frameSDRPipeline = NULL;
         }
         else
            assert(metal_frame_convert_hdr(&imported, (__bridge void*)device, (__bridge void*)buffer,
                     pipeline, (__bridge void*)output));
         CFRelease(pipeline);
         dispatch_semaphore_t complete = dispatch_semaphore_create(0);
         [buffer addCompletedHandler:^(id<MTLCommandBuffer> completed) { (void)completed; dispatch_semaphore_signal(complete); }];
         [buffer commit];
         assert(!done.signaledValue);
         /* Separate queue avoids placing the readiness signal behind its wait. */
         id<MTLCommandQueue> producer = [device newCommandQueue];
         id<MTLCommandBuffer> signal = [producer commandBuffer]; [signal encodeSignalEvent:ready value:3]; [signal commit];
         assert(!dispatch_semaphore_wait(complete, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC)));
         assert(buffer.status == MTLCommandBufferStatusCompleted && done.signaledValue == 7);
         uint16_t actual[140];
         [output getBytes:actual bytesPerRow:7 * 8 fromRegion:MTLRegionMake2D(0, 0, 7, 5) mipmapLevel:0];
         unsigned negative = 0, bright = 0;
         for (unsigned i = 0; i < 140; i++)
         {
            double expected = hdr_expected[position * 8 + f * 2 + hlg][i], value = hdr_half(actual[i]);
            double tolerance = fmax(0.002, fabs(expected) * 0.002);
            if ((i & 3) == 3) assert(actual[i] == 0x3c00);
            if (fabs(value - expected) > tolerance)
               fprintf(stderr, "HDR case %u component %u: %.9g expected %.9g\n", f * 2 + hlg, i, value, expected);
            assert(fabs(value - expected) <= tolerance);
            if (value < 0) negative++;
            if (value > 1) bright++;
         }
         assert(negative && bright);
      }
   }
   puts("Metal HDR native: 48 cropped PQ/HLG/chroma conversions, movie history and CPU transitions preserve linear scRGB");
}
