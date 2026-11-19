/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later
 * Apple API oracle only; shipping host/codec code is compiled as C89.
 */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../../record/record_pyrowave_metal.h"
#define RPYROWAVE_SHADER_METAL
#include <rpyrowave_shaders.h>

static double metal_native_half(uint16_t value)
{
   unsigned exponent = (value >> 10) & 31;
   double result = exponent ? ldexp(1024.0 + (value & 1023), (int)exponent - 25)
      : ldexp((double)(value & 1023), -24);
   if (exponent == 31) return 1e30;
   return value & 32768 ? -result : result;
}

static double metal_native_clip(double value)
{
   return value < 0.0 ? 0.0 : value > 1.0 ? 1.0 : value;
}

static double metal_native_sample(const rpyrowave_image_t *image, int x, int y, unsigned component)
{
   unsigned shift = component ? 1 : 0;
   if (x < 0) x = 0;
   if (y < 0) y = 0;
   if ((unsigned)x >= image->sequence.width >> shift) x = (int)(image->sequence.width >> shift) - 1;
   if ((unsigned)y >= image->sequence.height >> shift) y = (int)(image->sequence.height >> shift) - 1;
   return image->planes[component][y * image->stride + x];
}

static double metal_native_chroma(const rpyrowave_image_t *image, unsigned x, unsigned y, unsigned component)
{
   double cx = x * 0.5 - 0.25;
   double cy = y * 0.5 - 0.25;
   int ix = (int)floor(cx);
   int iy = (int)floor(cy);
   double wx = cx - ix;
   double wy = cy - iy;
   return (1.0 - wy) * ((1.0 - wx) * metal_native_sample(image, ix, iy, component)
         + wx * metal_native_sample(image, ix + 1, iy, component))
      + wy * ((1.0 - wx) * metal_native_sample(image, ix, iy + 1, component)
         + wx * metal_native_sample(image, ix + 1, iy + 1, component));
}

static int metal_native_compare(const rpyrowave_image_t *image, const void *data, size_t pitch, double *maximum)
{
   unsigned x;
   unsigned y;
   for (y = 0; y < image->sequence.height; y++)
      for (x = 0; x < image->sequence.width; x++)
      {
         const uint16_t *pixel = (const uint16_t*)((const uint8_t*)data + y * pitch) + x * 4;
         double luma = metal_native_sample(image, (int)x, (int)y, 0);
         double cb = metal_native_chroma(image, x, y, 1) - 128.0 / 255.0;
         double cr = metal_native_chroma(image, x, y, 2) - 128.0 / 255.0;
         double expected[4];
         unsigned component;
         expected[0] = luma + 1.5748 * cr;
         expected[2] = luma + 1.8556 * cb;
         expected[1] = (luma - 0.2126 * expected[0] - 0.0722 * expected[2]) / 0.7152;
         expected[3] = 1.0;
         for (component = 0; component < 4; component++)
         {
            double difference = fabs(metal_native_half(pixel[component]) - metal_native_clip(expected[component]));
            if (!(difference < 0.003))
            {
               fprintf(stderr, "Metal RGB differs at %u,%u,%u: %.9f\n", x, y, component, difference);
               return 0;
            }
            if (difference > *maximum) *maximum = difference;
         }
      }
   return 1;
}

static int metal_native_encoder_compare(const rpyrowave_image_t *image, const float *rgb,
      unsigned stride, double *maximum)
{
   unsigned component;
   for (component = 0; component < 3; component++)
   {
      unsigned shift = component ? 1 : 0;
      unsigned x;
      unsigned y;
      for (y = 0; y < image->sequence.height >> shift; y++)
         for (x = 0; x < image->sequence.width >> shift; x++)
         {
            double channels[3] = { 0.0, 0.0, 0.0 };
            double luma;
            double expected;
            double difference;
            unsigned dx;
            unsigned dy;
            unsigned channel;
            for (dy = 0; dy <= shift; dy++)
               for (dx = 0; dx <= shift; dx++)
                  for (channel = 0; channel < 3; channel++)
                     channels[channel] += rgb[((3 + (y << shift) + dy) * stride
                           + 2 + (x << shift) + dx) * 4 + channel] / (shift ? 4.0 : 1.0);
            luma = 0.2126 * channels[0] + 0.7152 * channels[1] + 0.0722 * channels[2];
            expected = component == 0 ? luma : component == 1
               ? 128.0 / 255.0 + (channels[2] - luma) / 1.8556
               : 128.0 / 255.0 + (channels[0] - luma) / 1.5748;
            difference = fabs(image->planes[component][y * image->stride + x] - metal_native_clip(expected));
            if (!(difference < 0.003))
            {
               fprintf(stderr, "Metal encoded source differs at %u,%u,%u: %.9f\n", x, y, component, difference);
               return 0;
            }
            if (difference > *maximum) *maximum = difference;
         }
   }
   return 1;
}

#define METAL_NATIVE_CHECK(expr) do { if (!(expr)) { fprintf(stderr, "Native Metal failed line %u: %s\n", \
   (unsigned)__LINE__, #expr); goto end; } } while (0)

static int metal_native_run(id<MTLDevice> device, unsigned width, unsigned height, unsigned count)
{
   static const uint8_t missing_sign[12] = { 1, 0, 3, 0, 128, 0, 0, 0, 1, 0, 0, 255 };
   record_pyrowave_metal_config_t config;
   record_pyrowave_metal_t *encoder = NULL;
   record_pyrowave_metal_t *decoder = NULL;
   rpyrowave_decoder_t *cpu = NULL;
   record_gpu_frame_t frame;
   record_pyrowave_packet_t packet;
   record_pyrowave_metal_image_t image;
   rpyrowave_image_t reference;
   id<MTLCommandQueue> queue = [device newCommandQueue];
   id<MTLTexture> source = nil;
   id<MTLBuffer> readback = nil;
   MTLTextureDescriptor *descriptor;
   MTLResourceOptions resource_options;
   MTLStorageMode storage = MTLStorageModeManaged;
   float *rgb = NULL;
   unsigned texture_width = width + 4;
   unsigned texture_height = height + 6;
   unsigned x;
   unsigned y;
   unsigned iteration;
   size_t pitch = ((size_t)width * 8 + 255) & ~(size_t)255;
   uint64_t ticket = 0;
   double maximum = 0.0;
   double source_maximum = 0.0;
   int success = 0;
   memset(&config, 0, sizeof(config));
   if (@available(macOS 10.15, *))
      if (device.hasUnifiedMemory) storage = MTLStorageModeShared;
   resource_options = storage == MTLStorageModeManaged ? MTLResourceStorageModeManaged : MTLResourceStorageModeShared;
   rgb = (float*)malloc((size_t)texture_width * texture_height * 4 * sizeof(float));
   cpu = rpyrowave_decoder_new(width, height, 0, 16 * 1024 * 1024);
   METAL_NATIVE_CHECK(queue && rgb && cpu);
   for (y = 0; y < texture_height; y++)
      for (x = 0; x < texture_width; x++)
      {
         size_t offset = ((size_t)y * texture_width + x) * 4;
         rgb[offset] = (float)x / (texture_width - 1);
         rgb[offset + 1] = (float)y / (texture_height - 1);
         rgb[offset + 2] = (rgb[offset] + rgb[offset + 1]) * 0.5f;
         rgb[offset + 3] = 0.25f;
      }
   descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
      width:texture_width height:texture_height mipmapped:NO];
   descriptor.storageMode = storage;
   descriptor.usage = MTLTextureUsageShaderRead;
   source = [device newTextureWithDescriptor:descriptor];
   readback = [device newBufferWithLength:pitch * height options:resource_options];
   METAL_NATIVE_CHECK(source && readback);
   [source replaceRegion:MTLRegionMake2D(0, 0, texture_width, texture_height)
      mipmapLevel:0 withBytes:rgb bytesPerRow:texture_width * 4 * sizeof(float)];
   config.device = (__bridge void*)device;
   config.queue = (__bridge void*)queue;
   config.shaders[0] = rpyrowave_shader_color_metal;
   config.shaders[1] = rpyrowave_shader_dwt_metal;
   config.shaders[2] = rpyrowave_shader_packet_metal;
   config.sequence.width = width;
   config.sequence.height = height;
   config.maximum_bytes = 262144;
   config.slots = 3;
   config.bit_depth = 8;
   config.texture_width = texture_width;
   config.texture_height = texture_height;
   config.x = 2;
   config.y = 3;
   encoder = record_pyrowave_metal_new(&config);
   config.decode = true;
   decoder = record_pyrowave_metal_new(&config);
   METAL_NATIVE_CHECK(encoder && decoder);
   for (iteration = 0; iteration < count; iteration++)
   {
      id<MTLCommandBuffer> commands = [queue commandBuffer];
      id<MTLCommandBuffer> decode_commands;
      id<MTLCommandBuffer> display_commands;
      id<MTLBlitCommandEncoder> blit;
      id<MTLTexture> texture;
      void *input;
      size_t capacity;
      unsigned slot;
      METAL_NATIVE_CHECK(record_pyrowave_metal_push(encoder, (__bridge void*)commands, (__bridge void*)source,
               (uint64_t)iteration * 16667, 16667, &ticket) == RECORD_GPU_QUEUE_OK);
      METAL_NATIVE_CHECK(record_pyrowave_metal_take(encoder, &frame) == RECORD_GPU_QUEUE_OK);
      METAL_NATIVE_CHECK(record_pyrowave_metal_packet(encoder, frame.slot, &packet) == RECORD_GPU_QUEUE_BUSY);
      [commands commit];
      [commands waitUntilCompleted]; /* Offline oracle only. */
      METAL_NATIVE_CHECK(commands.status == MTLCommandBufferStatusCompleted && record_pyrowave_metal_poll(encoder));
      METAL_NATIVE_CHECK(record_pyrowave_metal_packet(encoder, frame.slot, &packet) == RECORD_GPU_QUEUE_OK);
      METAL_NATIVE_CHECK(packet.bytes <= config.maximum_bytes
            && !rpyrowave_decode_frame(cpu, packet.data, packet.bytes, 0, &reference));
      METAL_NATIVE_CHECK(reference.sequence.sequence == (ticket & 7) && frame.ready_value == ticket
            && record_pyrowave_metal_source_retired(encoder) == ticket);
      METAL_NATIVE_CHECK(metal_native_encoder_compare(&reference, rgb, texture_width, &source_maximum));
      METAL_NATIVE_CHECK(record_pyrowave_metal_receive_acquire(decoder, &slot, &input, &capacity) == RECORD_GPU_QUEUE_OK);
      METAL_NATIVE_CHECK(capacity >= packet.bytes);
      memcpy(input, packet.data, packet.bytes);
      METAL_NATIVE_CHECK(record_pyrowave_metal_receive_submit(decoder, packet.bytes,
               frame.timestamp, frame.duration) == RECORD_GPU_QUEUE_OK);
      METAL_NATIVE_CHECK(record_pyrowave_metal_release(encoder, frame.slot));
      decode_commands = [queue commandBuffer];
      METAL_NATIVE_CHECK(record_pyrowave_metal_decode(decoder, (__bridge void*)decode_commands) == RECORD_GPU_QUEUE_OK);
      METAL_NATIVE_CHECK(record_pyrowave_metal_image(decoder, &image) == RECORD_GPU_QUEUE_BUSY);
      [decode_commands commit];
      [decode_commands waitUntilCompleted];
      METAL_NATIVE_CHECK(decode_commands.status == MTLCommandBufferStatusCompleted && record_pyrowave_metal_poll(decoder));
      METAL_NATIVE_CHECK(record_pyrowave_metal_image(decoder, &image) == RECORD_GPU_QUEUE_OK);
      METAL_NATIVE_CHECK(image.frame.timestamp == frame.timestamp && image.frame.duration == frame.duration);
      texture = (__bridge id<MTLTexture>)image.texture;
      display_commands = [queue commandBuffer];
      blit = [display_commands blitCommandEncoder];
      [blit copyFromTexture:texture sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
         sourceSize:MTLSizeMake(width, height, 1) toBuffer:readback destinationOffset:0
         destinationBytesPerRow:pitch destinationBytesPerImage:pitch * height];
      if (storage == MTLStorageModeManaged) [blit synchronizeResource:readback];
      [blit endEncoding];
      METAL_NATIVE_CHECK(record_pyrowave_metal_present(decoder, image.frame.slot, (__bridge void*)display_commands));
      METAL_NATIVE_CHECK(!record_pyrowave_metal_free(decoder));
      [display_commands commit];
      [display_commands waitUntilCompleted];
      METAL_NATIVE_CHECK(display_commands.status == MTLCommandBufferStatusCompleted && record_pyrowave_metal_poll(decoder));
      METAL_NATIVE_CHECK(metal_native_compare(&reference, readback.contents, pitch, &maximum));
   }
   {
      uint8_t *input;
      size_t capacity;
      unsigned slot;
      unsigned i;
      uint32_t first = 0x80000000U | ((height - 1) << 14) | (width - 1);
      id<MTLCommandBuffer> commands = [queue commandBuffer];
      void *span;
      METAL_NATIVE_CHECK(record_pyrowave_metal_receive_acquire(decoder, &slot, &span, &capacity) == RECORD_GPU_QUEUE_OK);
      input = (uint8_t*)span;
      for (i = 0; i < 4; i++) input[i] = (uint8_t)(first >> (i * 8));
      input[4] = 1; input[5] = input[6] = input[7] = 0;
      memcpy(input + 8, missing_sign, sizeof(missing_sign));
      METAL_NATIVE_CHECK(record_pyrowave_metal_receive_submit(decoder, 20, 0, 0) == RECORD_GPU_QUEUE_OK);
      METAL_NATIVE_CHECK(record_pyrowave_metal_decode(decoder, (__bridge void*)commands) == RECORD_GPU_QUEUE_OK);
      [commands commit];
      [commands waitUntilCompleted];
      METAL_NATIVE_CHECK(commands.status == MTLCommandBufferStatusCompleted && !record_pyrowave_metal_poll(decoder));
      METAL_NATIVE_CHECK(record_pyrowave_metal_image(decoder, &image) == RECORD_GPU_QUEUE_BUSY);
   }
   METAL_NATIVE_CHECK(record_pyrowave_metal_finish(encoder) && record_pyrowave_metal_finish(decoder));
   METAL_NATIVE_CHECK(record_pyrowave_metal_free(encoder));
   encoder = NULL;
   METAL_NATIVE_CHECK(record_pyrowave_metal_free(decoder));
   decoder = NULL;
   printf("Native Metal %ux%u: %u GPU encodes/decodes, malformed-body rejection, source %.9f, RGB %.9f PASS\n",
         width, height, count, source_maximum, maximum);
   success = 1;
end:
   if (encoder) record_pyrowave_metal_free(encoder);
   if (decoder) record_pyrowave_metal_free(decoder);
   rpyrowave_decoder_free(cpu);
   free(rgb);
   return success;
}

int main(void)
{
   @autoreleasepool
   {
      id<MTLDevice> device = MTLCreateSystemDefaultDevice();
      if (!device)
      {
         fprintf(stderr, "Native Metal PyroWave: no Metal device; GPU execution NOT VERIFIED\n");
         return 77;
      }
      printf("Native Metal device: %s\n", device.name.UTF8String);
      if (!metal_native_run(device, 130, 98, 12) || !metal_native_run(device, 1920, 1080, 2)) return EXIT_FAILURE;
   }
   return EXIT_SUCCESS;
}
