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
#include <file/file_path.h>
#include <lists/string_list.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../menu_content.h"
#include "../menu_driver.h"
#include "../menu_cbs.h"
#include "../menu_input.h"
#include "../menu_setting.h"
#include "../menu_shader.h"

#include "../widgets/menu_list.h"

#include "../../configuration.h"
#include "../../core.h"
#include "../../core_info.h"
#include "../../managers/cheat_manager.h"
#include "../../file_path_special.h"
#include "../../retroarch.h"
#include "../../ui/ui_companion_driver.h"

#ifndef BIND_ACTION_RIGHT
#define BIND_ACTION_RIGHT(cbs, name) \
   do { \
      cbs->action_right = name; \
      cbs->action_right_ident = #name; \
   } while(0)
#endif

#ifdef HAVE_SHADER_MANAGER
static int generic_shader_action_parameter_right(struct video_shader_parameter *param,
      unsigned type, const char *label, bool wraparound)
{
   param->current += param->step;
   param->current  = MIN(MAX(param->minimum, param->current), param->maximum);

   if (ui_companion_is_on_foreground())
      ui_companion_driver_notify_refresh();
   return 0;
}

int shader_action_parameter_right(unsigned type, const char *label, bool wraparound)
{
   video_shader_ctx_t shader_info;
   struct video_shader_parameter *param = NULL;

   video_shader_driver_get_current_shader(&shader_info);

   param = &shader_info.data->parameters[type - MENU_SETTINGS_SHADER_PARAMETER_0];
   if (!param)
      return menu_cbs_exit();
   return generic_shader_action_parameter_right(param, type, label, wraparound);
}

int shader_action_parameter_preset_right(unsigned type, const char *label,
      bool wraparound)
{
   struct video_shader_parameter *param = menu_shader_manager_get_parameters(
         type - MENU_SETTINGS_SHADER_PRESET_PARAMETER_0);
   if (!param)
      return menu_cbs_exit();
   return generic_shader_action_parameter_right(param, type, label, wraparound);
}
#endif

int generic_action_cheat_toggle(size_t idx, unsigned type, const char *label,
      bool wraparound)
{
   cheat_manager_toggle_index((unsigned)idx);

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
      if (settings->uints.input_remap_ids[inp_desc_user][inp_desc_button_index_offset] < RARCH_FIRST_CUSTOM_BIND - 1)
         settings->uints.input_remap_ids[inp_desc_user][inp_desc_button_index_offset]++;
   }
   else
   {
      if (settings->uints.input_remap_ids[inp_desc_user][inp_desc_button_index_offset] < 4 - 1)
         settings->uints.input_remap_ids[inp_desc_user][inp_desc_button_index_offset]++;
   }

   return 0;
}

static int action_right_scroll(unsigned type, const char *label,
      bool wraparound)
{
   size_t scroll_accel   = 0;
   unsigned scroll_speed = 0, fast_scroll_speed = 0;
   size_t selection      = menu_navigation_get_selection();

   if (!menu_driver_ctl(MENU_NAVIGATION_CTL_GET_SCROLL_ACCEL, &scroll_accel))
      return false;

   scroll_speed      = (unsigned)((MAX(scroll_accel, 2) - 2) / 4 + 1);
   fast_scroll_speed = 4 + 4 * scroll_speed;

   if (selection  + fast_scroll_speed < (menu_entries_get_size()))
   {
      size_t idx  = selection + fast_scroll_speed;

      menu_navigation_set_selection(idx);
      menu_driver_navigation_set(true);
   }
   else
   {
      if ((menu_entries_get_size() > 0))
         menu_driver_ctl(MENU_NAVIGATION_CTL_SET_LAST, NULL);
   }

   return 0;
}

static int action_right_goto_tab(void)
{
   menu_ctx_list_t list_info;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);
   file_list_t *menu_stack    = menu_entries_get_menu_stack_ptr(0);
   size_t selection           = menu_navigation_get_selection();
   menu_file_list_cbs_t *cbs  = menu_entries_get_actiondata_at_offset(selection_buf, selection);

   list_info.type             = MENU_LIST_HORIZONTAL;
   list_info.action           = MENU_ACTION_RIGHT;

   menu_driver_ctl(RARCH_MENU_CTL_LIST_CACHE, &list_info);

   if (cbs && cbs->action_content_list_switch)
      return cbs->action_content_list_switch(selection_buf, menu_stack,
            "", "", 0);

   return 0;
}

static int action_right_mainmenu(unsigned type, const char *label,
      bool wraparound)
{
   menu_ctx_list_t list_info;

   menu_driver_ctl(RARCH_MENU_CTL_LIST_GET_SELECTION, &list_info);

   list_info.type = MENU_LIST_PLAIN;

   menu_driver_ctl(RARCH_MENU_CTL_LIST_GET_SIZE,      &list_info);

   if (list_info.size == 1)
   {
      menu_ctx_list_t list_horiz_info;
      menu_ctx_list_t list_tabs_info;
      settings_t      *settings = config_get_ptr();

      list_horiz_info.type      = MENU_LIST_HORIZONTAL;
      list_tabs_info.type       = MENU_LIST_TABS;

      menu_driver_ctl(RARCH_MENU_CTL_LIST_GET_SIZE, &list_horiz_info);
      menu_driver_ctl(RARCH_MENU_CTL_LIST_GET_SIZE, &list_tabs_info);

      if ((list_info.selection != (list_horiz_info.size + list_tabs_info.size))
         || settings->bools.menu_navigation_wraparound_enable)
         return action_right_goto_tab();
   }
   else
      action_right_scroll(0, "", false);

   return 0;
}

static int action_right_shader_scale_pass(unsigned type, const char *label,
      bool wraparound)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned current_scale, delta;
   unsigned pass                         = 
      type - MENU_SETTINGS_SHADER_PASS_SCALE_0;
   struct video_shader_pass *shader_pass = menu_shader_manager_get_pass(pass);

   if (!shader_pass)
      return menu_cbs_exit();

   current_scale            = shader_pass->fbo.scale_x;
   delta                    = 1;
   current_scale            = (current_scale + delta) % 6;

   shader_pass->fbo.valid   = current_scale;
   shader_pass->fbo.scale_x = shader_pass->fbo.scale_y = current_scale;
#endif
   return 0;
}

static int action_right_shader_filter_pass(unsigned type, const char *label,
      bool wraparound)
{
#ifdef HAVE_SHADER_MANAGER
   unsigned pass                         = type - MENU_SETTINGS_SHADER_PASS_FILTER_0;
   unsigned delta                        = 1;
   struct video_shader_pass *shader_pass = menu_shader_manager_get_pass(pass);

   if (!shader_pass)
      return menu_cbs_exit();

   shader_pass->filter = ((shader_pass->filter + delta) % 3);
#endif
   return 0;
}

static int action_right_shader_filter_default(unsigned type, const char *label,
      bool wraparound)
{
#ifdef HAVE_SHADER_MANAGER
   rarch_setting_t *setting = menu_setting_find_enum(MENU_ENUM_LABEL_VIDEO_SMOOTH);
   if (!setting)
      return menu_cbs_exit();
   return menu_action_handle_setting(setting,
         setting_get_type(setting), MENU_ACTION_RIGHT,
         wraparound);
#else
   return 0;
#endif
}

static int action_right_cheat_num_passes(unsigned type, const char *label,
      bool wraparound)
{
   bool refresh      = false;
   unsigned new_size = 0;

   new_size = cheat_manager_get_size() + 1;
   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   cheat_manager_realloc(new_size);

   return 0;
}

static int action_right_shader_num_passes(unsigned type, const char *label,
      bool wraparound)
{
#ifdef HAVE_SHADER_MANAGER
   bool refresh                = false;
   struct video_shader *shader = menu_shader_get();

   if (!shader)
      return menu_cbs_exit();

   if ((menu_shader_manager_get_amount_passes() < GFX_MAX_SHADERS))
      menu_shader_manager_increment_amount_passes();

   menu_entries_ctl(MENU_ENTRIES_CTL_SET_REFRESH, &refresh);
   menu_driver_ctl(RARCH_MENU_CTL_SET_PREVENT_POPULATE, NULL);
   video_shader_resolve_parameters(NULL, shader);

#endif
   return 0;
}

static int action_right_video_resolution(unsigned type, const char *label,
      bool wraparound)
{
   video_driver_get_next_video_out();
   return 0;
}

static int playlist_association_right(unsigned type, const char *label,
      bool wraparound)
{
   char core_path[PATH_MAX_LENGTH];
   char new_playlist_cores[PATH_MAX_LENGTH];
   size_t i, next, found, current   = 0;
   core_info_t *info                = NULL;
   struct string_list *stnames      = NULL;
   struct string_list *stcores      = NULL;
   core_info_list_t           *list = NULL;
   settings_t *settings             = config_get_ptr();
   const char *path                 = path_basename(label);
   
   core_info_get_list(&list);
   if (!list)
      return menu_cbs_exit();

   core_path[0] = new_playlist_cores[0] = '\0';

   stnames = string_split(settings->arrays.playlist_names, ";");
   stcores = string_split(settings->arrays.playlist_cores, ";");

   if (!menu_content_playlist_find_associated_core(path, core_path, sizeof(core_path)))
         strlcpy(core_path,
               file_path_str(FILE_PATH_DETECT), sizeof(core_path));

   for (i = 0; i < list->count; i++)
   {
      core_info_t *info = core_info_get(list, i);
      if (string_is_equal(info->path, core_path))
         current = i;
   }

   next = current + 1;
   if (next >= list->count)
   {
      if (wraparound)
         next = 0;
      else
         next = list->count-1;
   }

   info = core_info_get(list, next);

   found = string_list_find_elem(stnames, path);
   if (found && info)
      string_list_set(stcores, (unsigned)(found-1), info->path);

   string_list_join_concat(new_playlist_cores,
         sizeof(new_playlist_cores), stcores, ";");

   strlcpy(settings->arrays.playlist_cores,
         new_playlist_cores, sizeof(settings->arrays.playlist_cores));

   string_list_free(stnames);
   string_list_free(stcores);
   return 0;
}

int core_setting_right(unsigned type, const char *label,
      bool wraparound)
{
   unsigned idx     = type - MENU_SETTINGS_CORE_OPTION_START;

   rarch_ctl(RARCH_CTL_CORE_OPTION_NEXT, &idx);

   return 0;
}

static int disk_options_disk_idx_right(unsigned type, const char *label,
      bool wraparound)
{
   command_event(CMD_EVENT_DISK_NEXT, NULL);

   return 0;
}

int bind_right_generic(unsigned type, const char *label,
       bool wraparound)
{
   return menu_setting_set(type, label, MENU_ACTION_RIGHT, wraparound);
}

static int menu_cbs_init_bind_right_compare_type(menu_file_list_cbs_t *cbs,
      unsigned type, const char *menu_label)
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
         case FILE_TYPE_PLAIN:
         case FILE_TYPE_DIRECTORY:
         case FILE_TYPE_PARENT_DIRECTORY:
         case FILE_TYPE_CARCHIVE:
         case FILE_TYPE_IN_CARCHIVE:
         case FILE_TYPE_CORE:
         case FILE_TYPE_RDB:
         case FILE_TYPE_RDB_ENTRY:
         case FILE_TYPE_RPL_ENTRY:
         case FILE_TYPE_CURSOR:
         case FILE_TYPE_SHADER:
         case FILE_TYPE_SHADER_PRESET:
         case FILE_TYPE_IMAGE:
         case FILE_TYPE_OVERLAY:
         case FILE_TYPE_VIDEOFILTER:
         case FILE_TYPE_AUDIOFILTER:
         case FILE_TYPE_CONFIG:
         case FILE_TYPE_USE_DIRECTORY:
         case FILE_TYPE_PLAYLIST_ENTRY:
         case MENU_INFO_MESSAGE:
         case FILE_TYPE_DOWNLOAD_CORE:
         case FILE_TYPE_CHEAT:
         case FILE_TYPE_REMAP:
         case FILE_TYPE_MOVIE:
         case FILE_TYPE_MUSIC:
         case FILE_TYPE_IMAGEVIEWER:
         case FILE_TYPE_PLAYLIST_COLLECTION:
         case FILE_TYPE_DOWNLOAD_CORE_CONTENT:
         case FILE_TYPE_DOWNLOAD_THUMBNAIL_CONTENT:
         case FILE_TYPE_DOWNLOAD_URL:
         case FILE_TYPE_SCAN_DIRECTORY:
         case FILE_TYPE_FONT:
         case MENU_SETTING_GROUP:
         case MENU_SETTINGS_CORE_INFO_NONE:
            if (  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HISTORY_TAB))   ||
                  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES_TAB)) ||
                  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB)) ||
                  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TAB)) ||
                  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_TAB)) ||
                  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_MUSIC_TAB)) ||
                  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_IMAGES_TAB)) ||
                  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_TAB)) ||
                  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU)) ||
                  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS_TAB))
                  )
            {
               BIND_ACTION_RIGHT(cbs, action_right_mainmenu);
               break;
            }
            BIND_ACTION_RIGHT(cbs, action_right_scroll);
            break;
         case MENU_SETTING_ACTION:
         case FILE_TYPE_CONTENTLIST_ENTRY:
            BIND_ACTION_RIGHT(cbs, action_right_mainmenu);
            break;
         default:
            return -1;
      }
   }

   return 0;
}

static int menu_cbs_init_bind_right_compare_label(menu_file_list_cbs_t *cbs,
      const char *label, uint32_t label_hash, const char *menu_label)
{

   if (cbs->setting)
   {
      const char *parent_group   = cbs->setting->parent_group;

      if (string_is_equal(parent_group, msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU)) 
               && (setting_get_type(cbs->setting) == ST_GROUP))
      {
         BIND_ACTION_RIGHT(cbs, action_right_scroll);
         return 0;
      }
   }

   if (strstr(label, "input_player") && strstr(label, "_joypad_index"))
   {
      unsigned i;
      for (i = 0; i < MAX_USERS; i++)
      {
         uint32_t label_setting_hash;
         char label_setting[128];

         label_setting[0] = '\0';

         snprintf(label_setting, sizeof(label_setting), "input_player%d_joypad_index", i + 1);
         label_setting_hash = msg_hash_calculate(label_setting);

         if (label_hash != label_setting_hash)
            continue;

         BIND_ACTION_RIGHT(cbs, bind_right_generic);
         return 0;
      }
   }

   if (string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB)))
   {
      BIND_ACTION_RIGHT(cbs, action_right_mainmenu);
      return 0;
   }

   if (strstr(label, "rdb_entry"))
   {
      BIND_ACTION_RIGHT(cbs, action_right_scroll);
   }
   else
   {
      if (cbs->enum_idx != MSG_UNKNOWN)
      {
         switch (cbs->enum_idx)
         {
            case MENU_ENUM_LABEL_CONNECT_NETPLAY_ROOM:
               BIND_ACTION_RIGHT(cbs, action_right_mainmenu);
               break;
            case MENU_ENUM_LABEL_VIDEO_SHADER_SCALE_PASS:
               BIND_ACTION_RIGHT(cbs, action_right_shader_scale_pass);
               break;
            case MENU_ENUM_LABEL_VIDEO_SHADER_FILTER_PASS:
               BIND_ACTION_RIGHT(cbs, action_right_shader_filter_pass);
               break;
            case MENU_ENUM_LABEL_VIDEO_SHADER_DEFAULT_FILTER:
               BIND_ACTION_RIGHT(cbs, action_right_shader_filter_default);
               break;
            case MENU_ENUM_LABEL_VIDEO_SHADER_NUM_PASSES:
               BIND_ACTION_RIGHT(cbs, action_right_shader_num_passes);
               break;
            case MENU_ENUM_LABEL_CHEAT_NUM_PASSES:
               BIND_ACTION_RIGHT(cbs, action_right_cheat_num_passes);
               break;
            case MENU_ENUM_LABEL_SCREEN_RESOLUTION:
               BIND_ACTION_RIGHT(cbs, action_right_video_resolution);
               break;
            case MENU_ENUM_LABEL_OPEN_ARCHIVE_DETECT_CORE:
            case MENU_ENUM_LABEL_LOAD_ARCHIVE_DETECT_CORE:
               BIND_ACTION_RIGHT(cbs, action_right_scroll);
               break;
            case MENU_ENUM_LABEL_NO_ITEMS:
            case MENU_ENUM_LABEL_NO_PLAYLIST_ENTRIES_AVAILABLE:
               if (  
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HISTORY_TAB))   ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU))       ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB))   ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_MUSIC_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_IMAGES_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU))
                  )
               {
                  BIND_ACTION_RIGHT(cbs, action_right_mainmenu);
               }
               else
               {
                  BIND_ACTION_RIGHT(cbs, action_right_scroll);
               }
               break;
            case MENU_ENUM_LABEL_START_VIDEO_PROCESSOR:
            case MENU_ENUM_LABEL_TAKE_SCREENSHOT:
               if (  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HISTORY_TAB))   ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_MUSIC_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_IMAGES_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_TAB)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU)) ||
                     string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS_TAB))
                  )
               {
                  BIND_ACTION_RIGHT(cbs, action_right_mainmenu);
                  break;
               }
            default:
               return -1;
         }
      }
      else
      {
         return -1;
      }
   }

   return 0;
}

int menu_cbs_init_bind_right(menu_file_list_cbs_t *cbs,
      const char *path, const char *label, unsigned type, size_t idx,
      const char *menu_label,
      uint32_t label_hash)
{
   if (!cbs)
      return menu_cbs_exit();

   BIND_ACTION_RIGHT(cbs, bind_right_generic);
    
   if (type == MENU_SETTING_NO_ITEM)
   {
      if (  string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HISTORY_TAB))   ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_FAVORITES_TAB)) ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_PLAYLISTS_TAB)) ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_ADD_TAB)) ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_TAB)) ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU)) ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_MUSIC_TAB)) ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_IMAGES_TAB)) ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_TAB)) ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_HORIZONTAL_MENU)) ||
            string_is_equal(menu_label, msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS_TAB))
         )
      {
            BIND_ACTION_RIGHT(cbs, action_right_mainmenu);
            return 0;
      }
   }

   if (menu_cbs_init_bind_right_compare_label(cbs, label, label_hash, menu_label
            ) == 0)
      return 0;

   if (menu_cbs_init_bind_right_compare_type(cbs, type, menu_label ) == 0)
      return 0;

   return menu_cbs_exit();
}
