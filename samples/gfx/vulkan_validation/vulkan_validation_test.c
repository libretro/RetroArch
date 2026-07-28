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
 * The context is brought up with VULKAN_WSI_NONE, so no window,
 * surface or swapchain is needed and it runs headless.  On CI
 * that means lavapipe:
 *
 *     apt-get install mesa-vulkan-drivers vulkan-validationlayers
 *     export VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.json
 *
 * With no Vulkan device at all the test exits 77 (the automake
 * convention for "skipped") rather than failing, so it is safe
 * to run unconditionally.
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

#include "../../../gfx/common/vulkan_common.h"

/* Scored by the RARCH_LOG hook in stubs_retroarch.c. */
extern int g_vk_validation_errors;
extern int g_vk_validation_warnings;

#define SKIP_EXIT_CODE 77

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

/* Bring the context up and take it down again.  Everything
 * vulkan_common.c does before a surface exists happens here. */
static int test_context_init_destroy(int *skipped)
{
   gfx_ctx_vulkan_data_t vk;

   *skipped = 0;
   reset_counts();
   memset(&vk, 0, sizeof(vk));

   if (!vulkan_context_init(&vk, VULKAN_WSI_NONE))
   {
      fputs("SKIP: no usable Vulkan device"
            " (set VK_DRIVER_FILES for a software ICD)\n", stderr);
      *skipped = 1;
      return 0;
    }

   /* Not printing vk.context.gpu_properties here on purpose: under
    * VULKAN_WSI_NONE it reads back zeroed even though
    * vulkan_context_init_device() fills it unconditionally two lines
    * before it fills memory_properties, which does come through.  That
    * is worth a look on its own and is not this test's business. */

   vulkan_context_destroy(&vk, false);

   return report("context init/destroy");
}

/* Repeat it.  A leak that validation only notices at
 * vkDestroyInstance shows up the first time round; one that
 * needs state left over from a previous context -- a cached
 * device, a stale queue index -- only shows up on the second.
 * RetroArch does this for real on every driver reinit. */
static int test_context_cycle(void)
{
   int i;

   for (i = 0; i < 3; i++)
   {
      gfx_ctx_vulkan_data_t vk;

      reset_counts();
      memset(&vk, 0, sizeof(vk));

      if (!vulkan_context_init(&vk, VULKAN_WSI_NONE))
      {
         fprintf(stderr, "FAIL: context init failed on cycle %d\n", i);
         return 1;
      }
      vulkan_context_destroy(&vk, false);

      if (g_vk_validation_errors || g_vk_validation_warnings)
      {
         fprintf(stderr,
               "FAIL: cycle %d produced %d validation error(s)"
               " and %d warning(s)\n",
               i, g_vk_validation_errors, g_vk_validation_warnings);
         return 1;
      }
   }

   printf("[pass] three init/destroy cycles are validation-clean\n");
   return 0;
}

/* vulkan_find_memory_type() picks the heap for every allocation
 * the backend makes; a wrong answer is a validation error at the
 * first vkBindImageMemory rather than here, so check the
 * contract directly.  The fallback form must never return the
 * "nothing matched" sentinel when the primary form would have
 * succeeded. */
static int test_memory_type_selection(void)
{
   gfx_ctx_vulkan_data_t vk;
   uint32_t i;
   int fail = 0;

   reset_counts();
   memset(&vk, 0, sizeof(vk));

   if (!vulkan_context_init(&vk, VULKAN_WSI_NONE))
   {
      fputs("FAIL: context init failed\n", stderr);
      return 1;
   }

   for (i = 0; i < vk.context.memory_properties.memoryTypeCount; i++)
   {
      const VkMemoryPropertyFlags want =
         vk.context.memory_properties.memoryTypes[i].propertyFlags;
      uint32_t got;

      if (!want)
         continue;

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

   vulkan_context_destroy(&vk, false);

   if (fail)
      return 1;

   printf("[pass] every advertised memory type is selectable\n");
   return 0;
}

int main(void)
{
   int skipped = 0;

   if (test_context_init_destroy(&skipped))
      return 1;
   if (skipped)
      return SKIP_EXIT_CODE;

   if (test_context_cycle())
      return 1;
   if (test_memory_type_selection())
      return 1;

   puts("ALL OK");
   return 0;
}
