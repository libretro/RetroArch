/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "../../../gfx/common/vulkan_h264_reference.h"
#include "../../../gfx/common/vulkan_video_caps.h"

static unsigned failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "line %u: %s\n", \
      (unsigned)__LINE__, #c); failures++; } } while (0)
static VkExtensionProperties props[7];
static VkQueueFamilyProperties2 queues[4];
static VkQueueFamilyVideoPropertiesKHR video[4];
#define H264 VK_VIDEO_CODEC_OPERATION_DECODE_H264_BIT_KHR
#define H265 VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR
#define VP9 VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR
#define AV1 VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR

static void test_extensions(void)
{
   unsigned i;
   for (i = 0; i < 7; i++)
      strcpy(props[i].extensionName, vulkan_video_device_extensions[i]);
   CHECK(vulkan_video_supported_codecs(props, 7) == (H264 | H265 | VP9 | AV1));
   CHECK(vulkan_video_supported_codecs(props, 6) == (H264 | H265 | VP9));
   CHECK(vulkan_video_supported_codecs(props, 5) == (H264 | H265));
   CHECK(vulkan_video_supported_codecs(props, 4) == H264);
   strcpy(props[3].extensionName, "VK_KHR_video_decode_h265");
   CHECK(vulkan_video_supported_codecs(props, 4) == H265);
   CHECK(!vulkan_video_supported_codecs(props, 3));
   props[0].extensionName[0] = 0;
   CHECK(!vulkan_video_supported_codecs(props, 5));
   CHECK(!vulkan_video_supported_codecs(NULL, 5));
   for (i = 0; i < 3; i++) CHECK(vulkan_video_extension_enabled(i, H265));
   CHECK(!vulkan_video_extension_enabled(3, H265));
   CHECK(vulkan_video_extension_enabled(4, H265));
   CHECK(vulkan_video_extension_enabled(3, H264));
   CHECK(!vulkan_video_extension_enabled(4, H264));
   CHECK(!vulkan_video_extension_enabled(5, H264 | H265));
   CHECK(vulkan_video_extension_enabled(5, VP9));
   CHECK(!vulkan_video_extension_enabled(6, H264 | H265 | VP9));
   CHECK(vulkan_video_extension_enabled(6, AV1));
   CHECK(!vulkan_video_extension_enabled(7, AV1));
   strcpy(props[0].extensionName, vulkan_video_device_extensions[0]);
   strcpy(props[3].extensionName, vulkan_video_device_extensions[5]);
   CHECK(vulkan_video_supported_codecs(props, 4) == VP9);
   strcpy(props[3].extensionName, vulkan_video_device_extensions[6]);
   CHECK(vulkan_video_supported_codecs(props, 4) == AV1);
}

static void test_families(void)
{
   unsigned i;
   uint32_t codecs = 123;
   for (i = 0; i < 4; i++)
   {
      queues[i].queueFamilyProperties.queueFlags = VK_QUEUE_VIDEO_DECODE_BIT_KHR;
      queues[i].queueFamilyProperties.queueCount = 1;
   }
   video[0].videoCodecOperations = H264 | H265;
   video[1].videoCodecOperations = H265;
   video[2].videoCodecOperations = H264;
   CHECK(vulkan_video_select_decode_family(queues, video, 4, 0, H264 | H265, &codecs) == 2);
   CHECK(codecs == H264);
   CHECK(vulkan_video_select_decode_family(queues, video, 4, 0, H265, &codecs) == 1);
   CHECK(codecs == H265);
   video[3].videoCodecOperations = H264 | H265;
   CHECK(vulkan_video_select_decode_family(queues, video, 4, 0, H264 | H265, &codecs) == 3);
   CHECK(codecs == (H264 | H265));
   queues[3].queueFamilyProperties.queueCount = 0;
   CHECK(vulkan_video_select_decode_family(queues, video, 4, 0, H264 | H265, &codecs) == 2);
   queues[2].queueFamilyProperties.queueFlags = VK_QUEUE_COMPUTE_BIT;
   CHECK(vulkan_video_select_decode_family(queues, video, 4, 0, H264 | H265, &codecs) == 1);
   CHECK(codecs == H265);
   CHECK(vulkan_video_select_decode_family(queues, video, 4, 0, H264, &codecs) == -1);
   CHECK(!codecs);
   CHECK(vulkan_video_select_decode_family(queues, video, 4, 0, 0, &codecs) == -1);
   CHECK(!codecs);
   CHECK(vulkan_video_select_decode_family(NULL, video, 4, 0, H265, &codecs) == -1);
   CHECK(!codecs);
   CHECK(vulkan_video_select_decode_family(queues, video, 4, 0, H265, NULL) == -1);
   queues[2].queueFamilyProperties.queueFlags = VK_QUEUE_VIDEO_DECODE_BIT_KHR;
   queues[3].queueFamilyProperties.queueCount = 1;
   video[1].videoCodecOperations = VP9;
   video[2].videoCodecOperations = H264;
   video[3].videoCodecOperations = H265 | VP9;
   CHECK(vulkan_video_select_decode_family(queues, video, 4, 0, H264 | H265 | VP9, &codecs) == 2);
   CHECK(codecs == H264);
   video[2].videoCodecOperations |= VP9;
   CHECK(vulkan_video_select_decode_family(queues, video, 4, 0, H264 | H265 | VP9, &codecs) == 2);
   CHECK(codecs == (H264 | VP9));
   CHECK(vulkan_video_select_decode_family(queues, video, 4, 0, VP9, &codecs) == 1);
   CHECK(codecs == VP9);
   video[1].videoCodecOperations = AV1;
   video[2].videoCodecOperations = H264;
   video[3].videoCodecOperations = VP9 | AV1;
   CHECK(vulkan_video_select_decode_family(queues, video, 4, 0, AV1, &codecs) == 1);
   CHECK(codecs == AV1);
   CHECK(vulkan_video_select_decode_family(queues, video, 4, 0, H264 | VP9 | AV1, &codecs) == 2);
   CHECK(codecs == H264);
   video[2].videoCodecOperations |= AV1;
   CHECK(vulkan_video_select_decode_family(queues, video, 4, 0, H264 | VP9 | AV1, &codecs) == 2);
   CHECK(codecs == (H264 | AV1));

}

static void test_h264_reference_wrap(void)
{
   unsigned bits, current, reference;
   CHECK(vulkan_h264_reference_wrap_driver(0x10de, 0x2b85,
            (617u << 22) | (42u << 14)));
   CHECK(!vulkan_h264_reference_wrap_driver(0x1002, 0x2b85,
            (617u << 22) | (42u << 14)));
   CHECK(!vulkan_h264_reference_wrap_driver(0x10de, 0x2b84,
            (617u << 22) | (42u << 14)));
   CHECK(!vulkan_h264_reference_wrap_driver(0x10de, 0x2b85,
            (617u << 22) | (43u << 14)));
   for (bits = 4; bits <= 16; bits++)
      for (current = 0; current < 16; current++)
         for (reference = 0; reference < 16; reference++)
         {
            unsigned frame = (1u << bits) - 16 + reference;
            uint16_t expected = (uint16_t)(frame > current
                  ? (int)frame - (1 << bits) : (int)frame);
            CHECK(vulkan_h264_reference_frame_num(frame, current, bits, 2,
                     false, true) == expected);
            CHECK(vulkan_h264_reference_frame_num(frame, current, bits, 2,
                     true, true) == frame);
            CHECK(vulkan_h264_reference_frame_num(frame, current, bits, 2,
                     false, false) == frame);
            CHECK(vulkan_h264_reference_frame_num(frame, current, bits, 0,
                     false, true) == frame);
            CHECK(vulkan_h264_reference_frame_num(frame, current, bits, 1,
                     false, true) == frame);
         }
   CHECK(vulkan_h264_reference_frame_num(15, 0, 3, 2, false, true) == 15);
   CHECK(vulkan_h264_reference_frame_num(15, 0, 17, 2, false, true) == 15);
}

int main(void)
{
   test_extensions(); test_families(); test_h264_reference_wrap();
   printf("Vulkan video codec negotiation: %u failures\n", failures);
   return failures ? 1 : 0;
}
