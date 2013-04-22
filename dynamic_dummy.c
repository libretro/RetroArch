/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
 *  Copyright (C) 2012-2013 - Michael Lelli
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

#include "libretro.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static uint16_t *frame_buf;

void libretro_dummy_retro_init(void)
{
   frame_buf = (uint16_t*)calloc(320 * 240, sizeof(uint16_t));
}

void libretro_dummy_retro_deinit(void)
{
   free(frame_buf);
   frame_buf = NULL;
}

unsigned libretro_dummy_retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void libretro_dummy_retro_set_controller_port_device(unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void libretro_dummy_retro_get_system_info(struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "No Core";
   info->library_version  = "";
   info->need_fullpath    = false;
   info->valid_extensions = ""; // Nothing.
}

// Doesn't really matter, but need something sane.
void libretro_dummy_retro_get_system_av_info(struct retro_system_av_info *info)
{
   info->timing.fps = 60.0;
   info->timing.sample_rate = 30000.0;

   info->geometry.base_width  = 320;
   info->geometry.base_height = 240;
   info->geometry.max_width   = 320;
   info->geometry.max_height  = 240;
   info->geometry.aspect_ratio = 4.0 / 3.0;
}

static retro_video_refresh_t dummy_video_cb;
static retro_audio_sample_t dummy_audio_cb;
static retro_audio_sample_batch_t dummy_audio_batch_cb;
static retro_environment_t dummy_environ_cb;
static retro_input_poll_t dummy_input_poll_cb;
static retro_input_state_t dummy_input_state_cb;

void libretro_dummy_retro_set_environment(retro_environment_t cb)
{
   dummy_environ_cb = cb;

   // We know it's supported, it's internal to RetroArch.
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;
   dummy_environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
}

void libretro_dummy_retro_set_audio_sample(retro_audio_sample_t cb)
{
   dummy_audio_cb = cb;
}

void libretro_dummy_retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
   dummy_audio_batch_cb = cb;
}

void libretro_dummy_retro_set_input_poll(retro_input_poll_t cb)
{
   dummy_input_poll_cb = cb;
}

void libretro_dummy_retro_set_input_state(retro_input_state_t cb)
{
   dummy_input_state_cb = cb;
}

void libretro_dummy_retro_set_video_refresh(retro_video_refresh_t cb)
{
   dummy_video_cb = cb;
}

void libretro_dummy_retro_reset(void)
{}

void libretro_dummy_retro_run(void)
{
   dummy_input_poll_cb();
   for (unsigned i = 0; i < 320 * 240; i++)
      frame_buf[i] = 4 << 5;
   dummy_video_cb(frame_buf, 320, 240, 640);
}

// This should never be called, it's only used as a placeholder.
bool libretro_dummy_retro_load_game(const struct retro_game_info *info)
{
   (void)info;
   return false;
}

void libretro_dummy_retro_unload_game(void)
{}

unsigned libretro_dummy_retro_get_region(void)
{
   return RETRO_REGION_NTSC;
}

bool libretro_dummy_retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num)
{
   (void)type;
   (void)info;
   (void)num;
   return false;
}

size_t libretro_dummy_retro_serialize_size(void)
{
   return 0;
}

bool libretro_dummy_retro_serialize(void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

bool libretro_dummy_retro_unserialize(const void *data, size_t size)
{
   (void)data;
   (void)size;
   return false;
}

void *libretro_dummy_retro_get_memory_data(unsigned id)
{
   (void)id;
   return NULL;
}

size_t libretro_dummy_retro_get_memory_size(unsigned id)
{
   (void)id;
   return 0;
}

void libretro_dummy_retro_cheat_reset(void)
{}

void libretro_dummy_retro_cheat_set(unsigned index, bool enabled, const char *code)
{
   (void)index;
   (void)enabled;
   (void)code;
}


