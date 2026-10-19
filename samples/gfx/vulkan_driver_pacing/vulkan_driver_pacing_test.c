/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vulkan_driver_pacing_test.c).
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

/* The Driver Pacing backend in gfx/common/vulkan_common.c, driven
 * through fake VK_NV_low_latency2 entry points: what the mode and
 * sleep state machine does without a GPU. The device setup is not
 * run (it needs a loader); the state it would leave is filled in by
 * hand, the way vulkan_low_latency_device_init() leaves it. */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../../../gfx/common/vulkan_common.h"
#include "../../../gfx/video_driver.h"

static unsigned failures;

static void check(bool ok, const char *what)
{
   if (ok)
      return;
   failures++;
   printf("   FAIL: %s\n", what);
}

/* --- the fake driver --------------------------------------------- */

#define MAX_EVENTS 64
enum { EV_MODE, EV_SLEEP, EV_WAIT, EV_MARK };
struct event { int kind; uint64_t a; unsigned b; unsigned c; };
static struct event events[MAX_EVENTS];
static unsigned num_events;
static VkResult mode_result  = VK_SUCCESS;
static VkResult sleep_result = VK_SUCCESS;
static VkResult wait_result  = VK_SUCCESS;
static uint64_t last_wait_timeout;

static void ev(int kind, uint64_t a, unsigned b, unsigned c)
{
   if (num_events < MAX_EVENTS)
   {
      events[num_events].kind = kind;
      events[num_events].a    = a;
      events[num_events].b    = b;
      events[num_events].c    = c;
   }
   num_events++;
}

static unsigned count(int kind)
{
   unsigned i, n = 0;
   for (i = 0; i < num_events && i < MAX_EVENTS; i++)
      if (events[i].kind == kind)
         n++;
   return n;
}

static const struct event *last(int kind)
{
   unsigned i;
   for (i = num_events < MAX_EVENTS ? num_events : MAX_EVENTS; i > 0; i--)
      if (events[i - 1].kind == kind)
         return &events[i - 1];
   return NULL;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_set_sleep_mode(VkDevice device,
      VkSwapchainKHR swapchain, const VkLatencySleepModeInfoNV *info)
{
   (void)device;
   ev(EV_MODE, (uint64_t)(uintptr_t)swapchain,
         info ? (info->lowLatencyMode | (info->lowLatencyBoost << 1)) : 0xff,
         info ? info->minimumIntervalUs : 0);
   return mode_result;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_sleep(VkDevice device,
      VkSwapchainKHR swapchain, const VkLatencySleepInfoNV *info)
{
   (void)device;
   ev(EV_SLEEP, info->value, (unsigned)(uintptr_t)swapchain, 0);
   return sleep_result;
}

static VKAPI_ATTR void VKAPI_CALL fake_marker(VkDevice device,
      VkSwapchainKHR swapchain, const VkSetLatencyMarkerInfoNV *info)
{
   (void)device;
   ev(EV_MARK, info->presentID, (unsigned)info->marker,
         (unsigned)(uintptr_t)swapchain);
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_wait(VkDevice device,
      const VkSemaphoreWaitInfo *info, uint64_t timeout)
{
   (void)device;
   last_wait_timeout = timeout;
   ev(EV_WAIT, info->pValues[0], info->semaphoreCount, 0);
   return wait_result;
}

static gfx_ctx_vulkan_data_t vk;

static void reset(bool supported, uintptr_t swapchain)
{
   memset(&vk, 0, sizeof(vk));
   vk.context.device             = (VkDevice)(uintptr_t)0x10;
   vk.swapchain                  = (VkSwapchainKHR)swapchain;
   vk.low_latency.supported      = supported;
   vk.low_latency.set_sleep_mode = (PFN_vkVoidFunction)fake_set_sleep_mode;
   vk.low_latency.sleep          = (PFN_vkVoidFunction)fake_sleep;
   vk.low_latency.set_marker     = (PFN_vkVoidFunction)fake_marker;
   vk.low_latency.wait_semaphores= (PFN_vkVoidFunction)fake_wait;
   vk.low_latency.semaphore      = (VkSemaphore)(uintptr_t)0x20;
   num_events   = 0;
   mode_result  = sleep_result = wait_result = VK_SUCCESS;
}

/* --- the tests ---------------------------------------------------- */

static void test_capability(void)
{
   reset(false, 0x100);
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_DEVICE,
         "no extension: the device reason");
   check(vulkan_low_latency_sleep(&vk, 0, 20000) == 0,
         "no extension: no release");
   vulkan_low_latency_marker(&vk, 1, VK_LATENCY_MARKER_INPUT_SAMPLE_NV);
   check(num_events == 0, "no extension: nothing called");

   reset(true, 0);
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_SWAPCHAIN,
         "no swapchain yet: the swapchain reason");
   check(num_events == 0, "no swapchain: nothing called");
   vk.context.swapchain_external = true;
   vk.swapchain = (VkSwapchainKHR)(uintptr_t)0x100;
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_SWAPCHAIN,
         "external swapchain (a headset's): the swapchain reason");
   printf("   capability: device, swapchain and external gates\n");
}

static void test_mode(void)
{
   const struct event *e;
   reset(true, 0x100);
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_NONE, "on: in effect");
   e = last(EV_MODE);
   check(e && e->a == 0x100 && e->b == 1 && e->c == 0,
         "on: low latency mode set on the swapchain, no boost, no interval");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_NONE && count(EV_MODE) == 1,
         "on again: nothing set again");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_BOOST)
         == VIDEO_DRIVER_PACING_REASON_NONE, "boost: in effect");
   e = last(EV_MODE);
   check(count(EV_MODE) == 2 && e && e->b == 3, "boost: set with the boost bit");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_OFF)
         == VIDEO_DRIVER_PACING_REASON_OFF, "off: the off reason");
   e = last(EV_MODE);
   check(count(EV_MODE) == 3 && e && e->b == 0xff, "off: the mode taken off (NULL)");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_OFF)
         == VIDEO_DRIVER_PACING_REASON_OFF && count(EV_MODE) == 3,
         "off again: nothing called");
   check(vulkan_low_latency_sleep(&vk, 0, 20000) == 0 && count(EV_SLEEP) == 0,
         "off: no release");
   printf("   mode: set once per change, taken off once, boost carried\n");
}

static void test_release(void)
{
   const struct event *e;
   uint64_t id;
   reset(true, 0x100);
   vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON);
   id = vulkan_low_latency_sleep(&vk, 0, 20000);
   check(id == 1, "first release: frame 1");
   check(count(EV_SLEEP) == 1 && count(EV_WAIT) == 1,
         "first release: one sleep, one wait");
   e = last(EV_SLEEP);
   check(e && e->a == 1, "first release: the sleep signals value 1");
   e = last(EV_WAIT);
   check(e && e->a == 1 && e->b == 1, "first release: the wait is for value 1");
   check(last_wait_timeout == 20000ull * 1000ull,
         "the wait is bounded by what the runloop allows, in ns");
   id = vulkan_low_latency_sleep(&vk, 0, 20000);
   e = last(EV_SLEEP);
   check(id == 2 && e && e->a == 2, "second release: frame 2, value 2");
   check(count(EV_MODE) == 1, "no interval change: the mode is not set again");

   id = vulkan_low_latency_sleep(&vk, 16667, 50000);
   e = last(EV_MODE);
   check(id == 3 && count(EV_MODE) == 2 && e && e->c == 16667 && e->b == 1,
         "a timer period: the minimum interval goes to the driver before the sleep");
   check(last(EV_SLEEP)->a == 3, "and the sleep follows");
   id = vulkan_low_latency_sleep(&vk, 16667, 50000);
   check(id == 4 && count(EV_MODE) == 2, "same period: not set again");
   id = vulkan_low_latency_sleep(&vk, 0, 50000);
   e = last(EV_MODE);
   check(id == 5 && count(EV_MODE) == 3 && e && e->c == 0,
         "the timer stands down: the interval goes back to none");
   printf("   release: one sleep and one bounded wait a frame, the interval "
          "set only when it changes\n");
}

static void test_markers(void)
{
   const struct event *e;
   reset(true, 0x100);
   vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON);
   vulkan_low_latency_render_begin(&vk);
   check(count(EV_MARK) == 0, "no frame released yet: no render markers");
   vulkan_low_latency_sleep(&vk, 0, 20000);
   vulkan_low_latency_marker(&vk, 1, VK_LATENCY_MARKER_SIMULATION_START_NV);
   vulkan_low_latency_marker(&vk, 1, VK_LATENCY_MARKER_INPUT_SAMPLE_NV);
   vulkan_low_latency_render_begin(&vk);
   vulkan_low_latency_render_begin(&vk);
   check(count(EV_MARK) == 4, "sim start, input, sim end, render start: once each");
   /* After the mode set, the sleep and its wait */
   check(events[3].kind == EV_MARK && events[3].a == 1
         && events[3].b == VK_LATENCY_MARKER_SIMULATION_START_NV
         && events[4].b == VK_LATENCY_MARKER_INPUT_SAMPLE_NV
         && events[5].b == VK_LATENCY_MARKER_SIMULATION_END_NV
         && events[6].b == VK_LATENCY_MARKER_RENDERSUBMIT_START_NV,
         "markers in order, on frame 1");
   check(vk.low_latency.stage == 1, "the frame is at its render stage");
   vulkan_low_latency_marker(&vk, 0, VK_LATENCY_MARKER_INPUT_SAMPLE_NV);
   check(count(EV_MARK) == 4, "frame 0 is no frame: no marker");
   vulkan_low_latency_sleep(&vk, 0, 20000);
   check(vk.low_latency.stage == 0, "a new release starts the stages over");
   vulkan_low_latency_render_begin(&vk);
   e = last(EV_MARK);
   check(count(EV_MARK) == 6 && e && e->a == 2,
         "the next frame's render markers name frame 2");
   printf("   markers: input and simulation from the runloop, render once "
          "per released frame, nothing for no frame\n");
}

static void test_generation(void)
{
   reset(true, 0x100);
   vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON);
   vulkan_low_latency_sleep(&vk, 0, 20000);
   /* The swapchain is replaced; the context records a new one with
    * no mode, as vulkan_create_swapchain() does, even when the handle
    * value comes back the same. */
   vk.low_latency.swapchain = VK_NULL_HANDLE;
   vk.low_latency.mode      = 0;
   check(vulkan_low_latency_sleep(&vk, 0, 20000) == 0 && count(EV_SLEEP) == 1,
         "a replaced swapchain: no release until the mode is set on it");
   vulkan_low_latency_marker(&vk, 1, VK_LATENCY_MARKER_INPUT_SAMPLE_NV);
   check(count(EV_MARK) == 0, "a replaced swapchain: no marker names it");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_NONE && count(EV_MODE) == 2,
         "asked again: the mode is set on the new swapchain");
   check(vulkan_low_latency_sleep(&vk, 0, 20000) == 2,
         "and releases resume with the next frame number");

   /* A different handle, the mode still recorded on the old one */
   vk.swapchain = (VkSwapchainKHR)(uintptr_t)0x200;
   check(vulkan_low_latency_sleep(&vk, 0, 20000) == 0,
         "another swapchain: the old generation never gets a sleep");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_NONE && last(EV_MODE)->a == 0x200,
         "asked again: set on the new handle");
   printf("   generation: nothing names a swapchain the mode was not set on\n");
}

static void test_fallback(void)
{
   reset(true, 0x100);
   mode_result = VK_ERROR_UNKNOWN;
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_FAILED, "a failed mode set: failed");
   mode_result = VK_SUCCESS;
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_FAILED && count(EV_MODE) == 1,
         "failed latches: not asked again");
   check(vulkan_low_latency_sleep(&vk, 0, 20000) == 0 && count(EV_SLEEP) == 0,
         "failed: no release");

   reset(true, 0x100);
   vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON);
   wait_result = VK_TIMEOUT;
   check(vulkan_low_latency_sleep(&vk, 0, 20000) == 0,
         "a signal that never comes: no release, within the bound");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_FAILED,
         "and the backend reports failed from then on");
   wait_result = VK_SUCCESS;
   check(vulkan_low_latency_sleep(&vk, 0, 20000) == 0 && count(EV_SLEEP) == 1,
         "no second sleep on a semaphore the driver still owes a signal");

   reset(true, 0x100);
   vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON);
   sleep_result = VK_ERROR_DEVICE_LOST;
   check(vulkan_low_latency_sleep(&vk, 0, 20000) == 0 && count(EV_WAIT) == 0,
         "a failed sleep: no wait, no release");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_FAILED, "a failed sleep: failed");
   printf("   fallback: a backend error or an unsignalled sleep latches, "
          "the runloop paces as before\n");
}

int main(void)
{
   printf("vulkan driver pacing backend:\n");
   test_capability();
   test_mode();
   test_release();
   test_markers();
   test_generation();
   test_fallback();
   if (failures)
   {
      printf("FAILED: %u check(s)\n", failures);
      return 1;
   }
   printf("ok: the mode is set once per swapchain and change, each content "
          "frame gets one sleep and one bounded wait, markers name the "
          "released frame only, a replaced swapchain starts over, and "
          "errors fall back for good\n");
   return 0;
}
