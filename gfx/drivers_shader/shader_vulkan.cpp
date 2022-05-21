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
      std::vector<std::function<void ()>> &calls;
};

class Buffer
{
   public:
      Buffer(VkDevice device,
            const VkPhysicalDeviceMemoryProperties &mem_props,
            size_t size, VkBufferUsageFlags usage);
      ~Buffer();

      Buffer(Buffer&&) = delete;
      void operator=(Buffer&&) = delete;

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
      {}

      ~Pass();

      Pass(Pass&&) = delete;
      void operator=(Pass&&) = delete;

      VkDevice device;
      const VkPhysicalDeviceMemoryProperties &memory_properties;
      VkPipelineCache cache;
      unsigned num_sync_indices;
      unsigned sync_index;
      bool final_pass;

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

      slang_reflection reflection;

      uint64_t frame_count        = 0;
      int32_t frame_direction     = 1;
      unsigned frame_count_period = 0;
      unsigned pass_number        = 0;

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
      bool require_clear = false;
};

static bool vulkan_pass_build(Pass *pass)
{
   unsigned i;
   unsigned j = 0;
   std::unordered_map<std::string, slang_semantic_map> semantic_map;

   pass->framebuffer.reset();
   pass->fb_feedback.reset();

   if (!pass->final_pass)
      pass->framebuffer = std::unique_ptr<Framebuffer>(
            new Framebuffer(pass->device,
               pass->memory_properties,
               pass->current_framebuffer_size,
               pass->pass_info.rt_format,
               pass->pass_info.max_levels));

   for (i = 0; i < pass->parameters.size(); i++)
   {
      if (!slang_set_unique_map(
               semantic_map, pass->parameters[i].id,
               slang_semantic_map{ SLANG_SEMANTIC_FLOAT_PARAMETER, j }))
         return false;
      j++;
   }

   pass->reflection                              = slang_reflection{};
   pass->reflection.pass_number                  = pass->pass_number;
   pass->reflection.texture_semantic_map         = 
      &pass->common->texture_semantic_map;
   pass->reflection.texture_semantic_uniform_map = 
      &pass->common->texture_semantic_uniform_map;
   pass->reflection.semantic_map                 = &semantic_map;

   if (!slang_reflect_spirv(
            pass->vertex_shader,
            pass->fragment_shader, &pass->reflection))
      return false;

   /* Filter out parameters which we will never use anyways. */
   pass->filtered_parameters.clear();

   for (i = 0; 
         i < pass->reflection.semantic_float_parameters.size(); i++)
   {
      if (pass->reflection.semantic_float_parameters[i].uniform ||
          pass->reflection.semantic_float_parameters[i].push_constant)
         pass->filtered_parameters.push_back(pass->parameters[i]);
   }

   VkPipelineInputAssemblyStateCreateInfo input_assembly = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
   VkVertexInputAttributeDescription attributes[2]       = {{0}};
   VkVertexInputBindingDescription binding               = {0};
   VkPipelineVertexInputStateCreateInfo vertex_input     = {
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
   VkPipelineRasterizationStateCreateInfo raster         = {
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
   VkPipelineColorBlendAttachmentState blend_attachment  = {0};
   VkPipelineColorBlendStateCreateInfo blend             = {
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
   VkPipelineViewportStateCreateInfo viewport            = {
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
   VkPipelineDepthStencilStateCreateInfo depth_stencil   = {
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
   VkPipelineMultisampleStateCreateInfo multisample      = {
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
   VkPipelineDynamicStateCreateInfo dynamic              = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
   static const VkDynamicState dynamics[]                = {
      VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
   VkPipelineShaderStageCreateInfo shader_stages[2]      = {
      { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
      { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
   };
   VkShaderModuleCreateInfo module_info                  = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
   VkGraphicsPipelineCreateInfo pipe                     = {
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
   std::vector<VkDescriptorSetLayoutBinding> bindings;
   std::vector<VkDescriptorPoolSize> desc_counts;
   VkPushConstantRange push_range                  = {};
   VkDescriptorSetLayoutCreateInfo set_layout_info = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
   VkPipelineLayoutCreateInfo layout_info          = {
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
   VkDescriptorPoolCreateInfo pool_info            = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
   VkDescriptorSetAllocateInfo alloc_info          = {
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };

   /* Main UBO. */
   VkShaderStageFlags ubo_mask                     = 0;

   if (pass->reflection.ubo_stage_mask & SLANG_STAGE_VERTEX_MASK)
      ubo_mask |= VK_SHADER_STAGE_VERTEX_BIT;
   if (pass->reflection.ubo_stage_mask & SLANG_STAGE_FRAGMENT_MASK)
      ubo_mask |= VK_SHADER_STAGE_FRAGMENT_BIT;

   if (ubo_mask != 0)
   {
      bindings.push_back({ pass->reflection.ubo_binding,
            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
            ubo_mask, nullptr });
      desc_counts.push_back({ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            pass->num_sync_indices });
   }

   /* Semantic textures. */
   for (auto &semantic : pass->reflection.semantic_textures)
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
         desc_counts.push_back({ 
               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
               pass->num_sync_indices });
      }
   }

   set_layout_info.bindingCount           = bindings.size();
   set_layout_info.pBindings              = bindings.data();

   if (vkCreateDescriptorSetLayout(pass->device,
            &set_layout_info, NULL, &pass->set_layout) != VK_SUCCESS)
      return false;

   layout_info.setLayoutCount             = 1;
   layout_info.pSetLayouts                = &pass->set_layout;

   /* Push constants */
   if (     pass->reflection.push_constant_stage_mask 
         && pass->reflection.push_constant_size)
   {
      if (pass->reflection.push_constant_stage_mask 
            & SLANG_STAGE_VERTEX_MASK)
         push_range.stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
      if (pass->reflection.push_constant_stage_mask 
            & SLANG_STAGE_FRAGMENT_MASK)
         push_range.stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;

#ifdef VULKAN_DEBUG
      RARCH_LOG("[Vulkan]: Push Constant Block: %u bytes.\n",
            (unsigned int)pass->reflection.push_constant_size);
#endif

      layout_info.pushConstantRangeCount = 1;
      layout_info.pPushConstantRanges    = &push_range;
      pass->push.buffer.resize((pass->reflection.push_constant_size 
               + sizeof(uint32_t) - 1) / sizeof(uint32_t));
   }

   pass->push.stages     = push_range.stageFlags;
   push_range.size       = pass->reflection.push_constant_size;

   if (vkCreatePipelineLayout(pass->device,
            &layout_info, NULL, &pass->pipeline_layout) != VK_SUCCESS)
      return false;

   pool_info.maxSets                    = pass->num_sync_indices;
   pool_info.poolSizeCount              = desc_counts.size();
   pool_info.pPoolSizes                 = desc_counts.data();
   if (vkCreateDescriptorPool(pass->device,
            &pool_info, nullptr, &pass->pool) != VK_SUCCESS)
      return false;

   alloc_info.descriptorPool            = pass->pool;
   alloc_info.descriptorSetCount        = 1;
   alloc_info.pSetLayouts               = &pass->set_layout;

   pass->sets.resize(pass->num_sync_indices);

   for (i = 0; i < pass->num_sync_indices; i++)
      vkAllocateDescriptorSets(pass->device,
            &alloc_info, &pass->sets[i]);

   /* Input assembly */
   input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

   /* VAO state */
   attributes[0].location  = 0;
   attributes[0].binding   = 0;
   attributes[0].format    = VK_FORMAT_R32G32_SFLOAT;
   attributes[0].offset    = 0;
   attributes[1].location  = 1;
   attributes[1].binding   = 0;
   attributes[1].format    = VK_FORMAT_R32G32_SFLOAT;
   attributes[1].offset    = 2 * sizeof(float);

   binding.binding         = 0;
   binding.stride          = 4 * sizeof(float);
   binding.inputRate       = VK_VERTEX_INPUT_RATE_VERTEX;

   vertex_input.vertexBindingDescriptionCount   = 1;
   vertex_input.pVertexBindingDescriptions      = &binding;
   vertex_input.vertexAttributeDescriptionCount = 2;
   vertex_input.pVertexAttributeDescriptions    = attributes;

   /* Raster state */
   raster.polygonMode                           = VK_POLYGON_MODE_FILL;
   raster.cullMode                              = VK_CULL_MODE_NONE;
   raster.frontFace                             = 
      VK_FRONT_FACE_COUNTER_CLOCKWISE;
   raster.depthClampEnable                      = false;
   raster.rasterizerDiscardEnable               = false;
   raster.depthBiasEnable                       = false;
   raster.lineWidth                             = 1.0f;

   /* Blend state */
   blend_attachment.blendEnable                 = false;
   blend_attachment.colorWriteMask              = 0xf;
   blend.attachmentCount                        = 1;
   blend.pAttachments                           = &blend_attachment;

   /* Viewport state */
   viewport.viewportCount                       = 1;
   viewport.scissorCount                        = 1;

   /* Depth-stencil state */
   depth_stencil.depthTestEnable                = false;
   depth_stencil.depthWriteEnable               = false;
   depth_stencil.depthBoundsTestEnable          = false;
   depth_stencil.stencilTestEnable              = false;
   depth_stencil.minDepthBounds                 = 0.0f;
   depth_stencil.maxDepthBounds                 = 1.0f;

   /* Multisample state */
   multisample.rasterizationSamples             = 
      VK_SAMPLE_COUNT_1_BIT;

   /* Dynamic state */
   dynamic.pDynamicStates    = dynamics;
   dynamic.dynamicStateCount = ARRAY_SIZE(dynamics);

   /* Shaders */
   module_info.codeSize      = pass->vertex_shader.size() 
      * sizeof(uint32_t);
   module_info.pCode         = pass->vertex_shader.data();
   shader_stages[0].stage    = VK_SHADER_STAGE_VERTEX_BIT;
   shader_stages[0].pName    = "main";
   vkCreateShaderModule(pass->device, &module_info, NULL,
         &shader_stages[0].module);

   module_info.codeSize     = pass->fragment_shader.size() 
      * sizeof(uint32_t);
   module_info.pCode        = pass->fragment_shader.data();
   shader_stages[1].stage   = VK_SHADER_STAGE_FRAGMENT_BIT;
   shader_stages[1].pName   = "main";
   vkCreateShaderModule(pass->device, &module_info, NULL,
         &shader_stages[1].module);

   pipe.stageCount          = 2;
   pipe.pStages             = shader_stages;
   pipe.pVertexInputState   = &vertex_input;
   pipe.pInputAssemblyState = &input_assembly;
   pipe.pRasterizationState = &raster;
   pipe.pColorBlendState    = &blend;
   pipe.pMultisampleState   = &multisample;
   pipe.pViewportState      = &viewport;
   pipe.pDepthStencilState  = &depth_stencil;
   pipe.pDynamicState       = &dynamic;
   pipe.renderPass          = pass->final_pass 
      ? pass->swapchain_render_pass
      : pass->framebuffer->render_pass;
   pipe.layout              = pass->pipeline_layout;

   if (vkCreateGraphicsPipelines(pass->device,
            pass->cache, 1, &pipe,
            NULL, &pass->pipeline) != VK_SUCCESS)
   {
      vkDestroyShaderModule(pass->device,
            shader_stages[0].module, NULL);
      vkDestroyShaderModule(pass->device,
            shader_stages[1].module, NULL);
      return false;
   }

   vkDestroyShaderModule(pass->device, shader_stages[0].module, NULL);
   vkDestroyShaderModule(pass->device, shader_stages[1].module, NULL);
   return true;
}

static void vulkan_pass_build_semantic_texture(Pass *pass,
      VkDescriptorSet set, uint8_t *buffer,
      slang_texture_semantic semantic, const Texture &texture)
{
   unsigned width  = texture.texture.width;
   unsigned height = texture.texture.height;
   unsigned index  = 0;
   auto &refl      = pass->reflection.semantic_textures[semantic];

   if (index < refl.size())
   {
      if (buffer && refl[index].uniform)
      {
         float *_data = reinterpret_cast<float *>(buffer + refl[index].ubo_offset);
         _data[0]     = (float)(width);
         _data[1]     = (float)(height);
         _data[2]     = 1.0f / (float)(width);
         _data[3]     = 1.0f / (float)(height);
      }
      if (refl[index].push_constant)
      {
         float *_data = reinterpret_cast<float *>(pass->push.buffer.data() + (refl[index].push_constant_offset >> 2));
         _data[0]     = (float)(width);
         _data[1]     = (float)(height);
         _data[2]     = 1.0f / (float)(width);
         _data[3]     = 1.0f / (float)(height);
      }
   }

   if (pass->reflection.semantic_textures[semantic][0].texture)
   {
      VULKAN_PASS_SET_TEXTURE(
            pass->device,
            set,
            pass->common->samplers[texture.filter][texture.mip_filter][texture.address],
            pass->reflection.semantic_textures[semantic][0].binding,
            texture.texture.view,
            texture.texture.layout);
   }
}

static void vulkan_pass_build_semantic_texture_array(Pass *pass,
      VkDescriptorSet set, uint8_t *buffer,
      slang_texture_semantic semantic, unsigned index, const Texture &texture)
{
   auto &refl      = pass->reflection.semantic_textures[semantic];
   unsigned width  = texture.texture.width;
   unsigned height = texture.texture.height;

   if (index < refl.size())
   {
      if (buffer && refl[index].uniform)
      {
         float *_data = reinterpret_cast<float *>(buffer + refl[index].ubo_offset);
         _data[0]     = (float)(width);
         _data[1]     = (float)(height);
         _data[2]     = 1.0f / (float)(width);
         _data[3]     = 1.0f / (float)(height);
      }
      if (refl[index].push_constant)
      {
         float *_data = reinterpret_cast<float *>(pass->push.buffer.data() + (refl[index].push_constant_offset >> 2));
         _data[0]     = (float)(width);
         _data[1]     = (float)(height);
         _data[2]     = 1.0f / (float)(width);
         _data[3]     = 1.0f / (float)(height);
      }
   }

   if (index < pass->reflection.semantic_textures[semantic].size() &&
         pass->reflection.semantic_textures[semantic][index].texture)
   {
      VULKAN_PASS_SET_TEXTURE(
            pass->device,
            set,
            pass->common->samplers[texture.filter][texture.mip_filter][texture.address],
            pass->reflection.semantic_textures[semantic][index].binding,
            texture.texture.view,
            texture.texture.layout);
   }
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
   barriers[0].subresourceRange.baseArrayLayer = 0;
   barriers[1].subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

   vkCmdPipelineBarrier(cmd,
         VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         false,
         0,
         NULL,
         0,
         NULL,
         2,
         barriers);

   /* First pass */
   {
      VkImageBlit blit_region                   = {{0}};
      unsigned src_width                        = MAX(size.width,       1u);
      unsigned src_height                       = MAX(size.height,      1u);
      unsigned target_width                     = MAX(size.width  >> 1, 1u);
      unsigned target_height                    = MAX(size.height >> 1, 1u);

      blit_region.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      blit_region.srcSubresource.mipLevel       = 0;
      blit_region.srcSubresource.baseArrayLayer = 0;
      blit_region.srcSubresource.layerCount     = 1;
      blit_region.dstSubresource                = blit_region.srcSubresource;
      blit_region.dstSubresource.mipLevel       = 1;
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

   /* For subsequent passes, we have to transition
    * from DST_OPTIMAL to SRC_OPTIMAL,
    * but only do so one mip-level at a time. */

   for (i = 2; i < levels; i++)
   {
      unsigned src_width, src_height, target_width, target_height;
      VkImageBlit blit_region                   = {{0}};

      barriers[0].srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
      barriers[0].dstAccessMask                 = VK_ACCESS_TRANSFER_READ_BIT;
      barriers[0].subresourceRange.baseMipLevel = i - 1;
      barriers[0].subresourceRange.levelCount   = 1;
      barriers[0].oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
      barriers[0].newLayout                     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

      vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            false,
            0,
            NULL,
            0,
            NULL,
            1,
            barriers);

      src_width                                 = MAX(size.width  >> (i - 1), 1u);
      src_height                                = MAX(size.height >> (i - 1), 1u);
      target_width                              = MAX(size.width  >> i,       1u);
      target_height                             = MAX(size.height >> i,       1u);

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
         false,
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

static void vulkan_framebuffer_set_size(Framebuffer *_fb,
      DeferredDisposer &disposer, const Size2D &size, VkFormat format)
{
   _fb->size = size;
   if (format != VK_FORMAT_UNDEFINED)
	  _fb->format = format;

#ifdef VULKAN_DEBUG
   RARCH_LOG("[Vulkan filter chain]: Updating framebuffer size %ux%u (format: %u).\n",
         size.width, size.height, (unsigned)fb->format);
#endif

   {
      /* The current framebuffers, etc, might still be in use
       * so defer deletion.
       * We'll most likely be able to reuse the memory,
       * so don't free it here.
       *
       * Fake lambda init captures for C++11.
       */
      VkDevice d       = _fb->device;
      VkImage i        = _fb->image;
      VkImageView v    = _fb->view;
      VkImageView fbv  = _fb->fb_view;
      VkFramebuffer fb = _fb->framebuffer;
      disposer.calls.push_back(std::move([=]
      {
         if (fb != VK_NULL_HANDLE)
            vkDestroyFramebuffer(d, fb, nullptr);
         if (v != VK_NULL_HANDLE)
            vkDestroyImageView(d, v, nullptr);
         if (fbv != VK_NULL_HANDLE)
            vkDestroyImageView(d, fbv, nullptr);
         if (i != VK_NULL_HANDLE)
            vkDestroyImage(d, i, nullptr);
      }));
   }

   _fb->init(&disposer);
}

static Size2D vulkan_pass_get_output_size(Pass *pass,
      const Size2D &original,
      const Size2D &source)
{
   float width  = 0.0f;
   float height = 0.0f;
   switch (pass->pass_info.scale_type_x)
   {
      case GLSLANG_FILTER_CHAIN_SCALE_ORIGINAL:
         width = float(original.width) * pass->pass_info.scale_x;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_SOURCE:
         width = float(source.width) * pass->pass_info.scale_x;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT:
         width = (retroarch_get_rotation() % 2 
               ? pass->current_viewport.height 
               : pass->current_viewport.width)
               * pass->pass_info.scale_x;
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
         height = float(original.height) * pass->pass_info.scale_y;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_SOURCE:
         height = float(source.height) * pass->pass_info.scale_y;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT:
         height = (retroarch_get_rotation() % 2 
               ? pass->current_viewport.width 
               : pass->current_viewport.height) 
            * pass->pass_info.scale_y;
         break;

      case GLSLANG_FILTER_CHAIN_SCALE_ABSOLUTE:
         height = pass->pass_info.scale_y;
         break;

      default:
         break;
   }

   return { unsigned(roundf(width)), unsigned(roundf(height)) };
}

static void vulkan_pass_build_commands(
      Pass *pass,
      DeferredDisposer &disposer,
      VkCommandBuffer cmd,
      const Texture &original,
      const Texture &source,
      const VkViewport &vp,
      const float *mvp)
{
   unsigned i;
   Size2D size;
   uint8_t *u             = nullptr;

   pass->current_viewport = vp;
   size                   = vulkan_pass_get_output_size(pass,
         { original.texture.width, original.texture.height },
         { source.texture.width, source.texture.height });

   if (pass->framebuffer &&
         (size.width  != pass->framebuffer->size.width ||
          size.height != pass->framebuffer->size.height))
      vulkan_framebuffer_set_size(pass->framebuffer.get(), disposer, size, VK_FORMAT_UNDEFINED);

   pass->current_framebuffer_size = size;

   if (pass->reflection.ubo_stage_mask && pass->common->ubo_mapped)
      u =  pass->common->ubo_mapped 
         + pass->ubo_offset
         + pass->sync_index 
         * pass->common->ubo_sync_index_stride;

   VkDescriptorSet set = pass->sets[pass->sync_index];

   /* MVP */
   if (u && pass->reflection.semantics[SLANG_SEMANTIC_MVP].uniform)
   {
      size_t offset = pass->reflection.semantics[SLANG_SEMANTIC_MVP].ubo_offset;
      if (mvp)
         memcpy(u + offset, mvp, sizeof(float) * 16);
      else /* Build identity matrix */
      {
         float *data = reinterpret_cast<float *>(u + offset);
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
   }

   if (pass->reflection.semantics[SLANG_SEMANTIC_MVP].push_constant)
   {
      size_t offset = pass->reflection.semantics[SLANG_SEMANTIC_MVP].push_constant_offset;
      if (mvp)
         memcpy(pass->push.buffer.data() + (offset >> 2), mvp, sizeof(float) * 16);
      else /* Build identity matrix */
      {
         float *data = reinterpret_cast<float *>(pass->push.buffer.data() + (offset >>
                  2));
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
   }

   /* Output information */
   {
      auto &refl      = pass->reflection.semantics[SLANG_SEMANTIC_OUTPUT];
      unsigned width  = pass->current_framebuffer_size.width;
      unsigned height = pass->current_framebuffer_size.height;

      if (u && refl.uniform)
      {
         float *_data = reinterpret_cast<float *>(u + refl.ubo_offset);
         _data[0]     = (float)(width);
         _data[1]     = (float)(height);
         _data[2]     = 1.0f / (float)(width);
         _data[3]     = 1.0f / (float)(height);
      }
      if (refl.push_constant)
      {
         float *_data = reinterpret_cast<float *>
            (pass->push.buffer.data() + (refl.push_constant_offset >> 2));
         _data[0]     = (float)(width);
         _data[1]     = (float)(height);
         _data[2]     = 1.0f / (float)(width);
         _data[3]     = 1.0f / (float)(height);
      }
   }
   {
      auto &refl      = pass->reflection.semantics[
         SLANG_SEMANTIC_FINAL_VIEWPORT];
      unsigned width  = unsigned(pass->current_viewport.width);
      unsigned height = unsigned(pass->current_viewport.height);
      if (u && refl.uniform)
      {
         float *_data = reinterpret_cast<float *>(u + refl.ubo_offset);
         _data[0]     = (float)(width);
         _data[1]     = (float)(height);
         _data[2]     = 1.0f / (float)(width);
         _data[3]     = 1.0f / (float)(height);
      }
      if (refl.push_constant)
      {
         float *_data = reinterpret_cast<float *>
            (pass->push.buffer.data() + (refl.push_constant_offset >> 2));
         _data[0]     = (float)(width);
         _data[1]     = (float)(height);
         _data[2]     = 1.0f / (float)(width);
         _data[3]     = 1.0f / (float)(height);
      }
   }

   {
      uint32_t value = pass->frame_count_period 
         ? uint32_t(pass->frame_count % pass->frame_count_period) 
         : uint32_t(pass->frame_count);
      auto &refl     = pass->reflection.semantics[SLANG_SEMANTIC_FRAME_COUNT];
      if (u && refl.uniform)
         *reinterpret_cast<uint32_t*>(u + pass->reflection.semantics[SLANG_SEMANTIC_FRAME_COUNT].ubo_offset) = value;
      if (refl.push_constant)
         *reinterpret_cast<uint32_t*>(pass->push.buffer.data() + (refl.push_constant_offset >> 2)) = value;
   }
   {
      auto &refl = pass->reflection.semantics[SLANG_SEMANTIC_FRAME_DIRECTION];
      if (u && refl.uniform)
         *reinterpret_cast<int32_t*>(u + pass->reflection.semantics[SLANG_SEMANTIC_FRAME_DIRECTION].ubo_offset)
            = pass->frame_direction;
      if (refl.push_constant)
         *reinterpret_cast<int32_t*>(pass->push.buffer.data() +
               (refl.push_constant_offset >> 2)) = pass->frame_direction;
   }

   /* Standard inputs */
   vulkan_pass_build_semantic_texture(pass, set, u, SLANG_TEXTURE_SEMANTIC_ORIGINAL, original);
   vulkan_pass_build_semantic_texture(pass, set, u, SLANG_TEXTURE_SEMANTIC_SOURCE, source);

   /* ORIGINAL_HISTORY[0] is an alias of ORIGINAL. */
   vulkan_pass_build_semantic_texture_array(pass, set, u,
         SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, 0, original);

   /* Parameters. */
   for (i = 0; i < pass->filtered_parameters.size(); i++)
   {
      unsigned index = pass->filtered_parameters[i].semantic_index;
      float    value = pass->common->shader_preset->parameters[
         pass->filtered_parameters[i].index].current;
      auto &refl     = pass->reflection.semantic_float_parameters[index];
      /* We will have filtered out stale parameters. */
      if (u && refl.uniform)
         *reinterpret_cast<float*>(u + refl.ubo_offset) = value;
      if (refl.push_constant)
         *reinterpret_cast<float*>(pass->push.buffer.data() + (refl.push_constant_offset >> 2)) = value;
   }

   /* Previous inputs. */
   for (i = 0; i < pass->common->original_history.size(); i++)
      vulkan_pass_build_semantic_texture_array(pass, set, u,
            SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY, i + 1,
            pass->common->original_history[i]);

   /* Previous passes. */
   for (i = 0; i < pass->common->pass_outputs.size(); i++)
      vulkan_pass_build_semantic_texture_array(pass, set, u,
            SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, i,
            pass->common->pass_outputs[i]);

   /* Feedback FBOs. */
   for (i = 0; i < pass->common->fb_feedback.size(); i++)
      vulkan_pass_build_semantic_texture_array(pass, set, u,
            SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, i,
            pass->common->fb_feedback[i]);

   /* LUTs. */
   for (i = 0; i < pass->common->luts.size(); i++)
      vulkan_pass_build_semantic_texture_array(pass, set, u,
            SLANG_TEXTURE_SEMANTIC_USER, i,
            pass->common->luts[i]->texture);

   if (pass->reflection.ubo_stage_mask)
      vulkan_set_uniform_buffer(
            pass->device,
            pass->sets[pass->sync_index],
            pass->reflection.ubo_binding,
            pass->common->ubo->buffer,
            pass->ubo_offset + pass->sync_index * pass->common->ubo_sync_index_stride,
            pass->reflection.ubo_size);

   /* The final pass is always executed inside
    * another render pass since the frontend will
    * want to overlay various things on top for
    * the passes that end up on-screen. */
   if (!pass->final_pass)
   {
      VkRenderPassBeginInfo rp_info;

      /* Render. */
      VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,
            pass->framebuffer->image,
            1,
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
      rp_info.renderPass               = pass->framebuffer->render_pass;
      rp_info.framebuffer              = pass->framebuffer->framebuffer;
      rp_info.renderArea.offset.x      = 0;
      rp_info.renderArea.offset.y      = 0;
      rp_info.renderArea.extent.width  = pass->current_framebuffer_size.width;
      rp_info.renderArea.extent.height = pass->current_framebuffer_size.height;
      rp_info.clearValueCount          = 0;
      rp_info.pClearValues             = nullptr;

      vkCmdBeginRenderPass(cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);
   }

   vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pass->pipeline);
   vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
         pass->pipeline_layout,
         0, 1, &pass->sets[pass->sync_index], 0, nullptr);

   if (pass->push.stages != 0)
      vkCmdPushConstants(cmd, pass->pipeline_layout,
            pass->push.stages, 0,
            pass->reflection.push_constant_size,
            pass->push.buffer.data());

   {
      VkDeviceSize offset = pass->final_pass ? 16 * sizeof(float) : 0;
      vkCmdBindVertexBuffers(cmd, 0, 1,
            &pass->common->vbo->buffer,
            &offset);
   }

   if (pass->final_pass)
   {
      const VkRect2D sci = {
         {
            int32_t(pass->current_viewport.x),
            int32_t(pass->current_viewport.y)
         },
         {
            uint32_t(pass->current_viewport.width),
            uint32_t(pass->current_viewport.height)
         },
      };
      vkCmdSetViewport(cmd, 0, 1, &pass->current_viewport);
      vkCmdSetScissor(cmd, 0, 1, &sci);
      vkCmdDraw(cmd, 4, 1, 0, 0);
   }
   else
   {
      const VkViewport _vp = {
         0.0f, 0.0f,
         float(pass->current_framebuffer_size.width),
         float(pass->current_framebuffer_size.height),
         0.0f, 1.0f
      };
      const VkRect2D sci = {
         { 0, 0 },
         {
            pass->current_framebuffer_size.width,
            pass->current_framebuffer_size.height
         },
      };

      vkCmdSetViewport(cmd, 0, 1, &_vp);
      vkCmdSetScissor(cmd, 0, 1, &sci);
      vkCmdDraw(cmd, 4, 1, 0, 0);
      vkCmdEndRenderPass(cmd);

      if (pass->framebuffer->levels > 1)
         vulkan_framebuffer_generate_mips(
               pass->framebuffer->framebuffer,
               pass->framebuffer->image,
               pass->framebuffer->size,
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
               VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
               VK_QUEUE_FAMILY_IGNORED,
               VK_QUEUE_FAMILY_IGNORED);
      }
   }
}

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

static VkFormat glslang_format_to_vk(glslang_format fmt)
{
   switch (fmt)
   {
      case SLANG_FORMAT_R8_UNORM:
         return VK_FORMAT_R8_UNORM;
      case SLANG_FORMAT_R8_SINT:
         return VK_FORMAT_R8_SINT;
      case SLANG_FORMAT_R8_UINT:
         return VK_FORMAT_R8_UINT;
      case SLANG_FORMAT_R8G8_UNORM:
         return VK_FORMAT_R8G8_UNORM;
      case SLANG_FORMAT_R8G8_SINT:
         return VK_FORMAT_R8G8_SINT;
      case SLANG_FORMAT_R8G8_UINT:
         return VK_FORMAT_R8G8_UINT;
      case SLANG_FORMAT_R8G8B8A8_UNORM:
         return VK_FORMAT_R8G8B8A8_UNORM;
      case SLANG_FORMAT_R8G8B8A8_SINT:
         return VK_FORMAT_R8G8B8A8_SINT;
      case SLANG_FORMAT_R8G8B8A8_UINT:
         return VK_FORMAT_R8G8B8A8_UINT;
      case SLANG_FORMAT_R8G8B8A8_SRGB:
         return VK_FORMAT_R8G8B8A8_SRGB;
      case SLANG_FORMAT_A2B10G10R10_UNORM_PACK32:
         return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
      case SLANG_FORMAT_A2B10G10R10_UINT_PACK32:
         return VK_FORMAT_A2B10G10R10_UINT_PACK32;

      case SLANG_FORMAT_R16_UINT:
         return VK_FORMAT_R16_UINT;
      case SLANG_FORMAT_R16_SINT:
         return VK_FORMAT_R16_SINT;
      case SLANG_FORMAT_R16_SFLOAT:
         return VK_FORMAT_R16_SFLOAT;
      case SLANG_FORMAT_R16G16_UINT:
         return VK_FORMAT_R16G16_UINT;
      case SLANG_FORMAT_R16G16_SINT:
         return VK_FORMAT_R16G16_SINT;
      case SLANG_FORMAT_R16G16_SFLOAT:
         return VK_FORMAT_R16G16_SFLOAT;
      case SLANG_FORMAT_R16G16B16A16_UINT:
         return VK_FORMAT_R16G16B16A16_UINT;
      case SLANG_FORMAT_R16G16B16A16_SINT:
         return VK_FORMAT_R16G16B16A16_SINT;
      case SLANG_FORMAT_R16G16B16A16_SFLOAT:
         return VK_FORMAT_R16G16B16A16_SFLOAT;

      case SLANG_FORMAT_R32_UINT:
         return VK_FORMAT_R32_UINT;
      case SLANG_FORMAT_R32_SINT:
         return VK_FORMAT_R32_SINT;
      case SLANG_FORMAT_R32_SFLOAT:
         return VK_FORMAT_R32_SFLOAT;
      case SLANG_FORMAT_R32G32_UINT:
         return VK_FORMAT_R32G32_UINT;
      case SLANG_FORMAT_R32G32_SINT:
         return VK_FORMAT_R32G32_SINT;
      case SLANG_FORMAT_R32G32_SFLOAT:
         return VK_FORMAT_R32G32_SFLOAT;
      case SLANG_FORMAT_R32G32B32A32_UINT:
         return VK_FORMAT_R32G32B32A32_UINT;
      case SLANG_FORMAT_R32G32B32A32_SINT:
         return VK_FORMAT_R32G32B32A32_SINT;
      case SLANG_FORMAT_R32G32B32A32_SFLOAT:
         return VK_FORMAT_R32G32B32A32_SFLOAT;
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
   std::unique_ptr<Buffer> buffer;
   VkMemoryRequirements mem_reqs;
   VkImageCreateInfo image_info    = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
   VkImageViewCreateInfo view_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
   VkMemoryAllocateInfo alloc      = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
   VkImage tex                     = VK_NULL_HANDLE;
   VkDeviceMemory memory           = VK_NULL_HANDLE;
   VkImageView view                = VK_NULL_HANDLE;
   VkBufferImageCopy region        = {};
   void *ptr                       = nullptr;

   image.width                     = 0;
   image.height                    = 0;
   image.pixels                    = NULL;
   image.supports_rgba             = video_driver_supports_rgba();

   if (!image_texture_load(&image, shader->path))
      return {};

   image_info.imageType            = VK_IMAGE_TYPE_2D;
   image_info.format               = VK_FORMAT_B8G8R8A8_UNORM;
   image_info.extent.width         = image.width;
   image_info.extent.height        = image.height;
   image_info.extent.depth         = 1;
   image_info.mipLevels            = shader->mipmap 
      ? glslang_num_miplevels(image.width, image.height) : 1;
   image_info.arrayLayers          = 1;
   image_info.samples              = VK_SAMPLE_COUNT_1_BIT;
   image_info.tiling               = VK_IMAGE_TILING_OPTIMAL;
   image_info.usage                = VK_IMAGE_USAGE_SAMPLED_BIT |
                                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   image_info.initialLayout        = VK_IMAGE_LAYOUT_UNDEFINED;

   vkCreateImage(info->device, &image_info, nullptr, &tex);
   vkGetImageMemoryRequirements(info->device, tex, &mem_reqs);

   alloc.allocationSize            = mem_reqs.size;
   alloc.memoryTypeIndex           = vulkan_find_memory_type(
         &*info->memory_properties,
         mem_reqs.memoryTypeBits,
         VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

   if (vkAllocateMemory(info->device, &alloc, nullptr, &memory) != VK_SUCCESS)
      goto error;

   vkBindImageMemory(info->device, tex, memory, 0);

   view_info.image                       = tex;
   view_info.viewType                    = VK_IMAGE_VIEW_TYPE_2D;
   view_info.format                      = VK_FORMAT_B8G8R8A8_UNORM;
   view_info.components.r                = VK_COMPONENT_SWIZZLE_R;
   view_info.components.g                = VK_COMPONENT_SWIZZLE_G;
   view_info.components.b                = VK_COMPONENT_SWIZZLE_B;
   view_info.components.a                = VK_COMPONENT_SWIZZLE_A;
   view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   view_info.subresourceRange.levelCount = image_info.mipLevels;
   view_info.subresourceRange.layerCount = 1;
   vkCreateImageView(info->device, &view_info, nullptr, &view);

   buffer                                = 
      std::unique_ptr<Buffer>(new Buffer(info->device, *info->memory_properties,
               image.width * image.height * sizeof(uint32_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT));
   if (!buffer->mapped && vkMapMemory(buffer->device, buffer->memory, 0,
            buffer->size, 0, &buffer->mapped) == VK_SUCCESS)
      ptr = buffer->mapped;
   memcpy(ptr, image.pixels, image.width * image.height * sizeof(uint32_t));
   if (buffer->mapped)
      vkUnmapMemory(buffer->device, buffer->memory);
   buffer->mapped = nullptr;

   VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd, tex,
         VK_REMAINING_MIP_LEVELS,
         VK_IMAGE_LAYOUT_UNDEFINED,
         shader->mipmap ? VK_IMAGE_LAYOUT_GENERAL 
         : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         0,
         VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_QUEUE_FAMILY_IGNORED,
         VK_QUEUE_FAMILY_IGNORED
         );

   region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   region.imageSubresource.mipLevel       = 0;
   region.imageSubresource.baseArrayLayer = 0;
   region.imageSubresource.layerCount     = 1;
   region.imageExtent.width               = image.width;
   region.imageExtent.height              = image.height;
   region.imageExtent.depth               = 1;

   vkCmdCopyBufferToImage(cmd,
         buffer->buffer,
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
   unsigned i;
   VkCommandBufferBeginInfo begin_info           = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
   VkSubmitInfo submit_info                      = {
      VK_STRUCTURE_TYPE_SUBMIT_INFO };
   VkCommandBuffer cmd                           = VK_NULL_HANDLE;
   VkCommandBufferAllocateInfo cmd_info          = { 
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
   bool recording                                = false;

   cmd_info.commandPool                          = info->command_pool;
   cmd_info.level                                = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   cmd_info.commandBufferCount                   = 1;

   vkAllocateCommandBuffers(info->device, &cmd_info, &cmd);
   begin_info.flags                              = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   vkBeginCommandBuffer(cmd, &begin_info);
   recording                                     = true;

   for (i = 0; i < shader->luts; i++)
   {
      std::unique_ptr<StaticTexture> image = 
         vulkan_filter_chain_load_lut(cmd, info, chain, &shader->lut[i]);
      if (!image)
      {
         RARCH_ERR("[Vulkan]: Failed to load LUT \"%s\".\n", shader->lut[i].path);
         goto error;
      }

      chain->common.luts.push_back(std::move(std::move(image)));
   }

   vkEndCommandBuffer(cmd);
   submit_info.commandBufferCount = 1;
   submit_info.pCommandBuffers    = &cmd;
   vkQueueSubmit(info->queue, 1, &submit_info, VK_NULL_HANDLE);
   vkQueueWaitIdle(info->queue);
   vkFreeCommandBuffers(info->device, info->command_pool, 1, &cmd);
   for (i = 0; i < chain->common.luts.size(); i++)
      chain->common.luts[i]->buffer.reset();
   return true;

error:
   if (recording)
      vkEndCommandBuffer(cmd);
   if (cmd != VK_NULL_HANDLE)
      vkFreeCommandBuffers(info->device, info->command_pool, 1, &cmd);
   return false;
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
   unsigned i;
   unsigned num_indices, num_passes;
   max_input_size = { info.max_input_size.width, info.max_input_size.height };
   swapchain_info = info.swapchain;
   num_indices    = info.swapchain.num_indices;
   for (auto &calls : deferred_calls)
   {
      for (auto &call : calls)
         call();
      calls.clear();
   }
   deferred_calls.resize(num_indices);
   num_passes = info.num_passes;
   pass_info.resize(num_passes);
   passes.reserve(num_passes);

   for (i = 0; i < num_passes; i++)
   {
      passes.emplace_back(new Pass(device, memory_properties,
               cache, deferred_calls.size(), i + 1 == num_passes));
      passes.back()->common      = &common;
      passes.back()->pass_number = i;
   }
}

vulkan_filter_chain::~vulkan_filter_chain()
{
   vkDeviceWaitIdle(device);
   for (auto &calls : deferred_calls)
   {
      for (auto &call : calls)
         call();
      calls.clear();
   }
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
   VkMemoryRequirements mem_reqs;
   VkMemoryAllocateInfo alloc = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
   VkBufferCreateInfo info    = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };

   info.size                  = size;
   info.usage                 = usage;
   info.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
   vkCreateBuffer(device, &info, nullptr, &buffer);

   vkGetBufferMemoryRequirements(device, buffer, &mem_reqs);

   alloc.allocationSize       = mem_reqs.size;
   alloc.memoryTypeIndex      = vulkan_find_memory_type(
         &mem_props, mem_reqs.memoryTypeBits,
         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
         | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

   vkAllocateMemory(device, &alloc, NULL, &memory);
   vkBindBufferMemory(device, buffer, memory, 0);
}

Buffer::~Buffer()
{
   if (mapped)
   {
      vkUnmapMemory(device, memory);
      mapped = nullptr;
   }
   if (memory != VK_NULL_HANDLE)
      vkFreeMemory(device, memory, nullptr);
   if (buffer != VK_NULL_HANDLE)
      vkDestroyBuffer(device, buffer, nullptr);
}

Pass::~Pass()
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

CommonResources::CommonResources(VkDevice device,
      const VkPhysicalDeviceMemoryProperties &memory_properties)
   : device(device)
{
   unsigned i;
   void *ptr                    = NULL;
   VkSamplerCreateInfo info     = { 
      VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
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

   if (!vbo->mapped && vkMapMemory(vbo->device, vbo->memory, 0, vbo->size, 0,
            &vbo->mapped) == VK_SUCCESS)
      ptr = vbo->mapped;
   memcpy(ptr, vbo_data, sizeof(vbo_data));
   if (vbo->mapped)
      vkUnmapMemory(vbo->device, vbo->memory);
   vbo->mapped = nullptr;

   info.mipLodBias              = 0.0f;
   info.maxAnisotropy           = 1.0f;
   info.compareEnable           = false;
   info.minLod                  = 0.0f;
   info.maxLod                  = VK_LOD_CLAMP_NONE;
   info.unnormalizedCoordinates = false;
   info.borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

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

static void vulkan_initialize_render_pass(VkDevice device, VkFormat format,
      VkRenderPass *render_pass)
{
   VkAttachmentReference color_ref;
   VkRenderPassCreateInfo rp_info;
   VkAttachmentDescription attachment;
   VkSubpassDescription subpass = {0};

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

   subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpass.colorAttachmentCount = 1;
   subpass.pColorAttachments    = &color_ref;

   vkCreateRenderPass(device, &rp_info, NULL, render_pass);
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
#ifdef VULKAN_DEBUG
   RARCH_LOG("[Vulkan filter chain]: Creating framebuffer %ux%u (max %u level(s)).\n",
         max_size.width, max_size.height, max_levels);
#endif
   vulkan_initialize_render_pass(device, format, &render_pass);
   init(nullptr);
}

void Framebuffer::init(DeferredDisposer *disposer)
{
   VkMemoryRequirements mem_reqs;
   VkImageCreateInfo info                    = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
   VkMemoryAllocateInfo alloc                = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
   VkImageViewCreateInfo view_info           = {
      VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };

   info.imageType         = VK_IMAGE_TYPE_2D;
   info.format            = format;
   info.extent.width      = size.width;
   info.extent.height     = size.height;
   info.extent.depth      = 1;
   info.mipLevels         = std::min(max_levels,
         glslang_num_miplevels(size.width, size.height));
   info.arrayLayers       = 1;
   info.samples           = VK_SAMPLE_COUNT_1_BIT;
   info.tiling            = VK_IMAGE_TILING_OPTIMAL;
   info.usage             = VK_IMAGE_USAGE_SAMPLED_BIT |
                            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                            VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

   info.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
   info.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
   levels                 = info.mipLevels;

   vkCreateImage(device, &info, nullptr, &image);

   vkGetImageMemoryRequirements(device, image, &mem_reqs);

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
         disposer->calls.push_back(std::move([=] { vkFreeMemory(d, m, nullptr); }));
      }

      memory.type = alloc.memoryTypeIndex;
      memory.size = mem_reqs.size;

      vkAllocateMemory(device, &alloc, nullptr, &memory.memory);
   }

   vkBindImageMemory(device, image, memory.memory, 0);

   view_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
   view_info.format                          = format;
   view_info.image                           = image;
   view_info.subresourceRange.baseMipLevel   = 0;
   view_info.subresourceRange.baseArrayLayer = 0;
   view_info.subresourceRange.levelCount     = levels;
   view_info.subresourceRange.layerCount     = 1;
   view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   view_info.components.r                    = VK_COMPONENT_SWIZZLE_R;
   view_info.components.g                    = VK_COMPONENT_SWIZZLE_G;
   view_info.components.b                    = VK_COMPONENT_SWIZZLE_B;
   view_info.components.a                    = VK_COMPONENT_SWIZZLE_A;

   vkCreateImageView(device, &view_info, nullptr, &view);
   view_info.subresourceRange.levelCount = 1;
   vkCreateImageView(device, &view_info, nullptr, &fb_view);

   /* Initialize framebuffer */
   {
      VkFramebufferCreateInfo info = {
         VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
      info.renderPass      = render_pass;
      info.attachmentCount = 1;
      info.pAttachments    = &fb_view;
      info.width           = size.width;
      info.height          = size.height;
      info.layers          = 1;

      vkCreateFramebuffer(device, &info, nullptr, &framebuffer);
   }
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

static bool vulkan_filter_chain_initialize(vulkan_filter_chain_t *chain)
{
   unsigned i, j;
   VkPhysicalDeviceProperties props;
   size_t required_images = 0;
   bool use_feedbacks     = false;
   Size2D source          = chain->max_input_size;

   /* Initialize alias */
   chain->common.texture_semantic_map.clear();
   chain->common.texture_semantic_uniform_map.clear();

   for (i = 0; i < chain->passes.size(); i++)
   {
      const std::string name = chain->passes[i]->pass_name;
      if (name.empty())
         continue;

      j = &chain->passes[i] - chain->passes.data();

      if (!slang_set_unique_map(
               chain->common.texture_semantic_map, name,
               slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, j }))
         return false;

      if (!slang_set_unique_map(
               chain->common.texture_semantic_uniform_map, name + "Size",
               slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_PASS_OUTPUT, j }))
         return false;

      if (!slang_set_unique_map(
               chain->common.texture_semantic_map, name + "Feedback",
               slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, j }))
         return false;

      if (!slang_set_unique_map(
               chain->common.texture_semantic_uniform_map, name + "FeedbackSize",
               slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_PASS_FEEDBACK, j }))
         return false;
   }

   for (i = 0; i < chain->common.luts.size(); i++)
   {
      j = &chain->common.luts[i] - chain->common.luts.data();
      if (!slang_set_unique_map(
               chain->common.texture_semantic_map,
               chain->common.luts[i]->id,
               slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_USER, j }))
         return false;

      if (!slang_set_unique_map(
               chain->common.texture_semantic_uniform_map,
               chain->common.luts[i]->id + "Size",
               slang_texture_semantic_map{ SLANG_TEXTURE_SEMANTIC_USER, j }))
         return false;
   }

   for (i = 0; i < chain->passes.size(); i++)
   {
#ifdef VULKAN_DEBUG
      const char *name = chain->passes[i]->pass_name.c_str();
      RARCH_LOG("[slang]: Building pass #%u (%s)\n", i,
            string_is_empty(name) ?
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE) :
            name);
#endif
      if (chain->passes[i]->pool != VK_NULL_HANDLE)
         vkDestroyDescriptorPool(chain->device, chain->passes[i]->pool, nullptr);
      if (chain->passes[i]->pipeline != VK_NULL_HANDLE)
         vkDestroyPipeline(chain->device, chain->passes[i]->pipeline, nullptr);
      if (chain->passes[i]->set_layout != VK_NULL_HANDLE)
         vkDestroyDescriptorSetLayout(chain->device, chain->passes[i]->set_layout, nullptr);
      if (chain->passes[i]->pipeline_layout != VK_NULL_HANDLE)
         vkDestroyPipelineLayout(chain->device, chain->passes[i]->pipeline_layout, nullptr);

      chain->passes[i]->pool                     = VK_NULL_HANDLE;
      chain->passes[i]->pipeline                 = VK_NULL_HANDLE;
      chain->passes[i]->set_layout               = VK_NULL_HANDLE;

      chain->passes[i]->current_viewport         = chain->swapchain_info.viewport;
      chain->passes[i]->pass_info                = chain->pass_info[i];

      chain->passes[i]->num_sync_indices         = chain->swapchain_info.num_indices;
      chain->passes[i]->sync_index               = 0;

      chain->passes[i]->current_framebuffer_size =
         vulkan_pass_get_output_size(chain->passes[i].get(),
               chain->max_input_size, source);
      chain->passes[i]->swapchain_render_pass    = chain->swapchain_info.render_pass;

      source                                     = chain->passes[i]->current_framebuffer_size;
      if (!vulkan_pass_build(chain->passes[i].get()))
         return false;
   }

   chain->require_clear         = false;

   /* Initialize UBO (Uniform Buffer Object) */
   chain->common.ubo.reset();
   chain->common.ubo_offset     = 0;

   vkGetPhysicalDeviceProperties(chain->gpu, &props);
   chain->common.ubo_alignment  = props.limits.minUniformBufferOffsetAlignment;

   /* Who knows. :) */
   if (chain->common.ubo_alignment == 0)
      chain->common.ubo_alignment = 1;

   /* Allocate pass buffers */
   for (i = 0; i < chain->passes.size(); i++)
   {
      if (chain->passes[i]->reflection.ubo_stage_mask)
      {
         /* Align */
         chain->passes[i]->common->ubo_offset = (chain->passes[i]->common->ubo_offset +
               chain->passes[i]->common->ubo_alignment - 1) &
            ~(chain->passes[i]->common->ubo_alignment - 1);
         chain->passes[i]->ubo_offset = chain->passes[i]->common->ubo_offset;

         /* Allocate */
         chain->passes[i]->common->ubo_offset += chain->passes[i]->reflection.ubo_size;
      }
   }

   chain->common.ubo_offset            = 
      (chain->common.ubo_offset + chain->common.ubo_alignment - 1) &
      ~(chain->common.ubo_alignment - 1);
   chain->common.ubo_sync_index_stride = chain->common.ubo_offset;

   if (chain->common.ubo_offset != 0)
      chain->common.ubo                = std::unique_ptr<Buffer>(new Buffer(chain->device,
               chain->memory_properties,
	       chain->common.ubo_offset * chain->deferred_calls.size(),
               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT));

   if (!chain->common.ubo->mapped && vkMapMemory(chain->common.ubo->device,
            chain->common.ubo->memory, 0, chain->common.ubo->size, 0, &chain->common.ubo->mapped) == VK_SUCCESS)
      chain->common.ubo_mapped = static_cast<uint8_t*>(chain->common.ubo->mapped);

   /* Initialize history */
   chain->original_history.clear();
   chain->common.original_history.clear();

   for (i = 0; i < chain->passes.size(); i++)
      required_images =
         std::max(required_images,
               chain->passes[i]->reflection.semantic_textures[
               SLANG_TEXTURE_SEMANTIC_ORIGINAL_HISTORY].size());

   /* If required images are less than 2, not using frame history */
   if (required_images >= 2)
   {
      /* We don't need to store array element #0,
       * since it's aliased with the actual original. */
      required_images--;
      chain->original_history.reserve(required_images);
      chain->common.original_history.resize(required_images);

      for (i = 0; i < required_images; i++)
         chain->original_history.emplace_back(
               new Framebuffer(chain->device,
		       chain->memory_properties,
		       chain->max_input_size,
		       chain->original_format, 1));

#ifdef VULKAN_DEBUG
      RARCH_LOG("[Vulkan filter chain]: Using history of %u frames.\n", unsigned(required_images));
#endif

      /* On first frame, we need to clear the textures to
       * a known state, but we need
       * a command buffer for that, so just defer to first frame.
       */
      chain->require_clear = true;
   }

   /* Initialize feedback */
   chain->common.fb_feedback.clear();
   /* Final pass cannot have feedback. */
   for (i = 0; i < chain->passes.size() - 1; i++)
   {
      bool use_feedback = false;
      for (auto &pass : chain->passes)
      {
         const slang_reflection &r = pass->reflection;
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
         if (!chain->passes[i]->final_pass)
            return false;
         chain->passes[i]->fb_feedback = std::unique_ptr<Framebuffer>(
               new Framebuffer(
		       chain->device,
		       chain->memory_properties,
		       chain->passes[i]->current_framebuffer_size,
		       chain->passes[i]->pass_info.rt_format,
		       chain->passes[i]->pass_info.max_levels));
#ifdef VULKAN_DEBUG
         RARCH_LOG("[Vulkan filter chain]: Using framebuffer feedback for pass #%u.\n", i);
#endif
      }
   }

   if (use_feedbacks)
   {
      chain->common.fb_feedback.resize(chain->passes.size() - 1);
      chain->require_clear = true;
   }

   chain->common.pass_outputs.resize(chain->passes.size());
   return true;
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

   chain->pass_info[0]     = pass_info;

   vulkan_filter_chain_set_shader(chain.get(), 0,
         VK_SHADER_STAGE_VERTEX_BIT,
         opaque_vert,
         sizeof(opaque_vert) / sizeof(uint32_t));
   vulkan_filter_chain_set_shader(chain.get(), 0,
         VK_SHADER_STAGE_FRAGMENT_BIT,
         opaque_frag,
         sizeof(opaque_frag) / sizeof(uint32_t));

   if (!vulkan_filter_chain_initialize(chain.get()))
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

   bool last_pass_is_fbo = shader->pass[shader->passes - 1].fbo.valid;
   auto tmpinfo          = *info;
   tmpinfo.num_passes    = shader->passes + (last_pass_is_fbo ? 1 : 0);

   std::unique_ptr<vulkan_filter_chain> chain{ new vulkan_filter_chain(tmpinfo) };
   if (!chain)
      return nullptr;

   if (shader->luts && !vulkan_filter_chain_load_luts(info, chain.get(), shader.get()))
      return nullptr;

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
         return nullptr;
      }

      for (auto &meta_param : output.meta.parameters)
      {
         if (shader->num_parameters >= GFX_MAX_PARAMETERS)
         {
            RARCH_ERR("[Vulkan]: Exceeded maximum number of parameters (%u).\n", GFX_MAX_PARAMETERS);
            return nullptr;
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
               return nullptr;
            }
            chain->passes[i]->parameters.push_back({ meta_param.id, unsigned(itr - shader->parameters), unsigned(chain->passes[i]->parameters.size()) });
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
            chain->passes[i]->parameters.push_back({ meta_param.id, shader->num_parameters, unsigned(chain->passes[i]->parameters.size()) });
            shader->num_parameters++;
         }
      }

      vulkan_filter_chain_set_shader(chain.get(), i,
            VK_SHADER_STAGE_VERTEX_BIT,
            output.vertex.data(),
            output.vertex.size());
      vulkan_filter_chain_set_shader(chain.get(), i,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            output.fragment.data(),
            output.fragment.size());
      chain->passes[i]->frame_count_period = pass->frame_count_mod;

      if (!output.meta.name.empty())
         chain->passes[i]->pass_name = output.meta.name.c_str();

      /* Preset overrides. */
      if (*pass->alias)
         chain->passes[i]->pass_name = pass->alias;

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

      if (!pass->fbo.valid)
      {
         pass_info.scale_type_x    = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
         pass_info.scale_type_y    = GLSLANG_FILTER_CHAIN_SCALE_SOURCE;
         pass_info.scale_x         = 1.0f;
         pass_info.scale_y         = 1.0f;

         if (i + 1 == shader->passes)
         {
            pass_info.scale_type_x = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
            pass_info.scale_type_y = GLSLANG_FILTER_CHAIN_SCALE_VIEWPORT;
#ifdef VULKAN_HDR_SWAPCHAIN
            if (tmpinfo.swapchain.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32)
            {
               pass_info.rt_format    = glslang_format_to_vk( output.meta.rt_format);

#ifdef VULKAN_DEBUG
               RARCH_LOG("[slang]: Using render target format %s for pass output #%u.\n",
                     glslang_format_to_string(output.meta.rt_format), i);
#endif
            }
            else
#endif /* VULKAN_HDR_SWAPCHAIN */
            {
               pass_info.rt_format    = tmpinfo.swapchain.format;

               if (explicit_format)
                  RARCH_WARN("[slang]: Using explicit format for last pass in chain,"
                        " but it is not rendered to framebuffer, using swapchain format instead.\n");
            }
         }
         else
         {
            pass_info.rt_format    = glslang_format_to_vk(
                  output.meta.rt_format);
#ifdef VULKAN_DEBUG
            RARCH_LOG("[slang]: Using render target format %s for pass output #%u.\n",
                  glslang_format_to_string(output.meta.rt_format), i);
#endif
         }
      }
      else
      {
         /* Preset overrides shader.
          * Kinda ugly ... */
         if (pass->fbo.srgb_fbo)
            output.meta.rt_format = SLANG_FORMAT_R8G8B8A8_SRGB;
         else if (pass->fbo.fp_fbo)
            output.meta.rt_format = SLANG_FORMAT_R16G16B16A16_SFLOAT;

         pass_info.rt_format      = glslang_format_to_vk(output.meta.rt_format);

#ifdef VULKAN_DEBUG
         RARCH_LOG("[slang]: Using render target format %s for pass output #%u.\n",
               glslang_format_to_string(output.meta.rt_format), i);
#endif

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

      chain->pass_info[i]     = pass_info;
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

      chain->pass_info[shader->passes] = pass_info;

      vulkan_filter_chain_set_shader(chain.get(),
            shader->passes,
            VK_SHADER_STAGE_VERTEX_BIT,
            opaque_vert,
            sizeof(opaque_vert) / sizeof(uint32_t));
      vulkan_filter_chain_set_shader(chain.get(),
            shader->passes,
            VK_SHADER_STAGE_FRAGMENT_BIT,
            opaque_frag,
            sizeof(opaque_frag) / sizeof(uint32_t));
   }

   chain->common.shader_preset = std::move(std::move(shader));

   if (!vulkan_filter_chain_initialize(chain.get()))
      return nullptr;

   return chain.release();
}

struct video_shader *vulkan_filter_chain_get_preset(
      vulkan_filter_chain_t *chain)
{
   return chain->common.shader_preset.get();
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
   switch (stage)
   {
      case VK_SHADER_STAGE_VERTEX_BIT:
         chain->passes[pass]->vertex_shader.clear();
         chain->passes[pass]->vertex_shader.insert(
               end(chain->passes[pass]->vertex_shader),
               spirv, spirv + spirv_words);
         break;
      case VK_SHADER_STAGE_FRAGMENT_BIT:
         chain->passes[pass]->fragment_shader.clear();
         chain->passes[pass]->fragment_shader.insert(
               end(chain->passes[pass]->fragment_shader),
               spirv, spirv + spirv_words);
         break;
      default:
         break;
   }
}

VkFormat vulkan_filter_chain_get_pass_rt_format(
      vulkan_filter_chain_t *chain,
      unsigned pass)
{
   return chain->pass_info[pass].rt_format;
}

bool vulkan_filter_chain_update_swapchain_info(
      vulkan_filter_chain_t *chain,
      const vulkan_filter_chain_swapchain_info *info)
{
   unsigned num_indices;
   vkDeviceWaitIdle(chain->device);
   for (auto &calls : chain->deferred_calls)
   {
      for (auto &call : calls)
         call();
      calls.clear();
   }
   chain->swapchain_info = *info;
   num_indices           = info->num_indices;
   for (auto &calls : chain->deferred_calls)
   {
      for (auto &call : calls)
         call();
      calls.clear();
   }
   chain->deferred_calls.resize(num_indices);
   return vulkan_filter_chain_initialize(chain);
}

void vulkan_filter_chain_notify_sync_index(
      vulkan_filter_chain_t *chain,
      unsigned index)
{
   unsigned i;
   auto &calls = chain->deferred_calls[index];
   for (auto &call : calls)
      call();
   calls.clear();

   chain->current_sync_index = index;

   for (i = 0; i < chain->passes.size(); i++)
      chain->passes[i]->sync_index = index;
}

bool vulkan_filter_chain_init(vulkan_filter_chain_t *chain)
{
   return vulkan_filter_chain_initialize(chain);
}

void vulkan_filter_chain_set_input_texture(
      vulkan_filter_chain_t *chain,
      const struct vulkan_filter_chain_texture *texture)
{
   chain->input_texture = *texture;
}

void vulkan_filter_chain_set_frame_count(
      vulkan_filter_chain_t *chain,
      uint64_t count)
{
   unsigned i;
   for (i = 0; i < chain->passes.size(); i++)
      chain->passes[i]->frame_count = count;
}

void vulkan_filter_chain_set_frame_count_period(
      vulkan_filter_chain_t *chain,
      unsigned pass,
      unsigned period)
{
   chain->passes[pass]->frame_count_period = period;
}

void vulkan_filter_chain_set_frame_direction(
      vulkan_filter_chain_t *chain,
      int32_t direction)
{
   unsigned i;
   for (i = 0; i < chain->passes.size(); i++)
      chain->passes[i]->frame_direction = direction;
}

void vulkan_filter_chain_build_offscreen_passes(
      vulkan_filter_chain_t *chain,
      VkCommandBuffer cmd, const VkViewport *vp)
{
   unsigned i;
   Texture source;

   /* First frame, make sure our history and feedback textures 
    * are in a clean state.
    * Clear framebuffers.
    */
   if (chain->require_clear)
   {
      unsigned i;
      for (i = 0; i < chain->original_history.size(); i++)
      {
         VkClearColorValue color;
         VkImageSubresourceRange range;
         VkImage image = chain->original_history[i]->image;

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
      for (i = 0; i < chain->passes.size(); i++)
      {
         Framebuffer *fb = chain->passes[i]->fb_feedback.get();
         if (fb)
         {
            VkClearColorValue color;
            VkImageSubresourceRange range;
            VkImage image = fb->image;

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
      }
      chain->require_clear = false;
   }

   /* Update history info */
   for (i = 0; i < chain->original_history.size(); i++)
   {
      Texture *source          = (Texture*)&chain->common.original_history[i];

      if (!source)
         continue;

      source->texture.layout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

      source->texture.view     = chain->original_history[i]->view;
      source->texture.image    = chain->original_history[i]->image;
      source->texture.width    = chain->original_history[i]->size.width;
      source->texture.height   = chain->original_history[i]->size.height;
      source->filter           = chain->passes.front()->pass_info.source_filter;
      source->mip_filter       = chain->passes.front()->pass_info.mip_filter;
      source->address          = chain->passes.front()->pass_info.address;
   }

   /* Update feedback info? */
   if (!chain->common.fb_feedback.empty())
   {
      for (i = 0; i < chain->passes.size() - 1; i++)
      {
         Framebuffer *fb         = chain->passes[i]->fb_feedback.get();
         if (!fb)
            continue;

         Texture *source         = &chain->common.fb_feedback[i];

         if (!source)
            continue;

         source->texture.image   = fb->image;
         source->texture.view    = fb->view;
         source->texture.layout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
         source->texture.width   = fb->size.width;
         source->texture.height  = fb->size.height;
         source->filter          = chain->passes[i]->pass_info.source_filter;
         source->mip_filter      = chain->passes[i]->pass_info.mip_filter;
         source->address         = chain->passes[i]->pass_info.address;
      }
   }

   DeferredDisposer disposer(chain->deferred_calls[chain->current_sync_index]);
   const Texture original = {
      chain->input_texture,
      chain->passes.front()->pass_info.source_filter,
      chain->passes.front()->pass_info.mip_filter,
      chain->passes.front()->pass_info.address,
   };

   source = original;

   for (i = 0; i < chain->passes.size() - 1; i++)
   {
      vulkan_pass_build_commands(chain->passes[i].get(), 
            disposer, cmd,
            original, source, *vp, nullptr);

      const Framebuffer &fb   = *chain->passes[i]->framebuffer;

      source.texture.view     = fb.view;
      source.texture.layout   = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      source.texture.width    = fb.size.width;
      source.texture.height   = fb.size.height;
      source.filter           = chain->passes[i + 1]->pass_info.source_filter;
      source.mip_filter       = chain->passes[i + 1]->pass_info.mip_filter;
      source.address          = chain->passes[i + 1]->pass_info.address;

      chain->common.pass_outputs[i]  = source;
   }
}

void vulkan_filter_chain_build_viewport_pass(
      vulkan_filter_chain_t *chain,
      VkCommandBuffer cmd, const VkViewport *vp, const float *mvp)
{
   unsigned i;
   Texture source;

   /* First frame, make sure our history and 
    * feedback textures are in a clean state.
    * Clear framebuffers.
    */
   if (chain->require_clear)
   {
      unsigned i;
      for (i = 0; i < chain->original_history.size(); i++)
      {
         VkClearColorValue color;
         VkImageSubresourceRange range;
         VkImage image = chain->original_history[i]->image;

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
      for (i = 0; i < chain->passes.size(); i++)
      {
         Framebuffer *fb = chain->passes[i]->fb_feedback.get();
         if (fb)
         {
            VkClearColorValue color;
            VkImageSubresourceRange range;
            VkImage image = fb->image;

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
      }
      chain->require_clear = false;
   }

   DeferredDisposer disposer(chain->deferred_calls[chain->current_sync_index]);
   const Texture original = {
      chain->input_texture,
      chain->passes.front()->pass_info.source_filter,
      chain->passes.front()->pass_info.mip_filter,
      chain->passes.front()->pass_info.address,
   };

   if (chain->passes.size() == 1)
   {
      source = {
         chain->input_texture,
         chain->passes.back()->pass_info.source_filter,
         chain->passes.back()->pass_info.mip_filter,
         chain->passes.back()->pass_info.address,
      };
   }
   else
   {
      const Framebuffer &fb  = *chain->passes[chain->passes.size() - 2]->framebuffer;
      source.texture.view    = fb.view;
      source.texture.layout  = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
      source.texture.width   = fb.size.width;
      source.texture.height  = fb.size.height;
      source.filter          = chain->passes.back()->pass_info.source_filter;
      source.mip_filter      = chain->passes.back()->pass_info.mip_filter;
      source.address         = chain->passes.back()->pass_info.address;
   }

   vulkan_pass_build_commands(chain->passes.back().get(), 
         disposer, cmd,
         original, source, *vp, mvp);

   /* For feedback FBOs, swap current and previous. */
   for (i = 0; i < chain->passes.size(); i++)
   {
      if (chain->passes[i]->fb_feedback)
         swap(chain->passes[i]->framebuffer, chain->passes[i]->fb_feedback);
   }
}

void vulkan_filter_chain_end_frame(
      vulkan_filter_chain_t *chain,
      VkCommandBuffer cmd)
{
   /* If we need to keep old frames, copy it after fragment is complete.
    * TODO: We can improve pipelining by figuring out which
    * pass is the last that reads from
    * the history and dispatch the copy earlier. */
   if (!chain->original_history.empty())
   {
      DeferredDisposer disposer(chain->deferred_calls[chain->current_sync_index]);
      std::unique_ptr<Framebuffer> tmp;
      VkImageLayout src_layout = chain->input_texture.layout;

      /* Transition input texture to something appropriate. */
      if (chain->input_texture.layout != VK_IMAGE_LAYOUT_GENERAL)
      {
         VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,
               chain->input_texture.image,
               VK_REMAINING_MIP_LEVELS,
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

      std::unique_ptr<Framebuffer> &back = chain->original_history.back();
      swap(back, tmp);

      if   (chain->input_texture.width      != tmp->size.width  ||
            chain->input_texture.height     != tmp->size.height ||
            (chain->input_texture.format    != VK_FORMAT_UNDEFINED 
             && chain->input_texture.format != tmp->format))
         vulkan_framebuffer_set_size(
			 tmp.get(), disposer,
			 { chain->input_texture.width,
			 chain->input_texture.height },
			 chain->input_texture.format);

      /* Copy framebuffer */
      {
         VkImageCopy region;
         VkImage image                        = tmp->image;
         VkImage src_image                    = chain->input_texture.image;
         struct Size2D size                   = tmp->size;

         VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd, image,VK_REMAINING_MIP_LEVELS,
               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
               0, VK_ACCESS_TRANSFER_WRITE_BIT,
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

      /* Transition input texture back. */
      if (chain->input_texture.layout != VK_IMAGE_LAYOUT_GENERAL)
      {
         VULKAN_IMAGE_LAYOUT_TRANSITION_LEVELS(cmd,
               chain->input_texture.image,
               VK_REMAINING_MIP_LEVELS,
               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
               chain->input_texture.layout,
               0,
               VK_ACCESS_SHADER_READ_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
               VK_QUEUE_FAMILY_IGNORED,
               VK_QUEUE_FAMILY_IGNORED);
      }

      /* Should ring buffer, but we don't have *that* many passes. */
      move_backward(begin(chain->original_history), end(chain->original_history)
            - 1, end(chain->original_history));
      swap(chain->original_history.front(), tmp);
   }
}
