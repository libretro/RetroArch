/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vulkan_surface_format_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* vulkan_sdr8_surface_format() against the surface format lists drivers
 * report.  A list that puts an RGBA-ordered format ahead of B8G8R8A8
 * must still yield B8G8R8A8, since only that format reaches a display
 * plane through the compositor; a list without it keeps the first 8-bit
 * UNORM entry, and a list with none yields -1. */

#include <stdio.h>

#include "../../../gfx/common/vulkan_surface_format.h"

#define SRGB VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
#define EXT  VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT

static int failures = 0;

static void expect(const char *name,
      const VkSurfaceFormatKHR *formats, uint32_t count, int want)
{
   int got = vulkan_sdr8_surface_format(formats, count);
   if (got != want)
   {
      printf("[FAIL] %s: index %d, expected %d\n", name, got, want);
      failures++;
   }
   else
      printf("[ok]   %s\n", name);
}

int main(void)
{
   /* RGBA listed first, as the NVIDIA Wayland WSI does. */
   static const VkSurfaceFormatKHR rgba_first[] = {
      { VK_FORMAT_R8G8B8A8_UNORM,           SRGB },
      { VK_FORMAT_R8G8B8A8_SRGB,            SRGB },
      { VK_FORMAT_B8G8R8A8_UNORM,           SRGB },
      { VK_FORMAT_B8G8R8A8_SRGB,            SRGB }
   };
   /* BGRA listed first, as Mesa does. */
   static const VkSurfaceFormatKHR bgra_first[] = {
      { VK_FORMAT_B8G8R8A8_SRGB,            SRGB },
      { VK_FORMAT_B8G8R8A8_UNORM,           SRGB },
      { VK_FORMAT_R8G8B8A8_UNORM,           SRGB }
   };
   /* 10-bit and an extended-colour-space BGRA ahead of the sRGB one. */
   static const VkSurfaceFormatKHR wide_first[] = {
      { VK_FORMAT_A2B10G10R10_UNORM_PACK32, SRGB },
      { VK_FORMAT_B8G8R8A8_UNORM,           EXT  },
      { VK_FORMAT_A8B8G8R8_UNORM_PACK32,    SRGB },
      { VK_FORMAT_B8G8R8A8_UNORM,           SRGB }
   };
   /* No BGRA at all: the first 8-bit UNORM entry. */
   static const VkSurfaceFormatKHR no_bgra[] = {
      { VK_FORMAT_R8G8B8A8_SRGB,            SRGB },
      { VK_FORMAT_A8B8G8R8_UNORM_PACK32,    SRGB },
      { VK_FORMAT_R8G8B8A8_UNORM,           SRGB }
   };
   /* BGRA only in a non-sRGB colour space still beats nothing. */
   static const VkSurfaceFormatKHR bgra_ext_only[] = {
      { VK_FORMAT_R16G16B16A16_SFLOAT,      EXT  },
      { VK_FORMAT_B8G8R8A8_UNORM,           EXT  }
   };
   static const VkSurfaceFormatKHR none[] = {
      { VK_FORMAT_R16G16B16A16_SFLOAT,      EXT  },
      { VK_FORMAT_R5G6B5_UNORM_PACK16,      SRGB }
   };

   expect("rgba listed first",     rgba_first,    4, 2);
   expect("bgra listed first",     bgra_first,    3, 1);
   expect("wide formats first",    wide_first,    4, 3);
   expect("no bgra",               no_bgra,       3, 1);
   expect("bgra extended only",    bgra_ext_only, 2, 1);
   expect("no 8-bit unorm",        none,          2, -1);
   expect("empty list",            none,          0, -1);

   if (failures)
   {
      printf("%d failure(s)\n", failures);
      return 1;
   }
   printf("all passed\n");
   return 0;
}
