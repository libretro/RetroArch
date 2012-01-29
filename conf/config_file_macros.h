/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _CONFIG_FILE_MACROS_H
#define _CONFIG_FILE_MACROS_H

/* Macros to ease config getting. */

#define CONFIG_GET_BOOL(var, key) if (config_get_bool(conf, key, &tmp_bool)) \
   g_settings.var = tmp_bool

#define CONFIG_GET_INT(var, key) if (config_get_int(conf, key, &tmp_int)) \
   g_settings.var = tmp_int

#define CONFIG_GET_DOUBLE(var, key) if (config_get_double(conf, key, &tmp_double)) \
   g_settings.var = tmp_double

#define CONFIG_GET_STRING(var, key) \
   config_get_array(conf, key, g_settings.var, sizeof(g_settings.var))

#ifdef SSNES_CONSOLE

#define CONFIG_GET_BOOL_CONSOLE(var, key) if (config_get_bool(conf, key, &tmp_bool)) \
   g_console.var = tmp_bool

#define CONFIG_GET_INT_CONSOLE(var, key) if (config_get_int(conf, key, &tmp_int)) \
   g_console.var = tmp_int

#define CONFIG_GET_DOUBLE_CONSOLE(var, key) if (config_get_double(conf, key, &tmp_double)) \
   g_console.var = tmp_double

#define CONFIG_GET_STRING_CONSOLE(var, key) \
   config_get_array(conf, key, g_console.var, sizeof(g_console.var))

#endif

#define CONFIG_GET_BOOL_EXTERN(var, key) if (config_get_bool(conf, key, &tmp_bool)) \
   g_extern.var = tmp_bool

#define CONFIG_GET_INT_EXTERN(var, key) if (config_get_int(conf, key, &tmp_int)) \
   g_extern.var = tmp_int

#define CONFIG_GET_DOUBLE_EXTERN(var, key) if (config_get_double(conf, key, &tmp_double)) \
   g_extern.var = tmp_double

#define CONFIG_GET_STRING_EXTERN(var, key) \
   config_get_array(conf, key, g_extern.var, sizeof(g_extern.var))

#endif
