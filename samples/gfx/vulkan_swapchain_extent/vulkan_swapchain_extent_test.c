/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vulkan_swapchain_extent_test.c).
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

#include <stdio.h>
#include <string.h>

#include "../../../gfx/common/vulkan_swapchain_extent.h"

static int failures;

static void expect(const char *name, const VkSurfaceCapabilitiesKHR *caps,
      unsigned width, unsigned height, int android,
      unsigned want_width, unsigned want_height)
{
   VkExtent2D got = vulkan_swapchain_extent(caps, width, height, android);
   if (got.width != want_width || got.height != want_height)
   {
      fprintf(stderr, "FAIL %s: %ux%u, expected %ux%u\n", name,
            got.width, got.height, want_width, want_height);
      failures++;
   }
}

int main(void)
{
   VkSurfaceCapabilitiesKHR caps;
   unsigned i;
   memset(&caps, 0, sizeof(caps));
   caps.minImageExtent.width  = 1;
   caps.minImageExtent.height = 1;
   caps.maxImageExtent.width  = 4096;
   caps.maxImageExtent.height = 4096;

   /* A rebuild on SUBOPTIMAL can precede the orientation callback.
    * Each callback must win over the extent of the previous swapchain. */
   for (i = 0; i < 20; i++)
   {
      caps.currentExtent.width  = 1280;
      caps.currentExtent.height = 2620;
      expect("early rebuild", &caps, 1280, 2620, 1, 1280, 2620);
      expect("landscape callback", &caps, 2620, 1280, 1, 2620, 1280);
      caps.currentExtent.width  = 2620;
      caps.currentExtent.height = 1280;
      expect("portrait callback", &caps, 1280, 2620, 1, 1280, 2620);
   }
   expect("desktop fixed extent", &caps, 1280, 2620, 0, 2620, 1280);
   caps.currentExtent.width = caps.currentExtent.height = UINT32_MAX;
   expect("desktop variable extent", &caps, 1280, 2620, 0, 1280, 2620);
   expect("android variable extent", &caps, 2620, 1280, 1, 2620, 1280);
   expect("upper clamp", &caps, 5000, 6000, 1, 4096, 4096);
   expect("lower clamp", &caps, 0, 0, 1, 1, 1);
   caps.maxImageExtent.width  = caps.minImageExtent.width  = 0;
   caps.maxImageExtent.height = caps.minImageExtent.height = 0;
   expect("unavailable surface", &caps, 1280, 2620, 1, 0, 0);

   if (failures)
      return 1;
   puts("PASS: swapchain extents follow Android orientation and surface limits");
   return 0;
}
