/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../record/record_gpu_dispatch_vulkan_hevc.c"
static record_gpu_dispatch_vulkan_config_t copied;
static record_gpu_dispatch_vulkan_start_t worker_start;
static unsigned phase, queries, opened, expected_depth, expected_ctb;
static VkVideoEncodeH265CtbSizeFlagsKHR supported;
static bool query_fail;
record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_backend(
      const record_gpu_dispatch_vulkan_config_t *config,
      record_gpu_dispatch_vulkan_start_t start,
      const record_gpu_output_vulkan_config_t *output, const char *path,
      const vulkan_encode_capture_ops_t *capture)
{
   assert(phase == 1 && output && path && !capture);
   copied = *config;
   worker_start = start;
   return (record_gpu_dispatch_vulkan_t*)(uintptr_t)1;
}
record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_backend_stream(
      const record_gpu_dispatch_vulkan_config_t *config,
      record_gpu_dispatch_vulkan_start_t start,
      const record_gpu_output_vulkan_config_t *output, const char *path,
      const vulkan_encode_capture_ops_t *ops, bool legacy)
{
   (void)legacy;
   return record_gpu_dispatch_vulkan_new_backend(config, start, output, path, ops);
}

static VKAPI_ATTR VkResult VKAPI_CALL capabilities(VkPhysicalDevice physical,
      const VkVideoProfileInfoKHR *profile, VkVideoCapabilitiesKHR *out)
{
   const VkVideoEncodeH265ProfileInfoKHR *codec = (const VkVideoEncodeH265ProfileInfoKHR*)profile->pNext;
   VkVideoEncodeCapabilitiesKHR *encode = (VkVideoEncodeCapabilitiesKHR*)out->pNext;
   VkVideoEncodeH265CapabilitiesKHR *hevc = (VkVideoEncodeH265CapabilitiesKHR*)encode->pNext;
   assert(phase == 2 && physical == (VkPhysicalDevice)(uintptr_t)2);
   assert(profile->videoCodecOperation == VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR);
   assert(profile->lumaBitDepth == profile->chromaBitDepth);
   assert(profile->lumaBitDepth == (expected_depth == 10
            ? VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR : VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR));
   assert(codec->stdProfileIdc == (expected_depth == 10
            ? STD_VIDEO_H265_PROFILE_IDC_MAIN_10 : STD_VIDEO_H265_PROFILE_IDC_MAIN));
   hevc->ctbSizes = supported;
   queries++;
   return query_fail ? VK_ERROR_FORMAT_NOT_SUPPORTED : VK_SUCCESS;
}
record_gpu_session_vulkan_t *record_gpu_session_vulkan_new_hevc(
      const vulkan_encode_hevc_open_t *config, const vulkan_encode_hevc_ops_t *ops,
      const record_gpu_output_vulkan_config_t *output, const char *path)
{
   assert(phase == 2 && ops && output && path);
   assert(config->session.video.bit_depth == expected_depth);
   assert(config->session.video.ctb_size == expected_ctb);
   opened++;
   return (record_gpu_session_vulkan_t*)(uintptr_t)3;
}
int main(void)
{
   vulkan_encode_hevc_open_t config;
   vulkan_encode_hevc_ops_t ops;
   record_gpu_output_vulkan_config_t output;
   unsigned mode;
   memset(&config, 0, sizeof(config));
   memset(&ops, 0, sizeof(ops));
   memset(&output, 0, sizeof(output));
   ops.session.capabilities = capabilities;
   config.session.physical = (VkPhysicalDevice)(uintptr_t)2;
   for (expected_depth = 8; expected_depth <= 10; expected_depth += 2)
      for (mode = 0; mode < 5; mode++)
      {
         queries = opened = 0;
         query_fail = mode == 4;
         supported = mode == 0 ? VK_VIDEO_ENCODE_H265_CTB_SIZE_16_BIT_KHR
            : mode == 1 ? VK_VIDEO_ENCODE_H265_CTB_SIZE_32_BIT_KHR
            : mode == 2 ? VK_VIDEO_ENCODE_H265_CTB_SIZE_64_BIT_KHR : 0;
         expected_ctb = mode == 0 ? 16 : mode == 1 ? 32 : 64;
         phase = 1;
         config.session.video.bit_depth = expected_depth;
         assert(record_gpu_dispatch_vulkan_new_hevc(&config, &ops, &output, "unused", NULL));
         assert(!queries && !opened);
         config.session.video.bit_depth = 0;
         phase = 2;
         assert((worker_start(&copied, &output, "unused") != NULL) == (mode < 3));
         assert(queries == 1 && opened == (mode < 3 ? 1u : 0u));
      }
   assert(!record_gpu_dispatch_vulkan_new_hevc(NULL, &ops, &output, "unused", NULL));
   assert(!record_gpu_dispatch_vulkan_new_hevc(&config, NULL, &output, "unused", NULL));
   puts("HEVC dispatcher factory: immutable snapshots, worker CTB selection and capability refusal PASS");
   return 0;
}
