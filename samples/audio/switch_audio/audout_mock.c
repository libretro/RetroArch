/* The service behind the stand-in <switch.h>: buffers appended go into
 * a queue, play out oldest first when the test calls audout_mock_play(),
 * and wait in a released queue until the driver takes them back. */

#include <string.h>

#include "switch.h"
#include "audout_mock.h"

#define MOCK_SLOTS 16

static AudioOutBuffer *g_queued[MOCK_SLOTS];
static AudioOutBuffer *g_released[MOCK_SLOTS];
static unsigned g_nqueued, g_nreleased, g_appends;
static size_t   g_bytes, g_last;

void audout_mock_reset(void)
{
   g_nqueued = g_nreleased = g_appends = 0;
   g_bytes   = g_last = 0;
}

void audout_mock_play(unsigned n)
{
   while (n-- && g_nqueued)
   {
      if (g_nreleased < MOCK_SLOTS)
         g_released[g_nreleased++] = g_queued[0];
      memmove(g_queued, g_queued + 1, (g_nqueued - 1) * sizeof(g_queued[0]));
      g_nqueued--;
   }
}

unsigned audout_mock_queued(void)       { return g_nqueued; }
size_t   audout_mock_bytes(void)        { return g_bytes; }
size_t   audout_mock_last_append(void)  { return g_last; }
unsigned audout_mock_appends(void)      { return g_appends; }

Result audoutInitialize(void)    { return 0; }
void   audoutExit(void)          { }
Result audoutStartAudioOut(void) { return 0; }
Result audoutStopAudioOut(void)  { return 0; }
u32    audoutGetSampleRate(void) { return 48000; }

Result audoutAppendAudioOutBuffer(AudioOutBuffer *buffer)
{
   if (g_nqueued >= MOCK_SLOTS)
      return 1;
   g_queued[g_nqueued++] = buffer;
   g_appends++;
   g_bytes += (size_t)buffer->data_size;
   g_last   = (size_t)buffer->data_size;
   return 0;
}

Result audoutGetReleasedAudioOutBuffer(AudioOutBuffer **buffer, u32 *released_count)
{
   if (!g_nreleased)
   {
      *buffer         = NULL;
      *released_count = 0;
      return 0;
   }
   *buffer = g_released[0];
   memmove(g_released, g_released + 1, (g_nreleased - 1) * sizeof(g_released[0]));
   g_nreleased--;
   *released_count = 1;
   return 0;
}

/* A blocking wait: the oldest queued buffer plays out if nothing has
 * been released yet. */
Result audoutWaitPlayFinish(AudioOutBuffer **released, u32 *released_count, u64 timeout)
{
   (void)timeout;
   if (!g_nreleased)
      audout_mock_play(1);
   return audoutGetReleasedAudioOutBuffer(released, released_count);
}
