/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "menu_input_dialog.h"

#include "../menu_driver.h"
#include "../../input/input_driver.h"

#ifdef HAVE_LIBNX
#include <switch.h>
#include "../../switch_performance_profiles.h"
#include "menu_osk.h"
#endif

static const char **menu_input_dialog_keyboard_buffer      = {NULL};
static bool menu_input_dialog_keyboard_display             = false;
static unsigned menu_input_dialog_keyboard_type            = 0;
static unsigned menu_input_dialog_keyboard_idx             = 0;
static char menu_input_dialog_keyboard_label_setting[256]  = {0};
static char menu_input_dialog_keyboard_label[256]          = {0};

static void menu_input_search_cb(void *userdata, const char *str)
{
   size_t idx = 0;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   if (!selection_buf)
      return;

   if (str && *str && file_list_search(selection_buf, str, &idx))
   {
      menu_navigation_set_selection(idx);
      menu_driver_navigation_set(true);
   }

   menu_input_dialog_end();
}

const char *menu_input_dialog_get_label_buffer(void)
{
   return menu_input_dialog_keyboard_label;
}

const char *menu_input_dialog_get_label_setting_buffer(void)
{
   return menu_input_dialog_keyboard_label_setting;
}

void menu_input_dialog_end(void)
{
   menu_input_dialog_keyboard_type             = 0;
   menu_input_dialog_keyboard_idx              = 0;
   menu_input_dialog_keyboard_display          = false;
   menu_input_dialog_keyboard_label[0]         = '\0';
   menu_input_dialog_keyboard_label_setting[0] = '\0';

   /* Avoid triggering tates on pressing return. */
   input_driver_set_flushing_input();
}

const char *menu_input_dialog_get_buffer(void)
{
   if (!(*menu_input_dialog_keyboard_buffer))
      return "";
   return *menu_input_dialog_keyboard_buffer;
}

unsigned menu_input_dialog_get_kb_type(void)
{
   return menu_input_dialog_keyboard_type;
}

unsigned menu_input_dialog_get_kb_idx(void)
{
   return menu_input_dialog_keyboard_idx;
}

#ifdef HAVE_LIBNX
#define LIBNX_SWKBD_LIMIT 500 /* enforced by HOS */
extern u32 __nx_applet_type;
extern void libnx_apply_overclock();
#endif

bool menu_input_dialog_get_display_kb(void)
{
#ifdef HAVE_LIBNX
   /* swkbd only works on "real" titles */
   if (__nx_applet_type != AppletType_Application && __nx_applet_type != AppletType_SystemApplication)
      return menu_input_dialog_keyboard_display;

   if (!menu_input_dialog_keyboard_display)
      return false;

   SwkbdConfig kbd;

   pcvSetClockRate(PcvModule_Cpu, SWITCH_CPU_SPEEDS_VALUES[SWITCH_DEFAULT_CPU_PROFILE]);

   Result rc = swkbdCreate(&kbd, 0);

   if (R_SUCCEEDED(rc))
   {
      char buf[LIBNX_SWKBD_LIMIT] = {'\0'};
      swkbdConfigMakePresetDefault(&kbd);

      swkbdConfigSetGuideText(&kbd, menu_input_dialog_keyboard_label);

      rc = swkbdShow(&kbd, buf, sizeof(buf));

      swkbdClose(&kbd);

      /* RetroArch uses key-by-key input
         so we need to simulate it */
      for (int i = 0; i < LIBNX_SWKBD_LIMIT; i++)
      {
         /* In case a previous "Enter" press closed the keyboard */
         if (!menu_input_dialog_keyboard_display)
            break;

         if (buf[i] == '\n' || buf[i] == '\0')
            input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);
         else
         {
            /* input_keyboard_line_append expects a null-terminated
               string, so just make one (yes, the touch keyboard is
               a list of "null-terminated characters") */
            char oldchar = buf[i+1];
            buf[i+1] = '\0';
            input_keyboard_line_append(&buf[i]);
            buf[i+1] = oldchar;
         }
      }

      /* fail-safe */
      if (menu_input_dialog_keyboard_display)
         input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);

      libnx_apply_overclock();
      return false;
   }
   else
   {
      libnx_apply_overclock();
      return menu_input_dialog_keyboard_display;
   }
#endif
   return menu_input_dialog_keyboard_display;
}

void menu_input_dialog_display_kb(void)
{
   menu_input_dialog_keyboard_display = true;
}

void menu_input_dialog_hide_kb(void)
{
   menu_input_dialog_keyboard_display = false;
}

bool menu_input_dialog_start_search(void)
{
   menu_handle_t      *menu = NULL;

   if (!menu_driver_ctl(
            RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return false;

   menu_input_dialog_display_kb();
   strlcpy(menu_input_dialog_keyboard_label, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SEARCH),
         sizeof(menu_input_dialog_keyboard_label));

   input_keyboard_ctl(RARCH_INPUT_KEYBOARD_CTL_LINE_FREE, NULL);

   menu_input_dialog_keyboard_buffer   =
      input_keyboard_start_line(menu, menu_input_search_cb);

   return true;
}

bool menu_input_dialog_start(menu_input_ctx_line_t *line)
{
   menu_handle_t    *menu      = NULL;
   if (!line)
      return false;
   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return false;

   menu_input_dialog_display_kb();

   /* Only copy over the menu label and setting if they exist. */
   if (line->label)
      strlcpy(menu_input_dialog_keyboard_label, line->label,
            sizeof(menu_input_dialog_keyboard_label));
   if (line->label_setting)
      strlcpy(menu_input_dialog_keyboard_label_setting,
            line->label_setting, sizeof(menu_input_dialog_keyboard_label_setting));

   menu_input_dialog_keyboard_type   = line->type;
   menu_input_dialog_keyboard_idx    = line->idx;

   input_keyboard_ctl(RARCH_INPUT_KEYBOARD_CTL_LINE_FREE, NULL);

   menu_input_dialog_keyboard_buffer =
      input_keyboard_start_line(menu, line->cb);

   return true;
}
