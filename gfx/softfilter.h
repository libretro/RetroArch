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

#ifndef SOFTFILTER_API_H__
#define SOFTFILTER_API_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned softfilter_simd_mask_t;

// Dynamic library entrypoint.
typedef const struct softfilter_implementation *(*softfilter_get_implementation_t)(softfilter_simd_mask_t);
const struct softfilter_implementation *softfilter_get_implementation(softfilter_simd_mask_t simd);

#define SOFTFILTER_API_VERSION 1

#define SOFTFILTER_FMT_NONE 0
#define SOFTFILTER_FMT_RGB565 (1 << 0)
#define SOFTFILTER_FMT_XRGB8888 (1 << 1)

typedef unsigned (*softfilter_query_input_formats_t)(void);
typedef unsigned (*softfilter_query_output_formats_t)(unsigned input_format);

typedef void (*softfilter_work_t)(void *userdata, void *thread_data);
typedef void (*softfilter_submit_t)(softfilter_work_t work, void *userdata);

typedef void *(*softfilter_create_t)(unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads);
typedef void (*softfilter_destroy_t)(void *data);

typedef void (*softfilter_query_output_size_t)(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height);

typedef void (*softfilter_process_t)(void *data,
      softfilter_submit_t callback,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride);

typedef unsigned (*softfilter_query_num_threads_t)(void *data, unsigned desired_threads);

struct softfilter_implementation
{
   softfilter_query_input_formats_t query_input_formats;
   softfilter_query_output_formats_t query_output_format;

   softfilter_create_t create;
   softfilter_destroy_t destroy;

   softfilter_query_num_threads_t query_num_threads;
   softfilter_query_output_size_t query_output_size;
   softfilter_process_t process;

   const char *ident;
   unsigned api_version;
};

#ifdef __cplusplus
}
#endif

#endif

