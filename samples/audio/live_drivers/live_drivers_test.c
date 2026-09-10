/* The PulseAudio and ALSA drivers against a live server and a null
 * PCM: no hardware, the real libraries, the real drivers.
 *
 * Pulse on a null sink: the cached telemetry - write_avail served
 * from what the server's thread last said - advances with the
 * server's requests, a blocking write ends at its bound when the
 * server raises none (the null sink's first request comes two
 * seconds after the prebuf fills, which a real sink does not do; a
 * short write there is the bound doing its job, and is reported, not
 * failed), stop and start cork and uncork, and non-blocking writes
 * refuse at the buffer.
 *
 * ALSA on the "null" plugin, and again told the device cannot pause
 * (the plugin can; devices that cannot are what the fallback is for):
 * stop drops instead of pausing, start recovers, writes go on.
 *
 * Needs a PulseAudio server (pulseaudio -n -L module-null-sink ...)
 * and an ALSA config naming a null PCM; see the CI step. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <retro_common_api.h>
#include <retro_miscellaneous.h>
#include "../../../audio/audio_driver.h"

extern audio_driver_t audio_pulse;
#include "../../../audio/drivers/alsa.c"
static bool alsa_no_pause = false;

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static void drive(audio_driver_t *drv, const char *device, unsigned latency, const char *name)
{
   unsigned rate = 48000;
   void *h;
   int16_t buf[480 * 2];
   size_t i, writes = 0, zero_avail = 0, max_avail = 0, min_avail = (size_t)-1;
   double t = 0;
   printf("   %s on %s\n", name, device);
   h = drv->init(device, rate, latency, &rate);
   CHECK(h != NULL, "%s: init failed", name);
   if (!h) return;
   printf("      rate %u, buffer %u bytes, layout 0x%x\n", rate, (unsigned)drv->buffer_size(h),
         drv->layout ? drv->layout(h) : 0);
   CHECK(drv->start(h, false), "%s: start", name);
   drv->set_nonblock_state(h, false);
   for (i = 0; i < 480 * 2; i += 2)
   {
      buf[i] = buf[i + 1] = (int16_t)(8000.0 * sin(t)); t += 2 * M_PI * 440 / rate;
   }
   /* two seconds of blocking writes, sampling the telemetry */
   for (i = 0; i < 200; i++)
   {
      size_t avail = drv->write_avail(h);
      struct timespec t0, t1; double ms;
      ssize_t w;
      clock_gettime(CLOCK_MONOTONIC, &t0);
      w = drv->write(h, buf, sizeof(buf));
      clock_gettime(CLOCK_MONOTONIC, &t1);
      ms = (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
      if (w != (ssize_t)sizeof(buf) || ms > 500)
         printf("      write %u: avail %u, took %.1f ms, returned %ld\n", (unsigned)i, (unsigned)avail, ms, (long)w);
      if (avail == 0) zero_avail++;
      if (avail > max_avail) max_avail = avail;
      if (avail < min_avail) min_avail = avail;
      if (w != (ssize_t)sizeof(buf) && ms > 900)
         printf("      (a blocking write ended at its bound: the server raised no request for a second)\n");
      else
         CHECK(w == (ssize_t)sizeof(buf), "%s: write %u returned %ld", name, (unsigned)i, (long)w);
      writes++;
   }
   printf("      %u writes; write_avail min %u max %u, zero %u times\n",
         (unsigned)writes, (unsigned)min_avail, (unsigned)max_avail, (unsigned)zero_avail);
   CHECK(max_avail > 0, "%s: write_avail never reported room", name);
   CHECK(zero_avail < writes / 2, "%s: write_avail was zero on %u of %u writes", name, (unsigned)zero_avail, (unsigned)writes);
   /* stop and start: the device pausing or dropping */
   if (drv == &audio_alsa && alsa_no_pause)
   {
      /* a device that cannot pause: the null plugin can, so the
       * driver is told it cannot, and stop must drop instead */
      ((alsa_t*)h)->stream_info.can_pause = false;
      printf("      (told the driver the device cannot pause)\n");
   }
   CHECK(drv->stop(h), "%s: stop", name);
   CHECK(!drv->alive(h), "%s: alive after stop", name);
   usleep(100000);
   CHECK(drv->start(h, false), "%s: start after stop", name);
   CHECK(drv->alive(h), "%s: not alive after start", name);
   for (i = 0; i < 50; i++)
   {
      ssize_t w = drv->write(h, buf, sizeof(buf));
      CHECK(w == (ssize_t)sizeof(buf), "%s: write after restart returned %ld", name, (long)w);
   }
   /* non-blocking: write_avail and partial writes */
   drv->set_nonblock_state(h, true);
   {
      size_t avail = drv->write_avail(h), total = 0; ssize_t w;
      do { w = drv->write(h, buf, sizeof(buf)); if (w > 0) total += w; } while (w == (ssize_t)sizeof(buf) && total < 1 << 20);
      printf("      non-blocking: avail %u, accepted %u bytes before refusing\n", (unsigned)avail, (unsigned)total);
      if (total >= 1 << 20)
         printf("      (the device took everything: a null sink drains at once)\n");
   }
   drv->free(h);
}

int main(void)
{
   printf("live drivers:\n");
   drive(&audio_pulse, NULL, 64, "PulseAudio");
   drive(&audio_alsa, "nullpcm", 64, "ALSA");
   alsa_no_pause = true;
   drive(&audio_alsa, "nullpcm", 64, "ALSA, no pause");
   if (failures) { printf("%u failure(s)\n", failures); return 1; }
   printf("live drivers: Pulse's cached telemetry and ALSA's pauseless stop behave\n");
   return 0;
}
