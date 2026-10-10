/* The PulseAudio and ALSA drivers against a live server and a null
 * PCM: no hardware, the real libraries, the real drivers.
 *
 * Pulse on a null sink: the cached telemetry - write_avail served
 * from what the server's thread last said - advances with the
 * server's requests, the writer takes the mainloop lock on none of
 * its writes, write_avail and wait_writable calls (it hands audio to
 * the server's thread through a ring; the driver's calls to the lock
 * are counted at the link boundary), a blocking write ends at its
 * bound when the
 * server raises none (the null sink's first request comes two
 * seconds after the prebuf fills, which a real sink does not do; a
 * short write there is the bound doing its job, and is reported, not
 * failed), stop and start cork and uncork, and non-blocking writes
 * refuse at the buffer. Before any audio neither driver reports a
 * device clock, and one reported later is a bounded number.
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
#include "../../../configuration.h"

extern audio_driver_t audio_pulse;

/* The driver's calls to the mainloop lock, counted while the writer
 * runs. libpulse's own thread locks its mutex directly, not through
 * this symbol, so only the driver's calls reach it. */
typedef struct pa_threaded_mainloop pa_threaded_mainloop;
void __real_pa_threaded_mainloop_lock(pa_threaded_mainloop *m);
static volatile int counting_locks = 0;
static unsigned writer_locks       = 0;
void __wrap_pa_threaded_mainloop_lock(pa_threaded_mainloop *m)
{
   if (counting_locks)
      writer_locks++;
   __real_pa_threaded_mainloop_lock(m);
}
#include "../../../audio/drivers/alsa.c"
static bool alsa_no_pause = false;
static bool alsa_expect_mmap = false;

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
   if (drv == &audio_alsa)
   {
      printf("      access: %s\n", ((alsa_t*)h)->stream_info.mmap ? "mmap" : "read/write");
      CHECK(((alsa_t*)h)->stream_info.mmap == alsa_expect_mmap,
            "%s: opened %s, expected %s", name,
            ((alsa_t*)h)->stream_info.mmap ? "mmap" : "read/write",
            alsa_expect_mmap ? "mmap" : "read/write");
   }
   /* No estimate before the device has played a second: the clock
    * word starts out saying so, not as a reading of 0 ppm. */
   if (drv->device_clock_ppm)
   {
      double ppm = 12345.0;
      CHECK(!drv->device_clock_ppm(h, &ppm) && ppm == 12345.0,
            "%s: a device clock of %+.0f ppm before any audio", name, ppm);
   }
   CHECK(drv->start(h, false), "%s: start", name);
   drv->set_nonblock_state(h, false);
   for (i = 0; i < 480 * 2; i += 2)
   {
      buf[i] = buf[i + 1] = (int16_t)(8000.0 * sin(t)); t += 2 * M_PI * 440 / rate;
   }
   /* two seconds of blocking writes, sampling the telemetry */
   counting_locks = (drv == &audio_pulse);
   writer_locks   = 0;
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
      if (drv->wait_writable && (i % 20) == 0)
         drv->wait_writable(h, 480 * 2 * sizeof(int16_t));
   }
   counting_locks = 0;
   if (drv == &audio_pulse)
   {
      printf("      mainloop lock taken %u times by 200 writes\n",
            writer_locks);
      CHECK(writer_locks == 0,
            "%s: the writer took the mainloop lock %u times", name,
            writer_locks);
   }
   printf("      %u writes; write_avail min %u max %u, zero %u times\n",
         (unsigned)writes, (unsigned)min_avail, (unsigned)max_avail, (unsigned)zero_avail);
   CHECK(max_avail > 0, "%s: write_avail never reported room", name);
   if (drv->device_clock_ppm)
   {
      double ppm = 12345.0;
      if (drv->device_clock_ppm(h, &ppm))
      {
         printf("      device clock %+.0f ppm\n", ppm);
         CHECK(ppm > -100000.0 && ppm < 100000.0,
               "%s: device clock %+.0f ppm", name, ppm);
      }
      else
         CHECK(ppm == 12345.0, "%s: no device clock, but *ppm was written", name);
   }
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
   /* The lend pair: a span of the driver's own buffer, produced into
    * and published, with no mainloop lock on the writer's side; none
    * while stopped, where write() restarts the stream instead. */
   if (drv->write_begin && drv->write_end)
   {
      unsigned lent = 0, short_spans = 0;
      size_t   want = sizeof(buf);
      void    *region;
      counting_locks = (drv == &audio_pulse);
      writer_locks   = 0;
      for (i = 0; i < 100; i++)
      {
         size_t got;
         if (drv->wait_writable)
            drv->wait_writable(h, want);
         got = drv->write_begin(h, want, &region);
         CHECK((got == 0) == (region == NULL),
               "%s: lend %u: span %u with region %p", name, (unsigned)i,
               (unsigned)got, region);
         if (!region)
            continue;
         CHECK(got <= want && got % (drv->use_float(h) ? 8 : 4) == 0,
               "%s: lend %u: span %u for %u asked", name, (unsigned)i,
               (unsigned)got, (unsigned)want);
         memcpy(region, buf, got);
         CHECK(drv->write_end(h, got) == (ssize_t)got,
               "%s: lend %u: not all of it published", name, (unsigned)i);
         if (got < want)
            short_spans++;
         lent++;
      }
      counting_locks = 0;
      printf("      lend: %u of 100 spans lent (%u cut at the wrap), mainloop lock taken %u times\n",
            lent, short_spans, writer_locks);
      if (drv == &audio_alsa && !((alsa_t*)h)->stream_info.mmap)
         CHECK(lent == 0, "%s: a read/write PCM lent %u spans", name, lent);
      else
      {
         CHECK(lent > 50, "%s: only %u of 100 lends", name, lent);
         if (drv == &audio_alsa)
            CHECK(snd_pcm_state(((alsa_t*)h)->pcm) == SND_PCM_STATE_RUNNING,
                  "%s: committed spans did not start the stream (%s)", name,
                  snd_pcm_state_name(snd_pcm_state(((alsa_t*)h)->pcm)));
      }
      CHECK(writer_locks == 0, "%s: the lend took the mainloop lock %u times",
            name, writer_locks);
      CHECK(drv->stop(h), "%s: stop before a lend", name);
      CHECK(drv->write_begin(h, want, &region) == 0 && !region,
            "%s: a stopped stream lent", name);
      CHECK(drv->start(h, false), "%s: start after the lend check", name);
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

/* What a lent span carries reaches the device as written: a plug over
 * a file plugin records every committed frame, so a numbered pattern
 * produced through the lend must come back out of the file in order. */
static void lend_bytes(const char *device, const char *path)
{
   unsigned rate = 48000, i;
   uint32_t next = 0, check = 0, word;
   size_t   frame, total = 0;
   void    *h, *region;
   FILE    *f;
   printf("   ALSA lend, bytes through %s\n", device);
   remove(path);
   config_get_ptr()->bools.audio_threaded_pipeline = true;
   h = audio_alsa.init(device, rate, 64, &rate);
   CHECK(h != NULL, "lend bytes: init failed");
   if (!h) return;
   CHECK(((alsa_t*)h)->stream_info.mmap, "lend bytes: not opened for mmap");
   audio_alsa.start(h, false);
   frame = ((alsa_t*)h)->stream_info.frame_bits / 8;
   for (i = 0; i < 400 && total < 96000 * frame / 2; i++)
   {
      size_t got, k;
      audio_alsa.wait_writable(h, 1024 * frame);
      got = audio_alsa.write_begin(h, 1024 * frame, &region);
      if (!region)
         continue;
      for (k = 0; k + 4 <= got; k += 4)
      {
         word = next++;
         memcpy((uint8_t*)region + k, &word, 4);
      }
      total += (size_t)audio_alsa.write_end(h, got);
   }
   audio_alsa.free(h);
   config_get_ptr()->bools.audio_threaded_pipeline = false;
   CHECK(total > 0, "lend bytes: nothing lent");
   if (!(f = fopen(path, "rb")))
   {
      CHECK(0, "lend bytes: %s not written", path);
      return;
   }
   while (fread(&word, 4, 1, f) == 1 && check < next)
   {
      if (word != check)
      {
         CHECK(0, "lend bytes: word %u reads %u", check, word);
         break;
      }
      check++;
   }
   fclose(f);
   remove(path);
   printf("      %u of %u lent words read back in order\n", check, next);
   CHECK(check > 0, "lend bytes: the file holds none of it");
}

int main(void)
{
   printf("live drivers:\n");
   drive(&audio_pulse, NULL, 64, "PulseAudio");
   drive(&audio_alsa, "nullpcm", 64, "ALSA");
   alsa_no_pause = true;
   drive(&audio_alsa, "nullpcm", 64, "ALSA, no pause");
   alsa_no_pause = false;
   /* The threaded pipeline: a plug PCM opens for mmap and lends its
    * buffer; the null PCM itself is no hw or plug, and does not. */
   config_get_ptr()->bools.audio_threaded_pipeline = true;
   drive(&audio_alsa, "nullpcm", 64, "ALSA, threaded pipeline, null");
   alsa_expect_mmap = true;
   drive(&audio_alsa, "plugnull", 64, "ALSA, threaded pipeline, plug");
   alsa_no_pause = true;
   drive(&audio_alsa, "plugnull", 64, "ALSA, threaded pipeline, plug, no pause");
   alsa_no_pause    = false;
   alsa_expect_mmap = false;
   config_get_ptr()->bools.audio_threaded_pipeline = false;
   drive(&audio_alsa, "plugnull", 64, "ALSA, plug, pipeline off");
   lend_bytes("plugfile", "/tmp/ra_live_drivers_lend.raw");
   if (failures) { printf("%u failure(s)\n", failures); return 1; }
   printf("live drivers: Pulse's cached telemetry and ALSA's pauseless stop behave\n");
   return 0;
}
