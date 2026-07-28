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
            s_dpy, &s_win, 640, 480, 1))
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

   XDestroyWindow(s_dpy, s_win);
   XCloseDisplay(s_dpy);

   if (ret == 0)
      puts("ALL OK");
   return ret;
}
