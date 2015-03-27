/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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
#include <file/dir_list.h>
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
      autoconfig_params_t *params)
{
   char ident[PATH_MAX_LENGTH], ident_idx[PATH_MAX_LENGTH];
   char input_driver[PATH_MAX_LENGTH];
   int input_vid = 0, input_pid = 0;

   if (!conf)
      return false;

   *ident = *input_driver = '\0';

   config_get_array(conf, "input_device", ident, sizeof(ident));
   config_get_array(conf, "input_driver", input_driver, sizeof(input_driver));
   config_get_int  (conf, "input_vendor_id", &input_vid);
   config_get_int  (conf, "input_product_id", &input_pid);

   snprintf(ident_idx,
         sizeof(ident_idx), "%s_p%u", ident, params->idx);

   /* If Vendor ID and Product ID matches, we've found our
    * entry. */
   if (     (params->vid == input_vid)
         && (params->pid == input_pid)
         && params->vid != 0
         && params->pid != 0
         && input_vid   != 0
         && input_pid   != 0)
      return true;

   /* Check for name match. */
   if (!strcmp(ident_idx, params->name))
      return true;
   if (!strcmp(ident, params->name) 
      && !strcmp(params->driver, input_driver))
      return true;

   return false;
}

static void input_autoconfigure_joypad_add(
      config_file_t *conf,
      autoconfig_params_t *params)
{
   char msg[PATH_MAX_LENGTH];
   settings_t *settings = config_get_ptr();

   /* This will be the case if input driver is reinitialized.
    * No reason to spam autoconfigure messages every time. */
   bool block_osd_spam = settings && 
      settings->input.autoconfigured[params->idx] && params->name;

   if (!settings)
      return;

   settings->input.autoconfigured[params->idx] = true;
   input_autoconfigure_joypad_conf(conf,
         settings->input.autoconf_binds[params->idx]);

   snprintf(msg, sizeof(msg), "Joypad port #%u (%s) configured.",
         params->idx, params->name);

   if (!block_osd_spam)
      rarch_main_msg_queue_push(msg, 0, 60, false);
   RARCH_LOG("%s\n", msg);
}

static bool input_autoconfigure_joypad_from_conf(
      config_file_t *conf, autoconfig_params_t *params)
{
   bool ret = false;

   if (!conf)
      return false;
   
   ret = input_try_autoconfigure_joypad_from_conf(conf,
         params);

   if (ret)
      input_autoconfigure_joypad_add(conf, params);

   config_file_free(conf);

   return ret;
}

void input_config_autoconfigure_joypad(autoconfig_params_t *params)
{
   size_t i;
   bool ret = false;
   struct string_list *list = NULL;
   settings_t *settings = config_get_ptr();

   if (!settings || !settings->input.autodetect_enable)
      return;

   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      settings->input.autoconf_binds[params->idx][i].joykey = NO_BTN;
      settings->input.autoconf_binds[params->idx][i].joyaxis = AXIS_NONE;
      settings->input.autoconf_binds[params->idx][i].joykey_label[0] = '\0';
      settings->input.autoconf_binds[params->idx][i].joyaxis_label[0] = '\0';
   }
   settings->input.autoconfigured[params->idx] = false;

   if (!params->name)
      return;

#if defined(HAVE_BUILTIN_AUTOCONFIG)
   /* First internal */
   for (i = 0; input_builtin_autoconfs[i]; i++)
   {
      config_file_t *conf = config_file_new_from_string(
            input_builtin_autoconfs[i]);

      if ((ret = input_autoconfigure_joypad_from_conf(conf, params)))
         break;
   }
#endif

   if (ret || !*settings->input.autoconfig_dir)
      return;

   /* Load from both cfg files and internal */
   list = dir_list_new(settings->input.autoconfig_dir, "cfg", false);

   if (!list)
      return;

   for (i = 0; i < list->size; i++)
   {
      config_file_t *conf = config_file_new(list->elems[i].data);

      if ((ret = input_autoconfigure_joypad_from_conf(conf, params)))
         break;
   }

   string_list_free(list);
}

const struct retro_keybind *input_get_auto_bind(unsigned port, unsigned id)
{
   settings_t *settings = config_get_ptr();
   unsigned joy_idx     = 0;
   
   if (settings)
      joy_idx = settings->input.joypad_map[port];

   if (joy_idx < MAX_USERS)
      return &settings->input.autoconf_binds[joy_idx][id];
   return NULL;
}
