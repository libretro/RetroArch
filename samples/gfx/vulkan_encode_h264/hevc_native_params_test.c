/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "hevc_native_context.h"
int main(int argc, char **argv)
{
   struct native_encoder *c = (struct native_encoder*)calloc(1, sizeof(*c));
   vulkan_encode_hevc_session_config_t config;
   vulkan_encode_hevc_session_t *session;
   unsigned char *headers = (unsigned char*)malloc(65536);
   size_t size = 0;
   FILE *file;
   unsigned ctb;
   int result = 77;
   assert(c && headers && argc == 3);
   if (!startup(c))
   {
      puts("UNAVAILABLE native HEVC encode device or validation layer");
      goto done;
   }
   memset(&config, 0, sizeof(config));
   config.physical = c->physical;
   config.device = c->device;
   config.family = c->family;
   config.video.width = 192;
   config.video.height = 128;
   config.video.fps_num = 60000;
   config.video.fps_den = 1001;
   config.video.bitrate = 2000000;
   config.video.bit_depth = (unsigned)atoi(argv[1]);
   config.video.color = config.video.bit_depth == 10 ? VULKAN_ENCODE_HEVC_PQ : VULKAN_ENCODE_HEVC_BT709;
   session = NULL;
   for (ctb = 64; ctb >= 16 && !session; ctb /= 2)
   {
      config.video.ctb_size = ctb;
      session = vulkan_encode_hevc_session_new(&config, &c->ops);
   }
   assert(session);
   assert(vulkan_encode_hevc_session_headers(session, headers, 65536, &size));
   file = fopen(argv[2], "wb");
   assert(file && fwrite(headers, 1, size, file) == size);
   assert(fclose(file) == 0);
   printf("device=%s depth=%u ctb=%u headers=%lu validation_errors=%u\n",
         c->properties.deviceName, config.video.bit_depth, config.video.ctb_size,
         (unsigned long)size, c->validation_errors);
   vulkan_encode_hevc_session_free(session);
   result = c->validation_errors ? 1 : 0;
done:
   if (c->device)
      c->destroy_device(c->device, NULL);
   if (c->messenger)
      c->destroy_messenger(c->instance, c->messenger, NULL);
   if (c->instance)
      c->destroy_instance(c->instance, NULL);
   if (c->library)
      dylib_close(c->library);
   free(headers);
   free(c);
   return result;
}
