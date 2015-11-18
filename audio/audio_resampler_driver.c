/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "audio_resampler_driver.h"
#ifdef RARCH_INTERNAL
#include "../performance.h"
#endif
#include <file/config_file_userdata.h>
#include <string.h>
#ifndef DONT_HAVE_STRING_LIST
#include "../string_list_special.h"
#endif

static const rarch_resampler_t *resampler_drivers[] = {
   &sinc_resampler,
   &CC_resampler,
   &nearest_resampler,
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
      if (strcasecmp(ident, resampler_drivers[i]->ident) == 0)
         return i;
   return -1;
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
   const rarch_resampler_t *drv = resampler_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

#ifndef DONT_HAVE_STRING_LIST
/**
 * config_get_audio_resampler_driver_options:
 *
 * Get an enumerated list of all resampler driver names, separated by '|'.
 *
 * Returns: string listing of all resampler driver names, separated by '|'.
 **/
const char* config_get_audio_resampler_driver_options(void)
{
   return char_list_new_special(STRING_LIST_AUDIO_RESAMPLER_DRIVERS, NULL);
}
#endif

/**
 * find_resampler_driver:
 * @ident                      : Identifier of resampler driver to find.
 *
 * Finds resampler by @ident name.
 *
 * Returns: resampler driver if resampler driver was found, otherwise
 * NULL.
 **/
static const rarch_resampler_t *find_resampler_driver(const char *ident)
{
   int i = find_resampler_driver_index(ident);

   if (i >= 0)
      return resampler_drivers[i];

   return resampler_drivers[0];
}

#ifndef RARCH_INTERNAL

#ifdef __cplusplus
extern "C" {
#endif
retro_get_cpu_features_t perf_get_cpu_features_cb;

#ifdef __cplusplus
}
#endif

#endif

static resampler_simd_mask_t resampler_get_cpu_features(void)
{
#ifdef RARCH_INTERNAL
   return retro_get_cpu_features();
#else
   return perf_get_cpu_features_cb();
#endif
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
      const rarch_resampler_t **backend,
      double bw_ratio)
{
   resampler_simd_mask_t mask = resampler_get_cpu_features();

   *re = (*backend)->init(&resampler_config, bw_ratio, mask);

   if (!*re)
      return false;
   return true;
}

/**
 * rarch_resampler_realloc:
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
bool rarch_resampler_realloc(void **re, const rarch_resampler_t **backend,
      const char *ident, double bw_ratio)
{
   if (*re && *backend)
      (*backend)->free(*re);

   *re      = NULL;
   *backend = find_resampler_driver(ident);

   if (!resampler_append_plugs(re, backend, bw_ratio))
      goto error;

   return true;

error:
   if (!*re)
      *backend = NULL;
   return false;
}
