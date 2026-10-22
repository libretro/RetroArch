/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "vulkan_android_cache_entry.h"
#include <string.h>
static const uint32_t vulkan_android_entry_rgba8_shader[] =
#include "../drivers/vulkan_shaders/external_rgb_to_rgba8888.comp.inc"
      ;
static const uint32_t vulkan_android_entry_fp16_shader[] =
#include "../drivers/vulkan_shaders/external_rgb_to_rgba16f.comp.inc"
      ;
bool vulkan_android_cache_entry_destroy(vulkan_android_cache_entry_t *e)
{
   vulkan_android_submission_t *s;
   if (!e)
      return true;
   s = &e->submission;
   if (s->inflight || s->held_owner)
   {
      vulkan_android_submission_poll(s);
      if (s->inflight || s->held_owner)
         return false;
   }
   if (s->fence)
      e->ops.destroy_fence(s->device, s->fence, NULL);
   if (s->command)
      e->ops.free_commands(s->device, e->command_pool, 1, &s->command);
   vulkan_android_pipeline_destroy(s->device, &s->pipeline_ops, &s->pipeline);
   vulkan_android_sync_destroy(s->device, &s->sync_ops, &s->sync);
   vulkan_android_image_destroy(s->device, &e->image_ops, &s->image);
   memset(e, 0, sizeof(*e));
   return true;
}
bool vulkan_android_cache_entry_create(const vulkan_android_entry_config_t *c,
      const android_frame_view_t *source, bool fp16,
      vulkan_android_cache_entry_t *out)
{
   VkSamplerYcbcrModelConversion model;
   VkSamplerYcbcrRange range;
   VkCommandBufferAllocateInfo command;
   VkFenceCreateInfo fence;
   vulkan_android_submission_t *s;
   if (!out)
      return false;
   memset(out, 0, sizeof(*out));
   if (!c || !source || !c->device || !c->queue || !c->command_pool
         || !c->sampler_descriptors || !c->memory
         || c->family >= VK_QUEUE_FAMILY_FOREIGN_EXT
         || !c->entry.allocate_commands || !c->entry.free_commands
         || !c->entry.create_fence || !c->entry.destroy_fence
         || !c->submit.fence_status || !c->submit.reset_fences
         || !c->submit.reset_command || !c->submit.begin || !c->submit.end
         || !c->submit.barrier || !c->submit.bind_pipeline
         || !c->submit.bind_sets || !c->submit.push || !c->submit.dispatch
         || !c->submit.submit)
      return false;
   switch (source->color.matrix)
   {
   case 1:
      model = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709;
      break;
   case 5:
   case 6:
      model = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_601;
      break;
   case 9:
      model = VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_2020;
      break;
   default:
      return false;
   }
   if (source->color.range != 1 && source->color.range != 2)
      return false;
   range = source->color.range == 2 ? VK_SAMPLER_YCBCR_RANGE_ITU_FULL
                                    : VK_SAMPLER_YCBCR_RANGE_ITU_NARROW;
   s = &out->submission;
   s->device = c->device;
   s->queue = c->queue;
   s->family = c->family;
   s->pipeline_ops = c->pipeline;
   s->sync_ops = c->sync;
   s->ops = c->submit;
   out->command_pool = c->command_pool;
   out->ops = c->entry;
   out->image_ops = c->image;
   if (!vulkan_android_submission_configure(s, source, fp16))
      goto fail;
   if (!vulkan_android_image_create(c->device, &c->image,
             (struct AHardwareBuffer *)source->image.buffer,
             source->image.buffer_width, source->image.buffer_height, c->memory,
             model, range, &s->image))
      goto fail;
   if (!vulkan_android_pipeline_create(c->device, &c->pipeline,
             s->image.sampler, c->sampler_descriptors, 1,
             fp16 ? vulkan_android_entry_fp16_shader
                  : vulkan_android_entry_rgba8_shader,
             fp16 ? sizeof(vulkan_android_entry_fp16_shader)
                  : sizeof(vulkan_android_entry_rgba8_shader),
             &s->pipeline))
      goto fail;
   if (!vulkan_android_sync_create(c->device, &c->sync, &s->sync))
      goto fail;
   memset(&command, 0, sizeof(command));
   command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   command.commandPool = c->command_pool;
   command.commandBufferCount = 1;
   if (c->entry.allocate_commands(c->device, &command, &s->command)
         != VK_SUCCESS)
      goto fail;
   memset(&fence, 0, sizeof(fence));
   fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
   if (c->entry.create_fence(c->device, &fence, NULL, &s->fence) != VK_SUCCESS)
      goto fail;
   return true;
fail:
   vulkan_android_cache_entry_destroy(out);
   return false;
}
