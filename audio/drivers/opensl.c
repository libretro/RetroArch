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

#include <SLES/OpenSLES.h>
#ifdef ANDROID
#include <SLES/OpenSLES_Android.h>
#endif

#include <string.h>

#include <retro_atomic.h>
#include <retro_math.h>
#include <retro_timers.h>
#include <rthreads/retro_eventcount.h>

#include "../audio_driver.h"
#include "../../verbosity.h"

/* Upper bound on how long a blocking write will wait for the buffer
 * queue callback before giving up and reporting a short write.  Never
 * reached in normal operation - the callback fires as each enqueued
 * block finishes - so the value only decides how long a device that
 * has stopped consuming takes to be noticed.  Matches the flat
 * bail-out timeouts in sdl2_audio.c and wasapi.c. */
#define OPENSL_STALL_TIMEOUT_US 256000

/* Helper macros, COM-style. */
#define SLObjectItf_Realize(a, ...) ((*(a))->Realize(a, __VA_ARGS__))
#define SLObjectItf_GetInterface(a, ...) ((*(a))->GetInterface(a, __VA_ARGS__))
#define SLObjectItf_Destroy(a) ((*(a))->Destroy((a)))

#define SLEngineItf_CreateOutputMix(a, ...) ((*(a))->CreateOutputMix(a, __VA_ARGS__))
#define SLEngineItf_CreateAudioPlayer(a, ...) ((*(a))->CreateAudioPlayer(a, __VA_ARGS__))

#define SLPlayItf_SetPlayState(a, ...) ((*(a))->SetPlayState(a, __VA_ARGS__))
#define SLPlayItf_GetPlayState(a, ...) ((*(a))->GetPlayState(a, __VA_ARGS__))

typedef struct sl
{
   uint8_t **buffer;
   uint8_t *buffer_chunk;

   SLObjectItf engine_object;
   SLEngineItf engine;

   SLObjectItf output_mix;
   SLObjectItf buffer_queue_object;
   SLAndroidSimpleBufferQueueItf buffer_queue;
   SLPlayItf player;

   unsigned buf_size;
   unsigned buf_count;
   unsigned buffer_index;
   unsigned buffer_ptr;
   /* The player was created with a float PCM_EX format (Android, API
    * level 21+); blocks then hold 32-bit float frames and the frontend
    * skips its float-to-int16 pass. */
   bool use_float;
   /* The layout asked for, and the channels the player was created
    * with. They agree unless the wider format was refused. */
   uint32_t layout;
   unsigned channels;
   bool nonblock;
   bool is_paused;
} sl_t;

/* What the callback reaches.  Android dispatches into a player after
 * Destroy has returned (#19561), so none of this can live in the
 * handle sl_free() releases: it is static, the driver being one
 * instance at a time, and it is never freed.  Nothing is allocated
 * until the driver is first initialized, and nothing grows across
 * init/free cycles - on Android the eventcount is a bare futex word
 * with no allocation at all. */
typedef struct sl_shared
{
   /* Zero from the moment sl_free() starts, so a dispatch that
    * arrives after it touches nothing.  Published with release: the
    * fields below are written before it is set. */
   retro_atomic_int_t live;
   /* The queue whose player owns this session.  A dispatch from a
    * player that has been replaced carries the old interface and is
    * dropped, so it cannot spend the new player's credit. */
   SLAndroidSimpleBufferQueueItf bq;
   /* Blocks currently enqueued on the device.  Decremented by
    * opensl_callback on the OpenSL engine thread, incremented and
    * read by the writer thread.  Android is always weakly-ordered
    * ARM/AArch64, so the reads need acquire semantics to pair with
    * the acq_rel RMWs - the previous plain volatile reads had none. */
   retro_atomic_int_t buffered_blocks;
   /* Writers park here when the queue is full or unmoved. */
   retro_eventcount_t park;
   /* Frames the device has finished with, for the sink rate estimate.
    * The callback fires once per block the device has played, so it is
    * the device's own clock ticking - the same thing the WASAPI pump
    * and the ASIO callback count. Written only by the callback, read by
    * the frontend through sl_frames_consumed(). */
   retro_atomic_size_t consumed;
   /* Callbacks that left the queue empty: the device has nothing to
    * play next and goes quiet until the writer enqueues again. */
   retro_atomic_size_t underruns;
   unsigned frames_per_block;
   bool park_ready;
} sl_shared_t;

static sl_shared_t sl_shared;

/* Fully lock-free, no sleep, no clock: the writer parks on a
 * retro_eventcount, which on this driver's one real platform is a
 * futex - no lock at all, a relative timeout as the stall bound, so
 * no deadline arithmetic and no wall clock in the hot path - and
 * whose prepare/commit window removes the lost-wakeup race the old
 * condition variable had to tolerate. The construct's other
 * backends keep the file honest if it ever compiles elsewhere. The
 * driver owns no mutex any thread could ever touch after free
 * (#19561's poisoned mutex is libwilhelm's own; see sl_free), and
 * the callback's notify makes no syscall unless a writer is
 * actually parked - the eventcount gates that itself, with the
 * seq_cst pairing a hand-rolled waiter flag gets subtly wrong. */
static void opensl_callback(SLAndroidSimpleBufferQueueItf bq, void *ctx)
{
   sl_shared_t *sh = (sl_shared_t*)ctx;
   /* A player torn down, or replaced, since this was dispatched. */
   if (!retro_atomic_load_acquire_int(&sh->live) || bq != sh->bq)
      return;
   if (retro_atomic_fetch_sub_int(&sh->buffered_blocks, 1) == 1)
      retro_atomic_fetch_add_size(&sh->underruns, 1);
   /* A block the device has played: device time, whatever the writer
    * managed to supply. */
   retro_atomic_fetch_add_size(&sh->consumed, sh->frames_per_block);
   /* Wake a parked writer; with none parked this is one atomic
    * bump and one load, no syscall. */
   retro_eventcount_notify(&sh->park);
}

/* Frames the device has taken since the player started. Counting the
 * blocks it has finished with counts device time; there is no queue to
 * subtract, unlike ALSA, because a block is only handed back once it
 * has been played. */
static size_t sl_frames_consumed(void *data)
{
   sl_t *sl = (sl_t*)data;
   if (!sl)
      return 0;
   return retro_atomic_load_acquire_size(&sl_shared.consumed);
}

#define GOTO_IF_FAIL(x) do { \
   if ((res = (x)) != SL_RESULT_SUCCESS) \
      goto error; \
} while (0)

static void sl_free(void *data)
{
   sl_t *sl = (sl_t*)data;
   if (!sl)
      return;

   /* Before anything is torn down, so a dispatch already on its way
    * spends nothing and wakes nobody. */
   retro_atomic_store_release_int(&sl_shared.live, 0);

   /* Teardown ordering hardened for #19561: libwilhelm's AudioTrack
    * thread crashing in pthread_mutex_lock on 0x55-poisoned memory,
    * every frame of the stack inside libwilhelm - its own object
    * mutex, freed by our Destroy while its callback thread was still
    * in flight.  The spec makes Destroy synchronize with callbacks,
    * but Android's implementation has raced that when torn down from
    * PLAYING with buffers still queued.  So: stop, clear the queue,
    * and confirm the STOPPED transition (bounded - GetPlayState is
    * cheap and the transition is normally immediate) before any
    * Destroy, so the AudioTrack thread has nothing left to run when
    * the objects go. */
   if (sl->player)
   {
      SLPlayItf_SetPlayState(sl->player, SL_PLAYSTATE_STOPPED);
      if (sl->buffer_queue)
         (*sl->buffer_queue)->Clear(sl->buffer_queue);
      {
         /* A foreign API's state transition has nothing to park on,
          * so this one confirmation loop polls - iteration-bounded,
          * cold path, once per session.  Normally the very first
          * read already says STOPPED. */
         int i;
         SLuint32 state = SL_PLAYSTATE_PLAYING;
         for (i = 0; i < 50; i++)
         {
            if (   SLPlayItf_GetPlayState(sl->player, &state)
                     != SL_RESULT_SUCCESS
                || state == SL_PLAYSTATE_STOPPED)
               break;
            retro_sleep(1);
         }
      }
   }

   if (sl->buffer_queue_object)
      SLObjectItf_Destroy(sl->buffer_queue_object);

   if (sl->output_mix)
      SLObjectItf_Destroy(sl->output_mix);

   if (sl->engine_object)
      SLObjectItf_Destroy(sl->engine_object);

   /* sl_shared.park is not freed here: the callback that outlives the
    * player notifies it. It is initialized once and reused. */
   free(sl->buffer);
   free(sl->buffer_chunk);
   free(sl);
}

static void *sl_init(const char *device, unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   unsigned i;
   unsigned frames_per_block;
   unsigned channels;
   unsigned frame_size                             = 2 * sizeof(int16_t);
   SLDataFormat_PCM fmt_pcm                        = {0};
#if defined(ANDROID) && defined(__ANDROID_API__) && (__ANDROID_API__ >= 21)
   SLAndroidDataFormat_PCM_EX fmt_pcm_ex           = {0};
#endif
   SLDataSource audio_src                          = {0};
   SLDataSink audio_sink                           = {0};
   SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {0};
   SLDataLocator_OutputMix loc_outmix              = {0};
   SLresult res                                    = 0;
   SLInterfaceID                                id = SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
   SLboolean                                req    = SL_BOOLEAN_TRUE;
   sl_t                                        *sl = (sl_t*)calloc(1, sizeof(sl_t));

   (void)device;
   if (!sl)
      goto error;

   /* calloc zero-fill is not a portable initializer for an atomic -
    * initialize it explicitly before anything can touch it. */
   retro_atomic_int_init(&sl_shared.live, 0);
   retro_atomic_int_init(&sl_shared.buffered_blocks, 0);
   retro_atomic_size_init(&sl_shared.consumed, 0);
   sl_shared.bq = NULL;
   /* Once for the process: the previous session's callback may still
    * be parked against it, and a second init would leak the first. */
   if (!sl_shared.park_ready)
   {
      if (!retro_eventcount_init(&sl_shared.park))
         goto error;
      sl_shared.park_ready = true;
   }

   RARCH_LOG("[OpenSL] Requested audio latency: %u ms.\n", latency);

   /* Anything wider than stereo needs the PCM_EX format below, which
    * is API level 21 and up; without it the count is not one this
    * driver can ask for and the stereo path is what runs. */
   sl->layout = audio_driver_requested_layout();
   channels   = audio_layout_channels(sl->layout);
   if (channels < 2 || channels > 8)
      channels = 2;

   GOTO_IF_FAIL(slCreateEngine(&sl->engine_object, 0, NULL, 0, NULL, NULL));
   GOTO_IF_FAIL(SLObjectItf_Realize(sl->engine_object, SL_BOOLEAN_FALSE));
   GOTO_IF_FAIL(SLObjectItf_GetInterface(sl->engine_object, SL_IID_ENGINE, &sl->engine));

   GOTO_IF_FAIL(SLEngineItf_CreateOutputMix(sl->engine, &sl->output_mix, 0, NULL, NULL));
   GOTO_IF_FAIL(SLObjectItf_Realize(sl->output_mix, SL_BOOLEAN_FALSE));

   /* Sizes in frames first; the byte size follows the format chosen.
    *
    * The device's burst is a granularity, not a block size: a queue
    * whose blocks are not a multiple of it leaves the fast mixer,
    * which costs latency rather than saving it. So the block is the
    * burst where the platform reports one, and the latency decides
    * how many - the same shape every other driver has, where the
    * frontend asks in milliseconds and the driver rounds it to what
    * the device can do. Where nothing is reported, the block is
    * derived from the latency as before. */
   {
      unsigned burst   = audio_driver_device_block_frames();
      frames_per_block = burst ? burst : next_pow2(32 * latency) / 4;
   }
   if (frames_per_block < 2)
      frames_per_block = 2;

   sl->buf_count    = (latency * rate + 500) / 1000;
   sl->buf_count    = (sl->buf_count + frames_per_block / 2) / frames_per_block;

   /* Two at least: the block being filled is the block the device is
    * playing, so a queue of one cannot be written to at all. This is
    * the floor of the buffer-queue path - a latency below two bursts
    * is not reachable, and the buffer_size() this driver reports says
    * so rather than pretending otherwise. */
   if (sl->buf_count < 2)
      sl->buf_count = 2;

   loc_bufq.locatorType   = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
   loc_bufq.numBuffers    = sl->buf_count;

   loc_outmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
   loc_outmix.outputMix   = sl->output_mix;

   audio_src.pLocator     = &loc_bufq;
   audio_sink.pLocator    = &loc_outmix;

#if defined(ANDROID) && defined(__ANDROID_API__) && (__ANDROID_API__ >= 21)
   /* The layout the frontend wants, where the player takes it. The
    * frontend's mask is the channel mask OpenSL asks for - both are
    * the WAVEFORMATEXTENSIBLE bits - so it goes across as it is, and
    * the frames arrive in its ascending-bit order either way. Refused,
    * this falls through to the stereo attempts below unchanged. */
   if (channels > 2)
   {
      fmt_pcm_ex.formatType     = SL_ANDROID_DATAFORMAT_PCM_EX;
      fmt_pcm_ex.numChannels    = channels;
      fmt_pcm_ex.sampleRate     = rate * 1000; /* milli-Hz */
      fmt_pcm_ex.bitsPerSample  = 32;
      fmt_pcm_ex.containerSize  = 32;
      fmt_pcm_ex.channelMask    = sl->layout;
      fmt_pcm_ex.endianness     = SL_BYTEORDER_LITTLEENDIAN;
      fmt_pcm_ex.representation = SL_ANDROID_PCM_REPRESENTATION_FLOAT;
      audio_src.pFormat         = &fmt_pcm_ex;

      res = SLEngineItf_CreateAudioPlayer(sl->engine, &sl->buffer_queue_object,
            &audio_src, &audio_sink, 1, &id, &req);
      if (res == SL_RESULT_SUCCESS)
      {
         sl->use_float = true;
         sl->channels  = channels;
         frame_size    = channels * sizeof(float);
         RARCH_LOG("[OpenSL] Float output, %u channels.\n", channels);
      }
      else
      {
         sl->buffer_queue_object = NULL;
         RARCH_LOG("[OpenSL] %u channels refused (0x%x), using stereo.\n",
               channels, (unsigned)res);
      }
   }

   /* Float first. API level 21 added SLAndroidDataFormat_PCM_EX; a
    * runtime that refuses it (older device, or a libOpenSLES built
    * without it) fails player creation, and the 16-bit format below is
    * tried in that case. */
   if (!sl->use_float)
   {
   fmt_pcm_ex.formatType     = SL_ANDROID_DATAFORMAT_PCM_EX;
   fmt_pcm_ex.numChannels    = 2;
   fmt_pcm_ex.sampleRate     = rate * 1000; /* milli-Hz */
   fmt_pcm_ex.bitsPerSample  = 32;
   fmt_pcm_ex.containerSize  = 32;
   fmt_pcm_ex.channelMask    = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
   fmt_pcm_ex.endianness     = SL_BYTEORDER_LITTLEENDIAN;
   fmt_pcm_ex.representation = SL_ANDROID_PCM_REPRESENTATION_FLOAT;
   audio_src.pFormat         = &fmt_pcm_ex;

   res = SLEngineItf_CreateAudioPlayer(sl->engine, &sl->buffer_queue_object,
         &audio_src, &audio_sink, 1, &id, &req);
   if (res == SL_RESULT_SUCCESS)
   {
      sl->use_float = true;
      sl->channels  = 2;
      frame_size    = 2 * sizeof(float);
      RARCH_LOG("[OpenSL] Float output.\n");
   }
   else
   {
      sl->buffer_queue_object = NULL;
      RARCH_LOG("[OpenSL] Float output refused (0x%x), using 16-bit.\n",
            (unsigned)res);
   }
   }
#endif

   if (!sl->use_float)
   {
      fmt_pcm.formatType     = SL_DATAFORMAT_PCM;
      fmt_pcm.numChannels    = 2;
      fmt_pcm.samplesPerSec  = rate * 1000; /* Samplerate is in milli-Hz. */
      fmt_pcm.bitsPerSample  = 16;
      fmt_pcm.containerSize  = 16;
      fmt_pcm.channelMask    = SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;
      fmt_pcm.endianness     = SL_BYTEORDER_LITTLEENDIAN; /* Android only. */
      audio_src.pFormat      = &fmt_pcm;

      GOTO_IF_FAIL(SLEngineItf_CreateAudioPlayer(sl->engine, &sl->buffer_queue_object,
               &audio_src, &audio_sink,
               1, &id, &req));
      sl->channels           = 2;
      frame_size             = 2 * sizeof(int16_t);
   }
   GOTO_IF_FAIL(SLObjectItf_Realize(sl->buffer_queue_object, SL_BOOLEAN_FALSE));

   sl->buf_size               = frames_per_block * frame_size;
   sl_shared.frames_per_block = frames_per_block;

   sl->buffer       = (uint8_t**)calloc(sizeof(uint8_t*), sl->buf_count);
   if (!sl->buffer)
      goto error;

   sl->buffer_chunk = (uint8_t*)calloc(sl->buf_count, sl->buf_size);
   if (!sl->buffer_chunk)
      goto error;

   for (i = 0; i < sl->buf_count; i++)
      sl->buffer[i] = sl->buffer_chunk + i * sl->buf_size;

   RARCH_LOG("[OpenSL] Setting audio latency: Block size = %u, Blocks = %u, Total = %u...\n",
         sl->buf_size, sl->buf_count, sl->buf_size * sl->buf_count);

   GOTO_IF_FAIL(SLObjectItf_GetInterface(sl->buffer_queue_object, SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
            &sl->buffer_queue));


   (*sl->buffer_queue)->RegisterCallback(sl->buffer_queue, opensl_callback,
         &sl_shared);

   /* Enqueue a bit to get stuff rolling. */
   sl_shared.bq           = sl->buffer_queue;
   retro_atomic_store_release_int(&sl_shared.buffered_blocks,
         (int)sl->buf_count);
   /* Last, and before the first enqueue can be played back: a dispatch
    * that sees this sees everything above it. */
   retro_atomic_store_release_int(&sl_shared.live, 1);
   sl->buffer_index       = 0;

   for (i = 0; i < sl->buf_count; i++)
      (*sl->buffer_queue)->Enqueue(sl->buffer_queue, sl->buffer[i], sl->buf_size);

   GOTO_IF_FAIL(SLObjectItf_GetInterface(sl->buffer_queue_object, SL_IID_PLAY, &sl->player));
   GOTO_IF_FAIL(SLPlayItf_SetPlayState(sl->player, SL_PLAYSTATE_PLAYING));

   return sl;

error:
   RARCH_ERR("[OpenSL] Couldn't initialize OpenSL ES driver. Error code: %d.\n", (int)res);
   sl_free(sl);
   return NULL;
}

static bool sl_stop(void *data)
{
   sl_t      *sl = (sl_t*)data;
   sl->is_paused = (SLPlayItf_SetPlayState(sl->player, SL_PLAYSTATE_STOPPED)
         == SL_RESULT_SUCCESS) ? true : false;

   return sl->is_paused ? true : false;
}

static bool sl_alive(void *data)
{
   sl_t *sl = (sl_t*)data;
   if (!sl)
      return false;
   return !sl->is_paused;
}

static void sl_set_nonblock_state(void *data, bool state)
{
   sl_t *sl = (sl_t*)data;
   if (sl)
      sl->nonblock = state;
}

static bool sl_start(void *data, bool is_shutdown)
{
   sl_t      *sl = (sl_t*)data;
   sl->is_paused = (SLPlayItf_SetPlayState(sl->player, SL_PLAYSTATE_PLAYING)
         == SL_RESULT_SUCCESS) ? false : true;
   return sl->is_paused ? false : true;
}

static ssize_t sl_write(void *data, const void *s, size_t len)
{
   size_t _len = 0;
   sl_t           *sl = (sl_t*)data;
   const uint8_t *buf = (const uint8_t*)s;

   while (len)
   {
      size_t avail_write;

      if (sl->nonblock)
      {
         if (retro_atomic_load_acquire_int(&sl_shared.buffered_blocks)
               == (int)sl->buf_count)
            break;
      }
      else
      {
         /* The driver's only throttle: the eventcount's documented
          * loop, predicate re-checked inside the prepare/commit
          * window so a callback landing there cancels the park
          * instead of being lost.  The timeout is the stall bound,
          * needed because a dead device sends no callback, no
          * shutdown or error signal exists anywhere in this driver,
          * and an unbounded wait parked the thread the core runs on
          * for good. */
         bool stalled = false;
         while (retro_atomic_load_acquire_int(&sl_shared.buffered_blocks)
               == (int)sl->buf_count)
         {
            int key = retro_eventcount_prepare_wait(&sl_shared.park);
            if (retro_atomic_load_acquire_int(&sl_shared.buffered_blocks)
                  != (int)sl->buf_count)
            {
               retro_eventcount_cancel_wait(&sl_shared.park);
               break;
            }
            if (!retro_eventcount_commit_wait_timeout(&sl_shared.park, key,
                     OPENSL_STALL_TIMEOUT_US))
            {
               stalled = true;
               break;
            }
         }

         /* Report what was enqueued so far, exactly as the nonblock
          * path above does when the queue is full.  Any partial block
          * stays in sl->buffer_ptr for the next call. */
         if (stalled)
            break;
      }

      avail_write = MIN(sl->buf_size - sl->buffer_ptr, len);

      if (avail_write)
      {
         memcpy(sl->buffer[sl->buffer_index] + sl->buffer_ptr, buf, avail_write);
         sl->buffer_ptr += avail_write;
         buf            += avail_write;
         len            -= avail_write;
         _len           += avail_write;
      }

      if (sl->buffer_ptr >= sl->buf_size)
      {
         SLresult res     = (*sl->buffer_queue)->Enqueue(sl->buffer_queue, sl->buffer[sl->buffer_index], sl->buf_size);

         /* A block the device refused is not on the device: it is not
          * counted, and the index stays on it. */
         if (res != SL_RESULT_SUCCESS)
         {
            RARCH_ERR("[OpenSL] Failed to write. Error: 0x%x.\n", (unsigned)res);
            return -1;
         }
         sl->buffer_index = (sl->buffer_index + 1) % sl->buf_count;
         retro_atomic_fetch_add_int(&sl_shared.buffered_blocks, 1);
         sl->buffer_ptr   = 0;
      }
   }

   return _len;
}

/* Sleep on the condition the buffer-done callback signals until at
 * least len bytes fit, capped at half the queue so the wait always
 * ends. Returns the free space then, or 0 after the stall timeout. */
static size_t sl_wait_writable(void *data, size_t len)
{
   sl_t *sl     = (sl_t*)data;
   size_t total = (size_t)sl->buf_size * sl->buf_count;
   size_t avail;

   if (len > total / 2)
      len = total / 2;

   for (;;)
   {
      int buffered = retro_atomic_load_acquire_int(&sl_shared.buffered_blocks);
      /* Whole blocks not on the device, less the one being filled,
       * plus what is left of that one. With every block enqueued there
       * is no block to fill and the space is nil; said so, rather than
       * left to the unsigned arithmetic that only came to zero because
       * buffer_ptr is always zero in that state. */
      if (buffered >= (int)sl->buf_count)
         avail = 0;
      else
         avail = ((size_t)(sl->buf_count - buffered - 1) * sl->buf_size
               + (sl->buf_size - sl->buffer_ptr));
      if (avail >= len)
         return avail;
      /* Same parking as sl_write's throttle: block until the counter
       * moves off the value just sampled, stalled after the same
       * timeout, with the change-since-sample race closed by the
       * prepare/commit window. */
      while (retro_atomic_load_acquire_int(&sl_shared.buffered_blocks)
            == buffered)
      {
         int key = retro_eventcount_prepare_wait(&sl_shared.park);
         if (retro_atomic_load_acquire_int(&sl_shared.buffered_blocks)
               != buffered)
         {
            retro_eventcount_cancel_wait(&sl_shared.park);
            break;
         }
         if (!retro_eventcount_commit_wait_timeout(&sl_shared.park, key,
                  OPENSL_STALL_TIMEOUT_US))
            return 0;
      }
   }
}

static size_t sl_write_avail(void *data)
{
   sl_t *sl     = (sl_t*)data;
   int buffered = retro_atomic_load_acquire_int(&sl_shared.buffered_blocks);
   return ((sl->buf_count - buffered - 1) * sl->buf_size + (sl->buf_size - (int)sl->buffer_ptr));
}

static size_t sl_buffer_size(void *data)
{
   sl_t *sl = (sl_t*)data;
   return sl->buf_size * sl->buf_count;
}

/* True when init managed to create the player with a float format
 * (Android API level 21+); false on the 16-bit path every other build
 * and runtime takes. */
static bool sl_use_float(void *data)
{
   sl_t *sl = (sl_t*)data;
   return sl->use_float;
}

static size_t sl_underruns(void *data)
{
   sl_t *sl = (sl_t*)data;
   if (!sl)
      return 0;
   return retro_atomic_load_acquire_size(&sl_shared.underruns);
}

static uint32_t sl_layout(void *data)
{
   sl_t *sl = (sl_t*)data;
   if (!sl || sl->channels != audio_layout_channels(sl->layout))
      return AUDIO_LAYOUT_STEREO;
   return sl->layout;
}

audio_driver_t audio_opensl = {
   sl_init,
   sl_write,
   sl_stop,
   sl_start,
   sl_alive,
   sl_set_nonblock_state,
   sl_free,
   sl_use_float,
   "opensl",
   NULL,
   NULL,
   sl_write_avail,
   sl_buffer_size,
   NULL, /* write_raw */
   sl_wait_writable,
   sl_frames_consumed,
   sl_underruns,
   sl_layout
};
