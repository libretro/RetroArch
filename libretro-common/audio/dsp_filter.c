/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (dsp_filter.c).
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

#include <retro_miscellaneous.h>

#include <compat/posix_string.h>
#include <dynamic/dylib.h>

#include <file/file_path.h>
#include <file/config_file_userdata.h>
#include <features/features_cpu.h>
#include <lists/string_list.h>
#include <string/stdstring.h>
#include <libretro_dspfilter.h>

#include <audio/dsp_filter.h>

struct retro_dsp_plug
{
#ifdef HAVE_DYLIB
   dylib_t lib;
#endif
   const struct dspfilter_implementation *impl;
};

struct retro_dsp_instance
{
   const struct dspfilter_implementation *impl;
   void *impl_data;
};

struct retro_dsp_filter
{
   config_file_t *conf;

   struct retro_dsp_plug *plugs;
   unsigned num_plugs;

   struct retro_dsp_instance *instances;
   unsigned num_instances;
};

static const struct dspfilter_implementation *find_implementation(
      retro_dsp_filter_t *dsp, const char *ident)
{
   unsigned i;
   for (i = 0; i < dsp->num_plugs; i++)
   {
      if (string_is_equal(dsp->plugs[i].impl->short_ident, ident))
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

static bool create_filter_graph(retro_dsp_filter_t *dsp, float sample_rate)
{
   unsigned i;
   struct retro_dsp_instance *instances = NULL;
   unsigned filters                     = 0;

   if (!config_get_uint(dsp->conf, "filters", &filters))
      return false;

   instances = (struct retro_dsp_instance*)calloc(filters, sizeof(*instances));
   if (!instances)
      return false;

   dsp->instances     = instances;
   dsp->num_instances = filters;

   for (i = 0; i < filters; i++)
   {
      struct config_file_userdata userdata;
      struct dspfilter_info info;
      char key[64];
      char name[64];

      key[0] = name[0] = '\0';

      info.input_rate  = sample_rate;

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

      dsp->instances[i].impl_data = dsp->instances[i].impl->init(&info,
            &dspfilter_config, &userdata);
      if (!dsp->instances[i].impl_data)
         return false;
   }

   return true;
}

#if defined(HAVE_FILTERS_BUILTIN)
extern const struct dspfilter_implementation *chorus_dspfilter_get_implementation(dspfilter_simd_mask_t mask);
extern const struct dspfilter_implementation *delta_dspfilter_get_implementation(dspfilter_simd_mask_t mask);
extern const struct dspfilter_implementation *echo_dspfilter_get_implementation(dspfilter_simd_mask_t mask);
extern const struct dspfilter_implementation *eq_dspfilter_get_implementation(dspfilter_simd_mask_t mask);
extern const struct dspfilter_implementation *iir_dspfilter_get_implementation(dspfilter_simd_mask_t mask);
extern const struct dspfilter_implementation *panning_dspfilter_get_implementation(dspfilter_simd_mask_t mask);
extern const struct dspfilter_implementation *phaser_dspfilter_get_implementation(dspfilter_simd_mask_t mask);
extern const struct dspfilter_implementation *reverb_dspfilter_get_implementation(dspfilter_simd_mask_t mask);
extern const struct dspfilter_implementation *tremolo_dspfilter_get_implementation(dspfilter_simd_mask_t mask);
extern const struct dspfilter_implementation *vibrato_dspfilter_get_implementation(dspfilter_simd_mask_t mask);
extern const struct dspfilter_implementation *wahwah_dspfilter_get_implementation(dspfilter_simd_mask_t mask);

static const dspfilter_get_implementation_t dsp_plugs_builtin[] = {
   chorus_dspfilter_get_implementation,
   delta_dspfilter_get_implementation,
   echo_dspfilter_get_implementation,
   eq_dspfilter_get_implementation,
   iir_dspfilter_get_implementation,
   panning_dspfilter_get_implementation,
   phaser_dspfilter_get_implementation,
   reverb_dspfilter_get_implementation,
   tremolo_dspfilter_get_implementation,
   vibrato_dspfilter_get_implementation,
   wahwah_dspfilter_get_implementation,
};

static bool append_plugs(retro_dsp_filter_t *dsp, struct string_list *list)
{
   unsigned i;
   dspfilter_simd_mask_t mask   = (dspfilter_simd_mask_t)cpu_features_get();
   struct retro_dsp_plug *plugs = (struct retro_dsp_plug*)
      calloc(ARRAY_SIZE(dsp_plugs_builtin), sizeof(*plugs));

   if (!plugs)
      return false;

   dsp->plugs     = plugs;
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
static bool append_plugs(retro_dsp_filter_t *dsp, struct string_list *list)
{
   unsigned i;
   dspfilter_simd_mask_t mask = (dspfilter_simd_mask_t)cpu_features_get();
   unsigned list_size         = list ? (unsigned)list->size : 0;

   for (i = 0; i < list_size; i++)
   {
      dspfilter_get_implementation_t cb;
      const struct dspfilter_implementation *impl = NULL;
      struct retro_dsp_plug *new_plugs            = NULL;
      dylib_t lib                                 =
         dylib_load(list->elems[i].data);

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

      new_plugs = (struct retro_dsp_plug*)
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

retro_dsp_filter_t *retro_dsp_filter_new(
      const char *filter_config,
      void *string_data,
      float sample_rate)
{
   config_file_t *conf           = NULL;
   struct string_list *plugs     = NULL;
   retro_dsp_filter_t *dsp       = (retro_dsp_filter_t*)calloc(1, sizeof(*dsp));

   if (!dsp)
      return NULL;

   if (!(conf = config_file_new_from_path_to_string(filter_config)))
      goto error;

   dsp->conf = conf;

   if (string_data)
      plugs = (struct string_list*)string_data;

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
   retro_dsp_filter_free(dsp);
   return NULL;
}

void retro_dsp_filter_free(retro_dsp_filter_t *dsp)
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

void retro_dsp_filter_process(retro_dsp_filter_t *dsp,
      struct retro_dsp_data *data)
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
