/* The 3DS CSND and NDSP drivers against the latency contract.
 *
 * audio_driver.h: buffer_size() and write_avail() are in bytes of the
 * format write() takes - here int16 stereo - write_avail() never above
 * buffer_size(). The rate control steers write_avail() toward half of
 * buffer_size(); fast-forward bounds the resampler's input by
 * write_avail(), and the threaded pipeline sizes its passes from it,
 * all in bytes. The driver keeps its ring as 2048 frames across two
 * channel buffers; a report in frames is a quarter of the room.
 *
 * CSND's device is the system tick (ctr_mock.h): playing n samples moves
 * the driver's play position by n frames, and the room must grow by
 * n frames' bytes; a write of n bytes must take exactly n bytes of it.
 *
 * Both drivers keep a 2048-frame ring and take at most half of it a
 * call: NDSP's write copied the caller's whole length into its ring
 * with a single wrap, so a write longer than the ring ran past it -
 * ASan sees that - and CSND's overwrote its own unplayed audio. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>

#include "../../../audio/audio_driver.h"
#include "ctr_mock.h"

extern audio_driver_t audio_ctr_csnd;
extern audio_driver_t audio_ctr_dsp;

#define FRAME_BYTES 4u      /* int16 stereo */
#define RING_FRAMES 2048u
#define HALF_RING   (RING_FRAMES / 2 * FRAME_BYTES)

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

static int16_t big[2 * 8192];   /* 32 KiB: four rings' worth */

static void test_csnd(void)
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
      printf("FAIL: csnd init\n");
      failures++;
      return;
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

   /* A write longer than the ring takes half the ring, no more. */
   wrote = drv->write(h, big, sizeof(big));
   CHECK(wrote == (ssize_t)HALF_RING, "a long write takes half the ring",
         wrote, HALF_RING);

   drv->free(h);
}

static void test_dsp(void)
{
   unsigned rate = 0, i;
   ssize_t  wrote;
   void    *h;
   const audio_driver_t *drv = &audio_ctr_dsp;

   for (i = 0; i < sizeof(big) / sizeof(big[0]); i++)
      big[i] = (int16_t)i;

   mock_sample_pos = 0;
   h = drv->init(NULL, 48000, 64, &rate);
   if (!h)
   {
      printf("FAIL: dsp init\n");
      failures++;
      return;
   }
   CHECK(rate == 32728, "new_rate is NDSP's output rate", rate, 32728);
   CHECK(drv->buffer_size(h) == RING_FRAMES * FRAME_BYTES,
         "buffer_size is the ring in bytes", drv->buffer_size(h),
         RING_FRAMES * FRAME_BYTES);

   /* Non-blocking, longer than the ring: half of it goes in, and the
    * copy stays inside the ring (ASan is the check on that). */
   drv->set_nonblock_state(h, true);
   wrote = drv->write(h, big, sizeof(big));
   CHECK(wrote == (ssize_t)HALF_RING, "a long write takes half the ring",
         wrote, HALF_RING);

   /* Again from a position where the half wraps the ring's end. */
   mock_sample_pos = 1900;
   wrote = drv->write(h, big, sizeof(big));
   CHECK(wrote == (ssize_t)HALF_RING, "a long write across the wrap takes half the ring",
         wrote, HALF_RING);

   /* Blocking: the write waits for the channel to play and takes half
    * the ring. */
   drv->set_nonblock_state(h, false);
   wrote = drv->write(h, big, sizeof(big));
   CHECK(wrote == (ssize_t)HALF_RING, "a blocking long write takes half the ring",
         wrote, HALF_RING);

   /* Whole frames only. */
   drv->set_nonblock_state(h, true);
   wrote = drv->write(h, big, 1023);
   CHECK(wrote % FRAME_BYTES == 0, "a write takes whole frames", wrote, 1020);

   drv->free(h);
}

int main(void)
{
   test_csnd();
   test_dsp();

   if (failures)
   {
      printf("[fail] ctr_audio_test: %u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] ctr_audio_test\n");
   return 0;
}
