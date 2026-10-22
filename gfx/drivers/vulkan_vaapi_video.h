/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
/* Included after the private renderer types and existing queue callbacks. */
static VkResult vulkan_vaapi_renderer_submit(void *context, VkQueue queue,
      uint32_t count, const VkSubmitInfo *submits, VkFence fence)
{
   vk_t *vk = (vk_t *)context;
   VkResult result;
   vulkan_lock_queue(vk);
   result = vkQueueSubmit(queue, count, submits, fence);
   vulkan_unlock_queue(vk);
   vulkan_check_device_lost(vk, result);
   return result;
}

static void vulkan_vaapi_renderer_destroy_destination(
      VkDevice device, void *destination)
{
   struct vk_texture *texture = (struct vk_texture *)destination;
   vulkan_destroy_texture(device, texture);
   free(texture);
}

static vaapi_hevc_vulkan_cache_t *vulkan_vaapi_renderer_cache(
      vk_t *vk, struct vk_texture *texture)
{
   vaapi_hevc_vulkan_cache_config_t config;
   VkDevice device = vk->context->device;
   PFN_vkGetInstanceProcAddr instance_proc =
         vulkan_symbol_wrapper_instance_proc_addr();
   if (!vk->context->prime_import_device || !instance_proc)
      return NULL;
   memset(&config, 0, sizeof(config));
   config.gpu          = vk->context->gpu;
   config.device       = device;
   config.queue        = vk->context->queue;
   config.family       = vk->context->graphics_queue_index;
   config.memory       = &vk->context->memory_properties;
   config.prime_device = vk->context->prime_import_device;
   config.destroy_deferred_destination =
         vulkan_vaapi_renderer_destroy_destination;
   config.deferred_destination = texture;
#define VK_VAAPI_RESOLVE(field, name) \
   config.field = (PFN_vk##name)vkGetDeviceProcAddr(device, "vk" #name); \
   if (!config.field) \
      return NULL
   VK_VAAPI_RESOLVE(image.create_image, CreateImage);
   VK_VAAPI_RESOLVE(image.destroy_image, DestroyImage);
   VK_VAAPI_RESOLVE(image.requirements, GetImageMemoryRequirements2);
   VK_VAAPI_RESOLVE(image.fd_properties, GetMemoryFdPropertiesKHR);
   VK_VAAPI_RESOLVE(image.allocate, AllocateMemory);
   VK_VAAPI_RESOLVE(image.free_memory, FreeMemory);
   VK_VAAPI_RESOLVE(image.bind, BindImageMemory2);
   VK_VAAPI_RESOLVE(image.create_view, CreateImageView);
   VK_VAAPI_RESOLVE(image.destroy_view, DestroyImageView);
   VK_VAAPI_RESOLVE(resources.create_sampler, CreateSampler);
   VK_VAAPI_RESOLVE(resources.destroy_sampler, DestroySampler);
   VK_VAAPI_RESOLVE(resources.create_set_layout, CreateDescriptorSetLayout);
   VK_VAAPI_RESOLVE(resources.destroy_set_layout, DestroyDescriptorSetLayout);
   VK_VAAPI_RESOLVE(resources.create_layout, CreatePipelineLayout);
   VK_VAAPI_RESOLVE(resources.destroy_layout, DestroyPipelineLayout);
   VK_VAAPI_RESOLVE(resources.create_pool, CreateDescriptorPool);
   VK_VAAPI_RESOLVE(resources.destroy_pool, DestroyDescriptorPool);
   VK_VAAPI_RESOLVE(resources.allocate_sets, AllocateDescriptorSets);
   VK_VAAPI_RESOLVE(resources.create_shader, CreateShaderModule);
   VK_VAAPI_RESOLVE(resources.destroy_shader, DestroyShaderModule);
   VK_VAAPI_RESOLVE(resources.create_pipeline, CreateComputePipelines);
   VK_VAAPI_RESOLVE(resources.destroy_pipeline, DestroyPipeline);
   VK_VAAPI_RESOLVE(resources.create_commands, CreateCommandPool);
   VK_VAAPI_RESOLVE(resources.destroy_commands, DestroyCommandPool);
   VK_VAAPI_RESOLVE(resources.allocate_commands, AllocateCommandBuffers);
   VK_VAAPI_RESOLVE(resources.create_fence, CreateFence);
   VK_VAAPI_RESOLVE(resources.destroy_fence, DestroyFence);
   VK_VAAPI_RESOLVE(submit.fence_status, GetFenceStatus);
   VK_VAAPI_RESOLVE(submit.reset_fences, ResetFences);
   VK_VAAPI_RESOLVE(submit.reset_command, ResetCommandBuffer);
   VK_VAAPI_RESOLVE(submit.begin, BeginCommandBuffer);
   VK_VAAPI_RESOLVE(submit.end, EndCommandBuffer);
   VK_VAAPI_RESOLVE(submit.update, UpdateDescriptorSets);
   VK_VAAPI_RESOLVE(submit.barrier, CmdPipelineBarrier);
   VK_VAAPI_RESOLVE(submit.bind_pipeline, CmdBindPipeline);
   VK_VAAPI_RESOLVE(submit.bind_sets, CmdBindDescriptorSets);
   VK_VAAPI_RESOLVE(submit.push, CmdPushConstants);
   VK_VAAPI_RESOLVE(submit.dispatch, CmdDispatch);
#undef VK_VAAPI_RESOLVE
   config.get_format = (PFN_vkGetPhysicalDeviceFormatProperties2)instance_proc(
         vk->context->instance, "vkGetPhysicalDeviceFormatProperties2");
   config.get_image =
         (PFN_vkGetPhysicalDeviceImageFormatProperties2)instance_proc(
               vk->context->instance,
               "vkGetPhysicalDeviceImageFormatProperties2");
   config.submit.submit  = vulkan_vaapi_renderer_submit;
   config.submit.context = vk;
   if (!vulkan_prime_image_bind_io(&config.image))
      return NULL;
   return vaapi_hevc_vulkan_cache_new(
         &config, 32, texture->format == VK_FORMAT_R16G16B16A16_SFLOAT);
}

static enum video_texture_update vulkan_vaapi_renderer_import(
      vk_t *vk, struct vk_texture *texture, const video_frame_import_t *input)
{
   video_hw_frame_t frame;
   bool prepare = false;
   int result;
   if (texture->dims != input->dims
         || (texture->format == VK_FORMAT_R16G16B16A16_SFLOAT)
                  != (input->output_pixfmt == IMAGE_PIXFMT_FP16))
      return VIDEO_TEXTURE_UPDATE_REFUSED;
   memset(&frame, 0, sizeof(frame));
   frame.owner      = input->owner;
   frame.abandon    = input->abandon;
   frame.image      = input->image;
   frame.done       = input->done;
   frame.done_value = input->done_value;
   frame.width      = VIDEO_SCALE_W(input->dims);
   frame.height     = VIDEO_SCALE_H(input->dims);
   frame.crop_x     = input->crop_x;
   frame.crop_y     = input->crop_y;
   frame.pixfmt     = input->pixfmt;
   if (!texture->vaapi_cache)
   {
      texture->vaapi_cache = vulkan_vaapi_renderer_cache(vk, texture);
      if (!texture->vaapi_cache)
         return VIDEO_TEXTURE_UPDATE_REFUSED;
      texture->vaapi_next = vk->vaapi_textures;
      vk->vaapi_textures  = texture;
   }
   result = vaapi_hevc_vulkan_cache_present(texture->vaapi_cache, &frame,
         texture->image, texture->view, texture->format, frame.width,
         frame.height, texture->layout, &prepare);
   if (prepare)
   {
      result = vaapi_hevc_vulkan_cache_prepare(texture->vaapi_cache, &frame);
      if (result > 0)
         result = vaapi_hevc_vulkan_cache_present(texture->vaapi_cache, &frame,
               texture->image, texture->view, texture->format, frame.width,
               frame.height, texture->layout, NULL);
   }
   if (result == 1)
   {
      texture->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      return VIDEO_TEXTURE_UPDATE_DONE;
   }
   return result == 0 || result == -2 ? VIDEO_TEXTURE_UPDATE_DROPPED
                                      : VIDEO_TEXTURE_UPDATE_REFUSED;
}
