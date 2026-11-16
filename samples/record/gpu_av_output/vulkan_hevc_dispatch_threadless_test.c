/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#undef HAVE_THREADS
#include "../../../record/record_gpu_dispatch_vulkan.c"
#include "../../../record/record_gpu_dispatch_vulkan_hevc.c"
record_gpu_session_vulkan_t *record_gpu_session_vulkan_new_hevc(
      const vulkan_encode_hevc_open_t *c, const vulkan_encode_hevc_ops_t *ops,
      const record_gpu_output_vulkan_config_t *o, const char *path)
{
   (void)c; (void)ops; (void)o; (void)path;
   assert(!"threadless admission must not start a session");
   return NULL;
}
int main(void)
{
   vulkan_encode_hevc_open_t config;
   vulkan_encode_hevc_ops_t ops;
   record_gpu_output_vulkan_config_t output;
   memset(&config, 0, sizeof(config));
   memset(&ops, 0, sizeof(ops));
   memset(&output, 0, sizeof(output));
   assert(!record_gpu_dispatch_vulkan_new_hevc(&config, &ops, &output, "unused", NULL));
   puts("HEVC dispatcher: threadless admission refused before startup PASS");
   return 0;
}
