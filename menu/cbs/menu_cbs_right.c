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

#include <file/file_path.h>

#include "../menu_driver.h"
#include "../menu_cbs.h"
#include "../menu_hash.h"
#include "../menu_input.h"
#include "../menu_setting.h"
#include "../menu_shader.h"
#include "../menu_navigation.h"

#include "../../cheats.h"
#include "../../general.h"
#include "../../retroarch.h"
#include "../../system.h"
#include "../../ui/ui_companion_driver.h"

#ifndef BIND_ACTION_RIGHT
#define BIND_ACTION_RIGHT(cbs, name) \
   cbs->action_right = name; \
   cbs->action_right_ident = #name;
#endif

#ifdef HAVE_SHADER_MANAGER
static int generic_shader_action_parameter_right(
      struct video_shader *shader, struct video_shader_parameter *param,
      unsigned type, const char *label, bool wraparound)
{
   if (!shader)
      return -1;

   param->current += param->step;
   param->current  = min(max(param->minimum, param->current), param->maximum);

   if (ui_companion_is_on_foreground())
      ui_companion_driver_notify_refresh();
   return 0;
}

int shader_action_parameter_right(unsigned type, const char *label, bool wraparound)
{
   struct video_shader          *shader = video_shader_driver_get_current_shader();
   struct video_shader_parameter *param = &shader->parameters[type - MENU_SETTINGS_SHADER_PARAMETER_0];
   return generic_shader_action_parameter_right(shader, param, type, label, wraparound);
}

int shader_action_parameter_preset_right(unsigned type, const char *label,
      bool wraparound)
{
   struct video_shader_parameter *param = NULL;
   struct video_shader      *shader     = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_SHADER_GET,
         &shader);

   param = shader ? 
      &shader->parameters[type - MENU_SETTINGS_SHADER_PRESET_PARAMETER_0] :
      NULL;
   return generic_shader_action_parameter_right(shader, param, type, label, wraparound);
}
#endif

int generic_action_cheat_toggle(size_t idx, unsigned type, const char *label,
      bool wraparound)
{
   cheat_manager_toggle_index(idx);

   return 0;
}

int action_right_cheat(unsigned type, const char *label,
      bool wraparound)
{
   size_t idx             = type - MENU_SETTINGS_CHEAT_BEGIN;
   return generic_action_cheat_toggle(idx, type, label,
         wraparound);
}

int action_right_input_desc(unsigned type, const char *label,
      bool wraparound)
{
   unsigned inp_desc_index_offset = type - MENU_SETTINGS_INPUT_DESC_BEGIN;
   unsigned inp_desc_user         = inp_desc_index_offset / (RARCH_FIRST_CUSTOM_BIND + 4);
   unsigned inp_desc_button_index_offset = inp_desc_index_offset - (inp_desc_user * (RARCH_FIRST_CUSTOM_BIND + 4));
   settings_t *settings = config_get_ptr();

   if (inp_desc_button_index_offset < RARCH_FIRST_CUSTOM_BIND)
   {
      if (settings->input.remap_ids[inp_desc_user][inp_desc_button_index_offset] < RARCH_FIRST_CUSTOM_BIND - 1)
         settings->input.remap_ids[inp_desc_user][inp_desc_button_index_offset]++;
   }
   else
   {
      if (settings->input.remap_ids[inp_desc_user][inp_desc_button_index_offset] < 4 - 1)
         settings->input.remap_ids[inp_desc_user][inp_desc_button_index_offset]++;
   }

   return 0;
}

static int action_right_scroll(unsigned type, const char *label,
      bool wraparound)
{
   size_t selection;
   size_t scroll_accel   = 0;
   unsigned scroll_speed = 0, fast_scroll_speed = 0;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return false;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SCROLL_ACCEL, &scroll_accel))
      return false;

   scroll_speed      = (max(scroll_accel, 2) - 2) / 4 + 1;
   fast_scroll_speed = 4 + 4 * scroll_speed;

   if (selection  + fast_scroll_speed < (menu_entries_get_size()))
   {
      size_t idx  = selection + fast_scroll_speed;
      bool scroll = true;
      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &idx);
      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET, &scroll);
   }
   else
   {
      if ((menu_entries_get_size() > 0))
         menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_LAST, NULL);
   }

   return 0;
}

static int action_right_mainmenu(unsigned type, const char *label,
      bool wraparound)
{
   size_t selection          = 0;
   menu_file_list_cbs_t *cbs = NULL;
   unsigned        push_list = 0;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   file_list_t *menu_stack   = menu_entries_get_menu_stack_ptr(0);
   settings_t      *settings = config_get_ptr();
   unsigned           action = MENU_ACTION_RIGHT;
   size_t          list_size = menu_driver_list_get_size(MENU_LIST_PLAIN);

   if (list_size == 1)
   {
      size_t list_size_horiz = menu_driver_list_get_size(MENU_LIST_HORIZONTAL);
      size_t list_size_tabs  = menu_driver_list_get_size(MENU_LIST_TABS);
      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &selection);

      if ((menu_driver_list_get_selection() != (list_size_horiz + list_size_tabs))
         || settings->menu.navigation.wraparound.enable)
         push_list = 1;
   }
   else
      push_list = 2;

   menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection);

   cbs = menu_entries_get_actiondata_at_offset(selection_buf, selection);

   switch (push_list)
   {
      case 1:
         menu_driver_list_cache(MENU_LIST_HORIZONTAL, action);

         if (cbs && cbs->action_content_list_switch)
            return cbs->action_content_list_switch(selection_buf, menu_stack,
                  "", "", 0);
         break;
      case 2:
         action_right_scroll(0, "", false);
         break;
      case 0:
      default:
         break;
   }

   return 0;
}

static int action_right_shader_scale_pass(unsigned type, const char *label,
      bool wraparound)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned pass = type - MENU_SETTINGS_SHADER_PASS_SCALE_0;
   struct video_shader *shader           = NULL;
   struct video_shader_pass *shader_pass = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_SHADER_GET,
         &shader);
   if (!shader)
      return -1;
   shader_pass = &shader->pass[pass];
   if (!shader_pass)
      return -1;

   {
      unsigned current_scale   = shader_pass->fbo.scale_x;
      unsigned delta           = 1;
      current_scale            = (current_scale + delta) % 6;

      shader_pass->fbo.valid   = current_scale;
      shader_pass->fbo.scale_x = shader_pass->fbo.scale_y = current_scale;

   }
#endif
   return 0;
}

static int action_right_shader_filter_pass(unsigned type, const char *label,
      bool wraparound)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned pass                         = type - MENU_SETTINGS_SHADER_PASS_FILTER_0;
   unsigned delta                        = 1;
   struct video_shader *shader           = NULL;
   struct video_shader_pass *shader_pass = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_SHADER_GET,
         &shader);
   if (!shader)
      return -1;
   shader_pass = &shader->pass[pass];
   if (!shader_pass)
      return -1;

   shader_pass->filter = ((shader_pass->filter + delta) % 3);
#endif
   return 0;
}

static int action_right_shader_filter_default(unsigned type, const char *label,
      bool wraparound)
{
#ifdef HAVE_SHADER_MANAGER
   rarch_setting_t *setting = menu_setting_find(menu_hash_to_str(MENU_LABEL_VIDEO_SMOOTH));
   if (!setting)
      return -1;
   return menu_action_handle_setting(setting, menu_setting_get_type(setting), MENU_ACTION_RIGHT,
         wraparound);
#else
   return 0;
#endif
}

static int action_right_cheat_num_passes(unsigned type, const char *label,
      bool wraparound)
{
   unsigned new_size = 0;

   new_size = cheat_manager_get_size() + 1;
   menu_entries_set_refresh(false);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   cheat_manager_realloc(new_size);

   return 0;
}

static int action_right_shader_num_passes(unsigned type, const char *label,
      bool wraparound)
{
#ifdef HAVE_SHADER_MANAGER
   struct video_shader *shader = NULL;

   menu_driver_ctl(RARCH_MENU_CTL_SHADER_GET,
         &shader);
   if (!shader)
      return -1;

   if ((shader->passes < GFX_MAX_SHADERS))
      shader->passes++;
   menu_entries_set_refresh(false);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   video_shader_resolve_parameters(NULL, shader);

#endif
   return 0;
}

static int action_right_video_resolution(unsigned type, const char *label,
      bool wraparound)
{
   video_driver_ctl(RARCH_DISPLAY_CTL_GET_NEXT_VIDEO_OUT, NULL);

   return 0;
}

static int playlist_association_right(unsigned type, const char *label,
      bool wraparound)
{
   size_t i, next, found, current = 0;
   char core_path[PATH_MAX_LENGTH]  = {0};
   core_info_t *info                = NULL;
   struct string_list *stnames      = NULL;
   struct string_list *stcores      = NULL;
   char new_playlist_cores[PATH_MAX_LENGTH] = {0};
   settings_t *settings             = config_get_ptr();
   const char *path                 = path_basename(label);
   core_info_list_t           *list = NULL;
   
   runloop_ctl(RUNLOOP_CTL_CURRENT_CORE_LIST_GET, &list);
   if (!list)
      return -1;

   stnames = string_split(settings->playlist_names, ";");
   stcores = string_split(settings->playlist_cores, ";");

   if (!menu_playlist_find_associated_core(path, core_path, sizeof(core_path)))
         strlcpy(core_path, "DETECT", sizeof(core_path));

   for (i = 0; i < list->count; i++)
   {
      core_info_t *info = core_info_get(list, i);
      if (!strcmp(info->path, core_path))
         current = i;
   }

   next = current + 1;
   if (next >= list->count)
   {
      if (wraparound)
         next = list->count-1;
      else
         next = 0;
   }

   info = core_info_get(list, next);

   found = string_list_find_elem(stnames, path);
   if (found)
      string_list_set(stcores, found-1, info->path);

   string_list_join_concat(new_playlist_cores, sizeof(new_playlist_cores), stcores, ";");

   strlcpy(settings->playlist_cores, new_playlist_cores, sizeof(settings->playlist_cores));

   return 0;
}

int core_setting_right(unsigned type, const char *label,
      bool wraparound)
{
   unsigned idx     = type - MENU_SETTINGS_CORE_OPTION_START;

   runloop_ctl(RUNLOOP_CTL_CORE_OPTION_NEXT, &idx);

   return 0;
}

static int disk_options_disk_idx_right(unsigned type, const char *label,
      bool wraparound)
{
   event_command(EVENT_CMD_DISK_NEXT);

   return 0;
}

int bind_right_generic(unsigned type, const char *label,
       bool wraparound)
{
   return menu_setting_set(type, label, MENU_ACTION_RIGHT, wraparound);
}

static int menu_cbs_init_bind_right_compare_type(menu_file_list_cbs_t *cbs,
      unsigned type, uint32_t label_hash, uint32_t menu_label_hash)
{
   if (type >= MENU_SETTINGS_CHEAT_BEGIN
         && type <= MENU_SETTINGS_CHEAT_END)
   {
      BIND_ACTION_RIGHT(cbs, action_right_cheat);
   }
#ifdef HAVE_SHADER_MANAGER
   else if (type >= MENU_SETTINGS_SHADER_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PARAMETER_LAST)
   {
      BIND_ACTION_RIGHT(cbs, shader_action_parameter_right);
   }
   else if (type >= MENU_SETTINGS_SHADER_PRESET_PARAMETER_0
         && type <= MENU_SETTINGS_SHADER_PRESET_PARAMETER_LAST)
   {
      BIND_ACTION_RIGHT(cbs, shader_action_parameter_preset_right);
   }
#endif
   else if (type >= MENU_SETTINGS_INPUT_DESC_BEGIN
         && type <= MENU_SETTINGS_INPUT_DESC_END)
   {
      BIND_ACTION_RIGHT(cbs, action_right_input_desc);
   }
   else if ((type >= MENU_SETTINGS_PLAYLIST_ASSOCIATION_START))
   {
      BIND_ACTION_RIGHT(cbs, playlist_association_right);
   }
   else if ((type >= MENU_SETTINGS_CORE_OPTION_START))
   {
      BIND_ACTION_RIGHT(cbs, core_setting_right);
   }
   else
   {
      switch (type)
      {
         case MENU_SETTINGS_CORE_DISK_OPTIONS_DISK_INDEX:
            BIND_ACTION_RIGHT(cbs, disk_options_disk_idx_right);
            break;
         case MENU_FILE_PLAIN:
         case MENU_FILE_DIRECTORY:
         case MENU_FILE_PARENT_DIRECTORY:
         case MENU_FILE_CARCHIVE:
         case MENU_FILE_IN_CARCHIVE:
         case MENU_FILE_CORE:
         case MENU_FILE_RDB:
         case MENU_FILE_RDB_ENTRY:
         case MENU_FILE_RPL_ENTRY:
         case MENU_FILE_CURSOR:
         case MENU_FILE_SHADER:
         case MENU_FILE_SHADER_PRESET:
         case MENU_FILE_IMAGE:
         case MENU_FILE_OVERLAY:
         case MENU_FILE_VIDEOFILTER:
         case MENU_FILE_AUDIOFILTER:
         case MENU_FILE_CONFIG:
         case MENU_FILE_USE_DIRECTORY:
         case MENU_FILE_PLAYLIST_ENTRY:
         case MENU_INFO_MESSAGE:
         case MENU_FILE_DOWNLOAD_CORE:
         case MENU_FILE_CHEAT:
         case MENU_FILE_REMAP:
         case MENU_FILE_MOVIE:
         case MENU_FILE_MUSIC:
         case MENU_FILE_IMAGEVIEWER:
         case MENU_FILE_PLAYLIST_COLLECTION:
         case MENU_FILE_DOWNLOAD_CORE_CONTENT:
         case MENU_FILE_SCAN_DIRECTORY:
         case MENU_SETTING_GROUP:
            switch (menu_label_hash)
            {
               case MENU_VALUE_HORIZONTAL_MENU:
               case MENU_VALUE_MAIN_MENU:
                  BIND_ACTION_RIGHT(cbs, action_right_mainmenu);
                  break;
               default:
                  BIND_ACTION_RIGHT(cbs, action_right_scroll);
                  break;
            }
         case MENU_SETTING_ACTION:
         case MENU_FILE_CONTENTLIST_ENTRY:
            BIND_ACTION_RIGHT(cbs, action_right_mainmenu);
            break;
         default:
            return -1;
      }
   }

   return 0;
}

static int menu_cbs_init_bind_right_compare_label(menu_file_list_cbs_t *cbs,
      const char *label, uint32_t label_hash, uint32_t menu_label_hash, const char *elem0)
{
   unsigned i;

   if (cbs->setting)
   {
      const char *parent_group   = menu_setting_get_parent_group(cbs->setting);
      uint32_t parent_group_hash = menu_hash_calculate(parent_group);

      if ((parent_group_hash == MENU_LABEL_SETTINGS) && (menu_setting_get_type(cbs->setting) == ST_GROUP))
      {
         BIND_ACTION_RIGHT(cbs, action_right_scroll);
         return 0;
      }
   }

   for (i = 0; i < MAX_USERS; i++)
   {
      uint32_t label_setting_hash;
      char label_setting[PATH_MAX_LENGTH];

      label_setting[0] = '\0';
      snprintf(label_setting, sizeof(label_setting), "input_player%d_joypad_index", i + 1);

      label_setting_hash = menu_hash_calculate(label_setting);

      if (label_hash != label_setting_hash)
         continue;

      BIND_ACTION_RIGHT(cbs, bind_right_generic);
      return 0;
   }

   if (strstr(label, "rdb_entry"))
   {
      BIND_ACTION_RIGHT(cbs, action_right_scroll);
   }
   else
   {
      switch (label_hash)
      {
         case MENU_LABEL_VIDEO_SHADER_SCALE_PASS:
            BIND_ACTION_RIGHT(cbs, action_right_shader_scale_pass);
            break;
         case MENU_LABEL_VIDEO_SHADER_FILTER_PASS:
            BIND_ACTION_RIGHT(cbs, action_right_shader_filter_pass);
            break;
         case MENU_LABEL_VIDEO_SHADER_DEFAULT_FILTER:
            BIND_ACTION_RIGHT(cbs, action_right_shader_filter_default);
            break;
         case MENU_LABEL_VIDEO_SHADER_NUM_PASSES:
            BIND_ACTION_RIGHT(cbs, action_right_shader_num_passes);
            break;
         case MENU_LABEL_CHEAT_NUM_PASSES:
            BIND_ACTION_RIGHT(cbs, action_right_cheat_num_passes);
            break;
         case MENU_LABEL_SCREEN_RESOLUTION:
            BIND_ACTION_RIGHT(cbs, action_right_video_resolution);
            break;
         case MENU_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE:
            switch (menu_label_hash)
            {
               case MENU_VALUE_HORIZONTAL_MENU:
               case MENU_VALUE_MAIN_MENU:
                  BIND_ACTION_RIGHT(cbs, action_right_mainmenu);
                  break;
            }
         default:
            return -1;
      }
   }

   return 0;
}

int menu_cbs_init_bind_right(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *elem0, const char *elem1, const char *menu_label,
      uint32_t label_hash, uint32_t menu_label_hash)
{
   if (!cbs)
      return -1;

   BIND_ACTION_RIGHT(cbs, bind_right_generic);
    
    if (type == MENU_SETTING_NO_ITEM)
    {
        switch (menu_label_hash)
        {
            case MENU_VALUE_HORIZONTAL_MENU:
            case MENU_VALUE_MAIN_MENU:
            case 153956705: /* TODO/FIXME - dehardcode */
                BIND_ACTION_RIGHT(cbs, action_right_mainmenu);
                return 0;
            default:
                break;
        }
    }

   if (menu_cbs_init_bind_right_compare_label(cbs, label, label_hash, menu_label_hash, elem0) == 0)
      return 0;

   if (menu_cbs_init_bind_right_compare_type(cbs, type, label_hash, menu_label_hash) == 0)
      return 0;

   return -1;
}
