/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Hans-Kristian Arntzen
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

#include <stdint.h>
#include <math.h>
#include <string.h>

#include <compat/strl.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <formats/image.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include <retro_math.h>
#include <string/stdstring.h>
#include <libretro.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "../font_driver.h"

#include "../common/vulkan_common.h"

#include "../../configuration.h"
#ifdef HAVE_REWIND
#include "../../state_manager.h"
#endif

#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../record/record_driver.h"

#include "../video_coord_array.h"

static struct vk_descriptor_manager vulkan_create_descriptor_manager(
      VkDevice device,
      const VkDescriptorPoolSize *sizes,
      unsigned num_sizes,
      VkDescriptorSetLayout set_layout)
{
   int i;
   struct vk_descriptor_manager manager;

   manager.current    = NULL;
   manager.count      = 0;

   for (i = 0; i < VULKAN_MAX_DESCRIPTOR_POOL_SIZES; i++)
   {
      manager.sizes[i].type            = VK_DESCRIPTOR_TYPE_SAMPLER;
      manager.sizes[i].descriptorCount = 0;
   }
   memcpy(manager.sizes, sizes, num_sizes * sizeof(*sizes));
   manager.set_layout = set_layout;
   manager.num_sizes  = num_sizes;

   manager.head       = vulkan_alloc_descriptor_pool(device, &manager);
   return manager;
}

static void vulkan_destroy_descriptor_manager(
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

static struct vk_buffer_chain vulkan_buffer_chain_init(
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



static void vulkan_set_viewport(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate);
static bool vulkan_is_mapped_swapchain_texture_ptr(const vk_t* vk,
      const void* ptr);

#ifdef HAVE_OVERLAY
static void vulkan_overlay_free(vk_t *vk);
static void vulkan_render_overlay(vk_t *vk, unsigned width, unsigned height);
#endif
static void vulkan_viewport_info(void *data, struct video_viewport *vp);

static const gfx_ctx_driver_t *vulkan_get_context(vk_t *vk, settings_t *settings)
{
   void                 *ctx_data  = NULL;
   unsigned major                  = 1;
   unsigned minor                  = 0;
   enum gfx_ctx_api api            = GFX_CTX_VULKAN_API;
   const gfx_ctx_driver_t *gfx_ctx = video_context_driver_init_first(
         vk, settings->arrays.video_context_driver,
         api, major, minor, false, &ctx_data);

   if (ctx_data)
      vk->ctx_data = ctx_data;

   return gfx_ctx;
}

static void vulkan_init_render_pass(
      vk_t *vk)
{
   VkRenderPassCreateInfo rp_info;
   VkAttachmentReference color_ref;
   VkAttachmentDescription attachment;
   VkSubpassDescription subpass;

   attachment.flags             = 0;
   /* Backbuffer format. */
   attachment.format            = vk->context->swapchain_format;
   /* Not multisampled. */
   attachment.samples           = VK_SAMPLE_COUNT_1_BIT;
   /* When starting the frame, we want tiles to be cleared. */
   attachment.loadOp            = VK_ATTACHMENT_LOAD_OP_CLEAR;
   /* When end the frame, we want tiles to be written out. */
   attachment.storeOp           = VK_ATTACHMENT_STORE_OP_STORE;
   /* Don't care about stencil since we're not using it. */
   attachment.stencilLoadOp     = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   attachment.stencilStoreOp    = VK_ATTACHMENT_STORE_OP_DONT_CARE;

   /* The image layout will be attachment_optimal
    * when we're executing the renderpass. */
   attachment.initialLayout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
   attachment.finalLayout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

   /* Color attachment reference */
   color_ref.attachment         = 0;
   color_ref.layout             = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

   /* We have one subpass.
    * This subpass has 1 color attachment. */
   subpass.flags                    = 0;
   subpass.pipelineBindPoint        = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subpass.inputAttachmentCount     = 0;
   subpass.pInputAttachments        = NULL;
   subpass.colorAttachmentCount     = 1;
   subpass.pColorAttachments        = &color_ref;
   subpass.pResolveAttachments      = NULL;
   subpass.pDepthStencilAttachment  = NULL;
   subpass.preserveAttachmentCount  = 0;
   subpass.pPreserveAttachments     = NULL;

   /* Finally, create the renderpass. */
   rp_info.sType                = 
      VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
   rp_info.pNext                = NULL;
   rp_info.flags                = 0;
   rp_info.attachmentCount      = 1;
   rp_info.pAttachments         = &attachment;
   rp_info.subpassCount         = 1;
   rp_info.pSubpasses           = &subpass;
   rp_info.dependencyCount      = 0;
   rp_info.pDependencies        = NULL;

   vkCreateRenderPass(vk->context->device,
         &rp_info, NULL, &vk->render_pass);
}

static void vulkan_init_framebuffers(
      vk_t *vk)
{
   int i;

   for (i = 0; i < (int) vk->num_swapchain_images; i++)
   {
      VkImageViewCreateInfo view =
      { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
      VkFramebufferCreateInfo info =
      { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };

      vk->backbuffers[i].image = vk->context->swapchain_images[i];

      if (vk->context->swapchain_images[i] == VK_NULL_HANDLE)
      {
         vk->backbuffers[i].view        = VK_NULL_HANDLE;
         vk->backbuffers[i].framebuffer = VK_NULL_HANDLE;
         continue;
      }

      /* Create an image view which we can render into. */
      view.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
      view.format                          = vk->context->swapchain_format;
      view.image                           = vk->backbuffers[i].image;
      view.subresourceRange.baseMipLevel   = 0;
      view.subresourceRange.baseArrayLayer = 0;
      view.subresourceRange.levelCount     = 1;
      view.subresourceRange.layerCount     = 1;
      view.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
      view.components.r                    = VK_COMPONENT_SWIZZLE_R;
      view.components.g                    = VK_COMPONENT_SWIZZLE_G;
      view.components.b                    = VK_COMPONENT_SWIZZLE_B;
      view.components.a                    = VK_COMPONENT_SWIZZLE_A;

      vkCreateImageView(vk->context->device,
            &view, NULL, &vk->backbuffers[i].view);

      /* Create the framebuffer */
      info.renderPass      = vk->render_pass;
      info.attachmentCount = 1;
      info.pAttachments    = &vk->backbuffers[i].view;
      info.width           = vk->context->swapchain_width;
      info.height          = vk->context->swapchain_height;
      info.layers          = 1;

      vkCreateFramebuffer(vk->context->device,
            &info, NULL, &vk->backbuffers[i].framebuffer);
   }
}

static void vulkan_init_pipeline_layout(
      vk_t *vk)
{
   VkPipelineLayoutCreateInfo layout_info;
   VkDescriptorSetLayoutCreateInfo set_layout_info;
   VkDescriptorSetLayoutBinding bindings[3];

   bindings[0].binding            = 0;
   bindings[0].descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   bindings[0].descriptorCount    = 1;
   bindings[0].stageFlags         = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
   bindings[0].pImmutableSamplers = NULL;

   bindings[1].binding            = 1;
   bindings[1].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   bindings[1].descriptorCount    = 1;
   bindings[1].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
   bindings[1].pImmutableSamplers = NULL;
   
   bindings[2].binding            = 2;
   bindings[2].descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   bindings[2].descriptorCount    = 1;
   bindings[2].stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
   bindings[2].pImmutableSamplers = NULL;

   set_layout_info.sType          = 
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   set_layout_info.pNext          = NULL;
   set_layout_info.flags          = 0;
   set_layout_info.bindingCount   = 3;
   set_layout_info.pBindings      = bindings;

   vkCreateDescriptorSetLayout(vk->context->device,
         &set_layout_info, NULL, &vk->pipelines.set_layout);

   layout_info.sType                  = 
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   layout_info.pNext                  = NULL;
   layout_info.flags                  = 0;
   layout_info.setLayoutCount         = 1;
   layout_info.pSetLayouts            = &vk->pipelines.set_layout;
   layout_info.pushConstantRangeCount = 0;
   layout_info.pPushConstantRanges    = NULL;

   vkCreatePipelineLayout(vk->context->device,
         &layout_info, NULL, &vk->pipelines.layout);
}

static void vulkan_init_pipelines(vk_t *vk)
{
#ifdef VULKAN_HDR_SWAPCHAIN
   static const uint32_t hdr_frag[] =
#include "vulkan_shaders/hdr.frag.inc"
      ;
#endif /* VULKAN_HDR_SWAPCHAIN */

   static const uint32_t alpha_blend_vert[] =
#include "vulkan_shaders/alpha_blend.vert.inc"
      ;

   static const uint32_t alpha_blend_frag[] =
#include "vulkan_shaders/alpha_blend.frag.inc"
      ;

   static const uint32_t font_frag[] =
#include "vulkan_shaders/font.frag.inc"
      ;

   static const uint32_t pipeline_ribbon_vert[] =
#include "vulkan_shaders/pipeline_ribbon.vert.inc"
      ;

   static const uint32_t pipeline_ribbon_frag[] =
#include "vulkan_shaders/pipeline_ribbon.frag.inc"
      ;

   static const uint32_t pipeline_ribbon_simple_vert[] =
#include "vulkan_shaders/pipeline_ribbon_simple.vert.inc"
      ;

   static const uint32_t pipeline_ribbon_simple_frag[] =
#include "vulkan_shaders/pipeline_ribbon_simple.frag.inc"
      ;

   static const uint32_t pipeline_snow_simple_frag[] =
#include "vulkan_shaders/pipeline_snow_simple.frag.inc"
      ;

   static const uint32_t pipeline_snow_frag[] =
#include "vulkan_shaders/pipeline_snow.frag.inc"
      ;

   static const uint32_t pipeline_bokeh_frag[] =
#include "vulkan_shaders/pipeline_bokeh.frag.inc"
      ;

   int i;
   VkPipelineInputAssemblyStateCreateInfo input_assembly = {
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
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

   VkPipelineShaderStageCreateInfo shader_stages[2]      = {
      { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
      { VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO },
   };

   VkGraphicsPipelineCreateInfo pipe                     = {
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
   VkShaderModuleCreateInfo module_info                  = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
   VkVertexInputAttributeDescription attributes[3]       = {{0}};
   VkVertexInputBindingDescription binding               = {0};

   static const VkDynamicState dynamics[] = {
      VK_DYNAMIC_STATE_VIEWPORT,
      VK_DYNAMIC_STATE_SCISSOR,
   };

   vulkan_init_pipeline_layout(vk);

   /* Input assembly */
   input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

   /* VAO state */
   attributes[0].location  = 0;
   attributes[0].binding   = 0;
   attributes[0].format    = VK_FORMAT_R32G32_SFLOAT;
   attributes[0].offset    = 0;
   attributes[1].location  = 1;
   attributes[1].binding   = 0;
   attributes[1].format    = VK_FORMAT_R32G32_SFLOAT;
   attributes[1].offset    = 2 * sizeof(float);
   attributes[2].location  = 2;
   attributes[2].binding   = 0;
   attributes[2].format    = VK_FORMAT_R32G32B32A32_SFLOAT;
   attributes[2].offset    = 4 * sizeof(float);

   binding.binding         = 0;
   binding.stride          = sizeof(struct vk_vertex);
   binding.inputRate       = VK_VERTEX_INPUT_RATE_VERTEX;

   vertex_input.vertexBindingDescriptionCount   = 1;
   vertex_input.pVertexBindingDescriptions      = &binding;
   vertex_input.vertexAttributeDescriptionCount = 3;
   vertex_input.pVertexAttributeDescriptions    = attributes;

   /* Raster state */
   raster.polygonMode                   = VK_POLYGON_MODE_FILL;
   raster.cullMode                      = VK_CULL_MODE_NONE;
   raster.frontFace                     = VK_FRONT_FACE_COUNTER_CLOCKWISE;
   raster.depthClampEnable              = false;
   raster.rasterizerDiscardEnable       = false;
   raster.depthBiasEnable               = false;
   raster.lineWidth                     = 1.0f;

   /* Blend state */
   blend_attachment.blendEnable         = false;
   blend_attachment.colorWriteMask      = 0xf;
   blend.attachmentCount                = 1;
   blend.pAttachments                   = &blend_attachment;

   /* Viewport state */
   viewport.viewportCount               = 1;
   viewport.scissorCount                = 1;

   /* Depth-stencil state */
   depth_stencil.depthTestEnable        = false;
   depth_stencil.depthWriteEnable       = false;
   depth_stencil.depthBoundsTestEnable  = false;
   depth_stencil.stencilTestEnable      = false;
   depth_stencil.minDepthBounds         = 0.0f;
   depth_stencil.maxDepthBounds         = 1.0f;

   /* Multisample state */
   multisample.rasterizationSamples     = VK_SAMPLE_COUNT_1_BIT;

   /* Dynamic state */
   dynamic.pDynamicStates               = dynamics;
   dynamic.dynamicStateCount            = ARRAY_SIZE(dynamics);

   pipe.stageCount                      = 2;
   pipe.pStages                         = shader_stages;
   pipe.pVertexInputState               = &vertex_input;
   pipe.pInputAssemblyState             = &input_assembly;
   pipe.pRasterizationState             = &raster;
   pipe.pColorBlendState                = &blend;
   pipe.pMultisampleState               = &multisample;
   pipe.pViewportState                  = &viewport;
   pipe.pDepthStencilState              = &depth_stencil;
   pipe.pDynamicState                   = &dynamic;
   pipe.renderPass                      = vk->render_pass;
   pipe.layout                          = vk->pipelines.layout;

   module_info.codeSize                 = sizeof(alpha_blend_vert);
   module_info.pCode                    = alpha_blend_vert;
   shader_stages[0].stage               = VK_SHADER_STAGE_VERTEX_BIT;
   shader_stages[0].pName               = "main";
   vkCreateShaderModule(vk->context->device,
         &module_info, NULL, &shader_stages[0].module);

   blend_attachment.blendEnable         = true;
   blend_attachment.colorWriteMask      = 0xf;
   blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
   blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
   blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
   blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
   blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
   blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;

   /* Glyph pipeline */
   module_info.codeSize                 = sizeof(font_frag);
   module_info.pCode                    = font_frag;
   shader_stages[1].stage               = VK_SHADER_STAGE_FRAGMENT_BIT;
   shader_stages[1].pName               = "main";
   vkCreateShaderModule(vk->context->device,
         &module_info, NULL, &shader_stages[1].module);

   vkCreateGraphicsPipelines(vk->context->device, vk->pipelines.cache,
         1, &pipe, NULL, &vk->pipelines.font);
   vkDestroyShaderModule(vk->context->device, shader_stages[1].module, NULL);

   /* Alpha-blended pipeline. */
   module_info.codeSize   = sizeof(alpha_blend_frag);
   module_info.pCode      = alpha_blend_frag;
   shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
   shader_stages[1].pName = "main";
   vkCreateShaderModule(vk->context->device,
         &module_info, NULL, &shader_stages[1].module);

   vkCreateGraphicsPipelines(vk->context->device, vk->pipelines.cache,
         1, &pipe, NULL, &vk->pipelines.alpha_blend);

   /* Build display pipelines. */
   for (i = 0; i < 4; i++)
   {
      input_assembly.topology = i & 2 ?
         VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP :
         VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
      blend_attachment.blendEnable = i & 1;
      vkCreateGraphicsPipelines(vk->context->device, vk->pipelines.cache,
            1, &pipe, NULL, &vk->display.pipelines[i]);
   }

   vkDestroyShaderModule(vk->context->device, shader_stages[1].module, NULL);

#ifdef VULKAN_HDR_SWAPCHAIN
   blend_attachment.blendEnable = false;

   /* HDR pipeline. */
   module_info.codeSize   = sizeof(hdr_frag);
   module_info.pCode      = hdr_frag;
   shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
   shader_stages[1].pName = "main";
   vkCreateShaderModule(vk->context->device,
         &module_info, NULL, &shader_stages[1].module);

   vkCreateGraphicsPipelines(vk->context->device, vk->pipelines.cache,
         1, &pipe, NULL, &vk->pipelines.hdr);

   /* Build display hdr pipelines. */
   for (i = 4; i < 6; i++)
   {
      input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
      vkCreateGraphicsPipelines(vk->context->device, vk->pipelines.cache,
            1, &pipe, NULL, &vk->display.pipelines[i]);
   }
   
   vkDestroyShaderModule(vk->context->device, shader_stages[1].module, NULL);

   blend_attachment.blendEnable = true; 
#endif /* VULKAN_HDR_SWAPCHAIN */

   vkDestroyShaderModule(vk->context->device, shader_stages[0].module, NULL);

   /* Other menu pipelines. */
   for (i = 0; i < ARRAY_SIZE(vk->display.pipelines) - 6; i++)
   {
      switch (i >> 1)
      {
         case 0:
            module_info.codeSize   = sizeof(pipeline_ribbon_vert);
            module_info.pCode      = pipeline_ribbon_vert;
            break;

         case 1:
            module_info.codeSize   = sizeof(pipeline_ribbon_simple_vert);
            module_info.pCode      = pipeline_ribbon_simple_vert;
            break;

         case 2:
            module_info.codeSize   = sizeof(alpha_blend_vert);
            module_info.pCode      = alpha_blend_vert;
            break;

         case 3:
            module_info.codeSize   = sizeof(alpha_blend_vert);
            module_info.pCode      = alpha_blend_vert;
            break;

         case 4:
            module_info.codeSize   = sizeof(alpha_blend_vert);
            module_info.pCode      = alpha_blend_vert;
            break;

         default:
            break;
      }

      shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
      shader_stages[0].pName = "main";
      vkCreateShaderModule(vk->context->device,
            &module_info, NULL, &shader_stages[0].module);

      switch (i >> 1)
      {
         case 0:
            module_info.codeSize   = sizeof(pipeline_ribbon_frag);
            module_info.pCode      = pipeline_ribbon_frag;
            break;

         case 1:
            module_info.codeSize   = sizeof(pipeline_ribbon_simple_frag);
            module_info.pCode      = pipeline_ribbon_simple_frag;
            break;

         case 2:
            module_info.codeSize   = sizeof(pipeline_snow_simple_frag);
            module_info.pCode      = pipeline_snow_simple_frag;
            break;

         case 3:
            module_info.codeSize   = sizeof(pipeline_snow_frag);
            module_info.pCode      = pipeline_snow_frag;
            break;

         case 4:
            module_info.codeSize   = sizeof(pipeline_bokeh_frag);
            module_info.pCode      = pipeline_bokeh_frag;
            break;

         default:
            break;
      }

      shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      shader_stages[1].pName = "main";
      vkCreateShaderModule(vk->context->device,
            &module_info, NULL, &shader_stages[1].module);

      switch (i >> 1)
      {
         case 0:
         case 1:
            blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            break;
         default:
            blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            break;
      }

      input_assembly.topology = i & 1 ?
         VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP :
         VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

      vkCreateGraphicsPipelines(vk->context->device, vk->pipelines.cache,
            1, &pipe, NULL, &vk->display.pipelines[6 + i]);

      vkDestroyShaderModule(vk->context->device, shader_stages[0].module, NULL);
      vkDestroyShaderModule(vk->context->device, shader_stages[1].module, NULL);
   }
}

static void vulkan_init_samplers(vk_t *vk)
{
   VkSamplerCreateInfo info;

   info.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
   info.pNext                   = NULL;
   info.flags                   = 0;
   info.magFilter               = VK_FILTER_NEAREST;
   info.minFilter               = VK_FILTER_NEAREST;
   info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
   info.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
   info.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
   info.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
   info.mipLodBias              = 0.0f;
   info.anisotropyEnable        = false;
   info.maxAnisotropy           = 1.0f;
   info.compareEnable           = false;
   info.minLod                  = 0.0f;
   info.maxLod                  = 0.0f;
   info.borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
   info.unnormalizedCoordinates = false;
   vkCreateSampler(vk->context->device,
         &info, NULL, &vk->samplers.nearest);

   info.magFilter               = VK_FILTER_LINEAR;
   info.minFilter               = VK_FILTER_LINEAR;
   vkCreateSampler(vk->context->device,
         &info, NULL, &vk->samplers.linear);

   info.maxLod                  = VK_LOD_CLAMP_NONE;
   info.magFilter               = VK_FILTER_NEAREST;
   info.minFilter               = VK_FILTER_NEAREST;
   info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
   vkCreateSampler(vk->context->device,
         &info, NULL, &vk->samplers.mipmap_nearest);

   info.magFilter               = VK_FILTER_LINEAR;
   info.minFilter               = VK_FILTER_LINEAR;
   info.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
   vkCreateSampler(vk->context->device,
         &info, NULL, &vk->samplers.mipmap_linear);
}

static void vulkan_buffer_chain_free(
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


static void vulkan_deinit_buffers(vk_t *vk)
{
   int i;
   for (i = 0; i < (int) vk->num_swapchain_images; i++)
   {
      vulkan_buffer_chain_free(
            vk->context->device, &vk->swapchain[i].vbo);
      vulkan_buffer_chain_free(
            vk->context->device, &vk->swapchain[i].ubo);
   }
}

static void vulkan_deinit_descriptor_pool(vk_t *vk)
{
   int i;
   for (i = 0; i < (int) vk->num_swapchain_images; i++)
      vulkan_destroy_descriptor_manager(
            vk->context->device,
            &vk->swapchain[i].descriptor_manager);
}

static void vulkan_init_textures(vk_t *vk)
{
   const uint32_t zero = 0;

   if (!(vk->flags & VK_FLAG_HW_ENABLE))
   {
      int i;
      for (i = 0; i < (int) vk->num_swapchain_images; i++)
      {
         vk->swapchain[i].texture = vulkan_create_texture(
               vk, NULL, vk->tex_w, vk->tex_h, vk->tex_fmt,
               NULL, NULL, VULKAN_TEXTURE_STREAMED);

         {
            struct vk_texture *texture = &vk->swapchain[i].texture;
            VK_MAP_PERSISTENT_TEXTURE(vk->context->device, texture);
         }

         if (vk->swapchain[i].texture.type == VULKAN_TEXTURE_STAGING)
            vk->swapchain[i].texture_optimal = vulkan_create_texture(
                  vk, NULL, vk->tex_w, vk->tex_h, vk->tex_fmt,
                  NULL, NULL, VULKAN_TEXTURE_DYNAMIC);
      }
   }

   vk->default_texture = vulkan_create_texture(vk, NULL,
         1, 1, VK_FORMAT_B8G8R8A8_UNORM,
         &zero, NULL, VULKAN_TEXTURE_STATIC);
}

static void vulkan_deinit_textures(vk_t *vk)
{
   int i;
   const void* cached_frame;

   /* Avoid memcpying from a destroyed/unmapped texture later on. */
   video_driver_cached_frame_get(&cached_frame, NULL, NULL, NULL);
   if (vulkan_is_mapped_swapchain_texture_ptr(vk, cached_frame))
      video_driver_set_cached_frame_ptr(NULL);

   vkDestroySampler(vk->context->device, vk->samplers.nearest,        NULL);
   vkDestroySampler(vk->context->device, vk->samplers.linear,         NULL);
   vkDestroySampler(vk->context->device, vk->samplers.mipmap_nearest, NULL);
   vkDestroySampler(vk->context->device, vk->samplers.mipmap_linear,  NULL);

   for (i = 0; i < (int) vk->num_swapchain_images; i++)
   {
      if (vk->swapchain[i].texture.memory != VK_NULL_HANDLE)
         vulkan_destroy_texture(
               vk->context->device, &vk->swapchain[i].texture);

      if (vk->swapchain[i].texture_optimal.memory != VK_NULL_HANDLE)
         vulkan_destroy_texture(
               vk->context->device, &vk->swapchain[i].texture_optimal);
   }

   if (vk->default_texture.memory != VK_NULL_HANDLE)
      vulkan_destroy_texture(vk->context->device, &vk->default_texture);
}

static void vulkan_deinit_command_buffers(vk_t *vk)
{
   int i;
   for (i = 0; i < (int) vk->num_swapchain_images; i++)
   {
      if (vk->swapchain[i].cmd)
         vkFreeCommandBuffers(vk->context->device,
               vk->swapchain[i].cmd_pool, 1, &vk->swapchain[i].cmd);

      vkDestroyCommandPool(vk->context->device,
            vk->swapchain[i].cmd_pool, NULL);
   }
}

static void vulkan_deinit_pipelines(vk_t *vk)
{
   int i;

   vkDestroyPipelineLayout(vk->context->device,
         vk->pipelines.layout, NULL);
   vkDestroyDescriptorSetLayout(vk->context->device,
         vk->pipelines.set_layout, NULL);
   vkDestroyPipeline(vk->context->device,
         vk->pipelines.alpha_blend, NULL);
   vkDestroyPipeline(vk->context->device,
         vk->pipelines.font, NULL);
#ifdef VULKAN_HDR_SWAPCHAIN
   vkDestroyPipeline(vk->context->device,
         vk->pipelines.hdr, NULL);
#endif /* VULKAN_HDR_SWAPCHAIN */

   for (i = 0; i < ARRAY_SIZE(vk->display.pipelines); i++)
      vkDestroyPipeline(vk->context->device,
            vk->display.pipelines[i], NULL);
}

static void vulkan_deinit_framebuffers(vk_t *vk)
{
   int i;
   for (i = 0; i < (int) vk->num_swapchain_images; i++)
   {
      if (vk->backbuffers[i].framebuffer)
         vkDestroyFramebuffer(vk->context->device,
               vk->backbuffers[i].framebuffer, NULL);

      if (vk->backbuffers[i].view)
         vkDestroyImageView(vk->context->device,
               vk->backbuffers[i].view, NULL);
   }

   vkDestroyRenderPass(vk->context->device, vk->render_pass, NULL);
}

#ifdef VULKAN_HDR_SWAPCHAIN
static void vulkan_set_hdr_max_nits(void* data, float max_nits)
{
   vk_t *vk                            = (vk_t*)data;
   vulkan_hdr_uniform_t* mapped_ubo    = (vulkan_hdr_uniform_t*)vk->hdr.ubo.mapped;

   vk->hdr.max_output_nits             = max_nits;
   mapped_ubo->max_nits                = max_nits;
}

static void vulkan_set_hdr_paper_white_nits(void* data, float paper_white_nits)
{
   vk_t *vk                            = (vk_t*)data;
   vulkan_hdr_uniform_t* mapped_ubo    = (vulkan_hdr_uniform_t*)vk->hdr.ubo.mapped;

   mapped_ubo->paper_white_nits = paper_white_nits;
}

static void vulkan_set_hdr_contrast(void* data, float contrast)
{
   vk_t *vk                            = (vk_t*)data;
   vulkan_hdr_uniform_t* mapped_ubo    = (vulkan_hdr_uniform_t*)vk->hdr.ubo.mapped;

   mapped_ubo->contrast                = contrast;
}

static void vulkan_set_hdr_expand_gamut(void* data, bool expand_gamut)
{
   vk_t *vk                            = (vk_t*)data;
   vulkan_hdr_uniform_t* mapped_ubo    = (vulkan_hdr_uniform_t*)vk->hdr.ubo.mapped;

   mapped_ubo->expand_gamut     = expand_gamut ? 1.0f : 0.0f;
}

static void vulkan_set_hdr_inverse_tonemap(vk_t* vk, bool inverse_tonemap)
{
   vulkan_hdr_uniform_t* mapped_ubo = (vulkan_hdr_uniform_t*)vk->hdr.ubo.mapped;

   mapped_ubo->inverse_tonemap      = inverse_tonemap ? 1.0f : 0.0f;
}

static void vulkan_set_hdr10(vk_t* vk, bool hdr10)
{
   vulkan_hdr_uniform_t* mapped_ubo = (vulkan_hdr_uniform_t*)vk->hdr.ubo.mapped;

   mapped_ubo->hdr10                = hdr10 ? 1.0f : 0.0f;
}
#endif /* VULKAN_HDR_SWAPCHAIN */

static bool vulkan_init_default_filter_chain(vk_t *vk)
{
   struct vulkan_filter_chain_create_info info;

   if (!vk->context)
      return false;

   info.device                = vk->context->device;
   info.gpu                   = vk->context->gpu;
   info.memory_properties     = &vk->context->memory_properties;
   info.pipeline_cache        = vk->pipelines.cache;
   info.queue                 = vk->context->queue;
   info.command_pool          = vk->swapchain[vk->context->current_frame_index].cmd_pool;
   info.num_passes            = 0;
   info.original_format       = vk->tex_fmt;
   info.max_input_size.width  = vk->tex_w;
   info.max_input_size.height = vk->tex_h;
   info.swapchain.viewport    = vk->vk_vp;
   info.swapchain.format      = vk->context->swapchain_format;
   info.swapchain.render_pass = vk->render_pass;
   info.swapchain.num_indices = vk->context->num_swapchain_images;

   vk->filter_chain           = vulkan_filter_chain_create_default(
         &info,
         vk->video.smooth 
         ? GLSLANG_FILTER_CHAIN_LINEAR 
         : GLSLANG_FILTER_CHAIN_NEAREST);

   if (!vk->filter_chain)
   {
      RARCH_ERR("Failed to create filter chain.\n");
      return false;
   }

#ifdef VULKAN_HDR_SWAPCHAIN
   if (vk->context->flags & VK_CTX_FLAG_HDR_ENABLE)
   {
      struct video_shader* shader_preset = vulkan_filter_chain_get_preset(
      vk->filter_chain); 
      VkFormat rt_format = (shader_preset && shader_preset->passes) ?
         vulkan_filter_chain_get_pass_rt_format(vk->filter_chain, shader_preset->passes - 1) : VK_FORMAT_UNDEFINED;
      bool emits_hdr10 = shader_preset && shader_preset->passes && vulkan_filter_chain_emits_hdr10(vk->filter_chain);

      switch (rt_format)
      {
         case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
            /* If the last shader pass uses a RGB10A2 back buffer 
             * and HDR has been enabled, assume we want to skip 
             * the inverse tonemapper and HDR10 conversion.
             * If we just inherited HDR10 format based on backbuffer,
             * we would have used RGBA8, and thus we should do inverse tonemap as expected. */
            vulkan_set_hdr_inverse_tonemap(vk, !emits_hdr10);
            vulkan_set_hdr10(vk, !emits_hdr10);
            vk->flags |= VK_FLAG_SHOULD_RESIZE;
            break;
         case VK_FORMAT_R16G16B16A16_SFLOAT:
            /* If the last shader pass uses a RGBA16 backbuffer 
             * and HDR has been enabled, assume we want to 
             * skip the inverse tonemapper */
            vulkan_set_hdr_inverse_tonemap(vk, false);
            vulkan_set_hdr10(vk, true);
            vk->flags |= VK_FLAG_SHOULD_RESIZE;
            break;
         case VK_FORMAT_UNDEFINED:
         default:
            vulkan_set_hdr_inverse_tonemap(vk, true);
            vulkan_set_hdr10(vk, true);
            break;
      }
   } 
#endif /* VULKAN_HDR_SWAPCHAIN */

   return true;
}

static bool vulkan_init_filter_chain_preset(vk_t *vk, const char *shader_path)
{
   struct vulkan_filter_chain_create_info info;

   info.device                = vk->context->device;
   info.gpu                   = vk->context->gpu;
   info.memory_properties     = &vk->context->memory_properties;
   info.pipeline_cache        = vk->pipelines.cache;
   info.queue                 = vk->context->queue;
   info.command_pool          = vk->swapchain[vk->context->current_frame_index].cmd_pool;
   info.num_passes            = 0;
   info.original_format       = vk->tex_fmt;
   info.max_input_size.width  = vk->tex_w;
   info.max_input_size.height = vk->tex_h;
   info.swapchain.viewport    = vk->vk_vp;
   info.swapchain.format      = vk->context->swapchain_format;
   info.swapchain.render_pass = vk->render_pass;
   info.swapchain.num_indices = vk->context->num_swapchain_images;

   vk->filter_chain           = vulkan_filter_chain_create_from_preset(
         &info, shader_path,
         vk->video.smooth
         ? GLSLANG_FILTER_CHAIN_LINEAR 
         : GLSLANG_FILTER_CHAIN_NEAREST);

   if (!vk->filter_chain)
   {
      RARCH_ERR("[Vulkan]: Failed to create preset: \"%s\".\n", shader_path);
      return false;
   }

#ifdef VULKAN_HDR_SWAPCHAIN
   if (vk->context->flags & VK_CTX_FLAG_HDR_ENABLE)
   {
      struct video_shader* shader_preset = vulkan_filter_chain_get_preset(vk->filter_chain); 
      VkFormat rt_format = (shader_preset && shader_preset->passes) ? vulkan_filter_chain_get_pass_rt_format(vk->filter_chain, shader_preset->passes - 1) : VK_FORMAT_UNDEFINED;
      bool emits_hdr10 = shader_preset && shader_preset->passes && vulkan_filter_chain_emits_hdr10(vk->filter_chain);

      switch (rt_format)
      {
         case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
            /* If the last shader pass uses a RGB10A2 backbuffer 
             * and HDR has been enabled, assume we want to 
             * skip the inverse tonemapper and HDR10 conversion
             * If we just inherited HDR10 format based on backbuffer,
             * we would have used RGBA8, and thus we should do inverse tonemap as expected. */
            vulkan_set_hdr_inverse_tonemap(vk, !emits_hdr10);
            vulkan_set_hdr10(vk, !emits_hdr10);
            vk->flags |= VK_FLAG_SHOULD_RESIZE;
            break;
         case VK_FORMAT_R16G16B16A16_SFLOAT:
            /* If the last shader pass uses a RGBA16 backbuffer 
             * and HDR has been enabled, assume we want to 
             * skip the inverse tonemapper */
            vulkan_set_hdr_inverse_tonemap(vk, false);
            vulkan_set_hdr10(vk, true);
            vk->flags |= VK_FLAG_SHOULD_RESIZE;
            break;
         case VK_FORMAT_UNDEFINED:
         default:
            vulkan_set_hdr_inverse_tonemap(vk, true);
            vulkan_set_hdr10(vk, true);
            break;
      }
   } 
#endif /* VULKAN_HDR_SWAPCHAIN */

   return true;
}

static bool vulkan_init_filter_chain(vk_t *vk)
{
   const char     *shader_path = video_shader_get_current_shader_preset();
   enum rarch_shader_type type = video_shader_parse_type(shader_path);

   if (string_is_empty(shader_path))
   {
      RARCH_LOG("[Vulkan]: Loading stock shader.\n");
      return vulkan_init_default_filter_chain(vk);
   }

   if (type != RARCH_SHADER_SLANG)
   {
      RARCH_LOG("[Vulkan]: Only Slang shaders are supported, falling back to stock.\n");
      return vulkan_init_default_filter_chain(vk);
   }

   if (!shader_path || !vulkan_init_filter_chain_preset(vk, shader_path))
      vulkan_init_default_filter_chain(vk);

   return true;
}

static void vulkan_init_static_resources(vk_t *vk)
{
   int i;
   uint32_t blank[4 * 4];
   VkCommandPoolCreateInfo pool_info = {
      VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };

   /* Create the pipeline cache. */
   VkPipelineCacheCreateInfo cache   = {
      VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO };

   pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

   if (!vk->context)
      return;

   vkCreatePipelineCache(vk->context->device,
         &cache, NULL, &vk->pipelines.cache);

   pool_info.queueFamilyIndex = vk->context->graphics_queue_index;

   vkCreateCommandPool(vk->context->device,
         &pool_info, NULL, &vk->staging_pool);

   for (i = 0; i < 4 * 4; i++)
      blank[i] = -1u;

   vk->display.blank_texture = vulkan_create_texture(vk, NULL,
         4, 4, VK_FORMAT_B8G8R8A8_UNORM,
         blank, NULL, VULKAN_TEXTURE_STATIC);
}

static void vulkan_deinit_static_resources(vk_t *vk)
{
   int i;
   vkDestroyPipelineCache(vk->context->device,
         vk->pipelines.cache, NULL);
   vulkan_destroy_texture(
         vk->context->device,
         &vk->display.blank_texture);

   vkDestroyCommandPool(vk->context->device,
         vk->staging_pool, NULL);
   free(vk->hw.cmd);
   free(vk->hw.wait_dst_stages);
   free(vk->hw.semaphores);

   for (i = 0; i < VULKAN_MAX_SWAPCHAIN_IMAGES; i++)
      if (vk->readback.staging[i].memory != VK_NULL_HANDLE)
         vulkan_destroy_texture(
               vk->context->device,
               &vk->readback.staging[i]);
}

static void vulkan_deinit_menu(vk_t *vk)
{
   int i;
   for (i = 0; i < VULKAN_MAX_SWAPCHAIN_IMAGES; i++)
   {
      if (vk->menu.textures[i].memory)
         vulkan_destroy_texture(
               vk->context->device, &vk->menu.textures[i]);
      if (vk->menu.textures_optimal[i].memory)
         vulkan_destroy_texture(
               vk->context->device, &vk->menu.textures_optimal[i]);
   }
}

#ifdef VULKAN_HDR_SWAPCHAIN
static void vulkan_destroy_hdr_buffer(VkDevice device, struct vk_image *img)
{
   vkDestroyImageView(device, img->view, NULL);
   vkDestroyImage(device, img->image, NULL);
   vkDestroyFramebuffer(device, img->framebuffer, NULL);
   vkFreeMemory(device, img->memory, NULL);
   memset(img, 0, sizeof(*img));
}
#endif

static void vulkan_free(void *data)
{
   vk_t *vk = (vk_t*)data;
   if (!vk)
      return;

   if (vk->context && vk->context->device)
   {
#ifdef HAVE_THREADS
      slock_lock(vk->context->queue_lock);
#endif
      vkQueueWaitIdle(vk->context->queue);
#ifdef HAVE_THREADS
      slock_unlock(vk->context->queue_lock);
#endif
      vulkan_deinit_pipelines(vk);
      vulkan_deinit_framebuffers(vk);
      vulkan_deinit_descriptor_pool(vk);
      vulkan_deinit_textures(vk);
      vulkan_deinit_buffers(vk);
      vulkan_deinit_command_buffers(vk);

      /* No need to init this since textures are create on-demand. */
      vulkan_deinit_menu(vk);

      font_driver_free_osd();

      vulkan_deinit_static_resources(vk);
#ifdef HAVE_OVERLAY
      vulkan_overlay_free(vk);
#endif

      if (vk->filter_chain)
         vulkan_filter_chain_free((vulkan_filter_chain_t*)vk->filter_chain);

#ifdef VULKAN_HDR_SWAPCHAIN
      vulkan_destroy_buffer(vk->context->device, &vk->hdr.ubo);
      vulkan_destroy_hdr_buffer(vk->context->device, &vk->main_buffer);
      video_driver_unset_hdr_support();
#endif /* VULKAN_HDR_SWAPCHAIN */

      if (vk->ctx_driver && vk->ctx_driver->destroy)
         vk->ctx_driver->destroy(vk->ctx_data);
      video_context_driver_free();
   }

   scaler_ctx_gen_reset(&vk->readback.scaler_bgr);
   scaler_ctx_gen_reset(&vk->readback.scaler_rgb);
   free(vk);
}

static uint32_t vulkan_get_sync_index(void *handle)
{
   vk_t *vk = (vk_t*)handle;
   return vk->context->current_frame_index;
}

static uint32_t vulkan_get_sync_index_mask(void *handle)
{
   vk_t *vk = (vk_t*)handle;
   return (1 << vk->context->num_swapchain_images) - 1;
}

static void vulkan_set_image(void *handle,
      const struct retro_vulkan_image *image,
      uint32_t num_semaphores,
      const VkSemaphore *semaphores,
      uint32_t src_queue_family)
{
   vk_t *vk              = (vk_t*)handle;

   vk->hw.image          = image;
   vk->hw.num_semaphores = num_semaphores;

   if (num_semaphores > 0)
   {
      int i;

      /* Allocate one extra in case we need to use WSI acquire semaphores. */
      VkPipelineStageFlags *stage_flags = (VkPipelineStageFlags*)realloc(vk->hw.wait_dst_stages,
            sizeof(VkPipelineStageFlags) * (vk->hw.num_semaphores + 1));

      VkSemaphore *new_semaphores = (VkSemaphore*)realloc(vk->hw.semaphores,
            sizeof(VkSemaphore) * (vk->hw.num_semaphores + 1));

      vk->hw.wait_dst_stages = stage_flags;
      vk->hw.semaphores      = new_semaphores;

      for (i = 0; i < (int) vk->hw.num_semaphores; i++)
      {
         vk->hw.wait_dst_stages[i] = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
         vk->hw.semaphores[i]      = semaphores[i];
      }

      vk->flags                   |= VK_FLAG_HW_VALID_SEMAPHORE;
      vk->hw.src_queue_family      = src_queue_family;
   }
}

static void vulkan_wait_sync_index(void *handle)
{
   /* no-op. RetroArch already waits for this
    * in gfx_ctx_swap_buffers(). */
}

static void vulkan_set_command_buffers(void *handle, uint32_t num_cmd,
      const VkCommandBuffer *cmd)
{
   vk_t *vk                   = (vk_t*)handle;
   unsigned required_capacity = num_cmd + 1;
   if (required_capacity > vk->hw.capacity_cmd)
   {
      VkCommandBuffer *hw_cmd = (VkCommandBuffer*)
         realloc(vk->hw.cmd,
            sizeof(VkCommandBuffer) * required_capacity);

      vk->hw.cmd              = hw_cmd;
      vk->hw.capacity_cmd     = required_capacity;
   }

   vk->hw.num_cmd             = num_cmd;
   memcpy(vk->hw.cmd, cmd, sizeof(VkCommandBuffer) * num_cmd);
}

static void vulkan_lock_queue(void *handle)
{
#ifdef HAVE_THREADS
   vk_t *vk = (vk_t*)handle;
   slock_lock(vk->context->queue_lock);
#endif
}

static void vulkan_unlock_queue(void *handle)
{
#ifdef HAVE_THREADS
   vk_t *vk = (vk_t*)handle;
   slock_unlock(vk->context->queue_lock);
#endif
}

static void vulkan_set_signal_semaphore(void *handle, VkSemaphore semaphore)
{
   vk_t *vk = (vk_t*)handle;
   vk->hw.signal_semaphore = semaphore;
}

static void vulkan_init_hw_render(vk_t *vk)
{
   struct retro_hw_render_interface_vulkan *iface   =
      &vk->hw.iface;
   struct retro_hw_render_callback *hwr =
      video_driver_get_hw_context();

   if (hwr->context_type != RETRO_HW_CONTEXT_VULKAN)
      return;

   vk->flags                    |= VK_FLAG_HW_ENABLE;

   iface->interface_type         = RETRO_HW_RENDER_INTERFACE_VULKAN;
   iface->interface_version      = RETRO_HW_RENDER_INTERFACE_VULKAN_VERSION;
   iface->instance               = vk->context->instance;
   iface->gpu                    = vk->context->gpu;
   iface->device                 = vk->context->device;

   iface->queue                  = vk->context->queue;
   iface->queue_index            = vk->context->graphics_queue_index;

   iface->handle                 = vk;
   iface->set_image              = vulkan_set_image;
   iface->get_sync_index         = vulkan_get_sync_index;
   iface->get_sync_index_mask    = vulkan_get_sync_index_mask;
   iface->wait_sync_index        = vulkan_wait_sync_index;
   iface->set_command_buffers    = vulkan_set_command_buffers;
   iface->lock_queue             = vulkan_lock_queue;
   iface->unlock_queue           = vulkan_unlock_queue;
   iface->set_signal_semaphore   = vulkan_set_signal_semaphore;

   iface->get_device_proc_addr   = vkGetDeviceProcAddr;
   iface->get_instance_proc_addr = vulkan_symbol_wrapper_instance_proc_addr();
}

static void vulkan_init_readback(vk_t *vk, settings_t *settings)
{
   /* Only bother with this if we're doing GPU recording.
    * Check recording_st->enable and not
    * driver.recording_data, because recording is
    * not initialized yet.
    */
   recording_state_t 
      *recording_st        = recording_state_get_ptr();
   bool recording_enabled  = recording_st->enable;
   bool video_gpu_record   = settings->bools.video_gpu_record;

   if (!(video_gpu_record && recording_enabled))
   {
      vk->flags                       &= ~VK_FLAG_READBACK_STREAMED;
      return;
   }

   vk->flags                          |=  VK_FLAG_READBACK_STREAMED;

   vk->readback.scaler_bgr.in_width    = vk->vp.width;
   vk->readback.scaler_bgr.in_height   = vk->vp.height;
   vk->readback.scaler_bgr.out_width   = vk->vp.width;
   vk->readback.scaler_bgr.out_height  = vk->vp.height;
   vk->readback.scaler_bgr.in_fmt      = SCALER_FMT_ARGB8888;
   vk->readback.scaler_bgr.out_fmt     = SCALER_FMT_BGR24;
   vk->readback.scaler_bgr.scaler_type = SCALER_TYPE_POINT;

   vk->readback.scaler_rgb.in_width    = vk->vp.width;
   vk->readback.scaler_rgb.in_height   = vk->vp.height;
   vk->readback.scaler_rgb.out_width   = vk->vp.width;
   vk->readback.scaler_rgb.out_height  = vk->vp.height;
   vk->readback.scaler_rgb.in_fmt      = SCALER_FMT_ABGR8888;
   vk->readback.scaler_rgb.out_fmt     = SCALER_FMT_BGR24;
   vk->readback.scaler_rgb.scaler_type = SCALER_TYPE_POINT;

   if (!scaler_ctx_gen_filter(&vk->readback.scaler_bgr))
   {
      vk->flags &= ~VK_FLAG_READBACK_STREAMED;
      RARCH_ERR("[Vulkan]: Failed to initialize scaler context.\n");
   }

   if (!scaler_ctx_gen_filter(&vk->readback.scaler_rgb))
   {
      vk->flags &= ~VK_FLAG_READBACK_STREAMED;
      RARCH_ERR("[Vulkan]: Failed to initialize scaler context.\n");
   }
}

static void *vulkan_init(const video_info_t *video,
      input_driver_t **input,
      void **input_data)
{
   unsigned full_x, full_y;
   unsigned win_width;
   unsigned win_height;
   unsigned mode_width                = 0;
   unsigned mode_height               = 0;
   int interval                       = 0;
   unsigned temp_width                = 0;
   unsigned temp_height               = 0;
   const gfx_ctx_driver_t *ctx_driver = NULL;
   settings_t *settings               = config_get_ptr();
#ifdef VULKAN_HDR_SWAPCHAIN
   vulkan_hdr_uniform_t* mapped_ubo   = NULL;
#endif
   vk_t *vk                           = (vk_t*)calloc(1, sizeof(*vk));
   if (!vk)
      return NULL;
   ctx_driver                         = vulkan_get_context(vk, settings);
   if (!ctx_driver)
   {
      RARCH_ERR("[Vulkan]: Failed to get Vulkan context.\n");
      goto error;
   }

#ifdef VULKAN_HDR_SWAPCHAIN
   vk->hdr.max_output_nits             = settings->floats.video_hdr_max_nits;
   vk->hdr.min_output_nits             = 0.001f;
   vk->hdr.max_cll                     = 0.0f;
   vk->hdr.max_fall                    = 0.0f;
#endif /* VULKAN_HDR_SWAPCHAIN */

   vk->video                          = *video;
   vk->ctx_driver                     = ctx_driver;

   video_context_driver_set((const gfx_ctx_driver_t*)ctx_driver);
   
   RARCH_LOG("[Vulkan]: Found vulkan context: \"%s\".\n", ctx_driver->ident);

   if (vk->ctx_driver->get_video_size)
      vk->ctx_driver->get_video_size(vk->ctx_data,
            &mode_width, &mode_height);

   full_x                             = mode_width;
   full_y                             = mode_height;
   mode_width                         = 0;
   mode_height                        = 0;

   RARCH_LOG("[Vulkan]: Detecting screen resolution: %ux%u.\n", full_x, full_y);
   interval = video->vsync ? video->swap_interval : 0;

   if (ctx_driver->swap_interval)
   {
      bool adaptive_vsync_enabled            = video_driver_test_all_flags(
            GFX_CTX_FLAGS_ADAPTIVE_VSYNC) && video->adaptive_vsync;
      if (adaptive_vsync_enabled && interval == 1)
         interval = -1;
      ctx_driver->swap_interval(vk->ctx_data, interval);
   }

   win_width  = video->width;
   win_height = video->height;

   if (video->fullscreen && (win_width == 0) && (win_height == 0))
   {
      win_width  = full_x;
      win_height = full_y;
   }

   if (     !vk->ctx_driver->set_video_mode
         || !vk->ctx_driver->set_video_mode(vk->ctx_data,
            win_width, win_height, video->fullscreen))
   {
      RARCH_ERR("[Vulkan]: Failed to set video mode.\n");
      goto error;
   }

   if (vk->ctx_driver->get_video_size)
      vk->ctx_driver->get_video_size(vk->ctx_data,
            &mode_width, &mode_height);

   temp_width  = mode_width;
   temp_height = mode_height;

   if (temp_width != 0 && temp_height != 0)
      video_driver_set_size(temp_width, temp_height);
   video_driver_get_size(&temp_width, &temp_height);
   vk->video_width       = temp_width;
   vk->video_height      = temp_height;

   RARCH_LOG("[Vulkan]: Using resolution %ux%u.\n", temp_width, temp_height);

   if (!vk->ctx_driver || !vk->ctx_driver->get_context_data)
   {
      RARCH_ERR("[Vulkan]: Failed to get context data.\n");
      goto error;
   }

   *(void**)&vk->context = vk->ctx_driver->get_context_data(vk->ctx_data);

   if (video->vsync)
      vk->flags         |=  VK_FLAG_VSYNC;
   else
      vk->flags         &= ~VK_FLAG_VSYNC;
   if (video->fullscreen)
      vk->flags         |=  VK_FLAG_FULLSCREEN;
   else
      vk->flags         &= ~VK_FLAG_FULLSCREEN;
   vk->tex_w             = RARCH_SCALE_BASE * video->input_scale;
   vk->tex_h             = RARCH_SCALE_BASE * video->input_scale;
   vk->tex_fmt           = video->rgb32
      ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_R5G6B5_UNORM_PACK16;
   if (video->force_aspect)
      vk->flags         |=  VK_FLAG_KEEP_ASPECT;
   else
      vk->flags         &= ~VK_FLAG_KEEP_ASPECT;
   RARCH_LOG("[Vulkan]: Using %s format.\n", video->rgb32 ? "BGRA8888" : "RGB565");

   /* Set the viewport to fix recording, since it needs to know
    * the viewport sizes before we start running. */
   vulkan_set_viewport(vk, temp_width, temp_height, false, true);

#ifdef VULKAN_HDR_SWAPCHAIN
   vk->hdr.ubo                  = vulkan_create_buffer(vk->context, sizeof(vulkan_hdr_uniform_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

   mapped_ubo                   = (vulkan_hdr_uniform_t*)vk->hdr.ubo.mapped;

   mapped_ubo->mvp              = vk->mvp_no_rot; 
   mapped_ubo->max_nits         = settings->floats.video_hdr_max_nits;
   mapped_ubo->paper_white_nits = settings->floats.video_hdr_paper_white_nits;
   mapped_ubo->contrast         = VIDEO_HDR_MAX_CONTRAST - settings->floats.video_hdr_display_contrast;
   mapped_ubo->expand_gamut     = settings->bools.video_hdr_expand_gamut;
   mapped_ubo->inverse_tonemap  = 1.0f;     /* Use this to turn on/off the inverse tonemap */
   mapped_ubo->hdr10            = 1.0f;     /* Use this to turn on/off the hdr10 */
#endif /* VULKAN_HDR_SWAPCHAIN */

   vulkan_init_hw_render(vk);
   vulkan_init_static_resources(vk);
   if (vk->context)
   {
      int i;
      static const VkDescriptorPoolSize pool_sizes[2] = {
         { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS },
         { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS * 2 },
      };
      vk->num_swapchain_images = vk->context->num_swapchain_images;

      vulkan_init_render_pass(vk);
      vulkan_init_framebuffers(vk);
      vulkan_init_pipelines(vk);
      vulkan_init_samplers(vk);
      vulkan_init_textures(vk);

      for (i = 0; i < (int) vk->num_swapchain_images; i++)
      {
         VkCommandPoolCreateInfo pool_info;
         VkCommandBufferAllocateInfo info;

         vk->swapchain[i].descriptor_manager =
            vulkan_create_descriptor_manager(
                  vk->context->device,
                  pool_sizes, 2, vk->pipelines.set_layout);
         vk->swapchain[i].vbo                = 
            vulkan_buffer_chain_init(
               VULKAN_BUFFER_BLOCK_SIZE, 16,
               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
         vk->swapchain[i].ubo                = 
            vulkan_buffer_chain_init(
               VULKAN_BUFFER_BLOCK_SIZE,
               vk->context->gpu_properties.limits.minUniformBufferOffsetAlignment,
               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

         pool_info.sType            =
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
         pool_info.pNext            = NULL;
         /* RESET_COMMAND_BUFFER_BIT allows command buffer to be reset. */
         pool_info.flags            =
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
         pool_info.queueFamilyIndex = vk->context->graphics_queue_index;

         vkCreateCommandPool(vk->context->device,
               &pool_info, NULL, &vk->swapchain[i].cmd_pool);

         info.sType                 =
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
         info.pNext                 = NULL;
         info.commandPool           = vk->swapchain[i].cmd_pool;
         info.level                 = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
         info.commandBufferCount    = 1;

         vkAllocateCommandBuffers(vk->context->device,
               &info, &vk->swapchain[i].cmd);
      }
   }

   if (!vulkan_init_filter_chain(vk))
   {
      RARCH_ERR("[Vulkan]: Failed to init filter chain.\n");
      goto error;
   }

   if (vk->ctx_driver->input_driver)
   {
      const char *joypad_name = settings->arrays.input_joypad_driver;
      vk->ctx_driver->input_driver(
            vk->ctx_data, joypad_name,
            input, input_data);
   }

   if (video->font_enable)
      font_driver_init_osd(vk,
            video,
            false,
            video->is_threaded,
            FONT_DRIVER_RENDER_VULKAN_API);

#if OSX
   // The MoltenVK driver needs this, particularly after driver reinit
   vk->flags |= VK_FLAG_SHOULD_RESIZE;
#endif

   vulkan_init_readback(vk, settings);
   return vk;

error:
   vulkan_free(vk);
   return NULL;
}

static void vulkan_check_swapchain(vk_t *vk)
{
   struct vulkan_filter_chain_swapchain_info filter_info;

#ifdef HAVE_THREADS
   slock_lock(vk->context->queue_lock);
#endif
   vkQueueWaitIdle(vk->context->queue);
#ifdef HAVE_THREADS
   slock_unlock(vk->context->queue_lock);
#endif
   vulkan_deinit_pipelines(vk);
   vulkan_deinit_framebuffers(vk);
   vulkan_deinit_descriptor_pool(vk);
   vulkan_deinit_textures(vk);
   vulkan_deinit_buffers(vk);
   vulkan_deinit_command_buffers(vk);
   if (vk->context)
   {
      int i;
      static const VkDescriptorPoolSize pool_sizes[2] = {
         { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS },
         { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VULKAN_DESCRIPTOR_MANAGER_BLOCK_SETS * 2 },
      };
      vk->num_swapchain_images = vk->context->num_swapchain_images;

      vulkan_init_render_pass(vk);
      vulkan_init_framebuffers(vk);
      vulkan_init_pipelines(vk);
      vulkan_init_samplers(vk);
      vulkan_init_textures(vk);

      for (i = 0; i < (int) vk->num_swapchain_images; i++)
      {
         VkCommandPoolCreateInfo pool_info;
         VkCommandBufferAllocateInfo info;

         vk->swapchain[i].descriptor_manager =
            vulkan_create_descriptor_manager(
                  vk->context->device,
                  pool_sizes, 2, vk->pipelines.set_layout);

         vk->swapchain[i].vbo       = vulkan_buffer_chain_init(
               VULKAN_BUFFER_BLOCK_SIZE,
               16,
               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
         vk->swapchain[i].ubo       = vulkan_buffer_chain_init(
               VULKAN_BUFFER_BLOCK_SIZE,
               vk->context->gpu_properties.limits.minUniformBufferOffsetAlignment,
               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

         pool_info.sType            =
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
         pool_info.pNext            = NULL;
         /* RESET_COMMAND_BUFFER_BIT allows command buffer to be reset. */
         pool_info.flags            =
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
         pool_info.queueFamilyIndex = vk->context->graphics_queue_index;

         vkCreateCommandPool(vk->context->device,
               &pool_info, NULL, &vk->swapchain[i].cmd_pool);

         info.sType                 =
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
         info.pNext                 = NULL;
         info.commandPool           = vk->swapchain[i].cmd_pool;
         info.level                 = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
         info.commandBufferCount    = 1;

         vkAllocateCommandBuffers(vk->context->device,
               &info, &vk->swapchain[i].cmd);
      }
   }
   vk->context->flags              &= ~VK_CTX_FLAG_INVALID_SWAPCHAIN;

   filter_info.viewport             = vk->vk_vp;
   filter_info.format               = vk->context->swapchain_format;
   filter_info.render_pass          = vk->render_pass;
   filter_info.num_indices          = vk->context->num_swapchain_images;
   if (
       !vulkan_filter_chain_update_swapchain_info(
          (vulkan_filter_chain_t*)vk->filter_chain,
          &filter_info)
      )
      RARCH_ERR("Failed to update filter chain info. This will probably lead to a crash ...\n");
}

static void vulkan_set_nonblock_state(void *data, bool state,
      bool adaptive_vsync_enabled,
      unsigned swap_interval)
{
   vk_t *vk                    = (vk_t*)data;

   if (!vk)
      return;

   if (vk->ctx_driver->swap_interval)
   {
      int interval             = 0;
      if (!state)
         interval = swap_interval;
      if (adaptive_vsync_enabled && interval == 1)
         interval = -1;
      vk->ctx_driver->swap_interval(vk->ctx_data, interval);
   }

   /* Changing vsync might require recreating the swapchain,
    * which means new VkImages to render into. */
   if (vk->context->flags & VK_CTX_FLAG_INVALID_SWAPCHAIN)
      vulkan_check_swapchain(vk);
}

static bool vulkan_alive(void *data)
{
   bool ret             = false;
   bool quit            = false;
   bool resize          = false;
   vk_t *vk             = (vk_t*)data;
   unsigned temp_width  = vk->video_width;
   unsigned temp_height = vk->video_height;

   vk->ctx_driver->check_window(vk->ctx_data,
            &quit, &resize, &temp_width, &temp_height);

   if (quit)
      vk->flags |= VK_FLAG_QUITTING;
   else if (resize)
      vk->flags |= VK_FLAG_SHOULD_RESIZE;

   ret = (!(vk->flags & VK_FLAG_QUITTING));

   if (temp_width != 0 && temp_height != 0)
   {
      video_driver_set_size(temp_width, temp_height);
      vk->video_width  = temp_width;
      vk->video_height = temp_height;
   }

   return ret;
}

static bool vulkan_suppress_screensaver(void *data, bool enable)
{
   bool enabled = enable;
   vk_t *vk     = (vk_t*)data;

   if (vk->ctx_data && vk->ctx_driver->suppress_screensaver)
      return vk->ctx_driver->suppress_screensaver(vk->ctx_data, enabled);
   return false;
}

static bool vulkan_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   vk_t *vk = (vk_t*)data;
   if (!vk)
      return false;

   if (vk->filter_chain)
      vulkan_filter_chain_free((vulkan_filter_chain_t*)vk->filter_chain);
   vk->filter_chain = NULL;

   if (!string_is_empty(path) && type != RARCH_SHADER_SLANG)
   {
      RARCH_WARN("[Vulkan]: Only Slang shaders are supported. Falling back to stock.\n");
      path = NULL;
   }

   if (string_is_empty(path))
   {
      vulkan_init_default_filter_chain(vk);
      return true;
   }

   if (!vulkan_init_filter_chain_preset(vk, path))
   {
      RARCH_ERR("[Vulkan]: Failed to create filter chain: \"%s\". Falling back to stock.\n", path);
      vulkan_init_default_filter_chain(vk);
      return false;
   }

   return true;
}

static void vulkan_set_projection(vk_t *vk,
      struct video_ortho *ortho, bool allow_rotate)
{
   float radians, cosine, sine;
   static math_matrix_4x4 rot     = {
      {  0.0f,     0.0f,    0.0f,    0.0f ,
         0.0f,     0.0f,    0.0f,    0.0f ,
         0.0f,     0.0f,    0.0f,    0.0f ,
         0.0f,     0.0f,    0.0f,    1.0f }
   };

   /* Calculate projection. */
   matrix_4x4_ortho(vk->mvp_no_rot, ortho->left, ortho->right,
         ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (!allow_rotate)
   {
      vk->mvp = vk->mvp_no_rot;
      return;
   }

   radians                 = M_PI * vk->rotation / 180.0f;
   cosine                  = cosf(radians);
   sine                    = sinf(radians);
   MAT_ELEM_4X4(rot, 0, 0) = cosine;
   MAT_ELEM_4X4(rot, 0, 1) = -sine;
   MAT_ELEM_4X4(rot, 1, 0) = sine;
   MAT_ELEM_4X4(rot, 1, 1) = cosine;
   matrix_4x4_multiply(vk->mvp, rot, vk->mvp_no_rot);
}

static void vulkan_set_rotation(void *data, unsigned rotation)
{
   vk_t *vk               = (vk_t*)data;
   struct video_ortho ortho = {0, 1, 0, 1, -1, 1};

   if (!vk)
      return;

   vk->rotation = 270 * rotation;
   vulkan_set_projection(vk, &ortho, true);
}

static void vulkan_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   vk_t *vk               = (vk_t*)data;
   if (vk->ctx_driver->set_video_mode)
      vk->ctx_driver->set_video_mode(vk->ctx_data,
            width, height, fullscreen);
}

static void vulkan_set_viewport(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate)
{
   int x                     = 0;
   int y                     = 0;
   float device_aspect       = (float)viewport_width / viewport_height;
   struct video_ortho ortho  = {0, 1, 0, 1, -1, 1};
   settings_t *settings      = config_get_ptr();
   bool video_scale_integer  = settings->bools.video_scale_integer;
   unsigned aspect_ratio_idx = settings->uints.video_aspect_ratio_idx;
   vk_t *vk                  = (vk_t*)data;

   if (vk->ctx_driver->translate_aspect)
      device_aspect         = vk->ctx_driver->translate_aspect(
            vk->ctx_data, viewport_width, viewport_height);

   if (video_scale_integer && !force_full)
   {
      video_viewport_get_scaled_integer(&vk->vp,
            viewport_width, viewport_height,
            video_driver_get_aspect_ratio(),
            vk->flags & VK_FLAG_KEEP_ASPECT);
      viewport_width  = vk->vp.width;
      viewport_height = vk->vp.height;
   }
   else if ((vk->flags & VK_FLAG_KEEP_ASPECT) && !force_full)
   {
      float desired_aspect = video_driver_get_aspect_ratio();

#if defined(HAVE_MENU)
      if (aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         const struct video_viewport *custom = video_viewport_get_custom();

         /* Vulkan has top-left origin viewport. */
         x               = custom->x;
         y               = custom->y;
         viewport_width  = custom->width;
         viewport_height = custom->height;
      }
      else
#endif
      {
         float delta;

         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
         {
            /* If the aspect ratios of screen and desired aspect
             * ratio are sufficiently equal (floating point stuff),
             * assume they are actually equal.
             */
         }
         else if (device_aspect > desired_aspect)
         {
            delta          = (desired_aspect / device_aspect - 1.0f)
               / 2.0f + 0.5f;
            x              = (int)roundf(viewport_width * (0.5f - delta));
            viewport_width = (unsigned)roundf(2.0f * viewport_width * delta);
         }
         else
         {
            delta           = (device_aspect / desired_aspect - 1.0f)
               / 2.0f + 0.5f;
            y               = (int)roundf(viewport_height * (0.5f - delta));
            viewport_height = (unsigned)roundf(2.0f * viewport_height * delta);
         }
      }

      vk->vp.x      = x;
      vk->vp.y      = y;
      vk->vp.width  = viewport_width;
      vk->vp.height = viewport_height;
   }
   else
   {
      vk->vp.x      = 0;
      vk->vp.y      = 0;
      vk->vp.width  = viewport_width;
      vk->vp.height = viewport_height;
   }

#if defined(RARCH_MOBILE)
   /* In portrait mode, we want viewport to gravitate to top of screen. */
   if (device_aspect < 1.0f)
      vk->vp.y = 0;
#endif

   vulkan_set_projection(vk, &ortho, allow_rotate);

   /* Set last backbuffer viewport. */
   if (!force_full)
   {
      vk->vp_out_width  = viewport_width;
      vk->vp_out_height = viewport_height;
   }

   vk->vk_vp.x          = (float)vk->vp.x;
   vk->vk_vp.y          = (float)vk->vp.y;
   vk->vk_vp.width      = (float)vk->vp.width;
   vk->vk_vp.height     = (float)vk->vp.height;
   vk->vk_vp.minDepth   = 0.0f;
   vk->vk_vp.maxDepth   = 1.0f;

   vk->tracker.dirty |= VULKAN_DIRTY_DYNAMIC_BIT;
}

static void vulkan_readback(vk_t *vk)
{
   VkBufferImageCopy region;
   struct vk_texture *staging;
   struct video_viewport vp;
   VkMemoryBarrier barrier;

   vp.x                                   = 0;
   vp.y                                   = 0;
   vp.width                               = 0;
   vp.height                              = 0;
   vp.full_width                          = 0;
   vp.full_height                         = 0;

   vulkan_viewport_info(vk, &vp);

   region.bufferOffset                    = 0;
   region.bufferRowLength                 = 0;
   region.bufferImageHeight               = 0;
   region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   region.imageSubresource.mipLevel       = 0;
   region.imageSubresource.baseArrayLayer = 0;
   region.imageSubresource.layerCount     = 1;
   region.imageOffset.x                   = vp.x;
   region.imageOffset.y                   = vp.y;
   region.imageOffset.z                   = 0;
   region.imageExtent.width               = vp.width;
   region.imageExtent.height              = vp.height;
   region.imageExtent.depth               = 1;

   staging  = &vk->readback.staging[vk->context->current_frame_index];
   *staging = vulkan_create_texture(vk,
         staging->memory != VK_NULL_HANDLE ? staging : NULL,
         vk->vp.width, vk->vp.height,
         VK_FORMAT_B8G8R8A8_UNORM, /* Formats don't matter for readback since it's a raw copy. */
         NULL, NULL, VULKAN_TEXTURE_READBACK);

   vkCmdCopyImageToBuffer(vk->cmd, vk->backbuffer->image,
         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
         staging->buffer,
         1, &region);

   /* Make the data visible to host. */
   barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
   barrier.pNext         = NULL;
   barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
   barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
   vkCmdPipelineBarrier(vk->cmd,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_HOST_BIT, 0,
         1, &barrier, 0, NULL, 0, NULL);
}

static void vulkan_inject_black_frame(vk_t *vk, video_frame_info_t *video_info)
{
   VkCommandBufferBeginInfo begin_info = {
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
   VkSubmitInfo submit_info            = {
      VK_STRUCTURE_TYPE_SUBMIT_INFO };

   const VkClearColorValue clear_color = {{ 0.0f, 0.0f, 0.0f, 1.0f }};
   const VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
   unsigned frame_index                = vk->context->current_frame_index;
   unsigned swapchain_index            = vk->context->current_swapchain_index;
   struct vk_per_frame *chain          = &vk->swapchain[frame_index];
   struct vk_image *backbuffer         = &vk->backbuffers[swapchain_index];
   vk->chain                           = chain;
   vk->cmd                             = chain->cmd;
   begin_info.flags                    = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   vkResetCommandBuffer(vk->cmd, 0);
   vkBeginCommandBuffer(vk->cmd, &begin_info);

   VULKAN_IMAGE_LAYOUT_TRANSITION(vk->cmd, backbuffer->image,
         VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         0, VK_ACCESS_TRANSFER_WRITE_BIT,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT);

   vkCmdClearColorImage(vk->cmd, backbuffer->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
         &clear_color, 1, &range);

   VULKAN_IMAGE_LAYOUT_TRANSITION(vk->cmd, backbuffer->image,
         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
         VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
         VK_PIPELINE_STAGE_TRANSFER_BIT,
         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

   vkEndCommandBuffer(vk->cmd);

   submit_info.commandBufferCount      = 1;
   submit_info.pCommandBuffers         = &vk->cmd;
   if (
            (vk->context->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
         && (vk->context->swapchain_semaphores[swapchain_index] !=
            VK_NULL_HANDLE))
   {
      submit_info.signalSemaphoreCount = 1;
      submit_info.pSignalSemaphores    = &vk->context->swapchain_semaphores[swapchain_index];
   }

   if (     (vk->context->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
         && (vk->context->swapchain_acquire_semaphore != VK_NULL_HANDLE))
   {
      static const VkPipelineStageFlags wait_stage        =
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

      vk->context->swapchain_wait_semaphores[frame_index] =
         vk->context->swapchain_acquire_semaphore;
      vk->context->swapchain_acquire_semaphore            = VK_NULL_HANDLE;
      submit_info.waitSemaphoreCount                      = 1;
      submit_info.pWaitSemaphores                         = &vk->context->swapchain_wait_semaphores[frame_index];
      submit_info.pWaitDstStageMask                       = &wait_stage;
   }

#ifdef HAVE_THREADS
   slock_lock(vk->context->queue_lock);
#endif
   vkQueueSubmit(vk->context->queue, 1,
         &submit_info, vk->context->swapchain_fences[frame_index]);
   vk->context->swapchain_fences_signalled[frame_index] = true;
#ifdef HAVE_THREADS
   slock_unlock(vk->context->queue_lock);
#endif
}

static bool vulkan_frame(void *data, const void *frame,
      unsigned frame_width, unsigned frame_height,
      uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   int i;
   VkSubmitInfo submit_info;
   VkClearValue clear_color;
   VkRenderPassBeginInfo rp_info;
   VkCommandBufferBeginInfo begin_info;
   VkSemaphore signal_semaphores[2];
   vk_t *vk                                      = (vk_t*)data;
   bool waits_for_semaphores                     = false;
   unsigned width                                = video_info->width;
   unsigned height                               = video_info->height;
   bool statistics_show                          = video_info->statistics_show;
   const char *stat_text                         = video_info->stat_text;
   unsigned black_frame_insertion                = video_info->black_frame_insertion;
   bool input_driver_nonblock_state              = video_info->input_driver_nonblock_state;
   bool runloop_is_slowmotion                    = video_info->runloop_is_slowmotion;
   bool runloop_is_paused                        = video_info->runloop_is_paused;
   unsigned video_width                          = video_info->width;
   unsigned video_height                         = video_info->height;
   struct font_params *osd_params                = (struct font_params*)
      &video_info->osd_stat_params;
#ifdef HAVE_MENU
   bool menu_is_alive                            = video_info->menu_is_alive;
#endif
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active                           = video_info->widgets_active;
#endif
   unsigned frame_index                          =
      vk->context->current_frame_index;
   unsigned swapchain_index                      =
      vk->context->current_swapchain_index;
   bool overlay_behind_menu                      = video_info->overlay_behind_menu;

#ifdef VULKAN_HDR_SWAPCHAIN
   bool use_main_buffer                          = (vk->context->flags &
         VK_CTX_FLAG_HDR_ENABLE)
      && (!vk->filter_chain || !vulkan_filter_chain_emits_hdr10(vk->filter_chain));
#endif /* VULKAN_HDR_SWAPCHAIN */

   /* Bookkeeping on start of frame. */
   struct vk_per_frame *chain                    = &vk->swapchain[frame_index];
   struct vk_image *backbuffer                   = &vk->backbuffers[swapchain_index];
   struct vk_descriptor_manager *manager         = &chain->descriptor_manager;
   struct vk_buffer_chain *buff_chain_vbo        = &chain->vbo;
   struct vk_buffer_chain *buff_chain_ubo        = &chain->ubo;

   vk->chain                                     = chain;
   vk->backbuffer                                = backbuffer;

   VK_DESCRIPTOR_MANAGER_RESTART(manager);
   VK_BUFFER_CHAIN_DISCARD(buff_chain_vbo);
   VK_BUFFER_CHAIN_DISCARD(buff_chain_ubo);

   /* Start recording the command buffer. */
   vk->cmd                                       = chain->cmd;

   begin_info.sType                              = 
      VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin_info.pNext                              = NULL;
   begin_info.flags                              = 
      VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   begin_info.pInheritanceInfo                   = NULL;

   vkResetCommandBuffer(vk->cmd, 0);

   vkBeginCommandBuffer(vk->cmd, &begin_info);

   vk->tracker.dirty                 = 0;
   vk->tracker.scissor.offset.x      = 0;
   vk->tracker.scissor.offset.y      = 0;
   vk->tracker.scissor.extent.width  = 0;
   vk->tracker.scissor.extent.height = 0;
   vk->flags                        &= ~VK_FLAG_TRACKER_USE_SCISSOR;
   vk->tracker.pipeline              = VK_NULL_HANDLE;
   vk->tracker.view                  = VK_NULL_HANDLE;
   vk->tracker.sampler               = VK_NULL_HANDLE;
   for (i = 0; i < 16; i++)
      vk->tracker.mvp.data[i]        = 0.0f;

   waits_for_semaphores              = 
       (vk->flags & VK_FLAG_HW_ENABLE) && frame &&
       !vk->hw.num_cmd && (vk->flags & VK_FLAG_HW_VALID_SEMAPHORE);

   if (waits_for_semaphores &&
       vk->hw.src_queue_family != VK_QUEUE_FAMILY_IGNORED &&
       vk->hw.src_queue_family != vk->context->graphics_queue_index)
   {
      /* Acquire ownership of image from other queue family. */
      VULKAN_TRANSFER_IMAGE_OWNERSHIP(vk->cmd,
            vk->hw.image->create_info.image,
            vk->hw.image->image_layout,
            /* Create a dependency chain from semaphore wait. */
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            vk->hw.src_queue_family, vk->context->graphics_queue_index);
   }

   /* Upload texture */
   if (frame && (!(vk->flags & VK_FLAG_HW_ENABLE)))
   {
      unsigned y;
      uint8_t *dst        = NULL;
      const uint8_t *src  = (const uint8_t*)frame;
      unsigned bpp        = vk->video.rgb32 ? 4 : 2;

      if (     chain->texture.width  != frame_width
            || chain->texture.height != frame_height)
      {
         chain->texture = vulkan_create_texture(vk, &chain->texture,
               frame_width, frame_height, chain->texture.format, NULL, NULL,
               chain->texture_optimal.memory
               ? VULKAN_TEXTURE_STAGING : VULKAN_TEXTURE_STREAMED);

         {
            struct vk_texture *texture = &chain->texture;
            VK_MAP_PERSISTENT_TEXTURE(vk->context->device, texture);
         }

         if (chain->texture.type == VULKAN_TEXTURE_STAGING)
            chain->texture_optimal = vulkan_create_texture(
                  vk,
                  &chain->texture_optimal,
                  frame_width, frame_height,
                  chain->texture_optimal.format,
                  NULL, NULL, VULKAN_TEXTURE_DYNAMIC);
      }

      if (frame != chain->texture.mapped)
      {
         dst = (uint8_t*)chain->texture.mapped;
         if (     (chain->texture.stride == pitch )
               && pitch == frame_width * bpp)
            memcpy(dst, src, frame_width * frame_height * bpp);
         else
            for (y = 0; y < frame_height; y++,
                  dst += chain->texture.stride, src += pitch)
               memcpy(dst, src, frame_width * bpp);
      }

      VULKAN_SYNC_TEXTURE_TO_GPU_COND_OBJ(vk, chain->texture);

      /* If we have an optimal texture, copy to that now. */
      if (chain->texture_optimal.memory != VK_NULL_HANDLE)
      {
         struct vk_texture *dynamic = &chain->texture_optimal;
         struct vk_texture *staging = &chain->texture;
         VULKAN_COPY_STAGING_TO_DYNAMIC(vk, vk->cmd, dynamic, staging);
      }

      vk->last_valid_index = frame_index;
   }

   /* Notify filter chain about the new sync index. */
   vulkan_filter_chain_notify_sync_index(
         (vulkan_filter_chain_t*)vk->filter_chain, frame_index);
   vulkan_filter_chain_set_frame_count(
         (vulkan_filter_chain_t*)vk->filter_chain, frame_count);
#ifdef HAVE_REWIND
   vulkan_filter_chain_set_frame_direction(
         (vulkan_filter_chain_t*)vk->filter_chain,
         state_manager_frame_is_reversed() ? -1 : 1);
#else
   vulkan_filter_chain_set_frame_direction(
         (vulkan_filter_chain_t*)vk->filter_chain,
         1);
#endif

   /* Render offscreen filter chain passes. */
   {
      /* Set the source texture in the filter chain */
      struct vulkan_filter_chain_texture input;

      if (vk->flags & VK_FLAG_HW_ENABLE)
      {
         /* Does this make that this can happen at all? */
         if (vk->hw.image && vk->hw.image->create_info.image)
         {
            if (frame)
            {
               input.width     = frame_width;
               input.height    = frame_height;
            }
            else
            {
               input.width     = vk->hw.last_width;
               input.height    = vk->hw.last_height;
            }

            input.image        = vk->hw.image->create_info.image;
            input.view         = vk->hw.image->image_view;
            input.layout       = vk->hw.image->image_layout;

            /* The format can change on a whim. */
            input.format       = vk->hw.image->create_info.format;
         }
         else
         {
            /* Fall back to the default, black texture.
             * This can happen if we restart the video 
             * driver while in the menu. */
            input.width        = vk->default_texture.width;
            input.height       = vk->default_texture.height;
            input.image        = vk->default_texture.image;
            input.view         = vk->default_texture.view;
            input.layout       = vk->default_texture.layout;
            input.format       = vk->default_texture.format;
         }

         vk->hw.last_width     = input.width;
         vk->hw.last_height    = input.height;
      }
      else
      {
         struct vk_texture *tex = &vk->swapchain[vk->last_valid_index].texture;
         if (vk->swapchain[vk->last_valid_index].texture_optimal.memory 
               != VK_NULL_HANDLE)
            tex = &vk->swapchain[vk->last_valid_index].texture_optimal;
         else if (tex->image)
            vulkan_transition_texture(vk, vk->cmd, tex);

         input.image  = tex->image;
         input.view   = tex->view;
         input.layout = tex->layout;
         input.width  = tex->width;
         input.height = tex->height;
         input.format = VK_FORMAT_UNDEFINED; /* It's already configured. */
      }

      vulkan_filter_chain_set_input_texture((vulkan_filter_chain_t*)
            vk->filter_chain, &input);
   }

   vulkan_set_viewport(vk, width, height, false, true);

   vulkan_filter_chain_build_offscreen_passes(
         (vulkan_filter_chain_t*)vk->filter_chain,
         vk->cmd, &vk->vk_vp);

#if defined(HAVE_MENU)
   /* Upload menu texture. */
   if (vk->flags & VK_FLAG_MENU_ENABLE)
   {
       if (vk->menu.textures[vk->menu.last_index].image != VK_NULL_HANDLE ||
           vk->menu.textures[vk->menu.last_index].buffer != VK_NULL_HANDLE)
       {
           struct vk_texture *optimal = &vk->menu.textures_optimal[vk->menu.last_index];
           struct vk_texture *texture = &vk->menu.textures[vk->menu.last_index];

           if (optimal->memory != VK_NULL_HANDLE)
           {
               if (vk->menu.dirty[vk->menu.last_index])
               {
                  struct vk_texture *dynamic = optimal;
                  struct vk_texture *staging = texture;
                  VULKAN_SYNC_TEXTURE_TO_GPU_COND_PTR(vk, staging);
                  VULKAN_COPY_STAGING_TO_DYNAMIC(vk, vk->cmd,
                        dynamic, staging);
                  vk->menu.dirty[vk->menu.last_index] = false;
               }
           }
       }
   }
#endif

#ifdef VULKAN_HDR_SWAPCHAIN
   if (use_main_buffer)
      backbuffer = &vk->main_buffer;
#endif /* VULKAN_HDR_SWAPCHAIN */

   /* Render to backbuffer. */
   if (     (backbuffer->image != VK_NULL_HANDLE)
         && (vk->context->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN))
   {
      rp_info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      rp_info.pNext                    = NULL;
      rp_info.renderPass               = vk->render_pass;
      rp_info.framebuffer              = backbuffer->framebuffer;
      rp_info.renderArea.offset.x      = 0;
      rp_info.renderArea.offset.y      = 0;
      rp_info.renderArea.extent.width  = vk->context->swapchain_width;
      rp_info.renderArea.extent.height = vk->context->swapchain_height;
      rp_info.clearValueCount          = 1;
      rp_info.pClearValues             = &clear_color;

      clear_color.color.float32[0]     = 0.0f;
      clear_color.color.float32[1]     = 0.0f;
      clear_color.color.float32[2]     = 0.0f;
      clear_color.color.float32[3]     = 0.0f;

      /* Prepare backbuffer for rendering. */
      VULKAN_IMAGE_LAYOUT_TRANSITION(vk->cmd, backbuffer->image,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

      /* Begin render pass and set up viewport */
      vkCmdBeginRenderPass(vk->cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

      vulkan_filter_chain_build_viewport_pass(
            (vulkan_filter_chain_t*)vk->filter_chain, vk->cmd,
            &vk->vk_vp, vk->mvp.data);

#ifdef HAVE_OVERLAY
      if ((vk->flags & VK_FLAG_OVERLAY_ENABLE) && overlay_behind_menu)
         vulkan_render_overlay(vk, video_width, video_height);
#endif

#if defined(HAVE_MENU)
      if (vk->flags & VK_FLAG_MENU_ENABLE)
      {
         menu_driver_frame(menu_is_alive, video_info);

         if (vk->menu.textures[vk->menu.last_index].image  != VK_NULL_HANDLE ||
             vk->menu.textures[vk->menu.last_index].buffer != VK_NULL_HANDLE)
         {
            struct vk_draw_quad quad;
            struct vk_texture *optimal = &vk->menu.textures_optimal[vk->menu.last_index];
            settings_t *settings       = config_get_ptr();
            bool menu_linear_filter    = settings->bools.menu_linear_filter;

            vulkan_set_viewport(vk, width, height, ((vk->flags &
                     VK_FLAG_MENU_FULLSCREEN) > 0), false);

            quad.pipeline              = vk->pipelines.alpha_blend;
            quad.texture               = &vk->menu.textures[vk->menu.last_index];

            if (optimal->memory != VK_NULL_HANDLE)
               quad.texture = optimal;

            if (menu_linear_filter)
               quad.sampler = (optimal->flags & VK_TEX_FLAG_MIPMAP) ?
                  vk->samplers.mipmap_linear : vk->samplers.linear;
            else
               quad.sampler = (optimal->flags & VK_TEX_FLAG_MIPMAP) ?
                  vk->samplers.mipmap_nearest : vk->samplers.nearest;

            quad.mvp        = &vk->mvp_no_rot;
            quad.color.r    = 1.0f;
            quad.color.g    = 1.0f;
            quad.color.b    = 1.0f;
            quad.color.a    = vk->menu.alpha;
            vulkan_draw_quad(vk, &quad);
         }
      }
      else if (statistics_show)
      {
         if (osd_params)
            font_driver_render_msg(vk,
                  stat_text,
                  osd_params, NULL);
      }
#endif

#ifdef HAVE_OVERLAY
      if ((vk->flags & VK_FLAG_OVERLAY_ENABLE) && !overlay_behind_menu)
         vulkan_render_overlay(vk, video_width, video_height);
#endif

      if (!string_is_empty(msg))
         font_driver_render_msg(vk, msg, NULL, NULL);

#ifdef HAVE_GFX_WIDGETS
      if (widgets_active)
         gfx_widgets_frame(video_info);
#endif

      /* End the render pass. We're done rendering to backbuffer now. */
      vkCmdEndRenderPass(vk->cmd);

#ifdef VULKAN_HDR_SWAPCHAIN
      /* Copy over back buffer to swap chain render targets */
      if (use_main_buffer)
      {
         backbuffer = &vk->backbuffers[swapchain_index];

         vulkan_hdr_uniform_t* mapped_ubo = (vulkan_hdr_uniform_t*)vk->hdr.ubo.mapped;

         mapped_ubo->mvp                  = vk->mvp_no_rot;          

         rp_info.sType                    = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
         rp_info.pNext                    = NULL;
         rp_info.renderPass               = vk->render_pass;
         rp_info.framebuffer              = backbuffer->framebuffer;
         rp_info.renderArea.offset.x      = 0;
         rp_info.renderArea.offset.y      = 0;
         rp_info.renderArea.extent.width  = vk->context->swapchain_width;
         rp_info.renderArea.extent.height = vk->context->swapchain_height;
         rp_info.clearValueCount          = 1;
         rp_info.pClearValues             = &clear_color;

         clear_color.color.float32[0]     = 0.0f;
         clear_color.color.float32[1]     = 0.0f;
         clear_color.color.float32[2]     = 0.0f;
         clear_color.color.float32[3]     = 0.0f;

         /* Prepare backbuffer for rendering. */
         VULKAN_IMAGE_LAYOUT_TRANSITION(vk->cmd, backbuffer->image,
               VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               0, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

         VULKAN_IMAGE_LAYOUT_TRANSITION(vk->cmd, vk->main_buffer.image,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);   

         /* Begin render pass and set up viewport */
         vkCmdBeginRenderPass(vk->cmd, &rp_info, VK_SUBPASS_CONTENTS_INLINE);

         {
            if (vk->pipelines.hdr != vk->tracker.pipeline)
            {
               vkCmdBindPipeline(vk->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vk->pipelines.hdr);
      
               vk->tracker.pipeline = vk->pipelines.hdr;
               /* Changing pipeline invalidates dynamic state. */
               vk->tracker.dirty   |= VULKAN_DIRTY_DYNAMIC_BIT;
            }
         }

         {
            VkWriteDescriptorSet write;
            VkDescriptorImageInfo image_info;
            VkDescriptorSet set = vulkan_descriptor_manager_alloc(
                  vk->context->device,
                  &vk->chain->descriptor_manager);     

            vulkan_set_uniform_buffer(vk->context->device,
                  set,
                  0,
                  vk->hdr.ubo.buffer,
                  0,
                  vk->hdr.ubo.size);                  

            image_info.sampler              = vk->samplers.nearest;
            image_info.imageView            = vk->main_buffer.view;
            image_info.imageLayout          = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            write.sType                     = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.pNext                     = NULL;
            write.dstSet                    = set;
            write.dstBinding                = 2;
            write.dstArrayElement           = 0;
            write.descriptorCount           = 1;
            write.descriptorType            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            write.pImageInfo                = &image_info;
            write.pBufferInfo               = NULL;
            write.pTexelBufferView          = NULL;
            
            vkUpdateDescriptorSets(vk->context->device, 1, &write, 0, NULL);
   
            vkCmdBindDescriptorSets(vk->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
                  vk->pipelines.layout, 0,
                  1, &set, 0, NULL);

            vk->tracker.view    = vk->main_buffer.view;
            vk->tracker.sampler = vk->samplers.nearest;
         }

         {
            VkViewport viewport;
            VkRect2D sci;

            viewport.x             = 0.0f;
            viewport.y             = 0.0f;
            viewport.width         = vk->context->swapchain_width;
            viewport.height        = vk->context->swapchain_height;
            viewport.minDepth      = 0.0f;
            viewport.maxDepth      = 1.0f;

            sci.offset.x           = (int32_t)viewport.x;
            sci.offset.y           = (int32_t)viewport.y;
            sci.extent.width       = (uint32_t)viewport.width;
            sci.extent.height      = (uint32_t)viewport.height;
            vkCmdSetViewport(vk->cmd, 0, 1, &viewport);
            vkCmdSetScissor(vk->cmd,  0, 1, &sci);
         }

         /* Upload VBO */
         {
            struct vk_buffer_range range;

            vulkan_buffer_chain_alloc(vk->context, &vk->chain->vbo, 6 * sizeof(struct vk_vertex), &range);

            {
               struct vk_vertex  *pv = (struct vk_vertex*)range.data;
               struct vk_color   color;

               color.r = 1.0f;
               color.g = 1.0f;
               color.b = 1.0f;
               color.a = 1.0f;               

               VULKAN_WRITE_QUAD_VBO(pv, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, &color);
            }

            vkCmdBindVertexBuffers(vk->cmd, 0, 1,
                  &range.buffer, &range.offset);
         }         

         vkCmdDraw(vk->cmd, 6, 1, 0, 0);

         vkCmdEndRenderPass(vk->cmd);
      }
#endif /* VULKAN_HDR_SWAPCHAIN */
   }

   /* End the filter chain frame.
    * This must happen outside a render pass.
    */
   vulkan_filter_chain_end_frame((vulkan_filter_chain_t*)vk->filter_chain, vk->cmd);

   if ( 
            (backbuffer->image != VK_NULL_HANDLE)
         && (vk->context->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
      )
   {
      if (     (vk->flags & VK_FLAG_READBACK_PENDING) 
            || (vk->flags & VK_FLAG_READBACK_STREAMED))
      {
         /* We cannot safely read back from an image which
          * has already been presented as we need to
          * maintain the PRESENT_SRC_KHR layout.
          *
          * If we're reading back, 
          * perform the readback before presenting.
          */
         VULKAN_IMAGE_LAYOUT_TRANSITION(
               vk->cmd,
               backbuffer->image,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
               VK_ACCESS_TRANSFER_READ_BIT,
               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT);

         vulkan_readback(vk);

         /* Prepare for presentation after transfers are complete. */
         VULKAN_IMAGE_LAYOUT_TRANSITION(
               vk->cmd,
               backbuffer->image,
               VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
               0,
               VK_ACCESS_MEMORY_READ_BIT,
               VK_PIPELINE_STAGE_TRANSFER_BIT,
               VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

         vk->flags &= ~VK_FLAG_READBACK_PENDING;
      }
      else
      {
         /* Prepare backbuffer for presentation. */
         VULKAN_IMAGE_LAYOUT_TRANSITION(
               vk->cmd,
               backbuffer->image,
               VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
               VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
               VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
               0,
               VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
               VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
      }
   }

   if (waits_for_semaphores &&
       vk->hw.src_queue_family != VK_QUEUE_FAMILY_IGNORED &&
       vk->hw.src_queue_family != vk->context->graphics_queue_index)
   {
      /* Release ownership of image back to other queue family. */
      VULKAN_TRANSFER_IMAGE_OWNERSHIP(vk->cmd,
            vk->hw.image->create_info.image,
            vk->hw.image->image_layout,
            VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            vk->context->graphics_queue_index, vk->hw.src_queue_family);
   }

   vkEndCommandBuffer(vk->cmd);

   /* Submit command buffers to GPU. */
   submit_info.sType                 = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit_info.pNext                 = NULL;

   if (vk->hw.num_cmd)
   {
      /* vk->hw.cmd has already been allocated for this. */
      vk->hw.cmd[vk->hw.num_cmd]     = vk->cmd;

      submit_info.commandBufferCount = vk->hw.num_cmd + 1;
      submit_info.pCommandBuffers    = vk->hw.cmd;

      vk->hw.num_cmd                 = 0;
   }
   else
   {
      submit_info.commandBufferCount = 1;
      submit_info.pCommandBuffers    = &vk->cmd;
   }

   if (waits_for_semaphores)
   {
      submit_info.waitSemaphoreCount = vk->hw.num_semaphores;
      submit_info.pWaitSemaphores    = vk->hw.semaphores;
      submit_info.pWaitDstStageMask  = vk->hw.wait_dst_stages;

      /* Consume the semaphores. */
      vk->flags                     &= ~VK_FLAG_HW_VALID_SEMAPHORE;

      /* We allocated space for this. */
      if (    (vk->context->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
           && (vk->context->swapchain_acquire_semaphore != VK_NULL_HANDLE))
      {
         vk->context->swapchain_wait_semaphores[frame_index]    =
            vk->context->swapchain_acquire_semaphore;
         vk->context->swapchain_acquire_semaphore               = VK_NULL_HANDLE;

         vk->hw.semaphores[submit_info.waitSemaphoreCount]      = vk->context->swapchain_wait_semaphores[frame_index];
         vk->hw.wait_dst_stages[submit_info.waitSemaphoreCount] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
         submit_info.waitSemaphoreCount++;
      }
   }
   else if ((vk->context->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
         && (vk->context->swapchain_acquire_semaphore != VK_NULL_HANDLE))
   {
      static const VkPipelineStageFlags wait_stage        =
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

      vk->context->swapchain_wait_semaphores[frame_index] =
         vk->context->swapchain_acquire_semaphore;
      vk->context->swapchain_acquire_semaphore            = VK_NULL_HANDLE;

      submit_info.waitSemaphoreCount = 1;
      submit_info.pWaitSemaphores    = &vk->context->swapchain_wait_semaphores[frame_index];
      submit_info.pWaitDstStageMask  = &wait_stage;
   }
   else
   {
      submit_info.waitSemaphoreCount = 0;
      submit_info.pWaitSemaphores    = NULL;
      submit_info.pWaitDstStageMask  = NULL;
   }

   submit_info.signalSemaphoreCount  = 0;

   if ((vk->context->swapchain_semaphores[swapchain_index] 
         != VK_NULL_HANDLE)
         && (vk->context->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN))
      signal_semaphores[submit_info.signalSemaphoreCount++] = vk->context->swapchain_semaphores[swapchain_index];

   if (vk->hw.signal_semaphore != VK_NULL_HANDLE)
   {
      signal_semaphores[submit_info.signalSemaphoreCount++] = vk->hw.signal_semaphore;
      vk->hw.signal_semaphore = VK_NULL_HANDLE;
   }
   submit_info.pSignalSemaphores = submit_info.signalSemaphoreCount ? signal_semaphores : NULL;

#ifdef HAVE_THREADS
   slock_lock(vk->context->queue_lock);
#endif
   vkQueueSubmit(vk->context->queue, 1,
         &submit_info, vk->context->swapchain_fences[frame_index]);
   vk->context->swapchain_fences_signalled[frame_index] = true;
#ifdef HAVE_THREADS
   slock_unlock(vk->context->queue_lock);
#endif

   if (vk->ctx_driver->swap_buffers)
      vk->ctx_driver->swap_buffers(vk->ctx_data);

   if (!(vk->context->flags & VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK))
   {
      if (vk->ctx_driver->update_window_title)
         vk->ctx_driver->update_window_title(vk->ctx_data);
   }

   /* Handle spurious swapchain invalidations as soon as we can,
    * i.e. right after swap buffers. */
#ifdef VULKAN_HDR_SWAPCHAIN
   bool video_hdr_enable          = video_info->hdr_enable;
   if (       (vk->flags & VK_FLAG_SHOULD_RESIZE)
         || (((vk->context->flags & VK_CTX_FLAG_HDR_ENABLE) > 0) 
         != video_hdr_enable))
#else
   if (vk->flags & VK_FLAG_SHOULD_RESIZE)
#endif /* VULKAN_HDR_SWAPCHAIN */
   {
#ifdef VULKAN_HDR_SWAPCHAIN
      if (video_hdr_enable)
      {
         vk->context->flags |= VK_CTX_FLAG_HDR_ENABLE;
#ifdef HAVE_THREADS
         slock_lock(vk->context->queue_lock);
#endif
         vkQueueWaitIdle(vk->context->queue);
#ifdef HAVE_THREADS
         slock_unlock(vk->context->queue_lock);
#endif
         vulkan_destroy_hdr_buffer(vk->context->device, &vk->main_buffer);
      }
      else
         vk->context->flags &= ~VK_CTX_FLAG_HDR_ENABLE;

#endif /* VULKAN_HDR_SWAPCHAIN */

      gfx_ctx_mode_t mode;
      mode.width  = width;
      mode.height = height;

      if (vk->ctx_driver->set_resize)
         vk->ctx_driver->set_resize(vk->ctx_data, mode.width, mode.height);

#ifdef VULKAN_HDR_SWAPCHAIN
      if (   (vk->context->swapchain_colour_space)
          == VK_COLOR_SPACE_HDR10_ST2084_EXT)
         vk->flags          |=  VK_FLAG_HDR_SUPPORT;
      else
      {
         vk->flags          &= ~VK_FLAG_HDR_SUPPORT;
         vk->context->flags &= ~VK_CTX_FLAG_HDR_ENABLE;
      }

      if (vk->context->flags & VK_CTX_FLAG_HDR_ENABLE)
      {
         VkMemoryRequirements mem_reqs;
         VkImageCreateInfo image_info;
         VkMemoryAllocateInfo alloc;
         VkImageViewCreateInfo view;
         VkFramebufferCreateInfo info;

         memset(&vk->main_buffer, 0, sizeof(vk->main_buffer));

         /* Create the image */
         image_info.sType                = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
         image_info.pNext                = NULL;
         image_info.flags                = 0;
         image_info.imageType            = VK_IMAGE_TYPE_2D;
         image_info.format               = vk->context->swapchain_format;
         image_info.extent.width         = video_width;
         image_info.extent.height        = video_height;
         image_info.extent.depth         = 1;
         image_info.mipLevels            = 1;
         image_info.arrayLayers          = 1;
         image_info.samples              = VK_SAMPLE_COUNT_1_BIT;
         image_info.tiling               = VK_IMAGE_TILING_OPTIMAL;
         image_info.usage                = VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
         image_info.sharingMode          = VK_SHARING_MODE_EXCLUSIVE;
         image_info.queueFamilyIndexCount= 0;
         image_info.pQueueFamilyIndices  = NULL;
         image_info.initialLayout        = VK_IMAGE_LAYOUT_UNDEFINED;

         vkCreateImage(vk->context->device, &image_info, NULL, &vk->main_buffer.image);
         vulkan_debug_mark_image(vk->context->device, vk->main_buffer.image);
         vkGetImageMemoryRequirements(vk->context->device, vk->main_buffer.image, &mem_reqs);
         alloc.sType                     = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
         alloc.pNext                     = NULL;
         alloc.allocationSize            = mem_reqs.size;
         alloc.memoryTypeIndex           = vulkan_find_memory_type(
               &vk->context->memory_properties,
               mem_reqs.memoryTypeBits,
               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

         vkAllocateMemory(vk->context->device, &alloc, NULL, &vk->main_buffer.memory);
         vulkan_debug_mark_memory(vk->context->device, vk->main_buffer.memory);

         vkBindImageMemory(vk->context->device, vk->main_buffer.image, vk->main_buffer.memory, 0);

         /* Create an image view which we can render into. */
         view.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
         view.pNext                           = NULL;
         view.flags                           = 0;
         view.image                           = vk->main_buffer.image;
         view.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
         view.format                          = image_info.format;
         view.components.r                    = VK_COMPONENT_SWIZZLE_R;
         view.components.g                    = VK_COMPONENT_SWIZZLE_G;
         view.components.b                    = VK_COMPONENT_SWIZZLE_B;
         view.components.a                    = VK_COMPONENT_SWIZZLE_A;
         view.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
         view.subresourceRange.baseMipLevel   = 0;
         view.subresourceRange.levelCount     = 1;
         view.subresourceRange.baseArrayLayer = 0;
         view.subresourceRange.layerCount     = 1;

         vkCreateImageView(vk->context->device, &view, NULL, &vk->main_buffer.view);

         /* Create the framebuffer */
         info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
         info.pNext           = NULL;
         info.flags           = 0;
         info.renderPass      = vk->render_pass;
         info.attachmentCount = 1;
         info.pAttachments    = &vk->main_buffer.view;
         info.width           = vk->context->swapchain_width;
         info.height          = vk->context->swapchain_height;
         info.layers          = 1;

         vkCreateFramebuffer(vk->context->device, &info, NULL, &vk->main_buffer.framebuffer);
      }
#endif /* VULKAN_HDR_SWAPCHAIN */
      vk->flags &= ~VK_FLAG_SHOULD_RESIZE;
   }

   if (vk->context->flags & VK_CTX_FLAG_INVALID_SWAPCHAIN)
      vulkan_check_swapchain(vk); 

   /* Disable BFI during fast forward, slow-motion,
    * and pause to prevent flicker. */
   if (
            (backbuffer->image != VK_NULL_HANDLE)
         && (vk->context->flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN)
         && black_frame_insertion
         && !input_driver_nonblock_state
         && !runloop_is_slowmotion
         && !runloop_is_paused
         && (!(vk->flags & VK_FLAG_MENU_ENABLE)))
   {   
      int n;
      for (n = 0; n < (int) black_frame_insertion; ++n)
      {
         vulkan_inject_black_frame(vk, video_info);
         if (vk->ctx_driver->swap_buffers)
            vk->ctx_driver->swap_buffers(vk->ctx_data);
      }
   }

   /* Vulkan doesn't directly support swap_interval > 1, 
    * so we fake it by duping out more frames. */
   if (      (vk->context->swap_interval > 1)
         && (!(vk->context->flags & VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK)))
   {
      int i;
      vk->context->flags |= VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK;
      for (i = 1; i < (int) vk->context->swap_interval; i++)
      {
         if (!vulkan_frame(vk, NULL, 0, 0, frame_count, 0, msg,
                  video_info))
         {
            vk->context->flags &= ~VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK;
            return false;
         }
      }
      vk->context->flags &= ~VK_CTX_FLAG_SWAP_INTERVAL_EMULATION_LOCK;
   }

   return true;
}

static void vulkan_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   vk_t *vk = (vk_t*)data;
   if (vk)
      vk->flags |= VK_FLAG_KEEP_ASPECT | VK_FLAG_SHOULD_RESIZE;
}

static void vulkan_apply_state_changes(void *data)
{
   vk_t *vk = (vk_t*)data;
   if (vk)
      vk->flags |= VK_FLAG_SHOULD_RESIZE;
}

static void vulkan_show_mouse(void *data, bool state)
{
   vk_t                            *vk = (vk_t*)data;

   if (vk && vk->ctx_driver->show_mouse)
      vk->ctx_driver->show_mouse(vk->ctx_data, state);
}

static struct video_shader *vulkan_get_current_shader(void *data)
{
   vk_t *vk = (vk_t*)data;
   if (vk && vk->filter_chain)
      return vulkan_filter_chain_get_preset((vulkan_filter_chain_t*)vk->filter_chain);
   return NULL;
}

static bool vulkan_get_current_sw_framebuffer(void *data,
      struct retro_framebuffer *framebuffer)
{
   struct vk_per_frame *chain = NULL;
   vk_t *vk                   = (vk_t*)data;
   vk->chain                  =
      &vk->swapchain[vk->context->current_frame_index];
   chain                      = vk->chain;

   if (chain->texture.width != framebuffer->width ||
         chain->texture.height != framebuffer->height)
   {
      chain->texture   = vulkan_create_texture(vk, &chain->texture,
            framebuffer->width, framebuffer->height, chain->texture.format,
            NULL, NULL, VULKAN_TEXTURE_STREAMED);
      {
         struct vk_texture *texture = &chain->texture;
         VK_MAP_PERSISTENT_TEXTURE(vk->context->device, texture);
      }

      if (chain->texture.type == VULKAN_TEXTURE_STAGING)
      {
         chain->texture_optimal = vulkan_create_texture(
               vk,
               &chain->texture_optimal,
               framebuffer->width,
               framebuffer->height,
               chain->texture_optimal.format,
               NULL, NULL, VULKAN_TEXTURE_DYNAMIC);
      }
   }

   framebuffer->data         = chain->texture.mapped;
   framebuffer->pitch        = chain->texture.stride;
   framebuffer->format       = vk->video.rgb32
      ? RETRO_PIXEL_FORMAT_XRGB8888 : RETRO_PIXEL_FORMAT_RGB565;
   framebuffer->memory_flags = 0;

   if (vk->context->memory_properties.memoryTypes[
         chain->texture.memory_type].propertyFlags &
         VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
      framebuffer->memory_flags |= RETRO_MEMORY_TYPE_CACHED;

   return true;
}

static bool vulkan_is_mapped_swapchain_texture_ptr(const vk_t* vk,
      const void* ptr)
{
   int i;
   for (i = 0; i < (int) vk->num_swapchain_images; i++)
   {
      if (ptr == vk->swapchain[i].texture.mapped)
         return true;
   }

   return false;
}

static bool vulkan_get_hw_render_interface(void *data,
      const struct retro_hw_render_interface **iface)
{
   vk_t *vk = (vk_t*)data;
   *iface   = (const struct retro_hw_render_interface*)&vk->hw.iface;
   return ((vk->flags & VK_FLAG_HW_ENABLE) > 0);
}

static void vulkan_set_texture_frame(void *data,
      const void *frame, bool rgb32, unsigned width, unsigned height,
      float alpha)
{
   unsigned y, stride;
   uint8_t *ptr                        = NULL;
   uint8_t *dst                        = NULL;
   const uint8_t *src                  = NULL;
   vk_t *vk                            = (vk_t*)data;
   unsigned idx                        = 0;
   struct vk_texture *texture          = NULL;
   struct vk_texture *texture_optimal  = NULL;
   const VkComponentMapping br_swizzle = {
      VK_COMPONENT_SWIZZLE_B,
      VK_COMPONENT_SWIZZLE_G,
      VK_COMPONENT_SWIZZLE_R,
      VK_COMPONENT_SWIZZLE_A,
   };

   if (!vk)
      return;

   idx             = vk->context->current_frame_index;
   texture         = &vk->menu.textures[idx  ];
   texture_optimal = &vk->menu.textures_optimal[idx  ];

   /* B4G4R4A4 must be supported, but R4G4B4A4 is optional,
    * just apply the swizzle in the image view instead. */
   *texture = vulkan_create_texture(vk,
         texture->memory ? texture : NULL,
         width, height,
         rgb32 ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_B4G4R4A4_UNORM_PACK16,
         NULL, rgb32 ? NULL : &br_swizzle,
         texture_optimal->memory ? VULKAN_TEXTURE_STAGING : VULKAN_TEXTURE_STREAMED);

   vkMapMemory(vk->context->device, texture->memory,
         texture->offset, texture->size, 0, (void**)&ptr);

   dst       = ptr;
   src       = (const uint8_t*)frame;
   stride    = (rgb32 ? sizeof(uint32_t) : sizeof(uint16_t)) * width;

   for (y = 0; y < height; y++, dst += texture->stride, src += stride)
      memcpy(dst, src, stride);

   vk->menu.alpha      = alpha;
   vk->menu.last_index = idx;

   if (texture->type == VULKAN_TEXTURE_STAGING)
      *texture_optimal = vulkan_create_texture(vk,
            texture_optimal->memory ? texture_optimal : NULL,
            width, height,
            rgb32 ? VK_FORMAT_B8G8R8A8_UNORM : VK_FORMAT_B4G4R4A4_UNORM_PACK16,
            NULL, rgb32 ? NULL : &br_swizzle,
            VULKAN_TEXTURE_DYNAMIC);
   else
   {
      VULKAN_SYNC_TEXTURE_TO_GPU_COND_PTR(vk, texture);
   }

   vkUnmapMemory(vk->context->device, texture->memory);
   vk->menu.dirty[idx] = true;
}

static void vulkan_set_texture_enable(void *data, bool state, bool fullscreen)
{
   vk_t *vk = (vk_t*)data;
   if (!vk)
      return;

   if (state)
      vk->flags        |=  VK_FLAG_MENU_ENABLE;
   else
      vk->flags        &= ~VK_FLAG_MENU_ENABLE;
   if (fullscreen)
      vk->flags        |=  VK_FLAG_MENU_FULLSCREEN;
   else
      vk->flags        &= ~VK_FLAG_MENU_FULLSCREEN;
}

#define VK_T0 0xff000000u
#define VK_T1 0xffffffffu

static uintptr_t vulkan_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   struct vk_texture *texture  = NULL;
   vk_t *vk                    = (vk_t*)video_data;
   struct texture_image *image = (struct texture_image*)data;
   if (!image)
      return 0;

   if (!(texture = (struct vk_texture*)calloc(1, sizeof(*texture))))
      return 0;

   if (!image->pixels || !image->width || !image->height)
   {
      /* Create a dummy texture instead. */
      static const uint32_t checkerboard[] = {
         VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1,
         VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0,
         VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1,
         VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0,
         VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1,
         VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0,
         VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1,
         VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0, VK_T1, VK_T0,
      };
      *texture                = vulkan_create_texture(vk, NULL,
            8, 8, VK_FORMAT_B8G8R8A8_UNORM,
            checkerboard, NULL, VULKAN_TEXTURE_STATIC);
      texture->flags         &= ~(VK_TEX_FLAG_DEFAULT_SMOOTH
                                | VK_TEX_FLAG_MIPMAP);
   }
   else
   {
      *texture = vulkan_create_texture(vk, NULL,
            image->width, image->height, VK_FORMAT_B8G8R8A8_UNORM,
            image->pixels, NULL, VULKAN_TEXTURE_STATIC);
      if (filter_type == TEXTURE_FILTER_MIPMAP_LINEAR || filter_type ==
            TEXTURE_FILTER_LINEAR)
         texture->flags |= VK_TEX_FLAG_DEFAULT_SMOOTH;
      if (filter_type == TEXTURE_FILTER_MIPMAP_LINEAR)
         texture->flags |= VK_TEX_FLAG_MIPMAP;
   }

   return (uintptr_t)texture;
}

static void vulkan_unload_texture(void *data, 
      bool threaded, uintptr_t handle)
{
   vk_t *vk                         = (vk_t*)data;
   struct vk_texture *texture       = (struct vk_texture*)handle;
   if (!texture || !vk)
      return;

   /* TODO: We really want to defer this deletion instead,
    * but this will do for now. */
#ifdef HAVE_THREADS
   slock_lock(vk->context->queue_lock);
#endif
   vkQueueWaitIdle(vk->context->queue);
#ifdef HAVE_THREADS
   slock_unlock(vk->context->queue_lock);
#endif
   vulkan_destroy_texture(
         vk->context->device, texture);
   free(texture);
}

static float vulkan_get_refresh_rate(void *data)
{
   float refresh_rate;

   if (video_context_driver_get_refresh_rate(&refresh_rate))
       return refresh_rate;

   return 0.0f;
}

static uint32_t vulkan_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES);
   BIT32_SET(flags, GFX_CTX_FLAGS_BLACK_FRAME_INSERTION);
   BIT32_SET(flags, GFX_CTX_FLAGS_MENU_FRAME_FILTERING);
   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);
   BIT32_SET(flags, GFX_CTX_FLAGS_OVERLAY_BEHIND_MENU_SUPPORTED);

   return flags;
}

static void vulkan_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *desc, size_t desc_len)
{
   vk_t *vk = (vk_t*)data;
   if (vk && vk->ctx_driver && vk->ctx_driver->get_video_output_size)
      vk->ctx_driver->get_video_output_size(
            vk->ctx_data,
            width, height, desc, desc_len);
}

static void vulkan_get_video_output_prev(void *data)
{
   vk_t *vk = (vk_t*)data;
   if (vk && vk->ctx_driver && vk->ctx_driver->get_video_output_prev)
      vk->ctx_driver->get_video_output_prev(vk->ctx_data);
}

static void vulkan_get_video_output_next(void *data)
{
   vk_t *vk = (vk_t*)data;
   if (vk && vk->ctx_driver && vk->ctx_driver->get_video_output_next)
      vk->ctx_driver->get_video_output_next(vk->ctx_data);
}

static const video_poke_interface_t vulkan_poke_interface = {
   vulkan_get_flags,
   vulkan_load_texture,
   vulkan_unload_texture,
   vulkan_set_video_mode,
   vulkan_get_refresh_rate, /* get_refresh_rate */
   NULL,
   vulkan_get_video_output_size,
   vulkan_get_video_output_prev,
   vulkan_get_video_output_next,
   NULL,
   NULL,
   vulkan_set_aspect_ratio,
   vulkan_apply_state_changes,
   vulkan_set_texture_frame,
   vulkan_set_texture_enable,
   font_driver_render_msg,
   vulkan_show_mouse,
   NULL,                               /* grab_mouse_toggle */
   vulkan_get_current_shader,
   vulkan_get_current_sw_framebuffer,
   vulkan_get_hw_render_interface,
#ifdef VULKAN_HDR_SWAPCHAIN   
   vulkan_set_hdr_max_nits,
   vulkan_set_hdr_paper_white_nits,
   vulkan_set_hdr_contrast,
   vulkan_set_hdr_expand_gamut
#else
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
#endif /* VULKAN_HDR_SWAPCHAIN */
};

static void vulkan_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   (void)data;
   *iface = &vulkan_poke_interface;
}

static void vulkan_viewport_info(void *data, struct video_viewport *vp)
{
   unsigned width, height;
   vk_t *vk = (vk_t*)data;

   if (!vk)
      return;

   width           = vk->video_width;
   height          = vk->video_height;
   /* Make sure we get the correct viewport. */
   vulkan_set_viewport(vk, width, height, false, true);

   *vp             = vk->vp;
   vp->full_width  = width;
   vp->full_height = height;
}

static bool vulkan_read_viewport(void *data, uint8_t *buffer, bool is_idle)
{
   struct vk_texture *staging       = NULL;
   vk_t *vk                         = (vk_t*)data;

   if (!vk)
      return false;

   staging = &vk->readback.staging[vk->context->current_frame_index];

   if (vk->flags & VK_FLAG_READBACK_STREAMED)
   {
      const uint8_t *src     = NULL;
      struct scaler_ctx *ctx = NULL;

      switch (vk->context->swapchain_format)
      {
         case VK_FORMAT_R8G8B8A8_UNORM:
         case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
            ctx = &vk->readback.scaler_rgb;
            break;

         case VK_FORMAT_B8G8R8A8_UNORM:
            ctx = &vk->readback.scaler_bgr;
            break;

         default:
            RARCH_ERR("[Vulkan]: Unexpected swapchain format. Cannot readback.\n");
            break;
      }

      if (ctx)
      {
         if (staging->memory == VK_NULL_HANDLE)
            return false;

         buffer += 3 * (vk->vp.height - 1) * vk->vp.width;
         vkMapMemory(vk->context->device, staging->memory,
               staging->offset, staging->size, 0, (void**)&src);

         if (     (staging->flags & VK_TEX_FLAG_NEED_MANUAL_CACHE_MANAGEMENT)
               && (staging->memory != VK_NULL_HANDLE))
            VULKAN_SYNC_TEXTURE_TO_CPU(vk->context->device, staging->memory);

         ctx->in_stride  =  (int)staging->stride;
         ctx->out_stride = -(int)vk->vp.width * 3;
         scaler_ctx_scale_direct(ctx, buffer, src);

         vkUnmapMemory(vk->context->device, staging->memory);
      }
   }
   else
   {
      /* Synchronous path only for now. */
      /* TODO: How will we deal with format conversion?
       * For now, take the simplest route and use image blitting
       * with conversion. */
      vk->flags |= VK_FLAG_READBACK_PENDING;

      if (!is_idle)
         video_driver_cached_frame();

#ifdef HAVE_THREADS
      slock_lock(vk->context->queue_lock);
#endif
      vkQueueWaitIdle(vk->context->queue);
#ifdef HAVE_THREADS
      slock_unlock(vk->context->queue_lock);
#endif

      if (!staging->memory)
      {
         RARCH_ERR("[Vulkan]: Attempted to readback synchronously, but no image is present.\nThis can happen if vsync is disabled on Windows systems due to mailbox emulation.\n");
         return false;
      }

      if (!staging->mapped)
      {
         VK_MAP_PERSISTENT_TEXTURE(vk->context->device, staging);
      }

      if (     (staging->flags & VK_TEX_FLAG_NEED_MANUAL_CACHE_MANAGEMENT)
            && (staging->memory != VK_NULL_HANDLE))
         VULKAN_SYNC_TEXTURE_TO_CPU(vk->context->device, staging->memory);

      {
         int y;
         const uint8_t *src = (const uint8_t*)staging->mapped;
         buffer            += 3 * (vk->vp.height - 1) * vk->vp.width;

         switch (vk->context->swapchain_format)
         {
            case VK_FORMAT_B8G8R8A8_UNORM:
               for (y = 0; y < (int) vk->vp.height; y++,
                     src += staging->stride, buffer -= 3 * vk->vp.width)
               {
                  int x;
                  for (x = 0; x < (int) vk->vp.width; x++)
                  {
                     buffer[3 * x + 0] = src[4 * x + 0];
                     buffer[3 * x + 1] = src[4 * x + 1];
                     buffer[3 * x + 2] = src[4 * x + 2];
                  }
               }
               break;

            case VK_FORMAT_R8G8B8A8_UNORM:
            case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
               for (y = 0; y < (int) vk->vp.height; y++,
                     src += staging->stride, buffer -= 3 * vk->vp.width)
               {
                  int x;
                  for (x = 0; x < (int) vk->vp.width; x++)
                  {
                     buffer[3 * x + 2] = src[4 * x + 0];
                     buffer[3 * x + 1] = src[4 * x + 1];
                     buffer[3 * x + 0] = src[4 * x + 2];
                  }
               }
               break;

            default:
               RARCH_ERR("[Vulkan]: Unexpected swapchain format.\n");
               break;
         }
      }
      vulkan_destroy_texture(
            vk->context->device, staging);
   }
   return true;
}

#ifdef HAVE_OVERLAY
static void vulkan_overlay_enable(void *data, bool enable)
{
   vk_t *vk = (vk_t*)data;
   if (!vk)
      return;

   if (enable)
      vk->flags |=  VK_FLAG_OVERLAY_ENABLE;
   else
      vk->flags &= ~VK_FLAG_OVERLAY_ENABLE;
   if (vk->ctx_driver->show_mouse)
      vk->ctx_driver->show_mouse(vk->ctx_data, enable);
}

static void vulkan_overlay_full_screen(void *data, bool enable)
{
   vk_t *vk = (vk_t*)data;
   if (!vk)
      return;

   if (enable)
      vk->flags |=  VK_FLAG_OVERLAY_FULLSCREEN;
   else
      vk->flags &= ~VK_FLAG_OVERLAY_FULLSCREEN;
}

static void vulkan_overlay_free(vk_t *vk)
{
   int i;
   if (!vk)
      return;

   free(vk->overlay.vertex);
   for (i = 0; i < (int) vk->overlay.count; i++)
      if (vk->overlay.images[i].memory != VK_NULL_HANDLE)
         vulkan_destroy_texture(
               vk->context->device,
               &vk->overlay.images[i]);

   if (vk->overlay.images)
      free(vk->overlay.images);

   memset(&vk->overlay, 0, sizeof(vk->overlay));
}

static void vulkan_overlay_set_alpha(void *data,
      unsigned image, float mod)
{
   int i;
   struct vk_vertex *pv;
   vk_t *vk = (vk_t*)data;

   if (!vk)
      return;

   pv = &vk->overlay.vertex[image * 4];
   for (i = 0; i < 4; i++)
   {
      pv[i].color.r = 1.0f;
      pv[i].color.g = 1.0f;
      pv[i].color.b = 1.0f;
      pv[i].color.a = mod;
   }
}

static void vulkan_render_overlay(vk_t *vk, unsigned width,
      unsigned height)
{
   int i;
   struct video_viewport vp;

   if (!vk)
      return;

   vp                       = vk->vp;
   vulkan_set_viewport(vk, width, height,
         ((vk->flags & VK_FLAG_OVERLAY_FULLSCREEN) > 0),
         false);

   for (i = 0; i < (int) vk->overlay.count; i++)
   {
      struct vk_draw_triangles call;
      struct vk_buffer_range range;

      if (!vulkan_buffer_chain_alloc(vk->context, &vk->chain->vbo,
               4 * sizeof(struct vk_vertex), &range))
         break;

      memcpy(range.data, &vk->overlay.vertex[i * 4],
            4 * sizeof(struct vk_vertex));

      call.vertices     = 4;
      call.uniform_size = sizeof(vk->mvp);
      call.uniform      = &vk->mvp;
      call.vbo          = &range;
      call.texture      = &vk->overlay.images[i];
      call.pipeline     = vk->display.pipelines[3]; /* Strip with blend */
      call.sampler      = (call.texture->flags & VK_TEX_FLAG_MIPMAP)
         ? vk->samplers.mipmap_linear : vk->samplers.linear;
      vulkan_draw_triangles(vk, &call);
   }

   /* Restore the viewport so we don't mess with recording. */
   vk->vp = vp;
}

static void vulkan_overlay_vertex_geom(void *data, unsigned image,
      float x, float y,
      float w, float h)
{
   struct vk_vertex *pv = NULL;
   vk_t             *vk = (vk_t*)data;
   if (!vk)
      return;

   pv      = &vk->overlay.vertex[4 * image];

   pv[0].x = x;
   pv[0].y = y;
   pv[1].x = x;
   pv[1].y = y + h;
   pv[2].x = x + w;
   pv[2].y = y;
   pv[3].x = x + w;
   pv[3].y = y + h;
}

static void vulkan_overlay_tex_geom(void *data, unsigned image,
      float x, float y,
      float w, float h)
{
   struct vk_vertex *pv = NULL;
   vk_t *vk             = (vk_t*)data;
   if (!vk)
      return;

   pv          = &vk->overlay.vertex[4 * image];

   pv[0].tex_x = x;
   pv[0].tex_y = y;
   pv[1].tex_x = x;
   pv[1].tex_y = y + h;
   pv[2].tex_x = x + w;
   pv[2].tex_y = y;
   pv[3].tex_x = x + w;
   pv[3].tex_y = y + h;
}

static bool vulkan_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   int i;
   bool old_enabled;
   const struct texture_image *images =
      (const struct texture_image*)image_data;
   vk_t *vk                           = (vk_t*)data;
   static const struct vk_color white = {
      1.0f, 1.0f, 1.0f, 1.0f,
   };

   if (!vk)
      return false;

#ifdef HAVE_THREADS
   slock_lock(vk->context->queue_lock);
#endif
   vkQueueWaitIdle(vk->context->queue);
#ifdef HAVE_THREADS
   slock_unlock(vk->context->queue_lock);
#endif
   old_enabled        = vk->flags & VK_FLAG_OVERLAY_ENABLE;
   vulkan_overlay_free(vk);

   if (!(vk->overlay.images = (struct vk_texture*)
            calloc(num_images, sizeof(*vk->overlay.images))))
      goto error;
   vk->overlay.count  = num_images;

   if (!(vk->overlay.vertex = (struct vk_vertex*)
      calloc(4 * num_images, sizeof(*vk->overlay.vertex))))
      goto error;

   for (i = 0; i < (int) num_images; i++)
   {
      int j;
      vk->overlay.images[i] = vulkan_create_texture(vk, NULL,
            images[i].width, images[i].height,
            VK_FORMAT_B8G8R8A8_UNORM, images[i].pixels,
            NULL, VULKAN_TEXTURE_STATIC);

      vulkan_overlay_tex_geom(vk, i, 0, 0, 1, 1);
      vulkan_overlay_vertex_geom(vk, i, 0, 0, 1, 1);
      for (j = 0; j < 4; j++)
         vk->overlay.vertex[4 * i + j].color = white;
   }

   if (old_enabled)
      vk->flags |=  VK_FLAG_OVERLAY_ENABLE;
   else
      vk->flags &= ~VK_FLAG_OVERLAY_ENABLE;

   return true;

error:
   vulkan_overlay_free(vk);
   return false;
}

static const video_overlay_interface_t vulkan_overlay_interface = {
   vulkan_overlay_enable,
   vulkan_overlay_load,
   vulkan_overlay_tex_geom,
   vulkan_overlay_vertex_geom,
   vulkan_overlay_full_screen,
   vulkan_overlay_set_alpha,
};

static void vulkan_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface) { *iface = &vulkan_overlay_interface; }
#endif

#ifdef HAVE_GFX_WIDGETS
static bool vulkan_gfx_widgets_enabled(void *data) { return true; }
#endif

static bool vulkan_has_windowed(void *data)
{
   vk_t *vk        = (vk_t*)data;
   if (vk && vk->ctx_driver)
      return vk->ctx_driver->has_windowed;
   return false;
}

static bool vulkan_focus(void *data)
{
   vk_t *vk        = (vk_t*)data;
   if (vk && vk->ctx_driver && vk->ctx_driver->has_focus)
      return vk->ctx_driver->has_focus(vk->ctx_data);
   return true;
}

video_driver_t video_vulkan = {
   vulkan_init,
   vulkan_frame,
   vulkan_set_nonblock_state,
   vulkan_alive,
   vulkan_focus,
   vulkan_suppress_screensaver,
   vulkan_has_windowed,
   vulkan_set_shader,
   vulkan_free,
   "vulkan",
   vulkan_set_viewport,
   vulkan_set_rotation,
   vulkan_viewport_info,
   vulkan_read_viewport,
   NULL,                         /* vulkan_read_frame_raw */

#ifdef HAVE_OVERLAY
   vulkan_get_overlay_interface,
#endif
#ifdef HAVE_VIDEO_LAYOUT
   NULL,
#endif
   vulkan_get_poke_interface,
   NULL,                         /* vulkan_wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   vulkan_gfx_widgets_enabled
#endif
};
