/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "vulkan_encode_h264_session.h"
#include <stdlib.h>

static bool vehs_family(const vulkan_encode_h264_session_config_t *config,
      const vulkan_encode_h264_session_ops_t *ops)
{
   struct family_properties
   {
      VkQueueFamilyProperties2 family[64];
      VkQueueFamilyVideoPropertiesKHR video[64];
   } *properties;
   uint32_t count = 0, i;
   bool valid;
   ops->families(config->physical, &count, NULL);
   if (!count || count > 64 || config->family >= count)
      return false;
   properties = (struct family_properties*)calloc(1, sizeof(*properties));
   if (!properties)
      return false;
   for (i = 0; i < count; i++)
   {
      properties->family[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
      properties->family[i].pNext = &properties->video[i];
      properties->video[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR;
      properties->video[i].pNext = NULL;
   }
   ops->families(config->physical, &count, properties->family);
   valid = count <= 64 && config->family < count &&
         properties->family[config->family].queueFamilyProperties.queueCount &&
         (properties->family[config->family].queueFamilyProperties.queueFlags &
               VK_QUEUE_VIDEO_ENCODE_BIT_KHR) &&
         (properties->video[config->family].videoCodecOperations &
               VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR);
   free(properties);
   return valid;
}

static bool vehs_format(const vulkan_encode_h264_session_config_t *config,
      vulkan_encode_h264_session_t *s, VkImageUsageFlags usage)
{
   VkPhysicalDeviceVideoFormatInfoKHR info;
   VkVideoFormatPropertiesKHR *formats;
   uint32_t count = 0, i, capacity;
   bool found = false;
   info.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR;
   info.pNext = &s->profiles;
   info.imageUsage = usage;
   if (s->ops->formats(config->physical, &info, &count, NULL) != VK_SUCCESS ||
         !count || count > 64)
      return false;
   formats = (VkVideoFormatPropertiesKHR*)calloc(count, sizeof(*formats));
   if (!formats)
      return false;
   capacity = count;
   for (i = 0; i < count; i++)
   {
      formats[i].sType = VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR;
      formats[i].pNext = NULL;
   }
   if (s->ops->formats(config->physical, &info, &count, formats) == VK_SUCCESS &&
         count <= capacity)
      for (i = 0; i < count; i++)
         if (formats[i].format == VK_FORMAT_G8_B8R8_2PLANE_420_UNORM &&
             formats[i].imageType == VK_IMAGE_TYPE_2D &&
             formats[i].imageTiling == VK_IMAGE_TILING_OPTIMAL &&
             (formats[i].imageUsageFlags & usage) == usage)
            found = true;
   free(formats);
   return found;
}

static bool vehs_bind(vulkan_encode_h264_session_t *s,
      VkPhysicalDevice physical)
{
   struct session_memory
   {
      VkVideoSessionMemoryRequirementsKHR requirements[32];
      VkBindVideoSessionMemoryInfoKHR bind[32];
      VkPhysicalDeviceMemoryProperties properties;
   } *memory;
   VkMemoryAllocateInfo allocate;
   uint32_t count = 0, i, j, chosen;
   bool bound = false;
   if (s->ops->requirements(s->device, s->session, &count, NULL) != VK_SUCCESS ||
       count > 32)
      return false;
   if (!count)
      return true;
   memory = (struct session_memory*)calloc(1, sizeof(*memory));
   if (!memory)
      return false;
   for (i = 0; i < count; i++)
   {
      memory->requirements[i].sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR;
      memory->requirements[i].pNext = NULL;
   }
   s->ops->memory(physical, &memory->properties);
   if (s->ops->requirements(s->device, s->session, &count,
            memory->requirements) != VK_SUCCESS || count > 32 ||
         memory->properties.memoryTypeCount > VK_MAX_MEMORY_TYPES)
      goto done;
   if (!count)
   {
      bound = true;
      goto done;
   }
   for (i = 0; i < count; i++)
   {
      const VkMemoryRequirements *req = &memory->requirements[i].memoryRequirements;
      chosen = VK_MAX_MEMORY_TYPES;
      for (j = 0; j < memory->properties.memoryTypeCount; j++)
      {
         VkMemoryPropertyFlags flags = memory->properties.memoryTypes[j].propertyFlags;
         if (!(req->memoryTypeBits & (1u << j)) ||
               (flags & VK_MEMORY_PROPERTY_PROTECTED_BIT))
            continue;
         if (chosen == VK_MAX_MEMORY_TYPES)
            chosen = j;
         if (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
         {
            chosen = j;
            break;
         }
      }
      if (!req->size || chosen == VK_MAX_MEMORY_TYPES)
         goto done;
      allocate.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocate.pNext = NULL;
      allocate.allocationSize = req->size;
      allocate.memoryTypeIndex = chosen;
      if (s->ops->allocate(s->device, &allocate, NULL,
               &s->memory[s->memory_count]) != VK_SUCCESS)
         goto done;
      memory->bind[i].sType = VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR;
      memory->bind[i].pNext = NULL;
      memory->bind[i].memoryBindIndex = memory->requirements[i].memoryBindIndex;
      memory->bind[i].memory = s->memory[s->memory_count++];
      memory->bind[i].memoryOffset = 0;
      memory->bind[i].memorySize = req->size;
   }
   bound = s->ops->bind(s->device, s->session, count, memory->bind) == VK_SUCCESS;
done:
   free(memory);
   return bound;
}

void vulkan_encode_h264_session_free(vulkan_encode_h264_session_t *s)
{
   unsigned i;
   if (!s)
      return;
   if (s->parameters)
      s->ops->destroy_parameters(s->device, s->parameters, NULL);
   if (s->session)
      s->ops->destroy(s->device, s->session, NULL);
   for (i = 0; i < s->memory_count; i++)
      s->ops->free_memory(s->device, s->memory[i], NULL);
   free(s);
}

vulkan_encode_h264_session_t *vulkan_encode_h264_session_new(
      const vulkan_encode_h264_session_config_t *config,
      const vulkan_encode_h264_session_ops_t *ops)
{
   vulkan_encode_h264_session_t *s;
   VkVideoSessionCreateInfoKHR create;
   VkVideoSessionKHR session;
   VkVideoSessionParametersKHR native_parameters;
   VkVideoSessionParametersCreateInfoKHR parameters;
   VkVideoEncodeH264SessionParametersCreateInfoKHR codec;
   VkVideoEncodeH264SessionParametersAddInfoKHR add;
   if (!config || !config->physical || !config->device || !ops ||
       !ops->families || !ops->memory || !ops->capabilities || !ops->formats ||
       !ops->create || !ops->destroy || !ops->requirements || !ops->bind ||
       !ops->allocate || !ops->free_memory || !ops->parameters ||
       !ops->destroy_parameters || !ops->headers || !vehs_family(config, ops))
      return NULL;
   s = (vulkan_encode_h264_session_t*)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
   s->ops = ops;
   s->device = config->device;
   s->codec_profile.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_INFO_KHR;
   s->codec_profile.pNext = NULL;
   s->codec_profile.stdProfileIdc = STD_VIDEO_H264_PROFILE_IDC_HIGH;
   s->profile.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR;
   s->profile.pNext = &s->codec_profile;
   s->profile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR;
   s->profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
   s->profile.lumaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
   s->profile.chromaBitDepth = VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
   s->profiles.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR;
   s->profiles.pNext = NULL;
   s->profiles.profileCount = 1;
   s->profiles.pProfiles = &s->profile;
   s->capabilities.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
   s->capabilities.pNext = &s->encode;
   s->encode.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_CAPABILITIES_KHR;
   s->encode.pNext = &s->codec;
   s->codec.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_CAPABILITIES_KHR;
   s->codec.pNext = NULL;
   if (ops->capabilities(config->physical, &s->profile, &s->capabilities) != VK_SUCCESS ||
       !vulkan_encode_h264_params(&s->params, &config->video, s->codec.maxLevelIdc) ||
       s->capabilities.maxDpbSlots < 2 || s->capabilities.maxActiveReferencePictures < 1 ||
       !(s->encode.rateControlModes & VK_VIDEO_ENCODE_RATE_CONTROL_MODE_DISABLED_BIT_KHR) ||
       !s->encode.maxQualityLevels || !s->codec.maxPPictureL0ReferenceCount ||
       s->capabilities.stdHeaderVersion.specVersion !=
             VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_SPEC_VERSION ||
       strcmp(s->capabilities.stdHeaderVersion.extensionName,
             VK_STD_VULKAN_VIDEO_CODEC_H264_ENCODE_EXTENSION_NAME) ||
       !s->capabilities.pictureAccessGranularity.width ||
       !s->capabilities.pictureAccessGranularity.height ||
       s->params.coded_width % s->capabilities.pictureAccessGranularity.width ||
       s->params.coded_height % s->capabilities.pictureAccessGranularity.height ||
       s->params.coded_width < s->capabilities.minCodedExtent.width ||
       s->params.coded_height < s->capabilities.minCodedExtent.height ||
       s->params.coded_width > s->capabilities.maxCodedExtent.width ||
       s->params.coded_height > s->capabilities.maxCodedExtent.height ||
       !vehs_format(config, s, VK_IMAGE_USAGE_VIDEO_ENCODE_SRC_BIT_KHR) ||
       !vehs_format(config, s, VK_IMAGE_USAGE_VIDEO_ENCODE_DPB_BIT_KHR))
      goto fail;
   create.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR;
   create.pNext = NULL;
   create.queueFamilyIndex = config->family;
   create.flags = 0;
   create.pVideoProfile = &s->profile;
   create.pictureFormat = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
   create.maxCodedExtent.width = s->params.coded_width;
   create.maxCodedExtent.height = s->params.coded_height;
   create.referencePictureFormat = VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;
   create.maxDpbSlots = 2;
   create.maxActiveReferencePictures = 1;
   create.pStdHeaderVersion = &s->capabilities.stdHeaderVersion;
   if (ops->create(s->device, &create, NULL, &session) != VK_SUCCESS)
      goto fail;
   s->session = session;
   if (!vehs_bind(s, config->physical))
      goto fail;
   add.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_ADD_INFO_KHR;
   add.pNext = NULL;
   add.stdSPSCount = 1;
   add.pStdSPSs = &s->params.sps;
   add.stdPPSCount = 1;
   add.pStdPPSs = &s->params.pps;
   codec.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_CREATE_INFO_KHR;
   codec.pNext = NULL;
   codec.maxStdSPSCount = 1;
   codec.maxStdPPSCount = 1;
   codec.pParametersAddInfo = &add;
   parameters.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR;
   parameters.pNext = &codec;
   parameters.flags = 0;
   parameters.videoSessionParametersTemplate = VK_NULL_HANDLE;
   parameters.videoSession = s->session;
   if (ops->parameters(s->device, &parameters, NULL, &native_parameters) != VK_SUCCESS)
      goto fail;
   s->parameters = native_parameters;
   return s;
fail:
   vulkan_encode_h264_session_free(s);
   return NULL;
}

bool vulkan_encode_h264_session_headers(vulkan_encode_h264_session_t *s,
      void *data, size_t capacity, size_t *size)
{
   VkVideoEncodeH264SessionParametersGetInfoKHR codec;
   VkVideoEncodeSessionParametersGetInfoKHR info;
   VkVideoEncodeSessionParametersFeedbackInfoKHR feedback;
   size_t required = 0, written;
   if (!s || !size)
      return false;
   *size = 0;
   codec.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_SESSION_PARAMETERS_GET_INFO_KHR;
   codec.pNext = NULL;
   codec.writeStdSPS = VK_TRUE;
   codec.writeStdPPS = VK_TRUE;
   codec.stdSPSId = 0;
   codec.stdPPSId = 0;
   info.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_PARAMETERS_GET_INFO_KHR;
   info.pNext = &codec;
   info.videoSessionParameters = s->parameters;
   feedback.sType = VK_STRUCTURE_TYPE_VIDEO_ENCODE_SESSION_PARAMETERS_FEEDBACK_INFO_KHR;
   feedback.pNext = NULL;
   if (s->ops->headers(s->device, &info, &feedback, &required, NULL) != VK_SUCCESS ||
         !required || required > 65536)
      return false;
   *size = required;
   if (!data || capacity < required)
      return false;
   written = required;
   if (s->ops->headers(s->device, &info, &feedback, &written, data) != VK_SUCCESS ||
         !written || written > required)
   {
      *size = 0;
      return false;
   }
   *size = written;
   return true;
}
