/* The device behind the three console audio drivers: it reads the
 * window it is handed and takes a period over it, which is what the
 * drivers' hand-rolled SPSC ring is written against.
 *
 * The frames the writer sends carry a running count, so a window that
 * does not continue the last one means the writer reached data the
 * device had not finished with - the ring lapped, or a period was
 * published free before the device took it. Checked here rather than
 * in the test so it holds for whichever driver is running. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <rthreads/rthreads.h>

#include <psp2/audioout.h>
#include <libSceAudioOut.h>

#include "device_mock.h"

#define MOCK_GRAIN   512u
#define MOCK_PERIOD_US 1000

static volatile uint64_t sink;

retro_atomic_size_t mock_periods;
retro_atomic_size_t mock_silent;
retro_atomic_size_t mock_breaks;
static uint32_t   expect;
static int        primed;

void mock_device_reset(void)
{
   retro_atomic_size_init(&mock_periods, 0);
   retro_atomic_size_init(&mock_silent, 0);
   retro_atomic_size_init(&mock_breaks, 0);
   expect       = 0;
   primed       = 0;
}

/* A window of MOCK_GRAIN frames, read the way the hardware would. */
static void mock_consume(const void *buf)
{
   const uint32_t *w = (const uint32_t*)buf;
   unsigned i;
   uint64_t acc = 0;
   int silent   = 1;

   for (i = 0; i < MOCK_GRAIN; i++)
   {
      acc += w[i];
      if (w[i])
         silent = 0;
   }
   sink += acc;

   if (silent)
      retro_atomic_fetch_add_size(&mock_silent, 1);
   else
   {
      /* The first window seen sets the baseline: psp1 hands over the
       * window after read_pos, so its first period is not frame 0. */
      if (!primed)
      {
         expect = w[0];
         primed = 1;
      }
      for (i = 0; i < MOCK_GRAIN; i++)
      {
         if (w[i] != expect)
         {
            if (MOCK_READ(mock_breaks) < 4)
               fprintf(stderr,
                     "      break at frame %u of the window: got %u, "
                     "expected %u\n", i, (unsigned)w[i], (unsigned)expect);
            retro_atomic_fetch_add_size(&mock_breaks, 1);
            expect = w[i];
         }
         expect++;
      }
   }
   retro_atomic_fetch_add_size(&mock_periods, 1);
   usleep(MOCK_PERIOD_US);
}

/* PSP1 */
int sceAudioSRCChReserve(int samples, int freq, int channels)
{
   (void)samples; (void)channels;
   return (freq == 48000 || freq == 44100) ? 1 : -1;
}
int sceAudioSRCChRelease(void) { return 0; }
int sceAudioSRCOutputBlocking(int vol, void *buf)
{
   (void)vol;
   mock_consume(buf);
   return (int)MOCK_GRAIN;
}

/* Vita */
int sceAudioOutOpenPort(int type, int len, int freq, int mode)
{
   (void)type; (void)len; (void)mode;
   return freq == 48000 ? 2 : -1;
}
int sceAudioOutReleasePort(int port) { (void)port; return 0; }

/* Vita and PS4 share this one. */
int sceAudioOutOutput(int port, const void *buf)
{
   (void)port;
   mock_consume(buf);
   return 0;
}

/* PS4 */
int sceAudioOutInit(void) { return 0; }
int sceAudioOutOpen(unsigned user, int type, int index, unsigned len,
      unsigned freq, unsigned mode)
{
   (void)user; (void)type; (void)index; (void)len; (void)mode;
   return freq == 48000 ? 3 : -1;
}
int sceAudioOutClose(int port) { (void)port; return 0; }

/* Thread-creation failure, for the lane that checks a driver whose
 * worker never starts reports the failure rather than a live driver
 * with nothing consuming its ring. The three driver units are compiled
 * with sthread_create renamed to this; this unit is not, so the real
 * one is still reachable. */
int mock_thread_fail;

sthread_t *mock_sthread_create(void (*thread_func)(void*), void *userdata)
{
   if (mock_thread_fail)
      return NULL;
   return sthread_create(thread_func, userdata);
}
