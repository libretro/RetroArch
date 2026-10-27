/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define VK_NO_PROTOTYPES
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include "../../../gfx/common/vulkan_video_caps.h"
#ifdef AV1_SESSION_TEST
#include "av1_session.h"
#endif
#ifdef VULKAN_IMPORT_TEST
#include "import_test.h"
#elif defined(HEVC_DECODE_TEST)
#include "hevc_decode.h"
#elif defined(HEVC_SESSION_TEST)
#include "hevc_session.h"
#endif

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

static bool probe_vp9(VkPhysicalDevice gpu,
      PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR get_caps,
      PFN_vkGetInstanceProcAddr get, VkInstance instance)
{
   VkPhysicalDeviceVideoDecodeVP9FeaturesKHR feature;
   VkPhysicalDeviceFeatures2 features;
   PFN_vkGetPhysicalDeviceFeatures2 query = (PFN_vkGetPhysicalDeviceFeatures2)
      get(instance, "vkGetPhysicalDeviceFeatures2");
   unsigned depth;
   memset(&feature, 0, sizeof(feature)); memset(&features, 0, sizeof(features));
   feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VIDEO_DECODE_VP9_FEATURES_KHR;
   features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2; features.pNext = &feature;
   if (query) query(gpu, &features);
   printf("  VP9 decode feature: %u\n", feature.videoDecodeVP9);
   for (depth = 8; depth <= 10; depth += 2)
   {
      VkVideoDecodeVP9ProfileInfoKHR vp9;
      VkVideoProfileInfoKHR profile;
      VkVideoCapabilitiesKHR caps;
      VkVideoDecodeCapabilitiesKHR decode;
      VkVideoDecodeVP9CapabilitiesKHR vc;
      VkResult ret;
      memset(&vp9, 0, sizeof(vp9)); memset(&profile, 0, sizeof(profile));
      memset(&caps, 0, sizeof(caps)); memset(&decode, 0, sizeof(decode)); memset(&vc, 0, sizeof(vc));
      vp9.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_VP9_PROFILE_INFO_KHR;
      vp9.stdProfile = depth == 8 ? STD_VIDEO_VP9_PROFILE_0 : STD_VIDEO_VP9_PROFILE_2;
      profile.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR; profile.pNext = &vp9;
      profile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR;
      profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
      profile.lumaBitDepth = profile.chromaBitDepth = depth == 8
         ? VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR : VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
      caps.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR; caps.pNext = &decode;
      decode.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR; decode.pNext = &vc;
      vc.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_VP9_CAPABILITIES_KHR;
      ret = get_caps(gpu, &profile, &caps);
      printf("  VP9 %u-bit: result %d, max %ux%u, DPB slots %u, flags 0x%x\n", depth,
            (int)ret, caps.maxCodedExtent.width, caps.maxCodedExtent.height, caps.maxDpbSlots, decode.flags);
   }
   return feature.videoDecodeVP9 != VK_FALSE;
}

static void probe_av1(VkPhysicalDevice gpu,
      PFN_vkGetPhysicalDeviceVideoCapabilitiesKHR get_caps)
{
   unsigned depth, grain;
   for (depth = 8; depth <= 10; depth += 2)
   {
      for (grain = 0; grain <= 1; grain++)
      {
         VkVideoDecodeAV1ProfileInfoKHR av1;
         VkVideoProfileInfoKHR profile;
         VkVideoCapabilitiesKHR caps;
         VkVideoDecodeCapabilitiesKHR decode;
         VkVideoDecodeAV1CapabilitiesKHR ac;
         VkResult ret;
         av1.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_PROFILE_INFO_KHR;
         av1.pNext = NULL;
         av1.stdProfile = STD_VIDEO_AV1_PROFILE_MAIN;
         av1.filmGrainSupport = grain ? VK_TRUE : VK_FALSE;
         profile.sType = VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR;
         profile.pNext = &av1;
         profile.videoCodecOperation = VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR;
         profile.chromaSubsampling = VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
         profile.lumaBitDepth = profile.chromaBitDepth = depth == 8
            ? VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR
            : VK_VIDEO_COMPONENT_BIT_DEPTH_10_BIT_KHR;
         caps.sType = VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
         caps.pNext = &decode;
         decode.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_CAPABILITIES_KHR;
         decode.pNext = &ac;
         ac.sType = VK_STRUCTURE_TYPE_VIDEO_DECODE_AV1_CAPABILITIES_KHR;
         ac.pNext = NULL;
         ret = get_caps(gpu, &profile, &caps);
         printf("  AV1 Main %u-bit, grain %u: result %d", depth, grain, (int)ret);
         if (ret == VK_SUCCESS)
            printf(", extent %ux%u..%ux%u, DPB %u, references %u, flags 0x%x, level %u",
                  caps.minCodedExtent.width, caps.minCodedExtent.height,
                  caps.maxCodedExtent.width, caps.maxCodedExtent.height,
                  caps.maxDpbSlots, caps.maxActiveReferencePictures,
                  decode.flags, (unsigned)ac.maxLevel);
         putchar('\n');
      }
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
   unsigned nprops_vp9;
   int have_vp9 = 0, have_av1 = 0;
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
   for (nprops_vp9 = 0; nprops_vp9 < n; nprops_vp9++)
      if (!strcmp(extensions[nprops_vp9].extensionName, VK_KHR_VIDEO_DECODE_VP9_EXTENSION_NAME))
         have_vp9 = 1;
   for (i = 0; i < n; i++)
      if (!strcmp(extensions[i].extensionName, VK_KHR_VIDEO_DECODE_AV1_EXTENSION_NAME))
         have_av1 = 1;
   if (have_av1 && get_caps)
      probe_av1(gpu, get_caps);
   if (have_vp9 && get_caps && !probe_vp9(gpu, get_caps, get_proc, instance))
      codecs &= ~VK_VIDEO_CODEC_OPERATION_DECODE_VP9_BIT_KHR;
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
#ifdef AV1_SESSION_TEST
   {
      uint32_t j, common = 0;
      int av1_family = -1;
      for (i = 0; i < VULKAN_VIDEO_BASE_EXTENSIONS; i++)
         for (j = 0; j < n; j++)
            if (!strcmp(extensions[j].extensionName, vulkan_video_device_extensions[i]))
            {
               common++;
               break;
            }
      for (i = 0; i < count; i++)
         if (i != graphics && queues[i].queueFamilyProperties.queueCount
               && (queues[i].queueFamilyProperties.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR)
               && (video[i].videoCodecOperations & VK_VIDEO_CODEC_OPERATION_DECODE_AV1_BIT_KHR))
         {
            av1_family = (int)i;
            break;
         }
      if (have_av1 && common == VULKAN_VIDEO_BASE_EXTENSIONS && av1_family >= 0)
         ok = av1_session_probe(instance, gpu, (uint32_t)av1_family, get_proc);
      else
         puts("SKIP: no AV1 video extensions/dedicated family; no sessions created");
   }
#endif
#ifdef VULKAN_IMPORT_TEST
   if (graphics != UINT32_MAX && (queues[graphics].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT))
      ok = import_test_gpu(instance, gpu, graphics, get_proc);
   else puts("SKIP: no graphics/compute family for import");
#endif
#ifdef HEVC_SESSION_TEST
   if (selected & VK_VIDEO_CODEC_OPERATION_DECODE_H265_BIT_KHR)
#ifdef HEVC_DECODE_TEST
      ok = hevc_decode_probe(instance, gpu, (uint32_t)family, graphics, selected, get_proc);
#else
      ok = hevc_session_probe(instance, gpu, (uint32_t)family, get_proc);
#endif
   else puts("SKIP: no selected HEVC decode family; no sessions created");
#endif
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
   if (!library)
   {
      puts("SKIP: no Vulkan loader");
#ifdef HEVC_DECODE_TEST
      if (getenv("H26X_TEST_WEBM") || getenv("H26X_TEST_MP4")) return 77;
#endif
      return 0;
   }
#ifdef _WIN32
   get_proc = (PFN_vkGetInstanceProcAddr)GetProcAddress(library, "vkGetInstanceProcAddr");
#else
   get_proc = (PFN_vkGetInstanceProcAddr)dlsym(library, "vkGetInstanceProcAddr");
#endif
   if (!get_proc) { result = 1; goto unload; }
   create_instance = (PFN_vkCreateInstance)get_proc(VK_NULL_HANDLE, "vkCreateInstance");
   if (!create_instance) { result = 1; goto unload; }
   memset(&app, 0, sizeof(app)); memset(&info, 0, sizeof(info));
   app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
#ifdef HEVC_DECODE_TEST
   app.apiVersion = VK_API_VERSION_1_3;
#else
   app.apiVersion = VK_API_VERSION_1_1;
#endif
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
#ifdef HEVC_DECODE_TEST
   if (!result && (getenv("H26X_TEST_WEBM") || getenv("H26X_TEST_MP4"))
         && (!h26x_webm_executed || h26x_webm_skipped))
   {
      puts("SKIP: Matroska Vulkan fixture did not execute on a suitable device");
      result = 77;
   }
#ifdef HAVE_RVP9
   if (!result && getenv("VP9_TEST_MP4") && !vp9_mp4_executed)
   {
      puts("SKIP: VP9 MP4 Vulkan fixture did not execute on a suitable device");
      result = 77;
   }
#endif
#endif
#ifdef _WIN32
   FreeLibrary(library);
#else
   dlclose(library);
#endif
   return result;
}
