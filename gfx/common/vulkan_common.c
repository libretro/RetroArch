/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Hans-Kristian Arntzen
 *  Copyright (C) 2016-2019 - Brad Parker
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <retro_assert.h>
#include <dynamic/dylib.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_X11
#ifdef HAVE_XCB
#include <X11/Xlib-xcb.h>
#endif
#endif

#include "vulkan_common.h"
#include <retro_timers.h>
#include "../../configuration.h"
#include "../include/vulkan/vulkan.h"
#include <retro_assert.h>
#include "vksym.h"
#include <libretro_vulkan.h>
#include <retro_math.h>
#include <lists/string_list.h>

#define VENDOR_ID_AMD 0x1002
#define VENDOR_ID_NV 0x10DE
#define VENDOR_ID_INTEL 0x8086

#if defined(_WIN32) || defined(ANDROID)
#define VULKAN_EMULATE_MAILBOX
#endif

static dylib_t                       vulkan_library;
static VkInstance                    cached_instance_vk;
static VkDevice                      cached_device_vk;
static retro_vulkan_destroy_device_t cached_destroy_device_vk;
static struct string_list *vulkan_gpu_list = NULL;

//#define WSI_HARDENING_TEST
#ifdef WSI_HARDENING_TEST
static unsigned wsi_harden_counter = 0;
static unsigned wsi_harden_counter2 = 0;

static void trigger_spurious_error_vkresult(VkResult *res)
{
   ++wsi_harden_counter;
   if ((wsi_harden_counter & 15) == 12)
      *res = VK_ERROR_OUT_OF_DATE_KHR;
   else if ((wsi_harden_counter & 31) == 13)
      *res = VK_ERROR_OUT_OF_DATE_KHR;
   else if ((wsi_harden_counter & 15) == 6)
      *res = VK_ERROR_SURFACE_LOST_KHR;
}

static bool trigger_spurious_error(void)
{
   ++wsi_harden_counter2;
   return ((wsi_harden_counter2 & 15) == 9) || ((wsi_harden_counter2 & 15) == 10);
}
#endif

#ifdef VULKAN_DEBUG
static VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_cb(
      VkDebugReportFlagsEXT flags,
      VkDebugReportObjectTypeEXT objectType,
      uint64_t object,
      size_t location,
      int32_t messageCode,
      const char *pLayerPrefix,
      const char *pMessage,
      void *pUserData)
{
   (void)objectType;
   (void)object;
   (void)location;
   (void)messageCode;
   (void)pUserData;

   if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT)
   {
      RARCH_ERR("[Vulkan]: Error: %s: %s\n",
            pLayerPrefix, pMessage);
   }
#if 0
   else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT)
   {
      RARCH_WARN("[Vulkan]: Warning: %s: %s\n",
            pLayerPrefix, pMessage);
   }
   else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT)
   {
      RARCH_LOG("[Vulkan]: Performance warning: %s: %s\n",
            pLayerPrefix, pMessage);
   }
   else
   {
      RARCH_LOG("[Vulkan]: Information: %s: %s\n",
            pLayerPrefix, pMessage);
   }
#endif

   return VK_FALSE;
}
#endif

void vulkan_emulated_mailbox_deinit(struct vulkan_emulated_mailbox *mailbox)
{
   if (mailbox->thread)
   {
      slock_lock(mailbox->lock);
      mailbox->dead = true;
      scond_signal(mailbox->cond);
      slock_unlock(mailbox->lock);
      sthread_join(mailbox->thread);
   }

   if (mailbox->lock)
      slock_free(mailbox->lock);
   if (mailbox->cond)
      scond_free(mailbox->cond);

   memset(mailbox, 0, sizeof(*mailbox));
}

VkResult vulkan_emulated_mailbox_acquire_next_image(
      struct vulkan_emulated_mailbox *mailbox,
      unsigned *index)
{
   VkResult res;
   if (mailbox->swapchain == VK_NULL_HANDLE)
      return VK_ERROR_OUT_OF_DATE_KHR;

   slock_lock(mailbox->lock);

   if (!mailbox->has_pending_request)
   {
      mailbox->request_acquire = true;
      scond_signal(mailbox->cond);
   }

   mailbox->has_pending_request = true;

   if (mailbox->acquired)
   {
      res = mailbox->result;
      *index = mailbox->index;
      mailbox->has_pending_request = false;
      mailbox->acquired = false;
   }
   else
      res = VK_TIMEOUT;

   slock_unlock(mailbox->lock);
   return res;
}

VkResult vulkan_emulated_mailbox_acquire_next_image_blocking(
      struct vulkan_emulated_mailbox *mailbox,
      unsigned *index)
{
   VkResult res;
   if (mailbox->swapchain == VK_NULL_HANDLE)
      return VK_ERROR_OUT_OF_DATE_KHR;

   slock_lock(mailbox->lock);

   if (!mailbox->has_pending_request)
   {
      mailbox->request_acquire = true;
      scond_signal(mailbox->cond);
   }

   mailbox->has_pending_request = true;

   while (!mailbox->acquired)
      scond_wait(mailbox->cond, mailbox->lock);

   res = mailbox->result;
   if (res == VK_SUCCESS)
      *index = mailbox->index;
   mailbox->has_pending_request = false;
   mailbox->acquired = false;

   slock_unlock(mailbox->lock);
   return res;
}

static void vulkan_emulated_mailbox_loop(void *userdata)
{
   VkFence fence;
   VkFenceCreateInfo info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
   struct vulkan_emulated_mailbox *mailbox = (struct vulkan_emulated_mailbox*)userdata;

   if (!mailbox)
      return;

   vkCreateFence(mailbox->device, &info, NULL, &fence);

   for (;;)
   {
      slock_lock(mailbox->lock);
      while (!mailbox->dead && !mailbox->request_acquire)
         scond_wait(mailbox->cond, mailbox->lock);

      if (mailbox->dead)
      {
         slock_unlock(mailbox->lock);
         break;
      }

      mailbox->request_acquire = false;
      slock_unlock(mailbox->lock);

      mailbox->result = vkAcquireNextImageKHR(mailbox->device, mailbox->swapchain, UINT64_MAX,
            VK_NULL_HANDLE, fence, &mailbox->index);

      /* VK_SUBOPTIMAL_KHR can be returned on Android 10 when prerotate is not dealt with.
       * This is not an error we need to care about, and we'll treat it as SUCCESS. */
      if (mailbox->result == VK_SUBOPTIMAL_KHR)
         mailbox->result = VK_SUCCESS;

      if (mailbox->result == VK_SUCCESS)
         vkWaitForFences(mailbox->device, 1, &fence, true, UINT64_MAX);
      vkResetFences(mailbox->device, 1, &fence);

      if (mailbox->result == VK_SUCCESS)
      {
         slock_lock(mailbox->lock);
         mailbox->acquired = true;
         scond_signal(mailbox->cond);
         slock_unlock(mailbox->lock);
      }
   }

   vkDestroyFence(mailbox->device, fence, NULL);
}

bool vulkan_emulated_mailbox_init(
      struct vulkan_emulated_mailbox *mailbox,
      VkDevice device,
      VkSwapchainKHR swapchain)
{
   memset(mailbox, 0, sizeof(*mailbox));
   mailbox->device    = device;
   mailbox->swapchain = swapchain;

   mailbox->cond      = scond_new();
   if (!mailbox->cond)
      return false;
   mailbox->lock      = slock_new();
   if (!mailbox->lock)
      return false;
   mailbox->thread    = sthread_create(vulkan_emulated_mailbox_loop, mailbox);
   if (!mailbox->thread)
      return false;
   return true;
}

uint32_t vulkan_find_memory_type(
      const VkPhysicalDeviceMemoryProperties *mem_props,
      uint32_t device_reqs, uint32_t host_reqs)
{
   uint32_t i;
   for (i = 0; i < VK_MAX_MEMORY_TYPES; i++)
   {
      if ((device_reqs & (1u << i)) &&
            (mem_props->memoryTypes[i].propertyFlags & host_reqs) == host_reqs)
         return i;
   }

   RARCH_ERR("[Vulkan]: Failed to find valid memory type. This should never happen.");
   abort();
}

uint32_t vulkan_find_memory_type_fallback(
      const VkPhysicalDeviceMemoryProperties *mem_props,
      uint32_t device_reqs, uint32_t host_reqs_first,
      uint32_t host_reqs_second)
{
   uint32_t i;
   for (i = 0; i < VK_MAX_MEMORY_TYPES; i++)
   {
      if ((device_reqs & (1u << i)) &&
            (mem_props->memoryTypes[i].propertyFlags & host_reqs_first) == host_reqs_first)
         return i;
   }

   if (host_reqs_first == 0)
   {
      RARCH_ERR("[Vulkan]: Failed to find valid memory type. This should never happen.");
      abort();
   }

   return vulkan_find_memory_type_fallback(mem_props,
         device_reqs, host_reqs_second, 0);
}

void vulkan_transfer_image_ownership(VkCommandBuffer cmd,
      VkImage image, VkImageLayout layout,
      VkPipelineStageFlags src_stages,
      VkPipelineStageFlags dst_stages,
      uint32_t src_queue_family,
      uint32_t dst_queue_family)
{
   VkImageMemoryBarrier barrier =
   { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

   barrier.srcAccessMask               = 0;
   barrier.dstAccessMask               = 0;
   barrier.oldLayout                   = layout;
   barrier.newLayout                   = layout;
   barrier.srcQueueFamilyIndex         = src_queue_family;
   barrier.dstQueueFamilyIndex         = dst_queue_family;
   barrier.image                       = image;
   barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
   barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

   vkCmdPipelineBarrier(cmd, src_stages, dst_stages,
         false, 0, NULL, 0, NULL, 1, &barrier);
}

void vulkan_copy_staging_to_dynamic(vk_t *vk, VkCommandBuffer cmd,
      struct vk_texture *dynamic,
      struct vk_texture *staging)
{
   VkBufferImageCopy region;

   retro_assert(dynamic->type == VULKAN_TEXTURE_DYNAMIC);
   retro_assert(staging->type == VULKAN_TEXTURE_STAGING);

   vulkan_sync_texture_to_gpu(vk, staging);

   /* We don't have to sync against previous TRANSFER,
    * since we observed the completion by fences.
    *
    * If we have a single texture_optimal, we would need to sync against
    * previous transfers to avoid races.
    *
    * We would also need to optionally maintain extra textures due to
    * changes in resolution, so this seems like the sanest and
    * simplest solution. */
   vulkan_image_layout_transition(vk, cmd, dynamic->image,
         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         0, VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT);

   memset(&region, 0, sizeof(region));
   region.imageExtent.width           = dynamic->width;
   region.imageExtent.height          = dynamic->height;
   region.imageExtent.depth           = 1;
   region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   region.imageSubresource.layerCount = 1;

   vkCmdCopyBufferToImage(cmd,
         staging->buffer,
         dynamic->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         1, &region);

   vulkan_image_layout_transition(vk, cmd,
         dynamic->image,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

   dynamic->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

#ifdef VULKAN_DEBUG_TEXTURE_ALLOC
static VkImage vk_images[4 * 1024];
static unsigned vk_count;

void vulkan_log_textures(void)
{
   unsigned i;
   for (i = 0; i < vk_count; i++)
   {
      RARCH_WARN("[Vulkan]: Found leaked texture %llu.\n",
            (unsigned long long)vk_images[i]);
   }
   vk_count = 0;
}

static unsigned track_seq;
static void vulkan_track_alloc(VkImage image)
{
   vk_images[vk_count++] = image;
   RARCH_LOG("[Vulkan]: Alloc %llu (%u).\n",
         (unsigned long long)image, track_seq);
   track_seq++;
}

static void vulkan_track_dealloc(VkImage image)
{
   unsigned i;
   for (i = 0; i < vk_count; i++)
   {
      if (image == vk_images[i])
      {
         vk_count--;
         memmove(vk_images + i, vk_images + 1 + i,
               sizeof(VkImage) * (vk_count - i));
         return;
      }
   }
   retro_assert(0 && "Couldn't find VkImage in dealloc!");
}
#endif

void vulkan_sync_texture_to_gpu(vk_t *vk, const struct vk_texture *tex)
{
   VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
   if (!tex || !tex->need_manual_cache_management || tex->memory == VK_NULL_HANDLE)
      return;

   range.memory = tex->memory;
   range.offset = 0;
   range.size = VK_WHOLE_SIZE;
   vkFlushMappedMemoryRanges(vk->context->device, 1, &range);
}

void vulkan_sync_texture_to_cpu(vk_t *vk, const struct vk_texture *tex)
{
   VkMappedMemoryRange range = { VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
   if (!tex || !tex->need_manual_cache_management || tex->memory == VK_NULL_HANDLE)
      return;

   range.memory = tex->memory;
   range.offset = 0;
   range.size   = VK_WHOLE_SIZE;
   vkInvalidateMappedMemoryRanges(vk->context->device, 1, &range);
}

static unsigned vulkan_num_miplevels(unsigned width, unsigned height)
{
   unsigned size = MAX(width, height);
   unsigned levels = 0;
   while (size)
   {
      levels++;
      size >>= 1;
   }
   return levels;
}

struct vk_texture vulkan_create_texture(vk_t *vk,
      struct vk_texture *old,
      unsigned width, unsigned height,
      VkFormat format,
      const void *initial,
      const VkComponentMapping *swizzle,
      enum vk_texture_type type)
{
   unsigned i;
   struct vk_texture tex;
   VkMemoryRequirements mem_reqs;
   VkSubresourceLayout layout;
   VkDevice device                      = vk->context->device;
   VkImageCreateInfo info               = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
   VkBufferCreateInfo buffer_info       = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
   VkImageViewCreateInfo view           = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
   VkMemoryAllocateInfo alloc           = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
   VkImageSubresource subresource       = { VK_IMAGE_ASPECT_COLOR_BIT };
   VkCommandBufferAllocateInfo cmd_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
   VkSubmitInfo submit_info             = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
   VkCommandBufferBeginInfo begin_info  = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };

   memset(&tex, 0, sizeof(tex));

   info.imageType     = VK_IMAGE_TYPE_2D;
   info.format        = format;
   info.extent.width  = width;
   info.extent.height = height;
   info.extent.depth  = 1;
   info.arrayLayers   = 1;
   info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

   buffer_info.size        = width * height * vulkan_format_to_bpp(format);
   buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   /* For simplicity, always build mipmaps for
    * static textures, samplers can be used to enable it dynamically.
    */
   if (type == VULKAN_TEXTURE_STATIC)
   {
      info.mipLevels  = vulkan_num_miplevels(width, height);
      tex.mipmap      = true;
   }
   else
      info.mipLevels  = 1;

   info.samples       = VK_SAMPLE_COUNT_1_BIT;

   if (type == VULKAN_TEXTURE_STREAMED)
   {
      VkFormatProperties format_properties;
      const VkFormatFeatureFlags required = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
         VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;

      vkGetPhysicalDeviceFormatProperties(
            vk->context->gpu, format, &format_properties);

      if ((format_properties.linearTilingFeatures & required) != required)
      {
         RARCH_LOG("[Vulkan]: GPU does not support using linear images as textures. Falling back to copy path.\n");
         type = VULKAN_TEXTURE_STAGING;
      }
   }

   switch (type)
   {
      case VULKAN_TEXTURE_STATIC:
         retro_assert(initial && "Static textures must have initial data.\n");
         info.tiling        = VK_IMAGE_TILING_OPTIMAL;
         info.usage         = VK_IMAGE_USAGE_SAMPLED_BIT |
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
         info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
         break;

      case VULKAN_TEXTURE_DYNAMIC:
         retro_assert(!initial && "Dynamic textures must not have initial data.\n");
         info.tiling        = VK_IMAGE_TILING_OPTIMAL;
         info.usage         = VK_IMAGE_USAGE_SAMPLED_BIT |
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
         info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
         break;

      case VULKAN_TEXTURE_STREAMED:
         info.usage         = VK_IMAGE_USAGE_SAMPLED_BIT |
                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
         info.tiling        = VK_IMAGE_TILING_LINEAR;
         info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
         break;

      case VULKAN_TEXTURE_STAGING:
         buffer_info.usage  = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
         info.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
         info.tiling        = VK_IMAGE_TILING_LINEAR;
         break;

      case VULKAN_TEXTURE_READBACK:
         buffer_info.usage  = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
         info.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
         info.tiling        = VK_IMAGE_TILING_LINEAR;
         break;
   }

   if (type != VULKAN_TEXTURE_STAGING && type != VULKAN_TEXTURE_READBACK)
   {
      vkCreateImage(device, &info, NULL, &tex.image);
#if 0
      vulkan_track_alloc(tex.image);
#endif
      vkGetImageMemoryRequirements(device, tex.image, &mem_reqs);
   }
   else
   {
      /* Linear staging textures are not guaranteed to be supported,
       * use buffers instead. */
      vkCreateBuffer(device, &buffer_info, NULL, &tex.buffer);
      vkGetBufferMemoryRequirements(device, tex.buffer, &mem_reqs);
   }
   alloc.allocationSize = mem_reqs.size;

   switch (type)
   {
      case VULKAN_TEXTURE_STATIC:
      case VULKAN_TEXTURE_DYNAMIC:
         alloc.memoryTypeIndex = vulkan_find_memory_type_fallback(
               &vk->context->memory_properties,
               mem_reqs.memoryTypeBits,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);
         break;

      default:
         /* Try to find a memory type which is cached, even if it means manual cache management. */
         alloc.memoryTypeIndex = vulkan_find_memory_type_fallback(
               &vk->context->memory_properties,
               mem_reqs.memoryTypeBits,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
               VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

         tex.need_manual_cache_management =
            (vk->context->memory_properties.memoryTypes[alloc.memoryTypeIndex].propertyFlags &
             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0;
         break;
   }

   /* If the texture is STREAMED and it's not DEVICE_LOCAL, we expect to hit a slower path,
    * so fallback to copy path. */
   if (type == VULKAN_TEXTURE_STREAMED &&
         (vk->context->memory_properties.memoryTypes[alloc.memoryTypeIndex].propertyFlags &
          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == 0)
   {
      /* Recreate texture but for STAGING this time ... */
      RARCH_LOG("[Vulkan]: GPU supports linear images as textures, but not DEVICE_LOCAL. Falling back to copy path.\n");
      type = VULKAN_TEXTURE_STAGING;
      vkDestroyImage(device, tex.image, NULL);
      tex.image          = (VkImage)NULL;
      info.initialLayout = VK_IMAGE_LAYOUT_GENERAL;

      buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
      vkCreateBuffer(device, &buffer_info, NULL, &tex.buffer);
      vkGetBufferMemoryRequirements(device, tex.buffer, &mem_reqs);

      alloc.allocationSize  = mem_reqs.size;
      alloc.memoryTypeIndex = vulkan_find_memory_type_fallback(
            &vk->context->memory_properties,
            mem_reqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
            VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   }

   /* We're not reusing the objects themselves. */
   if (old && old->view != VK_NULL_HANDLE)
      vkDestroyImageView(vk->context->device, old->view, NULL);
   if (old && old->image != VK_NULL_HANDLE)
   {
      vkDestroyImage(vk->context->device, old->image, NULL);
#ifdef VULKAN_DEBUG_TEXTURE_ALLOC
      vulkan_track_dealloc(old->image);
#endif
   }
   if (old && old->buffer != VK_NULL_HANDLE)
      vkDestroyBuffer(vk->context->device, old->buffer, NULL);

   /* We can pilfer the old memory and move it over to the new texture. */
   if (old &&
         old->memory_size >= mem_reqs.size &&
         old->memory_type == alloc.memoryTypeIndex)
   {
      tex.memory      = old->memory;
      tex.memory_size = old->memory_size;
      tex.memory_type = old->memory_type;

      if (old->mapped)
         vkUnmapMemory(device, old->memory);

      old->memory     = VK_NULL_HANDLE;
   }
   else
   {
      vkAllocateMemory(device, &alloc, NULL, &tex.memory);
      tex.memory_size = alloc.allocationSize;
      tex.memory_type = alloc.memoryTypeIndex;
   }

   if (old)
   {
      if (old->memory != VK_NULL_HANDLE)
         vkFreeMemory(device, old->memory, NULL);
      memset(old, 0, sizeof(*old));
   }

   if (tex.image)
      vkBindImageMemory(device, tex.image, tex.memory, 0);
   if (tex.buffer)
      vkBindBufferMemory(device, tex.buffer, tex.memory, 0);

   if (type != VULKAN_TEXTURE_STAGING && type != VULKAN_TEXTURE_READBACK)
   {
      view.image                       = tex.image;
      view.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
      view.format                      = format;
      if (swizzle)
         view.components               = *swizzle;
      else
      {
         view.components.r             = VK_COMPONENT_SWIZZLE_R;
         view.components.g             = VK_COMPONENT_SWIZZLE_G;
         view.components.b             = VK_COMPONENT_SWIZZLE_B;
         view.components.a             = VK_COMPONENT_SWIZZLE_A;
      }
      view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      view.subresourceRange.levelCount = info.mipLevels;
      view.subresourceRange.layerCount = 1;

      vkCreateImageView(device, &view, NULL, &tex.view);
   }
   else
      tex.view = VK_NULL_HANDLE;

   if (tex.image && info.tiling == VK_IMAGE_TILING_LINEAR)
      vkGetImageSubresourceLayout(device, tex.image, &subresource, &layout);
   else if (tex.buffer)
   {
      layout.offset   = 0;
      layout.size     = buffer_info.size;
      layout.rowPitch = width * vulkan_format_to_bpp(format);
   }
   else
      memset(&layout, 0, sizeof(layout));

   tex.stride = layout.rowPitch;
   tex.offset = layout.offset;
   tex.size   = layout.size;
   tex.layout = info.initialLayout;

   tex.width  = width;
   tex.height = height;
   tex.format = format;
   tex.type   = type;

   if (initial && (type == VULKAN_TEXTURE_STREAMED || type == VULKAN_TEXTURE_STAGING))
   {
      unsigned y;
      uint8_t *dst       = NULL;
      const uint8_t *src = NULL;
      void *ptr          = NULL;
      unsigned bpp       = vulkan_format_to_bpp(tex.format);
      unsigned stride    = tex.width * bpp;

      vkMapMemory(device, tex.memory, tex.offset, tex.size, 0, &ptr);

      dst                = (uint8_t*)ptr;
      src                = (const uint8_t*)initial;
      for (y = 0; y < tex.height; y++, dst += tex.stride, src += stride)
         memcpy(dst, src, width * bpp);

      vulkan_sync_texture_to_gpu(vk, &tex);
      vkUnmapMemory(device, tex.memory);
   }
   else if (initial && type == VULKAN_TEXTURE_STATIC)
   {
      VkBufferImageCopy region;
      VkCommandBuffer staging;
      struct vk_texture tmp       = vulkan_create_texture(vk, NULL,
            width, height, format, initial, NULL, VULKAN_TEXTURE_STAGING);

      cmd_info.commandPool        = vk->staging_pool;
      cmd_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
      cmd_info.commandBufferCount = 1;

      vkAllocateCommandBuffers(vk->context->device, &cmd_info, &staging);

      begin_info.flags            = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

      vkBeginCommandBuffer(staging, &begin_info);

      /* If doing mipmapping on upload, keep in general so we can easily do transfers to
       * and transfers from the images without having to
       * mess around with lots of extra transitions at per-level granularity.
       */
      vulkan_image_layout_transition(vk,
            staging,
            tex.image,
            VK_IMAGE_LAYOUT_UNDEFINED,
            tex.mipmap ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0, VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT);

      memset(&region, 0, sizeof(region));
      region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      region.imageSubresource.layerCount = 1;
      region.imageExtent.width           = width;
      region.imageExtent.height          = height;
      region.imageExtent.depth           = 1;

      vkCmdCopyBufferToImage(staging,
            tmp.buffer,
            tex.image,
            tex.mipmap ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &region);

      if (tex.mipmap)
      {
         for (i = 1; i < info.mipLevels; i++)
         {
            VkImageBlit blit_region;
            unsigned src_width                        = MAX(width >> (i - 1), 1);
            unsigned src_height                       = MAX(height >> (i - 1), 1);
            unsigned target_width                     = MAX(width >> i, 1);
            unsigned target_height                    = MAX(height >> i, 1);
            memset(&blit_region, 0, sizeof(blit_region));

            blit_region.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            blit_region.srcSubresource.mipLevel       = i - 1;
            blit_region.srcSubresource.baseArrayLayer = 0;
            blit_region.srcSubresource.layerCount     = 1;
            blit_region.dstSubresource                = blit_region.srcSubresource;
            blit_region.dstSubresource.mipLevel       = i;
            blit_region.srcOffsets[1].x               = src_width;
            blit_region.srcOffsets[1].y               = src_height;
            blit_region.srcOffsets[1].z               = 1;
            blit_region.dstOffsets[1].x               = target_width;
            blit_region.dstOffsets[1].y               = target_height;
            blit_region.dstOffsets[1].z               = 1;

            /* Only injects execution and memory barriers,
             * not actual transition. */
            vulkan_image_layout_transition(vk, staging, tex.image,
                  VK_IMAGE_LAYOUT_GENERAL,
                  VK_IMAGE_LAYOUT_GENERAL,
                  VK_ACCESS_TRANSFER_WRITE_BIT,
                  VK_ACCESS_TRANSFER_READ_BIT,
                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                  VK_PIPELINE_STAGE_TRANSFER_BIT);

            vkCmdBlitImage(staging,
                  tex.image, VK_IMAGE_LAYOUT_GENERAL,
                  tex.image, VK_IMAGE_LAYOUT_GENERAL,
                  1, &blit_region, VK_FILTER_LINEAR);
         }

         /* Complete our texture. */
         vulkan_image_layout_transition(vk, staging, tex.image,
               VK_IMAGE_LAYOUT_GENERAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_ACCESS_TRANSFER_WRITE_BIT,
               VK_ACCESS_SHADER_READ_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
      }
      else
      {
         vulkan_image_layout_transition(vk, staging, tex.image,
               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_ACCESS_TRANSFER_WRITE_BIT,
               VK_ACCESS_SHADER_READ_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
      }

      vkEndCommandBuffer(staging);
      submit_info.commandBufferCount = 1;
      submit_info.pCommandBuffers    = &staging;

#ifdef HAVE_THREADS
      slock_lock(vk->context->queue_lock);
#endif
      vkQueueSubmit(vk->context->queue,
            1, &submit_info, VK_NULL_HANDLE);

      /* TODO: Very crude, but texture uploads only happen
       * during init, so waiting for GPU to complete transfer
       * and blocking isn't a big deal. */
      vkQueueWaitIdle(vk->context->queue);
#ifdef HAVE_THREADS
      slock_unlock(vk->context->queue_lock);
#endif

      vkFreeCommandBuffers(vk->context->device,
            vk->staging_pool, 1, &staging);
      vulkan_destroy_texture(
            vk->context->device, &tmp);
      tex.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   }
   return tex;
}

void vulkan_destroy_texture(
      VkDevice device,
      struct vk_texture *tex)
{
   if (tex->mapped)
      vkUnmapMemory(device, tex->memory);
   if (tex->view)
      vkDestroyImageView(device, tex->view, NULL);
   if (tex->image)
      vkDestroyImage(device, tex->image, NULL);
   if (tex->buffer)
      vkDestroyBuffer(device, tex->buffer, NULL);
   if (tex->memory)
      vkFreeMemory(device, tex->memory, NULL);

#ifdef VULKAN_DEBUG_TEXTURE_ALLOC
   if (tex->image)
      vulkan_track_dealloc(tex->image);
#endif
   memset(tex, 0, sizeof(*tex));
}

static void vulkan_write_quad_descriptors(
      VkDevice device,
      VkDescriptorSet set,
      VkBuffer buffer,
      VkDeviceSize offset,
      VkDeviceSize range,
      const struct vk_texture *texture,
      VkSampler sampler)
{
   VkDescriptorBufferInfo buffer_info;
   VkWriteDescriptorSet write      = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

   buffer_info.buffer              = buffer;
   buffer_info.offset              = offset;
   buffer_info.range               = range;

   write.dstSet                    = set;
   write.dstBinding                = 0;
   write.descriptorCount           = 1;
   write.descriptorType            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   write.pBufferInfo               = &buffer_info;
   vkUpdateDescriptorSets(device, 1, &write, 0, NULL);

   if (texture)
   {
      VkDescriptorImageInfo image_info;

      image_info.sampler              = sampler;
      image_info.imageView            = texture->view;
      image_info.imageLayout          = texture->layout;

      write.dstSet                    = set;
      write.dstBinding                = 1;
      write.descriptorCount           = 1;
      write.descriptorType            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
      write.pImageInfo                = &image_info;
      vkUpdateDescriptorSets(device, 1, &write, 0, NULL);
   }
}

void vulkan_transition_texture(vk_t *vk, VkCommandBuffer cmd, struct vk_texture *texture)
{
   if (!texture->image)
      return;

   /* Transition to GENERAL layout for linear streamed textures.
    * We're using linear textures here, so only
    * GENERAL layout is supported.
    * If we're already in GENERAL, add a host -> shader read memory barrier
    * to invalidate texture caches.
    */
   if (texture->layout != VK_IMAGE_LAYOUT_PREINITIALIZED &&
       texture->layout != VK_IMAGE_LAYOUT_GENERAL)
      return;

   switch (texture->type)
   {
      case VULKAN_TEXTURE_STREAMED:
         vulkan_image_layout_transition(vk, cmd, texture->image,
               texture->layout, VK_IMAGE_LAYOUT_GENERAL,
               VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
               VK_PIPELINE_STAGE_HOST_BIT,
               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
         break;

      default:
         retro_assert(0 && "Attempting to transition invalid texture type.\n");
         break;
   }
   texture->layout = VK_IMAGE_LAYOUT_GENERAL;
}

static void vulkan_check_dynamic_state(vk_t *vk)
{
   VkRect2D sci;

   if (vk->tracker.use_scissor)
      sci = vk->tracker.scissor;
   else
   {
      /* No scissor -> viewport */
      sci.offset.x      = vk->vp.x;
      sci.offset.y      = vk->vp.y;
      sci.extent.width  = vk->vp.width;
      sci.extent.height = vk->vp.height;
   }

   vkCmdSetViewport(vk->cmd, 0, 1, &vk->vk_vp);
   vkCmdSetScissor (vk->cmd, 0, 1, &sci);

   vk->tracker.dirty &= ~VULKAN_DIRTY_DYNAMIC_BIT;
}

void vulkan_draw_triangles(vk_t *vk, const struct vk_draw_triangles *call)
{
   if (call->texture)
      vulkan_transition_texture(vk, vk->cmd, call->texture);

   if (call->pipeline != vk->tracker.pipeline)
   {
      vkCmdBindPipeline(vk->cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS, call->pipeline);
      vk->tracker.pipeline = call->pipeline;

      /* Changing pipeline invalidates dynamic state. */
      vk->tracker.dirty |= VULKAN_DIRTY_DYNAMIC_BIT;
   }

   if (vk->tracker.dirty & VULKAN_DIRTY_DYNAMIC_BIT)
      vulkan_check_dynamic_state(vk);

   /* Upload descriptors */
   {
      VkDescriptorSet set;

      /* Upload UBO */
      struct vk_buffer_range range;
      if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->ubo,
               call->uniform_size, &range))
         return;

      memcpy(range.data, call->uniform, call->uniform_size);

      set = vulkan_descriptor_manager_alloc(
            vk->context->device,
            &vk->chain->descriptor_manager);

      vulkan_write_quad_descriptors(
            vk->context->device,
            set,
            range.buffer,
            range.offset,
            call->uniform_size,
            call->texture,
            call->sampler);

      vkCmdBindDescriptorSets(vk->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            vk->pipelines.layout, 0,
            1, &set, 0, NULL);

      vk->tracker.view = VK_NULL_HANDLE;
      vk->tracker.sampler = VK_NULL_HANDLE;
      memset(&vk->tracker.mvp, 0, sizeof(vk->tracker.mvp));
   }

   /* VBO is already uploaded. */
   vkCmdBindVertexBuffers(vk->cmd, 0, 1,
         &call->vbo->buffer, &call->vbo->offset);

   /* Draw the quad */
   vkCmdDraw(vk->cmd, call->vertices, 1, 0, 0);
}

void vulkan_draw_quad(vk_t *vk, const struct vk_draw_quad *quad)
{
   vulkan_transition_texture(vk, vk->cmd, quad->texture);

   if (quad->pipeline != vk->tracker.pipeline)
   {
      vkCmdBindPipeline(vk->cmd,
            VK_PIPELINE_BIND_POINT_GRAPHICS, quad->pipeline);

      vk->tracker.pipeline = quad->pipeline;
      /* Changing pipeline invalidates dynamic state. */
      vk->tracker.dirty   |= VULKAN_DIRTY_DYNAMIC_BIT;
   }

   if (vk->tracker.dirty & VULKAN_DIRTY_DYNAMIC_BIT)
      vulkan_check_dynamic_state(vk);

   /* Upload descriptors */
   {
      VkDescriptorSet set;
      struct vk_buffer_range range;

      if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->ubo,
               sizeof(*quad->mvp), &range))
         return;

      if (
               string_is_equal_fast(quad->mvp,
                  &vk->tracker.mvp, sizeof(*quad->mvp))
            || quad->texture->view != vk->tracker.view
            || quad->sampler != vk->tracker.sampler)
      {
         /* Upload UBO */
         struct vk_buffer_range range;

         if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->ubo,
                  sizeof(*quad->mvp), &range))
            return;

         memcpy(range.data, quad->mvp, sizeof(*quad->mvp));

         set = vulkan_descriptor_manager_alloc(
               vk->context->device,
               &vk->chain->descriptor_manager);

         vulkan_write_quad_descriptors(
               vk->context->device,
               set,
               range.buffer,
               range.offset,
               sizeof(*quad->mvp),
               quad->texture,
               quad->sampler);

         vkCmdBindDescriptorSets(vk->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
               vk->pipelines.layout, 0,
               1, &set, 0, NULL);

         vk->tracker.view    = quad->texture->view;
         vk->tracker.sampler = quad->sampler;
         vk->tracker.mvp     = *quad->mvp;
      }
   }

   /* Upload VBO */
   {
      struct vk_buffer_range range;
      if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->vbo,
               6 * sizeof(struct vk_vertex), &range))
         return;

      vulkan_write_quad_vbo((struct vk_vertex*)range.data,
            0.0f, 0.0f, 1.0f, 1.0f,
            0.0f, 0.0f, 1.0f, 1.0f,
            &quad->color);

      vkCmdBindVertexBuffers(vk->cmd, 0, 1,
            &range.buffer, &range.offset);
   }

   /* Draw the quad */
   vkCmdDraw(vk->cmd, 6, 1, 0, 0);
}

void vulkan_image_layout_transition(
      vk_t *vk,
      VkCommandBuffer cmd, VkImage image,
      VkImageLayout old_layout,
      VkImageLayout new_layout,
      VkAccessFlags srcAccess,
      VkAccessFlags dstAccess,
      VkPipelineStageFlags srcStages,
      VkPipelineStageFlags dstStages)
{
   VkImageMemoryBarrier barrier        =
   { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

   barrier.srcAccessMask               = srcAccess;
   barrier.dstAccessMask               = dstAccess;
   barrier.oldLayout                   = old_layout;
   barrier.newLayout                   = new_layout;
   barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
   barrier.image                       = image;
   barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
   barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

   vkCmdPipelineBarrier(cmd,
         srcStages,
         dstStages,
         0,
         0, NULL,
         0, NULL,
         1, &barrier);
}

void vulkan_image_layout_transition_levels(
      VkCommandBuffer cmd, VkImage image, uint32_t levels,
      VkImageLayout old_layout, VkImageLayout new_layout,
      VkAccessFlags src_access, VkAccessFlags dst_access,
      VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages)
{
   VkImageMemoryBarrier barrier        = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };

   barrier.srcAccessMask               = src_access;
   barrier.dstAccessMask               = dst_access;
   barrier.oldLayout                   = old_layout;
   barrier.newLayout                   = new_layout;
   barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
   barrier.image                       = image;
   barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier.subresourceRange.levelCount = levels;
   barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

   vkCmdPipelineBarrier(cmd,
         src_stages,
         dst_stages,
         false,
         0, NULL,
         0, NULL,
         1, &barrier);
}

struct vk_buffer vulkan_create_buffer(
      const struct vulkan_context *context,
      size_t size, VkBufferUsageFlags usage)
{
   struct vk_buffer buffer;
   VkMemoryRequirements mem_reqs;
   VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
   VkBufferCreateInfo info    = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };

   info.size                  = size;
   info.usage                 = usage;
   info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
   vkCreateBuffer(context->device, &info, NULL, &buffer.buffer);

   vkGetBufferMemoryRequirements(context->device, buffer.buffer, &mem_reqs);

   alloc.allocationSize       = mem_reqs.size;
   alloc.memoryTypeIndex      = vulkan_find_memory_type(
         &context->memory_properties,
         mem_reqs.memoryTypeBits,
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
   vkAllocateMemory(context->device, &alloc, NULL, &buffer.memory);
   vkBindBufferMemory(context->device, buffer.buffer, buffer.memory, 0);

   buffer.size = size;

   vkMapMemory(context->device,
         buffer.memory, 0, buffer.size, 0, &buffer.mapped);
   return buffer;
}

void vulkan_destroy_buffer(
      VkDevice device,
      struct vk_buffer *buffer)
{
   vkUnmapMemory(device, buffer->memory);
   vkFreeMemory(device, buffer->memory, NULL);

   vkDestroyBuffer(device, buffer->buffer, NULL);

   memset(buffer, 0, sizeof(*buffer));
}

static struct vk_descriptor_pool *vulkan_alloc_descriptor_pool(
      VkDevice device,
      const struct vk_descriptor_manager *manager)
{
   unsigned i;
   VkDescriptorPoolCreateInfo pool_info   = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
   VkDescriptorSetAllocateInfo alloc_info = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };

   struct vk_descriptor_pool *pool        =
      (struct vk_descriptor_pool*)calloc(1, sizeof(*pool));
   if (!pool)
      return NULL;

   pool_info.maxSets       = VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS;
   pool_info.poolSizeCount = manager->num_sizes;
   pool_info.pPoolSizes    = manager->sizes;
   pool_info.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

   vkCreateDescriptorPool(device, &pool_info, NULL, &pool->pool);

   /* Just allocate all descriptor sets up front. */
   alloc_info.descriptorPool     = pool->pool;
   alloc_info.descriptorSetCount = 1;
   alloc_info.pSetLayouts        = &manager->set_layout;

   for (i = 0; i < VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS; i++)
      vkAllocateDescriptorSets(device, &alloc_info, &pool->sets[i]);

   return pool;
}

VkDescriptorSet vulkan_descriptor_manager_alloc(
      VkDevice device, struct vk_descriptor_manager *manager)
{
   if (manager->count < VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS)
      return manager->current->sets[manager->count++];

   while (manager->current->next)
   {
      manager->current = manager->current->next;
      manager->count   = 0;
      return manager->current->sets[manager->count++];
   }

   manager->current->next = vulkan_alloc_descriptor_pool(device, manager);
   retro_assert(manager->current->next);

   manager->current = manager->current->next;
   manager->count   = 0;
   return manager->current->sets[manager->count++];
}

struct vk_descriptor_manager vulkan_create_descriptor_manager(
      VkDevice device,
      const VkDescriptorPoolSize *sizes,
      unsigned num_sizes,
      VkDescriptorSetLayout set_layout)
{
   struct vk_descriptor_manager manager;
   memset(&manager, 0, sizeof(manager));
   retro_assert(num_sizes <= VULKAN_MAX_DESCRIPTOR_POOL_SIZES);
   memcpy(manager.sizes, sizes, num_sizes * sizeof(*sizes));
   manager.num_sizes  = num_sizes;
   manager.set_layout = set_layout;

   manager.head       = vulkan_alloc_descriptor_pool(device, &manager);
   retro_assert(manager.head);
   return manager;
}

void vulkan_destroy_descriptor_manager(
      VkDevice device,
      struct vk_descriptor_manager *manager)
{
   struct vk_descriptor_pool *node = manager->head;

   while (node)
   {
      struct vk_descriptor_pool *next = node->next;

      vkFreeDescriptorSets(device, node->pool,
            VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS, node->sets);
      vkDestroyDescriptorPool(device, node->pool, NULL);

      free(node);
      node = next;
   }

   memset(manager, 0, sizeof(*manager));
}

static void vulkan_buffer_chain_step(struct vk_buffer_chain *chain)
{
   chain->current = chain->current->next;
   chain->offset  = 0;
}

static bool vulkan_buffer_chain_suballoc(struct vk_buffer_chain *chain,
      size_t size, struct vk_buffer_range *range)
{
   VkDeviceSize next_offset = chain->offset + size;
   if (next_offset <= chain->current->buffer.size)
   {
      range->data   = (uint8_t*)chain->current->buffer.mapped + chain->offset;
      range->buffer = chain->current->buffer.buffer;
      range->offset = chain->offset;
      chain->offset = (next_offset + chain->alignment - 1)
         & ~(chain->alignment - 1);

      return true;
   }

   return false;
}

static struct vk_buffer_node *vulkan_buffer_chain_alloc_node(
      const struct vulkan_context *context,
      size_t size, VkBufferUsageFlags usage)
{
   struct vk_buffer_node *node = (struct vk_buffer_node*)
      calloc(1, sizeof(*node));
   if (!node)
      return NULL;

   node->buffer = vulkan_create_buffer(
         context, size, usage);
   return node;
}

struct vk_buffer_chain vulkan_buffer_chain_init(
      VkDeviceSize block_size,
      VkDeviceSize alignment,
      VkBufferUsageFlags usage)
{
   struct vk_buffer_chain chain;

   chain.block_size = block_size;
   chain.alignment  = alignment;
   chain.offset     = 0;
   chain.usage      = usage;
   chain.head       = NULL;
   chain.current    = NULL;

   return chain;
}

bool vulkan_buffer_chain_alloc(const struct vulkan_context *context,
      struct vk_buffer_chain *chain,
      size_t size, struct vk_buffer_range *range)
{
   if (!chain->head)
   {
      chain->head = vulkan_buffer_chain_alloc_node(context,
            chain->block_size, chain->usage);
      if (!chain->head)
         return false;

      chain->current = chain->head;
      chain->offset = 0;
   }

   if (vulkan_buffer_chain_suballoc(chain, size, range))
      return true;

   /* We've exhausted the current chain, traverse list until we
    * can find a block we can use. Usually, we just step once. */
   while (chain->current->next)
   {
      vulkan_buffer_chain_step(chain);
      if (vulkan_buffer_chain_suballoc(chain, size, range))
         return true;
   }

   /* We have to allocate a new node, might allocate larger
    * buffer here than block_size in case we have
    * a very large allocation. */
   if (size < chain->block_size)
      size = chain->block_size;

   chain->current->next = vulkan_buffer_chain_alloc_node(
         context, size, chain->usage);
   if (!chain->current->next)
      return false;

   vulkan_buffer_chain_step(chain);
   /* This cannot possibly fail. */
   retro_assert(vulkan_buffer_chain_suballoc(chain, size, range));
   return true;
}

void vulkan_buffer_chain_free(
      VkDevice device,
      struct vk_buffer_chain *chain)
{
   struct vk_buffer_node *node = chain->head;
   while (node)
   {
      struct vk_buffer_node *next = node->next;
      vulkan_destroy_buffer(device, &node->buffer);

      free(node);
      node = next;
   }
   memset(chain, 0, sizeof(*chain));
}

static bool vulkan_load_instance_symbols(gfx_ctx_vulkan_data_t *vk)
{
   if (!vulkan_symbol_wrapper_load_core_instance_symbols(vk->context.instance))
      return false;

   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance, vkDestroySurfaceKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance, vkGetPhysicalDeviceSurfaceSupportKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance, vkGetPhysicalDeviceSurfaceCapabilitiesKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance, vkGetPhysicalDeviceSurfaceFormatsKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance, vkGetPhysicalDeviceSurfacePresentModesKHR);
   return true;
}

static bool vulkan_load_device_symbols(gfx_ctx_vulkan_data_t *vk)
{
   if (!vulkan_symbol_wrapper_load_core_device_symbols(vk->context.device))
      return false;

   VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(vk->context.device, vkCreateSwapchainKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(vk->context.device, vkDestroySwapchainKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(vk->context.device, vkGetSwapchainImagesKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(vk->context.device, vkAcquireNextImageKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_DEVICE_EXTENSION_SYMBOL(vk->context.device, vkQueuePresentKHR);
   return true;
}

static bool vulkan_find_extensions(const char **exts, unsigned num_exts,
      const VkExtensionProperties *properties, unsigned property_count)
{
   unsigned i, ext;
   bool found;
   for (ext = 0; ext < num_exts; ext++)
   {
      found = false;
      for (i = 0; i < property_count; i++)
      {
         if (string_is_equal(exts[ext], properties[i].extensionName))
         {
            found = true;
            break;
         }
      }

      if (!found)
         return false;
   }
   return true;
}

static bool vulkan_find_instance_extensions(const char **exts, unsigned num_exts)
{
   uint32_t property_count;
   bool ret                          = true;
   VkExtensionProperties *properties = NULL;

   if (vkEnumerateInstanceExtensionProperties(NULL, &property_count, NULL) != VK_SUCCESS)
      return false;

   properties = (VkExtensionProperties*)malloc(property_count * sizeof(*properties));
   if (!properties)
   {
      ret = false;
      goto end;
   }

   if (vkEnumerateInstanceExtensionProperties(NULL, &property_count, properties) != VK_SUCCESS)
   {
      ret = false;
      goto end;
   }

   if (!vulkan_find_extensions(exts, num_exts, properties, property_count))
   {
      RARCH_ERR("[Vulkan]: Could not find instance extensions. Will attempt without them.\n");
      ret = false;
      goto end;
   }

end:
   free(properties);
   return ret;
}

static bool vulkan_find_device_extensions(VkPhysicalDevice gpu,
      const char **enabled, unsigned *enabled_count,
      const char **exts, unsigned num_exts,
      const char **optional_exts, unsigned num_optional_exts)
{
   uint32_t property_count;
   unsigned i;
   bool ret                          = true;
   VkExtensionProperties *properties = NULL;

   if (vkEnumerateDeviceExtensionProperties(gpu, NULL, &property_count, NULL) != VK_SUCCESS)
      return false;

   properties = (VkExtensionProperties*)malloc(property_count * sizeof(*properties));
   if (!properties)
   {
      ret = false;
      goto end;
   }

   if (vkEnumerateDeviceExtensionProperties(gpu, NULL, &property_count, properties) != VK_SUCCESS)
   {
      ret = false;
      goto end;
   }

   if (!vulkan_find_extensions(exts, num_exts, properties, property_count))
   {
      RARCH_ERR("[Vulkan]: Could not find device extension. Will attempt without it.\n");
      ret = false;
      goto end;
   }

   memcpy((void*)enabled, exts, num_exts * sizeof(*exts));
   *enabled_count = num_exts;

   for (i = 0; i < num_optional_exts; i++)
      if (vulkan_find_extensions(&optional_exts[i], 1, properties, property_count))
         enabled[(*enabled_count)++] = optional_exts[i];

end:
   free(properties);
   return ret;
}

static bool vulkan_context_init_gpu(gfx_ctx_vulkan_data_t *vk)
{
   unsigned i;
   uint32_t gpu_count               = 0;
   VkPhysicalDevice *gpus           = NULL;
   union string_list_elem_attr attr = {0};
   settings_t *settings             = config_get_ptr();

   if (vkEnumeratePhysicalDevices(vk->context.instance,
            &gpu_count, NULL) != VK_SUCCESS)
   {
      RARCH_ERR("[Vulkan]: Failed to enumerate physical devices.\n");
      return false;
   }

   gpus = (VkPhysicalDevice*)calloc(gpu_count, sizeof(*gpus));
   if (!gpus)
   {
      RARCH_ERR("[Vulkan]: Failed to enumerate physical devices.\n");
      return false;
   }

   if (vkEnumeratePhysicalDevices(vk->context.instance,
            &gpu_count, gpus) != VK_SUCCESS)
   {
      RARCH_ERR("[Vulkan]: Failed to enumerate physical devices.\n");
      free(gpus);
      return false;
   }

   if (gpu_count < 1)
   {
      RARCH_ERR("[Vulkan]: Failed to enumerate Vulkan physical device.\n");
      free(gpus);
      return false;
   }

   if (vulkan_gpu_list)
      string_list_free(vulkan_gpu_list);

   vulkan_gpu_list = string_list_new();

   for (i = 0; i < gpu_count; i++)
   {
      VkPhysicalDeviceProperties gpu_properties;

      vkGetPhysicalDeviceProperties(gpus[i],
            &gpu_properties);

      RARCH_LOG("[Vulkan]: Found GPU at index %d: %s\n", i, gpu_properties.deviceName);

      string_list_append(vulkan_gpu_list, gpu_properties.deviceName, attr);
   }

   video_driver_set_gpu_api_devices(GFX_CTX_VULKAN_API, vulkan_gpu_list);

   if (0 <= settings->ints.vulkan_gpu_index && settings->ints.vulkan_gpu_index < (int)gpu_count)
   {
      RARCH_LOG("[Vulkan]: Using GPU index %d.\n", settings->ints.vulkan_gpu_index);
      vk->context.gpu = gpus[settings->ints.vulkan_gpu_index];
   }
   else
   {
      RARCH_WARN("[Vulkan]: Invalid GPU index %d, using first device found.\n", settings->ints.vulkan_gpu_index);
      vk->context.gpu = gpus[0];
   }

   free(gpus);
   return true;
}

static bool vulkan_context_init_device(gfx_ctx_vulkan_data_t *vk)
{
   bool use_device_ext;
   uint32_t queue_count;
   unsigned i;
   static const float one             = 1.0f;
   bool found_queue                   = false;

   VkPhysicalDeviceFeatures features  = { false };
   VkDeviceQueueCreateInfo queue_info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
   VkDeviceCreateInfo device_info     = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };

   const char *enabled_device_extensions[8];
   unsigned enabled_device_extension_count = 0;

   static const char *device_extensions[] = {
      "VK_KHR_swapchain",
   };

   static const char *optional_device_extensions[] = {
      "VK_KHR_sampler_mirror_clamp_to_edge",
   };

#ifdef VULKAN_DEBUG
   static const char *device_layers[] = { "VK_LAYER_LUNARG_standard_validation" };
#endif

   struct retro_hw_render_context_negotiation_interface_vulkan *iface =
      (struct retro_hw_render_context_negotiation_interface_vulkan*)video_driver_get_context_negotiation_interface();

   if (iface && iface->interface_type != RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN)
   {
      RARCH_WARN("[Vulkan]: Got HW context negotiation interface, but it's the wrong API.\n");
      iface = NULL;
   }

   if (iface && iface->interface_version != RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN_VERSION)
   {
      RARCH_WARN("[Vulkan]: Got HW context negotiation interface, but it's the wrong interface version.\n");
      iface = NULL;
   }

   if (!cached_device_vk && iface && iface->create_device)
   {
      struct retro_vulkan_context context = { 0 };
      const VkPhysicalDeviceFeatures features = { 0 };

      bool ret = iface->create_device(&context, vk->context.instance,
            vk->context.gpu,
            vk->vk_surface,
            vulkan_symbol_wrapper_instance_proc_addr(),
            device_extensions,
            ARRAY_SIZE(device_extensions),
#ifdef VULKAN_DEBUG
            device_layers,
            ARRAY_SIZE(device_layers),
#else
            NULL,
            0,
#endif
            &features);

      if (!ret)
      {
         RARCH_WARN("[Vulkan]: Failed to create device with negotiation interface. Falling back to default path.\n");
      }
      else
      {
         vk->context.destroy_device = iface->destroy_device;

         vk->context.device = context.device;
         vk->context.queue = context.queue;
         vk->context.gpu = context.gpu;
         vk->context.graphics_queue_index = context.queue_family_index;

         if (context.presentation_queue != context.queue)
         {
            RARCH_ERR("[Vulkan]: Present queue != graphics queue. This is currently not supported.\n");
            return false;
         }
      }
   }

   if (cached_device_vk && cached_destroy_device_vk)
   {
      vk->context.destroy_device = cached_destroy_device_vk;
      cached_destroy_device_vk   = NULL;
   }

   if (!vulkan_context_init_gpu(vk))
      return false;

   vkGetPhysicalDeviceProperties(vk->context.gpu,
         &vk->context.gpu_properties);
   vkGetPhysicalDeviceMemoryProperties(vk->context.gpu,
         &vk->context.memory_properties);

#ifdef VULKAN_EMULATE_MAILBOX
   /* Win32 windowed mode seems to deal just fine with toggling VSync.
    * Fullscreen however ... */
   vk->emulate_mailbox = vk->fullscreen;
#endif

   RARCH_LOG("[Vulkan]: Using GPU: %s\n", vk->context.gpu_properties.deviceName);

   {
      char device_str[128];
      char driver_version[64];
      char api_version[64];
      char version_str[128];
      int pos = 0;

      device_str[0] = driver_version[0] = api_version[0] = version_str[0] = '\0';

      strlcpy(device_str, vk->context.gpu_properties.deviceName, sizeof(device_str));
      strlcat(device_str, " ", sizeof(device_str));

      pos += snprintf(driver_version + pos, sizeof(driver_version) - pos, "%u", VK_VERSION_MAJOR(vk->context.gpu_properties.driverVersion));
      strlcat(driver_version, ".", sizeof(driver_version));
      pos++;
      pos += snprintf(driver_version + pos, sizeof(driver_version) - pos, "%u", VK_VERSION_MINOR(vk->context.gpu_properties.driverVersion));
      pos++;
      strlcat(driver_version, ".", sizeof(driver_version));
      pos += snprintf(driver_version + pos, sizeof(driver_version) - pos, "%u", VK_VERSION_PATCH(vk->context.gpu_properties.driverVersion));

      strlcat(device_str, driver_version, sizeof(device_str));

      pos = 0;

      pos += snprintf(api_version + pos, sizeof(api_version) - pos, "%u", VK_VERSION_MAJOR(vk->context.gpu_properties.apiVersion));
      strlcat(api_version, ".", sizeof(api_version));
      pos++;
      pos += snprintf(api_version + pos, sizeof(api_version) - pos, "%u", VK_VERSION_MINOR(vk->context.gpu_properties.apiVersion));
      pos++;
      strlcat(api_version, ".", sizeof(api_version));
      pos += snprintf(api_version + pos, sizeof(api_version) - pos, "%u", VK_VERSION_PATCH(vk->context.gpu_properties.apiVersion));

      strlcat(version_str, api_version, sizeof(device_str));

      video_driver_set_gpu_device_string(device_str);
      video_driver_set_gpu_api_version_string(version_str);
   }

   if (vk->context.device == VK_NULL_HANDLE)
   {
      VkQueueFamilyProperties *queue_properties = NULL;
      vkGetPhysicalDeviceQueueFamilyProperties(vk->context.gpu,
            &queue_count, NULL);

      if (queue_count < 1)
      {
         RARCH_ERR("[Vulkan]: Invalid number of queues detected.\n");
         return false;
      }

      queue_properties = (VkQueueFamilyProperties*)malloc(queue_count * sizeof(*queue_properties));
      if (!queue_properties)
         return false;

      vkGetPhysicalDeviceQueueFamilyProperties(vk->context.gpu,
            &queue_count, queue_properties);

      for (i = 0; i < queue_count; i++)
      {
         VkQueueFlags required;
         VkBool32 supported = VK_FALSE;
         vkGetPhysicalDeviceSurfaceSupportKHR(
               vk->context.gpu, i,
               vk->vk_surface, &supported);

         required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
         if (supported && ((queue_properties[i].queueFlags & required) == required))
         {
            vk->context.graphics_queue_index = i;
            RARCH_LOG("[Vulkan]: Queue family %u supports %u sub-queues.\n",
                  i, queue_properties[i].queueCount);
            found_queue = true;
            break;
         }
      }

      free(queue_properties);

      if (!found_queue)
      {
         RARCH_ERR("[Vulkan]: Did not find suitable graphics queue.\n");
         return false;
      }

      use_device_ext = vulkan_find_device_extensions(vk->context.gpu,
              enabled_device_extensions, &enabled_device_extension_count,
              device_extensions, ARRAY_SIZE(device_extensions),
              optional_device_extensions, ARRAY_SIZE(optional_device_extensions));

      if (!use_device_ext)
      {
          RARCH_ERR("[Vulkan]: Could not find required device extensions.\n");
          return false;
      }

      queue_info.queueFamilyIndex         = vk->context.graphics_queue_index;
      queue_info.queueCount               = 1;
      queue_info.pQueuePriorities         = &one;

      device_info.queueCreateInfoCount    = 1;
      device_info.pQueueCreateInfos       = &queue_info;
      device_info.enabledExtensionCount   = enabled_device_extension_count;
      device_info.ppEnabledExtensionNames = enabled_device_extension_count ? enabled_device_extensions : NULL;
      device_info.pEnabledFeatures        = &features;
#ifdef VULKAN_DEBUG
      device_info.enabledLayerCount       = ARRAY_SIZE(device_layers);
      device_info.ppEnabledLayerNames     = device_layers;
#endif

      if (cached_device_vk)
      {
         vk->context.device = cached_device_vk;
         cached_device_vk   = NULL;

         video_driver_set_video_cache_context_ack();
         RARCH_LOG("[Vulkan]: Using cached Vulkan context.\n");
      }
      else if (vkCreateDevice(vk->context.gpu, &device_info,
               NULL, &vk->context.device) != VK_SUCCESS)
      {
         RARCH_ERR("[Vulkan]: Failed to create device.\n");
         return false;
      }
   }

   if (!vulkan_load_device_symbols(vk))
   {
      RARCH_ERR("[Vulkan]: Failed to load device symbols.\n");
      return false;
   }

   vkGetDeviceQueue(vk->context.device,
      vk->context.graphics_queue_index, 0, &vk->context.queue);

#ifdef HAVE_THREADS
   vk->context.queue_lock = slock_new();
   if (!vk->context.queue_lock)
   {
      RARCH_ERR("[Vulkan]: Failed to create queue lock.\n");
      return false;
   }
#endif

   return true;
}

bool vulkan_context_init(gfx_ctx_vulkan_data_t *vk,
      enum vulkan_wsi_type type)
{
   unsigned i;
   VkResult res;
   PFN_vkGetInstanceProcAddr GetInstanceProcAddr;
   VkInstanceCreateInfo info          = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
   VkApplicationInfo app              = { VK_STRUCTURE_TYPE_APPLICATION_INFO };

   const char *instance_extensions[4];
   unsigned ext_count = 0;

#ifdef VULKAN_DEBUG
   instance_extensions[ext_count++] = "VK_EXT_debug_report";
   static const char *instance_layers[] = { "VK_LAYER_LUNARG_standard_validation" };
#endif

   bool use_instance_ext;
   struct retro_hw_render_context_negotiation_interface_vulkan *iface =
      (struct retro_hw_render_context_negotiation_interface_vulkan*)video_driver_get_context_negotiation_interface();

   if (iface && iface->interface_type != RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN)
   {
      RARCH_WARN("[Vulkan]: Got HW context negotiation interface, but it's the wrong API.\n");
      iface = NULL;
   }

   if (iface && iface->interface_version != RETRO_HW_RENDER_CONTEXT_NEGOTIATION_INTERFACE_VULKAN_VERSION)
   {
      RARCH_WARN("[Vulkan]: Got HW context negotiation interface, but it's the wrong interface version.\n");
      iface = NULL;
   }

   instance_extensions[ext_count++] = "VK_KHR_surface";

   switch (type)
   {
      case VULKAN_WSI_WAYLAND:
         instance_extensions[ext_count++] = "VK_KHR_wayland_surface";
         break;
      case VULKAN_WSI_ANDROID:
         instance_extensions[ext_count++] = "VK_KHR_android_surface";
         break;
      case VULKAN_WSI_WIN32:
         instance_extensions[ext_count++] = "VK_KHR_win32_surface";
         break;
      case VULKAN_WSI_XLIB:
         instance_extensions[ext_count++] = "VK_KHR_xlib_surface";
         break;
      case VULKAN_WSI_XCB:
         instance_extensions[ext_count++] = "VK_KHR_xcb_surface";
         break;
      case VULKAN_WSI_MIR:
         instance_extensions[ext_count++] = "VK_KHR_mir_surface";
         break;
      case VULKAN_WSI_DISPLAY:
         instance_extensions[ext_count++] = "VK_KHR_display";
         break;
      case VULKAN_WSI_MVK_MACOS:
         instance_extensions[ext_count++] = "VK_MVK_macos_surface";
         break;
      case VULKAN_WSI_MVK_IOS:
         instance_extensions[ext_count++] = "VK_MVK_ios_surface";
         break;
      case VULKAN_WSI_NONE:
      default:
         break;
   }

   if (!vulkan_library)
   {
#ifdef _WIN32
      vulkan_library = dylib_load("vulkan-1.dll");
#elif __APPLE__
      vulkan_library = dylib_load("libMoltenVK.dylib");
#else
      vulkan_library = dylib_load("libvulkan.so");
      if (!vulkan_library)
      {
         vulkan_library = dylib_load("libvulkan.so.1");
      }
#endif
   }

   if (!vulkan_library)
   {
      RARCH_ERR("[Vulkan]: Failed to open Vulkan loader.\n");
      return false;
   }

   RARCH_LOG("Vulkan dynamic library loaded.\n");

   GetInstanceProcAddr =
      (PFN_vkGetInstanceProcAddr)dylib_proc(vulkan_library, "vkGetInstanceProcAddr");

   if (!GetInstanceProcAddr)
   {
      RARCH_ERR("[Vulkan]: Failed to load vkGetInstanceProcAddr symbol, broken loader?\n");
      return false;
   }

   vulkan_symbol_wrapper_init(GetInstanceProcAddr);

   if (!vulkan_symbol_wrapper_load_global_symbols())
   {
      RARCH_ERR("[Vulkan]: Failed to load global Vulkan symbols, broken loader?\n");
      return false;
   }

   use_instance_ext = vulkan_find_instance_extensions(instance_extensions, ext_count);

   app.pApplicationName              = msg_hash_to_str(MSG_PROGRAM);
   app.applicationVersion            = 0;
   app.pEngineName                   = msg_hash_to_str(MSG_PROGRAM);
   app.engineVersion                 = 0;
   app.apiVersion                    = VK_MAKE_VERSION(1, 0, 18);

   info.pApplicationInfo             = &app;
   info.enabledExtensionCount        = use_instance_ext ? ext_count : 0;
   info.ppEnabledExtensionNames      = use_instance_ext ? instance_extensions : NULL;
#ifdef VULKAN_DEBUG
   info.enabledLayerCount            = ARRAY_SIZE(instance_layers);
   info.ppEnabledLayerNames          = instance_layers;
#endif

   if (iface && iface->get_application_info)
   {
      info.pApplicationInfo = iface->get_application_info();
      if (info.pApplicationInfo->pApplicationName)
      {
         RARCH_LOG("[Vulkan]: App: %s (version %u)\n",
               info.pApplicationInfo->pApplicationName,
               info.pApplicationInfo->applicationVersion);
      }

      if (info.pApplicationInfo->pEngineName)
      {
         RARCH_LOG("[Vulkan]: Engine: %s (version %u)\n",
               info.pApplicationInfo->pEngineName,
               info.pApplicationInfo->engineVersion);
      }
   }

   if (cached_instance_vk)
   {
      vk->context.instance           = cached_instance_vk;
      cached_instance_vk             = NULL;
      res                            = VK_SUCCESS;
   }
   else
      res = vkCreateInstance(&info, NULL, &vk->context.instance);

#ifdef VULKAN_DEBUG
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkCreateDebugReportCallbackEXT);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkDebugReportMessageEXT);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkDestroyDebugReportCallbackEXT);

   {
      VkDebugReportCallbackCreateInfoEXT info =
      { VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT };
      info.flags =
         VK_DEBUG_REPORT_ERROR_BIT_EXT |
         VK_DEBUG_REPORT_WARNING_BIT_EXT |
         VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
      info.pfnCallback = vulkan_debug_cb;

      if (vk->context.instance)
         vkCreateDebugReportCallbackEXT(vk->context.instance, &info, NULL,
               &vk->context.debug_callback);
   }
   RARCH_LOG("[Vulkan]: Enabling Vulkan debug layers.\n");
#endif

   /* Try different API versions if driver has compatible
    * but slightly different VK_API_VERSION. */
   for (i = 1; i < 4 && res == VK_ERROR_INCOMPATIBLE_DRIVER; i++)
   {
      info.pApplicationInfo = &app;
      app.apiVersion = VK_MAKE_VERSION(1, 0, i);
      res = vkCreateInstance(&info, NULL, &vk->context.instance);
   }

   if (res != VK_SUCCESS)
   {
      RARCH_ERR("Failed to create Vulkan instance (%d).\n", res);
      return false;
   }

   if (!vulkan_load_instance_symbols(vk))
   {
      RARCH_ERR("[Vulkan]: Failed to load instance symbols.\n");
      return false;
   }

   return true;
}

static bool vulkan_update_display_mode(
      unsigned *width,
      unsigned *height,
      const VkDisplayModePropertiesKHR *mode,
      const struct vulkan_display_surface_info *info)
{
   unsigned visible_width  = mode->parameters.visibleRegion.width;
   unsigned visible_height = mode->parameters.visibleRegion.height;

   if (!info->width || !info->height)
   {
      /* Strategy here is to pick something which is largest resolution. */
      unsigned area = visible_width * visible_height;
      if (area > (*width) * (*height))
      {
         *width     = visible_width;
         *height    = visible_height;
         return true;
      }
   }
   else
   {
      /* For particular resolutions, find the closest. */
      int delta_x     = (int)info->width - (int)visible_width;
      int delta_y     = (int)info->height - (int)visible_height;
      int old_delta_x = (int)info->width - (int)*width;
      int old_delta_y = (int)info->height - (int)*height;

      int dist        = delta_x * delta_x + delta_y * delta_y;
      int old_dist    = old_delta_x * old_delta_x + old_delta_y * old_delta_y;

      if (dist < old_dist)
      {
         *width       = visible_width;
         *height      = visible_height;
         return true;
      }
   }

   return false;
}

static bool vulkan_create_display_surface(gfx_ctx_vulkan_data_t *vk,
      unsigned *width, unsigned *height,
      const struct vulkan_display_surface_info *info)
{
   bool ret                                  = true;
   uint32_t display_count                    = 0;
   uint32_t plane_count                      = 0;
   VkDisplayPropertiesKHR *displays          = NULL;
   VkDisplayPlanePropertiesKHR *planes       = NULL;
   uint32_t mode_count                       = 0;
   VkDisplayModePropertiesKHR *modes         = NULL;
   unsigned dpy, i, j;
   uint32_t best_plane                       = UINT32_MAX;
   VkDisplayPlaneAlphaFlagBitsKHR alpha_mode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
   VkDisplaySurfaceCreateInfoKHR create_info = { VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR };
   VkDisplayModeKHR best_mode                = VK_NULL_HANDLE;
   /* Monitor index starts on 1, 0 is auto. */
   unsigned monitor_index                    = info->monitor_index;
   unsigned saved_width                      = *width;
   unsigned saved_height                     = *height;

   /* We need to decide on GPU here to be able to query support. */
   if (!vulkan_context_init_gpu(vk))
      return false;

   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkGetPhysicalDeviceDisplayPropertiesKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkGetPhysicalDeviceDisplayPlanePropertiesKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkGetDisplayPlaneSupportedDisplaysKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkGetDisplayModePropertiesKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkCreateDisplayModeKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkGetDisplayPlaneCapabilitiesKHR);
   VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_EXTENSION_SYMBOL(vk->context.instance,
         vkCreateDisplayPlaneSurfaceKHR);

#define GOTO_FAIL() do { \
   ret = false; \
   goto end; \
} while(0)

   if (vkGetPhysicalDeviceDisplayPropertiesKHR(vk->context.gpu, &display_count, NULL) != VK_SUCCESS)
      GOTO_FAIL();
   displays = (VkDisplayPropertiesKHR*)calloc(display_count, sizeof(*displays));
   if (!displays)
      GOTO_FAIL();
   if (vkGetPhysicalDeviceDisplayPropertiesKHR(vk->context.gpu, &display_count, displays) != VK_SUCCESS)
      GOTO_FAIL();

   if (vkGetPhysicalDeviceDisplayPlanePropertiesKHR(vk->context.gpu, &plane_count, NULL) != VK_SUCCESS)
      GOTO_FAIL();
   planes = (VkDisplayPlanePropertiesKHR*)calloc(plane_count, sizeof(*planes));
   if (!planes)
      GOTO_FAIL();
   if (vkGetPhysicalDeviceDisplayPlanePropertiesKHR(vk->context.gpu, &plane_count, planes) != VK_SUCCESS)
      GOTO_FAIL();

   if (monitor_index > display_count)
   {
      RARCH_WARN("Monitor index is out of range, using automatic display.\n");
      monitor_index = 0;
   }

retry:
   for (dpy = 0; dpy < display_count; dpy++)
   {
      VkDisplayKHR display;
      if (monitor_index != 0 && (monitor_index - 1) != dpy)
         continue;

      display    = displays[dpy].display;
      best_mode  = VK_NULL_HANDLE;
      best_plane = UINT32_MAX;

      if (vkGetDisplayModePropertiesKHR(vk->context.gpu,
            display, &mode_count, NULL) != VK_SUCCESS)
         GOTO_FAIL();

      modes = (VkDisplayModePropertiesKHR*)calloc(mode_count, sizeof(*modes));
      if (!modes)
         GOTO_FAIL();

      if (vkGetDisplayModePropertiesKHR(vk->context.gpu,
            display, &mode_count, modes) != VK_SUCCESS)
         GOTO_FAIL();

      for (i = 0; i < mode_count; i++)
      {
         const VkDisplayModePropertiesKHR *mode = &modes[i];
         if (vulkan_update_display_mode(width, height, mode, info))
            best_mode = modes[i].displayMode;
      }

      free(modes);
      modes = NULL;
      mode_count = 0;

      if (best_mode == VK_NULL_HANDLE)
         continue;

      for (i = 0; i < plane_count; i++)
      {
         uint32_t supported_count = 0;
         VkDisplayKHR *supported = NULL;
         VkDisplayPlaneCapabilitiesKHR plane_caps;
         vkGetDisplayPlaneSupportedDisplaysKHR(vk->context.gpu, i, &supported_count, NULL);
         if (!supported_count)
            continue;

         supported = (VkDisplayKHR*)calloc(supported_count, sizeof(*supported));
         if (!supported)
            GOTO_FAIL();

         vkGetDisplayPlaneSupportedDisplaysKHR(vk->context.gpu, i, &supported_count,
               supported);

         for (j = 0; j < supported_count; j++)
         {
            if (supported[j] == display)
            {
               if (best_plane == UINT32_MAX)
                  best_plane = j;
               break;
            }
         }

         free(supported);
         supported = NULL;

         if (j == supported_count)
            continue;

         if (planes[i].currentDisplay == VK_NULL_HANDLE ||
             planes[i].currentDisplay == display)
            best_plane = j;
         else
            continue;

         vkGetDisplayPlaneCapabilitiesKHR(vk->context.gpu,
               best_mode, i, &plane_caps);

         if (plane_caps.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR)
         {
            best_plane = j;
            alpha_mode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
            goto out;
         }
      }
   }
out:

   if (best_plane == UINT32_MAX && monitor_index != 0)
   {
      RARCH_WARN("Could not find suitable surface for monitor index: %u.\n",
            monitor_index);
      RARCH_WARN("Retrying first suitable monitor.\n");
      monitor_index = 0;
      best_mode = VK_NULL_HANDLE;
      *width = saved_width;
      *height = saved_height;
      goto retry;
   }

   if (best_mode == VK_NULL_HANDLE)
      GOTO_FAIL();
   if (best_plane == UINT32_MAX)
      GOTO_FAIL();

   create_info.displayMode = best_mode;
   create_info.planeIndex = best_plane;
   create_info.planeStackIndex = planes[best_plane].currentStackIndex;
   create_info.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
   create_info.globalAlpha = 1.0f;
   create_info.alphaMode = alpha_mode;
   create_info.imageExtent.width = *width;
   create_info.imageExtent.height = *height;

   if (vkCreateDisplayPlaneSurfaceKHR(vk->context.instance,
            &create_info, NULL, &vk->vk_surface) != VK_SUCCESS)
      GOTO_FAIL();

end:
   free(displays);
   free(planes);
   free(modes);
   return ret;
}

bool vulkan_surface_create(gfx_ctx_vulkan_data_t *vk,
      enum vulkan_wsi_type type,
      void *display, void *surface,
      unsigned width, unsigned height,
      unsigned swap_interval)
{
   switch (type)
   {
      case VULKAN_WSI_WAYLAND:
#ifdef HAVE_WAYLAND
         {
            PFN_vkCreateWaylandSurfaceKHR create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateWaylandSurfaceKHR", create))
               return false;
            VkWaylandSurfaceCreateInfoKHR surf_info;

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
            surf_info.pNext   = NULL;
            surf_info.flags   = 0;
            surf_info.display = (struct wl_display*)display;
            surf_info.surface = (struct wl_surface*)surface;

            if (create(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface) != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_ANDROID:
#ifdef ANDROID
         {
            PFN_vkCreateAndroidSurfaceKHR create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateAndroidSurfaceKHR", create))
               return false;
            VkAndroidSurfaceCreateInfoKHR surf_info;

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
            surf_info.flags  = 0;
            surf_info.window = (ANativeWindow*)surface;

            if (create(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface) != VK_SUCCESS)
            {
               RARCH_ERR("[Vulkan]: Failed to create Android surface.\n");
               return false;
            }
            RARCH_LOG("[Vulkan]: Created Android surface: %llu\n",
                  (unsigned long long)vk->vk_surface);
         }
#endif
         break;
      case VULKAN_WSI_WIN32:
#ifdef _WIN32
         {
            VkWin32SurfaceCreateInfoKHR surf_info;
            PFN_vkCreateWin32SurfaceKHR create;

            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateWin32SurfaceKHR", create))
               return false;

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
            surf_info.flags     = 0;
            surf_info.hinstance = *(const HINSTANCE*)display;
            surf_info.hwnd      = *(const HWND*)surface;

            if (create(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface) != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_XLIB:
#ifdef HAVE_XLIB
         {
            PFN_vkCreateXlibSurfaceKHR create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateXlibSurfaceKHR", create))
               return false;
            VkXlibSurfaceCreateInfoKHR surf_info;

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
            surf_info.flags  = 0;
            surf_info.dpy    = (Display*)display;
            surf_info.window = *(const Window*)surface;

            if (create(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface)
                  != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_XCB:
#ifdef HAVE_X11
#ifdef HAVE_XCB
         {
            PFN_vkCreateXcbSurfaceKHR create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateXcbSurfaceKHR", create))
               return false;
            VkXcbSurfaceCreateInfoKHR surf_info;

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType      = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
            surf_info.flags      = 0;
            surf_info.connection = XGetXCBConnection((Display*)display);
            surf_info.window     = *(const xcb_window_t*)surface;

            if (create(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface)
                  != VK_SUCCESS)
               return false;
         }
#endif
#endif
         break;
      case VULKAN_WSI_MIR:
#ifdef HAVE_MIR
         {
            PFN_vkCreateMirSurfaceKHR create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateMirSurfaceKHR", create))
               return false;
            VkMirSurfaceCreateInfoKHR surf_info;

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType      = VK_STRUCTURE_TYPE_MIR_SURFACE_CREATE_INFO_KHR;
            surf_info.connection = display;
            surf_info.mirSurface = surface;

            if (create(vk->context.instance,
                     &surf_info, NULL, &vk->vk_surface)
                  != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_DISPLAY:
         {
            if (!vulkan_create_display_surface(vk,
                     &width, &height,
                     (const struct vulkan_display_surface_info*)display))
               return false;
         }
         break;
      case VULKAN_WSI_MVK_MACOS:
#ifdef HAVE_COCOA
         {
            PFN_vkCreateMacOSSurfaceMVK create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateMacOSSurfaceMVK", create))
               return false;
            VkMacOSSurfaceCreateInfoMVK surf_info;

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
            surf_info.pNext = NULL;
            surf_info.flags = 0;
            surf_info.pView = surface;

            if (create(vk->context.instance, &surf_info, NULL, &vk->vk_surface)
                != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_MVK_IOS:
#ifdef HAVE_COCOATOUCH
         {
            PFN_vkCreateIOSSurfaceMVK create;
            if (!VULKAN_SYMBOL_WRAPPER_LOAD_INSTANCE_SYMBOL(vk->context.instance, "vkCreateIOSSurfaceMVK", create))
               return false;
            VkIOSSurfaceCreateInfoMVK surf_info;

            memset(&surf_info, 0, sizeof(surf_info));

            surf_info.sType = VK_STRUCTURE_TYPE_IOS_SURFACE_CREATE_INFO_MVK;
            surf_info.pNext = NULL;
            surf_info.flags = 0;
            surf_info.pView = surface;

            if (create(vk->context.instance, &surf_info, NULL, &vk->vk_surface)
                != VK_SUCCESS)
               return false;
         }
#endif
         break;
      case VULKAN_WSI_NONE:
      default:
         return false;
   }

   /* Must create device after surface since we need to be able to query queues to use for presentation. */
   if (!vulkan_context_init_device(vk))
      return false;

   if (!vulkan_create_swapchain(
            vk, width, height, swap_interval))
      return false;

   vulkan_acquire_next_image(vk);
   return true;
}

static void vulkan_destroy_swapchain(gfx_ctx_vulkan_data_t *vk)
{
   unsigned i;

   vulkan_emulated_mailbox_deinit(&vk->mailbox);
   if (vk->swapchain != VK_NULL_HANDLE)
   {
      vkDeviceWaitIdle(vk->context.device);
      vkDestroySwapchainKHR(vk->context.device, vk->swapchain, NULL);
      memset(vk->context.swapchain_images, 0, sizeof(vk->context.swapchain_images));
      vk->swapchain = VK_NULL_HANDLE;
      vk->context.has_acquired_swapchain = false;
   }

   for (i = 0; i < VULKAN_MAX_SWAPCHAIN_IMAGES; i++)
   {
      if (vk->context.swapchain_semaphores[i] != VK_NULL_HANDLE)
         vkDestroySemaphore(vk->context.device,
               vk->context.swapchain_semaphores[i], NULL);
      if (vk->context.swapchain_fences[i] != VK_NULL_HANDLE)
         vkDestroyFence(vk->context.device,
               vk->context.swapchain_fences[i], NULL);
   }

   memset(vk->context.swapchain_semaphores, 0, sizeof(vk->context.swapchain_semaphores));
   memset(vk->context.swapchain_fences, 0, sizeof(vk->context.swapchain_fences));
}

void vulkan_present(gfx_ctx_vulkan_data_t *vk, unsigned index)
{
   VkPresentInfoKHR present           = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
   VkResult result                    = VK_SUCCESS;
   VkResult err                       = VK_SUCCESS;

   if (!vk->context.has_acquired_swapchain)
      return;
   vk->context.has_acquired_swapchain = false;

   /* We're still waiting for a proper swapchain, so just fake it. */
   if (vk->swapchain == VK_NULL_HANDLE)
   {
      retro_sleep(10);
      return;
   }

   present.swapchainCount          = 1;
   present.pSwapchains             = &vk->swapchain;
   present.pImageIndices           = &index;
   present.pResults                = &result;
   present.waitSemaphoreCount      = 1;
   present.pWaitSemaphores         = &vk->context.swapchain_semaphores[index];

   /* Better hope QueuePresent doesn't block D: */
#ifdef HAVE_THREADS
   slock_lock(vk->context.queue_lock);
#endif
   err = vkQueuePresentKHR(vk->context.queue, &present);

   /* VK_SUBOPTIMAL_KHR can be returned on Android 10 when prerotate is not dealt with.
    * This is not an error we need to care about, and we'll treat it as SUCCESS. */
   if (result == VK_SUBOPTIMAL_KHR)
      result = VK_SUCCESS;
   if (err == VK_SUBOPTIMAL_KHR)
      err = VK_SUCCESS;

#ifdef WSI_HARDENING_TEST
   trigger_spurious_error_vkresult(&err);
#endif

   if (err != VK_SUCCESS || result != VK_SUCCESS)
   {
      RARCH_LOG("[Vulkan]: QueuePresent failed, destroying swapchain.\n");
      vulkan_destroy_swapchain(vk);
   }

#ifdef HAVE_THREADS
   slock_unlock(vk->context.queue_lock);
#endif
}

void vulkan_context_destroy(gfx_ctx_vulkan_data_t *vk,
      bool destroy_surface)
{
   if (!vk->context.instance)
      return;

   if (vk->context.device)
      vkDeviceWaitIdle(vk->context.device);

   vulkan_destroy_swapchain(vk);

   if (destroy_surface && vk->vk_surface != VK_NULL_HANDLE)
   {
      vkDestroySurfaceKHR(vk->context.instance,
            vk->vk_surface, NULL);
      vk->vk_surface = VK_NULL_HANDLE;
   }

#ifdef VULKAN_DEBUG
   if (vk->context.debug_callback)
      vkDestroyDebugReportCallbackEXT(vk->context.instance, vk->context.debug_callback, NULL);
#endif

   if (video_driver_is_video_cache_context())
   {
      cached_device_vk         = vk->context.device;
      cached_instance_vk       = vk->context.instance;
      cached_destroy_device_vk = vk->context.destroy_device;
   }
   else
   {
      if (vk->context.device)
      {
         vkDestroyDevice(vk->context.device, NULL);
         vk->context.device = NULL;
      }
      if (vk->context.instance)
      {
         if (vk->context.destroy_device)
            vk->context.destroy_device();

         vkDestroyInstance(vk->context.instance, NULL);
         vk->context.instance = NULL;

         if (vulkan_library)
         {
            dylib_close(vulkan_library);
            vulkan_library = NULL;
         }
      }
   }

   video_driver_set_gpu_api_devices(GFX_CTX_VULKAN_API, NULL);
   if (vulkan_gpu_list)
   {
      string_list_free(vulkan_gpu_list);
      vulkan_gpu_list = NULL;
   }
}

static void vulkan_acquire_clear_fences(gfx_ctx_vulkan_data_t *vk)
{
   unsigned i;
   for (i = 0; i < vk->context.num_swapchain_images; i++)
   {
      if (vk->context.swapchain_fences[i])
      {
         vkDestroyFence(vk->context.device,
               vk->context.swapchain_fences[i], NULL);
         vk->context.swapchain_fences[i] = VK_NULL_HANDLE;
      }
      vk->context.swapchain_fences_signalled[i] = false;
   }
}

static void vulkan_acquire_wait_fences(gfx_ctx_vulkan_data_t *vk)
{
   VkFenceCreateInfo fence_info =
   { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

   unsigned index      = vk->context.current_swapchain_index;
   VkFence *next_fence = &vk->context.swapchain_fences[index];

   if (*next_fence != VK_NULL_HANDLE)
   {
      if (vk->context.swapchain_fences_signalled[index])
         vkWaitForFences(vk->context.device, 1, next_fence, true, UINT64_MAX);
      vkResetFences(vk->context.device, 1, next_fence);
   }
   else
      vkCreateFence(vk->context.device, &fence_info, NULL, next_fence);
   vk->context.swapchain_fences_signalled[index] = false;
}

static void vulkan_create_wait_fences(gfx_ctx_vulkan_data_t *vk)
{
   VkFenceCreateInfo fence_info =
   { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };

   unsigned i;
   for (i = 0; i < vk->context.num_swapchain_images; i++)
   {
      if (!vk->context.swapchain_fences[i])
         vkCreateFence(vk->context.device, &fence_info, NULL,
               &vk->context.swapchain_fences[i]);
   }
}

void vulkan_acquire_next_image(gfx_ctx_vulkan_data_t *vk)
{
   unsigned index;
   VkResult err;
   VkFence fence;
   VkFenceCreateInfo fence_info   =
   { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
   VkSemaphoreCreateInfo sem_info =
   { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
   bool is_retrying               = false;

retry:
   if (vk->swapchain == VK_NULL_HANDLE)
   {
      /* We don't have a swapchain, try to create one now. */
      if (!vulkan_create_swapchain(vk, vk->context.swapchain_width,
               vk->context.swapchain_height, vk->context.swap_interval))
      {
         RARCH_ERR("[Vulkan]: Failed to create new swapchain.\n");
         return;
      }

      if (vk->swapchain == VK_NULL_HANDLE)
      {
         /* We still don't have a swapchain, so just fake it ... */
         vk->context.current_swapchain_index = 0;
         vulkan_acquire_clear_fences(vk);
         vulkan_acquire_wait_fences(vk);
         vk->context.invalid_swapchain = true;
         return;
      }
   }

   retro_assert(!vk->context.has_acquired_swapchain);

   if (vk->emulating_mailbox)
   {
      /* Non-blocking acquire. If we don't get a swapchain frame right away,
       * just skip rendering to the swapchain this frame, similar to what
       * MAILBOX would do. */
      err   = vulkan_emulated_mailbox_acquire_next_image(
            &vk->mailbox, &vk->context.current_swapchain_index);
      fence = VK_NULL_HANDLE;
   }
   else
   {
      vkCreateFence(vk->context.device, &fence_info, NULL, &fence);
      err = vkAcquireNextImageKHR(vk->context.device,
            vk->swapchain, UINT64_MAX,
            VK_NULL_HANDLE, fence, &vk->context.current_swapchain_index);

      /* VK_SUBOPTIMAL_KHR can be returned on Android 10 
       * when prerotate is not dealt with.
       * This is not an error we need to care about, and 
       * we'll treat it as SUCCESS. */
      if (err == VK_SUBOPTIMAL_KHR)
         err = VK_SUCCESS;
   }

   if (err == VK_SUCCESS)
   {
      if (fence != VK_NULL_HANDLE)
         vkWaitForFences(vk->context.device, 1, &fence, true, UINT64_MAX);
      vk->context.has_acquired_swapchain = true;
   }
   else
      vk->context.has_acquired_swapchain = false;

#ifdef WSI_HARDENING_TEST
   trigger_spurious_error_vkresult(&err);
#endif

   if (fence != VK_NULL_HANDLE)
      vkDestroyFence(vk->context.device, fence, NULL);

   if (err == VK_NOT_READY || err == VK_TIMEOUT)
   {
      /* Just pretend we have a swapchain index, round-robin style. */
      vk->context.current_swapchain_index =
         (vk->context.current_swapchain_index + 1) % vk->context.num_swapchain_images;
   }
   else if (err == VK_ERROR_OUT_OF_DATE_KHR)
   {
      /* Throw away the old swapchain and try again. */
      vulkan_destroy_swapchain(vk);

      if (is_retrying)
      {
         RARCH_ERR("[Vulkan]: Swapchain is out of date, trying to create new one. Have tried multiple times ...\n");
         retro_sleep(10);
      }
      else
         RARCH_ERR("[Vulkan]: Swapchain is out of date, trying to create new one.\n");
      is_retrying = true;
      vulkan_acquire_clear_fences(vk);
      goto retry;
   }
   else if (err != VK_SUCCESS)
   {
      /* We are screwed, don't try anymore. Maybe it will work later. */
      vulkan_destroy_swapchain(vk);
      RARCH_ERR("[Vulkan]: Failed to acquire from swapchain (err = %d).\n",
            (int)err);
      if (err == VK_ERROR_SURFACE_LOST_KHR)
         RARCH_ERR("[Vulkan]: Got VK_ERROR_SURFACE_LOST_KHR.\n");
      /* Force driver to reset swapchain image handles. */
      vk->context.invalid_swapchain = true;
      vulkan_acquire_clear_fences(vk);
      return;
   }

   index = vk->context.current_swapchain_index;
   if (vk->context.swapchain_semaphores[index] == VK_NULL_HANDLE)
   {
      vkCreateSemaphore(vk->context.device, &sem_info,
            NULL, &vk->context.swapchain_semaphores[index]);
   }
   vulkan_acquire_wait_fences(vk);
}

bool vulkan_create_swapchain(gfx_ctx_vulkan_data_t *vk,
      unsigned width, unsigned height,
      unsigned swap_interval)
{
   unsigned i;
   uint32_t format_count;
   uint32_t present_mode_count;
   uint32_t desired_swapchain_images;
   VkSurfaceCapabilitiesKHR surface_properties;
   VkSurfaceFormatKHR formats[256];
   VkPresentModeKHR present_modes[16];
   VkSurfaceFormatKHR format;
   VkExtent2D swapchain_size;
   VkSwapchainKHR old_swapchain;
   VkSurfaceTransformFlagBitsKHR pre_transform;
   VkSwapchainCreateInfoKHR info           = {
      VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
   VkPresentModeKHR swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
   settings_t                    *settings = config_get_ptr();
   VkCompositeAlphaFlagBitsKHR composite   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   VkResult res;

   vkDeviceWaitIdle(vk->context.device);
   vulkan_acquire_clear_fences(vk);

   if (swap_interval == 0 && vk->emulate_mailbox)
   {
      swap_interval          = 1;
      vk->emulating_mailbox  = true;
   }
   else
      vk->emulating_mailbox  = false;

   vk->created_new_swapchain = true;

   if (vk->swapchain != VK_NULL_HANDLE &&
         !vk->context.invalid_swapchain &&
         vk->context.swapchain_width == width &&
         vk->context.swapchain_height == height &&
         vk->context.swap_interval == swap_interval)
   {
      /* Do not bother creating a swapchain redundantly. */
      RARCH_LOG("[Vulkan]: Do not need to re-create swapchain.\n");
      vulkan_create_wait_fences(vk);

      if (vk->emulating_mailbox && vk->mailbox.swapchain == VK_NULL_HANDLE)
      {
         vulkan_emulated_mailbox_init(&vk->mailbox, vk->context.device, vk->swapchain);
         vk->created_new_swapchain = false;
         return true;
      }
      else if (!vk->emulating_mailbox && vk->mailbox.swapchain != VK_NULL_HANDLE)
      {
         /* We are tearing down, and entering a state where we are supposed to have
          * acquired an image, so block until we have acquired. */
         if (!vk->context.has_acquired_swapchain)
            res = vulkan_emulated_mailbox_acquire_next_image_blocking(
                  &vk->mailbox,
                  &vk->context.current_swapchain_index);
         else
            res = VK_SUCCESS;

         vulkan_emulated_mailbox_deinit(&vk->mailbox);

         if (res == VK_SUCCESS)
         {
            vk->context.has_acquired_swapchain = true;
            vk->created_new_swapchain          = false;
            return true;
         }

         /* We failed for some reason, so create a new swapchain. */
         vk->context.has_acquired_swapchain = false;
      }
      else
      {
         vk->created_new_swapchain = false;
         return true;
      }
   }

   vulkan_emulated_mailbox_deinit(&vk->mailbox);

   present_mode_count = 0;
   vkGetPhysicalDeviceSurfacePresentModesKHR(
         vk->context.gpu, vk->vk_surface,
         &present_mode_count, NULL);
   if (present_mode_count < 1 || present_mode_count > 16)
   {
      RARCH_ERR("[Vulkan]: Bogus present modes found.\n");
      return false;
   }
   vkGetPhysicalDeviceSurfacePresentModesKHR(
         vk->context.gpu, vk->vk_surface,
         &present_mode_count, present_modes);

   for (i = 0; i < present_mode_count; i++)
   {
      RARCH_LOG("[Vulkan]: Swapchain supports present mode: %u.\n",
            present_modes[i]);
   }

   vk->context.swap_interval = swap_interval;
   for (i = 0; i < present_mode_count; i++)
   {
      if (!swap_interval && present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
      {
         swapchain_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
         break;
      }
      else if (!swap_interval && present_modes[i]
            == VK_PRESENT_MODE_IMMEDIATE_KHR)
      {
         swapchain_present_mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
         break;
      }
      else if (swap_interval && present_modes[i] == VK_PRESENT_MODE_FIFO_KHR)
      {
         /* Kind of tautological since FIFO must always be present. */
         swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
         break;
      }
   }

   RARCH_LOG("[Vulkan]: Creating swapchain with present mode: %u\n",
         (unsigned)swapchain_present_mode);

   vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vk->context.gpu,
         vk->vk_surface, &surface_properties);
   vkGetPhysicalDeviceSurfaceFormatsKHR(vk->context.gpu,
         vk->vk_surface, &format_count, NULL);
   vkGetPhysicalDeviceSurfaceFormatsKHR(vk->context.gpu,
         vk->vk_surface, &format_count, formats);

   format.format = VK_FORMAT_UNDEFINED;
   if (format_count == 1 && formats[0].format == VK_FORMAT_UNDEFINED)
   {
      format        = formats[0];
      format.format = VK_FORMAT_B8G8R8A8_UNORM;
   }
   else
   {
      if (format_count == 0)
      {
         RARCH_ERR("[Vulkan]: Surface has no formats.\n");
         return false;
      }

      for (i = 0; i < format_count; i++)
      {
         if (
               formats[i].format == VK_FORMAT_R8G8B8A8_UNORM ||
               formats[i].format == VK_FORMAT_B8G8R8A8_UNORM ||
               formats[i].format == VK_FORMAT_A8B8G8R8_UNORM_PACK32)
            format = formats[i];
      }

      if (format.format == VK_FORMAT_UNDEFINED)
         format = formats[0];
   }

   if (surface_properties.currentExtent.width == -1)
   {
      swapchain_size.width     = width;
      swapchain_size.height    = height;
   }
   else
      swapchain_size           = surface_properties.currentExtent;

#ifdef WSI_HARDENING_TEST
   if (trigger_spurious_error())
   {
      surface_properties.maxImageExtent.width = 0;
      surface_properties.maxImageExtent.height = 0;
      surface_properties.minImageExtent.width = 0;
      surface_properties.minImageExtent.height = 0;
   }
#endif

   /* Clamp swapchain size to boundaries. */
   if (swapchain_size.width > surface_properties.maxImageExtent.width)
      swapchain_size.width = surface_properties.maxImageExtent.width;
   if (swapchain_size.width < surface_properties.minImageExtent.width)
      swapchain_size.width = surface_properties.minImageExtent.width;
   if (swapchain_size.height > surface_properties.maxImageExtent.height)
      swapchain_size.height = surface_properties.maxImageExtent.height;
   if (swapchain_size.height < surface_properties.minImageExtent.height)
      swapchain_size.height = surface_properties.minImageExtent.height;

   if (swapchain_size.width == 0 && swapchain_size.height == 0)
   {
      /* Cannot create swapchain yet, try again later. */
      if (vk->swapchain != VK_NULL_HANDLE)
         vkDestroySwapchainKHR(vk->context.device, vk->swapchain, NULL);
      vk->swapchain                    = VK_NULL_HANDLE;
      vk->context.swapchain_width      = width;
      vk->context.swapchain_height     = height;
      vk->context.num_swapchain_images = 1;

      memset(vk->context.swapchain_images, 0, sizeof(vk->context.swapchain_images));
      RARCH_LOG("[Vulkan]: Cannot create a swapchain yet. Will try again later ...\n");
      return true;
   }

   RARCH_LOG("[Vulkan]: Using swapchain size %u x %u.\n",
         swapchain_size.width, swapchain_size.height);

   /* Unless we have other reasons to clamp, we should prefer 3 images.
    * We hard sync against the swapchain, so if we have 2 images,
    * we would be unable to overlap CPU and GPU, which can get very slow
    * for GPU-rendered cores. */
   desired_swapchain_images = settings->uints.video_max_swapchain_images;

   /* Clamp images requested to what is supported by the implementation. */
   if (desired_swapchain_images < surface_properties.minImageCount)
      desired_swapchain_images = surface_properties.minImageCount;

   if ((surface_properties.maxImageCount > 0)
         && (desired_swapchain_images > surface_properties.maxImageCount))
      desired_swapchain_images = surface_properties.maxImageCount;

   if (surface_properties.supportedTransforms
         & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
      pre_transform            = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
   else
      pre_transform            = surface_properties.currentTransform;

   if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
      composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
   else if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
      composite = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
   else if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
      composite = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
   else if (surface_properties.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
      composite = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;

   old_swapchain               = vk->swapchain;

   info.surface                = vk->vk_surface;
   info.minImageCount          = desired_swapchain_images;
   info.imageFormat            = format.format;
   info.imageColorSpace        = format.colorSpace;
   info.imageExtent.width      = swapchain_size.width;
   info.imageExtent.height     = swapchain_size.height;
   info.imageArrayLayers       = 1;
   info.imageSharingMode       = VK_SHARING_MODE_EXCLUSIVE;
   info.preTransform           = pre_transform;
   info.compositeAlpha         = composite;
   info.presentMode            = swapchain_present_mode;
   info.clipped                = true;
   info.oldSwapchain           = old_swapchain;
   info.imageUsage             = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
      | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

#ifdef _WIN32
   /* On Windows, do not try to reuse the swapchain.
    * It causes a lot of issues on nVidia for some reason. */
   info.oldSwapchain = VK_NULL_HANDLE;
   if (old_swapchain != VK_NULL_HANDLE)
      vkDestroySwapchainKHR(vk->context.device, old_swapchain, NULL);
#endif

   if (vkCreateSwapchainKHR(vk->context.device,
            &info, NULL, &vk->swapchain) != VK_SUCCESS)
   {
      RARCH_ERR("[Vulkan]: Failed to create swapchain.\n");
      return false;
   }

#ifndef _WIN32
   if (old_swapchain != VK_NULL_HANDLE)
      vkDestroySwapchainKHR(vk->context.device, old_swapchain, NULL);
#endif

   vk->context.swapchain_width  = swapchain_size.width;
   vk->context.swapchain_height = swapchain_size.height;

   /* Make sure we create a backbuffer format that is as we expect. */
   switch (format.format)
   {
      case VK_FORMAT_B8G8R8A8_SRGB:
         vk->context.swapchain_format  = VK_FORMAT_B8G8R8A8_UNORM;
         vk->context.swapchain_is_srgb = true;
         break;

      case VK_FORMAT_R8G8B8A8_SRGB:
         vk->context.swapchain_format  = VK_FORMAT_R8G8B8A8_UNORM;
         vk->context.swapchain_is_srgb = true;
         break;

      case VK_FORMAT_R8G8B8_SRGB:
         vk->context.swapchain_format  = VK_FORMAT_R8G8B8_UNORM;
         vk->context.swapchain_is_srgb = true;
         break;

      case VK_FORMAT_B8G8R8_SRGB:
         vk->context.swapchain_format  = VK_FORMAT_B8G8R8_UNORM;
         vk->context.swapchain_is_srgb = true;
         break;

      default:
         vk->context.swapchain_format  = format.format;
         break;
   }

   vkGetSwapchainImagesKHR(vk->context.device, vk->swapchain,
         &vk->context.num_swapchain_images, NULL);
   vkGetSwapchainImagesKHR(vk->context.device, vk->swapchain,
         &vk->context.num_swapchain_images, vk->context.swapchain_images);

   RARCH_LOG("[Vulkan]: Got %u swapchain images.\n",
         vk->context.num_swapchain_images);

   /* Force driver to reset swapchain image handles. */
   vk->context.invalid_swapchain      = true;
   vk->context.has_acquired_swapchain = false;
   vulkan_create_wait_fences(vk);

   if (vk->emulating_mailbox)
      vulkan_emulated_mailbox_init(&vk->mailbox, vk->context.device, vk->swapchain);

   return true;
}
