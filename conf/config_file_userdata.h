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

#ifndef _CONFIG_FILE_USERDATA_H
#define _CONFIG_FILE_USERDATA_H

#include <string.h>
#include "config_file.h"

struct config_file_userdata
{
   config_file_t *conf;
   const char *prefix[2];
};

int config_userdata_get_float(void *userdata, const char *key_str,
      float *value, float default_value);

int config_userdata_get_int(void *userdata, const char *key_str,
      int *value, int default_value);

int config_userdata_get_float_array(void *userdata, const char *key_str,
      float **values, unsigned *out_num_values,
      const float *default_values, unsigned num_default_values);

int config_userdata_get_int_array(void *userdata, const char *key_str,
      int **values, unsigned *out_num_values,
      const int *default_values, unsigned num_default_values);

int config_userdata_get_string(void *userdata, const char *key_str,
      char **output, const char *default_output);

void config_userdata_free(void *ptr);

#endif
