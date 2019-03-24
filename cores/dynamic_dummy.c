/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
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
#include <stdlib.h>
#include <string.h>

#include <libretro.h>

#if defined(HAVE_MENU) && defined(HAVE_RGUI)
#include <string/stdstring.h>
#include "../configuration.h"
#include "../menu/menu_defines.h"
#endif

#include "internal_cores.h"

static uint16_t *dummy_frame_buf;

static uint16_t frame_buf_width;
static uint16_t frame_buf_height;

#if defined(HAVE_LIBNX) && defined(HAVE_STATIC_DUMMY)
void retro_init(void) { libretro_dummy_retro_init(); }
void retro_deinit(void) { libretro_dummy_retro_deinit(); }
unsigned retro_api_version(void) { return libretro_dummy_retro_api_version(); }
void retro_set_controller_port_device(unsigned port, unsigned device) { libretro_dummy_retro_set_controller_port_device(port, device); }
void retro_get_system_info(struct retro_system_info *info) { libretro_dummy_retro_get_system_info(info); }
void retro_get_system_av_info(struct retro_system_av_info *info) { retro_get_system_av_info(info); }
void retro_set_environment(retro_environment_t cb) { libretro_dummy_retro_set_environment(cb); }
void retro_set_audio_sample(retro_audio_sample_t cb) { libretro_dummy_retro_set_audio_sample(cb); }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { libretro_dummy_retro_set_audio_sample_batch(cb); }
void retro_set_input_poll(retro_input_poll_t cb) { libretro_dummy_retro_set_input_poll(cb); }
void retro_set_input_state(retro_input_state_t cb) { libretro_dummy_retro_set_input_state(cb); }
void retro_set_video_refresh(retro_video_refresh_t cb) { libretro_dummy_retro_set_video_refresh(cb); }
void retro_reset(void) { libretro_dummy_retro_reset(); }
void retro_run(void) { libretro_dummy_retro_run(); }
bool retro_load_game(const struct retro_game_info *info) { return libretro_dummy_retro_load_game(info); }
void retro_unload_game(void) { libretro_dummy_retro_unload_game(); }
unsigned retro_get_region(void) { return libretro_dummy_retro_get_region(); }
bool retro_load_game_special(unsigned type, const struct retro_game_info *info, size_t num) { return libretro_dummy_retro_load_game_special(type, info, num); }
size_t retro_serialize_size(void) { return libretro_dummy_retro_serialize_size(); }
bool retro_serialize(void *data, size_t size) { return libretro_dummy_retro_serialize(data, size); }
bool retro_unserialize(const void *data, size_t size) { return libretro_dummy_retro_unserialize(data, size); }
void *retro_get_memory_data(unsigned id) { return libretro_dummy_retro_get_memory_data(id); }
size_t retro_get_memory_size(unsigned id) { return libretro_dummy_retro_get_memory_size(id); }
void retro_cheat_reset(void) { libretro_dummy_retro_cheat_reset(); }
void retro_cheat_set(unsigned idx, bool enabled, const char *code) { libretro_dummy_retro_cheat_set(idx, enabled, code); }
#endif

void libretro_dummy_retro_init(void)
{
#if defined(HAVE_MENU) && defined(HAVE_RGUI)
   settings_t *settings = config_get_ptr();
#endif
   unsigned i;

   /* Sensible defaults */
   frame_buf_width = 320;
   frame_buf_height = 240;

#if defined(HAVE_MENU) && defined(HAVE_RGUI)
   if (string_is_equal(settings->arrays.menu_driver, "rgui"))
   {
      switch (settings->uints.menu_rgui_aspect_ratio)
      {
         case RGUI_ASPECT_RATIO_16_9:
         case RGUI_ASPECT_RATIO_16_9_CENTRE:
            frame_buf_width = 426;
            break;
         case RGUI_ASPECT_RATIO_16_10:
         case RGUI_ASPECT_RATIO_16_10_CENTRE:
            frame_buf_width = 384;
            break;
         default:
            /* 4:3 */
            frame_buf_width = 320;
            break;
      }
   }
#endif

   dummy_frame_buf = (uint16_t*)calloc(frame_buf_width * frame_buf_height, sizeof(uint16_t));
   for (i = 0; i < (unsigned)(frame_buf_width * frame_buf_height); i++)
      dummy_frame_buf[i] = 4 << 5;
}

void libretro_dummy_retro_deinit(void)
{
   if (dummy_frame_buf)
      free(dummy_frame_buf);
   dummy_frame_buf = NULL;
}

unsigned libretro_dummy_retro_api_version(void)
{
   return RETRO_API_VERSION;
}

void libretro_dummy_retro_set_controller_port_device(
      unsigned port, unsigned device)
{
   (void)port;
   (void)device;
}

void libretro_dummy_retro_get_system_info(
      struct retro_system_info *info)
{
   memset(info, 0, sizeof(*info));
   info->library_name     = "";
   info->library_version  = "";
   info->need_fullpath    = false;
   info->valid_extensions = ""; /* Nothing. */
}

static retro_video_refresh_t dummy_video_cb;
static retro_audio_sample_t dummy_audio_cb;
static retro_audio_sample_batch_t dummy_audio_batch_cb;
static retro_environment_t dummy_environ_cb;
static retro_input_poll_t dummy_input_poll_cb;
static retro_input_state_t dummy_input_state_cb;

/* Doesn't really matter, but need something sane. */
void libretro_dummy_retro_get_system_av_info(
      struct retro_system_av_info *info)
{
   float refresh_rate = 0.0;
   if (!dummy_environ_cb(RETRO_ENVIRONMENT_GET_TARGET_REFRESH_RATE, &refresh_rate))
      refresh_rate    = 60.0;

   info->timing.fps           = refresh_rate;
   info->timing.sample_rate   = 30000.0;

   info->geometry.base_width  = frame_buf_width;
   info->geometry.base_height = frame_buf_height;
   info->geometry.max_width   = frame_buf_width;
   info->geometry.max_height  = frame_buf_height;
   info->geometry.aspect_ratio = (float)frame_buf_width / (float)frame_buf_height;
}

void libretro_dummy_retro_set_environment(retro_environment_t cb)
{
   enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_RGB565;

   dummy_environ_cb = cb;

   /* We know it's supported, it's internal to RetroArch. */
   dummy_environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt);
}

void libretro_dummy_retro_set_audio_sample(retro_audio_sample_t cb)
{
   dummy_audio_cb = cb;
}

void libretro_dummy_retro_set_audio_sample_batch(
      retro_audio_sample_batch_t cb)
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
   dummy_video_cb(dummy_frame_buf, frame_buf_width, frame_buf_height, 2 * frame_buf_width);
}

/* This should never be called, it's only used as a placeholder. */
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

bool libretro_dummy_retro_load_game_special(unsigned type,
      const struct retro_game_info *info, size_t num)
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

bool libretro_dummy_retro_unserialize(const void *data,
      size_t size)
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

void libretro_dummy_retro_cheat_set(unsigned idx,
      bool enabled, const char *code)
{
   (void)idx;
   (void)enabled;
   (void)code;
}
