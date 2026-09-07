/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2016 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include "../include/vulkan/vk_sdk_platform.h"
#include "shader_vulkan.h"
#include "glslang_util.h"
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <formats/image.h>
#include <string/stdstring.h>
#include <gfx/math/matrix_4x4.h>
#include <retro_miscellaneous.h>

#include "slang_process.h"

#include "../common/vulkan_common.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../msg_hash.h"
#include "../../input/input_driver.h"

typedef struct vulkan_filter_chain_texture vulkan_filter_chain_texture;
typedef struct vulkan_filter_chain_pass_info vulkan_filter_chain_pass_info;
typedef struct vulkan_filter_chain_swapchain_info vulkan_filter_chain_swapchain_info;
typedef struct FramebufferMemoryPool FramebufferMemoryPool;
typedef struct CommonResources CommonResources;
typedef struct vulkan_filter_chain vulkan_filter_chain;
typedef struct vulkan_filter_chain_create_info vulkan_filter_chain_create_info;

/* Maximum number of texture descriptor writes that can be batched in a
 * single pass.  This covers: Original, Source, OriginalHistory[0],
 * up to ~30 history/pass-output/feedback/LUT textures.  If a shader
 * preset somehow exceeds this, the batch is flushed early. */
#define VULKAN_MAX_DESCRIPTOR_WRITES 64

/* Append a texture descriptor write to the batch arrays.
 * The caller must call vulkan_flush_descriptor_writes() once all
 * textures have been queued. */
#define VULKAN_PASS_SET_TEXTURE_BATCHED(set, _sampler, binding, image_view, image_layout, \
      image_infos, writes, write_count) \
{ \
   unsigned _idx = (write_count); \
   VkDescriptorImageInfo *_img = &(image_infos)[_idx]; \
   VkWriteDescriptorSet  *_wr  = &(writes)[_idx]; \
   _img->sampler         = _sampler; \
   _img->imageView       = image_view; \
   _img->imageLayout     = image_layout; \
   _wr->sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; \
   _wr->pNext            = NULL; \
   _wr->dstSet           = set; \
   _wr->dstBinding       = binding; \
   _wr->dstArrayElement  = 0; \
   _wr->descriptorCount  = 1; \
   _wr->descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; \
   _wr->pImageInfo       = _img; \
   _wr->pBufferInfo      = NULL; \
   _wr->pTexelBufferView = NULL; \
   (write_count)++; \
}


   static bool vulkan_initialize_render_pass(VkDevice device, VkFormat format,
         VkRenderPass *render_pass)
   {
      VkAttachmentReference color_ref;
      VkRenderPassCreateInfo rp_info;
      VkAttachmentDescription attachment;
      VkSubpassDescription subpass;

      rp_info.sType                = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
      rp_info.pNext                = NULL;
      rp_info.flags                = 0;
      rp_info.attachmentCount      = 1;
      rp_info.pAttachments         = &attachment;
      rp_info.subpassCount         = 1;
      rp_info.pSubpasses           = &subpass;
      rp_info.dependencyCount      = 0;
      rp_info.pDependencies        = NULL;

      color_ref.attachment         = 0;
      color_ref.layout             = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      /* We will always write to the entire framebuffer,
       * so we don't really need to clear. */
      attachment.flags             = 0;
      attachment.format            = format;
      attachment.samples           = VK_SAMPLE_COUNT_1_BIT;
      attachment.loadOp            = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment.storeOp           = VK_ATTACHMENT_STORE_OP_STORE;
      attachment.stencilLoadOp     = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
      attachment.stencilStoreOp    = VK_ATTACHMENT_STORE_OP_DONT_CARE;
      attachment.initialLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      attachment.finalLayout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

      subpass.flags                     = 0;
      subpass.pipelineBindPoint         = VK_PIPELINE_BIND_POINT_GRAPHICS;
      subpass.inputAttachmentCount      = 0;
      subpass.pInputAttachments         = NULL;
      subpass.colorAttachmentCount      = 1;
      subpass.pColorAttachments         = &color_ref;
      subpass.pResolveAttachments       = NULL;
      subpass.pDepthStencilAttachment   = NULL;
      subpass.preserveAttachmentCount   = 0;
      subpass.pPreserveAttachments      = NULL;

      return vkCreateRenderPass(device, &rp_info, NULL, render_pass)
            == VK_SUCCESS;
   }

   static void vulkan_framebuffer_clear(VkImage image, VkCommandBuffer cmd)
   {
      VkClearColorValue color;
      VkImageSubresourceRange range;

      VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,
            image,
            VK_REMAINING_MIP_LEVELS,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED);

      color.float32[0]     = 0.0f;
      color.float32[1]     = 0.0f;
      color.float32[2]     = 0.0f;
      color.float32[3]     = 0.0f;
      range.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      range.baseMipLevel   = 0;
      range.levelCount     = 1;
      range.baseArrayLayer = 0;
      range.layerCount     = 1;

      vkCmdClearColorImage(cmd,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            &color,
            1,
            &range);

      VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,
            image,
            VK_REMAINING_MIP_LEVELS,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED);
   }

   static void vulkan_framebuffer_generate_mips(
         VkFramebuffer framebuffer,
         VkImage image,
         unsigned size_width, unsigned size_height,
         VkCommandBuffer cmd,
         unsigned levels
         )
   {
      unsigned i;
      /* This is run every frame, so make sure
       * we aren't opting into the "lazy" way of doing this. :) */
      VkImageMemoryBarrier barriers[2];

      /* First, transfer the input mip level to TRANSFER_SRC_OPTIMAL.
       * This should allow the surface to stay compressed.
       * All subsequent mip-layers are now transferred into DST_OPTIMAL from
       * UNDEFINED at this point.
       */

      /* Input */
      barriers[0].sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barriers[0].pNext                           = NULL;
      barriers[0].srcAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      barriers[0].dstAccessMask                   = VK_ACCESS_TRANSFER_READ_BIT;
      barriers[0].oldLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
      barriers[0].newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barriers[0].srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      barriers[0].dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      barriers[0].image                           = image;
      barriers[0].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      barriers[0].subresourceRange.baseMipLevel   = 0;
      barriers[0].subresourceRange.levelCount     = 1;
      barriers[0].subresourceRange.baseArrayLayer = 0;
      barriers[0].subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

      /* The rest of the mip chain */
      barriers[1].sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
      barriers[1].pNext                           = NULL;
      barriers[1].srcAccessMask                   = 0;
      barriers[1].dstAccessMask                   = VK_ACCESS_TRANSFER_WRITE_BIT;
      barriers[1].oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
      barriers[1].newLayout                       = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barriers[1].srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      barriers[1].dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
      barriers[1].image                           = image;
      barriers[1].subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      barriers[1].subresourceRange.baseMipLevel   = 1;
      barriers[1].subresourceRange.levelCount     = VK_REMAINING_MIP_LEVELS;
      barriers[1].subresourceRange.baseArrayLayer = 0;
      barriers[1].subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

      /* The source mip was produced by a color-attachment write, so
       * COLOR_ATTACHMENT_OUTPUT is the precise stage to wait on.
       * ALL_GRAPHICS would force an unnecessary flush of earlier
       * pipeline stages on tile-based GPUs. */
      vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            NULL,
            0,
            NULL,
            2,
            barriers);

      for (i = 1; i < levels; i++)
      {
         unsigned src_width, src_height, target_width, target_height;
         VkImageBlit blit_region = {{0}};

         /* For subsequent passes, we have to transition
          * from DST_OPTIMAL to SRC_OPTIMAL,
          * but only do so one mip-level at a time. */
         if (i > 1)
         {
            barriers[0].srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
            barriers[0].dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;
            barriers[0].subresourceRange.baseMipLevel = i - 1;
            barriers[0].subresourceRange.levelCount   = 1;
            barriers[0].oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barriers[0].newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            vkCmdPipelineBarrier(cmd,
                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                  0,
                  0,
                  NULL,
                  0,
                  NULL,
                  1,
                  barriers);
         }

         src_width                                 = MAX(size_width >> (i - 1), 1u);
         src_height                                = MAX(size_height >> (i - 1), 1u);
         target_width                              = MAX(size_width >> i, 1u);
         target_height                             = MAX(size_height >> i, 1u);

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

         vkCmdBlitImage(cmd,
               image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
               image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               1, &blit_region, VK_FILTER_LINEAR);
      }

      /* We are now done, and we have all mip-levels except
       * the last in TRANSFER_SRC_OPTIMAL,
       * and the last one still on TRANSFER_DST_OPTIMAL,
       * so do a final barrier which
       * moves everything to SHADER_READ_ONLY_OPTIMAL in
       * one go along with the execution barrier to next pass.
       * Read-to-read memory barrier, so only need execution
       * barrier for first transition.
       */
      barriers[0].srcAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;
      barriers[0].dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;
      barriers[0].subresourceRange.baseMipLevel = 0;
      barriers[0].subresourceRange.levelCount   = levels - 1;
      barriers[0].oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barriers[0].newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      /* This is read-after-write barrier. */
      barriers[1].srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
      barriers[1].dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;
      barriers[1].subresourceRange.baseMipLevel = levels - 1;
      barriers[1].subresourceRange.levelCount   = 1;
      barriers[1].oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barriers[1].newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            NULL,
            0,
            NULL,
            2, barriers);

      /* Next pass will wait for ALL_GRAPHICS_BIT, and since
       * we have dstStage as FRAGMENT_SHADER,
       * the dependency chain will ensure we don't start
       * next pass until the mipchain is complete. */
   }

   static void vulkan_framebuffer_copy(VkImage image,
         unsigned size_width, unsigned size_height,
         VkCommandBuffer cmd,
         VkImage src_image, VkImageLayout src_layout)
   {
      VkImageCopy region;

      /* The destination transitions from UNDEFINED (contents discarded),
       * so there is no data dependency on prior fragment work.
       * TOP_OF_PIPE_BIT with srcAccessMask 0 is the correct minimal
       * barrier — same pattern used by vulkan_framebuffer_clear(). */
      VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(
            cmd,
            image,
            VK_REMAINING_MIP_LEVELS,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED);

      region.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      region.srcSubresource.mipLevel       = 0;
      region.srcSubresource.baseArrayLayer = 0;
      region.srcSubresource.layerCount     = 1;
      region.srcOffset.x                   = 0;
      region.srcOffset.y                   = 0;
      region.srcOffset.z                   = 0;
      region.dstSubresource                = region.srcSubresource;
      region.dstOffset.x                   = 0;
      region.dstOffset.y                   = 0;
      region.dstOffset.z                   = 0;
      region.extent.width                  = size_width;
      region.extent.height                 = size_height;
      region.extent.depth                  = 1;

      vkCmdCopyImage(cmd,
            src_image, src_layout,
            image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1, &region);

      VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,
            image,
            VK_REMAINING_MIP_LEVELS,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED);
   }

static void vulkan_flush_descriptor_writes(VkDevice device,
      VkWriteDescriptorSet *writes, unsigned *write_count)
{
   if (*write_count > 0)
   {
      vkUpdateDescriptorSets(device, *write_count, writes, 0, NULL);
      *write_count = 0;
   }
}

static const uint32_t opaque_vert[] =
#include "../drivers/vulkan_shaders/opaque.vert.inc"
;

static const uint32_t opaque_frag[] =
#include "../drivers/vulkan_shaders/opaque.frag.inc"
;

#ifdef VULKAN_HDR_SWAPCHAIN
static const uint32_t hdr_frag[] =
#include "../drivers/vulkan_shaders/hdr.frag.inc"
;
#endif /* VULKAN_HDR_SWAPCHAIN */

typedef struct
{
   vulkan_filter_chain_texture texture;
   glslang_filter_chain_filter filter;
   glslang_filter_chain_filter mip_filter;
   glslang_filter_chain_address address;
} Texture;

struct deferred_disposes;

/* A host-visible, host-coherent VkBuffer with its backing memory.
 * Zero-initialized = empty; slang_buffer_free() is safe on any state. */
struct slang_buffer
{
   VkDevice device;
   VkBuffer buffer;
   VkDeviceMemory memory;
   size_t size;
   void *mapped;
};

static bool slang_buffer_init(struct slang_buffer *buf,
      VkDevice device, const VkPhysicalDeviceMemoryProperties *mem_props,
      size_t len, VkBufferUsageFlags usage);
static void *slang_buffer_map(struct slang_buffer *buf);
static void slang_buffer_unmap(struct slang_buffer *buf);
static void slang_buffer_free(struct slang_buffer *buf);

/* An immutable LUT texture with its staging buffer (freed after
 * upload) and its semantic id.  Zero-initialized = empty;
 * slang_static_texture_free() is safe on any state. */
struct slang_static_texture
{
   VkDevice device;
   VkImage image;
   VkImageView view;
   VkDeviceMemory memory;
   struct slang_buffer buffer;
   char *id;
   Texture texture;
};

/* Takes ownership of *staging, which is zeroed. */
static bool slang_static_texture_init(struct slang_static_texture *tex,
      const char *id,
      VkDevice device,
      VkImage image,
      VkImageView view,
      VkDeviceMemory memory,
      struct slang_buffer *staging,
      unsigned width, unsigned height,
      bool linear,
      bool mipmap,
      glslang_filter_chain_address address)
{
   tex->device            = device;
   tex->image             = image;
   tex->view              = view;
   tex->memory            = memory;
   tex->buffer            = *staging;
   memset(staging, 0, sizeof(*staging));
   if (!(tex->id = strdup(id)))
      return false;

   tex->texture.filter         = GLSLANG_FILTER_CHAIN_NEAREST;
   tex->texture.mip_filter     = GLSLANG_FILTER_CHAIN_NEAREST;
   tex->texture.address        = address;
   tex->texture.texture.image  = image;
   tex->texture.texture.view   = view;
   tex->texture.texture.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   tex->texture.texture.width  = width;
   tex->texture.texture.height = height;

   if (linear)
      tex->texture.filter      = GLSLANG_FILTER_CHAIN_LINEAR;
   if (mipmap && linear)
      tex->texture.mip_filter  = GLSLANG_FILTER_CHAIN_LINEAR;
   return true;
}

static void slang_static_texture_free(struct slang_static_texture *tex)
{
   slang_buffer_free(&tex->buffer);
   if (tex->view != VK_NULL_HANDLE)
      vkDestroyImageView(tex->device, tex->view, NULL);
   if (tex->image != VK_NULL_HANDLE)
      vkDestroyImage(tex->device, tex->image, NULL);
   if (tex->memory != VK_NULL_HANDLE)
      vkFreeMemory(tex->device, tex->memory, NULL);
   free(tex->id);
   tex->view   = VK_NULL_HANDLE;
   tex->image  = VK_NULL_HANDLE;
   tex->memory = VK_NULL_HANDLE;
   tex->id     = NULL;
}

/* Recycle pool for framebuffer device-memory allocations.
 *
 * A resize must allocate a fresh backing allocation: reusing one that is
 * still bound to an in-flight image aliases two live optimal-tiled images,
 * which is invalid on some drivers (e.g. Mali).  To avoid a
 * vkAllocateMemory/vkFreeMemory pair on every resize, freed blocks are
 * parked here and handed back out later.
 *
 * A block only enters the pool from a deferred disposer callback, i.e.
 * after the image that aliased it has been destroyed and the referencing
 * GPU work has retired, so a recycled block never aliases a live image.
 *
 * Owned by CommonResources; its address is therefore stable and it is
 * alive at every deferred_calls drain point (including the destructor-body
 * flush()), which is why a deferred callback may capture a pointer to it
 * even though it must never capture the framebuffer object itself. */
struct fbpool_block
{
   VkDeviceMemory memory;
   size_t         size;
   uint32_t       type;
};

struct FramebufferMemoryPool
{

   /* Keep only a handful of blocks; oscillating between two sizes (the
    * common "toggle integer scale" case) needs at most two. */
   struct fbpool_block blocks[4];
   size_t num_blocks;
#define FBPOOL_MAX_BLOCKS 4

   /* Best-fit: smallest block that satisfies type+size, or a null block on
    * miss.  The returned block is removed from the pool and owned by the
    * caller. */
};

static struct fbpool_block fbpool_acquire(
      FramebufferMemoryPool *pool, size_t size, uint32_t type)
   {
      size_t i;
      size_t best = (size_t)-1;
      struct fbpool_block result;
      result.memory = VK_NULL_HANDLE;
      result.size   = 0;
      result.type   = type;
      for (i = 0; i < pool->num_blocks; i++)
      {
         if (     pool->blocks[i].type == type
               && pool->blocks[i].size >= size
               && (best == (size_t)-1 || pool->blocks[i].size < pool->blocks[best].size))
            best = i;
      }
      if (best == (size_t)-1)
         return result;
      result       = pool->blocks[best];
      pool->blocks[best] = pool->blocks[pool->num_blocks - 1];
      pool->num_blocks--;
      return result;
   }

static void fbpool_release(FramebufferMemoryPool *pool,
      VkDevice device, VkDeviceMemory memory,
      size_t size, uint32_t type)
   {
      struct fbpool_block b;
      if (memory == VK_NULL_HANDLE)
         return;
      if (pool->num_blocks >= FBPOOL_MAX_BLOCKS)
      {
         /* Pool full: evict the smallest block so the larger, more reusable
          * allocations survive.  If the incoming block is itself the
          * smallest, just free it. */
         size_t i, smallest = 0;
         for (i = 1; i < pool->num_blocks; i++)
            if (pool->blocks[i].size < pool->blocks[smallest].size)
               smallest = i;
         if (pool->blocks[smallest].size >= size)
         {
            vkFreeMemory(device, memory, NULL);
            return;
         }
         vkFreeMemory(device, pool->blocks[smallest].memory, NULL);
         pool->blocks[smallest] = pool->blocks[pool->num_blocks - 1];
         pool->num_blocks--;
      }
      b.memory = memory;
      b.size   = size;
      b.type   = type;
      pool->blocks[pool->num_blocks++] = b;
   }

static void fbpool_drain(FramebufferMemoryPool *pool, VkDevice device)
   {
      size_t i;
      for (i = 0; i < pool->num_blocks; i++)
         vkFreeMemory(device, pool->blocks[i].memory, NULL);
      pool->num_blocks = 0;
   }


/* One queued disposal: the resources a replaced framebuffer instance
 * held.  Replacing a framebuffer is the only operation that ever defers
 * work, so the queue holds plain records of this shape instead of
 * type-erased callables. */
struct deferred_fb_dispose
{
   VkDevice device;
   FramebufferMemoryPool *pool;
   VkFramebuffer framebuffer;
   VkImageView view;
   VkImageView fb_view;
   VkImage image;
   VkDeviceMemory memory;
   size_t memory_size;
   uint32_t memory_type;
   /* VK_NULL_HANDLE unless the format changed with the resize. */
   VkRenderPass render_pass;
};

/* Per-sync-index queue of pending disposals. */
struct deferred_disposes
{
   struct deferred_fb_dispose *calls;
   size_t size;
   size_t cap;
};

static void deferred_fb_dispose_run(const struct deferred_fb_dispose *c)
{
   if (c->framebuffer != VK_NULL_HANDLE)
      vkDestroyFramebuffer(c->device, c->framebuffer, NULL);
   if (c->view != VK_NULL_HANDLE)
      vkDestroyImageView(c->device, c->view, NULL);
   if (c->fb_view != VK_NULL_HANDLE)
      vkDestroyImageView(c->device, c->fb_view, NULL);
   /* The image must be destroyed before its memory becomes reusable;
    * the pool only ever hands out blocks whose image is already gone. */
   if (c->image != VK_NULL_HANDLE)
      vkDestroyImage(c->device, c->image, NULL);
   if (c->memory != VK_NULL_HANDLE)
   {
      if (c->pool)
         fbpool_release(c->pool, c->device, c->memory, c->memory_size,
               c->memory_type);
      else
         vkFreeMemory(c->device, c->memory, NULL);
   }
   if (c->render_pass != VK_NULL_HANDLE)
      vkDestroyRenderPass(c->device, c->render_pass, NULL);
}

static void deferred_disposes_run_clear(struct deferred_disposes *d)
{
   size_t i;
   for (i = 0; i < d->size; i++)
      deferred_fb_dispose_run(&d->calls[i]);
   d->size = 0;
}

static bool deferred_disposes_push(struct deferred_disposes *d,
      const struct deferred_fb_dispose *call)
{
   if (d->size == d->cap)
   {
      size_t new_cap                        = d->cap ? d->cap * 2 : 4;
      struct deferred_fb_dispose *new_calls = (struct deferred_fb_dispose*)
         realloc(d->calls, new_cap * sizeof(*new_calls));
      if (!new_calls)
         return false;
      d->calls = new_calls;
      d->cap   = new_cap;
   }
   d->calls[d->size++] = *call;
   return true;
}

/* An offscreen render target: image, sampled/attachment views, the
 * framebuffer and its render pass, and pooled backing memory.
 * Zero-initialized = empty; slang_framebuffer_free() is safe on any
 * state, including the partial states a failed (re)build leaves. */
struct slang_framebuffer
{
   unsigned size_width;
      unsigned size_height;
   VkFormat format;
   unsigned max_levels;
   const VkPhysicalDeviceMemoryProperties *memory_properties;
   VkDevice device;
   VkImage image;
   VkImageView view;
   VkImageView fb_view;
   unsigned levels;
   VkFramebuffer framebuffer;
   VkRenderPass render_pass;
   struct
   {
      size_t size;
      uint32_t type;
      VkDeviceMemory memory;
   } memory;
   /* Chain-scoped, may be null. Never stored into a deferred record
    * as a member; copy to a local first (see set_size). */
   FramebufferMemoryPool *mem_pool;
};

static bool slang_framebuffer_build(struct slang_framebuffer *fb);
static bool slang_framebuffer_set_size(struct slang_framebuffer *fb,
      struct deferred_disposes *disposer,
      unsigned width, unsigned height,
      VkFormat format);
static void slang_framebuffer_free(struct slang_framebuffer *fb);
/* Allocates and fully initializes; returns NULL on any failure. */
static struct slang_framebuffer *slang_framebuffer_new(VkDevice device,
      const VkPhysicalDeviceMemoryProperties *mem_props,
      unsigned max_width, unsigned max_height,
      VkFormat format, unsigned max_levels,
      FramebufferMemoryPool *mem_pool);
/* Frees *fb if set and nulls it. */
static void slang_framebuffer_delete(struct slang_framebuffer **fb);

/* Fresh zeroed array of new_count Textures (the vector resize sites all
 * follow a clear(), and every element is rewritten before it is read). */
static bool texture_array_resize(Texture **arr, size_t *count,
      size_t new_count)
{
   free(*arr);
   *arr   = NULL;
   *count = 0;
   if (new_count &&
         !(*arr = (Texture*)calloc(new_count, sizeof(**arr))))
      return false;
   *count = new_count;
   return true;
}

struct CommonResources
{

   struct slang_buffer vbo;
   struct slang_buffer ubo;
   uint8_t *ubo_mapped;
   size_t ubo_sync_index_stride;
   size_t ubo_offset;
   size_t ubo_alignment;          /* init: 1 */

   VkSampler samplers[GLSLANG_FILTER_CHAIN_COUNT][GLSLANG_FILTER_CHAIN_COUNT][GLSLANG_FILTER_CHAIN_ADDRESS_COUNT];

   /* Descriptor write batch shared by every pass of the chain. Passes
    * build their semantics one after another on the thread recording
    * the frame, so one pair of scratch arrays serves all of them and
    * keeps 5.6 KiB off that thread's stack. */
   VkWriteDescriptorSet  batch_writes[VULKAN_MAX_DESCRIPTOR_WRITES];
   VkDescriptorImageInfo batch_image_infos[VULKAN_MAX_DESCRIPTOR_WRITES];

   Texture *original_history;
   size_t num_original_history;
   Texture *fb_feedback;
   size_t num_fb_feedback;
   Texture *pass_outputs;
   size_t num_pass_outputs;
   struct slang_static_texture *luts;
   size_t num_luts;

   slang_texture_semantic_name_map texture_semantic_map;
   slang_texture_semantic_name_map texture_semantic_uniform_map;
   struct video_shader *shader_preset;

   VkDevice device;

   /* Recycled framebuffer memory blocks, shared across all framebuffers of
    * this chain. Drained in the destructor. */
   FramebufferMemoryPool framebuffer_pool;

   /* Shared per-frame state: written once per frame by the filter chain,
    * read by every pass in build_semantics().  Eliminates N per-pass
    * copies of identical data and the O(passes) broadcast loops. */
   uint64_t frame_count;
   int32_t frame_direction;       /* init: 1 */
   uint32_t frame_time_delta;
   float original_fps;
   uint32_t rotation;
   float core_aspect;
   float core_aspect_rot;
   uint32_t total_subframes;      /* init: 1 */
   uint32_t current_subframe;     /* init: 1 */
#ifdef VULKAN_ROLLING_SCANLINE_SIMULATION
   bool simulate_scanline;
#endif /* VULKAN_ROLLING_SCANLINE_SIMULATION */
#ifdef VULKAN_HDR_SWAPCHAIN
   unsigned hdr_mode;
   float paper_white_nits;
   unsigned expand_gamut;
   float scanlines;
   unsigned subpixel_layout;
   float inverse_tonemap;
   float hdr10;
#endif /* VULKAN_HDR_SWAPCHAIN */
};

static void common_resources_init(CommonResources *common,
      VkDevice device,
      const VkPhysicalDeviceMemoryProperties *memory_properties);
static void common_resources_free(CommonResources *common);

struct slang_pass_parameter
{
   char *id;       /* owned (strdup'd) */
   unsigned index;
   unsigned semantic_index;
};

struct slang_pass
{
   VkDevice device;
   const VkPhysicalDeviceMemoryProperties *memory_properties;
   VkPipelineCache cache;
   unsigned num_sync_indices;
   unsigned sync_index;
   bool final_pass;

   VkPipeline pipeline;
   VkPipelineLayout pipeline_layout;
   VkDescriptorSetLayout set_layout;
   VkDescriptorPool pool;
   VkDescriptorSet *sets;

   CommonResources *common;

   unsigned current_framebuffer_size_width;
      unsigned current_framebuffer_size_height;
   VkViewport curr_vp;
   vulkan_filter_chain_pass_info pass_info;

   uint32_t *vertex_shader;
   size_t num_vertex_shader;
   uint32_t *fragment_shader;
   size_t num_fragment_shader;

   struct slang_framebuffer *framebuffer;
   struct slang_framebuffer *fb_feedback;
   VkRenderPass swapchain_render_pass;

   /* Plain C struct: explicitly zero-initialized by slang_pass_new()'s
    * calloc, since the first build() and a teardown before any build()
    * both run slang_reflection_free() on it. */
   slang_reflection reflection;

   uint64_t frame_count;   /* shadow: may differ from common due to frame_count_period */
   unsigned frame_count_period;
   unsigned pass_number;

   size_t ubo_offset;
   char *pass_name;

   struct slang_pass_parameter *parameters;
   size_t num_parameters;
   /* Indices into parameters[]; rebuilt by every build(), so they
    * survive parameters growing (and thus moving) in between. */
   size_t *filtered_parameters;
   size_t num_filtered;

   struct
   {
      VkShaderStageFlags stages;
      uint32_t *buffer;          /* uint32_t for alignment. */
      size_t buffer_size;        /* in uint32_t units */
   } push;
};

static struct slang_pass *slang_pass_new(VkDevice device,
      const VkPhysicalDeviceMemoryProperties *memory_properties,
      VkPipelineCache cache, unsigned num_sync_indices, bool final_pass)
{
   struct slang_pass *pass = (struct slang_pass*)calloc(1, sizeof(*pass));
   if (!pass)
      return NULL;
   pass->device            = device;
   pass->memory_properties = memory_properties;
   pass->cache             = cache;
   pass->num_sync_indices  = num_sync_indices;
   pass->final_pass        = final_pass;
   return pass;
}

static void slang_pass_free(struct slang_pass *pass);
static void slang_pass_clear_vk(struct slang_pass *pass);

static void slang_pass_set_pass_info(struct slang_pass *pass,
      unsigned max_original_width, unsigned max_original_height,
      unsigned max_source_width, unsigned max_source_height,
      const vulkan_filter_chain_swapchain_info swapchain,
      const vulkan_filter_chain_pass_info info,
      unsigned *out_width, unsigned *out_height);
static void slang_pass_set_shader(struct slang_pass *pass,
      VkShaderStageFlags stage, const uint32_t *spirv, size_t spirv_words);
static bool slang_pass_build(struct slang_pass *pass);
static bool slang_pass_init_feedback(struct slang_pass *pass);
static void slang_pass_build_commands(struct slang_pass *pass,
      struct deferred_disposes *disposer,
      VkCommandBuffer cmd,
      const Texture *original,
      const Texture *source,
      const VkViewport *vp,
      const float *mvp);
static bool slang_pass_add_parameter(struct slang_pass *pass,
      unsigned parameter_index, const char *id);
static void slang_pass_end_frame(struct slang_pass *pass);
static void slang_pass_allocate_buffers(struct slang_pass *pass);

static INLINE void slang_pass_notify_sync_index(struct slang_pass *pass,
      unsigned index) { pass->sync_index = index; }
static INLINE void slang_pass_set_frame_count(struct slang_pass *pass,
      uint64_t count) { pass->frame_count = count; }
static INLINE void slang_pass_set_frame_count_period(
      struct slang_pass *pass, unsigned p) { pass->frame_count_period = p; }
static INLINE void slang_pass_set_name(struct slang_pass *pass,
      const char *name)
{
   free(pass->pass_name);
   pass->pass_name = name ? strdup(name) : NULL;
}
static INLINE const char *slang_pass_get_name(
      const struct slang_pass *pass)
{
   return pass->pass_name ? pass->pass_name : "";
}

/* struct here since we're implementing the opaque typedef from C. */
struct vulkan_filter_chain
{
   VkDevice device;
   VkPhysicalDevice gpu;
   VkPhysicalDeviceMemoryProperties memory_properties;
   VkPipelineCache cache;
   struct slang_pass **passes;
   size_t pass_count;
   vulkan_filter_chain_pass_info *pass_info;
   size_t pass_info_count;
   struct deferred_disposes *deferred_calls;
   unsigned num_deferred;
   CommonResources common;
   VkFormat original_format;

   vulkan_filter_chain_texture input_texture;

   unsigned max_input_size_width;
      unsigned max_input_size_height;
   unsigned deferred_source_width;
      unsigned deferred_source_height;   /* accumulated source size for per-frame builds */
   vulkan_filter_chain_swapchain_info swapchain_info;
   unsigned current_sync_index;

   struct slang_framebuffer **original_history;
   size_t num_history;
   unsigned history_ring_index;
   bool require_clear;
   bool alias_initialized;
   bool emits_hdr_colorspace;
   bool emits_hdr16_output;
};

static struct vulkan_filter_chain *slang_chain_new(
      const vulkan_filter_chain_create_info *info);
static void slang_chain_free(struct vulkan_filter_chain *chain);

/* Takes ownership; any previous preset is freed. */
static INLINE void slang_chain_set_shader_preset(
      struct vulkan_filter_chain *chain, struct video_shader *shader)
{
   free(chain->common.shader_preset);
   chain->common.shader_preset = shader;
}
static INLINE struct video_shader *slang_chain_get_shader_preset(
      struct vulkan_filter_chain *chain)
{
   return chain->common.shader_preset;
}

static void slang_chain_set_pass_info(struct vulkan_filter_chain *chain,
      unsigned pass, const vulkan_filter_chain_pass_info info);
static void slang_chain_set_shader(struct vulkan_filter_chain *chain,
      unsigned pass, VkShaderStageFlags stage,
      const uint32_t *spirv, size_t spirv_words);

static bool slang_chain_init(struct vulkan_filter_chain *chain);
static bool slang_chain_init_single_pass(struct vulkan_filter_chain *chain,
      unsigned pass_idx);
static bool slang_chain_init_alias_early(struct vulkan_filter_chain *chain);
static bool slang_chain_compile_full_pass(struct vulkan_filter_chain *chain,
      unsigned pass_idx, enum glslang_filter_chain_filter default_filter);
static bool slang_chain_finalize(struct vulkan_filter_chain *chain);
static bool slang_chain_update_swapchain_info(
      struct vulkan_filter_chain *chain,
      const vulkan_filter_chain_swapchain_info info);

static void slang_chain_notify_sync_index(struct vulkan_filter_chain *chain,
      unsigned index);
static void slang_chain_set_input_texture(struct vulkan_filter_chain *chain,
      const vulkan_filter_chain_texture texture);
static void slang_chain_build_offscreen_passes(
      struct vulkan_filter_chain *chain,
      VkCommandBuffer cmd, const VkViewport vp);
static void slang_chain_build_viewport_pass(
      struct vulkan_filter_chain *chain,
      VkCommandBuffer cmd, const VkViewport vp, const float *mvp);
static void slang_chain_end_frame(struct vulkan_filter_chain *chain,
      VkCommandBuffer cmd);

static void slang_chain_set_frame_count(struct vulkan_filter_chain *chain,
      uint64_t count);
static void slang_chain_set_frame_count_period(
      struct vulkan_filter_chain *chain, unsigned pass, unsigned period);
static void slang_chain_set_shader_subframes(
      struct vulkan_filter_chain *chain, uint32_t total_subframes);
static void slang_chain_set_current_shader_subframe(
      struct vulkan_filter_chain *chain, uint32_t current_subframe);
#ifdef VULKAN_ROLLING_SCANLINE_SIMULATION
static void slang_chain_set_simulate_scanline(
      struct vulkan_filter_chain *chain, bool simulate_scanline);
#endif /* VULKAN_ROLLING_SCANLINE_SIMULATION */
static void slang_chain_set_frame_direction(
      struct vulkan_filter_chain *chain, int32_t direction);
static void slang_chain_set_frame_time_delta(
      struct vulkan_filter_chain *chain, uint32_t time_delta);
static void slang_chain_set_original_fps(struct vulkan_filter_chain *chain,
      float fps);
static void slang_chain_set_rotation(struct vulkan_filter_chain *chain,
      uint32_t rot);
static void slang_chain_set_core_aspect(struct vulkan_filter_chain *chain,
      float coreaspect);
static void slang_chain_set_core_aspect_rot(
      struct vulkan_filter_chain *chain, float coreaspect);
#ifdef VULKAN_HDR_SWAPCHAIN
static void slang_chain_set_hdr_mode(struct vulkan_filter_chain *chain,
      unsigned hdr_mode);
static void slang_chain_set_paper_white_nits(
      struct vulkan_filter_chain *chain, float paper_white_nits);
static void slang_chain_set_expand_gamut(struct vulkan_filter_chain *chain,
      unsigned expand_gamut);
static void slang_chain_set_scanlines(struct vulkan_filter_chain *chain,
      float scanlines);
static void slang_chain_set_subpixel_layout(
      struct vulkan_filter_chain *chain, unsigned subpixel_layout);
static void slang_chain_set_inverse_tonemap(
      struct vulkan_filter_chain *chain, float inverse_tonemap);
static void slang_chain_set_hdr10(struct vulkan_filter_chain *chain,
      float hdr10);
#endif /* VULKAN_HDR_SWAPCHAIN */

static void slang_chain_set_pass_name(struct vulkan_filter_chain *chain,
      unsigned pass, const char *name);
/* Takes ownership of *texture, which is zeroed. */
static bool slang_chain_add_static_texture(struct vulkan_filter_chain *chain,
      struct slang_static_texture *texture);

static void slang_chain_add_parameter(struct vulkan_filter_chain *chain,
      unsigned pass, unsigned parameter_index, const char *id);
static void slang_chain_release_staging_buffers(
      struct vulkan_filter_chain *chain);

static VkFormat slang_chain_get_pass_rt_format(
      struct vulkan_filter_chain *chain, unsigned pass);

static bool slang_chain_emits_hdr10(const struct vulkan_filter_chain *chain);
static void slang_chain_set_emits_hdr10(struct vulkan_filter_chain *chain);
static bool slang_chain_emits_hdr16(const struct vulkan_filter_chain *chain);
static void slang_chain_set_emits_hdr16(struct vulkan_filter_chain *chain);

static void slang_chain_flush(struct vulkan_filter_chain *chain);
static void slang_chain_set_num_passes(struct vulkan_filter_chain *chain,
      unsigned passes);
static void slang_chain_execute_deferred(struct vulkan_filter_chain *chain);
static void slang_chain_set_num_sync_indices(
      struct vulkan_filter_chain *chain, unsigned num_indices);
static void slang_chain_set_swapchain_info(struct vulkan_filter_chain *chain,
      const vulkan_filter_chain_swapchain_info info);
static bool slang_chain_init_ubo(struct vulkan_filter_chain *chain);
static bool slang_chain_init_history(struct vulkan_filter_chain *chain);
static bool slang_chain_init_feedback(struct vulkan_filter_chain *chain);
static bool slang_chain_init_alias(struct vulkan_filter_chain *chain);
static void slang_chain_update_history(struct vulkan_filter_chain *chain,
      struct deferred_disposes *disposer, VkCommandBuffer cmd);
static void slang_chain_clear_history_and_feedback(
      struct vulkan_filter_chain *chain, VkCommandBuffer cmd);
static void slang_chain_update_feedback_info(
      struct vulkan_filter_chain *chain);
static void slang_chain_update_history_info(
      struct vulkan_filter_chain *chain);

static uint32_t find_memory_type_fallback(
      const VkPhysicalDeviceMemoryProperties *mem_props,
      uint32_t device_reqs, uint32_t host_reqs)
{
   unsigned i;
   for (i = 0; i < VK_MAX_MEMORY_TYPES; i++)
   {
      if ((device_reqs & (1u << i)) &&
            (mem_props->memoryTypes[i].propertyFlags & host_reqs) == host_reqs)
         return i;
   }

   return vulkan_find_memory_type(mem_props, device_reqs, 0);
}

static void build_identity_matrix(float *data)
{
   data[ 0] = 1.0f;
   data[ 1] = 0.0f;
   data[ 2] = 0.0f;
   data[ 3] = 0.0f;
   data[ 4] = 0.0f;
   data[ 5] = 1.0f;
   data[ 6] = 0.0f;
   data[ 7] = 0.0f;
   data[ 8] = 0.0f;
   data[ 9] = 0.0f;
   data[10] = 1.0f;
   data[11] = 0.0f;
   data[12] = 0.0f;
   data[13] = 0.0f;
   data[14] = 0.0f;
   data[15] = 1.0f;
}

static VkFormat glslang_format_to_vk(glslang_format fmt)
{
#undef FMT
#define FMT(x) case SLANG_FORMAT_##x: return VK_FORMAT_##x
   switch (fmt)
   {
      FMT(R8_UNORM);
      FMT(R8_SINT);
      FMT(R8_UINT);
      FMT(R8G8_UNORM);
      FMT(R8G8_SINT);
      FMT(R8G8_UINT);
      FMT(R8G8B8A8_UNORM);
      FMT(R8G8B8A8_SINT);
      FMT(R8G8B8A8_UINT);
      FMT(R8G8B8A8_SRGB);

      FMT(A2B10G10R10_UNORM_PACK32);
      FMT(A2B10G10R10_UINT_PACK32);

      FMT(R16_UINT);
      FMT(R16_SINT);
      FMT(R16_SFLOAT);
      FMT(R16G16_UINT);
      FMT(R16G16_SINT);
      FMT(R16G16_SFLOAT);
      FMT(R16G16B16A16_UINT);
      FMT(R16G16B16A16_SINT);
      FMT(R16G16B16A16_SFLOAT);

      FMT(R32_UINT);
      FMT(R32_SINT);
      FMT(R32_SFLOAT);
      FMT(R32G32_UINT);
      FMT(R32G32_SINT);
      FMT(R32G32_SFLOAT);
      FMT(R32G32B32A32_UINT);
      FMT(R32G32B32A32_SINT);
      FMT(R32G32B32A32_SFLOAT);

      default:
         break;
   }
   return VK_FORMAT_UNDEFINED;
}

static bool vulkan_filter_chain_load_lut(
      struct slang_static_texture *out,
      VkCommandBuffer cmd,
      const struct vulkan_filter_chain_create_info *info,
      vulkan_filter_chain *chain,
      const struct video_shader_lut *shader)
{
   unsigned i;
   struct texture_image image;
   VkBufferImageCopy region;
   VkImageCreateInfo image_info;
   struct slang_buffer buffer          = { 0 };
   VkMemoryRequirements mem_reqs;
   VkImageViewCreateInfo view_info;
   VkMemoryAllocateInfo alloc;
   VkImage tex                     = VK_NULL_HANDLE;
   VkDeviceMemory memory           = VK_NULL_HANDLE;
   VkImageView view                = VK_NULL_HANDLE;
   void *ptr                       = NULL;

   image.width                     = 0;
   image.height                    = 0;
   image.pixels                    = NULL;
   image.supports_rgba             = (video_driver_get_disp_flags() & VIDEO_FLAG_USE_RGBA);

   if (!image_texture_load(&image, shader->path))
      return false;

   image_info.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   image_info.pNext                 = NULL;
   image_info.flags                 = 0;
   image_info.imageType             = VK_IMAGE_TYPE_2D;
   image_info.format                = VK_FORMAT_B8G8R8A8_UNORM;
   image_info.extent.width          = image.width;
   image_info.extent.height         = image.height;
   image_info.extent.depth          = 1;
   image_info.mipLevels             = shader->mipmap
      ? glslang_num_miplevels(image.width, image.height) : 1;
   image_info.arrayLayers           = 1;
   image_info.samples               = VK_SAMPLE_COUNT_1_BIT;
   image_info.tiling                = VK_IMAGE_TILING_OPTIMAL;
   image_info.usage                 = VK_IMAGE_USAGE_SAMPLED_BIT
                                    | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                                    | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   image_info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
   image_info.queueFamilyIndexCount = 0;
   image_info.pQueueFamilyIndices   = NULL;
   image_info.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;

   if (vkCreateImage(info->device, &image_info, NULL, &tex) != VK_SUCCESS)
   {
      image_texture_free(&image);
      return false;
   }
   vulkan_debug_mark_image(info->device, tex);
   vkGetImageMemoryRequirements(info->device, tex, &mem_reqs);

   alloc.sType                     = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.pNext                     = NULL;
   alloc.allocationSize            = mem_reqs.size;
   alloc.memoryTypeIndex           = vulkan_find_memory_type(
         &*info->memory_properties,
         mem_reqs.memoryTypeBits,
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

   if (vkAllocateMemory(info->device, &alloc, NULL, &memory) != VK_SUCCESS)
      goto error;

   vulkan_debug_mark_memory(info->device, memory);
   vkBindImageMemory(info->device, tex, memory, 0);

   view_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view_info.pNext                           = NULL;
   view_info.flags                           = 0;
   view_info.image                           = tex;
   view_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
   view_info.format                          = VK_FORMAT_B8G8R8A8_UNORM;
   view_info.components.r                    = VK_COMPONENT_SWIZZLE_R;
   view_info.components.g                    = VK_COMPONENT_SWIZZLE_G;
   view_info.components.b                    = VK_COMPONENT_SWIZZLE_B;
   view_info.components.a                    = VK_COMPONENT_SWIZZLE_A;
   view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   view_info.subresourceRange.baseMipLevel   = 0;
   view_info.subresourceRange.levelCount     = image_info.mipLevels;
   view_info.subresourceRange.baseArrayLayer = 0;
   view_info.subresourceRange.layerCount     = 1;
   if (vkCreateImageView(info->device, &view_info, NULL, &view) != VK_SUCCESS)
   {
      image_texture_free(&image);
      goto error;
   }

   slang_buffer_init(&buffer, info->device, info->memory_properties,
         image.width * image.height * sizeof(uint32_t),
         VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
   ptr                                   = slang_buffer_map(&buffer);
   memcpy(ptr, image.pixels, image.width * image.height * sizeof(uint32_t));
   slang_buffer_unmap(&buffer);

   VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,
         tex,
         VK_REMAINING_MIP_LEVELS,
         VK_IMAGE_LAYOUT_UNDEFINED,
           shader->mipmap
         ? VK_IMAGE_LAYOUT_GENERAL
         : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         0,
         VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_QUEUE_FAMILY_IGNORED,
         VK_QUEUE_FAMILY_IGNORED
         );

   region.bufferOffset                    = 0;
   region.bufferRowLength                 = 0;
   region.bufferImageHeight               = 0;
   region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   region.imageSubresource.mipLevel       = 0;
   region.imageSubresource.baseArrayLayer = 0;
   region.imageSubresource.layerCount     = 1;
   region.imageOffset.x                   = 0;
   region.imageOffset.y                   = 0;
   region.imageOffset.z                   = 0;
   region.imageExtent.width               = image.width;
   region.imageExtent.height              = image.height;
   region.imageExtent.depth               = 1;

   vkCmdCopyBufferToImage(cmd,
         buffer.buffer,
         tex,
         shader->mipmap
         ? VK_IMAGE_LAYOUT_GENERAL
         : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         1, &region);

   for (i = 1; i < image_info.mipLevels; i++)
   {
      VkImageBlit blit_region;
      unsigned src_width                        = MAX(image.width >> (i - 1), 1u);
      unsigned src_height                       = MAX(image.height >> (i - 1), 1u);
      unsigned target_width                     = MAX(image.width >> i, 1u);
      unsigned target_height                    = MAX(image.height >> i, 1u);

      blit_region.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      blit_region.srcSubresource.mipLevel       = i - 1;
      blit_region.srcSubresource.baseArrayLayer = 0;
      blit_region.srcSubresource.layerCount     = 1;
      blit_region.srcOffsets[0].x               = 0;
      blit_region.srcOffsets[0].y               = 0;
      blit_region.srcOffsets[0].z               = 0;
      blit_region.srcOffsets[1].x               = src_width;
      blit_region.srcOffsets[1].y               = src_height;
      blit_region.srcOffsets[1].z               = 1;
      blit_region.dstSubresource                = blit_region.srcSubresource;
      blit_region.dstSubresource.mipLevel       = i;
      blit_region.dstOffsets[0].x               = 0;
      blit_region.dstOffsets[0].y               = 0;
      blit_region.dstOffsets[0].z               = 0;
      blit_region.dstOffsets[1].x               = target_width;
      blit_region.dstOffsets[1].y               = target_height;
      blit_region.dstOffsets[1].z               = 1;

      /* Only injects execution and memory barriers,
       * not actual transition. */
      VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(
            cmd,
            tex,
            VK_REMAINING_MIP_LEVELS,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED);

      vkCmdBlitImage(cmd,
            tex, VK_IMAGE_LAYOUT_GENERAL,
            tex, VK_IMAGE_LAYOUT_GENERAL,
            1, &blit_region, VK_FILTER_LINEAR);
   }

   VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(
         cmd,
         tex,
         VK_REMAINING_MIP_LEVELS,
         shader->mipmap
         ? VK_IMAGE_LAYOUT_GENERAL
         : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_ACCESS_SHADER_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
         VK_QUEUE_FAMILY_IGNORED,
         VK_QUEUE_FAMILY_IGNORED);

   image_texture_free(&image);
   image.pixels = NULL;

   if (!slang_static_texture_init(out, shader->id, info->device,
            tex, view, memory, &buffer, image.width, image.height,
            shader->filter != RARCH_FILTER_NEAREST,
            image_info.mipLevels > 1,
            rarch_wrap_to_address(shader->wrap)))
      goto error;
   return true;

error:
   if (image.pixels)
      image_texture_free(&image);
   if (tex != VK_NULL_HANDLE)
      vkDestroyImage(info->device, tex, NULL);
   if (view != VK_NULL_HANDLE)
      vkDestroyImageView(info->device, view, NULL);
   if (memory != VK_NULL_HANDLE)
      vkFreeMemory(info->device, memory, NULL);
   slang_buffer_free(&buffer);
   return false;
}

static bool vulkan_filter_chain_load_luts(
      const struct vulkan_filter_chain_create_info *info,
      vulkan_filter_chain *chain,
      struct video_shader *shader)
{
   size_t i;
   VkSubmitInfo submit_info;
   VkCommandBufferAllocateInfo cmd_info;
   VkCommandBufferBeginInfo begin_info;
   VkCommandBuffer cmd                           = VK_NULL_HANDLE;

   cmd_info.sType                                = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   cmd_info.pNext                                = NULL;
   cmd_info.commandPool                          = info->command_pool;
   cmd_info.level                                = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   cmd_info.commandBufferCount                   = 1;

   if (vkAllocateCommandBuffers(info->device, &cmd_info, &cmd) != VK_SUCCESS)
      return false;

   begin_info.sType                              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin_info.pNext                              = NULL;
   begin_info.flags                              = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   begin_info.pInheritanceInfo                   = NULL;

   if (vkBeginCommandBuffer(cmd, &begin_info) != VK_SUCCESS)
   {
      vkFreeCommandBuffers(info->device, info->command_pool, 1, &cmd);
      return false;
   }

   for (i = 0; i < shader->luts; i++)
   {
      struct slang_static_texture image;
      memset(&image, 0, sizeof(image));
      if (!vulkan_filter_chain_load_lut(&image, cmd, info, chain,
               &shader->lut[i]))
      {
         RARCH_ERR("[Vulkan] Failed to load LUT \"%s\".\n", shader->lut[i].path);
         vkEndCommandBuffer(cmd);
         if (cmd != VK_NULL_HANDLE)
            vkFreeCommandBuffers(info->device, info->command_pool, 1, &cmd);
         return false;
      }
      if (!slang_chain_add_static_texture(chain, &image))
      {
         slang_static_texture_free(&image);
         vkEndCommandBuffer(cmd);
         vkFreeCommandBuffers(info->device, info->command_pool, 1, &cmd);
         return false;
      }
   }

   if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
   {
      vkFreeCommandBuffers(info->device, info->command_pool, 1, &cmd);
      return false;
   }

   submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit_info.pNext                = NULL;
   submit_info.waitSemaphoreCount   = 0;
   submit_info.pWaitSemaphores      = NULL;
   submit_info.pWaitDstStageMask    = NULL;
   submit_info.commandBufferCount   = 1;
   submit_info.pCommandBuffers      = &cmd;
   submit_info.signalSemaphoreCount = 0;
   submit_info.pSignalSemaphores    = NULL;

   {
      VkFenceCreateInfo fence_info;
      VkFence fence                = VK_NULL_HANDLE;

      fence_info.sType             = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fence_info.pNext             = NULL;
      fence_info.flags             = 0;

      vkCreateFence(info->device, &fence_info, NULL, &fence);
      if (fence == VK_NULL_HANDLE)
      {
         vkFreeCommandBuffers(info->device, info->command_pool, 1, &cmd);
         return false;
      }
      if (vkQueueSubmit(info->queue, 1, &submit_info, fence) != VK_SUCCESS)
      {
         vkDestroyFence(info->device, fence, NULL);
         vkFreeCommandBuffers(info->device, info->command_pool, 1, &cmd);
         return false;
      }
      vkWaitForFences(info->device, 1, &fence, VK_TRUE, UINT64_MAX);
      vkDestroyFence(info->device, fence, NULL);
   }

   vkFreeCommandBuffers(info->device, info->command_pool, 1, &cmd);
   slang_chain_release_staging_buffers(chain);
   return true;
}

static struct vulkan_filter_chain *slang_chain_new(
      const vulkan_filter_chain_create_info *info)
{
   struct vulkan_filter_chain *chain = (struct vulkan_filter_chain*)
      calloc(1, sizeof(*chain));
   if (!chain)
      return NULL;
   chain->device            = info->device;
   chain->gpu               = info->gpu;
   chain->memory_properties = *info->memory_properties;
   chain->cache             = info->pipeline_cache;
   chain->original_format   = info->original_format;
   common_resources_init(&chain->common, info->device,
         info->memory_properties);
   chain->max_input_size_width  = info->max_input_size.width;
   chain->max_input_size_height = info->max_input_size.height;
   chain->deferred_source_width  = chain->max_input_size_width;
   chain->deferred_source_height = chain->max_input_size_height;
   slang_chain_set_swapchain_info(chain, info->swapchain);
   slang_chain_set_num_passes(chain, info->num_passes);
   return chain;
}

static void slang_chain_free(struct vulkan_filter_chain *chain)
{
   unsigned i;
   if (!chain)
      return;
   slang_chain_flush(chain);
   for (i = 0; i < chain->pass_count; i++)
      slang_pass_free(chain->passes[i]);
   free(chain->passes);
   for (i = 0; i < chain->num_history; i++)
      slang_framebuffer_delete(&chain->original_history[i]);
   free(chain->original_history);
   for (i = 0; i < chain->num_deferred; i++)
      free(chain->deferred_calls[i].calls);
   free(chain->deferred_calls);
   free(chain->pass_info);
   /* Last, as in the implicit C++ member-destruction order it replaces:
    * the pool must stay alive through the flush drain above. */
   common_resources_free(&chain->common);
   free(chain);
}

static void slang_chain_set_swapchain_info(struct vulkan_filter_chain *chain,
      
      const vulkan_filter_chain_swapchain_info info)
{
   chain->swapchain_info = info;
   slang_chain_set_num_sync_indices(chain, info.num_indices);
}

static void slang_chain_set_num_sync_indices(struct vulkan_filter_chain *chain,
      unsigned num_indices)
{
   unsigned i;
   slang_chain_execute_deferred(chain);
   /* Every queue is empty now; only capacity storage remains. */
   for (i = num_indices; i < chain->num_deferred; i++)
      free(chain->deferred_calls[i].calls);
   if (!num_indices)
   {
      free(chain->deferred_calls);
      chain->deferred_calls = NULL;
   }
   else
   {
      struct deferred_disposes *new_calls = (struct deferred_disposes*)
         realloc(chain->deferred_calls, num_indices * sizeof(*new_calls));
      if (!new_calls)
      {
         /* Growth failed: keep the old, still-valid block and count.
          * (The vector this replaces terminated the process here.) */
         RARCH_ERR("[Vulkan] Failed to size deferred-disposal queues.\n");
         return;
      }
      for (i = chain->num_deferred; i < num_indices; i++)
      {
         new_calls[i].calls = NULL;
         new_calls[i].size  = 0;
         new_calls[i].cap   = 0;
      }
      chain->deferred_calls = new_calls;
   }
   chain->num_deferred = num_indices;
}

static void slang_chain_notify_sync_index(struct vulkan_filter_chain *chain,
      unsigned index)
{
   unsigned i;
   if (index < chain->num_deferred)
      deferred_disposes_run_clear(&chain->deferred_calls[index]);

   chain->current_sync_index = index;

   for (i = 0; i < chain->pass_count; i++)
      slang_pass_notify_sync_index(chain->passes[i], index);
}

static bool slang_chain_update_swapchain_info(struct vulkan_filter_chain *chain,
      
      const vulkan_filter_chain_swapchain_info info)
{
   slang_chain_flush(chain);
   slang_chain_set_swapchain_info(chain, info);
   return slang_chain_init(chain);
}

static void slang_chain_release_staging_buffers(struct vulkan_filter_chain *chain)
{
   unsigned i;
   for (i = 0; i < chain->common.num_luts; i++)
      slang_buffer_free(&chain->common.luts[i].buffer);
}

static void slang_chain_execute_deferred(struct vulkan_filter_chain *chain)
{
   unsigned i;
   for (i = 0; i < chain->num_deferred; i++)
      deferred_disposes_run_clear(&chain->deferred_calls[i]);
}

static void slang_chain_flush(struct vulkan_filter_chain *chain)
{
   vkDeviceWaitIdle(chain->device);
   slang_chain_execute_deferred(chain);
}

static void slang_chain_update_history_info(struct vulkan_filter_chain *chain)
{
   unsigned i;
   unsigned hist_size = (unsigned)chain->num_history;

   for (i = 0; i < hist_size; i++)
   {
      /* Map logical index i (0 = most recent) to the ring buffer slot. */
      unsigned ring_slot = (chain->history_ring_index + i) % hist_size;
      Texture *source = &chain->common.original_history[i];

      source->texture.layout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      source->texture.view     = chain->original_history[ring_slot]->view;
      source->texture.image    = chain->original_history[ring_slot]->image;
      source->texture.width    = chain->original_history[ring_slot]->size_width;
      source->texture.height   = chain->original_history[ring_slot]->size_height;
      source->filter           = chain->passes[0]->pass_info.source_filter;
      source->mip_filter       = chain->passes[0]->pass_info.mip_filter;
      source->address          = chain->passes[0]->pass_info.address;
   }
}

static void slang_chain_update_feedback_info(struct vulkan_filter_chain *chain)
{
   unsigned i;
   if (!chain->common.num_fb_feedback)
      return;

   for (i = 0; i < chain->pass_count - 1; i++)
   {
      struct slang_framebuffer *fb = chain->passes[i]->fb_feedback;
      Texture *source;
      if (!fb)
         continue;

      source                  = &chain->common.fb_feedback[i];

      source->texture.image   = fb->image;
      source->texture.view    = fb->view;
      source->texture.layout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      source->texture.width   = fb->size_width;
      source->texture.height  = fb->size_height;
      source->filter          = chain->passes[i]->pass_info.source_filter;
      source->mip_filter      = chain->passes[i]->pass_info.mip_filter;
      source->address         = chain->passes[i]->pass_info.address;
   }
}

static void slang_chain_build_offscreen_passes(struct vulkan_filter_chain *chain,
      VkCommandBuffer cmd,
      const VkViewport vp)
{
   unsigned i;
   Texture source;
   struct deferred_disposes *disposer;
   Texture original;

   /* First frame, make sure our history and feedback textures
    * are in a clean state. */
   if (chain->require_clear)
   {
      slang_chain_clear_history_and_feedback(chain, cmd);
      chain->require_clear = false;
   }

   slang_chain_update_history_info(chain);
   slang_chain_update_feedback_info(chain);

   disposer            = &chain->deferred_calls[chain->current_sync_index];
   original.texture    = chain->input_texture;
   original.filter     = chain->passes[0]->pass_info.source_filter;
   original.mip_filter = chain->passes[0]->pass_info.mip_filter;
   original.address    = chain->passes[0]->pass_info.address;

   source = original;

   /* A pass may sample PassOutput[j] for a j that has not been produced
    * yet this frame (a forward or self reference), and on the first frame
    * no pass output exists at all. chain->common.pass_outputs entries are only
    * filled lazily below, after each pass renders, so any not-yet-produced
    * slot would otherwise still hold a zero-initialized Texture whose image
    * view is VK_NULL_HANDLE. Binding that null view into a descriptor set
    * and submitting it to vkUpdateDescriptorSets crashes inside the driver.
    * Seed every slot with the current input texture so unproduced outputs
    * sample defined data; real outputs overwrite their slot as they render. */
   for (i = 0; i < chain->common.num_pass_outputs; i++)
      chain->common.pass_outputs[i] = original;

   for (i = 0; i < chain->pass_count - 1; i++)
   {
      const struct slang_framebuffer *fb;
      slang_pass_build_commands(chain->passes[i], disposer, cmd,
            &original, &source, &vp, NULL);

      fb = chain->passes[i]->framebuffer;

      source.texture.image    = fb->image;
      source.texture.view     = fb->view;
      source.texture.layout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      source.texture.width    = fb->size_width;
      source.texture.height   = fb->size_height;
      source.filter           = chain->passes[i + 1]->pass_info.source_filter;
      source.mip_filter       = chain->passes[i + 1]->pass_info.mip_filter;
      source.address          = chain->passes[i + 1]->pass_info.address;

      chain->common.pass_outputs[i]  = source;
   }
}

static void slang_chain_update_history(struct vulkan_filter_chain *chain,
      struct deferred_disposes *disposer,
      VkCommandBuffer cmd)
{
   unsigned next_history_ring_index;
   struct slang_framebuffer *target;
   bool copy_history;
   VkImageLayout src_layout = chain->input_texture.layout;
   unsigned hist_size       = (unsigned)chain->num_history;

   /* Transition input texture to something appropriate. */
   if (chain->input_texture.layout != VK_IMAGE_LAYOUT_GENERAL)
   {
      VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,
            chain->input_texture.image,VK_REMAINING_MIP_LEVELS,
            chain->input_texture.layout,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            0,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED);

      src_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   }

   /* Advance ring index backwards: the oldest slot becomes the newest.
    * This replaces the O(N) move_backward with O(1) index arithmetic. */
   next_history_ring_index = (chain->history_ring_index == 0)
      ? hist_size - 1
      : chain->history_ring_index - 1;

   target       = chain->original_history[next_history_ring_index];
   copy_history = true;

   if   (    chain->input_texture.width  != target->size_width
         ||  chain->input_texture.height != target->size_height
         || (chain->input_texture.format != VK_FORMAT_UNDEFINED
         &&  chain->input_texture.format != target->format))
   {
      unsigned new_width  = chain->input_texture.width;
      unsigned new_height = chain->input_texture.height;
      copy_history    = slang_framebuffer_set_size(target, disposer,
            new_width, new_height, chain->input_texture.format);
   }

   if (copy_history)
   {
      chain->history_ring_index = next_history_ring_index;
      vulkan_framebuffer_copy(target->image,
            target->size_width, target->size_height,
            cmd, chain->input_texture.image, src_layout);
   }
   else
      RARCH_ERR("[Vulkan] Failed to resize shader history framebuffer.\n");

   /* Transition input texture back. */
   if (chain->input_texture.layout != VK_IMAGE_LAYOUT_GENERAL)
   {
      VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,
            chain->input_texture.image,VK_REMAINING_MIP_LEVELS,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            chain->input_texture.layout,
            0,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED);
   }
}

static void slang_chain_end_frame(struct vulkan_filter_chain *chain,
      VkCommandBuffer cmd)
{
   /* If we need to keep old frames, copy it after fragment is complete.
    * TODO: We can improve pipelining by figuring out which
    * pass is the last that reads from
    * the history and dispatch the copy earlier. */
   if (chain->num_history)
   {
      slang_chain_update_history(chain, &chain->deferred_calls[chain->current_sync_index], cmd);
   }
}

static void slang_chain_build_viewport_pass(struct vulkan_filter_chain *chain,
      
      VkCommandBuffer cmd, const VkViewport vp, const float *mvp)
{
   unsigned i;
   Texture source;
   Texture original;
   struct deferred_disposes *disposer = &chain->deferred_calls[chain->current_sync_index];

   original.texture    = chain->input_texture;
   original.filter     = chain->passes[0]->pass_info.source_filter;
   original.mip_filter = chain->passes[0]->pass_info.mip_filter;
   original.address    = chain->passes[0]->pass_info.address;

   if (chain->pass_count == 1)
   {
      source.texture    = chain->input_texture;
      source.filter     = chain->passes[chain->pass_count - 1]->pass_info.source_filter;
      source.mip_filter = chain->passes[chain->pass_count - 1]->pass_info.mip_filter;
      source.address    = chain->passes[chain->pass_count - 1]->pass_info.address;
   }
   else
   {
      const struct slang_framebuffer *fb = chain->passes[chain->pass_count - 2]->framebuffer;
      source.texture.image   = fb->image;
      source.texture.view    = fb->view;
      source.texture.layout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      source.texture.width   = fb->size_width;
      source.texture.height  = fb->size_height;
      source.filter          = chain->passes[chain->pass_count - 1]->pass_info.source_filter;
      source.mip_filter      = chain->passes[chain->pass_count - 1]->pass_info.mip_filter;
      source.address         = chain->passes[chain->pass_count - 1]->pass_info.address;
   }

   slang_pass_build_commands(chain->passes[chain->pass_count - 1], disposer, cmd,
         &original, &source, &vp, mvp);

   /* For feedback FBOs, swap current and previous. */
   for (i = 0; i < chain->pass_count; i++)
      slang_pass_end_frame(chain->passes[i]);
}

static bool slang_chain_init_history(struct vulkan_filter_chain *chain)
{
   unsigned i;
   size_t required_images = 0;

   for (i = 0; i < chain->num_history; i++)
      slang_framebuffer_delete(&chain->original_history[i]);
   free(chain->original_history);
   chain->original_history = NULL;
   chain->num_history      = 0;
   texture_array_resize(&chain->common.original_history,
         &chain->common.num_original_history, 0);
   chain->history_ring_index = 0;

   for (i = 0; i < chain->pass_count; i++)
   {
      size_t _y = chain->passes[i]->reflection.semantic_textures[
               SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY].size;
      required_images = MAX(required_images, _y);
   }

   if (required_images < 2)
   {
#ifdef VULKAN_DEBUG
      RARCH_LOG("[Vulkan] Not using frame history.\n");
#endif
      return true;
   }

   /* We don't need to store array element #0,
    * since it's aliased with the actual original. */
   required_images--;
   if (!(chain->original_history = (struct slang_framebuffer**)
            calloc(required_images, sizeof(*chain->original_history))))
      return false;
   if (!texture_array_resize(&chain->common.original_history,
            &chain->common.num_original_history, required_images))
      return false;

   for (i = 0; i < required_images; i++)
   {
      if (!(chain->original_history[i] = slang_framebuffer_new(
               chain->device, &chain->memory_properties,
               chain->max_input_size_width, chain->max_input_size_height,
               chain->original_format,
               1, &chain->common.framebuffer_pool)))
         return false;
      chain->num_history++;
   }

#ifdef VULKAN_DEBUG
   RARCH_LOG("[Vulkan] Using history of %u frames.\n", (unsigned)(required_images));
#endif

   /* On first frame, we need to clear the textures to
    * a known state, but we need
    * a command buffer for that, so just defer to first frame.
    */
   chain->require_clear = true;
   return true;
}

static bool slang_chain_init_feedback(struct vulkan_filter_chain *chain)
{
   unsigned i;
   bool use_feedbacks = false;

   texture_array_resize(&chain->common.fb_feedback, &chain->common.num_fb_feedback, 0);

   /* Final pass cannot have feedback. */
   for (i = 0; i < chain->pass_count - 1; i++)
   {
      size_t j;
      bool use_feedback = false;
      for (j = 0; j < chain->pass_count; j++)
      {
         const struct slang_pass *pass = chain->passes[j];
         const slang_reflection *r = &pass->reflection;
         const slang_texture_semantic_array *feedbacks =
            &r->semantic_textures[SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK];

         if (i < feedbacks->size && feedbacks->data[i].texture)
         {
            use_feedback  = true;
            use_feedbacks = true;
            break;
         }
      }

      if (use_feedback)
      {
         if (!slang_pass_init_feedback(chain->passes[i]))
            return false;
         RARCH_LOG("[Vulkan] Using framebuffer feedback for pass #%u.\n", i);
      }
   }

   if (!use_feedbacks)
   {
#ifdef VULKAN_DEBUG
      RARCH_LOG("[Vulkan] Not using framebuffer feedback.\n");
#endif
      return true;
   }

   if (!texture_array_resize(&chain->common.fb_feedback,
            &chain->common.num_fb_feedback, chain->pass_count - 1))
      return false;
   chain->require_clear = true;
   return true;
}

static bool slang_chain_init_alias(struct vulkan_filter_chain *chain)
{
   unsigned i;

   slang_texture_semantic_name_map_free(&chain->common.texture_semantic_map);
   slang_texture_semantic_name_map_free(&chain->common.texture_semantic_uniform_map);

   for (i = 0; i < (unsigned)chain->pass_count; i++)
   {
      const char *name = slang_pass_get_name(chain->passes[i]);
      if (!*name)
         continue;

      if (!slang_texture_semantic_name_map_set_unique(
               &chain->common.texture_semantic_map, name, NULL,
               SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, i))
         return false;

      if (!slang_texture_semantic_name_map_set_unique(
               &chain->common.texture_semantic_uniform_map, name, "Size",
               SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, i))
         return false;

      if (!slang_texture_semantic_name_map_set_unique(
               &chain->common.texture_semantic_map, name, "Feedback",
               SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, i))
         return false;

      if (!slang_texture_semantic_name_map_set_unique(
               &chain->common.texture_semantic_uniform_map, name, "FeedbackSize",
               SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, i))
         return false;
   }

   for (i = 0; i < (unsigned)chain->common.num_luts; i++)
   {
      if (!slang_texture_semantic_name_map_set_unique(
               &chain->common.texture_semantic_map,
               chain->common.luts[i].id, NULL,
               SLANG_TEXTURE_SEMANTIC_USER, i))
         return false;

      if (!slang_texture_semantic_name_map_set_unique(
               &chain->common.texture_semantic_uniform_map,
               chain->common.luts[i].id, "Size",
               SLANG_TEXTURE_SEMANTIC_USER, i))
         return false;
   }

   return true;
}

static void slang_chain_set_pass_info(struct vulkan_filter_chain *chain,
      unsigned pass,
      const vulkan_filter_chain_pass_info info)
{
   chain->pass_info[pass] = info;
}

static VkFormat slang_chain_get_pass_rt_format(struct vulkan_filter_chain *chain,
      unsigned pass)
{
   return chain->pass_info[pass].rt_format;
}

static bool slang_chain_emits_hdr10(const struct vulkan_filter_chain *chain)
{
   return chain->emits_hdr_colorspace;
}

static void slang_chain_set_emits_hdr10(struct vulkan_filter_chain *chain)
{
   chain->emits_hdr_colorspace = true;
}

static bool slang_chain_emits_hdr16(const struct vulkan_filter_chain *chain)
{
   return chain->emits_hdr16_output;
}

static void slang_chain_set_emits_hdr16(struct vulkan_filter_chain *chain)
{
   chain->emits_hdr16_output = true;
}

static void slang_chain_set_num_passes(struct vulkan_filter_chain *chain,
      unsigned num_passes)
{
   unsigned i;

   {
      vulkan_filter_chain_pass_info *new_info =
         (vulkan_filter_chain_pass_info*)realloc(chain->pass_info,
               num_passes * sizeof(*chain->pass_info));
      if (!new_info && num_passes)
         return;
      chain->pass_info = new_info;
      if (num_passes > chain->pass_info_count)
         memset(&chain->pass_info[chain->pass_info_count], 0,
               (num_passes - chain->pass_info_count) * sizeof(*chain->pass_info));
      chain->pass_info_count = num_passes;
   }
   for (i = 0; i < chain->pass_count; i++)
      slang_pass_free(chain->passes[i]);
   free(chain->passes);
   chain->pass_count = 0;
   if (!(chain->passes = (struct slang_pass**)
            calloc(num_passes, sizeof(*chain->passes))))
      return;
   for (i = 0; i < num_passes; i++)
   {
      if (!(chain->passes[i] = slang_pass_new(chain->device, &chain->memory_properties,
               chain->cache, chain->num_deferred, i + 1 == num_passes)))
         return;
      chain->passes[i]->common      = &chain->common;
      chain->passes[i]->pass_number = i;
      chain->pass_count++;
   }
}

static void slang_chain_set_shader(struct vulkan_filter_chain *chain,
      
      unsigned pass,
      VkShaderStageFlags stage,
      const uint32_t *spirv,
      size_t spirv_words)
{
   slang_pass_set_shader(chain->passes[pass], stage, spirv, spirv_words);
}

static void slang_chain_add_parameter(struct vulkan_filter_chain *chain,
      unsigned pass,
      unsigned index, const char *id)
{
   slang_pass_add_parameter(chain->passes[pass], index, id);
}

static bool slang_chain_init_ubo(struct vulkan_filter_chain *chain)
{
   unsigned i;
   VkPhysicalDeviceProperties props;

   slang_buffer_free(&chain->common.ubo);
   chain->common.ubo_offset            = 0;

   vkGetPhysicalDeviceProperties(chain->gpu, &props);
   chain->common.ubo_alignment         = props.limits.minUniformBufferOffsetAlignment;

   /* Who knows. :) */
   if (chain->common.ubo_alignment == 0)
      chain->common.ubo_alignment = 1;

   for (i = 0; i < chain->pass_count; i++)
      slang_pass_allocate_buffers(chain->passes[i]);

   chain->common.ubo_offset            =
      (chain->common.ubo_offset + chain->common.ubo_alignment - 1) &
      ~(chain->common.ubo_alignment - 1);
   chain->common.ubo_sync_index_stride = chain->common.ubo_offset;

   if (chain->common.ubo_offset != 0)
      slang_buffer_init(&chain->common.ubo, chain->device,
            &chain->memory_properties, chain->common.ubo_offset * chain->num_deferred,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

   if (chain->common.ubo.buffer != VK_NULL_HANDLE)
      chain->common.ubo_mapped         = ((uint8_t*)(slang_buffer_map(&chain->common.ubo)));
   else
      chain->common.ubo_mapped         = NULL;
   return true;
}

static bool slang_chain_init(struct vulkan_filter_chain *chain)
{
   unsigned i;
   unsigned source_w = chain->max_input_size_width;
   unsigned source_h = chain->max_input_size_height;

   if (!slang_chain_init_alias(chain))
      return false;
   chain->alias_initialized = true;

   for (i = 0; i < chain->pass_count; i++)
   {
      const char *name = slang_pass_get_name(chain->passes[i]);
      RARCH_DBG("[Vulkan] Building pass #%u (%s).\n", i,
            (name && *name)
            ? name
            : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));
      slang_pass_set_pass_info(chain->passes[i],
            chain->max_input_size_width, chain->max_input_size_height,
            source_w, source_h, chain->swapchain_info, chain->pass_info[i],
            &source_w, &source_h);
      if (!slang_pass_build(chain->passes[i]))
         return false;
   }

   chain->require_clear = false;
   if (!slang_chain_init_ubo(chain))
      return false;
   RARCH_DBG("[Vulkan] Chain UBO ready.\n");
   if (!slang_chain_init_history(chain))
      return false;
   RARCH_DBG("[Vulkan] Chain history ready.\n");
   if (!slang_chain_init_feedback(chain))
      return false;
   RARCH_DBG("[Vulkan] Chain feedback ready.\n");
   texture_array_resize(&chain->common.pass_outputs,
         &chain->common.num_pass_outputs, chain->pass_count);
   return true;
}

static bool slang_chain_init_single_pass(struct vulkan_filter_chain *chain,
      unsigned pass_idx)
{
   if (pass_idx >= chain->pass_count)
      return false;

   /* Only call set_pass_info on this pass, using the accumulated
    * source size. set_pass_info calls clear_vk() which destroys
    * Vulkan objects — we must NOT re-call it on prior chain->passes. */
   slang_pass_set_pass_info(chain->passes[pass_idx],
         chain->max_input_size_width, chain->max_input_size_height,
         chain->deferred_source_width, chain->deferred_source_height,
         chain->swapchain_info, chain->pass_info[pass_idx],
         &chain->deferred_source_width, &chain->deferred_source_height);

   RARCH_LOG("[Vulkan] Building pass #%u (%s)\n", pass_idx,
         *slang_pass_get_name(chain->passes[pass_idx])
         ? slang_pass_get_name(chain->passes[pass_idx])
         : msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));

   if (!slang_pass_build(chain->passes[pass_idx]))
      return false;

   return true;
}

static bool slang_chain_compile_full_pass(struct vulkan_filter_chain *chain,
      unsigned pass_idx,
      glslang_filter_chain_filter default_filter)
{
   size_t j;
   const struct video_shader_pass *pass;
   const struct video_shader_pass *next_pass;
   glslang_output output;
   struct vulkan_filter_chain_pass_info p_info;
   bool explicit_format;
   struct video_shader *shader = chain->common.shader_preset;

   if (!shader || pass_idx >= chain->pass_count)
      return false;

   /* Extra opaque pass (last_pass_is_fbo) — SPIRV already set */

   if (pass_idx >= shader->passes)
      return slang_chain_init_single_pass(chain, pass_idx);

   pass      = &shader->pass[pass_idx];
   next_pass = pass_idx + 1 < shader->passes
      ? &shader->pass[pass_idx + 1] : NULL;

   /* ---- SPIRV cross-compile (CPU) ---- */
   if (!glslang_compile_shader(pass->source.path, &output))
   {
      RARCH_ERR("[Vulkan] Failed to compile shader: \"%s\".\n",
            pass->source.path);
      return false;
   }

   /* ---- Extract parameters ---- */
   for (j = 0; j < output.meta.num_parameters; j++)
   {
      const glslang_parameter *meta_param = &output.meta.parameters[j];
      struct video_shader_parameter *itr;

      if (shader->num_parameters >= GFX_MAX_PARAMETERS)
      {
         RARCH_ERR("[Vulkan] Exceeded maximum number of parameters (%u).\n",
               GFX_MAX_PARAMETERS);
         glslang_output_free(&output);
         return false;
      }

      itr = NULL;
      {
         unsigned k;
         size_t mid_len = strlen(meta_param->id);
         for (k = 0; k < shader->num_parameters; k++)
         {
            /* Gate the memcmp behind two byte loads; the scan is
             * O(n^2) across Mega Bezel-scale parameter counts. */
            const char *sid = shader->parameters[k].id;
            if (sid[0] == meta_param->id[0] && sid[mid_len] == '\0'
                  && !memcmp(sid, meta_param->id, mid_len))
            {
               itr = &shader->parameters[k];
               break;
            }
         }
      }

      if (itr)
      {
         if (   strcmp(meta_param->desc, itr->desc)
             || meta_param->initial != itr->initial
             || meta_param->minimum != itr->minimum
             || meta_param->maximum != itr->maximum
             || meta_param->step    != itr->step)
         {
            RARCH_ERR("[Vulkan] Duplicate parameters found for \"%s\","
                  " but arguments do not match.\n", itr->id);
            glslang_output_free(&output);
            return false;
         }
         slang_chain_add_parameter(chain, pass_idx,
               (unsigned)(itr - shader->parameters), meta_param->id);
      }
      else
      {
         struct video_shader_parameter *param =
            &shader->parameters[shader->num_parameters];
         strlcpy(param->id, meta_param->id, sizeof(param->id));
         strlcpy(param->desc, meta_param->desc, sizeof(param->desc));
         param->initial = meta_param->initial;
         param->minimum = meta_param->minimum;
         param->maximum = meta_param->maximum;
         param->step    = meta_param->step;
         slang_chain_add_parameter(chain, pass_idx, shader->num_parameters, meta_param->id);
         shader->num_parameters++;
      }
   }

   /* ---- Set SPIRV on the pass ---- */
   slang_chain_set_shader(chain, pass_idx, VK_SHADER_STAGE_VERTEX_BIT,
         output.vertex, output.vertex_len);
   slang_chain_set_shader(chain, pass_idx, VK_SHADER_STAGE_FRAGMENT_BIT,
         output.fragment, output.fragment_len);

   slang_chain_set_frame_count_period(chain, pass_idx, pass->frame_count_mod);

   /* ---- Pass name ---- */
   if (output.meta.name[0])
      slang_chain_set_pass_name(chain, pass_idx, output.meta.name);
   if (*pass->alias)
      slang_chain_set_pass_name(chain, pass_idx, pass->alias);

   /* Update alias map incrementally */
   if (*slang_pass_get_name(chain->passes[pass_idx]))
   {
      chain->alias_initialized = false;
      if (!slang_chain_init_alias_early(chain))
      {
         glslang_output_free(&output);
         return false;
      }
   }

   /* ---- Pass info (scale, filter, format) ---- */
   p_info.scale_type_x  = GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL;
   p_info.scale_type_y  = GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL;
   p_info.scale_x       = 0.0f;
   p_info.scale_y       = 0.0f;
   p_info.rt_format     = VK_FORMAT_UNDEFINED;
   p_info.source_filter = GLSLANG_FILTER_CHAIN_LINEAR;
   p_info.mip_filter    = GLSLANG_FILTER_CHAIN_LINEAR;
   p_info.address       = GLSLANG_FILTER_CHAIN_ADDRESS_REPEAT;
   p_info.max_levels    = 0;

   if (pass->filter == RARCH_FILTER_UNSPEC)
      p_info.source_filter = default_filter;
   else
   {
      p_info.source_filter =
         pass->filter == RARCH_FILTER_LINEAR
         ? GLSLANG_FILTER_CHAIN_LINEAR
         : GLSLANG_FILTER_CHAIN_NEAREST;
   }
   p_info.address    = rarch_wrap_to_address(pass->wrap);
   p_info.max_levels = 1;

   if (next_pass && next_pass->mipmap)
      p_info.max_levels = ~0u;

   p_info.mip_filter =
      (pass->filter != RARCH_FILTER_NEAREST && p_info.max_levels > 1)
      ? GLSLANG_FILTER_CHAIN_LINEAR
      : GLSLANG_FILTER_CHAIN_NEAREST;

   explicit_format = output.meta.rt_format != SLANG_FORMAT_UNKNOWN;

   if (output.meta.rt_format == SLANG_FORMAT_UNKNOWN)
      output.meta.rt_format = SLANG_FORMAT_R8G8B8A8_UNORM;

   if (!(pass->fbo.flags & FBO_SCALE_FLAG_VALID))
   {
      p_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
      p_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
      p_info.scale_x      = 1.0f;
      p_info.scale_y      = 1.0f;

      if (pass_idx + 1 == shader->passes)
      {
         VkFormat pass_format;
         p_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
         p_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;

         pass_format = glslang_format_to_vk(output.meta.rt_format);

         if (explicit_format && vulkan_is_hdr10_format(pass_format))
            slang_chain_set_emits_hdr10(chain);
#ifdef VULKAN_HDR_SWAPCHAIN
         if (explicit_format && pass_format == VK_FORMAT_R16G16B16A16_SFLOAT)
            slang_chain_set_emits_hdr16(chain);
#endif
         p_info.rt_format = chain->swapchain_info.format;

         if (explicit_format && pass_format != p_info.rt_format)
            RARCH_WARN("[Vulkan] Using explicit format for last pass in chain,"
                  " but it is not rendered to framebuffer,"
                  " using swapchain format instead.\n");
      }
      else
      {
         p_info.rt_format = glslang_format_to_vk(output.meta.rt_format);
         RARCH_LOG("[Vulkan] Using render target format %s for pass output #%u.\n",
               glslang_format_to_string(output.meta.rt_format), pass_idx);
      }
   }
   else
   {
      if (pass->fbo.flags & FBO_SCALE_FLAG_SRGB_FBO)
         output.meta.rt_format = SLANG_FORMAT_R8G8B8A8_SRGB;
      else if (pass->fbo.flags & FBO_SCALE_FLAG_FP_FBO)
         output.meta.rt_format = SLANG_FORMAT_R16G16B16A16_SFLOAT;
      else if (pass->fbo.flags & FBO_SCALE_FLAG_RGB10_FBO)
         output.meta.rt_format = SLANG_FORMAT_A2B10G10R10_UNORM_PACK32;

      p_info.rt_format = glslang_format_to_vk(output.meta.rt_format);

      RARCH_LOG("[Vulkan] Using render target format %s for pass output #%u.\n",
            glslang_format_to_string(output.meta.rt_format), pass_idx);

#ifdef VULKAN_HDR_SWAPCHAIN
      if (pass_idx + 1 == shader->passes)
      {
         if (explicit_format && vulkan_is_hdr10_format(p_info.rt_format))
            slang_chain_set_emits_hdr10(chain);
         else if (explicit_format && p_info.rt_format == VK_FORMAT_R16G16B16A16_SFLOAT)
            slang_chain_set_emits_hdr16(chain);
      }
#endif

      switch (pass->fbo.type_x)
      {
         case RARCH_SCALE_INPUT:
            p_info.scale_x      = pass->fbo.scale_x;
            p_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
            break;
         case RARCH_SCALE_ABSOLUTE:
            p_info.scale_x      = (float)(pass->fbo.abs_x);
            p_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE;
            break;
         case RARCH_SCALE_VIEWPORT:
            p_info.scale_x      = pass->fbo.scale_x;
            p_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
            break;
      }

      switch (pass->fbo.type_y)
      {
         case RARCH_SCALE_INPUT:
            p_info.scale_y      = pass->fbo.scale_y;
            p_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
            break;
         case RARCH_SCALE_ABSOLUTE:
            p_info.scale_y      = (float)(pass->fbo.abs_y);
            p_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE;
            break;
         case RARCH_SCALE_VIEWPORT:
            p_info.scale_y      = pass->fbo.scale_y;
            p_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
            break;
      }
   }

   slang_chain_set_pass_info(chain, pass_idx, p_info);
   glslang_output_free(&output);

   /* ---- Vulkan pipeline creation ---- */
   return slang_chain_init_single_pass(chain, pass_idx);
}

static bool slang_chain_init_alias_early(struct vulkan_filter_chain *chain)
{
   if (chain->alias_initialized)
      return true;
   if (!slang_chain_init_alias(chain))
      return false;
   chain->alias_initialized = true;
   return true;
}

static bool slang_chain_finalize(struct vulkan_filter_chain *chain)
{
   if (!chain->alias_initialized)
   {
      if (!slang_chain_init_alias(chain))
         return false;
      chain->alias_initialized = true;
   }

   chain->require_clear = false;
   if (!slang_chain_init_ubo(chain))
      return false;
   if (!slang_chain_init_history(chain))
      return false;
   if (!slang_chain_init_feedback(chain))
      return false;
   texture_array_resize(&chain->common.pass_outputs,
         &chain->common.num_pass_outputs, chain->pass_count);
   return true;
}

static void slang_chain_clear_history_and_feedback(struct vulkan_filter_chain *chain,
      VkCommandBuffer cmd)
{
   unsigned i;
   for (i = 0; i < chain->num_history; i++)
      vulkan_framebuffer_clear(chain->original_history[i]->image, cmd);
   for (i = 0; i < chain->pass_count; i++)
   {
      struct slang_framebuffer *fb = chain->passes[i]->fb_feedback;
      if (fb)
         vulkan_framebuffer_clear(fb->image, cmd);
   }
}

static void slang_chain_set_input_texture(struct vulkan_filter_chain *chain,
      
      const vulkan_filter_chain_texture texture)
{
   chain->input_texture = texture;
}

static void slang_chain_set_frame_count(struct vulkan_filter_chain *chain,
      uint64_t count)
{
   unsigned i;
   chain->common.frame_count = count;
   for (i = 0; i < chain->pass_count; i++)
      slang_pass_set_frame_count(chain->passes[i], count);
}

static void slang_chain_set_frame_count_period(struct vulkan_filter_chain *chain,
      
      unsigned pass, unsigned period)
{
   slang_pass_set_frame_count_period(chain->passes[pass], period);
}

static void slang_chain_set_shader_subframes(struct vulkan_filter_chain *chain,
      uint32_t total_subframes)
{
   chain->common.total_subframes = total_subframes;
}

static void slang_chain_set_current_shader_subframe(struct vulkan_filter_chain *chain,
      uint32_t current_subframe)
{
   chain->common.current_subframe = current_subframe;
}

#ifdef VULKAN_ROLLING_SCANLINE_SIMULATION
static void slang_chain_set_simulate_scanline(struct vulkan_filter_chain *chain,
      bool simulate_scanline)
{
   chain->common.simulate_scanline = simulate_scanline;
}
#endif /* VULKAN_ROLLING_SCANLINE_SIMULATION */

static void slang_chain_set_frame_direction(struct vulkan_filter_chain *chain,
      int32_t direction)
{
   chain->common.frame_direction = direction;
}

static void slang_chain_set_frame_time_delta(struct vulkan_filter_chain *chain,
      uint32_t time_delta)
{
   chain->common.frame_time_delta = time_delta;
}

static void slang_chain_set_original_fps(struct vulkan_filter_chain *chain,
      float fps)
{
   chain->common.original_fps = fps;
}

static void slang_chain_set_rotation(struct vulkan_filter_chain *chain,
      uint32_t rot)
{
   chain->common.rotation = rot;
}

static void slang_chain_set_core_aspect(struct vulkan_filter_chain *chain,
      float coreaspect)
{
   chain->common.core_aspect = coreaspect;
}

static void slang_chain_set_core_aspect_rot(struct vulkan_filter_chain *chain,
      float coreaspectrot)
{
   chain->common.core_aspect_rot = coreaspectrot;
}

#ifdef VULKAN_HDR_SWAPCHAIN
static void slang_chain_set_hdr_mode(struct vulkan_filter_chain *chain,
      unsigned hdr_mode)
{
   chain->common.hdr_mode = hdr_mode;
}

static void slang_chain_set_paper_white_nits(struct vulkan_filter_chain *chain,
      float paper_white_nits)
{
   chain->common.paper_white_nits = paper_white_nits;
}



static void slang_chain_set_expand_gamut(struct vulkan_filter_chain *chain,
      unsigned expand_gamut)
{
   chain->common.expand_gamut = expand_gamut;
}

static void slang_chain_set_scanlines(struct vulkan_filter_chain *chain,
      float scanlines)
{
   chain->common.scanlines = scanlines;
}

static void slang_chain_set_subpixel_layout(struct vulkan_filter_chain *chain,
      unsigned subpixel_layout)
{
   chain->common.subpixel_layout = subpixel_layout;
}

static void slang_chain_set_inverse_tonemap(struct vulkan_filter_chain *chain,
      float inverse_tonemap)
{
   chain->common.inverse_tonemap = inverse_tonemap;
}

static void slang_chain_set_hdr10(struct vulkan_filter_chain *chain,
      float hdr10)
{
   chain->common.hdr10 = hdr10;
}

#endif /* VULKAN_HDR_SWAPCHAIN */

static void slang_chain_set_pass_name(struct vulkan_filter_chain *chain,
      unsigned pass, const char *name)
{
   slang_pass_set_name(chain->passes[pass], name);
}

static bool slang_chain_add_static_texture(struct vulkan_filter_chain *chain,
      
      struct slang_static_texture *texture)
{
   struct slang_static_texture *new_luts = (struct slang_static_texture*)
      realloc(chain->common.luts, (chain->common.num_luts + 1) * sizeof(*new_luts));
   if (!new_luts)
      return false;
   chain->common.luts                        = new_luts;
   chain->common.luts[chain->common.num_luts++]     = *texture;
   memset(texture, 0, sizeof(*texture));
   return true;
}


static bool slang_buffer_init(struct slang_buffer *buf,
      VkDevice device, const VkPhysicalDeviceMemoryProperties *mem_props,
      size_t len, VkBufferUsageFlags usage)
{
   VkBufferCreateInfo info;
   VkMemoryRequirements mem_reqs;
   VkMemoryAllocateInfo alloc;

   buf->device = device;
   buf->size   = len;
   buf->mapped = NULL;

   info.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   info.pNext                 = NULL;
   info.flags                 = 0;
   info.size                  = len;
   info.usage                 = usage;
   info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
   info.queueFamilyIndexCount = 0;
   info.pQueueFamilyIndices   = NULL;
   if (vkCreateBuffer(device, &info, NULL, &buf->buffer) != VK_SUCCESS)
   {
      buf->buffer = VK_NULL_HANDLE;
      buf->memory = VK_NULL_HANDLE;
      return false;
   }

   vkGetBufferMemoryRequirements(device, buf->buffer, &mem_reqs);

   alloc.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.pNext                = NULL;
   alloc.allocationSize       = mem_reqs.size;
   alloc.memoryTypeIndex      = vulkan_find_memory_type(
         mem_props, mem_reqs.memoryTypeBits,
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

   if (vkAllocateMemory(device, &alloc, NULL, &buf->memory) != VK_SUCCESS)
   {
      buf->memory = VK_NULL_HANDLE;
      return false;
   }

   vulkan_debug_mark_memory(device, buf->memory);
   vkBindBufferMemory(device, buf->buffer, buf->memory, 0);
   return true;
}

static void *slang_buffer_map(struct slang_buffer *buf)
{
   if (!buf->mapped)
   {
      if (vkMapMemory(buf->device, buf->memory, 0, buf->size, 0,
               &buf->mapped) != VK_SUCCESS)
         return NULL;
   }
   return buf->mapped;
}

static void slang_buffer_unmap(struct slang_buffer *buf)
{
   if (buf->mapped)
      vkUnmapMemory(buf->device, buf->memory);
   buf->mapped = NULL;
}

static void slang_buffer_free(struct slang_buffer *buf)
{
   if (buf->mapped)
      slang_buffer_unmap(buf);
   if (buf->memory != VK_NULL_HANDLE)
      vkFreeMemory(buf->device, buf->memory, NULL);
   if (buf->buffer != VK_NULL_HANDLE)
      vkDestroyBuffer(buf->device, buf->buffer, NULL);
   buf->buffer = VK_NULL_HANDLE;
   buf->memory = VK_NULL_HANDLE;
}

static void slang_pass_free(struct slang_pass *pass)
{
   size_t i;
   slang_pass_clear_vk(pass);
   slang_framebuffer_delete(&pass->framebuffer);
   slang_framebuffer_delete(&pass->fb_feedback);
   slang_reflection_free(&pass->reflection);
   for (i = 0; i < pass->num_parameters; i++)
      free(pass->parameters[i].id);
   free(pass->parameters);
   free(pass->filtered_parameters);
   free(pass->vertex_shader);
   free(pass->fragment_shader);
   free(pass->push.buffer);
   free(pass->sets);
   free(pass->pass_name);
   free(pass);
}

static bool slang_pass_add_parameter(struct slang_pass *pass,
      unsigned index, const char *id)
{
   struct slang_pass_parameter *new_params = (struct slang_pass_parameter*)
      realloc(pass->parameters, (pass->num_parameters + 1) * sizeof(*new_params));
   if (!new_params)
      return false;
   pass->parameters                                  = new_params;
   if (!(pass->parameters[pass->num_parameters].id = strdup(id)))
      return false;
   pass->parameters[pass->num_parameters].index            = index;
   pass->parameters[pass->num_parameters].semantic_index   = (unsigned)pass->num_parameters;
   pass->num_parameters++;
   return true;
}

static void slang_pass_set_shader(struct slang_pass *pass,
      VkShaderStageFlags stage,
      const uint32_t *spirv,
      size_t spirv_words)
{
   switch (stage)
   {
      case VK_SHADER_STAGE_VERTEX_BIT:
         free(pass->vertex_shader);
         pass->num_vertex_shader = 0;
         if (!(pass->vertex_shader = (uint32_t*)
                  malloc(spirv_words * sizeof(uint32_t))))
            break;
         memcpy(pass->vertex_shader, spirv, spirv_words * sizeof(uint32_t));
         pass->num_vertex_shader = spirv_words;
         break;
      case VK_SHADER_STAGE_FRAGMENT_BIT:
         free(pass->fragment_shader);
         pass->num_fragment_shader = 0;
         if (!(pass->fragment_shader = (uint32_t*)
                  malloc(spirv_words * sizeof(uint32_t))))
            break;
         memcpy(pass->fragment_shader, spirv, spirv_words * sizeof(uint32_t));
         pass->num_fragment_shader = spirv_words;
         break;
      default:
         break;
   }
}

static void slang_pass_get_output_size(struct slang_pass *pass,
      unsigned original_width, unsigned original_height,
      unsigned source_width, unsigned source_height,
      unsigned *out_width, unsigned *out_height)
{
   float width  = 0.0f;
   float height = 0.0f;
   switch (pass->pass_info.scale_type_x)
   {
      case GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL:
         width = (float)(original_width) * pass->pass_info.scale_x;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_SOURCE:
         width = (float)(source_width) * pass->pass_info.scale_x;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT:
         width = (retroarch_get_rotation() % 2 ? pass->curr_vp.height : pass->curr_vp.width) * pass->pass_info.scale_x;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE:
         width = pass->pass_info.scale_x;
         break;

      default:
         break;
   }

   switch (pass->pass_info.scale_type_y)
   {
      case GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL:
         height = (float)(original_height) * pass->pass_info.scale_y;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_SOURCE:
         height = (float)(source_height) * pass->pass_info.scale_y;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT:
         height = (retroarch_get_rotation() % 2 ? pass->curr_vp.width : pass->curr_vp.height) * pass->pass_info.scale_y;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE:
         height = pass->pass_info.scale_y;
         break;

      default:
         break;
   }

   *out_width  = (unsigned)(roundf(width));
   *out_height = (unsigned)(roundf(height));
}

static void slang_pass_set_pass_info(struct slang_pass *pass,
      unsigned max_original_width, unsigned max_original_height,
      unsigned max_source_width, unsigned max_source_height,
      const vulkan_filter_chain_swapchain_info swapchain,
      const vulkan_filter_chain_pass_info info,
      unsigned *out_width, unsigned *out_height)
{
   slang_pass_clear_vk(pass);

   pass->curr_vp                  = swapchain.vp;
   pass->pass_info                = info;

   pass->num_sync_indices         = swapchain.num_indices;
   pass->sync_index               = 0;

   slang_pass_get_output_size(pass,
         max_original_width, max_original_height,
         max_source_width, max_source_height,
         &pass->current_framebuffer_size_width,
         &pass->current_framebuffer_size_height);
   pass->swapchain_render_pass    = swapchain.render_pass;

   *out_width  = pass->current_framebuffer_size_width;
   *out_height = pass->current_framebuffer_size_height;
}

static void slang_pass_clear_vk(struct slang_pass *pass)
{
   if (pass->pool != VK_NULL_HANDLE)
      vkDestroyDescriptorPool(pass->device, pass->pool, NULL);
   if (pass->pipeline != VK_NULL_HANDLE)
      vkDestroyPipeline(pass->device, pass->pipeline, NULL);
   if (pass->set_layout != VK_NULL_HANDLE)
      vkDestroyDescriptorSetLayout(pass->device, pass->set_layout, NULL);
   if (pass->pipeline_layout != VK_NULL_HANDLE)
      vkDestroyPipelineLayout(pass->device, pass->pipeline_layout, NULL);

   pass->pool            = VK_NULL_HANDLE;
   pass->pipeline        = VK_NULL_HANDLE;
   pass->set_layout      = VK_NULL_HANDLE;
   pass->pipeline_layout = VK_NULL_HANDLE;
}

static bool slang_pass_init_pipeline_layout(struct slang_pass *pass)
{
   unsigned i;
   VkPushConstantRange push_range;
   VkDescriptorPoolCreateInfo pool_info;
   VkPipelineLayoutCreateInfo layout_info;
   VkDescriptorSetLayoutBinding *bindings = NULL;
   VkDescriptorPoolSize *desc_counts      = NULL;
   size_t num_bindings                    = 0;
   size_t max_bindings                    = 1; /* the UBO slot */
   VkDescriptorSetLayoutCreateInfo set_layout_info;
   VkDescriptorSetAllocateInfo alloc_info;
   /* Main UBO. */
   VkShaderStageFlags ubo_mask = 0;

   if (pass->reflection.ubo_stage_mask & SLANG_STAGE_VERTEX_MASK)
      ubo_mask |= VK_SHADER_STAGE_VERTEX_BIT;
   if (pass->reflection.ubo_stage_mask & SLANG_STAGE_FRAGMENT_MASK)
      ubo_mask |= VK_SHADER_STAGE_FRAGMENT_BIT;

   /* Upper bound, then fill: the UBO slot plus every semantic-texture
    * entry that is actually referenced. */
   for (i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
      max_bindings += pass->reflection.semantic_textures[i].size;
   if (!(bindings = (VkDescriptorSetLayoutBinding*)
            calloc(max_bindings, sizeof(*bindings))))
      return false;
   if (!(desc_counts = (VkDescriptorPoolSize*)
            calloc(max_bindings, sizeof(*desc_counts))))
   {
      free(bindings);
      return false;
   }

   if (ubo_mask != 0)
   {
      bindings[num_bindings].binding            = pass->reflection.ubo_binding;
      bindings[num_bindings].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      bindings[num_bindings].descriptorCount    = 1;
      bindings[num_bindings].stageFlags         = ubo_mask;
      bindings[num_bindings].pImmutableSamplers = NULL;
      desc_counts[num_bindings].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      desc_counts[num_bindings].descriptorCount = pass->num_sync_indices;
      num_bindings++;
   }

   /* Semantic textures. */
   for (i = 0; i < SLANG_NUM_TEXTURE_SEMANTICS; i++)
   {
      const slang_texture_semantic_array *semantic =
         &pass->reflection.semantic_textures[i];
      size_t ti;
      for (ti = 0; ti < semantic->size; ti++)
      {
         const slang_texture_semantic_meta *texture = &semantic->data[ti];
         VkShaderStageFlags stages = 0;

         if (!texture->texture)
            continue;

         if (texture->stage_mask & SLANG_STAGE_VERTEX_MASK)
            stages |= VK_SHADER_STAGE_VERTEX_BIT;
         if (texture->stage_mask & SLANG_STAGE_FRAGMENT_MASK)
            stages |= VK_SHADER_STAGE_FRAGMENT_BIT;

         bindings[num_bindings].binding            = texture->binding;
         bindings[num_bindings].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
         bindings[num_bindings].descriptorCount    = 1;
         bindings[num_bindings].stageFlags         = stages;
         bindings[num_bindings].pImmutableSamplers = NULL;
         desc_counts[num_bindings].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
         desc_counts[num_bindings].descriptorCount = pass->num_sync_indices;
         num_bindings++;
      }
   }

   set_layout_info.sType                  = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   set_layout_info.pNext                  = NULL;
   set_layout_info.flags                  = 0;
   set_layout_info.bindingCount           = (uint32_t)num_bindings;
   set_layout_info.pBindings              = bindings;

   if (vkCreateDescriptorSetLayout(pass->device,
            &set_layout_info, NULL, &pass->set_layout) != VK_SUCCESS)
   {
      free(bindings);
      free(desc_counts);
      return false;
   }

   layout_info.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   layout_info.pNext                      = NULL;
   layout_info.flags                      = 0;
   layout_info.setLayoutCount             = 1;
   layout_info.pSetLayouts                = &pass->set_layout;
   layout_info.pushConstantRangeCount     = 0;
   layout_info.pPushConstantRanges        = NULL;

   push_range.stageFlags                  = 0;
   push_range.offset                      = 0;
   push_range.size                        = 0;

   /* Push constants */
   if (pass->reflection.push_constant_stage_mask && pass->reflection.push_constant_size)
   {
      if (pass->reflection.push_constant_stage_mask & SLANG_STAGE_VERTEX_MASK)
         push_range.stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
      if (pass->reflection.push_constant_stage_mask & SLANG_STAGE_FRAGMENT_MASK)
         push_range.stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;

#ifdef VULKAN_DEBUG
      RARCH_LOG("[Vulkan] Push Constant Block: %u bytes.\n", (unsigned int)pass->reflection.push_constant_size);
#endif

      layout_info.pushConstantRangeCount = 1;
      layout_info.pPushConstantRanges    = &push_range;

      {
         size_t new_size = (pass->reflection.push_constant_size
               + sizeof(uint32_t) - 1) / sizeof(uint32_t);
         free(pass->push.buffer);
         pass->push.buffer_size = 0;
         if (!(pass->push.buffer = (uint32_t*)calloc(new_size, sizeof(uint32_t))))
         {
            free(bindings);
            free(desc_counts);
            return false;
         }
         pass->push.buffer_size = new_size;
      }
   }

   pass->push.stages     = push_range.stageFlags;
   push_range.size = (uint32_t)pass->reflection.push_constant_size;

   if (vkCreatePipelineLayout(pass->device,
            &layout_info, NULL, &pass->pipeline_layout) != VK_SUCCESS)
   {
      free(bindings);
      free(desc_counts);
      return false;
   }

   pool_info.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   pool_info.pNext                      = NULL;
   pool_info.flags                      = 0;
   pool_info.maxSets                    = pass->num_sync_indices;
   pool_info.poolSizeCount              = (uint32_t)num_bindings;
   pool_info.pPoolSizes                 = desc_counts;
   {
      VkResult res = vkCreateDescriptorPool(pass->device, &pool_info, NULL,
            &pass->pool);
      free(bindings);
      free(desc_counts);
      if (res != VK_SUCCESS)
         return false;
   }

   alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   alloc_info.pNext              = NULL;
   alloc_info.descriptorPool     = pass->pool;
   alloc_info.descriptorSetCount = 1;
   alloc_info.pSetLayouts        = &pass->set_layout;

   free(pass->sets);
   if (!(pass->sets = (VkDescriptorSet*)
            calloc(pass->num_sync_indices, sizeof(*pass->sets))))
      return false;

   for (i = 0; i < pass->num_sync_indices; i++)
   {
      if (vkAllocateDescriptorSets(pass->device, &alloc_info, &pass->sets[i]) != VK_SUCCESS)
         return false;
   }

   return true;
}

static bool slang_pass_init_pipeline(struct slang_pass *pass)
{
   VkGraphicsPipelineCreateInfo pipe;
   VkVertexInputBindingDescription binding;
   VkPipelineDynamicStateCreateInfo dynamic;
   VkPipelineInputAssemblyStateCreateInfo input_assembly;
   VkPipelineVertexInputStateCreateInfo vertex_input;
   VkPipelineRasterizationStateCreateInfo raster;
   VkShaderModuleCreateInfo module_info;
   VkPipelineMultisampleStateCreateInfo multisample;
   VkVertexInputAttributeDescription attributes[2];
   VkPipelineViewportStateCreateInfo vp;
   VkPipelineColorBlendAttachmentState blend_attachment  = {0};
   VkPipelineColorBlendStateCreateInfo blend             = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
   VkPipelineDepthStencilStateCreateInfo depth_stencil   = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
   static const VkDynamicState dynamics[]                = {
      VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
   VkPipelineShaderStageCreateInfo shader_stages[2]      = {
      { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
      { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
   };

   if (!slang_pass_init_pipeline_layout(pass))
      return false;

   /* Input assembly */
   input_assembly.sType                         = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
   input_assembly.pNext                         = NULL;
   input_assembly.flags                         = 0;
   /* Restart on: it only acts on indexed draws, of which the shader
    * chain makes none, and Metal cannot turn it off for strips, so
    * MoltenVK would otherwise warn once per pass on every load. */
   input_assembly.topology                      = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
   input_assembly.primitiveRestartEnable        = VK_TRUE;

   /* VAO state */
   attributes[0].location                       = 0;
   attributes[0].binding                        = 0;
   attributes[0].format                         = VK_FORMAT_R32G32_SFLOAT;
   attributes[0].offset                         = 0;
   attributes[1].location                       = 1;
   attributes[1].binding                        = 0;
   attributes[1].format                         = VK_FORMAT_R32G32_SFLOAT;
   attributes[1].offset                         = 2 * sizeof(float);

   binding.binding                              = 0;
   binding.stride                               = 4 * sizeof(float);
   binding.inputRate                            = VK_VERTEX_INPUT_RATE_VERTEX;

   vertex_input.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
   vertex_input.pNext                           = NULL;
   vertex_input.flags                           = 0;
   vertex_input.vertexBindingDescriptionCount   = 1;
   vertex_input.pVertexBindingDescriptions      = &binding;
   vertex_input.vertexAttributeDescriptionCount = 2;
   vertex_input.pVertexAttributeDescriptions    = attributes;

   /* Raster state */
   raster.sType                                 = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
   raster.pNext                                 = NULL;
   raster.flags                                 = 0;
   raster.depthClampEnable                      = VK_FALSE;
   raster.rasterizerDiscardEnable               = VK_FALSE;
   raster.polygonMode                           = VK_POLYGON_MODE_FILL;
   raster.cullMode                              = VK_CULL_MODE_NONE;
   raster.frontFace                             = VK_FRONT_FACE_COUNTER_CLOCKWISE;
   raster.depthBiasEnable                       = VK_FALSE;
   raster.depthBiasConstantFactor               = 0.0f;
   raster.depthBiasClamp                        = 0.0f;
   raster.depthBiasSlopeFactor                  = 0.0f;
   raster.lineWidth                             = 1.0f;

   /* Blend state */
   blend_attachment.blendEnable                 = VK_FALSE;
   blend_attachment.colorWriteMask              = 0xf;
   blend.attachmentCount                        = 1;
   blend.pAttachments                           = &blend_attachment;

   /* Viewport state */
   vp.sType                                     = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
   vp.pNext                                     = NULL;
   vp.flags                                     = 0;
   vp.viewportCount                             = 1;
   vp.pViewports                                = NULL;
   vp.scissorCount                              = 1;
   vp.pScissors                                 = NULL;

   /* Depth-stencil state */
   depth_stencil.depthTestEnable                = VK_FALSE;
   depth_stencil.depthWriteEnable               = VK_FALSE;
   depth_stencil.depthCompareOp                 = VK_COMPARE_OP_NEVER;
   depth_stencil.depthBoundsTestEnable          = VK_FALSE;
   depth_stencil.stencilTestEnable              = VK_FALSE;
   depth_stencil.minDepthBounds                 = 0.0f;
   depth_stencil.maxDepthBounds                 = 1.0f;

   /* Multisample state */
   multisample.sType                            = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
   multisample.pNext                            = NULL;
   multisample.flags                            = 0;
   multisample.rasterizationSamples             = VK_SAMPLE_COUNT_1_BIT;
   multisample.sampleShadingEnable              = VK_FALSE;
   multisample.minSampleShading                 = 0.0f;
   multisample.pSampleMask                      = NULL;
   multisample.alphaToCoverageEnable            = VK_FALSE;
   multisample.alphaToOneEnable                 = VK_FALSE;

   /* Dynamic state */
   dynamic.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
   dynamic.pNext             = NULL;
   dynamic.flags             = 0;
   dynamic.dynamicStateCount = sizeof(dynamics) / sizeof(dynamics[0]);
   dynamic.pDynamicStates    = dynamics;

   /* Shaders */
   module_info.sType         = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
   module_info.pNext         = NULL;
   module_info.flags         = 0;
   module_info.codeSize      = pass->num_vertex_shader * sizeof(uint32_t);
   module_info.pCode         = pass->vertex_shader;
   shader_stages[0].stage    = VK_SHADER_STAGE_VERTEX_BIT;
   shader_stages[0].pName    = "main";
   if (vkCreateShaderModule(pass->device, &module_info, NULL,
            &shader_stages[0].module) != VK_SUCCESS)
      return false;

   module_info.codeSize      = pass->num_fragment_shader * sizeof(uint32_t);
   module_info.pCode         = pass->fragment_shader;
   shader_stages[1].stage    = VK_SHADER_STAGE_FRAGMENT_BIT;
   shader_stages[1].pName    = "main";
   if (vkCreateShaderModule(pass->device, &module_info, NULL,
            &shader_stages[1].module) != VK_SUCCESS)
   {
      vkDestroyShaderModule(pass->device, shader_stages[0].module, NULL);
      return false;
   }

   pipe.sType                = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
   pipe.pNext                = NULL;
   pipe.flags                = 0;
   pipe.stageCount           = 2;
   pipe.pStages              = shader_stages;
   pipe.pVertexInputState    = &vertex_input;
   pipe.pInputAssemblyState  = &input_assembly;
   pipe.pTessellationState   = NULL;
   pipe.pViewportState       = &vp;
   pipe.pRasterizationState  = &raster;
   pipe.pMultisampleState    = &multisample;
   pipe.pDepthStencilState   = &depth_stencil;
   pipe.pColorBlendState     = &blend;
   pipe.pDynamicState        = &dynamic;
   pipe.layout               = pass->pipeline_layout;
   pipe.renderPass           = pass->final_pass
	   ? pass->swapchain_render_pass
	   : pass->framebuffer->render_pass;
   pipe.subpass              = 0;
   pipe.basePipelineHandle   = VK_NULL_HANDLE;
   pipe.basePipelineIndex    = 0;

   if (vkCreateGraphicsPipelines(pass->device,
            pass->cache, 1, &pipe, NULL, &pass->pipeline) != VK_SUCCESS)
   {
      vkDestroyShaderModule(pass->device, shader_stages[0].module, NULL);
      vkDestroyShaderModule(pass->device, shader_stages[1].module, NULL);
      return false;
   }

   vkDestroyShaderModule(pass->device, shader_stages[0].module, NULL);
   vkDestroyShaderModule(pass->device, shader_stages[1].module, NULL);
   return true;
}

static void common_resources_init(CommonResources *common,
      VkDevice device,
      const VkPhysicalDeviceMemoryProperties *memory_properties)
{
   void *ptr;
   unsigned i;
   VkSamplerCreateInfo info;
   const float vbo_data[]       = {
      /* Offscreen */
      -1.0f, -1.0f, 0.0f, 0.0f,
      -1.0f, +1.0f, 0.0f, 1.0f,
       1.0f, -1.0f, 1.0f, 0.0f,
       1.0f, +1.0f, 1.0f, 1.0f,

       /* Final */
      0.0f,  0.0f, 0.0f, 0.0f,
      0.0f, +1.0f, 0.0f, 1.0f,
      1.0f,  0.0f, 1.0f, 0.0f,
      1.0f, +1.0f, 1.0f, 1.0f,
   };

   memset(common, 0, sizeof(*common));
   common->device          = device;
   common->ubo_alignment   = 1;
   common->frame_direction = 1;
   common->total_subframes = 1;
   common->current_subframe= 1;
   /* The final pass uses an MVP designed for [0, 1] range VBO.
    * For in-between passes, we just go with identity matrices,
    * so keep it simple.
    */


   slang_buffer_init(&common->vbo, common->device,
         memory_properties, sizeof(vbo_data), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

   ptr                          = slang_buffer_map(&common->vbo);
   memcpy(ptr, vbo_data, sizeof(vbo_data));
   slang_buffer_unmap(&common->vbo);

   info.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
   info.pNext                   = NULL;
   info.flags                   = 0;
   info.magFilter               = VK_FILTER_NEAREST;
   info.minFilter               = VK_FILTER_NEAREST;
   info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
   info.addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   info.addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   info.addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   info.mipLodBias              = 0.0f;
   info.anisotropyEnable        = VK_FALSE;
   info.maxAnisotropy           = 1.0f;
   info.compareEnable           = VK_FALSE;
   info.compareOp               = VK_COMPARE_OP_NEVER;
   info.minLod                  = 0.0f;
   info.maxLod                  = VK_LOD_CLAMP_NONE;
   info.borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
   info.unnormalizedCoordinates = VK_FALSE;

   for (i = 0; i < GLSLANG_FILTER_CHAIN_COUNT; i++)
   {
      unsigned j;

      switch ((glslang_filter_chain_filter)i)
      {
         case GLSLANG_FILTER_CHAIN_LINEAR:
            info.magFilter = VK_FILTER_LINEAR;
            info.minFilter = VK_FILTER_LINEAR;
            break;

         case GLSLANG_FILTER_CHAIN_NEAREST:
            info.magFilter = VK_FILTER_NEAREST;
            info.minFilter = VK_FILTER_NEAREST;
            break;

         default:
            break;
      }

      for (j = 0; j < GLSLANG_FILTER_CHAIN_COUNT; j++)
      {
         unsigned k;

         switch ((glslang_filter_chain_filter)j)
         {
            case GLSLANG_FILTER_CHAIN_LINEAR:
               info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
               break;

            case GLSLANG_FILTER_CHAIN_NEAREST:
               info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
               break;

            default:
               break;
         }

         for (k = 0; k < GLSLANG_FILTER_CHAIN_ADDRESS_COUNT; k++)
         {
            VkSamplerAddressMode mode = VK_SAMPLER_ADDRESS_MODE_MAX_ENUM;

            switch ((glslang_filter_chain_address)k)
            {
               case GLSLANG_FILTER_CHAIN_ADDRESS_REPEAT:
                  mode = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                  break;

               case GLSLANG_FILTER_CHAIN_ADDRESS_MIRRORED_REPEAT:
                  mode = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
                  break;

               case GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_EDGE:
                  mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                  break;

               case GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_BORDER:
                  mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                  break;

               case GLSLANG_FILTER_CHAIN_ADDRESS_MIRROR_CLAMP_TO_EDGE:
                  mode = VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE;
                  break;

               default:
                  break;
            }

            info.addressModeU = mode;
            info.addressModeV = mode;
            info.addressModeW = mode;
            if (vkCreateSampler(common->device, &info, NULL,
                     &common->samplers[i][j][k]) != VK_SUCCESS)
               common->samplers[i][j][k] = VK_NULL_HANDLE;
         }
      }
   }
}

static void common_resources_free(CommonResources *common)
{
   slang_texture_semantic_name_map_free(&common->texture_semantic_map);
   slang_texture_semantic_name_map_free(&common->texture_semantic_uniform_map);
   {
      unsigned i, j, k;
      for (i = 0; i < GLSLANG_FILTER_CHAIN_COUNT; i++)
         for (j = 0; j < GLSLANG_FILTER_CHAIN_COUNT; j++)
            for (k = 0; k < GLSLANG_FILTER_CHAIN_ADDRESS_COUNT; k++)
               if (common->samplers[i][j][k] != VK_NULL_HANDLE)
                  vkDestroySampler(common->device,
                        common->samplers[i][j][k], NULL);
   }
   fbpool_drain(&common->framebuffer_pool, common->device);
   slang_buffer_free(&common->vbo);
   slang_buffer_free(&common->ubo);
   {
      size_t i;
      for (i = 0; i < common->num_luts; i++)
         slang_static_texture_free(&common->luts[i]);
   }
   free(common->luts);
   free(common->original_history);
   free(common->fb_feedback);
   free(common->pass_outputs);
   free(common->shader_preset);
}

static void slang_pass_allocate_buffers(struct slang_pass *pass)
{
   if (pass->reflection.ubo_stage_mask)
   {
      /* Align */
      pass->common->ubo_offset = (pass->common->ubo_offset + pass->common->ubo_alignment - 1) &
         ~(pass->common->ubo_alignment - 1);
      pass->ubo_offset = pass->common->ubo_offset;

      /* Allocate */
      pass->common->ubo_offset += pass->reflection.ubo_size;
   }
}

static void slang_pass_end_frame(struct slang_pass *pass)
{
   if (pass->fb_feedback)
   {
      struct slang_framebuffer *tmp = pass->framebuffer;
      pass->framebuffer                   = pass->fb_feedback;
      pass->fb_feedback                   = tmp;
   }
}

static bool slang_pass_init_feedback(struct slang_pass *pass)
{
   if (pass->final_pass)
      return false;

   pass->fb_feedback = slang_framebuffer_new(pass->device, pass->memory_properties,
         pass->current_framebuffer_size_width,
         pass->current_framebuffer_size_height,
         pass->pass_info.rt_format, pass->pass_info.max_levels,
         pass->common ? &pass->common->framebuffer_pool : NULL);
   return pass->fb_feedback != NULL;
}

static bool slang_pass_build(struct slang_pass *pass)
{
   unsigned i;
   unsigned j = 0;
   slang_semantic_name_map semantic_map = { 0 };

   /* The reflection below reads the semantic maps out of common
    * unconditionally, so a pass without one cannot be built at all.
    * slang_chain_set_num_passes() assigns it directly after
    * slang_pass_new(), which is the only way a pass reaches here. */
   if (!pass->common)
      return false;

   slang_framebuffer_delete(&pass->framebuffer);
   slang_framebuffer_delete(&pass->fb_feedback);

   if (!pass->final_pass)
   {
      if (!(pass->framebuffer = slang_framebuffer_new(pass->device, pass->memory_properties,
               pass->current_framebuffer_size_width,
               pass->current_framebuffer_size_height,
               pass->pass_info.rt_format, pass->pass_info.max_levels,
               &pass->common->framebuffer_pool)))
         return false;
   }

   for (i = 0; i < pass->num_parameters; i++)
   {
      if (!slang_semantic_name_map_set_unique(
               &semantic_map, pass->parameters[i].id, NULL,
               SLANG_SEMANTIC_FLOAT_PARAMETER, j))
      {
         slang_semantic_name_map_free(&semantic_map);
         return false;
      }
      j++;
   }

   slang_reflection_free(&pass->reflection);
   if (!slang_reflection_init(&pass->reflection))
   {
      slang_semantic_name_map_free(&semantic_map);
      return false;
   }
   pass->reflection.pass_number                  = pass->pass_number;
   pass->reflection.texture_semantic_map         = &pass->common->texture_semantic_map;
   pass->reflection.texture_semantic_uniform_map = &pass->common->texture_semantic_uniform_map;
   pass->reflection.semantic_map                 = &semantic_map;

   {
      bool refl_ok = slang_reflect_spirv(
            pass->vertex_shader, pass->num_vertex_shader,
            pass->fragment_shader, pass->num_fragment_shader,
            &pass->reflection);
      /* The parameter map is only needed during pass->reflection; the
       * pass->reflection keeps a dangling pointer otherwise. */
      slang_semantic_name_map_free(&semantic_map);
      pass->reflection.semantic_map = NULL;
      if (!refl_ok)
         return false;
   }

   {
      const slang_semantic_meta *g =
         &pass->reflection.semantics[SLANG_SEMANTIC_GYROSCOPE];
      const slang_semantic_meta *a =
         &pass->reflection.semantics[SLANG_SEMANTIC_ACCELEROMETER];
      const slang_semantic_meta *r =
         &pass->reflection.semantics[SLANG_SEMANTIC_ACCELEROMETER_REST];
      if (g->uniform || g->push_constant ||
          a->uniform || a->push_constant ||
          r->uniform || r->push_constant)
         input_state_get_ptr()->shader_uses_sensors = true;
   }

   /* Filter out pass->parameters which we will never use anyways.
    * The stored values index pass->parameters[], so the walk is
    * bounded by both counts: the reflected float parameters decide
    * which entries are live, the parameter count keeps the indices
    * in range. Sizing the allocation off num_parameters alone left
    * the walk free to run past it, and left the array NULL when a
    * pass carries no parameters at all. */
   {
      size_t num_reflected = pass->reflection.num_float_parameters;
      size_t k;

      if (num_reflected > pass->num_parameters)
         num_reflected          = pass->num_parameters;

      pass->num_filtered        = 0;
      free(pass->filtered_parameters);
      pass->filtered_parameters = NULL;
      if (num_reflected &&
            !(pass->filtered_parameters = (size_t*)
               malloc(num_reflected * sizeof(*pass->filtered_parameters))))
         return false;

      for (k = 0; k < num_reflected; k++)
      {
         if (pass->reflection.semantic_float_parameters[k].uniform ||
             pass->reflection.semantic_float_parameters[k].push_constant)
            pass->filtered_parameters[pass->num_filtered++] = k;
      }
   }

   return slang_pass_init_pipeline(pass);
}

static void slang_pass_set_semantic_texture(struct slang_pass *pass,
      VkDescriptorSet set,
      enum slang_texture_semantic semantic, const Texture *texture,
      VkDescriptorImageInfo *image_infos, VkWriteDescriptorSet *writes,
      unsigned *write_count)
{
   if (pass->reflection.semantic_textures[semantic].data[0].texture
         && texture->texture.view != VK_NULL_HANDLE)
   {
      if (*write_count >= VULKAN_MAX_DESCRIPTOR_WRITES)
         vulkan_flush_descriptor_writes(pass->device, writes, write_count);
      VULKAN_PASS_SET_TEXTURE_BATCHED(set, pass->common->samplers[texture->filter][texture->mip_filter][texture->address], pass->reflection.semantic_textures[semantic].data[0].binding, texture->texture.view, texture->texture.layout, image_infos, writes, *write_count);
   }
}

static void slang_pass_set_semantic_texture_array(struct slang_pass *pass,
      VkDescriptorSet set,
      enum slang_texture_semantic semantic, unsigned index,
      const Texture *texture,
      VkDescriptorImageInfo *image_infos, VkWriteDescriptorSet *writes,
      unsigned *write_count)
{
   if (index < pass->reflection.semantic_textures[semantic].size &&
         pass->reflection.semantic_textures[semantic].data[index].texture &&
         texture->texture.view != VK_NULL_HANDLE)
   {
      if (*write_count >= VULKAN_MAX_DESCRIPTOR_WRITES)
         vulkan_flush_descriptor_writes(pass->device, writes, write_count);
      VULKAN_PASS_SET_TEXTURE_BATCHED(set, pass->common->samplers[texture->filter][texture->mip_filter][texture->address],  pass->reflection.semantic_textures[semantic].data[index].binding, texture->texture.view, texture->texture.layout, image_infos, writes, *write_count);
   }
}

static void slang_pass_build_semantic_texture_array_vec4(struct slang_pass *pass,
      uint8_t *data, enum slang_texture_semantic semantic,
      unsigned index, unsigned width, unsigned height)
{
   const slang_texture_semantic_array *arr =
      &pass->reflection.semantic_textures[semantic];
   const slang_texture_semantic_meta *refl;

   if (index >= arr->size)
      return;
   refl = &arr->data[index];

   if (data && refl->uniform)
   {
      float *_data = ((float *)(data + refl->ubo_offset));
      _data[0]     = (float)(width);
      _data[1]     = (float)(height);
      _data[2]     = 1.0f / (float)(width);
      _data[3]     = 1.0f / (float)(height);
   }

   if (refl->push_constant)
   {
      float *_data = ((float *)(pass->push.buffer + (refl->push_constant_offset >> 2)));
      _data[0]     = (float)(width);
      _data[1]     = (float)(height);
      _data[2]     = 1.0f / (float)(width);
      _data[3]     = 1.0f / (float)(height);
   }
}

static void slang_pass_build_semantic_texture_vec4(struct slang_pass *pass,
      uint8_t *data, enum slang_texture_semantic semantic,
      unsigned width, unsigned height)
{
   slang_pass_build_semantic_texture_array_vec4(pass, data, semantic, 0, width, height);
}

static void slang_pass_build_semantic_vec4(struct slang_pass *pass,
      uint8_t *data, enum slang_semantic semantic,
      unsigned width, unsigned height)
{
   const slang_semantic_meta *refl = &pass->reflection.semantics[semantic];

   if (data && refl->uniform)
   {
      float *_data = ((float *)(data + refl->ubo_offset));
      _data[0]     = (float)(width);
      _data[1]     = (float)(height);
      _data[2]     = 1.0f / (float)(width);
      _data[3]     = 1.0f / (float)(height);
   }

   if (refl->push_constant)
   {
      float *_data = (float*)
            (pass->push.buffer + (refl->push_constant_offset >> 2));
      _data[0]     = (float)(width);
      _data[1]     = (float)(height);
      _data[2]     = 1.0f / (float)(width);
      _data[3]     = 1.0f / (float)(height);
   }
}

static void slang_pass_build_semantic_parameter(struct slang_pass *pass,
      uint8_t *data, unsigned index, float value)
{
   const slang_semantic_meta *refl = &pass->reflection.semantic_float_parameters[index];

   /* We will have filtered out stale pass->parameters. */
   if (data && refl->uniform)
      *((float*)(data + refl->ubo_offset)) = value;

   if (refl->push_constant)
      *((float*)(pass->push.buffer + (refl->push_constant_offset >> 2))) = value;
}

static void slang_pass_build_semantic_uint(struct slang_pass *pass,
      uint8_t *data, enum slang_semantic semantic,
      uint32_t value)
{
   const slang_semantic_meta *refl = &pass->reflection.semantics[semantic];

   if (data && refl->uniform)
      *((uint32_t*)(data + pass->reflection.semantics[semantic].ubo_offset)) = value;

   if (refl->push_constant)
      *((uint32_t*)(pass->push.buffer + (refl->push_constant_offset >> 2))) = value;
}

static void slang_pass_build_semantic_int(struct slang_pass *pass,
      uint8_t *data, enum slang_semantic semantic,
                              int32_t value)
{
   const slang_semantic_meta *refl = &pass->reflection.semantics[semantic];

   if (data && refl->uniform)
      *((int32_t*)(data + pass->reflection.semantics[semantic].ubo_offset)) = value;

   if (refl->push_constant)
      *((int32_t*)(pass->push.buffer + (refl->push_constant_offset >> 2))) = value;
}

static void slang_pass_build_semantic_float(struct slang_pass *pass,
      uint8_t *data, enum slang_semantic semantic,
                              float value)
{
   const slang_semantic_meta *refl = &pass->reflection.semantics[semantic];

   if (data && refl->uniform)
      *((float*)(data + pass->reflection.semantics[semantic].ubo_offset)) = value;

   if (refl->push_constant)
      *((float*)(pass->push.buffer + (refl->push_constant_offset >> 2))) = value;
}

static void slang_pass_build_semantic_vec3(struct slang_pass *pass,
      uint8_t *data, enum slang_semantic semantic,
                              const float *values)
{
   const slang_semantic_meta *refl = &pass->reflection.semantics[semantic];

   if (data && refl->uniform)
      memcpy(data + refl->ubo_offset, values, 3 * sizeof(float));

   if (refl->push_constant)
      memcpy(pass->push.buffer + (refl->push_constant_offset >> 2), values, 3 * sizeof(float));
}


static void slang_pass_build_semantic_texture(struct slang_pass *pass,
      VkDescriptorSet set, uint8_t *buffer,
      enum slang_texture_semantic semantic, const Texture *texture,
      VkDescriptorImageInfo *image_infos, VkWriteDescriptorSet *writes,
      unsigned *write_count)
{
   slang_pass_build_semantic_texture_vec4(pass, buffer, semantic,
         texture->texture.width, texture->texture.height);
   slang_pass_set_semantic_texture(pass, set, semantic, texture,
         image_infos, writes, write_count);
}

static void slang_pass_build_semantic_texture_array(struct slang_pass *pass,
      VkDescriptorSet set, uint8_t *buffer,
      enum slang_texture_semantic semantic, unsigned index, const Texture *texture,
      VkDescriptorImageInfo *image_infos, VkWriteDescriptorSet *writes,
      unsigned *write_count)
{
   slang_pass_build_semantic_texture_array_vec4(pass, buffer, semantic, index,
         texture->texture.width, texture->texture.height);
   slang_pass_set_semantic_texture_array(pass, set, semantic, index, texture,
         image_infos, writes, write_count);
}

static void slang_pass_build_semantics(struct slang_pass *pass,
      VkDescriptorSet set, uint8_t *buffer,
      const float *mvp, const Texture *original, const Texture *source)
{
   unsigned i;
   /* Batch arrays for descriptor writes - flushed once at the end. */
   VkDescriptorImageInfo *batch_image_infos = pass->common->batch_image_infos;
   VkWriteDescriptorSet  *batch_writes      = pass->common->batch_writes;
   unsigned               batch_count       = 0;

   /* MVP */
   if (buffer && pass->reflection.semantics[SLANG_SEMANTIC_MVP].uniform)
   {
      size_t offset = pass->reflection.semantics[SLANG_SEMANTIC_MVP].ubo_offset;
      if (mvp)
         memcpy(buffer + offset, mvp, sizeof(float) * 16);
      else
         build_identity_matrix((float*)(buffer + offset));
   }

   if (pass->reflection.semantics[SLANG_SEMANTIC_MVP].push_constant)
   {
      size_t offset = pass->reflection.semantics[SLANG_SEMANTIC_MVP].push_constant_offset;
      if (mvp)
         memcpy(pass->push.buffer + (offset >> 2), mvp, sizeof(float) * 16);
      else
         build_identity_matrix((float*)(pass->push.buffer + (offset >> 2)));
   }

   /* Output information */
   slang_pass_build_semantic_vec4(pass, buffer, SLANG_SEMANTIC_OUTPUT,
                       pass->current_framebuffer_size_width,
                       pass->current_framebuffer_size_height);
   slang_pass_build_semantic_vec4(pass, buffer, SLANG_SEMANTIC_FINAL_VIEWPORT,
                       (unsigned)(pass->curr_vp.width),
                       (unsigned)(pass->curr_vp.height));

   slang_pass_build_semantic_uint(pass, buffer, SLANG_SEMANTIC_FRAME_COUNT,
                       pass->frame_count_period
                       ? (uint32_t)(pass->frame_count % pass->frame_count_period)
                       : (uint32_t)(pass->frame_count));

   slang_pass_build_semantic_int(pass, buffer, SLANG_SEMANTIC_FRAME_DIRECTION,
                      pass->common->frame_direction);

   slang_pass_build_semantic_uint(pass, buffer, SLANG_SEMANTIC_TOTAL_SUBFRAMES,
                      pass->common->total_subframes);

   slang_pass_build_semantic_uint(pass, buffer, SLANG_SEMANTIC_CURRENT_SUBFRAME,
                      pass->common->current_subframe);

   slang_pass_build_semantic_uint(pass, buffer, SLANG_SEMANTIC_FRAME_TIME_DELTA,
                      pass->common->frame_time_delta);

   slang_pass_build_semantic_float(pass, buffer, SLANG_SEMANTIC_ORIGINAL_FPS,
                      pass->common->original_fps);

   slang_pass_build_semantic_uint(pass, buffer, SLANG_SEMANTIC_ROTATION,
                      pass->common->rotation);

   slang_pass_build_semantic_float(pass, buffer, SLANG_SEMANTIC_CORE_ASPECT,
                      pass->common->core_aspect);

   slang_pass_build_semantic_float(pass, buffer, SLANG_SEMANTIC_CORE_ASPECT_ROT,
                      pass->common->core_aspect_rot);

#ifdef VULKAN_HDR_SWAPCHAIN
   slang_pass_build_semantic_uint(pass, buffer, SLANG_SEMANTIC_HDR,
                      pass->common->hdr_mode);

   slang_pass_build_semantic_float(pass, buffer, SLANG_SEMANTIC_PAPER_WHITE_NITS,
                      pass->common->paper_white_nits);

   slang_pass_build_semantic_float(pass, buffer, SLANG_SEMANTIC_SCANLINES,
                      pass->common->scanlines);

   slang_pass_build_semantic_uint(pass, buffer, SLANG_SEMANTIC_SUBPIXEL_LAYOUT,
                      pass->common->subpixel_layout);

   slang_pass_build_semantic_uint(pass, buffer, SLANG_SEMANTIC_EXPAND_GAMUT,
                      pass->common->expand_gamut);

   slang_pass_build_semantic_float(pass, buffer, SLANG_SEMANTIC_INVERSE_TONEMAP,
                      pass->common->inverse_tonemap);

   slang_pass_build_semantic_float(pass, buffer, SLANG_SEMANTIC_HDR10,
                      pass->common->hdr10);
#endif /* VULKAN_HDR_SWAPCHAIN */

   /* Sensor uniforms — per-frame snapshot cached
    * by input_driver_poll() on the main thread */
   {
      input_driver_state_t *input_st = input_state_get_ptr();
      slang_pass_build_semantic_vec3(pass, buffer, SLANG_SEMANTIC_GYROSCOPE,
                        input_st->sensor_gyroscope_cache);
      slang_pass_build_semantic_vec3(pass, buffer, SLANG_SEMANTIC_ACCELEROMETER,
                        input_st->sensor_accelerometer_cache);
      slang_pass_build_semantic_vec3(pass, buffer, SLANG_SEMANTIC_ACCELEROMETER_REST,
                        input_st->sensor_accelerometer_rest);
   }

   /* Standard inputs */
   slang_pass_build_semantic_texture(pass, set, buffer, SLANG_TEXTURE_SEMANTIC_ORIGINAL, original,
         batch_image_infos, batch_writes, &batch_count);
   slang_pass_build_semantic_texture(pass, set, buffer, SLANG_TEXTURE_SEMANTIC_SOURCE, source,
         batch_image_infos, batch_writes, &batch_count);

   /* ORIGINAL_HISTORY[0] is an alias of ORIGINAL. */
   slang_pass_build_semantic_texture_array(pass, set, buffer,
         SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, 0, original,
         batch_image_infos, batch_writes, &batch_count);

   /* Parameters. */
   for (i = 0; i < pass->num_filtered; i++)
      slang_pass_build_semantic_parameter(pass, buffer,
            pass->parameters[pass->filtered_parameters[i]].semantic_index,
            pass->common->shader_preset->parameters[
            pass->parameters[pass->filtered_parameters[i]].index].current);

   /* Previous inputs. */
   for (i = 0; i < pass->common->num_original_history; i++)
      slang_pass_build_semantic_texture_array(pass, set, buffer,
            SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, i + 1,
            &pass->common->original_history[i],
            batch_image_infos, batch_writes, &batch_count);

   /* Previous passes. */
   for (i = 0; i < pass->common->num_pass_outputs; i++)
      slang_pass_build_semantic_texture_array(pass, set, buffer,
            SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, i,
            &pass->common->pass_outputs[i],
            batch_image_infos, batch_writes, &batch_count);

   /* Feedback FBOs. */
   for (i = 0; i < pass->common->num_fb_feedback; i++)
      slang_pass_build_semantic_texture_array(pass, set, buffer,
            SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, i,
            &pass->common->fb_feedback[i],
            batch_image_infos, batch_writes, &batch_count);

   /* LUTs. */
   for (i = 0; i < pass->common->num_luts; i++)
      slang_pass_build_semantic_texture_array(pass, set, buffer,
            SLANG_TEXTURE_SEMANTIC_USER, i,
            &pass->common->luts[i].texture,
            batch_image_infos, batch_writes, &batch_count);

   /* Flush all batched descriptor writes in a single driver call. */
   vulkan_flush_descriptor_writes(pass->device, batch_writes, &batch_count);
}

static void slang_pass_build_commands(struct slang_pass *pass,
      
      struct deferred_disposes *disposer,
      VkCommandBuffer cmd,
      const Texture *original,
      const Texture *source,
      const VkViewport *vp,
      const float *mvp)
{
   uint8_t *u       = NULL;
   VkRect2D sci;
   VkViewport fb_vp;
   unsigned size_width;
   unsigned size_height;
   unsigned size_orig_width;
   unsigned size_orig_height;
   unsigned size_src_width;
   unsigned size_src_height;

   pass->curr_vp          = *vp;
   size_orig_width        = original->texture.width;
   size_orig_height       = original->texture.height;
   size_src_width         = source->texture.width;
   size_src_height        = source->texture.height;
   slang_pass_get_output_size(pass, size_orig_width, size_orig_height,
         size_src_width, size_src_height, &size_width, &size_height);

   if (pass->framebuffer &&
         (size_width  != pass->framebuffer->size_width ||
          size_height != pass->framebuffer->size_height))
   {
      if (!slang_framebuffer_set_size(pass->framebuffer, disposer,
               size_width, size_height, VK_FORMAT_UNDEFINED))
      {
         RARCH_ERR("[Vulkan] Failed to resize shader pass->framebuffer.\n");
         return;
      }
   }

   pass->current_framebuffer_size_width  = size_width;
   pass->current_framebuffer_size_height = size_height;

   if (pass->reflection.ubo_stage_mask && pass->common->ubo_mapped)
      u = pass->common->ubo_mapped + pass->ubo_offset +
         pass->sync_index * pass->common->ubo_sync_index_stride;

   slang_pass_build_semantics(pass, pass->sets[pass->sync_index], u, mvp, original, source);

   if (pass->reflection.ubo_stage_mask)
   {
      VULKAN_SET_UNIFORM_BUFFER(pass->device,
            pass->sets[pass->sync_index],
            pass->reflection.ubo_binding,
            pass->common->ubo.buffer,
            pass->ubo_offset + pass->sync_index * pass->common->ubo_sync_index_stride,
            pass->reflection.ubo_size);
   }

   /* The final pass is always executed inside
    * another render pass since the frontend will
    * want to overlay various things on top for
    * the passes that end up on-screen. */
   if (!pass->final_pass)
   {
      VkRenderPassBeginInfo rp_info;

      /* Render.  The image transitions from UNDEFINED (contents
       * discarded), so no prior work needs to complete — use
       * TOP_OF_PIPE_BIT as the source stage. */
      VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,
            pass->framebuffer->image, 1,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            0,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED);

      rp_info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      rp_info.pNext                    = NULL;
      rp_info.renderPass               = pass->framebuffer->render_pass;
      rp_info.framebuffer              = pass->framebuffer->framebuffer;
      rp_info.renderArea.offset.x      = 0;
      rp_info.renderArea.offset.y      = 0;
      rp_info.renderArea.extent.width  = pass->current_framebuffer_size_width;
      rp_info.renderArea.extent.height = pass->current_framebuffer_size_height;
      rp_info.clearValueCount          = 0;
      rp_info.pClearValues             = NULL;

      vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
   }

   vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pass->pipeline);
   vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
         pass->pipeline_layout,
         0, 1, &pass->sets[pass->sync_index], 0, NULL);

   if (pass->push.stages != 0)
   {
      vkCmdPushConstants(cmd, pass->pipeline_layout,
            pass->push.stages, 0, (uint32_t)pass->reflection.push_constant_size,
            pass->push.buffer);
   }

   {
      VkDeviceSize offset = pass->final_pass ? 16 * sizeof(float) : 0;
      vkCmdBindVertexBuffers(cmd, 0, 1,
            &pass->common->vbo.buffer,
            &offset);
   }

   if (pass->final_pass)
   {
      vkCmdSetViewport(cmd, 0, 1, &pass->curr_vp);

#ifdef VULKAN_ROLLING_SCANLINE_SIMULATION
      if (pass->common->simulate_scanline)
      {
         sci.offset.x      = (int32_t)(pass->curr_vp.x);
         sci.offset.y      = (int32_t)((pass->curr_vp.height / (float)(pass->common->total_subframes))
                                       * (float)(pass->common->current_subframe - 1));
         sci.extent.width  = (uint32_t)(pass->curr_vp.width);
         sci.extent.height = (uint32_t)(pass->curr_vp.height / (float)(pass->common->total_subframes));
      }
      else
#endif /* VULKAN_ROLLING_SCANLINE_SIMULATION */
      {
         sci.offset.x      = (int32_t)(pass->curr_vp.x);
         sci.offset.y      = (int32_t)(pass->curr_vp.y);
         sci.extent.width  = (uint32_t)(pass->curr_vp.width);
         sci.extent.height = (uint32_t)(pass->curr_vp.height);
      }
      vkCmdSetScissor(cmd, 0, 1, &sci);
   }
   else
   {
      fb_vp.x        = 0.0f;
      fb_vp.y        = 0.0f;
      fb_vp.width    = (float)(pass->current_framebuffer_size_width);
      fb_vp.height   = (float)(pass->current_framebuffer_size_height);
      fb_vp.minDepth = 0.0f;
      fb_vp.maxDepth = 1.0f;

      vkCmdSetViewport(cmd, 0, 1, &fb_vp);

#ifdef VULKAN_ROLLING_SCANLINE_SIMULATION
      if (pass->common->simulate_scanline)
      {
         sci.offset.x      = 0;
         sci.offset.y      = (int32_t)(((float)(pass->current_framebuffer_size_height) / (float)(pass->common->total_subframes))
                                       * (float)(pass->common->current_subframe - 1));
         sci.extent.width  = (uint32_t)(pass->current_framebuffer_size_width);
         sci.extent.height = (uint32_t)((float)(pass->current_framebuffer_size_height) / (float)(pass->common->total_subframes));
      }
      else
#endif /* VULKAN_ROLLING_SCANLINE_SIMULATION */
      {
         sci.offset.x      = 0;
         sci.offset.y      = 0;
         sci.extent.width  = pass->current_framebuffer_size_width;
         sci.extent.height = pass->current_framebuffer_size_height;
      }
      vkCmdSetScissor(cmd, 0, 1, &sci);
   }

   vkCmdDraw(cmd, 4, 1, 0, 0);

   if (!pass->final_pass)
   {
      vkCmdEndRenderPass(cmd);

      if (pass->framebuffer->levels > 1)
         vulkan_framebuffer_generate_mips(
               pass->framebuffer->framebuffer,
               pass->framebuffer->image,
               pass->framebuffer->size_width,
               pass->framebuffer->size_height,
               cmd,
               pass->framebuffer->levels);
      else
      {
         /* Barrier to sync with next pass. */
         VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(
               cmd,
               pass->framebuffer->image,
               VK_REMAINING_MIP_LEVELS,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
               VK_ACCESS_SHADER_READ_BIT,
               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
               VK_QUEUE_FAMILY_IGNORED,
               VK_QUEUE_FAMILY_IGNORED);
      }
   }
}

static struct slang_framebuffer *slang_framebuffer_new(VkDevice device,
      const VkPhysicalDeviceMemoryProperties *mem_props,
      unsigned max_width, unsigned max_height,
      VkFormat format, unsigned max_levels,
      FramebufferMemoryPool *mem_pool)
{
   struct slang_framebuffer *fb = (struct slang_framebuffer*)
      calloc(1, sizeof(*fb));
   if (!fb)
      return NULL;
   fb->size_width        = max_width;
   fb->size_height       = max_height;
   fb->format            = format;
   fb->max_levels        = MAX(max_levels, 1u);
   fb->memory_properties = mem_props;
   fb->device            = device;
   fb->mem_pool          = mem_pool;

   RARCH_LOG("[Vulkan] Creating framebuffer %ux%u (max %u level(s)).\n",
         max_width, max_height, max_levels);
   if (!vulkan_initialize_render_pass(device, format, &fb->render_pass))
   {
      RARCH_ERR("[Vulkan] Failed to create render pass for "
            "framebuffer %ux%u.\n", max_width, max_height);
      free(fb);
      return NULL;
   }
   if (!slang_framebuffer_build(fb))
   {
      RARCH_ERR("[Vulkan] Failed to create framebuffer %ux%u.\n",
            max_width, max_height);
      slang_framebuffer_free(fb);
      free(fb);
      return NULL;
   }
   return fb;
}

static void slang_framebuffer_delete(struct slang_framebuffer **fb)
{
   if (*fb)
   {
      slang_framebuffer_free(*fb);
      free(*fb);
      *fb = NULL;
   }
}

static bool slang_framebuffer_build(struct slang_framebuffer *fb)
{
   VkFramebufferCreateInfo fb_info;
   VkMemoryRequirements mem_reqs;
   VkImageCreateInfo info;
   VkMemoryAllocateInfo alloc;
   VkImageViewCreateInfo view_info;
   VkImage new_image             = VK_NULL_HANDLE;
   VkImageView new_view          = VK_NULL_HANDLE;
   VkImageView new_fb_view       = VK_NULL_HANDLE;
   VkFramebuffer new_framebuffer = VK_NULL_HANDLE;
   VkDeviceMemory new_memory     = VK_NULL_HANDLE;
   uint32_t new_memory_type;
   size_t new_memory_size        = 0;
   unsigned new_levels;
   size_t _y                = glslang_num_miplevels(fb->size_width, fb->size_height);

   info.sType               = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   info.pNext               = NULL;
   info.flags               = 0;
   info.imageType           = VK_IMAGE_TYPE_2D;
   info.format              = fb->format;
   info.extent.width        = fb->size_width;
   info.extent.height       = fb->size_height;
   info.extent.depth        = 1;
   info.mipLevels           = (uint32_t)MIN(fb->max_levels, _y);
   info.arrayLayers         = 1;
   info.samples             = VK_SAMPLE_COUNT_1_BIT;
   info.tiling              = VK_IMAGE_TILING_OPTIMAL;
   info.usage               = VK_IMAGE_USAGE_SAMPLED_BIT
                            | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                            | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
   info.sharingMode         = VK_SHARING_MODE_EXCLUSIVE;
   info.queueFamilyIndexCount = 0;
   info.pQueueFamilyIndices = NULL;
   info.initialLayout       = VK_IMAGE_LAYOUT_UNDEFINED;
   new_levels               = info.mipLevels;

   if (!info.extent.width || !info.extent.height || !new_levels
         || fb->format == VK_FORMAT_UNDEFINED)
   {
      RARCH_ERR("[Vulkan] Refusing invalid fb->framebuffer fb->image "
            "(%ux%u, fb->format %u, fb->levels %u).\n",
            info.extent.width, info.extent.height,
            (unsigned)fb->format, new_levels);
      return false;
   }

   if (vkCreateImage(fb->device, &info, NULL, &new_image) != VK_SUCCESS)
      goto error;
   vulkan_debug_mark_image(fb->device, new_image);

   vkGetImageMemoryRequirements(fb->device, new_image, &mem_reqs);

   alloc.sType            = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.pNext            = NULL;
   alloc.allocationSize   = mem_reqs.size;
   new_memory_type       = find_memory_type_fallback(
         fb->memory_properties, mem_reqs.memoryTypeBits,
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
   alloc.memoryTypeIndex  = new_memory_type;

   /* The old fb->image can still be in flight, so we can never reuse this
    * fb->framebuffer's own live allocation here (that aliases two live
    * optimal-tiled images, which is invalid on Mali). We can, however,
    * reuse a block that was already retired to the chain-scoped pool: by
    * construction its previous fb->image has been destroyed and drained. */
   if (fb->mem_pool)
   {
      struct fbpool_block blk =
         fbpool_acquire(fb->mem_pool, mem_reqs.size, new_memory_type);
      if (blk.memory != VK_NULL_HANDLE)
      {
         new_memory      = blk.memory;
         new_memory_size = blk.size;
      }
   }

   if (new_memory == VK_NULL_HANDLE)
   {
      if (vkAllocateMemory(fb->device, &alloc, NULL, &new_memory) != VK_SUCCESS)
         goto error;
      new_memory_size = mem_reqs.size;
      vulkan_debug_mark_memory(fb->device, new_memory);
   }

   if (vkBindImageMemory(fb->device, new_image, new_memory, 0) != VK_SUCCESS)
      goto error;

   view_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view_info.pNext                           = NULL;
   view_info.flags                           = 0;
   view_info.image                           = new_image;
   view_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
   view_info.format                          = fb->format;
   view_info.components.r                    = VK_COMPONENT_SWIZZLE_R;
   view_info.components.g                    = VK_COMPONENT_SWIZZLE_G;
   view_info.components.b                    = VK_COMPONENT_SWIZZLE_B;
   view_info.components.a                    = VK_COMPONENT_SWIZZLE_A;
   view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   view_info.subresourceRange.baseMipLevel   = 0;
   view_info.subresourceRange.levelCount     = new_levels;
   view_info.subresourceRange.baseArrayLayer = 0;
   view_info.subresourceRange.layerCount     = 1;

   if (vkCreateImageView(fb->device, &view_info, NULL, &new_view) != VK_SUCCESS)
      goto error;
   view_info.subresourceRange.levelCount     = 1;
   if (vkCreateImageView(fb->device, &view_info, NULL, &new_fb_view) != VK_SUCCESS)
      goto error;

   /* Initialize fb->framebuffer */
   fb_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
   fb_info.pNext           = NULL;
   fb_info.flags           = 0;
   fb_info.renderPass      = fb->render_pass;
   fb_info.attachmentCount = 1;
   fb_info.pAttachments    = &new_fb_view;
   fb_info.width           = fb->size_width;
   fb_info.height          = fb->size_height;
   fb_info.layers          = 1;

   if (vkCreateFramebuffer(fb->device, &fb_info, NULL,
            &new_framebuffer) != VK_SUCCESS)
      goto error;

   fb->image         = new_image;
   fb->view          = new_view;
   fb->fb_view       = new_fb_view;
   fb->framebuffer   = new_framebuffer;
   fb->levels        = new_levels;
   fb->memory.type   = new_memory_type;
   fb->memory.size   = new_memory_size;
   fb->memory.memory = new_memory;
   return true;

error:
   if (new_framebuffer != VK_NULL_HANDLE)
      vkDestroyFramebuffer(fb->device, new_framebuffer, NULL);
   if (new_view != VK_NULL_HANDLE)
      vkDestroyImageView(fb->device, new_view, NULL);
   if (new_fb_view != VK_NULL_HANDLE)
      vkDestroyImageView(fb->device, new_fb_view, NULL);
   if (new_image != VK_NULL_HANDLE)
      vkDestroyImage(fb->device, new_image, NULL);
   if (new_memory != VK_NULL_HANDLE)
      vkFreeMemory(fb->device, new_memory, NULL);
   return false;
}

static bool slang_framebuffer_set_size(struct slang_framebuffer *fb,
      struct deferred_disposes *disposer,
      unsigned width, unsigned height,
      VkFormat format)
{
   unsigned old_size_width       = fb->size_width;
   unsigned old_size_height      = fb->size_height;
   VkFormat old_format           = fb->format;
   VkRenderPass old_render_pass  = fb->render_pass;
   VkImage old_image             = fb->image;
   VkImageView old_view          = fb->view;
   VkImageView old_fb_view       = fb->fb_view;
   VkFramebuffer old_framebuffer = fb->framebuffer;
   VkDeviceMemory old_memory     = fb->memory.memory;
   size_t old_memory_size        = fb->memory.size;
   uint32_t old_memory_type      = fb->memory.type;
   VkFormat new_format           = format == VK_FORMAT_UNDEFINED ? old_format : format;
   bool format_changed           = new_format != old_format;

   fb->size_width              = width;
   fb->size_height             = height;
   fb->format                  = new_format;

   RARCH_LOG("[Vulkan] Updating fb->framebuffer size %ux%u (format: %u).\n",
         width, height, (unsigned)fb->format);

   if (format_changed)
   {
      if (!vulkan_initialize_render_pass(fb->device, fb->format, &fb->render_pass))
      {
         fb->size_width  = old_size_width;
         fb->size_height = old_size_height;
         fb->format = old_format;
         fb->render_pass  = old_render_pass;
         return false;
      }
   }

   if (!slang_framebuffer_build(fb))
   {
      if (format_changed)
         vkDestroyRenderPass(fb->device, fb->render_pass, NULL);
      fb->size_width  = old_size_width;
      fb->size_height = old_size_height;
      fb->format = old_format;
      fb->render_pass  = old_render_pass;
      return false;
   }

   /* The replaced resources can still be referenced by an in-flight
    * command buffer. Defer their destruction together, including fb->memory. */
   {
      /* The pool pointer is copied into the record: the record must not
       * reference the framebuffer object, since it may be destroyed before
       * the queue drains. The pool is chain-scoped and outlives every
       * drain point. */
      struct deferred_fb_dispose call;
      call.device      = fb->device;
      call.pool        = fb->mem_pool;
      call.framebuffer = old_framebuffer;
      call.view        = old_view;
      call.fb_view     = old_fb_view;
      call.image       = old_image;
      call.memory      = old_memory;
      call.memory_size = old_memory_size;
      call.memory_type = old_memory_type;
      call.render_pass = format_changed ? old_render_pass : VK_NULL_HANDLE;
      /* On queue failure the resources must NOT be destroyed here - an
       * in-flight command buffer can still reference them; leaking is
       * the safe direction. (The vector terminated the process here.) */
      if (!deferred_disposes_push(disposer, &call))
         RARCH_ERR("[Vulkan] Failed to defer fb->framebuffer disposal, leaking replaced resources.\n");
   }

   return true;
}

static void slang_framebuffer_free(struct slang_framebuffer *fb)
{
   if (fb->framebuffer != VK_NULL_HANDLE)
      vkDestroyFramebuffer(fb->device, fb->framebuffer, NULL);
   if (fb->render_pass != VK_NULL_HANDLE)
      vkDestroyRenderPass(fb->device, fb->render_pass, NULL);
   if (fb->view != VK_NULL_HANDLE)
      vkDestroyImageView(fb->device, fb->view, NULL);
   if (fb->fb_view != VK_NULL_HANDLE)
      vkDestroyImageView(fb->device, fb->fb_view, NULL);
   if (fb->image != VK_NULL_HANDLE)
      vkDestroyImage(fb->device, fb->image, NULL);
   if (fb->memory.memory != VK_NULL_HANDLE)
      vkFreeMemory(fb->device, fb->memory.memory, NULL);
   fb->framebuffer   = VK_NULL_HANDLE;
   fb->render_pass   = VK_NULL_HANDLE;
   fb->view          = VK_NULL_HANDLE;
   fb->fb_view       = VK_NULL_HANDLE;
   fb->image         = VK_NULL_HANDLE;
   fb->memory.memory = VK_NULL_HANDLE;
}

/* C glue */
vulkan_filter_chain_t *vulkan_filter_chain_new(
      const vulkan_filter_chain_create_info *info)
{
   return slang_chain_new(info);
}

vulkan_filter_chain_t *vulkan_filter_chain_create_default(
      const struct vulkan_filter_chain_create_info *info,
      glslang_filter_chain_filter filter)
{
   struct vulkan_filter_chain_pass_info pass_info;
   struct vulkan_filter_chain_create_info tmpinfo = *info;
   vulkan_filter_chain *chain;

   tmpinfo.num_passes      = 1;

   chain = slang_chain_new(&tmpinfo);
   if (!chain)
      return NULL;
   RARCH_DBG("[Vulkan] Stock chain allocated.\n");

   pass_info.scale_type_x  = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
   pass_info.scale_type_y  = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
   pass_info.scale_x       = 1.0f;
   pass_info.scale_y       = 1.0f;
   pass_info.rt_format     = tmpinfo.swapchain.format;
   pass_info.source_filter = filter;
   pass_info.mip_filter    = GLSLANG_FILTER_CHAIN_NEAREST;
   pass_info.address       = GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_EDGE;
   pass_info.max_levels    = 0;

   slang_chain_set_pass_info(chain, 0, pass_info);

   slang_chain_set_shader(chain, 0, VK_SHADER_STAGE_VERTEX_BIT,
         opaque_vert,
         sizeof(opaque_vert) / sizeof(uint32_t));

#ifdef VULKAN_HDR_SWAPCHAIN
   if (info->hdr_enabled)
   {
      slang_chain_set_shader(chain, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
            hdr_frag,
            sizeof(hdr_frag) / sizeof(uint32_t));
   }
   else
#endif /* VULKAN_HDR_SWAPCHAIN */ 
   {
      slang_chain_set_shader(chain, 0, VK_SHADER_STAGE_FRAGMENT_BIT,
            opaque_frag,
            sizeof(opaque_frag) / sizeof(uint32_t));
   }

   if (!slang_chain_init(chain))
   {
      slang_chain_free(chain);
      return NULL;
   }

   RARCH_DBG("[Vulkan] Stock chain built.\n");
   return chain;
}

vulkan_filter_chain_t *vulkan_filter_chain_create_from_preset(
      const struct vulkan_filter_chain_create_info *info,
      const char *path, glslang_filter_chain_filter filter)
{
   unsigned i;
   bool last_pass_is_fbo;
   glslang_output output;
   struct vulkan_filter_chain_create_info tmpinfo;
   void *include_cache = NULL;
   vulkan_filter_chain *chain = NULL;
   struct video_shader *shader = (struct video_shader*)
      calloc(1, sizeof(*shader));

   glslang_output_init(&output);

   if (!shader)
      return NULL;

    if (!video_shader_load_preset_into_shader(path, shader))
        goto error;

   last_pass_is_fbo   = shader->pass[shader->passes - 1].fbo.flags &
      FBO_SCALE_FLAG_VALID;
   tmpinfo            = *info;
   tmpinfo.num_passes = shader->passes + (last_pass_is_fbo ? 1 : 0);

   chain = slang_chain_new(&tmpinfo);
   if (!chain)
      goto error;

   if (shader->luts && !vulkan_filter_chain_load_luts(info, chain, shader))
      goto error;

   shader->num_parameters = 0;

   /* One include cache for every pass of this preset.  The passes share
    * helper .inc files, so without this each pass re-reads them: a
    * 24-pass preset over 8 shared helpers issues 216 reads for 32
    * distinct files.  The guard frees it on every exit from here,
    * including the error paths below. */
   {
   include_cache = glslang_include_cache_new();

   for (i = 0; i < shader->passes; i++)
   {
      struct vulkan_filter_chain_pass_info pass_info;
      bool explicit_format;
      const struct video_shader_pass *pass      = &shader->pass[i];
      const struct video_shader_pass *next_pass =
         i + 1 < shader->passes ? &shader->pass[i + 1] : NULL;

      pass_info.scale_type_x  = GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL;
      pass_info.scale_type_y  = GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL;
      pass_info.scale_x       = 0.0f;
      pass_info.scale_y       = 0.0f;
      pass_info.rt_format     = VK_FORMAT_UNDEFINED;
      pass_info.source_filter = GLSLANG_FILTER_CHAIN_LINEAR;
      pass_info.mip_filter    = GLSLANG_FILTER_CHAIN_LINEAR;
      pass_info.address       = GLSLANG_FILTER_CHAIN_ADDRESS_REPEAT;
      pass_info.max_levels    = 0;

      if (!glslang_compile_shader_cached(pass->source.path, &output,
               include_cache))
      {
         RARCH_ERR("[Vulkan] Failed to compile shader: \"%s\".\n",
               pass->source.path);
         goto error;
      }

      {
      size_t j;
      for (j = 0; j < output.meta.num_parameters; j++)
      {
         unsigned k;
         const glslang_parameter *meta_param = &output.meta.parameters[j];
         struct video_shader_parameter *itr = NULL;

         if (shader->num_parameters >= GFX_MAX_PARAMETERS)
         {
            RARCH_ERR("[Vulkan] Exceeded maximum number of parameters (%u).\n", GFX_MAX_PARAMETERS);
            goto error;
         }

         /* Find existing parameter with matching id.  Gated memcmp:
          * O(n^2) across Mega Bezel-scale parameter counts. */
         {
            size_t mid_len = strlen(meta_param->id);
            for (k = 0; k < shader->num_parameters; k++)
            {
               const char *sid = shader->parameters[k].id;
               if (sid[0] == meta_param->id[0] && sid[mid_len] == '\0'
                     && !memcmp(sid, meta_param->id, mid_len))
               {
                  itr = &shader->parameters[k];
                  break;
               }
            }
         }

         if (itr)
         {
            /* Allow duplicate #pragma parameter, but
             * only if they are exactly the same. */
            if (   strcmp(meta_param->desc, itr->desc)
                || meta_param->initial != itr->initial
                || meta_param->minimum != itr->minimum
                || meta_param->maximum != itr->maximum
                || meta_param->step    != itr->step)
            {
               RARCH_ERR("[Vulkan] Duplicate parameters found for \"%s\", but arguments do not match.\n",
                     itr->id);
               goto error;
            }
            slang_chain_add_parameter(chain, i, (unsigned)(itr - shader->parameters), meta_param->id);
         }
         else
         {
            struct video_shader_parameter *param = &shader->parameters[shader->num_parameters];
            strlcpy(param->id, meta_param->id, sizeof(param->id));
            strlcpy(param->desc, meta_param->desc, sizeof(param->desc));
            param->initial = meta_param->initial;
            param->minimum = meta_param->minimum;
            param->maximum = meta_param->maximum;
            param->step    = meta_param->step;
            slang_chain_add_parameter(chain, i, shader->num_parameters, meta_param->id);
            shader->num_parameters++;
         }
      }
      }

      slang_chain_set_shader(chain, i,
            VK_SHADER_STAGE_VERTEX_BIT,
            output.vertex,
            output.vertex_len);

      slang_chain_set_shader(chain, i,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            output.fragment,
            output.fragment_len);

      slang_chain_set_frame_count_period(chain, i, pass->frame_count_mod);

      if (output.meta.name[0])
         slang_chain_set_pass_name(chain, i, output.meta.name);

      /* Preset overrides. */
      if (*pass->alias)
         slang_chain_set_pass_name(chain, i, pass->alias);

      if (pass->filter == RARCH_FILTER_UNSPEC)
         pass_info.source_filter = filter;
      else
      {
         pass_info.source_filter =
            pass->filter == RARCH_FILTER_LINEAR
            ? GLSLANG_FILTER_CHAIN_LINEAR
            : GLSLANG_FILTER_CHAIN_NEAREST;
      }
      pass_info.address    = rarch_wrap_to_address(pass->wrap);
      pass_info.max_levels = 1;

      /* TODO: Expose max_levels in slangp.
       * Preset format is a bit awkward in that it uses mipmap_input,
       * so we must check if next pass needs the mipmapping.
       */
      if (next_pass && next_pass->mipmap)
         pass_info.max_levels = ~0u;

      pass_info.mip_filter =
         (pass->filter != RARCH_FILTER_NEAREST && pass_info.max_levels > 1)
         ? GLSLANG_FILTER_CHAIN_LINEAR
         : GLSLANG_FILTER_CHAIN_NEAREST;

      explicit_format              = output.meta.rt_format != SLANG_FORMAT_UNKNOWN;

      /* Set a reasonable default. */
      if (output.meta.rt_format == SLANG_FORMAT_UNKNOWN)
         output.meta.rt_format     = SLANG_FORMAT_R8G8B8A8_UNORM;

      if (!(pass->fbo.flags & FBO_SCALE_FLAG_VALID))
      {
         pass_info.scale_type_x    = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
         pass_info.scale_type_y    = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
         pass_info.scale_x         = 1.0f;
         pass_info.scale_y         = 1.0f;

         if (i + 1 == shader->passes)
         {
            VkFormat pass_format;
            pass_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
            pass_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;

            pass_format = glslang_format_to_vk(output.meta.rt_format);

            /* If final pass explicitly emits RGB10, consider it HDR color space. */
            if (explicit_format && vulkan_is_hdr10_format(pass_format))
               slang_chain_set_emits_hdr10(chain);

#ifdef VULKAN_HDR_SWAPCHAIN
            /* If the final pass explicitly requests RGBA16F, flag it so
             * the driver knows this shader handles HDR conversion and
             * can skip the internal HDR pipeline (passthrough mode).
             * The final pass renders directly to the swapchain, so
             * rt_format inherits the swapchain format — the hardware
             * handles any quantisation (e.g. float → 10-bit for PQ). */
            if (explicit_format && pass_format == VK_FORMAT_R16G16B16A16_SFLOAT)
               slang_chain_set_emits_hdr16(chain);
#endif

            /* Inherit swapchain format. */
            pass_info.rt_format = tmpinfo.swapchain.format;

            if (explicit_format && pass_format != pass_info.rt_format)
            {
               RARCH_WARN("[Vulkan] Using explicit format for last pass in chain,"
                        " but it is not rendered to framebuffer, using swapchain format instead.\n");
            }
         }
         else
         {
            pass_info.rt_format    = glslang_format_to_vk(
                  output.meta.rt_format);
            RARCH_LOG("[Vulkan] Using render target format %s for pass output #%u.\n",
                  glslang_format_to_string(output.meta.rt_format), i);
         }
      }
      else
      {
         /* Preset overrides shader.
          * Kinda ugly ... */
         if (pass->fbo.flags & FBO_SCALE_FLAG_SRGB_FBO)
            output.meta.rt_format = SLANG_FORMAT_R8G8B8A8_SRGB;
         else if (pass->fbo.flags & FBO_SCALE_FLAG_FP_FBO)
            output.meta.rt_format = SLANG_FORMAT_R16G16B16A16_SFLOAT;
         else if (pass->fbo.flags & FBO_SCALE_FLAG_RGB10_FBO)
            output.meta.rt_format = SLANG_FORMAT_A2B10G10R10_UNORM_PACK32;

         pass_info.rt_format      = glslang_format_to_vk(output.meta.rt_format);

         RARCH_LOG("[Vulkan] Using render target format %s for pass output #%u.\n",
               glslang_format_to_string(output.meta.rt_format), i);

#ifdef VULKAN_HDR_SWAPCHAIN
         /* If the final pass explicitly emits an HDR10 or RGBA16F
          * format via #pragma format, flag it so the driver skips its
          * own inverse-tonemap / HDR10 conversion (passthrough mode).
          * Without this, the hidden copy pass added for scale_type
          * would re-apply HDR processing on already-HDR content. */
         if (i + 1 == shader->passes)
         {
            if (explicit_format && vulkan_is_hdr10_format(pass_info.rt_format))
               slang_chain_set_emits_hdr10(chain);
            else if (explicit_format && pass_info.rt_format == VK_FORMAT_R16G16B16A16_SFLOAT)
               slang_chain_set_emits_hdr16(chain);
         }
#endif /* VULKAN_HDR_SWAPCHAIN */

         switch (pass->fbo.type_x)
         {
            case RARCH_SCALE_INPUT:
               pass_info.scale_x      = pass->fbo.scale_x;
               pass_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
               break;

            case RARCH_SCALE_ABSOLUTE:
               pass_info.scale_x      = (float)(pass->fbo.abs_x);
               pass_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE;
               break;

            case RARCH_SCALE_VIEWPORT:
               pass_info.scale_x      = pass->fbo.scale_x;
               pass_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
               break;
         }

         switch (pass->fbo.type_y)
         {
            case RARCH_SCALE_INPUT:
               pass_info.scale_y      = pass->fbo.scale_y;
               pass_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
               break;

            case RARCH_SCALE_ABSOLUTE:
               pass_info.scale_y      = (float)(pass->fbo.abs_y);
               pass_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE;
               break;

            case RARCH_SCALE_VIEWPORT:
               pass_info.scale_y      = pass->fbo.scale_y;
               pass_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
               break;
         }
      }

      slang_chain_set_pass_info(chain, i, pass_info);
      glslang_output_free(&output);
   }
   glslang_include_cache_free(include_cache);
   include_cache = NULL;
   }   /* include cache scope: freed just above, and on any error exit */

   if (last_pass_is_fbo)
   {
      struct vulkan_filter_chain_pass_info pass_info;

      pass_info.scale_type_x  = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
      pass_info.scale_type_y  = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
      pass_info.scale_x       = 1.0f;
      pass_info.scale_y       = 1.0f;

      pass_info.rt_format     = tmpinfo.swapchain.format;

      pass_info.source_filter = filter;
      pass_info.mip_filter    = GLSLANG_FILTER_CHAIN_NEAREST;
      pass_info.address       = GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_EDGE;

      pass_info.max_levels    = 0;

      slang_chain_set_pass_info(chain, shader->passes, pass_info);

      slang_chain_set_shader(chain, shader->passes,
            VK_SHADER_STAGE_VERTEX_BIT,
            opaque_vert,
            sizeof(opaque_vert) / sizeof(uint32_t));

      /* The hidden copy/scale pass is always a plain blit.
       * HDR conversion is handled by the HDR pipeline, not here. */
      slang_chain_set_shader(chain, shader->passes,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            opaque_frag,
            sizeof(opaque_frag) / sizeof(uint32_t));
   }

   slang_chain_set_shader_preset(chain, shader);
   shader = NULL; /* owned by the chain now */

   if (!slang_chain_init(chain))
      goto error;

   glslang_output_free(&output);
   return chain;

error:
   glslang_include_cache_free(include_cache);
   slang_chain_free(chain);
   free(shader);
   glslang_output_free(&output);
   return NULL;
}

/* ---- Deferred (per-frame) Vulkan filter chain construction ---- */

vulkan_filter_chain_t *vulkan_filter_chain_create_deferred(
      const struct vulkan_filter_chain_create_info *info,
      const char *path,
      glslang_filter_chain_filter filter,
      unsigned *out_num_passes)
{
   unsigned i;
   bool last_pass_is_fbo;
   struct vulkan_filter_chain_create_info tmpinfo;
   vulkan_filter_chain *chain = NULL;
   struct video_shader *shader = (struct video_shader*)
      calloc(1, sizeof(*shader));

   if (!shader)
      return NULL;

   if (!video_shader_load_preset_into_shader(path, shader))
   {
      free(shader);
      return NULL;
   }

   last_pass_is_fbo   = shader->pass[shader->passes - 1].fbo.flags &
      FBO_SCALE_FLAG_VALID;
   tmpinfo            = *info;
   tmpinfo.num_passes = shader->passes + (last_pass_is_fbo ? 1 : 0);

   chain = slang_chain_new(&tmpinfo);
   if (!chain)
   {
      free(shader);
      return NULL;
   }

   if (shader->luts && !vulkan_filter_chain_load_luts(info, chain, shader))
   {
      slang_chain_free(chain);
      free(shader);
      return NULL;
   }

   shader->num_parameters = 0;

   /* Set pass names from preset aliases only */
   for (i = 0; i < shader->passes; i++)
   {
      if (*shader->pass[i].alias)
         slang_chain_set_pass_name(chain, i, shader->pass[i].alias);
   }

   if (last_pass_is_fbo)
   {
      struct vulkan_filter_chain_pass_info pass_info;

      pass_info.scale_type_x  = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
      pass_info.scale_type_y  = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
      pass_info.scale_x       = 1.0f;
      pass_info.scale_y       = 1.0f;
      pass_info.rt_format     = tmpinfo.swapchain.format;
      pass_info.source_filter = filter;
      pass_info.mip_filter    = GLSLANG_FILTER_CHAIN_NEAREST;
      pass_info.address       = GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_EDGE;
      pass_info.max_levels    = 0;

      slang_chain_set_pass_info(chain, shader->passes, pass_info);

      slang_chain_set_shader(chain, shader->passes,
            VK_SHADER_STAGE_VERTEX_BIT,
            opaque_vert,
            sizeof(opaque_vert) / sizeof(uint32_t));

      slang_chain_set_shader(chain, shader->passes,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            opaque_frag,
            sizeof(opaque_frag) / sizeof(uint32_t));
   }

   slang_chain_set_shader_preset(chain, shader);
   shader = NULL; /* owned by the chain now */

   if (!slang_chain_init_alias_early(chain))
   {
      RARCH_ERR("[Vulkan] Deferred: failed to initialize alias map.\n");
      slang_chain_free(chain);
      return NULL;
   }

   if (out_num_passes)
      *out_num_passes = tmpinfo.num_passes;

   return chain;
}

bool vulkan_filter_chain_compile_pass(
      vulkan_filter_chain_t *chain,
      unsigned pass_index,
      glslang_filter_chain_filter filter)
{
   return slang_chain_compile_full_pass(chain, pass_index, filter);
}

bool vulkan_filter_chain_finalize(vulkan_filter_chain_t *chain)
{
   return slang_chain_finalize(chain);
}

struct video_shader *vulkan_filter_chain_get_preset(
      vulkan_filter_chain_t *chain)
{
   return slang_chain_get_shader_preset(chain);
}

void vulkan_filter_chain_free(
      vulkan_filter_chain_t *chain)
{
   slang_chain_free(chain);
   input_state_get_ptr()->shader_uses_sensors = false;
}

void vulkan_filter_chain_set_shader(
      vulkan_filter_chain_t *chain,
      unsigned pass,
      VkShaderStageFlags stage,
      const uint32_t *spirv,
      size_t spirv_words)
{
   slang_chain_set_shader(chain, pass, stage, spirv, spirv_words);
}

void vulkan_filter_chain_set_pass_info(
      vulkan_filter_chain_t *chain,
      unsigned pass,
      const struct vulkan_filter_chain_pass_info *info)
{
   slang_chain_set_pass_info(chain, pass, *info);
}

VkFormat vulkan_filter_chain_get_pass_rt_format(
      vulkan_filter_chain_t *chain,
      unsigned pass)
{
   return slang_chain_get_pass_rt_format(chain, pass);
}

bool vulkan_filter_chain_update_swapchain_info(
      vulkan_filter_chain_t *chain,
      const vulkan_filter_chain_swapchain_info *info)
{
   return slang_chain_update_swapchain_info(chain, *info);
}

void vulkan_filter_chain_notify_sync_index(
      vulkan_filter_chain_t *chain,
      unsigned index)
{
   slang_chain_notify_sync_index(chain, index);
}

bool vulkan_filter_chain_init(vulkan_filter_chain_t *chain)
{
   return slang_chain_init(chain);
}

void vulkan_filter_chain_set_input_texture(
      vulkan_filter_chain_t *chain,
      const struct vulkan_filter_chain_texture *texture)
{
   slang_chain_set_input_texture(chain, *texture);
}

void vulkan_filter_chain_set_frame_count(
      vulkan_filter_chain_t *chain,
      uint64_t count)
{
   slang_chain_set_frame_count(chain, count);
}

void vulkan_filter_chain_set_frame_count_period(
      vulkan_filter_chain_t *chain,
      unsigned pass,
      unsigned period)
{
   slang_chain_set_frame_count_period(chain, pass, period);
}

void vulkan_filter_chain_set_shader_subframes(
      vulkan_filter_chain_t *chain,
      uint32_t tot_subframes)
{
   slang_chain_set_shader_subframes(chain, tot_subframes);
}

void vulkan_filter_chain_set_current_shader_subframe(
      vulkan_filter_chain_t *chain,
      uint32_t cur_subframe)
{
   slang_chain_set_current_shader_subframe(chain, cur_subframe);
}

#ifdef VULKAN_ROLLING_SCANLINE_SIMULATION
void vulkan_filter_chain_set_simulate_scanline(
      vulkan_filter_chain_t *chain,
      bool simulate_scanline)
{
   slang_chain_set_simulate_scanline(chain, simulate_scanline);
}
#endif /* VULKAN_ROLLING_SCANLINE_SIMULATION */

void vulkan_filter_chain_set_frame_direction(
      vulkan_filter_chain_t *chain,
      int32_t direction)
{
   slang_chain_set_frame_direction(chain, direction);
}

void vulkan_filter_chain_set_frame_time_delta(
      vulkan_filter_chain_t *chain,
      uint32_t time_delta)
{
   slang_chain_set_frame_time_delta(chain, time_delta);
}

void vulkan_filter_chain_set_original_fps(
      vulkan_filter_chain_t *chain,
      float fps)
{
   slang_chain_set_original_fps(chain, fps);
}

void vulkan_filter_chain_set_rotation(
      vulkan_filter_chain_t *chain,
      uint32_t rot)
{
   slang_chain_set_rotation(chain, rot);
}

void vulkan_filter_chain_set_core_aspect(
      vulkan_filter_chain_t *chain,
      float coreaspect)
{
   slang_chain_set_core_aspect(chain, coreaspect);
}

void vulkan_filter_chain_set_core_aspect_rot(
      vulkan_filter_chain_t *chain,
      float coreaspectrot)
{
   slang_chain_set_core_aspect_rot(chain, coreaspectrot);
}

#ifdef VULKAN_HDR_SWAPCHAIN
void vulkan_filter_chain_set_hdr_mode(
      vulkan_filter_chain_t *chain,
      unsigned hdr_mode)
{
   slang_chain_set_hdr_mode(chain, hdr_mode);
}

void vulkan_filter_chain_set_paper_white_nits(
      vulkan_filter_chain_t *chain,
      float paper_white_nits)
{
   slang_chain_set_paper_white_nits(chain, paper_white_nits);
}


void vulkan_filter_chain_set_expand_gamut(
      vulkan_filter_chain_t *chain,
      unsigned expand_gamut)
{
   slang_chain_set_expand_gamut(chain, expand_gamut);
}

void vulkan_filter_chain_set_scanlines(
      vulkan_filter_chain_t *chain,
      float scanlines)
{
   slang_chain_set_scanlines(chain, scanlines);
}

void vulkan_filter_chain_set_subpixel_layout(
      vulkan_filter_chain_t *chain,
      unsigned subpixel_layout)
{
   slang_chain_set_subpixel_layout(chain, subpixel_layout);
}

void vulkan_filter_chain_set_inverse_tonemap(
      vulkan_filter_chain_t *chain,
      float inverse_tonemap)
{
   slang_chain_set_inverse_tonemap(chain, inverse_tonemap);
}

void vulkan_filter_chain_set_hdr10(
      vulkan_filter_chain_t *chain,
      float hdr10)
{
   slang_chain_set_hdr10(chain, hdr10);
}
#endif /* VULKAN_HDR_SWAPCHAIN */

void vulkan_filter_chain_set_pass_name(
      vulkan_filter_chain_t *chain,
      unsigned pass,
      const char *name)
{
   slang_chain_set_pass_name(chain, pass, name);
}

void vulkan_filter_chain_build_offscreen_passes(
      vulkan_filter_chain_t *chain,
      VkCommandBuffer cmd, const VkViewport *vp)
{
   slang_chain_build_offscreen_passes(chain, cmd, *vp);
}

void vulkan_filter_chain_build_viewport_pass(
      vulkan_filter_chain_t *chain,
      VkCommandBuffer cmd, const VkViewport *vp, const float *mvp)
{
   slang_chain_build_viewport_pass(chain, cmd, *vp, mvp);
}

void vulkan_filter_chain_end_frame(
      vulkan_filter_chain_t *chain,
      VkCommandBuffer cmd)
{
   slang_chain_end_frame(chain, cmd);
}

bool vulkan_filter_chain_emits_hdr10(vulkan_filter_chain_t *chain)
{
   return slang_chain_emits_hdr10(chain);
}

bool vulkan_filter_chain_emits_hdr16(vulkan_filter_chain_t *chain)
{
   return slang_chain_emits_hdr16(chain);
}
