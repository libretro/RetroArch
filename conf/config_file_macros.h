/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#ifndef CONFIG_FILE_MACROS_H__
#define CONFIG_FILE_MACROS_H__

// Macros to ease config getting.
#include <stdint.h>

#define CONFIG_GET_BOOL_BASE(conf, base, var, key) do { \
   bool tmp = false; \
   if (config_get_bool(conf, key, &tmp)) \
      base.var = tmp; \
} while(0)

#define CONFIG_GET_INT_BASE(conf, base, var, key) do { \
   int tmp = 0; \
   if (config_get_int(conf, key, &tmp)) \
      base.var = tmp; \
} while(0)

#define CONFIG_GET_UINT64_BASE(conf, base, var, key) do { \
   uint64_t tmp = 0; \
   if (config_get_int(conf, key, &tmp)) \
      base.var = tmp; \
} while(0)

#define CONFIG_GET_FLOAT_BASE(conf, base, var, key) do { \
   float tmp = 0.0f; \
   if (config_get_float(conf, key, &tmp)) \
      base.var = tmp; \
} while(0)

#define CONFIG_GET_STRING_BASE(conf, base, var, key) \
   config_get_array(conf, key, base.var, sizeof(base.var))

#define CONFIG_GET_PATH_BASE(conf, base, var, key) \
   config_get_path(conf, key, base.var, sizeof(base.var))

#define CONFIG_GET_BOOL(var, key) CONFIG_GET_BOOL_BASE(conf, g_settings, var, key)
#define CONFIG_GET_INT(var, key) CONFIG_GET_INT_BASE(conf, g_settings, var, key)
#define CONFIG_GET_UINT64(var, key) CONFIG_GET_UINT64_BASE(conf, g_settings, var, key)
#define CONFIG_GET_FLOAT(var, key) CONFIG_GET_FLOAT_BASE(conf, g_settings, var, key)
#define CONFIG_GET_STRING(var, key) CONFIG_GET_STRING_BASE(conf, g_settings, var, key)
#define CONFIG_GET_PATH(var, key) CONFIG_GET_PATH_BASE(conf, g_settings, var, key)

#define CONFIG_GET_BOOL_EXTERN(var, key) CONFIG_GET_BOOL_BASE(conf, g_extern, var, key)
#define CONFIG_GET_INT_EXTERN(var, key) CONFIG_GET_INT_BASE(conf, g_extern, var, key)
#define CONFIG_GET_FLOAT_EXTERN(var, key) CONFIG_GET_FLOAT_BASE(conf, g_extern, var, key)
#define CONFIG_GET_STRING_EXTERN(var, key) CONFIG_GET_STRING_BASE(conf, g_extern, var, key)
#define CONFIG_GET_PATH_EXTERN(var, key) CONFIG_GET_PATH_BASE(conf, g_extern, var, key)

#endif

