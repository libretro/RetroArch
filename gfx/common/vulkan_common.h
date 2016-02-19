/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016 - Hans-Kristian Arntzen
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

#define VK_PROTOTYPES
#include <vulkan/vulkan.h>

#include <gfx/math/matrix_4x4.h>
#include <formats/image.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include "boolean.h"
#include "../../driver.h"
#include "../../performance.h"
#include "../../libretro.h"
#include "../../general.h"
#include "../../retroarch.h"
#include "../font_driver.h"
#include "../video_context_driver.h"
#include "libretro_vulkan.h"
#include "../drivers_shader/shader_vulkan.h"
#include <rthreads/rthreads.h>
#include <gfx/scaler/scaler.h>

#define VULKAN_MAX_SWAPCHAIN_IMAGES 8

#define VULKAN_DIRTY_DYNAMIC_BIT 0x0001

enum vulkan_wsi_type
{
   VULKAN_WSI_NONE = 0,
   VULKAN_WSI_WAYLAND,
   VULKAN_WSI_MIR,
   VULKAN_WSI_ANDROID,
   VULKAN_WSI_WIN32,
   VULKAN_WSI_XCB,
   VULKAN_WSI_XLIB
};

typedef struct vulkan_context
{
   VkInstance instance;
   VkPhysicalDevice gpu;
   VkDevice device;
   VkQueue queue;
   uint32_t graphics_queue_index;

   VkPhysicalDeviceProperties gpu_properties;
   VkPhysicalDeviceMemoryProperties memory_properties;

   bool invalid_swapchain;
   VkImage swapchain_images[VULKAN_MAX_SWAPCHAIN_IMAGES];
   VkFence swapchain_fences[VULKAN_MAX_SWAPCHAIN_IMAGES];
   VkSemaphore swapchain_semaphores[VULKAN_MAX_SWAPCHAIN_IMAGES];
   uint32_t num_swapchain_images;
   uint32_t current_swapchain_index;
   unsigned swapchain_width;
   unsigned swapchain_height;
   VkFormat swapchain_format;

   slock_t *queue_lock;

   /* Used by screenshot to get blits with correct colorspace. */
   bool swapchain_is_srgb;
} vulkan_context_t;

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
   VkImage image;
   VkImageView view;
   VkDeviceMemory memory;

   unsigned width, height;
   VkFormat format;

   void *mapped;
   size_t offset;
   size_t stride;
   size_t size;
   VkDeviceSize memory_size;
   uint32_t memory_type;

   VkImageLayout layout;
   bool default_smooth;
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
void vulkan_buffer_chain_discard(struct vk_buffer_chain *chain);
bool vulkan_buffer_chain_alloc(const struct vulkan_context *context,
      struct vk_buffer_chain *chain, size_t size, struct vk_buffer_range *range);
void vulkan_buffer_chain_free(VkDevice device, struct vk_buffer_chain *chain);

#define VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS 16
#define VULKAN_BUFFER_BLOCK_SIZE (4 * 1024)
#define VULKAN_MAX_DESCRIPTOR_POOL_SIZES 16

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
   VkPipeline pipeline;
   struct vk_texture *texture;
   VkSampler sampler;
   const math_matrix_4x4 *mvp;

   const struct vk_buffer_range *vbo;
   unsigned vertices;
};

typedef struct vk
{
   vulkan_context_t *context;
   video_info_t video;
   unsigned full_x, full_y;

   unsigned tex_w, tex_h;
   VkFormat tex_fmt;
   bool vsync;
   bool keep_aspect;
   bool fullscreen;
   bool quitting;
   bool should_resize;
   unsigned vp_out_width, vp_out_height;

   unsigned rotation;
   math_matrix_4x4 mvp, mvp_no_rot;
   struct video_viewport vp;
   VkViewport vk_vp;

   VkRenderPass render_pass;
   struct vk_per_frame swapchain[VULKAN_MAX_SWAPCHAIN_IMAGES];
   unsigned num_swapchain_images;

   VkCommandBuffer cmd; /* Currently active command buffer. */
   VkCommandPool staging_pool; /* Staging pool for doing buffer transfers on GPU. */

   struct
   {
      struct vk_texture staging[VULKAN_MAX_SWAPCHAIN_IMAGES];
      struct scaler_ctx scaler;
      bool pending;
      bool streamed;
   } readback;

   struct
   {
      struct vk_texture *images;
      struct vk_vertex *vertex;
      unsigned count;
      bool enable;
      bool full_screen;
   } overlay;

   struct
   {
      VkPipeline alpha_blend;
      VkDescriptorSetLayout set_layout;
      VkPipelineLayout layout;
      VkPipelineCache cache;
   } pipelines;

   struct
   {
      bool blend;
      VkPipeline pipelines[4];
      struct vk_texture blank_texture;
   } display;

   struct
   {
      struct vk_texture textures[VULKAN_MAX_SWAPCHAIN_IMAGES];
      float alpha;
      unsigned last_index;
      bool enable;
      bool full_screen;
   } menu;

   struct
   {
      VkSampler linear;
      VkSampler nearest;
   } samplers;

   unsigned last_valid_index;

   struct vk_per_frame *chain;

   struct
   {
      struct retro_hw_render_interface_vulkan iface;
      const struct retro_vulkan_image *image;
      const VkSemaphore *semaphores;
      uint32_t num_semaphores;

      VkPipelineStageFlags *wait_dst_stages;

      VkCommandBuffer *cmd;
      uint32_t num_cmd;
      unsigned capacity_cmd;

      unsigned last_width;
      unsigned last_height;

      bool enable;
   } hw;

   struct
   {
      VkPipeline pipeline;
      VkImageView view;
      VkSampler sampler;
      math_matrix_4x4 mvp;
      uint64_t dirty;
   } tracker;

   vulkan_filter_chain_t *filter_chain;
} vk_t;

uint32_t vulkan_find_memory_type(const VkPhysicalDeviceMemoryProperties *mem_props,
      uint32_t device_reqs, uint32_t host_reqs);

uint32_t vulkan_find_memory_type_fallback(const VkPhysicalDeviceMemoryProperties *mem_props,
      uint32_t device_reqs, uint32_t host_reqs);

enum vk_texture_type
{
   VULKAN_TEXTURE_STREAMED = 0,
   VULKAN_TEXTURE_STATIC,
   VULKAN_TEXTURE_READBACK
};

struct vk_texture vulkan_create_texture(vk_t *vk,
      struct vk_texture *old,
      unsigned width, unsigned height,
      VkFormat format,
      const void *initial, const VkComponentMapping *swizzle,
      enum vk_texture_type type);

void vulkan_transition_texture(vk_t *vk, struct vk_texture *texture);

void vulkan_map_persistent_texture(VkDevice device, struct vk_texture *tex);
void vulkan_destroy_texture(VkDevice device, struct vk_texture *tex);

/* VBO will be written to here. */
void vulkan_draw_quad(vk_t *vk, const struct vk_draw_quad *quad);

/* The VBO needs to be written to before calling this.
 * Use vulkan_buffer_chain_alloc.
 */
void vulkan_draw_triangles(vk_t *vk, const struct vk_draw_triangles *call);

void vulkan_image_layout_transition(vk_t *vk, VkCommandBuffer cmd, VkImage image,
      VkImageLayout old_layout, VkImageLayout new_layout,
      VkAccessFlags srcAccess, VkAccessFlags dstAccess,
      VkPipelineStageFlags srcStages, VkPipelineStageFlags dstStages);

static inline unsigned vulkan_format_to_bpp(VkFormat format)
{
   switch (format)
   {
      case VK_FORMAT_B8G8R8A8_UNORM:
         return 4;

      case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
      case VK_FORMAT_R5G6B5_UNORM_PACK16:
         return 2;

      case VK_FORMAT_R8_UNORM:
         return 1;

      default:
         RARCH_ERR("[Vulkan]: Unknown format.\n");
         abort();
   }
}

static inline void vulkan_write_quad_vbo(struct vk_vertex *pv,
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
      pv[i].x = x + strip[2 * i + 0] * width;
      pv[i].y = y + strip[2 * i + 1] * height;
      pv[i].tex_x = tex_x + strip[2 * i + 0] * tex_width;
      pv[i].tex_y = tex_y + strip[2 * i + 1] * tex_height;
      pv[i].color = *color;
   }
}

struct vk_buffer vulkan_create_buffer(const struct vulkan_context *context,
      size_t size, VkBufferUsageFlags usage);
void vulkan_destroy_buffer(VkDevice device, struct vk_buffer *buffer);

VkDescriptorSet vulkan_descriptor_manager_alloc(VkDevice device, struct vk_descriptor_manager *manager);
void vulkan_descriptor_manager_restart(struct vk_descriptor_manager *manager);
struct vk_descriptor_manager vulkan_create_descriptor_manager(VkDevice device,
      const VkDescriptorPoolSize *sizes, unsigned num_sizes,
      VkDescriptorSetLayout set_layout);
void vulkan_destroy_descriptor_manager(VkDevice device,
      struct vk_descriptor_manager *manager);
#endif

