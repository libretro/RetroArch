/* The Xbox 360 libxenon driver against the latency contract.
 *
 * audio_driver.h: buffer_size() is what the driver holds between write()
 * returning and the device playing it, write_avail() what it takes now
 * without blocking, both in bytes of int16 stereo; with neither, the
 * rate control is off for the session. libxenon's ring is 64 KiB and its
 * submit copies in without checking what is unplayed, so the driver has
 * to keep the queue within it - and its own staging buffer holds 2048
 * frames, which a longer write must not run past.
 *
 * xenon_mock.c is the queue; xenon_mock_play() plays bytes out of it. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>

#include "../../../audio/audio_driver.h"
#include "xenon_mock.h"

extern audio_driver_t audio_xenon360;

#define FRAME_BYTES 4u

static unsigned failures = 0;

#define CHECK(cond, msg, a, b) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s (%ld, %ld)\n", __FILE__, __LINE__, \
               msg, (long)(a), (long)(b)); \
         failures++; \
      } \
   } while (0)

static uint32_t big[8192];   /* 32 KiB: longer than the driver's staging */

int main(void)
{
   static const size_t plays[] = { 1000, 4096, 777, 12288, 3 };
   const audio_driver_t *drv = &audio_xenon360;
   unsigned rate = 0, k;
   size_t   size, wa, latency_bytes;
   ssize_t  w;
   void    *h;

   xenon_mock_reset();
   h = drv->init(NULL, 48000, 64, &rate);
   if (!h)
   {
      printf("FAIL: init\n");
      return 1;
   }
   CHECK(rate == 48000, "new_rate is the hardware's", rate, 48000);
   drv->set_nonblock_state(h, true);

   /* A write longer than the staging buffer, onto an empty queue: it
    * goes in passes, and never more than the driver lets queue. */
   w = drv->write(h, big, sizeof(big));
   CHECK(w > 0 && (size_t)w <= sizeof(big), "a long write takes some of it", w, sizeof(big));
   CHECK((size_t)w == xenon_mock_submitted(), "what the write took is what was submitted",
         w, xenon_mock_submitted());

   if (!drv->write_avail || !drv->buffer_size)
   {
      printf("FAIL: write_avail and buffer_size are missing; rate control is off\n");
      drv->free(h);
      return 1;
   }

   size          = drv->buffer_size(h);
   latency_bytes = 48000u * 64u / 1000u * FRAME_BYTES;
   CHECK(size == latency_bytes, "buffer_size is the latency setting", size, latency_bytes);
   CHECK((size_t)w == size, "the long write took the whole size", w, size);
   CHECK(drv->write_avail(h) == 0, "a full queue has no room", drv->write_avail(h), 0);

   xenon_mock_play((size_t)-1);
   wa = drv->write_avail(h);
   CHECK(wa == size, "an empty queue is the whole size", wa, size);

   for (k = 0; k < sizeof(plays) / sizeof(plays[0]); k++)
   {
      wa = drv->write_avail(h);
      CHECK(wa <= size && wa % FRAME_BYTES == 0, "write_avail within the size, in frames",
            wa, size);
      /* asked for more than the room: it takes the room, no more */
      w = drv->write(h, big, wa + 4096);
      CHECK(w == (ssize_t)wa, "the write takes what write_avail said", w, wa);
      CHECK(drv->write_avail(h) == 0, "no room after a write of the room",
            drv->write_avail(h), 0);
      xenon_mock_play(plays[k]);
      CHECK(drv->write_avail(h) == (plays[k] & ~(size_t)3) ||
            plays[k] >= size,
            "the room grows by the frames played", drv->write_avail(h), plays[k]);
   }
   CHECK(xenon_mock_peak() <= size, "the queue never held more than the size",
         xenon_mock_peak(), size);

   /* Blocking: the write waits on the queue to play and takes it all. */
   drv->set_nonblock_state(h, false);
   w = drv->write(h, big, sizeof(big));
   CHECK(w == (ssize_t)sizeof(big), "a blocking write takes it all", w, sizeof(big));
   CHECK(xenon_mock_peak() <= size, "a blocking write keeps within the size",
         xenon_mock_peak(), size);

   drv->free(h);

   if (failures)
   {
      printf("[fail] xenon360_test: %u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] xenon360_test\n");
   return 0;
}
