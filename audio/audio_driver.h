/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __AUDIO_DRIVER__H
#define __AUDIO_DRIVER__H

#include <stdint.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_spsc.h>
#include <retro_atomic.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#else
typedef struct slock slock_t;
typedef struct scond scond_t;
#endif
#include <retro_inline.h>
#include <libretro.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_DSP_FILTER
#include <audio/dsp_filter.h>
#endif
#ifdef HAVE_AUDIOMIXER
#include <audio/audio_mixer.h>
#endif
#include <audio/audio_resampler.h>
#include <audio/sinc_resampler_int16.h>

#include "audio_defines.h"

#define AUDIO_BUFFER_FREE_SAMPLES_COUNT (8 * 1024)

RETRO_BEGIN_DECLS

#ifdef HAVE_AUDIOMIXER
typedef struct audio_mixer_stream
{
   audio_mixer_sound_t *handle;
   audio_mixer_voice_t *voice;
   audio_mixer_stop_cb_t stop_cb;
   void *buf;
   char *name;
   size_t bufsize;
   float volume;
   enum audio_mixer_stream_type stream_type;
   enum audio_mixer_type type;
   enum audio_mixer_state state;
} audio_mixer_stream_t;

typedef struct audio_mixer_stream_params
{
   void *buf;
   char *basename;
   audio_mixer_stop_cb_t cb;
   size_t bufsize;
   unsigned slot_selection_idx;
   float volume;
   enum audio_mixer_slot_selection_type slot_selection_type;
   enum audio_mixer_stream_type stream_type;
   enum audio_mixer_type type;
   enum audio_mixer_state state;
   /* Optional ownership transfer: when buf_owner is non-NULL,
    * add_stream borrows 'buf' from it instead of copying, and
    * buf_owner_free(buf_owner) runs when the stream's sound is
    * destroyed - or immediately on any failure path, or after the
    * conversion for WAV.  Ownership transfers on the call in every
    * outcome; 'buf' must stay valid inside the owner until release.
    * Callers with no owned object set both to NULL and keep today's
    * copy semantics. */
   void *buf_owner;
   void (*buf_owner_free)(void *owner);
   /* Optional: receives the slot index the stream landed in (or -1 on
    * failure).  NULL when the caller does not need it. */
   int *out_slot;
   /* Optional (windowed Ogg-Opus only): the stream's last-page granule,
    * found by the feeder from a bounded tail peek so the decoder need
    * not scan the whole file for it.  0 means not supplied - the
    * decoder does its normal full end-granule scan. */
   int64_t end_granule;
   /* Windowed sources: bytes resident from the start of buf when the
    * stream is added, bounding the decoder's container header parse.
    * 0 means the whole buffer is there. */
   size_t avail;
} audio_mixer_stream_params_t;
#endif

typedef struct audio_driver
{
   /* Creates and initializes handle to audio driver.
    *
    * latency is the user's audio latency setting, in milliseconds. It
    * is the amount of audio the driver should be able to hold between
    * write() returning and the device consuming it - the buffering the
    * driver itself controls, sized to that much of the output format
    * at the rate the device ends up at. A driver that can learn the
    * device's own latency beyond that may subtract it, so the total the
    * user hears approaches the setting; one that cannot reports what it
    * has and leaves the rest to buffer_size() below. The frontend has
    * already applied the policy minimum before calling; a driver only
    * raises latency further for a hardware minimum of its own.
    *
    * Returns: audio driver handle on success, otherwise NULL.
    **/
   void *(*init)(const char *device, unsigned rate,
         unsigned latency, unsigned block_frames, unsigned *new_rate);

   /*
    * @data         : Pointer to audio data handle.
    * @buf          : Audio buffer data.
    * @size         : Size of audio buffer.
    *
    * Write samples to audio driver.
    *
    * Write data in buffer to audio driver.
    * A frame here is defined as one combined sample of left and right
    * channels. (I.e. 44.1kHz, 16-bit stereo has 88.2k samples/s, and
    * 44.1k frames/s.)
    *
    * Samples are interleaved in format LRLRLRLRLR ...
    * If the driver returns true in use_float(), a floating point
    * format will be used, with range [-1.0, 1.0].
    * If not, signed 16-bit samples in native byte ordering will be used.
    *
    * This function returns the number of frames successfully written.
    * If an error occurs, -1 should be returned.
    * Note that non-blocking behavior that cannot write at this time
    * should return 0 as returning -1 will terminate the driver.
    *
    * Unless said otherwise with set_nonblock_state(), all writes
    * are blocking, and it should block till it has written all frames.
    */
   ssize_t (*write)(void *data, const void *s, size_t len);

   /**
    * Temporarily pauses the audio driver.
    *
    * Reports the resulting state, not the work done: a driver that is
    * already stopped returns \c true, because it is stopped. Whether
    * the hardware offers a pause primitive is an implementation detail
    * - a driver with none simply stops consuming samples - so it must
    * not leak into the return value or into \c alive.
    *
    * @param data Opaque handle to the audio driver context
    * that was returned by \c init.
    * @return \c true if the audio driver is now paused,
    * \c false if it could not be.
    **/
   bool (*stop)(void *data);

   /**
    * Resumes audio driver from the paused state.
    *
    * Reports the resulting state, as \c stop does: a driver that is
    * already running returns \c true.
    *
    * Returning \c false is not a soft failure. audio_driver_start()
    * clears AUDIO_FLAG_ACTIVE on it, which disables audio for the rest
    * of the session, so it is reserved for a stream that genuinely
    * cannot be resumed. A driver that can recover by other means -
    * reinitialising the stream, falling back to a restart when a pause
    * primitive it advertised turns out not to work - should do that and
    * return \c true.
    **/
   bool (*start)(void *data, bool is_shutdown);

   /**
    * Is the audio driver currently running?
    *
    * Must agree with the last successful \c stop or \c start. Drivers
    * that track this with a flag have to maintain it on every path
    * through both, including the ones where the hardware call was
    * skipped; a flag updated only inside the branch that talks to the
    * hardware leaves \c alive reporting the opposite of the truth.
    **/
   bool (*alive)(void *data);

   /* Should we care about blocking in audio thread? Fast forwarding.
    *
    * If state is true, nonblocking operation is assumed.
    * This is typically used for fast-forwarding. If driver cannot
    * implement nonblocking writes, this can be disregarded, but should
    * log a message to stderr.
    * */
   void (*set_nonblock_state)(void *data, bool toggle);

   /* Stops and frees driver. */
   void (*free)(void *data);

   /* Defines if driver will take standard floating point samples,
    * or int16_t samples.
    *
    * If true is returned, the audio driver is capable of using
    * floating point data. This will likely increase performance as the
    * resampler unit uses floating point. The sample range is
    * [-1.0, 1.0].
    * */
   bool (*use_float)(void *data);

   /* Human-readable identifier. */
   const char *ident;

   /* Optional. Get audio device list (allocates, caller has to free this).
    *
    * data is the driver context and MAY BE NULL. Enumeration must not
    * require an initialised driver: the frontend builds this list
    * whether or not init succeeded, so the user can pick a device when
    * the current one failed, and refreshes it from the menu. A driver
    * that caches a list at init may return the cache when given its
    * context and must still enumerate when given NULL. */
   void *(*device_list_new)(void *data);

   /* Optional. Frees audio device list. data MAY BE NULL. */
   void (*device_list_free)(void *data, void *data2);

   /**
    * Optional. How much the driver will take right now without
    * blocking, in bytes of the driver's output format - int16 or
    * float stereo frames as use_float() decides. Counts every stage
    * the driver controls that has room: its own fifo or ring, and the
    * device-side buffer where the device reports its fill, so that a
    * device with room and a fifo with room add up. Never more than
    * buffer_size() reports. The rate control samples this once per
    * frame and steers it toward half of buffer_size(); fast-forward
    * bounds the resampler's input by it; the threaded pipeline sizes
    * its passes from it. A driver that cannot measure its fill should
    * leave this NULL rather than report a constant, which reads as a
    * device that never drains.
    */
   size_t (*write_avail)(void *data);

   /**
    * Optional. The most the driver can hold between write() returning
    * and the device consuming it, in the same bytes write_avail()
    * counts: every stage the driver controls, summed - fifo plus
    * engine buffer plus queued blocks. Read once at init, so it is a
    * property of the opened device; a driver whose server renegotiates
    * it afterwards pushes the new size through
    * audio_driver_set_buffer_size(), as pulse does. Half of it is the
    * rate control's setpoint, so half of it, in time, is the latency
    * the user hears from this driver at steady state; a stage the
    * driver cannot see (the hardware's own DMA, a mixing daemon's sink)
    * adds to that and is not reported here. Zero disables rate
    * control for the session, which the frontend logs; a driver that
    * has a buffer reports it rather than zero.
    */
   size_t (*buffer_size)(void *data);

   /**
    * Optional. Write raw int16 samples with resampling handled by driver.
    * If non-NULL, audio_driver will call this instead of doing software
    * resampling. The driver is responsible for resampling from input_rate
    * to its output rate, applying the rate_adjust factor for A/V sync,
    * and applying the volume gain to the output.
    *
    * @param data        Driver context
    * @param samples     Interleaved int16 stereo samples (LRLRLR...)
    * @param frames      Number of frames (pairs of samples)
    * @param input_rate  Source sample rate in Hz
    * @param rate_adjust Rate adjustment multiplier for A/V sync (1.0 = normal)
    * @param volume      Volume gain to apply (0.0 = muted, 1.0 = full volume)
    * @return Number of frames written, or -1 on error
    */
   ssize_t (*write_raw)(void *data, const int16_t *samples, size_t frames,
         unsigned input_rate, double rate_adjust, float volume);

   /**
    * Optional. Sleeps until the device can accept at least len bytes
    * without blocking, then returns how many it will take, as
    * write_avail() would. The sleep must be bounded: 0 means no space
    * is coming from this call - the device is gone, the stream has
    * failed, or nothing drained within the driver's bounded wait - and
    * the caller skips the pass and retries on a later wake, so a
    * stalled device costs dropped audio rather than a parked audio
    * thread. The threaded pipeline calls it with the size of the chunk
    * it is about to write, so the write itself never blocks and the
    * device fill it measures for rate control beforehand is the real
    * one. Drivers without it cannot host the threaded pipeline and
    * keep the inline path.
    */
   size_t (*wait_writable)(void *data, size_t len);

   /**
    * Optional. Frames the device has consumed since start(), in output
    * frames, monotonic and free-running on the device's own clock: a
    * driver with a callback counts what each callback took, one with a
    * queue counts what it released less what the device still holds.
    * Compared over time with the frames the frontend wrote, it gives
    * the device's real sample rate against the host's clock, and the
    * frontend trims its resampling ratio by that - slowly, by parts per
    * million - so a buffer no longer drifts to an underrun or overrun
    * on the difference between two crystals. NULL leaves the frontend
    * without the estimate, as before.
    */
   size_t (*frames_consumed)(void *data);

   /* Optional. Periods the device played silence for want of audio
    * since init: the callback found less than one period in the
    * buffer and zero-filled it. Counted where it happens, one atomic
    * add on that path only; never logged from there. Read by the
    * frontend for the statistics overlay each frame, and once at
    * driver teardown for the log. NULL when the driver cannot tell. */
   size_t (*underruns)(void *data);
} audio_driver_t;

/* A snapshot of the sink estimate's counts, taken when a window opens. */
typedef struct
{
   double   offered;                   /* sink_offered */
   uint64_t consumed;                  /* frames_consumed() */
} audio_sink_mark_t;

/* A sum of windows: their time, and what the source and the device did. */
typedef struct
{
   int64_t  usec;
   double   offered;
   double   consumed;
} audio_sink_sum_t;

typedef struct
{
   double src_ratio_orig;
   double src_ratio_curr;

   uint64_t free_samples_count;

   struct string_list *devices_list;
   /* The driver whose device_list_new built devices_list, so that its
    * device_list_free is the one that releases it - not whichever
    * driver is configured or running when the list is next rebuilt. */
   const audio_driver_t *devices_list_driver;

   /**
    * A scratch buffer for audio output to be processed,
    * up to (but excluding) the point where it's converted to 16-bit audio
    * to give to the driver.
    */
   float *output_samples_buf;
   size_t output_samples_buf_length;
#ifdef HAVE_REWIND
   int16_t *rewind_buf;
#endif

   /**
    * The driver's int16 output staging buffer. Holds the final 16-bit samples
    * that are sent to the audio driver and to recording, from whichever source
    * produced them: the float->s16 conversion of the float resampler's output,
    * or the integer s16 resampler writing here directly.
    */
   int16_t *output_samples_int16;
   size_t output_samples_int16_length;
   /**
    * Accumulator for the single-sample core callback
    * (audio_driver_sample). Holds AUDIO_SAMPLE_ACCUM_INT16S int16 samples;
    * data_ptr is the write index. Emptied by audio_driver_frame_end() once
    * the core has returned from retro_run(), or by the overflow guard in
    * audio_driver_sample() when a single frame delivers more than it holds.
    * Separate from output_samples_int16 so a flush never resamples in place
    * over its own input.
    */
   int16_t *sample_accum;
#ifdef HAVE_DSP_FILTER
   retro_dsp_filter_t *dsp;
#endif
   const retro_resampler_t *resampler;

   void *resampler_data;

   /* Optional deterministic integer (s16) resampler, used by the s16 path
    * in audio_driver_flush() when the selected resampler has an int16
    * implementation ("sinc", "nearest", "CC") and no float-domain stage is
    * active.  NULL otherwise.  The process/free entry points are selected
    * alongside the handle so the s16 path is backend-agnostic. */
   void *resampler_data_int16;
   void (*resampler_int16_process)(void *, struct resampler_data_int16 *);
   void (*resampler_int16_free)(void *);

   /**
    * The current audio driver.
    */
   const audio_driver_t *current_audio;

   void *context_audio_data;

   /**
    * Scratch buffer for preparing data for the resampler
    */
   float *input_data;
   float *synth_buf;
   /* int16 scratch for the s16 path: running a fully-int16 DSP chain
    * and/or summing an in-process synth without an int16<->float round-trip.
    * Allocated only when an int16 resampler exists, since that path cannot
    * run without one; NULL otherwise. */
   int16_t *input_data_int16;
   size_t input_data_length;
#ifdef HAVE_AUDIOMIXER
   struct audio_mixer_stream mixer_streams[AUDIO_MIXER_MAX_SYSTEM_STREAMS];
#endif
   struct retro_audio_callback callback;                 /* ptr alignment */
   /**
    * Backing storage for every int16 and every float scratch buffer the
    * driver owns. The named buffer pointers below (output_samples_int16,
    * sample_accum, rewind_buf, input_data_int16; input_data, synth_buf,
    * output_samples_buf) all point into one of these two blocks and are
    * never freed individually. See audio_driver_init_internal() for the
    * layout.
    */
   int16_t *arena_int16;                                 /* ptr alignment */
   float   *arena_float;                                 /* ptr alignment */
   /**
    * Threaded pipeline (AUDIO_FLAG_PIPELINE_THREADED). pipe_ring carries
    * raw int16 stereo frames at the core's rate from the main thread to
    * the audio thread; lock-free, one producer (frame end / rewind /
    * menu audio, all main thread) and one consumer (the wrapper thread).
    * pipe_scratch is the consumer's bounce buffer for one slice pulled
    * out of the ring; pipe_conv is the producer's staging area for the
    * float batch callback, which must be int16 before it is published.
    * Both are regions of arena_int16.
    */
   retro_spsc_t pipe_ring;
   int16_t *pipe_scratch;
   int16_t *pipe_conv;
   /* Written once by the wrapper thread as it leaves its loop, read by
    * the producer's wait. Its own field, not a bit in flags: the main
    * thread read-modify-writes flags and a second writer would lose
    * bits. */
   volatile bool pipe_consumer_gone;
   /* Throttle channel for audio_sync without vsync: the consumer bumps
    * pipe_gen under pipe_lock after every pass and signals pipe_cond;
    * a producer that found the ring full waits for the generation to
    * change. The ring itself is never touched under this lock; the lock
    * only orders the two generation counters against their waits. */
   /**
    * Held across a pipeline pass, and by the main thread whenever it
    * changes something a pass reads: the DSP filter pointer and the
    * mixer's voices. With the threaded pipeline a pass runs on the
    * audio thread, so a menu action that swaps the DSP filter or
    * starts a sound would otherwise race it - the filter free is a
    * use-after-free window, the mixer a torn voice. Uncontended on
    * the frame-synchronous path, where both are the same thread.
    * Never held across a device write.
    */
   slock_t *state_lock;
   slock_t *pipe_lock;
   scond_t *pipe_cond;
   unsigned pipe_gen;
   /* Data channel the other way: the producer bumps pipe_data_gen and
    * signals pipe_data_cond after every publish; the consumer sleeps on
    * it while the ring is empty. */
   scond_t *pipe_data_cond;
   unsigned pipe_data_gen;
   /* Set by audio_driver_pipeline_wake() under pipe_lock and cleared by
    * the consumer when it acts on it. Sticky, unlike the signal, so a
    * wake raised before the consumer reaches its wait is not lost. */
   bool     pipe_wake;
   /* Set by the producer under pipe_lock when a full ring did not drain
    * within its bounded wait, cleared by the consumer when a pass
    * completes. While set, the producer drops rather than waits, so a
    * device that has stopped draining costs the frame nothing beyond
    * the one wait that found it out. */
   bool     pipe_stalled;
   /**
    * What the audio thread needs to know about the runloop and the
    * menu, published by the main thread with
    * audio_driver_publish_runloop() at every frame end and on every
    * start and stop, so the audio thread never reads
    * runloop_state.flags, menu state or settings directly. A frame of
    * staleness in "paused" or "fast-forward" is harmless; a torn or
    * racing read of the runloop's flag word is not.
    */
   retro_atomic_int_t runloop_snapshot;
   /* Upper bound on input samples per consumer pass: one video frame's
    * worth, capped to a slice. */
   size_t   pipe_pass_int16s;
   /* Rate control's fill on the threaded pipeline, as free space in
    * device bytes, sampled by the producer before each publish and
    * read by the consumer's controller; -1 before the first sample.
    *
    * The device's own fill cannot be the control variable here: the
    * consumer waits for half the device's buffer and writes half, so
    * that fill sits between half and full whatever the clocks do, and a
    * controller reading it - at any point of the pass - sees a constant
    * error and pins the ratio at a bound. What the clocks move is the
    * pipe ring in front of the device: a production surplus collects
    * there, a deficit empties it. So the fill is the two together, the
    * pipe's frames counted at the device's rate, and it is read where
    * the frame-synchronous path read it, once a frame on the core's
    * thread before the frame is published - the pipe at its low point,
    * the device wherever it is. The device's mean free space over the
    * pass is a quarter of its buffer, the controller's setpoint is
    * half, and the difference is added so a balanced pipe reads as no
    * error. */
   retro_atomic_int_t pipe_ctrl_avail;
   /* The driver's underrun count as the consumer last saw it; a change
    * means the device played silence since, and what the pipe holds
    * past its target is late audio, discarded. Consumer thread only. */
   size_t   pipe_underruns_seen;
   /* Whether the consumer's next pass is its first: it waits for the
    * pipe's target then, not just a frame. Consumer thread only after
    * init. */
   bool     pipe_priming;
   /* The audio thread's own copy of AUDIO_FLAG_PIPELINE_THREADED. Set
    * before the wrapper thread is released and cleared after it is
    * joined, so the thread never reads the flags word - which the main
    * thread read-modify-writes at will - just to know it is running. */
   bool pipe_threaded;
   /* The fast-forward speedup multiplier, measured by the producer on
    * the core's thread and read by the consumer, as a Q16 fixed-point
    * value. The consumer's own cadence is the device's, so it cannot
    * measure how fast the core is running; only the producer can. */
   retro_atomic_int_t pipe_ff_mult_q16;
#ifdef HAVE_REWIND
   size_t rewind_ptr;
   size_t rewind_size;
#endif
   size_t buffer_size;
   /* The device stage behind the driver's buffer, in frames at the
    * output rate, as the driver reports it: what ASIOGetLatencies gives
    * for output, say. 0 when the driver reports none. Set by the driver
    * through audio_driver_set_device_latency(); shown in the statistics
    * overlay next to the buffer, since it is the part of the path the
    * setting cannot reach and the part that differs most between
    * devices. */
   size_t device_latency_frames;
   size_t data_ptr;

   unsigned free_samples_buf[AUDIO_BUFFER_FREE_SAMPLES_COUNT];

#ifdef HAVE_AUDIOMIXER
   float mixer_volume_gain;
#endif

   float rate_control_delta;
   float input;
   float volume_gain;

   enum resampler_quality resampler_quality;

   /**
    * AUDIO_FLAG_* word. Read-modify-written by the main thread and
    * read by the audio thread (core audio callbacks, the threaded
    * pipeline), so it is an atomic int accessed only through
    * AUDIO_FLAGS_GET / AUDIO_FLAGS_SET / AUDIO_FLAGS_CLEAR: the RMWs
    * cannot lose bits against each other and a reader always sees a
    * whole word. Individual bits are still only meaningful together
    * with the ordering their setters already establish (driver start
    * before the thread runs, init before start, and so on).
    */
   retro_atomic_int_t flags;

   char resampler_ident[64];

   /* A driver's request to be reinitialised - a device change or an
    * unplug, set from its notification thread. Taken by the runloop
    * once a frame on the main thread with no audio lock held; see
    * audio_driver_take_reinit_request(). */
   retro_atomic_int_t reinit_request;
   bool mute_enable;
#ifdef HAVE_AUDIOMIXER
   bool mixer_mute_enable;
   uint8_t mixer_streams_playing;  /* Count of currently playing mixer streams */
#endif

   /* Sample the flush delta-time when fast forwarding to find the correct ratio. */
   retro_time_t last_flush_time;
   /* Exponential moving average */
   retro_time_t avg_flush_delta;

   /* Rate-limit state for the DRC compute.
    *
    * The DRC ratio is updated approximately once per game-frame's worth
    * of submitted samples rather than once per audio_driver_sample_batch
    * call. Cores that submit a single batch per retro_run see DRC fire
    * once per frame as before. Cores that submit N sub-frame batches
    * per retro_run see DRC fire approximately once per frame (when the
    * cumulative samples cross the threshold) instead of N times per
    * frame. This:
    *   - removes (N-1) per-call audio->write_avail calls (a syscall on
    *     ALSA/WASAPI/Pulse/PipeWire backends; cheap on CoreAudio)
    *   - stabilises the resampler ratio (the DRC was designed around
    *     a once-per-frame time constant; per-batch updates sample
    *     write_avail at sub-frame phase, adding noise to the loop)
    *
    * drc_threshold_int16s is recomputed by audio_driver_update_drc_threshold
    * from the current input sample rate and av_info.timing.fps whenever
    * audio_driver_init_internal runs (which is also where the input
    * rate gets set, and where SET_SYSTEM_AV_INFO drives reinit). This
    * keeps the "one frame's worth" target accurate at any output sample
    * rate (48 kHz, 96 kHz, 192 kHz, ...) and any content fps. */
   double   cached_rate_adjust;        /* last computed factor; default 1.0 */

   /* Sink rate estimation: see audio_driver_sink_update(). Counted
    * where the driver's write() is called, on the thread that flushes;
    * the estimate runs there too. */
   double   sink_offered;              /* the source's count, in frames at the nominal
                                          ratio: what the core published into the
                                          pipeline's ring, or off it what was offered
                                          with the ratio in force divided out. Owned
                                          by the thread that closes the windows. */
   uint64_t sink_offered_raw;          /* output frames offered, as offered */
   uint64_t sink_accepted;             /* output frames the driver took */
   /* The estimate: windows of a few seconds, each read as a change in
    * the counts above and in the device's consumption, kept when it
    * measures the clocks and summed; the bias is the summed ratio.
    * See audio_driver_sink_update(). */
   int64_t  sink_started;              /* usec; 0 = not started */
   int64_t  sink_window_at;            /* usec; when the open window closes */
   int64_t  sink_apply_at;             /* usec; the next setting of the bias */
   audio_sink_mark_t sink_at_window;   /* the counts when the open window opened */
   audio_sink_sum_t  sink_kept;        /* the windows summed for the bias */
   audio_sink_sum_t  sink_pending;     /* windows since the last kept one, for the rates shown meanwhile */
   unsigned sink_settled;              /* kept windows in a row, up to 2, after which the sums stand */
   unsigned sink_applied;              /* times the bias has been set */
   unsigned sink_discarded;            /* windows left out in a row */
   unsigned sink_warned;               /* AUDIO_SINK_WARNED_* said once each */
   double   sink_bias;                 /* multiplied into the ratio; 1.0 = none */
   /* The bias for the thread that resamples, in hundredths of a part
    * per million: on the threaded pipeline the estimate runs on the
    * core's thread and the resampler on the audio thread, and a double
    * is not a single word. */
   retro_atomic_int_t sink_bias_q;
   double   sink_rate_hz;              /* the device's rate as measured; 0 = unknown */
   double   sink_source_hz;            /* the source's rate at the nominal ratio, as measured */
   size_t   samples_since_drc;         /* int16 samples submitted since last update */
   size_t   drc_threshold_int16s;      /* one frame's worth of stereo int16 at the current rate */
   /* Set by audio_driver_frame_end() so the next flush recomputes the
    * DRC factor regardless of samples_since_drc. Pins the write_avail()
    * measurement to the same point of every frame - the first flush the
    * core makes after retro_run() returns - instead of wherever the
    * sample-count gate happens to trip. */
   bool     drc_pending;

   /* Last-flush sample-format diagnostics for the on-screen statistics
    * overlay. stat_core_is_float records whether the core delivered float
    * (audio_driver_sample_batch_float) or int16 (audio_driver_sample_batch)
    * samples; stat_frontend_is_float records whether the frontend processed
    * that audio through the float resampler path (true) or an integer path
    * (false: either the write_raw raw-int16 fast path or the deterministic
    * s16 resampler path). */
   bool     stat_core_is_float;
   bool     stat_frontend_is_float;

   /* Unity passthrough state: set when the float path skipped the
    * resampler because the ratio was exactly 1.0 (see audio_driver_flush).
    * Used to re-initialise the resampler on the transition back to actual
    * resampling so it does not resume from a stale ring buffer. */
   bool     resampler_bypassed;
} audio_driver_state_t;

bool audio_driver_enable_callback(void);

bool audio_driver_disable_callback(void);

bool audio_driver_mixer_extension_supported(const char *ext);

void audio_driver_dsp_filter_free(void);

bool audio_driver_dsp_filter_init(const char *device);

void audio_driver_set_buffer_size(size_t bufsize);

/* Records the device stage behind the driver's buffer, in frames at the
 * output rate; 0 to say the driver reports none. Reset when a driver is
 * initialised, so a driver that reports one calls this after each init
 * or reinit, and again if the device changes it. */
void audio_driver_set_device_latency(size_t frames);

bool audio_driver_get_devices_list(void **ptr);

void audio_driver_setup_rewind(void);

/**
 * audio_driver_set_nonblock_state:
 *
 * Hands the blocking state to the driver and records it in
 * AUDIO_FLAG_NONBLOCK so the threaded pipeline's producer can see it.
 * Every caller that used to reach current_audio->set_nonblock_state()
 * directly goes through here.
 **/
void audio_driver_set_nonblock_state(bool nonblock);

/**
 * audio_driver_pipeline_consumer_exit:
 *
 * Called by the audio thread wrapper as its thread leaves the loop, so
 * a producer waiting for ring space stops waiting for a consumer that
 * will never run again.
 **/
void audio_driver_pipeline_consumer_exit(void);

/**
 * audio_driver_pipeline_wake:
 *
 * Wakes a consumer sleeping for data and a producer sleeping for room,
 * without giving either. The audio thread wrapper calls it before it
 * joins its thread, so a consumer parked on an empty ring returns to
 * the loop and sees that it is being shut down instead of sleeping
 * out its timeout.
 **/
void audio_driver_pipeline_wake(void);

/**
 * audio_driver_state_lock:
 * audio_driver_state_unlock:
 *
 * Guards the DSP filter and the mixer against a pipeline pass on the
 * audio thread. Cheap and safe to call before the audio driver is up:
 * the lock exists for the driver's lifetime and both are no-ops
 * without it.
 **/
void audio_driver_state_lock(void);
void audio_driver_state_unlock(void);

/* Bits of audio_driver_state_t::runloop_snapshot. */
enum audio_runloop_snapshot_bits
{
   AUDIO_SNAP_PAUSED      = (1 << 0),
   AUDIO_SNAP_SLOWMOTION  = (1 << 1),
   AUDIO_SNAP_FASTMOTION  = (1 << 2),
   AUDIO_SNAP_MENU_ALIVE  = (1 << 3),
   AUDIO_SNAP_MENU_PAUSES = (1 << 4),
   AUDIO_SNAP_ALLOW_PAUSE = (1 << 5)
};

/**
 * audio_driver_publish_runloop:
 *
 * Main thread only. Captures the runloop, menu and setting bits the
 * audio thread consults into runloop_snapshot. Called from the frame
 * end and from audio_driver_start()/stop(); cheap enough to call
 * anywhere else those bits change.
 **/
void audio_driver_publish_runloop(void);

/* Accessors for audio_driver_state_t::flags; see the field. GET is an
 * acquire load, SET/CLEAR are acq_rel RMWs, all returning the whole
 * word so a caller can test bits on the result. */
#define AUDIO_FLAGS_GET(st)         retro_atomic_load_acquire_int(&(st)->flags)
#define AUDIO_FLAGS_SET(st, bits)   retro_atomic_fetch_or_int(&(st)->flags, (bits))
#define AUDIO_FLAGS_CLEAR(st, bits) retro_atomic_fetch_and_int(&(st)->flags, ~(bits))

/**
 * audio_driver_frame_end:
 *
 * Marks the end of one emulated frame on the thread that produced it.
 * Flushes whatever the single-sample callback accumulated during
 * retro_run() and arms a DRC recompute for the next flush. Must be
 * called after every retro_run() of the running core, on the same
 * thread. No-op when a core audio callback is registered, since that
 * core delivers audio on the audio thread and audio_driver_callback()
 * flushes it there.
 **/
void audio_driver_frame_end(void);

bool audio_driver_callback(void);

bool audio_driver_has_callback(void);

void audio_driver_frame_is_reverse(void);

void audio_set_float(enum audio_action action, float val);

float *audio_get_float_ptr(enum audio_action action);

bool *audio_get_bool_ptr(enum audio_action action);

#ifdef HAVE_AUDIOMIXER
audio_mixer_stream_t *audio_driver_mixer_get_stream(unsigned i);

bool audio_driver_mixer_add_stream(audio_mixer_stream_params_t *params);

/* Compressed-byte read position of the stream in 'slot' (its
 * decoder's offset within the source buffer), or -1 when the slot
 * holds no live stream.  The windowed-source feeder's polling
 * input. */
int64_t audio_driver_mixer_stream_byte_tell(unsigned i);

/* Raise a windowed stream's resident prefix as its feeder slides the
 * window forward.  The mirror of audio_driver_mixer_stream_byte_tell. */
void audio_driver_mixer_stream_set_avail(unsigned i, size_t avail);

void audio_driver_mixer_play_stream(unsigned i);

void audio_driver_mixer_play_menu_sound(unsigned i);

void audio_driver_mixer_play_scroll_sound(bool direction_up);

void audio_driver_mixer_play_menu_sound_looped(unsigned i);

void audio_driver_mixer_play_stream_sequential(unsigned i);

void audio_driver_mixer_play_stream_looped(unsigned i);

void audio_driver_mixer_stop_stream(unsigned i);

float audio_driver_mixer_get_stream_volume(unsigned i);

void audio_driver_mixer_set_stream_volume(unsigned i, float vol);

void audio_driver_mixer_remove_stream(unsigned i);

enum audio_mixer_state audio_driver_mixer_get_stream_state(unsigned i);

const char *audio_driver_mixer_get_stream_name(unsigned i);

unsigned audio_driver_mixer_get_streams_playing(void);

void audio_driver_load_system_sounds(void);

#endif

bool audio_driver_start(bool is_shutdown);

bool audio_driver_stop(void);

#ifdef HAVE_TRANSLATE
/* TODO/FIXME - Doesn't currently work.  Fix this. */
bool audio_driver_is_ai_service_speech_running(void);
#endif

/**
 * audio_compute_buffer_statistics:
 *
 * Computes audio buffer statistics.
 *
 **/
bool audio_compute_buffer_statistics(audio_statistics_t *stats);

bool audio_driver_init_internal(void *data, bool audio_cb_inited);

bool audio_driver_deinit(void);

/* Rebuild audio_driver_st.devices_list from the driver that is, or
 * would be, in use. Does not require the driver to be initialised. */
void audio_driver_refresh_devices_list(void);

bool audio_driver_find_driver(const char *audio_drv,
      const char *prefix, bool verbosity_enabled);

/**
 * audio_driver_sample:
 * @left                 : value of the left audio channel.
 * @right                : value of the right audio channel.
 *
 * Audio sample render callback function.
 **/
void audio_driver_sample(int16_t left, int16_t right);

/**
 * audio_driver_sample_batch:
 * @data                 : pointer to audio buffer.
 * @frames               : amount of audio frames to push.
 *
 * Batched audio sample render callback function.
 *
 * Returns: amount of frames sampled.
 **/
size_t audio_driver_sample_batch(const int16_t *data, size_t frames);

/**
 * audio_driver_sample_batch_float:
 *
 * Float counterpart of audio_driver_sample_batch(), handed to a core via
 * RETRO_ENVIRONMENT_GET_AUDIO_SAMPLE_BATCH_FLOAT. Samples are interleaved
 * stereo float normalized to [-1.0, 1.0].
 *
 * @return Number of frames processed.
 **/
size_t audio_driver_sample_batch_float(const float *data, size_t frames);

#ifdef HAVE_REWIND
/**
 * audio_driver_sample_rewind:
 * @left                 : value of the left audio channel.
 * @right                : value of the right audio channel.
 *
 * Audio sample render callback function (rewind version).
 * This callback function will be used instead of
 * audio_driver_sample when rewinding is activated.
 **/
void audio_driver_sample_rewind(int16_t left, int16_t right);

/**
 * audio_driver_sample_batch_rewind:
 * @data                 : pointer to audio buffer.
 * @frames               : amount of audio frames to push.
 *
 * Batched audio sample render callback function (rewind version).
 *
 * This callback function will be used instead of
 * audio_driver_sample_batch when rewinding is activated.
 *
 * Returns: amount of frames sampled.
 **/
size_t audio_driver_sample_batch_rewind(
      const int16_t *data, size_t frames);
#endif

#ifdef HAVE_MENU
void audio_driver_menu_sample(void);
#endif

extern audio_driver_t audio_rsound;
extern audio_driver_t audio_audioio;
extern audio_driver_t audio_oss;
extern audio_driver_t audio_alsa;
extern audio_driver_t audio_tinyalsa;
extern audio_driver_t audio_roar;
extern audio_driver_t audio_openal;
extern audio_driver_t audio_opensl;
extern audio_driver_t audio_jack;
extern audio_driver_t audio_sdl;
extern audio_driver_t audio_sdl3;
extern audio_driver_t audio_xa;
extern audio_driver_t audio_pulse;
extern audio_driver_t audio_pipewire;
extern audio_driver_t audio_dsound;
extern audio_driver_t audio_wasapi;
#ifdef HAVE_ASIO
extern audio_driver_t audio_asio;
/* Opens the running ASIO driver's control panel. False when ASIO is not
 * the driver running - it was picked in the menu and audio has not
 * been reinitialised since - so the caller can say so. */
bool audio_asio_open_control_panel(void);
bool audio_asio_output_channel_name(unsigned ch, char *buf, size_t len);
unsigned audio_asio_output_channel_count(void);
#endif
extern audio_driver_t audio_coreaudio;
extern audio_driver_t audio_xenon360;
extern audio_driver_t audio_ps3;
extern audio_driver_t audio_gx;
extern audio_driver_t audio_ax;
extern audio_driver_t audio_psp;
extern audio_driver_t audio_ps2;
extern audio_driver_t audio_ctr_csnd;
extern audio_driver_t audio_ctr_dsp;
#ifdef HAVE_THREADS
#endif
extern audio_driver_t audio_switch;
extern audio_driver_t audio_switch_libnx_audren;
extern audio_driver_t audio_rwebaudio;
extern audio_driver_t audio_audioworklet;

audio_driver_state_t *audio_state_get_ptr(void);

/**
 * audio_driver_update_drc_threshold:
 *
 * Recompute drc_threshold_int16s for the current sample rate / fps.
 * Called from audio_driver_init_internal whenever audio is initialized
 * or reinitialised (which covers SET_SYSTEM_AV_INFO and output-rate
 * setting changes), and from refresh-rate change paths in retroarch.c
 * that update audio_driver_state_t::input without driving a full audio
 * reinit.
 **/
void audio_driver_update_drc_threshold(audio_driver_state_t *audio_st);

const char *audio_driver_get_ident(void);

/* Periods the device played silence for want of audio since the driver
 * was initialised, where the driver counts them; 0 otherwise. */
size_t audio_driver_get_underruns(void);

/* Whether a driver has asked to be reinitialised since the last call;
 * clears the request. The runloop calls this once a frame, on the main
 * thread, holding no audio lock, and acts on it there: the reinit
 * tears the driver down and up, which frees the state lock and, on
 * the threaded pipeline, joins the audio thread, so it can be run
 * neither under the lock nor from that thread. */
bool audio_driver_take_reinit_request(void);

double audio_driver_get_buffer_latency_ms(void);

/* The device stage behind the buffer in ms, as set by
 * audio_driver_set_device_latency(); 0 when the driver reports none. */
double audio_driver_get_device_latency_ms(void);

/* The device's sample rate as measured against the host clock, in Hz,
 * and the ratio bias applied for it; 0 when no driver reports
 * frames_consumed() or no window has completed yet. */
double audio_driver_get_sink_rate_hz(double *bias, double *source_hz);

extern audio_driver_t *audio_drivers[];

RETRO_END_DECLS

#endif /* __AUDIO_DRIVER__H */
