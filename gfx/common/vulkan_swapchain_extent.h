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

#ifndef VULKAN_SWAPCHAIN_EXTENT_H__
#define VULKAN_SWAPCHAIN_EXTENT_H__

#include <stdint.h>
#include <retro_inline.h>
#include "../include/vulkan/vulkan_core.h"

static INLINE VkExtent2D vulkan_swapchain_extent(
      const VkSurfaceCapabilitiesKHR *caps,
      unsigned width, unsigned height, int use_requested_extent)
{
   VkExtent2D size = caps->currentExtent;

   /* Android permits a different imageExtent. Its currentExtent can
    * still describe the previous orientation when the resize arrives. */
   if (use_requested_extent || size.width == UINT32_MAX)
   {
      size.width  = width;
      size.height = height;
   }

   if (size.width > caps->maxImageExtent.width)
      size.width = caps->maxImageExtent.width;
   if (size.width < caps->minImageExtent.width)
      size.width = caps->minImageExtent.width;
   if (size.height > caps->maxImageExtent.height)
      size.height = caps->maxImageExtent.height;
   if (size.height < caps->minImageExtent.height)
      size.height = caps->minImageExtent.height;

   return size;
}

#endif
