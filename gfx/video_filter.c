/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <file/file_path.h>
#include <file/config_file_userdata.h>
#include <lists/dir_list.h>
#include <dynamic/dylib.h>
#include <features/features_cpu.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../frontend/frontend_driver.h"
#include "../dynamic.h"
#include "../performance_counters.h"
#include "../verbosity.h"
#include "video_filter.h"
#include "video_filters/softfilter.h"

struct rarch_soft_plug
{
#ifdef HAVE_DYLIB
   dylib_t lib;
#endif
   const struct softfilter_implementation *impl;
};

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>

struct filter_thread_data
{
   sthread_t *thread;
   const struct softfilter_work_packet *packet;
   scond_t *cond;
   slock_t *lock;
   void *userdata;
   bool die;
   bool done;
};

static void filter_thread_loop(void *data)
{
   struct filter_thread_data *thr = (struct filter_thread_data*)data;

   for (;;)
   {
      bool die;
      slock_lock(thr->lock);
      while (thr->done && !thr->die)
         scond_wait(thr->cond, thr->lock);
      die = thr->die;
      slock_unlock(thr->lock);

      if (die)
         break;

      if (thr->packet && thr->packet->work)
         thr->packet->work(thr->userdata, thr->packet->thread_data);

      slock_lock(thr->lock);
      thr->done = true;
      scond_signal(thr->cond);
      slock_unlock(thr->lock);
   }
}
#endif

struct rarch_softfilter
{
   config_file_t *conf;

   const struct softfilter_implementation *impl;
   void *impl_data;

   struct rarch_soft_plug *plugs;
   unsigned num_plugs;

   unsigned max_width, max_height;
   enum retro_pixel_format pix_fmt, out_pix_fmt;

   struct softfilter_work_packet *packets;
   unsigned threads;

#ifdef HAVE_THREADS
   struct filter_thread_data *thread_data;
#endif
};

static const struct softfilter_implementation *
softfilter_find_implementation(rarch_softfilter_t *filt, const char *ident)
{
   unsigned i;

   for (i = 0; i < filt->num_plugs; i++)
   {
      if (string_is_equal(filt->plugs[i].impl->short_ident, ident))
         return filt->plugs[i].impl;
   }

   return NULL;
}

static const struct softfilter_config softfilter_config = {
   config_userdata_get_float,
   config_userdata_get_int,
   config_userdata_get_float_array,
   config_userdata_get_int_array,
   config_userdata_get_string,
   config_userdata_free,
};

static bool create_softfilter_graph(rarch_softfilter_t *filt,
      enum retro_pixel_format in_pixel_format,
      unsigned max_width, unsigned max_height,
      softfilter_simd_mask_t cpu_features,
      unsigned threads)
{
   unsigned input_fmts, input_fmt, output_fmts, i = 0;
   struct config_file_userdata userdata;
   char key[64], name[64];

   (void)i;

   key[0] = name[0] = '\0';

   snprintf(key, sizeof(key), "filter");

   if (!config_get_array(filt->conf, key, name, sizeof(name)))
   {
      RARCH_ERR("Could not find 'filter' array in config.\n");
      return false;
   }

   if (filt->num_plugs == 0)
   {
      RARCH_ERR("No filter plugs found. Exiting...\n");
      return false;
   }

   filt->impl = softfilter_find_implementation(filt, name);
   if (!filt->impl)
   {
      RARCH_ERR("Could not find implementation.\n");
      return false;
   }

   userdata.conf = filt->conf;
   /* Index-specific configs take priority over ident-specific. */
   userdata.prefix[0] = key;
   userdata.prefix[1] = filt->impl->short_ident;

   /* Simple assumptions. */
   filt->pix_fmt = in_pixel_format;
   input_fmts = filt->impl->query_input_formats();

   switch (in_pixel_format)
   {
      case RETRO_PIXEL_FORMAT_XRGB8888:
         input_fmt = SOFTFILTER_FMT_XRGB8888;
         break;
      case RETRO_PIXEL_FORMAT_RGB565:
         input_fmt = SOFTFILTER_FMT_RGB565;
         break;
      default:
         return false;
   }

   if (!(input_fmt & input_fmts))
   {
      RARCH_ERR("Softfilter does not support input format.\n");
      return false;
   }

   output_fmts = filt->impl->query_output_formats(input_fmt);
   /* If we have a match of input/output formats, use that. */
   if (output_fmts & input_fmt)
      filt->out_pix_fmt = in_pixel_format;
   else if (output_fmts & SOFTFILTER_FMT_XRGB8888)
      filt->out_pix_fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   else if (output_fmts & SOFTFILTER_FMT_RGB565)
      filt->out_pix_fmt = RETRO_PIXEL_FORMAT_RGB565;
   else
   {
      RARCH_ERR("Did not find suitable output format for softfilter.\n");
      return false;
   }

   filt->max_width = max_width;
   filt->max_height = max_height;

   filt->impl_data = filt->impl->create(
         &softfilter_config, input_fmt, input_fmt, max_width, max_height,
         threads != RARCH_SOFTFILTER_THREADS_AUTO ? threads :
         cpu_features_get_core_amount(), cpu_features,
         &userdata);
   if (!filt->impl_data)
   {
      RARCH_ERR("Failed to create softfilter state.\n");
      return false;
   }

   threads = filt->impl->query_num_threads(filt->impl_data);
   if (!threads)
   {
      RARCH_ERR("Invalid number of threads.\n");
      return false;
   }

   filt->threads = threads;
   RARCH_LOG("Using %u threads for softfilter.\n", threads);

   filt->packets = (struct softfilter_work_packet*)
      calloc(threads, sizeof(*filt->packets));
   if (!filt->packets)
   {
      RARCH_ERR("Failed to allocate softfilter packets.\n");
      return false;
   }

#ifdef HAVE_THREADS
   if(filt->threads>1){
      filt->thread_data = (struct filter_thread_data*)
         calloc(threads, sizeof(*filt->thread_data));
      if (!filt->thread_data)
         return false;

      for (i = 0; i < threads; i++)
      {
         filt->thread_data[i].userdata = filt->impl_data;
         filt->thread_data[i].done = true;

         filt->thread_data[i].lock = slock_new();
         if (!filt->thread_data[i].lock)
            return false;
         filt->thread_data[i].cond = scond_new();
         if (!filt->thread_data[i].cond)
            return false;
         filt->thread_data[i].thread = sthread_create(
               filter_thread_loop, &filt->thread_data[i]);
         if (!filt->thread_data[i].thread)
            return false;
      }
   }
#endif

   return true;
}

#ifdef HAVE_DYLIB
static bool append_softfilter_plugs(rarch_softfilter_t *filt,
      struct string_list *list)
{
   unsigned i;
   softfilter_simd_mask_t mask = (softfilter_simd_mask_t)cpu_features_get();

   for (i = 0; i < list->size; i++)
   {
      softfilter_get_implementation_t cb;
      const struct softfilter_implementation *impl = NULL;
      struct rarch_soft_plug *new_plugs = NULL;
      dylib_t lib = dylib_load(list->elems[i].data);

      if (!lib)
         continue;

      cb = (softfilter_get_implementation_t)
         dylib_proc(lib, "softfilter_get_implementation");

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

      if (impl->api_version != SOFTFILTER_API_VERSION)
      {
         dylib_close(lib);
         continue;
      }

      new_plugs = (struct rarch_soft_plug*)
         realloc(filt->plugs, sizeof(*filt->plugs) * (filt->num_plugs + 1));
      if (!new_plugs)
      {
         dylib_close(lib);
         return false;
      }

      RARCH_LOG("[SoftFilter]: Found plug: %s (%s).\n",
            impl->ident, impl->short_ident);

      filt->plugs = new_plugs;
      filt->plugs[filt->num_plugs].lib = lib;
      filt->plugs[filt->num_plugs].impl = impl;
      filt->num_plugs++;
   }

   return true;
}
#elif defined(HAVE_FILTERS_BUILTIN)
extern const struct softfilter_implementation *blargg_ntsc_snes_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *lq2x_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *phosphor2x_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *twoxbr_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *epx_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *twoxsai_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *supereagle_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *supertwoxsai_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *twoxbr_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *darken_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *scale2x_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *normal2x_get_implementation(softfilter_simd_mask_t simd);
extern const struct softfilter_implementation *scanline2x_get_implementation(softfilter_simd_mask_t simd);

static const softfilter_get_implementation_t soft_plugs_builtin[] = {
   blargg_ntsc_snes_get_implementation,
   lq2x_get_implementation,
   phosphor2x_get_implementation,
   twoxbr_get_implementation,
   darken_get_implementation,
   twoxsai_get_implementation,
   supertwoxsai_get_implementation,
   supereagle_get_implementation,
   epx_get_implementation,
   scale2x_get_implementation,
   normal2x_get_implementation,
   scanline2x_get_implementation,
};

static bool append_softfilter_plugs(rarch_softfilter_t *filt,
      struct string_list *list)
{
   unsigned i;
   softfilter_simd_mask_t mask = (softfilter_simd_mask_t)cpu_features_get();

   (void)list;

   filt->plugs = (struct rarch_soft_plug*)
      calloc(ARRAY_SIZE(soft_plugs_builtin), sizeof(*filt->plugs));

   if (!filt->plugs)
      return false;

   filt->num_plugs = ARRAY_SIZE(soft_plugs_builtin);

   for (i = 0; i < ARRAY_SIZE(soft_plugs_builtin); i++)
   {
      filt->plugs[i].impl = soft_plugs_builtin[i](mask);
      if (!filt->plugs[i].impl)
         return false;
   }

   return true;
}
#else
static bool append_softfilter_plugs(rarch_softfilter_t *filt,
      struct string_list *list)
{
   (void)filt;
   (void)list;

   return false;
}
#endif

rarch_softfilter_t *rarch_softfilter_new(const char *filter_config,
      unsigned threads,
      enum retro_pixel_format in_pixel_format,
      unsigned max_width, unsigned max_height)
{
   softfilter_simd_mask_t cpu_features = (softfilter_simd_mask_t)cpu_features_get();
   char basedir[PATH_MAX_LENGTH];
#ifdef HAVE_DYLIB
   char ext_name[PATH_MAX_LENGTH];
#endif
   struct string_list *plugs     = NULL;
   rarch_softfilter_t *filt      = NULL;

   (void)basedir;

   filt = (rarch_softfilter_t*)calloc(1, sizeof(*filt));
   if (!filt)
      return NULL;

   if (!(filt->conf = config_file_new_from_path_to_string(filter_config)))
   {
      RARCH_ERR("[SoftFilter]: Did not find config: %s\n", filter_config);
      goto error;
   }

#if defined(HAVE_DYLIB)
   fill_pathname_basedir(basedir, filter_config, sizeof(basedir));

   if (!frontend_driver_get_core_extension(ext_name, sizeof(ext_name)))
         goto error;

   plugs = dir_list_new(basedir, ext_name, false, false, false, false);

   if (!plugs)
   {
      RARCH_ERR("[SoftFilter]: Could not build up string list...\n");
      goto error;
   }
#endif
   if (!append_softfilter_plugs(filt, plugs))
   {
      RARCH_ERR("[SoftFitler]: Failed to append softfilter plugins...\n");
      goto error;
   }

   if (plugs)
      string_list_free(plugs);
   plugs = NULL;

   if (!create_softfilter_graph(filt, in_pixel_format,
            max_width, max_height, cpu_features, threads))
   {
      RARCH_ERR("[SoftFitler]: Failed to create softfilter graph...\n");
      goto error;
   }

   return filt;

error:
   if (plugs)
      string_list_free(plugs);
   plugs = NULL;
   rarch_softfilter_free(filt);
   return NULL;
}

void rarch_softfilter_free(rarch_softfilter_t *filt)
{
   unsigned i = 0;
   (void)i;

   if (!filt)
      return;

   free(filt->packets);
   if (filt->impl && filt->impl_data)
      filt->impl->destroy(filt->impl_data);

#ifdef HAVE_DYLIB
   for (i = 0; i < filt->num_plugs; i++)
   {
      if (filt->plugs[i].lib)
         dylib_close(filt->plugs[i].lib);
   }
   free(filt->plugs);
#endif

#ifdef HAVE_THREADS
   if(filt->threads>1){
      for (i = 0; i < filt->threads; i++)
      {
         if (!filt->thread_data[i].thread)
            continue;
         slock_lock(filt->thread_data[i].lock);
         filt->thread_data[i].die = true;
         scond_signal(filt->thread_data[i].cond);
         slock_unlock(filt->thread_data[i].lock);
         sthread_join(filt->thread_data[i].thread);
         slock_free(filt->thread_data[i].lock);
         scond_free(filt->thread_data[i].cond);
      }
      free(filt->thread_data);
   }
#endif

   if (filt->conf)
      config_file_free(filt->conf);

   free(filt);
}

void rarch_softfilter_get_max_output_size(rarch_softfilter_t *filt,
      unsigned *width, unsigned *height)
{
   rarch_softfilter_get_output_size(filt, width, height,
         filt->max_width, filt->max_height);
}

void rarch_softfilter_get_output_size(rarch_softfilter_t *filt,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   if (filt && filt->impl && filt->impl->query_output_size)
      filt->impl->query_output_size(filt->impl_data, out_width,
            out_height, width, height);
}

enum retro_pixel_format rarch_softfilter_get_output_format(
      rarch_softfilter_t *filt)
{
   return filt->out_pix_fmt;
}

void rarch_softfilter_process(rarch_softfilter_t *filt,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height,
      size_t input_stride)
{
   unsigned i;

   if (!filt)
      return;

   if (filt->impl && filt->impl->get_work_packets)
      filt->impl->get_work_packets(filt->impl_data, filt->packets,
            output, output_stride, input, width, height, input_stride);

#ifdef HAVE_THREADS
   if(filt->threads>1){
      /* Fire off workers */
      for (i = 0; i < filt->threads; i++)
      {
#if 0
         RARCH_LOG("Firing off filter thread %u ...\n", i);
#endif
         filt->thread_data[i].packet = &filt->packets[i];
         slock_lock(filt->thread_data[i].lock);
         filt->thread_data[i].done = false;
         scond_signal(filt->thread_data[i].cond);
         slock_unlock(filt->thread_data[i].lock);
      }

      /* Wait for workers */
      for (i = 0; i < filt->threads; i++)
      {
#if 0
         RARCH_LOG("Waiting for filter thread %u ...\n", i);
#endif
         slock_lock(filt->thread_data[i].lock);
         while (!filt->thread_data[i].done)
            scond_wait(filt->thread_data[i].cond, filt->thread_data[i].lock);
         slock_unlock(filt->thread_data[i].lock);
      }
   } else {
      for (i = 0; i < filt->threads; i++)
         filt->packets[i].work(filt->impl_data, filt->packets[i].thread_data);
   }
#else
   for (i = 0; i < filt->threads; i++)
      filt->packets[i].work(filt->impl_data, filt->packets[i].thread_data);
#endif
}
