/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "menu_common.h"
#include "menu_input_line_cb.h"
#include "menu_action.h"
#include "menu_entries.h"
#include "menu_shader.h"


int menu_action_setting_apply(rarch_setting_t *setting)
{
   if (setting->change_handler)
      setting->change_handler(setting);

   if (setting->flags & SD_FLAG_EXIT
         && setting->cmd_trigger.triggered)
   {
      setting->cmd_trigger.triggered = false;
      return -1;
   }

   return 0;
}

int menu_action_setting_boolean(
      rarch_setting_t *setting, unsigned action)
{
   if (setting->action_ok)
      setting->action_ok(setting, action);

   return menu_action_setting_apply(setting);
}

int menu_action_setting_unsigned_integer(
      rarch_setting_t *setting, unsigned action)
{
   if (setting->action_ok)
      setting->action_ok(setting, action);

   return menu_action_setting_apply(setting);
}

int menu_action_setting_fraction(
      rarch_setting_t *setting, unsigned action)
{
   if (setting->action_ok)
      setting->action_ok(setting, action);

   return menu_action_setting_apply(setting);
}

void menu_action_setting_driver(
      rarch_setting_t *setting, unsigned action)
{
   if (!strcmp(setting->name, "audio_resampler_driver"))
   {
      switch (action)
      {
         case MENU_ACTION_LEFT:
            find_prev_resampler_driver();
            break;
         case MENU_ACTION_RIGHT:
            find_next_resampler_driver();
            break;
      }
   }
   else if (setting->flags & SD_FLAG_IS_DRIVER)
   {
      const char *label    = setting->name;
      char *drv            = (char*)setting->value.string;
      size_t sizeof_driver = setting->size;

      switch (action)
      {
         case MENU_ACTION_LEFT:
            find_prev_driver(label, drv, sizeof_driver);
            break;
         case MENU_ACTION_RIGHT:
            find_next_driver(label, drv, sizeof_driver);
            break;
      }
   }
}

int menu_action_setting_set_current_string(
      rarch_setting_t *setting, const char *str)
{
   strlcpy(setting->value.string, str, setting->size);

   return menu_action_setting_apply(setting);
}

int menu_action_set_current_string_based_on_label(
      const char *label, const char *str)
{
   if (!strcmp(label, "video_shader_preset_save_as"))
      menu_shader_manager_save_preset(str, false);

   return 0;
}

int menu_action_setting_set_current_string_path(
      rarch_setting_t *setting, const char *dir, const char *path)
{
   fill_pathname_join(setting->value.string, dir, path, setting->size);

   return menu_action_setting_apply(setting);
}

static int menu_entries_set_current_path_selection(
      rarch_setting_t *setting, const char *start_path,
      const char *label, unsigned type,
      unsigned action)
{
   switch (action)
   {
      case MENU_ACTION_OK:
         menu_entries_push(driver.menu->menu_stack,
               start_path, label, type,
               driver.menu->selection_ptr);

         if (setting->cmd_trigger.idx != RARCH_CMD_NONE)
            setting->cmd_trigger.triggered = true;
         break;
      case MENU_ACTION_START:
         *setting->value.string = '\0';
         break;
   }

   return menu_action_setting_apply(setting);
}

static int menu_action_handle_setting(rarch_setting_t *setting,
      unsigned type, unsigned action)
{
   if (!setting)
      return -1;

   if (setting->type == ST_BOOL)
      return menu_action_setting_boolean(setting, action);
   if (setting->type == ST_UINT)
      return menu_action_setting_unsigned_integer(setting, action);
   if (setting->type == ST_FLOAT)
      return menu_action_setting_fraction(setting, action);
   if (setting->type == ST_PATH)
      return menu_entries_set_current_path_selection(setting,
            setting->default_value.string, setting->name, type, action);

   if (setting->type == ST_DIR)
   {
      if (action == MENU_ACTION_START)
      {
         *setting->value.string = '\0';
         return menu_action_setting_apply(setting);
      }
      return 0;
   }

   if (setting->type == ST_STRING)
   {
      if (
            (setting->flags & SD_FLAG_ALLOW_INPUT) || 
            type == MENU_FILE_LINEFEED_SWITCH)
      {
         switch (action)
         {
            case MENU_ACTION_OK:
               menu_key_start_line(driver.menu, setting->short_description,
                     setting->name, st_string_callback);
               break;
            case MENU_ACTION_START:
               *setting->value.string = '\0';
               break;
         }
      }
      else
         menu_action_setting_driver(setting, action);
   }

   return 0;
}

#ifdef GEKKO
enum
{
   GX_RESOLUTIONS_512_192 = 0,
   GX_RESOLUTIONS_598_200,
   GX_RESOLUTIONS_640_200,
   GX_RESOLUTIONS_384_224,
   GX_RESOLUTIONS_448_224,
   GX_RESOLUTIONS_480_224,
   GX_RESOLUTIONS_512_224,
   GX_RESOLUTIONS_576_224,
   GX_RESOLUTIONS_608_224,
   GX_RESOLUTIONS_640_224,
   GX_RESOLUTIONS_340_232,
   GX_RESOLUTIONS_512_232,
   GX_RESOLUTIONS_512_236,
   GX_RESOLUTIONS_336_240,
   GX_RESOLUTIONS_352_240,
   GX_RESOLUTIONS_384_240,
   GX_RESOLUTIONS_512_240,
   GX_RESOLUTIONS_530_240,
   GX_RESOLUTIONS_640_240,
   GX_RESOLUTIONS_512_384,
   GX_RESOLUTIONS_598_400,
   GX_RESOLUTIONS_640_400,
   GX_RESOLUTIONS_384_448,
   GX_RESOLUTIONS_448_448,
   GX_RESOLUTIONS_480_448,
   GX_RESOLUTIONS_512_448,
   GX_RESOLUTIONS_576_448,
   GX_RESOLUTIONS_608_448,
   GX_RESOLUTIONS_640_448,
   GX_RESOLUTIONS_340_464,
   GX_RESOLUTIONS_512_464,
   GX_RESOLUTIONS_512_472,
   GX_RESOLUTIONS_352_480,
   GX_RESOLUTIONS_384_480,
   GX_RESOLUTIONS_512_480,
   GX_RESOLUTIONS_530_480,
   GX_RESOLUTIONS_608_480,
   GX_RESOLUTIONS_640_480,
   GX_RESOLUTIONS_LAST,
};

unsigned menu_gx_resolutions[GX_RESOLUTIONS_LAST][2] = {
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

unsigned menu_current_gx_resolution = GX_RESOLUTIONS_640_480;
#endif

int menu_action_setting_set(unsigned type, const char *label,
      unsigned action)
{
   unsigned port = driver.menu->current_pad;
   const file_list_t *list = (const file_list_t*)driver.menu->selection_buf;

   /* Check if setting belongs to settings menu. */

   rarch_setting_t *setting = (rarch_setting_t*)setting_data_find_setting(
         driver.menu->list_settings, list->list[driver.menu->selection_ptr].label);

   if (setting)
      return menu_action_handle_setting(setting, type, action);

   /* Check if setting belongs to main menu. */

   setting = (rarch_setting_t*)setting_data_find_setting(
         driver.menu->list_mainmenu, list->list[driver.menu->selection_ptr].label);

   if (setting)
      return menu_action_handle_setting(setting, type, action);

   /* Fallback. */

   if (!strcmp(label, "input_bind_device_id"))
   {
      int *p = (int*)&g_settings.input.joypad_map[port];

      switch (action)
      {
         case MENU_ACTION_START:
            *p = port;
            break;
         case MENU_ACTION_LEFT:
            (*p)--;
            break;
         case MENU_ACTION_RIGHT:
            (*p)++;
            break;
      }

      if (*p < -1)
         *p = -1;
      else if (*p >= MAX_PLAYERS)
         *p = MAX_PLAYERS - 1;
   }
   else if (!strcmp(label, "input_bind_device_type"))
   {
      unsigned current_device, current_index, i, devices[128];
      const struct retro_controller_info *desc;
      unsigned types = 0;

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
      current_index = 0;
      for (i = 0; i < types; i++)
      {
         if (current_device == devices[i])
         {
            current_index = i;
            break;
         }
      }

      switch (action)
      {
         case MENU_ACTION_START:
            current_device = RETRO_DEVICE_JOYPAD;

            g_settings.input.libretro_device[port] = current_device;
            pretro_set_controller_port_device(port, current_device);
            break;

         case MENU_ACTION_LEFT:
            current_device = devices
               [(current_index + types - 1) % types];

            g_settings.input.libretro_device[port] = current_device;
            pretro_set_controller_port_device(port, current_device);
            break;

         case MENU_ACTION_RIGHT:
         case MENU_ACTION_OK:
            current_device = devices
               [(current_index + 1) % types];

            g_settings.input.libretro_device[port] = current_device;
            pretro_set_controller_port_device(port, current_device);
            break;
      }
   }
   else if (!strcmp(label, "input_bind_player_no"))
   {
      switch (action)
      {
         case MENU_ACTION_START:
            driver.menu->current_pad = 0;
            break;
         case MENU_ACTION_LEFT:
            if (driver.menu->current_pad != 0)
               driver.menu->current_pad--;
            break;
         case MENU_ACTION_RIGHT:
            if (driver.menu->current_pad < MAX_PLAYERS - 1)
               driver.menu->current_pad++;
            break;
      }

      if (port != driver.menu->current_pad)
         driver.menu->need_refresh = true;
      port = driver.menu->current_pad;
   }
   else if (!strcmp(label, "input_bind_analog_dpad_mode"))
   {
      switch (action)
      {
         case MENU_ACTION_START:
            g_settings.input.analog_dpad_mode[port] = 0;
            break;

         case MENU_ACTION_OK:
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
   }
   else
   {
      switch (type)
      {
#if defined(GEKKO)
         case MENU_SETTINGS_VIDEO_RESOLUTION:
            switch (action)
            {
               case MENU_ACTION_LEFT:
                  if (menu_current_gx_resolution > 0)
                     menu_current_gx_resolution--;
                  break;
               case MENU_ACTION_RIGHT:
                  if (menu_current_gx_resolution < GX_RESOLUTIONS_LAST - 1)
                  {
#ifdef HW_RVL
                     if ((menu_current_gx_resolution + 1) > GX_RESOLUTIONS_640_480)
                        if (CONF_GetVideo() != CONF_VIDEO_PAL)
                           return 0;
#endif

                     menu_current_gx_resolution++;
                  }
                  break;
               case MENU_ACTION_OK:
                  if (driver.video_data)
                     gx_set_video_mode(driver.video_data, menu_gx_resolutions
                           [menu_current_gx_resolution][0],
                           menu_gx_resolutions[menu_current_gx_resolution][1]);
                  break;
            }
            break;
#elif defined(__CELLOS_LV2__)
         case MENU_SETTINGS_VIDEO_RESOLUTION:
            switch (action)
            {
               case MENU_ACTION_LEFT:
                  if (g_extern.console.screen.resolutions.current.idx)
                  {
                     g_extern.console.screen.resolutions.current.idx--;
                     g_extern.console.screen.resolutions.current.id =
                        g_extern.console.screen.resolutions.list
                        [g_extern.console.screen.resolutions.current.idx];
                  }
                  break;
               case MENU_ACTION_RIGHT:
                  if (g_extern.console.screen.resolutions.current.idx + 1 <
                        g_extern.console.screen.resolutions.count)
                  {
                     g_extern.console.screen.resolutions.current.idx++;
                     g_extern.console.screen.resolutions.current.id =
                        g_extern.console.screen.resolutions.list
                        [g_extern.console.screen.resolutions.current.idx];
                  }
                  break;
               case MENU_ACTION_OK:
                  if (g_extern.console.screen.resolutions.list[
                        g_extern.console.screen.resolutions.current.idx] == 
                        CELL_VIDEO_OUT_RESOLUTION_576)
                  {
                     if (g_extern.console.screen.pal_enable)
                        g_extern.console.screen.pal60_enable = true;
                  }
                  else
                  {
                     g_extern.console.screen.pal_enable = false;
                     g_extern.console.screen.pal60_enable = false;
                  }

                  rarch_main_command(RARCH_CMD_REINIT);
                  break;
            }
#endif
            break;
      }
   }

   return 0;
}
