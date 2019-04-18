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

#ifndef SOFTFILTER_API_H__
#define SOFTFILTER_API_H__

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SOFTFILTER_SIMD_SSE      (1 << 0)
#define SOFTFILTER_SIMD_SSE2     (1 << 1)
#define SOFTFILTER_SIMD_VMX      (1 << 2)
#define SOFTFILTER_SIMD_VMX128   (1 << 3)
#define SOFTFILTER_SIMD_AVX      (1 << 4)
#define SOFTFILTER_SIMD_NEON     (1 << 5)
#define SOFTFILTER_SIMD_SSE3     (1 << 6)
#define SOFTFILTER_SIMD_SSSE3    (1 << 7)
#define SOFTFILTER_SIMD_MMX      (1 << 8)
#define SOFTFILTER_SIMD_MMXEXT   (1 << 9)
#define SOFTFILTER_SIMD_SSE4     (1 << 10)
#define SOFTFILTER_SIMD_SSE42    (1 << 11)
#define SOFTFILTER_SIMD_AVX2     (1 << 12)
#define SOFTFILTER_SIMD_VFPU     (1 << 13)
#define SOFTFILTER_SIMD_PS       (1 << 14)

/* A bit-mask of all supported SIMD instruction sets.
 * Allows an implementation to pick different
 * softfilter_implementation structs. */
typedef unsigned softfilter_simd_mask_t;

/* Returns true if config key was found. Otherwise, returns false,
 * and sets value to default value. */
typedef int (*softfilter_config_get_float_t)(void *userdata,
      const char *key, float *value, float default_value);

typedef int (*softfilter_config_get_int_t)(void *userdata,
      const char *key, int *value, int default_value);

/* Allocates an array with values. free() with softfilter_config_free_t. */
typedef int (*softfilter_config_get_float_array_t)(void *userdata,
      const char *key,
      float **values, unsigned *out_num_values,
      const float *default_values, unsigned num_default_values);

typedef int (*softfilter_config_get_int_array_t)(void *userdata, const char *key,
      int **values, unsigned *out_num_values,
      const int *default_values, unsigned num_default_values);

typedef int (*softfilter_config_get_string_t)(void *userdata,
      const char *key, char **output, const char *default_output);

/* Calls free() in host runtime.
 * Sometimes needed on Windows. free() on NULL is fine. */
typedef void (*softfilter_config_free_t)(void *ptr);

struct softfilter_config
{
   softfilter_config_get_float_t get_float;
   softfilter_config_get_int_t get_int;

   softfilter_config_get_float_array_t get_float_array;
   softfilter_config_get_int_array_t get_int_array;

   softfilter_config_get_string_t get_string;
   /* Avoid problems where softfilter plug and
    * host are linked against different C runtimes. */
   softfilter_config_free_t free;
};

/* Dynamic library entrypoint. */
typedef const struct softfilter_implementation
*(*softfilter_get_implementation_t)(softfilter_simd_mask_t);

/* The same SIMD mask argument is forwarded to create() callback
 * as well to avoid having to keep lots of state around. */
const struct softfilter_implementation *softfilter_get_implementation(
      softfilter_simd_mask_t simd);

#define SOFTFILTER_API_VERSION  2

/* Required base color formats */

#define SOFTFILTER_FMT_NONE     0
#define SOFTFILTER_FMT_RGB565   (1 << 0)
#define SOFTFILTER_FMT_XRGB8888 (1 << 1)

/* Optional color formats */
#define SOFTFILTER_FMT_RGB4444  (1 << 2)

#define SOFTFILTER_BPP_RGB565   2
#define SOFTFILTER_BPP_XRGB8888 4

/* Softfilter implementation.
 * Returns a bitmask of supported input formats. */
typedef unsigned (*softfilter_query_input_formats_t)(void);

/* Returns a bitmask of supported output formats
 * for a given input format. */
typedef unsigned (*softfilter_query_output_formats_t)(unsigned input_format);

/* In softfilter_process_t, the softfilter implementation
 * submits work units to a worker thread pool. */
typedef void (*softfilter_work_t)(void *data, void *thread_data);
struct softfilter_work_packet
{
   softfilter_work_t work;
   void *thread_data;
};

/* Create a filter with given input and output formats as well as
 * maximum possible input size.
 *
 * Input sizes can very per call to softfilter_process_t, but they
 * will never be larger than the maximum. */
typedef void *(*softfilter_create_t)(const struct softfilter_config *config,
      unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd, void *userdata);

typedef void (*softfilter_destroy_t)(void *data);

/* Given an input size, query the output size of the filter.
 * If width and height == max_width/max_height, no other combination
 * of width/height must return a larger size in any dimension. */
typedef void (*softfilter_query_output_size_t)(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height);

/* First step of processing a frame. The filter submits work by
 * filling in the packets array.
 *
 * The number of elements in the array is as returned by query_num_threads.
 * The processing itself happens in worker threads after this returns.
 */
typedef void (*softfilter_get_work_packets_t)(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride);

/* Returns the number of worker threads the filter will use.
 * This can differ from the value passed to create() instead the filter
 * cannot be parallelized, etc. The number of threads must be less-or-equal
 * compared to the value passed to create(). */
typedef unsigned (*softfilter_query_num_threads_t)(void *data);

struct softfilter_implementation
{
   softfilter_query_input_formats_t query_input_formats;
   softfilter_query_output_formats_t query_output_formats;

   softfilter_create_t create;
   softfilter_destroy_t destroy;

   softfilter_query_num_threads_t query_num_threads;
   softfilter_query_output_size_t query_output_size;
   softfilter_get_work_packets_t get_work_packets;

   /* Must be SOFTFILTER_API_VERSION. */
   unsigned api_version;
   /* Human readable identifier of implementation. */
   const char *ident;
   /* Computer-friendly short version of ident.
    * Lower case, no spaces and special characters, etc. */
   const char *short_ident;
};

#ifdef __cplusplus
}
#endif

#endif
