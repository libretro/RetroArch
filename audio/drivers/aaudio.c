/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
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

/* AAudio, Android 8.0 (API 26) and up. libaaudio.so is resolved at
 * runtime, so the build needs no API 26 headers and the binary still
 * loads on older releases, where the frontend drops this driver from
 * the list. */

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>
#include <retro_atomic.h>
#include <retro_math.h>
#include <retro_spsc.h>
#include <dynamic/dylib.h>
#include <lists/string_list.h>
#include <string/stdstring.h>
#include <rthreads/retro_eventcount.h>
#ifdef HAVE_RAC3
#include <formats/rac3.h>
#include <formats/iec61937.h>
#endif

#include "../audio_driver.h"
#include "../audio_device_label.h"
#include "../common/android_audio.h"
#include "../../configuration.h"
#include "../../verbosity.h"

/* Bounds a write parked on a stream that has stopped calling back. */
#define AAUDIO_STALL_TIMEOUT_US 256000
/* Bounds the wait for a stopped stream at teardown. */
#define AAUDIO_STOP_TIMEOUT_NS  200000000LL

/* The subset of <aaudio/AAudio.h> this driver uses. */
typedef struct AAudioStreamStruct        AAudioStream;
typedef struct AAudioStreamBuilderStruct AAudioStreamBuilder;
typedef int32_t aaudio_result_t;

#define AAUDIO_OK                           0
#define AAUDIO_UNSPECIFIED                  0
#define AAUDIO_DIRECTION_OUTPUT             0
#define AAUDIO_FORMAT_PCM_I16               1
#define AAUDIO_FORMAT_PCM_FLOAT             2
/* API 34: IEC 61937 bursts in a 2-channel 16-bit carrier. */
#define AAUDIO_FORMAT_IEC61937              5
#define AAUDIO_SHARING_MODE_SHARED          1
#define AAUDIO_PERFORMANCE_MODE_NONE        10
#define AAUDIO_PERFORMANCE_MODE_LOW_LATENCY 12
#define AAUDIO_CALLBACK_RESULT_CONTINUE     0
#define AAUDIO_STREAM_STATE_STOPPED         10
#define AAUDIO_STREAM_STATE_CLOSED          12
#define AAUDIO_STREAM_STATE_DISCONNECTED    13
#define AAUDIO_USAGE_GAME                   14
#define AAUDIO_CONTENT_TYPE_MUSIC           2
/* API 32 channel masks use the WAVEFORMATEXTENSIBLE bits for every
 * position the frontend has, so a layout passes across unchanged. */
#define AAUDIO_CHANNEL_INVALID              0xFFFFFFFFu
#define AAUDIO_LAYOUT_MASK_ALL              0x7FFu

/* AC-3 frames are 1536 samples; the stream runs at 48 kHz. */
#define AAUDIO_AC3_FRAMES                   1536
#define AAUDIO_AC3_RATE                     48000

typedef int32_t (*aaudio_data_cb_t)(AAudioStream*, void*, void*, int32_t);
typedef void    (*aaudio_error_cb_t)(AAudioStream*, void*, aaudio_result_t);

typedef struct aaudio_api
{
   aaudio_result_t (*create_builder)(AAudioStreamBuilder**);
   aaudio_result_t (*builder_delete)(AAudioStreamBuilder*);
   aaudio_result_t (*builder_open)(AAudioStreamBuilder*, AAudioStream**);
   void (*builder_set_device_id)(AAudioStreamBuilder*, int32_t);
   void (*builder_set_sample_rate)(AAudioStreamBuilder*, int32_t);
   void (*builder_set_channel_count)(AAudioStreamBuilder*, int32_t);
   void (*builder_set_format)(AAudioStreamBuilder*, int32_t);
   void (*builder_set_sharing_mode)(AAudioStreamBuilder*, int32_t);
   void (*builder_set_direction)(AAudioStreamBuilder*, int32_t);
   void (*builder_set_performance_mode)(AAudioStreamBuilder*, int32_t);
   void (*builder_set_data_callback)(AAudioStreamBuilder*, aaudio_data_cb_t, void*);
   void (*builder_set_error_callback)(AAudioStreamBuilder*, aaudio_error_cb_t, void*);
   /* API 28; NULL before. */
   void (*builder_set_usage)(AAudioStreamBuilder*, int32_t);
   void (*builder_set_content_type)(AAudioStreamBuilder*, int32_t);
   /* API 32; NULL before. */
   void (*builder_set_channel_mask)(AAudioStreamBuilder*, uint32_t);
   uint32_t (*stream_get_channel_mask)(AAudioStream*);
   aaudio_result_t (*stream_close)(AAudioStream*);
   aaudio_result_t (*stream_request_start)(AAudioStream*);
   aaudio_result_t (*stream_request_pause)(AAudioStream*);
   aaudio_result_t (*stream_request_stop)(AAudioStream*);
   aaudio_result_t (*stream_wait_for_state_change)(AAudioStream*,
         int32_t, int32_t*, int64_t);
   aaudio_result_t (*stream_set_buffer_size)(AAudioStream*, int32_t);
   int32_t (*stream_get_buffer_size)(AAudioStream*);
   int32_t (*stream_get_buffer_capacity)(AAudioStream*);
   int32_t (*stream_get_frames_per_burst)(AAudioStream*);
   int32_t (*stream_get_sample_rate)(AAudioStream*);
   int32_t (*stream_get_channel_count)(AAudioStream*);
   int32_t (*stream_get_format)(AAudioStream*);
   int32_t (*stream_get_device_id)(AAudioStream*);
   int32_t (*stream_get_sharing_mode)(AAudioStream*);
   int32_t (*stream_get_performance_mode)(AAudioStream*);
   int32_t (*stream_get_state)(AAudioStream*);
   const char *(*result_text)(aaudio_result_t);
   dylib_t lib;
} aaudio_api_t;

/* Resolved once and never closed: the device's threads run inside the
 * library, and a stream may call back while another is being opened. */
static aaudio_api_t aaudio_api;
static bool         aaudio_api_ready;
static bool         aaudio_api_tried;

#define AAUDIO_SYM(field, name) \
   if (!(sym = dylib_proc(aaudio_api.lib, name))) \
      goto fail; \
   memcpy(&aaudio_api.field, &sym, sizeof(sym))

#define AAUDIO_SYM_OPT(field, name) \
   if ((sym = dylib_proc(aaudio_api.lib, name))) \
      memcpy(&aaudio_api.field, &sym, sizeof(sym))

static bool aaudio_api_load(void)
{
   function_t sym;

   if (aaudio_api_tried)
      return aaudio_api_ready;
   aaudio_api_tried = true;

   if (!(aaudio_api.lib = dylib_load("libaaudio.so")))
   {
      RARCH_ERR("[AAudio] libaaudio.so not available.\n");
      return false;
   }

   AAUDIO_SYM(create_builder,               "AAudio_createStreamBuilder");
   AAUDIO_SYM(builder_delete,               "AAudioStreamBuilder_delete");
   AAUDIO_SYM(builder_open,                 "AAudioStreamBuilder_openStream");
   AAUDIO_SYM(builder_set_device_id,        "AAudioStreamBuilder_setDeviceId");
   AAUDIO_SYM(builder_set_sample_rate,      "AAudioStreamBuilder_setSampleRate");
   AAUDIO_SYM(builder_set_channel_count,    "AAudioStreamBuilder_setChannelCount");
   AAUDIO_SYM(builder_set_format,           "AAudioStreamBuilder_setFormat");
   AAUDIO_SYM(builder_set_sharing_mode,     "AAudioStreamBuilder_setSharingMode");
   AAUDIO_SYM(builder_set_direction,        "AAudioStreamBuilder_setDirection");
   AAUDIO_SYM(builder_set_performance_mode, "AAudioStreamBuilder_setPerformanceMode");
   AAUDIO_SYM(builder_set_data_callback,    "AAudioStreamBuilder_setDataCallback");
   AAUDIO_SYM(builder_set_error_callback,   "AAudioStreamBuilder_setErrorCallback");
   AAUDIO_SYM(stream_close,                 "AAudioStream_close");
   AAUDIO_SYM(stream_request_start,         "AAudioStream_requestStart");
   AAUDIO_SYM(stream_request_pause,         "AAudioStream_requestPause");
   AAUDIO_SYM(stream_request_stop,          "AAudioStream_requestStop");
   AAUDIO_SYM(stream_wait_for_state_change, "AAudioStream_waitForStateChange");
   AAUDIO_SYM(stream_set_buffer_size,       "AAudioStream_setBufferSizeInFrames");
   AAUDIO_SYM(stream_get_buffer_size,       "AAudioStream_getBufferSizeInFrames");
   AAUDIO_SYM(stream_get_buffer_capacity,   "AAudioStream_getBufferCapacityInFrames");
   AAUDIO_SYM(stream_get_frames_per_burst,  "AAudioStream_getFramesPerBurst");
   AAUDIO_SYM(stream_get_sample_rate,       "AAudioStream_getSampleRate");
   AAUDIO_SYM(stream_get_channel_count,     "AAudioStream_getChannelCount");
   AAUDIO_SYM(stream_get_format,            "AAudioStream_getFormat");
   AAUDIO_SYM(stream_get_device_id,         "AAudioStream_getDeviceId");
   AAUDIO_SYM(stream_get_sharing_mode,      "AAudioStream_getSharingMode");
   AAUDIO_SYM(stream_get_performance_mode,  "AAudioStream_getPerformanceMode");
   AAUDIO_SYM(stream_get_state,             "AAudioStream_getState");
   AAUDIO_SYM(result_text,                  "AAudio_convertResultToText");

   AAUDIO_SYM_OPT(builder_set_usage,        "AAudioStreamBuilder_setUsage");
   AAUDIO_SYM_OPT(builder_set_content_type, "AAudioStreamBuilder_setContentType");
   /* Only as a pair: a mask asked for must be one that can be read back. */
   AAUDIO_SYM_OPT(builder_set_channel_mask, "AAudioStreamBuilder_setChannelMask");
   AAUDIO_SYM_OPT(stream_get_channel_mask,  "AAudioStream_getChannelMask");
   if (!aaudio_api.builder_set_channel_mask || !aaudio_api.stream_get_channel_mask)
   {
      aaudio_api.builder_set_channel_mask = NULL;
      aaudio_api.stream_get_channel_mask  = NULL;
   }

   aaudio_api_ready = true;
   return true;

fail:
   RARCH_ERR("[AAudio] libaaudio.so lacks a required symbol.\n");
   dylib_close(aaudio_api.lib);
   memset(&aaudio_api, 0, sizeof(aaudio_api));
   return false;
}

#undef AAUDIO_SYM
#undef AAUDIO_SYM_OPT

/* What the error callback reaches. AAudio runs it on a thread of its
 * own that is not ordered against close(), so it touches nothing a
 * free() releases: it is told which stream it belongs to by the
 * generation it was registered with, and is dropped when that stream
 * is no longer the current one. Never freed; one driver at a time. */
typedef struct aaudio_shared
{
   retro_eventcount_t  park;
   retro_atomic_int_t *reinit_request;
   retro_atomic_int_t  generation;
   /* The generation whose stream reported an error. */
   retro_atomic_int_t  dead;
   bool                park_ready;
} aaudio_shared_t;

static aaudio_shared_t aaudio_shared;

typedef struct aaudio
{
   retro_spsc_t        ring;
   /* Bytes handed to the device, and callbacks that found the ring
    * short and padded it. Bumped on the callback thread. */
   retro_atomic_size_t consumed_bytes;
   retro_atomic_size_t underruns;
   AAudioStream       *stream;
#ifdef HAVE_RAC3
   /* AC-3 over IEC 61937: the frontend writes float frames of the
    * layout, 1536 of them make a burst of 1536 carrier frames, and the
    * ring and the device see only bursts. A frontend frame and a
    * device frame are the same count; only the bytes differ. */
   rac3_encoder_t     *ac3;
   float              *ac3_in;
   size_t              ac3_frame_bytes;
   unsigned            ac3_in_frames;
   /* Bytes of the pause burst the last callback left unfinished. */
   unsigned            pause_off;
   /* A burst has been written: pause bursts before it are not gaps. */
   retro_atomic_int_t  fed;
#endif
   size_t              ring_size;
   /* Device side: bytes of a frame in the ring and on the stream. */
   size_t              frame_bytes;
   uint32_t            layout;
   unsigned            rate;
   int                 generation;
#ifdef HAVE_RAC3
   uint8_t             pause[IEC61937_PAUSE_BURST_BYTES];
   uint8_t             ac3_frame[RAC3_MAX_FRAME_BYTES];
   uint8_t             ac3_burst[IEC61937_AC3_BURST_BYTES];
#endif
   bool                ring_init;
   bool                use_float;
   bool                nonblock;
   bool                is_paused;
} aaudio_t;

static bool aaudio_is_dead(const aaudio_t *aa)
{
   return retro_atomic_load_acquire_int(&aaudio_shared.dead)
      == aa->generation;
}

/* Room against the size asked for: retro_spsc rounds the capacity up. */
static size_t aaudio_ring_room(const aaudio_t *aa)
{
   size_t room   = retro_spsc_write_avail(&aa->ring);
   size_t excess = aa->ring.capacity - aa->ring_size;
   return room > excess ? room - excess : 0;
}

/* Room in the frontend's bytes. On the AC-3 path that is the ring's
 * frames less the ones still in the encoder, at the frontend's frame
 * size. */
static size_t aaudio_avail(const aaudio_t *aa)
{
#ifdef HAVE_RAC3
   if (aa->ac3)
   {
      size_t room = aaudio_ring_room(aa) / aa->frame_bytes;
      return room > aa->ac3_in_frames
         ? (room - aa->ac3_in_frames) * aa->ac3_frame_bytes : 0;
   }
#endif
   return aaudio_ring_room(aa);
}

#ifdef HAVE_RAC3
/* The device's buffer from the ring, and pause bursts where the ring
 * has run dry - a receiver given zeros drops its lock and mutes. The
 * ring only ever holds whole bursts, so it runs dry on a boundary; a
 * pause burst cut short by the end of the buffer is finished first
 * on the next call. */
static void aaudio_ac3_fill(aaudio_t *aa, uint8_t *out, size_t len)
{
   size_t pos  = 0;
   bool   gap  = false;
   while (pos < len)
   {
      size_t n;
      if (!aa->pause_off)
      {
         pos += retro_spsc_read(&aa->ring, out + pos, len - pos);
         if (pos >= len)
            break;
         gap = true;
      }
      n = IEC61937_PAUSE_BURST_BYTES - aa->pause_off;
      if (n > len - pos)
         n = len - pos;
      memcpy(out + pos, aa->pause + aa->pause_off, n);
      pos          += n;
      aa->pause_off = (unsigned)((aa->pause_off + n)
            % IEC61937_PAUSE_BURST_BYTES);
   }
   if (gap && retro_atomic_load_acquire_int(&aa->fed))
      retro_atomic_fetch_add_size(&aa->underruns, 1);
}
#endif

static int32_t aaudio_data_cb(AAudioStream *stream, void *user,
      void *audio, int32_t frames)
{
   aaudio_t *aa = (aaudio_t*)user;
   size_t   len = (size_t)frames * aa->frame_bytes;
   (void)stream;
#ifdef HAVE_RAC3
   if (aa->ac3)
      aaudio_ac3_fill(aa, (uint8_t*)audio, len);
   else
#endif
   {
      size_t _len = retro_spsc_read(&aa->ring, audio, len);
      if (_len < len)
      {
         memset((uint8_t*)audio + _len, 0, len - _len);
         retro_atomic_fetch_add_size(&aa->underruns, 1);
      }
   }
   retro_atomic_fetch_add_size(&aa->consumed_bytes, len);
   retro_eventcount_notify(&aaudio_shared.park);
   return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

/* A disconnect - headphones out, a USB or Bluetooth device gone, a
 * route change on 9.0 and up - ends the stream for good. The frontend
 * reopens on the main thread; this only says so and frees the writer. */
static void aaudio_error_cb(AAudioStream *stream, void *user,
      aaudio_result_t error)
{
   int gen = (int)(intptr_t)user;
   (void)stream;
   (void)error;
   if (gen != retro_atomic_load_acquire_int(&aaudio_shared.generation))
      return;
   retro_atomic_store_release_int(&aaudio_shared.dead, gen);
   retro_eventcount_notify(&aaudio_shared.park);
   if (aaudio_shared.reinit_request)
      retro_atomic_store_release_int(aaudio_shared.reinit_request, 1);
}

/* The positions a stream of this many channels plays, where only the
 * count could be asked for: Android's canonical mask for the count.
 * Zero for a count the frontend has no layout for. */
static uint32_t aaudio_canonical_layout(int32_t channels)
{
   switch (channels)
   {
      case 2:
         return AUDIO_LAYOUT_STEREO;
      case 3:
         return AUDIO_LAYOUT_STEREO | AUDIO_SPEAKER_FRONT_CENTER;
      case 4:
         return AUDIO_LAYOUT_QUAD;
      case 5:
         return AUDIO_LAYOUT_QUAD | AUDIO_SPEAKER_FRONT_CENTER;
      case 6:
         return AUDIO_LAYOUT_5POINT1;
      case 7:
         return AUDIO_LAYOUT_5POINT1 | AUDIO_SPEAKER_BACK_CENTER;
      case 8:
         return AUDIO_LAYOUT_7POINT1;
      default:
         break;
   }
   return 0;
}

static const char *aaudio_result_text(aaudio_result_t res)
{
   const char *s = aaudio_api.result_text ? aaudio_api.result_text(res) : NULL;
   return s ? s : "?";
}

static aaudio_result_t aaudio_open(aaudio_t *aa, int32_t device_id,
      uint32_t layout, int32_t format, bool use_mask, int32_t rate,
      int32_t perf)
{
   AAudioStreamBuilder *b = NULL;
   aaudio_result_t res    = aaudio_api.create_builder(&b);
   if (res != AAUDIO_OK)
      return res;

   aaudio_api.builder_set_direction(b, AAUDIO_DIRECTION_OUTPUT);
   aaudio_api.builder_set_sharing_mode(b, AAUDIO_SHARING_MODE_SHARED);
   aaudio_api.builder_set_performance_mode(b, perf);
   aaudio_api.builder_set_format(b, format);
   aaudio_api.builder_set_sample_rate(b, rate);
   if (device_id > 0)
      aaudio_api.builder_set_device_id(b, device_id);
   if (use_mask)
      aaudio_api.builder_set_channel_mask(b, layout);
   else
      aaudio_api.builder_set_channel_count(b,
            (int32_t)audio_layout_channels(layout));
   if (aaudio_api.builder_set_usage)
      aaudio_api.builder_set_usage(b, AAUDIO_USAGE_GAME);
   if (aaudio_api.builder_set_content_type)
      aaudio_api.builder_set_content_type(b, AAUDIO_CONTENT_TYPE_MUSIC);
   aaudio_api.builder_set_data_callback(b, aaudio_data_cb, aa);
   aaudio_api.builder_set_error_callback(b, aaudio_error_cb,
         (void*)(intptr_t)aa->generation);

   res = aaudio_api.builder_open(b, &aa->stream);
   aaudio_api.builder_delete(b);
   if (res != AAUDIO_OK)
      aa->stream = NULL;
   return res;
}

/* PCM at the device's own rate, which keeps the stream on the fast
 * path; the frontend resamples to whatever it is. */
static aaudio_result_t aaudio_open_pcm(aaudio_t *aa, int32_t device_id,
      uint32_t layout, int32_t format, bool use_mask)
{
   return aaudio_open(aa, device_id, layout, format, use_mask,
         AAUDIO_UNSPECIFIED, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
}

/* The layout as a mask where the platform takes one, then as a count,
 * then stereo; on the device asked for, then on the default. */
static aaudio_result_t aaudio_open_best(aaudio_t *aa, int32_t *device_id,
      uint32_t *layout, int32_t format)
{
   aaudio_result_t res = AAUDIO_OK;
   int pass;

   for (pass = 0; pass < 2; pass++)
   {
      if (pass)
      {
         if (*device_id <= 0)
            break;
         RARCH_WARN("[AAudio] Could not open device %d (%s). Using the default device.\n",
               (int)*device_id, aaudio_result_text(res));
         *device_id = 0;
      }
      if (*layout != AUDIO_LAYOUT_STEREO)
      {
         if (     aaudio_api.builder_set_channel_mask
               && (res = aaudio_open_pcm(aa, *device_id, *layout, format,
                     true)) == AAUDIO_OK)
            return res;
         if ((res = aaudio_open_pcm(aa, *device_id, *layout, format,
                     false)) == AAUDIO_OK)
            return res;
         RARCH_LOG("[AAudio] %u channels refused (%s), trying stereo.\n",
               audio_layout_channels(*layout), aaudio_result_text(res));
      }
      if ((res = aaudio_open_pcm(aa, *device_id, AUDIO_LAYOUT_STEREO,
                  format, false)) == AAUDIO_OK)
      {
         *layout = AUDIO_LAYOUT_STEREO;
         return res;
      }
   }
   return res;
}

/* What the open stream plays, as a layout; zero when it is nothing the
 * frontend can produce. */
static uint32_t aaudio_stream_layout(aaudio_t *aa)
{
   int32_t  channels = aaudio_api.stream_get_channel_count(aa->stream);
   if (aaudio_api.stream_get_channel_mask)
   {
      uint32_t mask = aaudio_api.stream_get_channel_mask(aa->stream);
      if (     mask != AAUDIO_CHANNEL_INVALID
            && !(mask & ~AAUDIO_LAYOUT_MASK_ALL)
            && audio_layout_channels(mask) == (unsigned)channels)
         return mask;
   }
   return aaudio_canonical_layout(channels);
}

static void aaudio_close_stream(AAudioStream *stream)
{
   int32_t state;
   int     i;

   if (!stream)
      return;

   /* Stopped before it is closed, so no callback is still running in a
    * ring about to be freed. */
   aaudio_api.stream_request_stop(stream);
   state = aaudio_api.stream_get_state(stream);
   for (i = 0; i < 4
         && state != AAUDIO_STREAM_STATE_STOPPED
         && state != AAUDIO_STREAM_STATE_DISCONNECTED
         && state != AAUDIO_STREAM_STATE_CLOSED; i++)
   {
      int32_t next = state;
      if (aaudio_api.stream_wait_for_state_change(stream, state, &next,
               AAUDIO_STOP_TIMEOUT_NS / 4) != AAUDIO_OK)
         break;
      state = next;
   }
   aaudio_api.stream_close(stream);
}

static void aaudio_free(void *data)
{
   aaudio_t *aa = (aaudio_t*)data;
   if (!aa)
      return;

   /* Before the stream goes, so a late error report is dropped. */
   retro_atomic_fetch_add_int(&aaudio_shared.generation, 1);
   aaudio_close_stream(aa->stream);
   aa->stream = NULL;

#ifdef HAVE_RAC3
   if (aa->ac3)
      rac3_encoder_free(aa->ac3);
   free(aa->ac3_in);
#endif
   if (aa->ring_init)
      retro_spsc_free(&aa->ring);
   free(aa);
}

#ifdef HAVE_RAC3
/* A wider layout the output cannot take as PCM goes as AC-3, where it
 * lists AC-3 among its encodings: an HDMI or eARC sink that takes
 * stereo PCM only usually decodes Dolby Digital. Android's mixer would
 * otherwise accept the layout and downmix it, so it is the output's
 * own channel counts that decide. A/52 takes 1.0 to 5.1; a 7.1 request
 * goes as 5.1, its back pair folded into the surround pair by the
 * upmix to what layout() reports. False leaves the PCM stream as it
 * was. */
static bool aaudio_try_ac3(aaudio_t *aa, uint32_t want)
{
   unsigned      max_ch = 0;
   bool          ac3    = false;
   uint32_t      enc_layout;
   unsigned      enc_ch, kbps;
   int32_t       id     = aaudio_api.stream_get_device_id(aa->stream);
   AAudioStream *pcm    = aa->stream;

   if (     !frontend_android_audio_device_caps(id, &max_ch, &ac3)
         || !ac3 || !max_ch || max_ch >= audio_layout_channels(want))
      return false;

   enc_layout = want;
   if (enc_layout & (AUDIO_SPEAKER_BACK_LEFT | AUDIO_SPEAKER_BACK_RIGHT))
      enc_layout = (enc_layout
            & ~(AUDIO_SPEAKER_BACK_LEFT | AUDIO_SPEAKER_BACK_RIGHT))
            | AUDIO_SPEAKER_SIDE_LEFT | AUDIO_SPEAKER_SIDE_RIGHT;
   if (rac3_layout_acmod(enc_layout) < 0)
      enc_layout = (enc_layout & AUDIO_SPEAKER_LOW_FREQUENCY)
         ? AUDIO_LAYOUT_5POINT1_SURROUND
         : (AUDIO_LAYOUT_5POINT1_SURROUND & ~AUDIO_SPEAKER_LOW_FREQUENCY);
   enc_ch = audio_layout_channels(enc_layout);
   kbps   = enc_ch >= 5 ? 640 : enc_ch >= 3 ? 448 : 256;

   if (!(aa->ac3 = rac3_encoder_new(AAUDIO_AC3_RATE, enc_layout, kbps)))
      return false;
   if (!(aa->ac3_in = (float*)calloc((size_t)AAUDIO_AC3_FRAMES * enc_ch,
               sizeof(float))))
      goto fail;

   aa->stream = NULL;
   if (aaudio_open(aa, id, AUDIO_LAYOUT_STEREO, AAUDIO_FORMAT_IEC61937,
            false, AAUDIO_AC3_RATE, AAUDIO_PERFORMANCE_MODE_NONE) != AAUDIO_OK)
      goto fail;
   if (     aaudio_api.stream_get_format(aa->stream) != AAUDIO_FORMAT_IEC61937
         || aaudio_api.stream_get_sample_rate(aa->stream) != AAUDIO_AC3_RATE
         || aaudio_api.stream_get_channel_count(aa->stream) != 2)
   {
      aaudio_close_stream(aa->stream);
      goto fail;
   }

   aaudio_close_stream(pcm);
   aa->layout          = enc_layout;
   aa->ac3_frame_bytes = enc_ch * sizeof(float);
   aa->frame_bytes     = 2 * sizeof(int16_t);
   aa->use_float       = true;
   iec61937_pause_burst(IEC61937_PAUSE_BURST_BYTES / 4, aa->pause,
         sizeof(aa->pause));
   RARCH_LOG("[AAudio] Output %d takes %u PCM channels: layout 0x%03x goes as AC-3 over IEC 61937, %u kbit/s.\n",
         (int)id, max_ch, enc_layout, kbps);
   return true;

fail:
   aa->stream = pcm;
   if (aa->ac3)
      rac3_encoder_free(aa->ac3);
   free(aa->ac3_in);
   aa->ac3    = NULL;
   aa->ac3_in = NULL;
   RARCH_LOG("[AAudio] Output %d does not take AC-3 over IEC 61937; staying on PCM.\n",
         (int)id);
   return false;
}
#endif

/* Fills the ring with what the device plays when nothing has been
 * written: silence, or pause bursts on the AC-3 path. */
static void aaudio_prefill(aaudio_t *aa, size_t bytes)
{
   while (bytes)
   {
      void  *ptr  = NULL;
      size_t span = retro_spsc_write_begin(&aa->ring, &ptr);
      if (span > bytes)
         span = bytes;
#ifdef HAVE_RAC3
      if (aa->ac3)
         span -= span % IEC61937_PAUSE_BURST_BYTES;
#endif
      if (!span || !ptr)
      {
         retro_spsc_write_end(&aa->ring, 0);
         break;
      }
#ifdef HAVE_RAC3
      if (aa->ac3)
      {
         size_t i;
         for (i = 0; i < span; i += IEC61937_PAUSE_BURST_BYTES)
            memcpy((uint8_t*)ptr + i, aa->pause, IEC61937_PAUSE_BURST_BYTES);
      }
      else
#endif
         memset(ptr, 0, span);
      retro_spsc_write_end(&aa->ring, span);
      bytes -= span;
   }
}

static void *aaudio_init(const char *device, unsigned rate,
      unsigned latency, unsigned *new_rate)
{
   aaudio_result_t res;
   int32_t  burst, device_frames, want_device, format;
   size_t   ring_frames;
   uint32_t layout;
#ifdef HAVE_RAC3
   uint32_t want;
#endif
   int32_t  device_id   = 0;
   bool     pcm_format  = true;
   settings_t *settings = config_get_ptr();
   aaudio_t *aa;

   if (!aaudio_api_load())
      return NULL;

   if (!aaudio_shared.park_ready)
   {
      if (!retro_eventcount_init(&aaudio_shared.park))
         return NULL;
      aaudio_shared.park_ready = true;
   }
   aaudio_shared.reinit_request = &audio_state_get_ptr()->reinit_request;

   if (!(aa = (aaudio_t*)calloc(1, sizeof(*aa))))
      return NULL;

   retro_atomic_size_init(&aa->consumed_bytes, 0);
   retro_atomic_size_init(&aa->underruns, 0);
#ifdef HAVE_RAC3
   retro_atomic_int_init(&aa->fed, 0);
#endif
   aa->generation = retro_atomic_fetch_add_int(&aaudio_shared.generation, 1) + 1;

   /* A device list entry is "<id> (<name>, <type>)". */
   if (!string_is_empty(device))
   {
      char id[32];
      char *end = NULL;
      long  v;
      audio_device_label_path(id, sizeof(id), device);
      v = strtol(id, &end, 10);
      if (end != id && *end == '\0' && v > 0 && v <= 0x7FFFFFFFL)
         device_id = (int32_t)v;
   }

   layout = audio_driver_requested_layout();
   if (     audio_layout_channels(layout) < 2
         || audio_layout_channels(layout) > 8
         || (layout & ~AAUDIO_LAYOUT_MASK_ALL))
      layout = AUDIO_LAYOUT_STEREO;
#ifdef HAVE_RAC3
   want   = layout;
#endif

   format = (settings && settings->uints.audio_format_negotiation
         == AUDIO_FORMAT_NEGOTIATION_INT16)
      ? AAUDIO_FORMAT_PCM_I16 : AAUDIO_FORMAT_PCM_FLOAT;

   if ((res = aaudio_open_best(aa, &device_id, &layout, format)) != AAUDIO_OK)
   {
      RARCH_ERR("[AAudio] Could not open an output stream: %s.\n",
            aaudio_result_text(res));
      goto error;
   }

   aa->layout = aaudio_stream_layout(aa);
   if (!aa->layout && layout != AUDIO_LAYOUT_STEREO)
   {
      RARCH_LOG("[AAudio] Unusable channel layout, using stereo.\n");
      aaudio_close_stream(aa->stream);
      aa->stream = NULL;
      if ((res = aaudio_open_pcm(aa, device_id, AUDIO_LAYOUT_STEREO, format,
               false)) != AAUDIO_OK)
         goto error;
      aa->layout = aaudio_stream_layout(aa);
   }
   if (!aa->layout)
      goto error;

#ifdef HAVE_RAC3
   if (audio_layout_channels(want) > 2 && aaudio_try_ac3(aa, want))
      pcm_format = false;
#endif

   if (pcm_format)
   {
      switch (aaudio_api.stream_get_format(aa->stream))
      {
         case AAUDIO_FORMAT_PCM_FLOAT:
            aa->use_float = true;
            break;
         case AAUDIO_FORMAT_PCM_I16:
            aa->use_float = false;
            break;
         default:
            RARCH_ERR("[AAudio] Stream opened with an unknown sample format.\n");
            goto error;
      }
      aa->frame_bytes = audio_layout_channels(aa->layout)
         * (aa->use_float ? sizeof(float) : sizeof(int16_t));
   }

   aa->rate        = (unsigned)aaudio_api.stream_get_sample_rate(aa->stream);
   if (!aa->rate)
      aa->rate     = rate;
   if (new_rate)
      *new_rate    = aa->rate;

   /* The device stage is two bursts, the shortest that does not glitch
    * on a callback stream; the ring in front of it takes the rest of
    * the latency setting, and is never less than two bursts itself. */
   burst = aaudio_api.stream_get_frames_per_burst(aa->stream);
   if (burst <= 0)
      burst = 192;
   want_device = burst * 2;
   if (want_device > aaudio_api.stream_get_buffer_capacity(aa->stream))
      want_device = aaudio_api.stream_get_buffer_capacity(aa->stream);
   if (want_device > 0)
      aaudio_api.stream_set_buffer_size(aa->stream, want_device);
   device_frames = aaudio_api.stream_get_buffer_size(aa->stream);
   if (device_frames < 0)
      device_frames = 0;
   audio_driver_set_device_latency((size_t)device_frames);

   ring_frames = ((size_t)aa->rate * latency) / 1000;
   ring_frames = ring_frames > (size_t)device_frames
      ? ring_frames - (size_t)device_frames : 0;
   if (ring_frames < (size_t)burst * 2)
      ring_frames = (size_t)burst * 2;
#ifdef HAVE_RAC3
   /* Whole bursts, four at least: one is encoded on the writer's
    * thread when its 1536 frames are in, so the ring is filled in
    * steps of one against a span of four. */
   if (aa->ac3)
   {
      ring_frames = (ring_frames + AAUDIO_AC3_FRAMES - 1)
         / AAUDIO_AC3_FRAMES * AAUDIO_AC3_FRAMES;
      if (ring_frames < 4 * AAUDIO_AC3_FRAMES)
         ring_frames = 4 * AAUDIO_AC3_FRAMES;
   }
#endif
   aa->ring_size = ring_frames * aa->frame_bytes;
   if (!(aa->ring_init = retro_spsc_init_mirrored(&aa->ring, aa->ring_size)))
      goto error;

   /* Half the ring, the rate control's setpoint, so the first
    * callbacks have something to play and the writer does not start
    * full. */
   aaudio_prefill(aa, (ring_frames / 2) * aa->frame_bytes);

   RARCH_LOG("[AAudio] %u Hz, layout 0x%03x, %s, %s%s, burst %d frames, "
         "device buffer %d frames, ring %u frames (%u ms requested).\n",
         aa->rate, aa->layout,
         !pcm_format ? "AC-3" : aa->use_float ? "float" : "16-bit",
         aaudio_api.stream_get_sharing_mode(aa->stream)
            == AAUDIO_SHARING_MODE_SHARED ? "shared" : "exclusive",
         aaudio_api.stream_get_performance_mode(aa->stream)
            == AAUDIO_PERFORMANCE_MODE_LOW_LATENCY ? ", low latency" : "",
         (int)burst, (int)device_frames, (unsigned)ring_frames, latency);

   if ((res = aaudio_api.stream_request_start(aa->stream)) != AAUDIO_OK)
   {
      RARCH_ERR("[AAudio] Could not start the stream: %s.\n",
            aaudio_result_text(res));
      goto error;
   }

   return aa;

error:
   aaudio_free(aa);
   return NULL;
}

/* Waits until the ring has need bytes of room. False when it will not
 * get them: the stream was lost, or nothing drained within the bound. */
static bool aaudio_wait_room(aaudio_t *aa, size_t need)
{
   while (aaudio_ring_room(aa) < need)
   {
      int key;
      if (aaudio_is_dead(aa))
         return false;
      key = retro_eventcount_prepare_wait(&aaudio_shared.park);
      if (aaudio_ring_room(aa) >= need || aaudio_is_dead(aa))
      {
         retro_eventcount_cancel_wait(&aaudio_shared.park);
         continue;
      }
      if (!retro_eventcount_commit_wait_timeout(&aaudio_shared.park,
               key, AAUDIO_STALL_TIMEOUT_US))
         return false;
   }
   return true;
}

#ifdef HAVE_RAC3
/* The frames that complete a burst are taken only when the burst fits
 * whole: part of one puts the receiver out of sync until the next
 * preamble. Returns the frontend's bytes taken. */
static ssize_t aaudio_write_ac3(aaudio_t *aa, const void *s, size_t len)
{
   const uint8_t *src = (const uint8_t*)s;
   size_t frames      = len / aa->ac3_frame_bytes;
   size_t done        = 0;

   while (done < frames)
   {
      size_t take = AAUDIO_AC3_FRAMES - aa->ac3_in_frames;
      if (take > frames - done)
         take = frames - done;
      if (     aa->ac3_in_frames + take == AAUDIO_AC3_FRAMES
            && aaudio_ring_room(aa) < IEC61937_AC3_BURST_BYTES)
      {
         if (     aa->nonblock || aa->is_paused
               || !aaudio_wait_room(aa, IEC61937_AC3_BURST_BYTES))
            break;
      }
      memcpy((uint8_t*)aa->ac3_in
            + (size_t)aa->ac3_in_frames * aa->ac3_frame_bytes,
            src + done * aa->ac3_frame_bytes, take * aa->ac3_frame_bytes);
      aa->ac3_in_frames += (unsigned)take;
      done              += take;
      if (aa->ac3_in_frames == AAUDIO_AC3_FRAMES)
      {
         size_t n = rac3_encode_frame(aa->ac3, aa->ac3_in, aa->ac3_frame,
               sizeof(aa->ac3_frame));
         size_t b = n ? iec61937_wrap_ac3(aa->ac3_frame, n, 0,
               aa->ac3_burst, sizeof(aa->ac3_burst)) : 0;
         aa->ac3_in_frames = 0;
         if (b == IEC61937_AC3_BURST_BYTES)
         {
            retro_spsc_write(&aa->ring, aa->ac3_burst, b);
            retro_atomic_store_release_int(&aa->fed, 1);
         }
         else
            retro_atomic_fetch_add_size(&aa->underruns, 1);
      }
   }
   return (ssize_t)(done * aa->ac3_frame_bytes);
}
#endif

static ssize_t aaudio_write(void *data, const void *s, size_t len)
{
   aaudio_t *aa  = (aaudio_t*)data;
   size_t  _len  = 0;
   const uint8_t *buf = (const uint8_t*)s;

   /* A lost stream takes everything until the frontend reopens it, so
    * the core does not stall on a device that will never drain. */
   if (aaudio_is_dead(aa))
      return (ssize_t)len;
#ifdef HAVE_RAC3
   if (aa->ac3)
      return aaudio_write_ac3(aa, s, len);
#endif

   while (_len < len)
   {
      size_t avail = aaudio_ring_room(aa);
      if (!avail)
      {
         if (aa->nonblock || aa->is_paused || !aaudio_wait_room(aa, 1))
            break;
         continue;
      }
      if (avail > len - _len)
         avail = len - _len;
      retro_spsc_write(&aa->ring, buf + _len, avail);
      _len += avail;
   }

   return (ssize_t)_len;
}

/* The lend pair: the ring the callback drains, in whole frames. The
 * AC-3 path has no PCM front to lend. */
static size_t aaudio_write_begin(void *data, size_t len, void **region)
{
   aaudio_t *aa = (aaudio_t*)data;
   void   *ptr  = NULL;
   size_t  room, span;
   *region = NULL;
   if (!aa || !aa->ring_init || aa->is_paused || aaudio_is_dead(aa))
      return 0;
#ifdef HAVE_RAC3
   if (aa->ac3)
      return 0;
#endif
   room = aaudio_ring_room(aa);
   span = retro_spsc_write_begin(&aa->ring, &ptr);
   if (span > room)
      span = room;
   if (span > len)
      span = len;
   span -= span % aa->frame_bytes;
   if (!span || !ptr)
   {
      retro_spsc_write_end(&aa->ring, 0);
      return 0;
   }
   *region = ptr;
   return span;
}

static ssize_t aaudio_write_end(void *data, size_t len)
{
   aaudio_t *aa = (aaudio_t*)data;
   len -= len % aa->frame_bytes;
   retro_spsc_write_end(&aa->ring, len);
   return (ssize_t)len;
}

static size_t aaudio_buffer_size(void *data)
{
   aaudio_t *aa = (aaudio_t*)data;
#ifdef HAVE_RAC3
   if (aa->ac3)
      return aa->ring_size / aa->frame_bytes * aa->ac3_frame_bytes;
#endif
   return aa->ring_size;
}

static size_t aaudio_wait_writable(void *data, size_t len)
{
   aaudio_t *aa = (aaudio_t*)data;
   size_t total = aaudio_buffer_size(aa);
   int laps     = 8;

   if (len > total / 2)
      len = total / 2;

   while (laps-- > 0)
   {
      int key;
      size_t avail;
      if (aaudio_is_dead(aa))
         return 0;
      if ((avail = aaudio_avail(aa)) >= len)
         return avail;
      key = retro_eventcount_prepare_wait(&aaudio_shared.park);
      if (aaudio_avail(aa) >= len || aaudio_is_dead(aa))
      {
         retro_eventcount_cancel_wait(&aaudio_shared.park);
         continue;
      }
      if (!retro_eventcount_commit_wait_timeout(&aaudio_shared.park,
               key, AAUDIO_STALL_TIMEOUT_US))
         return 0;
   }
   return 0;
}

static bool aaudio_stop(void *data)
{
   aaudio_t *aa = (aaudio_t*)data;
   if (aa->is_paused)
      return true;
   if (     !aaudio_is_dead(aa)
         && aaudio_api.stream_request_pause(aa->stream) != AAUDIO_OK)
      return false;
   aa->is_paused = true;
   return true;
}

static bool aaudio_start(void *data, bool is_shutdown)
{
   aaudio_t *aa = (aaudio_t*)data;
   (void)is_shutdown;
   if (!aa->is_paused)
      return true;
   /* A lost stream is reopened by the frontend; not a failure. */
   if (     !aaudio_is_dead(aa)
         && aaudio_api.stream_request_start(aa->stream) != AAUDIO_OK)
      return false;
   aa->is_paused = false;
   return true;
}

static bool aaudio_alive(void *data)
{
   aaudio_t *aa = (aaudio_t*)data;
   return aa && !aa->is_paused;
}

static void aaudio_set_nonblock_state(void *data, bool state)
{
   aaudio_t *aa = (aaudio_t*)data;
   if (aa)
      aa->nonblock = state;
}

static bool aaudio_use_float(void *data)
{
   aaudio_t *aa = (aaudio_t*)data;
   return aa->use_float;
}

static size_t aaudio_write_avail(void *data)
{
   aaudio_t *aa = (aaudio_t*)data;
   if (aaudio_is_dead(aa))
      return aaudio_buffer_size(aa);
   return aaudio_avail(aa);
}

/* Device time: what each callback asked for, padding included. A
 * carrier frame on the AC-3 path is one frame of the layout. */
static size_t aaudio_frames_consumed(void *data)
{
   aaudio_t *aa = (aaudio_t*)data;
   if (!aa || !aa->frame_bytes)
      return 0;
   return retro_atomic_load_acquire_size(&aa->consumed_bytes)
      / aa->frame_bytes;
}

static size_t aaudio_underruns(void *data)
{
   aaudio_t *aa = (aaudio_t*)data;
   return aa ? retro_atomic_load_acquire_size(&aa->underruns) : 0;
}

static uint32_t aaudio_layout(void *data)
{
   aaudio_t *aa = (aaudio_t*)data;
   return (aa && aa->layout) ? aa->layout : AUDIO_LAYOUT_STEREO;
}

static void *aaudio_device_list_new(void *data)
{
   (void)data;
   return frontend_android_audio_outputs_new();
}

static void aaudio_device_list_free(void *data, void *list)
{
   (void)data;
   if (list)
      string_list_free((struct string_list*)list);
}

audio_driver_t audio_aaudio = {
   aaudio_init,
   aaudio_write,
   aaudio_stop,
   aaudio_start,
   aaudio_alive,
   aaudio_set_nonblock_state,
   aaudio_free,
   aaudio_use_float,
   "aaudio",
   aaudio_device_list_new,
   aaudio_device_list_free,
   aaudio_write_avail,
   aaudio_buffer_size,
   NULL, /* write_raw */
   aaudio_wait_writable,
   aaudio_frames_consumed,
   aaudio_underruns,
   aaudio_layout,
   NULL, /* frames_consumed_fallback */
   NULL, /* device_clock_ppm */
   NULL, /* thread_grant */
   aaudio_write_begin,
   aaudio_write_end
};
