/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (vulkan_mailbox_init_leak_test.c).
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

/* Regression test for the partial-init leak in
 * gfx/common/vulkan_common.c::vulkan_emulated_mailbox_init().
 *
 * The function brings up three things in sequence and used to
 * return `false` directly when the second or third failed,
 * leaking what the earlier ones had allocated.  The two
 * production call sites (vulkan_create_swapchain) ignore the
 * return value -- so an init failure also left the mailbox half
 * built while VK_DATA_FLAG_EMULATING_MAILBOX was still set,
 * setting up a crash the next time vulkan_acquire_next_image()
 * routed into the emulated path.
 *
 * Fix: route every early-failure path in
 * vulkan_emulated_mailbox_init() through `goto error` to a
 * single cleanup that calls vulkan_emulated_mailbox_deinit() --
 * which is safe on a partly built mailbox and ends with a
 * memset, so the deinit-on-failure shape matches the
 * deinit-on-shutdown shape exactly, leaving mailbox->swapchain
 * == VK_NULL_HANDLE and tripping the existing
 *   if (vk->mailbox.swapchain == VK_NULL_HANDLE)
 *      err = VK_ERROR_OUT_OF_DATE_KHR;
 * guard at vulkan_acquire_next_image().
 *
 * When this was written the three things were a condition
 * variable, a mutex and the thread.  The mailbox takes no lock
 * any more: they are two eventcounts and the thread, and the
 * failure stages below follow.
 *
 * IMPORTANT: this test keeps verbatim copies of
 * vulkan_emulated_mailbox_init() and
 * vulkan_emulated_mailbox_deinit() from vulkan_common.c.  If
 * either function amends, the copies below must follow.  The
 * test runs under -fsanitize=address with leak detection
 * enabled so any reintroduction of the partial-init leak is
 * caught at the leak level.  Convention used by the v3 vulkan
 * tests, the v4 slang test, and the security regression tests
 * under samples/tasks/.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Pull in ASan's leak-check entry point so the test can
 * deterministically check for leaks at the end of each probe
 * rather than relying on atexit detection.  The
 * __lsan_do_recoverable_leak_check() symbol is provided by ASan
 * in the same shared object as the runtime; when ASan isn't
 * built in this resolves to a weak symbol returning 0. */
extern int __lsan_do_recoverable_leak_check(void) __attribute__((weak));

/* --- Mocks for the libretro-common primitives used by the
 *     functions under test.  The eventcount mock owns a heap
 *     block, as the production one does on the backends that
 *     need a mutex and a condition variable, so that ASan can
 *     track allocate/free pairing; it supports controlled
 *     failure injection so the test can exercise each
 *     early-return stage of vulkan_emulated_mailbox_init.  Like
 *     the real retro_eventcount_free(), the mock's free is safe
 *     on a zeroed object and on one whose init failed. --- */

typedef struct mock_eventcount
{
   void *impl;
} retro_eventcount_t;

typedef struct mock_sthread_t {
   uint8_t padding[64];
} sthread_t;

typedef int retro_atomic_int_t;
#define retro_atomic_int_init(p, v)          (*(p) = (v))
#define retro_atomic_store_release_int(p, v) (*(p) = (v))

/* Mock VkDevice / VkSwapchainKHR -- both are dispatchable
 * Vulkan handles.  In production these are pointer-sized opaque;
 * for the test any sentinel value works as long as deinit's
 * memset clears them to 0. */
typedef void *VkDevice;
typedef void *VkSwapchainKHR;
#define VK_NULL_HANDLE ((void*)0)
typedef int VkResult;
#define VK_SUCCESS 0

struct vulkan_emulated_mailbox
{
   sthread_t *thread;
   VkDevice device;
   VkSwapchainKHR swapchain;
   int64_t timeout_us;

   retro_eventcount_t work;
   retro_eventcount_t answered;
   retro_atomic_int_t request;
   retro_atomic_int_t acquired;
   retro_atomic_int_t dead;

   unsigned index;
   VkResult result;
   bool has_pending_request;
};

/* Failure-injection knobs.  fail_eventcount_after counts
 * retro_eventcount_init calls: 0 fails the next one, 1 the one
 * after it, -1 none. */
static int fail_eventcount_after = -1;
static int fail_sthread_after    = -1;

static int alloc_count_eventcount = 0;
static int alloc_count_sthread    = 0;
static int free_count_eventcount  = 0;
static int free_count_sthread     = 0;

static bool retro_eventcount_init(retro_eventcount_t *ec)
{
   ec->impl = NULL;
   if (fail_eventcount_after == 0)
   {
      fail_eventcount_after = -1;
      return false;
   }
   if (fail_eventcount_after > 0)
      fail_eventcount_after--;
   alloc_count_eventcount++;
   ec->impl = calloc(1, 96);
   return ec->impl != NULL;
}

static void retro_eventcount_free(retro_eventcount_t *ec)
{
   if (ec->impl)
   {
      free_count_eventcount++;
      free(ec->impl);
      ec->impl = NULL;
   }
}

static void retro_eventcount_notify(retro_eventcount_t *ec) { (void)ec; }

/* Production sthread_create takes a (callback, userdata) pair.
 * For the test the callback is never invoked; we just allocate
 * the handle so the leak path is observable. */
typedef void (*sthread_callback_t)(void *);

static sthread_t *sthread_create(sthread_callback_t cb, void *arg)
{
   (void)cb;
   (void)arg;
   if (fail_sthread_after == 0)
   {
      fail_sthread_after = -1;
      return NULL;
   }
   if (fail_sthread_after > 0)
      fail_sthread_after--;
   alloc_count_sthread++;
   return (sthread_t*)calloc(1, sizeof(sthread_t));
}

static void sthread_join(sthread_t *t) { free_count_sthread++; free(t); }

static int64_t vulkan_mailbox_timeout_us(void) { return 16000; }

/* Stand-in for the production loop function.  Production
 * sthread_create takes a function pointer, so we need a real
 * symbol to take an address of even though it's never invoked. */
static void vulkan_emulated_mailbox_loop(void *unused)
{
   (void)unused;
}

/* === verbatim copy of vulkan_emulated_mailbox_deinit from
 *     gfx/common/vulkan_common.c.  Note: deinit must be visible
 *     before init below so the init's `goto error` branch
 *     resolves the symbol.  If vulkan_common.c amends either
 *     function, both copies must follow. === */
static void vulkan_emulated_mailbox_deinit(
      struct vulkan_emulated_mailbox *mailbox)
{
   if (mailbox->thread)
   {
      retro_atomic_store_release_int(&mailbox->dead, 1);
      retro_eventcount_notify(&mailbox->work);
      /* Wait for the background thread to see the dead flag.
       * Its acquire and fence waits are finite, so it will
       * unblock and exit the loop. */
      sthread_join(mailbox->thread);
   }

   /* Safe on ones never brought up: the struct starts zeroed. */
   retro_eventcount_free(&mailbox->answered);
   retro_eventcount_free(&mailbox->work);

   memset(mailbox, 0, sizeof(*mailbox));
}
/* === end verbatim copy === */

/* === verbatim copy of vulkan_emulated_mailbox_init from
 *     vulkan_common.c.  If the production function amends the
 *     goto-error chain or the deinit dispatch, this copy must
 *     follow. === */
static bool vulkan_emulated_mailbox_init(
      struct vulkan_emulated_mailbox *mailbox,
      VkDevice device,
      VkSwapchainKHR swapchain)
{
   /* Zeroed first, so that the error path can free an eventcount that
    * was never brought up. */
   memset(mailbox, 0, sizeof(*mailbox));
   mailbox->device              = device;
   mailbox->swapchain           = swapchain;
   mailbox->timeout_us          = vulkan_mailbox_timeout_us();
   mailbox->result              = VK_SUCCESS;
   retro_atomic_int_init(&mailbox->request, 0);
   retro_atomic_int_init(&mailbox->acquired, 0);
   retro_atomic_int_init(&mailbox->dead, 0);

   if (!retro_eventcount_init(&mailbox->work))
      goto error;
   if (!retro_eventcount_init(&mailbox->answered))
      goto error;
   if (!(mailbox->thread    = sthread_create(vulkan_emulated_mailbox_loop,
               mailbox)))
      goto error;
   return true;

error:
   /* Tear down anything we managed to allocate before failing.
    * vulkan_emulated_mailbox_deinit() is null-safe and ends with
    * a memset, so the struct is left in the same shape a caller
    * would see after a successful init+deinit cycle -- callers
    * that ignore our return value (the two sites in
    * vulkan_create_swapchain) will then take the
    * mailbox.swapchain == VK_NULL_HANDLE branch in
    * vulkan_acquire_next_image and skip the emulated path
    * cleanly instead of using a mailbox that never came up. */
   vulkan_emulated_mailbox_deinit(mailbox);
   return false;
}
/* === end verbatim copy === */

static int failures = 0;

/* Helper: reset all counters and inject a failure at one stage. */
static void reset_counters_and_inject(int eventcount_at, int sthread_at)
{
   fail_eventcount_after  = eventcount_at;
   fail_sthread_after     = sthread_at;
   alloc_count_eventcount = alloc_count_sthread = 0;
   free_count_eventcount  = free_count_sthread  = 0;
}

/* Verify that the mailbox struct ended up in the canonical
 * "deinit'd" shape: nothing held, swapchain == VK_NULL_HANDLE.
 * This is what vulkan_acquire_next_image's existing
 * `mailbox.swapchain == VK_NULL_HANDLE` guard relies on to skip
 * the emulated path safely. */
static bool check_mailbox_clean(const struct vulkan_emulated_mailbox *m)
{
   return    m->thread        == NULL
          && m->work.impl     == NULL
          && m->answered.impl == NULL
          && m->swapchain     == VK_NULL_HANDLE;
}

/* One failure stage: the mailbox is poisoned first, so a field
 * the error path forgets shows; init must report failure, free
 * exactly what it allocated, and leave the mailbox clean. */
static void test_failure_stage(const char *what,
      int eventcount_at, int sthread_at,
      int want_eventcounts, uint8_t poison)
{
   struct vulkan_emulated_mailbox mailbox;
   bool rv;

   reset_counters_and_inject(eventcount_at, sthread_at);
   memset(&mailbox, poison, sizeof(mailbox));

   rv = vulkan_emulated_mailbox_init(&mailbox,
         (VkDevice)0xD000,
         (VkSwapchainKHR)0x5000);

   if (rv)
   {
      printf("[ERROR] %s: init returned true\n", what);
      failures++;
      return;
   }
   if (     alloc_count_eventcount != want_eventcounts
         || free_count_eventcount  != want_eventcounts)
   {
      printf("[ERROR] %s: eventcount alloc/free mismatch"
            " (%d allocated, %d freed, expected %d of each)\n",
            what, alloc_count_eventcount, free_count_eventcount,
            want_eventcounts);
      failures++;
      return;
   }
   if (alloc_count_sthread != 0)
   {
      printf("[ERROR] %s: a thread was created\n", what);
      failures++;
      return;
   }
   if (!check_mailbox_clean(&mailbox))
   {
      printf("[ERROR] %s: mailbox not in clean state\n", what);
      failures++;
      return;
   }

   printf("[SUCCESS] %s: %d allocated, %d freed, clean state\n",
         what, want_eventcounts, want_eventcounts);
}

/* Probe: everything succeeds.  Init returns true and the mailbox
 * is fully populated.  We then run deinit by hand to clean up so
 * ASan's exit-time leak check stays clean. */
static void test_full_success(void)
{
   struct vulkan_emulated_mailbox mailbox;
   bool rv;

   reset_counters_and_inject(-1, -1);
   memset(&mailbox, 0x5A, sizeof(mailbox));

   rv = vulkan_emulated_mailbox_init(&mailbox,
         (VkDevice)0xD000,
         (VkSwapchainKHR)0x5000);

   if (!rv)
   {
      printf("[ERROR] full success: init returned false\n");
      failures++;
      return;
   }
   if (   mailbox.thread        == NULL
       || mailbox.work.impl     == NULL
       || mailbox.answered.impl == NULL
       || mailbox.swapchain     != (VkSwapchainKHR)0x5000
       || mailbox.device        != (VkDevice)0xD000
       || mailbox.request || mailbox.acquired || mailbox.dead
       || mailbox.has_pending_request)
   {
      printf("[ERROR] full success: mailbox not populated\n");
      failures++;
      return;
   }
   if (alloc_count_eventcount != 2 || alloc_count_sthread != 1)
   {
      printf("[ERROR] full success: alloc counts wrong "
            "(eventcount=%d sthread=%d)\n",
            alloc_count_eventcount, alloc_count_sthread);
      failures++;
      return;
   }

   /* Tear down by the same path the production code would use --
    * directly invoke deinit since we can't actually run the
    * mailbox loop in the test. */
   vulkan_emulated_mailbox_deinit(&mailbox);

   if (     free_count_eventcount != 2 || free_count_sthread != 1
         || !check_mailbox_clean(&mailbox))
   {
      printf("[ERROR] full success: deinit didn't return mailbox to clean state\n");
      failures++;
      return;
   }

   printf("[SUCCESS] full success path: 3 allocs / 3 frees, mailbox clean post-deinit\n");
}

/* Probe: explicitly check ASan's leak detector after each
 * failure stage runs.  When ASan is built in,
 * __lsan_do_recoverable_leak_check returns nonzero if there
 * are unfreed allocations (other than the mailbox struct
 * itself, which is on the stack).  When ASan isn't built in
 * the weak symbol resolves to 0 and this is a no-op probe
 * (still useful as a smoke test of the run-to-completion). */
static void test_lsan_clean_after_each_failure(void)
{
   struct vulkan_emulated_mailbox mailbox;

   reset_counters_and_inject(0, -1);
   (void)vulkan_emulated_mailbox_init(&mailbox, NULL, NULL);

   reset_counters_and_inject(1, -1);
   (void)vulkan_emulated_mailbox_init(&mailbox, NULL, NULL);

   reset_counters_and_inject(-1, 0);
   (void)vulkan_emulated_mailbox_init(&mailbox, NULL, NULL);

   if (__lsan_do_recoverable_leak_check)
   {
      if (__lsan_do_recoverable_leak_check())
      {
         printf("[ERROR] LSan reports leaks after running failure stages\n");
         failures++;
         return;
      }
      printf("[SUCCESS] LSan clean after all three failure stages\n");
   }
   else
   {
      printf("[SUCCESS] (LSan not present; run-to-completion smoke pass only)\n");
   }
}

/* The thread failing is its own shape: both eventcounts were
 * brought up and both must go. */
static void test_sthread_fails_third(void)
{
   struct vulkan_emulated_mailbox mailbox;
   bool rv;

   reset_counters_and_inject(-1, 0);
   memset(&mailbox, 0xEF, sizeof(mailbox));

   rv = vulkan_emulated_mailbox_init(&mailbox,
         (VkDevice)0xD000,
         (VkSwapchainKHR)0x5000);

   if (rv)
   {
      printf("[ERROR] sthread fails: init returned true\n");
      failures++;
      return;
   }
   if (alloc_count_eventcount != 2 || free_count_eventcount != 2)
   {
      printf("[ERROR] sthread fails: eventcount alloc/free mismatch (%d/%d)\n",
            alloc_count_eventcount, free_count_eventcount);
      failures++;
      return;
   }
   if (!check_mailbox_clean(&mailbox))
   {
      printf("[ERROR] sthread fails: mailbox not in clean state\n");
      failures++;
      return;
   }

   printf("[SUCCESS] sthread_create failure: both eventcounts freed (2 allocs, 2 frees), clean state\n");
}

int main(void)
{
   test_failure_stage("first eventcount fails",  0, -1, 0, 0xAB);
   test_failure_stage("second eventcount fails", 1, -1, 1, 0xCD);
   test_sthread_fails_third();
   test_full_success();
   test_lsan_clean_after_each_failure();

   if (failures)
   {
      printf("\n%d vulkan_mailbox_init_leak test(s) failed\n", failures);
      return 1;
   }
   printf("\nAll vulkan_mailbox_init_leak regression tests passed.\n");
   return 0;
}
