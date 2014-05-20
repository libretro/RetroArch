/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "../general.h"

#include "dsp_filter.h"
#include "../dynamic.h"
#include "../conf/config_file.h"
#include "filters/dspfilter.h"
#include "../file_path.h"
#include "../file_ext.h"
#include "../compat/posix_string.h"

#include <stdlib.h>

struct rarch_dsp_plug
{
#ifdef HAVE_DYLIB
   dylib_t lib;
#endif
   const struct dspfilter_implementation *impl;
};

struct rarch_dsp_instance
{
   const struct dspfilter_implementation *impl;
   void *impl_data;
};

struct rarch_dsp_filter
{
   config_file_t *conf;

   struct rarch_dsp_plug *plugs;
   unsigned num_plugs;

   struct rarch_dsp_instance *instances;
   unsigned num_instances;
};

const struct dspfilter_implementation *find_implementation(rarch_dsp_filter_t *dsp, const char *ident)
{
   unsigned i;
   for (i = 0; i < dsp->num_plugs; i++)
   {
      if (!strcmp(dsp->plugs[i].impl->short_ident, ident))
         return dsp->plugs[i].impl;
   }

   return NULL;
}

struct dsp_userdata
{
   config_file_t *conf;
   const char *prefix[2];
};

static int get_float(void *userdata, const char *key_str, float *value, float default_value)
{
   struct dsp_userdata *dsp = (struct dsp_userdata*)userdata;

   char key[2][256];
   snprintf(key[0], sizeof(key[0]), "%s_%s", dsp->prefix[0], key_str);
   snprintf(key[1], sizeof(key[1]), "%s_%s", dsp->prefix[1], key_str);

   bool got = config_get_float(dsp->conf, key[0], value);
   got = got || config_get_float(dsp->conf, key[1], value);

   if (!got)
      *value = default_value;
   return got;
}

static int get_int(void *userdata, const char *key_str, int *value, int default_value)
{
   struct dsp_userdata *dsp = (struct dsp_userdata*)userdata;

   char key[2][256];
   snprintf(key[0], sizeof(key[0]), "%s_%s", dsp->prefix[0], key_str);
   snprintf(key[1], sizeof(key[1]), "%s_%s", dsp->prefix[1], key_str);

   bool got = config_get_int(dsp->conf, key[0], value);
   got = got || config_get_int(dsp->conf, key[1], value);

   if (!got)
      *value = default_value;
   return got;
}

// Yup, still C >__<
#define get_array_setup() \
   struct dsp_userdata *dsp = (struct dsp_userdata*)userdata; \
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

static int get_float_array(void *userdata, const char *key_str,
      float **values, unsigned *out_num_values,
      const float *default_values, unsigned num_default_values)
{
   get_array_setup()
   get_array_body(float)
}

static int get_int_array(void *userdata, const char *key_str,
      int **values, unsigned *out_num_values,
      const int *default_values, unsigned num_default_values)
{
   get_array_setup()
   get_array_body(int)
}

static int get_string(void *userdata, const char *key_str,
      char **output, const char *default_output)
{
   get_array_setup()

   if (got)
   {
      *output = str;
      return true; 
   }
   else
   {
      *output = strdup(default_output);
      return false;
   }
}

static void dspfilter_free(void *ptr)
{
   free(ptr);
}

static const struct dspfilter_config dspfilter_config = {
   get_float,
   get_int,
   get_float_array,
   get_int_array,
   get_string,
   dspfilter_free,
};

static bool create_filter_graph(rarch_dsp_filter_t *dsp, float sample_rate)
{
   unsigned i;

   unsigned filters = 0;
   if (!config_get_uint(dsp->conf, "filters", &filters))
      return false;

   dsp->instances = (struct rarch_dsp_instance*)calloc(filters, sizeof(*dsp->instances));
   if (!dsp->instances)
      return false;

   dsp->num_instances = filters;

   for (i = 0; i < filters; i++)
   {
      char key[64];
      snprintf(key, sizeof(key), "filter%u", i);

      char name[64];
      if (!config_get_array(dsp->conf, key, name, sizeof(name)))
         return false;

      dsp->instances[i].impl = find_implementation(dsp, name);
      if (!dsp->instances[i].impl)
         return false;

      struct dsp_userdata userdata;
      userdata.conf = dsp->conf;
      userdata.prefix[0] = key; // Index-specific configs take priority over ident-specific.
      userdata.prefix[1] = dsp->instances[i].impl->short_ident;

      struct dspfilter_info info = { sample_rate };
      dsp->instances[i].impl_data = dsp->instances[i].impl->init(&info, &dspfilter_config, &userdata);
      if (!dsp->instances[i].impl_data)
         return false;
   }

   return true;
}

#ifdef HAVE_DYLIB
static bool append_plugs(rarch_dsp_filter_t *dsp, struct string_list *list)
{
   unsigned i;
   dspfilter_simd_mask_t mask = rarch_get_cpu_features();

   for (i = 0; i < list->size; i++)
   {
      dylib_t lib = dylib_load(list->elems[i].data);
      if (!lib)
         continue;

      dspfilter_get_implementation_t cb = (dspfilter_get_implementation_t)dylib_proc(lib, "dspfilter_get_implementation");
      if (!cb)
      {
         dylib_close(lib);
         continue;
      }

      const struct dspfilter_implementation *impl = cb(mask);
      if (!impl)
      {
         dylib_close(lib);
         continue;
      }

      if (impl->api_version != DSPFILTER_API_VERSION)
      {
         dylib_close(lib);
         continue;
      }

      struct rarch_dsp_plug *new_plugs = (struct rarch_dsp_plug*)realloc(dsp->plugs, sizeof(*dsp->plugs) * (dsp->num_plugs + 1));
      if (!new_plugs)
      {
         dylib_close(lib);
         return false;
      }

      RARCH_LOG("[DSP]: Found plug: %s (%s).\n", impl->ident, impl->short_ident);
      
      dsp->plugs = new_plugs;
      dsp->plugs[dsp->num_plugs].lib = lib;
      dsp->plugs[dsp->num_plugs].impl = impl;
      dsp->num_plugs++;
   }

   return true;
}
#else
// Append from built-in filters.
#endif

rarch_dsp_filter_t *rarch_dsp_filter_new(const char *filter_config, float sample_rate)
{
   char basedir[PATH_MAX];
   struct string_list *plugs = NULL;

   rarch_dsp_filter_t *dsp = (rarch_dsp_filter_t*)calloc(1, sizeof(*dsp));
   if (!dsp)
      return NULL;

   dsp->conf = config_file_new(filter_config);
   if (!dsp->conf)
   {
      RARCH_ERR("[DSP]: Did not find config: %s\n", filter_config);
      goto error;
   }

#ifdef HAVE_DYLIB
   fill_pathname_basedir(basedir, filter_config, sizeof(basedir));

   plugs = dir_list_new(basedir, EXT_EXECUTABLES, false);
   if (!plugs)
      goto error;

   if (!append_plugs(dsp, plugs))
      goto error;

   string_list_free(plugs);
   plugs = NULL;
#else
   if (!append_plugs(dsp))
      goto error;
#endif

   if (!create_filter_graph(dsp, sample_rate))
      goto error;

   return dsp;

error:
   string_list_free(plugs);
   rarch_dsp_filter_free(dsp);
   return NULL;
}

void rarch_dsp_filter_free(rarch_dsp_filter_t *dsp)
{
   unsigned i;
   if (!dsp)
      return;

   for (i = 0; i < dsp->num_instances; i++)
   {
      if (dsp->instances[i].impl_data && dsp->instances[i].impl)
         dsp->instances[i].impl->free(dsp->instances[i].impl_data);
   }
   free(dsp->instances);

#ifdef HAVE_DYLIB
   for (i = 0; i < dsp->num_plugs; i++)
      dylib_close(dsp->plugs[i].lib);
   free(dsp->plugs);
#endif

   if (dsp->conf)
      config_file_free(dsp->conf);

   free(dsp);
}

void rarch_dsp_filter_process(rarch_dsp_filter_t *dsp, struct rarch_dsp_data *data)
{
   unsigned i;
   struct dspfilter_output output = {0};
   struct dspfilter_input input   = {0};

   output.samples = data->input;
   output.frames  = data->input_frames;

   for (i = 0; i < dsp->num_instances; i++)
   {
      input.samples = output.samples;
      input.frames  = output.frames;
      dsp->instances[i].impl->process(dsp->instances[i].impl_data, &output, &input);
   }

   data->output        = output.samples;
   data->output_frames = output.frames;
}

