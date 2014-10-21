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

#include "config_file_userdata.h"
#include <string/string_list.h>

#define get_array_setup() \
   struct config_file_userdata *dsp = (struct config_file_userdata*)userdata; \
 \
   char key[2][256]; \
   snprintf(key[0], sizeof(key[0]), "%s_%s", dsp->prefix[0], key_str); \
   snprintf(key[1], sizeof(key[1]), "%s_%s", dsp->prefix[1], key_str); \
 \
   char *str = NULL; \
   bool got = config_get_string(dsp->conf, key[0], &str); \
   got = got || config_get_string(dsp->conf, key[1], &str);

#define get_array_body(T) \
   if (got) \
   { \
      unsigned i; \
      struct string_list *list = string_split(str, " "); \
      *values = (T*)calloc(list->size, sizeof(T)); \
      for (i = 0; i < list->size; i++) \
         (*values)[i] = (T)strtod(list->elems[i].data, NULL); \
      *out_num_values = list->size; \
      string_list_free(list); \
      return true; \
   } \
   else \
   { \
      *values = (T*)calloc(num_default_values, sizeof(T)); \
      memcpy(*values, default_values, sizeof(T) * num_default_values); \
      *out_num_values = num_default_values; \
      return false; \
   }

int config_userdata_get_float(void *userdata, const char *key_str,
      float *value, float default_value)
{
   struct config_file_userdata *dsp = (struct config_file_userdata*)userdata;

   char key[2][256];
   snprintf(key[0], sizeof(key[0]), "%s_%s", dsp->prefix[0], key_str);
   snprintf(key[1], sizeof(key[1]), "%s_%s", dsp->prefix[1], key_str);

   bool got = config_get_float(dsp->conf, key[0], value);
   got = got || config_get_float(dsp->conf, key[1], value);

   if (!got)
      *value = default_value;
   return got;
}

int config_userdata_get_int(void *userdata, const char *key_str,
      int *value, int default_value)
{
   struct config_file_userdata *dsp = (struct config_file_userdata*)userdata;

   char key[2][256];
   snprintf(key[0], sizeof(key[0]), "%s_%s", dsp->prefix[0], key_str);
   snprintf(key[1], sizeof(key[1]), "%s_%s", dsp->prefix[1], key_str);

   bool got = config_get_int(dsp->conf, key[0], value);
   got = got || config_get_int(dsp->conf, key[1], value);

   if (!got)
      *value = default_value;
   return got;
}

int config_userdata_get_float_array(void *userdata, const char *key_str,
      float **values, unsigned *out_num_values,
      const float *default_values, unsigned num_default_values)
{
   get_array_setup()
   get_array_body(float)
}

int config_userdata_get_int_array(void *userdata, const char *key_str,
      int **values, unsigned *out_num_values,
      const int *default_values, unsigned num_default_values)
{
   get_array_setup()
   get_array_body(int)
}

int config_userdata_get_string(void *userdata, const char *key_str,
      char **output, const char *default_output)
{
   get_array_setup()

   if (got)
   {
      *output = str;
      return true; 
   }

   *output = strdup(default_output);
   return false;
}

void config_userdata_free(void *ptr)
{
   if (ptr)
      free(ptr);
}
