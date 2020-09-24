/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (config_file_userdata.h).
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

#ifndef _LIBRETRO_SDK_CONFIG_FILE_USERDATA_H
#define _LIBRETRO_SDK_CONFIG_FILE_USERDATA_H

#include <string.h>

#include <file/config_file.h>

#include <retro_common_api.h>

RETRO_BEGIN_DECLS

struct config_file_userdata
{
   config_file_t *conf;
   const char *prefix[2];
};

int config_userdata_get_float(void *userdata, const char *key_str,
      float *value, float default_value);

int config_userdata_get_int(void *userdata, const char *key_str,
      int *value, int default_value);

int config_userdata_get_hex(void *userdata, const char *key_str,
      unsigned *value, unsigned default_value);

int config_userdata_get_float_array(void *userdata, const char *key_str,
      float **values, unsigned *out_num_values,
      const float *default_values, unsigned num_default_values);

int config_userdata_get_int_array(void *userdata, const char *key_str,
      int **values, unsigned *out_num_values,
      const int *default_values, unsigned num_default_values);

int config_userdata_get_string(void *userdata, const char *key_str,
      char **output, const char *default_output);

void config_userdata_free(void *ptr);

RETRO_END_DECLS

#endif
