/* The XAudio2 driver's lend (write_begin / write_end) in
 * audio/drivers/xaudio.c, the shipping driver on wine's XAudio2 or
 * Windows' own.
 *
 * The driver queues sixteen buffers cut from one allocation. A lend
 * hands out the free span at the head, up to the allocation's end, so
 * the frontend's resampler writes into the buffer XAudio2 plays rather
 * than into its own and then through write()'s copy. What this pins:
 *
 *   room             -> a lend is never more than write_avail reports,
 *                       nor past the allocation's end
 *   one stream       -> lends and writes mixed, each lend starts where
 *                       the last lend or write left off, wrapping only
 *                       at the end of the allocation
 *   no waiting       -> write_end publishes all it is given at once,
 *                       even filling the whole free room
 *   it plays         -> blocking on wait_writable, a second of audio
 *                       through lends plays at the pace a second of
 *                       written blocks does
 *   abandoned        -> write_end(0) publishes nothing */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "audio/audio_driver.h"

extern audio_driver_t audio_xa;

static unsigned failures = 0;

static void check(bool cond, const char *what)
{
   printf("  [%s] %s\n", cond ? "pass" : "FAIL", what);
   if (!cond)
      failures++;
}

static double now_ms(void)
{
   LARGE_INTEGER f, c;
   QueryPerformanceFrequency(&f);
   QueryPerformanceCounter(&c);
   return (double)c.QuadPart * 1000.0 / (double)f.QuadPart;
}

#define RATE  48000
#define BLOCK (480 * 2 * sizeof(int16_t)) /* 10 ms of stereo s16 */

/* A second of audio through blocking writes, or through lends where
 * the span reaches (the staged write where it stops at the end of the
 * allocation), after the queue is full; the ms it took, 0 if a block
 * did not go through */
static double play_second(audio_driver_t *d, void *xa, const uint8_t *block,
      bool lend)
{
   int    i;
   double t0 = 0;
   for (i = 0; i < 120; i++)
   {
      void  *region = NULL;
      size_t got    = 0;
      if (i == 20)
         t0 = now_ms();
      d->wait_writable(xa, BLOCK);
      if (lend && (got = d->write_begin(xa, BLOCK, &region)) == BLOCK)
      {
         memset(region, 0, got);
         if (d->write_end(xa, got) != (ssize_t)got)
            return 0;
         continue;
      }
      if (got)
         d->write_end(xa, 0);
      if (d->write(xa, block, BLOCK) != (ssize_t)BLOCK)
         return 0;
   }
   return now_ms() - t0;
}

int main(void)
{
   audio_driver_t *d = &audio_xa;
   unsigned new_rate = 0;
   void    *xa;
   uint8_t  block[BLOCK];
   uint8_t *base = NULL, *top = NULL, *next = NULL;
   size_t   total, lent_bytes = 0;
   bool     room_ok = true, stream_ok = true, end_ok = true;
   int      i;

   memset(block, 0, sizeof(block));
   printf("init\n");
   if (!(xa = d->init(NULL, RATE, 64, &new_rate)))
   {
      puts(getenv("REQUIRE_XAUDIO") ? "FAIL: no XAudio2"
            : "SKIP: no XAudio2 device");
      return getenv("REQUIRE_XAUDIO") ? 1 : 0;
   }
   check(d->write_begin && d->write_end, "the driver lends");
   d->set_nonblock_state(xa, false);
   d->start(xa, false);
   total = d->buffer_size(xa);

   printf("lends and writes, one stream\n");
   /* the first lend finds the head at the allocation's start */
   for (i = 0; i < 400; i++)
   {
      void  *region = NULL;
      size_t avail, got, want = BLOCK * (1 + (size_t)(i % 7));

      d->wait_writable(xa, BLOCK);
      avail = d->write_avail(xa);
      if (i % 5 == 4)
      {
         /* a plain write moves the head the same way */
         size_t n = BLOCK < avail ? BLOCK : avail;
         if (d->write(xa, block, n) != (ssize_t)n)
            end_ok = false;
         if (next)
            next += n;
         continue;
      }
      got = d->write_begin(xa, want, &region);
      /* XAudio2's thread frees buffers at any time, so the room read
       * before the lend can be less than the lend found: the room is
       * read again after it. Only this thread takes room, so it can
       * only have grown since the lend measured it. */
      if (got > d->write_avail(xa))
         room_ok = false;
      if (!got)
         continue;
      if (!base)
      {
         base = (uint8_t*)region;
         top  = base + total;
      }
      if ((uint8_t*)region + got > top || (uint8_t*)region < base)
         room_ok = false;
      if (next && next >= top)
         next = base + (size_t)(next - top);
      if (next && (uint8_t*)region != next)
         stream_ok = false;
      memset(region, 0, got);
      /* publish a part or all of it */
      if (i % 3 == 0)
         got -= got / 2 & ~(size_t)3;
      if (d->write_end(xa, got) != (ssize_t)got)
         end_ok = false;
      next        = (uint8_t*)region + got;
      lent_bytes += got;
   }
   check(room_ok, "a lend is within the free room and the allocation");
   check(stream_ok, "each lend starts where the stream left off");
   check(end_ok, "write_end and write publish all they are given");

   printf("the whole free room at once\n");
   {
      void  *region = NULL;
      size_t got;
      double t;
      /* wait for most of the queue to drain */
      Sleep(200);
      got = d->write_begin(xa, total, &region);
      memset(region, 0, got);
      t   = now_ms();
      check(got > 0 && d->write_end(xa, got) == (ssize_t)got,
            "filling it publishes it all");
      check(now_ms() - t < 20, "without waiting");
   }

   printf("abandoned\n");
   {
      void  *region = NULL, *again = NULL;
      size_t got, avail;
      d->wait_writable(xa, BLOCK);
      avail = d->write_avail(xa);
      got   = d->write_begin(xa, BLOCK, &region);
      check(d->write_end(xa, 0) == 0, "write_end(0) publishes nothing");
      check(d->write_avail(xa) >= avail, "and takes no room");
      d->write_begin(xa, BLOCK, &again);
      check(!got || again == region, "the next lend is the same span");
      d->write_end(xa, 0);
   }

   printf("it plays\n");
   {
      double plain = play_second(d, xa, block, false);
      double lent  = play_second(d, xa, block, true);
      printf("  (1 s of audio: %.0f ms written, %.0f ms lent; %u bytes lent "
            "before)\n", plain, lent, (unsigned)lent_bytes);
      check(plain > 0 && lent > 0, "every block went through");
      /* the device's pace, whatever wine's is, as plain writes keep it */
      check(lent > plain * 0.85 && lent < plain * 1.15,
            "lent blocks play at the pace written ones do");
   }

   d->free(xa);
   printf("\n%s\n", failures ? "FAILURES" : "all passed");
   return failures ? 1 : 0;
}
