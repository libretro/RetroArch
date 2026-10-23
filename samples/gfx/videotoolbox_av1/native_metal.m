/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "native_reference.h"

/* Appended to the existing native fixture with shipping Metal sections. */
void videotoolbox_av1_test_export(video_hw_frame_t *frame, unsigned depth,
      unsigned position, uint32_t expected_hash)
{
   @autoreleasepool
   {
      assert(frame && frame->owner && frame->api == VIDEO_HW_API_METAL);
      RArchMetalFrameLease *lease
            = (__bridge RArchMetalFrameLease *)frame->owner;
      id<MTLDevice> device = lease->device;
      id done = lease->done, listener = lease->listener;
      uint64_t value = frame->done_value;
      unsigned width = frame->width, height = frame->height;
      unsigned bytes = depth == 10 ? 2 : 1;
      id<MTLCommandQueue> queue = [device newCommandQueue];
      id<MTLCommandBuffer> buffer = [queue commandBuffer];
      id<MTLTexture> output[2];
      hevc_metal_copy_t copy;
      assert(width == 128 && height == 96 && !frame->crop_x && !frame->crop_y);
      assert(frame->pixfmt == (depth == 10 ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12));
      assert(frame->color.primaries == 1 && frame->color.transfer == 1
            && frame->color.matrix == 1 && frame->color.range == 1);
      assert(lease->view.chroma_position == position);
      assert(queue && buffer.retainedReferences);
      for (unsigned i = 0; i < 2; i++)
      {
         id<MTLTexture> source = lease->planes[i];
         assert(source.width == (i ? width / 2 : width)
               && source.height == (i ? height / 2 : height));
         MTLTextureDescriptor *desc = [MTLTextureDescriptor
               texture2DDescriptorWithPixelFormat:source.pixelFormat
                                            width:source.width
                                           height:source.height
                                        mipmapped:NO];
         desc.storageMode = MTLStorageModeShared;
         output[i] = [device newTextureWithDescriptor:desc];
         assert(output[i]);
         copy.output[i] = (__bridge void *)output[i];
      }
      video_frame_import_t imported;
      memset(&imported, 0, sizeof(imported));
      imported.image = frame->image;
      imported.owner = frame->owner;
      imported.abandon = frame->abandon;
      imported.wait = frame->wait;
      imported.wait_value = frame->wait_value;
      imported.done = frame->done;
      imported.done_value = frame->done_value;
      imported.pixfmt = frame->pixfmt;
      imported.dims = VIDEO_SCALE_PACK(width, height);
      imported.plane_views[0] = frame->plane_views[0];
      imported.plane_views[1] = frame->plane_views[1];
      imported.layout = frame->layout;
      imported.yuv = frame->yuv;
      imported.color = frame->color;
      imported.array_slice = frame->array_slice;
      [(id<RArchMetalEventCommandBuffer>)buffer
            encodeWaitForEvent:(id)lease->ready
                         value:frame->wait_value];
      assert(hevc_metal_encode(&copy, &lease->view, (__bridge void *)buffer));
      id<MTLTexture> front = hevc_movie_import(device, buffer, &imported);
      MTLTextureDescriptor *desc = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                         width:width
                                        height:height
                                     mipmapped:NO];
      desc.storageMode = MTLStorageModeShared;
      id<MTLTexture> rgb = [device newTextureWithDescriptor:desc];
      id<MTLBlitCommandEncoder> encoder = [buffer blitCommandEncoder];
      assert(rgb && encoder && front.width == width && front.height == height);
      [encoder copyFromTexture:front
                   sourceSlice:0
                   sourceLevel:0
                  sourceOrigin:MTLOriginMake(0, 0, 0)
                    sourceSize:MTLSizeMake(width, height, 1)
                     toTexture:rgb
              destinationSlice:0
              destinationLevel:0
             destinationOrigin:MTLOriginMake(0, 0, 0)];
      [encoder endEncoding];
      memset(frame, 0, sizeof(*frame));
      lease = nil;
      assert([done signaledValue] < value);
      [buffer commit];
      /* Readbacks and bounded completion belong only to this fixture. */
      pool_test_wait((__bridge void *)done, (__bridge void *)listener, value);
      assert(buffer.status == MTLCommandBufferStatusCompleted);
      uint8_t *y = malloc((size_t)width * height * bytes);
      uint8_t *uv = malloc((size_t)width * (height / 2) * bytes);
      uint8_t *rgba = malloc((size_t)width * height * 4);
      assert(y && uv && rgba);
      [output[0] getBytes:y
              bytesPerRow:width * bytes
               fromRegion:MTLRegionMake2D(0, 0, width, height)
              mipmapLevel:0];
      [output[1] getBytes:uv
              bytesPerRow:width * bytes
               fromRegion:MTLRegionMake2D(0, 0, width / 2, height / 2)
              mipmapLevel:0];
      [rgb getBytes:rgba
            bytesPerRow:width * 4
             fromRegion:MTLRegionMake2D(0, 0, width, height)
            mipmapLevel:0];
      uint32_t hash = 2166136261u;
      for (unsigned plane = 0; plane < 3; plane++)
         for (unsigned row = 0; row < (plane ? height / 2 : height); row++)
            for (unsigned col = 0; col < (plane ? width / 2 : width); col++)
            {
               size_t at = plane ? (size_t)row * width + col * 2 + plane - 1
                                 : (size_t)row * width + col;
               unsigned code = av1_reference_code(plane ? uv : y, at, depth);
               hash = (hash ^ (code & 255)) * 16777619u;
               if (depth == 10)
                  hash = (hash ^ (code >> 8)) * 16777619u;
            }
      assert(hash == expected_hash);
      unsigned worst = 0;
      for (unsigned row = 0; row < height; row++)
         for (unsigned col = 0; col < width; col++)
         {
            uint8_t expected[4];
            assert(av1_reference_pixel(y, uv, width, height, depth, 0, position,
                  col, row, expected));
            for (unsigned channel = 0; channel < 4; channel++)
            {
               int delta = (int)rgba[((size_t)row * width + col) * 4 + channel]
                     - expected[channel];
               unsigned error = (unsigned)(delta < 0 ? -delta : delta);
               if (error > worst)
                  worst = error;
               if (error > 1)
                  fprintf(stderr,
                        "AV1 Metal %u-bit position %u pixel %u,%u channel %u "
                        "error %u\n",
                        depth, position, col, row, channel, error);
               assert(error <= 1);
            }
         }
      free(y);
      free(uv);
      free(rgba);
      printf("AV1 Metal decoded-plane hash and FrameView pixels: max error "
             "%u/255\n",
            worst);
   }
}
