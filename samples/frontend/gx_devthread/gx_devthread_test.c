/* The Wii's device thread in frontend/drivers/platform_gx_libogc.c, with
 * libogc's SD and USB disc interfaces and libfat stood in for.
 *
 * Held to: a card put in is mounted, and one taken out unmounted, on the
 * thread's next look; stopping it wakes the thread at once rather than
 * after its once-a-second look.
 *
 * GX_DEVTHREAD_TEST_NO_WAKE leaves the thread unwoken at stop, and the
 * test must fail. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <retro_atomic.h>
#include <rthreads/retro_eventcount.h>
#include <features/features_cpu.h>
#include <retro_timers.h>

#ifdef GX_DEVTHREAD_TEST_NO_WAKE
#define retro_eventcount_notify(ec) ((void)(ec))
#endif
/* The Wii's build, for the frontend's source alone: the libretro-common
 * units it links stay the host's. */
#define GEKKO
#define HW_RVL
#include "frontend/drivers/platform_gx_libogc.c"
#ifdef GX_DEVTHREAD_TEST_NO_WAKE
#undef retro_eventcount_notify
#endif

#define CHECK(c) do { if (!(c)) { \
   fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); \
   exit(1); } } while (0)

/* ---- libogc and libfat ---- */
const devoptab_t *devoptab_list[8];
static retro_atomic_int_t sd_in, usb_in, sd_mounted, usb_mounted;

static bool up(void)        { return true; }
static bool sd_present(void)  { return retro_atomic_load_acquire_int(&sd_in) != 0; }
static bool usb_present(void) { return retro_atomic_load_acquire_int(&usb_in) != 0; }

const DISC_INTERFACE __io_wiisd      = { up, sd_present };
const DISC_INTERFACE __io_usbstorage = { up, usb_present };

bool fatInitDefault(void) { return true; }

bool fatMountSimple(const char *name, const DISC_INTERFACE *interface)
{
   if (!interface->isInserted())
      return false;
   retro_atomic_store_release_int(
         interface == &__io_wiisd ? &sd_mounted : &usb_mounted, 1);
   (void)name;
   return true;
}

void fatUnmount(const char *name)
{
   retro_atomic_store_release_int(
         !strcmp(name, "sd:") ? &sd_mounted : &usb_mounted, 0);
}

int  IOS_GetVersion(void)    { return 58; }
int  IOS_ReloadIOS(int v)    { (void)v; return 0; }
void L2Enhance(void)         { }
void __exception_setreload(int t) { (void)t; }
bool gx_init_mem2(void)      { return true; }

/* ---- the lanes ---- */

/* Waits, at most @seconds, for @cond to hold. */
#define WAIT_FOR(cond, seconds) do { \
   retro_time_t w_ = cpu_features_get_time_usec() + (seconds) * 1000000; \
   while (!(cond) && cpu_features_get_time_usec() < w_) \
      retro_sleep(10); } while (0)

int main(void)
{
   retro_time_t start, took;
   retro_atomic_int_init(&sd_in, 1);
   retro_atomic_int_init(&usb_in, 0);
   retro_atomic_int_init(&sd_mounted, 0);
   retro_atomic_int_init(&usb_mounted, 0);

   frontend_gx_init(NULL);
   CHECK(retro_atomic_load_acquire_int(&sd_mounted)
         && !retro_atomic_load_acquire_int(&usb_mounted));
   CHECK(gx_device_thread != NULL);

   retro_atomic_store_release_int(&usb_in, 1);
   WAIT_FOR(retro_atomic_load_acquire_int(&usb_mounted), 3);
   CHECK(retro_atomic_load_acquire_int(&usb_mounted));
   printf("drive put in: mounted\n");

   retro_atomic_store_release_int(&sd_in, 0);
   WAIT_FOR(!retro_atomic_load_acquire_int(&sd_mounted), 3);
   CHECK(!retro_atomic_load_acquire_int(&sd_mounted));
   printf("card taken out: unmounted\n");

   /* just after a look, the thread waits a whole second */
   retro_sleep(50);
   start = cpu_features_get_time_usec();
   frontend_gx_deinit(NULL);
   took  = cpu_features_get_time_usec() - start;
   printf("stopped in %lld ms\n", (long long)(took / 1000));
   CHECK(took < 500000);
   CHECK(gx_device_thread == NULL);

   printf("PASS: gx device thread\n");
   return 0;
}
