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

#ifdef APPLE
#include "input/apple_keycode.h"
#endif

static void get_input_config_prefix(char *buf, size_t sizeof_buf,
      const rarch_setting_t *setting)
{
   snprintf(buf, sizeof_buf, "input%cplayer%d", setting->index ? '_' : '\0',
         setting->index);
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
//FIXME - make portable

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

static bool setting_data_load_config(const rarch_setting_t* settings,
      config_file_t* config)
{
   if (!config)
      return false;

   for (; settings->type != ST_NONE; settings++)
   {
      switch (settings->type)
      {
         case ST_BOOL:
            config_get_bool  (config, settings->name, settings->value.boolean);
            break;
         case ST_PATH:
         case ST_DIR:
            config_get_path  (config, settings->name, settings->value.string,
                  settings->size);
            break;
         case ST_STRING:
            config_get_array (config, settings->name, settings->value.string,
                  settings->size);
            break;
         case ST_INT:
            config_get_int(config, settings->name, settings->value.integer);
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
               input_config_parse_key       (config, prefix, settings->name,
                     settings->value.keybind);
               input_config_parse_joy_button(config, prefix, settings->name,
                     settings->value.keybind);
               input_config_parse_joy_axis  (config, prefix, settings->name,
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
            config_set_bool(config, settings->name, *settings->value.boolean);
            break;
         case ST_PATH:
         case ST_DIR:
            config_set_path(config, settings->name, settings->value.string);
            break;
         case ST_STRING:
            config_set_string(config, settings->name, settings->value.string);
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
      if (setting->type <= ST_GROUP && strcmp(setting->name, name) == 0)
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

void setting_data_get_string_representation(const rarch_setting_t* setting,
      char* buf, size_t sizeof_buf)
{
   if (!setting || !buf || !sizeof_buf)
      return;

   switch (setting->type)
   {
      case ST_BOOL:
         snprintf(buf, sizeof_buf, "%s", *setting->value.boolean ? "True" : "False");
         break;
      case ST_INT:
         snprintf(buf, sizeof_buf, "%d", *setting->value.integer);
         break;
      case ST_UINT:
         snprintf(buf, sizeof_buf, "%u", *setting->value.unsigned_integer);
         break;
      case ST_FLOAT:
         snprintf(buf, sizeof_buf, "%f", *setting->value.fraction);
         break;
      case ST_PATH:
      case ST_DIR:
      case ST_STRING:
         strlcpy(buf, setting->value.string, sizeof_buf);
         break;
      case ST_BIND:
         {
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

rarch_setting_t setting_data_group_setting(enum setting_type type, const char* name)
{
   rarch_setting_t result = { type, name };

   result.short_description = name;
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

void setting_data_get_description(const void *data, char *msg,
      size_t sizeof_msg)
{
    const rarch_setting_t *setting = (const rarch_setting_t*)data;
    
    if (!setting)
       return;

    if (!strcmp(setting->name, "input_driver"))
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
    else if (!strcmp(setting->name, "load_content"))
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
    else if (!strcmp(setting->name, "core_list"))
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
    else if (!strcmp(setting->name, "history_list"))
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
    else if (!strcmp(setting->name, "audio_resampler_driver"))
    {
       if (!strcmp(g_settings.audio.resampler, "sinc"))
          snprintf(msg, sizeof_msg,
                " -- Windowed SINC implementation.");
       else if (!strcmp(g_settings.audio.resampler, "CC"))
          snprintf(msg, sizeof_msg,
                " -- Convoluted Cosine implementation.");
    }
    else if (!strcmp(setting->name, "video_driver"))
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
    else if (!strcmp(setting->name, "audio_dsp_plugin"))
       snprintf(msg, sizeof_msg,
             " -- Audio DSP plugin.\n"
             " Processes audio before it's sent to \n"
             "the driver."
             );
    else if (!strcmp(setting->name, "libretro_dir_path"))
       snprintf(msg, sizeof_msg,
             " -- Core Directory. \n"
             " \n"
             "A directory for where to search for \n"
             "libretro core implementations.");
    else if (!strcmp(setting->name, "video_disable_composition"))
       snprintf(msg, sizeof_msg,
             "-- Forcibly disable composition.\n"
             "Only valid on Windows Vista/7 for now.");
    else if (!strcmp(setting->name, "libretro_log_level"))
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
    else if (!strcmp(setting->name, "log_verbosity"))
         snprintf(msg, sizeof_msg,
               "-- Enable or disable verbosity level \n"
               "of frontend.");
    else if (!strcmp(setting->name, "perfcnt_enable"))
         snprintf(msg, sizeof_msg,
               "-- Enable or disable frontend \n"
               "performance counters.");
    else if (!strcmp(setting->name, "system_directory"))
         snprintf(msg, sizeof_msg,
               "-- System Directory. \n"
               " \n"
               "Sets the 'system' directory.\n"
               "Implementations can query for this\n"
               "directory to load BIOSes, \n"
               "system-specific configs, etc.");
    else if (!strcmp(setting->name, "rgui_show_start_screen"))
       snprintf(msg, sizeof_msg,
             " -- Show startup screen in menu.\n"
             "Is automatically set to false when seen\n"
             "for the first time.\n"
             " \n"
             "This is only updated in config if\n"
             "'Config Save On Exit' is set to true.\n");
    else if (!strcmp(setting->name, "config_save_on_exit"))
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
    else if (!strcmp(setting->name, "core_specific_config"))
         snprintf(msg, sizeof_msg,
               " -- Load up a specific config file \n"
               "based on the core being used.\n");
    else if (!strcmp(setting->name, "video_scale"))
         snprintf(msg, sizeof_msg,
               " -- Fullscreen resolution.\n"
               " \n"
               "Resolution of 0 uses the \n"
               "resolution of the environment.\n");
    else if (!strcmp(setting->name, "video_vsync"))
         snprintf(msg, sizeof_msg,
               " -- Video V-Sync.\n");
    else if (!strcmp(setting->name, "video_hard_sync"))
         snprintf(msg, sizeof_msg,
               " -- Attempts to hard-synchronize \n"
               "CPU and GPU.\n"
               " \n"
               "Can reduce latency at cost of \n"
               "performance.");
    else if (!strcmp(setting->name, "video_hard_sync_frames"))
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
    else if (!strcmp(setting->name, "video_frame_delay"))
         snprintf(msg, sizeof_msg,
               " -- Sets how many milliseconds to delay\n"
               "after VSync before running the core.\n"
                "\n"
               "Can reduce latency at cost of\n"
               "performance.\n"
               " \n"
               "Maximum is 15.");
    else if (!strcmp(setting->name, "audio_rate_control_delta"))
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
    else if (!strcmp(setting->name, "video_filter"))
#ifdef HAVE_FILTERS_BUILTIN
       snprintf(msg, sizeof_msg,
             " -- CPU-based video filter.");
#else
    snprintf(msg, sizeof_msg,
          " -- CPU-based video filter.\n"
          " \n"
          "Path to a dynamic library.");
#endif
    else if (!strcmp(setting->name, "video_fullscreen"))
       snprintf(msg, sizeof_msg, " -- Toggles fullscreen.");
    else if (!strcmp(setting->name, "audio_device"))
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
    else if (!strcmp(setting->name, "video_black_frame_insertion"))
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
    else if (!strcmp(setting->name, "video_threaded"))
         snprintf(msg, sizeof_msg,
               " -- Use threaded video driver.\n"
               " \n"
               "Using this might improve performance at \n"
               "possible cost of latency and more video \n"
               "stuttering.");
    else if (!strcmp(setting->name, "video_scale_integer"))
         snprintf(msg, sizeof_msg,
               " -- Only scales video in integer \n"
               "steps.\n"
               " \n"
               "The base size depends on system-reported \n"
               "geometry and aspect ratio.\n"
               " \n"
               "If Force Aspect is not set, X/Y will be \n"
               "integer scaled independently.");
    else if (!strcmp(setting->name, "video_crop_overscan"))
         snprintf(msg, sizeof_msg,
               " -- Forces cropping of overscanned \n"
               "frames.\n"
               " \n"
               "Exact behavior of this option is \n"
               "core-implementation specific.");
    else if (!strcmp(setting->name, "video_monitor_index"))
         snprintf(msg, sizeof_msg,
               " -- Which monitor to prefer.\n"
               " \n"
               "0 (default) means no particular monitor \n"
               "is preferred, 1 and up (1 being first \n"
               "monitor), suggests RetroArch to use that \n"
               "particular monitor.");
    else if (!strcmp(setting->name, "video_rotation"))
         snprintf(msg, sizeof_msg,
               " -- Forces a certain rotation \n"
               "of the screen.\n"
               " \n"
               "The rotation is added to rotations which\n"
               "the libretro core sets (see Video Allow\n"
               "Rotate).");
    else if (!strcmp(setting->name, "audio_volume"))
         snprintf(msg, sizeof_msg,
               " -- Audio volume, expressed in dB.\n"
               " \n"
               " 0 dB is normal volume. No gain will be applied.\n"
               "Gain can be controlled in runtime with Input\n"
               "Volume Up / Input Volume Down.");
    else if (!strcmp(setting->name, "block_sram_overwrite"))
         snprintf(msg, sizeof_msg,
               " -- Block SRAM from being overwritten \n"
               "when loading save states.\n"
               " \n"
               "Might potentially lead to buggy games.");
    else if (!strcmp(setting->name, "fastforward_ratio"))
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
    else if (!strcmp(setting->name, "pause_nonactive"))
         snprintf(msg, sizeof_msg,
               " -- Pause gameplay when window focus \n"
               "is lost.");
    else if (!strcmp(setting->name, "video_gpu_screenshot"))
         snprintf(msg, sizeof_msg,
               " -- Screenshots output of GPU shaded \n"
               "material if available.");
    else if (!strcmp(setting->name, "autosave_interval"))
         snprintf(msg, sizeof_msg,
               " -- Autosaves the non-volatile SRAM \n"
               "at a regular interval.\n"
               " \n"
               "This is disabled by default unless set \n"
               "otherwise. The interval is measured in \n"
               "seconds. \n"
               " \n"
               "A value of 0 disables autosave.");
    else if (!strcmp(setting->name, "screenshot_directory"))
         snprintf(msg, sizeof_msg,
               " -- Screenshot Directory. \n"
               " \n"
               "Directory to dump screenshots to."
               );
    else if (!strcmp(setting->name, "video_swap_interval"))
         snprintf(msg, sizeof_msg,
               " -- VSync Swap Interval.\n"
               " \n"
               "Uses a custom swap interval for VSync. Set this \n"
               "to effectively halve monitor refresh rate.");
    else if (!strcmp(setting->name, "video_refresh_rate_auto"))
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
    else if (!strcmp(setting->name, "savefile_directory"))
         snprintf(msg, sizeof_msg,
               " -- Savefile Directory. \n"
               " \n"
               "Save all save files (*.srm) to this \n"
               "directory. This includes related files like \n"
               ".bsv, .rt, .psrm, etc...\n"
               " \n"
               "This will be overridden by explicit command line\n"
               "options.");
    else if (!strcmp(setting->name, "savestate_directory"))
         snprintf(msg, sizeof_msg,
               " -- Savestate Directory. \n"
               " \n"
               "Save all save states (*.state) to this \n"
               "directory.\n"
               " \n"
               "This will be overridden by explicit command line\n"
               "options.");
    else if (!strcmp(setting->name, "assets_directory"))
         snprintf(msg, sizeof_msg,
               " -- Assets Directory. \n"
               " \n"
               " This location is queried by default when \n"
               "menu interfaces try to look for loadable \n"
               "assets, etc.");
    else if (!strcmp(setting->name, "slowmotion_ratio"))
         snprintf(msg, sizeof_msg,
               " -- Slowmotion ratio."
               " \n"
               "When slowmotion, content will slow\n"
               "down by factor.");
    else if (!strcmp(setting->name, "input_axis_threshold"))
         snprintf(msg, sizeof_msg,
               " -- Defines axis threshold.\n"
               " \n"
               " Possible values are [0.0, 1.0].");
    else if (!strcmp(setting->name, "rewind_granularity"))
         snprintf(msg, sizeof_msg,
               " -- Rewind granularity.\n"
               " \n"
               " When rewinding defined number of \n"
               "frames, you can rewind several frames \n"
               "at a time, increasing the rewinding \n"
               "speed.");
    else if (!strcmp(setting->name, "rewind_enable"))
         snprintf(msg, sizeof_msg,
               " -- Enable rewinding.\n"
               " \n"
               "This will take a performance hit, \n"
               "so it is disabled by default.");
    else if (!strcmp(setting->name, "input_autodetect_enable"))
         snprintf(msg, sizeof_msg,
               " -- Enable input auto-detection.\n"
               " \n"
               "Will attempt to auto-configure \n"
               "joypads, Plug-and-Play style.");
    else if (!strcmp(setting->name, "camera_allow"))
         snprintf(msg, sizeof_msg,
               " -- Allow or disallow camera access by \n"
               "cores.");
    else if (!strcmp(setting->name, "location_allow"))
       snprintf(msg, sizeof_msg,
             " -- Allow or disallow location services \n"
             "access by cores.");
    else if (!strcmp(setting->name, "savestate_auto_save"))
         snprintf(msg, sizeof_msg,
               " -- Automatically saves a savestate at the \n"
               "end of RetroArch's lifetime.\n"
               " \n"
               "RetroArch will automatically load any savestate\n"
               "with this path on startup if 'Savestate Auto\n"
               "Load' is set.");
    else
       snprintf(msg, sizeof_msg,
             "-- No info on this item is available. --\n");
}

static void general_read_handler(const void *data)
{
    const rarch_setting_t *setting = (const rarch_setting_t*)data;
    
    if (!setting)
       return;
    
    if (!strcmp(setting->name, "fps_show"))
       *setting->value.boolean = g_settings.fps_show;
    else if (!strcmp(setting->name, "pause_nonactive"))
       *setting->value.boolean = g_settings.pause_nonactive;
    else if (!strcmp(setting->name, "config_save_on_exit"))
       *setting->value.boolean = g_settings.config_save_on_exit;
    else if (!strcmp(setting->name, "rewind_enable"))
       *setting->value.boolean = g_settings.rewind_enable;
    else if (!strcmp(setting->name, "rewind_granularity"))
         *setting->value.unsigned_integer = g_settings.rewind_granularity;
    else if (!strcmp(setting->name, "block_sram_overwrite"))
        *setting->value.boolean = g_settings.block_sram_overwrite;
#ifdef GEKKO
    else if (!strcmp(setting->name, "video_viwidth"))
        *setting->value.unsigned_integer = g_settings.video.viwidth;
#endif
    else if (!strcmp(setting->name, "video_smooth"))
        *setting->value.boolean = g_settings.video.smooth;
    else if (!strcmp(setting->name, "video_monitor_index"))
        *setting->value.unsigned_integer = g_settings.video.monitor_index;
    else if (!strcmp(setting->name, "video_disable_composition"))
        *setting->value.boolean = g_settings.video.disable_composition;
    else if (!strcmp(setting->name, "video_vsync"))
       *setting->value.boolean = g_settings.video.vsync;
    else if (!strcmp(setting->name, "video_hard_sync"))
        *setting->value.boolean = g_settings.video.hard_sync;
    else if (!strcmp(setting->name, "video_hard_sync_frames"))
        *setting->value.unsigned_integer = g_settings.video.hard_sync_frames;
    else if (!strcmp(setting->name, "video_frame_delay"))
        *setting->value.unsigned_integer = g_settings.video.frame_delay;
    else if (!strcmp(setting->name, "video_scale_integer"))
        *setting->value.boolean = g_settings.video.scale_integer;
    else if (!strcmp(setting->name, "video_fullscreen"))
        *setting->value.boolean = g_settings.video.fullscreen;
    else if (!strcmp(setting->name, "video_rotation"))
         *setting->value.unsigned_integer = g_settings.video.rotation;
    else if (!strcmp(setting->name, "video_gamma"))
         *setting->value.unsigned_integer = g_extern.console.screen.gamma_correction;
    else if (!strcmp(setting->name, "video_threaded"))
        *setting->value.boolean = g_settings.video.threaded;
    else if (!strcmp(setting->name, "video_swap_interval"))
       *setting->value.unsigned_integer = g_settings.video.swap_interval;
    else if (!strcmp(setting->name, "video_crop_overscan"))
        *setting->value.boolean = g_settings.video.crop_overscan;
    else if (!strcmp(setting->name, "video_black_frame_insertion"))
        *setting->value.boolean = g_settings.video.black_frame_insertion;
    else if (!strcmp(setting->name, "video_font_path"))
        strlcpy(setting->value.string, g_settings.video.font_path, setting->size);
    else if (!strcmp(setting->name, "video_font_size"))
        *setting->value.fraction = g_settings.video.font_size;
#ifdef HAVE_OVERLAY
    else if (!strcmp(setting->name, "input_overlay_opacity"))
        *setting->value.fraction = g_settings.input.overlay_opacity;
#endif
    else if (!strcmp(setting->name, "audio_enable"))
        *setting->value.boolean = g_settings.audio.enable;
    else if (!strcmp(setting->name, "audio_sync"))
        *setting->value.boolean = g_settings.audio.sync;
    else if (!strcmp(setting->name, "audio_mute"))
        *setting->value.boolean = g_extern.audio_data.mute;
    else if (!strcmp(setting->name, "audio_volume"))
        *setting->value.fraction = g_extern.audio_data.volume_db;
    else if (!strcmp(setting->name, "audio_device"))
        strlcpy(setting->value.string, g_settings.audio.device, setting->size);
    else if (!strcmp(setting->name, "audio_block_frames"))
        *setting->value.unsigned_integer = g_settings.audio.block_frames;
    else if (!strcmp(setting->name, "audio_latency"))
        *setting->value.unsigned_integer = g_settings.audio.latency;
    else if (!strcmp(setting->name, "audio_dsp_plugin"))
    {
#ifdef HAVE_DYLIB
        strlcpy(setting->value.string, g_settings.audio.dsp_plugin, setting->size);
#endif
    }
    else if (!strcmp(setting->name, "state_slot"))
        *setting->value.integer = g_settings.state_slot;
    else if (!strcmp(setting->name, "audio_rate_control_delta"))
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
    else if (!strcmp(setting->name, "audio_out_rate"))
        *setting->value.unsigned_integer = g_settings.audio.out_rate;
    else if (!strcmp(setting->name, "input_autodetect_enable"))
        *setting->value.boolean = g_settings.input.autodetect_enable;
    else if (!strcmp(setting->name, "input_turbo_period"))
        *setting->value.unsigned_integer = g_settings.input.turbo_period;
    else if (!strcmp(setting->name, "input_duty_cycle"))
        *setting->value.unsigned_integer = g_settings.input.turbo_duty_cycle;
    else if (!strcmp(setting->name, "input_axis_threshold"))
        *setting->value.fraction = g_settings.input.axis_threshold;
    else if (!strcmp(setting->name, "savestate_auto_save"))
        g_settings.savestate_auto_save = *setting->value.boolean;
    else if (!strcmp(setting->name, "savestate_auto_load"))
        g_settings.savestate_auto_load = *setting->value.boolean;
    else if (!strcmp(setting->name, "savestate_auto_index"))
        g_settings.savestate_auto_index = *setting->value.boolean;
    else if (!strcmp(setting->name, "slowmotion_ratio"))
        g_settings.slowmotion_ratio = *setting->value.fraction;
    else if (!strcmp(setting->name, "fastforward_ratio"))
        g_settings.fastforward_ratio = *setting->value.fraction;
    else if (!strcmp(setting->name, "autosave_interval"))
        *setting->value.unsigned_integer = g_settings.autosave_interval;
    else if (!strcmp(setting->name, "video_font_enable"))
        *setting->value.boolean = g_settings.video.font_enable;
    else if (!strcmp(setting->name, "video_gpu_screenshot"))
        *setting->value.boolean = g_settings.video.gpu_screenshot;
#ifdef HAVE_NETPLAY
    else if (!strcmp(setting->name, "netplay_client_swap_input"))
        *setting->value.boolean = g_settings.input.netplay_client_swap_input;
    else if (!strcmp(setting->name, "netplay_tcp_udp_port"))
        *setting->value.unsigned_integer = g_extern.netplay_port;
#endif
#ifdef HAVE_OVERLAY
    else if (!strcmp(setting->name, "input_overlay"))
        strlcpy(setting->value.string, g_settings.input.overlay, setting->size);
    else if (!strcmp(setting->name, "input_overlay_scale"))
       *setting->value.fraction = g_settings.input.overlay_scale;
#endif
    else if (!strcmp(setting->name, "video_allow_rotate"))
        g_settings.video.allow_rotate = *setting->value.boolean;
    else if (!strcmp(setting->name, "video_windowed_fullscreen"))
        *setting->value.boolean = g_settings.video.windowed_fullscreen;
    else if (!strcmp(setting->name, "video_fullscreen_x"))
        *setting->value.unsigned_integer = g_settings.video.fullscreen_x;
    else if (!strcmp(setting->name, "video_fullscreen_y"))
        *setting->value.unsigned_integer = g_settings.video.fullscreen_y;
    else if (!strcmp(setting->name, "video_refresh_rate"))
       *setting->value.fraction = g_settings.video.refresh_rate;
    else if (!strcmp(setting->name, "video_refresh_rate_auto"))
       *setting->value.fraction = g_settings.video.refresh_rate;
    else if (!strcmp(setting->name,  "video_aspect_ratio"))
        *setting->value.fraction = g_settings.video.aspect_ratio;
    else if (!strcmp(setting->name, "video_scale"))
       *setting->value.fraction = g_settings.video.scale;
    else if (!strcmp(setting->name, "video_force_aspect"))
        *setting->value.boolean = g_settings.video.force_aspect;
    else if (!strcmp(setting->name, "aspect_ratio_index"))
        *setting->value.unsigned_integer = g_settings.video.aspect_ratio_idx;
    else if (!strcmp(setting->name, "video_message_pos_x"))
        *setting->value.fraction = g_settings.video.msg_pos_x;
    else if (!strcmp(setting->name, "video_message_pos_y"))
        *setting->value.fraction = g_settings.video.msg_pos_y;
    else if (!strcmp(setting->name, "network_cmd_enable"))
        *setting->value.boolean = g_settings.network_cmd_enable;
    else if (!strcmp(setting->name, "stdin_cmd_enable"))
        *setting->value.boolean = g_settings.stdin_cmd_enable;
    else if (!strcmp(setting->name, "video_post_filter_record"))
        *setting->value.boolean = g_settings.video.post_filter_record;
    else if (!strcmp(setting->name, "video_gpu_record"))
        *setting->value.boolean = g_settings.video.gpu_record;
#ifdef HAVE_OVERLAY
    else if (!strcmp(setting->name, "overlay_directory"))
        strlcpy(setting->value.string, g_extern.overlay_dir, setting->size);
#endif
    else if (!strcmp(setting->name, "joypad_autoconfig_dir"))
        strlcpy(setting->value.string, g_settings.input.autoconfig_dir, setting->size);
    else if (!strcmp(setting->name, "screenshot_directory"))
        strlcpy(setting->value.string, g_settings.screenshot_directory, setting->size);
    else if (!strcmp(setting->name, "savefile_directory"))
        strlcpy(setting->value.string, g_extern.savefile_dir, setting->size);
    else if (!strcmp(setting->name, "savestate_directory"))
        strlcpy(setting->value.string, g_extern.savestate_dir, setting->size);
    else if (!strcmp(setting->name, "system_directory"))
        strlcpy(setting->value.string, g_settings.system_directory,  setting->size);
    else if (!strcmp(setting->name, "extraction_directory"))
        strlcpy(setting->value.string, g_settings.extraction_directory, setting->size);
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
    else if (!strcmp(setting->name, "rgui_show_start_screen"))
        *setting->value.boolean = g_settings.menu_show_start_screen;
    else if (!strcmp(setting->name,  "game_history_size"))
        *setting->value.unsigned_integer = g_settings.content_history_size;
    else if (!strcmp(setting->name, "content_directory"))
        strlcpy(setting->value.string, g_settings.content_directory, setting->size);
#ifdef HAVE_MENU
    else if (!strcmp(setting->name, "rgui_browser_directory"))
        strlcpy(setting->value.string, g_settings.menu_content_directory, setting->size);
    else if (!strcmp(setting->name, "assets_directory"))
        strlcpy(setting->value.string, g_settings.assets_directory, setting->size);
    else if (!strcmp(setting->name, "rgui_config_directory"))
        strlcpy(setting->value.string, g_settings.menu_config_directory,  setting->size);
#endif
    else if (!strcmp(setting->name, "libretro_path"))
        strlcpy(setting->value.string, g_settings.libretro, setting->size);
    else if (!strcmp(setting->name, "libretro_info_path"))
        strlcpy(setting->value.string, g_settings.libretro_info_path, setting->size);
    else if (!strcmp(setting->name, "libretro_dir_path"))
        strlcpy(setting->value.string, g_settings.libretro_directory, setting->size);
    else if (!strcmp(setting->name, "core_options_path"))
        strlcpy(setting->value.string, g_settings.core_options_path, setting->size);
    else if (!strcmp(setting->name, "cheat_database_path"))
        strlcpy(setting->value.string, g_settings.cheat_database,  setting->size);
    else if (!strcmp(setting->name, "cheat_settings_path"))
        strlcpy(setting->value.string, g_settings.cheat_settings_path, setting->size);
    else if (!strcmp(setting->name, "game_history_path"))
        strlcpy(setting->value.string, g_settings.content_history_path, setting->size);
    else if (!strcmp(setting->name, "video_filter_dir"))
        strlcpy(setting->value.string, g_settings.video.filter_dir, setting->size);
    else if (!strcmp(setting->name, "video_filter_flicker"))
       *setting->value.unsigned_integer = g_extern.console.screen.flicker_filter_index;
    else if (!strcmp(setting->name, "audio_filter_dir"))
        strlcpy(setting->value.string, g_settings.audio.filter_dir, setting->size);
    else if (!strcmp(setting->name, "video_shader_dir"))
        strlcpy(setting->value.string, g_settings.video.shader_dir, setting->size);
    else if (!strcmp(setting->name, "video_aspect_ratio_auto"))
        *setting->value.boolean = g_settings.video.aspect_ratio_auto;
    else if (!strcmp(setting->name, "video_filter"))
        strlcpy(setting->value.string, g_settings.video.softfilter_plugin, setting->size);
    else if (!strcmp(setting->name, "camera_allow"))
        *setting->value.boolean = g_settings.camera.allow;
    else if (!strcmp(setting->name, "location_allow"))
        *setting->value.boolean = g_settings.location.allow;
    else if (!strcmp(setting->name, "video_shared_context"))
       *setting->value.boolean = g_settings.video.shared_context;
#ifdef HAVE_NETPLAY
    else if (!strcmp(setting->name, "netplay_enable"))
       *setting->value.boolean = g_extern.netplay_enable;
    else if (!strcmp(setting->name, "netplay_mode"))
       *setting->value.boolean = g_extern.netplay_is_client;
    else if (!strcmp(setting->name, "netplay_spectator_mode_enable"))
       *setting->value.boolean = g_extern.netplay_is_spectate;
    else if (!strcmp(setting->name, "netplay_delay_frames"))
       *setting->value.unsigned_integer = g_extern.netplay_sync_frames;
    else if (!strcmp(setting->name, "netplay_tcp_udp_port"))
       *setting->value.unsigned_integer = g_extern.netplay_port;
    else if (!strcmp(setting->name, "netplay_ip_address"))
       strlcpy(setting->value.string, g_extern.netplay_server, setting->size);
#endif
    else if (!strcmp(setting->name, "log_verbosity"))
        *setting->value.boolean = g_extern.verbosity;
    else if (!strcmp(setting->name, "perfcnt_enable"))
       *setting->value.boolean = g_extern.perfcnt_enable;
    else if (!strcmp(setting->name, "core_specific_config"))
       *setting->value.boolean = g_settings.core_specific_config;
    else if (!strcmp(setting->name, "dummy_on_core_shutdown"))
       *setting->value.boolean = g_settings.load_dummy_on_core_shutdown;
    else if (!strcmp(setting->name, "libretro_log_level"))
       *setting->value.unsigned_integer = g_settings.libretro_log_level;
    else if (!strcmp(setting->name, "osk_enable"))
       *setting->value.boolean = g_settings.osk.enable;
    else if (!strcmp(setting->name, "user_language"))
       *setting->value.unsigned_integer = g_settings.user_language;
    else if (!strcmp(setting->name, "netplay_nickname"))
       strlcpy(setting->value.string, g_settings.username, setting->size);
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
         extern void menu_push_info_screen(void);
         menu_push_info_screen();
#endif
         *setting->value.boolean = false;
      }
   }
   else if (!strcmp(setting->name, "fps_show"))
      g_settings.fps_show = *setting->value.boolean;
   else if (!strcmp(setting->name, "pause_nonactive"))
      g_settings.pause_nonactive = *setting->value.boolean;
   else if (!strcmp(setting->name, "config_save_on_exit"))
      g_settings.config_save_on_exit = *setting->value.boolean;
   else if (!strcmp(setting->name, "rewind_enable"))
   {
      g_settings.rewind_enable = *setting->value.boolean;
      rarch_cmd = RARCH_CMD_REWIND;
   }
   else if (!strcmp(setting->name, "rewind_granularity"))
      g_settings.rewind_granularity = *setting->value.unsigned_integer;
   else if (!strcmp(setting->name, "block_sram_overwrite"))
      g_settings.block_sram_overwrite = *setting->value.boolean;
#ifdef GEKKO
   else if (!strcmp(setting->name, "video_viwidth"))
      g_settings.video.viwidth = *setting->value.unsigned_integer;
#endif
   else if (!strcmp(setting->name, "video_smooth"))
   {
      g_settings.video.smooth = *setting->value.boolean;

      if (driver.video_data && driver.video_poke && driver.video_poke->set_filtering)
         driver.video_poke->set_filtering(driver.video_data, 1, g_settings.video.smooth);
   }
   else if (!strcmp(setting->name, "video_monitor_index"))
   {
      g_settings.video.monitor_index = *setting->value.unsigned_integer;
      rarch_cmd = RARCH_CMD_REINIT;
   }
   else if (!strcmp(setting->name, "video_disable_composition"))
   {
      g_settings.video.disable_composition = *setting->value.boolean;
      rarch_cmd = RARCH_CMD_REINIT;
   }
   else if (!strcmp(setting->name, "video_vsync"))
      g_settings.video.vsync = *setting->value.boolean;
   else if (!strcmp(setting->name, "video_hard_sync"))
      g_settings.video.hard_sync = *setting->value.boolean;
   else if (!strcmp(setting->name, "video_hard_sync_frames"))
      g_settings.video.hard_sync_frames = *setting->value.unsigned_integer;
   else if (!strcmp(setting->name, "video_frame_delay"))
      g_settings.video.frame_delay = *setting->value.unsigned_integer;
   else if (!strcmp(setting->name, "video_scale_integer"))
      g_settings.video.scale_integer = *setting->value.boolean;
   else if (!strcmp(setting->name, "video_fullscreen"))
   {
      g_settings.video.fullscreen = *setting->value.boolean;
      rarch_cmd = RARCH_CMD_REINIT;
   }
   else if (!strcmp(setting->name, "video_rotation"))
   {
      g_settings.video.rotation = *setting->value.unsigned_integer;
      if (driver.video && driver.video->set_rotation)
         driver.video->set_rotation(driver.video_data, (g_settings.video.rotation + g_extern.system.rotation) % 4);
   }
   else if (!strcmp(setting->name, "video_gamma"))
   {
      g_extern.console.screen.gamma_correction = *setting->value.unsigned_integer;
      rarch_cmd = RARCH_CMD_VIDEO_APPLY_STATE_CHANGES;
   }
   else if (!strcmp(setting->name, "video_threaded"))
   {
      g_settings.video.threaded = *setting->value.boolean;
      rarch_cmd = RARCH_CMD_REINIT;
   }
   else if (!strcmp(setting->name, "video_swap_interval"))
   {
      g_settings.video.swap_interval = *setting->value.unsigned_integer;
      rarch_cmd = RARCH_CMD_VIDEO_SET_BLOCKING_STATE;
   }
   else if (!strcmp(setting->name, "video_crop_overscan"))
      g_settings.video.crop_overscan = *setting->value.boolean;
   else if (!strcmp(setting->name, "video_black_frame_insertion"))
      g_settings.video.black_frame_insertion = *setting->value.boolean;
   else if (!strcmp(setting->name, "video_font_path"))
      strlcpy(g_settings.video.font_path, setting->value.string, sizeof(g_settings.video.font_path));
   else if (!strcmp(setting->name, "video_font_size"))
      g_settings.video.font_size = *setting->value.fraction;
#ifdef HAVE_OVERLAY
   else if (!strcmp(setting->name, "input_overlay_opacity"))
   {
      g_settings.input.overlay_opacity = *setting->value.fraction;
      rarch_cmd = RARCH_CMD_OVERLAY_SET_ALPHA_MOD;
   }
#endif
   else if (!strcmp(setting->name, "audio_enable"))
      g_settings.audio.enable = *setting->value.boolean;
   else if (!strcmp(setting->name, "audio_sync"))
      g_settings.audio.sync = *setting->value.boolean;
   else if (!strcmp(setting->name, "audio_mute"))
      g_extern.audio_data.mute = *setting->value.boolean;
   else if (!strcmp(setting->name, "audio_volume"))
   {
      g_extern.audio_data.volume_db = *setting->value.fraction;
      g_extern.audio_data.volume_gain = db_to_gain(g_extern.audio_data.volume_db);
   }
   else if (!strcmp(setting->name, "audio_device"))
      strlcpy(g_settings.audio.device, setting->value.string, sizeof(g_settings.audio.device));
   else if (!strcmp(setting->name, "audio_block_frames"))
      g_settings.audio.block_frames = *setting->value.unsigned_integer;
   else if (!strcmp(setting->name, "audio_latency"))
      g_settings.audio.latency = *setting->value.unsigned_integer;
   else if (!strcmp(setting->name, "audio_dsp_plugin"))
   {
      strlcpy(g_settings.audio.dsp_plugin, setting->value.string, sizeof(g_settings.audio.dsp_plugin));
      rarch_cmd = RARCH_CMD_DSP_FILTER_INIT;
   }
   else if (!strcmp(setting->name, "state_slot"))
      g_settings.state_slot = *setting->value.integer;
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
   else if (!strcmp(setting->name, "audio_out_rate"))
      g_settings.audio.out_rate = *setting->value.unsigned_integer;
   else if (!strcmp(setting->name, "input_autodetect_enable"))
      g_settings.input.autodetect_enable = *setting->value.boolean;
   else if (!strcmp(setting->name, "input_turbo_period"))
      g_settings.input.turbo_period = *setting->value.unsigned_integer;
   else if (!strcmp(setting->name, "input_duty_cycle"))
      g_settings.input.turbo_duty_cycle = *setting->value.unsigned_integer;
   else if (!strcmp(setting->name, "input_axis_threshold"))
      g_settings.input.axis_threshold = max(min(*setting->value.fraction, 0.95f), 0.05f);
   else if (!strcmp(setting->name, "savestate_auto_save"))
      g_settings.savestate_auto_save = *setting->value.boolean;
   else if (!strcmp(setting->name, "savestate_auto_load"))
      g_settings.savestate_auto_load = *setting->value.boolean;
   else if (!strcmp(setting->name, "savestate_auto_index"))
      g_settings.savestate_auto_index = *setting->value.boolean;
   else if (!strcmp(setting->name, "slowmotion_ratio"))
      g_settings.slowmotion_ratio = *setting->value.fraction;
   else if (!strcmp(setting->name, "fastforward_ratio"))
      g_settings.fastforward_ratio = *setting->value.fraction;
   else if (!strcmp(setting->name, "autosave_interval"))
   {
      g_settings.autosave_interval = *setting->value.unsigned_integer;
      rarch_cmd = RARCH_CMD_AUTOSAVE;
   }
   else if (!strcmp(setting->name, "video_font_enable"))
      g_settings.video.font_enable = *setting->value.boolean;
   else if (!strcmp(setting->name, "video_gpu_screenshot"))
      g_settings.video.gpu_screenshot = *setting->value.boolean;
#ifdef HAVE_NETPLAY
   else if (!strcmp(setting->name, "netplay_client_swap_input"))
      g_settings.input.netplay_client_swap_input = *setting->value.boolean;
#endif
#ifdef HAVE_OVERLAY
   else if (!strcmp(setting->name, "input_overlay"))
   {
      strlcpy(g_settings.input.overlay, setting->value.string, sizeof(g_settings.input.overlay));
      rarch_cmd = RARCH_CMD_OVERLAY_REINIT;
   }
   else if (!strcmp(setting->name, "input_overlay_scale"))
   {
      g_settings.input.overlay_scale = *setting->value.fraction;
      rarch_cmd = RARCH_CMD_OVERLAY_SET_SCALE_FACTOR;
   }
#endif
   else if (!strcmp(setting->name, "video_allow_rotate"))
      g_settings.video.allow_rotate = *setting->value.boolean;
   else if (!strcmp(setting->name, "video_windowed_fullscreen"))
      g_settings.video.windowed_fullscreen = *setting->value.boolean;
   else if (!strcmp(setting->name, "video_fullscreen_x"))
      g_settings.video.fullscreen_x = *setting->value.unsigned_integer;
   else if (!strcmp(setting->name, "video_fullscreen_y"))
      g_settings.video.fullscreen_y = *setting->value.unsigned_integer;
    else if (!strcmp(setting->name, "video_refresh_rate"))
       g_settings.video.refresh_rate = *setting->value.fraction;
   else if (!strcmp(setting->name, "video_refresh_rate_auto"))
   {
      if (driver.video && driver.video_data)
      {
         driver_set_monitor_refresh_rate(*setting->value.fraction);

         /* In case refresh rate update forced non-block video. */
         rarch_cmd = RARCH_CMD_VIDEO_SET_BLOCKING_STATE;
      }
   }
   else if (!strcmp(setting->name,  "video_aspect_ratio"))
      g_settings.video.aspect_ratio = *setting->value.fraction;
   else if (!strcmp(setting->name, "video_scale"))
   {
      g_settings.video.scale = roundf(*setting->value.fraction);

      if (!g_settings.video.fullscreen)
         rarch_cmd = RARCH_CMD_REINIT;
   }
   else if (!strcmp(setting->name, "video_force_aspect"))
      g_settings.video.force_aspect = *setting->value.boolean;
   else if (!strcmp(setting->name, "aspect_ratio_index"))
   {
      g_settings.video.aspect_ratio_idx = *setting->value.unsigned_integer;
      rarch_cmd = RARCH_CMD_VIDEO_SET_ASPECT_RATIO;
   }
   else if (!strcmp(setting->name, "video_message_pos_x"))
      g_settings.video.msg_pos_x = *setting->value.fraction;
   else if (!strcmp(setting->name, "video_message_pos_y"))
      g_settings.video.msg_pos_y = *setting->value.fraction;
   else if (!strcmp(setting->name, "network_cmd_enable"))
      g_settings.network_cmd_enable = *setting->value.boolean;
   else if (!strcmp(setting->name, "stdin_cmd_enable"))
      g_settings.stdin_cmd_enable = *setting->value.boolean;
   else if (!strcmp(setting->name, "video_post_filter_record"))
      g_settings.video.post_filter_record = *setting->value.boolean;
   else if (!strcmp(setting->name, "video_gpu_record"))
      g_settings.video.gpu_record = *setting->value.boolean;
#ifdef HAVE_OVERLAY
   else if (!strcmp(setting->name, "overlay_directory"))
      strlcpy(g_extern.overlay_dir, setting->value.string, sizeof(g_extern.overlay_dir));
#endif
   else if (!strcmp(setting->name, "joypad_autoconfig_dir"))
      strlcpy(g_settings.input.autoconfig_dir, setting->value.string, sizeof(g_settings.input.autoconfig_dir));
   else if (!strcmp(setting->name, "screenshot_directory"))
      strlcpy(g_settings.screenshot_directory, setting->value.string, sizeof(g_settings.screenshot_directory));
   else if (!strcmp(setting->name, "savefile_directory"))
      strlcpy(g_extern.savefile_dir, setting->value.string, sizeof(g_extern.savefile_dir));
   else if (!strcmp(setting->name, "savestate_directory"))
      strlcpy(g_extern.savestate_dir, setting->value.string, sizeof(g_extern.savestate_dir));
   else if (!strcmp(setting->name, "system_directory"))
      strlcpy(g_settings.system_directory, setting->value.string, sizeof(g_settings.system_directory));
   else if (!strcmp(setting->name, "extraction_directory"))
      strlcpy(g_settings.extraction_directory, setting->value.string, sizeof(g_settings.extraction_directory));
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
   else if (!strcmp(setting->name, "rgui_show_start_screen"))
      g_settings.menu_show_start_screen = *setting->value.boolean;
   else if (!strcmp(setting->name,  "game_history_size"))
      g_settings.content_history_size = *setting->value.unsigned_integer;
   else if (!strcmp(setting->name, "content_directory"))
      strlcpy(g_settings.content_directory, setting->value.string, sizeof(g_settings.content_directory));
#ifdef HAVE_MENU
   else if (!strcmp(setting->name, "rgui_browser_directory"))
      strlcpy(g_settings.menu_content_directory, setting->value.string, sizeof(g_settings.menu_content_directory));
   else if (!strcmp(setting->name, "assets_directory"))
      strlcpy(g_settings.assets_directory, setting->value.string, sizeof(g_settings.assets_directory));
   else if (!strcmp(setting->name, "rgui_config_directory"))
      strlcpy(g_settings.menu_config_directory, setting->value.string, sizeof(g_settings.menu_config_directory));
#endif
   else if (!strcmp(setting->name, "libretro_path"))
      strlcpy(g_settings.libretro, setting->value.string, sizeof(g_settings.libretro));
   else if (!strcmp(setting->name, "libretro_info_path"))
   {
      strlcpy(g_settings.libretro_info_path, setting->value.string, sizeof(g_settings.libretro_info_path));
      rarch_cmd = RARCH_CMD_CORE_INFO_INIT;
   }
   else if (!strcmp(setting->name, "libretro_dir_path"))
   {
      strlcpy(g_settings.libretro_directory, setting->value.string, sizeof(g_settings.libretro_directory));
      rarch_cmd = RARCH_CMD_CORE_INFO_INIT;
   }
   else if (!strcmp(setting->name, "core_options_path"))
      strlcpy(g_settings.core_options_path, setting->value.string, sizeof(g_settings.core_options_path));
   else if (!strcmp(setting->name, "cheat_database_path"))
      strlcpy(g_settings.cheat_database, setting->value.string, sizeof(g_settings.cheat_database));
   else if (!strcmp(setting->name, "cheat_settings_path"))
      strlcpy(g_settings.cheat_settings_path, setting->value.string, sizeof(g_settings.cheat_settings_path));
   else if (!strcmp(setting->name, "game_history_path"))
      strlcpy(g_settings.content_history_path, setting->value.string, sizeof(g_settings.content_history_path));
   else if (!strcmp(setting->name, "video_filter_dir"))
      strlcpy(g_settings.video.filter_dir, setting->value.string, sizeof(g_settings.video.filter_dir));
   else if (!strcmp(setting->name, "netplay_nickname"))
      strlcpy(g_settings.username, setting->value.string, sizeof(g_settings.username));
   else if (!strcmp(setting->name, "audio_filter_dir"))
      strlcpy(g_settings.audio.filter_dir, setting->value.string, sizeof(g_settings.audio.filter_dir));
   else if (!strcmp(setting->name, "video_shader_dir"))
      strlcpy(g_settings.video.shader_dir, setting->value.string, sizeof(g_settings.video.shader_dir));
   else if (!strcmp(setting->name, "video_aspect_ratio_auto"))
      g_settings.video.aspect_ratio_auto = *setting->value.boolean;
   else if (!strcmp(setting->name, "video_filter"))
   {
      strlcpy(g_settings.video.softfilter_plugin, setting->value.string, sizeof(g_settings.video.softfilter_plugin));
      rarch_cmd = RARCH_CMD_REINIT;
   }
   else if (!strcmp(setting->name, "video_filter_flicker"))
      g_extern.console.screen.flicker_filter_index = *setting->value.unsigned_integer;
   else if (!strcmp(setting->name, "camera_allow"))
      g_settings.camera.allow = *setting->value.boolean;
   else if (!strcmp(setting->name, "location_allow"))
      g_settings.location.allow = *setting->value.boolean;
   else if (!strcmp(setting->name, "video_shared_context"))
      g_settings.video.shared_context = *setting->value.boolean;
#ifdef HAVE_NETPLAY
   else if (!strcmp(setting->name, "netplay_ip_address"))
      strlcpy(g_extern.netplay_server, setting->value.string, sizeof(g_extern.netplay_server));
   else if (!strcmp(setting->name, "netplay_enable"))
      g_extern.netplay_enable = *setting->value.boolean;
   else if (!strcmp(setting->name, "netplay_mode"))
   {
      g_extern.netplay_is_client = *setting->value.boolean;
      if (!g_extern.netplay_is_client)
         *g_extern.netplay_server = '\0';
   }
   else if (!strcmp(setting->name, "netplay_spectator_mode_enable"))
   {
      g_extern.netplay_is_spectate = *setting->value.boolean;
      if (g_extern.netplay_is_spectate)
         *g_extern.netplay_server = '\0';
   }
   else if (!strcmp(setting->name, "netplay_delay_frames"))
   {
      g_extern.netplay_sync_frames = *setting->value.unsigned_integer;
   }
#endif
   else if (!strcmp(setting->name, "log_verbosity"))
      g_extern.verbosity = *setting->value.boolean;
   else if (!strcmp(setting->name, "perfcnt_enable"))
      g_extern.perfcnt_enable = *setting->value.boolean;
   else if (!strcmp(setting->name, "core_specific_config"))
      g_settings.core_specific_config = *setting->value.boolean;
   else if (!strcmp(setting->name, "dummy_on_core_shutdown"))
      g_settings.load_dummy_on_core_shutdown = *setting->value.boolean;
   else if (!strcmp(setting->name, "libretro_log_level"))
      g_settings.libretro_log_level = *setting->value.unsigned_integer;
   else if (!strcmp(setting->name, "osk_enable"))
      g_settings.osk.enable = *setting->value.boolean;
   else if (!strcmp(setting->name, "user_language"))
      g_settings.user_language = *setting->value.unsigned_integer;

   if (rarch_cmd)
      rarch_main_command(rarch_cmd);
}

#define APPEND(VALUE) if (index == list_size) { list_size *= 2; list = (rarch_setting_t*)realloc(list, sizeof(rarch_setting_t) * list_size); } (list[index++]) = VALUE
#define START_GROUP(NAME)                       { const char *GROUP_NAME = NAME; APPEND(setting_data_group_setting (ST_GROUP, NAME));
#define END_GROUP()                             APPEND(setting_data_group_setting (ST_END_GROUP, 0)); }
#define START_SUB_GROUP(NAME)                   { const char *SUBGROUP_NAME = NAME; APPEND(setting_data_group_setting (ST_SUB_GROUP, NAME));
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
   int list_size = 32;
   static bool lists[32];

   if (list)
   {
      if (regenerate)
      {
         free(list);
         list = NULL;
      }
      else
         return list;
   }

   list = (rarch_setting_t*)malloc(sizeof(rarch_setting_t) * list_size);

   START_GROUP("Main Menu")
      START_SUB_GROUP("State")
#if defined(HAVE_DYNAMIC) || defined(HAVE_LIBRETRO_MANAGEMENT)
      CONFIG_BOOL(lists[0],     "core_list",     "Core",          false, "...", "...", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#endif
      if (g_extern.history)
      {
         CONFIG_BOOL(lists[1],     "history_list",  "Load Content (History)", false, "...", "...", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      }
      if (driver.menu && driver.menu->core_info && core_info_list_num_info_files(driver.menu->core_info))
      {
         CONFIG_BOOL(lists[2],     "detect_core_list",  "Load Content (Detect Core)", false, "...", "...", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      }
      CONFIG_BOOL(lists[3],     "load_content",  "Load Content", false, "...", "...", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(lists[4],     "core_options",  "Core Options", false, "...", "...", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(lists[5],     "core_information",  "Core Information", false, "...", "...", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(lists[6],     "settings",  "Settings", false, "...", "...", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      if (g_extern.perfcnt_enable)
      {
         CONFIG_BOOL(lists[7],     "performance_counters",  "Performance Counters", false, "...", "...", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      }
      if (g_extern.main_is_init && !g_extern.libretro_dummy)
      {
         CONFIG_BOOL(lists[8],     "savestate",  "Save State", false, "...", "...", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
         CONFIG_BOOL(lists[9],     "loadstate",  "Load State", false, "...", "...", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
         CONFIG_BOOL(lists[10],     "take_screenshot",  "Take Screenshot", false, "", "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
         CONFIG_BOOL(lists[11],     "resume_content",  "Resume Content", false, "", "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
         CONFIG_BOOL(lists[12],     "restart_content",  "Restart Content", false, "", "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      }
#ifndef HAVE_DYNAMIC
      CONFIG_BOOL(lists[13], "restart_retroarch", "Restart RetroArch", false, "", "",GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#endif
      CONFIG_BOOL(lists[14], "configurations", "Configurations", false, "", "",GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(lists[15], "save_new_config", "Save New Config", false, "", "",GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(lists[16], "help", "Help", false, "", "",GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(lists[17], "quit_retroarch", "Quit RetroArch", false, "", "",GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()
      END_GROUP()

      rarch_setting_t terminator = { ST_NONE };
   APPEND(terminator);

   /* flatten this array to save ourselves some kilobytes */
   return (rarch_setting_t*)realloc(list, sizeof(rarch_setting_t) * index); 
}
#endif

rarch_setting_t *setting_data_get_list(void)
{
   int i, player, index = 0;
   static rarch_setting_t* list = NULL;
   int list_size = 512;

   if (list)
      return list;

   list = (rarch_setting_t*)malloc(sizeof(rarch_setting_t) * list_size);

   /***********/
   /* DRIVERS */
   /***********/
   START_GROUP("Driver Options")
      START_SUB_GROUP("State")
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

      /*******************/
      /* General Options */
      /*******************/
      START_GROUP("General Options")
      START_SUB_GROUP("General Options")
      CONFIG_BOOL(g_extern.verbosity,                      "log_verbosity",        "Logging Verbosity", false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_UINT(g_settings.libretro_log_level,           "libretro_log_level",        "Libretro Logging Level", libretro_log_level, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 3, 1.0, true, true)
      CONFIG_BOOL(g_extern.perfcnt_enable,               "perfcnt_enable",       "Performance Counters", false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.config_save_on_exit,          "config_save_on_exit",        "Configuration Save On Exit", config_save_on_exit, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.core_specific_config,       "core_specific_config",        "Configuration Per-Core", default_core_specific_config, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.load_dummy_on_core_shutdown, "dummy_on_core_shutdown",      "Dummy On Core Shutdown", load_dummy_on_core_shutdown, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.fps_show,                   "fps_show",                   "Show Framerate",             fps_show, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.rewind_enable,              "rewind_enable",              "Rewind",                     rewind_enable, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      //CONFIG_SIZE(g_settings.rewind_buffer_size,          "rewind_buffer_size",         "Rewind Buffer Size",       rewind_buffer_size, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
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
      START_SUB_GROUP("Miscellaneous")
      CONFIG_BOOL(g_settings.network_cmd_enable,         "network_cmd_enable",         "Network Commands",           network_cmd_enable, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      //CONFIG_INT(g_settings.network_cmd_port,            "network_cmd_port",           "Network Command Port",       network_cmd_port, GROUP_NAME, SUBGROUP_NAME, NULL)
      CONFIG_BOOL(g_settings.stdin_cmd_enable,           "stdin_cmd_enable",           "stdin command",              stdin_cmd_enable, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()
      END_GROUP()

      /*********/
      /* VIDEO */
      /*********/
      START_GROUP("Video Options")
      START_SUB_GROUP("State")
      CONFIG_BOOL(g_settings.video.shared_context,  "video_shared_context",  "HW Shared Context Enable",   false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()
      START_SUB_GROUP("Monitor")
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

      START_SUB_GROUP("Aspect")
      CONFIG_BOOL(g_settings.video.force_aspect,         "video_force_aspect",         "Force aspect ratio",         force_aspect, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_FLOAT(g_settings.video.aspect_ratio,        "video_aspect_ratio",         "Aspect Ratio",               aspect_ratio, "%.2f", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.video.aspect_ratio_auto,    "video_aspect_ratio_auto",    "Use Auto Aspect Ratio",      aspect_ratio_auto, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_UINT(g_settings.video.aspect_ratio_idx,     "aspect_ratio_index",         "Aspect Ratio Index",         aspect_ratio_idx, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, LAST_ASPECT_RATIO, 1, true, true)
      END_SUB_GROUP()

      START_SUB_GROUP("Scaling")
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
      CONFIG_UINT(g_settings.video.rotation,             "video_rotation",             "Rotation",                   0, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 3, 1, true, true)
#if defined(HW_RVL) || defined(_XBOX360)
      CONFIG_UINT(g_extern.console.screen.gamma_correction, "video_gamma",             "Gamma",                      0, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, MAX_GAMMA_SETTING, 1, true, true)
#endif
      END_SUB_GROUP()


      START_SUB_GROUP("Synchronization")
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

      START_SUB_GROUP("Miscellaneous")
      CONFIG_BOOL(g_settings.video.post_filter_record,   "video_post_filter_record",   "Post filter record Enable",         post_filter_record, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.video.gpu_record,           "video_gpu_record",           "GPU Record Enable",                 gpu_record, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.video.gpu_screenshot,       "video_gpu_screenshot",       "GPU Screenshot Enable",             gpu_screenshot, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.video.allow_rotate,         "video_allow_rotate",         "Allow rotation",             allow_rotate, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.video.crop_overscan,        "video_crop_overscan",        "Crop Overscan (reload)",     crop_overscan, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#ifndef HAVE_FILTERS_BUILTIN
      CONFIG_PATH(g_settings.video.softfilter_plugin,    "video_filter",               "Software filter",            g_settings.video.filter_dir, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)       WITH_FLAGS(SD_FLAG_ALLOW_EMPTY) WITH_VALUES("filt")
#endif
#ifdef _XBOX1
      CONFIG_UINT(g_settings.video.swap_interval,        "video_filter_flicker",        "Flicker filter",        0, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)       WITH_RANGE(0, 5, 1, true, true)
#endif
      END_SUB_GROUP()

      END_GROUP()

      START_GROUP("Shader Options")
      START_SUB_GROUP("State")
      CONFIG_BOOL(g_settings.video.shader_enable,        "video_shader_enable",        "Enable Shaders",             shader_enable, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, NULL, NULL)
      CONFIG_PATH(g_settings.video.shader_path,          "video_shader",               "Shader",                     "", GROUP_NAME, SUBGROUP_NAME, NULL, NULL)       WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
      END_SUB_GROUP()
      END_GROUP()

      START_GROUP("Font Options")
      START_SUB_GROUP("Messages")
      CONFIG_PATH(g_settings.video.font_path,            "video_font_path",            "Font Path",                  "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)       WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
      CONFIG_FLOAT(g_settings.video.font_size,           "video_font_size",            "OSD Font Size",              font_size, "%.1f", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.video.font_enable,          "video_font_enable",          "OSD Font Enable",            font_enable, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_FLOAT(g_settings.video.msg_pos_x,           "video_message_pos_x",        "Message X Position",         message_pos_offset_x, "%.1f", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_FLOAT(g_settings.video.msg_pos_y,           "video_message_pos_y",        "Message Y Position",         message_pos_offset_y, "%.1f", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      /* message color */
      END_SUB_GROUP()
      END_GROUP()

      /*********/
      /* AUDIO */
      /*********/
      START_GROUP("Audio Options")
      START_SUB_GROUP("State")
      CONFIG_BOOL(g_settings.audio.enable,               "audio_enable",               "Audio Enable",                     audio_enable, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_extern.audio_data.mute,              "audio_mute",                 "Audio Mute",                 false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_FLOAT(g_settings.audio.volume,              "audio_volume",               "Volume Level",               audio_volume, "%.1f", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(-80, 12, 1.0, true, true)
      END_SUB_GROUP()

      START_SUB_GROUP("Synchronization")
      CONFIG_BOOL(g_settings.audio.sync,                 "audio_sync",                 "Audio Sync Enable",                audio_sync, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_UINT(g_settings.audio.latency,              "audio_latency",              "Audio Latency",                    g_defaults.settings.out_latency ? g_defaults.settings.out_latency : out_latency, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_FLOAT(g_settings.audio.rate_control_delta,  "audio_rate_control_delta",   "Audio Rate Control Delta",         rate_control_delta, "%.3f", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 0, 0.001, true, false)
      CONFIG_UINT(g_settings.audio.block_frames,         "audio_block_frames",         "Block Frames",               0, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()

      START_SUB_GROUP("Miscellaneous")
      CONFIG_STRING(g_settings.audio.device,             "audio_device",               "Device",                     "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_UINT(g_settings.audio.out_rate,             "audio_out_rate",             "Audio Output Rate",          out_rate, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_PATH(g_settings.audio.dsp_plugin,           "audio_dsp_plugin",           "DSP Plugin",                 g_settings.audio.filter_dir, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)          WITH_FLAGS(SD_FLAG_ALLOW_EMPTY) WITH_VALUES("dsp")
      END_SUB_GROUP()
      END_GROUP()

      /*********/
      /* INPUT */
      /*********/
      START_GROUP("Input Options")
      START_SUB_GROUP("State")
      CONFIG_BOOL(g_settings.input.autodetect_enable,    "input_autodetect_enable",    "Autodetect Enable",   input_autodetect_enable, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()

      START_SUB_GROUP("Joypad Mapping")
      //TODO: input_libretro_device_p%u
      CONFIG_INT(g_settings.input.joypad_map[0],         "input_player1_joypad_index", "Player 1 Pad Index",         0, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_INT(g_settings.input.joypad_map[1],         "input_player2_joypad_index", "Player 2 Pad Index",         1, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_INT(g_settings.input.joypad_map[2],         "input_player3_joypad_index", "Player 3 Pad Index",         2, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_INT(g_settings.input.joypad_map[3],         "input_player4_joypad_index", "Player 4 Pad Index",         3, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_INT(g_settings.input.joypad_map[4],         "input_player5_joypad_index", "Player 5 Pad Index",         4, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()

      START_SUB_GROUP("Turbo/Deadzone")
      CONFIG_FLOAT(g_settings.input.axis_threshold,      "input_axis_threshold",       "Input Axis Threshold",       axis_threshold, "%.3f", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_UINT(g_settings.input.turbo_period,         "input_turbo_period",         "Turbo Period",               turbo_period, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_UINT(g_settings.input.turbo_duty_cycle,     "input_duty_cycle",           "Duty Cycle",                 turbo_duty_cycle, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()

      // The second argument to config bind is 1 based for players and 0 only for meta keys
      START_SUB_GROUP("Meta Keys")
      for (i = 0; i != RARCH_BIND_LIST_END; i ++)
         if (input_config_bind_map[i].meta)
         {
            const struct input_bind_map* bind = &input_config_bind_map[i];
            CONFIG_BIND(g_settings.input.binds[0][i], 0, bind->base, bind->desc, &retro_keybinds_1[i], GROUP_NAME, SUBGROUP_NAME)
         }
   END_SUB_GROUP()

      for (player = 0; player < MAX_PLAYERS; player ++)
      {
         char buffer[32];
         const struct retro_keybind* const defaults = (player == 0) ? retro_keybinds_1 : retro_keybinds_rest;

         snprintf(buffer, 32, "Player %d", player + 1);
         START_SUB_GROUP(strdup(buffer))
            for (i = 0; i != RARCH_BIND_LIST_END; i ++)
            {
               if (!input_config_bind_map[i].meta)
               {
                  const struct input_bind_map* bind = (const struct input_bind_map*)&input_config_bind_map[i];
                  CONFIG_BIND(g_settings.input.binds[player][i], player + 1, bind->base, bind->desc, &defaults[i], GROUP_NAME, SUBGROUP_NAME)
               }
            }
         END_SUB_GROUP()
      }
   START_SUB_GROUP("Onscreen Keyboard")
      CONFIG_BOOL(g_settings.osk.enable, "osk_enable", "Onscreen Keyboard Enable",     false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()

      START_SUB_GROUP("Miscellaneous")
      CONFIG_BOOL(g_settings.input.netplay_client_swap_input, "netplay_client_swap_input", "Swap Netplay Input",     netplay_client_swap_input, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()
      END_GROUP()

#ifdef HAVE_OVERLAY
      /*******************/
      /* OVERLAY OPTIONS */
      /*******************/
      START_GROUP("Overlay Options")
      START_SUB_GROUP("State")
      CONFIG_PATH(g_settings.input.overlay,              "input_overlay",              "Overlay Preset",              g_extern.overlay_dir, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_ALLOW_EMPTY) WITH_VALUES("cfg")
      CONFIG_FLOAT(g_settings.input.overlay_opacity,     "input_overlay_opacity",      "Overlay Opacity",            0.7f, "%.2f", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 1, 0.01, true, true)
      CONFIG_FLOAT(g_settings.input.overlay_scale,       "input_overlay_scale",        "Overlay Scale",              1.0f, "%.2f", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 2, 0.01, true, true)
      END_SUB_GROUP()
      END_GROUP()
#endif

#ifdef HAVE_NETPLAY
      /*******************/
      /* NETPLAY OPTIONS */
      /*******************/
      START_GROUP("Netplay Options")
      START_SUB_GROUP("State")
      CONFIG_BOOL(g_extern.netplay_enable,            "netplay_enable",  "Netplay Enable",        false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#ifdef HAVE_NETPLAY
      CONFIG_STRING(g_extern.netplay_server,          "netplay_ip_address",   "IP Address",       "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#endif
      CONFIG_BOOL(g_extern.netplay_is_client,         "netplay_mode",    "Netplay Client Enable",          false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_extern.netplay_is_spectate,       "netplay_spectator_mode_enable",    "Netplay Spectator Enable",          false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_UINT(g_extern.netplay_sync_frames,       "netplay_delay_frames",      "Netplay Delay Frames",      0, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 10, 1, true, false)
      CONFIG_UINT(g_extern.netplay_port,       "netplay_tcp_udp_port",      "Netplay TCP/UDP Port",      RARCH_DEFAULT_PORT, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(1, 99999, 1, true, true)
      END_SUB_GROUP()
      END_GROUP()
#endif

      /*******************/
      /* USER OPTIONS */
      /*******************/
      START_GROUP("User Options")
      START_SUB_GROUP("State")
      CONFIG_STRING(g_settings.username,          "netplay_nickname",   "Username",       "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_UINT(g_settings.user_language,     "user_language",      "Language",       def_user_language, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, RETRO_LANGUAGE_LAST-1, 1, true, true)
      END_SUB_GROUP()
      END_GROUP()

      /*********/
      /* PATHS */
      /*********/
      START_GROUP("Path Options")
      START_SUB_GROUP("State")
#ifdef HAVE_MENU
      CONFIG_BOOL(g_settings.menu_show_start_screen,     "rgui_show_start_screen",     "Show Start Screen", menu_show_start_screen, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
#endif
      CONFIG_UINT(g_settings.content_history_size,          "game_history_size",          "Content History Size",       default_content_history_size, GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_RANGE(0, 0, 1.0, true, false)
      END_SUB_GROUP()
      START_SUB_GROUP("Paths")
#ifdef HAVE_MENU
      CONFIG_DIR(g_settings.menu_content_directory,     "rgui_browser_directory",     "Browser Directory",          "", "<default>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
      CONFIG_DIR(g_settings.content_directory,     "content_directory",     "Content Directory",          "", "<default>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
      CONFIG_DIR(g_settings.assets_directory,           "assets_directory",           "Assets Directory",           "", "<default>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
      CONFIG_DIR(g_settings.menu_config_directory,      "rgui_config_directory",      "Config Directory",           "", "<default>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)

#endif
      CONFIG_PATH(g_settings.libretro,                   "libretro_path",              "Libretro Path",              "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
      CONFIG_DIR(g_settings.libretro_directory,         "libretro_dir_path",         "Core Directory",              g_defaults.core_dir ? g_defaults.core_dir : "", "<None>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)   WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
      CONFIG_DIR(g_settings.libretro_info_path,         "libretro_info_path",         "Core Info Directory",        g_defaults.core_info_dir ? g_defaults.core_info_dir : "", "<None>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)   WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
      CONFIG_PATH(g_settings.core_options_path,          "core_options_path",          "Core Options Path",          "", "Paths", SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
      CONFIG_PATH(g_settings.cheat_database,             "cheat_database_path",        "Cheat Database",             "", "Paths", SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
      CONFIG_PATH(g_settings.cheat_settings_path,        "cheat_settings_path",        "Cheat Settings",             "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)
      CONFIG_PATH(g_settings.content_history_path,          "game_history_path",          "Content History Path",       "", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY)

      CONFIG_DIR(g_settings.video.filter_dir,         "video_filter_dir",         "VideoFilter Directory",              "", "<default>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)   WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
      CONFIG_DIR(g_settings.audio.filter_dir,         "audio_filter_dir",         "AudioFilter Directory",              "", "<default>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)   WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
#if defined(HAVE_DYLIB) && defined(HAVE_SHADER_MANAGER)
      CONFIG_DIR(g_settings.video.shader_dir,           "video_shader_dir",           "Shader Directory", g_defaults.shader_dir ? g_defaults.shader_dir : "", "<default>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)  WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
#endif

#ifdef HAVE_OVERLAY
      CONFIG_DIR(g_extern.overlay_dir,                  "overlay_directory",          "Overlay Directory", g_defaults.overlay_dir ? g_defaults.overlay_dir : "", "<default>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler) WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
#endif
      CONFIG_DIR(g_settings.screenshot_directory,       "screenshot_directory",       "Screenshot Directory",       "", "<Content dir>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)                WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
      CONFIG_DIR(g_settings.input.autoconfig_dir,       "joypad_autoconfig_dir",      "Joypad Autoconfig Directory", "", "<default>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)          WITH_FLAGS(SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR)
      CONFIG_DIR(g_extern.savefile_dir, "savefile_directory", "Savefile Directory", "", "<Content dir>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler);
   CONFIG_DIR(g_extern.savestate_dir, "savestate_directory", "Savestate Directory", "", "<Content dir>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_DIR(g_settings.system_directory, "system_directory", "System Directory", "", "<Content dir>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_DIR(g_settings.extraction_directory, "extraction_directory", "Extraction Directory", "", "<None>", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()
      END_GROUP()

      /***********/
      /* PRIVACY */
      /***********/
      START_GROUP("Privacy Options")
      START_SUB_GROUP("State")
      CONFIG_BOOL(g_settings.camera.allow,     "camera_allow",     "Allow Camera",          false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      CONFIG_BOOL(g_settings.location.allow,     "location_allow",     "Allow Location",          false, "OFF", "ON", GROUP_NAME, SUBGROUP_NAME, general_write_handler, general_read_handler)
      END_SUB_GROUP()
      END_GROUP()

      rarch_setting_t terminator = { ST_NONE };
   APPEND(terminator);

   /* flatten this array to save ourselves some kilobytes */
   return (rarch_setting_t*)realloc(list, sizeof(rarch_setting_t) * index); 
}
