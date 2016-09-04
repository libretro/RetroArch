/*  RetroArch - A frontend for libretro.
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

#include <compat/strl.h>
#include <features/features_cpu.h>

#include "menu_driver.h"
#include "menu_popup.h"

#include "../configuration.h"
#ifdef HAVE_CHEEVOS
#include "../cheevos.h"
#endif
#include "../input/input_autodetect.h"
#include "../input/input_config.h"

static bool                menu_popup_pending_push   = false;
static unsigned            menu_popup_current_id     = 0;
static enum menu_help_type menu_popup_current_type   = MENU_HELP_NONE;

int menu_popup_iterate_help(char *s, size_t len, const char *label)
{
#ifdef HAVE_CHEEVOS
   cheevos_ctx_desc_t desc_info;
#endif
   bool do_exit              = false;
   settings_t *settings      = config_get_ptr();

   switch (menu_popup_current_type)
   {
      case MENU_HELP_WELCOME:
         {
            static int64_t timeout_end;
            int64_t timeout;
            static bool timer_begin = false;
            static bool timer_end   = false;
            int64_t current         = cpu_features_get_time_usec();

            if (!timer_begin)
            {
               timeout_end = cpu_features_get_time_usec() +
                  3 /* seconds */ * 1000000;
               timer_begin = true;
               timer_end   = false;
            }

            timeout = (timeout_end - current) / 1000000;

            menu_hash_get_help_enum(
                  MENU_ENUM_LABEL_WELCOME_TO_RETROARCH,
                  s, len);

            if (!timer_end && timeout <= 0)
            {
               timer_end   = true;
               timer_begin = false;
               timeout_end = 0;
               do_exit     = true;
            }
         }
         break;
      case MENU_HELP_CONTROLS:
         {
            unsigned i;
            char s2[PATH_MAX_LENGTH] = {0};
            const unsigned binds[] = {
               RETRO_DEVICE_ID_JOYPAD_UP,
               RETRO_DEVICE_ID_JOYPAD_DOWN,
               RETRO_DEVICE_ID_JOYPAD_A,
               RETRO_DEVICE_ID_JOYPAD_B,
               RETRO_DEVICE_ID_JOYPAD_SELECT,
               RETRO_DEVICE_ID_JOYPAD_START,
               RARCH_MENU_TOGGLE,
               RARCH_QUIT_KEY,
               RETRO_DEVICE_ID_JOYPAD_X,
               RETRO_DEVICE_ID_JOYPAD_Y,
            };
            char desc[ARRAY_SIZE(binds)][64] = {{0}};

            for (i = 0; i < ARRAY_SIZE(binds); i++)
            {
               const struct retro_keybind *keybind = 
                  (const struct retro_keybind*)
                  &settings->input.binds[0][binds[i]];
               const struct retro_keybind *auto_bind = 
                  (const struct retro_keybind*)
                  input_get_auto_bind(0, binds[i]);

               input_config_get_bind_string(desc[i],
                     keybind, auto_bind, sizeof(desc[i]));
            }

            menu_hash_get_help_enum(MENU_ENUM_LABEL_VALUE_MENU_ENUM_CONTROLS_PROLOG,
                  s2, sizeof(s2));

            snprintf(s, len,
                  "%s"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n"
                  "[%s]: "
                  "%-20s\n",

                  s2,

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_UP),
                  desc[0],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_SCROLL_DOWN),
                  desc[1],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM),
                  desc[2],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK),
                  desc[3],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_INFO),
                  desc[4],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_START),
                  desc[5],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_MENU),
                  desc[6],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_QUIT),
                  desc[7],

                  msg_hash_to_str(
                        MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_TOGGLE_KEYBOARD),
                  desc[8]

                  );
         }
         break;
         
#ifdef HAVE_CHEEVOS
      case MENU_HELP_CHEEVOS_DESCRIPTION:
         desc_info.idx = menu_popup_current_id;
         desc_info.s   = s;
         desc_info.len = len;
         cheevos_get_description(&desc_info);
         break;
#endif

      case MENU_HELP_WHAT_IS_A_CORE:
         menu_hash_get_help_enum(MENU_ENUM_LABEL_VALUE_WHAT_IS_A_CORE_DESC,
               s, len);
         break;
      case MENU_HELP_LOADING_CONTENT:
         menu_hash_get_help_enum(MENU_ENUM_LABEL_LOAD_CONTENT_LIST,
               s, len);
         break;
      case MENU_HELP_CHANGE_VIRTUAL_GAMEPAD:
         menu_hash_get_help_enum(
               MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC,
               s, len);
         break;
      case MENU_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
         menu_hash_get_help_enum(
               MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC,
               s, len);
         break;
      case MENU_HELP_SCANNING_CONTENT:
         menu_hash_get_help_enum(MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT_DESC,
               s, len);
         break;
      case MENU_HELP_EXTRACT:
         menu_hash_get_help_enum(MENU_ENUM_LABEL_VALUE_EXTRACTING_PLEASE_WAIT,
               s, len);

         if (settings->bundle_finished)
         {
            settings->bundle_finished = false;
            do_exit                   = true;
         }
         break;
      case MENU_HELP_NONE:
      default:
         break;
   }

   if (do_exit)
   {
      menu_popup_current_type = MENU_HELP_NONE;
      return 1;
   }

   return 0;
}

bool menu_popup_is_push_pending(void)
{
   return menu_popup_pending_push;
}

void menu_popup_unset_pending_push(void)
{
   menu_popup_pending_push = false;
}

void menu_popup_push_pending(bool push, enum menu_help_type type)
{
   menu_popup_pending_push = push;
   menu_popup_current_type = type;
}

void menu_popup_push(void)
{
   menu_displaylist_info_t info = {0};

   if (!menu_popup_is_push_pending())
      return;

   info.list = menu_entries_get_menu_stack_ptr(0);
   strlcpy(info.label,
         msg_hash_to_str(MENU_ENUM_LABEL_HELP),
         sizeof(info.label));
   info.enum_idx = MENU_ENUM_LABEL_HELP;

   menu_displaylist_ctl(DISPLAYLIST_HELP, &info);
}

void menu_popup_reset(void)
{
   menu_popup_pending_push = false;
   menu_popup_current_id   = 0;
   menu_popup_current_type = MENU_HELP_NONE;
}
