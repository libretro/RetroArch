/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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


#include "driver.h"
#include "general.h"
#include "file.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "compat/posix_string.h"
#include "audio/utils.h"
#include "audio/resampler.h"
#include "gfx/thread_wrapper.h"
#include "audio/thread_wrapper.h"
#include "gfx/gfx_common.h"

#ifdef HAVE_X11
#include "gfx/context/x11_common.h"
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

static const audio_driver_t *audio_drivers[] = {
#ifdef HAVE_ALSA
   &audio_alsa,
#ifndef __QNX__
   &audio_alsathread,
#endif
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
#ifdef HAVE_SDL
   &audio_sdl,
#endif
#ifdef HAVE_XAUDIO
   &audio_xa,
#endif
#ifdef HAVE_DSOUND
   &audio_dsound,
#endif
#ifdef HAVE_PULSE
   &audio_pulse,
#endif
#ifdef __CELLOS_LV2__
   &audio_ps3,
#endif
#ifdef XENON
   &audio_xenon360,
#endif
#ifdef GEKKO
   &audio_gx,
#endif
#ifdef HAVE_NULLAUDIO
   &audio_null,
#endif
};

static const video_driver_t *video_drivers[] = {
#ifdef HAVE_OPENGL
   &video_gl,
#endif
#ifdef XENON
   &video_xenon360,
#endif
#if defined(_XBOX) && (defined(HAVE_D3D8) || defined(HAVE_D3D9))
   &video_xdk_d3d,
#endif
#if defined(HAVE_WIN32_D3D9)
   &video_d3d9,
#endif
#ifdef SN_TARGET_PSP2
   &video_vita,
#endif
//#ifdef PSP
   //&video_psp1,
//#endif
#ifdef HAVE_SDL
   &video_sdl,
#endif
#ifdef HAVE_XVIDEO
   &video_xvideo,
#endif
#ifdef GEKKO
   &video_gx,
#endif
#ifdef HAVE_VG
   &video_vg,
#endif
#ifdef HAVE_NULLVIDEO
   &video_null,
#endif
#ifdef HAVE_OMAP
   &video_omap,
#endif
};

static const input_driver_t *input_drivers[] = {
#ifdef __CELLOS_LV2__
   &input_ps3,
#endif
#if defined(SN_TARGET_PSP2) || defined(PSP)
   &input_psp,
#endif
#ifdef HAVE_SDL
   &input_sdl,
#endif
#ifdef HAVE_DINPUT
   &input_dinput,
#endif
#ifdef HAVE_X11
   &input_x,
#endif
#ifdef XENON
   &input_xenon360,
#endif
#if defined(HAVE_XINPUT2) || defined(HAVE_XINPUT_XBOX1)
   &input_xinput,
#endif
#ifdef GEKKO
   &input_gx,
#endif
#ifdef ANDROID
   &input_android,
#endif
#if defined(__linux__) && !defined(ANDROID)
   &input_linuxraw,
#endif
#if defined(IOS) || defined(OSX) //< Don't use __APPLE__ as it breaks basic SDL builds
   &input_apple,
#endif
#ifdef __BLACKBERRY_QNX__
   &input_qnx,
#endif
#ifdef HAVE_NULLINPUT
   &input_null,
#endif
};

static void find_audio_driver(void)
{
   for (unsigned i = 0; i < ARRAY_SIZE(audio_drivers); i++)
   {
      if (strcasecmp(g_settings.audio.driver, audio_drivers[i]->ident) == 0)
      {
         driver.audio = audio_drivers[i];
         return;
      }
   }
   RARCH_ERR("Couldn't find any audio driver named \"%s\"\n", g_settings.audio.driver);
   RARCH_LOG_OUTPUT("Available audio drivers are:\n");
   for (size_t i = 0; i < ARRAY_SIZE(audio_drivers); i++)
      RARCH_LOG_OUTPUT("\t%s\n", audio_drivers[i]->ident);

   rarch_fail(1, "find_audio_driver()");
}

static void find_video_driver(void)
{
   for (unsigned i = 0; i < ARRAY_SIZE(video_drivers); i++)
   {
      if (strcasecmp(g_settings.video.driver, video_drivers[i]->ident) == 0)
      {
         driver.video = video_drivers[i];
         return;
      }
   }
   RARCH_ERR("Couldn't find any video driver named \"%s\"\n", g_settings.video.driver);
   RARCH_LOG_OUTPUT("Available video drivers are:\n");
   for (size_t i = 0; i < ARRAY_SIZE(video_drivers); i++)
      RARCH_LOG_OUTPUT("\t%s\n", video_drivers[i]->ident);

   rarch_fail(1, "find_video_driver()");
}

static void find_input_driver(void)
{
   for (unsigned i = 0; i < ARRAY_SIZE(input_drivers); i++)
   {
      if (strcasecmp(g_settings.input.driver, input_drivers[i]->ident) == 0)
      {
         driver.input = input_drivers[i];
         return;
      }
   }
   RARCH_ERR("Couldn't find any input driver named \"%s\"\n", g_settings.input.driver);
   RARCH_LOG_OUTPUT("Available input drivers are:\n");
   for (size_t i = 0; i < ARRAY_SIZE(input_drivers); i++)
      RARCH_LOG_OUTPUT("\t%s\n", input_drivers[i]->ident);

   rarch_fail(1, "find_input_driver()");
}

void init_drivers_pre(void)
{
   find_audio_driver();
   find_video_driver();
   find_input_driver();
}

static void adjust_system_rates(void)
{
   g_extern.system.force_nonblock = false;
   const struct retro_system_timing *info = &g_extern.system.av_info.timing;

   if (info->fps <= 0.0 || info->sample_rate <= 0.0)
      return;

   float timing_skew = fabs(1.0f - info->fps / g_settings.video.refresh_rate);
   if (timing_skew > 0.05f) // We don't want to adjust pitch too much. If we have extreme cases, just don't readjust at all.
   {
      RARCH_LOG("Timings deviate too much. Will not adjust. (Display = %.2f Hz, Game = %.2f Hz)\n",
            g_settings.video.refresh_rate,
            (float)info->fps);

      // We won't be able to do VSync reliably as game FPS > monitor FPS.
      if (info->fps > g_settings.video.refresh_rate)
      {
         g_extern.system.force_nonblock = true;
         RARCH_LOG("Game FPS > Monitor FPS. Cannot rely on VSync.\n");
      }

      g_settings.audio.in_rate = info->sample_rate;
   }
   else
      g_settings.audio.in_rate = info->sample_rate *
         (g_settings.video.refresh_rate / info->fps);

   RARCH_LOG("Set audio input rate to: %.2f Hz.\n", g_settings.audio.in_rate);

   if (driver.video_data)
   {
      if (g_extern.system.force_nonblock)
         video_set_nonblock_state_func(true);
      else
         driver_set_nonblock_state(driver.nonblock_state);
   }
}

void driver_set_monitor_refresh_rate(float hz)
{
   char msg[256];
   snprintf(msg, sizeof(msg), "Setting refresh rate to: %.3f Hz.", hz);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);
   RARCH_LOG("%s\n", msg);

   g_settings.video.refresh_rate = hz;
   adjust_system_rates();

   g_extern.audio_data.orig_src_ratio =
      g_extern.audio_data.src_ratio =
      (double)g_settings.audio.out_rate / g_settings.audio.in_rate;
}

void driver_set_nonblock_state(bool nonblock)
{
   // Only apply non-block-state for video if we're using vsync.
   if (g_extern.video_active && driver.video_data)
   {
      bool video_nb = nonblock;
      if (!g_settings.video.vsync || g_extern.system.force_nonblock)
         video_nb = true;
      video_set_nonblock_state_func(video_nb);
   }

   if (g_extern.audio_active && driver.audio_data)
      audio_set_nonblock_state_func(g_settings.audio.sync ? nonblock : true);

   g_extern.audio_data.chunk_size = nonblock ?
      g_extern.audio_data.nonblock_chunk_size : g_extern.audio_data.block_chunk_size;
}

uintptr_t driver_get_current_framebuffer(void)
{
#ifdef HAVE_FBO
   if (driver.video_poke && driver.video_poke->get_current_framebuffer)
      return driver.video_poke->get_current_framebuffer(driver.video_data);
   else
#endif
      return 0;
}

retro_proc_address_t driver_get_proc_address(const char *sym)
{
#ifdef HAVE_FBO
   if (driver.video_poke && driver.video_poke->get_proc_address)
      return driver.video_poke->get_proc_address(driver.video_data, sym);
   else
#endif
      return NULL;
}

// Only called once on init and deinit.
// Video and input drivers need to be active (owned)
// before retroarch core starts.
// Core handles audio.

// FIXME - start() and stop() can't be ifdeffable with
// RARCH_CONSOLE since more systems now need similar functionality

void global_init_drivers(void)
{
   init_drivers_pre(); // Set driver.* function callbacks.
#if defined(HAVE_RGUI) || defined(HAVE_RMENU)
   driver.video->start(); // Statically starts video driver. Sets driver.video_data.
#endif
   driver.input_data = driver.input->init();

#ifdef HAVE_OVERLAY
   if (*g_settings.input.overlay)
   {
      driver.overlay = input_overlay_new(g_settings.input.overlay);
      if (!driver.overlay)
	     RARCH_ERR("Failed to load overlay.\n");
   }
#endif
}

void global_uninit_drivers(void)
{
   if (driver.video_data)
   {
      driver.video_data = NULL;
   }

   if (driver.input_data)
   {
      driver.input->free(driver.input_data);
      driver.input_data = NULL;
   }
}

void init_drivers(void)
{
   driver.video_data_own = !driver.video_data;
   driver.audio_data_own = !driver.audio_data;
   driver.input_data_own = !driver.input_data;

   adjust_system_rates();

   g_extern.frame_count = 0;
   init_video_input();

   if (!driver.video_cache_context_ack && g_extern.system.hw_render_callback.context_reset)
      g_extern.system.hw_render_callback.context_reset();
   driver.video_cache_context_ack = false;

   init_audio();

   // Keep non-throttled state as good as possible.
   if (driver.nonblock_state)
      driver_set_nonblock_state(driver.nonblock_state);

   g_extern.system.frame_time_last = 0;
}

void uninit_drivers(void)
{
   uninit_audio();

   if (g_extern.system.hw_render_callback.context_destroy && !driver.video_cache_context)
      g_extern.system.hw_render_callback.context_destroy();

   uninit_video_input();

   if (driver.video_data_own)
      driver.video_data = NULL;
   if (driver.audio_data_own)
      driver.audio_data = NULL;
   if (driver.input_data_own)
      driver.input_data = NULL;

   driver.video_data_own = false;
   driver.audio_data_own = false;
   driver.input_data_own = false;
}

#ifdef HAVE_DYLIB
static void init_dsp_plugin(void)
{
   if (!(*g_settings.audio.dsp_plugin))
      return;

   rarch_dsp_info_t info = {0};

   g_extern.audio_data.dsp_lib = dylib_load(g_settings.audio.dsp_plugin);
   if (!g_extern.audio_data.dsp_lib)
   {
      RARCH_ERR("Failed to open DSP plugin: \"%s\" ...\n", g_settings.audio.dsp_plugin);
      return;
   }

   const rarch_dsp_plugin_t* (RARCH_API_CALLTYPE *plugin_init)(void) = 
      (const rarch_dsp_plugin_t *(RARCH_API_CALLTYPE*)(void))dylib_proc(g_extern.audio_data.dsp_lib, "rarch_dsp_plugin_init");

   if (!plugin_init)
   {
      RARCH_ERR("Failed to find symbol \"rarch_dsp_plugin_init\" in DSP plugin.\n");
      goto error;
   }

   g_extern.audio_data.dsp_plugin = plugin_init();
   if (!g_extern.audio_data.dsp_plugin)
   {
      RARCH_ERR("Failed to get a valid DSP plugin.\n");
      goto error;
   }

   if (g_extern.audio_data.dsp_plugin->api_version != RARCH_DSP_API_VERSION)
   {
      RARCH_ERR("DSP plugin API mismatch. RetroArch: %d, Plugin: %d\n", RARCH_DSP_API_VERSION, g_extern.audio_data.dsp_plugin->api_version);
      goto error;
   }

   RARCH_LOG("Loaded DSP plugin: \"%s\"\n", g_extern.audio_data.dsp_plugin->ident ? g_extern.audio_data.dsp_plugin->ident : "Unknown");

   info.input_rate = g_settings.audio.in_rate;

   g_extern.audio_data.dsp_handle = g_extern.audio_data.dsp_plugin->init(&info);
   if (!g_extern.audio_data.dsp_handle)
   {
      RARCH_ERR("Failed to init DSP plugin.\n");
      goto error;
   }

   return;

error:
   if (g_extern.audio_data.dsp_lib)
      dylib_close(g_extern.audio_data.dsp_lib);
   g_extern.audio_data.dsp_plugin = NULL;
   g_extern.audio_data.dsp_lib = NULL;
}

static void deinit_dsp_plugin(void)
{
   if (g_extern.audio_data.dsp_lib && g_extern.audio_data.dsp_plugin)
   {
      g_extern.audio_data.dsp_plugin->free(g_extern.audio_data.dsp_handle);
      dylib_close(g_extern.audio_data.dsp_lib);
   }
}
#endif

void init_audio(void)
{
   audio_convert_init_simd();

   // Resource leaks will follow if audio is initialized twice.
   if (driver.audio_data)
      return;

   // Accomodate rewind since at some point we might have two full buffers.
   size_t max_bufsamples = AUDIO_CHUNK_SIZE_NONBLOCKING * 2;
   size_t outsamples_max = max_bufsamples * AUDIO_MAX_RATIO * g_settings.slowmotion_ratio;

   // Used for recording even if audio isn't enabled.
   rarch_assert(g_extern.audio_data.conv_outsamples = (int16_t*)malloc(outsamples_max * sizeof(int16_t)));

   g_extern.audio_data.block_chunk_size    = AUDIO_CHUNK_SIZE_BLOCKING;
   g_extern.audio_data.nonblock_chunk_size = AUDIO_CHUNK_SIZE_NONBLOCKING;
   g_extern.audio_data.chunk_size          = g_extern.audio_data.block_chunk_size;

   // Needs to be able to hold full content of a full max_bufsamples in addition to its own.
   rarch_assert(g_extern.audio_data.rewind_buf = (int16_t*)malloc(max_bufsamples * sizeof(int16_t)));
   g_extern.audio_data.rewind_size             = max_bufsamples;

   if (!g_settings.audio.enable)
   {
      g_extern.audio_active = false;
      return;
   }

#ifdef HAVE_THREADS
   find_audio_driver();
   if (g_extern.system.audio_callback)
   {
      RARCH_LOG("Starting threaded audio driver ...\n");
      if (!rarch_threaded_audio_init(&driver.audio, &driver.audio_data,
               *g_settings.audio.device ? g_settings.audio.device : NULL,
               g_settings.audio.out_rate, g_settings.audio.latency,
               driver.audio))
      {
         RARCH_ERR("Cannot open threaded audio driver ... Exiting ...\n");
         rarch_fail(1, "init_audio()");
      }
   }
   else
#endif
   {
      driver.audio_data = audio_init_func(*g_settings.audio.device ? g_settings.audio.device : NULL,
            g_settings.audio.out_rate, g_settings.audio.latency);
   }

   if (!driver.audio_data)
   {
      RARCH_ERR("Failed to initialize audio driver. Will continue without audio.\n");
      g_extern.audio_active = false;
   }

   if (g_extern.audio_active && driver.audio->use_float && audio_use_float_func())
      g_extern.audio_data.use_float = true;

   if (!g_settings.audio.sync && g_extern.audio_active)
   {
      audio_set_nonblock_state_func(true);
      g_extern.audio_data.chunk_size = g_extern.audio_data.nonblock_chunk_size;
   }

   g_extern.audio_data.orig_src_ratio =
      g_extern.audio_data.src_ratio =
      (double)g_settings.audio.out_rate / g_settings.audio.in_rate;

   const char *resampler = *g_settings.audio.resampler ? g_settings.audio.resampler : NULL;
   if (!rarch_resampler_realloc(&g_extern.audio_data.resampler_data, &g_extern.audio_data.resampler,
         resampler, g_extern.audio_data.orig_src_ratio))
   {
      RARCH_ERR("Failed to initialize resampler \"%s\".\n", resampler ? resampler : "(default)");
      g_extern.audio_active = false;
   }

   rarch_assert(g_extern.audio_data.data = (float*)malloc(max_bufsamples * sizeof(float)));

   g_extern.audio_data.data_ptr = 0;

   rarch_assert(g_settings.audio.out_rate < g_settings.audio.in_rate * AUDIO_MAX_RATIO);
   rarch_assert(g_extern.audio_data.outsamples = (float*)malloc(outsamples_max * sizeof(float)));

   g_extern.audio_data.rate_control = false;
   if (!g_extern.system.audio_callback && g_extern.audio_active && g_settings.audio.rate_control)
   {
      if (driver.audio->buffer_size && driver.audio->write_avail)
      {
         g_extern.audio_data.driver_buffer_size = audio_buffer_size_func();
         g_extern.audio_data.rate_control = true;
      }
      else
         RARCH_WARN("Audio rate control was desired, but driver does not support needed features.\n");
   }

   g_extern.audio_data.volume_db   = g_settings.audio.volume;
   g_extern.audio_data.volume_gain = db_to_gain(g_settings.audio.volume);

#ifdef HAVE_DYLIB
   init_dsp_plugin();
#endif

   g_extern.measure_data.buffer_free_samples_count = 0;

   if (g_extern.audio_active && !g_extern.audio_data.mute && g_extern.system.audio_callback) // Threaded driver is initially stopped.
      audio_start_func();
}

static void compute_audio_buffer_statistics(void)
{
   unsigned samples = min(g_extern.measure_data.buffer_free_samples_count, AUDIO_BUFFER_FREE_SAMPLES_COUNT);
   if (samples < 3)
      return;

   uint64_t accum = 0;
   for (unsigned i = 1; i < samples; i++)
      accum += g_extern.measure_data.buffer_free_samples[i];

   int avg = accum / (samples - 1);

   uint64_t accum_var = 0;
   for (unsigned i = 1; i < samples; i++)
   {
      int diff = avg - g_extern.measure_data.buffer_free_samples[i];
      accum_var += diff * diff;
   }

   unsigned stddev = (unsigned)sqrt((double)accum_var / (samples - 2));

   float avg_filled = 1.0f - (float)avg / g_extern.audio_data.driver_buffer_size;
   float deviation = (float)stddev / g_extern.audio_data.driver_buffer_size;

   unsigned low_water_size = g_extern.audio_data.driver_buffer_size * 3 / 4;
   unsigned high_water_size = g_extern.audio_data.driver_buffer_size / 4;

   unsigned low_water_count = 0;
   unsigned high_water_count = 0;
   for (unsigned i = 1; i < samples; i++)
   {
      if (g_extern.measure_data.buffer_free_samples[i] >= low_water_size)
         low_water_count++;
      else if (g_extern.measure_data.buffer_free_samples[i] <= high_water_size)
         high_water_count++;
   }

   RARCH_LOG("Average audio buffer saturation: %.2f %%, standard deviation (percentage points): %.2f %%.\n",
         avg_filled * 100.0, deviation * 100.0);
   RARCH_LOG("Amount of time spent close to underrun: %.2f %%. Close to blocking: %.2f %%.\n",
         (100.0 * low_water_count) / (samples - 1),
         (100.0 * high_water_count) / (samples - 1));
}

bool driver_monitor_fps_statistics(double *refresh_rate, double *deviation, unsigned *sample_points)
{
   if (g_settings.video.threaded)
      return false;

   unsigned samples = min(MEASURE_FRAME_TIME_SAMPLES_COUNT, g_extern.measure_data.frame_time_samples_count);
   if (samples < 2)
      return false;

   // Measure statistics on frame time (microsecs), *not* FPS.
   rarch_time_t accum = 0;
   for (unsigned i = 0; i < samples; i++)
      accum += g_extern.measure_data.frame_time_samples[i];

#if 0
   for (unsigned i = 0; i < samples; i++)
      RARCH_LOG("Interval #%u: %d usec / frame.\n",
            i, (int)g_extern.measure_data.frame_time_samples[i]);
#endif

   rarch_time_t avg = accum / samples;
   rarch_time_t accum_var = 0;

   // Drop first measurement. It is likely to be bad.
   for (unsigned i = 0; i < samples; i++)
   {
      rarch_time_t diff = g_extern.measure_data.frame_time_samples[i] - avg;
      accum_var += diff * diff;
   }

   *deviation = sqrt((double)accum_var / (samples - 1)) / avg;
   *refresh_rate = 1000000.0 / avg;
   *sample_points = samples;

   return true;
}

static void compute_monitor_fps_statistics(void)
{
   if (g_settings.video.threaded)
   {
      RARCH_LOG("Monitor FPS estimation is disabled for threaded video.\n");
      return;
   }

   if (g_extern.measure_data.frame_time_samples_count < 2 * MEASURE_FRAME_TIME_SAMPLES_COUNT)
   {
      RARCH_LOG("Does not have enough samples for monitor refresh rate estimation. Requires to run for at least %u frames.\n",
            2 * MEASURE_FRAME_TIME_SAMPLES_COUNT);
      return;
   }

   double avg_fps = 0.0;
   double stddev = 0.0;
   unsigned samples = 0;
   if (driver_monitor_fps_statistics(&avg_fps, &stddev, &samples))
   {
      RARCH_LOG("Average monitor Hz: %.6f Hz. (%.3f %% frame time deviation, based on %u last samples).\n",
            avg_fps, 100.0 * stddev, samples);
   }
}

void uninit_audio(void)
{
   if (driver.audio_data && driver.audio)
      driver.audio->free(driver.audio_data);

   free(g_extern.audio_data.conv_outsamples);
   g_extern.audio_data.conv_outsamples = NULL;
   g_extern.audio_data.data_ptr        = 0;

   free(g_extern.audio_data.rewind_buf);
   g_extern.audio_data.rewind_buf = NULL;

   if (!g_settings.audio.enable)
   {
      g_extern.audio_active = false;
      return;
   }

   rarch_resampler_freep(&g_extern.audio_data.resampler, &g_extern.audio_data.resampler_data);

   free(g_extern.audio_data.data);
   g_extern.audio_data.data = NULL;

   free(g_extern.audio_data.outsamples);
   g_extern.audio_data.outsamples = NULL;

#ifdef HAVE_DYLIB
   deinit_dsp_plugin();
#endif

   compute_audio_buffer_statistics();
}

#ifdef HAVE_DYLIB
static void deinit_filter(void)
{
   g_extern.filter.active = false;

   if (g_extern.filter.lib)
      dylib_close(g_extern.filter.lib);
   g_extern.filter.lib = NULL;

   free(g_extern.filter.buffer);
   free(g_extern.filter.colormap);
   free(g_extern.filter.scaler_out);
   g_extern.filter.buffer     = NULL;
   g_extern.filter.colormap   = NULL;
   g_extern.filter.scaler_out = NULL;

   scaler_ctx_gen_reset(&g_extern.filter.scaler);
   memset(&g_extern.filter.scaler, 0, sizeof(g_extern.filter.scaler));
}

static void init_filter(bool rgb32)
{
   if (g_extern.filter.active)
      return;
   if (!*g_settings.video.filter_path)
      return;

   if (g_extern.system.hw_render_callback.context_type)
   {
      RARCH_WARN("Cannot use CPU filters when hardware rendering is used.\n");
      return;
   }

   RARCH_LOG("Loading bSNES filter from \"%s\"\n", g_settings.video.filter_path);
   g_extern.filter.lib = dylib_load(g_settings.video.filter_path);
   if (!g_extern.filter.lib)
   {
      RARCH_ERR("Failed to load filter \"%s\"\n", g_settings.video.filter_path);
      return;
   }

   struct retro_game_geometry *geom = &g_extern.system.av_info.geometry;
   unsigned width   = geom->max_width;
   unsigned height  = geom->max_height;
   unsigned pow2_x  = 0;
   unsigned pow2_y  = 0;
   unsigned maxsize = 0;

   g_extern.filter.psize = 
      (void (*)(unsigned*, unsigned*))dylib_proc(g_extern.filter.lib, "filter_size");
   g_extern.filter.prender = 
      (void (*)(uint32_t*, uint32_t*, 
                unsigned, const uint16_t*, 
                unsigned, unsigned, unsigned))dylib_proc(g_extern.filter.lib, "filter_render");

   if (!g_extern.filter.psize || !g_extern.filter.prender)
   {
      RARCH_ERR("Failed to find functions in filter...\n");
      goto error;
   }

   g_extern.filter.active = true;
   g_extern.filter.psize(&width, &height);

   pow2_x  = next_pow2(width);
   pow2_y  = next_pow2(height);
   maxsize = pow2_x > pow2_y ? pow2_x : pow2_y; 
   g_extern.filter.scale = maxsize / RARCH_SCALE_BASE;

   g_extern.filter.buffer = (uint32_t*)malloc(RARCH_SCALE_BASE * RARCH_SCALE_BASE *
         g_extern.filter.scale * g_extern.filter.scale * sizeof(uint32_t));
   if (!g_extern.filter.buffer)
      goto error;

   g_extern.filter.pitch = RARCH_SCALE_BASE * g_extern.filter.scale * sizeof(uint32_t);

   g_extern.filter.colormap = (uint32_t*)malloc(0x10000 * sizeof(uint32_t));
   if (!g_extern.filter.colormap)
      goto error;

   // Set up conversion map from 16-bit XRGB1555 to 32-bit ARGB.
   for (unsigned i = 0; i < 0x10000; i++)
   {
      unsigned r = (i >> 10) & 0x1f;
      unsigned g = (i >>  5) & 0x1f;
      unsigned b = (i >>  0) & 0x1f;

      r = (r << 3) | (r >> 2);
      g = (g << 3) | (g >> 2);
      b = (b << 3) | (b >> 2);
      g_extern.filter.colormap[i] = (r << 16) | (g << 8) | (b << 0);
   }

   g_extern.filter.scaler_out = (uint16_t*)malloc(sizeof(uint16_t) * geom->max_width * geom->max_height);
   if (!g_extern.filter.scaler_out)
      goto error;

   g_extern.filter.scaler.scaler_type = SCALER_TYPE_POINT;
   g_extern.filter.scaler.in_fmt      = rgb32 ? SCALER_FMT_ARGB8888 : SCALER_FMT_RGB565;
   g_extern.filter.scaler.out_fmt     = SCALER_FMT_0RGB1555;

   if (!scaler_ctx_gen_filter(&g_extern.filter.scaler))
      goto error;

   return;

error:
   RARCH_ERR("CPU filter init failed.\n");
   deinit_filter();
}
#endif

static void deinit_shader_dir(void)
{
   // It handles NULL, no worries :D
   dir_list_free(g_extern.shader_dir.list);
   g_extern.shader_dir.list = NULL;
   g_extern.shader_dir.ptr  = 0;
}

static void init_shader_dir(void)
{
   if (!*g_settings.video.shader_dir)
      return;

   g_extern.shader_dir.list = dir_list_new(g_settings.video.shader_dir, "shader|cg|cgp|glsl|glslp", false);
   if (!g_extern.shader_dir.list || g_extern.shader_dir.list->size == 0)
   {
      deinit_shader_dir();
      return;
   }

   g_extern.shader_dir.ptr  = 0;
   dir_list_sort(g_extern.shader_dir.list, false);

   for (unsigned i = 0; i < g_extern.shader_dir.list->size; i++)
      RARCH_LOG("Found shader \"%s\"\n", g_extern.shader_dir.list->elems[i].data);
}

static void deinit_pixel_converter(void)
{
   scaler_ctx_gen_reset(&driver.scaler);
   memset(&driver.scaler, 0, sizeof(driver.scaler));
   free(driver.scaler_out);
   driver.scaler_out = NULL;
}

static bool init_video_pixel_converter(unsigned size)
{
   // This function can be called multiple times without deiniting first on consoles.
   deinit_pixel_converter();

   if (g_extern.system.pix_fmt == RETRO_PIXEL_FORMAT_0RGB1555)
   {
      RARCH_WARN("0RGB1555 pixel format is deprecated, and will be slower. For 15/16-bit, RGB565 format is preferred.\n");

      driver.scaler.scaler_type = SCALER_TYPE_POINT;
      driver.scaler.in_fmt      = SCALER_FMT_0RGB1555;

      // TODO: Pick either ARGB8888 or RGB565 depending on driver ...
      driver.scaler.out_fmt     = SCALER_FMT_RGB565;

      if (!scaler_ctx_gen_filter(&driver.scaler))
         return false;

      driver.scaler_out = calloc(sizeof(uint16_t), size * size);
   }

   return true;
}

void init_video_input(void)
{
#ifdef HAVE_DYLIB
   init_filter(g_extern.system.pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888);
#endif

   init_shader_dir();

   const struct retro_game_geometry *geom = &g_extern.system.av_info.geometry;
   unsigned max_dim = max(geom->max_width, geom->max_height);
   unsigned scale = next_pow2(max_dim) / RARCH_SCALE_BASE;
   scale = max(scale, 1);

   if (g_extern.filter.active)
      scale = g_extern.filter.scale;

   g_extern.system.aspect_ratio = g_settings.video.aspect_ratio;

   if (g_extern.system.aspect_ratio < 0.0f)
   {
      if (geom->aspect_ratio > 0.0f && g_settings.video.aspect_ratio_auto)
         g_extern.system.aspect_ratio = geom->aspect_ratio;
      else
         g_extern.system.aspect_ratio = (float)geom->base_width / geom->base_height; // 1:1 PAR.

      RARCH_LOG("Adjusting aspect ratio to %.2f\n", g_extern.system.aspect_ratio);
   }

   unsigned width;
   unsigned height;
   if (g_settings.video.fullscreen)
   {
      width = g_settings.video.fullscreen_x;
      height = g_settings.video.fullscreen_y;
   }
   else
   {
      if (g_settings.video.force_aspect)
      {
         unsigned base_width = roundf(geom->base_height * g_extern.system.aspect_ratio); // Do rounding here to simplify integer scale correctness.
         width = roundf(base_width * g_settings.video.xscale);
         height = roundf(geom->base_height * g_settings.video.yscale);
      }
      else
      {
         width = roundf(geom->base_width * g_settings.video.xscale);
         height = roundf(geom->base_height * g_settings.video.yscale);
      }
   }

   if (width && height)
      RARCH_LOG("Video @ %ux%u\n", width, height);
   else
      RARCH_LOG("Video @ fullscreen\n");

   driver.display_type  = RARCH_DISPLAY_NONE;
   driver.video_display = 0;
   driver.video_window  = 0;

   if (!init_video_pixel_converter(RARCH_SCALE_BASE * scale))
   {
      RARCH_ERR("Failed to init pixel converter.\n");
      rarch_fail(1, "init_video_input()");
   }

   video_info_t video = {0};
   video.width = width;
   video.height = height;
   video.fullscreen = g_settings.video.fullscreen;
   video.vsync = g_settings.video.vsync && !g_extern.system.force_nonblock;
   video.force_aspect = g_settings.video.force_aspect;
   video.smooth = g_settings.video.smooth;
   video.input_scale = scale;
   video.rgb32 = g_extern.filter.active || (g_extern.system.pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888);

   const input_driver_t *tmp = driver.input;
#ifdef HAVE_THREADS
   if (g_settings.video.threaded && !g_extern.system.hw_render_callback.context_type) // Can't do hardware rendering with threaded driver currently.
   {
      find_video_driver(); // Need to grab the "real" video driver interface on a reinit.
      RARCH_LOG("Starting threaded video driver ...\n");
      if (!rarch_threaded_video_init(&driver.video, &driver.video_data,
               &driver.input, &driver.input_data,
               driver.video, &video))
      {
         RARCH_ERR("Cannot open threaded video driver ... Exiting ...\n");
         rarch_fail(1, "init_video_input()");
      }
   }
   else
#endif
      driver.video_data = video_init_func(&video, &driver.input, &driver.input_data);

   if (driver.video_data == NULL)
   {
      RARCH_ERR("Cannot open video driver ... Exiting ...\n");
      rarch_fail(1, "init_video_input()");
   }

   driver.video_poke = NULL;
   if (driver.video->poke_interface)
      driver.video->poke_interface(driver.video_data, &driver.video_poke);

   if (driver.video->set_rotation && g_extern.system.rotation)
      video_set_rotation_func(g_extern.system.rotation);

   if (driver.video_poke && driver.video_poke->set_aspect_ratio &&
         g_settings.video.aspect_ratio_idx != ASPECT_RATIO_CONFIG)
      driver.video_poke->set_aspect_ratio(driver.video_data, g_settings.video.aspect_ratio_idx);

#ifdef HAVE_X11
   if (driver.display_type == RARCH_DISPLAY_X11)
   {
      RARCH_LOG("Suspending screensaver (X11).\n");
      x11_suspend_screensaver(driver.video_window);
   }
#endif

   // Video driver didn't provide an input driver so we use configured one.
   if (driver.input == NULL)
   {
      RARCH_LOG("Graphics driver did not initialize an input driver. Attempting to pick a suitable driver.\n");
      driver.input = tmp;
      if (driver.input != NULL)
      {
         driver.input_data = input_init_func();
         if (driver.input_data == NULL)
         {
            RARCH_ERR("Cannot init input driver. Exiting ...\n");
            rarch_fail(1, "init_video_input()");
         }
      }
      else
      {
         RARCH_ERR("Cannot find input driver. Exiting ...\n");
         rarch_fail(1, "init_video_input()");
      }
   }

#ifdef HAVE_OVERLAY
   if (driver.overlay)
   {
      input_overlay_free(driver.overlay);
      driver.overlay = NULL;
   }

   if (*g_settings.input.overlay)
   {
      driver.overlay = input_overlay_new(g_settings.input.overlay);
      if (!driver.overlay)
         RARCH_ERR("Failed to load overlay.\n");
   }
#endif

   g_extern.measure_data.frame_time_samples_count = 0;
}

void uninit_video_input(void)
{
#ifdef HAVE_OVERLAY
   if (driver.overlay)
   {
      input_overlay_free(driver.overlay);
      driver.overlay = NULL;
      driver.overlay_state = 0;
   }
#endif

   if (driver.input_data != driver.video_data && driver.input)
      input_free_func();

   if (driver.video_data && driver.video)
      video_free_func();

   deinit_pixel_converter();

#ifdef HAVE_DYLIB
   deinit_filter();
#endif

   deinit_shader_dir();
   compute_monitor_fps_statistics();
}

driver_t driver;

