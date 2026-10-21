/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (stubs_win32.c).
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

/* The DXGI presenter vulkan_common.c reaches for on Windows. The
 * outputs under test never use it: every call says it is not there. */

#if defined(_WIN32) && !defined(__WINRT__)
#include "../../../gfx/common/vulkan_common.h"
#include "../../../gfx/common/win_vulkan_dxgi.h"

ra_vulkan_dxgi_t *ra_vulkan_dxgi_create(VkInstance instance,
      VkPhysicalDevice gpu, VkDevice device, VkQueue queue, unsigned family,
      HWND window, unsigned width, unsigned height, VkFormat format,
      void *queue_lock)
{ return NULL; }
int ra_vulkan_dxgi_resize(ra_vulkan_dxgi_t *presenter,
      unsigned width, unsigned height, VkFormat format)
{ return 0; }
void ra_vulkan_dxgi_destroy(ra_vulkan_dxgi_t *presenter) { }
void ra_vulkan_dxgi_hdr_metadata(ra_vulkan_dxgi_t *presenter, float peak) { }
VkImage ra_vulkan_dxgi_image(ra_vulkan_dxgi_t *presenter, unsigned index)
{ return VK_NULL_HANDLE; }
VkResult ra_vulkan_dxgi_acquire(ra_vulkan_dxgi_t *presenter,
      VkSemaphore semaphore, VkFence fence, unsigned *index,
      unsigned interval)
{ return VK_ERROR_SURFACE_LOST_KHR; }
VkResult ra_vulkan_dxgi_present(ra_vulkan_dxgi_t *presenter,
      unsigned index, unsigned count, const VkSemaphore *wait,
      unsigned interval)
{ return VK_ERROR_SURFACE_LOST_KHR; }
void *ra_vulkan_dxgi_device(ra_vulkan_dxgi_t *presenter) { return NULL; }
#else
typedef int vulkan_output_stubs_win32_unused;
#endif
