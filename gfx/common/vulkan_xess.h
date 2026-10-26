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

#ifndef _VULKAN_XESS_H
#define _VULKAN_XESS_H

#include "vulkan_upscaler.h"

RETRO_BEGIN_DECLS

/* XeSS Super Resolution on a Vulkan device (libretro-common
 * gfx/intel/xess_loader.h): one context per device, initialised per
 * output size, quality and flags, an output image the filter chain
 * reads in place of the core's. The library (libxess) comes from the
 * XeSS directory. Nothing here blocks: the pipelines build in the
 * library's background thread, the frames shown as they are until
 * they are ready. */
typedef struct vulkan_xess vulkan_xess_t;

/* What the library asks of the instance and device, those @gpu has */
unsigned vulkan_xess_instance_extensions(const char *dir,
      const char **list, unsigned count, unsigned cap);
unsigned vulkan_xess_device_extensions(const char *dir, VkPhysicalDevice gpu,
      PFN_vkGetInstanceProcAddr gipa, VkInstance instance,
      const char **list, unsigned count, unsigned cap);
/* The library's feature chain ahead of @pnext, structures the chain
 * already names left to it */
const void *vulkan_xess_device_features(const char *dir, VkPhysicalDevice gpu,
      PFN_vkGetInstanceProcAddr gipa, VkInstance instance, const void *pnext);

vulkan_xess_t *vulkan_xess_init(VkInstance instance, VkPhysicalDevice gpu,
      VkDevice device,
      const VkPhysicalDeviceMemoryProperties *memory_properties,
      const char *dir);

/* @mode is an XESS_QUALITY_ */
bool vulkan_xess_optimal(vulkan_xess_t *xess, unsigned mode,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims);

bool vulkan_xess_upscale(vulkan_xess_t *xess, VkCommandBuffer cmd,
      const vulkan_upscaler_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned mode, unsigned out_dims, vulkan_upscaler_image_t *out);

void vulkan_xess_frame_done(vulkan_xess_t *xess);
void vulkan_xess_free(vulkan_xess_t *xess);

RETRO_END_DECLS

#endif
