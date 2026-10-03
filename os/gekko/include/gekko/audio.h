/* Audio DMA: 16-bit big-endian stereo PCM from memory to the output.
 *
 * The DMA plays one buffer and then whichever buffer was queued while
 * it played; the callback runs (in interrupt context) each time a
 * buffer starts, which is the moment to queue the next one. */

#ifndef GEKKO_AUDIO_H
#define GEKKO_AUDIO_H

#include <gekko/gekko.h>

typedef void (*gk_audio_fn)(void *data);

/* rate: 48000 or 32000. */
void gk_audio_init(unsigned rate);
void gk_audio_set_cb(gk_audio_fn fn, void *data);
/* The buffer played after the current one: 32-byte aligned, a
 * multiple of 32 bytes (at most 1 MiB), flushed by the caller. */
void gk_audio_queue(const void *pcm, size_t bytes);
void gk_audio_start(void);
void gk_audio_stop(void);
/* Bytes left in the buffer playing now. */
size_t gk_audio_remaining(void);

#endif
