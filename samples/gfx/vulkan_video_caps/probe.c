/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define VK_NO_PROTOTYPES
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include "../../../gfx/common/vulkan_video_caps.h"

static void probe_hevc(VkPhysicalDevice gpu,
      PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR get_caps)
{
   unsigned depth;
   for (depth = 8; depth <= 10; depth += 2)
   {
      VkVideoDecodeH265ProfileInfoKHR hevc;
      VkVideoProfileInfoKHR profile;
      VkVideoCapabilitiesKHR caps;
      VkVideoDecodeCapabilitiesKHR decode;
      VkVideoDecodeH265CapabilitiesKHR hc;
      VkResult ret;
      memset(&hevc, 0, sizeof(hevc)); memset(&profile, 0, sizeof(profile));
      memset(&caps, 0, sizeof(caps)); memset(&decode, 0, sizeof(decode));
      memset(&hc, 0, sizeof(hc));
      hevc.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_PROFILE_INFO_KHR;
      hevc.stdProfileIdc = depth == 8 ? STD_VIDEO_H265_PROFILE_IDC_MAIN : STD_VIDEO_H265_PROFILE_IDC_MAIN_10;
      profile.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR; profile.pNext = &hevc;
      profile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR;
      profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
      profile.lumaBitDepth = profile.chromaBitDepth = depth == 8
         ? VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR : VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
      caps.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR; caps.pNext = &decode;
      decode.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR; decode.pNext = &hc;
      hc.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_H265_CAPABILITIES_KHR;
      ret = get_caps(gpu, &profile, &caps);
      printf("  HEVC %u-bit: result %d, max %ux%u, DPB slots %u\n", depth,
            (int)ret, caps.maxCodedExtent.width, caps.maxCodedExtent.height, caps.maxDpbSlots);
   }
}

static int probe_gpu(VkInstance instance, VkPhysicalDevice gpu,
      PFN_vkGetInstanceProcAddr get_proc)
{
   PFN_vkEnumerateDeviceExtensionProperties get_extensions;
   PFN_vkGetPhysicalDeviceQueueFamilyProperties2 get_queues;
   PFN_vkGetPhysicalDeviceProperties get_properties;
   PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR get_caps;
   VkExtensionProperties *extensions = NULL;
   VkQueueFamilyProperties2 *queues = NULL;
   VkQueueFamilyVideoPropertiesKHR *video = NULL;
   VkPhysicalDeviceProperties properties;
   uint32_t n = 0, count = 0, i, graphics = UINT32_MAX, codecs, selected = 0;
   int family, ok = 0;
   get_extensions = (PFN_vkEnumerateDeviceExtensionProperties)get_proc(instance, "vkEnumerateDeviceExtensionProperties");
   get_queues = (PFN_vkGetPhysicalDeviceQueueFamilyProperties2)get_proc(instance, "vkGetPhysicalDeviceQueueFamilyProperties2");
   get_properties = (PFN_vkGetPhysicalDeviceProperties)get_proc(instance, "vkGetPhysicalDeviceProperties");
   get_caps = (PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR)get_proc(instance, "vkGetPhysicalDeviceVideoCapabilitiesKHR");
   if (!get_extensions || !get_queues || !get_properties) return 0;
   if (get_extensions(gpu, NULL, &n, NULL) != VK_SUCCESS || !n) goto done;
   extensions = (VkExtensionProperties*)calloc(n, sizeof(*extensions));
   if (!extensions || get_extensions(gpu, NULL, &n, extensions) != VK_SUCCESS) goto done;
   codecs = vulkan_video_supported_codecs(extensions, n);
   get_queues(gpu, &count, NULL);
   if (!count) goto done;
   queues = (VkQueueFamilyProperties2*)calloc(count, sizeof(*queues));
   video = (VkQueueFamilyVideoPropertiesKHR*)calloc(count, sizeof(*video));
   if (!queues || !video) goto done;
   for (i = 0; i < count; i++)
   {
      queues[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2; queues[i].pNext = &video[i];
      video[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR;
   }
   get_queues(gpu, &count, queues);
   for (i = 0; i < count; i++)
      if (queues[i].queueFamilyProperties.queueCount
            && (queues[i].queueFamilyProperties.queueFlags & VK_QUEUE_GRAPHICS_BIT))
      { graphics = i; break; }
   family = vulkan_video_select_decode_family(queues, video, count, graphics, codecs, &selected);
   get_properties(gpu, &properties);
   printf("%s: advertised codecs 0x%x, dedicated family %d, selected 0x%x\n",
         properties.deviceName, codecs, family, selected);
   if ((selected & VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR) && get_caps) probe_hevc(gpu, get_caps);
   ok = 1;
done:
   free(video); free(queues); free(extensions); return ok;
}

int main(void)
{
#ifdef _WIN32
   HMODULE library = LoadLibraryA("vulkan-1.dll");
#else
   void *library = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
#endif
   PFN_vkGetInstanceProcAddr get_proc;
   PFN_vkCreateInstance create_instance;
   PFN_vkDestroyInstance destroy_instance;
   PFN_vkEnumeratePhysicalDevices enumerate;
   VkApplicationInfo app;
   VkInstanceCreateInfo info;
   VkInstance instance;
   VkPhysicalDevice *gpus;
   uint32_t n = 0, i;
   int result = 0;
   if (!library) { puts("SKIP: no Vulkan loader"); return 0; }
#ifdef _WIN32
   get_proc = (PFN_vkGetInstanceProcAddr)GetProcAddress(library, "vkGetInstanceProcAddr");
#else
   get_proc = (PFN_vkGetInstanceProcAddr)dlsym(library, "vkGetInstanceProcAddr");
#endif
   if (!get_proc) { result = 1; goto unload; }
   create_instance = (PFN_vkCreateInstance)get_proc(VK_NULL_HANDLE, "vkCreateInstance");
   if (!create_instance) { result = 1; goto unload; }
   memset(&app, 0, sizeof(app)); memset(&info, 0, sizeof(info));
   app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; app.apiVersion = VK_API_VERSION_1_1;
   info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; info.pApplicationInfo = &app;
   if (create_instance(&info, NULL, &instance) != VK_SUCCESS)
   { puts("SKIP: Vulkan 1.1 instance unavailable"); goto unload; }
   destroy_instance = (PFN_vkDestroyInstance)get_proc(instance, "vkDestroyInstance");
   enumerate = (PFN_vkEnumeratePhysicalDevices)get_proc(instance, "vkEnumeratePhysicalDevices");
   if (!destroy_instance || !enumerate) { result = 1; goto unload; }
   if (enumerate(instance, &n, NULL) != VK_SUCCESS || !n)
   { puts("SKIP: no physical devices"); destroy_instance(instance, NULL); goto unload; }
   gpus = (VkPhysicalDevice*)calloc(n, sizeof(*gpus));
   if (!gpus || enumerate(instance, &n, gpus) != VK_SUCCESS) result = 1;
   else for (i = 0; i < n; i++) if (!probe_gpu(instance, gpus[i], get_proc)) result = 1;
   free(gpus); destroy_instance(instance, NULL);
unload:
#ifdef _WIN32
   FreeLibrary(library);
#else
   dlclose(library);
#endif
   return result;
}
