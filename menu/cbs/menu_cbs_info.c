/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include "../menu_driver.h"
#include "../menu_cbs.h"

#include "../widgets/menu_dialog.h"

#include "../../configuration.h"

#ifndef BIND_ACTION_INFO
#define BIND_ACTION_INFO(cbs, name) \
   cbs->action_info = name; \
   cbs->action_info_ident = #name;
#endif

#ifdef HAVE_NETWORKING
#include "../../network/netplay/netplay_discovery.h"
#endif

static int action_info_default(unsigned type, const char *label)
{
   menu_displaylist_info_t info;
   file_list_t *menu_stack      = menu_entries_get_menu_stack_ptr(0);
   size_t selection             = menu_navigation_get_selection();
#ifdef HAVE_AUDIOMIXER
   settings_t *settings         = config_get_ptr();
#endif

   menu_displaylist_info_init(&info);

   info.list                    = menu_stack;
   info.directory_ptr           = selection;
   info.enum_idx                = MENU_ENUM_LABEL_INFO_SCREEN;
   info.label                   = strdup(
         msg_hash_to_str(MENU_ENUM_LABEL_INFO_SCREEN));

   if (!menu_displaylist_ctl(DISPLAYLIST_HELP, &info))
      goto error;

#ifdef HAVE_AUDIOMIXER
   if (settings->bools.audio_enable_menu && settings->bools.audio_enable_menu_notice)
      audio_driver_mixer_play_menu_sound(AUDIO_MIXER_SYSTEM_SLOT_NOTICE);
#endif

   if (!menu_displaylist_process(&info))
      goto error;

   menu_displaylist_info_free(&info);

   return 0;

error:
   menu_displaylist_info_free(&info);
   return -1;
}

#ifdef HAVE_CHEEVOS
int  generic_action_ok_help(const char *path,
      const char *label, unsigned type, size_t idx, size_t entry_idx,
      enum msg_hash_enums id, enum menu_dialog_type id2);

static int action_info_cheevos(unsigned type, const char *label)
{
   unsigned new_id        = type - MENU_SETTINGS_CHEEVOS_START;

   menu_dialog_set_current_id(new_id);

   return generic_action_ok_help(NULL, label, new_id, 0, 0,
      MENU_ENUM_LABEL_CHEEVOS_DESCRIPTION,
      MENU_DIALOG_HELP_CHEEVOS_DESCRIPTION);
}
#endif

int menu_cbs_init_bind_info(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx)
{
   if (!cbs)
      return -1;

#ifdef HAVE_CHEEVOS
   if ((type >= MENU_SETTINGS_CHEEVOS_START) &&
      (type < MENU_SETTINGS_NETPLAY_ROOMS_START))
   {
      BIND_ACTION_INFO(cbs, action_info_cheevos);
      return 0;
   }
#endif

   BIND_ACTION_INFO(cbs, action_info_default);

   return -1;
}
