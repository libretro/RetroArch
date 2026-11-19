#ifndef RPYROWAVE_VULKAN_TEST_CONTEXT_H
#define RPYROWAVE_VULKAN_TEST_CONTEXT_H
/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: MIT
 * Fence waits and raw readback are confined to this offline executable.
 */
#define _CRT_SECURE_NO_WARNINGS
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include "gpu_vectors.h"

#define RPY_VK_INSTANCE_FUNCTIONS(X) \
   X(vkDestroyInstance) X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceProperties) \
   X(vkGetPhysicalDeviceMemoryProperties) X(vkGetPhysicalDeviceQueueFamilyProperties) \
   X(vkCreateDevice) X(vkGetDeviceProcAddr)
#define RPY_VK_DEVICE_FUNCTIONS(X) \
   X(vkDestroyDevice) X(vkGetDeviceQueue) X(vkCreateBuffer) X(vkDestroyBuffer) \
   X(vkGetBufferMemoryRequirements) X(vkAllocateMemory) X(vkFreeMemory) \
   X(vkBindBufferMemory) X(vkMapMemory) X(vkUnmapMemory) \
   X(vkCreateDescriptorSetLayout) X(vkDestroyDescriptorSetLayout) \
   X(vkCreateDescriptorPool) X(vkDestroyDescriptorPool) X(vkAllocateDescriptorSets) \
   X(vkUpdateDescriptorSets) X(vkCreatePipelineLayout) X(vkDestroyPipelineLayout) \
   X(vkCreateShaderModule) X(vkDestroyShaderModule) X(vkCreateComputePipelines) \
   X(vkDestroyPipeline) X(vkCreateCommandPool) X(vkDestroyCommandPool) \
   X(vkAllocateCommandBuffers) X(vkBeginCommandBuffer) X(vkEndCommandBuffer) \
   X(vkResetCommandBuffer) X(vkCmdBindPipeline) X(vkCmdBindDescriptorSets) \
   X(vkCmdDispatch) X(vkCmdPipelineBarrier) X(vkCmdCopyBuffer) \
   X(vkCreateFence) X(vkDestroyFence) X(vkResetFences) X(vkQueueSubmit) X(vkWaitForFences)

typedef struct rpy_vk_buffer
{
   VkBuffer buffer;
   VkDeviceMemory memory;
   void *mapped;
} rpy_vk_buffer_t;

typedef struct rpy_vk_test
{
#define RPY_VK_DECLARE(name) PFN_##name name;
   RPY_VK_INSTANCE_FUNCTIONS(RPY_VK_DECLARE)
   RPY_VK_DEVICE_FUNCTIONS(RPY_VK_DECLARE)
#undef RPY_VK_DECLARE
   PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
   PFN_vkCreateInstance vkCreateInstance;
   PFN_vkEnumerateInstanceLayerProperties vkEnumerateInstanceLayerProperties;
   PFN_vkEnumerateInstanceExtensionProperties vkEnumerateInstanceExtensionProperties;
   PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
   PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
   VkPhysicalDeviceProperties properties;
   VkPhysicalDeviceMemoryProperties memory_properties;
   VkInstance instance;
   VkPhysicalDevice physical;
   VkDevice device;
   VkQueue queue;
   VkDebugUtilsMessengerEXT debug;
   VkDescriptorSetLayout set_layout;
   VkDescriptorPool descriptor_pool;
   VkDescriptorSet sets[2];
   VkPipelineLayout pipeline_layout;
   VkShaderModule shader;
   VkPipeline pipeline;
   VkCommandPool command_pool;
   VkCommandBuffer command;
   VkFence fence;
   rpy_vk_buffer_t atlas[2];
   rpy_vk_buffer_t upload;
   rpy_vk_buffer_t readback;
   rpy_vk_buffer_t uniform;
   size_t parameter_stride;
   unsigned cursor;
   unsigned family;
   unsigned errors;
#if defined(_WIN32)
   HMODULE library;
#else
   void *library;
#endif
} rpy_vk_test_t;

static VKAPI_ATTR VkBool32 VKAPI_CALL rpy_vk_message(
      VkDebugUtilsMessageSeverityFlagBitsEXT severity,
      VkDebugUtilsMessageTypeFlagsEXT type,
      const VkDebugUtilsMessengerCallbackDataEXT *message, void *userdata)
{
   rpy_vk_test_t *test = (rpy_vk_test_t*)userdata;
   (void)type;
   if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
   {
      test->errors++;
      fprintf(stderr, "Vulkan validation [%s]: %s\n",
            message->pMessageIdName ? message->pMessageIdName : "unknown", message->pMessage);
   }
   return VK_FALSE;
}

static int rpy_vk_layer(rpy_vk_test_t *test)
{
   VkLayerProperties *properties;
   uint32_t count = 0;
   uint32_t i;
   int found = 0;
   if (test->vkEnumerateInstanceLayerProperties(&count, NULL) != VK_SUCCESS || !count)
      return 0;
   properties = (VkLayerProperties*)malloc(count * sizeof(*properties));
   if (!properties)
      return 0;
   if (test->vkEnumerateInstanceLayerProperties(&count, properties) == VK_SUCCESS)
      for (i = 0; i < count; i++)
         if (!strcmp(properties[i].layerName, "VK_LAYER_KHRONOS_validation"))
            found = 1;
   free(properties);
   return found;
}

static int rpy_vk_extension(rpy_vk_test_t *test, const char *layer, const char *name)
{
   VkExtensionProperties *properties;
   uint32_t count = 0;
   uint32_t i;
   int found = 0;
   if (test->vkEnumerateInstanceExtensionProperties(layer, &count, NULL) != VK_SUCCESS || !count)
      return 0;
   properties = (VkExtensionProperties*)malloc(count * sizeof(*properties));
   if (!properties)
      return 0;
   if (test->vkEnumerateInstanceExtensionProperties(layer, &count, properties) == VK_SUCCESS)
      for (i = 0; i < count; i++)
         if (!strcmp(properties[i].extensionName, name))
            found = 1;
   free(properties);
   return found;
}

static int rpy_vk_initialize(rpy_vk_test_t *test)
{
   VkApplicationInfo application;
   VkInstanceCreateInfo instance;
   VkDebugUtilsMessengerCreateInfoEXT debug;
   VkValidationFeaturesEXT validation_features;
   VkValidationFeatureEnableEXT synchronization = VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
#ifdef VK_EXT_layer_settings
   VkLayerSettingsCreateInfoEXT settings;
   VkLayerSettingEXT shader_accesses;
   VkBool32 enabled = VK_TRUE;
#endif
   VkDeviceQueueCreateInfo queue;
   VkDeviceCreateInfo device;
   VkPhysicalDevice *physical = NULL;
   uint32_t count = 0;
   uint32_t i;
   int validation;
   int debug_extension;
   const char *layer = "VK_LAYER_KHRONOS_validation";
   const char *extensions[3];
   float priority = 1.0f;
#if defined(_WIN32)
   PROC symbol;
   test->library = LoadLibraryA("vulkan-1.dll");
   if (!test->library)
      return 0;
   symbol = GetProcAddress(test->library, "vkGetInstanceProcAddr");
#else
   void *symbol;
   test->library = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
   if (!test->library)
      return 0;
   symbol = dlsym(test->library, "vkGetInstanceProcAddr");
#endif
   if (!symbol)
      return 0;
   memcpy(&test->vkGetInstanceProcAddr, &symbol, sizeof(symbol));
#define RPY_VK_GLOBAL(name) do { \
   PFN_vkVoidFunction entry = test->vkGetInstanceProcAddr(VK_NULL_HANDLE, #name); \
   if (!entry) \
      return 0; \
   memcpy(&test->name, &entry, sizeof(entry)); \
} while (0)
   RPY_VK_GLOBAL(vkCreateInstance);
   RPY_VK_GLOBAL(vkEnumerateInstanceLayerProperties);
   RPY_VK_GLOBAL(vkEnumerateInstanceExtensionProperties);
#undef RPY_VK_GLOBAL
   validation = rpy_vk_layer(test);
   debug_extension = rpy_vk_extension(test, NULL, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
   printf("Vulkan validation layer: %s\n", validation ? "enabled" : "unavailable");
   if (getenv("RPY_REQUIRE_VALIDATION") && (!validation || !debug_extension))
      return 0;
   debug.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
   debug.pNext = NULL;
   debug.flags = 0;
   debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
   debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
      | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
   debug.pfnUserCallback = rpy_vk_message;
   debug.pUserData = test;
   application.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   application.pNext = NULL;
   application.pApplicationName = "rpyrowave-compute-test";
   application.applicationVersion = 1;
   application.pEngineName = NULL;
   application.engineVersion = 0;
   application.apiVersion = VK_API_VERSION_1_1;
   instance.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   instance.pNext = debug_extension ? &debug : NULL;
   instance.flags = 0;
   instance.pApplicationInfo = &application;
   instance.enabledLayerCount = validation ? 1 : 0;
   instance.ppEnabledLayerNames = validation ? &layer : NULL;
   extensions[0] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
   instance.enabledExtensionCount = debug_extension ? 1 : 0;
   instance.ppEnabledExtensionNames = extensions;
   if (getenv("RPY_VALIDATE_SYNC"))
   {
      if (!validation) return 0;
      memset(&validation_features, 0, sizeof(validation_features));
      validation_features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
      validation_features.pNext = instance.pNext;
      validation_features.enabledValidationFeatureCount = 1;
      validation_features.pEnabledValidationFeatures = &synchronization;
      instance.pNext = &validation_features;
      extensions[instance.enabledExtensionCount++] = VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME;
#ifdef VK_EXT_layer_settings
      if (getenv("RPY_VALIDATE_SHADER_ACCESSES")
            && rpy_vk_extension(test, layer, VK_EXT_LAYER_SETTINGS_EXTENSION_NAME))
      {
         memset(&settings, 0, sizeof(settings));
         shader_accesses.pLayerName = layer;
         shader_accesses.pSettingName = "syncval_shader_accesses_heuristic";
         shader_accesses.type = VK_LAYER_SETTING_TYPE_BOOL32_EXT;
         shader_accesses.valueCount = 1;
         shader_accesses.pValues = &enabled;
         settings.sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT;
         settings.pNext = instance.pNext;
         settings.settingCount = 1;
         settings.pSettings = &shader_accesses;
         instance.pNext = &settings;
         extensions[instance.enabledExtensionCount++] = VK_EXT_LAYER_SETTINGS_EXTENSION_NAME;
      }
#endif
   }
   if (test->vkCreateInstance(&instance, NULL, &test->instance) != VK_SUCCESS)
      return 0;
#define RPY_VK_LOAD_INSTANCE(name) do { \
   PFN_vkVoidFunction entry = test->vkGetInstanceProcAddr(test->instance, #name); \
   if (!entry) \
      return 0; \
   memcpy(&test->name, &entry, sizeof(entry)); \
} while (0);
   RPY_VK_INSTANCE_FUNCTIONS(RPY_VK_LOAD_INSTANCE)
   if (debug_extension)
   {
      RPY_VK_LOAD_INSTANCE(vkCreateDebugUtilsMessengerEXT)
      RPY_VK_LOAD_INSTANCE(vkDestroyDebugUtilsMessengerEXT)
      if (test->vkCreateDebugUtilsMessengerEXT(test->instance, &debug, NULL, &test->debug) != VK_SUCCESS)
         return 0;
   }
#undef RPY_VK_LOAD_INSTANCE
   if (test->vkEnumeratePhysicalDevices(test->instance, &count, NULL) != VK_SUCCESS || !count)
      return 0;
   physical = (VkPhysicalDevice*)malloc(count * sizeof(*physical));
   if (!physical)
      return 0;
   if (test->vkEnumeratePhysicalDevices(test->instance, &count, physical) != VK_SUCCESS)
   {
      free(physical);
      return 0;
   }
   for (i = 0; i < count && !test->physical; i++)
   {
      VkQueueFamilyProperties *families;
      uint32_t family_count = 0;
      uint32_t family;
      test->vkGetPhysicalDeviceProperties(physical[i], &test->properties);
      if (test->properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
            && test->properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
            && !(getenv("RPY_ALLOW_SOFTWARE")
               && test->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU))
         continue;
      test->vkGetPhysicalDeviceQueueFamilyProperties(physical[i], &family_count, NULL);
      families = (VkQueueFamilyProperties*)malloc(family_count * sizeof(*families));
      if (!families)
         continue;
      test->vkGetPhysicalDeviceQueueFamilyProperties(physical[i], &family_count, families);
      for (family = 0; family < family_count; family++)
         if (families[family].queueFlags & VK_QUEUE_COMPUTE_BIT)
         {
            test->physical = physical[i];
            test->family = family;
            break;
         }
      free(families);
   }
   free(physical);
   if (!test->physical)
      return 0;
   printf("Vulkan %s: %s\n", test->properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU
         ? "software device" : "hardware", test->properties.deviceName);
   test->vkGetPhysicalDeviceMemoryProperties(test->physical, &test->memory_properties);
   queue.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
   queue.pNext = NULL;
   queue.flags = 0;
   queue.queueFamilyIndex = test->family;
   queue.queueCount = 1;
   queue.pQueuePriorities = &priority;
   device.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   device.pNext = NULL;
   device.flags = 0;
   device.queueCreateInfoCount = 1;
   device.pQueueCreateInfos = &queue;
   device.enabledLayerCount = 0;
   device.ppEnabledLayerNames = NULL;
   device.enabledExtensionCount = 0;
   device.ppEnabledExtensionNames = NULL;
   device.pEnabledFeatures = NULL;
   if (test->vkCreateDevice(test->physical, &device, NULL, &test->device) != VK_SUCCESS)
      return 0;
#define RPY_VK_LOAD_DEVICE(name) do { \
   PFN_vkVoidFunction entry = test->vkGetDeviceProcAddr(test->device, #name); \
   if (!entry) \
      return 0; \
   memcpy(&test->name, &entry, sizeof(entry)); \
} while (0);
   RPY_VK_DEVICE_FUNCTIONS(RPY_VK_LOAD_DEVICE)
#undef RPY_VK_LOAD_DEVICE
   test->vkGetDeviceQueue(test->device, test->family, 0, &test->queue);
   return 1;
}

static int rpy_vk_buffer_create(rpy_vk_test_t *test, rpy_vk_buffer_t *buffer,
      size_t bytes, VkBufferUsageFlags usage, VkMemoryPropertyFlags flags)
{
   VkBufferCreateInfo info;
   VkMemoryRequirements requirements;
   VkMemoryAllocateInfo allocation;
   uint32_t i;
   info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   info.pNext = NULL;
   info.flags = 0;
   info.size = bytes;
   info.usage = usage;
   info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
   info.queueFamilyIndexCount = 0;
   info.pQueueFamilyIndices = NULL;
   if (test->vkCreateBuffer(test->device, &info, NULL, &buffer->buffer) != VK_SUCCESS)
      return 0;
   test->vkGetBufferMemoryRequirements(test->device, buffer->buffer, &requirements);
   for (i = 0; i < test->memory_properties.memoryTypeCount; i++)
      if ((requirements.memoryTypeBits & (1U << i))
            && (test->memory_properties.memoryTypes[i].propertyFlags & flags) == flags)
         break;
   if (i == test->memory_properties.memoryTypeCount)
      return 0;
   allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocation.pNext = NULL;
   allocation.allocationSize = requirements.size;
   allocation.memoryTypeIndex = i;
   if (test->vkAllocateMemory(test->device, &allocation, NULL, &buffer->memory) != VK_SUCCESS
         || test->vkBindBufferMemory(test->device, buffer->buffer, buffer->memory, 0) != VK_SUCCESS)
      return 0;
   if (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
      if (test->vkMapMemory(test->device, buffer->memory, 0, bytes, 0, &buffer->mapped) != VK_SUCCESS)
         return 0;
   return 1;
}

static void rpy_vk_buffer_free(rpy_vk_test_t *test, rpy_vk_buffer_t *buffer)
{
   if (buffer->mapped)
      test->vkUnmapMemory(test->device, buffer->memory);
   if (buffer->buffer)
      test->vkDestroyBuffer(test->device, buffer->buffer, NULL);
   if (buffer->memory)
      test->vkFreeMemory(test->device, buffer->memory, NULL);
}

static int rpy_vk_parameters(void *userdata, const rpyrowave_dwt_parameters_t *parameters,
      unsigned source, unsigned x, unsigned y, unsigned z)
{
   rpy_vk_test_t *test = (rpy_vk_test_t*)userdata;
   unsigned slot = (parameters->inverse ? 50 : 0) + test->cursor++;
   (void)source;
   (void)x;
   (void)y;
   (void)z;
   memcpy((uint8_t*)test->uniform.mapped + slot * test->parameter_stride,
         parameters, sizeof(*parameters));
   return 1;
}

static int rpy_vk_pipeline(rpy_vk_test_t *test, const uint8_t *shader, size_t shader_size)
{
   VkDescriptorSetLayoutBinding bindings[3];
   VkDescriptorSetLayoutCreateInfo layout;
   VkDescriptorPoolSize sizes[2];
   VkDescriptorPoolCreateInfo pool;
   VkDescriptorSetLayout layouts[2];
   VkDescriptorSetAllocateInfo allocate;
   VkPipelineLayoutCreateInfo pipeline_layout;
   VkShaderModuleCreateInfo module;
   VkComputePipelineCreateInfo pipeline;
   VkCommandPoolCreateInfo commands;
   VkCommandBufferAllocateInfo command;
   VkFenceCreateInfo fence;
   unsigned i;
   for (i = 0; i < 3; i++)
   {
      bindings[i].binding = i;
      bindings[i].descriptorType = i == 2 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
         : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
      bindings[i].descriptorCount = 1;
      bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
      bindings[i].pImmutableSamplers = NULL;
   }
   layout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   layout.pNext = NULL;
   layout.flags = 0;
   layout.bindingCount = 3;
   layout.pBindings = bindings;
   if (test->vkCreateDescriptorSetLayout(test->device, &layout, NULL, &test->set_layout) != VK_SUCCESS)
      return 0;
   sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
   sizes[0].descriptorCount = 4;
   sizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
   sizes[1].descriptorCount = 2;
   pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   pool.pNext = NULL;
   pool.flags = 0;
   pool.maxSets = 2;
   pool.poolSizeCount = 2;
   pool.pPoolSizes = sizes;
   if (test->vkCreateDescriptorPool(test->device, &pool, NULL, &test->descriptor_pool) != VK_SUCCESS)
      return 0;
   layouts[0] = layouts[1] = test->set_layout;
   allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   allocate.pNext = NULL;
   allocate.descriptorPool = test->descriptor_pool;
   allocate.descriptorSetCount = 2;
   allocate.pSetLayouts = layouts;
   if (test->vkAllocateDescriptorSets(test->device, &allocate, test->sets) != VK_SUCCESS)
      return 0;
   pipeline_layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   pipeline_layout.pNext = NULL;
   pipeline_layout.flags = 0;
   pipeline_layout.setLayoutCount = 1;
   pipeline_layout.pSetLayouts = &test->set_layout;
   pipeline_layout.pushConstantRangeCount = 0;
   pipeline_layout.pPushConstantRanges = NULL;
   if (test->vkCreatePipelineLayout(test->device, &pipeline_layout, NULL, &test->pipeline_layout) != VK_SUCCESS)
      return 0;
   module.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   module.pNext = NULL;
   module.flags = 0;
   module.codeSize = shader_size;
   module.pCode = (const uint32_t*)shader;
   if ((shader_size & 3) || test->vkCreateShaderModule(test->device, &module, NULL, &test->shader) != VK_SUCCESS)
      return 0;
   pipeline.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
   pipeline.pNext = NULL;
   pipeline.flags = 0;
   pipeline.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   pipeline.stage.pNext = NULL;
   pipeline.stage.flags = 0;
   pipeline.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
   pipeline.stage.module = test->shader;
   pipeline.stage.pName = "main";
   pipeline.stage.pSpecializationInfo = NULL;
   pipeline.layout = test->pipeline_layout;
   pipeline.basePipelineHandle = VK_NULL_HANDLE;
   pipeline.basePipelineIndex = -1;
   if (test->vkCreateComputePipelines(test->device, VK_NULL_HANDLE, 1, &pipeline, NULL, &test->pipeline) != VK_SUCCESS)
      return 0;
   commands.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   commands.pNext = NULL;
   commands.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   commands.queueFamilyIndex = test->family;
   if (test->vkCreateCommandPool(test->device, &commands, NULL, &test->command_pool) != VK_SUCCESS)
      return 0;
   command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   command.pNext = NULL;
   command.commandPool = test->command_pool;
   command.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   command.commandBufferCount = 1;
   if (test->vkAllocateCommandBuffers(test->device, &command, &test->command) != VK_SUCCESS)
      return 0;
   fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fence.pNext = NULL;
   fence.flags = 0;
   return test->vkCreateFence(test->device, &fence, NULL, &test->fence) == VK_SUCCESS;
}

static void rpy_vk_descriptors(rpy_vk_test_t *test, size_t bytes)
{
   unsigned set;
   for (set = 0; set < 2; set++)
   {
      unsigned i;
      VkDescriptorBufferInfo buffers[3];
      VkWriteDescriptorSet writes[3];
      for (i = 0; i < 3; i++)
      {
         buffers[i].buffer = i == 2 ? test->uniform.buffer : test->atlas[set ^ i].buffer;
         buffers[i].offset = 0;
         buffers[i].range = i == 2 ? sizeof(rpyrowave_dwt_parameters_t) : bytes;
         writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
         writes[i].pNext = NULL;
         writes[i].dstSet = test->sets[set];
         writes[i].dstBinding = i;
         writes[i].dstArrayElement = 0;
         writes[i].descriptorCount = 1;
         writes[i].descriptorType = i == 2 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
            : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
         writes[i].pImageInfo = NULL;
         writes[i].pBufferInfo = &buffers[i];
         writes[i].pTexelBufferView = NULL;
      }
      test->vkUpdateDescriptorSets(test->device, 3, writes, 0, NULL);
   }
}

static int rpy_vk_dispatch(void *userdata, const rpyrowave_dwt_parameters_t *parameters,
      unsigned source, unsigned x, unsigned y, unsigned z)
{
   rpy_vk_test_t *test = (rpy_vk_test_t*)userdata;
   unsigned slot = (parameters->inverse ? 50 : 0) + test->cursor++;
   uint32_t offset = (uint32_t)(slot * test->parameter_stride);
   VkMemoryBarrier barrier;
   test->vkCmdBindDescriptorSets(test->command, VK_PIPELINE_BIND_POINT_COMPUTE,
         test->pipeline_layout, 0, 1, &test->sets[source], 1, &offset);
   test->vkCmdDispatch(test->command, x, y, z);
   barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
   barrier.pNext = NULL;
   barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
   barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
   test->vkCmdPipelineBarrier(test->command, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, NULL, 0, NULL);
   return 1;
}

static void rpy_vk_buffer_barrier(rpy_vk_test_t *test, VkBuffer buffer,
      VkAccessFlags before, VkAccessFlags after, VkPipelineStageFlags src, VkPipelineStageFlags dst)
{
   VkBufferMemoryBarrier barrier;
   barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
   barrier.pNext = NULL;
   barrier.srcAccessMask = before;
   barrier.dstAccessMask = after;
   barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.buffer = buffer;
   barrier.offset = 0;
   barrier.size = (VkDeviceSize)-1;
   test->vkCmdPipelineBarrier(test->command, src, dst, 0, 0, NULL, 1, &barrier, 0, NULL);
}

static int rpy_vk_begin(rpy_vk_test_t *test)
{
   VkCommandBufferBeginInfo begin;
   begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin.pNext = NULL;
   begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   begin.pInheritanceInfo = NULL;
   if (test->vkBeginCommandBuffer(test->command, &begin) != VK_SUCCESS)
      return 0;
   test->vkCmdBindPipeline(test->command, VK_PIPELINE_BIND_POINT_COMPUTE, test->pipeline);
   return 1;
}

static int rpy_vk_submit(rpy_vk_test_t *test, size_t bytes)
{
   VkBufferCopy copy;
   VkSubmitInfo submit;
   copy.srcOffset = 0;
   copy.dstOffset = 0;
   copy.size = bytes;
   rpy_vk_buffer_barrier(test, test->atlas[0].buffer, VK_ACCESS_SHADER_WRITE_BIT,
         VK_ACCESS_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
   test->vkCmdCopyBuffer(test->command, test->atlas[0].buffer, test->readback.buffer, 1, &copy);
   rpy_vk_buffer_barrier(test, test->readback.buffer, VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_ACCESS_HOST_READ_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT);
   if (test->vkEndCommandBuffer(test->command) != VK_SUCCESS)
      return 0;
   submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.pNext = NULL;
   submit.waitSemaphoreCount = 0;
   submit.pWaitSemaphores = NULL;
   submit.pWaitDstStageMask = NULL;
   submit.commandBufferCount = 1;
   submit.pCommandBuffers = &test->command;
   submit.signalSemaphoreCount = 0;
   submit.pSignalSemaphores = NULL;
   return test->vkQueueSubmit(test->queue, 1, &submit, test->fence) == VK_SUCCESS
      && test->vkWaitForFences(test->device, 1, &test->fence, VK_TRUE,
            (uint64_t)3000000000U) == VK_SUCCESS;
}

static void rpy_vk_free(rpy_vk_test_t *test)
{
   if (!test)
      return;
   if (test->device && test->vkDestroyDevice)
   {
      rpy_vk_buffer_free(test, &test->uniform);
      rpy_vk_buffer_free(test, &test->readback);
      rpy_vk_buffer_free(test, &test->upload);
      rpy_vk_buffer_free(test, &test->atlas[0]);
      rpy_vk_buffer_free(test, &test->atlas[1]);
      if (test->fence) test->vkDestroyFence(test->device, test->fence, NULL);
      if (test->command_pool) test->vkDestroyCommandPool(test->device, test->command_pool, NULL);
      if (test->pipeline) test->vkDestroyPipeline(test->device, test->pipeline, NULL);
      if (test->shader) test->vkDestroyShaderModule(test->device, test->shader, NULL);
      if (test->pipeline_layout) test->vkDestroyPipelineLayout(test->device, test->pipeline_layout, NULL);
      if (test->descriptor_pool) test->vkDestroyDescriptorPool(test->device, test->descriptor_pool, NULL);
      if (test->set_layout) test->vkDestroyDescriptorSetLayout(test->device, test->set_layout, NULL);
      test->vkDestroyDevice(test->device, NULL);
   }
   if (test->debug && test->vkDestroyDebugUtilsMessengerEXT)
      test->vkDestroyDebugUtilsMessengerEXT(test->instance, test->debug, NULL);
   if (test->instance && test->vkDestroyInstance)
      test->vkDestroyInstance(test->instance, NULL);
   if (test->library)
#if defined(_WIN32)
      FreeLibrary(test->library);
#else
      dlclose(test->library);
#endif
   free(test);
}

#define RPY_VK_CHECK(expr) do { if (!(expr)) { \
   fprintf(stderr, "Vulkan failed line %u: %s\n", (unsigned)__LINE__, #expr); goto end; \
} } while (0)


#endif
