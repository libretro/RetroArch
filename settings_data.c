/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "driver.h"
#include "gfx/video_monitor.h"
#include "settings_data.h"
#include "dynamic.h"
#include <file/file_path.h>
#include "input/input_autodetect.h"
#include "config.def.h"
#include "file_ext.h"
#include "settings.h"
#include "retroarch.h"
#include "performance.h"

#if defined(__CELLOS_LV2__)
#include <sdk_version.h>

#if (CELL_SDK_VERSION > 0x340000)
#include <sysutil/sysutil_bgmplayback.h>
#endif

#endif

#ifdef HAVE_MENU
#include "menu/menu_entries.h"
#endif

/**
 * setting_data_reset_setting:
 * @setting            : pointer to setting
 *
 * Reset a setting's value to its defaults.
 **/
void setting_data_reset_setting(rarch_setting_t* setting)
{
   if (!setting)
      return;

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
      case ST_ACTION:
         break;
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

/**
 * setting_data_reset:
 * @settings           : pointer to settings
 *
 * Reset all settings to their default values.
 **/
void setting_data_reset(rarch_setting_t* settings)
{
   for (; settings->type != ST_NONE; settings++)
      setting_data_reset_setting(settings);
}

/**
 * setting_data_reset:
 * @settings           : pointer to settings
 * @name               : name of setting to search for
 *
 * Search for a setting with a specified name (@name).
 *
 * Returns: pointer to setting if found, NULL otherwise.
 **/
rarch_setting_t* setting_data_find_setting(rarch_setting_t* settings,
      const char* name)
{
   bool found = false;

   if (!settings || !name)
      return NULL;

   for (; settings->type != ST_NONE; settings++)
   {
      if (settings->type <= ST_GROUP && !strcmp(settings->name, name))
      {
         found = true;
         break;
      }
   }

   if (!found)
      return NULL;

   if (settings->short_description && settings->short_description[0] == '\0')
      return NULL;

   if (settings->read_handler)
      settings->read_handler(settings);

   return settings;
}

/**
 * setting_data_set_with_string_representation:
 * @setting            : pointer to setting
 * @value              : value for the setting (string)
 *
 * Set a settings' value with a string. It is assumed
 * that the string has been properly formatted.
 **/
void setting_data_set_with_string_representation(rarch_setting_t* setting,
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
      case ST_ACTION:
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

/**
 * setting_data_get_string_representation:
 * @setting            : pointer to setting
 * @type_str           : buffer to write contents of string representation to.
 * @sizeof_type_str    : size of the buffer (@type_str)
 *
 * Get a setting value's string representation.
 **/
void setting_data_get_string_representation(void *data,
      char *type_str, size_t sizeof_type_str)
{
   rarch_setting_t* setting = (rarch_setting_t*)data;
   if (!setting || !type_str || !sizeof_type_str)
      return;

   if (setting->get_string_representation)
      setting->get_string_representation(setting, type_str, sizeof_type_str);
}

/**
 ******* ACTION START CALLBACK FUNCTIONS *******
**/

/**
 * setting_data_action_start_savestates:
 * @data               : pointer to setting
 *
 * Function callback for 'Savestate' action's 'Action Start'
 * function pointer.
 *
 * Returns: 0 on success, -1 on error.
 **/
static int setting_data_action_start_savestates(void *data)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   g_settings.state_slot = 0;

   return 0;
}

static int setting_data_action_start_bind_device(void *data)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   g_settings.input.joypad_map[setting->index_offset] = setting->index_offset;
   return 0;
}

static int setting_data_bool_action_start_default(void *data)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   *setting->value.boolean = setting->default_value.boolean;

   return 0;
}

static int setting_data_string_dir_action_start_default(void *data)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   *setting->value.string = '\0';

   return 0;
}

static int setting_data_action_start_analog_dpad_mode(void *data)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   *setting->value.unsigned_integer = 0;

   return 0;
}

static int setting_data_uint_action_start_default(void *data)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   *setting->value.unsigned_integer = setting->default_value.unsigned_integer;

   return 0;
}

static int setting_data_action_start_libretro_device_type(void *data)
{
   unsigned current_device, i, devices[128], types = 0, port = 0;
   const struct retro_controller_info *desc = NULL;
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   *setting->value.unsigned_integer = setting->default_value.unsigned_integer;

   port = setting->index_offset;

   devices[types++] = RETRO_DEVICE_NONE;
   devices[types++] = RETRO_DEVICE_JOYPAD;

   /* Only push RETRO_DEVICE_ANALOG as default if we use an 
    * older core which doesn't use SET_CONTROLLER_INFO. */
   if (!g_extern.system.num_ports)
      devices[types++] = RETRO_DEVICE_ANALOG;

   desc = port < g_extern.system.num_ports ?
      &g_extern.system.ports[port] : NULL;
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

   current_device = g_settings.input.libretro_device[port];

   current_device = RETRO_DEVICE_JOYPAD;

   g_settings.input.libretro_device[port] = current_device;
   pretro_set_controller_port_device(port, current_device);

   return 0;
}

static int setting_data_action_start_video_refresh_rate_auto(
      void *data)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   g_extern.measure_data.frame_time_samples_count = 0;

   return 0;
}

static int setting_data_fraction_action_start_default(
      void *data)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   *setting->value.fraction = setting->default_value.fraction;

   return 0;
}

static int setting_data_uint_action_start_linefeed(void *data)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   *setting->value.unsigned_integer = setting->default_value.unsigned_integer;

   return 0;
}

static int setting_data_string_action_start_allow_input(void *data)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   *setting->value.string = '\0';

   return 0;
}

static int setting_data_bind_action_start(void *data)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   struct retro_keybind *def_binds = (struct retro_keybind *)retro_keybinds_1;
   struct retro_keybind *keybind = NULL;

   if (!setting)
      return -1;

   keybind = (struct retro_keybind*)setting->value.keybind;
   if (!keybind)
      return -1;

   if (!g_extern.menu.bind_mode_keyboard)
   {
      keybind->joykey = NO_BTN;
      keybind->joyaxis = AXIS_NONE;
      return 0;
   }

   if (setting->index_offset)
      def_binds = (struct retro_keybind*)retro_keybinds_rest;

   if (!def_binds)
      return -1;

   keybind->key = def_binds[setting->bind_type - MENU_SETTINGS_BIND_BEGIN].key;

   return 0;
}

/**
 ******* ACTION TOGGLE CALLBACK FUNCTIONS *******
**/

/**
 * setting_data_action_toggle_analog_dpad_mode
 * @data               : pointer to setting
 * @action             : toggle action value. Can be either one of :
 *                       MENU_ACTION_RIGHT | MENU_ACTION_LEFT
 *
 * Function callback for 'Analog D-Pad Mode' action's 'Action Toggle'
 * function pointer.
 *
 * Returns: 0 on success, -1 on error.
 **/
static int setting_data_action_toggle_analog_dpad_mode(void *data, unsigned action)
{
   unsigned port = 0;
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   port = setting->index_offset;

   switch (action)
   {
      case MENU_ACTION_RIGHT:
         g_settings.input.analog_dpad_mode[port] =
            (g_settings.input.analog_dpad_mode[port] + 1)
            % ANALOG_DPAD_LAST;
         break;

      case MENU_ACTION_LEFT:
         g_settings.input.analog_dpad_mode[port] =
            (g_settings.input.analog_dpad_mode
             [port] + ANALOG_DPAD_LAST - 1) % ANALOG_DPAD_LAST;
         break;
   }

   return 0;
}

/**
 * setting_data_action_toggle_libretro_device_type
 * @data               : pointer to setting
 * @action             : toggle action value. Can be either one of :
 *                       MENU_ACTION_RIGHT | MENU_ACTION_LEFT
 *
 * Function callback for 'Libretro Device Type' action's 'Action Toggle'
 * function pointer.
 *
 * Returns: 0 on success, -1 on error.
 **/
static int setting_data_action_toggle_libretro_device_type(
      void *data, unsigned action)
{
   unsigned current_device, current_idx, i, devices[128],
            types = 0, port = 0;
   const struct retro_controller_info *desc = NULL;
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   port = setting->index_offset;

   devices[types++] = RETRO_DEVICE_NONE;
   devices[types++] = RETRO_DEVICE_JOYPAD;

   /* Only push RETRO_DEVICE_ANALOG as default if we use an 
    * older core which doesn't use SET_CONTROLLER_INFO. */
   if (!g_extern.system.num_ports)
      devices[types++] = RETRO_DEVICE_ANALOG;

   desc = port < g_extern.system.num_ports ?
      &g_extern.system.ports[port] : NULL;
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

   current_device = g_settings.input.libretro_device[port];
   current_idx = 0;
   for (i = 0; i < types; i++)
   {
      if (current_device == devices[i])
      {
         current_idx = i;
         break;
      }
   }

   switch (action)
   {
      case MENU_ACTION_LEFT:
         current_device = devices
            [(current_idx + types - 1) % types];

         g_settings.input.libretro_device[port] = current_device;
         pretro_set_controller_port_device(port, current_device);
         break;

      case MENU_ACTION_RIGHT:
         current_device = devices
            [(current_idx + 1) % types];

         g_settings.input.libretro_device[port] = current_device;
         pretro_set_controller_port_device(port, current_device);
         break;
   }

   return 0;
}

/**
 * setting_data_action_toggle_savestates
 * @data               : pointer to setting
 * @action             : toggle action value. Can be either one of :
 *                       MENU_ACTION_RIGHT | MENU_ACTION_LEFT
 *
 * Function callback for 'SaveStates' action's 'Action Toggle'
 * function pointer.
 *
 * Returns: 0 on success, -1 on error.
 **/
static int setting_data_action_toggle_savestates(
      void *data, unsigned action)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   switch (action)
   {
      case MENU_ACTION_LEFT:
         /* Slot -1 is (auto) slot. */
         if (g_settings.state_slot >= 0)
            g_settings.state_slot--;
         break;
      case MENU_ACTION_RIGHT:
         g_settings.state_slot++;
         break;
   }

   return 0;
}

/**
 * setting_data_action_toggle_bind_device
 * @data               : pointer to setting
 * @action             : toggle action value. Can be either one of :
 *                       MENU_ACTION_RIGHT | MENU_ACTION_LEFT
 *
 * Function callback for 'Bind Device' action's 'Action Toggle'
 * function pointer.
 *
 * Returns: 0 on success, -1 on error.
 **/
static int setting_data_action_toggle_bind_device(void *data, unsigned action)
{
   unsigned *p = NULL;
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   p = &g_settings.input.joypad_map[setting->index_offset];

   switch (action)
   {
      case MENU_ACTION_LEFT:
         if ((*p) >= g_settings.input.max_users)
            *p = g_settings.input.max_users - 1;
         else if ((*p) > 0)
            (*p)--;
         break;
      case MENU_ACTION_RIGHT:
         if (*p < g_settings.input.max_users)
            (*p)++;
         break;
   }

   return 0;
}

static int setting_data_bool_action_toggle_default(void *data, unsigned action)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   switch (action)
   {
      case MENU_ACTION_LEFT:
      case MENU_ACTION_RIGHT:
         *setting->value.boolean = !(*setting->value.boolean);
         break;
   }

   return 0;
}

static int setting_data_uint_action_toggle_default(void *data, unsigned action)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   switch (action)
   {
      case MENU_ACTION_LEFT:
         if (*setting->value.unsigned_integer != setting->min)
            *setting->value.unsigned_integer =
               *setting->value.unsigned_integer - setting->step;

         if (setting->enforce_minrange)
         {
            if (*setting->value.unsigned_integer < setting->min)
               *setting->value.unsigned_integer = setting->min;
         }
         break;

      case MENU_ACTION_RIGHT:
         *setting->value.unsigned_integer =
            *setting->value.unsigned_integer + setting->step;

         if (setting->enforce_maxrange)
         {
            if (*setting->value.unsigned_integer > setting->max)
               *setting->value.unsigned_integer = setting->max;
         }
         break;
   }

   return 0;
}

static int setting_data_fraction_action_toggle_default(
      void *data, unsigned action)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   switch (action)
   {
      case MENU_ACTION_LEFT:
         *setting->value.fraction =
            *setting->value.fraction - setting->step;

         if (setting->enforce_minrange)
         {
            if (*setting->value.fraction < setting->min)
               *setting->value.fraction = setting->min;
         }
         break;

      case MENU_ACTION_RIGHT:
         *setting->value.fraction = 
            *setting->value.fraction + setting->step;

         if (setting->enforce_maxrange)
         {
            if (*setting->value.fraction > setting->max)
               *setting->value.fraction = setting->max;
         }
         break;
   }

   return 0;
}

static int setting_data_string_action_toggle_driver(void *data,
      unsigned action)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   switch (action)
   {
      case MENU_ACTION_LEFT:
         find_prev_driver(setting->name, setting->value.string, setting->size);
         break;
      case MENU_ACTION_RIGHT:
         find_next_driver(setting->name, setting->value.string, setting->size);
         break;
   }

   return 0;
}

static int core_list_action_toggle(void *data, unsigned action)
{
   rarch_setting_t *setting = (rarch_setting_t *)data;

   if (!setting)
      return -1;

   /* If the user CANCELs the browse, then g_settings.libretro is now
    * set to a directory, which is very bad and will cause a crash
    * later on. I need to be able to add something to call when a
    * cancel happens.
    */
   strlcpy(setting->value.string, g_settings.libretro_directory, setting->size);

   return 0;
}

/**
 * load_content_action_toggle:
 * @data               : pointer to setting
 * @action             : toggle action value. Can be either one of :
 *                       MENU_ACTION_RIGHT | MENU_ACTION_LEFT
 *
 * Function callback for 'Load Content' action's 'Action Toggle'
 * function pointer.
 *
 * Returns: 0 on success, -1 on error.
 **/
static int load_content_action_toggle(void *data, unsigned action)
{
   rarch_setting_t *setting = (rarch_setting_t *)data;

   if (!setting)
      return -1;

   strlcpy(setting->value.string, g_settings.menu_content_directory, setting->size);

   if (g_extern.menu.info.valid_extensions)
      setting->values = g_extern.menu.info.valid_extensions;
   else
      setting->values = g_extern.system.valid_extensions;

   return 0;
}

/**
 ******* ACTION OK CALLBACK FUNCTIONS *******
**/

static int setting_data_action_ok_bind_all(void *data, unsigned action)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   menu_handle_t *menu = menu_driver_resolve();

   if (!setting || !menu)
      return -1;

   menu->binds.target = &g_settings.input.binds
      [setting->index_offset][0];
   menu->binds.begin = MENU_SETTINGS_BIND_BEGIN;
   menu->binds.last = MENU_SETTINGS_BIND_LAST;

   menu_list_push_stack(
         menu->menu_list,
         "",
         "custom_bind_all",
         g_extern.menu.bind_mode_keyboard ?
         MENU_SETTINGS_CUSTOM_BIND_KEYBOARD :
         MENU_SETTINGS_CUSTOM_BIND,
         menu->navigation.selection_ptr);

   if (g_extern.menu.bind_mode_keyboard)
   {
      menu->binds.timeout_end =
         rarch_get_time_usec() + 
         MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
      input_keyboard_wait_keys(menu,
            menu_input_custom_bind_keyboard_cb);
   }
   else
   {
      menu_input_poll_bind_get_rested_axes(&menu->binds);
      menu_input_poll_bind_state(&menu->binds);
   }

   return 0;
}

static int setting_data_action_ok_bind_defaults(void *data, unsigned action)
{
   unsigned i;
   struct retro_keybind *target = NULL;
   const struct retro_keybind *def_binds = NULL;
   rarch_setting_t *setting = (rarch_setting_t*)data;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu)
      return -1;
   if (!setting)
      return -1;

   target = (struct retro_keybind*)
      &g_settings.input.binds[setting->index_offset][0];
   def_binds =  (setting->index_offset) ? 
      retro_keybinds_rest : retro_keybinds_1;

   if (!target)
      return -1;

   menu->binds.begin = MENU_SETTINGS_BIND_BEGIN;
   menu->binds.last  = MENU_SETTINGS_BIND_LAST;

   for (i = MENU_SETTINGS_BIND_BEGIN;
         i <= MENU_SETTINGS_BIND_LAST; i++, target++)
   {
      if (g_extern.menu.bind_mode_keyboard)
         target->key = def_binds[i - MENU_SETTINGS_BIND_BEGIN].key;
      else
      {
         target->joykey = NO_BTN;
         target->joyaxis = AXIS_NONE;
      }
   }

   return 0;
}

static int setting_data_bool_action_ok_exit(void *data, unsigned action)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   if (setting->cmd_trigger.idx != RARCH_CMD_NONE)
   {
      rarch_main_command(setting->cmd_trigger.idx);
      rarch_main_command(RARCH_CMD_RESUME);
   }

   return 0;
}



static int setting_data_bool_action_ok_default(void *data, unsigned action)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   if (setting->cmd_trigger.idx != RARCH_CMD_NONE)
      setting->cmd_trigger.triggered = true;

   return 0;
}



static int setting_data_uint_action_ok_default(void *data, unsigned action)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   if (setting->cmd_trigger.idx != RARCH_CMD_NONE)
      setting->cmd_trigger.triggered = true;

   return 0;
}


static int setting_data_action_ok_video_refresh_rate_auto(
      void *data, unsigned action)
{
   double video_refresh_rate, deviation = 0.0;
   unsigned sample_points = 0;
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   if (video_monitor_fps_statistics(&video_refresh_rate,
            &deviation, &sample_points))
   {
      driver_set_refresh_rate(video_refresh_rate);
      /* Incase refresh rate update forced non-block video. */
      rarch_main_command(RARCH_CMD_VIDEO_SET_BLOCKING_STATE);
   }

   if (setting->cmd_trigger.idx != RARCH_CMD_NONE)
      setting->cmd_trigger.triggered = true;

   return 0;
}


static int setting_data_fraction_action_ok_default(
      void *data, unsigned action)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   if (setting->cmd_trigger.idx != RARCH_CMD_NONE)
      setting->cmd_trigger.triggered = true;

   return 0;
}


static int setting_data_uint_action_ok_linefeed(void *data, unsigned action)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   menu_input_key_start_line(setting->short_description,
         setting->name, 0, 0, menu_input_st_uint_callback);

   return 0;
}

static int setting_data_action_action_ok(void *data, unsigned action)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   if (setting->cmd_trigger.idx != RARCH_CMD_NONE)
      rarch_main_command(setting->cmd_trigger.idx);

   return 0;
}

static int setting_data_bind_action_ok(void *data, unsigned action)
{
   struct retro_keybind *keybind = NULL;
   rarch_setting_t *setting = (rarch_setting_t*)data;
   menu_handle_t *menu = menu_driver_resolve();

   if (!setting)
      return -1;

   if (!menu || !menu->menu_list)
      return -1;
   
   keybind = (struct retro_keybind*)setting->value.keybind;

   if (!keybind)
      return -1;

   menu->binds.begin  = setting->bind_type;
   menu->binds.last   = setting->bind_type;
   menu->binds.target = keybind;
   menu->binds.user = setting->index_offset;
   menu_list_push_stack(
         menu->menu_list,
         "",
         "custom_bind",
         g_extern.menu.bind_mode_keyboard ?
         MENU_SETTINGS_CUSTOM_BIND_KEYBOARD : MENU_SETTINGS_CUSTOM_BIND,
         menu->navigation.selection_ptr);

   if (g_extern.menu.bind_mode_keyboard)
   {
      menu->binds.timeout_end = rarch_get_time_usec() +
         MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
      input_keyboard_wait_keys(menu,
            menu_input_custom_bind_keyboard_cb);
   }
   else
   {
      menu_input_poll_bind_get_rested_axes(&menu->binds);
      menu_input_poll_bind_state(&menu->binds);
   }

   return 0;
}

static int setting_data_string_action_ok_allow_input(void *data,
      unsigned action)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;

   menu_input_key_start_line(setting->short_description,
         setting->name, 0, 0, menu_input_st_string_callback);

   return 0;
}

/**
 ******* ACTION CANCEL CALLBACK FUNCTIONS *******
**/

/**
 ******* SET LABEL CALLBACK FUNCTIONS *******
**/

/**
 * setting_data_get_string_representation_st_bool:
 * @setting            : pointer to setting
 * @type_str           : string for the type to be represented on-screen as
 *                       a label.
 * @type_str_size      : size of @type_str
 *
 * Set a settings' label value. The setting is of type ST_BOOL.
 **/
static void setting_data_get_string_representation_st_bool(void *data,
      char *type_str, size_t type_str_size)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (setting)
      strlcpy(type_str, *setting->value.boolean ? setting->boolean.on_label :
            setting->boolean.off_label, type_str_size);
}

static void setting_data_get_string_representation_st_action(void *data,
      char *type_str, size_t type_str_size)
{
   strlcpy(type_str, "...", type_str_size);
}

static void setting_data_get_string_representation_st_group(void *data,
      char *type_str, size_t type_str_size)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (setting)
      strlcpy(type_str, "...", type_str_size);
}

static void setting_data_get_string_representation_st_sub_group(void *data,
      char *type_str, size_t type_str_size)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (setting)
      strlcpy(type_str, "...", type_str_size);
}

/**
 * setting_data_get_string_representation_st_float:
 * @setting            : pointer to setting
 * @type_str           : string for the type to be represented on-screen as
 *                       a label.
 * @type_str_size      : size of @type_str
 *
 * Set a settings' label value. The setting is of type ST_FLOAT.
 **/
static void setting_data_get_string_representation_st_float(void *data,
      char *type_str, size_t type_str_size)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (setting)
      snprintf(type_str, type_str_size, setting->rounding_fraction,
            *setting->value.fraction);
}

static void setting_data_get_string_representation_st_float_video_refresh_rate_auto(void *data,
      char *type_str, size_t type_str_size)
{
   double video_refresh_rate = 0.0;
   double deviation = 0.0;
   unsigned sample_points = 0;
   rarch_setting_t *setting = (rarch_setting_t*)data;
   if (!setting)
      return;

   if (video_monitor_fps_statistics(&video_refresh_rate, &deviation, &sample_points))
      snprintf(type_str, type_str_size, "%.3f Hz (%.1f%% dev, %u samples)",
            video_refresh_rate, 100.0 * deviation, sample_points);
   else
      strlcpy(type_str, "N/A", type_str_size);
}

static void setting_data_get_string_representation_st_dir(void *data,
      char *type_str, size_t type_str_size)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (setting)
      strlcpy(type_str,
            *setting->value.string ?
            setting->value.string : setting->dir.empty_path,
            type_str_size);
}

static void setting_data_get_string_representation_st_path(void *data,
      char *type_str, size_t type_str_size)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (setting)
      strlcpy(type_str, path_basename(setting->value.string), type_str_size);
}

static void setting_data_get_string_representation_st_string(void *data,
      char *type_str, size_t type_str_size)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (setting)
      strlcpy(type_str, setting->value.string, type_str_size);
}

static void setting_data_get_string_representation_st_bind(void *data,
      char *type_str, size_t type_str_size)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   const struct retro_keybind* keybind = NULL;
   const struct retro_keybind* auto_bind =  NULL;

   if (!setting)
      return;
   
   keybind   = (const struct retro_keybind*)setting->value.keybind;
   auto_bind = (const struct retro_keybind*)input_get_auto_bind(setting->index_offset, keybind->id);

   input_get_bind_string(type_str, keybind, auto_bind, type_str_size);
}

static void setting_data_get_string_representation_int(void *data,
      char *type_str, size_t type_str_size)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (setting)
      snprintf(type_str, type_str_size, "%d", *setting->value.integer);
}

static void setting_data_get_string_representation_uint_video_monitor_index(void *data,
      char *type_str, size_t type_str_size)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   if (!setting)
      return;

   if (*setting->value.unsigned_integer)
      snprintf(type_str, type_str_size, "%u",
            *setting->value.unsigned_integer);
   else
      strlcpy(type_str, "0 (Auto)", type_str_size);
}

static void setting_data_get_string_representation_uint_video_rotation(void *data,
      char *type_str, size_t type_str_size)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   if (setting)
      strlcpy(type_str, rotation_lut[*setting->value.unsigned_integer],
            type_str_size);
}

static void setting_data_get_string_representation_uint_aspect_ratio_index(void *data,
      char *type_str, size_t type_str_size)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   if (setting)
      strlcpy(type_str,
            aspectratio_lut[*setting->value.unsigned_integer].name,
            type_str_size);
}

static void setting_data_get_string_representation_uint_libretro_device(void *data,
      char *type_str, size_t type_str_size)
{
   const struct retro_controller_description *desc = NULL;
   const char *name = NULL;
   rarch_setting_t *setting = (rarch_setting_t*)data;
   if (!setting)
      return;

   if (setting->index_offset < g_extern.system.num_ports)
      desc = libretro_find_controller_description(
            &g_extern.system.ports[setting->index_offset],
            g_settings.input.libretro_device
            [setting->index_offset]);

   if (desc)
      name = desc->desc;

   if (!name)
   {
      /* Find generic name. */

      switch (g_settings.input.libretro_device
            [setting->index_offset])
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

static void setting_data_get_string_representation_uint_archive_mode(void *data,
      char *type_str, size_t type_str_size)
{
   const char *name = "Unknown";

   (void)data;

   switch (g_settings.archive.mode)
   {
      case 0:
         name = "Ask";
         break;
      case 1:
         name = "Load Archive";
         break;
      case 2:
         name = "Open Archive";
         break;
   }

   strlcpy(type_str, name, type_str_size);
}

static void setting_data_get_string_representation_uint_analog_dpad_mode(void *data,
      char *type_str, size_t type_str_size)
{
   static const char *modes[] = {
      "None",
      "Left Analog",
      "Right Analog",
   };
   rarch_setting_t *setting = (rarch_setting_t*)data;
   if (!setting)
      return;

   (void)data;

   strlcpy(type_str, modes[g_settings.input.analog_dpad_mode
         [setting->index_offset] % ANALOG_DPAD_LAST],
         type_str_size);
}

static void setting_data_get_string_representation_uint_autosave_interval(void *data,
      char *type_str, size_t type_str_size)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   if (!setting)
      return;

   if (*setting->value.unsigned_integer)
      snprintf(type_str, type_str_size, "%u seconds",
            *setting->value.unsigned_integer);
   else
      strlcpy(type_str, "OFF", type_str_size);
}

static void setting_data_get_string_representation_uint_user_language(void *data,
      char *type_str, size_t type_str_size)
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
   rarch_setting_t *setting = (rarch_setting_t*)data;
   if (!setting)
      return;

   strlcpy(type_str, modes[g_settings.user_language], type_str_size);
}

static void setting_data_get_string_representation_uint_libretro_log_level(void *data,
      char *type_str, size_t type_str_size)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   if (!setting)
      return;
   static const char *modes[] = {
      "0 (Debug)",
      "1 (Info)",
      "2 (Warning)",
      "3 (Error)"
   };

   strlcpy(type_str, modes[*setting->value.unsigned_integer],
         type_str_size);
}

static void setting_data_get_string_representation_uint(void *data,
      char *type_str, size_t type_str_size)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;
   if (setting)
      snprintf(type_str, type_str_size, "%u",
            *setting->value.unsigned_integer);
}

/**
 ******* LIST BUILDING HELPER FUNCTIONS *******
**/

/**
 * setting_data_action_setting:
 * @name               : Name of setting.
 * @short_description  : Short description of setting.
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 *
 * Initializes a setting of type ST_ACTION.
 *
 * Returns: setting of type ST_ACTION.
 **/
rarch_setting_t setting_data_action_setting(const char* name,
      const char* short_description,
      const char *group, const char *subgroup)
{
   rarch_setting_t result;

   memset(&result, 0, sizeof(result));
   
   result.type                      = ST_ACTION;
   result.name                      = name;

   result.short_description         = short_description;
   result.group                     = group;
   result.subgroup                  = subgroup;
   result.change_handler            = NULL;
   result.deferred_handler          = NULL;
   result.read_handler              = NULL;
   result.get_string_representation = &setting_data_get_string_representation_st_action;
   result.action_start              = NULL;
   result.action_iterate            = NULL;
   result.action_toggle             = NULL;
   result.action_ok                 = setting_data_action_action_ok;
   result.action_cancel             = NULL;

   return result;
}

/**
 * setting_data_group_setting:
 * @type               : type of settting.
 * @name               : name of setting.
 *
 * Initializes a setting of type ST_GROUP.
 *
 * Returns: setting of type ST_GROUP.
 **/
rarch_setting_t setting_data_group_setting(enum setting_type type, const char* name)
{
   rarch_setting_t result;

   memset(&result, 0, sizeof(result));
   
   result.type              = type;
   result.name              = name;
   result.short_description = name;

   result.get_string_representation       = &setting_data_get_string_representation_st_group;

   return result;
}

/**
 * setting_data_subgroup_setting:
 * @type               : type of settting.
 * @name               : name of setting.
 * @parent_name        : group that the subgroup setting belongs to.
 *
 * Initializes a setting of type ST_SUBGROUP.
 *
 * Returns: setting of type ST_SUBGROUP.
 **/
rarch_setting_t setting_data_subgroup_setting(enum setting_type type,
      const char* name, const char *parent_name)
{
   rarch_setting_t result;

   memset(&result, 0, sizeof(result));
   
   result.type              = type;
   result.name              = name;

   result.short_description = name;
   result.group             = parent_name;

   result.get_string_representation       = &setting_data_get_string_representation_st_sub_group;

   return result;
}

/**
 * setting_data_float_setting:
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of float setting.
 * @default_value      : Default value (in float).
 * @rounding           : Rounding (for float-to-string representation).
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a setting of type ST_FLOAT.
 *
 * Returns: setting of type ST_FLOAT.
 **/
rarch_setting_t setting_data_float_setting(const char* name,
      const char* short_description, float* target, float default_value,
      const char *rounding, const char *group, const char *subgroup,
      change_handler_t change_handler, change_handler_t read_handler)
{
   rarch_setting_t result;

   memset(&result, 0, sizeof(result));
   
   result.type                    = ST_FLOAT;
   result.name                    = name;
   result.size                    = sizeof(float);
   result.short_description       = short_description;
   result.group                   = group;
   result.subgroup                = subgroup;

   result.rounding_fraction       = rounding;
   result.change_handler          = change_handler;
   result.read_handler            = read_handler;
   result.value.fraction          = target;
   result.original_value.fraction = *target;
   result.default_value.fraction  = default_value;
   result.action_start            = setting_data_fraction_action_start_default;
   result.action_toggle           = setting_data_fraction_action_toggle_default;
   result.action_ok               = setting_data_fraction_action_ok_default;
   result.action_cancel           = NULL;

   result.get_string_representation       = &setting_data_get_string_representation_st_float;

   return result;
}

/**
 * setting_data_bool_setting:
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of bool setting.
 * @default_value      : Default value (in bool format).
 * @off                : String value for "Off" label.
 * @on                 : String value for "On"  label.
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a setting of type ST_BOOL.
 *
 * Returns: setting of type ST_BOOL.
 **/
rarch_setting_t setting_data_bool_setting(const char* name,
      const char* short_description, bool* target, bool default_value,
      const char *off, const char *on,
      const char *group, const char *subgroup,
      change_handler_t change_handler, change_handler_t read_handler)
{
   rarch_setting_t result;

   memset(&result, 0, sizeof(result));
   
   result.type                   = ST_BOOL;
   result.name                   = name;
   result.size                   = sizeof(bool);
   result.short_description      = short_description;
   result.group                  = group;
   result.subgroup               = subgroup;

   result.change_handler         = change_handler;
   result.read_handler           = read_handler;
   result.value.boolean          = target;
   result.original_value.boolean = *target;
   result.default_value.boolean  = default_value;
   result.boolean.off_label      = off;
   result.boolean.on_label       = on;

   result.action_start           = setting_data_bool_action_start_default;
   result.action_toggle          = setting_data_bool_action_toggle_default;
   result.action_ok              = setting_data_bool_action_ok_default;
   result.action_cancel          = NULL;

   result.get_string_representation       = &setting_data_get_string_representation_st_bool;
   return result;
}

/**
 * setting_data_int_setting:
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of signed integer setting.
 * @default_value      : Default value (in signed integer format).
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a setting of type ST_INT. 
 *
 * Returns: setting of type ST_INT.
 **/
rarch_setting_t setting_data_int_setting(const char* name,
      const char* short_description, int* target, int default_value,
      const char *group, const char *subgroup, change_handler_t change_handler,
      change_handler_t read_handler)
{
   rarch_setting_t result;

   memset(&result, 0, sizeof(result));
   
   result.type                   = ST_INT;
   result.name                   = name;
   result.size                   = sizeof(int);
   result.short_description      = short_description;
   result.group                  = group;
   result.subgroup               = subgroup;

   result.change_handler         = change_handler;
   result.read_handler           = read_handler;
   result.value.integer          = target;
   result.original_value.integer = *target;
   result.default_value.integer  = default_value;

   result.get_string_representation       = &setting_data_get_string_representation_int;

   return result;
}

/**
 * setting_data_uint_setting:
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of unsigned integer setting.
 * @default_value      : Default value (in unsigned integer format).
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a setting of type ST_UINT. 
 *
 * Returns: setting of type ST_UINT.
 **/
rarch_setting_t setting_data_uint_setting(const char* name,
      const char* short_description, unsigned int* target,
      unsigned int default_value, const char *group, const char *subgroup,
      change_handler_t change_handler, change_handler_t read_handler)
{
   rarch_setting_t result;

   memset(&result, 0, sizeof(result));
   
   result.type                            = ST_UINT;
   result.name                            = name;
   result.size                            = sizeof(unsigned int);
   result.short_description               = short_description;
   result.group                           = group;
   result.subgroup                        = subgroup;

   result.change_handler                  = change_handler;
   result.read_handler                    = read_handler;
   result.value.unsigned_integer          = target;
   result.original_value.unsigned_integer = *target;
   result.default_value.unsigned_integer  = default_value;
   result.action_start                    = setting_data_uint_action_start_default;
   result.action_toggle                   = setting_data_uint_action_toggle_default;
   result.action_ok                       = setting_data_uint_action_ok_default;
   result.action_cancel                   = NULL;
   result.get_string_representation       = &setting_data_get_string_representation_uint;

   return result;
}

/**
 * setting_data_bind_setting:
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of bind setting.
 * @idx                : Index of bind setting.
 * @idx_offset         : Index offset of bind setting.
 * @default_value      : Default value (in bind format).
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 *
 * Initializes a setting of type ST_BIND. 
 *
 * Returns: setting of type ST_BIND.
 **/
rarch_setting_t setting_data_bind_setting(const char* name,
      const char* short_description, struct retro_keybind* target,
      uint32_t idx, uint32_t idx_offset,
      const struct retro_keybind* default_value,
      const char *group, const char *subgroup)
{
   rarch_setting_t result;

   memset(&result, 0, sizeof(result));
   
   result.type                  = ST_BIND;
   result.name                  = name;
   result.size                  = 0;
   result.short_description     = short_description;
   result.group                 = group;
   result.subgroup              = subgroup;

   result.value.keybind         = target;
   result.default_value.keybind = default_value;
   result.index                 = idx;
   result.index_offset          = idx_offset;
   result.action_start          = setting_data_bind_action_start;
   result.action_ok             = setting_data_bind_action_ok;
   result.action_cancel         = NULL;
   result.get_string_representation       = &setting_data_get_string_representation_st_bind;

   return result;
}

/**
 * setting_data_string_setting:
 * @type               : type of setting.
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of string setting.
 * @size               : Size of string setting.
 * @default_value      : Default value (in string format).
 * @empty              : TODO/FIXME: ???
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a string setting (of type @type). 
 *
 * Returns: String setting of type @type.
 **/
rarch_setting_t setting_data_string_setting(enum setting_type type,
      const char* name, const char* short_description, char* target,
      unsigned size, const char* default_value, const char *empty,
      const char *group, const char *subgroup, change_handler_t change_handler,
      change_handler_t read_handler)
{
   rarch_setting_t result;

   memset(&result, 0, sizeof(result));
   
   result.type                 = type;
   result.name                 = name;
   result.size                 = size;
   result.short_description    = short_description;
   result.group                = group;
   result.subgroup             = subgroup;

   result.dir.empty_path       = empty;
   result.change_handler       = change_handler;
   result.read_handler         = read_handler;
   result.value.string         = target;
   result.default_value.string = default_value;
   result.action_start         = NULL;
   result.get_string_representation       = &setting_data_get_string_representation_st_string;

   switch (type)
   {
      case ST_DIR:
         result.action_start           = setting_data_string_dir_action_start_default;
         result.browser_selection_type = ST_DIR;
         result.get_string_representation = &setting_data_get_string_representation_st_dir;
         break;
      case ST_PATH:
         result.action_start           = setting_data_string_dir_action_start_default;
         result.browser_selection_type = ST_PATH;
         result.get_string_representation = &setting_data_get_string_representation_st_path;
         break;
      default:
         break;
   }

   return result;
}

/**
 * setting_data_string_setting_options:
 * @type               : type of settting.
 * @name               : name of setting.
 * @short_description  : Short description of setting.
 * @target             : Target of bind setting.
 * @size               : Size of string setting.
 * @default_value      : Default value.
 * @empty              : N/A.
 * @values             : Values, separated by a delimiter.
 * @group              : Group that the setting belongs to.
 * @subgroup           : Subgroup that the setting belongs to.
 * @change_handler     : Function callback for change handler function pointer.
 * @read_handler       : Function callback for read handler function pointer.
 *
 * Initializes a string options list setting. 
 *
 * Returns: string option list setting.
 **/
rarch_setting_t setting_data_string_setting_options(enum setting_type type,
 const char* name, const char* short_description, char* target,
 unsigned size, const char* default_value,
 const char *empty, const char *values,
 const char *group, const char *subgroup,
 change_handler_t change_handler, change_handler_t read_handler)
{
  rarch_setting_t result = setting_data_string_setting(type, name,
        short_description, target, size, default_value, empty, group,
        subgroup, change_handler, read_handler);

  result.values          = values;
  return result;
}

/**
 * setting_data_get_description:
 * @label              : identifier label of setting
 * @msg                : output message 
 * @sizeof_msg         : size of @msg
 *
 * Writes a 'Help' description message to @msg if there is
 * one available based on the identifier label of the setting
 * (@label).
 *
 * Returns: 0 (always for now). TODO: make it handle -1 as well.
 **/
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
               "makes it simpler, but not as flexible as udev. \n" "Mice, etc, are not supported at all. \n"
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
      else if (!strcmp(g_settings.video.driver, "sunxi"))
         snprintf(msg, sizeof_msg,
               " -- Sunxi-G2D Video Driver. \n"
               " \n"
               "This is a low-level Sunxi video driver. \n"
               "Uses the G2D block in Allwinner SoCs.");
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
   else if (!strcmp(label, "audio_max_timing_skew"))
   {
      snprintf(msg, sizeof_msg,
            " -- Maximum audio timing skew.\n"
            " \n"
            "Defines the maximum change in input rate.\n"
            "You may want to increase this to enable\n"
            "very large changes in timing, for example\n"
            "running PAL cores on NTSC displays, at the\n"
            "cost of inaccurate audio pitch.\n"
            " \n"
            " Input rate is defined as: \n"
            " input rate * (1.0 +/- (max timing skew))");
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
            " -- Netplay flip users.");
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
            "Picks which gamepad to use for user N. \n"
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

#ifdef HAVE_MENU
static void get_string_representation_bind_device(void * data, char *type_str,
      size_t type_str_size)
{
   unsigned map = 0;
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return;

   map = g_settings.input.joypad_map[setting->index_offset];

   if (map < g_settings.input.max_users)
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


static void get_string_representation_savestate(void * data, char *type_str,
      size_t type_str_size)
{
   snprintf(type_str, type_str_size, "%d", g_settings.state_slot);
   if (g_settings.state_slot == -1)
      strlcat(type_str, " (Auto)", type_str_size);
}

/**
 * setting_data_get_label:
 * @list               : File list on which to perform the search
 * @type_str           : String for the type to be represented on-screen as
 *                       a label.
 * @type_str_size      : Size of @type_str
 * @w                  : Width of the string (for text label representation
 *                       purposes in the menu display driver).
 * @type               : Identifier of setting.
 * @menu_label         : Menu Label identifier of setting.
 * @label              : Label identifier of setting.
 * @idx                : Index identifier of setting.
 *
 * Get associated label of a setting.
 **/
void setting_data_get_label(file_list_t *list, char *type_str,
      size_t type_str_size, unsigned *w, unsigned type, 
      const char *menu_label, const char *label, unsigned idx)
{
   rarch_setting_t *setting_data = NULL;
   rarch_setting_t *setting      = NULL;
   menu_handle_t *menu = menu_driver_resolve();

   if (!menu || !menu->menu_list || !label)
      return;

   setting_data = (rarch_setting_t*)menu->list_settings;

   if (!setting_data)
      return;

   setting = (rarch_setting_t*)setting_data_find_setting(setting_data,
         list->list[idx].label);

   if (setting)
      setting_data_get_string_representation(setting, type_str, type_str_size);
}
#endif

static void general_read_handler(void *data)
{
   rarch_setting_t *setting = (rarch_setting_t*)data;

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
   else if (!strcmp(setting->name, "audio_max_timing_skew"))
      *setting->value.fraction = g_settings.audio.max_timing_skew;
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
}

static void general_write_handler(void *data)
{
   unsigned rarch_cmd = RARCH_CMD_NONE;
   rarch_setting_t *setting = (rarch_setting_t*)data;

   if (!setting)
      return;

   if (setting->cmd_trigger.idx != RARCH_CMD_NONE)
   {
      if (setting->flags & SD_FLAG_EXIT)
      {
         if (*setting->value.boolean)
            *setting->value.boolean = false;
      }
      if (setting->cmd_trigger.triggered ||
            (setting->flags & SD_FLAG_CMD_APPLY_AUTO))
         rarch_cmd = setting->cmd_trigger.idx;
   }

   if (!strcmp(setting->name, "help"))
   {
      menu_handle_t *menu = menu_driver_resolve();

      if (!menu || !menu->menu_list)
         return;

      if (*setting->value.boolean)
      {
#ifdef HAVE_MENU
         menu_list_push_stack_refresh(
               menu->menu_list,
               "",
               "help",
               0,
               0);
#endif
         *setting->value.boolean = false;
      }
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
   else if (!strcmp(setting->name, "video_rotation"))
   {
      if (driver.video && driver.video->set_rotation)
         driver.video->set_rotation(driver.video_data,
               (*setting->value.unsigned_integer +
                g_extern.system.rotation) % 4);
   }
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
   else if (!strcmp(setting->name, "audio_latency"))
      rarch_cmd = RARCH_CMD_AUDIO_REINIT;
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
   else if (!strcmp(setting->name, "audio_max_timing_skew"))
      g_settings.audio.max_timing_skew = *setting->value.fraction;
   else if (!strcmp(setting->name, "video_refresh_rate_auto"))
   {
      if (driver.video && driver.video_data)
      {
         driver_set_refresh_rate(*setting->value.fraction);

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

   if (rarch_cmd || setting->cmd_trigger.triggered)
      rarch_main_command(rarch_cmd);
}

#define START_GROUP(group_info, NAME) \
{ \
   group_info.name = NAME; \
   if (!(settings_list_append(list, list_info, setting_data_group_setting (ST_GROUP, NAME)))) return false; \
}

#define END_GROUP(list, list_info) \
{ \
   if (!(settings_list_append(list, list_info, setting_data_group_setting (ST_END_GROUP, 0)))) return false; \
}

#define START_SUB_GROUP(list, list_info, NAME, group_info, subgroup_info) \
{ \
   subgroup_info.name = NAME; \
   if (!(settings_list_append(list, list_info, setting_data_subgroup_setting (ST_SUB_GROUP, NAME, group_info)))) return false; \
}

#define END_SUB_GROUP(list, list_info) \
{ \
   if (!(settings_list_append(list, list_info, setting_data_group_setting (ST_END_SUB_GROUP, 0)))) return false; \
}

#define CONFIG_ACTION(NAME, SHORT, group_info, subgroup_info) \
{ \
   if (!settings_list_append(list, list_info, setting_data_action_setting  (NAME, SHORT, group_info, subgroup_info))) return false; \
}

#define CONFIG_BOOL(TARGET, NAME, SHORT, DEF, OFF, ON, group_info, subgroup_info, CHANGE_HANDLER, READ_HANDLER) \
{ \
   if (!settings_list_append(list, list_info, setting_data_bool_setting  (NAME, SHORT, &TARGET, DEF, OFF, ON, group_info, subgroup_info, CHANGE_HANDLER, READ_HANDLER)))return false; \
}

#define CONFIG_INT(TARGET, NAME, SHORT, DEF, group_info, subgroup_info, CHANGE_HANDLER, READ_HANDLER) \
{ \
   if (!(settings_list_append(list, list_info, setting_data_int_setting   (NAME, SHORT, &TARGET, DEF, group_info, subgroup_info, CHANGE_HANDLER, READ_HANDLER)))) return false; \
}

#define CONFIG_UINT(TARGET, NAME, SHORT, DEF, group_info, subgroup_info, CHANGE_HANDLER, READ_HANDLER) \
{ \
   if (!(settings_list_append(list, list_info, setting_data_uint_setting  (NAME, SHORT, &TARGET, DEF, group_info, subgroup_info, CHANGE_HANDLER, READ_HANDLER)))) return false; \
}

#define CONFIG_FLOAT(TARGET, NAME, SHORT, DEF, ROUNDING, group_info, subgroup_info, CHANGE_HANDLER, READ_HANDLER) \
{ \
   if (!(settings_list_append(list, list_info, setting_data_float_setting (NAME, SHORT, &TARGET, DEF, ROUNDING, group_info, subgroup_info, CHANGE_HANDLER, READ_HANDLER)))) return false; \
}

#define CONFIG_PATH(TARGET, NAME, SHORT, DEF, group_info, subgroup_info, CHANGE_HANDLER, READ_HANDLER) \
{ \
   if (!(settings_list_append(list, list_info, setting_data_string_setting(ST_PATH, NAME, SHORT, TARGET, sizeof(TARGET), DEF, "", group_info, subgroup_info, CHANGE_HANDLER, READ_HANDLER)))) return false; \
}

#define CONFIG_DIR(TARGET, NAME, SHORT, DEF, EMPTY, group_info, subgroup_info, CHANGE_HANDLER, READ_HANDLER) \
{ \
   if (!(settings_list_append(list, list_info, setting_data_string_setting(ST_DIR, NAME, SHORT, TARGET, sizeof(TARGET), DEF, EMPTY, group_info, subgroup_info, CHANGE_HANDLER, READ_HANDLER)))) return false; \
}

#define CONFIG_STRING(TARGET, NAME, SHORT, DEF, group_info, subgroup_info, CHANGE_HANDLER, READ_HANDLER) \
{ \
   if (!(settings_list_append(list, list_info, setting_data_string_setting(ST_STRING, NAME, SHORT, TARGET, sizeof(TARGET), DEF, "", group_info, subgroup_info, CHANGE_HANDLER, READ_HANDLER)))) return false; \
}

#define CONFIG_STRING_OPTIONS(TARGET, NAME, SHORT, DEF, OPTS, group_info, subgroup_info, CHANGE_HANDLER, READ_HANDLER) \
{ \
  if (!(settings_list_append(list, list_info, setting_data_string_setting_options(ST_STRING, NAME, SHORT, TARGET, sizeof(TARGET), DEF, "", OPTS, group_info, subgroup_info, CHANGE_HANDLER, READ_HANDLER)))) return false; \
}

#define CONFIG_HEX(TARGET, NAME, SHORT, group_info, subgroup_info)

#define CONFIG_BIND(TARGET, PLAYER, PLAYER_OFFSET, NAME, SHORT, DEF, group_info, subgroup_info) \
{ \
   if (!(settings_list_append(list, list_info, setting_data_bind_setting  (NAME, SHORT, &TARGET, PLAYER, PLAYER_OFFSET, DEF, group_info, subgroup_info)))) return false; \
}

#ifdef GEKKO
#define MAX_GAMMA_SETTING 2
#else
#define MAX_GAMMA_SETTING 1
#endif

static void setting_data_add_special_callbacks(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned values)
{
   unsigned idx = list_info->index - 1;

   if (values & SD_FLAG_ALLOW_INPUT)
   {
      switch ((*list)[idx].type)
      {
         case ST_UINT:
            (*list)[idx].action_start  = setting_data_uint_action_start_linefeed;
            (*list)[idx].action_ok     = setting_data_uint_action_ok_linefeed;
            (*list)[idx].action_cancel = NULL;
            break;
         case ST_STRING:
            (*list)[idx].action_start  = setting_data_string_action_start_allow_input;
            (*list)[idx].action_ok     = setting_data_string_action_ok_allow_input;
            (*list)[idx].action_cancel = NULL;
            break;
         default:
            break;
      }
   }
   else if (values & SD_FLAG_IS_DRIVER)
      (*list)[idx].action_toggle = setting_data_string_action_toggle_driver;
}

static void settings_data_list_current_add_flags(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info,
      unsigned values)
{
   settings_list_current_add_flags(
         list,
         list_info,
         values);
   setting_data_add_special_callbacks(list, list_info, values);
}

static void core_list_change_handler(void *data)
{
  rarch_setting_t *setting = (rarch_setting_t *)data;
  (void)setting;

  rarch_main_command(RARCH_CMD_LOAD_CORE);
}

/**
 * load_content_change_handler:
 * @data               : pointer to setting
 *
 * Function callback for 'Load Content' action's 'Change Handler'
 * function pointer.
 **/
static void load_content_change_handler(void *data)
{
   rarch_setting_t *setting = (rarch_setting_t *)data;

   if (!setting)
      return;

   /* This does not appear to be robust enough because sometimes I get
    * crashes. I think it is because LOAD_CORE has not yet run. I'm not
    * sure the best way to test for that.
    */
   rarch_main_command(RARCH_CMD_LOAD_CONTENT);
}

static void overlay_enable_toggle_change_handler(void *data)
{
   rarch_setting_t *setting = (rarch_setting_t *)data;

   if (!setting)
      return;

   if (setting->value.boolean)
      rarch_main_command(RARCH_CMD_OVERLAY_INIT);
   else
      rarch_main_command(RARCH_CMD_OVERLAY_DEINIT);
}

static bool setting_data_append_list_main_menu_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;

   START_GROUP(group_info, "Main Menu");
   START_SUB_GROUP(list, list_info, "State", group_info.name, subgroup_info);
#if defined(HAVE_DYNAMIC) || defined(HAVE_LIBRETRO_MANAGEMENT)
   CONFIG_ACTION(
         "core_list",
         "Core Selection",
         group_info.name,
         subgroup_info.name);
   (*list)[list_info->index - 1].size = sizeof(g_settings.libretro);
   (*list)[list_info->index - 1].value.string = g_settings.libretro;
   (*list)[list_info->index - 1].values = EXT_EXECUTABLES;
   // It is not a good idea to have chosen action_toggle as the place
   // to put this callback. It should be called whenever the browser
   // needs to get the directory to browse into. It's not quite like
   // get_string_representation, but it is close.
   (*list)[list_info->index - 1].action_toggle = core_list_action_toggle;
   (*list)[list_info->index - 1].change_handler = core_list_change_handler;
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_BROWSER_ACTION);
#endif

#ifdef HAVE_NETWORKING
   CONFIG_ACTION(
         "core_updater_list",
         "Core Updater",
         group_info.name,
         subgroup_info.name);
#endif

   if (g_settings.history_list_enable)
   {
      CONFIG_ACTION(
            "history_list",
            "Load Content (History)",
            group_info.name,
            subgroup_info.name);
   }
   if (
         driver.menu 
         && g_extern.core_info 
         && core_info_list_num_info_files(g_extern.core_info))
   {
      CONFIG_ACTION(
            "detect_core_list",
            "Load Content (Detect Core)",
            group_info.name,
            subgroup_info.name);
      settings_data_list_current_add_flags(list, list_info, SD_FLAG_BROWSER_ACTION);
   }
   CONFIG_ACTION(
         "load_content",
         "Load Content",
         group_info.name,
         subgroup_info.name);
   (*list)[list_info->index - 1].size = sizeof(g_extern.fullpath);
   (*list)[list_info->index - 1].value.string = g_extern.fullpath;
   (*list)[list_info->index - 1].action_toggle = load_content_action_toggle;
   (*list)[list_info->index - 1].change_handler = load_content_change_handler;
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_BROWSER_ACTION);


   CONFIG_ACTION(
         "core_information",
         "Core Information",
         group_info.name,
         subgroup_info.name);

   CONFIG_ACTION(
         "management",
         "Management",
         group_info.name,
         subgroup_info.name);

   CONFIG_ACTION(
         "options",
         "Options",
         group_info.name,
         subgroup_info.name);

   CONFIG_ACTION(
         "settings",
         "Settings",
         group_info.name,
         subgroup_info.name);

   if (g_extern.perfcnt_enable)
   {
      CONFIG_ACTION(
            "performance_counters",
            "Performance Counters",
            group_info.name,
            subgroup_info.name);
   }
   if (g_extern.main_is_init && !g_extern.libretro_dummy)
   {
      CONFIG_ACTION(
            "savestate",
            "Save State",
            group_info.name,
            subgroup_info.name);
      (*list)[list_info->index - 1].action_toggle = &setting_data_action_toggle_savestates;
      (*list)[list_info->index - 1].action_start  = &setting_data_action_start_savestates;
      (*list)[list_info->index - 1].action_ok     = &setting_data_bool_action_ok_exit;
      (*list)[list_info->index - 1].get_string_representation = &get_string_representation_savestate;
      settings_list_current_add_cmd  (list, list_info, RARCH_CMD_SAVE_STATE);

      CONFIG_ACTION(
            "loadstate",
            "Load State",
            group_info.name,
            subgroup_info.name);
      (*list)[list_info->index - 1].action_toggle = &setting_data_action_toggle_savestates;
      (*list)[list_info->index - 1].action_start  = &setting_data_action_start_savestates;
      (*list)[list_info->index - 1].action_ok     = &setting_data_bool_action_ok_exit;
      (*list)[list_info->index - 1].get_string_representation = &get_string_representation_savestate;
      settings_list_current_add_cmd  (list, list_info, RARCH_CMD_LOAD_STATE);

      CONFIG_ACTION(
            "take_screenshot",
            "Take Screenshot",
            group_info.name,
            subgroup_info.name);
      settings_list_current_add_cmd  (list, list_info, RARCH_CMD_TAKE_SCREENSHOT);

      CONFIG_ACTION(
            "resume_content",
            "Resume Content",
            group_info.name,
            subgroup_info.name);
      settings_list_current_add_cmd  (list, list_info, RARCH_CMD_RESUME);
      (*list)[list_info->index - 1].action_ok     = &setting_data_bool_action_ok_exit;

      CONFIG_ACTION(
            "restart_content",
            "Restart Content",
            group_info.name,
            subgroup_info.name);
      settings_list_current_add_cmd(list, list_info, RARCH_CMD_RESET);
      (*list)[list_info->index - 1].action_ok = &setting_data_bool_action_ok_exit;
   }
#ifndef HAVE_DYNAMIC
   CONFIG_ACTION(
         "restart_retroarch",
         "Restart RetroArch",
         group_info.name,
         subgroup_info.name);
   settings_list_current_add_cmd(list, list_info, RARCH_CMD_RESTART_RETROARCH);
#endif

   CONFIG_ACTION(
         "configurations",
         "Configurations",
         group_info.name,
         subgroup_info.name);

   CONFIG_ACTION(
         "save_new_config",
         "Save New Config",
         group_info.name,
         subgroup_info.name);
   settings_list_current_add_cmd(list, list_info, RARCH_CMD_MENU_SAVE_CONFIG);

   CONFIG_ACTION(
         "help",
         "Help",
         group_info.name,
         subgroup_info.name);

#if !defined(IOS)
   /* Apple rejects iOS apps that lets you forcibly quit an application. */
   CONFIG_ACTION(
         "quit_retroarch",
         "Quit RetroArch",
         group_info.name,
         subgroup_info.name);
   settings_list_current_add_cmd(list, list_info, RARCH_CMD_QUIT_RETROARCH);
#endif

   END_SUB_GROUP(list, list_info);
   END_GROUP(list, list_info);

   return true;
}

static bool setting_data_append_list_driver_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   
   START_GROUP(group_info, "Driver Settings");

   START_SUB_GROUP(list, list_info, "State", group_info.name, subgroup_info);
   
   CONFIG_STRING_OPTIONS(
         g_settings.input.driver,
         "input_driver",
         "Input Driver",
         config_get_default_input(),
         config_get_input_driver_options(),
         group_info.name,
         subgroup_info.name,
         NULL,
         NULL);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);

   CONFIG_STRING_OPTIONS(
         g_settings.video.driver,
         "video_driver",
         "Video Driver",
         config_get_default_video(),
         config_get_video_driver_options(),
         group_info.name,
         subgroup_info.name,
         NULL,
         NULL);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);

   CONFIG_STRING_OPTIONS(
         g_settings.audio.driver,
         "audio_driver",
         "Audio Driver",
         config_get_default_audio(),
         config_get_audio_driver_options(),
         group_info.name,
         subgroup_info.name,
         NULL,
         NULL);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);

   CONFIG_STRING_OPTIONS(
         g_settings.audio.resampler,
         "audio_resampler_driver",
         "Audio Resampler Driver",
         config_get_default_audio_resampler(),
         config_get_audio_resampler_driver_options(),
         group_info.name,
         subgroup_info.name,
         NULL,
         NULL);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);

   CONFIG_STRING_OPTIONS(
         g_settings.camera.driver,
         "camera_driver",
         "Camera Driver",
         config_get_default_camera(),
         config_get_camera_driver_options(),
         group_info.name,
         subgroup_info.name,
         NULL,
         NULL);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);

   CONFIG_STRING_OPTIONS(
         g_settings.location.driver,
         "location_driver",
         "Location Driver",
         config_get_default_location(),
         config_get_location_driver_options(),
         group_info.name,
         subgroup_info.name,
         NULL,
         NULL);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);

#ifdef HAVE_MENU
   CONFIG_STRING_OPTIONS(
         g_settings.menu.driver,
         "menu_driver",
         "Menu Driver",
         config_get_default_menu(),
         config_get_menu_driver_options(),
         group_info.name,
         subgroup_info.name,
         NULL,
         NULL);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);
#endif

   CONFIG_STRING_OPTIONS(
         g_settings.input.joypad_driver,
         "input_joypad_driver",
         "Joypad Driver",
         config_get_default_joypad(),
         config_get_joypad_driver_options(),
         group_info.name,
         subgroup_info.name,
         NULL,
         NULL);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DRIVER);

   END_SUB_GROUP(list, list_info);
   END_GROUP(list, list_info);

   return true;
}

static bool setting_data_append_list_general_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;

   START_GROUP(group_info, "General Settings");

   START_SUB_GROUP(list, list_info, "State", group_info.name, subgroup_info);

   CONFIG_BOOL(
         g_settings.load_dummy_on_core_shutdown,
         "dummy_on_core_shutdown",
         "Dummy On Core Shutdown",
         load_dummy_on_core_shutdown,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.core_specific_config,
         "core_specific_config",
         "Configuration Per-Core",
         default_core_specific_config,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);


   END_SUB_GROUP(list, list_info);

   START_SUB_GROUP(list, list_info, "Logging", group_info.name, subgroup_info);

   CONFIG_BOOL(
         g_extern.verbosity,
         "log_verbosity",
         "Logging Verbosity",
         false,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);


   CONFIG_UINT(g_settings.libretro_log_level,
         "libretro_log_level",
         "Libretro Logging Level",
         libretro_log_level,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 0, 3, 1.0, true, true);
   (*list)[list_info->index - 1].get_string_representation = 
      &setting_data_get_string_representation_uint_libretro_log_level;

   END_SUB_GROUP(list, list_info);

   START_SUB_GROUP(list, list_info, "Performance Counters", group_info.name, subgroup_info);

   CONFIG_BOOL(g_extern.perfcnt_enable,
         "perfcnt_enable",
         "Performance Counters",
         false,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(g_settings.config_save_on_exit,
         "config_save_on_exit",
         "Configuration Save On Exit",
         config_save_on_exit,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   END_SUB_GROUP(list, list_info);

   START_SUB_GROUP(list, list_info, "Frame rewinding", group_info.name, subgroup_info);

   CONFIG_BOOL(
         g_settings.rewind_enable,
         "rewind_enable",
         "Rewind",
         rewind_enable,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_cmd(list, list_info, RARCH_CMD_REWIND_TOGGLE);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);
#if 0
   CONFIG_SIZE(
         g_settings.rewind_buffer_size,
         "rewind_buffer_size",
         "Rewind Buffer Size",
         rewind_buffer_size,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler)
#endif
      CONFIG_UINT(
            g_settings.rewind_granularity,
            "rewind_granularity",
            "Rewind Granularity",
            rewind_granularity,
            group_info.name,
            subgroup_info.name,
            general_write_handler,
            general_read_handler);
   settings_list_current_add_range(list, list_info, 1, 32768, 1, true, false);

   END_SUB_GROUP(list, list_info);

   START_SUB_GROUP(list, list_info, "Saving", group_info.name, subgroup_info);

   CONFIG_INT(
         g_settings.state_slot,
         "state_slot",
         "State Slot",
         0,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.block_sram_overwrite,
         "block_sram_overwrite",
         "SRAM Block overwrite",
         block_sram_overwrite,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

#ifdef HAVE_THREADS
   CONFIG_UINT(
         g_settings.autosave_interval,
         "autosave_interval",
         "SRAM Autosave",
         autosave_interval,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_cmd(list, list_info, RARCH_CMD_AUTOSAVE_INIT);
   settings_list_current_add_range(list, list_info, 0, 0, 10, true, false);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);
   (*list)[list_info->index - 1].get_string_representation = 
      &setting_data_get_string_representation_uint_autosave_interval;
#endif

   CONFIG_BOOL(
         g_settings.savestate_auto_index,
         "savestate_auto_index",
         "Save State Auto Index",
         savestate_auto_index,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.savestate_auto_save,
         "savestate_auto_save",
         "Auto Save State",
         savestate_auto_save,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
            general_read_handler);

   CONFIG_BOOL(
         g_settings.savestate_auto_load,
         "savestate_auto_load",
         "Auto Load State",
         savestate_auto_load,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);


   END_SUB_GROUP(list, list_info);

   START_SUB_GROUP(list, list_info, "Frame throttling", group_info.name, subgroup_info);

   CONFIG_BOOL(
         g_settings.fastforward_ratio_throttle_enable,
         "fastforward_ratio_throttle_enable",
         "Limit Maximum Run Speed",
         fastforward_ratio_throttle_enable,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_FLOAT(
         g_settings.fastforward_ratio,
         "fastforward_ratio",
         "Maximum Run Speed",
         fastforward_ratio,
         "%.1fx",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 1, 10, 0.1, true, true);

   CONFIG_FLOAT(
         g_settings.slowmotion_ratio,
         "slowmotion_ratio",
         "Slow-Motion Ratio",
         slowmotion_ratio,
         "%.1fx",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 1, 10, 1.0, true, true);

   END_SUB_GROUP(list, list_info);

   END_GROUP(list, list_info);

   return true;
}

static bool setting_data_append_list_video_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;

   START_GROUP(group_info, "Video Settings");
   START_SUB_GROUP(list, list_info, "State", group_info.name, subgroup_info);

   CONFIG_BOOL(g_settings.fps_show,
         "fps_show",
         "Show Framerate",
         fps_show,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.video.shared_context,
         "video_shared_context",
         "HW Shared Context Enable",
         false,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   END_SUB_GROUP(list, list_info);
   START_SUB_GROUP(list, list_info, "Monitor", group_info.name, subgroup_info);

   CONFIG_UINT(
         g_settings.video.monitor_index,
         "video_monitor_index",
         "Monitor Index",
         monitor_index,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_cmd(list, list_info, RARCH_CMD_REINIT);
   settings_list_current_add_range(list, list_info, 0, 1, 1, true, false);
   (*list)[list_info->index - 1].get_string_representation = 
      &setting_data_get_string_representation_uint_video_monitor_index;

#if !defined(RARCH_CONSOLE) && !defined(RARCH_MOBILE)
   CONFIG_BOOL(
         g_settings.video.fullscreen,
         "video_fullscreen",
         "Use Fullscreen mode",
         fullscreen,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_cmd(list, list_info, RARCH_CMD_REINIT);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);
#endif
   CONFIG_BOOL(
         g_settings.video.windowed_fullscreen,
         "video_windowed_fullscreen",
         "Windowed Fullscreen Mode",
         windowed_fullscreen,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_UINT(
         g_settings.video.fullscreen_x,
         "video_fullscreen_x",
         "Fullscreen Width",
         fullscreen_x,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_UINT(
         g_settings.video.fullscreen_y,
         "video_fullscreen_y",
         "Fullscreen Height",
         fullscreen_y,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_FLOAT(
         g_settings.video.refresh_rate,
         "video_refresh_rate",
         "Refresh Rate",
         refresh_rate,
         "%.3f Hz",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 0, 0, 0.001, true, false);

   CONFIG_BOOL(g_settings.fps_monitor_enable,
         "fps_monitor_enable",
         "Monitor FPS Enable",
         true,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_FLOAT(
         g_settings.video.refresh_rate,
         "video_refresh_rate_auto",
         "Estimated Monitor FPS",
         refresh_rate,
         "%.3f Hz",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   (*list)[list_info->index - 1].action_start = 
      &setting_data_action_start_video_refresh_rate_auto;
   (*list)[list_info->index - 1].action_ok = 
      &setting_data_action_ok_video_refresh_rate_auto;
   (*list)[list_info->index - 1].get_string_representation = 
      &setting_data_get_string_representation_st_float_video_refresh_rate_auto;

   CONFIG_BOOL(
         g_settings.video.force_srgb_disable,
         "video_force_srgb_disable",
         "Force-disable sRGB FBO",
         false,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_cmd(list, list_info, RARCH_CMD_REINIT);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

   END_SUB_GROUP(list, list_info);

   START_SUB_GROUP(list, list_info, "Aspect", group_info.name, subgroup_info);
   CONFIG_BOOL(
         g_settings.video.force_aspect,
         "video_force_aspect",
         "Force aspect ratio",
         force_aspect,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_FLOAT(
         g_settings.video.aspect_ratio,
         "video_aspect_ratio",
         "Aspect Ratio",
         aspect_ratio,
         "%.2f",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.video.aspect_ratio_auto,
         "video_aspect_ratio_auto",
         "Use Auto Aspect Ratio",
         aspect_ratio_auto,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_UINT(
         g_settings.video.aspect_ratio_idx,
         "aspect_ratio_index",
         "Aspect Ratio Index",
         aspect_ratio_idx,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_cmd(
         list,
         list_info,
         RARCH_CMD_VIDEO_SET_ASPECT_RATIO);
   settings_list_current_add_range(
         list,
         list_info,
         0,
         LAST_ASPECT_RATIO,
         1,
         true,
         true);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);
   (*list)[list_info->index - 1].get_string_representation = 
      &setting_data_get_string_representation_uint_aspect_ratio_index;

   END_SUB_GROUP(list, list_info);

   START_SUB_GROUP(list, list_info, "Scaling", group_info.name, subgroup_info);

#if !defined(RARCH_CONSOLE) && !defined(RARCH_MOBILE)
   CONFIG_FLOAT(
         g_settings.video.scale,
         "video_scale",
         "Windowed Scale",
         scale,
         "%.1fx",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 1.0, 10.0, 1.0, true, true);
#endif

   CONFIG_BOOL(
         g_settings.video.scale_integer,
         "video_scale_integer",
         "Integer Scale",
         scale_integer,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_INT(
         g_extern.console.screen.viewports.custom_vp.x,
         "custom_viewport_x",
         "Custom Viewport X",
         0,
         group_info.name,
         subgroup_info.name,
         NULL,
         NULL);
   
   CONFIG_INT(
         g_extern.console.screen.viewports.custom_vp.y,
         "custom_viewport_y",
         "Custom Viewport Y",
         0,
         group_info.name,
         subgroup_info.name,
         NULL,
         NULL);

   CONFIG_UINT(
         g_extern.console.screen.viewports.custom_vp.width,
         "custom_viewport_width",
         "Custom Viewport Width",
         0,
         group_info.name,
         subgroup_info.name,
         NULL,
         NULL);

   CONFIG_UINT(
         g_extern.console.screen.viewports.custom_vp.height,
         "custom_viewport_height",
         "Custom Viewport Height",
         0,
         group_info.name,
         subgroup_info.name,
         NULL,
         NULL);

#ifdef GEKKO
   CONFIG_UINT(
         g_settings.video.viwidth,
         "video_viwidth",
         "Set Screen Width",
         video_viwidth,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 640, 720, 2, true, true);

   CONFIG_BOOL(
         g_settings.video.vfilter,
         "video_vfilter",
         "Deflicker",
         video_vfilter,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
#endif

   CONFIG_BOOL(
         g_settings.video.smooth,
         "video_smooth",
         "Use Bilinear Filtering",
         video_smooth,
         "Point filtering",
         "Bilinear filtering",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

#if defined(__CELLOS_LV2__)
   CONFIG_BOOL(
         g_extern.console.screen.pal60_enable,
         "pal60_enable",
         "Use PAL60 Mode",
         false,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
#endif

   CONFIG_UINT(
         g_settings.video.rotation,
         "video_rotation",
         "Rotation",
         0,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 0, 3, 1, true, true);
   (*list)[list_info->index - 1].get_string_representation = 
      &setting_data_get_string_representation_uint_video_rotation;

#if defined(HW_RVL) || defined(_XBOX360)
   CONFIG_UINT(
         g_extern.console.screen.gamma_correction,
         "video_gamma",
         "Gamma",
         0,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_cmd(
         list,
         list_info,
         RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);
   settings_list_current_add_range(
         list,
         list_info,
         0,
         MAX_GAMMA_SETTING,
         1,
         true,
         true);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);
#endif
   END_SUB_GROUP(list, list_info);

   START_SUB_GROUP(
         list,
         list_info,
         "Synchronization",
         group_info.name,
         subgroup_info);

#if defined(HAVE_THREADS) && !defined(RARCH_CONSOLE)
   CONFIG_BOOL(
         g_settings.video.threaded,
         "video_threaded",
         "Threaded Video",
         video_threaded,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_cmd(list, list_info, RARCH_CMD_REINIT);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);
#endif

   CONFIG_BOOL(
         g_settings.video.vsync,
         "video_vsync",
         "VSync",
         vsync,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_UINT(
         g_settings.video.swap_interval,
         "video_swap_interval",
         "VSync Swap Interval",
         swap_interval,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_cmd(list, list_info, RARCH_CMD_VIDEO_SET_BLOCKING_STATE);
   settings_list_current_add_range(list, list_info, 1, 4, 1, true, true);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

   CONFIG_BOOL(
         g_settings.video.hard_sync,
         "video_hard_sync",
         "Hard GPU Sync",
         hard_sync,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_UINT(
         g_settings.video.hard_sync_frames,
         "video_hard_sync_frames",
         "Hard GPU Sync Frames",
         hard_sync_frames,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 0, 3, 1, true, true);

   CONFIG_UINT(
         g_settings.video.frame_delay,
         "video_frame_delay",
         "Frame Delay",
         frame_delay,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 0, 15, 1, true, true);
#if !defined(RARCH_MOBILE)
   CONFIG_BOOL(
         g_settings.video.black_frame_insertion,
         "video_black_frame_insertion",
         "Black Frame Insertion",
         black_frame_insertion,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
#endif
   END_SUB_GROUP(list, list_info);

   START_SUB_GROUP(
         list,
         list_info,
         "Miscellaneous",
         group_info.name,
         subgroup_info);

   CONFIG_BOOL(
         g_settings.video.post_filter_record,
         "video_post_filter_record",
         "Post filter record Enable",
         post_filter_record,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.video.gpu_record,
         "video_gpu_record",
         "GPU Record Enable",
         gpu_record,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.video.gpu_screenshot,
         "video_gpu_screenshot",
         "GPU Screenshot Enable",
         gpu_screenshot,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.video.allow_rotate,
         "video_allow_rotate",
         "Allow rotation",
         allow_rotate,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.video.crop_overscan,
         "video_crop_overscan",
         "Crop Overscan (reload)",
         crop_overscan,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

#if defined(_XBOX1) || defined(HW_RVL)
   CONFIG_BOOL(
         g_extern.console.softfilter_enable,
         "soft_filter",
         "Soft Filter Enable",
         false,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_cmd(
         list,
         list_info,
         RARCH_CMD_VIDEO_APPLY_STATE_CHANGES);
#endif

#ifdef _XBOX1
   CONFIG_UINT(
         g_settings.video.swap_interval,
         "video_filter_flicker",
         "Flicker filter",
         0,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 0, 5, 1, true, true);
#endif
   END_SUB_GROUP(list, list_info);
   END_GROUP(list, list_info);

   return true;
}

static bool setting_data_append_list_font_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;

   START_GROUP(group_info, "Font Settings");
   START_SUB_GROUP(list, list_info, "Messages", group_info.name, subgroup_info);

   CONFIG_PATH(
         g_settings.video.font_path,
         "video_font_path",
         "Font Path",
         "",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_EMPTY);

   CONFIG_FLOAT(
         g_settings.video.font_size,
         "video_font_size",
         "OSD Font Size",
         font_size,
         "%.1f",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 1.00, 100.00, 1.0, true, true);

#ifndef RARCH_CONSOLE
   CONFIG_BOOL(
         g_settings.video.font_enable,
         "video_font_enable",
         "OSD Font Enable",
         font_enable,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
#endif

   CONFIG_FLOAT(
         g_settings.video.msg_pos_x,
         "video_message_pos_x",
         "Message X Position",
         message_pos_offset_x,
         "%.3f",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 0, 1, 0.01, true, true);

   CONFIG_FLOAT(
         g_settings.video.msg_pos_y,
         "video_message_pos_y",
         "Message Y Position",
         message_pos_offset_y,
         "%.3f",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 0, 1, 0.01, true, true);

   END_SUB_GROUP(list, list_info);
   END_GROUP(list, list_info);

   return true;
}

static bool setting_data_append_list_audio_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;

   START_GROUP(group_info, "Audio Settings");
   START_SUB_GROUP(list, list_info, "State", group_info.name, subgroup_info);

   CONFIG_BOOL(
         g_settings.audio.enable,
         "audio_enable",
         "Audio Enable",
         audio_enable,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.audio.mute_enable,
         "audio_mute_enable",
         "Audio Mute",
         false,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_FLOAT(
         g_settings.audio.volume,
         "audio_volume",
         "Volume Level",
         audio_volume,
         "%.1f",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, -80, 12, 1.0, true, true);

#ifdef __CELLOS_LV2__
   CONFIG_BOOL(
         g_extern.console.sound.system_bgm_enable,
         "system_bgm_enable",
         "System BGM Enable",
         false,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
#endif

   END_SUB_GROUP(list, list_info);

   START_SUB_GROUP(
         list,
         list_info,
         "Synchronization",
         group_info.name,
         subgroup_info);

   CONFIG_BOOL(
         g_settings.audio.sync,
         "audio_sync",
         "Audio Sync Enable",
         audio_sync,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_UINT(
         g_settings.audio.latency,
         "audio_latency",
         "Audio Latency",
         g_defaults.settings.out_latency ? 
         g_defaults.settings.out_latency : out_latency,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 1, 256, 1.0, true, true);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_IS_DEFERRED);

   CONFIG_FLOAT(
         g_settings.audio.rate_control_delta,
         "audio_rate_control_delta",
         "Audio Rate Control Delta",
         rate_control_delta,
         "%.3f",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(
         list,
         list_info,
         0,
         0,
         0.001,
         true,
         false);

   CONFIG_FLOAT(
         g_settings.audio.max_timing_skew,
         "audio_max_timing_skew",
         "Audio Maximum Timing Skew",
         max_timing_skew,
         "%.2f",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(
         list,
         list_info,
         0.01,
         0.5,
         0.01,
         true,
         true);

   CONFIG_UINT(
         g_settings.audio.block_frames,
         "audio_block_frames",
         "Block Frames",
         0,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   END_SUB_GROUP(list, list_info);

   START_SUB_GROUP(
         list,
         list_info,
         "Miscellaneous",
         group_info.name,
         subgroup_info);

   CONFIG_STRING(
         g_settings.audio.device,
         "audio_device",
         "Device",
         "",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

   CONFIG_UINT(
         g_settings.audio.out_rate,
         "audio_out_rate",
         "Audio Output Rate",
         out_rate,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_PATH(
         g_settings.audio.dsp_plugin,
         "audio_dsp_plugin",
         "DSP Plugin",
         g_settings.audio.filter_dir,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_values(list, list_info, "dsp");
   settings_list_current_add_cmd(list, list_info, RARCH_CMD_DSP_FILTER_INIT);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_EMPTY);

   END_SUB_GROUP(list, list_info);
   END_GROUP(list, list_info);

   return true;
}

static bool setting_data_append_list_input_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;
   unsigned i, user;

   START_GROUP(group_info, "Input Settings");
   START_SUB_GROUP(list, list_info, "State", group_info.name, subgroup_info);

   CONFIG_UINT(
         g_settings.input.max_users,
         "input_max_users",
         "Max Users",
         MAX_USERS,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 1, MAX_USERS, 1, true, true);

   CONFIG_BOOL(
         g_settings.input.remap_binds_enable,
         "input_remap_binds_enable",
         "Remap Binds Enable",
         true,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.input.autodetect_enable,
         "input_autodetect_enable",
         "Autoconfig Enable",
         input_autodetect_enable,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.input.autoconfig_descriptor_label_show,
         "autoconfig_descriptor_label_show",
         "Show Autoconfig Descriptor Labels",
         true,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.input.input_descriptor_label_show,
         "input_descriptor_label_show",
         "Show Core Input Descriptor Labels",
         input_descriptor_label_show,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.input.input_descriptor_hide_unbound,
         "input_descriptor_hide_unbound",
         "Hide Unbound Core Input Descriptors",
         input_descriptor_hide_unbound,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   END_SUB_GROUP(list, list_info);

   START_SUB_GROUP(
         list,
         list_info,
         "Joypad Mapping",
         group_info.name,
         subgroup_info);

   CONFIG_BOOL(
         g_extern.menu.bind_mode_keyboard,
         "input_bind_mode",
         "Bind Mode",
         false,
         "RetroPad",
         "RetroKeyboard",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   for (user = 0; user < g_settings.input.max_users; user ++)
   {
      /* These constants match the string lengths.
       * Keep them up to date or you'll get some really obvious bugs.
       * 2 is the length of '99'; we don't need more users than that.
       */
      /* FIXME/TODO - really need to clean up this mess in some way. */
      static char key[MAX_USERS][64];
      static char key_type[MAX_USERS][64];
      static char key_analog[MAX_USERS][64];
      static char key_bind_all[MAX_USERS][64];
      static char key_bind_defaults[MAX_USERS][64];

      static char label[MAX_USERS][64];
      static char label_type[MAX_USERS][64];
      static char label_analog[MAX_USERS][64];
      static char label_bind_all[MAX_USERS][64];
      static char label_bind_defaults[MAX_USERS][64];

      snprintf(key[user], sizeof(key[user]),
               "input_player%d_joypad_index", user + 1);
      snprintf(key_type[user], sizeof(key_type[user]),
               "input_libretro_device_p%u", user + 1);
      snprintf(key_analog[user], sizeof(key_analog[user]),
               "input_player%u_analog_dpad_mode", user + 1);
      snprintf(key_bind_all[user], sizeof(key_bind_all[user]),
               "input_player%u_bind_all", user + 1);
      snprintf(key_bind_defaults[user], sizeof(key_bind_defaults[user]),
               "input_player%u_bind_defaults", user + 1);

      snprintf(label[user], sizeof(label[user]),
               "User %d Device Index", user + 1);
      snprintf(label_type[user], sizeof(label_type[user]),
               "User %d Device Type", user + 1);
      snprintf(label_analog[user], sizeof(label_analog[user]),
               "User %d Analog To Digital Type", user + 1);
      snprintf(label_bind_all[user], sizeof(label_bind_all[user]),
               "User %d Bind All", user + 1);
      snprintf(label_bind_defaults[user], sizeof(label_bind_defaults[user]),
               "User %d Bind Default All", user + 1);

      CONFIG_UINT(
            g_settings.input.libretro_device[user],
            key_type[user],
            label_type[user],
            user,
            group_info.name,
            subgroup_info.name,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index = user + 1;
      (*list)[list_info->index - 1].index_offset = user;
      (*list)[list_info->index - 1].action_toggle = &setting_data_action_toggle_libretro_device_type;
      (*list)[list_info->index - 1].action_start = &setting_data_action_start_libretro_device_type;
      (*list)[list_info->index - 1].get_string_representation = 
         &setting_data_get_string_representation_uint_libretro_device;

      CONFIG_UINT(
            g_settings.input.analog_dpad_mode[user],
            key_analog[user],
            label_analog[user],
            user,
            group_info.name,
            subgroup_info.name,
            general_write_handler,
            general_read_handler);
      (*list)[list_info->index - 1].index = user + 1;
      (*list)[list_info->index - 1].index_offset = user;
      (*list)[list_info->index - 1].action_toggle = &setting_data_action_toggle_analog_dpad_mode;
      (*list)[list_info->index - 1].action_start = &setting_data_action_start_analog_dpad_mode;
      (*list)[list_info->index - 1].get_string_representation = 
         &setting_data_get_string_representation_uint_analog_dpad_mode;

      CONFIG_ACTION(
            key[user],
            label[user],
            group_info.name,
            subgroup_info.name);
      (*list)[list_info->index - 1].index = user + 1;
      (*list)[list_info->index - 1].index_offset = user;
      (*list)[list_info->index - 1].action_start  = &setting_data_action_start_bind_device;
      (*list)[list_info->index - 1].action_toggle = &setting_data_action_toggle_bind_device;
      (*list)[list_info->index - 1].get_string_representation = &get_string_representation_bind_device;

      CONFIG_ACTION(
            key_bind_all[user],
            label_bind_all[user],
            group_info.name,
            subgroup_info.name);
      (*list)[list_info->index - 1].index          = user + 1;
      (*list)[list_info->index - 1].index_offset   = user;
      (*list)[list_info->index - 1].action_ok      = &setting_data_action_ok_bind_all;
      (*list)[list_info->index - 1].action_cancel  = NULL;

      CONFIG_ACTION(
            key_bind_defaults[user],
            label_bind_defaults[user],
            group_info.name,
            subgroup_info.name);
      (*list)[list_info->index - 1].index          = user + 1;
      (*list)[list_info->index - 1].index_offset   = user;
      (*list)[list_info->index - 1].action_ok      = &setting_data_action_ok_bind_defaults;
      (*list)[list_info->index - 1].action_cancel  = NULL;
   }

   START_SUB_GROUP(
         list,
         list_info,
         "Turbo/Deadzone",
         group_info.name,
         subgroup_info);

   CONFIG_FLOAT(
         g_settings.input.axis_threshold,
         "input_axis_threshold",
         "Input Axis Threshold",
         axis_threshold,
         "%.3f",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 0, 1.00, 0.001, true, true);

   CONFIG_UINT(
         g_settings.input.turbo_period,
         "input_turbo_period",
         "Turbo Period",
         turbo_period,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 1, 0, 1, true, false);

   CONFIG_UINT(
         g_settings.input.turbo_duty_cycle,
         "input_duty_cycle",
         "Duty Cycle",
         turbo_duty_cycle,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 1, 0, 1, true, false);

   END_SUB_GROUP(list, list_info);

   /* The second argument to config bind is 1 
    * based for users and 0 only for meta keys. */
   START_SUB_GROUP(
         list,
         list_info,
         "Meta Keys",
         group_info.name,
         subgroup_info);

   for (i = 0; i < RARCH_BIND_LIST_END; i ++)
   {
      const struct input_bind_map* keybind = (const struct input_bind_map*)
         &input_config_bind_map[i];

      if (!keybind || !keybind->meta)
         continue;

      CONFIG_BIND(g_settings.input.binds[0][i], 0, 0,
            keybind->base, keybind->desc, &retro_keybinds_1[i],
            group_info.name, subgroup_info.name);
      settings_list_current_add_bind_type(list, list_info, i + MENU_SETTINGS_BIND_BEGIN);
   }
   END_SUB_GROUP(list, list_info);

   for (user = 0; user < g_settings.input.max_users; user++)
   {
      /* This constants matches the string length.
       * Keep it up to date or you'll get some really obvious bugs.
       * 2 is the length of '99'; we don't need more users than that.
       */
      static char buffer[MAX_USERS][7+2+1];
      const struct retro_keybind* const defaults =
         (user == 0) ? retro_keybinds_1 : retro_keybinds_rest;

      snprintf(buffer[user], sizeof(buffer[user]), "User %d", user + 1);

      START_SUB_GROUP(
            list,
            list_info,
            buffer[user],
            group_info.name,
            subgroup_info);

      for (i = 0; i < RARCH_BIND_LIST_END; i ++)
      {
         char label[PATH_MAX_LENGTH];
         char name[PATH_MAX_LENGTH];
         bool do_add = true;
         const struct input_bind_map* keybind = 
            (const struct input_bind_map*)&input_config_bind_map[i];

         if (!keybind || keybind->meta)
            continue;

         if (
               g_settings.input.input_descriptor_label_show
               && (i < RARCH_FIRST_META_KEY)
               && (g_extern.has_set_input_descriptors)
               && (i != RARCH_TURBO_ENABLE)
               )
         {
            if (g_extern.system.input_desc_btn[user][i])
               snprintf(label, sizeof(label), "%s %s", buffer[user],
                     g_extern.system.input_desc_btn[user][i]);
            else
            {
               snprintf(label, sizeof(label), "%s %s", buffer[user], "N/A");

               if (g_settings.input.input_descriptor_hide_unbound)
                  do_add = false;
            }
         }
         else
            snprintf(label, sizeof(label), "%s %s", buffer[user], keybind->desc);

         snprintf(name, sizeof(name), "p%u_%s", user + 1, keybind->base);

         if (do_add)
         {
            CONFIG_BIND(
                  g_settings.input.binds[user][i],
                  user + 1,
                  user,
                  strdup(name), /* TODO: Find a way to fix these memleaks. */
                  strdup(label),
                  &defaults[i],
                  group_info.name,
                  subgroup_info.name);
            settings_list_current_add_bind_type(list, list_info, i + MENU_SETTINGS_BIND_BEGIN);
         }
      }
      END_SUB_GROUP(list, list_info);
   }

   START_SUB_GROUP(
         list,
         list_info,
         "Miscellaneous",
         group_info.name,
         subgroup_info);

   CONFIG_BOOL(
         g_settings.input.netplay_client_swap_input,
         "netplay_client_swap_input",
         "Swap Netplay Input",
         netplay_client_swap_input,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   END_SUB_GROUP(list, list_info);
   END_GROUP(list, list_info);

   return true;
}

static bool setting_data_append_list_overlay_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
#ifdef HAVE_OVERLAY
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;

   START_GROUP(group_info, "Overlay Settings");
   START_SUB_GROUP(list, list_info, "State", group_info.name, subgroup_info);

   CONFIG_BOOL(
         g_settings.input.overlay_enable,
         "input_overlay_enable",
         "Overlay Enable",
         true,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   (*list)[list_info->index - 1].change_handler = overlay_enable_toggle_change_handler;

   CONFIG_PATH(
         g_settings.input.overlay,
         "input_overlay",
         "Overlay Preset",
         g_extern.overlay_dir,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_values(list, list_info, "cfg");
   settings_list_current_add_cmd(list, list_info, RARCH_CMD_OVERLAY_INIT);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_EMPTY);

   CONFIG_FLOAT(
         g_settings.input.overlay_opacity,
         "input_overlay_opacity",
         "Overlay Opacity",
         0.7f,
         "%.2f",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_cmd(list, list_info, RARCH_CMD_OVERLAY_SET_ALPHA_MOD);
   settings_list_current_add_range(list, list_info, 0, 1, 0.01, true, true);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

   CONFIG_FLOAT(
         g_settings.input.overlay_scale,
         "input_overlay_scale",
         "Overlay Scale",
         1.0f,
         "%.2f",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_cmd(list, list_info, RARCH_CMD_OVERLAY_SET_SCALE_FACTOR);
   settings_list_current_add_range(list, list_info, 0, 2, 0.01, true, true);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

   END_SUB_GROUP(list, list_info);
   END_GROUP(list, list_info);
#endif

   return true;
}

static bool setting_data_append_list_osk_overlay_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
#ifdef HAVE_OVERLAY
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;

   START_GROUP(group_info, "Onscreen Keyboard Overlay Settings");
   START_SUB_GROUP(list, list_info, "State", group_info.name, subgroup_info);

   CONFIG_BOOL(
         g_settings.osk.enable,
         "input_osk_overlay_enable",
         "OSK Overlay Enable",
         true,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_PATH(
         g_settings.osk.overlay,
         "input_osk_overlay",
         "OSK Overlay Preset",
         g_extern.osk_overlay_dir,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_values(list, list_info, "cfg");
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_EMPTY);

   END_SUB_GROUP(list, list_info);
   END_GROUP(list, list_info);
#endif

   return true;
}

static bool setting_data_append_list_menu_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
#ifdef HAVE_MENU
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;

   START_GROUP(group_info, "Menu Settings");
   START_SUB_GROUP(list, list_info, "State", group_info.name, subgroup_info);

   CONFIG_PATH(
         g_settings.menu.wallpaper,
         "menu_wallpaper",
         "Menu Wallpaper",
         "",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_values(list, list_info, "png");
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_EMPTY);

   CONFIG_BOOL(
         g_settings.menu.throttle,
         "menu_throttle",
         "Throttle menu speed",
         false,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);


   CONFIG_BOOL(
         g_settings.menu.pause_libretro,
         "menu_pause_libretro",
         "Pause Libretro",
         true,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_cmd(list, list_info, RARCH_CMD_MENU_PAUSE_LIBRETRO);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

   CONFIG_BOOL(
         g_settings.menu.mouse_enable,
         "menu_mouse_enable",
         "Mouse Enable",
         false,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   END_SUB_GROUP(list, list_info);

   START_SUB_GROUP(list, list_info, "Navigation", group_info.name, subgroup_info);

   CONFIG_BOOL(
         g_settings.menu.navigation.wraparound.horizontal_enable,
         "menu_navigation_wraparound_horizontal_enable",
         "Navigation Wrap-Around Horizontal",
         true,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.menu.navigation.wraparound.vertical_enable,
         "menu_navigation_wraparound_vertical_enable",
         "Navigation Wrap-Around Vertical",
         true,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   END_SUB_GROUP(list, list_info);

   START_SUB_GROUP(list, list_info, "Settings View", group_info.name, subgroup_info);

   CONFIG_BOOL(
         g_settings.menu.collapse_subgroups_enable,
         "menu_collapse_subgroups_enable",
         "Collapse SubGroups",
         collapse_subgroups_enable,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   END_SUB_GROUP(list, list_info);

   START_SUB_GROUP(list, list_info, "Browser", group_info.name, subgroup_info);

   CONFIG_BOOL(
         g_settings.menu.navigation.browser.filter.supported_extensions_enable,
         "menu_navigation_browser_filter_supported_extensions_enable",
         "Browser - Filter by supported extensions",
         true,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.menu_show_start_screen,
         "rgui_show_start_screen",
         "Show Start Screen",
         menu_show_start_screen,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.menu.timedate_enable,
         "menu_timedate_enable",
         "Show time / date",
         true,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.menu.core_enable,
         "menu_core_enable",
         "Show core name",
         true,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);


   END_SUB_GROUP(list, list_info);

   END_GROUP(list, list_info);
#endif

   return true;
}

static bool setting_data_append_list_ui_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;

   START_GROUP(group_info, "UI Settings");
   START_SUB_GROUP(list, list_info, "State", group_info.name, subgroup_info);

   CONFIG_BOOL(
         g_settings.video.disable_composition,
         "video_disable_composition",
         "Window Compositing Disable Hint",
         disable_composition,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_cmd(list, list_info, RARCH_CMD_REINIT);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO);

   CONFIG_BOOL(
         g_settings.pause_nonactive,
         "pause_nonactive",
         "Window Unfocus Pause Hint",
         pause_nonactive,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.ui.menubar_enable,
         "ui_menubar_enable",
         "Menubar Enable Hint",
         true,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.ui.suspend_screensaver_enable,
         "suspend_screensaver_enable",
         "Suspend Screensaver Enable Hint",
         true,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   END_SUB_GROUP(list, list_info);

   END_GROUP(list, list_info);

   return true;
}

static bool setting_data_append_list_archive_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;

   START_GROUP(group_info, "Archive Settings");
   START_SUB_GROUP(list, list_info, "State", group_info.name, subgroup_info);

   CONFIG_UINT(
         g_settings.archive.mode,
         "archive_mode",
         "Archive Mode",
         0,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 0, 2, 1, true, true);
   (*list)[list_info->index - 1].get_string_representation = 
      &setting_data_get_string_representation_uint_archive_mode;

   END_SUB_GROUP(list, list_info);
   END_GROUP(list, list_info);

   return true;
}

static bool setting_data_append_list_core_updater_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
#ifdef HAVE_NETWORKING
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;

   START_GROUP(group_info, "Core Updater Settings");

   START_SUB_GROUP(list, list_info, "State", group_info.name, subgroup_info);

   CONFIG_STRING(
         g_settings.network.buildbot_url,
         "core_updater_buildbot_url",
         "Buildbot Core URL",
         buildbot_server_url, 
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

   CONFIG_STRING(
         g_settings.network.buildbot_assets_url,
         "core_updater_buildbot_assets_url",
         "Buildbot Assets URL",
         buildbot_assets_server_url, 
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

   CONFIG_BOOL(
         g_settings.network.buildbot_auto_extract_archive,
         "core_updater_auto_extract_archive",
         "Automatically extract downloaded archive",
         true,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   END_SUB_GROUP(list, list_info);
   END_GROUP(list, list_info);
#endif

   return true;
}

static bool setting_data_append_list_netplay_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
#ifdef HAVE_NETPLAY
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;

   START_GROUP(group_info, "Network Settings");

   START_SUB_GROUP(list, list_info, "Netplay", group_info.name, subgroup_info);

   CONFIG_BOOL(
         g_extern.netplay_enable,
         "netplay_enable",
         "Netplay Enable",
         false,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);


   CONFIG_STRING(
         g_extern.netplay_server,
         "netplay_ip_address",
         "IP Address",
         "",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

   CONFIG_BOOL(
         g_extern.netplay_is_client,
         "netplay_mode",
         "Netplay Client Enable",
         false,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_extern.netplay_is_spectate,
         "netplay_spectator_mode_enable",
         "Netplay Spectator Enable",
         false,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   
   CONFIG_UINT(
         g_extern.netplay_sync_frames,
         "netplay_delay_frames",
         "Netplay Delay Frames",
         0,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 0, 10, 1, true, false);

   CONFIG_UINT(
         g_extern.netplay_port,
         "netplay_tcp_udp_port",
         "Netplay TCP/UDP Port",
         RARCH_DEFAULT_PORT,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 1, 99999, 1, true, true);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

   END_SUB_GROUP(list, list_info);

   START_SUB_GROUP(
         list,
         list_info,
         "Miscellaneous",
         group_info.name,
         subgroup_info);

#if defined(HAVE_NETWORK_CMD)
   CONFIG_BOOL(
         g_settings.network_cmd_enable,
         "network_cmd_enable",
         "Network Commands",
         network_cmd_enable,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
#if 0
   CONFIG_INT(
         g_settings.network_cmd_port,
         "network_cmd_port",
         "Network Command Port",
         network_cmd_port,
         group_info.name,
         subgroup_info.name,
         NULL);
#endif
   CONFIG_BOOL(
         g_settings.stdin_cmd_enable,
         "stdin_cmd_enable",
         "stdin command",
         stdin_cmd_enable,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
#endif
   END_SUB_GROUP(list, list_info);
   END_GROUP(list, list_info);
#endif

   return true;
}

static bool setting_data_append_list_patch_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;

   START_GROUP(group_info, "Patch Settings");
   START_SUB_GROUP(list, list_info, "State", group_info.name, subgroup_info);

   CONFIG_BOOL(
         g_extern.ups_pref,
         "ups_pref",
         "UPS Patching Enable",
         true,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_extern.bps_pref,
         "bps_pref",
         "BPS Patching Enable",
         true,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_extern.ips_pref,
         "ips_pref",
         "IPS Patching Enable",
         true,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   END_SUB_GROUP(list, list_info);
   END_GROUP(list, list_info);

   return true;
}

static bool setting_data_append_list_playlist_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;

   START_GROUP(group_info, "Playlist Settings");
   START_SUB_GROUP(list, list_info, "History", group_info.name, subgroup_info);

   CONFIG_BOOL(
         g_settings.history_list_enable,
         "history_list_enable",
         "History List Enable",
         true,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_UINT(
         g_settings.content_history_size,
         "game_history_size",
         "History List Size",
         default_content_history_size,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(list, list_info, 0, 0, 1.0, true, false);

   END_SUB_GROUP(list, list_info);
   END_GROUP(list, list_info);

   return true;
}

static bool setting_data_append_list_user_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;

   START_GROUP(group_info, "User Settings");
   START_SUB_GROUP(list, list_info, "State", group_info.name, subgroup_info);

   CONFIG_STRING(
         g_settings.username,
         "netplay_nickname",
         "Username",
         "",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);

   CONFIG_UINT(
         g_settings.user_language,
         "user_language",
         "Language",
         def_user_language,
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_range(
         list,
         list_info,
         0,
         RETRO_LANGUAGE_LAST-1,
         1,
         true,
         true);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_INPUT);
   (*list)[list_info->index - 1].get_string_representation = 
      &setting_data_get_string_representation_uint_user_language;

   END_SUB_GROUP(list, list_info);
   END_GROUP(list, list_info);

   return true;
}

static bool setting_data_append_list_path_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;

   START_GROUP(group_info, "Path Settings");

   START_SUB_GROUP(list, list_info, "Paths", group_info.name, subgroup_info);
#ifdef HAVE_MENU
   CONFIG_DIR(
         g_settings.menu_content_directory,
         "rgui_browser_directory",
         "Browser Directory",
         "",
         "<default>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

   CONFIG_DIR(
         g_settings.content_directory,
         "content_directory",
         "Content Directory",
         "",
         "<default>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

   CONFIG_DIR(
         g_settings.assets_directory,
         "assets_directory",
         "Assets Directory",
         "",
         "<default>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

   CONFIG_DIR(
         g_settings.menu_config_directory,
         "rgui_config_directory",
         "Config Directory",
         "",
         "<default>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

#endif

   CONFIG_DIR(
         g_settings.libretro_directory,
         "libretro_dir_path",
         "Core Directory",
         g_defaults.core_dir,
         "<None>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_cmd(list, list_info, RARCH_CMD_CORE_INFO_INIT);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

   CONFIG_DIR(
         g_settings.libretro_info_path,
         "libretro_info_path",
         "Core Info Directory",
         g_defaults.core_info_dir,
         "<None>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_list_current_add_cmd(list, list_info, RARCH_CMD_CORE_INFO_INIT);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

#ifdef HAVE_LIBRETRODB
   CONFIG_DIR(
         g_settings.content_database,
         "content_database_path",
         "Content Database Directory",
         "",
         "<None>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

   CONFIG_DIR(
         g_settings.cursor_directory,
         "cursor_directory",
         "Cursor Directory",
         "",
         "<None>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);
#endif

   CONFIG_DIR(
         g_settings.cheat_database,
         "cheat_database_path",
         "Cheat Database Directory",
         "",
         "<None>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

   CONFIG_PATH(
         g_settings.content_history_path,
         "game_history_path",
         "Content History Path",
         "",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_ALLOW_EMPTY);

   CONFIG_DIR(
         g_settings.video.filter_dir,
         "video_filter_dir",
         "VideoFilter Directory",
         "",
         "<default>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

   CONFIG_DIR(
         g_settings.audio.filter_dir,
         "audio_filter_dir",
         "AudioFilter Directory",
         "",
         "<default>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

   CONFIG_DIR(
         g_settings.video.shader_dir,
         "video_shader_dir",
         "Shader Directory",
         g_defaults.shader_dir,
         "<default>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

#ifdef HAVE_OVERLAY
   CONFIG_DIR(
         g_extern.overlay_dir,
         "overlay_directory",
         "Overlay Directory",
         g_defaults.overlay_dir,
         "<default>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

   CONFIG_DIR(
         g_extern.osk_overlay_dir,
         "osk_overlay_directory",
         "OSK Overlay Directory",
         g_defaults.osk_overlay_dir,
         "<default>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);
#endif

   CONFIG_DIR(
         g_settings.screenshot_directory,
         "screenshot_directory",
         "Screenshot Directory",
         "",
         "<Content dir>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

   CONFIG_DIR(
         g_settings.input.autoconfig_dir,
         "joypad_autoconfig_dir",
         "Joypad Autoconfig Directory",
         "",
         "<default>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

   CONFIG_DIR(
         g_settings.input_remapping_directory,
         "input_remapping_directory",
         "Input Remapping Directory",
         "",
         "<None>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

   CONFIG_DIR(
         g_settings.playlist_directory,
         "playlist_directory",
         "Playlist Directory",
         "",
         "<default>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

   CONFIG_DIR(
         g_extern.savefile_dir,
         "savefile_directory",
         "Savefile Directory",
         "",
         "<Content dir>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

   CONFIG_DIR(
         g_extern.savestate_dir,
         "savestate_directory",
         "Savestate Directory",
         "",
         "<Content dir>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

   CONFIG_DIR(
         g_settings.system_directory,
         "system_directory",
         "System Directory",
         "",
         "<Content dir>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);

   CONFIG_DIR(
         g_settings.extraction_directory,
         "extraction_directory",
         "Extraction Directory",
         "",
         "<None>",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   settings_data_list_current_add_flags(
         list,
         list_info,
         SD_FLAG_ALLOW_EMPTY | SD_FLAG_PATH_DIR | SD_FLAG_BROWSER_ACTION);
   END_SUB_GROUP(list, list_info);
   END_GROUP(list, list_info);

   return true;
}

static bool setting_data_append_list_privacy_options(
      rarch_setting_t **list,
      rarch_setting_info_t *list_info)
{
   rarch_setting_group_info_t group_info;
   rarch_setting_group_info_t subgroup_info;

   START_GROUP(group_info, "Privacy Settings");
   START_SUB_GROUP(list, list_info, "State", group_info.name, subgroup_info);

   CONFIG_BOOL(
         g_settings.camera.allow,
         "camera_allow",
         "Allow Camera",
         false,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);

   CONFIG_BOOL(
         g_settings.location.allow,
         "location_allow",
         "Allow Location",
         false,
         "OFF",
         "ON",
         group_info.name,
         subgroup_info.name,
         general_write_handler,
         general_read_handler);
   END_SUB_GROUP(list, list_info);
   END_GROUP(list, list_info);

   return true;
}


/**
 * setting_data_new:
 * @mask               : Bitmask of settings to include.
 *
 * Request a list of settings based on @mask.
 *
 * Returns: settings list composed of all requested
 * settings on success, otherwise NULL.
 **/
rarch_setting_t *setting_data_new(unsigned mask)
{
   rarch_setting_t terminator = { ST_NONE };
   rarch_setting_t* list = NULL;
   rarch_setting_t* resized_list = NULL;
   rarch_setting_info_t *list_info = (rarch_setting_info_t*)
      settings_info_list_new();
   if (!list_info)
      return NULL;

   list = (rarch_setting_t*)settings_list_new(list_info->size);
   if (!list)
      goto error;

   if (mask & SL_FLAG_MAIN_MENU)
   {
      if (!setting_data_append_list_main_menu_options(&list, list_info))
         goto error;
   }

   if (mask & SL_FLAG_DRIVER_OPTIONS)
   {
      if (!setting_data_append_list_driver_options(&list, list_info))
         goto error;
   }

   if (mask & SL_FLAG_GENERAL_OPTIONS)
   {
      if (!setting_data_append_list_general_options(&list, list_info))
         goto error;
   }

   if (mask & SL_FLAG_GENERAL_OPTIONS)
   {
      if (!setting_data_append_list_video_options(&list, list_info))
         goto error;
   }

   if (mask & SL_FLAG_FONT_OPTIONS)
   {
      if (!setting_data_append_list_font_options(&list, list_info))
         goto error;
   }

   if (mask & SL_FLAG_AUDIO_OPTIONS)
   {
      if (!setting_data_append_list_audio_options(&list, list_info))
         goto error;
   }

   if (mask & SL_FLAG_INPUT_OPTIONS)
   {
      if (!setting_data_append_list_input_options(&list, list_info))
         goto error;
   }

   if (mask & SL_FLAG_OVERLAY_OPTIONS)
   {
      if (!setting_data_append_list_overlay_options(&list, list_info))
         goto error;
   }
   
   if (mask & SL_FLAG_OSK_OVERLAY_OPTIONS)
   {
      if (!setting_data_append_list_osk_overlay_options(&list, list_info))
         goto error;
   }

   if (mask & SL_FLAG_MENU_OPTIONS)
   {
      if (!setting_data_append_list_menu_options(&list, list_info))
         goto error;
   }

   if (mask & SL_FLAG_UI_OPTIONS)
   {
      if (!setting_data_append_list_ui_options(&list, list_info))
         goto error;
   }

   if (mask & SL_FLAG_PATCH_OPTIONS)
   {
      if (!setting_data_append_list_patch_options(&list, list_info))
         goto error;
   }

   if (mask & SL_FLAG_PLAYLIST_OPTIONS)
   {
      if (!setting_data_append_list_playlist_options(&list, list_info))
         goto error;
   }

   if (mask & SL_FLAG_CORE_UPDATER_OPTIONS)
   {
      if (!setting_data_append_list_core_updater_options(&list, list_info))
         goto error;
   }

   if (mask & SL_FLAG_NETPLAY_OPTIONS)
   {
      if (!setting_data_append_list_netplay_options(&list, list_info))
         goto error;
   }

   if (mask & SL_FLAG_ARCHIVE_OPTIONS)
   {
      if (!setting_data_append_list_archive_options(&list, list_info))
         goto error;
   }

   if (mask & SL_FLAG_USER_OPTIONS)
   {
      if (!setting_data_append_list_user_options(&list, list_info))
         goto error;
   }

   if (mask & SL_FLAG_PATH_OPTIONS)
   {
      if (!setting_data_append_list_path_options(&list, list_info))
         goto error;
   }

   if (mask & SL_FLAG_PRIVACY_OPTIONS)
   {
      if (!setting_data_append_list_privacy_options(&list, list_info))
         goto error;
   }

   if (!(settings_list_append(&list, list_info, terminator)))
      goto error;

   /* flatten this array to save ourselves some kilobytes. */
   resized_list = (rarch_setting_t*) realloc(list, list_info->index * sizeof(rarch_setting_t));
   if (resized_list)
      list = resized_list;
   else
      goto error;

   settings_info_list_free(list_info);

   return list;

error:
   RARCH_ERR("Allocation failed.\n");
   settings_info_list_free(list_info);
   settings_list_free(list);

   return NULL;
}
