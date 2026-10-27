/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
/* Included by vulkan.c after its private renderer types and queue helpers. */
static void vulkan_android_renderer_destroy_destination(
      VkDevice device, void *destination)
{
   struct vk_texture *texture = (struct vk_texture *)destination;
   vulkan_destroy_texture(device, texture);
   free(texture);
}
static vulkan_android_cache_t *vulkan_android_renderer_cache(
      vk_t *vk, struct vk_texture *texture, bool fp16)
{
   vulkan_android_entry_config_t config;
   VkCommandPoolCreateInfo pool;
   vulkan_android_cache_t *cache;
   VkDevice device = vk->context->device;
   vulkan_android_cache_device_collect(device);
   if (!(vk->context->flags & VK_CTX_FLAG_ANDROID_IMAGES))
      return NULL;
   memset(&config, 0, sizeof(config));
   config.device = device;
   config.queue = vk->context->queue;
   config.family = vk->context->graphics_queue_index;
   config.memory = &vk->context->memory_properties;
   config.destroy_deferred_destination
         = vulkan_android_renderer_destroy_destination;
   config.deferred_destination = texture;
   config.sampler_descriptors = vk->context->android_sampler_descriptors;
#define VK_ANDROID_RESOLVE(field, name)                                        \
   config.field = (PFN_vk##name)vkGetDeviceProcAddr(device, "vk" #name);       \
   if (!config.field)                                                          \
   return NULL
   VK_ANDROID_RESOLVE(
         image.properties, GetAndroidHardwareBufferPropertiesANDROID);
   VK_ANDROID_RESOLVE(image.create_image, CreateImage);
   VK_ANDROID_RESOLVE(image.destroy_image, DestroyImage);
   VK_ANDROID_RESOLVE(image.allocate, AllocateMemory);
   VK_ANDROID_RESOLVE(image.free_memory, FreeMemory);
   VK_ANDROID_RESOLVE(image.bind, BindImageMemory);
   VK_ANDROID_RESOLVE(image.create_conversion, CreateSamplerYcbcrConversion);
   VK_ANDROID_RESOLVE(image.destroy_conversion, DestroySamplerYcbcrConversion);
   VK_ANDROID_RESOLVE(image.create_sampler, CreateSampler);
   VK_ANDROID_RESOLVE(image.destroy_sampler, DestroySampler);
   VK_ANDROID_RESOLVE(image.create_view, CreateImageView);
   VK_ANDROID_RESOLVE(image.destroy_view, DestroyImageView);
   VK_ANDROID_RESOLVE(pipeline.create_set_layout, CreateDescriptorSetLayout);
   VK_ANDROID_RESOLVE(pipeline.destroy_set_layout, DestroyDescriptorSetLayout);
   VK_ANDROID_RESOLVE(pipeline.create_layout, CreatePipelineLayout);
   VK_ANDROID_RESOLVE(pipeline.destroy_layout, DestroyPipelineLayout);
   VK_ANDROID_RESOLVE(pipeline.create_pool, CreateDescriptorPool);
   VK_ANDROID_RESOLVE(pipeline.destroy_pool, DestroyDescriptorPool);
   VK_ANDROID_RESOLVE(pipeline.allocate_sets, AllocateDescriptorSets);
   VK_ANDROID_RESOLVE(pipeline.update_sets, UpdateDescriptorSets);
   VK_ANDROID_RESOLVE(pipeline.create_shader, CreateShaderModule);
   VK_ANDROID_RESOLVE(pipeline.destroy_shader, DestroyShaderModule);
   VK_ANDROID_RESOLVE(pipeline.create_pipeline, CreateComputePipelines);
   VK_ANDROID_RESOLVE(pipeline.destroy_pipeline, DestroyPipeline);
   VK_ANDROID_RESOLVE(sync.create, CreateSemaphore);
   VK_ANDROID_RESOLVE(sync.destroy, DestroySemaphore);
   VK_ANDROID_RESOLVE(sync.import_fd, ImportSemaphoreFdKHR);
   VK_ANDROID_RESOLVE(sync.export_fd, GetSemaphoreFdKHR);
   VK_ANDROID_RESOLVE(submit.fence_status, GetFenceStatus);
   VK_ANDROID_RESOLVE(submit.reset_fences, ResetFences);
   VK_ANDROID_RESOLVE(submit.reset_command, ResetCommandBuffer);
   VK_ANDROID_RESOLVE(submit.begin, BeginCommandBuffer);
   VK_ANDROID_RESOLVE(submit.end, EndCommandBuffer);
   VK_ANDROID_RESOLVE(submit.barrier, CmdPipelineBarrier);
   VK_ANDROID_RESOLVE(submit.bind_pipeline, CmdBindPipeline);
   VK_ANDROID_RESOLVE(submit.bind_sets, CmdBindDescriptorSets);
   VK_ANDROID_RESOLVE(submit.push, CmdPushConstants);
   VK_ANDROID_RESOLVE(submit.dispatch, CmdDispatch);
   VK_ANDROID_RESOLVE(entry.allocate_commands, AllocateCommandBuffers);
   VK_ANDROID_RESOLVE(entry.free_commands, FreeCommandBuffers);
   VK_ANDROID_RESOLVE(entry.create_fence, CreateFence);
   VK_ANDROID_RESOLVE(entry.destroy_fence, DestroyFence);
   VK_ANDROID_RESOLVE(destroy_owned_pool, DestroyCommandPool);
#undef VK_ANDROID_RESOLVE
   config.submit.submit = vulkan_android_queue_push;
   config.submit.submit_context = &vk->android_submissions;
   memset(&pool, 0, sizeof(pool));
   pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
   pool.queueFamilyIndex = config.family;
   if (vkCreateCommandPool(device, &pool, NULL, &config.command_pool)
         != VK_SUCCESS)
      return NULL;
   cache = vulkan_android_cache_new(&config, 32, fp16);
   if (!cache)
      vkDestroyCommandPool(device, config.command_pool, NULL);
   return cache;
}
static bool vulkan_android_renderer_source(struct vk_texture *texture,
      const video_frame_import_t *input, video_hw_frame_t *frame)
{
   android_frame_view_t source;
   bool matches;
   memset(frame, 0, sizeof(*frame));
   frame->image = input->image;
   frame->done = input->done;
   frame->done_value = input->done_value;
   frame->owner = input->owner;
   frame->abandon = input->abandon;
   frame->color = input->color;
   if (!android_frame_claim(frame->owner, frame->done, frame->done_value,
             frame->image, &source))
      return false;
   matches = source.pixfmt == input->pixfmt
         && source.image.right - source.image.left
               == VIDEO_SCALE_W(texture->dims)
         && source.image.bottom - source.image.top
               == VIDEO_SCALE_H(texture->dims)
         && android_frame_resolve_color(&source, &input->color);
   return android_frame_unclaim(frame->owner, frame->done, frame->done_value)
         && matches;
}

static enum video_texture_update vulkan_android_renderer_prepare(
      vk_t *vk, struct vk_texture *texture, const video_frame_import_t *input)
{
   video_hw_frame_t frame;
   int result;
   if (!vulkan_android_renderer_source(texture, input, &frame))
      return VIDEO_TEXTURE_UPDATE_REFUSED;
   if (!texture->android_cache)
   {
      texture->android_cache = vulkan_android_renderer_cache(
            vk, texture, texture->format == VK_FORMAT_R16G16B16A16_SFLOAT);
      if (texture->android_cache)
      {
         texture->android_next = vk->android_textures;
         vk->android_textures = texture;
      }
   }
   if (!texture->android_cache)
      return VIDEO_TEXTURE_UPDATE_REFUSED;
   result = vulkan_android_cache_prepare(texture->android_cache, &frame);
   return result > 0                   ? VIDEO_TEXTURE_UPDATE_DONE
         : result == 0 || result == -2 ? VIDEO_TEXTURE_UPDATE_DROPPED
                                       : VIDEO_TEXTURE_UPDATE_REFUSED;
}

static enum video_texture_update vulkan_android_renderer_import(
      vk_t *vk, struct vk_texture *texture, const video_frame_import_t *input)
{
   video_hw_frame_t frame;
   int result;
   (void)vk;
   if (!texture->android_cache
         || !vulkan_android_renderer_source(texture, input, &frame))
      return VIDEO_TEXTURE_UPDATE_REFUSED;
   result = vulkan_android_cache_present(texture->android_cache, &frame,
         texture->image, texture->view, texture->layout, NULL);
   if (result == 1)
   {
      texture->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      return VIDEO_TEXTURE_UPDATE_DONE;
   }
   return result == 0 || result == -2 ? VIDEO_TEXTURE_UPDATE_DROPPED
                                      : VIDEO_TEXTURE_UPDATE_REFUSED;
}
