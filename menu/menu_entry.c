/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2015 - Jay McCarthy
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

#include "menu.h"
#include "menu_display.h"
#include "menu_entry.h"
#include "menu_navigation.h"
#include "menu_setting.h"
#include "menu_input.h"
#include "../settings.h"
#include "../runloop_data.h"
#include "drivers/shared.h"

// This file provides an abstraction of the currently displayed
// menu. It is organized into event-system where the UI companion
// calls this functions and RetroArch responds by changing the global
// state (including arranging for these functions to return different
// values). Its only interaction back to the UI is to arrange for
// notify_list_loaded on the UI companion.

// Returns the starting index of the menu entry list
size_t menu_entries_get_start(void)
{
   menu_handle_t *menu       = menu_driver_get_ptr();
   
   if (!menu)
     return 0;

   return menu->begin;
}

// Returns the last index + 1 of the menu entry list
size_t menu_entries_get_end(void)
{
   menu_list_t *menu_list    = menu_list_get_ptr();
   
   if (!menu_list)
      return 0;

   return menu_list_get_size(menu_list);
}

static void get_title(const char *label, const char *dir,
      unsigned menu_type, char *title, size_t sizeof_title)
{
   char elem0[PATH_MAX_LENGTH], elem1[PATH_MAX_LENGTH];
   char elem0_path[PATH_MAX_LENGTH], elem1_path[PATH_MAX_LENGTH];
   struct string_list *list_label = string_split(label, "|");
   struct string_list *list_path  = string_split(dir, "|");

   *elem0 = *elem1 = *elem0_path = *elem1_path = 0;

   if (list_label)
   {
      if (list_label->size > 0)
      {
         strlcpy(elem0, list_label->elems[0].data, sizeof(elem0));
         if (list_label->size > 1)
            strlcpy(elem1, list_label->elems[1].data, sizeof(elem1));
      }
      string_list_free(list_label);
   }

   if (list_path)
   {
      if (list_path->size > 0)
      {
         strlcpy(elem0_path, list_path->elems[0].data, sizeof(elem0_path));
         if (list_path->size > 1)
            strlcpy(elem1_path, list_path->elems[1].data, sizeof(elem1_path));
      }
      string_list_free(list_path);
   }

#if 0
   RARCH_LOG("label %s, elem0 %s, elem1 %s\n", label, elem0, elem1);
#endif
   if (!strcmp(label, "deferred_database_manager_list"))
      snprintf(title, sizeof_title, "DATABASE SELECTION - %s", (elem0_path[0] != '\0') ? path_basename(elem0_path) : "");
   else if (!strcmp(label, "deferred_cursor_manager_list"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST - %s", (elem0_path[0] != '\0') ? path_basename(elem0_path) : "");
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_developer"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: DEVELOPER - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_publisher"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: PUBLISHER - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_origin"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: ORIGIN - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_franchise"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: FRANCHISE - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_edge_magazine_rating"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: EDGE MAGAZINE RATING - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_edge_magazine_issue"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: EDGE MAGAZINE ISSUE - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_releasemonth"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: RELEASEDATE BY MONTH - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_releaseyear"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: RELEASEDATE BY YEAR - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_esrb_rating"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: ESRB RATING - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_elspa_rating"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: ELSPA RATING - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_pegi_rating"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: PEGI RATING - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_cero_rating"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: CERO RATING - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_bbfc_rating"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: BBFC RATING - %s)", elem0_path);
   else if (!strcmp(label, "deferred_cursor_manager_list_rdb_entry_max_users"))
      snprintf(title, sizeof_title, "DATABASE CURSOR LIST (FILTER: MAX USERS - %s)", elem0_path);
   else if (!strcmp(elem0, "deferred_rdb_entry_detail"))
      snprintf(title, sizeof_title, "DATABASE INFO: %s", elem1);
   else if (!strcmp(label, "deferred_core_list"))
      snprintf(title, sizeof_title, "DETECTED CORES %s", dir);
   else if (!strcmp(label, "configurations"))
      snprintf(title, sizeof_title, "CONFIG %s", dir);
   else if (!strcmp(label, "disk_image_append"))
      snprintf(title, sizeof_title, "DISK APPEND %s", dir);
   else if (menu_entries_common_is_settings_entry(elem0))
   {
      strlcpy(title, string_to_upper(elem0), sizeof_title);
      if (elem1[0] != '\0')
      {
         strlcat(title, " - ", sizeof_title);
         strlcat(title, string_to_upper(elem1), sizeof_title);
      }
   }
   else if (menu_type == MENU_SETTINGS_CUSTOM_BIND ||
         menu_type == MENU_SETTINGS_CUSTOM_BIND_KEYBOARD)
   {
      strlcpy(title, "INPUT SETTINGS", sizeof_title);
      if (elem1[0] != '\0')
      {
         strlcat(title, " - ", sizeof_title);
         strlcat(title, string_to_upper(elem1), sizeof_title);
      }
   }
   else if (
            !strcmp(label, "performance_counters")
         || !strcmp(label, "core_list")
         || !strcmp(label, "management")
         || !strcmp(label, "options")
         || !strcmp(label, "settings")
         || !strcmp(label, "frontend_counters")
         || !strcmp(label, "core_counters")
         || !strcmp(label, "history_list")
         || !strcmp(label, "info_screen")
         || !strcmp(label, "system_information")
         || !strcmp(label, "core_information")
         || !strcmp(label, "video_shader_parameters")
         || !strcmp(label, "video_shader_preset_parameters")
         || !strcmp(label, "disk_options")
         || !strcmp(label, "core_options")
         || !strcmp(label, "shader_options")
         || !strcmp(label, "video_options")
         || !strcmp(label, "core_cheat_options")
         || !strcmp(label, "core_input_remapping_options")
         || !strcmp(label, "database_manager_list")
         || !strcmp(label, "cursor_manager_list")
         || (!strcmp(label, "deferred_core_updater_list"))
         )
   {
      sanitize_to_string(title, label, sizeof_title);
   }
   else if (!strcmp(label, "video_shader_pass"))
      snprintf(title, sizeof_title, "SHADER %s", dir);
   else if (!strcmp(label, "video_shader_preset"))
      snprintf(title, sizeof_title, "SHADER PRESET %s", dir);
   else if (!strcmp(label, "cheat_file_load"))
      snprintf(title, sizeof_title, "CHEAT FILE %s", dir);
   else if (!strcmp(label, "remap_file_load"))
      snprintf(title, sizeof_title, "REMAP FILE %s", dir);
   else if (menu_type == MENU_SETTINGS_CUSTOM_VIEWPORT ||
         !strcmp(label, "custom_viewport_2") ||
         !strcmp(label, "help") ||
         menu_type == MENU_SETTINGS)
      snprintf(title, sizeof_title, "MENU %s", dir);
   else if (!strcmp(label, "input_overlay"))
      snprintf(title, sizeof_title, "OVERLAY %s", dir);
   else if (!strcmp(label, "video_font_path"))
      snprintf(title, sizeof_title, "FONT %s", dir);
   else if (!strcmp(label, "video_filter"))
      snprintf(title, sizeof_title, "FILTER %s", dir);
   else if (!strcmp(label, "audio_dsp_plugin"))
      snprintf(title, sizeof_title, "DSP FILTER %s", dir);
   else if (!strcmp(label, "rgui_browser_directory"))
      snprintf(title, sizeof_title, "BROWSER DIR %s", dir);
   else if (!strcmp(label, "playlist_directory"))
      snprintf(title, sizeof_title, "PLAYLIST DIR %s", dir);
   else if (!strcmp(label, "content_directory"))
      snprintf(title, sizeof_title, "CONTENT DIR %s", dir);
   else if (!strcmp(label, "screenshot_directory"))
      snprintf(title, sizeof_title, "SCREENSHOT DIR %s", dir);
   else if (!strcmp(label, "video_shader_dir"))
      snprintf(title, sizeof_title, "SHADER DIR %s", dir);
   else if (!strcmp(label, "video_filter_dir"))
      snprintf(title, sizeof_title, "FILTER DIR %s", dir);
   else if (!strcmp(label, "audio_filter_dir"))
      snprintf(title, sizeof_title, "DSP FILTER DIR %s", dir);
   else if (!strcmp(label, "savestate_directory"))
      snprintf(title, sizeof_title, "SAVESTATE DIR %s", dir);
   else if (!strcmp(label, "libretro_dir_path"))
      snprintf(title, sizeof_title, "LIBRETRO DIR %s", dir);
   else if (!strcmp(label, "libretro_info_path"))
      snprintf(title, sizeof_title, "LIBRETRO INFO DIR %s", dir);
   else if (!strcmp(label, "rgui_config_directory"))
      snprintf(title, sizeof_title, "CONFIG DIR %s", dir);
   else if (!strcmp(label, "savefile_directory"))
      snprintf(title, sizeof_title, "SAVEFILE DIR %s", dir);
   else if (!strcmp(label, "overlay_directory"))
      snprintf(title, sizeof_title, "OVERLAY DIR %s", dir);
   else if (!strcmp(label, "system_directory"))
      snprintf(title, sizeof_title, "SYSTEM DIR %s", dir);
   else if (!strcmp(label, "assets_directory"))
      snprintf(title, sizeof_title, "ASSETS DIR %s", dir);
   else if (!strcmp(label, "extraction_directory"))
      snprintf(title, sizeof_title, "EXTRACTION DIR %s", dir);
   else if (!strcmp(label, "joypad_autoconfig_dir"))
      snprintf(title, sizeof_title, "AUTOCONFIG DIR %s", dir);
   else
   {
      driver_t *driver = driver_get_ptr();

      if (driver->menu->defer_core)
         snprintf(title, sizeof_title, "CONTENT %s", dir);
      else
      {
         global_t *global      = global_get_ptr();
         const char *core_name = global->menu.info.library_name;

         if (!core_name)
            core_name = global->system.info.library_name;
         if (!core_name)
            core_name = "No Core";
         snprintf(title, sizeof_title, "CONTENT (%s) %s", core_name, dir);
      }
   }
}

// Sets title to what the name of the current menu should be
void menu_entries_get_title(char *title, size_t title_len)
{
   const char *dir           = NULL;
   const char *label         = NULL;
   unsigned menu_type        = 0;
   menu_list_t *menu_list    = menu_list_get_ptr();
   
   if (!menu_list)
      return;

   menu_list_get_last_stack(menu_list, &dir, &label, &menu_type);
   get_title(label, dir, menu_type, title, title_len);
}

// Returns true if a Back button should be shown (i.e. we are at least
// one level deep in the menu hierarchy)
uint32_t menu_entries_show_back(void)
{
   menu_list_t *menu_list    = menu_list_get_ptr();
   
   if (!menu_list)
      return false;

   return (menu_list_get_stack_size(menu_list) > 1);
}

/* Clicks the back button */
int menu_entries_select_back(void)
{
  menu_list_t *menu_list = menu_list_get_ptr();
  if (!menu_list)
    return -1;
  
  menu_setting_apply_deferred();
  menu_list_pop_stack(menu_list);
    
  if (menu_needs_refresh())
      menu_do_refresh(MENU_ACTION_CANCEL);

  rarch_main_data_iterate();
  
  return 0;
}

// Sets title_msg to the name of the current core (shown at the top of the UI)
void menu_entries_get_core_title(char *title_msg, size_t title_msg_len)
{
   global_t *global          = global_get_ptr();
   const char *core_name     = global->menu.info.library_name;
   const char *core_version  = global->menu.info.library_version;

   if (!core_name)
      core_name = global->system.info.library_name;
   if (!core_name)
      core_name = "No Core";

   if (!core_version)
      core_version = global->system.info.library_version;
   if (!core_version)
      core_version = "";

   snprintf(title_msg, title_msg_len, "%s - %s %s", PACKAGE_VERSION,
         core_name, core_version);
}

rarch_setting_t *menu_entry_get_setting(uint32_t i)
{
   rarch_setting_t *setting;
   const char *path = NULL, *entry_label = NULL;
   unsigned type = 0;
   const char *dir           = NULL;
   const char *label         = NULL;
   menu_list_t *menu_list    = menu_list_get_ptr();
   unsigned menu_type        = 0;

   menu_list_get_last_stack(menu_list, &dir, &label, &menu_type);

   menu_list_get_at_offset(menu_list->selection_buf, i, &path,
         &entry_label, &type);

   setting = menu_setting_find(
         menu_list->selection_buf->list[i].label);

   return setting;
}

enum menu_entry_type menu_entry_get_type(uint32_t i)
{
   rarch_setting_t *setting;
   const char *path = NULL, *entry_label = NULL;
   unsigned type = 0;
   const char *dir           = NULL;
   const char *label         = NULL;
   menu_list_t *menu_list    = menu_list_get_ptr();
   unsigned menu_type        = 0;

   menu_list_get_last_stack(menu_list, &dir, &label, &menu_type);

   menu_list_get_at_offset(menu_list->selection_buf, i, &path,
         &entry_label, &type);

   setting = menu_entry_get_setting(i);

   // XXX Really a special kind of ST_ACTION, but this should be
   // changed
   if (setting_is_of_path_type(setting))
      return MENU_ENTRY_PATH;
   else if (setting && setting->type == ST_BOOL )
      return MENU_ENTRY_BOOL;
   else if (setting && setting->type == ST_BIND )
      return MENU_ENTRY_BIND;
   else if (setting_is_of_enum_type(setting))
      return MENU_ENTRY_ENUM;
   else if (setting && setting->type == ST_INT )
      return MENU_ENTRY_INT;
   else if (setting && setting->type == ST_UINT )
      return MENU_ENTRY_UINT;
   else if (setting && setting->type == ST_FLOAT )
      return MENU_ENTRY_FLOAT;
   else if (setting && setting->type == ST_PATH )
      return MENU_ENTRY_PATH;
   else if (setting && setting->type == ST_DIR )
      return MENU_ENTRY_DIR;
   else if (setting && setting->type == ST_STRING )
      return MENU_ENTRY_STRING;
   else if (setting && setting->type == ST_HEX )
      return MENU_ENTRY_HEX;
   else
      return MENU_ENTRY_ACTION;
}

void menu_entry_get_label(uint32_t i, char *label, size_t sizeof_label)
{
   menu_entry_t entry;
   menu_entry_get(&entry, i, NULL, true);

   strlcpy(label, entry.path, sizeof_label);
}

uint32_t menu_entry_get_bool_value(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   return *setting->value.boolean;
}

void menu_entry_set_bool_value(uint32_t i, uint32_t new_val)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   *setting->value.boolean = new_val;
}

struct string_list *menu_entry_enum_values(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   return string_split(setting->values, "|");
}

void menu_entry_enum_set_value_with_string(uint32_t i, const char *s)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   setting_set_with_string_representation(setting, s);
}

int32_t menu_entry_bind_index(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   if (setting->index)
      return setting->index - 1;
   return 0;
}

void menu_entry_bind_key_set(uint32_t i, int32_t value)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   BINDFOR(*setting).key = (enum retro_key)value;
}

void menu_entry_bind_joykey_set(uint32_t i, int32_t value)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   BINDFOR(*setting).joykey = value;
}

void menu_entry_bind_joyaxis_set(uint32_t i, int32_t value)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   BINDFOR(*setting).joyaxis = value;
}

void menu_entry_pathdir_selected(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   if (setting_is_of_path_type(setting))
      setting->action_toggle( setting, MENU_ACTION_RIGHT, false);
}

uint32_t menu_entry_pathdir_allow_empty(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   return setting->flags & SD_FLAG_ALLOW_EMPTY;
}

uint32_t menu_entry_pathdir_for_directory(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   return setting->flags & SD_FLAG_PATH_DIR;
}

void menu_entry_pathdir_get_value(uint32_t i, char *s, size_t len)
{
   menu_entry_t entry;
   menu_entry_get(&entry, i, NULL, true);
   strlcpy(s, entry.value, len);
}

void menu_entry_pathdir_set_value(uint32_t i, const char *s)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   setting_set_with_string_representation(setting, s);
}

void menu_entry_pathdir_extensions(uint32_t i, char *s, size_t len)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   if (setting)
      strlcpy(s, setting->values, len);
}

void menu_entry_reset(uint32_t i)
{
   menu_entry_t entry;
   menu_entry_get(&entry, i, NULL, true);

   menu_entry_action(&entry, i, MENU_ACTION_START);
}

void menu_entry_get_value(uint32_t i, char *s, size_t len)
{
   menu_entry_t entry;
   menu_entry_get(&entry, i, NULL, true);
   strlcpy(s, entry.value, len);
}

void menu_entry_set_value(uint32_t i, const char *s)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   setting_set_with_string_representation(setting, s);
}

uint32_t menu_entry_num_has_range(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   return (setting->flags & SD_FLAG_HAS_RANGE);
}

float menu_entry_num_min(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   return setting->min;
}

float menu_entry_num_max(uint32_t i)
{
   rarch_setting_t *setting = menu_entry_get_setting(i);
   return setting->max;
}

void menu_entry_get(menu_entry_t *entry, size_t i,
      void *userdata, bool use_representation)
{
   const char *label         = NULL;
   const char *path          = NULL;
   const char *entry_label   = NULL;
   menu_file_list_cbs_t *cbs = NULL;
   file_list_t *list         = NULL;
   menu_list_t *menu_list    = menu_list_get_ptr();

   if (!menu_list)
      return;

   menu_list_get_last_stack(menu_list, NULL, &label, NULL);
   
   list = userdata ? (file_list_t*)userdata : menu_list->selection_buf;

   if (!list)
      return;

   menu_list_get_at_offset(list, i, &path, &entry_label, &entry->type);

   cbs = menu_list_get_actiondata_at_offset(list, i);

   if (cbs && cbs->action_get_representation && use_representation)
      cbs->action_get_representation(list,
            &entry->spacing, entry->type, i, label,
            entry->value,  sizeof(entry->value), 
            entry_label, path,
            entry->path, sizeof(entry->path));

   entry->id         = i;

   if (path && !use_representation)
      strlcpy(entry->path,  path,        sizeof(entry->path));
   if (entry_label)
      strlcpy(entry->label, entry_label, sizeof(entry->label));
}

bool menu_entry_is_currently_selected(menu_entry_t *entry)
{
   menu_navigation_t *nav = menu_navigation_get_ptr();
   if (!entry || !nav)
      return false;
   return (entry->id == nav->selection_ptr);
}

int menu_entry_get_current_id(bool use_representation)
{
   size_t i;
   menu_list_t   *menu_list = menu_list_get_ptr();
   size_t               end = menu_list_get_size(menu_list);

   for (i = 0; i < end; i++)
   {
      menu_entry_t entry;
      menu_entry_get(&entry, i, NULL, use_representation);

      if (menu_entry_is_currently_selected(&entry))
         return i;
   }

   return -1;
}

// Performs whatever actions are associated with menu entry 'i'. This
// is the most important function because it does all the work
// associated with clicking on things in the UI. This includes loading
// cores and updating the currently displayed menu
int menu_entry_select(uint32_t i)
{
   menu_entry_t entry;
   menu_navigation_t *nav = menu_navigation_get_ptr();
    
   nav->selection_ptr = i;
   menu_entry_get(&entry, i, NULL, false);

   return menu_entry_action(&entry, i, MENU_ACTION_SELECT);
}

int menu_entry_iterate(unsigned action)
{
   const char *label         = NULL;
   menu_file_list_cbs_t *cbs = NULL;
   menu_list_t *menu_list    = menu_list_get_ptr();

   if (!menu_list)
      return -1;

   if (action != MENU_ACTION_NOOP || menu_needs_refresh() || menu_display_update_pending())
      menu_display_fb_set_dirty();

   cbs = (menu_file_list_cbs_t*)menu_list_get_last_stack_actiondata(menu_list);

   menu_list_get_last_stack(menu_list, NULL, &label, NULL);

   if (cbs && cbs->action_iterate)
      return cbs->action_iterate(label, action);

   return -1;
}

int menu_entry_action(menu_entry_t *entry, unsigned i, enum menu_action action)
{
   int ret                   = 0;
   menu_navigation_t *nav    = menu_navigation_get_ptr();
   menu_handle_t *menu       = menu_driver_get_ptr();
   menu_list_t *menu_list    = menu_list_get_ptr();
   menu_file_list_cbs_t *cbs = menu_list_get_actiondata_at_offset(menu_list->selection_buf, i);

   switch (action)
   {
      case MENU_ACTION_UP:
         if (cbs && cbs->action_up)
            ret = cbs->action_up(entry->type, entry->label);
         break;
      case MENU_ACTION_DOWN:
         if (cbs && cbs->action_down)
            ret = cbs->action_down(entry->type, entry->label);
         break;
      case MENU_ACTION_SCROLL_UP:
         menu_navigation_descend_alphabet(nav, &nav->selection_ptr);
         break;
      case MENU_ACTION_SCROLL_DOWN:
         menu_navigation_ascend_alphabet(nav, &nav->selection_ptr);
         break;

      case MENU_ACTION_CANCEL:
         if (cbs && cbs->action_cancel)
            ret = cbs->action_cancel(entry->path, entry->label, entry->type, i);
         break;

      case MENU_ACTION_OK:
         if (cbs && cbs->action_ok)
            ret = cbs->action_ok(entry->path, entry->label, entry->type, i);
         break;
      case MENU_ACTION_START:
         if (cbs && cbs->action_start)
            ret = cbs->action_start(entry->type, entry->label);
         break;
      case MENU_ACTION_LEFT:
         if (cbs && cbs->action_left)
            ret = cbs->action_left(entry->type, entry->label, false);
         break;
      case MENU_ACTION_RIGHT:
         if (cbs && cbs->action_right)
            ret = cbs->action_right(entry->type, entry->label, false);
         break;
      case MENU_ACTION_INFO:
         if (cbs && cbs->action_info)
            ret = cbs->action_info(entry->type, entry->label);
         break;
      case MENU_ACTION_SELECT:
         if (cbs && cbs->action_select)
            ret = cbs->action_select(entry->path, entry->label, entry->type, i);
         break;

      case MENU_ACTION_REFRESH:
         if (cbs && cbs->action_refresh)
         {
            ret = cbs->action_refresh(menu_list->selection_buf, menu_list->menu_stack);
            menu_unset_refresh();
         }
         break;

      case MENU_ACTION_MESSAGE:
         menu->msg_force = true;
         break;

      case MENU_ACTION_SEARCH:
         menu_input_search_start();
         break;

      case MENU_ACTION_SCAN:
         if (cbs && cbs->action_scan)
            ret = cbs->action_scan(entry->path, entry->label, entry->type, i);
         break;

      default:
         break;
   }

   return ret;
}
