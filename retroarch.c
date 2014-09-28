/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2012-2014 - Michael Lelli
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
#include "retro.h"
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
#include "input/keyboard_line.h"
#include "input/input_common.h"
#include "git_version.h"
#include "intl/intl.h"

#ifdef HAVE_MENU
#include "frontend/menu/menu_common.h"
#include "frontend/menu/menu_shader.h"
#include "frontend/menu/menu_input_line_cb.h"
#endif

#ifdef _WIN32
#ifdef _XBOX
#include <xtl.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "msvc/msvc_compat.h"
#endif

/* To avoid continous switching if we hold the button down, we require
 * that the button must go from pressed to unpressed back to pressed 
 * to be able to toggle between then.
 */
static void check_fast_forward_button(bool fastforward_pressed,
      bool hold_pressed, bool old_hold_pressed)
{
   if (fastforward_pressed)
   {
      driver.nonblock_state = !driver.nonblock_state;
      driver_set_nonblock_state(driver.nonblock_state);
   }
   else if (old_hold_pressed != hold_pressed)
   {
      driver.nonblock_state = hold_pressed;
      driver_set_nonblock_state(driver.nonblock_state);
   }
}

static bool take_screenshot_viewport(void)
{
   char screenshot_path[PATH_MAX];
   const char *screenshot_dir = NULL;
   uint8_t *buffer = NULL;
   bool retval = false;
   struct rarch_viewport vp = {0};

   if (driver.video && driver.video->viewport_info)
      driver.video->viewport_info(driver.video_data, &vp);

   if (!vp.width || !vp.height)
      return false;

   if (!(buffer = (uint8_t*)malloc(vp.width * vp.height * 3)))
      return false;

   if (driver.video && driver.video->read_viewport)
      if (!driver.video->read_viewport(driver.video_data, buffer))
         goto done;

   screenshot_dir = g_settings.screenshot_directory;

   if (!*g_settings.screenshot_directory)
   {
      fill_pathname_basedir(screenshot_path, g_extern.basename,
            sizeof(screenshot_path));
      screenshot_dir = screenshot_path;
   }

   /* Data read from viewport is in bottom-up order, suitable for BMP. */
   if (!screenshot_dump(screenshot_dir, buffer, vp.width, vp.height,
            vp.width * 3, true))
      goto done;

   retval = true;

done:
   if (buffer)
      free(buffer);
   return retval;
}

static bool take_screenshot_raw(void)
{
   char screenshot_path[PATH_MAX];
   const void *data           = g_extern.frame_cache.data;
   unsigned width             = g_extern.frame_cache.width;
   unsigned height            = g_extern.frame_cache.height;
   int pitch                  = g_extern.frame_cache.pitch;
   const char *screenshot_dir = g_settings.screenshot_directory;

   if (!*g_settings.screenshot_directory)
   {
      fill_pathname_basedir(screenshot_path, g_extern.basename,
            sizeof(screenshot_path));
      screenshot_dir = screenshot_path;
   }

   /* Negative pitch is needed as screenshot takes bottom-up,
    * but we use top-down.
    */
   return screenshot_dump(screenshot_dir,
         (const uint8_t*)data + (height - 1) * pitch,
         width, height, -pitch, false);
}

static void take_screenshot(void)
{
   bool viewport_read = false;
   bool ret = false;
   const char *msg = NULL;

   /* No way to infer screenshot directory. */
   if ((!*g_settings.screenshot_directory) && (!*g_extern.basename))
      return;

   viewport_read = (g_settings.video.gpu_screenshot ||
         g_extern.system.hw_render_callback.context_type
         != RETRO_HW_CONTEXT_NONE) && driver.video->read_viewport &&
      driver.video->viewport_info;

   /* Clear out message queue to avoid OSD fonts to appear on screenshot. */
   msg_queue_clear(g_extern.msg_queue);

   if (viewport_read)
   {
      /* Avoid taking screenshot of GUI overlays. */
      if (driver.video_poke && driver.video_poke->set_texture_enable)
         driver.video_poke->set_texture_enable(driver.video_data,
               false, false);

      if (driver.video)
         rarch_render_cached_frame();
   }

   if (viewport_read)
      ret = take_screenshot_viewport();
   else if (g_extern.frame_cache.data &&
         (g_extern.frame_cache.data != RETRO_HW_FRAME_BUFFER_VALID))
      ret = take_screenshot_raw();
   else
      RARCH_ERR(RETRO_LOG_TAKE_SCREENSHOT_ERROR);

   if (ret)
   {
      RARCH_LOG(RETRO_LOG_TAKE_SCREENSHOT);
      msg = RETRO_MSG_TAKE_SCREENSHOT;
   }
   else
   {
      RARCH_WARN(RETRO_LOG_TAKE_SCREENSHOT_FAILED);
      msg = RETRO_MSG_TAKE_SCREENSHOT_FAILED;
   }

   msg_queue_push(g_extern.msg_queue, msg, 1, g_extern.is_paused ? 1 : 180);

   if (g_extern.is_paused)
      rarch_render_cached_frame();
}

static void readjust_audio_input_rate(void)
{
   int avail = driver.audio->write_avail(driver.audio_data);

   //RARCH_LOG_OUTPUT("Audio buffer is %u%% full\n",
   //      (unsigned)(100 - (avail * 100) / g_extern.audio_data.driver_buffer_size));

   unsigned write_index = g_extern.measure_data.buffer_free_samples_count++ &
      (AUDIO_BUFFER_FREE_SAMPLES_COUNT - 1);
   int      half_size   = g_extern.audio_data.driver_buffer_size / 2;
   int      delta_mid   = avail - half_size;
   double   direction   = (double)delta_mid / half_size;
   double   adjust      = 1.0 + g_settings.audio.rate_control_delta *
      direction;

   g_extern.measure_data.buffer_free_samples[write_index] = avail;
   g_extern.audio_data.src_ratio = g_extern.audio_data.orig_src_ratio * adjust;

   //RARCH_LOG_OUTPUT("New rate: %lf, Orig rate: %lf\n",
   //      g_extern.audio_data.src_ratio, g_extern.audio_data.orig_src_ratio);
}

void rarch_deinit_gpu_recording(void)
{
   if (g_extern.record_gpu_buffer)
      free(g_extern.record_gpu_buffer);
   g_extern.record_gpu_buffer = NULL;
}

void rarch_recording_dump_frame(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   struct ffemu_video_data ffemu_data = {0};

   ffemu_data.pitch   = pitch;
   ffemu_data.width   = width;
   ffemu_data.height  = height;
   ffemu_data.data    = data;

   if (g_extern.record_gpu_buffer)
   {
      struct rarch_viewport vp = {0};

      if (driver.video && driver.video->viewport_info)
         driver.video->viewport_info(driver.video_data, &vp);

      if (!vp.width || !vp.height)
      {
         RARCH_WARN("Viewport size calculation failed! Will continue using raw data. This will probably not work right ...\n");
         rarch_deinit_gpu_recording();

         rarch_recording_dump_frame(data, width, height, pitch);
         return;
      }

      /* User has resized. We kinda have a problem now. */
      if (vp.width != g_extern.record_gpu_width ||
            vp.height != g_extern.record_gpu_height)
      {
         static const char msg[] = "Recording terminated due to resize.";
         RARCH_WARN("%s\n", msg);
         msg_queue_clear(g_extern.msg_queue);
         msg_queue_push(g_extern.msg_queue, msg, 1, 180);

         rarch_deinit_recording();
         return;
      }

      /* Big bottleneck.
       * Since we might need to do read-backs asynchronously,
       * it might take 3-4 times before this returns true. */
      if (driver.video && driver.video->read_viewport)
         if (!driver.video->read_viewport(driver.video_data,
                  g_extern.record_gpu_buffer))
            return;

      ffemu_data.pitch  = g_extern.record_gpu_width * 3;
      ffemu_data.width  = g_extern.record_gpu_width;
      ffemu_data.height = g_extern.record_gpu_height;
      ffemu_data.data   = g_extern.record_gpu_buffer +
         (ffemu_data.height - 1) * ffemu_data.pitch;

      ffemu_data.pitch  = -ffemu_data.pitch;
   }

   if (!g_extern.record_gpu_buffer)
      ffemu_data.is_dupe = !data;

   if (g_extern.rec_driver && g_extern.rec_driver->push_video)
      g_extern.rec_driver->push_video(g_extern.rec, &ffemu_data);
}

static void init_recording(void)
{
   struct ffemu_params params = {0};
   const struct retro_system_av_info *info = {0};

   if (!g_extern.recording_enable)
      return;

   if (g_extern.libretro_dummy)
   {
      RARCH_WARN(RETRO_LOG_INIT_RECORDING_SKIPPED);
      return;
   }

   if (!g_settings.video.gpu_record
         && g_extern.system.hw_render_callback.context_type)
   {
      RARCH_WARN("Libretro core is hardware rendered. Must use post-shaded recording as well.\n");
      return;
   }

   RARCH_LOG("Custom timing given: FPS: %.4f, Sample rate: %.4f\n",
         (float)g_extern.system.av_info.timing.fps,
         (float)g_extern.system.av_info.timing.sample_rate);

   info = (const struct retro_system_av_info*)&g_extern.system.av_info;
   params.out_width  = info->geometry.base_width;
   params.out_height = info->geometry.base_height;
   params.fb_width   = info->geometry.max_width;
   params.fb_height  = info->geometry.max_height;
   params.channels   = 2;
   params.filename   = g_extern.record_path;
   params.fps        = g_extern.system.av_info.timing.fps;
   params.samplerate = g_extern.system.av_info.timing.sample_rate;
   params.pix_fmt    = (g_extern.system.pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888) ?
      FFEMU_PIX_ARGB8888 : FFEMU_PIX_RGB565;
   params.config     = NULL;
   
   if (*g_extern.record_config)
      params.config = g_extern.record_config;

   if (g_settings.video.gpu_record && driver.video->read_viewport)
   {
      struct rarch_viewport vp = {0};
      if (driver.video && driver.video->viewport_info)
         driver.video->viewport_info(driver.video_data, &vp);

      if (!vp.width || !vp.height)
      {
         RARCH_ERR("Failed to get viewport information from video driver. "
               "Cannot start recording ...\n");
         return;
      }

      params.out_width  = vp.width;
      params.out_height = vp.height;
      params.fb_width   = next_pow2(vp.width);
      params.fb_height  = next_pow2(vp.height);

      if (g_settings.video.force_aspect &&
            (g_extern.system.aspect_ratio > 0.0f))
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

      if (g_settings.video.force_aspect &&
            (g_extern.system.aspect_ratio > 0.0f))
         params.aspect_ratio = g_extern.system.aspect_ratio;
      else
         params.aspect_ratio = (float)params.out_width / params.out_height;

      if (g_settings.video.post_filter_record && g_extern.filter.filter)
      {
         unsigned max_width  = 0;
         unsigned max_height = 0;

         if (g_extern.filter.out_rgb32)
            params.pix_fmt = FFEMU_PIX_ARGB8888;
         else
            params.pix_fmt =  FFEMU_PIX_RGB565;

         rarch_softfilter_get_max_output_size(g_extern.filter.filter,
               &max_width, &max_height);
         params.fb_width  = next_pow2(max_width);
         params.fb_height = next_pow2(max_height);
      }
   }

   RARCH_LOG("Recording to %s @ %ux%u. (FB size: %ux%u pix_fmt: %u)\n",
         g_extern.record_path,
         params.out_width, params.out_height,
         params.fb_width, params.fb_height,
         (unsigned)params.pix_fmt);

   if (!ffemu_init_first(&g_extern.rec_driver, &g_extern.rec, &params))
   {
      RARCH_ERR(RETRO_LOG_INIT_RECORDING_FAILED);
      rarch_deinit_gpu_recording();
   }
}

void rarch_deinit_recording(void)
{
   if (!g_extern.rec || !g_extern.rec_driver)
      return;

   if (g_extern.rec_driver->finalize)
      g_extern.rec_driver->finalize(g_extern.rec);
   if (g_extern.rec_driver->free)
      g_extern.rec_driver->free(g_extern.rec);

   g_extern.rec = NULL;
   g_extern.rec_driver = NULL;

   rarch_deinit_gpu_recording();
}

void rarch_render_cached_frame(void)
{
   const void *frame = g_extern.frame_cache.data;
   void *recording   = g_extern.rec;

   /* Cannot allow recording when pushing duped frames. */
   g_extern.rec = NULL;

   if (frame == RETRO_HW_FRAME_BUFFER_VALID)
      frame = NULL; /* Dupe */

   /* Not 100% safe, since the library might have
    * freed the memory, but no known implementations do this.
    * It would be really stupid at any rate ...
    */
   if (driver.retro_ctx.frame_cb)
   driver.retro_ctx.frame_cb(frame,
         g_extern.frame_cache.width,
         g_extern.frame_cache.height,
         g_extern.frame_cache.pitch);

   g_extern.rec = recording;
}

bool rarch_audio_flush(const int16_t *data, size_t samples)
{
   const void *output_data        = NULL;
   unsigned output_frames         = 0;
   size_t   output_size           = sizeof(float);
   struct resampler_data src_data = {0};
   struct rarch_dsp_data dsp_data = {0};

   if (g_extern.rec)
   {
      struct ffemu_audio_data ffemu_data = {0};
      ffemu_data.data                    = data;
      ffemu_data.frames                  = samples / 2;

      if (g_extern.rec_driver && g_extern.rec_driver->push_audio)
         g_extern.rec_driver->push_audio(g_extern.rec, &ffemu_data);
   }

   if (g_extern.is_paused || g_extern.audio_data.mute)
      return true;
   if (!g_extern.audio_active)
      return false;

   RARCH_PERFORMANCE_INIT(audio_convert_s16);
   RARCH_PERFORMANCE_START(audio_convert_s16);
   audio_convert_s16_to_float(g_extern.audio_data.data, data, samples,
         g_extern.audio_data.volume_gain);
   RARCH_PERFORMANCE_STOP(audio_convert_s16);

   dsp_data.input                 = g_extern.audio_data.data;
   dsp_data.input_frames          = samples >> 1;

   if (g_extern.audio_data.dsp)
   {
      RARCH_PERFORMANCE_INIT(audio_dsp);
      RARCH_PERFORMANCE_START(audio_dsp);
      rarch_dsp_filter_process(g_extern.audio_data.dsp, &dsp_data);
      RARCH_PERFORMANCE_STOP(audio_dsp);
   }

   src_data.data_in      = dsp_data.output ?
      dsp_data.output : g_extern.audio_data.data;
   src_data.input_frames = dsp_data.output ?
      dsp_data.output_frames : (samples >> 1);

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

   if (!g_extern.audio_data.use_float)
   {
      RARCH_PERFORMANCE_INIT(audio_convert_float);
      RARCH_PERFORMANCE_START(audio_convert_float);
      audio_convert_float_to_s16(g_extern.audio_data.conv_outsamples,
            (const float*)output_data, output_frames * 2);
      RARCH_PERFORMANCE_STOP(audio_convert_float);

      output_data = g_extern.audio_data.conv_outsamples;
      output_size = sizeof(int16_t);
   }

   if (driver.audio->write(driver.audio_data, output_data,
            output_frames * output_size * 2) < 0)
   {
      RARCH_ERR(RETRO_LOG_AUDIO_WRITE_FAILED);
      return false;
   }

   return true;
}

#include "config.features.h"

#define _PSUPP(var, name, desc) printf("\t%s:\n\t\t%s: %s\n", name, desc, _##var##_supp ? "yes" : "no")
static void print_features(void)
{
   puts("");
   puts("Features:");
   _PSUPP(sdl, "SDL", "SDL drivers");
   _PSUPP(sdl2, "SDL2", "SDL2 drivers");
   _PSUPP(x11, "X11", "X11 drivers");
   _PSUPP(wayland, "wayland", "Wayland drivers");
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
   fprintf(file, "MSVC (%d) %u-bit\n", _MSC_VER, (unsigned)
         (CHAR_BIT * sizeof(size_t)));
#elif defined(__SNC__)
   fprintf(file, "SNC (%d) %u-bit\n",
      __SN_VER__, (unsigned)(CHAR_BIT * sizeof(size_t)));
#elif defined(_WIN32) && defined(__GNUC__)
   fprintf(file, "MinGW (%d.%d.%d) %u-bit\n",
      __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, (unsigned)
      (CHAR_BIT * sizeof(size_t)));
#elif defined(__clang__)
   fprintf(file, "Clang/LLVM (%s) %u-bit\n",
      __clang_version__, (unsigned)(CHAR_BIT * sizeof(size_t)));
#elif defined(__GNUC__)
   fprintf(file, "GCC (%d.%d.%d) %u-bit\n",
      __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__, (unsigned)
      (CHAR_BIT * sizeof(size_t)));
#else
   fprintf(file, "Unknown compiler %u-bit\n",
      (unsigned)(CHAR_BIT * sizeof(size_t)));
#endif
   fprintf(file, "Built: %s\n", __DATE__);
}

static void print_help(void)
{
   puts("===================================================================");
#ifdef HAVE_GIT_VERSION
   printf(RETRO_FRONTEND ": Frontend for libretro -- v" PACKAGE_VERSION " -- %s --\n", rarch_git_version);
#else
   puts(RETRO_FRONTEND ": Frontend for libretro -- v" PACKAGE_VERSION " --");
#endif
   print_compiler(stdout);
   puts("===================================================================");
   puts("Usage: retroarch [content file] [options...]");
   puts("\t-h/--help: Show this help message.");
   puts("\t--menu: Do not require content or libretro core to be loaded, starts directly in menu.");
   puts("\t\tIf no arguments are passed to " RETRO_FRONTEND ", it is equivalent to using --menu as only argument.");
   puts("\t--features: Prints available features compiled into " RETRO_FRONTEND ".");
   puts("\t-s/--save: Path for save file (*.srm).");
   puts("\t-f/--fullscreen: Start " RETRO_FRONTEND " in fullscreen regardless of config settings.");
   puts("\t-S/--savestate: Path to use for save states. If not selected, *.state will be assumed.");
   puts("\t-c/--config: Path for config file." RARCH_DEFAULT_CONF_PATH_STR);
   puts("\t--appendconfig: Extra config files are loaded in, and take priority over config selected in -c (or default).");
   puts("\t\tMultiple configs are delimited by ','.");
#ifdef HAVE_DYNAMIC
   puts("\t-L/--libretro: Path to libretro implementation. Overrides any config setting.");
#endif
   puts("\t--subsystem: Use a subsystem of the libretro core. Multiple content files are loaded as multiple arguments.");
   puts("\t\tIf a content file is skipped, use a blank (\"\") command line argument");
   puts("\t\tContent must be loaded in an order which depends on the particular subsystem used.");
   puts("\t\tSee verbose log output to learn how a particular subsystem wants content to be loaded.");

   printf("\t-N/--nodevice: Disconnects controller device connected to port (1 to %d).\n", MAX_PLAYERS);
   printf("\t-A/--dualanalog: Connect a DualAnalog controller to port (1 to %d).\n", MAX_PLAYERS);
   printf("\t-d/--device: Connect a generic device into port of the device (1 to %d).\n", MAX_PLAYERS);
   puts("\t\tFormat is port:ID, where ID is an unsigned number corresponding to the particular device.\n");

   puts("\t-P/--bsvplay: Playback a BSV movie file.");
   puts("\t-R/--bsvrecord: Start recording a BSV movie file from the beginning.");
   puts("\t-M/--sram-mode: Takes an argument telling how SRAM should be handled in the session.");
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
#endif
   puts("\t--nick: Picks a username (for use with netplay). Not mandatory.");
#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
   puts("\t--command: Sends a command over UDP to an already running " RETRO_FRONTEND " process.");
   puts("\t\tAvailable commands are listed if command is invalid.");
#endif

   puts("\t-r/--record: Path to record video file.\n\t\tUsing .mkv extension is recommended.");
   puts("\t--recordconfig: Path to settings used during recording.");
   puts("\t--size: Overrides output video size when recording (format: WIDTHxHEIGHT).");
   puts("\t-v/--verbose: Verbose logging.");
   puts("\t-U/--ups: Specifies path for UPS patch that will be applied to content.");
   puts("\t--bps: Specifies path for BPS patch that will be applied to content.");
   puts("\t--ips: Specifies path for IPS patch that will be applied to content.");
   puts("\t--no-patch: Disables all forms of content patching.");
   puts("\t-D/--detach: Detach " RETRO_FRONTEND " from the running console. Not relevant for all platforms.\n");
}

static void set_basename(const char *path)
{
   char *dst = NULL;

   strlcpy(g_extern.fullpath, path, sizeof(g_extern.fullpath));
   strlcpy(g_extern.basename, path, sizeof(g_extern.basename));
   /* Removing extension is a bit tricky for compressed files.
    * Basename means:
    * /file/to/path/game.extension should be:
    * /file/to/path/game
    *
    * Two things to consider here are: /file/to/path/ is expected
    * to be a directory and "game" is a single file. This is used for
    * states and srm default paths
    *
    * For compressed files we have:
    *
    * /file/to/path/comp.7z#game.extension and
    * /file/to/path/comp.7z#folder/game.extension
    *
    * The choice I take here is:
    * /file/to/path/game as basename. We might end up in a writable dir then
    * and the name of srm and states are meaningful.
    *
    */
#ifdef HAVE_COMPRESSION
   path_basedir(g_extern.basename);
   fill_pathname_dir(g_extern.basename,path,"",sizeof(g_extern.basename));
#endif

   if ((dst = strrchr(g_extern.basename, '.')))
      *dst = '\0';
}

static void set_special_paths(char **argv, unsigned num_content)
{
   unsigned i;
   union string_list_elem_attr attr;

   /* First content file is the significant one. */
   set_basename(argv[0]);

   g_extern.subsystem_fullpaths = string_list_new();
   rarch_assert(g_extern.subsystem_fullpaths);

   attr.i = 0;

   for (i = 0; i < num_content; i++)
      string_list_append(g_extern.subsystem_fullpaths, argv[i], attr);

   /* We defer SRAM path updates until we can resolve it.
    * It is more complicated for special content types. */

   if (!g_extern.has_set_state_path)
      fill_pathname_noext(g_extern.savestate_name, g_extern.basename,
            ".state", sizeof(g_extern.savestate_name));

   if (path_is_directory(g_extern.savestate_name))
   {
      fill_pathname_dir(g_extern.savestate_name, g_extern.basename,
            ".state", sizeof(g_extern.savestate_name));
      RARCH_LOG("Redirecting save state to \"%s\".\n",
            g_extern.savestate_name);
   }

   /* If this is already set,
    * do not overwrite it as this was initialized before in
    * a menu or otherwise. */
   if (!*g_settings.system_directory)
      fill_pathname_basedir(g_settings.system_directory, argv[0],
            sizeof(g_settings.system_directory));
}

static void set_paths(const char *path)
{
   set_basename(path);

   if (!g_extern.has_set_save_path)
      fill_pathname_noext(g_extern.savefile_name, g_extern.basename,
            ".srm", sizeof(g_extern.savefile_name));
   if (!g_extern.has_set_state_path)
      fill_pathname_noext(g_extern.savestate_name, g_extern.basename,
            ".state", sizeof(g_extern.savestate_name));

   if (path_is_directory(g_extern.savefile_name))
   {
      fill_pathname_dir(g_extern.savefile_name, g_extern.basename,
            ".srm", sizeof(g_extern.savefile_name));
      RARCH_LOG("Redirecting save file to \"%s\".\n", g_extern.savefile_name);
   }
   if (path_is_directory(g_extern.savestate_name))
   {
      fill_pathname_dir(g_extern.savestate_name, g_extern.basename,
            ".state", sizeof(g_extern.savestate_name));
      RARCH_LOG("Redirecting save state to \"%s\".\n", g_extern.savestate_name);
   }

   /* If this is already set, do not overwrite it
    * as this was initialized before in a menu or otherwise. */
   if (!*g_settings.system_directory)
      fill_pathname_basedir(g_settings.system_directory, path,
            sizeof(g_settings.system_directory));
}

static void parse_input(int argc, char *argv[])
{
   g_extern.libretro_no_content = false;
   g_extern.libretro_dummy = false;
   g_extern.has_set_save_path = false;
   g_extern.has_set_state_path = false;
   g_extern.has_set_libretro = false;
   g_extern.has_set_libretro_directory = false;
   g_extern.has_set_verbosity = false;

   g_extern.has_set_netplay_mode = false;
   g_extern.has_set_username = false;
   g_extern.has_set_netplay_ip_address = false;
   g_extern.has_set_netplay_delay_frames = false;
   g_extern.has_set_netplay_ip_port = false;

   g_extern.ups_pref = false;
   g_extern.bps_pref = false;
   g_extern.ips_pref = false;
   *g_extern.ups_name = '\0';
   *g_extern.bps_name = '\0';
   *g_extern.ips_name = '\0';

   *g_extern.subsystem = '\0';

   if (argc < 2)
   {
      g_extern.libretro_dummy = true;
      return;
   }

   /* Make sure we can call parse_input several times ... */
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
      { "record", 1, NULL, 'r' },
      { "recordconfig", 1, &val, 'R' },
      { "size", 1, &val, 's' },
      { "verbose", 0, NULL, 'v' },
      { "config", 1, NULL, 'c' },
      { "appendconfig", 1, &val, 'C' },
      { "nodevice", 1, NULL, 'N' },
      { "dualanalog", 1, NULL, 'A' },
      { "device", 1, NULL, 'd' },
      { "savestate", 1, NULL, 'S' },
      { "bsvplay", 1, NULL, 'P' },
      { "bsvrecord", 1, NULL, 'R' },
      { "sram-mode", 1, NULL, 'M' },
#ifdef HAVE_NETPLAY
      { "host", 0, NULL, 'H' },
      { "connect", 1, NULL, 'C' },
      { "frames", 1, NULL, 'F' },
      { "port", 1, &val, 'p' },
      { "spectate", 0, &val, 'S' },
#endif
      { "nick", 1, &val, 'N' },
#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
      { "command", 1, &val, 'c' },
#endif
      { "ups", 1, NULL, 'U' },
      { "bps", 1, &val, 'B' },
      { "ips", 1, &val, 'I' },
      { "no-patch", 0, &val, 'n' },
      { "detach", 0, NULL, 'D' },
      { "features", 0, &val, 'f' },
      { "subsystem", 1, NULL, 'Z' },
      { NULL, 0, NULL, 0 }
   };

#define FFMPEG_RECORD_ARG "r:"

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


#define BSV_MOVIE_ARG "P:R:M:"

   const char *optstring = "hs:fvS:A:c:U:DN:d:" BSV_MOVIE_ARG NETPLAY_ARG DYNAMIC_ARG FFMPEG_RECORD_ARG;

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

         case 'Z':
            strlcpy(g_extern.subsystem, optarg, sizeof(g_extern.subsystem));
            break;

         case 'd':
         {
            unsigned id = 0;
            port = 0;
            struct string_list *list = string_split(optarg, ":");
            if (list && list->size == 2)
            {
               port = strtol(list->elems[0].data, NULL, 0);
               id = strtoul(list->elems[1].data, NULL, 0);
            }
            string_list_free(list);

            if (port < 1 || port > MAX_PLAYERS)
            {
               RARCH_ERR("Connect device to a valid port.\n");
               print_help();
               rarch_fail(1, "parse_input()");
            }
            g_settings.input.libretro_device[port - 1] = id;
            g_extern.has_set_libretro_device[port - 1] = true;
            break;
         }

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
            strlcpy(g_extern.savefile_name, optarg,
                  sizeof(g_extern.savefile_name));
            g_extern.has_set_save_path = true;
            break;

         case 'f':
            g_extern.force_fullscreen = true;
            break;

         case 'S':
            strlcpy(g_extern.savestate_name, optarg,
                  sizeof(g_extern.savestate_name));
            g_extern.has_set_state_path = true;
            break;

         case 'v':
            g_extern.verbosity = true;
            g_extern.has_set_verbosity = true;
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

         case 'c':
            strlcpy(g_extern.config_path, optarg,
                  sizeof(g_extern.config_path));
            break;

         case 'r':
            strlcpy(g_extern.record_path, optarg,
                  sizeof(g_extern.record_path));
            g_extern.recording_enable = true;
            break;

#ifdef HAVE_DYNAMIC
         case 'L':
            if (path_is_directory(optarg))
            {
               *g_settings.libretro = '\0';
               strlcpy(g_settings.libretro_directory, optarg,
                     sizeof(g_settings.libretro_directory));
               g_extern.has_set_libretro = true;
               g_extern.has_set_libretro_directory = true;
               RARCH_WARN("Using old --libretro behavior. Setting libretro_directory to \"%s\" instead.\n", optarg);
            }
            else
            {
               strlcpy(g_settings.libretro, optarg,
                     sizeof(g_settings.libretro));
               g_extern.has_set_libretro = true;
            }
            break;
#endif
         case 'P':
         case 'R':
            strlcpy(g_extern.bsv.movie_start_path, optarg,
                  sizeof(g_extern.bsv.movie_start_path));
            g_extern.bsv.movie_start_playback  = (c == 'P');
            g_extern.bsv.movie_start_recording = (c == 'R');
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

#ifdef HAVE_NETPLAY
         case 'H':
            g_extern.has_set_netplay_ip_address = true;
            g_extern.netplay_enable = true;
            *g_extern.netplay_server = '\0';
            break;

         case 'C':
            g_extern.has_set_netplay_ip_address = true;
            g_extern.netplay_enable = true;
            strlcpy(g_extern.netplay_server, optarg,
                  sizeof(g_extern.netplay_server));
            break;

         case 'F':
            g_extern.netplay_sync_frames = strtol(optarg, NULL, 0);
            g_extern.has_set_netplay_delay_frames = true;
            break;
#endif

         case 'U':
            strlcpy(g_extern.ups_name, optarg,
                  sizeof(g_extern.ups_name));
            g_extern.ups_pref = true;
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
                  g_extern.has_set_netplay_ip_port = true;
                  g_extern.netplay_port = strtoul(optarg, NULL, 0);
                  break;

               case 'S':
                  g_extern.has_set_netplay_mode = true;
                  g_extern.netplay_is_spectate = true;
                  break;

#endif
               case 'N':
                  g_extern.has_set_username = true;
                  strlcpy(g_settings.username, optarg,
                        sizeof(g_settings.username));
                  break;

#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
               case 'c':
                  if (network_cmd_send(optarg))
                     exit(0);
                  else
                     rarch_fail(1, "network_cmd_send()");
                  break;
#endif

               case 'C':
                  strlcpy(g_extern.append_config_path, optarg,
                        sizeof(g_extern.append_config_path));
                  break;

               case 'B':
                  strlcpy(g_extern.bps_name, optarg,
                        sizeof(g_extern.bps_name));
                  g_extern.bps_pref = true;
                  break;

               case 'I':
                  strlcpy(g_extern.ips_name, optarg,
                        sizeof(g_extern.ips_name));
                  g_extern.ips_pref = true;
                  break;

               case 'n':
                  g_extern.block_patch = true;
                  break;

               case 's':
               {
                  if (sscanf(optarg, "%ux%u", &g_extern.record_width,
                           &g_extern.record_height) != 2)
                  {
                     RARCH_ERR("Wrong format for --size.\n");
                     print_help();
                     rarch_fail(1, "parse_input()");
                  }
                  break;
               }

               case 'R':
                  strlcpy(g_extern.record_config, optarg,
                        sizeof(g_extern.record_config));
                  break;
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
         RARCH_ERR("--menu was used, but content file was passed as well.\n");
         rarch_fail(1, "parse_input()");
      }
   }
   else if (!*g_extern.subsystem && optind < argc)
      set_paths(argv[optind]);
   else if (*g_extern.subsystem && optind < argc)
      set_special_paths(argv + optind, argc - optind);
   else
      g_extern.libretro_no_content = true;

   /* Copy SRM/state dirs used, so they can be reused on reentrancy. */
   if (g_extern.has_set_save_path &&
         path_is_directory(g_extern.savefile_name))
      strlcpy(g_extern.savefile_dir, g_extern.savefile_name,
            sizeof(g_extern.savefile_dir));
   if (g_extern.has_set_state_path &&
         path_is_directory(g_extern.savestate_name))
      strlcpy(g_extern.savestate_dir, g_extern.savestate_name,
            sizeof(g_extern.savestate_dir));
}

static void init_controllers(void)
{
   unsigned i;

   for (i = 0; i < MAX_PLAYERS; i++)
   {
      unsigned device = g_settings.input.libretro_device[i];
      const struct retro_controller_description *desc = NULL;
      const char *ident = NULL;

      if (i < g_extern.system.num_ports)
         desc = libretro_find_controller_description(
               &g_extern.system.ports[i], device);

      if (desc)
         ident = desc->desc;

      if (!ident)
      {
         /* If we're trying to connect a completely unknown device,
          * revert back to JOYPAD. */
         if (device != RETRO_DEVICE_JOYPAD && device != RETRO_DEVICE_NONE)
         {
            /* Do not fix g_settings.input.libretro_device[i],
             * because any use of dummy core will reset this,
             * which is not a good idea. */
            RARCH_WARN("Input device ID %u is unknown to this libretro implementation. Using RETRO_DEVICE_JOYPAD.\n", device);
            device = RETRO_DEVICE_JOYPAD;
         }
         ident = "Joypad";
      }

      if (device == RETRO_DEVICE_NONE)
      {
         RARCH_LOG("Disconnecting device from port %u.\n", i + 1);
         pretro_set_controller_port_device(i, device);
      }
      else if (device != RETRO_DEVICE_JOYPAD)
      {
         /* Some cores do not properly range check port argument.
          * This is broken behavior of course, but avoid breaking
          * cores needlessly. */
         RARCH_LOG("Connecting %s (ID: %u) to port %u.\n", ident,
               device, i + 1);
         pretro_set_controller_port_device(i, device);
      }
   }
}

static inline bool load_save_files(void)
{
   unsigned i;

   if (!g_extern.savefiles || g_extern.sram_load_disable)
      return false;

   for (i = 0; i < g_extern.savefiles->size; i++)
      load_ram_file(g_extern.savefiles->elems[i].data,
            g_extern.savefiles->elems[i].attr.i);
    
    return true;
}

static inline bool save_files(void)
{
   unsigned i;

   if (!g_extern.savefiles || !g_extern.use_sram)
      return false;

   for (i = 0; i < g_extern.savefiles->size; i++)
   {
      unsigned type    = g_extern.savefiles->elems[i].attr.i;
      const char *path = g_extern.savefiles->elems[i].data;
      RARCH_LOG("Saving RAM type #%u to \"%s\".\n", type, path);
      save_ram_file(path, type);
   }

   return true;
}


static void init_msg_queue(void)
{
   if (!g_extern.msg_queue)
      rarch_assert(g_extern.msg_queue = msg_queue_new(8));
}

void rarch_deinit_msg_queue(void)
{
   if (g_extern.msg_queue)
      msg_queue_free(g_extern.msg_queue);
   g_extern.msg_queue = NULL;
}

static void init_cheats(void)
{
   bool allow_cheats = true;
#ifdef HAVE_NETPLAY
   allow_cheats &= !g_extern.netplay;
#endif
   allow_cheats &= !g_extern.bsv.movie;

   if (!allow_cheats)
      return;

   if (*g_settings.cheat_database)
      g_extern.cheat = cheat_manager_new(g_settings.cheat_database);
}

static void deinit_cheats(void)
{
   if (g_extern.cheat)
      cheat_manager_free(g_extern.cheat);
   g_extern.cheat = NULL;
}

static void init_rewind(void)
{
   void *state = NULL;
#ifdef HAVE_NETPLAY
   if (g_extern.netplay)
      return;
#endif

   if (!g_settings.rewind_enable || g_extern.state_manager)
      return;

   if (g_extern.system.audio_callback.callback)
   {
      RARCH_ERR(RETRO_LOG_REWIND_INIT_FAILED_THREADED_AUDIO);
      return;
   }

   g_extern.state_size = pretro_serialize_size();
   if (!g_extern.state_size)
   {
      RARCH_ERR(RETRO_LOG_REWIND_INIT_FAILED_NO_SAVESTATES);
      return;
   }

   RARCH_LOG(RETRO_MSG_REWIND_INIT "%u MB\n",
         (unsigned)(g_settings.rewind_buffer_size / 1000000));

   g_extern.state_manager = state_manager_new(g_extern.state_size,
         g_settings.rewind_buffer_size);

   if (!g_extern.state_manager)
      RARCH_WARN(RETRO_LOG_REWIND_INIT_FAILED);

   state_manager_push_where(g_extern.state_manager, &state);
   pretro_serialize(state, g_extern.state_size);
   state_manager_push_do(g_extern.state_manager);
}

static void deinit_rewind(void)
{
#ifdef HAVE_NETPLAY
    if (g_extern.netplay)
        return;
#endif
    
   if (g_extern.state_manager)
      state_manager_free(g_extern.state_manager);
   g_extern.state_manager = NULL;
}

static void init_movie(void)
{
   if (g_extern.bsv.movie_start_playback)
   {
      if (!(g_extern.bsv.movie = bsv_movie_init(g_extern.bsv.movie_start_path,
                  RARCH_MOVIE_PLAYBACK)))
      {
         RARCH_ERR("Failed to load movie file: \"%s\".\n",
               g_extern.bsv.movie_start_path);
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

      msg_queue_clear(g_extern.msg_queue);
      if ((g_extern.bsv.movie = bsv_movie_init(g_extern.bsv.movie_start_path,
                  RARCH_MOVIE_RECORD)))
      {
         msg_queue_push(g_extern.msg_queue, msg, 1, 180);
         RARCH_LOG("Starting movie record to \"%s\".\n",
               g_extern.bsv.movie_start_path);
         g_settings.rewind_granularity = 1;
      }
      else
      {
         msg_queue_push(g_extern.msg_queue, "Failed to start movie record.", 1, 180);
         RARCH_ERR("Failed to start movie record.\n");
      }
   }
}

static void deinit_movie(void)
{
   if (g_extern.bsv.movie)
      bsv_movie_free(g_extern.bsv.movie);
   g_extern.bsv.movie = NULL;
}

#define RARCH_DEFAULT_PORT 55435

#ifdef HAVE_NETPLAY
static void init_netplay(void)
{
   struct retro_callbacks cbs = {0};

   if (!g_extern.netplay_enable)
      return;

   if (g_extern.bsv.movie_start_playback)
   {
      RARCH_WARN(RETRO_LOG_MOVIE_STARTED_INIT_NETPLAY_FAILED);
      return;
   }

   retro_set_default_callbacks(&cbs);

   if (*g_extern.netplay_server)
   {
      RARCH_LOG("Connecting to netplay host...\n");
      g_extern.netplay_is_client = true;
   }
   else
      RARCH_LOG("Waiting for client...\n");

   g_extern.netplay = netplay_new(
         g_extern.netplay_is_client ? g_extern.netplay_server : NULL,
         g_extern.netplay_port ? g_extern.netplay_port : RARCH_DEFAULT_PORT,
         g_extern.netplay_sync_frames, &cbs, g_extern.netplay_is_spectate,
         g_settings.username);

   if (!g_extern.netplay)
   {
      g_extern.netplay_is_client = false;
      RARCH_WARN(RETRO_LOG_INIT_NETPLAY_FAILED);

      if (g_extern.msg_queue)
         msg_queue_push(g_extern.msg_queue,
               RETRO_MSG_INIT_NETPLAY_FAILED,
               0, 180);
   }
}

static void deinit_netplay(void)
{
   if (g_extern.netplay)
      netplay_free(g_extern.netplay);
   g_extern.netplay = NULL;
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

   if (!(driver.command = rarch_cmd_new(g_settings.stdin_cmd_enable
               && !driver.stdin_claimed,
               g_settings.network_cmd_enable, g_settings.network_cmd_port)))
      RARCH_ERR("Failed to initialize command interface.\n");
}

static void deinit_command(void)
{
   if (driver.command)
      rarch_cmd_free(driver.command);
   driver.command = NULL;
}

#endif

#if defined(HAVE_THREADS)
static void init_autosave(void)
{
   unsigned i;

   if (g_settings.autosave_interval < 1 || !g_extern.savefiles)
      return;

   if (!(g_extern.autosave = (autosave_t**)calloc(g_extern.savefiles->size,
               sizeof(*g_extern.autosave))))
      return;

   g_extern.num_autosave = g_extern.savefiles->size;

   for (i = 0; i < g_extern.savefiles->size; i++)
   {
      const char *path = g_extern.savefiles->elems[i].data;
      unsigned    type = g_extern.savefiles->elems[i].attr.i;

      if (pretro_get_memory_size(type) > 0)
      {
         g_extern.autosave[i] = autosave_new(path,
               pretro_get_memory_data(type),
               pretro_get_memory_size(type),
               g_settings.autosave_interval);
         if (!g_extern.autosave[i])
            RARCH_WARN(RETRO_LOG_INIT_AUTOSAVE_FAILED);
      }
   }
}

static void deinit_autosave(void)
{
   unsigned i;
   for (i = 0; i < g_extern.num_autosave; i++)
      autosave_free(g_extern.autosave[i]);

   if (g_extern.autosave)
      free(g_extern.autosave);
   g_extern.autosave = NULL;

   g_extern.num_autosave = 0;
}
#endif

static void set_savestate_auto_index(void)
{
   char state_dir[PATH_MAX], state_base[PATH_MAX];
   size_t i;
   struct string_list *dir_list = NULL;
   unsigned max_index = 0;

   if (!g_settings.savestate_auto_index)
      return;

   /* Find the file in the same directory as g_extern.savestate_name
    * with the largest numeral suffix.
    *
    * E.g. /foo/path/content.state, will try to find
    * /foo/path/content.state%d, where %d is the largest number available.
    */

   fill_pathname_basedir(state_dir, g_extern.savestate_name,
         sizeof(state_dir));
   fill_pathname_base(state_base, g_extern.savestate_name,
         sizeof(state_base));

   if (!(dir_list = dir_list_new(state_dir, NULL, false)))
      return;

   for (i = 0; i < dir_list->size; i++)
   {
      char elem_base[PATH_MAX];
      const char *dir_elem = dir_list->elems[i].data;

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

   g_settings.state_slot = max_index;
   RARCH_LOG("Found last state slot: #%d\n", g_settings.state_slot);
}

static void deinit_savefiles(void)
{
   if (g_extern.savefiles)
      string_list_free(g_extern.savefiles);
   g_extern.savefiles = NULL;
}

static void fill_pathnames(void)
{
   deinit_savefiles();
   g_extern.savefiles = string_list_new();
   rarch_assert(g_extern.savefiles);

   if (*g_extern.subsystem)
   {
      /* For subsystems, we know exactly which RAM types are supported. */

      unsigned i, j;
      const struct retro_subsystem_info *info = 
         (const struct retro_subsystem_info*)libretro_find_subsystem_info(
               g_extern.system.special, g_extern.system.num_special,
               g_extern.subsystem);

      /* We'll handle this error gracefully later. */
      unsigned num_content = min(info ? info->num_roms : 0,
            g_extern.subsystem_fullpaths ?
            g_extern.subsystem_fullpaths->size : 0);

      bool use_sram_dir = path_is_directory(g_extern.savefile_name);

      for (i = 0; i < num_content; i++)
      {
         for (j = 0; j < info->roms[i].num_memory; j++)
         {
            union string_list_elem_attr attr;
            char path[PATH_MAX], ext[32];
            const struct retro_subsystem_memory_info *mem =
               (const struct retro_subsystem_memory_info*)
               &info->roms[i].memory[j];

            snprintf(ext, sizeof(ext), ".%s", mem->extension);

            if (use_sram_dir)
            {
               /* Redirect content fullpath to save directory. */
               strlcpy(path, g_extern.savefile_name, sizeof(path));
               fill_pathname_dir(path,
                     g_extern.subsystem_fullpaths->elems[i].data, ext,
                     sizeof(path));
            }
            else
            {
               fill_pathname(path, g_extern.subsystem_fullpaths->elems[i].data,
                     ext, sizeof(path));
            }

            attr.i = mem->type;
            string_list_append(g_extern.savefiles, path, attr);
         }
      }

      /* Let other relevant paths be inferred from the main SRAM location. */
      if (!g_extern.has_set_save_path)
         fill_pathname_noext(g_extern.savefile_name, g_extern.basename, ".srm",
               sizeof(g_extern.savefile_name));
      if (path_is_directory(g_extern.savefile_name))
      {
         fill_pathname_dir(g_extern.savefile_name, g_extern.basename, ".srm",
               sizeof(g_extern.savefile_name));
         RARCH_LOG("Redirecting save file to \"%s\".\n",
               g_extern.savefile_name);
      }
   }
   else
   {
      char savefile_name_rtc[PATH_MAX];
      union string_list_elem_attr attr;

      attr.i = RETRO_MEMORY_SAVE_RAM;
      string_list_append(g_extern.savefiles, g_extern.savefile_name, attr);

      /* Infer .rtc save path from save ram path. */
      attr.i = RETRO_MEMORY_RTC;
      fill_pathname(savefile_name_rtc,
            g_extern.savefile_name, ".rtc", sizeof(savefile_name_rtc));
      string_list_append(g_extern.savefiles, savefile_name_rtc, attr);
   }

   fill_pathname(g_extern.bsv.movie_path, g_extern.savefile_name, "",
         sizeof(g_extern.bsv.movie_path));

   if (*g_extern.basename)
   {
      if (!*g_extern.ups_name)
         fill_pathname_noext(g_extern.ups_name, g_extern.basename, ".ups",
               sizeof(g_extern.ups_name));
      if (!*g_extern.bps_name)
         fill_pathname_noext(g_extern.bps_name, g_extern.basename, ".bps",
               sizeof(g_extern.bps_name));
      if (!*g_extern.ips_name)
         fill_pathname_noext(g_extern.ips_name, g_extern.basename, ".ips",
               sizeof(g_extern.ips_name));
   }
}

static void load_auto_state(void)
{
   char savestate_name_auto[PATH_MAX];

#ifdef HAVE_NETPLAY
   if (g_extern.netplay_enable && !g_extern.netplay_is_spectate)
      return;
#endif

   if (!g_settings.savestate_auto_load)
      return;

   fill_pathname_noext(savestate_name_auto, g_extern.savestate_name,
         ".auto", sizeof(savestate_name_auto));

   if (path_file_exists(savestate_name_auto))
   {
      char msg[PATH_MAX];
      bool ret = load_state(savestate_name_auto);

      RARCH_LOG("Found auto savestate in: %s\n", savestate_name_auto);

      snprintf(msg, sizeof(msg), "Auto-loading savestate from \"%s\" %s.",
            savestate_name_auto, ret ? "succeeded" : "failed");
      msg_queue_push(g_extern.msg_queue, msg, 1, 180);
      RARCH_LOG("%s\n", msg);
   }
}

static bool save_auto_state(void)
{
   char savestate_name_auto[PATH_MAX];

   if (!g_settings.savestate_auto_save || g_extern.libretro_dummy ||
       g_extern.libretro_no_content)
       return false;

   fill_pathname_noext(savestate_name_auto, g_extern.savestate_name,
         ".auto", sizeof(savestate_name_auto));

   bool ret = save_state(savestate_name_auto);
   RARCH_LOG("Auto save state to \"%s\" %s.\n", savestate_name_auto, ret ?
         "succeeded" : "failed");
    
   return true;
}

/* Save or load state here. */

static void rarch_load_state(const char *path,
      char *msg, size_t sizeof_msg)
{
   if (load_state(path))
   {
      if (g_settings.state_slot < 0)
         snprintf(msg, sizeof_msg,
               "Loaded state from slot #-1 (auto).");
      else
         snprintf(msg, sizeof_msg,
               "Loaded state from slot #%d.", g_settings.state_slot);
   }
   else
      snprintf(msg, sizeof_msg,
            "Failed to load state from \"%s\".", path);
}

static void rarch_save_state(const char *path,
      char *msg, size_t sizeof_msg)
{
   if (save_state(path))
   {
      if (g_settings.state_slot < 0)
         snprintf(msg, sizeof_msg,
               "Saved state to slot #-1 (auto).");
      else
         snprintf(msg, sizeof_msg,
               "Saved state to slot #%d.", g_settings.state_slot);
   }
   else
      snprintf(msg, sizeof_msg,
            "Failed to save state to \"%s\".", path);
}

static void main_state(unsigned cmd)
{
   char path[PATH_MAX], msg[PATH_MAX];

   if (g_settings.state_slot > 0)
      snprintf(path, sizeof(path), "%s%d",
            g_extern.savestate_name, g_settings.state_slot);
   else if (g_settings.state_slot < 0)
      snprintf(path, sizeof(path), "%s.auto",
            g_extern.savestate_name);
   else
      strlcpy(path, g_extern.savestate_name, sizeof(path));

   if (pretro_serialize_size())
   {
      if (cmd == RARCH_CMD_SAVE_STATE)
         rarch_save_state(path, msg, sizeof(msg));
      else if (cmd == RARCH_CMD_LOAD_STATE)
         rarch_load_state(path, msg, sizeof(msg));
   }
   else
      strlcpy(msg, "Core does not support save states.", sizeof(msg));

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 2, 180);
   RARCH_LOG("%s\n", msg);
}


static void set_fullscreen(bool fullscreen)
{
   g_settings.video.fullscreen = fullscreen;
   driver.video_cache_context = 
      g_extern.system.hw_render_callback.cache_context;
   driver.video_cache_context_ack = false;
   rarch_main_command(RARCH_CMD_RESET_CONTEXT);
   driver.video_cache_context = false;

   /* Poll input to avoid possibly stale data to corrupt things. */
   driver.input->poll(driver.input_data);
}

bool rarch_check_fullscreen(bool pressed)
{
   if (pressed)
   {
      /* If we go fullscreen we drop all drivers and 
       * reinitialize to be safe. */
      g_settings.video.fullscreen = !g_settings.video.fullscreen;
      rarch_main_command(RARCH_CMD_REINIT);
   }

   return pressed;
}

static void state_slot(void)
{
   char msg[PATH_MAX];

   if (g_extern.msg_queue)
      msg_queue_clear(g_extern.msg_queue);

   snprintf(msg, sizeof(msg), "State slot: %d",
         g_settings.state_slot);

   if (g_extern.msg_queue)
      msg_queue_push(g_extern.msg_queue, msg, 1, 180);

   RARCH_LOG("%s\n", msg);
}

static void check_stateslots(
      bool pressed_increase, bool pressed_decrease)
{
   /* Save state slots */
   if (pressed_increase)
   {
      g_settings.state_slot++;
      state_slot();
   }

   if (pressed_decrease)
   {
      if (g_settings.state_slot > 0)
         g_settings.state_slot--;
      state_slot();
   }
}

static inline void flush_rewind_audio(void)
{
   /* We just rewound. Flush rewind audio buffer. */
   g_extern.audio_active = rarch_audio_flush(g_extern.audio_data.rewind_buf
         + g_extern.audio_data.rewind_ptr,
         g_extern.audio_data.rewind_size - g_extern.audio_data.rewind_ptr)
      && g_extern.audio_active;

   g_extern.frame_is_reverse = false;
}

static inline void setup_rewind_audio(void)
{
   unsigned i;

   /* Push audio ready to be played. */
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

static void check_rewind(bool pressed)
{
   static bool first = true;

   if (g_extern.frame_is_reverse)
      flush_rewind_audio();

   if (first)
   {
      first = false;
      return;
   }

   if (!g_extern.state_manager)
      return;

   if (pressed)
   {
      const void *buf = NULL;

      msg_queue_clear(g_extern.msg_queue);
      if (state_manager_pop(g_extern.state_manager, &buf))
      {
         g_extern.frame_is_reverse = true;
         setup_rewind_audio();

         msg_queue_push(g_extern.msg_queue, RETRO_MSG_REWINDING, 0,
               g_extern.is_paused ? 1 : 30);
         pretro_unserialize(buf, g_extern.state_size);

         if (g_extern.bsv.movie)
            bsv_movie_frame_rewind(g_extern.bsv.movie);
      }
      else
         msg_queue_push(g_extern.msg_queue,
               RETRO_MSG_REWIND_REACHED_END, 0, 30);
   }
   else
   {
      static unsigned cnt = 0;

      cnt = (cnt + 1) % (g_settings.rewind_granularity ?
            g_settings.rewind_granularity : 1); /* Avoid possible SIGFPE. */

      if ((cnt == 0) || g_extern.bsv.movie)
      {
         void *state = NULL;
         state_manager_push_where(g_extern.state_manager, &state);

         RARCH_PERFORMANCE_INIT(rewind_serialize);
         RARCH_PERFORMANCE_START(rewind_serialize);
         pretro_serialize(state, g_extern.state_size);
         RARCH_PERFORMANCE_STOP(rewind_serialize);

         state_manager_push_do(g_extern.state_manager);
      }
   }

   retro_set_rewind_callbacks();
}

static void check_slowmotion(bool pressed)
{
   g_extern.is_slowmotion = pressed;

   if (!g_extern.is_slowmotion)
      return;

   if (g_settings.video.black_frame_insertion)
      rarch_render_cached_frame();

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, g_extern.frame_is_reverse ?
         "Slow motion rewind." : "Slow motion.", 0, 30);
}

static bool check_movie_init(void)
{
   char path[PATH_MAX], msg[PATH_MAX];
   bool ret = true;
   
   if (g_extern.bsv.movie)
      return false;

   g_settings.rewind_granularity = 1;

   if (g_settings.state_slot > 0)
   {
      snprintf(path, sizeof(path), "%s%d.bsv",
            g_extern.bsv.movie_path, g_settings.state_slot);
   }
   else
   {
      snprintf(path, sizeof(path), "%s.bsv",
            g_extern.bsv.movie_path);
   }

   snprintf(msg, sizeof(msg), "Starting movie record to \"%s\".", path);

   g_extern.bsv.movie = bsv_movie_init(path, RARCH_MOVIE_RECORD);

   if (!g_extern.bsv.movie)
      ret = false;

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, g_extern.bsv.movie ?
         msg : "Failed to start movie record.", 1, 180);

   if (g_extern.bsv.movie)
      RARCH_LOG("Starting movie record to \"%s\".\n", path);
   else
      RARCH_ERR("Failed to start movie record.\n");

   return ret;
}

static bool check_movie_record(void)
{
   if (!g_extern.bsv.movie)
      return false;

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue,
         RETRO_MSG_MOVIE_RECORD_STOPPING, 2, 180);
   RARCH_LOG(RETRO_LOG_MOVIE_RECORD_STOPPING);
   deinit_movie();

   return true;
}

static bool check_movie_playback(void)
{
   if (!g_extern.bsv.movie_end)
      return false;

   msg_queue_push(g_extern.msg_queue,
         RETRO_MSG_MOVIE_PLAYBACK_ENDED, 1, 180);
   RARCH_LOG(RETRO_LOG_MOVIE_PLAYBACK_ENDED);

   deinit_movie();
   g_extern.bsv.movie_end = false;
   g_extern.bsv.movie_playback = false;

   return true;
}

static bool check_movie(void)
{
   if (g_extern.bsv.movie_playback)
      return check_movie_playback();
   if (!g_extern.bsv.movie)
      return check_movie_init();
   return check_movie_record();
}

static void check_pause(bool pressed, bool frameadvance_pressed)
{
   static bool old_focus    = true;
   bool focus               = true;
   bool has_set_audio_stop  = false;
   bool has_set_audio_start = false;

   /* FRAMEADVANCE will set us into pause mode. */
   pressed |= !g_extern.is_paused && frameadvance_pressed;

   if (g_settings.pause_nonactive)
      focus = driver.video->focus(driver.video_data);

   if (focus && pressed)
   {
      g_extern.is_paused = !g_extern.is_paused;

      if (g_extern.is_paused)
      {
         RARCH_LOG("Paused.\n");
         has_set_audio_stop = true;
      }
      else
      {
         RARCH_LOG("Unpaused.\n");
         has_set_audio_start = true;
      }
   }
   else if (focus && !old_focus)
   {
      RARCH_LOG("Unpaused.\n");
      g_extern.is_paused  = false;
      has_set_audio_start = true;
   }
   else if (!focus && old_focus)
   {
      RARCH_LOG("Paused.\n");
      g_extern.is_paused = true;
      has_set_audio_stop = true;
   }

   if (has_set_audio_stop)
      rarch_main_command(RARCH_CMD_AUDIO_STOP);
   if (has_set_audio_start)
      rarch_main_command(RARCH_CMD_AUDIO_START);

   if (g_extern.is_paused && g_settings.video.black_frame_insertion)
      rarch_render_cached_frame();

   old_focus = focus;
}

static void check_oneshot(
      bool oneshot_pressed,
      bool rewind_pressed)
{
   /* Rewind buttons works like FRAMEREWIND when paused.
    * We will one-shot in that case. */
   g_extern.is_oneshot = oneshot_pressed | rewind_pressed;
}

static void check_turbo(void)
{
   unsigned i;
   static const struct retro_keybind *binds[MAX_PLAYERS] = {
      g_settings.input.binds[0],
      g_settings.input.binds[1],
      g_settings.input.binds[2],
      g_settings.input.binds[3],
      g_settings.input.binds[4],
      g_settings.input.binds[5],
      g_settings.input.binds[6],
      g_settings.input.binds[7],
      g_settings.input.binds[8],
      g_settings.input.binds[9],
      g_settings.input.binds[10],
      g_settings.input.binds[11],
      g_settings.input.binds[12],
      g_settings.input.binds[13],
      g_settings.input.binds[14],
      g_settings.input.binds[15],
   };

   g_extern.turbo_count++;


   if (driver.block_libretro_input)
   {
      memset(g_extern.turbo_frame_enable, 0,
            sizeof(g_extern.turbo_frame_enable));
      return;

   }

   for (i = 0; i < MAX_PLAYERS; i++)
      g_extern.turbo_frame_enable[i] =
         driver.input->input_state(driver.input_data, binds, i,
               RETRO_DEVICE_JOYPAD, 0, RARCH_TURBO_ENABLE);
}

static void check_shader_dir(bool pressed_next, bool pressed_prev)
{
   char msg[PATH_MAX];
   const char *shader = NULL, *ext = NULL;
   enum rarch_shader_type type = RARCH_SHADER_NONE;
   bool should_apply = false;

   if (!g_extern.shader_dir.list || !driver.video->set_shader)
      return;

   if (pressed_next)
   {
      should_apply = true;
      g_extern.shader_dir.ptr = (g_extern.shader_dir.ptr + 1) %
         g_extern.shader_dir.list->size;
   }
   else if (pressed_prev)
   {
      should_apply = true;
      if (g_extern.shader_dir.ptr == 0)
         g_extern.shader_dir.ptr = g_extern.shader_dir.list->size - 1;
      else
         g_extern.shader_dir.ptr--;
   }

   if (!should_apply)
      return;

   {
      shader = g_extern.shader_dir.list->elems[g_extern.shader_dir.ptr].data;
      ext    = path_get_extension(shader);

      if (strcmp(ext, "glsl") == 0 || strcmp(ext, "glslp") == 0)
         type = RARCH_SHADER_GLSL;
      else if (strcmp(ext, "cg") == 0 || strcmp(ext, "cgp") == 0)
         type = RARCH_SHADER_CG;

      if (type == RARCH_SHADER_NONE)
         return;

      msg_queue_clear(g_extern.msg_queue);

      snprintf(msg, sizeof(msg), "Shader #%u: \"%s\".",
            (unsigned)g_extern.shader_dir.ptr, shader);
      msg_queue_push(g_extern.msg_queue, msg, 1, 120);
      RARCH_LOG("Applying shader \"%s\".\n", shader);

      if (!driver.video->set_shader(driver.video_data, type, shader))
         RARCH_WARN("Failed to apply shader.\n");
   }
}

void rarch_disk_control_append_image(const char *path)
{
   char msg[PATH_MAX];
   unsigned new_index;
   const struct retro_disk_control_callback *control = 
      (const struct retro_disk_control_callback*)&g_extern.system.disk_control;
   struct retro_game_info info = {0};
   rarch_disk_control_set_eject(true, false);

   control->add_image_index();
   new_index = control->get_num_images();
   if (!new_index)
      return;
   new_index--;

   info.path = path;
   control->replace_image_index(new_index, &info);

   rarch_disk_control_set_index(new_index);

   snprintf(msg, sizeof(msg), "Appended disk: %s", path);
   RARCH_LOG("%s\n", msg);
   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 0, 180);

#if defined(HAVE_THREADS)
   deinit_autosave();
#endif

   /* TODO: Need to figure out what to do with subsystems case. */
   if (!*g_extern.subsystem)
   {
      /* Update paths for our new image.
       * If we actually use append_image, we assume that we
       * started out in a single disk case, and that this way
       * of doing it makes the most sense. */
      set_paths(path);
      fill_pathnames();
   }

#if defined(HAVE_THREADS)
   init_autosave();
#endif

   rarch_disk_control_set_eject(false, false);
}

void rarch_disk_control_set_eject(bool new_state, bool log)
{
   char msg[PATH_MAX];
   const struct retro_disk_control_callback *control = 
      (const struct retro_disk_control_callback*)&g_extern.system.disk_control;
   bool error = false;

   if (!control->get_num_images)
      return;

   *msg = '\0';

   if (control->set_eject_state(new_state))
      snprintf(msg, sizeof(msg), "%s virtual disk tray.",
            new_state ? "Ejected" : "Closed");
   else
   {
      error = true;
      snprintf(msg, sizeof(msg), "Failed to %s virtual disk tray.",
            new_state ? "eject" : "close");
   }

   if (*msg)
   {
      if (error)
         RARCH_ERR("%s\n", msg);
      else
         RARCH_LOG("%s\n", msg);

      /* Only noise in menu. */
      if (log)
      {
         msg_queue_clear(g_extern.msg_queue);
         msg_queue_push(g_extern.msg_queue, msg, 1, 180);
      }
   }
}

void rarch_disk_control_set_index(unsigned next_index)
{
   char msg[PATH_MAX];
   unsigned num_disks;
   const struct retro_disk_control_callback *control = 
      (const struct retro_disk_control_callback*)&g_extern.system.disk_control;
   bool error = false;

   if (!control->get_num_images)
      return;

   *msg = '\0';

   num_disks = control->get_num_images();

   if (control->set_image_index(next_index))
   {
      if (next_index < num_disks)
         snprintf(msg, sizeof(msg), "Setting disk %u of %u in tray.",
               next_index + 1, num_disks);
      else
         strlcpy(msg, "Removed disk from tray.", sizeof(msg));
   }
   else
   {
      if (next_index < num_disks)
         snprintf(msg, sizeof(msg), "Failed to set disk %u of %u.",
               next_index + 1, num_disks);
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

static void check_mute(void)
{
   const char *msg = !g_extern.audio_data.mute ?
      "Audio muted." : "Audio unmuted.";
   g_extern.audio_data.mute = !g_extern.audio_data.mute;

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);

   if (driver.audio_data)
   {
      if (g_extern.audio_data.mute)
         driver.audio->stop(driver.audio_data);
      else if (!driver.audio->start(driver.audio_data))
      {
         RARCH_ERR("Failed to unmute audio.\n");
         g_extern.audio_active = false;
      }
   }

   RARCH_LOG("%s\n", msg);
}

static void check_volume(bool pressed_up, bool pressed_down)
{
   char msg[256];
   float db_change   = 0.0f;

   if (!pressed_up && !pressed_down)
      return;

   if (pressed_up)
      db_change += 0.5f;
   if (pressed_down)
      db_change -= 0.5f;

   g_extern.audio_data.volume_db += db_change;
   g_extern.audio_data.volume_db = max(g_extern.audio_data.volume_db, -80.0f);
   g_extern.audio_data.volume_db = min(g_extern.audio_data.volume_db, 12.0f);

   snprintf(msg, sizeof(msg), "Volume: %.1f dB", g_extern.audio_data.volume_db);
   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);
   RARCH_LOG("%s\n", msg);

   g_extern.audio_data.volume_gain = db_to_gain(g_extern.audio_data.volume_db);
}

#ifdef HAVE_NETPLAY
static void check_netplay_flip(bool pressed, bool fullscreen_toggle_pressed)
{
   if (pressed)
      netplay_flip_players(g_extern.netplay);

   rarch_check_fullscreen(fullscreen_toggle_pressed);
}
#endif

void rarch_check_block_hotkey(bool enable_hotkey)
{
   static const struct retro_keybind *bind = 
      &g_settings.input.binds[0][RARCH_ENABLE_HOTKEY];
   bool use_hotkey_enable;

   /* Don't block the check to RARCH_ENABLE_HOTKEY
    * unless we're really supposed to. */
   driver.block_hotkey = driver.block_input;

   // If we haven't bound anything to this, always allow hotkeys.
   use_hotkey_enable = bind->key != RETROK_UNKNOWN ||
      bind->joykey != NO_BTN ||
      bind->joyaxis != AXIS_NONE;

   driver.block_hotkey = driver.block_input ||
      (use_hotkey_enable && !enable_hotkey);

   /* If we hold ENABLE_HOTKEY button, block all libretro input to allow 
    * hotkeys to be bound to same keys as RetroPad. */
   driver.block_libretro_input = use_hotkey_enable && enable_hotkey;
}

static void check_grab_mouse_toggle(void)
{
   static bool grab_mouse_state  = false;

   grab_mouse_state = !grab_mouse_state;
   RARCH_LOG("Grab mouse state: %s.\n", grab_mouse_state ? "yes" : "no");
   driver.input->grab_mouse(driver.input_data, grab_mouse_state);

   if (driver.video_poke && driver.video_poke->show_mouse)
      driver.video_poke->show_mouse(driver.video_data, !grab_mouse_state);
}

static void check_disk_eject(
      const struct retro_disk_control_callback *control)
{
   bool new_state = !control->get_eject_state();
   rarch_disk_control_set_eject(new_state, true);
}

static void check_disk_next(
      const struct retro_disk_control_callback *control)
{
   unsigned num_disks = control->get_num_images();
   unsigned current   = control->get_image_index();
   if (num_disks && num_disks != UINT_MAX)
   {
      /* Use "no disk" state when index == num_disks. */
      unsigned next_index = current >= num_disks ?
         0 : ((current + 1) % (num_disks + 1));
      rarch_disk_control_set_index(next_index);
   }
   else
      RARCH_ERR("Got invalid disk index from libretro.\n");
}

/* Checks for stuff like fullscreen, save states, etc.
 * Return false when RetroArch is paused. */

static bool do_state_checks(
      retro_input_t input, retro_input_t old_input,
      retro_input_t trigger_input)
{
   if (BIND_PRESSED(trigger_input, RARCH_SCREENSHOT))
      rarch_main_command(RARCH_CMD_TAKE_SCREENSHOT);

   if (g_extern.audio_active)
   {
      if (BIND_PRESSED(trigger_input, RARCH_MUTE))
         check_mute();
   }

   check_volume_func(input, old_input);

   check_turbo();

   if (driver.input->grab_mouse)
   {
      if (BIND_PRESSED(trigger_input, RARCH_GRAB_MOUSE_TOGGLE))
         check_grab_mouse_toggle();
   }

#ifdef HAVE_OVERLAY
   if (driver.overlay)
   {
      if (BIND_PRESSED(trigger_input, RARCH_OVERLAY_NEXT))
         input_overlay_next(driver.overlay);
   }
#endif

#ifdef HAVE_NETPLAY
   if (g_extern.netplay)
   {
      check_netplay_flip_func(trigger_input);
      return true;
   }
#endif
   check_pause_func(trigger_input);

   check_oneshot_func(trigger_input);

   if (check_fullscreen_func(trigger_input) && g_extern.is_paused)
      rarch_render_cached_frame();

   if (g_extern.is_paused && !g_extern.is_oneshot)
      return false;

   check_fast_forward_button_func(input, old_input, trigger_input);

   check_stateslots_func(trigger_input);

   if (BIND_PRESSED(trigger_input, RARCH_SAVE_STATE_KEY))
      rarch_main_command(RARCH_CMD_SAVE_STATE);
   else if (!g_extern.bsv.movie) /* Immutable */
   {
      if (BIND_PRESSED(trigger_input, RARCH_LOAD_STATE_KEY))
         rarch_main_command(RARCH_CMD_LOAD_STATE);
   }

   check_rewind_func(input);

   check_slowmotion_func(input);

   if (BIND_PRESSED(trigger_input, RARCH_MOVIE_RECORD_TOGGLE))
      check_movie();

   check_shader_dir_func(trigger_input);

   if (g_extern.cheat)
   {
      if (BIND_PRESSED(trigger_input, RARCH_CHEAT_INDEX_PLUS))
         cheat_manager_index_next(g_extern.cheat);
      else if (BIND_PRESSED(trigger_input, RARCH_CHEAT_INDEX_MINUS))
         cheat_manager_index_prev(g_extern.cheat);
      else if (BIND_PRESSED(trigger_input, RARCH_CHEAT_TOGGLE))
         cheat_manager_toggle(g_extern.cheat);
   }

   if (g_extern.system.disk_control.get_num_images)
   {
      const struct retro_disk_control_callback *control = 
         (const struct retro_disk_control_callback*)
         &g_extern.system.disk_control;

      if (BIND_PRESSED(trigger_input, RARCH_DISK_EJECT_TOGGLE))
         check_disk_eject(control);
      else if (BIND_PRESSED(trigger_input, RARCH_DISK_NEXT))
         check_disk_next(control);
   }

   if (BIND_PRESSED(trigger_input, RARCH_RESET))
      rarch_main_command(RARCH_CMD_RESET);

   return true;
}

static void init_state(void)
{
   g_extern.video_active = true;
   g_extern.audio_active = true;
}

static void deinit_log_file(void)
{
   if (g_extern.log_file)
      fclose(g_extern.log_file);
   g_extern.log_file = NULL;
}

void rarch_main_clear_state(void)
{
   unsigned i;

   memset(&g_settings, 0, sizeof(g_settings));

   deinit_log_file();

   memset(&g_extern, 0, sizeof(g_extern));

   init_state();

   for (i = 0; i < MAX_PLAYERS; i++)
      g_settings.input.libretro_device[i] = RETRO_DEVICE_JOYPAD;

   init_msg_queue();
}

#ifdef HAVE_ZLIB
#define DEFAULT_EXT "zip"
#else
#define DEFAULT_EXT ""
#endif

static void init_system_info(void)
{
   struct retro_system_info *info = (struct retro_system_info*)
      &g_extern.system.info;
   pretro_get_system_info(info);

   if (!info->library_name)
      info->library_name = "Unknown";
   if (!info->library_version)
      info->library_version = "v0";

#ifdef RARCH_CONSOLE
   snprintf(g_extern.title_buf, sizeof(g_extern.title_buf), "%s %s",
         info->library_name, info->library_version);
#else
   snprintf(g_extern.title_buf, sizeof(g_extern.title_buf),
         RETRO_FRONTEND " : %s %s",
         info->library_name, info->library_version);
#endif
   strlcpy(g_extern.system.valid_extensions, info->valid_extensions ?
         info->valid_extensions : DEFAULT_EXT,
         sizeof(g_extern.system.valid_extensions));
   g_extern.system.block_extract = info->block_extract;
}

static void init_system_av_info(void)
{
   pretro_get_system_av_info(&g_extern.system.av_info);
   g_extern.frame_limit.last_frame_time = rarch_get_time_usec();
}

static void verify_api_version(void)
{
   /* TODO - when libretro v2 gets added, allow for switching
    * between libretro version backend dynamically. */
   RARCH_LOG("Version of libretro API: %u\n", pretro_api_version());
   RARCH_LOG("Compiled against API: %u\n", RETRO_API_VERSION);
   if (pretro_api_version() != RETRO_API_VERSION)
      RARCH_WARN(RETRO_LOG_LIBRETRO_ABI_BREAK);
}

/* Make sure we haven't compiled for something we cannot run.
 * Ideally, code would get swapped out depending on CPU support, 
 * but this will do for now.
 */
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

static void init_sram(void)
{
   g_extern.use_sram = g_extern.use_sram && !g_extern.sram_save_disable
#ifdef HAVE_NETPLAY
   && (!g_extern.netplay || !g_extern.netplay_is_client)
#endif
   ;

   if (g_extern.use_sram)
   {
#if defined(HAVE_THREADS)
      init_autosave();
#endif
   }
   else
      RARCH_LOG("SRAM will not be saved.\n");
}

static void deinit_core(void)
{
   pretro_unload_game();
   pretro_deinit();
   uninit_drivers();
   uninit_libretro_sym();
}

int rarch_main_init(int argc, char *argv[])
{
   int sjlj_ret;

   init_state();

   if ((sjlj_ret = setjmp(g_extern.error_sjlj_context)) > 0)
   {
      RARCH_ERR("Fatal error received in: \"%s\"\n", g_extern.error_string);
      return sjlj_ret;
   }
   g_extern.error_in_init = true;
   parse_input(argc, argv);

   if (g_extern.verbosity)
   {
      RARCH_LOG_OUTPUT("=== Build =======================================");
      print_compiler(stderr);
      RARCH_LOG_OUTPUT("Version: %s\n", PACKAGE_VERSION);
#ifdef HAVE_GIT_VERSION
      RARCH_LOG_OUTPUT("Git: %s\n", rarch_git_version);
#endif
      RARCH_LOG_OUTPUT("=================================================\n");
   }

   validate_cpu_features();
   config_load();

   init_libretro_sym(g_extern.libretro_dummy);
   init_system_info();

   init_drivers_pre();

   verify_api_version();
   pretro_init();

   g_extern.use_sram = !g_extern.libretro_dummy &&
      !g_extern.libretro_no_content;

   if (g_extern.libretro_no_content && !g_extern.libretro_dummy)
   {
      if (!init_content_file())
         goto error;
   }
   else if (!g_extern.libretro_dummy)
   {
      fill_pathnames();

      if (!init_content_file())
         goto error;

      set_savestate_auto_index();

      if (load_save_files())
         RARCH_LOG("Skipping SRAM load.\n");

      load_auto_state();

      init_movie();

#ifdef HAVE_NETPLAY
      init_netplay();
#endif
   }

   retro_init_libretro_cbs(&driver.retro_ctx);
   init_system_av_info();
   init_drivers();

#ifdef HAVE_COMMAND
   init_command();
#endif

   init_rewind();
   init_controllers();

   init_recording();

   init_sram();

   init_cheats();

   g_extern.error_in_init = false;
   g_extern.main_is_init  = true;
   return 0;

error:
   deinit_core();

   g_extern.main_is_init = false;
   return 1;
}

static inline void update_frame_time(void)
{
   retro_time_t delta = 0;
   retro_time_t time = rarch_get_time_usec();
   bool is_locked_fps = g_extern.is_paused || driver.nonblock_state;

   is_locked_fps |= !!g_extern.rec;

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
   retro_time_t current = rarch_get_time_usec();
   retro_time_t target  = 0, to_sleep_ms = 0;

   g_extern.frame_limit.minimum_frame_time = (retro_time_t)
      roundf(1000000.0f / (g_extern.system.av_info.timing.fps *
               g_settings.fastforward_ratio));

   target = g_extern.frame_limit.last_frame_time + 
      g_extern.frame_limit.minimum_frame_time;
   to_sleep_ms = (target - current) / 1000;

   if (to_sleep_ms > 0)
   {
      rarch_sleep((unsigned int)to_sleep_ms);

      /* Combat jitter a bit. */
      g_extern.frame_limit.last_frame_time += 
         g_extern.frame_limit.minimum_frame_time;
   }
   else
      g_extern.frame_limit.last_frame_time = rarch_get_time_usec();
}

/* TODO - can we refactor command.c to do this? Should be local and not
 * stdin or network-based */

void rarch_main_set_state(unsigned cmd)
{
   switch (cmd)
   {
      case RARCH_ACTION_STATE_MENU_PREINIT:
         {
            int i;

            /* Menu should always run with vsync on. */
            rarch_main_command(RARCH_CMD_VIDEO_SET_BLOCKING_STATE);

            /* Stop all rumbling before entering the menu. */
            for (i = 0; i < MAX_PLAYERS; i++)
            {
               driver_set_rumble_state(i, RETRO_RUMBLE_STRONG, 0);
               driver_set_rumble_state(i, RETRO_RUMBLE_WEAK, 0);
            }

            rarch_main_command(RARCH_CMD_AUDIO_STOP);

#ifdef HAVE_MENU
            if (driver.menu)
            {
               /* Override keyboard callback to redirect to menu instead.
                * We'll use this later for something ...
                * FIXME: This should probably be moved to menu_common somehow. */
               g_extern.frontend_key_event = g_extern.system.key_event;
               g_extern.system.key_event = menu_key_event;

               driver.menu->need_refresh = true;
               rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING);
            }
#endif
            g_extern.system.frame_time_last = 0;
         }
         break;
      case RARCH_ACTION_STATE_LOAD_CONTENT:
#ifdef HAVE_MENU
         if (!load_menu_content())
         {
            /* If content loading fails, we go back to menu. */
            if (driver.menu)
               rarch_main_set_state(RARCH_ACTION_STATE_MENU_PREINIT);
         }
#endif
         break;
      case RARCH_ACTION_STATE_MENU_RUNNING:
         g_extern.is_menu = true;
         break;
      case RARCH_ACTION_STATE_MENU_RUNNING_FINISHED:
         g_extern.is_menu = false;
         break;
      case RARCH_ACTION_STATE_EXITSPAWN:
         g_extern.lifecycle_state |= (1ULL << MODE_EXITSPAWN);
         break;
      case RARCH_ACTION_STATE_QUIT:
         g_extern.system.shutdown = true;
         rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);
         break;
      case RARCH_ACTION_STATE_FORCE_QUIT:
         g_extern.lifecycle_state = 0;
         rarch_main_set_state(RARCH_ACTION_STATE_QUIT);
         break;
      case RARCH_ACTION_STATE_NONE:
      default:
         break;
   }
}

/* Save a new config to a file. Filename is based
 * on heuristics to avoid typing. */

static void save_core_config(void)
{
   char config_dir[PATH_MAX], config_name[PATH_MAX],
        config_path[PATH_MAX], msg[PATH_MAX];
   bool found_path = false;

   *config_dir = '\0';

   if (*g_settings.menu_config_directory)
      strlcpy(config_dir, g_settings.menu_config_directory,
            sizeof(config_dir));
   else if (*g_extern.config_path) /* Fallback */
      fill_pathname_basedir(config_dir, g_extern.config_path,
            sizeof(config_dir));
   else
   {
      const char *msg = "Config directory not set. Cannot save new config.";
      msg_queue_clear(g_extern.msg_queue);
      msg_queue_push(g_extern.msg_queue, msg, 1, 180);
      RARCH_ERR("%s\n", msg);
      return;
   }

   /* Infer file name based on libretro core. */
   if (*g_settings.libretro && path_file_exists(g_settings.libretro))
   {
      unsigned i;

      /* In case of collision, find an alternative name. */
      for (i = 0; i < 16; i++)
      {
         char tmp[64];
         fill_pathname_base(config_name, g_settings.libretro,
               sizeof(config_name));
         path_remove_extension(config_name);
         fill_pathname_join(config_path, config_dir, config_name,
               sizeof(config_path));

         *tmp = '\0';

         if (i)
            snprintf(tmp, sizeof(tmp), "-%u.cfg", i);
         else
            strlcpy(tmp, ".cfg", sizeof(tmp));

         strlcat(config_path, tmp, sizeof(config_path));

         if (!path_file_exists(config_path))
         {
            found_path = true;
            break;
         }
      }
   }

   /* Fallback to system time... */
   if (!found_path)
   {
      RARCH_WARN("Cannot infer new config path. Use current time.\n");
      fill_dated_filename(config_name, "cfg", sizeof(config_name));
      fill_pathname_join(config_path, config_dir, config_name,
            sizeof(config_path));
   }

   if (config_save_file(config_path))
   {
      strlcpy(g_extern.config_path, config_path,
            sizeof(g_extern.config_path));
      snprintf(msg, sizeof(msg), "Saved new config to \"%s\".",
            config_path);
      RARCH_LOG("%s\n", msg);
   }
   else
   {
      snprintf(msg, sizeof(msg), "Failed saving config to \"%s\".",
            config_path);
      RARCH_ERR("%s\n", msg);
   }

   msg_queue_clear(g_extern.msg_queue);
   msg_queue_push(g_extern.msg_queue, msg, 1, 180);
}

static void history_playlist_new(void)
{
   bool init_history = true;

   if (g_extern.history)
      return;

   if (!path_file_exists(g_settings.content_history_path))
      init_history = write_empty_file(
            g_settings.content_history_path);

   if (init_history)
      g_extern.history = content_playlist_init(
            g_settings.content_history_path,
            g_settings.content_history_size);
}

static void history_playlist_free(void)
{
   if (g_extern.history)
      content_playlist_free(g_extern.history);
   g_extern.history = NULL;
}

void rarch_main_command(unsigned cmd)
{
   bool boolean = false;

   switch (cmd)
   {
      case RARCH_CMD_LOAD_CONTENT_PERSIST:
         rarch_main_command(RARCH_CMD_LOAD_CORE);
         rarch_main_set_state(RARCH_ACTION_STATE_LOAD_CONTENT);
         break;
      case RARCH_CMD_LOAD_CONTENT:
#ifdef HAVE_DYNAMIC
         rarch_main_command(RARCH_CMD_LOAD_CONTENT_PERSIST);
#else
         rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH,
               (void*)g_settings.libretro);
         rarch_environment_cb(RETRO_ENVIRONMENT_EXEC,
               (void*)g_extern.fullpath);
#endif
         break;
      case RARCH_CMD_LOAD_CORE:
#ifdef HAVE_MENU
         if (driver.menu)
            rarch_update_system_info(&g_extern.menu.info,
                  &driver.menu->load_no_content);
#endif
         break;
      case RARCH_CMD_LOAD_STATE:
         /* Disallow savestate load when we absolutely 
          * cannot change game state. */
         if (g_extern.bsv.movie)
            return;

#ifdef HAVE_NETPLAY
         if (g_extern.netplay)
            return;
#endif
         main_state(cmd);
         break;
      case RARCH_CMD_RESET:
         RARCH_LOG(RETRO_LOG_RESETTING_CONTENT);
         msg_queue_clear(g_extern.msg_queue);
         msg_queue_push(g_extern.msg_queue, "Reset.", 1, 120);
         pretro_reset();
         /* bSNES since v073r01 resets controllers to JOYPAD
          * after a reset, so just enforce it here. */
         init_controllers();
         break;
      case RARCH_CMD_SAVE_STATE:
         if (g_settings.savestate_auto_index)
            g_settings.state_slot++;

         main_state(cmd);
         break;
      case RARCH_CMD_TAKE_SCREENSHOT:
         take_screenshot();
         break;
      case RARCH_CMD_PREPARE_DUMMY:
         *g_extern.fullpath = '\0';

#ifdef HAVE_MENU
         if (driver.menu)
            driver.menu->load_no_content = false;
#endif

         rarch_main_set_state(RARCH_ACTION_STATE_LOAD_CONTENT);
         g_extern.system.shutdown = false;
         break;
      case RARCH_CMD_QUIT:
         g_extern.system.shutdown = true;
         break;
      case RARCH_CMD_REINIT:
         set_fullscreen(g_settings.video.fullscreen);
         break;
      case RARCH_CMD_REWIND:
         if (g_settings.rewind_enable)
            init_rewind();
         else
            deinit_rewind();
         break;
      case RARCH_CMD_AUTOSAVE:
#ifdef HAVE_THREADS
         deinit_autosave();
         init_autosave();
#endif
         break;
      case RARCH_CMD_AUDIO_STOP:
         if (driver.audio_data)
            driver.audio->stop(driver.audio_data);
         break;
      case RARCH_CMD_AUDIO_START:
         if (driver.audio_data && !g_extern.audio_data.mute
               && !driver.audio->start(driver.audio_data))
         {
            RARCH_ERR("Failed to start audio driver. Will continue without audio.\n");
            g_extern.audio_active = false;
         }
         break;
      case RARCH_CMD_OVERLAY_INIT:
#ifdef HAVE_OVERLAY
         if (!*g_settings.input.overlay)
            break;

         driver.overlay = input_overlay_new(g_settings.input.overlay);
         if (!driver.overlay)
            RARCH_ERR("Failed to load overlay.\n");
#endif
         break;
      case RARCH_CMD_OVERLAY_DEINIT:
#ifdef HAVE_OVERLAY
         input_overlay_free(driver.overlay);
         driver.overlay = NULL;
         memset(&driver.overlay_state, 0, sizeof(driver.overlay_state));
#endif
         break;
      case RARCH_CMD_OVERLAY_REINIT:
         rarch_main_command(RARCH_CMD_OVERLAY_DEINIT);
         rarch_main_command(RARCH_CMD_OVERLAY_INIT);
         break;
      case RARCH_CMD_DSP_FILTER_INIT:
         rarch_main_command(RARCH_CMD_DSP_FILTER_DEINIT);
         if (!*g_settings.audio.dsp_plugin)
            break;

         g_extern.audio_data.dsp = rarch_dsp_filter_new(
               g_settings.audio.dsp_plugin, g_extern.audio_data.in_rate);
         if (!g_extern.audio_data.dsp)
            RARCH_ERR("[DSP]: Failed to initialize DSP filter \"%s\".\n",
                  g_settings.audio.dsp_plugin);
         break;
      case RARCH_CMD_DSP_FILTER_DEINIT:
         if (g_extern.audio_data.dsp)
            rarch_dsp_filter_free(g_extern.audio_data.dsp);
         g_extern.audio_data.dsp = NULL;
         break;
      case RARCH_CMD_RECORD_INIT:
         init_recording();
         break;
      case RARCH_CMD_RECORD_DEINIT:
         rarch_deinit_recording();
         break;
      case RARCH_CMD_HISTORY_INIT:
         history_playlist_new();
         break;
      case RARCH_CMD_HISTORY_DEINIT:
         history_playlist_free();
         break;
      case RARCH_CMD_CORE_INFO_INIT:
         core_info_list_free(g_extern.core_info);
         g_extern.core_info = NULL;
         if (*g_settings.libretro_directory)
            g_extern.core_info = core_info_list_new(g_settings.libretro_directory);
#ifdef HAVE_MENU
         if (driver.menu_ctx && driver.menu_ctx->init_core_info)
            driver.menu_ctx->init_core_info(driver.menu);
#endif
         break;
      case RARCH_CMD_VIDEO_APPLY_STATE_CHANGES:
         if (driver.video_data && driver.video_poke
               && driver.video_poke->apply_state_changes)
            driver.video_poke->apply_state_changes(driver.video_data);
         break;
      case RARCH_CMD_VIDEO_SET_NONBLOCKING_STATE:
         boolean = true; /* fall-through */
      case RARCH_CMD_VIDEO_SET_BLOCKING_STATE:
         if (driver.video && driver.video->set_nonblock_state)
            driver.video->set_nonblock_state(driver.video_data, boolean);
         break;
      case RARCH_CMD_VIDEO_SET_ASPECT_RATIO:
         if (driver.video_data && driver.video_poke
               && driver.video_poke->set_aspect_ratio)
            driver.video_poke->set_aspect_ratio(driver.video_data,
                  g_settings.video.aspect_ratio_idx);
         break;
      case RARCH_CMD_AUDIO_SET_NONBLOCKING_STATE:
         boolean = true; /* fall-through */
      case RARCH_CMD_AUDIO_SET_BLOCKING_STATE:
         if (driver.audio && driver.audio->set_nonblock_state)
            driver.audio->set_nonblock_state(driver.audio_data, boolean);
         break;
      case RARCH_CMD_OVERLAY_SET_SCALE_FACTOR:
#ifdef HAVE_OVERLAY
         input_overlay_set_scale_factor(driver.overlay,
               g_settings.input.overlay_scale);
#endif
         break;
      case RARCH_CMD_OVERLAY_SET_ALPHA_MOD:
#ifdef HAVE_OVERLAY
         input_overlay_set_alpha_mod(driver.overlay,
               g_settings.input.overlay_opacity);
#endif
         break;
      case RARCH_CMD_RESET_CONTEXT:
         uninit_drivers();
         init_drivers();
         break;
      case RARCH_CMD_QUIT_RETROARCH:
         rarch_main_set_state(RARCH_ACTION_STATE_FORCE_QUIT);
         break;
      case RARCH_CMD_RESUME:
         rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);
         break;
      case RARCH_CMD_RESTART_RETROARCH:
#if defined(GEKKO) && defined(HW_RVL)
         fill_pathname_join(g_extern.fullpath, g_defaults.core_dir,
               SALAMANDER_FILE,
               sizeof(g_extern.fullpath));
#endif
         rarch_main_set_state(RARCH_ACTION_STATE_EXITSPAWN);
         break;
      case RARCH_CMD_MENU_SAVE_CONFIG:
         save_core_config();
         break;
      case RARCH_CMD_SHADERS_APPLY_CHANGES:
         menu_shader_manager_apply_changes();
         break;
   }
}

bool rarch_main_iterate(void)
{
   unsigned i;
   retro_input_t old_input, trigger_input;
   retro_input_t input = meta_input_keys_pressed(RARCH_FIRST_META_KEY,
         RARCH_BIND_LIST_END, &old_input);

   trigger_input = input & ~old_input;

   /* Time to drop? */
   if (
         g_extern.system.shutdown ||
         check_quit_key_func(input) ||
         !driver.video->alive(driver.video_data))
      return false;

   if (g_extern.is_menu)
   {
      if (
            !menu_iterate(input, old_input, trigger_input) ||
            (check_enter_menu_func(trigger_input) &&
             g_extern.main_is_init && !g_extern.libretro_dummy)
         )
      {
         rarch_main_set_state(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED);
         driver_set_nonblock_state(driver.nonblock_state);

         rarch_main_command(RARCH_CMD_AUDIO_START);

         driver.block_libretro_input_until = g_extern.frame_count + (5);
         /* Restore libretro keyboard callback. */
         g_extern.system.key_event = g_extern.frontend_key_event;
      }
      return true;
   }

   if (check_enter_menu_func(trigger_input) || (g_extern.libretro_dummy))
   {
      /* Always go into menu if dummy core is loaded. */
      rarch_main_set_state(RARCH_ACTION_STATE_MENU_PREINIT);
      return true; /* Enter menu on next run. */
   }

   if (g_extern.exec)
   {
      g_extern.exec = false;
      return false;
   }

   if (!do_state_checks(input, old_input, trigger_input))
   {
      driver.retro_ctx.poll_cb();
      rarch_sleep(10);
      return true;
   }

#if defined(HAVE_THREADS)
   lock_autosave();
#endif

#ifdef HAVE_NETPLAY
   if (g_extern.netplay)
      netplay_pre_frame(g_extern.netplay);
#endif

   if (g_extern.bsv.movie)
      bsv_movie_set_frame_start(g_extern.bsv.movie);

   if (g_extern.system.camera_callback.caps)
      driver_camera_poll();

   /* Update binds for analog dpad modes. */
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      if (!g_settings.input.analog_dpad_mode[i])
         continue;

      input_push_analog_dpad(g_settings.input.binds[i],
            g_settings.input.analog_dpad_mode[i]);
      input_push_analog_dpad(g_settings.input.autoconf_binds[i],
            g_settings.input.analog_dpad_mode[i]);
   }

   if ((g_settings.video.frame_delay > 0) && !driver.nonblock_state)
      rarch_sleep(g_settings.video.frame_delay);

   if (g_extern.system.frame_time.callback)
      update_frame_time();

   /* Run libretro for one frame. */
   pretro_run();

   if (g_settings.fastforward_ratio >= 0.0f)
      limit_frame_time();

   for (i = 0; i < MAX_PLAYERS; i++)
   {
      if (!g_settings.input.analog_dpad_mode[i])
         continue;

      input_pop_analog_dpad(g_settings.input.binds[i]);
      input_pop_analog_dpad(g_settings.input.autoconf_binds[i]);
   }

   if (g_extern.bsv.movie)
      bsv_movie_set_frame_end(g_extern.bsv.movie);

#ifdef HAVE_NETPLAY
   if (g_extern.netplay)
      netplay_post_frame(g_extern.netplay);
#endif

#if defined(HAVE_THREADS)
   unlock_autosave();
#endif

   return true;
}

static void free_temporary_content(void)
{
   unsigned i;
   for (i = 0; i < g_extern.temporary_content->size; i++)
   {
      const char *path = g_extern.temporary_content->elems[i].data;

      RARCH_LOG("Removing temporary content file: %s.\n", path);
      if (remove(path) < 0)
         RARCH_ERR("Failed to remove temporary file: %s.\n", path);
   }
   string_list_free(g_extern.temporary_content);
}

static void deinit_temporary_content(void)
{
   if (g_extern.temporary_content)
      free_temporary_content();
   g_extern.temporary_content = NULL;
}

static void deinit_subsystem_fullpaths(void)
{
   if (g_extern.subsystem_fullpaths)
      string_list_free(g_extern.subsystem_fullpaths);
   g_extern.subsystem_fullpaths = NULL;
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
      deinit_autosave();
#endif

   rarch_deinit_recording();

   save_files();

   deinit_rewind();
   deinit_cheats();

   deinit_movie();

   save_auto_state();

   deinit_core();

   deinit_temporary_content();
   deinit_subsystem_fullpaths();
   deinit_savefiles();

   g_extern.main_is_init = false;
}
