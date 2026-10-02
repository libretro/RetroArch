/* The 3DS CSND driver against the latency contract.
 *
 * audio_driver.h: buffer_size() and write_avail() are in bytes of the
 * format write() takes - here int16 stereo - write_avail() never above
 * buffer_size(). The rate control steers write_avail() toward half of
 * buffer_size(); fast-forward bounds the resampler's input by
 * write_avail(), and the threaded pipeline sizes its passes from it,
 * all in bytes. The driver keeps its ring as 2048 frames across two
 * channel buffers; a report in frames is a quarter of the room.
 *
 * The device is the system tick (csnd_mock.h): playing n samples moves
 * the driver's play position by n frames, and the room must grow by
 * n frames' bytes; a write of n bytes must take exactly n bytes of it. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>

#include "../../../audio/audio_driver.h"
#include "csnd_mock.h"

extern audio_driver_t audio_ctr_csnd;

#define FRAME_BYTES 4u      /* int16 stereo */
#define RING_FRAMES 2048u

static unsigned failures = 0;

#define CHECK(cond, msg, a, b) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: %s (%lu, %lu)\n", __FILE__, __LINE__, \
               msg, (unsigned long)(a), (unsigned long)(b)); \
         failures++; \
      } \
   } while (0)

static void play(unsigned samples)
{
   mock_tick += (u64)samples * MOCK_TICKS_PER_SAMPLE;
}

int main(void)
{
   int16_t buf[2 * 512];
   unsigned rate = 0;
   size_t   size, before, after;
   ssize_t  wrote;
   void    *h;
   const audio_driver_t *drv = &audio_ctr_csnd;

   memset(buf, 0, sizeof(buf));

   h = drv->init(NULL, 48000, 64, &rate);
   if (!h)
   {
      printf("FAIL: init\n");
      return 1;
   }
   CHECK(rate == 32730, "new_rate is CSND's output rate", rate, 32730);
   drv->set_nonblock_state(h, true);

   /* The capacity, in bytes of what write() takes. */
   size = drv->buffer_size(h);
   CHECK(size == RING_FRAMES * FRAME_BYTES,
         "buffer_size is the ring in bytes", size, RING_FRAMES * FRAME_BYTES);

   /* Half the ring plays out: that much room, in bytes. */
   play(RING_FRAMES / 2);
   before = drv->write_avail(h);
   CHECK(before == (RING_FRAMES / 2) * FRAME_BYTES,
         "write_avail is the played-out room in bytes",
         before, (RING_FRAMES / 2) * FRAME_BYTES);
   CHECK(before <= size, "write_avail within buffer_size", before, size);

   /* A write of 1024 bytes - 256 frames - takes 1024 bytes of room. */
   wrote = drv->write(h, buf, 1024);
   CHECK(wrote == 1024, "the write is taken whole", wrote, 1024);
   after = drv->write_avail(h);
   CHECK(before - after == 1024,
         "the room shrinks by the bytes written", before - after, 1024);

   /* 128 samples play: the room comes back by their bytes. */
   before = after;
   play(128);
   after  = drv->write_avail(h);
   CHECK(after - before == 128 * FRAME_BYTES,
         "the room grows by the bytes played", after - before,
         128 * FRAME_BYTES);
   CHECK(after <= size, "write_avail within buffer_size", after, size);
   CHECK(after % FRAME_BYTES == 0, "write_avail is whole frames",
         after % FRAME_BYTES, 0);

   /* Frames and bytes agree across a full write of the room. */
   before = drv->write_avail(h);
   wrote  = drv->write(h, buf, sizeof(buf));
   after  = drv->write_avail(h);
   CHECK(wrote == (ssize_t)sizeof(buf), "a second write is taken whole",
         wrote, sizeof(buf));
   CHECK(before - after == sizeof(buf),
         "the room shrinks by the bytes written", before - after,
         sizeof(buf));

   drv->free(h);

   if (failures)
   {
      printf("[fail] ctr_csnd_test: %u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] ctr_csnd_test\n");
   return 0;
}
