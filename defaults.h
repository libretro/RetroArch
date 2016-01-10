/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifndef __RARCH_DEFAULTS_H
#define __RARCH_DEFAULTS_H

#include <stdint.h>

#include <retro_miscellaneous.h>

#ifndef IS_SALAMANDER
#include "playlist.h"
#endif

struct defaults
{
   struct
   {
      char core_assets[PATH_MAX_LENGTH];
      char menu_config[PATH_MAX_LENGTH];
      char autoconfig[PATH_MAX_LENGTH];
      char audio_filter[PATH_MAX_LENGTH];
      char video_filter[PATH_MAX_LENGTH];
      char assets[PATH_MAX_LENGTH];
      char core[PATH_MAX_LENGTH];
      char core_info[PATH_MAX_LENGTH];
      char overlay[PATH_MAX_LENGTH];
      char osk_overlay[PATH_MAX_LENGTH];
      char port[PATH_MAX_LENGTH];
      char shader[PATH_MAX_LENGTH];
      char savestate[PATH_MAX_LENGTH];
      char resampler[PATH_MAX_LENGTH];
      char sram[PATH_MAX_LENGTH];
      char screenshot[PATH_MAX_LENGTH];
      char system[PATH_MAX_LENGTH];
      char playlist[PATH_MAX_LENGTH];
      char content_history[PATH_MAX_LENGTH];
      char remap[PATH_MAX_LENGTH];
      char cache[PATH_MAX_LENGTH];
      char wallpapers[PATH_MAX_LENGTH];
      char database[PATH_MAX_LENGTH];
      char cursor[PATH_MAX_LENGTH];
      char cheats[PATH_MAX_LENGTH];
   } dir;

   struct
   {
      char config[PATH_MAX_LENGTH];
      char core[PATH_MAX_LENGTH];
      char buildbot_server_url[PATH_MAX_LENGTH];
   } path;

   struct
   {
      int out_latency;
      float video_refresh_rate;
      bool video_threaded_enable;
   } settings; 

#ifndef IS_SALAMANDER
   content_playlist_t *history;
#endif
};

/* Public data structures. */
extern struct defaults g_defaults;


#endif
