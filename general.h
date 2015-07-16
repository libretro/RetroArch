/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef __RARCH_GENERAL_H
#define __RARCH_GENERAL_H

#include <boolean.h>
#include <stdint.h>
#include <limits.h>
#include <compat/strl.h>
#include <retro_inline.h>
#include <retro_miscellaneous.h>
#include "configuration.h"
#include "driver.h"
#include "playlist.h"
#include "runloop.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "1.2.2"
#endif

/* Platform-specific headers */

/* Windows */
#ifdef _WIN32
#ifdef _XBOX
#include <xtl.h>
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include <compat/posix_string.h>
#endif

#ifdef HAVE_COMMAND
#include "command.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum sound_mode_enums
{
   SOUND_MODE_NORMAL = 0,
#ifdef HAVE_RSOUND
   SOUND_MODE_RSOUND,
#endif
#ifdef HAVE_HEADSET
   SOUND_MODE_HEADSET,
#endif
   SOUND_MODE_LAST
};

struct defaults
{
   char core_assets_dir[PATH_MAX_LENGTH];
   char menu_config_dir[PATH_MAX_LENGTH];
   char config_path[PATH_MAX_LENGTH];
   char core_path[PATH_MAX_LENGTH];
   char autoconfig_dir[PATH_MAX_LENGTH];
   char audio_filter_dir[PATH_MAX_LENGTH];
   char video_filter_dir[PATH_MAX_LENGTH];
   char assets_dir[PATH_MAX_LENGTH];
   char core_dir[PATH_MAX_LENGTH];
   char core_info_dir[PATH_MAX_LENGTH];
   char overlay_dir[PATH_MAX_LENGTH];
   char osk_overlay_dir[PATH_MAX_LENGTH];
   char port_dir[PATH_MAX_LENGTH];
   char shader_dir[PATH_MAX_LENGTH];
   char savestate_dir[PATH_MAX_LENGTH];
   char resampler_dir[PATH_MAX_LENGTH];
   char sram_dir[PATH_MAX_LENGTH];
   char screenshot_dir[PATH_MAX_LENGTH];
   char system_dir[PATH_MAX_LENGTH];
   char playlist_dir[PATH_MAX_LENGTH];
   char content_history_dir[PATH_MAX_LENGTH];
   char remap_dir[PATH_MAX_LENGTH];
   char extraction_dir[PATH_MAX_LENGTH];
   char wallpapers_dir[PATH_MAX_LENGTH];
   char database_dir[PATH_MAX_LENGTH];
   char cursor_dir[PATH_MAX_LENGTH];
   char cheats_dir[PATH_MAX_LENGTH];

   struct
   {
      int out_latency;
      float video_refresh_rate;
      bool video_threaded_enable;
   } settings; 

   content_playlist_t *history;
};

/* Public data structures. */
extern struct defaults g_defaults;

#ifdef __cplusplus
}
#endif

/**
 * db_to_gain:
 * @db          : Decibels.
 *
 * Converts decibels to voltage gain.
 *
 * Returns: voltage gain value.
 **/
static INLINE float db_to_gain(float db)
{
   return powf(10.0f, db / 20.0f);
}

/**
 * rarch_fail:
 * @error_code  : Error code.
 * @error       : Error message to show.
 *
 * Sanely kills the program.
 **/
static INLINE void rarch_fail(int error_code, const char *error)
{
   global_t *global = global_get_ptr();

   if (!global)
      return;

   /* We cannot longjmp unless we're in rarch_main_init().
    * If not, something went very wrong, and we should 
    * just exit right away. */
   rarch_assert(global->error_in_init);

   strlcpy(global->error_string, error,
         sizeof(global->error_string));
   longjmp(global->error_sjlj_context, error_code);
}

#endif


