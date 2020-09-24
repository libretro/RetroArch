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

#include <file/file_path.h>
#include <lists/string_list.h>

#include <file/config_file_userdata.h>

int config_userdata_get_float(void *userdata, const char *key_str,
      float *value, float default_value)
{
   bool got;
   char key[2][256];
   struct config_file_userdata *usr = (struct config_file_userdata*)userdata;

   fill_pathname_join_delim(key[0], usr->prefix[0], key_str, '_', sizeof(key[0]));
   fill_pathname_join_delim(key[1], usr->prefix[1], key_str, '_', sizeof(key[1]));

   got = config_get_float  (usr->conf, key[0], value);
   got = got || config_get_float(usr->conf, key[1], value);

   if (!got)
      *value = default_value;
   return got;
}

int config_userdata_get_int(void *userdata, const char *key_str,
      int *value, int default_value)
{
   bool got;
   char key[2][256];
   struct config_file_userdata *usr = (struct config_file_userdata*)userdata;

   fill_pathname_join_delim(key[0], usr->prefix[0], key_str, '_', sizeof(key[0]));
   fill_pathname_join_delim(key[1], usr->prefix[1], key_str, '_', sizeof(key[1]));

   got = config_get_int  (usr->conf, key[0], value);
   got = got || config_get_int(usr->conf, key[1], value);

   if (!got)
      *value = default_value;
   return got;
}

int config_userdata_get_hex(void *userdata, const char *key_str,
      unsigned *value, unsigned default_value)
{
   bool got;
   char key[2][256];
   struct config_file_userdata *usr = (struct config_file_userdata*)userdata;

   fill_pathname_join_delim(key[0], usr->prefix[0], key_str, '_', sizeof(key[0]));
   fill_pathname_join_delim(key[1], usr->prefix[1], key_str, '_', sizeof(key[1]));

   got = config_get_hex(usr->conf, key[0], value);
   got = got || config_get_hex(usr->conf, key[1], value);

   if (!got)
      *value = default_value;
   return got;
}

int config_userdata_get_float_array(void *userdata, const char *key_str,
      float **values, unsigned *out_num_values,
      const float *default_values, unsigned num_default_values)
{
   char key[2][256];
   struct config_file_userdata *usr = (struct config_file_userdata*)userdata;
   char *str = NULL;

   fill_pathname_join_delim(key[0], usr->prefix[0], key_str, '_', sizeof(key[0]));
   fill_pathname_join_delim(key[1], usr->prefix[1], key_str, '_', sizeof(key[1]));

   if (  config_get_string(usr->conf, key[0], &str) ||
         config_get_string(usr->conf, key[1], &str))
   {
      unsigned i;
      struct string_list list = {0};
      string_list_initialize(&list);
      string_split_noalloc(&list, str, " ");
      *values = (float*)calloc(list.size, sizeof(float));
      for (i = 0; i < list.size; i++)
         (*values)[i] = (float)strtod(list.elems[i].data, NULL);
      *out_num_values = (unsigned)list.size;
      string_list_deinitialize(&list);
      free(str);
      return true;
   }

   *values = (float*)calloc(num_default_values, sizeof(float));
   memcpy(*values, default_values, sizeof(float) * num_default_values);
   *out_num_values = num_default_values;
   return false;
}

int config_userdata_get_int_array(void *userdata, const char *key_str,
      int **values, unsigned *out_num_values,
      const int *default_values, unsigned num_default_values)
{
   char key[2][256];
   struct config_file_userdata *usr = (struct config_file_userdata*)userdata;
   char *str = NULL;
   fill_pathname_join_delim(key[0], usr->prefix[0], key_str, '_', sizeof(key[0]));
   fill_pathname_join_delim(key[1], usr->prefix[1], key_str, '_', sizeof(key[1]));

   if (  config_get_string(usr->conf, key[0], &str) ||
         config_get_string(usr->conf, key[1], &str))
   {
      unsigned i;
      struct string_list list = {0};
      string_list_initialize(&list);
      string_split_noalloc(&list, str, " ");
      *values = (int*)calloc(list.size, sizeof(int));
      for (i = 0; i < list.size; i++)
         (*values)[i] = (int)strtod(list.elems[i].data, NULL);
      *out_num_values = (unsigned)list.size;
      string_list_deinitialize(&list);
      free(str);
      return true;
   }

   *values = (int*)calloc(num_default_values, sizeof(int));
   memcpy(*values, default_values, sizeof(int) * num_default_values);
   *out_num_values = num_default_values;
   return false;
}

int config_userdata_get_string(void *userdata, const char *key_str,
      char **output, const char *default_output)
{
   char key[2][256];
   struct config_file_userdata *usr = (struct config_file_userdata*)userdata;
   char *str = NULL;
   fill_pathname_join_delim(key[0], usr->prefix[0], key_str, '_', sizeof(key[0]));
   fill_pathname_join_delim(key[1], usr->prefix[1], key_str, '_', sizeof(key[1]));

   if (  config_get_string(usr->conf, key[0], &str) ||
         config_get_string(usr->conf, key[1], &str))
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
