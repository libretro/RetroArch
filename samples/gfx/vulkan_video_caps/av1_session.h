/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "../../../gfx/common/vulkan_av1_params.h"
#include "av1_session_data.h"

typedef struct av1_session_api
{
   PFN_vkCreateVideoSessionKHR create;
   PFN_vkDestroyVideoSessionKHR destroy;
   PFN_vkGetVideoSessionMemoryRequirementsKHR requirements;
   PFN_vkBindVideoSessionMemoryKHR bind;
   PFN_vkCreateVideoSessionParametersKHR parameters;
   PFN_vkDestroyVideoSessionParametersKHR destroy_parameters;
   PFN_vkAllocateMemory allocate;
   PFN_vkFreeMemory free_memory;
} av1_session_api_t;

static int av1_test_session(VkInstance instance, VkPhysicalDevice gpu,
                            VkDevice device, uint32_t family,
                            PFN_vkGetInstanceProcAddr get,
                            const av1_session_api_t *api,
                            const struct av1_sequence_fixture *fixture)
{
   rav1_sequence_t *seq = (rav1_sequence_t *)calloc(1, sizeof(*seq));
   vulkan_av1_sequence_t mapped;
   VkVideoDecodeAV1ProfileInfoKHR av1;
   VkVideoProfileInfoKHR profile;
   VkVideoDecodeAV1CapabilitiesKHR ac;
   VkVideoDecodeCapabilitiesKHR decode;
   VkVideoCapabilitiesKHR caps;
   VkVideoSessionCreateInfoKHR create;
   VkVideoSessionParametersCreateInfoKHR params;
   VkVideoDecodeAV1SessionParametersCreateInfoKHR ap;
   VkVideoSessionMemoryRequirementsKHR *reqs = NULL;
   VkBindVideoSessionMemoryInfoKHR *binds = NULL;
   VkPhysicalDeviceMemoryProperties memory;
   VkVideoSessionKHR session = VK_NULL_HANDLE;
   VkVideoSessionParametersKHR parameters = VK_NULL_HANDLE;
   VkResult ret;
   uint32_t count = 0, capacity, i, j, allocated = 0;
   int ok = 0;
   PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR get_caps =
       (PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR)get(
           instance, "vkGetPhysicalDeviceVideoCapabilitiesKHR");
   PFN_vkGetPhysicalDeviceMemoryProperties get_memory =
       (PFN_vkGetPhysicalDeviceMemoryProperties)get(
           instance, "vkGetPhysicalDeviceMemoryProperties");
   if (!seq || !get_caps || !get_memory ||
       !rav1_sequence_parse(seq, fixture->data, fixture->size) ||
       !vulkan_av1_sequence(&mapped, seq) || seq->bit_depth != fixture->depth ||
       seq->film_grain_params_present != fixture->grain)
      goto done;
   av1.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PROFILE_INFO_KHR;
   av1.pNext = NULL;
   av1.stdProfile = mapped.header.seq_profile;
   av1.filmGrainSupport = seq->film_grain_params_present ? VK_TRUE : VK_FALSE;
   profile.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR;
   profile.pNext = &av1;
   profile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR;
   profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
   profile.lumaBitDepth = profile.chromaBitDepth =
       fixture->depth == 8 ? VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR
                           : VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
   ac.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_CAPABILITIES_KHR;
   ac.pNext = NULL;
   decode.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR;
   decode.pNext = &ac;
   caps.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
   caps.pNext = &decode;
   ret = get_caps(gpu, &profile, &caps);
   if (ret == VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR ||
       ret == VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR ||
       ret == VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR)
   {
      ok = 2;
      goto done;
   }
   if (ret != VK_SUCCESS)
      goto done;
   if ((unsigned)ac.maxLevel < seq->operating_points[0].level ||
       caps.maxDpbSlots < 9 || caps.maxActiveReferencePictures < 7 ||
       seq->max_width < caps.minCodedExtent.width ||
       seq->max_width > caps.maxCodedExtent.width ||
       seq->max_height < caps.minCodedExtent.height ||
       seq->max_height > caps.maxCodedExtent.height)
   {
      ok = 2;
      goto done;
   }
   create.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR;
   create.pNext = NULL;
   create.queueFamilyIndex = family;
   create.flags = 0;
   create.pVideoProfile = &profile;
   create.pictureFormat = create.referencePictureFormat =
       fixture->depth == 8
           ? VK_FORMAT_G8_B8R8_2PLANE_420_UNORM
           : VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
   create.maxCodedExtent.width = seq->max_width;
   create.maxCodedExtent.height = seq->max_height;
   create.maxDpbSlots = 9;
   create.maxActiveReferencePictures = 7;
   create.pStdHeaderVersion = &caps.stdHeaderVersion;
   if (api->create(device, &create, NULL, &session) != VK_SUCCESS)
      goto done;
   if (api->requirements(device, session, &count, NULL) != VK_SUCCESS ||
       count > 64)
      goto done;
   capacity = count;
   reqs = (VkVideoSessionMemoryRequirementsKHR *)calloc(count ? count : 1,
                                                        sizeof(*reqs));
   binds = (VkBindVideoSessionMemoryInfoKHR *)calloc(count ? count : 1,
                                                     sizeof(*binds));
   if (!reqs || !binds)
      goto done;
   for (i = 0; i < count; i++)
   {
      reqs[i].sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR;
      reqs[i].pNext = NULL;
   }
   if (api->requirements(device, session, &count, reqs) != VK_SUCCESS ||
       count > capacity)
      goto done;
   get_memory(gpu, &memory);
   for (i = 0; i < count; i++)
   {
      VkMemoryAllocateInfo allocation;
      for (j = 0; j < memory.memoryTypeCount; j++)
         if ((reqs[i].memoryRequirements.memoryTypeBits & (UINT32_C(1) << j)) &&
             !(memory.memoryTypes[j].propertyFlags &
               VK_MEMORY_PROPERTY_PROTECTED_BIT))
            break;
      if (j == memory.memoryTypeCount)
         goto done;
      allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocation.pNext = NULL;
      allocation.allocationSize = reqs[i].memoryRequirements.size;
      allocation.memoryTypeIndex = j;
      if (api->allocate(device, &allocation, NULL, &binds[i].memory) !=
          VK_SUCCESS)
         goto done;
      allocated++;
      binds[i].sType = VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR;
      binds[i].pNext = NULL;
      binds[i].memoryBindIndex = reqs[i].memoryBindIndex;
      binds[i].memoryOffset = 0;
      binds[i].memorySize = reqs[i].memoryRequirements.size;
   }
   if (count && api->bind(device, session, count, binds) != VK_SUCCESS)
      goto done;
   ap.sType =
       VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_SESSION_PARAMETERS_CREATE_INFO_KHR;
   ap.pNext = NULL;
   ap.pStdSequenceHeader = &mapped.header;
   params.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR;
   params.pNext = &ap;
   params.flags = 0;
   params.videoSessionParametersTemplate = VK_NULL_HANDLE;
   params.videoSession = session;
   ok = api->parameters(device, &params, NULL, &parameters) == VK_SUCCESS;
done:
   if (parameters)
      api->destroy_parameters(device, parameters, NULL);
   if (session)
      api->destroy(device, session, NULL);
   for (i = 0; i < allocated; i++)
      api->free_memory(device, binds[i].memory, NULL);
   free(binds);
   free(reqs);
   free(seq);
   printf("AV1 Vulkan session and parameters %s: %s\n", fixture->name,
          ok == 2 ? "SKIP (profile/limits unsupported)"
          : ok    ? "PASS"
                  : "FAIL");
   return ok;
}

static int av1_session_probe(VkInstance instance, VkPhysicalDevice gpu,
                             uint32_t family, PFN_vkGetInstanceProcAddr get)
{
   static const char *extensions[] = {
       "VK_KHR_synchronization2", "VK_KHR_video_queue",
       "VK_KHR_video_decode_queue", "VK_KHR_video_decode_av1"};
   VkDeviceQueueCreateInfo queue;
   VkDeviceCreateInfo info;
   VkDevice device = VK_NULL_HANDLE;
   av1_session_api_t api;
   float priority = 1.0f;
   unsigned i;
   int ok = 1;
   PFN_vkCreateDevice create =
       (PFN_vkCreateDevice)get(instance, "vkCreateDevice");
   PFN_vkDestroyDevice destroy;
   if (!create)
      return 0;
   queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   queue.pNext = NULL;
   queue.flags = 0;
   queue.queueFamilyIndex = family;
   queue.queueCount = 1;
   queue.pQueuePriorities = &priority;
   info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   info.pNext = NULL;
   info.flags = 0;
   info.queueCreateInfoCount = 1;
   info.pQueueCreateInfos = &queue;
   info.enabledLayerCount = 0;
   info.ppEnabledLayerNames = NULL;
   info.enabledExtensionCount = sizeof(extensions) / sizeof(extensions[0]);
   info.ppEnabledExtensionNames = extensions;
   info.pEnabledFeatures = NULL;
   if (create(gpu, &info, NULL, &device) != VK_SUCCESS)
      return 0;
   destroy = (PFN_vkDestroyDevice)get(instance, "vkDestroyDevice");
   api.create =
       (PFN_vkCreateVideoSessionKHR)get(instance, "vkCreateVideoSessionKHR");
   api.destroy =
       (PFN_vkDestroyVideoSessionKHR)get(instance, "vkDestroyVideoSessionKHR");
   api.requirements = (PFN_vkGetVideoSessionMemoryRequirementsKHR)get(
       instance, "vkGetVideoSessionMemoryRequirementsKHR");
   api.bind = (PFN_vkBindVideoSessionMemoryKHR)get(
       instance, "vkBindVideoSessionMemoryKHR");
   api.parameters = (PFN_vkCreateVideoSessionParametersKHR)get(
       instance, "vkCreateVideoSessionParametersKHR");
   api.destroy_parameters = (PFN_vkDestroyVideoSessionParametersKHR)get(
       instance, "vkDestroyVideoSessionParametersKHR");
   api.allocate = (PFN_vkAllocateMemory)get(instance, "vkAllocateMemory");
   api.free_memory = (PFN_vkFreeMemory)get(instance, "vkFreeMemory");
   if (!destroy || !api.create || !api.destroy || !api.requirements ||
       !api.bind || !api.parameters || !api.destroy_parameters ||
       !api.allocate || !api.free_memory)
      ok = 0;
   if (ok)
      for (i = 0;
           i < sizeof(av1_sequence_fixtures) / sizeof(av1_sequence_fixtures[0]);
           i++)
         if (!av1_test_session(instance, gpu, device, family, get, &api,
                               &av1_sequence_fixtures[i]))
            ok = 0;
   if (destroy)
      destroy(device, NULL);
   return ok;
}
