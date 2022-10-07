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

#ifndef SHADER_VULKAN_H
#define SHADER_VULKAN_H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "glslang_util.h"

#include "../include/vulkan/vulkan.h"

RETRO_BEGIN_DECLS

typedef struct vulkan_filter_chain vulkan_filter_chain_t;

struct vulkan_filter_chain_texture
{
   VkImage image;
   VkImageView view;
   VkImageLayout layout;
   unsigned width;
   unsigned height;
   VkFormat format;
};

struct vulkan_filter_chain_pass_info
{
   /* Maximum number of mip-levels to use. */
   unsigned max_levels;

   float scale_x;
   float scale_y;

   /* Ignored for the last pass, swapchain info 
    * will be used instead. */
   VkFormat rt_format;
   /* For the last pass, make sure VIEWPORT scale
    * with scale factors of 1 are used. */
   enum glslang_filter_chain_scale scale_type_x;
   enum glslang_filter_chain_scale scale_type_y;
   /* The filter to use for source in this pass. */
   enum glslang_filter_chain_filter source_filter;
   enum glslang_filter_chain_filter mip_filter;
   enum glslang_filter_chain_address address;
};

struct vulkan_filter_chain_swapchain_info
{
   VkViewport viewport;
   VkFormat format;
   VkRenderPass render_pass;
   unsigned num_indices;
};

struct vulkan_filter_chain_create_info
{
   VkDevice device;
   VkPhysicalDevice gpu;
   const VkPhysicalDeviceMemoryProperties *memory_properties;
   VkPipelineCache pipeline_cache;
   VkQueue queue;
   VkCommandPool command_pool;
   unsigned num_passes;

   VkFormat original_format;
   struct
   {
      unsigned width, height;
   } max_input_size;
   struct vulkan_filter_chain_swapchain_info swapchain;
};

vulkan_filter_chain_t *vulkan_filter_chain_new(
      const struct vulkan_filter_chain_create_info *info);
void vulkan_filter_chain_free(vulkan_filter_chain_t *chain);

void vulkan_filter_chain_set_shader(vulkan_filter_chain_t *chain,
      unsigned pass,
      VkShaderStageFlags stage,
      const uint32_t *spirv,
      size_t spirv_words);

VkFormat vulkan_filter_chain_get_pass_rt_format(
      vulkan_filter_chain_t *chain,
      unsigned pass);      

bool vulkan_filter_chain_update_swapchain_info(vulkan_filter_chain_t *chain,
      const struct vulkan_filter_chain_swapchain_info *info);

void vulkan_filter_chain_notify_sync_index(vulkan_filter_chain_t *chain,
      unsigned index);

bool vulkan_filter_chain_init(vulkan_filter_chain_t *chain);

void vulkan_filter_chain_set_input_texture(vulkan_filter_chain_t *chain,
      const struct vulkan_filter_chain_texture *texture);

void vulkan_filter_chain_set_frame_count(vulkan_filter_chain_t *chain,
      uint64_t count);

void vulkan_filter_chain_set_frame_count_period(vulkan_filter_chain_t *chain,
      unsigned pass,
      unsigned period);

void vulkan_filter_chain_set_frame_direction(vulkan_filter_chain_t *chain,
      int32_t direction);

void vulkan_filter_chain_build_offscreen_passes(vulkan_filter_chain_t *chain,
      VkCommandBuffer cmd, const VkViewport *vp);
void vulkan_filter_chain_build_viewport_pass(vulkan_filter_chain_t *chain,
      VkCommandBuffer cmd, const VkViewport *vp, const float *mvp);
void vulkan_filter_chain_end_frame(vulkan_filter_chain_t *chain,
      VkCommandBuffer cmd);

vulkan_filter_chain_t *vulkan_filter_chain_create_default(
      const struct vulkan_filter_chain_create_info *info,
      enum glslang_filter_chain_filter filter);

vulkan_filter_chain_t *vulkan_filter_chain_create_from_preset(
      const struct vulkan_filter_chain_create_info *info,
      const char *path, enum glslang_filter_chain_filter filter);

struct video_shader *vulkan_filter_chain_get_preset(
      vulkan_filter_chain_t *chain);

bool vulkan_filter_chain_emits_hdr10(vulkan_filter_chain_t *chain);

RETRO_END_DECLS

#endif
