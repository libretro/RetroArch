/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_gpu_dispatch_vulkan_hevc.h"
#include "record_gpu_dispatch_vulkan_backend.h"
#include <string.h>
static record_gpu_session_vulkan_t *record_vkd_hevc_start(
      const record_gpu_dispatch_vulkan_config_t *config,
      const record_gpu_output_vulkan_config_t *output, const char *path)
{
   vulkan_encode_hevc_open_t encoder = config->hevc.encoder;
   if (!encoder.session.video.ctb_size)
   {
      VkVideoProfileInfoKHR profile;
      VkVideoEncodeH265ProfileInfoKHR codec;
      VkVideoCapabilitiesKHR capabilities;
      VkVideoEncodeCapabilitiesKHR encode;
      VkVideoEncodeH265CapabilitiesKHR hevc;
      if (!config->hevc.ops.session.capabilities)
         return NULL;
      memset(&profile, 0, sizeof(profile));
      memset(&codec, 0, sizeof(codec));
      memset(&capabilities, 0, sizeof(capabilities));
      memset(&encode, 0, sizeof(encode));
      memset(&hevc, 0, sizeof(hevc));
      profile.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR;
      profile.pNext = &codec;
      profile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR;
      profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
      profile.lumaBitDepth = profile.chromaBitDepth = encoder.session.video.bit_depth == 10
         ? VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR : VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
      codec.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_PROFILE_INFO_KHR;
      codec.stdProfileIdc = encoder.session.video.bit_depth == 10
         ? STD_VIDEO_H265_PROFILE_IDC_MAIN_10 : STD_VIDEO_H265_PROFILE_IDC_MAIN;
      capabilities.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
      capabilities.pNext = &encode;
      encode.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_CAPABILITIES_KHR;
      encode.pNext = &hevc;
      hevc.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H265_CAPABILITIES_KHR;
      if (config->hevc.ops.session.capabilities(encoder.session.physical,
               &profile, &capabilities) != VK_SUCCESS)
         return NULL;
      encoder.session.video.ctb_size = (hevc.ctbSizes & VK_VIDEO_ENCODE_H265_CTB_SIZE_64_BIT_KHR) ? 64
         : (hevc.ctbSizes & VK_VIDEO_ENCODE_H265_CTB_SIZE_32_BIT_KHR) ? 32
         : (hevc.ctbSizes & VK_VIDEO_ENCODE_H265_CTB_SIZE_16_BIT_KHR) ? 16 : 0;
      if (!encoder.session.video.ctb_size)
         return NULL;
   }
   return record_gpu_session_vulkan_new_hevc(&encoder, &config->hevc.ops, output, path);
}
record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_hevc(
      const vulkan_encode_hevc_open_t *encoder,
      const vulkan_encode_hevc_ops_t *ops,
      const record_gpu_output_vulkan_config_t *output, const char *path,
      const vulkan_encode_capture_ops_t *capture_ops)
{
   record_gpu_dispatch_vulkan_config_t config;
   if (!encoder || !ops)
      return NULL;
   config.hevc.encoder = *encoder;
   config.hevc.ops = *ops;
   return record_gpu_dispatch_vulkan_new_backend(&config, record_vkd_hevc_start,
         output, path, capture_ops);
}

record_gpu_dispatch_vulkan_t *record_gpu_dispatch_vulkan_new_hevc_stream(
      const vulkan_encode_hevc_open_t *encoder,
      const vulkan_encode_hevc_ops_t *ops,
      const record_gpu_output_vulkan_config_t *output, const char *url,
      const vulkan_encode_capture_ops_t *capture_ops, bool legacy)
{
   record_gpu_dispatch_vulkan_config_t config;
   if (!encoder || !ops || !output || output->video.codec != RECORD_MP4_HEVC)
      return NULL;
   config.hevc.encoder = *encoder;
   config.hevc.ops = *ops;
   return record_gpu_dispatch_vulkan_new_backend_stream(&config, record_vkd_hevc_start,
         output, url, capture_ops, legacy);
}
