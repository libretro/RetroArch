/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <stdlib.h>

#include <retro_miscellaneous.h>

#include <compat/posix_string.h>
#include <dynamic/dylib.h>

#include <file/file_path.h>
#include <file/config_file_userdata.h>
#include <file/dir_list.h>

#include "audio_dsp_filter.h"
#include "audio_filters/dspfilter.h"

#include "../frontend/frontend_driver.h"
#include "../performance.h"
#include "../dynamic.h"

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

static const struct dspfilter_implementation *find_implementation(
      rarch_dsp_filter_t *dsp, const char *ident)
{
   unsigned i;
   for (i = 0; i < dsp->num_plugs; i++)
   {
      if (!strcmp(dsp->plugs[i].impl->short_ident, ident))
         return dsp->plugs[i].impl;
   }

   return NULL;
}

static const struct dspfilter_config dspfilter_config = {
   config_userdata_get_float,
   config_userdata_get_int,
   config_userdata_get_float_array,
   config_userdata_get_int_array,
   config_userdata_get_string,
   config_userdata_free,
};

static bool create_filter_graph(rarch_dsp_filter_t *dsp, float sample_rate)
{
   unsigned i, filters = 0;

   if (!config_get_uint(dsp->conf, "filters", &filters))
      return false;

   dsp->instances = (struct rarch_dsp_instance*)
      calloc(filters, sizeof(*dsp->instances));
   if (!dsp->instances)
      return false;

   dsp->num_instances = filters;

   for (i = 0; i < filters; i++)
   {
      struct config_file_userdata userdata;
      struct dspfilter_info info;
      char key[64]                         = {0};
      char name[64]                        = {0};

      info.input_rate = sample_rate;

      snprintf(key, sizeof(key), "filter%u", i);

      if (!config_get_array(dsp->conf, key, name, sizeof(name)))
         return false;

      dsp->instances[i].impl = find_implementation(dsp, name);
      if (!dsp->instances[i].impl)
         return false;

      userdata.conf = dsp->conf;
      /* Index-specific configs take priority over ident-specific. */
      userdata.prefix[0] = key;
      userdata.prefix[1] = dsp->instances[i].impl->short_ident;

      dsp->instances[i].impl_data = dsp->instances[i].impl->init(&info, &dspfilter_config, &userdata);
      if (!dsp->instances[i].impl_data)
         return false;
   }

   return true;
}

#if defined(HAVE_FILTERS_BUILTIN)
extern const struct dspfilter_implementation *panning_dspfilter_get_implementation(dspfilter_simd_mask_t mask);
extern const struct dspfilter_implementation *iir_dspfilter_get_implementation(dspfilter_simd_mask_t mask);
extern const struct dspfilter_implementation *echo_dspfilter_get_implementation(dspfilter_simd_mask_t mask);
extern const struct dspfilter_implementation *phaser_dspfilter_get_implementation(dspfilter_simd_mask_t mask);
extern const struct dspfilter_implementation *wahwah_dspfilter_get_implementation(dspfilter_simd_mask_t mask);
extern const struct dspfilter_implementation *eq_dspfilter_get_implementation(dspfilter_simd_mask_t mask);
extern const struct dspfilter_implementation *chorus_dspfilter_get_implementation(dspfilter_simd_mask_t mask);

static const dspfilter_get_implementation_t dsp_plugs_builtin[] = {
   panning_dspfilter_get_implementation,
   iir_dspfilter_get_implementation,
   echo_dspfilter_get_implementation,
   phaser_dspfilter_get_implementation,
   wahwah_dspfilter_get_implementation,
   eq_dspfilter_get_implementation,
   chorus_dspfilter_get_implementation,
};

static bool append_plugs(rarch_dsp_filter_t *dsp, struct string_list *list)
{
   unsigned i;
   dspfilter_simd_mask_t mask = retro_get_cpu_features();

   (void)list;

   dsp->plugs = (struct rarch_dsp_plug*)
      calloc(ARRAY_SIZE(dsp_plugs_builtin), sizeof(*dsp->plugs));
   if (!dsp->plugs)
      return false;

   dsp->num_plugs = ARRAY_SIZE(dsp_plugs_builtin);

   for (i = 0; i < ARRAY_SIZE(dsp_plugs_builtin); i++)
   {
      dsp->plugs[i].impl = dsp_plugs_builtin[i](mask);
      if (!dsp->plugs[i].impl)
         return false;
   }

   return true;
}
#elif defined(HAVE_DYLIB)
static bool append_plugs(rarch_dsp_filter_t *dsp, struct string_list *list)
{
   unsigned i;
   dspfilter_simd_mask_t mask = retro_get_cpu_features();

   for (i = 0; i < list->size; i++)
   {
      const struct dspfilter_implementation *impl = NULL;
      struct rarch_dsp_plug *new_plugs = NULL;
      dylib_t lib = dylib_load(list->elems[i].data);
      dspfilter_get_implementation_t cb;

      if (!lib)
         continue;

      cb = (dspfilter_get_implementation_t)dylib_proc(lib, "dspfilter_get_implementation");
      if (!cb)
      {
         dylib_close(lib);
         continue;
      }

      impl = cb(mask);
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

      new_plugs = (struct rarch_dsp_plug*)
         realloc(dsp->plugs, sizeof(*dsp->plugs) * (dsp->num_plugs + 1));
      if (!new_plugs)
      {
         dylib_close(lib);
         return false;
      }

      /* Found plug. */
      
      dsp->plugs = new_plugs;
      dsp->plugs[dsp->num_plugs].lib = lib;
      dsp->plugs[dsp->num_plugs].impl = impl;
      dsp->num_plugs++;
   }

   return true;
}
#endif

rarch_dsp_filter_t *rarch_dsp_filter_new(
      const char *filter_config, float sample_rate)
{
#ifdef HAVE_DYLIB
   char basedir[PATH_MAX_LENGTH];
   char ext_name[PATH_MAX_LENGTH];
#endif
   struct string_list *plugs     = NULL;
   rarch_dsp_filter_t *dsp       = NULL;

   dsp = (rarch_dsp_filter_t*)calloc(1, sizeof(*dsp));
   if (!dsp)
      return NULL;

   dsp->conf = config_file_new(filter_config);
   if (!dsp->conf)   /* Did not find config. */
      goto error;

#if !defined(HAVE_FILTERS_BUILTIN) && defined(HAVE_DYLIB)
   fill_pathname_basedir(basedir, filter_config, sizeof(basedir));

   if (!frontend_driver_get_core_extension(ext_name, sizeof(ext_name)))
         goto error;

   plugs = dir_list_new(basedir, ext_name, false, false);
   if (!plugs)
      goto error;
#endif

#if defined(HAVE_DYLIB) || defined(HAVE_FILTERS_BUILTIN)
   if (!append_plugs(dsp, plugs))
      goto error;
#endif

   if (plugs)
      string_list_free(plugs);
   plugs = NULL;

   if (!create_filter_graph(dsp, sample_rate))
      goto error;

   return dsp;

error:
   if (plugs)
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
   {
      if (dsp->plugs[i].lib)
         dylib_close(dsp->plugs[i].lib);
   }
   free(dsp->plugs);
#endif

   if (dsp->conf)
      config_file_free(dsp->conf);

   free(dsp);
}

void rarch_dsp_filter_process(rarch_dsp_filter_t *dsp,
      struct rarch_dsp_data *data)
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
      dsp->instances[i].impl->process(
            dsp->instances[i].impl_data, &output, &input);
   }

   data->output        = output.samples;
   data->output_frames = output.frames;
}
