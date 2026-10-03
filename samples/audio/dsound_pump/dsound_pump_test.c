/* Regression test for the DirectSound pump thread in
 * audio/drivers/dsound.c, run under Windows or wine.
 *
 * The pump used to retry every millisecond whenever the DirectSound
 * buffer had no room or the staging ring had nothing queued. It now
 * waits for the time the play cursor needs to move that far, worked
 * out from the rate, or until the writer queues a block or the thread
 * is stopped.
 *
 * The contract this pins, on a real (or wine-emulated) device:
 *
 *   blocking writes        -> audio flows, and the writer waits on the
 *                             device rather than running ahead
 *   frames_consumed        -> advances with it
 *   the writer stops       -> the pump fills silence, it does not stall
 *   free while it waits    -> returns at once, not after the wait
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#include "audio/audio_driver.h"

extern audio_driver_t audio_dsound;

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

#define RATE 48000

int main(void)
{
   unsigned new_rate = 0;
   void *ds;
   int16_t block[480 * 2];            /* 10 ms of stereo */
   double t0, took;
   size_t before, after;
   int i;

   memset(block, 0, sizeof(block));
   printf("init\n");
   ds = audio_dsound.init(NULL, RATE, 64, &new_rate);
   check(ds != NULL, "driver initialises");
   if (!ds)
      return 1;
   audio_dsound.set_nonblock_state(ds, false);
   audio_dsound.start(ds, false);

   printf("steady writes\n");
   /* Fill the pipeline first, then time a second of audio. */
   for (i = 0; i < 20; i++)
      audio_dsound.write(ds, block, sizeof(block));
   before = audio_dsound.frames_consumed(ds);
   t0     = now_ms();
   for (i = 0; i < 100; i++)
      if (audio_dsound.write(ds, block, sizeof(block)) != (ssize_t)sizeof(block))
         break;
   took  = now_ms() - t0;
   after = audio_dsound.frames_consumed(ds);
   printf("  (1 s of audio took %.0f ms, %u frames consumed)\n", took, (unsigned)(after - before));
   check(i == 100, "every blocking write went through");
   check(took > 300, "blocking writes wait for the device");
   check(after - before > RATE / 2, "the device consumed it");

   printf("the writer stops\n");
   before = audio_dsound.frames_consumed(ds);
   Sleep(300);
   after  = audio_dsound.frames_consumed(ds);
   check(after - before > RATE / 10, "the pump keeps the device fed with silence");

   printf("free while the pump waits\n");
   t0   = now_ms();
   audio_dsound.free(ds);
   took = now_ms() - t0;
   check(took < 100, "free returns at once");

   if (failures)
   {
      printf("\n%u failure(s)\n", failures);
      return 1;
   }
   printf("\nall passed\n");
   return 0;
}
