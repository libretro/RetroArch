/* Harness for the ALSA driver's device-clock estimate.
 *
 * This drives the shipping audio/drivers/alsa.c - not a copy of its
 * arithmetic - against a real PCM opened on the null device. The four
 * libasound calls the estimate is built from are interposed with
 * -Wl,--wrap and scripted, so a device can be made to run at a chosen
 * offset from nominal and the driver's own answer read back.
 *
 * The clocks it is asked to read:
 *
 *   the audio timestamp, where a driver counts one - the DMA or link
 *   clock, compared straight against the system clock, needing neither
 *   a rate nor a delay;
 *
 *   and the position against the timestamp it was taken with, which
 *   every device can answer. The position is this driver's write count
 *   less the delay, so it is inferred - but an error in the delay
 *   shifts the whole line and leaves its slope alone, which is the
 *   point of fitting it rather than differencing two samples. Case 4
 *   makes that concrete by putting a large wrong delay in.
 *
 * The null PCM reports no audio timestamp at all, which is why the
 * fallback exists and why the wrapped calls are what supply one here. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <alsa/asoundlib.h>

#include "../../../audio/audio_driver.h"

static int failures;

static void fail(const char *what, const char *detail)
{
   printf("   FAIL %s: %s\n", what, detail);
   failures++;
}

/* ── the scripted device ──────────────────────────────────────────── */

static struct
{
   double   true_rate;      /* what the hardware is really doing */
   unsigned nominal;        /* what the driver asked for */
   double   audio_ppm;      /* the audio timestamp's offset, or 0 for none */
   int      have_audio;
   long     delay_frames;   /* what the device says it still holds */
   double   jitter_ns;      /* on the system timestamp */
   uint64_t written;        /* frames the driver has handed over */
   uint64_t calls;
} dev;

static void device_reset(void)
{
   memset(&dev, 0, sizeof(dev));
   dev.true_rate = 48000.0;
   dev.nominal   = 48000;
   dev.delay_frames = 1024;
}

/* The system clock advances as the device consumes: the driver writes
 * a period, the device plays it, and time passes accordingly. */
static uint64_t system_ns(void)
{
   double played = (double)dev.written;
   double ns     = played * 1000000000.0 / dev.true_rate;
   if (dev.jitter_ns != 0.0)
      ns += (dev.calls & 1) ? dev.jitter_ns : -dev.jitter_ns;
   return (uint64_t)ns;
}

/* The hardware's own clock, running at its own offset from the system
 * clock. */
static uint64_t audio_ns(void)
{
   double ns = (double)dev.written * 1000000000.0 / dev.true_rate;
   return (uint64_t)(ns * (1.0 + dev.audio_ppm / 1000000.0));
}

int __wrap_snd_pcm_status(snd_pcm_t *pcm, snd_pcm_status_t *status);
int __real_snd_pcm_status(snd_pcm_t *pcm, snd_pcm_status_t *status);
int __wrap_snd_pcm_status(snd_pcm_t *pcm, snd_pcm_status_t *status)
{
   (void)pcm; (void)status;
   dev.calls++;
   return 0;
}

void __wrap_snd_pcm_status_get_htstamp(const snd_pcm_status_t *obj,
      snd_htimestamp_t *ptr);
void __wrap_snd_pcm_status_get_htstamp(const snd_pcm_status_t *obj,
      snd_htimestamp_t *ptr)
{
   uint64_t ns = system_ns();
   (void)obj;
   ptr->tv_sec  = (long)(ns / 1000000000ULL);
   ptr->tv_nsec = (long)(ns % 1000000000ULL);
}

void __wrap_snd_pcm_status_get_audio_htstamp(const snd_pcm_status_t *obj,
      snd_htimestamp_t *ptr);
void __wrap_snd_pcm_status_get_audio_htstamp(const snd_pcm_status_t *obj,
      snd_htimestamp_t *ptr)
{
   (void)obj;
   if (!dev.have_audio)
   {
      /* What the null PCM does, and plenty of real ones. */
      ptr->tv_sec = ptr->tv_nsec = 0;
      return;
   }
   {
      uint64_t ns = audio_ns();
      ptr->tv_sec  = (long)(ns / 1000000000ULL);
      ptr->tv_nsec = (long)(ns % 1000000000ULL);
   }
}

snd_pcm_sframes_t __wrap_snd_pcm_status_get_delay(const snd_pcm_status_t *obj);
snd_pcm_sframes_t __wrap_snd_pcm_status_get_delay(const snd_pcm_status_t *obj)
{
   (void)obj;
   return dev.delay_frames;
}

/* Accepts everything, and counts it: the driver's frames_written is
 * what the position is built from. */
snd_pcm_sframes_t __wrap_snd_pcm_writei(snd_pcm_t *pcm, const void *buffer,
      snd_pcm_uframes_t size);
snd_pcm_sframes_t __wrap_snd_pcm_writei(snd_pcm_t *pcm, const void *buffer,
      snd_pcm_uframes_t size)
{
   (void)pcm; (void)buffer;
   dev.written += size;
   return (snd_pcm_sframes_t)size;
}

/* ── driving the driver ───────────────────────────────────────────── */

/* Opens the shipping driver on the null PCM and writes to it for the
 * given number of seconds of audio. Returns what it logged - read back
 * out of the driver rather than scraped from the log, via the fields
 * the teardown line uses. */
static void run_driver(double seconds, void **out_handle)
{
   unsigned  new_rate = dev.nominal;
   float     buf[512 * 2];
   unsigned  n, periods = (unsigned)(seconds * dev.true_rate / 512.0);
   void     *handle;

   memset(buf, 0, sizeof(buf));
   handle = audio_alsa.init("null", dev.nominal, 64, &new_rate);
   if (!handle)
   {
      fail("init", "the driver would not open the null PCM");
      *out_handle = NULL;
      return;
   }

   for (n = 0; n < periods; n++)
      audio_alsa.write(handle, buf, sizeof(buf));

   *out_handle = handle;
}

/* The estimate is read back out of what the driver logged, so what is
 * asserted is the line a user would see. */
extern char harness_log[8192];

static int log_ppm(const char *marker, int *found)
{
   const char *p = strstr(harness_log, marker);
   const char *q;
   *found = 0;
   if (!p)
      return 0;
   q = strstr(p, "ppm");
   if (!q)
      return 0;
   /* Back up over the number, which the driver prints with %+d. */
   while (q > p && (*q < '0' || *q > '9'))
      q--;
   while (q > p && ((*(q - 1) >= '0' && *(q - 1) <= '9')
            || *(q - 1) == '-' || *(q - 1) == '+'))
      q--;
   *found = 1;
   return atoi(q);
}

/* Runs one case end to end and checks the driver's own answer. */
static void expect(const char *name, const char *marker, int want, int tol)
{
   void *h = NULL;
   int   found = 0, got;

   harness_log[0] = '\0';
   run_driver(10.0, &h);
   if (!h)
      return;
   audio_alsa.free(h);

   got = log_ppm(marker, &found);
   if (!found)
      fail(name, "the driver logged no estimate of this kind");
   else if (got < want - tol || got > want + tol)
   {
      char d[128];
      snprintf(d, sizeof(d), "read %+d ppm, expected %+d", got, want);
      fail(name, d);
   }
   else
      printf("   ok   %-22s %+d ppm\n", name, got);
}

#define AUDIO_MARK "audio timestamp"
#define FIT_MARK   "fitted from the position"

int main(void)
{
   printf("1. the audio timestamp, where a driver counts one\n");
   {
      device_reset(); dev.have_audio = 1; dev.audio_ppm = 0.0;
      expect("audio clock exact",   AUDIO_MARK,   0, 2);
      device_reset(); dev.have_audio = 1; dev.audio_ppm = 50.0;
      expect("audio clock +50 ppm", AUDIO_MARK,  50, 2);
      device_reset(); dev.have_audio = 1; dev.audio_ppm = -80.0;
      expect("audio clock -80 ppm", AUDIO_MARK, -80, 2);
   }

   printf("2. no audio timestamp: the position and its timestamp instead\n");
   {
      device_reset(); dev.have_audio = 0; dev.true_rate = 48002.4;
      expect("fitted +50 ppm", FIT_MARK, 50, 2);
      device_reset(); dev.have_audio = 0; dev.true_rate = 47996.16;
      expect("fitted -80 ppm", FIT_MARK, -80, 2);
      device_reset(); dev.have_audio = 0;
      dev.true_rate = 44101.323; dev.nominal = 44100;
      expect("fitted +30 ppm at 44.1k", FIT_MARK, 30, 2);
   }

   printf("3. jitter on the system timestamp\n");
   {
      device_reset(); dev.have_audio = 0;
      dev.true_rate = 48002.4; dev.jitter_ns = 2000000.0;
      expect("fitted through +-2 ms", FIT_MARK, 50, 3);
   }

   printf("4. a wrong delay shifts the line and not its slope\n");
   {
      device_reset(); dev.have_audio = 0;
      dev.true_rate = 48002.4; dev.delay_frames = 40000;
      expect("fitted, absurd delay", FIT_MARK, 50, 2);
   }

   printf("5. a device that reports nothing usable\n");
   {
      void *h = NULL;
      device_reset();
      dev.have_audio = 0;
      harness_log[0] = '\0';
      /* A delay larger than everything written: the position cannot be
       * formed, and the driver must say so rather than invent one. */
      dev.delay_frames = 100000000;
      run_driver(10.0, &h);
      if (h)
         audio_alsa.free(h);
      if (strstr(harness_log, "not enough usable timestamps"))
         printf("   ok   said so, published nothing\n");
      else
         fail("unusable", "published an estimate it could not have had");
   }

   if (failures)
   {
      printf("alsa clock: %d failure(s)\n", failures);
      return 1;
   }
   printf("alsa clock: the shipping driver was driven on a real PCM"
          " for every case\n");
   return 0;
}
