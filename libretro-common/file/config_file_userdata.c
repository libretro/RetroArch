/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (config_file_userdata.c).
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

#include <stdlib.h>
#include <file/file_path.h>

#include <file/config_file_userdata.h>

int config_userdata_get_float(void *userdata, const char *key_str,
      float *value, float default_value)
{
   char key[256];
   struct config_file_userdata *usr = (struct config_file_userdata*)userdata;
   *value = default_value;
   fill_pathname_join_delim(key, usr->prefix[0], key_str, '_', sizeof(key));
   if (config_get_float(usr->conf, key, value))
      return 1;
   fill_pathname_join_delim(key, usr->prefix[1], key_str, '_', sizeof(key));
   if (config_get_float(usr->conf, key, value))
      return 1;
   return 0;
}

int config_userdata_get_int(void *userdata, const char *key_str,
      int *value, int default_value)
{
   char key[256];
   struct config_file_userdata *usr = (struct config_file_userdata*)userdata;
   *value = default_value;
   fill_pathname_join_delim(key, usr->prefix[0], key_str, '_', sizeof(key));
   if (config_get_int(usr->conf, key, value))
      return 1;
   fill_pathname_join_delim(key, usr->prefix[1], key_str, '_', sizeof(key));
   if (config_get_int(usr->conf, key, value))
      return 1;
   return 0;
}

int config_userdata_get_hex(void *userdata, const char *key_str,
      unsigned *value, unsigned default_value)
{
   char key[256];
   struct config_file_userdata *usr = (struct config_file_userdata*)userdata;
   *value = default_value;
   fill_pathname_join_delim(key, usr->prefix[0], key_str, '_', sizeof(key));
   if (config_get_hex(usr->conf, key, value))
      return 1;
   fill_pathname_join_delim(key, usr->prefix[1], key_str, '_', sizeof(key));
   if (config_get_hex(usr->conf, key, value))
      return 1;
   return 0;
}

int config_userdata_get_float_array(void *userdata, const char *key_str,
      float **values, unsigned *out_num_values,
      const float *default_values, unsigned num_default_values)
{
   char key[2][256];
   char *str = NULL;
   struct config_file_userdata *usr = (struct config_file_userdata*)userdata;
   fill_pathname_join_delim(key[0], usr->prefix[0], key_str, '_', sizeof(key[0]));
   fill_pathname_join_delim(key[1], usr->prefix[1], key_str, '_', sizeof(key[1]));
   if (     config_get_string(usr->conf, key[0], &str)
         || config_get_string(usr->conf, key[1], &str))
   {
      unsigned count = 0;
      const char *p = str;
      char *tok, *end;
      float *arr;

      /* Count tokens */
      while (*p)
      {
         while (*p == ' ')
            p++;
         if (!*p)
            break;
         count++;
         while (*p && *p != ' ')
            p++;
      }

      if (count == 0)
      {
         free(str);
         goto use_defaults;
      }

      arr = (float*)calloc(count, sizeof(float));
      if (!arr)
      {
         free(str);
         *values         = NULL;
         *out_num_values = 0;
         return 0;
      }

      /* Parse tokens */
      count = 0;
      tok   = str;
      while (*tok)
      {
         while (*tok == ' ')
            tok++;
         if (!*tok)
            break;
         arr[count++] = (float)strtod(tok, &end);
         tok = end;
      }

      *values         = arr;
      *out_num_values = count;
      free(str);
      return 1;
   }

use_defaults:
   if (num_default_values > 0 && default_values)
   {
      float *arr = (float*)calloc(num_default_values, sizeof(float));
      if (!arr)
      {
         *values         = NULL;
         *out_num_values = 0;
         return 0;
      }
      memcpy(arr, default_values, sizeof(float) * num_default_values);
      *values         = arr;
      *out_num_values = num_default_values;
   }
   else
   {
      *values         = NULL;
      *out_num_values = 0;
   }
   return 0;
}

int config_userdata_get_int_array(void *userdata, const char *key_str,
      int **values, unsigned *out_num_values,
      const int *default_values, unsigned num_default_values)
{
   char key[2][256];
   char *str = NULL;
   struct config_file_userdata *usr = (struct config_file_userdata*)userdata;
   fill_pathname_join_delim(key[0], usr->prefix[0], key_str, '_', sizeof(key[0]));
   fill_pathname_join_delim(key[1], usr->prefix[1], key_str, '_', sizeof(key[1]));
   if (     config_get_string(usr->conf, key[0], &str)
         || config_get_string(usr->conf, key[1], &str))
   {
      unsigned i, count = 0;
      const char *p = str;

      /* First pass: count tokens */
      while (*p)
      {
         while (*p == ' ')
            p++;
         if (!*p)
            break;
         count++;
         while (*p && *p != ' ')
            p++;
      }

      if (count == 0)
      {
         free(str);
         *values         = NULL;
         *out_num_values = 0;
         return 1;
      }

      *values = (int*)calloc(count, sizeof(int));
      if (!*values)
      {
         free(str);
         *out_num_values = 0;
         return 1;
      }

      /* Second pass: parse integers */
      p = str;
      i = 0;
      while (*p && i < count)
      {
         char *end;
         while (*p == ' ')
            p++;
         if (!*p)
            break;
         (*values)[i++] = (int)strtol(p, &end, 0);
         p = end;
      }

      *out_num_values = count;
      free(str);
      return 1;
   }

   *values = (int*)calloc(num_default_values, sizeof(int));
   if (*values)
      memcpy(*values, default_values, sizeof(int) * num_default_values);
   *out_num_values = *values ? num_default_values : 0;
   return 0;
}

int config_userdata_get_string(void *userdata, const char *key_str,
      char **output, const char *default_output)
{
   char key[2][256];
   char *str = NULL;
   struct config_file_userdata *usr = (struct config_file_userdata*)userdata;
   fill_pathname_join_delim(key[0], usr->prefix[0], key_str, '_', sizeof(key[0]));
   fill_pathname_join_delim(key[1], usr->prefix[1], key_str, '_', sizeof(key[1]));
   if (     config_get_string(usr->conf, key[0], &str)
         || config_get_string(usr->conf, key[1], &str))
   {
      *output = str;
      return 1;
   }
   *output = strdup(default_output);
   return 0;
}

void config_userdata_free(void *ptr)
{
   if (ptr)
      free(ptr);
}
