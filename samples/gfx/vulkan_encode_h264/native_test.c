/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error Native acceptance requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dynamic/dylib.h>
#include "../../../gfx/common/vulkan_encode_h264.h"

struct native_encoder
{
   dylib_t library;
   VkInstance instance;
   VkPhysicalDevice physical;
   VkDevice device;
   VkQueue queue;
   VkDebugUtilsMessengerEXT messenger;
   unsigned validation_errors;
   PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger;
   unsigned family;
   PFN_vkGetInstanceProcAddr instance_proc;
   PFN_vkGetDeviceProcAddr device_proc;
   PFN_vkDestroyInstance destroy_instance;
   PFN_vkDestroyDevice destroy_device;
   VkPhysicalDeviceProperties properties;
   VkPhysicalDeviceFeatures2 features;
   VkPhysicalDeviceVulkan12Features version12;
   VkPhysicalDeviceVulkan13Features version13;
   VkVideoEncodeH264ProfileInfoKHR codec_profile;
   VkVideoProfileInfoKHR profile;
   VkVideoCapabilitiesKHR caps;
   VkVideoEncodeCapabilitiesKHR encode_caps;
   VkVideoEncodeH264CapabilitiesKHR codec_caps;
   vulkan_encode_h264_ops_t ops;
   vulkan_encode_h264_open_t config;
   uint8_t headers[65536];
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
      if (!strcmp(layers[i].layerName,"VK_LAYER_KHRONOS_validation")) found=true;
   free(layers);
   return found;
}
static void load_ops(struct native_encoder *c)
{
   INSTANCE(ops.session.families, GetPhysicalDeviceQueueFamilyProperties2);
   INSTANCE(ops.session.memory, GetPhysicalDeviceMemoryProperties);
   INSTANCE(ops.session.capabilities, GetPhysicalDeviceVideoCapabilitiesKHR);
   INSTANCE(ops.session.formats, GetPhysicalDeviceVideoFormatPropertiesKHR);
   DEVICE(ops.session.create, CreateVideoSessionKHR);
   DEVICE(ops.session.destroy, DestroyVideoSessionKHR);
   DEVICE(ops.session.requirements, GetVideoSessionMemoryRequirementsKHR);
   DEVICE(ops.session.bind, BindVideoSessionMemoryKHR);
   DEVICE(ops.session.allocate, AllocateMemory);
   DEVICE(ops.session.free_memory, FreeMemory);
   DEVICE(ops.session.parameters, CreateVideoSessionParametersKHR);
   DEVICE(ops.session.destroy_parameters, DestroyVideoSessionParametersKHR);
   DEVICE(ops.session.headers, GetEncodedVideoSessionParametersKHR);
   INSTANCE(ops.pool.memory_properties, GetPhysicalDeviceMemoryProperties);
   INSTANCE(ops.pool.video_formats, GetPhysicalDeviceVideoFormatPropertiesKHR);
   INSTANCE(ops.pool.image_properties, GetPhysicalDeviceImageFormatProperties2);
   INSTANCE(ops.pool.format_properties, GetPhysicalDeviceFormatProperties2);
   DEVICE(ops.pool.create_image, CreateImage);
   DEVICE(ops.pool.destroy_image, DestroyImage);
   DEVICE(ops.pool.image_memory, GetImageMemoryRequirements);
   DEVICE(ops.pool.bind_image, BindImageMemory);
   DEVICE(ops.pool.create_view, CreateImageView);
   DEVICE(ops.pool.destroy_view, DestroyImageView);
   DEVICE(ops.pool.create_buffer, CreateBuffer);
   DEVICE(ops.pool.destroy_buffer, DestroyBuffer);
   DEVICE(ops.pool.buffer_memory, GetBufferMemoryRequirements);
   DEVICE(ops.pool.bind_buffer, BindBufferMemory);
   DEVICE(ops.pool.map, MapMemory);
   DEVICE(ops.pool.unmap, UnmapMemory);
   DEVICE(ops.pool.allocate, AllocateMemory);
   DEVICE(ops.pool.free_memory, FreeMemory);
   DEVICE(ops.pool.create_commands, CreateCommandPool);
   DEVICE(ops.pool.destroy_commands, DestroyCommandPool);
   DEVICE(ops.pool.allocate_command, AllocateCommandBuffers);
   DEVICE(ops.pool.create_fence, CreateFence);
   DEVICE(ops.pool.destroy_fence, DestroyFence);
   DEVICE(ops.pool.create_queries, CreateQueryPool);
   DEVICE(ops.pool.destroy_queries, DestroyQueryPool);
   DEVICE(ops.pool.create_semaphore, CreateSemaphore);
   DEVICE(ops.pool.destroy_semaphore, DestroySemaphore);
   DEVICE(ops.submit.reset_command, ResetCommandBuffer);
   DEVICE(ops.submit.begin, BeginCommandBuffer);
   DEVICE(ops.submit.end, EndCommandBuffer);
   DEVICE(ops.submit.barrier, CmdPipelineBarrier2);
   DEVICE(ops.submit.reset_query, CmdResetQueryPool);
   DEVICE(ops.submit.begin_query, CmdBeginQuery);
   DEVICE(ops.submit.end_query, CmdEndQuery);
   DEVICE(ops.submit.begin_coding, CmdBeginVideoCodingKHR);
   DEVICE(ops.submit.control, CmdControlVideoCodingKHR);
   DEVICE(ops.submit.encode, CmdEncodeVideoKHR);
   DEVICE(ops.submit.end_coding, CmdEndVideoCodingKHR);
   DEVICE(ops.submit.reset_fence, ResetFences);
   DEVICE(ops.submit.submit, QueueSubmit);
   DEVICE(ops.submit.status, GetFenceStatus);
   DEVICE(ops.submit.feedback, GetQueryPoolResults);
}
static bool select_device(struct native_encoder *c)
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
      VK_KHR_VIDEO_ENCODE_QUEUE_EXTENSION_NAME,VK_KHR_VIDEO_ENCODE_H264_EXTENSION_NAME};
   uint32_t count=0,n,i,j,k;
   unsigned supported;
   bool found=false;
   enumerate=(PFN_vkEnumeratePhysicalDevices)c->instance_proc(c->instance,"vkEnumeratePhysicalDevices");
   properties=(PFN_vkGetPhysicalDeviceProperties)c->instance_proc(c->instance,"vkGetPhysicalDeviceProperties");
   families=(PFN_vkGetPhysicalDeviceQueueFamilyProperties2)c->instance_proc(c->instance,"vkGetPhysicalDeviceQueueFamilyProperties2");
   extensions=(PFN_vkEnumerateDeviceExtensionProperties)c->instance_proc(c->instance,"vkEnumerateDeviceExtensionProperties");
   assert(enumerate && properties && families && extensions);
   assert(enumerate(c->instance,&count,NULL)==VK_SUCCESS && count<=32);
   if (!count) return false;
   devices=(VkPhysicalDevice*)calloc(count,sizeof(*devices));
   assert(devices && enumerate(c->instance,&count,devices)==VK_SUCCESS);
   for (i=0;i<count && !found;i++)
   {
      properties(devices[i],&c->properties);
      if (c->properties.apiVersion<VK_API_VERSION_1_3) continue;
      n=0;
      assert(extensions(devices[i],NULL,&n,NULL)==VK_SUCCESS && n<=1024);
      names=(VkExtensionProperties*)calloc(n?n:1,sizeof(*names));
      assert(names && extensions(devices[i],NULL,&n,names)==VK_SUCCESS);
      supported=0;
      for (j=0;j<3;j++)
         for (k=0;k<n;k++)
            if (!strcmp(required[j],names[k].extensionName)) { supported++; break; }
      free(names);
      if (supported!=3) continue;
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
               && (video[j].videoCodecOperations&VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR))
         {
            c->physical=devices[i]; c->family=j; found=true; break;
         }
      free(video); free(queues);
   }
   free(devices);
   return found;
}
static bool startup(struct native_encoder *c)
{
   VkApplicationInfo application;
   VkDebugUtilsMessengerCreateInfoEXT debug;
   PFN_vkCreateDebugUtilsMessengerEXT create_messenger;
   const char *layer="VK_LAYER_KHRONOS_validation";
   const char *debug_extension=VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
   VkInstanceCreateInfo instance;
   VkDeviceQueueCreateInfo queue;
   VkDeviceCreateInfo device;
   VkPhysicalDeviceFeatures enabled;
   PFN_vkCreateInstance create_instance;
   PFN_vkCreateDevice create_device;
   PFN_vkGetDeviceQueue get_queue;
   PFN_vkGetPhysicalDeviceFeatures2 features;
   static const char *extensions[]={VK_KHR_VIDEO_QUEUE_EXTENSION_NAME,
      VK_KHR_VIDEO_ENCODE_QUEUE_EXTENSION_NAME,VK_KHR_VIDEO_ENCODE_H264_EXTENSION_NAME};
   float priority=1;
   bool storage;
#ifdef _WIN32
   c->library=dylib_load("vulkan-1.dll");
#else
   c->library=dylib_load("libvulkan.so.1");
#endif
   if (!c->library) return false;
   c->instance_proc=(PFN_vkGetInstanceProcAddr)dylib_proc(c->library,"vkGetInstanceProcAddr");
   if (!c->instance_proc || !validation_available(c)) return false;
   create_instance=(PFN_vkCreateInstance)c->instance_proc(VK_NULL_HANDLE,"vkCreateInstance");
   assert(create_instance);
   memset(&application,0,sizeof(application));
   application.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO;
   application.pApplicationName="RetroArch native encoder acceptance";
   application.apiVersion=VK_API_VERSION_1_3;
   memset(&instance,0,sizeof(instance));
   instance.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   instance.pApplicationInfo=&application;
   memset(&debug,0,sizeof(debug));
   debug.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
   debug.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
   debug.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
      |VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
   debug.pfnUserCallback=debug_message;
   debug.pUserData=c;
   instance.pNext=&debug;
   instance.enabledLayerCount=1;
   instance.ppEnabledLayerNames=&layer;
   instance.enabledExtensionCount=1;
   instance.ppEnabledExtensionNames=&debug_extension;
   if (create_instance(&instance,NULL,&c->instance)!=VK_SUCCESS) return false;
   INSTANCE(destroy_instance,DestroyInstance);
   INSTANCE(destroy_messenger,DestroyDebugUtilsMessengerEXT);
   create_messenger=(PFN_vkCreateDebugUtilsMessengerEXT)c->instance_proc(c->instance,
         "vkCreateDebugUtilsMessengerEXT");
   assert(create_messenger && create_messenger(c->instance,&debug,NULL,&c->messenger)==VK_SUCCESS);
   if (!select_device(c)) return false;
   features=(PFN_vkGetPhysicalDeviceFeatures2)c->instance_proc(c->instance,"vkGetPhysicalDeviceFeatures2");
   assert(features);
   c->features.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
   c->features.pNext=&c->version12;
   c->version12.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
   c->version12.pNext=&c->version13;
   c->version13.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
   features(c->physical,&c->features);
   if (!c->version12.timelineSemaphore || !c->version13.synchronization2) return false;
   storage=c->features.features.shaderStorageImageExtendedFormats!=0;
   memset(&enabled,0,sizeof(enabled));
   enabled.shaderStorageImageExtendedFormats=storage;
   memset(&c->version12,0,sizeof(c->version12));
   memset(&c->version13,0,sizeof(c->version13));
   c->version12.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
   c->version12.pNext=&c->version13;
   c->version12.timelineSemaphore=VK_TRUE;
   c->version13.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
   c->version13.synchronization2=VK_TRUE;
   memset(&queue,0,sizeof(queue));
   queue.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   queue.queueFamilyIndex=c->family;
   queue.queueCount=1;
   queue.pQueuePriorities=&priority;
   memset(&device,0,sizeof(device));
   device.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   device.pNext=&c->version12;
   device.pEnabledFeatures=&enabled;
   device.queueCreateInfoCount=1;
   device.pQueueCreateInfos=&queue;
   device.enabledExtensionCount=3;
   device.ppEnabledExtensionNames=extensions;
   create_device=(PFN_vkCreateDevice)c->instance_proc(c->instance,"vkCreateDevice");
   assert(create_device && create_device(c->physical,&device,NULL,&c->device)==VK_SUCCESS);
   INSTANCE(device_proc,GetDeviceProcAddr);
   DEVICE(destroy_device,DestroyDevice);
   get_queue=(PFN_vkGetDeviceQueue)c->device_proc(c->device,"vkGetDeviceQueue");
   assert(get_queue);
   get_queue(c->device,c->family,0,&c->queue);
   load_ops(c);
   c->codec_profile.sType=VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_PROFILE_INFO_KHR;
   c->codec_profile.stdProfileIdc=STD_VIDEO_H264_PROFILE_IDC_HIGH;
   c->profile.sType=VK_STRUCTURE_TYPE_VIDEO_PROFILE_INFO_KHR;
   c->profile.pNext=&c->codec_profile;
   c->profile.videoCodecOperation=VK_VIDEO_CODEC_OPERATION_ENCODE_H264_BIT_KHR;
   c->profile.chromaSubsampling=VK_VIDEO_CHROMA_SUBSAMPLING_420_BIT_KHR;
   c->profile.lumaBitDepth=c->profile.chromaBitDepth=VK_VIDEO_COMPONENT_BIT_DEPTH_8_BIT_KHR;
   c->caps.sType=VK_STRUCTURE_TYPE_VIDEO_CAPABILITIES_KHR;
   c->caps.pNext=&c->encode_caps;
   c->encode_caps.sType=VK_STRUCTURE_TYPE_VIDEO_ENCODE_CAPABILITIES_KHR;
   c->encode_caps.pNext=&c->codec_caps;
   c->codec_caps.sType=VK_STRUCTURE_TYPE_VIDEO_ENCODE_H264_CAPABILITIES_KHR;
   if (c->ops.session.capabilities(c->physical,&c->profile,&c->caps)!=VK_SUCCESS)
      return false;
   if (!c->caps.pictureAccessGranularity.width || !c->caps.pictureAccessGranularity.height
         || c->caps.pictureAccessGranularity.width>4096 || c->caps.pictureAccessGranularity.height>4096)
      return false;
   c->config.session.physical=c->physical;
   c->config.session.device=c->device;
   c->config.session.family=c->family;
   c->config.session.video.width=192;
   c->config.session.video.height=128;
   if (c->config.session.video.width<c->caps.minCodedExtent.width)
      c->config.session.video.width=c->caps.minCodedExtent.width;
   if (c->config.session.video.height<c->caps.minCodedExtent.height)
      c->config.session.video.height=c->caps.minCodedExtent.height;
   c->config.session.video.width=(c->config.session.video.width+c->caps.pictureAccessGranularity.width-1)
      /c->caps.pictureAccessGranularity.width*c->caps.pictureAccessGranularity.width;
   c->config.session.video.height=(c->config.session.video.height+c->caps.pictureAccessGranularity.height-1)
      /c->caps.pictureAccessGranularity.height*c->caps.pictureAccessGranularity.height;
   if (c->config.session.video.width>4096 || c->config.session.video.height>4096
         || c->config.session.video.width>c->caps.maxCodedExtent.width
         || c->config.session.video.height>c->caps.maxCodedExtent.height) return false;
   c->config.session.video.fps_num=30;
   c->config.session.video.fps_den=1;
   c->config.session.video.bitrate=2000000;
   c->config.queue=c->queue;
   c->config.producer_family=c->family;
   c->config.slots=2;
   c->config.packet_size=1048576;
   c->config.gop=30; c->config.qp=22;
   c->config.storage_image_extended_formats=storage;
   printf("extent=%ux%u\n",c->config.session.video.width,c->config.session.video.height);
   printf("device=%s vendor=%u driver=%u api=%u family=%u\n",c->properties.deviceName,
         c->properties.vendorID,c->properties.driverVersion,c->properties.apiVersion,c->family);
   return true;
}
int main(void)
{
   struct native_encoder *c=(struct native_encoder*)calloc(1,sizeof(*c));
   vulkan_encode_h264_t *encoder;
   vulkan_encode_packet_t packet;
   size_t bytes;
   unsigned cycle;
   bool storage;
   const vulkan_encode_pool_t *pool;
   int result=77;
   assert(c);
   if (!startup(c))
      puts("UNAVAILABLE Vulkan validation layer or Vulkan 1.3 H.264 encode device/features");
   else
   {
      storage=c->config.storage_image_extended_formats;
      for (cycle=0;cycle<3;cycle++)
      {
         c->config.slots=cycle==0?2:cycle==1?4:16;
         c->config.storage_image_extended_formats=cycle==1?false:storage;
         encoder=vulkan_encode_h264_open(&c->config,&c->ops);
         assert(encoder);
         pool=vulkan_encode_h264_pool(encoder);
         assert(pool && pool->count==c->config.slots);
         if (!c->config.storage_image_extended_formats) assert(!pool->storage_images);
         printf("pool slots=%u storage_images=%u\n",pool->count,pool->storage_images?1u:0u);
         bytes=0;
         assert(vulkan_encode_h264_headers(encoder,c->headers,sizeof(c->headers),&bytes));
         assert(bytes && bytes<=sizeof(c->headers));
         assert(vulkan_encode_h264_finish(encoder)==VULKAN_ENCODE_OK);
         assert(vulkan_encode_h264_poll(encoder,&packet)==VULKAN_ENCODE_END);
         assert(vulkan_encode_h264_free(encoder));
         printf("PASS native cold H264 session/pool/header/empty drain cycle=%u headers=%lu\n",
               cycle,(unsigned long)bytes);
      }
      result=0;
   }
   if (c->device) c->destroy_device(c->device,NULL);
   if (c->messenger) c->destroy_messenger(c->instance,c->messenger,NULL);
   if (c->instance) c->destroy_instance(c->instance,NULL);
   if (c->validation_errors) result=1;
   printf("validation_errors=%u\n",c->validation_errors);
   if (c->library) dylib_close(c->library);
   free(c);
   return result;
}
