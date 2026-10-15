/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef VULKAN_VIDEO_CAPS_H
#define VULKAN_VIDEO_CAPS_H
#include <string.h>
#include <boolean.h>
#include <retro_inline.h>
#include "../include/vulkan/vulkan.h"

#define VULKAN_VIDEO_BASE_EXTENSIONS 3
static const char *vulkan_video_device_extensions[] = {
   "VK_KHR_synchronization2",
   "VK_KHR_video_queue",
   "VK_KHR_video_decode_queue",
   "VK_KHR_video_decode_h264",
   "VK_KHR_video_decode_h265",
   "VK_KHR_video_decode_vp9",
   "VK_KHR_video_decode_av1"
};

static INLINE uint32_t vulkan_video_supported_codecs(
      const VkExtensionProperties *props, uint32_t count)
{
   uint32_t i, j, found = 0, codecs = 0;
   if (!props) return 0;
   for (i = 0; i < sizeof(vulkan_video_device_extensions) / sizeof(vulkan_video_device_extensions[0]); i++)
      for (j = 0; j < count; j++)
         if (!strcmp(props[j].extensionName, vulkan_video_device_extensions[i]))
         {
            if (i < VULKAN_VIDEO_BASE_EXTENSIONS) found++;
            else if (i == 3) codecs |= VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR;
            else if (i == 4) codecs |= VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
            else if (i == 5)
               codecs |= VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR;
            else
               codecs |= VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR;
            break;
         }
   return found == VULKAN_VIDEO_BASE_EXTENSIONS ? codecs : 0;
}

static INLINE bool vulkan_video_extension_enabled(unsigned index, uint32_t codecs)
{
   if (index < VULKAN_VIDEO_BASE_EXTENSIONS) return true;
   if (index == 3) return !!(codecs & VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR);
   if (index == 4) return !!(codecs & VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR);
   if (index == 5) return !!(codecs & VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR);
   if (index == 6)
      return !!(codecs & VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR);
   return false;
}

static INLINE int vulkan_video_select_decode_family(
      const VkQueueFamilyProperties2 *queues,
      const VkQueueFamilyVideoPropertiesKHR *video, uint32_t count,
      uint32_t graphics, uint32_t available, uint32_t *codecs)
{
   uint32_t i, best_score = 0;
   int best = -1;
   if (!codecs) return -1;
   *codecs = 0;
   if (!queues || !video) return -1;
   for (i = 0; i < count; i++)
   {
      uint32_t supported, score;
      if (i == graphics || !queues[i].queueFamilyProperties.queueCount
            || !(queues[i].queueFamilyProperties.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR))
         continue;
      supported = available & video[i].videoCodecOperations;
      /* Prefer shared coverage, retaining H.264 on split families. */
      score = (supported & VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR ? 8 : 0)
         + (supported & VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR ? 4 : 0)
         + (supported & VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR ? 2 : 0)
         + (supported & VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR ? 1 : 0);
      if (score > best_score)
      {
         best = (int)i; best_score = score; *codecs = supported;
      }
   }
   return best;
}
#endif
