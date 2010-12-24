/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
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
#include "config.h"
#include "general.h"
#include <stdio.h>

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

void init_audio(void)
{
   if (!audio_enable)
   {
      audio_active = false;
      return;
   }

   driver.audio_data = driver.audio->init(audio_device, out_rate, out_latency);
   if ( driver.audio_data == NULL )
      audio_active = false;

   if (!audio_sync && audio_active)
      driver.audio->set_nonblock_state(driver.audio_data, true);

   int err;
   source = src_new(SAMPLERATE_QUALITY, 2, &err);
   if (!source)
      audio_active = false;
}

void uninit_audio(void)
{
   if (!audio_enable)
   {
      audio_active = false;
      return;
   }

   if ( driver.audio_data && driver.audio )
      driver.audio->free(driver.audio_data);

   if ( source )
      src_delete(source);
}

void init_video_input(void)
{
   int scale;

   // We multiply scales with 2 to allow for hi-res games.
#if VIDEO_FILTER == FILTER_NONE
   scale = 2;
#elif VIDEO_FILTER == FILTER_HQ2X
   scale = 4;
#elif VIDEO_FILTER == FILTER_HQ4X
   scale = 8;
#elif VIDEO_FILTER == FILTER_NTSC
   scale = 8;
#elif VIDEO_FILTER == FILTER_GRAYSCALE
   scale = 2;
#elif VIDEO_FILTER == FILTER_BLEED
   scale = 2;
#else
   scale = 2;
#endif

   video_info_t video = {
      .width = (fullscreen) ? fullscreen_x : (296 * xscale),
      .height = (fullscreen) ? fullscreen_y : (224 * yscale),
      .fullscreen = fullscreen,
      .vsync = vsync,
      .force_aspect = force_aspect,
      .smooth = video_smooth,
      .input_scale = scale,
   };

   const input_driver_t *tmp = driver.input;
   driver.video_data = driver.video->init(&video, &(driver.input));

   if ( driver.video_data == NULL )
   {
      SSNES_ERR("Cannot open video driver... Exiting ...\n");
      exit(1);
   }

   if ( driver.input != NULL )
   {
      driver.input_data = driver.video_data;
   }
   else
   {
      driver.input = tmp;
      if (driver.input != NULL)
      {
         driver.input_data = driver.input->init();
         if ( driver.input_data == NULL )
            exit(1);
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
   if ( driver.video_data && driver.video )
      driver.video->free(driver.video_data);

   if ( driver.input_data != driver.video_data && driver.input )
      driver.input->free(driver.input_data);
}

bool video_active = true;
bool audio_active = true;

driver_t driver = {
#if VIDEO_DRIVER == VIDEO_GL
   .video = &video_gl,
#else
#error "Define a valid video driver in config.h"
#endif

#if AUDIO_DRIVER == AUDIO_RSOUND
   .audio = &audio_rsound,
#elif AUDIO_DRIVER == AUDIO_OSS
   .audio = &audio_oss,
#elif AUDIO_DRIVER == AUDIO_ALSA
   .audio = &audio_alsa,
#elif AUDIO_DRIVER == AUDIO_ROAR
   .audio = &audio_roar,
#elif AUDIO_DRIVER == AUDIO_AL
   .audio = &audio_openal,
#else
#error "Define a valid audio driver in config.h"
#endif
};

