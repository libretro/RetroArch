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

#include "filter.h"
#include "filters/softfilter.h"
#include "../dynamic.h"
#include "../general.h"
#include "../performance.h"
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_THREADS
#include "../thread.h"

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
      slock_lock(thr->lock);
      while (thr->done && !thr->die)
         scond_wait(thr->cond, thr->lock);
      bool die = thr->die;
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
#if !defined(HAVE_FILTERS_BUILTIN) && defined(HAVE_DYLIB)
   dylib_t lib;
#endif

   const struct softfilter_implementation *impl;
   void *impl_data;

   unsigned max_width, max_height;
   enum retro_pixel_format pix_fmt, out_pix_fmt;

   struct softfilter_work_packet *packets;
   unsigned threads;

#ifdef HAVE_THREADS
   struct filter_thread_data *thread_data;
#endif
};

#ifdef HAVE_FILTERS_BUILTIN
static const struct softfilter_implementation *(*softfilter_drivers[]) (softfilter_simd_mask_t) =
{
   NULL,
   &blargg_ntsc_snes_rf_get_implementation,
   &blargg_ntsc_snes_composite_get_implementation,
   &blargg_ntsc_snes_svideo_get_implementation,
   &blargg_ntsc_snes_rgb_get_implementation,
   &lq2x_get_implementation,
   &phosphor2x_get_implementation,
   &twoxbr_get_implementation,
   &darken_get_implementation,
   &twoxsai_get_implementation,
   &supertwoxsai_get_implementation,
   &supereagle_get_implementation,
   &epx_get_implementation,
   &scale2x_get_implementation,
};

unsigned softfilter_get_last_idx(void)
{
   return sizeof(softfilter_drivers) / sizeof(softfilter_drivers[0]);
}

static softfilter_get_implementation_t softfilter_get_implementation_from_idx(unsigned i)
{
   if (i < softfilter_get_last_idx())
      return softfilter_drivers[i];
   return NULL;
}

#endif

const char *rarch_softfilter_get_name(void *data)
{
   (void)data;
#ifdef HAVE_FILTERS_BUILTIN
   unsigned cpu_features;
   const struct softfilter_implementation *impl;
   softfilter_get_implementation_t cb = (softfilter_get_implementation_t)softfilter_get_implementation_from_idx(g_settings.video.filter_idx);
   if (cb)
   {
      cpu_features = rarch_get_cpu_features();
      impl = (const struct softfilter_implementation *)cb(cpu_features);
      if (impl)
         return impl->ident;
   }

   return NULL;
#else
   rarch_softfilter_t *filt = (rarch_softfilter_t*)data;
   if (!filt || !filt->impl)
      return NULL;

   return filt->impl->ident;
#endif
}

rarch_softfilter_t *rarch_softfilter_new(const char *filter_path,
      unsigned threads,
      enum retro_pixel_format in_pixel_format,
      unsigned max_width, unsigned max_height)
{
   unsigned i, cpu_features, output_fmts, input_fmts, input_fmt;
   softfilter_get_implementation_t cb;
    
   i = 0;
   (void)i;

   rarch_softfilter_t *filt = (rarch_softfilter_t*)calloc(1, sizeof(*filt));
   if (!filt)
      return NULL;

#if defined(HAVE_FILTERS_BUILTIN)
   cb = (softfilter_get_implementation_t)softfilter_get_implementation_from_idx(g_settings.video.filter_idx);
#elif defined(HAVE_DYLIB)
   filt->lib = dylib_load(filter_path);
   if (!filt->lib)
      goto error;

   cb = (softfilter_get_implementation_t)dylib_proc(filt->lib, "softfilter_get_implementation");
#endif
   if (!cb)
   {
      RARCH_ERR("Couldn't find softfilter symbol.\n");
      goto error;
   }

   cpu_features = rarch_get_cpu_features();
   filt->impl = cb(cpu_features);
   if (!filt->impl)
      goto error;

   RARCH_LOG("Loaded softfilter \"%s\".\n", filt->impl->ident);

   if (filt->impl->api_version != SOFTFILTER_API_VERSION)
   {
      RARCH_ERR("Softfilter ABI mismatch.\n");
      goto error;
   }

   // Simple assumptions.
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
         goto error;
   }

   if (!(input_fmt & input_fmts))
   {
      RARCH_ERR("Softfilter does not support input format.\n");
      goto error;
   }

   output_fmts = filt->impl->query_output_formats(input_fmt);
   if (output_fmts & input_fmt) // If we have a match of input/output formats, use that.
      filt->out_pix_fmt = in_pixel_format;
   else if (output_fmts & SOFTFILTER_FMT_XRGB8888)
      filt->out_pix_fmt = RETRO_PIXEL_FORMAT_XRGB8888;
   else if (output_fmts & SOFTFILTER_FMT_RGB565)
      filt->out_pix_fmt = RETRO_PIXEL_FORMAT_RGB565;
   else
   {
      RARCH_ERR("Did not find suitable output format for softfilter.\n");
      goto error;
   }

   filt->max_width = max_width;
   filt->max_height = max_height;

   filt->impl_data = filt->impl->create(input_fmt, input_fmt, max_width, max_height,
         threads != RARCH_SOFTFILTER_THREADS_AUTO ? threads : rarch_get_cpu_cores(), cpu_features);
   if (!filt->impl_data)
   {
      RARCH_ERR("Failed to create softfilter state.\n");
      goto error;
   }

   threads = filt->impl->query_num_threads(filt->impl_data);
   if (!threads)
   {
      RARCH_ERR("Invalid number of threads.\n");
      goto error;
   }

   RARCH_LOG("Using %u threads for softfilter.\n", threads);

   filt->packets = (struct softfilter_work_packet*)calloc(threads, sizeof(*filt->packets));
   if (!filt->packets)
   {
      RARCH_ERR("Failed to allocate softfilter packets.\n");
      goto error;
   }

#ifdef HAVE_THREADS
   filt->thread_data = (struct filter_thread_data*)calloc(threads, sizeof(*filt->thread_data));
   if (!filt->thread_data)
      goto error;
   filt->threads = threads;

   for (i = 0; i < threads; i++)
   {
      filt->thread_data[i].userdata = filt->impl_data;
      filt->thread_data[i].done = true;

      filt->thread_data[i].lock = slock_new();
      if (!filt->thread_data[i].lock)
         goto error;
      filt->thread_data[i].cond = scond_new();
      if (!filt->thread_data[i].cond)
         goto error;
      filt->thread_data[i].thread = sthread_create(filter_thread_loop, &filt->thread_data[i]);
      if (!filt->thread_data[i].thread)
         goto error;
   }
#endif

   return filt;

error:
   rarch_softfilter_free(filt);
   return NULL;
}

void rarch_softfilter_free(rarch_softfilter_t *filt)
{
   unsigned i;
   i = 0;
   (void)i;
   if (!filt)
      return;

   free(filt->packets);
   if (filt->impl && filt->impl_data)
      filt->impl->destroy(filt->impl_data);
#if !defined(HAVE_FILTERS_BUILTIN) && defined(HAVE_DYLIB)
   if (filt->lib)
      dylib_close(filt->lib);
#endif
#ifdef HAVE_THREADS
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
#endif
   free(filt);
}

void rarch_softfilter_get_max_output_size(rarch_softfilter_t *filt,
      unsigned *width, unsigned *height)
{
   rarch_softfilter_get_output_size(filt, width, height, filt->max_width, filt->max_height);
}

void rarch_softfilter_get_output_size(rarch_softfilter_t *filt,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height)
{
   if (filt && filt->impl && filt->impl->query_output_size)
      filt->impl->query_output_size(filt->impl_data, out_width, out_height, width, height);
}

enum retro_pixel_format rarch_softfilter_get_output_format(rarch_softfilter_t *filt)
{
   return filt->out_pix_fmt;
}

void rarch_softfilter_process(rarch_softfilter_t *filt,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride)
{
   unsigned i;

   if (filt && filt->impl && filt->impl->get_work_packets)
      filt->impl->get_work_packets(filt->impl_data, filt->packets,
            output, output_stride, input, width, height, input_stride);
   
#ifdef HAVE_THREADS
   // Fire off workers
   for (i = 0; i < filt->threads; i++)
   {
      //RARCH_LOG("Firing off filter thread %u ...\n", i);
      filt->thread_data[i].packet = &filt->packets[i];
      slock_lock(filt->thread_data[i].lock);
      filt->thread_data[i].done = false;
      scond_signal(filt->thread_data[i].cond);
      slock_unlock(filt->thread_data[i].lock);
   }

   // Wait for workers
   for (i = 0; i < filt->threads; i++)
   {
      //RARCH_LOG("Waiting for filter thread %u ...\n", i);
      slock_lock(filt->thread_data[i].lock);
      while (!filt->thread_data[i].done)
         scond_wait(filt->thread_data[i].cond, filt->thread_data[i].lock);
      slock_unlock(filt->thread_data[i].lock);
   }
#else
   for (i = 0; i < filt->threads; i++)
      filt->packets[i].work(filt->impl_data, filt->packets[i].thread_data);
#endif
}

