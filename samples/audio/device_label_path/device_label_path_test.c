/* Regression test: a device list entry with the hardware's name after
 * the path - "/dev/dsp0 (HDA Intel)", as oss and audioio list their
 * nodes - is stored by the menu as shown and comes back to init as the
 * device string. oss opened that string as a path and failed; the
 * device the user had just picked from the list could not be used.
 *
 * What this pins: audio_device_label_path() takes the path off the
 * front of an entry and leaves a bare path alone; and both drivers
 * reach open() with the path, not the entry. Each driver is compiled
 * whole with open() redirected to a recorder that refuses, so init
 * stops right there and no device is touched. */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>      /* before open is renamed below */

static char opened[256];
static int  opens;

static int record_open(const char *path, int flags, ...)
{
   (void)flags;
   strncpy(opened, path, sizeof(opened) - 1);
   opens++;
   errno = ENOENT;
   return -1;
}

/* Both drivers, with open() ours. Everything else they call is the C
 * library, and they call none of it before open() fails. fcntl.h is
 * included above, so its own open is declared before the rename. */
#define open record_open
#include "audio/drivers/oss.c"
#include "audio/drivers/audioio.c"
#undef open

#include "audio/audio_device_label.h"

/* What the drivers reach for in RetroArch. */
void RARCH_LOG(const char *f, ...)  { (void)f; }
void RARCH_WARN(const char *f, ...) { (void)f; }
void RARCH_ERR(const char *f, ...)  { (void)f; }
uint32_t audio_driver_requested_layout(void) { return AUDIO_LAYOUT_STEREO; }
unsigned audio_layout_channels(uint32_t layout) { (void)layout; return 2; }

static unsigned failures;

static void check(int cond, const char *what)
{
   printf("  [%s] %s\n", cond ? "pass" : "FAIL", what);
   if (!cond)
      failures++;
}

static void expect_open(audio_driver_t *drv, const char *entry,
      const char *path, const char *what)
{
   unsigned new_rate = 0;
   void *ctx;
   opened[0] = '\0';
   opens     = 0;
   ctx       = drv->init(entry, 48000, 64, &new_rate);
   check(!ctx && opens == 1 && !strcmp(opened, path), what);
   if (ctx)
      drv->free(ctx);
}

int main(void)
{
   char buf[64];

   printf("audio_device_label_path\n");
   audio_device_label_path(buf, sizeof(buf), "/dev/dsp0 (HDA Intel)");
   check(!strcmp(buf, "/dev/dsp0"), "an entry with a name gives the path");
   audio_device_label_path(buf, sizeof(buf), "/dev/dsp");
   check(!strcmp(buf, "/dev/dsp"), "a bare path is copied as it is");
   audio_device_label_path(buf, sizeof(buf), "/dev/audio1 (a (b) c)");
   check(!strcmp(buf, "/dev/audio1"), "cut at the first bracket, whatever follows");
   audio_device_label_path(buf, 6, "/dev/dsp0 (x)");
   check(!strcmp(buf, "/dev/"), "a short buffer truncates and terminates");

   printf("oss\n");
   expect_open(&audio_oss, "/dev/dsp3 (HDA Intel PCH)", "/dev/dsp3",
         "init opens the path of a list entry");
   expect_open(&audio_oss, "/dev/dsp1", "/dev/dsp1",
         "init opens a bare path as it is");
   expect_open(&audio_oss, NULL, DEFAULT_OSS_DEV,
         "no device: the default");

   printf("audioio\n");
   expect_open(&audio_audioio, "/dev/audio2 (hdafg0)", "/dev/audio2",
         "init opens the path of a list entry");
   expect_open(&audio_audioio, "/dev/audio0", "/dev/audio0",
         "init opens a bare path as it is");
   expect_open(&audio_audioio, NULL, "/dev/audio",
         "no device: the default");

   printf("%u failure%s\n", failures, failures == 1 ? "" : "s");
   return failures ? 1 : 0;
}
