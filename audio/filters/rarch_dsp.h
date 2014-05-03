/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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
 *
 */

#ifndef DSPFILTER_API_H__
#define DSPFILTER_API_H__

#ifdef __cplusplus
extern "C" {
#endif

#define DSPFILTER_SIMD_SSE      (1 << 0)
#define DSPFILTER_SIMD_SSE2     (1 << 1)
#define DSPFILTER_SIMD_VMX      (1 << 2)
#define DSPFILTER_SIMD_VMX128   (1 << 3)
#define DSPFILTER_SIMD_AVX      (1 << 4)
#define DSPFILTER_SIMD_NEON     (1 << 5)
#define DSPFILTER_SIMD_SSE3     (1 << 6)
#define DSPFILTER_SIMD_SSSE3    (1 << 7)
#define DSPFILTER_SIMD_MMX      (1 << 8)
#define DSPFILTER_SIMD_MMXEXT   (1 << 9)
#define DSPFILTER_SIMD_SSE4     (1 << 10)
#define DSPFILTER_SIMD_SSE42    (1 << 11)
#define DSPFILTER_SIMD_AVX2     (1 << 12)
#define DSPFILTER_SIMD_VFPU     (1 << 13)
#define DSPFILTER_SIMD_PS       (1 << 14)

// A bit-mask of all supported SIMD instruction sets.
// Allows an implementation to pick different dspfilter_implementation structs.
typedef unsigned dspfilter_simd_mask_t;

// Dynamic library endpoint.
typedef const struct dspfilter_implementation *(*dspfilter_get_implementation_t)(dspfilter_simd_mask_t);
// The same SIMD mask argument is forwarded to create() callback as well to avoid having to keep lots of state around.
const struct dspfilter_implementation *rarch_dsp_plugin_init(dspfilter_simd_mask_t);

#define RARCH_DSP_API_VERSION 6

typedef struct rarch_dsp_info
{
   // Input sample rate that the DSP plugin receives.
   float input_rate;
} rarch_dsp_info_t;

typedef struct rarch_dsp_output
{
   // The DSP plugin has to provide the buffering for the output samples.
   // This is for performance reasons to avoid redundant copying of data.
   // The samples are laid out in interleaving order: LRLRLRLR
   // The range of the samples are [-1.0, 1.0]. 
   // This range cannot be exceeded without horrible audio glitches.
   const float *samples;

   // Frames which the DSP plugin outputted for the current process.
   // One frame is here defined as a combined sample of 
   // left and right channels. 
   // (I.e. 44.1kHz, 16bit stereo will have 
   // 88.2k samples/sec and 44.1k frames/sec.)
   unsigned frames;
} rarch_dsp_output_t;

typedef struct rarch_dsp_input
{
   // Input data for the DSP. The samples are interleaved in order: LRLRLRLR
   const float *samples;

   // Number of frames for input data.
   // One frame is here defined as a combined sample of 
   // left and right channels. 
   // (I.e. 44.1kHz, 16bit stereo will have 
   // 88.2k samples/sec and 44.1k frames/sec.)
   unsigned frames;
} rarch_dsp_input_t;

// Creates a handle of the plugin. Returns NULL if failed.
typedef void *(*dspfilter_init_t)(const rarch_dsp_info_t *info);

// Frees the handle.
typedef void (*dspfilter_free_t)(void *data);

// Processes input data. 
// The plugin is allowed to return variable sizes for output data.
typedef void (*dspfilter_process_t)(void *data, rarch_dsp_output_t *output, 
      const rarch_dsp_input_t *input);

// Signal plugin that it may open a configuring window or
// something similar. The behavior of this function
// is thus plugin dependent. Implementing this is optional,
// and can be set to NULL.
typedef void (*dspfilter_config_t)(void *data);

// Called every frame, allows creating a GUI main loop in the main thread.
// GUI events can be processed here in a non-blocking fashion.
// Can be set to NULL to ignore it.
typedef void (*dspfilter_events_t)(void *data);

struct dspfilter_implementation
{
   dspfilter_init_t     init;
   dspfilter_process_t  process;
   dspfilter_free_t     free;
   int api_version; // Must be RARCH_DSP_API_VERSION
   dspfilter_config_t   config;
   const char *ident; // Human readable identifier of implementation.
   dspfilter_events_t   events;
};


#ifdef __cplusplus
}
#endif

#endif
