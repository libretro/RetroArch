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

#include <compat/strl.h>
#include <file/file_path.h>

#include "../menu_driver.h"
#include "../menu_cbs.h"
#include "../menu_navigation.h"
#include "../../file_path_special.h"

#ifndef BIND_ACTION_SUBLABEL
#define BIND_ACTION_SUBLABEL(cbs, name) \
   cbs->action_sublabel = name; \
   cbs->action_sublabel_ident = #name;
#endif

static int action_bind_sublabel_generic(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   return 0;
}

static int action_bind_sublabel_video_settings_list(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_VIDEO_SETTINGS), len);
   return 0;
}

static int action_bind_sublabel_suspend_screensaver_enable(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_SUSPEND_SCREENSAVER_ENABLE), len);
   return 0;
}

static int action_bind_sublabel_audio_settings_list(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_AUDIO_SETTINGS), len);
   return 0;
}

static int action_bind_sublabel_fps_show(
      file_list_t *list,
      unsigned type, unsigned i,
      const char *label, const char *path,
      char *s, size_t len)
{
   strlcpy(s, msg_hash_to_str(MENU_ENUM_SUBLABEL_FPS_SHOW), len);
   return 0;
}

int menu_cbs_init_bind_sublabel(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return -1;

   BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_generic);

   if (cbs->enum_idx != MSG_UNKNOWN)
   {
      switch (cbs->enum_idx)
      {
         case MENU_ENUM_LABEL_FPS_SHOW:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_fps_show);
            break;
         case MENU_ENUM_LABEL_VIDEO_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_video_settings_list);
            break;
         case MENU_ENUM_LABEL_AUDIO_SETTINGS:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_audio_settings_list);
            break;
         case MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE:
            BIND_ACTION_SUBLABEL(cbs, action_bind_sublabel_suspend_screensaver_enable);
            break;
         default:
         case MSG_UNKNOWN:
            return -1;
      }
   }

   return 0;
}
