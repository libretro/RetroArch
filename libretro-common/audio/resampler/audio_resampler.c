/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (audio_resampler.c).
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

#include <string.h>

#include <string/stdstring.h>
#include <features/features_cpu.h>
#include <file/config_file_userdata.h>

#include <audio/audio_resampler.h>

static void resampler_null_process(void *a, struct resampler_data *b) { }
static void resampler_null_free(void *a) { }
static void *resampler_null_init(const struct resampler_config *a, double b,
      enum resampler_quality c, resampler_simd_mask_t d) { return (void*)0; }

retro_resampler_t null_resampler = {
   resampler_null_init,
   resampler_null_process,
   resampler_null_free,
   RESAMPLER_API_VERSION,
   "null",
   "null"
};

static const retro_resampler_t *resampler_drivers[] = {
   &sinc_resampler,
#ifdef HAVE_CC_RESAMPLER
   &CC_resampler,
#endif
#ifdef HAVE_NEAREST_RESAMPLER
   &nearest_resampler,
#endif
   &null_resampler,
   NULL,
};

static const struct resampler_config resampler_config = {
   config_userdata_get_float,
   config_userdata_get_int,
   config_userdata_get_float_array,
   config_userdata_get_int_array,
   config_userdata_get_string,
   config_userdata_free,
};

/**
 * find_resampler_driver_index:
 * @ident                      : Identifier of resampler driver to find.
 *
 * Finds resampler driver index by @ident name.
 *
 * Returns: resampler driver index if resampler driver was found, otherwise
 * -1.
 **/
static int find_resampler_driver_index(const char *ident)
{
   unsigned i;

   for (i = 0; resampler_drivers[i]; i++)
      if (string_is_equal_noncase(ident, resampler_drivers[i]->ident))
         return i;
   return -1;
}

/**
 * find_resampler_driver:
 * @ident                      : Identifier of resampler driver to find.
 *
 * Finds resampler by @ident name.
 *
 * Returns: resampler driver if resampler driver was found, otherwise
 * NULL.
 **/
static const retro_resampler_t *find_resampler_driver(const char *ident)
{
   int i = find_resampler_driver_index(ident);

   if (i >= 0)
      return resampler_drivers[i];

   return resampler_drivers[0];
}

/**
 * resampler_append_plugs:
 * @re                         : Resampler handle
 * @backend                    : Resampler backend that is about to be set.
 * @bw_ratio                   : Bandwidth ratio.
 *
 * Initializes resampler driver based on queried CPU features.
 *
 * Returns: true (1) if successfully initialized, otherwise false (0).
 **/
static bool resampler_append_plugs(void **re,
      const retro_resampler_t **backend,
      enum resampler_quality quality,
      double bw_ratio)
{
   resampler_simd_mask_t mask = (resampler_simd_mask_t)cpu_features_get();

   if (*backend)
      *re = (*backend)->init(&resampler_config, bw_ratio, quality, mask);

   if (!*re)
      return false;
   return true;
}


/**
 * audio_resampler_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to audio resampler driver at index. Can be NULL
 * if nothing found.
 **/
const void *audio_resampler_driver_find_handle(int idx)
{
   const void *drv = resampler_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * audio_resampler_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of audio resampler driver at index.
 * Can be NULL if nothing found.
 **/
const char *audio_resampler_driver_find_ident(int idx)
{
   const retro_resampler_t *drv = resampler_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

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
      const char *ident, enum resampler_quality quality, double bw_ratio)
{
   if (*re && *backend)
      (*backend)->free(*re);

   *re      = NULL;
   *backend = find_resampler_driver(ident);

   if (!resampler_append_plugs(re, backend, quality, bw_ratio))
   {
      if (!*re)
         *backend = NULL;
      return false;
   }

   return true;
}
