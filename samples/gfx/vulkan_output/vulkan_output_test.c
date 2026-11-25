/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vulkan_output_test.c).
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

/* vulkan_output_* (gfx/common/vulkan_common.c), the second output's
 * swapchain, against a fake device: the real code, with every Vulkan
 * entry point it calls pointed at a fake here. What it pins down is
 * that nothing the output does in a frame waits - no idle wait, no
 * fence wait, no semaphore from the presentation engine in the queue -
 * and that nothing is destroyed before the fence that covers it. */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <boolean.h>
#include <rthreads/rthreads.h>

#include "../../../gfx/common/vulkan_common.h"

bool video_driver_thread_wrapper_active(void) { return false; }

static unsigned failures;
#define CHECK(c) do { if (!(c)) { failures++; \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

#define FAKE(type) ((type)(uintptr_t)(next_handle++))

enum fence_state { FENCE_UNSIGNALED = 0, FENCE_SIGNALED, FENCE_GPU, FENCE_PE };

static uintptr_t next_handle = 0x1000;
static struct { VkFence handle; int state; } fences[16];
static unsigned num_fences;

static int live_surfaces, live_swapchains, live_views, live_framebuffers;
static int live_semaphores, live_fences;
static int idle_waits, fence_waits, acquires, creates, empty_submits;

/* What the fake device answers */
static int queue_presents       = 1;
static VkFormat surface_format  = VK_FORMAT_B8G8R8A8_UNORM;
/* A third pair, listed when its format is set */
static VkFormat surface_format2;
static VkColorSpaceKHR surface_space2;
/* In place of those, a depth-30 X surface under the fake HDR10 layer:
 * A2R10G10B10 in HDR10, then in sRGB (1), or in HDR10 only (2) */
static int depth30;
/* In place of all of those, exactly these pairs, when any are set */
static VkSurfaceFormatKHR pairs[4];
static uint32_t num_pairs;
static VkPresentModeKHR modes[2];
static uint32_t num_modes;
static uint32_t min_images      = 2;
static uint32_t extra_images;      /* the driver makes more than asked */
static VkResult acquire_result  = VK_SUCCESS;
static int acquire_ready        = 1;
static VkResult present_result  = VK_SUCCESS;
static int gpu_done             = 1;
/* The presentation engine never gives an acquired image back */
static int pe_never;
/* The caller has no render pass for the format asked */
static int pass_none;

/* What the caller sets */
static bool sdr_10bit;

/* What the code under test asked for */
static VkSwapchainKHR old_swapchain;
static VkPresentModeKHR made_mode;
static VkFormat made_format;
static VkColorSpaceKHR made_space;
static VkFormat made_view_format;
static VkRenderPass made_fb_pass;
static uint32_t made_min_images;
static VkSemaphore present_wait;
static VkFormat pass_asked;
static VkSwapchainKHR destroyed[16];
static unsigned num_destroyed;
static uint32_t image_counter;
/* What the HDR metadata was sent for */
static int meta_sends;
static VkSwapchainKHR meta_swapchain;
static float meta_peak;

static int *fence_state(VkFence f)
{
   unsigned i;
   for (i = 0; i < num_fences; i++)
      if (fences[i].handle == f)
         return &fences[i].state;
   return NULL;
}

static void complete_gpu(void)
{
   unsigned i;
   for (i = 0; i < num_fences; i++)
      if (fences[i].state == FENCE_GPU)
         fences[i].state = FENCE_SIGNALED;
}

static void release_images(void)
{
   unsigned i;
   for (i = 0; i < num_fences; i++)
      if (fences[i].state == FENCE_PE)
         fences[i].state = FENCE_SIGNALED;
}

static int was_destroyed(VkSwapchainKHR s)
{
   unsigned i;
   for (i = 0; i < num_destroyed; i++)
      if (destroyed[i] == s)
         return 1;
   return 0;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_create_surface(VkInstance i,
      const void *info, const VkAllocationCallbacks *a, VkSurfaceKHR *s)
{
   (void)i; (void)info; (void)a;
   *s = FAKE(VkSurfaceKHR);
   live_surfaces++;
   return VK_SUCCESS;
}

static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL fake_gipa(VkInstance i,
      const char *name)
{
   (void)i;
   if (     !strcmp(name, "vkCreateXlibSurfaceKHR")
         || !strcmp(name, "vkCreateWin32SurfaceKHR"))
      return (PFN_vkVoidFunction)fake_create_surface;
   return NULL;
}

static VKAPI_ATTR void VKAPI_CALL fake_destroy_surface(VkInstance i,
      VkSurfaceKHR s, const VkAllocationCallbacks *a)
{ (void)i; (void)s; (void)a; live_surfaces--; }

static VKAPI_ATTR VkResult VKAPI_CALL fake_surface_support(
      VkPhysicalDevice g, uint32_t q, VkSurfaceKHR s, VkBool32 *ok)
{ (void)g; (void)q; (void)s; *ok = queue_presents ? VK_TRUE : VK_FALSE;
  return VK_SUCCESS; }

static VKAPI_ATTR VkResult VKAPI_CALL fake_surface_caps(VkPhysicalDevice g,
      VkSurfaceKHR s, VkSurfaceCapabilitiesKHR *c)
{
   (void)g; (void)s;
   memset(c, 0, sizeof(*c));
   c->minImageCount           = min_images;
   c->maxImageCount           = 8;
   c->currentExtent.width     = 1240;
   c->currentExtent.height    = 1080;
   c->minImageExtent.width    = 1;
   c->minImageExtent.height   = 1;
   c->maxImageExtent.width    = 4096;
   c->maxImageExtent.height   = 4096;
   c->maxImageArrayLayers     = 1;
   c->supportedTransforms     = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
   c->currentTransform        = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
   c->supportedCompositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
   c->supportedUsageFlags     = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
   return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_surface_formats(
      VkPhysicalDevice g, VkSurfaceKHR s, uint32_t *n, VkSurfaceFormatKHR *f)
{
   uint32_t listed = surface_format2 != VK_FORMAT_UNDEFINED ? 3 : 2;
   (void)g; (void)s;
   if (num_pairs)
   {
      if (f && *n >= num_pairs)
         memcpy(f, pairs, num_pairs * sizeof(*f));
      *n = num_pairs;
      return VK_SUCCESS;
   }
   if (depth30)
   {
      listed = depth30 == 1 ? 2 : 1;
      if (f && *n >= listed)
      {
         f[0].format     = VK_FORMAT_A2R10G10B10_UNORM_PACK32;
         f[0].colorSpace = VK_COLOR_SPACE_HDR10_ST2084_EXT;
         if (listed > 1)
         {
            f[1].format     = VK_FORMAT_A2R10G10B10_UNORM_PACK32;
            f[1].colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
         }
      }
      *n = listed;
      return VK_SUCCESS;
   }
   if (f && *n >= listed)
   {
      f[0].format     = VK_FORMAT_R5G6B5_UNORM_PACK16;
      f[0].colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
      f[1].format     = surface_format;
      f[1].colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
      if (listed > 2)
      {
         f[2].format     = surface_format2;
         f[2].colorSpace = surface_space2;
      }
   }
   *n = listed;
   return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_surface_modes(VkPhysicalDevice g,
      VkSurfaceKHR s, uint32_t *n, VkPresentModeKHR *m)
{
   uint32_t i;
   (void)g; (void)s;
   if (m)
      for (i = 0; i < num_modes && i < *n; i++)
         m[i] = modes[i];
   *n = num_modes;
   return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_create_swapchain(VkDevice d,
      const VkSwapchainCreateInfoKHR *info, const VkAllocationCallbacks *a,
      VkSwapchainKHR *s)
{
   (void)d; (void)a;
   creates++;
   old_swapchain   = info->oldSwapchain;
   made_mode       = info->presentMode;
   made_min_images = info->minImageCount;
   made_format     = info->imageFormat;
   made_space      = info->imageColorSpace;
   *s              = FAKE(VkSwapchainKHR);
   live_swapchains++;
   return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL fake_destroy_swapchain(VkDevice d,
      VkSwapchainKHR s, const VkAllocationCallbacks *a)
{
   (void)d; (void)a;
   if (num_destroyed < 16)
      destroyed[num_destroyed++] = s;
   live_swapchains--;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_swapchain_images(VkDevice d,
      VkSwapchainKHR s, uint32_t *n, VkImage *img)
{
   uint32_t i;
   uint32_t made = made_min_images + extra_images;
   (void)d; (void)s;
   if (img)
   {
      for (i = 0; i < made && i < *n; i++)
         img[i] = FAKE(VkImage);
      if (*n < made)
         return VK_INCOMPLETE;
   }
   *n = made;
   return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_create_view(VkDevice d,
      const VkImageViewCreateInfo *i, const VkAllocationCallbacks *a,
      VkImageView *v)
{ (void)d; (void)a; made_view_format = i->format; *v = FAKE(VkImageView);
  live_views++; return VK_SUCCESS; }

static VKAPI_ATTR void VKAPI_CALL fake_destroy_view(VkDevice d,
      VkImageView v, const VkAllocationCallbacks *a)
{ (void)d; (void)v; (void)a; live_views--; }

static VKAPI_ATTR VkResult VKAPI_CALL fake_create_fb(VkDevice d,
      const VkFramebufferCreateInfo *i, const VkAllocationCallbacks *a,
      VkFramebuffer *f)
{ (void)d; (void)a; made_fb_pass = i->renderPass; *f = FAKE(VkFramebuffer);
  live_framebuffers++; return VK_SUCCESS; }

static VKAPI_ATTR void VKAPI_CALL fake_destroy_fb(VkDevice d,
      VkFramebuffer f, const VkAllocationCallbacks *a)
{ (void)d; (void)f; (void)a; live_framebuffers--; }

static VKAPI_ATTR VkResult VKAPI_CALL fake_create_sem(VkDevice d,
      const VkSemaphoreCreateInfo *i, const VkAllocationCallbacks *a,
      VkSemaphore *s)
{ (void)d; (void)i; (void)a; *s = FAKE(VkSemaphore); live_semaphores++;
  return VK_SUCCESS; }

static VKAPI_ATTR void VKAPI_CALL fake_destroy_sem(VkDevice d,
      VkSemaphore s, const VkAllocationCallbacks *a)
{ (void)d; (void)s; (void)a; live_semaphores--; }

static VKAPI_ATTR VkResult VKAPI_CALL fake_create_fence(VkDevice d,
      const VkFenceCreateInfo *i, const VkAllocationCallbacks *a, VkFence *f)
{
   (void)d; (void)i; (void)a;
   *f                         = FAKE(VkFence);
   fences[num_fences].handle  = *f;
   fences[num_fences++].state = FENCE_UNSIGNALED;
   live_fences++;
   return VK_SUCCESS;
}

static VKAPI_ATTR void VKAPI_CALL fake_destroy_fence(VkDevice d, VkFence f,
      const VkAllocationCallbacks *a)
{
   int *st = fence_state(f);
   (void)d; (void)a;
   /* A fence the GPU or the presentation engine still owes is not
    * destroyed */
   CHECK(st && *st != FENCE_GPU && *st != FENCE_PE);
   live_fences--;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_reset_fences(VkDevice d,
      uint32_t n, const VkFence *f)
{
   uint32_t i;
   (void)d;
   for (i = 0; i < n; i++)
   {
      int *st = fence_state(f[i]);
      CHECK(st && *st != FENCE_GPU && *st != FENCE_PE);
      if (st)
         *st = FENCE_UNSIGNALED;
   }
   return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_fence_status(VkDevice d,
      VkFence f)
{
   int *st = fence_state(f);
   (void)d;
   return (st && *st == FENCE_SIGNALED) ? VK_SUCCESS : VK_NOT_READY;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_wait_fences(VkDevice d,
      uint32_t n, const VkFence *f, VkBool32 all, uint64_t timeout)
{
   uint32_t i;
   (void)d; (void)all; (void)timeout;
   fence_waits++;
   for (i = 0; i < n; i++)
   {
      int *st = fence_state(f[i]);
      if (st && *st == FENCE_PE && pe_never)
         return VK_TIMEOUT;
      if (st)
         *st = FENCE_SIGNALED;
   }
   return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_acquire(VkDevice d,
      VkSwapchainKHR s, uint64_t timeout, VkSemaphore sem, VkFence f,
      uint32_t *index)
{
   int *st = fence_state(f);
   (void)d; (void)s;
   acquires++;
   /* No wait, and nothing for the queue to wait on */
   CHECK(timeout == 0);
   CHECK(sem == VK_NULL_HANDLE);
   CHECK(st && *st == FENCE_UNSIGNALED);
   if (acquire_result != VK_SUCCESS && acquire_result != VK_SUBOPTIMAL_KHR)
   {
      VkResult r = acquire_result;
      /* Out of date once: the remade swapchain is fine */
      if (r == VK_ERROR_OUT_OF_DATE_KHR)
         acquire_result = VK_SUCCESS;
      return r;
   }
   *index = image_counter++ % made_min_images;
   if (st)
      *st  = acquire_ready ? FENCE_SIGNALED : FENCE_PE;
   return acquire_result;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_present(VkQueue q,
      const VkPresentInfoKHR *p)
{
   (void)q;
   present_wait = p->pWaitSemaphores[0];
   if (p->pResults)
      p->pResults[0] = present_result;
   return present_result;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_submit(VkQueue q, uint32_t n,
      const VkSubmitInfo *s, VkFence f)
{
   int *st = fence_state(f);
   (void)q;
   CHECK(n == 1 && s);
   if (n == 1 && s)
   {
      CHECK(s->sType == VK_STRUCTURE_TYPE_SUBMIT_INFO);
      CHECK(s->pNext == NULL);
      CHECK(s->waitSemaphoreCount == 0 && s->pWaitSemaphores == NULL);
      CHECK(s->pWaitDstStageMask == NULL);
      CHECK(s->commandBufferCount == 0 && s->pCommandBuffers == NULL);
      CHECK(s->signalSemaphoreCount == 0 && s->pSignalSemaphores == NULL);
   }
   empty_submits++;
   if (st)
      *st = gpu_done ? FENCE_SIGNALED : FENCE_GPU;
   return VK_SUCCESS;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_queue_idle(VkQueue q)
{ (void)q; idle_waits++; return VK_SUCCESS; }

static VKAPI_ATTR VkResult VKAPI_CALL fake_device_idle(VkDevice d)
{ (void)d; idle_waits++; return VK_SUCCESS; }

static VKAPI_ATTR void VKAPI_CALL fake_set_hdr_metadata(VkDevice d,
      uint32_t n, const VkSwapchainKHR *s, const VkHdrMetadataEXT *m)
{
   (void)d;
   CHECK(n == 1);
   meta_sends++;
   meta_swapchain = s[0];
   meta_peak      = m->maxLuminance;
}

static VkRenderPass fake_pass;

static VkRenderPass fake_pass_for(void *data, VkFormat format)
{
   (void)data;
   pass_asked = format;
   return pass_none ? VK_NULL_HANDLE : fake_pass;
}

static void install(void)
{
   vulkan_symbol_wrapper_init(fake_gipa);
   vkDestroySurfaceKHR                       = fake_destroy_surface;
   vkGetPhysicalDeviceSurfaceSupportKHR      = fake_surface_support;
   vkGetPhysicalDeviceSurfaceCapabilitiesKHR = fake_surface_caps;
   vkGetPhysicalDeviceSurfaceFormatsKHR      = fake_surface_formats;
   vkGetPhysicalDeviceSurfacePresentModesKHR = fake_surface_modes;
   vkCreateSwapchainKHR                      = fake_create_swapchain;
   vkDestroySwapchainKHR                     = fake_destroy_swapchain;
   vkGetSwapchainImagesKHR                   = fake_swapchain_images;
   vkCreateImageView                         = fake_create_view;
   vkDestroyImageView                        = fake_destroy_view;
   vkCreateFramebuffer                       = fake_create_fb;
   vkDestroyFramebuffer                      = fake_destroy_fb;
   vkCreateSemaphore                         = fake_create_sem;
   vkDestroySemaphore                        = fake_destroy_sem;
   vkCreateFence                             = fake_create_fence;
   vkDestroyFence                            = fake_destroy_fence;
   vkResetFences                             = fake_reset_fences;
   vkGetFenceStatus                          = fake_fence_status;
   vkWaitForFences                           = fake_wait_fences;
   vkAcquireNextImageKHR                     = fake_acquire;
   vkQueuePresentKHR                         = fake_present;
   vkQueueSubmit                             = fake_submit;
   vkQueueWaitIdle                           = fake_queue_idle;
   vkDeviceWaitIdle                          = fake_device_idle;
}

static char instance_obj, gpu_obj, device_obj, queue_obj;
static vulkan_context_t ctx;

static void reset(void)
{
   num_fences       = 0;
   live_surfaces    = live_swapchains = live_views = 0;
   live_framebuffers = live_semaphores = live_fences = 0;
   idle_waits       = fence_waits = acquires = creates = empty_submits = 0;
   queue_presents   = 1;
   surface_format   = VK_FORMAT_B8G8R8A8_UNORM;
   modes[0]         = VK_PRESENT_MODE_FIFO_KHR;
   modes[1]         = VK_PRESENT_MODE_MAILBOX_KHR;
   num_modes        = 2;
   min_images       = 2;
   extra_images     = 0;
   acquire_result   = VK_SUCCESS;
   acquire_ready    = 1;
   present_result   = VK_SUCCESS;
   gpu_done         = 1;
   pe_never         = 0;
   num_destroyed    = 0;
   image_counter    = 0;
   surface_format2  = VK_FORMAT_UNDEFINED;
   surface_space2   = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
   depth30          = 0;
   num_pairs        = 0;
   meta_sends       = 0;
   meta_swapchain   = VK_NULL_HANDLE;
   meta_peak        = 0.0f;
   pass_none        = 0;
   sdr_10bit        = false;
   pass_asked       = VK_FORMAT_UNDEFINED;
   made_format      = VK_FORMAT_UNDEFINED;
   made_space       = VK_COLOR_SPACE_MAX_ENUM_KHR;
   made_view_format = VK_FORMAT_UNDEFINED;
   made_fb_pass     = VK_NULL_HANDLE;
   /* The owner's output is SDR */
   ctx.swapchain_format       = VK_FORMAT_B8G8R8A8_UNORM;
   ctx.swapchain_colour_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
   ctx.flags                 &= ~VK_CTX_FLAG_HDR_ENABLE;
}

static void hdr10_owner(void)
{
   ctx.swapchain_format       = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
   ctx.swapchain_colour_space = VK_COLOR_SPACE_HDR10_ST2084_EXT;
   ctx.flags                 |= VK_CTX_FLAG_HDR_ENABLE;
}

static void list_pair(VkFormat format, VkColorSpaceKHR space)
{
   pairs[num_pairs].format       = format;
   pairs[num_pairs++].colorSpace = space;
}

static bool make(vulkan_output_t *out)
{
#ifdef _WIN32
   static HINSTANCE inst;
   static HWND wnd;
   void *display = &inst;
   void *surface = &wnd;
   enum vulkan_wsi_type type = VULKAN_WSI_WIN32;
#else
   static Window wnd = 1;
   void *display = NULL;
   void *surface = &wnd;
   enum vulkan_wsi_type type = VULKAN_WSI_XLIB;
#endif
   memset(out, 0, sizeof(*out));
   /* Not 0, which is sRGB: a check of it sees what was picked */
   out->color_space      = VK_COLOR_SPACE_MAX_ENUM_KHR;
   out->pass_for         = fake_pass_for;
   out->set_hdr_metadata = fake_set_hdr_metadata;
   out->sdr_10bit        = sdr_10bit;
   return vulkan_output_create(out, &ctx, type, display, surface,
         VIDEO_SCALE_PACK(1240, 1080));
}

static void all_gone(void)
{
   CHECK(live_surfaces == 0);
   CHECK(live_swapchains == 0);
   CHECK(live_views == 0);
   CHECK(live_framebuffers == 0);
   CHECK(live_semaphores == 0);
   CHECK(live_fences == 0);
}

/* Made in the context's format, MAILBOX first, one image over the least */
static void test_create(void)
{
   vulkan_output_t out;
   reset();
   CHECK(make(&out));
   CHECK(out.swapchain != VK_NULL_HANDLE);
   CHECK(made_mode == VK_PRESENT_MODE_MAILBOX_KHR);
   CHECK(made_min_images == 3);
   CHECK(out.num_images == 3);
   CHECK(live_views == 3 && live_framebuffers == 3 && live_semaphores == 3);
   CHECK(out.dims == VIDEO_SCALE_PACK(1240, 1080));
   CHECK(made_format == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(made_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(out.color_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(pass_asked == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(out.render_pass == fake_pass && made_fb_pass == fake_pass);
   CHECK(vulkan_output_retire(&out));
   all_gone();

   reset();
   num_modes = 1;
   CHECK(make(&out));
   CHECK(made_mode == VK_PRESENT_MODE_FIFO_KHR);
   CHECK(vulkan_output_retire(&out));
   all_gone();
   CHECK(idle_waits == 0 && fence_waits == 0);
}

/* No surface format to match, or a queue that can't present: refused,
 * and retire still frees the surface */
static void test_refused(void)
{
   vulkan_output_t out;
   reset();
   surface_format = VK_FORMAT_R8G8B8A8_UNORM;
   CHECK(!make(&out));
   CHECK(out.flags & VK_OUTPUT_FLAG_LOST);
   CHECK(vulkan_output_retire(&out));
   all_gone();

   reset();
   queue_presents = 0;
   CHECK(!make(&out));
   CHECK(vulkan_output_retire(&out));
   all_gone();

   reset();
   extra_images = 20;
   CHECK(!make(&out));
   CHECK(vulkan_output_retire(&out));
   all_gone();
}

/* The caller has no render pass for the format: refused before any
 * swapchain is made */
static void test_no_pass(void)
{
   vulkan_output_t out;
   reset();
   pass_none = 1;
   CHECK(!make(&out));
   CHECK(out.flags & VK_OUTPUT_FLAG_LOST);
   CHECK(pass_asked == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(creates == 0);
   CHECK(vulkan_output_retire(&out));
   all_gone();
}

/* Under HDR output a second output is made in SDR, in a format of its
 * own, with the caller's render pass for that format */
static void test_hdr_owner_sdr_surface(void)
{
   vulkan_output_t out;
   reset();
   hdr10_owner();
   CHECK(make(&out));
   CHECK(!(out.flags & VK_OUTPUT_FLAG_LOST));
   CHECK(out.format == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(out.color_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(made_format == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(made_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(made_view_format == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(pass_asked == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(out.render_pass == fake_pass && made_fb_pass == fake_pass);
   CHECK(vulkan_output_retire(&out));
   all_gone();
}

/* 10-bit sRGB only with the main window's 10-bit SDR option, as the
 * main window takes it */
static void test_hdr_owner_sdr_10bit(void)
{
   vulkan_output_t out;
   reset();
   hdr10_owner();
   surface_format2 = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
   sdr_10bit       = true;
   CHECK(make(&out));
   CHECK(out.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
   CHECK(out.color_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(made_format == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
   CHECK(made_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(pass_asked == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
   CHECK(vulkan_output_retire(&out));
   all_gone();

   reset();
   hdr10_owner();
   surface_format2 = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
   CHECK(make(&out));
   CHECK(out.format == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(out.color_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(made_format == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(pass_asked == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(vulkan_output_retire(&out));
   all_gone();
}

/* No 8-bit format, as on a depth-30 X window: the first sRGB pair, as
 * the main window falls back, never the HDR one; without one, lost */
static void test_hdr_owner_no_8bit(void)
{
   vulkan_output_t out;
   reset();
   hdr10_owner();
   depth30 = 1;
   CHECK(make(&out));
   CHECK(!(out.flags & VK_OUTPUT_FLAG_LOST));
   CHECK(out.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32);
   CHECK(made_format == VK_FORMAT_A2R10G10B10_UNORM_PACK32);
   CHECK(made_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(pass_asked == VK_FORMAT_A2R10G10B10_UNORM_PACK32);
   CHECK(vulkan_output_retire(&out));
   all_gone();

   reset();
   hdr10_owner();
   depth30 = 2;
   CHECK(!make(&out));
   CHECK(out.flags & VK_OUTPUT_FLAG_LOST);
   CHECK(creates == 0);
   CHECK(vulkan_output_retire(&out));
   all_gone();
}

/* With the 10-bit SDR option and the main window's format not listed,
 * a 10-bit format in HDR10 and in sRGB: the sRGB pair, never HDR10 */
static void test_hdr_owner_other_10bit(void)
{
   vulkan_output_t out;
   reset();
   hdr10_owner();
   ctx.swapchain_format = VK_FORMAT_A2R10G10B10_UNORM_PACK32;
   list_pair(VK_FORMAT_A2B10G10R10_UNORM_PACK32,
         VK_COLOR_SPACE_HDR10_ST2084_EXT);
   list_pair(VK_FORMAT_A2B10G10R10_UNORM_PACK32,
         VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   list_pair(VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   sdr_10bit = true;
   CHECK(make(&out));
   CHECK(out.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
   CHECK(out.color_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(made_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(pass_asked == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
   CHECK(meta_sends == 0);
   CHECK(vulkan_output_retire(&out));
   all_gone();
}

/* Under HDR output the SDR pick is in the sRGB colour space only, and
 * never an *_SRGB format, whose views would encode the values again */
static void test_hdr_owner_sdr_space_and_encoding(void)
{
   vulkan_output_t out;
   reset();
   hdr10_owner();
   list_pair(VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT);
   list_pair(VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(make(&out));
   CHECK(out.format == VK_FORMAT_R8G8B8A8_UNORM);
   CHECK(out.color_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(made_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(vulkan_output_retire(&out));
   all_gone();

   reset();
   hdr10_owner();
   list_pair(VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   list_pair(VK_FORMAT_A2B10G10R10_UNORM_PACK32,
         VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(make(&out));
   CHECK(out.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
   CHECK(made_view_format == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
   CHECK(vulkan_output_retire(&out));
   all_gone();

   reset();
   hdr10_owner();
   list_pair(VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   list_pair(VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT);
   CHECK(!make(&out));
   CHECK(out.flags & VK_OUTPUT_FLAG_LOST);
   CHECK(creates == 0);
   CHECK(vulkan_output_retire(&out));
   all_gone();
}

/* A surface that lists the main window's HDR pair is made in it, and
 * its metadata says the display's peak, or 1000 nits */
static void test_hdr_surface_takes_owner_mode(void)
{
   vulkan_output_t out;
   reset();
   hdr10_owner();
   list_pair(VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   list_pair(VK_FORMAT_A2B10G10R10_UNORM_PACK32,
         VK_COLOR_SPACE_HDR10_ST2084_EXT);
   CHECK(make(&out));
   CHECK(out.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
   CHECK(out.color_space == VK_COLOR_SPACE_HDR10_ST2084_EXT);
   CHECK(made_format == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
   CHECK(made_space == VK_COLOR_SPACE_HDR10_ST2084_EXT);
   CHECK(pass_asked == VK_FORMAT_A2B10G10R10_UNORM_PACK32);
   CHECK(meta_sends == 1);
   CHECK(meta_swapchain == out.swapchain);
   CHECK(meta_peak == 1000.0f);

   out.peak_nits = 600.0f;
   CHECK(vulkan_output_resize(&out, VIDEO_SCALE_PACK(800, 600)));
   CHECK(creates == 2);
   CHECK(meta_sends == 2);
   CHECK(meta_swapchain == out.swapchain);
   CHECK(meta_peak == 600.0f);
   CHECK(vulkan_output_retire(&out));
   all_gone();
}

/* An HDR10 surface under scRGB output: SDR, with no metadata */
static void test_scrgb_owner_hdr10_surface(void)
{
   vulkan_output_t out;
   reset();
   ctx.swapchain_format       = VK_FORMAT_R16G16B16A16_SFLOAT;
   ctx.swapchain_colour_space = VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT;
   ctx.flags                 |= VK_CTX_FLAG_HDR_ENABLE;
   list_pair(VK_FORMAT_A2B10G10R10_UNORM_PACK32,
         VK_COLOR_SPACE_HDR10_ST2084_EXT);
   list_pair(VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(make(&out));
   CHECK(out.format == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(out.color_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(made_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(pass_asked == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(meta_sends == 0);
   CHECK(vulkan_output_retire(&out));
   all_gone();
}

/* A surface that loses HDR is remade in SDR at the next out of date,
 * with the render pass for its new format, not lost */
static void test_surface_loses_hdr(void)
{
   vulkan_output_t out;
   reset();
   hdr10_owner();
   list_pair(VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   list_pair(VK_FORMAT_A2B10G10R10_UNORM_PACK32,
         VK_COLOR_SPACE_HDR10_ST2084_EXT);
   CHECK(make(&out));
   CHECK(out.color_space == VK_COLOR_SPACE_HDR10_ST2084_EXT);

   num_pairs = 0;
   list_pair(VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   acquire_result = VK_ERROR_OUT_OF_DATE_KHR;
   CHECK(vulkan_output_acquire(&out));
   CHECK(!(out.flags & VK_OUTPUT_FLAG_LOST));
   CHECK(creates == 2);
   CHECK(out.format == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(out.color_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(made_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(pass_asked == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(meta_sends == 1);
   CHECK(vulkan_output_present(&out));
   CHECK(vulkan_output_retire(&out));
   all_gone();
}

/* HDR output turned off again: the next swapchain is in the main
 * window's own pair */
static void test_owner_hdr_off_again(void)
{
   vulkan_output_t out;
   reset();
   hdr10_owner();
   list_pair(VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   list_pair(VK_FORMAT_A2B10G10R10_UNORM_PACK32,
         VK_COLOR_SPACE_HDR10_ST2084_EXT);
   CHECK(make(&out));
   CHECK(out.color_space == VK_COLOR_SPACE_HDR10_ST2084_EXT);

   ctx.flags            &= ~VK_CTX_FLAG_HDR_ENABLE;
   ctx.swapchain_format  = VK_FORMAT_B8G8R8A8_UNORM;
   CHECK(vulkan_output_resize(&out, VIDEO_SCALE_PACK(1240, 1080)));
   CHECK(creates == 2);
   CHECK(out.format == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(out.color_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(made_format == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(made_space == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
   CHECK(pass_asked == VK_FORMAT_B8G8R8A8_UNORM);
   CHECK(meta_sends == 1);
   CHECK(vulkan_output_retire(&out));
   all_gone();
}

/* No image at once is a skipped frame; an image the presentation engine
 * still reads is kept, not waited for */
static void test_acquire(void)
{
   vulkan_output_t out;
   reset();
   CHECK(make(&out));

   acquire_result = VK_NOT_READY;
   CHECK(!vulkan_output_acquire(&out));
   CHECK(!(out.flags & VK_OUTPUT_FLAG_ACQUIRED));

   acquire_result = VK_SUCCESS;
   acquire_ready  = 0;
   CHECK(!vulkan_output_acquire(&out));
   CHECK(out.flags & VK_OUTPUT_FLAG_ACQUIRED);
   CHECK(acquires == 2);
   CHECK(!vulkan_output_acquire(&out));
   CHECK(acquires == 2);
   /* Nor can the swapchain be remade under it */
   CHECK(!vulkan_output_resize(&out, VIDEO_SCALE_PACK(800, 600)));
   CHECK(creates == 1);

   release_images();
   CHECK(vulkan_output_acquire(&out));
   CHECK(acquires == 2);
   CHECK(vulkan_output_present(&out));
   CHECK(present_wait == out.semaphores[out.index]);
   CHECK(!(out.flags & (VK_OUTPUT_FLAG_ACQUIRED | VK_OUTPUT_FLAG_READY)));

   acquire_ready = 1;
   CHECK(vulkan_output_acquire(&out));
   CHECK(acquires == 3);
   CHECK(vulkan_output_present(&out));
   CHECK(idle_waits == 0 && fence_waits == 0);
   CHECK(vulkan_output_retire(&out));
   all_gone();
}

/* Out of date remakes the swapchain at once; the old one goes only
 * when the fence behind it has signalled */
static void test_out_of_date(void)
{
   vulkan_output_t out;
   VkSwapchainKHR first;
   reset();
   CHECK(make(&out));
   first          = out.swapchain;
   gpu_done       = 0;
   acquire_result = VK_ERROR_OUT_OF_DATE_KHR;
   CHECK(vulkan_output_acquire(&out));
   CHECK(creates == 2);
   CHECK(old_swapchain == first);
   CHECK(out.swapchain != first);
   CHECK(empty_submits == 1);
   CHECK(!was_destroyed(first));
   CHECK(vulkan_output_present(&out));
   CHECK(vulkan_output_acquire(&out));
   CHECK(!was_destroyed(first));
   complete_gpu();
   CHECK(vulkan_output_present(&out));
   CHECK(vulkan_output_acquire(&out));
   CHECK(was_destroyed(first));
   CHECK(vulkan_output_present(&out));

   /* Out of date at present: remade there */
   present_result = VK_ERROR_OUT_OF_DATE_KHR;
   CHECK(vulkan_output_acquire(&out));
   CHECK(vulkan_output_present(&out));
   CHECK(creates == 3);
   present_result = VK_SUCCESS;
   CHECK(idle_waits == 0 && fence_waits == 0);

   CHECK(!vulkan_output_retire(&out));
   complete_gpu();
   CHECK(vulkan_output_retire(&out));
   all_gone();
}

/* Suboptimal: one rebuild, then kept until a present is optimal */
static void test_suboptimal(void)
{
   vulkan_output_t out;
   reset();
   CHECK(make(&out));
   present_result = VK_SUBOPTIMAL_KHR;
   CHECK(vulkan_output_acquire(&out));
   CHECK(vulkan_output_present(&out));
   CHECK(creates == 2);
   CHECK(vulkan_output_acquire(&out));
   CHECK(vulkan_output_present(&out));
   CHECK(creates == 2);
   present_result = VK_SUCCESS;
   CHECK(vulkan_output_acquire(&out));
   CHECK(vulkan_output_present(&out));
   present_result = VK_SUBOPTIMAL_KHR;
   CHECK(vulkan_output_acquire(&out));
   CHECK(vulkan_output_present(&out));
   CHECK(creates == 3);
   CHECK(vulkan_output_retire(&out));
   all_gone();
}

/* A lost surface is dropped, never waited on or remade */
static void test_lost(void)
{
   vulkan_output_t out;
   reset();
   CHECK(make(&out));
   acquire_result = VK_ERROR_SURFACE_LOST_KHR;
   CHECK(!vulkan_output_acquire(&out));
   CHECK(out.flags & VK_OUTPUT_FLAG_LOST);
   CHECK(!vulkan_output_acquire(&out));
   CHECK(creates == 1);
   CHECK(vulkan_output_retire(&out));
   all_gone();

   reset();
   CHECK(make(&out));
   present_result = VK_ERROR_SURFACE_LOST_KHR;
   CHECK(vulkan_output_acquire(&out));
   CHECK(!vulkan_output_present(&out));
   CHECK(out.flags & VK_OUTPUT_FLAG_LOST);
   CHECK(creates == 1);
   CHECK(vulkan_output_retire(&out));
   all_gone();
   CHECK(idle_waits == 0 && fence_waits == 0);
}

/* Retire never waits in a frame; finish waits, for teardown only */
static void test_retire(void)
{
   vulkan_output_t out;
   reset();
   CHECK(make(&out));
   gpu_done      = 0;
   acquire_ready = 0;
   CHECK(!vulkan_output_acquire(&out));
   CHECK(!vulkan_output_retire(&out));
   CHECK(live_swapchains == 1);
   release_images();
   CHECK(!vulkan_output_retire(&out));
   CHECK(live_swapchains == 1);
   complete_gpu();
   CHECK(vulkan_output_retire(&out));
   all_gone();
   CHECK(idle_waits == 0 && fence_waits == 0);

   reset();
   CHECK(make(&out));
   gpu_done      = 0;
   acquire_ready = 0;
   CHECK(!vulkan_output_acquire(&out));
   vulkan_output_finish(&out);
   CHECK(fence_waits > 0);
   CHECK(vulkan_output_retire(&out));
   all_gone();
   CHECK(idle_waits == 0);

   /* An acquire never answered: finish gives up on it after its bound,
    * and its fence is left rather than destroyed while still owed */
   reset();
   CHECK(make(&out));
   acquire_ready = 0;
   pe_never      = 1;
   CHECK(!vulkan_output_acquire(&out));
   vulkan_output_finish(&out);
   CHECK(out.flags & VK_OUTPUT_FLAG_FENCE_PENDING);
   CHECK(vulkan_output_retire(&out));
   CHECK(live_fences == 1);
   CHECK(live_surfaces == 0 && live_swapchains == 0);
}

int main(void)
{
   memset(&ctx, 0, sizeof(ctx));
   ctx.instance             = (VkInstance)&instance_obj;
   ctx.gpu                  = (VkPhysicalDevice)&gpu_obj;
   ctx.device               = (VkDevice)&device_obj;
   ctx.queue                = (VkQueue)&queue_obj;
   ctx.graphics_queue_index = 0;
#ifdef HAVE_THREADS
   ctx.queue_lock           = slock_new();
#endif
   fake_pass                = FAKE(VkRenderPass);
   install();

   test_create();
   test_refused();
   test_no_pass();
   test_hdr_owner_sdr_surface();
   test_hdr_owner_sdr_10bit();
   test_hdr_owner_no_8bit();
   test_hdr_owner_other_10bit();
   test_hdr_owner_sdr_space_and_encoding();
   test_hdr_surface_takes_owner_mode();
   test_scrgb_owner_hdr10_surface();
   test_surface_loses_hdr();
   test_owner_hdr_off_again();
   test_acquire();
   test_out_of_date();
   test_suboptimal();
   test_lost();
   test_retire();

#ifdef HAVE_THREADS
   slock_free(ctx.queue_lock);
#endif
   if (failures)
   {
      fprintf(stderr, "vulkan_output: %u failure(s)\n", failures);
      return 1;
   }
   printf("vulkan_output: ok\n");
   return 0;
}
