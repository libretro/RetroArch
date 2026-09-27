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
/* The window the device is playing: the one the last output call
 * queued, still being read by the DMA. The next call plays it out
 * before it queues its own. */
static const uint32_t *playing;

void mock_device_reset(void)
{
   retro_atomic_size_init(&mock_periods, 0);
   retro_atomic_size_init(&mock_silent, 0);
   retro_atomic_size_init(&mock_breaks, 0);
   expect       = 0;
   primed       = 0;
   playing      = 0;
}

/* One window's worth of playback: the DMA reads the frames as the
 * period passes, in slices, so a writer that lands in the window
 * after the call that queued it returned is caught the way the
 * hardware catches it - part of the window played from before the
 * write, part from after. */
#define MOCK_SLICES 4u

static void mock_play(const uint32_t *w)
{
   unsigned i, slice;
   uint64_t acc = 0;
   int silent   = 1;
   int primed_here = primed;
   uint32_t exp = expect;

   for (slice = 0; slice < MOCK_SLICES; slice++)
   {
      usleep(MOCK_PERIOD_US / MOCK_SLICES);
      for (i = slice * (MOCK_GRAIN / MOCK_SLICES);
            i < (slice + 1) * (MOCK_GRAIN / MOCK_SLICES); i++)
      {
         acc += w[i];
         if (w[i])
            silent = 0;
      }
   }
   sink += acc;

   if (silent)
      retro_atomic_fetch_add_size(&mock_silent, 1);
   else
   {
      /* The first window seen sets the baseline. */
      if (!primed_here)
      {
         exp = w[0];
         primed = 1;
      }
      for (i = 0; i < MOCK_GRAIN; i++)
      {
         if (w[i] != exp)
         {
            if (MOCK_READ(mock_breaks) < 4)
               fprintf(stderr,
                     "      break at frame %u of the window: got %u, "
                     "expected %u\n", i, (unsigned)w[i], (unsigned)exp);
            retro_atomic_fetch_add_size(&mock_breaks, 1);
            exp = w[i];
         }
         exp++;
      }
      expect = exp;
   }
   retro_atomic_fetch_add_size(&mock_periods, 1);
}

/* The output call as the SDKs document it: blocks until the window
 * queued by the previous call has been output, then queues this one
 * and returns with it still to be read. sceAudioOutOutput(port, NULL)
 * is the documented way to wait for that last window, which is why a
 * normal call cannot have. */
static void mock_consume(const void *buf)
{
   if (playing)
      mock_play(playing);
   playing = (const uint32_t*)buf;
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
