/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/vulkan_encode_h264_session.h"
static unsigned calls, fail_call, allocations, fail_allocation;
static unsigned live_session, live_parameters, live_memory, memory_made, bound;
static unsigned malformed;
static void *test_calloc(size_t count, size_t size)
{
   return ++allocations == fail_allocation ? NULL : calloc(count, size);
}
#define calloc test_calloc
#include "../../../gfx/common/vulkan_encode_h264_session.c"
#undef calloc
#define HANDLE(type,n) ((type)(uintptr_t)(n))
static VkResult result(void)
{
   return ++calls == fail_call ? VK_ERROR_INITIALIZATION_FAILED : VK_SUCCESS;
}
static VKAPI_ATTR void VKAPI_CALL families(VkPhysicalDevice physical,
      uint32_t *count, VkQueueFamilyProperties2 *properties)
{
   unsigned i;
   assert(physical == HANDLE(VkPhysicalDevice, 1));
   if (!properties)
   {
      *count = malformed == 1 ? 65 : 2;
      return;
   }
   assert(*count == 2);
   for (i = 0; i < 2; i++)
   {
      VkQueueFamilyVideoPropertiesKHR *video =
            (VkQueueFamilyVideoPropertiesKHR*)properties[i].pNext;
      assert(properties[i].sType == VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2);
      assert(video->sType == VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR);
      memset(&properties[i].queueFamilyProperties, 0, sizeof(properties[i].queueFamilyProperties));
      properties[i].queueFamilyProperties.queueCount = 1;
      properties[i].queueFamilyProperties.queueFlags = i ?
            VK_QUEUE_VIDEO_ENCODE_BIT_KHR : VK_QUEUE_GRAPHICS_BIT;
      video->videoCodecOperations = malformed == 2 ? 0 : VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR;
   }
}
static VKAPI_ATTR void VKAPI_CALL memory_properties(VkPhysicalDevice physical,
      VkPhysicalDeviceMemoryProperties *properties)
{
   assert(physical == HANDLE(VkPhysicalDevice, 1));
   memset(properties, 0, sizeof(*properties));
   properties->memoryTypeCount = malformed == 18 ? 0 : 4;
   properties->memoryTypes[1].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
   properties->memoryTypes[2].propertyFlags = VK_MEMORY_PROPERTY_PROTECTED_BIT;
   properties->memoryTypes[3].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
}
static VKAPI_ATTR VkResult VKAPI_CALL capabilities(VkPhysicalDevice physical,
      const VkVideoProfileInfoKHR *profile, VkVideoCapabilitiesKHR *caps)
{
   VkVideoEncodeCapabilitiesKHR *encode = (VkVideoEncodeCapabilitiesKHR*)caps->pNext;
   VkVideoEncodeH264CapabilitiesKHR *codec = (VkVideoEncodeH264CapabilitiesKHR*)encode->pNext;
   VkResult status = result();
   assert(physical == HANDLE(VkPhysicalDevice, 1));
   assert(profile->videoCodecOperation == VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR);
   assert(profile->lumaBitDepth == VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR);
   if (status != VK_SUCCESS)
      return status;
   caps->minCodedExtent.width = malformed == 12 ? 208 : 160; caps->minCodedExtent.height = 64;
   caps->maxCodedExtent.width = malformed == 13 ? 160 : 4096; caps->maxCodedExtent.height = 4096;
   caps->pictureAccessGranularity.width = malformed == 3 ? 0 : malformed == 14 ? 128 : 16;
   caps->pictureAccessGranularity.height = 16;
   caps->maxDpbSlots = malformed == 4 ? 1 : 2;
   caps->maxActiveReferencePictures = malformed == 9 ? 0 : 1;
   caps->stdHeaderVersion.specVersion = VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_SPEC_VERSION;
   strcpy(caps->stdHeaderVersion.extensionName, VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_EXTENSION_NAME);
   if (malformed == 5)
      caps->stdHeaderVersion.specVersion++;
   encode->rateControlModes = malformed == 6 ? 0 : VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR;
   encode->maxQualityLevels = malformed == 10 ? 0 : 1;
   codec->maxLevelIdc = malformed == 11 ? STD_VIDEO_H264_LEVEL_IDC_3_2 : STD_VIDEO_H264_LEVEL_IDC_5_2;
   codec->maxPPictureL0ReferenceCount = malformed == 17 ? 0 : 1;
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL formats(VkPhysicalDevice physical,
      const VkPhysicalDeviceVideoFormatInfoKHR *info, uint32_t *count,
      VkVideoFormatPropertiesKHR *properties)
{
   VkResult status = result();
   assert(physical == HANDLE(VkPhysicalDevice, 1));
   assert(info->imageUsage == VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR ||
         info->imageUsage == VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR);
   if (status != VK_SUCCESS)
      return status;
   if (!properties)
      *count = 1;
   else
   {
      assert(*count == 1);
      properties->format = malformed == 7 ? VK_FORMAT_R8_UNORM : VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
      properties->imageType = VK_IMAGE_TYPE_2D;
      properties->imageTiling = malformed == 15 ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
      properties->imageUsageFlags = malformed == 16 ? 0 : info->imageUsage;
   }
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL create_session(VkDevice device,
      const VkVideoSessionCreateInfoKHR *info, const VkAllocationCallbacks *allocator,
      VkVideoSessionKHR *session)
{
   VkResult status = result();
   assert(device == HANDLE(VkDevice, 2) && !allocator && !live_session);
   assert(info->maxDpbSlots == 2 && info->maxActiveReferencePictures == 1);
   assert(info->maxCodedExtent.width == 192 && info->maxCodedExtent.height == 128);
   *session = HANDLE(VkVideoSessionKHR, status == VK_SUCCESS ? 3 : 999);
   if (status == VK_SUCCESS)
      live_session = 1;
   return status;
}
static VKAPI_ATTR void VKAPI_CALL destroy_session(VkDevice device,
      VkVideoSessionKHR session, const VkAllocationCallbacks *allocator)
{
   assert(device == HANDLE(VkDevice, 2) && !allocator && live_session && !live_parameters);
   assert(session == HANDLE(VkVideoSessionKHR, 3));
   live_session = 0;
}
static VKAPI_ATTR VkResult VKAPI_CALL requirements(VkDevice device,
      VkVideoSessionKHR session, uint32_t *count, VkVideoSessionMemoryRequirementsKHR *memory)
{
   unsigned i;
   VkResult status = result();
   assert(device == HANDLE(VkDevice, 2) && session == HANDLE(VkVideoSessionKHR, 3));
   if (status != VK_SUCCESS)
      return status;
   if (!memory)
   {
      *count = malformed == 19 ? 33 : malformed == 20 ? 0 : 2;
      if (malformed == 20) bound = 1;
   }
   else
      for (i = 0; i < 2; i++)
      {
         assert(memory[i].sType == VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR);
         memory[i].memoryBindIndex = i ? 8 : 3;
         memory[i].memoryRequirements.size = 4096;
         memory[i].memoryRequirements.alignment = 256;
         memory[i].memoryRequirements.memoryTypeBits = malformed == 8 ? 4 : i ? 2 : 8;
      }
   return VK_SUCCESS;
}
static VKAPI_ATTR VkResult VKAPI_CALL allocate(VkDevice device,
      const VkMemoryAllocateInfo *info, const VkAllocationCallbacks *allocator,
      VkDeviceMemory *memory)
{
   VkResult status = result();
   assert(device == HANDLE(VkDevice, 2) && !allocator && live_session);
   assert(info->memoryTypeIndex == (memory_made ? 1 : 3));
   assert(info->allocationSize == 4096);
   *memory = HANDLE(VkDeviceMemory, status == VK_SUCCESS ? 10 + memory_made : 999);
   if (status == VK_SUCCESS)
   {
      memory_made++;
      live_memory++;
   }
   return status;
}
static VKAPI_ATTR void VKAPI_CALL free_memory(VkDevice device,
      VkDeviceMemory memory, const VkAllocationCallbacks *allocator)
{
   assert(device == HANDLE(VkDevice, 2) && !allocator && !live_session && live_memory);
   assert(memory == HANDLE(VkDeviceMemory, 10) || memory == HANDLE(VkDeviceMemory, 11));
   live_memory--;
}
static VKAPI_ATTR VkResult VKAPI_CALL bind_memory(VkDevice device,
      VkVideoSessionKHR session, uint32_t count, const VkBindVideoSessionMemoryInfoKHR *memory)
{
   VkResult status = result();
   assert(device == HANDLE(VkDevice, 2) && session == HANDLE(VkVideoSessionKHR, 3));
   assert(count == 2 && live_memory == 2);
   assert(memory[0].memoryBindIndex == 3 && memory[1].memoryBindIndex == 8);
   assert(memory[0].memorySize == 4096 && !memory[0].memoryOffset);
   if (status == VK_SUCCESS)
      bound = 1;
   return status;
}
static VKAPI_ATTR VkResult VKAPI_CALL parameters(VkDevice device,
      const VkVideoSessionParametersCreateInfoKHR *info,
      const VkAllocationCallbacks *allocator, VkVideoSessionParametersKHR *parameters)
{
   const VkVideoEncodeH264SessionParametersCreateInfoKHR *codec =
         (const VkVideoEncodeH264SessionParametersCreateInfoKHR*)info->pNext;
   const StdVideoH264SequenceParameterSet *sps = codec->pParametersAddInfo->pStdSPSs;
   VkResult status = result();
   assert(device == HANDLE(VkDevice, 2) && !allocator && live_session && bound);
   assert(info->videoSession == HANDLE(VkVideoSessionKHR, 3));
   assert(sps->pSequenceParameterSetVui->time_scale == 24);
   *parameters = HANDLE(VkVideoSessionParametersKHR, status == VK_SUCCESS ? 4 : 999);
   if (status == VK_SUCCESS)
      live_parameters = 1;
   return status;
}
static VKAPI_ATTR void VKAPI_CALL destroy_parameters(VkDevice device,
      VkVideoSessionParametersKHR parameters, const VkAllocationCallbacks *allocator)
{
   assert(device == HANDLE(VkDevice, 2) && !allocator && live_session && live_parameters);
   assert(parameters == HANDLE(VkVideoSessionParametersKHR, 4));
   live_parameters = 0;
}
static VKAPI_ATTR VkResult VKAPI_CALL headers(VkDevice device,
      const VkVideoEncodeSessionParametersGetInfoKHR *info,
      VkVideoEncodeSessionParametersFeedbackInfoKHR *feedback, size_t *size, void *data)
{
   VkResult status = result();
   assert(device == HANDLE(VkDevice, 2) && info->videoSessionParameters == HANDLE(VkVideoSessionParametersKHR, 4));
   assert(live_parameters && feedback->sType == VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_PARAMETERS_FEEDBACK_INFO_KHR);
   if (status != VK_SUCCESS)
      return status;
   if (data)
   {
      assert(*size == 8);
      memcpy(data, "OVERRIDE", 8);
   }
   *size = 8;
   return VK_SUCCESS;
}
static const vulkan_encode_h264_session_ops_t ops = {
   families, memory_properties, capabilities, formats, create_session,
   destroy_session, requirements, bind_memory, allocate, free_memory,
   parameters, destroy_parameters, headers
};
static vulkan_encode_h264_session_config_t config;
static void reset(void)
{
   assert(!live_session && !live_parameters && !live_memory);
   calls = fail_call = allocations = fail_allocation = memory_made = bound = malformed = 0;
   config.physical = HANDLE(VkPhysicalDevice, 1);
   config.device = HANDLE(VkDevice, 2);
   config.family = 1;
   config.video.width = 192; config.video.height = 128;
   config.video.fps_num = 12; config.video.fps_den = 1;
   config.video.bitrate = 1000000; config.video.full_range = false;
}
int main(void)
{
   vulkan_encode_h264_session_t *session;
   vulkan_encode_h264_session_ops_t missing;
   unsigned i;
   size_t size;
   char data[8];
   reset(); session = vulkan_encode_h264_session_new(&config, &ops);
   assert(session && calls == 12 && allocations == 5);
   memset(data, 0x55, sizeof(data));
   assert(!vulkan_encode_h264_session_headers(session, data, 7, &size) && size == 8);
   for (i = 0; i < sizeof(data); i++) assert(data[i] == 0x55);
   assert(vulkan_encode_h264_session_headers(session, data, sizeof(data), &size));
   assert(size == 8 && !memcmp(data, "OVERRIDE", 8));
   vulkan_encode_h264_session_free(session);
   for (i = 1; i <= 2; i++)
   {
      reset(); session = vulkan_encode_h264_session_new(&config, &ops);
      assert(session); fail_call = calls + i;
      assert(!vulkan_encode_h264_session_headers(session, data, sizeof(data), &size));
      assert(!size);
      vulkan_encode_h264_session_free(session);
   }
   for (i = 1; i <= 12; i++)
   {
      reset(); fail_call = i;
      assert(!vulkan_encode_h264_session_new(&config, &ops));
      assert(!live_session && !live_parameters && !live_memory);
   }
   for (i = 1; i <= 5; i++)
   {
      reset(); fail_allocation = i;
      assert(!vulkan_encode_h264_session_new(&config, &ops));
      assert(!live_session && !live_parameters && !live_memory);
   }
   for (i = 1; i <= 19; i++)
   {
      reset(); malformed = i;
      assert(!vulkan_encode_h264_session_new(&config, &ops));
      assert(!live_session && !live_parameters && !live_memory);
   }
   reset(); malformed = 20;
   session = vulkan_encode_h264_session_new(&config, &ops);
   assert(session && !live_memory);
   vulkan_encode_h264_session_free(session);
   reset(); config.family = 2;
   assert(!vulkan_encode_h264_session_new(&config, &ops));
   config.family = 0;
   assert(!vulkan_encode_h264_session_new(&config, &ops));
   assert(!vulkan_encode_h264_session_new(NULL, &ops));
   assert(!vulkan_encode_h264_session_new(&config, NULL));
#define MISSING(member) do { missing = ops; missing.member = NULL; \
   assert(!vulkan_encode_h264_session_new(&config, &missing)); } while (0)
   reset();
   MISSING(families); MISSING(memory); MISSING(capabilities); MISSING(formats);
   MISSING(create); MISSING(destroy); MISSING(requirements); MISSING(bind);
   MISSING(allocate); MISSING(free_memory); MISSING(parameters);
   MISSING(destroy_parameters); MISSING(headers);
   puts("Vulkan H264 session startup/partial failure/headers ownership PASS");
   return 0;
}
