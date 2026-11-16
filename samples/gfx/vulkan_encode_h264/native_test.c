/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error Native acceptance requires assertions
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <dynamic/dylib.h>
#include "../../../gfx/common/vulkan_encode_h264.h"
#include "../../../gfx/common/vulkan_encode_capture.h"
#include "../../../record/record_gpu_session_vulkan.h"
#include "../../../record/record_gpu_renderer_vulkan.h"
#include "../../../gfx/common/vulkan_encode_device.h"

struct native_capture_command
{
   VkCommandBufferBeginInfo begin;
   VkImageMemoryBarrier2 image;
   VkDependencyInfo dependency;
   VkClearColorValue color;
   VkTimelineSemaphoreSubmitInfo timeline;
   VkSubmitInfo submit;
   VkPipelineStageFlags wait_stage;
   vulkan_encode_capture_source_t source;
   uint64_t ready,done,binary_value;
};

struct native_encoder
{
   dylib_t library;
   VkInstance instance;
   VkPhysicalDevice physical;
   VkDevice device;
   VkQueue queue,producer_queue;
   unsigned producer_family;
   VkDebugUtilsMessengerEXT messenger;
   unsigned validation_errors;
   PFN_vkDestroyDebugUtilsMessengerEXT destroy_messenger;
   unsigned family;
   unsigned encode_queue_index;
   uint32_t api_version;
   VkQueueFlags queue_flags;
   VkCommandPool producer_commands;
   VkCommandBuffer producer[2];
   uint64_t producer_completion[2];
   VkBuffer upload;
   VkDeviceMemory upload_memory;
   void *upload_data;
   PFN_vkGetInstanceProcAddr instance_proc;
   PFN_vkGetDeviceProcAddr device_proc;
   PFN_vkDestroyInstance destroy_instance;
   PFN_vkDestroyDevice destroy_device;
   VkPhysicalDeviceProperties properties;
   VkPhysicalDeviceMemoryProperties memory;
   VkPhysicalDeviceFeatures2 features;
   VkPhysicalDeviceVulkan12Features version12;
   VkPhysicalDeviceVulkan13Features version13;
   VkVideoEncodeH264ProfileInfoKHR codec_profile;
   VkVideoProfileInfoKHR profile;
   VkVideoCapabilitiesKHR caps;
   VkVideoEncodeCapabilitiesKHR encode_caps;
   VkVideoEncodeH264CapabilitiesKHR codec_caps;
   struct native_capture_command capture_command;
   vulkan_encode_capture_ops_t capture_ops;
   vulkan_encode_capture_t *capture;
   record_gpu_renderer_vulkan_t *renderer;
   bool capture_borrowed;
   VkImage rgb;
   VkImageView rgb_view;
   VkDeviceMemory rgb_memory;
   VkSemaphore capture_signals[2];
   char shader_error[512];
   vulkan_encode_h264_ops_t ops;
   vulkan_encode_packet_t packets[2];
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
#define DEVICE_SYNC(member) do { \
   c->member=(PFN_vkCmdPipelineBarrier2)c->device_proc(c->device, \
         c->api_version<VK_API_VERSION_1_3?"vkCmdPipelineBarrier2KHR":"vkCmdPipelineBarrier2"); \
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
   DEVICE_SYNC(ops.submit.barrier);
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
static void load_capture_ops(struct native_encoder *c)
{
   DEVICE(capture_ops.create_set_layout, CreateDescriptorSetLayout);
   DEVICE(capture_ops.destroy_set_layout, DestroyDescriptorSetLayout);
   DEVICE(capture_ops.create_descriptor_pool, CreateDescriptorPool);
   DEVICE(capture_ops.destroy_descriptor_pool, DestroyDescriptorPool);
   DEVICE(capture_ops.allocate_sets, AllocateDescriptorSets);
   DEVICE(capture_ops.update_sets, UpdateDescriptorSets);
   DEVICE(capture_ops.create_sampler, CreateSampler);
   DEVICE(capture_ops.destroy_sampler, DestroySampler);
   DEVICE(capture_ops.create_pipeline_layout, CreatePipelineLayout);
   DEVICE(capture_ops.destroy_pipeline_layout, DestroyPipelineLayout);
   DEVICE(capture_ops.create_shader, CreateShaderModule);
   DEVICE(capture_ops.destroy_shader, DestroyShaderModule);
   DEVICE(capture_ops.create_pipelines, CreateComputePipelines);
   DEVICE(capture_ops.destroy_pipeline, DestroyPipeline);
   DEVICE_SYNC(capture_ops.barrier);
   DEVICE(capture_ops.bind_pipeline, CmdBindPipeline);
   DEVICE(capture_ops.bind_sets, CmdBindDescriptorSets);
   DEVICE(capture_ops.push, CmdPushConstants);
   DEVICE(capture_ops.dispatch, CmdDispatch);
   DEVICE(capture_ops.copy, CmdCopyBufferToImage);
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
            c->physical=devices[i]; c->family=j;
            c->queue_flags=queues[j].queueFamilyProperties.queueFlags;
            found=true; break;
         }
      c->producer_family=c->family;
      if (found)
         for (j=0;j<n;j++)
            if (queues[j].queueFamilyProperties.queueCount
                  && (queues[j].queueFamilyProperties.queueFlags&VK_QUEUE_COMPUTE_BIT)
                  && (queues[j].queueFamilyProperties.queueFlags&VK_QUEUE_TRANSFER_BIT))
            { c->producer_family=j; break; }
      free(video); free(queues);
   }
   free(devices);
   return found;
}
static bool startup(struct native_encoder *c,bool capture,bool planned)
{
   VkApplicationInfo application;
   VkDebugUtilsMessengerCreateInfoEXT debug;
   VkValidationFeaturesEXT validation;
   VkValidationFeatureEnableEXT synchronization=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
   PFN_vkCreateDebugUtilsMessengerEXT create_messenger;
   const char *layer="VK_LAYER_KHRONOS_validation";
   const char *debug_extensions[]={VK_EXT_DEBUG_UTILS_EXTENSION_NAME,VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME};
   VkInstanceCreateInfo instance;
   VkDeviceQueueCreateInfo queue[2];
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
   c->api_version=planned?VK_API_VERSION_1_1:VK_API_VERSION_1_3;
   application.apiVersion=c->api_version;
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
   memset(&validation,0,sizeof(validation));
   validation.sType=VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
   validation.pNext=&debug;
   validation.enabledValidationFeatureCount=1;
   validation.pEnabledValidationFeatures=&synchronization;
   instance.pNext=&validation;
   instance.enabledLayerCount=1;
   instance.ppEnabledLayerNames=&layer;
   instance.enabledExtensionCount=2;
   instance.ppEnabledExtensionNames=debug_extensions;
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
   c->features.pNext=planned?NULL:&c->version12;
   c->version12.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
   c->version12.pNext=&c->version13;
   c->version13.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
   features(c->physical,&c->features);
   if (!planned && (!c->version12.timelineSemaphore || !c->version13.synchronization2)) return false;
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
   if (!capture) c->producer_family=c->family;
   if (capture && c->producer_family==c->family && !(c->queue_flags&VK_QUEUE_COMPUTE_BIT))
      return false;
   memset(queue,0,sizeof(queue));
   queue[0].sType=queue[1].sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   queue[0].queueFamilyIndex=c->family;
   queue[1].queueFamilyIndex=c->producer_family;
   queue[0].queueCount=queue[1].queueCount=1;
   queue[0].pQueuePriorities=queue[1].pQueuePriorities=&priority;
   memset(&device,0,sizeof(device));
   device.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   device.pNext=&c->version12;
   device.pEnabledFeatures=&enabled;
   device.queueCreateInfoCount=c->family==c->producer_family?1:2;
   device.pQueueCreateInfos=queue;
   device.enabledExtensionCount=3;
   device.ppEnabledExtensionNames=extensions;
   create_device=(PFN_vkCreateDevice)c->instance_proc(c->instance,"vkCreateDevice");
   assert(create_device);
   if (planned)
   {
      vulkan_encode_device_ops_t ops;
      vulkan_encode_device_plan_t *plan;
      bool plan_storage;
      device.pNext=NULL;
      device.queueCreateInfoCount=1;
      queue[0].queueFamilyIndex=c->producer_family;
      device.enabledExtensionCount=0;
      ops.features=features;
      ops.families=(PFN_vkGetPhysicalDeviceQueueFamilyProperties2)c->instance_proc(c->instance,"vkGetPhysicalDeviceQueueFamilyProperties2");
      ops.extensions=(PFN_vkEnumerateDeviceExtensionProperties)c->instance_proc(c->instance,"vkEnumerateDeviceExtensionProperties");
      plan=vulkan_encode_device_plan_new(c->physical,c->api_version,c->producer_family,&device,&ops);
      if (!plan) return false;
      assert(vulkan_encode_device_plan_queue(plan,&c->family,&c->encode_queue_index,&plan_storage));
      assert(plan_storage==storage);
      assert(create_device(c->physical,vulkan_encode_device_plan_info(plan),NULL,&c->device)==VK_SUCCESS);
      printf("device_plan=1 instance_api=%u family=%u queue_index=%u extensions=%u\n",
            c->api_version,c->family,c->encode_queue_index,vulkan_encode_device_plan_info(plan)->enabledExtensionCount);
      vulkan_encode_device_plan_free(plan);
   }
   else assert(create_device(c->physical,&device,NULL,&c->device)==VK_SUCCESS);
   INSTANCE(device_proc,GetDeviceProcAddr);
   DEVICE(destroy_device,DestroyDevice);
   get_queue=(PFN_vkGetDeviceQueue)c->device_proc(c->device,"vkGetDeviceQueue");
   assert(get_queue);
   get_queue(c->device,c->family,c->encode_queue_index,&c->queue);
   get_queue(c->device,c->producer_family,0,&c->producer_queue);
   load_ops(c);
   if (capture) load_capture_ops(c);
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
   c->config.producer_family=c->producer_family;
   c->config.slots=2;
   c->config.packet_size=1048576;
   c->config.gop=30; c->config.qp=22;
   c->config.storage_image_extended_formats=storage;
   printf("extent=%ux%u\n",c->config.session.video.width,c->config.session.video.height);
   printf("device=%s vendor=%u driver=%u api=%u family=%u\n",c->properties.deviceName,
         c->properties.vendorID,c->properties.driverVersion,c->properties.apiVersion,c->family);
   return true;
}
static void upload_new(struct native_encoder *c,const vulkan_encode_pool_t *pool)
{
   VkBufferCreateInfo buffer;
   VkMemoryRequirements requirements;
   VkMemoryAllocateInfo allocation;
   VkCommandPoolCreateInfo commands;
   VkCommandBufferAllocateInfo command;
   unsigned i;
   assert(c->queue_flags&VK_QUEUE_TRANSFER_BIT);
   memset(&buffer,0,sizeof(buffer));
   buffer.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   buffer.size=(VkDeviceSize)pool->extent.width*pool->extent.height*3/2;
   buffer.usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
   assert(c->ops.pool.create_buffer(c->device,&buffer,NULL,&c->upload)==VK_SUCCESS);
   c->ops.pool.buffer_memory(c->device,c->upload,&requirements);
   c->ops.pool.memory_properties(c->physical,&c->memory);
   for (i=0;i<c->memory.memoryTypeCount;i++)
      if ((requirements.memoryTypeBits&(1u<<i))
            && (c->memory.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                  |VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
               ==(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) break;
   assert(i<c->memory.memoryTypeCount);
   memset(&allocation,0,sizeof(allocation));
   allocation.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocation.allocationSize=requirements.size;
   allocation.memoryTypeIndex=i;
   assert(c->ops.pool.allocate(c->device,&allocation,NULL,&c->upload_memory)==VK_SUCCESS);
   assert(c->ops.pool.bind_buffer(c->device,c->upload,c->upload_memory,0)==VK_SUCCESS);
   assert(c->ops.pool.map(c->device,c->upload_memory,0,buffer.size,0,&c->upload_data)==VK_SUCCESS);
   memset(&commands,0,sizeof(commands));
   commands.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   commands.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   commands.queueFamilyIndex=c->producer_family;
   assert(c->ops.pool.create_commands(c->device,&commands,NULL,&c->producer_commands)==VK_SUCCESS);
   memset(&command,0,sizeof(command));
   command.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   command.commandPool=c->producer_commands;
   command.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   command.commandBufferCount=2;
   assert(c->ops.pool.allocate_command(c->device,&command,c->producer)==VK_SUCCESS);
}
static void upload_frame(struct native_encoder *c,const vulkan_encode_pool_t *pool,
      unsigned frame)
{
   VkCommandBufferBeginInfo begin;
   VkImageMemoryBarrier2 image;
   VkBufferMemoryBarrier2 buffer;
   VkDependencyInfo dependency;
   VkBufferImageCopy regions[2];
   VkTimelineSemaphoreSubmitInfo timeline;
   VkSubmitInfo submit;
   PFN_vkCmdCopyBufferToImage copy;
   VkCommandBuffer command=c->producer[frame%2];
   uint64_t ready=frame+1;
   unsigned i;
   size_t luma=(size_t)pool->extent.width*pool->extent.height;
   memset(c->upload_data,32+frame*12,luma);
   memset((uint8_t*)c->upload_data+luma,128,luma/2);
   copy=(PFN_vkCmdCopyBufferToImage)c->device_proc(c->device,"vkCmdCopyBufferToImage");
   assert(copy && c->ops.submit.reset_command(command,0)==VK_SUCCESS);
   memset(&begin,0,sizeof(begin));
   begin.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   assert(c->ops.submit.begin(command,&begin)==VK_SUCCESS);
   memset(&image,0,sizeof(image));
   image.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
   image.srcStageMask=frame<2?VK_PIPELINE_STAGE_2_NONE:VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR;
   image.srcAccessMask=frame<2?0:VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR;
   image.dstStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   image.dstAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;
   image.oldLayout=frame<2?VK_IMAGE_LAYOUT_UNDEFINED:VK_IMAGE_LAYOUT_GENERAL;
   image.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   image.srcQueueFamilyIndex=image.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
   image.image=pool->slot[frame%2].image;
   image.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT;
   image.subresourceRange.levelCount=image.subresourceRange.layerCount=1;
   memset(&buffer,0,sizeof(buffer));
   buffer.sType=VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
   buffer.srcStageMask=VK_PIPELINE_STAGE_2_HOST_BIT;
   buffer.srcAccessMask=VK_ACCESS_2_HOST_WRITE_BIT;
   buffer.dstStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   buffer.dstAccessMask=VK_ACCESS_2_TRANSFER_READ_BIT;
   buffer.srcQueueFamilyIndex=buffer.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
   buffer.buffer=c->upload;
   buffer.size=luma*3/2;
   memset(&dependency,0,sizeof(dependency));
   dependency.sType=VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
   dependency.imageMemoryBarrierCount=dependency.bufferMemoryBarrierCount=1;
   dependency.pImageMemoryBarriers=&image;
   dependency.pBufferMemoryBarriers=&buffer;
   c->ops.submit.barrier(command,&dependency);
   memset(regions,0,sizeof(regions));
   for (i=0;i<2;i++)
   {
      regions[i].bufferOffset=i?luma:0;
      regions[i].imageSubresource.aspectMask=i?VK_IMAGE_ASPECT_PLANE_1_BIT:VK_IMAGE_ASPECT_PLANE_0_BIT;
      regions[i].imageSubresource.layerCount=1;
      regions[i].imageExtent.width=pool->extent.width>>i;
      regions[i].imageExtent.height=pool->extent.height>>i;
      regions[i].imageExtent.depth=1;
   }
   copy(command,c->upload,pool->slot[frame%2].image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,2,regions);
   image.srcStageMask=VK_PIPELINE_STAGE_2_TRANSFER_BIT;
   image.srcAccessMask=VK_ACCESS_2_TRANSFER_WRITE_BIT;
   image.dstStageMask=VK_PIPELINE_STAGE_2_VIDEO_ENCODE_BIT_KHR;
   image.dstAccessMask=VK_ACCESS_2_VIDEO_ENCODE_READ_BIT_KHR;
   image.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
   image.newLayout=VK_IMAGE_LAYOUT_GENERAL;
   dependency.bufferMemoryBarrierCount=0;
   dependency.pBufferMemoryBarriers=NULL;
   c->ops.submit.barrier(command,&dependency);
   assert(c->ops.submit.end(command)==VK_SUCCESS);
   memset(&timeline,0,sizeof(timeline));
   timeline.sType=VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
   timeline.signalSemaphoreValueCount=1;
   timeline.pSignalSemaphoreValues=&ready;
   memset(&submit,0,sizeof(submit));
   submit.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.pNext=&timeline;
   submit.commandBufferCount=submit.signalSemaphoreCount=1;
   submit.pCommandBuffers=&command;
   submit.pSignalSemaphores=&pool->ready;
   assert(c->ops.submit.submit(c->queue,1,&submit,VK_NULL_HANDLE)==VK_SUCCESS);
}
static void native_record_pause(void);
#include "native_capture.h"
static int encode_pictures(struct native_encoder *c,const char *path,unsigned capture_mode)
{
   vulkan_encode_h264_t *encoder;
   const vulkan_encode_pool_t *pool;
   vulkan_encode_packet_t *packet=&c->packets[0],*again=&c->packets[1];
   PFN_vkWaitForFences wait_fence;
   size_t bytes=0;
   unsigned frame, first, batch = capture_mode ? 2 : 1;
   FILE *file=fopen(path,"wb");
   assert(file);
   c->config.slots=2;
   c->config.gop=4;
   c->config.storage_image_extended_formats=capture_mode==1
      && c->features.features.shaderStorageImageExtendedFormats;
   encoder=vulkan_encode_h264_open(&c->config,&c->ops);
   assert(encoder);
   pool=vulkan_encode_h264_pool(encoder);
   assert(pool);
   if (capture_mode==1 && !pool->storage_images)
   {
      assert(vulkan_encode_h264_free(encoder) && !fclose(file));
      puts("UNAVAILABLE direct storage capture format");
      return 77;
   }
   if (capture_mode!=1) assert(!pool->storage_images);
   assert(vulkan_encode_h264_headers(encoder,c->headers,sizeof(c->headers),&bytes));
   assert(fwrite(c->headers,1,bytes,file)==bytes);
   if (capture_mode) capture_new(c,pool);
   else upload_new(c,pool);
   wait_fence=(PFN_vkWaitForFences)c->device_proc(c->device,"vkWaitForFences");
   assert(wait_fence);
   for (first=0;first<12;first+=batch)
   {
      for (frame=first;frame<first+batch;frame++)
      {
         if (capture_mode) capture_frame(c,pool,frame);
         else upload_frame(c,pool,frame);
         assert(vulkan_encode_h264_submit(encoder,frame%2,frame,1,frame+1,false)==VULKAN_ENCODE_OK);
      }
      if (capture_mode)
         assert(vulkan_encode_h264_submit(encoder,0,first+2,1,first+3,false)==VULKAN_ENCODE_BUSY);
      for (frame=first;frame<first+batch;frame++)
      {
         /* Test orchestration only; the shipping encoder polls without waits. */
         assert(wait_fence(c->device,1,&pool->slot[frame%2].fence,VK_TRUE,2000000000)==VK_SUCCESS);
         assert(vulkan_encode_h264_poll(encoder,packet)==VULKAN_ENCODE_OK);
         assert(packet->data && packet->size && packet->timestamp==frame && packet->duration==1);
         assert(packet->keyframe==(frame%4==0));
         assert(vulkan_encode_h264_poll(encoder,again)==VULKAN_ENCODE_OK);
         assert(packet->data==again->data && packet->size==again->size && packet->slot==again->slot);
         assert(fwrite(packet->data,1,packet->size,file)==packet->size);
         assert(vulkan_encode_h264_release(encoder));
      }
   }
   if (capture_mode)
   {
      for (frame=12;frame<14;frame++)
      {
         capture_frame(c,pool,frame);
         assert(vulkan_encode_h264_submit(encoder,frame%2,frame,1,frame+1,false)==VULKAN_ENCODE_OK);
      }
      assert(wait_fence(c->device,1,&pool->slot[0].fence,VK_TRUE,2000000000)==VK_SUCCESS);
      assert(vulkan_encode_h264_poll(encoder,packet)==VULKAN_ENCODE_OK);
      assert(packet->timestamp==12 && packet->data && packet->size);
      bytes=packet->size<sizeof(c->headers)?packet->size:sizeof(c->headers);
      memcpy(c->headers,packet->data,bytes);
      assert(!vulkan_encode_h264_free(encoder));
      assert(!vulkan_encode_h264_drain(encoder));
      assert(!memcmp(c->headers,packet->data,bytes));
      assert(vulkan_encode_h264_submit(encoder,0,14,1,15,false)==VULKAN_ENCODE_ERROR);
      assert(vulkan_encode_h264_release(encoder));
      /* Producer commands precede encoder completion on the ready timeline. */
      assert(wait_fence(c->device,1,&pool->slot[1].fence,VK_TRUE,2000000000)==VK_SUCCESS);
      assert(vulkan_encode_h264_drain(encoder));
      puts("PASS native two in-flight pictures, full-ring BUSY, borrowed cancellation and drain");
   }
   else
   {
      assert(vulkan_encode_h264_finish(encoder)==VULKAN_ENCODE_OK);
      assert(vulkan_encode_h264_poll(encoder,packet)==VULKAN_ENCODE_END);
   }
   c->ops.pool.destroy_commands(c->device,c->producer_commands,NULL);
   if (capture_mode) capture_free(c);
   else
   {
      c->ops.pool.unmap(c->device,c->upload_memory);
      c->ops.pool.destroy_buffer(c->device,c->upload,NULL);
      c->ops.pool.free_memory(c->device,c->upload_memory,NULL);
   }
   assert(vulkan_encode_h264_free(encoder));
   assert(!fclose(file));
   printf("PASS native coded pictures=12 GOP=4 stable output borrows capture=%u\n",capture_mode);
   return 0;
}
#include "native_session.h"
int main(int argc,char **argv)
{
   struct native_encoder *c=(struct native_encoder*)calloc(1,sizeof(*c));
   vulkan_encode_h264_t *encoder;
   vulkan_encode_packet_t *packet;
   size_t bytes;
   unsigned cycle;
   bool storage;
   const vulkan_encode_pool_t *pool;
   int result=77;
   assert(c && (argc==1 || (argc==3 && (!strcmp(argv[1],"--pictures")
               || !strcmp(argv[1],"--capture-direct") || !strcmp(argv[1],"--capture-buffer")
               || !strcmp(argv[1],"--session-direct") || !strcmp(argv[1],"--session-buffer")
               || !strcmp(argv[1],"--dispatch-direct") || !strcmp(argv[1],"--dispatch-buffer")
               || !strcmp(argv[1],"--plan-direct") || !strcmp(argv[1],"--plan-buffer")))));
   packet=&c->packets[0];
   if (!startup(c,argc==3 && strcmp(argv[1],"--pictures"),argc==3 && !strncmp(argv[1],"--plan-",7)))
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
         assert(vulkan_encode_h264_poll(encoder,packet)==VULKAN_ENCODE_END);
         assert(vulkan_encode_h264_free(encoder));
         printf("PASS native cold H264 session/pool/header/empty drain cycle=%u headers=%lu\n",
               cycle,(unsigned long)bytes);
      }
      if (argc==3 && !strcmp(argv[1],"--pictures") && !(c->queue_flags&VK_QUEUE_TRANSFER_BIT))
         puts("UNAVAILABLE test upload on the selected encode queue");
      else
      {
         result=argc==3 && (!strncmp(argv[1],"--session-",10) || !strncmp(argv[1],"--dispatch-",11) || !strncmp(argv[1],"--plan-",7))
            ? session_pictures(c,argv[2],!strcmp(argv[1],"--session-direct")
                  || !strcmp(argv[1],"--dispatch-direct") || !strcmp(argv[1],"--plan-direct"),
                  !strncmp(argv[1],"--dispatch-",11) || !strncmp(argv[1],"--plan-",7))
            :argc==3?encode_pictures(c,argv[2],!strcmp(argv[1],"--capture-direct")
               ?1:!strcmp(argv[1],"--capture-buffer")?2:0):0;
      }
   }
   if (c->device) c->destroy_device(c->device,NULL);
   if (c->messenger) c->destroy_messenger(c->instance,c->messenger,NULL);
   if (c->instance) c->destroy_instance(c->instance,NULL);
   if (c->validation_errors) result=1;
   printf("validation_sync=1\nvalidation_errors=%u\n",c->validation_errors);
   if (c->library) dylib_close(c->library);
   free(c);
   return result;
}
