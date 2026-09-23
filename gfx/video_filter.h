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

#ifndef RARCH_FILTER_H__
#define RARCH_FILTER_H__

#include <stddef.h>

#include <libretro.h>
#include <retro_common_api.h>

#include "video_defines.h"   /* VIDEO_SCALE_PACK */

#define RARCH_SOFTFILTER_THREADS_AUTO 0

RETRO_BEGIN_DECLS

typedef struct rarch_softfilter rarch_softfilter_t;

rarch_softfilter_t *rarch_softfilter_new(
      const char *filter_path,
      unsigned threads,
      enum retro_pixel_format in_pixel_format,
      unsigned max_dims);

void rarch_softfilter_free(rarch_softfilter_t *filt);

/* Sizes are words in VIDEO_SCALE_PACK's layout. The two size queries
 * seed *out_dims with what the caller has, and a filter with no size
 * query of its own leaves it that way. */
void rarch_softfilter_get_max_output_size(rarch_softfilter_t *filt,
      unsigned *out_dims);

void rarch_softfilter_get_output_size(rarch_softfilter_t *filt,
      unsigned *out_dims, unsigned in_dims);

enum retro_pixel_format rarch_softfilter_get_output_format(
      rarch_softfilter_t *filt);

void rarch_softfilter_process(rarch_softfilter_t *filt,
      void *output, size_t output_stride,
      const void *input, unsigned in_dims, size_t input_stride);

const char *rarch_softfilter_get_name(void *data);

RETRO_END_DECLS

#endif
