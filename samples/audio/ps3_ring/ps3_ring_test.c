/* The PS3 driver's blocking write and its teardown flag.
 *
 * Two things this pins, both of which have a history.
 *
 * ps3_audio_wait_block() must hold cond_lock across sysLwCondWait():
 * the call requires the lwcond's own mutex and fails at once with EPERM
 * without it, which turned the write's bounded waits into a few
 * microseconds of spinning that returned zero and dropped the audio
 * whenever the ring was full. The device mock refuses the same way and
 * counts it, so a write that fills the ring and then has to wait is the
 * check: it must come back having written, with no refusal counted.
 *
 * quit_thread is set by the thread tearing the driver down and read by
 * the output thread's loop every pass, with no lock on either side.
 * Against the volatile bool it used to be, ThreadSanitizer reports that
 * pair; it is clean as an atomic. Both functions here are the shipping
 * ones - the driver's own translation unit is linked, not modelled. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rthreads/rthreads.h>
#include <retro_atomic.h>

#include "../../../audio/audio_driver.h"
#include "device_mock.h"

#define WRITE_FRAMES 256u
/* More than the ring holds several times over, so the writer cannot get
 * through without the output thread draining underneath it - which is
 * the only way the wait is exercised at all. */
#define WRITE_ROUNDS 200u
#define ROUNDS        16u

static const audio_driver_t *drv = &audio_ps3;

/* A write big enough to outrun the ring, so the writer has to wait on
 * the output thread rather than sail through. */
static int check_blocking_write(void)
{
   unsigned  new_rate = 0;
   void     *h;
   uint32_t *chunk;
   size_t    bufsz;
   unsigned  round;
   int       bad = 0;

   mock_device_reset();
   h = drv->init(NULL, 48000, 64, &new_rate);
   if (!h)
   {
      printf("FAIL  init returned NULL\n");
      return 1;
   }
   if (!drv->alive(h))
   {
      printf("FAIL  not alive after init\n");
      drv->free(h);
      return 1;
   }

   bufsz = drv->buffer_size(h);
   chunk = (uint32_t*)calloc(bufsz ? bufsz : 4096, 1);
   if (!chunk)
   {
      drv->free(h);
      printf("ok    (no memory for the write check)\n");
      return 0;
   }
   /* Non-silent, so the device can tell a fed block from a starved one. */
   memset(chunk, 0x3f, bufsz ? bufsz : 4096);

   /* Blocking: the writer must wait for the output thread and then get
    * its frames in, round after round. */
   drv->set_nonblock_state(h, false);
   for (round = 0; round < WRITE_ROUNDS; round++)
   {
      ssize_t w = drv->write(h, chunk, WRITE_FRAMES * sizeof(uint32_t));
      if (w <= 0)
      {
         printf("FAIL  a blocking write returned %ld at round %u\n",
               (long)w, round);
         bad = 1;
         break;
      }
   }

   if (!bad && MOCK_READ(mock_cond_eperm))
   {
      printf("FAIL  the wait was refused %lu times: cond_lock was not held\n",
            (unsigned long)MOCK_READ(mock_cond_eperm));
      bad = 1;
   }
   if (!bad && !MOCK_READ(mock_blocks))
   {
      printf("FAIL  the device never took a block\n");
      bad = 1;
   }

   if (!bad)
      printf("ok    %u blocking writes, %lu blocks taken, no refused wait\n",
            WRITE_ROUNDS, (unsigned long)MOCK_READ(mock_blocks));

   free(chunk);
   drv->free(h);
   return bad;
}

/* Teardown while the output thread is running: the write of quit_thread
 * against that thread's read of it. */
static int check_teardown_flag(void)
{
   unsigned new_rate = 0;
   unsigned round;

   for (round = 0; round < ROUNDS; round++)
   {
      void *h;
      mock_device_reset();
      if (!(h = drv->init(NULL, 48000, 64, &new_rate)))
      {
         printf("FAIL  init returned NULL at round %u\n", round);
         return 1;
      }
      /* Let the output thread get into its loop, so the free below
       * writes the flag while that loop is reading it. */
      while (MOCK_READ(mock_blocks) < 2)
         usleep(200);
      drv->free(h);
   }

   printf("ok    %u teardowns under a running output thread\n", ROUNDS);
   return 0;
}

int main(void)
{
   int bad = 0;
   setvbuf(stdout, NULL, _IONBF, 0);
   printf("ps3_ring: the blocking write holds the cond's mutex, and teardown crosses two threads\n");
   bad |= check_blocking_write();
   bad |= check_teardown_flag();
   printf(bad ? "ps3_ring: FAILED\n" : "ps3_ring: ok\n");
   return bad;
}
