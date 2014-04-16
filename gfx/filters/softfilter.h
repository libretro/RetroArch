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

// A bit-mask of all supported SIMD instruction sets.
// Allows an implementation to pick different softfilter_implementation structs.
typedef unsigned softfilter_simd_mask_t;

// Dynamic library entrypoint.
typedef const struct softfilter_implementation *(*softfilter_get_implementation_t)(softfilter_simd_mask_t);
// The same SIMD mask argument is forwarded to create() callback as well to avoid having to keep lots of state around.
const struct softfilter_implementation *softfilter_get_implementation(softfilter_simd_mask_t simd);

#define SOFTFILTER_API_VERSION 1

#define SOFTFILTER_FMT_NONE 0
#define SOFTFILTER_FMT_RGB565 (1 << 0)
#define SOFTFILTER_FMT_XRGB8888 (1 << 1)

#define SOFTFILTER_BPP_RGB565 2
#define SOFTFILTER_BPP_XRGB8888 4

// Softfilter implementation.
// Returns a bitmask of supported input formats.
typedef unsigned (*softfilter_query_input_formats_t)(void);

// Returns a bitmask of supported output formats for a given input format.
typedef unsigned (*softfilter_query_output_formats_t)(unsigned input_format);

// In softfilter_process_t, the softfilter implementation submits work units to a worker thread pool.
typedef void (*softfilter_work_t)(void *data, void *thread_data);
struct softfilter_work_packet
{
   softfilter_work_t work;
   void *thread_data;
};

// Create a filter with given input and output formats as well as maximum possible input size.
// Input sizes can very per call to softfilter_process_t, but they will never be larger than the maximum.
typedef void *(*softfilter_create_t)(unsigned in_fmt, unsigned out_fmt,
      unsigned max_width, unsigned max_height,
      unsigned threads, softfilter_simd_mask_t simd);
typedef void (*softfilter_destroy_t)(void *data);

// Given an input size, query the output size of the filter.
// If width and height == max_width/max_height, no other combination of width/height must return a larger size in any dimension.
typedef void (*softfilter_query_output_size_t)(void *data,
      unsigned *out_width, unsigned *out_height,
      unsigned width, unsigned height);

// First step of processing a frame. The filter submits work by filling in the packets array.
// The number of elements in the array is as returned by query_num_threads.
// The processing itself happens in worker threads after this returns.
typedef void (*softfilter_get_work_packets_t)(void *data,
      struct softfilter_work_packet *packets,
      void *output, size_t output_stride,
      const void *input, unsigned width, unsigned height, size_t input_stride);

// Returns the number of worker threads the filter will use.
// This can differ from the value passed to create() instead the filter cannot be parallelized, etc. The number of threads must be less-or-equal compared to the value passed to create().
typedef unsigned (*softfilter_query_num_threads_t)(void *data);
/////

struct softfilter_thread_data
{
   void *out_data;
   const void *in_data;
   size_t out_pitch;
   size_t in_pitch;
   unsigned colfmt;
   unsigned width;
   unsigned height;
   int first;
   int last;
};

struct filter_data
{
   unsigned threads;
   struct softfilter_thread_data *workers;
   unsigned in_fmt;
};

struct softfilter_implementation
{
   softfilter_query_input_formats_t query_input_formats;
   softfilter_query_output_formats_t query_output_formats;

   softfilter_create_t create;
   softfilter_destroy_t destroy;

   softfilter_query_num_threads_t query_num_threads;
   softfilter_query_output_size_t query_output_size;
   softfilter_get_work_packets_t get_work_packets;

   const char *ident; // Human readable identifier of implementation.
   unsigned api_version; // Must be SOFTFILTER_API_VERSION
};

#ifdef __cplusplus
}
#endif

#endif

