/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_pyrowave_source.h"
#ifdef HAVE_VULKAN
#include "record_pyrowave_player_vulkan.h"
#include "../record/record_pyrowave_receiver.h"
#include <stdlib.h>
#include <string.h>

typedef struct record_pyrowave_source_vk
{
   record_pyrowave_vulkan_t *codec;
   VkDevice device;
   VkImage image;
   VkImageView view;
   VkDeviceMemory memory;
   VkFence fences[RECORD_GPU_QUEUE_SLOTS], retirement;
   uint64_t tickets[RECORD_GPU_QUEUE_SLOTS];
   bool busy[RECORD_GPU_QUEUE_SLOTS];
   unsigned count, pending, width, height;
   bool initialized, pending_codec, retirement_submitted, quiesced;
   PFN_vkCreateImage CreateImage;
   PFN_vkDestroyImage DestroyImage;
   PFN_vkGetImageMemoryRequirements GetImageMemoryRequirements;
   PFN_vkAllocateMemory AllocateMemory;
   PFN_vkFreeMemory FreeMemory;
   PFN_vkBindImageMemory BindImageMemory;
   PFN_vkCreateImageView CreateImageView;
   PFN_vkDestroyImageView DestroyImageView;
   PFN_vkCreateFence CreateFence;
   PFN_vkDestroyFence DestroyFence;
   PFN_vkGetFenceStatus GetFenceStatus;
   PFN_vkResetFences ResetFences;
   PFN_vkCmdPipelineBarrier CmdPipelineBarrier;
   PFN_vkCmdCopyImage CmdCopyImage;
} record_pyrowave_source_vk_t;

static bool record_pyrowave_source_vk_idle(void *user)
{
   record_pyrowave_source_vk_t *s = (record_pyrowave_source_vk_t*)user;
   unsigned i;
   for (i = 0; i < s->count; i++) if (s->busy[i]) return false;
   return true;
}

static bool record_pyrowave_source_vk_poll(void *user)
{
   record_pyrowave_source_vk_t *s = (record_pyrowave_source_vk_t*)user;
   unsigned i;
   bool valid = record_pyrowave_vulkan_poll(s->codec);
   uint64_t retired = record_pyrowave_vulkan_source_retired(s->codec);
   /* Consume adapter loans BEFORE resetting their private completion fences. */
   for (i = 0; i < s->count; i++)
   {
      VkResult result;
      if (!s->busy[i] || s->pending == i) continue;
      /* A second status query could observe completion the adapter missed,
       * then erase its still-borrowed signal. Its retired ticket is the ack. */
      if (s->tickets[i] && s->tickets[i] > retired) continue;
      result = s->tickets[i] ? VK_SUCCESS : s->GetFenceStatus(s->device, s->fences[i]);
      if (result == VK_NOT_READY) continue;
      if (result != VK_SUCCESS || s->ResetFences(s->device, 1, &s->fences[i]) != VK_SUCCESS)
      { valid = false; continue; }
      s->busy[i] = false;
   }
   return valid;
}

static uint64_t record_pyrowave_source_vk_retired(void *user)
{
   return record_pyrowave_vulkan_source_retired(((record_pyrowave_source_vk_t*)user)->codec);
}

static bool record_pyrowave_source_vk_free(void *user)
{
   record_pyrowave_source_vk_t *s = (record_pyrowave_source_vk_t*)user;
   unsigned i;
   if (!s) return true;
   if (!record_pyrowave_source_vk_idle(s) || !record_pyrowave_vulkan_free(s->codec)) return false;
   if (s->retirement) s->DestroyFence(s->device, s->retirement, NULL);
   for (i = 0; i < s->count; i++) if (s->fences[i]) s->DestroyFence(s->device, s->fences[i], NULL);
   if (s->view) s->DestroyImageView(s->device, s->view, NULL);
   if (s->image) s->DestroyImage(s->device, s->image, NULL);
   if (s->memory) s->FreeMemory(s->device, s->memory, NULL);
   free(s);
   return true;
}

#if defined(HAVE_PYROWAVE) && defined(HAVE_THREADS) && defined(HAVE_CONFIGFILE) && !defined(ANDROID)
static bool record_pyrowave_source_vk_quiesce(void *user, void *renderer)
{
   record_pyrowave_source_vk_t *s = (record_pyrowave_source_vk_t*)user;
   if (!s->quiesced)
      s->quiesced = video_vulkan_pyrowave_quiesce(renderer, s->retirement,
            &s->retirement_submitted) > 0;
   return s->quiesced;
}
static bool record_pyrowave_source_vk_abandon(void *user)
{
   record_pyrowave_source_vk_t *s = (record_pyrowave_source_vk_t*)user;
   if (!s || !s->quiesced) return false;
   if (!record_pyrowave_vulkan_abandon(s->codec)) return false;
   s->codec = NULL;
   memset(s->busy, 0, sizeof(s->busy)); s->pending = RECORD_GPU_QUEUE_SLOTS;
   return record_pyrowave_source_vk_free(s);
}

#endif

static bool record_pyrowave_source_vk_image(record_pyrowave_source_vk_t *s,
      const record_pyrowave_source_config_t *config, const VkPhysicalDeviceMemoryProperties *memory)
{
   VkImageCreateInfo image;
   VkImageViewCreateInfo view;
   VkMemoryRequirements requirements;
   VkMemoryAllocateInfo allocation;
   VkFenceCreateInfo fence;
   unsigned i;
   memset(&image, 0, sizeof(image)); image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   image.imageType = VK_IMAGE_TYPE_2D; image.format = (VkFormat)config->format;
   if (image.format == VK_FORMAT_R8G8B8A8_SRGB) image.format = VK_FORMAT_R8G8B8A8_UNORM;
   if (image.format == VK_FORMAT_B8G8R8A8_SRGB) image.format = VK_FORMAT_B8G8R8A8_UNORM;
   image.extent.width = config->full_width; image.extent.height = config->full_height; image.extent.depth = 1;
   image.mipLevels = image.arrayLayers = 1; image.samples = VK_SAMPLE_COUNT_1_BIT;
   image.tiling = VK_IMAGE_TILING_OPTIMAL;
   image.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
   if (s->CreateImage(s->device, &image, NULL, &s->image) != VK_SUCCESS) return false;
   s->GetImageMemoryRequirements(s->device, s->image, &requirements);
   for (i = 0; i < memory->memoryTypeCount; i++)
      if ((requirements.memoryTypeBits & (1u << i))
            && (memory->memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)) break;
   if (i == memory->memoryTypeCount) return false;
   memset(&allocation, 0, sizeof(allocation)); allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocation.allocationSize = requirements.size; allocation.memoryTypeIndex = i;
   if (s->AllocateMemory(s->device, &allocation, NULL, &s->memory) != VK_SUCCESS
         || s->BindImageMemory(s->device, s->image, s->memory, 0) != VK_SUCCESS) return false;
   memset(&view, 0, sizeof(view)); view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view.image = s->image; view.viewType = VK_IMAGE_VIEW_TYPE_2D; view.format = image.format;
   view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   view.subresourceRange.levelCount = view.subresourceRange.layerCount = 1;
   if (s->CreateImageView(s->device, &view, NULL, &s->view) != VK_SUCCESS) return false;
   memset(&fence, 0, sizeof(fence)); fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   if (s->CreateFence(s->device, &fence, NULL, &s->retirement) != VK_SUCCESS) return false;
   for (i = 0; i < s->count; i++)
      if (s->CreateFence(s->device, &fence, NULL, &s->fences[i]) != VK_SUCCESS) return false;
   return true;
}

static record_pyrowave_vulkan_t *record_pyrowave_source_vk_codec(
      const record_pyrowave_source_config_t *config, bool decode)
{
   static const char *const names[3] = { "color.spv", "dwt.spv", "packet.spv" };
   record_pyrowave_vulkan_config_t cfg;
   record_pyrowave_vulkan_t *codec = NULL;
   PFN_vkGetInstanceProcAddr instance_proc;
   PFN_vkGetDeviceProcAddr device_proc;
   PFN_vkGetPhysicalDeviceProperties properties_proc;
   PFN_vkGetPhysicalDeviceMemoryProperties memory_proc;
   PFN_vkGetPhysicalDeviceFormatProperties format_proc;
   PFN_vkGetPhysicalDeviceQueueFamilyProperties families_proc;
   VkPhysicalDeviceProperties properties;
   VkPhysicalDeviceMemoryProperties memory;
   VkFormatProperties format;
   VkQueueFamilyProperties *families = NULL;
   record_pyrowave_shader_t shaders[3];
   uint32_t count = 0;
   unsigned i;
   if (!config || !config->device || !config->physical || !config->instance
         || !config->queue || !config->instance_proc || !config->slots || config->slots > RECORD_GPU_QUEUE_SLOTS) return NULL;
   memset(shaders, 0, sizeof(shaders));
   memset(&cfg, 0, sizeof(cfg));
   instance_proc = (PFN_vkGetInstanceProcAddr)config->instance_proc;
   device_proc = (PFN_vkGetDeviceProcAddr)instance_proc((VkInstance)config->instance, "vkGetDeviceProcAddr");
   properties_proc = (PFN_vkGetPhysicalDeviceProperties)instance_proc((VkInstance)config->instance, "vkGetPhysicalDeviceProperties");
   memory_proc = (PFN_vkGetPhysicalDeviceMemoryProperties)instance_proc((VkInstance)config->instance, "vkGetPhysicalDeviceMemoryProperties");
   format_proc = (PFN_vkGetPhysicalDeviceFormatProperties)instance_proc((VkInstance)config->instance, "vkGetPhysicalDeviceFormatProperties");
   families_proc = (PFN_vkGetPhysicalDeviceQueueFamilyProperties)instance_proc((VkInstance)config->instance, "vkGetPhysicalDeviceQueueFamilyProperties");
   if (!device_proc || !properties_proc || !memory_proc || !format_proc || !families_proc) return NULL;
   properties_proc((VkPhysicalDevice)config->physical, &properties);
   memory_proc((VkPhysicalDevice)config->physical, &memory);
   format_proc((VkPhysicalDevice)config->physical, VK_FORMAT_R32G32B32A32_SFLOAT, &format);
   cfg.rgba32f_features = format.optimalTilingFeatures;
   families_proc((VkPhysicalDevice)config->physical, &count, NULL);
   if (!count || count > 256 || config->family >= count) return NULL;
   families = (VkQueueFamilyProperties*)calloc(count, sizeof(*families));
   if (!families) return NULL;
   families_proc((VkPhysicalDevice)config->physical, &count, families);
   cfg.queue_flags = families[config->family].queueFlags;
   free(families);
   if (!(cfg.queue_flags & VK_QUEUE_COMPUTE_BIT)) return NULL;
   for (i = 0; i < 3; i++)
   {
      if (!record_pyrowave_source_shader(config->directory, names[i], &shaders[i])
            || shaders[i].bytes % 4 || shaders[i].bytes < 20 || ((const uint32_t*)shaders[i].data)[0] != 0x07230203u) goto done;
      cfg.shaders[i] = (const uint32_t*)shaders[i].data; cfg.shader_bytes[i] = shaders[i].bytes;
   }
   cfg.get_proc = device_proc; cfg.device = (VkDevice)config->device; cfg.queue = (VkQueue)config->queue;
   cfg.scale_source = config->scale_source;
   cfg.memory = &memory; cfg.limits = &properties.limits;
   cfg.sequence = config->stream.sequence; cfg.queue_family = config->family;
   cfg.maximum_bytes = config->stream.maximum_frame_bytes; cfg.slots = config->slots;
   cfg.bit_depth = config->stream.bit_depth; cfg.texture_mode = config->texture_mode;
   cfg.texture_width = config->full_width; cfg.texture_height = config->full_height;
   cfg.x = config->x; cfg.y = config->y;
   cfg.decode = decode;
   codec = record_pyrowave_vulkan_new(&cfg);
done:
   for (i = 0; i < 3; i++) record_pyrowave_source_shader_release(&shaders[i]);
   return codec;
}

record_pyrowave_vulkan_t *record_pyrowave_player_vulkan_new(
      const record_pyrowave_source_config_t *config)
{
   return record_pyrowave_source_vk_codec(config, true);
}

bool record_pyrowave_source_vulkan_new(record_pyrowave_source_t *source,
      const record_pyrowave_source_config_t *config)
{
   record_pyrowave_source_vk_t *s;
   PFN_vkGetInstanceProcAddr instance_proc;
   PFN_vkGetDeviceProcAddr device_proc;
   PFN_vkGetPhysicalDeviceMemoryProperties memory_proc;
   VkPhysicalDeviceMemoryProperties memory;
   bool ok = false;
   if (!source || !config || !config->instance || !config->instance_proc
         || !config->device || !config->physical || !config->queue
         || !config->slots || config->slots > RECORD_GPU_QUEUE_SLOTS) return false;
   instance_proc = (PFN_vkGetInstanceProcAddr)config->instance_proc;
   device_proc = (PFN_vkGetDeviceProcAddr)instance_proc((VkInstance)config->instance, "vkGetDeviceProcAddr");
   memory_proc = (PFN_vkGetPhysicalDeviceMemoryProperties)instance_proc((VkInstance)config->instance, "vkGetPhysicalDeviceMemoryProperties");
   if (!device_proc || !memory_proc) return false;
   memory_proc((VkPhysicalDevice)config->physical, &memory);
   s = (record_pyrowave_source_vk_t*)calloc(1, sizeof(*s));
   if (!s) return false;
   s->device = (VkDevice)config->device;
   s->count = config->slots; s->pending = RECORD_GPU_QUEUE_SLOTS;
   s->width = config->full_width; s->height = config->full_height;
#define RPY_SOURCE_VK_LOAD(name) \
   do { s->name = (PFN_vk##name)device_proc(s->device, "vk" #name); if (!s->name) goto done; } while (0)
   RPY_SOURCE_VK_LOAD(CreateImage); RPY_SOURCE_VK_LOAD(DestroyImage);
   RPY_SOURCE_VK_LOAD(GetImageMemoryRequirements); RPY_SOURCE_VK_LOAD(AllocateMemory);
   RPY_SOURCE_VK_LOAD(FreeMemory); RPY_SOURCE_VK_LOAD(BindImageMemory);
   RPY_SOURCE_VK_LOAD(CreateImageView); RPY_SOURCE_VK_LOAD(DestroyImageView);
   RPY_SOURCE_VK_LOAD(CreateFence); RPY_SOURCE_VK_LOAD(DestroyFence);
   RPY_SOURCE_VK_LOAD(GetFenceStatus); RPY_SOURCE_VK_LOAD(ResetFences);
   RPY_SOURCE_VK_LOAD(CmdPipelineBarrier); RPY_SOURCE_VK_LOAD(CmdCopyImage);
#undef RPY_SOURCE_VK_LOAD
   if (!record_pyrowave_source_vk_image(s, config, &memory)) goto done;
   s->codec = record_pyrowave_source_vk_codec(config, false);
   if (!s->codec) goto done;
   memset(source, 0, sizeof(*source)); source->user = s;
   record_pyrowave_vulkan_session_video(s->codec, &source->video);
   source->poll = record_pyrowave_source_vk_poll; source->idle = record_pyrowave_source_vk_idle;
   source->retired = record_pyrowave_source_vk_retired; source->free = record_pyrowave_source_vk_free;
#if defined(HAVE_PYROWAVE) && defined(HAVE_THREADS) && defined(HAVE_CONFIGFILE) && !defined(ANDROID)
   source->quiesce = record_pyrowave_source_vk_quiesce;
   source->abandon = record_pyrowave_source_vk_abandon;
#endif
   ok = true;
done:
   if (!ok) record_pyrowave_source_vk_free(s);
   return ok;
}

static void record_pyrowave_source_vk_barrier(record_pyrowave_source_vk_t *s, VkCommandBuffer commands,
      VkImage image, VkImageLayout before, VkImageLayout after,
      VkAccessFlags source_access, VkAccessFlags target_access,
      VkPipelineStageFlags source_stage, VkPipelineStageFlags target_stage)
{
   VkImageMemoryBarrier barrier;
   memset(&barrier, 0, sizeof(barrier)); barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   barrier.image = image; barrier.oldLayout = before; barrier.newLayout = after;
   barrier.srcAccessMask = source_access; barrier.dstAccessMask = target_access;
   barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier.subresourceRange.levelCount = barrier.subresourceRange.layerCount = 1;
   s->CmdPipelineBarrier(commands, source_stage, target_stage, 0, 0, NULL, 0, NULL, 1, &barrier);
}

int record_pyrowave_source_vulkan_push(record_pyrowave_source_t *source,
      VkCommandBuffer commands, VkImage image, uint64_t timestamp,
      uint64_t duration, uint64_t *ticket, VkFence *completion)
{
   record_pyrowave_source_vk_t *s;
   VkImageCopy copy;
   unsigned slot;
   uint64_t previous;
   int result;
   if (completion) *completion = VK_NULL_HANDLE;
   if (!source || !source->user || !completion || !ticket || !commands || !image
         || !duration || timestamp > (uint64_t)-1 - duration) return RECORD_GPU_QUEUE_ERROR;
   s = (record_pyrowave_source_vk_t*)source->user;
   previous = *ticket;
   if (s->pending != RECORD_GPU_QUEUE_SLOTS) return RECORD_GPU_QUEUE_BUSY;
   for (slot = 0; slot < s->count; slot++) if (!s->busy[slot]) break;
   if (slot == s->count) return RECORD_GPU_QUEUE_BUSY;
   s->pending = slot; s->busy[slot] = true;
   *completion = s->fences[slot];
   /* Swapchain images need not be sampleable. Keep this required copy on
    * GPU, using a non-sRGB view so the codec sees the actual code values. */
   record_pyrowave_source_vk_barrier(s, commands, image,
         VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
   record_pyrowave_source_vk_barrier(s, commands, s->image,
         s->initialized ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, s->initialized ? VK_ACCESS_SHADER_READ_BIT : 0,
         VK_ACCESS_TRANSFER_WRITE_BIT, s->initialized ? VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT);
   memset(&copy, 0, sizeof(copy));
   copy.srcSubresource.aspectMask = copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   copy.srcSubresource.layerCount = copy.dstSubresource.layerCount = 1;
   copy.extent.width = s->width; copy.extent.height = s->height; copy.extent.depth = 1;
   s->CmdCopyImage(commands, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         s->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
   record_pyrowave_source_vk_barrier(s, commands, image,
         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
         VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
   record_pyrowave_source_vk_barrier(s, commands, s->image,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
   s->initialized = true;
   result = record_pyrowave_vulkan_push(s->codec, commands, s->fences[slot], s->view,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, timestamp, duration, ticket);
   s->pending_codec = *ticket != previous;
   s->tickets[slot] = s->pending_codec ? *ticket : 0;
   return result;
}

bool record_pyrowave_source_vulkan_discard(record_pyrowave_source_t *source, VkFence fence)
{
   record_pyrowave_source_vk_t *s = (record_pyrowave_source_vk_t*)source->user;
   if (!fence || s->pending == RECORD_GPU_QUEUE_SLOTS) return true;
   if (fence != s->fences[s->pending]) return false;
   if (s->pending_codec && !record_pyrowave_vulkan_discard(s->codec, fence)) return false;
   s->busy[s->pending] = false; s->pending = RECORD_GPU_QUEUE_SLOTS;
   return true;
}

bool record_pyrowave_source_vulkan_submitted(record_pyrowave_source_t *source, VkFence fence, VkResult result)
{
   record_pyrowave_source_vk_t *s = (record_pyrowave_source_vk_t*)source->user;
   if (!fence || s->pending == RECORD_GPU_QUEUE_SLOTS) return true;
   if (fence != s->fences[s->pending]) return false;
   if (result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY)
   { record_pyrowave_source_vulkan_discard(source, fence); return false; }
   if (result != VK_SUCCESS) return false;
   s->pending = RECORD_GPU_QUEUE_SLOTS;
   return true;
}
#endif
