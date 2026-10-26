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

#ifndef _VULKAN_FFX_H
#define _VULKAN_FFX_H

#include "vulkan_upscaler.h"

RETRO_BEGIN_DECLS

/* FSR on a Vulkan device through the FidelityFX API (libretro-common
 * gfx/amd/ffx_loader.h): one context per output size and flags, an
 * output image the filter chain reads in place of the core's. The
 * library (amd_fidelityfx_vk.dll, FSR 3.1) comes from the FSR
 * directory. Nothing here blocks. */
typedef struct vulkan_ffx vulkan_ffx_t;

/* The device extensions the library wants that @gpu has (none are
 * required: without them it runs its 32-bit shaders) */
unsigned vulkan_ffx_device_extensions(const char *dir, VkPhysicalDevice gpu,
      PFN_vkGetInstanceProcAddr gipa, VkInstance instance,
      const char **list, unsigned count, unsigned cap);
/* The 16-bit shader features chained ahead of @pnext where @gpu has
 * them and the chain does not already name them */
const void *vulkan_ffx_device_features(const char *dir, VkPhysicalDevice gpu,
      PFN_vkGetInstanceProcAddr gipa, VkInstance instance, const void *pnext);

vulkan_ffx_t *vulkan_ffx_init(VkPhysicalDevice gpu, VkDevice device,
      PFN_vkGetDeviceProcAddr gdpa,
      const VkPhysicalDeviceMemoryProperties *memory_properties,
      const char *dir);

/* @mode is an FFX_UPSCALE_MODE_ */
bool vulkan_ffx_optimal(vulkan_ffx_t *ffx, unsigned mode,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims);

bool vulkan_ffx_upscale(vulkan_ffx_t *ffx, VkCommandBuffer cmd,
      const vulkan_upscaler_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned mode, unsigned out_dims, vulkan_upscaler_image_t *out);

void vulkan_ffx_frame_done(vulkan_ffx_t *ffx);
void vulkan_ffx_free(vulkan_ffx_t *ffx);

RETRO_END_DECLS

#endif
