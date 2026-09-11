/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2023-2025 - Jesse Talavera-Greenberg
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

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <rthreads/rthreads.h>
#include <retro_spsc.h>
#include <retro_inline.h>
#include <retro_math.h>
#include <lists/string_list.h>
#include <string/stdstring.h>

#include "SDL.h"

/* Upper bound on how long a blocking write or read will wait for the
 * SDL callback before giving up and reporting a short count.  Never
 * reached in normal operation - the callback signals every period -
 * so the exact value only decides how long a stalled device takes to
 * be noticed.  coreaudio.c already uses a flat 300ms for the same
 * purpose on iOS. */
#define SDL_AUDIO_STALL_TIMEOUT_US 256000
#include "SDL_audio.h"

#include "../audio_driver.h"
#include "../../configuration.h"
#include "../../msg_hash.h"
#include "../../runloop.h"
#include "../../verbosity.h"

/* Frames per device buffer for the given slice of the latency setting:
 * a power of two, as SDL prefers, rounded down so the device stage
 * never exceeds its share, with a floor a device will accept. Rounding
 * up put a 16 ms share at 21.3 ms on a 48 kHz device. */
static INLINE int sdl_audio_find_num_frames(int rate, int latency)
{
   int frames = (rate * latency) / 1000;
   int pow2   = (int)prev_pow2((uint32_t)frames);
   if (pow2 < 64)
      pow2    = 64;
   return pow2;
}

/* Room in a ring against its logical size: the physical room less the
 * capacity that lies beyond the size asked for (retro_spsc rounds up
 * to a power of two). */
static size_t sdl_ring_room(const retro_spsc_t *ring, size_t ring_size)
{
   size_t room   = retro_spsc_write_avail(ring);
   size_t excess = ring->capacity - ring_size;
   return room > excess ? room - excess : 0;
}

#ifdef HAVE_SDL2
#ifdef HAVE_MICROPHONE
#include "../microphone_driver.h"

typedef struct sdl_microphone_handle
{
#ifdef HAVE_THREADS
   slock_t *lock;
   scond_t *cond;
#endif

   /**
    * The queue used to store incoming samples from the driver.
    * Single producer (SDL's capture thread in sdl_audio_record_cb),
    * single consumer (the core thread in sdl_microphone_read), so it
    * is a lock-free retro_spsc ring and neither side needs
    * SDL_LockAudioDevice() to touch it.  retro_spsc rounds the
    * capacity up to a power of two; ring_size is the size asked for
    * and the producer never fills past it, so the queue behaves like
    * the old fifo of the same size.
    */
   retro_spsc_t      ring;
   size_t            ring_size;
   bool              ring_init;
   SDL_AudioDeviceID device_id;
   SDL_AudioSpec device_spec;
} sdl_microphone_handle_t;

typedef struct sdl_microphone
{
   bool nonblock;
} sdl_microphone_t;

static void *sdl_microphone_init(void)
{
   sdl_microphone_t *sdl        = NULL;
   uint32_t sdl_subsystem_flags = SDL_WasInit(0);
   /* Initialise audio subsystem, if required */
   if (sdl_subsystem_flags == 0)
   {
      if (SDL_Init(SDL_INIT_AUDIO) < 0)
         return NULL;
   }
   else if ((sdl_subsystem_flags & SDL_INIT_AUDIO) == 0)
   {
      if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
         return NULL;
   }
   if (!(sdl = (sdl_microphone_t*)calloc(1, sizeof(*sdl))))
      return NULL;
   return sdl;
}

static void sdl_microphone_close_mic(void *driver_context, void *mic_context)
{
   sdl_microphone_handle_t *mic = (sdl_microphone_handle_t *)mic_context;

   if (mic)
   {
      /* If the microphone was originally initialized successfully... */
      if (mic->device_id > 0)
         SDL_CloseAudioDevice(mic->device_id);

      if (mic->ring_init)
         retro_spsc_free(&mic->ring);

#ifdef HAVE_THREADS
      slock_free(mic->lock);
      scond_free(mic->cond);
#endif

      RARCH_LOG("[SDL audio] Freed microphone with former device ID %u.\n", mic->device_id);
      free(mic);
   }
}

static void sdl_microphone_free(void *data)
{
   sdl_microphone_t *sdl = (sdl_microphone_t*)data;

   if (sdl)
      SDL_QuitSubSystem(SDL_INIT_AUDIO);
   free(sdl);
   /* NOTE: The microphone frontend should've closed the mics by now */
}

static void sdl_audio_record_cb(void *data, Uint8 *stream, int len)
{
   sdl_microphone_handle_t *mic = (sdl_microphone_handle_t*)data;
   size_t                 avail = sdl_ring_room(&mic->ring, mic->ring_size);
   size_t             read_size = MIN(len, (int)avail);
   /* If the sample buffer is almost full, just write as much as we can into it*/
   retro_spsc_write(&mic->ring, stream, read_size);
#ifdef HAVE_THREADS
   scond_signal(mic->cond);
#endif
}

static void *sdl_microphone_open_mic(void *driver_context, const char *device,
      unsigned rate, unsigned latency, unsigned *new_rate)
{
   int frames;
   size_t bufsize;
   void *tmp                    = NULL;
   sdl_microphone_handle_t *mic = NULL;
   SDL_AudioSpec desired_spec   = {0};

#if __APPLE__
   if (!string_is_equal(audio_driver_get_ident(), "sdl2"))
   {
      const char *msg = msg_hash_to_str(MSG_SDL2_MIC_NEEDS_SDL2_AUDIO);
      runloop_msg_queue_push(msg, strlen(msg),
            1, 100, true, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_WARNING);
      return NULL;
   }
#endif

   /* If the audio driver wasn't initialized yet... */
   if (!SDL_WasInit(SDL_INIT_AUDIO))
   {
      RARCH_ERR("[SDL mic] Attempted to initialize input device before initializing the audio subsystem.\n");
      return NULL;
   }

   if (!(mic = (sdl_microphone_handle_t *)
            calloc(1, sizeof(sdl_microphone_handle_t))))
      return NULL;

   /* Only print SDL audio devices if verbose logging is enabled */
   if (verbosity_is_enabled())
   {
      int i;
      int num_available_microphones = SDL_GetNumAudioDevices(true);
      RARCH_DBG("[SDL mic] %d audio capture devices found:\n", num_available_microphones);
      for (i = 0; i < num_available_microphones; ++i)
         RARCH_DBG("[SDL mic]    - %s\n", SDL_GetAudioDeviceName(i, true));
   }

   /* We have to buffer up some data ourselves, so we let SDL
    * carry approximately half of the latency.
    *
    * SDL double buffers audio and we do as well. */
   frames                = sdl_audio_find_num_frames(rate, latency / 4);

   desired_spec.freq     = rate;
#ifdef HAVE_SDL2
   /* Same negotiation hint the output device honours above. The libretro
    * microphone interface is int16 only, so 'Int16' here keeps the whole
    * capture path integer instead of converting a float stream back down.
    * SDL converts transparently if the device's native format differs.
    * SDL1.2 capture is int16 only. */
   desired_spec.format   = (config_get_ptr()->uints.audio_format_negotiation
         == AUDIO_FORMAT_NEGOTIATION_INT16)
         ? AUDIO_S16SYS : AUDIO_F32SYS;
#else
   desired_spec.format   = AUDIO_S16SYS;
#endif
   desired_spec.channels = 1; /* Microphones only usually provide input in mono */
   desired_spec.samples  = frames;
   desired_spec.userdata = mic;
   desired_spec.callback = sdl_audio_record_cb;

   mic->device_id = SDL_OpenAudioDevice(
         NULL,
         true,
         &desired_spec,
         &mic->device_spec,
           SDL_AUDIO_ALLOW_FREQUENCY_CHANGE
         | SDL_AUDIO_ALLOW_FORMAT_CHANGE);

   if (mic->device_id == 0)
   {
      RARCH_ERR("[SDL mic] Failed to open SDL audio input device: %s.\n", SDL_GetError());
      goto error;
   }
   RARCH_DBG("[SDL mic] Opened SDL audio input device with ID %u.\n",
             mic->device_id);
   RARCH_DBG("[SDL mic] Requested a microphone frequency of %u Hz, received %u Hz.\n",
             desired_spec.freq, mic->device_spec.freq);
   RARCH_DBG("[SDL mic] Requested %u channels for microphone, received %u.\n",
             desired_spec.channels, mic->device_spec.channels);
   RARCH_DBG("[SDL mic] Requested a %u-sample microphone buffer, received %u samples (%u bytes).\n",
             frames, mic->device_spec.samples, mic->device_spec.size);
   RARCH_DBG("[SDL mic] Received a microphone silence value of %u.\n", mic->device_spec.silence);
   RARCH_DBG("[SDL mic] Requested microphone audio format: %u-bit %s %s %s endian.\n",
             SDL_AUDIO_BITSIZE(desired_spec.format),
             SDL_AUDIO_ISSIGNED(desired_spec.format) ? "signed" : "unsigned",
             SDL_AUDIO_ISFLOAT(desired_spec.format) ? "floating-point" : "integer",
             SDL_AUDIO_ISBIGENDIAN(desired_spec.format) ? "big" : "little");

   RARCH_DBG("[SDL mic] Received microphone audio format: %u-bit %s %s %s endian.\n",
             SDL_AUDIO_BITSIZE(mic->device_spec.format),
             SDL_AUDIO_ISSIGNED(mic->device_spec.format) ? "signed" : "unsigned",
             SDL_AUDIO_ISFLOAT(mic->device_spec.format) ? "floating-point" : "integer",
             SDL_AUDIO_ISBIGENDIAN(mic->device_spec.format) ? "big" : "little");

   if (new_rate)
      *new_rate = mic->device_spec.freq;

#ifdef HAVE_THREADS
   mic->lock = slock_new();
   mic->cond = scond_new();
#endif

   RARCH_LOG("[SDL audio] Requested %u ms latency for input device, received %d ms.\n",
             latency, (int)(mic->device_spec.samples * 4 * 1000 / mic->device_spec.freq));

   /* Create a buffer twice as big as needed and prefill the buffer. */
   bufsize            = mic->device_spec.samples * 2 * (SDL_AUDIO_BITSIZE(mic->device_spec.format) / 8);
   tmp                = calloc(1, bufsize);
   mic->ring_size     = bufsize;
   mic->ring_init     = retro_spsc_init(&mic->ring, bufsize);

   RARCH_DBG("[SDL audio] Initialized microphone sample queue with %u bytes.\n", bufsize);

   /* Bail to the existing 'error' label on OOM - it frees 'mic' and
    * returns NULL, same as the outer calloc-failure path at the top
    * of this function.  'tmp' is freed by the 'if (tmp)' block below,
    * not here (calloc may have succeeded even if the ring init
    * failed, so defer tmp cleanup to after the guard). */
   if (!mic->ring_init)
   {
      free(tmp);
      goto error;
   }

   if (tmp)
   {
      retro_spsc_write(&mic->ring, tmp, bufsize);
      free(tmp);
   }

   RARCH_LOG("[SDL audio] Initialized microphone with device ID %u.\n", mic->device_id);
   return mic;

error:
   /* HAVE_THREADS may have init'd mic->lock and mic->cond between
    * the SDL_OpenAudioDevice call and the retro_spsc_init below.
    * slock_free/scond_free are NULL-tolerant so the earlier goto
    * error site (device_id == 0) where these are still NULL is
    * fine too.  'mic' is always non-NULL at this label because
    * the calloc at the top of the function returns early on
    * failure without goto'ing here. */
   if (mic->device_id > 0)
      SDL_CloseAudioDevice(mic->device_id);
#ifdef HAVE_THREADS
   slock_free(mic->lock);
   scond_free(mic->cond);
#endif
   free(mic);
   return NULL;
}

static bool sdl_microphone_mic_alive(const void *data, const void *mic_context)
{
   const sdl_microphone_handle_t *mic = (const sdl_microphone_handle_t*)mic_context;
   if (!mic)
      return false;
   /* Both params must be non-null */
   return SDL_GetAudioDeviceStatus(mic->device_id) == SDL_AUDIO_PLAYING;
}

static bool sdl_microphone_start_mic(void *driver_context, void *mic_context)
{
   sdl_microphone_handle_t *mic = (sdl_microphone_handle_t*)mic_context;
   if (!mic)
      return false;
   SDL_PauseAudioDevice(mic->device_id, false);
   if (SDL_GetAudioDeviceStatus(mic->device_id) != SDL_AUDIO_PLAYING)
   {
      RARCH_ERR("[SDL mic] Failed to start microphone %u: %s.\n", mic->device_id, SDL_GetError());
      return false;
   }
   RARCH_DBG("[SDL mic] Started microphone %u.\n", mic->device_id);
   return true;
}

static bool sdl_microphone_stop_mic(void *driver_context, void *mic_context)
{
   sdl_microphone_t        *sdl = (sdl_microphone_t*)driver_context;
   sdl_microphone_handle_t *mic = (sdl_microphone_handle_t*)mic_context;

   if (!sdl || !mic)
      return false;

   SDL_PauseAudioDevice(mic->device_id, true);

   switch (SDL_GetAudioDeviceStatus(mic->device_id))
   {
      case SDL_AUDIO_PLAYING:
         RARCH_ERR("[SDL mic] Microphone %u failed to pause.\n", mic->device_id);
         return false;
      case SDL_AUDIO_STOPPED:
         RARCH_WARN("[SDL mic] Microphone %u is in state STOPPED; it may not start again.\n",
               mic->device_id);
         /* fall-through */
      case SDL_AUDIO_PAUSED:
         break;
      default:
         RARCH_ERR("[SDL mic] Microphone %u is in unknown state.\n",
               mic->device_id);
         return false;
   }

   return true;
}

static void sdl_microphone_set_nonblock_state(void *driver_context, bool state)
{
   sdl_microphone_t *sdl = (sdl_microphone_t*)driver_context;
   if (sdl)
      sdl->nonblock = state;
}

/* Sleeps until the capture queue holds len bytes, then says how many it
 * holds. The same bounded wait sdl_microphone_read() does - the SDL
 * capture callback is the only thing that ever signals this condition,
 * so an untimed wait never returns once the device stops calling back -
 * without the copy out. */
static size_t sdl_microphone_wait_readable(void *driver_context,
      void *mic_context, size_t len)
{
   sdl_microphone_handle_t *mic = (sdl_microphone_handle_t*)mic_context;
   size_t avail;

   if (!mic || !mic->ring_init)
      return 0;

   avail = retro_spsc_read_avail(&mic->ring);

   if (avail >= len)
      return avail;

#ifdef HAVE_THREADS
   slock_lock(mic->lock);
   scond_wait_timeout(mic->cond, mic->lock, SDL_AUDIO_STALL_TIMEOUT_US);
   slock_unlock(mic->lock);
#endif

   return retro_spsc_read_avail(&mic->ring);
}

static int sdl_microphone_read(void *driver_context, void *mic_context, void *sv, size_t len)
{
   int ret    = 0;
   uint8_t *s = (uint8_t*)sv;
   sdl_microphone_t        *sdl = (sdl_microphone_t*)driver_context;
   sdl_microphone_handle_t *mic = (sdl_microphone_handle_t*)mic_context;

   if (!sdl || !mic || !s)
      return -1;

   /* If we shouldn't block on an empty queue... */
   if (sdl->nonblock)
   {
      /* Read as much data as will fit in buf; the ring is SPSC so the
       * SDL capture thread can keep pushing while we pull. */
      ret = (int)retro_spsc_read(&mic->ring, s, len);
   }
   else
   {
      size_t read = 0;

      /* Until we've given the caller as much data as they've asked for... */
      while (read < len)
      {
         size_t avail;
#ifdef HAVE_THREADS
         bool signalled;
#endif

         avail = retro_spsc_read_avail(&mic->ring);

         if (avail == 0)
         { /* If the incoming sample queue is empty... */
            /* Wait for the SDL microphone thread to
             * push some incoming samples */
#ifdef HAVE_THREADS
            slock_lock(mic->lock);
            /* Let *only* the SDL microphone thread access
             * the incoming sample queue.  Bounded for the same reason
             * as the playback path above: sdl_microphone_read_cb is
             * the only thing that ever signals this condition, so once
             * the capture device stops calling back an untimed wait
             * here never returns. */
            signalled = scond_wait_timeout(mic->cond, mic->lock,
                  SDL_AUDIO_STALL_TIMEOUT_US);
            slock_unlock(mic->lock);
            /* Allow this thread to access the incoming sample queue,
             * which we'll do next iteration */
            if (!signalled)
               break;   /* Report what we managed to capture */
#else
            break;
#endif
         }
         else
         {
            size_t read_amt = MIN(len - read, avail);
            retro_spsc_read(&mic->ring, s + read, read_amt);
            /* Read as many samples as we have available without
             * underflowing the queue */
            read += read_amt;
         }
      }
      ret = (int)read;
   }

   return ret;
}

static bool sdl_microphone_mic_use_float(const void *driver_context, const void *mic_context)
{
   sdl_microphone_handle_t *mic = (sdl_microphone_handle_t*)mic_context;
   return SDL_AUDIO_ISFLOAT(mic->device_spec.format);
}

microphone_driver_t microphone_sdl = {
      sdl_microphone_init,
      sdl_microphone_free,
      sdl_microphone_read,
      sdl_microphone_set_nonblock_state,
      "sdl2",
      NULL,
      NULL,
      sdl_microphone_open_mic,
      sdl_microphone_close_mic,
      sdl_microphone_mic_alive,
      sdl_microphone_start_mic,
      sdl_microphone_stop_mic,
      sdl_microphone_mic_use_float,
      sdl_microphone_wait_readable
};
#endif
#else
typedef Uint32 SDL_AudioDeviceID;

/** Compatibility stub that defers to SDL_PauseAudio. */
#define SDL_PauseAudioDevice(dev, pause_on) SDL_PauseAudio(pause_on)

/** Compatibility stub that defers to SDL_LockAudio. */
#define SDL_LockAudioDevice(dev) SDL_LockAudio()

/** Compatibility stub that defers to SDL_UnlockAudio. */
#define SDL_UnlockAudioDevice(dev) SDL_UnlockAudio()

/** Compatibility stub that defers to SDL_CloseAudio. */
#define SDL_CloseAudioDevice(dev) SDL_CloseAudio()

/* Macros for checking audio format bits that were introduced in SDL 2 */
#define SDL_AUDIO_MASK_BITSIZE       (0xFF)
#define SDL_AUDIO_MASK_DATATYPE      (1<<8)
#define SDL_AUDIO_MASK_ENDIAN        (1<<12)
#define SDL_AUDIO_MASK_SIGNED        (1<<15)
#define SDL_AUDIO_BITSIZE(x)         (x & SDL_AUDIO_MASK_BITSIZE)
#define SDL_AUDIO_ISFLOAT(x)         (x & SDL_AUDIO_MASK_DATATYPE)
#define SDL_AUDIO_ISBIGENDIAN(x)     (x & SDL_AUDIO_MASK_ENDIAN)
#define SDL_AUDIO_ISSIGNED(x)        (x & SDL_AUDIO_MASK_SIGNED)
#define SDL_AUDIO_ISINT(x)           (!SDL_AUDIO_ISFLOAT(x))
#define SDL_AUDIO_ISLITTLEENDIAN(x)  (!SDL_AUDIO_ISBIGENDIAN(x))
#define SDL_AUDIO_ISUNSIGNED(x)      (!SDL_AUDIO_ISSIGNED(x))
#endif

typedef struct sdl_audio
{
#ifdef HAVE_THREADS
   slock_t *lock;
   scond_t *cond;
#endif
   /**
    * The queue used to store outgoing samples to be played by the driver.
    * Audio from the core ultimately makes its way here,
    * the last stop before the driver plays it.
    */
   /* Outgoing samples.  Single producer (the core thread in
    * sdl_audio_write), single consumer (SDL's playback thread in
    * sdl_audio_playback_cb): a lock-free retro_spsc ring, so the
    * writer no longer has to SDL_LockAudioDevice() - i.e. stall the
    * playback callback - to push samples.  speaker_ring_size is the
    * size asked for; see sdl_ring_room. */
   retro_spsc_t speaker_ring;
   size_t       speaker_ring_size;
   bool         speaker_ring_init;
   bool nonblock;
   bool is_paused;
   SDL_AudioSpec device_spec;
   uint32_t      layout;   /* the layout asked for, reported when the count matched */
   SDL_AudioDeviceID speaker_device;
} sdl_audio_t;

static void sdl_audio_playback_cb(void *data, Uint8 *stream, int len)
{
   sdl_audio_t  *sdl = (sdl_audio_t*)data;
   size_t       _len = retro_spsc_read(&sdl->speaker_ring, stream, (size_t)len);
#ifdef HAVE_THREADS
   scond_signal(sdl->cond);
#endif
   /* If underrun, fill rest with silence. */
   memset(stream + _len, 0, len - _len);
}

static void *sdl_audio_list_new(void *u)
{
#ifdef HAVE_SDL2
   int i, num = 0;
   union string_list_elem_attr attr;
   struct string_list *sl = string_list_new();

   if (!sl)
      return NULL;

   attr.i = 0;
   num    = SDL_GetNumAudioDevices(false);

   for (i = 0; i < num; i++)
      string_list_append(sl, SDL_GetAudioDeviceName(i, false), attr);

   return sl;
#else
   /* TODO/FIXME - Any possible SDL1 implementation here, or
    * do we have to piggyback off OS-specific audio device
    * enumeration here? */
   return NULL;
#endif
}

static void sdl_audio_free(void *data);

static void *sdl_audio_init(const char *device,
      unsigned rate, unsigned latency,
       unsigned *new_rate)
{
   int frames;
   size_t bufsize;
   SDL_AudioSpec spec           = {0};
   void *tmp                    = NULL;
   sdl_audio_t *sdl             = NULL;
   uint32_t sdl_subsystem_flags = SDL_WasInit(0);

   /* Initialise audio subsystem, if required */
   if (sdl_subsystem_flags == 0)
   {
      if (SDL_Init(SDL_INIT_AUDIO) < 0)
         return NULL;
   }
   else if ((sdl_subsystem_flags & SDL_INIT_AUDIO) == 0)
   {
      if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0)
         return NULL;
   }

   sdl = (sdl_audio_t*)calloc(1, sizeof(*sdl));
   if (!sdl)
      return NULL;

   /* We have to buffer up some data ourselves, so we let SDL
    * carry approximately half of the latency.
    *
    * SDL double buffers audio and we do as well. */
   frames        = sdl_audio_find_num_frames(rate, latency / 4);

   /* First, let's initialize the output device. */
   spec.freq     = rate;
#ifdef HAVE_SDL2
   /* SDL2 can open either format; honour the negotiation hint (SDL converts
    * transparently if the device's native format differs). SDL1.2 output is
    * int16 only. */
   spec.format   = (config_get_ptr()->uints.audio_format_negotiation
         == AUDIO_FORMAT_NEGOTIATION_INT16)
         ? AUDIO_S16SYS : AUDIO_F32SYS;
#else
   spec.format   = AUDIO_S16SYS;
#endif
   /* The channels of the layout the frontend wants; SDL converts if
    * the device lacks them, so what it grants is what the frontend
    * gets, in SDL's fixed interleaved order for the count - which is
    * the layout's ascending-bit order. SDL does not say whether a
    * six-channel device's rear pair is at the back or the sides; it
    * takes the frames in that order and maps them itself, so the
    * layout reported is the one asked for. */
   sdl->layout   = audio_driver_requested_layout();
   spec.channels = (Uint8)audio_layout_channels(sdl->layout);
   spec.samples  = frames; /* This is in audio frames, not samples ... :( */
   spec.callback = sdl_audio_playback_cb;
   spec.userdata = sdl;

   /* No compatibility stub for SDL_OpenAudioDevice because its return value
    * is different from that of SDL_OpenAudio. */
#ifdef HAVE_SDL2
   sdl->speaker_device = SDL_OpenAudioDevice(NULL, false, &spec, &sdl->device_spec, 0);

   if (sdl->speaker_device == 0)
#else
   sdl->speaker_device = SDL_OpenAudio(&spec, &sdl->device_spec);

   if (sdl->speaker_device < 0)
#endif
   {
      RARCH_ERR("[SDL audio] Failed to open SDL audio output device: %s.\n", SDL_GetError());
      free(sdl);
      return NULL;
   }

   *new_rate                = sdl->device_spec.freq;
   RARCH_DBG("[SDL audio] Opened SDL audio out device with ID %u.\n",
             sdl->speaker_device);
   RARCH_DBG("[SDL audio] Requested a speaker frequency of %u Hz, received %u Hz.\n",
             spec.freq, sdl->device_spec.freq);
   RARCH_DBG("[SDL audio] Requested %u channels for speaker, received %u.\n",
             spec.channels, sdl->device_spec.channels);
   RARCH_DBG("[SDL audio] Requested a %u-frame speaker buffer, received %u frames (%u bytes).\n",
             frames, sdl->device_spec.samples, sdl->device_spec.size);
   RARCH_DBG("[SDL audio] Got a speaker silence value of %u.\n", sdl->device_spec.silence);
   RARCH_DBG("[SDL audio] Requested speaker audio format: %u-bit %s %s %s endian.\n",
             SDL_AUDIO_BITSIZE(spec.format),
             SDL_AUDIO_ISSIGNED(spec.format) ? "signed" : "unsigned",
             SDL_AUDIO_ISFLOAT(spec.format) ? "floating-point" : "integer",
             SDL_AUDIO_ISBIGENDIAN(spec.format) ? "big" : "little");
   RARCH_DBG("[SDL audio] Received speaker audio format: %u-bit %s %s %s endian.\n",
             SDL_AUDIO_BITSIZE(sdl->device_spec.format),
             SDL_AUDIO_ISSIGNED(sdl->device_spec.format) ? "signed" : "unsigned",
             SDL_AUDIO_ISFLOAT(sdl->device_spec.format) ? "floating-point" : "integer",
             SDL_AUDIO_ISBIGENDIAN(sdl->device_spec.format) ? "big" : "little");

#ifdef HAVE_THREADS
   sdl->lock                = slock_new();
   sdl->cond                = scond_new();
#endif

   /* The fifo in front of the device holds the latency setting, as the
    * other drivers' buffers do; the device buffer behind it, a quarter
    * of the setting rounded down to a power of two, is SDL's and adds
    * on top, unmeasured. The fifo is never smaller than two device
    * buffers, so the callback always has a buffer's worth in hand.
    * It used to be two device buffers exactly, with the device buffer
    * rounded up - 42.7 ms reported against a 64 ms setting - and the
    * line here claimed four. */
   {
      size_t frame_bytes = (size_t)sdl->device_spec.channels
            * (SDL_AUDIO_BITSIZE(sdl->device_spec.format) / 8);
      size_t fifo_frames = ((size_t)(*new_rate) * latency) / 1000;
      if (fifo_frames < (size_t)sdl->device_spec.samples * 2)
         fifo_frames     = (size_t)sdl->device_spec.samples * 2;
      bufsize            = fifo_frames * frame_bytes;
   }

   RARCH_LOG("[SDL audio] Requested %u ms latency: %u ms fifo, plus a %u-frame (%u ms) device buffer.\n",
         latency,
         (unsigned)(bufsize / (2 * (SDL_AUDIO_BITSIZE(sdl->device_spec.format) / 8)) * 1000 / (*new_rate)),
         (unsigned)sdl->device_spec.samples,
         (unsigned)(sdl->device_spec.samples * 1000 / (*new_rate)));

   tmp                    = calloc(1, bufsize);
   sdl->speaker_ring_size = bufsize;
   sdl->speaker_ring_init = retro_spsc_init(&sdl->speaker_ring, bufsize);

   /* Bail on OOM (see the mic path above). */
   if (!sdl->speaker_ring_init)
   {
      free(tmp);
      sdl_audio_free(sdl);
      return NULL;
   }

   if (tmp)
   {
      retro_spsc_write(&sdl->speaker_ring, tmp, bufsize);
      free(tmp);
   }

   RARCH_DBG("[SDL audio] Initialized speaker sample queue with %u bytes.\n", bufsize);

   SDL_PauseAudioDevice(sdl->speaker_device, false);

   return sdl;
}

static ssize_t sdl_audio_write(void *data, const void *s, size_t len)
{
   size_t _len      = 0;
   sdl_audio_t *sdl = (sdl_audio_t*)data;

   /* If we shouldn't wait for space in a full outgoing sample queue... */
   if (sdl->nonblock)
   {
      size_t avail     = sdl_ring_room(&sdl->speaker_ring, sdl->speaker_ring_size);
      size_t write_amt = (avail > len) ? len : avail; /* Enqueue as much data as we can */
      retro_spsc_write(&sdl->speaker_ring, s, write_amt);
      _len             = write_amt; /* If the queue was full...well, too bad. */
   }
   else
   {
      /* Until we've written all the sample data we have available... */
      while (_len < len)
      {
         size_t avail;
#ifdef HAVE_THREADS
         bool signalled;
#endif

         avail = sdl_ring_room(&sdl->speaker_ring, sdl->speaker_ring_size);

         /* If the outgoing sample queue is full... */
         if (avail == 0)
         {
            /* Wait for the SDL speaker thread to play the enqueued samples,
             * which will free up space for us to write new ones. */
#ifdef HAVE_THREADS
            slock_lock(sdl->lock);
            /* Bounded: sdl_audio_playback_cb signals without holding
             * sdl->lock, so a signal raised between the avail test and
             * this wait reaches no waiter.  Normally the next callback covers
             * that.  If the device has stopped calling back at all there is no
             * next one, and an untimed wait here parked the core's thread for
             * good. */
            signalled = scond_wait_timeout(sdl->cond, sdl->lock,
                  SDL_AUDIO_STALL_TIMEOUT_US);
            slock_unlock(sdl->lock);
            /* Now let this thread use the outgoing sample queue (which we'll do next iteration) */
            if (!signalled)
               break;   /* Report what we managed to enqueue */
#else
            /* Without threads there is nothing to wait on, and spinning
             * here never made progress either. */
            break;
#endif
         }
         else
         {
            size_t write_amt = len - _len > avail ? avail : len - _len;
            retro_spsc_write(&sdl->speaker_ring, (const char*)s + _len, write_amt);
            /* Enqueue as many samples as we have available without overflowing the queue */
            _len += write_amt;
         }
      }
   }

   return _len;
}

static bool sdl_audio_stop(void *data)
{
   sdl_audio_t *sdl = (sdl_audio_t*)data;
   sdl->is_paused   = true;
   SDL_PauseAudioDevice(sdl->speaker_device, true);
   return true;
}

static bool sdl_audio_alive(void *data)
{
   sdl_audio_t *sdl = (sdl_audio_t*)data;
   if (!sdl)
      return false;
   return !sdl->is_paused;
}

static bool sdl_audio_start(void *data, bool is_shutdown)
{
   sdl_audio_t *sdl = (sdl_audio_t*)data;
   sdl->is_paused   = false;
   SDL_PauseAudioDevice(sdl->speaker_device, false);
   return true;
}

static void sdl_audio_set_nonblock_state(void *data, bool state)
{
   sdl_audio_t *sdl = (sdl_audio_t*)data;
   if (sdl)
      sdl->nonblock = state;
}

static void sdl_audio_free(void *data)
{
   sdl_audio_t *sdl = (sdl_audio_t*)data;

   if (sdl)
   {
      if (sdl->speaker_device > 0)
      {
         SDL_CloseAudioDevice(sdl->speaker_device);
      }

      if (sdl->speaker_ring_init)
         retro_spsc_free(&sdl->speaker_ring);

#ifdef HAVE_THREADS
      slock_free(sdl->lock);
      scond_free(sdl->cond);
#endif

      SDL_QuitSubSystem(SDL_INIT_AUDIO);
   }
   free(sdl);
}

static uint32_t sdl_audio_layout(void *data)
{
   sdl_audio_t *sdl = (sdl_audio_t*)data;
   if (!sdl || sdl->device_spec.channels != audio_layout_channels(sdl->layout))
      return AUDIO_LAYOUT_STEREO;
   return sdl->layout;
}

static bool sdl_audio_use_float(void *data)
{
   sdl_audio_t *sdl = (sdl_audio_t*)data;
   return SDL_AUDIO_ISFLOAT(sdl->device_spec.format) ? true : false;
}

/* TODO/FIXME - implement */
static size_t sdl_audio_write_avail(void *data)
{
   sdl_audio_t *sdl = (sdl_audio_t*)data;
   return sdl_ring_room(&sdl->speaker_ring, sdl->speaker_ring_size);
}

static size_t sdl_audio_buffer_size(void *data)
{
   sdl_audio_t *sdl = (sdl_audio_t*)data;
   /* The size asked for, which is what write_avail() can reach; the
    * ring's physical capacity may be larger (power of two). */
   return sdl->speaker_ring_size;
}

/* Sleep on the condition the speaker thread signals after every pull
 * until at least len bytes fit in the outgoing queue, capped at half
 * of it so the wait always ends. Returns the free space then, or 0
 * when the thread has gone quiet for the stall timeout (device lost)
 * or there is no thread to wait on. */
static size_t sdl_audio_wait_writable(void *data, size_t len)
{
   sdl_audio_t *sdl = (sdl_audio_t*)data;
   size_t avail;
   /* Each wait ends on a timeout; this ends the loop when the device
    * keeps calling back but never frees enough. */
   int laps = 8;

   if (len > sdl->speaker_ring_size / 2)
      len = sdl->speaker_ring_size / 2;

   for (;;)
   {
#ifdef HAVE_THREADS
      bool signalled;
#endif
      if (laps-- < 0)
         return 0;
      avail = sdl_ring_room(&sdl->speaker_ring, sdl->speaker_ring_size);
      if (avail >= len)
         return avail;
#ifdef HAVE_THREADS
      slock_lock(sdl->lock);
      signalled = scond_wait_timeout(sdl->cond, sdl->lock,
            SDL_AUDIO_STALL_TIMEOUT_US);
      slock_unlock(sdl->lock);
      if (!signalled)
         return 0;
#else
      return 0;
#endif
   }
}

static void sdl_audio_list_free(void *u, void *slp)
{
   struct string_list *sl = (struct string_list*)slp;

   if (sl)
      string_list_free(sl);
}

audio_driver_t audio_sdl = {
   sdl_audio_init,
   sdl_audio_write,
   sdl_audio_stop,
   sdl_audio_start,
   sdl_audio_alive,
   sdl_audio_set_nonblock_state,
   sdl_audio_free,
   sdl_audio_use_float,
#ifdef HAVE_SDL2
   "sdl2",
#else
   "sdl",
#endif
   sdl_audio_list_new,
   sdl_audio_list_free,
   sdl_audio_write_avail,
   sdl_audio_buffer_size,
   NULL, /* write_raw */
   sdl_audio_wait_writable,
   NULL, /* consumed */
   NULL, /* underruns */
   sdl_audio_layout
};
