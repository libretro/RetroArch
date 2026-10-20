#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <CoreVideo/CoreVideo.h>
#include <dispatch/dispatch.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* SHIPPING_METAL */

static bool metal_hevc_decode_enabled(void *unused) { (void)unused; return true; }
static void hevc_test_barrier(void *unused) { (void)unused; }
int videotoolbox_hevc_test_start(void *owner, void *device, int hdr)
{
   void *pipeline = metal_frame_sdr_pipeline_create(device);
   bool started;
   assert(!metal_hevc_start(NULL, device, pipeline));
   assert(!metal_hevc_start(owner, NULL, pipeline));
   assert(!metal_hevc_start(owner, device, NULL));
   started = metal_hevc_start_supported(owner, device, pipeline, hdr != 0);
   if (pipeline) CFRelease(pipeline);
   return started;
}
void videotoolbox_hevc_test_stop(void *owner) { metal_hevc_stop(owner); }
int videotoolbox_hevc_test_owns(void *owner) { return metal_hevc_owns(owner); }
void videotoolbox_hevc_test_retired(void)
{
   /* Fixture-only barrier: production retirement remains asynchronous. */
   dispatch_sync_f(metal_hevc_retirement_queue, NULL, hevc_test_barrier);
}


typedef struct hevc_metal_copy
{
   void *output[2];
} hevc_metal_copy_t;

static bool hevc_metal_encode(void *user, const metal_frame_view_t *view, void *commands)
{
   hevc_metal_copy_t *copy = user;
   id<MTLCommandBuffer> buffer = (__bridge id<MTLCommandBuffer>)commands;
   id<MTLBlitCommandEncoder> encoder = [buffer blitCommandEncoder];
   if (!encoder) return false;
   for (unsigned i = 0; i < 2; i++)
   {
      id<MTLTexture> input = (__bridge id<MTLTexture>)view->planes[i];
      [encoder copyFromTexture:input sourceSlice:0 sourceLevel:0
         sourceOrigin:MTLOriginMake(0, 0, 0)
         sourceSize:MTLSizeMake(input.width, input.height, 1)
         toTexture:(__bridge id<MTLTexture>)copy->output[i] destinationSlice:0
         destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
   }
   [encoder endEncoding];
   return true;
}

static uint32_t hevc_metal_crc(uint32_t crc, unsigned value, unsigned depth)
{
   for (unsigned byte = 0; byte < (depth == 10 ? 2u : 1u); byte++)
   {
      crc ^= (value >> (byte * 8)) & 255;
      for (unsigned bit = 0; bit < 8; bit++)
         crc = (crc >> 1) ^ ((crc & 1) ? UINT32_C(0xedb88320) : 0);
   }
   return crc;
}

void *videotoolbox_hevc_test_device(void *unused)
{
   id<MTLDevice> device = MTLCreateSystemDefaultDevice();
   (void)unused;
   return device ? (void*)CFRetain((__bridge CFTypeRef)device) : NULL;
}
void videotoolbox_hevc_test_device_release(void *device) { CFRelease(device); }
void *videotoolbox_hevc_test_device_retain(void *device) { return (void*)CFRetain(device); }

/* Consume an adapter-produced lease after adapter, session and pool teardown.
 * GPU readback and waits are confined to this pixel acceptance fixture. */
void videotoolbox_hevc_test_export(video_hw_frame_t *frame, unsigned depth,
      unsigned full, const uint32_t expected[3])
{
   @autoreleasepool
   {
      RArchMetalFrameLease *lease = (__bridge RArchMetalFrameLease*)frame->owner;
      id<MTLDevice> device = lease->device;
      id done = lease->done, listener = lease->listener;
      uint64_t value = frame->done_value;
      id<MTLCommandQueue> queue = [device newCommandQueue];
      id<MTLCommandBuffer> buffer = [queue commandBuffer];
      id<MTLTexture> output[2];
      hevc_metal_copy_t copy;
      assert(frame->width == 64 && frame->height == 64 && !frame->crop_x && !frame->crop_y);
      assert(frame->color.range == (full ? 2u : 1u));
      assert(frame->pixfmt == (depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12));
      for (unsigned i = 0; i < 2; i++)
      {
         MTLPixelFormat format = depth == 10
            ? (i ? MTLPixelFormatRG16Unorm : MTLPixelFormatR16Unorm)
            : (i ? MTLPixelFormatRG8Unorm : MTLPixelFormatR8Unorm);
         MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
            width:i ? 32 : 64 height:i ? 32 : 64 mipmapped:NO];
         desc.storageMode = MTLStorageModeShared;
         output[i] = [device newTextureWithDescriptor:desc]; assert(output[i]);
         copy.output[i] = (__bridge void*)output[i];
      }
      video_frame_import_t imported;
      memset(&imported, 0, sizeof(imported));
      imported.image = frame->image; imported.owner = frame->owner; imported.abandon = frame->abandon;
      imported.wait = frame->wait; imported.wait_value = frame->wait_value;
      imported.done = frame->done; imported.done_value = frame->done_value;
      imported.pixfmt = frame->pixfmt; imported.dims = VIDEO_SCALE_PACK(64, 64);
      imported.plane_views[0] = frame->plane_views[0]; imported.plane_views[1] = frame->plane_views[1];
      imported.layout = frame->layout; imported.yuv = frame->yuv; imported.color = frame->color;
      imported.array_slice = frame->array_slice; imported.crop_x = frame->crop_x; imported.crop_y = frame->crop_y;
      /* Test-only source readback before the shipping importer consumes the
       * one lease. Both operations share its readiness and retained buffer. */
      [(id<RArchMetalEventCommandBuffer>)buffer encodeWaitForEvent:(id)lease->ready value:frame->wait_value];
      assert(hevc_metal_encode(&copy, &lease->view, (__bridge void*)buffer));
      id<MTLTexture> front = hevc_movie_import(device, buffer, &imported);
      MTLTextureDescriptor *rgb_desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:
         (frame->color.transfer == 16 || frame->color.transfer == 18) ? MTLPixelFormatRGBA16Float : MTLPixelFormatRGBA8Unorm
         width:64 height:64 mipmapped:NO];
      rgb_desc.storageMode = MTLStorageModeShared;
      id<MTLTexture> rgb = [device newTextureWithDescriptor:rgb_desc]; assert(rgb);
      id<MTLBlitCommandEncoder> rgb_copy = [buffer blitCommandEncoder]; assert(rgb_copy);
      [rgb_copy copyFromTexture:front sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
         sourceSize:MTLSizeMake(64, 64, 1) toTexture:rgb destinationSlice:0 destinationLevel:0
         destinationOrigin:MTLOriginMake(0, 0, 0)];
      [rgb_copy endEncoding];
      unsigned transfer = frame->color.transfer;
      memset(frame, 0, sizeof(*frame));
      [buffer commit];
      pool_test_wait((__bridge void*)done, (__bridge void*)listener, value);
      [buffer waitUntilCompleted]; assert(buffer.status == MTLCommandBufferStatusCompleted);
      uint8_t *bytes = malloc(8192); assert(bytes);
      for (unsigned plane = 0; plane < 3; plane++)
      {
         unsigned index = plane ? 1 : 0, width = plane ? 32 : 64;
         unsigned step = plane ? 2 : 1, component = plane == 2 ? 1 : 0;
         unsigned stride = width * step * (depth == 10 ? 2 : 1);
         uint32_t crc = UINT32_MAX;
         [output[index] getBytes:bytes bytesPerRow:stride
            fromRegion:MTLRegionMake2D(0, 0, width, width) mipmapLevel:0];
         for (unsigned y = 0; y < width; y++) for (unsigned x = 0; x < width; x++)
         {
            const uint8_t *row = bytes + y * stride;
            unsigned sample = depth == 10 ? ((const uint16_t*)row)[x * step + component] >> 6
               : row[x * step + component];
            crc = hevc_metal_crc(crc, sample, depth);
         }
         assert((crc ^ UINT32_MAX) == expected[plane]);
      }
      free(bytes);
      hevc_movie_pixels(output[0], output[1], rgb, depth, full, transfer);
      puts("VideoToolbox native adapter: exported pixels survive decoder, session and pool teardown");
   }
}

/* Consumes the callback's owned image after decoder and slot teardown. */
void videotoolbox_hevc_metal_test(CVPixelBufferRef image, unsigned depth,
      unsigned full, const uint32_t expected[3])
{
   @autoreleasepool
   {
      id<MTLDevice> device = MTLCreateSystemDefaultDevice();
      if (!device || ![device respondsToSelector:@selector(newSharedEvent)])
      {
         CVPixelBufferRelease(image);
         puts("VideoToolbox Metal: shared events unavailable");
         exit(77);
      }
      CVMetalTextureCacheRef cache = NULL;
      id<MTLCommandQueue> queue = [device newCommandQueue];
      id<MTLSharedEvent> ready = [device newSharedEvent];
      id<MTLSharedEvent> done[2] = { [device newSharedEvent], [device newSharedEvent] };
      MTLSharedEventListener *listener = [[MTLSharedEventListener alloc]
         initWithDispatchQueue:dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0)];
      metal_frame_source_t source;
      video_hw_frame_t frame[2];
      id<MTLTexture> output[2];
      hevc_metal_copy_t copy;
      assert(queue && ready && done[0] && done[1] && listener);
      assert(!CVMetalTextureCacheCreate(NULL, NULL, device, NULL, &cache));
      {
         static unsigned pool_cases;
         if (pool_cases++ < 4)
            test_cv_pool((__bridge void*)device, cache, image,
                  depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12, 64, 64);
      }
      memset(&source, 0, sizeof(source));
      source.device = (__bridge void *)device;
      source.ready = (__bridge void *)ready;
      source.listener = (__bridge void *)listener;
      source.ready_value = source.done_value = 1;
      source.width = source.height = 64;
      source.pixfmt = depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
      source.color.range = full ? 2 : 1;
      source.color.primaries = source.color.transfer = source.color.matrix = 1;
      for (unsigned i = 0; i < 2; i++)
      {
         source.done = (__bridge void *)done[i];
         assert(metal_frame_create_cv(&source, image, cache, &frame[i]));
         assert(frame[i].color.range == (full ? 2u : 1u));
      }
      CVPixelBufferRelease(image);
      CFRelease(cache);
      for (unsigned i = 0; i < 2; i++)
      {
         MTLPixelFormat format = depth == 10
            ? (i ? MTLPixelFormatRG16Unorm : MTLPixelFormatR16Unorm)
            : (i ? MTLPixelFormatRG8Unorm : MTLPixelFormatR8Unorm);
         MTLTextureDescriptor *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format
            width:i ? 32 : 64 height:i ? 32 : 64 mipmapped:NO];
         desc.storageMode = MTLStorageModeShared;
         output[i] = [device newTextureWithDescriptor:desc];
         assert(output[i]); copy.output[i] = (__bridge void *)output[i];
      }
      video_frame_import_t imported;
      memset(&imported, 0, sizeof(imported));
      imported.image = frame[0].image; imported.owner = frame[0].owner;
      imported.abandon = frame[0].abandon;
      imported.wait = frame[0].wait; imported.wait_value = frame[0].wait_value;
      imported.done = frame[0].done; imported.done_value = frame[0].done_value;
      imported.pixfmt = frame[0].pixfmt; imported.dims = VIDEO_SCALE_PACK(64, 64);
      id<MTLCommandBuffer> buffer = [queue commandBuffer];
      assert(metal_frame_encode(&imported, (__bridge void *)device,
         (__bridge void *)buffer, hevc_metal_encode, &copy));
      dispatch_semaphore_t retired[2] = { dispatch_semaphore_create(0), dispatch_semaphore_create(0) };
      for (unsigned i = 0; i < 2; i++)
      {
         dispatch_semaphore_t finished = retired[i];
         [done[i] notifyListener:listener atValue:1 block:^(id<MTLSharedEvent> event, uint64_t value) {
            (void)event; (void)value; dispatch_semaphore_signal(finished);
         }];
      }
      video_hw_frame_abandon(&frame[1]);
      [buffer commit];
      assert(!done[0].signaledValue && !done[1].signaledValue);
      ready.signaledValue = 1;
      for (unsigned i = 0; i < 2; i++)
         assert(!dispatch_semaphore_wait(retired[i], dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC)));
      [buffer waitUntilCompleted];
      assert(buffer.status == MTLCommandBufferStatusCompleted && done[0].signaledValue == 1);
      uint8_t *bytes = malloc(8192);
      assert(bytes);
      for (unsigned plane = 0; plane < 3; plane++)
      {
         unsigned index = plane ? 1 : 0, width = plane ? 32 : 64;
         unsigned step = plane ? 2 : 1, component = plane == 2 ? 1 : 0;
         unsigned stride = width * step * (depth == 10 ? 2 : 1);
         uint32_t crc = UINT32_MAX;
         [output[index] getBytes:bytes bytesPerRow:stride
            fromRegion:MTLRegionMake2D(0, 0, width, width) mipmapLevel:0];
         for (unsigned y = 0; y < width; y++) for (unsigned x = 0; x < width; x++)
         {
            const uint8_t *row = bytes + y * stride;
            unsigned value = depth == 10 ? ((const uint16_t *)row)[x * step + component] >> 6
               : row[x * step + component];
            crc = hevc_metal_crc(crc, value, depth);
         }
         assert((crc ^ UINT32_MAX) == expected[plane]);
      }
      free(bytes);
      puts("VideoToolbox Metal: decoded planes and two leases survive source teardown");
   }
}
