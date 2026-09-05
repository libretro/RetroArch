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
 * Pre-fix the function had three sequential allocations
 * (scond_new, slock_new, sthread_create) and on any of the
 * latter two failures returned `false` directly, leaking the
 * already-allocated cond and/or lock.  The two production call
 * sites (vulkan_create_swapchain) ignore the return value -- so
 * an init failure also left vk->mailbox.lock == NULL and cond ==
 * NULL while VK_DATA_FLAG_EMULATING_MAILBOX was still set,
 * setting up a NULL-deref the next time
 * vulkan_acquire_next_image() routed into the emulated path
 * (slock_lock(NULL) inside
 * vulkan_emulated_mailbox_acquire_next_image()).
 *
 * The leak itself was small (a libretro-common slock_t + scond_t,
 * each holding a pthread mutex + cond_t worth of data), and the
 * trigger required scond_new() or slock_new() to fail -- which
 * means OOM or pthread resource exhaustion.  Realistic mainly on
 * memory-constrained devices.  Worth fixing because the same
 * change closes the NULL-deref crash via the
 * vulkan_emulated_mailbox_deinit() memset at line 132 of
 * vulkan_common.c, leaving mailbox->swapchain == VK_NULL_HANDLE
 * and tripping the existing
 *   if (vk->mailbox.swapchain == VK_NULL_HANDLE)
 *      err = VK_ERROR_OUT_OF_DATE_KHR;
 * guard at vulkan_acquire_next_image().
 *
 * Fix: route every early-failure path in
 * vulkan_emulated_mailbox_init() through `goto error` to a
 * single cleanup that calls vulkan_emulated_mailbox_deinit() --
 * which is null-safe and ends with a memset, so the
 * deinit-on-failure shape matches the deinit-on-shutdown shape
 * exactly.
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

/* --- Mocks for the libretro-common threading wrappers used
 *     by the function under test.  Each is a thin shim that
 *     malloc()'s a small struct and supports controlled failure
 *     injection so the test can exercise each early-return
 *     stage of vulkan_emulated_mailbox_init.  Production
 *     scond_new / slock_new / sthread_create wrap pthread
 *     primitives; for the test we only need ASan to track
 *     allocate/free pairing. --- */

typedef struct mock_scond_t {
   /* Padding so leak diagnostics show a meaningful object
    * size in the report.  The libretro-common scond_t holds at
    * least a pthread_cond_t which is ~48 bytes on glibc. */
   uint8_t padding[64];
} scond_t;

typedef struct mock_slock_t {
   /* libretro-common slock_t holds at least a pthread_mutex_t
    * which is ~40 bytes on glibc. */
   uint8_t padding[64];
} slock_t;

typedef struct mock_sthread_t {
   uint8_t padding[64];
} sthread_t;

/* Mock VkDevice / VkSwapchainKHR -- both are dispatchable
 * Vulkan handles.  In production these are pointer-sized opaque;
 * for the test any sentinel value works as long as deinit's
 * memset clears them to 0. */
typedef void *VkDevice;
typedef void *VkSwapchainKHR;
#define VK_NULL_HANDLE ((void*)0)
typedef int VkResult;
#define VK_SUCCESS 0
#define VK_MAILBOX_FLAG_DEAD                 (1u << 0)
#define VK_MAILBOX_FLAG_HAS_PENDING_REQUEST  (1u << 1)
#define VK_MAILBOX_FLAG_REQUEST_ACQUIRE      (1u << 2)
#define VK_MAILBOX_FLAG_ACQUIRED             (1u << 3)

struct vulkan_emulated_mailbox
{
   sthread_t      *thread;
   slock_t        *lock;
   scond_t        *cond;
   VkDevice        device;
   VkSwapchainKHR  swapchain;
   unsigned        index;
   VkResult        result;
   uint32_t        flags;
};

/* Failure-injection knobs.  Each is consulted by the
 * corresponding mock and returns NULL when the matching counter
 * is positive (which the test decrements as it injects). */
static int fail_scond_after = -1;
static int fail_slock_after = -1;
static int fail_sthread_after = -1;

static int alloc_count_scond = 0;
static int alloc_count_slock = 0;
static int alloc_count_sthread = 0;
static int free_count_scond = 0;
static int free_count_slock = 0;
static int free_count_sthread = 0;

static scond_t *scond_new(void)
{
   if (fail_scond_after == 0)
   {
      fail_scond_after = -1;
      return NULL;
   }
   if (fail_scond_after > 0)
      fail_scond_after--;
   alloc_count_scond++;
   return (scond_t*)calloc(1, sizeof(scond_t));
}

static slock_t *slock_new(void)
{
   if (fail_slock_after == 0)
   {
      fail_slock_after = -1;
      return NULL;
   }
   if (fail_slock_after > 0)
      fail_slock_after--;
   alloc_count_slock++;
   return (slock_t*)calloc(1, sizeof(slock_t));
}

static void scond_free(scond_t *c)   { free_count_scond++;   free(c); }
static void slock_free(slock_t *l)   { free_count_slock++;   free(l); }

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

/* The lock/signal calls are no-ops for the test; only deinit
 * uses them and only when thread != NULL. */
static void slock_lock(slock_t *l)     { (void)l; }
static void slock_unlock(slock_t *l)   { (void)l; }
static void scond_signal(scond_t *c)   { (void)c; }

/* Stand-in for the production loop function.  Production
 * sthread_create takes a function pointer, so we need a real
 * symbol to take an address of even though it's never invoked. */
static void vulkan_emulated_mailbox_loop(void *unused)
{
   (void)unused;
}

/* === verbatim copy of vulkan_emulated_mailbox_deinit from
 *     gfx/common/vulkan_common.c.  Note: deinit must be visible
 *     before init below so the post-fix init's `goto error`
 *     branch resolves the symbol.  If vulkan_common.c amends
 *     either function, both copies must follow. === */
static void vulkan_emulated_mailbox_deinit(
      struct vulkan_emulated_mailbox *mailbox)
{
   if (mailbox->thread)
   {
      slock_lock(mailbox->lock);
      mailbox->flags |= VK_MAILBOX_FLAG_DEAD;
      scond_signal(mailbox->cond);
      slock_unlock(mailbox->lock);
      sthread_join(mailbox->thread);
   }

   if (mailbox->lock)
      slock_free(mailbox->lock);
   if (mailbox->cond)
      scond_free(mailbox->cond);

   memset(mailbox, 0, sizeof(*mailbox));
}
/* === end verbatim copy === */

/* === verbatim copy of the post-fix vulkan_emulated_mailbox_init
 *     from vulkan_common.c.  If the production function amends
 *     the goto-error chain or the deinit dispatch, this copy
 *     must follow. === */
static bool vulkan_emulated_mailbox_init(
      struct vulkan_emulated_mailbox *mailbox,
      VkDevice device,
      VkSwapchainKHR swapchain)
{
   mailbox->thread              = NULL;
   mailbox->lock                = NULL;
   mailbox->cond                = NULL;
   mailbox->device              = device;
   mailbox->swapchain           = swapchain;
   mailbox->index               = 0;
   mailbox->result              = VK_SUCCESS;
   mailbox->flags               = 0;

   if (!(mailbox->cond      = scond_new()))
      goto error;
   if (!(mailbox->lock      = slock_new()))
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
    * cleanly instead of dereferencing a NULL lock/cond. */
   vulkan_emulated_mailbox_deinit(mailbox);
   return false;
}
/* === end verbatim copy === */

static int failures = 0;

/* Helper: reset all counters and inject a failure at one stage. */
static void reset_counters_and_inject(int scond_at, int slock_at, int sthread_at)
{
   fail_scond_after  = scond_at;
   fail_slock_after  = slock_at;
   fail_sthread_after = sthread_at;
   alloc_count_scond = alloc_count_slock = alloc_count_sthread = 0;
   free_count_scond  = free_count_slock  = free_count_sthread  = 0;
}

/* Verify that the mailbox struct ended up in the canonical
 * "deinit'd" shape: all pointer fields NULL, swapchain ==
 * VK_NULL_HANDLE.  This is what vulkan_acquire_next_image's
 * existing `mailbox.swapchain == VK_NULL_HANDLE` guard relies on
 * to skip the emulated path safely. */
static bool check_mailbox_clean(const struct vulkan_emulated_mailbox *m)
{
   return    m->thread    == NULL
          && m->lock      == NULL
          && m->cond      == NULL
          && m->swapchain == VK_NULL_HANDLE;
}

/* Probe: scond_new fails on the very first call.  Pre-fix
 * behaviour: just `return false` with nothing leaked (this
 * stage was already correct).  Post-fix: same observable result
 * via the goto-error path. */
static void test_scond_fails_first(void)
{
   struct vulkan_emulated_mailbox mailbox;
   bool rv;

   reset_counters_and_inject(0, -1, -1);
   memset(&mailbox, 0xAB, sizeof(mailbox));   /* poison to detect zero'ing */

   rv = vulkan_emulated_mailbox_init(&mailbox,
         (VkDevice)0xD000,
         (VkSwapchainKHR)0x5000);

   if (rv)
   {
      printf("[ERROR] scond fails: init returned true\n");
      failures++;
      return;
   }
   if (alloc_count_scond  != 0
       || alloc_count_slock != 0
       || alloc_count_sthread != 0)
   {
      printf("[ERROR] scond fails: unexpected allocations "
            "(scond=%d slock=%d sthread=%d)\n",
            alloc_count_scond, alloc_count_slock, alloc_count_sthread);
      failures++;
      return;
   }
   if (!check_mailbox_clean(&mailbox))
   {
      printf("[ERROR] scond fails: mailbox not in clean state\n");
      failures++;
      return;
   }

   printf("[SUCCESS] scond_new failure: clean state, no leaks\n");
}

/* Probe: slock_new fails on the second call.  Pre-fix this
 * leaked the scond.  Post-fix the goto-error path runs deinit
 * which scond_free's the cond. */
static void test_slock_fails_second(void)
{
   struct vulkan_emulated_mailbox mailbox;
   bool rv;

   reset_counters_and_inject(-1, 0, -1);
   memset(&mailbox, 0xCD, sizeof(mailbox));

   rv = vulkan_emulated_mailbox_init(&mailbox,
         (VkDevice)0xD000,
         (VkSwapchainKHR)0x5000);

   if (rv)
   {
      printf("[ERROR] slock fails: init returned true\n");
      failures++;
      return;
   }
   /* The leak signature: scond was allocated (1) but post-fix
    * must have free'd it via deinit. */
   if (alloc_count_scond != 1)
   {
      printf("[ERROR] slock fails: expected scond alloc=1, got %d\n",
            alloc_count_scond);
      failures++;
      return;
   }
   if (free_count_scond != 1)
   {
      printf("[ERROR] slock fails: expected scond free=1 (would-be leak), got %d\n",
            free_count_scond);
      failures++;
      return;
   }
   if (!check_mailbox_clean(&mailbox))
   {
      printf("[ERROR] slock fails: mailbox not in clean state\n");
      failures++;
      return;
   }

   printf("[SUCCESS] slock_new failure: scond free'd (1 alloc, 1 free), clean state\n");
}

/* Probe: sthread_create fails on the third call.  Pre-fix this
 * leaked both the scond and the slock.  Post-fix deinit free's
 * both. */
static void test_sthread_fails_third(void)
{
   struct vulkan_emulated_mailbox mailbox;
   bool rv;

   reset_counters_and_inject(-1, -1, 0);
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
   if (alloc_count_scond != 1 || free_count_scond != 1)
   {
      printf("[ERROR] sthread fails: scond alloc/free mismatch (%d/%d)\n",
            alloc_count_scond, free_count_scond);
      failures++;
      return;
   }
   if (alloc_count_slock != 1 || free_count_slock != 1)
   {
      printf("[ERROR] sthread fails: slock alloc/free mismatch (%d/%d)\n",
            alloc_count_slock, free_count_slock);
      failures++;
      return;
   }
   if (!check_mailbox_clean(&mailbox))
   {
      printf("[ERROR] sthread fails: mailbox not in clean state\n");
      failures++;
      return;
   }

   printf("[SUCCESS] sthread_create failure: scond+slock free'd (2 allocs, 2 frees), clean state\n");
}

/* Probe: all three allocations succeed.  Init returns true and
 * the mailbox is fully populated.  We then run deinit by hand
 * to clean up so ASan's exit-time leak check stays clean. */
static void test_full_success(void)
{
   struct vulkan_emulated_mailbox mailbox;
   bool rv;

   reset_counters_and_inject(-1, -1, -1);
   memset(&mailbox, 0, sizeof(mailbox));

   rv = vulkan_emulated_mailbox_init(&mailbox,
         (VkDevice)0xD000,
         (VkSwapchainKHR)0x5000);

   if (!rv)
   {
      printf("[ERROR] full success: init returned false\n");
      failures++;
      return;
   }
   if (   mailbox.thread    == NULL
       || mailbox.lock      == NULL
       || mailbox.cond      == NULL
       || mailbox.swapchain != (VkSwapchainKHR)0x5000
       || mailbox.device    != (VkDevice)0xD000)
   {
      printf("[ERROR] full success: mailbox not populated\n");
      failures++;
      return;
   }
   if (alloc_count_scond != 1
       || alloc_count_slock != 1
       || alloc_count_sthread != 1)
   {
      printf("[ERROR] full success: alloc counts wrong "
            "(scond=%d slock=%d sthread=%d)\n",
            alloc_count_scond, alloc_count_slock, alloc_count_sthread);
      failures++;
      return;
   }

   /* Tear down by the same path the production code would use --
    * directly invoke deinit since we can't actually run the
    * mailbox loop in the test. */
   vulkan_emulated_mailbox_deinit(&mailbox);

   if (!check_mailbox_clean(&mailbox))
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

   /* Run all three failure stages, deinit'ing the leftover on
    * the success-stage cases.  After each, leak count should
    * be 0.  Calling __lsan_do_recoverable_leak_check
    * immediately produces the report and returns 1 if leaks
    * are present. */
   reset_counters_and_inject(0, -1, -1);
   (void)vulkan_emulated_mailbox_init(&mailbox, NULL, NULL);

   reset_counters_and_inject(-1, 0, -1);
   (void)vulkan_emulated_mailbox_init(&mailbox, NULL, NULL);

   reset_counters_and_inject(-1, -1, 0);
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

int main(void)
{
   test_scond_fails_first();
   test_slock_fails_second();
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
