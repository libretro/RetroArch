/**
 *  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2025 - Daniel De Matteis
 *  Copyright (C) 2023-2025 - Jesse Talavera-Greenberg
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation, either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with RetroArch. If not, see <http://www.gnu.org/licenses/>.
 **/

#include <limits.h>
#include <math.h>
#include <memalign.h>

#if defined(__SSE__)
#include <xmmintrin.h>
#endif

#if (defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(HAVE_NEON))
#include <arm_neon.h>
#include <features/features_cpu.h>
#endif

#include "audio_driver.h"
#include "../defaults.h"
#include "audio_stretch.h"
#include "audio_speed_lpf.h"

#include <retro_assert.h>
#include <string/stdstring.h>
#include <encodings/utf.h>
#include <retro_miscellaneous.h>
#include <clamping.h>
#include <memalign.h>
#include <audio/conversion/float_to_s16.h>
#include <audio/conversion/s16_to_float.h>
#include <audio/sinc_resampler_int16.h>
#ifdef HAVE_NEAREST_RESAMPLER
#include <audio/nearest_resampler_int16.h>
#endif
#ifdef HAVE_CC_RESAMPLER
#include <audio/cc_resampler_int16.h>
#endif
#include <audio/conversion/dual_mono.h>
#ifdef HAVE_AUDIOMIXER
#include <audio/audio_mixer.h>
#include "../tasks/task_audio_mixer.h"
#endif
#ifdef HAVE_DSP_FILTER
#include <audio/dsp_filter.h>
#endif
#include <lists/string_list.h>
#include <lists/dir_list.h>

#include <compat/strl.h>

#ifdef HAVE_THREADS
#include "audio_thread_wrapper.h"
#include "audio_pipeline_stretch.h"
#define AUDIO_TRANSPORT_FOLLOW 1
#define AUDIO_TRANSPORT_LOWPASS 2
#define AUDIO_TRANSPORT_UPDATE 4
static bool audio_driver_transport_update_runloop(audio_driver_state_t *audio_st);
static bool audio_driver_transport_recover(bool resume, uint32_t tempo);
static bool audio_driver_pipe_ff_waits(audio_driver_state_t *audio_st);
static bool audio_driver_pipeline_transport_publish(uint32_t tempo_q16,
      bool active, bool reset, uint32_t cutoff, bool engage_ahead);
#endif

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif

#ifdef HAVE_NETWORKING
#include "../network/netplay/netplay.h"
#endif

#ifdef HAVE_MICROPHONE
#include "microphone_driver.h"
#endif

#include "../command.h"
#include "../configuration.h"
#include "../driver.h"
#include "../frontend/frontend_driver.h"
#include "../retroarch.h"
#include "../list_special.h"
#include "../file_path_special.h"
#include "../record/record_driver.h"
#include "../tasks/task_content.h"
#include "../runloop.h"
#include "../midi_driver.h"
#include "../verbosity.h"

/* Largest number of int16 samples handed to audio_driver_flush() in one
 * call. The batch callback slices anything bigger into pieces of this
 * many samples, and the single-sample accumulator is the same size, so
 * both core callback styles reach the resampler in the same unit. */
#define AUDIO_CHUNK_SIZE_NONBLOCKING   2048

/* Capacity of the single-sample accumulator (audio_driver_state_t::
 * sample_accum) in int16 samples. Equal to the batch slice size: a core
 * emitting more than this per retro_run() gets its audio flushed in
 * slices exactly as a batch core delivering the same count would. */
#define AUDIO_SAMPLE_ACCUM_INT16S      AUDIO_CHUNK_SIZE_NONBLOCKING

/* Threaded pipeline: the consumer pulls at most this many int16 samples
 * out of pipe_ring per pass (one batch slice), and the producer stages
 * at most this many when it has to convert a float batch first. */
#define AUDIO_PIPE_SLICE_INT16S        AUDIO_CHUNK_SIZE_NONBLOCKING
/* The canonical wide frame on a multi-channel core's ring: a slot per
 * speaker position bit, FL and FR first. */
#define AUDIO_PIPE_CANON_CHANNELS      11

/* pipe_ring capacity in video frames of core audio. The ring is only
 * the hand-off between the frame-end publish and the device-paced
 * consumer: the consumer takes what each device period needs and no
 * more, so the ring holds about one frame in steady state and rate
 * control steers the device, exactly as the inline path does. Three
 * frames is slack for the producer's burst and the consumer's period
 * to slip past each other, not latency. */
#define AUDIO_PIPE_RING_FRAMES         3

/* Longest the producer waits for ring space before dropping, in
 * microseconds. With audio_sync on this wait is the frontend's audio
 * throttle, the same thing a blocking driver write was; the cap only
 * exists so a device that stops draining cannot hang the frontend. */
#define AUDIO_PIPE_WAIT_MAX_US         1000000
/* The producer's own bound is derived from the ring rather than flat:
 * this many of its playing times, floored so a tiny ring still leaves
 * a usable wait. See audio_driver_pipe_wait_us(). */
#define AUDIO_PIPE_WAIT_FILLS          4
#define AUDIO_PIPE_WAIT_MIN_US         20000

/* The policy floor on the latency setting, in milliseconds, applied
 * once here before any driver sees it. The setting's range starts at
 * zero, and zero reached the drivers: some floored it themselves, each
 * at its own value, one refused to open, one fell back to a fixed
 * fifo. A driver keeps only a floor of its hardware's own.
 *
 * Eight is the default and was for a long time the only value. It is a
 * setting now - audio_latency_floor - because the reasoning above is
 * about drivers mishandling zero rather than about hardware, and an
 * exclusive-mode driver that negotiates its period with the device can
 * go below it where the device allows. Never below one: zero is the
 * value that caused the trouble this floor exists for. */
#define AUDIO_LATENCY_MIN_MS           1

/* Region spacing inside the two scratch arenas, in elements: 64 bytes
 * for either type. Every region starts on a 64-byte boundary for the
 * SIMD conversion and resampler paths, and one extra 64-byte pad sits
 * between regions so consecutive regions are never a multiple of 4 KiB
 * apart. The buffer sizes are all 4096 * n, so without the pad the
 * resampler's input and output, or the s16 path's source and scratch,
 * would share their low 12 address bits and stall on load/store
 * aliasing. */
#define AUDIO_ARENA_ALIGN_INT16        32
#define AUDIO_ARENA_ALIGN_FLOAT        16

/* Advance an arena cursor past a region of n elements: round up to the
 * alignment, then leave one alignment unit of padding. */
#define AUDIO_ARENA_NEXT(cur, n, align) \
   ((((cur) + (n) + (align) - 1) / (align)) * (align) + (align))

#define AUDIO_MAX_RATIO                16
#define AUDIO_MIN_RATIO                0.0625

/* Frames the pause tail spans: the only place the driver invents audio. */
#define AUDIO_PAUSE_TAIL_FRAMES        512

/* Period the pause tail repeats, and the window it is matched over, in output
 * frames. 96..1024 is roughly 47Hz to 500Hz at 48kHz. */
#define AUDIO_PAUSE_TAIL_MIN_PERIOD    96
#define AUDIO_PAUSE_TAIL_MAX_PERIOD    1024
#define AUDIO_PAUSE_TAIL_CORR_FRAMES   128

/* Frames over which the tail's join to what was playing is eased in. */
#define AUDIO_PAUSE_TAIL_JOIN_FRAMES   128

/* Output frames dropped after the pause tail, to swallow whatever the
 * resampler was still holding. */
#define AUDIO_PAUSE_MUTE_FRAMES        512

/* Fastforward timing calculations running average samples. Helps with
 * a consistent pitch when fast-forwarding. */
#define AUDIO_FF_EXP_AVG_SAMPLES       16

/* Assumed content fps when av_info.timing.fps is unset or implausible
 * (cold start before core load, etc.). Used only to bootstrap the DRC
 * threshold; once a core is loaded and SET_SYSTEM_AV_INFO has fired,
 * audio_driver_update_drc_threshold uses the actual fps. */
#define AUDIO_DRC_FALLBACK_FPS         60.0

/* Floor on the DRC threshold (int16 stereo samples). Guards against
 * pathological input rates or fps values resulting in a near-zero
 * threshold that would fire on every call. ~10 ms at 48 kHz stereo. */
#define AUDIO_DRC_MIN_THRESHOLD_INT16S 1024

#define MENU_SOUND_FORMATS "ogg|mod|xm|s3m|mp3|flac|wav"

 /* Converts decibels to voltage gain. Returns voltage gain value. */
#define DB_TO_GAIN(db) (powf(10.0f, (db) / 20.0f))
/* The s16 mixer pipeline takes Q16.16 gains so that it needs no float
 * of its own. The frontend still keeps its gains as floats, so the
 * conversion happens here, at the boundary. */
#define GAIN_TO_Q16(g) ((int32_t)((g) * 65536.0f + 0.5f))

#if (defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(HAVE_NEON))
static bool clamp_float_neon_enabled = false;

static void audio_driver_clamp_init_simd(void)
{
   uint64_t cpu = cpu_features_get();
   if (cpu & RETRO_SIMD_NEON)
      clamp_float_neon_enabled = true;
}
#else
static void audio_driver_clamp_init_simd(void) { }
#endif

audio_driver_t audio_null = {
   NULL, /* init */
   NULL, /* write */
   NULL, /* stop */
   NULL, /* start */
   NULL, /* alive */
   NULL, /* set_nonblock_state */
   NULL, /* free */
   NULL, /* use_float */
   "null",
   NULL,
   NULL,
   NULL, /* write_avail */
   NULL, /* buffer_size */
   NULL  /* write_raw */
};

audio_driver_t *audio_drivers[] = {
#ifdef HAVE_ALSA
   &audio_alsa,
#endif
#ifdef HAVE_TINYALSA
   &audio_tinyalsa,
#endif
#if defined(HAVE_AUDIOIO)
   &audio_audioio,
#endif
#if defined(HAVE_OSS) || defined(HAVE_OSS_BSD)
   &audio_oss,
#endif
#ifdef HAVE_RSOUND
   &audio_rsound,
#endif
#ifdef HAVE_COREAUDIO
   &audio_coreaudio,
#endif
#ifdef HAVE_AL
   &audio_openal,
#endif
#ifdef HAVE_SL
   &audio_opensl,
#endif
#ifdef HAVE_ROAR
   &audio_roar,
#endif
#ifdef HAVE_JACK
   &audio_jack,
#endif
#ifdef HAVE_WASAPI
   &audio_wasapi,
#endif
#ifdef HAVE_ASIO
   &audio_asio,
#endif
#ifdef HAVE_WDMKS
   &audio_wdmks,
#endif
#ifdef HAVE_XAUDIO
   &audio_xa,
#endif
#ifdef HAVE_DSOUND
   &audio_dsound,
#endif
#ifdef HAVE_SDL
   &audio_sdl1,
#endif
#ifdef HAVE_SDL2
   &audio_sdl2,
#endif
#ifdef HAVE_SDL3
   &audio_sdl3,
#endif
#ifdef HAVE_PULSE
   &audio_pulse,
#endif
#ifdef HAVE_PIPEWIRE
   &audio_pipewire,
#endif
#if defined(__PSL1GHT__) || defined(__PS3__)
   &audio_ps3,
#endif
#ifdef XENON
   &audio_xenon360,
#endif
#ifdef GEKKO
   &audio_gx,
#endif
#ifdef WIIU
   &audio_ax,
#endif
#if defined(HAVE_RWEBAUDIO)
   &audio_rwebaudio,
#endif
#if defined(HAVE_AUDIOWORKLET)
   &audio_audioworklet,
#endif
#if defined(PSP)
  &audio_psp,
#endif
#if defined(VITA)
  &audio_psp2,
#endif
#if defined(ORBIS)
  &audio_ps4,
#endif
#if defined(PS2)
  &audio_ps2,
#endif
#ifdef _3DS
   &audio_ctr_csnd,
   &audio_ctr_dsp,
#endif
#ifdef SWITCH
   &audio_switch,
#ifdef HAVE_LIBNX
   &audio_switch_libnx_audren,
#endif
#endif
   &audio_null,
   NULL,
};

#ifdef HAVE_MICROPHONE
static microphone_driver_state_t mic_driver_st;

microphone_driver_t microphone_null = {
      NULL,
      NULL,
      NULL,
      "null",
      NULL,
      NULL,
      NULL,
      NULL
};

microphone_driver_t *microphone_drivers[] = {
#ifdef HAVE_ALSA
      &microphone_alsa,
#endif
#ifdef HAVE_WASAPI
      &microphone_wasapi,
#endif
#ifdef HAVE_SDL2
      &microphone_sdl, /* Microphones are not supported in SDL 1 */
#endif
#ifdef HAVE_SDL3
      &microphone_sdl3,
#endif
#ifdef HAVE_PIPEWIRE
      &microphone_pipewire,
#endif
#if defined(HAVE_COREAUDIO)
      &microphone_coreaudio,
#endif
      &microphone_null,
      NULL,
};
#endif

static audio_driver_state_t audio_driver_st = {0}; /* double alignment */

/* The setting's values, in menu order: what each opens. */
static uint32_t audio_driver_layout_of_setting(unsigned value)
{
   switch (value)
   {
      case 1:  return AUDIO_LAYOUT_QUAD;
      case 2:  return AUDIO_LAYOUT_5POINT1;
      case 3:  return AUDIO_LAYOUT_5POINT1_SURROUND;
      case 4:  return AUDIO_LAYOUT_7POINT1;
      default: break;
   }
   return AUDIO_LAYOUT_STEREO;
}

uint32_t audio_driver_requested_layout(void)
{
   settings_t *settings = config_get_ptr();
   return audio_driver_layout_of_setting(
         settings ? settings->uints.audio_output_layout : 0);
}

static INLINE size_t audio_driver_dev_frame_bytes(const audio_driver_state_t *audio_st)
{
   unsigned ch = audio_st->out_channels ? audio_st->out_channels : 2;
   return ch * ((AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_USE_FLOAT)
         ? sizeof(float) : sizeof(int16_t));
}


/**************************************/

audio_driver_state_t *audio_state_get_ptr(void)
{
   return &audio_driver_st;
}

/**
 * config_get_audio_driver_options:
 *
 * Get an enumerated list of all audio driver names, separated by '|'.
 *
 * Returns: string listing of all audio driver names, separated by '|'.
 **/
const char *config_get_audio_driver_options(void)
{
   return char_list_new_special(STRING_LIST_AUDIO_DRIVERS, NULL);
}

#ifdef HAVE_TRANSLATE
/* TODO/FIXME - Doesn't currently work.  Fix this. */
bool audio_driver_is_ai_service_speech_running(void)
{
#ifdef HAVE_AUDIOMIXER
   enum audio_mixer_state res = audio_driver_mixer_get_stream_state(10);
   if (!((res == AUDIO_STREAM_STATE_NONE) || (res == AUDIO_STREAM_STATE_STOPPED)))
      return true;
#endif
   return false;
}
#endif

/* The driver whose device_list_new to call. The devices listed are the
 * ones the Device setting will be applied to, and that is the
 * configured driver: when it is the one running, ask it, through the
 * wrapper if the threaded pipeline is on; when the user has picked a
 * different driver in the menu and nothing has reinitialised yet, ask
 * the configured driver with no context, which enumeration allows.
 * Asking the running driver in that case listed the old driver's
 * devices under the new driver's name until the next restart. */
static const audio_driver_t *audio_driver_enumeration_driver(
      audio_driver_state_t *audio_st, void **ctx)
{
   settings_t *settings        = config_get_ptr();
   const audio_driver_t *audio = audio_st->current_audio;
   const char *running_ident   = audio ? audio->ident : NULL;
   *ctx                        = audio_st->context_audio_data;
#ifdef HAVE_THREADS
   if (audio && string_is_equal(audio->ident, "audio-thread"))
   {
      const audio_driver_t *inner =
         audio_thread_wrapped_driver(audio_st->context_audio_data);
      if (inner)
         running_ident = inner->ident; /* the wrapper forwards, context intact */
      else
      {
         /* Wrapper selected but never started: no inner driver to ask.
          * Fall through to the configured one with no context. */
         audio         = NULL;
         running_ident = NULL;
      }
   }
#endif
   if (     audio
         && running_ident
         && !string_is_equal(running_ident, settings->arrays.audio_driver))
      audio = NULL;
   if (!audio)
   {
      int i = (int)driver_find_index("audio_driver",
            settings->arrays.audio_driver);
      if (i >= 0)
         audio = (const audio_driver_t*)audio_drivers[i];
      *ctx  = NULL;
   }
   return audio;
}

static bool audio_driver_free_devices_list(void)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   const audio_driver_t *audio    = audio_st->devices_list_driver;
   if (!audio_st->devices_list)
      return false;
   /* Released by the driver that built it. The context it was built
    * with may be gone by now; every driver's free takes the list alone
    * and tolerates a NULL context. */
   if (audio && audio->device_list_free)
      audio->device_list_free(NULL, audio_st->devices_list);
   else
      string_list_free(audio_st->devices_list);
   audio_st->devices_list        = NULL;
   audio_st->devices_list_driver = NULL;
   return true;
}

void audio_driver_refresh_devices_list(void)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   void *ctx                      = NULL;
   const audio_driver_t *audio;
   audio_driver_free_devices_list();
   audio = audio_driver_enumeration_driver(audio_st, &ctx);
   if (audio && audio->device_list_new)
   {
      audio_st->devices_list = (struct string_list*)
         audio->device_list_new(ctx);
      audio_st->devices_list_driver = audio;
#ifdef HAVE_THREADS
      /* Built through the wrapper: the wrapped driver allocated it,
       * and is the one to free it - the wrapper's own free needs the
       * context to find that driver, which the free below does not
       * carry. */
      if (string_is_equal(audio->ident, "audio-thread"))
      {
         const audio_driver_t *inner = audio_thread_wrapped_driver(ctx);
         if (inner)
            audio_st->devices_list_driver = inner;
      }
#endif
   }
}

#ifdef DEBUG
static void audio_driver_report_audio_buffer_statistics(void)
{
   audio_statistics_t audio_stats;
   audio_stats.samples                   = 0;
   audio_stats.average_buffer_saturation = 0.0f;
   audio_stats.std_deviation_percentage  = 0.0f;
   audio_stats.close_to_underrun         = 0.0f;
   audio_stats.close_to_blocking         = 0.0f;

   if (!audio_compute_buffer_statistics(&audio_stats))
      return;

   RARCH_LOG("[Audio] Average audio buffer saturation: %.2f %%,"
         " standard deviation (percentage points): %.2f %%.\n"
         "[Audio] Amount of time spent close to underrun: %.2f %%."
         " Close to blocking: %.2f %%.\n",
         audio_stats.average_buffer_saturation,
         audio_stats.std_deviation_percentage,
         audio_stats.close_to_underrun,
         audio_stats.close_to_blocking);
}
#endif

static void audio_driver_deinit_resampler(void)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   if (audio_st->resampler && audio_st->resampler_data)
      audio_st->resampler->free(audio_st->resampler_data);
   if (audio_st->resampler_data_int16 && audio_st->resampler_int16_free)
      audio_st->resampler_int16_free(audio_st->resampler_data_int16);
   audio_st->resampler          = NULL;
   audio_st->resampler_data     = NULL;
   audio_st->resampler_data_int16 = NULL;
   audio_st->resampler_int16_process = NULL;
   audio_st->resampler_int16_free    = NULL;
   audio_st->resampler_int16_reset   = NULL;
   audio_st->resampler_ident[0] = '\0';
   audio_st->resampler_quality  = RESAMPLER_QUALITY_DONTCARE;
   audio_st->resampler_hq       = false;
}

static bool audio_driver_resampler_realloc(audio_driver_state_t *audio_st,
      bool hq_oversampling)
{
   bool initialized = retro_resampler_realloc_hq(&audio_st->resampler_data,
         &audio_st->resampler, audio_st->resampler_ident,
         audio_st->resampler_quality, audio_st->src_ratio_orig, hq_oversampling);
   audio_st->resampler_hq = initialized && hq_oversampling
      && audio_st->src_ratio_orig >= 2.0
      && audio_st->resampler
      && (audio_st->resampler->caps & RESAMPLER_CAP_HQ_OVERSAMPLE);
   return initialized;
}

size_t audio_driver_get_underruns(void)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   if (     audio_st->current_audio && audio_st->current_audio->underruns
         && audio_st->context_audio_data)
      return audio_st->current_audio->underruns(audio_st->context_audio_data);
   return 0;
}

/* ---- the discrete extras ------------------------------------------ */

static void audio_driver_extra_free(audio_driver_state_t *audio_st)
{
   unsigned i;
   for (i = 0; i < audio_st->extra.nres; i++)
   {
      if (!audio_st->extra.res[i])
         continue;
      if (audio_st->extra.res_int16)
      {
         if (audio_st->resampler_int16_free)
            audio_st->resampler_int16_free(audio_st->extra.res[i]);
      }
      else if (audio_st->extra.res_drv)
         audio_st->extra.res_drv->free(audio_st->extra.res[i]);
      audio_st->extra.res[i] = NULL;
   }
   free(audio_st->extra.in_f);
   free(audio_st->extra.in_i);
   free(audio_st->extra.pair_in);
   free(audio_st->extra.pair_out);
   free(audio_st->extra.pair_in_i);
   free(audio_st->extra.pair_out_i);
   free(audio_st->extra.out_f);
   free(audio_st->extra.out_i);
   memset(&audio_st->extra, 0, sizeof(audio_st->extra));
}

/* An int16 resampler instance of the kind the front pair uses. */
static void *audio_driver_int16_resampler_new(audio_driver_state_t *audio_st)
{
   retro_resampler_int16_t rs;
   retro_resampler_int16_new(&rs,
         (audio_st->resampler && audio_st->resampler->short_ident)
               ? audio_st->resampler->short_ident : NULL,
         audio_st->resampler_quality, audio_st->src_ratio_orig,
         audio_st->resampler_hq);
   return rs.data;
}

/* Room for a batch of extras: the buffers, and resampler instances of
 * the kind the front pair's path uses. The instances are remade when
 * the kind changes. */
static bool audio_driver_extra_prepare(audio_driver_state_t *audio_st,
      unsigned channels, uint32_t positions, size_t frames, bool is_float, bool int16_path)
{
   unsigned nres = (channels + 1) / 2, i;
   size_t cap_out = frames * 4 + 1024;
   size_t front_cap = int16_path
      ? audio_st->output_samples_int16_length / (2 * sizeof(int16_t))
      : audio_st->output_samples_buf_length / (2 * sizeof(float));
   /* The front pair's ratio bound must also fit every extra pair. */
   if (cap_out < front_cap)
      cap_out = front_cap;
   if (!channels || channels != audio_st->extra.channels || positions != audio_st->extra.positions
         || int16_path != audio_st->extra.res_int16 || nres != audio_st->extra.nres)
   {
      audio_driver_extra_free(audio_st);
      if (!channels || channels > 2 * ARRAY_SIZE(audio_st->extra.res))
         return false;
      audio_st->extra.channels  = channels;
      audio_st->extra.positions = positions;
      audio_st->extra.nres      = nres;
      audio_st->extra.res_int16 = int16_path;
      for (i = 0; i < nres; i++)
      {
         if (int16_path)
            audio_st->extra.res[i] = audio_driver_int16_resampler_new(audio_st);
         else
         {
            const retro_resampler_t *drv = NULL;
            retro_resampler_realloc_hq(&audio_st->extra.res[i], &drv,
                  audio_st->resampler_ident, audio_st->resampler_quality,
                  audio_st->src_ratio_orig, audio_st->resampler_hq);
            audio_st->extra.res_drv = drv;
         }
         if (!audio_st->extra.res[i])
         {
            audio_driver_extra_free(audio_st);
            return false;
         }
      }
   }
   if (frames > audio_st->extra.cap_in)
   {
      size_t width = channels > 2 ? channels : 2;
      if (frames > ((size_t)-1 - 1024) / 4
            || frames > (size_t)-1 / width / sizeof(float)
            || cap_out > (size_t)-1 / width / sizeof(float))
      {
         audio_driver_extra_free(audio_st);
         return false;
      }
      free(audio_st->extra.in_f);
      free(audio_st->extra.in_i);
      free(audio_st->extra.pair_in);
      free(audio_st->extra.pair_in_i);
      audio_st->extra.in_f = (float*)malloc(frames * channels * sizeof(float));
      audio_st->extra.in_i = (int16_t*)malloc(frames * channels * sizeof(int16_t));
      if (channels != 2)
      {
         if (int16_path)
            audio_st->extra.pair_in_i = (int16_t*)malloc(frames * 2 * sizeof(int16_t));
         else
            audio_st->extra.pair_in = (float*)malloc(frames * 2 * sizeof(float));
      }
      audio_st->extra.cap_in = frames;
   }
   if (cap_out > audio_st->extra.cap_out)
   {
      size_t width = channels > 2 ? channels : 2;
      if (cap_out > (size_t)-1 / width / sizeof(float))
      {
         audio_driver_extra_free(audio_st);
         return false;
      }
      free(audio_st->extra.pair_out);
      free(audio_st->extra.pair_out_i);
      free(audio_st->extra.out_f);
      free(audio_st->extra.out_i);
      if (channels != 2)
      {
         if (int16_path)
            audio_st->extra.pair_out_i = (int16_t*)malloc(cap_out * 2 * sizeof(int16_t));
         else
            audio_st->extra.pair_out = (float*)malloc(cap_out * 2 * sizeof(float));
      }
      audio_st->extra.out_f      = (float*)malloc(cap_out * channels * sizeof(float));
      audio_st->extra.out_i      = (int16_t*)malloc(cap_out * channels * sizeof(int16_t));
      audio_st->extra.cap_out    = cap_out;
   }
   if (!audio_st->extra.in_f || !audio_st->extra.in_i
         || !audio_st->extra.out_f || !audio_st->extra.out_i
         || (channels != 2 && (int16_path
            ? (!audio_st->extra.pair_in_i || !audio_st->extra.pair_out_i)
            : (!audio_st->extra.pair_in || !audio_st->extra.pair_out))))
   {
      audio_driver_extra_free(audio_st);
      return false;
   }
   audio_st->extra.is_float  = is_float;
   audio_st->extra.in_frames = frames;
   return true;
}

/* Consumer-owned, or called after the wrapper has parked the consumer. */
static void audio_driver_reset_resamplers(audio_driver_state_t *audio_st)
{
   unsigned i;
   if (audio_st->resampler && audio_st->resampler->reset)
   {
      if (audio_st->resampler_data)
         audio_st->resampler->reset(audio_st->resampler_data);
      if (!audio_st->extra.res_int16)
         for (i = 0; i < audio_st->extra.nres; i++)
            if (audio_st->extra.res[i])
               audio_st->resampler->reset(audio_st->extra.res[i]);
   }
   if (audio_st->resampler_int16_reset)
   {
      if (audio_st->resampler_data_int16)
         audio_st->resampler_int16_reset(audio_st->resampler_data_int16);
      if (audio_st->extra.res_int16)
         for (i = 0; i < audio_st->extra.nres; i++)
            if (audio_st->extra.res[i])
               audio_st->resampler_int16_reset(audio_st->extra.res[i]);
   }
   audio_st->resampler_bypassed = false;
   audio_st->extra.bypassed = false;
   audio_st->extra.pending = false;
   audio_st->extra.out_frames = 0;
}

/* The extras through their resamplers, in pairs, at the front pair's
 * ratio on the front pair's input count, just after the fronts went
 * through: the same kind of resampler on the same count at the same
 * ratio produces the same number of frames, which is what keeps the
 * channels aligned. bypass: the fronts were copied at unity. */
static void audio_driver_extra_resample(audio_driver_state_t *audio_st,
      double ratio, size_t input_frames, bool bypass, bool int16_path)
{
   unsigned i, ch = audio_st->extra.channels;
   size_t f, out_frames = 0;
   if (!audio_st->extra.pending)
      return;
   if (input_frames > audio_st->extra.in_frames)
      input_frames = audio_st->extra.in_frames;
   /* Keep the extra histories fresh while the front pair bypasses SRC.
    *
    * Only where there is a front resampler to borrow the entry point
    * from. A bit-stream pass does not resample the front pair, so
    * audio_driver_init_internal() never made one - and this reached
    * through it for reset without asking, which is a null dereference
    * the moment a core with more than two channels is bitstreamed.
    *
    * The process() call further down is not exposed the same way: it
    * only runs when the front pair did resample, which is exactly
    * when a resampler exists. Bailing out of the whole function would
    * be wrong - the extras still have to be resampled on that path,
    * because they are what the encoder is handed. */
   if (bypass && !audio_st->extra.bypassed && !int16_path
         && audio_st->resampler && audio_st->resampler->reset)
      for (i = 0; i < audio_st->extra.nres; i++)
         audio_st->resampler->reset(audio_st->extra.res[i]);
   audio_st->extra.bypassed = bypass;
   if (bypass)
   {
      size_t n = input_frames < audio_st->extra.cap_out
         ? input_frames : audio_st->extra.cap_out;
      size_t samples = n * ch;
      if (int16_path)
      {
         if (audio_st->extra.is_float)
            convert_float_to_s16(audio_st->extra.out_i, audio_st->extra.in_f, samples);
         else
            memcpy(audio_st->extra.out_i, audio_st->extra.in_i, samples * sizeof(int16_t));
      }
      else
      {
         if (!audio_st->extra.is_float)
            convert_s16_to_float(audio_st->extra.out_f, audio_st->extra.in_i, samples, 1.0f);
         else
            memcpy(audio_st->extra.out_f, audio_st->extra.in_f, samples * sizeof(float));
      }
      audio_st->extra.out_frames = n;
      audio_st->extra.pending    = false;
      return;
   }
   /* the front path decided its format; the extras follow it */
   if (int16_path && audio_st->extra.is_float)
      convert_float_to_s16(audio_st->extra.in_i, audio_st->extra.in_f, input_frames * ch);
   else if (!int16_path && !audio_st->extra.is_float)
      convert_s16_to_float(audio_st->extra.in_f, audio_st->extra.in_i, input_frames * ch, 1.0f);
   /* One pair already has the resampler's interleaved stereo layout. */
   if (ch == 2)
   {
      if (int16_path)
      {
         struct resampler_data_int16 d;
         d.data_in       = audio_st->extra.in_i;
         d.data_out      = audio_st->extra.out_i;
         d.input_frames  = (unsigned)input_frames;
         d.output_frames = 0;
         d.ratio         = ratio;
         audio_st->resampler_int16_process(audio_st->extra.res[0], &d);
         out_frames = d.output_frames;
      }
      else
      {
         struct resampler_data d;
         d.data_in       = audio_st->extra.in_f;
         d.data_out      = audio_st->extra.out_f;
         d.input_frames  = input_frames;
         d.output_frames = 0;
         d.ratio         = ratio;
         audio_st->resampler->process(audio_st->extra.res[0], &d);
         out_frames = d.output_frames;
      }
      audio_st->extra.out_frames = out_frames < audio_st->extra.cap_out
         ? out_frames : audio_st->extra.cap_out;
      audio_st->extra.pending = false;
      return;
   }
   for (i = 0; i < audio_st->extra.nres; i++)
   {
      unsigned c0 = 2 * i, c1 = 2 * i + 1 < ch ? 2 * i + 1 : 2 * i;
      size_t n;
      if (int16_path)
      {
         struct resampler_data_int16 d;
         for (f = 0; f < input_frames; f++)
         {
            audio_st->extra.pair_in_i[2 * f]     = audio_st->extra.in_i[f * ch + c0];
            audio_st->extra.pair_in_i[2 * f + 1] = audio_st->extra.in_i[f * ch + c1];
         }
         d.data_in       = audio_st->extra.pair_in_i;
         d.data_out      = audio_st->extra.pair_out_i;
         d.input_frames  = (unsigned)input_frames;
         d.output_frames = 0;
         d.ratio         = ratio;
         audio_st->resampler_int16_process(audio_st->extra.res[i], &d);
         n = d.output_frames;
         if (n > audio_st->extra.cap_out) n = audio_st->extra.cap_out;
         for (f = 0; f < n; f++)
         {
            audio_st->extra.out_i[f * ch + c0] = audio_st->extra.pair_out_i[2 * f];
            if (c1 != c0)
               audio_st->extra.out_i[f * ch + c1] = audio_st->extra.pair_out_i[2 * f + 1];
         }
      }
      else
      {
         struct resampler_data d;
         for (f = 0; f < input_frames; f++)
         {
            audio_st->extra.pair_in[2 * f]     = audio_st->extra.in_f[f * ch + c0];
            audio_st->extra.pair_in[2 * f + 1] = audio_st->extra.in_f[f * ch + c1];
         }
         d.data_in       = audio_st->extra.pair_in;
         d.data_out      = audio_st->extra.pair_out;
         d.input_frames  = input_frames;
         d.output_frames = 0;
         d.ratio         = ratio;
         audio_st->resampler->process(audio_st->extra.res[i], &d);
         n = d.output_frames;
         if (n > audio_st->extra.cap_out) n = audio_st->extra.cap_out;
         for (f = 0; f < n; f++)
         {
            audio_st->extra.out_f[f * ch + c0] = audio_st->extra.pair_out[2 * f];
            if (c1 != c0)
               audio_st->extra.out_f[f * ch + c1] = audio_st->extra.pair_out[2 * f + 1];
         }
      }
      out_frames = n;
   }
   audio_st->extra.out_frames = out_frames;
   audio_st->extra.pending    = false;
}

struct audio_inline_transport
{
   audio_stretch_stream_t *stream;
   void *output, *source, *front;
   unsigned channels, layout;
   audio_speed_lpf_t lpf;
   uint32_t cutoff;
   bool floating, bypassed, source_progress;
};

static void audio_driver_inline_reset(audio_driver_state_t *audio_st)
{
   struct audio_inline_transport *t = audio_st->inline_transport;
   if (!t) return;
   audio_stretch_stream_reset(t->stream);
   audio_speed_lpf_reset(&t->lpf);
   t->bypassed = false;
}

static void audio_driver_inline_free(audio_driver_state_t *audio_st)
{
   struct audio_inline_transport *t = audio_st->inline_transport;
   if (!t) return;
   audio_stretch_stream_free(t->stream);
   memalign_free(t->output);
   free(t);
   audio_st->inline_transport = NULL;
}

#ifdef HAVE_THREADS
/* Whether ring position 'at' lies ahead of 'from': the wrap-safe test
 * the layout queue uses, since the ring holds at most one capacity. */
static INLINE bool audio_driver_pipe_ahead(const audio_driver_state_t *audio_st,
      size_t from, size_t at)
{
   size_t d = at - from;
   return d && d <= audio_st->pipe_ring.capacity;
}

/* The main thread's side of the resume mark: a new sequence, armed at
 * @at or not armed at all. The main thread is the mark's only writer. */
static void audio_driver_pipe_fade_publish(audio_driver_state_t *audio_st,
      bool armed, size_t at)
{
   unsigned seq = ((unsigned)retro_atomic_load_relaxed_int(
            &audio_st->pipe_fade_in_mark) >> 1) + 1;
   if (armed)
      retro_atomic_store_release_size(&audio_st->pipe_fade_in_at, at);
   retro_atomic_store_release_int(&audio_st->pipe_fade_in_mark,
         (int)((seq << 1) | (armed ? 1u : 0u)));
}

/* The consumer's side: the resume point a mark still asks for, and the
 * mark's sequence to record once it is acted on. The position is taken
 * between two reads of the mark that agree, so it is that mark's. */
static bool audio_driver_pipe_fade_pending(audio_driver_state_t *audio_st,
      size_t *at, unsigned *seq)
{
   for (;;)
   {
      unsigned mark = (unsigned)retro_atomic_load_acquire_int(
            &audio_st->pipe_fade_in_mark);
      if (!(mark & 1u) || (mark >> 1) == audio_st->pipe_fade_in_seen)
         return false;
      *at = retro_atomic_load_acquire_size(&audio_st->pipe_fade_in_at);
      if ((unsigned)retro_atomic_load_acquire_int(
               &audio_st->pipe_fade_in_mark) == mark)
      {
         *seq = mark >> 1;
         return true;
      }
   }
}

/* The ring's positions restart with it; so do the marks in them. Both
 * owners are parked. */
static void audio_driver_pipe_marks_clear(audio_driver_state_t *audio_st)
{
   audio_st->pipe_discard_seen =
         retro_atomic_load_acquire_int(&audio_st->pipe_discard_gen);
   audio_st->pipe_fade_in_seen =
         (unsigned)retro_atomic_load_acquire_int(
               &audio_st->pipe_fade_in_mark) >> 1;
   audio_st->pipe_arm_fade     = false;
}
#endif

static bool audio_driver_deinit_internal(bool audio_enable)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   const audio_driver_t *audio    = audio_st->current_audio;

   /* Drop the flags that observers gate on before the context and the
    * scratch buffers go away, so a flush that races this teardown bails
    * at its gate instead of dereferencing a half-torn state. Init
    * re-establishes both flags. */
   AUDIO_FLAGS_CLEAR(audio_st, AUDIO_FLAG_ACTIVE | AUDIO_FLAG_CONTROL);

   if (audio && audio->free)
   {
      /* The session's silence, said once, here: the device is being
       * torn down and nothing is disturbed. Never logged live - a
       * line at the minimum setting costs a fraction of its margin.
       * The overlay has the count meanwhile. */
      size_t n = audio_driver_get_underruns();
      if (n)
         RARCH_LOG("[Audio] Driver \"%s\": %u period%s of silence for want of audio this session.\n",
               audio_driver_get_ident(), (unsigned)n, n == 1 ? "" : "s");
      if (audio_st->context_audio_data)
         audio->free(audio_st->context_audio_data);
      audio_st->context_audio_data = NULL;
   }

   audio_st->pipe_pending = NULL;
   audio_st->pipe_pending_bytes = 0;
   audio_driver_inline_free(audio_st);
   audio_st->transport_lpf_only = false;

   /* The pause tail continues the waveform, and across a core swap or a
    * driver change there is no waveform to continue. */
   audio_st->last_out[0]              = 0.0f;
   audio_st->last_out[1]              = 0.0f;
   audio_st->fade_in_frames           = 0;
   audio_st->fade_in_pending          = false;
   audio_st->pause_mute_frames        = 0;
   audio_st->pause_hist_pos           = 0;
   audio_st->pause_hist_fill          = 0;
   audio_st->resume_topup_pending     = false;
   retro_atomic_store_release_int(&audio_st->core_silenced, 0);

#ifdef HAVE_THREADS
   audio_pipeline_stretch_free(audio_st->pipe_transport);
   audio_st->pipe_transport = NULL;
   audio_pipeline_stretch_free(audio_st->pipe_transport_suspended);
   audio_st->pipe_transport_suspended = NULL;
   if (audio_st->pipe_transport_output)
      memalign_free(audio_st->pipe_transport_output);
   audio_st->pipe_transport_output = NULL;
   audio_st->pipe_transport_serial = audio_st->pipe_transport_search = 0;
   audio_st->pipe_transport_rate = 0;
   audio_st->pipe_transport_follow = 0;
#endif

   /* All scratch buffers live in the two arenas; the named pointers are
    * views into them. */
   if (audio_st->arena_int16)
      memalign_free(audio_st->arena_int16);
   audio_st->arena_int16              = NULL;
   audio_st->output_samples_int16     = NULL;
   audio_st->sample_accum             = NULL;
   audio_st->pipe_scratch             = NULL;
   audio_st->pipe_conv                = NULL;
   if (audio_st->pipe_arena)
      memalign_free(audio_st->pipe_arena);
   audio_st->pipe_arena               = NULL;
   free(audio_st->pipe_wide);
   audio_st->pipe_wide                = NULL;
   audio_st->pipe_wide_bytes          = 0;
   audio_st->pipe_channels            = 2;
   free(audio_st->multi_fold);
   audio_st->multi_fold               = NULL;
   audio_st->multi_fold_frames        = 0;
   free(audio_st->record_remap);
   audio_st->record_remap             = NULL;
   audio_st->record_remap_frames      = 0;
   audio_st->core_layout              = AUDIO_LAYOUT_STEREO;
   audio_driver_extra_free(audio_st);
   /* The wrapper thread was joined by audio->free() above, so nothing
    * reads the ring any more. */
   retro_spsc_free(&audio_st->pipe_ring);
   AUDIO_FLAGS_CLEAR(audio_st, AUDIO_FLAG_PIPELINE_THREADED
                             | AUDIO_FLAG_STARTED);
#ifdef HAVE_THREADS
   audio_st->pipe_park_ready          = false;
   retro_eventcount_free(&audio_st->pipe_space);
   retro_eventcount_free(&audio_st->pipe_data);
   /* Last: everything above may have taken it, and audio->free() at
    * the top of this function joined the thread that contends for it.
    * Nothing that runs after this point may touch the mixer or the
    * DSP filter from another thread - see audio_driver_state_lock(). */
   if (audio_st->state_lock)
      slock_free(audio_st->state_lock);
   audio_st->state_lock               = NULL;
#endif
   audio_st->pipe_threaded            = false;
   audio_st->input_data_int16         = NULL;
   audio_st->data_ptr                 = 0;
#ifdef HAVE_REWIND
   audio_st->rewind_buf               = NULL;
   audio_st->rewind_buf_f             = NULL;
   audio_st->rewind_size              = 0;
   audio_st->rewind_ptr               = 0;
#endif

   if (audio_st->arena_float)
      memalign_free(audio_st->arena_float);
   audio_st->arena_float              = NULL;
   free(audio_st->upmix_buf);
   free(audio_st->upmix_i16);
   free(audio_st->virt_buf);
   audio_st->upmix_buf                = NULL;
   audio_st->upmix_i16                = NULL;
   audio_st->virt_buf                 = NULL;
   audio_st->virtualize               = false;
   audio_st->upmix_frames             = 0;
   audio_st->out_layout               = AUDIO_LAYOUT_STEREO;
   audio_st->out_channels             = 2;
   audio_st->input_data               = NULL;
   audio_st->synth_buf                = NULL;
   audio_st->output_samples_buf       = NULL;

   if (!audio_enable)
   {
      AUDIO_FLAGS_CLEAR(audio_st, AUDIO_FLAG_ACTIVE);
      return false;
   }

   audio_driver_deinit_resampler();

#ifdef HAVE_DSP_FILTER
   audio_driver_dsp_filter_free();
#endif
#ifdef DEBUG
   audio_driver_report_audio_buffer_statistics();
#endif

   return true;
}

#ifdef HAVE_AUDIOMIXER
static void audio_driver_mixer_deinit(void)
{
   unsigned i;

   AUDIO_FLAGS_CLEAR(&audio_driver_st, AUDIO_FLAG_MIXER_ACTIVE
                                      | AUDIO_FLAG_MIXER_INITED);

   for (i = 0; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
   {
      audio_driver_mixer_stop_stream(i);
      audio_driver_mixer_remove_stream(i);
   }

   audio_mixer_done();
}
#endif

bool audio_driver_deinit(void)
{
#ifdef HAVE_AUDIOMIXER
   audio_driver_mixer_deinit();
#endif
   audio_driver_free_devices_list();
   return audio_driver_deinit_internal(config_get_ptr()->bools.audio_enable);
}

bool audio_driver_find_driver(const char *audio_drv,
      const char *prefix, bool verbosity_enabled)
{
   int i;

#ifdef HAVE_ALSA
   /* "alsathread" was a second ALSA playback driver, removed once the
    * threaded pipeline covered what it did; a config that still names it
    * means ALSA, not the first entry in the table. Aliased for a
    * release, then to go. The microphone driver of the same name is
    * unaffected - it is a separate list. */
   if (string_is_equal(audio_drv, "alsathread"))
      audio_drv = "alsa";
   /* The menu label and its help text stay in msg_hash: fifteen packed
    * translation headers carry the entry with byte counts and a
    * contiguity check, and rewriting those to delete a string nothing
    * can now reach is a larger and riskier change than removing the
    * driver was. The driver is not in the list, so the label is never
    * shown and the help lookup never matches. */
#endif
#ifdef HAVE_COREAUDIO
   /* "coreaudio3" was a second Apple driver, merged into "coreaudio";
    * a config that still names it means the driver, not the first
    * entry in the table. Aliased for a release, then to go. */
   if (string_is_equal(audio_drv, "coreaudio3"))
      audio_drv = "coreaudio";
#endif

   i = (int)driver_find_index("audio_driver", audio_drv);

   if (i >= 0)
      audio_driver_st.current_audio = (const audio_driver_t*)
         audio_drivers[i];
   else
   {
      const audio_driver_t *tmp = NULL;
      if (verbosity_enabled)
      {
         unsigned d;
         RARCH_ERR("Couldn't find any %s named \"%s\"\n", prefix, audio_drv);
         RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
         for (d = 0; audio_drivers[d]; d++)
         {
            if (audio_drivers[d])
               RARCH_LOG_OUTPUT("\t%s\n", audio_drivers[d]->ident);
         }
         RARCH_WARN("Going to default to first %s...\n", prefix);
      }

      tmp = (const audio_driver_t*)audio_drivers[0];

      if (!tmp)
         return false;
      audio_driver_st.current_audio = tmp;
   }

   return true;
}

/**
 * Writes audio samples to audio driver's output.
 * Will first perform DSP processing (if enabled) and resampling.
 *
 * @param audio_st The overall state of the audio driver.
 * @param slowmotion_ratio The factor by which slow motion extends the core's runtime
 * (e.g. a value of 2 means the core is running at half speed).
 * @param data Audio output data that was most recently provided by the core.
 * @param samples The size of \c data, in samples.
 * @param is_slowmotion True if the core is currently running in slow motion.
 * @param is_fastmotion True if the core is currently running in fast-forward.
 **/
/**
 * audio_driver_compute_rate_adjust:
 *
 * Compute the rate-control adjustment factor used to nudge the
 * resampler ratio for A/V sync, by sampling the audio driver's
 * FIFO write-available count and comparing it to the half-full
 * mark. Records the sampled value in the rolling history at
 * free_samples_buf[] for diagnostics, and ticks free_samples_count.
 *
 * Caller is responsible for checking that AUDIO_FLAG_CONTROL is set
 * before calling this, and for sample-count rate-limiting (we don't
 * want to call audio->write_avail more often than the DRC time
 * constant warrants - see audio_driver_state_t::drc_threshold_int16s).
 *
 * Used by both the write_raw fast path (which applies the result
 * as rate_adjust) and the resampler slow path (which multiplies
 * src_ratio_orig by the result to produce src_ratio_curr). The two
 * sites previously duplicated this body verbatim.
 *
 * Returns the rate-adjust factor (typically very near 1.0, within
 * rate_control_delta) and also stores it in audio_st->cached_rate_adjust
 * so callers that skip the recompute can read the most recent value
 * without re-running the DRC.
 **/
/* The bias as the resampling thread reads it; see sink_bias_q. */
static INLINE double audio_driver_sink_bias(audio_driver_state_t *audio_st)
{
   return 1.0 + (double)retro_atomic_load_acquire_int(&audio_st->sink_bias_q) / 1e8;
}

static double audio_driver_compute_rate_adjust(audio_driver_state_t *audio_st)
{
   unsigned write_idx;
   int avail;
   int half_size;
   int delta_mid;
   double direction;
   double effective_delta;
   double rate_adjust;

   /* The device context can go away under us mid-frame (deinit nulls it
    * before the observer gates clear) - degrade to no rate control for
    * this batch instead of calling write_avail(NULL). The writability
    * helper above already guards its write_avail() call the same way. */
   if (!audio_st->context_audio_data)
   {
      audio_st->cached_rate_adjust = 1.0;
      audio_st->samples_since_drc  = 0;
      return 1.0;
   }

   write_idx              =
         audio_st->free_samples_count++ & (AUDIO_BUFFER_FREE_SAMPLES_COUNT - 1);
   /* On the threaded pipeline the producer sampled the fill; see
    * pipe_ctrl_avail. Until it has, the device's own. */
   avail                  = audio_st->pipe_threaded
         ? retro_atomic_load_acquire_int(&audio_st->pipe_ctrl_avail) : -1;
   if (avail < 0)
      avail               = (int)audio_st->current_audio->write_avail(
            audio_st->context_audio_data);
   /* Never above the buffer: a driver that counts a stage in
    * write_avail() it left out of buffer_size() would otherwise push
    * the direction term past +1, and the controller with it. The
    * contract forbids it; the clamp is for the driver that has not
    * caught up with the contract. */
   if (avail > (int)audio_st->buffer_size)
      avail               = (int)audio_st->buffer_size;
   half_size              = (int)(audio_st->buffer_size / 2);
   /* half_size is the setpoint's denominator. A driver may report a zero
    * buffer size at runtime as well as at init - pulse pushes its size
    * through audio_driver_set_buffer_size() from write_avail() on every
    * sample, and notes there that it "can change spuriously" - so guard
    * the division here too rather than relying only on the init-time
    * gate. Returning 1.0 leaves the ratio untouched, which degrades to
    * "no rate control" instead of feeding inf/NaN into src_ratio_curr. */
   if (half_size <= 0)
   {
      audio_st->free_samples_buf[write_idx] = avail;
      audio_st->cached_rate_adjust          = 1.0;
      audio_st->samples_since_drc           = 0;
      return 1.0;
   }

   delta_mid              = avail - half_size;
   direction              = (double)delta_mid / half_size;
   /* Scale rate_control_delta inversely with the resampling ratio
    * so the effective loop gain stays constant regardless of output
    * sample rate (e.g. 96 kHz+).  Without this, high ratios amplify
    * the correction and the controller oscillates, causing audible
    * volume pumping.
    *
    * Only scale down (ratio > 1.0).  At sub-unity ratios the original
    * delta is already well-tuned and dividing by a fraction would
    * over-amplify corrections. */
   effective_delta        = (audio_st->src_ratio_orig > 1.0)
         ? audio_st->rate_control_delta / audio_st->src_ratio_orig
         : audio_st->rate_control_delta;
   rate_adjust            = 1.0 + effective_delta * direction;

   /* The sink estimate sees rate control's own corrections, so that a
    * constant one migrates into the bias and the fill re-centres. */
   rate_adjust *= audio_driver_sink_bias(audio_st);

   audio_st->free_samples_buf[write_idx] = avail;
   audio_st->cached_rate_adjust          = rate_adjust;
   audio_st->samples_since_drc           = 0;
   return rate_adjust;
}

/* The sink rate estimate.
 *
 * Two counts against the host clock: frames the device consumed, and
 * frames the frontend offered with the resampling ratio divided out,
 * so that sum is what the source produced at the nominal rate. Their
 * ratio over enough time is the two clocks' offset - a crystal is off
 * by tens of parts per million, a bad one by a few hundred - and is
 * applied as a bias on resampling: the slow, integral term beside rate
 * control's fast, proportional one, and the only correction with rate
 * control off.
 *
 * The counts are read in windows of AUDIO_SINK_WINDOW_USEC. A window
 * measures the clocks only if both sides were at rate over it: the
 * device within two percent (a pause, a stall, a driver that stopped
 * consuming), the source within the band a bias could correct (a core
 * warming up after load, or the main thread held for some tens of
 * milliseconds, is a source a percent slow with the device at rate),
 * and nothing dropped before it could be offered. Other windows are
 * left out. The kept windows are summed for the whole session, so a
 * device that counts in whole periods - 480 frames on a shared WASAPI
 * engine, 2500 ppm of noise in one window - averages out; the sums
 * open on the second kept window in a row, so a slow start is not in
 * them. Every AUDIO_SINK_BASELINE_USEC of summed time the bias is set
 * from the ratio; a ratio past AUDIO_SINK_BIAS_PLAUSIBLE - five times
 * the worst plausible crystal - is not a clock and is refused, said once
 * with both sides so the line explains which one moved. With a
 * blocking writer the source follows the device and no bias applies.
 *
 * The windows close on the thread that counts the source: after each
 * write on the frame-synchronous path, after each publish on the
 * threaded pipeline, where the count is what entered the ring - whole
 * publishes, less the change in what the ring holds - what it holds
 * is offered but still ahead of the device - and with nothing it
 * refused in the measure. Closed on the consumer instead, a window
 * held a fraction of a burst either way, thousands of parts per
 * million of phase noise against a band of five hundred. */
#define AUDIO_SINK_BIAS_PLAUSIBLE   0.0005
#define AUDIO_SINK_DEVICE_BAND      0.02
/* A window's own source band - a few milliseconds of the closing
 * publish's lateness in a window of seconds - and the windows before
 * the current one over which the band a bias could correct is judged. */
#ifndef AUDIO_SINK_WINDOW_BAND
#define AUDIO_SINK_WINDOW_BAND      0.002
#endif
#define AUDIO_SINK_BAND_WINDOWS     4
/* Overridable so a harness running in real time can run them short. */
#ifndef AUDIO_SINK_BASELINE_USEC
#define AUDIO_SINK_BASELINE_USEC    30000000
#endif
#ifndef AUDIO_SINK_WINDOW_USEC
#define AUDIO_SINK_WINDOW_USEC      4000000
#endif

/* Said once each. IMPLAUSIBLE, TOO_SLOW and UNSETTLED are the
 * estimator's and live in sink_warned; DROPPED is the flush's and
 * lives in sink_warned_flush. The two run on different threads; see
 * the note at their declaration. */
enum
{
   AUDIO_SINK_WARNED_IMPLAUSIBLE = 1 << 0,
   AUDIO_SINK_WARNED_DROPPED     = 1 << 1,
   AUDIO_SINK_WARNED_TOO_SLOW    = 1 << 2,
   AUDIO_SINK_WARNED_UNSETTLED   = 1 << 3
};

/* What the pipe ring holds, in nominal device frames; 0 off the
 * threaded pipeline. Read on the producer's side of the ring. */
static double audio_driver_sink_pipe_frames(audio_driver_state_t *audio_st)
{
#ifdef HAVE_THREADS
   if (audio_st->pipe_threaded && audio_st->pipe_frame_bytes)
      return (double)retro_spsc_read_avail(&audio_st->pipe_ring)
            / audio_st->pipe_frame_bytes * audio_st->src_ratio_orig;
#endif
   return 0.0;
}

/* What the device's buffer holds, in device frames: its size less its
 * free space; 0 where the driver reports neither. */
static double audio_driver_sink_device_frames(audio_driver_state_t *audio_st)
{
   const audio_driver_t *audio = audio_st->current_audio;
   if (audio && audio->write_avail && audio_st->buffer_size && audio_st->context_audio_data)
   {
      size_t frame_bytes = audio_driver_dev_frame_bytes(audio_st);
      size_t avail       = audio->write_avail(audio_st->context_audio_data);
      if (avail > audio_st->buffer_size)
         avail = audio_st->buffer_size;
      return (double)(audio_st->buffer_size - avail) / (double)frame_bytes;
   }
   return 0.0;
}

static void audio_driver_sink_mark(audio_driver_state_t *audio_st,
      audio_sink_mark_t *m, uint64_t consumed, uint64_t consumed_alt)
{
   m->offered      = audio_st->sink_offered;
   m->consumed     = consumed;
   m->consumed_alt = consumed_alt;
   m->pipe         = audio_driver_sink_pipe_frames(audio_st);
   m->device       = audio_driver_sink_device_frames(audio_st);
}

static INLINE double audio_driver_sink_ppm(double count, double nominal)
{
   return (count / nominal - 1.0) * 1e6;
}

static void audio_driver_sink_log_refused(audio_driver_state_t *audio_st,
      unsigned rate, double dev_ppm, double src_ppm, double ratio_ppm, unsigned flag)
{
   const char *why;
   double band = AUDIO_SINK_BIAS_PLAUSIBLE * 1e6;
   if (audio_st->sink_warned & flag)
      return;
   audio_st->sink_warned |= flag;
   if (fabs(dev_ppm) <= band && fabs(src_ppm) > band)
      why = "That is the source's clock, not the device's: with audio sync off the core is paced by the frame timer or the display, and rate control absorbs the difference. A bias is for crystals";
   else if (fabs(src_ppm) <= band && fabs(dev_ppm) > band)
      why = "That is too far off for a crystal, so the driver is most likely not counting device time";
   else
      why = "That is too far apart to be two crystals, so the driver and the frontend are most likely not counting the same thing";
   RARCH_WARN("[Audio] Sink rate: the device takes %+.0f ppm of %u and the source produces %+.0f ppm, a ratio of %+.0f ppm. %s. Not biasing resampling (driver \"%s\"); the rates are still shown.\n",
         dev_ppm, rate, src_ppm, ratio_ppm, why, audio_driver_get_ident());
}

/* Closes the window that has just ended. */
static void audio_driver_sink_window(audio_driver_state_t *audio_st,
      int64_t now_usec, uint64_t consumed, uint64_t consumed_alt,
      unsigned rate)
{
   audio_sink_mark_t *at = &audio_st->sink_at_window;
   int64_t  wdt      = now_usec - (audio_st->sink_window_at - AUDIO_SINK_WINDOW_USEC);
   double   nominal  = (double)rate * (double)wdt / 1e6;
   /* The source's count is what was offered less what the pipe and
    * the device's buffer took in over the window: what they hold now
    * was offered but is not yet consumed, and what they gave up was
    * offered earlier. Otherwise the pipe primed to its target and the
    * device filled from empty read as a source slow by that much for
    * the whole baseline - a 64 ms buffer, 2000 ppm over thirty
    * seconds, refused. */
   double   pipe_now = audio_driver_sink_pipe_frames(audio_st);
   double   dev_now  = audio_driver_sink_device_frames(audio_st);
   double   offered  = audio_st->sink_offered - at->offered
         - (pipe_now - at->pipe) - (dev_now - at->device);
   double   taken    = consumed >= at->consumed ? (double)(consumed - at->consumed) : 0.0;
   bool     kept, device_ok;

   /* What the driver's own approximation would have said over the
    * same window, where it has one. Nothing is decided from it; it is
    * reported, so that what the approximation costs on real hardware
    * is a number someone can read rather than an argument. */
   if (consumed_alt || at->consumed_alt)
   {
      double taken_alt = consumed_alt >= at->consumed_alt
            ? (double)(consumed_alt - at->consumed_alt) : 0.0;
      if (taken > 0.0)
         audio_st->sink_alt_ppm = (taken_alt / taken - 1.0) * 1e6;
   }

   audio_st->sink_window_at = now_usec + AUDIO_SINK_WINDOW_USEC;
   audio_driver_sink_mark(audio_st, at, consumed, consumed_alt);
   if (nominal <= 0.0)
      return;

   /* The source's band, in two parts. The window on its own must be
    * within AUDIO_SINK_WINDOW_BAND: a stall of tens of milliseconds
    * is a percent or more of a window and is out, while the lateness
    * of the closing publish - a few milliseconds of the core's run
    * time, which the next window carries back - is not. Windows that
    * pass are then judged together with the last few: the sum must be
    * within the band a bias could correct, the lateness having
    * cancelled over them. */
   device_ok = fabs(taken / nominal - 1.0) <= AUDIO_SINK_DEVICE_BAND;
   kept      = device_ok && fabs(offered / nominal - 1.0) <= AUDIO_SINK_WINDOW_BAND;
   if (kept)
   {
      /* The last few windows within their own bands, kept or not by
       * the sum test, tile the time before this one: a publish a
       * stall pushed across a window boundary is short in one window
       * and long in the next, and only the pair sums to the clock. A
       * window outside its own band - a pause, a dry spell - is not
       * in the ring: it would fail every sum for the next few windows
       * whatever they measured. */
      int64_t  span = wdt;
      double   sum  = offered;
      double   took = taken;
      unsigned i;
      for (i = 0; i < AUDIO_SINK_BAND_WINDOWS; i++)
      {
         span += audio_st->sink_recent_usec[i];
         sum  += audio_st->sink_recent_offered[i];
         took += audio_st->sink_recent_consumed[i];
      }
      audio_st->sink_recent_usec[audio_st->sink_recent_head]     = wdt;
      audio_st->sink_recent_offered[audio_st->sink_recent_head]  = offered;
      audio_st->sink_recent_consumed[audio_st->sink_recent_head] = taken;
      audio_st->sink_recent_head = (audio_st->sink_recent_head + 1) % AUDIO_SINK_BAND_WINDOWS;
      /* The source within the band a bias could correct, and the
       * ratio itself within it: the ratio carries rate control's
       * adjustment, which is the point - a steady one migrates into
       * the bias - but a window in which rate control sat at its
       * bound, filling a buffer or a pipe, is not a clock. */
      if (kept)
         kept = fabs(sum / ((double)rate * (double)span / 1e6) - 1.0) <= AUDIO_SINK_BIAS_PLAUSIBLE
             && sum > 0.0
             && fabs(took / sum - 1.0) <= AUDIO_SINK_BIAS_PLAUSIBLE;
   }
   else if (!device_ok)
   {
      /* A dry spell or a stalled device: the time before it cannot be
       * bridged, and its own window is no measure of either clock. */
      memset(audio_st->sink_recent_usec,     0, sizeof(audio_st->sink_recent_usec));
      memset(audio_st->sink_recent_offered,  0, sizeof(audio_st->sink_recent_offered));
      memset(audio_st->sink_recent_consumed, 0, sizeof(audio_st->sink_recent_consumed));
      audio_st->sink_recent_head = 0;
   }

   if (kept)
   {
      audio_st->sink_discarded = 0;
      if (audio_st->sink_settled < 2)
         audio_st->sink_settled++;
      /* The windows left out since the last kept one come in with
       * this one when, taken together with it, they read as the clock
       * - a publish a stall pushed across a boundary is short in one
       * window and long in the next, and only the pair sums to the
       * rate; leaving out the short half alone biased the sums by the
       * shift. A pause, a stall the source did not make up, or a slow
       * start read as a slow source over the pending span, and stay
       * out: the device steady through them is not enough, the
       * source has to have been too. */
      audio_st->sink_kept.usec     += wdt;
      audio_st->sink_kept.offered  += offered;
      audio_st->sink_kept.consumed += taken;
      if (!audio_st->sink_pending_broken && audio_st->sink_pending.usec > 0)
      {
         double span = (double)(audio_st->sink_pending.usec + wdt);
         double sum  = audio_st->sink_pending.offered + offered;
         double took = audio_st->sink_pending.consumed + taken;
         if (   fabs(sum / ((double)rate * span / 1e6) - 1.0) <= AUDIO_SINK_BIAS_PLAUSIBLE
             && sum > 0.0
             && fabs(took / sum - 1.0) <= AUDIO_SINK_BIAS_PLAUSIBLE)
         {
            audio_st->sink_kept.usec     += audio_st->sink_pending.usec;
            audio_st->sink_kept.offered  += audio_st->sink_pending.offered;
            audio_st->sink_kept.consumed += audio_st->sink_pending.consumed;
         }
      }
      memset(&audio_st->sink_pending, 0, sizeof(audio_st->sink_pending));
      audio_st->sink_pending_broken = false;
   }
   else
   {
      audio_st->sink_discarded++;
      /* Before the sums stand, a left-out window restarts them: a slow
       * start is not to be in them. After, it is just left out. */
      if (audio_st->sink_settled < 2)
      {
         audio_st->sink_settled = 0;
         memset(&audio_st->sink_kept, 0, sizeof(audio_st->sink_kept));
      }
      if (device_ok)
      {
         audio_st->sink_pending.usec     += wdt;
         audio_st->sink_pending.offered  += offered;
         audio_st->sink_pending.consumed += taken;
      }
      else
      {
         /* Nothing before a dry spell is carried past it, and nothing
          * after it merges into the sums: the pending sum from here
          * is for the rates shown only, until the next kept window
          * drops it. */
         audio_st->sink_pending.usec     = wdt;
         audio_st->sink_pending.offered  = offered;
         audio_st->sink_pending.consumed = taken;
         audio_st->sink_pending_broken   = true;
      }
   }

   /* The rates shown: the sums where they stand, the windows left out
    * meanwhile where they do not, so the overlay has them from the
    * first window and shows a source off the band while it is. */
   {
      const audio_sink_sum_t *sum = audio_st->sink_kept.usec > 0
            ? &audio_st->sink_kept : &audio_st->sink_pending;
      if (sum->usec > 0)
      {
         audio_st->sink_rate_hz   = sum->consumed * 1e6 / (double)sum->usec;
         audio_st->sink_source_hz = sum->offered  * 1e6 / (double)sum->usec;
      }
   }

   /* A source off the band for a baseline's worth with nothing summed
    * is said so once: with audio sync off that is its clock. */
   if (     audio_st->sink_kept.usec == 0
         && audio_st->sink_pending.usec >= AUDIO_SINK_BASELINE_USEC)
   {
      double dev_ppm = audio_driver_sink_ppm(audio_st->sink_rate_hz, (double)rate);
      audio_driver_sink_log_refused(audio_st, rate, dev_ppm,
            audio_driver_sink_ppm(audio_st->sink_source_hz, (double)rate),
            audio_driver_sink_ppm(audio_st->sink_rate_hz, audio_st->sink_source_hz),
            fabs(dev_ppm) > AUDIO_SINK_BIAS_PLAUSIBLE * 1e6
               ? AUDIO_SINK_WARNED_IMPLAUSIBLE : AUDIO_SINK_WARNED_UNSETTLED);
   }
}

/* Sets the bias from the sums, every baseline's worth of summed time. */
static void audio_driver_sink_apply(audio_driver_state_t *audio_st,
      int64_t now_usec, unsigned rate)
{
   double r;

   if (     audio_st->sink_kept.usec < AUDIO_SINK_BASELINE_USEC
         || now_usec < audio_st->sink_apply_at)
      return;
   audio_st->sink_apply_at = now_usec + AUDIO_SINK_BASELINE_USEC;

   if (!(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_NONBLOCK))
   {
      if (!audio_st->sink_applied++)
         RARCH_LOG("[Audio] Sink rate: the device takes %.1f Hz against the host clock (%+.0f ppm of %u); the writer blocks, so the source follows the device and resampling is not biased.\n",
               audio_st->sink_rate_hz,
               audio_driver_sink_ppm(audio_st->sink_rate_hz, (double)rate), rate);
      return;
   }

   r = audio_st->sink_kept.offered > 0.0
         ? audio_st->sink_kept.consumed / audio_st->sink_kept.offered
         : audio_st->sink_bias;
   if (fabs(r - 1.0) > AUDIO_SINK_BIAS_PLAUSIBLE)
   {
      audio_driver_sink_log_refused(audio_st, rate,
            audio_driver_sink_ppm(audio_st->sink_rate_hz, (double)rate),
            audio_driver_sink_ppm(audio_st->sink_source_hz, (double)rate),
            (r - 1.0) * 1e6, AUDIO_SINK_WARNED_IMPLAUSIBLE);
      return;
   }
   /* A correction every thirty seconds holds only a buffer that lasts
    * thirty seconds of the mismatch it corrects. */
   if (     !(audio_st->sink_warned & AUDIO_SINK_WARNED_TOO_SLOW)
         && audio_st->buffer_size > 0 && fabs(r - 1.0) > 0.0)
   {
      size_t frame_bytes = audio_driver_dev_frame_bytes(audio_st);
      double buffer_sec  = (double)audio_st->buffer_size / (double)frame_bytes / (double)rate;
      double drain_sec   = (buffer_sec * 0.5) / fabs(r - 1.0);
      if (drain_sec < (double)AUDIO_SINK_BASELINE_USEC / 1e6)
      {
         audio_st->sink_warned |= AUDIO_SINK_WARNED_TOO_SLOW;
         RARCH_WARN("[Audio] Sink rate: the device and the source differ by %+.0f ppm, which empties half of this driver's %.1f ms buffer in %.0f s - sooner than the %.0f s it takes to measure and apply a correction. The correction cannot hold a buffer this small: turn on Audio Rate Control, which works every frame, or raise Audio Latency.\n",
               (r - 1.0) * 1e6, buffer_sec * 1000.0, drain_sec,
               (double)AUDIO_SINK_BASELINE_USEC / 1e6);
      }
   }

   audio_st->sink_bias = r;
   retro_atomic_store_release_int(&audio_st->sink_bias_q, (int)((r - 1.0) * 1e8));
   audio_st->sink_applied++;
   /* The first application is logged; the rest are on the overlay. A
    * line every thirty seconds of play is a stall the loop does not
    * need. */
   if (audio_st->sink_applied == 1)
      RARCH_LOG("[Audio] Sink rate: the device takes %.1f Hz against the host clock (%+.0f ppm of %u) and the source produces %.1f Hz (%+.0f ppm) over %.0f s summed, %.0f s in; resampling biased by %+.0f ppm.\n",
         audio_st->sink_rate_hz, audio_driver_sink_ppm(audio_st->sink_rate_hz, (double)rate), rate,
         audio_st->sink_source_hz, audio_driver_sink_ppm(audio_st->sink_source_hz, (double)rate),
         (double)audio_st->sink_kept.usec / 1e6,
         (double)(now_usec - audio_st->sink_started) / 1e6,
         (audio_st->sink_bias - 1.0) * 1e6);
}

/* Refused frames: the driver took less than was offered. Said once, on
 * the thread that writes, whose counts these are and whose warn-once
 * word sink_warned_flush is. The estimator keeps the other one: on the
 * threaded pipeline it runs on the core's thread, and this runs on the
 * audio thread. */
static void audio_driver_sink_refused(audio_driver_state_t *audio_st)
{
   uint64_t offered  = audio_st->sink_offered_raw;
   uint64_t accepted = audio_st->sink_accepted;
   if (     (audio_st->sink_warned_flush & AUDIO_SINK_WARNED_DROPPED)
         || !(retro_atomic_load_acquire_int(&audio_st->runloop_snapshot)
            & AUDIO_SNAP_SINK_EST))
      return;
   if (audio_st->pipe_pending_bytes)
   {
      size_t fb = audio_driver_dev_frame_bytes(audio_st);
      accepted += (audio_st->pipe_pending_bytes + fb - 1) / fb;
   }
   if (offered > 48000 * 30 && accepted < offered && (offered - accepted) * 1000 > offered)
   {
      audio_st->sink_warned_flush |= AUDIO_SINK_WARNED_DROPPED;
      RARCH_WARN("[Audio] The driver refused %.2f%% of the audio offered: it is being dropped, most likely a buffer smaller than what the core delivers per frame with audio sync off.\n",
            100.0 * (double)(offered - accepted) / (double)offered);
   }
}

/**
 * audio_driver_sink_update:
 *
 * Runs on the thread that flushes, after each write; see the note at
 * AUDIO_SINK_BIAS_PLAUSIBLE. now_usec is a parameter so the harness can
 * drive the clock.
 */
static void audio_driver_sink_update(audio_driver_state_t *audio_st,
      int64_t now_usec)
{
   uint64_t consumed;
   uint64_t consumed_alt;
   const audio_driver_t *audio = audio_st->current_audio;
   unsigned rate = audio_st->out_rate;

   if (!(retro_atomic_load_acquire_int(&audio_st->runloop_snapshot)
         & AUDIO_SNAP_SINK_EST))
   {
      /* Off means no bias, and no baseline: turning it back on
       * measures afresh. The overlay draws the rates while one is
       * known; with the option off there is none. */
      audio_st->sink_bias = 1.0;
      retro_atomic_store_release_int(&audio_st->sink_bias_q, 0);
      audio_st->sink_started   = 0;
      audio_st->sink_applied   = 0;
      audio_st->sink_rate_hz   = 0.0;
      audio_st->sink_source_hz = 0.0;
      return;
   }

   if (!audio || !audio->frames_consumed || !rate)
      return;

   /* Once a window: this runs after every write, and frames_consumed()
    * is not always a cheap read. */
   if (audio_st->sink_started && now_usec < audio_st->sink_window_at)
      return;

   consumed     = audio->frames_consumed(audio_st->context_audio_data);
   consumed_alt = audio->frames_consumed_fallback
         ? audio->frames_consumed_fallback(audio_st->context_audio_data) : 0;

   if (!audio_st->sink_started)
   {
      audio_st->sink_started   = now_usec;
      audio_st->sink_window_at = now_usec + AUDIO_SINK_WINDOW_USEC;
      audio_st->sink_apply_at  = now_usec + AUDIO_SINK_BASELINE_USEC;
      audio_st->sink_settled   = 0;
      audio_st->sink_discarded = 0;
      memset(&audio_st->sink_kept,    0, sizeof(audio_st->sink_kept));
      memset(&audio_st->sink_pending, 0, sizeof(audio_st->sink_pending));
      audio_st->sink_pending_broken = false;
      memset(audio_st->sink_recent_usec,     0, sizeof(audio_st->sink_recent_usec));
      memset(audio_st->sink_recent_offered,  0, sizeof(audio_st->sink_recent_offered));
      memset(audio_st->sink_recent_consumed, 0, sizeof(audio_st->sink_recent_consumed));
      audio_st->sink_recent_head = 0;
      audio_driver_sink_mark(audio_st, &audio_st->sink_at_window,
            consumed, consumed_alt);
      return;
   }

   audio_driver_sink_window(audio_st, now_usec, consumed, consumed_alt, rate);

   audio_driver_sink_apply(audio_st, now_usec, rate);
}

/* Parts per million by which the driver's own approximation of the
 * device's consumption differs from the clock it actually reads, over
 * the last window. Zero where a driver has only the one number. */
double audio_driver_get_sink_alt_ppm(void)
{
   return audio_driver_st.sink_alt_ppm;
}

/* The device's own clock against the rate the driver asked for, in
 * parts per million, where the driver can measure one. False until
 * there is an estimate, and for every driver that has no clock to
 * read. Nothing acts on this; it is shown beside the sink estimate so
 * the two can be compared on real hardware. */
bool audio_driver_get_device_clock_ppm(double *ppm)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   if (     !audio_st->current_audio
         || !audio_st->current_audio->device_clock_ppm
         || !audio_st->context_audio_data)
      return false;
   return audio_st->current_audio->device_clock_ppm(
         audio_st->context_audio_data, ppm);
}

double audio_driver_get_sink_rate_hz(double *bias, double *source_hz)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   if (bias)
      *bias = audio_st->sink_bias > 0.0 ? audio_st->sink_bias : 1.0;
   if (source_hz)
      *source_hz = audio_st->sink_source_hz;
   return audio_st->sink_rate_hz;
}

/**
 * audio_driver_update_drc_threshold:
 *
 * Recompute drc_threshold_int16s for the current sample rate / fps.
 * The threshold is one game-frame's worth of stereo int16 samples at
 * the core's input rate: (input_rate / fps) * 2.
 *
 * Called by audio_driver_init_internal once input rate is set, and
 * implicitly re-runs through it on every SET_SYSTEM_AV_INFO since
 * that envcall forces an audio driver reinit. So rate/fps changes
 * mid-session are picked up automatically.
 *
 * Uses video_state's av_info for fps. If fps is unset or implausibly
 * small (cold start before core load), falls back to
 * AUDIO_DRC_FALLBACK_FPS (60 Hz) so the menu audio path still has a
 * sensible threshold. The minimum threshold is floored at
 * AUDIO_DRC_MIN_THRESHOLD_INT16S to handle pathological rate/fps
 * combinations.
 **/
void audio_driver_update_drc_threshold(audio_driver_state_t *audio_st)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   double fps                     = (video_st->av_info.timing.fps > 1.0)
         ? video_st->av_info.timing.fps
         : AUDIO_DRC_FALLBACK_FPS;
   double input_rate              = (audio_st->input > 0.0f)
         ? audio_st->input
         : (double)config_get_ptr()->uints.audio_output_sample_rate;
   /* (samples per frame at the core's submission rate) * 2 channels.
    * Floor at AUDIO_DRC_MIN_THRESHOLD_INT16S so pathological inputs
    * don't push the threshold below ~10 ms worth of audio. */
   size_t threshold               = (size_t)floor(input_rate / fps) * 2;

   if (threshold < AUDIO_DRC_MIN_THRESHOLD_INT16S)
      threshold                   = AUDIO_DRC_MIN_THRESHOLD_INT16S;
   audio_st->drc_threshold_int16s = threshold;
}

/* Fast-forward "speedup" pitch tracking, shared by the float and the
 * deterministic s16 paths.  Measures the real wall-clock time between
 * flushes, keeps an exponential moving average of it over the last
 * AUDIO_FF_EXP_AVG_SAMPLES flushes, and returns a multiplier for the
 * resampler ratio so the audio time-stretches to track the actual output
 * speed rather than crackling or being muted.  The EMA smooths the estimate
 * so pitches stay recognizable and avoids time compression/decompression
 * every frame. The 1.0x interval is averaged too: batch sizes vary, and a
 * per-batch figure leaves pitch-preserving playback short of real time; see
 * https://en.wikipedia.org/wiki/Moving_average#Exponential_moving_average.
 *
 * The state (last_flush_time, avg_flush_delta, avg_expected_delta) lives on
 * audio_st and the arithmetic is identical regardless of caller, so it is
 * float/int16 agnostic: a session may move between the two paths
 * mid-fast-forward and the wall-clock series stays continuous.  The first
 * flush of a fast-forward seeds the averages at the configured fast-forward
 * ratio and returns its multiplier, and the measured speed takes over from
 * there; audio_driver_ff_mult_reset() arms that seed again once
 * fast-forward is released, so the idle time between two fast-forwards
 * is never read as one enormous flush interval.
 *
 * With the threaded pipeline the flush runs on the audio thread, whose
 * cadence is set by the device draining, not by the core: a flush
 * interval there is the previous ratio played back, and measuring it
 * would only confirm whatever the last multiplier was. The producer
 * measures instead, at frame boundaries on the core's thread,
 * and hands the result to the consumer in pipe_ff_mult_q16. */
/* Bound a resampler ratio against the capacity of the output scratch.
 *
 * Neither struct resampler_data nor struct resampler_data_int16 carries an
 * output-capacity field: every driver emits frames until its phase
 * accumulator runs out of input and writes them straight to data_out (see
 * the unbounded `while (resamp->time < phases)` loop in the sinc driver).
 * The caller therefore owns the bound.
 *
 * audio_driver_init_internal sizes both output scratches at
 * max_buffer_samples * AUDIO_MAX_RATIO * slowmotion_ratio samples, i.e. it
 * assumes the effective ratio never exceeds AUDIO_MAX_RATIO relative to a
 * full input batch.  But the effective ratio is a product -
 * src_ratio_orig (output rate / core rate) * the rate-control adjustment *
 * slowmotion_ratio * the fast-forward pitch multiplier - and only the last
 * factor is clamped to AUDIO_MAX_RATIO.  src_ratio_orig is taken straight
 * from the core's reported sample rate with no upper limit, so a core
 * declaring a low rate against a high output rate overruns the headroom on
 * its own; the fast-forward multiplier (which exceeds 1.0 whenever the core
 * runs slower than realtime with fast-forward held) multiplies on top.
 *
 * Clamping rather than asserting keeps the failure mode proportionate: an
 * over-range ratio only mistunes the output for as long as the condition
 * lasts, whereas letting it through writes past the end of a heap block.
 *
 * The worst-case output count is not ceil(input_frames * ratio).  The drivers
 * carry a phase accumulator between calls, and a call that begins with the
 * accumulator already below `phases` emits before it consumes, so a single
 * call can run ahead of the steady-state rate.  Measured across the sinc
 * float, sinc int16, CC int16 and nearest drivers, the overshoot is about one
 * input frame's worth of output at moderate ratios and grows with
 * accumulator drift at extreme ones (~64 frames at a ratio of 1177).
 *
 * Rather than model that, reserve 16 input frames' worth of output, which is
 * an order of magnitude more than any overshoot measured over the full ratio
 * range.  The reservation is proportional, so the absolute margin scales with
 * the buffer: ~470 frames at the default sizing, ~4700 at slowmotion_ratio
 * 10.  It still admits every ratio the buffer can physically hold - the
 * allocation supports 32 and this permits 31.5 - so a legitimate extreme such
 * as an 8 kHz core against 192 kHz output (ratio 24) passes unclamped. */
static double audio_driver_bound_ratio(double ratio,
      size_t input_frames, size_t capacity_frames)
{
   double max_ratio;

   if (input_frames == 0 || capacity_frames == 0)
      return ratio;

   max_ratio = (double)capacity_frames / (double)(input_frames + 16);

   return (ratio > max_ratio) ? max_ratio : ratio;
}

/* The ratio a flush resamples at, composed in one place: the current
 * ratio - rate control's adjustment and the sink bias in it - times
 * slow motion, times fast-forward's speedup multiplier where the
 * caller has one. The consumer's preflight and the flush itself use
 * this, so what is reserved is what is produced. The resampler bound
 * for that ratio is the same sixteen input frames' worth of overshoot
 * audio_driver_bound_ratio() reserves. */
/* The slowmotion ratio the main thread last published; the audio
 * side's counterpart to the setting, safe on any thread. */
static INLINE float audio_driver_snapshot_slowmotion(
      audio_driver_state_t *audio_st)
{
   float ratio;
   int   bits = retro_atomic_load_acquire_int(
         &audio_st->runloop_slowmotion_bits);
   memcpy(&ratio, &bits, sizeof(ratio));
   return ratio;
}

/* The fast-forward ratio the main thread last published, the
 * override's when one is active. Safe on any thread; the estimate's
 * seed must not read the runloop or the settings mid-submit. */
static INLINE float audio_driver_snapshot_ffratio(
      audio_driver_state_t *audio_st)
{
   float ratio;
   int   bits = retro_atomic_load_acquire_int(
         &audio_st->runloop_ffratio_bits);
   memcpy(&ratio, &bits, sizeof(ratio));
   return ratio;
}

static INLINE double audio_driver_effective_ratio(
      const audio_driver_state_t *audio_st, bool is_slowmotion,
      float slowmotion_ratio, double ff_mult)
{
   double ratio = audio_st->src_ratio_curr;
   if (is_slowmotion)
      ratio *= slowmotion_ratio;
   return ratio * ff_mult;
}

static INLINE size_t audio_driver_output_bound(double ratio, size_t input_frames)
{
   return (size_t)((double)(input_frames + 16) * ratio) + 1;
}

/* The inverse: the largest input frame count whose bound still fits in
 * output_frames. Not output_frames/ratio - the bound adds a resampler
 * margin of sixteen frames and a rounding frame on top of the ratio, so
 * the plain division overshoots it by that margin, and a caller that
 * waits for output_frames of room and then writes the bound has written
 * more than it waited for. Solved, then walked down until the bound
 * itself agrees, so the two can never disagree by a rounding step. */
static INLINE size_t audio_driver_input_bound(double ratio, size_t output_frames)
{
   double n;
   size_t input_frames;
   if (ratio <= 0.0 || output_frames < 2)
      return 0;
   n = (double)(output_frames - 1) / ratio - 16.0;
   if (n <= 0.0)
      return 0;
   input_frames = (size_t)n;
   while (     input_frames
         && audio_driver_output_bound(ratio, input_frames) > output_frames)
      input_frames--;
   return input_frames;
}

/* A harness may define this to count a flush that produced more than
 * its bound; the shipping build has no check on the audio thread. */
#ifndef AUDIO_OUTPUT_BOUND_CHECK
#define AUDIO_OUTPUT_BOUND_CHECK(produced, bound) do {} while (0)
#endif

static double audio_driver_fastforward_ratio_mult(
      audio_driver_state_t *audio_st, size_t input_frames)
{
   const retro_time_t flush_time = cpu_features_get_time_usec();
   double mult                   = 1.0;
   /* What we should see if the speed was 1.0x, converted to microsecs. */
   const double expected_flush_delta =
         (input_frames / audio_st->input * 1000000);

   if (audio_st->last_flush_time > 0)
   {
      const retro_time_t n      = AUDIO_FF_EXP_AVG_SAMPLES;
      audio_st->avg_flush_delta = audio_st->avg_flush_delta * (n - 1) / n +
            (flush_time - audio_st->last_flush_time) / n;
      audio_st->avg_expected_delta = audio_st->avg_expected_delta * (n - 1) / n
            + expected_flush_delta / n;

      /* How much does avg_flush_delta deviate from the 1.0x delta? */
      mult = MAX(AUDIO_MIN_RATIO,
            MIN(AUDIO_MAX_RATIO,
               audio_st->avg_flush_delta / audio_st->avg_expected_delta));
   }
   else
   {
      /* Seed at the configured ratio. The limiter has the core there
       * within a frame, and the average would take some fifty flushes
       * to follow it, overfilling the non-blocking device meanwhile.
       * The ratio comes published: this runs mid-submit, on the audio
       * thread when the core owns its audio callback. */
      double ratio = audio_driver_snapshot_ffratio(audio_st);
      if (ratio > 1.0)
         mult = 1.0 / ratio;
      else
         ratio = 1.0;
      audio_st->avg_flush_delta    = (retro_time_t)(expected_flush_delta / ratio);
      audio_st->avg_expected_delta = expected_flush_delta;
   }

   audio_st->last_flush_time = flush_time;
   return mult;
}

/* The inline flush's multiplier. The first flush of a hold spans the
 * frame before the limiter lifted, so it plays at 1.0: at the seeded
 * ratio the device is a frame short at the edge. */
static double audio_driver_inline_ff_mult(audio_driver_state_t *audio_st,
      size_t input_frames)
{
   bool seed   = !audio_st->last_flush_time;
   double mult = audio_driver_fastforward_ratio_mult(audio_st, input_frames);
   return seed ? 1.0 : mult;
}

static INLINE void audio_driver_ff_mult_reset(audio_driver_state_t *audio_st)
{
   audio_st->last_flush_time = 0;
#ifdef HAVE_THREADS
   audio_st->pipe_ff_frames = 0;
#endif
}

#ifdef HAVE_THREADS
#define AUDIO_PIPE_FF_TRIM_MAX  0.08
#define AUDIO_PIPE_FF_TRIM_GAIN 0.5

/* The pull rate the multiplier sets and the core's rate never agree
 * exactly, and a core that waits on a full ring runs at the pull rate.
 * Steer the ring toward half full: above it the consumer pulls faster,
 * so the core it holds back speeds up and is measured faster; below it
 * slower, so the ring is not run dry. */
static double audio_driver_pipe_ff_trim(audio_driver_state_t *audio_st)
{
   double cap = (double)audio_st->pipe_ring.capacity;
   double err;
   if (!(cap > 0.0))
      return 1.0;
   err = AUDIO_PIPE_FF_TRIM_GAIN
      * ((double)retro_spsc_read_avail(&audio_st->pipe_ring) - cap * 0.5) / cap;
   if (err > AUDIO_PIPE_FF_TRIM_MAX)
      err = AUDIO_PIPE_FF_TRIM_MAX;
   else if (err < -AUDIO_PIPE_FF_TRIM_MAX)
      err = -AUDIO_PIPE_FF_TRIM_MAX;
   return 1.0 + err;
}
#endif

static void audio_driver_ff_frame_end(audio_driver_state_t *audio_st)
{
#ifdef HAVE_THREADS
   if (audio_st->pipe_threaded)
   {
      size_t frames = audio_st->pipe_ff_frames;
      if (audio_st->pipe_transport_follow)
         audio_st->pipe_transport_follow |= AUDIO_TRANSPORT_UPDATE;
      if (frames)
      {
         double mult = audio_driver_fastforward_ratio_mult(audio_st, frames);
         audio_st->pipe_ff_frames = 0;
         if (audio_driver_pipe_ff_waits(audio_st))
            mult = MIN(AUDIO_MAX_RATIO, MAX(AUDIO_MIN_RATIO,
                     mult / audio_driver_pipe_ff_trim(audio_st)));
         retro_atomic_store_release_int(&audio_st->pipe_ff_mult_q16,
               (int)(mult * 65536.0));
         return;
      }
   }
#endif
   /* Keep intervals across batched audio, but exclude released/paused gaps. */
   if (audio_st->last_flush_time)
   {
      int snap = retro_atomic_load_acquire_int(&audio_st->runloop_snapshot);
      if (!(snap & AUDIO_SNAP_FASTMOTION) || (snap & AUDIO_SNAP_PAUSED)
            || ((snap & AUDIO_SNAP_MENU_ALIVE) && (snap & AUDIO_SNAP_MENU_PAUSES)
               && (snap & AUDIO_SNAP_ALLOW_PAUSE)))
         audio_driver_ff_mult_reset(audio_st);
   }
}

/* The callback-core variant of the producer measurement above. A
 * callback core delivers no frames on the main thread, so there is
 * nothing to count - but what fast-forward changes for it is
 * retro_run cadence, so that is what gets measured: one video frame's
 * worth of audio at the core's input rate is the expected interval,
 * the wall clock between frame ends is the achieved one, and the EMA
 * turns the pair into the achieved-speed multiplier exactly as the
 * threaded pipeline's producer does. Published in pipe_ff_mult_q16
 * for the audio thread, whose flush reads it and never runs the
 * cadence measurement itself - on that thread the cadence is the
 * device draining, and measuring it would only read back the
 * multiplier last applied (see audio_driver_ff_mult()).
 *
 * Runs only when the snapshot carries the fastmotion bit, which
 * publish withholds for callback cores unless the person opted in
 * (audio_fastforward_callback), so the off state costs one load. The
 * reset stores race the audio thread's own idle reset at fast-forward
 * boundaries; both only ever store zeros and unity there, so the
 * worst case is one stale EMA sample that the next frame corrects. */
static void audio_driver_ff_callback_frame_end(audio_driver_state_t *audio_st)
{
   int snap = retro_atomic_load_acquire_int(&audio_st->runloop_snapshot);
   if ((snap & AUDIO_SNAP_FASTMOTION) && !(snap & AUDIO_SNAP_PAUSED))
   {
      video_driver_state_t *video_st = video_state_get_ptr();
      double fps        = (video_st->av_info.timing.fps > 1.0)
            ? video_st->av_info.timing.fps
            : AUDIO_DRC_FALLBACK_FPS;
      double input_rate = (audio_st->input > 0.0f)
            ? audio_st->input
            : (double)config_get_ptr()->uints.audio_output_sample_rate;
      size_t frames     = (size_t)(input_rate / fps);
      if (frames < 1)
         frames         = 1;
      retro_atomic_store_release_int(&audio_st->pipe_ff_mult_q16,
            (int)(audio_driver_fastforward_ratio_mult(audio_st, frames)
               * 65536.0));
   }
   else if (audio_st->last_flush_time)
   {
      audio_driver_ff_mult_reset(audio_st);
      retro_atomic_store_release_int(&audio_st->pipe_ff_mult_q16, 65536);
   }
}

/* Whether fast-forward audio follows the measured speed: Speedup asks
 * for it outright, and pitch preservation needs it to hold pitch. The
 * filter-only transport keeps the ordinary Speedup or discard handling. */
static INLINE bool audio_driver_ff_follows(const audio_driver_state_t *audio_st,
      bool speedup)
{
   if (speedup)
      return true;
#ifdef HAVE_THREADS
   if (audio_st->pipe_threaded)
      return audio_st->pipe_transport && !audio_st->transport_lpf_only;
#endif
   return audio_st->inline_transport && !audio_st->transport_lpf_only;
}

#ifdef HAVE_THREADS
/* Whether a fast-forward producer waits on a full ring rather than
 * dropping: when the consumer pulls at the core's speed and a limiter
 * bounds that speed. Unlimited, waiting would hold the core to the
 * fastest tempo the audio can play. */
static bool audio_driver_pipe_ff_waits(audio_driver_state_t *audio_st)
{
   int snap = retro_atomic_load_acquire_int(&audio_st->runloop_snapshot);
   return (snap & AUDIO_SNAP_SYNC)
      && audio_driver_ff_follows(audio_st, (snap & AUDIO_SNAP_FF_SPEEDUP) != 0)
      && audio_driver_snapshot_ffratio(audio_st) > 1.0;
}
#endif

/* The speedup multiplier for a flush: measured here on the inline
 * pipeline, where the flush runs at the core's cadence; taken from the
 * producer's measurement on the threaded one. */
static double audio_driver_ff_mult(audio_driver_state_t *audio_st,
      size_t input_frames)
{
#ifdef HAVE_THREADS
   if (audio_st->pipe_threaded)
      return (double)retro_atomic_load_acquire_int(
            &audio_st->pipe_ff_mult_q16) / 65536.0;
#endif
   /* A callback core's flush runs on the audio thread, where cadence
    * is the device draining: measuring it here would read back the
    * multiplier last applied. Take the frame-end measurement instead
    * (see audio_driver_ff_callback_frame_end()). */
   if (audio_st->callback.callback)
      return (double)retro_atomic_load_acquire_int(
            &audio_st->pipe_ff_mult_q16) / 65536.0;
   return audio_driver_inline_ff_mult(audio_st, input_frames);
}

/* Frames of headroom deliberately resampled beyond what the device
 * reports writable.  Covers the drain between the write_avail() sample
 * below and the write itself (a fast-forward flush interval is a few
 * hundred microseconds, i.e. some tens of frames at typical output
 * rates), plus any avail rounding inside the driver.  Overfilling by
 * this margin reproduces today's behaviour exactly - the driver's
 * non-blocking write drops the excess - while underfilling would starve
 * the device and open audible gaps, so the margin errs on the side of
 * a little discarded work: at 16 taps the slack costs ~1 us per flush. */
#define AUDIO_FF_DISCARD_SLACK_FRAMES 64

/**
 * audio_driver_ff_discard_bound:
 *
 * During fast-forward without "speedup" pitch tracking, the driver is
 * non-blocking and its buffer is pinned close to full - it drains at
 * real-time rate while flushes arrive at fast-forward rate - so almost
 * every frame the resampler produces is dropped by the write below.
 * At high multipliers that is >90% of the convolution (and of the
 * conversion/clamp/write stages after it) spent on frames that are
 * never heard.
 *
 * Bound the resampler's *input* so it only produces what the device
 * can accept (plus slack): frames the write would have dropped from
 * the tail of the chunk are instead never resampled.  The audible
 * result is unchanged - the same head-of-chunk fragments reach the
 * device either way - only the discard moves from after the resampler
 * to before it.
 *
 * Deliberately input capping and not ratio bounding: shrinking the
 * ratio would time-compress the chunk into the writable space, which
 * is exactly the pitch-raising "speedup" behaviour the user declined
 * by leaving that option off.
 *
 * write_avail() returns bytes (see wasapi/alsa/dsound implementations);
 * under threaded audio it is the same locked accessor the rate-control
 * path already calls once per DRC interval.  Drivers without
 * write_avail() keep the old behaviour.
 *
 * Returns: capped input frame count (<= in_frames, never 0 while the
 * slack is non-zero, so the resampler ring stays warm).
 **/
static size_t audio_driver_ff_discard_bound(audio_driver_state_t *audio_st,
      double ratio, size_t in_frames)
{
   size_t avail_bytes;
   size_t out_frame_bytes;
   size_t max_out_frames;
   size_t max_in_frames;
   const audio_driver_t *audio = audio_st->current_audio;

   if (!audio || !audio->write_avail || !audio_st->context_audio_data)
      return in_frames;
   /* Also rejects NaN. */
   if (!(ratio > 0.0))
      return in_frames;

   avail_bytes     = audio->write_avail(audio_st->context_audio_data);
   if (audio_st->buffer_size && avail_bytes > audio_st->buffer_size)
      avail_bytes  = audio_st->buffer_size;
   out_frame_bytes = audio_driver_dev_frame_bytes(audio_st);
   max_out_frames  = avail_bytes / out_frame_bytes
         + AUDIO_FF_DISCARD_SLACK_FRAMES;
   max_in_frames   = (size_t)((double)max_out_frames / ratio);

   return (in_frames < max_in_frames) ? in_frames : max_in_frames;
}

/* Whether the deterministic integer (s16) game path would run for a core
 * delivering samples in format is_float.  Mirrors the use_i16 gate in
 * audio_driver_flush; also used at mixer play() time to choose the voice
 * format so mixer sounds decode/resample/mix in the same domain as the game
 * audio, avoiding the s16<->float voice fold on the int16 path. */
static bool audio_driver_mixer_use_s16(bool is_float)
{
   return    (audio_driver_st.resampler_data_int16 != NULL)
          && (retro_atomic_load_acquire_int(
                &audio_driver_st.runloop_snapshot) & AUDIO_SNAP_FASTPATH_S16)
          && !is_float
#ifdef HAVE_DSP_FILTER
          && (!audio_driver_st.dsp
                || retro_dsp_filter_supports_int16(audio_driver_st.dsp))
#endif
          ;
}

/* Saturating, NaN-safe float -> s16 conversion of one sample.
 *
 * Mirrors the scalar tail of convert_float_to_s16() exactly - same
 * round-half-away-from-zero, same NaN handling - so a value converted here
 * is bit-identical to one converted by the shared routine.
 *
 * Casting a non-finite or out-of-int32-range float to an integer is
 * undefined.  On x86 it yields the "integer indefinite" INT32_MIN, so a NaN
 * or a large POSITIVE sample emerges as full-scale negative; other targets
 * saturate, or produce something else again.  convert_float_to_s16() guards
 * against this, but three sites in this file open-coded the same conversion
 * without the guard.  They all route through here now.
 *
 * The NaN test is on the bit pattern rather than (v != v) because -ffast-math
 * (implied by -Ofast on HAVE_C_A7A7 builds) is entitled to fold the latter to
 * false.  Returns a value already clamped to the s16 range, so callers that
 * sum it onto existing audio only need their own overflow clamp.
 */
static INLINE int32_t audio_float_to_s16_sat(float v)
{
   uint32_t bits;
   float    scaled = v * 0x8000;

   memcpy(&bits, &scaled, sizeof(bits));
   if ((bits & 0x7FFFFFFFu) > 0x7F800000u)
      return 0;

   scaled += (scaled >= 0.0f ? 0.5f : -0.5f);

   if (scaled >  32767.0f)
      return  32767;
   if (scaled < -32768.0f)
      return -32768;
   return (int32_t)scaled;
}

#ifdef HAVE_AUDIOMIXER
/* Cross-format voice folds.  A voice's format is fixed at play() time but the
 * game pipeline picks int16-vs-float per flush, so a voice can occasionally
 * land on the other path (e.g. a float-only DSP loaded while an s16 voice was
 * sounding).  These sum the "foreign" voices via their native mixer into a
 * scratch, then fold onto the active buffer so nothing is ever dropped.  They
 * are gated by audio_mixer_has_{float,s16}_voices() so the common single-
 * format case skips them entirely. */
static void audio_mixer_fold_s16_voices_into_float(float *dst,
      int16_t *scratch, unsigned frames, float gain, bool override)
{
   unsigned k;
   unsigned total = frames * 2;
   memset(scratch, 0, total * sizeof(int16_t));
   audio_mixer_mix_s16(scratch, frames, GAIN_TO_Q16(gain), override);
   for (k = 0; k < total; k++)
      dst[k] += (float)scratch[k] * (1.0f / 0x8000);
}

static void audio_mixer_fold_float_voices_into_s16(int16_t *dst,
      float *scratch, unsigned frames, float gain, bool override)
{
   unsigned k;
   unsigned total = frames * 2;
   memset(scratch, 0, total * sizeof(float));
   audio_mixer_mix(scratch, frames, gain, override);
   for (k = 0; k < total; k++)
   {
      int32_t s  = (int32_t)dst[k] + audio_float_to_s16_sat(scratch[k]);
      if      (s >  32767) s =  32767;
      else if (s < -32768) s = -32768;
      dst[k]     = (int16_t)s;
   }
}
#endif

/* The bytes of one frame as the device takes them: its channels at
 * its sample size. */
/* Hands frames of the stereo mix to the driver. On a stereo device
 * the buffer goes as it is, float or int16 as the driver takes it -
 * this is the write as it always was. On a wider device the stereo
 * is widened into upmix_buf, narrowed to int16 there if the driver
 * wants that, and the wider frames go. Returns the bytes the driver
 * accepted, in frames of the device's own size for the caller's
 * accounting. */
/* The device frame from the fronts, the core's extras where the device
 * has their positions, and the upmix's fill for the rest. A pair the
 * core sent at the back goes to a device with only a side pair, and
 * the reverse; a position the device lacks that the core sent is
 * folded into the fronts at -3 dB (LFE dropped, as the device has no
 * speaker for it). The upmix has already filled every slot from the
 * fronts; the core's channels replace what they cover. */
static int audio_driver_extra_slot(uint32_t layout, uint32_t position)
{
   uint32_t below;
   if (!(layout & position))
      return -1;
   below = layout & (position - 1);
   return (int)audio_layout_channels(below);
}

static void audio_driver_extra_merge_layout_f32(audio_driver_state_t *audio_st,
      float *dev, size_t frames, uint32_t into_layout, unsigned into_channels)
{
   const unsigned ch  = audio_st->extra.channels;
   const unsigned och = into_channels;
   const uint32_t pos = audio_st->extra.positions;
   const uint32_t dev_layout = into_layout;
   int dst[11], srcslot[11];
   unsigned n = 0, bit, k;
   size_t f;
   /* each extra: its slot in the extras frame, and where it goes */
   for (bit = 2; bit < 11; bit++)
   {
      uint32_t p = 1u << bit, to = p;
      if (!(pos & p))
         continue;
      srcslot[n] = audio_driver_extra_slot(pos, p);
      if (!(dev_layout & to))
      {
         /* a back pair for a side-only device and the reverse */
         if (p == AUDIO_SPEAKER_BACK_LEFT)  to = AUDIO_SPEAKER_SIDE_LEFT;
         if (p == AUDIO_SPEAKER_BACK_RIGHT) to = AUDIO_SPEAKER_SIDE_RIGHT;
         if (p == AUDIO_SPEAKER_SIDE_LEFT)  to = AUDIO_SPEAKER_BACK_LEFT;
         if (p == AUDIO_SPEAKER_SIDE_RIGHT) to = AUDIO_SPEAKER_BACK_RIGHT;
      }
      dst[n] = (dev_layout & to) ? audio_driver_extra_slot(dev_layout, to) : -(int)bit - 1;
      n++;
   }
   for (f = 0; f < frames; f++)
   {
      const float *e = audio_st->extra.out_f + f * ch;
      float *d = dev + f * och;
      for (k = 0; k < n; k++)
      {
         if (dst[k] >= 0)
            d[dst[k]] = e[srcslot[k]];
         else
         {
            /* no speaker for it: fold into the fronts, LFE dropped */
            unsigned b = (unsigned)(-dst[k] - 1);
            float v = e[srcslot[k]] * 0.70710678f;
            if (b == 3) continue;
            if (b == 2 || b == 8) { d[0] += v; d[1] += v; }
            else if (b == 4 || b == 6 || b == 9) d[0] += v;
            else d[1] += v;
         }
      }
   }
}

/* The device's own frame, which is the usual case. */
static void audio_driver_extra_merge_f32(audio_driver_state_t *audio_st,
      float *dev, size_t frames)
{
   audio_driver_extra_merge_layout_f32(audio_st, dev, frames,
         audio_st->out_layout, audio_st->out_channels);
}

static void audio_driver_extra_merge_s16(audio_driver_state_t *audio_st,
      int16_t *dev, size_t frames)
{
   const unsigned ch  = audio_st->extra.channels;
   const unsigned och = audio_st->out_channels;
   const uint32_t pos = audio_st->extra.positions;
   const uint32_t dev_layout = audio_st->out_layout;
   int dst[11], srcslot[11];
   unsigned n = 0, bit, k;
   size_t f;
   for (bit = 2; bit < 11; bit++)
   {
      uint32_t p = 1u << bit, to = p;
      if (!(pos & p))
         continue;
      srcslot[n] = audio_driver_extra_slot(pos, p);
      if (!(dev_layout & to))
      {
         if (p == AUDIO_SPEAKER_BACK_LEFT)  to = AUDIO_SPEAKER_SIDE_LEFT;
         if (p == AUDIO_SPEAKER_BACK_RIGHT) to = AUDIO_SPEAKER_SIDE_RIGHT;
         if (p == AUDIO_SPEAKER_SIDE_LEFT)  to = AUDIO_SPEAKER_BACK_LEFT;
         if (p == AUDIO_SPEAKER_SIDE_RIGHT) to = AUDIO_SPEAKER_BACK_RIGHT;
      }
      dst[n] = (dev_layout & to) ? audio_driver_extra_slot(dev_layout, to) : -(int)bit - 1;
      n++;
   }
   for (f = 0; f < frames; f++)
   {
      const int16_t *e = audio_st->extra.out_i + f * ch;
      int16_t *d = dev + f * och;
      for (k = 0; k < n; k++)
      {
         if (dst[k] >= 0)
            d[dst[k]] = e[srcslot[k]];
         else
         {
            unsigned b = (unsigned)(-dst[k] - 1);
            int32_t v = ((int32_t)e[srcslot[k]] * 23170 + 16384) >> 15;
            int32_t l = d[0], r = d[1];
            if (b == 3) continue;
            if (b == 2 || b == 8) { l += v; r += v; }
            else if (b == 4 || b == 6 || b == 9) l += v;
            else r += v;
            d[0] = (int16_t)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
            d[1] = (int16_t)(r > 32767 ? 32767 : r < -32768 ? -32768 : r);
         }
      }
   }
}

/* A non-blocking write the ring cannot take is cut short. The discard
 * path means to lose that; the pitch-preserving stream is continuous, and
 * its first output after a press meets the ring the 1x blocking writes
 * left full. Wait for the room instead: the ring frees it within a period. */
static ssize_t audio_driver_dev_write(audio_driver_state_t *audio_st,
      const audio_driver_t *audio, const void *buf, size_t bytes,
      bool wait_room)
{
   ssize_t w = audio->write(audio_st->context_audio_data, buf, bytes);
   if (     wait_room
         && w >= 0 && (size_t)w < bytes
         && (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_NONBLOCK)
         && !audio_st->pipe_threaded
         && audio_st->inline_transport
         && !audio_st->inline_transport->bypassed
         && !audio_st->transport_lpf_only
         && audio->set_nonblock_state)
   {
      ssize_t rest;
      audio->set_nonblock_state(audio_st->context_audio_data, false);
      rest = audio->write(audio_st->context_audio_data,
            (const uint8_t*)buf + w, bytes - (size_t)w);
      audio->set_nonblock_state(audio_st->context_audio_data,
            (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_NONBLOCK) != 0);
      if (rest > 0)
         w += rest;
   }
   return w;
}

static ssize_t audio_driver_write_frames(audio_driver_state_t *audio_st,
      const audio_driver_t *audio, const void *stereo, size_t frames,
      bool stereo_is_float, bool wait_room)
{
   bool   dev_float = (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_USE_FLOAT) != 0;
   size_t sample    = dev_float ? sizeof(float) : sizeof(int16_t);

   if (audio_st->out_channels <= 2 && audio_st->virtualize && audio_st->virt_buf)
   {
      /* Headphones: stereo to a virtual 5.1 and back to two ears. */
      const float *src = (const float*)stereo;
      if (frames > audio_st->upmix_frames)
         frames = audio_st->upmix_frames;
      if (!stereo_is_float)
      {
         float *stage = (float*)audio_st->upmix_i16;
         convert_s16_to_float(stage, (const int16_t*)stereo, frames * 2, 1.0f);
         src = stage;
      }
      audio_upmix_process(&audio_st->upmix, audio_st->virt_buf, src, frames);
      /* The virtual 5.1 the binaural stage renders was, until here,
       * always the upmix's guess at one: the stereo widened, with the
       * rears a copy of the fronts at -3 dB. A core that delivered its
       * own rears had them folded into that stereo and then invented
       * again from it. Where the entry left the core's channels
       * beside the fronts, they replace the invented ones - so what
       * reaches the ears is the core's rear, placed behind the
       * listener, rather than its front placed behind them. */
      if (audio_st->extra.out_frames >= frames && audio_st->extra.channels)
      {
         if (audio_st->extra.res_int16)
            convert_s16_to_float(audio_st->extra.out_f, audio_st->extra.out_i,
                  frames * audio_st->extra.channels, 1.0f);
         audio_driver_extra_merge_layout_f32(audio_st, audio_st->virt_buf,
               frames, AUDIO_LAYOUT_5POINT1, 6);
         audio_st->extra.out_frames = 0;
      }
      audio_binaural_process(&audio_st->binaural, audio_st->upmix_buf, audio_st->virt_buf, frames);
      if (dev_float)
         return audio_driver_dev_write(audio_st, audio, audio_st->upmix_buf,
               frames * 2 * sizeof(float), wait_room);
      convert_float_to_s16(audio_st->upmix_i16, audio_st->upmix_buf, frames * 2);
      return audio_driver_dev_write(audio_st, audio, audio_st->upmix_i16,
            frames * 2 * sizeof(int16_t), wait_room);
   }

   if (audio_st->out_channels <= 2 || !audio_st->upmix_buf)
      return audio_driver_dev_write(audio_st, audio, stereo,
            frames * 2 * sample, wait_room);

   if (frames > audio_st->upmix_frames)
      frames = audio_st->upmix_frames;
   /* An int16 mix into an int16 device stays int16 through the upmix:
    * the fixed-point form, so the all-integer pipeline never rounds
    * through float on the way out. It was widened to float, upmixed
    * and narrowed again - two conversions and a rounding for nothing. */
   if (!stereo_is_float && !dev_float)
   {
      audio_upmix_process_s16(&audio_st->upmix, audio_st->upmix_i16,
            (const int16_t*)stereo, frames);
      if (audio_st->extra.out_frames >= frames && audio_st->extra.channels)
      {
         if (audio_st->extra.res_int16)
            audio_driver_extra_merge_s16(audio_st, audio_st->upmix_i16, frames);
         else
         {
            /* extras resampled as float behind an int16 front: narrow */
            convert_float_to_s16(audio_st->extra.out_i, audio_st->extra.out_f, frames * audio_st->extra.channels);
            audio_driver_extra_merge_s16(audio_st, audio_st->upmix_i16, frames);
         }
         audio_st->extra.out_frames = 0;
      }
      return audio_driver_dev_write(audio_st, audio, audio_st->upmix_i16,
            frames * audio_st->out_channels * sizeof(int16_t), wait_room);
   }
   if (!stereo_is_float)
   {
      /* Narrowed stereo from the int16 path into a float device: the
       * one conversion the mixed formats need, into the head of the
       * int16 scratch reinterpreted as float (frames * channels int16
       * is frames * channels / 2 floats, enough for stereo when
       * channels >= 4), then upmixed from there. */
      float *stage = (float*)audio_st->upmix_i16;
      convert_s16_to_float(stage, (const int16_t*)stereo, frames * 2, 1.0f);
      audio_upmix_process(&audio_st->upmix, audio_st->upmix_buf, stage, frames);
   }
   else
      audio_upmix_process(&audio_st->upmix, audio_st->upmix_buf,
            (const float*)stereo, frames);

   if (audio_st->extra.out_frames >= frames && audio_st->extra.channels)
   {
      if (!audio_st->extra.res_int16)
         audio_driver_extra_merge_f32(audio_st, audio_st->upmix_buf, frames);
      else
      {
         convert_s16_to_float(audio_st->extra.out_f, audio_st->extra.out_i, frames * audio_st->extra.channels, 1.0f);
         audio_driver_extra_merge_f32(audio_st, audio_st->upmix_buf, frames);
      }
      audio_st->extra.out_frames = 0;
   }
   if (dev_float)
      return audio_driver_dev_write(audio_st, audio, audio_st->upmix_buf,
            frames * audio_st->out_channels * sizeof(float), wait_room);
   /* a float mix into an int16 device: the one narrowing it needs */
   convert_float_to_s16(audio_st->upmix_i16, audio_st->upmix_buf,
         frames * audio_st->out_channels);
   return audio_driver_dev_write(audio_st, audio, audio_st->upmix_i16,
         frames * audio_st->out_channels * sizeof(int16_t), wait_room);
}

/* The three software flush exits pass persistent stereo output scratch.
 * Resolve the actual device buffer only when a threaded write was short. */
static INLINE void audio_driver_retain_output(audio_driver_state_t *audio_st,
      const void *stereo, size_t frames, ssize_t written)
{
   size_t bytes = frames * audio_driver_dev_frame_bytes(audio_st);
   if (written >= 0 && (size_t)written < bytes && audio_st->pipe_threaded)
   {
      size_t sample = (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_USE_FLOAT)
         ? sizeof(float) : sizeof(int16_t);
      unsigned channels = 2;
      const uint8_t *data = (const uint8_t*)stereo;
      if ((audio_st->out_channels <= 2 && audio_st->virtualize && audio_st->virt_buf)
            || (audio_st->out_channels > 2 && audio_st->upmix_buf))
      {
         if (frames > audio_st->upmix_frames) frames = audio_st->upmix_frames;
         channels = audio_st->out_channels > 2 ? audio_st->out_channels : 2;
         data = (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_USE_FLOAT)
            ? (const uint8_t*)audio_st->upmix_buf : (const uint8_t*)audio_st->upmix_i16;
      }
      bytes = frames * channels * sample;
      if ((size_t)written < bytes)
      {
         audio_st->pipe_pending = data + written;
         audio_st->pipe_pending_bytes = bytes - written;
      }
   }
}

/* Copy or sanitize in place without modifying borrowed core/ring input. */
static void audio_driver_copy_clamp(float *buf, const float *input,
      unsigned total_samples)
{
   unsigned i              = 0;

#if (defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(HAVE_NEON))
   if (clamp_float_neon_enabled)
   {
      float32x4_t vpos1 = vdupq_n_f32( 1.0f);
      float32x4_t vneg1 = vdupq_n_f32(-1.0f);
      uint32x4_t  vabsm = vdupq_n_u32(0x7FFFFFFFu);
      uint32x4_t  vinfb = vdupq_n_u32(0x7F800000u);
      for (; i + 8 <= total_samples; i += 8)
      {
         float32x4_t v0 = vld1q_f32(input + i);
         float32x4_t v1 = vld1q_f32(input + i + 4);
         /* Squash NaN to silence first: vmin/vmax propagate a NaN
          * rather than clamping it, so without this it would reach
          * the driver untouched.
          *
          * The test is on the bit pattern (NaN iff the exponent is
          * all ones and the mantissa is non-zero, i.e. the value
          * masked of its sign exceeds the +inf encoding) rather
          * than vceqq_f32(v, v): -Ofast (HAVE_C_A7A7) implies
          * -ffinite-math-only, under which GCC folds the float
          * compare to all-ones and drops the mask entirely.
          * Integer compares are immune to that. */
         {
            uint32x4_t b0 = vreinterpretq_u32_f32(v0);
            uint32x4_t b1 = vreinterpretq_u32_f32(v1);
            v0 = vreinterpretq_f32_u32(vandq_u32(b0,
                  vcleq_u32(vandq_u32(b0, vabsm), vinfb)));
            v1 = vreinterpretq_f32_u32(vandq_u32(b1,
                  vcleq_u32(vandq_u32(b1, vabsm), vinfb)));
         }
         v0             = vminq_f32(v0, vpos1);
         v0             = vmaxq_f32(v0, vneg1);
         v1             = vminq_f32(v1, vpos1);
         v1             = vmaxq_f32(v1, vneg1);
         vst1q_f32(buf + i,     v0);
         vst1q_f32(buf + i + 4, v1);
      }
   }
#elif defined(__SSE__)
   {
      __m128 vpos1 = _mm_set1_ps( 1.0f);
      __m128 vneg1 = _mm_set1_ps(-1.0f);
      for (; i + 4 <= total_samples; i += 4)
      {
         __m128 v = _mm_loadu_ps(input + i);
         /* Squash NaN to silence first. _mm_min_ps/_mm_max_ps
          * return their second operand when either input is NaN,
          * which would silently turn a NaN into full scale here.
          * _mm_cmpord_ps(v, v) is all-zero only for NaN lanes, and
          * unlike a plain float compare it survives -ffast-math
          * because it lowers to cmpordps directly. Kept as an SSE1
          * op so the SSE-without-SSE2 build still compiles. */
         v        = _mm_and_ps(v, _mm_cmpord_ps(v, v));
         v        = _mm_min_ps(v, vpos1);
         v        = _mm_max_ps(v, vneg1);
         _mm_storeu_ps(buf + i, v);
      }
   }
#endif
   for (; i < total_samples; i++)
   {
      /* NaN fails both ordered comparisons below, so test for it
       * explicitly. The test is done on the bit pattern rather than
       * as (v != v) because -Ofast (HAVE_C_A7A7) implies -ffast-math,
       * under which the compiler is entitled to fold that to false.
       * Zero matches wav_to_s16's handling of non-finite input. */
      uint32_t bits;
      buf[i] = input[i];
      memcpy(&bits, &buf[i], sizeof(bits));
      if      ((bits & 0x7FFFFFFFu) > 0x7F800000u)
         buf[i] =  0.0f;
      else if (buf[i] >  1.0f)
         buf[i] =  1.0f;
      else if (buf[i] < -1.0f)
         buf[i] = -1.0f;
   }
}

/* The pause ramp runs on whichever buffer is about to reach the device:
 * float on the resampler path, int16 on the deterministic one. History is
 * kept in normalised float so the tail can go back out through
 * audio_driver_write_float(). */
static float audio_driver_pause_read(const void *buf, bool is_float, size_t i)
{
   if (is_float)
      return ((const float*)buf)[i];
   return (float)((const int16_t*)buf)[i] * (1.0f / 0x8000);
}

static void audio_driver_pause_write(void *buf, bool is_float, size_t i,
      float v)
{
   if (is_float)
   {
      ((float*)buf)[i] = v;
      return;
   }
   ((int16_t*)buf)[i] = (int16_t)audio_float_to_s16_sat(v);
}

/**
 * audio_driver_pause_track:
 * @buf        : interleaved stereo, the last buffer before the device.
 * @num_frames : frame count in @buf.
 * @is_float   : whether @buf is float rather than int16.
 *
 * Applies the resume ramp if one is outstanding, drops what a pause left
 * in the pipeline, and keeps a copy of the tail for the pause concealment
 * to continue from. Belongs on the last buffer before the device so it
 * sees exactly what is played. See audio_driver_pause_fade().
 **/
static void audio_driver_pause_track(audio_driver_state_t *audio_st,
      void *buf, size_t num_frames, bool is_float)
{
   if (num_frames < 1)
      return;

   if (audio_st->fade_in_frames > 0)
   {
      size_t n = (audio_st->fade_in_frames < num_frames)
            ? audio_st->fade_in_frames : num_frames;
      size_t j;
      for (j = 0; j < n; j++)
      {
         /* Progress against the whole ramp, not what is left of it: the
          * counter falls as the ramp is spent, so measuring from it would
          * restart the gain near zero at the head of every buffer. */
         unsigned done  = AUDIO_PAUSE_TAIL_FRAMES
               - audio_st->fade_in_frames + (unsigned)j + 1;
         float    g     = 0.5f * (1.0f - cosf((float)M_PI * (float)done
               / (float)AUDIO_PAUSE_TAIL_FRAMES));
         audio_driver_pause_write(buf, is_float, (j * 2) + 0,
               audio_driver_pause_read(buf, is_float, (j * 2) + 0) * g);
         audio_driver_pause_write(buf, is_float, (j * 2) + 1,
               audio_driver_pause_read(buf, is_float, (j * 2) + 1) * g);
      }
      audio_st->fade_in_frames -= (unsigned)n;
   }

   /* Drop what the pipeline was still holding when the pause tail was
    * written straight to the device ahead of it. */
   if (audio_st->pause_mute_frames > 0)
   {
      size_t n = (audio_st->pause_mute_frames < num_frames)
            ? audio_st->pause_mute_frames : num_frames;
      memset(buf, 0, n * 2 * (is_float ? sizeof(float) : sizeof(int16_t)));
      audio_st->pause_mute_frames -= (unsigned)n;
   }

   audio_st->last_out[0] = audio_driver_pause_read(buf, is_float,
         ((num_frames - 1) * 2) + 0);
   audio_st->last_out[1] = audio_driver_pause_read(buf, is_float,
         ((num_frames - 1) * 2) + 1);

   /* Keep the tail of what is about to be played, for the pause
    * concealment to continue from. */
   {
      size_t first = 0;
      size_t n     = num_frames;
      size_t j;
      if (n > AUDIO_PAUSE_HIST_FRAMES)
      {
         first = n - AUDIO_PAUSE_HIST_FRAMES;
         n     = AUDIO_PAUSE_HIST_FRAMES;
      }
      for (j = 0; j < n; j++)
      {
         audio_st->pause_hist[(audio_st->pause_hist_pos * 2) + 0] =
               audio_driver_pause_read(buf, is_float, ((first + j) * 2) + 0);
         audio_st->pause_hist[(audio_st->pause_hist_pos * 2) + 1] =
               audio_driver_pause_read(buf, is_float, ((first + j) * 2) + 1);
         audio_st->pause_hist_pos =
               (audio_st->pause_hist_pos + 1) % AUDIO_PAUSE_HIST_FRAMES;
      }
      audio_st->pause_hist_fill =
            (audio_st->pause_hist_fill + (unsigned)n
                  > AUDIO_PAUSE_HIST_FRAMES)
            ? AUDIO_PAUSE_HIST_FRAMES
            : audio_st->pause_hist_fill + (unsigned)n;
   }
}

/**
 * audio_driver_resume_topup:
 *
 * Fills the device with silence before the first buffer after a resume.
 * The runloop hands nothing over between the last write of a pause and the
 * first of resumed core audio, so the device drains, and rate control
 * refills it too slowly to keep the driver's pulls whole.
 *
 * On the first flush rather than at the resume, so it lands after that hole
 * whatever its length; the surplus is what rate control already takes out.
 * Chunked against a driver's per-write limit and bounded by write_avail(),
 * so a blocking write cannot wait on it.
 **/
static void audio_driver_resume_topup(audio_driver_state_t *audio_st)
{
   const audio_driver_t *audio = audio_st->current_audio;
   static const float zeros[1024 * 2];
   size_t room;

   audio_st->resume_topup_pending = false;
   if (     !audio || !audio->write || !audio->write_avail
         || !audio_st->context_audio_data
         || audio_st->buffer_size == 0)
      return;
   room = audio->write_avail(audio_st->context_audio_data);
   while (room > 0)
   {
      size_t n = (room < sizeof(zeros)) ? room : sizeof(zeros);
      if (audio->write(audio_st->context_audio_data, zeros, n) <= 0)
         break;
      room -= n;
   }
}

static void audio_driver_flush(audio_driver_state_t *audio_st,
      float slowmotion_ratio,
      const void *data, size_t samples, bool is_float,
      bool is_slowmotion, bool is_fastforward)
{
   struct resampler_data src_data;
   bool output_sanitized          = false;
   int snap                       = retro_atomic_load_acquire_int(
         &audio_st->runloop_snapshot);
   bool ff_speedup                = (snap & AUDIO_SNAP_FF_SPEEDUP) != 0;
   const audio_driver_t *audio    = audio_st->current_audio;
   float audio_volume_gain        =
         (audio_st->mute_enable || AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_MUTED)
               ? 0.0f
               : audio_st->volume_gain;

   /* Record the core's delivered sample format for the statistics overlay. */
   audio_st->stat_core_is_float = is_float;

   if (audio_st->resume_topup_pending)
      audio_driver_resume_topup(audio_st);

   /* Fast path: if driver handles resampling and no DSP/mixer is active,
    * bypass software resampling entirely. An active in-process MIDI synth
    * also disables the fast path, since its PCM is mixed into the float
    * buffer below (which the fast path would skip).
    * The fast path feeds int16 directly to audio->write_raw, so it is
    * only available when the core delivered int16; float-native cores
    * fall through to the float resampler path below (which is exactly
    * where the redundant int16<->float round-trip is avoided). */
   if (audio->write_raw
         && !audio_st->resampler_hq
         && !is_float
         /* The resume ramp and the pause mute are applied on the last
          * buffer before the device, which this path never sees; take
          * the normal path until they are spent. */
         && !audio_st->fade_in_frames
         && !audio_st->pause_mute_frames
         /* The raw path hands the driver stereo int16 as it is: no
          * upmix, no headphone render. Only when the output is plain
          * stereo. */
         && audio_st->out_channels <= 2
         && !audio_st->virtualize
         && !midi_driver_synth_active()
#ifdef HAVE_DSP_FILTER
         && !audio_st->dsp
#endif
#ifdef HAVE_AUDIOMIXER
         && audio_st->mixer_streams_playing == 0
#endif
      )
   {
      size_t frames                  = (unsigned)(samples >> 1);
      double rate_adjust             = 1.0;
      unsigned input_rate            = (unsigned)audio_st->input;

      /* Rate control for A/V sync. The DRC compute is gated on a
       * sample-count threshold so multi-batch cores don't fire it on
       * every batch_cb; intermediate calls reuse the cached factor.
       * Single-batch cores cross the threshold on the first call so
       * behaviour is unchanged for them. */
      if (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_CONTROL)
      {
         audio_st->samples_since_drc += samples;
         if (     audio_st->drc_pending
               || audio_st->samples_since_drc >= audio_st->drc_threshold_int16s)
         {
            rate_adjust           = audio_driver_compute_rate_adjust(audio_st);
            audio_st->drc_pending = false;
         }
         else
            rate_adjust = audio_st->cached_rate_adjust;
      }

      if (is_slowmotion)
         rate_adjust                *= slowmotion_ratio;

      /* Nothing at the output rate passes through here to keep as history,
       * so the pause tail has nothing true to continue from. Forget what
       * the normal path last recorded rather than let a later pause repeat
       * audio from long ago. */
      audio_st->last_out[0]      = 0.0f;
      audio_st->last_out[1]      = 0.0f;
      audio_st->pause_hist_pos   = 0;
      audio_st->pause_hist_fill  = 0;

      /* Driver-side SRC needs the same speed multiplier as software SRC.
       * In the threaded path this reads the producer's cadence estimate. */
      if (!is_fastforward && !audio_st->pipe_threaded)
         audio_driver_ff_mult_reset(audio_st);
      if (is_fastforward && ff_speedup)
         rate_adjust *= audio_driver_ff_mult(audio_st, frames);

      /* Note: mute/volume is not applied here.  Per the write_raw
       * contract in audio_driver.h the driver MUST apply the passed
       * gain (0.0 when muted) to its output - a driver that drops it
       * silently disables the volume and mute settings. */
      audio_st->stat_frontend_is_float = false;
      {
         ssize_t  w    = audio->write_raw(audio_st->context_audio_data,
               data, frames, input_rate, rate_adjust, audio_volume_gain);
         /* The sink estimate, which this path was not feeding at all:
          * offered went uncounted and sink_update() was never called,
          * so on any driver taking the fast path the estimator sat
          * inert - no Sink row, no measurement, and no sign that it was
          * doing nothing.
          *
          * The driver does the resampling here, so what it will hand
          * the device is the frames given scaled by the rate ratio and
          * by rate control's adjustment. sink_offered wants that with
          * the adjustment divided out, as the other write sites do, so
          * the bias measures the clocks and not rate control's own
          * corrections. write_raw returns accepted input frames; scale
          * those to output frames as well before comparing the counters.
          *
          * The bias reaches the driver only through the rate_adjust
          * above, which compute_rate_adjust() multiplies it into - so
          * with rate control off it is measured and shown but not
          * applied, exactly as it is for a blocking writer. */
         unsigned out_rate = audio_st->out_rate;
         if (out_rate && input_rate)
         {
            double nominal = (double)frames * (double)out_rate
                  / (double)input_rate;
            uint64_t offered = (uint64_t)(nominal * rate_adjust);
            audio_st->sink_offered_raw += offered;
            if (!audio_st->pipe_threaded)
               audio_st->sink_offered  += nominal;
            if (w > 0)
               /* Reuse the full-write count, including identical rounding. */
               audio_st->sink_accepted += (size_t)w == frames ? offered
                  : (uint64_t)((double)w * (double)out_rate
                        / (double)input_rate * rate_adjust);
         }
      }
      audio_driver_sink_refused(audio_st);
      if (!audio_st->pipe_threaded)
         audio_driver_sink_update(audio_st, cpu_features_get_time_usec());
      return;
   }

   /* Deterministic integer (s16) path.
    *
    * When enabled by the 'Resample to Fixed Integer' hint, the core delivered
    * int16, and the selected resampler has an int16 variant (sinc, nearest,
    * or CC), resample s16 -> s16 directly.  Volume is applied in fixed point
    * (Q16), so non-unity gain no
    * longer forces the float path; an int16-capable DSP chain runs in the
    * integer domain, and the audio mixer is summed in float on top of the
    * resampled game audio in both output branches below.  Fast-forward
    * "speedup" pitch tracking is applied to the integer ratio too, so it no
    * longer forces the float path.  An in-process MIDI synth (whose PCM is
    * float) no longer forces it either: its samples are converted to s16 and
    * summed into the game audio before the integer DSP/resample, exactly where
    * the float path sums them.  This removes the s16<->float round-trip on the
    * game signal and is bit-identical across platforms for that signal
    * (reproducible output for validation and regression comparison).  Any
    * condition failing falls through to the float path below.
    *
    * This buys determinism, not speed, and on hosts with a vector FPU it
    * costs speed.  The float sinc driver is SIMD (SSE/NEON/AltiVec) while
    * the int16 one is scalar, so on desktop x86 this path measures roughly
    * twice the cost of the float path for the same output - about 30 us
    * against 15 us for a 512-frame batch at 32 kHz -> 48 kHz.  It wins where
    * there is no usable FPU, or with a cheap kernel: the nearest driver
    * measures ~2.0 us here against ~3.5 us on the float path.  Pick it for
    * reproducibility (netplay, rewind, regression comparison) or for
    * soft-float targets, not as a general optimisation. */
   {
      static int audio_i16_path_logged = -1;
      bool use_i16 = audio_driver_mixer_use_s16(is_float);

      if (audio_i16_path_logged != (int)use_i16)
      {
         const char *rs_ident =
               (audio_st->resampler && audio_st->resampler->short_ident)
                     ? audio_st->resampler->short_ident : "resampler";
         RARCH_LOG("[Audio] %s resampler active path: %s\n",
               rs_ident,
               use_i16 ? "integer s16 (no float round-trip)" : "float");
         audio_i16_path_logged = (int)use_i16;
      }

      if (use_i16)
      {
         struct resampler_data_int16 s16;
         const int16_t *rs_in = (const int16_t*)data;
         unsigned rs_frames   = (unsigned)(samples >> 1);
         double   i16_ratio;
         unsigned out_frames;
         bool     synth_on    = midi_driver_synth_active()
               && audio_st->synth_buf && audio_st->input_data_int16;
         /* Writable view of the input for the synth sum and the DSP
          * chain. The single-sample accumulator is this driver's own
          * buffer and is discarded when the flush returns, so it can be
          * modified in place; any other source is a core's const buffer
          * and is copied into the scratch on first use. */
         int16_t *work        = (rs_in == audio_st->sample_accum)
               ? audio_st->sample_accum : NULL;

         audio_st->stat_frontend_is_float = false;

         /* If an in-process synth is sounding, render its PCM at the input
          * rate and sum it (converted to s16, saturating) into a writable
          * copy of the game audio before DSP/resampling - mirroring the float
          * path, which sums the synth into its pre-resample buffer.  The synth
          * is added unscaled; the output stage applies the master volume gain
          * to the combined signal, matching the float path's (game+synth)*gain
          * ordering. */
         if (synth_on)
         {
            if (!work)
            {
               memcpy(audio_st->input_data_int16, data,
                     samples * sizeof(int16_t));
               work = audio_st->input_data_int16;
            }
            if (midi_driver_render_audio(audio_st->synth_buf, rs_frames,
                     (unsigned)audio_st->input))
            {
               size_t s;
               for (s = 0; s < samples; s++)
               {
                  int32_t v  = (int32_t)work[s]
                            + audio_float_to_s16_sat(
                                 audio_st->synth_buf[s]);
                  if      (v >  32767) v =  32767;
                  else if (v < -32768) v = -32768;
                  work[s] = (int16_t)v;
               }
            }
            rs_in = work;
         }

#ifdef HAVE_DSP_FILTER
         /* Run an int16-capable DSP chain in place on the writable view,
          * then resample its output.  When a synth was summed above the
          * view already holds game+synth. */
         if (audio_st->dsp && audio_st->input_data_int16)
         {
            struct retro_dsp_data_int16 dsp_data;
            if (!work)
            {
               memcpy(audio_st->input_data_int16, data,
                     samples * sizeof(int16_t));
               work = audio_st->input_data_int16;
            }
            dsp_data.input        = work;
            dsp_data.input_frames = rs_frames;
            dsp_data.output       = NULL;
            dsp_data.output_frames = 0;
            retro_dsp_filter_process_int16(audio_st->dsp, &dsp_data);
            rs_in     = dsp_data.output;
            rs_frames = dsp_data.output_frames;
         }
#endif

         /* Rate control - identical to the float resampler path below. */
         if (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_CONTROL)
         {
            audio_st->samples_since_drc += samples;
            if (     audio_st->drc_pending
                  || audio_st->samples_since_drc >= audio_st->drc_threshold_int16s)
            {
               audio_st->src_ratio_curr = audio_st->src_ratio_orig *
                     audio_driver_compute_rate_adjust(audio_st);
               audio_st->drc_pending    = false;
            }
         }
         /* Without rate control nothing else sets the ratio: the
          * sink bias is applied here, on the thread that resamples. */
         if (!(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_CONTROL))
            audio_st->src_ratio_curr = audio_st->src_ratio_orig
                  * audio_driver_sink_bias(audio_st);
         if (!is_fastforward && !audio_st->pipe_threaded)
            audio_driver_ff_mult_reset(audio_st);
         i16_ratio = audio_driver_effective_ratio(audio_st, is_slowmotion,
               slowmotion_ratio,
               (is_fastforward && ff_speedup)
                  ? audio_driver_ff_mult(audio_st, rs_frames) : 1.0);
         /* Without speedup the device is pinned near full and the
          * write below drops most of the output; resample only what
          * it can accept.  See audio_driver_ff_discard_bound. */
         if (is_fastforward && !ff_speedup)
            rs_frames = (unsigned)audio_driver_ff_discard_bound(
                  audio_st, i16_ratio, rs_frames);

         /* The int16 resampler writes to output_samples_int16 with no
          * capacity argument; bound the ratio to what that buffer holds. */
         i16_ratio         = audio_driver_bound_ratio(i16_ratio, rs_frames,
               audio_st->output_samples_int16_length
                     / (2 * sizeof(int16_t)));

         s16.data_in       = rs_in;
         s16.data_out      = audio_st->output_samples_int16;
         s16.input_frames  = rs_frames;
         s16.output_frames = 0;
         s16.ratio         = i16_ratio;
         audio_st->resampler_int16_process(audio_st->resampler_data_int16, &s16);
         audio_driver_extra_resample(audio_st, i16_ratio, rs_frames, false, true);

         out_frames = (unsigned)s16.output_frames;

         if (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_USE_FLOAT)
         {
            /* Float-output driver: fold volume into the single s16 -> float
             * pass at the output rate.  The integer resampler already
             * saturated to the s16 range. */
            convert_s16_to_float(audio_st->output_samples_buf,
                  audio_st->output_samples_int16, out_frames * 2,
                  audio_volume_gain);
#ifdef HAVE_AUDIOMIXER
            /* Sum the mixer voices in float on top of the (already
             * volume-scaled) game audio, mirroring the float path: game gets
             * the master gain above, voices get mixer_gain here, and
             * audio_mixer_mix() clamps the summed buffer as its final step. */
            if (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_MIXER_ACTIVE)
            {
               bool override                       = true;
               float mixer_gain                    = 0.0f;
               bool audio_driver_mixer_mute_enable = audio_st->mixer_mute_enable;

               if (!audio_driver_mixer_mute_enable)
               {
                  if (audio_st->mixer_volume_gain == 1.0f)
                     override                      = false;
                  mixer_gain                       = audio_st->mixer_volume_gain;
               }
               /* s16 voices (the common case on this int16 path): fold onto
                * the float game buffer first so audio_mixer_mix's final clamp
                * covers them.  output_samples_int16 is free here -- the game
                * s16 was already converted to float above. */
               if (audio_mixer_has_s16_voices())
                  audio_mixer_fold_s16_voices_into_float(
                        audio_st->output_samples_buf,
                        audio_st->output_samples_int16, out_frames,
                        mixer_gain, override);
               audio_mixer_mix(audio_st->output_samples_buf,
                     out_frames, mixer_gain, override);
            }
#endif
            audio_driver_pause_track(audio_st,
                  audio_st->output_samples_buf, out_frames, true);
            AUDIO_FLAGS_SET(audio_st, AUDIO_FLAG_WROTE);
            {
               ssize_t w = audio_driver_write_frames(audio_st, audio,
                     audio_st->output_samples_buf, out_frames, true, true);
               audio_driver_retain_output(audio_st, audio_st->output_samples_buf, out_frames, w);
               audio_st->sink_offered_raw += out_frames;
               if (!audio_st->pipe_threaded)
                  audio_st->sink_offered  += (double)out_frames * audio_st->src_ratio_orig / audio_st->src_ratio_curr;
               if (w > 0)
                  audio_st->sink_accepted += (uint64_t)w / audio_driver_dev_frame_bytes(audio_st);
            }
            audio_driver_sink_refused(audio_st);
            if (!audio_st->pipe_threaded)
               audio_driver_sink_update(audio_st, cpu_features_get_time_usec());
         }
         else
         {
            /* s16-output driver: apply volume as a deterministic Q16 gain
             * (round half away from zero on the shift, saturate).
             *
             * The +0x8000 bias before the shift makes this round to
             * nearest rather than toward zero.  Truncating here costs
             * ~6 dB of quantisation noise (the error is uniform over
             * two LSBs instead of one) and opens a deadband of
             * 2*(65536/gain_q16)-1 input codes around silence, which
             * swallows low-level detail at heavy attenuation.
             *
             * Mirroring the bias across the sign keeps the quantiser
             * odd-symmetric, so it stays DC-free on symmetric signals
             * and remains bit-exact reproducible - the s16 path's
             * determinism guarantee for netplay/rewind is unaffected,
             * this is integer arithmetic either way.
             *
             * No overflow risk: the largest |p| is 32768 * gain_q16,
             * and gain_q16 tops out at 260904 (+12 dB), so |p| stays
             * under 2^33. */
            if (audio_volume_gain != 1.0f)
            {
               int32_t  gain_q16 = (int32_t)(audio_volume_gain * 65536.0f + 0.5f);
               unsigned k;
               unsigned total    = out_frames * 2;
               int16_t *ob       = audio_st->output_samples_int16;
               for (k = 0; k < total; k++)
               {
                  int64_t p = (int64_t)ob[k] * gain_q16;
                  int64_t v = (p >= 0)
                        ?  ((  p + 0x8000) >> 16)
                        : -(((-p + 0x8000) >> 16));
                  if      (v >  32767) v =  32767;
                  else if (v < -32768) v = -32768;
                  ob[k] = (int16_t)v;
               }
            }
#ifdef HAVE_AUDIOMIXER
            /* Sum the mixer voices onto the int16 game audio, in the integer
             * domain.  Voices played on the s16 path (the common case here,
             * since the game itself is on the int16 path) are summed
             * directly via audio_mixer_mix_s16 with saturating add -- no float
             * scratch, no round-trip.  Any voice that happens to be float (a
             * voice that outlived a pipeline-format change) is folded via
             * audio_mixer_mix into output_samples_buf and quantised on top,
             * matching convert_float_to_s16's rounding. */
            if (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_MIXER_ACTIVE)
            {
               bool     override                   = true;
               float    mixer_gain                 = 0.0f;
               bool audio_driver_mixer_mute_enable  = audio_st->mixer_mute_enable;
               int16_t *ob                         = audio_st->output_samples_int16;

               if (!audio_driver_mixer_mute_enable)
               {
                  if (audio_st->mixer_volume_gain == 1.0f)
                     override                      = false;
                  mixer_gain                       = audio_st->mixer_volume_gain;
               }

               audio_mixer_mix_s16(ob, out_frames,
                     GAIN_TO_Q16(mixer_gain), override);
               if (audio_mixer_has_float_voices())
                  audio_mixer_fold_float_voices_into_s16(ob,
                        audio_st->output_samples_buf, out_frames,
                        mixer_gain, override);
            }
#endif
            audio_driver_pause_track(audio_st,
                  audio_st->output_samples_int16, out_frames, false);
            AUDIO_FLAGS_SET(audio_st, AUDIO_FLAG_WROTE);
            {
               ssize_t w = audio_driver_write_frames(audio_st, audio,
                     audio_st->output_samples_int16, out_frames, false, true);
               audio_driver_retain_output(audio_st, audio_st->output_samples_int16, out_frames, w);
               audio_st->sink_offered_raw += out_frames;
               if (!audio_st->pipe_threaded)
                  audio_st->sink_offered  += (double)out_frames * audio_st->src_ratio_orig / audio_st->src_ratio_curr;
               if (w > 0)
                  audio_st->sink_accepted += (uint64_t)w / audio_driver_dev_frame_bytes(audio_st);
            }
            audio_driver_sink_refused(audio_st);
            if (!audio_st->pipe_threaded)
               audio_driver_sink_update(audio_st, cpu_features_get_time_usec());
         }
         return;
      }
   }

   src_data.data_out                 = NULL;
   src_data.output_frames            = 0;
   /* We'll assign a proper output to the resampler later in this function */

   /* Reached only when neither integer path above returned: the core is
    * float-native, or an int16 core fell through (s16 path disabled, or a
    * float-only DSP/condition forced conversion). Either way the pipeline
    * runs in float from here. */
   audio_st->stat_frontend_is_float = true;

   /* Bring the core's audio into the float input buffer the resampler/DSP
    * operate on. For a float-native core this is just a gain-scaled copy
    * (or a plain copy at unity gain) - no int16<->float conversion - which
    * is the whole point of float audio negotiation. */
   /* Decide whether anything downstream actually needs to write to the
    * pre-resample buffer.  Only four things ever produce into input_data:
    * the s16 -> float conversion, a non-unity gain scale, the in-process
    * synth sum, and the DSP chain (which processes in place because the
    * core's buffer is const).  When none of them applies - a float-native
    * core at unity gain with no synth sounding and no DSP filter loaded -
    * the copy produces a byte-for-byte duplicate that the resampler only
    * ever reads, since struct resampler_data::data_in is const.  Point the
    * resampler at the core's buffer instead and skip a full pass over the
    * batch.  The resampler sees identical bytes either way, so output is
    * bit-exact.
    *
    * Nothing downstream requires the pointer to be aligned: every
    * resampler reads data_in scalar-wise or with unaligned loads, and so
    * does the bypass clamp. */
   {
      bool synth_on   = midi_driver_synth_active() && audio_st->synth_buf;
      bool copy_input = !is_float
            || (audio_volume_gain != 1.0f)
            || synth_on
#ifdef HAVE_DSP_FILTER
            || (audio_st->dsp != NULL)
#endif
            ;

      if (!copy_input)
         src_data.data_in            = (const float*)data;
      else
      {
         if (is_float)
         {
            const float *fin = (const float*)data;
            if (audio_volume_gain == 1.0f)
               memcpy(audio_st->input_data, fin, samples * sizeof(float));
            else
            {
               size_t s;
               for (s = 0; s < samples; s++)
                  audio_st->input_data[s] = fin[s] * audio_volume_gain;
            }
         }
         else
            convert_s16_to_float(audio_st->input_data,
                  (const int16_t*)data, samples, audio_volume_gain);

         /* Mix in-process MIDI synth output (e.g. fmsynth) into the float
          * input before resampling, so it shares the core's resampler, gain
          * and output path. No-op unless a render-capable MIDI driver is
          * active and sounding.  audio_st->synth_buf is allocated to the
          * same size as input_data. */
         if (synth_on)
         {
            size_t frames = samples >> 1;
            if (midi_driver_render_audio(audio_st->synth_buf, frames,
                     (unsigned)audio_st->input))
            {
               size_t s;
               for (s = 0; s < samples; s++)
                  audio_st->input_data[s] +=
                        audio_st->synth_buf[s] * audio_volume_gain;
            }
         }

         src_data.data_in            = audio_st->input_data;
      }
   }

   src_data.input_frames             = samples >> 1;

   /* Remember, we allocated buffers that are twice as big as needed.
    * (see audio_driver_init) */

#ifdef HAVE_DSP_FILTER
   /* If we want to process our audio for reasons besides resampling... */
   if (audio_st->dsp)
   {
      struct retro_dsp_data dsp_data;

      dsp_data.input                 = audio_st->input_data;
      dsp_data.input_frames          = (unsigned)(samples >> 1);
      dsp_data.output                = NULL;
      dsp_data.output_frames         = 0;

      /* Initialize the DSP input/output.
       * Our DSP implementations generally operate directly on the
       * input buffer, so the output/output_frames attributes here are zero;
       * the DSP filter will set them to useful values, most likely to be
       * the same as the inputs. */

      retro_dsp_filter_process(audio_st->dsp, &dsp_data);

      /* If the DSP filter succeeded... */
      if (dsp_data.output)
      {
         /* Then let's pass the DSP's output to the resampler's input */
         src_data.data_in            = dsp_data.output;
         src_data.input_frames       = dsp_data.output_frames;
      }
   }
#endif

   src_data.data_out                 = audio_st->output_samples_buf;

   /* Now the resampler will write to the driver state's scratch buffer */

   /* Readjust the audio input rate. Recomputed on the first flush after
    * audio_driver_frame_end() armed drc_pending, so the measurement lands
    * at the same point of every frame; the sample-count gate covers
    * producers that never mark a frame end (menu audio) and frames that
    * deliver more than one threshold's worth. Otherwise src_ratio_curr
    * retains its previous value and the resampler runs with it. */
   if (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_CONTROL)
   {
      audio_st->samples_since_drc += samples;
      if (     audio_st->drc_pending
            || audio_st->samples_since_drc >= audio_st->drc_threshold_int16s)
      {
         double adjust            = audio_driver_compute_rate_adjust(audio_st);
         audio_st->src_ratio_curr = audio_st->src_ratio_orig * adjust;
         audio_st->drc_pending    = false;

#if 0
         if (verbosity_is_enabled())
         {
            RARCH_LOG_OUTPUT("[Audio] Audio buffer is %u%% full\n",
                  (unsigned)(100 - (audio_st->free_samples_buf[
                     (audio_st->free_samples_count - 1)
                     & (AUDIO_BUFFER_FREE_SAMPLES_COUNT - 1)] * 100) /
                     audio_st->buffer_size));
            RARCH_LOG_OUTPUT("[Audio] New rate: %lf, Orig rate: %lf\n",
                  audio_st->src_ratio_curr,
                  audio_st->src_ratio_orig);
         }
#endif
      }
   }

   if (!(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_CONTROL))
      audio_st->src_ratio_curr = audio_st->src_ratio_orig
            * audio_driver_sink_bias(audio_st);
   if (!is_fastforward && !audio_st->pipe_threaded)
      audio_driver_ff_mult_reset(audio_st);
   src_data.ratio           = audio_driver_effective_ratio(audio_st,
         is_slowmotion, slowmotion_ratio,
         (is_fastforward && ff_speedup)
            ? audio_driver_ff_mult(audio_st, src_data.input_frames) : 1.0);

   if (is_fastforward)
   {
      /* Without speedup the device is pinned near full and the write
       * below drops most of the output; resample only what it can
       * accept.  See audio_driver_ff_discard_bound. */
      if (!ff_speedup)
         src_data.input_frames = audio_driver_ff_discard_bound(
               audio_st, src_data.ratio, src_data.input_frames);
   }

   /* Bound the ratio to what the output scratch holds.  The float result is
    * later narrowed into output_samples_int16 for s16 drivers, so take the
    * smaller of the two capacities rather than assuming they match. */
   {
      size_t cap_f = audio_st->output_samples_buf_length
            / (2 * sizeof(float));
      size_t cap_i = audio_st->output_samples_int16_length
            / (2 * sizeof(int16_t));

      src_data.ratio = audio_driver_bound_ratio(src_data.ratio,
            src_data.input_frames, (cap_f < cap_i) ? cap_f : cap_i);
   }

   /* Unity passthrough: with dynamic rate control disabled the resampler
    * ratio is static, and when it is exactly 1.0 (input rate == output
    * rate, no slow-motion / fast-forward pitch change) the resampler would
    * only apply its non-identity unity-ratio response for no benefit.
    * Copy the already gain/synth/DSP-processed input straight to the output
    * buffer instead, skipping the convolution (and, for int16 drivers, the
    * s16<->float round-trip performed in the write stage below).
    *
    * process() is skipped while passing through, so the resampler ring goes
    * stale; on the transition back to real resampling (slow-motion or fast-
    * forward engaged) re-initialise it first so it resumes from a clean
    * state rather than convolving new input against stale samples.  The
    * transition is rare and already an audio-discontinuity moment, so the
    * one-off realloc there is not on the steady-state hot path.
    *
    * Gated on DRC not actually adjusting: with rate control active and a
    * non-zero delta, src_ratio_curr is dithered every batch to track A/V
    * sync and is essentially never a stable 1.0, which would otherwise
    * toggle the bypass and thrash the resampler state.
    *
    * A zero delta is the exception, and it is a setting the user can reach:
    * the 'Dynamic Audio Rate Control' menu item is audio_rate_control_delta,
    * a float whose range starts at 0.000, and moving it to zero does not
    * clear AUDIO_FLAG_CONTROL - that flag comes from the separate
    * audio_rate_control bool.  At delta zero
    * audio_driver_compute_rate_adjust() returns 1.0 + 0 * direction, i.e.
    * exactly 1.0, so src_ratio_curr is pinned to src_ratio_orig and there is
    * nothing to dither.  Testing the flag alone therefore sent every
    * rate-matched user who had turned the slider down to zero through a full
    * sinc convolution for an identity resample: measured at 10.7 us per
    * 512-frame batch against 1.6 us for the bypass, on every flush.
    *
    * If the slider is moved back off zero the ratio starts moving, the
    * bypass stops being taken, and the existing resampler_bypassed
    * transition re-initialises the ring - the same path already used when
    * slow-motion or fast-forward engages. */
   if (     (   !(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_CONTROL)
             || audio_st->rate_control_delta == 0.0f)
         && src_data.ratio == 1.0
         && !audio_st->resampler_hq)
   {
      if ((AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_USE_FLOAT)
#ifdef HAVE_AUDIOMIXER
            && !(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_MIXER_ACTIVE)
#endif
         )
      {
         audio_driver_copy_clamp(audio_st->output_samples_buf,
               src_data.data_in, (unsigned)src_data.input_frames * 2);
         output_sanitized = true;
      }
      else
         memcpy(audio_st->output_samples_buf, src_data.data_in,
               src_data.input_frames * 2 * sizeof(float));
      src_data.output_frames       = src_data.input_frames;
      audio_st->resampler_bypassed = true;
   }
   else
   {
      if (audio_st->resampler_bypassed)
      {
         /* The history is stale after a bypass; forgotten in place
          * where the backend can, on this thread with no allocator,
          * or the state re-created where it cannot. */
         audio_st->resampler_bypassed = false;
         if (audio_st->resampler && audio_st->resampler->reset
               && audio_st->resampler_data)
            audio_st->resampler->reset(audio_st->resampler_data);
         else
            audio_driver_resampler_realloc(audio_st, audio_st->resampler_hq);
      }
      if (audio_st->resampler_data)
         audio_st->resampler->process(audio_st->resampler_data, &src_data);
      else
         src_data.output_frames = 0;
   }
   audio_driver_extra_resample(audio_st, src_data.ratio, src_data.input_frames,
         audio_st->resampler_bypassed, false);

#ifdef HAVE_AUDIOMIXER
   if (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_MIXER_ACTIVE)
   {
      bool override                       = true;
      float mixer_gain                    = 0.0f;
      bool audio_driver_mixer_mute_enable = audio_st->mixer_mute_enable;

      if (!audio_driver_mixer_mute_enable)
      {
         if (audio_st->mixer_volume_gain == 1.0f)
            override                      = false;
         mixer_gain                       = audio_st->mixer_volume_gain;

      }
      /* Fold any s16 voices (played while the int16 path was active, now on
       * the float path) onto the float buffer before the float mix, so the
       * clamp inside audio_mixer_mix covers them.  output_samples_int16 is
       * unused until the float->s16 driver conversion further below. */
      if (audio_mixer_has_s16_voices())
         audio_mixer_fold_s16_voices_into_float(
               audio_st->output_samples_buf,
               audio_st->output_samples_int16,
               (unsigned)src_data.output_frames,
               mixer_gain, override);
      audio_mixer_mix(audio_st->output_samples_buf,
            src_data.output_frames, mixer_gain, override);
   }
#endif

   /* Now we write our processed audio output to the driver.
    * It may not be played immediately, depending on
    * the driver implementation. */
   {
      const void *output_data = audio_st->output_samples_buf;
      unsigned output_frames  = (unsigned)src_data.output_frames; /* Unit: frames */
      AUDIO_OUTPUT_BOUND_CHECK(output_frames,
            audio_driver_output_bound(src_data.ratio, src_data.input_frames));

      /* Clamp float samples to [-1.0, 1.0] before writing to the
       * audio driver.  Three sources can produce out-of-range values:
       *
       *   1. Volume gain above 0 dB (up to +12 dB ≈ 3.98× multiplier).
       *   2. Sinc resampler overshoot on transients — Gibbs phenomenon
       *      causes up to ~28% overshoot on step edges even at unity gain.
       *   3. Mixer voice summation adding on top of core audio.
       *
       * Without clamping, float-output drivers (WASAPI, PulseAudio,
       * PipeWire, CoreAudio, JACK) pass these values straight to the
       * hardware.  The s16 path (convert_float_to_s16) saturates, but
       * the float path had no protection.
       *
       * When the audio mixer is active, audio_mixer_mix() unconditionally
       * clamps the entire buffer (core audio + voices) as its final step,
       * so an additional pass here would be redundant.  We rely on this
       * because audio_mixer_mix() is called on the same buffer immediately
       * above, with no intervening modifications.
       *
       * Likewise, when the active driver consumes int16 (the path below
       * via convert_float_to_s16), saturation is performed by the
       * conversion itself: the scalar fallback clamps to the s16 range,
       * and every SIMD variant (SSE2 _mm_packs_epi32, NEON vqmovn_s32,
       * AltiVec vec_packs, PSP/Allegrex vi2s.q) uses a saturating narrow.
       * Skipping the float-clamp pass on the s16 path produces a
       * bit-identical result with one fewer touch of the buffer. */
      if (!output_sanitized &&
            (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_USE_FLOAT)
#ifdef HAVE_AUDIOMIXER
         && !(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_MIXER_ACTIVE)
#endif
         )
      {
         audio_driver_copy_clamp(audio_st->output_samples_buf,
               audio_st->output_samples_buf, output_frames * 2);
      }

      /* Resume ramp, and remember where the waveform got to: both belong on
       * the last buffer before the device. See audio_driver_pause_fade(). */
      audio_driver_pause_track(audio_st, (void*)output_data, output_frames,
            true);

      /* A float driver takes the float mix as it is; an int16 one gets
       * it narrowed. A wider device gets either widened on the way. */
      if (!(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_USE_FLOAT))
      {
         convert_float_to_s16(audio_st->output_samples_int16,
               (const float*)output_data, output_frames * 2);
         output_data          = audio_st->output_samples_int16;
      }

      AUDIO_FLAGS_SET(audio_st, AUDIO_FLAG_WROTE);
      {
         size_t  fb = audio_driver_dev_frame_bytes(audio_st);
         ssize_t w  = audio_driver_write_frames(audio_st, audio, output_data,
               output_frames,
               (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_USE_FLOAT) != 0, true);
         audio_driver_retain_output(audio_st, output_data, output_frames, w);
         audio_st->sink_offered_raw += (uint64_t)output_frames;
         if (!audio_st->pipe_threaded)
            audio_st->sink_offered  += (double)output_frames * audio_st->src_ratio_orig / audio_st->src_ratio_curr;
         if (w > 0)
            audio_st->sink_accepted += (uint64_t)w / fb;
      }
      audio_driver_sink_refused(audio_st);
      if (!audio_st->pipe_threaded)
         audio_driver_sink_update(audio_st, cpu_features_get_time_usec());
   }
}

#ifdef HAVE_AUDIOMIXER
audio_mixer_stream_t *audio_driver_mixer_get_stream(unsigned i)
{
   if (i > (AUDIO_MIXER_MAX_SYSTEM_STREAMS-1))
      return NULL;
   return &audio_driver_st.mixer_streams[i];
}

const char *audio_driver_mixer_get_stream_name(unsigned i)
{
   if (i > (AUDIO_MIXER_MAX_SYSTEM_STREAMS-1))
      return msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE);
   if (audio_driver_st.mixer_streams[i].name && *audio_driver_st.mixer_streams[i].name)
      return audio_driver_st.mixer_streams[i].name;
   return msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE);
}

unsigned audio_driver_mixer_get_streams_playing(void)
{
   return audio_driver_st.mixer_streams_playing;
}

#endif

static bool audio_driver_multi_discrete(audio_driver_state_t *audio_st, unsigned layout);
static INLINE bool audio_driver_pipe_widen_prefix(void *output, const void *input,
      size_t frames, unsigned channels, unsigned layout, bool floating);

static void audio_driver_inline_pack(struct audio_inline_transport *t,
      const void *data, size_t frames, unsigned channels, unsigned layout)
{
   unsigned bit, count = 0, c, slots[AUDIO_PIPE_CANON_CHANNELS];
   size_t f, sample = t->floating ? sizeof(float) : sizeof(int16_t);
   if (audio_driver_pipe_widen_prefix(t->source, data, frames, channels, layout, t->floating))
      return;
   for (bit = 0; bit < t->channels; bit++)
      if (layout & (1u << bit)) slots[count++] = bit;
   memset(t->source, 0, frames * t->channels * sample);
   if (t->floating)
   {
      float *dst = (float*)t->source;
      const float *src = (const float*)data;
      for (f = 0; f < frames; f++)
         for (c = 0; c < count; c++)
            dst[f*t->channels+slots[c]] = src[f*channels+c];
   }
   else
   {
      int16_t *dst = (int16_t*)t->source;
      const int16_t *src = (const int16_t*)data;
      for (f = 0; f < frames; f++)
         for (c = 0; c < count; c++)
            dst[f*t->channels+slots[c]] = src[f*channels+c];
   }
}

static void audio_driver_inline_render(audio_driver_state_t *audio_st,
      const void *data, size_t frames, unsigned layout, float slowmotion_ratio,
      bool slowmotion, bool fastforward)
{
   struct audio_inline_transport *t = audio_st->inline_transport;
   const void *front = data;
   if (t->channels > 2)
   {
      unsigned positions = layout & ~AUDIO_LAYOUT_STEREO;
      unsigned ex = audio_layout_channels(positions), bit, c = 0;
      unsigned slots[AUDIO_PIPE_CANON_CHANNELS];
      size_t f;
      bool discrete = ex && audio_driver_multi_discrete(audio_st, layout)
         && audio_driver_extra_prepare(audio_st, ex, positions, AUDIO_CHUNK_SIZE_NONBLOCKING >> 1,
               t->floating, audio_driver_mixer_use_s16(t->floating));
      if (discrete) audio_st->extra.in_frames = frames;
      for (bit = 2; bit < t->channels; bit++)
         if (positions & (1u << bit)) slots[c++] = bit;
      if (discrete || layout == AUDIO_LAYOUT_STEREO)
      {
         if (t->floating)
         {
            const float *src = (const float*)data;
            float *dst = (float*)t->front;
            for (f = 0; f < frames; f++)
            {
               dst[2*f] = src[f*t->channels]; dst[2*f+1] = src[f*t->channels+1];
               if (discrete)
                  for (c = 0; c < ex; c++)
                     audio_st->extra.in_f[f*ex+c] = src[f*t->channels+slots[c]];
            }
         }
         else
         {
            const int16_t *src = (const int16_t*)data;
            int16_t *dst = (int16_t*)t->front;
            for (f = 0; f < frames; f++)
            {
               dst[2*f] = src[f*t->channels]; dst[2*f+1] = src[f*t->channels+1];
               if (discrete)
                  for (c = 0; c < ex; c++)
                     audio_st->extra.in_i[f*ex+c] = src[f*t->channels+slots[c]];
            }
         }
         audio_st->extra.pending = discrete;
      }
      else if (t->floating)
         audio_downmix_f32((float*)t->front, (const float*)data, frames,
               (1u << AUDIO_PIPE_CANON_CHANNELS) - 1, t->channels);
      else
         audio_downmix_s16((int16_t*)t->front, (const int16_t*)data, frames,
               (1u << AUDIO_PIPE_CANON_CHANNELS) - 1, t->channels);
      front = t->front;
   }
   audio_driver_flush(audio_st, slowmotion_ratio, front, frames * 2,
         t->floating, slowmotion, fastforward);
   audio_st->extra.pending = false;
}

/* Called with the audio state lock, before SRC and device output. */
#if defined(__GNUC__)
__attribute__((noinline))
#elif defined(_MSC_VER)
__declspec(noinline)
#endif
static bool audio_driver_inline_process(audio_driver_state_t *audio_st,
      float slowmotion_ratio, const void *data, size_t frames, bool floating,
      bool slowmotion, bool fastforward, unsigned layout)
{
   struct audio_inline_transport *t = audio_st->inline_transport;
   int snap = retro_atomic_load_acquire_int(&audio_st->runloop_snapshot);
   size_t left = frames;
   size_t frame;
   const uint8_t *source = (const uint8_t*)data;
   retro_time_t old_time = audio_st->last_flush_time;
   retro_time_t old_delta = audio_st->avg_flush_delta, source_time;
   double old_expected = audio_st->avg_expected_delta;
   double duration = slowmotion ? slowmotion_ratio : 1.0;
   uint64_t bits;
   uint32_t tempo, cutoff;
   bool supported, follow;
   if (!t) return false;
   follow = audio_driver_ff_follows(audio_st, (snap & AUDIO_SNAP_FF_SPEEDUP) != 0);
   frame = t->channels * (floating ? sizeof(float) : sizeof(int16_t));
   supported = floating == t->floating && !audio_st->extra.pending
      && audio_st->core_layout == layout
      && (layout & AUDIO_LAYOUT_STEREO) == AUDIO_LAYOUT_STEREO;
#ifdef HAVE_REWIND
   supported = supported && !state_manager_frame_is_reversed();
#endif
   if (supported && fastforward && follow)
      duration *= audio_driver_inline_ff_mult(audio_st, left);
   memcpy(&bits, &duration, sizeof(bits));
   supported = supported && bits < UINT64_C(0x7ff0000000000000)
      && duration >= 1.0 / 32.0 && duration <= 4.0;
   if (!supported)
   {
      audio_st->last_flush_time = old_time;
      audio_st->avg_flush_delta = old_delta;
      audio_st->avg_expected_delta = old_expected;
      if (!t->bypassed)
      {
         bool pending = audio_st->extra.pending;
         audio_driver_inline_reset(audio_st);
         audio_driver_reset_resamplers(audio_st);
         audio_st->extra.pending = pending;
         t->bypassed = true;
      }
      return false;
   }
   if (t->bypassed || t->layout != layout)
   {
      audio_driver_inline_reset(audio_st);
      audio_driver_reset_resamplers(audio_st);
   }
   t->layout = layout;
   source_time = fastforward ? audio_st->last_flush_time : 0;
   tempo = (uint32_t)(65536.0 / duration + 0.5);
   cutoff = (snap & AUDIO_SNAP_STRETCH_LPF)
      ? audio_speed_lpf_cutoff(t->lpf.rate, tempo) : 0;
   if (cutoff != t->cutoff)
   {
      audio_speed_lpf_set(&t->lpf, cutoff != 0,
            cutoff ? cutoff : (t->cutoff ? t->cutoff : 20));
      t->cutoff = cutoff;
   }
   do
   {
      size_t used, frames;
      const void *output;
      /* Once active, the engine stays active at unity rather than
       * exiting to raw: the exit would drain the lookahead its search
       * kept as one burst onto a device the hold left full, and the
       * next engage would pause output to fill it again. The cost is
       * that lookahead as holdback after the first speed change. */
      if (!audio_stretch_stream_push_view_limit(t->stream, source, left,
               &used, tempo / 65536.0,
               !audio_st->transport_lpf_only && (tempo != 65536
                  || !audio_stretch_stream_quiescent(t->stream)),
               AUDIO_CHUNK_SIZE_NONBLOCKING >> 1))
         break;
      if (used) t->source_progress = true;
      source += used * frame; left -= used;
      output = audio_stretch_stream_peek(t->stream, &frames);
      if (frames)
      {
         if (!audio_speed_lpf_quiescent(&t->lpf))
         {
            audio_speed_lpf_process_into(&t->lpf, output, t->output, frames);
            output = t->output;
         }
         /* Filter-only retains ordinary pitch-changing SRC. The achieved
          * fast-forward duration was already measured above, once per input. */
         audio_driver_inline_render(audio_st, output, frames, layout,
               audio_st->transport_lpf_only ? (float)duration : 1.0f,
               audio_st->transport_lpf_only, fastforward && !follow);
         audio_stretch_stream_consume(t->stream, frames);
      }
      if (!used && !frames) break;
   } while (left || !audio_stretch_stream_needs_input(t->stream));
   /* Flush cadence must not replace the core's source cadence. */
   audio_st->last_flush_time = source_time;
   return true;
}

static bool audio_driver_inline_flush(audio_driver_state_t *audio_st,
      float slowmotion_ratio, const void *data, size_t samples, bool floating,
      bool slowmotion, bool fastforward)
{
   struct audio_inline_transport *t = audio_st->inline_transport;
   if (!t) return false;
   if (t->channels > 2 && floating == t->floating)
   {
      if (samples > AUDIO_CHUNK_SIZE_NONBLOCKING) return false;
      audio_driver_inline_pack(t, data, samples >> 1, 2, AUDIO_LAYOUT_STEREO);
      data = t->source;
   }
   return audio_driver_inline_process(audio_st, slowmotion_ratio, data,
         samples >> 1, floating, slowmotion, fastforward, AUDIO_LAYOUT_STEREO);
}

static bool audio_driver_inline_prepare(audio_driver_state_t *audio_st,
      bool floating, unsigned channels)
{
   struct audio_inline_transport *t;
   size_t bytes, capacity = AUDIO_CHUNK_SIZE_NONBLOCKING >> 1;
   if (audio_st->inline_transport) return true;
   t = (struct audio_inline_transport*)calloc(1, sizeof(*t));
   if (!t) return false;
   t->floating = floating;
   t->channels = channels;
   t->layout = AUDIO_LAYOUT_STEREO;
   t->stream = audio_stretch_stream_new((unsigned)audio_st->input, t->channels, t->floating, 3);
   bytes = capacity * t->channels * (t->floating ? sizeof(float) : sizeof(int16_t));
   t->output = memalign_alloc(64, t->channels > 2 ? bytes * 2 + bytes * 2 / t->channels : bytes);
   if (!t->stream || !t->output || !audio_stretch_stream_bind(t->stream,
            t->output, AUDIO_CHUNK_SIZE_NONBLOCKING >> 1))
   {
      audio_stretch_stream_free(t->stream);
      if (t->output) memalign_free(t->output);
      free(t);
      return false;
   }
   if (t->channels > 2)
   {
      t->source = (uint8_t*)t->output + bytes;
      t->front = (uint8_t*)t->source + bytes;
   }
   audio_speed_lpf_init(&t->lpf, (unsigned)audio_st->input, t->channels, t->floating);
   audio_st->inline_transport = t;
   return true;
}

static bool audio_driver_transport_configure(const settings_t *settings);

bool audio_driver_init_internal(void *settings_data, bool audio_cb_inited)
{
   unsigned new_rate              = 0;
   settings_t *settings           = (settings_t*)settings_data;
   bool audio_enable              = settings->bools.audio_enable;
   bool audio_sync                = settings->bools.audio_sync;
   bool audio_rate_control        = settings->bools.audio_rate_control;
   float slowmotion_ratio         = settings->floats.slowmotion_ratio;
   unsigned setting_audio_latency = settings->uints.audio_latency;
   unsigned runloop_audio_latency = runloop_state_get_ptr()->audio_latency;
   unsigned audio_latency         = (runloop_audio_latency > setting_audio_latency)
         ? runloop_audio_latency : setting_audio_latency;
   size_t max_buffer_samples      = AUDIO_CHUNK_SIZE_NONBLOCKING * 2;
   bool latency_floored           = false;
   /* Accommodate rewind since at some point we might have two full buffers. */
   size_t outsamples_max          = max_buffer_samples * AUDIO_MAX_RATIO * slowmotion_ratio;
   size_t audio_buf_length        = max_buffer_samples * sizeof(float);
   bool verbosity_enabled         = verbosity_is_enabled();
   /* One block per element type, carved into regions with a 64-byte
    * pad between them (see AUDIO_ARENA_NEXT). Cursors are in elements. */
   size_t i16_out_conv            = 0;
   size_t i16_accum               = AUDIO_ARENA_NEXT(i16_out_conv,
         outsamples_max, AUDIO_ARENA_ALIGN_INT16);
   size_t i16_in_scratch          = AUDIO_ARENA_NEXT(i16_accum,
         AUDIO_SAMPLE_ACCUM_INT16S, AUDIO_ARENA_ALIGN_INT16);
#ifdef HAVE_REWIND
   /* Needs to be able to hold full content of a full max_buffer_samples
    * in addition to its own. */
   size_t i16_rewind              = AUDIO_ARENA_NEXT(i16_in_scratch,
         max_buffer_samples, AUDIO_ARENA_ALIGN_INT16);
   size_t i16_total               = i16_rewind + max_buffer_samples;
#else
   size_t i16_total               = i16_in_scratch + max_buffer_samples;
#endif
   size_t f32_input               = 0;
   size_t f32_synth               = AUDIO_ARENA_NEXT(f32_input,
         max_buffer_samples, AUDIO_ARENA_ALIGN_FLOAT);
   size_t f32_out                 = AUDIO_ARENA_NEXT(f32_synth,
         max_buffer_samples, AUDIO_ARENA_ALIGN_FLOAT);
#ifdef HAVE_REWIND
   size_t f32_rewind              = AUDIO_ARENA_NEXT(f32_out,
         outsamples_max, AUDIO_ARENA_ALIGN_FLOAT);
   size_t f32_total               = f32_rewind + max_buffer_samples;
#else
   size_t f32_total               = f32_out + outsamples_max;
#endif
   int16_t *arena_int16           = (int16_t*)memalign_alloc(64,
         i16_total * sizeof(int16_t));
   float *arena_float             = (float*)memalign_alloc(64,
         f32_total * sizeof(float));

   convert_s16_to_float_init_simd();
   convert_float_to_s16_init_simd();
   audio_driver_clamp_init_simd();

   {
      unsigned floor_ms = settings->uints.audio_latency_floor;
      if (floor_ms < AUDIO_LATENCY_MIN_MS)
         floor_ms = AUDIO_LATENCY_MIN_MS;

      if (audio_latency < floor_ms)
      {
         RARCH_WARN("[Audio] Latency setting of %u ms is below the %u ms"
               " floor; using %u ms.\n",
               audio_latency, floor_ms, floor_ms);
         latency_floored = true;
         audio_latency   = floor_ms;
      }
   }

   if (!arena_int16 || !arena_float)
   {
      if (arena_int16)
         memalign_free(arena_int16);
      if (arena_float)
         memalign_free(arena_float);
      goto error;
   }

   memset(arena_float + f32_input, 0, audio_buf_length);

   audio_driver_st.arena_int16                 = arena_int16;
   audio_driver_st.arena_float                 = arena_float;
   audio_driver_st.input_data                  = arena_float + f32_input;
   audio_driver_st.synth_buf                   = arena_float + f32_synth;
   audio_driver_st.input_data_length           = audio_buf_length;
   /* Pointed at its region further down, once it is known whether an
    * int16 resampler exists; the s16 path that uses it cannot run
    * without one. */
   audio_driver_st.input_data_int16            = NULL;
   audio_driver_st.output_samples_int16        = arena_int16 + i16_out_conv;
   audio_driver_st.output_samples_int16_length = outsamples_max * sizeof(int16_t);
   audio_driver_st.sample_accum                = arena_int16 + i16_accum;
   audio_driver_st.data_ptr                    = 0;
#ifdef HAVE_REWIND
   audio_driver_st.rewind_buf                  = arena_int16 + i16_rewind;
   audio_driver_st.rewind_size                 = max_buffer_samples;
   audio_driver_st.rewind_ptr                  = max_buffer_samples;
#endif
   /* Set now so a return before the driver starts (audio disabled)
    * leaves the float output region reachable through its pointer. */
   audio_driver_st.output_samples_buf          = arena_float + f32_out;
#ifdef HAVE_REWIND
   audio_driver_st.rewind_buf_f                = arena_float + f32_rewind;
#endif
   audio_driver_st.output_samples_buf_length   = outsamples_max * sizeof(float);

   if (!audio_enable)
   {
      AUDIO_FLAGS_CLEAR(&audio_driver_st, AUDIO_FLAG_ACTIVE);
      return false;
   }

   AUDIO_FLAGS_SET(&audio_driver_st, AUDIO_FLAG_ACTIVE);

   if (!(audio_driver_find_driver(settings->arrays.audio_driver,
         "audio driver", verbosity_enabled)))
   {
      RARCH_ERR("Failed to initialize audio driver.\n");
      return false;
   }

   if (!audio_driver_st.current_audio || !audio_driver_st.current_audio->init)
   {
      RARCH_ERR("Failed to initialize audio driver. Will continue without audio.\n");
      AUDIO_FLAGS_CLEAR(&audio_driver_st, AUDIO_FLAG_ACTIVE);
      return false;
   }

#ifdef HAVE_THREADS
   /* A core with its own audio callback already renders on the audio
    * thread and would be a second producer for the ring; it keeps the
    * inline pipeline. So does a driver without wait_writable(): the
    * consumer paces on that, and without it the only pacer would be a
    * blocking write, which pins the device full and adds its whole
    * buffer to the latency. */
   if (     settings->bools.audio_threaded_pipeline
         && !audio_cb_inited
         && !audio_driver_st.current_audio->wait_writable)
      RARCH_LOG("[Audio] Threaded pipeline requested, but driver \"%s\" "
            "has no wait_writable(); using the inline pipeline.\n",
            audio_driver_st.current_audio->ident);

   if (     settings->bools.audio_threaded_pipeline
         && !audio_cb_inited
         && audio_driver_st.current_audio->wait_writable)
   {
      /* Capacity in bytes of int16 stereo frames at the core's rate.
       * audio_driver_st.input is already set by the caller
       * (driver_adjust_system_rates); fps comes from the same av_info. */
      double fps       = video_state_get_ptr()->av_info.timing.fps;
      size_t per_frame = (fps > 0.0)
            ? (size_t)(audio_driver_st.input / fps) : 1024;
      size_t frames    = per_frame * AUDIO_PIPE_RING_FRAMES;
      size_t bytes;
      /* With a non-blocking writer the ring holds a buffer's worth on
       * purpose - see audio_driver_pipe_target_frames() - on top of
       * the frames it holds for the pass; the setting says how much
       * that is, before the driver has said what it made of it. */
      if (!settings->bools.audio_sync)
      {
         size_t latency_frames = (size_t)(audio_driver_st.input * audio_latency / 1000.0);
         if (latency_frames < per_frame)
            latency_frames = per_frame;
         frames       += latency_frames * 2;
      }
      audio_driver_st.pipe_float       = audio_driver_st.core_float;
      /* a multi-channel core's ring carries the canonical wide frame */
      audio_driver_st.pipe_channels    = audio_driver_st.core_multi
            ? AUDIO_PIPE_CANON_CHANNELS : 2;
      audio_driver_st.pipe_frame_bytes = audio_driver_st.pipe_channels
            * (audio_driver_st.pipe_float ? sizeof(float) : sizeof(int16_t));
      audio_driver_st.pipe_layout = AUDIO_LAYOUT_STEREO;
      audio_pipeline_layout_init(&audio_driver_st.pipe_layouts, AUDIO_LAYOUT_STEREO);
      bytes            = frames * audio_driver_st.pipe_frame_bytes;
      if (bytes < 4096)
         bytes         = 4096;
      if (!retro_spsc_init(&audio_driver_st.pipe_ring, bytes))
      {
         RARCH_ERR("[Audio] Cannot allocate the pipeline ring. Exiting...\n");
         return false;
      }
      audio_driver_pipe_marks_clear(&audio_driver_st);
      audio_driver_st.pipe_pass_frames    = per_frame;
      if (audio_driver_st.pipe_pass_frames > AUDIO_PIPE_SLICE_INT16S / 2)
         audio_driver_st.pipe_pass_frames = AUDIO_PIPE_SLICE_INT16S / 2;
      if (audio_driver_st.pipe_pass_frames < 64)
         audio_driver_st.pipe_pass_frames = 64;
      /* A pass of the widest frame for the consumer's bounce and the
       * producer's staging, whichever format the ring carries. */
      if (!audio_driver_st.pipe_arena)
         audio_driver_st.pipe_arena = memalign_alloc(64,
               2 * (AUDIO_PIPE_SLICE_INT16S / 2) * 2 * sizeof(float));
      if (!audio_driver_st.pipe_arena)
      {
         RARCH_ERR("[Audio] Cannot allocate the pipeline scratch. Exiting...\n");
         retro_spsc_free(&audio_driver_st.pipe_ring);
         return false;
      }
      audio_driver_st.pipe_scratch = (uint8_t*)audio_driver_st.pipe_arena;
      audio_driver_st.pipe_conv    = audio_driver_st.pipe_scratch
            + (AUDIO_PIPE_SLICE_INT16S / 2) * 2 * sizeof(float);
      retro_atomic_store_release_int(&audio_driver_st.pipe_ctrl_avail, -1);
      audio_driver_st.pipe_underruns_seen = 0;
      audio_driver_st.pipe_priming        = true;
      if (audio_driver_st.pipe_channels > 2)
      {
         size_t wide = audio_driver_st.pipe_pass_frames * AUDIO_PIPE_CANON_CHANNELS * sizeof(float);
         if (wide > audio_driver_st.pipe_wide_bytes)
         {
            free(audio_driver_st.pipe_wide);
            audio_driver_st.pipe_wide       = (uint8_t*)malloc(wide);
            audio_driver_st.pipe_wide_bytes = audio_driver_st.pipe_wide ? wide : 0;
         }
         if (!audio_driver_st.pipe_wide)
         {
            RARCH_ERR("[Audio] Cannot allocate wide-frame storage. Exiting...\n");
            retro_spsc_free(&audio_driver_st.pipe_ring);
            return false;
         }
      }
      retro_atomic_int_init(&audio_driver_st.pipe_gen, 0);
      retro_atomic_int_init(&audio_driver_st.pipe_stalled, 0);
      retro_atomic_store_release_int(&audio_driver_st.pipe_ff_mult_q16, 65536);
      audio_driver_ff_mult_reset(&audio_driver_st);
      retro_atomic_int_init(&audio_driver_st.pipe_data_gen, 0);
      retro_atomic_int_init(&audio_driver_st.pipe_wake, 0);
      if (     !retro_eventcount_init(&audio_driver_st.pipe_space)
            || !retro_eventcount_init(&audio_driver_st.pipe_data))
      {
         /* Both, because || stops at the first failure and the one
          * before it is up. Free is safe on an object init never
          * reached, so this needs no flag to say which got that far,
          * and it makes the block hold whether or not the caller goes
          * on to run the audio teardown that would also free them. */
         retro_eventcount_free(&audio_driver_st.pipe_data);
         retro_eventcount_free(&audio_driver_st.pipe_space);
         retro_spsc_free(&audio_driver_st.pipe_ring);
         RARCH_ERR("[Audio] Cannot create the pipeline throttle. Exiting...\n");
         return false;
      }
      audio_driver_st.pipe_park_ready     = true;
      AUDIO_FLAGS_SET(&audio_driver_st, AUDIO_FLAG_PIPELINE_THREADED);
      audio_driver_st.pipe_threaded       = true;
      retro_atomic_int_init(&audio_driver_st.pipe_consumer_gone, 0);
      RARCH_LOG("[Audio] Threaded pipeline: ring holds %u frames of %s core audio.\n",
            (unsigned)(audio_driver_st.pipe_ring.capacity / audio_driver_st.pipe_frame_bytes),
            audio_driver_st.pipe_float ? "float" : "int16");
   }

   /* Before the driver's init, which is where a driver that reports a
    * device stage sets it. */
   audio_driver_st.device_latency_frames = 0;

   if (audio_cb_inited || (AUDIO_FLAGS_GET(&audio_driver_st) & AUDIO_FLAG_PIPELINE_THREADED))
   {
      RARCH_LOG("[Audio] Starting threaded audio driver...\n");
      if (!audio_init_thread(
               &audio_driver_st.current_audio,
               &audio_driver_st.context_audio_data,
                *settings->arrays.audio_device
               ? settings->arrays.audio_device : NULL,
               settings->uints.audio_output_sample_rate, &new_rate,
               audio_latency,
               settings->bools.audio_thread_priority,
               settings->bools.thread_prefer_fast_cores,
               audio_driver_st.current_audio))
      {
         RARCH_ERR("[Audio] Cannot open threaded audio driver. Exiting...\n");
         return false;
      }
   }
   else
#endif
   {
      audio_driver_st.context_audio_data =
         audio_driver_st.current_audio->init(*settings->arrays.audio_device
               ? settings->arrays.audio_device : NULL,
               settings->uints.audio_output_sample_rate,
               audio_latency,
               &new_rate);
      RARCH_LOG("[Audio] Started synchronous audio driver.\n");
   }

   if (new_rate != 0)
      configuration_set_int(settings,
            settings->uints.audio_output_sample_rate, new_rate);

   if (!audio_driver_st.context_audio_data)
   {
      RARCH_ERR("Failed to initialize audio driver. Will continue without audio.\n");
      AUDIO_FLAGS_CLEAR(&audio_driver_st, AUDIO_FLAG_ACTIVE);
   }

   AUDIO_FLAGS_CLEAR(&audio_driver_st, AUDIO_FLAG_USE_FLOAT);
   if (     (AUDIO_FLAGS_GET(&audio_driver_st) & AUDIO_FLAG_ACTIVE)
         && audio_driver_st.current_audio->use_float(
            audio_driver_st.context_audio_data))
      AUDIO_FLAGS_SET(&audio_driver_st, AUDIO_FLAG_USE_FLOAT);

   /* The layout the device opened with: stereo unless the driver can
    * say more. A wider device gets the upmix stage and its buffers,
    * sized to the largest write the pipeline makes. */
   audio_driver_st.out_layout   = AUDIO_LAYOUT_STEREO;
   audio_driver_st.out_channels = 2;
   audio_driver_st.core_layout  = AUDIO_LAYOUT_STEREO;
   if (     (AUDIO_FLAGS_GET(&audio_driver_st) & AUDIO_FLAG_ACTIVE)
         && audio_driver_st.current_audio->layout)
   {
      uint32_t layout = audio_driver_st.current_audio->layout(
            audio_driver_st.context_audio_data);
      if (layout != AUDIO_LAYOUT_STEREO && audio_layout_supported(layout))
      {
         unsigned ch   = audio_layout_channels(layout);
         size_t frames = audio_driver_st.output_samples_buf_length
               / (2 * sizeof(float));
         audio_driver_st.upmix_buf = (float*)malloc(frames * ch * sizeof(float));
         audio_driver_st.upmix_i16 = (int16_t*)malloc(frames * ch * sizeof(int16_t));
         if (audio_driver_st.upmix_buf && audio_driver_st.upmix_i16
               && audio_upmix_init(&audio_driver_st.upmix, layout,
                  settings->uints.audio_output_sample_rate))
         {
            audio_driver_st.out_layout   = layout;
            audio_driver_st.out_channels = ch;
            audio_driver_st.upmix_frames = frames;
            RARCH_LOG("[Audio] Device opened with %u channels, layout 0x%03x%s; the stereo mix is upmixed to it.\n",
                  ch, layout,
                  (layout & (AUDIO_SPEAKER_SIDE_LEFT | AUDIO_SPEAKER_SIDE_RIGHT))
                  && !(layout & AUDIO_SPEAKER_BACK_LEFT) ? " (rear pair at the sides)" : "");
         }
         else
         {
            free(audio_driver_st.upmix_buf);
            free(audio_driver_st.upmix_i16);
            audio_driver_st.upmix_buf = NULL;
            audio_driver_st.upmix_i16 = NULL;
            RARCH_WARN("[Audio] Device opened with layout 0x%03x but the upmix stage could not be set up; writing stereo.\n", layout);
         }
      }
      else if (layout != AUDIO_LAYOUT_STEREO)
         RARCH_WARN("[Audio] Device opened with layout 0x%03x, which the frontend cannot fill; writing stereo.\n", layout);
   }

   /* Headphone virtual surround: on a stereo device with the setting
    * on, the mix goes stereo, virtual 5.1, two ears. Buffers: the
    * virtual 5.1 at six floats a frame, and the render in upmix_buf
    * and upmix_i16 at two. */
   audio_driver_st.virtualize = false;
   if (     (AUDIO_FLAGS_GET(&audio_driver_st) & AUDIO_FLAG_ACTIVE)
         && audio_driver_st.out_channels == 2
         && settings->bools.audio_headphone_virtual_surround)
   {
      size_t frames = audio_driver_st.output_samples_buf_length / (2 * sizeof(float));
      audio_driver_st.virt_buf  = (float*)malloc(frames * 6 * sizeof(float));
      audio_driver_st.upmix_buf = (float*)malloc(frames * 2 * sizeof(float));
      audio_driver_st.upmix_i16 = (int16_t*)malloc(frames * 6 * sizeof(int16_t));
      if (audio_driver_st.virt_buf && audio_driver_st.upmix_buf && audio_driver_st.upmix_i16
            && audio_upmix_init(&audio_driver_st.upmix, AUDIO_LAYOUT_5POINT1,
               settings->uints.audio_output_sample_rate)
            && audio_binaural_init(&audio_driver_st.binaural, AUDIO_LAYOUT_5POINT1,
               settings->uints.audio_output_sample_rate))
      {
         audio_driver_st.virtualize   = true;
         audio_driver_st.upmix_frames = frames;
         RARCH_LOG("[Audio] Headphone virtual surround: the stereo mix goes through a virtual 5.1 to two ears.\n");
      }
      else
      {
         free(audio_driver_st.virt_buf);
         free(audio_driver_st.upmix_buf);
         free(audio_driver_st.upmix_i16);
         audio_driver_st.virt_buf  = NULL;
         audio_driver_st.upmix_buf = NULL;
         audio_driver_st.upmix_i16 = NULL;
      }
   }

   if (     !audio_sync
         && (AUDIO_FLAGS_GET(&audio_driver_st) & AUDIO_FLAG_ACTIVE))
   {
      if (     (AUDIO_FLAGS_GET(&audio_driver_st) & AUDIO_FLAG_ACTIVE)
            && audio_driver_st.context_audio_data)
         audio_driver_set_nonblock_state(true);
   }

   if (audio_driver_st.input <= 0.0f)
   {
      /* Should never happen. */
      RARCH_WARN("[Audio] Input rate is invalid (%.3f Hz)."
            " Using output rate (%u Hz).\n",
            audio_driver_st.input,
            settings->uints.audio_output_sample_rate);

      audio_driver_st.input = settings->uints.audio_output_sample_rate;
   }

   audio_driver_st.src_ratio_orig    =
      audio_driver_st.src_ratio_curr =
      (double)settings->uints.audio_output_sample_rate / audio_driver_st.input;
   audio_driver_st.out_rate          = settings->uints.audio_output_sample_rate;

   if (*settings->arrays.audio_resampler)
      strlcpy(audio_driver_st.resampler_ident,
            settings->arrays.audio_resampler,
            sizeof(audio_driver_st.resampler_ident));
   else
      audio_driver_st.resampler_ident[0] = '\0';

   audio_driver_st.resampler_quality = (enum resampler_quality)settings->uints.audio_resampler_quality;

   if (!audio_driver_resampler_realloc(&audio_driver_st,
            settings->bools.audio_resampler_hq_oversampling))
   {
      RARCH_ERR("Failed to initialize resampler \"%s\".\n",
            audio_driver_st.resampler_ident);
      AUDIO_FLAGS_CLEAR(&audio_driver_st, AUDIO_FLAG_ACTIVE);
   }

   if (audio_driver_st.resampler_hq)
      RARCH_LOG("[Audio] HQ sinc oversampling active (nominal ratio %.3f).\n",
            audio_driver_st.src_ratio_orig);

   /* Freshly (re)allocated resampler: ring is clean, not in passthrough. */
   audio_driver_st.resampler_bypassed = false;

   /* Deterministic integer (s16) path: allocate an int16 resampler
    * mirroring the float one when the selected backend has an int16
    * implementation.  It is used by audio_driver_flush() for int16 cores
    * unless a MIDI synth is sounding or a float-only DSP filter is loaded;
    * DSP (int16-capable), the mixer, non-unity gain and fast-forward speedup
    * are all handled in the integer domain.  Otherwise the float path runs. */
   if (audio_driver_st.resampler_data_int16 && audio_driver_st.resampler_int16_free)
   {
      audio_driver_st.resampler_int16_free(audio_driver_st.resampler_data_int16);
      audio_driver_st.resampler_data_int16 = NULL;
   }
   audio_driver_st.resampler_data_int16  = NULL;
   audio_driver_st.resampler_int16_process = NULL;
   audio_driver_st.resampler_int16_free    = NULL;
   audio_driver_st.resampler_int16_reset   = NULL;
   if (     audio_driver_st.resampler
         && audio_driver_st.resampler->short_ident)
   {
      const char *rs_ident = audio_driver_st.resampler->short_ident;
      retro_resampler_int16_t rs;
      retro_resampler_int16_new(&rs, rs_ident,
            audio_driver_st.resampler_quality,
            audio_driver_st.src_ratio_orig, audio_driver_st.resampler_hq);
      audio_driver_st.resampler_data_int16    = rs.data;
      audio_driver_st.resampler_int16_process = rs.process;
      audio_driver_st.resampler_int16_free    = rs.free;
      audio_driver_st.resampler_int16_reset   = rs.reset;
      if (audio_driver_st.resampler_int16_process)
         RARCH_LOG("[Audio] %s resampler: integer s16 path %s.\n",
               rs_ident,
               audio_driver_st.resampler_data_int16
                     ? "available" : "unavailable");
   }

   /* int16 scratch for the s16 path.  Same frame capacity as
    * input_data, in int16.  Its region is part of arena_int16 either way
    * (8 KiB at the default chunk size); what is gated is the pointer,
    * because flush uses it to decide whether the s16 path is live.
    *
    * The gate is deliberately the resampler and nothing else.  The scratch
    * is only *touched* when a synth is sounding or a DSP filter is loaded,
    * but both of those are runtime state that can change without an audio
    * reinit - and audio_fastpath_s16 is CMD_EVENT_NONE, so toggling the
    * hint does not reinit either.  Narrowing the gate to any of them would
    * leave the s16 path live against a NULL scratch. */
   if (audio_driver_st.resampler_data_int16)
      audio_driver_st.input_data_int16 = arena_int16 + i16_in_scratch;

   AUDIO_FLAGS_CLEAR(&audio_driver_st, AUDIO_FLAG_CONTROL);

   /* The sink estimate starts over with the driver. */
   audio_driver_st.sink_bias           = 1.0;
   retro_atomic_store_release_int(&audio_driver_st.sink_bias_q, 0);
   audio_driver_st.sink_started        = 0;
   audio_driver_st.sink_offered        = 0.0;
   audio_driver_st.sink_offered_raw    = 0;
   audio_driver_st.sink_accepted       = 0;
   audio_driver_st.sink_applied        = 0;
   audio_driver_st.sink_rate_hz        = 0.0;
   audio_driver_st.sink_source_hz      = 0.0;
   audio_driver_st.sink_warned         = 0;
   audio_driver_st.sink_warned_flush   = 0;

   /* The driver's buffer, whether or not rate control will use it: it
    * is what the latency setting became, shown in the statistics
    * overlay and read back here against the setting. It was read only
    * on the rate control path, so with rate control off the overlay
    * had nothing to show and the log nothing to say. */
   audio_driver_st.buffer_size = 0;
   if (     (AUDIO_FLAGS_GET(&audio_driver_st) & AUDIO_FLAG_ACTIVE)
         && audio_driver_st.current_audio->buffer_size)
   {
      audio_driver_st.buffer_size =
         audio_driver_st.current_audio->buffer_size(
               audio_driver_st.context_audio_data);
      if (audio_driver_st.buffer_size > 0)
      {
         /* The reported buffer against the setting, in the units the
          * user thinks in. Half of it is the rate control's setpoint,
          * so half of it in time is the latency this driver gives at
          * steady state; a driver that reports only some of its
          * stages, or in the wrong unit, shows here as a size that
          * does not match the setting. */
         unsigned out_rate     = new_rate
               ? new_rate : settings->uints.audio_output_sample_rate;
         /* the device's frame: the layout's channels, not two - a 5.1
          * driver's buffer read three times its size here */
         size_t   frame_bytes  = audio_driver_dev_frame_bytes(&audio_driver_st);
         double   buffer_ms    = out_rate
               ? (double)audio_driver_st.buffer_size / frame_bytes
                  * 1000.0 / out_rate
               : 0.0;
         const char *ident     = audio_driver_st.current_audio->ident;
#ifdef HAVE_THREADS
         /* Name the driver the user chose, not the wrapper it runs
          * under. */
         if (string_is_equal(ident, "audio-thread"))
         {
            const audio_driver_t *inner = audio_thread_wrapped_driver(
                  audio_driver_st.context_audio_data);
            if (inner)
               ident           = inner->ident;
         }
#endif
         RARCH_LOG("[Audio] Driver \"%s\" reports a %u-byte buffer: "
               "%.1f ms of %u-channel %s at %u Hz against a %u ms latency setting%s; "
               "rate control %s it near %.1f ms.\n",
               ident,
               (unsigned)audio_driver_st.buffer_size, buffer_ms,
               audio_driver_st.out_channels ? audio_driver_st.out_channels : 2,
               (AUDIO_FLAGS_GET(&audio_driver_st) & AUDIO_FLAG_USE_FLOAT) ? "float" : "int16",
               out_rate, audio_latency,
               latency_floored ? " (raised to the minimum)" : "",
               audio_rate_control ? "holds" : "is off; on, it would hold",
               buffer_ms / 2.0);
      }
   }

   if (
            (   !audio_cb_inited
             || audio_driver_st.pipe_threaded)
         && (AUDIO_FLAGS_GET(&audio_driver_st) & AUDIO_FLAG_ACTIVE)
         && (audio_rate_control)
         )
   {
      /* Audio rate control requires write_avail
       * and buffer_size to be implemented.
       *
       * The reported size must also be usable: it is the DRC setpoint's
       * denominator (half_size in audio_driver_compute_rate_adjust), so
       * zero yields an infinite or NaN rate_adjust. That propagates into
       * src_ratio_curr, and the sinc drivers' fixed-point time step is
       * (uint32_t)(phases / ratio), which collapses to 0 - leaving
       * `while (time < phases) { ...; time += 0; }` spinning and writing
       * past the output buffer. Several drivers can legitimately report
       * 0 here (asio, xaudio and both libnx audren drivers return 0 on a
       * null handle; audioio returns 0 when its ioctl fails), and oss
       * already carries a local "return something non-zero to avoid
       * SIGFPE" workaround for the same hazard. Check it once, centrally,
       * and fall back to no rate control rather than making every driver
       * defend itself. */
      if (    !audio_driver_st.current_audio->write_avail
           || !audio_driver_st.current_audio->buffer_size)
         RARCH_WARN("[Audio] Rate control was desired, but driver does not support needed features.\n");
      else if (audio_driver_st.buffer_size == 0)
         RARCH_WARN("[Audio] Rate control was desired, but the driver "
               "reported a zero buffer size.\n");
      else
         AUDIO_FLAGS_SET(&audio_driver_st, AUDIO_FLAG_CONTROL);
   }

   command_event(CMD_EVENT_DSP_FILTER_INIT, NULL);

   audio_driver_st.free_samples_count = 0;

   /* Reset DRC rate-limit state. cached_rate_adjust defaults to 1.0
    * (no adjustment) for the brief window before the first compute
    * crosses audio_st->drc_threshold_int16s. drc_threshold_int16s is
    * recomputed from the just-set input rate and av_info.timing.fps;
    * since SET_SYSTEM_AV_INFO drives audio reinit, mid-session rate
    * or fps changes flow through this same path. */
   audio_driver_st.cached_rate_adjust = 1.0;
   audio_driver_st.samples_since_drc  = 0;
#ifdef HAVE_THREADS
   /* Before anything that could start a thread reaching the mixer or
    * the DSP filter: those readers assume it is already there. */
   if (!audio_driver_st.state_lock)
      audio_driver_st.state_lock      = slock_new();
#endif
   /* Armed so the first flush measures rather than running on the
    * default factor until the sample-count gate trips. */
   audio_driver_st.drc_pending        = true;
   audio_driver_update_drc_threshold(&audio_driver_st);

#ifdef HAVE_AUDIOMIXER
   audio_mixer_init(settings->uints.audio_output_sample_rate);
   AUDIO_FLAGS_SET(&audio_driver_st, AUDIO_FLAG_MIXER_INITED);
#endif

   if (!audio_driver_transport_configure(settings))
      RARCH_WARN("[Audio] Pitch-preserving playback unavailable; using ordinary playback.\n");

   /* The wrapper thread is created parked and nothing restarts it after
    * a mid-session reinit (SET_SYSTEM_AV_INFO) - the runloop only issues
    * AUDIO_START around pause and menu transitions - so start it here
    * whenever it was created. */
   if (     (AUDIO_FLAGS_GET(&audio_driver_st) & AUDIO_FLAG_ACTIVE)
         && (   audio_cb_inited
             || (AUDIO_FLAGS_GET(&audio_driver_st) & AUDIO_FLAG_PIPELINE_THREADED)))
      audio_driver_start(false);

   return true;

error:
   return audio_driver_deinit();
}

void audio_driver_pipeline_consumer_exit(void)
{
   /* Release: the consumer's loop is done before the word that says so. */
   retro_atomic_store_release_int(&audio_driver_st.pipe_consumer_gone, 1);
}

void audio_driver_publish_runloop(void)
{
   int      ratio_bits;
   float    ratio;
   settings_t *settings = config_get_ptr();
   uint32_t rf = runloop_get_flags();
   int      v  = 0;
   if (rf & RUNLOOP_FLAG_PAUSED)
      v |= AUDIO_SNAP_PAUSED;
   if (rf & RUNLOOP_FLAG_SLOWMOTION)
      v |= AUDIO_SNAP_SLOWMOTION;
   /* A callback core's audio is paced by the device, not by retro_run:
    * fast-forward speeds the video while the callback keeps rendering
    * real time, so there is no surplus for the speed machinery to
    * absorb. Worse, that machinery measures flush cadence, and on the
    * callback's audio thread the cadence IS the device draining - the
    * measurement confirms whatever multiplier it last applied, seeded
    * at the configured ratio, which sped the audio up and crackled.
    * Every speed consumer reads this snapshot and callback cores never
    * run the threaded pipeline, so withholding the bit here restores
    * the stable behavior: fast-forward leaves callback audio alone.
    * Opting in publishes the bit, and the multiplier then comes from
    * the frame-end cadence measurement, never the flush's own (see
    * audio_driver_ff_callback_frame_end()). */
   if ((rf & RUNLOOP_FLAG_FASTMOTION)
         && (   !audio_driver_st.callback.callback
             || settings->bools.audio_fastforward_callback))
      v |= AUDIO_SNAP_FASTMOTION;
   if (settings->bools.audio_sync)
      v |= AUDIO_SNAP_SYNC;
   if (settings->bools.audio_fastforward_speedup)
      v |= AUDIO_SNAP_FF_SPEEDUP;
   if (settings->bools.audio_sink_rate_estimation)
      v |= AUDIO_SNAP_SINK_EST;
   if (settings->bools.audio_fastpath_s16)
      v |= AUDIO_SNAP_FASTPATH_S16;
   if (settings->bools.audio_time_stretch_lowpass)
      v |= AUDIO_SNAP_STRETCH_LPF;
   ratio = settings->floats.slowmotion_ratio;
   memcpy(&ratio_bits, &ratio, sizeof(ratio_bits));
   retro_atomic_store_release_int(
         &audio_driver_st.runloop_slowmotion_bits, ratio_bits);
   {
      /* The seed of the fast-forward estimate wants the ratio the
       * limiter runs the core at: the override's while one is
       * active, the setting's otherwise, resolved here where both
       * are the main thread's to read. */
      const struct retro_fastforwarding_override *o =
            &runloop_state_get_ptr()->fastmotion_override.current;
      ratio = (o->fastforward && o->ratio >= 0.0f)
            ? o->ratio : settings->floats.fastforward_ratio;
      memcpy(&ratio_bits, &ratio, sizeof(ratio_bits));
      retro_atomic_store_release_int(
            &audio_driver_st.runloop_ffratio_bits, ratio_bits);
   }
#ifdef HAVE_MENU
   if (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE)
      v |= AUDIO_SNAP_MENU_ALIVE;
   if (settings->bools.menu_pause_libretro)
      v |= AUDIO_SNAP_MENU_PAUSES;
#ifdef HAVE_NETWORKING
   if (netplay_driver_ctl(RARCH_NETPLAY_CTL_ALLOW_PAUSE, NULL))
      v |= AUDIO_SNAP_ALLOW_PAUSE;
#else
   v |= AUDIO_SNAP_ALLOW_PAUSE;
#endif
#endif
   retro_atomic_store_release_int(&audio_driver_st.runloop_snapshot, v);
}

#ifdef HAVE_THREADS
/**
 * audio_driver_pipeline_pass_done:
 *
 * Audio thread. Says that ring space has opened, and with @drained
 * that the device is moving again if the producer had found it
 * stalled - false where that is not what happened: a pass dropped
 * while paused advances the generation so a waiting producer is not
 * left hanging, but it has not shown the device draining, and a
 * stop or reinit has not either.
 *
 * Exactly one of these per pass, which is the whole cadence of this
 * channel: what the producer waits for is a pass completing, so there
 * is nothing finer to report and nothing to batch. With no producer
 * parked the notify is a read-modify-write and a load.
 **/
static void audio_driver_pipeline_pass_done(audio_driver_state_t *audio_st,
      bool drained)
{
   /* Cleared before the generation it is read with: a producer woken by
    * the notify below re-reads both, and must not find the new
    * generation beside a stall that this pass has just disproved. */
   if (drained)
      retro_atomic_store_release_int(&audio_st->pipe_stalled, 0);
   retro_atomic_fetch_add_int(&audio_st->pipe_gen, 1);
   retro_eventcount_notify(&audio_st->pipe_space);
}
#endif

/**
 * audio_driver_pipeline_signal:
 *
 * Main thread. Wakes the consumer once for everything published since
 * its last wake. Called at the frame end and from the per-frame
 * producers that run without one (menu audio, rewind reversal), and before
 * a full-ring space wait that would otherwise prevent the frame from ending.
 **/
static void audio_driver_pipeline_signal(audio_driver_state_t *audio_st)
{
#ifdef HAVE_THREADS
   if (!audio_st->pipe_threaded)
      return;
   retro_atomic_fetch_add_int(&audio_st->pipe_data_gen, 1);
   retro_eventcount_notify(&audio_st->pipe_data);
#else
   (void)audio_st;
#endif
}

/**
 * audio_driver_state_lock:
 *
 * Guards the mixer streams and the DSP filter against the audio
 * thread. The NULL test is not optional locking: the lock is created
 * before that thread exists and freed after audio->free() has joined
 * it, so every moment at which a second thread can reach this state
 * is a moment at which the lock is there. A caller that reaches the
 * mixer from another thread outside an initialised audio driver would
 * find no lock and no diagnostic, so anything that widens who touches
 * that state has to widen this lifetime with it.
 *
 * This lock is the deliberate remainder of the subsystem's threading
 * design, not a candidate for the atomics treatment the rest of the
 * audio state received. Everything cheap to publish already crosses
 * without it - runloop_snapshot, the AUDIO_FLAGS word, the SPSC pipe
 * ring, the eventcount wakeups - and what it still serializes is the
 * part atomics cannot: the driver's own write() and control calls
 * against device reinit, the resampler and DSP instances against
 * their replacement, the mixer's voice table against task-thread
 * play() calls, and a nesting contract the mixer_lock sample pins.
 * The per-batch cost is one uncontended acquire around work that is
 * microseconds of resampling and device I/O; the batch budget holds
 * with room. Shrinking a section here is welcome; removing the lock
 * means re-answering every one of those ownership questions at once.
 **/
void audio_driver_state_lock(void)
{
#ifdef HAVE_THREADS
   if (audio_driver_st.state_lock)
      slock_lock(audio_driver_st.state_lock);
#endif
}

void audio_driver_state_unlock(void)
{
#ifdef HAVE_THREADS
   if (audio_driver_st.state_lock)
      slock_unlock(audio_driver_st.state_lock);
#endif
}

/* Converts to the device's format if it needs it, and writes. The pause
 * tail reaches the device outside audio_driver_flush()'s own conversion. */
static void audio_driver_write_float(audio_driver_state_t *audio_st,
      const audio_driver_t *audio, const float *data, size_t frames)
{
   if (frames < 1)
      return;
   if (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_USE_FLOAT)
      audio_driver_write_frames(audio_st, audio, data, frames, true, false);
   else
   {
      int16_t buf[AUDIO_PAUSE_TAIL_FRAMES * 2];
      if (frames > AUDIO_PAUSE_TAIL_FRAMES)
         frames = AUDIO_PAUSE_TAIL_FRAMES;
      convert_float_to_s16(buf, data, frames * 2);
      audio_driver_write_frames(audio_st, audio, buf, frames, false, false);
   }
}

/* Index into pause_hist for a given age. The one modulo here is paid once
 * per call; audio_driver_pause_tail_period()'s search instead walks the
 * ring with audio_driver_pause_hist_prev() to avoid paying it per frame. */
static unsigned audio_driver_pause_hist_idx(
      audio_driver_state_t *audio_st, unsigned age)
{
   return (audio_st->pause_hist_pos + AUDIO_PAUSE_HIST_FRAMES - age)
         % AUDIO_PAUSE_HIST_FRAMES;
}

/* The stereo frame 'age' frames back from the most recently played one; age
 * 1 is that frame itself. The caller checks the age against
 * pause_hist_fill. */
static const float *audio_driver_pause_hist_at(
      audio_driver_state_t *audio_st, unsigned age)
{
   return &audio_st->pause_hist[audio_driver_pause_hist_idx(audio_st, age) * 2];
}

/* Steps a pause_hist index one frame older, wrapping without a modulo. */
static INLINE unsigned audio_driver_pause_hist_prev(unsigned idx)
{
   return idx ? (idx - 1) : (AUDIO_PAUSE_HIST_FRAMES - 1);
}

/* Period, in output frames, that the pause tail should repeat: the lag whose
 * copy of the last AUDIO_PAUSE_TAIL_CORR_FRAMES frames matches them best.
 *
 * Fading the last sample as a constant is a decaying DC offset rather than a
 * decaying sound, and is heard as a thump. Repeating one period keeps the
 * spectrum of what was playing and only takes its level away - the same
 * concealment a packet-loss hider does.
 *
 * Returns 0 when there is not enough history to search, leaving the caller on
 * the DC ramp. */
static unsigned audio_driver_pause_tail_period(
      audio_driver_state_t *audio_st)
{
   unsigned p;
   unsigned best      = 0;
   float    best_diff = -1.0f;
   unsigned max_p     = AUDIO_PAUSE_TAIL_MAX_PERIOD;
   float    e_ref     = 0.0f;
   unsigned k;
   unsigned idx;

   if (audio_st->pause_hist_fill
         < AUDIO_PAUSE_TAIL_MIN_PERIOD + AUDIO_PAUSE_TAIL_CORR_FRAMES)
      return 0;
   /* Search only as far back as there is history for. */
   if (audio_st->pause_hist_fill < max_p + AUDIO_PAUSE_TAIL_CORR_FRAMES)
      max_p = audio_st->pause_hist_fill - AUDIO_PAUSE_TAIL_CORR_FRAMES;

   /* Matched on the channel sum: the two channels share a fundamental, and
    * scoring them together stops a quiet channel's noise choosing the period
    * for a loud one. The energy only decides whether there is anything here
    * worth matching. */
   idx = audio_driver_pause_hist_idx(audio_st, 1);
   for (k = 0; k < AUDIO_PAUSE_TAIL_CORR_FRAMES; k++)
   {
      const float *f = &audio_st->pause_hist[idx * 2];
      float        v = f[0] + f[1];
      e_ref         += v * v;
      idx            = audio_driver_pause_hist_prev(idx);
   }
   if (e_ref <= 0.0f)
      return 0;

   for (p = AUDIO_PAUSE_TAIL_MIN_PERIOD; p <= max_p; p++)
   {
      float    diff = 0.0f;
      unsigned ia   = audio_driver_pause_hist_idx(audio_st, 1);
      unsigned ib   = audio_driver_pause_hist_idx(audio_st, 1 + p);
      for (k = 0; k < AUDIO_PAUSE_TAIL_CORR_FRAMES; k++)
      {
         const float *a = &audio_st->pause_hist[ia * 2];
         const float *b = &audio_st->pause_hist[ib * 2];
         float        d = (a[0] + a[1]) - (b[0] + b[1]);
         diff          += d * d;
         ia             = audio_driver_pause_hist_prev(ia);
         ib             = audio_driver_pause_hist_prev(ib);
      }
      /* Scored by distance rather than correlation: a normalised
       * correlation is blind to level, so the same phrase at half or twice
       * the volume scores a perfect 1.0 and repeating it steps the
       * waveform. */
      if (best_diff < 0.0f || diff < best_diff)
      {
         best_diff = diff;
         best      = p;
      }
   }

   /* No threshold on the score: content with no periodicity has no right
    * answer, and repeating the closest recent stretch of it still hands back
    * the right spectrum at the right level. The DC ramp is only for having no
    * history. */
   return best;
}

/* Answered on the ramp, not on RUNLOOP_FLAG_PAUSED: menu_pause_libretro
 * stops core_run() without setting it, so a core with its own audio thread
 * carries on after the tail, and a state load is not paused at all. Only
 * the core's own audio is gated; the menu filler and mixer share the flush. */
bool audio_driver_core_silenced(void)
{
   return retro_atomic_load_acquire_int(
         &audio_driver_st.core_silenced) != 0;
}

void audio_driver_pause_fade(bool paused)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   const audio_driver_t *audio    = audio_st->current_audio;
   bool was_silenced;
   unsigned i;

   if (     !audio
         || !audio->write
         || !audio_st->context_audio_data
         || !(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_ACTIVE))
      return;

   /* Whether the stream is already down decides what a resume has to do, so
    * read it before the store below overwrites it. */
   was_silenced = audio_driver_core_silenced();

   /* Published before the lock is taken, so a core delivering on a thread of
    * its own stops handing audio over while this waits for the flush already
    * in flight. See audio_driver_core_silenced(). */
   retro_atomic_store_release_int(&audio_st->core_silenced, paused ? 1 : 0);

   /* Everything below writes to the device or touches state audio_driver_
    * flush() owns, and flush runs on whichever thread the core delivers on.
    * Without the lock the tail lands in the middle of a core thread's own
    * write and that thread's buffer arrives behind it at full level. */
   audio_driver_state_lock();

   if (!paused)
   {
      /* Ramp the first frames of whatever comes back, in audio_driver_flush;
       * nothing to write here, the core has not produced it yet.
       *
       * Only when the stream was actually taken down: a resume can arrive
       * without its pause (menu_pause_libretro turned off from inside the
       * menu it paused), and ramping audio that never stopped is a dip in the
       * middle of it. */
      if (was_silenced)
      {
         audio_st->fade_in_pending = true;
         /* And refill the device ahead of that first frame; see
          * audio_driver_resume_topup(). */
         audio_st->resume_topup_pending = true;
      }
      /* Whatever the pipeline was holding at the pause is long gone; do not
       * take the resumed audio for it. */
      audio_st->pause_mute_frames = 0;
      audio_driver_state_unlock();
      return;
   }

   audio_st->fade_in_frames  = 0;
   audio_st->fade_in_pending = false;
#ifdef HAVE_THREADS
   /* Nothing published so far reaches the device: the tail below ends
    * the stream, and the consumer takes the ring out up to here. */
   retro_atomic_store_release_size(&audio_st->pipe_discard_to,
         retro_atomic_load_acquire_size(&audio_st->pipe_ring.head));
   retro_atomic_fetch_add_int(&audio_st->pipe_discard_gen, 1);
   audio_driver_pipe_fade_publish(audio_st, false, 0);
#endif

   /* Only when something was playing is there a step to smooth. */
   if (     audio_st->last_out[0] != 0.0f
         || audio_st->last_out[1] != 0.0f)
   {
      float    ramp[AUDIO_PAUSE_TAIL_FRAMES * 2];
      unsigned period = audio_driver_pause_tail_period(audio_st);
      unsigned n      = AUDIO_PAUSE_TAIL_FRAMES;
      float    join[2];

      /* While fast-forward keeps a non-blocking device pinned full only a
       * fraction of the tail fits; fit the ramp to the room so it still
       * reaches silence. */
      if (     (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_NONBLOCK)
            && audio->write_avail)
      {
         size_t fb   = audio_driver_dev_frame_bytes(audio_st);
         size_t room = audio->write_avail(audio_st->context_audio_data) / fb;
         if (room < n)
            n = (unsigned)room;
      }

      /* However good the match, the repeat starts at its own value rather
       * than the one the stream stopped on, and that difference is a step at
       * the head of the ramp where the gain is still 1. Start the tail where
       * the audio actually stopped and let the correction decay away. The
       * same difference recurs at every wrap, so it is applied against the
       * position within the period rather than within the ramp. */
      if (period)
      {
         /* Age period+1, not period: the frame matching the last one played
          * is a whole period before it, and age period is already its
          * successor - the frame the tail opens on. */
         const float *f = audio_driver_pause_hist_at(audio_st, period + 1);
         join[0] = audio_st->last_out[0] - f[0];
         join[1] = audio_st->last_out[1] - f[1];
      }
      else
         join[0] = join[1] = 0.0f;

      for (i = 0; i < n; i++)
      {
         /* Raised cosine: a linear ramp still corners at both ends. */
         float g = 0.5f * (1.0f + cosf((float)M_PI * (float)(i + 1)
               / (float)n));
         if (period)
         {
            /* Age 'period' continues the waveform from where it stopped;
             * wrapping back to it repeats that cycle for as long as the
             * envelope lasts. */
            unsigned     u = (unsigned)(i % period);
            const float *f = audio_driver_pause_hist_at(audio_st, period - u);
            /* Over whichever is shorter, the join window or the period. The
             * correction must be spent by the time the repeat wraps: the
             * waveform steps by -join there and j returning to 1 is what
             * cancels it, so an undecayed remainder would recur every wrap as
             * a sawtooth at the period's own rate. */
            unsigned    jn = (AUDIO_PAUSE_TAIL_JOIN_FRAMES < period)
                  ? AUDIO_PAUSE_TAIL_JOIN_FRAMES : period - 1;
            float        j = (u < jn)
                  ? (1.0f - (float)u / (float)jn)
                  : 0.0f;
            ramp[(i * 2) + 0] = (f[0] + join[0] * j) * g;
            ramp[(i * 2) + 1] = (f[1] + join[1] * j) * g;
         }
         else
         {
            ramp[(i * 2) + 0] = audio_st->last_out[0] * g;
            ramp[(i * 2) + 1] = audio_st->last_out[1] * g;
         }
      }
      audio_driver_write_float(audio_st, audio, ramp, n);
   }

   audio_st->last_out[0]       = 0.0f;
   audio_st->last_out[1]       = 0.0f;
   audio_st->pause_mute_frames = AUDIO_PAUSE_MUTE_FRAMES;
   /* The stretch holds its lookahead of stale audio at full level, and the
    * silence the menu feeds would push it out behind the ramp that just
    * ended the sound. Safe to discard here and nowhere else: the output is
    * going to silence either way. */
   audio_driver_inline_reset(audio_st);
   audio_driver_state_unlock();
}

bool audio_driver_jump_fade_begin(void)
{
   if (audio_driver_core_silenced())
      return false;
   audio_driver_pause_fade(true);
   return true;
}

void audio_driver_jump_fade_end(bool ramped)
{
   if (ramped)
      audio_driver_pause_fade(false);
}

/* Arms the resume ramp owed since the last resume. Under the state lock,
 * on the flush that carries the core's first audio. */
static void audio_driver_arm_resume(audio_driver_state_t *audio_st)
{
   if (audio_st->fade_in_pending)
   {
      audio_st->fade_in_frames  = AUDIO_PAUSE_TAIL_FRAMES;
      audio_st->fade_in_pending = false;
   }
}

void audio_driver_pipeline_wake(void)
{
#ifdef HAVE_THREADS
   audio_driver_state_t *audio_st = &audio_driver_st;
   if (!audio_st->pipe_park_ready)
      return;
   /* Sticky first, then the generation, then the notify: a consumer
    * woken by it re-reads both and must not find the new generation
    * beside a wake that has not been raised yet. */
   retro_atomic_store_release_int(&audio_st->pipe_wake, 1);
   retro_atomic_fetch_add_int(&audio_st->pipe_data_gen, 1);
   retro_eventcount_notify(&audio_st->pipe_data);
   /* Outside the lock, because the space channel does not take one.
    * A stop or a reinit is not a pass, so the stall is left as it was:
    * whether the device drains again is for a pass to say. */
   audio_driver_pipeline_pass_done(audio_st, false);
#endif
}

void audio_driver_set_nonblock_state(bool nonblock)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   if (nonblock)
      AUDIO_FLAGS_SET(audio_st, AUDIO_FLAG_NONBLOCK);
   else
      AUDIO_FLAGS_CLEAR(audio_st, AUDIO_FLAG_NONBLOCK);
   if (     audio_st->current_audio
         && audio_st->current_audio->set_nonblock_state
         && audio_st->context_audio_data)
      audio_st->current_audio->set_nonblock_state(
            audio_st->context_audio_data, nonblock);
}

#ifdef HAVE_THREADS
/* The most one consumer pass may put into the device, in device bytes.
 * A quarter of the buffer, which on a driver that sets its own IO
 * period from the buffer - CoreAudio takes a quarter - is one period.
 *
 * It was a half. The consequence of the fraction is not the size of the
 * write, it is where the device's fill sits: the pass waits for room
 * for a whole chunk before it takes one, so the fill runs between the
 * buffer less a chunk and the buffer, and the chunk is therefore the
 * margin the consumer thread is given to wake up and finish. Half left
 * half the buffer - 4 ms at an 8 ms setting - to absorb a late wake,
 * 250 times a second. The inline path has no such band: its write
 * blocks inside the driver and holds the device near full, which is the
 * whole of the buffer as margin, and is why the inline path is clean on
 * hardware where the threaded one is not.
 *
 * A quarter raises the floor from half the buffer to three quarters of
 * it and makes each pass one device period, at the cost of twice as
 * many passes - the same rate the device's own callback already runs
 * at. */
static size_t audio_driver_pipe_chunk_bytes(audio_driver_state_t *audio_st)
{
   return audio_st->buffer_size / 4;
}

/* What the pipe ring is to hold on purpose, in core frames. With a
 * blocking writer no margin beyond the publish in flight: the device's
 * buffer is the margin and the writer waits on it. With a non-blocking
 * writer a late frame is dropped, not waited for, so the pipe holds
 * another buffer's worth ahead of the device - the setting's worth of
 * margin against a core that delivers late, at the setting's worth of
 * latency on top of the device's - within what the ring can hold with
 * a publish in flight.
 *
 * The one-publish floor below applies to both. It used to be reached
 * only by the non-blocking side, the blocking one returning zero here
 * before it: a pipe that is to hold nothing. It cannot hold nothing.
 * The core publishes a frame's worth at once and the consumer can only
 * move it at the device's pace, so a publish is in the pipe for most
 * of every frame whatever the writer does - and this number is rate
 * control's setpoint for the fill, which subtracts what the pipe holds
 * (see pipe_ctrl_avail). Against a setpoint of zero the pipe's own
 * contents read as overfill: several times the device buffer of it,
 * so the controller's free space clamped to zero and stayed there,
 * correcting downward at its full delta until the pipe drained. Empty,
 * it read as no error again, so the loop turned around - and at the
 * drained end of that cycle the consumer waits for data between
 * publishes, with only the device's own buffer behind it, which is
 * shorter than a video frame at the low latency settings. That is a
 * gap at the frame rate, and it is what the threaded pipeline was
 * doing on CoreAudio at 8 ms while the inline path was clean. */
/* How long the producer may wait for the consumer to complete a pass.
 * That pass is paced by the device draining the source the ring holds,
 * so the bound is the ring's own playing time at the speed it is being
 * pulled at - shorter in fast-forward, where the pull is faster and
 * where this wait is now reached at all. Flat, it was a second, and a
 * device that had stopped draining cost the frontend every bit of it
 * before the stall was latched. */
static retro_time_t audio_driver_pipe_wait_us(audio_driver_state_t *audio_st)
{
   double us, mult = 1.0;
   size_t frames;
   if (!audio_st->pipe_frame_bytes || !(audio_st->input > 0.0f))
      return AUDIO_PIPE_WAIT_MAX_US;
   if (retro_atomic_load_acquire_int(&audio_st->runloop_snapshot)
         & AUDIO_SNAP_FASTMOTION)
   {
      mult = (double)retro_atomic_load_acquire_int(&audio_st->pipe_ff_mult_q16)
         / 65536.0;
      if (!(mult > 0.0) || mult > 1.0)
         mult = 1.0;
   }
   frames = audio_st->pipe_ring.capacity / audio_st->pipe_frame_bytes;
   us     = (double)frames * mult * AUDIO_PIPE_WAIT_FILLS * 1000000.0
      / (double)audio_st->input;
   if (us < AUDIO_PIPE_WAIT_MIN_US)
      return AUDIO_PIPE_WAIT_MIN_US;
   if (us > AUDIO_PIPE_WAIT_MAX_US)
      return AUDIO_PIPE_WAIT_MAX_US;
   return (retro_time_t)us;
}

static size_t audio_driver_pipe_target_frames(audio_driver_state_t *audio_st)
{
   size_t frame_bytes, target, ring_max;
   int snap = retro_atomic_load_acquire_int(&audio_st->runloop_snapshot);
   if (!audio_st->buffer_size)
      return 0;
   if (snap & AUDIO_SNAP_SYNC)
      target      = 0;
   else
   {
      frame_bytes = audio_driver_dev_frame_bytes(audio_st);
      target      = (size_t)((double)audio_st->buffer_size / frame_bytes
            / audio_st->src_ratio_orig);
   }
   /* Never under one publish: the core delivers a frame at a time, and
    * a pipe holding less than that between publishes is a device that
    * runs dry between them whatever its own buffer holds. */
   if (target < audio_st->pipe_pass_frames)
      target = audio_st->pipe_pass_frames;
   /* Within the ring less two publishes: one arriving, one of swing
    * in the fill between the core's publish and the consumer's pass. */
   ring_max    = audio_st->pipe_ring.capacity / audio_st->pipe_frame_bytes;
   if (ring_max > 2 * audio_st->pipe_pass_frames)
      ring_max -= 2 * audio_st->pipe_pass_frames;
   else
      ring_max  = 0;
   return target < ring_max ? target : ring_max;
}
#endif

/* Input and output do not overlap. Convert only the two populated slots. */
static INLINE void audio_driver_pipe_widen_stereo(void *output, const void *input,
      size_t frames, unsigned channels, bool input_float, bool output_float)
{
   size_t f;
   size_t sample = output_float ? sizeof(float) : sizeof(int16_t);
   memset(output, 0, frames * channels * sample);
   if (input_float == output_float)
   {
      for (f = 0; f < frames; f++)
         memcpy((uint8_t*)output + f * channels * sample,
               (const uint8_t*)input + f * 2 * sample, 2 * sample);
   }
   else if (output_float)
   {
      float *dst = (float*)output;
      const int16_t *src = (const int16_t*)input;
      for (f = 0; f < frames; f++)
      {
         dst[f * channels]     = src[2 * f] * (1.0f / 32768.0f);
         dst[f * channels + 1] = src[2 * f + 1] * (1.0f / 32768.0f);
      }
   }
   else
   {
      int16_t *dst = (int16_t*)output;
      const float *src = (const float*)input;
      for (f = 0; f < frames; f++)
      {
         dst[f * channels]     = (int16_t)audio_float_to_s16_sat(src[2 * f]);
         dst[f * channels + 1] = (int16_t)audio_float_to_s16_sat(src[2 * f + 1]);
      }
   }
}

/**
 * audio_driver_submit:
 *
 * Producer-side entry for one block of int16 stereo core audio. With
 * the threaded pipeline it publishes the block into pipe_ring for the
 * audio thread; otherwise it runs the pipeline inline, exactly as
 * before. When the ring is full the producer waits unless the driver
 * is in its non-blocking state (audio_sync off, or a fast-forward the
 * consumer does not pull at a limited speed), in
 * which case the remainder is dropped - the same choice a full device
 * buffer forces on a non-blocking write. This is the only place the
 * main thread ever waits on audio, and it waits on the device draining,
 * not on a lock.
 **/
static void audio_driver_submit_width(audio_driver_state_t *audio_st,
      float slowmotion_ratio, const void *data, size_t samples, bool is_float,
      bool is_slowmotion, bool is_fastforward, bool from_core,
      unsigned canon_width);

/* A publish of stereo frames, samples = frames * 2, from the classic
 * entries and the accumulator. from_core says the audio is the core's
 * own rather than the menu's filler; see audio_driver_arm_resume(). */
static void audio_driver_submit(audio_driver_state_t *audio_st,
      float slowmotion_ratio, const void *data, size_t samples, bool is_float,
      bool is_slowmotion, bool is_fastforward, bool from_core)
{
   if (audio_st->pipe_channels > 2)
      audio_st->pipe_layout = AUDIO_LAYOUT_STEREO;
   audio_driver_submit_width(audio_st, slowmotion_ratio, data, samples, is_float,
         is_slowmotion, is_fastforward, from_core, 2);
}

#ifdef HAVE_THREADS
/* layout == 0 means already canonical. No input/output alias is permitted. */
static void audio_driver_pipe_map(void *dst, const void *src, size_t frames,
      unsigned channels, unsigned pc, unsigned layout, const unsigned *slots,
      bool input_float, bool output_float)
{
   size_t f;
   unsigned c;
   size_t sample = output_float ? sizeof(float) : sizeof(int16_t);
   if (!layout)
   {
      if (input_float == output_float)
         memcpy(dst, src, frames * channels * sample);
      else if (input_float)
         convert_float_to_s16((int16_t*)dst, (const float*)src, frames * channels);
      else
         convert_s16_to_float((float*)dst, (const int16_t*)src, frames * channels, 1.0f);
      return;
   }
   if (channels == 2 && layout == AUDIO_LAYOUT_STEREO)
   {
      audio_driver_pipe_widen_stereo(dst, src, frames, pc, input_float, output_float);
      return;
   }
   if (input_float == output_float && audio_driver_pipe_widen_prefix(dst,
            src, frames, channels, layout, input_float))
      return;
   memset(dst, 0, frames * pc * sample);
   for (f = 0; f < frames; f++)
      for (c = 0; c < channels; c++)
      {
         size_t in = f * channels + c, out = f * pc + slots[c];
         if (output_float)
            ((float*)dst)[out] = input_float ? ((const float*)src)[in]
               : ((const int16_t*)src)[in] * (1.0f / 32768.0f);
         else if (input_float)
            ((int16_t*)dst)[out] = (int16_t)audio_float_to_s16_sat(((const float*)src)[in]);
         else
            ((int16_t*)dst)[out] = ((const int16_t*)src)[in];
      }
}

static size_t audio_driver_pipe_write_mapped(audio_driver_state_t *audio_st,
      const void *data, size_t frames, unsigned channels, unsigned layout,
      const unsigned *slots, bool floating)
{
   const unsigned pc = audio_st->pipe_channels ? audio_st->pipe_channels : 2;
   const size_t fb = audio_st->pipe_frame_bytes;
   const size_t input_fb = channels * (floating ? sizeof(float) : sizeof(int16_t));
   size_t done = 0;
   while (done < frames)
   {
      void *dst;
      size_t span = retro_spsc_write_begin(&audio_st->pipe_ring, &dst);
      size_t n = span / fb;
      const uint8_t *src = (const uint8_t*)data + done * input_fb;
      if (n > frames - done) n = frames - done;
      if (n && !((uintptr_t)dst % (audio_st->pipe_float ? sizeof(float) : sizeof(int16_t))))
      {
         if (layout && floating != audio_st->pipe_float)
         {
            /* Keep the bulk SIMD converters on mixed-format sources. Only
             * packed active channels enter scratch; canonical silence is
             * written directly into the reservation, never converted. */
            size_t cap = (AUDIO_PIPE_SLICE_INT16S / 2) * 2 / channels;
            if (n > cap) n = cap;
            if (floating)
               convert_float_to_s16((int16_t*)audio_st->pipe_conv,
                     (const float*)src, n * channels);
            else
               convert_s16_to_float((float*)audio_st->pipe_conv,
                     (const int16_t*)src, n * channels, 1.0f);
            audio_driver_pipe_map(dst, audio_st->pipe_conv, n, channels, pc,
                  layout, slots, audio_st->pipe_float, audio_st->pipe_float);
         }
         else
            audio_driver_pipe_map(dst, src, n, channels, pc, layout, slots,
                  floating, audio_st->pipe_float);
         retro_spsc_write_end(&audio_st->pipe_ring, n * fb);
      }
      else
      {
         union { float f[AUDIO_PIPE_CANON_CHANNELS]; int16_t i[AUDIO_PIPE_CANON_CHANNELS]; } frame;
         retro_spsc_write_end(&audio_st->pipe_ring, 0);
         if (retro_spsc_write_avail(&audio_st->pipe_ring) < fb) break;
         /* A physical wrap can split a logical frame. Publish it atomically
          * through the frame writer; never expose a partial frame. */
         audio_driver_pipe_map(&frame, src, 1, channels, pc, layout, slots,
               floating, audio_st->pipe_float);
         n = retro_spsc_write_frames(&audio_st->pipe_ring, &frame, 1, fb);
         if (!n) break;
      }
      done += n;
   }
   return done * fb;
}
#endif

/* Publish native frames. A packed width smaller than the wide ring maps
 * the producer layout into canonical slots; stereo always uses FL/FR. */
static void audio_driver_submit_width(audio_driver_state_t *audio_st,
      float slowmotion_ratio, const void *data, size_t samples, bool is_float,
      bool is_slowmotion, bool is_fastforward, bool from_core,
      unsigned canon_width)
{
#ifdef HAVE_THREADS
   if (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_PIPELINE_THREADED)
   {
      const uint8_t *p = (const uint8_t*)data;
      const unsigned pc = audio_st->pipe_channels ? audio_st->pipe_channels : 2;
      size_t frames    = samples / canon_width;
      size_t len       = frames * audio_st->pipe_frame_bytes;
      size_t input_fb  = canon_width * (is_float ? sizeof(float) : sizeof(int16_t));
      unsigned slots[AUDIO_PIPE_CANON_CHANNELS], bit, count = 0;
      unsigned source_layout = canon_width != pc && pc > 2 ? audio_st->pipe_layout : 0;
      bool mapped;
      if (source_layout)
      {
         for (bit = 0; bit < pc; bit++)
            if (source_layout & (1u << bit)) slots[count++] = bit;
         if (count != canon_width) return;
         audio_st->pipe_layout = source_layout;
      }
      mapped = source_layout || is_float != audio_st->pipe_float;

      /* Rate control's fill, before this frame goes in; see
       * pipe_ctrl_avail. */
      if (     (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_CONTROL)
            && audio_st->current_audio->write_avail
            && audio_st->context_audio_data
            && audio_st->buffer_size)
      {
         /* the device's frame: the pipe's frames and the target are
          * put into the device's bytes, which write_avail() and
          * buffer_size are in - a 5.1 device's are three times a
          * stereo one's, and taken as stereo the target read as a
          * third of the buffer, an occupancy the fifo could never
          * reach: rate control pinned at its bound, "underrun 100%" */
         size_t frame_bytes = audio_driver_dev_frame_bytes(audio_st);
         double pipe_frames = (double)retro_spsc_read_avail(&audio_st->pipe_ring)
               / audio_st->pipe_frame_bytes;
         double pipe_bytes  = pipe_frames * audio_st->src_ratio_orig * frame_bytes;
         double target      = (double)audio_driver_pipe_target_frames(audio_st)
               * audio_st->src_ratio_orig * frame_bytes;
         /* Free space as the controller reads it: the device's, plus
          * enough that the band the consumer actually holds it in reads
          * as no error, plus the pipe target so the pipe holding that
          * much reads as none either, less what the pipe holds.
          *
          * That band is the buffer less a chunk up to the buffer, so
          * the free space averages half a chunk and the controller
          * wants half the buffer; the difference between the two is the
          * correction. It was written out as a quarter of the buffer,
          * which is that difference for a chunk of half the buffer and
          * for no other - a constant that silently depended on the cap
          * in a different function. Derived from the chunk instead, so
          * changing one moves the other. */
         double band = (double)audio_st->buffer_size / 2.0
               - (double)audio_driver_pipe_chunk_bytes(audio_st) / 2.0;
         double eff = (double)audio_st->current_audio->write_avail(
                  audio_st->context_audio_data)
               + band + target - pipe_bytes;
         if (eff < 0.0)
            eff = 0.0;
         else if (eff > (double)audio_st->buffer_size)
            eff = (double)audio_st->buffer_size;
         retro_atomic_store_release_int(&audio_st->pipe_ctrl_avail, (int)eff);

      }
      /* Count before the ring write so dropped source still contributes.
       * Measure once at frame end, independently of publish granularity. */
      if (is_fastforward && audio_driver_ff_follows(audio_st,
               (retro_atomic_load_acquire_int(&audio_st->runloop_snapshot)
                & AUDIO_SNAP_FF_SPEEDUP) != 0))
      {
         /* The first frame of a hold publishes the seed, so the tempo
          * lands with this frame's source rather than a frame later. */
         if (!audio_st->last_flush_time && !audio_st->pipe_ff_frames)
         {
            double ratio = audio_driver_snapshot_ffratio(audio_st);
            if (ratio > 1.0)
               retro_atomic_store_release_int(&audio_st->pipe_ff_mult_q16,
                     (int)(65536.0 / ratio));
         }
         audio_st->pipe_ff_frames = frames > SIZE_MAX - audio_st->pipe_ff_frames
            ? SIZE_MAX : audio_st->pipe_ff_frames + frames;
      }
      else
         audio_driver_ff_mult_reset(audio_st);
      if (len && (audio_st->pipe_transport_follow & AUDIO_TRANSPORT_UPDATE)
            && !audio_driver_transport_update_runloop(audio_st))
         len = 0;
      /* Never publish samples without their preceding layout boundary. */
      if (pc > 2 && len && !audio_pipeline_layout_publish(&audio_st->pipe_layouts,
               retro_atomic_load_relaxed_size(&audio_st->pipe_ring.head),
               audio_st->pipe_layout))
         len = 0;

      /* The resume ramp belongs to the core's first audio, not to
       * whatever the consumer flushes next: mark where that starts. */
      if (from_core && audio_st->fade_in_pending)
      {
         audio_driver_pipe_fade_publish(audio_st, true,
               retro_atomic_load_relaxed_size(&audio_st->pipe_ring.head));
         audio_st->fade_in_pending = false;
      }

      while (len)
      {
         int gen;
         size_t n = mapped
            ? audio_driver_pipe_write_mapped(audio_st, p,
                  len / audio_st->pipe_frame_bytes, canon_width, source_layout, slots, is_float)
            : pc > 2 ? retro_spsc_write_frames(&audio_st->pipe_ring, p,
                  len / audio_st->pipe_frame_bytes, audio_st->pipe_frame_bytes)
                  * audio_st->pipe_frame_bytes
            : retro_spsc_write(&audio_st->pipe_ring, p, len);
         /* The sink estimate's source count: what entered the ring,
          * at the nominal ratio. Counted here, on the thread that
          * closes its windows, so a window holds whole publishes and
          * neither the ring nor what it refused is in the measure. */
         audio_st->sink_offered += (double)(n / audio_st->pipe_frame_bytes)
               * audio_st->src_ratio_orig;
         p       += (n / audio_st->pipe_frame_bytes) * input_fb;
         len     -= n;
         if (!len)
            break;
         /* Only wait on a consumer that can make progress: the driver
          * is started, and blocking or, in fast-forward, pulling at the
          * core's speed. A driver reinit from inside
          * retro_run() (SET_SYSTEM_AV_INFO) creates the wrapper thread
          * parked until the runloop starts it, and the runloop is us;
          * a wrapper whose device write failed exits its loop and will
          * never drain again, and says so through pipe_consumer_gone.
          * Not through the driver's alive(): the wrapper implements
          * that by parking and resuming its thread, which stops and
          * restarts the device every call. */
         if (is_fastforward
               ? !audio_driver_pipe_ff_waits(audio_st)
               : (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_NONBLOCK) != 0)
            break;
         if (     !(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_STARTED)
               || retro_atomic_load_acquire_int(
                     &audio_st->pipe_consumer_gone))
            break;
         /* Sleep until the consumer has completed a pass. The
          * generation is taken first, then the wait window is opened
          * on pipe_space, then the generation is read again inside it:
          * a pass that finished anywhere across that window is either
          * seen by the re-check or stops the commit from sleeping, so
          * a pass between the failed write and the park cannot be
          * missed. No lock is involved - the generation is an atomic
          * and the eventcount only parks.
          *
          * The timed wait bounds the stall if the device stops
          * draining, and a stall found once is not waited on again
          * until a pass completes - otherwise a device that never
          * drains costs every frame the full wait. */
         gen = retro_atomic_load_acquire_int(&audio_st->pipe_gen);
         if (retro_atomic_load_acquire_int(&audio_st->pipe_stalled))
            break;
         if (retro_spsc_write_avail(&audio_st->pipe_ring) < audio_st->pipe_frame_bytes)
         {
            /* One deadline for the whole wait, not one per iteration.
             * The eventcount reports a spurious return as a wake, as a
             * condition variable does, so a bound applied per iteration
             * would be a bound on nothing: enough of them and the
             * frontend sits here for multiples of it. */
            retro_time_t deadline = cpu_features_get_time_usec()
                  + audio_driver_pipe_wait_us(audio_st);
            /* The frame-end signal cannot arrive while its producer waits
             * here. Wake a consumer parked for data before waiting for space. */
            audio_driver_pipeline_signal(audio_st);
            while (retro_atomic_load_acquire_int(&audio_st->pipe_gen) == gen)
            {
               retro_time_t now;
               int key = retro_eventcount_prepare_wait(&audio_st->pipe_space);
               /* Re-read inside the window: a pass that completed
                * between the check above and the registration notifies
                * an eventcount that now has a waiter on it, so this
                * either sees the new generation or the commit below
                * returns at once. Neither can be missed. */
               if (retro_atomic_load_acquire_int(&audio_st->pipe_gen) != gen)
               {
                  retro_eventcount_cancel_wait(&audio_st->pipe_space);
                  break;
               }
               now = cpu_features_get_time_usec();
               if (now >= deadline)
               {
                  retro_eventcount_cancel_wait(&audio_st->pipe_space);
                  break;
               }
               retro_eventcount_commit_wait_timeout(&audio_st->pipe_space,
                     key, (int64_t)(deadline - now));
            }
            if (retro_atomic_load_acquire_int(&audio_st->pipe_gen) == gen)
            {
               retro_atomic_store_release_int(&audio_st->pipe_stalled, 1);
               RARCH_WARN("[Audio] Device stopped draining; dropping audio until it resumes.\n");
               break;
            }
         }
      }
      audio_driver_sink_update(audio_st, cpu_features_get_time_usec());
      /* No wake here: the consumer is woken once per frame by
       * audio_driver_pipeline_signal(), from the frame end and from the
       * other per-frame producers. Waking per publish would have a core
       * that batches per scanline wake it hundreds of times a frame,
       * and a consumer that outranks the main thread would then run a
       * scanline-sized pass each time. */
      return;
   }
#endif
   audio_driver_state_lock();
   if (from_core)
      audio_driver_arm_resume(audio_st);
   if (!audio_st->inline_transport
         || !audio_driver_inline_flush(audio_st, slowmotion_ratio, data, samples,
            is_float, is_slowmotion, is_fastforward))
      audio_driver_flush(audio_st, slowmotion_ratio, data, samples, is_float,
            is_slowmotion, is_fastforward);
   audio_driver_state_unlock();
}

#ifdef HAVE_THREADS
/* Keep infrequent retries out of the steady-state consumer's code path. */
#if defined(__GNUC__)
__attribute__((noinline))
#elif defined(_MSC_VER)
__declspec(noinline)
#endif
static void audio_driver_pipeline_retry(audio_driver_state_t *audio_st)
{
   const audio_driver_t *audio = audio_st->current_audio;
   int snap;
   snap = retro_atomic_load_acquire_int(&audio_st->runloop_snapshot);
   if ((snap & AUDIO_SNAP_PAUSED)
         || !(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_ACTIVE)
         || (audio->underruns && !(snap & AUDIO_SNAP_SYNC)
            && audio->underruns(audio_st->context_audio_data) != audio_st->pipe_underruns_seen))
   {
      audio_st->pipe_pending = NULL;
      audio_st->pipe_pending_bytes = 0;
      audio_driver_state_lock();
      audio_driver_reset_resamplers(audio_st);
      audio_driver_state_unlock();
      return;
   }
   else
   {
      size_t bytes = audio_st->pipe_pending_bytes;
      size_t ready = audio->wait_writable(audio_st->context_audio_data, bytes);
      ssize_t written;
      if (!ready) return;
      if (bytes > ready) bytes = ready;
      audio_driver_state_lock();
      written = audio->write(audio_st->context_audio_data, audio_st->pipe_pending, bytes);
      if (written > 0 && (size_t)written <= bytes)
      {
         size_t fb = audio_driver_dev_frame_bytes(audio_st);
         size_t before = (audio_st->pipe_pending_bytes + fb - 1) / fb;
         audio_st->pipe_pending += written;
         audio_st->pipe_pending_bytes -= written;
         audio_st->sink_accepted += before
            - (audio_st->pipe_pending_bytes + fb - 1) / fb;
      }
      else if (written < 0 || (size_t)written > bytes)
         audio_st->pipe_pending_bytes = 0;
      if (!audio_st->pipe_pending_bytes) audio_st->pipe_pending = NULL;
      audio_driver_state_unlock();
      if (!written)
      {
         /* A driver reporting room but accepting nothing must not spin. */
         if (!retro_atomic_load_acquire_int(&audio_st->pipe_wake))
         {
            int key = retro_eventcount_prepare_wait(&audio_st->pipe_data);
            if (retro_atomic_load_acquire_int(&audio_st->pipe_wake))
               retro_eventcount_cancel_wait(&audio_st->pipe_data);
            else
               retro_eventcount_commit_wait_timeout(&audio_st->pipe_data,
                     key, 1000);
         }
      }
      return;
   }
}

/* Caller owns this native block and has resolved pending device output.
 * Wide source must not overlap the stereo scratch used for channel splitting. */
static INLINE void audio_driver_pipeline_render(audio_driver_state_t *audio_st,
      const void *source, size_t have, uint32_t layout, int snap)
{
   const void *front = source;
   audio_driver_state_lock();
   /* A pause that landed since this chunk was taken has already ended
    * the stream; the chunk is stale behind its tail. */
   if (retro_atomic_load_acquire_int(&audio_st->pipe_discard_gen)
         != audio_st->pipe_discard_seen)
   {
      audio_st->pipe_arm_fade = false;
      audio_driver_state_unlock();
      return;
   }
   if (audio_st->pipe_arm_fade)
   {
      audio_st->fade_in_frames = AUDIO_PAUSE_TAIL_FRAMES;
      audio_st->pipe_arm_fade  = false;
   }
   if (audio_st->pipe_channels > 2)
   {
      const unsigned pc = audio_st->pipe_channels ? audio_st->pipe_channels : 2;
      uint32_t pos      = layout & ~AUDIO_LAYOUT_STEREO;
      unsigned ex       = audio_layout_channels(pos);
      unsigned slot[AUDIO_PIPE_CANON_CHANNELS];
      unsigned bit, n = 0;
      size_t f; unsigned c;
      for (bit = 2; bit < pc; bit++)
         if (pos & (1u << bit))
            slot[n++] = bit;
      if (ex && audio_driver_extra_prepare(audio_st, ex, pos,
               have, audio_st->pipe_float, audio_driver_mixer_use_s16(audio_st->pipe_float)))
      {
         if (audio_st->pipe_float)
         {
            const float *w = (const float*)source;
            float *fr = (float*)audio_st->pipe_scratch;
            for (f = 0; f < have; f++)
            {
               fr[2 * f]     = w[f * pc];
               fr[2 * f + 1] = w[f * pc + 1];
               for (c = 0; c < ex; c++)
                  audio_st->extra.in_f[f * ex + c] = w[f * pc + slot[c]];
            }
         }
         else
         {
            const int16_t *w = (const int16_t*)source;
            int16_t *fr = (int16_t*)audio_st->pipe_scratch;
            for (f = 0; f < have; f++)
            {
               fr[2 * f]     = w[f * pc];
               fr[2 * f + 1] = w[f * pc + 1];
               for (c = 0; c < ex; c++)
                  audio_st->extra.in_i[f * ex + c] = w[f * pc + slot[c]];
            }
         }
         audio_st->extra.pending = true;
      }
      else
      {
         /* stereo in the wide frame, or no room for the extras: the
          * fronts alone */
         if (audio_st->pipe_float)
         {
            const float *w = (const float*)source;
            float *fr = (float*)audio_st->pipe_scratch;
            for (f = 0; f < have; f++) { fr[2 * f] = w[f * pc]; fr[2 * f + 1] = w[f * pc + 1]; }
         }
         else
         {
            const int16_t *w = (const int16_t*)source;
            int16_t *fr = (int16_t*)audio_st->pipe_scratch;
            for (f = 0; f < have; f++) { fr[2 * f] = w[f * pc]; fr[2 * f + 1] = w[f * pc + 1]; }
         }
      }
      front = audio_st->pipe_scratch;
   }
   audio_driver_flush(audio_st,
         audio_driver_snapshot_slowmotion(audio_st),
         front, have * 2, audio_st->pipe_float,
         (snap & AUDIO_SNAP_SLOWMOTION) ? true : false,
         (snap & AUDIO_SNAP_FASTMOTION) ? true : false);
   audio_st->extra.pending = false;
   audio_driver_state_unlock();
}

bool audio_driver_pipeline_transport_request_speed(uint32_t tempo_q16,
      bool active, bool reset, bool lowpass)
{
   return audio_driver_pipeline_transport_request(tempo_q16, active, reset,
         lowpass ? audio_speed_lpf_cutoff(audio_driver_st.pipe_transport_rate,
            tempo_q16) : 0);
}

static bool audio_driver_transport_runloop_tempo(uint32_t *tempo)
{
   settings_t *settings = config_get_ptr();
   uint32_t flags = runloop_get_flags();
   uint64_t bits;
   double duration = 1.0;
   if (!(flags & RUNLOOP_FLAG_PAUSED))
   {
      if (flags & RUNLOOP_FLAG_SLOWMOTION)
         duration *= settings->floats.slowmotion_ratio;
      /* Same rule as audio_driver_publish_runloop(): fast-forward
       * does not touch a callback core's device-paced audio unless
       * the person opted in. */
      if ((flags & RUNLOOP_FLAG_FASTMOTION)
            && (   !audio_driver_st.callback.callback
                || settings->bools.audio_fastforward_callback)
            && audio_driver_ff_follows(&audio_driver_st,
               settings->bools.audio_fastforward_speedup))
         duration *= retro_atomic_load_acquire_int(
               &audio_driver_st.pipe_ff_mult_q16) / 65536.0;
   }
   /* Check before converting: also rejects NaN, infinities and an unseeded
    * fast-forward estimate. Unsupported duration needs caller fallback. */
   memcpy(&bits, &duration, sizeof(bits));
   if (bits >= UINT64_C(0x7ff0000000000000)
         || !(duration >= 1.0 / 32.0 && duration <= 4.0)) return false;
   *tempo = (uint32_t)(65536.0 / duration + 0.5);
   return true;
}

/* The submit path's variant: the tempo from the published snapshot,
 * so a callback core's audio thread never reads the runloop or
 * settings. Configure-time callers keep the live derivation above. */
static bool audio_driver_transport_snapshot_tempo(uint32_t *tempo)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   int snap = retro_atomic_load_acquire_int(&audio_st->runloop_snapshot);
   uint64_t bits;
   double duration = 1.0;
   if (!(snap & AUDIO_SNAP_PAUSED))
   {
      if (snap & AUDIO_SNAP_SLOWMOTION)
         duration *= audio_driver_snapshot_slowmotion(audio_st);
      if ((snap & AUDIO_SNAP_FASTMOTION) && audio_driver_ff_follows(audio_st,
               (snap & AUDIO_SNAP_FF_SPEEDUP) != 0))
         duration *= retro_atomic_load_acquire_int(
               &audio_st->pipe_ff_mult_q16) / 65536.0;
   }
   memcpy(&bits, &duration, sizeof(bits));
   if (bits >= UINT64_C(0x7ff0000000000000)
         || !(duration >= 1.0 / 32.0 && duration <= 4.0)) return false;
   *tempo = (uint32_t)(65536.0 / duration + 0.5);
   return true;
}

bool audio_driver_pipeline_transport_request_runloop(bool reset, bool lowpass)
{
   uint32_t tempo;
   if (!audio_driver_transport_runloop_tempo(&tempo)) return false;
   return audio_driver_pipeline_transport_request_speed(tempo,
         !audio_driver_st.transport_lpf_only && tempo != 65536, reset, lowpass);
}

static bool audio_driver_transport_update_runloop(audio_driver_state_t *audio_st)
{
   uint32_t tempo;
   if (!audio_driver_transport_snapshot_tempo(&tempo))
   {
      if (audio_st->pipe_transport && !audio_driver_transport_recover(false, 65536))
         return false;
      audio_st->pipe_transport_follow &= ~AUDIO_TRANSPORT_UPDATE;
      return true;
   }
   if (audio_st->pipe_transport_suspended)
   {
      /* Keep publishing legacy source until both queues can hand over. */
      if (retro_spsc_read_avail(&audio_st->pipe_ring)
            || !audio_driver_transport_recover(true, tempo))
      {
         audio_st->pipe_transport_follow &= ~AUDIO_TRANSPORT_UPDATE;
         return true;
      }
   }
   if (!audio_driver_pipeline_transport_publish(tempo,
            !audio_st->transport_lpf_only && tempo != 65536, false,
            (audio_st->pipe_transport_follow & AUDIO_TRANSPORT_LOWPASS)
            ? audio_speed_lpf_cutoff(audio_st->pipe_transport_rate, tempo) : 0,
            true))
      return false;
   audio_st->pipe_transport_follow &= ~AUDIO_TRANSPORT_UPDATE;
   return true;
}

static bool audio_driver_pipeline_transport_publish(uint32_t tempo_q16,
      bool active, bool reset, uint32_t cutoff, bool engage_ahead)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   audio_pipeline_layout_t *metadata = &audio_st->pipe_layouts;
   size_t head, position;
   bool result;
   if (!audio_st->pipe_threaded || !audio_st->pipe_transport)
      return false;
   head     = retro_atomic_load_relaxed_size(&metadata->head);
   position = retro_atomic_load_relaxed_size(&audio_st->pipe_ring.head);
   /* Entering fast-forward the ring holds a few frames of 1x source.
    * Positioned behind it, the tempo would only take effect once that
    * played out at 1x, with the core running ahead into a full ring
    * meanwhile; put it at the consumer's position instead, when no
    * earlier event is pending so the queue stays in order. */
   if (     engage_ahead && active && tempo_q16 > 65536
         && !(metadata->published_control & AUDIO_PIPELINE_STRETCH)
         && head == retro_atomic_load_acquire_size(&metadata->tail))
      position = retro_atomic_load_acquire_size(&audio_st->pipe_ring.tail);
   result = audio_pipeline_layout_publish_processing(metadata, position,
         metadata->published_layout, tempo_q16, active, reset, cutoff);
   if (retro_atomic_load_relaxed_size(&metadata->head) != head)
      audio_driver_pipeline_signal(audio_st);
   return result;
}

bool audio_driver_pipeline_transport_request(uint32_t tempo_q16,
      bool active, bool reset, uint32_t cutoff)
{
   return audio_driver_pipeline_transport_publish(tempo_q16, active, reset,
         cutoff, false);
}

static bool audio_driver_transport_discard(size_t frames, bool draining)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   if (!audio_st->pipe_transport
         || !audio_pipeline_stretch_discard(audio_st->pipe_transport, frames))
      return false;
   audio_st->pipe_pending = NULL;
   audio_st->pipe_pending_bytes = 0;
   audio_driver_state_lock();
   audio_driver_reset_resamplers(audio_st);
   audio_driver_state_unlock();
   if (frames)
   {
      audio_driver_pipeline_pass_done(audio_st, draining);
   }
   return true;
}

bool audio_driver_pipeline_transport_discard(size_t frames)
{
   return audio_driver_transport_discard(frames, true);
}

static bool audio_driver_pipeline_transport_run(struct audio_pipeline_stretch *stage,
      uint32_t *serial, size_t input_budget, size_t output_budget,
      bool finishing, bool *complete, bool *progress)
{
   int snap;
   double ratio;
   size_t frame_bytes, cap, released;
   struct audio_pipeline_stretch_block block;
   audio_driver_state_t *audio_st = &audio_driver_st;
   const audio_driver_t *audio    = audio_st->current_audio;
   *progress = false;
   if (!stage || !serial || !complete || !audio_st->pipe_threaded
         || !audio || !audio->wait_writable || !audio_st->context_audio_data)
      return false;
   *complete = false;
   if (!output_budget) return true;
   snap = retro_atomic_load_acquire_int(&audio_st->runloop_snapshot);
   if ((snap & AUDIO_SNAP_PAUSED)
         || !(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_ACTIVE)
         || !audio_st->output_samples_buf
         || (audio->underruns && !(snap & AUDIO_SNAP_SYNC)
            && audio->underruns(audio_st->context_audio_data) != audio_st->pipe_underruns_seen))
      return false;
   if (audio_st->pipe_pending_bytes)
   {
      audio_driver_pipeline_retry(audio_st);
      *progress = true;
      return true;
   }
   /* WSOLA owns duration only when pitch preservation is selected. A
    * filter-only pass keeps the ordinary SRC ratio and device-space bound. */
   if (!audio_st->transport_lpf_only)
   {
      snap &= ~(AUDIO_SNAP_SLOWMOTION | AUDIO_SNAP_FASTMOTION);
      ratio = audio_driver_effective_ratio(audio_st, false, 1.0f, 1.0);
   }
   else
      ratio = audio_driver_effective_ratio(audio_st,
            (snap & AUDIO_SNAP_SLOWMOTION) != 0,
            audio_driver_snapshot_slowmotion(audio_st),
            ((snap & AUDIO_SNAP_FASTMOTION)
             && (snap & AUDIO_SNAP_FF_SPEEDUP))
               ? audio_driver_ff_mult(audio_st, input_budget) : 1.0);
   frame_bytes = audio_driver_dev_frame_bytes(audio_st);
   if (output_budget > audio_st->pipe_pass_frames)
      output_budget = audio_st->pipe_pass_frames;
   if (audio_st->buffer_size)
   {
      cap = audio_driver_input_bound(ratio,
            audio_driver_pipe_chunk_bytes(audio_st) / frame_bytes);
      if (cap < 32) cap = 32;
      if (output_budget > cap) output_budget = cap;
   }
   if (!output_budget) return true;
   if (!audio->wait_writable(audio_st->context_audio_data,
            audio_driver_output_bound(ratio, output_budget) * frame_bytes))
      return true;
   if (finishing)
   {
      if (!audio_pipeline_stretch_finish(stage, output_budget, &block, complete))
         return false;
   }
   else if (!audio_pipeline_stretch_next(stage, input_budget, output_budget, &block))
      return false;
   if (*serial != block.reset_serial)
   {
      audio_driver_state_lock();
      audio_driver_reset_resamplers(audio_st);
      audio_driver_state_unlock();
      *serial = block.reset_serial;
   }
   released = block.input_used;
   *progress = released || block.frames;
   if (block.frames)
   {
      audio_driver_pipeline_render(audio_st, block.data, block.frames,
            block.layout, snap);
      if (!audio_pipeline_stretch_consume(stage, block.frames)) return false;
      if (block.passthrough) released += block.frames;
   }
   if (released)
   {
      audio_driver_pipeline_pass_done(audio_st, true);
   }
   return true;
}

bool audio_driver_pipeline_transport_step(struct audio_pipeline_stretch *stage,
      uint32_t *serial, size_t input_budget, size_t output_budget,
      bool finishing, bool *complete)
{
   bool progress;
   return audio_driver_pipeline_transport_run(stage, serial, input_budget,
         output_budget, finishing, complete, &progress);
}

static bool audio_driver_transport_wait(audio_driver_state_t *st, size_t need,
      unsigned metadata_threshold)
{
   size_t events;
   bool ready;
   int key;
   if (retro_atomic_load_acquire_int(&st->pipe_wake))
   {
      retro_atomic_store_release_int(&st->pipe_wake, 0);
      return false;
   }
   events = retro_atomic_load_acquire_size(&st->pipe_layouts.head)
      - retro_atomic_load_relaxed_size(&st->pipe_layouts.tail);
   ready = retro_spsc_read_avail(&st->pipe_ring) >= need
      || (metadata_threshold && events >= metadata_threshold);
   if (ready)
      return true;
   /* Registered before the conditions are read again, so a publish or a
    * wake that lands from here on either shows up in the re-check or
    * makes the commit below return at once. */
   key    = retro_eventcount_prepare_wait(&st->pipe_data);
   events = retro_atomic_load_acquire_size(&st->pipe_layouts.head)
      - retro_atomic_load_relaxed_size(&st->pipe_layouts.tail);
   if (    retro_spsc_read_avail(&st->pipe_ring) >= need
        || (metadata_threshold && events >= metadata_threshold)
        || retro_atomic_load_acquire_int(&st->pipe_wake))
      retro_eventcount_cancel_wait(&st->pipe_data);
   else
      retro_eventcount_commit_wait_timeout(&st->pipe_data, key,
            AUDIO_PIPE_WAIT_MAX_US);
   if (retro_atomic_load_acquire_int(&st->pipe_wake))
   {
      retro_atomic_store_release_int(&st->pipe_wake, 0);
      return false;
   }
   events = retro_atomic_load_acquire_size(&st->pipe_layouts.head)
      - retro_atomic_load_relaxed_size(&st->pipe_layouts.tail);
   return retro_spsc_read_avail(&st->pipe_ring) >= need
      || (metadata_threshold && events >= metadata_threshold);
}

static void audio_driver_transport_consume(audio_driver_state_t *st)
{
   bool complete, progress;
   const audio_driver_t *audio = st->current_audio;
   size_t input_budget         = st->pipe_pass_frames;
   size_t held = retro_spsc_read_avail(&st->pipe_ring) / st->pipe_frame_bytes;
   int snap    = retro_atomic_load_acquire_int(&st->runloop_snapshot);
   if ((snap & AUDIO_SNAP_PAUSED) || !(AUDIO_FLAGS_GET(st) & AUDIO_FLAG_ACTIVE)
         || !st->output_samples_buf)
   {
      if (held > st->pipe_pass_frames) held = st->pipe_pass_frames;
      audio_driver_transport_discard(held, false);
      if (!held) audio_driver_transport_wait(st, st->pipe_frame_bytes, 0);
      return;
   }

   if (audio->underruns && !(snap & AUDIO_SNAP_SYNC))
   {
      size_t seen = audio->underruns(st->context_audio_data);
      if (seen != st->pipe_underruns_seen)
      {
         size_t target = audio_driver_pipe_target_frames(st);
         if (!audio_driver_pipeline_transport_discard(held > target ? held - target : 0))
            return;
         st->pipe_underruns_seen = seen;
         held = retro_spsc_read_avail(&st->pipe_ring) / st->pipe_frame_bytes;
      }
   }
   if (st->pipe_priming)
   {
      size_t target = audio_driver_pipe_target_frames(st);
      size_t need = 1;
      if (target)
      {
         size_t room = st->pipe_ring.capacity / st->pipe_frame_bytes;
         size_t device = (size_t)((double)st->buffer_size
               / audio_driver_dev_frame_bytes(st) / st->src_ratio_orig);
         size_t limit = room > st->pipe_pass_frames ? room - st->pipe_pass_frames : 1;
         need = target + device;
         if (need > limit) need = limit;
      }
      if (!audio_driver_transport_wait(st, need * st->pipe_frame_bytes,
               AUDIO_PIPELINE_LAYOUT_CAPACITY))
         return;
      st->pipe_priming = false;
   }
   else if (!held && !st->pipe_pending_bytes
         && audio_pipeline_stretch_needs_input(st->pipe_transport)
         && !audio_driver_transport_wait(st, st->pipe_frame_bytes, 1))
      return;
   /* What a pause left in the ring is stale behind its tail, and so is
    * what the stage holds: taken out unplayed, up to where the pause was
    * published. */
   {
      size_t tail = retro_atomic_load_relaxed_size(&st->pipe_ring.tail);
      int    gen  = retro_atomic_load_acquire_int(&st->pipe_discard_gen);
      if (gen != st->pipe_discard_seen)
      {
         size_t to = retro_atomic_load_acquire_size(&st->pipe_discard_to);
         if (audio_driver_pipe_ahead(st, tail, to))
         {
            size_t frames = (to - tail) / st->pipe_frame_bytes;
            if (frames > held)
               frames = held;
            if (frames > st->pipe_pass_frames)
               frames = st->pipe_pass_frames;
            audio_driver_transport_discard(frames, false);
            return;
         }
         audio_driver_transport_discard(0, false);
         st->pipe_discard_seen = gen;
      }
      /* Never across the point the core's audio resumes at: there the
       * stage drops the silence it holds and the ramp is armed for what
       * it renders next. */
      {
         size_t   at;
         unsigned seq;
         if (audio_driver_pipe_fade_pending(st, &at, &seq))
         {
            if (audio_driver_pipe_ahead(st, tail, at))
            {
               size_t upto = (at - tail) / st->pipe_frame_bytes;
               if (input_budget > upto)
                  input_budget = upto;
            }
            else
            {
               audio_driver_transport_discard(0, false);
               st->pipe_arm_fade     = true;
               st->pipe_fade_in_seen = seq;
            }
         }
      }
   }
   if (!audio_driver_pipeline_transport_run(st->pipe_transport,
            &st->pipe_transport_serial, input_budget,
            st->pipe_pass_frames, false, &complete, &progress))
      return;
   /* Drain retained synthesis before sleeping for more source. */
   if (!progress && !st->pipe_pending_bytes)
      audio_driver_transport_wait(st, st->pipe_frame_bytes, 1);
}

/**
 * audio_driver_pipeline_consume:
 *
 * Consumer side of the threaded pipeline, called from the wrapper
 * thread's loop. Pulls up to one slice out of pipe_ring and runs the
 * pipeline on it; the write at the end goes to the real driver through
 * the wrapper and blocks when the device is full, which is what paces
 * this loop. With nothing to pull it parks on the pipe_data
 * eventcount rather than spin, with a bound so a missed notify costs a
 * millisecond rather than the stream.
 *
 * The producer does notify it - once per frame, from
 * audio_driver_pipeline_signal(), and not per publish. Some cores hand
 * over audio a scanline at a time, and notifying on every
 * retro_spsc_write() would turn one frame's audio into hundreds of
 * tiny passes; pipeline_wakeups measures exactly that, and measures
 * 3.0 wakes a frame against 211 for the per-publish shape. The ring
 * carries the data, so a publish needs no announcement.
 **/
static void audio_driver_pipeline_consume(audio_driver_state_t *audio_st)
{
   void    *source;
   int      snap;
   double   out_ratio;
   size_t   frame_bytes, out_bytes, have;
   const audio_driver_t *audio = audio_st->current_audio;

   if (audio_st->pipe_pending_bytes)
   {
      audio_driver_pipeline_retry(audio_st);
      return;
   }

   /* First wait: enough published audio for a pass. The producer
    * advances pipe_data_gen and notifies once per frame, not once per
    * publish - the paragraph on audio_driver_pipeline_consume() above
    * says why, and this comment used to say the opposite of it.
    *
    * The wait window is opened on pipe_data before the ring and the
    * generation are read again, so a frame published between the first
    * test and the park cannot be slept through. No lock is involved.
    *
    * A timed wait is only a guard against a stopped producer: the
    * wrapper parks this thread when the driver is stopped, so in
    * normal operation nothing here ever times out. */
   /* The first pass waits for the pipe's target and the device's
    * buffer, not just a frame: with a non-blocking writer the pipe is
    * to hold a buffer's worth ahead of a device that starts empty, and
    * it is cheaper to start that far behind once than to have rate
    * control build it at half a percent. */
   {
      size_t need = audio_st->pipe_frame_bytes;
      if (audio_st->pipe_priming)
      {
         size_t target = audio_driver_pipe_target_frames(audio_st);
         if (target)
         {
            size_t frame_bytes = audio_driver_dev_frame_bytes(audio_st);
            size_t device = (size_t)((double)audio_st->buffer_size / frame_bytes
                  / audio_st->src_ratio_orig);
            size_t room   = audio_st->pipe_ring.capacity / audio_st->pipe_frame_bytes;
            target += device;
            if (target > room - audio_st->pipe_pass_frames)
               target = room - audio_st->pipe_pass_frames;
            need = target * audio_st->pipe_frame_bytes;
         }
      }
   while (retro_spsc_read_avail(&audio_st->pipe_ring) < need)
   {
      int gen, key;
      /* A wake means the wrapper wants this thread back at its loop -
       * stop, free, or a reinit - not that there is data. Return so it
       * can see why. */
      if (retro_atomic_load_acquire_int(&audio_st->pipe_wake))
      {
         retro_atomic_store_release_int(&audio_st->pipe_wake, 0);
         return;
      }
      /* A full metadata queue must not deadlock startup priming. */
      if (audio_st->pipe_channels > 2
            && retro_atomic_load_acquire_size(&audio_st->pipe_layouts.head)
             - retro_atomic_load_relaxed_size(&audio_st->pipe_layouts.tail)
                  == AUDIO_PIPELINE_LAYOUT_CAPACITY)
      {
         size_t bytes = retro_spsc_read_avail(&audio_st->pipe_ring);
         if (bytes >= audio_st->pipe_frame_bytes) break;
         audio_pipeline_layout_limit(&audio_st->pipe_layouts,
               retro_atomic_load_relaxed_size(&audio_st->pipe_ring.tail),
               0, audio_st->pipe_ring.capacity);
      }
      gen = retro_atomic_load_acquire_int(&audio_st->pipe_data_gen);
      key = retro_eventcount_prepare_wait(&audio_st->pipe_data);
      /* The re-check the condition variable used to get from holding
       * the lock across it: a frame signalled between the ring test
       * above and this registration cannot be slept through. */
      if (    retro_spsc_read_avail(&audio_st->pipe_ring) >= need
           || retro_atomic_load_acquire_int(&audio_st->pipe_wake)
           || retro_atomic_load_acquire_int(&audio_st->pipe_data_gen) != gen)
      {
         retro_eventcount_cancel_wait(&audio_st->pipe_data);
         continue;
      }
      if (!retro_eventcount_commit_wait_timeout(&audio_st->pipe_data, key,
               AUDIO_PIPE_WAIT_MAX_US))
      {
         /* Nothing came within the bound. A producer that has stopped
          * is the wrapper's business, not this pass's. */
         if (retro_atomic_load_acquire_int(&audio_st->pipe_data_gen) == gen)
            return;
      }
   }
   audio_st->pipe_priming = false;
   }

   have  = retro_spsc_read_avail(&audio_st->pipe_ring) / audio_st->pipe_frame_bytes;
   if (have > audio_st->pipe_pass_frames)
      have = audio_st->pipe_pass_frames;

   /* Never write more than half the device buffer in one pass, so the
    * room waited for below exists at any fill and a small buffer never
    * has to drain to empty before it can take a chunk. Whatever is left
    * stays in the ring and goes on the next pass without waiting. */
   /* The device's frame, whatever the layout: taken as stereo on a
    * 5.1 device, the cap was three times half the buffer and the room
    * waited for a third of what the write needed, so a chunk could
    * arrive at a fifo without room for it - and a driver that frames
    * its output, as the AC-3 path does, then wrote part of a burst. */
   frame_bytes = audio_driver_dev_frame_bytes(audio_st);
   /* The ratio the flush will use, needed here as well as at the wait
    * below: the cap and the room asked for have to be the same
    * arithmetic or the driver is asked to accept more than the cap was
    * sized for. On this path the fast-forward multiplier is a value the
    * producer published and does not depend on the frame count passed
    * to it, so taking it before the cap is the same number as after. */
   snap        = retro_atomic_load_acquire_int(&audio_st->runloop_snapshot);
   out_ratio   = audio_driver_effective_ratio(audio_st,
         (snap & AUDIO_SNAP_SLOWMOTION) ? true : false,
         audio_driver_snapshot_slowmotion(audio_st),
         (   (snap & AUDIO_SNAP_FASTMOTION)
          && (snap & AUDIO_SNAP_FF_SPEEDUP))
            ? audio_driver_ff_mult(audio_st, have) : 1.0);
   if (audio_st->buffer_size)
   {
      /* The cap is the inverse of the bound, not the ratio: what this
       * pass may take is whatever the write it turns into still fits in
       * the chunk. Dividing by the ratio alone left the bound's
       * sixteen-frame margin outside the cap, so out_bytes came out a
       * little over the chunk on every pass - and a driver whose
       * wait_writable() tops the request out then had to either block
       * inside the write or drop the tail, neither of which the pass
       * had waited for.
       *
       * Applied whatever it works out to, with a floor rather than an
       * escape: it used to be skipped entirely below 32 frames, which
       * on a small buffer left have at a whole publish and asked the
       * driver to wait for a chunk larger than its ring - a wait that
       * can only end with the device empty. */
      size_t cap = audio_driver_input_bound(out_ratio,
            audio_driver_pipe_chunk_bytes(audio_st) / frame_bytes);
      if (cap < 32)
         cap = 32;
      if (have > cap)
         have = cap;
   }

   /* Late audio is not kept. A core that stalls leaves the device
    * playing silence for the stall, then delivers the frames it missed
    * in a burst; kept, they would play late, the output behind by the
    * stall for good, rate control pinned against the backlog, the ring
    * filling until it dropped. When the driver says it has played
    * silence since the last pass, what the pipe holds past its target
    * arrived after that silence and is discarded here, on the thread
    * that reads the ring. On no other occasion is anything dropped:
    * the pipe's fill swings by a chunk with the phase between the
    * core's publish and this pass, and a threshold on it alone dropped
    * healthy audio at some phases and none at others. */
   if (     audio->underruns && audio_st->context_audio_data
         && !(snap & AUDIO_SNAP_SYNC))
   {
      size_t seen = audio->underruns(audio_st->context_audio_data);
      if (seen != audio_st->pipe_underruns_seen)
      {
         size_t target = audio_driver_pipe_target_frames(audio_st) * audio_st->pipe_frame_bytes;
         size_t held   = retro_spsc_read_avail(&audio_st->pipe_ring);
         bool discarded = false;
         audio_st->pipe_underruns_seen = seen;
         while (held > target)
         {
            size_t take = held - target;
            if (take > audio_st->pipe_pass_frames * audio_st->pipe_frame_bytes)
               take = audio_st->pipe_pass_frames * audio_st->pipe_frame_bytes;
            /* Discarded, not consumed: skip the bytes in place rather
             * than copy them out to be thrown away. */
            if (!retro_spsc_skip(&audio_st->pipe_ring, take))
               break;
            held -= take;
            discarded = true;
         }
         if (discarded)
         {
            audio_driver_state_lock();
            audio_driver_reset_resamplers(audio_st);
            audio_driver_state_unlock();
         }
         if (have > held / audio_st->pipe_frame_bytes)
            have = held / audio_st->pipe_frame_bytes;
      }
   }

   /* Discards can cross boundaries and reduce the earlier availability. */
   {
      size_t bytes = have * audio_st->pipe_frame_bytes;
      if (audio_st->pipe_channels > 2)
      {
         size_t limited = audio_pipeline_layout_limit(&audio_st->pipe_layouts,
               retro_atomic_load_relaxed_size(&audio_st->pipe_ring.tail),
               bytes, audio_st->pipe_ring.capacity);
         if (limited < bytes) have = limited / audio_st->pipe_frame_bytes;
      }
      if (!have)
      {
         audio_driver_pipeline_pass_done(audio_st, true);
         return;
      }
   }

   /* What a pause left in the ring is stale behind its tail: taken out
    * unplayed, up to where the pause was published. */
   {
      size_t tail = retro_atomic_load_relaxed_size(&audio_st->pipe_ring.tail);
      int    gen  = retro_atomic_load_acquire_int(&audio_st->pipe_discard_gen);
      if (gen != audio_st->pipe_discard_seen)
      {
         size_t to = retro_atomic_load_acquire_size(&audio_st->pipe_discard_to);
         if (audio_driver_pipe_ahead(audio_st, tail, to))
         {
            size_t stale = to - tail;
            size_t avail = retro_spsc_read_avail(&audio_st->pipe_ring);
            retro_spsc_skip(&audio_st->pipe_ring, stale < avail ? stale : avail);
            audio_driver_state_lock();
            audio_driver_reset_resamplers(audio_st);
            audio_driver_state_unlock();
            audio_driver_pipeline_pass_done(audio_st, false);
            return;
         }
         audio_st->pipe_discard_seen = gen;
      }
      /* Never across the point the core's audio resumes at: the ramp is
       * armed for the chunk that starts there. */
      {
         size_t   at;
         unsigned seq;
         if (audio_driver_pipe_fade_pending(audio_st, &at, &seq))
         {
            if (audio_driver_pipe_ahead(audio_st, tail, at))
            {
               size_t upto = (at - tail) / audio_st->pipe_frame_bytes;
               if (have > upto)
                  have = upto;
            }
            else
            {
               audio_st->pipe_arm_fade     = true;
               audio_st->pipe_fade_in_seen = seq;
            }
         }
      }
   }

   if (    (snap & AUDIO_SNAP_PAUSED)
        || !(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_ACTIVE)
        || !audio_st->output_samples_buf)
   {
      /* Nothing is going to the device while paused; the chunk is
       * dropped so the ring keeps flowing for the producer.  Skipped
       * in place - nothing reads it, so nothing needs the copy. */
      retro_spsc_skip(&audio_st->pipe_ring, have * audio_st->pipe_frame_bytes);
      audio_driver_state_lock();
      audio_driver_reset_resamplers(audio_st);
      audio_driver_state_unlock();
      audio_driver_pipeline_pass_done(audio_st, false);
      return;
   }

   /* Second wait: room for this chunk, so the write inside flush()
    * returns at once and the fill rate control reads before it is the
    * device's real fill, as it is for the inline path. The chunk is
    * one publish's worth resampled, so a little more than have*ratio;
    * wait for that with a small margin.
    *
    * Waited for before the chunk leaves the ring. A device that makes
    * no room gets nothing taken on its behalf: the ring stays full, the
    * producer's one bounded wait finds it so, and it drops at full
    * frame rate until a pass completes. Taking the chunk first and then
    * finding no room threw it away while telling the producer the ring
    * had drained, and paced the frontend at one frame per failed pass. */
   /* What this chunk will produce at the ratio the flush will use -
    * slow motion and fast-forward's speedup in, the same composition -
    * so the room waited for is the room the write needs. */
   out_bytes   = audio_driver_output_bound(out_ratio, have) * frame_bytes;
   if (!audio->wait_writable(audio_st->context_audio_data, out_bytes))
      return;

   source = audio_st->pipe_channels > 2 ? audio_st->pipe_wide : audio_st->pipe_scratch;
   retro_spsc_read(&audio_st->pipe_ring, source, have * audio_st->pipe_frame_bytes);

   /* Let a throttled producer know ring space has opened - and that
    * the device is draining again, if it had been found stalled. */
   audio_driver_pipeline_pass_done(audio_st, true);

   audio_driver_pipeline_render(audio_st, source,
         have, audio_st->pipe_layouts.current_layout, snap);
}
#endif

/* Recording keeps its native int16 bypass; conversion/remap uses one chunk. */
static void audio_driver_record_push(audio_driver_state_t *audio_st,
      const void *data, size_t frames, unsigned channels, uint32_t layout, bool is_float)
{
   recording_state_t *record_st = recording_state_get_ptr();
   struct record_audio_data ffemu_data;
   uint32_t rlayout   = record_st->layout ? record_st->layout : AUDIO_LAYOUT_STEREO;
   unsigned rchannels = record_st->channels ? record_st->channels : 2;
   size_t capacity, need;
   if (!record_st->data || !record_st->driver || !record_st->driver->push_audio)
      return;
   if (!is_float && rlayout == layout)
   {
      ffemu_data.data   = data;
      ffemu_data.frames = frames;
      record_st->driver->push_audio(record_st->data, &ffemu_data);
      return;
   }
   if (!frames) return;
   capacity = frames > (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1)
      ? (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1) : frames;
   need = capacity * (channels > rchannels ? channels : rchannels);
   if (need > audio_st->record_remap_frames)
   {
      int16_t *nb = (int16_t*)realloc(audio_st->record_remap, need * 2 * sizeof(int16_t));
      if (!nb) return;
      audio_st->record_remap        = nb;
      audio_st->record_remap_frames = need;
   }
   while (frames)
   {
      size_t n = frames > capacity ? capacity : frames;
      const int16_t *src = (const int16_t*)data;
      if (is_float)
      {
         /* Separate halves keep conversion input out of the remap output. */
         int16_t *n16 = audio_st->record_remap + need;
         convert_float_to_s16(n16, (const float*)data, n * channels);
         src = n16;
      }
      if (rlayout != layout)
      {
         audio_layout_remap_s16(audio_st->record_remap, rlayout, src, layout, n);
         src = audio_st->record_remap;
      }
      ffemu_data.data   = src;
      ffemu_data.frames = n;
      record_st->driver->push_audio(record_st->data, &ffemu_data);
      data = (const uint8_t*)data + n * channels * (is_float ? sizeof(float) : sizeof(int16_t));
      frames -= n;
   }
}

/**
 * audio_driver_sample_accum_flush:
 *
 * Hands everything the single-sample callback has accumulated to
 * recording and to audio_driver_flush(), then empties the accumulator.
 * Shared by the overflow guard in audio_driver_sample(), the frame-end
 * flush on the main thread and the post-callback flush on the audio
 * thread, so the three sites cannot drift apart in what they check.
 * The caller guarantees data_ptr > 0 and that it runs on the thread
 * that filled the accumulator.
 **/
static void audio_driver_sample_accum_flush(audio_driver_state_t *audio_st)
{
   /* Runs on the main thread from the frame end, or on the audio thread
    * for a core with its own audio callback; the snapshot is right for
    * both, the runloop's own flag word only for the first. */
   int snap                        = retro_atomic_load_acquire_int(
         &audio_st->runloop_snapshot);
   audio_driver_record_push(audio_st, audio_st->sample_accum,
         audio_st->data_ptr / 2, 2, AUDIO_LAYOUT_STEREO, false);

   if (!(    (snap & AUDIO_SNAP_PAUSED)
         ||  audio_driver_core_silenced()
         || !(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_ACTIVE)
         || !(audio_st->output_samples_buf)))
      audio_driver_submit(audio_st,
            audio_driver_snapshot_slowmotion(audio_st),
            audio_st->sample_accum,
            audio_st->data_ptr, false,
            (snap & AUDIO_SNAP_SLOWMOTION) ? true : false,
            (snap & AUDIO_SNAP_FASTMOTION) ? true : false, true);

   audio_st->data_ptr = 0;
}

void audio_driver_sample(int16_t left, int16_t right)
{
   audio_driver_state_t *audio_st  = &audio_driver_st;
   if (!audio_st->sample_accum)
      return;
   if (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_SUSPENDED)
      return;
   audio_st->sample_accum[audio_st->data_ptr++] = left;
   audio_st->sample_accum[audio_st->data_ptr++] = right;

   /* Overflow guard only: the frame's audio is normally flushed as one
    * unit by audio_driver_frame_end() once retro_run() returns. */
   if (audio_st->data_ptr < AUDIO_SAMPLE_ACCUM_INT16S)
      return;

   audio_driver_sample_accum_flush(audio_st);
}

void audio_driver_frame_end(void)
{
   audio_driver_state_t *audio_st  = &audio_driver_st;

   audio_driver_publish_runloop();

   /* A core audio callback fills the accumulator on the audio thread and
    * audio_driver_callback() flushes it there; touching it here would
    * race that thread. The frame boundary itself is still this thread's
    * to measure - it is the only cadence fast-forward changes for a
    * callback core. */
   if (audio_st->callback.callback)
   {
      audio_driver_ff_callback_frame_end(audio_st);
      return;
   }

   /* A suspended frame (run-ahead, preemptive frames) produced nothing
    * and flushes nothing, so arming here would leave the flag to be
    * consumed by a mid-frame guard flush of the next audible frame and
    * that frame's own flush would then recompute a second time. */
   if (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_SUSPENDED)
      return;

   /* With the threaded pipeline the rate-control measurement happens on
    * the audio thread, gated by its own sample count; this flag is only
    * meaningful when the flush runs here. */
   if (     (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_CONTROL)
         && !(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_PIPELINE_THREADED))
      audio_st->drc_pending = true;

   if (audio_st->data_ptr)
      audio_driver_sample_accum_flush(audio_st);

   audio_driver_ff_frame_end(audio_st);
   audio_driver_pipeline_signal(audio_st);
}

size_t audio_driver_sample_batch(const int16_t *data, size_t frames)
{
   uint32_t runloop_flags;
   bool recording_push_audio      = false;
   bool flush_audio               = false;
   size_t frames_remaining        = frames;
   recording_state_t *record_st   = recording_state_get_ptr();
   audio_driver_state_t *audio_st = &audio_driver_st;
   float slowmotion_ratio         = audio_driver_snapshot_slowmotion(&audio_driver_st);

   if ((AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_SUSPENDED) || (frames < 1))
      return frames;

   runloop_flags                  = (uint32_t)retro_atomic_load_acquire_int(
         &audio_st->runloop_snapshot);
   flush_audio                    = !((runloop_flags & AUDIO_SNAP_PAUSED)
            ||  audio_driver_core_silenced()
            || !(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_ACTIVE)
            || !(audio_st->output_samples_buf));
   recording_push_audio           = record_st->data
           && record_st->driver
           && record_st->driver->push_audio;

   /* We want to run this loop at least once, so use a
    * do...while (do...while has only a single conditional
    * jump, as opposed to for and while which have a
    * conditional jump and an unconditional jump). Note,
    * however, that this is only relevant for compilers
    * that are poor at optimisation... */

   do
   {
      size_t frames_to_write =
            (frames_remaining > (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1))
                  ? (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1)
                  : frames_remaining;

      if (recording_push_audio)
         audio_driver_record_push(audio_st, data, frames_to_write, 2, AUDIO_LAYOUT_STEREO, false);

      if (flush_audio)
         audio_driver_submit(audio_st, slowmotion_ratio, data,
               frames_to_write << 1, false,
               (runloop_flags & AUDIO_SNAP_SLOWMOTION) ? true : false,
               (runloop_flags & AUDIO_SNAP_FASTMOTION) ? true : false,
               true);

      frames_remaining -= frames_to_write;
      data             += frames_to_write << 1;
   } while (frames_remaining > 0);

   return frames;
}

/* Multichannel batches use bounded native staging before the classic
 * stereo entry. Discrete layouts carry extras beside the front pair;
 * other layouts fold to stereo. */
static bool audio_driver_multi_fold_room(audio_driver_state_t *audio_st,
      size_t frames, size_t sample)
{
   if (frames > audio_st->multi_fold_frames)
   {
      /* float and int16 batches share the buffer: sized for float,
       * the larger */
      void *n = realloc(audio_st->multi_fold, frames * 2 * sizeof(float));
      if (!n)
         return false;
      audio_st->multi_fold        = n;
      audio_st->multi_fold_frames = frames;
   }
   (void)sample;
   return true;
}

static bool audio_driver_multi_layout_ok(unsigned channels, unsigned layout)
{
   return audio_layout_known(layout)
       && channels >= 1 && channels <= 8
       && audio_layout_channels(layout) == channels;
}

/* Whether a batch of this layout goes discretely: a device wider than
 * stereo, the front pair present (the pipeline is that pair), and no
 * threaded ring in between (it carries stereo). Otherwise the fold. */
static bool audio_driver_multi_discrete(audio_driver_state_t *audio_st, unsigned layout)
{
   /* A device wider than stereo takes the channels as they are; a
    * stereo device with headphone virtualisation on has a virtual 5.1
    * behind it, and the core's own channels belong in that rather
    * than the upmix's guess at one. Either way the front pair goes
    * through the pipeline and the rest beside it. */
   return ((audio_st->out_channels > 2)
          || (audio_st->virtualize && audio_st->virt_buf))
       && (layout & AUDIO_LAYOUT_STEREO) == AUDIO_LAYOUT_STEREO;
}

/* On the threaded pipeline the ring carries the canonical wide frame
 * for a core that negotiated before the driver came up: the batch is
 * built into it - a slot per speaker bit, FL and FR first, the rest
 * zero - and published, with its layout beside it for the consumer.
 * Returns true when the batch was taken; false to fold it instead. */
/* Common layouts share the first six canonical positions. */
static INLINE bool audio_driver_pipe_widen_prefix(void *output, const void *input,
      size_t frames, unsigned channels, unsigned layout, bool floating)
{
   size_t f;
   uint8_t *dst = (uint8_t*)output;
   const uint8_t *src = (const uint8_t*)input;
   if (!((channels == 6 && layout == AUDIO_LAYOUT_5POINT1)
            || (channels == 8 && layout == AUDIO_LAYOUT_7POINT1))) return false;
   if (floating)
   {
      if (channels == 6)
         for (f = 0; f < frames; f++)
         {
            memcpy(dst + f * AUDIO_PIPE_CANON_CHANNELS * sizeof(float), src + f * 6 * sizeof(float), 6 * sizeof(float));
            memset(dst + f * AUDIO_PIPE_CANON_CHANNELS * sizeof(float) + 6 * sizeof(float), 0, 5 * sizeof(float));
         }
      else
         for (f = 0; f < frames; f++)
         {
            memcpy(dst + f * AUDIO_PIPE_CANON_CHANNELS * sizeof(float), src + f * 8 * sizeof(float), 6 * sizeof(float));
            memset(dst + f * AUDIO_PIPE_CANON_CHANNELS * sizeof(float) + 6 * sizeof(float), 0, 3 * sizeof(float));
            memcpy(dst + (f * AUDIO_PIPE_CANON_CHANNELS + 9) * sizeof(float), src + (f * 8 + 6) * sizeof(float), 2 * sizeof(float));
         }
   }
   else
   {
      if (channels == 6)
         for (f = 0; f < frames; f++)
         {
            memcpy(dst + f * AUDIO_PIPE_CANON_CHANNELS * sizeof(int16_t), src + f * 6 * sizeof(int16_t), 6 * sizeof(int16_t));
            memset(dst + f * AUDIO_PIPE_CANON_CHANNELS * sizeof(int16_t) + 6 * sizeof(int16_t), 0, 5 * sizeof(int16_t));
         }
      else
         for (f = 0; f < frames; f++)
         {
            memcpy(dst + f * AUDIO_PIPE_CANON_CHANNELS * sizeof(int16_t), src + f * 8 * sizeof(int16_t), 6 * sizeof(int16_t));
            memset(dst + f * AUDIO_PIPE_CANON_CHANNELS * sizeof(int16_t) + 6 * sizeof(int16_t), 0, 3 * sizeof(int16_t));
            memcpy(dst + (f * AUDIO_PIPE_CANON_CHANNELS + 9) * sizeof(int16_t), src + (f * 8 + 6) * sizeof(int16_t), 2 * sizeof(int16_t));
         }
   }
   return true;
}

static bool audio_driver_multi_pipe(audio_driver_state_t *audio_st,
      const void *data, size_t frames, unsigned channels, unsigned layout, bool is_float)
{
   uint32_t runloop_flags;
   if (!audio_st->pipe_threaded || audio_st->pipe_channels != AUDIO_PIPE_CANON_CHANNELS)
      return false;
#ifdef HAVE_REWIND
   /* played in reverse, the frames go to the rewind buffer through
    * the classic entry's fold */
   if (state_manager_frame_is_reversed())
      return false;
#endif
   /* Discard before recording or publishing source samples. */
   if (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_SUSPENDED)
      return true;
   if (audio_st->float_gate && audio_st->float_gate())
      return true;
   runloop_flags = (uint32_t)retro_atomic_load_acquire_int(
         &audio_st->runloop_snapshot);
   audio_driver_record_push(audio_st, data, frames, channels, layout, is_float);
   if (      (runloop_flags & AUDIO_SNAP_PAUSED)
         ||  audio_driver_core_silenced()
         || !(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_ACTIVE)
         || !audio_st->output_samples_buf)
      return true;
   audio_st->pipe_layout = layout;
   audio_driver_submit_width(audio_st, audio_driver_snapshot_slowmotion(audio_st),
         data, frames * channels, is_float,
         (runloop_flags & AUDIO_SNAP_SLOWMOTION) ? true : false,
         (runloop_flags & AUDIO_SNAP_FASTMOTION) ? true : false, true,
         channels);
   return true;
}

/* The non-threaded pipeline: the front pair to the fold buffer, the
 * rest to the extras; the front pair then goes through the classic
 * entry with the extras pending beside it, following whichever
 * resampler path the flush takes. */
static bool audio_driver_multi_split_f32(audio_driver_state_t *audio_st,
      const float *data, size_t frames, unsigned channels, unsigned layout)
{
   unsigned ch = channels - 2;
   size_t f; unsigned c;
   bool int16_path = audio_driver_mixer_use_s16(true);
   float *fronts;
   if (!audio_driver_multi_fold_room(audio_st, frames, sizeof(float)))
      return false;
   if (!audio_driver_extra_prepare(audio_st, ch, layout & ~AUDIO_LAYOUT_STEREO, frames, true, int16_path))
      return false;
   fronts = (float*)audio_st->multi_fold;
   for (f = 0; f < frames; f++)
   {
      fronts[2 * f]     = data[f * channels];
      fronts[2 * f + 1] = data[f * channels + 1];
      for (c = 0; c < ch; c++)
         audio_st->extra.in_f[f * ch + c] = data[f * channels + 2 + c];
   }
   audio_st->extra.pending = true;
   return true;
}

static bool audio_driver_multi_split_s16(audio_driver_state_t *audio_st,
      const int16_t *data, size_t frames, unsigned channels, unsigned layout)
{
   unsigned ch = channels - 2;
   size_t f; unsigned c;
   bool int16_path = audio_driver_mixer_use_s16(false);
   int16_t *fronts;
   if (!audio_driver_multi_fold_room(audio_st, frames, sizeof(int16_t)))
      return false;
   if (!audio_driver_extra_prepare(audio_st, ch, layout & ~AUDIO_LAYOUT_STEREO, frames, false, int16_path))
      return false;
   fronts = (int16_t*)audio_st->multi_fold;
   for (f = 0; f < frames; f++)
   {
      fronts[2 * f]     = data[f * channels];
      fronts[2 * f + 1] = data[f * channels + 1];
      for (c = 0; c < ch; c++)
         audio_st->extra.in_i[f * ch + c] = data[f * channels + 2 + c];
   }
   audio_st->extra.pending = true;
   return true;
}

static bool audio_driver_inline_multi(audio_driver_state_t *audio_st,
      const void *data, size_t frames, unsigned channels, unsigned layout, bool floating)
{
   struct audio_inline_transport *t = audio_st->inline_transport;
   uint32_t flags;
   size_t done = 0, frame = channels * (floating ? sizeof(float) : sizeof(int16_t));
   float ratio = audio_driver_snapshot_slowmotion(audio_st);
   if (audio_st->pipe_threaded || !t || t->channels <= 2 || floating != t->floating
         || (layout & AUDIO_LAYOUT_STEREO) != AUDIO_LAYOUT_STEREO)
      return false;
#ifdef HAVE_REWIND
   if (state_manager_frame_is_reversed()) return false;
#endif
   if (audio_st->float_gate && audio_st->float_gate()) return true;
   audio_driver_record_push(audio_st, data, frames, channels, layout, floating);
   flags = (uint32_t)retro_atomic_load_acquire_int(&audio_st->runloop_snapshot);
   if ((flags & AUDIO_SNAP_PAUSED) || audio_driver_core_silenced()
         || !(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_ACTIVE)
         || !audio_st->output_samples_buf) return true;
   audio_driver_state_lock();
   audio_driver_arm_resume(audio_st);
   while (done < frames)
   {
      size_t n = frames - done;
      bool slow = (flags & AUDIO_SNAP_SLOWMOTION) != 0;
      bool fast = (flags & AUDIO_SNAP_FASTMOTION) != 0;
      if (n > (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1)) n = AUDIO_CHUNK_SIZE_NONBLOCKING >> 1;
      audio_driver_inline_pack(t, (const uint8_t*)data + done * frame, n, channels, layout);
      if (!audio_driver_inline_process(audio_st, ratio, t->source, n, floating, slow, fast, layout))
         audio_driver_inline_render(audio_st, t->source, n, layout, ratio, slow, fast);
      done += n;
   }
   audio_driver_state_unlock();
   return true;
}

#ifdef HAVE_REWIND
/* Cached multichannel callbacks cannot be swapped by the state manager. */
static size_t audio_driver_multi_rewind(audio_driver_state_t *audio_st,
      const void *data, size_t frames, unsigned channels, unsigned layout,
      bool floating)
{
   size_t done = 0;
   size_t sample = floating ? sizeof(float) : sizeof(int16_t);
   if ((AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_SUSPENDED)
         || (audio_st->float_gate && audio_st->float_gate()))
      return frames;
   if (layout == AUDIO_LAYOUT_STEREO)
      return floating ? audio_driver_sample_batch_float((const float*)data, frames)
         : audio_driver_sample_batch_rewind((const int16_t*)data, frames);
   while (done < frames && audio_st->rewind_ptr >= 2)
   {
      size_t n = frames - done;
      if (n > audio_st->rewind_ptr / 2) n = audio_st->rewind_ptr / 2;
      if (n > (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1)) n = AUDIO_CHUNK_SIZE_NONBLOCKING >> 1;
      if (!audio_driver_multi_fold_room(audio_st, n, sample))
         return done;
      if (floating)
      {
         audio_downmix_f32((float*)audio_st->multi_fold,
               (const float*)data + done * channels, n, layout, channels);
         audio_driver_sample_batch_float((const float*)audio_st->multi_fold, n);
      }
      else
      {
         audio_downmix_s16((int16_t*)audio_st->multi_fold,
               (const int16_t*)data + done * channels, n, layout, channels);
         audio_driver_sample_batch_rewind((const int16_t*)audio_st->multi_fold, n);
      }
      done += n;
   }
   return frames;
}
#endif

size_t audio_driver_sample_batch_multi_int16(const int16_t *data, size_t frames,
      unsigned channels, unsigned layout)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   if (!data || !audio_driver_multi_layout_ok(channels, layout))
      return 0;
   audio_st->core_layout = layout;
#ifdef HAVE_REWIND
   if (state_manager_frame_is_reversed())
      return audio_driver_multi_rewind(audio_st, data, frames, channels, layout, false);
#endif
   if (layout == AUDIO_LAYOUT_STEREO)
      return audio_driver_sample_batch(data, frames);
   if (!frames)
      return 0;
   if (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_SUSPENDED)
      return frames;
   if (audio_st->inline_transport
         && audio_driver_inline_multi(audio_st, data, frames, channels, layout, false))
      return frames;
   if (audio_driver_multi_discrete(audio_st, layout))
   {
      if (!audio_st->pipe_threaded)
      {
         size_t done = 0;
         while (done < frames)
         {
            size_t n = frames - done, taken;
            if (n > (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1))
               n = AUDIO_CHUNK_SIZE_NONBLOCKING >> 1;
            if (!audio_driver_multi_fold_room(audio_st, n, sizeof(int16_t)))
               return done;
            /* Extras are consumed by one stereo flush, not a whole batch. */
            if (!audio_driver_multi_split_s16(audio_st, data + done * channels,
                     n, channels, layout))
               audio_downmix_s16((int16_t*)audio_st->multi_fold,
                     data + done * channels, n, layout, channels);
            taken = audio_driver_sample_batch((const int16_t*)audio_st->multi_fold, n);
            audio_st->extra.pending = false;
            done += taken;
            if (taken < n) break;
         }
         return done;
      }
      if (audio_driver_multi_pipe(audio_st, data, frames, channels, layout, false))
         return frames;
   }
   {
      size_t done = 0;
      while (done < frames)
      {
         size_t n = frames - done, taken;
         if (n > (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1))
            n = AUDIO_CHUNK_SIZE_NONBLOCKING >> 1;
         if (!audio_driver_multi_fold_room(audio_st, n, sizeof(int16_t)))
            return done;
         audio_downmix_s16((int16_t*)audio_st->multi_fold,
               data + done * channels, n, layout, channels);
         taken = audio_driver_sample_batch((const int16_t*)audio_st->multi_fold, n);
         done += taken;
         if (taken < n) break;
      }
      return done;
   }
}

size_t audio_driver_sample_batch_multi_float(const float *data, size_t frames,
      unsigned channels, unsigned layout)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   if (!data || !audio_driver_multi_layout_ok(channels, layout))
      return 0;
   audio_st->core_layout = layout;
#ifdef HAVE_REWIND
   if (state_manager_frame_is_reversed())
      return audio_driver_multi_rewind(audio_st, data, frames, channels, layout, true);
#endif
   if (layout == AUDIO_LAYOUT_STEREO)
      return audio_driver_sample_batch_float(data, frames);
   if (!frames)
      return 0;
   if (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_SUSPENDED)
      return frames;
   if (audio_st->inline_transport
         && audio_driver_inline_multi(audio_st, data, frames, channels, layout, true))
      return frames;
   if (audio_driver_multi_discrete(audio_st, layout))
   {
      if (!audio_st->pipe_threaded)
      {
         size_t done = 0;
         while (done < frames)
         {
            size_t n = frames - done, taken;
            if (n > (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1))
               n = AUDIO_CHUNK_SIZE_NONBLOCKING >> 1;
            if (!audio_driver_multi_fold_room(audio_st, n, sizeof(float)))
               return done;
            /* Extras are consumed by one stereo flush, not a whole batch. */
            if (!audio_driver_multi_split_f32(audio_st, data + done * channels,
                     n, channels, layout))
               audio_downmix_f32((float*)audio_st->multi_fold,
                     data + done * channels, n, layout, channels);
            taken = audio_driver_sample_batch_float((const float*)audio_st->multi_fold, n);
            audio_st->extra.pending = false;
            done += taken;
            if (taken < n) break;
         }
         return done;
      }
      if (audio_driver_multi_pipe(audio_st, data, frames, channels, layout, true))
         return frames;
   }
   {
      size_t done = 0;
      while (done < frames)
      {
         size_t n = frames - done, taken;
         if (n > (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1))
            n = AUDIO_CHUNK_SIZE_NONBLOCKING >> 1;
         if (!audio_driver_multi_fold_room(audio_st, n, sizeof(float)))
            return done;
         audio_downmix_f32((float*)audio_st->multi_fold,
               data + done * channels, n, layout, channels);
         taken = audio_driver_sample_batch_float((const float*)audio_st->multi_fold, n);
         done += taken;
         if (taken < n) break;
      }
      return done;
   }
}

/* Float counterpart of audio_driver_sample_batch(). Used only when the
 * core negotiated float output via
 * RETRO_ENVIRONMENT_GET_AUDIO_SAMPLE_BATCH_FLOAT. 'data' is interleaved
 * stereo float in [-1.0, 1.0]; 'frames' is the frame count (2 floats per
 * frame). Funnels into the same audio_driver_flush() as the int16 path,
 * but with is_float=true so the redundant int16->float conversion is
 * skipped. Recording and reverse-audio (rewind), which are int16
 * subsystems, are bridged here by converting only the affected chunk. */
size_t audio_driver_sample_batch_float(const float *data, size_t frames)
{
   uint32_t runloop_flags;
   bool recording_push_audio      = false;
   bool flush_audio               = false;
   size_t frames_remaining        = frames;
   recording_state_t *record_st   = recording_state_get_ptr();
   audio_driver_state_t *audio_st = &audio_driver_st;
   float slowmotion_ratio         = audio_driver_snapshot_slowmotion(&audio_driver_st);

   if ((AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_SUSPENDED) || (frames < 1))
      return frames;
   /* Netplay's interception, for the entry it cannot swap. */
   if (audio_st->float_gate && audio_st->float_gate())
      return frames;

#ifdef HAVE_REWIND
   /* While frames are being played in reverse, the int16 path swaps the
    * core callback to audio_driver_sample_batch_rewind(). A float core
    * keeps the cached float pointer, so route it to the reverse buffer. */
   if (state_manager_frame_is_reversed())
   {
      size_t i;
      /* A float core's reverse buffer is float: no conversion in, none
       * out when it plays back. */
      if (audio_st->core_float && audio_st->rewind_buf_f)
      {
         for (i = 0; i < frames && audio_st->rewind_ptr >= 2; i++)
         {
            audio_st->rewind_buf_f[--audio_st->rewind_ptr] = data[2*i+1];
            audio_st->rewind_buf_f[--audio_st->rewind_ptr] = data[2*i];
         }
         return frames;
      }
      for (i = 0; i < frames && audio_st->rewind_ptr >= 2; i++)
      {
         /* Inline saturating float->s16 to avoid an extra scratch copy.
          * Must round the same way convert_float_to_s16() does - half
          * away from zero - not truncate: the recording bridge lower in
          * this same function calls that converter directly, and a
          * truncating variant here would quantise the rewind audio with
          * twice the error and a one-LSB dead band around silence. */
         audio_st->rewind_buf[--audio_st->rewind_ptr] =
               (int16_t)audio_float_to_s16_sat(data[2*i+1]);
         audio_st->rewind_buf[--audio_st->rewind_ptr] =
               (int16_t)audio_float_to_s16_sat(data[2*i]);
      }
      return frames;
   }
#endif

   runloop_flags                  = (uint32_t)retro_atomic_load_acquire_int(
         &audio_st->runloop_snapshot);
   flush_audio                    = !((runloop_flags & AUDIO_SNAP_PAUSED)
            ||  audio_driver_core_silenced()
            || !(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_ACTIVE)
            || !(audio_st->output_samples_buf));
   recording_push_audio           = record_st->data
           && record_st->driver
           && record_st->driver->push_audio;

   do
   {
      size_t frames_to_write =
            (frames_remaining > (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1))
                  ? (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1)
                  : frames_remaining;

      /* The recorder consumes int16. Convert just this chunk into the
       * int16 staging buffer (sized for >= a full chunk) and hand that
       * over; push_audio copies synchronously. */
#ifdef HAVE_THREADS
      if (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_PIPELINE_THREADED)
      {
         /* The recorder consumes int16: converted into its own staging
          * only while it records. The ring takes the float frames as
          * they are on a float ring, and converts them on an int16 one. */
         if (recording_push_audio)
            audio_driver_record_push(audio_st, data, frames_to_write, 2, AUDIO_LAYOUT_STEREO, true);
         if (flush_audio)
            audio_driver_submit(audio_st, slowmotion_ratio,
                  data, frames_to_write << 1, true,
                  (runloop_flags & AUDIO_SNAP_SLOWMOTION) ? true : false,
                  (runloop_flags & AUDIO_SNAP_FASTMOTION) ? true : false,
                  true);
         frames_remaining -= frames_to_write;
         data             += frames_to_write << 1;
         continue;
      }
#endif
      if (recording_push_audio)
         audio_driver_record_push(audio_st, data, frames_to_write, 2, AUDIO_LAYOUT_STEREO, true);

      if (flush_audio)
      {
         audio_driver_state_lock();
         audio_driver_arm_resume(audio_st);
         if (!audio_st->inline_transport
               || !audio_driver_inline_flush(audio_st, slowmotion_ratio, data,
                  frames_to_write << 1, true,
                  (runloop_flags & AUDIO_SNAP_SLOWMOTION) ? true : false,
                  (runloop_flags & AUDIO_SNAP_FASTMOTION) ? true : false))
            audio_driver_flush(audio_st, slowmotion_ratio, data,
               frames_to_write << 1, true,
               (runloop_flags & AUDIO_SNAP_SLOWMOTION) ? true : false,
               (runloop_flags & AUDIO_SNAP_FASTMOTION) ? true : false);
         audio_driver_state_unlock();
      }

      frames_remaining -= frames_to_write;
      data             += frames_to_write << 1;
   } while (frames_remaining > 0);

   return frames;
}

#ifdef HAVE_REWIND
void audio_driver_sample_rewind(int16_t left, int16_t right)
{
   audio_driver_state_t *audio_st  = &audio_driver_st;
   if (audio_st->rewind_ptr < 2
         || (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_SUSPENDED))
      return;

   if (audio_st->core_float && audio_st->rewind_buf_f)
   {
      audio_st->rewind_buf_f[--audio_st->rewind_ptr] = right * (1.0f / 32768.0f);
      audio_st->rewind_buf_f[--audio_st->rewind_ptr] = left  * (1.0f / 32768.0f);
      return;
   }
   audio_st->rewind_buf[--audio_st->rewind_ptr] = right;
   audio_st->rewind_buf[--audio_st->rewind_ptr] = left;
}

size_t audio_driver_sample_batch_rewind(
      const int16_t *data, size_t frames)
{
   size_t i;
   audio_driver_state_t *audio_st  = &audio_driver_st;

   if (AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_SUSPENDED)
      return frames;

   /* Match the arena selected by reverse playback, even for mixed callbacks. */
   if (audio_st->core_float && audio_st->rewind_buf_f)
   {
      for (i = 0; i < frames && audio_st->rewind_ptr >= 2; i++)
      {
         audio_st->rewind_buf_f[--audio_st->rewind_ptr] = data[2*i+1] * (1.0f / 32768.0f);
         audio_st->rewind_buf_f[--audio_st->rewind_ptr] = data[2*i]   * (1.0f / 32768.0f);
      }
      return frames;
   }
   for (i = 0; i < frames && audio_st->rewind_ptr >= 2; i++)
   {
      audio_st->rewind_buf[--audio_st->rewind_ptr] = data[2*i+1];
      audio_st->rewind_buf[--audio_st->rewind_ptr] = data[2*i];
   }

   return frames;
}
#endif

#ifdef HAVE_DSP_FILTER
void audio_driver_dsp_filter_free(void)
{
   audio_driver_state_t *audio_st  = &audio_driver_st;
   audio_driver_state_lock();
   if (audio_st->dsp)
      retro_dsp_filter_free(audio_st->dsp);
   audio_st->dsp = NULL;
   audio_driver_state_unlock();
}

bool audio_driver_dsp_filter_init(const char *device)
{
   retro_dsp_filter_t *audio_driver_dsp = NULL;
   struct string_list *plugs            = NULL;
#if defined(HAVE_DYLIB) && !defined(HAVE_FILTERS_BUILTIN)
   char ext_name[16];
   char basedir[NAME_MAX_LENGTH];
   fill_pathname_basedir(basedir, device, sizeof(basedir));
   if (!frontend_driver_get_core_extension(ext_name, sizeof(ext_name)))
      return false;
   if (!(plugs = dir_list_new(basedir, ext_name, false, true, false, false)))
      return false;
#endif
   audio_driver_dsp = retro_dsp_filter_new(
         device, plugs, audio_driver_st.input);
   if (!audio_driver_dsp)
      return false;

   audio_driver_state_lock();
   audio_driver_st.dsp = audio_driver_dsp;
   audio_driver_state_unlock();

   return true;
}
#endif

unsigned audio_driver_device_block_frames(void)
{
   return g_defaults.settings_out_block_frames > 0
      ? (unsigned)g_defaults.settings_out_block_frames : 0;
}

void audio_driver_set_buffer_size(size_t bufsize)
{
   /* Ignore zero. buffer_size is a divisor for every consumer that reads
    * it - the DRC setpoint in audio_driver_compute_rate_adjust() and the
    * saturation / water-mark statistics in
    * audio_compute_buffer_statistics() - and the init-time gate on
    * AUDIO_FLAG_CONTROL only covers the value the driver reported when it
    * was opened. This setter is the one path that can replace it later:
    * pulse calls it from inside write_avail() on every sample and notes
    * there that the size "can change spuriously". Keeping the last known
    * good size degrades to a slightly stale setpoint; adopting a zero
    * feeds inf/NaN into the statistics overlay and forces
    * close_to_underrun to 100%.
    *
    * Guarding here rather than at each consumer means anything added
    * later inherits the invariant instead of having to rediscover it. */
   if (bufsize > 0)
      audio_driver_st.buffer_size = bufsize;
}

#ifdef HAVE_REWIND
void audio_driver_setup_rewind(void)
{
   audio_driver_state_t *audio_st  = &audio_driver_st;

   /* Every retro_run() is followed by audio_driver_frame_end(), so the
    * single-sample accumulator is empty at every frame boundary, and
    * this runs at one. Anything still pending means a frame ran without
    * that call; play it forward rather than lose it. */
   retro_assert(audio_st->data_ptr == 0);
   if (audio_st->data_ptr)
      audio_driver_sample_accum_flush(audio_st);

   audio_st->rewind_ptr = audio_st->rewind_size;
}
#endif

bool audio_driver_get_devices_list(void **data)
{
   struct string_list**ptr     = (struct string_list**)data;
   if (!ptr)
      return false;
   *ptr = audio_driver_st.devices_list;
   return true;
}

#ifdef HAVE_AUDIOMIXER
bool audio_driver_mixer_extension_supported(const char *ext)
{
#ifdef HAVE_RVORBIS
   if (string_is_equal_noncase("ogg", ext))
      return true;
#endif
#ifdef HAVE_RMODTRACKER
   if (string_is_equal_noncase("mod", ext))
      return true;
   if (string_is_equal_noncase("s3m", ext))
      return true;
   if (string_is_equal_noncase("xm", ext))
      return true;
#endif
#ifdef HAVE_RFLAC
   if (string_is_equal_noncase("flac", ext))
      return true;
#endif
#ifdef HAVE_RMP3
   if (string_is_equal_noncase("mp3", ext))
      return true;
#endif
#ifdef HAVE_RAAC
#ifdef HAVE_RMP4
   if (string_is_equal_noncase("m4a", ext))
      return true;
#endif
   if (string_is_equal_noncase("aac", ext))
      return true;
#endif
#ifdef HAVE_ROPUS
   if (string_is_equal_noncase("opus", ext))
      return true;
#endif
#if defined(HAVE_RWEBM) && (defined(HAVE_ROPUS) || defined(HAVE_RVORBIS))
   if (string_is_equal_noncase("weba", ext))
      return true;
#endif
   if (string_is_equal_noncase("wav", ext))
      return true;
   return false;
}

static int audio_mixer_find_index(
      audio_mixer_sound_t *sound)
{
   unsigned i;

   for (i = 0; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
   {
      audio_mixer_sound_t *handle = audio_driver_st.mixer_streams[i].handle;
      if (handle == sound)
         return i;
   }
   return -1;
}

static void audio_mixer_play_stop_cb(
      audio_mixer_sound_t *sound, unsigned reason)
{
   int idx = audio_mixer_find_index(sound);

   switch (reason)
   {
      case AUDIO_MIXER_SOUND_FINISHED:
         audio_mixer_destroy(sound);

         if (idx >= 0)
         {
            unsigned i = (unsigned)idx;

            if (*audio_driver_st.mixer_streams[i].name)
               free(audio_driver_st.mixer_streams[i].name);

            audio_driver_st.mixer_streams[i].name    = NULL;
            audio_driver_st.mixer_streams[i].state   = AUDIO_STREAM_STATE_NONE;
            audio_driver_st.mixer_streams[i].volume  = 0.0f;
            audio_driver_st.mixer_streams[i].buf     = NULL;
            audio_driver_st.mixer_streams[i].stop_cb = NULL;
            audio_driver_st.mixer_streams[i].handle  = NULL;
            audio_driver_st.mixer_streams[i].voice   = NULL;
            if (audio_driver_st.mixer_streams_playing > 0)
               audio_driver_st.mixer_streams_playing--;
         }
         break;
      case AUDIO_MIXER_SOUND_STOPPED:
         break;
      case AUDIO_MIXER_SOUND_REPEATED:
         break;
   }
}

static void audio_mixer_menu_stop_cb(
      audio_mixer_sound_t *sound, unsigned reason)
{
   int idx = audio_mixer_find_index(sound);

   switch (reason)
   {
      case AUDIO_MIXER_SOUND_FINISHED:
         if (idx >= 0)
         {
            unsigned i                              = (unsigned)idx;
            audio_driver_st.mixer_streams[i].state   = AUDIO_STREAM_STATE_STOPPED;
            audio_driver_st.mixer_streams[i].volume  = 0.0f;
            if (audio_driver_st.mixer_streams_playing > 0)
               audio_driver_st.mixer_streams_playing--;
         }
         break;
      case AUDIO_MIXER_SOUND_STOPPED:
      case AUDIO_MIXER_SOUND_REPEATED:
         break;
   }
}

static void audio_mixer_play_stop_sequential_cb(
      audio_mixer_sound_t *sound, unsigned reason)
{
   int idx = audio_mixer_find_index(sound);

   switch (reason)
   {
      case AUDIO_MIXER_SOUND_FINISHED:
         audio_mixer_destroy(sound);

         if (idx >= 0)
         {
            unsigned i = (unsigned)idx;

            if (*audio_driver_st.mixer_streams[i].name)
               free(audio_driver_st.mixer_streams[i].name);

            if (i < AUDIO_MIXER_MAX_STREAMS)
               audio_driver_st.mixer_streams[i].stream_type = AUDIO_STREAM_TYPE_USER;
            else
               audio_driver_st.mixer_streams[i].stream_type = AUDIO_STREAM_TYPE_SYSTEM;

            audio_driver_st.mixer_streams[i].name           = NULL;
            audio_driver_st.mixer_streams[i].state          = AUDIO_STREAM_STATE_NONE;
            audio_driver_st.mixer_streams[i].volume         = 0.0f;
            audio_driver_st.mixer_streams[i].buf            = NULL;
            audio_driver_st.mixer_streams[i].stop_cb        = NULL;
            audio_driver_st.mixer_streams[i].handle         = NULL;
            audio_driver_st.mixer_streams[i].voice          = NULL;
            if (audio_driver_st.mixer_streams_playing > 0)
               audio_driver_st.mixer_streams_playing--;

            i++;

            for (; i < AUDIO_MIXER_MAX_SYSTEM_STREAMS; i++)
            {
               if (audio_driver_st.mixer_streams[i].state
                     == AUDIO_STREAM_STATE_STOPPED)
               {
                  audio_driver_mixer_play_stream_sequential(i);
                  break;
               }
            }
         }
         break;
      case AUDIO_MIXER_SOUND_STOPPED:
      case AUDIO_MIXER_SOUND_REPEATED:
         break;
   }
}

/* Defined below, next to their locking public entry points. */
static void audio_driver_mixer_stop_stream_locked(unsigned i);
static void audio_driver_mixer_remove_stream_locked(unsigned i);

static bool audio_driver_mixer_get_free_stream_slot(
      unsigned *id, enum audio_mixer_stream_type type)
{
   unsigned     i = AUDIO_MIXER_MAX_STREAMS;
   unsigned count = AUDIO_MIXER_MAX_SYSTEM_STREAMS;

   if (type == AUDIO_STREAM_TYPE_USER)
   {
      i           = 0;
      count       = AUDIO_MIXER_MAX_STREAMS;
   }

   for (; i < count; i++)
   {
      if (audio_driver_st.mixer_streams[i].state == AUDIO_STREAM_STATE_NONE)
      {
         *id = i;
         return true;
      }
   }

   return false;
}

bool audio_driver_mixer_add_stream(audio_mixer_stream_params_t *params)
{
   unsigned free_slot            = 0;
   audio_mixer_voice_t *voice    = NULL;
   audio_mixer_sound_t *handle   = NULL;
   audio_mixer_stop_cb_t stop_cb = audio_mixer_play_stop_cb;

   bool looped                   = (params->state == AUDIO_STREAM_STATE_PLAYING_LOOPED);
   void *buf                     = NULL;

   audio_driver_state_lock();
   /* Ownership of params->buf_owner transfers on this call in every
    * outcome: each failure return releases it. */
   if (params->out_slot)
      *params->out_slot = -1;
   /* Outside the mixer's init..done window - a mixer load task
    * retiring after audio teardown at shutdown, or any stream pushed
    * during a session whose audio driver failed to initialize -
    * claiming a voice would write into a subsystem whose voice locks
    * audio_mixer_done() already freed. Refuse; ownership is released
    * as on every other failure path. */
   if (!(AUDIO_FLAGS_GET(&audio_driver_st) & AUDIO_FLAG_MIXER_INITED))
   {
      if (params->buf_owner)
         params->buf_owner_free(params->buf_owner);
      audio_driver_state_unlock();
      return false;
   }
   if (params->stream_type == AUDIO_STREAM_TYPE_NONE)
   {
      if (params->buf_owner)
         params->buf_owner_free(params->buf_owner);
      audio_driver_state_unlock();
      return false;
   }

   switch (params->slot_selection_type)
   {
      case AUDIO_MIXER_SLOT_SELECTION_MANUAL:
         free_slot = params->slot_selection_idx;

         /* The unlocked internals below index the array directly, so
          * the range the public entry points check is checked here. */
         if (free_slot >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
         {
            if (params->buf_owner)
               params->buf_owner_free(params->buf_owner);
            audio_driver_state_unlock();
            return false;
         }

         /* If we are using a manually specified
          * slot, must free any existing stream
          * before assigning the new one. The lock is already
          * held here, so the unlocked internals are what run. */
         audio_driver_mixer_stop_stream_locked(free_slot);
         audio_driver_mixer_remove_stream_locked(free_slot);
         break;
      case AUDIO_MIXER_SLOT_SELECTION_AUTOMATIC:
      default:
         if (!audio_driver_mixer_get_free_stream_slot(
                  &free_slot, params->stream_type))
         {
            if (params->buf_owner)
               params->buf_owner_free(params->buf_owner);
            audio_driver_state_unlock();
            return false;
         }
         break;
   }

   if (params->state == AUDIO_STREAM_STATE_NONE)
   {
      if (params->buf_owner)
         params->buf_owner_free(params->buf_owner);
      audio_driver_state_unlock();
      return false;
   }

   if (params->buf_owner)
      /* borrowed: the sound reads straight out of the owner's bytes,
       * and destroy hands them back - no copy is made */
      buf = params->buf;
   else
   {
      if (!(buf = malloc(params->bufsize)))
      {
         audio_driver_state_unlock();
         return false;
      }
      memcpy(buf, params->buf, params->bufsize);
   }

   switch (params->type)
   {
      case AUDIO_MIXER_TYPE_WAV:
         handle = audio_mixer_load_wav(buf, params->bufsize,
               audio_driver_st.resampler_ident,
               audio_driver_st.resampler_quality,
               audio_driver_mixer_use_s16(
                     audio_driver_st.stat_core_is_float));
         /* WAV is a special case - input buffer is not
          * free()'d when sound playback is complete (it is
          * converted to a PCM buffer, which is free()'d instead),
          * so release the source here */
         if (params->buf_owner)
            params->buf_owner_free(params->buf_owner);
         else
            free(buf);
         buf = NULL;
         break;
      case AUDIO_MIXER_TYPE_WAV_STREAM:
         /* unlike AUDIO_MIXER_TYPE_WAV above, nothing is decoded at
          * load: the sound keeps these bytes and reads frames from
          * them as it mixes, so the source stays owned by the sound
          * (or by its lender) exactly as the compressed types' does */
         handle = audio_mixer_load_wav_stream(buf,
               params->bufsize);
         break;
      case AUDIO_MIXER_TYPE_OGG:
         handle = audio_mixer_load_ogg(buf, params->bufsize);
         break;
      case AUDIO_MIXER_TYPE_MOD:
         handle = audio_mixer_load_mod(buf, params->bufsize);
         break;
      case AUDIO_MIXER_TYPE_FLAC:
#ifdef HAVE_RFLAC
         handle = audio_mixer_load_flac(buf, params->bufsize);
#endif
         break;
      case AUDIO_MIXER_TYPE_MP3:
#ifdef HAVE_RMP3
         handle = audio_mixer_load_mp3(buf, params->bufsize);
#endif
         break;
      case AUDIO_MIXER_TYPE_M4A:
#ifdef HAVE_RAAC
         handle = audio_mixer_load_m4a(buf, params->bufsize);
#endif
         break;
      case AUDIO_MIXER_TYPE_AC3:
#ifdef HAVE_RAC3
         handle = audio_mixer_load_ac3(buf, params->bufsize);
#endif
         break;
      case AUDIO_MIXER_TYPE_LPCM:
#ifdef HAVE_RLPCM
         handle = audio_mixer_load_lpcm(buf, params->bufsize);
#endif
         break;
      case AUDIO_MIXER_TYPE_OPUS:
#ifdef HAVE_ROPUS
         handle = audio_mixer_load_opus(buf, params->bufsize);
#endif
         break;
      case AUDIO_MIXER_TYPE_WEBA:
#if defined(HAVE_RWEBM) && (defined(HAVE_ROPUS) || defined(HAVE_RVORBIS))
         handle = audio_mixer_load_weba_avail(buf, params->bufsize,
               params->avail);
#endif
         break;
      case AUDIO_MIXER_TYPE_NONE:
         break;
   }

   if (!handle)
   {
      if (params->buf_owner)
      {
         /* WAV already released above; buf is NULL there */
         if (buf)
            params->buf_owner_free(params->buf_owner);
      }
      else
         free(buf);
      audio_driver_state_unlock();
      return false;
   }

#ifdef HAVE_ROPUS
   /* Windowed Ogg-Opus supplies the last-page granule so the decoder
    * skips its full-file end scan when it opens at play time. */
   if (params->end_granule > 0)
      audio_mixer_sound_set_end_granule(handle, params->end_granule);
#endif
   /* NOT under HAVE_ROPUS: the resident bound applies to every
    * windowed arm, and a build without Opus silently dropped it -
    * the decoder then opened against the whole file's length with
    * only the head committed. */
   if (params->avail)
      audio_mixer_sound_set_avail(handle, params->avail);

   switch (params->state)
   {
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
         stop_cb = audio_mixer_play_stop_sequential_cb;
         /* fall-through */
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING:
         if (audio_driver_mixer_use_s16(audio_driver_st.stat_core_is_float))
            voice = audio_mixer_play_s16(handle, looped,
                  GAIN_TO_Q16(params->volume),
                  audio_driver_st.resampler_ident,
                  audio_driver_st.resampler_quality, stop_cb);
         else
            voice = audio_mixer_play(handle, looped, params->volume,
                  audio_driver_st.resampler_ident,
                  audio_driver_st.resampler_quality, stop_cb);
         break;
      default:
         break;
   }

   if (params->buf_owner && buf)
      /* the sound holds the borrowed bytes for its lifetime; destroy
       * hands the owner back */
      audio_mixer_sound_set_data_owner(handle,
            params->buf_owner, params->buf_owner_free);

   AUDIO_FLAGS_SET(&audio_driver_st, AUDIO_FLAG_MIXER_ACTIVE);

   audio_driver_st.mixer_streams[free_slot].name        =
      (params->basename && *params->basename) ? strdup(params->basename) : NULL;
   audio_driver_st.mixer_streams[free_slot].buf         = buf;
   audio_driver_st.mixer_streams[free_slot].handle      = handle;
   audio_driver_st.mixer_streams[free_slot].voice       = voice;
   audio_driver_st.mixer_streams[free_slot].stream_type = params->stream_type;
   audio_driver_st.mixer_streams[free_slot].type        = params->type;
   audio_driver_st.mixer_streams[free_slot].state       = params->state;
   audio_driver_st.mixer_streams[free_slot].volume      = params->volume;
   audio_driver_st.mixer_streams[free_slot].stop_cb     = stop_cb;

   if (   params->state == AUDIO_STREAM_STATE_PLAYING
       || params->state == AUDIO_STREAM_STATE_PLAYING_LOOPED
       || params->state == AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL)
      audio_driver_st.mixer_streams_playing++;

   if (params->out_slot)
      *params->out_slot = (int)free_slot;

   audio_driver_state_unlock();

   return true;
}

int64_t audio_driver_mixer_stream_byte_tell(unsigned i)
{
   audio_mixer_voice_t *voice;
   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return -1;
   if (audio_driver_st.mixer_streams[i].state == AUDIO_STREAM_STATE_NONE)
      return -1;
   if (!(voice = audio_driver_st.mixer_streams[i].voice))
      return -1;
   return (int64_t)audio_mixer_voice_buffer_tell(voice);
}

void audio_driver_mixer_stream_set_avail(unsigned i, size_t avail)
{
   audio_mixer_voice_t *voice;
   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;
   if (audio_driver_st.mixer_streams[i].state == AUDIO_STREAM_STATE_NONE)
      return;
   if (!(voice = audio_driver_st.mixer_streams[i].voice))
      return;
   audio_mixer_voice_set_avail(voice, avail);
}

enum audio_mixer_state audio_driver_mixer_get_stream_state(unsigned i)
{
   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return AUDIO_STREAM_STATE_NONE;

   return audio_driver_st.mixer_streams[i].state;
}

static void audio_driver_mixer_play_stream_internal(
      unsigned i, unsigned type)
{
   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;

   audio_driver_state_lock();

   switch (audio_driver_st.mixer_streams[i].state)
   {
      case AUDIO_STREAM_STATE_STOPPED:
         if (audio_driver_mixer_use_s16(audio_driver_st.stat_core_is_float))
            audio_driver_st.mixer_streams[i].voice =
               audio_mixer_play_s16(audio_driver_st.mixer_streams[i].handle,
                  (type == AUDIO_STREAM_STATE_PLAYING_LOOPED) ? true : false,
                  AUDIO_MIXER_GAIN_UNITY,
                  audio_driver_st.resampler_ident,
                  audio_driver_st.resampler_quality,
                  audio_driver_st.mixer_streams[i].stop_cb);
         else
            audio_driver_st.mixer_streams[i].voice =
               audio_mixer_play(audio_driver_st.mixer_streams[i].handle,
                  (type == AUDIO_STREAM_STATE_PLAYING_LOOPED) ? true : false,
                  1.0f, audio_driver_st.resampler_ident,
                  audio_driver_st.resampler_quality,
                  audio_driver_st.mixer_streams[i].stop_cb);
         audio_driver_st.mixer_streams[i].state = (enum audio_mixer_state)type;
         audio_driver_st.mixer_streams_playing++;
         break;
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
      case AUDIO_STREAM_STATE_NONE:
         break;
   }
   audio_driver_state_unlock();
}

#if defined(HAVE_MENU)
static void audio_driver_load_menu_bgm_callback(retro_task_t *task,
      void *task_data, void *user_data, const char *error)
{
   if (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE)
      audio_driver_mixer_play_menu_sound_looped(AUDIO_MIXER_SYSTEM_SLOT_BGM);
}
#endif

void audio_driver_load_system_sounds(void)
{
   char basename_noext[NAME_MAX_LENGTH];
   char sounds_path[PATH_MAX_LENGTH];
   char sounds_fallback_path[PATH_MAX_LENGTH];
   settings_t *settings                  = config_get_ptr();
   const char *dir_assets                = settings->paths.directory_assets;
   const bool audio_enable_menu          = settings->bools.audio_enable_menu;
   const bool audio_enable_menu_ok       = audio_enable_menu && settings->bools.audio_enable_menu_ok;
   const bool audio_enable_menu_cancel   = audio_enable_menu && settings->bools.audio_enable_menu_cancel;
   const bool audio_enable_menu_notice   = audio_enable_menu && settings->bools.audio_enable_menu_notice;
   const bool audio_enable_menu_bgm      = audio_enable_menu && settings->bools.audio_enable_menu_bgm;
   const bool audio_enable_menu_scroll   = audio_enable_menu && settings->bools.audio_enable_menu_scroll;
   const bool audio_enable_cheevo_unlock = settings->bools.cheevos_unlock_sound_enable;
   const char *path_ok                   = NULL;
   const char *path_cancel               = NULL;
   const char *path_notice               = NULL;
   const char *path_notice_back          = NULL;
#if defined(HAVE_MENU)
   const char *path_bgm                  = NULL;
#endif
   const char *path_cheevo_unlock        = NULL;
   const char *path_up                   = NULL;
   const char *path_down                 = NULL;
   struct string_list *list              = NULL;
   struct string_list *list_fallback     = NULL;
   unsigned i                            = 0;

   if (!audio_enable_menu && !audio_enable_cheevo_unlock)
      goto end;

   sounds_path[0] = basename_noext[0] ='\0';

   fill_pathname_join_special(
         sounds_fallback_path,
         dir_assets,
         "sounds",
         sizeof(sounds_fallback_path));

   fill_pathname_application_special(
         sounds_path,
         sizeof(sounds_path),
         APPLICATION_SPECIAL_DIRECTORY_ASSETS_SOUNDS);

   list          = dir_list_new(sounds_path, MENU_SOUND_FORMATS, false, false, false, false);
   list_fallback = dir_list_new(sounds_fallback_path, MENU_SOUND_FORMATS, false, false, false, false);

   /* If primary list is NULL or empty, try to use fallback */
   if ((!list || list->size == 0) && list_fallback && list_fallback->size > 0)
   {
      if (list)
         string_list_free(list);
      list          = list_fallback;
      list_fallback = NULL;
   }

   if (!list || list->size == 0)
      goto end;

   if (list_fallback && list_fallback->size > 0)
   {
      for (i = 0; i < list_fallback->size; i++)
      {
         if (list->size == 0 || !string_list_find_elem(list, list_fallback->elems[i].data))
         {
            union string_list_elem_attr attr = {0};
            string_list_append(list, list_fallback->elems[i].data, attr);
         }
      }
   }

   for (i = 0; i < list->size; i++)
   {
      const char *path = list->elems[i].data;
      const char *ext  = path_get_extension(path);

      if (audio_driver_mixer_extension_supported(ext))
      {
         basename_noext[0] = '\0';
         fill_pathname(basename_noext, path_basename(path), "",
               sizeof(basename_noext));

         if (string_is_equal_noncase(basename_noext, "ok"))
            path_ok = path;
         else if (string_is_equal_noncase(basename_noext, "cancel"))
            path_cancel = path;
         else if (string_is_equal_noncase(basename_noext, "notice"))
            path_notice = path;
         else if (string_is_equal_noncase(basename_noext, "notice_back"))
            path_notice_back = path;
#if defined(HAVE_MENU)
         else if (string_is_equal_noncase(basename_noext, "bgm"))
            path_bgm = path;
#endif
         else if (string_is_equal_noncase(basename_noext, "unlock"))
            path_cheevo_unlock = path;
         else if (string_is_equal_noncase(basename_noext, "up"))
            path_up = path;
         else if (string_is_equal_noncase(basename_noext, "down"))
            path_down = path;
      }
   }

   if (path_ok && audio_enable_menu_ok)
      task_push_audio_mixer_load(path_ok, NULL, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_OK);
   if (path_cancel && audio_enable_menu_cancel)
      task_push_audio_mixer_load(path_cancel, NULL, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_CANCEL);
   if (audio_enable_menu_notice)
   {
      if (path_notice)
         task_push_audio_mixer_load(path_notice, NULL, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_NOTICE);
      if (path_notice_back)
          task_push_audio_mixer_load(path_notice_back, NULL, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_NOTICE_BACK);
   }

#if defined(HAVE_MENU)
   if (path_bgm && audio_enable_menu_bgm)
      task_push_audio_mixer_load(path_bgm, audio_driver_load_menu_bgm_callback, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_BGM);
#endif

   if (path_cheevo_unlock && audio_enable_cheevo_unlock)
      task_push_audio_mixer_load(path_cheevo_unlock, NULL, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_ACHIEVEMENT_UNLOCK);

   if (audio_enable_menu_scroll)
   {
      if (path_up)
         task_push_audio_mixer_load(path_up, NULL, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_UP);
      if (path_down)
         task_push_audio_mixer_load(path_down, NULL, NULL, true, AUDIO_MIXER_SLOT_SELECTION_MANUAL, AUDIO_MIXER_SYSTEM_SLOT_DOWN);
   }

end:
   if (list)
      string_list_free(list);
   if (list_fallback)
      string_list_free(list_fallback);
}

void audio_driver_mixer_play_stream(unsigned i)
{
   audio_driver_st.mixer_streams[i].stop_cb = audio_mixer_play_stop_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING);
}

void audio_driver_mixer_play_menu_sound_looped(unsigned i)
{
   audio_driver_st.mixer_streams[i].stop_cb = audio_mixer_menu_stop_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING_LOOPED);
}

void audio_driver_mixer_play_menu_sound(unsigned i)
{
   audio_driver_st.mixer_streams[i].stop_cb = audio_mixer_menu_stop_cb;
   audio_driver_mixer_stop_stream(i);
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING);
}

void audio_driver_mixer_play_scroll_sound(bool direction_up)
{
   settings_t *settings          = config_get_ptr();
   bool        audio_enable_menu = settings->bools.audio_enable_menu;
   bool audio_enable_menu_scroll = settings->bools.audio_enable_menu_scroll;
   if (audio_enable_menu && audio_enable_menu_scroll)
      audio_driver_mixer_play_menu_sound(direction_up
            ? AUDIO_MIXER_SYSTEM_SLOT_UP : AUDIO_MIXER_SYSTEM_SLOT_DOWN);
}

void audio_driver_mixer_play_stream_looped(unsigned i)
{
   audio_driver_st.mixer_streams[i].stop_cb = audio_mixer_play_stop_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING_LOOPED);
}

void audio_driver_mixer_play_stream_sequential(unsigned i)
{
   audio_driver_st.mixer_streams[i].stop_cb = audio_mixer_play_stop_sequential_cb;
   audio_driver_mixer_play_stream_internal(i, AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL);
}

float audio_driver_mixer_get_stream_volume(unsigned i)
{
   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return 0.0f;

   return audio_driver_st.mixer_streams[i].volume;
}

void audio_driver_mixer_set_stream_volume(unsigned i, float vol)
{
   audio_mixer_voice_t *voice             = NULL;

   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;

   audio_driver_state_lock();

   audio_driver_st.mixer_streams[i].volume = vol;

   voice                                  =
      audio_driver_st.mixer_streams[i].voice;

   if (voice)
      audio_mixer_voice_set_volume(voice, DB_TO_GAIN(vol));
   audio_driver_state_unlock();
}

/* Callers already holding the state lock use this; the public
 * entry point below takes the lock and calls it. The lock is a
 * plain mutex, so a locked caller that reached the public entry
 * point instead would deadlock against itself. */
static void audio_driver_mixer_stop_stream_locked(unsigned i)
{
   switch (audio_driver_st.mixer_streams[i].state)
   {
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
         {
            audio_mixer_voice_t *voice     = audio_driver_st.mixer_streams[i].voice;

            if (voice)
               audio_mixer_stop(voice);
            audio_driver_st.mixer_streams[i].state   = AUDIO_STREAM_STATE_STOPPED;
            audio_driver_st.mixer_streams[i].volume  = 1.0f;
            if (audio_driver_st.mixer_streams_playing > 0)
               audio_driver_st.mixer_streams_playing--;
         }
         break;
      case AUDIO_STREAM_STATE_STOPPED:
      case AUDIO_STREAM_STATE_NONE:
         break;
   }
}

void audio_driver_mixer_stop_stream(unsigned i)
{
   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;
   audio_driver_state_lock();
   audio_driver_mixer_stop_stream_locked(i);
   audio_driver_state_unlock();
}

static void audio_driver_mixer_remove_stream_locked(unsigned i)
{
   switch (audio_driver_st.mixer_streams[i].state)
   {
      case AUDIO_STREAM_STATE_PLAYING:
      case AUDIO_STREAM_STATE_PLAYING_LOOPED:
      case AUDIO_STREAM_STATE_PLAYING_SEQUENTIAL:
         audio_driver_mixer_stop_stream_locked(i);
         /* fall-through */
      case AUDIO_STREAM_STATE_STOPPED:
         {
            audio_mixer_sound_t *handle = audio_driver_st.mixer_streams[i].handle;
            if (handle)
               audio_mixer_destroy(handle);

            /* A stream loaded without a basename - every menu sound -
             * carries a NULL name, so the pointer is tested before the
             * bytes behind it. */
            if (audio_driver_st.mixer_streams[i].name)
               free(audio_driver_st.mixer_streams[i].name);

            audio_driver_st.mixer_streams[i].state   = AUDIO_STREAM_STATE_NONE;
            audio_driver_st.mixer_streams[i].stop_cb = NULL;
            audio_driver_st.mixer_streams[i].volume  = 0.0f;
            audio_driver_st.mixer_streams[i].handle  = NULL;
            audio_driver_st.mixer_streams[i].voice   = NULL;
            audio_driver_st.mixer_streams[i].name    = NULL;
         }
         break;
      case AUDIO_STREAM_STATE_NONE:
         break;
   }
}

void audio_driver_mixer_remove_stream(unsigned i)
{
   if (i >= AUDIO_MIXER_MAX_SYSTEM_STREAMS)
      return;
   audio_driver_state_lock();
   audio_driver_mixer_remove_stream_locked(i);
   audio_driver_state_unlock();
}

bool audio_driver_mixer_toggle_mute(void)
{
   audio_driver_st.mixer_mute_enable  =
      !audio_driver_st.mixer_mute_enable;
   return true;
}
#endif

bool audio_driver_enable_callback(void)
{
   if (!audio_driver_st.callback.callback)
      return false;
   if (audio_driver_st.callback.set_state)
      audio_driver_st.callback.set_state(true);
   return true;
}

bool audio_driver_disable_callback(void)
{
   if (!audio_driver_st.callback.callback)
      return false;

   if (audio_driver_st.callback.set_state)
      audio_driver_st.callback.set_state(false);
   return true;
}

bool audio_driver_callback(void)
{
   /* Runs on the audio thread: read the main thread's published
    * snapshot, not the runloop, menu or settings themselves.
    *
    * The snapshot alone is not enough here: it is republished around
    * retro_run and driver start and stop, and a pause is exactly when
    * retro_run stops being called, so the pause it announces is the
    * one state the snapshot cannot be trusted to carry. core_silenced
    * is stored by audio_driver_pause_fade() on every pause, resume and
    * menu transition, so it is current when the snapshot is not.
    * Without it this loop kept invoking the core's callback behind the
    * menu, rendering audio for the delivery gates to drop against the
    * same core_silenced word, and asked again at the idle interval for
    * as long as the menu stayed open. */
   int  snap        = retro_atomic_load_acquire_int(
         &audio_driver_st.runloop_snapshot);
   bool core_paused = audio_driver_core_silenced()
      || (snap & AUDIO_SNAP_PAUSED)
      || (   (snap & AUDIO_SNAP_MENU_PAUSES)
          && (snap & AUDIO_SNAP_MENU_ALIVE)
          && (snap & AUDIO_SNAP_ALLOW_PAUSE));

#ifdef HAVE_THREADS
   if (audio_driver_st.pipe_threaded)
   {
      (void)core_paused;
      if (audio_driver_st.pipe_transport)
         audio_driver_transport_consume(&audio_driver_st);
      else
         audio_driver_pipeline_consume(&audio_driver_st);
      return true;
   }
#endif

   if (!audio_driver_st.callback.callback)
      return false;

   /* This callback owns inline source timing; the main frame must not reset it. */
   if (core_paused || !(snap & AUDIO_SNAP_FASTMOTION))
      audio_driver_ff_mult_reset(&audio_driver_st);

   if (!core_paused)
   {
      /* Counted at every device write this thread makes, whichever
       * route the core's samples take to it. */
      uint64_t offered = audio_driver_st.sink_offered_raw;
      if (audio_driver_st.inline_transport)
         audio_driver_st.inline_transport->source_progress = false;

      audio_driver_st.callback.callback();
      /* The core rendered on this (audio) thread; deliver it now rather
       * than holding a partial chunk across callbacks. */
      if (audio_driver_st.data_ptr)
         audio_driver_sample_accum_flush(&audio_driver_st);

      /* Bounded stretch lookahead may consume several batches before a write. */
      if (audio_driver_st.sink_offered_raw != offered
            || (audio_driver_st.inline_transport
               && audio_driver_st.inline_transport->source_progress))
         return true;
   }

   /* No device write or bounded transport input: park before asking again. */
   return false;
}

bool audio_driver_has_callback(void)
{
   return audio_driver_st.callback.callback != NULL;
}

static INLINE bool audio_driver_alive(void)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   const audio_driver_t *audio    = audio_st->current_audio;
   if (audio && audio->alive && audio_st->context_audio_data)
      return audio->alive(audio_st->context_audio_data);
   return false;
}

bool audio_driver_start(bool is_shutdown)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   const audio_driver_t *audio    = audio_st->current_audio;
   if (
            !audio
         || !audio->start
         || !audio_st->context_audio_data)
      goto error;
   /* Set before the driver's start(): the wrapper's start releases its
    * thread, which reads flags from then on, so the write has to be
    * ordered before that release by the wrapper's own lock. */
   audio_driver_publish_runloop();
   AUDIO_FLAGS_SET(audio_st, AUDIO_FLAG_STARTED);
   if (!audio->start(audio_st->context_audio_data, is_shutdown))
   {
      AUDIO_FLAGS_CLEAR(audio_st, AUDIO_FLAG_STARTED);
      goto error;
   }

   RARCH_DBG("[Audio] Started audio driver \"%s\" (is_shutdown=%s)\n",
         audio->ident, is_shutdown ? "true" : "false");

   return true;

error:
   RARCH_ERR("%s\n",
         msg_hash_to_str(MSG_FAILED_TO_START_AUDIO_DRIVER));
   AUDIO_FLAGS_CLEAR(&audio_driver_st, AUDIO_FLAG_ACTIVE);
   return false;
}

/* The driver's buffer as opened, in milliseconds of the output format
 * at the output rate: what buffer_size() reported at init, which is
 * what the latency setting asked for as far as the driver could honour
 * it. Zero when no driver is up or it reports no buffer. Half of it is
 * where rate control holds the fill, so half of it in time is the
 * latency heard from the driver at steady state. */
void audio_driver_set_device_latency(size_t frames)
{
   audio_driver_st.device_latency_frames = frames;
}

double audio_driver_get_device_latency_ms(void)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   settings_t *settings           = config_get_ptr();
   unsigned    rate               = settings->uints.audio_output_sample_rate;

   if (!audio_st->current_audio || !audio_st->device_latency_frames || !rate)
      return 0.0;
   return (double)audio_st->device_latency_frames * 1000.0 / rate;
}

double audio_driver_get_buffer_latency_ms(void)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   settings_t *settings           = config_get_ptr();
   unsigned    rate               = settings->uints.audio_output_sample_rate;
   size_t      frame_bytes        = audio_driver_dev_frame_bytes(audio_st);

   if (!audio_st->current_audio || !audio_st->buffer_size || !rate)
      return 0.0;
   return (double)audio_st->buffer_size / frame_bytes * 1000.0 / rate;
}

bool audio_driver_take_reinit_request(void)
{
   /* fetch-and with zero: the old value, and none left. Exchange
    * would do, but is not on every backend of retro_atomic.h - the
    * volatile fallback, which the PS2 and older ARM toolchains take,
    * has no CAS-class operations; fetch-and is on all of them. */
   return retro_atomic_fetch_and_int(&audio_driver_st.reinit_request, 0) != 0;
}

void audio_driver_set_float_gate(audio_driver_float_gate_t gate)
{
   audio_driver_st.float_gate = gate;
}

/* Negotiation runs on the source owner, between audio callbacks. */
static void audio_driver_inline_set_format(audio_driver_state_t *audio_st,
      bool floating, bool multi)
{
   struct audio_inline_transport *t = audio_st->inline_transport;
   unsigned channels;
   if (audio_st->pipe_threaded || !t)
      return;
   /* Keep the canonical arena when returning to stereo. */
   channels = multi ? AUDIO_PIPE_CANON_CHANNELS : t->channels;
   if (t->floating == floating && t->channels == channels)
      return;
   if (audio_st->data_ptr)
      audio_driver_sample_accum_flush(audio_st);
   audio_driver_state_lock();
   audio_driver_inline_free(audio_st);
   audio_driver_reset_resamplers(audio_st);
   audio_st->last_flush_time = 0;
   audio_st->avg_flush_delta = 0;
   audio_st->avg_expected_delta = 0;
   /* A format boundary discards the old tail. Allocation failure leaves
    * ordinary native playback available until audio reinitialization. */
   audio_driver_inline_prepare(audio_st, floating, channels);
   audio_driver_state_unlock();
}

void audio_driver_set_core_multi(bool core_multi)
{
   audio_driver_inline_set_format(&audio_driver_st,
         audio_driver_st.core_float, core_multi);
   audio_driver_st.core_multi = core_multi;
}

#ifdef HAVE_THREADS
static void audio_driver_transport_clear(audio_driver_state_t *audio_st)
{
   audio_st->pipe_transport_follow = 0;
   audio_st->pipe_pending = NULL;
   audio_st->pipe_pending_bytes = 0;
   audio_driver_state_lock();
   audio_driver_reset_resamplers(audio_st);
   audio_driver_state_unlock();
   audio_pipeline_stretch_free(audio_st->pipe_transport);
   audio_st->pipe_transport = NULL;
   audio_pipeline_stretch_free(audio_st->pipe_transport_suspended);
   audio_st->pipe_transport_suspended = NULL;
   if (audio_st->pipe_transport_output)
      memalign_free(audio_st->pipe_transport_output);
   audio_st->pipe_transport_output = NULL;
   audio_st->pipe_transport_serial = audio_st->pipe_transport_search = 0;
   audio_st->pipe_transport_rate = 0;
}

static bool audio_driver_transport_bind(audio_driver_state_t *audio_st,
      unsigned rate, uint32_t search_channels, bool floating)
{
   audio_pipeline_stretch_t *stage;
   void *output;
   uint8_t follow = audio_st->pipe_transport_follow;
   size_t frame = audio_st->pipe_channels * (floating ? sizeof(float) : sizeof(int16_t));
   size_t frames = audio_st->pipe_pass_frames;
   if (rate < 8000 || rate > 192000 || !audio_st->pipe_channels
         || audio_st->pipe_channels > AUDIO_PIPE_CANON_CHANNELS || !frames
         || frames > SIZE_MAX / frame) return false;
   output = memalign_alloc(64, frames * frame);
   if (!output) return false;
   stage = audio_pipeline_stretch_new(rate, audio_st->pipe_channels,
         floating, search_channels, &audio_st->pipe_ring,
         &audio_st->pipe_layouts, output, frames);
   if (!stage)
   {
      memalign_free(output);
      return false;
   }
   audio_driver_transport_clear(audio_st);
   /* Both owners are parked and the source is empty. Start at an aligned
    * physical frame, retaining the latest published processing request. */
   {
      audio_pipeline_layout_t *metadata = &audio_st->pipe_layouts;
      uint32_t control = metadata->published_control;
      uint32_t cutoff = metadata->published_cutoff;
      unsigned layout = metadata->published_layout;
      retro_spsc_clear(&audio_st->pipe_ring);
      audio_driver_pipe_marks_clear(audio_st);
      audio_pipeline_layout_init(metadata, layout);
      metadata->published_control = metadata->current_control = control;
      metadata->published_cutoff = metadata->current_cutoff = cutoff;
   }
   audio_st->pipe_transport = stage;
   audio_st->pipe_transport_follow = follow;
   audio_st->pipe_transport_output = output;
   audio_st->pipe_transport_rate = rate;
   audio_st->pipe_transport_search = search_channels;
   return true;
}

enum audio_transport_action
{
   AUDIO_TRANSPORT_PREPARE,
   AUDIO_TRANSPORT_RELEASE,
   AUDIO_TRANSPORT_SUSPEND,
   AUDIO_TRANSPORT_RESUME
};

struct audio_transport_prepare
{
   unsigned rate;
   uint32_t search, control, cutoff;
   enum audio_transport_action action;
   bool result;
};

static void audio_driver_transport_control(void *userdata)
{
   struct audio_transport_prepare *request = (struct audio_transport_prepare*)userdata;
   audio_driver_state_t *audio_st = &audio_driver_st;
   if (request->action == AUDIO_TRANSPORT_RELEASE)
   {
      if (audio_st->pipe_transport || audio_st->pipe_transport_suspended)
         audio_driver_transport_clear(audio_st);
      request->result = true;
   }
   else if (request->action == AUDIO_TRANSPORT_SUSPEND)
   {
      request->result = audio_driver_transport_discard(0, false);
      if (request->result)
      {
         audio_st->pipe_transport_suspended = audio_st->pipe_transport;
         audio_st->pipe_transport = NULL;
      }
   }
   else if (audio_st->pipe_threaded
         && audio_st->current_audio && audio_st->current_audio->wait_writable
         && audio_st->context_audio_data
         && !retro_spsc_read_avail(&audio_st->pipe_ring)
         && !audio_st->pipe_pending_bytes)
   {
      if (request->action == AUDIO_TRANSPORT_RESUME)
      {
         request->result = audio_st->pipe_transport_suspended != NULL;
         if (request->result)
         {
            audio_pipeline_layout_t *q = &audio_st->pipe_layouts;
            retro_spsc_clear(&audio_st->pipe_ring);
            audio_driver_pipe_marks_clear(audio_st);
            audio_pipeline_layout_init(q, q->published_layout);
            audio_st->pipe_transport = audio_st->pipe_transport_suspended;
            audio_st->pipe_transport_suspended = NULL;
            audio_pipeline_stretch_discard(audio_st->pipe_transport, 0);
         }
      }
      else
      {
         request->result = audio_driver_transport_bind(audio_st,
               request->rate, request->search, audio_st->pipe_float);
         if (request->result) audio_st->pipe_transport_follow = 0;
      }
      if (request->result && request->control)
      {
         audio_pipeline_layout_t *q = &audio_st->pipe_layouts;
         q->published_control = q->current_control = request->control;
         q->published_cutoff = q->current_cutoff = request->cutoff;
      }
   }
}

static void audio_driver_transport_transaction(struct audio_transport_prepare *request)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   if (audio_st->current_audio &&
         string_is_equal(audio_st->current_audio->ident, "audio-thread"))
      audio_thread_apply_control(audio_st->context_audio_data,
            audio_driver_transport_control, request);
   else
      audio_driver_transport_control(request);
}

static bool audio_driver_transport_recover(bool resume, uint32_t tempo)
{
   struct audio_transport_prepare request;
   audio_driver_state_t *audio_st = &audio_driver_st;
   request.rate = 0; request.search = 0;
   request.control = (tempo == 65536 || audio_st->transport_lpf_only)
      ? tempo : tempo | AUDIO_PIPELINE_STRETCH;
   request.cutoff = (audio_st->pipe_transport_follow & AUDIO_TRANSPORT_LOWPASS)
      ? audio_speed_lpf_cutoff(audio_st->pipe_transport_rate, tempo) : 0;
   request.action = resume ? AUDIO_TRANSPORT_RESUME : AUDIO_TRANSPORT_SUSPEND;
   request.result = false;
   audio_driver_transport_transaction(&request);
   return request.result;
}

bool audio_driver_pipeline_transport_prepare(unsigned rate, uint32_t search_channels)
{
   struct audio_transport_prepare request;
   request.rate = rate; request.search = search_channels;
   request.control = request.cutoff = 0;
   request.action = AUDIO_TRANSPORT_PREPARE; request.result = false;
   audio_driver_transport_transaction(&request);
   return request.result;
}

bool audio_driver_pipeline_transport_prepare_runloop(unsigned rate,
      uint32_t search_channels, bool lowpass)
{
   struct audio_transport_prepare request;
   uint32_t tempo;
   if (!audio_driver_transport_runloop_tempo(&tempo)) return false;
   request.rate = rate; request.search = search_channels;
   request.control = (tempo == 65536 || audio_driver_st.transport_lpf_only)
      ? tempo : tempo | AUDIO_PIPELINE_STRETCH;
   request.cutoff = lowpass ? audio_speed_lpf_cutoff(rate, tempo) : 0;
   request.action = AUDIO_TRANSPORT_PREPARE; request.result = false;
   audio_driver_transport_transaction(&request);
   return request.result;
}

bool audio_driver_pipeline_transport_start_runloop(unsigned rate,
      uint32_t search_channels, bool lowpass)
{
   if (!audio_driver_pipeline_transport_prepare_runloop(rate, search_channels, lowpass))
      return false;
   audio_driver_st.pipe_transport_follow = AUDIO_TRANSPORT_FOLLOW | AUDIO_TRANSPORT_UPDATE
      | (lowpass ? AUDIO_TRANSPORT_LOWPASS : 0);
   return true;
}

void audio_driver_pipeline_transport_release(void)
{
   struct audio_transport_prepare request;
   request.rate = 0; request.search = 0;
   request.control = request.cutoff = 0;
   request.action = AUDIO_TRANSPORT_RELEASE; request.result = false;
   audio_driver_transport_transaction(&request);
}

static void audio_driver_pipe_set_format(void *userdata)
{
   audio_driver_state_t *audio_st = (audio_driver_state_t*)userdata;
   /* Empty source alone is not quiescence: the wrapper must also be parked. */
   if (!retro_spsc_read_avail(&audio_st->pipe_ring))
   {
      bool suspended = audio_st->pipe_transport_suspended != NULL;
      if ((audio_st->pipe_transport || suspended)
            && !audio_driver_transport_bind(audio_st, audio_st->pipe_transport_rate,
               audio_st->pipe_transport_search, audio_st->core_float))
         /* Keep the new source native even if optional state cannot be rebuilt. */
         audio_driver_transport_clear(audio_st);
      if (suspended && audio_st->pipe_transport)
      {
         audio_st->pipe_transport_suspended = audio_st->pipe_transport;
         audio_st->pipe_transport = NULL;
      }
      audio_st->pipe_float = audio_st->core_float;
      audio_st->pipe_frame_bytes = (size_t)audio_st->pipe_channels
            * (audio_st->pipe_float ? sizeof(float) : sizeof(int16_t));
   }
}
#endif

void audio_driver_set_core_float(bool core_float)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   audio_driver_inline_set_format(audio_st, core_float, audio_st->core_multi);
#ifdef HAVE_REWIND
   /* Captured samples belong to the previous native arena. */
   if (audio_st->core_float != core_float)
      audio_st->rewind_ptr = audio_st->rewind_size;
#endif
   audio_st->core_float = core_float;
#ifdef HAVE_THREADS
   if (audio_st->pipe_threaded && audio_st->pipe_float != core_float)
   {
      /* A populated ring keeps its native format. Recheck after parking,
       * since the consumer may finish its last source while we wait. */
      if (audio_st->current_audio &&
            string_is_equal(audio_st->current_audio->ident, "audio-thread"))
         audio_thread_apply_control(audio_st->context_audio_data,
               audio_driver_pipe_set_format, audio_st);
      else
         audio_driver_pipe_set_format(audio_st);
   }
#endif
}

/* The speaker path in effect, for the overlay: the layout the device
 * opened with and how it is filled, in a few words. */
static const char *audio_layout_name(uint32_t layout, char *buf, size_t len)
{
   switch (layout)
   {
      case AUDIO_LAYOUT_STEREO:           return "stereo";
      case AUDIO_LAYOUT_QUAD:             return "4.0";
      case AUDIO_LAYOUT_5POINT1:          return "5.1";
      case AUDIO_LAYOUT_5POINT1_SURROUND: return "5.1 sides";
      case AUDIO_LAYOUT_7POINT1:          return "7.1";
      default: break;
   }
   if (layout == AUDIO_SPEAKER_FRONT_CENTER)
      return "mono";
   snprintf(buf, len, "%u channels", audio_layout_channels(layout));
   return buf;
}

size_t audio_driver_get_layout_desc(char *s, size_t len)
{
   const audio_driver_state_t *st = &audio_driver_st;
   char tmp[24];
   const char *name = audio_layout_name(st->out_layout, tmp, sizeof(tmp));
   /* what the core delivers, when it is not the stereo everything
    * else is: folded to stereo at the boundary today */
   const char *from = (st->core_layout && st->core_layout != AUDIO_LAYOUT_STEREO)
         ? audio_layout_name(st->core_layout, tmp, sizeof(tmp)) : "stereo";
   if (st->out_channels > 2)
      return snprintf(s, len, "%s (upmixed from %s)", name, from);
   if (st->virtualize)
      return snprintf(s, len, "stereo, virtual 5.1 to headphones");
   if (st->core_layout && st->core_layout != AUDIO_LAYOUT_STEREO)
      return snprintf(s, len, "stereo (folded from %s)", from);
   return snprintf(s, len, "stereo");
}

const char *audio_driver_get_ident(void)
{
   audio_driver_state_t *audio_st = &audio_driver_st;
   const audio_driver_t *audio    = audio_st->current_audio;
   if (!audio)
      return NULL;
#ifdef HAVE_THREADS
   /* Threaded pipeline: current_audio is the wrapper. Report the driver
    * it wraps, which is what every caller of this actually wants. */
   if (string_is_equal(audio->ident, "audio-thread"))
   {
      const char *wrapped = audio_thread_wrapped_ident(audio_st->context_audio_data);
      if (wrapped)
         return wrapped;
   }
#endif
   return audio->ident;
}

bool audio_driver_stop(void)
{
   bool stopped;
   audio_driver_state_t *audio_st = &audio_driver_st;
   const audio_driver_t *audio    = audio_st->current_audio;
   if (     !audio
         || !audio->stop
         || !audio_driver_st.context_audio_data
         || !audio_driver_alive()
      )
      return false;
   audio_driver_publish_runloop();
   stopped = audio->stop(audio_driver_st.context_audio_data);

   if (stopped)
   {
      /* The stopped interval is not part of the source cadence. */
      audio_driver_ff_mult_reset(audio_st);
#ifdef HAVE_THREADS
      retro_atomic_store_release_int(&audio_st->pipe_ff_mult_q16, 65536);
#endif
      /* The wrapper has parked the consumer before returning from stop. */
#ifdef HAVE_THREADS
      if (!audio_driver_pipeline_transport_discard(0))
#endif
      {
         audio_st->pipe_pending = NULL;
         audio_st->pipe_pending_bytes = 0;
         audio_driver_state_lock();
         audio_driver_inline_reset(audio_st);
         audio_driver_reset_resamplers(audio_st);
         audio_driver_state_unlock();
      }
      AUDIO_FLAGS_CLEAR(audio_st, AUDIO_FLAG_STARTED);
      RARCH_DBG("[Audio] Stopped audio driver \"%s\".\n", audio->ident);
   }

   return stopped;
}

#ifdef HAVE_REWIND
void audio_driver_frame_is_reverse(void)
{
   audio_driver_state_t *audio_st  = &audio_driver_st;
   uint32_t runloop_flags          = (uint32_t)retro_atomic_load_acquire_int(
         &audio_st->runloop_snapshot);

   bool  rewind_float = audio_st->core_float && audio_st->rewind_buf_f;

   if (audio_st->rewind_ptr >= audio_st->rewind_size
         || (!rewind_float && !audio_st->rewind_buf))
      return;

   audio_driver_record_push(audio_st,
         rewind_float ? (const void*)(audio_st->rewind_buf_f + audio_st->rewind_ptr)
                      : (const void*)(audio_st->rewind_buf + audio_st->rewind_ptr),
         (audio_st->rewind_size - audio_st->rewind_ptr) / 2,
         2, AUDIO_LAYOUT_STEREO, rewind_float);

   if (!(
             (runloop_flags & AUDIO_SNAP_PAUSED)
         || !(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_ACTIVE)
         || !(audio_st->output_samples_buf)))
      if (!(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_SUSPENDED))
         audio_driver_submit(audio_st,
               audio_driver_snapshot_slowmotion(audio_st),
               rewind_float
                  ? (const void*)(audio_st->rewind_buf_f + audio_st->rewind_ptr)
                  : (const void*)(audio_st->rewind_buf   + audio_st->rewind_ptr),
               audio_st->rewind_size - audio_st->rewind_ptr, rewind_float,
               (runloop_flags & AUDIO_SNAP_SLOWMOTION) ? true : false,
               (runloop_flags & AUDIO_SNAP_FASTMOTION) ? true : false,
               true);

   /* A reversed frame is published here, before the frame end that
    * follows it, which will wake the consumer for it. */
}
#endif

void audio_set_float(enum audio_action action, float val)
{
   switch (action)
   {
      case AUDIO_ACTION_VOLUME_GAIN:
         audio_driver_st.volume_gain        = DB_TO_GAIN(val);
         break;
      case AUDIO_ACTION_MIXER_VOLUME_GAIN:
#ifdef HAVE_AUDIOMIXER
         audio_driver_st.mixer_volume_gain  = DB_TO_GAIN(val);
#endif
         break;
      case AUDIO_ACTION_RATE_CONTROL_DELTA:
         audio_driver_st.rate_control_delta = val;
         break;
      case AUDIO_ACTION_NONE:
      default:
         break;
   }
}

float *audio_get_float_ptr(enum audio_action action)
{
   switch (action)
   {
      case AUDIO_ACTION_RATE_CONTROL_DELTA:
         return &audio_driver_st.rate_control_delta;
      case AUDIO_ACTION_NONE:
      default:
         break;
   }

   return NULL;
}

bool *audio_get_bool_ptr(enum audio_action action)
{
   switch (action)
   {
      case AUDIO_ACTION_MIXER_MUTE_ENABLE:
#ifdef HAVE_AUDIOMIXER
         return &audio_driver_st.mixer_mute_enable;
#else
         break;
#endif
      case AUDIO_ACTION_MUTE_ENABLE:
         return &audio_driver_st.mute_enable;
      case AUDIO_ACTION_NONE:
      default:
         break;
   }

   return NULL;
}

bool audio_compute_buffer_statistics(audio_statistics_t *stats)
{
   unsigned i, low_water_size, high_water_size, avg, stddev;
   uint64_t accum                 = 0;
   uint64_t accum_var             = 0;
   unsigned low_water_count       = 0;
   unsigned high_water_count      = 0;
   audio_driver_state_t *audio_st = &audio_driver_st;
   unsigned samples               = MIN(
         (unsigned)audio_st->free_samples_count,
         AUDIO_BUFFER_FREE_SAMPLES_COUNT);

   if (samples < 3)
      return false;

   stats->samples                 = (unsigned)
      audio_st->free_samples_count;

   if (!(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_CONTROL))
      return false;

#ifdef WARPUP
   /* uint64 to double not implemented, fair chance
    * signed int64 to double doesn't exist either */
   /* https://forums.libretro.com/t/unsupported-platform-help/13903/ */
   (void)stddev;
#elif defined(_MSC_VER) && _MSC_VER <= 1200
   /* FIXME: error C2520: conversion from unsigned __int64
    * to double not implemented, use signed __int64 */
   (void)stddev;
#else
   for (i = 1; i < samples; i++)
      accum += audio_st->free_samples_buf[i];

   avg = (unsigned)accum / (samples - 1);

   for (i = 1; i < samples; i++)
   {
      int diff     = avg - audio_st->free_samples_buf[i];
      accum_var   += diff * diff;
   }

   stddev                                = (unsigned)
      sqrt((double)accum_var / (samples - 2));

   stats->average_buffer_saturation      = (1.0f - (float)avg
         / audio_st->buffer_size) * 100.0;
   stats->std_deviation_percentage       = ((float)stddev
         / audio_st->buffer_size)  * 100.0;
#endif

   low_water_size  = (unsigned)(audio_st->buffer_size * 3 / 4);
   high_water_size = (unsigned)(audio_st->buffer_size     / 4);

   for (i = 1; i < samples; i++)
   {
      if (audio_st->free_samples_buf[i] >= low_water_size)
         low_water_count++;
      else if (audio_st->free_samples_buf[i] <= high_water_size)
         high_water_count++;
   }

   stats->close_to_underrun      = (100.0f * low_water_count)  / (samples - 1);
   stats->close_to_blocking      = (100.0f * high_water_count) / (samples - 1);

   return true;
}

#ifdef HAVE_MENU
void audio_driver_menu_sample(void)
{
   static int16_t samples_buf[1024]       = {0};
   settings_t *settings                   = config_get_ptr();
   float slowmotion_ratio                 = settings->floats.slowmotion_ratio;
   video_driver_state_t *video_st         = video_state_get_ptr();
   uint32_t runloop_flags                 = runloop_get_flags();
   recording_state_t *recording_st        = recording_state_get_ptr();
   struct retro_system_av_info *av_info   = &video_st->av_info;
   const struct retro_system_timing *info =
      (const struct retro_system_timing*)&av_info->timing;
   unsigned sample_count;
   double frames;
   uint64_t rate_bits, fps_bits;
   audio_driver_state_t *audio_st         = &audio_driver_st;
   bool check_flush                       = !(
            !(AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_ACTIVE)
         || !audio_st->output_samples_buf);

   memcpy(&rate_bits, &info->sample_rate, sizeof(rate_bits));
   memcpy(&fps_bits, &info->fps, sizeof(fps_bits));
   if (rate_bits >= UINT64_C(0x7ff0000000000000)
         || fps_bits >= UINT64_C(0x7ff0000000000000)
         || info->sample_rate <= 0.0 || info->fps <= 0.0)
      return;
   frames = floor(info->sample_rate / info->fps);
   /* Validate before conversion and the stereo sample-count multiply. */
   if (!(frames >= 0.0 && frames <= UINT_MAX / 2))
      return;
   sample_count = (unsigned)frames * 2;

   if ((AUDIO_FLAGS_GET(audio_st) & AUDIO_FLAG_SUSPENDED))
      check_flush                         = false;

   while (sample_count > 1024)
   {
      if (  recording_st->data   &&
            recording_st->driver &&
            recording_st->driver->push_audio)
      {
         struct record_audio_data ffemu_data;

         ffemu_data.data                    = samples_buf;
         ffemu_data.frames                  = 1024 / 2;

         recording_st->driver->push_audio(
               recording_st->data, &ffemu_data);
      }
      if (check_flush)
         audio_driver_submit(audio_st,
               slowmotion_ratio,
               samples_buf,
               1024, false,
               (runloop_flags & AUDIO_SNAP_SLOWMOTION) ? true : false,
               (runloop_flags & AUDIO_SNAP_FASTMOTION) ? true : false,
               false);
      sample_count -= 1024;
   }

   if (     recording_st->data
         && recording_st->driver
         && recording_st->driver->push_audio)
   {
      struct record_audio_data ffemu_data;

      ffemu_data.data                    = samples_buf;
      ffemu_data.frames                  = sample_count / 2;

      recording_st->driver->push_audio(
            recording_st->data, &ffemu_data);
   }

   if (check_flush)
      audio_driver_submit(audio_st, slowmotion_ratio, samples_buf, sample_count, false,
            (runloop_flags & AUDIO_SNAP_SLOWMOTION) ? true : false,
            (runloop_flags & AUDIO_SNAP_FASTMOTION) ? true : false,
            false);

   /* This is the menu's frame; no frame end follows it. */
   audio_driver_ff_frame_end(audio_st);
   audio_driver_pipeline_signal(audio_st);
}
#endif

#ifdef HAVE_MICROPHONE
microphone_driver_state_t *microphone_state_get_ptr(void)
{
   return &mic_driver_st;
}

#define mic_driver_get_sample_size(microphone) \
   (((microphone)->flags & MICROPHONE_FLAG_USE_FLOAT) ? sizeof(float) : sizeof(int16_t))

#ifdef HAVE_THREADS
/* Defined below, next to microphone_driver_read(), which is the other
 * half of the same split. */
static void microphone_driver_capture_thread(void *data);
#endif

static bool mic_driver_open_mic_internal(retro_microphone_t* microphone);
bool microphone_driver_start(void)
{
   microphone_driver_state_t *mic_st = &mic_driver_st;
   retro_microphone_t    *microphone = &mic_st->microphone;

   /* If there's an opened microphone that the core turned on... */
   if (microphone->flags & MICROPHONE_FLAG_ACTIVE)
   {
      /* If this microphone was requested before the driver was ready...*/
      if (microphone->flags & MICROPHONE_FLAG_PENDING)
      {
         retro_assert(microphone->microphone_context == NULL);
         /* The microphone context shouldn't have been created yet */

         /* Now that the driver and driver context are ready, let's initialize the mic */
         if (mic_driver_open_mic_internal(microphone))
         {
            /* open_mic_internal will start the microphone if it's enabled */
            RARCH_DBG("[Microphone] Initialized a previously-pending microphone.\n");
         }
         else
         {
            RARCH_ERR("[Microphone] Failed to initialize a previously pending microphone; microphone will not be used.\n");

            microphone_driver_close_mic(microphone);
            /* Not returning false because a mic failure shouldn't take down the driver;
             * what if the player just unplugged their mic? */
         }
      }
      /* The microphone was already created, so let's just unpause it */
      else
      {
         microphone_driver_set_mic_state(microphone, true);

         RARCH_DBG("[Microphone] Started a microphone that was enabled when the driver was last stopped.\n");
      }
   }

   return true;
}

bool microphone_driver_stop(void)
{
   microphone_driver_state_t *mic_st = &mic_driver_st;
   retro_microphone_t    *microphone = &mic_st->microphone;

   /* If there's an opened microphone that the core
    * turned on and received... */
   if (      (microphone->flags & MICROPHONE_FLAG_ACTIVE)
         &&  (microphone->flags & MICROPHONE_FLAG_ENABLED)
         && !(microphone->flags & MICROPHONE_FLAG_PENDING))
      return mic_st->driver->stop_mic(mic_st->driver_context,
               microphone->microphone_context);
   /* If the mic is pending, then we don't need to do anything. */
   return true;
}

/**
 * config_get_microphone_driver_options:
 *
 * Get an enumerated list of all microphone driver names, separated by '|'.
 *
 * Returns: string listing of all microphone driver names, separated by '|'.
 **/
const char *config_get_microphone_driver_options(void)
{
   return char_list_new_special(STRING_LIST_MICROPHONE_DRIVERS, NULL);
}

bool microphone_driver_find_driver(void *settings_data, const char *prefix,
      bool verbosity_enabled)
{
#ifdef HAVE_ALSA
   /* "alsathread" was a second ALSA capture driver, removed once the
    * frontend's threaded capture covered what it did; a config that
    * still names it means ALSA, not the first entry in the table.
    * Aliased for a release, then to go. */
   {
      settings_t *_settings = (settings_t*)settings_data;
      if (string_is_equal(_settings->arrays.microphone_driver, "alsathread"))
         strlcpy(_settings->arrays.microphone_driver, "alsa",
               sizeof(_settings->arrays.microphone_driver));
   }
#endif
   settings_t *settings = (settings_t*)settings_data;
   int i                 = (int)driver_find_index(
         "microphone_driver",
         settings->arrays.microphone_driver);

   if (i >= 0)
      mic_driver_st.driver = (const microphone_driver_t *)
            microphone_drivers[i];
   else
   {
      const microphone_driver_t *tmp = NULL;
      if (verbosity_enabled)
      {
         unsigned d;
         RARCH_ERR("Couldn't find any %s named \"%s\".\n", prefix,
                   settings->arrays.microphone_driver);

         RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
         for (d = 0; microphone_drivers[d]; d++)
         {
            if (microphone_drivers[d])
               RARCH_LOG_OUTPUT("\t%s\n", microphone_drivers[d]->ident);
         }
         RARCH_WARN("Going to default to first %s...\n", prefix);
      }

      tmp = (const microphone_driver_t *)microphone_drivers[0];

      if (!tmp)
         return false;
      mic_driver_st.driver = tmp;
   }

   return true;
}

static void mic_driver_microphone_handle_init(retro_microphone_t *microphone,
      const retro_microphone_params_t *params)
{
   if (microphone)
   {
      const settings_t *settings        = config_get_ptr();
      unsigned microphone_sample_rate   = settings->uints.microphone_sample_rate;
      microphone->microphone_context    = NULL;
      microphone->flags                 = MICROPHONE_FLAG_ACTIVE;
      microphone->requested_params.rate = params ? params->rate : microphone_sample_rate;
      microphone->actual_params.rate    = 0;
      /* We don't set the actual parameters until we actually open the mic.
       * (Remember, the core can request one before the driver is ready.) */
      microphone->effective_params.rate = params ? params->rate : microphone_sample_rate;
      /* We set the effective parameters because
       * the frontend has to do what it can
       * to give the core what it asks for. */
   }
}

static void mic_driver_microphone_handle_free(retro_microphone_t *microphone, bool is_reset)
{
   microphone_driver_state_t *mic_st     = &mic_driver_st;
   const microphone_driver_t *mic_driver = mic_st->driver;
   void *driver_context                  = mic_st->driver_context;

   if (!microphone)
      return;

   if (!driver_context)
      RARCH_WARN("[Microphone] Attempted to free a microphone without an active driver context.\n");

#ifdef HAVE_THREADS
   /* First of all, before anything it touches goes away. The worker
    * holds the microphone context across a bounded wait_readable() and
    * a read(), and it writes into the fifo - so closing the device or
    * freeing the fifo underneath it is a use-after-free, which is what
    * ThreadSanitizer reported here: close_mic() used to run first and
    * raced the worker's read on the same handle. */
   if (microphone->capture_thread)
   {
      retro_atomic_store_release_int(&microphone->capture_running, 0);
      /* The worker re-reads capture_running after every return from its
       * park, so releasing it is all this has to do. */
      retro_eventcount_notify(&microphone->capture_park);
      sthread_join(microphone->capture_thread);
      microphone->capture_thread = NULL;
      microphone->worker_sample_size = 0;
   }
   /* Safe on a microphone that never had a worker: init zeroes it and
    * free tolerates that. */
   retro_eventcount_free(&microphone->capture_park);
#endif

   if (microphone->microphone_context)
   {
      mic_driver->close_mic(driver_context, microphone->microphone_context);
      microphone->microphone_context = NULL;
   }

   if (microphone->outgoing_init)
   {
      retro_spsc_free(&microphone->outgoing_samples);
      microphone->outgoing_init = false;
   }

   if (microphone->resampler && microphone->resampler->free && microphone->resampler_data)
      microphone->resampler->free(microphone->resampler_data);

   if (microphone->resampler_data_int16 && microphone->resampler_int16_free)
      microphone->resampler_int16_free(microphone->resampler_data_int16);

   microphone->resampler                = NULL;
   microphone->resampler_data           = NULL;
   microphone->resampler_data_int16     = NULL;
   microphone->resampler_int16_process  = NULL;
   microphone->resampler_int16_free     = NULL;

   /* If the mic driver is being reset and the microphone was already valid... */
   if ((microphone->flags & MICROPHONE_FLAG_ACTIVE) && is_reset)
      microphone->flags |= MICROPHONE_FLAG_PENDING;
      /* ...then we need to keep the handle itself valid
       * so it can be reinitialized.
       * Otherwise the core will lose mic input. */
   else
      memset(microphone, 0, sizeof(*microphone));
   /* Do NOT free the microphone handle itself! It's allocated statically! */
}

/* The staging buffers the mic read pipeline flushes through. Only
 * allocated once a core actually opens a microphone: with mic
 * support enabled by default, allocating them at driver init cost
 * every session over 800KB that all but a handful ever used.
 * Freed and nulled by deinit, so a later open reallocates.
 *
 * Two blocks, one per element type, carved into regions with the same
 * AUDIO_ARENA_NEXT() spacing as the playback arenas: 64-byte aligned
 * starts, one 64-byte pad between regions so no pair sits a multiple
 * of 4 KiB apart. Allocation is all-or-nothing, so a failure never
 * leaves the pipeline with some buffers present and others NULL. */
static bool mic_driver_allocate_frames(microphone_driver_state_t *mic_st)
{
   size_t max_frames    = AUDIO_CHUNK_SIZE_NONBLOCKING * AUDIO_MAX_RATIO;
   /* Cursors are in elements. */
   size_t i16_dual_mono = 0;
   size_t i16_resampled = AUDIO_ARENA_NEXT(i16_dual_mono,
         max_frames * 2, AUDIO_ARENA_ALIGN_INT16);
   size_t i16_final     = AUDIO_ARENA_NEXT(i16_resampled,
         max_frames * 2, AUDIO_ARENA_ALIGN_INT16);
   size_t i16_total     = i16_final + max_frames;
   /* input_frames holds whatever sample format the driver delivers;
    * its region is sized in floats, the widest format. */
   size_t f32_input     = 0;
   size_t f32_converted = AUDIO_ARENA_NEXT(f32_input,
         max_frames, AUDIO_ARENA_ALIGN_FLOAT);
   size_t f32_dual_mono = AUDIO_ARENA_NEXT(f32_converted,
         max_frames, AUDIO_ARENA_ALIGN_FLOAT);
   size_t f32_resampled = AUDIO_ARENA_NEXT(f32_dual_mono,
         max_frames * 2, AUDIO_ARENA_ALIGN_FLOAT);
   size_t f32_mono      = AUDIO_ARENA_NEXT(f32_resampled,
         max_frames * 2, AUDIO_ARENA_ALIGN_FLOAT);
   size_t f32_total     = f32_mono + max_frames;
   int16_t *arena_int16;
   float *arena_float;

   if (mic_st->arena_int16)
      return true;

   arena_int16 = (int16_t*)memalign_alloc(64, i16_total * sizeof(int16_t));
   arena_float = (float*)memalign_alloc(64, f32_total * sizeof(float));

   if (!arena_int16 || !arena_float)
   {
      if (arena_int16)
         memalign_free(arena_int16);
      if (arena_float)
         memalign_free(arena_float);
      return false;
   }

   mic_st->arena_int16                   = arena_int16;
   mic_st->arena_float                   = arena_float;

   mic_st->input_frames                  = arena_float + f32_input;
   mic_st->input_frames_length           = max_frames * sizeof(float);
   mic_st->converted_input_frames        = arena_float + f32_converted;
   mic_st->converted_input_frames_length = max_frames * sizeof(float);
   /* Need room for dual-mono frames */
   mic_st->dual_mono_frames              = arena_float + f32_dual_mono;
   mic_st->dual_mono_frames_length       = max_frames * sizeof(float) * 2;
   mic_st->resampled_frames              = arena_float + f32_resampled;
   mic_st->resampled_frames_length       = max_frames * sizeof(float) * 2;
   mic_st->resampled_mono_frames         = arena_float + f32_mono;
   mic_st->resampled_mono_frames_length  = max_frames * sizeof(float);

   /* Interleaved stereo, for the integer flush path. */
   mic_st->dual_mono_frames_int16        = arena_int16 + i16_dual_mono;
   mic_st->dual_mono_frames_int16_length = max_frames * sizeof(int16_t) * 2;
   mic_st->resampled_frames_int16        = arena_int16 + i16_resampled;
   mic_st->resampled_frames_int16_length = max_frames * sizeof(int16_t) * 2;
   mic_st->final_frames                  = arena_int16 + i16_final;
   mic_st->final_frames_length           = max_frames * sizeof(int16_t);

   return true;
}

bool microphone_driver_init_internal(void *settings_data)
{
   settings_t *settings   = (settings_t*)settings_data;
   microphone_driver_state_t *mic_st = &mic_driver_st;
   bool verbosity_enabled = verbosity_is_enabled();

   /* If the user has mic support turned off... */
   if (!settings->bools.microphone_enable)
   {
      mic_st->flags &= ~MICROPHONE_DRIVER_FLAG_ACTIVE;
      /* Ensure microphone struct is clean to prevent crashes on deinit
       * if there was stale data from a previous session */
      memset(&mic_st->microphone, 0, sizeof(mic_st->microphone));
      return false;
   }

   convert_s16_to_float_init_simd();
   convert_float_to_s16_init_simd();
   audio_driver_clamp_init_simd();

   if (!(microphone_driver_find_driver(settings,
               "microphone driver", verbosity_enabled)))
   {
      RARCH_ERR("[Microphone] Failed to initialize microphone driver. Will continue without mic input.\n");
      goto error;
   }

   if (!mic_st->driver || !mic_st->driver->init)
      goto error;

   if (!(mic_st->driver_context = mic_st->driver->init()))
      goto error;

   if (*settings->arrays.microphone_resampler)
      strlcpy(mic_st->resampler_ident,
            settings->arrays.microphone_resampler,
            sizeof(mic_st->resampler_ident));
   else
      mic_st->resampler_ident[0] = '\0';

   mic_st->resampler_quality     = (enum resampler_quality)settings->uints.microphone_resampler_quality;

   RARCH_LOG("[Microphone] Initialized microphone driver.\n");

   /* The mic driver was initialized, now we're ready to open mics */
   mic_st->flags |= MICROPHONE_DRIVER_FLAG_ACTIVE;

   if (!microphone_driver_start())
      goto error;

   return true;

error:
   RARCH_ERR("[Microphone] Failed to start microphone driver. Will continue without audio input.\n");
   mic_st->flags &= ~MICROPHONE_DRIVER_FLAG_ACTIVE;
   return microphone_driver_deinit(false);
}

/**
 *
 * @param microphone Handle to the microphone to init with a context
 */
static bool mic_driver_open_mic_internal(retro_microphone_t* microphone)
{
   microphone_driver_state_t *mic_st     = &mic_driver_st;
   settings_t *settings                  = config_get_ptr();
   const microphone_driver_t *mic_driver = mic_st->driver;
   void *driver_context                  = mic_st->driver_context;
   unsigned runloop_audio_latency        = runloop_state_get_ptr()->audio_latency;
   unsigned setting_audio_latency        = settings->uints.microphone_latency;
   unsigned audio_latency                = MAX(runloop_audio_latency, setting_audio_latency);
   size_t max_samples                    = AUDIO_CHUNK_SIZE_NONBLOCKING * 1 * AUDIO_MAX_RATIO;

   if (!microphone || !mic_driver || !(mic_st->flags & MICROPHONE_DRIVER_FLAG_ACTIVE))
      return false;

   if (!mic_driver_allocate_frames(mic_st))
      goto error;

   microphone->outgoing_size = max_samples * sizeof(int16_t);
   microphone->outgoing_init = retro_spsc_init(&microphone->outgoing_samples,
         microphone->outgoing_size);
   if (!microphone->outgoing_init)
      goto error;

   microphone->microphone_context = mic_driver->open_mic(driver_context,
      *settings->arrays.microphone_device ? settings->arrays.microphone_device : NULL,
      microphone->requested_params.rate,
      audio_latency,
      &microphone->actual_params.rate);

   if (!microphone->microphone_context)
      goto error;

   microphone_driver_set_mic_state(microphone, microphone->flags & MICROPHONE_FLAG_ENABLED);


   RARCH_LOG("[Microphone] Requested microphone sample rate of %uHz, got %uHz.\n",
             microphone->requested_params.rate,
             microphone->actual_params.rate
   );

   if (     mic_driver->mic_use_float
         && mic_driver->mic_use_float(mic_st->driver_context, microphone->microphone_context))
      microphone->flags      |= MICROPHONE_FLAG_USE_FLOAT;

   microphone->orig_ratio = (double)microphone->effective_params.rate / microphone->actual_params.rate;

   if (!retro_resampler_realloc(
         &microphone->resampler_data,
         &microphone->resampler,
         mic_st->resampler_ident,
         mic_st->resampler_quality,
         microphone->orig_ratio))
   {
      RARCH_ERR("[Microphone] Failed to initialize resampler \"%s\".\n", mic_st->resampler_ident);
      goto error;
   }

   /* The libretro microphone interface hands the core int16 unconditionally,
    * so when the device also delivers int16 there is no reason for the flush
    * to detour through float: build the deterministic integer counterpart of
    * the resampler just chosen and let microphone_driver_flush() use it.
    * Float devices, and resamplers with no integer implementation, leave
    * these NULL and keep the float path. */
   microphone->resampler_data_int16    = NULL;
   microphone->resampler_int16_process = NULL;
   microphone->resampler_int16_free    = NULL;
   if (     !(microphone->flags & MICROPHONE_FLAG_USE_FLOAT)
         &&   microphone->resampler
         &&   microphone->resampler->short_ident)
   {
      retro_resampler_int16_t rs;
      retro_resampler_int16_new(&rs, microphone->resampler->short_ident,
            mic_st->resampler_quality, microphone->orig_ratio, false);
      microphone->resampler_data_int16    = rs.data;
      microphone->resampler_int16_process = rs.process;
      microphone->resampler_int16_free    = rs.free;
      if (!microphone->resampler_data_int16)
      {
         microphone->resampler_int16_process = NULL;
         microphone->resampler_int16_free    = NULL;
      }
      RARCH_LOG("[Microphone] Resample path: %s\n",
            microphone->resampler_data_int16
                  ? "integer s16 (no float round-trip)" : "float");
   }

   microphone->flags &= ~MICROPHONE_FLAG_PENDING;
   RARCH_LOG("[Microphone] Initialized microphone.\n");
#ifdef HAVE_THREADS
   /* Last, after every field the worker reads has been written: the
    * flags, orig_ratio and both resamplers above. Starting it earlier
    * raced all three - ThreadSanitizer caught the worker reading them
    * while this function was still filling them in. */
   /* Threaded capture, on the same setting as the playback pipeline and
    * the same condition: the driver must be able to wait on its device,
    * or the worker would poll. A driver without wait_readable() keeps
    * the frame-synchronous path, exactly as a playback driver without
    * wait_writable() does. */
   if (     settings->bools.audio_threaded_pipeline
         && mic_driver->wait_readable)
   {
      /* Before the thread exists, so it never reads ::flags itself. */
      microphone->worker_sample_size = mic_driver_get_sample_size(microphone);
      retro_atomic_int_init(&microphone->capture_running, 1);

      if (     retro_eventcount_init(&microphone->capture_park)
            && (microphone->capture_thread = sthread_create(
                  microphone_driver_capture_thread, mic_st)))
         RARCH_LOG("[Microphone] Threaded capture: the worker owns the read"
               " and the resampler.\n");
      else
      {
         /* Any part missing and the whole thing is off; the
          * frame-synchronous path below needs none of it. */
         retro_atomic_store_release_int(&microphone->capture_running, 0);
         retro_eventcount_free(&microphone->capture_park);
         microphone->worker_sample_size = 0;
         RARCH_WARN("[Microphone] Could not start the capture worker;"
               " reading on the frame instead.\n");
      }
   }
   else if (settings->bools.audio_threaded_pipeline)
      RARCH_LOG("[Microphone] Threaded capture requested, but driver \"%s\""
            " has no wait_readable(); reading on the frame.\n",
            mic_driver->ident);
#endif
   return true;
error:
   mic_driver_microphone_handle_free(microphone, false);
   RARCH_ERR("[Microphone] Driver attempted to initialize the microphone but failed.\n");
   return false;
}

static void microphone_driver_close_mic_internal(retro_microphone_t *microphone, bool is_reset)
{
   microphone_driver_state_t *mic_st     = &mic_driver_st;
   const microphone_driver_t *mic_driver = mic_st->driver;
   void *driver_context                  = mic_st->driver_context;

   if (     microphone
         && driver_context
         && mic_driver
         && mic_driver->close_mic)
      mic_driver_microphone_handle_free(microphone, is_reset);
}

void microphone_driver_close_mic(retro_microphone_t *microphone)
{
   mic_driver_microphone_handle_free(microphone, false);
}

bool microphone_driver_set_mic_state(retro_microphone_t *microphone, bool state)
{
   microphone_driver_state_t *mic_st     = &mic_driver_st;
   const microphone_driver_t *mic_driver = mic_st->driver;
   void *driver_context                  = mic_st->driver_context;

   if (!microphone
         || !(microphone->flags & MICROPHONE_FLAG_ACTIVE)
         || !mic_driver
         || !mic_driver->start_mic
         || !mic_driver->stop_mic)
      return false;
   /* If the provided microphone was null or invalid, or the driver is incomplete, stop. */

   /* If the driver is initialized... */
   if (driver_context && microphone->microphone_context)
   {
      bool success;

      /* If we want to enable this mic... */
      if (state)
      {
         success = mic_driver->start_mic(driver_context, microphone->microphone_context);
         /* Enable the mic. (Enabling an active mic is a successful noop.) */

         if (success)
         {
            microphone->flags |= MICROPHONE_FLAG_ENABLED;
            RARCH_LOG("[Microphone] Enabled microphone.\n");
         }
         else
         {
            RARCH_ERR("[Microphone] Failed to enable microphone.\n");
         }
      }
      else
      { /* If we want to pause this mic... */
         success = mic_driver->stop_mic(driver_context, microphone->microphone_context);

         /* Disable the mic. (If the mic is already stopped, disabling it should still be successful.) */
         if (success)
         {
            microphone->flags &= ~MICROPHONE_FLAG_ENABLED;
            RARCH_LOG("[Microphone] Disabled microphone.\n");
         }
         else
         {
            RARCH_ERR("[Microphone] Failed to disable microphone.\n");
         }
      }

      return success;
   }
   else
   { /* The driver's not ready yet, so we'll make a note
      * of what the mic's state should be */
      if (state)
         microphone->flags |= MICROPHONE_FLAG_ENABLED;
      else
         microphone->flags &= ~MICROPHONE_FLAG_ENABLED;

      RARCH_DBG("[Microphone] Set pending state to %s.\n",
                state ? "enabled" : "disabled");
      return true;
      /* This isn't an error */
   }
}

bool microphone_driver_get_mic_state(const retro_microphone_t *microphone)
{
   if (!microphone || !(microphone->flags & MICROPHONE_FLAG_ACTIVE))
      return false;
   return microphone->flags & MICROPHONE_FLAG_ENABLED;
}

/**
 * Pull queued microphone samples from the driver
 * and copy them to the provided buffer(s).
 *
 * Note that microphone samples are provided in mono,
 * so a "sample" and a "frame" are equivalent here.
 *
 * @param mic_st The overall state of the audio driver.
 * @param[out] frames The buffer in which the core will receive microphone samples.
 * @param num_frames The size of \c frames, in samples.
 */
/* Room in the outgoing ring, in bytes, against the size asked for:
 * retro_spsc rounds capacity up to a power of two. */
static size_t microphone_outgoing_room(const retro_microphone_t *microphone)
{
   size_t room   = retro_spsc_write_avail(&microphone->outgoing_samples);
   size_t excess = microphone->outgoing_samples.capacity - microphone->outgoing_size;
   return room > excess ? room - excess : 0;
}

static size_t microphone_driver_flush(
      microphone_driver_state_t *mic_st,
      retro_microphone_t *microphone,
      size_t num_frames)
{
   struct resampler_data resampler_data;
   /* The worker's snapshot when it is running, so the capture path does
    * not read ::flags on a thread that does not own it; the
    * frame-synchronous path derives it as before. The sample size and
    * the USE_FLOAT bit say the same thing - the device's format, fixed
    * at open - so every test below asks this rather than the flags word
    * the main thread keeps writing. */
#ifdef HAVE_THREADS
   unsigned sample_size = microphone->worker_sample_size
         ? microphone->worker_sample_size
         : mic_driver_get_sample_size(microphone);
#else
   /* No worker without threads; the frame-synchronous path derives it. */
   unsigned sample_size = mic_driver_get_sample_size(microphone);
#endif
   size_t bytes_to_read = MIN(mic_st->input_frames_length, num_frames * sample_size);
   size_t frames_to_enqueue;
   int bytes_read       = mic_st->driver->read(
         mic_st->driver_context,
         microphone->microphone_context,
         mic_st->input_frames,
         bytes_to_read);
   /* First, get the most recent mic data */

   if (bytes_read <= 0)
      return 0;

   /* A driver must not report more than it was asked for.  Nothing else
    * validates this, and every buffer below is sized from the request, so
    * an over-report would run the dual-mono up-channel and the resampler
    * off the end of their allocations. */
   if ((size_t)bytes_read > bytes_to_read)
      bytes_read = (int)bytes_to_read;

   resampler_data.input_frames = bytes_read / sample_size;
   /* This is in frames, not samples or bytes;
    * we're up-channeling the audio to stereo,
    * so this number still applies. */

   resampler_data.output_frames = 0;
   /* The resampler sets the value of output_frames */

   resampler_data.data_in  = mic_st->dual_mono_frames;
   resampler_data.data_out = mic_st->resampled_frames;
   /* The buffers that will be used for the resampler's input and output */

   resampler_data.ratio    = (double)microphone->effective_params.rate / (double)microphone->actual_params.rate;

   if (fabs(resampler_data.ratio - 1.0f) < 1e-8)
   { /* If the mic's native rate is practically the same as the requested one... */

      /* ...then skip the resampler, since it'll produce (more or less) identical results. */
      frames_to_enqueue = MIN(microphone_outgoing_room(microphone) / sizeof(int16_t), resampler_data.input_frames);

      /* If this mic provides floating-point samples... */
      if (sample_size == sizeof(float))
      {
         convert_float_to_s16(mic_st->final_frames, (const float*)mic_st->input_frames, resampler_data.input_frames);
         retro_spsc_write(&microphone->outgoing_samples, mic_st->final_frames, frames_to_enqueue * sizeof(int16_t));
      }
      else
         retro_spsc_write(&microphone->outgoing_samples, mic_st->input_frames, frames_to_enqueue * sizeof(int16_t));

      return frames_to_enqueue;
   }
   /* Couldn't take the fast path, so let's resample the mic input */

   /* Deterministic integer path: the device gave us int16 and the selected
    * resampler has an integer implementation, so the whole flush stays in
    * the integer domain. Structurally identical to the float path below --
    * duplicate mono into interleaved stereo, resample, take the left
    * channel -- but without the s16->float->s16 round-trip, which is pure
    * requantisation on a signal that starts and ends as int16. */
   if (     !(sample_size == sizeof(float))
         &&   microphone->resampler_data_int16
         &&   microphone->resampler_int16_process)
   {
      struct resampler_data_int16 s16;
      const int16_t *src = (const int16_t*)mic_st->input_frames;
      int16_t       *dst = mic_st->dual_mono_frames_int16;
      size_t         n;

      for (n = 0; n < resampler_data.input_frames; n++)
      {
         dst[(n << 1)    ] = src[n];
         dst[(n << 1) + 1] = src[n];
      }

      s16.data_in       = dst;
      s16.data_out      = mic_st->resampled_frames_int16;
      s16.input_frames  = resampler_data.input_frames;
      s16.output_frames = 0;
      /* Same unbounded-output contract as the game path: the driver emits
       * until its input runs out, with no reference to the destination
       * size. */
      s16.ratio         = audio_driver_bound_ratio(resampler_data.ratio,
            resampler_data.input_frames,
            MIN(mic_st->resampled_frames_int16_length
                     / (2 * sizeof(int16_t)),
                mic_st->final_frames_length / sizeof(int16_t)));
      microphone->resampler_int16_process(
            microphone->resampler_data_int16, &s16);

      /* Left channel only; both carry the same signal. */
      for (n = 0; n < s16.output_frames; n++)
         mic_st->final_frames[n] = mic_st->resampled_frames_int16[n << 1];

      frames_to_enqueue = MIN(
            microphone_outgoing_room(microphone) / sizeof(int16_t),
            s16.output_frames);
      retro_spsc_write(&microphone->outgoing_samples, mic_st->final_frames,
            frames_to_enqueue * sizeof(int16_t));
      return frames_to_enqueue;
   }

   /* First we need to format the input for the resampler. */
   /* If this mic provides floating-point samples... */
   if (sample_size == sizeof(float))
      /* Samples are already in floating-point, so we just need to up-channel them. */
      convert_to_dual_mono_float(mic_st->dual_mono_frames,
            (const float*)mic_st->input_frames, resampler_data.input_frames);
   else
   {
      /* Samples are 16-bit, so we need to convert them first. */
      convert_s16_to_float(mic_st->converted_input_frames, (const int16_t*)mic_st->input_frames, resampler_data.input_frames, 1.0f);
      convert_to_dual_mono_float(mic_st->dual_mono_frames, mic_st->converted_input_frames, resampler_data.input_frames);
   }

   /* Now we resample the mic data.  Bound the ratio first: the output is
    * narrowed to mono and then to int16 through three separate buffers, so
    * take the smallest capacity of the three. */
   resampler_data.ratio = audio_driver_bound_ratio(resampler_data.ratio,
         resampler_data.input_frames,
         MIN(MIN(mic_st->resampled_frames_length / (2 * sizeof(float)),
                 mic_st->resampled_mono_frames_length / sizeof(float)),
             mic_st->final_frames_length / sizeof(int16_t)));

   microphone->resampler->process(microphone->resampler_data, &resampler_data);

   /* Next, we convert the resampled data back to mono... */
   convert_to_mono_float_left(mic_st->resampled_mono_frames, mic_st->resampled_frames, resampler_data.output_frames);
   /* Why the left channel? No particular reason.
    * Left and right channels are the same in this case anyway. */

   /* Finally, we convert the audio back to 16-bit ints, as the mic interface requires. */
   convert_float_to_s16(mic_st->final_frames, mic_st->resampled_mono_frames, resampler_data.output_frames);

   frames_to_enqueue = MIN(microphone_outgoing_room(microphone) / sizeof(int16_t), resampler_data.output_frames);
   retro_spsc_write(&microphone->outgoing_samples, mic_st->final_frames, frames_to_enqueue * sizeof(int16_t));
   return frames_to_enqueue;
}

#ifdef HAVE_THREADS
/* The capture worker: block on the device, flush a slice, repeat.
 *
 * Everything expensive in the old read path happens here instead - the
 * blocking read, the dual-mono up-channel and the resampler - so the
 * core's retro_microphone_read() is left with a fifo read. That is the
 * same split the threaded playback pipeline makes, in the same place,
 * for the same reason: none of it belongs inside a frame.
 *
 * wait_readable() is bounded by contract, so a device that stops
 * delivering returns 0 and this loops back to check capture_running
 * rather than parking. */
static void microphone_driver_capture_thread(void *data)
{
   microphone_driver_state_t *mic_st = (microphone_driver_state_t*)data;
   retro_microphone_t *microphone    = &mic_st->microphone;

   while (retro_atomic_load_acquire_int(&microphone->capture_running))
   {
      size_t slice = AUDIO_CHUNK_SIZE_NONBLOCKING;
      unsigned sample_size;
      size_t room;

      if (     !mic_st->driver
            || !mic_st->driver->wait_readable
            || !microphone->outgoing_init)
         break;

      /* Do not read more than the ring can take, or the flush would
       * discard what it could not enqueue and the device would run
       * ahead of the core.  Producer-side query, no lock needed. */
      room = microphone_outgoing_room(microphone);

      sample_size = microphone->worker_sample_size;
      if (room < slice * sizeof(int16_t))
      {
         /* The core is not consuming; wait for it rather than spin.
          * Registered before the room is read again, so a read that
          * lands from here on either shows up in the re-check or makes
          * the commit return at once. */
         int key = retro_eventcount_prepare_wait(&microphone->capture_park);
         if (     microphone_outgoing_room(microphone) >= slice * sizeof(int16_t)
               || !retro_atomic_load_acquire_int(&microphone->capture_running))
            retro_eventcount_cancel_wait(&microphone->capture_park);
         else
            retro_eventcount_commit_wait_timeout(&microphone->capture_park,
                  key, 20000);
         continue;
      }

      /* What the device has now, which can be less than was asked for:
       * a driver returns short whenever its bounded wait expires with
       * something in hand, and alsa returns a period even when the
       * slice spans several. read() blocks until it has everything it
       * was asked for, so reading the whole slice on a short count
       * parks the worker inside the driver - and teardown joins this
       * thread unconditionally. Read what is there. */
      {
         size_t want = slice * sample_size;
         size_t got  = mic_st->driver->wait_readable(mic_st->driver_context,
               microphone->microphone_context, want);

         if (got < sample_size)
            continue;
         if (got < want)
            slice = got / sample_size;
      }

      /* The flush - device read, up-channel, resample, ring write -
       * runs without the lock: SPSC, this thread is the producer.  The
       * core's read no longer waits behind the resampler. */
      microphone_driver_flush(mic_st, microphone, slice);
      retro_eventcount_notify(&microphone->capture_park);
   }
}

/* True when this microphone is being served by the worker. */
static bool microphone_driver_capture_threaded(
      const microphone_driver_state_t *mic_st,
      const retro_microphone_t *microphone)
{
   return microphone->capture_thread != NULL;
}
#endif

int microphone_driver_read(retro_microphone_t *microphone, int16_t* frames, size_t num_frames)
{
   uint32_t runloop_flags            = runloop_get_flags();
   size_t frames_remaining           = num_frames;
   microphone_driver_state_t *mic_st = &mic_driver_st;
   const microphone_driver_t *driver = mic_st->driver;
   bool core_paused                  = (runloop_flags & RUNLOOP_FLAG_PAUSED)           ? true : false;
   bool is_fastforward               = (runloop_flags & RUNLOOP_FLAG_FASTMOTION)       ? true : false;
   bool is_slowmo                    = (runloop_flags & RUNLOOP_FLAG_SLOWMOTION)       ? true : false;
   bool is_rewind                    = state_manager_frame_is_reversed();
   bool driver_active                = (mic_st->flags & MICROPHONE_DRIVER_FLAG_ACTIVE) ? true : false;

   /* If the provided arguments aren't valid... */
   if (!frames || !microphone)
      return -1;

   /* If the microphone or driver aren't active... */
   if (!driver_active || !(microphone->flags & MICROPHONE_FLAG_ACTIVE))
      return -1;

   /* If the driver is invalid or doesn't have the functions it needs... */
   if (!driver || !driver->read || !driver->mic_alive)
      return -1;

   /* If the core didn't actually ask for any frames... */
   if (num_frames == 0)
      return 0;

   if (   (microphone->flags & MICROPHONE_FLAG_PENDING)
      ||  (microphone->flags & MICROPHONE_FLAG_SUSPENDED)
      || !(microphone->flags & MICROPHONE_FLAG_ENABLED)
      || is_fastforward
      || is_slowmo
      || is_rewind
      || core_paused
      )
   { /* If the microphone is pending, suspended, or disabled...
        ...or if the core is paused, in fast-forward, slow-mo, or rewind...*/
      memset(frames, 0, num_frames * sizeof(*frames));
      return (int)num_frames;
      /* ...then copy silence to the provided buffer. Not an error if the mic is pending,
       * because the user might have requested a microphone
       * before the driver could provide it. */
   }

   /* Why mute the mic when the core isn't running at standard speed?
    * Because I couldn't think of anything useful for the mic to do.
    * If you can, send a PR! */

   /* If the driver or microphone's state haven't been allocated... */
   if (!mic_st->driver_context || !microphone->microphone_context)
      return -1;

   /* If the mic isn't active like it should be at this point... */
   if (!driver->mic_alive(mic_st->driver_context, microphone->microphone_context))
   {
      RARCH_ERR("[Microphone] Mic frontend has the mic enabled, but the backend has it disabled.\n");
      return -1;
   }

   /* If the core asked for more frames than the ring can hold... */
   if (num_frames * sizeof(int16_t) > microphone->outgoing_size)
      return -1;

   retro_assert(mic_st->input_frames != NULL);

#ifdef HAVE_THREADS
   if (microphone_driver_capture_threaded(mic_st, microphone))
   {
      /* The worker is doing the reading, the up-channelling and the
       * resampling; all that is left here is to take what it has, and
       * to wait a bounded moment if it has not caught up. Silence is
       * the same answer the synchronous path gives for a device that
       * will not deliver, arrived at without blocking the frame on it. */
      size_t want = num_frames * sizeof(int16_t);
      size_t got;

      if (retro_spsc_read_avail(&microphone->outgoing_samples) < want)
      {
         int key = retro_eventcount_prepare_wait(&microphone->capture_park);
         /* One bounded wait, not a loop: a frame does not get held for
          * a device that will not deliver, and silence below is the
          * same answer the synchronous path gives. */
         if (retro_spsc_read_avail(&microphone->outgoing_samples) >= want)
            retro_eventcount_cancel_wait(&microphone->capture_park);
         else
            retro_eventcount_commit_wait_timeout(&microphone->capture_park,
                  key, 10000);
      }
      /* Consumer side, no lock: SPSC. */
      got = retro_spsc_read(&microphone->outgoing_samples, frames, want);
      retro_eventcount_notify(&microphone->capture_park);

      if (got < want)
         memset((uint8_t*)frames + got, 0, want - got);
      return (int)num_frames;
   }
#endif

   {
      unsigned stall_count = 0;
      while (retro_spsc_read_avail(&microphone->outgoing_samples) < num_frames * sizeof(int16_t))
      { /* Until we can give the core the frames it asked for... */
         size_t frames_to_read = MIN(AUDIO_CHUNK_SIZE_NONBLOCKING, frames_remaining);
         size_t frames_read    = microphone_driver_flush(
               mic_st, microphone, frames_to_read);
         if (frames_read == 0)
         {
            if (++stall_count > 512)
            { /* Driver isn't providing data; return silence */
               memset(frames, 0, num_frames * sizeof(int16_t));
               return (int)num_frames;
            }
         }
         else
            stall_count = 0;
         frames_remaining -= frames_read;
      } /* If the queue already has enough samples to give, the loop will be skipped */
   }

   retro_spsc_read(&microphone->outgoing_samples, frames, num_frames * sizeof(int16_t));
   return (int)num_frames;
}

bool microphone_driver_get_effective_params(const retro_microphone_t *microphone, retro_microphone_params_t *params)
{
   /* If the arguments are null... */
   if (!microphone || !params)
      return false;
   /* If this isn't an opened microphone... */
   if (!(microphone->flags & MICROPHONE_FLAG_ACTIVE))
      return false;
   *params = microphone->effective_params;
   return true;
}

/* NOTE: The core may request a microphone before the driver is ready.
 * A pending handle will be provided in that case, and the frontend will
 * initialize the microphone when the time is right;
 * do not call this function twice on the same mic. */
retro_microphone_t *microphone_driver_open_mic(const retro_microphone_params_t *params)
{
   microphone_driver_state_t *mic_st     = &mic_driver_st;
   const settings_t *settings            = config_get_ptr();
   const microphone_driver_t *mic_driver = mic_st->driver;
   void *driver_context                  = mic_st->driver_context;

   if (!settings)
      return NULL;

   /* Not checking mic_st->flags because they might not be set yet;
    * don't forget, the core can ask for a mic
    * before the audio driver is ready to create one. */
   if (!settings->bools.microphone_enable)
   {
      RARCH_DBG("[Microphone] Refused to open microphone because it's disabled in the settings.\n");
      return NULL;
   }

   if (mic_driver == &microphone_null)
   {
      RARCH_WARN("[Microphone] Cannot open microphone, null driver is configured.\n");
      return NULL;
   }

   if (        !mic_driver
            && (string_is_equal(settings->arrays.microphone_driver, "null")
            || !*settings->arrays.microphone_driver))
   { /* If the mic driver hasn't been initialized, but it's not going to be... */
      RARCH_ERR("[Microphone] Cannot open microphone as the driver won't be initialized.\n");
      return NULL;
   }

   /* If the core has requested a second microphone... */
   if (mic_st->microphone.flags & MICROPHONE_FLAG_ACTIVE)
   {
      RARCH_ERR("[Microphone] Failed to open a second microphone, frontend only supports one at a time right now.\n");
      if (mic_st->microphone.flags & MICROPHONE_FLAG_PENDING)
         /* If that mic is pending... */
         RARCH_ERR("[Microphone] A microphone is pending initialization.\n");
      else
         /* That mic is initialized */
         RARCH_ERR("[Microphone] An initialized microphone exists.\n");

      return NULL;
   }

   /* Cores might ask for a microphone before the audio driver is ready to provide them;
    * if that happens, we have to initialize the microphones later.
    * But the user still wants a handle, so we'll give them one.
    */
   mic_driver_microphone_handle_init(&mic_st->microphone, params);

   /* If driver_context is NULL, the handle won't have
    * a valid microphone context (but we'll create one later) */
   if (driver_context)
   {
      /* If the microphone driver is ready to open a microphone... */
      if (mic_driver_open_mic_internal(&mic_st->microphone)) /* If the microphone was successfully initialized... */
         RARCH_LOG("[Microphone] Opened the requested microphone successfully.\n");
      else
         goto error;
   }
   else
   { /* If the driver isn't ready to create a microphone... */
      mic_st->microphone.flags |= MICROPHONE_FLAG_PENDING;
      RARCH_LOG("[Microphone] Microphone requested before driver context was ready; deferring initialization.\n");
   }

   return &mic_st->microphone;
error:
   mic_driver_microphone_handle_free(&mic_st->microphone, false);
   /* This function cleans up any resources and unsets all flags */

   return NULL;
}

static bool microphone_driver_free_devices_list(void)
{
   microphone_driver_state_t *mic_st = &mic_driver_st;
   const microphone_driver_t *mic    = mic_st->devices_list_driver;
   if (!mic_st->devices_list)
      return false;
   /* Released by the driver that built it, with no context: none of
    * the microphone drivers' frees read one, and the context it was
    * built with may be gone. */
   if (mic && mic->device_list_free)
      mic->device_list_free(NULL, mic_st->devices_list);
   else
      string_list_free(mic_st->devices_list);
   mic_st->devices_list        = NULL;
   mic_st->devices_list_driver = NULL;
   return true;
}

/* The driver whose device_list_new to call, as for audio: the running
 * driver when it is the one configured, the configured driver with no
 * context otherwise. The list was built once at init, only when the
 * driver had opened, and always from the running driver: a driver
 * picked in the menu showed the old driver's devices until a restart,
 * and one that failed to open showed nothing to pick instead. */
static const microphone_driver_t *microphone_driver_enumeration_driver(
      microphone_driver_state_t *mic_st, const void **ctx)
{
   settings_t *settings           = config_get_ptr();
   const microphone_driver_t *mic = mic_st->driver;
   *ctx                           = mic_st->driver_context;
   if (     mic && mic_st->driver_context
         && string_is_equal(mic->ident, settings->arrays.microphone_driver))
      return mic;
   {
      int i = (int)driver_find_index("microphone_driver",
            settings->arrays.microphone_driver);
      *ctx  = NULL;
      if (i >= 0)
         return microphone_drivers[i];
   }
   return NULL;
}

void microphone_driver_refresh_devices_list(void)
{
   microphone_driver_state_t *mic_st = &mic_driver_st;
   const void *ctx                   = NULL;
   const microphone_driver_t *mic;
   microphone_driver_free_devices_list();
   mic = microphone_driver_enumeration_driver(mic_st, &ctx);
   if (mic && mic->device_list_new)
   {
      mic_st->devices_list        = mic->device_list_new(ctx);
      mic_st->devices_list_driver = mic;
   }
}

bool microphone_driver_deinit(bool is_reset)
{
   microphone_driver_state_t *mic_st = &mic_driver_st;
   const microphone_driver_t *mic    = mic_st->driver;

   microphone_driver_free_devices_list();
   microphone_driver_close_mic_internal(&mic_st->microphone, is_reset);

   if (mic && mic->free)
   {
      if (mic_st->driver_context)
         mic->free(mic_st->driver_context);

      mic_st->driver_context = NULL;
   }

   /* All staging buffers are views into the two arenas. */
   if (mic_st->arena_int16)
      memalign_free(mic_st->arena_int16);
   mic_st->arena_int16                   = NULL;
   mic_st->dual_mono_frames_int16        = NULL;
   mic_st->dual_mono_frames_int16_length = 0;
   mic_st->resampled_frames_int16        = NULL;
   mic_st->resampled_frames_int16_length = 0;
   mic_st->final_frames                  = NULL;
   mic_st->final_frames_length           = 0;

   if (mic_st->arena_float)
      memalign_free(mic_st->arena_float);
   mic_st->arena_float                   = NULL;
   mic_st->input_frames                  = NULL;
   mic_st->input_frames_length           = 0;
   mic_st->converted_input_frames        = NULL;
   mic_st->converted_input_frames_length = 0;
   mic_st->dual_mono_frames              = NULL;
   mic_st->dual_mono_frames_length       = 0;
   mic_st->resampled_frames              = NULL;
   mic_st->resampled_frames_length       = 0;
   mic_st->resampled_mono_frames         = NULL;
   mic_st->resampled_mono_frames_length  = 0;

   mic_st->resampler_quality  = RESAMPLER_QUALITY_DONTCARE;
   mic_st->flags             &= ~MICROPHONE_DRIVER_FLAG_ACTIVE;
   memset(mic_st->resampler_ident, '\0', sizeof(mic_st->resampler_ident));

   return true;
}

bool microphone_driver_get_devices_list(void **data)
{
   struct string_list**ptr     = (struct string_list**)data;
   if (!ptr)
      return false;
   *ptr = mic_driver_st.devices_list;
   return true;
}
#endif

/* Keep optional startup work out of the common initialization body. */
#if defined(__GNUC__)
__attribute__((noinline))
#elif defined(_MSC_VER)
__declspec(noinline)
#endif
static bool audio_driver_transport_configure(const settings_t *settings)
{
#ifdef HAVE_THREADS
   uint32_t tempo;
#endif
   uint32_t rate_bits;
   audio_driver_state_t *audio_st = &audio_driver_st;
   audio_st->transport_lpf_only   = !settings->bools.audio_time_stretch
      && settings->bools.audio_time_stretch_lowpass;
   if (!settings->bools.audio_time_stretch && !audio_st->transport_lpf_only)
      return true;
   memcpy(&rate_bits, &audio_st->input, sizeof(rate_bits));
   if (rate_bits >= UINT32_C(0x7f800000)
         || audio_st->input < 8000.0f || audio_st->input > 192000.0f)
      return false;
   if (!audio_st->pipe_threaded)
      return (audio_st->core_multi || audio_st->core_layout == AUDIO_LAYOUT_STEREO)
         && audio_driver_inline_prepare(audio_st, audio_st->core_float,
               audio_st->core_multi ? AUDIO_PIPE_CANON_CHANNELS : 2);
#ifdef HAVE_THREADS
   /* The front pair drives one shared search for every native channel. */
   if (audio_driver_transport_runloop_tempo(&tempo))
      return audio_driver_pipeline_transport_start_runloop(
            (unsigned)audio_st->input, 3, settings->bools.audio_time_stretch_lowpass);
   if (audio_driver_pipeline_transport_prepare((unsigned)audio_st->input, 3))
   {
      audio_st->pipe_transport_follow = AUDIO_TRANSPORT_FOLLOW | AUDIO_TRANSPORT_UPDATE
         | (settings->bools.audio_time_stretch_lowpass ? AUDIO_TRANSPORT_LOWPASS : 0);
      return audio_driver_transport_recover(false, 65536);
   }
#endif
   return false;
}
