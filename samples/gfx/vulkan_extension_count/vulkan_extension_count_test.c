/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vulkan_extension_count_test.c).
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

/* Regression test for the heap-overflow fix in
 * gfx/common/vulkan_common.c::vulkan_find_device_extensions.
 *
 * Pre-fix the function appended the required-extension list to
 * the caller's `enabled` buffer twice -- once via memcpy at the
 * top of the body, then again in a per-element loop:
 *
 *   memcpy((void*)(enabled + count), exts, num_exts * sizeof(*exts));
 *   count += num_exts;
 *   for (i = 0; i < num_exts; i++)
 *      if (vulkan_find_extensions(&exts[i], 1, properties, ...))
 *         enabled[count++] = exts[i];                        // <-- duplicate
 *   for (i = 0; i < num_optional_exts; i++)
 *      if (vulkan_find_extensions(&optional_exts[i], 1, ...))
 *         enabled[count++] = optional_exts[i];
 *
 * Net writes worst-case: count_initial + 2*num_required +
 * num_optional.
 *
 * The caller in vulkan_context_create_device_wrapper sized its
 * malloc for count_initial + num_required + num_optional entries:
 *
 *   const char **device_extensions = malloc(
 *      (info.enabledExtensionCount
 *       + ARRAY_SIZE(vulkan_device_extensions)
 *       + ARRAY_SIZE(vulkan_optional_device_extensions))
 *      * sizeof(const char *));
 *
 * With current ARRAY_SIZE values (1 required, 3 optional) and a
 * GPU exposing all optional extensions, the second pass of writes
 * placed one extra string-pointer past the end of the malloc'd
 * block -- 8 bytes on a 64-bit host.  Reachable whenever a
 * libretro core uses the Vulkan HW context-negotiation interface
 * (iface->create_device / create_device2), which is the standard
 * path for cores using Vulkan HW rendering.
 *
 * The instance-side cousin vulkan_find_instance_extensions only
 * logs in its loop -- the divergence indicates the device-side
 * loop was a stale leftover from a refactor.
 *
 * The duplicate writes also produced duplicate entries in
 * ppEnabledExtensionNames passed to vkCreateDevice, which the
 * spec forbids (VUID-VkDeviceCreateInfo-
 * ppEnabledExtensionNames-01840).
 *
 * Fix: drop the per-element loop entirely; rely on the early
 * vulkan_find_extensions(exts, num_exts, ...) check at the top
 * of the function, which already validates that all required
 * extensions are present.
 *
 * IMPORTANT: this test keeps a verbatim copy of the post-fix
 * append-to-enabled[] block from vulkan_common.c.  If the
 * function amends, the copy below must follow.  Convention used
 * by archive_name_safety_test, http_method_match_test,
 * video_shader_wildcard_test, input_remap_bounds_test,
 * bsv_replay_bounds_test, bps_patch_bounds_test in
 * samples/tasks/.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* --- Mock the parts of the Vulkan extension-property API the
 *     function under test actually consults.  In production
 *     vulkan_find_extensions walks an array of
 *     VkExtensionProperties returned by
 *     vkEnumerateDeviceExtensionProperties; for the test we feed
 *     a flat array of strings and report which of the queried
 *     names match.  The test-side mock only needs to be
 *     consistent with how the post-fix code calls it. --- */

struct mock_ext_props {
   const char **names;
   unsigned     count;
};

static bool mock_find_extensions(const char * const *exts, unsigned num_exts,
      const struct mock_ext_props *props)
{
   unsigned i, j;
   for (i = 0; i < num_exts; i++)
   {
      bool found = false;
      for (j = 0; j < props->count; j++)
      {
         if (strcmp(exts[i], props->names[j]) == 0)
         {
            found = true;
            break;
         }
      }
      if (!found)
         return false;
   }
   return true;
}

/* === verbatim copy of the post-fix append block from
 *     gfx/common/vulkan_common.c::vulkan_find_device_extensions.
 *     The early "all required extensions present" check at the
 *     top of the function is folded into the caller's pre-flight
 *     here so the test exercises the same write pattern.  If
 *     vulkan_common.c amends this block, the copy must follow. === */
static bool find_device_extensions_postfix(
      const char **enabled, unsigned *inout_enabled_count,
      const char **exts, unsigned num_exts,
      const char **optional_exts, unsigned num_optional_exts,
      const struct mock_ext_props *props)
{
   unsigned i;
   unsigned count = *inout_enabled_count;

   if (!mock_find_extensions(exts, num_exts, props))
      return false;

   /* Required extensions: presence already validated by the
    * vulkan_find_extensions() check above. Append in one shot. */
   memcpy((void*)(enabled + count), exts, num_exts * sizeof(*exts));
   for (i = 0; i < num_exts; i++)
      (void)exts[i];  /* would be RARCH_DBG in production */
   count += num_exts;

   for (i = 0; i < num_optional_exts; i++)
   {
      if (mock_find_extensions(&optional_exts[i], 1, props))
         enabled[count++] = optional_exts[i];
   }

   *inout_enabled_count = count;
   return true;
}
/* === end verbatim copy === */

static int failures = 0;

/* Probe: caller sized for required + optional only; run the
 * post-fix append against it.  ASan instruments the malloc'd
 * region exactly, so a regression to the pre-fix double-write
 * trips heap-buffer-overflow.  Caller layout matches
 * vulkan_context_create_device_wrapper. */
static void test_create_device_wrapper_caller(void)
{
   /* Simulates: 1 required, 3 optional, GPU exposes everything,
    * info.enabledExtensionCount initially 0 (nothing pre-pushed
    * by the negotiation interface). */
   static const char *required[]  = { "VK_KHR_swapchain" };
   static const char *optional[]  = {
      "VK_KHR_sampler_mirror_clamp_to_edge",
      "VK_EXT_full_screen_exclusive",
      "VK_KHR_portability_subset"
   };
   static const char *gpu_exts[]  = {
      "VK_KHR_swapchain",
      "VK_KHR_sampler_mirror_clamp_to_edge",
      "VK_EXT_full_screen_exclusive",
      "VK_KHR_portability_subset",
      "VK_EXT_some_other_ext"  /* GPU has extras; must not be picked up */
   };
   const unsigned num_req  = sizeof(required) / sizeof(required[0]);
   const unsigned num_opt  = sizeof(optional) / sizeof(optional[0]);
   const unsigned cap      = 0 + num_req + num_opt;  /* caller's malloc */
   const char **enabled    = (const char **)malloc(cap * sizeof(const char *));
   unsigned count          = 0;
   struct mock_ext_props props = { gpu_exts, sizeof(gpu_exts)/sizeof(gpu_exts[0]) };
   unsigned i, expected;
   bool ok;

   if (!enabled)
   {
      printf("[ERROR] malloc failed\n");
      failures++;
      return;
   }

   ok = find_device_extensions_postfix(enabled, &count,
         required, num_req,
         optional, num_opt,
         &props);

   if (!ok)
   {
      printf("[ERROR] post-fix predicate rejected an all-supported GPU\n");
      failures++;
      free((void*)enabled);
      return;
   }

   /* Post-fix writes exactly num_req + num_opt slots when the GPU
    * exposes everything.  Pre-fix would have written 2*num_req +
    * num_opt -- one past the malloc'd region. */
   expected = num_req + num_opt;
   if (count != expected)
   {
      printf("[ERROR] count = %u, expected %u (pre-fix would be %u)\n",
            count, expected, 2 * num_req + num_opt);
      failures++;
      free((void*)enabled);
      return;
   }

   /* No duplicates -- the spec forbids them in
    * ppEnabledExtensionNames.  Pre-fix had VK_KHR_swapchain
    * appearing twice. */
   for (i = 0; i < count; i++)
   {
      unsigned j;
      for (j = i + 1; j < count; j++)
      {
         if (strcmp(enabled[i], enabled[j]) == 0)
         {
            printf("[ERROR] duplicate extension '%s' in enabled list\n",
                  enabled[i]);
            failures++;
            free((void*)enabled);
            return;
         }
      }
   }

   printf("[SUCCESS] create_device_wrapper caller: %u entries, no overflow, no duplicates\n",
         count);
   free((void*)enabled);
}

/* Probe: stack-buffer caller (vulkan_context_init_device uses
 * `const char *enabled_device_extensions[8]`).  Capacity 8 vs.
 * worst-case post-fix writes of num_req + num_opt = 4.  This
 * exercise documents the second call site -- the buffer fits
 * comfortably today, but only by a margin of 4. */
static void test_init_device_caller(void)
{
   static const char *required[]  = { "VK_KHR_swapchain" };
   static const char *optional[]  = {
      "VK_KHR_sampler_mirror_clamp_to_edge",
      "VK_EXT_full_screen_exclusive",
      "VK_KHR_portability_subset"
   };
   static const char *gpu_exts[]  = {
      "VK_KHR_swapchain",
      "VK_KHR_sampler_mirror_clamp_to_edge",
      "VK_EXT_full_screen_exclusive",
      "VK_KHR_portability_subset"
   };
   const unsigned num_req  = sizeof(required) / sizeof(required[0]);
   const unsigned num_opt  = sizeof(optional) / sizeof(optional[0]);
   /* Mirror the production stack array literally. */
   const char *enabled[8] = {0};
   unsigned count          = 0;
   struct mock_ext_props props = { gpu_exts, sizeof(gpu_exts)/sizeof(gpu_exts[0]) };
   bool ok;

   ok = find_device_extensions_postfix(enabled, &count,
         required, num_req,
         optional, num_opt,
         &props);

   if (!ok || count != num_req + num_opt)
   {
      printf("[ERROR] init_device caller: ok=%d count=%u expected=%u\n",
            (int)ok, count, num_req + num_opt);
      failures++;
      return;
   }

   if (count > sizeof(enabled) / sizeof(enabled[0]))
   {
      printf("[ERROR] init_device caller: count=%u > stack capacity 8\n",
            count);
      failures++;
      return;
   }

   printf("[SUCCESS] init_device caller: %u entries fits in stack[8]\n", count);
}

/* Probe: GPU exposes none of the optional extensions.  Post-fix
 * writes exactly num_req slots; the caller's malloc still fits
 * the optional set's worth of slack as required by the function
 * signature, but only the required ones are populated. */
static void test_no_optional_supported(void)
{
   static const char *required[]  = { "VK_KHR_swapchain" };
   static const char *optional[]  = {
      "VK_KHR_sampler_mirror_clamp_to_edge",
      "VK_EXT_full_screen_exclusive",
      "VK_KHR_portability_subset"
   };
   static const char *gpu_exts[]  = {
      "VK_KHR_swapchain"  /* required only, no optional */
   };
   const unsigned num_req  = 1;
   const unsigned num_opt  = 3;
   const unsigned cap      = num_req + num_opt;
   const char **enabled    = (const char **)malloc(cap * sizeof(const char *));
   unsigned count          = 0;
   struct mock_ext_props props = { gpu_exts, 1 };
   bool ok;

   if (!enabled) { printf("[ERROR] malloc\n"); failures++; return; }

   ok = find_device_extensions_postfix(enabled, &count,
         required, num_req, optional, num_opt, &props);

   if (!ok || count != num_req)
   {
      printf("[ERROR] no-optional case: ok=%d count=%u expected=%u\n",
            (int)ok, count, num_req);
      failures++;
   }
   else
      printf("[SUCCESS] no-optional supported: %u entries (required only)\n",
            count);

   free((void*)enabled);
}

/* Probe: GPU is missing a required extension.  Post-fix returns
 * false up front, no writes happen.  Caller gets to free
 * everything cleanly. */
static void test_required_missing_rejects(void)
{
   static const char *required[]  = { "VK_KHR_swapchain" };
   static const char *optional[]  = {
      "VK_KHR_sampler_mirror_clamp_to_edge"
   };
   static const char *gpu_exts[]  = {
      "VK_EXT_some_other_ext"  /* no swapchain support */
   };
   const unsigned cap      = 1 + 1;
   const char **enabled    = (const char **)malloc(cap * sizeof(const char *));
   unsigned count          = 0;
   struct mock_ext_props props = { gpu_exts, 1 };
   bool ok;

   if (!enabled) { printf("[ERROR] malloc\n"); failures++; return; }

   ok = find_device_extensions_postfix(enabled, &count,
         required, 1, optional, 1, &props);

   if (ok)
   {
      printf("[ERROR] missing required extension was accepted\n");
      failures++;
   }
   else if (count != 0)
   {
      printf("[ERROR] missing-required path mutated count to %u\n", count);
      failures++;
   }
   else
      printf("[SUCCESS] missing required extension rejected, count untouched\n");

   free((void*)enabled);
}

/* Probe: caller pre-pushes some entries via the negotiation
 * interface (info.enabledExtensionCount > 0).  Post-fix appends
 * starting from `count`, doesn't disturb pre-pushed entries. */
static void test_with_prepushed_entries(void)
{
   static const char *required[]  = { "VK_KHR_swapchain" };
   static const char *optional[]  = {
      "VK_KHR_sampler_mirror_clamp_to_edge",
      "VK_EXT_full_screen_exclusive"
   };
   static const char *gpu_exts[]  = {
      "VK_KHR_swapchain",
      "VK_KHR_sampler_mirror_clamp_to_edge",
      "VK_EXT_full_screen_exclusive"
   };
   const unsigned num_req  = 1;
   const unsigned num_opt  = 2;
   const unsigned prepushed = 2;
   const unsigned cap       = prepushed + num_req + num_opt;
   const char **enabled     = (const char **)malloc(cap * sizeof(const char *));
   unsigned count           = prepushed;
   struct mock_ext_props props = { gpu_exts, 3 };
   bool ok;

   if (!enabled) { printf("[ERROR] malloc\n"); failures++; return; }

   /* Pre-push two entries the way create_device_wrapper does. */
   enabled[0] = "VK_KHR_external_memory";
   enabled[1] = "VK_KHR_external_semaphore";

   ok = find_device_extensions_postfix(enabled, &count,
         required, num_req, optional, num_opt, &props);

   if (!ok || count != prepushed + num_req + num_opt)
   {
      printf("[ERROR] prepushed case: ok=%d count=%u expected=%u\n",
            (int)ok, count, prepushed + num_req + num_opt);
      failures++;
   }
   else if (   strcmp(enabled[0], "VK_KHR_external_memory") != 0
            || strcmp(enabled[1], "VK_KHR_external_semaphore") != 0)
   {
      printf("[ERROR] prepushed case: prepended entries clobbered\n");
      failures++;
   }
   else
      printf("[SUCCESS] prepushed entries preserved, %u entries total\n", count);

   free((void*)enabled);
}

int main(void)
{
   test_create_device_wrapper_caller();
   test_init_device_caller();
   test_no_optional_supported();
   test_required_missing_rejects();
   test_with_prepushed_entries();

   if (failures)
   {
      printf("\n%d vulkan_extension_count test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll vulkan_extension_count regression tests passed.\n");
   return 0;
}
