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

int menu_action_setting_boolean(
      rarch_setting_t *setting, unsigned action)
{
   if (
         !strcmp(setting->name, "savestate") ||
         !strcmp(setting->name, "loadstate"))
   {
      if (action == MENU_ACTION_START)
         g_settings.state_slot = 0;
      else if (action == MENU_ACTION_LEFT)
      {
         // Slot -1 is (auto) slot.
         if (g_settings.state_slot >= 0)
            g_settings.state_slot--;
      }
      else if (action == MENU_ACTION_RIGHT)
         g_settings.state_slot++;
      else if (action == MENU_ACTION_OK)
      {
         *setting->value.boolean = !(*setting->value.boolean);

         if (setting->cmd_trigger.idx != RARCH_CMD_NONE)
            setting->cmd_trigger.triggered = true;
      }
   }
   else
   {
      switch (action)
      {
         case MENU_ACTION_OK:
            if (setting->cmd_trigger.idx != RARCH_CMD_NONE)
               setting->cmd_trigger.triggered = true;
            /* fall-through */
         case MENU_ACTION_LEFT:
         case MENU_ACTION_RIGHT:
            *setting->value.boolean = !(*setting->value.boolean);
            break;
         case MENU_ACTION_START:
            *setting->value.boolean = setting->default_value.boolean;
            break;
      }
   }

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

int menu_action_setting_unsigned_integer(
      rarch_setting_t *setting, unsigned id, unsigned action)
{
   if (id == MENU_FILE_LINEFEED)
   {
      if (action == MENU_ACTION_OK)
         menu_key_start_line(driver.menu, setting->short_description,
               setting->name, st_uint_callback);
      else if (action == MENU_ACTION_START)
         *setting->value.unsigned_integer =
            setting->default_value.unsigned_integer;
   }
   else
   {
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

         case MENU_ACTION_OK:
            if (setting->cmd_trigger.idx != RARCH_CMD_NONE)
               setting->cmd_trigger.triggered = true;
            /* fall-through */
         case MENU_ACTION_RIGHT:
            *setting->value.unsigned_integer =
               *setting->value.unsigned_integer + setting->step;

            if (setting->enforce_maxrange)
            {
               if (*setting->value.unsigned_integer > setting->max)
                  *setting->value.unsigned_integer = setting->max;
            }
            break;

         case MENU_ACTION_START:
            *setting->value.unsigned_integer =
               setting->default_value.unsigned_integer;
            break;
      }
   } 

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

int menu_action_setting_fraction(
      rarch_setting_t *setting, unsigned action)
{
   if (!strcmp(setting->name, "video_refresh_rate_auto"))
   {
      if (action == MENU_ACTION_START)
         g_extern.measure_data.frame_time_samples_count = 0;
      else if (action == MENU_ACTION_OK)
      {
         double refresh_rate, deviation = 0.0;
         unsigned sample_points = 0;

         if (driver_monitor_fps_statistics(&refresh_rate,
                  &deviation, &sample_points))
         {
            driver_set_monitor_refresh_rate(refresh_rate);
            /* Incase refresh rate update forced non-block video. */
            rarch_main_command(RARCH_CMD_VIDEO_SET_BLOCKING_STATE);
         }

         if (setting->cmd_trigger.idx != RARCH_CMD_NONE)
            setting->cmd_trigger.triggered = true;
      }
   }
   else if (!strcmp(setting->name, "fastforward_ratio"))
   {
      bool clamp_value = false;
      if (action == MENU_ACTION_START)
        *setting->value.fraction  = setting->default_value.fraction;
      else if (action == MENU_ACTION_LEFT)
      {
         *setting->value.fraction -= setting->step;

         /* Avoid potential rounding errors when going from 1.1 to 1.0. */
         if (*setting->value.fraction < 0.95f) 
            *setting->value.fraction = setting->default_value.fraction;
         else
            clamp_value = true;
      }
      else if (action == MENU_ACTION_RIGHT)
      {
         *setting->value.fraction += setting->step;
         clamp_value = true;
      }
      if (clamp_value)
         g_settings.fastforward_ratio =
            max(min(*setting->value.fraction, setting->max), 1.0f);
   }
   else
   {
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

         case MENU_ACTION_OK:
            if (setting->cmd_trigger.idx != RARCH_CMD_NONE)
               setting->cmd_trigger.triggered = true;
            /* fall-through */
         case MENU_ACTION_RIGHT:
            *setting->value.fraction = 
               *setting->value.fraction + setting->step;

            if (setting->enforce_maxrange)
            {
               if (*setting->value.fraction > setting->max)
                  *setting->value.fraction = setting->max;
            }
            break;

         case MENU_ACTION_START:
            *setting->value.fraction = setting->default_value.fraction;
            break;
      }
   }

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

void menu_action_setting_driver(
      rarch_setting_t *setting, unsigned action)
{
   if (!strcmp(setting->name, "audio_resampler_driver"))
   {
      if (action == MENU_ACTION_LEFT)
         find_prev_resampler_driver();
      else if (action == MENU_ACTION_RIGHT)
         find_next_resampler_driver();
   }
   else if (setting->flags & SD_FLAG_IS_DRIVER)
   {
      const char *label    = setting->name;
      char *driver         = (char*)setting->value.string;
      size_t sizeof_driver = setting->size;

      switch (action)
      {
         case MENU_ACTION_LEFT:
            find_prev_driver(label, driver, sizeof_driver);
            break;
         case MENU_ACTION_RIGHT:
            find_next_driver(label, driver, sizeof_driver);
            break;
      }
   }
}
