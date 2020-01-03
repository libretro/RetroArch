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
#include <gfx/math/matrix_4x4.h>
#include <gfx/scaler/scaler.h>
#include <rthreads/rthreads.h>
#include <formats/image.h>

#include <libretro.h>
#include <libretro_vulkan.h>

#include "../../driver.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
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

typedef struct vulkan_context
{
   bool invalid_swapchain;
   /* Used by screenshot to get blits with correct colorspace. */
   bool swapchain_is_srgb;
   bool swap_interval_emulation_lock;
   bool has_acquired_swapchain;

   unsigned swapchain_width;
   unsigned swapchain_height;
   unsigned swap_interval;

   uint32_t graphics_queue_index;
   uint32_t num_swapchain_images;
   uint32_t current_swapchain_index;

   VkInstance instance;
   VkPhysicalDevice gpu;
   VkDevice device;
   VkQueue queue;

   VkPhysicalDeviceProperties gpu_properties;
   VkPhysicalDeviceMemoryProperties memory_properties;

   VkImage swapchain_images[VULKAN_MAX_SWAPCHAIN_IMAGES];
   VkFence swapchain_fences[VULKAN_MAX_SWAPCHAIN_IMAGES];
   bool swapchain_fences_signalled[VULKAN_MAX_SWAPCHAIN_IMAGES];
   VkSemaphore swapchain_semaphores[VULKAN_MAX_SWAPCHAIN_IMAGES];
   VkFormat swapchain_format;

   slock_t *queue_lock;
   retro_vulkan_destroy_device_t destroy_device;

#ifdef VULKAN_DEBUG
   VkDebugReportCallbackEXT debug_callback;
#endif
} vulkan_context_t;

struct vulkan_emulated_mailbox
{
   sthread_t *thread;
   VkDevice device;
   VkSwapchainKHR swapchain;
   slock_t *lock;
   scond_t *cond;

   unsigned index;
   bool acquired;
   bool request_acquire;
   bool dead;
   bool has_pending_request;
   VkResult result;
};

bool vulkan_emulated_mailbox_init(struct vulkan_emulated_mailbox *mailbox,
      VkDevice device, VkSwapchainKHR swapchain);
void vulkan_emulated_mailbox_deinit(struct vulkan_emulated_mailbox *mailbox);
VkResult vulkan_emulated_mailbox_acquire_next_image(struct vulkan_emulated_mailbox *mailbox, unsigned *index);
VkResult vulkan_emulated_mailbox_acquire_next_image_blocking(struct vulkan_emulated_mailbox *mailbox, unsigned *index);

typedef struct gfx_ctx_vulkan_data
{
   bool need_new_swapchain;
   bool created_new_swapchain;
   bool emulate_mailbox;
   bool emulating_mailbox;
   vulkan_context_t context;
   VkSurfaceKHR vk_surface;
   VkSwapchainKHR swapchain;

   struct vulkan_emulated_mailbox mailbox;
   /* Used to check if we need to use mailbox emulation or not.
    * Only relevant on Windows for now. */
   bool fullscreen;
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
   struct vk_color color;
};

struct vk_image
{
   VkImage image;
   VkImageView view;
   VkFramebuffer framebuffer;
};

struct vk_texture
{
   enum vk_texture_type type;
   bool default_smooth;
   bool need_manual_cache_management;
   bool mipmap;
   uint32_t memory_type;
   unsigned width, height;
   size_t offset;
   size_t stride;
   size_t size;

   void *mapped;

   VkImage image;
   VkImageView view;
   VkDeviceMemory memory;
   VkBuffer buffer;

   VkFormat format;

   VkDeviceSize memory_size;

   VkImageLayout layout;
};

struct vk_buffer
{
   VkBuffer buffer;
   VkDeviceMemory memory;
   VkDeviceSize size;
   void *mapped;
};

struct vk_buffer_node
{
   struct vk_buffer buffer;
   struct vk_buffer_node *next;
};

struct vk_buffer_chain
{
   VkDeviceSize block_size;
   VkDeviceSize alignment;
   VkDeviceSize offset;
   VkBufferUsageFlags usage;
   struct vk_buffer_node *head;
   struct vk_buffer_node *current;
};

struct vk_buffer_range
{
   uint8_t *data;
   VkBuffer buffer;
   VkDeviceSize offset;
};

struct vk_buffer_chain vulkan_buffer_chain_init(
      VkDeviceSize block_size,
      VkDeviceSize alignment,
      VkBufferUsageFlags usage);

#define VK_BUFFER_CHAIN_DISCARD(chain) \
{ \
   chain->current = chain->head; \
   chain->offset = 0; \
}

bool vulkan_buffer_chain_alloc(const struct vulkan_context *context,
      struct vk_buffer_chain *chain, size_t size,
      struct vk_buffer_range *range);

void vulkan_buffer_chain_free(
      VkDevice device,
      struct vk_buffer_chain *chain);

struct vk_descriptor_pool
{
   VkDescriptorPool pool;
   VkDescriptorSet sets[VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS];
   struct vk_descriptor_pool *next;
};

struct vk_descriptor_manager
{
   struct vk_descriptor_pool *head;
   struct vk_descriptor_pool *current;
   unsigned count;

   VkDescriptorPoolSize sizes[VULKAN_MAX_DESCRIPTOR_POOL_SIZES];
   VkDescriptorSetLayout set_layout;
   unsigned num_sizes;
};

struct vk_per_frame
{
   struct vk_image backbuffer;
   struct vk_texture texture;
   struct vk_texture texture_optimal;
   struct vk_buffer_chain vbo;
   struct vk_buffer_chain ubo;
   struct vk_descriptor_manager descriptor_manager;

   VkCommandPool cmd_pool;
   VkCommandBuffer cmd;
};

struct vk_draw_quad
{
   VkPipeline pipeline;
   struct vk_texture *texture;
   VkSampler sampler;
   const math_matrix_4x4 *mvp;
   struct vk_color color;
};

struct vk_draw_triangles
{
   unsigned vertices;
   size_t uniform_size;
   const void *uniform;
   const struct vk_buffer_range *vbo;
   struct vk_texture *texture;
   VkPipeline pipeline;
   VkSampler sampler;
};

typedef struct vk
{
   bool vsync;
   bool keep_aspect;
   bool fullscreen;
   bool quitting;
   bool should_resize;

   unsigned video_width;
   unsigned video_height;

   unsigned tex_w, tex_h;
   unsigned vp_out_width, vp_out_height;
   unsigned rotation;
   unsigned num_swapchain_images;
   unsigned last_valid_index;

   vulkan_context_t *context;
   video_info_t video;
   void *ctx_data;
   const gfx_ctx_driver_t *ctx_driver;

   VkFormat tex_fmt;
   math_matrix_4x4 mvp, mvp_no_rot;
   VkViewport vk_vp;
   VkRenderPass render_pass;
   struct video_viewport vp;
   struct vk_per_frame *chain;
   struct vk_per_frame swapchain[VULKAN_MAX_SWAPCHAIN_IMAGES];
   struct vk_texture default_texture;

   /* Currently active command buffer. */
   VkCommandBuffer cmd;
   /* Staging pool for doing buffer transfers on GPU. */
   VkCommandPool staging_pool;

   struct
   {
      bool pending;
      bool streamed;
      struct scaler_ctx scaler_bgr;
      struct scaler_ctx scaler_rgb;
      struct vk_texture staging[VULKAN_MAX_SWAPCHAIN_IMAGES];
   } readback;

   struct
   {
      bool enable;
      bool full_screen;
      unsigned count;
      struct vk_texture *images;
      struct vk_vertex *vertex;
   } overlay;

   struct
   {
      VkPipeline alpha_blend;
      VkPipeline font;
      VkDescriptorSetLayout set_layout;
      VkPipelineLayout layout;
      VkPipelineCache cache;
   } pipelines;

   struct
   {
      bool blend;
      VkPipeline pipelines[7 * 2];
      struct vk_texture blank_texture;
   } display;

   struct
   {
      bool enable;
      bool full_screen;
      unsigned last_index;
      float alpha;
      struct vk_texture textures[VULKAN_MAX_SWAPCHAIN_IMAGES];
      struct vk_texture textures_optimal[VULKAN_MAX_SWAPCHAIN_IMAGES];
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
      bool enable;
      bool valid_semaphore;

      unsigned capacity_cmd;
      unsigned last_width;
      unsigned last_height;
      uint32_t num_semaphores;
      uint32_t num_cmd;
      uint32_t src_queue_family;

      struct retro_hw_render_interface_vulkan iface;
      const struct retro_vulkan_image *image;
      const VkSemaphore *semaphores;
      VkSemaphore signal_semaphore;
      VkPipelineStageFlags *wait_dst_stages;
      VkCommandBuffer *cmd;
   } hw;

   struct
   {
      uint64_t dirty;
      VkRect2D scissor;
      bool use_scissor;
      VkPipeline pipeline;
      VkImageView view;
      VkSampler sampler;
      math_matrix_4x4 mvp;
   } tracker;

   void *filter_chain;
} vk_t;

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

void vulkan_sync_texture_to_gpu(vk_t *vk, const struct vk_texture *tex);
void vulkan_sync_texture_to_cpu(vk_t *vk, const struct vk_texture *tex);

void vulkan_transition_texture(vk_t *vk, VkCommandBuffer cmd, struct vk_texture *texture);

void vulkan_transfer_image_ownership(VkCommandBuffer cmd,
      VkImage image, VkImageLayout layout,
      VkPipelineStageFlags src_stages,
      VkPipelineStageFlags dst_stages,
      uint32_t src_queue_family,
      uint32_t dst_queue_family);

void vulkan_destroy_texture(
      VkDevice device,
      struct vk_texture *tex);

void vulkan_copy_staging_to_dynamic(vk_t *vk, VkCommandBuffer cmd,
      struct vk_texture *dynamic,
      struct vk_texture *staging);

/* VBO will be written to here. */
void vulkan_draw_quad(vk_t *vk, const struct vk_draw_quad *quad);

/* The VBO needs to be written to before calling this.
 * Use vulkan_buffer_chain_alloc.
 */
void vulkan_draw_triangles(vk_t *vk, const struct vk_draw_triangles *call);

void vulkan_image_layout_transition(vk_t *vk,
      VkCommandBuffer cmd, VkImage image,
      VkImageLayout old_layout, VkImageLayout new_layout,
      VkAccessFlags srcAccess, VkAccessFlags dstAccess,
      VkPipelineStageFlags srcStages, VkPipelineStageFlags dstStages);

void vulkan_image_layout_transition_levels(
      VkCommandBuffer cmd, VkImage image, uint32_t levels,
      VkImageLayout old_layout, VkImageLayout new_layout,
      VkAccessFlags src_access, VkAccessFlags dst_access,
      VkPipelineStageFlags src_stages, VkPipelineStageFlags dst_stages);

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

      default:
         RARCH_ERR("[Vulkan]: Unknown format.\n");
         abort();
   }
}

static INLINE void vulkan_write_quad_vbo(struct vk_vertex *pv,
      float x, float y, float width, float height,
      float tex_x, float tex_y, float tex_width, float tex_height,
      const struct vk_color *color)
{
   unsigned i;
   static const float strip[2 * 6] = {
      0.0f, 0.0f,
      0.0f, 1.0f,
      1.0f, 0.0f,
      1.0f, 1.0f,
      1.0f, 0.0f,
      0.0f, 1.0f,
   };

   for (i = 0; i < 6; i++)
   {
      pv[i].x     = x + strip[2 * i + 0] * width;
      pv[i].y     = y + strip[2 * i + 1] * height;
      pv[i].tex_x = tex_x + strip[2 * i + 0] * tex_width;
      pv[i].tex_y = tex_y + strip[2 * i + 1] * tex_height;
      pv[i].color = *color;
   }
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

#define VK_DESCRIPTOR_MANAGER_RESTART(manager) \
{ \
   manager->current = manager->head; \
   manager->count = 0; \
}

#define VK_MAP_PERSISTENT_TEXTURE(device, texture) \
{ \
   vkMapMemory(device, texture->memory, texture->offset, texture->size, 0, &texture->mapped); \
}

struct vk_descriptor_manager vulkan_create_descriptor_manager(
      VkDevice device,
      const VkDescriptorPoolSize *sizes, unsigned num_sizes,
      VkDescriptorSetLayout set_layout);

void vulkan_destroy_descriptor_manager(
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

RETRO_END_DECLS

#endif
