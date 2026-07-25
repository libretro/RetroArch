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

/* Default mix rate, used by rmodtracker_open_memory. */
#define RMODTRACKER_RATE 48000

/* The range rmodtracker_open_memory_rate accepts. */
#define RMODTRACKER_RATE_MIN 8000
#define RMODTRACKER_RATE_MAX 192000

typedef struct rmodtracker rmodtracker;

/* Parse a module (MOD/S3M/XM autodetected) from memory and create a
 * replayer. The data is copied where required; the caller may free the
 * buffer after this returns. Returns NULL on unrecognised or malformed
 * input. */
rmodtracker *rmodtracker_open_memory(const void *data, size_t size);

/* The same, mixed at sample_rate rather than RMODTRACKER_RATE. A
 * replayer synthesises at whatever rate it is asked for, so a caller
 * that knows its output rate should say so and be handed audio it does
 * not have to resample - which is both a resampling stage saved and the
 * one that would otherwise sit between the synthesis and the output.
 * Rates outside RMODTRACKER_RATE_MIN..MAX are refused. Everything the
 * rest of this header counts in frames - the duration, a seek target -
 * is at the rate chosen here. */
rmodtracker *rmodtracker_open_memory_rate(const void *data, size_t size,
      int sample_rate);

void rmodtracker_close(rmodtracker *rmt);

/* The rate this module is being mixed at. */
int rmodtracker_sample_rate(rmodtracker *rmt);

/* Length of one pass through the sequence, in frames at the mix rate. */
/* Voices the module itself has - four in a classic MOD, up to
 * thirty-two in an XM or S3M.  Not the channel count of the audio it
 * produces: the replayer mixes those voices down to interleaved
 * stereo, so two is always what comes out. */
int rmodtracker_voices(rmodtracker *rmt);

int rmodtracker_duration_frames(rmodtracker *rmt);

/* Restart playback from the beginning of the sequence. */
void rmodtracker_rewind(rmodtracker *rmt);

/* Move playback to 'frame', counted from the start of the sequence at
 * the mix rate. Returns the frame actually reached, which is short of
 * the one asked for only when the song ends first.
 *
 * A module has no seek table: its state at any moment is the result of
 * every row played before it, so this restarts and works forward. The
 * mixing is skipped, which is nearly all of the cost, but the walk is
 * still proportional to the distance - seeking to the end of a long
 * module is not free, and it is done on the calling thread. Seeking to
 * 0 is rewinding and costs nothing.
 *
 * A module loops, so every frame number names a position in the stream
 * and an accidental one would be walked to just as faithfully as a
 * deliberate one. The target is therefore capped at a single pass,
 * which bounds the work; ask for more and the return value says where
 * it stopped. */
int rmodtracker_seek(rmodtracker *rmt, int frame);

/* Render interleaved stereo. Both return the number of frames written,
 * which is less than 'frames' only at the end of the module. */
size_t rmodtracker_get_samples_s16_interleaved(rmodtracker *rmt,
      int16_t *out, size_t frames);
size_t rmodtracker_get_samples_float_interleaved(rmodtracker *rmt,
      float *out, size_t frames);

RETRO_END_DECLS

#endif
