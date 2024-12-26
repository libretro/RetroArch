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
#include "glslang_util_cxx.h"
#include <vector>
#include <memory>
#include <functional>
#include <utility>
#include <algorithm>
#include <string.h>

#include <compat/strl.h>
#include <formats/image.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>

#include "slang_reflection.h"
#include "slang_reflection.hpp"

#include "../common/vulkan_common.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../msg_hash.h"

static const uint32_t opaque_vert[] =
#include "../drivers/vulkan_shaders/opaque.vert.inc"
;

static const uint32_t opaque_frag[] =
#include "../drivers/vulkan_shaders/opaque.frag.inc"
;

static void vulkan_initialize_render_pass(VkDevice device, VkFormat format,
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

   vkCreateRenderPass(device, &rp_info, NULL, render_pass);
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
      struct Size2D size,
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

   vkCmdPipelineBarrier(cmd,
         VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
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

      src_width                                 = MAX(size.width >> (i - 1), 1u);
      src_height                                = MAX(size.height >> (i - 1), 1u);
      target_width                              = MAX(size.width >> i, 1u);
      target_height                             = MAX(size.height >> i, 1u);

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
      struct Size2D size,
      VkCommandBuffer cmd,
      VkImage src_image, VkImageLayout src_layout)
{
   VkImageCopy region;

   VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(
         cmd,
         image,
         VK_REMAINING_MIP_LEVELS,
         VK_IMAGE_LAYOUT_UNDEFINED,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         0,
         VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
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
   region.extent.width                  = size.width;
   region.extent.height                 = size.height;
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

struct Texture
{
   vulkan_filter_chain_texture texture;
   glslang_filter_chain_filter filter;
   glslang_filter_chain_filter mip_filter;
   glslang_filter_chain_address address;
};

class DeferredDisposer
{
   public:
      DeferredDisposer(std::vector<std::function<void ()>> &calls) : calls(calls) {}

      void defer(std::function<void ()> func)
      {
         calls.push_back(std::move(func));
      }

   private:
      std::vector<std::function<void ()>> &calls;
};

class Buffer
{
   public:
      Buffer(VkDevice device,
            const VkPhysicalDeviceMemoryProperties &mem_props,
            size_t size, VkBufferUsageFlags usage);
      ~Buffer();

      size_t get_size() const { return size; }
      void *map();
      void unmap();

      const VkBuffer &get_buffer() const { return buffer; }

      Buffer(Buffer&&) = delete;
      void operator=(Buffer&&) = delete;

   private:
      VkDevice device;
      VkBuffer buffer;
      VkDeviceMemory memory;
      size_t size;
      void *mapped = nullptr;
};

class StaticTexture
{
   public:
      StaticTexture(std::string id,
            VkDevice device,
            VkImage image,
            VkImageView view,
            VkDeviceMemory memory,
            std::unique_ptr<Buffer> buffer,
            unsigned width, unsigned height,
            bool linear,
            bool mipmap,
            glslang_filter_chain_address address);
      ~StaticTexture();

      StaticTexture(StaticTexture&&) = delete;
      void operator=(StaticTexture&&) = delete;

      void release_staging_buffer() { buffer.reset(); }
      void set_id(std::string name) { id = std::move(name); }
      const std::string &get_id() const { return id; }
      const Texture &get_texture() const { return texture; }

   private:
      VkDevice device;
      VkImage image;
      VkImageView view;
      VkDeviceMemory memory;
      std::unique_ptr<Buffer> buffer;
      std::string id;
      Texture texture;
};

class Framebuffer
{
   public:
      Framebuffer(VkDevice device,
            const VkPhysicalDeviceMemoryProperties &mem_props,
            const Size2D &max_size, VkFormat format, unsigned max_levels);

      ~Framebuffer();
      Framebuffer(Framebuffer&&) = delete;
      void operator=(Framebuffer&&) = delete;

      void set_size(DeferredDisposer &disposer, const Size2D &size, VkFormat format = VK_FORMAT_UNDEFINED);

      const Size2D &get_size() const { return size; }
      VkFormat get_format() const { return format; }
      VkImage get_image() const { return image; }
      VkImageView get_view() const { return view; }
      VkFramebuffer get_framebuffer() const { return framebuffer; }
      VkRenderPass get_render_pass() const { return render_pass; }

      unsigned get_levels() const { return levels; }

   private:
      Size2D size;
      VkFormat format;
      unsigned max_levels;
      const VkPhysicalDeviceMemoryProperties &memory_properties;
      VkDevice device           = VK_NULL_HANDLE;
      VkImage image             = VK_NULL_HANDLE;
      VkImageView view          = VK_NULL_HANDLE;
      VkImageView fb_view       = VK_NULL_HANDLE;
      unsigned levels           = 0;

      VkFramebuffer framebuffer = VK_NULL_HANDLE;
      VkRenderPass render_pass  = VK_NULL_HANDLE;

      struct
      {
         size_t size            = 0;
         uint32_t type          = 0;
         VkDeviceMemory memory  = VK_NULL_HANDLE;
      } memory;

      void init(DeferredDisposer *disposer);
};

struct CommonResources
{
   CommonResources(VkDevice device,
         const VkPhysicalDeviceMemoryProperties &memory_properties);
   ~CommonResources();

   std::unique_ptr<Buffer> vbo;
   std::unique_ptr<Buffer> ubo;
   uint8_t *ubo_mapped          = nullptr;
   size_t ubo_sync_index_stride = 0;
   size_t ubo_offset            = 0;
   size_t ubo_alignment         = 1;

   VkSampler samplers[GLSLANG_FILTER_CHAIN_COUNT][GLSLANG_FILTER_CHAIN_COUNT][GLSLANG_FILTER_CHAIN_ADDRESS_COUNT];

   std::vector<Texture> original_history;
   std::vector<Texture> fb_feedback;
   std::vector<Texture> pass_outputs;
   std::vector<std::unique_ptr<StaticTexture>> luts;

   std::unordered_map<std::string, slang_texture_semantic_map> texture_semantic_map;
   std::unordered_map<std::string, slang_texture_semantic_map> texture_semantic_uniform_map;
   std::unique_ptr<video_shader> shader_preset;

   VkDevice device;
};

class Pass
{
   public:
      Pass(VkDevice device,
            const VkPhysicalDeviceMemoryProperties &memory_properties,
            VkPipelineCache cache, unsigned num_sync_indices, bool final_pass) :
         device(device),
         memory_properties(memory_properties),
         cache(cache),
         num_sync_indices(num_sync_indices),
         final_pass(final_pass)
#ifdef VULKAN_ROLLING_SCANLINE_SIMULATION         
         ,simulate_scanline(false)
#endif // VULKAN_ROLLING_SCANLINE_SIMULATION         
      {}

      ~Pass();

      Pass(Pass&&) = delete;
      void operator=(Pass&&) = delete;

      const Framebuffer &get_framebuffer() const { return *framebuffer; }
      Framebuffer *get_feedback_framebuffer() { return fb_feedback.get(); }

      Size2D set_pass_info(
            const Size2D &max_original,
            const Size2D &max_source,
            const vulkan_filter_chain_swapchain_info &swapchain,
            const vulkan_filter_chain_pass_info &info);

      void set_shader(VkShaderStageFlags stage,
            const uint32_t *spirv,
            size_t spirv_words);

      bool build();
      bool init_feedback();

      void build_commands(
            DeferredDisposer &disposer,
            VkCommandBuffer cmd,
            const Texture &original,
            const Texture &source,
            const VkViewport &vp,
            const float *mvp);

      void notify_sync_index(unsigned index) { sync_index = index; }
      void set_frame_count(uint64_t count) { frame_count = count; }
      void set_frame_count_period(unsigned p) { frame_count_period = p; }
      void set_shader_subframes(uint32_t ts) { total_subframes = ts; }
      void set_current_shader_subframe(uint32_t cs) { current_subframe = cs; }
#ifdef VULKAN_ROLLING_SCANLINE_SIMULATION      
      void set_simulate_scanline(bool simulate) { simulate_scanline = simulate; }
#endif // VULKAN_ROLLING_SCANLINE_SIMULATION      
      void set_frame_direction(int32_t dir) { frame_direction = dir; }
      void set_rotation(uint32_t rot) { rotation = rot; }
      void set_name(const char *name) { pass_name = name; }
      const std::string &get_name() const { return pass_name; }
      glslang_filter_chain_filter get_source_filter() const { 
         return pass_info.source_filter; }

      glslang_filter_chain_filter get_mip_filter() const
      {
         return pass_info.mip_filter;
      }

      glslang_filter_chain_address get_address_mode() const
      {
         return pass_info.address;
      }

      void set_common_resources(CommonResources *c) { this->common = c; }
      const slang_reflection &get_reflection() const { return reflection; }
      void set_pass_number(unsigned pass) { pass_number = pass; }

      void add_parameter(unsigned parameter_index, const std::string &id);

      void end_frame();
      void allocate_buffers();

   private:
      VkDevice device;
      const VkPhysicalDeviceMemoryProperties &memory_properties;
      VkPipelineCache cache;
      unsigned num_sync_indices;
      unsigned sync_index;
      bool final_pass;
#ifdef VULKAN_ROLLING_SCANLINE_SIMULATION      
      bool simulate_scanline;
#endif // VULKAN_ROLLING_SCANLINE_SIMULATION

      Size2D get_output_size(const Size2D &original_size,
            const Size2D &max_source) const;

      VkPipeline pipeline              = VK_NULL_HANDLE;
      VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
      VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
      VkDescriptorPool pool            = VK_NULL_HANDLE;

      std::vector<VkDescriptorSet> sets;
      CommonResources *common = nullptr;

      Size2D current_framebuffer_size;
      VkViewport current_viewport;
      vulkan_filter_chain_pass_info pass_info;

      std::vector<uint32_t> vertex_shader;
      std::vector<uint32_t> fragment_shader;
      std::unique_ptr<Framebuffer> framebuffer;
      std::unique_ptr<Framebuffer> fb_feedback;
      VkRenderPass swapchain_render_pass;

      void clear_vk();
      bool init_pipeline();
      bool init_pipeline_layout();

      void set_semantic_texture(VkDescriptorSet set,
            slang_texture_semantic semantic,
            const Texture &texture);
      void set_semantic_texture_array(VkDescriptorSet set,
            slang_texture_semantic semantic, unsigned index,
            const Texture &texture);

      slang_reflection reflection;
      void build_semantics(VkDescriptorSet set, uint8_t *buffer,
            const float *mvp, const Texture &original, const Texture &source);
      void build_semantic_vec4(uint8_t *data, slang_semantic semantic,
            unsigned width, unsigned height);
      void build_semantic_uint(uint8_t *data, slang_semantic semantic, uint32_t value);
      void build_semantic_int(uint8_t *data, slang_semantic semantic, int32_t value);
      void build_semantic_parameter(uint8_t *data, unsigned index, float value);
      void build_semantic_texture_vec4(uint8_t *data,
            slang_texture_semantic semantic,
            unsigned width, unsigned height);
      void build_semantic_texture_array_vec4(uint8_t *data,
            slang_texture_semantic semantic, unsigned index,
            unsigned width, unsigned height);
      void build_semantic_texture(VkDescriptorSet set, uint8_t *buffer,
            slang_texture_semantic semantic, const Texture &texture);
      void build_semantic_texture_array(VkDescriptorSet set, uint8_t *buffer,
            slang_texture_semantic semantic, unsigned index, const Texture &texture);

      uint64_t frame_count        = 0;
      int32_t frame_direction     = 1;
      uint32_t rotation           = 0;
      unsigned frame_count_period = 0;
      unsigned pass_number        = 0;
      uint32_t total_subframes    = 1;
      uint32_t current_subframe   = 1;

      size_t ubo_offset           = 0;
      std::string pass_name;

      struct Parameter
      {
         std::string id;
         unsigned index;
         unsigned semantic_index;
      };

      std::vector<Parameter> parameters;
      std::vector<Parameter> filtered_parameters;

      struct PushConstant
      {
         VkShaderStageFlags stages = 0;
         std::vector<uint32_t> buffer; /* uint32_t to have correct alignment. */
      };
      PushConstant push;
};

/* struct here since we're implementing the opaque typedef from C. */
struct vulkan_filter_chain
{
   public:
      vulkan_filter_chain(const vulkan_filter_chain_create_info &info);
      ~vulkan_filter_chain();

      inline void set_shader_preset(std::unique_ptr<video_shader> shader)
      {
         common.shader_preset = std::move(shader);
      }

      inline video_shader *get_shader_preset()
      {
         return common.shader_preset.get();
      }

      void set_pass_info(unsigned pass,
            const vulkan_filter_chain_pass_info &info);
      void set_shader(unsigned pass, VkShaderStageFlags stage,
            const uint32_t *spirv, size_t spirv_words);

      bool init();
      bool update_swapchain_info(
            const vulkan_filter_chain_swapchain_info &info);

      void notify_sync_index(unsigned index);
      void set_input_texture(const vulkan_filter_chain_texture &texture);
      void build_offscreen_passes(VkCommandBuffer cmd, const VkViewport &vp);
      void build_viewport_pass(VkCommandBuffer cmd,
            const VkViewport &vp, const float *mvp);
      void end_frame(VkCommandBuffer cmd);

      void set_frame_count(uint64_t count);
      void set_frame_count_period(unsigned pass, unsigned period);
      void set_shader_subframes(uint32_t total_subframes);
      void set_current_shader_subframe(uint32_t current_subframe);
 #ifdef VULKAN_ROLLING_SCANLINE_SIMULATION     
      void set_simulate_scanline(bool simulate_scanline);
 #endif // VULKAN_ROLLING_SCANLINE_SIMULATION     
      void set_frame_direction(int32_t direction);
      void set_rotation(uint32_t rot);
      void set_pass_name(unsigned pass, const char *name);

      void add_static_texture(std::unique_ptr<StaticTexture> texture);
      void add_parameter(unsigned pass, unsigned parameter_index, const std::string &id);
      void release_staging_buffers();

      VkFormat get_pass_rt_format(unsigned pass);

      bool emits_hdr10() const;
      void set_hdr10();

   private:
      VkDevice device;
      VkPhysicalDevice gpu;
      const VkPhysicalDeviceMemoryProperties &memory_properties;
      VkPipelineCache cache;
      std::vector<std::unique_ptr<Pass>> passes;
      std::vector<vulkan_filter_chain_pass_info> pass_info;
      std::vector<std::vector<std::function<void ()>>> deferred_calls;
      CommonResources common;
      VkFormat original_format;

      vulkan_filter_chain_texture input_texture;

      Size2D max_input_size;
      vulkan_filter_chain_swapchain_info swapchain_info;
      unsigned current_sync_index;

      std::vector<std::unique_ptr<Framebuffer>> original_history;
      bool require_clear        = false;
      bool emits_hdr_colorspace = false;

      void flush();

      void set_num_passes(unsigned passes);
      void execute_deferred();
      void set_num_sync_indices(unsigned num_indices);
      void set_swapchain_info(const vulkan_filter_chain_swapchain_info &info);

      bool init_ubo();
      bool init_history();
      bool init_feedback();
      bool init_alias();
      void update_history(DeferredDisposer &disposer, VkCommandBuffer cmd);
      void clear_history_and_feedback(VkCommandBuffer cmd);
      void update_feedback_info();
      void update_history_info();
};

static uint32_t find_memory_type_fallback(
      const VkPhysicalDeviceMemoryProperties &mem_props,
      uint32_t device_reqs, uint32_t host_reqs)
{
   unsigned i;
   for (i = 0; i < VK_MAX_MEMORY_TYPES; i++)
   {
      if ((device_reqs & (1u << i)) &&
            (mem_props.memoryTypes[i].propertyFlags & host_reqs) == host_reqs)
         return i;
   }

   return vulkan_find_memory_type(&mem_props, device_reqs, 0);
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

static std::unique_ptr<StaticTexture> vulkan_filter_chain_load_lut(
      VkCommandBuffer cmd,
      const struct vulkan_filter_chain_create_info *info,
      vulkan_filter_chain *chain,
      const video_shader_lut *shader)
{
   unsigned i;
   texture_image image;
   VkBufferImageCopy region;
   VkImageCreateInfo image_info;
   std::unique_ptr<Buffer> buffer;
   VkMemoryRequirements mem_reqs;
   VkImageViewCreateInfo view_info;
   VkMemoryAllocateInfo alloc;
   VkImage tex                     = VK_NULL_HANDLE;
   VkDeviceMemory memory           = VK_NULL_HANDLE;
   VkImageView view                = VK_NULL_HANDLE;
   void *ptr                       = nullptr;

   image.width                     = 0;
   image.height                    = 0;
   image.pixels                    = NULL;
   image.supports_rgba             = video_driver_supports_rgba();

   if (!image_texture_load(&image, shader->path))
      return {};

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

   vkCreateImage(info->device, &image_info, nullptr, &tex);
   vulkan_debug_mark_image(info->device, tex);
   vkGetImageMemoryRequirements(info->device, tex, &mem_reqs);

   alloc.sType                     = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.pNext                     = NULL;
   alloc.allocationSize            = mem_reqs.size;
   alloc.memoryTypeIndex           = vulkan_find_memory_type(
         &*info->memory_properties,
         mem_reqs.memoryTypeBits,
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

   if (vkAllocateMemory(info->device, &alloc, nullptr, &memory) != VK_SUCCESS)
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
   vkCreateImageView(info->device, &view_info, nullptr, &view);

   buffer                                = 
      std::unique_ptr<Buffer>(new Buffer(info->device, *info->memory_properties,
               image.width * image.height * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
   ptr                                   = buffer->map();
   memcpy(ptr, image.pixels, image.width * image.height * sizeof(uint32_t));
   buffer->unmap();

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
         buffer->get_buffer(),
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
   image.pixels = nullptr;

   return std::unique_ptr<StaticTexture>(new StaticTexture(shader->id, info->device,
            tex, view, memory, std::move(buffer), image.width, image.height,
            shader->filter != RARCH_FILTER_NEAREST,
            image_info.mipLevels > 1,
            rarch_wrap_to_address(shader->wrap)));

error:
   if (image.pixels)
      image_texture_free(&image);
   if (tex != VK_NULL_HANDLE)
      vkDestroyImage(info->device, tex, nullptr);
   if (view != VK_NULL_HANDLE)
      vkDestroyImageView(info->device, view, nullptr);
   if (memory != VK_NULL_HANDLE)
      vkFreeMemory(info->device, memory, nullptr);
   return {};
}

static bool vulkan_filter_chain_load_luts(
      const struct vulkan_filter_chain_create_info *info,
      vulkan_filter_chain *chain,
      video_shader *shader)
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

   vkAllocateCommandBuffers(info->device, &cmd_info, &cmd);
   begin_info.sType                              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin_info.pNext                              = NULL;
   begin_info.flags                              = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   begin_info.pInheritanceInfo                   = NULL;
   vkBeginCommandBuffer(cmd, &begin_info);

   for (i = 0; i < shader->luts; i++)
   {
      std::unique_ptr<StaticTexture> image = 
         vulkan_filter_chain_load_lut(cmd, info, chain, &shader->lut[i]);
      if (!image)
      {
         RARCH_ERR("[Vulkan]: Failed to load LUT \"%s\".\n", shader->lut[i].path);
         vkEndCommandBuffer(cmd);
         if (cmd != VK_NULL_HANDLE)
            vkFreeCommandBuffers(info->device, info->command_pool, 1, &cmd);
         return false;
      }

      chain->add_static_texture(std::move(image));
   }

   vkEndCommandBuffer(cmd);
   submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit_info.pNext                = NULL;
   submit_info.waitSemaphoreCount   = 0;
   submit_info.pWaitSemaphores      = NULL;
   submit_info.commandBufferCount   = 1;
   submit_info.pCommandBuffers      = &cmd;
   submit_info.signalSemaphoreCount = 0;
   submit_info.pSignalSemaphores    = NULL;
   vkQueueSubmit(info->queue, 1, &submit_info, VK_NULL_HANDLE);
   vkQueueWaitIdle(info->queue);
   vkFreeCommandBuffers(info->device, info->command_pool, 1, &cmd);
   chain->release_staging_buffers();
   return true;
}

vulkan_filter_chain::vulkan_filter_chain(
      const vulkan_filter_chain_create_info &info)
   : device(info.device),
     gpu(info.gpu),
     memory_properties(*info.memory_properties),
     cache(info.pipeline_cache),
     common(info.device, *info.memory_properties),
     original_format(info.original_format)
{
   max_input_size = { info.max_input_size.width, info.max_input_size.height };
   set_swapchain_info(info.swapchain);
   set_num_passes(info.num_passes);
}

vulkan_filter_chain::~vulkan_filter_chain()
{
   flush();
}

void vulkan_filter_chain::set_swapchain_info(
      const vulkan_filter_chain_swapchain_info &info)
{
   swapchain_info = info;
   set_num_sync_indices(info.num_indices);
}

void vulkan_filter_chain::set_num_sync_indices(unsigned num_indices)
{
   execute_deferred();
   deferred_calls.resize(num_indices);
}

void vulkan_filter_chain::notify_sync_index(unsigned index)
{
   unsigned i;
   auto &calls = deferred_calls[index];
   for (auto &call : calls)
      call();
   calls.clear();

   current_sync_index = index;

   for (i = 0; i < passes.size(); i++)
      passes[i]->notify_sync_index(index);
}

bool vulkan_filter_chain::update_swapchain_info(
      const vulkan_filter_chain_swapchain_info &info)
{
   flush();
   set_swapchain_info(info);
   return init();
}

void vulkan_filter_chain::release_staging_buffers()
{
   unsigned i;
   for (i = 0; i < common.luts.size(); i++)
      common.luts[i]->release_staging_buffer();
}

void vulkan_filter_chain::execute_deferred()
{
   for (auto &calls : deferred_calls)
   {
      for (auto &call : calls)
         call();
      calls.clear();
   }
}

void vulkan_filter_chain::flush()
{
   vkDeviceWaitIdle(device);
   execute_deferred();
}

void vulkan_filter_chain::update_history_info()
{
   unsigned i = 0;

   for (i = 0; i < original_history.size(); i++)
   {
      Texture *source = (Texture*)&common.original_history[i];

      if (!source)
         continue;

      source->texture.layout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      source->texture.view     = original_history[i]->get_view();
      source->texture.image    = original_history[i]->get_image();
      source->texture.width    = original_history[i]->get_size().width;
      source->texture.height   = original_history[i]->get_size().height;
      source->filter           = passes.front()->get_source_filter();
      source->mip_filter       = passes.front()->get_mip_filter();
      source->address          = passes.front()->get_address_mode();
   }
}

void vulkan_filter_chain::update_feedback_info()
{
   unsigned i;
   if (common.fb_feedback.empty())
      return;

   for (i = 0; i < passes.size() - 1; i++)
   {
      Framebuffer *fb = passes[i]->get_feedback_framebuffer();
      if (!fb)
         continue;

      Texture *source         = &common.fb_feedback[i];

      if (!source)
         continue;

      source->texture.image   = fb->get_image();
      source->texture.view    = fb->get_view();
      source->texture.layout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      source->texture.width   = fb->get_size().width;
      source->texture.height  = fb->get_size().height;
      source->filter          = passes[i]->get_source_filter();
      source->mip_filter      = passes[i]->get_mip_filter();
      source->address         = passes[i]->get_address_mode();
   }
}

void vulkan_filter_chain::build_offscreen_passes(VkCommandBuffer cmd,
      const VkViewport &vp)
{
   unsigned i;
   Texture source;

   /* First frame, make sure our history and feedback textures 
    * are in a clean state. */
   if (require_clear)
   {
      clear_history_and_feedback(cmd);
      require_clear = false;
   }

   update_history_info();
   update_feedback_info();

   DeferredDisposer disposer(deferred_calls[current_sync_index]);
   const Texture original = {
      input_texture,
      passes.front()->get_source_filter(),
      passes.front()->get_mip_filter(),
      passes.front()->get_address_mode(),
   };

   source = original;

   for (i = 0; i < passes.size() - 1; i++)
   {
      passes[i]->build_commands(disposer, cmd,
            original, source, vp, nullptr);

      const Framebuffer &fb   = passes[i]->get_framebuffer();

      source.texture.view     = fb.get_view();
      source.texture.layout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      source.texture.width    = fb.get_size().width;
      source.texture.height   = fb.get_size().height;
      source.filter           = passes[i + 1]->get_source_filter();
      source.mip_filter       = passes[i + 1]->get_mip_filter();
      source.address          = passes[i + 1]->get_address_mode();

      common.pass_outputs[i]  = source;
   }
}

void vulkan_filter_chain::update_history(DeferredDisposer &disposer,
      VkCommandBuffer cmd)
{
   std::unique_ptr<Framebuffer> tmp;
   VkImageLayout src_layout = input_texture.layout;

   /* Transition input texture to something appropriate. */
   if (input_texture.layout != VK_IMAGE_LAYOUT_GENERAL)
   {
      VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,
            input_texture.image,VK_REMAINING_MIP_LEVELS,
            input_texture.layout,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            0,
            VK_ACCESS_TRANSFER_READ_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED);

      src_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
   }

   std::unique_ptr<Framebuffer> &back = original_history.back();
   swap(back, tmp);

   if   (    input_texture.width  != tmp->get_size().width
         ||  input_texture.height != tmp->get_size().height
         || (input_texture.format != VK_FORMAT_UNDEFINED 
         &&  input_texture.format != tmp->get_format()))
      tmp->set_size(disposer, { input_texture.width, input_texture.height }, input_texture.format);

   vulkan_framebuffer_copy(tmp->get_image(), tmp->get_size(),
         cmd, input_texture.image, src_layout);

   /* Transition input texture back. */
   if (input_texture.layout != VK_IMAGE_LAYOUT_GENERAL)
   {
      VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,
            input_texture.image,VK_REMAINING_MIP_LEVELS,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            input_texture.layout,
            0,
            VK_ACCESS_SHADER_READ_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED);
   }

   /* Should ring buffer, but we don't have *that* many passes. */
   move_backward(begin(original_history), end(original_history) - 1, end(original_history));
   swap(original_history.front(), tmp);
}

void vulkan_filter_chain::end_frame(VkCommandBuffer cmd)
{
   /* If we need to keep old frames, copy it after fragment is complete.
    * TODO: We can improve pipelining by figuring out which
    * pass is the last that reads from
    * the history and dispatch the copy earlier. */
   if (!original_history.empty())
   {
      DeferredDisposer disposer(deferred_calls[current_sync_index]);
      update_history(disposer, cmd);
   }
}

void vulkan_filter_chain::build_viewport_pass(
      VkCommandBuffer cmd, const VkViewport &vp, const float *mvp)
{
   unsigned i;
   Texture source;

   /* First frame, make sure our history and 
    * feedback textures are in a clean state. */
   if (require_clear)
   {
      clear_history_and_feedback(cmd);
      require_clear = false;
   }

   DeferredDisposer disposer(deferred_calls[current_sync_index]);
   const Texture original = {
      input_texture,
      passes.front()->get_source_filter(),
      passes.front()->get_mip_filter(),
      passes.front()->get_address_mode(),
   };

   if (passes.size() == 1)
   {
      source = {
         input_texture,
         passes.back()->get_source_filter(),
         passes.back()->get_mip_filter(),
         passes.back()->get_address_mode(),
      };
   }
   else
   {
      const Framebuffer &fb  = passes[passes.size() - 2]->get_framebuffer();
      source.texture.view    = fb.get_view();
      source.texture.layout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      source.texture.width   = fb.get_size().width;
      source.texture.height  = fb.get_size().height;
      source.filter          = passes.back()->get_source_filter();
      source.mip_filter      = passes.back()->get_mip_filter();
      source.address         = passes.back()->get_address_mode();
   }

   passes.back()->build_commands(disposer, cmd,
         original, source, vp, mvp);

   /* For feedback FBOs, swap current and previous. */
   for (i = 0; i < passes.size(); i++)
      passes[i]->end_frame();
}

bool vulkan_filter_chain::init_history()
{
   unsigned i;
   size_t required_images = 0;

   original_history.clear();
   common.original_history.clear();

   for (i = 0; i < passes.size(); i++)
      required_images =
         std::max(required_images,
               passes[i]->get_reflection().semantic_textures[
               SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY].size());

   if (required_images < 2)
   {
#ifdef VULKAN_DEBUG
      RARCH_LOG("[Vulkan filter chain]: Not using frame history.\n");
#endif
      return true;
   }

   /* We don't need to store array element #0,
    * since it's aliased with the actual original. */
   required_images--;
   original_history.reserve(required_images);
   common.original_history.resize(required_images);

   for (i = 0; i < required_images; i++)
      original_history.emplace_back(new Framebuffer(device, memory_properties,
               max_input_size, original_format, 1));

#ifdef VULKAN_DEBUG
   RARCH_LOG("[Vulkan filter chain]: Using history of %u frames.\n", unsigned(required_images));
#endif

   /* On first frame, we need to clear the textures to
    * a known state, but we need
    * a command buffer for that, so just defer to first frame.
    */
   require_clear = true;
   return true;
}

bool vulkan_filter_chain::init_feedback()
{
   unsigned i;
   bool use_feedbacks = false;

   common.fb_feedback.clear();

   /* Final pass cannot have feedback. */
   for (i = 0; i < passes.size() - 1; i++)
   {
      bool use_feedback = false;
      for (auto &pass : passes)
      {
         const slang_reflection &r = pass->get_reflection();
         auto          &feedbacks  = r.semantic_textures[
            SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK];

         if (i < feedbacks.size() && feedbacks[i].texture)
         {
            use_feedback  = true;
            use_feedbacks = true;
            break;
         }
      }

      if (use_feedback)
      {
         if (!passes[i]->init_feedback())
            return false;
         RARCH_LOG("[Vulkan filter chain]: Using framebuffer feedback for pass #%u.\n", i);
      }
   }

   if (!use_feedbacks)
   {
#ifdef VULKAN_DEBUG
      RARCH_LOG("[Vulkan filter chain]: Not using framebuffer feedback.\n");
#endif
      return true;
   }

   common.fb_feedback.resize(passes.size() - 1);
   require_clear = true;
   return true;
}

bool vulkan_filter_chain::init_alias()
{
   int i;
   
   common.texture_semantic_map.clear();
   common.texture_semantic_uniform_map.clear();

   for (i = 0; i < (int)passes.size(); i++)
   {
      unsigned j;
      const std::string name = passes[i]->get_name();
      if (name.empty())
         continue;

      j = (unsigned)(&passes[i] - passes.data());

      if (!slang_set_unique_map(
               common.texture_semantic_map, name,
               slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, j }))
         return false;

      if (!slang_set_unique_map(
               common.texture_semantic_uniform_map, name + "Size",
               slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, j }))
         return false;

      if (!slang_set_unique_map(
               common.texture_semantic_map, name + "Feedback",
               slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, j }))
         return false;

      if (!slang_set_unique_map(
               common.texture_semantic_uniform_map, name + "FeedbackSize",
               slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, j }))
         return false;
   }

   for (i = 0; i < (int)common.luts.size(); i++)
   {
      unsigned j = (unsigned)(&common.luts[i] - common.luts.data());
      if (!slang_set_unique_map(
               common.texture_semantic_map,
               common.luts[i]->get_id(),
               slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_USER, j }))
         return false;

      if (!slang_set_unique_map(
               common.texture_semantic_uniform_map,
               common.luts[i]->get_id() + "Size",
               slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_USER, j }))
         return false;
   }

   return true;
}

void vulkan_filter_chain::set_pass_info(unsigned pass,
      const vulkan_filter_chain_pass_info &info)
{
   pass_info[pass] = info;
}

VkFormat vulkan_filter_chain::get_pass_rt_format(unsigned pass)
{
   return pass_info[pass].rt_format;
}

bool vulkan_filter_chain::emits_hdr10() const
{
   return emits_hdr_colorspace;
}

void vulkan_filter_chain::set_hdr10()
{
   emits_hdr_colorspace = true;
}

void vulkan_filter_chain::set_num_passes(unsigned num_passes)
{
   unsigned i;

   pass_info.resize(num_passes);
   passes.reserve(num_passes);

   for (i = 0; i < num_passes; i++)
   {
      passes.emplace_back(new Pass(device, memory_properties,
               cache, (unsigned)deferred_calls.size(), i + 1 == num_passes));
      passes.back()->set_common_resources(&common);
      passes.back()->set_pass_number(i);
   }
}

void vulkan_filter_chain::set_shader(
      unsigned pass,
      VkShaderStageFlags stage,
      const uint32_t *spirv,
      size_t spirv_words)
{
   passes[pass]->set_shader(stage, spirv, spirv_words);
}

void vulkan_filter_chain::add_parameter(unsigned pass,
      unsigned index, const std::string &id)
{
   passes[pass]->add_parameter(index, id);
}

bool vulkan_filter_chain::init_ubo()
{
   unsigned i;
   VkPhysicalDeviceProperties props;

   common.ubo.reset();
   common.ubo_offset            = 0;

   vkGetPhysicalDeviceProperties(gpu, &props);
   common.ubo_alignment         = props.limits.minUniformBufferOffsetAlignment;

   /* Who knows. :) */
   if (common.ubo_alignment == 0)
      common.ubo_alignment = 1;

   for (i = 0; i < passes.size(); i++)
      passes[i]->allocate_buffers();

   common.ubo_offset            = 
      (common.ubo_offset + common.ubo_alignment - 1) &
      ~(common.ubo_alignment - 1);
   common.ubo_sync_index_stride = common.ubo_offset;

   if (common.ubo_offset != 0)
      common.ubo                = std::unique_ptr<Buffer>(new Buffer(device,
               memory_properties, common.ubo_offset * deferred_calls.size(),
               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));

   common.ubo_mapped            = static_cast<uint8_t*>(common.ubo->map());
   return true;
}

bool vulkan_filter_chain::init()
{
   unsigned i;
   Size2D source = max_input_size;

   if (!init_alias())
      return false;

   for (i = 0; i < passes.size(); i++)
   {
#ifdef VULKAN_DEBUG
      const char *name = passes[i]->get_name().c_str();
      RARCH_LOG("[slang]: Building pass #%u (%s)\n", i,
            string_is_empty(name) ?
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE) :
            name);
#endif
      source = passes[i]->set_pass_info(max_input_size,
            source, swapchain_info, pass_info[i]);
      if (!passes[i]->build())
         return false;
   }

   require_clear = false;
   if (!init_ubo())
      return false;
   if (!init_history())
      return false;
   if (!init_feedback())
      return false;
   common.pass_outputs.resize(passes.size());
   return true;
}

void vulkan_filter_chain::clear_history_and_feedback(VkCommandBuffer cmd)
{
   unsigned i;
   for (i = 0; i < original_history.size(); i++)
      vulkan_framebuffer_clear(original_history[i]->get_image(), cmd);
   for (i = 0; i < passes.size(); i++)
   {
      Framebuffer *fb = passes[i]->get_feedback_framebuffer();
      if (fb)
         vulkan_framebuffer_clear(fb->get_image(), cmd);
   }
}

void vulkan_filter_chain::set_input_texture(
      const vulkan_filter_chain_texture &texture)
{
   input_texture = texture;
}

void vulkan_filter_chain::add_static_texture(
      std::unique_ptr<StaticTexture> texture)
{
   common.luts.push_back(std::move(texture));
}

void vulkan_filter_chain::set_frame_count(uint64_t count)
{
   unsigned i;
   for (i = 0; i < passes.size(); i++)
      passes[i]->set_frame_count(count);
}

void vulkan_filter_chain::set_frame_count_period(
      unsigned pass, unsigned period)
{
   passes[pass]->set_frame_count_period(period);
}

void vulkan_filter_chain::set_shader_subframes(uint32_t total_subframes)
{
   unsigned i;
   for (i = 0; i < passes.size(); i++)
      passes[i]->set_shader_subframes(total_subframes);
}

void vulkan_filter_chain::set_current_shader_subframe(uint32_t current_subframe)
{
   unsigned i;
   for (i = 0; i < passes.size(); i++)
      passes[i]->set_current_shader_subframe(current_subframe);
}

#ifdef VULKAN_ROLLING_SCANLINE_SIMULATION
void vulkan_filter_chain::set_simulate_scanline(bool simulate_scanline)
{
   unsigned i;
   for (i = 0; i < passes.size(); i++)
      passes[i]->set_simulate_scanline(simulate_scanline);
}
#endif // VULKAN_ROLLING_SCANLINE_SIMULATION

void vulkan_filter_chain::set_frame_direction(int32_t direction)
{
   unsigned i;
   for (i = 0; i < passes.size(); i++)
      passes[i]->set_frame_direction(direction);
}

void vulkan_filter_chain::set_rotation(uint32_t rot)
{
   unsigned i;
   for (i = 0; i < passes.size(); i++)
      passes[i]->set_rotation(rot);
}

void vulkan_filter_chain::set_pass_name(unsigned pass, const char *name)
{
   passes[pass]->set_name(name);
}

StaticTexture::StaticTexture(std::string id,
      VkDevice device,
      VkImage image,
      VkImageView view,
      VkDeviceMemory memory,
      std::unique_ptr<Buffer> buffer,
      unsigned width, unsigned height,
      bool linear,
      bool mipmap,
      glslang_filter_chain_address address)
   : device(device),
     image(image),
     view(view),
     memory(memory),
     buffer(std::move(buffer)),
     id(std::move(id))
{
   texture.filter         = GLSLANG_FILTER_CHAIN_NEAREST;
   texture.mip_filter     = GLSLANG_FILTER_CHAIN_NEAREST;
   texture.address        = address;
   texture.texture.image  = image;
   texture.texture.view   = view;
   texture.texture.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   texture.texture.width  = width;
   texture.texture.height = height;

   if (linear)
      texture.filter      = GLSLANG_FILTER_CHAIN_LINEAR;
   if (mipmap && linear)
      texture.mip_filter  = GLSLANG_FILTER_CHAIN_LINEAR;
}

StaticTexture::~StaticTexture()
{
   if (view != VK_NULL_HANDLE)
      vkDestroyImageView(device, view, nullptr);
   if (image != VK_NULL_HANDLE)
      vkDestroyImage(device, image, nullptr);
   if (memory != VK_NULL_HANDLE)
      vkFreeMemory(device, memory, nullptr);
}

Buffer::Buffer(VkDevice device,
      const VkPhysicalDeviceMemoryProperties &mem_props,
      size_t size, VkBufferUsageFlags usage) :
   device(device), size(size)
{
   VkBufferCreateInfo info;
   VkMemoryRequirements mem_reqs;
   VkMemoryAllocateInfo alloc;

   info.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   info.pNext                 = NULL;
   info.flags                 = 0;
   info.size                  = size;
   info.usage                 = usage;
   info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
   info.queueFamilyIndexCount = 0;
   info.pQueueFamilyIndices   = NULL;
   vkCreateBuffer(device, &info, nullptr, &buffer);

   vkGetBufferMemoryRequirements(device, buffer, &mem_reqs);

   alloc.sType                = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.pNext                = NULL;
   alloc.allocationSize       = mem_reqs.size;
   alloc.memoryTypeIndex      = vulkan_find_memory_type(
         &mem_props, mem_reqs.memoryTypeBits,
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

   vkAllocateMemory(device, &alloc, NULL, &memory);
   vulkan_debug_mark_memory(device, memory);
   vkBindBufferMemory(device, buffer, memory, 0);
}

void *Buffer::map()
{
   if (!mapped)
   {
      if (vkMapMemory(device, memory, 0, size, 0, &mapped) != VK_SUCCESS)
         return nullptr;
   }
   return mapped;
}

void Buffer::unmap()
{
   if (mapped)
      vkUnmapMemory(device, memory);
   mapped = nullptr;
}

Buffer::~Buffer()
{
   if (mapped)
      unmap();
   if (memory != VK_NULL_HANDLE)
      vkFreeMemory(device, memory, nullptr);
   if (buffer != VK_NULL_HANDLE)
      vkDestroyBuffer(device, buffer, nullptr);
}

Pass::~Pass()
{
   clear_vk();
}

void Pass::add_parameter(unsigned index, const std::string &id)
{
   parameters.push_back({ id, index, unsigned(parameters.size()) });
}

void Pass::set_shader(VkShaderStageFlags stage,
      const uint32_t *spirv,
      size_t spirv_words)
{
   switch (stage)
   {
      case VK_SHADER_STAGE_VERTEX_BIT:
         vertex_shader.clear();
         vertex_shader.insert(end(vertex_shader),
               spirv, spirv + spirv_words);
         break;
      case VK_SHADER_STAGE_FRAGMENT_BIT:
         fragment_shader.clear();
         fragment_shader.insert(end(fragment_shader),
               spirv, spirv + spirv_words);
         break;
      default:
         break;
   }
}

Size2D Pass::get_output_size(const Size2D &original,
      const Size2D &source) const
{
   float width  = 0.0f;
   float height = 0.0f;
   switch (pass_info.scale_type_x)
   {
      case GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL:
         width = float(original.width) * pass_info.scale_x;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_SOURCE:
         width = float(source.width) * pass_info.scale_x;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT:
         width = (retroarch_get_rotation() % 2 ? current_viewport.height : current_viewport.width) * pass_info.scale_x;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE:
         width = pass_info.scale_x;
         break;

      default:
         break;
   }

   switch (pass_info.scale_type_y)
   {
      case GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL:
         height = float(original.height) * pass_info.scale_y;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_SOURCE:
         height = float(source.height) * pass_info.scale_y;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT:
         height = (retroarch_get_rotation() % 2 ? current_viewport.width : current_viewport.height) * pass_info.scale_y;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE:
         height = pass_info.scale_y;
         break;

      default:
         break;
   }

   return { unsigned(roundf(width)), unsigned(roundf(height)) };
}

Size2D Pass::set_pass_info(
      const Size2D &max_original,
      const Size2D &max_source,
      const vulkan_filter_chain_swapchain_info &swapchain,
      const vulkan_filter_chain_pass_info &info)
{
   clear_vk();

   current_viewport         = swapchain.viewport;
   pass_info                = info;

   num_sync_indices         = swapchain.num_indices;
   sync_index               = 0;

   current_framebuffer_size = get_output_size(max_original, max_source);
   swapchain_render_pass    = swapchain.render_pass;

   return current_framebuffer_size;
}

void Pass::clear_vk()
{
   if (pool != VK_NULL_HANDLE)
      vkDestroyDescriptorPool(device, pool, nullptr);
   if (pipeline != VK_NULL_HANDLE)
      vkDestroyPipeline(device, pipeline, nullptr);
   if (set_layout != VK_NULL_HANDLE)
      vkDestroyDescriptorSetLayout(device, set_layout, nullptr);
   if (pipeline_layout != VK_NULL_HANDLE)
      vkDestroyPipelineLayout(device, pipeline_layout, nullptr);

   pool       = VK_NULL_HANDLE;
   pipeline   = VK_NULL_HANDLE;
   set_layout = VK_NULL_HANDLE;
}

bool Pass::init_pipeline_layout()
{
   unsigned i;
   VkPushConstantRange push_range;
   VkDescriptorPoolCreateInfo pool_info;
   VkPipelineLayoutCreateInfo layout_info;
   std::vector<VkDescriptorSetLayoutBinding> bindings;
   std::vector<VkDescriptorPoolSize> desc_counts;
   VkDescriptorSetLayoutCreateInfo set_layout_info;
   VkDescriptorSetAllocateInfo alloc_info;
   /* Main UBO. */
   VkShaderStageFlags ubo_mask = 0;

   if (reflection.ubo_stage_mask & SLANG_STAGE_VERTEX_MASK)
      ubo_mask |= VK_SHADER_STAGE_VERTEX_BIT;
   if (reflection.ubo_stage_mask & SLANG_STAGE_FRAGMENT_MASK)
      ubo_mask |= VK_SHADER_STAGE_FRAGMENT_BIT;

   if (ubo_mask != 0)
   {
      bindings.push_back({ reflection.ubo_binding,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
            ubo_mask, nullptr });
      desc_counts.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, num_sync_indices });
   }

   /* Semantic textures. */
   for (auto &semantic : reflection.semantic_textures)
   {
      for (auto &texture : semantic)
      {
         VkShaderStageFlags stages = 0;

         if (!texture.texture)
            continue;

         if (texture.stage_mask & SLANG_STAGE_VERTEX_MASK)
            stages |= VK_SHADER_STAGE_VERTEX_BIT;
         if (texture.stage_mask & SLANG_STAGE_FRAGMENT_MASK)
            stages |= VK_SHADER_STAGE_FRAGMENT_BIT;

         bindings.push_back({ texture.binding,
               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
               stages, nullptr });
         desc_counts.push_back({ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, num_sync_indices });
      }
   }

   set_layout_info.sType                  = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   set_layout_info.pNext                  = NULL;
   set_layout_info.flags                  = 0;
   set_layout_info.bindingCount           = (uint32_t)bindings.size();
   set_layout_info.pBindings              = bindings.data();

   if (vkCreateDescriptorSetLayout(device,
            &set_layout_info, NULL, &set_layout) != VK_SUCCESS)
      return false;

   layout_info.sType                      = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   layout_info.pNext                      = NULL;
   layout_info.flags                      = 0;
   layout_info.setLayoutCount             = 1;
   layout_info.pSetLayouts                = &set_layout;
   layout_info.pushConstantRangeCount     = 0;
   layout_info.pPushConstantRanges        = NULL;

   push_range.stageFlags                  = 0;
   push_range.offset                      = 0;
   push_range.size                        = 0;

   /* Push constants */
   if (reflection.push_constant_stage_mask && reflection.push_constant_size)
   {
      if (reflection.push_constant_stage_mask & SLANG_STAGE_VERTEX_MASK)
         push_range.stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
      if (reflection.push_constant_stage_mask & SLANG_STAGE_FRAGMENT_MASK)
         push_range.stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;

#ifdef VULKAN_DEBUG
      RARCH_LOG("[Vulkan]: Push Constant Block: %u bytes.\n", (unsigned int)reflection.push_constant_size);
#endif

      layout_info.pushConstantRangeCount = 1;
      layout_info.pPushConstantRanges    = &push_range;
      push.buffer.resize((reflection.push_constant_size + sizeof(uint32_t) - 1) / sizeof(uint32_t));
   }

   push.stages     = push_range.stageFlags;
   push_range.size = (uint32_t)reflection.push_constant_size;

   if (vkCreatePipelineLayout(device,
            &layout_info, NULL, &pipeline_layout) != VK_SUCCESS)
      return false;

   pool_info.sType                      = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   pool_info.pNext                      = NULL;
   pool_info.flags                      = 0;
   pool_info.maxSets                    = num_sync_indices;
   pool_info.poolSizeCount              = (uint32_t)desc_counts.size();
   pool_info.pPoolSizes                 = desc_counts.data();
   if (vkCreateDescriptorPool(device, &pool_info, nullptr, &pool) != VK_SUCCESS)
      return false;

   alloc_info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   alloc_info.pNext              = NULL;
   alloc_info.descriptorPool     = pool;
   alloc_info.descriptorSetCount = 1;
   alloc_info.pSetLayouts        = &set_layout;

   sets.resize(num_sync_indices);

   for (i = 0; i < num_sync_indices; i++)
      vkAllocateDescriptorSets(device, &alloc_info, &sets[i]);

   return true;
}

bool Pass::init_pipeline()
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
   VkPipelineViewportStateCreateInfo viewport;
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

   if (!init_pipeline_layout())
      return false;

   /* Input assembly */
   input_assembly.sType                         = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
   input_assembly.pNext                         = NULL;
   input_assembly.flags                         = 0;
   input_assembly.topology                      = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
   input_assembly.primitiveRestartEnable        = VK_FALSE;

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
   viewport.sType                               = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
   viewport.pNext                               = NULL;
   viewport.flags                               = 0;
   viewport.viewportCount                       = 1;
   viewport.pViewports                          = NULL;
   viewport.scissorCount                        = 1;
   viewport.pScissors                           = NULL;

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
   module_info.codeSize      = vertex_shader.size() * sizeof(uint32_t);
   module_info.pCode         = vertex_shader.data();
   shader_stages[0].stage    = VK_SHADER_STAGE_VERTEX_BIT;
   shader_stages[0].pName    = "main";
   vkCreateShaderModule(device, &module_info, NULL, &shader_stages[0].module);

   module_info.codeSize      = fragment_shader.size() * sizeof(uint32_t);
   module_info.pCode         = fragment_shader.data();
   shader_stages[1].stage    = VK_SHADER_STAGE_FRAGMENT_BIT;
   shader_stages[1].pName    = "main";
   vkCreateShaderModule(device, &module_info, NULL, &shader_stages[1].module);

   pipe.sType                = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
   pipe.pNext                = NULL;
   pipe.flags                = 0;
   pipe.stageCount           = 2;
   pipe.pStages              = shader_stages;
   pipe.pVertexInputState    = &vertex_input;
   pipe.pInputAssemblyState  = &input_assembly;
   pipe.pTessellationState   = NULL;
   pipe.pViewportState       = &viewport;
   pipe.pRasterizationState  = &raster;
   pipe.pMultisampleState    = &multisample;
   pipe.pDepthStencilState   = &depth_stencil;
   pipe.pColorBlendState     = &blend;
   pipe.pDynamicState        = &dynamic;
   pipe.layout               = pipeline_layout;
   pipe.renderPass           = final_pass 
	   ? swapchain_render_pass
	   : framebuffer->get_render_pass();
   pipe.subpass              = 0;
   pipe.basePipelineHandle   = VK_NULL_HANDLE;
   pipe.basePipelineIndex    = 0;

   if (vkCreateGraphicsPipelines(device,
            cache, 1, &pipe, NULL, &pipeline) != VK_SUCCESS)
   {
      vkDestroyShaderModule(device, shader_stages[0].module, NULL);
      vkDestroyShaderModule(device, shader_stages[1].module, NULL);
      return false;
   }

   vkDestroyShaderModule(device, shader_stages[0].module, NULL);
   vkDestroyShaderModule(device, shader_stages[1].module, NULL);
   return true;
}

CommonResources::CommonResources(VkDevice device,
      const VkPhysicalDeviceMemoryProperties &memory_properties)
   : device(device)
{
   void *ptr;
   unsigned i;
   VkSamplerCreateInfo info;
   /* The final pass uses an MVP designed for [0, 1] range VBO.
    * For in-between passes, we just go with identity matrices,
    * so keep it simple.
    */
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

   vbo                          = 
      std::unique_ptr<Buffer>(new Buffer(device,
               memory_properties, sizeof(vbo_data), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT));

   ptr                          = vbo->map();
   memcpy(ptr, vbo_data, sizeof(vbo_data));
   vbo->unmap();

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

      switch (static_cast<glslang_filter_chain_filter>(i))
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

         switch (static_cast<glslang_filter_chain_filter>(j))
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

            switch (static_cast<glslang_filter_chain_address>(k))
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
            vkCreateSampler(device, &info, nullptr, &samplers[i][j][k]);
         }
      }
   }
}

CommonResources::~CommonResources()
{
   for (auto &i : samplers)
      for (auto &j : i)
         for (auto &k : j)
            if (k != VK_NULL_HANDLE)
               vkDestroySampler(device, k, nullptr);
}

void Pass::allocate_buffers()
{
   if (reflection.ubo_stage_mask)
   {
      /* Align */
      common->ubo_offset = (common->ubo_offset + common->ubo_alignment - 1) &
         ~(common->ubo_alignment - 1);
      ubo_offset = common->ubo_offset;

      /* Allocate */
      common->ubo_offset += reflection.ubo_size;
   }
}

void Pass::end_frame()
{
   if (fb_feedback)
      swap(framebuffer, fb_feedback);
}

bool Pass::init_feedback()
{
   if (final_pass)
      return false;

   fb_feedback = std::unique_ptr<Framebuffer>(
         new Framebuffer(device, memory_properties,
            current_framebuffer_size,
            pass_info.rt_format, pass_info.max_levels));
   return true;
}

bool Pass::build()
{
   unsigned i;
   unsigned j = 0;
   std::unordered_map<std::string, slang_semantic_map> semantic_map;

   framebuffer.reset();
   fb_feedback.reset();

   if (!final_pass)
      framebuffer = std::unique_ptr<Framebuffer>(
            new Framebuffer(device, memory_properties,
               current_framebuffer_size,
               pass_info.rt_format, pass_info.max_levels));

   for (i = 0; i < parameters.size(); i++)
   {
      if (!slang_set_unique_map(
               semantic_map, parameters[i].id,
               slang_semantic_map{ SLANG_SEMANTIC_FLOAT_PARAMETER, j }))
         return false;
      j++;
   }

   reflection                              = slang_reflection{};
   reflection.pass_number                  = pass_number;
   reflection.texture_semantic_map         = &common->texture_semantic_map;
   reflection.texture_semantic_uniform_map = &common->texture_semantic_uniform_map;
   reflection.semantic_map                 = &semantic_map;

   if (!slang_reflect_spirv(vertex_shader, fragment_shader, &reflection))
      return false;

   /* Filter out parameters which we will never use anyways. */
   filtered_parameters.clear();

   for (i = 0; i < reflection.semantic_float_parameters.size(); i++)
   {
      if (reflection.semantic_float_parameters[i].uniform ||
          reflection.semantic_float_parameters[i].push_constant)
         filtered_parameters.push_back(parameters[i]);
   }

   return init_pipeline();
}

void Pass::set_semantic_texture(VkDescriptorSet set,
      slang_texture_semantic semantic, const Texture &texture)
{
   if (reflection.semantic_textures[semantic][0].texture)
   {
      VULKAN_PASS_SET_TEXTURE(device, set, common->samplers[texture.filter][texture.mip_filter][texture.address], reflection.semantic_textures[semantic][0].binding, texture.texture.view, texture.texture.layout);
   }
}

void Pass::set_semantic_texture_array(VkDescriptorSet set,
      slang_texture_semantic semantic, unsigned index,
      const Texture &texture)
{
   if (index < reflection.semantic_textures[semantic].size() &&
         reflection.semantic_textures[semantic][index].texture)
   {
      VULKAN_PASS_SET_TEXTURE(device, set, common->samplers[texture.filter][texture.mip_filter][texture.address],  reflection.semantic_textures[semantic][index].binding, texture.texture.view, texture.texture.layout);
   }
}

void Pass::build_semantic_texture_array_vec4(uint8_t *data, slang_texture_semantic semantic,
      unsigned index, unsigned width, unsigned height)
{
   auto &refl = reflection.semantic_textures[semantic];

   if (index >= refl.size())
      return;

   if (data && refl[index].uniform)
   {
      float *_data = reinterpret_cast<float *>(data + refl[index].ubo_offset);
      _data[0]     = (float)(width);
      _data[1]     = (float)(height);
      _data[2]     = 1.0f / (float)(width);
      _data[3]     = 1.0f / (float)(height);
   }

   if (refl[index].push_constant)
   {
      float *_data = reinterpret_cast<float *>(push.buffer.data() + (refl[index].push_constant_offset >> 2));
      _data[0]     = (float)(width);
      _data[1]     = (float)(height);
      _data[2]     = 1.0f / (float)(width);
      _data[3]     = 1.0f / (float)(height);
   }
}

void Pass::build_semantic_texture_vec4(uint8_t *data, slang_texture_semantic semantic,
      unsigned width, unsigned height)
{
   build_semantic_texture_array_vec4(data, semantic, 0, width, height);
}

void Pass::build_semantic_vec4(uint8_t *data, slang_semantic semantic,
      unsigned width, unsigned height)
{
   auto &refl = reflection.semantics[semantic];

   if (data && refl.uniform)
   {
      float *_data = reinterpret_cast<float *>(data + refl.ubo_offset);
      _data[0]     = (float)(width);
      _data[1]     = (float)(height);
      _data[2]     = 1.0f / (float)(width);
      _data[3]     = 1.0f / (float)(height);
   }

   if (refl.push_constant)
   {
      float *_data = reinterpret_cast<float *>
            (push.buffer.data() + (refl.push_constant_offset >> 2));
      _data[0]     = (float)(width);
      _data[1]     = (float)(height);
      _data[2]     = 1.0f / (float)(width);
      _data[3]     = 1.0f / (float)(height);
   }
}

void Pass::build_semantic_parameter(uint8_t *data, unsigned index, float value)
{
   auto &refl = reflection.semantic_float_parameters[index];

   /* We will have filtered out stale parameters. */
   if (data && refl.uniform)
      *reinterpret_cast<float*>(data + refl.ubo_offset) = value;

   if (refl.push_constant)
      *reinterpret_cast<float*>(push.buffer.data() + (refl.push_constant_offset >> 2)) = value;
}

void Pass::build_semantic_uint(uint8_t *data, slang_semantic semantic,
      uint32_t value)
{
   auto &refl = reflection.semantics[semantic];

   if (data && refl.uniform)
      *reinterpret_cast<uint32_t*>(data + reflection.semantics[semantic].ubo_offset) = value;

   if (refl.push_constant)
      *reinterpret_cast<uint32_t*>(push.buffer.data() + (refl.push_constant_offset >> 2)) = value;
}

void Pass::build_semantic_int(uint8_t *data, slang_semantic semantic,
                              int32_t value)
{
   auto &refl = reflection.semantics[semantic];

   if (data && refl.uniform)
      *reinterpret_cast<int32_t*>(data + reflection.semantics[semantic].ubo_offset) = value;

   if (refl.push_constant)
      *reinterpret_cast<int32_t*>(push.buffer.data() + (refl.push_constant_offset >> 2)) = value;
}

void Pass::build_semantic_texture(VkDescriptorSet set, uint8_t *buffer,
      slang_texture_semantic semantic, const Texture &texture)
{
   build_semantic_texture_vec4(buffer, semantic,
         texture.texture.width, texture.texture.height);
   set_semantic_texture(set, semantic, texture);
}

void Pass::build_semantic_texture_array(VkDescriptorSet set, uint8_t *buffer,
      slang_texture_semantic semantic, unsigned index, const Texture &texture)
{
   build_semantic_texture_array_vec4(buffer, semantic, index,
         texture.texture.width, texture.texture.height);
   set_semantic_texture_array(set, semantic, index, texture);
}

void Pass::build_semantics(VkDescriptorSet set, uint8_t *buffer,
      const float *mvp, const Texture &original, const Texture &source)
{
   unsigned i;

   /* MVP */
   if (buffer && reflection.semantics[SLANG_SEMANTIC_MVP].uniform)
   {
      size_t offset = reflection.semantics[SLANG_SEMANTIC_MVP].ubo_offset;
      if (mvp)
         memcpy(buffer + offset, mvp, sizeof(float) * 16);
      else
         build_identity_matrix(reinterpret_cast<float *>(buffer + offset));
   }

   if (reflection.semantics[SLANG_SEMANTIC_MVP].push_constant)
   {
      size_t offset = reflection.semantics[SLANG_SEMANTIC_MVP].push_constant_offset;
      if (mvp)
         memcpy(push.buffer.data() + (offset >> 2), mvp, sizeof(float) * 16);
      else
         build_identity_matrix(reinterpret_cast<float *>(push.buffer.data() + (offset >> 2)));
   }

   /* Output information */
   build_semantic_vec4(buffer, SLANG_SEMANTIC_OUTPUT,
                       current_framebuffer_size.width,
                       current_framebuffer_size.height);
   build_semantic_vec4(buffer, SLANG_SEMANTIC_FINAL_VIEWPORT,
                       unsigned(current_viewport.width),
                       unsigned(current_viewport.height));

   build_semantic_uint(buffer, SLANG_SEMANTIC_FRAME_COUNT,
                       frame_count_period 
                       ? uint32_t(frame_count % frame_count_period) 
                       : uint32_t(frame_count));

   build_semantic_int(buffer, SLANG_SEMANTIC_FRAME_DIRECTION,
                      frame_direction);

   build_semantic_uint(buffer, SLANG_SEMANTIC_TOTAL_SUBFRAMES,
                      total_subframes);

   build_semantic_uint(buffer, SLANG_SEMANTIC_CURRENT_SUBFRAME,
                      current_subframe);

   build_semantic_uint(buffer, SLANG_SEMANTIC_ROTATION,
                      rotation);

   /* Standard inputs */
   build_semantic_texture(set, buffer, SLANG_TEXTURE_SEMANTIC_ORIGINAL, original);
   build_semantic_texture(set, buffer, SLANG_TEXTURE_SEMANTIC_SOURCE, source);

   /* ORIGINAL_HISTORY[0] is an alias of ORIGINAL. */
   build_semantic_texture_array(set, buffer,
         SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, 0, original);

   /* Parameters. */
   for (i = 0; i < filtered_parameters.size(); i++)
      build_semantic_parameter(buffer,
            filtered_parameters[i].semantic_index,
            common->shader_preset->parameters[
            filtered_parameters[i].index].current);

   /* Previous inputs. */
   for (i = 0; i < common->original_history.size(); i++)
      build_semantic_texture_array(set, buffer,
            SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, i + 1,
            common->original_history[i]);

   /* Previous passes. */
   for (i = 0; i < common->pass_outputs.size(); i++)
      build_semantic_texture_array(set, buffer,
            SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, i,
            common->pass_outputs[i]);

   /* Feedback FBOs. */
   for (i = 0; i < common->fb_feedback.size(); i++)
      build_semantic_texture_array(set, buffer,
            SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, i,
            common->fb_feedback[i]);

   /* LUTs. */
   for (i = 0; i < common->luts.size(); i++)
      build_semantic_texture_array(set, buffer,
            SLANG_TEXTURE_SEMANTIC_USER, i,
            common->luts[i]->get_texture());
}

void Pass::build_commands(
      DeferredDisposer &disposer,
      VkCommandBuffer cmd,
      const Texture &original,
      const Texture &source,
      const VkViewport &vp,
      const float *mvp)
{
   uint8_t *u       = nullptr;

   current_viewport = vp;
   Size2D size      = get_output_size(
         { original.texture.width, original.texture.height },
         { source.texture.width, source.texture.height });

   if (framebuffer &&
         (size.width  != framebuffer->get_size().width ||
          size.height != framebuffer->get_size().height))
      framebuffer->set_size(disposer, size);

   current_framebuffer_size = size;

   if (reflection.ubo_stage_mask && common->ubo_mapped)
      u = common->ubo_mapped + ubo_offset +
         sync_index * common->ubo_sync_index_stride;

   build_semantics(sets[sync_index], u, mvp, original, source);

   if (reflection.ubo_stage_mask)
   {
      VULKAN_SET_UNIFORM_BUFFER(device,
            sets[sync_index],
            reflection.ubo_binding,
            common->ubo->get_buffer(),
            ubo_offset + sync_index * common->ubo_sync_index_stride,
            reflection.ubo_size);
   }

   /* The final pass is always executed inside
    * another render pass since the frontend will
    * want to overlay various things on top for
    * the passes that end up on-screen. */
   if (!final_pass)
   {
      VkRenderPassBeginInfo rp_info;

      /* Render. */
      VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,
            framebuffer->get_image(), 1,
            VK_IMAGE_LAYOUT_UNDEFINED,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            0,
            VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | 
            VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED);

      rp_info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      rp_info.pNext                    = NULL;
      rp_info.renderPass               = framebuffer->get_render_pass();
      rp_info.framebuffer              = framebuffer->get_framebuffer();
      rp_info.renderArea.offset.x      = 0;
      rp_info.renderArea.offset.y      = 0;
      rp_info.renderArea.extent.width  = current_framebuffer_size.width;
      rp_info.renderArea.extent.height = current_framebuffer_size.height;
      rp_info.clearValueCount          = 0;
      rp_info.pClearValues             = nullptr;

      vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
   }

   vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
   vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
         pipeline_layout,
         0, 1, &sets[sync_index], 0, nullptr);

   if (push.stages != 0)
   {
      vkCmdPushConstants(cmd, pipeline_layout,
            push.stages, 0, (uint32_t)reflection.push_constant_size,
            push.buffer.data());
   }

   {
      VkDeviceSize offset = final_pass ? 16 * sizeof(float) : 0;
      vkCmdBindVertexBuffers(cmd, 0, 1,
            &common->vbo->get_buffer(),
            &offset);
   }

   if (final_pass)
   {
      vkCmdSetViewport(cmd, 0, 1, &current_viewport);

#ifdef VULKAN_ROLLING_SCANLINE_SIMULATION
      if (simulate_scanline)
      {
         const VkRect2D sci = {
            {
               int32_t(current_viewport.x),
               int32_t((current_viewport.height / float(total_subframes)) 
                        * float(current_subframe - 1))
            },
            {
               uint32_t(current_viewport.width),
               uint32_t(current_viewport.height / float(total_subframes))
            },
         };
         vkCmdSetScissor(cmd, 0, 1, &sci);
      }
      else
#endif // VULKAN_ROLLING_SCANLINE_SIMULATION 
      {
         const VkRect2D sci = {
            {
               int32_t(current_viewport.x),
               int32_t(current_viewport.y)
            },
            {
               uint32_t(current_viewport.width),
               uint32_t(current_viewport.height)
            },
         };
         vkCmdSetScissor(cmd, 0, 1, &sci);
      }     
   }
   else
   {
      const VkViewport _vp = {
         0.0f, 0.0f,
         float(current_framebuffer_size.width),
         float(current_framebuffer_size.height),
         0.0f, 1.0f
      };

      vkCmdSetViewport(cmd, 0, 1, &_vp);

#ifdef VULKAN_ROLLING_SCANLINE_SIMULATION
      if (simulate_scanline)
      {
         const VkRect2D sci = {
            {
               0,
               int32_t((float(current_framebuffer_size.height) / float(total_subframes)) 
                        * float(current_subframe - 1))
            },
            {
               uint32_t(current_framebuffer_size.width),
               uint32_t(float(current_framebuffer_size.height) / float(total_subframes))
            },
         };
         vkCmdSetScissor(cmd, 0, 1, &sci);
      }
      else
#endif // VULKAN_ROLLING_SCANLINE_SIMULATION
      {
         const VkRect2D sci = {
            { 0, 0 },
            {
               current_framebuffer_size.width,
               current_framebuffer_size.height
            },
         };
         vkCmdSetScissor(cmd, 0, 1, &sci);
      }      
   }

   vkCmdDraw(cmd, 4, 1, 0, 0);

   if (!final_pass)
   {
      vkCmdEndRenderPass(cmd);

      if (framebuffer->get_levels() > 1)
         vulkan_framebuffer_generate_mips(
               framebuffer->get_framebuffer(),
               framebuffer->get_image(),
               framebuffer->get_size(),
               cmd,
               framebuffer->get_levels());
      else
      {
         /* Barrier to sync with next pass. */
         VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(
               cmd,
               framebuffer->get_image(),
               VK_REMAINING_MIP_LEVELS,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
               VK_ACCESS_SHADER_READ_BIT,
               VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
               VK_QUEUE_FAMILY_IGNORED,
               VK_QUEUE_FAMILY_IGNORED);
      }
   }
}

Framebuffer::Framebuffer(
      VkDevice device,
      const VkPhysicalDeviceMemoryProperties &mem_props,
      const Size2D &max_size, VkFormat format,
      unsigned max_levels) :
   size(max_size),
   format(format),
   max_levels(std::max(max_levels, 1u)),
   memory_properties(mem_props),
   device(device)
{
   RARCH_LOG("[Vulkan filter chain]: Creating framebuffer %ux%u (max %u level(s)).\n",
         max_size.width, max_size.height, max_levels);
   vulkan_initialize_render_pass(device, format, &render_pass);
   init(nullptr);
}

void Framebuffer::init(DeferredDisposer *disposer)
{
   VkFramebufferCreateInfo fb_info;
   VkMemoryRequirements mem_reqs;
   VkImageCreateInfo info;
   VkMemoryAllocateInfo alloc;
   VkImageViewCreateInfo view_info;

   info.sType               = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   info.pNext               = NULL;
   info.flags               = 0;
   info.imageType           = VK_IMAGE_TYPE_2D;
   info.format              = format;
   info.extent.width        = size.width;
   info.extent.height       = size.height;
   info.extent.depth        = 1;
   info.mipLevels           = std::min(max_levels,
         glslang_num_miplevels(size.width, size.height));
   info.arrayLayers         = 1;
   info.samples             = VK_SAMPLE_COUNT_1_BIT;
   info.tiling              = VK_IMAGE_TILING_OPTIMAL;
   info.usage               = VK_IMAGE_USAGE_SAMPLED_BIT
                            | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                            | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
   info.sharingMode         = VK_SHARING_MODE_EXCLUSIVE;
   info.pQueueFamilyIndices = NULL;
   info.initialLayout       = VK_IMAGE_LAYOUT_UNDEFINED;
   levels                   = info.mipLevels;

   vkCreateImage(device, &info, nullptr, &image);
   vulkan_debug_mark_image(device, image);

   vkGetImageMemoryRequirements(device, image, &mem_reqs);

   alloc.sType            = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.pNext            = NULL;
   alloc.allocationSize   = mem_reqs.size;
   alloc.memoryTypeIndex  = find_memory_type_fallback(
         memory_properties, mem_reqs.memoryTypeBits,
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

   /* Can reuse already allocated memory. */
   if (memory.size < mem_reqs.size || memory.type != alloc.memoryTypeIndex)
   {
      /* Memory might still be in use since we don't want
       * to totally stall
       * the world for framebuffer recreation. */
      if (memory.memory != VK_NULL_HANDLE && disposer)
      {
         VkDevice       d = device;
         VkDeviceMemory m = memory.memory;
         disposer->defer([=] { vkFreeMemory(d, m, nullptr); });
      }

      memory.type = alloc.memoryTypeIndex;
      memory.size = mem_reqs.size;

      vkAllocateMemory(device, &alloc, nullptr, &memory.memory);
      vulkan_debug_mark_memory(device, memory.memory);
   }

   vkBindImageMemory(device, image, memory.memory, 0);

   view_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view_info.pNext                           = NULL;
   view_info.flags                           = 0;
   view_info.image                           = image;
   view_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
   view_info.format                          = format;
   view_info.components.r                    = VK_COMPONENT_SWIZZLE_R;
   view_info.components.g                    = VK_COMPONENT_SWIZZLE_G;
   view_info.components.b                    = VK_COMPONENT_SWIZZLE_B;
   view_info.components.a                    = VK_COMPONENT_SWIZZLE_A;
   view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   view_info.subresourceRange.baseMipLevel   = 0;
   view_info.subresourceRange.levelCount     = levels;
   view_info.subresourceRange.baseArrayLayer = 0;
   view_info.subresourceRange.layerCount     = 1;

   vkCreateImageView(device, &view_info, nullptr, &view);
   view_info.subresourceRange.levelCount     = 1;
   vkCreateImageView(device, &view_info, nullptr, &fb_view);

   /* Initialize framebuffer */
   fb_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
   fb_info.pNext           = NULL;
   fb_info.flags           = 0;
   fb_info.renderPass      = render_pass;
   fb_info.attachmentCount = 1;
   fb_info.pAttachments    = &fb_view;
   fb_info.width           = size.width;
   fb_info.height          = size.height;
   fb_info.layers          = 1;

   vkCreateFramebuffer(device, &fb_info, nullptr, &framebuffer);
}

void Framebuffer::set_size(DeferredDisposer &disposer, const Size2D &size, VkFormat format)
{
   this->size = size;
   if (format != VK_FORMAT_UNDEFINED)
	  this->format = format;

   RARCH_LOG("[Vulkan filter chain]: Updating framebuffer size %ux%u (format: %u).\n",
         size.width, size.height, (unsigned)this->format);

   {
      /* The current framebuffers, etc, might still be in use
       * so defer deletion.
       * We'll most likely be able to reuse the memory,
       * so don't free it here.
       *
       * Fake lambda init captures for C++11.
       */
      VkDevice d       = device;
      VkImage i        = image;
      VkImageView v    = view;
      VkImageView fbv  = fb_view;
      VkFramebuffer fb = framebuffer;
      disposer.defer([=]
      {
         if (fb != VK_NULL_HANDLE)
            vkDestroyFramebuffer(d, fb, nullptr);
         if (v != VK_NULL_HANDLE)
            vkDestroyImageView(d, v, nullptr);
         if (fbv != VK_NULL_HANDLE)
            vkDestroyImageView(d, fbv, nullptr);
         if (i != VK_NULL_HANDLE)
            vkDestroyImage(d, i, nullptr);
      });
   }

   init(&disposer);
}

Framebuffer::~Framebuffer()
{
   if (framebuffer != VK_NULL_HANDLE)
      vkDestroyFramebuffer(device, framebuffer, nullptr);
   if (render_pass != VK_NULL_HANDLE)
      vkDestroyRenderPass(device, render_pass, nullptr);
   if (view != VK_NULL_HANDLE)
      vkDestroyImageView(device, view, nullptr);
   if (fb_view != VK_NULL_HANDLE)
      vkDestroyImageView(device, fb_view, nullptr);
   if (image != VK_NULL_HANDLE)
      vkDestroyImage(device, image, nullptr);
   if (memory.memory != VK_NULL_HANDLE)
      vkFreeMemory(device, memory.memory, nullptr);
}

/* C glue */
vulkan_filter_chain_t *vulkan_filter_chain_new(
      const vulkan_filter_chain_create_info *info)
{
   return new vulkan_filter_chain(*info);
}

vulkan_filter_chain_t *vulkan_filter_chain_create_default(
      const struct vulkan_filter_chain_create_info *info,
      glslang_filter_chain_filter filter)
{
   struct vulkan_filter_chain_pass_info pass_info;
   auto tmpinfo            = *info;

   tmpinfo.num_passes      = 1;

   std::unique_ptr<vulkan_filter_chain> chain{ new vulkan_filter_chain(tmpinfo) };
   if (!chain)
      return nullptr;

   pass_info.scale_type_x  = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
   pass_info.scale_type_y  = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
   pass_info.scale_x       = 1.0f;
   pass_info.scale_y       = 1.0f;
   pass_info.rt_format     = tmpinfo.swapchain.format;
   pass_info.source_filter = filter;
   pass_info.mip_filter    = GLSLANG_FILTER_CHAIN_NEAREST;
   pass_info.address       = GLSLANG_FILTER_CHAIN_ADDRESS_CLAMP_TO_EDGE;
   pass_info.max_levels    = 0;

   chain->set_pass_info(0, pass_info);

   chain->set_shader(0, VK_SHADER_STAGE_VERTEX_BIT,
         opaque_vert,
         sizeof(opaque_vert) / sizeof(uint32_t));
   chain->set_shader(0, VK_SHADER_STAGE_FRAGMENT_BIT,
         opaque_frag,
         sizeof(opaque_frag) / sizeof(uint32_t));

   if (!chain->init())
      return nullptr;

   return chain.release();
}

vulkan_filter_chain_t *vulkan_filter_chain_create_from_preset(
      const struct vulkan_filter_chain_create_info *info,
      const char *path, glslang_filter_chain_filter filter)
{
   unsigned i;
   std::unique_ptr<video_shader> shader{ new video_shader() };

   if (!shader)
      return nullptr;

    if (!video_shader_load_preset_into_shader(path, shader.get()))
        return nullptr;

   bool last_pass_is_fbo = shader->pass[shader->passes - 1].fbo.flags &
      FBO_SCALE_FLAG_VALID;
   auto tmpinfo          = *info;
   tmpinfo.num_passes    = shader->passes + (last_pass_is_fbo ? 1 : 0);

   std::unique_ptr<vulkan_filter_chain> chain{ new vulkan_filter_chain(tmpinfo) };
   if (!chain)
      goto error;

   if (shader->luts && !vulkan_filter_chain_load_luts(info, chain.get(), shader.get()))
      goto error;

   shader->num_parameters = 0;

   for (i = 0; i < shader->passes; i++)
   {
      glslang_output output;
      struct vulkan_filter_chain_pass_info pass_info;
      const video_shader_pass *pass      = &shader->pass[i];
      const video_shader_pass *next_pass =
         i + 1 < shader->passes ? &shader->pass[i + 1] : nullptr;

      pass_info.scale_type_x  = GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL;
      pass_info.scale_type_y  = GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL;
      pass_info.scale_x       = 0.0f;
      pass_info.scale_y       = 0.0f;
      pass_info.rt_format     = VK_FORMAT_UNDEFINED;
      pass_info.source_filter = GLSLANG_FILTER_CHAIN_LINEAR;
      pass_info.mip_filter    = GLSLANG_FILTER_CHAIN_LINEAR;
      pass_info.address       = GLSLANG_FILTER_CHAIN_ADDRESS_REPEAT;
      pass_info.max_levels    = 0;

      if (!glslang_compile_shader(pass->source.path, &output))
      {
         RARCH_ERR("[Vulkan]: Failed to compile shader: \"%s\".\n",
               pass->source.path);
         goto error;
      }

      for (auto &meta_param : output.meta.parameters)
      {
         if (shader->num_parameters >= GFX_MAX_PARAMETERS)
         {
            RARCH_ERR("[Vulkan]: Exceeded maximum number of parameters (%u).\n", GFX_MAX_PARAMETERS);
            goto error;
         }

         auto itr = std::find_if(shader->parameters, shader->parameters + shader->num_parameters,
               [&](const video_shader_parameter &param)
               {
                  return meta_param.id == param.id;
               });

         if (itr != shader->parameters + shader->num_parameters)
         {
            /* Allow duplicate #pragma parameter, but
             * only if they are exactly the same. */
            if (meta_param.desc    != itr->desc ||
                meta_param.initial != itr->initial ||
                meta_param.minimum != itr->minimum ||
                meta_param.maximum != itr->maximum ||
                meta_param.step    != itr->step)
            {
               RARCH_ERR("[Vulkan]: Duplicate parameters found for \"%s\", but arguments do not match.\n",
                     itr->id);
               goto error;
            }
            chain->add_parameter(i, (unsigned)(itr - shader->parameters), meta_param.id);
         }
         else
         {
            video_shader_parameter *param = &shader->parameters[shader->num_parameters];
            strlcpy(param->id, meta_param.id.c_str(), sizeof(param->id));
            strlcpy(param->desc, meta_param.desc.c_str(), sizeof(param->desc));
            param->initial = meta_param.initial;
            param->minimum = meta_param.minimum;
            param->maximum = meta_param.maximum;
            param->step    = meta_param.step;
            chain->add_parameter(i, shader->num_parameters, meta_param.id);
            shader->num_parameters++;
         }
      }

      chain->set_shader(i,
            VK_SHADER_STAGE_VERTEX_BIT,
            output.vertex.data(),
            output.vertex.size());

      chain->set_shader(i,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            output.fragment.data(),
            output.fragment.size());

      chain->set_frame_count_period(i, pass->frame_count_mod);

      if (!output.meta.name.empty())
         chain->set_pass_name(i, output.meta.name.c_str());

      /* Preset overrides. */
      if (*pass->alias)
         chain->set_pass_name(i, pass->alias);

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

      bool explicit_format         = output.meta.rt_format != SLANG_FORMAT_UNKNOWN;

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
            pass_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
            pass_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;

            /* Always inherit swapchain format. */
            pass_info.rt_format  = tmpinfo.swapchain.format;
            VkFormat pass_format = glslang_format_to_vk(output.meta.rt_format);

            /* If final pass explicitly emits RGB10, consider it HDR color space. */
            if (explicit_format && vulkan_is_hdr10_format(pass_format))
               chain->set_hdr10();

            if (explicit_format && pass_format != pass_info.rt_format)
            {
               RARCH_WARN("[slang]: Using explicit format for last pass in chain,"
                     " but it is not rendered to framebuffer, using swapchain format instead.\n");
            }
         }
         else
         {
            pass_info.rt_format    = glslang_format_to_vk(
                  output.meta.rt_format);
            RARCH_LOG("[slang]: Using render target format %s for pass output #%u.\n",
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

         pass_info.rt_format      = glslang_format_to_vk(output.meta.rt_format);

         RARCH_LOG("[slang]: Using render target format %s for pass output #%u.\n",
               glslang_format_to_string(output.meta.rt_format), i);

         switch (pass->fbo.type_x)
         {
            case RARCH_SCALE_INPUT:
               pass_info.scale_x      = pass->fbo.scale_x;
               pass_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
               break;

            case RARCH_SCALE_ABSOLUTE:
               pass_info.scale_x      = float(pass->fbo.abs_x);
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
               pass_info.scale_y      = float(pass->fbo.abs_y);
               pass_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE;
               break;

            case RARCH_SCALE_VIEWPORT:
               pass_info.scale_y      = pass->fbo.scale_y;
               pass_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
               break;
         }
      }

      chain->set_pass_info(i, pass_info);
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

      chain->set_pass_info(shader->passes, pass_info);

      chain->set_shader(shader->passes,
            VK_SHADER_STAGE_VERTEX_BIT,
            opaque_vert,
            sizeof(opaque_vert) / sizeof(uint32_t));

      chain->set_shader(shader->passes,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            opaque_frag,
            sizeof(opaque_frag) / sizeof(uint32_t));
   }

   chain->set_shader_preset(std::move(shader));

   if (!chain->init())
      goto error;

   return chain.release();

error:
   return nullptr;
}

struct video_shader *vulkan_filter_chain_get_preset(
      vulkan_filter_chain_t *chain)
{
   return chain->get_shader_preset();
}

void vulkan_filter_chain_free(
      vulkan_filter_chain_t *chain)
{
   delete chain;
}

void vulkan_filter_chain_set_shader(
      vulkan_filter_chain_t *chain,
      unsigned pass,
      VkShaderStageFlags stage,
      const uint32_t *spirv,
      size_t spirv_words)
{
   chain->set_shader(pass, stage, spirv, spirv_words);
}

void vulkan_filter_chain_set_pass_info(
      vulkan_filter_chain_t *chain,
      unsigned pass,
      const struct vulkan_filter_chain_pass_info *info)
{
   chain->set_pass_info(pass, *info);
}

VkFormat vulkan_filter_chain_get_pass_rt_format(
      vulkan_filter_chain_t *chain,
      unsigned pass)
{
   return chain->get_pass_rt_format(pass);
}

bool vulkan_filter_chain_update_swapchain_info(
      vulkan_filter_chain_t *chain,
      const vulkan_filter_chain_swapchain_info *info)
{
   return chain->update_swapchain_info(*info);
}

void vulkan_filter_chain_notify_sync_index(
      vulkan_filter_chain_t *chain,
      unsigned index)
{
   chain->notify_sync_index(index);
}

bool vulkan_filter_chain_init(vulkan_filter_chain_t *chain)
{
   return chain->init();
}

void vulkan_filter_chain_set_input_texture(
      vulkan_filter_chain_t *chain,
      const struct vulkan_filter_chain_texture *texture)
{
   chain->set_input_texture(*texture);
}

void vulkan_filter_chain_set_frame_count(
      vulkan_filter_chain_t *chain,
      uint64_t count)
{
   chain->set_frame_count(count);
}

void vulkan_filter_chain_set_frame_count_period(
      vulkan_filter_chain_t *chain,
      unsigned pass,
      unsigned period)
{
   chain->set_frame_count_period(pass, period);
}

void vulkan_filter_chain_set_shader_subframes(
      vulkan_filter_chain_t *chain,
      uint32_t tot_subframes)
{
   chain->set_shader_subframes(tot_subframes);
}

void vulkan_filter_chain_set_current_shader_subframe(
      vulkan_filter_chain_t *chain,
      uint32_t cur_subframe)
{
   chain->set_current_shader_subframe(cur_subframe);
}

#ifdef VULKAN_ROLLING_SCANLINE_SIMULATION
void vulkan_filter_chain_set_simulate_scanline(
      vulkan_filter_chain_t *chain,
      bool simulate_scanline)
{
   chain->set_simulate_scanline(simulate_scanline);
}
#endif // VULKAN_ROLLING_SCANLINE_SIMULATION

void vulkan_filter_chain_set_frame_direction(
      vulkan_filter_chain_t *chain,
      int32_t direction)
{
   chain->set_frame_direction(direction);
}

void vulkan_filter_chain_set_rotation(
      vulkan_filter_chain_t *chain,
      uint32_t rot)
{
   chain->set_rotation(rot);
}

void vulkan_filter_chain_set_pass_name(
      vulkan_filter_chain_t *chain,
      unsigned pass,
      const char *name)
{
   chain->set_pass_name(pass, name);
}

void vulkan_filter_chain_build_offscreen_passes(
      vulkan_filter_chain_t *chain,
      VkCommandBuffer cmd, const VkViewport *vp)
{
   chain->build_offscreen_passes(cmd, *vp);
}

void vulkan_filter_chain_build_viewport_pass(
      vulkan_filter_chain_t *chain,
      VkCommandBuffer cmd, const VkViewport *vp, const float *mvp)
{
   chain->build_viewport_pass(cmd, *vp, mvp);
}

void vulkan_filter_chain_end_frame(
      vulkan_filter_chain_t *chain,
      VkCommandBuffer cmd)
{
   chain->end_frame(cmd);
}

bool vulkan_filter_chain_emits_hdr10(vulkan_filter_chain_t *chain)
{
   return chain->emits_hdr10();
}
