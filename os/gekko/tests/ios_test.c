/* Wii IOS checks, run in Dolphin by os/gekko/tests/run-dolphin.sh:
 * the IPC mailbox (sync, async, queued), settings, the STM event
 * hook, and power off at the end. */

#include <stdio.h>
#include <string.h>

#include <gekko/conf.h>
#include <gekko/ios.h>
#include <gekko/usb.h>
#include <gekko/power.h>
#include <gekko/thread.h>

static unsigned failures;

#define CHECK(cond, what) \
   do { \
      if (cond) \
         gk_debug_printf("ok   %s", what); \
      else \
      { \
         gk_debug_printf("FAIL %s", what); \
         failures++; \
      } \
   } while (0)

static volatile uint32_t async_done;
static volatile int32_t  async_result[16];

static void async_cb(int32_t result, void *data)
{
   async_result[(intptr_t)data] = result;
   async_done++;
   gk_futex_wake(&async_done, 1);
}

static void test_mailbox(void)
{
   static uint32_t out[16][8] __attribute__((aligned(32)));
   int32_t fd, bad, fds[16];
   int i;

   fd  = gk_ios_open("/dev/stm/immediate", 0);
   bad = gk_ios_open("/dev/does/not/exist", 0);
   CHECK(fd >= 0 && bad < 0, "ipc: open succeeds, a missing device fails");

   /* Many threads at once queue behind the mailbox. */
   for (i = 0; i < 16; i++)
      fds[i] = gk_ios_ioctl_async(fd, 0x7001 /* read version */, NULL, 0,
            out[i], sizeof(out[i]), async_cb, (void*)(intptr_t)i);
   while (async_done < 16)
      gk_futex_wait(&async_done, async_done, GK_US_TO_TICKS(1000000));
   for (i = 0; i < 16; i++)
      if (fds[i] != 0 || async_result[i] < 0)
         break;
   CHECK(i == 16, "ipc: 16 asynchronous requests queued and completed");
   CHECK(gk_ios_close(fd) == 0, "ipc: close");
}

static volatile int hook_events;

static void on_button(void *data)
{
   (void)data;
   hook_events++;
}

static void test_hook(void)
{
   /* Arm and release the button hook: release completes it with no
    * event, which must not reach the callbacks. */
   gk_power_set_callbacks(on_button, on_button, NULL);
   gk_sleep_us(10000);
   gk_power_set_callbacks(NULL, NULL, NULL);
   gk_sleep_us(10000);
   CHECK(hook_events == 0, "stm: the button hook arms and releases quietly");
}

static void test_conf(void)
{
   int wide = gk_conf_wide(), pgs = gk_conf_progressive();
   int e60 = gk_conf_eurgb60(), video = gk_conf_video();
   int dh  = gk_conf_display_offset_h();
   gk_debug_printf("     conf: wide %d progressive %d eurgb60 %d video %d dh %d",
         wide, pgs, e60, video, dh);
   CHECK(wide >= 0 && pgs >= 0 && e60 >= 0, "conf: SYSCONF read and parsed");
   CHECK(video >= 0, "conf: setting.txt read and decrypted");
   CHECK(dh >= -32 && dh <= 32, "conf: display offset in range");
}

/* A fresh IOS of the same number, where it is installed (an emulator
 * without the title refuses); the tests after it run on whichever. */
static void test_reload(void)
{
   uint32_t major = gk_ios_version() >> 16;
   uint64_t t0;
   int ret;
   t0  = gk_ticks();
   ret = gk_ios_reload(major);
   gk_debug_printf("     reload %s after %u ms", ret ? "refused" : "done",
         (unsigned)(gk_ticks_to_us(gk_ticks() - t0) / 1000));
   CHECK((gk_ios_version() >> 16) == major,
         "reload: the same IOS, started over or still running");
   CHECK(gk_usb_supported(), "reload: requests go through after it");
}

static void test_lomem(void)
{
   CHECK(gk_ios_version() >> 16,
         "lomem: the loader's globals outlive the load");
}

int main(int argc, char **argv)
{
   (void)argc;
   (void)argv;
   gk_debug_printf("ios test: IOS %u.%u", (unsigned)(gk_ios_version() >> 16),
         (unsigned)(gk_ios_version() & 0xffff));
   test_lomem();
   test_reload();
   test_mailbox();
   test_conf();
   test_hook();
   gk_debug_printf("%s (%u failure(s))", failures ? "FAILED" : "PASSED",
         failures);
   gk_power_off();
   return 0;
}
