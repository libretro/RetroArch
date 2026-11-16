/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define HEVC_NATIVE_HELPERS_ONLY
#include "hevc_native_encode_test.c"
#include "../../../gfx/common/vulkan_encode_hevc.h"

struct native_hevc_adapter
{
   struct native_hevc_encoder gpu;
   vulkan_encode_hevc_ops_t ops;
   vulkan_encode_hevc_open_t config;
};

static vulkan_encode_hevc_t *native_adapter_new_mode(struct native_hevc_adapter *a,
      unsigned depth, bool compute, bool storage, bool full_range)
{
   struct native_hevc_encoder *e = &a->gpu;
   struct native_encoder *c = &e->base;
   vulkan_encode_hevc_t *encoder = NULL;
   PFN_vkGetDeviceQueue get_queue;
   unsigned ctb;
   if (!startup_mode(c, compute) || (!compute && !(c->queue_flags & VK_QUEUE_TRANSFER_BIT)))
      return NULL;
   load_frame_ops(e);
   get_queue = (PFN_vkGetDeviceQueue)c->device_proc(c->device, "vkGetDeviceQueue");
   assert(get_queue);
   get_queue(c->device, c->family, 0, &e->queue);
   a->ops.session = c->ops;
   a->ops.pool = e->pool_ops;
   a->ops.submit = e->submit_ops;
   a->config.session.physical = c->physical;
   a->config.session.device = c->device;
   a->config.session.family = c->family;
   a->config.session.video.width = 192;
   a->config.session.video.height = 128;
   a->config.session.video.fps_num = 30;
   a->config.session.video.fps_den = 1;
   a->config.session.video.bitrate = 2000000;
   a->config.session.video.bit_depth = depth;
   a->config.session.video.color = !compute && a->config.session.video.bit_depth == 10
      ? VULKAN_ENCODE_HEVC_PQ : VULKAN_ENCODE_HEVC_BT709;
   a->config.queue = e->queue;
   a->config.packet_size = 1048576;
   a->config.slots = 2;
   a->config.producer_family = c->producer_family;
   a->config.storage_image_extended_formats = storage && c->enabled_features.shaderStorageImageExtendedFormats;
   a->config.session.video.full_range = full_range;
   a->config.gop = 8;
   a->config.qp = 22;
   for (ctb = 64; ctb >= 16 && !encoder; ctb /= 2)
   {
      a->config.session.video.ctb_size = ctb;
      encoder = vulkan_encode_hevc_open(&a->config, &a->ops);
   }
   assert(encoder);
   if (!compute)
      upload_new(e, vulkan_encode_hevc_pool(encoder));
   return encoder;
}

static void native_adapter_context_free(struct native_hevc_adapter *a)
{
   struct native_hevc_encoder *e = &a->gpu;
   struct native_encoder *c = &e->base;
   if (e->producer_commands)
      e->pool_ops.destroy_commands(c->device, e->producer_commands, NULL);
   if (e->upload_memory)
   {
      e->pool_ops.unmap(c->device, e->upload_memory);
      e->pool_ops.destroy_buffer(c->device, e->upload, NULL);
      e->pool_ops.free_memory(c->device, e->upload_memory, NULL);
   }
   if (c->device)
      c->destroy_device(c->device, NULL);
   if (c->messenger)
      c->destroy_messenger(c->instance, c->messenger, NULL);
   if (c->instance)
      c->destroy_instance(c->instance, NULL);
   if (c->library)
      dylib_close(c->library);
   free(a);
}

#define native_adapter_new(a, depth) native_adapter_new_mode((a), (depth), false, false, false)
#ifndef HEVC_NATIVE_ADAPTER_HELPERS_ONLY
int main(int argc, char **argv)
{
   struct native_hevc_adapter *a = (struct native_hevc_adapter*)calloc(1, sizeof(*a));
   struct native_hevc_encoder *e;
   struct native_encoder *c;
   vulkan_encode_hevc_t *encoder = NULL;
   const vulkan_encode_pool_t *pool;
   vulkan_encode_packet_t packet, again;
   PFN_vkWaitForFences wait;
   unsigned char *headers = (unsigned char*)malloc(65536);
   unsigned first, frame;
   size_t size;
   FILE *file;
   bool cancel;
   int result = 77;
   assert(a && headers && (argc == 3 || argc == 4));
   cancel = argc == 4;
   e = &a->gpu;
   c = &e->base;
   encoder = native_adapter_new(a, (unsigned)atoi(argv[1]));
   if (!encoder)
   {
      puts("UNAVAILABLE native HEVC encode device or validation layer");
      goto done;
   }
   pool = vulkan_encode_hevc_pool(encoder);
   assert(pool && pool->count == 2);
   wait = (PFN_vkWaitForFences)c->device_proc(c->device, "vkWaitForFences");
   assert(wait);
   file = fopen(argv[2], "wb");
   assert(file && vulkan_encode_hevc_headers(encoder, headers, 65536, &size));
   assert(fwrite(headers, 1, size, file) == size);
   for (first = 0; first < 24; first += 2)
   {
      for (frame = first; frame < first + 2; frame++)
      {
         upload_frame(e, pool, frame);
         assert(vulkan_encode_hevc_submit(encoder, frame % 2, frame, 1,
                  frame + 1, frame == 7) == VULKAN_ENCODE_OK);
      }
      assert(vulkan_encode_hevc_submit(encoder, 0, first + 2, 1,
               first + 3, false) == VULKAN_ENCODE_BUSY);
      for (frame = first; frame < first + 2; frame++)
      {
         /* Native fixture only; the adapter itself never waits. */
         assert(wait(c->device, 1, &pool->slot[frame % 2].fence,
                  VK_TRUE, 2000000000) == VK_SUCCESS);
         assert(vulkan_encode_hevc_poll(encoder, &packet) == VULKAN_ENCODE_OK);
         assert(packet.slot == frame % 2 && packet.timestamp == frame && packet.duration == 1);
         assert(vulkan_encode_hevc_poll(encoder, &again) == VULKAN_ENCODE_OK);
         assert(packet.data == again.data && packet.size == again.size && packet.size);
         assert(!vulkan_encode_hevc_free(encoder));
         assert(fwrite(packet.data, 1, packet.size, file) == packet.size);
         assert(vulkan_encode_hevc_release(encoder));
      }
   }
   if (cancel)
   {
      unsigned char first_byte;
      for (frame = 24; frame < 26; frame++)
      {
         upload_frame(e, pool, frame);
         assert(vulkan_encode_hevc_submit(encoder, frame % 2, frame, 1,
                  frame + 1, false) == VULKAN_ENCODE_OK);
      }
      assert(wait(c->device, 1, &pool->slot[0].fence, VK_TRUE, 2000000000) == VK_SUCCESS);
      assert(vulkan_encode_hevc_poll(encoder, &packet) == VULKAN_ENCODE_OK);
      first_byte = *(const unsigned char*)packet.data;
      assert(!vulkan_encode_hevc_drain(encoder) && !vulkan_encode_hevc_free(encoder));
      assert(*(const unsigned char*)packet.data == first_byte);
      assert(vulkan_encode_hevc_release(encoder));
      assert(wait(c->device, 1, &pool->slot[1].fence, VK_TRUE, 2000000000) == VK_SUCCESS);
      assert(vulkan_encode_hevc_drain(encoder));
   }
   else
   {
      assert(vulkan_encode_hevc_finish(encoder) == VULKAN_ENCODE_OK);
      assert(vulkan_encode_hevc_poll(encoder, &packet) == VULKAN_ENCODE_END);
   }
   assert(vulkan_encode_hevc_free(encoder));
   assert(fclose(file) == 0);
   printf("device=%s depth=%u pictures=24 in_flight=2 cancel=%u validation_errors=%u\n",
         c->properties.deviceName, a->config.session.video.bit_depth,
         cancel ? 1u : 0u, c->validation_errors);
   result = c->validation_errors ? 1 : 0;
done:
   free(headers);
   native_adapter_context_free(a);
   return result;
}
#endif
