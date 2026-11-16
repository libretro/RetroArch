/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define HEVC_NATIVE_CAPTURE_HELPERS_ONLY
#include "hevc_native_capture_test.c"
int main(int argc, char **argv)
{
   struct native_hevc_capture *n = (struct native_hevc_capture*)calloc(1, sizeof(*n));
   struct native_encoder *c;
   vulkan_encode_hevc_t *encoder;
   const vulkan_encode_pool_t *pool;
   vulkan_encode_packet_t packet;
   PFN_vkWaitForFences wait;
   unsigned char *headers = (unsigned char*)malloc(65536);
   unsigned frame, depth;
   size_t size;
   FILE *file;
   bool direct, full = false;
   unsigned mode;
   int result = 77;
   assert(n && headers && argc >= 5 && argc <= 7);
   for (frame = 5; frame < (unsigned)argc; frame++)
      if (!strcmp(argv[frame], "--pq-as-linear"))
         n->pq_signal = true;
      else
      {
         assert(!strcmp(argv[frame], "--full-range"));
         full = true;
      }
   c = &n->adapter.gpu.base;
   (void)upload_frame;
   depth = (unsigned)atoi(argv[1]);
   direct = atoi(argv[3]) != 0;
   mode = (unsigned)atoi(argv[4]);
   assert(depth == 10 && (mode == 1 || mode == 2));
   n->output_mode = mode;
   n->rgb_format = mode == 1 ? VK_FORMAT_A2B10G10R10_UNORM_PACK32 : VK_FORMAT_R16G16B16A16_SFLOAT;
   encoder = native_adapter_new_mode(&n->adapter, depth, true, direct, full);
   if (!encoder)
   {
      puts("UNAVAILABLE HEVC encode and compute capture device");
      goto done;
   }
   assert(vulkan_encode_hevc_free(encoder));
   n->adapter.config.session.video.color = VULKAN_ENCODE_HEVC_PQ;
   encoder = vulkan_encode_hevc_open(&n->adapter.config, &n->adapter.ops);
   assert(encoder);
   pool = vulkan_encode_hevc_pool(encoder);
   if (direct && !pool->storage_images)
   {
      assert(vulkan_encode_hevc_free(encoder));
      puts("UNAVAILABLE direct capture storage planes");
      goto done;
   }
   assert(pool->storage_images == direct);
   load_capture(n);
   capture_new(n, pool);
   wait = (PFN_vkWaitForFences)c->device_proc(c->device, "vkWaitForFences");
   assert(wait);
   file = fopen(argv[2], "wb");
   assert(file && vulkan_encode_hevc_headers(encoder, headers, 65536, &size));
   assert(fwrite(headers, 1, size, file) == size);
   for (frame = 0; frame < 24; frame++)
   {
      capture_frame(n, pool, frame);
      assert(vulkan_encode_hevc_submit(encoder, frame % 2, frame, 1,
               frame + 1, frame == 7) == VULKAN_ENCODE_OK);
      /* Fixture completion only; capture and encoding never wait. */
      assert(wait(c->device, 1, &pool->slot[frame % 2].fence,
               VK_TRUE, 2000000000) == VK_SUCCESS);
      assert(vulkan_encode_hevc_poll(encoder, &packet) == VULKAN_ENCODE_OK);
      assert(fwrite(packet.data, 1, packet.size, file) == packet.size);
      assert(vulkan_encode_hevc_release(encoder));
   }
   assert(vulkan_encode_hevc_finish(encoder) == VULKAN_ENCODE_OK);
   assert(vulkan_encode_hevc_poll(encoder, &packet) == VULKAN_ENCODE_END);
   capture_free(n);
   assert(vulkan_encode_hevc_free(encoder) && fclose(file) == 0);
   printf("device=%s depth=%u direct=%u hdr_mode=%u pictures=24 sync_validation=1 validation_errors=%u\n",
         c->properties.deviceName, depth, direct ? 1u : 0u, mode, c->validation_errors);
   result = c->validation_errors ? 1 : 0;
done:
   free(headers);
   native_adapter_context_free(&n->adapter);
   return result;
}
