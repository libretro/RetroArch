#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <vulkan/vulkan_symbol_wrapper.h>
#include <libretro_vulkan.h>
#include <retro_miscellaneous.h>

static struct retro_hw_render_callback hw_render;
static const struct retro_hw_render_interface_vulkan *vulkan;
static unsigned frame_count;
static VkQueue async_queue;
static uint32_t async_queue_index;

#define BASE_WIDTH 640
#define BASE_HEIGHT 360
#define MAX_SYNC 8

struct buffer
{
   VkBuffer buffer;
   VkDeviceMemory memory;
};

struct vulkan_data
{
   unsigned index;
   unsigned num_swapchain_images;
   uint32_t swapchain_mask;

   VkPhysicalDeviceMemoryProperties memory_properties;
   VkPhysicalDeviceProperties gpu_properties;

   VkDescriptorSetLayout set_layout;
   VkDescriptorPool desc_pool;
   VkDescriptorSet desc_set[MAX_SYNC];

   VkPipelineCache pipeline_cache;
   VkPipelineLayout pipeline_layout;
   VkPipeline pipeline;

   struct retro_vulkan_image images[MAX_SYNC];
   VkDeviceMemory image_memory[MAX_SYNC];
   VkCommandPool cmd_pool[MAX_SYNC];
   VkCommandBuffer cmd[MAX_SYNC];
   VkSemaphore acquire_semaphores[MAX_SYNC];

   bool need_acquire[MAX_SYNC];
};
static struct vulkan_data vk;

void retro_init(void)
{}

void retro_deinit(void)
{}

unsigned retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "TestCore Async Compute Vulkan";
   info->library_version  = "v1";
   info->need_fullpath    = false;
   info->valid_extensions = NULL; // Anything is fine, we don't care.
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->timing = (struct retro_system_timing) {
      .fps = 60.0,
      .sample_rate = 30000.0,
   };

   info->geometry = (struct retro_game_geometry) {
      .base_width   = BASE_WIDTH,
      .base_height  = BASE_HEIGHT,
      .max_width    = BASE_WIDTH,
      .max_height   = BASE_HEIGHT,
      .aspect_ratio = (float)BASE_WIDTH / (float)BASE_HEIGHT,
   };
}

static retro_video_refresh_t video_cb;
static retro_audio_sample_t audio_cb;
static retro_audio_sample_batch_t audio_batch_cb;
static retro_environment_t environ_cb;
static retro_input_poll_t input_poll_cb;
static retro_input_state_t input_state_cb;

void retro_set_environment(retro_environment_t cb)
{
   environ_cb = cb;

   bool no_rom = true;
   cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_rom);
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
   audio_cb = cb;
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   audio_batch_cb = cb;
}

void retro_set_input_poll(retro_input_poll_t cb)
{
   input_poll_cb = cb;
}

void retro_set_input_state(retro_input_state_t cb)
{
   input_state_cb = cb;
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
   video_cb = cb;
}

static uint32_t find_memory_type_from_requirements(
      uint32_t device_requirements, uint32_t host_requirements)
{
   const VkPhysicalDeviceMemoryProperties *props = &vk.memory_properties;
   for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++)
   {
      if (device_requirements & (1u << i))
      {
         if ((props->memoryTypes[i].propertyFlags & host_requirements) == host_requirements)
         {
            return i;
         }
      }
   }

   return 0;
}

static void vulkan_test_render(void)
{
   VkCommandBuffer cmd = vk.cmd[vk.index];

   VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
   begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   vkResetCommandBuffer(cmd, 0);
   vkBeginCommandBuffer(cmd, &begin_info);

   VkImageMemoryBarrier prepare_rendering = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
   prepare_rendering.srcAccessMask = 0;
   prepare_rendering.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
   prepare_rendering.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
   prepare_rendering.newLayout = VK_IMAGE_LAYOUT_GENERAL;

   if (vk.need_acquire[vk.index])
   {
      prepare_rendering.srcQueueFamilyIndex = vulkan->queue_index;
      prepare_rendering.dstQueueFamilyIndex = async_queue_index;
   }
   else
   {
      prepare_rendering.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      prepare_rendering.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   }
   prepare_rendering.image = vk.images[vk.index].create_info.image;
   prepare_rendering.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   prepare_rendering.subresourceRange.levelCount = 1;
   prepare_rendering.subresourceRange.layerCount = 1;
   vkCmdPipelineBarrier(cmd,
         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         false, 
         0, NULL,
         0, NULL,
         1, &prepare_rendering);

   vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, vk.pipeline);
   vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
         vk.pipeline_layout, 0,
         1, &vk.desc_set[vk.index], 0, NULL);

   const float constants[4] = {
      1.0f / BASE_WIDTH,
      1.0f / BASE_HEIGHT,
      (float)frame_count++,
      0.0f,
   };
   vkCmdPushConstants(cmd, vk.pipeline_layout,
         VK_SHADER_STAGE_COMPUTE_BIT,
         0, 16, constants);

   vkCmdDispatch(cmd, BASE_WIDTH / 8, BASE_HEIGHT / 8, 1);

   VkImageMemoryBarrier prepare_presentation = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
   prepare_presentation.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
   prepare_presentation.dstAccessMask = 0;
   prepare_presentation.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
   prepare_presentation.newLayout = VK_IMAGE_LAYOUT_GENERAL;

   if (async_queue && vulkan->queue_index != async_queue_index)
   {
      prepare_presentation.srcQueueFamilyIndex = async_queue_index;
      prepare_presentation.dstQueueFamilyIndex = vulkan->queue_index;
      vk.need_acquire[vk.index] = true;
   }
   else
   {
      prepare_presentation.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      prepare_presentation.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      vk.need_acquire[vk.index] = false;
   }

   prepare_presentation.image = vk.images[vk.index].create_info.image;
   prepare_presentation.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   prepare_presentation.subresourceRange.levelCount = 1;
   prepare_presentation.subresourceRange.layerCount = 1;
   vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
         false,
         0, NULL,
         0, NULL,
         1, &prepare_presentation);

   vkEndCommandBuffer(cmd);

   if (!async_queue)
      vulkan->lock_queue(vulkan->handle);

   VkSubmitInfo submit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
   submit.commandBufferCount = 1;
   submit.pCommandBuffers = &cmd;
   submit.signalSemaphoreCount = 1;
   submit.pSignalSemaphores = &vk.acquire_semaphores[vk.index];
   vkQueueSubmit(async_queue != VK_NULL_HANDLE ? async_queue : vulkan->queue,
         1, &submit, VK_NULL_HANDLE);

   if (!async_queue)
      vulkan->unlock_queue(vulkan->handle);
}

static VkShaderModule create_shader_module(const uint32_t *data, size_t size)
{
   VkShaderModuleCreateInfo module_info = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
   VkShaderModule module;
   module_info.codeSize = size;
   module_info.pCode = data;
   vkCreateShaderModule(vulkan->device, &module_info, NULL, &module);
   return module;
}

static void init_descriptor(void)
{
   VkDevice device = vulkan->device;

   VkDescriptorSetLayoutBinding binding = {0};
   binding.binding = 0;
   binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
   binding.descriptorCount = 1;
   binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
   binding.pImmutableSamplers = NULL;

   const VkDescriptorPoolSize pool_sizes[1] = {
      { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, vk.num_swapchain_images },
   };

   const VkPushConstantRange range = {
      VK_SHADER_STAGE_COMPUTE_BIT,
      0, 16,
   };

   VkDescriptorSetLayoutCreateInfo set_layout_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
   set_layout_info.bindingCount = 1;
   set_layout_info.pBindings = &binding;
   vkCreateDescriptorSetLayout(device, &set_layout_info, NULL, &vk.set_layout);

   VkPipelineLayoutCreateInfo layout_info = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
   layout_info.setLayoutCount = 1;
   layout_info.pSetLayouts = &vk.set_layout;
   layout_info.pushConstantRangeCount = 1;
   layout_info.pPushConstantRanges = &range;
   vkCreatePipelineLayout(device, &layout_info, NULL, &vk.pipeline_layout);

   VkDescriptorPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
   pool_info.maxSets = vk.num_swapchain_images;
   pool_info.poolSizeCount = 1;
   pool_info.pPoolSizes = pool_sizes;
   pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
   vkCreateDescriptorPool(device, &pool_info, NULL, &vk.desc_pool);

   VkDescriptorSetAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
   alloc_info.descriptorPool = vk.desc_pool;
   alloc_info.descriptorSetCount = 1;
   alloc_info.pSetLayouts = &vk.set_layout;

   for (unsigned i = 0; i < vk.num_swapchain_images; i++)
   {
      vkAllocateDescriptorSets(device, &alloc_info, &vk.desc_set[i]);

      VkWriteDescriptorSet write = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
      VkDescriptorImageInfo image_info;

      write.dstSet = vk.desc_set[i];
      write.dstBinding = 0;
      write.descriptorCount = 1;
      write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
      write.pImageInfo = &image_info;

      image_info.imageView = vk.images[i].image_view;
      image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
      image_info.sampler = VK_NULL_HANDLE;

      vkUpdateDescriptorSets(device, 1, &write, 0, NULL);
   }
}

static void init_pipeline(void)
{
   VkDevice device = vulkan->device;

   static const uint32_t raymarch_comp[] =
#include "shaders/raymarch.comp.inc"
      ;

   VkComputePipelineCreateInfo pipe = { VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };

   pipe.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   pipe.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
   pipe.stage.module = create_shader_module(raymarch_comp, sizeof(raymarch_comp));
   pipe.stage.pName = "main";
   pipe.layout = vk.pipeline_layout;

   vkCreateComputePipelines(vulkan->device, vk.pipeline_cache, 1, &pipe, NULL, &vk.pipeline);
   vkDestroyShaderModule(device, pipe.stage.module, NULL);
}

static void init_swapchain(void)
{
   VkDevice device = vulkan->device;

   for (unsigned i = 0; i < vk.num_swapchain_images; i++)
   {
      VkImageCreateInfo image = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };

      image.imageType = VK_IMAGE_TYPE_2D;
      image.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
      image.format = VK_FORMAT_R8G8B8A8_UNORM;
      image.extent.width = BASE_WIDTH;
      image.extent.height = BASE_HEIGHT;
      image.extent.depth = 1;
      image.samples = VK_SAMPLE_COUNT_1_BIT;
      image.tiling = VK_IMAGE_TILING_OPTIMAL;
      image.usage =
         VK_IMAGE_USAGE_STORAGE_BIT |
         VK_IMAGE_USAGE_SAMPLED_BIT |
         VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
      image.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      image.mipLevels = 1;
      image.arrayLayers = 1;

      uint32_t share_queues[] = { async_queue_index, vulkan->queue_index };
      if (async_queue && async_queue_index != vulkan->queue_index)
      {
         image.queueFamilyIndexCount = 2;
         image.pQueueFamilyIndices = share_queues;
      }

      vkCreateImage(device, &image, NULL, &vk.images[i].create_info.image);

      VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
      VkMemoryRequirements mem_reqs;

      vkGetImageMemoryRequirements(device, vk.images[i].create_info.image, &mem_reqs);
      alloc.allocationSize = mem_reqs.size;
      alloc.memoryTypeIndex = find_memory_type_from_requirements(
            mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
      vkAllocateMemory(device, &alloc, NULL, &vk.image_memory[i]);
      vkBindImageMemory(device, vk.images[i].create_info.image, vk.image_memory[i], 0);

      vk.images[i].create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      vk.images[i].create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      vk.images[i].create_info.format = VK_FORMAT_R8G8B8A8_UNORM;
      vk.images[i].create_info.subresourceRange.baseMipLevel = 0;
      vk.images[i].create_info.subresourceRange.baseArrayLayer = 0;
      vk.images[i].create_info.subresourceRange.levelCount = 1;
      vk.images[i].create_info.subresourceRange.layerCount = 1;
      vk.images[i].create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      vk.images[i].create_info.components.r = VK_COMPONENT_SWIZZLE_R;
      vk.images[i].create_info.components.g = VK_COMPONENT_SWIZZLE_G;
      vk.images[i].create_info.components.b = VK_COMPONENT_SWIZZLE_B;
      vk.images[i].create_info.components.a = VK_COMPONENT_SWIZZLE_A;

      vkCreateImageView(device, &vk.images[i].create_info,
            NULL, &vk.images[i].image_view);
      vk.images[i].image_layout = VK_IMAGE_LAYOUT_GENERAL;

      VkSemaphoreCreateInfo sem_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
      vkCreateSemaphore(device, &sem_info, NULL, &vk.acquire_semaphores[i]);
   }
}

static void init_command(void)
{
   VkCommandPoolCreateInfo pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
   VkCommandBufferAllocateInfo info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };

   pool_info.queueFamilyIndex = async_queue != VK_NULL_HANDLE ?
      async_queue_index : vulkan->queue_index;
   pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

   for (unsigned i = 0; i < vk.num_swapchain_images; i++)
   {
      vkCreateCommandPool(vulkan->device, &pool_info, NULL, &vk.cmd_pool[i]);
      info.commandPool = vk.cmd_pool[i];
      info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      info.commandBufferCount = 1;
      vkAllocateCommandBuffers(vulkan->device, &info, &vk.cmd[i]);
   }
}

static void vulkan_test_init(void)
{
   vkGetPhysicalDeviceProperties(vulkan->gpu, &vk.gpu_properties);
   vkGetPhysicalDeviceMemoryProperties(vulkan->gpu, &vk.memory_properties);

   unsigned num_images = 0;
   uint32_t mask = vulkan->get_sync_index_mask(vulkan->handle);
   for (unsigned i = 0; i < 32; i++)
      if (mask & (1u << i))
         num_images = i + 1;
   vk.num_swapchain_images = num_images;
   vk.swapchain_mask = mask;

   init_command();
   init_swapchain();
   init_descriptor();

   VkPipelineCacheCreateInfo pipeline_cache_info = { VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };
   vkCreatePipelineCache(vulkan->device, &pipeline_cache_info,
         NULL, &vk.pipeline_cache);

   init_pipeline();
}

static void vulkan_test_deinit(void)
{
   if (!vulkan)
      return;

   VkDevice device = vulkan->device;
   vkDeviceWaitIdle(device);

   for (unsigned i = 0; i < vk.num_swapchain_images; i++)
   {
      vkDestroyImageView(device, vk.images[i].image_view, NULL);
      vkFreeMemory(device, vk.image_memory[i], NULL);
      vkDestroyImage(device, vk.images[i].create_info.image, NULL);
      vkDestroySemaphore(device, vk.acquire_semaphores[i], NULL);
   }

   vkFreeDescriptorSets(device, vk.desc_pool, vk.num_swapchain_images, vk.desc_set);
   vkDestroyDescriptorPool(device, vk.desc_pool, NULL);

   vkDestroyPipeline(device, vk.pipeline, NULL);
   vkDestroyDescriptorSetLayout(device, vk.set_layout, NULL);
   vkDestroyPipelineLayout(device, vk.pipeline_layout, NULL);
   vkDestroyPipelineCache(device, vk.pipeline_cache, NULL);

   for (unsigned i = 0; i < vk.num_swapchain_images; i++)
   {
      vkFreeCommandBuffers(device, vk.cmd_pool[i], 1, &vk.cmd[i]);
      vkDestroyCommandPool(device, vk.cmd_pool[i], NULL);
   }

   memset(&vk, 0, sizeof(vk));
}

void retro_run(void)
{
   input_poll_cb();

   if (input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP))
   {
   }

   /* Very lazy way to do this. */
   if (vulkan->get_sync_index_mask(vulkan->handle) != vk.swapchain_mask)
   {
      vulkan_test_deinit();
      vulkan_test_init();
   }

   vulkan->wait_sync_index(vulkan->handle);

   vk.index = vulkan->get_sync_index(vulkan->handle);
   vulkan_test_render();
   vulkan->set_image(vulkan->handle, &vk.images[vk.index],
         1, &vk.acquire_semaphores[vk.index],
         async_queue && async_queue_index != vulkan->queue_index ?
         async_queue_index : VK_QUEUE_FAMILY_IGNORED);
   video_cb(RETRO_HW_FRAME_BUFFER_VALID, BASE_WIDTH, BASE_HEIGHT, 0);
}

static void context_reset(void)
{
   fprintf(stderr, "Context reset!\n");
   if (!environ_cb(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE, (void**)&vulkan) || !vulkan)
   {
      fprintf(stderr, "Failed to get HW rendering interface!\n");
      return;
   }

   if (vulkan->interface_version != RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION)
   {
      fprintf(stderr, "HW render interface mismatch, expected %u, got %u!\n",
            RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION, vulkan->interface_version);
      vulkan = NULL;
      return;
   }

   vulkan_symbol_wrapper_init(vulkan->get_instance_proc_addr);
   vulkan_symbol_wrapper_load_core_instance_symbols(vulkan->instance);
   vulkan_symbol_wrapper_load_core_device_symbols(vulkan->device);
   vulkan_test_init();
}

static void context_destroy(void)
{
   fprintf(stderr, "Context destroy!\n");
   vulkan_test_deinit();
   vulkan = NULL;
   memset(&vk, 0, sizeof(vk));
}

static const VkApplicationInfo *get_application_info(void)
{
   static const VkApplicationInfo info = {
      VK_STRUCTURE_TYPE_APPLICATION_INFO,
      NULL,
      "libretro-test-vulkan-async-compute",
      0,
      "libretro-test-vulkan-async-compute",
      0,
      VK_MAKE_VERSION(1, 0, 18),
   };
   return &info;
}

static bool create_device(struct retro_vulkan_context *context,
      VkInstance instance,
      VkPhysicalDevice gpu,
      VkSurfaceKHR surface,
      PFN_vkGetInstanceProcAddr get_instance_proc_addr,
      const char **required_device_extensions,
      unsigned num_required_device_extensions,
      const char **required_device_layers,
      unsigned num_required_device_layers,
      const VkPhysicalDeviceFeatures *required_features)
{
   async_queue = VK_NULL_HANDLE;
   vulkan_symbol_wrapper_init(get_instance_proc_addr);
   vulkan_symbol_wrapper_load_core_symbols(instance);

   if (gpu == VK_NULL_HANDLE)
   {
      uint32_t gpu_count;
      vkEnumeratePhysicalDevices(instance, &gpu_count, NULL);
      if (!gpu_count)
         return false;
      VkPhysicalDevice *gpus = calloc(gpu_count, sizeof(*gpus));
      if (!gpus)
         return false;

      vkEnumeratePhysicalDevices(instance, &gpu_count, gpus);
      gpu = gpus[0];
      free(gpus);
   }

   context->gpu = gpu;

   uint32_t queue_count;
   VkQueueFamilyProperties *queue_properties = NULL;
   vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_count, NULL);
   if (queue_count < 1)
      return false;
   queue_properties = calloc(queue_count, sizeof(*queue_properties));
   if (!queue_properties)
      return false;
   vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queue_count, queue_properties);

   if (surface != VK_NULL_HANDLE)
   {
      VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(instance,
            vkGetPhysicalDeviceSurfaceSupportKHR);
   }

   bool found_queue = false;
   for (uint32_t i = 0; i < queue_count; i++)
   {
      VkBool32 supported = surface == VK_NULL_HANDLE;

      if (surface != VK_NULL_HANDLE)
      {
         vkGetPhysicalDeviceSurfaceSupportKHR(
               gpu, i, surface, &supported);
      }

      VkQueueFlags required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
      if (supported && ((queue_properties[i].queueFlags & required) == required))
      {
         context->queue_family_index = i;
         found_queue = true;
         break;
      }
   }

   if (!found_queue)
   {
      free(queue_properties);
      return false;
   }

   bool same_queue_async = false;
   if (queue_properties[context->queue_family_index].queueCount >= 2)
      same_queue_async = true;

   if (!same_queue_async)
   {
      found_queue = false;
      for (uint32_t i = 0; i < queue_count; i++)
      {
         if (i == context->queue_family_index)
            continue;

         VkQueueFlags required = VK_QUEUE_COMPUTE_BIT;
         if ((queue_properties[i].queueFlags & required) == required)
         {
            async_queue_index = i;
            found_queue = true;
            break;
         }
      }
   }
   else
      async_queue_index = context->queue_family_index;

   free(queue_properties);
   if (!found_queue)
      return false;

   const float prios[] = { 0.5f, 0.5f };
   VkDeviceQueueCreateInfo queues[2] = {
      { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO },
      { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO },
   };

   if (same_queue_async)
   {
      queues[0].queueFamilyIndex = context->queue_family_index;
      queues[0].queueCount = 2;
      queues[0].pQueuePriorities = prios;
   }
   else
   {
      queues[0].queueFamilyIndex = context->queue_family_index;
      queues[0].queueCount = 1;
      queues[0].pQueuePriorities = &prios[0];
      queues[1].queueFamilyIndex = async_queue_index;
      queues[1].queueCount = 1;
      queues[1].pQueuePriorities = &prios[1];
   }

   VkDeviceCreateInfo device_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
   device_info.enabledExtensionCount = num_required_device_extensions;
   device_info.ppEnabledExtensionNames = required_device_extensions;
   device_info.enabledLayerCount = num_required_device_layers;
   device_info.ppEnabledLayerNames = required_device_layers;
   device_info.queueCreateInfoCount = same_queue_async ? 1 : 2;
   device_info.pQueueCreateInfos = queues;

   if (vkCreateDevice(gpu, &device_info, NULL, &context->device) != VK_SUCCESS)
      return false;

   vkGetDeviceQueue(context->device, context->queue_family_index, 0, &context->queue);
   if (same_queue_async)
      vkGetDeviceQueue(context->device, context->queue_family_index, 1, &async_queue);
   else
      vkGetDeviceQueue(context->device, async_queue_index, 0, &async_queue);

   context->presentation_queue = context->queue;
   context->presentation_queue_family_index = context->queue_family_index;
   return true;
}

static bool retro_init_hw_context(void)
{
   hw_render.context_type = RETRO_HW_CONTEXT_VULKAN;
   hw_render.version_major = VK_MAKE_VERSION(1, 0, 18);
   hw_render.version_minor = 0;
   hw_render.context_reset = context_reset;
   hw_render.context_destroy = context_destroy;
   hw_render.cache_context = false;
   if (!environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
      return false;

   static const struct retro_hw_render_context_negotiation_interface_vulkan iface = {
      RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN,
      RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN_VERSION,

      get_application_info,
      create_device,
   };

   environ_cb(RETRO_ENVIRONMENT_SET_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE, (void*)&iface);

   return true;
}

bool retro_load_game(const struct retro_game_info *info)
{
   if (!retro_init_hw_context())
   {
      fprintf(stderr, "HW Context could not be initialized, exiting...\n");
      return false;
   }

   fprintf(stderr, "Loaded game!\n");
   (void)info;

   frame_count = 0;
   return true;
}

void retro_unload_game(void)
{}

unsigned retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t retro_serialize_size(void)
{
   return 0;
}

bool retro_serialize(void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

bool retro_unserialize(const void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

void *retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void retro_reset(void)
{}

void retro_cheat_reset(void)
{}

void retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}

