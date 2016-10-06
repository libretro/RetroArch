/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#ifdef _WIN32
#include <direct.h>
#else
#include <unistd.h>
#endif

#include <lists/file_list.h>
#include <file/file_path.h>
#include <string/stdstring.h>
#include <lists/string_list.h>

#include <compat/strl.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#if defined(__CELLOS_LV2__)
#include <sdk_version.h>

#if (CELL_SDK_VERSION > 0x340000)
#include <sysutil/sysutil_bgmplayback.h>
#endif

#endif

#include "../frontend/frontend_driver.h"

#include "widgets/menu_input_bind_dialog.h"

#include "menu_setting.h"
#include "menu_driver.h"
#include "menu_animation.h"
#include "menu_display.h"
#include "menu_input.h"
#include "menu_navigation.h"

#include "../core.h"
#include "../configuration.h"
#include "../msg_hash.h"
#include "../defaults.h"
#include "../driver.h"
#include "../dirs.h"
#include "../paths.h"
#include "../dynamic.h"
#include "../runloop.h"
#include "../verbosity.h"
#include "../camera/camera_driver.h"
#include "../wifi/wifi_driver.h"
#include "../location/location_driver.h"
#include "../record/record_driver.h"
#include "../audio/audio_driver.h"
#include "../audio/audio_resampler_driver.h"
#include "../input/input_config.h"
#include "../input/input_autodetect.h"
#include "../config.def.h"
#include "../ui/ui_companion_driver.h"
#include "../performance_counters.h"
#include "../setting_list.h"
#include "../lakka.h"
#include "../retroarch.h"

#include "../tasks/tasks_internal.h"

#ifdef HAVE_CHEEVOS
static void setting_get_string_representation_cheevos_password(void *data,
      char *s, size_t len)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   if (!setting)
      return;

   if (*setting->value.target.string)
      snprintf(s, len, "%s",
            "********");
   else
      *setting->value.target.string = '\0';
}
#endif

static void setting_get_string_representation_uint_video_monitor_index(void *data,
      char *s, size_t len)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   if (!setting)
      return;

   if (*setting->value.target.unsigned_integer)
      snprintf(s, len, "%u",
            *setting->value.target.unsigned_integer);
   else
      strlcpy(s, "0 (Auto)", len);
}

static int setting_uint_action_left_custom_viewport_width(void *data, bool wraparound)
{
   video_viewport_t vp;
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
   video_viewport_t            *custom  = video_viewport_get_custom();
   settings_t                 *settings = config_get_ptr();
   struct retro_game_geometry     *geom = (struct retro_game_geometry*)
      &av_info->geometry;

   if (!settings || !av_info)
      return -1;

   video_driver_get_viewport_info(&vp);

   if (custom->width <= 1)
      custom->width = 1;
   else if (settings->video.scale_integer)
      custom->width -= geom->base_width;
   else
      custom->width -= 1;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   return 0;
}

static int setting_uint_action_right_custom_viewport_width(void *data, bool wraparound)
{
   video_viewport_t vp;
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
   video_viewport_t            *custom  = video_viewport_get_custom();
   settings_t                 *settings = config_get_ptr();
   struct retro_game_geometry     *geom = (struct retro_game_geometry*)
      &av_info->geometry;

   if (!settings || !av_info)
      return -1;

   video_driver_get_viewport_info(&vp);

   if (settings->video.scale_integer)
      custom->width += geom->base_width;
   else
      custom->width += 1;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   return 0;
}

static int setting_uint_action_left_custom_viewport_height(void *data, bool wraparound)
{
   video_viewport_t vp;
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
   video_viewport_t            *custom  = video_viewport_get_custom();
   settings_t                 *settings = config_get_ptr();
   struct retro_game_geometry     *geom = (struct retro_game_geometry*)
      &av_info->geometry;

   if (!settings || !av_info)
      return -1;

   video_driver_get_viewport_info(&vp);

   if (custom->height <= 1)
      custom->height = 1;
   else if (settings->video.scale_integer)
      custom->height -= geom->base_height;
   else
      custom->height -= 1;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   return 0;
}

static int setting_uint_action_right_custom_viewport_height(void *data, bool wraparound)
{
   video_viewport_t vp;
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
   video_viewport_t            *custom  = video_viewport_get_custom();
   settings_t                 *settings = config_get_ptr();
   struct retro_game_geometry     *geom = (struct retro_game_geometry*)
      &av_info->geometry;

   if (!settings || !av_info)
      return -1;

   video_driver_get_viewport_info(&vp);

   if (settings->video.scale_integer)
      custom->height += geom->base_height;
   else
      custom->height += 1;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   return 0;
}

#if !defined(RARCH_CONSOLE)
static int setting_string_action_left_audio_device(void *data, bool wraparound)
{
   int audio_device_index;
   struct string_list *ptr  = NULL;
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!audio_driver_get_devices_list((void**)&ptr))
      return -1;

   if (!ptr)
      return -1;

   /* Get index in the string list */
   audio_device_index = string_list_find_elem(ptr,setting->value.target.string) - 1;
   audio_device_index--;

   /* Reset index if needed */
   if (audio_device_index < 0)
      audio_device_index = ptr->size - 1;

   strlcpy(setting->value.target.string, ptr->elems[audio_device_index].data, setting->size);

   return 0;
}

static int setting_string_action_right_audio_device(void *data, bool wraparound)
{
   int audio_device_index;
   struct string_list *ptr  = NULL;
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!audio_driver_get_devices_list((void**)&ptr))
      return -1;

   if (!ptr)
      return -1;

   /* Get index in the string list */
   audio_device_index = string_list_find_elem(ptr,setting->value.target.string) -1;
   audio_device_index++;

   /* Reset index if needed */
   if (audio_device_index == (signed)ptr->size)
      audio_device_index = 0;

   strlcpy(setting->value.target.string, ptr->elems[audio_device_index].data, setting->size);

   return 0;
}
#endif

static int setting_string_action_left_driver(void *data,
      bool wraparound)
{
   driver_ctx_info_t drv;
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   drv.label = setting->name;
   drv.s     = setting->value.target.string;
   drv.len   = setting->size;

   if (!driver_ctl(RARCH_DRIVER_CTL_FIND_PREV, &drv))
      return -1;

   if (setting->change_handler)
      setting->change_handler(setting);

   return 0;
}

static int setting_string_action_right_driver(void *data,
      bool wraparound)
{
   driver_ctx_info_t drv;
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   drv.label = setting->name;
   drv.s     = setting->value.target.string;
   drv.len   = setting->size;

   if (!driver_ctl(RARCH_DRIVER_CTL_FIND_NEXT, &drv))
   {
      settings_t *settings = config_get_ptr();

      if (settings && settings->menu.navigation.wraparound.enable)
      {
         drv.label = setting->name;
         drv.s     = setting->value.target.string;
         drv.len   = setting->size;
         driver_ctl(RARCH_DRIVER_CTL_FIND_FIRST, &drv);
      }
   }

   if (setting->change_handler)
      setting->change_handler(setting);

   return 0;
}

static void setting_get_string_representation_uint_video_rotation(void *data,
      char *s, size_t len)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   if (setting)
      strlcpy(s, rotation_lut[*setting->value.target.unsigned_integer],
            len);
}

static void setting_get_string_representation_uint_aspect_ratio_index(void *data,
      char *s, size_t len)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   if (setting)
      strlcpy(s,
            aspectratio_lut[*setting->value.target.unsigned_integer].name,
            len);
}

static void setting_get_string_representation_uint_libretro_device(void *data,
      char *s, size_t len)
{
   unsigned index_offset;
   const struct retro_controller_description *desc = NULL;
   const char *name            = NULL;
   rarch_system_info_t *system = NULL;
   rarch_setting_t *setting    = (rarch_setting_t*)data;
   settings_t      *settings   = config_get_ptr();

   if (!setting)
      return;

   index_offset = setting_get_index_offset(setting);

   if (runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system)
         && system)
   {
      if (index_offset < system->ports.size)
         desc = libretro_find_controller_description(
               &system->ports.data[index_offset],
               settings->input.libretro_device
               [index_offset]);
   }

   if (desc)
      name = desc->desc;

   if (!name)
   {
      /* Find generic name. */

      switch (settings->input.libretro_device[index_offset])
      {
         case RETRO_DEVICE_NONE:
            name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE);
            break;
         case RETRO_DEVICE_JOYPAD:
            name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RETROPAD);
            break;
         case RETRO_DEVICE_ANALOG:
            name = "RetroPad w/ Analog";
            break;
         default:
            name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UNKNOWN);
            break;
      }
   }

   strlcpy(s, name, len);
}

static void setting_get_string_representation_uint_analog_dpad_mode(void *data,
      char *s, size_t len)
{
   const char *modes[3];
   rarch_setting_t *setting  = (rarch_setting_t*)data;
   settings_t      *settings = config_get_ptr();

   modes[0] = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE);
   modes[1] = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LEFT_ANALOG);
   modes[2] = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RIGHT_ANALOG);


   if (setting)
   {
      unsigned index_offset = setting_get_index_offset(setting);
      strlcpy(s, modes[settings->input.analog_dpad_mode
            [index_offset] % ANALOG_DPAD_LAST], len);
   }
}

#ifdef HAVE_THREADS
static void setting_get_string_representation_uint_autosave_interval(void *data,
      char *s, size_t len)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   if (!setting)
      return;

   if (*setting->value.target.unsigned_integer)
      snprintf(s, len, "%u %s",
            *setting->value.target.unsigned_integer, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SECONDS));
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF), len);
}
#endif

#ifdef HAVE_LANGEXTRA
static void setting_get_string_representation_uint_user_language(void *data,
      char *s, size_t len)
{
   const char *modes[RETRO_LANGUAGE_LAST];
   settings_t      *settings = config_get_ptr();

   modes[RETRO_LANGUAGE_ENGLISH]             = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_ENGLISH);
   modes[RETRO_LANGUAGE_JAPANESE]            = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_JAPANESE);
   modes[RETRO_LANGUAGE_FRENCH]              = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_FRENCH);
   modes[RETRO_LANGUAGE_SPANISH]             = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_SPANISH);
   modes[RETRO_LANGUAGE_GERMAN]              = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_GERMAN);
   modes[RETRO_LANGUAGE_ITALIAN]             = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_ITALIAN);
   modes[RETRO_LANGUAGE_DUTCH]               = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_DUTCH);
   modes[RETRO_LANGUAGE_PORTUGUESE]          = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_PORTUGUESE);
   modes[RETRO_LANGUAGE_RUSSIAN]             = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_RUSSIAN);
   modes[RETRO_LANGUAGE_KOREAN]              = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_KOREAN);
   modes[RETRO_LANGUAGE_CHINESE_TRADITIONAL] = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_CHINESE_TRADITIONAL);
   modes[RETRO_LANGUAGE_CHINESE_SIMPLIFIED]  = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_CHINESE_SIMPLIFIED);
   modes[RETRO_LANGUAGE_ESPERANTO]           = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_ESPERANTO);
   modes[RETRO_LANGUAGE_POLISH]              = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LANG_POLISH);

   if (settings)
      strlcpy(s, modes[settings->user_language], len);
}
#endif

static void setting_get_string_representation_uint_libretro_log_level(void *data,
      char *s, size_t len)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (setting)
   {
      static const char *modes[] = {
         "0 (Debug)",
         "1 (Info)",
         "2 (Warning)",
         "3 (Error)"
      };
      strlcpy(s, modes[*setting->value.target.unsigned_integer],
            len);
   }
}

enum setting_type menu_setting_get_browser_selection_type(rarch_setting_t *setting)
{
   if (!setting)
      return ST_NONE;
   return setting->browser_selection_type;
}

static void menu_settings_info_list_free(rarch_setting_info_t *list_info)
{
   if (list_info)
      free(list_info);
}

void menu_settings_list_increment(rarch_setting_t **list)
{
   if (!list || !*list)
      return;

   *list = *list + 1;
}

void menu_settings_list_current_add_range(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      float min, float max, float step,
      bool enforce_minrange_enable, bool enforce_maxrange_enable)
{
   unsigned idx = list_info->index - 1;

   (*list)[idx].min               = min;
   (*list)[idx].step              = step;
   (*list)[idx].max               = max;
   (*list)[idx].enforce_minrange  = enforce_minrange_enable;
   (*list)[idx].enforce_maxrange  = enforce_maxrange_enable;

   (*list)[list_info->index - 1].flags |= SD_FLAG_HAS_RANGE;
}

static void menu_settings_list_current_add_values(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *values)
{
   unsigned idx = list_info->index - 1;
   (*list)[idx].values = values;
}

void menu_settings_list_current_add_cmd(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      enum event_command values)
{
   unsigned idx = list_info->index - 1;
   (*list)[idx].cmd_trigger.idx = values;
}

void menu_settings_list_current_add_enum_idx(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      enum msg_hash_enums enum_idx)
{
   unsigned idx = list_info->index - 1;
   (*list)[idx].enum_idx = enum_idx;
}


int menu_setting_generic(rarch_setting_t *setting, bool wraparound)
{
   uint64_t flags = setting_get_flags(setting);
   if (setting_generic_action_ok_default(setting, wraparound) != 0)
      return -1;

   if (setting->change_handler)
      setting->change_handler(setting);

   if ((flags & SD_FLAG_EXIT) && setting->cmd_trigger.triggered)
   {
      setting->cmd_trigger.triggered = false;
      return -1;
   }

   return 0;
}

static int setting_handler(rarch_setting_t *setting, unsigned action)
{
   if (!setting)
      return -1;

   switch (action)
   {
      case MENU_ACTION_UP:
         if (setting->action_up)
            return setting->action_up(setting);
         break;
      case MENU_ACTION_DOWN:
         if (setting->action_down)
            return setting->action_down(setting);
         break;
      case MENU_ACTION_LEFT:
         if (setting->action_left)
            return setting->action_left(setting, true);
         break;
      case MENU_ACTION_RIGHT:
         if (setting->action_right)
            return setting->action_right(setting, false);
         break;
      case MENU_ACTION_SELECT:
         if (setting->action_select)
            return setting->action_select(setting, true);
         break;
      case MENU_ACTION_OK:
         if (setting->action_ok)
            return setting->action_ok(setting, false);
         break;
      case MENU_ACTION_CANCEL:
         if (setting->action_cancel)
            return setting->action_cancel(setting);
         break;
      case MENU_ACTION_START:
         if (setting->action_start)
            return setting->action_start(setting);
         break;
   }

   return -1;
}

int menu_action_handle_setting(rarch_setting_t *setting,
      unsigned type, unsigned action, bool wraparound)
{
   const char *name;
   menu_displaylist_info_t  info = {0};

   if (!setting)
      return -1;

   name         = menu_setting_get_name(setting);

   switch (setting_get_type(setting))
   {
      case ST_PATH:
         if (action == MENU_ACTION_OK)
         {
            size_t selection;
            file_list_t  *menu_stack = menu_entries_get_menu_stack_ptr(0);

            if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
               return -1;

            info.list           = menu_stack;
            info.directory_ptr  = selection;
            info.type           = type;
            info.enum_idx       = MSG_UNKNOWN;
            strlcpy(info.path,  setting->default_value.string, sizeof(info.path));
            strlcpy(info.label, name, sizeof(info.label));

            if (menu_displaylist_ctl(DISPLAYLIST_GENERIC, &info))
               menu_displaylist_ctl(DISPLAYLIST_PROCESS, &info);
         }
         /* fall-through. */
      case ST_BOOL:
      case ST_INT:
      case ST_UINT:
      case ST_HEX:
      case ST_FLOAT:
      case ST_STRING:
      case ST_STRING_OPTIONS:
      case ST_DIR:
      case ST_BIND:
      case ST_ACTION:
         if (setting_handler(setting, action) == 0)
            return menu_setting_generic(setting, wraparound);
         break;
      default:
         break;
   }

   return -1;
}

const char *menu_setting_get_values(rarch_setting_t *setting)
{
   if (!setting)
      return NULL;
   return setting->values;
}

enum msg_hash_enums menu_setting_get_enum_idx(rarch_setting_t *setting)
{
   if (!setting)
      return MSG_UNKNOWN;
   return setting->enum_idx;
}

const char *menu_setting_get_name(rarch_setting_t *setting)
{
   if (!setting)
      return NULL;
   return setting->name;
}

const char *menu_setting_get_parent_group(rarch_setting_t *setting)
{
   if (!setting)
      return NULL;
   return setting->parent_group;
}

const char *menu_setting_get_short_description(rarch_setting_t *setting)
{
   if (!setting)
      return NULL;
   return setting->short_description;
}

static rarch_setting_t *menu_setting_find_internal(rarch_setting_t *setting, 
      const char *label)
{
   uint32_t needle = msg_hash_calculate(label);

   for (; setting_get_type(setting) != ST_NONE; menu_settings_list_increment(&setting))
   {
      if (needle == setting->name_hash && setting_get_type(setting) <= ST_GROUP)
      {
         const char *name              = menu_setting_get_name(setting);
         const char *short_description = menu_setting_get_short_description(setting);
         /* make sure this isn't a collision */
         if (!string_is_equal(label, name))
            continue;

         if (string_is_empty(short_description))
            return NULL;

         if (setting->read_handler)
            setting->read_handler(setting);

         return setting;
      }
   }

   return NULL;
}

static rarch_setting_t *menu_setting_find_internal_enum(rarch_setting_t *setting, 
     enum msg_hash_enums enum_idx)
{
   for (; setting_get_type(setting) != ST_NONE; menu_settings_list_increment(&setting))
   {
      if (setting->enum_idx == enum_idx && setting_get_type(setting) <= ST_GROUP)
      {
         const char *short_description = menu_setting_get_short_description(setting);
         if (string_is_empty(short_description))
            return NULL;

         if (setting->read_handler)
            setting->read_handler(setting);

         return setting;
      }
   }

   return NULL;
}

/**
 * menu_setting_find:
 * @settings           : pointer to settings
 * @name               : name of setting to search for
 *
 * Search for a setting with a specified name (@name).
 *
 * Returns: pointer to setting if found, NULL otherwise.
 **/
rarch_setting_t *menu_setting_find(const char *label)
{
   rarch_setting_t *setting = NULL;

   menu_entries_ctl(MENU_ENTRIES_CTL_SETTINGS_GET, &setting);

   if (!setting || !label)
      return NULL;

   return menu_setting_find_internal(setting, label);
}

rarch_setting_t *menu_setting_find_enum(enum msg_hash_enums enum_idx)
{
   rarch_setting_t *setting = NULL;

   menu_entries_ctl(MENU_ENTRIES_CTL_SETTINGS_GET, &setting);

   if (!setting || enum_idx == 0)
      return NULL;

   return menu_setting_find_internal_enum(setting, enum_idx);
}

int menu_setting_set_flags(rarch_setting_t *setting)
{
   if (!setting)
      return 0;

   switch (setting_get_type(setting))
   {
      case ST_STRING_OPTIONS:
         return MENU_SETTING_STRING_OPTIONS;
      case ST_ACTION:
         return MENU_SETTING_ACTION;
      case ST_PATH:
         return FILE_TYPE_PATH;
      case ST_GROUP:
         return MENU_SETTING_GROUP;
      case ST_SUB_GROUP:
         return MENU_SETTING_SUBGROUP;
      default:
         break;
   }

   return 0;
}

int menu_setting_set(unsigned type, const char *label,
      unsigned action, bool wraparound)
{
   size_t selection;
   int ret                    = 0;
   menu_file_list_cbs_t *cbs  = NULL;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return 0;

   cbs = menu_entries_get_actiondata_at_offset(selection_buf, selection);

   if (!cbs)
      return 0;

   ret = menu_action_handle_setting(cbs->setting,
         type, action, wraparound);

   if (ret == -1)
      return 0;
   return ret;
}

void *setting_get_ptr(rarch_setting_t *setting)
{
   if (!setting)
      return NULL;

   switch (setting_get_type(setting))
   {
      case ST_BOOL:
         return setting->value.target.boolean;
      case ST_INT:
         return setting->value.target.integer;
      case ST_UINT:
         return setting->value.target.unsigned_integer;
      case ST_FLOAT:
         return setting->value.target.fraction;
      case ST_BIND:
         return setting->value.target.keybind;
      case ST_STRING:
      case ST_STRING_OPTIONS:
      case ST_PATH:
      case ST_DIR:
         return setting->value.target.string;
      default:
         break;
   }

   return NULL;
}

/**
 * setting_get_string_representation:
 * @setting            : pointer to setting
 * @s                  : buffer to write contents of string representation to.
 * @len                : size of the buffer (@s)
 *
 * Get a setting value's string representation.
 **/
void setting_get_string_representation(void *data, char *s, size_t len)
{
   rarch_setting_t* setting = (rarch_setting_t*)data;
   if (!setting || !s)
      return;

   if (setting->get_string_representation)
      setting->get_string_representation(setting, s, len);
}


/**
 * setting_action_start_savestates:
 * @data               : pointer to setting
 *
 * Function callback for 'Savestate' action's 'Action Start'
 * function pointer.
 *
 * Returns: 0 on success, -1 on error.
 **/
static int setting_action_start_bind_device(void *data)
{
   uint32_t index_offset;
   rarch_setting_t *setting  = (rarch_setting_t*)data;
   settings_t      *settings = config_get_ptr();

   if (!setting || !settings)
      return -1;

   index_offset = setting_get_index_offset(setting);

   settings->input.joypad_map[index_offset] = index_offset;
   return 0;
}


static int setting_action_start_custom_viewport_width(void *data)
{
   video_viewport_t vp;
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
   video_viewport_t            *custom  = video_viewport_get_custom();
   settings_t                 *settings = config_get_ptr();
   struct retro_game_geometry     *geom = (struct retro_game_geometry*)
      &av_info->geometry;

   if (!settings || !av_info)
      return -1;

   video_driver_get_viewport_info(&vp);

   if (settings->video.scale_integer)
      custom->width = ((custom->width + geom->base_width - 1) /
            geom->base_width) * geom->base_width;
   else
      custom->width = vp.full_width - custom->x;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   return 0;
}

static int setting_action_start_custom_viewport_height(void *data)
{
   video_viewport_t vp;
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
   video_viewport_t            *custom  = video_viewport_get_custom();
   settings_t                 *settings = config_get_ptr();
   struct retro_game_geometry     *geom = (struct retro_game_geometry*)
      &av_info->geometry;

   if (!settings || !av_info)
      return -1;

   video_driver_get_viewport_info(&vp);

   if (settings->video.scale_integer)
      custom->height = ((custom->height + geom->base_height - 1) /
            geom->base_height) * geom->base_height;
   else
      custom->height = vp.full_height - custom->y;

   aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
      (float)custom->width / custom->height;

   return 0;
}



static int setting_action_start_analog_dpad_mode(void *data)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   *setting->value.target.unsigned_integer = 0;

   return 0;
}

static int setting_action_start_libretro_device_type(void *data)
{
   retro_ctx_controller_info_t pad;
   unsigned index_offset, current_device;
   unsigned devices[128], types = 0, port = 0;
   const struct retro_controller_info *desc = NULL;
   rarch_system_info_t *system = NULL;
   settings_t        *settings = config_get_ptr();
   rarch_setting_t   *setting  = (rarch_setting_t*)data;

   if (setting_generic_action_start_default(setting) != 0)
      return -1;

   index_offset = setting_get_index_offset(setting);
   port         = index_offset;

   devices[types++] = RETRO_DEVICE_NONE;
   devices[types++] = RETRO_DEVICE_JOYPAD;

   if (runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system) 
         && system)
   {
      /* Only push RETRO_DEVICE_ANALOG as default if we use an 
       * older core which doesn't use SET_CONTROLLER_INFO. */
      if (!system->ports.size)
         devices[types++] = RETRO_DEVICE_ANALOG;

      if (port < system->ports.size)
         desc = &system->ports.data[port];
   }

   if (desc)
   {
      unsigned i;

      for (i = 0; i < desc->num_types; i++)
      {
         unsigned id = desc->types[i].id;
         if (types < ARRAY_SIZE(devices) &&
               id != RETRO_DEVICE_NONE &&
               id != RETRO_DEVICE_JOYPAD)
            devices[types++] = id;
      }
   }

   current_device = RETRO_DEVICE_JOYPAD;

   settings->input.libretro_device[port] = current_device;

   pad.port   = port;
   pad.device = current_device;
   core_set_controller_port_device(&pad);

   return 0;
}

static int setting_action_start_video_refresh_rate_auto(
      void *data)
{
   video_driver_monitor_reset();
   return 0;
}

/**
 ******* ACTION TOGGLE CALLBACK FUNCTIONS *******
**/

static int setting_action_left_analog_dpad_mode(void *data, bool wraparound)
{
   unsigned port = 0;
   rarch_setting_t *setting  = (rarch_setting_t*)data;
   settings_t      *settings = config_get_ptr();

   if (!setting)
      return -1;

   port = setting->index_offset;

   settings->input.analog_dpad_mode[port] =
      (settings->input.analog_dpad_mode
       [port] + ANALOG_DPAD_LAST - 1) % ANALOG_DPAD_LAST;

   return 0;
}

static int setting_action_right_analog_dpad_mode(void *data, bool wraparound)
{
   unsigned port = 0;
   rarch_setting_t *setting  = (rarch_setting_t*)data;
   settings_t      *settings = config_get_ptr();

   if (!setting)
      return -1;

   port = setting->index_offset;

   settings->input.analog_dpad_mode[port] =
      (settings->input.analog_dpad_mode[port] + 1)
      % ANALOG_DPAD_LAST;

   return 0;
}

static int setting_action_left_libretro_device_type(
      void *data, bool wraparound)
{
   retro_ctx_controller_info_t pad;
   unsigned current_device, current_idx, i, devices[128],
            types = 0, port = 0;
   const struct retro_controller_info *desc = NULL;
   rarch_setting_t *setting    = (rarch_setting_t*)data;
   settings_t      *settings   = config_get_ptr();
   rarch_system_info_t *system = NULL;

   if (!setting)
      return -1;

   port = setting->index_offset;

   devices[types++] = RETRO_DEVICE_NONE;
   devices[types++] = RETRO_DEVICE_JOYPAD;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);

   if (system)
   {
      /* Only push RETRO_DEVICE_ANALOG as default if we use an 
       * older core which doesn't use SET_CONTROLLER_INFO. */
      if (!system->ports.size)
         devices[types++] = RETRO_DEVICE_ANALOG;

      if (port < system->ports.size)
         desc = &system->ports.data[port];
   }

   if (desc)
   {
      for (i = 0; i < desc->num_types; i++)
      {
         unsigned id = desc->types[i].id;
         if (types < ARRAY_SIZE(devices) &&
               id != RETRO_DEVICE_NONE &&
               id != RETRO_DEVICE_JOYPAD)
            devices[types++] = id;
      }
   }

   current_device = settings->input.libretro_device[port];
   current_idx    = 0;
   for (i = 0; i < types; i++)
   {
      if (current_device != devices[i])
         continue;

      current_idx = i;
      break;
   }

   current_device = devices
      [(current_idx + types - 1) % types];

   settings->input.libretro_device[port] = current_device;

   pad.port   = port;
   pad.device = current_device;

   core_set_controller_port_device(&pad);

   return 0;
}

static int setting_action_right_libretro_device_type(
      void *data, bool wraparound)
{
   retro_ctx_controller_info_t pad;
   unsigned current_device, current_idx, i, devices[128],
            types = 0, port = 0;
   const struct retro_controller_info *desc = NULL;
   rarch_setting_t *setting    = (rarch_setting_t*)data;
   settings_t      *settings   = config_get_ptr();
   rarch_system_info_t *system = NULL;

   if (!setting)
      return -1;

   port = setting->index_offset;

   devices[types++] = RETRO_DEVICE_NONE;
   devices[types++] = RETRO_DEVICE_JOYPAD;

   if (runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system) 
         && system)
   {
      /* Only push RETRO_DEVICE_ANALOG as default if we use an 
       * older core which doesn't use SET_CONTROLLER_INFO. */
      if (!system->ports.size)
         devices[types++] = RETRO_DEVICE_ANALOG;

      if (port < system->ports.size)
         desc = &system->ports.data[port];
   }

   if (desc)
   {
      for (i = 0; i < desc->num_types; i++)
      {
         unsigned id = desc->types[i].id;
         if (types < ARRAY_SIZE(devices) &&
               id != RETRO_DEVICE_NONE &&
               id != RETRO_DEVICE_JOYPAD)
            devices[types++] = id;
      }
   }

   current_device = settings->input.libretro_device[port];
   current_idx    = 0;
   for (i = 0; i < types; i++)
   {
      if (current_device != devices[i])
         continue;

      current_idx = i;
      break;
   }

   current_device = devices
      [(current_idx + 1) % types];

   settings->input.libretro_device[port] = current_device;

   pad.port   = port;
   pad.device = current_device;

   core_set_controller_port_device(&pad);

   return 0;
}

static int setting_action_left_bind_device(void *data, bool wraparound)
{
   unsigned index_offset;
   unsigned               *p = NULL;
   rarch_setting_t *setting  = (rarch_setting_t*)data;
   settings_t      *settings = config_get_ptr();

   if (!setting)
      return -1;

   index_offset = setting_get_index_offset(setting);

   p = &settings->input.joypad_map[index_offset];

   if ((*p) >= settings->input.max_users)
      *p = settings->input.max_users - 1;
   else if ((*p) > 0)
      (*p)--;

   return 0;
}

static int setting_action_right_bind_device(void *data, bool wraparound)
{
   unsigned index_offset;
   unsigned               *p = NULL;
   rarch_setting_t *setting  = (rarch_setting_t*)data;
   settings_t      *settings = config_get_ptr();

   if (!setting)
      return -1;

   index_offset = setting_get_index_offset(setting);

   p = &settings->input.joypad_map[index_offset];

   if (*p < settings->input.max_users)
      (*p)++;

   return 0;
}



/**
 ******* ACTION OK CALLBACK FUNCTIONS *******
**/

static int setting_action_ok_bind_all(void *data, bool wraparound)
{
   (void)wraparound;
   if (!menu_input_key_bind_set_mode(MENU_INPUT_BINDS_CTL_BIND_ALL, data))
      return -1;
   return 0;
}

static int setting_action_ok_bind_all_save_autoconfig(void *data, bool wraparound)
{
   unsigned index_offset;
   settings_t    *settings   = config_get_ptr();
   rarch_setting_t *setting  = (rarch_setting_t*)data;

   (void)wraparound;

   if (!settings || !setting)
      return -1;

   index_offset = setting_get_index_offset(setting);

   if(config_save_autoconf_profile(
            settings->input.device_names[index_offset], index_offset))
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_AUTOCONFIG_FILE_SAVED_SUCCESSFULLY), 1, 100, true);
   else
      runloop_msg_queue_push(
            msg_hash_to_str(MSG_AUTOCONFIG_FILE_ERROR_SAVING), 1, 100, true);

   return 0;
}

static int setting_action_ok_bind_defaults(void *data, bool wraparound)
{
   unsigned i;
   menu_input_ctx_bind_limits_t lim;
   struct retro_keybind *target          = NULL;
   const struct retro_keybind *def_binds = NULL;
   rarch_setting_t *setting              = (rarch_setting_t*)data;
   settings_t    *settings               = config_get_ptr();

   (void)wraparound;

   if (!setting)
      return -1;

   target    = (struct retro_keybind*)
      &settings->input.binds[setting->index_offset][0];
   def_binds =  (setting->index_offset) ? 
      retro_keybinds_rest : retro_keybinds_1;

   if (!target)
      return -1;

   lim.min = MENU_SETTINGS_BIND_BEGIN;
   lim.max = MENU_SETTINGS_BIND_LAST;

   menu_input_key_bind_set_min_max(&lim);

   for (i = MENU_SETTINGS_BIND_BEGIN;
         i <= MENU_SETTINGS_BIND_LAST; i++, target++)
   {
      target->key     = def_binds[i - MENU_SETTINGS_BIND_BEGIN].key;
      target->joykey  = NO_BTN;
      target->joyaxis = AXIS_NONE;
   }

   return 0;
}

static void setting_get_string_representation_st_float_video_refresh_rate_auto(void *data,
      char *s, size_t len)
{
   double video_refresh_rate = 0.0;
   double deviation          = 0.0;
   unsigned sample_points    = 0;
   rarch_setting_t *setting  = (rarch_setting_t*)data;
   if (!setting)
      return;

   if (video_monitor_fps_statistics(&video_refresh_rate, &deviation, &sample_points))
   {
      snprintf(s, len, "%.3f Hz (%.1f%% dev, %u samples)",
            video_refresh_rate, 100.0 * deviation, sample_points);
      menu_animation_ctl(MENU_ANIMATION_CTL_SET_ACTIVE, NULL);
   }
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE), len);
}

static int setting_action_ok_video_refresh_rate_auto(void *data, bool wraparound)
{
   double video_refresh_rate = 0.0;
   double deviation          = 0.0;
   unsigned sample_points    = 0;
   rarch_setting_t *setting  = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   if (video_monitor_fps_statistics(&video_refresh_rate,
            &deviation, &sample_points))
   {
      float video_refresh_rate_float = (float)video_refresh_rate;
      driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE, &video_refresh_rate_float);
      /* Incase refresh rate update forced non-block video. */
      command_event(CMD_EVENT_VIDEO_SET_BLOCKING_STATE, NULL);
   }

   if (setting_generic_action_ok_default(setting, wraparound) != 0)
      return -1;

   return 0;
}

static void get_string_representation_bind_device(void * data, char *s,
      size_t len)
{
   unsigned index_offset, map = 0;
   rarch_setting_t *setting  = (rarch_setting_t*)data;
   settings_t      *settings = config_get_ptr();

   if (!setting)
      return;

   index_offset = setting_get_index_offset(setting);
   map          = settings->input.joypad_map[index_offset];

   if (map < settings->input.max_users)
   {
      const char *device_name = settings->input.device_names[map];

      if (*device_name)
         snprintf(s, len,
               "%s (#%u)",
               device_name,
               settings->input.device_name_index[map]);
      else
         snprintf(s, len,
               "%s (%s #%u)",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PORT),
               map);
   }
   else
      strlcpy(s, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DISABLED), len);
}


/**
 * menu_setting_get_label:
 * @list               : File list on which to perform the search
 * @s                  : String for the type to be represented on-screen as
 *                       a label.
 * @len                : Size of @s
 * @w                  : Width of the string (for text label representation
 *                       purposes in the menu display driver).
 * @type               : Identifier of setting.
 * @menu_label         : Menu Label identifier of setting.
 * @label              : Label identifier of setting.
 * @idx                : Index identifier of setting.
 *
 * Get associated label of a setting.
 **/
void menu_setting_get_label(void *data, char *s,
      size_t len, unsigned *w, unsigned type, 
      const char *menu_label, const char *label, unsigned idx)
{
   rarch_setting_t *setting = NULL;
   file_list_t *list        = (file_list_t*)data;
   if (!list || !label)
      return;

   setting = menu_setting_find(list->list[idx].label);

   if (setting)
      setting_get_string_representation(setting, s, len);
}

void general_read_handler(void *data)
{
   rarch_setting_t *setting  = (rarch_setting_t*)data;
   settings_t      *settings = config_get_ptr();

   if (!setting)
      return;

   if (setting->enum_idx != MSG_UNKNOWN)
   {
      switch (setting->enum_idx)
      {
         case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
            *setting->value.target.fraction = settings->audio.rate_control_delta;
            if (*setting->value.target.fraction < 0.0005)
            {
               settings->audio.rate_control = false;
               settings->audio.rate_control_delta = 0.0;
            }
            else
            {
               settings->audio.rate_control = true;
               settings->audio.rate_control_delta = *setting->value.target.fraction;
            }
            break;
         case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
            *setting->value.target.fraction = settings->audio.max_timing_skew;
            break;
         case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
            *setting->value.target.fraction = settings->video.refresh_rate;
            break;
         case MENU_ENUM_LABEL_INPUT_PLAYER1_JOYPAD_INDEX:
            *setting->value.target.integer = settings->input.joypad_map[0];
            break;
         case MENU_ENUM_LABEL_INPUT_PLAYER2_JOYPAD_INDEX:
            *setting->value.target.integer = settings->input.joypad_map[1];
            break;
         case MENU_ENUM_LABEL_INPUT_PLAYER3_JOYPAD_INDEX:
            *setting->value.target.integer = settings->input.joypad_map[2];
            break;
         case MENU_ENUM_LABEL_INPUT_PLAYER4_JOYPAD_INDEX:
            *setting->value.target.integer = settings->input.joypad_map[3];
            break;
         case MENU_ENUM_LABEL_INPUT_PLAYER5_JOYPAD_INDEX:
            *setting->value.target.integer = settings->input.joypad_map[4];
            break;
         default:
            break;
      }
   }
}

void general_write_handler(void *data)
{
   enum event_command rarch_cmd = CMD_EVENT_NONE;
   menu_displaylist_info_t info = {0};
   rarch_setting_t *setting     = (rarch_setting_t*)data;
   settings_t *settings         = config_get_ptr();
   global_t *global             = global_get_ptr();
   file_list_t *menu_stack      = menu_entries_get_menu_stack_ptr(0);

   if (!setting)
      return;

   if (setting->cmd_trigger.idx != CMD_EVENT_NONE)
   {
      uint64_t flags = setting_get_flags(setting);

      if (flags & SD_FLAG_EXIT)
      {
         if (*setting->value.target.boolean)
            *setting->value.target.boolean = false;
      }
      if (setting->cmd_trigger.triggered ||
            (flags & SD_FLAG_CMD_APPLY_AUTO))
         rarch_cmd = setting->cmd_trigger.idx;
   }

   switch (setting->enum_idx)
   {
      case MENU_ENUM_LABEL_VIDEO_THREADED:
         {
            if (*setting->value.target.boolean)
               task_queue_ctl(TASK_QUEUE_CTL_SET_THREADED, NULL);
            else
               task_queue_ctl(TASK_QUEUE_CTL_UNSET_THREADED, NULL);
         }
         break;
      case MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR:
         core_set_poll_type((unsigned int*)setting->value.target.integer);
         break;
      case MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER:
         {
            video_viewport_t vp;
            struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
            video_viewport_t            *custom  = video_viewport_get_custom();
            struct retro_game_geometry     *geom = (struct retro_game_geometry*)
               &av_info->geometry;

            video_driver_get_viewport_info(&vp);

            if (*setting->value.target.boolean)
            {
               custom->x      = 0;
               custom->y      = 0;
               custom->width  = ((custom->width + geom->base_width   - 1) / geom->base_width)  * geom->base_width;
               custom->height = ((custom->height + geom->base_height - 1) / geom->base_height) * geom->base_height;
               aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
                  (float)custom->width / custom->height;
            }
         }
         break;
      case MENU_ENUM_LABEL_HELP:
         if (*setting->value.target.boolean)
         {
            info.list          = menu_stack;
            info.type          = 0; 
            info.directory_ptr = 0;
            strlcpy(info.label,
                  msg_hash_to_str(MENU_ENUM_LABEL_HELP), sizeof(info.label));
            info.enum_idx      = MENU_ENUM_LABEL_HELP;

            if (menu_displaylist_ctl(DISPLAYLIST_GENERIC, &info))
               menu_displaylist_ctl(DISPLAYLIST_PROCESS, &info);
            setting_set_with_string_representation(setting, "false");
         }
         break;
      case MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW:
         settings->audio.max_timing_skew = *setting->value.target.fraction;
         break;
      case MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA:
         if (*setting->value.target.fraction < 0.0005)
         {
            settings->audio.rate_control = false;
            settings->audio.rate_control_delta = 0.0;
         }
         else
         {
            settings->audio.rate_control = true;
            settings->audio.rate_control_delta = *setting->value.target.fraction;
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO:
         driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE, setting->value.target.fraction);

         /* In case refresh rate update forced non-block video. */
         rarch_cmd = CMD_EVENT_VIDEO_SET_BLOCKING_STATE;
         break;
      case MENU_ENUM_LABEL_VIDEO_SCALE:
         settings->video.scale = roundf(*setting->value.target.fraction);

         if (!settings->video.fullscreen)
            rarch_cmd = CMD_EVENT_REINIT;
         break;
      case MENU_ENUM_LABEL_INPUT_PLAYER1_JOYPAD_INDEX:
         settings->input.joypad_map[0] = *setting->value.target.integer;
         break;
      case MENU_ENUM_LABEL_INPUT_PLAYER2_JOYPAD_INDEX:
         settings->input.joypad_map[1] = *setting->value.target.integer;
         break;
      case MENU_ENUM_LABEL_INPUT_PLAYER3_JOYPAD_INDEX:
         settings->input.joypad_map[2] = *setting->value.target.integer;
         break;
      case MENU_ENUM_LABEL_INPUT_PLAYER4_JOYPAD_INDEX:
         settings->input.joypad_map[3] = *setting->value.target.integer;
         break;
      case MENU_ENUM_LABEL_INPUT_PLAYER5_JOYPAD_INDEX:
         settings->input.joypad_map[4] = *setting->value.target.integer;
         break;
      case MENU_ENUM_LABEL_LOG_VERBOSITY:
         {
            if (setting 
                  && setting->value.target.boolean 
                  && *setting->value.target.boolean)
               verbosity_enable();
            else
               verbosity_disable();

            if (setting 
                  && setting->value.target.boolean
                  && *setting->value.target.boolean)
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_VERBOSITY, NULL);
            else
               retroarch_override_setting_unset(RARCH_OVERRIDE_SETTING_VERBOSITY, NULL);
         }
         break;
      case MENU_ENUM_LABEL_VIDEO_SMOOTH:
         video_driver_set_filtering(1, settings->video.smooth);
         break;
      case MENU_ENUM_LABEL_VIDEO_ROTATION:
         {
            rarch_system_info_t *system  = NULL;
            runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);
            if (system)
               video_driver_set_rotation(
                     (*setting->value.target.unsigned_integer +
                      system->rotation) % 4);
         }
         break;
      case MENU_ENUM_LABEL_AUDIO_VOLUME:
         audio_driver_set_volume_gain(db_to_gain(*setting->value.target.fraction));
         break;
      case MENU_ENUM_LABEL_AUDIO_LATENCY:
         rarch_cmd = CMD_EVENT_AUDIO_REINIT;
         break;
      case MENU_ENUM_LABEL_PAL60_ENABLE:
         if (*setting->value.target.boolean && global->console.screen.pal_enable)
            rarch_cmd = CMD_EVENT_REINIT;
         else
            setting_set_with_string_representation(setting, "false");
         break;
      case MENU_ENUM_LABEL_SYSTEM_BGM_ENABLE:
         if (*setting->value.target.boolean)
         {
#if defined(__CELLOS_LV2__) && (CELL_SDK_VERSION > 0x340000)
            cellSysutilEnableBgmPlayback();
#endif         
         }
         else
         {
#if defined(__CELLOS_LV2__) && (CELL_SDK_VERSION > 0x340000)
            cellSysutilDisableBgmPlayback();
#endif
         }
         break;
      case MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS:
#ifdef HAVE_NETWORKING
         {
            bool val = (!string_is_empty(setting->value.target.string));
            if (val)
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS, NULL);
            else
               retroarch_override_setting_unset(RARCH_OVERRIDE_SETTING_NETPLAY_IP_ADDRESS, NULL);
         }
#endif
         break;
      case MENU_ENUM_LABEL_NETPLAY_MODE:
#ifdef HAVE_NETWORKING
         retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_NETPLAY_MODE, NULL);
#endif
         break;
      case MENU_ENUM_LABEL_NETPLAY_SPECTATOR_MODE_ENABLE:
#ifdef HAVE_NETWORKING
         retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_NETPLAY_MODE, NULL);
#endif
         break;
      case MENU_ENUM_LABEL_NETPLAY_DELAY_FRAMES:
#ifdef HAVE_NETWORKING
         {
            bool val = (settings->netplay.sync_frames > 0);
            
            if (val)
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_NETPLAY_DELAY_FRAMES, NULL);
            else
               retroarch_override_setting_unset(RARCH_OVERRIDE_SETTING_NETPLAY_DELAY_FRAMES, NULL);
         }
#endif
         break;
      case MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES:
#ifdef HAVE_NETWORKING
         {
            bool val = (settings->netplay.check_frames > 0);

            if (val)
               retroarch_override_setting_set(RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES, NULL);
            else
               retroarch_override_setting_unset(RARCH_OVERRIDE_SETTING_NETPLAY_CHECK_FRAMES, NULL);
         }
#endif
      default:
         break;
   }

   if (rarch_cmd || setting->cmd_trigger.triggered)
      command_event(rarch_cmd, NULL);
}

#ifdef HAVE_OVERLAY
static void overlay_enable_toggle_change_handler(void *data)
{
   settings_t *settings  = config_get_ptr();
   rarch_setting_t *setting = (rarch_setting_t *)data;

   if (!setting)
      return;

   if (settings && settings->input.overlay_hide_in_menu)
   {
      command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);
      return;
   }

   if (setting->value.target.boolean)
      command_event(CMD_EVENT_OVERLAY_INIT, NULL);
   else
      command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);
}
#endif

#ifdef HAVE_LAKKA
static void systemd_service_toggle(const char *path, char *unit, bool enable)
{
   int pid = fork();
   char* args[] = {(char*)"systemctl", NULL, NULL, NULL};

   if (enable)
      args[1] = (char*)"start";
   else
      args[1] = (char*)"stop";
   args[2] = unit;

   if (enable)
      fclose(fopen(path, "w"));
   else
      remove(path);

   if (pid == 0)
      execvp(args[0], args);

   return;
}

static void ssh_enable_toggle_change_handler(void *data)
{
   bool enable           = false;
   settings_t *settings  = config_get_ptr();
   if (settings && settings->ssh_enable)
      enable = true;

   systemd_service_toggle(LAKKA_SSH_PATH, (char*)"sshd.service",
         enable);
}

static void samba_enable_toggle_change_handler(void *data)
{
   bool enable           = false;
   settings_t *settings  = config_get_ptr();
   if (settings && settings->samba_enable)
      enable = true;

   systemd_service_toggle(LAKKA_SAMBA_PATH, (char*)"smbd.service",
         enable);
}

static void bluetooth_enable_toggle_change_handler(void *data)
{
   bool enable           = false;
   settings_t *settings  = config_get_ptr();
   if (settings && settings->bluetooth_enable)
      enable = true;

   systemd_service_toggle(LAKKA_BLUETOOTH_PATH, (char*)"bluetooth.service",
         enable);
}
#endif

enum settings_list_type
{
   SETTINGS_LIST_NONE = 0,
   SETTINGS_LIST_MAIN_MENU,
   SETTINGS_LIST_DRIVERS,
   SETTINGS_LIST_CORE,
   SETTINGS_LIST_CONFIGURATION,
   SETTINGS_LIST_LOGGING,
   SETTINGS_LIST_SAVING,
   SETTINGS_LIST_REWIND,
   SETTINGS_LIST_VIDEO,
   SETTINGS_LIST_AUDIO,
   SETTINGS_LIST_INPUT,
   SETTINGS_LIST_INPUT_HOTKEY,
   SETTINGS_LIST_RECORDING,
   SETTINGS_LIST_FRAME_THROTTLING,
   SETTINGS_LIST_FONT,
   SETTINGS_LIST_OVERLAY,
   SETTINGS_LIST_MENU,
   SETTINGS_LIST_MENU_FILE_BROWSER,
   SETTINGS_LIST_MULTIMEDIA,
   SETTINGS_LIST_USER_INTERFACE,
   SETTINGS_LIST_PLAYLIST,
   SETTINGS_LIST_CHEEVOS,
   SETTINGS_LIST_CORE_UPDATER,
   SETTINGS_LIST_NETPLAY,
   SETTINGS_LIST_LAKKA_SERVICES,
   SETTINGS_LIST_USER,
   SETTINGS_LIST_USER_ACCOUNTS,
   SETTINGS_LIST_USER_ACCOUNTS_CHEEVOS,
   SETTINGS_LIST_DIRECTORY,
   SETTINGS_LIST_PRIVACY
};

static bool setting_append_list_input_player_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *parent_group,
      unsigned user)
{
   /* This constants matches the string length.
    * Keep it up to date or you'll get some really obvious bugs.
    * 2 is the length of '99'; we don't need more users than that.
    */
   static char buffer[MAX_USERS][13+2+1];
   static char group_lbl[MAX_USERS][PATH_MAX_LENGTH];
   unsigned i;
   rarch_setting_group_info_t group_info    = {0};
   rarch_setting_group_info_t subgroup_info = {0};
   settings_t *settings = config_get_ptr();
   const char *temp_value                     = NULL;
   const struct retro_keybind* const defaults =
      (user == 0) ? retro_keybinds_1 : retro_keybinds_rest;
   rarch_system_info_t *system = NULL;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);

   temp_value =msg_hash_to_str((enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_USER_1_BINDS + user));

   snprintf(buffer[user],    sizeof(buffer[user]),
         "%s %u", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER), user + 1);

   strlcpy(group_lbl[user], temp_value, sizeof(group_lbl[user]));

   START_GROUP(list, list_info, &group_info, group_lbl[user], parent_group);

   parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

   START_SUB_GROUP(
         list,
         list_info,
         buffer[user],
         &group_info,
         &subgroup_info,
         parent_group);

   {
      char tmp_string[PATH_MAX_LENGTH] = {0};
      /* These constants match the string lengths.
       * Keep them up to date or you'll get some really obvious bugs.
       * 2 is the length of '99'; we don't need more users than that.
       */
      /* FIXME/TODO - really need to clean up this mess in some way. */
      static char key[MAX_USERS][64];
      static char key_type[MAX_USERS][64];
      static char key_analog[MAX_USERS][64];
      static char key_bind_all[MAX_USERS][64];
      static char key_bind_all_save_autoconfig[MAX_USERS][64];
      static char key_bind_defaults[MAX_USERS][64];

      static char label[MAX_USERS][64];
      static char label_type[MAX_USERS][64];
      static char label_analog[MAX_USERS][64];
      static char label_bind_all[MAX_USERS][64];
      static char label_bind_all_save_autoconfig[MAX_USERS][64];
      static char label_bind_defaults[MAX_USERS][64];

      snprintf(tmp_string, sizeof(tmp_string), "input_player%u", user + 1);

      fill_pathname_join_delim(key[user], tmp_string, "joypad_index", '_',
            sizeof(key[user]));
      snprintf(key_type[user], sizeof(key_type[user]),
               msg_hash_to_str(MENU_ENUM_LABEL_INPUT_LIBRETRO_DEVICE),
               user + 1);
      snprintf(key_analog[user], sizeof(key_analog[user]),
               msg_hash_to_str(MENU_ENUM_LABEL_INPUT_PLAYER_ANALOG_DPAD_MODE),
               user + 1);
      fill_pathname_join_delim(key_bind_all[user], tmp_string, "bind_all", '_',
            sizeof(key_bind_all[user]));
      fill_pathname_join_delim(key_bind_all_save_autoconfig[user],
            tmp_string, "bind_all_save_autoconfig", '_',
            sizeof(key_bind_all_save_autoconfig[user]));
      fill_pathname_join_delim(key_bind_defaults[user],
            tmp_string, "bind_defaults", '_',
            sizeof(key_bind_defaults[user]));

      snprintf(label[user], sizeof(label[user]),
               "%s %u Device Index", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER), user + 1);
      snprintf(label_type[user], sizeof(label_type[user]),
               "%s %u Device Type", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER), user + 1);
      snprintf(label_analog[user], sizeof(label_analog[user]),
               "%s %u Analog To Digital Type", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER), user + 1);
      snprintf(label_bind_all[user], sizeof(label_bind_all[user]),
               "%s %u Bind All", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER), user + 1);
      snprintf(label_bind_defaults[user], sizeof(label_bind_defaults[user]),
               "%s %u Bind Default All", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER), user + 1);
      snprintf(label_bind_all_save_autoconfig[user], sizeof(label_bind_all_save_autoconfig[user]),
               "%s %u Save Autoconfig", msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER), user + 1);

      CONFIG_UINT(
            list, list_info,
            &settings->input.libretro_device[user],
            key_type[user],
            label_type[user],
            user,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index = user + 1;
      (*list)[list_info->index - 1].index_offset = user;
      (*list)[list_info->index - 1].action_left   = &setting_action_left_libretro_device_type;
      (*list)[list_info->index - 1].action_right  = &setting_action_right_libretro_device_type;
      (*list)[list_info->index - 1].action_select = &setting_action_right_libretro_device_type;
      (*list)[list_info->index - 1].action_start  = &setting_action_start_libretro_device_type;
      (*list)[list_info->index - 1].get_string_representation = 
         &setting_get_string_representation_uint_libretro_device;
      menu_settings_list_current_add_enum_idx(list, list_info, (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_LIBRETRO_DEVICE + user));

      CONFIG_UINT(
            list, list_info,
            &settings->input.analog_dpad_mode[user],
            key_analog[user],
            label_analog[user],
            user,
            &group_info,
            &subgroup_info,
            parent_group,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index = user + 1;
      (*list)[list_info->index - 1].index_offset = user;
      (*list)[list_info->index - 1].action_left   = &setting_action_left_analog_dpad_mode;
      (*list)[list_info->index - 1].action_right  = &setting_action_right_analog_dpad_mode;
      (*list)[list_info->index - 1].action_select = &setting_action_right_analog_dpad_mode;
      (*list)[list_info->index - 1].action_start = &setting_action_start_analog_dpad_mode;
      (*list)[list_info->index - 1].get_string_representation = 
         &setting_get_string_representation_uint_analog_dpad_mode;
      menu_settings_list_current_add_enum_idx(list, list_info, (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_PLAYER_ANALOG_DPAD_MODE + user));

      CONFIG_ACTION(
            list, list_info,
            key[user],
            label[user],
            &group_info,
            &subgroup_info,
            parent_group);
      (*list)[list_info->index - 1].index = user + 1;
      (*list)[list_info->index - 1].index_offset = user;
      (*list)[list_info->index - 1].action_start  = &setting_action_start_bind_device;
      (*list)[list_info->index - 1].action_left   = &setting_action_left_bind_device;
      (*list)[list_info->index - 1].action_right  = &setting_action_right_bind_device;
      (*list)[list_info->index - 1].action_select = &setting_action_right_bind_device;
      (*list)[list_info->index - 1].get_string_representation = &get_string_representation_bind_device;

      CONFIG_ACTION(
            list, list_info,
            key_bind_all[user],
            label_bind_all[user],
            &group_info,
            &subgroup_info,
            parent_group);
      (*list)[list_info->index - 1].index          = user + 1;
      (*list)[list_info->index - 1].index_offset   = user;
      (*list)[list_info->index - 1].action_ok      = &setting_action_ok_bind_all;
      (*list)[list_info->index - 1].action_cancel  = NULL;

      CONFIG_ACTION(
            list, list_info,
            key_bind_defaults[user],
            label_bind_defaults[user],
            &group_info,
            &subgroup_info,
            parent_group);
      (*list)[list_info->index - 1].index          = user + 1;
      (*list)[list_info->index - 1].index_offset   = user;
      (*list)[list_info->index - 1].action_ok      = &setting_action_ok_bind_defaults;
      (*list)[list_info->index - 1].action_cancel  = NULL;

      CONFIG_ACTION(
            list, list_info,
            key_bind_all_save_autoconfig[user],
            label_bind_all_save_autoconfig[user],
            &group_info,
            &subgroup_info,
            parent_group);
      (*list)[list_info->index - 1].index          = user + 1;
      (*list)[list_info->index - 1].index_offset   = user;
      (*list)[list_info->index - 1].action_ok      = &setting_action_ok_bind_all_save_autoconfig;
      (*list)[list_info->index - 1].action_cancel  = NULL;
   }

   for (i = 0; i < RARCH_BIND_LIST_END; i ++)
   {
      char label[PATH_MAX_LENGTH] = {0};
      char name[PATH_MAX_LENGTH]  = {0};

      if (input_config_bind_map_get_meta(i))
         continue;

      fill_pathname_noext(label, buffer[user],
            " ",
            sizeof(label));

      if (
            settings->input.input_descriptor_label_show
            && (i < RARCH_FIRST_META_KEY)
            && core_has_set_input_descriptor()
            && (i != RARCH_TURBO_ENABLE)
         )
      {
         if (system->input_desc_btn[user][i])
            strlcat(label, 
                  system->input_desc_btn[user][i],
                  sizeof(label));
         else
         {
            strlcat(label, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
                  sizeof(label));

            if (settings->input.input_descriptor_hide_unbound)
               continue;
         }
      }
      else
         strlcat(label, input_config_bind_map_get_desc(i), sizeof(label));

      snprintf(name, sizeof(name), "p%u_%s", user + 1, input_config_bind_map_get_base(i));

      CONFIG_BIND(
            list, list_info,
            &settings->input.binds[user][i],
            user + 1,
            user,
            strdup(name),
            strdup(label),
            &defaults[i],
            &group_info,
            &subgroup_info,
            parent_group);
      (*list)[list_info->index - 1].bind_type = i + MENU_SETTINGS_BIND_BEGIN;
   }

   END_SUB_GROUP(list, list_info, parent_group);
   END_GROUP(list, list_info, parent_group);

   return true;
}

static bool setting_append_list(
      enum settings_list_type type,
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      const char *parent_group)
{
   unsigned user;
   rarch_setting_group_info_t group_info    = {0};
   rarch_setting_group_info_t subgroup_info = {0};
   settings_t *settings  = config_get_ptr();
   global_t   *global   = global_get_ptr();

   (void)settings;
   (void)global;

   switch (type)
   {
      case SETTINGS_LIST_MAIN_MENU:
         START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU), parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_MAIN_MENU);
         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_INT(
               list, list_info,
               &settings->state_slot,
               msg_hash_to_str(MENU_ENUM_LABEL_STATE_SLOT),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_STATE_SLOT),
               0,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, -1, 0, 1, true, false);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_STATE_SLOT);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_START_CORE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_START_CORE),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_START_CORE);

#if defined(HAVE_VIDEO_PROCESSOR)
         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_START_VIDEO_PROCESSOR),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_START_VIDEO_PROCESSOR),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_START_VIDEO_PROCESSOR);
#endif

#if defined(HAVE_NETWORKING) && defined(HAVE_NETWORKGAMEPAD)
         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_START_NET_RETROPAD),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_START_NET_RETROPAD),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_START_NET_RETROPAD);
#endif

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CONTENT_SETTINGS);

#ifndef HAVE_DYNAMIC
         if (frontend_driver_has_fork())
#endif
         {
            char ext_name[PATH_MAX_LENGTH] = {0};

            if (frontend_driver_get_core_extension(ext_name, sizeof(ext_name)))
            {
               CONFIG_ACTION(
                     list, list_info,
                     msg_hash_to_str(MENU_ENUM_LABEL_CORE_LIST),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_LIST),
                     &group_info,
                     &subgroup_info,
                     parent_group);
               (*list)[list_info->index - 1].size                = path_get_realsize(RARCH_PATH_CORE);
               (*list)[list_info->index - 1].value.target.string = path_get_ptr(RARCH_PATH_CORE);
               (*list)[list_info->index - 1].values       = ext_name;
               menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_LOAD_CORE);
               settings_data_list_current_add_flags(list, list_info, SD_FLAG_BROWSER_ACTION);
               menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CORE_LIST);
            }
         }

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_LIST),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_LOAD_CONTENT_LIST);

         if (settings->history_list_enable)
         {
            CONFIG_ACTION(
                  list, list_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOAD_CONTENT_HISTORY),
                  &group_info,
                  &subgroup_info,
                  parent_group);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_LOAD_CONTENT_HISTORY);
         }

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_ADD_CONTENT_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ADD_CONTENT_LIST),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_ADD_CONTENT_LIST);

#if defined(HAVE_NETWORKING)
         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_NETPLAY);
#endif

#if defined(HAVE_NETWORKING)
         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_ONLINE_UPDATER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONLINE_UPDATER),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_ONLINE_UPDATER);
#endif

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SETTINGS);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_INFORMATION_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INFORMATION_LIST),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INFORMATION_LIST);

#ifndef __CELLOS_LV2__
         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_RESTART_RETROARCH),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RESTART_RETROARCH),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_RESTART_RETROARCH);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_RESTART_RETROARCH);
#endif

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_CONFIGURATIONS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONFIGURATIONS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CONFIGURATIONS);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_MENU_SAVE_CURRENT_CONFIG);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_SAVE_NEW_CONFIG),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_NEW_CONFIG),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_MENU_SAVE_CONFIG);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SAVE_NEW_CONFIG);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_CORE),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_CORE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_CORE);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG_OVERRIDE_GAME),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_MENU_SAVE_CURRENT_CONFIG_OVERRIDE_GAME);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SAVE_CURRENT_CONFIG_OVERRIDE_GAME);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_HELP_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HELP_LIST),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_HELP_LIST);

#if !defined(IOS)
         /* Apple rejects iOS apps that lets you forcibly quit an application. */
         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_QUIT_RETROARCH),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_QUIT_RETROARCH),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_QUIT);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_QUIT_RETROARCH);
#endif

#if defined(HAVE_LAKKA)
         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_SHUTDOWN),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHUTDOWN),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_SHUTDOWN);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SHUTDOWN);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_REBOOT),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REBOOT),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REBOOT);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_REBOOT);
#endif

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_DRIVER_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_DRIVER_SETTINGS);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_SETTINGS);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUDIO_SETTINGS);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_INPUT_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_SETTINGS);

         if (settings->menu.show_advanced_settings)
         {
            CONFIG_ACTION(
                  list, list_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_CORE_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_SETTINGS),
                  &group_info,
                  &subgroup_info,
                  parent_group);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CORE_SETTINGS);
         }

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_CONFIGURATION_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CONFIGURATION_SETTINGS);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_SAVING_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SAVING_SETTINGS);

         if (settings->menu.show_advanced_settings)
         {
            CONFIG_ACTION(
                  list, list_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_LOGGING_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS),
                  &group_info,
                  &subgroup_info,
                  parent_group);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_LOGGING_SETTINGS);
         }

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_REWIND_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_REWIND_SETTINGS);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_RECORDING_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_RECORDING_SETTINGS);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONSCREEN_OVERLAY_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_ONSCREEN_OVERLAY_SETTINGS);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_MENU_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_MENU_SETTINGS);

#if !defined(RARCH_CONSOLE) && !defined(HAVE_LAKKA)
         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS);
#endif

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RETRO_ACHIEVEMENTS_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_UPDATER_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UPDATER_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_UPDATER_SETTINGS);

         if (!string_is_equal(settings->wifi.driver, "null"))
         {
            CONFIG_ACTION(
                  list, list_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_WIFI_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_WIFI_SETTINGS),
                  &group_info,
                  &subgroup_info,
                  parent_group);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_WIFI_SETTINGS);
         }

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_NETWORK_SETTINGS);

#ifdef HAVE_LAKKA
         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_LAKKA_SERVICES),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_LAKKA_SERVICES);
#endif

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_PLAYLIST_SETTINGS);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_USER_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_USER_SETTINGS);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_DIRECTORY_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_DIRECTORY_SETTINGS);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_PRIVACY_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_PRIVACY_SETTINGS);

         for (user = 0; user < MAX_USERS; user++)
            setting_append_list_input_player_options(list, list_info, parent_group, user);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_DRIVERS:
         START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DRIVER_SETTINGS), parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_DRIVER_SETTINGS);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info,
               &subgroup_info, parent_group);

         CONFIG_STRING_OPTIONS(
               list, list_info,
               settings->input.driver,
               sizeof(settings->input.driver),
               msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DRIVER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_DRIVER),
               config_get_default_input(),
               config_get_input_driver_options(),
               &group_info,
               &subgroup_info,
               parent_group,
               general_read_handler,
               general_write_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);
         (*list)[list_info->index - 1].action_left  = setting_string_action_left_driver;
         (*list)[list_info->index - 1].action_right = setting_string_action_right_driver;
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_DRIVER);

         CONFIG_STRING_OPTIONS(
               list, list_info,
               settings->input.joypad_driver,
               sizeof(settings->input.driver),
               msg_hash_to_str(MENU_ENUM_LABEL_JOYPAD_DRIVER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_JOYPAD_DRIVER),
               config_get_default_joypad(),
               config_get_joypad_driver_options(),
               &group_info,
               &subgroup_info,
               parent_group,
               general_read_handler,
               general_write_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);
         (*list)[list_info->index - 1].action_left  = setting_string_action_left_driver;
         (*list)[list_info->index - 1].action_right = setting_string_action_right_driver;
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_JOYPAD_DRIVER);

         CONFIG_STRING_OPTIONS(
               list, list_info,
               settings->video.driver,
               sizeof(settings->video.driver),
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DRIVER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER),
               config_get_default_video(),
               config_get_video_driver_options(),
               &group_info,
               &subgroup_info,
               parent_group,
               general_read_handler,
               general_write_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);
         (*list)[list_info->index - 1].action_left  = setting_string_action_left_driver;
         (*list)[list_info->index - 1].action_right = setting_string_action_right_driver;
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_DRIVER);

         CONFIG_STRING_OPTIONS(
               list, list_info,
               settings->audio.driver,
               sizeof(settings->audio.driver),
               msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DRIVER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_DRIVER),
               config_get_default_audio(),
               config_get_audio_driver_options(),
               &group_info,
               &subgroup_info,
               parent_group,
               general_read_handler,
               general_write_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);
         (*list)[list_info->index - 1].action_left  = setting_string_action_left_driver;
         (*list)[list_info->index - 1].action_right = setting_string_action_right_driver;
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUDIO_DRIVER);

         CONFIG_STRING_OPTIONS(
               list, list_info,
               settings->audio.resampler,
               sizeof(settings->audio.resampler),
               msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_RESAMPLER_DRIVER),
               config_get_default_audio_resampler(),
               config_get_audio_resampler_driver_options(),
               &group_info,
               &subgroup_info,
               parent_group,
               general_read_handler,
               general_write_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);
         (*list)[list_info->index - 1].action_left  = setting_string_action_left_driver;
         (*list)[list_info->index - 1].action_right = setting_string_action_right_driver;
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUDIO_RESAMPLER_DRIVER);

         CONFIG_STRING_OPTIONS(
               list, list_info,
               settings->camera.driver,
               sizeof(settings->camera.driver),
               msg_hash_to_str(MENU_ENUM_LABEL_CAMERA_DRIVER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CAMERA_DRIVER),
               config_get_default_camera(),
               config_get_camera_driver_options(),
               &group_info,
               &subgroup_info,
               parent_group,
               general_read_handler,
               general_write_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);
         (*list)[list_info->index - 1].action_left  = setting_string_action_left_driver;
         (*list)[list_info->index - 1].action_right = setting_string_action_right_driver;
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CAMERA_DRIVER);

         CONFIG_STRING_OPTIONS(
               list, list_info,
               settings->wifi.driver,
               sizeof(settings->wifi.driver),
               msg_hash_to_str(MENU_ENUM_LABEL_WIFI_DRIVER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_WIFI_DRIVER),
               config_get_default_wifi(),
               config_get_wifi_driver_options(),
               &group_info,
               &subgroup_info,
               parent_group,
               general_read_handler,
               general_write_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);
         (*list)[list_info->index - 1].action_left  = setting_string_action_left_driver;
         (*list)[list_info->index - 1].action_right = setting_string_action_right_driver;
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_WIFI_DRIVER);

         CONFIG_STRING_OPTIONS(
               list, list_info,
               settings->location.driver,
               sizeof(settings->location.driver),
               msg_hash_to_str(MENU_ENUM_LABEL_LOCATION_DRIVER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOCATION_DRIVER),
               config_get_default_location(),
               config_get_location_driver_options(),
               &group_info,
               &subgroup_info,
               parent_group,
               general_read_handler,
               general_write_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);
         (*list)[list_info->index - 1].action_left  = setting_string_action_left_driver;
         (*list)[list_info->index - 1].action_right = setting_string_action_right_driver;
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_LOCATION_DRIVER);

         CONFIG_STRING_OPTIONS(
               list, list_info,
               settings->menu.driver,
               sizeof(settings->menu.driver),
               msg_hash_to_str(MENU_ENUM_LABEL_MENU_DRIVER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_DRIVER),
               config_get_default_menu(),
               config_get_menu_driver_options(),
               &group_info,
               &subgroup_info,
               parent_group,
               general_read_handler,
               general_write_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);
         (*list)[list_info->index - 1].action_left  = setting_string_action_left_driver;
         (*list)[list_info->index - 1].action_right = setting_string_action_right_driver;
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_MENU_DRIVER);

         CONFIG_STRING_OPTIONS(
               list, list_info,
               settings->record.driver,
               sizeof(settings->record.driver),
               msg_hash_to_str(MENU_ENUM_LABEL_RECORD_DRIVER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RECORD_DRIVER),
               config_get_default_record(),
               config_get_record_driver_options(),
               &group_info,
               &subgroup_info,
               parent_group,
               general_read_handler,
               general_write_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);
         (*list)[list_info->index - 1].action_left  = setting_string_action_left_driver;
         (*list)[list_info->index - 1].action_right = setting_string_action_right_driver;
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_RECORD_DRIVER);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_CORE:
         START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_SETTINGS), parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CORE_SETTINGS);

         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info,
               parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->video.shared_context,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHARED_CONTEXT),
               video_shared_context,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_SHARED_CONTEXT);

         CONFIG_BOOL(
               list, list_info,
               &settings->load_dummy_on_core_shutdown,
               msg_hash_to_str(MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DUMMY_ON_CORE_SHUTDOWN),
               load_dummy_on_core_shutdown,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_DUMMY_ON_CORE_SHUTDOWN);

         CONFIG_BOOL(
               list, list_info,
               &settings->set_supports_no_game_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE),
               true,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CORE_SET_SUPPORTS_NO_CONTENT_ENABLE);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_CONFIGURATION:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONFIGURATION_SETTINGS), parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_CONFIGURATION_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info,
               parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->config_save_on_exit,
               msg_hash_to_str(MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONFIG_SAVE_ON_EXIT),
               config_save_on_exit,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CONFIG_SAVE_ON_EXIT);

         CONFIG_BOOL(
               list, list_info,
               &settings->confirm_on_exit,
               msg_hash_to_str(MENU_ENUM_LABEL_CONFIRM_ON_EXIT),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONFIRM_ON_EXIT),
               confirm_on_exit,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CONFIRM_ON_EXIT);

         CONFIG_BOOL(
               list, list_info,
               &settings->show_hidden_files,
               msg_hash_to_str(MENU_ENUM_LABEL_SHOW_HIDDEN_FILES),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHOW_HIDDEN_FILES),
               show_hidden_files,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SHOW_HIDDEN_FILES);

         CONFIG_BOOL(
               list, list_info,
               &settings->game_specific_options,
               msg_hash_to_str(MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_GAME_SPECIFIC_OPTIONS),
               default_game_specific_options,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_GAME_SPECIFIC_OPTIONS);

         CONFIG_BOOL(
               list, list_info,
               &settings->auto_overrides_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUTO_OVERRIDES_ENABLE),
               default_auto_overrides_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUTO_OVERRIDES_ENABLE);

         CONFIG_BOOL(
               list, list_info,
               &settings->auto_remaps_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUTO_REMAPS_ENABLE),
               default_auto_remaps_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUTO_REMAPS_ENABLE);

         CONFIG_BOOL(
               list, list_info,
               &settings->auto_shaders_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_AUTO_SHADERS_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUTO_SHADERS_ENABLE),
               default_auto_shaders_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUTO_SHADERS_ENABLE);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_LOGGING:
         {
            bool *tmp_b = NULL;
            START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOGGING_SETTINGS), parent_group);
            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_LOGGING_SETTINGS);

            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

            START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info,
                  parent_group);

            CONFIG_BOOL(
                  list, list_info,
                  verbosity_get_ptr(),
                  msg_hash_to_str(MENU_ENUM_LABEL_LOG_VERBOSITY),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOG_VERBOSITY),
                  false,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_LOG_VERBOSITY);

            CONFIG_UINT(
                  list, list_info,
                  &settings->libretro_log_level,
                  msg_hash_to_str(MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_LOG_LEVEL),
                  libretro_log_level,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, 3, 1.0, true, true);
            (*list)[list_info->index - 1].get_string_representation = 
               &setting_get_string_representation_uint_libretro_log_level;
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_LIBRETRO_LOG_LEVEL);

            END_SUB_GROUP(list, list_info, parent_group);

            START_SUB_GROUP(list, list_info, "Performance Counters", &group_info, &subgroup_info,
                  parent_group);

            runloop_ctl(RUNLOOP_CTL_GET_PERFCNT, &tmp_b);

            CONFIG_BOOL(
                  list, list_info,
                  tmp_b,
                  msg_hash_to_str(MENU_ENUM_LABEL_PERFCNT_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PERFCNT_ENABLE),
                  false,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_PERFCNT_ENABLE);
         }
         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_SAVING:
         START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVING_SETTINGS), parent_group);
         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SAVING_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info,
               parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->sort_savefiles_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SORT_SAVEFILES_ENABLE),
               default_sort_savefiles_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SORT_SAVEFILES_ENABLE);

         CONFIG_BOOL(
               list, list_info,
               &settings->sort_savestates_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SORT_SAVESTATES_ENABLE),
               default_sort_savestates_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SORT_SAVESTATES_ENABLE);

         CONFIG_BOOL(
               list, list_info,
               &settings->block_sram_overwrite,
               msg_hash_to_str(MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BLOCK_SRAM_OVERWRITE),
               block_sram_overwrite,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_BLOCK_SRAM_OVERWRITE);

#ifdef HAVE_THREADS
         CONFIG_UINT(
               list, list_info,
               &settings->autosave_interval,
               msg_hash_to_str(MENU_ENUM_LABEL_AUTOSAVE_INTERVAL),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUTOSAVE_INTERVAL),
               autosave_interval,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_AUTOSAVE_INIT);
         menu_settings_list_current_add_range(list, list_info, 0, 0, 10, true, false);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);
         (*list)[list_info->index - 1].get_string_representation = 
            &setting_get_string_representation_uint_autosave_interval;
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUTOSAVE_INTERVAL);
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->savestate_auto_index,
               msg_hash_to_str(MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_INDEX),
               savestate_auto_index,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SAVESTATE_AUTO_INDEX);

         CONFIG_BOOL(
               list, list_info,
               &settings->savestate_auto_save,
               msg_hash_to_str(MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_SAVE),
               savestate_auto_save,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SAVESTATE_AUTO_SAVE);

         CONFIG_BOOL(
               list, list_info,
               &settings->savestate_auto_load,
               msg_hash_to_str(MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVESTATE_AUTO_LOAD),
               savestate_auto_load,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SAVESTATE_AUTO_LOAD);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_REWIND:
         START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REWIND_SETTINGS), parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_REWIND_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);


         CONFIG_BOOL(
               list, list_info,
               &settings->rewind_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_REWIND_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REWIND_ENABLE),
               rewind_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_CMD_APPLY_AUTO);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REWIND_TOGGLE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_REWIND_ENABLE);

#if 0
         CONFIG_SIZE(
               settings->rewind_buffer_size,
               "rewind_buffer_size",
               "Rewind Buffer Size",
               rewind_buffer_size,
               group_info,
               subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler)
#endif
            CONFIG_UINT(
                  list, list_info,
                  &settings->rewind_granularity,
                  msg_hash_to_str(MENU_ENUM_LABEL_REWIND_GRANULARITY),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_REWIND_GRANULARITY),
                  rewind_granularity,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 1, 32768, 1, true, false);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_REWIND_GRANULARITY);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_VIDEO:
         START_GROUP(list, list_info, &group_info, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SETTINGS), parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_SETTINGS);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

#if !defined(RARCH_CONSOLE) && !defined(RARCH_MOBILE)
         CONFIG_BOOL(
               list, list_info,
               &settings->ui.suspend_screensaver_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SUSPEND_SCREENSAVER_ENABLE),
               true,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SUSPEND_SCREENSAVER_ENABLE);
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->fps_show,
               msg_hash_to_str(MENU_ENUM_LABEL_FPS_SHOW),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FPS_SHOW),
               fps_show,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_FPS_SHOW);

         END_SUB_GROUP(list, list_info, parent_group);
         START_SUB_GROUP(list, list_info, "Platform-specific", &group_info, &subgroup_info, parent_group);

         video_driver_menu_settings((void**)list, (void*)list_info, (void*)&group_info, (void*)&subgroup_info, parent_group);

         END_SUB_GROUP(list, list_info, parent_group);

         END_SUB_GROUP(list, list_info, parent_group);
         START_SUB_GROUP(list, list_info, "Monitor", &group_info, &subgroup_info, parent_group);

         CONFIG_UINT(
               list, list_info,
               &settings->video.monitor_index,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_MONITOR_INDEX),
               monitor_index,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REINIT);
         menu_settings_list_current_add_range(list, list_info, 0, 1, 1, true, false);
         (*list)[list_info->index - 1].get_string_representation = 
            &setting_get_string_representation_uint_video_monitor_index;
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_MONITOR_INDEX);

         if (video_driver_has_windowed())
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->video.fullscreen,
                  msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_FULLSCREEN),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_FULLSCREEN),
                  fullscreen,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_CMD_APPLY_AUTO);
            menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REINIT);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_FULLSCREEN);
         }
         if (video_driver_has_windowed())
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->video.windowed_fullscreen,
                  msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_WINDOWED_FULLSCREEN),
                  windowed_fullscreen,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_WINDOWED_FULLSCREEN);
         }

         CONFIG_FLOAT(
               list, list_info,
               &settings->video.refresh_rate,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_REFRESH_RATE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE),
               refresh_rate,
               "%.3f Hz",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 0, 0, 0.001, true, false);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_REFRESH_RATE);

         CONFIG_FLOAT(
               list, list_info,
               &settings->video.refresh_rate,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_REFRESH_RATE_AUTO),
               refresh_rate,
               "%.3f Hz",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].action_start  = &setting_action_start_video_refresh_rate_auto;
         (*list)[list_info->index - 1].action_ok     = &setting_action_ok_video_refresh_rate_auto;
         (*list)[list_info->index - 1].action_select = &setting_action_ok_video_refresh_rate_auto;
         (*list)[list_info->index - 1].get_string_representation = 
            &setting_get_string_representation_st_float_video_refresh_rate_auto;
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_REFRESH_RATE_AUTO);

         if (string_is_equal(settings->video.driver, "gl"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->video.force_srgb_disable,
                  msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_FORCE_SRGB_DISABLE),
                  false,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_CMD_APPLY_AUTO | SD_FLAG_ADVANCED
                  );
            menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REINIT);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_FORCE_SRGB_DISABLE);
         }

         END_SUB_GROUP(list, list_info, parent_group);
         START_SUB_GROUP(list, list_info, "Aspect", &group_info, &subgroup_info, parent_group);
         CONFIG_UINT(
               list, list_info,
               &settings->video.aspect_ratio_idx,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_INDEX),
               aspect_ratio_idx,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_cmd(
               list,
               list_info,
               CMD_EVENT_VIDEO_SET_ASPECT_RATIO);
         menu_settings_list_current_add_range(
               list,
               list_info,
               0,
               LAST_ASPECT_RATIO,
               1,
               true,
               true);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);
         (*list)[list_info->index - 1].get_string_representation = 
            &setting_get_string_representation_uint_aspect_ratio_index;
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_ASPECT_RATIO_INDEX);

         CONFIG_INT(
               list, list_info,
               &settings->video_viewport_custom.x,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_X),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_X),
               0,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, -99999, 0, 1, false, false);
         menu_settings_list_current_add_cmd(
               list,
               list_info,
               CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_X);

         CONFIG_INT(
               list, list_info,
               &settings->video_viewport_custom.y,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_Y),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_Y),
               0,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, -99999, 0, 1, false, false);
         menu_settings_list_current_add_cmd(
               list,
               list_info,
               CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_Y);

         CONFIG_UINT(
               list, list_info,
               &settings->video_viewport_custom.width,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_WIDTH),
               0,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 0, 0, 1, true, false);
         (*list)[list_info->index - 1].action_start = &setting_action_start_custom_viewport_width;
         (*list)[list_info->index - 1].action_left  = setting_uint_action_left_custom_viewport_width;
         (*list)[list_info->index - 1].action_right = setting_uint_action_right_custom_viewport_width;
         menu_settings_list_current_add_cmd(
               list,
               list_info,
               CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_WIDTH);

         CONFIG_UINT(
               list, list_info,
               &settings->video_viewport_custom.height,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_VIEWPORT_CUSTOM_HEIGHT),
               0,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 0, 0, 1, true, false);
         (*list)[list_info->index - 1].action_start = &setting_action_start_custom_viewport_height;
         (*list)[list_info->index - 1].action_left  = setting_uint_action_left_custom_viewport_height;
         (*list)[list_info->index - 1].action_right = setting_uint_action_right_custom_viewport_height;
         menu_settings_list_current_add_cmd(
               list,
               list_info,
               CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_VIEWPORT_CUSTOM_HEIGHT);

         END_SUB_GROUP(list, list_info, parent_group);
         START_SUB_GROUP(list, list_info, "Scaling", &group_info, &subgroup_info, parent_group);

         if (video_driver_has_windowed())
         {
            CONFIG_FLOAT(
                  list, list_info,
                  &settings->video.scale,
                  msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SCALE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SCALE),
                  scale,
                  "%.1fx",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 1.0, 10.0, 1.0, true, true);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_SCALE);
         }

         CONFIG_BOOL(
               list, list_info,
               &settings->video.scale_integer,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SCALE_INTEGER),
               scale_integer,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_cmd(
               list,
               list_info,
               CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_SCALE_INTEGER);

#ifdef GEKKO
         CONFIG_UINT(
               list, list_info,
               &settings->video.viwidth,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_VI_WIDTH),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_VI_WIDTH),
               video_viwidth,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 640, 720, 2, true, true);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_VI_WIDTH);

         CONFIG_BOOL(
               list, list_info,
               &settings->video.vfilter,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_VFILTER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_VFILTER),
               video_vfilter,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_VFILTER);
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->video.smooth,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SMOOTH),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SMOOTH),
               video_smooth,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_SMOOTH);

         CONFIG_UINT(
               list, list_info,
               &settings->video.rotation,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_ROTATION),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_ROTATION),
               0,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 0, 3, 1, true, true);
         (*list)[list_info->index - 1].get_string_representation = 
            &setting_get_string_representation_uint_video_rotation;
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_ROTATION);

         END_SUB_GROUP(list, list_info, parent_group);
         START_SUB_GROUP(
               list,
               list_info,
               "Synchronization",
               &group_info,
               &subgroup_info,
               parent_group);

#if defined(HAVE_THREADS)
         CONFIG_BOOL(
               list, list_info,
               &settings->video.threaded,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_THREADED),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_THREADED),
               video_threaded,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_CMD_APPLY_AUTO | SD_FLAG_ADVANCED
               );
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REINIT);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_THREADED);
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->video.vsync,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_VSYNC),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_VSYNC),
               vsync,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_VSYNC);

         CONFIG_UINT(
               list, list_info,
               &settings->video.max_swapchain_images,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_MAX_SWAPCHAIN_IMAGES),
               max_swapchain_images,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 1, 4, 1, true, true);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO|SD_FLAG_ADVANCED);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_MAX_SWAPCHAIN_IMAGES);

         CONFIG_UINT(
               list, list_info,
               &settings->video.swap_interval,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SWAP_INTERVAL),
               swap_interval,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_VIDEO_SET_BLOCKING_STATE);
         menu_settings_list_current_add_range(list, list_info, 1, 4, 1, true, true);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO|SD_FLAG_ADVANCED);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_SWAP_INTERVAL);

         if (string_is_equal(settings->video.driver, "gl"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->video.hard_sync,
                  msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_HARD_SYNC),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC),
                  hard_sync,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_HARD_SYNC);

            CONFIG_UINT(
                  list, list_info,
                  &settings->video.hard_sync_frames,
                  msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_HARD_SYNC_FRAMES),
                  hard_sync_frames,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, 3, 1, true, true);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_HARD_SYNC_FRAMES);
         }

         CONFIG_UINT(
               list, list_info,
               &settings->video.frame_delay,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_FRAME_DELAY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_FRAME_DELAY),
               frame_delay,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 0, 15, 1, true, true);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_FRAME_DELAY);

#if !defined(RARCH_MOBILE)
         CONFIG_BOOL(
               list, list_info,
               &settings->video.black_frame_insertion,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_BLACK_FRAME_INSERTION),
               black_frame_insertion,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_BLACK_FRAME_INSERTION);
#endif
         END_SUB_GROUP(list, list_info, parent_group);
         START_SUB_GROUP(
               list,
               list_info,
               "Miscellaneous",
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->video.gpu_screenshot,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_GPU_SCREENSHOT),
               gpu_screenshot,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_GPU_SCREENSHOT);

         CONFIG_BOOL(
               list, list_info,
               &settings->video.allow_rotate,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_ALLOW_ROTATE),
               allow_rotate,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_ALLOW_ROTATE);

         CONFIG_BOOL(
               list, list_info,
               &settings->video.crop_overscan,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_CROP_OVERSCAN),
               crop_overscan,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_CROP_OVERSCAN);

         CONFIG_PATH(
               list, list_info,
               settings->path.softfilter_plugin,
               sizeof(settings->path.softfilter_plugin),
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_FILTER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_FILTER),
               settings->directory.video_filter,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_values(list, list_info, "filt");
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REINIT);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_FILTER);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_AUDIO:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_SETTINGS), parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUDIO_SETTINGS);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->audio.enable,
               msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_ENABLE),
               audio_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUDIO_ENABLE);

         CONFIG_BOOL(
               list, list_info,
               &settings->audio.mute_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_MUTE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_MUTE),
               false,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUDIO_MUTE);

         CONFIG_FLOAT(
               list, list_info,
               &settings->audio.volume,
               msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_VOLUME),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_VOLUME),
               audio_volume,
               "%.1f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, -80, 12, 1.0, true, true);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUDIO_VOLUME);

#ifdef __CELLOS_LV2__
         CONFIG_BOOL(
               list, list_info,
               &global->console.sound.system_bgm_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_SYSTEM_BGM_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_BGM_ENABLE),
               false,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SYSTEM_BGM_ENABLE);
#endif

         END_SUB_GROUP(list, list_info, parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(
               list,
               list_info,
               "Synchronization",
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->audio.sync,
               msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_SYNC),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_SYNC),
               audio_sync,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUDIO_SYNC);

         CONFIG_UINT(
               list, list_info,
               &settings->audio.latency,
               msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_LATENCY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_LATENCY),
               g_defaults.settings.out_latency ? 
               g_defaults.settings.out_latency : out_latency,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 8, 512, 16.0, true, true);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUDIO_LATENCY);

         CONFIG_FLOAT(
               list, list_info,
               &settings->audio.rate_control_delta,
               msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_RATE_CONTROL_DELTA),
               rate_control_delta,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(
               list,
               list_info,
               0,
               0,
               0.001,
               true,
               false);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUDIO_RATE_CONTROL_DELTA);

         CONFIG_FLOAT(
               list, list_info,
               &settings->audio.max_timing_skew,
               msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_MAX_TIMING_SKEW),
               max_timing_skew,
               "%.2f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(
               list,
               list_info,
               0.01,
               0.5,
               0.01,
               true,
               true);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUDIO_MAX_TIMING_SKEW);

         CONFIG_UINT(
               list, list_info,
               &settings->audio.block_frames,
               msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_BLOCK_FRAMES),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_BLOCK_FRAMES),
               0,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUDIO_BLOCK_FRAMES);

         END_SUB_GROUP(list, list_info, parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(
               list,
               list_info,
               "Miscellaneous",
               &group_info,
               &subgroup_info,
               parent_group);

#if !defined(RARCH_CONSOLE)
         CONFIG_STRING(
               list, list_info,
               settings->audio.device,
               sizeof(settings->audio.device),
               msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DEVICE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_DEVICE),
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT | SD_FLAG_ADVANCED);
         (*list)[list_info->index - 1].action_left   = &setting_string_action_left_audio_device;
         (*list)[list_info->index - 1].action_right  = &setting_string_action_right_audio_device;
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUDIO_DEVICE);
#endif

         CONFIG_UINT(
               list, list_info,
               &settings->audio.out_rate,
               msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_OUTPUT_RATE),
               out_rate,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUDIO_OUTPUT_RATE);

         CONFIG_PATH(
               list, list_info,
               settings->path.audio_dsp_plugin,
               sizeof(settings->path.audio_dsp_plugin),
               msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_DSP_PLUGIN),
               settings->directory.audio_filter,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_values(list, list_info, "dsp");
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_DSP_FILTER_INIT);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUDIO_DSP_PLUGIN);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_INPUT:
         {
            unsigned user;
            START_GROUP(list, list_info, &group_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_SETTINGS_BEGIN),
                  parent_group);

            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

            START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

            CONFIG_UINT(
                  list, list_info,
                  &settings->input.max_users,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_MAX_USERS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_MAX_USERS),
                  input_max_users,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 1, MAX_USERS, 1, true, true);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_MAX_USERS);

            CONFIG_UINT(
                  list, list_info,
                  &settings->input.poll_type_behavior,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_POLL_TYPE_BEHAVIOR),
                  input_poll_type_behavior,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, 2, 1, true, true);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR);

#if TARGET_OS_IPHONE
            CONFIG_BOOL(
                  list, list_info,
                  &settings->input.keyboard_gamepad_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_ICADE_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_ICADE_ENABLE),
                  false,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_POLL_TYPE_BEHAVIOR);

            CONFIG_UINT(
                  list, list_info,
                  &settings->input.keyboard_gamepad_mapping_type,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE),
                  1,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, 3, 1, true, true);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_KEYBOARD_GAMEPAD_MAPPING_TYPE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->input.small_keyboard_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_SMALL_KEYBOARD_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_SMALL_KEYBOARD_ENABLE),
                  false,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_SMALL_KEYBOARD_ENABLE);
#endif

#ifdef ANDROID
            CONFIG_BOOL(
                  list, list_info,
                  &settings->input.back_as_menu_toggle_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_BACK_AS_MENU_TOGGLE_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_BACK_AS_MENU_TOGGLE_ENABLE),
                  back_as_menu_toggle_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_BACK_AS_MENU_TOGGLE);
#endif

            CONFIG_UINT(
                  list, list_info,
                  &settings->input.menu_toggle_gamepad_combo,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO),
                  menu_toggle_gamepad_combo,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, 3, 1, true, true);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_MENU_ENUM_TOGGLE_GAMEPAD_COMBO);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->input.all_users_control_menu,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_ALL_USERS_CONTROL_MENU),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_ALL_USERS_CONTROL_MENU),
                  all_users_control_menu,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_ALL_USERS_CONTROL_MENU);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->input.remap_binds_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_REMAP_BINDS_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_REMAP_BINDS_ENABLE),
                  true,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_REMAP_BINDS_ENABLE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->input.autodetect_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_AUTODETECT_ENABLE),
                  input_autodetect_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_AUTODETECT_ENABLE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->input.input_descriptor_label_show,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_LABEL_SHOW),
                  true,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED
                  );
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_DESCRIPTOR_LABEL_SHOW);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->input.input_descriptor_hide_unbound,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_DESCRIPTOR_HIDE_UNBOUND),
                  input_descriptor_hide_unbound,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED
                  );
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_DESCRIPTOR_HIDE_UNBOUND);

            END_SUB_GROUP(list, list_info, parent_group);


            START_SUB_GROUP(
                  list,
                  list_info,
                  "Turbo/Deadzone",
                  &group_info,
                  &subgroup_info,
                  parent_group);

            CONFIG_FLOAT(
                  list, list_info,
                  &settings->input.axis_threshold,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_AXIS_THRESHOLD),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_AXIS_THRESHOLD),
                  axis_threshold,
                  "%.3f",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, 1.00, 0.001, true, true);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_AXIS_THRESHOLD);
            
            CONFIG_UINT(
                  list, list_info,
                  &settings->input.bind_timeout,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_BIND_TIMEOUT),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_BIND_TIMEOUT),
                  input_bind_timeout,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 1, 0, 1, true, false);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_BIND_TIMEOUT);

            CONFIG_UINT(
                  list, list_info,
                  &settings->input.turbo_period,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_TURBO_PERIOD),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_TURBO_PERIOD),
                  turbo_period,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 1, 0, 1, true, false);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_TURBO_PERIOD);

            CONFIG_UINT(
                  list, list_info,
                  &settings->input.turbo_duty_cycle,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_DUTY_CYCLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_DUTY_CYCLE),
                  turbo_duty_cycle,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 1, 0, 1, true, false);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_DUTY_CYCLE);

            END_SUB_GROUP(list, list_info, parent_group);

            START_SUB_GROUP(list, list_info, "Binds", &group_info, &subgroup_info, parent_group);

            CONFIG_ACTION(
                  list, list_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_HOTKEY_BINDS),
                  &group_info,
                  &subgroup_info,
                  parent_group);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS);

            for (user = 0; user < MAX_USERS; user++)
            {
               static char binds_list[MAX_USERS][PATH_MAX_LENGTH];
               static char binds_label[MAX_USERS][PATH_MAX_LENGTH];
               unsigned user_value = user + 1;

               snprintf(binds_list[user],  sizeof(binds_list[user]), "%d_input_binds_list", user_value);
               snprintf(binds_label[user], sizeof(binds_label[user]), "Input User %d Binds", user_value);

               CONFIG_ACTION(
                     list, list_info,
                     binds_list[user],
                     binds_label[user],
                     &group_info,
                     &subgroup_info,
                     parent_group);
               (*list)[list_info->index - 1].index          = user_value;
               (*list)[list_info->index - 1].index_offset   = user;

               menu_settings_list_current_add_enum_idx(list, list_info, (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_USER_1_BINDS + user));
            }

            END_SUB_GROUP(list, list_info, parent_group);

            END_GROUP(list, list_info, parent_group);
         }
         break;
      case SETTINGS_LIST_RECORDING:
            START_GROUP(list, list_info, &group_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RECORDING_SETTINGS),
                  parent_group);

            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_RECORDING_SETTINGS);

            START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

            CONFIG_BOOL(
                  list, list_info,
                  recording_is_enabled(),
                  msg_hash_to_str(MENU_ENUM_LABEL_RECORD_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RECORD_ENABLE),
                  false,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_RECORD_ENABLE);

            CONFIG_PATH(
                  list, list_info,
                  global->record.config,
                  sizeof(global->record.config),
                  msg_hash_to_str(MENU_ENUM_LABEL_RECORD_CONFIG),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RECORD_CONFIG),
                  "",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_values(list, list_info, "cfg");
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_RECORD_CONFIG);

            CONFIG_STRING(
                  list, list_info,
                  global->record.path,
                  sizeof(global->record.path),
                  msg_hash_to_str(MENU_ENUM_LABEL_RECORD_PATH),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RECORD_PATH),
                  "",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_RECORD_PATH);

            CONFIG_BOOL(
                  list, list_info,
                  recording_driver_get_use_output_dir_ptr(),
                  msg_hash_to_str(MENU_ENUM_LABEL_RECORD_USE_OUTPUT_DIRECTORY),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RECORD_USE_OUTPUT_DIRECTORY),
                  false,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_RECORD_USE_OUTPUT_DIRECTORY);

            END_SUB_GROUP(list, list_info, parent_group);

            START_SUB_GROUP(list, list_info, "Miscellaneous", &group_info, &subgroup_info, parent_group);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->video.post_filter_record,
                  msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_POST_FILTER_RECORD),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_POST_FILTER_RECORD),
                  post_filter_record,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_POST_FILTER_RECORD);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->video.gpu_record,
                  msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_GPU_RECORD),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_GPU_RECORD),
                  gpu_record,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_GPU_RECORD);

            END_SUB_GROUP(list, list_info, parent_group);
            END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_INPUT_HOTKEY:
         {
            unsigned i;
            START_GROUP(list, list_info, &group_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_INPUT_HOTKEY_BINDS_BEGIN),
                  parent_group);

            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

            START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info,
                  parent_group);

            for (i = 0; i < RARCH_BIND_LIST_END; i ++)
            {
               if (!input_config_bind_map_get_meta(i))
                  continue;

               CONFIG_BIND(
                     list, list_info,
                     &settings->input.binds[0][i], 0, 0,
                     strdup(input_config_bind_map_get_base(i)),
                     strdup(input_config_bind_map_get_desc(i)),
                     &retro_keybinds_1[i],
                     &group_info, &subgroup_info, parent_group);
               (*list)[list_info->index - 1].bind_type = i + MENU_SETTINGS_BIND_BEGIN;
               menu_settings_list_current_add_enum_idx(list, list_info, 
                     (enum msg_hash_enums)(MENU_ENUM_LABEL_INPUT_HOTKEY_BIND_BEGIN + i));
            }

            END_SUB_GROUP(list, list_info, parent_group);
            END_GROUP(list, list_info, parent_group);
         }
         break;
      case SETTINGS_LIST_FRAME_THROTTLING:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FRAME_THROTTLE_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_FRAME_THROTTLE_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_FLOAT(
               list, list_info,
               &settings->fastforward_ratio,
               msg_hash_to_str(MENU_ENUM_LABEL_FASTFORWARD_RATIO),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FASTFORWARD_RATIO),
               fastforward_ratio,
               "%.1fx",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_SET_FRAME_LIMIT);
         menu_settings_list_current_add_range(list, list_info, 0, 10, 1.0, true, true);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_FASTFORWARD_RATIO);

         CONFIG_FLOAT(
               list, list_info,
               &settings->slowmotion_ratio,
               msg_hash_to_str(MENU_ENUM_LABEL_SLOWMOTION_RATIO),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SLOWMOTION_RATIO),
               slowmotion_ratio,
               "%.1fx",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 1, 10, 1.0, true, true);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SLOWMOTION_RATIO);

         CONFIG_BOOL(
               list, list_info,
               &settings->menu.throttle_framerate,
               msg_hash_to_str(MENU_ENUM_LABEL_MENU_THROTTLE_FRAMERATE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_ENUM_THROTTLE_FRAMERATE),
               true,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_MENU_THROTTLE_FRAMERATE);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_FONT:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ONSCREEN_DISPLAY_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_ONSCREEN_DISPLAY_SETTINGS);

         START_SUB_GROUP(list, list_info, "Messages",
               &group_info,
               &subgroup_info,
               parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->video.font_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_FONT_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_FONT_ENABLE),
               font_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_FONT_ENABLE);

         CONFIG_PATH(
               list, list_info,
               settings->path.font,
               sizeof(settings->path.font),
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_FONT_PATH),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_FONT_PATH),
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_FONT_PATH);

         CONFIG_FLOAT(
               list, list_info,
               &settings->video.font_size,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_FONT_SIZE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_FONT_SIZE),
               font_size,
               "%.1f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 1.00, 100.00, 1.0, true, true);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_FONT_SIZE);

         CONFIG_FLOAT(
               list, list_info,
               &settings->video.msg_pos_x,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_X),
               message_pos_offset_x,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 0, 1, 0.01, true, true);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_X);

         CONFIG_FLOAT(
               list, list_info,
               &settings->video.msg_pos_y,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_MESSAGE_POS_Y),
               message_pos_offset_y,
               "%.3f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 0, 1, 0.01, true, true);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_MESSAGE_POS_Y);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_OVERLAY:
#ifdef HAVE_OVERLAY
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OVERLAY_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_OVERLAY_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->input.overlay_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_ENABLE),
               config_overlay_enable_default(),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         (*list)[list_info->index - 1].change_handler = overlay_enable_toggle_change_handler;
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE);

         CONFIG_BOOL(
               list, list_info,
               &settings->input.overlay_enable_autopreferred,
               msg_hash_to_str(MENU_ENUM_LABEL_OVERLAY_AUTOLOAD_PREFERRED),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OVERLAY_AUTOLOAD_PREFERRED),
               true,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         (*list)[list_info->index - 1].change_handler = overlay_enable_toggle_change_handler;
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_OVERLAY_AUTOLOAD_PREFERRED);

         CONFIG_BOOL(
               list, list_info,
               &settings->input.overlay_hide_in_menu,
               msg_hash_to_str(MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_OVERLAY_HIDE_IN_MENU),
               overlay_hide_in_menu,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         (*list)[list_info->index - 1].change_handler = overlay_enable_toggle_change_handler;
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_OVERLAY_HIDE_IN_MENU);

         CONFIG_BOOL(
               list, list_info,
               &settings->osk.enable,
               msg_hash_to_str(MENU_ENUM_LABEL_INPUT_OSK_OVERLAY_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_OSK_OVERLAY_ENABLE),
               true,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_OVERLAY_ENABLE);

         CONFIG_PATH(
               list, list_info,
               settings->path.overlay,
               sizeof(settings->path.overlay),
               msg_hash_to_str(MENU_ENUM_LABEL_OVERLAY_PRESET),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OVERLAY_PRESET),
               settings->directory.overlay,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_values(list, list_info, "cfg");
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_OVERLAY_INIT);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_OVERLAY_PRESET);

         CONFIG_FLOAT(
               list, list_info,
               &settings->input.overlay_opacity,
               msg_hash_to_str(MENU_ENUM_LABEL_OVERLAY_OPACITY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OVERLAY_OPACITY),
               0.7f,
               "%.2f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_OVERLAY_SET_ALPHA_MOD);
         menu_settings_list_current_add_range(list, list_info, 0, 1, 0.01, true, true);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_OVERLAY_OPACITY);

         CONFIG_FLOAT(
               list, list_info,
               &settings->input.overlay_scale,
               msg_hash_to_str(MENU_ENUM_LABEL_OVERLAY_SCALE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OVERLAY_SCALE),
               1.0f,
               "%.2f",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_OVERLAY_SET_SCALE_FACTOR);
         menu_settings_list_current_add_range(list, list_info, 0, 2, 0.01, true, true);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_OVERLAY_SCALE);

         END_SUB_GROUP(list, list_info, parent_group);

         START_SUB_GROUP(list, list_info, "Onscreen Keyboard Overlay", &group_info, &subgroup_info, parent_group);

         CONFIG_PATH(
               list, list_info,
               settings->path.osk_overlay,
               sizeof(settings->path.osk_overlay),
               msg_hash_to_str(MENU_ENUM_LABEL_KEYBOARD_OVERLAY_PRESET),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_KEYBOARD_OVERLAY_PRESET),
               dir_get_ptr(RARCH_DIR_OSK_OVERLAY),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_values(list, list_info, "cfg");
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_KEYBOARD_OVERLAY_PRESET);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
#endif
         break;
      case SETTINGS_LIST_MENU:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_SETTINGS),
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_MENU_SETTINGS);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_MENU_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         if (!string_is_equal(settings->menu.driver, "rgui"))
         {
            CONFIG_PATH(
                  list, list_info,
                  settings->path.menu_wallpaper,
                  sizeof(settings->path.menu_wallpaper),
                  msg_hash_to_str(MENU_ENUM_LABEL_MENU_WALLPAPER),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER),
                  "",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_values(list, list_info, "png");
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_MENU_WALLPAPER);

            CONFIG_FLOAT(
                  list, list_info,
                  &settings->menu.wallpaper.opacity,
                  msg_hash_to_str(MENU_ENUM_LABEL_MENU_WALLPAPER_OPACITY),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_WALLPAPER_OPACITY),
                  menu_wallpaper_opacity,
                  "%.3f",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0.0, 1.0, 0.010, true, true);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_MENU_WALLPAPER_OPACITY);
         }


         if (string_is_equal(settings->menu.driver, "xmb"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->menu.dynamic_wallpaper_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_DYNAMIC_WALLPAPER),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPER),
                  true,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE
                  );
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_DYNAMIC_WALLPAPER);
         }


         CONFIG_BOOL(
               list, list_info,
               &settings->menu.pause_libretro,
               msg_hash_to_str(MENU_ENUM_LABEL_PAUSE_LIBRETRO),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PAUSE_LIBRETRO),
               true,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_CMD_APPLY_AUTO
               );
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_MENU_PAUSE_LIBRETRO);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_PAUSE_LIBRETRO);

         CONFIG_BOOL(
               list, list_info,
               &settings->menu.mouse.enable,
               msg_hash_to_str(MENU_ENUM_LABEL_MOUSE_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MOUSE_ENABLE),
               def_mouse_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_MOUSE_ENABLE);

         CONFIG_BOOL(
               list, list_info,
               &settings->menu.pointer.enable,
               msg_hash_to_str(MENU_ENUM_LABEL_POINTER_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_POINTER_ENABLE),
               pointer_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_POINTER_ENABLE);

         CONFIG_BOOL(
               list, list_info,
               &settings->menu.linear_filter,
               msg_hash_to_str(MENU_ENUM_LABEL_MENU_LINEAR_FILTER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_LINEAR_FILTER),
               true,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_MENU_LINEAR_FILTER);

#ifdef RARCH_MOBILE
         /* We don't want mobile users being able to switch this off. */
         (*list)[list_info->index - 1].action_left   = NULL;
         (*list)[list_info->index - 1].action_right  = NULL;
         (*list)[list_info->index - 1].action_start  = NULL;
#endif

         END_SUB_GROUP(list, list_info, parent_group);

         START_SUB_GROUP(list, list_info, "Navigation", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->menu.navigation.wraparound.enable,
               msg_hash_to_str(MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NAVIGATION_WRAPAROUND),
               true,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_NAVIGATION_WRAPAROUND);

         END_SUB_GROUP(list, list_info, parent_group);
         START_SUB_GROUP(list, list_info, "Settings View", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->menu.show_advanced_settings,
               msg_hash_to_str(MENU_ENUM_LABEL_SHOW_ADVANCED_SETTINGS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SHOW_ADVANCED_SETTINGS),
               show_advanced_settings,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SHOW_ADVANCED_SETTINGS);

#ifdef HAVE_THREADS
         CONFIG_BOOL(
               list, list_info,
               &settings->threaded_data_runloop_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_THREADED_DATA_RUNLOOP_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_THREADED_DATA_RUNLOOP_ENABLE),
               threaded_data_runloop_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_ADVANCED
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_THREADED_DATA_RUNLOOP_ENABLE);
#endif

#if 0
         /* These colors are hints. The menu driver is not required to use them. */
         CONFIG_HEX(
               list, list_info,
               &settings->menu.entry_normal_color,
               msg_hash_to_str(MENU_ENUM_LABEL_ENTRY_NORMAL_COLOR),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ENTRY_NORMAL_COLOR),
               menu_entry_normal_color,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_ENTRY_NORMAL_COLOR);

         CONFIG_HEX(
               list, list_info,
               &settings->menu.entry_hover_color,
               msg_hash_to_str(MENU_ENUM_LABEL_ENTRY_HOVER_COLOR),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ENTRY_HOVER_COLOR),
               menu_entry_hover_color,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_ENTRY_HOVER_COLOR);

         CONFIG_HEX(
               list, list_info,
               &settings->menu.title_color,
               msg_hash_to_str(MENU_ENUM_LABEL_TITLE_COLOR),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TITLE_COLOR),
               menu_title_color,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_TITLE_COLOR);
#endif

         END_SUB_GROUP(list, list_info, parent_group);

         START_SUB_GROUP(list, list_info, "Display", &group_info, &subgroup_info, parent_group);

         /* only GLUI uses these values, don't show them on other drivers */
         if (string_is_equal(settings->menu.driver, "glui"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->menu.dpi.override_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_DPI_OVERRIDE_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_ENABLE),
                  menu_dpi_override_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_DPI_OVERRIDE_ENABLE);

            CONFIG_UINT(
                  list, list_info,
                  &settings->menu.dpi.override_value,
                  msg_hash_to_str(MENU_ENUM_LABEL_DPI_OVERRIDE_VALUE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DPI_OVERRIDE_VALUE),
                  menu_dpi_override_value,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 72, 999, 1, true, true);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_DPI_OVERRIDE_VALUE);
         }

#ifdef HAVE_XMB
         /* only XMB uses these values, don't show them on other drivers */
         if (string_is_equal(settings->menu.driver, "xmb"))
         {
            CONFIG_UINT(
                  list, list_info,
                  &settings->menu.xmb.alpha_factor,
                  msg_hash_to_str(MENU_ENUM_LABEL_XMB_ALPHA_FACTOR),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_ALPHA_FACTOR),
                  xmb_alpha_factor,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, 100, 1, true, true);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_XMB_ALPHA_FACTOR);

            CONFIG_UINT(
                  list, list_info,
                  &settings->menu.xmb.scale_factor,
                  msg_hash_to_str(MENU_ENUM_LABEL_XMB_SCALE_FACTOR),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_SCALE_FACTOR),
                  xmb_scale_factor,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, 100, 1, true, true);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_XMB_SCALE_FACTOR);

            CONFIG_PATH(
                  list, list_info,
                  settings->menu.xmb.font,
                  sizeof(settings->menu.xmb.font),
                  msg_hash_to_str(MENU_ENUM_LABEL_XMB_FONT),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_FONT),
                  settings->menu.xmb.font,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_XMB_FONT);

            CONFIG_UINT(
                  list, list_info,
                  &settings->menu.xmb.theme,
                  msg_hash_to_str(MENU_ENUM_LABEL_XMB_THEME),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_THEME),
                  xmb_icon_theme,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, 4, 1, true, true);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_XMB_THEME);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->menu.xmb.shadows_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_XMB_SHADOWS_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_SHADOWS_ENABLE),
                  xmb_shadows_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_XMB_SHADOWS_ENABLE);

#ifdef HAVE_SHADERPIPELINE
            CONFIG_UINT(
                  list, list_info,
                  &settings->menu.xmb.shader_pipeline,
                  msg_hash_to_str(MENU_ENUM_LABEL_XMB_RIBBON_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_RIBBON_ENABLE),
                  menu_shader_pipeline,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, XMB_SHADER_PIPELINE_LAST-1, 1, true, true);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_XMB_RIBBON_ENABLE);
#endif

            CONFIG_UINT(
                  list, list_info,
                  &settings->menu.xmb.menu_color_theme,
                  msg_hash_to_str(MENU_ENUM_LABEL_XMB_MENU_COLOR_THEME),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_MENU_COLOR_THEME),
                  xmb_theme,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, XMB_THEME_LAST-1, 1, true, true);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_XMB_MENU_COLOR_THEME);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->menu.xmb.show_settings,
                  msg_hash_to_str(MENU_ENUM_LABEL_XMB_SHOW_SETTINGS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_SHOW_SETTINGS),
                  xmb_show_settings,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_XMB_SHOW_SETTINGS);

#ifdef HAVE_IMAGEVIEWER
            CONFIG_BOOL(
                  list, list_info,
                  &settings->menu.xmb.show_images,
                  msg_hash_to_str(MENU_ENUM_LABEL_XMB_SHOW_IMAGES),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_SHOW_IMAGES),
                  xmb_show_images,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_XMB_SHOW_IMAGES);
#endif

#ifdef HAVE_FFMPEG
            CONFIG_BOOL(
                  list, list_info,
                  &settings->menu.xmb.show_music,
                  msg_hash_to_str(MENU_ENUM_LABEL_XMB_SHOW_MUSIC),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_SHOW_MUSIC),
                  xmb_show_music,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_XMB_SHOW_MUSIC);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->menu.xmb.show_video,
                  msg_hash_to_str(MENU_ENUM_LABEL_XMB_SHOW_VIDEO),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_SHOW_VIDEO),
                  xmb_show_video,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_XMB_SHOW_VIDEO);
#endif

            CONFIG_BOOL(
                  list, list_info,
                  &settings->menu.xmb.show_history,
                  msg_hash_to_str(MENU_ENUM_LABEL_XMB_SHOW_HISTORY),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_XMB_SHOW_HISTORY),
                  xmb_show_history,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_XMB_SHOW_HISTORY);
         }

#endif

#ifdef HAVE_MATERIALUI
         /* only MaterialUI uses these values, don't show them on other drivers */
         if (string_is_equal(settings->menu.driver, "glui"))
         {
            CONFIG_UINT(
                  list, list_info,
                  &settings->menu.materialui.menu_color_theme,
                  msg_hash_to_str(MENU_ENUM_LABEL_MATERIALUI_MENU_COLOR_THEME),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_COLOR_THEME),
                  MATERIALUI_THEME_BLUE,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, MATERIALUI_THEME_LAST-1, 1, true, true);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_MATERIALUI_MENU_COLOR_THEME);

            CONFIG_FLOAT(
                  list, list_info,
                  &settings->menu.header.opacity,
                  msg_hash_to_str(MENU_ENUM_LABEL_MATERIALUI_MENU_HEADER_OPACITY),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_HEADER_OPACITY),
                  menu_header_opacity,
                  "%.3f",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0.0, 1.0, 0.010, true, true);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_MATERIALUI_MENU_HEADER_OPACITY);

            CONFIG_FLOAT(
                  list, list_info,
                  &settings->menu.footer.opacity,
                  msg_hash_to_str(MENU_ENUM_LABEL_MATERIALUI_MENU_FOOTER_OPACITY),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MATERIALUI_MENU_FOOTER_OPACITY),
                  menu_footer_opacity,
                  "%.3f",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0.0, 1.0, 0.010, true, true);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_MATERIALUI_MENU_FOOTER_OPACITY);
         }
#endif

         CONFIG_BOOL(
               list, list_info,
               &settings->menu_show_start_screen,
               msg_hash_to_str(MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RGUI_SHOW_START_SCREEN),
               default_menu_show_start_screen,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_RGUI_SHOW_START_SCREEN);

         if (string_is_equal(settings->menu.driver, "xmb"))
         {
            CONFIG_UINT(
                  list, list_info,
                  &settings->menu.thumbnails,
                  msg_hash_to_str(MENU_ENUM_LABEL_THUMBNAILS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_THUMBNAILS),
                  menu_thumbnails_default,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, 3, 1, true, true);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_THUMBNAILS);
         }

         CONFIG_BOOL(
               list, list_info,
               &settings->menu.timedate_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_TIMEDATE_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_TIMEDATE_ENABLE),
               true,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_TIMEDATE_ENABLE);

         CONFIG_BOOL(
               list, list_info,
               &settings->menu.core_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_CORE_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_ENABLE),
               true,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CORE_ENABLE);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_MENU_FILE_BROWSER:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_FILE_BROWSER_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_MENU_FILE_BROWSER_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->menu.navigation.browser.filter.supported_extensions_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE),
               true,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_NAVIGATION_BROWSER_FILTER_SUPPORTED_EXTENSIONS_ENABLE);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_MULTIMEDIA:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MULTIMEDIA_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         if (!string_is_equal(settings->record.driver, "null"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->multimedia.builtin_mediaplayer_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_USE_BUILTIN_PLAYER),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USE_BUILTIN_PLAYER),
                  true,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_USE_BUILTIN_PLAYER);
         }


#ifdef HAVE_IMAGEVIEWER
         CONFIG_BOOL(
               list, list_info,
               &settings->multimedia.builtin_imageviewer_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_USE_BUILTIN_IMAGE_VIEWER),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USE_BUILTIN_IMAGE_VIEWER),
               true,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_USE_BUILTIN_IMAGE_VIEWER);
#endif

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_USER_INTERFACE:
#ifndef HAVE_LAKKA
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER_INTERFACE_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_USER_INTERFACE_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->pause_nonactive,
               msg_hash_to_str(MENU_ENUM_LABEL_PAUSE_NONACTIVE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PAUSE_NONACTIVE),
               pause_nonactive,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_PAUSE_NONACTIVE);

#if !defined(RARCH_MOBILE)
         CONFIG_BOOL(
               list, list_info,
               &settings->video.disable_composition,
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_DISABLE_COMPOSITION),
               disable_composition,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_CMD_APPLY_AUTO);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_REINIT);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_DISABLE_COMPOSITION);
#endif

         if (!string_is_equal(ui_companion_driver_get_ident(), "null"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->ui.companion_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_UI_COMPANION_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UI_COMPANION_ENABLE),
                  ui_companion_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_UI_COMPANION_ENABLE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->ui.companion_start_on_boot,
                  msg_hash_to_str(MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UI_COMPANION_START_ON_BOOT),
                  ui_companion_start_on_boot,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_UI_COMPANION_START_ON_BOOT);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->ui.menubar_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_UI_MENUBAR_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_UI_MENUBAR_ENABLE),
                  true,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_UI_MENUBAR_ENABLE);
         }


         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
#endif
         break;
      case SETTINGS_LIST_PLAYLIST:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_SETTINGS_BEGIN),
               parent_group);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "History", &group_info, &subgroup_info, parent_group);

#ifndef HAVE_LAKKA
         CONFIG_BOOL(
               list, list_info,
               &settings->history_list_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_HISTORY_LIST_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HISTORY_LIST_ENABLE),
               true,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_HISTORY_LIST_ENABLE);
#endif

         CONFIG_UINT(
               list, list_info,
               &settings->content_history_size,
               msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_HISTORY_SIZE),
               default_content_history_size,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(list, list_info, 0, 0, 1.0, true, false);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CONTENT_HISTORY_SIZE);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_CHEEVOS:
#ifdef HAVE_CHEEVOS
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEEVOS_SETTINGS),
               parent_group);
         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_RETRO_ACHIEVEMENTS_SETTINGS);
         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_BOOL(
               list, list_info,
               &settings->cheevos.enable,
               msg_hash_to_str(MENU_ENUM_LABEL_CHEEVOS_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ENABLE),
               cheevos_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CHEEVOS_ENABLE);

         CONFIG_BOOL(
               list, list_info,
               &settings->cheevos.test_unofficial,
               msg_hash_to_str(MENU_ENUM_LABEL_CHEEVOS_TEST_UNOFFICIAL),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEEVOS_TEST_UNOFFICIAL),
               true,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CHEEVOS_TEST_UNOFFICIAL);

         CONFIG_BOOL(
               list, list_info,
               &settings->cheevos.hardcore_mode_enable,
               msg_hash_to_str(MENU_ENUM_LABEL_CHEEVOS_HARDCORE_MODE_ENABLE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEEVOS_HARDCORE_MODE_ENABLE),
               false,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_CHEEVOS_HARDCORE_MODE_TOGGLE);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CHEEVOS_HARDCORE_MODE_ENABLE);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
#endif
         break;
      case SETTINGS_LIST_CORE_UPDATER:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_SETTINGS),
               parent_group);
         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_UPDATER_SETTINGS);
         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);
#ifdef HAVE_NETWORKING

         CONFIG_STRING(
               list, list_info,
               settings->network.buildbot_url,
               sizeof(settings->network.buildbot_url),
               msg_hash_to_str(MENU_ENUM_LABEL_CORE_UPDATER_BUILDBOT_URL),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_BUILDBOT_URL),
               buildbot_server_url, 
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CORE_UPDATER_BUILDBOT_URL);

         CONFIG_STRING(
               list, list_info,
               settings->network.buildbot_assets_url,
               sizeof(settings->network.buildbot_assets_url),
               msg_hash_to_str(MENU_ENUM_LABEL_BUILDBOT_ASSETS_URL),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BUILDBOT_ASSETS_URL),
               buildbot_assets_server_url, 
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_BUILDBOT_ASSETS_URL);

         CONFIG_BOOL(
               list, list_info,
               &settings->network.buildbot_auto_extract_archive,
               msg_hash_to_str(MENU_ENUM_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE),
               true,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler,
               SD_FLAG_NONE
               );
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CORE_UPDATER_AUTO_EXTRACT_ARCHIVE);
#endif
         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_NETPLAY:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETWORK_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_SETTINGS);

         START_SUB_GROUP(list, list_info, "Netplay", &group_info, &subgroup_info, parent_group);

         {
#if defined(HAVE_NETWORKING)
#if defined(HAVE_NETWORK_CMD)
            unsigned user;
#endif
            CONFIG_STRING(
                  list, list_info,
                  settings->netplay.server,
                  sizeof(settings->netplay.server),
                  msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_IP_ADDRESS),
                  "",
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_NETPLAY_IP_ADDRESS);

            CONFIG_UINT(
                  list, list_info,
                  &settings->netplay.port,
                  msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_TCP_UDP_PORT),
                  RARCH_DEFAULT_PORT,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, 65535, 1, true, true);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_NETPLAY_TCP_UDP_PORT);

            CONFIG_UINT(
                  list, list_info,
                  &settings->netplay.sync_frames,
                  msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_DELAY_FRAMES),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_DELAY_FRAMES),
                  0,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, 10, 1, true, false);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_NETPLAY_DELAY_FRAMES);

            CONFIG_UINT(
                  list, list_info,
                  &settings->netplay.check_frames,
                  msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_CHECK_FRAMES),
                  0,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_range(list, list_info, 0, 10, 1, true, false);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_NETPLAY_CHECK_FRAMES);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->netplay.is_spectate,
                  msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_SPECTATOR_MODE_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_SPECTATOR_MODE_ENABLE),
                  false,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_NETPLAY_SPECTATOR_MODE_ENABLE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->netplay.swap_input,
                  msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_CLIENT_SWAP_INPUT),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_CLIENT_SWAP_INPUT),
                  netplay_client_swap_input,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_NETPLAY_CLIENT_SWAP_INPUT);

            END_SUB_GROUP(list, list_info, parent_group);

            START_SUB_GROUP(
                  list,
                  list_info,
                  "Miscellaneous",
                  &group_info,
                  &subgroup_info,
                  parent_group);


#if defined(HAVE_NETWORK_CMD)
            CONFIG_BOOL(
                  list, list_info,
                  &settings->network_cmd_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_CMD_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETWORK_CMD_ENABLE),
                  network_cmd_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_NETWORK_CMD_ENABLE);

            CONFIG_UINT(
                  list, list_info,
                  &settings->network_cmd_port,
                  msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_CMD_PORT),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETWORK_CMD_PORT),
                  network_cmd_port,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  NULL,
                  NULL);
            menu_settings_list_current_add_range(list, list_info, 1, 99999, 1, true, true);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_NETWORK_CMD_PORT);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->network_remote_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_REMOTE_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_ENABLE),
                  false,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_NETWORK_REMOTE_ENABLE);

            CONFIG_UINT(
                  list, list_info,
                  &settings->network_remote_base_port,
                  msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_REMOTE_PORT),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETWORK_REMOTE_PORT),
                  network_remote_base_port,
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  NULL,
                  NULL);
            menu_settings_list_current_add_range(list, list_info, 1, 99999, 1, true, true);
            settings_data_list_current_add_flags(list, list_info, SD_FLAG_ADVANCED);
            /* TODO/FIXME - add enum_idx */

            for(user = 0; user < settings->input.max_users; user++)
            {
               char s1[64], s2[64];

               snprintf(s1, sizeof(s1), "%s_user_p%d", msg_hash_to_str(MENU_ENUM_LABEL_NETWORK_REMOTE_ENABLE), user + 1);
               snprintf(s2, sizeof(s2), "User %d Remote Enable", user + 1);


               CONFIG_BOOL(
                     list, list_info,
                     &settings->network_remote_enable_user[user],
                     /* todo: figure out this value, it's working fine but I don't think this is correct */
                     strdup(s1),
                     strdup(s2),
                     false,
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                     msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                     &group_info,
                     &subgroup_info,
                     parent_group,
                     general_write_handler,
                     general_read_handler,
                     SD_FLAG_ADVANCED);
               settings_data_list_current_add_free_flags(list, list_info, SD_FREE_FLAG_NAME | SD_FREE_FLAG_SHORT);
               menu_settings_list_current_add_enum_idx(list, list_info, (enum msg_hash_enums)(MENU_ENUM_LABEL_NETWORK_REMOTE_USER_1_ENABLE + user));
            }

            CONFIG_BOOL(
                  list, list_info,
                  &settings->stdin_cmd_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_STDIN_CMD_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_STDIN_CMD_ENABLE),
                  stdin_cmd_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_ADVANCED);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_STDIN_CMD_ENABLE);
#endif
#endif
         }
         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_LAKKA_SERVICES:
         {
#if defined(HAVE_LAKKA)
            START_GROUP(list, list_info, &group_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES),
                  parent_group);

            parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

            START_SUB_GROUP(list, list_info,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LAKKA_SERVICES),
                  &group_info, &subgroup_info, parent_group);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->ssh_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_SSH_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SSH_ENABLE),
                  true,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].change_handler = ssh_enable_toggle_change_handler;
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SSH_ENABLE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->samba_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_SAMBA_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAMBA_ENABLE),
                  true,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].change_handler = samba_enable_toggle_change_handler;
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SAMBA_ENABLE);

            CONFIG_BOOL(
                  list, list_info,
                  &settings->bluetooth_enable,
                  msg_hash_to_str(MENU_ENUM_LABEL_BLUETOOTH_ENABLE),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BLUETOOTH_ENABLE),
                  true,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            (*list)[list_info->index - 1].change_handler = bluetooth_enable_toggle_change_handler;
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_BLUETOOTH_ENABLE);

            END_SUB_GROUP(list, list_info, parent_group);
            END_GROUP(list, list_info, parent_group);
#endif
         }
         break;
      case SETTINGS_LIST_USER:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_USER_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_ACCOUNTS_LIST),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_ACCOUNTS_LIST);

         CONFIG_STRING(
               list, list_info,
               settings->username,
               sizeof(settings->username),
               msg_hash_to_str(MENU_ENUM_LABEL_NETPLAY_NICKNAME),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NETPLAY_NICKNAME),
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_NETPLAY_NICKNAME);

#ifdef HAVE_LANGEXTRA
         CONFIG_UINT(
               list, list_info,
               &settings->user_language,
               msg_hash_to_str(MENU_ENUM_LABEL_USER_LANGUAGE),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_USER_LANGUAGE),
               def_user_language,
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_range(
               list,
               list_info,
               0,
               RETRO_LANGUAGE_LAST-1,
               1,
               true,
               true);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_MENU_REFRESH);
         (*list)[list_info->index - 1].get_string_representation = 
            &setting_get_string_representation_uint_user_language;
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_USER_LANGUAGE);
#endif

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_USER_ACCOUNTS:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_LIST_END),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

#ifdef HAVE_CHEEVOS
         CONFIG_ACTION(
               list, list_info,
               msg_hash_to_str(MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_RETRO_ACHIEVEMENTS),
               &group_info,
               &subgroup_info,
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_ACCOUNTS_RETRO_ACHIEVEMENTS);
#endif

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_USER_ACCOUNTS_CHEEVOS:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_SETTINGS),
               parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

#ifdef HAVE_CHEEVOS
         CONFIG_STRING(
               list, list_info,
               settings->cheevos.username,
               sizeof(settings->cheevos.username),
               msg_hash_to_str(MENU_ENUM_LABEL_CHEEVOS_USERNAME),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_USERNAME),
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CHEEVOS_USERNAME);

         CONFIG_STRING(
               list, list_info,
               settings->cheevos.password,
               sizeof(settings->cheevos.password),
               msg_hash_to_str(MENU_ENUM_LABEL_CHEEVOS_PASSWORD),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ACCOUNTS_CHEEVOS_PASSWORD),
               "",
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         (*list)[list_info->index - 1].get_string_representation = 
            &setting_get_string_representation_cheevos_password;
         settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CHEEVOS_PASSWORD);
#endif

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_DIRECTORY:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_SETTINGS),
               parent_group);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_DIRECTORY_SETTINGS);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_DIRECTORY_SETTINGS);

         START_SUB_GROUP(list, list_info, "State", &group_info, &subgroup_info, parent_group);

         CONFIG_DIR(
               list, list_info,
               settings->directory.system,
               sizeof(settings->directory.system),
               msg_hash_to_str(MENU_ENUM_LABEL_SYSTEM_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SYSTEM_DIRECTORY),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SYSTEM_DIRECTORY);

         CONFIG_DIR(
               list, list_info,
               settings->directory.core_assets,
               sizeof(settings->directory.core_assets),
               msg_hash_to_str(MENU_ENUM_LABEL_CORE_ASSETS_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CORE_ASSETS_DIRECTORY),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CORE_ASSETS_DIRECTORY);

         CONFIG_DIR(
               list, list_info,
               settings->directory.assets,
               sizeof(settings->directory.assets),
               msg_hash_to_str(MENU_ENUM_LABEL_ASSETS_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ASSETS_DIRECTORY),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_ASSETS_DIRECTORY);

         CONFIG_DIR(
               list, list_info,
               settings->directory.dynamic_wallpapers,
               sizeof(settings->directory.dynamic_wallpapers),
               msg_hash_to_str(MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DYNAMIC_WALLPAPERS_DIRECTORY),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_DYNAMIC_WALLPAPERS_DIRECTORY);

         CONFIG_DIR(
               list, list_info,
               settings->directory.thumbnails,
               sizeof(settings->directory.thumbnails),
               msg_hash_to_str(MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_THUMBNAILS_DIRECTORY),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_THUMBNAILS_DIRECTORY);

         CONFIG_DIR(
               list, list_info,
               settings->directory.menu_content,
               sizeof(settings->directory.menu_content),
               msg_hash_to_str(MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RGUI_BROWSER_DIRECTORY),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_RGUI_BROWSER_DIRECTORY);

         CONFIG_DIR(
               list, list_info,
               settings->directory.menu_config,
               sizeof(settings->directory.menu_config),
               msg_hash_to_str(MENU_ENUM_LABEL_RGUI_CONFIG_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RGUI_CONFIG_DIRECTORY),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_RGUI_CONFIG_DIRECTORY);


         CONFIG_DIR(
               list, list_info,
               settings->directory.libretro,
               sizeof(settings->directory.libretro),
               msg_hash_to_str(MENU_ENUM_LABEL_LIBRETRO_DIR_PATH),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_DIR_PATH),
               g_defaults.dir.core,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_CORE_INFO_INIT);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_LIBRETRO_DIR_PATH);

         CONFIG_DIR(
               list, list_info,
               settings->path.libretro_info,
               sizeof(settings->path.libretro_info),
               msg_hash_to_str(MENU_ENUM_LABEL_LIBRETRO_INFO_PATH),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LIBRETRO_INFO_PATH),
               g_defaults.dir.core_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_cmd(list, list_info, CMD_EVENT_CORE_INFO_INIT);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_LIBRETRO_INFO_PATH);

#ifdef HAVE_LIBRETRODB
         CONFIG_DIR(
               list, list_info,
               settings->path.content_database,
               sizeof(settings->path.content_database),
               msg_hash_to_str(MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_DATABASE_DIRECTORY),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CONTENT_DATABASE_DIRECTORY);

         CONFIG_DIR(
               list, list_info,
               settings->directory.cursor,
               sizeof(settings->directory.cursor),
               msg_hash_to_str(MENU_ENUM_LABEL_CURSOR_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CURSOR_DIRECTORY),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CURSOR_DIRECTORY);
#endif

         CONFIG_DIR(
               list, list_info,
               settings->path.cheat_database,
               sizeof(settings->path.cheat_database),
               msg_hash_to_str(MENU_ENUM_LABEL_CHEAT_DATABASE_PATH),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CHEAT_DATABASE_PATH),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CHEAT_DATABASE_PATH);

         CONFIG_DIR(
               list, list_info,
               settings->directory.video_filter,
               sizeof(settings->directory.video_filter),
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_FILTER_DIR),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_DIR),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_FILTER_DIR);

         CONFIG_DIR(
               list, list_info,
               settings->directory.audio_filter,
               sizeof(settings->directory.audio_filter),
               msg_hash_to_str(MENU_ENUM_LABEL_AUDIO_FILTER_DIR),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_AUDIO_FILTER_DIR),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_AUDIO_FILTER_DIR);

         CONFIG_DIR(
               list, list_info,
               settings->directory.video_shader,
               sizeof(settings->directory.video_shader),
               msg_hash_to_str(MENU_ENUM_LABEL_VIDEO_SHADER_DIR),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_SHADER_DIR),
               g_defaults.dir.shader,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_VIDEO_SHADER_DIR);

         if (!string_is_equal(settings->record.driver, "null"))
         {
            CONFIG_DIR(
                  list, list_info,
                  global->record.output_dir,
                  sizeof(global->record.output_dir),
                  msg_hash_to_str(MENU_ENUM_LABEL_RECORDING_OUTPUT_DIRECTORY),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RECORDING_OUTPUT_DIRECTORY),
                  "",
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_RECORDING_OUTPUT_DIRECTORY);

            CONFIG_DIR(
                  list, list_info,
                  global->record.config_dir,
                  sizeof(global->record.config_dir),
                  msg_hash_to_str(MENU_ENUM_LABEL_RECORDING_CONFIG_DIRECTORY),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RECORDING_CONFIG_DIRECTORY),
                  "",
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_RECORDING_CONFIG_DIRECTORY);
         }
#ifdef HAVE_OVERLAY
         CONFIG_DIR(
               list, list_info,
               settings->directory.overlay,
               sizeof(settings->directory.overlay),
               msg_hash_to_str(MENU_ENUM_LABEL_OVERLAY_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OVERLAY_DIRECTORY),
               g_defaults.dir.overlay,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_OVERLAY_DIRECTORY);

         CONFIG_DIR(
               list, list_info,
               dir_get_ptr(RARCH_DIR_OSK_OVERLAY),
               dir_get_size(RARCH_DIR_OSK_OVERLAY),
               msg_hash_to_str(MENU_ENUM_LABEL_OSK_OVERLAY_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OSK_OVERLAY_DIRECTORY),
               g_defaults.dir.osk_overlay,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_OSK_OVERLAY_DIRECTORY);
#endif

         CONFIG_DIR(
               list, list_info,
               settings->directory.screenshot,
               sizeof(settings->directory.screenshot),
               msg_hash_to_str(MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SCREENSHOT_DIRECTORY),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SCREENSHOT_DIRECTORY);

         CONFIG_DIR(
               list, list_info,
               settings->directory.autoconfig,
               sizeof(settings->directory.autoconfig),
               msg_hash_to_str(MENU_ENUM_LABEL_JOYPAD_AUTOCONFIG_DIR),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_JOYPAD_AUTOCONFIG_DIR),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_JOYPAD_AUTOCONFIG_DIR);

         CONFIG_DIR(
               list, list_info,
               settings->directory.input_remapping,
               sizeof(settings->directory.input_remapping),
               msg_hash_to_str(MENU_ENUM_LABEL_INPUT_REMAPPING_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_INPUT_REMAPPING_DIRECTORY),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_INPUT_REMAPPING_DIRECTORY);

         CONFIG_DIR(
               list, list_info,
               settings->directory.playlist,
               sizeof(settings->directory.playlist),
               msg_hash_to_str(MENU_ENUM_LABEL_PLAYLIST_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLIST_DIRECTORY),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_DEFAULT),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_PLAYLIST_DIRECTORY);

         CONFIG_DIR(
               list, list_info,
               dir_get_ptr(RARCH_DIR_SAVEFILE),
               dir_get_size(RARCH_DIR_SAVEFILE),
               msg_hash_to_str(MENU_ENUM_LABEL_SAVEFILE_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVEFILE_DIRECTORY),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SAVEFILE_DIRECTORY);

         CONFIG_DIR(
               list, list_info,
               dir_get_ptr(RARCH_DIR_SAVESTATE),
               dir_get_size(RARCH_DIR_SAVESTATE),
               msg_hash_to_str(MENU_ENUM_LABEL_SAVESTATE_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVESTATE_DIRECTORY),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_CONTENT),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_SAVESTATE_DIRECTORY);

         CONFIG_DIR(
               list, list_info,
               settings->directory.cache,
               sizeof(settings->directory.cache),
               msg_hash_to_str(MENU_ENUM_LABEL_CACHE_DIRECTORY),
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CACHE_DIRECTORY),
               "",
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_DIRECTORY_NONE),
               &group_info,
               &subgroup_info,
               parent_group,
               general_write_handler,
               general_read_handler);
         menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CACHE_DIRECTORY);

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_PRIVACY:
         START_GROUP(list, list_info, &group_info,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PRIVACY_SETTINGS), parent_group);

         parent_group = msg_hash_to_str(MENU_ENUM_LABEL_PRIVACY_SETTINGS);

         START_SUB_GROUP(list, list_info, "State",
               &group_info, &subgroup_info, parent_group);

         if (!string_is_equal(settings->camera.driver, "null"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->camera.allow,
                  msg_hash_to_str(MENU_ENUM_LABEL_CAMERA_ALLOW),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CAMERA_ALLOW),
                  false,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_CAMERA_ALLOW);
         }

         if (!string_is_equal(settings->location.driver, "null"))
         {
            CONFIG_BOOL(
                  list, list_info,
                  &settings->location.allow,
                  msg_hash_to_str(MENU_ENUM_LABEL_LOCATION_ALLOW),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_LOCATION_ALLOW),
                  false,
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_OFF),
                  msg_hash_to_str(MENU_ENUM_LABEL_VALUE_ON),
                  &group_info,
                  &subgroup_info,
                  parent_group,
                  general_write_handler,
                  general_read_handler,
                  SD_FLAG_NONE);
            menu_settings_list_current_add_enum_idx(list, list_info, MENU_ENUM_LABEL_LOCATION_ALLOW);
         }

         END_SUB_GROUP(list, list_info, parent_group);
         END_GROUP(list, list_info, parent_group);
         break;
      case SETTINGS_LIST_NONE:
      default:
         break;
   }


   return true;
}

bool menu_setting_free(void *data)
{
   unsigned values, n;
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return false;

   /* Free data which was previously tagged */
   for (; setting_get_type(setting) != ST_NONE; menu_settings_list_increment(&setting))
      for (values = setting->free_flags, n = 0; values != 0; values >>= 1, n++)
         if (values & 1)
            switch (1 << n)
            {
            case SD_FREE_FLAG_VALUES:
               free((void*)setting->values);
               setting->values = NULL;
               break;
            case SD_FREE_FLAG_NAME:
               free((void*)setting->name);
               setting->name = NULL;
               break;
            case SD_FREE_FLAG_SHORT:
               free((void*)setting->short_description);
               setting->short_description = NULL;
               break;
            default:
               break;
            }

   free(data);

   return true;
}

static rarch_setting_t *menu_setting_new_internal(rarch_setting_info_t *list_info)
{
   unsigned i;
   rarch_setting_t* resized_list        = NULL;
   enum settings_list_type list_types[] = 
   {
      SETTINGS_LIST_MAIN_MENU,
      SETTINGS_LIST_DRIVERS,
      SETTINGS_LIST_CORE,
      SETTINGS_LIST_CONFIGURATION,
      SETTINGS_LIST_LOGGING,
      SETTINGS_LIST_SAVING,
      SETTINGS_LIST_REWIND,
      SETTINGS_LIST_VIDEO,
      SETTINGS_LIST_AUDIO,
      SETTINGS_LIST_INPUT,
      SETTINGS_LIST_INPUT_HOTKEY,
      SETTINGS_LIST_RECORDING,
      SETTINGS_LIST_FRAME_THROTTLING,
      SETTINGS_LIST_FONT,
      SETTINGS_LIST_OVERLAY,
      SETTINGS_LIST_MENU,
      SETTINGS_LIST_MENU_FILE_BROWSER,
      SETTINGS_LIST_MULTIMEDIA,
      SETTINGS_LIST_USER_INTERFACE,
      SETTINGS_LIST_PLAYLIST,
      SETTINGS_LIST_CHEEVOS,
      SETTINGS_LIST_CORE_UPDATER,
      SETTINGS_LIST_NETPLAY,
      SETTINGS_LIST_LAKKA_SERVICES,
      SETTINGS_LIST_USER,
      SETTINGS_LIST_USER_ACCOUNTS,
      SETTINGS_LIST_USER_ACCOUNTS_CHEEVOS,
      SETTINGS_LIST_DIRECTORY,
      SETTINGS_LIST_PRIVACY
   };
   rarch_setting_t terminator           = setting_terminator_setting();
   const char *root                     = msg_hash_to_str(MENU_ENUM_LABEL_MAIN_MENU);
   rarch_setting_t *list                = (rarch_setting_t*)calloc(
         list_info->size, sizeof(*list));

   if (!list)
      goto error;

   for (i = 0; i < ARRAY_SIZE(list_types); i++)
   {
      if (!setting_append_list(list_types[i], &list, list_info, root))
         goto error;
   }

   if (!(settings_list_append(&list, list_info)))
      goto error;
   if (terminator.name)
      terminator.name_hash = msg_hash_calculate(terminator.name);
   (*&list)[list_info->index++] = terminator;

   /* flatten this array to save ourselves some kilobytes. */
   resized_list = (rarch_setting_t*)realloc(list,
         list_info->index * sizeof(rarch_setting_t));
   if (!resized_list)
      goto error;

   list = resized_list;

   return list;

error:
   if (list)
      free(list);
   return NULL;
}

/**
 * menu_setting_new:
 * @mask               : Bitmask of settings to include.
 *
 * Request a list of settings based on @mask.
 *
 * Returns: settings list composed of all requested
 * settings on success, otherwise NULL.
 **/
static rarch_setting_t *menu_setting_new(void)
{
   rarch_setting_t* list           = NULL;
   rarch_setting_info_t *list_info = (rarch_setting_info_t*)
      calloc(1, sizeof(*list_info));

   if (!list_info)
      return NULL;

   list_info->size  = 32;

   list = menu_setting_new_internal(list_info);

   menu_settings_info_list_free(list_info);
   list_info = NULL;

   return list;
}

bool menu_setting_ctl(enum menu_setting_ctl_state state, void *data)
{
   uint64_t flags;

   switch (state)
   {
      case MENU_SETTING_CTL_IS_OF_PATH_TYPE:
         {
            bool cbs_bound           = false;
            rarch_setting_t *setting = (rarch_setting_t*)data;

            if (!setting)
               return false;

            flags                    = setting_get_flags(setting);

            if (setting_get_type(setting) != ST_ACTION)
               return false;

            if (!setting->change_handler)
               return false;

            cbs_bound = setting->action_right;
            cbs_bound = cbs_bound || setting->action_left;
            cbs_bound = cbs_bound || setting->action_select;

            if (!cbs_bound)
               return false;

            if (!(flags & SD_FLAG_BROWSER_ACTION))
               return false;
         }
         break;
      case MENU_SETTING_CTL_NEW:
         {
            rarch_setting_t **setting = (rarch_setting_t**)data;
            if (!setting)
               return false;
            *setting = menu_setting_new();
         }
         break;
      case MENU_SETTING_CTL_ACTION_RIGHT:
         {
            rarch_setting_t *setting = (rarch_setting_t*)data;
            if (!setting)
               return false;

            if (setting_handler(setting, MENU_ACTION_RIGHT) == -1)
               return false;
         }
         break;
      case MENU_SETTING_CTL_NONE:
      default:
         break;
   }

   return true;
}
