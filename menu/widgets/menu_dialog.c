/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_CHEEVOS
#include "../../cheevos-new/cheevos.h"
#endif

#include "menu_dialog.h"

#include "../menu_driver.h"

#include "../../retroarch.h"
#include "../../configuration.h"

#include "../../tasks/tasks_internal.h"
#include "../../performance_counters.h"

static bool                  menu_dialog_pending_push   = false;
static bool                  menu_dialog_active         = false;
static unsigned              menu_dialog_current_id     = 0;
static enum menu_dialog_type menu_dialog_current_type   = MENU_DIALOG_NONE;
static enum msg_hash_enums   menu_dialog_current_msg    = MSG_UNKNOWN;

int menu_dialog_iterate(char *s, size_t len, const char *label)
{
#ifdef HAVE_CHEEVOS
   rcheevos_ctx_desc_t desc_info;
#endif
   bool do_exit              = false;

   switch (menu_dialog_current_type)
   {
      case MENU_DIALOG_WELCOME:
         {
            static rarch_timer_t timer;

            if (!rarch_timer_is_running(&timer))
               rarch_timer_begin(&timer, 3);

            rarch_timer_tick(&timer);

            menu_hash_get_help_enum(
                  MENU_ENUM_LABEL_WELCOME_TO_RETROARCH,
                  s, len);

            if (!timer.timer_end && rarch_timer_has_expired(&timer))
            {
               rarch_timer_end(&timer);
               do_exit     = true;
            }
         }
         break;
      case MENU_DIALOG_HELP_CONTROLS:
         {
            unsigned i;
            char s2[PATH_MAX_LENGTH];
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
            char desc[ARRAY_SIZE(binds)][64];

            for (i = 0; i < ARRAY_SIZE(binds); i++)
               desc[i][0] = '\0';

            for (i = 0; i < ARRAY_SIZE(binds); i++)
            {
               const struct retro_keybind *keybind = &input_config_binds[0][binds[i]];
               const struct retro_keybind *auto_bind =
                  (const struct retro_keybind*)
                  input_config_get_bind_auto(0, binds[i]);

               input_config_get_bind_string(desc[i],
                     keybind, auto_bind, sizeof(desc[i]));
            }

            s2[0] = '\0';

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
      case MENU_DIALOG_HELP_CHEEVOS_DESCRIPTION:
         desc_info.idx = menu_dialog_current_id;
         desc_info.s   = s;
         desc_info.len = len;
         rcheevos_get_description((rcheevos_ctx_desc_t*) &desc_info);
         break;
#endif

      case MENU_DIALOG_HELP_WHAT_IS_A_CORE:
         menu_hash_get_help_enum(MENU_ENUM_LABEL_VALUE_WHAT_IS_A_CORE_DESC,
               s, len);
         break;
      case MENU_DIALOG_HELP_LOADING_CONTENT:
         menu_hash_get_help_enum(MENU_ENUM_LABEL_LOAD_CONTENT_LIST,
               s, len);
         break;
      case MENU_DIALOG_HELP_CHANGE_VIRTUAL_GAMEPAD:
         menu_hash_get_help_enum(
               MENU_ENUM_LABEL_VALUE_HELP_CHANGE_VIRTUAL_GAMEPAD_DESC,
               s, len);
         break;
      case MENU_DIALOG_HELP_AUDIO_VIDEO_TROUBLESHOOTING:
         menu_hash_get_help_enum(
               MENU_ENUM_LABEL_VALUE_HELP_AUDIO_VIDEO_TROUBLESHOOTING_DESC,
               s, len);
         break;
      case MENU_DIALOG_HELP_SEND_DEBUG_INFO:
         menu_hash_get_help_enum(
               MENU_ENUM_LABEL_VALUE_HELP_SEND_DEBUG_INFO_DESC,
               s, len);
         break;
      case MENU_DIALOG_HELP_SCANNING_CONTENT:
         menu_hash_get_help_enum(MENU_ENUM_LABEL_VALUE_HELP_SCANNING_CONTENT_DESC,
               s, len);
         break;
      case MENU_DIALOG_HELP_EXTRACT:
         {
            settings_t *settings      = config_get_ptr();
            menu_hash_get_help_enum(MENU_ENUM_LABEL_VALUE_EXTRACTING_PLEASE_WAIT,
                  s, len);

            if (settings->bools.bundle_finished)
            {
               settings->bools.bundle_finished = false;
               do_exit                         = true;
            }
         }
         break;
      case MENU_DIALOG_QUIT_CONFIRM:
      case MENU_DIALOG_INFORMATION:
      case MENU_DIALOG_QUESTION:
      case MENU_DIALOG_WARNING:
      case MENU_DIALOG_ERROR:
         menu_hash_get_help_enum(menu_dialog_current_msg,
               s, len);
         break;
      case MENU_DIALOG_NONE:
      default:
         break;
   }

   if (do_exit)
   {
      menu_dialog_current_type = MENU_DIALOG_NONE;
      return 1;
   }

   return 0;
}

bool menu_dialog_is_push_pending(void)
{
   return menu_dialog_pending_push;
}

void menu_dialog_unset_pending_push(void)
{
   menu_dialog_pending_push = false;
}

void menu_dialog_push_pending(bool push, enum menu_dialog_type type)
{
   menu_dialog_pending_push = push;
   menu_dialog_current_type = type;
   menu_dialog_active = true;
}

void menu_dialog_push(void)
{
   menu_displaylist_info_t info;
   const char *label;

   if (!menu_dialog_is_push_pending())
      return;

   menu_displaylist_info_init(&info);

   info.list                 = menu_entries_get_menu_stack_ptr(0);
   info.enum_idx             = MENU_ENUM_LABEL_HELP;

   /* Set the label string, if it exists. */
   label                     = msg_hash_to_str(MENU_ENUM_LABEL_HELP);
   if (label)
      info.label             = strdup(label);

   menu_displaylist_ctl(DISPLAYLIST_HELP, &info);
}

void menu_dialog_set_current_id(unsigned id)
{
   menu_dialog_current_id   = id;
}

void menu_dialog_reset(void)
{
   menu_dialog_pending_push = false;
   menu_dialog_current_id   = 0;
   menu_dialog_current_type = MENU_DIALOG_NONE;
   menu_dialog_current_msg  = MSG_UNKNOWN;
}

void menu_dialog_show_message(
      enum menu_dialog_type type, enum msg_hash_enums msg)
{
   menu_dialog_current_msg = msg;

   menu_dialog_push_pending(true, type);
   menu_dialog_push();
}

bool menu_dialog_is_active(void)
{
   return menu_dialog_active;
}

void menu_dialog_set_active(bool on)
{
   menu_dialog_active = on;
}

enum menu_dialog_type menu_dialog_get_current_type(void)
{
   return menu_dialog_current_type;
}
