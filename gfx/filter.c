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
#include "../dynamic.h"
#include "../general.h"
#include "../performance.h"
#include <stdlib.h>

struct rarch_softfilter
{
   dylib_t *lib;

   const struct softfilter_implementation *impl;
   void *impl_data;

   unsigned max_width, max_height;
   unsigned threads;
   enum retro_pixel_format pix_fmt, out_pix_fmt;
};

rarch_softfilter_t *rarch_softfilter_new(const char *filter_path,
      unsigned threads,
      enum retro_pixel_format in_pixel_format,
      unsigned max_width, unsigned max_height)
{
   rarch_softfilter_t *filt = (rarch_softfilter_t*)calloc(1, sizeof(*filt));
   if (!filt)
      return NULL;

   filt->lib = dylib_load(filter_path);
   if (!filt->lib)
      goto error;

   softfilter_get_implementation_t cb = (softfilter_get_implementation_t)dylib_proc(filt->lib, "softfilter_get_implementation");
   if (!cb)
   {
      RARCH_ERR("Couldn't find softfilter symbol.\n");
      goto error;
   }

   unsigned cpu_features = rarch_get_cpu_features();
   filt->impl = cb(cpu_features);
   if (!filt->impl)
      goto error;

   filt->max_width = max_width;
   filt->max_height = max_height;
   filt->pix_fmt = in_pixel_format;
   filt->threads = threads;

   filt->out_pix_fmt = in_pixel_format;

   return filt;

error:
   rarch_softfilter_free(filt);
   return NULL;
}

void rarch_softfilter_free(rarch_softfilter_t *filt)
{
   if (!filt)
      return;

   if (filt->impl && filt->impl_data)
      filt->impl->destroy(filt->impl_data);
   if (filt->lib)
      dylib_close(filt->lib);
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
}

