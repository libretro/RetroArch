/* Copyright  (C) 2010-2018 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_resampler.h).
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

#ifndef __LIBRETRO_SDK_AUDIO_RESAMPLER_DRIVER_H
#define __LIBRETRO_SDK_AUDIO_RESAMPLER_DRIVER_H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

#define RESAMPLER_SIMD_SSE      (1 << 0)
#define RESAMPLER_SIMD_SSE2     (1 << 1)
#define RESAMPLER_SIMD_VMX      (1 << 2)
#define RESAMPLER_SIMD_VMX128   (1 << 3)
#define RESAMPLER_SIMD_AVX      (1 << 4)
#define RESAMPLER_SIMD_NEON     (1 << 5)
#define RESAMPLER_SIMD_SSE3     (1 << 6)
#define RESAMPLER_SIMD_SSSE3    (1 << 7)
#define RESAMPLER_SIMD_MMX      (1 << 8)
#define RESAMPLER_SIMD_MMXEXT   (1 << 9)
#define RESAMPLER_SIMD_SSE4     (1 << 10)
#define RESAMPLER_SIMD_SSE42    (1 << 11)
#define RESAMPLER_SIMD_AVX2     (1 << 12)
#define RESAMPLER_SIMD_VFPU     (1 << 13)
#define RESAMPLER_SIMD_PS       (1 << 14)

enum resampler_quality
{
   RESAMPLER_QUALITY_DONTCARE = 0,
   RESAMPLER_QUALITY_LOWEST,
   RESAMPLER_QUALITY_LOWER,
   RESAMPLER_QUALITY_NORMAL,
   RESAMPLER_QUALITY_HIGHER,
   RESAMPLER_QUALITY_HIGHEST
};

/* A bit-mask of all supported SIMD instruction sets.
 * Allows an implementation to pick different
 * resampler_implementation structs.
 */
typedef unsigned resampler_simd_mask_t;

#define RESAMPLER_API_VERSION 1

struct resampler_data
{
   const float *data_in;
   float *data_out;

   size_t input_frames;
   size_t output_frames;

   double ratio;
};

/* Returns true if config key was found. Otherwise,
 * returns false, and sets value to default value.
 */
typedef int (*resampler_config_get_float_t)(void *userdata,
      const char *key, float *value, float default_value);

typedef int (*resampler_config_get_int_t)(void *userdata,
      const char *key, int *value, int default_value);

/* Allocates an array with values. free() with resampler_config_free_t. */
typedef int (*resampler_config_get_float_array_t)(void *userdata,
      const char *key, float **values, unsigned *out_num_values,
      const float *default_values, unsigned num_default_values);

typedef int (*resampler_config_get_int_array_t)(void *userdata,
      const char *key, int **values, unsigned *out_num_values,
      const int *default_values, unsigned num_default_values);

typedef int (*resampler_config_get_string_t)(void *userdata,
      const char *key, char **output, const char *default_output);

/* Calls free() in host runtime. Sometimes needed on Windows.
 * free() on NULL is fine. */
typedef void (*resampler_config_free_t)(void *ptr);

struct resampler_config
{
   resampler_config_get_float_t get_float;
   resampler_config_get_int_t get_int;

   resampler_config_get_float_array_t get_float_array;
   resampler_config_get_int_array_t get_int_array;

   resampler_config_get_string_t get_string;
   /* Avoid problems where resampler plug and host are
    * linked against different C runtimes. */
   resampler_config_free_t free;
};

/* Bandwidth factor. Will be < 1.0 for downsampling, > 1.0 for upsampling.
 * Corresponds to expected resampling ratio. */
typedef void *(*resampler_init_t)(const struct resampler_config *config,
      double bandwidth_mod, enum resampler_quality quality,
      resampler_simd_mask_t mask);

/* Frees the handle. */
typedef void (*resampler_free_t)(void *data);

/* Processes input data. */
typedef void (*resampler_process_t)(void *_data, struct resampler_data *data);

typedef struct retro_resampler
{
   resampler_init_t     init;
   resampler_process_t  process;
   resampler_free_t     free;

   /* Must be RESAMPLER_API_VERSION */
   unsigned api_version;

   /* Human readable identifier of implementation. */
   const char *ident;

   /* Computer-friendly short version of ident.
    * Lower case, no spaces and special characters, etc. */
   const char *short_ident;
} retro_resampler_t;

typedef struct audio_frame_float
{
   float l;
   float r;
} audio_frame_float_t;

extern retro_resampler_t sinc_resampler;
#ifdef HAVE_CC_RESAMPLER
extern retro_resampler_t CC_resampler;
#endif
extern retro_resampler_t nearest_resampler;

/**
 * audio_resampler_driver_find_handle:
 * @index              : index of driver to get handle to.
 *
 * Returns: handle to audio resampler driver at index. Can be NULL
 * if nothing found.
 **/
const void *audio_resampler_driver_find_handle(int index);

/**
 * audio_resampler_driver_find_ident:
 * @index              : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of audio resampler driver at index.
 * Can be NULL if nothing found.
 **/
const char *audio_resampler_driver_find_ident(int index);

/**
 * retro_resampler_realloc:
 * @re                         : Resampler handle
 * @backend                    : Resampler backend that is about to be set.
 * @ident                      : Identifier name for resampler we want.
 * @bw_ratio                   : Bandwidth ratio.
 *
 * Reallocates resampler. Will free previous handle before
 * allocating a new one. If ident is NULL, first resampler will be used.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool retro_resampler_realloc(void **re, const retro_resampler_t **backend,
      const char *ident, enum resampler_quality quality, double bw_ratio);

RETRO_END_DECLS

#endif
