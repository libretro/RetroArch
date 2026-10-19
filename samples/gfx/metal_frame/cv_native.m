#import <CoreVideo/CoreVideo.h>
/* SHIPPING_CV */

typedef struct cv_native_copy
{
   void *output[2];
} cv_native_copy_t;
static bool cv_native_encode(void *user, const metal_frame_view_t *view, void *commands)
{
   cv_native_copy_t *copy = user;
   id<MTLCommandBuffer> buffer = (__bridge id<MTLCommandBuffer>)commands;
   id<MTLBlitCommandEncoder> encoder = [buffer blitCommandEncoder];
   if (!encoder) return false;
   assert(view->color.max_cll == 1500 && view->color.transfer == 16 && view->color.primaries == 9);
   for (unsigned i = 0; i < 2; i++)
   {
      id<MTLTexture> input = (__bridge id<MTLTexture>)view->planes[i];
      [encoder copyFromTexture:input sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
         sourceSize:MTLSizeMake(input.width, input.height, 1)
         toTexture:(__bridge id<MTLTexture>)copy->output[i] destinationSlice:0 destinationLevel:0
         destinationOrigin:MTLOriginMake(0, 0, 0)];
   }
   [encoder endEncoding];
   return true;
}
static void test_cv_native(id<MTLDevice> device)
{
   const OSType formats[] = { kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange,
      kCVPixelFormatType_420YpCbCr8BiPlanarFullRange,
      kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange,
      kCVPixelFormatType_420YpCbCr10BiPlanarFullRange };
   for (unsigned f = 0; f < 4; f++) for (unsigned discard = 0; discard < 2; discard++)
   {
      @autoreleasepool
      {
         CVPixelBufferRef pixels = NULL;
         CVMetalTextureCacheRef cache = NULL;
         NSDictionary *attrs = @{ (__bridge id)kCVPixelBufferMetalCompatibilityKey: @YES,
            (__bridge id)kCVPixelBufferIOSurfacePropertiesKey: @{} };
         id<MTLCommandQueue> producer = [device newCommandQueue], consumer = [device newCommandQueue];
         id<MTLSharedEvent> ready = [device newSharedEvent], done = [device newSharedEvent];
         MTLSharedEventListener *listener = [[MTLSharedEventListener alloc]
            initWithDispatchQueue:dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0)];
         id<MTLTexture> outputs[2];
         cv_native_copy_t copy;
         unsigned char expected[2][256], actual[256];
         unsigned row_bytes[2], heights[2];
         metal_frame_source_t source;
         video_hw_frame_t frame;
         video_frame_import_t imported;
         assert(producer && consumer && ready && done && listener);
         assert(CVPixelBufferCreate(NULL, 10, 8, formats[f], (__bridge CFDictionaryRef)attrs, &pixels) == kCVReturnSuccess);
         assert(CVMetalTextureCacheCreate(NULL, NULL, device, NULL, &cache) == kCVReturnSuccess);
         assert(CVPixelBufferLockBaseAddress(pixels, 0) == kCVReturnSuccess);
         for (unsigned i = 0; i < 2; i++)
         {
            unsigned width = (unsigned)CVPixelBufferGetWidthOfPlane(pixels, i);
            unsigned height = (unsigned)CVPixelBufferGetHeightOfPlane(pixels, i);
            unsigned bpp = (f < 2 ? 1 : 2) * (i ? 2 : 1);
            unsigned stride = (unsigned)CVPixelBufferGetBytesPerRowOfPlane(pixels, i);
            unsigned char *base = CVPixelBufferGetBaseAddressOfPlane(pixels, i);
            assert(base && width * height * bpp <= sizeof(expected[i]));
            row_bytes[i] = width * bpp; heights[i] = height;
            for (unsigned y = 0; y < height; y++) for (unsigned x = 0; x < width * (i ? 2 : 1); x++)
            {
               unsigned value = (x * 31 + y * 17 + i * 43) % (f < 2 ? 256 : 1024);
               if (f < 2) expected[i][y * row_bytes[i] + x] = (unsigned char)value;
               else { uint16_t sample = (uint16_t)(value << 6); memcpy(expected[i] + y * row_bytes[i] + x * 2, &sample, 2); }
            }
            for (unsigned y = 0; y < height; y++) memcpy(base + y * stride, expected[i] + y * row_bytes[i], row_bytes[i]);
            MTLPixelFormat format = f < 2 ? (i ? MTLPixelFormatRG8Unorm : MTLPixelFormatR8Unorm)
               : (i ? MTLPixelFormatRG16Unorm : MTLPixelFormatR16Unorm);
            MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
               width:width height:height mipmapped:NO];
            desc.storageMode = MTLStorageModeShared;
            outputs[i] = [device newTextureWithDescriptor:desc];
            assert(outputs[i]); copy.output[i] = (__bridge void *)outputs[i];
         }
         assert(CVPixelBufferUnlockBaseAddress(pixels, 0) == kCVReturnSuccess);
         memset(&source, 0, sizeof(source));
         source.device = (__bridge void *)device; source.ready = (__bridge void *)ready;
         source.done = (__bridge void *)done; source.listener = (__bridge void *)listener;
         source.ready_value = 3; source.done_value = 7;
         source.width = 7; source.height = 5; source.crop_x = 2; source.crop_y = 2;
         source.pixfmt = f < 2 ? IMAGE_PIXFMT_NV12 : IMAGE_PIXFMT_P010;
         source.color.max_cll = 1500; source.color.transfer = 16; source.color.primaries = 9;
         CVBufferSetAttachment(pixels, kCVImageBufferColorPrimariesKey,
            kCVImageBufferColorPrimaries_ITU_R_2020, kCVAttachmentMode_ShouldPropagate);
         CVBufferSetAttachment(pixels, kCVImageBufferTransferFunctionKey,
            kCVImageBufferTransferFunction_SMPTE_ST_2084_PQ, kCVAttachmentMode_ShouldPropagate);
         CVBufferSetAttachment(pixels, kCVImageBufferYCbCrMatrixKey,
            kCVImageBufferYCbCrMatrix_ITU_R_2020, kCVAttachmentMode_ShouldPropagate);
         assert(metal_frame_create_cv(&source, pixels, cache, &frame));
         assert(frame.color.matrix == 9);
         assert(frame.color.range == (f & 1 ? 2u : 1u) && frame.crop_x == 2 && frame.crop_y == 2);
         CFRelease(pixels); CFRelease(cache);
         dispatch_semaphore_t finished = dispatch_semaphore_create(0);
         [done notifyListener:listener atValue:7 block:^(id<MTLSharedEvent> event, uint64_t value) {
            (void)event; (void)value; dispatch_semaphore_signal(finished);
         }];
         id<MTLCommandBuffer> buffer = [consumer commandBuffer];
         if (discard) video_hw_frame_abandon(&frame);
         else
         {
            memset(&imported, 0, sizeof(imported));
            imported.image = frame.image; imported.owner = frame.owner; imported.abandon = frame.abandon;
            imported.wait = frame.wait; imported.wait_value = frame.wait_value;
            imported.done = frame.done; imported.done_value = frame.done_value;
            imported.pixfmt = frame.pixfmt; imported.dims = VIDEO_SCALE_PACK(7, 5);
            imported.crop_x = 2; imported.crop_y = 2;
            assert(metal_frame_encode(&imported, (__bridge void *)device,
               (__bridge void *)buffer, cv_native_encode, &copy));
            [buffer commit];
         }
         assert(!done.signaledValue);
         id<MTLCommandBuffer> write = [producer commandBuffer];
         [write encodeSignalEvent:ready value:3]; [write commit];
         assert(!dispatch_semaphore_wait(finished, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC)));
         if (!discard)
         {
            [buffer waitUntilCompleted]; assert(buffer.status == MTLCommandBufferStatusCompleted);
            for (unsigned i = 0; i < 2; i++)
            {
               [outputs[i] getBytes:actual bytesPerRow:row_bytes[i]
                  fromRegion:MTLRegionMake2D(0, 0, outputs[i].width, outputs[i].height) mipmapLevel:0];
               assert(!memcmp(actual, expected[i], row_bytes[i] * heights[i]));
            }
         }
      }
   }
   puts("CoreVideo Metal native: eight NV12/P010 GPU/discard cases passed");
}
