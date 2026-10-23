/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "../../../gfx/common/vaapi_hevc_vulkan_frame.h"
#include <xf86drm.h>

static VkInstance import_instance;
static VkPhysicalDevice import_gpu;
static VkDevice import_device;
static VkPhysicalDeviceMemoryProperties import_memory;
static vulkan_prime_image_ops_t import_ops;
static unsigned imported;
#include "native_import_gpu.h"
static VkDebugUtilsMessengerEXT import_messenger;
static PFN_vkDestroyDebugUtilsMessengerEXT import_destroy_messenger;

static bool native_import_open(unsigned major_id, unsigned minor_id)
{
   drmDevicePtr drm = NULL;
   static struct stat status;
   static VkPhysicalDevice devices[32];
   static VkQueueFamilyProperties families[64];
   static VkPhysicalDeviceProperties properties;
   static const char *extensions[] = {VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
         VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,
         VK_EXT_IMAGE_DRM_FORMAT_MODIFIER_EXTENSION_NAME,
         VK_EXT_QUEUE_FAMILY_FOREIGN_EXTENSION_NAME,
         VK_KHR_IMAGE_FORMAT_LIST_EXTENSION_NAME};
   static VkApplicationInfo app;
   static VkInstanceCreateInfo info;
   static VkDeviceQueueCreateInfo queue_info;
   static VkDeviceCreateInfo device_info;
   static vulkan_drm_identity_t identity;
   static VkDebugUtilsMessengerCreateInfoEXT debug_info;
   const char *layer = "VK_LAYER_KHRONOS_validation";
   const char *debug = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
   PFN_vkCreateDebugUtilsMessengerEXT create_messenger;
   unsigned i, family;
   uint32_t count = 32, queues;
   float priority = 1;
   bool render;
   if (drmGetDeviceFromDevId(makedev(major_id, minor_id), 0, &drm) || !drm)
      return false;
   render = (drm->available_nodes & (1 << DRM_NODE_RENDER)) && drm->nodes
            && drm->nodes[DRM_NODE_RENDER]
            && !stat(drm->nodes[DRM_NODE_RENDER], &status)
            && S_ISCHR(status.st_mode) && major(status.st_rdev) == major_id
            && minor(status.st_rdev) == minor_id;
   drmFreeDevice(&drm);
   if (!render)
      return false;
   memset(&app, 0, sizeof(app));
   app.sType      = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   app.apiVersion = VK_API_VERSION_1_1;
   memset(&info, 0, sizeof(info));
   info.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   info.pApplicationInfo = &app;
   memset(&debug_info, 0, sizeof(debug_info));
   debug_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
   debug_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
   debug_info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                            | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                            | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
   debug_info.pfnUserCallback = import_debug;
   info.pNext                 = &debug_info;
   info.enabledLayerCount = info.enabledExtensionCount = 1;
   info.ppEnabledLayerNames                            = &layer;
   info.ppEnabledExtensionNames                        = &debug;
   if (vkCreateInstance(&info, NULL, &import_instance) != VK_SUCCESS)
      return false;
   create_messenger = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
         import_instance, "vkCreateDebugUtilsMessengerEXT");
   import_destroy_messenger =
         (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
               import_instance, "vkDestroyDebugUtilsMessengerEXT");
   if (!create_messenger || !import_destroy_messenger
         || create_messenger(
                  import_instance, &debug_info, NULL, &import_messenger)
                  != VK_SUCCESS)
      return false;
   if (vkEnumeratePhysicalDevices(import_instance, &count, devices)
               != VK_SUCCESS
         || !count || count > 32)
      return false;
   for (i = 0; i < count; i++)
      if (vulkan_drm_identity_query(&identity, devices[i],
                vkEnumerateDeviceExtensionProperties,
                vkGetPhysicalDeviceProperties2)
            && identity.major_id == major_id && identity.minor_id == minor_id)
      {
         import_gpu = devices[i];
         break;
      }
   if (!import_gpu)
      return false;
   vkGetPhysicalDeviceProperties(import_gpu, &properties);
   if (properties.apiVersion < VK_API_VERSION_1_1)
      return false;
   queues = 64;
   vkGetPhysicalDeviceQueueFamilyProperties(import_gpu, &queues, families);
   if (queues > 64)
      return false;
   for (family = 0; family < queues; family++)
      if (families[family].queueCount
            && (families[family].queueFlags
                     & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
                     == (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
         break;
   if (family == queues)
      return false;
   memset(&queue_info, 0, sizeof(queue_info));
   queue_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   queue_info.queueFamilyIndex = family;
   queue_info.queueCount       = 1;
   queue_info.pQueuePriorities = &priority;
   memset(&device_info, 0, sizeof(device_info));
   device_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   device_info.queueCreateInfoCount    = 1;
   device_info.pQueueCreateInfos       = &queue_info;
   device_info.enabledExtensionCount   = 5;
   device_info.ppEnabledExtensionNames = extensions;
   if (vkCreateDevice(import_gpu, &device_info, NULL, &import_device)
         != VK_SUCCESS)
      return false;
   vkGetPhysicalDeviceMemoryProperties(import_gpu, &import_memory);
   import_family = family;
   vkGetDeviceQueue(import_device, family, 0, &import_queue);
   import_ops.create_image  = vkCreateImage;
   import_ops.destroy_image = vkDestroyImage;
   import_ops.requirements  = vkGetImageMemoryRequirements2;
   import_ops.fd_properties =
         (PFN_vkGetMemoryFdPropertiesKHR)vkGetDeviceProcAddr(
               import_device, "vkGetMemoryFdPropertiesKHR");
   import_ops.allocate     = vkAllocateMemory;
   import_ops.free_memory  = vkFreeMemory;
   import_ops.bind         = vkBindImageMemory2;
   import_ops.create_view  = vkCreateImageView;
   import_ops.destroy_view = vkDestroyImageView;
   if (!import_ops.fd_properties || !vulkan_prime_image_bind_io(&import_ops))
      return false;
   printf("Native VA modifier import device: %s, DRM %u:%u\n",
         properties.deviceName, major_id, minor_id);
   return true;
}

static void native_import_close(void)
{
   if (import_device)
      vkDestroyDevice(import_device, NULL);
   if (import_messenger)
      import_destroy_messenger(import_instance, import_messenger, NULL);
   if (import_instance)
      vkDestroyInstance(import_instance, NULL);
   import_device   = VK_NULL_HANDLE;
   import_instance = VK_NULL_HANDLE;
}

static void native_import_check(const vaapi_frame_view_t *view,
      const video_hw_frame_t *lease, const VAImage *image, const uint8_t *data)
{
   static vaapi_hevc_vulkan_frame_t frame;
   const vaapi_hevc_export_t *source = view->source;
   vaapi_hevc_export_t returned;
   vaapi_frame_view_t restored;
   unsigned before = fd_count();
   memset(&returned, 0, sizeof(returned));
   assert(vaapi_hevc_vulkan_frame_prepare(&frame, source, import_gpu,
         import_device, true, &import_memory, &import_ops,
         vkGetPhysicalDeviceFormatProperties2,
         vkGetPhysicalDeviceImageFormatProperties2));
   assert(vaapi_hevc_vulkan_frame_matches(&frame, source));
   assert(frame.layers[0].image && frame.layers[1].image && frame.layers[0].view
          && frame.layers[1].view);
   assert(vaapi_frame_take(
         lease->owner, lease->done, lease->done_value, &frame.source));
   native_import_convert(&frame, view, image, data);
   assert(vaapi_hevc_vulkan_frame_retire(&frame, &returned));
   assert(returned.active && !source->active);
   assert(vaapi_frame_unclaim(
         lease->owner, lease->done, lease->done_value, &returned));
   assert(vaapi_frame_claim(lease->owner, lease->done, lease->done_value,
         lease->image, &restored));
   assert(!returned.active && restored.source == source && source->active
         && source->ready && restored.chroma_position == view->chroma_position);
   assert(fd_count() == before);
   imported++;
}
