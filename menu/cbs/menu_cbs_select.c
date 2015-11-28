/*  RetroArch - A frontend for libretro.
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

#include "../menu.h"
#include "../menu_entry.h"
#include "../menu_cbs.h"
#include "../menu_setting.h"

#include "../../runloop_data.h"

#ifndef BIND_ACTION_SELECT
#define BIND_ACTION_SELECT(cbs, name) \
   cbs->action_select = name; \
   cbs->action_select_ident = #name;
#endif

static int action_select_default(const char *path, const char *label, unsigned type,
      size_t idx)
{
   menu_entry_t entry;
   int ret                    = 0;
   enum menu_action action    = MENU_ACTION_NOOP;
   menu_file_list_cbs_t *cbs  = NULL;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   menu_entry_get(&entry, 0, idx, NULL, false);

   cbs = menu_entries_get_actiondata_at_offset(selection_buf, idx);

   if (!cbs)
      return -1;
    
   switch (menu_setting_get_type(cbs->setting))
   {
      case ST_BOOL:
      case ST_INT:
      case ST_UINT:
      case ST_FLOAT:
         action = MENU_ACTION_RIGHT;
         break;
      case ST_PATH:
      case ST_DIR:
      case ST_ACTION:
      case ST_STRING:
      case ST_HEX:
      case ST_BIND:
         action = MENU_ACTION_OK;
         break;
      default:
         break;
   }
    
   if (action == MENU_ACTION_NOOP)
   {
       if (cbs->action_ok)
           action = MENU_ACTION_OK;
       else
       {
           if (cbs->action_start)
               action = MENU_ACTION_START;
           if (cbs->action_right)
               action = MENU_ACTION_RIGHT;
       }
   }
    
   if (action != MENU_ACTION_NOOP)
       ret = menu_entry_action(&entry, idx, action);

   rarch_main_data_iterate(false, false);
    
   return ret;
}

static int action_select_path_use_directory(const char *path,
      const char *label, unsigned type, size_t idx)
{
   return action_ok_path_use_directory(path, label, type, idx, 0 /* unused */);
}

static int action_select_directory(const char *path, const char *label, unsigned type,
      size_t idx)
{
   return action_ok_directory_push(path, label, type, idx, 0 /* ignored */);
}

static int action_select_driver_setting(const char *path, const char *label, unsigned type,
      size_t idx)
{
   return bind_right_generic(type, label, true);
}

static int action_select_core_setting(const char *path, const char *label, unsigned type,
      size_t idx)
{
   return core_setting_right(type, label, true);
}

#ifdef HAVE_SHADER_MANAGER
static int shader_action_parameter_select(const char *path, const char *label, unsigned type,
      size_t idx)
{
   return shader_action_parameter_right(type, label, true);
}

static int shader_action_parameter_preset_select(const char *path, const char *label, unsigned type,
      size_t idx)
{
   return shader_action_parameter_preset_right(type, label, true);
}
#endif

static int action_select_cheat(const char *path, const char *label, unsigned type,
      size_t idx)
{
   return action_right_cheat(type, label, true);
}

static int action_select_input_desc(const char *path, const char *label, unsigned type,
      size_t idx)
{
   return action_right_input_desc(type, label, true);
}

static int menu_cbs_init_bind_select_compare_type(
      menu_file_list_cbs_t *cbs, unsigned type)
{
   if (type >= MENU_SETTINGS_CHEAT_BEGIN
         && type <= MENU_SETTINGS_CHEAT_END)
   {
      BIND_ACTION_SELECT(cbs, action_select_cheat);
   }
#ifdef HAVE_SHADER_MANAGER
   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
   {
      BIND_ACTION_SELECT(cbs, shader_action_parameter_select);
   }
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
   {
      BIND_ACTION_SELECT(cbs, shader_action_parameter_preset_select);
   }
#endif
   else if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_END)
   {
      BIND_ACTION_SELECT(cbs, action_select_input_desc);
   }
   else
   {
      switch (type)
      {
         case MENU_FILE_DIRECTORY:
            BIND_ACTION_SELECT(cbs, action_select_directory);
            break;
         case MENU_FILE_USE_DIRECTORY:
            BIND_ACTION_SELECT(cbs, action_select_path_use_directory);
            break;
         default:
            return -1;
      }
   }

   return 0;
}

static int menu_cbs_init_bind_select_compare_label(menu_file_list_cbs_t *cbs,
      const char *label, uint32_t hash, const char *elem0)
{
   return -1;
}

int menu_cbs_init_bind_select(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return -1;

   BIND_ACTION_SELECT(cbs, action_select_default);

   if (cbs->setting)
   {
      uint64_t flags = menu_setting_get_flags(cbs->setting);

      if (flags & SD_FLAG_IS_DRIVER)
      {
         BIND_ACTION_SELECT(cbs, action_select_driver_setting);
         return 0;
      }
   }

   if ((type >= MENU_SETTINGS_CORE_OPTION_START))
   {
      BIND_ACTION_SELECT(cbs, action_select_core_setting);
      return 0;
   }

   if (menu_cbs_init_bind_select_compare_label(cbs, label, label_hash, elem0) == 0)
      return 0;

   if (menu_cbs_init_bind_select_compare_type(cbs, type) == 0)
      return 0;

   return -1;
}
