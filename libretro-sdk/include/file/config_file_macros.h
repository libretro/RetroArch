/* Copyright  (C) 2010-2014 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (config_file_macros.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_CONFIG_FILE_MACROS_H__
#define __LIBRETRO_SDK_CONFIG_FILE_MACROS_H__

/* Macros to ease config getting. */
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
