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

#include "boolean.h"
#include "libretro.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "driver.h"
#include "file.h"
#include "general.h"
#include "dynamic.h"
#include "performance.h"
#include "audio/utils.h"
#include "record/ffemu.h"
#include "rewind.h"
#include "movie.h"
#include "compat/strl.h"
#include "screenshot.h"
#include "cheats.h"
#include "compat/getopt_rarch.h"
#include "compat/posix_string.h"

#ifdef _WIN32
#ifdef _XBOX
#include <xtl.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "msvc/msvc_compat.h"
#endif

// To avoid continous switching if we hold the button down, we require that the button must go from pressed,
// unpressed back to pressed to be able to toggle between then.
static void check_fast_forward_button(void)
{
   bool new_button_state = input_key_pressed_func(RARCH_FAST_FORWARD_KEY);
   bool new_hold_button_state = input_key_pressed_func(RARCH_FAST_FORWARD_HOLD_KEY);
   static bool old_button_state = false;
   static bool old_hold_button_state = false;

   if (new_button_state && !old_button_state)
   {
      driver.nonblock_state = !driver.nonblock_state;
      driver_set_nonblock_state(driver.nonblock_state);
   }
   else if (old_hold_button_state != new_hold_button_state)
   {
      driver.nonblock_state = new_hold_button_state;
      driver_set_nonblock_state(driver.nonblock_state);
   }

   old_button_state = new_button_state;
   old_hold_button_state = new_hold_button_state;
}

#if defined(HAVE_SCREENSHOTS) && !defined(_XBOX1)
static bool take_screenshot_viewport(void)
{
   struct rarch_viewport vp = {0};
   video_viewport_info_func(&vp);

   if (!vp.width || !vp.height)
      return false;

   uint8_t *buffer = (uint8_t*)malloc(vp.width * vp.height * 3);
   if (!buffer)
      return false;

   if (!video_read_viewport_func(buffer))
   {
      free(buffer);
      return false;
   }

   const char *screenshot_dir = g_settings.screenshot_directory;
   char screenshot_path[PATH_MAX];
   if (!*g_settings.screenshot_directory)
   {
      fill_pathname_basedir(screenshot_path, g_extern.basename, sizeof(screenshot_path));
      screenshot_dir = screenshot_path;
   }

   // Data read from viewport is in bottom-up order, suitable for BMP.
   if (!screenshot_dump(screenshot_dir,
         buffer,
         vp.width, vp.height, vp.width * 3, true))
   {
      free(buffer);
      return false;
   }

   free(buffer);
   return true;
}

static bool take_screenshot_raw(void)
{
   const void *data = g_extern.frame_cache.data;
   unsigned width   = g_extern.frame_cache.width;
   unsigned height  = g_extern.frame_cache.height;
   int pitch        = g_extern.frame_cache.pitch;

   const char *screenshot_dir = g_settings.screenshot_directory;
   char screenshot_path[PATH_MAX];
   if (!*g_settings.screenshot_directory)
   {
      fill_pathname_basedir(screenshot_path, g_extern.basename, sizeof(screenshot_path));
      screenshot_dir = screenshot_path;
   }

   // Negative pitch is needed as screenshot takes bottom-up,
   // but we use top-down.
   return screenshot_dump(screenshot_dir,
         (const uint8_t*)data + (height - 1) * pitch, 
         width, height, -pitch, false);
}

void rarch_take_screenshot(void)
{
   if ((!*g_settings.screenshot_directory) && (!*g_extern.basename)) // No way to infer screenshot directory.
      return;

   bool ret = false;
   bool viewport_read = (g_settings.video.gpu_screenshot ||
         g_extern.system.hw_render_callback.context_type != RETRO_HW_CONTEXT_NONE) &&
      driver.video->read_viewport &&
      driver.video->viewport_info;

   // Clear out message queue to avoid OSD fonts to appear on screenshot.
   msg_queue_clear(g_extern.msg_queue);

   if (viewport_read)
   {
      // Avoid taking screenshot of GUI overlays.
      if (driver.video_poke && driver.video_poke->set_texture_enable)
         driver.video_poke->set_texture_enable(driver.video_data, false, false);
      if (driver.video)
         rarch_render_cached_frame();
   }

   if (viewport_read)
      ret = take_screenshot_viewport();
   else if (g_extern.frame_cache.data && (g_extern.frame_cache.data != RETRO_HW_FRAME_BUFFER_VALID))
      ret = take_screenshot_raw();
   else
      RARCH_ERR("Cannot take screenshot. GPU rendering is used and read_viewport is not supported.\n");

   const char *msg = NULL;
   if (ret)
   {
      RARCH_LOG("Taking screenshot.\n");
      msg = "Taking screenshot.";
   }
   else
   {
      RARCH_WARN("Failed to take screenshot ...\n");
      msg = "Failed to take screenshot.";
   }

   if (g_extern.is_paused)
   {
      msg_queue_push(g_extern.msg_queue, msg, 1, 1);
      rarch_render_cached_frame();
   }
   else
      msg_queue_push(g_extern.msg_queue, msg, 1, 180);
}
#endif

static void readjust_audio_input_rate(void)
{
   int avail = audio_write_avail_func();
   //RARCH_LOG_OUTPUT("Audio buffer is %u%% full\n",
   //      (unsigned)(100 - (avail * 100) / g_extern.audio_data.driver_buffer_size));

   unsigned write_index = g_extern.measure_data.buffer_free_samples_count++ & (AUDIO_BUFFER_FREE_SAMPLES_COUNT - 1);
   g_extern.measure_data.buffer_free_samples[write_index] = avail;

   int half_size = g_extern.audio_data.driver_buffer_size / 2;
   int delta_mid = avail - half_size;
   double direction = (double)delta_mid / half_size;

   double adjust = 1.0 + g_settings.audio.rate_control_delta * direction;

   g_extern.audio_data.src_ratio = g_extern.audio_data.orig_src_ratio * adjust;

   //RARCH_LOG_OUTPUT("New rate: %lf, Orig rate: %lf\n",
   //      g_extern.audio_data.src_ratio, g_extern.audio_data.orig_src_ratio);
}

#ifdef HAVE_FFMPEG
static void deinit_recording(void);

static void recording_dump_frame(const void *data, unsigned width, unsigned height, size_t pitch)
{
   struct ffemu_video_data ffemu_data = {0};

   if (g_extern.record_gpu_buffer)
   {
      struct rarch_viewport vp = {0};
      video_viewport_info_func(&vp);
      if (!vp.width || !vp.height)
      {
         RARCH_WARN("Viewport size calculation failed! Will continue using raw data. This will probably not work right ...\n");
         free(g_extern.record_gpu_buffer);
         g_extern.record_gpu_buffer = NULL;

         recording_dump_frame(data, width, height, pitch);
         return;
      }

      // User has resized. We're kinda fucked now.
      if (vp.width != g_extern.record_gpu_width || vp.height != g_extern.record_gpu_height)
      {
         static const char msg[] = "Recording terminated due to resize.";
         RARCH_WARN("%s\n", msg);
         msg_queue_clear(g_extern.msg_queue);
         msg_queue_push(g_extern.msg_queue, msg, 1, 180);

         deinit_recording();
         g_extern.recording = false;
         return;
      }

      // Big bottleneck.
      // Since we might need to do read-backs asynchronously, it might take 3-4 times
      // before this returns true ...
      if (!video_read_viewport_func(g_extern.record_gpu_buffer))
         return;

      ffemu_data.pitch  = g_extern.record_gpu_width * 3;
      ffemu_data.width  = g_extern.record_gpu_width;
      ffemu_data.height = g_extern.record_gpu_height;
      ffemu_data.data   = g_extern.record_gpu_buffer + (ffemu_data.height - 1) * ffemu_data.pitch;

      ffemu_data.pitch  = -ffemu_data.pitch;
   }
   else
   {
      ffemu_data.data    = data;
      ffemu_data.pitch   = pitch;
      ffemu_data.width   = width;
      ffemu_data.height  = height;
      ffemu_data.is_dupe = !data;
   }

   ffemu_push_video(g_extern.rec, &ffemu_data);
}
#endif

static void video_frame(const void *data, unsigned width, unsigned height, size_t pitch)
{
   if (!g_extern.video_active)
      return;

   g_extern.frame_cache.data   = data;
   g_extern.frame_cache.width  = width;
   g_extern.frame_cache.height = height;
   g_extern.frame_cache.pitch  = pitch;

   if (g_extern.system.pix_fmt == RETRO_PIXEL_FORMAT_0RGB1555 && data && data != RETRO_HW_FRAME_BUFFER_VALID)
   {
      RARCH_PERFORMANCE_INIT(video_frame_conv);
      RARCH_PERFORMANCE_START(video_frame_conv);
      driver.scaler.in_width = width;
      driver.scaler.in_height = height;
      driver.scaler.out_width = width;
      driver.scaler.out_height = height;
      driver.scaler.in_stride = pitch;
      driver.scaler.out_stride = width * sizeof(uint16_t);

      scaler_ctx_scale(&driver.scaler, driver.scaler_out, data);
      data = driver.scaler_out;
      pitch = driver.scaler.out_stride;
      RARCH_PERFORMANCE_STOP(video_frame_conv);
   }

   // Slightly messy code,
   // but we really need to do processing before blocking on VSync for best possible scheduling.
#ifdef HAVE_FFMPEG
   if (g_extern.recording && (!g_extern.filter.active || !g_settings.video.post_filter_record || !data || g_extern.record_gpu_buffer))
      recording_dump_frame(data, width, height, pitch);
#endif

   const char *msg = msg_queue_pull(g_extern.msg_queue);
   driver.current_msg = msg;

#ifdef HAVE_DYLIB
   if (g_extern.filter.active && data)
   {
      struct scaler_ctx *scaler = &g_extern.filter.scaler;
      scaler->in_width   = scaler->out_width = width;
      scaler->in_height  = scaler->out_height = height;
      scaler->in_stride  = pitch;
      scaler->out_stride = width * sizeof(uint16_t);

      scaler_ctx_scale(scaler, g_extern.filter.scaler_out, data);

      unsigned owidth = width;
      unsigned oheight = height;
      g_extern.filter.psize(&owidth, &oheight);
      g_extern.filter.prender(g_extern.filter.colormap, g_extern.filter.buffer, 
            g_extern.filter.pitch, g_extern.filter.scaler_out, scaler->out_stride, width, height);

#ifdef HAVE_FFMPEG
      if (g_extern.recording && g_settings.video.post_filter_record)
         recording_dump_frame(g_extern.filter.buffer, owidth, oheight, g_extern.filter.pitch);
#endif

      if (!video_frame_func(g_extern.filter.buffer, owidth, oheight, g_extern.filter.pitch, msg))
         g_extern.video_active = false;
   }
   else if (!video_frame_func(data, width, height, pitch, msg))
      g_extern.video_active = false;
#else
   if (!video_frame_func(data, width, height, pitch, msg))
      g_extern.video_active = false;
#endif
}

void rarch_render_cached_frame(void)
{
#ifdef HAVE_FFMPEG
   // Cannot allow FFmpeg recording when pushing duped frames.
   bool recording = g_extern.recording;
   g_extern.recording = false;
#endif

   const void *frame = g_extern.frame_cache.data;
   if (frame == RETRO_HW_FRAME_BUFFER_VALID)
      frame = NULL; // Dupe

   // Not 100% safe, since the library might have
   // freed the memory, but no known implementations do this :D
   // It would be really stupid at any rate ...
   video_frame(frame,
         g_extern.frame_cache.width,
         g_extern.frame_cache.height,
         g_extern.frame_cache.pitch);

#ifdef HAVE_FFMPEG
   g_extern.recording = recording;
#endif
}

static bool audio_flush(const int16_t *data, size_t samples)
{
#ifdef HAVE_FFMPEG
   if (g_extern.recording)
   {
      struct ffemu_audio_data ffemu_data = {0};
      ffemu_data.data                    = data;
      ffemu_data.frames                  = samples / 2;

      ffemu_push_audio(g_extern.rec, &ffemu_data);
   }
#endif

   if (g_extern.is_paused || g_extern.audio_data.mute)
      return true;
   if (!g_extern.audio_active)
      return false;

   const float *output_data = NULL;
   unsigned output_frames      = 0;

   struct resampler_data src_data = {0};
   RARCH_PERFORMANCE_INIT(audio_convert_s16);
   RARCH_PERFORMANCE_START(audio_convert_s16);
   audio_convert_s16_to_float(g_extern.audio_data.data, data, samples,
         g_extern.audio_data.volume_gain);
   RARCH_PERFORMANCE_STOP(audio_convert_s16);

#if defined(HAVE_DYLIB)
   rarch_dsp_output_t dsp_output = {0};
   rarch_dsp_input_t dsp_input   = {0};
   dsp_input.samples             = g_extern.audio_data.data;
   dsp_input.frames              = samples >> 1;

   if (g_extern.audio_data.dsp_plugin)
      g_extern.audio_data.dsp_plugin->process(g_extern.audio_data.dsp_handle, &dsp_output, &dsp_input);

   src_data.data_in      = dsp_output.samples ? dsp_output.samples : g_extern.audio_data.data;
   src_data.input_frames = dsp_output.samples ? dsp_output.frames : (samples >> 1);
#else
   src_data.data_in      = g_extern.audio_data.data;
   src_data.input_frames = samples >> 1;
#endif

   src_data.data_out = g_extern.audio_data.outsamples;

   if (g_extern.audio_data.rate_control)
      readjust_audio_input_rate();

   src_data.ratio = g_extern.audio_data.src_ratio;
   if (g_extern.is_slowmotion)
      src_data.ratio *= g_settings.slowmotion_ratio;

   RARCH_PERFORMANCE_INIT(resampler_proc);
   RARCH_PERFORMANCE_START(resampler_proc);
   rarch_resampler_process(g_extern.audio_data.resampler,
         g_extern.audio_data.resampler_data, &src_data);
   RARCH_PERFORMANCE_STOP(resampler_proc);

   output_data   = g_extern.audio_data.outsamples;
   output_frames = src_data.output_frames;

   if (g_extern.audio_data.use_float)
   {
      if (audio_write_func(output_data, output_frames * sizeof(float) * 2) < 0)
      {
         RARCH_ERR("Audio backend failed to write. Will continue without sound.\n");
         return false;
      }
   }
   else
   {
      RARCH_PERFORMANCE_INIT(audio_convert_float);
      RARCH_PERFORMANCE_START(audio_convert_float);
      audio_convert_float_to_s16(g_extern.audio_data.conv_outsamples,
            output_data, output_frames * 2);
      RARCH_PERFORMANCE_STOP(audio_convert_float);

      if (audio_write_func(g_extern.audio_data.conv_outsamples, output_frames * sizeof(int16_t) * 2) < 0)
      {
         RARCH_ERR("Audio backend failed to write. Will continue without sound.\n");
         return false;
      }
   }

   return true;
}

static void audio_sample_rewind(int16_t left, int16_t right)
{
   g_extern.audio_data.rewind_buf[--g_extern.audio_data.rewind_ptr] = right;
   g_extern.audio_data.rewind_buf[--g_extern.audio_data.rewind_ptr] = left;
}

size_t audio_sample_batch_rewind(const int16_t *data, size_t frames)
{
   size_t i, samples;

   samples = frames << 1;
   for (i = 0; i < samples; i++)
      g_extern.audio_data.rewind_buf[--g_extern.audio_data.rewind_ptr] = data[i];

   return frames;
}

static void audio_sample(int16_t left, int16_t right)
{
   g_extern.audio_data.conv_outsamples[g_extern.audio_data.data_ptr++] = left;
   g_extern.audio_data.conv_outsamples[g_extern.audio_data.data_ptr++] = right;

   if (g_extern.audio_data.data_ptr < g_extern.audio_data.chunk_size)
      return;

   g_extern.audio_active = audio_flush(g_extern.audio_data.conv_outsamples,
         g_extern.audio_data.data_ptr) && g_extern.audio_active;

   g_extern.audio_data.data_ptr = 0;
}

size_t audio_sample_batch(const int16_t *data, size_t frames)
{
   if (frames > (AUDIO_CHUNK_SIZE_NONBLOCKING >> 1))
      frames = AUDIO_CHUNK_SIZE_NONBLOCKING >> 1;

   g_extern.audio_active = audio_flush(data, frames << 1) && g_extern.audio_active;
   return frames;
}

#ifdef HAVE_OVERLAY
static inline void input_poll_overlay(void)
{
   memset(&driver.overlay_state, 0, sizeof(driver.overlay_state));

   unsigned device = input_overlay_full_screen(driver.overlay) ?
      RARCH_DEVICE_POINTER_SCREEN : RETRO_DEVICE_POINTER;

   bool polled = false;
   unsigned i, j;
   for (i = 0;
         input_input_state_func(NULL, 0, device, i, RETRO_DEVICE_ID_POINTER_PRESSED);
         i++)
   {
      int16_t x = input_input_state_func(NULL, 0,
            device, i, RETRO_DEVICE_ID_POINTER_X);
      int16_t y = input_input_state_func(NULL, 0,
            device, i, RETRO_DEVICE_ID_POINTER_Y);

      input_overlay_state_t polled_data;
      input_overlay_poll(driver.overlay, &polled_data, x, y);

      driver.overlay_state.buttons |= polled_data.buttons;

      for (j = 0; j < 4; j++)
         if (!driver.overlay_state.analog[j])
            driver.overlay_state.analog[j] = polled_data.analog[j];

      polled = true;
   }

   if (polled)
      input_overlay_post_poll(driver.overlay);
   else
      input_overlay_poll_clear(driver.overlay);
}
#endif

void rarch_input_poll(void)
{
   input_poll_func();

#ifdef HAVE_OVERLAY
   if (driver.overlay) // Poll overlay state
      input_poll_overlay();
#endif
}

#ifndef RARCH_CONSOLE
// Turbo scheme: If turbo button is held, all buttons pressed except for D-pad will go into
// a turbo mode. Until the button is released again, the input state will be modulated by a periodic pulse defined
// by the configured duty cycle.
static bool input_apply_turbo(unsigned port, unsigned id, bool res)
{
   if (res && g_extern.turbo_frame_enable[port])
      g_extern.turbo_enable[port] |= (1 << id);
   else if (!res)
      g_extern.turbo_enable[port] &= ~(1 << id);

   if (g_extern.turbo_enable[port] & (1 << id))
      return res && ((g_extern.turbo_count % g_settings.input.turbo_period) < g_settings.input.turbo_duty_cycle);
   else
      return res;
}
#endif

static int16_t input_state(unsigned port, unsigned device, unsigned index, unsigned id)
{
   device &= RETRO_DEVICE_MASK;

#ifdef HAVE_BSV_MOVIE
   if (g_extern.bsv.movie && g_extern.bsv.movie_playback)
   {
      int16_t ret;
      if (bsv_movie_get_input(g_extern.bsv.movie, &ret))
         return ret;
      else
         g_extern.bsv.movie_end = true;
   }
#endif

   static const struct retro_keybind *binds[MAX_PLAYERS] = {
      g_settings.input.binds[0],
      g_settings.input.binds[1],
      g_settings.input.binds[2],
      g_settings.input.binds[3],
      g_settings.input.binds[4],
      g_settings.input.binds[5],
      g_settings.input.binds[6],
      g_settings.input.binds[7],
   };

   int16_t res = 0;
   if (id < RARCH_FIRST_META_KEY || device == RETRO_DEVICE_KEYBOARD)
      res = input_input_state_func(binds, port, device, index, id);

#ifdef HAVE_OVERLAY
   if (device == RETRO_DEVICE_JOYPAD && port == 0)
      res |= driver.overlay_state.buttons & (UINT64_C(1) << id) ? 1 : 0;
   else if (device == RETRO_DEVICE_ANALOG && port == 0)
   {
      unsigned base = (index == RETRO_DEVICE_INDEX_ANALOG_RIGHT) ? 2 : 0;
      base += (id == RETRO_DEVICE_ID_ANALOG_Y) ? 1 : 0;
      res += driver.overlay_state.analog[base];
   }
#endif

#ifndef RARCH_CONSOLE
   // Don't allow turbo for D-pad.
   if (device == RETRO_DEVICE_JOYPAD && (id < RETRO_DEVICE_ID_JOYPAD_UP || id > RETRO_DEVICE_ID_JOYPAD_RIGHT))
      res = input_apply_turbo(port, id, res);
#endif

#ifdef HAVE_BSV_MOVIE
   if (g_extern.bsv.movie && !g_extern.bsv.movie_playback)
      bsv_movie_set_input(g_extern.bsv.movie, res);
#endif

   return res;
}

#ifdef _WIN32
#define RARCH_DEFAULT_CONF_PATH_STR "\n\t\tDefaults to retroarch.cfg in same directory as retroarch.exe.\n\t\tIf a default config is not found, RetroArch will attempt to create one."
#else
#ifndef GLOBAL_CONFIG_DIR
#define GLOBAL_CONFIG_DIR "/etc"
#endif
#define RARCH_DEFAULT_CONF_PATH_STR "\n\t\tBy default looks for config in $XDG_CONFIG_HOME/retroarch/retroarch.cfg,\n\t\t$HOME/.config/retroarch/retroarch.cfg,\n\t\tand $HOME/.retroarch.cfg.\n\t\tIf a default config is not found, RetroArch will attempt to create one based on the skeleton config (" GLOBAL_CONFIG_DIR "/retroarch.cfg)."
#endif

#include "config.features.h"

#define _PSUPP(var, name, desc) printf("\t%s:\n\t\t%s: %s\n", name, desc, _##var##_supp ? "yes" : "no")
static void print_features(void)
{
   puts("");
   puts("Features:");
   _PSUPP(sdl, "SDL", "SDL drivers");
   _PSUPP(thread, "Threads", "Threading support");
   _PSUPP(opengl, "OpenGL", "OpenGL driver");
   _PSUPP(kms, "KMS", "KMS/EGL context support");
   _PSUPP(udev, "UDEV", "UDEV/EVDEV input driver support");
   _PSUPP(egl, "EGL", "EGL context support");
   _PSUPP(vg, "OpenVG", "OpenVG output support");
   _PSUPP(xvideo, "XVideo", "XVideo output");
   _PSUPP(alsa, "ALSA", "audio driver");
   _PSUPP(oss, "OSS", "audio driver");
   _PSUPP(jack, "Jack", "audio driver");
   _PSUPP(rsound, "RSound", "audio driver");
   _PSUPP(roar, "RoarAudio", "audio driver");
   _PSUPP(pulse, "PulseAudio", "audio driver");
   _PSUPP(dsound, "DirectSound", "audio driver");
   _PSUPP(xaudio, "XAudio2", "audio driver");
   _PSUPP(zlib, "zlib", "PNG encode/decode and .zip extraction");
   _PSUPP(al, "OpenAL", "audio driver");
   _PSUPP(dylib, "External", "External filter and plugin support");
   _PSUPP(cg, "Cg", "Cg pixel shaders");
   _PSUPP(libxml2, "libxml2", "libxml2 XML parsing");
   _PSUPP(sdl_image, "SDL_image", "SDL_image image loading");
   _PSUPP(fbo, "FBO", "OpenGL render-to-texture (multi-pass shaders)");
   _PSUPP(dynamic, "Dynamic", "Dynamic run-time loading of libretro library");
   _PSUPP(ffmpeg, "FFmpeg", "On-the-fly recording of gameplay with libavcodec");
   _PSUPP(freetype, "FreeType", "TTF font rendering with FreeType");
   _PSUPP(netplay, "Netplay", "Peer-to-peer netplay");
   _PSUPP(python, "Python", "Script support in shaders");
}
#undef _PSUPP

static void print_compiler(FILE *file)
{
   fprintf(file, "\nCompiler: ");
#if defined(_MSC_VER)
   fprintf(file, "MSVC (%d) %u-bit\n", _MSC_VER, (unsigned)(CHAR_BIT * sizeof(size_t)));
#elif defined(__SNC__)
   fprintf(file, "SNC (%d) %u-bit\n",
      __SN_VER__, (unsigned)(CHAR_BIT * sizeof(size_t)));
#elif defined(_WIN32) && defined(__GNUC__)
   fprintf(file, "MinGW (%d.%d.%d) %u-bit\n",
      __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, (unsigned)(CHAR_BIT * sizeof(size_t)));
#elif defined(__clang__)
   fprintf(file, "Clang/LLVM (%s) %u-bit\n",
      __VERSION__, (unsigned)(CHAR_BIT * sizeof(size_t)));
#elif defined(__GNUC__)
   fprintf(file, "GCC (%d.%d.%d) %u-bit\n",
      __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, (unsigned)(CHAR_BIT * sizeof(size_t)));
#else
   fprintf(file, "Unknown compiler %u-bit\n",
      (unsigned)(CHAR_BIT * sizeof(size_t)));
#endif
   fprintf(file, "Built: %s\n", __DATE__);
}

static void print_help(void)
{
   puts("===================================================================");
   /* To get around error 'too many decimal points in number - expected ')' before numeric constant */
   puts("RetroArch: Frontend for libretro -- v" PACKAGE_VERSION " --");
   print_compiler(stdout);
   puts("===================================================================");
   puts("Usage: retroarch [rom file] [options...]");
   puts("\t-h/--help: Show this help message.");
   puts("\t--menu: Do not require ROM or libretro core to be loaded, starts directly in menu.");
   puts("\t\tIf no arguments are passed to RetroArch, it is equivalent to using --menu as only argument.");
   puts("\t--features: Prints available features compiled into RetroArch.");
   puts("\t-s/--save: Path for save file (*.srm).");
   puts("\t-f/--fullscreen: Start RetroArch in fullscreen regardless of config settings.");
   puts("\t-S/--savestate: Path to use for save states. If not selected, *.state will be assumed.");
   puts("\t-c/--config: Path for config file." RARCH_DEFAULT_CONF_PATH_STR);
   puts("\t--appendconfig: Extra config files are loaded in, and take priority over config selected in -c (or default).");
   puts("\t\tMultiple configs are delimited by ','.");
#ifdef HAVE_DYNAMIC
   puts("\t-L/--libretro: Path to libretro implementation. Overrides any config setting.");
#endif
   puts("\t-g/--gameboy: Path to Gameboy ROM. Load SuperGameBoy as the regular rom.");
   puts("\t-b/--bsx: Path to BSX rom. Load BSX BIOS as the regular rom.");
   puts("\t-B/--bsxslot: Path to BSX slotted rom. Load BSX BIOS as the regular rom.");
   puts("\t--sufamiA: Path to A slot of Sufami Turbo. Load Sufami base cart as regular rom.");
   puts("\t--sufamiB: Path to B slot of Sufami Turbo.");

   printf("\t-N/--nodevice: Disconnects controller device connected to port (1 to %d).\n", MAX_PLAYERS);
   printf("\t-A/--dualanalog: Connect a DualAnalog controller to port (1 to %d).\n", MAX_PLAYERS);
   printf("\t-m/--mouse: Connect a mouse into port of the device (1 to %d).\n", MAX_PLAYERS); 
   puts("\t-p/--scope: Connect a virtual SuperScope into port 2. (SNES specific).");
   puts("\t-j/--justifier: Connect a virtual Konami Justifier into port 2. (SNES specific).");
   puts("\t-J/--justifiers: Daisy chain two virtual Konami Justifiers into port 2. (SNES specific).");
   puts("\t-4/--multitap: Connect a SNES multitap to port 2. (SNES specific).");

#ifdef HAVE_BSV_MOVIE
   puts("\t-P/--bsvplay: Playback a BSV movie file.");
   puts("\t-R/--bsvrecord: Start recording a BSV movie file from the beginning.");
   puts("\t-M/--sram-mode: Takes an argument telling how SRAM should be handled in the session.");
#endif
   puts("\t\t{no,}load-{no,}save describes if SRAM should be loaded, and if SRAM should be saved.");
   puts("\t\tDo note that noload-save implies that save files will be deleted and overwritten.");

#ifdef HAVE_NETPLAY
   puts("\t-H/--host: Host netplay as player 1.");
   puts("\t-C/--connect: Connect to netplay as player 2.");
   puts("\t--port: Port used to netplay. Default is 55435.");
   puts("\t-F/--frames: Sync frames when using netplay.");
   puts("\t--spectate: Netplay will become spectating mode.");
   puts("\t\tHost can live stream the game content to players that connect.");
   puts("\t\tHowever, the client will not be able to play. Multiple clients can connect to the host.");
   puts("\t--nick: Picks a nickname for use with netplay. Not mandatory.");
#endif
#ifdef HAVE_NETWORK_CMD
   puts("\t--command: Sends a command over UDP to an already running RetroArch process.");
   puts("\t\tAvailable commands are listed if command is invalid.");
#endif

#ifdef HAVE_FFMPEG
   puts("\t-r/--record: Path to record video file.\n\t\tUsing .mkv extension is recommended.");
   puts("\t--recordconfig: Path to settings used during recording.");
   puts("\t--size: Overrides output video size when recording with FFmpeg (format: WIDTHxHEIGHT).");
#endif
   puts("\t-v/--verbose: Verbose logging.");
   puts("\t-U/--ups: Specifies path for UPS patch that will be applied to ROM.");
   puts("\t--bps: Specifies path for BPS patch that will be applied to ROM.");
   puts("\t--ips: Specifies path for IPS patch that will be applied to ROM.");
   puts("\t--no-patch: Disables all forms of rom patching.");
   puts("\t-X/--xml: Specifies path to XML memory map.");
   puts("\t-D/--detach: Detach RetroArch from the running console. Not relevant for all platforms.\n");
}

static void set_basename(const char *path)
{
   strlcpy(g_extern.fullpath, path, sizeof(g_extern.fullpath));

   strlcpy(g_extern.basename, path, sizeof(g_extern.basename));
   char *dst = strrchr(g_extern.basename, '.');
   if (dst)
      *dst = '\0';
}

static void set_paths(const char *path)
{
   set_basename(path);

   if (!g_extern.has_set_save_path)
      fill_pathname_noext(g_extern.savefile_name_srm, g_extern.basename, ".srm", sizeof(g_extern.savefile_name_srm));
   if (!g_extern.has_set_state_path)
      fill_pathname_noext(g_extern.savestate_name, g_extern.basename, ".state", sizeof(g_extern.savestate_name));

   if (path_is_directory(g_extern.savefile_name_srm))
   {
      fill_pathname_dir(g_extern.savefile_name_srm, g_extern.basename, ".srm", sizeof(g_extern.savefile_name_srm));
      RARCH_LOG("Redirecting save file to \"%s\".\n", g_extern.savefile_name_srm);
   }
   if (path_is_directory(g_extern.savestate_name))
   {
      fill_pathname_dir(g_extern.savestate_name, g_extern.basename, ".state", sizeof(g_extern.savestate_name));
      RARCH_LOG("Redirecting save state to \"%s\".\n", g_extern.savestate_name);
   }

   // If this is already set,
   // do not overwrite it as this was initialized before in a menu or otherwise.
   if (!*g_settings.system_directory)
      fill_pathname_basedir(g_settings.system_directory, path, sizeof(g_settings.system_directory));
}

static void parse_input(int argc, char *argv[])
{
   g_extern.libretro_no_rom = false;
   g_extern.libretro_dummy = false;
   g_extern.has_set_save_path = false;
   g_extern.has_set_state_path = false;
   g_extern.has_set_libretro = false;

   if (argc < 2)
   {
      g_extern.libretro_dummy = true;
      return;
   }

   // Make sure we can call parse_input several times ...
   optind = 0;

   int val = 0;

   const struct option opts[] = {
#ifdef HAVE_DYNAMIC
      { "libretro", 1, NULL, 'L' },
#endif
      { "menu", 0, &val, 'M' },
      { "help", 0, NULL, 'h' },
      { "save", 1, NULL, 's' },
      { "fullscreen", 0, NULL, 'f' },
#ifdef HAVE_FFMPEG
      { "record", 1, NULL, 'r' },
      { "recordconfig", 1, &val, 'R' },
      { "size", 1, &val, 's' },
#endif
      { "verbose", 0, NULL, 'v' },
      { "gameboy", 1, NULL, 'g' },
      { "config", 1, NULL, 'c' },
      { "appendconfig", 1, &val, 'C' },
      { "mouse", 1, NULL, 'm' },
      { "nodevice", 1, NULL, 'N' },
      { "scope", 0, NULL, 'p' },
      { "justifier", 0, NULL, 'j' },
      { "justifiers", 0, NULL, 'J' },
      { "dualanalog", 1, NULL, 'A' },
      { "savestate", 1, NULL, 'S' },
      { "bsx", 1, NULL, 'b' },
      { "bsxslot", 1, NULL, 'B' },
      { "multitap", 0, NULL, '4' },
      { "sufamiA", 1, NULL, 'Y' },
      { "sufamiB", 1, NULL, 'Z' },
#ifdef HAVE_BSV_MOVIE
      { "bsvplay", 1, NULL, 'P' },
      { "bsvrecord", 1, NULL, 'R' },
      { "sram-mode", 1, NULL, 'M' },
#endif
#ifdef HAVE_NETPLAY
      { "host", 0, NULL, 'H' },
      { "connect", 1, NULL, 'C' },
      { "frames", 1, NULL, 'F' },
      { "port", 1, &val, 'p' },
      { "spectate", 0, &val, 'S' },
      { "nick", 1, &val, 'N' },
#endif
#ifdef HAVE_NETWORK_CMD
      { "command", 1, &val, 'c' },
#endif
      { "ups", 1, NULL, 'U' },
      { "bps", 1, &val, 'B' },
      { "ips", 1, &val, 'I' },
      { "no-patch", 0, &val, 'n' },
      { "xml", 1, NULL, 'X' },
      { "detach", 0, NULL, 'D' },
      { "features", 0, &val, 'f' },
      { NULL, 0, NULL, 0 }
   };

#ifdef HAVE_FFMPEG
#define FFMPEG_RECORD_ARG "r:"
#else
#define FFMPEG_RECORD_ARG
#endif

#ifdef HAVE_DYNAMIC
#define DYNAMIC_ARG "L:"
#else
#define DYNAMIC_ARG
#endif

#ifdef HAVE_NETPLAY
#define NETPLAY_ARG "HC:F:"
#else
#define NETPLAY_ARG
#endif

#ifdef HAVE_BSV_MOVIE
#define BSV_MOVIE_ARG "P:R:M:"
#else
#define BSV_MOVIE_ARG
#endif

   const char *optstring = "hs:fvS:m:p4jJA:g:b:c:B:Y:Z:U:DN:X:" BSV_MOVIE_ARG NETPLAY_ARG DYNAMIC_ARG FFMPEG_RECORD_ARG;

   for (;;)
   {
      val = 0;
      int c = getopt_long(argc, argv, optstring, opts, NULL);
      int port;

      if (c == -1)
         break;


      switch (c)
      {
         case 'h':
            print_help();
            exit(0);

         case '4':
            g_settings.input.libretro_device[1] = RETRO_DEVICE_JOYPAD_MULTITAP;
            g_extern.has_set_libretro_device[1] = true;
            break;

         case 'j':
            g_settings.input.libretro_device[1] = RETRO_DEVICE_LIGHTGUN_JUSTIFIER;
            g_extern.has_set_libretro_device[1] = true;
            break;

         case 'J':
            g_settings.input.libretro_device[1] = RETRO_DEVICE_LIGHTGUN_JUSTIFIERS;
            g_extern.has_set_libretro_device[1] = true;
            break;

         case 'A':
            port = strtol(optarg, NULL, 0);
            if (port < 1 || port > MAX_PLAYERS)
            {
               RARCH_ERR("Connect dualanalog to a valid port.\n");
               print_help();
               rarch_fail(1, "parse_input()");
            }
            g_settings.input.libretro_device[port - 1] = RETRO_DEVICE_ANALOG;
            g_extern.has_set_libretro_device[port - 1] = true;
            break;

         case 's':
            strlcpy(g_extern.savefile_name_srm, optarg, sizeof(g_extern.savefile_name_srm));
            g_extern.has_set_save_path = true;
            break;

         case 'f':
            g_extern.force_fullscreen = true;
            break;

         case 'g':
            strlcpy(g_extern.gb_rom_path, optarg, sizeof(g_extern.gb_rom_path));
            g_extern.game_type = RARCH_CART_SGB;
            break;

         case 'b':
            strlcpy(g_extern.bsx_rom_path, optarg, sizeof(g_extern.bsx_rom_path));
            g_extern.game_type = RARCH_CART_BSX;
            break;

         case 'B':
            strlcpy(g_extern.bsx_rom_path, optarg, sizeof(g_extern.bsx_rom_path));
            g_extern.game_type = RARCH_CART_BSX_SLOTTED;
            break;

         case 'Y':
            strlcpy(g_extern.sufami_rom_path[0], optarg, sizeof(g_extern.sufami_rom_path[0]));
            g_extern.game_type = RARCH_CART_SUFAMI;
            break;

         case 'Z':
            strlcpy(g_extern.sufami_rom_path[1], optarg, sizeof(g_extern.sufami_rom_path[1]));
            g_extern.game_type = RARCH_CART_SUFAMI;
            break;

         case 'S':
            strlcpy(g_extern.savestate_name, optarg, sizeof(g_extern.savestate_name));
            g_extern.has_set_state_path = true;
            break;

         case 'v':
            g_extern.verbose = true;
            break;

         case 'm':
            port = strtol(optarg, NULL, 0);
            if (port < 1 || port > MAX_PLAYERS)
            {
               RARCH_ERR("Connect mouse to a valid port.\n");
               print_help();
               rarch_fail(1, "parse_input()");
            }
            g_settings.input.libretro_device[port - 1] = RETRO_DEVICE_MOUSE;
            g_extern.has_set_libretro_device[port - 1] = true;
            break;

         case 'N':
            port = strtol(optarg, NULL, 0);
            if (port < 1 || port > MAX_PLAYERS)
            {
               RARCH_ERR("Disconnect device from a valid port.\n");
               print_help();
               rarch_fail(1, "parse_input()");
            }
            g_settings.input.libretro_device[port - 1] = RETRO_DEVICE_NONE;
            g_extern.has_set_libretro_device[port - 1] = true;
            break;

         case 'p':
            g_settings.input.libretro_device[1] = RETRO_DEVICE_LIGHTGUN_SUPER_SCOPE;
            g_extern.has_set_libretro_device[1] = true;
            break;

         case 'c':
            strlcpy(g_extern.config_path, optarg, sizeof(g_extern.config_path));
            break;

#ifdef HAVE_FFMPEG
         case 'r':
            strlcpy(g_extern.record_path, optarg, sizeof(g_extern.record_path));
            g_extern.recording = true;
            break;
#endif

#ifdef HAVE_DYNAMIC
         case 'L':
            strlcpy(g_settings.libretro, optarg, sizeof(g_settings.libretro));
            g_extern.has_set_libretro = true;
            break;
#endif

#ifdef HAVE_BSV_MOVIE
         case 'P':
         case 'R':
            strlcpy(g_extern.bsv.movie_start_path, optarg,
                  sizeof(g_extern.bsv.movie_start_path));
            g_extern.bsv.movie_start_playback = c == 'P';
            g_extern.bsv.movie_start_recording = c == 'R';
            break;

         case 'M':
            if (strcmp(optarg, "noload-nosave") == 0)
            {
               g_extern.sram_load_disable = true;
               g_extern.sram_save_disable = true;
            }
            else if (strcmp(optarg, "noload-save") == 0)
               g_extern.sram_load_disable = true;
            else if (strcmp(optarg, "load-nosave") == 0)
               g_extern.sram_save_disable = true;
            else if (strcmp(optarg, "load-save") != 0)
            {
               RARCH_ERR("Invalid argument in --sram-mode.\n");
               print_help();
               rarch_fail(1, "parse_input()");
            }
            break;
#endif

#ifdef HAVE_NETPLAY
         case 'H':
            g_extern.netplay_enable = true;
            break;

         case 'C':
            g_extern.netplay_enable = true;
            strlcpy(g_extern.netplay_server, optarg, sizeof(g_extern.netplay_server));
            break;

         case 'F':
            g_extern.netplay_sync_frames = strtol(optarg, NULL, 0);
            break;
#endif

         case 'U':
            strlcpy(g_extern.ups_name, optarg, sizeof(g_extern.ups_name));
            g_extern.ups_pref = true;
            break;

         case 'X':
            strlcpy(g_extern.xml_name, optarg, sizeof(g_extern.xml_name));
            break;

         case 'D':
#if defined(_WIN32) && !defined(_XBOX)
            FreeConsole();
#endif
            break;

         case 0:
            switch (val)
            {
               case 'M':
                  g_extern.libretro_dummy = true;
                  break;

#ifdef HAVE_NETPLAY
               case 'p':
                  g_extern.netplay_port = strtoul(optarg, NULL, 0);
                  break;

               case 'S':
                  g_extern.netplay_is_spectate = true;
                  break;

               case 'N':
                  strlcpy(g_extern.netplay_nick, optarg, sizeof(g_extern.netplay_nick));
                  break;
#endif

#ifdef HAVE_NETWORK_CMD
               case 'c':
                  if (network_cmd_send(optarg))
                     exit(0);
                  else
                     rarch_fail(1, "network_cmd_send()");
                  break;
#endif

               case 'C':
                  strlcpy(g_extern.append_config_path, optarg, sizeof(g_extern.append_config_path));
                  break;

               case 'B':
                  strlcpy(g_extern.bps_name, optarg, sizeof(g_extern.bps_name));
                  g_extern.bps_pref = true;
                  break;

               case 'I':
                  strlcpy(g_extern.ips_name, optarg, sizeof(g_extern.ips_name));
                  g_extern.ips_pref = true;
                  break;

               case 'n':
                  g_extern.block_patch = true;
                  break;

#ifdef HAVE_FFMPEG
               case 's':
               {
                  errno = 0;
                  char *ptr;
                  g_extern.record_width = strtoul(optarg, &ptr, 0);
                  if ((*ptr != 'x') || errno)
                  {
                     RARCH_ERR("Wrong format for --size.\n");
                     print_help();
                     rarch_fail(1, "parse_input()");
                  }

                  ptr++;
                  g_extern.record_height = strtoul(ptr, &ptr, 0);
                  if ((*ptr != '\0') || errno)
                  {
                     RARCH_ERR("Wrong format for --size.\n");
                     print_help();
                     rarch_fail(1, "parse_input()");
                  }
                  break;
               }

               case 'R':
                  strlcpy(g_extern.record_config, optarg, sizeof(g_extern.record_config));
                  break;
#endif
               case 'f':
                  print_features();
                  exit(0);

               default:
                  break;
            }
            break;

         case '?':
            print_help();
            rarch_fail(1, "parse_input()");

         default:
            RARCH_ERR("Error parsing arguments.\n");
            rarch_fail(1, "parse_input()");
      }
   }

   if (g_extern.libretro_dummy)
   {
      if (optind < argc)
      {
         RARCH_ERR("--menu was used, but ROM file was passed as well.\n");
         rarch_fail(1, "parse_input()");
      }
   }
   else if (optind < argc)
      set_paths(argv[optind]);
   else
      g_extern.libretro_no_rom = true;

   // Copy SRM/state dirs used, so they can be reused on reentrancy.
   if (g_extern.has_set_save_path && path_is_directory(g_extern.savefile_name_srm))
      strlcpy(g_extern.savefile_dir, g_extern.savefile_name_srm, sizeof(g_extern.savefile_dir));
   if (g_extern.has_set_state_path && path_is_directory(g_extern.savestate_name))
      strlcpy(g_extern.savestate_dir, g_extern.savestate_name, sizeof(g_extern.savestate_dir));
}

static void init_controllers(void)
{
   unsigned i;
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      unsigned device = g_settings.input.libretro_device[i];

      // This is default, don't bother.
      if (device == RETRO_DEVICE_JOYPAD)
         continue;

      pretro_set_controller_port_device(i, device);

      switch (device)
      {
         case RETRO_DEVICE_NONE:
            RARCH_LOG("Disconnecting device from port %u.\n", i + 1);
            break;

         case RETRO_DEVICE_ANALOG:
            RARCH_LOG("Connecting dualanalog to port %u.\n", i + 1);
            break;

         case RETRO_DEVICE_MOUSE:
            RARCH_LOG("Connecting mouse to port %u.\n", i + 1);
            break;

         case RETRO_DEVICE_LIGHTGUN_JUSTIFIER:
            RARCH_LOG("Connecting Justifier to port %u.\n", i + 1);
            break;

         case RETRO_DEVICE_LIGHTGUN_JUSTIFIERS:
            RARCH_LOG("Connecting Justifiers to port %u.\n", i + 1);
            break;

         case RETRO_DEVICE_JOYPAD_MULTITAP:
            RARCH_LOG("Connecting Multitap to port %u.\n", i + 1);
            break;

         case RETRO_DEVICE_LIGHTGUN_SUPER_SCOPE:
            RARCH_LOG("Connecting scope to port %u.\n", i + 1);
            break;

         default:
            break;
      }
   }
}

static inline void load_save_files(void)
{
   switch (g_extern.game_type)
   {
      case RARCH_CART_NORMAL:
         load_ram_file(g_extern.savefile_name_srm, RETRO_MEMORY_SAVE_RAM);
         load_ram_file(g_extern.savefile_name_rtc, RETRO_MEMORY_RTC);
         break;

      case RARCH_CART_SGB:
         load_ram_file(g_extern.savefile_name_srm, RETRO_MEMORY_SNES_GAME_BOY_RAM);
         load_ram_file(g_extern.savefile_name_rtc, RETRO_MEMORY_SNES_GAME_BOY_RTC);
         break;

      case RARCH_CART_BSX:
      case RARCH_CART_BSX_SLOTTED:
         load_ram_file(g_extern.savefile_name_srm, RETRO_MEMORY_SNES_BSX_RAM);
         load_ram_file(g_extern.savefile_name_psrm, RETRO_MEMORY_SNES_BSX_PRAM);
         break;

      case RARCH_CART_SUFAMI:
         load_ram_file(g_extern.savefile_name_asrm, RETRO_MEMORY_SNES_SUFAMI_TURBO_A_RAM);
         load_ram_file(g_extern.savefile_name_bsrm, RETRO_MEMORY_SNES_SUFAMI_TURBO_B_RAM);
         break;

      default:
         break;
   }
}

static inline void save_files(void)
{
   switch (g_extern.game_type)
   {
      case RARCH_CART_NORMAL:
         RARCH_LOG("Saving regular SRAM.\n");
         RARCH_LOG("SRM: %s\n", g_extern.savefile_name_srm);
         RARCH_LOG("RTC: %s\n", g_extern.savefile_name_rtc);
         save_ram_file(g_extern.savefile_name_srm, RETRO_MEMORY_SAVE_RAM);
         save_ram_file(g_extern.savefile_name_rtc, RETRO_MEMORY_RTC);
         break;

      case RARCH_CART_SGB:
         RARCH_LOG("Saving Gameboy SRAM.\n");
         RARCH_LOG("SRM: %s\n", g_extern.savefile_name_srm);
         RARCH_LOG("RTC: %s\n", g_extern.savefile_name_rtc);
         save_ram_file(g_extern.savefile_name_srm, RETRO_MEMORY_SNES_GAME_BOY_RAM);
         save_ram_file(g_extern.savefile_name_rtc, RETRO_MEMORY_SNES_GAME_BOY_RTC);
         break;

      case RARCH_CART_BSX:
      case RARCH_CART_BSX_SLOTTED:
         RARCH_LOG("Saving BSX (P)RAM.\n");
         RARCH_LOG("SRM:  %s\n", g_extern.savefile_name_srm);
         RARCH_LOG("PSRM: %s\n", g_extern.savefile_name_psrm);
         save_ram_file(g_extern.savefile_name_srm, RETRO_MEMORY_SNES_BSX_RAM);
         save_ram_file(g_extern.savefile_name_psrm, RETRO_MEMORY_SNES_BSX_PRAM);
         break;

      case RARCH_CART_SUFAMI:
         RARCH_LOG("Saving Sufami turbo A/B RAM.\n");
         RARCH_LOG("ASRM: %s\n", g_extern.savefile_name_asrm);
         RARCH_LOG("BSRM: %s\n", g_extern.savefile_name_bsrm);
         save_ram_file(g_extern.savefile_name_asrm, RETRO_MEMORY_SNES_SUFAMI_TURBO_A_RAM);
         save_ram_file(g_extern.savefile_name_bsrm, RETRO_MEMORY_SNES_SUFAMI_TURBO_B_RAM);
         break;

      default:
         break;
   }
}


#ifdef HAVE_FFMPEG
static void init_recording(void)
{
   if (!g_extern.recording)
      return;

   if (!g_settings.video.gpu_record && g_extern.system.hw_render_callback.context_type)
   {
      RARCH_WARN("Libretro core is hardware rendered. Must use post-shaded FFmpeg recording as well.\n");
      g_extern.recording = false;
      return;
   }

   double fps = g_extern.system.av_info.timing.fps;
   double samplerate = g_extern.system.av_info.timing.sample_rate;
   RARCH_LOG("Custom timing given: FPS: %.4f, Sample rate: %.4f\n", (float)fps, (float)samplerate);

   struct ffemu_params params = {0};
   const struct retro_system_av_info *info = &g_extern.system.av_info;
   params.out_width  = info->geometry.base_width;
   params.out_height = info->geometry.base_height;
   params.fb_width   = info->geometry.max_width;
   params.fb_height  = info->geometry.max_height;
   params.channels   = 2;
   params.filename   = g_extern.record_path;
   params.fps        = fps;
   params.samplerate = samplerate;
   params.pix_fmt    = g_extern.system.pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888 ? FFEMU_PIX_ARGB8888 : FFEMU_PIX_RGB565;
   params.config     = *g_extern.record_config ? g_extern.record_config : NULL;

   if (g_settings.video.gpu_record && driver.video->read_viewport)
   {
      struct rarch_viewport vp = {0};
      video_viewport_info_func(&vp);

      if (!vp.width || !vp.height)
      {
         RARCH_ERR("Failed to get viewport information from video driver. "
               "Cannot start recording ...\n");
         g_extern.recording = false;
         return;
      }

      params.out_width           = vp.width;
      params.out_height          = vp.height;
      params.fb_width            = next_pow2(vp.width);
      params.fb_height           = next_pow2(vp.height);

      if (g_settings.video.force_aspect && (g_extern.system.aspect_ratio > 0.0f))
         params.aspect_ratio  = g_extern.system.aspect_ratio;
      else
         params.aspect_ratio  = (float)vp.width / vp.height;

      params.pix_fmt             = FFEMU_PIX_BGR24;
      g_extern.record_gpu_width  = vp.width;
      g_extern.record_gpu_height = vp.height;

      RARCH_LOG("Detected viewport of %u x %u\n",
            vp.width, vp.height);

      g_extern.record_gpu_buffer = (uint8_t*)malloc(vp.width * vp.height * 3);
      if (!g_extern.record_gpu_buffer)
      {
         RARCH_ERR("Failed to allocate GPU record buffer.\n");
         g_extern.recording = false;
         return;
      }
   }
   else
   {
      if (g_extern.record_width || g_extern.record_height)
      {
         params.out_width = g_extern.record_width;
         params.out_height = g_extern.record_height;
      }

      if (g_settings.video.force_aspect && (g_extern.system.aspect_ratio > 0.0f))
         params.aspect_ratio = g_extern.system.aspect_ratio;
      else
         params.aspect_ratio = (float)params.out_width / params.out_height;

      if (g_settings.video.post_filter_record && g_extern.filter.active)
      {
         g_extern.filter.psize(&params.out_width, &params.out_height);
         params.pix_fmt = FFEMU_PIX_ARGB8888;

         unsigned max_width  = params.fb_width;
         unsigned max_height = params.fb_height;
         g_extern.filter.psize(&max_width, &max_height);
         params.fb_width  = next_pow2(max_width);
         params.fb_height = next_pow2(max_height);
      }
   }

   RARCH_LOG("Recording with FFmpeg to %s @ %ux%u. (FB size: %ux%u pix_fmt: %u)\n",
         g_extern.record_path,
         params.out_width, params.out_height,
         params.fb_width, params.fb_height,
         (unsigned)params.pix_fmt);

   g_extern.rec = ffemu_new(&params);
   if (!g_extern.rec)
   {
      RARCH_ERR("Failed to start FFmpeg recording.\n");
      g_extern.recording = false;

      free(g_extern.record_gpu_buffer);
      g_extern.record_gpu_buffer = NULL;
   }
}

static void deinit_recording(void)
{
   if (!g_extern.recording)
      return;

   ffemu_finalize(g_extern.rec);
   ffemu_free(g_extern.rec);
   g_extern.rec = NULL;

   free(g_extern.record_gpu_buffer);
   g_extern.record_gpu_buffer = NULL;
}
#endif

void rarch_init_msg_queue(void)
{
   if (g_extern.msg_queue)
      return;

   rarch_assert(g_extern.msg_queue = msg_queue_new(8));
}

void rarch_deinit_msg_queue(void)
{
   if (g_extern.msg_queue)
   {
      msg_queue_free(g_extern.msg_queue);
      g_extern.msg_queue = NULL;
   }
}

static void init_cheats(void)
{
   if (*g_settings.cheat_database)
      g_extern.cheat = cheat_manager_new(g_settings.cheat_database);
}

static void deinit_cheats(void)
{
   if (g_extern.cheat)
      cheat_manager_free(g_extern.cheat);
}

void rarch_init_rewind(void)
{
   if (!g_settings.rewind_enable || g_extern.state_manager)
      return;

   if (g_extern.system.audio_callback.callback)
   {
      RARCH_ERR("Implementation uses threaded audio. Cannot use rewind.\n");
      return;
   }

   g_extern.state_size = pretro_serialize_size();
   if (!g_extern.state_size)
   {
      RARCH_ERR("Implementation does not support save states. Cannot use rewind.\n");
      return;
   }

   // Make sure we allocate at least 4-byte multiple.
   size_t aligned_state_size = (g_extern.state_size + 3) & ~3;
   g_extern.state_buf = calloc(1, aligned_state_size);

   if (!g_extern.state_buf)
   {
      RARCH_ERR("Failed to allocate memory for rewind buffer.\n");
      return;
   }

   if (!pretro_serialize(g_extern.state_buf, g_extern.state_size))
   {
      RARCH_ERR("Failed to perform initial serialization for rewind.\n");
      free(g_extern.state_buf);
      g_extern.state_buf = NULL;
      return;
   }

   RARCH_LOG("Initing rewind buffer with size: %u MB\n", (unsigned)(g_settings.rewind_buffer_size / 1000000));
   g_extern.state_manager = state_manager_new(aligned_state_size, g_settings.rewind_buffer_size, g_extern.state_buf);

   if (!g_extern.state_manager)
      RARCH_WARN("Failed to init rewind buffer. Rewinding will be disabled.\n");
}

void rarch_deinit_rewind(void)
{
   if (g_extern.state_manager)
      state_manager_free(g_extern.state_manager);
   g_extern.state_manager = NULL;

   free(g_extern.state_buf);
   g_extern.state_buf = NULL;
}

#ifdef HAVE_BSV_MOVIE
static void init_movie(void)
{
   if (g_extern.bsv.movie_start_playback)
   {
      g_extern.bsv.movie = bsv_movie_init(g_extern.bsv.movie_start_path, RARCH_MOVIE_PLAYBACK);
      if (!g_extern.bsv.movie)
      {
         RARCH_ERR("Failed to load movie file: \"%s\".\n", g_extern.bsv.movie_start_path);
         rarch_fail(1, "init_movie()");
      }

      g_extern.bsv.movie_playback = true;
      msg_queue_push(g_extern.msg_queue, "Starting movie playback.", 2, 180);
      RARCH_LOG("Starting movie playback.\n");
      g_settings.rewind_granularity = 1;
   }
   else if (g_extern.bsv.movie_start_recording)
   {
      char msg[PATH_MAX];
      snprintf(msg, sizeof(msg), "Starting movie record to \"%s\".",
            g_extern.bsv.movie_start_path);

      g_extern.bsv.movie = bsv_movie_init(g_extern.bsv.movie_start_path, RARCH_MOVIE_RECORD);
      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue,
            g_extern.bsv.movie ? msg : "Failed to start movie record.", 1, 180);

      if (g_extern.bsv.movie)
      {
         RARCH_LOG("Starting movie record to \"%s\".\n", g_extern.bsv.movie_start_path);
         g_settings.rewind_granularity = 1;
      }
      else
         RARCH_ERR("Failed to start movie record.\n");
   }
}

static void deinit_movie(void)
{
   if (g_extern.bsv.movie)
      bsv_movie_free(g_extern.bsv.movie);
}
#endif

#define RARCH_DEFAULT_PORT 55435

#ifdef HAVE_NETPLAY
static void init_netplay(void)
{
   if (!g_extern.netplay_enable)
      return;

#ifdef HAVE_BSV_MOVIE
   if (g_extern.bsv.movie_start_playback)
   {
      RARCH_WARN("Movie playback has started. Cannot start netplay.\n");
      return;
   }
#endif

   struct retro_callbacks cbs = {0};
   cbs.frame_cb = video_frame;
   cbs.sample_cb = audio_sample;
   cbs.sample_batch_cb = audio_sample_batch;
   cbs.state_cb = input_state;

   if (*g_extern.netplay_server)
   {
      RARCH_LOG("Connecting to netplay host...\n");
      g_extern.netplay_is_client = true;
   }
   else
      RARCH_LOG("Waiting for client...\n");

   g_extern.netplay = netplay_new(g_extern.netplay_is_client ? g_extern.netplay_server : NULL,
         g_extern.netplay_port ? g_extern.netplay_port : RARCH_DEFAULT_PORT,
         g_extern.netplay_sync_frames, &cbs, g_extern.netplay_is_spectate,
         g_extern.netplay_nick);

   if (!g_extern.netplay)
   {
      g_extern.netplay_is_client = false;
      RARCH_WARN("Failed to init netplay ...\n");

      if (g_extern.msg_queue)
      {
         msg_queue_push(g_extern.msg_queue,
               "Failed to init netplay ...",
               0, 180);
      }
   }
}

static void deinit_netplay(void)
{
   if (g_extern.netplay)
      netplay_free(g_extern.netplay);
}
#endif

#ifdef HAVE_COMMAND
static void init_command(void)
{
   if (!g_settings.stdin_cmd_enable && !g_settings.network_cmd_enable)
      return;

   if (g_settings.stdin_cmd_enable && driver.stdin_claimed)
   {
      RARCH_WARN("stdin command interface is desired, but input driver has already claimed stdin.\n"
            "Cannot use this command interface.\n");
   }

   driver.command = rarch_cmd_new(g_settings.stdin_cmd_enable && !driver.stdin_claimed,
         g_settings.network_cmd_enable, g_settings.network_cmd_port);

   if (!driver.command)
      RARCH_ERR("Failed to initialize command interface.\n");
}

static void deinit_command(void)
{
   if (driver.command)
   {
      rarch_cmd_free(driver.command);
      driver.command = NULL;
   }
}
#endif

static void init_libretro_cbs_plain(void)
{
   pretro_set_video_refresh(video_frame);
   pretro_set_audio_sample(audio_sample);
   pretro_set_audio_sample_batch(audio_sample_batch);
   pretro_set_input_state(input_state);
   pretro_set_input_poll(rarch_input_poll);
}

static void init_libretro_cbs(void)
{
   init_libretro_cbs_plain();

#ifdef HAVE_NETPLAY
   if (g_extern.netplay)
   {
      pretro_set_video_refresh(g_extern.netplay_is_spectate ?
            video_frame : video_frame_net);

      pretro_set_audio_sample(g_extern.netplay_is_spectate ?
            audio_sample : audio_sample_net);
      pretro_set_audio_sample_batch(g_extern.netplay_is_spectate ?
            audio_sample_batch : audio_sample_batch_net);

      pretro_set_input_state(g_extern.netplay_is_spectate ?
            (g_extern.netplay_is_client ? input_state_spectate_client : input_state_spectate)
            : input_state_net);
   }
#endif
}

#if defined(HAVE_THREADS)
void rarch_init_autosave(void)
{
   int ram_types[2] = {-1, -1};
   const char *ram_paths[2] = {NULL, NULL};

   switch (g_extern.game_type)
   {
      case RARCH_CART_BSX:
      case RARCH_CART_BSX_SLOTTED:
         ram_types[0] = RETRO_MEMORY_SNES_BSX_RAM;
         ram_types[1] = RETRO_MEMORY_SNES_BSX_PRAM;
         ram_paths[0] = g_extern.savefile_name_srm;
         ram_paths[1] = g_extern.savefile_name_psrm;
         break;

      case RARCH_CART_SUFAMI:
         ram_types[0] = RETRO_MEMORY_SNES_SUFAMI_TURBO_A_RAM;
         ram_types[1] = RETRO_MEMORY_SNES_SUFAMI_TURBO_B_RAM;
         ram_paths[0] = g_extern.savefile_name_asrm;
         ram_paths[1] = g_extern.savefile_name_bsrm;
         break;

      case RARCH_CART_SGB:
         ram_types[0] = RETRO_MEMORY_SNES_GAME_BOY_RAM;
         ram_types[1] = RETRO_MEMORY_SNES_GAME_BOY_RTC;
         ram_paths[0] = g_extern.savefile_name_srm;
         ram_paths[1] = g_extern.savefile_name_rtc;
         break;

      default:
         ram_types[0] = RETRO_MEMORY_SAVE_RAM;
         ram_types[1] = RETRO_MEMORY_RTC;
         ram_paths[0] = g_extern.savefile_name_srm;
         ram_paths[1] = g_extern.savefile_name_rtc;
   }

   if (g_settings.autosave_interval > 0)
   {
      unsigned i;
      for (i = 0; i < sizeof(g_extern.autosave) / sizeof(g_extern.autosave[0]); i++)
      {
         if (ram_paths[i] && *ram_paths[i] && pretro_get_memory_size(ram_types[i]) > 0)
         {
            g_extern.autosave[i] = autosave_new(ram_paths[i], 
                  pretro_get_memory_data(ram_types[i]), 
                  pretro_get_memory_size(ram_types[i]), 
                  g_settings.autosave_interval);
            if (!g_extern.autosave[i])
               RARCH_WARN("Could not initialize autosave.\n");
         }
      }
   }
}

void rarch_deinit_autosave(void)
{
   unsigned i;
   for (i = 0; i < ARRAY_SIZE(g_extern.autosave); i++)
   {
      if (g_extern.autosave[i])
         autosave_free(g_extern.autosave[i]);
      g_extern.autosave[i] = NULL;
   }
}
#endif

static void set_savestate_auto_index(void)
{
   if (!g_settings.savestate_auto_index)
      return;

   // Find the file in the same directory as g_extern.savestate_name with the largest numeral suffix.
   // E.g. /foo/path/game.state, will try to find /foo/path/game.state%d, where %d is the largest number available.

   char state_dir[PATH_MAX];
   char state_base[PATH_MAX];

   fill_pathname_basedir(state_dir, g_extern.savestate_name, sizeof(state_dir));
   fill_pathname_base(state_base, g_extern.savestate_name, sizeof(state_base));

   unsigned max_index = 0;

   struct string_list *dir_list = dir_list_new(state_dir, NULL, false);
   if (!dir_list)
      return;

   size_t i;
   for (i = 0; i < dir_list->size; i++)
   {
      const char *dir_elem = dir_list->elems[i].data;

      char elem_base[PATH_MAX];
      fill_pathname_base(elem_base, dir_elem, sizeof(elem_base));

      if (strstr(elem_base, state_base) != elem_base)
         continue;

      const char *end = dir_elem + strlen(dir_elem);
      while ((end > dir_elem) && isdigit(end[-1])) end--;

      unsigned index = strtoul(end, NULL, 0);
      if (index > max_index)
         max_index = index;
   }

   dir_list_free(dir_list);

   g_extern.state_slot = max_index;
   RARCH_LOG("Found last state slot: #%u\n", g_extern.state_slot);
}

static void fill_pathnames(void)
{
   switch (g_extern.game_type)
   {
      case RARCH_CART_BSX:
      case RARCH_CART_BSX_SLOTTED:
         // BSX PSRM
         if (!g_extern.has_set_save_path)
         {
            fill_pathname(g_extern.savefile_name_srm,
                  g_extern.bsx_rom_path, ".srm", sizeof(g_extern.savefile_name_srm));
         }

         fill_pathname(g_extern.savefile_name_psrm,
               g_extern.savefile_name_srm, ".psrm", sizeof(g_extern.savefile_name_psrm));

         if (!g_extern.has_set_state_path)
         {
            fill_pathname(g_extern.savestate_name,
                  g_extern.bsx_rom_path, ".state", sizeof(g_extern.savestate_name));
         }
         break;

      case RARCH_CART_SUFAMI:
         if (g_extern.has_set_save_path && *g_extern.sufami_rom_path[0] && *g_extern.sufami_rom_path[1])
            RARCH_WARN("Sufami Turbo SRAM paths will be inferred from their respective paths to avoid conflicts.\n");

         // SUFAMI ARAM
         fill_pathname(g_extern.savefile_name_asrm,
               g_extern.sufami_rom_path[0], ".srm", sizeof(g_extern.savefile_name_asrm));

         // SUFAMI BRAM
         fill_pathname(g_extern.savefile_name_bsrm,
               g_extern.sufami_rom_path[1], ".srm", sizeof(g_extern.savefile_name_bsrm));

         if (!g_extern.has_set_state_path)
         {
            fill_pathname(g_extern.savestate_name,
                  *g_extern.sufami_rom_path[0] ?
                     g_extern.sufami_rom_path[0] : g_extern.sufami_rom_path[1],
                     ".state", sizeof(g_extern.savestate_name));
         }
         break;

      case RARCH_CART_SGB:
         if (!g_extern.has_set_save_path)
         {
            fill_pathname(g_extern.savefile_name_srm,
                  g_extern.gb_rom_path, ".srm", sizeof(g_extern.savefile_name_srm));
         }

         if (!g_extern.has_set_state_path)
         {
            fill_pathname(g_extern.savestate_name,
                  g_extern.gb_rom_path, ".state", sizeof(g_extern.savestate_name));
         }

         fill_pathname(g_extern.savefile_name_rtc,
               g_extern.savefile_name_srm, ".rtc", sizeof(g_extern.savefile_name_rtc));
         break;

      default:
         // Infer .rtc save path from save ram path.
         fill_pathname(g_extern.savefile_name_rtc,
               g_extern.savefile_name_srm, ".rtc", sizeof(g_extern.savefile_name_rtc));
   }

#ifdef HAVE_BSV_MOVIE
   fill_pathname(g_extern.bsv.movie_path, g_extern.savefile_name_srm, "", sizeof(g_extern.bsv.movie_path));
#endif

   if (*g_extern.basename)
   {
      if (!(*g_extern.ups_name))
         fill_pathname_noext(g_extern.ups_name, g_extern.basename, ".ups", sizeof(g_extern.ups_name));

      if (!(*g_extern.bps_name))
         fill_pathname_noext(g_extern.bps_name, g_extern.basename, ".bps", sizeof(g_extern.bps_name));

      if (!(*g_extern.ips_name))
         fill_pathname_noext(g_extern.ips_name, g_extern.basename, ".ips", sizeof(g_extern.ips_name));

      if (!(*g_extern.xml_name))
         fill_pathname_noext(g_extern.xml_name, g_extern.basename, ".xml", sizeof(g_extern.xml_name));
   }
}

static void load_auto_state(void)
{
#ifdef HAVE_NETPLAY
   if (g_extern.netplay_enable && !g_extern.netplay_is_spectate)
      return;
#endif

   if (!g_settings.savestate_auto_load)
      return;

   char savestate_name_auto[PATH_MAX];
   fill_pathname_noext(savestate_name_auto, g_extern.savestate_name,
         ".auto", sizeof(savestate_name_auto));

   if (path_file_exists(savestate_name_auto))
   {
      RARCH_LOG("Found auto savestate in: %s\n", savestate_name_auto);
      bool ret = load_state(savestate_name_auto);

      char msg[PATH_MAX];
      snprintf(msg, sizeof(msg), "Auto-loading savestate from \"%s\" %s.", savestate_name_auto, ret ? "succeeded" : "failed");
      msg_queue_push(g_extern.msg_queue, msg, 1, 180);
      RARCH_LOG("%s\n", msg);
   }
}

static void save_auto_state(void)
{
   if (!g_settings.savestate_auto_save)
      return;

   char savestate_name_auto[PATH_MAX];
   fill_pathname_noext(savestate_name_auto, g_extern.savestate_name,
         ".auto", sizeof(savestate_name_auto));

   bool ret = save_state(savestate_name_auto);
   RARCH_LOG("Auto save state to \"%s\" %s.\n", savestate_name_auto, ret ? "succeeded" : "failed");
}

void rarch_load_state(void)
{
   char load_path[PATH_MAX];

   if (g_extern.state_slot > 0)
      snprintf(load_path, sizeof(load_path), "%s%u", g_extern.savestate_name, g_extern.state_slot);
   else
      snprintf(load_path, sizeof(load_path), "%s", g_extern.savestate_name);

   size_t size = pretro_serialize_size();
   char msg[512];

   if (size)
   {
      if (load_state(load_path))
         snprintf(msg, sizeof(msg), "Loaded state from slot #%u.", g_extern.state_slot);
      else
         snprintf(msg, sizeof(msg), "Failed to load state from \"%s\".", load_path);
   }
   else
      strlcpy(msg, "Core does not support save states.", sizeof(msg));

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 2, 180);
   RARCH_LOG("%s\n", msg);
}

void rarch_save_state(void)
{
   if (g_settings.savestate_auto_index)
      g_extern.state_slot++;

   char save_path[PATH_MAX];

   if (g_extern.state_slot > 0)
      snprintf(save_path, sizeof(save_path), "%s%u", g_extern.savestate_name, g_extern.state_slot);
   else
      snprintf(save_path, sizeof(save_path), "%s", g_extern.savestate_name);

   size_t size = pretro_serialize_size();
   char msg[512];

   if (size)
   {
      if (save_state(save_path))
         snprintf(msg, sizeof(msg), "Saved state to slot #%u.", g_extern.state_slot);
      else
         snprintf(msg, sizeof(msg), "Failed to save state to \"%s\".", save_path);
   }
   else
      strlcpy(msg, "Core does not support save states.", sizeof(msg));

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 2, 180);
   RARCH_LOG("%s\n", msg);
}

// Save or load state here.
static void check_savestates(bool immutable)
{
   static bool old_should_savestate = false;
   bool should_savestate = input_key_pressed_func(RARCH_SAVE_STATE_KEY);

   if (should_savestate && !old_should_savestate)
      rarch_save_state();
   old_should_savestate = should_savestate;

   if (!immutable)
   {
      static bool old_should_loadstate = false;
      bool should_loadstate = input_key_pressed_func(RARCH_LOAD_STATE_KEY);

      if (!should_savestate && should_loadstate && !old_should_loadstate)
         rarch_load_state();
      old_should_loadstate = should_loadstate;
   }
}

void rarch_set_fullscreen(bool fullscreen)
{
   g_settings.video.fullscreen = fullscreen;
   driver.video_cache_context = g_extern.system.hw_render_callback.cache_context;
   driver.video_cache_context_ack = false;
   uninit_drivers();
   init_drivers();
   driver.video_cache_context = false;

   // Poll input to avoid possibly stale data to corrupt things.
   if (driver.input)
      input_poll_func();
}

static bool check_fullscreen(void)
{
   // If we go fullscreen we drop all drivers and reinit to be safe.
   static bool was_pressed = false;
   bool pressed = input_key_pressed_func(RARCH_FULLSCREEN_TOGGLE_KEY);
   bool toggle = pressed && !was_pressed;

   if (toggle)
   {
      g_settings.video.fullscreen = !g_settings.video.fullscreen;
      rarch_set_fullscreen(g_settings.video.fullscreen);
   }

   was_pressed = pressed;
   return toggle;
}

void rarch_state_slot_increase(void)
{
   g_extern.state_slot++;

   if (g_extern.msg_queue)
      msg_queue_clear(g_extern.msg_queue);
   char msg[256];

   snprintf(msg, sizeof(msg), "State slot: %u", g_extern.state_slot);

   if (g_extern.msg_queue)
      msg_queue_push(g_extern.msg_queue, msg, 1, 180);

   RARCH_LOG("%s\n", msg);
}

void rarch_state_slot_decrease(void)
{
   if (g_extern.state_slot > 0)
      g_extern.state_slot--;

   if (g_extern.msg_queue)
      msg_queue_clear(g_extern.msg_queue);

   char msg[256];

   snprintf(msg, sizeof(msg), "State slot: %u", g_extern.state_slot);

   if (g_extern.msg_queue)
      msg_queue_push(g_extern.msg_queue, msg, 1, 180);

   RARCH_LOG("%s\n", msg);
}

static void check_stateslots(void)
{
   // Save state slots
   static bool old_should_slot_increase = false;
   bool should_slot_increase = input_key_pressed_func(RARCH_STATE_SLOT_PLUS);
   if (should_slot_increase && !old_should_slot_increase)
      rarch_state_slot_increase();
   old_should_slot_increase = should_slot_increase;

   static bool old_should_slot_decrease = false;
   bool should_slot_decrease = input_key_pressed_func(RARCH_STATE_SLOT_MINUS);
   if (should_slot_decrease && !old_should_slot_decrease)
      rarch_state_slot_decrease();
   old_should_slot_decrease = should_slot_decrease;
}

static inline void flush_rewind_audio(void)
{
   if (g_extern.frame_is_reverse) // We just rewound. Flush rewind audio buffer.
   {
      g_extern.audio_active = audio_flush(g_extern.audio_data.rewind_buf + g_extern.audio_data.rewind_ptr,
            g_extern.audio_data.rewind_size - g_extern.audio_data.rewind_ptr) && g_extern.audio_active;
   }
}

static inline void setup_rewind_audio(void)
{
   unsigned i;
   // Push audio ready to be played.
   g_extern.audio_data.rewind_ptr = g_extern.audio_data.rewind_size;
   for (i = 0; i < g_extern.audio_data.data_ptr; i += 2)
   {
      g_extern.audio_data.rewind_buf[--g_extern.audio_data.rewind_ptr] =
         g_extern.audio_data.conv_outsamples[i + 1];

      g_extern.audio_data.rewind_buf[--g_extern.audio_data.rewind_ptr] =
         g_extern.audio_data.conv_outsamples[i + 0];
   }

   g_extern.audio_data.data_ptr = 0;
}

static void check_rewind(void)
{
   flush_rewind_audio();
   g_extern.frame_is_reverse = false;

   static bool first = true;
   if (first)
   {
      first = false;
      return;
   }

   if (!g_extern.state_manager)
      return;

   if (input_key_pressed_func(RARCH_REWIND))
   {
      msg_queue_clear(g_extern.msg_queue);
      void *buf;
      if (state_manager_pop(g_extern.state_manager, &buf))
      {
         g_extern.frame_is_reverse = true;
         setup_rewind_audio();

         msg_queue_push(g_extern.msg_queue, "Rewinding.", 0, g_extern.is_paused ? 1 : 30);
         pretro_unserialize(buf, g_extern.state_size);

#ifdef HAVE_BSV_MOVIE
         if (g_extern.bsv.movie)
            bsv_movie_frame_rewind(g_extern.bsv.movie);
#endif
      }
      else
         msg_queue_push(g_extern.msg_queue, "Reached end of rewind buffer.", 0, 30);
   }
   else
   {
      static unsigned cnt = 0;
      cnt = (cnt + 1) % (g_settings.rewind_granularity ? g_settings.rewind_granularity : 1); // Avoid possible SIGFPE.
#ifdef HAVE_BSV_MOVIE
      if (cnt == 0 || g_extern.bsv.movie)
#else
      if (cnt == 0)
#endif
      {
         pretro_serialize(g_extern.state_buf, g_extern.state_size);
         state_manager_push(g_extern.state_manager, g_extern.state_buf);
      }
   }

   pretro_set_audio_sample(g_extern.frame_is_reverse ?
         audio_sample_rewind : audio_sample);
   pretro_set_audio_sample_batch(g_extern.frame_is_reverse ?
         audio_sample_batch_rewind : audio_sample_batch);
}

static void check_slowmotion(void)
{
   g_extern.is_slowmotion = input_key_pressed_func(RARCH_SLOWMOTION);
   if (g_extern.is_slowmotion)
   {
      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue, g_extern.frame_is_reverse ? "Slow motion rewind." : "Slow motion.", 0, 30);
   }
}

#ifdef HAVE_BSV_MOVIE
static void movie_record_toggle(void)
{
   if (g_extern.bsv.movie)
   {
      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue, "Stopping movie record.", 2, 180);
      RARCH_LOG("Stopping movie record.\n");
      bsv_movie_free(g_extern.bsv.movie);
      g_extern.bsv.movie = NULL;
   }
   else
   {
      g_settings.rewind_granularity = 1;

      char path[PATH_MAX];
      if (g_extern.state_slot > 0)
      {
         snprintf(path, sizeof(path), "%s%u.bsv",
               g_extern.bsv.movie_path, g_extern.state_slot);
      }
      else
      {
         snprintf(path, sizeof(path), "%s.bsv",
               g_extern.bsv.movie_path);
      }

      char msg[PATH_MAX];
      snprintf(msg, sizeof(msg), "Starting movie record to \"%s\".", path);

      g_extern.bsv.movie = bsv_movie_init(path, RARCH_MOVIE_RECORD);
      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue, g_extern.bsv.movie ? msg : "Failed to start movie record.", 1, 180);

      if (g_extern.bsv.movie)
         RARCH_LOG("Starting movie record to \"%s\".\n", path);
      else
         RARCH_ERR("Failed to start movie record.\n");
   }
}

static void check_movie_record(bool pressed)
{
   if (pressed)
      movie_record_toggle();
}

static void check_movie_playback(bool pressed)
{
   if (g_extern.bsv.movie_end || pressed)
   {
      msg_queue_push(g_extern.msg_queue, "Movie playback ended.", 1, 180);
      RARCH_LOG("Movie playback ended.\n");

      bsv_movie_free(g_extern.bsv.movie);
      g_extern.bsv.movie = NULL;
      g_extern.bsv.movie_end = false;
      g_extern.bsv.movie_playback = false;
   }
}

static void check_movie(void)
{
   static bool old_button = false;
   bool new_button = input_key_pressed_func(RARCH_MOVIE_RECORD_TOGGLE);
   bool pressed = new_button && !old_button;

   if (g_extern.bsv.movie_playback)
      check_movie_playback(pressed);
   else
      check_movie_record(pressed);

   old_button = new_button;
}
#endif

static void check_pause(void)
{
   static bool old_state = false;
   bool new_state = input_key_pressed_func(RARCH_PAUSE_TOGGLE);

   // FRAMEADVANCE will set us into pause mode.
   new_state |= !g_extern.is_paused && input_key_pressed_func(RARCH_FRAMEADVANCE);

   static bool old_focus = true;
   bool focus = true;

   if (g_settings.pause_nonactive)
      focus = video_focus_func();

   if (focus && new_state && !old_state)
   {
      g_extern.is_paused = !g_extern.is_paused;

      if (g_extern.is_paused)
      {
         RARCH_LOG("Paused.\n");
         if (driver.audio_data)
            audio_stop_func();
      }
      else 
      {
         RARCH_LOG("Unpaused.\n");
         if (driver.audio_data)
         {
            if (!g_extern.audio_data.mute && !audio_start_func())
            {
               RARCH_ERR("Failed to resume audio driver. Will continue without audio.\n");
               g_extern.audio_active = false;
            }
         }
      }
   }
   else if (focus && !old_focus)
   {
      RARCH_LOG("Unpaused.\n");
      g_extern.is_paused = false;
      if (driver.audio_data && !g_extern.audio_data.mute && !audio_start_func())
      {
         RARCH_ERR("Failed to resume audio driver. Will continue without audio.\n");
         g_extern.audio_active = false;
      }
   }
   else if (!focus && old_focus)
   {
      RARCH_LOG("Paused.\n");
      g_extern.is_paused = true;
      if (driver.audio_data)
         audio_stop_func();
   }

   old_focus = focus;
   old_state = new_state;
}

static void check_oneshot(void)
{
   static bool old_state = false;
   bool new_state = input_key_pressed_func(RARCH_FRAMEADVANCE);
   g_extern.is_oneshot = (new_state && !old_state);
   old_state = new_state;

   // Rewind buttons works like FRAMEREWIND when paused. We will one-shot in that case.
   static bool old_rewind_state = false;
   bool new_rewind_state = input_key_pressed_func(RARCH_REWIND);
   g_extern.is_oneshot |= new_rewind_state && !old_rewind_state;
   old_rewind_state = new_rewind_state;
}

void rarch_game_reset(void)
{
   RARCH_LOG("Resetting game.\n");
   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, "Reset.", 1, 120);
   pretro_reset();
   init_controllers(); // bSNES since v073r01 resets controllers to JOYPAD after a reset, so just enforce it here.
}

static void check_reset(void)
{
   static bool old_state = false;
   bool new_state = input_key_pressed_func(RARCH_RESET);
   if (new_state && !old_state)
      rarch_game_reset();

   old_state = new_state;
}

static void check_turbo(void)
{
   unsigned i;

   g_extern.turbo_count++;

   static const struct retro_keybind *binds[MAX_PLAYERS] = {
      g_settings.input.binds[0],
      g_settings.input.binds[1],
      g_settings.input.binds[2],
      g_settings.input.binds[3],
      g_settings.input.binds[4],
      g_settings.input.binds[5],
      g_settings.input.binds[6],
      g_settings.input.binds[7],
   };

   for (i = 0; i < MAX_PLAYERS; i++)
      g_extern.turbo_frame_enable[i] =
         input_input_state_func(binds, i, RETRO_DEVICE_JOYPAD, 0, RARCH_TURBO_ENABLE);
}

static void check_shader_dir(void)
{
   static bool old_pressed_next;
   static bool old_pressed_prev;

   if (!g_extern.shader_dir.list || !driver.video->set_shader)
      return;

   bool should_apply = false;
   bool pressed_next = input_key_pressed_func(RARCH_SHADER_NEXT);
   bool pressed_prev = input_key_pressed_func(RARCH_SHADER_PREV);
   if (pressed_next && !old_pressed_next)
   {
      should_apply = true;
      g_extern.shader_dir.ptr = (g_extern.shader_dir.ptr + 1) % g_extern.shader_dir.list->size;
   }
   else if (pressed_prev && !old_pressed_prev)
   {
      should_apply = true;
      if (g_extern.shader_dir.ptr == 0)
         g_extern.shader_dir.ptr = g_extern.shader_dir.list->size - 1;
      else
         g_extern.shader_dir.ptr--;
   }

   if (should_apply)
   {
      const char *shader          = g_extern.shader_dir.list->elems[g_extern.shader_dir.ptr].data;
      enum rarch_shader_type type = RARCH_SHADER_NONE;

      const char *ext = strrchr(shader, '.');
      if (ext)
      {
         if (strcmp(ext, ".shader") == 0)
            type = RARCH_SHADER_GLSL;
         else if (strcmp(ext, ".cg") == 0 || strcmp(ext, ".cgp") == 0)
            type = RARCH_SHADER_CG;
      }

      if (type == RARCH_SHADER_NONE)
         return;

      msg_queue_clear(g_extern.msg_queue);

      char msg[512];
      snprintf(msg, sizeof(msg), "Shader #%u: \"%s\".", (unsigned)g_extern.shader_dir.ptr, shader);
      msg_queue_push(g_extern.msg_queue, msg, 1, 120);
      RARCH_LOG("Applying shader \"%s\".\n", shader);

      if (!video_set_shader_func(type, shader))
         RARCH_WARN("Failed to apply shader.\n");
   }

   old_pressed_next = pressed_next;
   old_pressed_prev = pressed_prev;
}

static void check_cheats(void)
{
   if (!g_extern.cheat)
      return;

   static bool old_pressed_prev;
   static bool old_pressed_next;
   static bool old_pressed_toggle;

   bool pressed_next = input_key_pressed_func(RARCH_CHEAT_INDEX_PLUS);
   bool pressed_prev = input_key_pressed_func(RARCH_CHEAT_INDEX_MINUS);
   bool pressed_toggle = input_key_pressed_func(RARCH_CHEAT_TOGGLE);

   if (pressed_next && !old_pressed_next)
      cheat_manager_index_next(g_extern.cheat);
   else if (pressed_prev && !old_pressed_prev)
      cheat_manager_index_prev(g_extern.cheat);
   else if (pressed_toggle && !old_pressed_toggle)
      cheat_manager_toggle(g_extern.cheat);

   old_pressed_prev = pressed_prev;
   old_pressed_next = pressed_next;
   old_pressed_toggle = pressed_toggle;
}

void rarch_disk_control_append_image(const char *path)
{
   const struct retro_disk_control_callback *control = &g_extern.system.disk_control;
   rarch_disk_control_set_eject(true, false);

   control->add_image_index();
   unsigned new_index = control->get_num_images();
   if (!new_index)
      return;
   new_index--;

   struct retro_game_info info = {0};
   info.path = path;
   control->replace_image_index(new_index, &info);

   rarch_disk_control_set_index(new_index);

   char msg[512];
   snprintf(msg, sizeof(msg), "Appended disk: %s", path);
   RARCH_LOG("%s\n", msg);
   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 0, 180);

#if defined(HAVE_THREADS)
   rarch_deinit_autosave();
#endif

   // Update paths for our new image.
   // If we actually use append_image,
   // we assume that we started out in a single disk case,
   // and that this way of doing it makes the most sense.
   set_paths(path);
   fill_pathnames();

#if defined(HAVE_THREADS)
   rarch_init_autosave();
#endif

   rarch_disk_control_set_eject(false, false);

}

void rarch_disk_control_set_eject(bool new_state, bool log)
{
   const struct retro_disk_control_callback *control = &g_extern.system.disk_control;
   if (!control->get_num_images)
      return;

   bool error = false;
   char msg[256];
   *msg = '\0';

   if (control->set_eject_state(new_state))
      snprintf(msg, sizeof(msg), "%s virtual disk tray.", new_state ? "Ejected" : "Closed");
   else
   {
      error = true;
      snprintf(msg, sizeof(msg), "Failed to %s virtual disk tray.", new_state ? "eject" : "close");
   }

   if (*msg)
   {
      if (error)
         RARCH_ERR("%s\n", msg);
      else
         RARCH_LOG("%s\n", msg);

      // Only noise in RGUI.
      if (log)
      {
         msg_queue_clear(g_extern.msg_queue);
         msg_queue_push(g_extern.msg_queue, msg, 1, 180);
      }
   }
}

void rarch_disk_control_set_index(unsigned next_index)
{
   const struct retro_disk_control_callback *control = &g_extern.system.disk_control;
   if (!control->get_num_images)
      return;

   bool error = false;
   char msg[256];
   *msg = '\0';

   unsigned num_disks = control->get_num_images();
   if (control->set_image_index(next_index))
   {
      if (next_index < num_disks)
         snprintf(msg, sizeof(msg), "Setting disk %u of %u in tray.", next_index + 1, num_disks);
      else
         strlcpy(msg, "Removed disk from tray.", sizeof(msg));
   }
   else
   {
      if (next_index < num_disks)
         snprintf(msg, sizeof(msg), "Failed to set disk %u of %u.", next_index + 1, num_disks);
      else
         strlcpy(msg, "Failed to remove disk from tray.", sizeof(msg));
      error = true;
   }

   if (*msg)
   {
      if (error)
         RARCH_ERR("%s\n", msg);
      else
         RARCH_LOG("%s\n", msg);
      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue, msg, 1, 180);
   }
}

static void check_disk(void)
{
   const struct retro_disk_control_callback *control = &g_extern.system.disk_control;
   if (!control->get_num_images)
      return;

   static bool old_pressed_eject;
   static bool old_pressed_next;

   bool pressed_eject = input_key_pressed_func(RARCH_DISK_EJECT_TOGGLE);
   bool pressed_next  = input_key_pressed_func(RARCH_DISK_NEXT);

   if (pressed_eject && !old_pressed_eject)
   {
      bool new_state = !control->get_eject_state();
      rarch_disk_control_set_eject(new_state, true);
   }
   else if (pressed_next && !old_pressed_next)
   {
      unsigned num_disks = control->get_num_images();
      unsigned current   = control->get_image_index();
      if (num_disks && num_disks != UINT_MAX)
      {
         // Use "no disk" state when index == num_disks.
         unsigned next_index = current >= num_disks ? 0 : ((current + 1) % (num_disks + 1));
         rarch_disk_control_set_index(next_index);
      }
      else
         RARCH_ERR("Got invalid disk index from libretro.\n");
   }

   old_pressed_eject = pressed_eject;
   old_pressed_next  = pressed_next;
}

#if defined(HAVE_SCREENSHOTS) && !defined(_XBOX)
static void check_screenshot(void)
{
   static bool old_pressed;
   bool pressed = input_key_pressed_func(RARCH_SCREENSHOT);
   if (pressed && !old_pressed)
      rarch_take_screenshot();

   old_pressed = pressed;
}
#endif

#ifdef HAVE_DYLIB
static void check_dsp_config(void)
{
   if (!g_extern.audio_data.dsp_plugin || !g_extern.audio_data.dsp_plugin->config)
      return;

   static bool old_pressed;
   bool pressed = input_key_pressed_func(RARCH_DSP_CONFIG);
   if (pressed && !old_pressed)
      g_extern.audio_data.dsp_plugin->config(g_extern.audio_data.dsp_handle);

   old_pressed = pressed;
}
#endif

static void check_mute(void)
{
   if (!g_extern.audio_active)
      return;

   static bool old_pressed;
   bool pressed = input_key_pressed_func(RARCH_MUTE);
   if (pressed && !old_pressed)
   {
      g_extern.audio_data.mute = !g_extern.audio_data.mute;

      const char *msg = g_extern.audio_data.mute ? "Audio muted." : "Audio unmuted.";
      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue, msg, 1, 180);

      if (driver.audio_data)
      {
         if (g_extern.audio_data.mute)
            audio_stop_func();
         else if (!audio_start_func())
         {
            RARCH_ERR("Failed to unmute audio.\n");
            g_extern.audio_active = false;
         }
      }

      RARCH_LOG("%s\n", msg);
   }

   old_pressed = pressed;
}

static void check_volume(void)
{
   if (!g_extern.audio_active)
      return;

   float db_change   = 0.0f;
   bool pressed_up   = input_key_pressed_func(RARCH_VOLUME_UP);
   bool pressed_down = input_key_pressed_func(RARCH_VOLUME_DOWN);
   if (!pressed_up && !pressed_down)
      return;

   if (pressed_up)
      db_change += 0.5f;
   if (pressed_down)
      db_change -= 0.5f;

   g_extern.audio_data.volume_db += db_change;
   g_extern.audio_data.volume_db = max(g_extern.audio_data.volume_db, -80.0f);
   g_extern.audio_data.volume_db = min(g_extern.audio_data.volume_db, 12.0f);

   char msg[256];
   snprintf(msg, sizeof(msg), "Volume: %.1f dB", g_extern.audio_data.volume_db);
   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);
   RARCH_LOG("%s\n", msg);

   g_extern.audio_data.volume_gain = db_to_gain(g_extern.audio_data.volume_db);
}

#ifdef HAVE_NETPLAY
static void check_netplay_flip(void)
{
   static bool old_pressed;
   bool pressed = input_key_pressed_func(RARCH_NETPLAY_FLIP);
   if (pressed && !old_pressed)
      netplay_flip_players(g_extern.netplay);

   old_pressed = pressed;
}
#endif

void rarch_check_block_hotkey(void)
{
   driver.block_hotkey = driver.block_input;

   // If we haven't bound anything to this, 
   // always allow hotkeys.
   static const struct retro_keybind *bind = &g_settings.input.binds[0][RARCH_ENABLE_HOTKEY];
   if (!driver.block_hotkey && bind->key == RETROK_UNKNOWN && bind->joykey == NO_BTN && bind->joyaxis == AXIS_NONE)
      return;

   driver.block_hotkey = driver.block_input || !input_key_pressed_func(RARCH_ENABLE_HOTKEY);
}

#ifdef HAVE_OVERLAY
void rarch_check_overlay(void)
{
   if (!driver.overlay)
      return;

   static bool old_pressed;
   bool pressed = input_key_pressed_func(RARCH_OVERLAY_NEXT);
   if (pressed && !old_pressed)
      input_overlay_next(driver.overlay);

   old_pressed = pressed;
}
#endif

#ifndef RARCH_CONSOLE
static void check_grab_mouse_toggle(void)
{
   static bool old_pressed;
   bool pressed = input_key_pressed_func(RARCH_GRAB_MOUSE_TOGGLE) &&
      driver.input->grab_mouse;

   static bool grab_mouse_state;

   if (pressed && !old_pressed)
   {
      grab_mouse_state = !grab_mouse_state;
      RARCH_LOG("Grab mouse state: %s.\n", grab_mouse_state ? "yes" : "no");
      driver.input->grab_mouse(driver.input_data, grab_mouse_state);

      if (driver.video_poke && driver.video_poke->show_mouse)
         driver.video_poke->show_mouse(driver.video_data, !grab_mouse_state);
   }

   old_pressed = pressed;
}
#endif

static void do_state_checks(void)
{
   rarch_check_block_hotkey();

#if defined(HAVE_SCREENSHOTS) && !defined(_XBOX)
   check_screenshot();
#endif
   check_mute();
   check_volume();

   check_turbo();

#ifndef RARCH_CONSOLE
   check_grab_mouse_toggle();
#endif

#ifdef HAVE_OVERLAY
   rarch_check_overlay();
#endif

#ifdef HAVE_NETPLAY
   if (!g_extern.netplay)
   {
#endif
      check_pause();
      check_oneshot();

      if (check_fullscreen() && g_extern.is_paused)
         rarch_render_cached_frame();

      if (g_extern.is_paused && !g_extern.is_oneshot)
         return;

      check_fast_forward_button();

      check_stateslots();
#ifdef HAVE_BSV_MOVIE
      check_savestates(g_extern.bsv.movie);
#else
      check_savestates(false);
#endif

      check_rewind();
      check_slowmotion();

#ifdef HAVE_BSV_MOVIE
      check_movie();
#endif
     
      check_shader_dir();
      check_cheats();
      check_disk();

#ifdef HAVE_DYLIB
      check_dsp_config();
#endif
      check_reset();
#ifdef HAVE_NETPLAY
   }
   else
   {
      check_netplay_flip();
      check_fullscreen();
   }
#endif
}

static void init_state(void)
{
   g_extern.video_active = true;
   g_extern.audio_active = true;
   g_extern.game_type = RARCH_CART_NORMAL;
}

static void init_state_first(void)
{
   unsigned i;

   init_state();
   for (i = 0; i < MAX_PLAYERS; i++)
      g_settings.input.libretro_device[i] = RETRO_DEVICE_JOYPAD;
}

void rarch_main_clear_state(void)
{
   memset(&g_settings, 0, sizeof(g_settings));

   if (g_extern.log_file)
      fclose(g_extern.log_file);

   memset(&g_extern, 0, sizeof(g_extern));

   init_state_first();
}

#ifdef HAVE_ZLIB
#define DEFAULT_EXT "ZIP|zip"
#else
#define DEFAULT_EXT ""
#endif

void rarch_init_system_info(void)
{
   struct retro_system_info *info = &g_extern.system.info;
   pretro_get_system_info(info);

   if (!info->library_name)
      info->library_name = "Unknown";
   if (!info->library_version)
      info->library_version = "v0";

#ifdef RARCH_CONSOLE
   snprintf(g_extern.title_buf, sizeof(g_extern.title_buf), "%s %s",
         info->library_name, info->library_version);
#else
   snprintf(g_extern.title_buf, sizeof(g_extern.title_buf), "RetroArch : %s %s",
         info->library_name, info->library_version);
#endif
   strlcpy(g_extern.system.valid_extensions, info->valid_extensions ? info->valid_extensions : DEFAULT_EXT,
         sizeof(g_extern.system.valid_extensions));
   g_extern.system.block_extract = info->block_extract;
}

static void init_system_av_info(void)
{
   pretro_get_system_av_info(&g_extern.system.av_info);
   g_extern.frame_limit.last_frame_time = rarch_get_time_usec();
   g_extern.frame_limit.minimum_frame_time = (retro_time_t)roundf(1000000.0f / (g_extern.system.av_info.timing.fps * g_settings.fastforward_ratio));
}

static void verify_api_version(void)
{
   RARCH_LOG("Version of libretro API: %u\n", pretro_api_version());
   RARCH_LOG("Compiled against API: %u\n", RETRO_API_VERSION);
   if (pretro_api_version() != RETRO_API_VERSION)
      RARCH_WARN("RetroArch is compiled against a different version of libretro than this libretro implementation.\n");
}

// Make sure we haven't compiled for something we cannot run.
// Ideally, code would get swapped out depending on CPU support, but this will do for now.
static void validate_cpu_features(void)
{
   uint64_t cpu = rarch_get_cpu_features();
   (void)cpu;

#define FAIL_CPU(simd_type) do { \
   RARCH_ERR(simd_type " code is compiled in, but CPU does not support this feature. Cannot continue.\n"); \
   rarch_fail(1, "validate_cpu_features()"); \
} while(0)

#ifdef __SSE__
   if (!(cpu & RETRO_SIMD_SSE))
      FAIL_CPU("SSE");
#endif
#ifdef __SSE2__
   if (!(cpu & RETRO_SIMD_SSE2))
      FAIL_CPU("SSE2");
#endif
#ifdef __AVX__
   if (!(cpu & RETRO_SIMD_AVX))
      FAIL_CPU("AVX");
#endif
}

int rarch_main_init(int argc, char *argv[])
{
   init_state();

   int sjlj_ret;
   if ((sjlj_ret = setjmp(g_extern.error_sjlj_context)) > 0)
   {
      RARCH_ERR("Fatal error received in: \"%s\"\n", g_extern.error_string); 
      return sjlj_ret;
   }
   g_extern.error_in_init = true;
   parse_input(argc, argv);

   if (g_extern.verbose)
   {
      RARCH_LOG_OUTPUT("=== Build =======================================");
      print_compiler(stderr);
      RARCH_LOG_OUTPUT("=================================================\n");
   }

   validate_cpu_features();
   config_load();

   init_libretro_sym(g_extern.libretro_dummy);
   rarch_init_system_info();

   init_drivers_pre();

   verify_api_version();
   pretro_init();

   g_extern.use_sram = !g_extern.libretro_dummy && !g_extern.libretro_no_rom;
   bool allow_cheats = true;

   if (g_extern.libretro_no_rom && !g_extern.libretro_dummy)
   {
      if (!init_rom_file(g_extern.game_type))
         goto error;
   }
   else if (!g_extern.libretro_dummy)
   {
      fill_pathnames();

      if (!init_rom_file(g_extern.game_type))
         goto error;

      set_savestate_auto_index();


      if (!g_extern.sram_load_disable)
         load_save_files();
      else
         RARCH_LOG("Skipping SRAM load.\n");

      load_auto_state();

#ifdef HAVE_BSV_MOVIE
      init_movie();
#endif

#ifdef HAVE_NETPLAY
      init_netplay();
#endif
   }

   init_libretro_cbs();
   init_system_av_info();
   init_drivers();

#ifdef HAVE_COMMAND
   init_command();
#endif

#ifdef HAVE_NETPLAY
   if (!g_extern.netplay)
#endif
      rarch_init_rewind();
      
   init_controllers();
   
#ifdef HAVE_FFMPEG
   init_recording();
#endif

#ifdef HAVE_NETPLAY
   g_extern.use_sram = g_extern.use_sram && !g_extern.sram_save_disable && !g_extern.netplay_is_client;
#else
   g_extern.use_sram = g_extern.use_sram && !g_extern.sram_save_disable;
#endif

   if (!g_extern.use_sram)
      RARCH_LOG("SRAM will not be saved.\n");

#if defined(HAVE_THREADS)
   if (g_extern.use_sram)
      rarch_init_autosave();
#endif
      
#ifdef HAVE_NETPLAY
   allow_cheats &= !g_extern.netplay;
#endif
#ifdef HAVE_BSV_MOVIE
   allow_cheats &= !g_extern.bsv.movie;
#endif
   if (allow_cheats)
      init_cheats();

   g_extern.error_in_init = false;
   g_extern.main_is_init  = true;
   return 0;

error:
   uninit_drivers();
   pretro_unload_game();
   pretro_deinit();
   uninit_libretro_sym();

   g_extern.main_is_init = false;
   return 1;
}

static inline bool check_enter_rgui(void)
{
   static bool old_rmenu_toggle = true;

   // Always go into menu if dummy core is loaded.
   bool rmenu_toggle = input_key_pressed_func(RARCH_MENU_TOGGLE) || (g_extern.libretro_dummy && !old_rmenu_toggle);
   if (rmenu_toggle && !old_rmenu_toggle)
   {
      g_extern.lifecycle_state |= (1ULL << MODE_MENU_PREINIT);
      old_rmenu_toggle = true;
      g_extern.system.frame_time_last = 0;
      return true;
   }
   else
   {
      old_rmenu_toggle = rmenu_toggle;
      return false;
   }
}

static inline void update_frame_time(void)
{
   if (!g_extern.system.frame_time.callback)
      return;

   retro_time_t time = rarch_get_time_usec();
   retro_time_t delta = 0;

   bool is_locked_fps = g_extern.is_paused || driver.nonblock_state;
#ifdef HAVE_FFMPEG
   is_locked_fps |= g_extern.recording;
#endif
   
   if (!g_extern.system.frame_time_last || is_locked_fps)
      delta = g_extern.system.frame_time.reference;
   else
      delta = time - g_extern.system.frame_time_last;

   if (!is_locked_fps && g_extern.is_slowmotion)
      delta /= g_settings.slowmotion_ratio;

   g_extern.system.frame_time_last = is_locked_fps ? 0 : time;
   g_extern.system.frame_time.callback(delta);
}

static inline void limit_frame_time(void)
{
   if (g_settings.fastforward_ratio < 0.0f)
      return;

   retro_time_t current = rarch_get_time_usec();
   retro_time_t target = g_extern.frame_limit.last_frame_time + g_extern.frame_limit.minimum_frame_time;
   retro_time_t to_sleep_ms = (target - current) / 1000;
   if (to_sleep_ms > 0)
   {
      rarch_sleep((unsigned int)to_sleep_ms);
      g_extern.frame_limit.last_frame_time += g_extern.frame_limit.minimum_frame_time; // Combat jitter a bit.
   }
   else
      g_extern.frame_limit.last_frame_time = rarch_get_time_usec();
}

bool rarch_main_iterate(void)
{
#ifdef HAVE_DYLIB
   // DSP plugin GUI events.
   if (g_extern.audio_data.dsp_handle && g_extern.audio_data.dsp_plugin->events)
      g_extern.audio_data.dsp_plugin->events(g_extern.audio_data.dsp_handle);
#endif

   // SHUTDOWN on consoles should exit RetroArch completely.
   if (g_extern.system.shutdown)
      return false;

   // Time to drop?
   if (input_key_pressed_func(RARCH_QUIT_KEY) || !video_alive_func())
      return false;

   if (check_enter_rgui())
      return false; // Enter menu, don't exit.

   if (g_extern.exec)
   {
      g_extern.exec = false;
      return false;
   }

#ifdef HAVE_COMMAND
   if (driver.command)
      rarch_cmd_pre_frame(driver.command);
#endif

   // Checks for stuff like fullscreen, save states, etc.
   do_state_checks();

   // Run libretro for one frame.
#if defined(HAVE_THREADS)
   lock_autosave();
#endif

#ifdef HAVE_NETPLAY
   if (g_extern.netplay)
      netplay_pre_frame(g_extern.netplay);
#endif

#ifdef HAVE_BSV_MOVIE
   if (g_extern.bsv.movie)
      bsv_movie_set_frame_start(g_extern.bsv.movie);
#endif

#ifdef HAVE_CAMERA
   if (g_extern.system.camera_callback.caps)
      driver_camera_poll();
#endif

   update_frame_time();
   pretro_run();
   limit_frame_time();

#ifdef HAVE_BSV_MOVIE
   if (g_extern.bsv.movie)
      bsv_movie_set_frame_end(g_extern.bsv.movie);
#endif

#ifdef HAVE_NETPLAY
   if (g_extern.netplay)
      netplay_post_frame(g_extern.netplay);
#endif

#if defined(HAVE_THREADS)
   unlock_autosave();
#endif

   return true;
}

void rarch_main_deinit(void)
{
#ifdef HAVE_NETPLAY
   deinit_netplay();
#endif
#ifdef HAVE_COMMAND
   deinit_command();
#endif

#if defined(HAVE_THREADS)
   if (g_extern.use_sram)
      rarch_deinit_autosave();
#endif

#ifdef HAVE_FFMPEG
   deinit_recording();
#endif

   if (g_extern.use_sram)
      save_files();

#ifdef HAVE_NETPLAY
   if (!g_extern.netplay)
#endif
      rarch_deinit_rewind();

   deinit_cheats();

#ifdef HAVE_BSV_MOVIE
   deinit_movie();
#endif

   if (!g_extern.libretro_dummy && !g_extern.libretro_no_rom)
      save_auto_state();

   uninit_drivers();
   pretro_unload_game();
   pretro_deinit();
   uninit_libretro_sym();

   if (g_extern.rom_file_temporary)
   {
      RARCH_LOG("Removing temporary ROM file: %s.\n", g_extern.last_rom);
      if (remove(g_extern.last_rom) < 0)
         RARCH_ERR("Failed to remove temporary file: %s.\n", g_extern.last_rom);
      g_extern.rom_file_temporary = false;
   }

   g_extern.main_is_init = false;
}

#define MAX_ARGS 32

int rarch_main_init_wrap(const struct rarch_main_wrap *args)
{
   unsigned i;
   if (g_extern.main_is_init)
      rarch_main_deinit();

   int argc = 0;
   char *argv[MAX_ARGS] = {NULL};
   char *argv_copy[MAX_ARGS];

   argv[argc++] = strdup("retroarch");

   if (!args->no_rom)
   {
      if (args->rom_path)
      {
         RARCH_LOG("Using ROM: %s.\n", args->rom_path);
         argv[argc++] = strdup(args->rom_path);
      }
      else
      {
         RARCH_LOG("No ROM, starting dummy core.\n");
         argv[argc++] = strdup("--menu");
      }
   }

   if (args->sram_path)
   {
      argv[argc++] = strdup("-s");
      argv[argc++] = strdup(args->sram_path);
   }

   if (args->state_path)
   {
      argv[argc++] = strdup("-S");
      argv[argc++] = strdup(args->state_path);
   }

   if (args->config_path)
   {
      argv[argc++] = strdup("-c");
      argv[argc++] = strdup(args->config_path);
   }

#ifdef HAVE_DYNAMIC
   if (args->libretro_path)
   {
      argv[argc++] = strdup("-L");
      argv[argc++] = strdup(args->libretro_path);
   }
#endif

   if (args->verbose)
      argv[argc++] = strdup("-v");

#ifdef HAVE_FILE_LOGGER
   for (i = 0; i < argc; i++)
      RARCH_LOG("arg #%d: %s\n", i, argv[i]);
#endif

   // The pointers themselves are not const, and can be messed around with by getopt_long().
   memcpy(argv_copy, argv, sizeof(argv));

   int ret = rarch_main_init(argc, argv);

   for (i = 0; i < ARRAY_SIZE(argv_copy); i++)
      free(argv_copy[i]);

   return ret;
}

bool rarch_main_idle_iterate(void)
{
#ifdef HAVE_COMMAND
   if (driver.command)
      rarch_cmd_pre_frame(driver.command);
#endif

   if (input_key_pressed_func(RARCH_QUIT_KEY) || !video_alive_func())
      return false;

   do_state_checks();
   rarch_input_poll();
   rarch_sleep(10);
   return true;
}
