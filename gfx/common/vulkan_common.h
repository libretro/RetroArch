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


#define VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS    16
#define VULKAN_MAX_DESCRIPTOR_POOL_SIZES        16
#define VULKAN_BUFFER_BLOCK_SIZE                (64 * 1024)

#define VULKAN_MAX_SWAPCHAIN_IMAGES             8

#define VULKAN_DIRTY_DYNAMIC_BIT                0x0001

#define VULKAN_HDR_SWAPCHAIN

#include "vksym.h"

#include <boolean.h>
#include <retro_inline.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>
#include <gfx/math/matrix_4x4.h>
#include <gfx/scaler/scaler.h>
#include <rthreads/rthreads.h>
#include <formats/image.h>

#include <libretro.h>
#include <libretro_vulkan.h>

#include "../video_defines.h"
#include "../font_driver.h"
#include "../drivers_shader/shader_vulkan.h"
#include "../include/vulkan/vulkan.h"

RETRO_BEGIN_DECLS

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

#ifdef VULKAN_HDR_SWAPCHAIN

#ifndef VKALIGN
#ifdef _MSC_VER
#define VKALIGN(x) __declspec(align(x))
#else
#define VKALIGN(x) __attribute__((aligned(x)))
#endif
#endif

typedef struct VKALIGN(16)
{
   math_matrix_4x4   mvp;
   float             contrast;         /* 2.0f    */
   float             paper_white_nits; /* 200.0f  */
   float             max_nits;         /* 1000.0f */
   float             expand_gamut;     /* 1.0f    */
   float             inverse_tonemap;  /* 1.0f    */
   float             hdr10;            /* 1.0f    */
} vulkan_hdr_uniform_t;
#endif /* VULKAN_HDR_SWAPCHAIN */

enum vulkan_context_flags
{
   VK_CTX_FLAG_INVALID_SWAPCHAIN            = (1 << 0),
   VK_CTX_FLAG_HDR_ENABLE                   = (1 << 1),
   /* Used by screenshot to get blits with correct colorspace. */
   VK_CTX_FLAG_SWAPCHAIN_IS_SRGB            = (1 << 2),
   VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK = (1 << 3),
   VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN       = (1 << 4)
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
   unsigned swap_interval;
   unsigned num_recycled_acquire_semaphores;

   uint8_t flags;

   bool swapchain_fences_signalled[VULKAN_MAX_SWAPCHAIN_IMAGES];
} vulkan_context_t;

enum vulkan_emulated_mailbox_flags
{
   VK_MAILBOX_FLAG_ACQUIRED            = (1 << 0),
   VK_MAILBOX_FLAG_REQUEST_ACQUIRE     = (1 << 1),
   VK_MAILBOX_FLAG_DEAD                = (1 << 2),
   VK_MAILBOX_FLAG_HAS_PENDING_REQUEST = (1 << 3)
};

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

typedef struct gfx_ctx_vulkan_data
{
   struct string_list *gpu_list;
   vulkan_context_t context;
   VkSurfaceKHR vk_surface;      /* ptr alignment */
   VkSwapchainKHR swapchain;     /* ptr alignment */
   struct vulkan_emulated_mailbox mailbox;
   uint8_t flags;
} gfx_ctx_vulkan_data_t;

struct vulkan_display_surface_info
{
   unsigned width;
   unsigned height;
   unsigned monitor_index;
};

struct vk_color
{
   float r, g, b, a;
};

struct vk_vertex
{
   float x, y;
   float tex_x, tex_y;
   struct vk_color color;        /* float alignment */
};

struct vk_image
{
   VkImage image;                /* ptr alignment */
   VkImageView view;             /* ptr alignment */
   VkFramebuffer framebuffer;    /* ptr alignment */
   VkDeviceMemory memory;        /* ptr alignment */
};

enum vk_texture_flags
{
   VK_TEX_FLAG_DEFAULT_SMOOTH               = (1 << 0),
   VK_TEX_FLAG_NEED_MANUAL_CACHE_MANAGEMENT = (1 << 1),
   VK_TEX_FLAG_MIPMAP                       = (1 << 2)
};

struct vk_texture
{
   VkDeviceSize memory_size;     /* uint64_t alignment */

   void *mapped;
   VkImage image;                /* ptr alignment */
   VkImageView view;             /* ptr alignment */
   VkBuffer buffer;              /* ptr alignment */
   VkDeviceMemory memory;        /* ptr alignment */

   size_t offset;
   size_t stride;
   size_t size;
   uint32_t memory_type;
   unsigned width, height;

   VkImageLayout layout;         /* enum alignment */
   VkFormat format;              /* enum alignment */
   enum vk_texture_type type;
   uint8_t flags;
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

struct vk_per_frame
{
   struct vk_texture texture;          /* uint64_t alignment */
   struct vk_texture texture_optimal;
   struct vk_buffer_chain vbo;         /* uint64_t alignment */
   struct vk_buffer_chain ubo;
   struct vk_descriptor_manager descriptor_manager;

   VkCommandPool cmd_pool; /* ptr alignment */
   VkCommandBuffer cmd;    /* ptr alignment */
};

struct vk_draw_quad
{
   struct vk_texture *texture;
   const math_matrix_4x4 *mvp;
   VkPipeline pipeline;          /* ptr alignment */
   VkSampler sampler;            /* ptr alignment */
   struct vk_color color;        /* float alignment */
};

struct vk_draw_triangles
{
   const void *uniform;
   const struct vk_buffer_range *vbo;
   struct vk_texture *texture;
   VkPipeline pipeline;          /* ptr alignment */
   VkSampler sampler;            /* ptr alignment */
   size_t uniform_size;
   unsigned vertices;
};

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

typedef struct vk
{
   vulkan_filter_chain_t *filter_chain;
   vulkan_context_t *context;
   void *ctx_data;
   const gfx_ctx_driver_t *ctx_driver;
   struct vk_per_frame *chain;
   struct vk_image *backbuffer;
#ifdef VULKAN_HDR_SWAPCHAIN
   struct vk_image main_buffer;
#endif /* VULKAN_HDR_SWAPCHAIN */

   unsigned video_width;
   unsigned video_height;

   unsigned tex_w, tex_h;
   unsigned vp_out_width, vp_out_height;
   unsigned rotation;
   unsigned num_swapchain_images;
   unsigned last_valid_index;

   video_info_t video;

   VkFormat tex_fmt;
   math_matrix_4x4 mvp, mvp_no_rot; /* float alignment */
   VkViewport vk_vp;
   VkRenderPass render_pass;
   struct video_viewport vp;
   struct vk_per_frame swapchain[VULKAN_MAX_SWAPCHAIN_IMAGES];
   struct vk_image backbuffers[VULKAN_MAX_SWAPCHAIN_IMAGES];
   struct vk_texture default_texture;

   /* Currently active command buffer. */
   VkCommandBuffer cmd;
   /* Staging pool for doing buffer transfers on GPU. */
   VkCommandPool staging_pool;

   struct
   {
      struct scaler_ctx scaler_bgr;
      struct scaler_ctx scaler_rgb;
      struct vk_texture staging[VULKAN_MAX_SWAPCHAIN_IMAGES];
   } readback;

   struct
   {
      struct vk_texture *images;
      struct vk_vertex *vertex;
      unsigned count;
   } overlay;

   struct
   {
      VkPipeline alpha_blend;
      VkPipeline font;
#ifdef VULKAN_HDR_SWAPCHAIN
      VkPipeline hdr;
#endif /* VULKAN_HDR_SWAPCHAIN */
      VkDescriptorSetLayout set_layout;
      VkPipelineLayout layout;
      VkPipelineCache cache;
   } pipelines;

   struct
   {
      VkPipeline pipelines[8 * 2];
      struct vk_texture blank_texture;
   } display;

#ifdef VULKAN_HDR_SWAPCHAIN
   struct
   {
      struct vk_buffer  ubo;
      float             max_output_nits;
      float             min_output_nits;
      float             max_cll;
      float             max_fall;
   } hdr;
#endif /* VULKAN_HDR_SWAPCHAIN */

   struct
   {
      struct vk_texture textures[VULKAN_MAX_SWAPCHAIN_IMAGES];
      struct vk_texture textures_optimal[VULKAN_MAX_SWAPCHAIN_IMAGES];
      unsigned last_index;
      float alpha;
      bool dirty[VULKAN_MAX_SWAPCHAIN_IMAGES];
   } menu;

   struct
   {
      VkSampler linear;
      VkSampler nearest;
      VkSampler mipmap_nearest;
      VkSampler mipmap_linear;
   } samplers;

   struct
   {
      const struct retro_vulkan_image *image;
      VkPipelineStageFlags *wait_dst_stages;
      VkCommandBuffer *cmd;
      VkSemaphore *semaphores;
      VkSemaphore signal_semaphore; /* ptr alignment */

      struct retro_hw_render_interface_vulkan iface;

      unsigned capacity_cmd;
      unsigned last_width;
      unsigned last_height;
      uint32_t num_semaphores;
      uint32_t num_cmd;
      uint32_t src_queue_family;

   } hw;

   struct
   {
      uint64_t dirty;
      VkPipeline pipeline; /* ptr alignment */
      VkImageView view;    /* ptr alignment */
      VkSampler sampler;   /* ptr alignment */
      math_matrix_4x4 mvp;
      VkRect2D scissor;    /* int32_t alignment */
   } tracker;
   uint32_t flags;
} vk_t;

#define VK_BUFFER_CHAIN_DISCARD(chain) \
{ \
   chain->current = chain->head; \
   chain->offset  = 0; \
}

#define VULKAN_SYNC_TEXTURE_TO_GPU(device, tex_memory) \
{ \
   VkMappedMemoryRange range; \
   range.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE; \
   range.pNext  = NULL; \
   range.memory = tex_memory; \
   range.offset = 0; \
   range.size   = VK_WHOLE_SIZE; \
   vkFlushMappedMemoryRanges(device, 1, &range); \
}

#define VULKAN_SYNC_TEXTURE_TO_CPU(device, tex_memory) \
{ \
   VkMappedMemoryRange range; \
   range.sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE; \
   range.pNext  = NULL; \
   range.memory = tex_memory; \
   range.offset = 0; \
   range.size   = VK_WHOLE_SIZE; \
   vkInvalidateMappedMemoryRanges(device, 1, &range); \
}

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

#define VULKAN_TRANSFER_IMAGE_OWNERSHIP(cmd, img, layout, src_stages, dst_stages, src_queue_family, dst_queue_family) VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd, img, VK_REMAINING_MIP_LEVELS, layout, layout, 0, 0, src_stages, dst_stages, src_queue_family, dst_queue_family)

#define VULKAN_IMAGE_LAYOUT_TRANSITION(cmd, img, old_layout, new_layout, src_access, dst_access, src_stages, dst_stages) VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd, img, VK_REMAINING_MIP_LEVELS, old_layout, new_layout, src_access, dst_access, src_stages, dst_stages, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED)

#define VK_DESCRIPTOR_MANAGER_RESTART(manager) \
{ \
   manager->current = manager->head; \
   manager->count = 0; \
}

#define VK_MAP_PERSISTENT_TEXTURE(device, texture) \
{ \
   vkMapMemory(device, texture->memory, texture->offset, texture->size, 0, &texture->mapped); \
}

#define VULKAN_PASS_SET_TEXTURE(device, set, _sampler, binding, image_view, image_layout) \
{ \
   VkDescriptorImageInfo image_info; \
   VkWriteDescriptorSet write; \
   image_info.sampler         = _sampler; \
   image_info.imageView       = image_view; \
   image_info.imageLayout     = image_layout; \
   write.sType                = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; \
   write.pNext                = NULL; \
   write.dstSet               = set; \
   write.dstBinding           = binding; \
   write.dstArrayElement      = 0; \
   write.descriptorCount      = 1; \
   write.descriptorType       = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; \
   write.pImageInfo           = &image_info; \
   write.pBufferInfo          = NULL; \
   write.pTexelBufferView     = NULL; \
   vkUpdateDescriptorSets(device, 1, &write, 0, NULL); \
}

#define VULKAN_WRITE_QUAD_VBO(pv, _x, _y, _width, _height, _tex_x, _tex_y, _tex_width, _tex_height, vulkan_color) \
{ \
   float r        = (vulkan_color)->r; \
   float g        = (vulkan_color)->g; \
   float b        = (vulkan_color)->b; \
   float a        = (vulkan_color)->a; \
   pv[0].x        = (_x)     + 0.0f * (_width); \
   pv[0].y        = (_y)     + 0.0f * (_height); \
   pv[0].tex_x    = (_tex_x) + 0.0f * (_tex_width); \
   pv[0].tex_y    = (_tex_y) + 0.0f * (_tex_height); \
   pv[0].color.r  = r; \
   pv[0].color.g  = g; \
   pv[0].color.b  = b; \
   pv[0].color.a  = a; \
   pv[1].x        = (_x)     + 0.0f * (_width); \
   pv[1].y        = (_y)     + 1.0f * (_height); \
   pv[1].tex_x    = (_tex_x) + 0.0f * (_tex_width); \
   pv[1].tex_y    = (_tex_y) + 1.0f * (_tex_height); \
   pv[1].color.r  = r; \
   pv[1].color.g  = g; \
   pv[1].color.b  = b; \
   pv[1].color.a  = a; \
   pv[2].x        = (_x)     + 1.0f * (_width); \
   pv[2].y        = (_y)     + 0.0f * (_height); \
   pv[2].tex_x    = (_tex_x) + 1.0f * (_tex_width); \
   pv[2].tex_y    = (_tex_y) + 0.0f * (_tex_height); \
   pv[2].color.r  = r; \
   pv[2].color.g  = g; \
   pv[2].color.b  = b; \
   pv[2].color.a  = a; \
   pv[3].x        = (_x)     + 1.0f * (_width); \
   pv[3].y        = (_y)     + 1.0f * (_height); \
   pv[3].tex_x    = (_tex_x) + 1.0f * (_tex_width); \
   pv[3].tex_y    = (_tex_y) + 1.0f * (_tex_height); \
   pv[3].color.r  = r; \
   pv[3].color.g  = g; \
   pv[3].color.b  = b; \
   pv[3].color.a  = a; \
   pv[4].x        = (_x)     + 1.0f * (_width); \
   pv[4].y        = (_y)     + 0.0f * (_height); \
   pv[4].tex_x    = (_tex_x) + 1.0f * (_tex_width); \
   pv[4].tex_y    = (_tex_y) + 0.0f * (_tex_height); \
   pv[4].color.r  = r; \
   pv[4].color.g  = g; \
   pv[4].color.b  = b; \
   pv[4].color.a  = a; \
   pv[5].x        = (_x)     + 0.0f * (_width); \
   pv[5].y        = (_y)     + 1.0f * (_height); \
   pv[5].tex_x    = (_tex_x) + 0.0f * (_tex_width); \
   pv[5].tex_y    = (_tex_y) + 1.0f * (_tex_height); \
   pv[5].color.r  = r; \
   pv[5].color.g  = g; \
   pv[5].color.b  = b; \
   pv[5].color.a  = a; \
}

bool vulkan_buffer_chain_alloc(const struct vulkan_context *context,
      struct vk_buffer_chain *chain, size_t size,
      struct vk_buffer_range *range);

struct vk_descriptor_pool *vulkan_alloc_descriptor_pool(
      VkDevice device,
      const struct vk_descriptor_manager *manager);

uint32_t vulkan_find_memory_type(
      const VkPhysicalDeviceMemoryProperties *mem_props,
      uint32_t device_reqs, uint32_t host_reqs);

uint32_t vulkan_find_memory_type_fallback(
      const VkPhysicalDeviceMemoryProperties *mem_props,
      uint32_t device_reqs, uint32_t host_reqs_first,
      uint32_t host_reqs_second);

struct vk_texture vulkan_create_texture(vk_t *vk,
      struct vk_texture *old,
      unsigned width, unsigned height,
      VkFormat format,
      const void *initial, const VkComponentMapping *swizzle,
      enum vk_texture_type type);

void vulkan_transition_texture(vk_t *vk, VkCommandBuffer cmd, struct vk_texture *texture);

void vulkan_destroy_texture(
      VkDevice device,
      struct vk_texture *tex);

/* Dynamic texture type should be set to : VULKAN_TEXTURE_DYNAMIC
 * Staging texture type should be set to : VULKAN_TEXTURE_STAGING
 */
#define VULKAN_COPY_STAGING_TO_DYNAMIC(vk, cmd, dynamic, staging) \
{ \
   VkBufferImageCopy region; \
   VULKAN_IMAGE_LAYOUT_TRANSITION( \
         cmd, \
         dynamic->image, \
         VK_IMAGE_LAYOUT_UNDEFINED, \
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, \
         0, \
         VK_ACCESS_TRANSFER_WRITE_BIT, \
         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, \
         VK_PIPELINE_STAGE_TRANSFER_BIT); \
   region.bufferOffset                    = 0; \
   region.bufferRowLength                 = 0; \
   region.bufferImageHeight               = 0; \
   region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT; \
   region.imageSubresource.mipLevel       = 0; \
   region.imageSubresource.baseArrayLayer = 0; \
   region.imageSubresource.layerCount     = 1; \
   region.imageOffset.x                   = 0; \
   region.imageOffset.y                   = 0; \
   region.imageOffset.z                   = 0; \
   region.imageExtent.width               = dynamic->width; \
   region.imageExtent.height              = dynamic->height; \
   region.imageExtent.depth               = 1; \
   vkCmdCopyBufferToImage( \
         cmd, \
         staging->buffer, \
         dynamic->image, \
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, \
         1, \
         &region); \
   VULKAN_IMAGE_LAYOUT_TRANSITION( \
         cmd, \
         dynamic->image, \
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, \
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, \
         VK_ACCESS_TRANSFER_WRITE_BIT, \
         VK_ACCESS_SHADER_READ_BIT, \
         VK_PIPELINE_STAGE_TRANSFER_BIT, \
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT); \
   dynamic->layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; \
}

/* We don't have to sync against previous TRANSFER,
 * since we observed the completion by fences.
 *
 * If we have a single texture_optimal, we would need to sync against
 * previous transfers to avoid races.
 *
 * We would also need to optionally maintain extra textures due to
 * changes in resolution, so this seems like the sanest and
 * simplest solution. */
#define VULKAN_SYNC_TEXTURE_TO_GPU_COND_PTR(vk, tex) \
   if (((tex)->flags & VK_TEX_FLAG_NEED_MANUAL_CACHE_MANAGEMENT) && (tex)->memory != VK_NULL_HANDLE) \
      VULKAN_SYNC_TEXTURE_TO_GPU(vk->context->device, (tex)->memory) \

#define VULKAN_SYNC_TEXTURE_TO_GPU_COND_OBJ(vk, tex) \
   if (((tex).flags & VK_TEX_FLAG_NEED_MANUAL_CACHE_MANAGEMENT) && (tex).memory != VK_NULL_HANDLE) \
      VULKAN_SYNC_TEXTURE_TO_GPU(vk->context->device, (tex).memory) \

/* VBO will be written to here. */
void vulkan_draw_quad(vk_t *vk, const struct vk_draw_quad *quad);

/* The VBO needs to be written to before calling this.
 * Use vulkan_buffer_chain_alloc.
 */
void vulkan_draw_triangles(vk_t *vk, const struct vk_draw_triangles *call);

static INLINE unsigned vulkan_format_to_bpp(VkFormat format)
{
   switch (format)
   {
      case VK_FORMAT_B8G8R8A8_UNORM:
         return 4;

      case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
      case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
      case VK_FORMAT_R5G6B5_UNORM_PACK16:
         return 2;

      case VK_FORMAT_R8_UNORM:
         return 1;

      default: /* Unknown format */
	 break;
   }

   return 0;
}

struct vk_buffer vulkan_create_buffer(
      const struct vulkan_context *context,
      size_t size, VkBufferUsageFlags usage);

void vulkan_destroy_buffer(
      VkDevice device,
      struct vk_buffer *buffer);

VkDescriptorSet vulkan_descriptor_manager_alloc(
      VkDevice device,
      struct vk_descriptor_manager *manager);

bool vulkan_context_init(gfx_ctx_vulkan_data_t *vk,
      enum vulkan_wsi_type type);

void vulkan_context_destroy(gfx_ctx_vulkan_data_t *vk,
      bool destroy_surface);

bool vulkan_surface_create(gfx_ctx_vulkan_data_t *vk,
      enum vulkan_wsi_type type,
      void *display, void *surface,
      unsigned width, unsigned height,
      unsigned swap_interval);

void vulkan_present(gfx_ctx_vulkan_data_t *vk, unsigned index);

void vulkan_acquire_next_image(gfx_ctx_vulkan_data_t *vk);

bool vulkan_create_swapchain(gfx_ctx_vulkan_data_t *vk,
      unsigned width, unsigned height,
      unsigned swap_interval);

void vulkan_set_uniform_buffer(
      VkDevice device,
      VkDescriptorSet set,
      unsigned binding,
      VkBuffer buffer,
      VkDeviceSize offset,
      VkDeviceSize range);

void vulkan_debug_mark_buffer(VkDevice device, VkBuffer buffer);
void vulkan_debug_mark_image(VkDevice device, VkImage image);
void vulkan_debug_mark_memory(VkDevice device, VkDeviceMemory memory);

RETRO_END_DECLS

#endif
