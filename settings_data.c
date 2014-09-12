/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

#include "settings_data.h"
#include "file_path.h"
#include "gfx/shader_common.h"
#include "input/input_common.h"
#include "config.def.h"
#include "retroarch_logger.h"

#ifdef APPLE
#include "input/apple_keycode.h"
#endif

#if defined(__CELLOS_LV2__)
#include <sdk_version.h>

#if (CELL_SDK_VERSION > 0x340000)
#include <sysutil/sysutil_bgmplayback.h>
#endif

#endif

#ifdef HAVE_MENU
#include "frontend/menu/menu_entries.h"
#endif

static void get_input_config_prefix(char *buf, size_t sizeof_buf,
      const rarch_setting_t *setting)
{
   snprintf(buf, sizeof_buf, "input%cplayer%d",
         setting->index ? '_' : '\0', setting->index);
}

static void get_input_config_key(char *buf, size_t sizeof_buf,
      const rarch_setting_t* setting, const char* type)
{
   char prefix[32];
   get_input_config_prefix(prefix, sizeof(prefix), setting);
   snprintf(buf, sizeof_buf, "%s_%s%c%s", prefix, setting->name,
         type ? '_' : '\0', type);
}

#ifdef APPLE
/* FIXME - make portable */

static void get_key_name(char *buf, size_t sizeof_buf,
      const rarch_setting_t* setting)
{
   uint32_t hidkey, i;

   if (BINDFOR(*setting).key == RETROK_UNKNOWN)
      return;

   hidkey = input_translate_rk_to_keysym(BINDFOR(*setting).key);

   for (i = 0; apple_key_name_map[i].hid_id; i++)
   {
      if (apple_key_name_map[i].hid_id == hidkey)
      {
         strlcpy(buf, apple_key_name_map[i].keyname, sizeof_buf);
         break;
      }
   }
}
#endif

static void get_button_name(char *buf, size_t sizeof_buf,
      const rarch_setting_t* setting)
{
   if (BINDFOR(*setting).joykey == NO_BTN)
      return;

   snprintf(buf, sizeof_buf, "%lld",
         (long long int)(BINDFOR(*setting).joykey));
}

static void get_axis_name(char *buf, size_t sizeof_buf,
      const rarch_setting_t* setting)
{
   uint32_t joyaxis = BINDFOR(*setting).joyaxis;

   if (AXIS_NEG_GET(joyaxis) != AXIS_DIR_NONE)
      snprintf(buf, sizeof_buf, "-%d", AXIS_NEG_GET(joyaxis));
   else if (AXIS_POS_GET(joyaxis) != AXIS_DIR_NONE)
      snprintf(buf, sizeof_buf, "+%d", AXIS_POS_GET(joyaxis));
}

void setting_data_reset_setting(const rarch_setting_t* setting)
{
   switch (setting->type)
   {
      case ST_BOOL:
         *setting->value.boolean          = setting->default_value.boolean;
         break;
      case ST_INT:
         *setting->value.integer          = setting->default_value.integer;
         break;
      case ST_UINT:
         *setting->value.unsigned_integer = setting->default_value.unsigned_integer;
         break;
      case ST_FLOAT:
         *setting->value.fraction         = setting->default_value.fraction;
         break;
      case ST_BIND:
         *setting->value.keybind          = *setting->default_value.keybind;
         break;
      case ST_STRING:
      case ST_PATH:
      case ST_DIR:
         if (setting->default_value.string)
         {
            if (setting->type == ST_STRING)
               strlcpy(setting->value.string, setting->default_value.string,
                     setting->size);
            else
               fill_pathname_expand_special(setting->value.string,
                     setting->default_value.string, setting->size);
         }
         break;
         /* TODO */
      case ST_HEX:
         break;
      case ST_GROUP:
         break;
      case ST_SUB_GROUP:
         break;
      case ST_END_GROUP:
         break;
      case ST_END_SUB_GROUP:
         break;
      case ST_NONE:
         break;
   }

   if (setting->change_handler)
      setting->change_handler(setting);
}

void setting_data_reset(const rarch_setting_t* settings)
{
   for (; settings->type != ST_NONE; settings++)
      setting_data_reset_setting(settings);
}

static bool setting_data_load_config(
      const rarch_setting_t* settings, config_file_t* config)
{
   if (!config)
      return false;

   for (; settings->type != ST_NONE; settings++)
   {
      switch (settings->type)
      {
         case ST_BOOL:
            config_get_bool(config, settings->name,
                  settings->value.boolean);
            break;
         case ST_PATH:
         case ST_DIR:
            config_get_path(config, settings->name,
                  settings->value.string, settings->size);
            break;
         case ST_STRING:
            config_get_array(config, settings->name,
                  settings->value.string, settings->size);
            break;
         case ST_INT:
            config_get_int(config, settings->name,
                  settings->value.integer);

            if (settings->flags & SD_FLAG_HAS_RANGE)
            {
               if (*settings->value.integer < settings->min)
                  *settings->value.integer = settings->min;
               if (*settings->value.integer > settings->max)
                  *settings->value.integer = settings->max;
            }
            break;
         case ST_UINT:
            config_get_uint(config, settings->name,
                  settings->value.unsigned_integer);

            if (settings->flags & SD_FLAG_HAS_RANGE)
            {
               if (*settings->value.unsigned_integer < settings->min)
                  *settings->value.unsigned_integer = settings->min;
               if (*settings->value.unsigned_integer > settings->max)
                  *settings->value.unsigned_integer = settings->max;
            }
            break;
         case ST_FLOAT:
            config_get_float(config, settings->name,
                  settings->value.fraction);

            if (settings->flags & SD_FLAG_HAS_RANGE)
            {
               if (*settings->value.fraction < settings->min)
                  *settings->value.fraction = settings->min;
               if (*settings->value.fraction > settings->max)
                  *settings->value.fraction = settings->max;
            }
            break;         
         case ST_BIND:
            {
               char prefix[32];
               get_input_config_prefix(prefix, sizeof(prefix), settings);
               input_config_parse_key(config, prefix, settings->name,
                     settings->value.keybind);
               input_config_parse_joy_button(config, prefix, settings->name,
                     settings->value.keybind);
               input_config_parse_joy_axis(config, prefix, settings->name,
                     settings->value.keybind);
            }
            break;
            /* TODO */
         case ST_HEX:
            break;
         case ST_GROUP:
            break;
         case ST_SUB_GROUP:
            break;
         case ST_END_GROUP:
            break;
         case ST_END_SUB_GROUP:
            break;
         case ST_NONE:
            break;
      }

      if (settings->change_handler)
         settings->change_handler(settings);
   }

   return true;
}

bool setting_data_load_config_path(const rarch_setting_t* settings,
      const char* path)
{
   config_file_t *config = (config_file_t*)config_file_new(path);

   if (!config)
      return NULL;

   setting_data_load_config(settings, config);
   config_file_free(config);

   return config;
}

bool setting_data_save_config(const rarch_setting_t* settings,
      config_file_t* config)
{
   if (!config)
      return false;

   for (; settings->type != ST_NONE; settings++)
   {
      switch (settings->type)
      {
         case ST_BOOL:
            config_set_bool(config, settings->name,
                  *settings->value.boolean);
            break;
         case ST_PATH:
         case ST_DIR:
            config_set_path(config, settings->name,
                  settings->value.string);
            break;
         case ST_STRING:
            config_set_string(config, settings->name,
                  settings->value.string);
            break;
         case ST_INT:
            if (settings->flags & SD_FLAG_HAS_RANGE)
            {
               if (*settings->value.integer < settings->min)
                  *settings->value.integer = settings->min;
               if (*settings->value.integer > settings->max)
                  *settings->value.integer = settings->max;
            }
            config_set_int(config, settings->name, *settings->value.integer);
            break;
         case ST_UINT:
            if (settings->flags & SD_FLAG_HAS_RANGE)
            {
               if (*settings->value.unsigned_integer < settings->min)
                  *settings->value.unsigned_integer = settings->min;
               if (*settings->value.unsigned_integer > settings->max)
                  *settings->value.unsigned_integer = settings->max;
            }
            config_set_uint64(config, settings->name,
                  *settings->value.unsigned_integer);
            break;
         case ST_FLOAT:
            if (settings->flags & SD_FLAG_HAS_RANGE)
            {
               if (*settings->value.fraction < settings->min)
                  *settings->value.fraction = settings->min;
               if (*settings->value.fraction > settings->max)
                  *settings->value.fraction = settings->max;
            }
            config_set_float(config, settings->name, *settings->value.fraction);
            break;
         case ST_BIND:
            {
               char button_name[32], axis_name[32], key_name[32],
                    input_config_key[64];
               strlcpy(button_name, "nul", sizeof(button_name));
               strlcpy(axis_name,   "nul", sizeof(axis_name));
               strlcpy(key_name,    "nul", sizeof(key_name));
               strlcpy(input_config_key, "nul", sizeof(input_config_key));

               get_button_name(button_name, sizeof(button_name), settings);
               get_axis_name(axis_name, sizeof(axis_name), settings);
#ifdef APPLE
               get_key_name(key_name, sizeof(key_name), settings);
#endif
               get_input_config_key(input_config_key,
                     sizeof(input_config_key), settings, 0);
               config_set_string(config, input_config_key, key_name);
               get_input_config_key(input_config_key,
                     sizeof(input_config_key), settings, "btn");
               config_set_string(config, input_config_key, button_name);
               get_input_config_key(input_config_key,
                     sizeof(input_config_key), settings, "axis");
               config_set_string(config, input_config_key, axis_name);
            }
            break;
            /* TODO */
         case ST_HEX:
            break;
         case ST_GROUP:
            break;
         case ST_SUB_GROUP:
            break;
         case ST_END_GROUP:
            break;
         case ST_END_SUB_GROUP:
            break;
         case ST_NONE:
            break;
      }
   }

   return true;
}

rarch_setting_t* setting_data_find_setting(rarch_setting_t* setting,
      const char* name)
{
   bool found = false;

   if (!name)
      return NULL;

   for (; setting->type != ST_NONE; setting++)
   {
      if (setting->type <= ST_GROUP && !strcmp(setting->name, name))
      {
         found = true;
         break;
      }
   }

   if (!found)
      return NULL;

   if (setting->short_description && setting->short_description[0] == '\0')
      return NULL;

   if (setting->read_handler)
      setting->read_handler(setting);

   return setting;
}

void setting_data_set_with_string_representation(const rarch_setting_t* setting,
      const char* value)
{
   if (!setting || !value)
      return;

   switch (setting->type)
   {
      case ST_INT:
         sscanf(value, "%d", setting->value.integer);
         if (setting->flags & SD_FLAG_HAS_RANGE)
         {
            if (*setting->value.integer < setting->min)
               *setting->value.integer = setting->min;
            if (*setting->value.integer > setting->max)
               *setting->value.integer = setting->max;
         }
         break;
      case ST_UINT:
         sscanf(value, "%u", setting->value.unsigned_integer);
         if (setting->flags & SD_FLAG_HAS_RANGE)
         {
            if (*setting->value.unsigned_integer < setting->min)
               *setting->value.unsigned_integer = setting->min;
            if (*setting->value.unsigned_integer > setting->max)
               *setting->value.unsigned_integer = setting->max;
         }
         break;      
      case ST_FLOAT:
         sscanf(value, "%f", setting->value.fraction);
         if (setting->flags & SD_FLAG_HAS_RANGE)
         {
            if (*setting->value.fraction < setting->min)
               *setting->value.fraction = setting->min;
            if (*setting->value.fraction > setting->max)
               *setting->value.fraction = setting->max;
         }
         break;
      case ST_PATH:
      case ST_DIR:
      case ST_STRING:
         strlcpy(setting->value.string, value, setting->size);
         break;

         /* TODO */
      case ST_HEX:
         break;
      case ST_GROUP:
         break;
      case ST_SUB_GROUP:
         break;
      case ST_END_GROUP:
         break;
      case ST_END_SUB_GROUP:
         break;
      case ST_NONE:
         break;
      case ST_BOOL:
         break;
      case ST_BIND:
         break;
   }

   if (setting->change_handler)
      setting->change_handler(setting);
}

static void menu_common_setting_set_label_st_bool(rarch_setting_t *setting,
      char *type_str, size_t type_str_size)
{
   if (!strcmp(setting->name, "savestate") ||
         !strcmp(setting->name, "loadstate"))
   {
      if (g_settings.state_slot < 0)
         strlcpy(type_str, "-1 (auto)", type_str_size);
      else
         snprintf(type_str, type_str_size, "%d", g_settings.state_slot);
   }
   else
      strlcpy(type_str, *setting->value.boolean ? setting->boolean.on_label :
            setting->boolean.off_label, type_str_size);
}

static void menu_common_setting_set_label_st_uint(rarch_setting_t *setting,
      char *type_str, size_t type_str_size)
{
   if (setting && !strcmp(setting->name, "video_monitor_index"))
   {
      if (*setting->value.unsigned_integer)
         snprintf(type_str, type_str_size, "%d",
               *setting->value.unsigned_integer);
      else
         strlcpy(type_str, "0 (Auto)", type_str_size);
   }
   else if (setting && !strcmp(setting->name, "video_rotation"))
      strlcpy(type_str, rotation_lut[*setting->value.unsigned_integer],
            type_str_size);
   else if (setting && !strcmp(setting->name, "aspect_ratio_index"))
      strlcpy(type_str,
            aspectratio_lut[*setting->value.unsigned_integer].name,
            type_str_size);
   else if (setting && !strcmp(setting->name, "autosave_interval"))
   {
      if (*setting->value.unsigned_integer)
         snprintf(type_str, type_str_size, "%u seconds",
               *setting->value.unsigned_integer);
      else
         strlcpy(type_str, "OFF", type_str_size);
   }
   else if (setting && !strcmp(setting->name, "user_language"))
   {
      static const char *modes[] = {
         "English",
         "Japanese",
         "French",
         "Spanish",
         "German",
         "Italian",
         "Dutch",
         "Portuguese",
         "Russian",
         "Korean",
         "Chinese (Traditional)",
         "Chinese (Simplified)"
      };

      strlcpy(type_str, modes[g_settings.user_language], type_str_size);
   }
   else if (setting && !strcmp(setting->name, "libretro_log_level"))
   {
      static const char *modes[] = {
         "0 (Debug)",
         "1 (Info)",
         "2 (Warning)",
         "3 (Error)"
      };

      strlcpy(type_str, modes[*setting->value.unsigned_integer],
            type_str_size);
   }
   else
      snprintf(type_str, type_str_size, "%d",
            *setting->value.unsigned_integer);
}

static void menu_common_setting_set_label_st_float(rarch_setting_t *setting,
      char *type_str, size_t type_str_size)
{
   if (setting && !strcmp(setting->name, "video_refresh_rate_auto"))
   {
      double refresh_rate = 0.0;
      double deviation = 0.0;
      unsigned sample_points = 0;

      if (driver_monitor_fps_statistics(&refresh_rate, &deviation, &sample_points))
         snprintf(type_str, type_str_size, "%.3f Hz (%.1f%% dev, %u samples)",
               refresh_rate, 100.0 * deviation, sample_points);
      else
         strlcpy(type_str, "N/A", type_str_size);
   }
   else
      snprintf(type_str, type_str_size, setting->rounding_fraction,
            *setting->value.fraction);
}

void setting_data_get_string_representation(rarch_setting_t* setting,
      char* buf, size_t sizeof_buf)
{
   if (!setting || !buf || !sizeof_buf)
      return;

   switch (setting->type)
   {
      case ST_BOOL:
         menu_common_setting_set_label_st_bool(setting, buf, sizeof_buf);
         break;
      case ST_INT:
         snprintf(buf, sizeof_buf, "%d", *setting->value.integer);
         break;
      case ST_UINT:
         menu_common_setting_set_label_st_uint(setting, buf, sizeof_buf);
         break;
      case ST_FLOAT:
         menu_common_setting_set_label_st_float(setting, buf, sizeof_buf);
         break;
      case ST_DIR:
         strlcpy(buf,
               *setting->value.string ?
               setting->value.string : setting->dir.empty_path,
               sizeof_buf);
         break;
      case ST_PATH:
         strlcpy(buf, path_basename(setting->value.string), sizeof_buf);
         break;
      case ST_STRING:
         strlcpy(buf, setting->value.string, sizeof_buf);
         break;
      case ST_BIND:
         {
#if 0
            char button_name[32], axis_name[32], key_name[32];

            strlcpy(button_name, "nul", sizeof(button_name));
            strlcpy(axis_name,   "nul", sizeof(axis_name));
            strlcpy(key_name,    "nul", sizeof(key_name));

            get_button_name(button_name, sizeof(button_name), setting);
            get_axis_name(axis_name, sizeof(axis_name), setting);
#ifdef APPLE
            get_key_name(key_name, sizeof(key_name), setting);
#endif
            snprintf(buf, sizeof_buf, "[KB:%s] [JS:%s] [AX:%s]", key_name, button_name, axis_name);
#else
#ifdef HAVE_MENU
            const struct retro_keybind* bind = (const struct retro_keybind*)
               &setting->value.keybind[driver.menu->current_pad];
            const struct retro_keybind* auto_bind = (const struct retro_keybind*)
               input_get_auto_bind(driver.menu->current_pad, bind->id);
            input_get_bind_string(buf, bind, auto_bind, sizeof_buf);
#endif
#endif
         }
         break;
         /* TODO */
      case ST_HEX:
         break;
      case ST_GROUP:
         strlcpy(buf, "...", sizeof_buf);
         break;
      case ST_SUB_GROUP:
         strlcpy(buf, "...", sizeof_buf);
         break;
      case ST_END_GROUP:
         break;
      case ST_END_SUB_GROUP:
         break;
      case ST_NONE:
         break;
   }
}

rarch_setting_t setting_data_group_setting(enum setting_type type, const char* name)
{
   rarch_setting_t result = { type, name };

   result.short_description = name;
   return result;
}

rarch_setting_t setting_data_subgroup_setting(enum setting_type type, const char* name,
      const char *parent_name)
{
   rarch_setting_t result = { type, name };

   result.short_description = name;
   result.group = parent_name;
   return result;
}

rarch_setting_t setting_data_float_setting(const char* name,
      const char* short_description, float* target, float default_value,
      const char *rounding, const char *group, const char *subgroup, change_handler_t change_handler,
      change_handler_t read_handler)
{
   rarch_setting_t result = { ST_FLOAT, name, sizeof(float), short_description,
      group, subgroup };

   result.rounding_fraction = rounding;
   result.change_handler = change_handler;
   result.read_handler = read_handler;
   result.value.fraction = target;
   result.default_value.fraction = default_value;
   return result;
}

rarch_setting_t setting_data_bool_setting(const char* name,
      const char* short_description, bool* target, bool default_value,
      const char *off, const char *on,
      const char *group, const char *subgroup, change_handler_t change_handler,
      change_handler_t read_handler)
{
   rarch_setting_t result = { ST_BOOL, name, sizeof(bool), short_description,
      group, subgroup };
   result.change_handler = change_handler;
   result.read_handler = read_handler;
   result.value.boolean = target;
   result.default_value.boolean = default_value;
   result.boolean.off_label = off;
   result.boolean.on_label = on;
   return result;
}

rarch_setting_t setting_data_int_setting(const char* name,
      const char* short_description, int* target, int default_value,
      const char *group, const char *subgroup, change_handler_t change_handler,
      change_handler_t read_handler)
{
    rarch_setting_t result = { ST_INT, name, sizeof(int), short_description,
       group, subgroup };

    result.change_handler = change_handler;
    result.read_handler = read_handler;
    result.value.integer = target;
    result.default_value.integer = default_value;
    return result;
}

rarch_setting_t setting_data_uint_setting(const char* name,
      const char* short_description, unsigned int* target,
      unsigned int default_value, const char *group, const char *subgroup,
      change_handler_t change_handler, change_handler_t read_handler)
{
   rarch_setting_t result = { ST_UINT, name, sizeof(unsigned int),
      short_description, group, subgroup };

   result.change_handler = change_handler;
   result.read_handler = read_handler;
   result.value.unsigned_integer = target;
   result.default_value.unsigned_integer = default_value;

   return result;
}

rarch_setting_t setting_data_string_setting(enum setting_type type,
      const char* name, const char* short_description, char* target,
      unsigned size, const char* default_value, const char *empty,
      const char *group, const char *subgroup, change_handler_t change_handler,
      change_handler_t read_handler)
{
   rarch_setting_t result = { type, name, size, short_description, group,
      subgroup };
    
   result.dir.empty_path = empty;
   result.change_handler = change_handler;
   result.read_handler = read_handler;
   result.value.string = target;
   result.default_value.string = default_value;

   return result;
}

rarch_setting_t setting_data_bind_setting(const char* name,
      const char* short_description, struct retro_keybind* target,
      uint32_t index, const struct retro_keybind* default_value,
      const char *group, const char *subgroup)
{
   rarch_setting_t result = { ST_BIND, name, 0, short_description, group,
      subgroup };

   result.value.keybind = target;
   result.default_value.keybind = default_value;
   result.index = index;

   return result;
}

int setting_data_get_description(const char *label, char *msg,
      size_t sizeof_msg)
{
    if (!strcmp(label, "input_driver"))
    {
       if (!strcmp(g_settings.input.driver, "udev"))
          snprintf(msg, sizeof_msg,
                " -- udev Input driver. \n"
                " \n"
                "This driver can run without X. \n"
                " \n"
                "It uses the recent evdev joypad API \n"
                "for joystick support. It supports \n"
                "hotplugging and force feedback (if \n"
                "supported by device). \n"
                " \n"
                "The driver reads evdev events for keyboard \n"
                "support. It also supports keyboard callback, \n"
                "mice and touchpads. \n"
                " \n"
                "By default in most distros, /dev/input nodes \n"
                "are root-only (mode 600). You can set up a udev \n"
                "rule which makes these accessible to non-root."
                );
       else if (!strcmp(g_settings.input.driver, "linuxraw"))
          snprintf(msg, sizeof_msg,
                " -- linuxraw Input driver. \n"
                " \n"
                "This driver requires an active TTY. Keyboard \n"
                "events are read directly from the TTY which \n"
                "makes it simpler, but not as flexible as udev. \n"
                "Mice, etc, are not supported at all. \n"
                " \n"
                "This driver uses the older joystick API \n"
                "(/dev/input/js*).");
       else
          snprintf(msg, sizeof_msg,
                " -- Input driver.\n"
                " \n"
                "Depending on video driver, it might \n"
                "force a different input driver.");

    }
    else if (!strcmp(label, "load_content"))
    {
       snprintf(msg, sizeof_msg,
             " -- Load Content. \n"
             "Browse for content. \n"
             " \n"
             "To load content, you need a \n"
             "libretro core to use, and a \n"
             "content file. \n"
             " \n"
             "To control where the menu starts \n"
             " to browse for content, set  \n"
             "Browser Directory. If not set,  \n"
             "it will start in root. \n"
             " \n"
             "The browser will filter out \n"
             "extensions for the last core set \n"
             "in 'Core', and use that core when \n"
             "content is loaded."
             );
    }
    else if (!strcmp(label, "core_list"))
    {
       snprintf(msg, sizeof_msg,
             " -- Core Selection. \n"
             " \n"
             "Browse for a libretro core \n"
             "implementation. Where the browser \n"
             "starts depends on your Core Directory \n"
             "path. If blank, it will start in root. \n"
             " \n"
             "If Core Directory is a directory, the menu \n"
             "will use that as top folder. If Core \n"
             "Directory is a full path, it will start \n"
             "in the folder where the file is.");
    }
    else if (!strcmp(label, "history_list"))
    {
       snprintf(msg, sizeof_msg,
             " -- Loading content from history. \n"
             " \n"
             "As content is loaded, content and libretro \n"
             "core combinations are saved to history. \n"
             " \n"
             "The history is saved to a file in the same \n"
             "directory as the RetroArch config file. If \n"
             "no config file was loaded in startup, history \n"
             "will not be saved or loaded, and will not exist \n"
             "in the main menu."
             );
    }
    else if (!strcmp(label, "audio_resampler_driver"))
    {
       if (!strcmp(g_settings.audio.resampler, "sinc"))
          snprintf(msg, sizeof_msg,
                " -- Windowed SINC implementation.");
       else if (!strcmp(g_settings.audio.resampler, "CC"))
          snprintf(msg, sizeof_msg,
                " -- Convoluted Cosine implementation.");
    }
    else if (!strcmp(label, "video_driver"))
    {
       if (!strcmp(g_settings.video.driver, "gl"))
          snprintf(msg, sizeof_msg,
                " -- OpenGL Video driver. \n"
                " \n"
                "This driver allows libretro GL cores to  \n"
                "be used in addition to software-rendered \n"
                "core implementations.\n"
                " \n"
                "Performance for software-rendered and \n"
                "libretro GL core implementations is \n"
                "dependent on your graphics card's \n"
                "underlying GL driver).");
       else if (!strcmp(g_settings.video.driver, "sdl2"))
          snprintf(msg, sizeof_msg,
                " -- SDL 2 Video driver.\n"
                " \n"
                "This is an SDL 2 software-rendered video \n"
                "driver.\n"
                " \n"
                "Performance for software-rendered libretro \n"
                "core implementations is dependent \n"
                "on your platform SDL implementation.");
       else if (!strcmp(g_settings.video.driver, "sdl"))
          snprintf(msg, sizeof_msg,
                " -- SDL Video driver.\n"
                " \n"
                "This is an SDL 1.2 software-rendered video \n"
                "driver.\n"
                " \n"
                "Performance is considered to be suboptimal. \n"
                "Consider using it only as a last resort.");
       else if (!strcmp(g_settings.video.driver, "d3d"))
          snprintf(msg, sizeof_msg,
                " -- Direct3D Video driver. \n"
                " \n"
                "Performance for software-rendered cores \n"
                "is dependent on your graphic card's \n"
                "underlying D3D driver).");
       else if (!strcmp(g_settings.video.driver, "exynos"))
          snprintf(msg, sizeof_msg,
                " -- Exynos-G2D Video Driver. \n"
                " \n"
                "This is a low-level Exynos video driver. \n"
                "Uses the G2D block in Samsung Exynos SoC \n"
                "for blit operations. \n"
                " \n"
                "Performance for software rendered cores \n"
                "should be optimal.");
       else
          snprintf(msg, sizeof_msg,
                " -- Current Video driver.");
    }
    else if (!strcmp(label, "audio_dsp_plugin"))
    {
       snprintf(msg, sizeof_msg,
             " -- Audio DSP plugin.\n"
             " Processes audio before it's sent to \n"
             "the driver."
             );
    }
    else if (!strcmp(label, "libretro_dir_path"))
    {
       snprintf(msg, sizeof_msg,
             " -- Core Directory. \n"
             " \n"
             "A directory for where to search for \n"
             "libretro core implementations.");
    }
    else if (!strcmp(label, "video_disable_composition"))
    {
       snprintf(msg, sizeof_msg,
             "-- Forcibly disable composition.\n"
             "Only valid on Windows Vista/7 for now.");
    }
    else if (!strcmp(label, "libretro_log_level"))
    {
         snprintf(msg, sizeof_msg,
               "-- Sets log level for libretro cores \n"
               "(GET_LOG_INTERFACE). \n"
               " \n"
               " If a log level issued by a libretro \n"
               " core is below libretro_log level, it \n"
               " is ignored.\n"
               " \n"
               " DEBUG logs are always ignored unless \n"
               " verbose mode is activated (--verbose).\n"
               " \n"
               " DEBUG = 0\n"
               " INFO  = 1\n"
               " WARN  = 2\n"
               " ERROR = 3"
               );
    }
    else if (!strcmp(label, "log_verbosity"))
    {
         snprintf(msg, sizeof_msg,
               "-- Enable or disable verbosity level \n"
               "of frontend.");
    }
    else if (!strcmp(label, "perfcnt_enable"))
    {
         snprintf(msg, sizeof_msg,
               "-- Enable or disable frontend \n"
               "performance counters.");
    }
    else if (!strcmp(label, "system_directory"))
    {
         snprintf(msg, sizeof_msg,
               "-- System Directory. \n"
               " \n"
               "Sets the 'system' directory.\n"
               "Implementations can query for this\n"
               "directory to load BIOSes, \n"
               "system-specific configs, etc.");
    }
    else if (!strcmp(label, "rgui_show_start_screen"))
    {
       snprintf(msg, sizeof_msg,
             " -- Show startup screen in menu.\n"
             "Is automatically set to false when seen\n"
             "for the first time.\n"
             " \n"
             "This is only updated in config if\n"
             "'Config Save On Exit' is set to true.\n");
    }
    else if (!strcmp(label, "config_save_on_exit"))
    {
         snprintf(msg, sizeof_msg,
               " -- Flushes config to disk on exit.\n"
               "Useful for menu as settings can be\n"
               "modified. Overwrites the config.\n"
               " \n"
               "#include's and comments are not \n"
               "preserved. \n"
               " \n"
               "By design, the config file is \n"
               "considered immutable as it is \n"
               "likely maintained by the user, \n"
               "and should not be overwritten \n"
               "behind the user's back."
#if defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)
               "\nThis is not not the case on \n"
               "consoles however, where \n"
               "looking at the config file \n"
               "manually isn't really an option."
#endif
               );
    }
    else if (!strcmp(label, "core_specific_config"))
    {
         snprintf(msg, sizeof_msg,
               " -- Load up a specific config file \n"
               "based on the core being used.\n");
    }
    else if (!strcmp(label, "video_scale"))
    {
         snprintf(msg, sizeof_msg,
               " -- Fullscreen resolution.\n"
               " \n"
               "Resolution of 0 uses the \n"
               "resolution of the environment.\n");
    }
    else if (!strcmp(label, "video_vsync"))
    {
         snprintf(msg, sizeof_msg,
               " -- Video V-Sync.\n");
    }
    else if (!strcmp(label, "video_hard_sync"))
    {
         snprintf(msg, sizeof_msg,
               " -- Attempts to hard-synchronize \n"
               "CPU and GPU.\n"
               " \n"
               "Can reduce latency at cost of \n"
               "performance.");
    }
    else if (!strcmp(label, "video_hard_sync_frames"))
    {
         snprintf(msg, sizeof_msg,
               " -- Sets how many frames CPU can \n"
               "run ahead of GPU when using 'GPU \n"
               "Hard Sync'.\n"
               " \n"
               "Maximum is 3.\n"
               " \n"
               " 0: Syncs to GPU immediately.\n"
               " 1: Syncs to previous frame.\n"
               " 2: Etc ...");
    }
    else if (!strcmp(label, "video_frame_delay"))
    {
         snprintf(msg, sizeof_msg,
               " -- Sets how many milliseconds to delay\n"
               "after VSync before running the core.\n"
                "\n"
               "Can reduce latency at cost of\n"
               "higher risk of stuttering.\n"
               " \n"
               "Maximum is 15.");
    }
    else if (!strcmp(label, "audio_rate_control_delta"))
    {
       snprintf(msg, sizeof_msg,
             " -- Audio rate control.\n"
             " \n"
             "Setting this to 0 disables rate control.\n"
             "Any other value controls audio rate control \n"
             "delta.\n"
             " \n"
             "Defines how much input rate can be adjusted \n"
             "dynamically.\n"
             " \n"
             " Input rate is defined as: \n"
             " input rate * (1.0 +/- (rate control delta))");
    }
    else if (!strcmp(label, "video_filter"))
    {
#ifdef HAVE_FILTERS_BUILTIN
       snprintf(msg, sizeof_msg,
             " -- CPU-based video filter.");
#else
    snprintf(msg, sizeof_msg,
          " -- CPU-based video filter.\n"
          " \n"
          "Path to a dynamic library.");
#endif
    }
    else if (!strcmp(label, "video_fullscreen"))
    {
       snprintf(msg, sizeof_msg, " -- Toggles fullscreen.");
    }
    else if (!strcmp(label, "audio_device"))
    {
       snprintf(msg, sizeof_msg,
             " -- Override the default audio device \n"
             "the audio driver uses.\n"
             "This is driver dependent. E.g.\n"
#ifdef HAVE_ALSA
             " \n"
             "ALSA wants a PCM device."
#endif
#ifdef HAVE_OSS
             " \n"
             "OSS wants a path (e.g. /dev/dsp)."
#endif
#ifdef HAVE_JACK
             " \n"
             "JACK wants portnames (e.g. system:playback1\n"
             ",system:playback_2)."
#endif
#ifdef HAVE_RSOUND
             " \n"
             "RSound wants an IP address to an RSound \n"
             "server."
#endif
             );
    }
    else if (!strcmp(label, "video_black_frame_insertion"))
    {
       snprintf(msg, sizeof_msg,
             " -- Inserts a black frame inbetween \n"
             "frames.\n"
             " \n"
             "Useful for 120 Hz monitors who want to \n"
             "play 60 Hz material with eliminated \n"
             "ghosting.\n"
             " \n"
             "Video refresh rate should still be \n"
             "configured as if it is a 60 Hz monitor \n"
             "(divide refresh rate by 2).");
    }
    else if (!strcmp(label, "video_threaded"))
    {
         snprintf(msg, sizeof_msg,
               " -- Use threaded video driver.\n"
               " \n"
               "Using this might improve performance at \n"
               "possible cost of latency and more video \n"
               "stuttering.");
    }
    else if (!strcmp(label, "video_scale_integer"))
    {
         snprintf(msg, sizeof_msg,
               " -- Only scales video in integer \n"
               "steps.\n"
               " \n"
               "The base size depends on system-reported \n"
               "geometry and aspect ratio.\n"
               " \n"
               "If Force Aspect is not set, X/Y will be \n"
               "integer scaled independently.");
    }
    else if (!strcmp(label, "video_crop_overscan"))
    {
         snprintf(msg, sizeof_msg,
               " -- Forces cropping of overscanned \n"
               "frames.\n"
               " \n"
               "Exact behavior of this option is \n"
               "core-implementation specific.");
    }
    else if (!strcmp(label, "video_monitor_index"))
    {
         snprintf(msg, sizeof_msg,
               " -- Which monitor to prefer.\n"
               " \n"
               "0 (default) means no particular monitor \n"
               "is preferred, 1 and up (1 being first \n"
               "monitor), suggests RetroArch to use that \n"
               "particular monitor.");
    }
    else if (!strcmp(label, "video_rotation"))
    {
         snprintf(msg, sizeof_msg,
               " -- Forces a certain rotation \n"
               "of the screen.\n"
               " \n"
               "The rotation is added to rotations which\n"
               "the libretro core sets (see Video Allow\n"
               "Rotate).");
    }
    else if (!strcmp(label, "audio_volume"))
    {
         snprintf(msg, sizeof_msg,
               " -- Audio volume, expressed in dB.\n"
               " \n"
               " 0 dB is normal volume. No gain will be applied.\n"
               "Gain can be controlled in runtime with Input\n"
               "Volume Up / Input Volume Down.");
    }
    else if (!strcmp(label, "block_sram_overwrite"))
    {
         snprintf(msg, sizeof_msg,
               " -- Block SRAM from being overwritten \n"
               "when loading save states.\n"
               " \n"
               "Might potentially lead to buggy games.");
    }
    else if (!strcmp(label, "fastforward_ratio"))
    {
         snprintf(msg, sizeof_msg,
               " -- Fastforward ratio."
               " \n"
               "The maximum rate at which content will\n"
               "be run when using fast forward.\n"
               " \n"
               " (E.g. 5.0 for 60 fps content => 300 fps \n"
               "cap).\n"
               " \n"
               "RetroArch will go to sleep to ensure that \n"
               "the maximum rate will not be exceeded.\n"
               "Do not rely on this cap to be perfectly \n"
               "accurate.");
    }
    else if (!strcmp(label, "pause_nonactive"))
    {
         snprintf(msg, sizeof_msg,
               " -- Pause gameplay when window focus \n"
               "is lost.");
    }
    else if (!strcmp(label, "video_gpu_screenshot"))
    {
         snprintf(msg, sizeof_msg,
               " -- Screenshots output of GPU shaded \n"
               "material if available.");
    }
    else if (!strcmp(label, "autosave_interval"))
    {
         snprintf(msg, sizeof_msg,
               " -- Autosaves the non-volatile SRAM \n"
               "at a regular interval.\n"
               " \n"
               "This is disabled by default unless set \n"
               "otherwise. The interval is measured in \n"
               "seconds. \n"
               " \n"
               "A value of 0 disables autosave.");
    }
    else if (!strcmp(label, "screenshot_directory"))
    {
         snprintf(msg, sizeof_msg,
               " -- Screenshot Directory. \n"
               " \n"
               "Directory to dump screenshots to."
               );
    }
    else if (!strcmp(label, "video_swap_interval"))
    {
         snprintf(msg, sizeof_msg,
               " -- VSync Swap Interval.\n"
               " \n"
               "Uses a custom swap interval for VSync. Set this \n"
               "to effectively halve monitor refresh rate.");
    }
    else if (!strcmp(label, "video_refresh_rate_auto"))
    {
         snprintf(msg, sizeof_msg,
               " -- Refresh Rate Auto.\n"
               " \n"
               "The accurate refresh rate of our monitor (Hz).\n"
               "This is used to calculate audio input rate with \n"
               "the formula: \n"
               " \n"
               "audio_input_rate = game input rate * display \n"
               "refresh rate / game refresh rate\n"
               " \n"
               "If the implementation does not report any \n"
               "values, NTSC defaults will be assumed for \n"
               "compatibility.\n"
               " \n"
               "This value should stay close to 60Hz to avoid \n"
               "large pitch changes. If your monitor does \n"
               "not run at 60Hz, or something close to it, \n"
               "disable VSync, and leave this at its default.");
    }
    else if (!strcmp(label, "savefile_directory"))
    {
       snprintf(msg, sizeof_msg,
             " -- Savefile Directory. \n"
             " \n"
             "Save all save files (*.srm) to this \n"
             "directory. This includes related files like \n"
             ".bsv, .rt, .psrm, etc...\n"
             " \n"
             "This will be overridden by explicit command line\n"
             "options.");
    }
    else if (!strcmp(label, "savestate_directory"))
    {
         snprintf(msg, sizeof_msg,
               " -- Savestate Directory. \n"
               " \n"
               "Save all save states (*.state) to this \n"
               "directory.\n"
               " \n"
               "This will be overridden by explicit command line\n"
               "options.");
    }
    else if (!strcmp(label, "assets_directory"))
    {
       snprintf(msg, sizeof_msg,
             " -- Assets Directory. \n"
             " \n"
             " This location is queried by default when \n"
             "menu interfaces try to look for loadable \n"
             "assets, etc.");
    }
    else if (!strcmp(label, "slowmotion_ratio"))
    {
         snprintf(msg, sizeof_msg,
               " -- Slowmotion ratio."
               " \n"
               "When slowmotion, content will slow\n"
               "down by factor.");
    }
    else if (!strcmp(label, "input_axis_threshold"))
    {
         snprintf(msg, sizeof_msg,
               " -- Defines axis threshold.\n"
               " \n"
               "How far an axis must be tilted to result\n"
               "in a button press.\n"
               " Possible values are [0.0, 1.0].");
    }
    else if (!strcmp(label, "input_turbo_period"))
    {
       snprintf(msg, sizeof_msg, 
             " -- Turbo period.\n"
             " \n"
             "Describes speed of which turbo-enabled\n"
             "buttons toggle."
             );
    }
    else if (!strcmp(label, "rewind_granularity"))
    {
         snprintf(msg, sizeof_msg,
               " -- Rewind granularity.\n"
               " \n"
               " When rewinding defined number of \n"
               "frames, you can rewind several frames \n"
               "at a time, increasing the rewinding \n"
               "speed.");
    }
    else if (!strcmp(label, "rewind_enable"))
    {
         snprintf(msg, sizeof_msg,
               " -- Enable rewinding.\n"
               " \n"
               "This will take a performance hit, \n"
               "so it is disabled by default.");
    }
    else if (!strcmp(label, "input_autodetect_enable"))
    {
         snprintf(msg, sizeof_msg,
               " -- Enable input auto-detection.\n"
               " \n"
               "Will attempt to auto-configure \n"
               "joypads, Plug-and-Play style.");
    }
    else if (!strcmp(label, "camera_allow"))
    {
         snprintf(msg, sizeof_msg,
               " -- Allow or disallow camera access by \n"
               "cores.");
    }
    else if (!strcmp(label, "location_allow"))
    {
       snprintf(msg, sizeof_msg,
             " -- Allow or disallow location services \n"
             "access by cores.");
    }
    else if (!strcmp(label, "savestate_auto_save"))
    {
         snprintf(msg, sizeof_msg,
               " -- Automatically saves a savestate at the \n"
               "end of RetroArch's lifetime.\n"
               " \n"
               "RetroArch will automatically load any savestate\n"
               "with this path on startup if 'Savestate Auto\n"
               "Load' is set.");
    }
    else if (!strcmp(label, "shader_apply_changes"))
    {
       snprintf(msg, sizeof_msg,
             " -- Apply Shader Changes. \n"
             " \n"
             "After changing shader settings, use this to \n"
             "apply changes. \n"
             " \n"
             "Changing shader settings is a somewhat \n"
             "expensive operation so it has to be \n"
             "done explicitly. \n"
             " \n"
             "When you apply shaders, the menu shader \n"
             "settings are saved to a temporary file (either \n"
             "menu.cgp or menu.glslp) and loaded. The file \n"
             "persists after RetroArch exits. The file is \n"
             "saved to Shader Directory."
             );

    }
    else if (!strcmp(label, "video_shader_preset")) 
    {
       snprintf(msg, sizeof_msg,
             " -- Load Shader Preset. \n"
             " \n"
             " Load a "
#ifdef HAVE_CG
             "Cg"
#endif
#ifdef HAVE_GLSL
#ifdef HAVE_CG
             "/"
#endif
             "GLSL"
#endif
#ifdef HAVE_HLSL
#if defined(HAVE_CG) || defined(HAVE_HLSL)
             "/"
#endif
             "HLSL"
#endif
             " preset directly. \n"
             "The menu shader menu is updated accordingly. \n"
             " \n"
             "If the CGP uses scaling methods which are not \n"
             "simple, (i.e. source scaling, same scaling \n"
             "factor for X/Y), the scaling factor displayed \n"
             "in the menu might not be correct."
             );
    }
    else if (!strcmp(label, "video_shader_num_passes")) 
    {
       snprintf(msg, sizeof_msg,
             " -- Shader Passes. \n"
             " \n"
             "RetroArch allows you to mix and match various \n"
             "shaders with arbitrary shader passes, with \n"
             "custom hardware filters and scale factors. \n"
             " \n"
             "This option specifies the number of shader \n"
             "passes to use. If you set this to 0, and use \n"
             "Apply Shader Changes, you use a 'blank' shader. \n"
             " \n"
             "The Default Filter option will affect the \n"
             "stretching filter.");
    }
    else if (!strcmp(label, "video_shader_parameters"))
    {
       snprintf(msg, sizeof_msg,
             "-- Shader Parameters. \n"
             " \n"
             "Modifies current shader directly. Will not be \n"
             "saved to CGP/GLSLP preset file.");
    }
    else if (!strcmp(label, "video_shader_preset_parameters"))
    {
       snprintf(msg, sizeof_msg,
             "-- Shader Preset Parameters. \n"
             " \n"
             "Modifies shader preset currently in menu."
             );
    }
    else if (!strcmp(label, "video_shader_pass"))
    {
       snprintf(msg, sizeof_msg,
             " -- Path to shader. \n"
             " \n"
             "All shaders must be of the same \n"
             "type (i.e. CG, GLSL or HLSL). \n"
             " \n"
             "Set Shader Directory to set where \n"
             "the browser starts to look for \n"
             "shaders."
             );
    }
    else if (!strcmp(label, "video_shader_filter_pass"))
    {
       snprintf(msg, sizeof_msg,
             " -- Hardware filter for this pass. \n"
             " \n"
             "If 'Don't Care' is set, 'Default \n"
             "Filter' will be used."
             );
    }
    else if (!strcmp(label, "video_shader_scale_pass"))
    {
       snprintf(msg, sizeof_msg,
             " -- Scale for this pass. \n"
             " \n"
             "The scale factor accumulates, i.e. 2x \n"
             "for first pass and 2x for second pass \n"
             "will give you a 4x total scale. \n"
             " \n"
             "If there is a scale factor for last \n"
             "pass, the result is stretched to \n"
             "screen with the filter specified in \n"
             "'Default Filter'. \n"
             " \n"
             "If 'Don't Care' is set, either 1x \n"
             "scale or stretch to fullscreen will \n"
             "be used depending if it's not the last \n"
             "pass or not."
             );
    }
    else if (
          !strcmp(label, "l_x_plus")  ||
          !strcmp(label, "l_x_minus") ||
          !strcmp(label, "l_y_plus")  ||
          !strcmp(label, "l_y_minus")
          )
       snprintf(msg, sizeof_msg,
             " -- Axis for analog stick (DualShock-esque).\n"
             " \n"
             "Bound as usual, however, if a real analog \n"
             "axis is bound, it can be read as a true analog.\n"
             " \n"
             "Positive X axis is right. \n"
             "Positive Y axis is down.");
    else if (!strcmp(label, "turbo"))
       snprintf(msg, sizeof_msg,
             " -- Turbo enable.\n"
             " \n"
             "Holding the turbo while pressing another \n"
             "button will let the button enter a turbo \n"
             "mode where the button state is modulated \n"
             "with a periodic signal. \n"
             " \n"
             "The modulation stops when the button \n"
             "itself (not turbo button) is released.");
    else if (!strcmp(label, "exit_emulator"))
            snprintf(msg, sizeof_msg,
                  " -- Key to exit RetroArch cleanly."
#if !defined(RARCH_MOBILE) && !defined(RARCH_CONSOLE)
                  "\nKilling it in any hard way (SIGKILL, \n"
                  "etc) will terminate without saving\n"
                  "RAM, etc. On Unix-likes,\n"
                  "SIGINT/SIGTERM allows\n"
                  "a clean deinitialization."
#endif
                  );
    else if (!strcmp(label, "rewind"))
            snprintf(msg, sizeof_msg,
                  " -- Hold button down to rewind.\n"
                  " \n"
                  "Rewind must be enabled.");
    else if (!strcmp(label, "load_state"))
       snprintf(msg, sizeof_msg,
             " -- Loads state.");
    else if (!strcmp(label, "save_state"))
       snprintf(msg, sizeof_msg,
                  " -- Saves state.");
    else if (!strcmp(label, "state_slot_increase") ||
          !strcmp(label, "state_slot_decrease"))
       snprintf(msg, sizeof_msg,
             " -- State slots.\n"
             " \n"
             " With slot set to 0, save state name is *.state \n"
             " (or whatever defined on commandline).\n"
             "When slot is != 0, path will be (path)(d), \n"
             "where (d) is slot number.");
    else if (!strcmp(label, "netplay_flip_players"))
       snprintf(msg, sizeof_msg,
             " -- Netplay flip players.");
    else if (!strcmp(label, "frame_advance"))
       snprintf(msg, sizeof_msg,
             " -- Frame advance when content is paused.");
    else if (!strcmp(label, "enable_hotkey"))
       snprintf(msg, sizeof_msg,
             " -- Enable other hotkeys.\n"
             " \n"
             " If this hotkey is bound to either keyboard, \n"
             "joybutton or joyaxis, all other hotkeys will \n"
             "be disabled unless this hotkey is also held \n"
             "at the same time. \n"
             " \n"
             "This is useful for RETRO_KEYBOARD centric \n"
             "implementations which query a large area of \n"
             "the keyboard, where it is not desirable that \n"
             "hotkeys get in the way.");
    else if (!strcmp(label, "slowmotion"))
       snprintf(msg, sizeof_msg,
             " -- Hold for slowmotion.");
    else if (!strcmp(label, "movie_record_toggle"))
       snprintf(msg, sizeof_msg,
             " -- Toggle between recording and not.");
    else if (!strcmp(label, "pause_toggle"))
       snprintf(msg, sizeof_msg,
             " -- Toggle between paused and non-paused state.");
    else if (!strcmp(label, "hold_fast_forward"))
       snprintf(msg, sizeof_msg,
             " -- Hold for fast-forward. Releasing button \n"
             "disables fast-forward.");
    else if (!strcmp(label, "shader_next"))
       snprintf(msg, sizeof_msg,
             " -- Applies next shader in directory.");
    else if (!strcmp(label, "reset"))
       snprintf(msg, sizeof_msg,
             " -- Reset the content.\n");
    else if (!strcmp(label, "cheat_index_plus"))
       snprintf(msg, sizeof_msg,
             " -- Increment cheat index.\n");
    else if (!strcmp(label, "cheat_index_minus"))
       snprintf(msg, sizeof_msg,
             " -- Decrement cheat index.\n");
    else if (!strcmp(label, "cheat_toggle"))
       snprintf(msg, sizeof_msg,
             " -- Toggle cheat index.\n");
    else if (!strcmp(label, "shader_prev"))
       snprintf(msg, sizeof_msg,
             " -- Applies previous shader in directory.");
    else if (!strcmp(label, "audio_mute"))
       snprintf(msg, sizeof_msg,
             " -- Mute/unmute audio.");
    else if (!strcmp(label, "screenshot"))
       snprintf(msg, sizeof_msg,
             " -- Take screenshot.");
    else if (!strcmp(label, "volume_up"))
       snprintf(msg, sizeof_msg,
             " -- Increases audio volume.");
    else if (!strcmp(label, "volume_down"))
       snprintf(msg, sizeof_msg,
             " -- Decreases audio volume.");
    else if (!strcmp(label, "overlay_next"))
       snprintf(msg, sizeof_msg,
             " -- Toggles to next overlay.\n"
             " \n"
             "Wraps around.");
    else if (!strcmp(label, "disk_eject_toggle"))
       snprintf(msg, sizeof_msg,
             " -- Toggles eject for disks.\n"
             " \n"
             "Used for multiple-disk content.");
    else if (!strcmp(label, "disk_next"))
       snprintf(msg, sizeof_msg,
             " -- Cycles through disk images. Use after \n"
             "ejecting. \n"
             " \n"
             " Complete by toggling eject again.");
    else if (!strcmp(label, "grab_mouse_toggle"))
       snprintf(msg, sizeof_msg,
             " -- Toggles mouse grab.\n"
             " \n"
             "When mouse is grabbed, RetroArch hides the \n"
             "mouse, and keeps the mouse pointer inside \n"
             "the window to allow relative mouse input to \n"
             "work better.");
    else if (!strcmp(label, "menu_toggle"))
       snprintf(msg, sizeof_msg,
             " -- Toggles menu.");
    else if (!strcmp(label, "input_bind_device_id"))
       snprintf(msg, sizeof_msg,
       " -- Input Device. \n"
          " \n"
          "Picks which gamepad to use for player N. \n"
          "The name of the pad is available."
          );
    else if (!strcmp(label, "input_bind_device_type"))
       snprintf(msg, sizeof_msg,
             " -- Input Device Type. \n"
             " \n"
             "Picks which device type to use. This is \n"
             "relevant for the libretro core itself."
             );
    else
       snprintf(msg, sizeof_msg,
             "-- No info on this item is available. --\n");

    return 0;
}

#ifdef GEKKO
static unsigned menu_gx_resolutions[GX_RESOLUTIONS_LAST][2] = {
   { 512, 192 },
   { 598, 200 },
   { 640, 200 },
   { 384, 224 },
   { 448, 224 },
   { 480, 224 },
   { 512, 224 },
   { 576, 224 },
   { 608, 224 },
   { 640, 224 },
   { 340, 232 },
   { 512, 232 },
   { 512, 236 },
   { 336, 240 },
   { 352, 240 },
   { 384, 240 },
   { 512, 240 },
   { 530, 240 },
   { 640, 240 },
   { 512, 384 },
   { 598, 400 },
   { 640, 400 },
   { 384, 448 },
   { 448, 448 },
   { 480, 448 },
   { 512, 448 },
   { 576, 448 },
   { 608, 448 },
   { 640, 448 },
   { 340, 464 },
   { 512, 464 },
   { 512, 472 },
   { 352, 480 },
   { 384, 480 },
   { 512, 480 },
   { 530, 480 },
   { 608, 480 },
   { 640, 480 },
};

static unsigned menu_current_gx_resolution = GX_RESOLUTIONS_640_480;
#endif

static void menu_common_setting_set_label_perf(char *type_str,
      size_t type_str_size, unsigned *w, unsigned type,
      const struct retro_perf_counter **counters, unsigned offset)
{
   if (counters[offset] && counters[offset]->call_cnt)
   {
      snprintf(type_str, type_str_size,
#ifdef _WIN32
            "%I64u ticks, %I64u runs.",
#else
            "%llu ticks, %llu runs.",
#endif
            ((unsigned long long)counters[offset]->total /
             (unsigned long long)counters[offset]->call_cnt),
            (unsigned long long)counters[offset]->call_cnt);
   }
   else
   {
      *type_str = '\0';
      *w = 0;
   }
}




void setting_data_get_label(char *type_str,
      size_t type_str_size, unsigned *w, unsigned type, 
      const char *menu_label, const char *label, unsigned index)
{
   rarch_setting_t *setting_data = (rarch_setting_t*)setting_data_get_list();
   rarch_setting_t *setting = (rarch_setting_t*)setting_data_find_setting(setting_data,
         driver.menu->selection_buf->list[index].label);

   if ((!strcmp(menu_label, "Shader Options") ||
            !strcmp(menu_label, "video_shader_parameters") ||
            !strcmp(menu_label, "video_shader_preset_parameters"))
         &&
         driver.menu_ctx && driver.menu_ctx->backend &&
         driver.menu_ctx->backend->shader_manager_get_str
      )
   {
      driver.menu_ctx->backend->shader_manager_get_str(
            driver.menu->shader, type_str, type_str_size,
            menu_label, label, type);
   }
   else if (!strcmp(label, "input_bind_device_id"))
   {
      int map = g_settings.input.joypad_map
         [driver.menu->current_pad];
      if (map >= 0 && map < MAX_PLAYERS)
      {
         const char *device_name = 
            g_settings.input.device_names[map];

         if (*device_name)
            strlcpy(type_str, device_name, type_str_size);
         else
            snprintf(type_str, type_str_size,
                  "N/A (port #%d)", map);
      }
      else
         strlcpy(type_str, "Disabled", type_str_size);
   }
   else if (!strcmp(label, "input_bind_device_type"))
   {
      const struct retro_controller_description *desc = NULL;
      if (driver.menu->current_pad < g_extern.system.num_ports)
         desc = libretro_find_controller_description(
               &g_extern.system.ports[driver.menu->current_pad],
               g_settings.input.libretro_device
               [driver.menu->current_pad]);

      const char *name = desc ? desc->desc : NULL;
      if (!name)
      {
         /* Find generic name. */

         switch (g_settings.input.libretro_device
               [driver.menu->current_pad])
         {
            case RETRO_DEVICE_NONE:
               name = "None";
               break;
            case RETRO_DEVICE_JOYPAD:
               name = "RetroPad";
               break;
            case RETRO_DEVICE_ANALOG:
               name = "RetroPad w/ Analog";
               break;
            default:
               name = "Unknown";
               break;
         }
      }

      strlcpy(type_str, name, type_str_size);
   }
   else if (!strcmp(label, "input_bind_player_no"))
      snprintf(type_str, type_str_size, "#%u",
            driver.menu->current_pad + 1);
   else if (!strcmp(label, "input_bind_analog_dpad_mode"))
   {
      static const char *modes[] = {
         "None",
         "Left Analog",
         "Right Analog",
         "Dual Analog",
      };

      strlcpy(type_str, modes[g_settings.input.analog_dpad_mode
            [driver.menu->current_pad] % ANALOG_DPAD_LAST],
            type_str_size);
   }
   else if (type >= MENU_SETTINGS_PERF_COUNTERS_BEGIN
         && type <= MENU_SETTINGS_PERF_COUNTERS_END)
      menu_common_setting_set_label_perf(type_str, type_str_size, w, type,
            perf_counters_rarch,
            type - MENU_SETTINGS_PERF_COUNTERS_BEGIN);
   else if (type >= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN
         && type <= MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_END)
      menu_common_setting_set_label_perf(type_str, type_str_size, w, type,
            perf_counters_libretro,
            type - MENU_SETTINGS_LIBRETRO_PERF_COUNTERS_BEGIN);
   else if (setting)
      setting_data_get_string_representation(setting, type_str, type_str_size);
   else
   {
      setting_data = (rarch_setting_t*)setting_data_get_mainmenu(true);

      setting = (rarch_setting_t*)setting_data_find_setting(setting_data,
            driver.menu->selection_buf->list[index].label);

      if (setting)
      {
         if (!strcmp(setting->name, "configurations"))
         {
            if (*g_extern.config_path)
               fill_pathname_base(type_str, g_extern.config_path,
                     type_str_size);
            else
               strlcpy(type_str, "<default>", type_str_size);
         }
         else if (!strcmp(setting->name, "disk_index"))
         {
            const struct retro_disk_control_callback *control =
               (const struct retro_disk_control_callback*)
               &g_extern.system.disk_control;
            unsigned images = control->get_num_images();
            unsigned current = control->get_image_index();
            if (current >= images)
               strlcpy(type_str, "No Disk", type_str_size);
            else
               snprintf(type_str, type_str_size, "%u", current + 1);
         }
         else
            setting_data_get_string_representation(setting, type_str, type_str_size);
      }
      else
      {
         switch (type)
         {
#if defined(GEKKO)
            case MENU_SETTINGS_VIDEO_RESOLUTION:
               strlcpy(type_str, gx_get_video_mode(), type_str_size);
               break;
#elif defined(__CELLOS_LV2__)
            case MENU_SETTINGS_VIDEO_RESOLUTION:
               {
                  unsigned width = gfx_ctx_get_resolution_width(
                        g_extern.console.screen.resolutions.list
                        [g_extern.console.screen.resolutions.current.idx]);
                  unsigned height = gfx_ctx_get_resolution_height(
                        g_extern.console.screen.resolutions.list
                        [g_extern.console.screen.resolutions.current.idx]);
                  snprintf(type_str, type_str_size, "%ux%u", width, height);
               }
               break;
#endif
            case MENU_SETTINGS_CUSTOM_VIEWPORT:
            case MENU_SETTINGS_CUSTOM_BIND_ALL:
            case MENU_SETTINGS_CUSTOM_BIND_DEFAULT_ALL:
               strlcpy(type_str, "...", type_str_size);
               break;
            case MENU_SETTINGS_CUSTOM_BIND_MODE:
               strlcpy(type_str, driver.menu->bind_mode_keyboard ?
                     "RetroKeyboard" : "RetroPad", type_str_size);
               break;
            default:
               *type_str = '\0';
               *w = 0;
               break;
         }
      }
   }
}

static void general_read_handler(const void *data)
{
    const rarch_setting_t *setting = (const rarch_setting_t*)data;
    
    if (!setting)
       return;
    
    if (!strcmp(setting->name, "audio_rate_control_delta"))
    {
       *setting->value.fraction = g_settings.audio.rate_control_delta;
        if (*setting->value.fraction < 0.0005)
        {
            g_settings.audio.rate_control = false;
            g_settings.audio.rate_control_delta = 0.0;
        }
        else
        {
            g_settings.audio.rate_control = true;
            g_settings.audio.rate_control_delta = *setting->value.fraction;
        }
    }
    else if (!strcmp(setting->name, "video_refresh_rate_auto"))
       *setting->value.fraction = g_settings.video.refresh_rate;
    else if (!strcmp(setting->name, "input_player1_joypad_index"))
        *setting->value.integer = g_settings.input.joypad_map[0];
    else if (!strcmp(setting->name, "input_player2_joypad_index"))
        *setting->value.integer = g_settings.input.joypad_map[1];
    else if (!strcmp(setting->name, "input_player3_joypad_index"))
        *setting->value.integer = g_settings.input.joypad_map[2];
    else if (!strcmp(setting->name, "input_player4_joypad_index"))
        *setting->value.integer = g_settings.input.joypad_map[3];
    else if (!strcmp(setting->name, "input_player5_joypad_index"))
        *setting->value.integer = g_settings.input.joypad_map[4];
    else if (!strcmp(setting->name, "log_verbosity"))
        *setting->value.boolean = g_extern.verbosity;
}

static void general_write_handler(const void *data)
{
   unsigned rarch_cmd = RARCH_CMD_NONE;
   const rarch_setting_t *setting = (const rarch_setting_t*)data;

   if (!setting)
      return;

   
   if (!strcmp(setting->name, "quit_retroarch"))
   {
      if (*setting->value.boolean)
      {
         rarch_cmd = RARCH_CMD_QUIT_RETROARCH;
         *setting->value.boolean = false;
      }
   }
   else if (!strcmp(setting->name, "save_new_config"))
   {
      if (*setting->value.boolean)
      {
         rarch_cmd = RARCH_CMD_MENU_SAVE_CONFIG;
         *setting->value.boolean = false;
      }
   }
   else if (!strcmp(setting->name, "restart_retroarch"))
   {
      if (*setting->value.boolean)
      {
         rarch_cmd = RARCH_CMD_RESTART_RETROARCH;
         *setting->value.boolean = false;
      }
   }
   else if (!strcmp(setting->name, "resume_content"))
   {
      if (*setting->value.boolean)
      {
         rarch_cmd = RARCH_CMD_RESUME;
         *setting->value.boolean = false;
      }
   }
   else if (!strcmp(setting->name, "restart_content"))
   {
      if (*setting->value.boolean)
      {
         rarch_cmd = RARCH_CMD_RESET;
         *setting->value.boolean = false;
      }
   }
   else if (!strcmp(setting->name, "take_screenshot"))
   {
      if (*setting->value.boolean)
      {
         rarch_cmd = RARCH_CMD_TAKE_SCREENSHOT;
         *setting->value.boolean = false;
      }
   }
   else if (!strcmp(setting->name, "help"))
   {
      if (*setting->value.boolean)
      {
#ifdef HAVE_MENU
         menu_entries_push(driver.menu->menu_stack, "", "help", 0, 0);
#endif
         *setting->value.boolean = false;
      }
   }
   else if (!strcmp(setting->name, "rewind_enable"))
      rarch_cmd = RARCH_CMD_REWIND;
   else if (!strcmp(setting->name, "soft_filter"))
   {
      if (*setting->value.boolean)
         rarch_cmd = RARCH_CMD_VIDEO_APPLY_STATE_CHANGES;
   }
   else if (!strcmp(setting->name, "video_smooth"))
   {
      if (driver.video_data && driver.video_poke
            && driver.video_poke->set_filtering)
         driver.video_poke->set_filtering(driver.video_data,
               1, g_settings.video.smooth);
   }
   else if (!strcmp(setting->name, "pal60_enable"))
   {
      if (*setting->value.boolean && g_extern.console.screen.pal_enable)
         rarch_cmd = RARCH_CMD_REINIT;
      else
         *setting->value.boolean = false;
   }
   else if (!strcmp(setting->name, "video_monitor_index"))
      rarch_cmd = RARCH_CMD_REINIT;
   else if (!strcmp(setting->name, "video_disable_composition"))
      rarch_cmd = RARCH_CMD_REINIT;
   else if (!strcmp(setting->name, "video_fullscreen"))
      rarch_cmd = RARCH_CMD_REINIT;
   else if (!strcmp(setting->name, "video_rotation"))
   {
      if (driver.video && driver.video->set_rotation)
         driver.video->set_rotation(driver.video_data,
               (*setting->value.unsigned_integer +
                g_extern.system.rotation) % 4);
   }
   else if (!strcmp(setting->name, "video_gamma"))
      rarch_cmd = RARCH_CMD_VIDEO_APPLY_STATE_CHANGES;
   else if (!strcmp(setting->name, "video_threaded"))
      rarch_cmd = RARCH_CMD_REINIT;
   else if (!strcmp(setting->name, "video_swap_interval"))
      rarch_cmd = RARCH_CMD_VIDEO_SET_BLOCKING_STATE;
#ifdef HAVE_OVERLAY
   else if (!strcmp(setting->name, "input_overlay_opacity"))
      rarch_cmd = RARCH_CMD_OVERLAY_SET_ALPHA_MOD;
#endif
   else if (!strcmp(setting->name, "system_bgm_enable"))
   {
      if (*setting->value.boolean)
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
   }
   else if (!strcmp(setting->name, "audio_volume"))
      g_extern.audio_data.volume_gain = db_to_gain(*setting->value.fraction);
   else if (!strcmp(setting->name, "audio_dsp_plugin"))
      rarch_cmd = RARCH_CMD_DSP_FILTER_INIT;
   else if (!strcmp(setting->name, "audio_rate_control_delta"))
   {
      if (*setting->value.fraction < 0.0005)
      {
         g_settings.audio.rate_control = false;
         g_settings.audio.rate_control_delta = 0.0;
      }
      else
      {
         g_settings.audio.rate_control = true;
         g_settings.audio.rate_control_delta = *setting->value.fraction;
      }
   }
   else if (!strcmp(setting->name, "savestate"))
   {
      if (*setting->value.boolean)
      {
         rarch_cmd = RARCH_CMD_SAVE_STATE;
         *setting->value.boolean = false;
      }
   }
   else if (!strcmp(setting->name, "loadstate"))
   {
      if (*setting->value.boolean)
      {
         rarch_cmd = RARCH_CMD_LOAD_STATE;
         *setting->value.boolean = false;
      }
   }
   else if (!strcmp(setting->name, "autosave_interval"))
      rarch_cmd = RARCH_CMD_AUTOSAVE;
#ifdef HAVE_OVERLAY
   else if (!strcmp(setting->name, "input_overlay"))
      rarch_cmd = RARCH_CMD_OVERLAY_REINIT;
   else if (!strcmp(setting->name, "input_overlay_scale"))
      rarch_cmd = RARCH_CMD_OVERLAY_SET_SCALE_FACTOR;
#endif
   else if (!strcmp(setting->name, "video_refresh_rate_auto"))
   {
      if (driver.video && driver.video_data)
      {
         driver_set_monitor_refresh_rate(*setting->value.fraction);

         /* In case refresh rate update forced non-block video. */
         rarch_cmd = RARCH_CMD_VIDEO_SET_BLOCKING_STATE;
      }
   }
   else if (!strcmp(setting->name, "video_scale"))
   {
      g_settings.video.scale = roundf(*setting->value.fraction);

      if (!g_settings.video.fullscreen)
         rarch_cmd = RARCH_CMD_REINIT;
   }
   else if (!strcmp(setting->name, "aspect_ratio_index"))
      rarch_cmd = RARCH_CMD_VIDEO_SET_ASPECT_RATIO;
   else if (!strcmp(setting->name, "input_player1_joypad_index"))
      g_settings.input.joypad_map[0] = *setting->value.integer;
   else if (!strcmp(setting->name, "input_player2_joypad_index"))
      g_settings.input.joypad_map[1] = *setting->value.integer;
   else if (!strcmp(setting->name, "input_player3_joypad_index"))
      g_settings.input.joypad_map[2] = *setting->value.integer;
   else if (!strcmp(setting->name, "input_player4_joypad_index"))
      g_settings.input.joypad_map[3] = *setting->value.integer;
   else if (!strcmp(setting->name, "input_player5_joypad_index"))
      g_settings.input.joypad_map[4] = *setting->value.integer;
   else if (!strcmp(setting->name, "libretro_info_path"))
      rarch_cmd = RARCH_CMD_CORE_INFO_INIT;
   else if (!strcmp(setting->name, "libretro_dir_path"))
      rarch_cmd = RARCH_CMD_CORE_INFO_INIT;
   else if (!strcmp(setting->name, "video_filter"))
      rarch_cmd = RARCH_CMD_REINIT;
#ifdef HAVE_NETPLAY
   else if (!strcmp(setting->name, "netplay_ip_address"))
      g_extern.has_set_netplay_ip_address = (setting->value.string[0] != '\0');
   else if (!strcmp(setting->name, "netplay_mode"))
   {
      if (!g_extern.netplay_is_client)
         *g_extern.netplay_server = '\0';
      g_extern.has_set_netplay_mode = true;
   }
   else if (!strcmp(setting->name, "netplay_spectator_mode_enable"))
   {
      if (g_extern.netplay_is_spectate)
         *g_extern.netplay_server = '\0';
   }
   else if (!strcmp(setting->name, "netplay_delay_frames"))
      g_extern.has_set_netplay_delay_frames = (g_extern.netplay_sync_frames > 0);
#endif
   else if (!strcmp(setting->name, "log_verbosity"))
   {
      g_extern.verbosity         = *setting->value.boolean;
      g_extern.has_set_verbosity = *setting->value.boolean;
   }

   if (rarch_cmd)
      rarch_main_command(rarch_cmd);
}

#define APPEND(VALUE)                                                                   \
   if (index == list_size)                                                              \
   {                                                                                    \
      rarch_setting_t* list_temp = NULL;                                                \
                                                                                        \
      list_size *= 2;                                                                   \
      list_temp = (rarch_setting_t*)realloc(list, sizeof(rarch_setting_t) * list_size); \
                                                                                        \
      if (list_temp)                                                                    \
      {                                                                                 \
         list = list_temp;                                                              \
      }                                                                                 \
      else                                                                              \
      {                                                                                 \
         RARCH_ERR("Settings list reallocation failed.\n");                             \
         free(list);                                                                    \
         list = NULL;                                                                   \
         return NULL;                                                                   \
      }                                                                                 \
   }                                                                                    \
   (list[index++]) = VALUE

#define START_GROUP(NAME)                       { const char *GROUP_NAME = NAME; APPEND(setting_data_group_setting (ST_GROUP, NAME));
#define END_GROUP()                             APPEND(setting_data_group_setting (ST_END_GROUP, 0)); }
#define START_SUB_GROUP(NAME, GROUPNAME)        { const char *SUBGROUP_NAME = NAME; (void)SUBGROUP_NAME; APPEND(setting_data_subgroup_setting (ST_SUB_GROUP, NAME, GROUPNAME));
#define END_SUB_GROUP()                         APPEND(setting_data_group_setting (ST_END_SUB_GROUP, 0)); }
#define CONFIG_BOOL(TARGET, NAME, SHORT, DEF, OFF, ON, GROUP, SUBGROUP, CHANGE_HANDLER, READ_HANDLER)   APPEND(setting_data_bool_setting  (NAME, SHORT, &TARGET, DEF, OFF, ON, GROUP, SUBGROUP, CHANGE_HANDLER, READ_HANDLER));
#define CONFIG_INT(TARGET, NAME, SHORT, DEF, GROUP, SUBGROUP, CHANGE_HANDLER, READ_HANDLER)    APPEND(setting_data_int_setting   (NAME, SHORT, &TARGET, DEF, GROUP, SUBGROUP, CHANGE_HANDLER, READ_HANDLER));
#define CONFIG_UINT(TARGET, NAME, SHORT, DEF, GROUP, SUBGROUP, CHANGE_HANDLER, READ_HANDLER)   APPEND(setting_data_uint_setting  (NAME, SHORT, &TARGET, DEF, GROUP, SUBGROUP, CHANGE_HANDLER, READ_HANDLER));
#define CONFIG_FLOAT(TARGET, NAME, SHORT, DEF, ROUNDING, GROUP, SUBGROUP, CHANGE_HANDLER, READ_HANDLER)  APPEND(setting_data_float_setting (NAME, SHORT, &TARGET, DEF, ROUNDING, GROUP, SUBGROUP, CHANGE_HANDLER, READ_HANDLER));
#define CONFIG_PATH(TARGET, NAME, SHORT, DEF, GROUP, SUBGROUP, CHANGE_HANDLER, READ_HANDLER)   APPEND(setting_data_string_setting(ST_PATH, NAME, SHORT, TARGET, sizeof(TARGET), DEF, "", GROUP, SUBGROUP, CHANGE_HANDLER, READ_HANDLER));
#define CONFIG_DIR(TARGET, NAME, SHORT, DEF, EMPTY, GROUP, SUBGROUP, CHANGE_HANDLER, READ_HANDLER)   APPEND(setting_data_string_setting(ST_DIR, NAME, SHORT, TARGET, sizeof(TARGET), DEF, EMPTY, GROUP, SUBGROUP, CHANGE_HANDLER, READ_HANDLER));
#define CONFIG_STRING(TARGET, NAME, SHORT, DEF, GROUP, SUBGROUP, CHANGE_HANDLER, READ_HANDLER) APPEND(setting_data_string_setting(ST_STRING, NAME, SHORT, TARGET, sizeof(TARGET), DEF, "", GROUP, SUBGROUP, CHANGE_HANDLER, READ_HANDLER));
#define CONFIG_HEX(TARGET, NAME, SHORT, GROUP, SUBGROUP)
#define CONFIG_BIND(TARGET, PLAYER, NAME, SHORT, DEF, GROUP, SUBGROUP) \
   APPEND(setting_data_bind_setting  (NAME, SHORT, &TARGET, PLAYER, DEF, GROUP, SUBGROUP));

#define WITH_FLAGS(FLAGS) (list[index - 1]).flags |= FLAGS;

#define WITH_RANGE(MIN, MAX, STEP, ENFORCE_MINRANGE, ENFORCE_MAXRANGE)    \
   (list[index - 1]).min = MIN; \
   (list[index - 1]).step = STEP; \
   (list[index - 1]).max = MAX; \
   (list[index - 1]).enforce_minrange = ENFORCE_MINRANGE; \
   (list[index - 1]).enforce_maxrange = ENFORCE_MAXRANGE; \
   WITH_FLAGS(SD_FLAG_HAS_RANGE)

#define WITH_VALUES(VALUES) (list[index -1]).values = VALUES;

#ifdef GEKKO
#define MAX_GAMMA_SETTING 2
#else
#define MAX_GAMMA_SETTING 1
#endif

#ifdef HAVE_MENU
rarch_setting_t *setting_data_get_mainmenu(bool regenerate)
{
   int index = 0;
   static rarch_setting_t* list = NULL;
   rarch_setting_t* list_tmp = NULL;
   int list_size = 32;
   static bool lists[32];

   if (list)
   {
      if (!regenerate)
         return list;

      free(list);
      list = NULL;
   }

   list = (rarch_setting_t*)malloc(sizeof(rarch_setting_t) * list_size);
   if (!list)
   {
      RARCH_ERR("setting_data_get_mainmenu list allocation failed.\n");
      return NULL;
   }

   START_GROUP("Main Menu")
      START_SUB_GROUP("State", GROUP_NAME)
#if defined(HAVE_DYNAMIC) || defined(HAVE_LIBRETRO_MANAGEMENT)
      CONFIG_BOOL(lists[0],     "core_list",     "Core",          false, "", "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_PUSH_ACTION)
#endif
      if (g_extern.history)
      {
         CONFIG_BOOL(lists[1],     "history_list",  "Load Content (History)", false, "", "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_PUSH_ACTION)
      }
      if (driver.menu && g_extern.core_info && core_info_list_num_info_files(g_extern.core_info))
      {
         CONFIG_BOOL(lists[2],     "detect_core_list",  "Load Content (Detect Core)", false, "", "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_PUSH_ACTION)
      }
      CONFIG_BOOL(lists[3],     "load_content",  "Load Content", false, "", "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_PUSH_ACTION)
      CONFIG_BOOL(lists[4],     "core_options",  "Core Options", false, "", "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_PUSH_ACTION)
      CONFIG_BOOL(lists[5],     "core_information",  "Core Information", false, "", "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_PUSH_ACTION)
      if (g_extern.main_is_init && !g_extern.libretro_dummy)
      {
         CONFIG_BOOL(lists[6],     "disk_options",  "Core Disk Options", false, "", "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_PUSH_ACTION)
      }
      CONFIG_BOOL(lists[7],     "settings",  "Settings", false, "", "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_PUSH_ACTION)
      if (g_extern.perfcnt_enable)
      {
         CONFIG_BOOL(lists[8],     "performance_counters",  "Performance Counters", false, "", "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_PUSH_ACTION)
      }
      if (g_extern.main_is_init && !g_extern.libretro_dummy)
      {
         CONFIG_BOOL(lists[9],     "savestate",  "Save State", false, "", "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
         CONFIG_BOOL(lists[10],     "loadstate",  "Load State", false, "", "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
         CONFIG_BOOL(lists[11],     "take_screenshot",  "Take Screenshot", false, "", "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_PUSH_ACTION)
         CONFIG_BOOL(lists[12],     "resume_content",  "Resume Content", false, "", "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_PUSH_ACTION)
         CONFIG_BOOL(lists[13],     "restart_content",  "Restart Content", false, "", "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_PUSH_ACTION)
      }
#ifndef HAVE_DYNAMIC
      CONFIG_BOOL(lists[14], "restart_retroarch", "Restart RetroArch", false, "", "",GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_PUSH_ACTION)
#endif
      CONFIG_BOOL(lists[15], "configurations", "Configurations", false, "", "",GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(lists[16], "save_new_config", "Save New Config", false, "", "",GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_PUSH_ACTION)
      CONFIG_BOOL(lists[17], "help", "Help", false, "", "",GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_PUSH_ACTION)
      CONFIG_BOOL(lists[18], "quit_retroarch", "Quit RetroArch", false, "", "",GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_PUSH_ACTION)
      END_SUB_GROUP()
      END_GROUP()

      rarch_setting_t terminator = { ST_NONE };
   APPEND(terminator);

   /* flatten this array to save ourselves some kilobytes */
   list_tmp = (rarch_setting_t*)realloc(list, sizeof(rarch_setting_t) * index);
   if (list_tmp)
   {
      list = list_tmp;
   }
   else
   {
      RARCH_ERR("setting_data_get_mainmenu list flattening failed.\n");
      free(list);
      list = NULL;
   }

   /* do not optimize into return realloc(),
    * list is static and must be written. */
   return (rarch_setting_t*)list;
}
#endif

rarch_setting_t *setting_data_get_list(void)
{
   int i, player, index = 0;
   static rarch_setting_t* list = NULL;
   rarch_setting_t* list_tmp = NULL;
   int list_size = 512;

   if (list)
      return list;

   list = (rarch_setting_t*)malloc(sizeof(rarch_setting_t) * list_size);
   if (!list)
   {
      RARCH_ERR("setting_data_get_list list allocation failed.\n");
      return NULL;
   }

   START_GROUP("Driver Options")
      START_SUB_GROUP("State", GROUP_NAME)
      CONFIG_STRING(g_settings.input.driver,             "input_driver",               "Input Driver",               config_get_default_input(), GROUP_NAME, SUBGROUP_NAME, NULL, NULL)
      CONFIG_STRING(g_settings.video.driver,             "video_driver",               "Video Driver",               config_get_default_video(), GROUP_NAME, SUBGROUP_NAME, NULL, NULL)
#ifdef HAVE_OPENGL
      CONFIG_STRING(g_settings.video.gl_context,         "video_gl_context",           "OpenGL Context Driver",      "", GROUP_NAME, SUBGROUP_NAME, NULL, NULL)
#endif
      CONFIG_STRING(g_settings.audio.driver,             "audio_driver",               "Audio Driver",               config_get_default_audio(), GROUP_NAME, SUBGROUP_NAME, NULL, NULL)
      CONFIG_STRING(g_settings.audio.resampler,          "audio_resampler_driver",     "Audio Resampler Driver",     config_get_default_audio_resampler(), GROUP_NAME, SUBGROUP_NAME, NULL, NULL)
      CONFIG_STRING(g_settings.camera.driver,            "camera_driver",              "Camera Driver",              config_get_default_camera(), GROUP_NAME, SUBGROUP_NAME, NULL, NULL)
      CONFIG_STRING(g_settings.location.driver,          "location_driver",            "Location Driver",            config_get_default_location(), GROUP_NAME, SUBGROUP_NAME, NULL, NULL)
#ifdef HAVE_MENU
      CONFIG_STRING(g_settings.menu.driver,              "menu_driver",                "Menu Driver",                config_get_default_menu(), GROUP_NAME, SUBGROUP_NAME, NULL, NULL)
#endif
      CONFIG_STRING(g_settings.input.joypad_driver,      "input_joypad_driver",        "Joypad Driver",              "", GROUP_NAME, SUBGROUP_NAME, NULL, NULL)
      CONFIG_STRING(g_settings.input.keyboard_layout,    "input_keyboard_layout",      "Keyboard Layout",            "", GROUP_NAME, SUBGROUP_NAME, NULL, NULL)

      END_SUB_GROUP()
      END_GROUP()

      START_GROUP("General Options")
      START_SUB_GROUP("State", GROUP_NAME)
      CONFIG_BOOL(g_extern.verbosity,                      "log_verbosity",        "Logging Verbosity", false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_UINT(g_settings.libretro_log_level,           "libretro_log_level",        "Libretro Logging Level", libretro_log_level, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 3, 1.0, true, true)
      CONFIG_BOOL(g_extern.perfcnt_enable,               "perfcnt_enable",       "Performance Counters", false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.config_save_on_exit,          "config_save_on_exit",        "Configuration Save On Exit", config_save_on_exit, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.core_specific_config,       "core_specific_config",        "Configuration Per-Core", default_core_specific_config, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.load_dummy_on_core_shutdown, "dummy_on_core_shutdown",      "Dummy On Core Shutdown", load_dummy_on_core_shutdown, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.fps_show,                   "fps_show",                   "Show Framerate",             fps_show, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.rewind_enable,              "rewind_enable",              "Rewind",                     rewind_enable, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#if 0
      CONFIG_SIZE(g_settings.rewind_buffer_size,          "rewind_buffer_size",         "Rewind Buffer Size",       rewind_buffer_size, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#endif
      CONFIG_UINT(g_settings.rewind_granularity,         "rewind_granularity",         "Rewind Granularity",         rewind_granularity, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(1, 32768, 1, true, false)
      CONFIG_BOOL(g_settings.block_sram_overwrite,       "block_sram_overwrite",       "SRAM Block overwrite",       block_sram_overwrite, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#ifdef HAVE_THREADS
      CONFIG_UINT(g_settings.autosave_interval,          "autosave_interval",          "SRAM Autosave",          autosave_interval, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 0, 10, true, false)
#endif
      CONFIG_BOOL(g_settings.video.disable_composition,  "video_disable_composition",  "Window Compositing Disable",         disable_composition, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.pause_nonactive,            "pause_nonactive",            "Window Unfocus Pause",       pause_nonactive, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_FLOAT(g_settings.fastforward_ratio,         "fastforward_ratio",          "Maximum Run Speed",         fastforward_ratio, "%.1fx", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 10, 0.1, true, true)
      CONFIG_FLOAT(g_settings.slowmotion_ratio,          "slowmotion_ratio",           "Slow-Motion Ratio",          slowmotion_ratio, "%.1fx", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)       WITH_RANGE(1, 10, 1.0, true, true)
      CONFIG_BOOL(g_settings.savestate_auto_index,       "savestate_auto_index",       "Save State Auto Index",      savestate_auto_index, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.savestate_auto_save,        "savestate_auto_save",        "Auto Save State",            savestate_auto_save, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.savestate_auto_load,        "savestate_auto_load",        "Auto Load State",            savestate_auto_load, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_INT(g_settings.state_slot,                    "state_slot",                 "State Slot",                 0, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()
      START_SUB_GROUP("Miscellaneous", GROUP_NAME)
#if defined(HAVE_NETWORK_CMD) && defined(HAVE_NETPLAY)
      CONFIG_BOOL(g_settings.network_cmd_enable,         "network_cmd_enable",         "Network Commands",           network_cmd_enable, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#if 0
      CONFIG_INT(g_settings.network_cmd_port,            "network_cmd_port",           "Network Command Port",       network_cmd_port, GROUP_NAME, SUBGROUP_NAME, NULL)
#endif
      CONFIG_BOOL(g_settings.stdin_cmd_enable,           "stdin_cmd_enable",           "stdin command",              stdin_cmd_enable, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#endif
      END_SUB_GROUP()
      END_GROUP()

      START_GROUP("Video Options")
      START_SUB_GROUP("State", GROUP_NAME)
      CONFIG_BOOL(g_settings.video.shared_context,  "video_shared_context",  "HW Shared Context Enable",   false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()
      START_SUB_GROUP("Monitor", GROUP_NAME)
      CONFIG_UINT(g_settings.video.monitor_index,        "video_monitor_index",        "Monitor Index",              monitor_index, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 1, 1, true, false)
#if !defined(RARCH_CONSOLE) && !defined(RARCH_MOBILE)
      CONFIG_BOOL(g_settings.video.fullscreen,           "video_fullscreen",           "Use Fullscreen mode",        fullscreen, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#endif
      CONFIG_BOOL(g_settings.video.windowed_fullscreen,  "video_windowed_fullscreen",  "Windowed Fullscreen Mode",   windowed_fullscreen, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_UINT(g_settings.video.fullscreen_x,         "video_fullscreen_x",         "Fullscreen Width",           fullscreen_x, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_UINT(g_settings.video.fullscreen_y,         "video_fullscreen_y",         "Fullscreen Height",          fullscreen_y, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_FLOAT(g_settings.video.refresh_rate,        "video_refresh_rate",         "Refresh Rate",               refresh_rate, "%.3f Hz", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 0, 0.001, true, false)
      CONFIG_FLOAT(g_settings.video.refresh_rate,        "video_refresh_rate_auto",    "Estimated Monitor FPS",      refresh_rate, "%.3f Hz", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()

      START_SUB_GROUP("Aspect", GROUP_NAME)
      CONFIG_BOOL(g_settings.video.force_aspect,         "video_force_aspect",         "Force aspect ratio",         force_aspect, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_FLOAT(g_settings.video.aspect_ratio,        "video_aspect_ratio",         "Aspect Ratio",               aspect_ratio, "%.2f", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.video.aspect_ratio_auto,    "video_aspect_ratio_auto",    "Use Auto Aspect Ratio",      aspect_ratio_auto, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_UINT(g_settings.video.aspect_ratio_idx,     "aspect_ratio_index",         "Aspect Ratio Index",         aspect_ratio_idx, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, LAST_ASPECT_RATIO, 1, true, true)
      END_SUB_GROUP()

      START_SUB_GROUP("Scaling", GROUP_NAME)
#if !defined(RARCH_CONSOLE) && !defined(RARCH_MOBILE)
      CONFIG_FLOAT(g_settings.video.scale,              "video_scale",               "Windowed Scale",                    scale, "%.1fx", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(1.0, 10.0, 1.0, true, true) 
#endif
      CONFIG_BOOL(g_settings.video.scale_integer,        "video_scale_integer",        "Integer Scale",      scale_integer, "OFF", "ON",  GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)

      CONFIG_INT(g_extern.console.screen.viewports.custom_vp.x,         "custom_viewport_x",       "Custom Viewport X",       0, GROUP_NAME, SUBGROUP_NAME, NULL, NULL)
      CONFIG_INT(g_extern.console.screen.viewports.custom_vp.y,         "custom_viewport_y",       "Custom Viewport Y",       0, GROUP_NAME, SUBGROUP_NAME, NULL, NULL)
      CONFIG_UINT(g_extern.console.screen.viewports.custom_vp.width,    "custom_viewport_width",   "Custom Viewport Width",   0, GROUP_NAME, SUBGROUP_NAME, NULL, NULL)
      CONFIG_UINT(g_extern.console.screen.viewports.custom_vp.height,   "custom_viewport_height",  "Custom Viewport Height",  0, GROUP_NAME, SUBGROUP_NAME, NULL, NULL)
#ifdef GEKKO
      CONFIG_UINT(g_settings.video.viwidth,              "video_viwidth",              "Set Screen Width",           video_viwidth, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#endif
      CONFIG_BOOL(g_settings.video.smooth,               "video_smooth",               "Use Bilinear Filtering",     video_smooth, "Point filtering", "Bilinear filtering", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#if defined(__CELLOS_LV2__)
      CONFIG_BOOL(g_extern.console.screen.pal60_enable,               "pal60_enable",               "Use PAL60 Mode",     false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#endif
      CONFIG_UINT(g_settings.video.rotation,             "video_rotation",             "Rotation",                   0, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 3, 1, true, true)
#if defined(HW_RVL) || defined(_XBOX360)
      CONFIG_UINT(g_extern.console.screen.gamma_correction, "video_gamma",             "Gamma",                      0, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, MAX_GAMMA_SETTING, 1, true, true)
#endif
      END_SUB_GROUP()


      START_SUB_GROUP("Synchronization", GROUP_NAME)
#if defined(HAVE_THREADS) && !defined(RARCH_CONSOLE)
      CONFIG_BOOL(g_settings.video.threaded,             "video_threaded",             "Threaded Video",         video_threaded, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#endif
      CONFIG_BOOL(g_settings.video.vsync,                "video_vsync",                "VSync",                      vsync, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_UINT(g_settings.video.swap_interval,        "video_swap_interval",        "VSync Swap Interval",        swap_interval, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)       WITH_RANGE(1, 4, 1, true, true)
      CONFIG_BOOL(g_settings.video.hard_sync,            "video_hard_sync",            "Hard GPU Sync",              hard_sync, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_UINT(g_settings.video.hard_sync_frames,     "video_hard_sync_frames",     "Hard GPU Sync Frames",       hard_sync_frames, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)    WITH_RANGE(0, 3, 1, true, true)
      CONFIG_UINT(g_settings.video.frame_delay,          "video_frame_delay",          "Frame Delay",                frame_delay, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)    WITH_RANGE(0, 15, 1, true, true)
#if !defined(RARCH_MOBILE)
      CONFIG_BOOL(g_settings.video.black_frame_insertion, "video_black_frame_insertion", "Black Frame Insertion",      black_frame_insertion, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#endif
      END_SUB_GROUP()

      START_SUB_GROUP("Miscellaneous", GROUP_NAME)
      CONFIG_BOOL(g_settings.video.post_filter_record,   "video_post_filter_record",   "Post filter record Enable",         post_filter_record, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.video.gpu_record,           "video_gpu_record",           "GPU Record Enable",                 gpu_record, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.video.gpu_screenshot,       "video_gpu_screenshot",       "GPU Screenshot Enable",             gpu_screenshot, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.video.allow_rotate,         "video_allow_rotate",         "Allow rotation",             allow_rotate, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.video.crop_overscan,        "video_crop_overscan",        "Crop Overscan (reload)",     crop_overscan, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#ifndef HAVE_FILTERS_BUILTIN
      CONFIG_PATH(g_settings.video.softfilter_plugin,    "video_filter",               "Software filter",            g_settings.video.filter_dir, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)       WITH_FLAGS(SD_FLAG_ALLOW_EMPTY) WITH_VALUES("filt")
#endif
#if defined(_XBOX1) || defined(HW_RVL)
      CONFIG_BOOL(g_extern.console.softfilter_enable,   "soft_filter",   "Soft Filter Enable",         false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#endif
#ifdef _XBOX1
      CONFIG_UINT(g_settings.video.swap_interval,        "video_filter_flicker",        "Flicker filter",        0, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)       WITH_RANGE(0, 5, 1, true, true)
#endif
      END_SUB_GROUP()

      END_GROUP()

      START_GROUP("Shader Options")
      START_SUB_GROUP("State", GROUP_NAME)
      CONFIG_BOOL(g_settings.video.shader_enable,        "video_shader_enable",        "Enable Shaders",             shader_enable, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, NULL, NULL)
      CONFIG_PATH(g_settings.video.shader_path,          "video_shader",               "Shader",                     "", GROUP_NAME, SUBGROUP_NAME, NULL, NULL)       WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
      END_SUB_GROUP()
      END_GROUP()

      START_GROUP("Font Options")
      START_SUB_GROUP("Messages", GROUP_NAME)
      CONFIG_PATH(g_settings.video.font_path,            "video_font_path",            "Font Path",                  "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)       WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
      CONFIG_FLOAT(g_settings.video.font_size,           "video_font_size",            "OSD Font Size",              font_size, "%.1f", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.video.font_enable,          "video_font_enable",          "OSD Font Enable",            font_enable, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_FLOAT(g_settings.video.msg_pos_x,           "video_message_pos_x",        "Message X Position",         message_pos_offset_x, "%.1f", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_FLOAT(g_settings.video.msg_pos_y,           "video_message_pos_y",        "Message Y Position",         message_pos_offset_y, "%.1f", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      /* message color */
      END_SUB_GROUP()
      END_GROUP()

      START_GROUP("Audio Options")
      START_SUB_GROUP("State", GROUP_NAME)
      CONFIG_BOOL(g_settings.audio.enable,               "audio_enable",               "Audio Enable",                     audio_enable, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_extern.audio_data.mute,              "audio_mute_enable",          "Audio Mute",                 false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_FLOAT(g_settings.audio.volume,              "audio_volume",               "Volume Level",               audio_volume, "%.1f", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(-80, 12, 1.0, true, true)
#ifdef __CELLOS_LV2__
      CONFIG_BOOL(g_extern.console.sound.system_bgm_enable,               "system_bgm_enable",               "System BGM Enable",                     false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#endif
      END_SUB_GROUP()

      START_SUB_GROUP("Synchronization", GROUP_NAME)
      CONFIG_BOOL(g_settings.audio.sync,                 "audio_sync",                 "Audio Sync Enable",                audio_sync, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_UINT(g_settings.audio.latency,              "audio_latency",              "Audio Latency",                    g_defaults.settings.out_latency ? g_defaults.settings.out_latency : out_latency, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_FLOAT(g_settings.audio.rate_control_delta,  "audio_rate_control_delta",   "Audio Rate Control Delta",         rate_control_delta, "%.3f", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 0, 0.001, true, false)
      CONFIG_UINT(g_settings.audio.block_frames,         "audio_block_frames",         "Block Frames",               0, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()

      START_SUB_GROUP("Miscellaneous", GROUP_NAME)
      CONFIG_STRING(g_settings.audio.device,             "audio_device",               "Device",                     "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_ALLOW_INPUT)
      CONFIG_UINT(g_settings.audio.out_rate,             "audio_out_rate",             "Audio Output Rate",          out_rate, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_PATH(g_settings.audio.dsp_plugin,           "audio_dsp_plugin",           "DSP Plugin",                 g_settings.audio.filter_dir, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)          WITH_FLAGS(SD_FLAG_ALLOW_EMPTY) WITH_VALUES("dsp")
      END_SUB_GROUP()
      END_GROUP()

      START_GROUP("Input Options")
      START_SUB_GROUP("State", GROUP_NAME)
      CONFIG_BOOL(g_settings.input.autodetect_enable,    "input_autodetect_enable",    "Autodetect Enable",   input_autodetect_enable, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()

      START_SUB_GROUP("Joypad Mapping", GROUP_NAME)
      /* TODO: input_libretro_device_p%u */
      CONFIG_INT(g_settings.input.joypad_map[0],         "input_player1_joypad_index", "Player 1 Pad Index",         0, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_INT(g_settings.input.joypad_map[1],         "input_player2_joypad_index", "Player 2 Pad Index",         1, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_INT(g_settings.input.joypad_map[2],         "input_player3_joypad_index", "Player 3 Pad Index",         2, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_INT(g_settings.input.joypad_map[3],         "input_player4_joypad_index", "Player 4 Pad Index",         3, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_INT(g_settings.input.joypad_map[4],         "input_player5_joypad_index", "Player 5 Pad Index",         4, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()

      START_SUB_GROUP("Turbo/Deadzone", GROUP_NAME)
      CONFIG_FLOAT(g_settings.input.axis_threshold,      "input_axis_threshold",       "Input Axis Threshold",       axis_threshold, "%.3f", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 1.00, 0.001, true, true)
      CONFIG_UINT(g_settings.input.turbo_period,         "input_turbo_period",         "Turbo Period",               turbo_period, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(1, 0, 1, true, false)
      CONFIG_UINT(g_settings.input.turbo_duty_cycle,     "input_duty_cycle",           "Duty Cycle",                 turbo_duty_cycle, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(1, 0, 1, true, false)
      END_SUB_GROUP()

      /* The second argument to config bind is 1 
       * based for players and 0 only for meta keys. */
      START_SUB_GROUP("Meta Keys", GROUP_NAME)
      for (i = 0; i != RARCH_BIND_LIST_END; i ++)
         if (input_config_bind_map[i].meta)
         {
            const struct input_bind_map* bind = (const struct input_bind_map*)
               &input_config_bind_map[i];
            CONFIG_BIND(g_settings.input.binds[0][i], 0,
                  bind->base, bind->desc, &retro_keybinds_1[i],
                  GROUP_NAME, SUBGROUP_NAME)
         }
   END_SUB_GROUP()

      for (player = 0; player < MAX_PLAYERS; player ++)
      {
         char buffer[PATH_MAX];
         const struct retro_keybind* const defaults =
            (player == 0) ? retro_keybinds_1 : retro_keybinds_rest;
         snprintf(buffer, sizeof(buffer), "Player %d", player + 1);

         START_SUB_GROUP(strdup(buffer), GROUP_NAME)
            for (i = 0; i != RARCH_BIND_LIST_END; i ++)
            {
               if (!input_config_bind_map[i].meta)
               {
                  const struct input_bind_map* bind = 
                     (const struct input_bind_map*)&input_config_bind_map[i];
                  CONFIG_BIND(g_settings.input.binds[player][i], player + 1,
                        bind->base, bind->desc, &defaults[i],
                        GROUP_NAME, SUBGROUP_NAME)
               }
            }
         END_SUB_GROUP()
      }
   START_SUB_GROUP("Onscreen Keyboard", GROUP_NAME)
      CONFIG_BOOL(g_settings.osk.enable, "osk_enable", "Onscreen Keyboard Enable",     false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()

      START_SUB_GROUP("Miscellaneous", GROUP_NAME)
      CONFIG_BOOL(g_settings.input.netplay_client_swap_input, "netplay_client_swap_input", "Swap Netplay Input",     netplay_client_swap_input, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()
      END_GROUP()

#ifdef HAVE_OVERLAY
      START_GROUP("Overlay Options")
      START_SUB_GROUP("State", GROUP_NAME)
      CONFIG_PATH(g_settings.input.overlay,              "input_overlay",              "Overlay Preset",              g_extern.overlay_dir, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_ALLOW_EMPTY) WITH_VALUES("cfg")
      CONFIG_FLOAT(g_settings.input.overlay_opacity,     "input_overlay_opacity",      "Overlay Opacity",            0.7f, "%.2f", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 1, 0.01, true, true)
      CONFIG_FLOAT(g_settings.input.overlay_scale,       "input_overlay_scale",        "Overlay Scale",              1.0f, "%.2f", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 2, 0.01, true, true)
      END_SUB_GROUP()
      END_GROUP()
#endif

#ifdef HAVE_NETPLAY
      START_GROUP("Netplay Options")
      START_SUB_GROUP("State", GROUP_NAME)
      CONFIG_BOOL(g_extern.netplay_enable,            "netplay_enable",  "Netplay Enable",        false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#ifdef HAVE_NETPLAY
      CONFIG_STRING(g_extern.netplay_server,          "netplay_ip_address",   "IP Address",       "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_ALLOW_INPUT)
#endif
      CONFIG_BOOL(g_extern.netplay_is_client,         "netplay_mode",    "Netplay Client Enable",          false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_extern.netplay_is_spectate,       "netplay_spectator_mode_enable",    "Netplay Spectator Enable",          false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_UINT(g_extern.netplay_sync_frames,       "netplay_delay_frames",      "Netplay Delay Frames",      0, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 10, 1, true, false)
      CONFIG_UINT(g_extern.netplay_port,       "netplay_tcp_udp_port",      "Netplay TCP/UDP Port",      RARCH_DEFAULT_PORT, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(1, 99999, 1, true, true) WITH_FLAGS(SD_FLAG_ALLOW_INPUT)
      END_SUB_GROUP()
      END_GROUP()
#endif

      START_GROUP("User Options")
      START_SUB_GROUP("State", GROUP_NAME)
      CONFIG_STRING(g_settings.username,          "netplay_nickname",   "Username",       "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_ALLOW_INPUT)
      CONFIG_UINT(g_settings.user_language,     "user_language",      "Language",       def_user_language, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, RETRO_LANGUAGE_LAST-1, 1, true, true) WITH_FLAGS(SD_FLAG_ALLOW_INPUT)
      END_SUB_GROUP()
      END_GROUP()

      START_GROUP("Path Options")
      START_SUB_GROUP("State", GROUP_NAME)
#ifdef HAVE_MENU
      CONFIG_BOOL(g_settings.menu_show_start_screen,     "rgui_show_start_screen",     "Show Start Screen", menu_show_start_screen, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#endif
      CONFIG_UINT(g_settings.content_history_size,          "game_history_size",          "Content History Size",       default_content_history_size, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 0, 1.0, true, false)
      END_SUB_GROUP()
      START_SUB_GROUP("Paths", GROUP_NAME)
#ifdef HAVE_MENU
      CONFIG_DIR(g_settings.menu_content_directory,     "rgui_browser_directory",     "Browser Directory",          "", "<default>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
      CONFIG_DIR(g_settings.content_directory,     "content_directory",     "Content Directory",          "", "<default>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
      CONFIG_DIR(g_settings.assets_directory,           "assets_directory",           "Assets Directory",           "", "<default>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
      CONFIG_DIR(g_settings.menu_config_directory,      "rgui_config_directory",      "Config Directory",           "", "<default>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)

#endif
      CONFIG_PATH(g_settings.libretro,                   "libretro_path",              "Libretro Path",              "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
      CONFIG_DIR(g_settings.libretro_directory,         "libretro_dir_path",         "Core Directory",              g_defaults.core_dir, "<None>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)   WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
      CONFIG_DIR(g_settings.libretro_info_path,         "libretro_info_path",         "Core Info Directory",        g_defaults.core_info_dir, "<None>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)   WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
      CONFIG_PATH(g_settings.core_options_path,          "core_options_path",          "Core Options Path",          "", "Paths", SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
      CONFIG_PATH(g_settings.cheat_database,             "cheat_database_path",        "Cheat Database",             "", "Paths", SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
      CONFIG_PATH(g_settings.cheat_settings_path,        "cheat_settings_path",        "Cheat Settings",             "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
      CONFIG_PATH(g_settings.content_history_path,          "game_history_path",          "Content History Path",       "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)

      CONFIG_DIR(g_settings.video.filter_dir,         "video_filter_dir",         "VideoFilter Directory",              "", "<default>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)   WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
      CONFIG_DIR(g_settings.audio.filter_dir,         "audio_filter_dir",         "AudioFilter Directory",              "", "<default>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)   WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
#if defined(HAVE_DYLIB) && defined(HAVE_SHADER_MANAGER)
      CONFIG_DIR(g_settings.video.shader_dir,           "video_shader_dir",           "Shader Directory", g_defaults.shader_dir, "<default>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)  WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
#endif

#ifdef HAVE_OVERLAY
      CONFIG_DIR(g_extern.overlay_dir,                  "overlay_directory",          "Overlay Directory", g_defaults.overlay_dir, "<default>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
#endif
      CONFIG_DIR(g_settings.screenshot_directory,       "screenshot_directory",       "Screenshot Directory",       "", "<Content dir>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
      CONFIG_DIR(g_settings.input.autoconfig_dir,       "joypad_autoconfig_dir",      "Joypad Autoconfig Directory", "", "<default>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)          WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
      CONFIG_DIR(g_extern.savefile_dir, "savefile_directory", "Savefile Directory", "", "<Content dir>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler);
   CONFIG_DIR(g_extern.savestate_dir, "savestate_directory", "Savestate Directory", "", "<Content dir>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_DIR(g_settings.system_directory, "system_directory", "System Directory", "", "<Content dir>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_DIR(g_settings.extraction_directory, "extraction_directory", "Extraction Directory", "", "<None>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()
      END_GROUP()

      START_GROUP("Privacy Options")
      START_SUB_GROUP("State", GROUP_NAME)
      CONFIG_BOOL(g_settings.camera.allow,     "camera_allow",     "Allow Camera",          false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.location.allow,     "location_allow",     "Allow Location",          false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()
      END_GROUP()

      rarch_setting_t terminator = { ST_NONE };
   APPEND(terminator);

   /* flatten this array to save ourselves some kilobytes. */
   list_tmp = (rarch_setting_t*)
      realloc(list, sizeof(rarch_setting_t) * index);
   if (list_tmp)
   {
      list = list_tmp;
   }
   else
   {
      RARCH_ERR("setting_data_get_list list flattening failed.\n");
      free(list);
      list = NULL;
   }

   /* do not optimize into return realloc(),
    * list is static and must be written. */
   return (rarch_setting_t*)list;
}
