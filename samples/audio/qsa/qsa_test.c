/* audio/drivers/alsa_qsa.c against the mock QSA device.
 *
 * The QNX driver has no build in CI and no hardware here, so it is
 * built against a stand-in <sys/asoundlib.h> and run: init and its
 * failure path, the rate it reports back, blocking and non-blocking
 * writes, what the device is actually given, stop and start, and the
 * recovery from an underrun. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sys/asoundlib.h"
#include "../../../audio/audio_driver.h"

extern audio_driver_t audio_alsa;

static unsigned failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { printf("      FAIL: "); printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

static int16_t frame[512 * 2];

int main(void)
{
   audio_driver_t *drv = &audio_alsa;
   void *h;
   unsigned new_rate;
   size_t i;

   printf("qsa:\n");

   printf("   init failing gives the frontend a NULL, and leaves nothing open\n");
   qsa_mock_reset();
   qsa_mock_set_open_error(-EBUSY);
   new_rate = 0;
   h = drv->init(NULL, 48000, 64, &new_rate);
   CHECK(h == NULL, "a failed open returned %p, which the frontend reads as a live driver", h);
   CHECK(qsa_mock_open_handles() == 0, "a failed open left %d handle(s) open", qsa_mock_open_handles());
   qsa_mock_reset();
   qsa_mock_set_params_error(-EINVAL);
   h = drv->init(NULL, 48000, 64, &new_rate);
   CHECK(h == NULL, "failed parameters returned %p", h);
   CHECK(qsa_mock_open_handles() == 0, "failed parameters left the device open");

   printf("   the rate the device took is the rate the frontend is told\n");
   qsa_mock_reset();
   new_rate = 0;
   h = drv->init(NULL, 44100, 64, &new_rate);
   CHECK(h != NULL, "init failed");
   if (!h) { printf("%u failure(s)\n", failures); return 1; }
   printf("      asked 44100, device took %d, frontend told %u\n", qsa_mock_device_rate(), new_rate);
   CHECK(new_rate == (unsigned)qsa_mock_device_rate(),
         "the device runs at %d and the frontend was told %u: everything plays at the wrong pitch",
         qsa_mock_device_rate(), new_rate);
   CHECK(drv->buffer_size(h) > 0, "buffer_size is zero");

   printf("   blocking writes reach the device\n");
   for (i = 0; i < 1024; i++) frame[i] = (int16_t)(i * 37);
   drv->set_nonblock_state(h, false);
   CHECK(drv->start(h, false), "start");
   {
      size_t sent = 0;
      for (i = 0; i < 64; i++)
      {
         ssize_t w = drv->write(h, frame, sizeof(frame));
         CHECK(w == (ssize_t)sizeof(frame), "write %u returned %ld", (unsigned)i, (long)w);
         if (w > 0) sent += w;
         qsa_mock_drain(sizeof(frame));   /* the device plays a frame's worth */
      }
      printf("      %u bytes accepted, %u reached the device\n",
            (unsigned)sent, (unsigned)qsa_mock_written());
      CHECK(qsa_mock_written() >= sent - drv->buffer_size(h),
            "%u bytes were accepted but only %u reached the device: audio was dropped",
            (unsigned)sent, (unsigned)qsa_mock_written());
   }

   printf("   a non-blocking write on a full device returns short\n");
   drv->set_nonblock_state(h, true);
   {
      size_t total = 0;
      ssize_t w;
      unsigned tries = 0;
      do {
         w = drv->write(h, frame, sizeof(frame));
         if (w > 0) total += w;
         tries++;
      } while (w == (ssize_t)sizeof(frame) && tries < 512);
      printf("      accepted %u bytes over %u writes before refusing\n", (unsigned)total, tries);
      CHECK(tries < 512, "non-blocking writes never returned short on a full device");
   }

   printf("   stop pauses and start resumes\n");
   drv->set_nonblock_state(h, false);
   CHECK(drv->stop(h), "stop");
   CHECK(qsa_mock_paused() == 1, "stop did not pause the device");
   CHECK(!drv->alive(h), "alive after stop");
   CHECK(drv->start(h, false), "start");
   CHECK(qsa_mock_paused() == 0, "start did not resume the device");
   CHECK(drv->alive(h), "not alive after start");

   printf("   an underrun is recovered and the write goes on\n");
   {
      int before = qsa_mock_prepares();
      size_t got;
      qsa_mock_set_status(SND_PCM_STATUS_UNDERRUN);
      got = qsa_mock_written();
      /* the device refuses while it is underrun; the driver has to
       * ask for the status, prepare, and carry on. The status clears
       * on the prepare, so one write is enough. */
      drv->write(h, frame, sizeof(frame));
      CHECK(qsa_mock_prepares() > before, "the driver did not prepare the device after an underrun");
      for (i = 0; i < 8; i++)
      {
         drv->write(h, frame, sizeof(frame));
         qsa_mock_drain(sizeof(frame));
      }
      printf("      prepares %d -> %d, %u bytes written after\n",
            before, qsa_mock_prepares(), (unsigned)(qsa_mock_written() - got));
      CHECK(qsa_mock_written() > got, "no audio reached the device after the underrun");
   }

   printf("   a blocking write on a stopped device gives up rather than hanging\n");
   {
      ssize_t w1, w2;
      size_t  before = qsa_mock_written();
      CHECK(drv->stop(h), "stop");
      /* The first goes into the driver's staging and is reported as
       * taken, which it is - it reaches the device when the device
       * takes it again. The second finds the staging full and the
       * device refusing, and has to come back short rather than
       * spinning on a device that is not draining. */
      w1 = drv->write(h, frame, sizeof(frame));
      w2 = drv->write(h, frame, sizeof(frame));
      printf("      staged %ld, then %ld with the device refusing; %u bytes reached it\n",
            (long)w1, (long)w2, (unsigned)(qsa_mock_written() - before));
      CHECK(w2 >= 0 && w2 < (ssize_t)sizeof(frame),
            "a second write to a paused device took %ld of %u bytes", (long)w2, (unsigned)sizeof(frame));
      CHECK(qsa_mock_written() == before, "a paused device was given %u bytes",
            (unsigned)(qsa_mock_written() - before));
      CHECK(drv->write_avail(h) == 0, "write_avail reports room with the staging full");
      CHECK(drv->start(h, false), "start");
   }

   drv->free(h);
   CHECK(qsa_mock_open_handles() == 0, "free left the device open");

   if (failures) { printf("%u failure(s)\n", failures); return 1; }
   printf("qsa: the QNX driver opens, reports its rate, writes, pauses and recovers\n");
   return 0;
}
