/* rmodtracker -- ProTracker MOD / Scream Tracker 3 S3M / FastTracker 2 XM
 * replayer for libretro-common.
 *
 * Twin output pipelines sharing one integer sequencer:
 *   - rmodtracker_get_samples_s16_interleaved(): fixed-point mixing end
 *     to end; output is bit-identical across compilers, optimisation
 *     flags and architectures.
 *   - rmodtracker_get_samples_float_interleaved(): the same control
 *     flow with sample-domain arithmetic in float, output in [-1, 1].
 * Getters may be freely alternated; a switch converts at most one tick
 * of carried samples.
 *
 * Replay engine based on ibxm/ac 20191214 (c) Martin Cameron (BSD);
 * licence retained in rmodtracker.c.
 */
#ifndef __LIBRETRO_SDK_FORMAT_RMODTRACKER_H__
#define __LIBRETRO_SDK_FORMAT_RMODTRACKER_H__

#include <stddef.h>
#include <stdint.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

/* Fixed engine mix rate. The audio mixer resamples to the output rate. */
#define RMODTRACKER_RATE 48000

typedef struct rmodtracker rmodtracker;

/* Parse a module (MOD/S3M/XM autodetected) from memory and create a
 * replayer. The data is copied where required; the caller may free the
 * buffer after this returns. Returns NULL on unrecognised or malformed
 * input. */
rmodtracker *rmodtracker_open_memory(const void *data, size_t size);

void rmodtracker_close(rmodtracker *rmt);

/* Always RMODTRACKER_RATE; provided for symmetry with the decoders. */
int rmodtracker_sample_rate(rmodtracker *rmt);

/* Length of one pass through the sequence, in frames at the mix rate. */
int rmodtracker_duration_frames(rmodtracker *rmt);

/* Restart playback from the beginning of the sequence. */
void rmodtracker_rewind(rmodtracker *rmt);

/* Render interleaved stereo. Both return the number of frames written,
 * which is less than 'frames' only at the end of the module. */
size_t rmodtracker_get_samples_s16_interleaved(rmodtracker *rmt,
      int16_t *out, size_t frames);
size_t rmodtracker_get_samples_float_interleaved(rmodtracker *rmt,
      float *out, size_t frames);

RETRO_END_DECLS

#endif
