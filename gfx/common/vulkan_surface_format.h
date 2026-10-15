/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2026 - The RetroArch team
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

#ifndef VULKAN_SURFACE_FORMAT_H__
#define VULKAN_SURFACE_FORMAT_H__

/* Which 8-bit SDR surface format the swapchain is created with.
 *
 * B8G8R8A8_UNORM (DRM XRGB8888/ARGB8888) is the one format every display
 * plane scans out, so a compositor can put a fullscreen window straight
 * on the primary plane only when the swapchain uses it; drivers that list
 * an RGBA-ordered format first (DRM XBGR8888) otherwise leave the window
 * composited.  It is also the layout of the driver's own offscreen and
 * readback buffers.  It is taken in the sRGB colour space whenever the
 * surface lists it, and the first 8-bit UNORM format otherwise. */

#include <stdint.h>

#include <retro_inline.h>

#include "../include/vulkan/vulkan_core.h"

/* Index into formats[] of the format to use, or -1 when the surface lists
 * no 8-bit UNORM format. */
static INLINE int vulkan_sdr8_surface_format(
      const VkSurfaceFormatKHR *formats, uint32_t count)
{
   uint32_t i;
   int first = -1;

   for (i = 0; i < count; i++)
   {
      VkFormat f = formats[i].format;
      if (     f == VK_FORMAT_B8G8R8A8_UNORM
            && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
         return (int)i;
      if (     first < 0
            && (   f == VK_FORMAT_B8G8R8A8_UNORM
                || f == VK_FORMAT_R8G8B8A8_UNORM
                || f == VK_FORMAT_A8B8G8R8_UNORM_PACK32))
         first = (int)i;
   }

   return first;
}

#endif
