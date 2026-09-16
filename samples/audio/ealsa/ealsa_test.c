/* The embedded ALSA driver in audio/drivers/alsa.c against a scripted
 * PCM device.
 *
 * It talks to the kernel's PCM ABI through one seam - open, ioctl,
 * close, poll - so a device that answers the way a card does is
 * enough to run it here, and unlike a card it can be told to refuse a
 * rate, a format, a channel count or a period size. That negotiation
 * is the part with nothing else to check it. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <string/stdstring.h>
#include "ealsa_mock.h"

/* the seam, before the driver is included */
#define EALSA_SYSCALLS 1
#define ealsa_open_dev(path, flags) ealsa_mock_open((path), (flags))
#define ealsa_ioctl(fd, req, arg)   ealsa_mock_ioctl((fd), (unsigned long)(req), (void*)(arg))
#define ealsa_close(fd)             ealsa_mock_close((fd))
#define ealsa_poll(fds, n, timeout) ealsa_mock_poll((void*)(fds), (n), (timeout))

#include "../../../audio/drivers/alsa.c"

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static uint32_t want_layout = AUDIO_LAYOUT_STEREO;
uint32_t audio_driver_requested_layout(void) { return want_layout; }

static uint8_t frame[8192];

static void caps_default(ealsa_mock_caps_t *c)
{
   memset(c, 0, sizeof(*c));
   c->rate_min = 8000;  c->rate_max = 192000;
   c->channels_min = 1; c->channels_max = 8;
   c->allow_float = 1;  c->allow_s16 = 1;
   c->period_min = 32;  c->period_max = 8192;
   c->period_granularity = 1;
   c->can_pause = 1;
}

int main(void)
{
   audio_driver_t *drv = &audio_tinyalsa;
   void *h;
   unsigned new_rate;
   ealsa_mock_caps_t c;
   size_t i;

   printf("embedded alsa:\n");

   printf("   a cooperative card: the asked-for rate, float, stereo\n");
   caps_default(&c);
   ealsa_mock_reset(&c);
   new_rate = 0;
   h = drv->init("0,0", 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed");
   if (!h) { printf("%u failure(s)\n", failures); return 1; }
   printf("      device took %u Hz, %u channels, format %d, period %u, buffer %u frames\n",
         ealsa_mock_rate(), ealsa_mock_channels(), ealsa_mock_format(),
         ealsa_mock_period(), ealsa_mock_buffer());
   CHECK(ealsa_mock_rate() == 48000, "the device was set to %u Hz", ealsa_mock_rate());
   CHECK(new_rate == 48000, "new_rate is %u", new_rate);
   CHECK(drv->use_float(h), "float was available and not taken");
   CHECK(drv->layout(h) == AUDIO_LAYOUT_STEREO, "layout is 0x%x", drv->layout(h));
   CHECK(ealsa_mock_channels() == 2, "%u channels opened", ealsa_mock_channels());
   /* the buffer has to hold the latency that was asked for: a card
    * settles an interval at its minimum, so a request that is only a
    * range opens the smallest buffer the card has */
   printf("      64 ms asked for; the buffer holds %.1f ms\n",
         ealsa_mock_buffer() * 1000.0 / 48000.0);
   CHECK(ealsa_mock_buffer() * 1000 / 48000 >= 48,
         "a 64 ms request opened a %u-frame buffer, %.1f ms",
         ealsa_mock_buffer(), ealsa_mock_buffer() * 1000.0 / 48000.0);
   /* the driver's idea of the buffer must be the device's */
   CHECK(drv->buffer_size(h) == (size_t)ealsa_mock_buffer() * 2 * sizeof(float),
         "buffer_size reports %u bytes, the device took %u frames of 8",
         (unsigned)drv->buffer_size(h), ealsa_mock_buffer());
   /* the software parameters: never auto-started, never auto-stopped */
   printf("      start threshold %lu (buffer %u), stop threshold %lu, boundary %lu\n",
         ealsa_mock_start_threshold(), ealsa_mock_buffer(),
         ealsa_mock_stop_threshold(), ealsa_mock_boundary());
   CHECK(ealsa_mock_start_threshold() > ealsa_mock_buffer(),
         "the kernel would start the stream by itself");
   CHECK(ealsa_mock_boundary() > 0 && ealsa_mock_boundary() % ealsa_mock_buffer() == 0,
         "the boundary %lu is not a multiple of the buffer %u",
         ealsa_mock_boundary(), ealsa_mock_buffer());
   CHECK(ealsa_mock_stop_threshold() >= ealsa_mock_boundary(),
         "the stream would stop on an underrun");
   drv->free(h);
   CHECK(ealsa_mock_open_fds() == 0, "free left %d descriptor(s) open", ealsa_mock_open_fds());

   printf("   a card with one rate: the frontend is told what it got\n");
   caps_default(&c);
   c.rate_min = c.rate_max = 44100;
   ealsa_mock_reset(&c);
   new_rate = 0;
   h = drv->init("0,0", 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed on a 44.1 kHz-only card");
   if (h)
   {
      printf("      asked 48000, device took %u, frontend told %u\n", ealsa_mock_rate(), new_rate);
      CHECK(ealsa_mock_rate() == 44100, "the device runs at %u", ealsa_mock_rate());
      CHECK(new_rate == 44100, "the frontend was told %u: everything would play at the wrong pitch", new_rate);
      drv->free(h);
   }

   printf("   a card without float: s16, and the frame size follows\n");
   caps_default(&c);
   c.allow_float = 0;
   ealsa_mock_reset(&c);
   h = drv->init("0,0", 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed on an s16-only card");
   if (h)
   {
      CHECK(!drv->use_float(h), "float was reported on a card that refused it");
      CHECK(drv->buffer_size(h) == (size_t)ealsa_mock_buffer() * 2 * sizeof(int16_t),
            "buffer_size %u does not match %u s16 stereo frames",
            (unsigned)drv->buffer_size(h), ealsa_mock_buffer());
      drv->free(h);
   }

   printf("   a card that counts periods its own way\n");
   caps_default(&c);
   c.period_min = 512; c.period_max = 4096; c.period_granularity = 512;
   c.periods_fixed = 2;
   ealsa_mock_reset(&c);
   h = drv->init("0,0", 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed on a card with its own period sizes (device refused: %s)", ealsa_mock_refine_why());
   if (h)
   {
      printf("      asked a 768-frame period and 4 of them; device gave %u x %u = %u frames\n",
            ealsa_mock_period(), ealsa_mock_buffer() / (ealsa_mock_period() ? ealsa_mock_period() : 1),
            ealsa_mock_buffer());
      CHECK(ealsa_mock_period() % 512 == 0, "the period %u is not the device's multiple", ealsa_mock_period());
      CHECK(ealsa_mock_buffer() * 1000 / 48000 >= 32,
            "on a card with its own grid a 64 ms request opened %.1f ms",
            ealsa_mock_buffer() * 1000.0 / 48000.0);
      /* the driver's own numbers have to be the device's, or every
       * byte count it reports is wrong */
      CHECK(drv->buffer_size(h) == (size_t)ealsa_mock_buffer() * 2 * sizeof(float),
            "buffer_size %u against the device's %u frames",
            (unsigned)drv->buffer_size(h), ealsa_mock_buffer());
      CHECK(drv->write_avail(h) == drv->buffer_size(h),
            "an empty device reports %u of %u bytes free",
            (unsigned)drv->write_avail(h), (unsigned)drv->buffer_size(h));
      drv->free(h);
   }

   printf("   a stereo-only card with 5.1 asked for: stereo, and it says so\n");
   caps_default(&c);
   c.channels_max = 2;
   ealsa_mock_reset(&c);
   want_layout = AUDIO_LAYOUT_5POINT1;
   h = drv->init("0,0", 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed when the layout was refused");
   if (h)
   {
      CHECK(ealsa_mock_channels() == 2, "%u channels opened", ealsa_mock_channels());
      CHECK(drv->layout(h) == AUDIO_LAYOUT_STEREO,
            "the driver reports 0x%x on a stereo device", drv->layout(h));
      drv->free(h);
   }

   printf("   a 5.1 card with 5.1 asked for: six channels\n");
   caps_default(&c);
   ealsa_mock_reset(&c);
   h = drv->init("0,0", 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed on a 5.1 card");
   if (h)
   {
      CHECK(ealsa_mock_channels() == 6, "%u channels opened", ealsa_mock_channels());
      CHECK(drv->layout(h) == AUDIO_LAYOUT_5POINT1, "the driver reports 0x%x", drv->layout(h));
      CHECK(drv->buffer_size(h) == (size_t)ealsa_mock_buffer() * 6 * sizeof(float),
            "buffer_size %u is not six channels of %u frames",
            (unsigned)drv->buffer_size(h), ealsa_mock_buffer());
      drv->free(h);
   }
   want_layout = AUDIO_LAYOUT_STEREO;

   printf("   a card that is not there\n");
   caps_default(&c);
   ealsa_mock_reset(&c);
   h = drv->init("3,1", 48000, 64, &new_rate);
   CHECK(h == NULL, "init returned %p for a device that does not exist", h);
   CHECK(ealsa_mock_open_fds() == 0, "a failed init left %d descriptor(s) open", ealsa_mock_open_fds());
   caps_default(&c);
   c.refine_fails = 1;
   ealsa_mock_reset(&c);
   h = drv->init("0,0", 48000, 64, &new_rate);
   CHECK(h == NULL, "init returned %p for a device with no usable parameters", h);
   CHECK(ealsa_mock_open_fds() == 0, "a refused device left %d descriptor(s) open", ealsa_mock_open_fds());

   printf("   writing: the device gets the frames, and is started once\n");
   caps_default(&c);
   ealsa_mock_reset(&c);
   h = drv->init("0,0", 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed");
   if (h)
   {
      size_t frame_bytes = 2 * sizeof(float);
      size_t chunk       = 256 * frame_bytes;
      size_t sent        = 0;
      CHECK(drv->start(h, false), "start");
      drv->set_nonblock_state(h, false);
      for (i = 0; i < 64; i++)
      {
         ssize_t w = drv->write(h, frame, chunk);
         CHECK(w == (ssize_t)chunk, "write %u returned %ld", (unsigned)i, (long)w);
         if (w > 0) sent += w / frame_bytes;
         ealsa_mock_drain(256);
      }
      printf("      %u frames written, %u reached the device, started %d time(s)\n",
            (unsigned)sent, (unsigned)ealsa_mock_written(), ealsa_mock_started());
      CHECK(ealsa_mock_written() == sent, "%u frames were accepted and %u reached the device",
            (unsigned)sent, (unsigned)ealsa_mock_written());
      CHECK(ealsa_mock_started() == 1, "the stream was not started");
      /* frames_consumed: what the device has played, never more than
       * what it was given */
      {
         size_t played = drv->frames_consumed(h);
         printf("      frames_consumed %u with %u still queued of %u written\n",
               (unsigned)played, (unsigned)ealsa_mock_queued(), (unsigned)ealsa_mock_written());
         CHECK(played == ealsa_mock_written() - ealsa_mock_queued(),
               "frames_consumed %u, device played %u",
               (unsigned)played, (unsigned)(ealsa_mock_written() - ealsa_mock_queued()));
         CHECK(played <= ealsa_mock_written(), "frames_consumed is ahead of what was written");
      }

      printf("   a full device: non-blocking comes back short, blocking waits\n");
      drv->set_nonblock_state(h, true);
      {
         size_t total = 0; ssize_t w; unsigned tries = 0;
         do { w = drv->write(h, frame, chunk); if (w > 0) total += w; tries++; }
         while (w == (ssize_t)chunk && tries < 256);
         printf("      accepted %u bytes over %u writes before refusing\n", (unsigned)total, tries);
         CHECK(tries < 256, "non-blocking writes never came back short");
         CHECK(drv->write_avail(h) == 0, "a full device reports %u bytes free",
               (unsigned)drv->write_avail(h));
      }
      drv->set_nonblock_state(h, false);
      {
         ssize_t w;
         size_t  before = ealsa_mock_written();
         /* the device is full and not draining: the write has to hand
          * the pass back rather than spin */
         w = drv->write(h, frame, chunk);
         printf("      a blocking write to a full, stalled device returned %ld\n", (long)w);
         CHECK(w >= 0 && w < (ssize_t)chunk, "it took %ld of %u bytes", (long)w, (unsigned)chunk);
         CHECK(ealsa_mock_written() == before, "%u frames went into a full device",
               (unsigned)(ealsa_mock_written() - before));
      }

      printf("   an underrun is prepared and the frames go out\n");
      {
         int before = ealsa_mock_prepares();
         size_t got;
         ealsa_mock_drain(ealsa_mock_queued());   /* it played everything */
         ealsa_mock_inject_xrun();
         got = ealsa_mock_written();
         drv->write(h, frame, chunk);
         printf("      prepares %d -> %d, %u frames written after\n",
               before, ealsa_mock_prepares(), (unsigned)(ealsa_mock_written() - got));
         CHECK(ealsa_mock_prepares() > before, "the device was not prepared after the underrun");
         CHECK(ealsa_mock_written() > got, "nothing reached the device after the underrun");
      }

      printf("   stop pauses, start resumes\n");
      CHECK(drv->stop(h), "stop");
      CHECK(ealsa_mock_paused() == 1, "stop did not pause the device");
      CHECK(!drv->alive(h), "alive after stop");
      CHECK(drv->start(h, false), "start");
      CHECK(ealsa_mock_paused() == 0, "start did not resume the device");
      CHECK(drv->alive(h), "not alive after start");
      drv->free(h);
      CHECK(ealsa_mock_open_fds() == 0, "free left a descriptor open");
   }

   printf("   a card that cannot pause: stop drops and start prepares\n");
   caps_default(&c);
   c.can_pause = 0;
   ealsa_mock_reset(&c);
   h = drv->init("0,0", 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed on a card that cannot pause");
   if (h)
   {
      size_t chunk = 256 * 2 * sizeof(float);
      int prep;
      CHECK(drv->start(h, false), "start");
      drv->write(h, frame, chunk);
      prep = ealsa_mock_prepares();
      CHECK(drv->stop(h), "stop");
      CHECK(ealsa_mock_paused() == 0, "a card that cannot pause was paused");
      CHECK(ealsa_mock_started() == 0, "the stream was left running after stop");
      CHECK(drv->start(h, false), "start after stop");
      CHECK(ealsa_mock_prepares() >= prep, "the stream was not prepared again");
      CHECK(drv->write(h, frame, chunk) > 0, "no audio after a stop and start");
      drv->free(h);
   }

   printf("   the device list is what the kernel exposes\n");
   caps_default(&c);
   ealsa_mock_reset(&c);
   {
      struct string_list *list = (struct string_list*)drv->device_list_new(NULL);
      CHECK(list != NULL, "no device list");
      if (list)
      {
         printf("      %u device(s): %s\n", (unsigned)list->size,
               list->size ? list->elems[0].data : "none");
         CHECK(list->size == 1, "%u devices for one card", (unsigned)list->size);
         CHECK(list->size && string_is_equal(list->elems[0].data, "0,0"),
               "the device is named %s", list->size ? list->elems[0].data : "?");
         drv->device_list_free(NULL, list);
      }
      CHECK(ealsa_mock_open_fds() == 0, "the listing left %d descriptor(s) open", ealsa_mock_open_fds());
   }

   /* The write and wait loops in the states no real card gives on
    * demand. These cases came from tinyalsa_write_test, which drove
    * the driver this one replaced; the loops are new code but the
    * properties are the same, and they are the ones a device that is
    * full, stalled or signalling can break. */
   printf("   the write and wait loops against a device that will not take\n");
   caps_default(&c);
   ealsa_mock_reset(&c);
   h = drv->init("0,0", 48000, 64, &new_rate);
   CHECK(h != NULL, "init failed");
   if (h)
   {
      size_t chunk = 256 * 2 * sizeof(float);
      ssize_t w;
      unsigned before;

      CHECK(drv->start(h, false), "start");

      /* Filled first, and then left to stop draining: a device that
       * refuses while still reporting room is not a device. */
      drv->set_nonblock_state(h, true);
      while ((w = drv->write(h, frame, chunk)) == (ssize_t)chunk)
         ;
      ealsa_mock_freeze(1);

      /* A full device, non-blocking: what it managed, and no more. */
      w = drv->write(h, frame, chunk);
      printf("      non-blocking at a device taking nothing: %ld of %u bytes\n",
            (long)w, (unsigned)chunk);
      CHECK(w == 0, "a device taking nothing accepted %ld bytes", (long)w);

      /* Blocking against a device that never signals: bounded. */
      drv->set_nonblock_state(h, false);
      before = ealsa_mock_polls();
      w = drv->write(h, frame, chunk);
      printf("      blocking at a device that never signals: %ld bytes after %u poll(s)\n",
            (long)w, ealsa_mock_polls() - before);
      CHECK(w >= 0 && w < (ssize_t)chunk, "it took %ld of %u bytes", (long)w, (unsigned)chunk);
      CHECK(ealsa_mock_polls() > before, "it never waited on the device at all");

      /* wait_writable at the same device: nothing, within its bound. */
      CHECK(drv->wait_writable(h, chunk) == 0,
            "wait_writable found room at a device taking nothing");

      /* A poll cut short by a signal is not a refusal. The device is
       * still full, so the write comes back short either way; what
       * says the signals were retried rather than taken for an answer
       * is that the loop polled past them. */
      ealsa_mock_inject_eintr(3);
      before = ealsa_mock_polls();
      w = drv->write(h, frame, chunk);
      printf("      three signals at a full device: %u poll(s), %ld bytes\n",
            ealsa_mock_polls() - before, (long)w);
      CHECK(ealsa_mock_polls() - before > 3,
            "the write stopped at the first of three signals (%u poll(s))",
            ealsa_mock_polls() - before);

      /* The first write on a started device reports what it took, or
       * the caller sends the opening chunk twice. */
      ealsa_mock_reset(&c);
      drv->free(h);
      h = drv->init("0,0", 48000, 64, &new_rate);
      CHECK(h != NULL, "re-init failed");
      if (h)
      {
         drv->set_nonblock_state(h, false);
         drv->start(h, false);
         w = drv->write(h, frame, chunk);
         printf("      the first write reported %ld of %u bytes\n", (long)w, (unsigned)chunk);
         CHECK(w == (ssize_t)chunk, "the first write reported %ld of %u bytes", (long)w, (unsigned)chunk);
         drv->free(h);
      }
   }

   if (failures) { printf("%u failure(s)\n", failures); return 1; }
   printf("embedded alsa: the driver negotiates what the card offers and reports it\n");
   return 0;
}
