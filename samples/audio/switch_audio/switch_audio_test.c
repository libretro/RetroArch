/* The Switch audout driver against the latency contract.
 *
 * audio_driver.h: buffer_size() is everything between write() returning
 * and the device playing it, write_avail() what the driver takes right
 * now without blocking, in the same bytes. The rate control steers the
 * room toward half the size. The driver queues buffers with the audout
 * service and fills each one in turn, so the room is the buffers the
 * service has released plus the rest of the one being filled, and the
 * size is every buffer carrying what it is appended with.
 *
 * audout_mock.c is the service; audout_mock_play() plays the oldest
 * queued buffers out. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>

#include "../../../audio/audio_driver.h"
#include "audout_mock.h"

extern audio_driver_t audio_switch;

#define BUFFERS     5           /* BUFFER_COUNT under libnx */
#define FRAME_BYTES 4u          /* int16 stereo */

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

static int16_t big[16384];

int main(void)
{
   static const unsigned plays[] = { 1, 3, 2, 5, 1, 4 };
   const audio_driver_t *drv = &audio_switch;
   unsigned rate = 0, k;
   size_t   size, chunk, latency_bytes, wa;
   ssize_t  w;
   void    *h;

   audout_mock_reset();
   h = drv->init(NULL, 48000, 64, &rate);
   if (!h)
   {
      printf("FAIL: init\n");
      return 1;
   }
   CHECK(rate == 48000, "new_rate is the service's rate", rate, 48000);
   drv->set_nonblock_state(h, true);

   /* The size is the latency setting, spread across the buffers in
    * whole frames: within a frame per buffer of it. */
   size          = drv->buffer_size(h);
   chunk         = size / BUFFERS;
   latency_bytes = 48000u * 64u / 1000u * FRAME_BYTES;
   CHECK(size % BUFFERS == 0 && chunk % FRAME_BYTES == 0,
         "buffer_size is whole frames in every buffer", size, chunk);
   CHECK(size <= latency_bytes && latency_bytes - size < BUFFERS * FRAME_BYTES,
         "buffer_size is the latency setting", size, latency_bytes);

   /* Init queues every buffer with the service: no room until one is
    * played out. */
   wa = drv->write_avail(h);
   CHECK(wa == 0, "no room while every buffer is queued", wa, 0);

   /* All played out: the whole size is room. */
   audout_mock_play(BUFFERS);
   wa = drv->write_avail(h);
   CHECK(wa == size, "every buffer released is the whole size", wa, size);

   for (k = 0; k < sizeof(plays) / sizeof(plays[0]); k++)
   {
      wa = drv->write_avail(h);
      CHECK(wa <= size, "write_avail within buffer_size", wa, size);
      /* asked for more than the room: it takes the room, no more */
      w = drv->write(h, big, wa + 4096);
      CHECK(w == (ssize_t)wa, "the write takes what write_avail said", w, wa);
      CHECK(drv->write_avail(h) == 0, "no room after a write of the room",
            drv->write_avail(h), 0);
      if (audout_mock_appends() > BUFFERS)
         CHECK(audout_mock_last_append() == chunk,
               "a buffer is appended carrying its chunk",
               audout_mock_last_append(), chunk);
      /* the service plays some: their room comes back */
      audout_mock_play(plays[k]);
      wa = drv->write_avail(h);
      CHECK(wa == plays[k] * chunk || (plays[k] > BUFFERS && wa == size),
            "the room grows by the buffers played", wa, plays[k] * chunk);
      /* a partial write leaves a buffer part-filled */
      if (wa >= 1000)
      {
         w = drv->write(h, big, 1000);
         CHECK(w == 1000 && wa - drv->write_avail(h) == 1000,
               "a 1000-byte write takes 1000 bytes of room", w,
               wa - drv->write_avail(h));
      }
   }

   /* Blocking: with nothing released, the write waits for the service
    * to play buffers out and takes everything. */
   drv->set_nonblock_state(h, false);
   w = drv->write(h, big, 3 * chunk);
   CHECK(w == (ssize_t)(3 * chunk), "a blocking write takes it all", w, 3 * chunk);

   drv->free(h);

   if (failures)
   {
      printf("[fail] switch_audio_test: %u check(s) failed\n", failures);
      return 1;
   }
   printf("[pass] switch_audio_test\n");
   return 0;
}
