/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef RARCH_HEVC_NATIVE_CONTEXT_H
#define RARCH_HEVC_NATIVE_CONTEXT_H
#ifdef NDEBUG
#error Native acceptance requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dynamic/dylib.h>
#include "../../../gfx/common/vulkan_encode_hevc_session.h"
struct native_encoder
{
   VkPhysicalDeviceFeatures2 features;
   VkPhysicalDeviceVulkan12Features available12, enabled12;
   VkPhysicalDeviceVulkan13Features available13, enabled13;
   dylib_t library;
   VkInstance instance;
   VkPhysicalDevice physical;
   VkDevice device;
   VkQueue queue, producer_queue;
   VkPhysicalDeviceFeatures enabled_features;
   VkDebugUtilsMessengerEXT messenger;
   PFN_vkGetInstanceProcAddr instance_proc;
   PFN_vkGetDeviceProcAddr device_proc;
   PFN_vkDestroyInstance destroy_instance;
   PFN_vkDestroyDevice destroy_device;
   PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger;
   VkPhysicalDeviceProperties properties;
   vulkan_encode_hevc_session_ops_t ops;
   VkQueueFlags queue_flags;
   unsigned family, producer_family, validation_errors;
};
#define INSTANCE(member, name) do { \
   c->member=(PFN_vk##name)c->instance_proc(c->instance,"vk" #name); \
   assert(c->member); \
} while (0)
#define DEVICE(member, name) do { \
   c->member=(PFN_vk##name)c->device_proc(c->device,"vk" #name); \
   assert(c->member); \
} while (0)
static VKAPI_ATTR VkBool32 VKAPI_CALL debug_message(
      VkDebugUtilsMessageSeverityFlagBitsEXT severity,
      VkDebugUtilsMessageTypeFlagsEXT type,
      const VkDebugUtilsMessengerCallbackDataEXT *message,void *userdata)
{
   struct native_encoder *c=(struct native_encoder*)userdata;
   (void)type;
   if (severity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
   {
      c->validation_errors++;
      fprintf(stderr,"%s\n",message->pMessage);
   }
   return VK_FALSE;
}
static bool validation_available(struct native_encoder *c)
{
   PFN_vkEnumerateInstanceLayerProperties enumerate;
   VkLayerProperties *layers;
   uint32_t count=0,i;
   bool found=false;
   enumerate=(PFN_vkEnumerateInstanceLayerProperties)c->instance_proc(VK_NULL_HANDLE,
         "vkEnumerateInstanceLayerProperties");
   assert(enumerate && enumerate(&count,NULL)==VK_SUCCESS && count<=128);
   layers=(VkLayerProperties*)calloc(count?count:1,sizeof(*layers));
   assert(layers && enumerate(&count,layers)==VK_SUCCESS);
   for (i=0;i<count;i++)
      if (!strcmp(layers[i].layerName,"VK_LAYER_KHRONOS_validation"))
         found=true;
   free(layers);
   return found;
}
static bool select_device(struct native_encoder *c, bool compute)
{
   PFN_vkEnumeratePhysicalDevices enumerate;
   PFN_vkGetPhysicalDeviceProperties properties;
   PFN_vkGetPhysicalDeviceQueueFamilyProperties2 families;
   PFN_vkEnumerateDeviceExtensionProperties extensions;
   VkPhysicalDevice *devices;
   VkQueueFamilyProperties2 *queues;
   VkQueueFamilyVideoPropertiesKHR *video;
   VkExtensionProperties *names;
   static const char *required[]={VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
      VK_KHR_VIDEO_ENCODE_QUEUE_EXTENSION_NAME,VK_KHR_VIDEO_ENCODE_H265_EXTENSION_NAME};
   uint32_t count=0,n,i,j,k;
   unsigned supported;
   bool found=false;
   enumerate=(PFN_vkEnumeratePhysicalDevices)c->instance_proc(c->instance,"vkEnumeratePhysicalDevices");
   properties=(PFN_vkGetPhysicalDeviceProperties)c->instance_proc(c->instance,"vkGetPhysicalDeviceProperties");
   families=(PFN_vkGetPhysicalDeviceQueueFamilyProperties2)c->instance_proc(c->instance,"vkGetPhysicalDeviceQueueFamilyProperties2");
   extensions=(PFN_vkEnumerateDeviceExtensionProperties)c->instance_proc(c->instance,"vkEnumerateDeviceExtensionProperties");
   assert(enumerate && properties && families && extensions);
   assert(enumerate(c->instance,&count,NULL)==VK_SUCCESS && count<=32);
   if (!count)
      return false;
   devices=(VkPhysicalDevice*)calloc(count,sizeof(*devices));
   assert(devices && enumerate(c->instance,&count,devices)==VK_SUCCESS);
   for (i=0;i<count && !found;i++)
   {
      properties(devices[i],&c->properties);
      if (c->properties.apiVersion<VK_API_VERSION_1_3)
         continue;
      n=0;
      assert(extensions(devices[i],NULL,&n,NULL)==VK_SUCCESS && n<=1024);
      names=(VkExtensionProperties*)calloc(n?n:1,sizeof(*names));
      assert(names && extensions(devices[i],NULL,&n,names)==VK_SUCCESS);
      supported=0;
      for (j=0;j<3;j++)
         for (k=0;k<n;k++)
            if (!strcmp(required[j],names[k].extensionName))
            {
               supported++;
               break;
            }
      free(names);
      if (supported!=3)
         continue;
      n=0;
      families(devices[i],&n,NULL);
      assert(n && n<=64);
      queues=(VkQueueFamilyProperties2*)calloc(n,sizeof(*queues));
      video=(VkQueueFamilyVideoPropertiesKHR*)calloc(n,sizeof(*video));
      assert(queues && video);
      for (j=0;j<n;j++)
      {
         queues[j].sType=VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
         queues[j].pNext=&video[j];
         video[j].sType=VK_STRUCTURE_TYPE_QUEUE_FAMILY_VIDEO_PROPERTIES_KHR;
      }
      families(devices[i],&n,queues);
      assert(n<=64);
      for (j=0;j<n;j++)
         if (queues[j].queueFamilyProperties.queueCount
               && (queues[j].queueFamilyProperties.queueFlags&VK_QUEUE_VIDEO_ENCODE_BIT_KHR)
               && (video[j].videoCodecOperations&VK_VIDEO_CODEC_OPERATION_ENCODE_H265_BIT_KHR))
         {
            c->queue_flags=queues[j].queueFamilyProperties.queueFlags;
            c->physical=devices[i];
            c->family=j;
            found=true;
            break;
         }
      c->producer_family=c->family;
      if (found && compute)
      {
         bool producer=false;
         for (j=0;j<n;j++)
            if (queues[j].queueFamilyProperties.queueCount &&
                  (queues[j].queueFamilyProperties.queueFlags & VK_QUEUE_COMPUTE_BIT))
            {
               c->producer_family=j;
               producer=true;
               break;
            }
         found=producer;
      }
      free(video);
      free(queues);
   }
   free(devices);
   return found;
}
static bool startup_mode(struct native_encoder *c, bool compute)
{
   VkApplicationInfo app;
   VkInstanceCreateInfo instance;
   VkDebugUtilsMessengerCreateInfoEXT debug;
   VkDeviceQueueCreateInfo queue[2];
   VkDeviceCreateInfo device;
   PFN_vkCreateInstance create_instance;
   VkValidationFeaturesEXT validation;
   VkValidationFeatureEnableEXT synchronization = VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
   PFN_vkCreateDevice create_device;
   PFN_vkGetDeviceQueue get_queue;
   PFN_vkGetPhysicalDeviceFeatures2 get_features;
   PFN_vkCreateDebugUtilsMessengerEXT create_messenger;
   const char *layer = "VK_LAYER_KHRONOS_validation";
   const char *debug_extensions[] = {VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME};
   const char *extensions[] = {VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
      VK_KHR_VIDEO_ENCODE_QUEUE_EXTENSION_NAME, VK_KHR_VIDEO_ENCODE_H265_EXTENSION_NAME};
   float priority = 1;
#ifdef _WIN32
   c->library = dylib_load("vulkan-1.dll");
#else
   c->library = dylib_load("libvulkan.so.1");
#endif
   if (!c->library)
      return false;
   c->instance_proc = (PFN_vkGetInstanceProcAddr)dylib_proc(c->library, "vkGetInstanceProcAddr");
   if (!c->instance_proc || !validation_available(c))
      return false;
   create_instance = (PFN_vkCreateInstance)c->instance_proc(VK_NULL_HANDLE, "vkCreateInstance");
   assert(create_instance);
   app.pNext = NULL;
   app.applicationVersion = 0;
   app.pEngineName = NULL;
   app.engineVersion = 0;
   app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   app.apiVersion = VK_API_VERSION_1_3;
   app.pApplicationName = "RetroArch native HEVC parameters";
   debug.pNext = NULL;
   debug.flags = 0;
   debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
   debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
   debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
   debug.pfnUserCallback = debug_message;
   debug.pUserData = c;
   instance.flags = 0;
   instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   validation.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
   validation.pNext = &debug;
   validation.enabledValidationFeatureCount = 1;
   validation.pEnabledValidationFeatures = &synchronization;
   validation.disabledValidationFeatureCount = 0;
   validation.pDisabledValidationFeatures = NULL;
   instance.pNext = &validation;
   instance.pApplicationInfo = &app;
   instance.enabledLayerCount = 1;
   instance.enabledExtensionCount = 2;
   instance.ppEnabledLayerNames = &layer;
   instance.ppEnabledExtensionNames = debug_extensions;
   assert(create_instance(&instance, NULL, &c->instance) == VK_SUCCESS);
   INSTANCE(destroy_instance, DestroyInstance);
   INSTANCE(destroy_messenger, DestroyDebugUtilsMessengerEXT);
   create_messenger = (PFN_vkCreateDebugUtilsMessengerEXT)c->instance_proc(c->instance,
         "vkCreateDebugUtilsMessengerEXT");
   assert(create_messenger && create_messenger(c->instance, &debug, NULL, &c->messenger) == VK_SUCCESS);
   if (!select_device(c, compute))
      return false;
   queue[0].pNext = queue[1].pNext = NULL;
   queue[0].flags = queue[1].flags = 0;
   queue[0].sType = queue[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   queue[0].queueFamilyIndex = c->family;
   queue[1].queueFamilyIndex = c->producer_family;
   queue[0].queueCount = queue[1].queueCount = 1;
   queue[0].pQueuePriorities = queue[1].pQueuePriorities = &priority;
   get_features = (PFN_vkGetPhysicalDeviceFeatures2)c->instance_proc(c->instance,
         "vkGetPhysicalDeviceFeatures2");
   assert(get_features);
   c->features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
   c->features.pNext = &c->available12;
   c->available12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
   c->available12.pNext = &c->available13;
   c->available13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
   get_features(c->physical, &c->features);
   if (!c->available12.timelineSemaphore || !c->available13.synchronization2)
      return false;
   c->enabled12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
   c->enabled12.pNext = &c->enabled13;
   c->enabled12.timelineSemaphore = VK_TRUE;
   c->enabled13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
   c->enabled13.synchronization2 = VK_TRUE;
   device.pNext = &c->enabled12;
   device.flags = 0;
   device.enabledLayerCount = 0;
   device.ppEnabledLayerNames = NULL;
   c->enabled_features.shaderStorageImageExtendedFormats = compute &&
         c->features.features.shaderStorageImageExtendedFormats;
   device.pEnabledFeatures = &c->enabled_features;
   device.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   device.queueCreateInfoCount = c->family == c->producer_family ? 1 : 2;
   device.pQueueCreateInfos = queue;
   device.enabledExtensionCount = 3;
   device.ppEnabledExtensionNames = extensions;
   create_device = (PFN_vkCreateDevice)c->instance_proc(c->instance, "vkCreateDevice");
   assert(create_device && create_device(c->physical, &device, NULL, &c->device) == VK_SUCCESS);
   INSTANCE(device_proc, GetDeviceProcAddr);
   DEVICE(destroy_device, DestroyDevice);
   get_queue = (PFN_vkGetDeviceQueue)c->device_proc(c->device, "vkGetDeviceQueue");
   assert(get_queue);
   get_queue(c->device, c->family, 0, &c->queue);
   get_queue(c->device, c->producer_family, 0, &c->producer_queue);
   INSTANCE(ops.families, GetPhysicalDeviceQueueFamilyProperties2);
   INSTANCE(ops.memory, GetPhysicalDeviceMemoryProperties);
   INSTANCE(ops.capabilities, GetPhysicalDeviceVideoCapabilitiesKHR);
   INSTANCE(ops.formats, GetPhysicalDeviceVideoFormatPropertiesKHR);
   DEVICE(ops.create, CreateVideoSessionKHR);
   DEVICE(ops.destroy, DestroyVideoSessionKHR);
   DEVICE(ops.requirements, GetVideoSessionMemoryRequirementsKHR);
   DEVICE(ops.bind, BindVideoSessionMemoryKHR);
   DEVICE(ops.allocate, AllocateMemory);
   DEVICE(ops.free_memory, FreeMemory);
   DEVICE(ops.parameters, CreateVideoSessionParametersKHR);
   DEVICE(ops.destroy_parameters, DestroyVideoSessionParametersKHR);
   DEVICE(ops.headers, GetEncodedVideoSessionParametersKHR);
   return true;
}

#define startup(c) startup_mode((c), false)
#endif
