/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "hevc_params_test.c"

typedef struct session_api
{
   PFN_vkCreateVideoSessionKHR create;
   PFN_vkDestroyVideoSessionKHR destroy;
   PFN_vkGetVideoSessionMemoryRequirementsKHR requirements;
   PFN_vkBindVideoSessionMemoryKHR bind;
   PFN_vkCreateVideoSessionParametersKHR parameters;
   PFN_vkDestroyVideoSessionParametersKHR destroy_parameters;
   PFN_vkAllocateMemory allocate;
   PFN_vkFreeMemory free_memory;
} session_api_t;

static int test_session(VkPhysicalDevice gpu, VkDevice device, uint32_t family,
      PFN_vkGetInstanceProcAddr get, VkInstance instance, const session_api_t *api,
      const hevc_fixture_t *fixture)
{
   vulkan_hevc_params_t *p = (vulkan_hevc_params_t*)calloc(1, sizeof(*p));
   VkVideoDecodeH265ProfileInfoKHR hevc = {0};
   VkVideoProfileInfoKHR profile = {0};
   VkVideoDecodeH265CapabilitiesKHR hc = {0};
   VkVideoDecodeCapabilitiesKHR dc = {0};
   VkVideoCapabilitiesKHR caps = {0};
   VkVideoSessionCreateInfoKHR create = {0};
   VkVideoProfileListInfoKHR profiles = {0};
   VkPhysicalDeviceVideoFormatInfoKHR format = {0};
   VkVideoFormatPropertiesKHR formats[16];
   VkVideoSessionKHR session = VK_NULL_HANDLE;
   VkVideoSessionParametersKHR parameters = VK_NULL_HANDLE;
   VkVideoDecodeH265SessionParametersAddInfoKHR add = {0};
   VkVideoDecodeH265SessionParametersCreateInfoKHR hp = {0};
   VkVideoSessionParametersCreateInfoKHR cp = {0};
   VkVideoSessionMemoryRequirementsKHR *reqs = NULL;
   VkBindVideoSessionMemoryInfoKHR *binds = NULL;
   VkPhysicalDeviceMemoryProperties memory;
   uint32_t count = 0, i, j, allocated = 0, num_formats = 16;
   int ok = 0;
   VkResult result;
   PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR get_caps =
      (PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR)get(instance, "vkGetPhysicalDeviceVideoCapabilitiesKHR");
   PFN_vkGetPhysicalDeviceMemoryProperties get_memory =
      (PFN_vkGetPhysicalDeviceMemoryProperties)get(instance, "vkGetPhysicalDeviceMemoryProperties");
   PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR get_formats =
      (PFN_vkGetPhysicalDeviceVideoFormatPropertiesKHR)get(instance, "vkGetPhysicalDeviceVideoFormatPropertiesKHR");
   if (!p || !build_fixture(fixture, p) || !get_caps || !get_memory || !get_formats) goto done;
   hevc.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR;
   hevc.stdProfileIdc = p->sps_ptl.general_profile_idc;
   profile.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR; profile.pNext = &hevc;
   profile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
   profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
   profile.lumaBitDepth = profile.chromaBitDepth = fixture->depth == 8
      ? VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR : VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
   hc.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR;
   dc.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR; dc.pNext = &hc;
   caps.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR; caps.pNext = &dc;
   result = get_caps(gpu, &profile, &caps);
   if (result == VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR
         || result == VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR
         || result == VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR)
   { ok = 2; goto done; }
   if (result != VK_SUCCESS) goto done;
   if (hc.maxLevelIdc < p->sps_ptl.general_level_idc) { ok = 2; goto done; }
   create.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_CREATE_INFO_KHR;
   create.queueFamilyIndex = family; create.pVideoProfile = &profile;
   create.pictureFormat = create.referencePictureFormat = fixture->depth == 8
      ? VK_FORMAT_G8_B8R8_2PLANE_420_UNORM : VK_FORMAT_G10X6_B10X6R10X6_2PLANE_420_UNORM_3PACK16;
   create.maxCodedExtent.width = p->sps.pic_width_in_luma_samples;
   create.maxCodedExtent.height = p->sps.pic_height_in_luma_samples;
   create.maxDpbSlots = (uint32_t)p->params.max_dec_pic_buffering;
   create.maxActiveReferencePictures = create.maxDpbSlots - 1;
   if (create.maxDpbSlots > caps.maxDpbSlots || create.maxActiveReferencePictures > caps.maxActiveReferencePictures
         || create.maxCodedExtent.width < caps.minCodedExtent.width || create.maxCodedExtent.width > caps.maxCodedExtent.width
         || create.maxCodedExtent.height < caps.minCodedExtent.height || create.maxCodedExtent.height > caps.maxCodedExtent.height)
   { ok = 2; goto done; }
   profiles.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_LIST_INFO_KHR;
   profiles.profileCount = 1; profiles.pProfiles = &profile;
   format.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_FORMAT_INFO_KHR; format.pNext = &profiles;
   format.imageUsage = VK_IMAGE_USAGE_VIDEO_DECODE_DPB_BIT_KHR | VK_IMAGE_USAGE_VIDEO_DECODE_DST_BIT_KHR;
   memset(formats, 0, sizeof(formats));
   for (i = 0; i < 16; i++) formats[i].sType = VK_STRUCTURE_TYPE_VIDEO_FORMAT_PROPERTIES_KHR;
   if (get_formats(gpu, &format, &num_formats, formats) != VK_SUCCESS) goto done;
   for (i = 0; i < num_formats; i++)
      if (formats[i].format == create.pictureFormat && formats[i].imageTiling == VK_IMAGE_TILING_OPTIMAL) break;
   if (i == num_formats) { ok = 2; goto done; }
   create.pStdHeaderVersion = &caps.stdHeaderVersion;
   if (api->create(device, &create, NULL, &session) != VK_SUCCESS) goto done;
   if (api->requirements(device, session, &count, NULL) != VK_SUCCESS || count > 64) goto done;
   reqs = (VkVideoSessionMemoryRequirementsKHR*)calloc(count ? count : 1, sizeof(*reqs));
   binds = (VkBindVideoSessionMemoryInfoKHR*)calloc(count ? count : 1, sizeof(*binds));
   if (!reqs || !binds) goto done;
   for (i = 0; i < count; i++) reqs[i].sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_MEMORY_REQUIREMENTS_KHR;
   if (api->requirements(device, session, &count, reqs) != VK_SUCCESS) goto done;
   get_memory(gpu, &memory);
   for (i = 0; i < count; i++)
   {
      VkMemoryAllocateInfo allocation = {0};
      for (j = 0; j < memory.memoryTypeCount; j++)
         if ((reqs[i].memoryRequirements.memoryTypeBits & (UINT32_C(1) << j))
               && !(memory.memoryTypes[j].propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT)) break;
      if (j == memory.memoryTypeCount) goto done;
      allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocation.allocationSize = reqs[i].memoryRequirements.size; allocation.memoryTypeIndex = j;
      if (api->allocate(device, &allocation, NULL, &binds[i].memory) != VK_SUCCESS) goto done;
      allocated++;
      binds[i].sType = VK_STRUCTURE_TYPE_BIND_VIDEO_SESSION_MEMORY_INFO_KHR;
      binds[i].memoryBindIndex = reqs[i].memoryBindIndex;
      binds[i].memorySize = reqs[i].memoryRequirements.size;
   }
   if (count && api->bind(device, session, count, binds) != VK_SUCCESS) goto done;
   add.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_ADD_INFO_KHR;
   add.stdVPSCount = add.stdSPSCount = add.stdPPSCount = 1;
   add.pStdVPSs = &p->vps; add.pStdSPSs = &p->sps; add.pStdPPSs = &p->pps;
   hp.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_SESSION_PARAMETERS_CREATE_INFO_KHR;
   hp.maxStdVPSCount = hp.maxStdSPSCount = hp.maxStdPPSCount = 1; hp.pParametersAddInfo = &add;
   cp.sType = VK_STRUCTURE_TYPE_VIDEO_SESSION_PARAMETERS_CREATE_INFO_KHR;
   cp.pNext = &hp; cp.videoSession = session;
   ok = api->parameters(device, &cp, NULL, &parameters) == VK_SUCCESS;
done:
   if (parameters) api->destroy_parameters(device, parameters, NULL);
   if (session) api->destroy(device, session, NULL);
   for (i = 0; i < allocated; i++) api->free_memory(device, binds[i].memory, NULL);
   free(binds); free(reqs); free(p);
   printf("HEVC Vulkan session and parameters %s: %s\n", fixture->name,
         ok == 2 ? "SKIP (profile/limits/format unsupported)" : ok ? "PASS" : "FAIL");
   return ok;
}

static int hevc_session_probe(VkInstance instance, VkPhysicalDevice gpu,
      uint32_t family, PFN_vkGetInstanceProcAddr get)
{
   static const char *extensions[] = {"VK_KHR_synchronization2", "VK_KHR_video_queue",
      "VK_KHR_video_decode_queue", "VK_KHR_video_decode_h265"};
   VkDeviceQueueCreateInfo queue = {0};
   VkDeviceCreateInfo info = {0};
   VkDevice device;
   session_api_t api;
   float priority = 1.0f;
   unsigned i;
   int ok = 1;
   PFN_vkCreateDevice create = (PFN_vkCreateDevice)get(instance, "vkCreateDevice");
   PFN_vkGetDeviceProcAddr proc = (PFN_vkGetDeviceProcAddr)get(instance, "vkGetDeviceProcAddr");
   PFN_vkDestroyDevice destroy;
   queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   queue.queueFamilyIndex = family; queue.queueCount = 1; queue.pQueuePriorities = &priority;
   info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO; info.queueCreateInfoCount = 1;
   info.pQueueCreateInfos = &queue; info.enabledExtensionCount = 4; info.ppEnabledExtensionNames = extensions;
   if (!create || !proc || create(gpu, &info, NULL, &device) != VK_SUCCESS) return 0;
   destroy = (PFN_vkDestroyDevice)proc(device, "vkDestroyDevice");
#define LOAD(field, type, name) api.field = (type)proc(device, name); if (!api.field) ok = 0
   LOAD(create, PFN_vkCreateVideoSessionKHR, "vkCreateVideoSessionKHR");
   LOAD(destroy, PFN_vkDestroyVideoSessionKHR, "vkDestroyVideoSessionKHR");
   LOAD(requirements, PFN_vkGetVideoSessionMemoryRequirementsKHR, "vkGetVideoSessionMemoryRequirementsKHR");
   LOAD(bind, PFN_vkBindVideoSessionMemoryKHR, "vkBindVideoSessionMemoryKHR");
   LOAD(parameters, PFN_vkCreateVideoSessionParametersKHR, "vkCreateVideoSessionParametersKHR");
   LOAD(destroy_parameters, PFN_vkDestroyVideoSessionParametersKHR, "vkDestroyVideoSessionParametersKHR");
   LOAD(allocate, PFN_vkAllocateMemory, "vkAllocateMemory");
   LOAD(free_memory, PFN_vkFreeMemory, "vkFreeMemory");
#undef LOAD
   if (ok)
      for (i = 0; i < sizeof(fixtures) / sizeof(fixtures[0]); i++)
         if (!test_session(gpu, device, family, get, instance, &api, &fixtures[i])) ok = 0;
   if (destroy) destroy(device, NULL);
   return ok;
}
