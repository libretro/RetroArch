/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vulkan_validation_test.c).
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

/* Runs RetroArch's real Vulkan context setup and teardown under
 * VK_LAYER_KHRONOS_validation and fails on any validation
 * error or warning.
 *
 * The other vulkan samples in this directory each pin down one
 * known defect.  This one is the opposite shape: it asserts a
 * property -- "gfx/common/vulkan_common.c provokes no
 * complaint from the validation layer" -- so it catches
 * regressions nobody has thought of yet.  Wrong queue family
 * indices, extensions used without being enabled, objects
 * outliving their parent, semaphores destroyed while pending:
 * all of it is invisible to a normal run on a forgiving driver
 * and all of it is a validation error here.
 *
 * HOW IT IS WIRED
 *
 * No debug messenger of our own.  vulkan_common.c already has
 * one: built with -DVULKAN_DEBUG it enables the validation layer
 * and VK_EXT_debug_utils and installs vulkan_debug_cb(), which
 * formats every message through RARCH_LOG.  The test supplies
 * that RARCH_LOG (see stubs_retroarch.c) and counts the lines.
 * So the instrumentation being exercised is the driver's own,
 * which is worth something on its own account -- if the debug
 * path stops reporting, this test stops passing.
 *
 * The context is brought up over a real Xlib surface, because
 * VULKAN_WSI_NONE reaches almost nothing:
 * vulkan_context_init_device() is called from
 * vulkan_surface_create(), not from vulkan_context_init(), and
 * vulkan_surface_create() answers VULKAN_WSI_NONE with `return
 * false`.  Init alone therefore leaves context.gpu NULL and
 * memory_properties zeroed -- a check written against it passes
 * by having nothing to check.
 *
 * With an Xvfb display and lavapipe the whole path runs
 * headless anyway:
 *
 *     apt-get install mesa-vulkan-drivers vulkan-validationlayers
 *     Xvfb :99 -screen 0 1280x720x24 &
 *     export DISPLAY=:99
 *     export VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json
 *
 * With no X display or no Vulkan device the test exits 77 (the
 * automake convention for "skipped") rather than failing, so it
 * is safe to run unconditionally.
 *
 * WHAT IT IS NOT
 *
 * gfx/drivers/vulkan.c proper is out of reach: it has 169
 * undefined frontend symbols, so standing it up would mean
 * stubbing most of the frontend, and its texture and descriptor
 * helpers are static besides.  What is covered here is
 * vulkan_common.c -- instance creation, physical device
 * selection, queue family selection, logical device creation,
 * the extension and layer negotiation around all of it, and the
 * teardown.  That is where the validation-visible mistakes in
 * the Vulkan backend have historically lived.
 *
 * THE HARNESS HAS TEETH
 *
 * Verified by deliberately breaking the driver: removing the
 * vkDestroyDebugUtilsMessengerEXT() call from
 * vulkan_context_destroy() makes the run fail with
 *
 *   ERROR Validation: [ VUID-vkDestroyInstance-instance-00629 ]
 *   ... VkDebugUtilsMessengerEXT 0x10000000001[] has not been
 *   destroyed ...
 *   errors=1
 *
 * so a leaked child object really does come back as a failure
 * rather than passing quietly.
 *
 * Build and run:
 *
 *     make
 *     VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json \
 *         ./vulkan_validation_test
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <X11/Xlib.h>

#include "../../../gfx/common/vulkan_common.h"

/* Scored by the RARCH_LOG hook in stubs_retroarch.c. */
extern int g_vk_validation_errors;
extern int g_vk_validation_warnings;

#define SKIP_EXIT_CODE 77

static Display *s_dpy;
static Window   s_win;

static void reset_counts(void)
{
   g_vk_validation_errors   = 0;
   g_vk_validation_warnings = 0;
}

static int report(const char *what)
{
   if (g_vk_validation_errors || g_vk_validation_warnings)
   {
      fprintf(stderr,
            "FAIL: %s produced %d validation error(s)"
            " and %d warning(s)\n",
            what, g_vk_validation_errors, g_vk_validation_warnings);
      return 1;
   }
   printf("[pass] %s is validation-clean\n", what);
   return 0;
}

/* Bring the whole thing up: instance, surface, physical device,
 * queues, logical device, swapchain.  Then take it down. */
static int context_up(gfx_ctx_vulkan_data_t *vk)
{
   memset(vk, 0, sizeof(*vk));

   if (!vulkan_context_init(vk, VULKAN_WSI_XLIB))
      return 0;

   if (!vulkan_surface_create(vk, VULKAN_WSI_XLIB,
            s_dpy, &s_win, VIDEO_SCALE_PACK(640, 480), 1))
   {
      vulkan_context_destroy(vk, true);
      return 0;
   }
   return 1;
}

static int test_context_init_destroy(int *skipped)
{
   gfx_ctx_vulkan_data_t vk;

   *skipped = 0;
   reset_counts();

   if (!context_up(&vk))
   {
      fputs("SKIP: could not bring up a Vulkan context"
            " (need a display and an ICD;"
            " see the header of this file)\n", stderr);
      *skipped = 1;
      return 0;
   }

   printf("       device: %s, %u memory type(s),"
          " %u swapchain image(s)\n",
         vk.context.gpu_properties.deviceName,
         vk.context.memory_properties.memoryTypeCount,
         vk.context.num_swapchain_images);

   /* Guard against the whole suite passing by having reached
    * nothing.  VULKAN_WSI_NONE used to leave exactly this state
    * and every check below still went green. */
   if (!vk.context.gpu)
   {
      fputs("FAIL: no physical device was selected\n", stderr);
      vulkan_context_destroy(&vk, true);
      return 1;
   }
   if (!vk.context.device)
   {
      fputs("FAIL: no logical device was created\n", stderr);
      vulkan_context_destroy(&vk, true);
      return 1;
   }
   if (!vk.context.memory_properties.memoryTypeCount)
   {
      fputs("FAIL: device reports no memory types\n", stderr);
      vulkan_context_destroy(&vk, true);
      return 1;
   }
   if (!vk.context.num_swapchain_images)
   {
      fputs("FAIL: swapchain has no images\n", stderr);
      vulkan_context_destroy(&vk, true);
      return 1;
   }

   vulkan_context_destroy(&vk, true);

   return report("context init/surface/device/swapchain/destroy");
}

/* Repeat it.  A leak that validation only notices at
 * vkDestroyInstance shows up the first time round; one that
 * needs state left over from a previous context -- a cached
 * device, a stale queue index -- only shows up on a later one.
 * RetroArch does this for real on every driver reinit. */
static int test_context_cycle(void)
{
   int i;

   for (i = 0; i < 3; i++)
   {
      gfx_ctx_vulkan_data_t vk;

      reset_counts();

      if (!context_up(&vk))
      {
         fprintf(stderr, "FAIL: context setup failed on cycle %d\n", i);
         return 1;
      }
      vulkan_context_destroy(&vk, true);

      if (g_vk_validation_errors || g_vk_validation_warnings)
      {
         fprintf(stderr,
               "FAIL: cycle %d produced %d validation error(s)"
               " and %d warning(s)\n",
               i, g_vk_validation_errors, g_vk_validation_warnings);
         return 1;
      }
   }

   printf("[pass] three setup/teardown cycles are validation-clean\n");
   return 0;
}

/* The validation layer retires a present's semaphore waits the moment
 * vkQueuePresentKHR returns - it has no fence to learn otherwise from
 * - so a semaphore destroyed under a pending present passes it clean.
 * This tracker sits on the symbol wrapper's function pointers and
 * keeps the one fact the layer drops: a present's wait, and the
 * swapchain it was made against, stay pending on that queue until a
 * fence submitted to the queue after the present has been waited on,
 * or the queue or device has been drained. Destroying either while it
 * is pending is the hazard. */
#define TRACK_MAX 64

struct track_present
{
   VkQueue        queue;
   VkSemaphore    sem;        /* VK_NULL_HANDLE for a swapchain entry */
   VkSwapchainKHR swapchain;  /* VK_NULL_HANDLE for a semaphore entry */
   unsigned       seq;        /* present count on queue when queued */
};

struct track_fence
{
   VkQueue queue;
   VkFence fence;
   unsigned seq;              /* present count on queue when submitted */
};

static struct track_present s_pending[TRACK_MAX];
static unsigned s_num_pending;
static struct track_fence s_fences[TRACK_MAX];
static unsigned s_num_fences;
static VkQueue  s_seq_queue[8];
static unsigned s_seq_count[8];
static unsigned s_num_queues;
static int      s_hazards;

static PFN_vkQueuePresentKHR      s_real_present;
static PFN_vkQueueSubmit          s_real_submit;
static PFN_vkWaitForFences        s_real_wait;
static PFN_vkQueueWaitIdle        s_real_queue_idle;
static PFN_vkDeviceWaitIdle       s_real_device_idle;
static PFN_vkDestroySemaphore     s_real_destroy_sem;
static PFN_vkDestroySwapchainKHR  s_real_destroy_swapchain;

static unsigned *track_seq(VkQueue queue)
{
   unsigned i;
   for (i = 0; i < s_num_queues; i++)
      if (s_seq_queue[i] == queue)
         return &s_seq_count[i];
   if (s_num_queues < 8)
   {
      s_seq_queue[s_num_queues] = queue;
      s_seq_count[s_num_queues] = 0;
      return &s_seq_count[s_num_queues++];
   }
   return &s_seq_count[0];
}

static void track_retire(VkQueue queue, unsigned upto)
{
   unsigned i = 0;
   while (i < s_num_pending)
   {
      if (     (queue == VK_NULL_HANDLE || s_pending[i].queue == queue)
            && s_pending[i].seq <= upto)
         s_pending[i] = s_pending[--s_num_pending];
      else
         i++;
   }
}

static VKAPI_ATTR VkResult VKAPI_CALL track_present_khr(VkQueue queue,
      const VkPresentInfoKHR *info)
{
   VkResult res     = s_real_present(queue, info);
   unsigned *seq    = track_seq(queue);
   unsigned i;
   (*seq)++;
   for (i = 0; i < info->waitSemaphoreCount && s_num_pending < TRACK_MAX; i++)
   {
      s_pending[s_num_pending].queue     = queue;
      s_pending[s_num_pending].sem       = info->pWaitSemaphores[i];
      s_pending[s_num_pending].swapchain = VK_NULL_HANDLE;
      s_pending[s_num_pending].seq       = *seq;
      s_num_pending++;
   }
   for (i = 0; i < info->swapchainCount && s_num_pending < TRACK_MAX; i++)
   {
      s_pending[s_num_pending].queue     = queue;
      s_pending[s_num_pending].sem       = VK_NULL_HANDLE;
      s_pending[s_num_pending].swapchain = info->pSwapchains[i];
      s_pending[s_num_pending].seq       = *seq;
      s_num_pending++;
   }
   return res;
}

static VKAPI_ATTR VkResult VKAPI_CALL track_submit(VkQueue queue,
      uint32_t count, const VkSubmitInfo *submits, VkFence fence)
{
   VkResult res = s_real_submit(queue, count, submits, fence);
   if (fence != VK_NULL_HANDLE && res == VK_SUCCESS)
   {
      unsigned i;
      for (i = 0; i < s_num_fences; i++)
         if (s_fences[i].fence == fence)
            break;
      if (i == s_num_fences && s_num_fences < TRACK_MAX)
         s_num_fences++;
      if (i < TRACK_MAX)
      {
         s_fences[i].queue = queue;
         s_fences[i].fence = fence;
         s_fences[i].seq   = *track_seq(queue);
      }
   }
   return res;
}

static VKAPI_ATTR VkResult VKAPI_CALL track_wait(VkDevice device,
      uint32_t count, const VkFence *fences, VkBool32 wait_all,
      uint64_t timeout)
{
   VkResult res = s_real_wait(device, count, fences, wait_all, timeout);
   if (res == VK_SUCCESS && wait_all)
   {
      unsigned i, j;
      for (i = 0; i < count; i++)
         for (j = 0; j < s_num_fences; j++)
            if (s_fences[j].fence == fences[i])
               track_retire(s_fences[j].queue, s_fences[j].seq);
   }
   return res;
}

static VKAPI_ATTR VkResult VKAPI_CALL track_queue_idle(VkQueue queue)
{
   VkResult res = s_real_queue_idle(queue);
   if (res == VK_SUCCESS)
      track_retire(queue, ~0u);
   return res;
}

static VKAPI_ATTR VkResult VKAPI_CALL track_device_idle(VkDevice device)
{
   VkResult res = s_real_device_idle(device);
   if (res == VK_SUCCESS)
      track_retire(VK_NULL_HANDLE, ~0u);
   return res;
}

static VKAPI_ATTR void VKAPI_CALL track_destroy_sem(VkDevice device,
      VkSemaphore sem, const VkAllocationCallbacks *alloc)
{
   unsigned i;
   for (i = 0; i < s_num_pending; i++)
      if (s_pending[i].sem == sem && sem != VK_NULL_HANDLE)
      {
         fprintf(stderr, "HAZARD: semaphore destroyed while a present"
               " queued behind it is not known to have completed\n");
         s_hazards++;
      }
   s_real_destroy_sem(device, sem, alloc);
}

static VKAPI_ATTR void VKAPI_CALL track_destroy_swapchain(VkDevice device,
      VkSwapchainKHR swapchain, const VkAllocationCallbacks *alloc)
{
   unsigned i;
   for (i = 0; i < s_num_pending; i++)
      if (s_pending[i].swapchain == swapchain && swapchain != VK_NULL_HANDLE)
      {
         fprintf(stderr, "HAZARD: swapchain destroyed while a present"
               " into it is not known to have completed\n");
         s_hazards++;
      }
   s_real_destroy_swapchain(device, swapchain, alloc);
}

static void track_install(void)
{
   s_num_pending = s_num_fences = s_num_queues = 0;
   s_hazards     = 0;
   s_real_present           = vkQueuePresentKHR;
   s_real_submit            = vkQueueSubmit;
   s_real_wait              = vkWaitForFences;
   s_real_queue_idle        = vkQueueWaitIdle;
   s_real_device_idle       = vkDeviceWaitIdle;
   s_real_destroy_sem       = vkDestroySemaphore;
   s_real_destroy_swapchain = vkDestroySwapchainKHR;
   vkQueuePresentKHR        = track_present_khr;
   vkQueueSubmit            = track_submit;
   vkWaitForFences          = track_wait;
   vkQueueWaitIdle          = track_queue_idle;
   vkDeviceWaitIdle         = track_device_idle;
   vkDestroySemaphore       = track_destroy_sem;
   vkDestroySwapchainKHR    = track_destroy_swapchain;
}

static void track_remove(void)
{
   vkQueuePresentKHR        = s_real_present;
   vkQueueSubmit            = s_real_submit;
   vkWaitForFences          = s_real_wait;
   vkQueueWaitIdle          = s_real_queue_idle;
   vkDeviceWaitIdle         = s_real_device_idle;
   vkDestroySemaphore       = s_real_destroy_sem;
   vkDestroySwapchainKHR    = s_real_destroy_swapchain;
}

/* A present is a queue operation: vkQueuePresentKHR returns while
 * the presentation engine still has to wait on the frame's swapchain
 * semaphore, and nothing on the frame fences covers that wait - the
 * fences are signalled by the render submission, which the present
 * comes after. A swapchain rebuild or teardown that waited on the
 * frame fences alone then destroyed the semaphore, and the swapchain
 * it belongs to, under a pending present. On Mali (Android, issue
 * #19601) that broke the driver's presentation sync for good: every
 * present after it failed, the swapchain was rebuilt every frame and
 * the menu froze. The validation layer cannot see it (see the tracker
 * above), so the tracker is the oracle: no semaphore or swapchain a
 * present is queued against is destroyed before a fence behind that
 * present has been waited on.
 *
 * The sequence is the driver's: acquire (done by the surface create),
 * a fenced submission signalling the swapchain semaphore, the present,
 * and a teardown right behind it. */
static int test_present_then_teardown(void)
{
   gfx_ctx_vulkan_data_t vk;
   VkSubmitInfo submit;
   VkCommandPoolCreateInfo pool_info;
   VkCommandBufferAllocateInfo cmd_info;
   VkCommandBufferBeginInfo begin;
   VkImageMemoryBarrier barrier;
   VkCommandPool pool = VK_NULL_HANDLE;
   VkCommandBuffer cmd = VK_NULL_HANDLE;
   VkSemaphore wait_sems[VULKAN_MAX_SWAPCHAIN_IMAGES + 1];
   VkPipelineStageFlags wait_stages[VULKAN_MAX_SWAPCHAIN_IMAGES + 1];
   unsigned index;
   unsigned frame;

   reset_counts();

   if (!context_up(&vk))
   {
      fputs("FAIL: context setup failed for the present test\n", stderr);
      return 1;
   }
   /* The pointers are loaded by the context init above. */
   track_install();

   if (!(vk.context.flags & VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN))
   {
      fputs("FAIL: surface create did not acquire an image\n", stderr);
      vulkan_context_destroy(&vk, true);
      return 1;
   }

   index = vk.context.current_swapchain_index;
   frame = vk.context.current_frame_index;

   if (     vk.context.swapchain_semaphores[index] == VK_NULL_HANDLE
         || vk.context.swapchain_fences[frame]     == VK_NULL_HANDLE)
   {
      fputs("FAIL: acquire left no swapchain semaphore or frame fence\n",
            stderr);
      vulkan_context_destroy(&vk, true);
      return 1;
   }

   /* The one command the frame needs: the acquired image into the
    * layout a present takes. */
   memset(&pool_info, 0, sizeof(pool_info));
   pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pool_info.queueFamilyIndex = vk.context.graphics_queue_index;
   vkCreateCommandPool(vk.context.device, &pool_info, NULL, &pool);

   memset(&cmd_info, 0, sizeof(cmd_info));
   cmd_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   cmd_info.commandPool        = pool;
   cmd_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   cmd_info.commandBufferCount = 1;
   vkAllocateCommandBuffers(vk.context.device, &cmd_info, &cmd);

   memset(&begin, 0, sizeof(begin));
   begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   vkBeginCommandBuffer(cmd, &begin);

   memset(&barrier, 0, sizeof(barrier));
   barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   barrier.dstAccessMask                   = VK_ACCESS_MEMORY_READ_BIT;
   barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
   barrier.newLayout                       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
   barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   barrier.image                           = vk.context.swapchain_images[index];
   barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   barrier.subresourceRange.levelCount     = 1;
   barrier.subresourceRange.layerCount     = 1;
   vkCmdPipelineBarrier(cmd,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
         0, 0, NULL, 0, NULL, 1, &barrier);
   vkEndCommandBuffer(cmd);

   memset(&submit, 0, sizeof(submit));
   submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   submit.commandBufferCount   = 1;
   submit.pCommandBuffers      = &cmd;
   submit.waitSemaphoreCount   = vulkan_context_take_acquire_waits(
         &vk.context, frame, wait_sems, wait_stages,
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);
   submit.pWaitSemaphores      = wait_sems;
   submit.pWaitDstStageMask    = wait_stages;
   submit.signalSemaphoreCount = 1;
   submit.pSignalSemaphores    = &vk.context.swapchain_semaphores[index];

   vkQueueSubmit(vk.context.queue, 1, &submit,
         vk.context.swapchain_fences[frame]);
   vk.context.swapchain_fences_signalled[frame] = true;

   vk.context.flags &= ~VK_CTX_FLAG_HAS_ACQUIRED_SWAPCHAIN;
   vulkan_present(&vk, index);

   /* The teardown right behind the present: the same path a resize
    * or an out-of-date acquire takes through vulkan_destroy_swapchain. */
   vulkan_surface_destroy(&vk);

   /* The context teardown drains the device; the pool goes after
    * that, once its command buffer can no longer be running. */
   vkDeviceWaitIdle(vk.context.device);
   vkDestroyCommandPool(vk.context.device, pool, NULL);
   vulkan_context_destroy(&vk, true);
   track_remove();

   if (s_hazards)
   {
      fprintf(stderr, "FAIL: a teardown right behind a present destroyed"
            " %d object(s) the present still needed\n", s_hazards);
      return 1;
   }
   return report("a teardown right behind a present");
}

/* vulkan_find_memory_type() picks the heap for every allocation
 * the backend makes; a wrong answer is a validation error at the
 * first vkBindImageMemory rather than here, so check the
 * contract directly. */
static int test_memory_type_selection(void)
{
   gfx_ctx_vulkan_data_t vk;
   uint32_t i, checked = 0;
   int fail = 0;

   reset_counts();

   if (!context_up(&vk))
   {
      fputs("FAIL: context setup failed\n", stderr);
      return 1;
   }

   for (i = 0; i < vk.context.memory_properties.memoryTypeCount; i++)
   {
      const VkMemoryPropertyFlags want =
         vk.context.memory_properties.memoryTypes[i].propertyFlags;
      uint32_t got;

      if (!want)
         continue;

      checked++;
      got = vulkan_find_memory_type(&vk.context.memory_properties,
            1u << i, want);

      if (got >= vk.context.memory_properties.memoryTypeCount)
      {
         fprintf(stderr,
               "FAIL: memory type %u advertises 0x%x but"
               " vulkan_find_memory_type() could not match it\n",
               i, (unsigned)want);
         fail = 1;
         break;
      }
      if (!(vk.context.memory_properties.memoryTypes[got].propertyFlags
               & want))
      {
         fprintf(stderr,
               "FAIL: asked for 0x%x, got type %u with 0x%x\n",
               (unsigned)want, got,
               (unsigned)vk.context.memory_properties
                  .memoryTypes[got].propertyFlags);
         fail = 1;
         break;
      }
   }

   vulkan_context_destroy(&vk, true);

   if (fail)
      return 1;
   if (!checked)
   {
      fputs("FAIL: no memory types to check\n", stderr);
      return 1;
   }

   printf("[pass] all %u advertised memory type(s) are selectable\n",
         checked);
   return 0;
}

int main(void)
{
   int skipped = 0;
   int ret     = 0;

   if (!(s_dpy = XOpenDisplay(NULL)))
   {
      fputs("SKIP: no X display (try Xvfb; see the header"
            " of this file)\n", stderr);
      return SKIP_EXIT_CODE;
   }
   s_win = XCreateSimpleWindow(s_dpy, DefaultRootWindow(s_dpy),
         0, 0, 640, 480, 0, 0, 0);
   XMapWindow(s_dpy, s_win);
   XSync(s_dpy, False);

   if (test_context_init_destroy(&skipped))
      ret = 1;
   else if (skipped)
      ret = SKIP_EXIT_CODE;
   else if (test_context_cycle())
      ret = 1;
   else if (test_memory_type_selection())
      ret = 1;
   else if (test_present_then_teardown())
      ret = 1;

   XDestroyWindow(s_dpy, s_win);
   XCloseDisplay(s_dpy);

   if (ret == 0)
      puts("ALL OK");
   return ret;
}
