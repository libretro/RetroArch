/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */


#include "driver.h"
#include "general.h"
#include "file.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

static const audio_driver_t *audio_drivers[] = {
#ifdef HAVE_ALSA
   &audio_alsa,
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
#ifdef HAVE_DYLIB
   &audio_ext,
#endif
};

static const video_driver_t *video_drivers[] = {
#ifdef HAVE_SDL
   &video_gl,
   &video_sdl,
#endif
#ifdef HAVE_XVIDEO
   &video_xvideo,
#endif
#ifdef HAVE_DYLIB
   &video_ext,
#endif
};

static const input_driver_t *input_drivers[] = {
#ifdef HAVE_SDL
   &input_sdl,
#endif
#ifdef HAVE_XVIDEO
   &input_x,
#endif
};

static void find_audio_driver(void)
{
   for (int i = 0; i < sizeof(audio_drivers) / sizeof(audio_driver_t*); i++)
   {
      if (strcasecmp(g_settings.audio.driver, audio_drivers[i]->ident) == 0)
      {
         driver.audio = audio_drivers[i];
         return;
      }
   }
   SSNES_ERR("Couldn't find any audio driver named \"%s\"\n", g_settings.audio.driver);
   fprintf(stderr, "Available audio drivers are:\n");
   for (int i = 0; i < sizeof(audio_drivers) / sizeof(audio_driver_t*); i++)
      fprintf(stderr, "\t%s\n", audio_drivers[i]->ident);

   exit(1);
}

static void find_video_driver(void)
{
   for (int i = 0; i < sizeof(video_drivers) / sizeof(video_driver_t*); i++)
   {
      if (strcasecmp(g_settings.video.driver, video_drivers[i]->ident) == 0)
      {
         driver.video = video_drivers[i];
         return;
      }
   }
   SSNES_ERR("Couldn't find any video driver named \"%s\"\n", g_settings.video.driver);
   fprintf(stderr, "Available video drivers are:\n");
   for (int i = 0; i < sizeof(video_drivers) / sizeof(video_driver_t*); i++)
      fprintf(stderr, "\t%s\n", video_drivers[i]->ident);

   exit(1);
}

static void find_input_driver(void)
{
   for (int i = 0; i < sizeof(input_drivers) / sizeof(input_driver_t*); i++)
   {
      if (strcasecmp(g_settings.input.driver, input_drivers[i]->ident) == 0)
      {
         driver.input = input_drivers[i];
         return;
      }
   }
   SSNES_ERR("Couldn't find any input driver named \"%s\"\n", g_settings.input.driver);
   fprintf(stderr, "Available input drivers are:\n");
   for (int i = 0; i < sizeof(input_drivers) / sizeof(input_driver_t*); i++)
      fprintf(stderr, "\t%s\n", input_drivers[i]->ident);

   exit(1);
}

void init_drivers(void)
{
   init_video_input();
   init_audio();
}

void uninit_drivers(void)
{
   uninit_video_input();
   uninit_audio();
}

static void init_dsp_plugin(void)
{
   if (!(*g_settings.audio.dsp_plugin))
      return;

   g_extern.audio_data.dsp_lib = dylib_load(g_settings.audio.dsp_plugin);
   if (!g_extern.audio_data.dsp_lib)
   {
      SSNES_ERR("Failed to open DSP plugin: \"%s\" ...\n", g_settings.audio.dsp_plugin);
      return;
   }

   const ssnes_dsp_plugin_t* (*SSNES_API_CALLTYPE plugin_init)(void) = 
      (const ssnes_dsp_plugin_t *(SSNES_API_CALLTYPE*)(void))dylib_proc(g_extern.audio_data.dsp_lib, "ssnes_dsp_plugin_init");
   if (!plugin_init)
   {
      SSNES_ERR("Failed to find symbol \"ssnes_dsp_plugin_init\" in DSP plugin.\n");
      goto error;
   }

   g_extern.audio_data.dsp_plugin = plugin_init();
   if (!g_extern.audio_data.dsp_plugin)
   {
      SSNES_ERR("Failed to get a valid DSP plugin.\n");
      goto error;
   }

   if (g_extern.audio_data.dsp_plugin->api_version != SSNES_DSP_API_VERSION)
   {
      SSNES_ERR("DSP plugin API mismatch! SSNES: %d, Plugin: %d\n", SSNES_DSP_API_VERSION, g_extern.audio_data.dsp_plugin->api_version);
      goto error;
   }

   SSNES_LOG("Loaded DSP plugin: \"%s\"\n", g_extern.audio_data.dsp_plugin->ident ? g_extern.audio_data.dsp_plugin->ident : "Unknown");

   const ssnes_dsp_info_t info = {
      .input_rate = g_settings.audio.in_rate,
      .output_rate = g_settings.audio.out_rate
   };

   g_extern.audio_data.dsp_handle = g_extern.audio_data.dsp_plugin->init(&info);
   if (!g_extern.audio_data.dsp_handle)
   {
      SSNES_ERR("Failed to init DSP plugin.\n");
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

#define AUDIO_CHUNK_SIZE_BLOCKING 64
#define AUDIO_CHUNK_SIZE_NONBLOCKING 2048 // So we don't get complete line-noise when fast-forwarding audio.
#define AUDIO_MAX_RATIO 16
void init_audio(void)
{
   if (!g_settings.audio.enable)
   {
      g_extern.audio_active = false;
      return;
   }

   find_audio_driver();

   g_extern.audio_data.block_chunk_size = AUDIO_CHUNK_SIZE_BLOCKING;
   g_extern.audio_data.nonblock_chunk_size = AUDIO_CHUNK_SIZE_NONBLOCKING;

   driver.audio_data = driver.audio->init(strlen(g_settings.audio.device) ? g_settings.audio.device : NULL, g_settings.audio.out_rate, g_settings.audio.latency);
   if (!driver.audio_data)
      g_extern.audio_active = false;

   if (g_extern.audio_active && driver.audio->use_float && driver.audio->use_float(driver.audio_data))
      g_extern.audio_data.use_float = true;

   if (!g_settings.audio.sync && g_extern.audio_active)
   {
      driver.audio->set_nonblock_state(driver.audio_data, true);
      g_extern.audio_data.chunk_size = g_extern.audio_data.nonblock_chunk_size;
   }
   else
      g_extern.audio_data.chunk_size = g_extern.audio_data.block_chunk_size;

   g_extern.audio_data.source = hermite_new(2);
   if (!g_extern.audio_data.source)
      g_extern.audio_active = false;

   size_t max_bufsamples = g_extern.audio_data.block_chunk_size > g_extern.audio_data.nonblock_chunk_size ?
      g_extern.audio_data.block_chunk_size : g_extern.audio_data.nonblock_chunk_size;

   max_bufsamples *= 2; // Accomodate rewind since at some point we might have two full buffers.

   assert((g_extern.audio_data.data = malloc(max_bufsamples * sizeof(float))));
   g_extern.audio_data.data_ptr = 0;
   assert(g_settings.audio.out_rate < g_settings.audio.in_rate * AUDIO_MAX_RATIO);
   assert((g_extern.audio_data.outsamples = malloc(max_bufsamples * sizeof(float) * AUDIO_MAX_RATIO)));
   assert((g_extern.audio_data.conv_outsamples = malloc(max_bufsamples * sizeof(int16_t) * AUDIO_MAX_RATIO)));

   // Needs to be able to hold full content of a full max_bufsamples in addition to its own.
   assert((g_extern.audio_data.rewind_buf = malloc(max_bufsamples * sizeof(int16_t))));
   g_extern.audio_data.rewind_size = max_bufsamples;

   g_extern.audio_data.src_ratio =
      (double)g_settings.audio.out_rate / g_settings.audio.in_rate;

   init_dsp_plugin();
}

void uninit_audio(void)
{
   if (!g_settings.audio.enable)
   {
      g_extern.audio_active = false;
      return;
   }

   if (driver.audio_data && driver.audio)
      driver.audio->free(driver.audio_data);

   if (g_extern.audio_data.source)
      hermite_free(g_extern.audio_data.source);

   free(g_extern.audio_data.data); g_extern.audio_data.data = NULL;
   free(g_extern.audio_data.outsamples); g_extern.audio_data.outsamples = NULL;
   free(g_extern.audio_data.conv_outsamples); g_extern.audio_data.conv_outsamples = NULL;
   free(g_extern.audio_data.rewind_buf); g_extern.audio_data.rewind_buf = NULL;

   deinit_dsp_plugin();
}

#ifdef HAVE_DYLIB
static void init_filter(void)
{
   if (g_extern.filter.active)
      return;
   if (*g_settings.video.filter_path == '\0')
      return;

   SSNES_LOG("Loading bSNES filter from \"%s\"\n", g_settings.video.filter_path);
   g_extern.filter.lib = dylib_load(g_settings.video.filter_path);
   if (!g_extern.filter.lib)
   {
      SSNES_ERR("Failed to load filter \"%s\"\n", g_settings.video.filter_path);
      return;
   }

   g_extern.filter.psize = 
      (void (*)(unsigned*, unsigned*))dylib_proc(g_extern.filter.lib, "filter_size");
   g_extern.filter.prender = 
      (void (*)(uint32_t*, uint32_t*, 
                unsigned, const uint16_t*, 
                unsigned, unsigned, unsigned))dylib_proc(g_extern.filter.lib, "filter_render");
   if (!g_extern.filter.psize || !g_extern.filter.prender)
   {
      SSNES_ERR("Failed to find functions in filter...\n");
      dylib_close(g_extern.filter.lib);
      g_extern.filter.lib = NULL;
      return;
   }

   g_extern.filter.active = true;

   unsigned width = 512;
   unsigned height = 512;
   g_extern.filter.psize(&width, &height);

   unsigned pow2_x = next_pow2(ceil(width));
   unsigned pow2_y = next_pow2(ceil(height));
   unsigned maxsize = pow2_x > pow2_y ? pow2_x : pow2_y; 
   g_extern.filter.scale = maxsize / 256;

   g_extern.filter.buffer = malloc(256 * 256 * g_extern.filter.scale * g_extern.filter.scale * sizeof(uint32_t));
   g_extern.filter.pitch = 256 * g_extern.filter.scale * sizeof(uint32_t);
   assert(g_extern.filter.buffer);

   g_extern.filter.colormap = malloc(32768 * sizeof(uint32_t));
   assert(g_extern.filter.colormap);

   // Set up conversion map from 16-bit XRGB1555 to 32-bit ARGB.
   for (int i = 0; i < 32768; i++)
   {
      unsigned r = (i >> 10) & 31;
      unsigned g = (i >>  5) & 31;
      unsigned b = (i >>  0) & 31;

      r = (r << 3) | (r >> 2);
      g = (g << 3) | (g >> 2);
      b = (b << 3) | (b >> 2);
      g_extern.filter.colormap[i] = (r << 16) | (g << 8) | (b << 0);
   }
}
#endif

static void deinit_filter(void)
{
   if (g_extern.filter.active)
   {
      g_extern.filter.active = false;
      dylib_close(g_extern.filter.lib);
      g_extern.filter.lib = NULL;
      free(g_extern.filter.buffer);
      free(g_extern.filter.colormap);
   }
}

#ifdef HAVE_XML
static void init_shader_dir(void)
{
   if (!*g_settings.video.shader_dir)
      return;

   g_extern.shader_dir.elems = dir_list_new(g_settings.video.shader_dir, ".shader");
   g_extern.shader_dir.size = 0;
   g_extern.shader_dir.ptr = 0;
   if (g_extern.shader_dir.elems)
   {
      while (g_extern.shader_dir.elems[g_extern.shader_dir.size])
      {
         SSNES_LOG("Found shader \"%s\"\n", g_extern.shader_dir.elems[g_extern.shader_dir.size]);
         g_extern.shader_dir.size++;
      }
   }
}

static void deinit_shader_dir(void)
{
   // It handles NULL, no worries :D
   dir_list_free(g_extern.shader_dir.elems);
   g_extern.shader_dir.elems = NULL;
   g_extern.shader_dir.size = 0;
   g_extern.shader_dir.ptr = 0;
}
#endif

void init_video_input(void)
{
#ifdef HAVE_DYLIB
   init_filter();
#endif

#ifdef HAVE_XML
   init_shader_dir();
#endif

   // We use at least 512x512 textures to accomodate for hi-res games.
   unsigned scale = 2;

   find_video_driver();
   find_input_driver();

   if (g_extern.filter.active)
      scale = g_extern.filter.scale;

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
         width = 224 * g_settings.video.xscale * g_settings.video.aspect_ratio;
         height = 224 * g_settings.video.yscale;
      }
      else
      {
         width = 256 * g_settings.video.xscale;
         height = 224 * g_settings.video.yscale;
      }
   }

   video_info_t video = {
      .width = width,
      .height = height,
      .fullscreen = g_settings.video.fullscreen,
      .vsync = g_settings.video.vsync,
      .force_aspect = g_settings.video.force_aspect,
      .smooth = g_settings.video.smooth,
      .input_scale = scale,
      .rgb32 = g_extern.filter.active
   };

   const input_driver_t *tmp = driver.input;
   driver.video_data = driver.video->init(&video, &driver.input, &driver.input_data);

   if (driver.video_data == NULL)
   {
      SSNES_ERR("Cannot open video driver ... Exiting ...\n");
      exit(1);
   }

   // Video driver didn't provide an input driver so we use configured one.
   if (driver.input == NULL)
   {
      driver.input = tmp;
      if (driver.input != NULL)
      {
         driver.input_data = driver.input->init();
         if (driver.input_data == NULL)
         {
            SSNES_ERR("Cannot init input driver. Exiting ...\n");
            exit(1);
         }
      }
      else
      {
         SSNES_ERR("Cannot find input driver. Exiting ...\n");
         exit(1);
      }
   }
}

void uninit_video_input(void)
{
   if (driver.input_data != driver.video_data && driver.input)
      driver.input->free(driver.input_data);

   if (driver.video_data && driver.video)
      driver.video->free(driver.video_data);

   deinit_filter();

#ifdef HAVE_XML
   deinit_shader_dir();
#endif
}

driver_t driver;

