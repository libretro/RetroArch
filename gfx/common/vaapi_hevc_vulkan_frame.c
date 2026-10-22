/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_hevc_vulkan_frame.h"

bool vaapi_hevc_vulkan_frame_prepare(vaapi_hevc_vulkan_frame_t *out,
      const vaapi_hevc_export_t *source, VkPhysicalDevice gpu, VkDevice device,
      bool prime_device, const VkPhysicalDeviceMemoryProperties *memory,
      const vulkan_prime_image_ops_t *ops,
      PFN_vkGetPhysicalDeviceFormatProperties2 get_format,
      PFN_vkGetPhysicalDeviceImageFormatProperties2 get_image)
{
   vulkan_prime_format_t plans[2];
   vulkan_prime_plane_t planes[4];
   unsigned i, j;
   if (!out || out->active || out->source.active || out->device || !source
         || source == &out->source || !device || !ops)
      return false;
   for (i = 0; i < 2; i++)
   {
      if (out->layers[i].image || out->layers[i].view)
         return false;
      for (j = 0; j < 4; j++)
         if (out->layers[i].memory[j])
            return false;
   }
   if (!vaapi_hevc_vulkan_query(
             plans, source, gpu, prime_device, get_format, get_image))
      return false;
   for (i = 0; i < 2; i++)
   {
      memset(planes, 0, sizeof(planes));
      for (j = 0; j < plans[i].memory_planes; j++)
      {
         unsigned object = source->descriptor.layers[i].object_index[j];
         planes[j].fd    = source->descriptor.objects[object].fd;
         planes[j].allocation_size = source->descriptor.objects[object].size;
         planes[j].offset          = source->descriptor.layers[i].offset[j];
         planes[j].pitch           = source->descriptor.layers[i].pitch[j];
      }
      if (!vulkan_prime_image_create(
                device, ops, memory, &plans[i], planes, &out->layers[i]))
      {
         for (j = 0; j < 2; j++)
            vulkan_prime_image_destroy(device, ops, &out->layers[j]);
         memset(out, 0, sizeof(*out));
         return false;
      }
   }
   out->descriptor = source->descriptor;
   out->pool       = source->pool;
   out->slot       = source->slot;
   for (i = 0; i < 4; i++)
   {
      out->descriptor.objects[i].fd = -1;
      out->object_device[i]         = source->object_device[i];
      out->object_inode[i]          = source->object_inode[i];
   }
   out->ops    = *ops;
   out->device = device;
   out->active = true;
   return true;
}

bool vaapi_hevc_vulkan_frame_create(vaapi_hevc_vulkan_frame_t *out,
      vaapi_hevc_export_t *source, VkPhysicalDevice gpu, VkDevice device,
      bool prime_device, const VkPhysicalDeviceMemoryProperties *memory,
      const vulkan_prime_image_ops_t *ops,
      PFN_vkGetPhysicalDeviceFormatProperties2 get_format,
      PFN_vkGetPhysicalDeviceImageFormatProperties2 get_image)
{
   if (!vaapi_hevc_vulkan_frame_prepare(out, source, gpu, device, prime_device,
             memory, ops, get_format, get_image))
      return false;
   out->source = *source;
   memset(source, 0, sizeof(*source));
   return true;
}

bool vaapi_hevc_vulkan_frame_barrier(const vaapi_hevc_vulkan_frame_t *frame,
      VkCommandBuffer command, PFN_vkCmdPipelineBarrier barrier,
      unsigned graphics_family, VkPipelineStageFlags stage, bool acquire)
{
   VkImageMemoryBarrier images[2];
   unsigned i;
   if (!frame || !frame->active || !frame->source.active || frame->pending
         || frame->broken || !command || !barrier || !frame->layers[0].image
         || !frame->layers[1].image
         || graphics_family >= VK_QUEUE_FAMILY_FOREIGN_EXT
         || (stage != VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
               && stage != VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT))
      return false;
   for (i = 0; i < 2; i++)
   {
      images[i].sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      images[i].pNext = NULL;
      images[i].subresourceRange.baseMipLevel   = 0;
      images[i].subresourceRange.baseArrayLayer = 0;
      images[i].image                           = frame->layers[i].image;
      images[i].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      images[i].subresourceRange.levelCount =
            images[i].subresourceRange.layerCount = 1;
      images[i].srcQueueFamilyIndex =
            acquire ? VK_QUEUE_FAMILY_FOREIGN_EXT : graphics_family;
      images[i].dstQueueFamilyIndex =
            acquire ? graphics_family : VK_QUEUE_FAMILY_FOREIGN_EXT;
      images[i].oldLayout = acquire ? VK_IMAGE_LAYOUT_GENERAL
                                    : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      images[i].newLayout = acquire ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                                    : VK_IMAGE_LAYOUT_GENERAL;
      images[i].srcAccessMask = acquire ? 0 : VK_ACCESS_SHADER_READ_BIT;
      images[i].dstAccessMask = acquire ? VK_ACCESS_SHADER_READ_BIT : 0;
   }
   barrier(command, acquire ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT : stage,
         acquire ? stage : VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0,
         NULL, 2, images);
   return true;
}

bool vaapi_hevc_vulkan_frame_track(
      vaapi_hevc_vulkan_frame_t *frame, VkFence fence)
{
   if (!frame || !frame->active || !frame->source.active || frame->pending
         || frame->broken || !fence)
      return false;
   frame->fence   = fence;
   frame->pending = true;
   return true;
}

bool vaapi_hevc_vulkan_frame_detach(
      vaapi_hevc_vulkan_frame_t *frame, vaapi_hevc_export_t *returned)
{
   if (!frame || !frame->active || !frame->source.active || frame->pending
         || frame->broken || !returned || returned->active
         || returned == &frame->source)
      return false;
   *returned = frame->source;
   memset(&frame->source, 0, sizeof(frame->source));
   return true;
}

bool vaapi_hevc_vulkan_frame_matches(
      const vaapi_hevc_vulkan_frame_t *frame, const vaapi_hevc_export_t *source)
{
   const VADRMPRIMESurfaceDescriptor *a, *b;
   unsigned i, j;
   if (!frame || !frame->active || frame->broken || !source || !source->active
         || !source->ready || source == &frame->source
         || source->pool != frame->pool || source->slot != frame->slot
         || !source->pool || source->slot >= 32 || !source->ticket)
      return false;
   a = &frame->descriptor;
   b = &source->descriptor;
   if (!a->num_objects || a->num_objects > 4 || a->num_layers != 2
         || a->fourcc != b->fourcc || a->width != b->width
         || a->height != b->height || a->num_objects != b->num_objects
         || a->num_layers != b->num_layers)
      return false;
   for (i = 0; i < a->num_objects; i++)
      if (b->objects[i].fd < 0 || !frame->object_inode[i]
            || frame->object_device[i] != source->object_device[i]
            || frame->object_inode[i] != source->object_inode[i]
            || a->objects[i].size != b->objects[i].size
            || a->objects[i].drm_format_modifier
                     != b->objects[i].drm_format_modifier)
         return false;
   for (i = 0; i < 2; i++)
   {
      if (!a->layers[i].num_planes || a->layers[i].num_planes > 4
            || a->layers[i].drm_format != b->layers[i].drm_format
            || a->layers[i].num_planes != b->layers[i].num_planes)
         return false;
      for (j = 0; j < a->layers[i].num_planes; j++)
         if (a->layers[i].object_index[j] != b->layers[i].object_index[j]
               || a->layers[i].offset[j] != b->layers[i].offset[j]
               || a->layers[i].pitch[j] != b->layers[i].pitch[j])
            return false;
   }
   return true;
}

bool vaapi_hevc_vulkan_frame_adopt(
      vaapi_hevc_vulkan_frame_t *frame, vaapi_hevc_export_t *source)
{
   if (!frame || frame->source.active || frame->pending
         || !vaapi_hevc_vulkan_frame_matches(frame, source))
      return false;
   frame->source = *source;
   memset(source, 0, sizeof(*source));
   return true;
}

int vaapi_hevc_vulkan_frame_poll(
      vaapi_hevc_vulkan_frame_t *frame, PFN_vkGetFenceStatus status)
{
   VkResult result;
   if (!frame || !frame->active || frame->broken || !status)
      return -1;
   if (!frame->pending)
      return 1;
   result = status(frame->device, frame->fence);
   if (result == VK_NOT_READY)
      return 0;
   if (result != VK_SUCCESS)
   {
      frame->broken = true;
      return -1;
   }
   frame->pending = false;
   frame->fence   = VK_NULL_HANDLE;
   return 1;
}

static bool vaapi_hevc_vulkan_frame_returnable(
      const vaapi_hevc_vulkan_frame_t *frame,
      const vaapi_hevc_export_t *returned)
{
   return frame && frame->active && returned && !returned->active
          && returned != &frame->source;
}

static void vaapi_hevc_vulkan_frame_return(
      vaapi_hevc_vulkan_frame_t *frame, vaapi_hevc_export_t *returned)
{
   *returned = frame->source;
   memset(frame, 0, sizeof(*frame));
}

bool vaapi_hevc_vulkan_frame_retire(
      vaapi_hevc_vulkan_frame_t *frame, vaapi_hevc_export_t *returned)
{
   unsigned i;
   if (!vaapi_hevc_vulkan_frame_returnable(frame, returned) || frame->pending
         || frame->broken)
      return false;
   for (i = 0; i < 2; i++)
      vulkan_prime_image_destroy(frame->device, &frame->ops, &frame->layers[i]);
   vaapi_hevc_vulkan_frame_return(frame, returned);
   return true;
}

bool vaapi_hevc_vulkan_frame_after_device_destroy(
      vaapi_hevc_vulkan_frame_t *frame, VkDevice device,
      vaapi_hevc_export_t *returned)
{
   if (!vaapi_hevc_vulkan_frame_returnable(frame, returned) || !device
         || frame->device != device)
      return false;
   vaapi_hevc_vulkan_frame_return(frame, returned);
   return true;
}
