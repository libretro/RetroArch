/* The queue behind the stand-in <xenon_sound/sound.h>: libxenon's ring
 * is 64 KiB, xenon_sound_submit() copies into it without looking at what
 * is unplayed, and xenon_sound_get_unplayed() reports the bytes still to
 * play. A submit that would lap the ring is counted as an overrun. */
#include "xenon_sound/sound.h"
#include "time/time.h"
#include "xenon_mock.h"

#define RING 65536

static size_t g_unplayed, g_peak, g_submitted;

void   xenon_mock_reset(void)        { g_unplayed = g_peak = g_submitted = 0; }
size_t xenon_mock_unplayed(void)     { return g_unplayed; }
size_t xenon_mock_peak(void)         { return g_peak; }
size_t xenon_mock_submitted(void)    { return g_submitted; }

void xenon_mock_play(size_t bytes)
{
   g_unplayed = bytes >= g_unplayed ? 0 : g_unplayed - bytes;
}

void xenon_sound_init(void) { }

void xenon_sound_submit(void *data, int len)
{
   (void)data;
   g_unplayed  += (size_t)len;
   g_submitted += (size_t)len;
   if (g_unplayed > g_peak)
      g_peak = g_unplayed;
}

int xenon_sound_get_free(void)
{
   return g_unplayed >= RING ? 0 : (int)(RING - g_unplayed);
}

int xenon_sound_get_unplayed(void) { return (int)g_unplayed; }

/* 50 us of 48 kHz int16 stereo is 9.6 bytes; play a frame's worth at
 * least so a waiting write makes progress. */
void udelay(int us)
{
   size_t bytes = (size_t)us * 192 / 1000;
   xenon_mock_play(bytes < 4 ? 4 : bytes);
}
