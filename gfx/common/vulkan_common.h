/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Hans-Kristian Arntzen
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

#ifndef VULKAN_COMMON_H__
#define VULKAN_COMMON_H__

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <lists/string_list.h>
#include <retro_inline.h>

#define VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS    16
#define VULKAN_MAX_DESCRIPTOR_POOL_SIZES        16
#define VULKAN_BUFFER_BLOCK_SIZE                (64 * 1024)

#define VULKAN_MAX_SWAPCHAIN_IMAGES             8

#define VULKAN_DIRTY_DYNAMIC_BIT                0x0001

#include "vksym.h"

#include <boolean.h>
#include <retro_inline.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>

#include <libretro.h>
#include <libretro_vulkan.h>

#include "../video_defines.h"
#include "../font_driver.h"
#include "../drivers_shader/shader_vulkan.h"
#include "../include/vulkan/vulkan.h"

#define VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd, img, levels, old_layout, new_layout, src_access, dst_access, src_stages, dst_stages, src_queue_family_idx, dst_queue_family_idx) \
{ \
   VkImageMemoryBarrier barrier; \
   barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; \
   barrier.pNext                           = NULL; \
   barrier.srcAccessMask                   = src_access; \
   barrier.dstAccessMask                   = dst_access; \
   barrier.oldLayout                       = old_layout; \
   barrier.newLayout                       = new_layout; \
   barrier.srcQueueFamilyIndex             = src_queue_family_idx; \
   barrier.dstQueueFamilyIndex             = dst_queue_family_idx; \
   barrier.image                           = img; \
   barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT; \
   barrier.subresourceRange.baseMipLevel   = 0; \
   barrier.subresourceRange.levelCount     = levels; \
   barrier.subresourceRange.baseArrayLayer = 0; \
   barrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS; \
   vkCmdPipelineBarrier(cmd, src_stages, dst_stages, 0, 0, NULL, 0, NULL, 1, &barrier); \
}

#define VULKAN_IMAGE_LAYOUT_TRANSITION(cmd, img, old_layout, new_layout, src_access, dst_access, src_stages, dst_stages) VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd, img, VK_REMAINING_MIP_LEVELS, old_layout, new_layout, src_access, dst_access, src_stages, dst_stages, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED)

#define VULKAN_SET_UNIFORM_BUFFER(_device, _set, _binding, _buffer, _offset, _range) \
{ \
   VkWriteDescriptorSet write; \
   VkDescriptorBufferInfo buffer_info; \
   buffer_info.buffer         = _buffer; \
   buffer_info.offset         = _offset; \
   buffer_info.range          = _range; \
   write.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; \
   write.pNext                = NULL; \
   write.dstSet               = _set; \
   write.dstBinding           = _binding; \
   write.dstArrayElement      = 0; \
   write.descriptorCount      = 1; \
   write.descriptorType       = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; \
   write.pImageInfo           = NULL; \
   write.pBufferInfo          = &buffer_info; \
   write.pTexelBufferView     = NULL; \
   vkUpdateDescriptorSets(_device, 1, &write, 0, NULL); \
}

RETRO_BEGIN_DECLS

enum vk_flags
{
   VK_FLAG_VSYNC               = (1 << 0),
   VK_FLAG_KEEP_ASPECT         = (1 << 1),
   VK_FLAG_FULLSCREEN          = (1 << 2),
   VK_FLAG_QUITTING            = (1 << 3),
   VK_FLAG_SHOULD_RESIZE       = (1 << 4),
   VK_FLAG_TRACKER_USE_SCISSOR = (1 << 5),
   VK_FLAG_HW_ENABLE           = (1 << 6),
   VK_FLAG_HW_VALID_SEMAPHORE  = (1 << 7),
   VK_FLAG_MENU_ENABLE         = (1 << 8),
   VK_FLAG_MENU_FULLSCREEN     = (1 << 9),
   VK_FLAG_HDR_SUPPORT         = (1 << 10),
   VK_FLAG_DISPLAY_BLEND       = (1 << 11),
   VK_FLAG_READBACK_PENDING    = (1 << 12),
   VK_FLAG_READBACK_STREAMED   = (1 << 13),
   VK_FLAG_OVERLAY_ENABLE      = (1 << 14),
   VK_FLAG_OVERLAY_FULLSCREEN  = (1 << 15)
};

enum vk_texture_type
{
   /* We will use the texture as a sampled linear texture. */
   VULKAN_TEXTURE_STREAMED = 0,

   /* We will use the texture as a linear texture, but only
    * for copying to a DYNAMIC texture. */
   VULKAN_TEXTURE_STAGING,

   /* We will use the texture as an optimally tiled texture,
    * and we will update the texture by copying from STAGING
    * textures. */
   VULKAN_TEXTURE_DYNAMIC,

   /* We will upload content once. */
   VULKAN_TEXTURE_STATIC,

   /* We will use the texture for reading back transfers from GPU. */
   VULKAN_TEXTURE_READBACK
};

enum vulkan_wsi_type
{
   VULKAN_WSI_NONE = 0,
   VULKAN_WSI_WAYLAND,
   VULKAN_WSI_MIR,
   VULKAN_WSI_ANDROID,
   VULKAN_WSI_WIN32,
   VULKAN_WSI_XCB,
   VULKAN_WSI_XLIB,
   VULKAN_WSI_DISPLAY,
   VULKAN_WSI_MVK_MACOS,
   VULKAN_WSI_MVK_IOS,
};

enum vulkan_context_flags
{
   VK_CTX_FLAG_INVALID_SWAPCHAIN            = (1 << 0),
   VK_CTX_FLAG_HDR_ENABLE                   = (1 << 1),
   /* Used by screenshot to get blits with correct colorspace. */
   VK_CTX_FLAG_SWAPCHAIN_IS_SRGB            = (1 << 2),
   VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK = (1 << 3),
   VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN       = (1 << 4),
   /* Whether HDR colorspaces are supported by the instance */
   VK_CTX_FLAG_HDR_SUPPORT                  = (1 << 5),
   /* scRGB mode: RGBA16F swapchain with extended linear sRGB colour space */
   VK_CTX_FLAG_HDR_SCRGB                    = (1 << 6),
};

enum vulkan_emulated_mailbox_flags
{
   VK_MAILBOX_FLAG_ACQUIRED            = (1 << 0),
   VK_MAILBOX_FLAG_REQUEST_ACQUIRE     = (1 << 1),
   VK_MAILBOX_FLAG_DEAD                = (1 << 2),
   VK_MAILBOX_FLAG_HAS_PENDING_REQUEST = (1 << 3)
};

enum gfx_ctx_vulkan_data_flags
{
   /* If set, prefer a path where we use
    * semaphores instead of fences for vkAcquireNextImageKHR.
    * Helps workaround certain performance issues on some drivers. */
   VK_DATA_FLAG_USE_WSI_SEMAPHORE       = (1 << 0),
   VK_DATA_FLAG_NEED_NEW_SWAPCHAIN      = (1 << 1),
   VK_DATA_FLAG_CREATED_NEW_SWAPCHAIN   = (1 << 2),
   VK_DATA_FLAG_EMULATE_MAILBOX         = (1 << 3),
   VK_DATA_FLAG_EMULATING_MAILBOX       = (1 << 4),
   /* Used to check if we need to use mailbox emulation or not.
    * Only relevant on Windows for now. */
   VK_DATA_FLAG_FULLSCREEN              = (1 << 5)
};

enum vk_texture_flags
{
   VK_TEX_FLAG_DEFAULT_SMOOTH               = (1 << 0),
   VK_TEX_FLAG_NEED_MANUAL_CACHE_MANAGEMENT = (1 << 1),
   VK_TEX_FLAG_MIPMAP                       = (1 << 2)
};

typedef struct vulkan_context
{
   slock_t *queue_lock;
   retro_vulkan_destroy_device_t destroy_device;   /* ptr alignment */

   VkInstance instance;
   VkPhysicalDevice gpu;
   VkDevice device;
   VkQueue queue;

   VkPhysicalDeviceProperties gpu_properties;
   VkPhysicalDeviceMemoryProperties memory_properties;

   VkPresentModeKHR present_modes[16];
   VkImage swapchain_images[VULKAN_MAX_SWAPCHAIN_IMAGES];
   VkFence swapchain_fences[VULKAN_MAX_SWAPCHAIN_IMAGES];
   VkFormat swapchain_format;
#ifdef VULKAN_HDR_SWAPCHAIN
   VkColorSpaceKHR swapchain_colour_space;
#endif /* VULKAN_HDR_SWAPCHAIN */

   VkSemaphore swapchain_semaphores[VULKAN_MAX_SWAPCHAIN_IMAGES];
   VkSemaphore swapchain_acquire_semaphore;
   VkSemaphore swapchain_recycled_semaphores[VULKAN_MAX_SWAPCHAIN_IMAGES];
   VkSemaphore swapchain_wait_semaphores[VULKAN_MAX_SWAPCHAIN_IMAGES];

#ifdef VULKAN_DEBUG
   VkDebugUtilsMessengerEXT debug_callback;
#endif
   uint32_t graphics_queue_index;
   uint32_t num_swapchain_images;
   uint32_t current_swapchain_index;
   uint32_t current_frame_index;

   unsigned swapchain_width;
   unsigned swapchain_height;
   unsigned num_recycled_acquire_semaphores;

   int8_t swap_interval;
   uint8_t flags;

   bool swapchain_fences_signalled[VULKAN_MAX_SWAPCHAIN_IMAGES];
} vulkan_context_t;

struct vulkan_emulated_mailbox
{
   sthread_t *thread;
   slock_t *lock;
   scond_t *cond;
   VkDevice device;              /* ptr alignment */
   VkSwapchainKHR swapchain;     /* ptr alignment */

   unsigned index;
   VkResult result;              /* enum alignment */
   uint8_t flags;
};

typedef struct gfx_ctx_vulkan_data
{
   struct string_list *gpu_list;
   vulkan_context_t context;
   VkSurfaceKHR vk_surface;      /* ptr alignment */
   VkSwapchainKHR swapchain;     /* ptr alignment */
   struct vulkan_emulated_mailbox mailbox;
   uint8_t flags;
   enum vulkan_wsi_type wsi_type;
   bool fse_supported;
} gfx_ctx_vulkan_data_t;

struct vulkan_display_surface_info
{
   unsigned width;
   unsigned height;
   unsigned monitor_index;
   unsigned refresh_rate_x1000;
};

struct vk_buffer
{
   VkDeviceSize size;      /* uint64_t alignment */
   void *mapped;
   VkBuffer buffer;        /* ptr alignment */
   VkDeviceMemory memory;  /* ptr alignment */
};

struct vk_buffer_node
{
   struct vk_buffer buffer;      /* uint64_t alignment */
   struct vk_buffer_node *next;
};

struct vk_buffer_chain
{
   VkDeviceSize block_size; /* uint64_t alignment */
   VkDeviceSize alignment;  /* uint64_t alignment */
   VkDeviceSize offset;     /* uint64_t alignment */
   struct vk_buffer_node *head;
   struct vk_buffer_node *current;
   VkBufferUsageFlags usage; /* uint32_t alignment */
};

struct vk_buffer_range
{
   VkDeviceSize offset; /* uint64_t alignment */
   uint8_t *data;
   VkBuffer buffer;     /* ptr alignment */
};

struct vk_descriptor_pool
{
   struct vk_descriptor_pool *next;
   VkDescriptorPool pool; /* ptr alignment */
   VkDescriptorSet sets[VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS]; /* ptr alignment */
};

struct vk_descriptor_manager
{
   struct vk_descriptor_pool *head;
   struct vk_descriptor_pool *current;
   VkDescriptorSetLayout set_layout; /* ptr alignment */
   VkDescriptorPoolSize sizes[VULKAN_MAX_DESCRIPTOR_POOL_SIZES]; /* uint32_t alignment */
   unsigned count;
   unsigned num_sizes;
};

uint32_t vulkan_find_memory_type(
      const VkPhysicalDeviceMemoryProperties *mem_props,
      uint32_t device_reqs, uint32_t host_reqs);

uint32_t vulkan_find_memory_type_fallback(
      const VkPhysicalDeviceMemoryProperties *mem_props,
      uint32_t device_reqs, uint32_t host_reqs_first,
      uint32_t host_reqs_second);

void vulkan_debug_mark_buffer(VkDevice device, VkBuffer buffer);

bool vulkan_context_init(gfx_ctx_vulkan_data_t *vk,
      enum vulkan_wsi_type type);

void vulkan_context_destroy(gfx_ctx_vulkan_data_t *vk,
      bool destroy_surface);

bool vulkan_surface_create(gfx_ctx_vulkan_data_t *vk,
      enum vulkan_wsi_type type,
      void *display, void *surface,
      unsigned width, unsigned height,
      int8_t swap_interval);

void vulkan_present(gfx_ctx_vulkan_data_t *vk, unsigned index);

void vulkan_acquire_next_image(gfx_ctx_vulkan_data_t *vk);

bool vulkan_create_swapchain(gfx_ctx_vulkan_data_t *vk,
      unsigned width, unsigned height,
      int8_t swap_interval);

void vulkan_debug_mark_image(VkDevice device, VkImage image);
void vulkan_debug_mark_memory(VkDevice device, VkDeviceMemory memory);

#ifdef VULKAN_HDR_SWAPCHAIN
bool vulkan_is_hdr10_format(VkFormat format);
#endif /* VULKAN_HDR_SWAPCHAIN */

RETRO_END_DECLS

#endif
