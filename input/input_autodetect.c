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

#include "input_common.h"
#include "input_autodetect.h"
#include "../dir_list.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "../general.h"

static void input_autoconfigure_joypad_conf(config_file_t *conf,
      struct retro_keybind *binds)
{
   unsigned i;
   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      input_config_parse_joy_button(conf, "input",
            input_config_bind_map[i].base, &binds[i]);
      input_config_parse_joy_axis(conf, "input",
            input_config_bind_map[i].base, &binds[i]);
   }
}

static bool input_try_autoconfigure_joypad_from_conf(config_file_t *conf,
      unsigned idx, const char *name, const char *drv,
      int32_t vid, int32_t pid, bool block_osd_spam)
{
   if (!conf)
      return false;

   char ident[PATH_MAX], ident_idx[PATH_MAX], input_driver[PATH_MAX];
   int input_vid = 0, input_pid = 0;
   bool cond_found_idx, cond_found_general,
        cond_found_vid = false, cond_found_pid = false;

   *ident = *input_driver = '\0';

   config_get_array(conf, "input_device", ident, sizeof(ident));
   config_get_array(conf, "input_driver", input_driver, sizeof(input_driver));
   config_get_int(conf, "input_vendor_id", &input_vid);
   config_get_int(conf, "input_product_id", &input_pid);

   snprintf(ident_idx, sizeof(ident_idx), "%s_p%u", ident, idx);

   //RARCH_LOG("ident_idx: %s\n", ident_idx);

   cond_found_idx     = !strcmp(ident_idx, name);
   cond_found_general = !strcmp(ident, name) && !strcmp(drv, input_driver);
   if ((vid != 0) && (input_vid != 0))
      cond_found_vid     = (vid == input_vid);
   if ((pid != 0) && (input_pid != 0))
      cond_found_pid     = (pid == input_pid);

   /* If Vendor ID and Product ID matches, we've found our
    * entry. */
   if (cond_found_vid && cond_found_pid)
      goto found;

   /* Check for name match. */
   if (cond_found_idx)
      goto found;
   else if (cond_found_general)
      goto found;

   return false;

found:
   g_settings.input.autoconfigured[idx] = true;
   input_autoconfigure_joypad_conf(conf, g_settings.input.autoconf_binds[idx]);

   char msg[512];
   snprintf(msg, sizeof(msg), "Joypad port #%u (%s) configured.",
         idx, name);

   if (!block_osd_spam)
      msg_queue_push(g_extern.msg_queue, msg, 0, 60);
   RARCH_LOG("%s\n", msg);

   return true;
}

void input_config_autoconfigure_joypad(unsigned idx,
      const char *name, int32_t vid, int32_t pid,
      const char *drv)
{
   size_t i;

   if (!g_settings.input.autodetect_enable)
      return;

   /* This will be the case if input driver is reinit.
    * No reason to spam autoconfigure messages
    * every time (fine in log). */
   bool block_osd_spam = g_settings.input.autoconfigured[idx] && name;

   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      g_settings.input.autoconf_binds[idx][i].joykey = NO_BTN;
      g_settings.input.autoconf_binds[idx][i].joyaxis = AXIS_NONE;
   }
   g_settings.input.autoconfigured[idx] = false;

   if (!name)
      return;

   /* false = load from both cfg files and internal */
   bool internal_only = !*g_settings.input.autoconfig_dir;

#if defined(HAVE_BUILTIN_AUTOCONFIG) && (!defined(_WIN32) || defined(HAVE_WINXINPUT))
   /* First internal */
   for (i = 0; input_builtin_autoconfs[i]; i++)
   {
      config_file_t *conf = (config_file_t*)
         config_file_new_from_string(input_builtin_autoconfs[i]);
      bool success = input_try_autoconfigure_joypad_from_conf(conf,
            idx, name, drv, vid, pid, block_osd_spam);
      config_file_free(conf);
      if (success)
         break;
   }
#endif

   /* Now try files */
   if (!internal_only)
   {
      struct string_list *list = dir_list_new(
            g_settings.input.autoconfig_dir, "cfg", false);
      if (!list)
         return;

      for (i = 0; i < list->size; i++)
      {
         config_file_t *conf = config_file_new(list->elems[i].data);
         if (!conf)
            continue;
         bool success = input_try_autoconfigure_joypad_from_conf(conf,
               idx, name, drv, vid, pid, block_osd_spam);
         config_file_free(conf);
         if (success)
            break;
      }

      string_list_free(list);
   }
}

const struct retro_keybind *input_get_auto_bind(unsigned port, unsigned id)
{
   unsigned int joy_idx = g_settings.input.joypad_map[port];
   return &g_settings.input.autoconf_binds[joy_idx][id];
}
