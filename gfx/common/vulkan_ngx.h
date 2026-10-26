/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - Libretro team
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

#ifndef _VULKAN_NGX_H
#define _VULKAN_NGX_H

#include <boolean.h>
#include <retro_common_api.h>
#include <libretro.h>
#include <vulkan/vulkan.h>

#include "../video_defines.h"

RETRO_BEGIN_DECLS

/* DLSS Super Resolution on a Vulkan device through NGX (libretro-common
 * gfx/nvidia/ngx_loader.h): one session per device, a feature per
 * output size and mode, an output image the filter chain reads in place
 * of the core's. Nothing here blocks: images and features that a frame
 * in flight may still use are retired over the next frames. */
typedef struct vulkan_ngx vulkan_ngx_t;

/* The frame the upscaler reads: the core's color image as set_image
 * handed it over, and what it produces: the output image, in
 * VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL once vulkan_ngx_upscale has
 * recorded the frame. */
typedef struct vulkan_ngx_image
{
   VkImage image;
   VkImageView view;
   VkImageLayout layout;
   VkFormat format;
   unsigned dims;             /* VIDEO_SCALE_PACK */
} vulkan_ngx_image_t;

/* The extensions NGX needs on the device and instance, appended to
 * @list (of @cap) past @count, those the GPU has and the list lacks;
 * the new count. Nothing appended where NGX is not loaded. */
unsigned vulkan_ngx_device_extensions(VkPhysicalDevice gpu,
      PFN_vkGetInstanceProcAddr gipa, VkInstance instance,
      const char **list, unsigned count, unsigned cap);
unsigned vulkan_ngx_instance_extensions(const char **list,
      unsigned count, unsigned cap);
#define VULKAN_NGX_MAX_EXTS 8

/* The session on @device: NGX initialized, DLSS Super Resolution found
 * available for it, the feature library looked for in @dlss_dir. NULL,
 * with the reason logged, when any of that is not so. */
vulkan_ngx_t *vulkan_ngx_init(VkInstance instance, VkPhysicalDevice gpu,
      VkDevice device, PFN_vkGetInstanceProcAddr gipa,
      PFN_vkGetDeviceProcAddr gdpa,
      const VkPhysicalDeviceMemoryProperties *memory_properties,
      const char *dlss_dir);

/* The mode's render size for an output of @out_dims, and the range
 * DLSS takes over all its modes (a frame outside the mode's own range
 * runs in the mode whose range it is in), each VIDEO_SCALE_PACK.
 * @quality is an enum ngx_perf_quality. */
bool vulkan_ngx_optimal(vulkan_ngx_t *ngx, unsigned quality,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims);

/* Records into @cmd the upscale of @color with @inputs to @out_dims,
 * then DLSS Neural Rendering over it where @nr (video_upscaler_nr_mode)
 * turns it on and it runs, and names the output in *@out. False,
 * nothing recorded, when the frame cannot be taken (a size no mode
 * takes, a feature that would not create): the caller shows the
 * color image as it is. Call once per frame, before anything that reads
 * *@out. */
bool vulkan_ngx_upscale(vulkan_ngx_t *ngx, VkCommandBuffer cmd,
      const vulkan_ngx_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned quality, unsigned nr, unsigned out_dims,
      vulkan_ngx_image_t *out);

/* The frame was submitted: retired images and features count down. */
void vulkan_ngx_frame_done(vulkan_ngx_t *ngx);

/* The device is idle (context teardown): everything released. */
void vulkan_ngx_free(vulkan_ngx_t *ngx);

RETRO_END_DECLS

#endif
