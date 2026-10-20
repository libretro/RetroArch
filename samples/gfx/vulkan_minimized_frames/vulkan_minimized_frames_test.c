/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vulkan_minimized_frames_test.c).
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

/* Frames drawn while the window is minimised, through the real
 * vulkan_acquire_next_image() in gfx/common/vulkan_common.c against
 * fake entry points. A minimised window has a 0x0 surface and no
 * swapchain can be made for it, yet the frame loop goes on drawing:
 * each frame re-records the command buffer of the current frame slot
 * and rewrites that slot's buffers and descriptors, then submits with
 * the slot's fence. So whatever the acquire leaves, a slot must not be
 * drawn into again before the submission last made on it is known to
 * have run - that is, it was made with a fence and that fence was
 * waited on since. Drawing into a slot the GPU is still reading is
 * what a GPU answers with VK_ERROR_DEVICE_LOST. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../../gfx/common/vulkan_common.h"

#define NUM_IMAGES 3
#define NUM_FRAMES 64
#define MAX_FENCES 256

static unsigned failures;

static void check(bool ok, const char *what)
{
   if (!ok)
   {
      printf("FAIL: %s\n", what);
      failures++;
   }
}

/* fence handles are 1-based indices into these */
static bool     fence_live[MAX_FENCES];
static bool     fence_pending[MAX_FENCES];
static unsigned num_fences;

/* per frame slot: the last submission's fence, 0 for none at all */
static uintptr_t slot_fence[NUM_IMAGES];
static bool      slot_used[NUM_IMAGES];

static unsigned fence_index(VkFence fence)
{
   uintptr_t i = (uintptr_t)fence;
   return (i && i <= MAX_FENCES) ? (unsigned)(i - 1) : MAX_FENCES;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_caps(VkPhysicalDevice gpu,
      VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *caps)
{
   /* minimised: no client area */
   memset(caps, 0, sizeof(*caps));
   return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_create_fence(VkDevice device,
      const VkFenceCreateInfo *info, const VkAllocationCallbacks *alloc,
      VkFence *fence)
{
   if (num_fences == MAX_FENCES)
      return VK_ERROR_OUT_OF_HOST_MEMORY;
   fence_live[num_fences]    = true;
   fence_pending[num_fences] = false;
   *fence = (VkFence)(uintptr_t)(++num_fences);
   return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL fake_destroy_fence(VkDevice device,
      VkFence fence, const VkAllocationCallbacks *alloc)
{
   unsigned i = fence_index(fence);
   if (i < MAX_FENCES)
   {
      check(!fence_pending[i], "a fence is destroyed with its submission unwaited");
      fence_live[i] = false;
   }
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_wait_fences(VkDevice device,
      uint32_t count, const VkFence *fences, VkBool32 all, uint64_t timeout)
{
   uint32_t n;
   for (n = 0; n < count; n++)
   {
      unsigned i = fence_index(fences[n]);
      if (i < MAX_FENCES)
         fence_pending[i] = false;
   }
   return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_reset_fences(VkDevice device,
      uint32_t count, const VkFence *fences)
{
   uint32_t n;
   for (n = 0; n < count; n++)
   {
      unsigned i = fence_index(fences[n]);
      if (i < MAX_FENCES)
         check(!fence_pending[i], "a fence is reset with its submission unwaited");
   }
   return VK_SUCCESS;
}

/* The frontend and the DXGI presenter, as far as the file links them */
bool video_driver_thread_wrapper_active(void) { return false; }

#ifdef VULKAN_DXGI_PACING
#include "../../../gfx/common/win_vulkan_dxgi.h"
ra_vulkan_dxgi_t *ra_vulkan_dxgi_create(VkInstance instance,
      VkPhysicalDevice gpu, VkDevice device, VkQueue queue, unsigned family,
      HWND window, unsigned width, unsigned height, VkFormat format,
      void *queue_lock) { return NULL; }
int ra_vulkan_dxgi_resize(ra_vulkan_dxgi_t *p, unsigned width,
      unsigned height, VkFormat format) { return 0; }
int ra_vulkan_dxgi_presentable(ra_vulkan_dxgi_t *p) { return 0; }
void ra_vulkan_dxgi_destroy(ra_vulkan_dxgi_t *p) { }
void ra_vulkan_dxgi_hdr_metadata(ra_vulkan_dxgi_t *p, float peak) { }
int64_t ra_vulkan_dxgi_present_time(ra_vulkan_dxgi_t *p) { return 0; }
VkImage ra_vulkan_dxgi_image(ra_vulkan_dxgi_t *p, unsigned index)
{ return VK_NULL_HANDLE; }
VkResult ra_vulkan_dxgi_acquire(ra_vulkan_dxgi_t *p, VkSemaphore semaphore,
      VkFence fence, unsigned *index, unsigned interval)
{ return VK_ERROR_OUT_OF_DATE_KHR; }
VkResult ra_vulkan_dxgi_present(ra_vulkan_dxgi_t *p, unsigned index,
      unsigned count, const VkSemaphore *wait, unsigned interval)
{ return VK_SUCCESS; }
void *ra_vulkan_dxgi_device(ra_vulkan_dxgi_t *p) { return NULL; }
#endif

static gfx_ctx_vulkan_data_t vk;

/* As vulkan_frame() does with the slot the acquire left: record into
 * it, then submit with its fence and mark the fence signalled. */
static void draw_frame(unsigned frame)
{
   unsigned slot = vk.context.current_frame_index;
   VkFence  fence;
   char     what[128];

   if (slot >= NUM_IMAGES)
   {
      check(false, "the frame slot is out of range");
      return;
   }

   if (slot_used[slot])
   {
      unsigned i = fence_index((VkFence)slot_fence[slot]);
      snprintf(what, sizeof(what), "frame %u re-records slot %u while its"
            " last submission %s", frame, slot,
            slot_fence[slot] ? "is still running" : "had no fence");
      check(slot_fence[slot] && i < MAX_FENCES && !fence_pending[i], what);
   }

   fence = vk.context.swapchain_fences[slot];
   if (fence != VK_NULL_HANDLE)
   {
      unsigned i = fence_index(fence);
      if (i < MAX_FENCES)
         fence_pending[i] = true;
   }
   slot_fence[slot] = (uintptr_t)fence;
   slot_used[slot]  = true;
   vk.context.swapchain_fences_signalled[slot] = true;
}

int main(void)
{
   unsigned frame;

   vkGetPhysicalDeviceSurfaceCapabilitiesKHR = fake_caps;
   vkCreateFence                             = fake_create_fence;
   vkDestroyFence                            = fake_destroy_fence;
   vkWaitForFences                           = fake_wait_fences;
   vkResetFences                             = fake_reset_fences;

   /* As a present that came back out of date leaves the context of a
    * window just minimised: the swapchain and its fences are gone, the
    * count of images and the frame slot are what they were. */
   memset(&vk, 0, sizeof(vk));
   vk.context.device                 = (VkDevice)(uintptr_t)0x10;
   vk.context.num_swapchain_images   = NUM_IMAGES;
   vk.context.current_frame_index    = 1;
   vk.context.swapchain_dims         = VIDEO_SCALE_PACK(1920, 1080);
   vk.context.swap_interval          = 1;
   vk.context.swapchain_settings.vsync = true;

   printf("vulkan frames with the window minimised:\n");
   for (frame = 0; frame < NUM_FRAMES; frame++)
   {
      vulkan_acquire_next_image(&vk);
      check(!vulkan_has_swapchain(&vk),
            "a swapchain was made for a 0x0 surface");
      check(!(vk.context.flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN),
            "an image was acquired with no swapchain");
      draw_frame(frame);
   }

   if (failures)
   {
      printf("FAILED: %u check(s)\n", failures);
      return 1;
   }
   printf("ok: %u frames drawn minimised, each into a slot whose last "
          "submission was fenced and waited on\n", NUM_FRAMES);
   return 0;
}
