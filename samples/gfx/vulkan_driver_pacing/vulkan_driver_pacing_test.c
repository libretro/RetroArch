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
 * sleep state machine does without a GPU, on one thread and with the
 * frame loop on another. The device setup is not run (it needs a
 * loader); the state it would leave is filled in by hand, the way
 * vulkan_low_latency_device_init() leaves it. */

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

/* What the backend asks the frontend: whether the frame loop is
 * another thread. */
static bool threaded;
bool video_driver_thread_wrapper_active(void) { return threaded; }

/* --- the fake driver --------------------------------------------- */

#define MAX_EVENTS 64
enum { EV_MODE, EV_SLEEP, EV_WAIT, EV_MARK, EV_DESTROY, EV_ANTILAG };
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

/* The markers so far, as a string of their numbers, for ordering
 * checks */
static const char *marks(void)
{
   static char buf[256];
   unsigned i;
   size_t n = 0;
   buf[0] = '\0';
   for (i = 0; i < num_events && i < MAX_EVENTS; i++)
      if (events[i].kind == EV_MARK && n + 8 < sizeof(buf))
         n += (size_t)snprintf(buf + n, sizeof(buf) - n, "%u:%u ",
               (unsigned)events[i].a, events[i].b);
   return buf;
}

static VKAPI_ATTR VkResult VKAPI_CALL fake_set_sleep_mode(VkDevice device,
      VkSwapchainKHR swapchain, const VkLatencySleepModeInfoNV *info)
{
   (void)device;
   /* Not optional: NVIDIA's driver reads it whatever the mode, and a
    * NULL here is a crash on taking the mode off */
   if (!info)
   {
      fprintf(stderr, "FAIL: vkSetLatencySleepModeNV given no pSleepModeInfo\n");
      abort();
   }
   ev(EV_MODE, (uint64_t)(uintptr_t)swapchain,
         info->lowLatencyMode | (info->lowLatencyBoost << 1),
         info->minimumIntervalUs);
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

static VKAPI_ATTR void VKAPI_CALL fake_destroy_swapchain(VkDevice device,
      VkSwapchainKHR swapchain, const VkAllocationCallbacks *alloc)
{
   (void)device; (void)alloc;
   ev(EV_DESTROY, (uint64_t)(uintptr_t)swapchain, 0, 0);
}

/* EV_ANTILAG: a = frame index, b = mode | stage << 4 (stage 0xf: no
 * presentation info), c = maxFPS */
static VKAPI_ATTR void VKAPI_CALL fake_anti_lag(VkDevice device,
      const VkAntiLagDataAMD *data)
{
   (void)device;
   ev(EV_ANTILAG,
         data->pPresentationInfo ? data->pPresentationInfo->frameIndex : 0,
         (unsigned)data->mode
         | ((data->pPresentationInfo ? (unsigned)data->pPresentationInfo->stage : 0xfu) << 4),
         data->maxFPS);
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
   retro_atomic_store_release_int(&vk.low_latency.answer,
         VIDEO_DRIVER_PACING_REASON_OFF);
   vkDestroySwapchainKHR         = fake_destroy_swapchain;
   num_events   = 0;
   threaded     = false;
   mode_result  = sleep_result = wait_result = VK_SUCCESS;
}

/* One frame as the presenter sees it: begin with the identity, then
 * the content present */
static void present_frame(uint64_t id, unsigned interval)
{
   vulkan_low_latency_frame_begin(&vk, id, interval);
   vulkan_low_latency_present_begin(&vk);
   vulkan_low_latency_present_end(&vk);
}

/* --- the tests ---------------------------------------------------- */

static void test_capability(void)
{
   reset(false, 0x100);
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_DEVICE,
         "no extension: the device reason");
   check(!vulkan_low_latency_wait(&vk, 1, 2, 20000), "no extension: no release");
   present_frame(1, 0);
   vulkan_low_latency_mark_main(&vk, 1, VK_LATENCY_MARKER_INPUT_SAMPLE_NV);
   check(num_events == 0, "no extension: nothing called");
   /* A core that made the device itself, without RetroArch's list */
   reset(false, 0x100);
   vk.low_latency.core_device = true;
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_CORE,
         "a core's device without the extension: the core reason");

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
   printf("   capability: device, core device, swapchain and external gates\n");
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
   check(count(EV_MODE) == 3 && e && e->b == 0 && e->c == 0,
         "off: the mode taken off with lowLatencyMode false");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_OFF)
         == VIDEO_DRIVER_PACING_REASON_OFF && count(EV_MODE) == 3,
         "off again: nothing called");
   present_frame(1, 0);
   check(count(EV_SLEEP) == 0 && count(EV_MARK) == 0,
         "off: a frame is presented with no sleep and no marker");
   check(!vulkan_low_latency_wait(&vk, 1, 2, 20000), "off: no release");
   printf("   mode: set once per change, taken off once, boost carried\n");
}

static void test_release(void)
{
   const struct event *e;
   reset(true, 0x100);
   vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON);
   check(vulkan_low_latency_wait(&vk, 0, 1, 20000) && count(EV_WAIT) == 0,
         "nothing handed over yet: released at once, no wait");

   present_frame(1, 0);
   e = last(EV_SLEEP);
   check(count(EV_SLEEP) == 1 && e && e->a == 1,
         "frame 1 presented: one sleep, signalling 1");
   check(vulkan_low_latency_wait(&vk, 1, 2, 20000), "frame 1: released");
   e = last(EV_WAIT);
   check(count(EV_WAIT) == 1 && e && e->a == 1 && e->b == 1,
         "frame 1: one wait, for value 1");
   check(last_wait_timeout == 20000ull * 1000ull,
         "the wait is bounded by what the runloop allows, in ns");

   present_frame(2, 0);
   e = last(EV_SLEEP);
   check(count(EV_SLEEP) == 2 && e && e->a == 2, "frame 2: the sleep signals 2");
   check(count(EV_MODE) == 1, "no interval change: the mode is not set again");

   present_frame(3, 16667);
   e = last(EV_MODE);
   check(count(EV_MODE) == 2 && e && e->c == 16667 && e->b == 1,
         "a timer period arrives with frame 3: set before its markers");
   check(last(EV_SLEEP)->a == 3, "and frame 3's sleep follows");
   present_frame(4, 16667);
   check(count(EV_MODE) == 2, "same period: not set again");
   present_frame(5, 0);
   e = last(EV_MODE);
   check(count(EV_MODE) == 3 && e && e->c == 0,
         "the timer stands down: the interval goes back to none");

   /* The same frame presented again, a repeat, a dark frame: out of
    * band on the same identity, no second sleep */
   num_events = 0;
   vulkan_low_latency_present_begin(&vk);
   vulkan_low_latency_present_end(&vk);
   check(count(EV_SLEEP) == 0 && count(EV_MARK) == 2
         && events[0].b == VK_LATENCY_MARKER_OUT_OF_BAND_PRESENT_START_NV
         && events[1].b == VK_LATENCY_MARKER_OUT_OF_BAND_PRESENT_END_NV
         && events[0].a == 5 && events[1].a == 5,
         "a second present of frame 5: out of band, no sleep");
   /* frame_begin with the same identity again: nothing new */
   vulkan_low_latency_frame_begin(&vk, 5, 0);
   check(count(EV_MARK) == 2, "frame 5 begun again: no markers again");
   /* An identity of 0: a frame the loop did not release; presented
    * out of band on the last released one */
   vulkan_low_latency_frame_begin(&vk, 0, 0);
   vulkan_low_latency_present_begin(&vk);
   vulkan_low_latency_present_end(&vk);
   check(count(EV_SLEEP) == 0 && count(EV_MARK) == 4,
         "an unreleased frame: out of band on frame 5, no sleep");
   printf("   release: one sleep per content frame after its present, a "
          "bounded wait for it, repeats out of band, the interval set "
          "when it changes\n");
}

static void test_markers(void)
{
   reset(true, 0x100);
   vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON);
   num_events = 0;
   /* The frame loop's own, through the published swapchain */
   vulkan_low_latency_mark_main(&vk, 1, VK_LATENCY_MARKER_SIMULATION_START_NV);
   vulkan_low_latency_mark_main(&vk, 1, VK_LATENCY_MARKER_INPUT_SAMPLE_NV);
   present_frame(1, 0);
   check(!strcmp(marks(), "1:0 1:6 1:1 1:2 1:3 1:4 1:5 "),
         "frame 1: sim start, input, sim end, render start, render end, "
         "present start, present end");
   if (strcmp(marks(), "1:0 1:6 1:1 1:2 1:3 1:4 1:5 "))
      printf("      got: %s\n", marks());
   check(count(EV_MARK) == 7 && last(EV_MARK)->c == 0x100,
         "markers name the swapchain the mode is on");
   num_events = 0;
   vulkan_low_latency_mark_main(&vk, 0, VK_LATENCY_MARKER_INPUT_SAMPLE_NV);
   check(count(EV_MARK) == 0, "frame 0 is no frame: no marker");
   vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_OFF);
   vulkan_low_latency_mark_main(&vk, 2, VK_LATENCY_MARKER_INPUT_SAMPLE_NV);
   check(count(EV_MARK) == 0, "mode off: the frame loop's marker names nothing");
   printf("   markers: the frame loop's on the published swapchain, the "
          "presenter's once per identity, in order\n");
}

static void test_generation(void)
{
   reset(true, 0x100);
   vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON);
   present_frame(1, 0);
   /* The swapchain is replaced and the old one retired, as
    * vulkan_create_swapchain() does; the handle value comes back the
    * same. */
   vulkan_low_latency_retire(&vk, (VkSwapchainKHR)(uintptr_t)0x100);
   check(count(EV_DESTROY) == 1, "the old swapchain, no marker call on it, goes at once");
   present_frame(2, 0);
   check(count(EV_SLEEP) == 1, "a replaced swapchain: no sleep until the mode is set on it");
   vulkan_low_latency_mark_main(&vk, 2, VK_LATENCY_MARKER_INPUT_SAMPLE_NV);
   check(count(EV_MARK) == 5, "a replaced swapchain: the frame loop's marker names nothing new");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_NONE && count(EV_MODE) == 2,
         "asked again: the mode is set on the new swapchain");
   present_frame(3, 0);
   check(last(EV_SLEEP)->a == 3, "and sleeps resume with the next identity");

   /* A different handle, the mode still recorded on the old one */
   vk.swapchain = (VkSwapchainKHR)(uintptr_t)0x200;
   present_frame(4, 0);
   check(last(EV_SLEEP)->a == 3, "another swapchain: the old generation never gets a sleep");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_NONE && last(EV_MODE)->a == 0x200,
         "asked again: set on the new handle");
   printf("   generation: nothing names a swapchain the mode was not set on\n");
}

static void test_threaded(void)
{
   const struct event *e;
   reset(true, 0x100);
   threaded = true;
   /* The frame loop asks; nothing is set until the presenter's frame */
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_PENDING && count(EV_MODE) == 0,
         "threaded: asked from the frame loop, pending until the presenter answers");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_PENDING, "threaded: still pending");
   check(!vulkan_low_latency_wait(&vk, 0, 1, 20000), "threaded: no release while pending");
   present_frame(0, 0);
   e = last(EV_MODE);
   check(count(EV_MODE) == 1 && e && e->a == 0x100 && e->b == 1,
         "the presenter's frame applies the request");
   check(count(EV_SLEEP) == 0, "the frame that applied it was not released: no sleep");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_NONE, "threaded: the answer comes back in effect");
   num_events = 0;
   vulkan_low_latency_mark_main(&vk, 1, VK_LATENCY_MARKER_SIMULATION_START_NV);
   check(count(EV_MARK) == 1 && last(EV_MARK)->c == 0x100,
         "the frame loop's marker reaches the published swapchain");
   present_frame(1, 0);
   check(last(EV_SLEEP) && last(EV_SLEEP)->a == 1, "frame 1 presented: its sleep");
   check(vulkan_low_latency_wait(&vk, 1, 2, 20000) && last(EV_WAIT)->a == 1,
         "the frame loop waits for frame 1");

   /* The presenter replaces the swapchain while the frame loop may be
    * in a marker call: the old one is kept until it is seen out */
   num_events = 0;
   retro_atomic_store_release_int(&vk.low_latency.in_call, 1);
   vk.swapchain = (VkSwapchainKHR)(uintptr_t)0x200;
   vulkan_low_latency_retire(&vk, (VkSwapchainKHR)(uintptr_t)0x100);
   check(count(EV_DESTROY) == 0 && vk.low_latency.num_retired == 1,
         "a swapchain in use by a marker call is kept");
   vulkan_low_latency_mark_main(&vk, 2, VK_LATENCY_MARKER_INPUT_SAMPLE_NV);
   check(count(EV_MARK) == 1 && last(EV_MARK)->c == 0x100,
         "until the loop is seen out, its markers still name the kept one");
   retro_atomic_store_release_int(&vk.low_latency.in_call, 0);
   present_frame(2, 0);
   e = last(EV_DESTROY);
   check(count(EV_DESTROY) == 1 && e && e->a == 0x100,
         "seen out of the call, the presenter's frame destroys it");
   /* The mode is on the new swapchain now (the standing request) */
   num_events = 0;
   vulkan_low_latency_mark_main(&vk, 2, VK_LATENCY_MARKER_INPUT_SAMPLE_NV);
   check(count(EV_MARK) == 1 && last(EV_MARK)->c == 0x200,
         "and the loop's markers name the new one");
   num_events = 0;

   /* Off from the frame loop: applied by the presenter, then answered */
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_OFF)
         == VIDEO_DRIVER_PACING_REASON_OFF, "threaded off: answered at once");
   num_events = 0;
   present_frame(3, 0);
   check(count(EV_MODE) == 1 && last(EV_MODE)->b == 0 && count(EV_SLEEP) == 0,
         "the presenter takes the mode off, no sleep");
   printf("   threaded: requests cross to the presenter, answers come back, "
          "a replaced swapchain outlives a marker call on it\n");
}

static void reset_amd(uintptr_t swapchain)
{
   reset(false, swapchain);
   vk.low_latency.anti_lag        = true;
   vk.low_latency.anti_lag_update = (PFN_vkVoidFunction)fake_anti_lag;
}

#define AL(mode, stage) ((unsigned)(mode) | ((unsigned)(stage) << 4))

static void test_anti_lag(void)
{
   const struct event *e;
   reset_amd(0x100);
   check(!vulkan_low_latency_wait(&vk, 0, 1, 20000) && count(EV_ANTILAG) == 0,
         "anti-lag off: no release, no update");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_NONE && count(EV_ANTILAG) == 0,
         "anti-lag on: in effect, nothing sent until a frame");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_BOOST)
         == VIDEO_DRIVER_PACING_REASON_NONE, "anti-lag boost: in effect (as On)");
   vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON);

   check(vulkan_low_latency_wait(&vk, 0, 1, 20000), "frame 1: released");
   e = last(EV_ANTILAG);
   check(count(EV_ANTILAG) == 1 && e && e->a == 1
         && e->b == AL(VK_ANTI_LAG_MODE_ON_AMD, VK_ANTI_LAG_STAGE_INPUT_AMD)
         && e->c == 0,
         "frame 1's INPUT update: on, the identity it will carry, no limit");
   present_frame(1, 16667);
   e = last(EV_ANTILAG);
   check(count(EV_ANTILAG) == 2 && e && e->a == 1
         && e->b == AL(VK_ANTI_LAG_MODE_ON_AMD, VK_ANTI_LAG_STAGE_PRESENT_AMD),
         "frame 1's PRESENT update names the same frame");
   check(count(EV_SLEEP) == 0 && count(EV_MARK) == 0 && count(EV_MODE) == 0,
         "anti-lag: none of low_latency2's calls");
   /* A second present of frame 1: nothing */
   vulkan_low_latency_present_begin(&vk);
   vulkan_low_latency_present_end(&vk);
   check(count(EV_ANTILAG) == 2, "a repeat of frame 1: no second PRESENT update");
   vulkan_low_latency_wait(&vk, 1, 2, 20000);
   e = last(EV_ANTILAG);
   check(e && e->a == 2 && e->c == 60,
         "frame 2's INPUT update carries the limit frame 1 brought: 60 fps");
   present_frame(2, 0);
   vulkan_low_latency_wait(&vk, 2, 3, 20000);
   check(last(EV_ANTILAG)->c == 0, "the limit goes when the timer stands down");
   vulkan_low_latency_mark_main(&vk, 3, VK_LATENCY_MARKER_INPUT_SAMPLE_NV);
   check(count(EV_MARK) == 0, "anti-lag: no markers");

   num_events = 0;
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_OFF)
         == VIDEO_DRIVER_PACING_REASON_OFF, "anti-lag off");
   e = last(EV_ANTILAG);
   check(count(EV_ANTILAG) == 1 && e && e->b == AL(VK_ANTI_LAG_MODE_OFF_AMD, 0xf),
         "off: one update taking the mode off, with no frame");
   check(!vulkan_low_latency_wait(&vk, 3, 4, 20000) && count(EV_ANTILAG) == 1,
         "off: no release, no INPUT update");
   present_frame(4, 0);
   check(count(EV_ANTILAG) == 1, "off: no PRESENT update");

   /* Threaded: the request crosses to the presenter */
   reset_amd(0x100);
   threaded = true;
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_PENDING, "threaded anti-lag: pending");
   check(!vulkan_low_latency_wait(&vk, 0, 1, 20000), "threaded anti-lag: no release while pending");
   present_frame(0, 0);
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_NONE, "threaded anti-lag: answered");
   check(vulkan_low_latency_wait(&vk, 0, 1, 20000)
         && last(EV_ANTILAG)->b == AL(VK_ANTI_LAG_MODE_ON_AMD, VK_ANTI_LAG_STAGE_INPUT_AMD),
         "threaded anti-lag: the frame loop's INPUT update");

   reset_amd(0);
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_SWAPCHAIN, "anti-lag, no swapchain: the swapchain reason");
   reset(false, 0x100);
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_DEVICE, "neither extension: the device reason");
   printf("   anti-lag: INPUT update as the hold naming the next frame, "
          "PRESENT update on that frame once, limit from the timer, off "
          "with one update, requests cross to the presenter\n");
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
   check(!vulkan_low_latency_wait(&vk, 1, 2, 20000), "failed: no release");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_OFF)
         == VIDEO_DRIVER_PACING_REASON_OFF
         && vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_NONE,
         "off then on again: tried anew");

   reset(true, 0x100);
   vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON);
   present_frame(1, 0);
   wait_result = VK_TIMEOUT;
   check(!vulkan_low_latency_wait(&vk, 1, 2, 20000)
         && !vulkan_low_latency_wait(&vk, 1, 2, 20000)
         && !vulkan_low_latency_wait(&vk, 1, 2, 20000),
         "signals that do not come: no release, within the bound");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_NONE,
         "three strikes: still in effect");
   check(!vulkan_low_latency_wait(&vk, 1, 2, 20000), "the fourth: no release");
   num_events = 0;
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_FAILED, "the fourth: failed from then on");
   wait_result = VK_SUCCESS;
   present_frame(2, 0);
   check(count(EV_SLEEP) == 0 && last(EV_MODE) && last(EV_MODE)->b == 0,
         "the mode comes off: no sleep on a semaphore the driver still owes a signal");

   reset(true, 0x100);
   vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON);
   sleep_result = VK_ERROR_DEVICE_LOST;
   present_frame(1, 0);
   check(!vulkan_low_latency_wait(&vk, 1, 2, 20000) && count(EV_WAIT) == 0,
         "a failed sleep: no wait, no release");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_FAILED, "a failed sleep: failed");
   printf("   fallback: a backend error or four unsignalled sleeps latch, "
          "the runloop paces as before, off and on tries anew\n");
}


#ifdef VULKAN_DXGI_PACING
/* --- presented through DXGI: Reflex through NVAPI ------------------ */

#include "../../../gfx/common/win_vulkan_dxgi.h"

/* The presenter's D3D12 device, as far as COM's references go */
typedef struct fake_com fake_com_t;
struct fake_com_vtbl
{
   void *query_interface;
   unsigned long (__stdcall *add_ref)(fake_com_t *self);
   unsigned long (__stdcall *release)(fake_com_t *self);
};
struct fake_com
{
   const struct fake_com_vtbl *lpVtbl;
   unsigned long refs;
};
static unsigned long __stdcall fake_add_ref(fake_com_t *self)
{ return ++self->refs; }
static unsigned long __stdcall fake_release(fake_com_t *self)
{ return --self->refs; }
static const struct fake_com_vtbl fake_com_vtbl = { NULL, fake_add_ref,
   fake_release };
static fake_com_t fake_d3d12 = { &fake_com_vtbl, 1 };

/* The presenter itself is never driven here: only its device is */
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
void *ra_vulkan_dxgi_device(ra_vulkan_dxgi_t *p)
{ return p ? (void*)&fake_d3d12 : NULL; }

/* fake_nvapi.c's log, read through the DLL the backend loaded */
typedef struct
{
   int kind;
   void *device;
   uint64_t a;
   unsigned b;
} nv_event_t;
enum { NV_MODE, NV_SLEEP, NV_MARK };
typedef unsigned (*nv_log_t)(const nv_event_t **out, int reset);
typedef void (*nv_fail_t)(int fail);
static nv_log_t  nv_log;
static nv_fail_t nv_fail;

static unsigned nv_count(int kind, const nv_event_t **last)
{
   const nv_event_t *e;
   unsigned i, n = nv_log(&e, 0), c = 0;
   *last = NULL;
   for (i = 0; i < n; i++)
      if (e[i].kind == kind)
      {
         c++;
         *last = &e[i];
      }
   return c;
}

static void nv_reset(void)
{
   const nv_event_t *e;
   nv_log(&e, 1);
}

static void test_dxgi(void)
{
   const nv_event_t *e;
   HMODULE dll = LoadLibraryA("nvapi64.dll");
   void *presenter = (void*)(uintptr_t)0x5;
   check(dll != NULL, "dxgi: the stand-in nvapi64.dll loads");
   if (!dll)
      return;
   nv_log  = (nv_log_t)GetProcAddress(dll, "fake_nvapi_log");
   nv_fail = (nv_fail_t)GetProcAddress(dll, "fake_nvapi_fail_sleep");
   if (!nv_log || !nv_fail)
      return;

   /* Vulkan's own backend is up, but nothing reaches it: the present
    * is DXGI's, and the mode goes on its D3D12 device */
   reset(true, 0x100);
   vk.dxgi_presenter = presenter;
   nv_reset();
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_NONE, "dxgi: in effect");
   check(nv_count(NV_MODE, &e) == 1 && e->device == &fake_d3d12
         && e->a == 1, "dxgi: the mode on the presenter's D3D12 device");
   check(fake_d3d12.refs == 2, "dxgi: the device held while it is used");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_BOOST)
         == VIDEO_DRIVER_PACING_REASON_NONE
         && nv_count(NV_MODE, &e) == 2 && e->a == 3,
         "dxgi: boost");
   check(fake_d3d12.refs == 2, "dxgi: held once");

   /* A frame: the presenter's markers on the device, the frame loop's
    * sleep and input marker on it too */
   nv_reset();
   present_frame(1, 16667);
   check(nv_count(NV_MODE, &e) == 1 && e->b == 16667,
         "dxgi: the frame's interval goes to the sleep mode");
   check(nv_count(NV_MARK, &e) == 5 && e->a == 1 && e->b == 5,
         "dxgi: simulation end, submit and present markers on frame 1");
   check(vulkan_low_latency_wait(&vk, 1, 2, 20000)
         && nv_count(NV_SLEEP, &e) == 1 && e->device == &fake_d3d12,
         "dxgi: the release is NvAPI_D3D_Sleep on the device");
   vulkan_low_latency_mark_main(&vk, 2,
         VIDEO_DRIVER_PACING_MARK_INPUT_SAMPLE);
   check(nv_count(NV_MARK, &e) == 6 && e->a == 2 && e->b == 6,
         "dxgi: the frame loop's input marker");
   check(count(EV_MODE) == 0 && count(EV_SLEEP) == 0
         && count(EV_MARK) == 0 && count(EV_WAIT) == 0,
         "dxgi: nothing of VK_NV_low_latency2 is called");

   /* A sleep NVAPI refuses: the frame loop falls back, the mode comes
    * off at the presenter, and asking anew brings it back */
   nv_fail(1);
   check(!vulkan_low_latency_wait(&vk, 2, 3, 20000),
         "dxgi: a failed sleep releases nothing");
   nv_fail(0);
   nv_reset();
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_BOOST)
         == VIDEO_DRIVER_PACING_REASON_FAILED
         && nv_count(NV_MODE, &e) == 1 && e->a == 0,
         "dxgi: failed, and the mode taken off the device");
   check(!vulkan_low_latency_wait(&vk, 2, 3, 20000)
         && nv_count(NV_SLEEP, &e) == 0, "dxgi: failed: no sleep");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_OFF)
         == VIDEO_DRIVER_PACING_REASON_OFF
         && vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_NONE,
         "dxgi: off and on again: in effect");

   /* Off: taken off, the frame loop not held */
   nv_reset();
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_OFF)
         == VIDEO_DRIVER_PACING_REASON_OFF
         && nv_count(NV_MODE, &e) == 1 && e->a == 0,
         "dxgi: off: the mode taken off the device");
   check(!vulkan_low_latency_wait(&vk, 2, 3, 20000)
         && nv_count(NV_SLEEP, &e) == 0, "dxgi: off: no sleep");

   /* Threaded: the presenter answers the frame loop's request */
   threaded = true;
   nv_reset();
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_PENDING && nv_count(NV_MODE, &e) == 0,
         "dxgi threaded: pending until the presenter's frame");
   present_frame(3, 0);
   check(nv_count(NV_MODE, &e) >= 1 && e->a == 1,
         "dxgi threaded: the presenter puts the mode on");
   check(vulkan_low_latency_set(&vk, VIDEO_DRIVER_PACING_ON)
         == VIDEO_DRIVER_PACING_REASON_NONE
         && vulkan_low_latency_wait(&vk, 3, 4, 20000)
         && nv_count(NV_SLEEP, &e) == 1,
         "dxgi threaded: in effect, the frame loop sleeps on the device");

   /* Native again: the device's mode comes off, Vulkan's own goes on */
   vk.dxgi_presenter = NULL;
   nv_reset();
   num_events = 0;
   present_frame(4, 0);
   check(nv_count(NV_MODE, &e) == 1 && e->a == 0,
         "dxgi gone: the mode taken off the D3D12 device");
   check(count(EV_MODE) == 1 && last(EV_MODE)->b == 1,
         "dxgi gone: VK_NV_low_latency2's mode on the swapchain");
   threaded = false;
   printf("   dxgi: through DXGI the mode, sleep and markers are NVAPI's "
          "on the presenter's D3D12 device, held while used, and fall "
          "back like the native path's\n");
}
#endif

int main(void)
{
   printf("vulkan driver pacing backend:\n");
   test_capability();
   test_mode();
   test_release();
   test_markers();
   test_generation();
   test_threaded();
   test_anti_lag();
   test_fallback();
#ifdef VULKAN_DXGI_PACING
   test_dxgi();
#endif
   if (failures)
   {
      printf("FAILED: %u check(s)\n", failures);
      return 1;
   }
   printf("ok: the mode is set once per swapchain and change, each content "
          "frame's present is followed by one sleep the frame loop waits "
          "for, repeats go out of band, markers name the released frame "
          "only, a replaced swapchain starts over and outlives a marker "
          "call, and errors fall back\n");
   return 0;
}
