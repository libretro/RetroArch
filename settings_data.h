/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2013-2014 - Jason Fetters
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

#ifndef __RARCH_SETTINGS_DATA_H__
#define __RARCH_SETTINGS_DATA_H__

#include <stdint.h>
#include "conf/config_file.h"
#include "miscellaneous.h"
#include "settings_list.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BINDFOR(s) (*(&(s))->value.keybind)

void setting_data_reset_setting(rarch_setting_t* setting);
void setting_data_reset(rarch_setting_t* settings);

bool setting_data_load_config_path(rarch_setting_t* settings,
      const char* path);
bool setting_data_save_config(rarch_setting_t* settings,
      config_file_t* config);

rarch_setting_t* setting_data_find_setting(rarch_setting_t* settings,
      const char* name);

void setting_data_set_with_string_representation(
      rarch_setting_t* setting, const char* value);
void setting_data_get_string_representation(rarch_setting_t* setting,
      char* buf, size_t sizeof_buf);

/* List building helper functions. */
rarch_setting_t setting_data_group_setting(enum setting_type type,
      const char* name);

rarch_setting_t setting_data_subgroup_setting(enum setting_type type,
      const char* name, const char *parent_name);

rarch_setting_t setting_data_bool_setting(const char* name,
      const char* description, bool* target, bool default_value,
      const char *off, const char *on, const char * group,
      const char *subgroup, change_handler_t change_handler,
      change_handler_t read_handler);

rarch_setting_t setting_data_int_setting(const char* name,
      const char* description, int* target, int default_value,
      const char *group, const char *subgroup,
      change_handler_t change_handler, change_handler_t read_handler);

rarch_setting_t setting_data_uint_setting(const char* name,
      const char* description, unsigned int* target,
      unsigned int default_value, const char *group,
      const char *subgroup, change_handler_t change_handler,
      change_handler_t read_handler);

rarch_setting_t setting_data_float_setting(const char* name,
      const char* description, float* target, float default_value,
      const char *rounding, const char *group, const char *subgroup,
      change_handler_t change_handler, change_handler_t read_handler);

rarch_setting_t setting_data_string_setting(enum setting_type type,
      const char* name, const char* description, char* target,
      unsigned size, const char* default_value, const char *empty,
      const char *group, const char *subgroup,
      change_handler_t change_handler, change_handler_t read_handler);

rarch_setting_t setting_data_bind_setting(const char* name,
      const char* description, struct retro_keybind* target, uint32_t index,
      const struct retro_keybind* default_value, const char *group,
      const char *subgroup);

int setting_data_get_description(const char *label, char *msg,
      size_t msg_sizeof);

#ifdef HAVE_MENU
rarch_setting_t* setting_data_get_mainmenu(bool regenerate);

void setting_data_get_label(char *type_str,
      size_t type_str_size, unsigned *w, unsigned type, 
      const char *menu_label, const char *label, unsigned index);
#endif
rarch_setting_t* setting_data_get_list(unsigned mask, bool need_refresh);

#ifdef __cplusplus
}
#endif

#endif
